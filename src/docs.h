/*
 * XU4 - Ultima IV documents
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DOCS_H
#define DOCS_H

#include <string>

enum GameDocument {
    DOC_HISTORY,        // The History of Britannia
    DOC_WISDOM,         // The Book of Mystic Wisdom
    DOC_HINTS,          // Hints
    DOC_KEYBOARD,       // Player reference card
    DOC_COUNT
};

const char* docsFileName(GameDocument doc);
const char* docsTitle(GameDocument doc);
bool docsLoad(GameDocument doc, std::string& text);
bool docsSplit(const std::string& text, std::string docs[DOC_COUNT]);
void docsExtract(const std::string& userPath);

#endif
