/*
 * XU4 - Sega Master System ROM
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SMSROM_H
#define SMSROM_H

#include <stdint.h>
#include <vector>

class U4FILE;

#define SMS_ROM_SIZE    0x80000
#define SMS_BANK_SIZE   0x4000

const std::vector<uint8_t>* sms_rom();
bool sms_haveRom();
bool sms_read(const char* spec, std::vector<uint8_t>& data);
U4FILE* sms_open(const char* spec);
void sms_close();

#endif
