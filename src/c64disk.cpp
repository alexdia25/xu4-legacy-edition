/*
 * XU4 - Commodore 64 disk images
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Reads data from the Ultima IV for Commodore 64 disk images (.d64, 35
 * tracks) so that modules can use the C64 graphics & music.  The game came
 * on four disk sides: Program, Britannia, Towne & Underworld.  It loads its
 * files by track & sector with its own loader, so the disks have no usable
 * directory.  The first byte of each disk holds its number (1-4), and every
 * disk carries the same music files, which is used to tell the images apart.
 *
 * Ultima IV Remastered (2.2.1, by Per Olofsson & Vanja Utne) is also read,
 * from its .d81 image or its first .d64.  Its files are normal files of the
 * disk directory: the loader (a two stage self-extracting program holding
 * the tiles, font & title screen) and GAM, which holds the other files of
 * the game, each one packed.  The loader is unpacked by running it on a
 * 6502 model until it starts the game.
 *
 * The images are looked for in the resource paths (and their u4, ultima4,
 * c64 & commodore subdirectories).
 */

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "c64cpu.h"
#include "c64disk.h"
#include "processDir.h"
#include "u4file.h"
#include "xu4.h"

using std::string;
using std::vector;

#define IMAGE_SIZE      174848      // 683 sectors.
#define IMAGE_SIZE_ERR  175531      // With a sector error table.
#define D81_SIZE        819200
#define DISK_SIDES      0x0F

/*
 * The files of the game.  Each is at the same place on every disk that has
 * it.  These are the names used in the file map of Ultima IV Remastered.
 */
struct C64File {
    const char* name;
    uint32_t offset;
    uint16_t length;
    uint16_t load;          // Load address in C64 memory.
    uint8_t  sides;         // Bit mask of C64Disk.
};

#define P   (1 << C64_PROGRAM)
#define B   (1 << C64_BRITANNIA)
#define T   (1 << C64_TOWNE)
#define U   (1 << C64_UNDERWORLD)

static const C64File c64Files[] = {
    { "tile", 0x16700, 0x2000, 0xB000, P },      // Map tiles
    { "font", 0x0F600, 0x0400, 0xE400, P },      // Text font
    { "mnu0", 0x18700, 0x1E00, 0x4000, P },      // Title screen
    { "mnu1", 0x1A500, 0x1E00, 0x7400, P },      // Title animation frames
    { "int0", 0x1C300, 0x1D00, 0x4000, P },      // Hills & tree
    { "int1", 0x1E000, 0x1D00, 0x4000, P },      // Circle of stones
    { "int2", 0x1FD00, 0x1D00, 0x4000, P },      // Fair from afar
    { "int3", 0x21A00, 0x1D00, 0x4000, P },      // Fair up close
    { "int4", 0x23700, 0x1D00, 0x4000, P },      // Gypsy wagon
    { "int5", 0x25400, 0x1D00, 0x4000, P },      // Gypsy
    { "int6", 0x10100, 0x1D00, 0x4000, P },      // Abacus
    { "gtab", 0x12F00, 0x0400, 0xE000, P },      // Graphics tables
    { "card", 0x0B500, 0x2300, 0x4000, P },      // Virtue cards
    { "col0", 0x11E00, 0x0400, 0x0400, P },      // Title screen colors
    { "col1", 0x12300, 0x0500, 0x9800, P },      // Card colors
    { "crea", 0x07E00, 0x3400, 0x6400, P },      // Character creation
    { "musc", 0x13300, 0x0800, 0xEC00, P },      // Music player
    { "mus0", 0x27200, 0x0B00, 0xF400, P|B|T|U },    // Music (Towne)
    { "mus1", 0x27D00, 0x0A00, 0xF400, P|B|T|U },    // Music (Britannia)
    { "mus2", 0x28700, 0x0A00, 0xF400, P|B|T|U },    // Music (Dungeon)
    { "mus3", 0x29100, 0x0900, 0xF400, P|B|T|U },    // Music (Castle)
    { "mus4", 0x29A00, 0x0600, 0xF400, P|B|T|U },    // Music (Lord British)
    { "infi", 0x0FF00, 0x1B00, 0x4000, U },      // End game graphics
    { NULL, 0, 0, 0, 0 }
};

#undef P
#undef B
#undef T
#undef U

static const C64File* findFile(const char* name)
{
    for (const C64File* it = c64Files; it->name; ++it) {
        if (strcmp(it->name, name) == 0)
            return it;
    }
    return NULL;
}

/*
 * Read a file from a disk image.
 */
bool c64_imageFile(const uint8_t* image, size_t size, const char* name,
                   vector<uint8_t>& data)
{
    data.clear();
    if (size != IMAGE_SIZE && size != IMAGE_SIZE_ERR)
        return false;
    const C64File* fi = findFile(name);
    if (! fi || ! (fi->sides & (1 << (image[0] - 1))))
        return false;
    data.assign(image + fi->offset, image + fi->offset + fi->length);
    return true;
}

/*
 * Identify which of the four Ultima IV disks an image is.
 */
bool c64_diskImage(const uint8_t* image, size_t size, C64Disk* role)
{
    // The header of the first music file (mus0), found on all disks.
    static const uint8_t musHeader[6] = { 0x05, 0x0B, 0x00, 0x11, 0x00, 0x70 };

    if (size != IMAGE_SIZE && size != IMAGE_SIZE_ERR)
        return false;
    if (image[0] < 1 || image[0] > C64_DISK_COUNT)
        return false;
    if (memcmp(image + 0x27200, musHeader, sizeof(musHeader)) != 0)
        return false;
    *role = C64Disk(image[0] - 1);
    return true;
}

//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// Ultima IV Remastered

/*
 * The first files of GAM, in order (from the Makefile of Ultima IV
 * Remastered): those of the Program disk.  The first .d64 of the 1541
 * version holds these & empty ones.
 */
static const uint16_t gamFiles[] = {
    0x118, 0x141, 0x145, 0x146, 0x147, 0x148, 0x149, 0x14a,
    0x14b, 0x151, 0x152, 0x153, 0x154, 0x156, 0x157, 0x158,
    0x159, 0x15a, 0x15b, 0x19f,
    0
};

// Offset of a sector in a .d64 (35 tracks) or .d81 (80 tracks of 40).
static long sectorOffset(size_t size, int track, int sec)
{
    static const uint8_t sectors[4] = { 21, 19, 18, 17 };
    long n = 0;
    if (size == D81_SIZE) {
        if (track < 1 || track > 80 || sec >= 40)
            return -1;
        n = (track - 1) * 40 + sec;
    } else {
        if (track < 1 || track > 35)
            return -1;
        for (int t = 1; t < track; ++t)
            n += sectors[(t > 17) + (t > 24) + (t > 30)];
        if (sec >= sectors[(track > 17) + (track > 24) + (track > 30)])
            return -1;
        n += sec;
    }
    return n * 256;
}

/*
 * Read a file of a disk directory by name.
 */
static bool directoryFile(const vector<uint8_t>& image, const char* name,
                          vector<uint8_t>& data)
{
    size_t size = image.size();
    bool d81 = (size == D81_SIZE);
    int t = d81 ? 40 : 18;
    int s = d81 ? 3 : 1;
    size_t nlen = strlen(name);
    int guard = 0;

    data.clear();
    while (t && ++guard < 64) {
        long off = sectorOffset(size, t, s);
        if (off < 0 || size_t(off + 256) > size)
            return false;
        const uint8_t* sec = &image[off];
        for (int i = 0; i < 8; ++i) {
            const uint8_t* ent = sec + i * 32;
            if ((ent[2] & 7) != 2)
                continue;               // Not a PRG file.
            size_t n = 0;
            while (n < 16 && ent[5 + n] != 0xA0)
                ++n;
            if (n != nlen || memcmp(ent + 5, name, n) != 0)
                continue;

            int ft = ent[3], fs = ent[4];
            int count = 0;
            while (ft && ++count < 4000) {
                long fo = sectorOffset(size, ft, fs);
                if (fo < 0 || size_t(fo + 256) > size)
                    return false;
                const uint8_t* fsec = &image[fo];
                if (fsec[0] == 0) {
                    if (fsec[1] >= 2)
                        data.insert(data.end(), fsec + 2, fsec + fsec[1] + 1);
                    break;
                }
                data.insert(data.end(), fsec + 2, fsec + 256);
                ft = fsec[0];
                fs = fsec[1];
            }
            return ! data.empty();
        }
        t = sec[0];
        s = sec[1];
    }
    return false;
}

static const char* remasterLoader(const vector<uint8_t>& image)
{
    return (image.size() == D81_SIZE) ? "ULTIMA IV" : "ULTIMA IV 1541";
}

static bool remasterImage(const vector<uint8_t>& image)
{
    vector<uint8_t> data;
    if (image.size() != D81_SIZE && image.size() != IMAGE_SIZE &&
        image.size() != IMAGE_SIZE_ERR)
        return false;
    return directoryFile(image, remasterLoader(image), data) &&
           directoryFile(image, "GAM", data);
}

/*
 * Unpack the "backpack" format of Ultima IV Remastered.  Return the number
 * of bytes read or 0 if the data is bad.
 */
static size_t unpack(const uint8_t* src, size_t size, vector<uint8_t>& out)
{
    size_t i = 0;
    uint8_t last = 0;
    out.clear();
    while (i < size) {
        int c = src[i++];
        if (c == 0) {
            return i;                   // End.
        } else if (c & 0x80) {
            // Copy 3-10 bytes from up to 4K back.
            if (i >= size)
                break;
            int len = ((c >> 4) & 7) + 3;
            int back = 0x1000 - (((c & 15) << 8) | src[i++]);
            if (back > (int) out.size())
                break;
            size_t from = out.size() - back;
            for (int n = 0; n < len; ++n)
                out.push_back(out[from + n]);
            last = out.back();
        } else if (c < 0x40) {
            // Literal bytes.
            if (i + c > size)
                break;
            out.insert(out.end(), src + i, src + i + c);
            i += c;
            last = out.back();
        } else {
            // Repeat the last byte.
            out.insert(out.end(), (c & 0x3F) + 2, last);
        }
    }
    return 0;
}

/*
 * Run a self-extracting program until it starts what it holds, which is at
 * the address done.  If that is not known (0), it is taken to be the first
 * place outside of the program, the stack & the page(s) the unpacker moves
 * itself to.
 */
static bool runExtractor(vector<uint8_t>& mem, uint16_t start,
                         uint16_t done, uint16_t* entry)
{
    Cpu6502 cpu(mem.data());
    int unpacker = -1;
    mem[1] = 0x37;
    cpu.pc = start;
    for (uint32_t n = 0; n < 20000000; ++n) {
        if (done) {
            if (cpu.pc == done) {
                *entry = done;
                return true;
            }
        } else {
            int page = cpu.pc >> 8;
            if (page != 1 && (page < 8 || page > 9)) {
                if (unpacker < 0) {
                    unpacker = page;
                } else if (page < unpacker || page > unpacker + 1) {
                    *entry = cpu.pc;
                    return true;
                }
            }
        }
        if (! cpu.step())
            return false;
    }
    return false;
}

// The address in the SYS line of a BASIC program at $0801.
static uint16_t basicStart(const vector<uint8_t>& mem)
{
    for (int i = 0x0805; i < 0x0820; ++i) {
        if (mem[i] == 0x9E)
            return (uint16_t) atoi((const char*) &mem[i + 1]);
    }
    return 0;
}

static vector<uint8_t> remaster;            // Disk image.
static vector<uint8_t> remasterMem;         // Unpacked loader.
static vector< vector<uint8_t> > remasterGam;
static bool remasterTried = false;

/*
 * Unpack the loader & the GAM files.
 */
static bool remasterPrepare()
{
    if (remasterTried)
        return ! remasterMem.empty();
    remasterTried = true;

    vector<uint8_t> prg, gam;
    if (! directoryFile(remaster, remasterLoader(remaster), prg) ||
        prg.size() < 16 || ! directoryFile(remaster, "GAM", gam))
        return false;

    // The first stage is the crack intro.  It begins by putting back the
    // BASIC line of the loader, which follows it from $0801.  (The extra
    // bytes after the memory keep the stage of an odd program in bounds.)
    vector<uint8_t> mem(0x10000 + 16, 0);
    uint16_t load = prg[0] | (prg[1] << 8);
    if (load != 0x0801 || prg.size() - 2 > size_t(0x10000 - load))
        return false;
    memcpy(&mem[load], &prg[2], prg.size() - 2);
    uint16_t entry = 0;
    if (! runExtractor(mem, basicStart(mem), 0, &entry))
        return false;
    // LDX #n  LDA stub,X  STA $0801,X
    const uint8_t* it = &mem[entry];
    if (it[0] != 0xA2 || it[2] != 0xBD || it[5] != 0x9D ||
        it[6] != 0x01 || it[7] != 0x08)
        return false;
    memmove(&mem[0x0801], &mem[it[3] | (it[4] << 8)], it[1] + 1);

    // The second stage is the loader, which starts at $2000.
    if (! runExtractor(mem, basicStart(mem), 0x2000, &entry))
        return false;
    mem.resize(0x10000);

    // GAM is the packed files, one after another, each with its load
    // address.
    vector<uint8_t> file;
    size_t pos = 0;
    remasterGam.clear();
    while (pos + 2 < gam.size()) {
        size_t used = unpack(&gam[pos + 2], gam.size() - pos - 2, file);
        if (! used)
            break;
        remasterGam.push_back(file);
        pos += 2 + used;
    }
    if (remasterGam.size() < 20)
        return false;

    remasterMem.swap(mem);
    return true;
}

/*
 * Read a file of Ultima IV Remastered: "mem" (the memory holding the
 * unpacked loader, from $0000) or a file of GAM by its number (such as
 * "145").
 */
static bool remasterFile(const char* name, vector<uint8_t>& data)
{
    if (! remasterPrepare())
        return false;
    if (strcmp(name, "mem") == 0) {
        data = remasterMem;
        return true;
    }
    unsigned long id = strtoul(name, NULL, 16);
    for (int i = 0; gamFiles[i]; ++i) {
        if (gamFiles[i] == id) {
            data = remasterGam[i];
            return ! data.empty();
        }
    }
    return false;
}

//----------------------------------------------------------------------------

static vector<uint8_t> disks[C64_DISK_COUNT];
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
    return e == "d64" || e == "d81";
}

static int scanImage(const char* name, int type, void* user)
{
    if ((type != PDIR_FILE && type != PDIR_LINK) || ! hasImageExtension(name))
        return PDIR_CONTINUE;

    string path = ((ScanContext*) user)->dir + "/" + name;
    FILE* fp = fopen(path.c_str(), "rb");
    if (! fp)
        return PDIR_CONTINUE;
    vector<uint8_t> image(D81_SIZE + 1);
    size_t n = fread(image.data(), 1, image.size(), fp);
    fclose(fp);

    C64Disk role;
    image.resize(n);
    if (c64_diskImage(image.data(), n, &role)) {
        if (disks[role].empty()) {
            if (xu4.verbose)
                printf("C64 disk %d: %s\n", role, path.c_str());
            disks[role].swap(image);
        }
    } else if (remaster.empty() && remasterImage(image)) {
        if (xu4.verbose)
            printf("Ultima IV Remastered: %s\n", path.c_str());
        remaster.swap(image);
    }
    return PDIR_CONTINUE;
}

static void findDisks()
{
    static const char* subdirs[] = {
        "", "/c64", "/commodore", "/u4", "/ultima4", NULL
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

/*
 * Read a file from one of the disks.  A file found on every disk (such as
 * the music) is read from any disk that is present.
 */
bool c64_readFile(C64Disk disk, const char* name, vector<uint8_t>& data)
{
    if (! searched)
        findDisks();
    const vector<uint8_t>& image = disks[disk];
    if (c64_imageFile(image.data(), image.size(), name, data))
        return true;

    const C64File* fi = findFile(name);
    if (fi && fi->sides == DISK_SIDES) {
        for (int i = 0; i < C64_DISK_COUNT; ++i) {
            const vector<uint8_t>& other = disks[i];
            if (c64_imageFile(other.data(), other.size(), name, data))
                return true;
        }
    }
    return false;
}

/*
 * Return true if the disk image is present.
 */
bool c64_haveDisk(C64Disk disk)
{
    if (! searched)
        findDisks();
    return ! disks[disk].empty();
}

/*
 * Return true if a disk image of Ultima IV Remastered is present.
 */
bool c64_haveRemaster()
{
    if (! searched)
        findDisks();
    return ! remaster.empty();
}

/*
 * Read the data of one or more files joined by '+', such as
 * "program/tile+gtab#1b0,100".  A part of a file can be selected with a
 * hexadecimal offset & length after a '#'.  The files of Ultima IV
 * Remastered are read with the "remaster/" disk name.
 */
bool c64_read(const char* spec, vector<uint8_t>& all)
{
    static const char* diskNames[C64_DISK_COUNT + 1] = {
        "program/", "britannia/", "towne/", "underworld/", "remaster/"
    };
    int d;
    for (d = 0; d <= C64_DISK_COUNT; ++d) {
        size_t len = strlen(diskNames[d]);
        if (strncmp(spec, diskNames[d], len) == 0) {
            spec += len;
            break;
        }
    }
    all.clear();
    if (d > C64_DISK_COUNT)
        return false;
    if (! searched)
        findDisks();

    vector<uint8_t> part;
    string names(spec);
    size_t start = 0;
    while (start <= names.size()) {
        size_t end = names.find('+', start);
        if (end == string::npos)
            end = names.size();
        string name(names.substr(start, end - start));
        size_t off = 0, len = string::npos;
        size_t hash = name.find('#');
        if (hash != string::npos) {
            unsigned long o = 0, l = 0;
            if (sscanf(name.c_str() + hash + 1, "%lx,%lx", &o, &l) != 2)
                return false;
            off = o;
            len = l;
            name.resize(hash);
        }
        bool found = (d == C64_DISK_COUNT) ?
                remasterFile(name.c_str(), part) :
                c64_readFile(C64Disk(d), name.c_str(), part);
        if (! found || off > part.size())
            return false;
        if (len > part.size() - off)
            len = part.size() - off;
        all.insert(all.end(), part.begin() + off, part.begin() + off + len);
        start = end + 1;
    }
    return true;
}

U4FILE* c64_open(const char* spec)
{
    vector<uint8_t> data;
    if (! c64_read(spec, data))
        return NULL;
    return u4fopen_buffer(data.data(), data.size());
}

void c64_close()
{
    for (int i = 0; i < C64_DISK_COUNT; ++i)
        vector<uint8_t>().swap(disks[i]);
    vector<uint8_t>().swap(remaster);
    vector<uint8_t>().swap(remasterMem);
    vector< vector<uint8_t> >().swap(remasterGam);
    remasterTried = false;
    searched = false;
}
