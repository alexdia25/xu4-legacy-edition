/*
 * XU4 - NES ROM
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NESROM_H
#define NESROM_H

#include <stdint.h>
#include <vector>

class U4FILE;

#define NES_ROM_SIZE    0x40000     // Program ROM, without the iNES header.
#define NES_BANK_SIZE   0x4000

const std::vector<uint8_t>* nes_rom();
bool nes_haveRom();
bool nes_read(const char* spec, std::vector<uint8_t>& data);
U4FILE* nes_open(const char* spec);
void nes_close();

#endif
