/*
 * XU4 - Ultima IV document reader
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Shows the History, Wisdom, Hints & Keyboard documents so that they can be
 * read without leaving the game, a table of the runes for reading the runic
 * signs & the world map, the How to Play guide and the quest journal.  Each
 * book is split into its chapters, which are read one at a time.
 *
 * The pages are drawn with the game's own character set inside the blue
 * frame of the title menus.  The documents keep the layout of the original
 * fixed width files, so they are shown at half the size of the game's text
 * (80 columns across).
 */

#include <cctype>
#include <cstring>
#include <algorithm>

#include "docreader.h"
#include "event.h"
#include "gamepad.h"
#include "gpu.h"
#include "gui.h"
#include "legacy.h"
#include "screen.h"
#include "settings.h"
#include "sound.h"
#include "u4.h"
#include "xu4.h"

using std::string;

#define ATTR_COUNT  7
#define QUAD_ATTR   (6 * ATTR_COUNT)
#define RUNE_QUADS  1200
#define FONT_RUNES  3           // Index in ScreenState::fontTable.
#define RUNE_MARK   '\x01'      // Starts a segment drawn in runes.

// Page layout (in page pixels).
#define EDGE        16          // Frame & its grey line.
#define TAB_Y       22
#define BAR_Y       38
#define RULE1_Y     60
#define BODY_X      (EDGE + 8)
#define BODY_Y      68
#define BODY_END    (page.height() - 46)
#define BODY_W      (page.width() - 2 * BODY_X)
#define BODY_H      (BODY_END - BODY_Y)
#define RULE2_Y     (page.height() - 42)
#define FOOT_Y      (page.height() - 37)   // Two lines of help.
#define SIZE_COUNT  3
#define LINE_H      10          // Body text line spacing.

enum HitIds {
    HIT_TAB   = 0,
    HIT_PREV  = 20,
    HIT_NEXT,
    HIT_TITLE,
    HIT_BACK,
    HIT_SMALLER,
    HIT_LARGER,
    HIT_LIST  = 100
};

static const char* bookTabs[DocReader::BOOK_COUNT] = {
    "History", "Wisdom", "Hints", "Keyboard", "Runes", "How to Play",
    "Journal"
};

static const char* shortTabs[DocReader::BOOK_COUNT] = {
    "History", "Wisdom", "Hints", "Keys", "Runes", "Help", "Journal"
};

// Columns of text for each text size.  The text is drawn on a page of its
// own which is stretched to fill the body of the reader, so fewer columns
// make bigger text.
static const uint8_t sizeCols[SIZE_COUNT] = { 74, 59, 50 };

static const char* bookTitle(int n)
{
    switch (n) {
        case DocReader::BOOK_RUNES:   return "The Runes of Britannia";
        case DocReader::BOOK_HELP:    return "How to Play";
        case DocReader::BOOK_JOURNAL: return "Journal";
    }
    return docsTitle(GameDocument(n));
}

/*
 * Return a word spelled in runes, using the combined runes for TH, EE, NG,
 * EA & ST (character codes 127-131 of the runes font).
 */
static string runeSpelling(const char* word)
{
    static const char* pairs[5] = { "TH", "EE", "NG", "EA", "ST" };
    string out(1, RUNE_MARK);
    while (*word) {
        int i;
        for (i = 0; i < 5; ++i) {
            if (word[0] == pairs[i][0] && word[1] == pairs[i][1])
                break;
        }
        if (i < 5) {
            out.push_back(char(127 + i));
            word += 2;
        } else
            out.push_back(*word++);
    }
    return out;
}

// Put text at a column of a line.
static void putColumn(string& line, size_t col, const string& text)
{
    if (line.size() < col)
        line.append(col - line.size(), ' ');
    line += text;
}

/*
 * Make the rune table document.
 */
static string runesText()
{
    static const char* letters[31] = {
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
        "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
        "TH", "EE", "NG", "EA", "ST"
    };
    static const char* words[] = {
        "Honesty", "Compassion", "Valor", "Justice", "Sacrifice", "Honor",
        "Spirituality", "Humility", "", "Truth", "Love", "Courage", "",
        "The Deep Forest", "The Isle of Deeds", "Fens of the Dead", NULL
    };
    string text;

    text += "                      The Runes of Britannia\n\n";
    text += "    The runes are written for the letters of our alphabet, and\n"
            "    five runes stand for a pair of letters: TH, EE, NG, EA & ST.\n";
    text += "\n\n";

    for (int row = 0; row < 8; ++row) {
        string line;
        for (int col = 0; col < 4; ++col) {
            int n = row * 4 + col;
            if (n >= 31)
                break;
            putColumn(line, 6 + col * 14, letters[n]);
            putColumn(line, 11 + col * 14, runeSpelling(letters[n]));
        }
        text += line + "\n\n";
    }

    text += "\n                        Words in Runes\n\n";
    for (const char** it = words; *it; ++it) {
        string line;
        if (**it) {
            string upper(*it);
            for (size_t i = 0; i < upper.size(); ++i)
                upper[i] = toupper((unsigned char) upper[i]);
            putColumn(line, 6, *it);
            putColumn(line, 26, runeSpelling(upper.c_str()));
            line += "\n";
        }
        text += line + "\n";
    }
    return text;
}

/*
 * Split text into lines, removing carriage returns and expanding tabs.
 */
static void splitLines(const string& text, std::vector<string>& lines)
{
    string line;
    lines.clear();
    for (size_t i = 0; i < text.size(); ++i) {
        char ch = text[i];
        if (ch == '\n') {
            lines.push_back(line);
            line.clear();
        } else if (ch == '\t') {
            line.append(8 - (line.size() & 7), ' ');
        } else if (ch != '\r') {
            line.push_back(ch);
        }
    }
    if (! line.empty())
        lines.push_back(line);
    while (! lines.empty() &&
           lines.back().find_first_not_of(' ') == string::npos)
        lines.pop_back();
}

static bool isBlank(const string& line)
{
    return line.find_first_not_of(' ') == string::npos;
}

static string trimmed(const string& line)
{
    size_t a = line.find_first_not_of(' ');
    if (a == string::npos)
        return string();
    size_t b = line.find_last_not_of(' ');
    return line.substr(a, b - a + 1);
}

/*
 * Title case an upper case heading, mending the misspellings of the
 * original documents.
 */
static string titleCase(const string& text)
{
    static const char* mend[][2] = {
        { "POLITCAL", "POLITICAL" }, { "MERCENTILE", "MERCANTILE" },
        { "LELEL", "LEVEL" }, { NULL, NULL }
    };
    static const char* small[] = {
        "a", "an", "and", "at", "for", "in", "of", "on", "or", "our", "the",
        "to", NULL
    };
    string out;
    size_t i = 0;
    bool first = true;
    while (i < text.size()) {
        if (text[i] == ' ') {
            out.push_back(' ');
            ++i;
            continue;
        }
        size_t j = text.find(' ', i);
        if (j == string::npos)
            j = text.size();
        string word = text.substr(i, j - i);
        for (int m = 0; mend[m][0]; ++m)
            if (word == mend[m][0])
                word = mend[m][1];
        for (char& ch : word)
            ch = tolower((unsigned char) ch);
        bool isSmall = false;
        for (const char** it = small; *it; ++it)
            if (word == *it)
                isSmall = true;
        if (first || ! isSmall)
            word[0] = toupper((unsigned char) word[0]);
        out += word;
        first = false;
        i = j;
    }
    return out;
}

// "CHAPTER IV" lines of the History.
static bool chapterLine(const string& t)
{
    if (t.compare(0, 8, "CHAPTER ") != 0 || t.size() > 14)
        return false;
    return t.find_first_not_of("IVXL", 8) == string::npos;
}

// "Garlic - " or "Magic Missile - " entries of the Book of Mystic Wisdom.
static bool entryLine(const string& line, string* name)
{
    if (line.empty() || ! isupper((unsigned char) line[0]))
        return false;
    // "Name - text" (ULTIMA4.TXT), or "Name: text" (Wisdom.txt of other
    // releases), where the name is capitalized words but not all upper
    // case like the History's "MAGE:".
    size_t dash = line.find(" - ");
    size_t colon = line.find(':');
    bool byColon = colon < dash;
    if (byColon)
        dash = colon;
    if (dash == string::npos || dash > 20)
        return false;
    bool lower = false;
    for (size_t i = 0; i < dash; ++i) {
        char ch = line[i];
        if (! isalpha((unsigned char) ch) && ch != ' ')
            return false;
        if (i && line[i-1] == ' ' && ch != ' ' && ! isupper((unsigned char) ch))
            return false;
        if (islower((unsigned char) ch))
            lower = true;
    }
    if (byColon && ! lower)
        return false;

    // The name, with the runs of spaces of justified text made single.
    name->clear();
    for (size_t i = 0; i < dash; ++i) {
        if (line[i] != ' ' || (i + 1 < dash && line[i+1] != ' '))
            *name += line[i];
    }
    while (! name->empty() && name->back() == ' ')
        name->pop_back();
    return true;
}

// Upper case headings standing alone, like "HOW TO RAISE VIRTUES".  Table
// rows (with runs of spaces) are not headings.
static bool headingLine(const string& line)
{
    string t = trimmed(line);
    if (t.size() < 4 || t.size() > 50 || t.find("   ") != string::npos)
        return false;
    if (t.find(' ') == string::npos)
        return false;
    bool letter = false;
    for (char ch : t) {
        if (islower((unsigned char) ch))
            return false;
        if (isalpha((unsigned char) ch))
            letter = true;
        else if (ch != ' ' && ch != '\'' && ch != '&' && ch != '-')
            return false;
    }
    return letter;
}

/*
 * Return true if a line of the original fixed width text carries on the
 * paragraph of the line before it: both are prose (no table columns), the
 * line is not a heading or list item, and the line before reached the
 * right margin.
 */
static bool joinsParagraph(const string& prev, const string& line)
{
    if (isBlank(prev) || isBlank(line) || headingLine(line))
        return false;
    size_t indent = line.find_first_not_of(' ');
    size_t prevIndent = prev.find_first_not_of(' ');
    if (line.find("   ", indent) != string::npos ||
        prev.find("   ", prevIndent) != string::npos)
        return false;
    if (indent > prevIndent && indent > 0)
        return false;
    // A list label such as "A)" or "1)" starts a new item.
    if (line.size() > indent + 1 && line[indent + 1] == ')')
        return false;
    return trimmed(prev).size() + prevIndent >= 50;
}

/*
 * Wrap a line to the page width.  Continued lines are indented to where
 * the text of the line starts (after a "1)" or "A)" label).
 */
static void wrapLine(const string& src, size_t cols, std::vector<string>& out,
                     size_t hang = string::npos)
{
    string line(src);
    while (! line.empty() && line.back() == ' ')
        line.pop_back();
    if (line.size() <= cols || line.find(RUNE_MARK) != string::npos) {
        out.push_back(line);
        return;
    }

    // A centered or indented line which fits without its indent is
    // centered (or moved left) instead of wrapped.
    size_t lead = line.find_first_not_of(' ');
    size_t len = line.size() - lead;
    bool joined = (hang != string::npos);
    if (! joined && len <= cols) {
        size_t indent = (lead >= 12) ? (cols - len) / 2
                                     : std::min(lead, cols - len);
        out.push_back(string(indent, ' ') + line.substr(lead));
        return;
    }

    // Prose rejoined into a paragraph continues at the indent of its
    // second line.
    size_t indent = joined ? hang : lead;
    size_t gap = joined ? string::npos : line.find("  ", indent);
    if (gap != string::npos && gap - indent <= 4) {
        size_t text = line.find_first_not_of(' ', gap);
        if (text != string::npos && text < 20)
            indent = text;
    }
    if (indent > cols / 2)
        indent = 0;

    while (line.size() > cols) {
        size_t cut = line.rfind(' ', cols);
        if (cut == string::npos || cut <= indent)
            cut = cols;
        out.push_back(line.substr(0, cut));
        size_t next = line.find_first_not_of(' ', cut);
        if (next == string::npos) {
            line.clear();
            break;
        }
        line = string(indent, ' ') + line.substr(next);
    }
    if (! line.empty())
        out.push_back(line);
}

//----------------------------------------------------------------------------

void DocReader::renderReader(ScreenState* ss, void* data)
{
    DocReader* dr = (DocReader*) data;
    WorkBuffer* work = dr->work;

    // Name the keys or buttons of whichever was used last.
    if (dr->padHelp != gamepad_prompts())
        dr->redraw();

    const TextPage& page = dr->page;
    dr->body.upload();
    dr->body.draw(ss, page, BODY_X, BODY_Y, BODY_W, BODY_H);
    dr->page.upload();
    dr->page.draw(ss);

    if (dr->hasRunes && ! dr->listOpen) {
        if (work->dirty)
            gpu_updateWorkBuffer(xu4.gpu, GPU_DLIST_GUI, work);
        gpu_enableGui(xu4.gpu, WID_NONE, 0);
        gpu_drawTrisRegion(xu4.gpu, GPU_DLIST_GUI, work->region);
    }
}

DocReader::DocReader()
{
    // Region 0 holds the runes; the work buffer needs at least two.
    int rsize[2] = { RUNE_QUADS * QUAD_ATTR, QUAD_ATTR };
    work = gpu_allocWorkBuffer(rsize, 2);
    book = DOC_HISTORY;
    lastBook = -1;
    chapter = top = 0;
    listOpen = 0;
    listSel = 0;
    pressed = -1;
    hasRunes = false;
    padHelp = false;
}

DocReader::~DocReader()
{
    gpu_freeWorkBuffer(work);
}

void DocReader::openBook(int n, int chap)
{
    string text;

    book = lastBook = n;
    if (n == BOOK_RUNES) {
        text = runesText();
    } else if (n == BOOK_HELP) {
        text = legacyHelpText();
    } else if (n == BOOK_JOURNAL) {
        text = legacyJournalText();
    } else if (! docsLoad(GameDocument(n), text)) {
        text = "\n"
            "This document was not found.\n\n"
            "The free GOG release of Ultima IV includes the printed\n"
            "documents as ULTIMA4.TXT.  Place it with the game files and\n"
            "xu4 will split it into separate documents.  Releases with\n"
            "separate History.txt, Wisdom.txt, Hints.txt and Keyboard.txt\n"
            "files are also found there, or in a docs folder beside the\n"
            "game files.\n\n"
            "The documents can also be put in the docs folder of\n";
        text += xu4.settings->getUserPath();
        text += "\nwith the names History.txt, Wisdom.txt, Hints.txt and\n"
                "Keyboard.txt.\n";
    }
    splitLines(text, lines);
    findChapters();
    listOpen = 0;
    openChapter(std::min(chap, int(chapters.size()) - 1));
}

/*
 * Split the book into chapters.  The History has numbered chapters, the
 * Book of Mystic Wisdom an entry for each reagent & spell, and the other
 * books upper case headings.  Text before the first chapter is the
 * Introduction.
 */
void DocReader::findChapters()
{
    std::vector<Chapter> found;
    Chapter ch;
    string name;
    size_t i;

    // Numbered chapters.
    for (i = 0; i < lines.size(); ++i) {
        string t = trimmed(lines[i]);
        if (chapterLine(t)) {
            ch.title = "Chapter " + t.substr(8);
            for (size_t j = i + 1; j < lines.size(); ++j) {
                if (! isBlank(lines[j])) {
                    ch.title += ": " + titleCase(trimmed(lines[j]));
                    break;
                }
            }
            ch.start = i;
            found.push_back(ch);
        }
    }

    // Entries.
    if (found.empty()) {
        for (i = 0; i < lines.size(); ++i) {
            if ((i == 0 || isBlank(lines[i-1])) &&
                entryLine(lines[i], &name)) {
                ch.title = name;
                ch.start = i;
                found.push_back(ch);
            }
        }
        if (found.size() < 5)
            found.clear();
    }

    // Headings.
    if (found.empty()) {
        for (i = 0; i < lines.size(); ++i) {
            if ((i == 0 || isBlank(lines[i-1])) && headingLine(lines[i])) {
                ch.title = titleCase(trimmed(lines[i]));
                ch.start = i;
                found.push_back(ch);
            }
        }
    }

    // A heading with nothing under it (such as the title of the book)
    // joins the next chapter.
    chapters.clear();
    for (i = 0; i < found.size(); ++i) {
        int end = (i + 1 < found.size()) ? found[i+1].start : lines.size();
        bool empty = titleOnly(found[i].start + 1, end);
        if (empty && i + 1 < found.size()) {
            found[i+1].start = found[i].start;
            continue;
        }
        chapters.push_back(found[i]);
    }

    // Introduction.  If it is only the title of the book, the title goes
    // at the top of the first chapter.
    int first = chapters.empty() ? lines.size() : chapters[0].start;
    if (! chapters.empty() && titleOnly(0, first)) {
        chapters[0].start = 0;
    } else {
        for (int n = 0; n < first; ++n) {
            if (! isBlank(lines[n])) {
                ch.title = chapters.empty() ? bookTitle(book)
                                            : "Introduction";
                ch.start = 0;
                chapters.insert(chapters.begin(), ch);
                break;
            }
        }
    }
    if (chapters.empty()) {
        ch.title = bookTitle(book);
        ch.start = 0;
        chapters.push_back(ch);
    }

    for (i = 0; i < chapters.size(); ++i)
        chapters[i].end = (i + 1 < chapters.size()) ? chapters[i+1].start
                                                     : lines.size();
}

// The first three letters of a name, in lower case.
static string entryKey(const string& name)
{
    string key;
    for (char ch : name) {
        if (isalpha((unsigned char) ch))
            key += tolower((unsigned char) ch);
        if (key.size() == 3)
            break;
    }
    return key;
}

/*
 * Find the chapter for a spell or reagent named as the game names it.  The
 * names differ a little from the book ("X-it" is "Xit", "Sulfur Ash" is
 * "Sulphurous Ash"), so only the first three letters are compared.
 * Returns -1 if there is none.
 */
int DocReader::findEntry(const string& name) const
{
    string key = entryKey(name);
    for (size_t i = 0; i < chapters.size(); ++i) {
        if (entryKey(chapters[i].title) == key)
            return i;
    }
    return -1;
}

/*
 * Return true if lines start to end hold nothing but headings and a few
 * centered title lines.
 */
bool DocReader::titleOnly(int start, int end) const
{
    int count = 0;
    for (int n = start; n < end; ++n) {
        const string& line = lines[n];
        if (isBlank(line) || headingLine(line))
            continue;
        if (line.find_first_not_of(' ') < 12 || ++count > 3)
            return false;
    }
    return true;
}

void DocReader::openChapter(int n)
{
    if (n < 0)
        n = 0;
    chapter = n;
    view.clear();

    size_t cols = body.width() / body.cellWidth();
    const Chapter& ch = chapters[n];

    // Lines too wide for the page are rejoined into paragraphs first so
    // that the prose wraps evenly at the bigger text sizes.
    bool reflow = false;
    for (int i = ch.start; i < ch.end; ++i) {
        if (! isBlank(lines[i]) &&
            trimmed(lines[i]).size() + lines[i].find_first_not_of(' ') >
                cols && lines[i].find(RUNE_MARK) == string::npos)
            reflow = true;
    }

    string para;
    size_t hang = string::npos;     // Indent of joined lines.
    for (int i = ch.start; i < ch.end; ++i) {
        const string& line = lines[i];
        if (reflow && i > ch.start && ! para.empty() &&
            joinsParagraph(lines[i-1], line)) {
            if (hang == string::npos)
                hang = line.find_first_not_of(' ');
            para += ' ';
            para += trimmed(line);
            continue;
        }
        if (! para.empty())
            wrapLine(para, cols, view, hang);
        hang = string::npos;
        para = line;
        while (! para.empty() && para.back() == ' ')
            para.pop_back();
        if (para.empty())
            view.push_back(para);
    }
    if (! para.empty())
        wrapLine(para, cols, view, hang);

    while (! view.empty() && isBlank(view.back()))
        view.pop_back();
    size_t lead = 0;
    while (lead < view.size() && isBlank(view[lead]))
        ++lead;
    view.erase(view.begin(), view.begin() + lead);

    top = 0;
    redraw();
}

int DocReader::bodyRows() const
{
    return body.height() / LINE_H;
}

void DocReader::scrollTo(int line)
{
    int end = int(view.size()) - bodyRows();
    if (line > end)
        line = end;
    if (line < 0)
        line = 0;
    if (line != top) {
        top = line;
        redraw();
    }
}

bool DocReader::present()
{
    if (! page.begin())
        return false;
    sizeBody();
    // Reopen the chapter last read in the same book (the journal is
    // remade each time, so it starts over).
    int chap = (book == lastBook && book != BOOK_JOURNAL) ? chapter : 0;
    openBook(book, chap);
    if (! entry.empty()) {
        int n = findEntry(entry);
        if (n >= 0)
            openChapter(n);
        entry.clear();
    }
    screenSetLayer(LAYER_TOP_MENU, renderReader, this);
    return true;
}

void DocReader::conclude()
{
    screenSetLayer(LAYER_TOP_MENU, NULL, NULL);
    page.release();
    body.release();
}

//----------------------------------------------------------------------------

void DocReader::addHit(int x, int y, int w, int h, int id)
{
    HitArea a;
    a.x = x;
    a.y = y;
    a.x2 = x + w;
    a.y2 = y + h;
    a.id = id;
    hits.push_back(a);
}

int DocReader::hitAt(int x, int y) const
{
    // The last added is on top.
    for (auto it = hits.rbegin(); it != hits.rend(); ++it) {
        if (x >= it->x && x < it->x2 && y >= it->y && y < it->y2)
            return it->id;
    }
    return -1;
}

void DocReader::redraw()
{
    hits.clear();
    hasRunes = false;
    gpu_endRegion(work, 0, gpu_beginRegion(work, 0));

    page.clear();
    page.frame(0, 0, page.width(), page.height(), 14);
    drawTabs();
    drawChapterBar();
    page.fill(EDGE + 4, RULE1_Y, page.width() - 2 * EDGE - 8, 2,
              TextPage::BLUE);
    page.fill(BODY_X, BODY_Y, BODY_W, BODY_H, TextPage::CLEAR);
    drawBody();
    page.fill(EDGE + 4, RULE2_Y, page.width() - 2 * EDGE - 8, 2,
              TextPage::BLUE);
    drawFooter();
    if (listOpen)
        drawChapterList();
}

void DocReader::drawTabs()
{
    int cw = page.cellWidth();
    int cols = (page.width() - 2 * EDGE) / cw;
    const char** names = bookTabs;
    int gap = 1;
    int total;
    int i;

    // Use the short names, then less space, if the tabs do not fit.
    for (int pass = 0; pass < 3; ++pass) {
        names = pass ? shortTabs : bookTabs;
        gap = (pass == 2) ? 0 : 1;
        total = 0;
        for (i = 0; i < BOOK_COUNT; ++i)
            total += strlen(names[i]) + 2 + gap;
        if (total - gap <= cols)
            break;
    }
    int x = (page.width() - (total - gap) * cw) / 2;

    for (i = 0; i < BOOK_COUNT; ++i) {
        int w = (strlen(names[i]) + 2) * cw;
        bool cur = (i == book);
        if (cur)
            page.fill(x, TAB_Y - 3, w, 14, TextPage::DK_BLUE);
        page.text(x + cw, TAB_Y, names[i],
                  cur ? TextPage::WHITE : TextPage::GREY);
        addHit(x, TAB_Y - 4, w, 16, HIT_TAB + i);
        x += w + gap * cw;
    }
}

void DocReader::drawChapterBar()
{
    const string& title = chapters[chapter].title;
    int cw = page.cellWidth();
    int W = page.width();
    int scale = (title.size() * 2 * cw <= size_t(W - 2 * (EDGE + 6 * cw)))
                ? 2 : 1;
    int y = (scale == 2) ? BAR_Y : BAR_Y + 4;
    size_t fit = (W - 2 * (EDGE + 6 * cw)) / cw;
    page.centered(y, title.substr(0, fit).c_str(), TextPage::WHITE, scale);
    addHit(EDGE + 6 * cw, BAR_Y - 2, W - 2 * (EDGE + 6 * cw), 20, HIT_TITLE);

    int n = chapters.size();
    if (n > 1) {
        char count[16];
        snprintf(count, sizeof(count), "%d/%d", chapter + 1, n);
        page.text(W - EDGE - 6 - page.textWidth(strlen(count)), BAR_Y + 4,
                  count, TextPage::GREY);
    }
    if (chapter > 0) {
        page.text(EDGE + 6, BAR_Y, "<", TextPage::BLUE, 2);
        addHit(EDGE, BAR_Y - 4, 4 * cw, 24, HIT_PREV);
    }
    if (chapter + 1 < n) {
        int x = W - EDGE - 6 - page.textWidth(strlen("00/00")) - 3 * cw;
        page.text(x, BAR_Y, ">", TextPage::BLUE, 2);
        addHit(x - cw, BAR_Y - 4, 4 * cw, 24, HIT_NEXT);
    }
}

void DocReader::drawBody()
{
    const ScreenState* ss = screenState();
    int cw = body.cellWidth();
    int x0 = 0;
    int rows = bodyRows();
    // Runes are placed in GUI units; the body page is scaled by zoom.
    float zoom = float(BODY_W) / body.width();
    float sx = float(ss->aspectW) / page.width();
    float sy = float(ss->aspectH) / page.height();

    body.clear();
    TxfDrawState ds;
    float* attr = NULL;

    for (int r = 0; r < rows; ++r) {
        size_t n = top + r;
        if (n >= view.size())
            break;
        const string& line = view[n];
        int y = r * LINE_H;
        size_t mark = line.find(RUNE_MARK);
        if (mark == string::npos) {
            body.text(x0, y, line.c_str(), (int) line.size(), TextPage::TEXT);
            continue;
        }

        // Runes are drawn by the GPU with the runes font, two cells each.
        if (! hasRunes) {
            hasRunes = true;
            ds.fontTable = ss->fontTable;
            txf_begin(&ds, FONT_RUNES, 19.0f * zoom * sy, 0.0f, 0.0f);
            ds.colorIndex = COL_WHITE;
            attr = gpu_beginRegion(work, 0);
        }
        int col = 0;
        size_t i = 0;
        while (i < line.size()) {
            if (line[i] == RUNE_MARK) {
                // Runes run to the end of the line or to a double space.
                size_t k = i + 1;
                for (; k < line.size(); ++k) {
                    if (line[k] == ' ') {
                        if (k + 1 >= line.size() || line[k+1] == ' ')
                            break;
                        ++col;
                        continue;
                    }
                    ds.x = ds.marginL = (BODY_X + col * cw * zoom) * sx;
                    ds.y = ss->aspectH - (BODY_Y + (y + 13) * zoom) * sy;
                    ds.prev = NULL;
                    attr = gui_emitText(&ds, attr, &line[k], 1);
                    col += 2;
                }
                i = k;
            } else {
                // Keep the columns of the table after wide runes.
                if (col < int(i))
                    col = i;
                body.text(x0 + col * cw, y, &line[i], 1, TextPage::TEXT);
                ++col;
                ++i;
            }
        }
    }
    if (hasRunes)
        gpu_endRegion(work, 0, attr);

    // Scroll bar.
    int total = view.size();
    if (total > rows) {
        int barH = BODY_H;
        int thumb = std::max(barH * rows / total, 8);
        int pos = (barH - thumb) * top / (total - rows);
        int bx = page.width() - EDGE - 6;
        page.fill(bx, BODY_Y, 2, barH, TextPage::DIM);
        page.fill(bx - 1, BODY_Y + pos, 4, thumb, TextPage::BLUE);
    }
}

void DocReader::drawFooter()
{
    // Two lines of help, each shortened until it fits.
    static const char* keyHelp1[] = {
        "Up/Down Scroll  Left/Right Chapter  Enter Contents",
        "Left/Right Chapter  Enter Contents",
        "Enter Contents", ""
    };
    static const char* keyHelp2[] = {
        "Tab or 1-7 Book  +/- Size", "Tab Book  +/- Size", "Tab Book", ""
    };
    static const char* padHelp1[] = {
        "Up/Down Scroll  Left/Right Chapter  A Contents",
        "Left/Right Chapter  A Contents",
        "A Contents", ""
    };
    static const char* padHelp2[] = {
        "Y Book  L/R Size", "Y Book", ""
    };
    int cw = page.cellWidth();
    bool pad = padHelp = gamepad_prompts();
    // Buttons from the right, between the two lines: Back, then larger &
    // smaller text.
    const char* label[3] = { pad ? "B Back" : "Esc Back", "+", "-" };
    int id[3] = { HIT_BACK, HIT_LARGER, HIT_SMALLER };
    int x = page.width() - EDGE - 6;
    int by = FOOT_Y + LINE_H / 2;
    for (int i = 0; i < 3; ++i) {
        int w = (strlen(label[i]) + 2) * cw;
        x -= w;
        page.fill(x, by - 3, w, 14, TextPage::DK_BLUE);
        page.text(x + cw, by, label[i], TextPage::WHITE);
        addHit(x, by - 4, w, 16, id[i]);
        x -= cw;
    }

    // As much help as fits before them.
    int room = (x - EDGE - 8) / cw - 1;
    const char** help = pad ? padHelp1 : keyHelp1;
    for (int line = 0; line < 2; ++line) {
        while (int(strlen(*help)) > room)
            ++help;
        page.text(EDGE + 8, FOOT_Y + line * LINE_H, *help, TextPage::GREY);
        help = pad ? padHelp2 : keyHelp2;
    }
}

/*
 * The table of contents, over the page.
 */
void DocReader::drawChapterList()
{
    int cw = page.cellWidth();
    int n = chapters.size();
    size_t widest = 12;
    for (const Chapter& ch : chapters)
        widest = std::max(widest, ch.title.size());
    int maxRows = (page.height() - 2 * EDGE - 48) / 9;
    int rows = std::min(n, maxRows);
    size_t maxTitle = (page.width() - 2 * EDGE - 16) / cw - 6;
    if (widest > maxTitle)
        widest = maxTitle;
    int w = (widest + 6) * cw + 2 * EDGE;
    int h = rows * 9 + 2 * EDGE + 28;
    int x = (page.width() - w) / 2;
    int y = (page.height() - h) / 2;

    page.fill(x, y, w, h, TextPage::BLACK);
    page.frame(x, y, w, h, 14);
    addHit(0, 0, page.width(), page.height(), -2);  // Swallow other clicks.
    page.centered(y + EDGE + 4, "Contents", TextPage::WHITE);

    int first = 0;
    if (listSel >= rows)
        first = listSel - rows + 1;
    int ty = y + EDGE + 20;
    for (int r = 0; r < rows; ++r) {
        int i = first + r;
        bool sel = (i == listSel);
        const string& title = chapters[i].title;
        char mark = sel ? CHARSET_PROMPT : CHARSET_BULLET;
        page.text(x + EDGE + cw, ty, &mark, 1,
                  sel ? TextPage::BLUE : TextPage::GREY);
        page.text(x + EDGE + 3 * cw, ty, title.c_str(),
                  (int) std::min(title.size(), widest),
                  sel ? TextPage::WHITE :
                  (i == chapter) ? TextPage::CYAN : TextPage::TEXT);
        addHit(x + EDGE, ty - 1, w - 2 * EDGE, 9, HIT_LIST + i);
        ty += 9;
    }
}

/*
 * Make the body page for the text size, with the shape of the body of the
 * reader.
 */
void DocReader::sizeBody()
{
    int size = xu4.settings->legacy.readerSize % SIZE_COUNT;
    int w = sizeCols[size] * page.cellWidth();
    body.beginPixels(w, w * BODY_H / BODY_W);
}

/*
 * Change the size of the text, keeping the place in the chapter.
 */
void DocReader::resize(int dir)
{
    uint8_t& size = xu4.settings->legacy.readerSize;
    int n = int(size) + dir;
    if (n < 0 || n >= SIZE_COUNT)
        return;
    soundPlay(SOUND_UI_TICK);
    size = n;
    xu4.settings->write();

    float place = view.empty() ? 0.0f : float(top) / view.size();
    sizeBody();
    openChapter(chapter);
    scrollTo(int(place * view.size() + 0.5f));
    redraw();
}

//----------------------------------------------------------------------------

void DocReader::activate(int id)
{
    if (id >= HIT_LIST) {
        listOpen = 0;
        openChapter(id - HIT_LIST);
    } else if (id >= HIT_TAB && id < HIT_TAB + BOOK_COUNT) {
        if (id - HIT_TAB != book)
            openBook(id - HIT_TAB);
    } else if (id == HIT_PREV) {
        if (chapter > 0)
            openChapter(chapter - 1);
    } else if (id == HIT_NEXT) {
        if (chapter + 1 < int(chapters.size()))
            openChapter(chapter + 1);
    } else if (id == HIT_TITLE) {
        listOpen = 1;
        listSel = chapter;
        redraw();
    } else if (id == HIT_SMALLER || id == HIT_LARGER) {
        resize(id == HIT_LARGER ? 1 : -1);
    } else if (id == HIT_BACK) {
        xu4.eventHandler->setControllerDone(true);
    }
}

bool DocReader::keyPressed(int key)
{
    int page = bodyRows() - 2;
    int n = chapters.size();

    if (listOpen) {
        switch (key) {
            case U4_UP:
                if (listSel > 0) {
                    --listSel;
                    soundPlay(SOUND_UI_TICK);
                    redraw();
                }
                return true;
            case U4_DOWN:
                if (listSel + 1 < n) {
                    ++listSel;
                    soundPlay(SOUND_UI_TICK);
                    redraw();
                }
                return true;
            case U4_PAGE_UP:
                listSel = std::max(0, listSel - 10);
                redraw();
                return true;
            case U4_PAGE_DOWN:
                listSel = std::min(n - 1, listSel + 10);
                redraw();
                return true;
            case U4_ENTER:
            case U4_SPACE:
            case U4_RIGHT:
                soundPlay(SOUND_UI_CLICK);
                activate(HIT_LIST + listSel);
                return true;
            case U4_ESC:
            case U4_LEFT:
            case 'c':
                listOpen = 0;
                redraw();
                return true;
        }
        return true;
    }

    switch (key) {
        case U4_UP:
            scrollTo(top - 1);
            return true;
        case U4_DOWN:
            scrollTo(top + 1);
            return true;
        case U4_SPACE:
        case U4_PAGE_DOWN:
            // At the end of a chapter Space goes on to the next.
            if (key == U4_SPACE && top + bodyRows() >= int(view.size()) &&
                chapter + 1 < n) {
                soundPlay(SOUND_UI_TICK);
                openChapter(chapter + 1);
            } else
                scrollTo(top + page);
            return true;
        case U4_BACKSPACE:
        case U4_PAGE_UP:
            scrollTo(top - page);
            return true;
        case U4_HOME:
            scrollTo(0);
            return true;
        case U4_END:
            scrollTo(view.size());
            return true;
        case U4_LEFT:
            if (chapter > 0) {
                soundPlay(SOUND_UI_TICK);
                openChapter(chapter - 1);
            }
            return true;
        case U4_RIGHT:
            if (chapter + 1 < n) {
                soundPlay(SOUND_UI_TICK);
                openChapter(chapter + 1);
            }
            return true;
        case U4_ENTER:
        case 'c':
            if (n > 1) {
                soundPlay(SOUND_UI_CLICK);
                activate(HIT_TITLE);
            }
            return true;
        case '+':
        case '=':
        case U4_PAD_R:
            resize(1);
            return true;
        case '-':
        case U4_PAD_L:
            resize(-1);
            return true;
        case U4_TAB:
        case 'z':
        case U4_PAD_Y:
            soundPlay(SOUND_UI_TICK);
            openBook((book + 1) % BOOK_COUNT);
            return true;
        case U4_ESC:
            xu4.eventHandler->setControllerDone(true);
            return true;
        default:
            if (key >= '1' && key < '1' + BOOK_COUNT) {
                soundPlay(SOUND_UI_TICK);
                openBook(key - '1');
                return true;
            }
            break;
    }
    return false;
}

bool DocReader::inputEvent(const InputEvent* ev)
{
    const ScreenState* ss = screenState();
    int x, y;
    page.pagePos(ss, ev->x - ss->aspectX, (ss->displayH - ev->y) - ss->aspectY,
                 &x, &y);

    switch (ev->type) {
        case IE_MOUSE_PRESS:
            pressed = (ev->n == CMOUSE_LEFT) ? hitAt(x, y) : -1;
            break;

        case IE_MOUSE_RELEASE:
            if (ev->n == CMOUSE_LEFT && pressed >= 0 &&
                hitAt(x, y) == pressed) {
                soundPlay(SOUND_UI_CLICK);
                activate(pressed);
            } else if (ev->n == CMOUSE_LEFT && listOpen && pressed == -2 &&
                       hitAt(x, y) == -2) {
                listOpen = 0;       // Clicked outside the list.
                redraw();
            }
            pressed = -1;
            break;

        case IE_MOUSE_WHEEL:
            if (listOpen) {
                int n = chapters.size();
                listSel += (ev->y < 0) ? 1 : -1;
                listSel = std::max(0, std::min(n - 1, listSel));
                redraw();
            } else if (ev->y < 0)
                scrollTo(top + 3);
            else if (ev->y > 0)
                scrollTo(top - 3);
            break;
    }
    return true;
}
