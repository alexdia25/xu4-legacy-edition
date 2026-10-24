/*
 * XU4 - 6502 processor
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A small model of the 6502 (documented instructions, no decimal mode) to
 * run code from the C64 disks: the music player and the self-extracting
 * programs of Ultima IV Remastered.  Writes to the SID ($D400-$D41F) can be
 * logged with the cycle count.
 */

#ifndef C64CPU_H
#define C64CPU_H

#include <stdint.h>
#include <vector>

struct SidWrite {
    uint32_t cycle;         // Cycle count of the processor.
    uint8_t reg;
    uint8_t value;
};

enum {
    P6502_C = 0x01, P6502_Z = 0x02, P6502_I = 0x04, P6502_D = 0x08,
    P6502_B = 0x10, P6502_U = 0x20, P6502_V = 0x40, P6502_N = 0x80
};

// Cycles of each documented instruction (0 for the others).
static const uint8_t cpu6502Cycles[256] = {
    7,6,0,0,0,3,5,0,3,2,2,0,0,4,6,0, 2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,
    6,6,0,0,3,3,5,0,4,2,2,0,4,4,6,0, 2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,
    6,6,0,0,0,3,5,0,3,2,2,0,3,4,6,0, 2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,
    6,6,0,0,0,3,5,0,4,2,2,0,5,4,6,0, 2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,
    0,6,0,0,3,3,3,0,2,0,2,0,4,4,4,0, 2,6,0,0,4,4,4,0,2,5,2,0,0,5,0,0,
    2,6,2,0,3,3,3,0,2,2,2,0,4,4,4,0, 2,5,0,0,4,4,4,0,2,4,2,0,4,4,4,0,
    2,6,0,0,3,3,5,0,2,2,2,0,4,4,6,0, 2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,
    2,6,0,0,3,3,5,0,2,2,2,0,4,4,6,0, 2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0
};

struct Cpu6502 {
    uint8_t* mem;
    std::vector<SidWrite>* writes;
    uint32_t cycles;
    uint16_t pc;
    uint8_t a, x, y, sp, p;

    Cpu6502(uint8_t* m) : mem(m), writes(NULL), cycles(0), pc(0),
                          a(0), x(0), y(0), sp(0xFD), p(P6502_U | P6502_I) {}

    uint8_t rd(uint16_t addr) const { return mem[addr]; }
    uint16_t rd16(uint16_t addr) const {
        return mem[addr] | (mem[uint16_t(addr + 1)] << 8);
    }
    uint16_t rd16zp(uint8_t addr) const {
        return mem[addr] | (mem[uint8_t(addr + 1)] << 8);
    }
    void wr(uint16_t addr, uint8_t v) {
        mem[addr] = v;
        if (addr >= 0xD400 && addr < 0xD420 && writes) {
            SidWrite sw = { cycles, uint8_t(addr - 0xD400), v };
            writes->push_back(sw);
        }
    }
    void push(uint8_t v) { mem[0x100 + sp] = v; --sp; }
    uint8_t pull() { ++sp; return mem[0x100 + sp]; }
    uint8_t nz(uint8_t v) {
        p = (p & ~(P6502_N | P6502_Z)) | (v & P6502_N) | (v ? 0 : P6502_Z);
        return v;
    }
    void compare(uint8_t r, uint8_t v) {
        p = (p & ~P6502_C) | (r >= v ? P6502_C : 0);
        nz(uint8_t(r - v));
    }
    void adc(uint8_t v) {
        int sum = a + v + (p & P6502_C);
        p &= ~(P6502_C | P6502_V);
        if (sum > 0xFF)
            p |= P6502_C;
        if (~(a ^ v) & (a ^ sum) & 0x80)
            p |= P6502_V;
        a = nz(uint8_t(sum));
    }
    uint8_t shift(int op, uint8_t v) {
        int carry = p & P6502_C;
        switch (op) {
            case 0: p = (p & ~P6502_C) | (v >> 7); v <<= 1; break;           // ASL
            case 1: p = (p & ~P6502_C) | (v >> 7); v = (v << 1) | carry;     // ROL
                    break;
            case 2: p = (p & ~P6502_C) | (v & 1); v >>= 1; break;            // LSR
            default:p = (p & ~P6502_C) | (v & 1); v = (v >> 1) | (carry << 7);
                    break;                                              // ROR
        }
        return nz(v);
    }

    bool step();
    bool call(uint16_t addr);
};

/*
 * Run one instruction.  Return false on an unknown instruction.
 */
inline bool Cpu6502::step()
{
    uint8_t op = rd(pc++);
    uint16_t ad = 0;
    int cyc = cpu6502Cycles[op];
    if (! cyc || op == 0x00)
        return false;
    cycles += cyc;

    // Effective address of the usual groups (cc = 01, 10 & 00).
    int mode = (op >> 2) & 7;
    int cc = op & 3;
    if (cc == 1) {
        switch (mode) {
            case 0: ad = rd16zp(rd(pc++) + x); break;               // (zp,X)
            case 1: ad = rd(pc++); break;                           // zp
            case 2: ad = pc++; break;                               // #imm
            case 3: ad = rd16(pc); pc += 2; break;                  // abs
            case 4: ad = rd16zp(rd(pc++)) + y; break;               // (zp),Y
            case 5: ad = uint8_t(rd(pc++) + x); break;              // zp,X
            case 6: ad = rd16(pc) + y; pc += 2; break;              // abs,Y
            case 7: ad = rd16(pc) + x; pc += 2; break;              // abs,X
        }
        switch (op >> 5) {
            case 0: a = nz(a | rd(ad)); break;                      // ORA
            case 1: a = nz(a & rd(ad)); break;                      // AND
            case 2: a = nz(a ^ rd(ad)); break;                      // EOR
            case 3: adc(rd(ad)); break;                             // ADC
            case 4: wr(ad, a); break;                               // STA
            case 5: a = nz(rd(ad)); break;                          // LDA
            case 6: compare(a, rd(ad)); break;                      // CMP
            case 7: adc(rd(ad) ^ 0xFF); break;                      // SBC
        }
        return true;
    }

    switch (op) {
    // Branches
    case 0x10: case 0x30: case 0x50: case 0x70:
    case 0x90: case 0xB0: case 0xD0: case 0xF0:
    {
        static const uint8_t flag[4] = { P6502_N, P6502_V, P6502_C, P6502_Z };
        int8_t off = int8_t(rd(pc++));
        bool set = (p & flag[op >> 6]) != 0;
        if (set == bool(op & 0x20))
            pc += off;
        return true;
    }
    case 0x20:                                                      // JSR
        ad = rd16(pc);
        pc += 1;
        push(pc >> 8);
        push(pc & 0xFF);
        pc = ad;
        return true;
    case 0x60:                                                      // RTS
        pc = pull();
        pc |= pull() << 8;
        ++pc;
        return true;
    case 0x40:                                                      // RTI
        p = pull() | P6502_U;
        pc = pull();
        pc |= pull() << 8;
        return true;
    case 0x4C: pc = rd16(pc); return true;                          // JMP
    case 0x6C:                                                      // JMP ()
        ad = rd16(pc);
        pc = rd(ad) | (rd((ad & 0xFF00) | uint8_t(ad + 1)) << 8);
        return true;

    case 0x08: push(p | P6502_B | P6502_U); return true;                      // PHP
    case 0x28: p = pull() | P6502_U; return true;                        // PLP
    case 0x48: push(a); return true;                                // PHA
    case 0x68: a = nz(pull()); return true;                         // PLA
    case 0x18: p &= ~P6502_C; return true;
    case 0x38: p |= P6502_C; return true;
    case 0x58: p &= ~P6502_I; return true;
    case 0x78: p |= P6502_I; return true;
    case 0xB8: p &= ~P6502_V; return true;
    case 0xD8: p &= ~P6502_D; return true;
    case 0xF8: p |= P6502_D; return true;
    case 0x88: y = nz(y - 1); return true;                          // DEY
    case 0xC8: y = nz(y + 1); return true;                          // INY
    case 0xCA: x = nz(x - 1); return true;                          // DEX
    case 0xE8: x = nz(x + 1); return true;                          // INX
    case 0x8A: a = nz(x); return true;                              // TXA
    case 0x98: a = nz(y); return true;                              // TYA
    case 0xAA: x = nz(a); return true;                              // TAX
    case 0xA8: y = nz(a); return true;                              // TAY
    case 0xBA: x = nz(sp); return true;                             // TSX
    case 0x9A: sp = x; return true;                                 // TXS
    case 0xEA: return true;                                         // NOP
    case 0x0A: case 0x2A: case 0x4A: case 0x6A:                     // Shift A
        a = shift(op >> 5, a);
        return true;
    }

    // The rest of groups 10 & 00.
    bool indexY = (op == 0x96 || op == 0xB6 || op == 0xBE);         // STX, LDX
    switch (mode) {
        case 0: ad = pc++; break;                                   // #imm
        case 1: ad = rd(pc++); break;                               // zp
        case 3: ad = rd16(pc); pc += 2; break;                      // abs
        case 5: ad = uint8_t(rd(pc++) + (indexY ? y : x)); break;   // zp,X
        case 7: ad = rd16(pc) + (indexY ? y : x); pc += 2; break;   // abs,X
        default: return false;
    }
    int group = op >> 5;
    if (cc == 2) {
        switch (group) {
            case 4: wr(ad, x); break;                               // STX
            case 5: x = nz(rd(ad)); break;                          // LDX
            case 6: wr(ad, nz(rd(ad) - 1)); break;                  // DEC
            case 7: wr(ad, nz(rd(ad) + 1)); break;                  // INC
            default: wr(ad, shift(group, rd(ad))); break;           // Shifts
        }
    } else {
        uint8_t v;
        switch (group) {
            case 1:                                                 // BIT
                v = rd(ad);
                p = (p & ~(P6502_N | P6502_V | P6502_Z)) | (v & (P6502_N | P6502_V)) |
                    ((v & a) ? 0 : P6502_Z);
                break;
            case 4: wr(ad, y); break;                               // STY
            case 5: y = nz(rd(ad)); break;                          // LDY
            case 6: compare(y, rd(ad)); break;                      // CPY
            case 7: compare(x, rd(ad)); break;                      // CPX
            default: return false;
        }
    }
    return true;
}

/*
 * Call a subroutine and run it until it returns.
 */
inline bool Cpu6502::call(uint16_t addr)
{
    const uint16_t ret = 0xFFFF;
    push((ret - 1) >> 8);
    push((ret - 1) & 0xFF);
    pc = addr;
    for (int n = 0; pc != ret; ++n) {
        if (n > 1000000 || ! step())
            return false;
    }
    return true;
}

#endif
