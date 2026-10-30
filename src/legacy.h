/*
 * legacy.h - Legacy Edition features after the Ultima III & V ports.
 */

#ifndef LEGACY_H
#define LEGACY_H

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

#endif
