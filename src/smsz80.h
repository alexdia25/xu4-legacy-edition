/*
 * XU4 - Z80 processor
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A model of the Z80 to run the sound driver of the Sega Master System
 * release of Ultima IV.  Memory is a flat 64K: the ROM banks are copied
 * in by the caller and writes below $C000 are ignored.  Writes to the PSG
 * sound chip (ports $40-$7F) are logged with the cycle count.
 */

#ifndef SMSZ80_H
#define SMSZ80_H

#include <stdint.h>
#include <vector>

struct PsgWrite {
    uint32_t cycle;         // Cycle count of the processor.
    uint8_t value;
};

struct Z80Cpu {
    enum {
        FS = 0x80, FZ = 0x40, FY = 0x20, FH = 0x10,
        FX = 0x08, FP = 0x04, FN = 0x02, FC = 0x01
    };

    uint8_t* mem;
    std::vector<PsgWrite>* writes;
    uint32_t cycles;
    uint8_t a, f, b, c, d, e, h, l;
    uint8_t a_, f_, b_, c_, d_, e_, h_, l_;
    uint16_t ix, iy, sp, pc;
    uint8_t iff, im, ireg, rreg;
    uint8_t sz[256], szp[256];

    Z80Cpu(uint8_t* m);

    uint8_t rd(uint16_t addr) const { return mem[addr]; }
    void wr(uint16_t addr, uint8_t v) {
        if (addr >= 0xC000)
            mem[addr] = v;
    }
    uint16_t rd16(uint16_t addr) const {
        return mem[addr] | (mem[uint16_t(addr + 1)] << 8);
    }
    void wr16(uint16_t addr, uint16_t v) {
        wr(addr, uint8_t(v));
        wr(uint16_t(addr + 1), uint8_t(v >> 8));
    }
    void push16(uint16_t v) { sp -= 2; wr16(sp, v); }
    uint16_t pop16() { uint16_t v = rd16(sp); sp += 2; return v; }
    uint8_t f8() { return mem[pc++]; }
    uint16_t f16() { uint8_t lo = f8(); return lo | (f8() << 8); }

    uint8_t in(uint16_t) { return 0xFF; }
    void out(uint16_t port, uint8_t v) {
        if ((port & 0xC0) == 0x40 && writes) {
            PsgWrite pw = { cycles, v };
            writes->push_back(pw);
        }
    }

    uint16_t BC() const { return (b << 8) | c; }
    uint16_t DE() const { return (d << 8) | e; }
    uint16_t HL() const { return (h << 8) | l; }
    void setBC(uint16_t v) { b = v >> 8; c = uint8_t(v); }
    void setDE(uint16_t v) { d = v >> 8; e = uint8_t(v); }
    void setHL(uint16_t v) { h = v >> 8; l = uint8_t(v); }

    void add8(uint8_t v, int cy) {
        int r = a + v + cy;
        f = sz[r & 0xFF] | ((r >> 8) & FC) | ((a ^ v ^ r) & FH) |
            ((((a ^ ~v) & (a ^ r)) & 0x80) ? FP : 0);
        a = uint8_t(r);
    }
    void sub8(uint8_t v, int cy, bool store) {
        int r = a - v - cy;
        uint8_t fl = (sz[r & 0xFF] & ~(FX | FY)) | ((r >> 8) & FC) | FN |
                     ((a ^ v ^ r) & FH) |
                     ((((a ^ v) & (a ^ r)) & 0x80) ? FP : 0);
        if (store) {
            a = uint8_t(r);
            f = fl | (r & (FX | FY));
        } else
            f = fl | (v & (FX | FY));
    }
    void alu(int op, uint8_t v) {
        switch (op) {
        case 0: add8(v, 0); break;
        case 1: add8(v, f & FC); break;
        case 2: sub8(v, 0, true); break;
        case 3: sub8(v, f & FC, true); break;
        case 4: a &= v; f = szp[a] | FH; break;
        case 5: a ^= v; f = szp[a]; break;
        case 6: a |= v; f = szp[a]; break;
        default: sub8(v, 0, false); break;
        }
    }
    uint8_t inc8(uint8_t v) {
        uint8_t r = v + 1;
        f = (f & FC) | sz[r] | ((r & 0x0F) == 0 ? FH : 0) |
            (v == 0x7F ? FP : 0);
        return r;
    }
    uint8_t dec8(uint8_t v) {
        uint8_t r = v - 1;
        f = (f & FC) | sz[r] | FN | ((v & 0x0F) == 0 ? FH : 0) |
            (v == 0x80 ? FP : 0);
        return r;
    }
    uint16_t add16(uint16_t x, uint16_t y) {
        uint32_t r = uint32_t(x) + y;
        f = (f & (FS | FZ | FP)) | ((r >> 16) & FC) |
            (((x ^ y ^ r) >> 8) & FH) | ((r >> 8) & (FX | FY));
        return uint16_t(r);
    }
    void adc16(uint16_t y) {
        int x = HL(), r = x + y + (f & FC);
        f = ((r >> 16) & FC) | (((x ^ y ^ r) >> 8) & FH) |
            ((r >> 8) & (FS | FX | FY)) | ((r & 0xFFFF) == 0 ? FZ : 0) |
            ((((x ^ ~y) & (x ^ r)) & 0x8000) ? FP : 0);
        setHL(uint16_t(r));
    }
    void sbc16(uint16_t y) {
        int x = HL(), r = x - y - (f & FC);
        f = ((r >> 16) & FC) | FN | (((x ^ y ^ r) >> 8) & FH) |
            ((r >> 8) & (FS | FX | FY)) | ((r & 0xFFFF) == 0 ? FZ : 0) |
            ((((x ^ y) & (x ^ r)) & 0x8000) ? FP : 0);
        setHL(uint16_t(r));
    }
    uint8_t rot(int op, uint8_t v) {
        uint8_t r, cy;
        switch (op) {
        case 0: cy = v >> 7; r = uint8_t((v << 1) | cy); break;
        case 1: cy = v & 1; r = (v >> 1) | (cy << 7); break;
        case 2: cy = v >> 7; r = uint8_t((v << 1) | (f & FC)); break;
        case 3: cy = v & 1; r = (v >> 1) | ((f & FC) << 7); break;
        case 4: cy = v >> 7; r = uint8_t(v << 1); break;
        case 5: cy = v & 1; r = (v >> 1) | (v & 0x80); break;
        case 6: cy = v >> 7; r = uint8_t((v << 1) | 1); break;
        default: cy = v & 1; r = v >> 1; break;
        }
        f = szp[r] | cy;
        return r;
    }
    void exx() {
        uint8_t t;
        t = b; b = b_; b_ = t; t = c; c = c_; c_ = t;
        t = d; d = d_; d_ = t; t = e; e = e_; e_ = t;
        t = h; h = h_; h_ = t; t = l; l = l_; l_ = t;
    }

    bool cond(int y) const;
    uint8_t getR(int i, int idx, uint16_t ea);
    void setR(int i, int idx, uint16_t ea, uint8_t v);
    uint16_t getRP(int p, int idx) const;
    void setRP(int p, int idx, uint16_t v);
    void cbOp(int idx, uint16_t ea);
    bool edOp();
    bool mainOp(uint8_t op, int idx);
    bool step();
    bool call(uint16_t addr, uint32_t maxCycles = 2000000);
};

#endif
