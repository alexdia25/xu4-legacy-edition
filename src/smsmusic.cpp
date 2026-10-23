/*
 * XU4 - Sega Master System music
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Plays a tune of the Sega Master System release of Ultima IV.  The game's
 * own sound driver (ROM bank 31, at $8000) is run on a Z80 model: once to
 * start the tune, then once a frame as the frame interrupt does.  The
 * writes it makes to the PSG sound chip (an SN76489) are timed by the
 * cycles taken and played on a model of the PSG.
 *
 * The tune is run until the driver comes back to a state it was in
 * before; the part between is the loop of the tune, which is what is
 * rendered.
 */

#include <string.h>
#include <math.h>
#include <vector>
#include <unordered_map>
#include "smsmusic.h"
#include "smsrom.h"
#include "smsz80.h"

#define SMS_CPU_HZ      3579545     // NTSC
#define FRAME_CYCLES    59736       // 262 lines of 228 cycles.
#define MAX_FRAMES      (60 * 60 * 6)

// The driver's entry points.
#define SND_FRAME       0x8000
#define SND_STOP        0x8003
#define SND_START       0x8006
#define SND_INIT        0x8009

struct Frame {
    uint32_t first;         // Index of its first write.
};

//----------------------------------------------------------------------------
// PSG

struct Psg {
    uint16_t period[4];     // Tone periods; noise control in [3].
    uint8_t  atten[4];
    uint16_t count[4];
    uint8_t  level[4];      // Output of the tone channels (0/1).
    uint16_t lfsr;
    int latch;              // Latched register (channel * 2 + volume).
    int amp[16];

    Psg() : lfsr(0x8000), latch(0) {
        for (int i = 0; i < 4; ++i) {
            period[i] = 0;
            atten[i] = 15;
            count[i] = 0;
            level[i] = 0;
        }
        // 2 dB per step; 15 is silent.
        for (int i = 0; i < 15; ++i)
            amp[i] = int(8000.0 * pow(10.0, -0.1 * i) + 0.5);
        amp[15] = 0;
    }

    void write(uint8_t v) {
        if (v & 0x80) {
            latch = (v >> 4) & 7;
            setLow(v & 15);
        } else if (! (latch & 1) && (latch >> 1) < 3) {
            int ch = latch >> 1;
            period[ch] = (period[ch] & 15) | ((v & 0x3F) << 4);
        } else {
            setLow(v & 15);
        }
    }

    void setLow(int data) {
        int ch = latch >> 1;
        if (latch & 1)
            atten[ch] = data;
        else if (ch < 3)
            period[ch] = (period[ch] & 0x3F0) | data;
        else {
            period[3] = data & 7;
            lfsr = 0x8000;
        }
    }

    // One step of the clock divided by 16; returns the summed output.
    int clock() {
        int out = 0;
        for (int ch = 0; ch < 3; ++ch) {
            if (count[ch] == 0 || --count[ch] == 0) {
                count[ch] = period[ch] ? period[ch] : 1;
                level[ch] ^= 1;
                if (ch == 2 && (period[3] & 3) == 3 && level[2])
                    shiftNoise();
            }
            // A period of 0 or 1 holds the output high.
            if (level[ch] || period[ch] <= 1)
                out += amp[atten[ch]];
        }
        if ((period[3] & 3) != 3) {
            if (count[3] == 0 || --count[3] == 0) {
                count[3] = 0x10 << (period[3] & 3);
                level[3] ^= 1;
                if (level[3])
                    shiftNoise();
            }
        }
        if (lfsr & 1)
            out += amp[atten[3]];
        return out;
    }

    void shiftNoise() {
        uint16_t fb = (period[3] & 4) ? ((lfsr ^ (lfsr >> 3)) & 1)
                                      : (lfsr & 1);
        lfsr = (lfsr >> 1) | (fb << 15);
    }
};

//----------------------------------------------------------------------------

// The driver's channels: eight of $28 bytes, then the one for sounds.
#define CHAN_BASE       0xDD5D
#define CHAN_SIZE       0x28
#define CHAN_SOUND      0xDE75
#define CHAN_TEMPO      0x0F    // Tempo accumulator; a carry is a tick.
#define CHAN_ENV_STEP   0x1E    // Frames since the note began.

static bool isChannel(int addr, int* off)
{
    if (addr >= CHAN_SOUND) {
        *off = addr - CHAN_SOUND;
        return *off < CHAN_SIZE;
    }
    if (addr < CHAN_BASE || addr >= CHAN_BASE + 8 * CHAN_SIZE)
        return false;
    *off = (addr - CHAN_BASE) % CHAN_SIZE;
    return true;
}

/*
 * Hash the driver's memory, leaving out what depends on how the ticks of
 * the tempo fall on the frames.
 */
static uint64_t stateHash(const uint8_t* mem)
{
    uint64_t h = 1469598103934665603ULL;
    for (int i = 0xDD00; i < 0xDF00; ++i) {
        int off;
        uint8_t v = mem[i];
        if (isChannel(i, &off) && (off == CHAN_TEMPO || off == CHAN_ENV_STEP))
            v = 0;
        h = (h ^ v) * 1099511628211ULL;
    }
    return h;
}

/*
 * Run the driver for a tune.  Return the index of the frame where the loop
 * starts, or -1 if the driver fails.  A tune that ends is all loop.
 *
 * The tempo of each channel is added to an accumulator every frame and the
 * tune moves on a tick when it carries, so the ticks drift across the
 * frames.  The loop is found by comparing the state at the ticks only, so
 * its length is rounded to whole frames.
 */
static int runDriver(uint8_t* mem, int tune, std::vector<Frame>& frames,
                     std::vector<PsgWrite>& writes)
{
    Z80Cpu cpu(mem);
    cpu.writes = &writes;

    mem[0xC001] = 1;            // NTSC timing.
    mem[0xC002] = 0;            // No FM sound unit.

    // Start the tune as play_music ($0621) does.
    cpu.a = 0;
    if (! cpu.call(SND_INIT) || ! cpu.call(SND_STOP))
        return -1;
    cpu.a = uint8_t(tune);
    if (! cpu.call(SND_START))
        return -1;

    std::unordered_map<uint64_t, int> seen;
    uint8_t tempo[9];
    for (int t = 0; t < MAX_FRAMES; ++t) {
        for (int c = 0; c < 9; ++c)
            tempo[c] = mem[(c < 8 ? CHAN_BASE + c * CHAN_SIZE : CHAN_SOUND)
                           + CHAN_TEMPO];

        Frame fr;
        fr.first = writes.size();
        cpu.cycles = 0;
        if (! cpu.call(SND_FRAME, FRAME_CYCLES))
            return -1;
        frames.push_back(fr);

        bool tick = false, playing = false;
        for (int c = 0; c < 9; ++c) {
            uint16_t ch = (c < 8) ? CHAN_BASE + c * CHAN_SIZE : CHAN_SOUND;
            if (mem[ch]) {
                playing = true;
                if (mem[ch + CHAN_TEMPO] < tempo[c])
                    tick = true;
            }
        }
        if (! playing)
            return 0;
        if (! tick)
            continue;

        uint64_t h = stateHash(mem);
        std::unordered_map<uint64_t, int>::const_iterator it = seen.find(h);
        if (it != seen.end())
            return it->second + 1;
        seen[h] = t;
    }
    return -1;
}

/*
 * Render the loop of a tune to 16-bit stereo samples.
 */
bool sms_renderMusic(int tune, int rate, std::vector<int16_t>& samples)
{
    const std::vector<uint8_t>* rom = sms_rom();
    if (! rom || tune < 1 || tune > 0x7F)
        return false;

    // Banks 0 & 1 in the first two slots, the driver's bank in the third.
    std::vector<uint8_t> mem(0x10000, 0);
    memcpy(&mem[0], rom->data(), 2 * SMS_BANK_SIZE);
    memcpy(&mem[0x8000], rom->data() + 31 * SMS_BANK_SIZE, SMS_BANK_SIZE);

    std::vector<Frame> frames;
    std::vector<PsgWrite> writes;
    int loopStart = runDriver(mem.data(), tune, frames, writes);
    if (loopStart < 0 || loopStart >= (int) frames.size())
        return false;

    Psg psg;
    const double clocksPerSample = double(SMS_CPU_HZ) / 16.0 / rate;
    double pending = 0.0;       // PSG clocks not yet rendered.
    double acc = 0.0;           // PSG clocks of the current sample.
    int sum = 0, count = 0;
    double dcIn = 0.0, dcOut = 0.0;
    const double dcPole = 1.0 - 2.0 * 3.14159265 * 20.0 / rate;

    // Writes made while starting the tune.
    for (size_t w = 0; w < frames[0].first; ++w)
        psg.write(writes[w].value);

    samples.clear();
    for (size_t t = 0; t < frames.size(); ++t) {
        size_t end = (t + 1 < frames.size()) ? frames[t+1].first
                                              : writes.size();
        bool out = (int(t) >= loopStart);
        uint32_t at = 0;

        for (size_t w = frames[t].first; w <= end; ++w) {
            uint32_t next = (w < end) ? writes[w].cycle : FRAME_CYCLES;
            if (next > FRAME_CYCLES)
                next = FRAME_CYCLES;
            if (next > at) {
                pending += (next - at) / 16.0;
                at = next;
            }
            while (pending >= 1.0) {
                pending -= 1.0;
                sum += psg.clock();
                ++count;
                acc += 1.0;
                if (acc >= clocksPerSample) {
                    acc -= clocksPerSample;
                    // Average the clocks of a sample & remove the DC.
                    double v = double(sum) / count;
                    sum = count = 0;
                    dcOut = v - dcIn + dcPole * dcOut;
                    dcIn = v;
                    if (out) {
                        int pcm = int(dcOut);
                        if (pcm > 32767)
                            pcm = 32767;
                        else if (pcm < -32767)
                            pcm = -32767;
                        samples.push_back((int16_t) pcm);
                        samples.push_back((int16_t) pcm);
                    }
                }
            }
            if (w < end)
                psg.write(writes[w].value);
        }
    }
    return ! samples.empty();
}
