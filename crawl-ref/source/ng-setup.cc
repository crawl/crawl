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

static void _newgame_give_item(object_class_type base, int sub_type,
                               int qty = 1, int plus = 0);

static void _init_player()
{
    you.init();
    dlua.callfn("dgn_clear_data", "");
}

static void _species_stat_init(species_type which_species)
{
    int s = 8; // strength
    int i = 8; // intelligence
    int d = 8; // dexterity

    // Note: The stats in in this list aren't intended to sum the same
    // for all races.  The fact that Mummies and Ghouls are really low
    // is considered acceptable (Mummies don't have to eat, and Ghouls
    // are supposed to be a really hard race). - bwr
    switch (which_species)
    {
    default:                    s =  8; i =  8; d =  8;      break;  // 24
    case SP_HUMAN:              s =  8; i =  8; d =  8;      break;  // 24
    case SP_DEMIGOD:            s = 11; i = 12; d = 11;      break;  // 34
    case SP_DEMONSPAWN:         s =  8; i =  9; d =  8;      break;  // 25

    case SP_HIGH_ELF:           s =  7; i = 11; d = 10;      break;  // 28
    case SP_DEEP_ELF:           s =  5; i = 12; d = 10;      break;  // 27

    case SP_DEEP_DWARF:         s = 11; i =  8; d =  8;      break;  // 27

    case SP_TROLL:              s = 15; i =  4; d =  5;      break;  // 24
    case SP_OGRE:               s = 12; i =  7; d =  5;      break;  // 24

    case SP_MINOTAUR:           s = 12; i =  5; d =  5;      break;  // 22
    case SP_GARGOYLE:           s = 11; i =  8; d =  5;      break;  // 24
    case SP_HILL_ORC:           s = 10; i =  8; d =  6;      break;  // 24
#if TAG_MAJOR_VERSION == 34
    case SP_LAVA_ORC:           s = 10; i =  8; d =  6;      break;  // 24
#endif
    case SP_CENTAUR:            s = 10; i =  7; d =  4;      break;  // 21
    case SP_NAGA:               s = 10; i =  8; d =  6;      break;  // 24

    case SP_MERFOLK:            s =  8; i =  7; d =  9;      break;  // 24
    case SP_TENGU:              s =  8; i =  8; d =  9;      break;  // 25
    case SP_FORMICID:           s = 12; i =  7; d =  6;      break;  // 25
    case SP_VINE_STALKER:       s = 10; i =  8; d =  9;      break;  // 27

    case SP_KOBOLD:             s =  6; i =  6; d = 11;      break;  // 23
    case SP_HALFLING:           s =  8; i =  7; d =  9;      break;  // 24
    case SP_SPRIGGAN:           s =  4; i =  9; d = 11;      break;  // 24

    case SP_MUMMY:              s = 11; i =  7; d =  7;      break;  // 25
    case SP_GHOUL:              s = 11; i =  3; d =  4;      break;  // 18
    case SP_VAMPIRE:            s =  7; i = 10; d =  9;      break;  // 26

    case SP_RED_DRACONIAN:
    case SP_WHITE_DRACONIAN:
    case SP_GREEN_DRACONIAN:
    case SP_YELLOW_DRACONIAN:
    case SP_GREY_DRACONIAN:
    case SP_BLACK_DRACONIAN:
    case SP_PURPLE_DRACONIAN:
    case SP_MOTTLED_DRACONIAN:
    case SP_PALE_DRACONIAN:
    case SP_BASE_DRACONIAN:     s = 10; i =  8; d =  6;      break;  // 24

    case SP_FELID:              s =  4; i =  9; d = 11;      break;  // 24
    case SP_OCTOPODE:           s =  7; i = 10; d =  7;      break;  // 24
    }

    you.base_stats[STAT_STR] = s;
    you.base_stats[STAT_INT] = i;
    you.base_stats[STAT_DEX] = d;
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
void unfocus_stats()
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
    _newgame_give_item(OBJ_POTIONS, POT_CURING);
    _newgame_give_item(OBJ_POTIONS, POT_HEAL_WOUNDS);
    _newgame_give_item(OBJ_POTIONS, POT_HASTE);
    _newgame_give_item(OBJ_POTIONS, POT_MAGIC, 2);
    _newgame_give_item(OBJ_POTIONS, POT_BERSERK_RAGE);
    _newgame_give_item(OBJ_SCROLLS, SCR_BLINKING);
}

void autopickup_starting_ammo(missile_type missile)
{
    if (Options.autopickup_starting_ammo)
        you.force_autopickup[OBJ_MISSILES][missile] = 1;
}

void give_basic_mutations(species_type speci)
{
    switch (speci)
    {
#if TAG_MAJOR_VERSION == 34
    case SP_LAVA_ORC:
        you.mutation[MUT_CONSERVE_SCROLLS] = 1;
        break;
#endif
    case SP_OGRE:
        you.mutation[MUT_TOUGH_SKIN]      = 1;
        break;
    case SP_HALFLING:
        you.mutation[MUT_MUTATION_RESISTANCE] = 1;
        break;
    case SP_MINOTAUR:
        you.mutation[MUT_HORNS]  = 2;
        break;
    case SP_DEMIGOD:
        you.mutation[MUT_SUSTAIN_ABILITIES] = 1;
        break;
    case SP_SPRIGGAN:
        you.mutation[MUT_ACUTE_VISION]    = 1;
        you.mutation[MUT_FAST]            = 3;
        you.mutation[MUT_HERBIVOROUS]     = 3;
        you.mutation[MUT_SLOW_METABOLISM] = 2;
        break;
    case SP_CENTAUR:
        you.mutation[MUT_TOUGH_SKIN]      = 3;
        you.mutation[MUT_FAST]            = 2;
        you.mutation[MUT_DEFORMED]        = 1;
        you.mutation[MUT_HOOVES]          = 3;
        break;
    case SP_NAGA:
        you.mutation[MUT_ACUTE_VISION]      = 1;
        you.mutation[MUT_POISON_RESISTANCE] = 1;
        you.mutation[MUT_DEFORMED]          = 1;
        you.mutation[MUT_SLOW]              = 2;
        break;
    case SP_MUMMY:
        you.mutation[MUT_TORMENT_RESISTANCE]         = 1;
        you.mutation[MUT_POISON_RESISTANCE]          = 1;
        you.mutation[MUT_COLD_RESISTANCE]            = 1;
        you.mutation[MUT_NEGATIVE_ENERGY_RESISTANCE] = 3;
        break;
    case SP_DEEP_DWARF:
        you.mutation[MUT_SLOW_HEALING]    = 3;
        you.mutation[MUT_PASSIVE_MAPPING] = 1;
        break;
    case SP_GHOUL:
        you.mutation[MUT_TORMENT_RESISTANCE]         = 1;
        you.mutation[MUT_POISON_RESISTANCE]          = 1;
        you.mutation[MUT_COLD_RESISTANCE]            = 1;
        you.mutation[MUT_NEGATIVE_ENERGY_RESISTANCE] = 3;
        you.mutation[MUT_CARNIVOROUS]                = 3;
        you.mutation[MUT_SLOW_HEALING]               = 1;
        break;
    case SP_GARGOYLE:
        you.mutation[MUT_PETRIFICATION_RESISTANCE]   = 1;
        you.mutation[MUT_NEGATIVE_ENERGY_RESISTANCE] = 1;
        you.mutation[MUT_SHOCK_RESISTANCE]           = 1;
        you.mutation[MUT_ROT_IMMUNITY]               = 1;
        break;
    case SP_TENGU:
        you.mutation[MUT_BEAK]   = 1;
        you.mutation[MUT_TALONS] = 3;
        break;
    case SP_TROLL:
        you.mutation[MUT_TOUGH_SKIN]      = 2;
        you.mutation[MUT_REGENERATION]    = 2;
        you.mutation[MUT_FAST_METABOLISM] = 3;
        you.mutation[MUT_GOURMAND]        = 1;
        you.mutation[MUT_SHAGGY_FUR]      = 1;
        break;
    case SP_KOBOLD:
        you.mutation[MUT_CARNIVOROUS] = 3;
        break;
    case SP_VAMPIRE:
        you.mutation[MUT_FANGS]        = 3;
        you.mutation[MUT_ACUTE_VISION] = 1;
        break;
    case SP_FELID:
        you.mutation[MUT_FANGS]           = 3;
        you.mutation[MUT_SHAGGY_FUR]      = 1;
        you.mutation[MUT_ACUTE_VISION]    = 1;
        you.mutation[MUT_FAST]            = 1;
        you.mutation[MUT_CARNIVOROUS]     = 3;
        you.mutation[MUT_SLOW_METABOLISM] = 1;
        you.mutation[MUT_PAWS]            = 1;
        break;
    case SP_OCTOPODE:
        you.mutation[MUT_CAMOUFLAGE]      = 1;
        you.mutation[MUT_GELATINOUS_BODY] = 1;
        break;
    case SP_FORMICID:
        you.mutation[MUT_ANTENNAE]    = 3;
        break;
#if TAG_MAJOR_VERSION == 34
    case SP_DJINNI:
        you.mutation[MUT_NEGATIVE_ENERGY_RESISTANCE] = 3;
        break;
#endif
    case SP_VINE_STALKER:
        you.mutation[MUT_FANGS]          = 2;
        you.mutation[MUT_ANTIMAGIC_BITE] = 1;
        you.mutation[MUT_REGENERATION]   = 1;
        you.mutation[MUT_MANA_SHIELD]    = 1;
        you.mutation[MUT_NO_DEVICE_HEAL] = 3;
        you.mutation[MUT_ROT_IMMUNITY]   = 1;
        break;
    default:
        break;
    }

    // Some mutations out-sourced because they're
    // relevant during character choice.
    you.mutation[MUT_CLAWS] = species_has_claws(speci, true);
    you.mutation[MUT_UNBREATHING] = species_is_unbreathing(speci);

    // Necessary mostly for wizmode race changing.
    you.mutation[MUT_COLD_BLOODED] = species_genus(speci) == GENPC_DRACONIAN;

    // Starting mutations are unremovable.
    for (int i = 0; i < NUM_MUTATIONS; ++i)
        you.innate_mutation[i] = you.mutation[i];
}

static void _newgame_make_item_tutorial(int slot, equipment_type eqslot,
                                 object_class_type base,
                                 int sub_type, int replacement = -1,
                                 int qty = 1, int plus = 0)
{
    newgame_make_item(slot, eqslot, base, sub_type, replacement, qty, plus,
                      true);
}

// Creates an item of a given base and sub type.
// replacement is used when handing out armour that is not wearable for
// some species; otherwise use -1.
void newgame_make_item(int slot, equipment_type eqslot,
                       object_class_type base,
                       int sub_type, int replacement,
                       int qty, int plus, bool force_tutorial)
{
    // Don't set normal equipment in the tutorial.
    if (!force_tutorial && crawl_state.game_is_tutorial())
        return;

    if (slot == -1)
    {
        // If another of the item type is already there, add to the
        // stack instead.
        for (int i = 0; i < ENDOFPACK; ++i)
        {
            item_def& item = you.inv[i];
            if (item.is_type(base, sub_type) && is_stackable_item(item))
            {
                item.quantity += qty;
                return;
            }
        }

        for (int i = 0; i < ENDOFPACK; ++i)
        {
            if (!you.inv[i].defined())
            {
                slot = i;
                break;
            }
        }
        ASSERT(slot != -1);
    }

    item_def &item(you.inv[slot]);
    item.base_type = base;
    item.sub_type  = sub_type;
    item.quantity  = qty;
    item.plus      = plus;
    item.special   = 0;

    if (is_deck(item))
    {
        item.plus = 6 + random2(6); // # of cards
        item.special = DECK_RARITY_COMMON;
        init_deck(item);
    }

    // If the character is restricted in wearing armour of equipment
    // slot eqslot, hand out replacement instead.
    if (item.base_type == OBJ_ARMOUR && replacement != -1
        && !can_wear_armour(item, false, false))
    {
        item.sub_type = replacement;
    }
    if (eqslot == EQ_WEAPON && replacement != -1
        && !can_wield(&item, false, false))
    {
        item.sub_type = replacement;
    }

    if (item.base_type == OBJ_ARMOUR && !can_wear_armour(item, false, false))
        return;

    if (is_shield(item) && you.weapon()
        && is_shield_incompatible(*you.weapon(), &item))
    {
        return;
    }

    if (eqslot == EQ_WEAPON && !can_wield(&item, false, false))
        return;

    if (eqslot != EQ_NONE && you.equip[eqslot] == -1)
        you.equip[eqslot] = slot;
}

static void _newgame_give_item(object_class_type base, int sub_type,
                               int qty, int plus)
{
    newgame_make_item(-1, EQ_NONE, base, sub_type, -1, qty, plus);
}

static void _newgame_clear_item(int slot)
{
    you.inv[slot] = item_def();

    for (int i = EQ_WEAPON; i < NUM_EQUIP; ++i)
        if (you.equip[i] == slot)
            you.equip[i] = -1;
}

static void _update_weapon(const newgame_def& ng)
{
    ASSERT(ng.weapon != NUM_WEAPONS);

    const int plus = you.char_class == JOB_HUNTER ? 1 : 0;

    switch (ng.weapon)
    {
    case WPN_THROWN:
        if (species_can_throw_large_rocks(ng.species))
        {
            newgame_make_item(1, EQ_NONE, OBJ_MISSILES, MI_LARGE_ROCK, -1,
                              4 + plus);
            newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_THROWING_NET, -1, 2);
            autopickup_starting_ammo(MI_LARGE_ROCK);
            autopickup_starting_ammo(MI_THROWING_NET);
        }
        else if (species_size(ng.species, PSIZE_TORSO) <= SIZE_SMALL)
        {
            newgame_make_item(1, EQ_NONE, OBJ_MISSILES, MI_TOMAHAWK, -1,
                              8 + 2 * plus);
            newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_THROWING_NET, -1, 2);
            autopickup_starting_ammo(MI_TOMAHAWK);
            autopickup_starting_ammo(MI_THROWING_NET);
        }
        else
        {
            newgame_make_item(1, EQ_NONE, OBJ_MISSILES, MI_JAVELIN, -1,
                              5 + plus);
            newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_THROWING_NET, -1, 2);
            autopickup_starting_ammo(MI_JAVELIN);
            autopickup_starting_ammo(MI_THROWING_NET);
        }
        break;
    case WPN_SHORTBOW:
        newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_SHORTBOW, -1, 1, plus);
        newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_ARROW, -1, 20);
        autopickup_starting_ammo(MI_ARROW);

        // Wield the bow instead.
        you.equip[EQ_WEAPON] = 1;
        break;
    case WPN_HAND_CROSSBOW:
        newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_HAND_CROSSBOW, -1, 1,
                          plus);
        newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_BOLT, -1, 20);
        autopickup_starting_ammo(MI_BOLT);

        // Wield the crossbow instead.
        you.equip[EQ_WEAPON] = 1;
        break;
    case WPN_HUNTING_SLING:
        newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_HUNTING_SLING, -1, 1,
                          plus);
        newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_SLING_BULLET, -1, 20);
        autopickup_starting_ammo(MI_SLING_BULLET);
        autopickup_starting_ammo(MI_STONE);


        // Wield the sling instead.
        you.equip[EQ_WEAPON] = 1;
        break;
    case WPN_UNARMED:
        _newgame_clear_item(0);
        break;
    case WPN_UNKNOWN:
        break;
    default:
        you.inv[0].sub_type = ng.weapon;
    }
}

static void _give_items_skills(const newgame_def& ng)
{
    int weap_skill = 0;

    switch (you.char_class)
    {
    case JOB_FIGHTER:
        // Equipment.
        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);
        _update_weapon(ng);

        if (player_genus(GENPC_DRACONIAN))
        {
            newgame_make_item(1, EQ_GLOVES, OBJ_ARMOUR, ARM_GLOVES);
            newgame_make_item(3, EQ_BOOTS, OBJ_ARMOUR, ARM_BOOTS);
        }
        else
        {
            newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_SCALE_MAIL,
                              ARM_ROBE);
        }
        newgame_make_item(2, EQ_SHIELD, OBJ_ARMOUR,
                          you.body_size() >= SIZE_MEDIUM ? ARM_SHIELD
                                                         : ARM_BUCKLER);
        newgame_make_item(4, EQ_NONE, OBJ_POTIONS, POT_MIGHT);

        // Skills.
        you.skills[SK_FIGHTING] = 3;
        you.skills[SK_SHIELDS]  = 3;
        you.skills[SK_ARMOUR]   = 3;

        weap_skill = (you.species == SP_FELID) ? 4 : 2;

        break;

    case JOB_GLADIATOR:
        // Equipment.
        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);
        _update_weapon(ng);

        newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                           ARM_ANIMAL_SKIN);
        newgame_make_item(2, EQ_HELMET, OBJ_ARMOUR, ARM_HELMET, ARM_HAT);
        newgame_make_item(3, EQ_NONE, OBJ_MISSILES, MI_THROWING_NET, -1, 3);
        autopickup_starting_ammo(MI_THROWING_NET);

        // Skills.
        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_THROWING] = 2;
        you.skills[SK_DODGING]  = 3;
        weap_skill = 3;
        break;

    case JOB_MONK:
        you.equip[EQ_WEAPON] = -1; // Monks fight unarmed.

        newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);

        you.skills[SK_FIGHTING]       = 3;
        you.skills[SK_UNARMED_COMBAT] = 4;
        you.skills[SK_DODGING]        = 3;
        you.skills[SK_STEALTH]        = 2;
        break;

    case JOB_BERSERKER:
        you.religion = GOD_TROG;
        you.piety = 35;

        // WEAPONS
        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);
        _update_weapon(ng);

        // ARMOUR
        newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ANIMAL_SKIN);

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

        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);
        set_item_ego_type(you.inv[0], OBJ_WEAPONS, SPWPN_CHAOS);
        _update_weapon(ng);

        newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                           ARM_ROBE, 1, 2);

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

        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD, -1, 1, +1);
        _update_weapon(ng);

        newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                              ARM_ROBE);

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
        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD, -1, 1, +0);
        _update_weapon(ng);

        newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                           ARM_ROBE);
        newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_BATTLE);

        you.skills[SK_FIGHTING]     = 2;
        you.skills[SK_ARMOUR]       = 1;
        you.skills[SK_DODGING]      = 1;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_CHARMS]       = 3;
        weap_skill = 2;
        break;

    case JOB_WARPER:
        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);
        _update_weapon(ng);

        newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                           ARM_ROBE);
        newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_SPATIAL_TRANSLOCATIONS);

        // One free escape.
        newgame_make_item(3, EQ_NONE, OBJ_SCROLLS, SCR_BLINKING);
        newgame_make_item(4, EQ_NONE, OBJ_MISSILES, MI_TOMAHAWK, -1, 5);
        set_item_ego_type(you.inv[4], OBJ_MISSILES, SPMSL_DISPERSAL);

        // Felids can't ever use tomahawks, so don't put them on autopickup.
        // The items we just gave will be destroyed later in _setup_generic.
        if (!is_useless_item(you.inv[4]))
            autopickup_starting_ammo(MI_TOMAHAWK);

        you.skills[SK_FIGHTING]       = 2;
        you.skills[SK_ARMOUR]         = 1;
        you.skills[SK_DODGING]        = 2;
        you.skills[SK_SPELLCASTING]   = 2;
        you.skills[SK_TRANSLOCATIONS] = 3;
        you.skills[SK_THROWING]       = 1;
        weap_skill = 2;
    break;

    case JOB_ARCANE_MARKSMAN:
        newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        _update_weapon(ng);

        // And give them a book
        newgame_make_item(3, EQ_NONE, OBJ_BOOKS, BOOK_DEBILITATION);

        you.skills[SK_FIGHTING]                   = 1;
        you.skills[item_attack_skill(you.inv[1])] = 2;
        you.skills[SK_DODGING]                    = 2;
        you.skills[SK_SPELLCASTING]               = 1;
        you.skills[SK_HEXES]                      = 3;
        break;

    case JOB_WIZARD:
        newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(1, EQ_HELMET, OBJ_ARMOUR, ARM_HAT);

        newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_MINOR_MAGIC);

        you.skills[SK_DODGING]        = 2;
        you.skills[SK_STEALTH]        = 2;
        you.skills[SK_SPELLCASTING]   = 3;
        you.skills[SK_TRANSLOCATIONS] = 1;
        you.skills[SK_CONJURATIONS]   = 1;
        you.skills[SK_SUMMONINGS]     = 1;
        break;

    case JOB_CONJURER:
        newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);

        newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_CONJURATIONS);

        you.skills[SK_CONJURATIONS] = 4;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_ENCHANTER:
        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_DAGGER, -1, 1, +1);
        newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE, -1, 1, +1);
        newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_MALEDICT);

        weap_skill = 1;
        you.skills[SK_HEXES]        = 3;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 3;
        break;

    case JOB_SUMMONER:
        newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(1, EQ_NONE, OBJ_BOOKS, BOOK_CALLINGS);

        you.skills[SK_SUMMONINGS]   = 4;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_NECROMANCER:
        newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(1, EQ_NONE, OBJ_BOOKS, BOOK_NECROMANCY);

        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_NECROMANCY]   = 4;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_TRANSMUTER:
        you.equip[EQ_WEAPON] = -1; // Transmuters fight unarmed.

        // Some sticks for sticks to snakes.
        newgame_make_item(1, EQ_NONE, OBJ_MISSILES, MI_ARROW, -1, 12);
        newgame_make_item(2, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(3, EQ_NONE, OBJ_BOOKS, BOOK_CHANGES);

        // keep picking up sticks
        autopickup_starting_ammo(MI_ARROW);

        you.skills[SK_FIGHTING]       = 1;
        you.skills[SK_UNARMED_COMBAT] = 3;
        you.skills[SK_DODGING]        = 2;
        you.skills[SK_SPELLCASTING]   = 2;
        you.skills[SK_TRANSMUTATIONS] = 2;
        break;

    case JOB_FIRE_ELEMENTALIST:
        newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(1, EQ_NONE, OBJ_BOOKS, BOOK_FLAMES);

        you.skills[SK_CONJURATIONS] = 1;
        you.skills[SK_FIRE_MAGIC]   = 3;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_ICE_ELEMENTALIST:
        newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(1, EQ_NONE, OBJ_BOOKS, BOOK_FROST);

        you.skills[SK_CONJURATIONS] = 1;
        you.skills[SK_ICE_MAGIC]    = 3;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_AIR_ELEMENTALIST:
        newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(1, EQ_NONE, OBJ_BOOKS, BOOK_AIR);

        you.skills[SK_CONJURATIONS] = 1;
        you.skills[SK_AIR_MAGIC]    = 3;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_EARTH_ELEMENTALIST:
        // stones in switch slot (b)
        newgame_make_item(1, EQ_NONE, OBJ_MISSILES, MI_STONE, -1, 20);
        newgame_make_item(2, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(3, EQ_NONE, OBJ_BOOKS, BOOK_GEOMANCY);

        // sandblast goes through a lot of stones
        autopickup_starting_ammo(MI_STONE);

        you.skills[SK_TRANSMUTATIONS] = 1;
        you.skills[SK_EARTH_MAGIC]    = 3;
        you.skills[SK_SPELLCASTING]   = 2;
        you.skills[SK_DODGING]        = 2;
        you.skills[SK_STEALTH]        = 2;
        break;

    case JOB_VENOM_MAGE:
        // Venom Mages don't need a starting weapon since acquiring a weapon
        // to poison should be easy, and Sting is *powerful*.
        newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(1, EQ_NONE, OBJ_BOOKS, BOOK_YOUNG_POISONERS);

        you.skills[SK_POISON_MAGIC] = 4;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_ASSASSIN:
        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_DAGGER, -1, 1, +2);
        newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_BLOWGUN);

        newgame_make_item(2, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(3, EQ_CLOAK, OBJ_ARMOUR, ARM_CLOAK);

        newgame_make_item(4, EQ_NONE, OBJ_MISSILES, MI_NEEDLE, -1, 8);
        set_item_ego_type(you.inv[4], OBJ_MISSILES, SPMSL_POISONED);
        newgame_make_item(5, EQ_NONE, OBJ_MISSILES, MI_NEEDLE, -1, 2);
        set_item_ego_type(you.inv[5], OBJ_MISSILES, SPMSL_CURARE);
        autopickup_starting_ammo(MI_NEEDLE);

        weap_skill = 2;
        you.skills[SK_FIGHTING]     = 2;
        you.skills[SK_DODGING]      = 1;
        you.skills[SK_STEALTH]      = 4;
        you.skills[SK_THROWING]     = 2;
        break;

    case JOB_HUNTER:
        // Equipment.
        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);
        _update_weapon(ng);

        newgame_make_item(3, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                           ARM_ANIMAL_SKIN);

        // Skills.
        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_DODGING]  = 2;
        you.skills[SK_STEALTH]  = 1;

        you.skills[item_attack_skill(you.inv[1])] = 4;
        break;

    case JOB_WANDERER:
        create_wanderer();
        break;

    case JOB_ARTIFICER:
        // Equipment. Short sword, wands, and armour or robe.
        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);

        newgame_make_item(1, EQ_NONE, OBJ_WANDS, WAND_FLAME,
                           -1, 1, 15, 0);
        newgame_make_item(2, EQ_NONE, OBJ_WANDS, WAND_ENSLAVEMENT,
                           -1, 1, 15, 0);
        newgame_make_item(3, EQ_NONE, OBJ_WANDS, WAND_RANDOM_EFFECTS,
                           -1, 1, 15, 0);

        newgame_make_item(4, EQ_BODY_ARMOUR, OBJ_ARMOUR,
                           ARM_LEATHER_ARMOUR, ARM_ROBE);

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
        newgame_make_item(-1, EQ_NONE, OBJ_WANDS, WAND_HEAL_WOUNDS, -1, 1, 5);

    // Zotdef: everyone gets bonus two potions of curing.

    if (crawl_state.game_is_zotdef())
        newgame_make_item(-1, EQ_NONE, OBJ_POTIONS, POT_CURING, -1, 2);

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
        for (int i = SK_SHORT_BLADES; i <= SK_CROSSBOWS; i++)
        {
            you.skills[SK_UNARMED_COMBAT] += you.skills[i];
            you.skills[i] = 0;
        }
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
    // These undead start with no food.
    if (you.species == SP_MUMMY || you.species == SP_GHOUL)
        return;

    item_def item;
    item.quantity = 1;
    if (you.species == SP_VAMPIRE)
    {
        item.base_type = OBJ_POTIONS;
        item.sub_type  = POT_BLOOD;
    }
    else
    {
        item.base_type = OBJ_FOOD;
        if (player_genus(GENPC_ORCISH) || you.species == SP_KOBOLD
            || you.species == SP_OGRE || you.species == SP_TROLL
            || you.species == SP_FELID)
        {
            item.sub_type = FOOD_MEAT_RATION;
        }
        else
            item.sub_type = FOOD_BREAD_RATION;
    }

    // Give another one for hungry species.
    if (player_mutation_level(MUT_FAST_METABOLISM))
        item.quantity = 2;

    const int slot = find_free_slot(item);
    you.inv[slot]  = item;       // will ASSERT if couldn't find free slot
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

    _newgame_make_item_tutorial(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);

    // No need for Shields skill without shield.
    you.skills[SK_SHIELDS] = 0;

    // Some spellcasting for the magic tutorial.
    if (crawl_state.map.find("tutorial_lesson4") != string::npos)
        you.skills[SK_SPELLCASTING] = 1;

    // Set Str low enough for the burdened tutorial.
    you.base_stats[STAT_STR] = 12;
}

static void _mark_starting_books()
{
    for (int i = 0; i < ENDOFPACK; ++i)
        if (you.inv[i].defined() && you.inv[i].base_type == OBJ_BOOKS)
            mark_had_book(you.inv[i]);
}

static void _give_basic_spells(job_type which_job)
{
    // Wanderers may or may not already have a spell. - bwr
    if (which_job == JOB_WANDERER)
        return;

    spell_type which_spell = SPELL_NO_SPELL;

    switch (which_job)
    {
    case JOB_WIZARD:
    case JOB_CONJURER:
        which_spell = SPELL_MAGIC_DART;
        break;
    case JOB_VENOM_MAGE:
        which_spell = SPELL_STING;
        break;
    case JOB_SUMMONER:
        which_spell = SPELL_SUMMON_SMALL_MAMMAL;
        break;
    case JOB_NECROMANCER:
        which_spell = SPELL_PAIN;
        break;
    case JOB_ENCHANTER:
    case JOB_ARCANE_MARKSMAN:
        which_spell = SPELL_CORONA;
        break;
    case JOB_FIRE_ELEMENTALIST:
        which_spell = SPELL_FLAME_TONGUE;
        break;
    case JOB_ICE_ELEMENTALIST:
        which_spell = SPELL_FREEZE;
        break;
    case JOB_AIR_ELEMENTALIST:
        which_spell = SPELL_SHOCK;
        break;
    case JOB_EARTH_ELEMENTALIST:
        which_spell = SPELL_SANDBLAST;
        break;
    case JOB_SKALD:
        which_spell = SPELL_INFUSION;
        break;
    case JOB_TRANSMUTER:
        which_spell = SPELL_BEASTLY_APPENDAGE;
        break;
    case JOB_WARPER:
        which_spell = SPELL_APPORTATION;
        break;

    default:
        break;
    }

    string temp;
    if (which_spell != SPELL_NO_SPELL
        && !spell_is_uncastable(which_spell, temp, false))
    {
        add_spell_to_memory(which_spell);
    }

    return;
}

static void _give_basic_knowledge(job_type which_job)
{
    identify_inventory();

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

    you.class_name = get_job_name(you.char_class);

    _species_stat_init(you.species);     // must be down here {dlb}

    // Before we get into the inventory init, set light radius based
    // on species vision. Currently, all species see out to 8 squares.
    update_vision_range();

    _jobs_stat_init(you.char_class);

    unfocus_stats();

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

    _mark_starting_books();

    _give_basic_spells(you.char_class);
    _give_basic_knowledge(you.char_class);

    // Clear known-useless items (potions for Mummies, etc).
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (you.inv[i].defined())
        {
            if (is_useless_item(you.inv[i]))
                _newgame_clear_item(i);
        }
    }

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

    // Brand items as original equipment.
    origin_set_inventory(origin_set_startequip);

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
