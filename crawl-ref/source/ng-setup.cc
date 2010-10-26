#include "AppHdr.h"

#include "ng-setup.h"

#include "abl-show.h"
#include "dungeon.h"
#include "files.h"
#include "food.h"
#include "hints.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "jobs.h"
#include "libutil.h"
#include "maps.h"
#include "misc.h"
#include "mutation.h"
#include "newgame.h"
#include "ng-init.h"
#include "ng-wanderer.h"
#include "options.h"
#include "player.h"
#include "skills.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-util.h"
#include "sprint.h"
#include "state.h"

#define MIN_START_STAT       3

static void _init_player(void)
{
    you.init();
}

// Recall that demonspawn & demigods get more later on. {dlb}
static void _species_stat_init(species_type which_species)
{
    int sb = 0; // strength base
    int ib = 0; // intelligence base
    int db = 0; // dexterity base

    // Note: The stats in in this list aren't intended to sum the same
    // for all races.  The fact that Mummies and Ghouls are really low
    // is considered acceptable (Mummies don't have to eat, and Ghouls
    // are supposed to be a really hard race). - bwr
    switch (which_species)
    {
    default:                    sb =  6; ib =  6; db =  6;      break;  // 18
    case SP_HUMAN:              sb =  6; ib =  6; db =  6;      break;  // 18
    case SP_DEMIGOD:            sb =  9; ib = 10; db =  9;      break;  // 28
    case SP_DEMONSPAWN:         sb =  6; ib =  7; db =  6;      break;  // 19

    case SP_HIGH_ELF:           sb =  5; ib =  9; db =  8;      break;  // 22
    case SP_DEEP_ELF:           sb =  3; ib = 10; db =  8;      break;  // 21
    case SP_SLUDGE_ELF:         sb =  6; ib =  7; db =  7;      break;  // 20

    case SP_MOUNTAIN_DWARF:     sb =  9; ib =  4; db =  5;      break;  // 18
    case SP_DEEP_DWARF:         sb =  9; ib =  6; db =  6;      break;  // 21

    case SP_TROLL:              sb = 13; ib =  2; db =  3;      break;  // 18
    case SP_OGRE:               sb = 10; ib =  5; db =  3;      break;  // 18

    case SP_MINOTAUR:           sb = 10; ib =  3; db =  3;      break;  // 16
    case SP_HILL_ORC:           sb =  9; ib =  3; db =  4;      break;  // 16
    case SP_CENTAUR:            sb =  8; ib =  5; db =  2;      break;  // 15
    case SP_NAGA:               sb =  8; ib =  6; db =  4;      break;  // 18

    case SP_MERFOLK:            sb =  6; ib =  5; db =  7;      break;  // 18
    case SP_KENKU:              sb =  6; ib =  6; db =  7;      break;  // 19

    case SP_KOBOLD:             sb =  5; ib =  4; db =  8;      break;  // 17
    case SP_HALFLING:           sb =  3; ib =  6; db =  9;      break;  // 18
    case SP_SPRIGGAN:           sb =  2; ib =  7; db =  9;      break;  // 18

    case SP_MUMMY:              sb =  9; ib =  5; db =  5;      break;  // 19
    case SP_GHOUL:              sb =  9; ib =  1; db =  2;      break;  // 13
    case SP_VAMPIRE:            sb =  5; ib =  8; db =  7;      break;  // 20

    case SP_RED_DRACONIAN:
    case SP_WHITE_DRACONIAN:
    case SP_GREEN_DRACONIAN:
    case SP_YELLOW_DRACONIAN:
    case SP_GREY_DRACONIAN:
    case SP_BLACK_DRACONIAN:
    case SP_PURPLE_DRACONIAN:
    case SP_MOTTLED_DRACONIAN:
    case SP_PALE_DRACONIAN:
    case SP_BASE_DRACONIAN:     sb =  9; ib =  6; db =  2;      break;  // 17

    case SP_CAT:                sb =  2; ib =  7; db =  9;      break;  // 18
    }

    you.base_stats[STAT_STR] = sb + 2;
    you.base_stats[STAT_INT] = ib + 2;
    you.base_stats[STAT_DEX] = db + 2;
}

static void _give_last_paycheck(job_type which_job)
{
    switch (which_job)
    {
    case JOB_HEALER:
        you.gold = 100;
        break;

    case JOB_WANDERER:
    case JOB_WARPER:
    case JOB_ARCANE_MARKSMAN:
    case JOB_ASSASSIN:
        you.gold = 50;
        break;

    default:
        you.gold = 20;
        break;

    case JOB_PALADIN:
    case JOB_MONK:
        you.gold = 0;
        break;
    }
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
    int hp = 0;  // HP base
    int mp = 0;  // MP base

    // Note: Wanderers are correct, they've got a challenging background. - bwr
    switch (which_job)
    {
    case JOB_FIGHTER:           s =  8; i =  0; d =  4; hp = 15; mp = 0; break;
    case JOB_BERSERKER:         s =  9; i = -1; d =  4; hp = 15; mp = 0; break;
    case JOB_GLADIATOR:         s =  7; i =  0; d =  5; hp = 14; mp = 0; break;
    case JOB_PALADIN:           s =  7; i =  2; d =  3; hp = 14; mp = 0; break;

    case JOB_CRUSADER:          s =  4; i =  4; d =  4; hp = 13; mp = 1; break;
    case JOB_CHAOS_KNIGHT:      s =  4; i =  4; d =  4; hp = 13; mp = 1; break;

    case JOB_REAVER:            s =  5; i =  5; d =  2; hp = 13; mp = 1; break;
    case JOB_HEALER:            s =  5; i =  5; d =  2; hp = 13; mp = 2; break;
    case JOB_PRIEST:            s =  5; i =  4; d =  3; hp = 12; mp = 1; break;

    case JOB_ASSASSIN:          s =  3; i =  3; d =  6; hp = 12; mp = 0; break;
    case JOB_STALKER:           s =  2; i =  4; d =  6; hp = 12; mp = 1; break;

    case JOB_HUNTER:            s =  4; i =  3; d =  5; hp = 13; mp = 0; break;
    case JOB_WARPER:            s =  3; i =  5; d =  4; hp = 12; mp = 1; break;
    case JOB_ARCANE_MARKSMAN:   s =  3; i =  5; d =  4; hp = 12; mp = 1; break;

    case JOB_MONK:              s =  3; i =  2; d =  7; hp = 13; mp = 0; break;
    case JOB_TRANSMUTER:        s =  2; i =  5; d =  5; hp = 12; mp = 1; break;

    case JOB_WIZARD:            s = -1; i = 10; d =  3; hp =  8; mp = 5; break;
    case JOB_CONJURER:          s =  0; i =  7; d =  5; hp = 10; mp = 3; break;
    case JOB_ENCHANTER:         s =  0; i =  7; d =  5; hp = 10; mp = 3; break;
    case JOB_FIRE_ELEMENTALIST: s =  0; i =  7; d =  5; hp = 10; mp = 3; break;
    case JOB_ICE_ELEMENTALIST:  s =  0; i =  7; d =  5; hp = 10; mp = 3; break;
    case JOB_AIR_ELEMENTALIST:  s =  0; i =  7; d =  5; hp = 10; mp = 3; break;
    case JOB_EARTH_ELEMENTALIST:s =  0; i =  7; d =  5; hp = 10; mp = 3; break;
    case JOB_SUMMONER:          s =  0; i =  7; d =  5; hp = 10; mp = 3; break;
    case JOB_VENOM_MAGE:        s =  0; i =  7; d =  5; hp = 10; mp = 3; break;
    case JOB_NECROMANCER:       s =  0; i =  7; d =  5; hp = 10; mp = 3; break;

    case JOB_WANDERER:
    {
        // Wanderers get their stats randomly distributed.
        _wanderer_assign_remaining_stats(12);
                                                        hp = 11; mp = 1; break;
    }

    case JOB_ARTIFICER:         s =  3; i =  4; d =  5; hp = 13; mp = 1; break;
    default:                    s =  0; i =  0; d =  0; hp = 10; mp = 0; break;
    }

    you.base_stats[STAT_STR] += s;
    you.base_stats[STAT_INT] += i;
    you.base_stats[STAT_DEX] += d;

    // Used for Jiyva's stat swapping if the player has not reached
    // experience level 3.
    you.last_chosen = (stat_type) random2(NUM_STATS);

    set_hp(hp, true);
    set_mp(mp, true);
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

void give_basic_mutations(species_type speci)
{
    // We should switch over to a size-based system
    // for the fast/slow metabolism when we get around to it.
    switch (speci)
    {
    case SP_HILL_ORC:
        you.mutation[MUT_SAPROVOROUS] = 1;
        break;
    case SP_OGRE:
        you.mutation[MUT_TOUGH_SKIN]      = 1;
        you.mutation[MUT_FAST_METABOLISM] = 1;
        you.mutation[MUT_SAPROVOROUS]     = 1;
        break;
    case SP_HALFLING:
        you.mutation[MUT_SLOW_METABOLISM] = 1;
        break;
    case SP_MINOTAUR:
        you.mutation[MUT_HORNS] = 2;
        break;
    case SP_SPRIGGAN:
        you.mutation[MUT_ACUTE_VISION]    = 1;
        you.mutation[MUT_FAST]            = 3;
        you.mutation[MUT_HERBIVOROUS]     = 3;
        you.mutation[MUT_SLOW_METABOLISM] = 3;
        break;
    case SP_CENTAUR:
        you.mutation[MUT_TOUGH_SKIN]      = 3;
        you.mutation[MUT_FAST]            = 2;
        you.mutation[MUT_DEFORMED]        = 1;
        you.mutation[MUT_FAST_METABOLISM] = 2;
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
        you.mutation[MUT_UNBREATHING]                = 1;
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
        you.mutation[MUT_SAPROVOROUS]                = 3;
        you.mutation[MUT_CARNIVOROUS]                = 3;
        you.mutation[MUT_SLOW_HEALING]               = 1;
        you.mutation[MUT_UNBREATHING]                = 1;
        break;
    case SP_KENKU:
        you.mutation[MUT_BEAK]   = 1;
        you.mutation[MUT_TALONS] = 3;
        break;
    case SP_TROLL:
        you.mutation[MUT_TOUGH_SKIN]      = 2;
        you.mutation[MUT_REGENERATION]    = 2;
        you.mutation[MUT_FAST_METABOLISM] = 3;
        you.mutation[MUT_SAPROVOROUS]     = 2;
        you.mutation[MUT_GOURMAND]        = 1;
        you.mutation[MUT_SHAGGY_FUR]      = 1;
        break;
    case SP_KOBOLD:
        you.mutation[MUT_SAPROVOROUS] = 2;
        you.mutation[MUT_CARNIVOROUS] = 3;
        break;
    case SP_VAMPIRE:
        you.mutation[MUT_FANGS]        = 3;
        you.mutation[MUT_ACUTE_VISION] = 1;
        you.mutation[MUT_UNBREATHING]  = 1;
        break;
    case SP_CAT:
        you.mutation[MUT_FANGS]           = 3;
        you.mutation[MUT_SHAGGY_FUR]      = 1;
        you.mutation[MUT_ACUTE_VISION]    = 1;
        you.mutation[MUT_FAST]            = 1;
        you.mutation[MUT_CARNIVOROUS]     = 3;
        you.mutation[MUT_SLOW_METABOLISM] = 2;
        break;
    default:
        break;
    }

    // Some mutations out-sourced because they're
    // relevant during character choice.
    you.mutation[MUT_CLAWS] = species_has_claws(speci, true);

    // Starting mutations are unremovable.
    for (int i = 0; i < NUM_MUTATIONS; ++i)
        you.innate_mutations[i] = you.mutation[i];
}

static void _newgame_make_item_tutorial(int slot, equipment_type eqslot,
                                 object_class_type base,
                                 int sub_type, int replacement = -1,
                                 int qty = 1, int plus = 0, int plus2 = 0)
{
    newgame_make_item(slot, eqslot, base, sub_type, replacement, qty, plus,
                      plus2, true);
}

// Creates an item of a given base and sub type.
// replacement is used when handing out armour that is not wearable for
// some species; otherwise use -1.
void newgame_make_item(int slot, equipment_type eqslot,
                       object_class_type base,
                       int sub_type, int replacement,
                       int qty, int plus, int plus2, bool force_tutorial)
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
            if (item.base_type == base && item.sub_type == sub_type
                && is_stackable_item(item))
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
    item.plus2     = plus2;
    item.special   = 0;

    // If the character is restricted in wearing armour of equipment
    // slot eqslot, hand out replacement instead.
    if (item.base_type == OBJ_ARMOUR && replacement != -1
        && !you_can_wear(eqslot))
    {
        // Don't replace shields with bucklers for large races or
        // draconians.
        if (sub_type != ARM_SHIELD
            || you.body_size(PSIZE_TORSO) < SIZE_LARGE
               && !player_genus(GENPC_DRACONIAN))
        {
            item.sub_type = replacement;
        }
    }

    if (eqslot != EQ_NONE && you.equip[eqslot] == -1)
        you.equip[eqslot] = slot;
}

void newgame_give_item(object_class_type base, int sub_type,
                       int qty, int plus, int plus2)
{
    newgame_make_item(-1, EQ_NONE, base, sub_type, -1, qty, plus, plus2);
}

static void _newgame_clear_item(int slot)
{
    you.inv[slot] = item_def();

    for (int i = EQ_WEAPON; i < NUM_EQUIP; ++i)
        if (you.equip[i] == slot)
            you.equip[i] = -1;
}

static void _give_wand(const newgame_def& ng)
{
    bool is_rod;
    int wand = start_to_wand(ng.wand, is_rod);
    ASSERT(wand != -1);

    if (is_rod)
        make_rod(you.inv[2], STAFF_STRIKING, 8);
    else
    {
        // 1 wand of random effects and one chosen lesser wand
        const wand_type choice = static_cast<wand_type>(wand);
        const int ncharges = 15;
        newgame_make_item(2, EQ_NONE, OBJ_WANDS, WAND_RANDOM_EFFECTS,
                           -1, 1, ncharges, 0);
        newgame_make_item(3, EQ_NONE, OBJ_WANDS, choice,
                           -1, 1, ncharges, 0);
    }
}

static void _update_weapon(const newgame_def& ng)
{
    ASSERT(ng.weapon != NUM_WEAPONS);

    if (ng.weapon == WPN_UNARMED)
        _newgame_clear_item(0);
    else
        you.inv[0].sub_type = ng.weapon;
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

        newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_SCALE_MAIL,
                          ARM_ROBE);
        newgame_make_item(2, EQ_SHIELD, OBJ_ARMOUR, ARM_SHIELD, ARM_BUCKLER);

        // Skills.
        you.skills[SK_FIGHTING] = 3;
        you.skills[SK_SHIELDS]  = 2;

        weap_skill = 2;

        you.skills[(player_effectively_in_light_armour()
                   ? SK_DODGING : SK_ARMOUR)] = 3;

        break;

    case JOB_GLADIATOR:
    {
        // Equipment.
        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);
        _update_weapon(ng);

        newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                           ARM_ANIMAL_SKIN);

        newgame_make_item(2, EQ_SHIELD, OBJ_ARMOUR, ARM_SHIELD, ARM_BUCKLER);

        int curr = 3;
        if (you_can_wear(EQ_HELMET))
        {
            newgame_make_item(3, EQ_HELMET, OBJ_ARMOUR, ARM_HELMET);
            curr++;
        }

        // Small species get stones, the others nets.
        if (you.body_size(PSIZE_BODY) < SIZE_MEDIUM)
            newgame_make_item(curr, EQ_NONE, OBJ_MISSILES, MI_STONE, -1, 20);
        else
        {
            newgame_make_item(curr, EQ_NONE, OBJ_MISSILES, MI_THROWING_NET, -1,
                               4);
        }

        // Skills.
        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_THROWING] = 2;
        you.skills[SK_DODGING]  = 2;
        you.skills[SK_SHIELDS]  = 2;
        you.skills[SK_UNARMED_COMBAT] = 2;
        weap_skill = 3;
        break;
    }

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
        if (you.has_claws())
            you.equip[EQ_WEAPON] = -1; // Trolls/Ghouls/Felids fight unarmed.
        else
        {
            // Species skilled with maces/flails get one, the others axes.
            weapon_type startwep = WPN_HAND_AXE;
            if (species_skills(SK_MACES_FLAILS, you.species) <
                species_skills(SK_AXES, you.species))
            {
                startwep = (player_genus(GENPC_OGREISH)) ? WPN_ANKUS : WPN_MACE;
            }

            newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, startwep);
        }

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
            you.skills[SK_ARMOUR] = 1; // for the eventual dragon scale mail :)
        }
        break;

    case JOB_PALADIN:
        you.religion = GOD_SHINING_ONE;
        you.piety = 28;

        // Equipment.
        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_FALCHION);
        newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_RING_MAIL,
                           ARM_ROBE);
        newgame_make_item(2, EQ_SHIELD, OBJ_ARMOUR, ARM_SHIELD, ARM_BUCKLER);
        newgame_make_item(3, EQ_NONE, OBJ_POTIONS, POT_HEALING);

        // Skills.
        you.skills[(player_effectively_in_light_armour()
                    ? SK_DODGING : SK_ARMOUR)] = 2;
        you.skills[SK_FIGHTING]    = 2;
        you.skills[SK_SHIELDS]     = 2;
        you.skills[SK_LONG_BLADES] = 3;
        you.skills[SK_INVOCATIONS] = 2;
        break;

    case JOB_PRIEST:
        you.religion = ng.religion;
        you.piety = 45;

        if (you.religion == GOD_BEOGH)
            newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_HAND_AXE);
        else
            newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_QUARTERSTAFF);

        newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);

        if (you.religion == GOD_ZIN)
            newgame_make_item(2, EQ_NONE, OBJ_POTIONS, POT_HEALING, -1, 2);

        you.skills[SK_FIGHTING]    = 2;
        you.skills[SK_INVOCATIONS] = 5;
        you.skills[SK_DODGING]     = 1;
        weap_skill = 3;
        break;

    case JOB_CHAOS_KNIGHT:
    {
        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD, -1, 1,
                           2, 2);
        _update_weapon(ng);

        you.religion = ng.religion;

        newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                           ARM_ROBE, 1, you.religion == GOD_XOM ? 2 : 0);

        you.skills[SK_FIGHTING] = 3;
        you.skills[SK_ARMOUR]   = 1;
        you.skills[SK_DODGING]  = 1;
        if (species_skills(SK_ARMOUR, you.species) >
            species_skills(SK_DODGING, you.species))
        {
            you.skills[SK_DODGING]++;
        }
        else
            you.skills[SK_ARMOUR]++;
        weap_skill = 2;

        if (you.religion == GOD_XOM)
        {
            you.skills[SK_FIGHTING]++;
            // The new (piety-aware) Xom uses piety in his own special way...
            // (Namely, 100 is neutral.)
            you.piety = 100;

            // The new Xom also uses gift_timeout in his own special way...
            // (Namely, a countdown to becoming bored.)
            you.gift_timeout = std::max(5, random2(40) + random2(40));
        }
        else // Makhleb or Lugonu
        {
            you.skills[SK_INVOCATIONS] = 2;

            if (you.religion == GOD_LUGONU)
            {
                // Chaos Knights of Lugonu start in the Abyss.  We need
                // to mark this unusual occurrence, so the player
                // doesn't get early access to OOD items, etc.
                you.char_direction = GDT_GAME_START;
                you.piety = 38;
            }
            else
                you.piety = 25;
        }
        break;
    }

    case JOB_HEALER:
        you.religion = GOD_ELYVILON;
        you.piety = 55;

        you.equip[EQ_WEAPON] = -1;

        newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(1, EQ_NONE, OBJ_POTIONS, POT_HEALING);
        newgame_make_item(2, EQ_NONE, OBJ_POTIONS, POT_HEAL_WOUNDS);

        you.skills[SK_FIGHTING]       = 2;
        you.skills[SK_DODGING]        = 1;
        you.skills[SK_INVOCATIONS]    = 4;
        break;

    case JOB_CRUSADER:
        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);
        _update_weapon(ng);

        newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                           ARM_ROBE);
        newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_WAR_CHANTS);

        you.skills[SK_FIGHTING]     = 3;
        you.skills[SK_ARMOUR]       = 1;
        you.skills[SK_DODGING]      = 1;
        you.skills[SK_SPELLCASTING] = 2;
        you.skills[SK_ENCHANTMENTS] = 2;
        weap_skill = 2;
        break;

    case JOB_REAVER:
        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);
        _update_weapon(ng);

        newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                           ARM_ROBE);
        newgame_make_item(2, EQ_NONE, OBJ_BOOKS,
                           start_to_book(BOOK_CONJURATIONS_I, ng.book));

        you.skills[SK_FIGHTING]     = 2;
        you.skills[SK_ARMOUR]       = 1;
        you.skills[SK_DODGING]      = 1;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_CONJURATIONS] = 2;
        weap_skill = 3;
        break;

    case JOB_WARPER:
        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD);
        _update_weapon(ng);

        newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                           ARM_ROBE);
        newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_SPATIAL_TRANSLOCATIONS);

        // One free escape.
        newgame_make_item(3, EQ_NONE, OBJ_SCROLLS, SCR_BLINKING);
        newgame_make_item(4, EQ_NONE, OBJ_MISSILES, MI_DART, -1, 20, 1);

        newgame_make_item(5, EQ_NONE, OBJ_MISSILES, MI_DART, -1, 5);
        set_item_ego_type(you.inv[5], OBJ_MISSILES, SPMSL_DISPERSAL);

        you.skills[SK_FIGHTING]       = 1;
        you.skills[SK_ARMOUR]         = 1;
        you.skills[SK_DODGING]        = 2;
        you.skills[SK_SPELLCASTING]   = 2;
        you.skills[SK_TRANSLOCATIONS] = 3;
        you.skills[SK_THROWING]       = 1;
        weap_skill = 3;
    break;

    case JOB_ARCANE_MARKSMAN:
        newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);

        switch (you.species)
        {
        case SP_HALFLING:
        case SP_KOBOLD:
            newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_SLING);
            newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_SLING_BULLET, -1,
                               30);

            // Wield the sling instead.
            you.equip[EQ_WEAPON] = 1;
            break;

        case SP_MOUNTAIN_DWARF:
        case SP_DEEP_DWARF:
            newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_CROSSBOW);
            newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_BOLT, -1, 25);

            // Wield the crossbow instead.
            you.equip[EQ_WEAPON] = 1;
            break;

        default:
            newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_BOW);
            newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_ARROW, -1, 25);

            // Wield the bow instead.
            you.equip[EQ_WEAPON] = 1;
            break;
        }

        // And give them a book
        newgame_make_item(3, EQ_NONE, OBJ_BOOKS, BOOK_BRANDS);

        you.skills[range_skill(you.inv[1])] = 2;
        you.skills[SK_DODGING]              = 1;
        you.skills[SK_SPELLCASTING]         = 2;
        you.skills[SK_ENCHANTMENTS]         = 2;
        break;

    case JOB_WIZARD:
        newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(1, EQ_HELMET, OBJ_ARMOUR, ARM_WIZARD_HAT);

        newgame_make_item(2, EQ_NONE, OBJ_BOOKS,
                           start_to_book(BOOK_MINOR_MAGIC_I, ng.book));

        you.skills[SK_DODGING]        = 2;
        you.skills[SK_STEALTH]        = 2;
        you.skills[SK_SPELLCASTING]   = 3;
        // All three starting books contain Translocations spells.
        you.skills[SK_TRANSLOCATIONS] = 1;

        // The other two schools depend on the chosen book.
        switch (you.inv[2].sub_type)
        {
        case BOOK_MINOR_MAGIC_I:
            you.skills[SK_CONJURATIONS] = 1;
            you.skills[SK_FIRE_MAGIC]   = 1;
            break;
        case BOOK_MINOR_MAGIC_II:
            you.skills[SK_CONJURATIONS] = 1;
            you.skills[SK_ICE_MAGIC]    = 1;
            break;
        case BOOK_MINOR_MAGIC_III:
            you.skills[SK_SUMMONINGS]   = 1;
            you.skills[SK_ENCHANTMENTS] = 1;
            break;
        }
        break;

    case JOB_CONJURER:
        newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);

        newgame_make_item(2, EQ_NONE, OBJ_BOOKS,
                           start_to_book(BOOK_CONJURATIONS_I, ng.book));

        you.skills[SK_CONJURATIONS] = 4;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_ENCHANTER:
        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_SHORT_SWORD, -1, 1, 1,
                           1);
        newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE, -1, 1, 1);
        newgame_make_item(2, EQ_NONE, OBJ_BOOKS, BOOK_CHARMS);

        // Gets some darts - this job is difficult to start off with.
        newgame_make_item(3, EQ_NONE, OBJ_MISSILES, MI_DART, -1, 16, 1);

        // Spriggans used to get a rod of striking, but now that anyone
        // can get one when playing an Artificer, this is no longer
        // necessary. (jpeg)
        if (you.species == SP_SPRIGGAN)
            you.inv[0].sub_type = WPN_DAGGER;

        if (player_genus(GENPC_OGREISH) || you.species == SP_TROLL)
            you.inv[0].sub_type = WPN_CLUB;

        weap_skill = 1;
        you.skills[SK_THROWING]     = 1;
        you.skills[SK_ENCHANTMENTS] = 4;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_SUMMONER:
        newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(1, EQ_NONE, OBJ_BOOKS, BOOK_CALLINGS);

        you.skills[SK_SUMMONINGS]   = 4;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_NECROMANCER:
        newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(1, EQ_NONE, OBJ_BOOKS, BOOK_NECROMANCY);

        you.skills[SK_SPELLCASTING] = 1;
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

        // A little bit of starting ammo for evaporate... don't need too
        // much now that the character can make their own. - bwr
        newgame_make_item(4, EQ_NONE, OBJ_POTIONS, POT_CONFUSION, -1, 2);
        newgame_make_item(5, EQ_NONE, OBJ_POTIONS, POT_POISON);

        // Spriggans used to get a rod of striking, but now that anyone
        // can get one when playing an Artificer, this is no longer
        // necessary. (jpeg)

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
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_ICE_ELEMENTALIST:
        newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(1, EQ_NONE, OBJ_BOOKS, BOOK_FROST);

        you.skills[SK_CONJURATIONS] = 1;
        you.skills[SK_ICE_MAGIC]    = 3;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_AIR_ELEMENTALIST:
        newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(1, EQ_NONE, OBJ_BOOKS, BOOK_AIR);

        you.skills[SK_CONJURATIONS] = 1;
        you.skills[SK_AIR_MAGIC]    = 3;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_EARTH_ELEMENTALIST:
        // stones in switch slot (b)
        newgame_make_item(1, EQ_NONE, OBJ_MISSILES, MI_STONE, -1, 20);
        newgame_make_item(2, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(3, EQ_NONE, OBJ_BOOKS, BOOK_GEOMANCY);

        you.skills[SK_TRANSMUTATIONS] = 1;
        you.skills[SK_EARTH_MAGIC]    = 3;
        you.skills[SK_SPELLCASTING]   = 1;
        you.skills[SK_DODGING]        = 2;
        you.skills[SK_STEALTH]        = 2;
        break;

    case JOB_VENOM_MAGE:
        // Venom Mages don't need a starting weapon since acquiring a weapon
        // to poison should be easy, and Sting is *powerful*.
        newgame_make_item(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(1, EQ_NONE, OBJ_BOOKS, BOOK_YOUNG_POISONERS);

        you.skills[SK_POISON_MAGIC] = 4;
        you.skills[SK_SPELLCASTING] = 1;
        you.skills[SK_DODGING]      = 2;
        you.skills[SK_STEALTH]      = 2;
        break;

    case JOB_STALKER:
        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_DAGGER, -1, 1, 2, 2);
        newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(2, EQ_CLOAK, OBJ_ARMOUR, ARM_CLOAK);
        newgame_make_item(3, EQ_NONE, OBJ_BOOKS, BOOK_STALKING);

        newgame_make_item(4, EQ_NONE, OBJ_POTIONS, POT_CONFUSION, -1, 2);

        if (player_genus(GENPC_OGREISH) || you.species == SP_TROLL)
            you.inv[0].sub_type = WPN_CLUB;

        weap_skill = 1;
        you.skills[SK_FIGHTING]       = 1;
        you.skills[SK_DODGING]        = 2;
        you.skills[SK_STEALTH]        = 2;
        you.skills[SK_STABBING]       = 2;
        you.skills[SK_SPELLCASTING]   = 1;
        you.skills[SK_TRANSMUTATIONS] = 1;
        break;

    case JOB_ASSASSIN:
        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_DAGGER, -1, 1, 2, 2);
        newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_BLOWGUN);

        newgame_make_item(2, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);
        newgame_make_item(3, EQ_CLOAK, OBJ_ARMOUR, ARM_CLOAK);

        newgame_make_item(4, EQ_NONE, OBJ_MISSILES, MI_NEEDLE, -1, 10);
        set_item_ego_type(you.inv[4], OBJ_MISSILES, SPMSL_POISONED);
        newgame_make_item(5, EQ_NONE, OBJ_MISSILES, MI_NEEDLE, -1, 3);
        set_item_ego_type(you.inv[5], OBJ_MISSILES, SPMSL_CURARE);

        if (you.species == SP_OGRE || you.species == SP_TROLL)
            you.inv[0].sub_type = WPN_CLUB;

        weap_skill = 2;
        you.skills[SK_FIGHTING]     = 2;
        you.skills[SK_DODGING]      = 1;
        you.skills[SK_STEALTH]      = 3;
        you.skills[SK_STABBING]     = 2;
        you.skills[SK_THROWING]     = 2;
        break;

    case JOB_HUNTER:
        // Equipment.
        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_DAGGER);

        switch (you.species)
        {
        case SP_MOUNTAIN_DWARF:
        case SP_DEEP_DWARF:
        case SP_HILL_ORC:
        case SP_CENTAUR:
            you.inv[0].sub_type = WPN_HAND_AXE;
            break;
        case SP_OGRE:
            you.inv[0].sub_type = WPN_CLUB;
            break;
        case SP_GHOUL:
        case SP_TROLL:
            _newgame_clear_item(0);
            break;
        default:
            break;
        }

        switch (you.species)
        {
        case SP_SLUDGE_ELF:
        case SP_HILL_ORC:
        case SP_MERFOLK:
            newgame_make_item(1, EQ_NONE, OBJ_MISSILES, MI_JAVELIN, -1, 6, 1);
            newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_THROWING_NET, -1,
                               2);
            break;

        case SP_OGRE:
            // Give ogres a knife for butchering, as they now start with
            // a club instead of an axe.
            newgame_make_item(4, EQ_NONE, OBJ_WEAPONS, WPN_KNIFE);
        case SP_TROLL:
            newgame_make_item(1, EQ_NONE, OBJ_MISSILES, MI_LARGE_ROCK, -1, 5,
                               1);
            newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_THROWING_NET, -1,
                               3);
            break;

        case SP_HALFLING:
        case SP_KOBOLD:
            newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_SLING);
            newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_SLING_BULLET, -1,
                               30, 1);

            // Give them a buckler, as well; sling users + bucklers is an ideal
            // combination.
            newgame_make_item(4, EQ_SHIELD, OBJ_ARMOUR, ARM_BUCKLER);
            you.skills[SK_SHIELDS] = 2;

            // Wield the sling instead.
            you.equip[EQ_WEAPON] = 1;
            break;

        case SP_MOUNTAIN_DWARF:
        case SP_DEEP_DWARF:
            newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_CROSSBOW);
            newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_BOLT, -1, 25, 1);

            // Wield the crossbow instead.
            you.equip[EQ_WEAPON] = 1;
            break;

        default:
            newgame_make_item(1, EQ_NONE, OBJ_WEAPONS, WPN_BOW);
            newgame_make_item(2, EQ_NONE, OBJ_MISSILES, MI_ARROW, -1, 25, 1);

            // Wield the bow instead.
            you.equip[EQ_WEAPON] = 1;
            break;
        }

        newgame_make_item(3, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_LEATHER_ARMOUR,
                           ARM_ANIMAL_SKIN);

        // Skills.
        you.skills[SK_FIGHTING] = 2;
        you.skills[SK_DODGING]  = 2;
        you.skills[SK_STEALTH]  = 1;
        weap_skill = 1;

        if (is_range_weapon(you.inv[1]))
            you.skills[range_skill(you.inv[1])] = 4;
        else
            you.skills[SK_THROWING] = 4;
        break;

    case JOB_WANDERER:
        create_wanderer();
        break;

    case JOB_ARTIFICER:
        // Equipment. Dagger, and armour or robe.
        newgame_make_item(0, EQ_WEAPON, OBJ_WEAPONS, WPN_DAGGER);
        newgame_make_item(1, EQ_BODY_ARMOUR, OBJ_ARMOUR,
                           ARM_LEATHER_ARMOUR, ARM_ROBE);

        // Choice of lesser wands, 15 charges plus wand of random
        // effects: confusion, enslavement, slowing, magic dart, frost,
        // flame; OR a rod of striking, 8 charges and no random effects.
        _give_wand(ng);

        // If an offensive wand or the rod of striking was chosen,
        // don't hand out a weapon.
        if (item_is_rod(you.inv[2]))
        {
            // If the rod of striking was chosen, put it in the first
            // slot and wield it.
            you.inv[0] = you.inv[2];
            you.equip[EQ_WEAPON] = 0;
            _newgame_clear_item(2);
        }
        else if (you.inv[3].base_type != OBJ_WANDS
                 || you.inv[3].sub_type != WAND_CONFUSION
                    && you.inv[3].sub_type != WAND_ENSLAVEMENT)
        {
            _newgame_clear_item(0);
        }

        // Skills
        you.skills[SK_EVOCATIONS]  = 4;
        you.skills[SK_TRAPS_DOORS] = 3;
        you.skills[SK_DODGING]     = 2;
        you.skills[SK_FIGHTING]    = 1;
        you.skills[SK_STEALTH]     = 1;
        break;

    default:
        break;
    }

    // Deep Dwarves get healing potions and wand of healing (3).
    if (you.species == SP_DEEP_DWARF)
    {
        newgame_make_item(-1, EQ_NONE, OBJ_POTIONS, POT_HEAL_WOUNDS, -1, 2);
        newgame_make_item(-1, EQ_NONE, OBJ_WANDS, WAND_HEALING, -1, 1, 3);
    }

    if (weap_skill)
    {
        if (!you.weapon())
            you.skills[SK_UNARMED_COMBAT] = weap_skill;
        else
            you.skills[weapon_skill(*you.weapon())] = weap_skill;
    }

    if (you.species == SP_CAT)
    {
        for (int i = SK_SHORT_BLADES; i <= SK_CROSSBOWS; i++)
        {
            you.skills[SK_UNARMED_COMBAT] += you.skills[i];
            you.skills[i] = 0;
        }
        you.skills[SK_DODGING] += you.skills[SK_ARMOUR];
        you.skills[SK_ARMOUR] = 0;
        you.skills[SK_THROWING] = 0;
        you.skills[SK_SHIELDS] = 0;
    }

    init_skill_order();

    if (you.religion != GOD_NO_GOD)
    {
        you.worshipped[you.religion] = 1;
        set_god_ability_slots();
    }
}

static void _give_species_bonus_hp()
{
    if (player_genus(GENPC_DRACONIAN) || player_genus(GENPC_DWARVEN))
        inc_max_hp(1);
    else
    {
        switch (you.species)
        {
        case SP_CENTAUR:
        case SP_OGRE:
        case SP_TROLL:
            inc_max_hp(3);
            break;

        case SP_GHOUL:
        case SP_MINOTAUR:
        case SP_NAGA:
        case SP_DEMIGOD:
            inc_max_hp(2);
            break;

        case SP_HILL_ORC:
        case SP_MUMMY:
        case SP_MERFOLK:
            inc_max_hp(1);
            break;

        case SP_HIGH_ELF:
        case SP_VAMPIRE:
            dec_max_hp(1);
            break;

        case SP_DEEP_ELF:
        case SP_HALFLING:
        case SP_KENKU:
        case SP_KOBOLD:
        case SP_SPRIGGAN:
            dec_max_hp(2);
            break;

        case SP_CAT:
            dec_max_hp(3);
            break;

        default:
            break;
        }
    }
}

// Adjust max_magic_points by species. {dlb}
static void _give_species_bonus_mp()
{
    switch (you.species)
    {
    case SP_VAMPIRE:
    case SP_SPRIGGAN:
    case SP_DEMIGOD:
    case SP_DEEP_ELF:
        inc_max_mp(1);
        break;

    default:
        break;
    }
}

static void _give_starting_food()
{
    // These undead start with no food.
    if (you.species == SP_MUMMY || you.species == SP_GHOUL)
        return;

    item_def item;
    item.quantity = 1;
    if (you.species == SP_SPRIGGAN)
    {
        item.base_type = OBJ_POTIONS;
        item.sub_type  = POT_PORRIDGE;
    }
    else if (you.species == SP_VAMPIRE)
    {
        item.base_type = OBJ_POTIONS;
        item.sub_type  = POT_BLOOD;
        init_stack_blood_potions(item);
    }
    else
    {
        item.base_type = OBJ_FOOD;
        if (you.species == SP_HILL_ORC || you.species == SP_KOBOLD
            || player_genus(GENPC_OGREISH) || you.species == SP_TROLL
            || you.species == SP_CAT)
        {
            item.sub_type = FOOD_MEAT_RATION;
        }
        else
            item.sub_type = FOOD_BREAD_RATION;
    }

    // Give another one for hungry species.
    // And healers, to give pacifists a better chance. [rob]
    if (player_mutation_level(MUT_FAST_METABOLISM)
        || you.char_class == JOB_HEALER)
    {
        item.quantity = 2;
    }

    const int slot = find_free_slot(item);
    you.inv[slot]  = item;       // will ASSERT if couldn't find free slot
}

static void _setup_tutorial_miscs()
{
    // Give him spellcasting
    you.skills[SK_SPELLCASTING] = 3;
    you.skills[SK_CONJURATIONS] = 1;

    // Give him some mana to play around with.
    inc_max_mp(2);

    _newgame_make_item_tutorial(0, EQ_BODY_ARMOUR, OBJ_ARMOUR, ARM_ROBE);

    // No need for Shields skill without shield.
    you.skills[SK_SHIELDS] = 0;

    // Make him hungry for the butchering tutorial.
    you.hunger = 2700;

    // Set Str low enough for the burdened tutorial.
    you.base_stats[STAT_STR] = 14;
}

static void _mark_starting_books()
{
    for (int i = 0; i < ENDOFPACK; ++i)
        if (you.inv[i].defined() && you.inv[i].base_type == OBJ_BOOKS)
            mark_had_book(you.inv[i]);
}

static void _racialise_starting_equipment()
{
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (you.inv[i].defined())
        {
            if (is_useless_item(you.inv[i]))
                _newgame_clear_item(i);
            // Don't change object type modifier unless it starts plain.
            else if ((you.inv[i].base_type == OBJ_ARMOUR
                    || you.inv[i].base_type == OBJ_WEAPONS)
                && get_equip_race(you.inv[i]) == ISFLAG_NO_RACE)
            {
                // Now add appropriate species type mod.
                // Fighters don't get elven body armour.
                if (player_genus(GENPC_ELVEN)
                    && (you.char_class != JOB_FIGHTER
                        || you.inv[i].base_type != OBJ_ARMOUR
                        || get_armour_slot(you.inv[i]) != EQ_BODY_ARMOUR))
                {
                    set_equip_race(you.inv[i], ISFLAG_ELVEN);
                }
                else if (player_genus(GENPC_DWARVEN))
                    set_equip_race(you.inv[i], ISFLAG_DWARVEN);
                else if (you.species == SP_HILL_ORC)
                    set_equip_race(you.inv[i], ISFLAG_ORCISH);
            }
        }
    }
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
        if (!you.skills[SK_CONJURATIONS])
        {
            // Wizards who start with Minor Magic III (summoning) have no
            // Conjurations skill, and thus get another starting spell.
            which_spell = SPELL_SUMMON_SMALL_MAMMALS;
            break;
        }
        // intentional fall-through
    case JOB_CONJURER:
    case JOB_REAVER:
        which_spell = SPELL_MAGIC_DART;
        break;
    case JOB_VENOM_MAGE:
        which_spell = SPELL_STING;
        break;
    case JOB_SUMMONER:
        which_spell = SPELL_SUMMON_SMALL_MAMMALS;
        break;
    case JOB_NECROMANCER:
        which_spell = SPELL_PAIN;
        break;
    case JOB_ENCHANTER:
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

    default:
        break;
    }

    if (which_spell != SPELL_NO_SPELL)
        add_spell_to_memory(which_spell);

    return;
}

// Give knowledge of things that aren't in the starting inventory.
static void _give_basic_knowledge(job_type which_job)
{
    if (you.species == SP_VAMPIRE)
        set_ident_type(OBJ_POTIONS, POT_BLOOD_COAGULATED, ID_KNOWN_TYPE);

    switch (which_job)
    {
    case JOB_STALKER:
    case JOB_ASSASSIN:
    case JOB_VENOM_MAGE:
        set_ident_type(OBJ_POTIONS, POT_POISON, ID_KNOWN_TYPE);
        break;

    case JOB_TRANSMUTER:
        set_ident_type(OBJ_POTIONS, POT_WATER, ID_KNOWN_TYPE);
        break;

    case JOB_ARTIFICER:
        if (!item_is_rod(you.inv[2]))
            set_ident_type(OBJ_SCROLLS, SCR_RECHARGING, ID_KNOWN_TYPE);
        break;

    default:
        break;
    }
}

// Characters are actually granted skill points, not skill levels.
// Here we take racial aptitudes into account in determining final
// skill levels.
static void _reassess_starting_skills()
{
    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        if (you.skills[i] == 0
            && (you.species != SP_VAMPIRE || i != SK_UNARMED_COMBAT))
        {
            continue;
        }

        // Grant the amount of skill points required for a human.
        you.skill_points[i] = skill_exp_needed(you.skills[i], i,
        static_cast<species_type>(SP_HUMAN)) + 1;

        // Find out what level that earns this character.
        you.skills[i] = 0;

        for (int lvl = 1; lvl <= 8; ++lvl)
        {
            if (you.skill_points[i] > skill_exp_needed(lvl, i))
                you.skills[i] = lvl;
            else
                break;
        }

        // Vampires should always have Unarmed Combat skill.
        if (you.species == SP_VAMPIRE && i == SK_UNARMED_COMBAT
            && you.skills[i] < 1)
        {
            you.skill_points[i] = skill_exp_needed(1, i);
            you.skills[i] = 1;
        }

        // Wanderers get at least 1 level in their skills.
        if (you.char_class == JOB_WANDERER && you.skills[i] < 1)
        {
            you.skill_points[i] = skill_exp_needed(1, i);
            you.skills[i] = 1;
        }

        // Spellcasters should always have Spellcasting skill.
        if (i == SK_SPELLCASTING && you.skills[i] < 1)
        {
            you.skill_points[i] = skill_exp_needed(1, i);
            you.skills[i] = 1;
        }
    }
}

// For items that get a random colour, give them a more thematic one.
static void _apply_job_colour(item_def &item)
{
    if (!Options.classic_item_colours)
        return;

    if (item.base_type != OBJ_ARMOUR)
        return;

    switch (item.sub_type)
    {
    case ARM_CLOAK:
    case ARM_ROBE:
    case ARM_NAGA_BARDING:
    case ARM_CENTAUR_BARDING:
    case ARM_CAP:
    case ARM_WIZARD_HAT:
        break;
    default:
        return;
    }

    switch (you.char_class)
    {
    case JOB_NECROMANCER:
    case JOB_ASSASSIN:
        item.colour = DARKGREY;
        break;
    case JOB_FIRE_ELEMENTALIST:
        item.colour = RED;
        break;
    case JOB_ICE_ELEMENTALIST:
        item.colour = BLUE;
        break;
    case JOB_AIR_ELEMENTALIST:
        item.colour = LIGHTBLUE;
        break;
    case JOB_EARTH_ELEMENTALIST:
        item.colour = BROWN;
        break;
    case JOB_VENOM_MAGE:
        item.colour = MAGENTA;
        break;
    default:
        break;
    }
}

static void _setup_normal_game();
static void _setup_tutorial();
static void _setup_sprint(const newgame_def& ng);
static void _setup_hints();
static void _setup_generic(const newgame_def& ng);

// Initialise a game based on the choice stored in ng.
void setup_game(const newgame_def& ng)
{
    crawl_state.type = ng.type;

    switch (crawl_state.type)
    {
    case GAME_TYPE_NORMAL:
        _setup_normal_game();
        break;
    case GAME_TYPE_TUTORIAL:
        _setup_tutorial();
        break;
    case GAME_TYPE_SPRINT:
        _setup_sprint(ng);
        break;
    case GAME_TYPE_HINTS:
        _setup_hints();
        break;
    case GAME_TYPE_ARENA:
    default:
        ASSERT(!"Bad game type");
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
static void _setup_tutorial()
{
    make_hungry(0, true);
}

/**
 * Special steps that sprint needs;
 */
static void _setup_sprint(const newgame_def& ng)
{
    set_sprint_map(ng.map);
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

    strlcpy(you.class_name, get_job_name(you.char_class), sizeof(you.class_name));

    _species_stat_init(you.species);     // must be down here {dlb}

    you.is_undead = get_undead_state(you.species);

    // Before we get into the inventory init, set light radius based
    // on species vision. Currently, all species see out to 8 squares.
    update_vision_range();

    _jobs_stat_init(you.char_class);
    _give_last_paycheck(you.char_class);

    unfocus_stats();

    // Needs to be done before handing out food.
    give_basic_mutations(you.species);

    // This function depends on stats and mutations being finalised.
    _give_items_skills(ng);

    _give_species_bonus_hp();
    _give_species_bonus_mp();

    if (you.species == SP_DEMONSPAWN)
        roll_demonspawn_mutations();

    _give_starting_food();

    if (crawl_state.game_is_sprint())
        sprint_give_items();

    // Give tutorial skills etc
    if (crawl_state.game_is_tutorial())
        _setup_tutorial_miscs();

    _mark_starting_books();

    _give_basic_spells(you.char_class);
    _give_basic_knowledge(you.char_class);

    _racialise_starting_equipment();
    initialise_item_descriptions();

    _reassess_starting_skills();
    calc_total_skill_points();

    for (int i = 0; i < ENDOFPACK; ++i)
        if (you.inv[i].defined())
        {
            // XXX: Why is this here? Elsewhere it's only ever used for runes.
            you.inv[i].flags |= ISFLAG_BEEN_IN_INV;

            // Identify all items in pack.
            set_ident_type(you.inv[i], ID_KNOWN_TYPE);
            set_ident_flags(you.inv[i], ISFLAG_IDENT_MASK);

            // link properly
            you.inv[i].pos.set(-1, -1);
            you.inv[i].link = i;
            you.inv[i].slot = index_to_letter(you.inv[i].link);
            item_colour(you.inv[i]);  // set correct special and colour
            _apply_job_colour(you.inv[i]);
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
    set_hp(you.hp_max, false);
    set_mp(you.max_magic_points, false);

    // tmpfile purging removed in favour of marking
    Generated_Levels.clear();

    initialise_branch_depths();
    initialise_temples();
    init_level_connectivity();

    // Generate the second name of Jiyva
    fix_up_jiyva_name();

    // Create the save file.
    you.save = new package((get_savedir_filename(you.your_name, "", "")
                            + SAVE_SUFFIX).c_str(), true, true);

    // Pretend that a savefile was just loaded, in order to
    // get things setup properly.
    SavefileCallback::post_restore();
}
