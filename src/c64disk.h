/*
 * XU4 - Commodore 64 disk images
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef C64DISK_H
#define C64DISK_H

#include <stdint.h>
#include <vector>

class U4FILE;

enum C64Disk {
    C64_PROGRAM,
    C64_BRITANNIA,
    C64_TOWNE,
    C64_UNDERWORLD,
    C64_DISK_COUNT
};

bool c64_diskImage(const uint8_t* image, size_t size, C64Disk* role);
bool c64_imageFile(const uint8_t* image, size_t size, const char* name,
                   std::vector<uint8_t>& data);
bool c64_readFile(C64Disk disk, const char* name, std::vector<uint8_t>& data);
bool c64_read(const char* spec, std::vector<uint8_t>& data);
U4FILE* c64_open(const char* spec);
bool c64_haveDisk(C64Disk disk);
bool c64_haveRemaster();
void c64_close();

#endif
