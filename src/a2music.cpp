/*
 * XU4 - Apple II Mockingboard music
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Plays a tune of the MUS* files (from the Apple II disks) through a model
 * of the Mockingboard sequencer and renders the two AY-3-8910 sound chips
 * to 16-bit stereo samples, the first chip on the left and the second on
 * the right.
 *
 * A music file holds a count of tunes and the offset of each.  A tune has
 * a count of channels, the offset of each channel's data, then a count of
 * patterns and their offsets (all relative to the tune).  A pattern begins
 * with a two byte return address for each channel.
 *
 * The data of a channel is read one byte at a time:
 *
 *   1-7F       Play note (plus transpose) for the current duration.
 *   80         Release the note for the current duration.
 *   81-FF      Set duration (low 7 bits) in ticks.
 *   00         Commands follow:
 *     00             End of tune.
 *     10 n           Release when n ticks of a note remain.
 *     11 n           Transpose.
 *     12 lo hi       Add to the tone period of notes.
 *     18 lo hi       Attack rate.        20 lo hi    Decay rate.
 *     28 lo hi       Sustain level.      30 lo hi    Release rate.
 *     38 lo hi       Attack level.       (Levels are 8.8 fixed point.)
 *     80 n           Call pattern n.
 *     81 n           Return from pattern n.
 *     82             Channel finished.
 *     83 lo hi       Set the tick period in CPU cycles.
 *   Any other value ends the commands.
 */

#include <stdint.h>
#include <string.h>
#include <math.h>
#include <vector>
#include "a2music.h"

#define A2_CPU_HZ       1023000
#define TICK_DEFAULT    17050       // CPU cycles (about 60 Hz)
#define MAX_CHANNELS    6
#define MAX_PATTERNS    64
#define MAX_TICKS       (60 * 60 * 10)
#define OVERSAMPLE      4

enum EnvStage { ENV_OFF, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };

struct A2Channel {
    uint32_t cursor;
    uint8_t  stage;
    uint8_t  finished;
    uint8_t  envDur;
    uint8_t  noteDur;
    uint8_t  relDur;
    uint8_t  transpose;
    uint16_t bend;
    uint16_t atkRate;
    uint16_t decay;
    uint16_t susLevel;
    uint16_t relRate;
    uint16_t atkLevel;
    uint16_t level;
};

struct A2Tick {
    uint16_t cycles;
    uint8_t  reg[2][16];    // Tone periods (0-5) & levels (8-10) used.
};

// Chip & voice of each channel when two sound chips are present.
static const uint8_t chanChip[MAX_CHANNELS]  = { 0, 1, 0, 1, 1, 0 };
static const uint8_t chanVoice[MAX_CHANNELS] = { 0, 0, 1, 1, 2, 2 };

static uint16_t tonePeriod(int note)
{
    // Equal temperament from A0 (27.5 Hz); the tone is CPU clock / 16.
    return (uint16_t) lround(A2_CPU_HZ / (16.0 * 27.5 * pow(2.0, note / 12.0)));
}

class A2Sequencer {
public:
    A2Sequencer(const uint8_t* data, size_t size)
        : data(data), size(size) {}

    bool run(int tune, std::vector<A2Tick>& ticks);

private:
    int  byte(uint32_t pos) const { return (pos < size) ? data[pos] : 0; }
    int  word(uint32_t pos) const { return byte(pos) | byte(pos + 1) << 8; }
    int  next(A2Channel& ch) { return byte(ch.cursor++); }
    int  next16(A2Channel& ch) { int lo = next(ch); return lo | next(ch) << 8; }
    bool readChannel(int i, bool& tuneDone);
    void envelope(int i);

    const uint8_t* data;
    size_t size;
    A2Channel chan[MAX_CHANNELS];
    uint32_t pattern[MAX_PATTERNS];
    uint32_t returnAddr[MAX_PATTERNS][MAX_CHANNELS];
    uint8_t reg[2][16];
    int channelsExist;
    int channels;
    int patterns;
    uint16_t tickCycles;
};

/*
 * Read channel data until a note starts or is released.
 * Return false if the channel has finished.
 */
bool A2Sequencer::readChannel(int i, bool& tuneDone)
{
    A2Channel& ch = chan[i];
    int n;

    for (int guard = 0; guard < 4096; ++guard) {
        n = next(ch);
        if (n & 0x80) {
            n &= 0x7f;
            if (n == 0) {
                ch.stage = ENV_RELEASE;
                ch.envDur = ch.noteDur;
                return true;
            }
            ch.noteDur = n;
        } else if (n) {
            int note = (n + ch.transpose) & 0xff;
            uint16_t period = (note < 128) ? tonePeriod(note) : 0;
            period += ch.bend;
            uint8_t* r = reg[chanChip[i]] + chanVoice[i] * 2;
            r[0] = period & 0xff;
            r[1] = period >> 8;
            ch.stage = ENV_ATTACK;
            ch.envDur = ch.noteDur;
            return true;
        } else {
            // Commands
            for (;;) {
                n = next(ch);
                if (n == 0) {
                    tuneDone = true;
                    return true;
                }
                switch (n) {
                    case 0x10: ch.relDur    = next(ch); continue;
                    case 0x11: ch.transpose = next(ch); continue;
                    case 0x12: ch.bend      = next16(ch); continue;
                    case 0x18: ch.atkRate   = next16(ch); continue;
                    case 0x20: ch.decay     = next16(ch); continue;
                    case 0x28: ch.susLevel  = next16(ch); continue;
                    case 0x30: ch.relRate   = next16(ch); continue;
                    case 0x38: ch.atkLevel  = next16(ch); continue;
                    case 0x80:
                        n = next(ch);
                        if (n < patterns && n < MAX_PATTERNS) {
                            returnAddr[n][i] = ch.cursor;
                            ch.cursor = pattern[n] + channelsExist * 2;
                        }
                        break;
                    case 0x81:
                        n = next(ch);
                        if (n < MAX_PATTERNS)
                            ch.cursor = returnAddr[n][i];
                        break;
                    case 0x82:
                        ch.finished = 1;
                        ch.stage = ENV_RELEASE;
                        return false;
                    case 0x83:
                        tickCycles = next16(ch);
                        break;
                }
                break;
            }
        }
    }
    ch.finished = 1;
    return false;
}

void A2Sequencer::envelope(int i)
{
    A2Channel& ch = chan[i];
    int level;

    switch (ch.stage) {
        case ENV_OFF:
            return;
        case ENV_ATTACK:
            level = ch.level + ch.atkRate;
            if (level > 0xffff || level >= ch.atkLevel) {
                level = ch.atkLevel;
                ch.stage = ENV_DECAY;
            }
            ch.level = level;
            break;
        case ENV_DECAY:
            level = ch.level - ch.decay;
            if (level < 0 || level < ch.susLevel) {
                level = ch.susLevel;
                ch.stage = ENV_SUSTAIN;
            }
            ch.level = level;
            break;
        case ENV_RELEASE:
            level = ch.level - ch.relRate;
            if (level < 0) {
                level = 0;
                ch.stage = ENV_OFF;
            }
            ch.level = level;
            goto set_level;
    }
    if (ch.envDur < ch.relDur)
        ch.stage = ENV_RELEASE;
set_level:
    reg[chanChip[i]][8 + chanVoice[i]] = ch.level >> 8;
}

/*
 * Play one pass of a tune and record the chip registers of each tick.
 */
bool A2Sequencer::run(int tune, std::vector<A2Tick>& ticks)
{
    if (tune < 0 || tune >= byte(0))
        return false;
    uint32_t base = word(1 + tune * 2);
    channelsExist = byte(base);
    channels = channelsExist;
    if (channels > MAX_CHANNELS)
        channels = MAX_CHANNELS;
    patterns = byte(base + 1 + channelsExist * 2);
    for (int i = 0; i < patterns && i < MAX_PATTERNS; ++i)
        pattern[i] = (base + word(base + 2 + channelsExist * 2 + i * 2)) & 0xffff;

    memset(chan, 0, sizeof(chan));
    memset(returnAddr, 0, sizeof(returnAddr));
    memset(reg, 0, sizeof(reg));
    for (int i = 0; i < channels; ++i) {
        chan[i].cursor = (base + word(base + 1 + i * 2)) & 0xffff;
        chan[i].envDur = 1;
    }
    tickCycles = TICK_DEFAULT;

    A2Tick tick;
    for (int t = 0; t < MAX_TICKS; ++t) {
        bool anyActive = false;
        bool tuneDone = false;
        for (int i = channels - 1; i >= 0; --i) {
            A2Channel& ch = chan[i];
            if (! ch.finished) {
                anyActive = true;
                if (--ch.envDur == 0) {
                    if (! readChannel(i, tuneDone))
                        continue;
                    if (tuneDone)
                        break;
                }
            }
            envelope(i);
        }
        if (tuneDone || ! anyActive)
            return ! ticks.empty();

        tick.cycles = tickCycles;
        memcpy(tick.reg, reg, sizeof(reg));
        ticks.push_back(tick);
    }
    return ! ticks.empty();
}

// AY-3-8910 output level of each volume setting.
static const float ayLevel[16] = {
    0.0f,    0.0137f, 0.0205f, 0.0291f, 0.0423f, 0.0618f, 0.0847f, 0.1369f,
    0.1691f, 0.2647f, 0.3527f, 0.4499f, 0.5704f, 0.6873f, 0.8482f, 1.0f
};

/*
 * Render tune of a music file to interleaved stereo samples.
 * Return false if the tune does not exist.
 */
bool a2_renderMusic(const uint8_t* data, size_t size, int tune, int rate,
                    std::vector<int16_t>& samples)
{
    std::vector<A2Tick> ticks;
    A2Sequencer seq(data, size);
    if (! seq.run(tune, ticks))
        return false;

    const double subRate = double(rate) * OVERSAMPLE;
    uint32_t phase[2][3];       // 0.32 fixed point
    uint32_t step[2][3];
    float amp[2][3];
    double frac = 0.0;

    memset(phase, 0, sizeof(phase));
    samples.clear();

    for (const A2Tick& tk : ticks) {
        for (int c = 0; c < 2; ++c) {
            for (int v = 0; v < 3; ++v) {
                int period = tk.reg[c][v*2] | (tk.reg[c][v*2+1] & 15) << 8;
                if (period == 0)
                    period = 1;
                double hz = A2_CPU_HZ / (16.0 * period);
                // Tones above the output range are silent.
                if (hz >= rate / 2) {
                    step[c][v] = 0;
                    amp[c][v] = 0.0f;
                } else {
                    step[c][v] = (uint32_t) (hz / subRate * 4294967296.0);
                    amp[c][v] = ayLevel[tk.reg[c][8 + v] & 15];
                }
            }
        }

        frac += double(tk.cycles) * rate / A2_CPU_HZ;
        int count = int(frac);
        frac -= count;

        for (int n = 0; n < count; ++n) {
            for (int c = 0; c < 2; ++c) {
                float sum = 0.0f;
                for (int s = 0; s < OVERSAMPLE; ++s) {
                    for (int v = 0; v < 3; ++v) {
                        phase[c][v] += step[c][v];
                        sum += (phase[c][v] & 0x80000000) ? -amp[c][v]
                                                          :  amp[c][v];
                    }
                }
                // Three voices at full level reach full scale.
                int pcm = int(sum * (32767.0f / (3 * OVERSAMPLE)));
                if (pcm > 32767)
                    pcm = 32767;
                else if (pcm < -32767)
                    pcm = -32767;
                samples.push_back((int16_t) pcm);
            }
        }
    }
    return true;
}
