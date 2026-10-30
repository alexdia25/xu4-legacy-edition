/*
 * legacy.cpp - Legacy Edition features after the Ultima III & V ports.
 */

#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <system_error>
#include <algorithm>
#include <chrono>
#include "context.h"
#include "discourse.h"
#include "event.h"
#include "game.h"
#include "location.h"
#include "names.h"
#include "map.h"
#include "mapmgr.h"
#include "screen.h"
#include "settings.h"
#include "party.h"
#include "spell.h"
#include "config.h"
#include "legacy.h"
#include "weapon.h"
#include "savegame.h"
#include "stats.h"
#include "xu4.h"

/*
 * Apply one of the cheats offered in the in-game settings.
 * Returns a message saying what was done.
 */
const char* legacyCheat(int cheat) {
    SaveGame* sg = c->saveGame;
    int i;

    switch (cheat) {
    case CHEAT_HEAL:
        for (i = 0; i < sg->members; i++) {
            sg->players[i].status = STAT_GOOD;
            sg->players[i].hp = sg->players[i].hpMax;
        }
        c->stats->update();
        return "The party is healed!";

    case CHEAT_GOLD:
        sg->gold += 500;
        if (sg->gold > 9999)
            sg->gold = 9999;
        c->stats->update();
        return "Gold!";

    case CHEAT_FOOD:
        sg->food += 100 * 100;
        if (sg->food > 999900)
            sg->food = 999900;
        c->stats->update();
        return "Food!";

    case CHEAT_REAGENTS:
        for (i = 0; i < REAG_MAX; i++) {
            sg->reagents[i] += 10;
            if (sg->reagents[i] > 99)
                sg->reagents[i] = 99;
        }
        return "Reagents!";

    case CHEAT_TOOLS:
        sg->keys    = (sg->keys    + 5 > 99) ? 99 : sg->keys + 5;
        sg->gems    = (sg->gems    + 5 > 99) ? 99 : sg->gems + 5;
        sg->torches = (sg->torches + 5 > 99) ? 99 : sg->torches + 5;
        return "Keys, gems & torches!";

    case CHEAT_LEVEL:
        for (i = 0; i < sg->members; i++) {
            SaveGamePlayerRecord& pr = sg->players[i];
            if (pr.hpMax < 800) {
                pr.hpMax += 100;
                if (pr.hpMax > 800)
                    pr.hpMax = 800;
                if (pr.xp < (pr.hpMax / 100 - 1) * 100 * 2)
                    pr.xp = (pr.hpMax / 100 - 1) * 100 * 2;
            }
            if (pr.status != STAT_DEAD)
                pr.hp = pr.hpMax;
        }
        c->stats->update();
        return "Every level raised!";

    case CHEAT_VIRTUE:
        for (i = 0; i < VIRT_MAX; i++) {
            if (sg->karma[i] != 0) {
                sg->karma[i] += 10;
                if (sg->karma[i] > 99)
                    sg->karma[i] = 99;
            }
        }
        c->stats->update();
        return "The virtues advance!";

    case CHEAT_ITEMS:
        sg->sextants = 1;
        sg->items |= ITEM_SKULL | ITEM_CANDLE | ITEM_BOOK | ITEM_BELL |
                     ITEM_KEY_C | ITEM_KEY_L | ITEM_KEY_T | ITEM_HORN |
                     ITEM_WHEEL;
        sg->stones = 0xff;
        sg->runes = 0xff;
        c->stats->update();
        return "The quest items!";

    case CHEAT_EQUIP:
        for (i = ARMR_NONE + 1; i < ARMR_MAX; i++)
            if (sg->armor[i] < 8)
                sg->armor[i] = 8;
        for (i = WEAP_HANDS + 1; i < WEAP_MAX; i++) {
            const Weapon* weapon = xu4.config->weapon(static_cast<WeaponType>(i));
            int n = (weapon->loseWhenUsed() || weapon->loseWhenRanged()) ? 99 : 8;
            if (sg->weapons[i] < n)
                sg->weapons[i] = n;
        }
        return "Arms & armour!";
    }
    return "";
}

/*
 * Return a few words on what a spell does (for the Spell Info option).
 */
const char* legacySpellEffect(int spell) {
    static const char* effects[26] = {
        "Wakes one",        // Awaken
        "Party leaps far",  // Blink
        "Cures poison",     // Cure
        "Removes a field",  // Dispel
        "Makes a field",    // Energy Field
        "Fire at a foe",    // Fireball
        "To a moongate",    // Gate Travel
        "Heals one",        // Heal
        "Ice at a foe",     // Iceball
        "Foes fight foes",  // Jinx
        "Slays a foe",      // Kill
        "Lights the way",   // Light
        "Strikes a foe",    // Magic Missile
        "Stops all magic",  // Negate
        "Disarms a chest",  // Open
        "Shields party",    // Protection
        "Party is faster",  // Quickness
        "Raises the dead",  // Resurrect
        "Puts foes asleep", // Sleep
        "Quake vs. foes",   // Tremor
        "Turns undead",     // Undead
        "Shows the land",   // View
        "Turns the wind",   // Winds
        "Out of dungeon",   // X-it
        "Up a level",       // Y-up
        "Down a level"      // Z-down
    };
    return (spell >= 0 && spell < 26) ? effects[spell] : "";
}

/*
 * Find a heal (or cure) the party needs: the most hurt (or a poisoned)
 * member and the caster with the most magic points who can cast it.
 * If onlyCaster is not negative only that member may cast.
 * Returns false if there is no such spell to cast.
 */
bool legacyHealShortcut(int* spell, int* caster, int* subject, int onlyCaster) {
    const SaveGame* sg = c->saveGame;
    Party* party = c->party;
    const int SP_CURE = 'c' - 'a';
    const int SP_HEAL = 'h' - 'a';
    int i;

    // Poison first, as it hurts each turn.
    int sp = -1, who = -1, worst = 0;
    for (i = 0; i < party->size(); i++) {
        PartyMember* pm = party->member(i);
        if (pm->isDead())
            continue;
        if (pm->getStatus() == STAT_POISONED && sg->mixtures[SP_CURE] > 0) {
            sp = SP_CURE;
            who = i;
            break;
        }
        int lost = pm->getMaxHp() - pm->getHp();
        if (lost > worst && sg->mixtures[SP_HEAL] > 0 &&
            pm->getHp() * 4 < pm->getMaxHp() * 3) {
            worst = lost;
            sp = SP_HEAL;
            who = i;
        }
    }
    if (sp < 0)
        return false;

    int need = spellGetRequiredMP(sp);
    int best = -1, bestMp = -1;
    for (i = 0; i < party->size(); i++) {
        if (onlyCaster >= 0 && i != onlyCaster)
            continue;
        PartyMember* pm = party->member(i);
        if (pm->isDisabled() || pm->getMp() < need)
            continue;
        if (pm->getMp() > bestMp) {
            bestMp = pm->getMp();
            best = i;
        }
    }
    if (best < 0)
        return false;

    *spell = sp;
    *caster = best;
    *subject = who;
    return true;
}

//--------------------------------------------------------------------------
// Saves & characters
//
// The game being played is always saved in the user directory as before.
// Each save is also copied to history/<character>/1 (the older ones move
// up to 8).  Other characters are kept in characters/<folder>/ and are
// swapped with the active one when chosen.  character.txt in the user
// directory names the folder of the active character.

namespace fs = std::filesystem;

bool legacyReloadPending = false;

#define HISTORY_COUNT   8
#define DNGSEEN_SAV     "dngseen.sav"

static const char* saveFiles[] = {
    PARTY_SAV, MONSTERS_SAV, OUTMONST_SAV, DNGMAP_SAV, TALKWORDS_SAV,
    "journal.sav", DNGSEEN_SAV, NULL
};

static fs::path userDir() {
    return fs::u8path(xu4.settings->getUserPath());
}

static void copySaveFiles(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    fs::create_directories(to, ec);
    for (const char** fn = saveFiles; *fn; ++fn) {
        fs::path src = from / *fn;
        fs::path dst = to / *fn;
        if (fs::exists(src, ec))
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
        else
            fs::remove(dst, ec);
    }
}

/*
 * Read the avatar's name & level from a party.sav file.
 */
static bool readPartyInfo(const fs::path& dir, std::string* name, int* level) {
    FILE* fp = fopen((dir / PARTY_SAV).string().c_str(), "rb");
    if (! fp)
        return false;
    SaveGame sg;
    bool ok = sg.read(fp) && sg.members > 0;
    fclose(fp);
    if (ok) {
        *name = sg.players[0].name;
        *level = sg.players[0].hpMax / 100;
    }
    return ok;
}

/*
 * Return a name safe to use as a directory.
 */
static std::string folderName(const std::string& name) {
    std::string out;
    for (char ch : name) {
        if (isalnum((unsigned char) ch) || ch == ' ' || ch == '-' || ch == '_')
            out += ch;
    }
    while (! out.empty() && out.back() == ' ')
        out.pop_back();
    return out.empty() ? std::string("Avatar") : out;
}

static std::string readText(const fs::path& file) {
    std::string text;
    FILE* fp = fopen(file.string().c_str(), "r");
    if (fp) {
        char buf[256];
        if (fgets(buf, sizeof(buf), fp)) {
            text = buf;
            while (! text.empty() && (text.back() == '\n' || text.back() == '\r'))
                text.pop_back();
        }
        fclose(fp);
    }
    return text;
}

static void writeText(const fs::path& file, const std::string& text) {
    FILE* fp = fopen(file.string().c_str(), "w");
    if (fp) {
        fprintf(fp, "%s\n", text.c_str());
        fclose(fp);
    }
}

/*
 * Return the character folder of the active game (creating the name from
 * the avatar if there is none yet), or an empty string if there is no game.
 */
static std::string activeFolder() {
    fs::path base = userDir();
    std::string folder = readText(base / "character.txt");
    if (folder.empty()) {
        std::string name;
        int level;
        if (! readPartyInfo(base, &name, &level))
            return folder;
        folder = folderName(name);
        writeText(base / "character.txt", folder);
    }
    return folder;
}

static long long fileTime(const fs::path& file) {
    std::error_code ec;
    auto ft = fs::last_write_time(file, ec);
    if (ec)
        return 0;
    auto sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ft - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return (long long) std::chrono::system_clock::to_time_t(sys);
}

static std::string describe(const fs::path& dir, const char* what) {
    std::string name;
    int level = 0;
    if (! readPartyInfo(dir, &name, &level))
        return std::string();
    std::string place = readText(dir / "place.txt");
    char buf[96];
    snprintf(buf, sizeof(buf), "%s%s, level %d%s%s", what, name.c_str(), level,
             place.empty() ? "" : " - ", place.c_str());
    return buf;
}

/*
 * Called after the game is saved: note where, and keep a copy in the
 * save history.
 */
void legacySaveDone(const char* userPath) {
    std::error_code ec;
    fs::path base = fs::u8path(userPath);
    std::string folder = activeFolder();
    if (folder.empty())
        return;

    const Map* map = c->location->map;
    const char* place = map->isWorldMap() ? "Britannia" : map->getName();
    writeText(base / "place.txt", place ? place : "");

    fs::path hist = base / "history" / fs::u8path(folder);
    fs::create_directories(hist, ec);
    fs::remove_all(hist / std::to_string(HISTORY_COUNT), ec);
    for (int i = HISTORY_COUNT - 1; i >= 1; --i) {
        fs::path from = hist / std::to_string(i);
        if (fs::exists(from, ec))
            fs::rename(from, hist / std::to_string(i + 1), ec);
    }
    fs::path dst = hist / "1";
    copySaveFiles(base, dst);
    fs::copy_file(base / "place.txt", dst / "place.txt",
                  fs::copy_options::overwrite_existing, ec);
}

/*
 * Save the game, if the Autosave option is on and the game can be saved
 * here.  Called on entering & leaving places.
 */
void legacyAutosave() {
    if (! xu4.settings->legacy.autosave)
        return;
    if (! (c->location->context & CTX_CAN_SAVE_GAME) || c->party->isDead())
        return;
    if (gameSave(xu4.settings->getUserPath().c_str()))
        screenMessage("%c(saved)%c\n", FG_GREY, FG_WHITE);
}

/*
 * The party has fallen.  With the Party Wipe option, offer to go back to
 * the last save.  Returns true if the game will be reloaded.
 */
bool legacyWipeChoice() {
    if (! xu4.settings->legacy.wipeChoice)
        return false;
    if (! fs::exists(userDir() / PARTY_SAV))
        return false;

    screenMessage("\n\n%cThe party has fallen!%c\nTry again from the last save? ",
                  FG_YELLOW, FG_WHITE);
    static const char* const labels[] = { "Yes", "No", NULL, NULL };
    int ch = EventHandler::readChoice("yn \033", labels);
    if (ch != 'y') {
        screenMessage("\n");
        return false;
    }

    // Restart from the title, which loads the save straight away.
    legacyReloadPending = true;
    xu4.gameReset = 1;
    xu4.eventHandler->quitGame();
    return true;
}

/*
 * List the active character (first) & the others kept.
 */
std::vector<LegacySaveInfo> legacyCharacters() {
    std::vector<LegacySaveInfo> list;
    std::error_code ec;
    fs::path base = userDir();
    std::string active = activeFolder();

    if (! active.empty()) {
        LegacySaveInfo info;
        info.dir    = base.string();
        info.folder = active;
        info.label  = describe(base, "");
        info.time   = fileTime(base / PARTY_SAV);
        info.active = true;
        if (! info.label.empty())
            list.push_back(info);
    }

    fs::path chars = base / "characters";
    if (fs::is_directory(chars, ec)) {
        std::vector<LegacySaveInfo> others;
        for (const auto& ent : fs::directory_iterator(chars, ec)) {
            if (! ent.is_directory())
                continue;
            LegacySaveInfo info;
            info.dir    = ent.path().string();
            // u8string() gives std::u8string from C++20 (GCC 16's default).
            auto name = ent.path().filename().u8string();
            info.folder.assign(name.begin(), name.end());
            if (info.folder == active)
                continue;
            info.label  = describe(ent.path(), "");
            info.time   = fileTime(ent.path() / PARTY_SAV);
            info.active = false;
            if (! info.label.empty())
                others.push_back(info);
        }
        std::sort(others.begin(), others.end(),
                  [](const LegacySaveInfo& a, const LegacySaveInfo& b) {
                      return a.time > b.time;
                  });
        list.insert(list.end(), others.begin(), others.end());
    }
    return list;
}

/*
 * List the saves of a character: the last one then the earlier ones kept
 * in its history.
 */
std::vector<LegacySaveInfo> legacySaveHistory(const LegacySaveInfo& ch) {
    std::vector<LegacySaveInfo> list;
    std::error_code ec;
    fs::path hist = userDir() / "history" / fs::u8path(ch.folder);
    for (int i = 1; i <= HISTORY_COUNT; ++i) {
        fs::path dir = hist / std::to_string(i);
        if (! fs::exists(dir / PARTY_SAV, ec))
            continue;
        LegacySaveInfo info;
        info.dir    = dir.string();
        info.folder = ch.folder;
        info.time   = fileTime(dir / PARTY_SAV);
        info.active = false;
        std::string place = readText(dir / "place.txt");
        char when[32];
        time_t t = (time_t) info.time;
        strftime(when, sizeof(when), "%b %d %H:%M", localtime(&t));
        info.label = std::string(when) + (place.empty() ? "" : " " + place);
        list.push_back(info);
    }
    return list;
}

/*
 * Keep the active game in characters/ so that a new one can be started
 * without losing it.  Returns false if there was nothing to keep.
 */
bool legacyParkActive() {
    std::string folder = activeFolder();
    if (folder.empty())
        return false;
    fs::path base = userDir();
    fs::path dst = base / "characters" / fs::u8path(folder);
    copySaveFiles(base, dst);
    std::error_code ec;
    fs::copy_file(base / "place.txt", dst / "place.txt",
                  fs::copy_options::overwrite_existing, ec);
    for (const char** fn = saveFiles; *fn; ++fn)
        fs::remove(base / *fn, ec);
    fs::remove(base / "character.txt", ec);
    fs::remove(base / "place.txt", ec);
    return true;
}

/*
 * Give a new character a folder that no other character uses.
 */
void legacyNameNewCharacter(const char* avatarName) {
    fs::path base = userDir();
    std::string folder = folderName(avatarName);
    std::string name = folder;
    std::error_code ec;
    for (int n = 2; fs::exists(base / "characters" / fs::u8path(name), ec) ||
                    fs::exists(base / "history" / fs::u8path(name), ec); ++n)
        name = folder + " " + std::to_string(n);
    writeText(base / "character.txt", name);
}

/*
 * Make a kept character the active one (the active one is kept first).
 */
bool legacySelectCharacter(const LegacySaveInfo& ch) {
    if (ch.active)
        return true;
    fs::path src = fs::u8path(ch.dir);
    legacyParkActive();
    fs::path base = userDir();
    copySaveFiles(src, base);
    std::error_code ec;
    fs::copy_file(src / "place.txt", base / "place.txt",
                  fs::copy_options::overwrite_existing, ec);
    writeText(base / "character.txt", ch.folder);
    fs::remove_all(src, ec);
    return true;
}

/*
 * Make an earlier save from the history the active game.
 */
bool legacyRestoreSave(const LegacySaveInfo& save) {
    fs::path base = userDir();
    if (activeFolder() != save.folder) {
        // Find and activate the character first.
        std::vector<LegacySaveInfo> chars = legacyCharacters();
        for (const LegacySaveInfo& ch : chars)
            if (ch.folder == save.folder)
                legacySelectCharacter(ch);
    }
    fs::path src = fs::u8path(save.dir);
    copySaveFiles(src, base);
    std::error_code ec;
    fs::copy_file(src / "place.txt", base / "place.txt",
                  fs::copy_options::overwrite_existing, ec);
    writeText(base / "character.txt", save.folder);
    return true;
}

/*
 * Delete a character with all its saves.
 */
bool legacyDeleteCharacter(const LegacySaveInfo& ch) {
    std::error_code ec;
    fs::path base = userDir();
    if (ch.active) {
        for (const char** fn = saveFiles; *fn; ++fn)
            fs::remove(base / *fn, ec);
        fs::remove(base / "character.txt", ec);
        fs::remove(base / "place.txt", ec);
    } else {
        fs::remove_all(fs::u8path(ch.dir), ec);
    }
    fs::remove_all(base / "history" / fs::u8path(ch.folder), ec);
    return true;
}

//--------------------------------------------------------------------------
// Journal
//
// Clues are lines of conversation which mention the things of the quest.
// They are kept in journal.sav as "place<TAB>person<TAB>text" lines as they
// are heard.

#define JOURNAL_SAV "journal.sav"

/*
 * Return the word for how far along the path of a virtue the Avatar is,
 * after what Hawkwind says of it.
 */
const char* legacyVirtueWord(int karma) {
    if (karma == 0)
        return "Avatar";        // Elevated to partial Avatarhood.
    if (karma < 20)
        return "Strayed";
    if (karma < 40)
        return "Seeker";
    if (karma < 60)
        return "Learned";
    if (karma < 99)
        return "Wise";
    return "Ready";             // Ready to be elevated at the shrine.
}

static bool isClue(const char* text) {
    static const char* terms[] = {
        "rune", "stone", "bell", "book", "candle", "skull", "horn",
        "wheel", "mantra", "key", "codex", "abyss", "syllable",
        "word of passage", "altar", "sextant", "mondain", NULL
    };
    std::string low(text);
    for (char& ch : low)
        ch = tolower((unsigned char) ch);
    for (const char** t = terms; *t; ++t)
        if (low.find(*t) != std::string::npos)
            return true;
    return false;
}

static std::string oneLine(const char* text) {
    std::string out;
    bool space = false;
    for (const char* cp = text; *cp; ++cp) {
        unsigned char ch = *cp;
        if (ch == '\n' || ch == '\r' || ch == '\t' || ch == ' ') {
            space = ! out.empty();
            continue;
        }
        if (ch < ' ')
            continue;       // Color codes & such.
        if (space)
            out += ' ';
        space = false;
        out += ch;
    }
    return out;
}

static std::vector<std::string> journalLines() {
    std::vector<std::string> lines;
    FILE* fp = fopen((userDir() / JOURNAL_SAV).string().c_str(), "r");
    if (fp) {
        char buf[1024];
        while (fgets(buf, sizeof(buf), fp)) {
            std::string line(buf);
            while (! line.empty() && (line.back() == '\n' || line.back() == '\r'))
                line.pop_back();
            if (! line.empty())
                lines.push_back(line);
        }
        fclose(fp);
    }
    return lines;
}

/*
 * Note a line of conversation in the journal if it is a clue to the quest.
 */
void legacyNoteClue(const char* place, const char* who, const char* text) {
    if (! text || ! isClue(text))
        return;
    std::string line = std::string(place ? place : "") + "\t" +
                       (who ? who : "") + "\t" + oneLine(text);
    for (const std::string& old : journalLines())
        if (old == line)
            return;
    FILE* fp = fopen((userDir() / JOURNAL_SAV).string().c_str(), "a");
    if (fp) {
        fprintf(fp, "%s\n", line.c_str());
        fclose(fp);
    }
}

/*
 * Forget the journal (for a new game).
 */
void legacyJournalReset() {
    std::error_code ec;
    fs::remove(userDir() / JOURNAL_SAV, ec);
    fs::remove(userDir() / DNGSEEN_SAV, ec);
    legacyDungeonLoad();
}

static void wrapText(std::string& out, const std::string& text,
                     const char* indent, size_t width) {
    size_t pos = 0;
    while (pos < text.size()) {
        size_t len = text.size() - pos;
        if (len > width) {
            len = text.rfind(' ', pos + width);
            if (len == std::string::npos || len <= pos)
                len = width;
            else
                len -= pos;
        }
        out += indent;
        out += text.substr(pos, len);
        out += "\n";
        pos += len;
        while (pos < text.size() && text[pos] == ' ')
            ++pos;
    }
}

/*
 * Write the journal as text for the document reader.
 */
std::string legacyJournalText() {
    std::string out;
    char buf[128];

    if (! c || ! c->saveGame || xu4.stage != StagePlay) {
        out = "\n\n    The journal is kept while a game is being played.\n";
        return out;
    }
    const SaveGame* sg = c->saveGame;
    int i, n;

    out += "\n    THE PATH OF THE AVATAR\n\n";
    for (i = 0; i < VIRT_MAX; ++i) {
        snprintf(buf, sizeof(buf), "      %-14s %s\n",
                 getVirtueName((Virtue) i), legacyVirtueWord(sg->karma[i]));
        out += buf;
    }
    out += "\n";
    wrapText(out, "Strayed, Seeker, Learned and Wise are steps along the "
                  "path of a virtue.  Ready means the Avatar may be "
                  "elevated in it, and Avatar that this is done.", "    ", 60);

    out += "\n\n    THE QUEST\n\n";
    std::string list;
    for (i = 0, n = 0; i < VIRT_MAX; ++i) {
        if (sg->runes & (1 << i)) {
            if (n++)
                list += ", ";
            list += getVirtueName((Virtue) i);
        }
    }
    snprintf(buf, sizeof(buf), "Runes (%d of 8): ", n);
    wrapText(out, std::string(buf) + (n ? list : "none yet"), "      ", 56);

    static const char* stones[8] = {
        "Blue", "Yellow", "Red", "Green", "Orange", "Purple", "White", "Black"
    };
    list.clear();
    for (i = 0, n = 0; i < 8; ++i) {
        if (sg->stones & (1 << i)) {
            if (n++)
                list += ", ";
            list += stones[i];
        }
    }
    snprintf(buf, sizeof(buf), "Stones (%d of 8): ", n);
    wrapText(out, std::string(buf) + (n ? list : "none yet"), "      ", 56);

    struct Thing { int bit; int usedBit; const char* name; };
    static const Thing principles[3] = {
        { ITEM_BELL,   ITEM_BELL_USED,   "The Bell of Courage" },
        { ITEM_BOOK,   ITEM_BOOK_USED,   "The Book of Truth" },
        { ITEM_CANDLE, ITEM_CANDLE_USED, "The Candle of Love" }
    };
    for (i = 0; i < 3; ++i) {
        const Thing& t = principles[i];
        snprintf(buf, sizeof(buf), "      %-22s %s\n", t.name,
                 (sg->items & t.usedBit) ? "used" :
                 (sg->items & t.bit) ? "held" : "-");
        out += buf;
    }

    list.clear();
    static const Thing keyParts[3] = {
        { ITEM_KEY_T, 0, "Truth" }, { ITEM_KEY_L, 0, "Love" },
        { ITEM_KEY_C, 0, "Courage" }
    };
    for (i = 0, n = 0; i < 3; ++i) {
        if (sg->items & keyParts[i].bit) {
            if (n++)
                list += ", ";
            list += keyParts[i].name;
        }
    }
    snprintf(buf, sizeof(buf), "Parts of the Key (%d of 3): ", n);
    wrapText(out, std::string(buf) + (n ? list : "none yet"), "      ", 56);

    list.clear();
    n = 0;
    if (sg->items & ITEM_SKULL_DESTROYED) {
        list += "the Skull, destroyed";
        ++n;
    } else if (sg->items & ITEM_SKULL) {
        list += "a Skull";
        ++n;
    }
    if (sg->items & ITEM_HORN)   { list += n++ ? ", a Horn" : "a Horn"; }
    if (sg->items & ITEM_WHEEL)  { list += n++ ? ", a Wheel" : "a Wheel"; }
    if (sg->sextants)            { list += n++ ? ", a Sextant" : "a Sextant"; }
    if (n)
        wrapText(out, "Also found: " + list, "      ", 56);

    out += "\n\n    CLUES HEARD\n\n";
    std::vector<std::string> lines = journalLines();
    if (lines.empty()) {
        wrapText(out, "Nothing yet.  Words of the runes, stones and other "
                      "things of the quest are noted here as they are heard.",
                 "      ", 56);
    }
    std::string lastPlace;
    for (const std::string& line : lines) {
        size_t t1 = line.find('\t');
        size_t t2 = (t1 == std::string::npos) ? t1 : line.find('\t', t1 + 1);
        if (t2 == std::string::npos)
            continue;
        std::string place = line.substr(0, t1);
        std::string who = line.substr(t1 + 1, t2 - t1 - 1);
        std::string text = line.substr(t2 + 1);
        if (place != lastPlace) {
            out += "\n      " + place + "\n";
            lastPlace = place;
        }
        wrapText(out, who + ": " + text, "        ", 54);
    }
    return out;
}

//----------------------------------------------------------------------------
// Dungeon map
//
// One bit for each cell of the eight levels of the eight dungeons, kept in
// dngseen.sav.  Bit x of dngSeen[dungeon][level][y] is set once the party
// has seen that cell.

static uint8_t dngSeen[8][8][8];

static uint8_t* seenRow(int mapId, int level, int y) {
    int dng = mapId - MAP_DECEIT;
    if (dng < 0 || dng > 7 || level < 0 || level > 7 || y < 0 || y > 7)
        return NULL;
    return &dngSeen[dng][level][y];
}

void legacyDungeonLoad() {
    memset(dngSeen, 0, sizeof(dngSeen));
    FILE* fp = fopen((userDir() / DNGSEEN_SAV).string().c_str(), "rb");
    if (fp) {
        if (fread(dngSeen, 1, sizeof(dngSeen), fp) != sizeof(dngSeen))
            memset(dngSeen, 0, sizeof(dngSeen));
        fclose(fp);
    }
}

bool legacyDungeonWrite(const char* userPath) {
    std::string fn(userPath);
    fn += DNGSEEN_SAV;
    FILE* fp = fopen(fn.c_str(), "wb");
    if (! fp)
        return false;
    bool ok = fwrite(dngSeen, 1, sizeof(dngSeen), fp) == sizeof(dngSeen);
    fclose(fp);
    return ok;
}

void legacyDungeonSee(int mapId, int level, int x, int y) {
    uint8_t* row = seenRow(mapId, level, y);
    if (row && x >= 0 && x < 8)
        *row |= 1 << x;
}

bool legacyDungeonSeen(int mapId, int level, int x, int y) {
    const uint8_t* row = seenRow(mapId, level, y);
    return row && x >= 0 && x < 8 && (*row & (1 << x));
}
