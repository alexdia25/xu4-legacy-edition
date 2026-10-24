/*
 * XU4 - C64 SID music
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Plays a tune of the music files (mus0-mus4) from the Ultima IV for C64
 * disks.  The game's own music player (musc, at $EC00) is run on a small
 * 6502 emulator: once to start the tune, then once for each tick of the
 * timer interrupt that drives it.  The writes it makes to the SID sound
 * chip are timed by the cycles taken and played on a model of the SID.
 *
 * The tune is run until the player comes back to a state it was in
 * before; the part between is the loop of the tune, which is what is
 * rendered (the few ticks before it are run to set up the sound chip).
 *
 * Only what the player uses is modelled: the oscillators with the
 * sawtooth, triangle, pulse & noise waveforms and the envelope generators.
 * The tunes use the sawtooth alone and no filter.
 */

#include <stdint.h>
#include <string.h>
#include <vector>
#include <unordered_map>
#include "c64cpu.h"
#include "c64music.h"

#define C64_CPU_HZ      1022727     // NTSC
#define PLAYER_ADDR     0xEC00
#define MUSIC_ADDR      0xF400
#define MAX_TICKS       20000
#define OVERSAMPLE      4

struct Tick {
    uint32_t cycles;        // Length of the tick.
    uint32_t first;         // Index of its first write.
};

//----------------------------------------------------------------------------
// SID

// Cycles between envelope steps for each attack, decay & release rate.
static const uint16_t envRate[16] = {
    9, 32, 63, 95, 149, 220, 267, 313, 392, 977, 1954, 3126, 3907, 11720,
    19532, 31251
};

enum EnvState { ENV_ATTACK, ENV_DECAY, ENV_RELEASE };

// Cycle counts are kept with 8 bits of fraction.
struct SidVoice {
    uint32_t acc;           // 24-bit oscillator (& 8 bits of fraction).
    uint32_t noise;         // Noise shift register.
    uint32_t envCount;      // Cycles (& fraction) since the last step.
    uint8_t  env;
    uint8_t  state;
    uint8_t  reg[7];
};

struct Sid {
    SidVoice voice[3];

    Sid() {
        memset(voice, 0, sizeof(voice));
        for (int i = 0; i < 3; ++i) {
            voice[i].noise = 0x7FFFF8;
            voice[i].state = ENV_RELEASE;
        }
    }

    void write(int reg, uint8_t value) {
        if (reg >= 21)
            return;             // Filter & volume are not used.
        SidVoice& v = voice[reg / 7];
        int r = reg % 7;
        if (r == 4) {
            int gate = value & 1;
            if (gate && ! (v.reg[4] & 1))
                v.state = ENV_ATTACK;
            else if (! gate && (v.reg[4] & 1))
                v.state = ENV_RELEASE;
        }
        v.reg[r] = value;
    }

    static uint32_t envPeriod(const SidVoice& v) {
        int rate;
        if (v.state == ENV_ATTACK)
            return envRate[v.reg[5] >> 4];
        rate = (v.state == ENV_DECAY) ? (v.reg[5] & 15) : (v.reg[6] & 15);
        // Decay & release slow down as the level falls.
        int mul = (v.env >= 0x5D) ? 1 : (v.env >= 0x36) ? 2 :
                  (v.env >= 0x1A) ? 4 : (v.env >= 0x0E) ? 8 :
                  (v.env >= 0x06) ? 16 : 30;
        return envRate[rate] * mul;
    }

    void clockEnvelope(SidVoice& v, uint32_t cycles) {
        v.envCount += cycles;
        uint32_t period = envPeriod(v) << 8;
        while (v.envCount >= period) {
            v.envCount -= period;
            if (v.state == ENV_ATTACK) {
                if (++v.env == 0xFF)
                    v.state = ENV_DECAY;
            } else if (v.state == ENV_DECAY) {
                uint8_t sustain = (v.reg[6] >> 4) * 0x11;
                if (v.env > sustain)
                    --v.env;
            } else if (v.env) {
                --v.env;
            }
            period = envPeriod(v) << 8;
        }
    }

    // Advance by a number of cycles and return the waveform output.
    int output(SidVoice& v, uint32_t cycles) {
        uint32_t freq = v.reg[0] | (v.reg[1] << 8);
        uint32_t prev = v.acc;
        uint8_t ctrl = v.reg[4];
        if (ctrl & 0x08) {
            v.acc = 0;          // Test bit.
        } else {
            v.acc += freq * cycles;
            // The noise register is clocked by bit 19 of the oscillator.
            uint32_t steps = ((prev & 0x0FFFFFFF) + freq * cycles) >> 28;
            while (steps--) {
                uint32_t bit = ((v.noise >> 22) ^ (v.noise >> 17)) & 1;
                v.noise = ((v.noise << 1) | bit) & 0x7FFFFF;
            }
        }

        uint32_t acc = v.acc >> 8;
        int wave = 0xFFF;
        int count = 0;
        if (ctrl & 0x10) {                                      // Triangle
            uint32_t t = (acc & 0x800000) ? ~acc : acc;
            wave &= (t >> 11) & 0xFFF;
            ++count;
        }
        if (ctrl & 0x20) {                                      // Sawtooth
            wave &= acc >> 12;
            ++count;
        }
        if (ctrl & 0x40) {                                      // Pulse
            uint32_t pw = v.reg[2] | ((v.reg[3] & 15) << 8);
            wave &= ((acc >> 12) >= pw) ? 0xFFF : 0;
            ++count;
        }
        if (ctrl & 0x80) {                                      // Noise
            uint32_t n = v.noise;
            wave &= ((n >> 11) & 0x800) | ((n >> 10) & 0x400) |
                    ((n >> 7) & 0x200) | ((n >> 5) & 0x100) |
                    ((n >> 4) & 0x080) | ((n >> 1) & 0x040) |
                    ((n << 1) & 0x020) | ((n << 2) & 0x010);
            ++count;
        }
        if (! count)
            return 0;
        return (wave - 0x800) * v.env;
    }

    // One output sample of the three voices, lasting the given cycles.
    int sample(double cycles) {
        int sum = 0;
        uint32_t sub = uint32_t(cycles * 256.0 / OVERSAMPLE + 0.5);
        for (int i = 0; i < 3; ++i) {
            SidVoice& v = voice[i];
            for (int s = 0; s < OVERSAMPLE; ++s)
                sum += output(v, sub);
            clockEnvelope(v, sub * OVERSAMPLE);
        }
        return sum / OVERSAMPLE;
    }
};

//----------------------------------------------------------------------------

static uint64_t stateHash(const uint8_t* mem)
{
    // The zero page, the stack & all the memory of the player & tune.
    uint64_t h = 1469598103934665603ULL;
    for (int i = 0; i < 0x200; ++i)
        h = (h ^ mem[i]) * 1099511628211ULL;
    for (int i = PLAYER_ADDR; i < 0x10000; ++i)
        h = (h ^ mem[i]) * 1099511628211ULL;
    return h;
}

/*
 * Run the player for a tune.  Return the index of the tick where the loop
 * starts, or -1 if the player fails.
 */
static int runPlayer(uint8_t* mem, int tune, std::vector<Tick>& ticks,
                     std::vector<SidWrite>& writes)
{
    Cpu6502 cpu(mem);
    cpu.writes = &writes;

    // Silence the SID, then start the tune as the game does.
    if (! cpu.call(0xECF5))
        return -1;
    mem[0x4D] = 1;              // Music on.
    mem[0x04] = 0;
    mem[0x02] = mem[0x03] = uint8_t(tune);
    mem[0x06] = MUSIC_ADDR & 0xFF;
    mem[0x07] = MUSIC_ADDR >> 8;
    if (! cpu.call(0xEC03))
        return -1;

    std::unordered_map<uint64_t, int> seen;
    for (int t = 0; t < MAX_TICKS; ++t) {
        uint64_t h = stateHash(mem);
        std::unordered_map<uint64_t, int>::const_iterator it = seen.find(h);
        if (it != seen.end())
            return it->second;
        seen[h] = t;

        // The timer interrupt swaps the player's zero page variables in &
        // out around a call to the player.
        Tick tk;
        tk.first = writes.size();
        cpu.cycles = 0;
        if (! cpu.call(0xECC4) || ! cpu.call(0xEE66) || ! cpu.call(0xECC4))
            return -1;
        tk.cycles = (mem[0xEC81] | (mem[0xEC82] << 8)) + 1;
        if (tk.cycles < 1000)
            return -1;
        ticks.push_back(tk);
    }
    return -1;
}

/*
 * Render the loop of a tune to 16-bit stereo samples.
 */
bool c64_renderMusic(const uint8_t* player, size_t playerSize,
                     const uint8_t* music, size_t musicSize, int tune,
                     int rate, std::vector<int16_t>& samples)
{
    if (! playerSize || playerSize > MUSIC_ADDR - PLAYER_ADDR ||
        ! musicSize || musicSize > 0x10000 - MUSIC_ADDR)
        return false;
    if (tune < 1 || tune >= music[0])
        return false;

    std::vector<uint8_t> mem(0x10000, 0);
    memcpy(&mem[PLAYER_ADDR], player, playerSize);
    memcpy(&mem[MUSIC_ADDR], music, musicSize);

    std::vector<Tick> ticks;
    std::vector<SidWrite> writes;
    int loopStart = runPlayer(mem.data(), tune, ticks, writes);
    if (loopStart < 0 || loopStart >= (int) ticks.size())
        return false;

    Sid sid;
    const double cyclesPerSample = double(C64_CPU_HZ) / rate;
    // Three voices at full level reach about 90% of full scale.
    const double gain = 0.9 * 32767.0 / (3 * 0x800 * 0xFF);
    double pending = 0.0;       // Cycles not yet rendered.

    // Writes made while starting the tune.
    for (size_t w = 0; w < ticks[0].first; ++w)
        sid.write(writes[w].reg, writes[w].value);

    samples.clear();
    for (size_t t = 0; t < ticks.size(); ++t) {
        const Tick& tk = ticks[t];
        size_t end = (t + 1 < ticks.size()) ? ticks[t+1].first : writes.size();
        bool out = (int(t) >= loopStart);
        uint32_t at = 0;

        for (size_t w = tk.first; w <= end; ++w) {
            uint32_t next = (w < end) ? writes[w].cycle : tk.cycles;
            if (next > tk.cycles)
                next = tk.cycles;
            pending += next - at;
            at = next;
            while (pending >= cyclesPerSample) {
                pending -= cyclesPerSample;
                int v = sid.sample(cyclesPerSample);
                if (out) {
                    int pcm = int(v * gain);
                    if (pcm > 32767)
                        pcm = 32767;
                    else if (pcm < -32767)
                        pcm = -32767;
                    samples.push_back((int16_t) pcm);
                    samples.push_back((int16_t) pcm);
                }
            }
            if (w < end)
                sid.write(writes[w].reg, writes[w].value);
        }
    }
    return ! samples.empty();
}
