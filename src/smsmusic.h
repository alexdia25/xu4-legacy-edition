/*
 * XU4 - Sega Master System music
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SMSMUSIC_H
#define SMSMUSIC_H

#include <stdint.h>
#include <vector>

bool sms_renderMusic(int tune, int rate, std::vector<int16_t>& samples);

#endif
