/*
 * XU4 - Ultima IV documents
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The free GOG release of Ultima IV includes the printed documentation as a
 * single ULTIMA4.TXT file.  Older releases had separate HISTORY.TXT,
 * WISDOM.TXT, HINTS.TXT & KEYBOARD.TXT files.  The combined file is split
 * into those documents so that they can be read separately.
 */

#include <cctype>
#include <cstdio>
#include <vector>
#include "docs.h"
#include "filesystem.h"
#include "u4file.h"
#include "xu4.h"

using std::string;
using std::vector;

static const char* docNames[DOC_COUNT] = {
    "History.txt", "Wisdom.txt", "Hints.txt", "Keyboard.txt"
};

const char* docsFileName(GameDocument doc) {
    return docNames[doc];
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

    // The History has no title in some releases.
    string history = joinLines(lines, hist, ref, eol);
    if (! contains(upperTrim(history.substr(0, 200)), "HISTORY OF BRITANNIA")) {
        string title("                  The History of Britannia");
        title += eol;
        title += eol;
        title += "                 as told by Kyle the Younger";
        title += eol;
        title += eol;
        history = title + history;
    }
    docs[DOC_HISTORY] = history;

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

/*
 * If ULTIMA4.TXT is found with the game files, split it into documents in
 * the docs directory of the user path.  Existing documents are kept.
 */
void docsExtract(const string& userPath) {
    string dir = userPath + "docs/";
    int i;

    for (i = 0; i < DOC_COUNT; ++i) {
        if (! Path::exists(dir + docNames[i]))
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
        if (Path::exists(path))
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
