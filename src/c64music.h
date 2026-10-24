/*
 * XU4 - C64 SID music
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef C64MUSIC_H
#define C64MUSIC_H

#include <stdint.h>
#include <stddef.h>
#include <vector>

bool c64_renderMusic(const uint8_t* player, size_t playerSize,
                     const uint8_t* music, size_t musicSize, int tune,
                     int rate, std::vector<int16_t>& samples);

#endif
