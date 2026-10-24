/*
 * imageloader_png.cpp
 */

#include <string.h>
#include <vector>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_surface.h>

/**
 * Loads in the PNG with the SDL3 built-in decoder.
 */
Image* loadImage_png(U4FILE *file) {
    Image* image = NULL;
    long start = file->tell();
    long size = file->length() - start;
    if (size < 8)
        return NULL;

    std::vector<uint8_t> data(size);
    if (file->read(data.data(), 1, size) != (size_t) size)
        return NULL;

    SDL_IOStream* io = SDL_IOFromConstMem(data.data(), size);
    if (! io)
        return NULL;
    SDL_Surface* surf = SDL_LoadPNG_IO(io, true);
    if (! surf)
        return NULL;

    // Image pixels are ABGR packed into a uint32_t (RGBA bytes on
    // little-endian machines).
    SDL_Surface* conv = surf;
    if (surf->format != SDL_PIXELFORMAT_ABGR8888) {
        conv = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_ABGR8888);
        SDL_DestroySurface(surf);
        if (! conv)
            return NULL;
    }

    image = Image::create(conv->w, conv->h);
    if (image) {
        const uint8_t* src = (const uint8_t*) conv->pixels;
        uint32_t* dst = image->pixels;
        for (int y = 0; y < conv->h; ++y) {
            memcpy(dst, src, conv->w * 4);
            src += conv->pitch;
            dst += image->w;
        }
    }
    SDL_DestroySurface(conv);
    return image;
}
