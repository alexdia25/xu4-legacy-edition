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

#include <ctype.h>
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

#include <jni.h>
#include "unzip.h"

static bool dataAdded = false;

/*
 * Return true (once) if files were added since the data was last read.
 */
bool androidTakeDataAdded() {
    bool added = dataAdded;
    dataAdded = false;
    return added;
}

// Call a static String/void/boolean method of XU4Activity.
static jclass activityClass(JNIEnv* env) {
    jobject act = (jobject) SDL_GetAndroidActivity();
    jclass cls = env->GetObjectClass(act);
    env->DeleteLocalRef(act);
    return cls;
}

static std::string displayName(const std::string& uri) {
    JNIEnv* env = (JNIEnv*) SDL_GetAndroidJNIEnv();
    jclass cls = activityClass(env);
    jmethodID mid = env->GetStaticMethodID(cls, "displayName",
                                   "(Ljava/lang/String;)Ljava/lang/String;");
    jstring juri = env->NewStringUTF(uri.c_str());
    jstring jname = (jstring) env->CallStaticObjectMethod(cls, mid, juri);
    std::string name;
    if (jname) {
        const char* cs = env->GetStringUTFChars(jname, NULL);
        name = cs;
        env->ReleaseStringUTFChars(jname, cs);
        env->DeleteLocalRef(jname);
    }
    env->DeleteLocalRef(juri);
    env->DeleteLocalRef(cls);
    return name;
}

struct PickedFile {
    std::string name;
    std::string uri;
};

// Show the folder picker and wait for the player's choice.
static bool pickFolder(std::vector<PickedFile>& files) {
    JNIEnv* env = (JNIEnv*) SDL_GetAndroidJNIEnv();
    jclass cls = activityClass(env);
    env->CallStaticVoidMethod(cls,
                        env->GetStaticMethodID(cls, "pickFolder", "()V"));
    jmethodID done = env->GetStaticMethodID(cls, "folderPickDone", "()Z");
    while (! env->CallStaticBooleanMethod(cls, done)) {
        SDL_PumpEvents();
        SDL_Delay(50);
    }
    jobjectArray arr = (jobjectArray) env->CallStaticObjectMethod(cls,
                        env->GetStaticMethodID(cls, "folderPickFiles",
                                               "()[Ljava/lang/String;"));
    if (arr) {
        jsize n = env->GetArrayLength(arr);
        for (jsize i = 0; i + 1 < n; i += 2) {
            PickedFile pf;
            for (int k = 0; k < 2; ++k) {
                jstring js = (jstring) env->GetObjectArrayElement(arr, i + k);
                const char* cs = env->GetStringUTFChars(js, NULL);
                (k ? pf.uri : pf.name) = cs;
                env->ReleaseStringUTFChars(js, cs);
                env->DeleteLocalRef(js);
            }
            files.push_back(pf);
        }
        env->DeleteLocalRef(arr);
    }
    env->DeleteLocalRef(cls);
    return arr != NULL;
}

struct DialogResult {
    bool done;
    std::vector<std::string> files;
};

static void SDLCALL dialogDone(void* user, const char* const* files, int) {
    DialogResult* res = (DialogResult*) user;
    if (files) {
        for (; *files; ++files)
            res->files.push_back(*files);
    }
    res->done = true;
}

// Show the file picker and wait for the player's choice.
static void pickFiles(std::vector<PickedFile>& files) {
    DialogResult res;
    res.done = false;
    SDL_ShowOpenFileDialog(dialogDone, &res, NULL, NULL, 0, NULL, true);
    while (! res.done) {
        SDL_PumpEvents();
        SDL_Delay(50);
    }
    for (const std::string& uri : res.files) {
        PickedFile pf;
        pf.uri = uri;
        pf.name = displayName(uri);
        if (pf.name.empty()) {
            size_t slash = uri.rfind('/');
            pf.name = uri.substr(slash == std::string::npos ? 0 : slash + 1);
        }
        files.push_back(pf);
    }
}

static std::string lower(const std::string& str) {
    std::string out(str);
    for (char& c : out)
        c = tolower((unsigned char) c);
    return out;
}

static std::string upper(const std::string& str) {
    std::string out(str);
    for (char& c : out)
        c = toupper((unsigned char) c);
    return out;
}

static std::string extension(const std::string& name) {
    size_t dot = name.rfind('.');
    return (dot == std::string::npos) ? "" : lower(name.substr(dot + 1));
}

// A file of Ultima IV for DOS.  These go in the data folder's u4 folder.
static bool isDosFile(const std::string& name) {
    static const char* exts[] = {
        "cga", "com", "con", "dng", "drv", "ega", "egz", "map", "new", "old",
        "pal", "pic", "tlk", "ult", NULL
    };
    std::string n = lower(name);
    if (n == "avatar.exe" || n == "title.exe")
        return true;
    std::string ext = extension(n);
    for (const char** it = exts; *it; ++it)
        if (ext == *it)
            return true;
    return false;
}

// The printed documents (ULTIMA4.TXT from GOG, or separate files), which go
// with the DOS files.
static bool isDocFile(const std::string& name) {
    std::string n = lower(name);
    return n == "ultima4.txt" || n == "history.txt" || n == "wisdom.txt" ||
           n == "hints.txt" || n == "keyboard.txt";
}

// A disk image or ROM of another version, which the modules look for.
static bool isImageFile(const std::string& name) {
    std::string ext = extension(name);
    return ext == "dsk" || ext == "do" || ext == "d64" || ext == "d81" ||
           ext == "sms";
}

static std::string baseName(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

struct ImportResult {
    int dos;
    bool avatar;
    bool upgrade;
    bool docs;
    bool installer;
    std::vector<std::string> images;
};

static bool saveData(const std::string& path, const void* data, size_t len) {
    std::string dir = path.substr(0, path.rfind('/'));
    SDL_CreateDirectory(dir.c_str());
    return SDL_SaveFile(path.c_str(), data, len);
}

static void addDos(ImportResult& res, const std::string& name,
                   const void* data, size_t len) {
    std::string path = std::string(androidInternalData()) + "/u4/" + upper(name);
    if (saveData(path, data, len)) {
        ++res.dos;
        if (lower(name) == "avatar.exe")
            res.avatar = true;
    }
}

static void addDoc(ImportResult& res, const std::string& name,
                   const void* data, size_t len) {
    std::string path = std::string(androidInternalData()) + "/u4/" + name;
    if (saveData(path, data, len))
        res.docs = true;
}

static void addImage(ImportResult& res, const std::string& name,
                     const void* data, size_t len) {
    std::string path = std::string(androidInternalData()) + "/" + name;
    if (saveData(path, data, len))
        res.images.push_back(name);
}

static bool zipRead(unzFile zf, std::vector<uint8_t>& data, size_t size) {
    data.resize(size);
    if (unzOpenCurrentFile(zf) != UNZ_OK)
        return false;
    int n = unzReadCurrentFile(zf, data.data(), (unsigned) size);
    unzCloseCurrentFile(zf);
    return n == (int) size;
}

/*
 * Take what is wanted from a zip file: the VGA upgrade is kept as it is, the
 * DOS game's files are unpacked (from the folder holding AVATAR.EXE, as in
 * ultima4.zip or a zip of the GOG game's folder), and so are disk images &
 * ROMs.
 */
static void importZip(ImportResult& res, const std::string& path) {
    unzFile zf = unzOpen(path.c_str());
    if (! zf)
        return;

    std::vector<std::string> names;
    std::vector<size_t> sizes;
    std::string avatarDir;
    bool haveAvatar = false, upgrade = false;
    char name[512];
    unz_file_info info;
    for (int r = unzGoToFirstFile(zf); r == UNZ_OK; r = unzGoToNextFile(zf)) {
        if (unzGetCurrentFileInfo(zf, &info, name, sizeof(name), NULL, 0,
                                  NULL, 0) != UNZ_OK)
            continue;
        std::string n(name);
        std::string base = lower(baseName(n));
        if (base == "u4vga.pal")
            upgrade = true;
        if (base == "avatar.exe" && ! haveAvatar) {
            haveAvatar = true;
            avatarDir = n.substr(0, n.size() - base.size());
        }
        names.push_back(n);
        sizes.push_back(info.uncompressed_size);
    }

    if (upgrade) {
        unzClose(zf);
        std::string dest = std::string(androidInternalData()) + "/u4upgrad.zip";
        if (SDL_RenamePath(path.c_str(), dest.c_str()))
            res.upgrade = true;
        return;
    }

    std::vector<uint8_t> data;
    size_t i = 0;
    for (int r = unzGoToFirstFile(zf); r == UNZ_OK;
         r = unzGoToNextFile(zf), ++i) {
        if (i >= names.size())
            break;
        const std::string& n = names[i];
        std::string base = baseName(n);
        if (haveAvatar && n == avatarDir + base && isDosFile(base)) {
            if (zipRead(zf, data, sizes[i]))
                addDos(res, base, data.data(), data.size());
        } else if (isDocFile(base)) {
            if (zipRead(zf, data, sizes[i]))
                addDoc(res, base, data.data(), data.size());
        } else if (isImageFile(base)) {
            if (zipRead(zf, data, sizes[i]))
                addImage(res, base, data.data(), data.size());
        }
    }
    unzClose(zf);
}

static ImportResult importFiles(const std::vector<PickedFile>& files) {
    ImportResult res;
    res.dos = 0;
    res.avatar = res.upgrade = res.docs = res.installer = false;
    std::string zipTemp = std::string(androidInternalData()) + "/import.zip";

    for (const PickedFile& pf : files) {
        std::string ext = extension(pf.name);
        bool dos = isDosFile(pf.name);
        bool image = isImageFile(pf.name);
        bool doc = isDocFile(pf.name);
        if (ext == "exe" && ! dos) {
            if (lower(pf.name).compare(0, 5, "setup") == 0)
                res.installer = true;   // The GOG installer.
            continue;
        }
        if (! dos && ! image && ! doc && ext != "zip")
            continue;

        size_t len;
        uint8_t* data = (uint8_t*) SDL_LoadFile(pf.uri.c_str(), &len);
        if (! data)
            continue;
        if (dos)
            addDos(res, pf.name, data, len);
        else if (image)
            addImage(res, pf.name, data, len);
        else if (doc)
            addDoc(res, pf.name, data, len);
        else if (SDL_SaveFile(zipTemp.c_str(), data, len)) {
            importZip(res, zipTemp);
            SDL_RemovePath(zipTemp.c_str());
        }
        SDL_free(data);
    }
    if (res.dos || res.upgrade || res.docs || ! res.images.empty())
        dataAdded = true;
    return res;
}

static std::string describe(const ImportResult& res) {
    std::vector<std::string> parts;
    if (res.avatar)
        parts.push_back("the Ultima IV for DOS files");
    else if (res.dos)
        parts.push_back(std::to_string(res.dos) + " Ultima IV file(s)");
    if (res.docs)
        parts.push_back("the documents");
    if (res.upgrade)
        parts.push_back("the VGA upgrade");
    for (const std::string& n : res.images)
        parts.push_back(n);

    std::string msg;
    if (parts.empty())
        msg = "Nothing XU4 uses was found in the chosen files.";
    else {
        msg = "Added ";
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i)
                msg += (i + 1 == parts.size()) ? " and " : ", ";
            msg += parts[i];
        }
        msg += '.';
    }
    if (res.installer)
        msg += "\n\nThe GOG installer only runs on Windows: install it on a "
               "PC and copy the folder it made to this device (or a zip of "
               "that folder), then choose that.";
    return msg;
}

static const char* gogHelp =
    "From GOG you get a Windows installer (setup_ultima_iv_...exe). "
    "Install it on a PC and copy the folder it made (C:\\GOG Games\\Ultima "
    "4 - Quest of the Avatar), or a zip of it, to this device. Then Choose "
    "Folder and pick that folder, or Choose Files and pick the zip "
    "(ultima4.zip works too).";

static const char* extrasHelp =
    "u4upgrad.zip (the VGA upgrade), Apple II (.dsk) or C64 (.d64, .d81) "
    "disk images and the Sega Master System ROM (.sms) can be added too, "
    "on their own or in zip files.";

/*
 * Ask for files with a message box.  Returns 1 for Choose Files, 2 for
 * Choose Folder, or 0 for the other button.
 */
static int askForFiles(const std::string& msg, const char* other) {
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Choose Files" },
        { 0, 2, "Choose Folder" },
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, other }
    };
    SDL_MessageBoxData box = {
        SDL_MESSAGEBOX_INFORMATION, NULL, "XU4 Legacy Edition",
        msg.c_str(), 3, buttons, NULL
    };
    int choice = 0;
    if (! SDL_ShowMessageBox(&box, &choice))
        return 0;
    return choice;
}

static bool pick(int choice, std::vector<PickedFile>& files) {
    if (choice == 2)
        return pickFolder(files) && ! files.empty();
    pickFiles(files);
    return ! files.empty();
}

/*
 * Ask for the Ultima IV for DOS files until they are found, copying what the
 * player picks into the data folder.
 */
void androidImportGameData() {
    if (! SDL_Init(SDL_INIT_VIDEO))
        errorFatal("Unable to initialize SDL (%s)", SDL_GetError());

    std::string msg = "XU4 needs the files of Ultima IV for DOS.\n\n";
    msg += gogHelp;
    msg += "\n\n";
    msg += extrasHelp;
    if (SDL_GetAndroidExternalStoragePath()) {
        msg += "\n\nFiles can also be copied over USB to:\n";
        msg += androidInternalData();
    }

    for (;;) {
        int choice = askForFiles(msg, "Quit");
        if (! choice)
            exit(0);

        std::vector<PickedFile> files;
        if (! pick(choice, files))
            continue;
        ImportResult res = importFiles(files);
        dataAdded = false;      // Read when the game starts anyway.
        if (u4fsetup())
            return;
        msg = describe(res) + "\n\nThe Ultima IV for DOS files are still "
              "missing.\n\n" + gogHelp;
    }
}

//----------------------------------------------------------------------------
// The stored game files

struct FileGroup {
    const char* label;
    std::vector<std::string> paths;
};

struct ListContext {
    std::string dir;
    std::vector<std::string>* names;
};

static SDL_EnumerationResult SDLCALL listName(void* user, const char*,
                                              const char* name) {
    ListContext* ctx = (ListContext*) user;
    SDL_PathInfo info;
    std::string path = ctx->dir + "/" + name;
    if (SDL_GetPathInfo(path.c_str(), &info) && info.type == SDL_PATHTYPE_FILE)
        ctx->names->push_back(name);
    return SDL_ENUM_CONTINUE;
}

static std::vector<std::string> listDir(const std::string& dir) {
    std::vector<std::string> names;
    ListContext ctx = { dir, &names };
    SDL_EnumerateDirectory(dir.c_str(), listName, &ctx);
    return names;
}

/*
 * Return the game files in the data folder, in groups that can be removed:
 * the DOS game, the documents, the VGA upgrade, and disk images & ROMs.
 */
static std::vector<FileGroup> storedGroups() {
    std::string data(androidInternalData());
    FileGroup dos  = { "Ultima IV for DOS", {} };
    FileGroup docs = { "Documents", {} };
    FileGroup upg  = { "VGA upgrade", {} };
    FileGroup imgs = { "Disk images & ROMs", {} };

    for (const std::string& n : listDir(data + "/u4"))
        (isDocFile(n) ? docs : dos).paths.push_back(data + "/u4/" + n);
    for (const std::string& n : listDir(data + "/docs"))
        docs.paths.push_back(data + "/docs/" + n);
    for (const std::string& n : listDir(data)) {
        if (lower(n) == "u4upgrad.zip")
            upg.paths.push_back(data + "/" + n);
        else if (extension(n) == "zip")
            dos.paths.push_back(data + "/" + n);    // ultima4.zip
        else if (isImageFile(n))
            imgs.paths.push_back(data + "/" + n);
    }

    std::vector<FileGroup> groups;
    for (FileGroup* g : { &dos, &docs, &upg, &imgs })
        if (! g->paths.empty())
            groups.push_back(*g);
    return groups;
}

static std::string describeStored(const std::vector<FileGroup>& groups) {
    std::string text = "Game files on this device:\n";
    if (groups.empty())
        return text + "  none\n";
    for (const FileGroup& g : groups) {
        text += "\n";
        text += g.label;
        text += ": ";
        if (g.paths.size() > 5)
            text += std::to_string(g.paths.size()) + " files";
        else {
            for (size_t i = 0; i < g.paths.size(); ++i) {
                if (i)
                    text += ", ";
                text += baseName(g.paths[i]);
            }
        }
    }
    return text + "\n";
}

/*
 * Ask which group of files to remove.  Returns true if any were removed.
 */
static bool removeFiles() {
    std::vector<FileGroup> groups = storedGroups();
    if (groups.empty())
        return false;

    std::vector<SDL_MessageBoxButtonData> buttons;
    for (size_t i = 0; i < groups.size(); ++i) {
        SDL_MessageBoxButtonData b = { 0, (int) i + 1, groups[i].label };
        buttons.push_back(b);
    }
    SDL_MessageBoxButtonData cancel = {
        SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"
    };
    buttons.push_back(cancel);

    std::string msg = describeStored(groups) +
                      "\nWhich files should be removed? Saved games are kept.";
    SDL_MessageBoxData box = {
        SDL_MESSAGEBOX_WARNING, NULL, "Remove Game Files", msg.c_str(),
        (int) buttons.size(), buttons.data(), NULL
    };
    int choice = 0;
    if (! SDL_ShowMessageBox(&box, &choice) || choice < 1)
        return false;

    for (const std::string& path : groups[choice - 1].paths)
        SDL_RemovePath(path.c_str());
    dataAdded = true;   // Read the data again.
    return true;
}

/*
 * Show the game files, and add or remove some.  Returns true if any were
 * added or removed; the data is then read again (androidTakeDataAdded).
 */
bool androidAddGameFiles() {
    std::string note;
    for (;;) {
        std::string msg = note + describeStored(storedGroups()) + "\nAdd: " +
                          extrasHelp;
        const SDL_MessageBoxButtonData buttons[] = {
            { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Choose Files" },
            { 0, 2, "Choose Folder" },
            { 0, 3, "Remove..." },
            { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Close" }
        };
        SDL_MessageBoxData box = {
            SDL_MESSAGEBOX_INFORMATION, NULL, "Game Files", msg.c_str(),
            4, buttons, NULL
        };
        int choice = 0;
        if (! SDL_ShowMessageBox(&box, &choice) || choice == 0)
            return dataAdded;

        if (choice == 3) {
            if (removeFiles())
                note = "Removed. They stop being used when you close this.\n\n";
            continue;
        }

        std::vector<PickedFile> files;
        if (! pick(choice, files))
            continue;
        ImportResult res = importFiles(files);
        note = describe(res) + "\n\n";
    }
}
