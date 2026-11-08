/*
 * $Id$
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include "types.h"

#define MOD_NAME_LIMIT                  40

#define MIN_SHAKE_INTERVAL              50

#define MAX_BATTLE_SPEED                10
#define MAX_KEY_DELAY                   1000
#define MAX_KEY_INTERVAL                100
#define MAX_CYCLES_PER_SECOND           20
#define MAX_SPELL_EFFECT_SPEED          10
#define MAX_CAMP_TIME                   10
#define MAX_INN_TIME                    10
#define MAX_SHRINE_TIME                 20
#define MAX_SHAKE_INTERVAL              200
#define MAX_VOLUME                      10

#define DEFAULT_SCALE                   2
#define DEFAULT_FULLSCREEN              0

#define DEFAULT_FILTER                  0       // point
// 0 = DOS
#define DEFAULT_LINEOFSIGHT             0

#define DEFAULT_GEM_LAYOUT              "Standard"
#define DEFAULT_SCREEN_SHAKES           1
#define DEFAULT_GAMMA                   100
#define DEFAULT_MUSIC_VOLUME            10
#define DEFAULT_SOUND_VOLUME            10
#define DEFAULT_VOLUME_FADES            1
#define DEFAULT_SHORTCUT_COMMANDS       0
#define DEFAULT_KEY_DELAY               500
#define DEFAULT_KEY_INTERVAL            30
#define DEFAULT_FILTER_MOVE_MESSAGES    0
#define DEFAULT_BATTLE_SPEED            5
#define DEFAULT_ENHANCEMENTS            1
#define DEFAULT_CYCLES_PER_SECOND       4
#define DEFAULT_ANIMATION_FRAMES_PER_SECOND 24
#define DEFAULT_DEBUG                   0
#define DEFAULT_VALIDATE_XML            1
#define DEFAULT_SPELL_EFFECT_SPEED      10
#define DEFAULT_CAMP_TIME               10
#define DEFAULT_INN_TIME                8
#define DEFAULT_SHRINE_TIME             16
#define DEFAULT_SHAKE_INTERVAL          100
#define DEFAULT_BATTLE_DIFFICULTY       BattleDiff_Normal
#define DEFAULT_LOGGING                 ""
#define DEFAULT_TITLE_SPEED_RANDOM      150
#define DEFAULT_TITLE_SPEED_OTHER       30

#define DEFAULT_PAUSE_FOR_EACH_TURN     100
#define DEFAULT_PAUSE_FOR_EACH_MOVEMENT 10

//--Tile transparency stuff
#define DEFAULT_SHADOW_PIXEL_OPACITY    64
#define DEFAULT_SHADOW_PIXEL_SIZE       2

enum BattleDifficulty {
    BattleDiff_Normal,
    BattleDiff_Hard,
    BattleDiff_Expert
};

struct SettingsEnhancementOptions {
    bool activePlayer;
    bool u5spellMixing;
    bool u5shrines;
    bool u5combat;
    bool slimeDivides;
    bool gazerSpawnsInsects;
    bool textColorization;
    bool c64chestTraps;
    bool smartEnterKey;
    bool smsInterface;      // Sega Master System style menus & talk
    bool peerShowsObjects;
    bool u4TileTransparencyHack;
    int  u4TileTransparencyHackPixelShadowOpacity;
    int  u4TrileTransparencyHackShadowBreadth;
};

/*
 * Legacy Edition options (after the Ultima III & V ports).  The
 * convenience features default to off so that the game plays as it always
 * has until they are chosen in the Legacy Options menu.
 */
enum LegacyPreset {
    LegacyPreset_Classic,
    LegacyPreset_Modern,
    LegacyPreset_Story,
    LegacyPreset_Custom
};

enum LegacyStarvation {
    Starve_Classic,         // Damage until death.
    Starve_Mild,            // Stops at half hit points.
    Starve_None
};

enum LegacyTurnTimer {
    TurnTimer_Fast,         // The original 20 second pass.
    TurnTimer_Slow,         // 60 seconds.
    TurnTimer_Off
};

enum LegacyAutoCombat {
    AutoCombat_Off,
    AutoCombat_Companions,  // All but the Avatar.
    AutoCombat_Party
};

enum LegacyDungeonMap {
    DungeonMap_Off,
    DungeonMap_Mini,        // A small map in the corner of the view.
    DungeonMap_TopDown      // The map in place of the 3-D view.
};

enum LegacyMoonSpeed {
    MoonSpeed_Slowest,      // An eighth of Normal.
    MoonSpeed_Slower,       // A quarter.
    MoonSpeed_Slow,         // Half.
    MoonSpeed_Normal,       // As DOS: a Felucca phase every 4 seconds.
    MoonSpeed_Fast,         // Twice as fast.
    MoonSpeed_Faster,       // Three times, about the pace of the SMS.
    MoonSpeed_Fastest       // Four times.
};

// How fast a held key, gamepad direction or touch repeats (Max Input).
enum LegacyHeldPace {
    HeldPace_System,        // Keys repeat as the system has them.
    HeldPace_Slow,
    HeldPace_Medium,
    HeldPace_Fast
};

enum LegacyMapView {
    MapView_Standard,       // The tiles the view was laid out for.
    MapView_Fill            // As many tiles as fit (smaller SMS tiles).
};

struct LegacyOptions {
    // Convenience
    bool bumpToAct;         // Walking into someone talks, a foe attacks...
    bool foldMoves;         // "North (x5)" instead of five lines.
    bool freeCancel;        // Backing out of a command spends no turn.
    bool shopCompare;       // Shops compare arms with what is held.
    bool spellInfo;         // Spell lists show cost & effect.
    bool healShortcut;      // Action menu offers Cast Heal/Cure.
    bool snapAttacks;       // X strikes a foe in reach (combat).
    bool pauseUnfocused;    // Pause while the window has no focus.
    bool autosave;          // Save on entering & leaving places, history.
    bool keepCharacters;    // New games keep the others; pick at Journey.
    bool batterySaver;      // Stop animating after a minute idle.
    bool scanlines;
    bool smoothMotion;      // The map slides from square to square.
    bool partyColours;      // Party panel names & hit points coloured.
    bool cheatsMenu;        // Cheats in the in-game settings.
    bool steamSaves;        // Saves also go to the Steam release's folder.
    bool journalHints;      // Journal place pages name who to ask.
    bool combatTips;        // A tip in the log during the first fights.
    bool menuCounts;        // Action menu: torches, gems & keys left.
    bool lightFirst;        // Action menu: torch or Light first if dark.
    bool nameSteal;         // Action menu: a town chest is "Steal Chest".
    bool xpOnZtats;         // Action menu: Ztats shows each member's XP.
    bool mapCreatures;      // Dungeon mini map marks creatures in sight.
    bool snapMark;          // Snap Attacks: mark the target, PgUp/PgDn or L/R pick another.
    bool autoExtras;        // Auto Combat: mark, action menu item, stuck check.
    bool sceneStop;         // Held keys stop repeating at a new scene.
    bool spellExtras;       // Spell lists: mana, mixtures made, castable first.
    bool shopColours;       // Shop lists grey the wares the party cannot pay for.
    bool quickMix;          // Console mixing: a known recipe by the number.
    bool blockReason;       // "Blocked by water!" in place of "Blocked!".
    bool askRules;          // A new game asks which rules to play by.
    bool mapHere;           // The map opens on the town or dungeon level.
    bool gateNotes;         // The map names where each moongate led.
    bool shopBuyer;         // Arms shops ask who is buying first.
    bool capDials;          // Shop dials stop at what the party can afford.
    uint8_t autoCombat;     // LegacyAutoCombat
    uint8_t dungeonMap;     // LegacyDungeonMap
    uint8_t readerSize;     // Document reader text size (0-2).
    uint8_t moonSpeed;      // LegacyMoonSpeed
    uint8_t heldPace;       // LegacyHeldPace
    uint8_t mapView;        // LegacyMapView
    uint8_t padMap[6];      // PadAction of each PadMapButton in play.
    // Rules
    uint8_t preset;         // LegacyPreset
    uint8_t starvation;     // LegacyStarvation
    uint8_t turnTimer;      // LegacyTurnTimer
    bool poisonKills;
    bool sharedXp;          // Kill experience is shared by the party.
    bool wipeChoice;        // A party wipe offers to load the last save.
    bool wakeOnHit;         // A sleeper who is hit has an even chance to wake.
};

struct MouseOptions {
    bool enabled;
};

/**
 * SettingsData stores all the settings information.
 */
class SettingsData {
public:
    bool operator==(const SettingsData &) const;
    bool operator!=(const SettingsData &) const;

    int                 battleSpeed;
    bool                campingAlwaysCombat;
    int                 campTime;
    bool                debug;
    bool                enhancements;
    SettingsEnhancementOptions enhancementsOptions;
    bool                filterMoveMessages;
    bool                fullscreen;
    int                 gameCyclesPerSecond;
    int                 screenAnimationFramesPerSecond;
    bool                innAlwaysCombat;
    int                 innTime;
    int                 keydelay;
    int                 keyinterval;
    LegacyOptions       legacy;
    MouseOptions        mouseOptions;
    int                 musicVol;
    unsigned int        scale;
    bool                screenShakes;
    int                 gamma;
    int                 shakeInterval;
    bool                shortcutCommands;
    int                 shrineTime;     // Seconds for one meditation cycle.
    int                 soundVol;
    int                 spellEffectSpeed;
    bool                volumeFades;
    int                 titleSpeedRandom;
    int                 titleSpeedOther;
    uint8_t             battleDiff;     // Used by Creature
    uint8_t             filter;         // Defined by screen
    uint8_t             lineOfSight;    // Defined by screen
    char game[MOD_NAME_LIMIT];
    char soundtrack[MOD_NAME_LIMIT];

    /**
     * Strings, classes, and other objects that cannot
     * be bitwise-compared must be placed here at the
     * end of the list so that our == and != operators
     * function correctly
     */
    long                end_of_bitwise_comparators;

    std::string         gemLayout;      // Defined by Config
    std::string         logging;        // Used by Debug
};

/**
 * The settings class is a singleton that holds all the settings
 * information.
 */
class Settings : public SettingsData {
public:
    static uint8_t settingEnum(const char** names, const char* value);
    static const char** battleDiffStrings();
    static const char** legacyPresetStrings();
    static const char** starvationStrings();
    static const char** turnTimerStrings();
    static const char** autoCombatStrings();
    static const char** dungeonMapStrings();
    static const char** mapViewStrings();
    static const char** moonSpeedStrings();
    static const char** heldPaceStrings();
    int heldRepeat(int* delay) const;
    static void applyLegacyPreset(SettingsData& data, int preset);

    void init(const char* profileName);
    void setData(const SettingsData &data);
    void setGame(const char* modName);
    void setSoundtrack(const char* modName);
    bool write();
    const std::string &getUserPath() const { return userPath; }

    std::string profile;

private:
    bool read();

    std::string userPath;
    std::string filename;
};

#endif
