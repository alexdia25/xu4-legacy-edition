/*
 * XU4 - NES image formats
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Decodes the graphics of Ultima: Quest of the Avatar for the NES.  Both
 * loaders are given the whole program ROM (256K, 16 banks of 16K; bank 15
 * is the one fixed at $C000).  The NES keeps its graphics compressed and
 * copies them into video RAM as needed:
 *
 *   Pattern chunks  Bank $0A holds chunks 0-$19, bank $0B $1A-$37 and bank
 *             $0C the rest, each bank with a table of pointers at $8000.
 *             A chunk starts with its length (bit 15: stored raw) & its
 *             video address (bit 15: one bit plane used for both), then
 *             the bytes, where $81 n repeats the previous byte n-1 more
 *             times ($81 $00 is a $81).
 *   Sprites   Bank $0C (numbers below $3C) & $0D, a table of pointers at
 *             $8000, each a length then bytes coded as above.  Every four
 *             8x8 patterns make a 16x16 frame (top left, top right, bottom
 *             left, bottom right).
 *   Map sets  The tiles of a map: a table of pointers at $EB19 & of banks
 *             at $EB81.  Words 7-9 of a set's header point to its 16x16
 *             tiles (four pattern numbers each), their attributes (the low
 *             two bits pick the palette) & the palette (12 bytes).  The
 *             patterns are chunk 8 and the two chunks at $EE23 for the
 *             set; the world (set 0) uses chunk 7.
 *
 *   nestiles  The 256 map graphics of xu4, 16x16, in the order of the DOS
 *             tiles, put together from the tiles & sprites of the NES as
 *             listed in nesTileSrc.  Sprites are drawn on pure black so the
 *             Tile Backgrounds option can make it transparent; the black
 *             of everything else is drawn very dark grey to stay opaque.
 *   nesfont   The NES text glyphs in place of the DOS letters, digits &
 *             punctuation (32-127) of charset.ega, which keeps the others.
 */

#include <string.h>
#include <vector>

#define NES_BANK(b)     ((b) * 0x4000)

// The NES palette (2C02).
static const uint32_t nesRGB[64] = {
    0x666666,0x002A88,0x1412A7,0x3B00A4,0x5C007E,0x6E0040,0x6C0600,0x561D00,
    0x333500,0x0B4800,0x005200,0x004F08,0x00404D,0x000000,0x000000,0x000000,
    0xADADAD,0x155FD9,0x4240FF,0x7527FE,0xA01ACC,0xB71E7B,0xB53120,0x994E00,
    0x6B6D00,0x388700,0x0C9300,0x008F32,0x007C8D,0x000000,0x000000,0x000000,
    0xFFFEFF,0x64B0FF,0x9290FF,0xC676FF,0xF36AFF,0xFE6ECC,0xFE8170,0xEA9E22,
    0xBCBE00,0x88D800,0x5CE430,0x45E082,0x48CDDE,0x4F4F4F,0x000000,0x000000,
    0xFFFEFF,0xC0DFFF,0xD3D2FF,0xE8C8FF,0xFBC2FF,0xFEC4EA,0xFECCC5,0xF7D8A5,
    0xE4E594,0xCFEF96,0xBDF4AB,0xB3F3CC,0xB5EBF2,0xB8B8B8,0x000000,0x000000
};

#define NES_OPAQUE_BLACK    0x080808    // Black that is not made transparent.

// The four sprite palettes of the game.
static const uint8_t nesSpritePal[4][4] = {
    {0x0F,0x0F,0x21,0x37}, {0x0F,0x0F,0x25,0x37},
    {0x0F,0x0F,0x30,0x27}, {0x0F,0x0F,0x16,0x38}
};

struct NesRom {
    const std::vector<uint8_t>& prg;

    NesRom(const std::vector<uint8_t>& d) : prg(d) {}

    uint8_t rd(int bank, int addr) const {
        size_t i = NES_BANK(bank) + (addr & 0x3FFF);
        return (i < prg.size()) ? prg[i] : 0;
    }
    int rd16(int bank, int addr) const {
        return rd(bank, addr) | (rd(bank, addr + 1) << 8);
    }
    uint8_t fixed(int addr) const { return rd(15, addr); }

    // Decode the $81 run-length coding.
    void unpack(int bank, int addr, size_t len, bool raw,
                std::vector<uint8_t>& out) const {
        uint8_t last = 0;
        out.clear();
        while (out.size() < len && addr < 0xC000) {
            uint8_t b = rd(bank, addr++);
            if (b == 0x81 && ! raw) {
                uint8_t n = rd(bank, addr++);
                if (n == 0) {
                    out.push_back(0x81);
                    last = 0x81;
                } else
                    out.insert(out.end(), n - 1, last);
            } else {
                out.push_back(b);
                last = b;
            }
        }
        out.resize(len);
    }

    // Copy pattern chunk n into the 8K pattern memory vram.
    void chunk(int n, uint8_t* vram) const {
        int bank = 0x0A;
        if (n >= 0x1E) {
            n -= 0x1E;
            ++bank;
        }
        if (n >= 0x1A) {
            n -= 0x1A;
            ++bank;
        }
        int p  = rd16(bank, 0x8000 + 2 * n);
        int w0 = rd16(bank, p);
        int w1 = rd16(bank, p + 2);
        bool raw = w0 & 0x8000;
        bool onePlane = w1 & 0x8000;
        int count = w0 & 0x7FFF;
        int addr = w1 & 0x1FFF;
        std::vector<uint8_t> data;
        unpack(bank, p + 4, onePlane ? count * 8 : count, raw, data);
        if (onePlane) {
            for (int t = 0; t < count && addr + 16 <= 0x2000; ++t, addr += 16) {
                memcpy(vram + addr, &data[t * 8], 8);
                memcpy(vram + addr + 8, &data[t * 8], 8);
            }
        } else {
            size_t n2 = std::min(data.size(), size_t(0x2000 - addr));
            memcpy(vram + addr, data.data(), n2);
        }
    }

    void sprite(int k, std::vector<uint8_t>& out) const {
        int bank = 0x0C;
        if (k >= 0x3C) {
            k -= 0x3C;
            ++bank;
        }
        int p = rd16(bank, 0x8000 + 2 * k);
        unpack(bank, p + 2, rd16(bank, p), false, out);
    }

    // The palettes the game gives a sprite: bits 4-5 of a table in bank 9
    // ($AC0B), and the lower half takes another palette if bit 7 of a
    // table in the fixed bank ($DFDB) is set.
    void spritePal(int k, int* top, int* bottom) const {
        static const uint8_t lower[4] = { 1, 0, 0, 0 };
        int p = (rd(9, 0xAC0B + k) >> 4) & 3;
        *top = p;
        *bottom = (fixed(0xDFDB + k) & 0x80) ? lower[p] : p;
    }
};

// A tile of the map sets with its patterns & palette.
struct NesMapSet {
    uint8_t vram[0x2000];
    int bank, metaAddr, attrAddr;
    uint8_t pal[16];

    void load(const NesRom& rom, int m) {
        memset(vram, 0, sizeof(vram));
        rom.chunk(0, vram);
        rom.chunk(1, vram);
        rom.chunk(2, vram);
        if (m == 0) {
            rom.chunk(7, vram);
        } else {
            rom.chunk(8, vram);
            rom.chunk(rom.fixed(0xEE23 + 2 * m), vram);
            rom.chunk(rom.fixed(0xEE24 + 2 * m), vram);
        }
        int p = rom.fixed(0xEB19 + 2 * m) | (rom.fixed(0xEB1A + 2 * m) << 8);
        bank = rom.fixed(0xEB81 + m);
        metaAddr = rom.rd16(bank, p + 14);
        attrAddr = rom.rd16(bank, p + 16);
        int palAddr = rom.rd16(bank, p + 18);
        for (int k = 0, i = 0; k < 16; ++k)
            pal[k] = (k & 3) ? rom.rd(bank, palAddr + i++) : 0x0F;
    }
};

static void nesPut(Image* img, int x, int y, int color, bool keepBlack)
{
    uint32_t rgb = nesRGB[color & 63];
    if (keepBlack && rgb == 0)
        rgb = NES_OPAQUE_BLACK;
    img->putPixel(x, y, rgb >> 16, (rgb >> 8) & 0xFF, rgb & 0xFF, IM_OPAQUE);
}

// Draw an 8x8 pattern of vram (two bit planes).
static void nesPattern(Image* img, int x0, int y0, const uint8_t* pat,
                       const uint8_t* pal4, bool hflip, bool background)
{
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int bit = 7 - c;
            int v = ((pat[r] >> bit) & 1) | (((pat[r + 8] >> bit) & 1) << 1);
            int x = x0 + (hflip ? 7 - c : c);
            if (v)
                nesPut(img, x, y0 + r, pal4[v], true);
            else if (background)
                nesPut(img, x, y0 + r, pal4[0], true);
        }
    }
}

/*
 * The source of each xu4 map graphic.  Map tiles name the set, the tile &
 * optionally a palette (0-3) or four colors of their own.  Sprites name
 * the sprite, frame & palette.  Others are put together by nesSpecial.
 */
enum NesSrcType { NS_NONE, NS_MAP, NS_SPRITE, NS_LETTER, NS_SPECIAL };

struct NesTileSrc {
    uint8_t type, a, b, c, flip;
    uint8_t col[4];         // Colors to use if col[0] is not zero.
};

#define MT(s,t)         {NS_MAP, s, t, 0xFF, 0, {0,0,0,0}}
#define MTP(s,t,p)      {NS_MAP, s, t, p, 0, {0,0,0,0}}
#define MTC(s,t,c1,c2,c3) {NS_MAP, s, t, 0xFF, 0, {0x0F,c1,c2,c3}}
#define NES_PAL_GAME    0xFF    // The sprite's palettes in the game.
#define SP(g,f,p)       {NS_SPRITE, g, f, p, 0, {0,0,0,0}}
#define SN(g,f)         {NS_SPRITE, g, f, NES_PAL_GAME, 0, {0,0,0,0}}
#define SNF(g,f)        {NS_SPRITE, g, f, NES_PAL_GAME, 1, {0,0,0,0}}
#define SPF(g,f,p)      {NS_SPRITE, g, f, p, 1, {0,0,0,0}}
#define SPC(g,f,c2,c3)  {NS_SPRITE, g, f, 0, 0, {0x0F,0x0F,c2,c3}}
#define SPCF(g,f,c2,c3) {NS_SPRITE, g, f, 0, 1, {0x0F,0x0F,c2,c3}}
#define LT(ch)          {NS_LETTER, ch, 0, 0, 0, {0,0,0,0}}
#define SPECIAL         {NS_SPECIAL, 0, 0, 0, 0, {0,0,0,0}}
#define CREATURE(g)     SN(g,0), SNF(g,0), SN(g,0), SNF(g,0)
#define CREATURE_P(g,p) SP(g,0,p), SPF(g,0,p), SP(g,0,p), SPF(g,0,p)

static const NesTileSrc nesTileSrc[256] = {
    // 0: sea, water, shallows, swamp, grass, brush, forest, hills
    MT(0,0x07), MT(0,0x24), MT(0,0x0F), MT(0,0x17),
    MT(0,0x0C), MT(0,0x09), MT(0,0x3B), MT(0,0x21),
    // 8: mountains, dungeon, city, castle, town, Lord British's castle
    //    (as on the NES: grass west of it, the two halves at the gate & east)
    MT(0,0x31), MT(0,0x35), MTP(0,0x34,3), MTP(0,0x38,2),
    MT(0,0x34), SPECIAL, MT(0,0x38), MT(0,0x39),
    // 16: ship west, north, east, south; horse west, east
    SPF(0x43,4,2), SP(0x43,2,2), SP(0x43,4,2), SP(0x43,0,2),
    SPF(0x1F,2,2), SP(0x1F,2,2),
    // 22: dungeon floor, bridge, balloon, bridge north & south ends
    MT(0x2C,0x02), MT(0,0x06), SN(0x46,0), MTP(0,0x06,2), MTP(0,0x06,3),
    // 27: ladder up & down, ruins, shrine
    MT(0x13,0x07), MT(0x2D,0x10), SPECIAL, MT(0,0x36),
    // 31: avatar, mage, bard, fighter, druid, tinker, paladin, ranger,
    //     shepherd (two frames each)
    SP(0x01,0,0),
    SN(1,0), SNF(1,0), SN(2,0), SNF(2,0), SN(3,0), SNF(3,0),
    SN(4,0), SNF(4,0), SN(5,0), SNF(5,0), SN(6,0), SNF(6,0),
    SN(7,0), SNF(7,0), SN(8,0), SNF(8,0),
    // 48: column, waterside SW SE NW NE, ship mast, wheel, rocks
    MT(1,0x0F), MT(5,0x0E), MT(5,0x0F), MT(5,0x26), MT(5,0x24),
    SPECIAL, SPECIAL, MT(0x2C,0x05),
    // 56: corpse, stone wall, locked door, door, chest, ankh, brick floor,
    //     wood floor
    SPECIAL, MT(5,0x0A), MTC(5,0x14,0x16,0x27,0x37), MT(5,0x14),
    SP(0x2C,0,3), MT(0x13,0x0B), MT(5,0x02), MT(5,0x23),
    // 64: moongate opening (3 frames) & open
    SP(0x47,0,1), SP(0x47,1,1), SPF(0x47,0,1), SPF(0x47,1,1),
    // 68: poison, energy, fire & sleep fields
    SPC(0x49,0,0x1A,0x2A), SP(0x49,1,0), SP(0x49,3,3), SP(0x49,2,1),
    // 72: solid, secret door, altar, campfire, lava
    MTC(5,0x0A,0x01,0x11,0x21), MTC(5,0x03,0x00,0x2D,0x10), MT(1,0x11),
    SP(0x0A,0,3), MT(0,0x33),
    // 77: miss, magic & hit flashes
    SP(0x42,0,2), SP(0x42,1,1), SP(0x42,2,3),
    // 80: guard, villager, singing bard, jester, beggar, child, bull,
    //     Lord British
    SN(0x09,0), SNF(0x09,0), SN(0x0D,0), SNF(0x0D,0),
    SN(0x11,0), SNF(0x11,0), SN(0x10,0), SNF(0x10,0),
    SN(0x0C,0), SNF(0x0C,0), SN(0x1C,0), SNF(0x1C,0),
    SPF(0x1F,2,3), SP(0x1F,2,3), SN(0x0B,0), SN(0x0B,1),
    // 96: letters A-Z
    LT('A'),LT('B'),LT('C'),LT('D'),LT('E'),LT('F'),LT('G'),LT('H'),
    LT('I'),LT('J'),LT('K'),LT('L'),LT('M'),LT('N'),LT('O'),LT('P'),
    LT('Q'),LT('R'),LT('S'),LT('T'),LT('U'),LT('V'),LT('W'),LT('X'),
    LT('Y'),LT('Z'),
    // 122: spaces, window, black, brick wall
    LT(' '), LT(' '), LT(' '), MT(1,0x08), {NS_NONE,0,0,0,0,{0,0,0,0}},
    MT(5,0x03),
    // 128: pirate ship, nixie, squid, serpent, sea horse, whirlpool,
    //      twister
    SPF(0x44,4,3), SP(0x44,2,3), SP(0x44,4,3), SP(0x44,0,3),
    SN(0x40,0), SNF(0x40,0), SN(0x3E,0), SNF(0x3E,0),
    SN(0x2B,0), SNF(0x2B,0), SN(0x27,0), SNF(0x27,0),
    SP(0x47,0,0), SP(0x47,1,0), SPC(0x49,1,0x10,0x30), SPC(0x49,2,0x10,0x30),
    // 144: rat, bat, spider, ghost, slime, troll, gremlin, mimic (not in
    //      the chest's palette), reaper,
    //      insects, gazer, phantom, orc, skeleton, rogue, python, ettin,
    //      headless, cyclops, wisp, mage, liche, lava lizard, zorn, daemon,
    //      hydra, dragon, balron
    CREATURE(0x3B), CREATURE(0x34), CREATURE(0x37), CREATURE(0x24),
    CREATURE(0x33), CREATURE(0x3A), CREATURE(0x3F), CREATURE_P(0x2C,1),
    CREATURE(0x25), CREATURE(0x35), CREATURE(0x38), CREATURE(0x14),
    CREATURE(0x2F), CREATURE(0x23), CREATURE(0x20), CREATURE(0x39),
    CREATURE(0x31), CREATURE(0x2E), CREATURE(0x30), CREATURE(0x2D),
    CREATURE(0x36), CREATURE(0x41), CREATURE(0x3D), CREATURE(0x26),
    CREATURE(0x28), CREATURE(0x2A), CREATURE(0x29), CREATURE(0x3C)
};

// NES glyph number for an ASCII character, or 0 if the NES has none.
static int nesGlyph(int ch)
{
    if (ch >= '0' && ch <= '9') return 0x80 + ch - '0';
    if (ch >= 'A' && ch <= 'O') return 0x91 + ch - 'A';
    if (ch >= 'P' && ch <= 'Z') return 0xA0 + ch - 'P';
    if (ch >= 'a' && ch <= 'o') return 0xD1 + ch - 'a';
    if (ch >= 'p' && ch <= 'z') return 0xE0 + ch - 'p';
    switch (ch) {
        case ' ':  return 0x7F;
        case '?':  return 0x8A;
        case '!':  return 0x90;
        case '-':  return 0xD0;
        case '/':  return 0xC0;
        case '.':  return 0xC1;
        case ',':  return 0xC2;
        case '"':  return 0xC4;
        case '\'': return 0xC6;
        case '+':  return 0xC9;
        case ':':  return 0xCC;
        case ';':  return 0xCD;
    }
    return 0;
}

// Draw glyph g (in pattern memory at $1000 + g * 16) with color rgb.
static void nesDrawGlyph(Image* img, int x0, int y0, const uint8_t* vram,
                         int g, uint32_t rgb, int scale = 1)
{
    const uint8_t* pat = vram + 0x1000 + g * 16;
    for (int r = 0; r < 8 * scale; ++r)
        for (int c = 0; c < 8 * scale; ++c)
            if ((pat[r / scale] >> (7 - c / scale)) & 1)
                img->putPixel(x0 + c, y0 + r, rgb >> 16, (rgb >> 8) & 0xFF,
                              rgb & 0xFF, IM_OPAQUE);
}

class NesTileBuilder {
public:
    NesTileBuilder(const NesRom& r) : rom(r), loaded(-1) {
        memset(fontVram, 0, sizeof(fontVram));
        rom.chunk(0, fontVram);
        rom.chunk(1, fontVram);
    }

    void mapTile(Image* img, int y0, int m, int t, int palIndex,
                 const uint8_t* col) {
        if (loaded != m) {
            set.load(rom, m);
            loaded = m;
        }
        int bank = set.bank;
        int p = palIndex;
        if (p > 3)
            p = rom.rd(bank, set.attrAddr + t) & 3;
        uint8_t pal4[4];
        memcpy(pal4, set.pal + p * 4, 4);
        if (col[0])
            memcpy(pal4, col, 4);
        for (int j = 0; j < 4; ++j) {
            int pat = rom.rd(bank, set.metaAddr + 4 * t + j);
            nesPattern(img, (j & 1) * 8, y0 + (j >> 1) * 8,
                       set.vram + 0x1000 + pat * 16, pal4, false, true);
        }
    }

    // Draw a sprite frame over what is already there (black if nothing).
    void sprite(Image* img, int y0, int g, int frame, int palIndex,
                bool flip, const uint8_t* col) {
        std::vector<uint8_t> data;
        rom.sprite(g, data);
        int frames = data.size() / 64;
        if (frames == 0)
            return;
        if (frame >= frames)
            frame = frames - 1;
        int top = palIndex & 3, bottom = top;
        if (palIndex == NES_PAL_GAME)
            rom.spritePal(g, &top, &bottom);
        for (int j = 0; j < 4; ++j) {
            int x = (j & 1) * 8;
            if (flip)
                x = 8 - x;
            const uint8_t* pal4 = col[0] ? col
                                : nesSpritePal[(j < 2) ? top : bottom];
            nesPattern(img, x, y0 + (j >> 1) * 8,
                       &data[frame * 64 + j * 16], pal4, flip, false);
        }
    }

    void letter(Image* img, int y0, int ch) {
        int g = nesGlyph(ch);
        if (g && ch != ' ')
            nesDrawGlyph(img, 4, y0 + 4, fontVram, g, nesRGB[0x2C]);
    }

    void glyphOver(Image* img, int y0, int g) {
        nesDrawGlyph(img, 0, y0, fontVram, g, nesRGB[0x30], 2);
    }

    const NesRom& rom;
    NesMapSet set;
    int loaded;
    uint8_t fontVram[0x2000];
};

static const uint8_t nesNoColor[4] = {0, 0, 0, 0};

static void nesSpecial(NesTileBuilder& tb, Image* img, int t)
{
    int y0 = t * 16;
    switch (t) {
    case 13:    // West of Lord British's castle: grass, as on the NES.
        tb.mapTile(img, y0, 0, 0x0C, 0xFF, nesNoColor);
        break;
    case 29:    // Ruins: the house in grey on the grass.
        tb.mapTile(img, y0, 0, 0x34, 0xFF, nesNoColor);
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                RGBA c;
                img->getPixel(x, y0 + y, c);
                int g = (c.r * 30 + c.g * 59 + c.b * 11) / 100;
                // Blend the grey with the green of the grass.
                img->putPixel(x, y0 + y, g * 65 / 100, (g * 65 + 147 * 35) / 100,
                              g * 65 / 100, IM_OPAQUE);
            }
        break;
    case 53:    // Ship mast: the deck planks with a large x.
        tb.mapTile(img, y0, 0x23, 0x05, 0xFF, nesNoColor);
        tb.glyphOver(img, y0, 0xCB);
        break;
    case 54:    // Ship wheel: the planks with + and x drawn large.
        tb.mapTile(img, y0, 0x23, 0x05, 0xFF, nesNoColor);
        tb.glyphOver(img, y0, 0xC9);
        tb.glyphOver(img, y0, 0xCB);
        break;
    case 56:    // Corpse: the skeleton lying on its side.
    {
        Image* tmp = Image::create(16, 16);
        tmp->fill(Image::black);
        tb.sprite(tmp, 0, 0x23, 0, 0, false, nesNoColor);
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                RGBA c;
                tmp->getPixel(15 - y, x, c);   // Turned a quarter left.
                img->putPixel(x, y0 + y, c.r, c.g, c.b, IM_OPAQUE);
            }
        delete tmp;
    }
        break;
    }
}

static Image* nesTiles(const std::vector<uint8_t>& prg)
{
    if (prg.size() < NES_BANK(16))
        return NULL;
    NesRom rom(prg);
    NesTileBuilder tb(rom);

    Image* img = Image::create(16, 16 * 256);
    img->fill(Image::black);
    for (int t = 0; t < 256; ++t) {
        const NesTileSrc& s = nesTileSrc[t];
        int y0 = t * 16;
        const uint8_t* col = s.col[0] ? s.col : nesNoColor;
        switch (s.type) {
        case NS_MAP:
            tb.mapTile(img, y0, s.a, s.b, s.c, col);
            break;
        case NS_SPRITE:
            tb.sprite(img, y0, s.a, s.b, s.c, s.flip, col);
            break;
        case NS_LETTER:
            tb.letter(img, y0, s.a);
            break;
        case NS_SPECIAL:
            nesSpecial(tb, img, t);
            break;
        }
    }
    return img;
}

/*
 * The NES glyphs replace the letters, digits & punctuation of the DOS
 * charset.
 */
static Image* nesFont(const std::vector<uint8_t>& prg)
{
    if (prg.size() < NES_BANK(16))
        return NULL;
    NesRom rom(prg);
    uint8_t vram[0x2000];
    memset(vram, 0, sizeof(vram));
    rom.chunk(0, vram);

    Image* img = NULL;
    U4FILE* dos = u4fopen("charset.ega");
    if (dos) {
        img = loadImage_u4(dos, FTYPE_U4RAW, 8, 8 * 256, 4);
        u4fclose(dos);
    }
    if (! img) {
        img = Image::create(8, 8 * 256);
        img->fill(Image::black);
    }

    for (int ch = 32; ch < 128; ++ch) {
        int g = nesGlyph(ch);
        if (! g)
            continue;
        img->fillRect(0, ch * 8, 8, 8, 0, 0, 0);
        nesDrawGlyph(img, 0, ch * 8, vram, g, 0xFFFFFF);
    }
    return img;
}

static Image* loadImage_nes(U4FILE* file, int ftype)
{
    std::vector<uint8_t> data(file->length());
    data.resize(file->read(data.data(), 1, data.size()));

    switch (ftype) {
        case FTYPE_NESTILES:
            return nesTiles(data);
        case FTYPE_NESFONT:
            return nesFont(data);
    }
    return NULL;
}
