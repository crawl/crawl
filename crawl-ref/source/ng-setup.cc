#include "AppHdr.h"

#include "ng-setup.h"

#include "ability.h"
#include "decks.h"
#include "dungeon.h"
#include "end.h"
#include "files.h"
#include "food.h"
#include "godcompanions.h"
#include "hints.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "jobs.h"
#include "mutation.h"
#include "ng-init.h"
#include "ng-wanderer.h"
#include "options.h"
#include "prompt.h"
#include "religion.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-util.h"
#include "state.h"

#define MIN_START_STAT       3

static void _init_player()
{
    you.init();
    dlua.callfn("dgn_clear_data", "");
}

// Randomly boost stats a number of times.
static void _wanderer_assign_remaining_stats(int points_left)
{
    while (points_left > 0)
    {
        // Stats that are already high will be chosen half as often.
        stat_type stat = static_cast<stat_type>(random2(NUM_STATS));
        if (you.base_stats[stat] > 17 && coinflip())
            continue;

        you.base_stats[stat]++;
        points_left--;
    }
}

static void _jobs_stat_init(job_type which_job)
{
    int s = 0;   // strength mod
    int i = 0;   // intelligence mod
    int d = 0;   // dexterity mod

    // Note: Wanderers are correct, they've got a challenging background. - bwr
    switch (which_job)
    {
    case JOB_FIGHTER:           s =  8; i =  0; d =  4; break;
    case JOB_BERSERKER:         s =  9; i = -1; d =  4; break;
    case JOB_GLADIATOR:         s =  7; i =  0; d =  5; break;

    case JOB_SKALD:             s =  4; i =  4; d =  4; break;
    case JOB_CHAOS_KNIGHT:      s =  4; i =  4; d =  4; break;
    case JOB_ABYSSAL_KNIGHT:    s =  4; i =  4; d =  4; break;

    case JOB_ASSASSIN:          s =  3; i =  3; d =  6; break;

    case JOB_HUNTER:            s =  4; i =  3; d =  5; break;
    case JOB_WARPER:            s =  3; i =  5; d =  4; break;
    case JOB_ARCANE_MARKSMAN:   s =  3; i =  5; d =  4; break;

    case JOB_MONK:              s =  3; i =  2; d =  7; break;
    case JOB_TRANSMUTER:        s =  2; i =  5; d =  5; break;

    case JOB_WIZARD:            s = -1; i = 10; d =  3; break;
    case JOB_CONJURER:          s =  0; i =  7; d =  5; break;
    case JOB_ENCHANTER:         s =  0; i =  7; d =  5; break;
    case JOB_FIRE_ELEMENTALIST: s =  0; i =  7; d =  5; break;
    case JOB_ICE_ELEMENTALIST:  s =  0; i =  7; d =  5; break;
    case JOB_AIR_ELEMENTALIST:  s =  0; i =  7; d =  5; break;
    case JOB_EARTH_ELEMENTALIST:s =  0; i =  7; d =  5; break;
    case JOB_SUMMONER:          s =  0; i =  7; d =  5; break;
    case JOB_VENOM_MAGE:        s =  0; i =  7; d =  5; break;
    case JOB_NECROMANCER:       s =  0; i =  7; d =  5; break;

    case JOB_WANDERER:
        // Wanderers get their stats randomly distributed.
        _wanderer_assign_remaining_stats(12);           break;

    case JOB_ARTIFICER:         s =  3; i =  4; d =  5; break;
    default:                    s =  0; i =  0; d =  0; break;
    }

    you.base_stats[STAT_STR] += s;
    you.base_stats[STAT_INT] += i;
    you.base_stats[STAT_DEX] += d;

    you.hp_max_adj_perm = 0;
}

// Make sure no stats are unacceptably low
// (currently possible only for GhBe - 1KB)
static void _unfocus_stats()
{
    int needed;

    for (int i = 0; i < NUM_STATS; ++i)
    {
        int j = (i + 1) % NUM_STATS;
        int k = (i + 2) % NUM_STATS;
        if ((needed = MIN_START_STAT - you.base_stats[i]) > 0)
        {
            if (you.base_stats[j] > you.base_stats[k])
                you.base_stats[j] -= needed;
            else
                you.base_stats[k] -= needed;
            you.base_stats[i] = MIN_START_STAT;
        }
    }
}

// Some consumables to make the starts of Sprint and Zotdef a little easier.
static void _give_bonus_items()
{
    newgame_make_item(OBJ_POTIONS, POT_CURING);
    newgame_make_item(OBJ_POTIONS, POT_HEAL_WOUNDS);
    newgame_make_item(OBJ_POTIONS, POT_HASTE);
    newgame_make_item(OBJ_POTIONS, POT_MAGIC, 2);
    newgame_make_item(OBJ_POTIONS, POT_BERSERK_RAGE);
    newgame_make_item(OBJ_SCROLLS, SCR_BLINKING);
}

static void _autopickup_ammo(missile_type missile)
{
    if (Options.autopickup_starting_ammo)
        you.force_autopickup[OBJ_MISSILES][missile] = 1;
}

/**
 * Make an item during character creation.
 *
 * Puts the item in the first available slot in the inventory, equipping or
 * memorising the first spell from it as appropriate. If the item would be
 * useless, we try with a possibly more useful sub_type, then, if it's still
 * useless, give up and don't create any item.
 *
 * @param base, sub_type what the item is
 * @param qty            what size stack to make
 * @param plus           what the value of item_def::plus should be
 * @param force_ego      what the value of item_def::special should be
 * @param force_tutorial whether to create it even in the tutorial
 * @returns a pointer to the item created, if any.
 */
item_def* newgame_make_item(object_class_type base,
                            int sub_type, int qty, int plus,
                            int force_ego, bool force_tutorial)
{
    // Don't set normal equipment in the tutorial.
    if (!force_tutorial && crawl_state.game_is_tutorial())
        return nullptr;

    // not an actual item
    if (sub_type == WPN_UNARMED)
        return nullptr;

    int slot;
    for (slot = 0; slot < ENDOFPACK; ++slot)
    {
        item_def& item = you.inv[slot];
        if (!item.defined())
            break;

        if (item.is_type(base, sub_type) && item.special == force_ego && is_stackable_item(item))
        {
            item.quantity += qty;
            return &item;
        }
    }

    item_def &item(you.inv[slot]);
    item.base_type = base;
    item.sub_type  = sub_type;
    item.quantity  = qty;
    item.plus      = plus;
    item.special   = force_ego;

    // If the character is restricted in wearing the requested armour,
    // hand out a replacement instead.
    if (item.base_type == OBJ_ARMOUR
        && !can_wear_armour(item, false, false))
    {
        if (item.sub_type == ARM_HELMET)
            item.sub_type = ARM_HAT;
        else if (item.sub_type == ARM_BUCKLER)
            item.sub_type = ARM_SHIELD;
        else if (is_shield(item))
            item.sub_type = ARM_BUCKLER;
        else
            item.sub_type = ARM_ROBE;
    }

    // If that didn't help, nothing will.
    if (is_useless_item(item))
    {
        item = item_def();
        return nullptr;
    }

    if (item.base_type == OBJ_WEAPONS && can_wield(&item, false, false)
        || item.base_type == OBJ_ARMOUR && can_wear_armour(item, false, false))
    {
        you.equip[get_item_slot(item)] = slot;
    }

    if (item.base_type == OBJ_MISSILES)
        _autopickup_ammo(static_cast<missile_type>(item.sub_type));
    // You can get the books without the corresponding items as a wanderer.
    else if (item.base_type == OBJ_BOOKS && item.sub_type == BOOK_GEOMANCY)
        _autopickup_ammo(MI_STONE);
    else if (item.base_type == OBJ_BOOKS && item.sub_type == BOOK_CHANGES)
        _autopickup_ammo(MI_ARROW);

    origin_set_startequip(item);

    // Wanderers may or may not already have a spell. - bwr
    // Also, when this function gets called their possible randbook
    // has not been initalised and will trigger an ASSERT.
    if (item.base_type == OBJ_BOOKS && you.char_class != JOB_WANDERER)
    {
        spell_type which_spell = spells_in_book(item)[0];
        if (!spell_is_useless(which_spell, false, true))
            add_spell_to_memory(which_spell);
    }

    return &item;
}

static void _give_ranged_weapon(weapon_type weapon, int plus, int skill)
{
    ASSERT(weapon != NUM_WEAPONS);

    switch (weapon)
    {
    case WPN_THROWN:
        if (species_can_throw_large_rocks(you.species))
            newgame_make_item(OBJ_MISSILES, MI_LARGE_ROCK, 4 + plus);
        else if (you.body_size(PSIZE_TORSO) <= SIZE_SMALL)
            newgame_make_item(OBJ_MISSILES, MI_TOMAHAWK, 8 + 2 * plus);
        else
            newgame_make_item(OBJ_MISSILES, MI_JAVELIN, 5 + plus);
        newgame_make_item(OBJ_MISSILES, MI_THROWING_NET, 2);
        you.skills[SK_THROWING] = skill;
        break;
    case WPN_SHORTBOW:
        newgame_make_item(OBJ_WEAPONS, WPN_SHORTBOW, 1, plus);
        newgame_make_item(OBJ_MISSILES, MI_ARROW, 20);
        you.skills[SK_BOWS] = skill;
        break;
    case WPN_HAND_CROSSBOW:
        newgame_make_item(OBJ_WEAPONS, WPN_HAND_CROSSBOW, 1, plus);
        newgame_make_item(OBJ_MISSILES, MI_BOLT, 20);
        you.skills[SK_CROSSBOWS] = skill;
        break;
    case WPN_HUNTING_SLING:
        newgame_make_item(OBJ_WEAPONS, WPN_HUNTING_SLING, 1, plus);
        newgame_make_item(OBJ_MISSILES, MI_SLING_BULLET, 20);
        you.skills[SK_SLINGS] = skill;
        _autopickup_ammo(MI_STONE);
        break;
    default:
        break;
    }
}

static void _give_items_skills(const newgame_def& ng)
{
    int weap_skill = 0;

    switch (you.char_class)
    {
    case JOB_FIGHTER:
        // Equipment.
        newgame_make_item(OBJ_WEAPONS, ng.weapon);

        newgame_make_item(OBJ_ARMOUR, ARM_SCALE_MAIL);
        newgame_make_item(OBJ_ARMOUR, ARM_SHIELD);
        newgame_make_item(OBJ_POTIONS, POT_MIGHT);

        // Skills.
        you.skills[SK_FIGHTING] = 3;
        you.skills[SK_SHIELDS]  = 3;
        you.skills[SK_ARMOUR]   = 3;

        weap_skill = (you.species == SP_FELID) ? 4 : 2;

        break;

    case JOB_GLADIATOR:
        // Equipment.
        newgame_make_item(OBJ_WEAPONS, ng.weapon);

        newgame_make_item(OBJ_ARMOUR, ARM_LEATHER_ARMOUR);
        newgame_make_item(OBJ_ARMOUR, ARM_HELMET);
        newgame_make_item(OBJ_MISSILES, MI_THROWING_NET, 3);

        // Skills.
        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_THROWING] = 2;
        you.skills[SK_DODGING]  = 3;
        weap_skill = 3;
        break;

    case JOB_MONK:
        newgame_make_item(OBJ_ARMOUR, ARM_ROBE);

        you.skills[SK_FIGHTING]       = 3;
        you.skills[SK_UNARMED_COMBAT] = 4;
        you.skills[SK_DODGING]        = 3;
        you.skills[SK_STEALTH]        = 2;
        break;

    case JOB_BERSERKER:
        you.religion = GOD_TROG;
        you.piety = 35;

        // WEAPONS
        newgame_make_item(OBJ_WEAPONS, ng.weapon);

        // ARMOUR
        newgame_make_item(OBJ_ARMOUR, ARM_ANIMAL_SKIN);

        // SKILLS
        you.skills[SK_FIGHTING] = 3;
        you.skills[SK_DODGING]  = 2;
        weap_skill = 3;

        if (you_can_wear(EQ_BODY_ARMOUR))
            you.skills[SK_ARMOUR] = 2;
        else
        {
            you.skills[SK_DODGING]++;
            if (!is_useless_skill(SK_ARMOUR))
                you.skills[SK_ARMOUR]++; // converted later
        }
        break;

    case JOB_CHAOS_KNIGHT:
        you.religion = GOD_XOM;
        you.piety = 100;
        you.gift_timeout = max(5, random2(40) + random2(40));

        newgame_make_item(OBJ_WEAPONS, ng.weapon, 1, 0, SPWPN_CHAOS);
        newgame_make_item(OBJ_ARMOUR, ARM_LEATHER_ARMOUR, 1, 2);

        you.skills[SK_FIGHTING] = 3;
        you.skills[SK_ARMOUR]   = 1;
        you.skills[SK_DODGING]  = 1;
        if (species_apt(SK_ARMOUR) < species_apt(SK_DODGING))
            you.skills[SK_DODGING]++;
        else
            you.skills[SK_ARMOUR]++;
        weap_skill = 3;
        break;

    case JOB_ABYSSAL_KNIGHT:
        you.religion = GOD_LUGONU;
        if (!crawl_state.game_is_zotdef() && !crawl_state.game_is_sprint())
            you.char_direction = GDT_GAME_START;
        you.piety = 38;

        newgame_make_item(OBJ_WEAPONS, ng.weapon, 1, +1);
        newgame_make_item(OBJ_ARMOUR, ARM_LEATHER_ARMOUR);

        you.skills[SK_FIGHTING]    = 3;
        you.skills[SK_ARMOUR]      = 1;
        you.skills[SK_DODGING]     = 1;
        if (species_apt(SK_ARMOUR) < species_apt(SK_DODGING))
            you.skills[SK_DODGING]++;
        else
            you.skills[SK_ARMOUR]++;

        you.skills[SK_INVOCATIONS] = 2;
        weap_skill = 2;
        break;

    case JOB_SKALD:
        newgame_make_item(OBJ_WEAPONS, ng.weapon);
        newgame_make_item(OBJ_ARMOUR, ARM_LEATHER_ARMOUR);
        newgame_make_item(OBJ_BOOKS, BOOK_BATTLE);

        you.skills[SK_FIGHTING]     = 2;
        you.skills[SK_ARMOUR]       = 1;
        you.skills[SK_DODGING]      = 1;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_CHARMS]       = 3;
        weap_skill = 2;
        break;

    case JOB_WARPER:
        newgame_make_item(OBJ_WEAPONS, ng.weapon);

        newgame_make_item(OBJ_ARMOUR, ARM_LEATHER_ARMOUR);
        newgame_make_item(OBJ_BOOKS, BOOK_SPATIAL_TRANSLOCATIONS);

        // One free escape.
        newgame_make_item(OBJ_SCROLLS, SCR_BLINKING);
        newgame_make_item(OBJ_MISSILES, MI_TOMAHAWK, 5, 0, SPMSL_DISPERSAL);

        you.skills[SK_FIGHTING]       = 2;
        you.skills[SK_ARMOUR]         = 1;
        you.skills[SK_DODGING]        = 2;
        you.skills[SK_SPELLCASTING]   = 2;
        you.skills[SK_TRANSLOCATIONS] = 3;
        you.skills[SK_THROWING]       = 1;
        weap_skill = 2;
    break;

    case JOB_ARCANE_MARKSMAN:
        newgame_make_item(OBJ_ARMOUR, ARM_ROBE);
        _give_ranged_weapon(ng.weapon, 0, 2);

        newgame_make_item(OBJ_BOOKS, BOOK_DEBILITATION);

        you.skills[SK_FIGHTING]                   = 1;
        you.skills[SK_DODGING]                    = 2;
        you.skills[SK_SPELLCASTING]               = 1;
        you.skills[SK_HEXES]                      = 3;
        break;

    case JOB_WIZARD:
        newgame_make_item(OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(OBJ_ARMOUR, ARM_HAT);

        newgame_make_item(OBJ_BOOKS, BOOK_MINOR_MAGIC);

        you.skills[SK_DODGING]        = 2;
        you.skills[SK_STEALTH]        = 2;
        you.skills[SK_SPELLCASTING]   = 3;
        you.skills[SK_TRANSLOCATIONS] = 1;
        you.skills[SK_CONJURATIONS]   = 1;
        you.skills[SK_SUMMONINGS]     = 1;
        break;

    case JOB_CONJURER:
        newgame_make_item(OBJ_ARMOUR, ARM_ROBE);

        newgame_make_item(OBJ_BOOKS, BOOK_CONJURATIONS);

        you.skills[SK_CONJURATIONS] = 4;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_ENCHANTER:
        newgame_make_item(OBJ_WEAPONS, WPN_DAGGER, 1, +1);
        newgame_make_item(OBJ_ARMOUR, ARM_ROBE, 1, +1);
        newgame_make_item(OBJ_BOOKS, BOOK_MALEDICT);

        weap_skill = 1;
        you.skills[SK_HEXES]        = 3;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 3;
        break;

    case JOB_SUMMONER:
        newgame_make_item(OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(OBJ_BOOKS, BOOK_CALLINGS);

        you.skills[SK_SUMMONINGS]   = 4;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_NECROMANCER:
        newgame_make_item(OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(OBJ_BOOKS, BOOK_NECROMANCY);

        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_NECROMANCY]   = 4;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_TRANSMUTER:
        // Some sticks for sticks to snakes.
        newgame_make_item(OBJ_MISSILES, MI_ARROW, 12);
        newgame_make_item(OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(OBJ_BOOKS, BOOK_CHANGES);

        you.skills[SK_FIGHTING]       = 1;
        you.skills[SK_UNARMED_COMBAT] = 3;
        you.skills[SK_DODGING]        = 2;
        you.skills[SK_SPELLCASTING]   = 2;
        you.skills[SK_TRANSMUTATIONS] = 2;
        break;

    case JOB_FIRE_ELEMENTALIST:
        newgame_make_item(OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(OBJ_BOOKS, BOOK_FLAMES);

        you.skills[SK_CONJURATIONS] = 1;
        you.skills[SK_FIRE_MAGIC]   = 3;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_ICE_ELEMENTALIST:
        newgame_make_item(OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(OBJ_BOOKS, BOOK_FROST);

        you.skills[SK_CONJURATIONS] = 1;
        you.skills[SK_ICE_MAGIC]    = 3;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_AIR_ELEMENTALIST:
        newgame_make_item(OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(OBJ_BOOKS, BOOK_AIR);

        you.skills[SK_CONJURATIONS] = 1;
        you.skills[SK_AIR_MAGIC]    = 3;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_EARTH_ELEMENTALIST:
        newgame_make_item(OBJ_MISSILES, MI_STONE, 20);
        newgame_make_item(OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(OBJ_BOOKS, BOOK_GEOMANCY);

        you.skills[SK_TRANSMUTATIONS] = 1;
        you.skills[SK_EARTH_MAGIC]    = 3;
        you.skills[SK_SPELLCASTING]   = 2;
        you.skills[SK_DODGING]        = 2;
        you.skills[SK_STEALTH]        = 2;
        break;

    case JOB_VENOM_MAGE:
        newgame_make_item(OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(OBJ_BOOKS, BOOK_YOUNG_POISONERS);

        you.skills[SK_POISON_MAGIC] = 4;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_ASSASSIN:
        newgame_make_item(OBJ_WEAPONS, WPN_DAGGER, 1, +2);
        newgame_make_item(OBJ_WEAPONS, WPN_BLOWGUN);

        newgame_make_item(OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(OBJ_ARMOUR, ARM_CLOAK);

        newgame_make_item(OBJ_MISSILES, MI_NEEDLE, 8, 0, SPMSL_POISONED);
        newgame_make_item(OBJ_MISSILES, MI_NEEDLE, 2, 0, SPMSL_CURARE);

        weap_skill = 2;
        you.skills[SK_FIGHTING]     = 2;
        you.skills[SK_DODGING]      = 1;
        you.skills[SK_STEALTH]      = 4;
        you.skills[SK_THROWING]     = 2;
        break;

    case JOB_HUNTER:
        newgame_make_item(OBJ_WEAPONS, WPN_SHORT_SWORD);
        _give_ranged_weapon(ng.weapon, 1, 4);

        newgame_make_item(OBJ_ARMOUR, ARM_LEATHER_ARMOUR);

        // Skills.
        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_DODGING]  = 2;
        you.skills[SK_STEALTH]  = 1;
        break;

    case JOB_WANDERER:
        create_wanderer();
        break;

    case JOB_ARTIFICER:
        // Equipment. Short sword, wands, and armour or robe.
        newgame_make_item(OBJ_WEAPONS, WPN_SHORT_SWORD);

        newgame_make_item(OBJ_WANDS, WAND_FLAME, 1, 15);
        newgame_make_item(OBJ_WANDS, WAND_ENSLAVEMENT, 1, 15);
        newgame_make_item(OBJ_WANDS, WAND_RANDOM_EFFECTS, 1, 15);

        newgame_make_item(OBJ_ARMOUR, ARM_LEATHER_ARMOUR);

        // Skills
        you.skills[SK_EVOCATIONS]  = 3;
        you.skills[SK_DODGING]     = 2;
        you.skills[SK_FIGHTING]    = 1;
        weap_skill                 = 1;
        you.skills[SK_STEALTH]     = 1;
        break;

    default:
        break;
    }

    // Deep Dwarves get a wand of heal wounds (5).
    if (you.species == SP_DEEP_DWARF)
        newgame_make_item(OBJ_WANDS, WAND_HEAL_WOUNDS, 1, 5);

    // Zotdef: everyone gets bonus two potions of curing.

    if (crawl_state.game_is_zotdef())
        newgame_make_item(OBJ_POTIONS, POT_CURING, 2);

    if (weap_skill)
    {
        item_def *weap = you.weapon();
        if (!weap)
            you.skills[SK_UNARMED_COMBAT] = weap_skill;
        else
            you.skills[item_attack_skill(*weap)] = weap_skill;
    }

    if (you.species == SP_FELID)
    {
        you.skills[SK_THROWING] = 0;
        you.skills[SK_SHIELDS] = 0;
    }

    if (!you_worship(GOD_NO_GOD))
    {
        you.worshipped[you.religion] = 1;
        set_god_ability_slots();
        if (!you_worship(GOD_XOM))
            you.piety_max[you.religion] = you.piety;
    }
}

static void _give_starting_food()
{
    // No food for those who don't need it.
    if (you_foodless())
        return;

    object_class_type base_type = OBJ_FOOD;
    int sub_type = FOOD_BREAD_RATION;
    int quantity = 1;
    if (you.species == SP_VAMPIRE)
    {
        base_type = OBJ_POTIONS;
        sub_type  = POT_BLOOD;
    }
    else if (player_mutation_level(MUT_CARNIVOROUS))
        sub_type = FOOD_MEAT_RATION;

    // Give another one for hungry species.
    if (player_mutation_level(MUT_FAST_METABOLISM))
        quantity = 2;

    newgame_make_item(base_type, sub_type, quantity);
}

static void _setup_tutorial_miscs()
{
    // Allow for a few specific hint mode messages.
    // A few more will be initialised by the tutorial map.
    tutorial_init_hints();

    // No gold to begin with.
    you.gold = 0;

    // Give them some mana to play around with.
    you.mp_max_adj += 2;

    newgame_make_item(OBJ_ARMOUR, ARM_ROBE, 1, 0, 0, true);

    // No need for Shields skill without shield.
    you.skills[SK_SHIELDS] = 0;

    // Some spellcasting for the magic tutorial.
    if (crawl_state.map.find("tutorial_lesson4") != string::npos)
        you.skills[SK_SPELLCASTING] = 1;
}

static void _give_basic_knowledge()
{
    identify_inventory();

    for (const item_def& i : you.inv)
        if (i.base_type == OBJ_BOOKS)
            mark_had_book(i);

    // Recognisable by appearance.
    you.type_ids[OBJ_POTIONS][POT_BLOOD] = ID_KNOWN_TYPE;
#if TAG_MAJOR_VERSION == 34
    you.type_ids[OBJ_POTIONS][POT_BLOOD_COAGULATED] = ID_KNOWN_TYPE;
    you.type_ids[OBJ_POTIONS][POT_PORRIDGE] = ID_KNOWN_TYPE;
#endif

    // Won't appear unidentified anywhere.
    you.type_ids[OBJ_SCROLLS][SCR_CURSE_WEAPON] = ID_KNOWN_TYPE;
    you.type_ids[OBJ_SCROLLS][SCR_CURSE_JEWELLERY] = ID_KNOWN_TYPE;
    you.type_ids[OBJ_SCROLLS][SCR_CURSE_ARMOUR] = ID_KNOWN_TYPE;
}

static void _setup_normal_game();
static void _setup_tutorial(const newgame_def& ng);
static void _setup_sprint(const newgame_def& ng);
static void _setup_zotdef(const newgame_def& ng);
static void _setup_hints();
static void _setup_generic(const newgame_def& ng);

// Initialise a game based on the choice stored in ng.
void setup_game(const newgame_def& ng)
{
    crawl_state.type = ng.type;
    crawl_state.map  = ng.map;

    switch (crawl_state.type)
    {
    case GAME_TYPE_NORMAL:
        _setup_normal_game();
        break;
    case GAME_TYPE_TUTORIAL:
        _setup_tutorial(ng);
        break;
    case GAME_TYPE_SPRINT:
        _setup_sprint(ng);
        break;
    case GAME_TYPE_ZOTDEF:
        _setup_zotdef(ng);
        break;
    case GAME_TYPE_HINTS:
        _setup_hints();
        break;
    case GAME_TYPE_ARENA:
    default:
        die("Bad game type");
        end(-1);
    }

    _setup_generic(ng);
}

/**
 * Special steps that normal game needs;
 */
static void _setup_normal_game()
{
    make_hungry(0, true);
}

/**
 * Special steps that tutorial game needs;
 */
static void _setup_tutorial(const newgame_def& ng)
{
    make_hungry(0, true);
}

/**
 * Special steps that sprint needs;
 */
static void _setup_sprint(const newgame_def& ng)
{
    // nothing currently
}

/**
 * Special steps that zotdef needs;
 */
static void _setup_zotdef(const newgame_def& ng)
{
    // nothing currently
}

/**
 * Special steps that hints mode needs;
 */
static void _setup_hints()
{
    init_hints();
}

static void _setup_generic(const newgame_def& ng)
{
    _init_player();

    you.your_name  = ng.name;
    you.species    = ng.species;
    you.char_class = ng.job;

    you.chr_class_name = get_job_name(you.char_class);

    species_stat_init(you.species);     // must be down here {dlb}

    // Before we get into the inventory init, set light radius based
    // on species vision. Currently, all species see out to 8 squares.
    update_vision_range();

    _jobs_stat_init(you.char_class);

    _unfocus_stats();

    // Needs to be done before handing out food.
    give_basic_mutations(you.species);

    // This function depends on stats and mutations being finalised.
    _give_items_skills(ng);

    if (you.species == SP_DEMONSPAWN)
        roll_demonspawn_mutations();

    _give_starting_food();

    if (crawl_state.game_is_sprint() || crawl_state.game_is_zotdef())
        _give_bonus_items();

    // Give tutorial skills etc
    if (crawl_state.game_is_tutorial())
        _setup_tutorial_miscs();

    _give_basic_knowledge();

    initialise_item_descriptions();

    // A first pass to link the items properly.
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (!you.inv[i].defined())
            continue;
        you.inv[i].pos = ITEM_IN_INVENTORY;
        you.inv[i].link = i;
        you.inv[i].slot = index_to_letter(you.inv[i].link);
        item_colour(you.inv[i]);  // set correct special and colour
    }

    // A second pass to apply item_slot option.
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (!you.inv[i].defined())
            continue;
        if (!you.inv[i].props.exists("adjusted"))
        {
            you.inv[i].props["adjusted"] = true;
            auto_assign_item_slot(you.inv[i]);
        }
    }

    reassess_starting_skills();
    init_skill_order();
    init_can_train();
    init_train();
    init_training();

    if (crawl_state.game_is_zotdef())
    {
        you.zot_points = 80;

        // There's little sense in training these skills in ZotDef
        you.train[SK_STEALTH] = 0;
    }

    // Apply autoinscribe rules to inventory.
    request_autoinscribe();
    autoinscribe();

    // We calculate hp and mp here; all relevant factors should be
    // finalised by now. (GDL)
    calc_hp();
    calc_mp();

    // Make sure the starting player is fully charged up.
    set_hp(you.hp_max);
    set_mp(you.max_magic_points);

    initialise_branch_depths();
    initialise_temples();
    init_level_connectivity();

    // Generate the second name of Jiyva
    fix_up_jiyva_name();

    // Get rid of god companions left from previous games
    init_companions();

    // Create the save file.
    if (Options.no_save)
        you.save = new package();
    else
        you.save = new package(get_savedir_filename(you.your_name).c_str(),
                               true, true);
}
