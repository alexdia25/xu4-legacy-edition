/*
 * update.cpp - Update Notice for the desktop releases.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Once a day ask GitHub for the latest full release (pre-releases are left
 * out) and, if it is newer than this one, offer the download page from the
 * title menu.  "Not This One" stops the notice until a later release comes
 * out.  Builds that are not a release (no plain version number) never ask,
 * nor do the Flatpak (which updates with flatpak update), Android (which
 * asks in its own way) and the web version.
 */

#include "update.h"

#if defined(ANDROID) || defined(__ANDROID__) || defined(__EMSCRIPTEN__)

void updateCheckStart() {}
void updateNoticeShow() {}

#else

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <SDL3/SDL.h>
#include "settings.h"
#include "xu4.h"

#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
#endif

#define RELEASES   "https://github.com/alexdia25/xu4-legacy-edition/releases/latest"
#define LATEST_API "https://api.github.com/repos/alexdia25/xu4-legacy-edition/releases/latest"
#define DAY_SECONDS (24 * 60 * 60)

static SDL_AtomicInt updateState;   // 0 idle/none, 1 checking, 2 newer.
static char latestVersion[32];
static std::string stateFile;

static bool plainVersion(const char* v) {
    if (! v || ! isdigit((unsigned char) *v))
        return false;
    for (; *v; ++v) {
        if (! isdigit((unsigned char) *v) && *v != '.')
            return false;
    }
    return true;
}

/*
 * Compare dotted version numbers like strcmp.
 */
static int compareVersions(const char* a, const char* b) {
    while (*a || *b) {
        long na = strtol(a, (char**) &a, 10);
        long nb = strtol(b, (char**) &b, 10);
        if (na != nb)
            return (na < nb) ? -1 : 1;
        if (*a == '.') ++a;
        if (*b == '.') ++b;
        if (! *a && ! *b)
            break;
    }
    return 0;
}

// update.txt holds the time of the last check and the release skipped.
static void readState(long long* checked, std::string& skipped) {
    *checked = 0;
    FILE* fp = fopen(stateFile.c_str(), "r");
    if (! fp)
        return;
    char buf[64];
    if (fgets(buf, sizeof buf, fp))
        *checked = atoll(buf);
    if (fgets(buf, sizeof buf, fp)) {
        buf[strcspn(buf, "\r\n")] = '\0';
        skipped = buf;
    }
    fclose(fp);
}

static void writeState(long long checked, const std::string& skipped) {
    FILE* fp = fopen(stateFile.c_str(), "w");
    if (fp) {
        fprintf(fp, "%lld\n%s\n", checked, skipped.c_str());
        fclose(fp);
    }
}

/*
 * Return the JSON of the latest release, or an empty string.
 */
static std::string fetchLatest() {
    std::string out;
#ifdef _WIN32
    HINTERNET net = InternetOpenA("xu4-legacy-edition",
                                  INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (! net)
        return out;
    DWORD timeout = 10000;
    InternetSetOptionA(net, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout,
                       sizeof timeout);
    InternetSetOptionA(net, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout,
                       sizeof timeout);
    const char* headers = "Accept: application/vnd.github+json\r\n";
    HINTERNET url = InternetOpenUrlA(net, LATEST_API, headers, (DWORD) -1L,
                        INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_COOKIES |
                        INTERNET_FLAG_RELOAD, 0);
    if (url) {
        char buf[4096];
        DWORD n;
        while (InternetReadFile(url, buf, sizeof buf, &n) && n > 0)
            out.append(buf, n);
        InternetCloseHandle(url);
    }
    InternetCloseHandle(net);
#else
    FILE* fp = popen("curl -fsSL --max-time 10 -A xu4-legacy-edition "
                     "-H 'Accept: application/vnd.github+json' "
                     LATEST_API " 2>/dev/null", "r");
    if (! fp)
        return out;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, fp)) > 0)
        out.append(buf, n);
    pclose(fp);
#endif
    return out;
}

/*
 * Return the version of the latest release (its tag without "legacy-"),
 * or an empty string.
 */
static std::string latestRelease() {
    std::string json = fetchLatest();
    size_t pos = json.find("\"tag_name\"");
    if (pos == std::string::npos)
        return std::string();
    pos = json.find('"', json.find(':', pos));
    if (pos == std::string::npos)
        return std::string();
    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos)
        return std::string();
    std::string tag = json.substr(pos + 1, end - pos - 1);
    if (tag.compare(0, 7, "legacy-") == 0)
        tag.erase(0, 7);
    return plainVersion(tag.c_str()) ? tag : std::string();
}

static int checkThread(void*) {
    std::string latest = latestRelease();
    if (latest.empty()) {
        SDL_SetAtomicInt(&updateState, 0);
        return 0;
    }
    long long checked;
    std::string skipped;
    readState(&checked, skipped);
    writeState((long long) time(NULL), skipped);

    if (compareVersions(latest.c_str(), VERSION) > 0 && latest != skipped &&
        latest.size() < sizeof latestVersion) {
        strcpy(latestVersion, latest.c_str());
        SDL_SetAtomicInt(&updateState, 2);
    } else
        SDL_SetAtomicInt(&updateState, 0);
    return 0;
}

/*
 * Start a check for a newer release, if the Update Notice option is on and
 * the last check was a day ago or more.
 */
void updateCheckStart() {
#ifdef VERSION
    if (! xu4.settings->legacy.updateNotice || ! plainVersion(VERSION) ||
        getenv("FLATPAK_ID"))
        return;
    stateFile = xu4.settings->getUserPath() + "update.txt";
    long long checked;
    std::string skipped;
    readState(&checked, skipped);
    long long now = (long long) time(NULL);
    if (now - checked < DAY_SECONDS && now >= checked)
        return;

    SDL_SetAtomicInt(&updateState, 1);
    SDL_Thread* th = SDL_CreateThread(checkThread, "update", NULL);
    if (th)
        SDL_DetachThread(th);
    else
        SDL_SetAtomicInt(&updateState, 0);
#endif
}

/*
 * Offer the download page once the check has found a newer release.
 * Called from the title menu.
 */
void updateNoticeShow() {
#ifdef VERSION
    if (SDL_GetAtomicInt(&updateState) != 2)
        return;
    SDL_SetAtomicInt(&updateState, 0);

    std::string msg = std::string("XU4 Legacy Edition ") + latestVersion +
                      " is out (you have " VERSION ").";
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Download" },
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Later" },
        { 0, 2, "Not This One" }
    };
    SDL_MessageBoxData box = {
        SDL_MESSAGEBOX_INFORMATION, NULL, "Update available",
        msg.c_str(), 3, buttons, NULL
    };
    int choice = 0;
    if (! SDL_ShowMessageBox(&box, &choice))
        return;
    if (choice == 1)
        SDL_OpenURL(RELEASES);
    else if (choice == 2) {
        long long checked;
        std::string skipped;
        readState(&checked, skipped);
        writeState(checked, latestVersion);
    }
#endif
}

#endif
