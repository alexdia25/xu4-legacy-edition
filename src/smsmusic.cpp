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
 * Each tune also has a version for the FM Sound Unit (a YM2413), which the
 * driver plays instead when it is told the unit is fitted.  The PSG is then
 * kept silent and the writes to the unit are played on emu2413, a model of
 * the chip (MIT licence, src/support/emu2413.c).
 *
 * The tune is run until the driver comes back to a state it was in
 * before; the part between is the loop of the tune, which is what is
 * rendered.
 */

#include <string.h>
#include <math.h>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include "smsmusic.h"
#include "smsrom.h"
#include "smsz80.h"
#include "support/emu2413.h"

#define NTSC_CPU_HZ     3579545
#define NTSC_FRAME      59736       // 262 lines of 228 cycles.
#define PAL_CPU_HZ      3546893
#define PAL_FRAME       71364       // 313 lines of 228 cycles.
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
static int runDriver(uint8_t* mem, int tune, bool fm, bool pal,
                     std::vector<Frame>& frames, std::vector<PsgWrite>& writes)
{
    Z80Cpu cpu(mem);
    cpu.writes = &writes;
    const uint32_t frameCycles = pal ? PAL_FRAME : NTSC_FRAME;

    // On a 50 Hz (PAL) console the driver speeds up the tempo of each
    // channel by its table at $8C6F.
    mem[0xC001] = pal ? 0 : 1;
    mem[0xC002] = fm ? 1 : 0;   // The FM sound unit is fitted.

    // Start the tune as sound_reset ($05C7) & play_music ($0621) do; the
    // driver is told of the FM unit with A = $C0.
    cpu.a = fm ? 0xC0 : 0;
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
        if (! cpu.call(SND_FRAME, frameCycles))
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

// Brings the FM tunes up to about the loudness of the PSG ones.
#define FM_LEVEL    4.0

static void chipWrite(Psg& psg, OPLL* opll, const PsgWrite& w)
{
    if (w.port == 0)
        psg.write(w.value);
    else if (opll)
        OPLL_writeIO(opll, w.port & 1, w.value);
}

/*
 * Render the loop of a tune to 16-bit stereo samples.
 */
static bool renderTune(const std::vector<uint8_t>* rom, int tune, int rate,
                       bool fm, bool pal, std::vector<int16_t>& samples)
{
    // Banks 0 & 1 in the first two slots, the driver's bank in the third.
    std::vector<uint8_t> mem(0x10000, 0);
    memcpy(&mem[0], rom->data(), 2 * SMS_BANK_SIZE);
    memcpy(&mem[0x8000], rom->data() + 31 * SMS_BANK_SIZE, SMS_BANK_SIZE);

    std::vector<Frame> frames;
    std::vector<PsgWrite> writes;
    // The sound chips run on the console's clock, which is a little slower
    // on a PAL console.
    const uint32_t cpuHz = pal ? PAL_CPU_HZ : NTSC_CPU_HZ;
    const uint32_t frameCycles = pal ? PAL_FRAME : NTSC_FRAME;
    int loopStart = runDriver(mem.data(), tune, fm, pal, frames, writes);
    if (loopStart < 0 || loopStart >= (int) frames.size())
        return false;

    Psg psg;
    OPLL* opll = NULL;
    if (fm) {
        opll = OPLL_new(cpuHz, rate);
        if (! opll)
            return false;
        OPLL_setChipType(opll, 0);      // YM2413
        OPLL_resetPatch(opll, OPLL_2413_TONE);
        OPLL_reset(opll);
    }
    const double clocksPerSample = double(cpuHz) / 16.0 / rate;
    double pending = 0.0;       // PSG clocks not yet rendered.
    double acc = 0.0;           // PSG clocks of the current sample.
    int sum = 0, count = 0;
    double dcIn = 0.0, dcOut = 0.0;
    const double dcPole = 1.0 - 2.0 * 3.14159265 * 20.0 / rate;

    // Writes made while starting the tune.
    for (size_t w = 0; w < frames[0].first; ++w)
        chipWrite(psg, opll, writes[w]);

    samples.clear();
    for (size_t t = 0; t < frames.size(); ++t) {
        size_t end = (t + 1 < frames.size()) ? frames[t+1].first
                                              : writes.size();
        bool out = (int(t) >= loopStart);
        uint32_t at = 0;

        for (size_t w = frames[t].first; w <= end; ++w) {
            uint32_t next = (w < end) ? writes[w].cycle : frameCycles;
            if (next > frameCycles)
                next = frameCycles;
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
                    if (opll)
                        v += OPLL_calc(opll) * FM_LEVEL;
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
                chipWrite(psg, opll, writes[w]);
        }
    }
    if (opll)
        OPLL_delete(opll);
    return ! samples.empty();
}

//----------------------------------------------------------------------------

/*
 * The last tunes rendered, kept (in mono) so that going back and forth
 * between places doesn't render them again; an FM tune takes about a
 * second.
 */
#define CACHE_TUNES     3

struct CachedTune {
    const uint8_t* rom;
    int tune, rate;
    bool fm, pal;
    std::vector<int16_t> mono;
};

static std::vector<CachedTune> cache;   // The most recent first.

/*
 * Render the loop of a tune to 16-bit stereo samples.  With fm, the tune
 * is the version for the FM Sound Unit; with pal, it is played as on a
 * 50 Hz console.
 */
bool sms_renderMusic(int tune, int rate, std::vector<int16_t>& samples,
                     bool fm, bool pal)
{
    const std::vector<uint8_t>* rom = sms_rom();
    if (! rom || tune < 1 || tune > 0x7F)
        return false;

    size_t i;
    for (i = 0; i < cache.size(); ++i) {
        const CachedTune& ct = cache[i];
        if (ct.rom == rom->data() && ct.tune == tune && ct.rate == rate &&
            ct.fm == fm && ct.pal == pal)
            break;
    }
    if (i == cache.size()) {
        if (! renderTune(rom, tune, rate, fm, pal, samples))
            return false;
        if (cache.size() >= CACHE_TUNES)
            cache.pop_back();
        cache.insert(cache.begin(), CachedTune());
        CachedTune& ct = cache.front();
        ct.rom = rom->data();
        ct.tune = tune;
        ct.rate = rate;
        ct.fm = fm;
        ct.pal = pal;
        ct.mono.resize(samples.size() / 2);
        for (size_t n = 0; n < ct.mono.size(); ++n)
            ct.mono[n] = samples[n * 2];
        return true;
    }

    if (i > 0)
        std::rotate(cache.begin(), cache.begin() + i, cache.begin() + i + 1);
    const std::vector<int16_t>& mono = cache.front().mono;
    samples.resize(mono.size() * 2);
    for (size_t n = 0; n < mono.size(); ++n)
        samples[n * 2] = samples[n * 2 + 1] = mono[n];
    return true;
}
