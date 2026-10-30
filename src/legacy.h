/*
 * legacy.h - Legacy Edition features after the Ultima III & V ports.
 */

#ifndef LEGACY_H
#define LEGACY_H

#include <string>
#include <vector>

enum LegacyCheat {
    CHEAT_HEAL,
    CHEAT_GOLD,
    CHEAT_FOOD,
    CHEAT_REAGENTS,
    CHEAT_TOOLS,
    CHEAT_LEVEL,
    CHEAT_VIRTUE,
    CHEAT_ITEMS,
    CHEAT_EQUIP
};

const char* legacyCheat(int cheat);
const char* legacySpellEffect(int spell);
bool legacyHealShortcut(int* spell, int* caster, int* subject, int onlyCaster);


// Saves & characters.
struct LegacySaveInfo {
    std::string dir;        // Directory holding the save files.
    std::string folder;     // Character folder name (characters only).
    std::string label;      // "Name, level 3 - Britain" ...
    long long   time;       // Seconds since the epoch of the save.
    bool        active;     // The character being played.
};

extern bool legacyReloadPending;

void legacySaveDone(const char* userPath);
void legacyAutosave();
bool legacyWipeChoice();
std::vector<LegacySaveInfo> legacyCharacters();
std::vector<LegacySaveInfo> legacySaveHistory(const LegacySaveInfo& ch);
bool legacyParkActive();
void legacyNameNewCharacter(const char* avatarName);
bool legacySelectCharacter(const LegacySaveInfo& ch);
bool legacyRestoreSave(const LegacySaveInfo& save);
bool legacyDeleteCharacter(const LegacySaveInfo& ch);

// Journal.
void legacyNoteClue(const char* place, const char* who, const char* text);
void legacyJournalReset();
std::string legacyJournalText();
const char* legacyVirtueWord(int karma);

// Dungeon map: the cells of each dungeon level the party has seen.
void legacyDungeonLoad();
bool legacyDungeonWrite(const char* userPath);
void legacyDungeonSee(int mapId, int level, int x, int y);
bool legacyDungeonSeen(int mapId, int level, int x, int y);

#endif
