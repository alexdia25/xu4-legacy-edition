/*
 * XU4 - NES music
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Plays a tune of the NES release of Ultima IV.  The game's own sound
 * driver (ROM bank 14, at $8000) is run on a 6502 model: once to start the
 * tune, then once a frame as the frame interrupt does.  The writes it makes
 * to the sound registers ($4000-$4017) are timed by the cycles taken and
 * played on a model of the console's sound unit (two pulse channels, a
 * triangle and a noise channel; the driver doesn't use the sample channel).
 *
 * The tune is run until the driver comes back to a state it was in
 * before; the part between is the loop of the tune, which is what is
 * rendered.  A tune that stops is rendered whole.
 */

#include <string.h>
#include <vector>
#include <unordered_map>
#include "nesmusic.h"
#include "nesrom.h"
#include "c64cpu.h"

#define NES_CPU_HZ      1789773     // NTSC
#define FRAME_CYCLES    29781
#define MAX_FRAMES      (60 * 60 * 6)

// The driver's entry points & variables.
#define SND_START       0x8036      // A = tune.
#define SND_FRAME       0x808D
#define SND_PLAYING     0x7E
#define SND_TEMPO       0x6C7
#define SND_TEMPO_ACC   0x6C8       // Tempo accumulator; a carry is a tick.

//----------------------------------------------------------------------------
// APU

static const uint8_t lengthTable[32] = {
    10,254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

static const uint16_t noisePeriod[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

static const uint8_t dutyTable[4] = { 0x02, 0x06, 0x1E, 0xF9 };

struct Envelope {
    uint8_t ctrl;           // Register 0 of the channel.
    bool start;
    uint8_t divider, decay;

    void clock() {
        if (start) {
            start = false;
            decay = 15;
            divider = ctrl & 15;
        } else if (divider) {
            --divider;
        } else {
            divider = ctrl & 15;
            if (decay)
                --decay;
            else if (ctrl & 0x20)
                decay = 15;
        }
    }
    int volume() const { return (ctrl & 0x10) ? (ctrl & 15) : decay; }
};

struct Apu {
    // Pulse channels.
    Envelope penv[2];
    uint16_t ptimer[2], pcount[2];
    uint8_t pstep[2], plen[2];
    // Triangle.
    uint8_t tctrl, tlinear, tlen, tstep;
    bool treload;
    uint16_t ttimer, tcount;
    // Noise.
    Envelope nenv;
    uint8_t nmode, nlen;
    uint16_t nperiod, ncount, lfsr;

    uint8_t enable;
    bool fiveStep;
    uint32_t seqCycle;      // CPU cycles into the frame sequence.
    bool odd;

    float pulseMix[31];
    float tndMix[203];

    Apu() {
        memset(this, 0, sizeof(*this));
        lfsr = 1;
        nperiod = noisePeriod[0];
        for (int i = 0; i < 31; ++i)
            pulseMix[i] = i ? 95.52f / (8128.0f / i + 100.0f) : 0.0f;
        for (int i = 0; i < 203; ++i)
            tndMix[i] = i ? 163.67f / (24329.0f / i + 100.0f) : 0.0f;
    }

    void write(int reg, uint8_t v) {
        int ch;
        switch (reg) {
        case 0x00: case 0x04:
            penv[reg >> 2].ctrl = v;
            break;
        case 0x02: case 0x06:
            ch = reg >> 2;
            ptimer[ch] = (ptimer[ch] & 0x700) | v;
            break;
        case 0x03: case 0x07:
            ch = reg >> 2;
            ptimer[ch] = (ptimer[ch] & 0xFF) | ((v & 7) << 8);
            if (enable & (1 << ch))
                plen[ch] = lengthTable[v >> 3];
            pstep[ch] = 0;
            penv[ch].start = true;
            break;
        case 0x08:
            tctrl = v;
            break;
        case 0x0A:
            ttimer = (ttimer & 0x700) | v;
            break;
        case 0x0B:
            ttimer = (ttimer & 0xFF) | ((v & 7) << 8);
            if (enable & 4)
                tlen = lengthTable[v >> 3];
            treload = true;
            break;
        case 0x0C:
            nenv.ctrl = v;
            break;
        case 0x0E:
            nmode = v & 0x80;
            nperiod = noisePeriod[v & 15];
            break;
        case 0x0F:
            if (enable & 8)
                nlen = lengthTable[v >> 3];
            nenv.start = true;
            break;
        case 0x15:
            enable = v & 0x1F;
            if (! (v & 1)) plen[0] = 0;
            if (! (v & 2)) plen[1] = 0;
            if (! (v & 4)) tlen = 0;
            if (! (v & 8)) nlen = 0;
            break;
        case 0x17:
            fiveStep = (v & 0x80) != 0;
            seqCycle = 0;
            if (fiveStep) {
                quarterFrame();
                halfFrame();
            }
            break;
        }
    }

    void quarterFrame() {
        penv[0].clock();
        penv[1].clock();
        nenv.clock();
        if (treload)
            tlinear = tctrl & 0x7F;
        else if (tlinear)
            --tlinear;
        if (! (tctrl & 0x80))
            treload = false;
    }

    void halfFrame() {
        for (int ch = 0; ch < 2; ++ch) {
            if (plen[ch] && ! (penv[ch].ctrl & 0x20))
                --plen[ch];
        }
        if (tlen && ! (tctrl & 0x80))
            --tlen;
        if (nlen && ! (nenv.ctrl & 0x20))
            --nlen;
    }

    // Run one CPU cycle & return the output (0 to about 1).
    float clock() {
        // Frame sequencer.
        ++seqCycle;
        switch (seqCycle) {
        case 7457:
        case 22371:
            quarterFrame();
            break;
        case 14913:
            quarterFrame();
            halfFrame();
            break;
        case 29829:
            if (! fiveStep) {
                quarterFrame();
                halfFrame();
                seqCycle = 0;
            }
            break;
        case 37281:
            quarterFrame();
            halfFrame();
            seqCycle = 0;
            break;
        }

        // The triangle's timer runs at the CPU rate; it stops (holding its
        // level) when a counter is zero or the pitch is above hearing.
        if (tcount == 0) {
            tcount = ttimer;
            if (tlen && tlinear && ttimer >= 2)
                tstep = (tstep + 1) & 31;
        } else
            --tcount;

        // The others run at half the CPU rate.
        odd = ! odd;
        if (odd) {
            for (int ch = 0; ch < 2; ++ch) {
                if (pcount[ch] == 0) {
                    pcount[ch] = ptimer[ch];
                    pstep[ch] = (pstep[ch] + 1) & 7;
                } else
                    --pcount[ch];
            }
            if (ncount == 0) {
                ncount = nperiod / 2 - 1;
                int fb = (lfsr ^ (lfsr >> (nmode ? 6 : 1))) & 1;
                lfsr = (lfsr >> 1) | (fb << 14);
            } else
                --ncount;
        }

        int p = 0;
        for (int ch = 0; ch < 2; ++ch) {
            int duty = penv[ch].ctrl >> 6;
            // The sweep unit isn't used, but it still mutes periods below
            // 8 or above $3FF (as its target, 2 x period, overflows).
            if (plen[ch] && ptimer[ch] >= 8 && ptimer[ch] < 0x400 &&
                ((dutyTable[duty] >> (7 - pstep[ch])) & 1))
                p += penv[ch].volume();
        }
        int t = (tstep < 16) ? 15 - tstep : tstep - 16;
        int n = (nlen && ! (lfsr & 1)) ? nenv.volume() : 0;
        return pulseMix[p] + tndMix[3 * t + 2 * n];
    }
};

//----------------------------------------------------------------------------

struct Frame {
    uint32_t first;         // Index of its first write.
};

/*
 * Hash the driver's memory, leaving out the tempo accumulator, which
 * depends on how the ticks fall on the frames.
 */
static uint64_t stateHash(const uint8_t* mem)
{
    uint64_t h = 1469598103934665603ULL;
    for (int i = 0x76; i < 0xC8; ++i)
        h = (h ^ mem[i]) * 1099511628211ULL;
    h = (h ^ mem[0x6C5]) * 1099511628211ULL;
    h = (h ^ mem[SND_TEMPO]) * 1099511628211ULL;
    for (int i = 0x6C00; i < 0x6C50; ++i)
        h = (h ^ mem[i]) * 1099511628211ULL;
    return h;
}

/*
 * Run the driver for a tune.  Return the index of the frame where the loop
 * starts, or -1 if the driver fails.  For a tune that stops, the frames
 * end where it stops and 0 is returned.
 *
 * The tempo is added to an accumulator every frame and the tune moves on a
 * tick when it carries, so the state is only compared at the ticks and the
 * length of the loop is rounded to whole frames.
 */
static int runDriver(uint8_t* mem, int tune, std::vector<Frame>& frames,
                     std::vector<SidWrite>& writes)
{
    Cpu6502 cpu(mem);
    cpu.writes = &writes;
    cpu.logBase = 0x4000;
    cpu.romStart = 0x8000;      // Bank switches & ROM are left alone.

    cpu.a = uint8_t(tune);
    if (! cpu.call(SND_START))
        return -1;

    std::unordered_map<uint64_t, int> seen;
    uint64_t lastHash = 0;
    for (int t = 0; t < MAX_FRAMES; ++t) {
        int acc = mem[SND_TEMPO_ACC];

        Frame fr;
        fr.first = writes.size();
        cpu.cycles = 0;
        if (! cpu.call(SND_FRAME))
            return -1;
        frames.push_back(fr);

        if (! mem[SND_PLAYING])
            return 0;
        if (acc + mem[SND_TEMPO] < 0x40)
            continue;           // No tick.

        uint64_t h = stateHash(mem);
        std::unordered_map<uint64_t, int>::const_iterator it = seen.find(h);
        if (it != seen.end()) {
            if (h == lastHash) {
                // Nothing changes on a tick: the tune has stopped.
                frames.resize(it->second + 1);
                return 0;
            }
            return it->second + 1;
        }
        seen[h] = t;
        lastHash = h;
    }
    return -1;
}

/*
 * Render the loop of a tune to 16-bit stereo samples.
 */
bool nes_renderMusic(int tune, int rate, std::vector<int16_t>& samples)
{
    const std::vector<uint8_t>* rom = nes_rom();
    if (! rom || tune < 1 || tune > 30)
        return false;

    // The driver's bank at $8000 and the fixed last bank at $C000.
    std::vector<uint8_t> mem(0x10000, 0);
    memcpy(&mem[0x8000], rom->data() + 14 * NES_BANK_SIZE, NES_BANK_SIZE);
    memcpy(&mem[0xC000], rom->data() + 15 * NES_BANK_SIZE, NES_BANK_SIZE);

    std::vector<Frame> frames;
    std::vector<SidWrite> writes;
    int loopStart = runDriver(mem.data(), tune, frames, writes);
    if (loopStart < 0 || loopStart >= (int) frames.size())
        return false;

    Apu apu;
    const double cyclesPerSample = double(NES_CPU_HZ) / rate;
    double acc = 0.0;
    float sum = 0.0f;
    int count = 0;
    double dcIn = 0.0, dcOut = 0.0, low = 0.0;
    const double dcPole = 1.0 - 2.0 * 3.14159265 * 37.0 / rate;
    const double lowPole = 0.5;     // Softens the edges a little.

    // Writes made while starting the tune.
    for (size_t w = 0; w < frames[0].first; ++w)
        apu.write(writes[w].reg, writes[w].value);

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
            for (; at < next; ++at) {
                sum += apu.clock();
                ++count;
                acc += 1.0;
                if (acc >= cyclesPerSample) {
                    acc -= cyclesPerSample;
                    // Average the cycles of a sample & remove the DC.
                    double v = double(sum) / count * 30000.0;
                    sum = 0.0f;
                    count = 0;
                    dcOut = v - dcIn + dcPole * dcOut;
                    dcIn = v;
                    low += (dcOut - low) * lowPole;
                    if (out) {
                        int pcm = int(low);
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
                apu.write(writes[w].reg, writes[w].value);
        }
    }
    return ! samples.empty();
}
