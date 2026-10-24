/*
 * XU4 - Z80 processor
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "smsz80.h"

// Cycles of the unprefixed instructions (the conditional ones not taken).
static const uint8_t z80Cycles[256] = {
    4,10,7,6,4,4,7,4,4,11,7,6,4,4,7,4,
    8,10,7,6,4,4,7,4,12,11,7,6,4,4,7,4,
    7,10,16,6,4,4,7,4,7,11,16,6,4,4,7,4,
    7,10,13,6,11,11,10,4,7,11,13,6,4,4,7,4,
    4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,
    4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,
    4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,
    7,7,7,7,7,7,4,7,4,4,4,4,4,4,7,4,
    4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,
    4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,
    4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,
    4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,
    5,10,10,10,10,11,7,11,5,10,10,0,10,17,7,11,
    5,10,10,11,10,11,7,11,5,4,10,11,10,0,7,11,
    5,10,10,19,10,11,7,11,5,4,10,4,10,0,7,11,
    5,10,10,4,10,11,7,11,5,6,10,4,10,0,7,11,
};

Z80Cpu::Z80Cpu(uint8_t* m) : mem(m), writes(0), cycles(0),
    a(0xFF), f(0xFF), b(0), c(0), d(0), e(0), h(0), l(0),
    a_(0), f_(0), b_(0), c_(0), d_(0), e_(0), h_(0), l_(0),
    ix(0xFFFF), iy(0xFFFF), sp(0xDFF0), pc(0), iff(0), im(1),
    ireg(0), rreg(0)
{
    for (int i = 0; i < 256; ++i) {
        int p = i;
        p ^= p >> 4;
        p ^= p >> 2;
        p ^= p >> 1;
        sz[i] = (i & 0x80) | (i == 0 ? FZ : 0) | (i & (FX | FY));
        szp[i] = sz[i] | ((p & 1) ? 0 : FP);
    }
}

bool Z80Cpu::cond(int y) const {
    switch (y) {
    case 0: return ! (f & FZ);
    case 1: return (f & FZ) != 0;
    case 2: return ! (f & FC);
    case 3: return (f & FC) != 0;
    case 4: return ! (f & FP);
    case 5: return (f & FP) != 0;
    case 6: return ! (f & FS);
    default: return (f & FS) != 0;
    }
}

// idx: 0 = hl, 1 = ix, 2 = iy
uint8_t Z80Cpu::getR(int i, int idx, uint16_t ea) {
    switch (i) {
    case 0: return b;
    case 1: return c;
    case 2: return d;
    case 3: return e;
    case 4: return idx == 0 ? h : idx == 1 ? ix >> 8 : iy >> 8;
    case 5: return idx == 0 ? l : idx == 1 ? uint8_t(ix) : uint8_t(iy);
    case 6: return rd(ea);
    default: return a;
    }
}

void Z80Cpu::setR(int i, int idx, uint16_t ea, uint8_t v) {
    switch (i) {
    case 0: b = v; break;
    case 1: c = v; break;
    case 2: d = v; break;
    case 3: e = v; break;
    case 4:
        if (idx == 0) h = v;
        else if (idx == 1) ix = (ix & 0xFF) | (v << 8);
        else iy = (iy & 0xFF) | (v << 8);
        break;
    case 5:
        if (idx == 0) l = v;
        else if (idx == 1) ix = (ix & 0xFF00) | v;
        else iy = (iy & 0xFF00) | v;
        break;
    case 6: wr(ea, v); break;
    default: a = v;
    }
}

uint16_t Z80Cpu::getRP(int p, int idx) const {
    switch (p) {
    case 0: return BC();
    case 1: return DE();
    case 2: return idx == 0 ? HL() : idx == 1 ? ix : iy;
    default: return sp;
    }
}

void Z80Cpu::setRP(int p, int idx, uint16_t v) {
    switch (p) {
    case 0: setBC(v); break;
    case 1: setDE(v); break;
    case 2:
        if (idx == 0) setHL(v);
        else if (idx == 1) ix = v;
        else iy = v;
        break;
    default: sp = v;
    }
}

void Z80Cpu::cbOp(int idx, uint16_t ea) {
    uint8_t op = f8();
    if (! idx)
        ea = HL();
    int x = op >> 6, y = (op >> 3) & 7, z = op & 7, reg = idx ? 6 : z;
    uint8_t v = getR(reg, 0, ea), r;
    switch (x) {
    case 0:
        r = rot(y, v);
        break;
    case 1: {
        uint8_t bit = v & (1 << y);
        f = (f & FC) | FH | (bit ? 0 : (FZ | FP)) | (bit & FS) |
            (v & (FX | FY));
        if (reg == 6)
            f = (f & ~(FX | FY)) | ((ea >> 8) & (FX | FY));
        cycles += idx ? 20 : (reg == 6 ? 12 : 8);
        return;
    }
    case 2:
        r = v & ~(1 << y);
        break;
    default:
        r = v | (1 << y);
        break;
    }
    setR(reg, 0, ea, r);
    if (idx && z != 6)
        setR(z, 0, 0, r);
    cycles += idx ? 23 : (reg == 6 ? 15 : 8);
}

bool Z80Cpu::edOp() {
    uint8_t op = f8();
    int x = op >> 6, y = (op >> 3) & 7, z = op & 7, p = y >> 1, q = y & 1;
    int cyc = 8;
    if (x == 1) {
        switch (z) {
        case 0: {
            uint8_t v = in(BC());
            f = (f & FC) | szp[v];
            if (y != 6)
                setR(y, 0, 0, v);
            cyc = 12;
            break;
        }
        case 1:
            out(BC(), y == 6 ? 0 : getR(y, 0, 0));
            cyc = 12;
            break;
        case 2:
            if (q == 0) sbc16(getRP(p, 0)); else adc16(getRP(p, 0));
            cyc = 15;
            break;
        case 3: {
            uint16_t ad = f16();
            if (q == 0) wr16(ad, getRP(p, 0)); else setRP(p, 0, rd16(ad));
            cyc = 20;
            break;
        }
        case 4: {
            uint8_t v = a;
            a = 0;
            sub8(v, 0, true);
            break;
        }
        case 5:
            pc = pop16();
            cyc = 14;
            break;
        case 6: {
            static const uint8_t mode[8] = {0, 0, 1, 2, 0, 0, 1, 2};
            im = mode[y];
            break;
        }
        default:
            switch (y) {
            case 0: ireg = a; cyc = 9; break;
            case 1: rreg = a; cyc = 9; break;
            case 2:
            case 3:
                a = (y == 2) ? ireg : rreg;
                f = (f & FC) | sz[a] | (iff ? FP : 0);
                cyc = 9;
                break;
            case 4: {                                   // RRD
                uint8_t m = rd(HL());
                wr(HL(), uint8_t((a << 4) | (m >> 4)));
                a = (a & 0xF0) | (m & 0x0F);
                f = (f & FC) | szp[a];
                cyc = 18;
                break;
            }
            case 5: {                                   // RLD
                uint8_t m = rd(HL());
                wr(HL(), uint8_t((m << 4) | (a & 0x0F)));
                a = (a & 0xF0) | (m >> 4);
                f = (f & FC) | szp[a];
                cyc = 18;
                break;
            }
            default:
                break;
            }
        }
    } else if (x == 2 && y >= 4 && z <= 3) {
        // Block instructions.
        int dir = (y & 1) ? -1 : 1;
        bool rep = (y >= 6), again;
        cyc = 16;
        switch (z) {
        case 0: {
            uint8_t v = rd(HL());
            wr(DE(), v);
            setHL(HL() + dir);
            setDE(DE() + dir);
            setBC(BC() - 1);
            uint8_t n = v + a;
            f = (f & (FS | FZ | FC)) | (BC() ? FP : 0) | (n & FX) |
                ((n << 4) & FY);
            again = rep && BC();
            break;
        }
        case 1: {
            uint8_t v = rd(HL()), r = a - v;
            setHL(HL() + dir);
            setBC(BC() - 1);
            f = (f & FC) | (sz[r] & ~(FX | FY)) | ((a ^ v ^ r) & FH) | FN |
                (BC() ? FP : 0);
            again = rep && BC() && r != 0;
            break;
        }
        case 2:
            wr(HL(), in(BC()));
            setHL(HL() + dir);
            --b;
            f = sz[b] | FN;
            again = rep && b;
            break;
        default: {
            uint8_t v = rd(HL());
            --b;
            out(BC(), v);
            setHL(HL() + dir);
            f = sz[b] | FN;
            again = rep && b;
            break;
        }
        }
        if (again) {
            pc -= 2;
            cyc = 21;
        }
    } else
        return false;
    cycles += cyc;
    return true;
}

bool Z80Cpu::mainOp(uint8_t op, int idx) {
    int x = op >> 6, y = (op >> 3) & 7, z = op & 7, p = y >> 1, q = y & 1;
    int cyc = z80Cycles[op];
    bool usesMem = (x == 1 && (y == 6 || z == 6) && op != 0x76) ||
                   (x == 2 && z == 6) ||
                   (x == 0 && (z == 4 || z == 5 || z == 6) && y == 6);
    uint16_t ea = 0;
    int extra = idx ? 4 : 0;
    if (usesMem) {
        if (idx) {
            int8_t dd = int8_t(f8());
            ea = uint16_t((idx == 1 ? ix : iy) + dd);
            cyc = (x == 0 && z != 6) ? 23 : 19;
            extra = 0;
        } else
            ea = HL();
    }
    cyc += extra;
    int ridx = usesMem ? 0 : idx;

    switch (x) {
    case 0:
        switch (z) {
        case 0:
            switch (y) {
            case 0: break;
            case 1: {
                uint8_t t = a; a = a_; a_ = t;
                t = f; f = f_; f_ = t;
                break;
            }
            case 2: {
                int8_t dd = int8_t(f8());
                if (--b) { pc += dd; cyc += 5; }
                break;
            }
            case 3: {
                int8_t dd = int8_t(f8());
                pc += dd;
                break;
            }
            default: {
                int8_t dd = int8_t(f8());
                if (cond(y - 4)) { pc += dd; cyc += 5; }
            }
            }
            break;
        case 1:
            if (q == 0)
                setRP(p, idx, f16());
            else
                setRP(2, idx, add16(getRP(2, idx), getRP(p, idx)));
            break;
        case 2:
            switch (y) {
            case 0: wr(BC(), a); break;
            case 1: a = rd(BC()); break;
            case 2: wr(DE(), a); break;
            case 3: a = rd(DE()); break;
            case 4: wr16(f16(), getRP(2, idx)); break;
            case 5: setRP(2, idx, rd16(f16())); break;
            case 6: wr(f16(), a); break;
            default: a = rd(f16());
            }
            break;
        case 3:
            setRP(p, idx, uint16_t(getRP(p, idx) + (q ? -1 : 1)));
            break;
        case 4:
            setR(y, ridx, ea, inc8(getR(y, ridx, ea)));
            break;
        case 5:
            setR(y, ridx, ea, dec8(getR(y, ridx, ea)));
            break;
        case 6:
            setR(y, ridx, ea, f8());
            break;
        default: {
            uint8_t cy;
            switch (y) {
            case 0: cy = a >> 7; a = uint8_t((a << 1) | cy); break;
            case 1: cy = a & 1; a = (a >> 1) | (cy << 7); break;
            case 2: cy = a >> 7; a = uint8_t((a << 1) | (f & FC)); break;
            case 3: cy = a & 1; a = (a >> 1) | ((f & FC) << 7); break;
            case 4: {                                   // DAA
                int corr = 0, cf = f & FC, n = f & FN, hf;
                if ((f & FH) || (a & 0x0F) > 9) corr = 6;
                if (cf || a > 0x99) { corr |= 0x60; cf = FC; }
                uint8_t r = n ? uint8_t(a - corr) : uint8_t(a + corr);
                hf = n ? (((f & FH) && (a & 0x0F) < 6) ? FH : 0)
                       : ((a & 0x0F) > 9 ? FH : 0);
                a = r;
                f = szp[r] | cf | n | hf;
                goto done;
            }
            case 5:                                     // CPL
                a ^= 0xFF;
                f = (f & (FS | FZ | FP | FC)) | FH | FN | (a & (FX | FY));
                goto done;
            case 6:                                     // SCF
                f = (f & (FS | FZ | FP)) | FC | (a & (FX | FY));
                goto done;
            default:                                    // CCF
                f = ((f & (FS | FZ | FP | FC)) | ((f & FC) ? FH : 0) |
                     (a & (FX | FY))) ^ FC;
                goto done;
            }
            f = (f & (FS | FZ | FP)) | cy | (a & (FX | FY));
        }
        }
        break;
    case 1:
        if (op == 0x76)
            return false;                               // HALT
        setR(y, z == 6 ? 0 : ridx, ea, getR(z, y == 6 ? 0 : ridx, ea));
        break;
    case 2:
        alu(y, getR(z, ridx, ea));
        break;
    default:
        switch (z) {
        case 0:
            if (cond(y)) { pc = pop16(); cyc += 6; }
            break;
        case 1:
            if (q == 0) {
                uint16_t v = pop16();
                if (p == 3) { a = v >> 8; f = uint8_t(v); }
                else setRP(p, idx, v);
            } else {
                switch (p) {
                case 0: pc = pop16(); break;
                case 1: exx(); break;
                case 2: pc = getRP(2, idx); break;
                default: sp = getRP(2, idx);
                }
            }
            break;
        case 2: {
            uint16_t t = f16();
            if (cond(y)) pc = t;
            break;
        }
        case 3:
            switch (y) {
            case 0: pc = f16(); break;
            case 2: { uint8_t n = f8(); out(uint16_t(n | (a << 8)), a); break; }
            case 3: { uint8_t n = f8(); a = in(uint16_t(n | (a << 8))); break; }
            case 4: {
                uint16_t v = rd16(sp);
                wr16(sp, getRP(2, idx));
                setRP(2, idx, v);
                break;
            }
            case 5: { uint16_t t = DE(); setDE(HL()); setHL(t); break; }
            case 6: iff = 0; break;
            default: iff = 1; break;
            }
            break;
        case 4: {
            uint16_t t = f16();
            if (cond(y)) { push16(pc); pc = t; cyc += 7; }
            break;
        }
        case 5:
            if (q == 0)
                push16(p == 3 ? uint16_t((a << 8) | f) : getRP(p, idx));
            else {
                uint16_t t = f16();
                push16(pc);
                pc = t;
            }
            break;
        case 6:
            alu(y, f8());
            break;
        default:
            push16(pc);
            pc = y * 8;
        }
    }
done:
    cycles += cyc;
    return true;
}

/*
 * Run one instruction.  Return false on HALT or an unknown instruction.
 */
bool Z80Cpu::step() {
    rreg = (rreg & 0x80) | ((rreg + 1) & 0x7F);
    uint8_t op = f8();
    if (op == 0xDD || op == 0xFD) {
        int idx = (op == 0xDD) ? 1 : 2;
        op = f8();
        if (op == 0xCB) {
            int8_t dd = int8_t(f8());
            cbOp(idx, uint16_t((idx == 1 ? ix : iy) + dd));
            return true;
        }
        if (op == 0xDD || op == 0xFD || op == 0xED) {
            cycles += 4;
            --pc;
            return true;
        }
        return mainOp(op, idx);
    }
    if (op == 0xCB) {
        cbOp(0, 0);
        return true;
    }
    if (op == 0xED)
        return edOp();
    return mainOp(op, 0);
}

/*
 * Call a subroutine and run until it returns.  Return false if it does not
 * return within the given number of cycles.
 */
bool Z80Cpu::call(uint16_t addr, uint32_t maxCycles) {
    const uint16_t stop = sp;
    uint32_t start = cycles;
    push16(0x0000);
    pc = addr;
    while (cycles - start < maxCycles) {
        if (pc == 0x0000 && sp == stop)
            return true;
        if (! step())
            return false;
    }
    return false;
}
