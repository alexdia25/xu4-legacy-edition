/*
 * XU4 - NES music
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NESMUSIC_H
#define NESMUSIC_H

#include <stdint.h>
#include <vector>

bool nes_renderMusic(int tune, int rate, std::vector<int16_t>& samples);

#endif
