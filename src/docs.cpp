/*
 * XU4 - Ultima IV documents
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The free GOG release of Ultima IV includes the printed documentation as a
 * single ULTIMA4.TXT file.  Older releases had separate HISTORY.TXT,
 * WISDOM.TXT, HINTS.TXT & KEYBOARD.TXT files.  The combined file is split
 * into those documents so that they can be read separately.
 *
 * Downloads which already include the separate files are also supported.
 * These are found with the game files, or in a docs folder next to them,
 * whatever the case of the file names.
 */

#include <cctype>
#include <cstdio>
#include <cstring>
#include <vector>
#include "docs.h"
#include "filesystem.h"
#include "settings.h"
#include "processDir.h"
#include "u4file.h"
#include "xu4.h"

using std::string;
using std::vector;

static const char* docNames[DOC_COUNT] = {
    "History.txt", "Wisdom.txt", "Hints.txt", "Keyboard.txt"
};

static const char* docTitles[DOC_COUNT] = {
    "History", "Wisdom", "Hints", "Keyboard"
};

const char* docsFileName(GameDocument doc) {
    return docNames[doc];
}

const char* docsTitle(GameDocument doc) {
    return docTitles[doc];
}

// Return line trimmed of surrounding white space and upper-cased.
static string upperTrim(const string& line) {
    size_t start = 0;
    size_t end = line.size();
    while (start < end && isspace((unsigned char) line[start]))
        ++start;
    while (end > start && isspace((unsigned char) line[end - 1]))
        --end;
    string s(line, start, end - start);
    for (size_t i = 0; i < s.size(); ++i)
        s[i] = toupper((unsigned char) s[i]);
    return s;
}

static bool isBlank(const string& line) {
    return upperTrim(line).empty();
}

static bool contains(const string& s, const char* word) {
    return s.find(word) != string::npos;
}

static bool isHistoryStart(const string& u) {
    return u == "CHAPTER I" || contains(u, "HISTORY OF BRITANNIA");
}

static bool isReferenceStart(const string& u) {
    return contains(u, "REFERENCE CARD") || u == "PLAYER REFERENCE" ||
           contains(u, "KEYBOARD REFERENCE");
}

// A short heading in capitals which mentions hints.
static bool isHintsStart(const string& line) {
    string u = upperTrim(line);
    if (u.size() > 40 || ! contains(u, "HINTS"))
        return false;
    for (size_t i = 0; i < line.size(); ++i) {
        if (islower((unsigned char) line[i]))
            return false;
    }
    return true;
}

// Center text in the 60 column layout of the older document files.
static string centerLine(const string& text) {
    int pad = (61 - (int) text.size()) / 2;
    if (pad < 0)
        pad = 0;
    return string(pad, ' ') + text;
}

static string joinLines(const vector<string>& lines, size_t start,
                        size_t end, const char* eol) {
    while (start < end && isBlank(lines[start]))
        ++start;
    while (end > start && isBlank(lines[end - 1]))
        --end;
    string out;
    for (size_t i = start; i < end; ++i) {
        out += lines[i];
        out += eol;
    }
    return out;
}

/*
 * Split the text of ULTIMA4.TXT into the separate documents.
 * The section headings are matched loosely, as they vary between releases.
 * Return false if the sections are not found.
 */
bool docsSplit(const string& text, string docs[DOC_COUNT]) {
    vector<string> lines;
    bool crlf = false;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == string::npos)
            nl = text.size();
        size_t end = nl;
        if (end > pos && text[end - 1] == '\r') {
            --end;
            crlf = true;
        }
        lines.push_back(text.substr(pos, end - pos));
        pos = nl + 1;
    }
    const char* eol = crlf ? "\r\n" : "\n";

    size_t count = lines.size();
    size_t hist, ref, hints;

    for (hist = 1; hist < count; ++hist) {
        if (isHistoryStart(upperTrim(lines[hist])))
            break;
    }
    for (ref = hist + 1; ref < count; ++ref) {
        if (isReferenceStart(upperTrim(lines[ref])))
            break;
    }
    for (hints = ref + 1; hints < count; ++hints) {
        if (isHintsStart(lines[hints]))
            break;
    }
    if (hints >= count)
        return false;

    // Include any title lines just above the History & reference card.
    while (hist > 1) {
        string u = upperTrim(lines[hist - 1]);
        if (u.empty() || contains(u, "HISTORY OF BRITANNIA") ||
            contains(u, "AS TOLD BY KYLE"))
            --hist;
        else
            break;
    }
    while (ref > hist + 1) {
        string u = upperTrim(lines[ref - 1]);
        if (u.empty() || u == "ULTIMA IV" || u == "QUEST OF THE AVATAR" ||
            isReferenceStart(u))
            --ref;
        else
            break;
    }

    docs[DOC_WISDOM] = joinLines(lines, 0, hist, eol);

    // The History has no title in some releases.  It is given the same
    // layout as the HISTORY.TXT of older releases, with the chapter
    // headings centered.
    vector<string> history;
    bool titled = false;
    while (hist < ref && isBlank(lines[hist]))
        ++hist;
    for (size_t i = hist; i < ref && i < hist + 6; ++i) {
        if (contains(upperTrim(lines[i]), "HISTORY OF BRITANNIA"))
            titled = true;
    }
    if (! titled) {
        string spaces(30, ' ');
        history.push_back("                  The History of Britannia");
        history.push_back(spaces);
        history.push_back("                 as told by Kyle the Younger");
        history.push_back(spaces);
    }
    size_t titleLines = history.size();
    for (size_t i = hist; i < ref; ++i) {
        string u = upperTrim(lines[i]);
        if (u.compare(0, 8, "CHAPTER ") != 0 ||
            u.find_first_not_of("IVXL", 8) != string::npos) {
            history.push_back(lines[i]);
            continue;
        }
        // Center the chapter number and the title which follows it, with
        // one blank line above (except after the title) and below.
        while (history.size() > titleLines && isBlank(history.back()))
            history.pop_back();
        if (history.size() > titleLines)
            history.push_back(string());
        history.push_back(centerLine(u));
        while (i + 1 < ref && isBlank(lines[i + 1]))
            ++i;
        if (i + 1 < ref)
            history.push_back(centerLine(upperTrim(lines[++i])));
        history.push_back(string());
        while (i + 1 < ref && isBlank(lines[i + 1]))
            ++i;
    }
    docs[DOC_HISTORY] = joinLines(history, 0, history.size(), eol);

    docs[DOC_KEYBOARD] = joinLines(lines, ref, hints, eol);

    string title("                       HINTS FOR ULTIMA IV");
    title += eol;
    title += eol;
    docs[DOC_HINTS] = title + joinLines(lines, hints + 1, count, eol);
    return true;
}

static bool readFile(const string& path, string& text) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (! fp)
        return false;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
        text.append(buf, n);
    fclose(fp);
    return true;
}

struct DocScan {
    string dir;
    vector<string> subdirs;
    string found[DOC_COUNT];
};

static bool sameName(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b) {
        if (tolower((unsigned char) *a) != tolower((unsigned char) *b))
            return false;
    }
    return *a == *b;
}

static int scanDocs(const char* name, int type, void* user) {
    DocScan* scan = (DocScan*) user;
    if (type == PDIR_DIR) {
        if (sameName(name, "docs") || sameName(name, "doc") ||
            sameName(name, "manual") || sameName(name, "manuals"))
            scan->subdirs.push_back(scan->dir + "/" + name);
        return PDIR_CONTINUE;
    }
    if (type != PDIR_FILE && type != PDIR_LINK)
        return PDIR_CONTINUE;
    for (int i = 0; i < DOC_COUNT; ++i) {
        if (scan->found[i].empty() && sameName(name, docNames[i]))
            scan->found[i] = scan->dir + "/" + name;
    }
    return PDIR_CONTINUE;
}

/*
 * Find the separate document files included with some releases.
 * The game directories are searched first, then any docs folder in them.
 */
static void gameDocs(string found[DOC_COUNT]) {
    DocScan scan;
    static const char* subdirs[] = { "", "/u4", "/ultima4", NULL };
    const StringTable* st = &xu4.resourcePaths;
    const char* root = sst_strings(st);
    for (uint32_t i = 0; i < st->used; ++i) {
        for (const char** sub = subdirs; *sub; ++sub) {
            scan.dir = string(root + sst_start(st, i)) + *sub;
            processDir(scan.dir.c_str(), scanDocs, &scan);
        }
    }
    for (size_t i = 0; i < scan.subdirs.size(); ++i) {
        scan.dir = scan.subdirs[i];
        processDir(scan.dir.c_str(), scanDocs, &scan);
    }
    for (int i = 0; i < DOC_COUNT; ++i) {
        found[i] = scan.found[i];
        if (xu4.verbose && ! found[i].empty())
            printf("Document: %s\n", found[i].c_str());
    }
}

/*
 * Read a document from the docs directory of the user path, or from the
 * separate files of releases which include them with the game files.
 * Return false if the document is not found.
 */
bool docsLoad(GameDocument doc, string& text) {
    text.clear();
    if (readFile(xu4.settings->getUserPath() + "docs/" + docNames[doc], text))
        return true;

    string found[DOC_COUNT];
    gameDocs(found);
    return ! found[doc].empty() && readFile(found[doc], text);
}

/*
 * If ULTIMA4.TXT is found with the game files, split it into documents in
 * the docs directory of the user path.  Existing documents, and those
 * included as separate files with the game, are kept.
 */
void docsExtract(const string& userPath) {
    string dir = userPath + "docs/";
    string shipped[DOC_COUNT];
    gameDocs(shipped);
    int i;

    for (i = 0; i < DOC_COUNT; ++i) {
        if (shipped[i].empty() && ! Path::exists(dir + docNames[i]))
            break;
    }
    if (i == DOC_COUNT)
        return;

    static const char* srcNames[] = {
        "ULTIMA4.TXT", "ultima4.txt", "Ultima4.txt", "Ultima4.TXT", NULL
    };
    string src;
    for (const char** it = srcNames; *it && src.empty(); ++it)
        src = u4find_path(*it, &u4Path.u4ForDOSPaths);
    if (src.empty())
        return;

    string text;
    string docs[DOC_COUNT];
    if (! readFile(src, text) || ! docsSplit(text, docs)) {
        if (xu4.verbose)
            printf("Cannot split documents from %s\n", src.c_str());
        return;
    }

    for (i = 0; i < DOC_COUNT; ++i) {
        string path = dir + docNames[i];
        if (! shipped[i].empty() || Path::exists(path))
            continue;
        FILE* fp = FileSystem::openFile(path, "wb");
        if (fp) {
            fwrite(docs[i].data(), 1, docs[i].size(), fp);
            fclose(fp);
            if (xu4.verbose)
                printf("Wrote %s\n", path.c_str());
        }
    }
}
