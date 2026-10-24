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
 *             tiles, others 32 wide (only full rows are drawn).  A picture
 *             is centered across the 320x200 screen, 16 pixels from the top.
 *   smstree   The smspic picture of the lake & the stones, with the
 *             moongate (the map graphic, followed by the palette of the
 *             map graphics) put below the picture as it would be seen at
 *             the stones, and the bare ground beside it.
 *   smscards  The smspic picture of the gypsy's abacus, then the eight
 *             virtue cards, each four bit planes run-length coded followed
 *             by 6x14 tile numbers & attributes (byte pairs).  The cards are
 *             drawn in the colors of the abacus, 48x112 pixels side by side,
 *             with the white & black beads (two tiles each of the abacus)
 *             below them at 0,112 & 8,112.
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
#define SMS_CARD_W      6       // Card size in tiles.
#define SMS_CARD_H      14
#define SMS_BEAD_WHITE  11      // Abacus tiles of the beads (upper half).
#define SMS_BEAD_BLACK  9

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

/*
 * Draw an 8x8 tile pattern with the attribute bits of the name table
 * (1: tile number bit 8, 2: horizontal flip, 4: vertical flip).
 */
static void smsTile(Image* img, int tx, int ty,
                    const std::vector<uint8_t>* planes, int name, int attr,
                    const uint8_t* pal)
{
    int tile = name | ((attr & 1) << 8);
    bool hflip = attr & 2;
    bool vflip = attr & 4;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            int c = smsPatternPixel(planes, tile, vflip ? 7 - y : y,
                                    hflip ? 7 - x : x);
            smsPut(img, tx + x, ty + y, pal[c]);
        }
    }
}

struct SmsPicture {
    std::vector<uint8_t> planes[4], names, attrs;
    const uint8_t* pal;
    int w, h;

    bool decode(const std::vector<uint8_t>& data, size_t& pos) {
        pal = data.data() + pos;
        pos += 16;
        if (pos > data.size() || ! smsPlanes(data, pos, planes) ||
            ! smsUnpack(data, pos, names) || ! smsUnpack(data, pos, attrs) ||
            names.size() != attrs.size())
            return false;
        w = (names.size() == 22 * 15) ? 22 : 32;
        h = names.size() / w;
        return h > 0;
    }
};

static Image* smsPicture(const std::vector<uint8_t>& data, size_t* end,
                         SmsPicture* keep = NULL)
{
    SmsPicture local;
    SmsPicture& pic = keep ? *keep : local;
    size_t pos = 0;
    if (! pic.decode(data, pos))
        return NULL;
    if (end)
        *end = pos;

    int x0 = (320 - pic.w * 8) / 2;
    int y0 = SMS_PIC_Y;
    if (y0 + pic.h * 8 > 200)
        return NULL;

    Image* img = Image::create(320, 200);
    img->fill(Image::black);
    for (int n = 0; n < pic.w * pic.h; ++n)
        smsTile(img, x0 + (n % pic.w) * 8, y0 + (n / pic.w) * 8, pic.planes,
                pic.names[n], pic.attrs[n], pic.pal);
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

static Image* smsCards(const std::vector<uint8_t>& data)
{
    SmsPicture abacus;
    size_t pos = 0;
    if (! abacus.decode(data, pos))
        return NULL;

    const int cw = SMS_CARD_W * 8;
    const int names = SMS_CARD_W * SMS_CARD_H;
    Image* img = Image::create(8 * cw, SMS_CARD_H * 8 + 16);
    img->fill(Image::black);

    std::vector<uint8_t> planes[4];
    for (int card = 0; card < 8; ++card) {
        if (! smsPlanes(data, pos, planes) || pos + 2 * names > data.size()) {
            delete img;
            return NULL;
        }
        for (int n = 0; n < names; ++n)
            smsTile(img, card * cw + (n % SMS_CARD_W) * 8,
                    (n / SMS_CARD_W) * 8, planes, data[pos + 2 * n],
                    data[pos + 2 * n + 1], abacus.pal);
        pos += 2 * names;
    }

    const int y = SMS_CARD_H * 8;
    for (int half = 0; half < 2; ++half) {
        smsTile(img, 0, y + half * 8, abacus.planes, SMS_BEAD_WHITE + half, 0,
                abacus.pal);
        smsTile(img, 8, y + half * 8, abacus.planes, SMS_BEAD_BLACK + half, 0,
                abacus.pal);
    }
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
        case FTYPE_SMSCARDS:
            return smsCards(data);
    }
    return NULL;
}
