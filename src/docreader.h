/*
 * XU4 - Ultima IV document reader
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DOCREADER_H
#define DOCREADER_H

#include <string>
#include <vector>
#include "controller.h"
#include "docs.h"
#include "textpage.h"

struct ScreenState;
struct WorkBuffer;

/*
 * Viewer for the printed game documents, the rune table, the How to Play
 * guide & the quest journal.  Each is split into chapters which are read
 * one at a time.
 */
class DocReader : public Controller {
public:
    enum Book {
        BOOK_RUNES = DOC_COUNT,
        BOOK_HELP,
        BOOK_JOURNAL,
        BOOK_COUNT
    };

    DocReader();
    ~DocReader();

    /* controller functions */
    virtual bool present();
    virtual void conclude();
    virtual bool keyPressed(int key);
    virtual bool inputEvent(const InputEvent*);

    void showJournal() { book = BOOK_JOURNAL; }
    void showBook(int n) { book = n; entry.clear(); }
    void showEntry(int n, const char* name) { book = n; entry = name; }

private:
    struct Chapter {
        std::string title;
        int start;              // First line in lines.
        int end;                // One past the last line.
    };

    struct HitArea {
        int16_t x, y, x2, y2;
        int16_t id;
    };

    TextPage page;
    std::vector<std::string> lines;     // The whole book.
    std::vector<Chapter> chapters;
    std::vector<std::string> view;      // The chapter, wrapped to the page.
    std::vector<HitArea> hits;
    std::string entry;          // Chapter to open (see showEntry).
    WorkBuffer* work;
    int book;
    int lastBook;               // Book & chapter to return to.
    int chapter;
    int top;                    // First line of view on the page.
    int listOpen;               // Chapter list showing.
    int listSel;
    int pressed;                // HitArea id pressed by the pointer.
    bool hasRunes;

    void openBook(int n, int chap = 0);
    void openChapter(int n);
    void findChapters();
    int  findEntry(const std::string& name) const;
    bool titleOnly(int start, int end) const;
    void redraw();
    void drawTabs();
    void drawChapterBar();
    void drawBody();
    void drawFooter();
    void drawChapterList();
    void addHit(int x, int y, int w, int h, int id);
    int  hitAt(int x, int y) const;
    int  bodyRows() const;
    void scrollTo(int line);
    void activate(int id);
    void resize(int dir);

    static void renderReader(ScreenState* ss, void* data);
};

#endif
