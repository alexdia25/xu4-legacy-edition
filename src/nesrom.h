/*
 * XU4 - NES ROM
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NESROM_H
#define NESROM_H

#include <stdint.h>
#include <vector>

class U4FILE;
struct NesDungeonView;

#define NES_ROM_SIZE    0x40000     // Program ROM, without the iNES header.
#define NES_BANK_SIZE   0x4000

const std::vector<uint8_t>* nes_rom();
bool nes_haveRom();
bool nes_read(const char* spec, std::vector<uint8_t>& data);
U4FILE* nes_open(const char* spec);
void nes_close();

// The first-person dungeon view of the NES game (in imageloader_nes.cpp).
NesDungeonView* nes_dungeonOpen();
void nes_dungeonClose(NesDungeonView*);
void nes_dungeonDraw(NesDungeonView*, int dx, int dy,
                     const uint8_t* level, int x, int y, int facing,
                     bool light);

#endif
