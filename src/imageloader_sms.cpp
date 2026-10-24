/*
 * XU4 - Sega Master System image formats
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Decodes the graphics of Ultima IV for the Sega Master System.  Colors
 * are bytes of the VDP palette: two bits each of red, green & blue.
 *
 *   smstiles  256 map graphics, 12x12 pixels, in the same order as the
 *             DOS tiles.  The data is a palette of 16 colors followed by
 *             the graphics, 128 bytes each: two columns of 8 pixels, each
 *             12 rows of a mask byte (set bits are transparent, drawn
 *             black) and four bit planes.  The last 4 pixels of the second
 *             column are always transparent.
 *   smspic    A picture: a palette of 16 colors, then four bit planes of
 *             the tile patterns, the tile numbers & the tile attributes,
 *             each run-length coded.  The pictures of the story are 22x15
 *             tiles, others 32 wide.  A story picture is centered in the
 *             part of the 320x200 screen above the text.
 *   smstree   The smspic picture of the lake & the stones, with the
 *             moongate (the map graphic, followed by the palette of the
 *             map graphics) put below the picture as it would be seen at
 *             the stones, and the bare ground beside it.
 *   smsfont   The 96 text glyphs from the space, 8 bytes each with one
 *             bit per pixel.  They replace those of the DOS charset.ega,
 *             which keeps the symbols the game uses for its own glyphs
 *             0-31.
 *
 * Run-length coding: a byte $01-$7F repeats the next byte that many
 * times, $80+n is followed by n bytes to copy, & $00 ends the data.
 */

#include <string.h>
#include <vector>

#define SMS_TILE_COUNT  256
#define SMS_TILE_W      12
#define SMS_TILE_H      12
#define SMS_PIC_X       72      // Story pictures, centered above the text.
#define SMS_PIC_Y       16
#define SMS_GATE_X      (SMS_PIC_X + 37)    // The moongate at the stones.
#define SMS_GATE_Y      (SMS_PIC_Y + 46)

static void smsPut(Image* img, int x, int y, uint8_t color)
{
    img->putPixel(x, y, (color & 3) * 85, ((color >> 2) & 3) * 85,
                  ((color >> 4) & 3) * 85, IM_OPAQUE);
}

/*
 * Decode a run-length coded stream starting at pos.  Return false if it
 * runs past the end of the data.
 */
static bool smsUnpack(const std::vector<uint8_t>& data, size_t& pos,
                      std::vector<uint8_t>& out)
{
    out.clear();
    while (pos < data.size()) {
        uint8_t t = data[pos++];
        if (t == 0)
            return true;
        if (t & 0x80) {
            size_t n = t & 0x7F;
            if (pos + n > data.size())
                return false;
            out.insert(out.end(), data.begin() + pos, data.begin() + pos + n);
            pos += n;
        } else {
            if (pos >= data.size())
                return false;
            out.insert(out.end(), t, data[pos++]);
        }
    }
    return false;
}

/*
 * Return the color of a pixel of an 8x8 pattern held in four planes (byte
 * k of each pattern row comes from plane k).
 */
static int smsPatternPixel(const std::vector<uint8_t>* planes, int tile,
                           int y, int x)
{
    size_t i = tile * 8 + y;
    if (i >= planes[0].size())
        return 0;
    int c = 0;
    for (int k = 0; k < 4; ++k)
        c |= ((planes[k][i] >> (7 - x)) & 1) << k;
    return c;
}

static bool smsPlanes(const std::vector<uint8_t>& data, size_t& pos,
                      std::vector<uint8_t>* planes)
{
    for (int k = 0; k < 4; ++k) {
        if (! smsUnpack(data, pos, planes[k]))
            return false;
        if (planes[k].size() != planes[0].size())
            return false;
    }
    return ! planes[0].empty();
}

/*
 * Draw a map graphic from data (with the palette pal) at x, y.  Its
 * transparent pixels are left as they are, or drawn black if opaque is
 * set.
 */
static void smsGraphic(Image* img, int x, int y, const uint8_t* data,
                       const uint8_t* pal, bool opaque)
{
    for (int r = 0; r < SMS_TILE_H; ++r) {
        for (int col = 0; col < 2; ++col) {
            const uint8_t* it = data + col * 60 + r * 5;
            for (int px = 0; px < 8 && col * 8 + px < SMS_TILE_W; ++px) {
                int bit = 7 - px;
                int c = 0;
                for (int k = 0; k < 4; ++k)
                    c |= ((it[1 + k] >> bit) & 1) << k;
                if (! ((it[0] >> bit) & 1))
                    smsPut(img, x + col * 8 + px, y + r, pal[c]);
                else if (opaque)
                    img->putPixel(x + col * 8 + px, y + r, 0, 0, 0,
                                  IM_OPAQUE);
            }
        }
    }
}

static Image* smsPicture(const std::vector<uint8_t>& data, size_t* end)
{
    std::vector<uint8_t> planes[4], names, attrs;
    size_t pos = 16;
    if (data.size() < 16 || ! smsPlanes(data, pos, planes) ||
        ! smsUnpack(data, pos, names) || ! smsUnpack(data, pos, attrs) ||
        names.size() != attrs.size())
        return NULL;
    const uint8_t* pal = data.data();
    if (end)
        *end = pos;

    int w = (names.size() == 22 * 15) ? 22 : 32;
    int h = (names.size() + w - 1) / w;
    int x0 = (w == 22) ? SMS_PIC_X : 0;
    int y0 = (w == 22) ? SMS_PIC_Y : 0;
    if (x0 + w * 8 > 320 || y0 + h * 8 > 200)
        return NULL;

    Image* img = Image::create(320, 200);
    img->fill(Image::black);
    for (size_t n = 0; n < names.size(); ++n) {
        int tile = names[n] | ((attrs[n] & 1) << 8);
        bool hflip = attrs[n] & 2;
        bool vflip = attrs[n] & 4;
        int tx = x0 + (n % w) * 8, ty = y0 + (n / w) * 8;
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                int c = smsPatternPixel(planes, tile, vflip ? 7 - y : y,
                                        hflip ? 7 - x : x);
                smsPut(img, tx + x, ty + y, pal[c]);
            }
        }
    }
    return img;
}

static Image* smsTiles(const std::vector<uint8_t>& data)
{
    if (data.size() < 16 + SMS_TILE_COUNT * 128)
        return NULL;

    Image* img = Image::create(SMS_TILE_W, SMS_TILE_H * SMS_TILE_COUNT);
    for (int t = 0; t < SMS_TILE_COUNT; ++t)
        smsGraphic(img, 0, t * SMS_TILE_H, &data[16 + t * 128], data.data(),
                   true);
    return img;
}

/*
 * The moongate goes at 0,152 and the ground at the stones at 12,152.
 */
static Image* smsTree(const std::vector<uint8_t>& data)
{
    size_t end;
    Image* img = smsPicture(data, &end);
    if (! img)
        return NULL;
    if (data.size() < end + 128 + 16) {
        delete img;
        return NULL;
    }
    img->drawSubRectOn(img, 0, 152, SMS_GATE_X, SMS_GATE_Y,
                       SMS_TILE_W, SMS_TILE_H);
    img->drawSubRectOn(img, SMS_TILE_W, 152, SMS_GATE_X, SMS_GATE_Y,
                       SMS_TILE_W, SMS_TILE_H);
    smsGraphic(img, 0, 152, &data[end], &data[end + 128], false);
    return img;
}

static Image* smsFont(const std::vector<uint8_t>& data)
{
    if (data.size() < 96 * 8)
        return NULL;

    const int first = ' ';
    Image* img = NULL;
    U4FILE* dos = u4fopen("charset.ega");
    if (dos) {
        img = loadImage_u4(dos, FTYPE_U4RAW, 8, 8 * 256, 4);
        u4fclose(dos);
    }
    if (img) {
        img->fillRect(0, first * 8, 8, 96 * 8, 0, 0, 0);
    } else {
        img = Image::create(8, 8 * 256);
        img->fill(Image::black);
    }

    for (int g = 0; g < 96; ++g) {
        for (int r = 0; r < 8; ++r) {
            uint8_t bits = data[g * 8 + r];
            for (int c = 0; c < 8; ++c) {
                if (bits & (0x80 >> c))
                    img->putPixel(c, (first + g) * 8 + r, 255, 255, 255,
                                  IM_OPAQUE);
            }
        }
    }
    return img;
}

static Image* loadImage_sms(U4FILE* file, int ftype)
{
    std::vector<uint8_t> data(file->length());
    data.resize(file->read(data.data(), 1, data.size()));

    switch (ftype) {
        case FTYPE_SMSTILES:
            return smsTiles(data);
        case FTYPE_SMSPIC:
            return smsPicture(data, NULL);
        case FTYPE_SMSTREE:
            return smsTree(data);
        case FTYPE_SMSFONT:
            return smsFont(data);
    }
    return NULL;
}
