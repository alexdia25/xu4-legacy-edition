/*
 * imageloader_cga.cpp
 *
 * The CGA graphics of the DOS game, read from its own files:
 *
 *   cgatiles  SHAPES.CGA: 256 map tiles of 16x16 pixels, 2 bits a pixel,
 *             64 bytes each with the even rows first, then the odd rows.
 *   cgafont   CHARSET.CGA: 128 glyphs of 8x8, 16 bytes each, interlaced
 *             like the tiles.  They replace glyphs 0-127 of CHARSET.EGA,
 *             which keeps the others.
 *   cgapic    A *.PIC screen of 320x200.  Pictures compressed with LZW hold
 *             a BSAVE of CGA memory (a 7 byte header, then the even lines
 *             and the odd lines 8192 bytes apart).  The others are PC Paint
 *             (Pictor) files whose run-length blocks hold the lines in
 *             order.
 *
 * The game used CGA palette 1 in high intensity: black, cyan, magenta and
 * white.
 */

static const uint8_t cgaPalette[4][3] = {
    {0x00, 0x00, 0x00}, {0x55, 0xFF, 0xFF}, {0xFF, 0x55, 0xFF}, {0xFF, 0xFF, 0xFF}
};

static inline void cgaPut(Image* img, int x, int y, int c)
{
    const uint8_t* rgb = cgaPalette[c & 3];
    img->putPixel(x, y, rgb[0], rgb[1], rgb[2], IM_OPAQUE);
}

// Draw a row of 2 bit pixels (4 to a byte, leftmost in the high bits).
static void cgaRow(Image* img, int x, int y, const uint8_t* src, int bytes)
{
    for (int i = 0; i < bytes; ++i) {
        int b = src[i];
        for (int k = 0; k < 4; ++k)
            cgaPut(img, x + i * 4 + k, y, b >> (6 - 2 * k));
    }
}

static Image* cgaTiles(const std::vector<uint8_t>& data)
{
    if (data.size() < 256 * 64)
        return NULL;
    Image* img = Image::create(16, 16 * 256);
    for (int t = 0; t < 256; ++t) {
        const uint8_t* tile = &data[t * 64];
        for (int y = 0; y < 16; ++y)
            cgaRow(img, 0, t * 16 + y, tile + ((y >> 1) + (y & 1) * 8) * 4, 4);
    }
    return img;
}

static Image* cgaFont(const std::vector<uint8_t>& data)
{
    if (data.size() < 128 * 16)
        return NULL;
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
    for (int g = 0; g < 128; ++g) {
        const uint8_t* glyph = &data[g * 16];
        for (int y = 0; y < 8; ++y)
            cgaRow(img, 0, g * 8 + y, glyph + ((y >> 1) + (y & 1) * 4) * 2, 2);
    }
    return img;
}

/*
 * Unpack the run-length blocks of a PC Paint (Pictor) file.  Return false
 * if it is not one.
 */
static bool cgaPictor(const std::vector<uint8_t>& data, std::vector<uint8_t>& out)
{
    size_t n = data.size();
    if (n < 19 || data[0] != 0x34 || data[1] != 0x12)
        return false;
    size_t p = 17 + (data[15] | (data[16] << 8));     // Skip the palette.
    if (p + 2 > n)
        return false;
    int blocks = data[p] | (data[p+1] << 8);
    p += 2;
    for (int b = 0; b < blocks && p + 5 <= n; ++b) {
        size_t end = p + (data[p] | (data[p+1] << 8));
        uint8_t marker = data[p+4];
        size_t q = p + 5;
        if (end > n)
            end = n;
        while (q < end) {
            uint8_t c = data[q++];
            if (c != marker) {
                out.push_back(c);
                continue;
            }
            if (q + 2 > end)
                break;
            unsigned count = data[q++];
            if (count == 0) {
                if (q + 3 > end)
                    break;
                count = data[q] | (data[q+1] << 8);
                q += 2;
            }
            out.insert(out.end(), count, data[q++]);
        }
        p = end;
    }
    return true;
}

static Image* cgaPicture(const std::vector<uint8_t>& data)
{
    std::vector<uint8_t> mem;
    bool interlaced = false;
    if (! cgaPictor(data, mem)) {
        void* raw = NULL;
        long len = decompress_u4_memory((void*) data.data(), data.size(), &raw);
        if (len < 7 + 8192 + 8000) {
            free(raw);
            return NULL;
        }
        mem.assign((uint8_t*) raw + 7, (uint8_t*) raw + len);
        free(raw);
        interlaced = true;
    }
    mem.resize(16384, 0);

    Image* img = Image::create(320, 200);
    for (int y = 0; y < 200; ++y) {
        size_t off = interlaced ? (y & 1) * 8192 + (y >> 1) * 80 : y * 80;
        cgaRow(img, 0, y, &mem[off], 80);
    }
    return img;
}

static Image* loadImage_cga(U4FILE* file, int ftype)
{
    std::vector<uint8_t> data(file->length());
    data.resize(file->read(data.data(), 1, data.size()));

    switch (ftype) {
        case FTYPE_CGATILES:
            return cgaTiles(data);
        case FTYPE_CGAFONT:
            return cgaFont(data);
        case FTYPE_CGAPIC:
            return cgaPicture(data);
    }
    return NULL;
}
