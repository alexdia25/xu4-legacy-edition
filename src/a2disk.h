/*
 * XU4 - Apple II disk images
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef A2DISK_H
#define A2DISK_H

#include <stdint.h>
#include <vector>

class U4FILE;

enum A2Disk {
    A2_PROGRAM,
    A2_BRITANNIA,
    A2_TOWNE,
    A2_UNDERWORLD,
    A2_DISK_COUNT
};

bool a2_diskImage(const uint8_t* image, size_t size, A2Disk* role);
bool a2_imageFile(const uint8_t* image, size_t size, const char* name,
                  std::vector<uint8_t>& data);
bool a2_readFile(A2Disk disk, const char* name, std::vector<uint8_t>& data);
U4FILE* a2_open(const char* spec);
bool a2_haveDisks();
void a2_close();

#endif
