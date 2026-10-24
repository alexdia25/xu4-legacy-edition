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
 *   smstitle  The title screen, which the SMS builds in video memory: the
 *             two palettes, the names of "present" ($C5CE), the frame
 *             tiles (bank $15 $80BE), then bank $14 from $8000: the names
 *             of the logo (12 rows), the line strokes of the signature,
 *             the patterns of "and", the underline, "present", "Origin
 *             Systems, Inc." & of "Ultima IV" (three planes).  The logo
 *             goes at the top of the 320x200 image & the frame of the map
 *             below it, the width of the screen.
 *   smsbeasts The figures in the top corners of the title: the sprite
 *             palette, the three tables of animation steps (bank 3
 *             $889D), then bank $14 $9610: the tile patterns, the 12 tiles
 *             of each figure frame & the four tiles of each frame of the
 *             dragon's fire.  The 56 steps of the left figure (32x24
 *             frames) are stacked down the image, followed by the 56
 *             steps of the fire & the dragon (48x24).
 *   smswindow A picture of the map window (the Codex, the visions, the
 *             end): a palette of 16 colors, the pictures, then the bank
 *             (16K) holding their patterns.  A picture is the address of
 *             its patterns in the bank, the number of columns, the height
 *             in pixels & the tile numbers, column by column (8 pixels
 *             wide).  A second picture is the rune of a vision, placed
 *             where the SMS puts it in the window.  The 132x132 window is
 *             centered in the 176x176 map area of the 320x200 image.
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
#define SMS_LOGO_X      32      // The title logo (32x12 tiles).
#define SMS_LOGO_ROWS   12
#define SMS_FRAME_Y     96      // The frame of the title map.
#define SMS_FIG_STEPS   56      // Animation steps of the title figures.

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

/*
 * A copy of the video memory patterns: 4 bytes (planes) per pixel row.
 */
struct SmsVram {
    uint8_t pat[0x200 * 32];

    SmsVram() { memset(pat, 0, sizeof(pat)); }
    void set(int tile, const uint8_t* src, int len) {
        memcpy(pat + tile * 32, src, len);
    }
    int pixel(int tile, int y, int x) const {
        const uint8_t* row = pat + tile * 32 + y * 4;
        int c = 0;
        for (int k = 0; k < 4; ++k)
            c |= ((row[k] >> (7 - x)) & 1) << k;
        return c;
    }
    // Draw a name (tile number, attribute bits 9 hflip, 10 vflip, 11
    // sprite palette).  Color 0 of the sprite palette is transparent.
    void draw(Image* img, int tx, int ty, int name, const uint8_t* pal) {
        int tile = name & 0x1FF;
        if (tile == 0)
            return;
        const uint8_t* pp = (name & 0x800) ? pal + 16 : pal;
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                int c = pixel(tile, (name & 0x400) ? 7 - y : y,
                                    (name & 0x200) ? 7 - x : x);
                if (c || ! (name & 0x800))
                    smsPut(img, tx + x, ty + y, pp[c]);
            }
        }
    }
};

#define SMS_TITLE_DATA  (16 + 16 + 14 + 0x140 + 0x1410)

static Image* smsTitle(const std::vector<uint8_t>& data)
{
    if (data.size() < SMS_TITLE_DATA)
        return NULL;
    const uint8_t* pal = &data[0];          // Background, then sprite.
    const uint8_t* present = &data[32];
    const uint8_t* frame = &data[46];
    const uint8_t* b14 = &data[46 + 0x140]; // Bank $14 from $8000.
    SmsVram* vram = new SmsVram;
    int i, y;

    vram->set(0xE9, frame, 0x140);
    vram->set(0x10E, b14 + 0x500, 3 * 32);          // and
    vram->set(0x111, b14 + 0x5E0, 13 * 32);         // Origin Systems, Inc.
    vram->set(0x1F2, b14 + 0x560, 4 * 32);          // The underline.
    for (i = 0; i < 6; ++i) {                       // present
        vram->set(0x11E + i, b14 + 0x5C0, 12);
        memcpy(vram->pat + (0x11E + i) * 32 + 12, b14 + 0x780 + i * 32, 20);
    }
    for (i = 0; i < 126; ++i) {                     // Ultima IV
        for (y = 0; y < 8; ++y)
            memcpy(vram->pat + (0x142 + i) * 32 + y * 4,
                   b14 + 0x840 + i * 24 + y * 3, 3);
    }

    uint16_t names[SMS_LOGO_ROWS * 32];
    for (i = 0; i < SMS_LOGO_ROWS * 32; ++i)
        names[i] = b14[i * 2] | (b14[i * 2 + 1] << 8);
    // Row 4: the underline with "present" in the middle (put there as the
    // title is drawn).  The fire of the dragon is left to smsbeasts.
    for (i = 7; i < 26; ++i)
        names[4 * 32 + i] = 0x9F5;
    names[4 * 32 + 25] = 0x9F3;
    for (i = 0; i < 7; ++i)
        names[4 * 32 + 13 + i] = present[i * 2] | (present[i * 2 + 1] << 8);
    names[1 * 32 + 25] = names[1 * 32 + 26] = 0;
    names[2 * 32 + 25] = names[2 * 32 + 26] = 0;

    // Plot the signature, a list of strokes: the first point (x, y), then
    // steps of a byte each (low nibble x, high nibble y, signed) up to $88.
    // A 0 ends the list.
    size_t pos = 0x380;
    while (pos + 2 < 0x500 && b14[pos]) {
        int x = b14[pos++];
        y = b14[pos++];
        for (;;) {
            int n = names[(y >> 3) % SMS_LOGO_ROWS * 32 + (x >> 3)] & 0x1FF;
            uint8_t* row = vram->pat + n * 32 + (y & 7) * 4;
            uint8_t bit = 0x80 >> (x & 7);
            row[0] |= bit;
            row[1] &= ~bit;
            row[2] &= ~bit;
            row[3] &= ~bit;
            if (pos >= 0x500)
                break;
            uint8_t step = b14[pos++];
            if (step == 0x88)
                break;
            x = (x + (int8_t(step << 4) >> 4)) & 0xFF;
            y = (y + (int8_t(step) >> 4)) & 0xFF;
        }
    }

    Image* img = Image::create(320, 200);
    img->fill(Image::black);
    for (i = 0; i < SMS_LOGO_ROWS * 32; ++i)
        vram->draw(img, SMS_LOGO_X + (i % 32) * 8, (i / 32) * 8, names[i],
                   pal);

    // The frame of the map, as wide as the screen.
    const int rows = 12, cols = 40;
    for (y = 0; y < rows; ++y) {
        int ty = SMS_FRAME_Y + y * 8;
        for (i = 0; i < cols; ++i) {
            int n;
            if (y == 0)
                n = (i == 0) ? 0xE9 : (i == cols - 1) ? 0xEB : 0xEA;
            else if (y == rows - 1)
                n = (i == 0) ? 0xEF : (i == cols - 1) ? 0xF2 :
                    (i == cols - 2) ? 0xF1 : 0xF0;
            else if (i == 0)
                n = (y == 1) ? 0xEC : 0xEE;
            else if (i == cols - 1)
                n = 0xED;
            else
                continue;
            vram->draw(img, i * 8, ty, n, pal);
        }
    }
    delete vram;
    return img;
}

#define SMS_BEAST_DATA  (16 + 3 * SMS_FIG_STEPS + 0x1878)

static Image* smsBeasts(const std::vector<uint8_t>& data)
{
    if (data.size() < SMS_BEAST_DATA)
        return NULL;
    uint8_t pal[32];
    memset(pal, 0, 16);
    memcpy(pal + 16, &data[0], 16);
    const uint8_t* steps = &data[16];
    const uint8_t* b14 = &data[16 + 3 * SMS_FIG_STEPS];    // From $9610.
    const uint8_t* figTiles = b14 + 0x15A0;     // 12 per frame ($ABB0).
    const uint8_t* fire = b14 + 0x1678;         // $AC88

    SmsVram* vram = new SmsVram;
    Image* img = Image::create(48, 2 * 24 * SMS_FIG_STEPS);
    img->fill(Image::black);

    // The fire stays as last drawn when a step has none ($FF), so go
    // around the steps once to find what is shown at the start.
    int fireFrame = -1;
    for (int pass = 0; pass < 2; ++pass) {
        for (int s = 0; s < SMS_FIG_STEPS; ++s) {
            if (steps[2 * SMS_FIG_STEPS + s] != 0xFF)
                fireFrame = steps[2 * SMS_FIG_STEPS + s] & 3;
            if (pass == 0)
                continue;

            int y = s * 24;
            for (int fig = 0; fig < 2; ++fig) {
                const uint8_t* ft = figTiles + steps[fig * SMS_FIG_STEPS + s] * 12;
                for (int k = 0; k < 12; ++k) {
                    vram->set(1 + k, b14 + ft[k] * 32, 32);
                    vram->draw(img, fig * 16 + (k & 3) * 8,
                               fig * 24 * SMS_FIG_STEPS + y + (k >> 2) * 8,
                               0x800 | (1 + k), pal);
                }
            }
            if (fireFrame >= 0) {
                for (int k = 0; k < 4; ++k) {
                    vram->set(1 + k, fire + fireFrame * 0x80 + k * 32, 32);
                    vram->draw(img, (k & 1) * 8,
                               24 * SMS_FIG_STEPS + y + 8 + (k >> 1) * 8,
                               0x800 | (1 + k), pal);
                }
            }
        }
    }
    delete vram;
    return img;
}

#define SMS_WIN_COLS    17      // Pattern columns of the map window.
#define SMS_WIN_SIZE    132     // Visible width & height (11 tiles of 12).
#define SMS_WIN_X       (8 + (176 - SMS_WIN_SIZE) / 2)
#define SMS_BANK        0x4000

static Image* smsWindow(const std::vector<uint8_t>& data)
{
    if (data.size() < 16 + SMS_BANK)
        return NULL;
    const uint8_t* pal = &data[0];
    const uint8_t* bank = &data[data.size() - SMS_BANK];
    size_t end = data.size() - SMS_BANK;

    // The window patterns: 8 pixel wide columns of 4 bytes per row.
    const int colBytes = 0x220;
    std::vector<uint8_t> win(SMS_WIN_COLS * colBytes, 0);
    int at[2] = { 0, 0x0DA0 - 0x20 };   // Where $7E78 is called to draw.

    size_t pos = 16;
    for (int n = 0; n < 2 && pos + 4 <= end; ++n) {
        int base = (data[pos] | data[pos+1] << 8) - 0x8000;
        int cols = data[pos+2];
        int rows = data[pos+3];
        pos += 4;
        for (int c = 0; c < cols; ++c) {
            int d = at[n] + c * colBytes;
            for (int left = rows; left > 0; left -= 8) {
                if (pos >= end)
                    return NULL;
                int src = base + data[pos++] * 32;
                int len = (left >= 8) ? 32 : left * 4;
                if (src < 0 || src + len > SMS_BANK ||
                    d + len > int(win.size()))
                    return NULL;
                memcpy(&win[d], bank + src, len);
                d += len;
            }
        }
    }

    Image* img = Image::create(320, 200);
    img->fill(Image::black);
    for (int y = 0; y < SMS_WIN_SIZE; ++y) {
        for (int x = 0; x < SMS_WIN_SIZE; ++x) {
            const uint8_t* row = &win[(x >> 3) * colBytes + y * 4];
            int c = 0;
            for (int k = 0; k < 4; ++k)
                c |= ((row[k] >> (7 - (x & 7))) & 1) << k;
            smsPut(img, SMS_WIN_X + x, SMS_WIN_X + y, pal[c]);
        }
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
        case FTYPE_SMSTITLE:
            return smsTitle(data);
        case FTYPE_SMSBEASTS:
            return smsBeasts(data);
        case FTYPE_SMSWINDOW:
            return smsWindow(data);
    }
    return NULL;
}
