/*
 * steam.cpp
 *
 * Finds the Steam release of Ultima IV (app 3067510), which installs to
 * <library>/steamapps/common/Ultima IV with the game run by DOSBox.  The
 * folder holding AVATAR.EXE (or ultima4.zip) is searched for a few levels
 * down, so the layout inside the install folder does not matter.
 */

#if !defined(__EMSCRIPTEN__) && !defined(ANDROID)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#include "stringTable.h"

#ifdef _WIN32
#include <windows.h>
#define SEP "\\"
#else
#define SEP "/"
#endif

#define STEAM_U4_DIR    "Ultima IV"
#define SEARCH_DEPTH    3

static bool isDir(const std::string& path) {
    SDL_PathInfo info;
    return SDL_GetPathInfo(path.c_str(), &info) &&
           info.type == SDL_PATHTYPE_DIRECTORY;
}

static bool hasFile(const std::string& dir, const char* name) {
    int count = 0;
    char** found = SDL_GlobDirectory(dir.c_str(), name,
                                     SDL_GLOB_CASEINSENSITIVE, &count);
    SDL_free(found);
    return count > 0;
}

/*
 * Return the folder under dir (or dir itself) with the DOS game files, or
 * an empty string.
 */
static std::string findGameDir(const std::string& dir, int depth) {
    if (hasFile(dir, "AVATAR.EXE") || hasFile(dir, "ultima4.zip"))
        return dir;
    if (depth <= 0)
        return std::string();

    std::string result;
    int count = 0;
    char** names = SDL_GlobDirectory(dir.c_str(), NULL, 0, &count);
    if (names) {
        for (int i = 0; i < count && result.empty(); ++i) {
            if (strchr(names[i], '/') || strchr(names[i], '\\'))
                continue;
            std::string sub = dir + SEP + names[i];
            if (isDir(sub))
                result = findGameDir(sub, depth - 1);
        }
        SDL_free(names);
    }
    return result;
}

/*
 * Add the "path" entries of steamapps/libraryfolders.vdf (the other Steam
 * libraries, e.g. on another drive or an SD card).
 */
static void readLibraryFolders(const std::string& root,
                               std::vector<std::string>& libs) {
    std::string vdf = root + SEP "steamapps" SEP "libraryfolders.vdf";
    FILE* fp = fopen(vdf.c_str(), "r");
    if (! fp)
        return;
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        const char* key = strstr(line, "\"path\"");
        if (! key)
            continue;
        const char* cp = strchr(key + 6, '"');
        if (! cp)
            continue;
        std::string path;
        for (++cp; *cp && *cp != '"'; ++cp) {
            if (*cp == '\\' && cp[1])
                ++cp;               // VDF escapes backslashes.
            path += *cp;
        }
        if (! path.empty())
            libs.push_back(path);
    }
    fclose(fp);
}

static void steamRoots(std::vector<std::string>& roots) {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD len = sizeof(buf);
    if (RegGetValueA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", "SteamPath",
                     RRF_RT_REG_SZ, NULL, buf, &len) == ERROR_SUCCESS) {
        for (char* cp = buf; *cp; ++cp) {
            if (*cp == '/')
                *cp = '\\';
        }
        roots.push_back(buf);
    }
    roots.push_back("C:\\Program Files (x86)\\Steam");
    roots.push_back("C:\\Program Files\\Steam");
#else
    const char* home = getenv("HOME");
    if (! home || ! home[0])
        return;
    std::string h(home);
#ifdef __APPLE__
    roots.push_back(h + "/Library/Application Support/Steam");
#else
    roots.push_back(h + "/.local/share/Steam");
    roots.push_back(h + "/.steam/steam");
    roots.push_back(h + "/.steam/debian-installation");
    roots.push_back(h + "/.var/app/com.valvesoftware.Steam/.local/share/Steam");
#endif
#endif
}

/*
 * Append the folder of the Steam release's game files to the resource
 * paths, if it is installed.
 */
void steamFindUltima4(StringTable* st) {
    std::vector<std::string> roots, libs;
    steamRoots(roots);
    for (const std::string& root : roots) {
        if (! isDir(root))
            continue;
        libs.push_back(root);
        readLibraryFolders(root, libs);
    }

    for (const std::string& lib : libs) {
        std::string dir = lib + SEP "steamapps" SEP "common" SEP STEAM_U4_DIR;
        if (! isDir(dir))
            continue;
        std::string game = findGameDir(dir, SEARCH_DEPTH);
        if (! game.empty()) {
            sst_append(st, game.c_str(), -1);
            return;
        }
    }
}

#endif
