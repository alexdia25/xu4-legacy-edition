#ifndef STEAM_H
#define STEAM_H

#include <string>

/*
 * The Steam release of Ultima IV (app 3067510) & its cloud saves.
 *
 * The DOS game run by Steam keeps its save (PARTY.SAV, MONSTERS.SAV,
 * OUTMONST.SAV & DNGMAP.SAV) in the install folder, and Steam Cloud carries
 * it between computers.  xu4 saves in the same format, so with the Steam
 * Cloud Saves option on, each save is also written there, and a newer save
 * found there (made on another computer) is offered at Journey Onward.
 */

struct SteamSaveInfo {
    std::string name;   // The Avatar's name.
    int level;
    std::string when;   // When the save was written, e.g. "2026-11-08 05:12".
};

bool steamGameFilesUsed();
bool steamSavesExport(const std::string& userPath, bool dungeon);
bool steamSavesNewer(const std::string& userPath, SteamSaveInfo* info);
bool steamSavesImport(const std::string& userPath);

#endif
