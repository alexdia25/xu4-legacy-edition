/*
 * imageloader.h
 */

#ifndef IMAGELOADER_H
#define IMAGELOADER_H

#include "image.h"
#include "u4file.h"

enum ImageFiletype {
    FTYPE_UNKNOWN,
    FTYPE_PNG,
    FTYPE_U4RAW,
    FTYPE_U4RLE,
    FTYPE_U4LZW,
    FTYPE_U5LZW,
    FTYPE_FMTOWNS,
    FTYPE_FMTOWNS_PIC,
    FTYPE_FMTOWNS_TIF,
    FTYPE_A2SHAPES,
    FTYPE_A2FONT,
    FTYPE_A2HGR,
    FTYPE_A2SPK,
    FTYPE_A2CARDS,
    FTYPE_A2BEADS,
    FTYPE_ATLAS         // Special internal type for ImageInfo.
};

#define BPP_CLUT8   -8

Image* loadImage(U4FILE *file, int ftype, int width, int height, int bpp);

#endif /* IMAGELOADER_H */
