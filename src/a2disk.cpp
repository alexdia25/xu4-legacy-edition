/*
 * XU4 - Apple II disk images
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Reads files from the four Ultima IV for Apple II disk images (140K,
 * DOS 3.3 sector order) so that modules can use the Apple II graphics.
 * The images are looked for in the resource paths (and their u4, ultima4,
 * apple2 & a2 subdirectories) and are told apart by their catalogs.
 */

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include "a2disk.h"
#include "processDir.h"
#include "u4file.h"
#include "xu4.h"

using std::string;
using std::vector;

#define SECTOR_SIZE     256
#define SECTORS         16
#define TRACKS          35
#define IMAGE_SIZE      (TRACKS * SECTORS * SECTOR_SIZE)
#define VTOC_TRACK      17

static const uint8_t* sector(const uint8_t* image, int track, int sec)
{
    if (track >= TRACKS || sec >= SECTORS)
        return NULL;
    return image + (track * SECTORS + sec) * SECTOR_SIZE;
}

/*
 * Find a catalog entry by name.  Return a pointer to the 35 byte entry or
 * NULL if not found.
 */
static const uint8_t* catalogEntry(const uint8_t* image, const char* name)
{
    const uint8_t* vtoc = sector(image, VTOC_TRACK, 0);
    const uint8_t* cat = sector(image, vtoc[1], vtoc[2]);
    size_t nlen = strlen(name);
    int guard = 0;

    while (cat && ++guard < 64) {
        for (int i = 0; i < 7; ++i) {
            const uint8_t* ent = cat + 0x0B + i * 35;
            if (ent[0] == 0)
                return NULL;            // End of catalog.
            if (ent[0] == 0xFF)
                continue;               // Deleted file.

            // Names are high-bit ASCII padded with spaces.  Control
            // characters (hidden on the screen) are skipped.
            const uint8_t* fn = ent + 3;
            size_t n = 0;
            int j;
            for (j = 0; j < 30; ++j) {
                int ch = fn[j] & 0x7F;
                if (ch < ' ')
                    continue;
                if (n == nlen) {
                    if (ch != ' ')
                        n = nlen + 1;   // Name is longer.
                    break;
                }
                if (ch != toupper((unsigned char) name[n]))
                    break;
                ++n;
            }
            if (n == nlen)
                return ent;
        }
        if (cat[1] == 0)
            break;
        cat = sector(image, cat[1], cat[2]);
    }
    return NULL;
}

/*
 * Read a file from a disk image.  For binary files the four byte header
 * (load address & length) is removed.
 */
bool a2_imageFile(const uint8_t* image, size_t size, const char* name,
                  vector<uint8_t>& data)
{
    data.clear();
    if (size != IMAGE_SIZE)
        return false;
    const uint8_t* ent = catalogEntry(image, name);
    if (! ent)
        return false;

    const uint8_t* tsl = sector(image, ent[0], ent[1]);
    int guard = 0;
    while (tsl && ++guard < 64) {
        for (int i = 0; i < 122; ++i) {
            int t = tsl[0x0C + i*2];
            int s = tsl[0x0D + i*2];
            if (t == 0 && s == 0)
                goto done;
            const uint8_t* sec = sector(image, t, s);
            if (! sec)
                return false;
            data.insert(data.end(), sec, sec + SECTOR_SIZE);
        }
        if (tsl[1] == 0)
            break;
        tsl = sector(image, tsl[1], tsl[2]);
    }
done:
    if ((ent[2] & 0x7F) == 0x04 && data.size() >= 4) {
        // Binary file.
        size_t len = data[2] | (data[3] << 8);
        if (len > data.size() - 4)
            len = data.size() - 4;
        data.erase(data.begin(), data.begin() + 4);
        data.resize(len);
    }
    return true;
}

/*
 * Identify which of the four Ultima IV disks an image is.
 */
bool a2_diskImage(const uint8_t* image, size_t size, A2Disk* role)
{
    static const char* marker[A2_DISK_COUNT] = {
        "ULT4", "LORD", "TALK", "DNGD"
    };
    if (size != IMAGE_SIZE)
        return false;
    const uint8_t* vtoc = sector(image, VTOC_TRACK, 0);
    if (vtoc[3] != 3 || vtoc[1] >= TRACKS)
        return false;               // Not DOS 3.3.
    for (int i = 0; i < A2_DISK_COUNT; ++i) {
        if (catalogEntry(image, marker[i])) {
            *role = A2Disk(i);
            return true;
        }
    }
    return false;
}

//----------------------------------------------------------------------------

static vector<uint8_t> disks[A2_DISK_COUNT];
static bool searched = false;

struct ScanContext {
    string dir;
};

static bool hasImageExtension(const char* name)
{
    const char* ext = strrchr(name, '.');
    if (! ext)
        return false;
    string e(ext + 1);
    for (size_t i = 0; i < e.size(); ++i)
        e[i] = tolower((unsigned char) e[i]);
    return e == "dsk" || e == "do";
}

static int scanImage(const char* name, int type, void* user)
{
    if ((type != PDIR_FILE && type != PDIR_LINK) || ! hasImageExtension(name))
        return PDIR_CONTINUE;

    string path = ((ScanContext*) user)->dir + "/" + name;
    FILE* fp = fopen(path.c_str(), "rb");
    if (! fp)
        return PDIR_CONTINUE;
    vector<uint8_t> image(IMAGE_SIZE + 1);
    size_t n = fread(image.data(), 1, image.size(), fp);
    fclose(fp);

    A2Disk role;
    image.resize(n);
    if (a2_diskImage(image.data(), n, &role) && disks[role].empty()) {
        if (xu4.verbose)
            printf("Apple II disk %d: %s\n", role, path.c_str());
        disks[role].swap(image);
    }
    return PDIR_CONTINUE;
}

static void findDisks()
{
    static const char* subdirs[] = {
        "", "/apple2", "/a2", "/u4", "/ultima4", NULL
    };
    const StringTable* st = &xu4.resourcePaths;
    const char* root = sst_strings(st);
    ScanContext ctx;

    searched = true;
    for (uint32_t i = 0; i < st->used; ++i) {
        for (const char** sub = subdirs; *sub; ++sub) {
            ctx.dir = string(root + sst_start(st, i)) + *sub;
            processDir(ctx.dir.c_str(), scanImage, &ctx);
        }
    }
}

bool a2_readFile(A2Disk disk, const char* name, vector<uint8_t>& data)
{
    if (! searched)
        findDisks();
    const vector<uint8_t>& image = disks[disk];
    return a2_imageFile(image.data(), image.size(), name, data);
}

/*
 * Open the data of one or more files joined by '+', such as
 * "program/SHP0+SHP1".
 */
U4FILE* a2_open(const char* spec)
{
    static const char* diskNames[A2_DISK_COUNT] = {
        "program/", "britannia/", "towne/", "underworld/"
    };
    int d;
    for (d = 0; d < A2_DISK_COUNT; ++d) {
        size_t len = strlen(diskNames[d]);
        if (strncmp(spec, diskNames[d], len) == 0) {
            spec += len;
            break;
        }
    }
    if (d == A2_DISK_COUNT)
        return NULL;

    vector<uint8_t> all, part;
    string names(spec);
    size_t start = 0;
    while (start <= names.size()) {
        size_t end = names.find('+', start);
        if (end == string::npos)
            end = names.size();
        if (! a2_readFile(A2Disk(d), names.substr(start, end - start).c_str(),
                          part))
            return NULL;
        all.insert(all.end(), part.begin(), part.end());
        start = end + 1;
    }
    return u4fopen_buffer(all.data(), all.size());
}

void a2_close()
{
    for (int i = 0; i < A2_DISK_COUNT; ++i)
        vector<uint8_t>().swap(disks[i]);
    searched = false;
}
