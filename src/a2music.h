/*
 * XU4 - Apple II Mockingboard music
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef A2MUSIC_H
#define A2MUSIC_H

#include <stdint.h>
#include <stddef.h>
#include <vector>

bool a2_renderMusic(const uint8_t* data, size_t size, int tune, int rate,
                    std::vector<int16_t>& samples);

#endif
