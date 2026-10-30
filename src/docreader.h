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
#include "gui.h"

struct ScreenState;
struct WorkBuffer;

/*
 * Scrolling viewer for the printed game documents.
 */
class DocReader : public Controller {
public:
    DocReader();
    ~DocReader();

    /* controller functions */
    virtual bool present();
    virtual void conclude();
    virtual bool keyPressed(int key);
    virtual bool inputEvent(const InputEvent*);

    void showJournal();

    enum Widgets {
        WI_TEXT,
        WI_TAB,
        WI_BACK = WI_TAB + DOC_COUNT + 2,     // Then the runes & journal.
        WI_COUNT
    };

private:
    std::vector<std::string> lines;
    GuiArea gbox[ WI_COUNT ];
    WorkBuffer* work;
    int doc;
    int firstLine;              // First line in the text region.
    int lineCount;              // Lines in the text region.
    int buttonDown;
    float scroll;
    float scrollTarget;
    float psize;
    float lineSpacing;
    float descender;
    float columnW;
    float textPad;

    void openDocument(int n);
    void layout();
    void generateText();
    void generateScrollBar();
    float maxScroll() const;
    void scrollTo(float y);
    void scrollLines(int n);

    static void renderReader(ScreenState* ss, void* data);
};

#endif
