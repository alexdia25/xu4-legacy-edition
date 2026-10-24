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
    FTYPE_A2DNGD,       // Dungeon view geometry (not an image).
    FTYPE_A2VIEW,
    FTYPE_A2REVEAL,
    FTYPE_C64TILES,
    FTYPE_C64FONT,
    FTYPE_C64HIRES,
    FTYPE_C64MULTI,
    FTYPE_C64TREE,
    FTYPE_C64CARDS,
    FTYPE_C64RCARDS,
    FTYPE_C64BEADS,
    FTYPE_SMSTILES,
    FTYPE_SMSPIC,
    FTYPE_SMSTREE,
    FTYPE_SMSFONT,
    FTYPE_SMSCARDS,
    FTYPE_ATLAS         // Special internal type for ImageInfo.
};

#define BPP_CLUT8   -8

Image* loadImage(U4FILE *file, int ftype, int width, int height, int bpp);

#endif /* IMAGELOADER_H */
