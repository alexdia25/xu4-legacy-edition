/*
 * XU4 - Sega Master System ROM
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Reads data from the ROM of Ultima IV: Quest of the Avatar for the Sega
 * Master System (Europe/USA, 512K) so that modules can use its graphics &
 * music.  The ROM is looked for in the resource paths (and their sms, u4 &
 * ultima4 subdirectories) as a file with the .sms extension, and is only
 * used if its SHA-1 matches, as the modules read it by offset.
 */

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "smsrom.h"
#include "processDir.h"
#include "u4file.h"
#include "xu4.h"

using std::string;
using std::vector;

static const char* romSha1 = "a90e21e5961bcf2e10b715a009c04e7c2017a3b1";

static vector<uint8_t> rom;
static bool searched = false;

static void sha1(const uint8_t* data, size_t len, char out[41])
{
    uint32_t hv[5] = {
        0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0
    };
    size_t total = ((len + 8) / 64 + 1) * 64;
    vector<uint8_t> m(total, 0);
    memcpy(m.data(), data, len);
    m[len] = 0x80;
    uint64_t bits = uint64_t(len) * 8;
    for (int i = 0; i < 8; ++i)
        m[total - 1 - i] = uint8_t(bits >> (8 * i));

#define ROL(x, n)   (((x) << (n)) | ((x) >> (32 - (n))))
    for (size_t c = 0; c < total; c += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i)
            w[i] = uint32_t(m[c + 4*i]) << 24 | m[c + 4*i + 1] << 16 |
                   m[c + 4*i + 2] << 8 | m[c + 4*i + 3];
        for (int i = 16; i < 80; ++i)
            w[i] = ROL(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
        uint32_t a = hv[0], b = hv[1], cc = hv[2], d = hv[3], e = hv[4];
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & cc) | (~b & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ cc ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & cc) | (b & d) | (cc & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ cc ^ d;
                k = 0xCA62C1D6;
            }
            uint32_t t = ROL(a, 5) + f + e + k + w[i];
            e = d;
            d = cc;
            cc = ROL(b, 30);
            b = a;
            a = t;
        }
        hv[0] += a;
        hv[1] += b;
        hv[2] += cc;
        hv[3] += d;
        hv[4] += e;
    }
#undef ROL
    for (int i = 0; i < 5; ++i)
        sprintf(out + 8 * i, "%08x", hv[i]);
}

struct ScanContext {
    string dir;
};

static int scanRom(const char* name, int type, void* user)
{
    if ((type != PDIR_FILE && type != PDIR_LINK) || ! rom.empty())
        return PDIR_CONTINUE;
    const char* ext = strrchr(name, '.');
    if (! ext || strlen(ext) != 4 || tolower(ext[1]) != 's' ||
        tolower(ext[2]) != 'm' || tolower(ext[3]) != 's')
        return PDIR_CONTINUE;

    string path = ((ScanContext*) user)->dir + "/" + name;
    FILE* fp = fopen(path.c_str(), "rb");
    if (! fp)
        return PDIR_CONTINUE;
    vector<uint8_t> image(SMS_ROM_SIZE + 1);
    size_t n = fread(image.data(), 1, image.size(), fp);
    fclose(fp);
    if (n != SMS_ROM_SIZE)
        return PDIR_CONTINUE;

    char sum[41];
    image.resize(n);
    sha1(image.data(), n, sum);
    if (strcmp(sum, romSha1) == 0) {
        if (xu4.verbose)
            printf("SMS ROM: %s\n", path.c_str());
        rom.swap(image);
    }
    return PDIR_CONTINUE;
}

static void findRom()
{
    static const char* subdirs[] = {
        "", "/sms", "/u4", "/ultima4", NULL
    };
    const StringTable* st = &xu4.resourcePaths;
    const char* root = sst_strings(st);
    ScanContext ctx;

    searched = true;
    for (uint32_t i = 0; i < st->used; ++i) {
        for (const char** sub = subdirs; *sub; ++sub) {
            ctx.dir = string(root + sst_start(st, i)) + *sub;
            processDir(ctx.dir.c_str(), scanRom, &ctx);
        }
    }
}

/*
 * Return the ROM, or NULL if it was not found.
 */
const vector<uint8_t>* sms_rom()
{
    if (! searched)
        findRom();
    return rom.empty() ? NULL : &rom;
}

bool sms_haveRom()
{
    return sms_rom() != NULL;
}

/*
 * Read parts of the ROM joined by '+', each a hexadecimal offset & length
 * such as "7c83,20+18000,1000".
 */
bool sms_read(const char* spec, vector<uint8_t>& all)
{
    all.clear();
    if (! sms_haveRom())
        return false;

    string parts(spec);
    size_t start = 0;
    while (start <= parts.size()) {
        size_t end = parts.find('+', start);
        if (end == string::npos)
            end = parts.size();
        unsigned long off, len;
        if (sscanf(parts.c_str() + start, "%lx,%lx", &off, &len) != 2 ||
            off > rom.size() || len > rom.size() - off)
            return false;
        all.insert(all.end(), rom.begin() + off, rom.begin() + off + len);
        start = end + 1;
    }
    return true;
}

U4FILE* sms_open(const char* spec)
{
    vector<uint8_t> data;
    if (! sms_read(spec, data))
        return NULL;
    return u4fopen_buffer(data.data(), data.size());
}

void sms_close()
{
    vector<uint8_t>().swap(rom);
    searched = false;
}
