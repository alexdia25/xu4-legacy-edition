/*
 * zipimport.cpp
 *
 * Takes the extra game data (Apple II & C64 disk images, the Master System
 * ROM and the VGA upgrade's u4upgrad.zip) out of zip files of any name and
 * layout found with the game files, e.g. a "u4multiplat.zip" holding them
 * all.  The files are copied once into an "imported" folder of the user's
 * data folder, which is added to the resource paths, so the readers of
 * each format find them like loose files.
 */

#if !defined(__EMSCRIPTEN__) && !defined(ANDROID)

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#include "stringTable.h"
#include "unzip.h"

extern bool zipImportVerbose;
bool zipImportVerbose = false;

#define IMPORT_DIR  "imported"

static bool wanted(const char* base) {
    static const char* exts[] = { ".dsk", ".d64", ".d81", ".sms", ".nes", NULL };
    size_t len = strlen(base);
    if (SDL_strcasecmp(base, "u4upgrad.zip") == 0)
        return true;
    for (const char** it = exts; *it; ++it) {
        size_t elen = strlen(*it);
        if (len > elen && SDL_strcasecmp(base + len - elen, *it) == 0)
            return true;
    }
    return false;
}

static const char* baseName(const char* path) {
    const char* slash = strrchr(path, '/');
    const char* bs = strrchr(path, '\\');
    if (bs && (! slash || bs > slash))
        slash = bs;
    return slash ? slash + 1 : path;
}

static long fileSize(const std::string& path) {
    SDL_PathInfo info;
    if (SDL_GetPathInfo(path.c_str(), &info) &&
        info.type == SDL_PATHTYPE_FILE)
        return (long) info.size;
    return -1;
}

/*
 * Copy the wanted files of one zip into outDir.  Return the number copied.
 */
static int importZip(const std::string& zipPath, const std::string& outDir) {
    unzFile f = unzOpen(zipPath.c_str());
    if (! f)
        return 0;

    int copied = 0;
    char name[512];
    unz_file_info info;
    for (int r = unzGoToFirstFile(f); r == UNZ_OK; r = unzGoToNextFile(f)) {
        if (unzGetCurrentFileInfo(f, &info, name, sizeof(name),
                                  NULL, 0, NULL, 0) != UNZ_OK)
            continue;
        const char* base = baseName(name);
        if (! base[0] || ! wanted(base))
            continue;
        if (strncmp(base, "._", 2) == 0)      // macOS resource forks
            continue;

        std::string out = outDir + "/" + base;
        if (fileSize(out) == (long) info.uncompressed_size)
            continue;                       // Already imported.

        if (unzOpenCurrentFile(f) != UNZ_OK)
            continue;
        SDL_CreateDirectory(outDir.c_str());
        std::string tmp = out + ".part";
        FILE* fp = fopen(tmp.c_str(), "wb");
        bool ok = fp != NULL;
        char buf[16384];
        int n;
        while (ok && (n = unzReadCurrentFile(f, buf, sizeof(buf))) > 0)
            ok = fwrite(buf, 1, n, fp) == (size_t) n;
        if (n < 0)
            ok = false;
        if (fp)
            fclose(fp);
        unzCloseCurrentFile(f);
        if (ok) {
            remove(out.c_str());
            ok = rename(tmp.c_str(), out.c_str()) == 0;
        }
        if (! ok)
            remove(tmp.c_str());
        else {
            ++copied;
            if (zipImportVerbose)
                printf("Imported %s from %s\n", base, zipPath.c_str());
        }
    }
    unzClose(f);
    return copied;
}

/*
 * Import the extra data from every zip in the resource paths (and their
 * u4 & ultima4 folders) into <dataDir>/imported, then add that folder to
 * the resource paths.
 */
void zipImportExtras(StringTable* st, const char* dataDir) {
    if (! dataDir || ! dataDir[0])
        return;
    std::string outDir = std::string(dataDir) + "/" IMPORT_DIR;

    std::vector<std::string> zips;
    static const char* subdirs[] = { "", "/u4", "/ultima4", NULL };
    const char* root = sst_strings(st);
    for (uint32_t i = 0; i < st->used; ++i) {
        for (const char** sub = subdirs; *sub; ++sub) {
            std::string dir = std::string(root + sst_start(st, i)) + *sub;
            int count = 0;
            char** names = SDL_GlobDirectory(dir.c_str(), "*.zip",
                                             SDL_GLOB_CASEINSENSITIVE, &count);
            for (int n = 0; n < count; ++n) {
                // u4upgrad.zip is read as it is.
                if (SDL_strcasecmp(baseName(names[n]), "u4upgrad.zip") != 0)
                    zips.push_back(dir + "/" + names[n]);
            }
            SDL_free(names);
        }
    }

    for (const std::string& zip : zips)
        importZip(zip, outDir);

    SDL_PathInfo info;
    if (SDL_GetPathInfo(outDir.c_str(), &info) &&
        info.type == SDL_PATHTYPE_DIRECTORY)
        sst_append(st, outDir.c_str(), -1);
}

#endif
