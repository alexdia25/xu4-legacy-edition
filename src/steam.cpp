/*
 * steam.cpp
 *
 * Finds the Steam release of Ultima IV (app 3067510), which installs to
 * <library>/steamapps/common/Ultima IV with the game run by DOSBox.  The
 * DOS game files (the same as the GOG release's) and ULTIMA4.TXT are at the
 * top of that folder, with DOSBOX/ & Extras/ (PDFs) beside them.  The
 * folder holding AVATAR.EXE (or ultima4.zip) is searched for a few levels
 * down in case a later build moves them.
 */

#if !defined(__EMSCRIPTEN__) && !defined(ANDROID)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#include <sys/stat.h>
#include <ctime>
#include "savegame.h"
#include "steam.h"
#include "stringTable.h"
#include "u4file.h"

#ifdef _WIN32
#include <windows.h>
#define SEP "\\"
#else
#define SEP "/"
#endif

#define STEAM_U4_DIR    "Ultima IV"
#define SEARCH_DEPTH    3

// The folder of the Steam release's game files, if it is installed.
static std::string steamGameDir;

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
            steamGameDir = game;
            return;
        }
    }
}

/*
 * Return true if the game files in use are those of the Steam release
 * (another copy, e.g. the GOG one, may be found first).
 */
bool steamGameFilesUsed() {
    if (steamGameDir.empty())
        return false;
    std::string path = u4find_path("AVATAR.EXE", &u4Path.u4ForDOSPaths);
    if (path.empty())
        path = u4find_path("avatar.exe", &u4Path.u4ForDOSPaths);
    if (path.empty())
        return false;
    // Started by Steam, the game files are found in the current directory
    // (the install folder), so compare the folders themselves.
    std::error_code ec;
    return std::filesystem::equivalent(
                std::filesystem::u8path(path).parent_path(),
                std::filesystem::u8path(steamGameDir), ec);
}

// The save files: ours (lower case) & the DOS game's (upper case).
struct SaveName {
    const char* ours;
    const char* dos;
};
static const SaveName saveNames[] = {
    { PARTY_SAV,    "PARTY.SAV" },
    { MONSTERS_SAV, "MONSTERS.SAV" },
    { OUTMONST_SAV, "OUTMONST.SAV" },
    { DNGMAP_SAV,   "DNGMAP.SAV" }
};

static bool readAll(const std::string& path, std::string& data) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (! fp)
        return false;
    data.clear();
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
        data.append(buf, n);
    bool ok = ! ferror(fp);
    fclose(fp);
    return ok;
}

/*
 * Write data to path.tmp, then put it in place, so a failed write leaves the
 * file as it was.
 */
static bool writeAll(const std::string& path, const std::string& data) {
    std::string tmp = path + ".tmp";
    FILE* fp = fopen(tmp.c_str(), "wb");
    if (! fp)
        return false;
    bool ok = fwrite(data.data(), 1, data.size(), fp) == data.size();
    if (fflush(fp) != 0 || ferror(fp))
        ok = false;
    if (fclose(fp) != 0)
        ok = false;
    if (ok) {
        remove(path.c_str());
        ok = (rename(tmp.c_str(), path.c_str()) == 0);
    }
    if (! ok)
        remove(tmp.c_str());
    return ok;
}

static std::string steamFile(const char* name) {
    return steamGameDir + SEP + name;
}

/*
 * Copy a save just written to userPath into the Steam release's folder.
 * Outside dungeons the DOS game does not read DNGMAP.SAV & OUTMONST.SAV,
 * so those are only copied for a save made in a dungeon.
 */
bool steamSavesExport(const std::string& userPath, bool dungeon) {
    if (steamGameDir.empty())
        return false;
    int count = dungeon ? 4 : 2;
    std::string data[4];
    for (int i = 0; i < count; ++i) {
        if (! readAll(userPath + saveNames[i].ours, data[i]))
            return false;
    }
    // PARTY.SAV last, so the others are in place when it says where.
    for (int i = count - 1; i >= 0; --i) {
        if (! writeAll(steamFile(saveNames[i].dos), data[i]))
            return false;
    }
    return true;
}

static bool modTime(const std::string& path, time_t* t) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return false;
    *t = st.st_mtime;
    return true;
}

/*
 * Return true if the Steam release's folder has a save newer than the one
 * in userPath (and not the same, nor the new game the release ships with).
 */
bool steamSavesNewer(const std::string& userPath, SteamSaveInfo* info) {
    if (steamGameDir.empty())
        return false;
    std::string steamParty = steamFile("PARTY.SAV");
    std::string theirs, ours, blank;
    time_t steamTime, ourTime;
    if (! modTime(steamParty, &steamTime) || ! readAll(steamParty, theirs))
        return false;
    if (readAll(steamFile("PARTY.NEW"), blank) && blank == theirs)
        return false;
    std::string ourParty = userPath + PARTY_SAV;
    if (modTime(ourParty, &ourTime) && readAll(ourParty, ours)) {
        if (ours == theirs || steamTime <= ourTime)
            return false;
    }

    FILE* fp = fopen(steamParty.c_str(), "rb");
    if (! fp)
        return false;
    SaveGame sg;
    bool ok = sg.read(fp) && sg.members > 0;
    fclose(fp);
    if (! ok)
        return false;
    if (info) {
        info->name = sg.players[0].name;
        info->level = sg.players[0].hpMax / 100;
        char buf[32];
        struct tm* tm = localtime(&steamTime);
        if (tm && strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm))
            info->when = buf;
        else
            info->when.clear();
    }
    return true;
}

/*
 * Copy the Steam release's save into userPath.  The save there before is
 * kept in userPath/before-steam/.
 */
bool steamSavesImport(const std::string& userPath) {
    std::string data[4];
    bool found[4];
    for (int i = 0; i < 4; ++i)
        found[i] = readAll(steamFile(saveNames[i].dos), data[i]);
    if (! found[0])
        return false;

    std::string keep = userPath + "before-steam" SEP;
    SDL_CreateDirectory(keep.c_str());
    for (int i = 0; i < 4; ++i) {
        std::string old;
        if (readAll(userPath + saveNames[i].ours, old))
            writeAll(keep + saveNames[i].ours, old);
    }

    // PARTY.SAV last, so a failure leaves our own save in place.
    for (int i = 3; i >= 0; --i) {
        if (found[i] && ! writeAll(userPath + saveNames[i].ours, data[i]))
            return false;
    }
    return true;
}

#else

#include "steam.h"

bool steamGameFilesUsed() { return false; }
bool steamSavesExport(const std::string&, bool) { return false; }
bool steamSavesNewer(const std::string&, SteamSaveInfo*) { return false; }
bool steamSavesImport(const std::string&) { return false; }

#endif
