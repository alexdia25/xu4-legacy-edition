/*
 * legacy.cpp - Legacy Edition features after the Ultima III & V ports.
 */

#include "context.h"
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
        "Moves the party",  // Blink
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
        "Opens a chest",    // Open
        "Shields party",    // Protection
        "Party is faster",  // Quickness
        "Raises the dead",  // Resurrect
        "Puts foes asleep", // Sleep
        "Shakes the foes",  // Tremor
        "Turns undead",     // Undead
        "Shows the map",    // View
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
