/*
 * Android support: storage folders, the modules packed in the APK, and
 * importing the player's game files.
 *
 * The game data, extra disk images & ROMs, settings and saved games live in
 * the app's external files folder (Android/data/<app id>/files), which can
 * be reached over USB from a computer.  render.pak & the modules come in the
 * APK's assets and are copied to the internal files folder when the app is
 * new or updated.
 */

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <SDL3/SDL.h>
#include "error.h"
#include "u4file.h"

extern "C" const char* androidInternalData();

static const char* moduleFiles[] = {
    "render.pak", "Ultima-IV.mod", "U4-Upgrade.mod", "Apple-II.mod",
    "Apple-II-Music.mod", "C64.mod", "C64-Music.mod", "C64-Remastered.mod",
    "SMS.mod", "SMS-Music.mod", NULL
};

/*
 * Return the folder for the game data, settings & saves.
 */
extern "C" const char* androidInternalData() {
    const char* path = SDL_GetAndroidExternalStoragePath();
    if (! path)
        path = SDL_GetAndroidInternalStoragePath();
    return path ? path : ".";
}

/*
 * Return the folder holding the modules copied from the APK.
 */
const char* androidModuleDir() {
    const char* path = SDL_GetAndroidInternalStoragePath();
    return path ? path : ".";
}

static bool copyFile(const char* src, const std::string& dest) {
    size_t size;
    void* data = SDL_LoadFile(src, &size);
    if (! data)
        return false;
    std::string tmp = dest + ".tmp";
    bool ok = SDL_SaveFile(tmp.c_str(), data, size) &&
              SDL_RenamePath(tmp.c_str(), dest.c_str());
    SDL_free(data);
    return ok;
}

/*
 * Copy the modules out of the APK when this version has not done so yet.
 */
void androidCopyModules() {
    std::string dir(androidModuleDir());
    std::string stamp = dir + "/modules.version";

    size_t len;
    char* have = (char*) SDL_LoadFile(stamp.c_str(), &len);
    bool current = have && len == strlen(VERSION) &&
                   memcmp(have, VERSION, len) == 0;
    SDL_free(have);
    if (current)
        return;

    for (const char** it = moduleFiles; *it; ++it) {
        // A missing asset is fine; the module list only shows what exists.
        copyFile(*it, dir + "/" + *it);
    }
    SDL_SaveFile(stamp.c_str(), VERSION, strlen(VERSION));
}

//----------------------------------------------------------------------------
// Game file import

struct DialogResult {
    bool done;
    std::string error;
    std::vector<std::string> files;
};

static void SDLCALL dialogDone(void* user, const char* const* files, int) {
    DialogResult* res = (DialogResult*) user;
    if (files) {
        for (; *files; ++files)
            res->files.push_back(*files);
    } else
        res->error = SDL_GetError();
    res->done = true;
}

static bool startsWith(const uint8_t* buf, size_t len, const char* sig) {
    size_t n = strlen(sig);
    return len >= n && memcmp(buf, sig, n) == 0;
}

static bool zipHas(const uint8_t* buf, size_t len, const char* name) {
    size_t n = strlen(name);
    for (size_t i = 0; i + n <= len; ++i) {
        if (SDL_strncasecmp((const char*) buf + i, name, n) == 0)
            return true;
    }
    return false;
}

/*
 * Choose a file name for an imported file from its contents, since the
 * Android file picker does not give the original name.
 */
static std::string importName(const uint8_t* buf, size_t len, int n) {
    char name[32];
    if (startsWith(buf, len, "PK\3\4")) {
        if (zipHas(buf, len, "AVATAR.EXE"))
            return "ultima4.zip";
        if (zipHas(buf, len, "U4VGA.PAL"))
            return "u4upgrad.zip";
        snprintf(name, sizeof(name), "import-%d.zip", n);
    } else if (len == 143360)
        snprintf(name, sizeof(name), "apple2-%d.dsk", n);   // DOS 3.3 order
    else if (len == 174848 || len == 175531)
        snprintf(name, sizeof(name), "c64-%d.d64", n);
    else if (len == 819200)
        snprintf(name, sizeof(name), "c64-%d.d81", n);
    else if (len == 256 * 1024 || len == 512 * 1024)
        snprintf(name, sizeof(name), "ultima4-%d.sms", n);
    else
        snprintf(name, sizeof(name), "import-%d.bin", n);
    return name;
}

static int importFiles(const std::vector<std::string>& files) {
    std::string dir(androidInternalData());
    int count = 0;
    for (size_t i = 0; i < files.size(); ++i) {
        size_t len;
        uint8_t* data = (uint8_t*) SDL_LoadFile(files[i].c_str(), &len);
        if (! data)
            continue;
        std::string dest = dir + "/" + importName(data, len, (int) i + 1);
        if (SDL_SaveFile(dest.c_str(), data, len))
            ++count;
        SDL_free(data);
    }
    return count;
}

/*
 * Ask for the Ultima IV for DOS files (ultima4.zip) until they are found,
 * copying what the player picks into the data folder.  Other files picked
 * at the same time (u4upgrad.zip, Apple II & C64 disk images, the SMS ROM)
 * are copied too.
 */
void androidImportGameData() {
    if (! SDL_Init(SDL_INIT_VIDEO))
        errorFatal("Unable to initialize SDL (%s)", SDL_GetError());

    std::string msg =
        "XU4 needs the Ultima IV for DOS game files.\n\n"
        "Choose ultima4.zip (from the free GOG release or the XU4 site). "
        "You can also pick u4upgrad.zip, Apple II or C64 disk images and "
        "the Sega Master System ROM at the same time.\n\n"
        "Files can also be copied over USB to:\n";
    msg += androidInternalData();

    for (;;) {
        static const SDL_MessageBoxButtonData buttons[] = {
            { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Choose Files" },
            { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit" }
        };
        SDL_MessageBoxData box = {
            SDL_MESSAGEBOX_INFORMATION, NULL, "XU4 Legacy Edition",
            msg.c_str(), 2, buttons, NULL
        };
        int choice = 0;
        if (! SDL_ShowMessageBox(&box, &choice) || choice != 1)
            exit(0);

        DialogResult res;
        res.done = false;
        SDL_ShowOpenFileDialog(dialogDone, &res, NULL, NULL, 0, NULL, true);
        while (! res.done) {
            SDL_PumpEvents();
            SDL_Delay(50);
        }

        if (! res.files.empty() && importFiles(res.files) && u4fsetup())
            return;
        msg = "ultima4.zip was not found in the chosen files.\n\n"
              "Choose ultima4.zip, or copy the game files over USB to:\n";
        msg += androidInternalData();
    }
}
