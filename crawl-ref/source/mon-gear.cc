/*
 * File:       mon-gear.cc
 * Summary:    Monsters' starting equipment.
 */

#include "AppHdr.h"

#include <algorithm>

#include "enum.h"
#include "externs.h"
#include "mon-gear.h"

#include "artefact.h"
#include "colour.h"
#include "dungeon.h"
#include "env.h"
#include "itemprop.h"
#include "items.h"
#include "makeitem.h"
#include "mgen_enum.h"
#include "mon-place.h"
#include "mon-util.h"
#include "random.h"
#include "spl-book.h"


static void _give_monster_item(monsters *mon, int thing,
                               bool force_item = false,
                               bool (monsters::*pickupfn)(item_def&, int) = NULL)
{
    if (thing == NON_ITEM || thing == -1)
        return;

    item_def &mthing = mitm[thing];
    ASSERT(mthing.is_valid());

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS,
         "Giving %s to %s...", mthing.name(DESC_PLAIN).c_str(),
         mon->name(DESC_PLAIN, true).c_str());
#endif

    mthing.pos.reset();
    mthing.link = NON_ITEM;
    unset_ident_flags(mthing, ISFLAG_IDENT_MASK);

    if (mon->undead_or_demonic()
        && (is_blessed(mthing)
            || get_weapon_brand(mthing) == SPWPN_HOLY_WRATH))
    {
        if (is_blessed(mthing))
            convert2bad(mthing);
        if (get_weapon_brand(mthing) == SPWPN_HOLY_WRATH)
            set_item_ego_type(mthing, OBJ_WEAPONS, SPWPN_NORMAL);
    }

    unwind_var<int> save_speedinc(mon->speed_increment);
    if (!(pickupfn ? (mon->*pickupfn)(mthing, false)
                   : mon->pickup_item(mthing, false, true)))
    {
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Destroying %s because %s doesn't want it!",
             mthing.name(DESC_PLAIN, false, true).c_str(),
             mon->name(DESC_PLAIN, true).c_str());
#endif
        destroy_item(thing, true);
        return;
    }
    if (!mthing.is_valid()) // missiles merged into an existing stack
        return;
    ASSERT(mthing.holding_monster() == mon);

    if (!force_item || mthing.colour == BLACK)
        item_colour(mthing);
}

static void _give_scroll(monsters *mon, int level)
{
    int thing_created = NON_ITEM;

    if (mon->type == MONS_ROXANNE)
    {
        // Not a scroll, but this comes closest.
        int which_book = (one_chance_in(3) ? BOOK_TRANSFIGURATIONS
                                           : BOOK_EARTH);

        thing_created = items(0, OBJ_BOOKS, which_book, true, level, 0);

        if (thing_created != NON_ITEM && coinflip())
        {
            // Give Roxanne a random book containing Statue Form instead.
            item_def &item(mitm[thing_created]);
            make_book_Roxanne_special(&item);
            _give_monster_item(mon, thing_created, true);
            return;
        }
    }
    else if (mons_is_unique(mon->type) && one_chance_in(3))
        thing_created = items(0, OBJ_SCROLLS, OBJ_RANDOM, true, level, 0);

    if (thing_created == NON_ITEM)
        return;

    mitm[thing_created].flags = 0;
    _give_monster_item(mon, thing_created, true);
}

static void _give_wand(monsters *mon, int level)
{
    if (mons_is_unique(mon->type) && !mons_class_flag(mon->type, M_NO_WAND)
        && (one_chance_in(5) || mon->type == MONS_MAURICE && one_chance_in(3)))
    {
        const int idx = items(0, OBJ_WANDS, OBJ_RANDOM, true, level, 0);

        if (idx == NON_ITEM)
            return;

        item_def& wand = mitm[idx];

        // Don't give top-tier wands before 5 HD.
        if (mon->hit_dice < 5 || mons_class_flag(mon->type, M_NO_HT_WAND))
        {
            // Technically these wands will be undercharged, but it
            // doesn't really matter.
            if (wand.sub_type == WAND_FIRE)
                wand.sub_type = WAND_FLAME;

            if (wand.sub_type == WAND_COLD)
                wand.sub_type = WAND_FROST;

            if (wand.sub_type == WAND_LIGHTNING)
                wand.sub_type = (coinflip() ? WAND_FLAME : WAND_FROST);
        }

        wand.flags = 0;
        _give_monster_item(mon, idx);
    }
}

static void _give_potion(monsters *mon, int level)
{
    if (mons_species(mon->type) == MONS_VAMPIRE && one_chance_in(5))
    {
        // This handles initialization of stack timer.
        const int thing_created =
            items(0, OBJ_POTIONS, POT_BLOOD, true, level, 0);

        if (thing_created == NON_ITEM)
            return;

        mitm[thing_created].flags = 0;
        _give_monster_item(mon, thing_created);
    }
    else if (mons_is_unique(mon->type) && one_chance_in(3))
    {
        const int thing_created =
            items(0, OBJ_POTIONS, OBJ_RANDOM, true, level, 0);

        if (thing_created == NON_ITEM)
            return;

        mitm[thing_created].flags = 0;
        _give_monster_item(mon, thing_created, false,
                           &monsters::pickup_potion);
    }
}

static item_make_species_type _give_weapon(monsters *mon, int level,
                                           bool melee_only = false,
                                           bool give_aux_melee = true)
{
    bool force_item = false;

    item_def               item;
    item_make_species_type item_race = MAKE_ITEM_RANDOM_RACE;

    item.base_type = OBJ_UNASSIGNED;

    if (mon->type == MONS_DANCING_WEAPON
        && player_in_branch(BRANCH_HALL_OF_BLADES))
    {
        level = MAKE_GOOD_ITEM;
    }

    // moved setting of quantity here to keep it in mind {dlb}
    item.quantity = 1;

    switch (mon->type)
    {
    case MONS_KOBOLD:
        // A few of the smarter kobolds have blowguns.
        if (one_chance_in(10) && level > 1)
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = WPN_BLOWGUN;
            break;
        }
        // intentional fallthrough
    case MONS_BIG_KOBOLD:
        if (x_chance_in_y(3, 5))     // give hand weapon
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = random_choose(WPN_DAGGER,      WPN_DAGGER,
                                           WPN_SHORT_SWORD, WPN_SHORT_SWORD,
                                           WPN_CLUB,        WPN_WHIP,       -1);
        }
        else
            return (item_race);
        break;

    case MONS_HOBGOBLIN:
        if (one_chance_in(3))
            item_race = MAKE_ITEM_ORCISH;

        if (x_chance_in_y(3, 5))     // give hand weapon
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = WPN_CLUB;
        }
        else
            return (item_race);
        break;

    case MONS_GOBLIN:
        if (one_chance_in(3))
            item_race = MAKE_ITEM_ORCISH;

        if (!melee_only && one_chance_in(12) && level)
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = WPN_SLING;
            break;
        }
        // deliberate fall through {dlb}
    case MONS_JESSICA:
    case MONS_IJYB:
        if (x_chance_in_y(3, 5))     // give hand weapon
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = (coinflip() ? WPN_DAGGER : WPN_CLUB);
        }
        else
            return (item_race);
        break;

    case MONS_WIGHT:
    case MONS_NORRIS:
        item.base_type = OBJ_WEAPONS;

        if (one_chance_in(6))
        {
            item.sub_type = random_choose(WPN_SPIKED_FLAIL, WPN_GREAT_MACE,
                                          WPN_WAR_AXE,      WPN_TRIDENT,   -1);
        }
        else
        {
            item.sub_type = random_choose(
                WPN_MACE,      WPN_FLAIL,       WPN_MORNINGSTAR,
                WPN_DAGGER,    WPN_SHORT_SWORD, WPN_LONG_SWORD,
                WPN_SCIMITAR,  WPN_GREAT_SWORD, WPN_HAND_AXE,
                WPN_BATTLEAXE, WPN_SPEAR,       WPN_HALBERD,
                -1);
        }

        if (coinflip())
        {
            force_item  = true;
            item.plus  += 1 + random2(3);
            item.plus2 += 1 + random2(3);

            if (one_chance_in(5))
                set_item_ego_type(item, OBJ_WEAPONS, SPWPN_FREEZING);
        }

        if (one_chance_in(3))
            do_curse_item(item);
        break;

    case MONS_EDMUND:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = random_choose_weighted (30, WPN_FLAIL, 10, WPN_SPIKED_FLAIL,
                                                 5, WPN_DIRE_FLAIL, 0);
        // "expensive" flail. {due}
        if (item.sub_type == WPN_FLAIL)
            level = MAKE_GOOD_ITEM;

        break;

    case MONS_GNOLL:
    case MONS_OGRE_MAGE:
    case MONS_NAGA_WARRIOR:
    case MONS_GREATER_NAGA:
    case MONS_DUANE:
        item_race = MAKE_ITEM_NO_RACE;
        if (!one_chance_in(5))
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = random_choose(WPN_SPEAR, WPN_SPEAR, WPN_HALBERD,
                                           WPN_CLUB,  WPN_WHIP,  WPN_FLAIL, -1);
        }
        break;

    case MONS_PIKEL:
        force_item = true; // guaranteed flaming or pain
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_WHIP;
        set_item_ego_type(item, OBJ_WEAPONS, coinflip() ? SPWPN_PAIN : SPWPN_FLAMING);
        item.plus  += random2(3);
        item.plus2 += random2(3);
        break;

    case MONS_GRUM:
        force_item = true; // guaranteed reaching
        item_race  = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = random_choose(WPN_WHIP,    WPN_WHIP,   WPN_SPEAR,
                                       WPN_HALBERD, WPN_GLAIVE, -1);
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_REACHING);
        item.plus  += -2 + random2(4);
        item.plus2 += -1 + random2(2);
        break;

    case MONS_CRAZY_YIUF:
        force_item        = true; // guaranteed chaos
        item_race         = MAKE_ITEM_NO_RACE;
        item.base_type    = OBJ_WEAPONS;
        item.sub_type     = WPN_QUARTERSTAFF;
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_CHAOS);
        item.plus        += 2 + random2(3);
        item.plus2       += 2 + random2(3);
        break;

    case MONS_JOSEPH:
        if (!melee_only)
        {
            item.base_type  = OBJ_WEAPONS;
            item.sub_type   = WPN_SLING;
            break;
        }
        item_race  = MAKE_ITEM_NO_RACE;
        item.base_type      = OBJ_WEAPONS;
        item.sub_type       = WPN_QUARTERSTAFF;
        break;

    case MONS_ORC:
    case MONS_ORC_PRIEST:
        item_race = MAKE_ITEM_ORCISH;
        // deliberate fall through {gdl}

    case MONS_DRACONIAN:
    case MONS_DRACONIAN_ZEALOT:
        if (!one_chance_in(5))
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = random_choose_weighted(
                30, WPN_DAGGER,      30, WPN_CLUB,
                27, WPN_FLAIL,       24, WPN_HAND_AXE,
                20, WPN_HAMMER,      20, WPN_SHORT_SWORD,
                20, WPN_MACE,        10, WPN_WHIP,
                10, WPN_TRIDENT,     10, WPN_FALCHION,
                10, WPN_MORNINGSTAR, 6,  WPN_WAR_AXE,
                3, WPN_SPIKED_FLAIL,
                0);
        }
        else
            return (item_race);
        break;

    case MONS_TERENCE:
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = random_choose_weighted( 30, WPN_FLAIL,
                                                 20, WPN_HAND_AXE,
                                                 20, WPN_SHORT_SWORD,
                                                 20, WPN_MACE,
                                                 10, WPN_TRIDENT,
                                                 10, WPN_FALCHION,
                                                 10, WPN_MORNINGSTAR,
                                                  3, WPN_SPIKED_FLAIL,
                                                  0);
        break;

    case MONS_DUVESSA:
        item_race = MAKE_ITEM_ELVEN;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = random_choose_weighted(30, WPN_SHORT_SWORD,
                                               10, WPN_SABRE,
                                               0);
        break;

    case MONS_MARA:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = random_choose(WPN_DEMON_BLADE, WPN_DEMON_TRIDENT,
                                      WPN_DEMON_WHIP, -1);
        level = MAKE_GOOD_ITEM;
        break;

    case MONS_DEEP_ELF_FIGHTER:
    case MONS_DEEP_ELF_HIGH_PRIEST:
    case MONS_DEEP_ELF_KNIGHT:
    case MONS_DEEP_ELF_PRIEST:
    case MONS_DEEP_ELF_SOLDIER:
        item_race = MAKE_ITEM_ELVEN;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = random_choose_weighted(
            22, WPN_LONG_SWORD, 22, WPN_SHORT_SWORD, 17, WPN_SCIMITAR,
            17, WPN_BOW,        5,  WPN_LONGBOW,
            0);
        break;

    case MONS_DEEP_ELF_BLADEMASTER:
    {
        item_race = MAKE_ITEM_ELVEN;
        item.base_type = OBJ_WEAPONS;

        // If the blademaster already has a weapon, give him the exact same
        // sub_type to match.

        const item_def *weap = mon->mslot_item(MSLOT_WEAPON);
        if (weap && weap->base_type == OBJ_WEAPONS)
            item.sub_type = weap->sub_type;
        else
        {
            item.sub_type = random_choose_weighted(40, WPN_SABRE,
                                                   10, WPN_SHORT_SWORD,
                                                   2,  WPN_QUICK_BLADE,
                                                   0);
        }
        break;
    }

    case MONS_DEEP_ELF_MASTER_ARCHER:
        item_race = MAKE_ITEM_ELVEN;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_LONGBOW;
        break;

    case MONS_DEEP_ELF_ANNIHILATOR:
    case MONS_DEEP_ELF_CONJURER:
    case MONS_DEEP_ELF_DEATH_MAGE:
    case MONS_DEEP_ELF_DEMONOLOGIST:
    case MONS_DEEP_ELF_MAGE:
    case MONS_DEEP_ELF_SORCERER:
    case MONS_DEEP_ELF_SUMMONER:
        item_race = MAKE_ITEM_ELVEN;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = random_choose(WPN_LONG_SWORD,  WPN_LONG_SWORD,
                                       WPN_SHORT_SWORD, WPN_SABRE,
                                       WPN_DAGGER,
                                       -1);
        break;

    case MONS_DRACONIAN_SHIFTER:
    case MONS_DRACONIAN_SCORCHER:
    case MONS_DRACONIAN_ANNIHILATOR:
    case MONS_DRACONIAN_CALLER:
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = random_choose(WPN_LONG_SWORD,  WPN_LONG_SWORD,
                                       WPN_SHORT_SWORD, WPN_SABRE,
                                       WPN_DAGGER,      WPN_WHIP,
                                       -1);
        break;

    case MONS_ORC_WARRIOR:
    case MONS_ORC_HIGH_PRIEST:
    case MONS_BLORK_THE_ORC:
        item_race = MAKE_ITEM_ORCISH;
        // deliberate fall-through {dlb}

    case MONS_DANCING_WEAPON:   // give_level may have been adjusted above
    case MONS_FRANCES:
    case MONS_FRANCIS:
    case MONS_HAROLD:
    case MONS_LOUISE:
    case MONS_NAGA:
    case MONS_NAGA_MAGE:
    case MONS_RUPERT:
    case MONS_SKELETAL_WARRIOR:
    case MONS_PALE_DRACONIAN:
    case MONS_RED_DRACONIAN:
    case MONS_WHITE_DRACONIAN:
    case MONS_GREEN_DRACONIAN:
    case MONS_MOTTLED_DRACONIAN:
    case MONS_BLACK_DRACONIAN:
    case MONS_YELLOW_DRACONIAN:
    case MONS_PURPLE_DRACONIAN:
    case MONS_TIAMAT:
        if (mons_genus(mon->type) == MONS_NAGA)
            item_race = MAKE_ITEM_NO_RACE;

        item.base_type = OBJ_WEAPONS;
        item.sub_type  = random_choose_weighted(
            10, WPN_LONG_SWORD, 10, WPN_SHORT_SWORD,
            10, WPN_SCIMITAR,   10, WPN_BATTLEAXE,
            10, WPN_HAND_AXE,   10, WPN_HALBERD,
            10, WPN_GLAIVE,     10, WPN_MORNINGSTAR,
            10, WPN_GREAT_MACE, 10, WPN_TRIDENT,
            9,  WPN_WAR_AXE,     9, WPN_FLAIL,
            1,  WPN_BROAD_AXE,   1, WPN_SPIKED_FLAIL,
            0);
        break;

    case MONS_WAYNE:
        item_race = MAKE_ITEM_DWARVEN;

        item.base_type = OBJ_WEAPONS;
        // speech references an axe
        item.sub_type  = random_choose(WPN_WAR_AXE, WPN_BROAD_AXE,
                                       WPN_BATTLEAXE, -1);
        break;

    case MONS_ORC_WARLORD:
    case MONS_SAINT_ROKA:
        // being at the top has its privileges
        if (one_chance_in(3))
            level = MAKE_GOOD_ITEM;
        // deliberate fall-through

    case MONS_ORC_KNIGHT:
        item_race = MAKE_ITEM_ORCISH;
        // Occasionally get crossbows.
        if (!melee_only && one_chance_in(9))
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = WPN_CROSSBOW;
            break;
        }
        // deliberate fall-through

    case MONS_URUG:
        item_race = MAKE_ITEM_ORCISH;
        // deliberate fall-through

    case MONS_NORBERT:
    case MONS_JOZEF:
    case MONS_VAULT_GUARD:
    case MONS_VAMPIRE_KNIGHT:
    case MONS_DRACONIAN_KNIGHT:
    {
        item.base_type = OBJ_WEAPONS;

        item.sub_type = random_choose_weighted(
            4, WPN_GREAT_SWORD, 4, WPN_LONG_SWORD,
            4, WPN_BATTLEAXE,   4, WPN_WAR_AXE,
            3, WPN_GREAT_MACE,  2, WPN_DIRE_FLAIL,
            1, WPN_BARDICHE,    1, WPN_GLAIVE,
            1, WPN_BROAD_AXE,   1, WPN_HALBERD,
            0);

        if (one_chance_in(4))
            item.plus += 1 + random2(3);
        break;
    }

    case MONS_POLYPHEMUS:
    case MONS_CYCLOPS:
    case MONS_STONE_GIANT:
        item.base_type = OBJ_MISSILES;
        item.sub_type  = MI_LARGE_ROCK;
        break;

    case MONS_TWO_HEADED_OGRE:
    case MONS_ETTIN:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = (one_chance_in(3) ? WPN_GIANT_SPIKED_CLUB
                                           : WPN_GIANT_CLUB);

        if (one_chance_in(10) || mon->type == MONS_ETTIN)
        {
            item.sub_type = (one_chance_in(10) ? WPN_DIRE_FLAIL
                                               : WPN_GREAT_MACE);
        }
        break;

    case MONS_REAPER:
        level = MAKE_GOOD_ITEM;
        // intentional fall-through...
    case MONS_SIGMUND:
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_SCYTHE;
        break;

    case MONS_BALRUG:
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_DEMON_WHIP;
        break;

    case MONS_RED_DEVIL:
        if (!one_chance_in(3))
        {
            item_race = MAKE_ITEM_NO_RACE;
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = (one_chance_in(3) ? WPN_DEMON_TRIDENT
                                               : WPN_TRIDENT);
        }
        break;

    case MONS_OGRE:
    case MONS_HILL_GIANT:
    case MONS_EROLCHA:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = (one_chance_in(3) ? WPN_GIANT_SPIKED_CLUB
                                           : WPN_GIANT_CLUB);

        if (one_chance_in(10))
        {
            item.sub_type = (one_chance_in(10) ? WPN_DIRE_FLAIL
                                               : WPN_GREAT_MACE);
        }
        break;

    case MONS_ILSUIW:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_TRIDENT;
        item.special = SPWPN_FREEZING;
        item.plus = random_range(-1, 6, 2);
        item.plus2 = random_range(-1, 6, 2);
        item.colour = ETC_ICE;
        force_item = true;
        break;

    case MONS_MERFOLK_IMPALER:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = random_choose_weighted(100, WPN_TRIDENT,
                                               15, WPN_DEMON_TRIDENT,
                                               0);
        if (coinflip())
            level = MAKE_GOOD_ITEM;
        else if (coinflip())
        {
            // Per dpeg request :)
            item.special = SPWPN_REACHING;
            item.plus = random_range(-1, 6, 2);
            item.plus2 = random_range(-1, 5, 2);
            force_item = true;
        }
        break;


    case MONS_MERFOLK_AQUAMANCER:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_SABRE;
        if (coinflip())
            level = MAKE_GOOD_ITEM;
        break;

    case MONS_MERFOLK_JAVELINEER:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_SPEAR;
        if (!one_chance_in(3))
            level = MAKE_GOOD_ITEM;
        break;

    case MONS_MERFOLK:
        if (active_monster_band == BAND_MERFOLK_IMPALER)
        {
            item_race = MAKE_ITEM_NO_RACE;
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = random_choose_weighted(10, WPN_SPEAR,
                                                    10, WPN_TRIDENT,
                                                    5, WPN_HALBERD,
                                                    5, WPN_GLAIVE,
                                                    0);
            break;
        }
        if (one_chance_in(3))
        {
            item_race = MAKE_ITEM_NO_RACE;
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = WPN_TRIDENT;
            break;
        }
        // intentionally fall through

    case MONS_MERMAID:
        if (one_chance_in(3))
        {
            item_race = MAKE_ITEM_NO_RACE;
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = WPN_SPEAR;
        }
        break;

    case MONS_CENTAUR:
    case MONS_CENTAUR_WARRIOR:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_BOW;
        if (mon->type == MONS_CENTAUR_WARRIOR && one_chance_in(3))
            item.sub_type = WPN_LONGBOW;
        break;

    case MONS_NESSOS:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_LONGBOW;
        item.special   = SPWPN_FLAME;
        item.plus     += 1 + random2(3);
        item.plus2    += 1 + random2(3);
        item.colour    = DARKGREY;
        force_item     = true;
        break;

    case MONS_YAKTAUR:
    case MONS_YAKTAUR_CAPTAIN:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_CROSSBOW;
        break;

    case MONS_EFREET:
    case MONS_ERICA:
    case MONS_AZRAEL:
        force_item     = true;
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_SCIMITAR;
        item.plus      = random2(5);
        item.plus2     = random2(5);
        item.colour    = RED;  // forced by force_item above {dlb}
        set_item_ego_type( item, OBJ_WEAPONS, SPWPN_FLAMING );
        break;

    case MONS_ANGEL:
        force_item     = true;
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.colour    = YELLOW;       // forced by force_item above {dlb}

        item.sub_type  = (one_chance_in(4) ? WPN_HOLY_SCOURGE
                                           : WPN_WHIP);

        set_equip_desc(item, ISFLAG_GLOWING);
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_HOLY_WRATH);
        item.plus  = 1 + random2(3);
        item.plus2 = 1 + random2(3);
        break;

    case MONS_DAEVA:
        force_item     = true;
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.colour    = YELLOW;       // forced by force_item above {dlb}

        item.sub_type  = (one_chance_in(4) ? WPN_HOLY_BLADE
                                           : WPN_LONG_SWORD);

        set_equip_desc(item, ISFLAG_GLOWING);
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_HOLY_WRATH);
        item.plus  = 1 + random2(3);
        item.plus2 = 1 + random2(3);
        break;

    case MONS_DONALD:
        force_item = true;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = random_choose_weighted(12, WPN_SCIMITAR,
                                               11, WPN_LONG_SWORD,
                                               1, WPN_EVENINGSTAR,
                                               4, WPN_WAR_AXE,
                                               5, WPN_BROAD_AXE,
                                               6, WPN_DEMON_WHIP,
                                               7, WPN_DEMON_BLADE,
                                               7, WPN_DEMON_TRIDENT,
                                               0);

        if (x_chance_in_y(5, 9))
        {
            set_item_ego_type(item, OBJ_WEAPONS,
                              random_choose_weighted(15, SPWPN_FLAMING,
                                                     2, SPWPN_DRAINING,
                                                     2, SPWPN_VORPAL,
                                                     2, SPWPN_DISTORTION,
                                                     2, SPWPN_SPEED,
                                                     2, SPWPN_PAIN,
                                                     0));
        }

        item.plus  += random2(6);
        item.plus2 += random2(6);

        item.colour = random_choose_weighted(1, CYAN,
                                             1, DARKGREY,
                                             2, BLUE,
                                             0);

        break;

    case MONS_HELL_KNIGHT:
    case MONS_MAUD:
    case MONS_FREDERICK:
    case MONS_MARGERY:
        force_item = true;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = random_choose_weighted(5, WPN_HALBERD,
                                               5, WPN_GLAIVE,
                                               6, WPN_WAR_AXE,
                                               6, WPN_GREAT_MACE,
                                               7, WPN_BATTLEAXE,
                                               8, WPN_LONG_SWORD,
                                               8, WPN_SCIMITAR,
                                               8, WPN_GREAT_SWORD,
                                               9, WPN_BROAD_AXE,
                                               10, WPN_DEMON_WHIP,
                                               13, WPN_DEMON_BLADE,
                                               14, WPN_DEMON_TRIDENT,
                                               0);

        if (x_chance_in_y(5, 9))
        {
            set_item_ego_type(item, OBJ_WEAPONS,
                              random_choose_weighted(15, SPWPN_FLAMING,
                                                     2, SPWPN_DRAINING,
                                                     2, SPWPN_VORPAL,
                                                     2, SPWPN_DISTORTION,
                                                     2, SPWPN_SPEED,
                                                     2, SPWPN_PAIN,
                                                     0));
        }

        item.plus  += random2(6);
        item.plus2 += random2(6);

        item.colour = random_choose_weighted(3, CYAN,
                                             4, DARKGREY,
                                             8, RED,
                                             0);
        break;

    case MONS_FIRE_GIANT:
        force_item = true;
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_GREAT_SWORD;
        item.plus      = 0;
        item.plus2     = 0;
        set_item_ego_type( item, OBJ_WEAPONS, SPWPN_FLAMING );

        item.colour = random_choose_weighted(3, CYAN,
                                             4, DARKGREY,
                                             8, RED,
                                             0);

        break;

    case MONS_FROST_GIANT:
        force_item = true;
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_BATTLEAXE;
        item.plus      = 0;
        item.plus2     = 0;
        set_item_ego_type( item, OBJ_WEAPONS, SPWPN_FREEZING );

        // forced by force_item above {dlb}
        item.colour = (one_chance_in(3) ? WHITE : CYAN);
        break;

    case MONS_ORC_WIZARD:
    case MONS_ORC_SORCERER:
    case MONS_NERGALLE:
        item_race = MAKE_ITEM_ORCISH;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_DAGGER;
        break;

    case MONS_DOWAN:
        item_race = MAKE_ITEM_ELVEN;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_DAGGER;
        break;

    case MONS_KOBOLD_DEMONOLOGIST:
    case MONS_NECROMANCER:
    case MONS_WIZARD:
    case MONS_PSYCHE:
    case MONS_JOSEPHINE:
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_DAGGER;

        if (mon->type == MONS_PSYCHE)
        {
            force_item = true;
            set_item_ego_type(item, OBJ_WEAPONS,
                              random_choose_weighted(3, SPWPN_CHAOS,
                                                     1, SPWPN_DISTORTION,
                                                     0));
        }
        break;

    case MONS_AGNES:
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_LAJATANG;
        if (!one_chance_in(3))
            level = MAKE_GOOD_ITEM;
        break;

    case MONS_SONJA:
        if (!melee_only)
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = WPN_BLOWGUN;
            item_race = MAKE_ITEM_NO_RACE;
            break;
        }
        force_item = true;
        item_race  = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = coinflip() ? WPN_DAGGER : WPN_SHORT_SWORD;
        set_item_ego_type(item, OBJ_WEAPONS,
                          random_choose_weighted(3, SPWPN_DISTORTION,
                                                 2, SPWPN_VENOM,
                                                 1, SPWPN_DRAINING,
                                                 0));
        break;

    case MONS_MAURICE:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = coinflip() ? WPN_DAGGER : WPN_SHORT_SWORD;
        break;

    case MONS_EUSTACHIO:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = (one_chance_in(3) ? WPN_FALCHION : WPN_SABRE);
        break;

    case MONS_NIKOLA:
        force_item = true;
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_SABRE;
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_ELECTROCUTION);
        item.plus      = random2(5);
        item.plus2     = random2(5);
        break;

    case MONS_CEREBOV:
        force_item = true;
        make_item_unrandart(item, UNRAND_CEREBOV);
        break;

    case MONS_DISPATER:
        force_item = true;
        make_item_unrandart(item, UNRAND_DISPATER);
        break;

    case MONS_ASMODEUS:
        force_item = true;
        make_item_unrandart(item, UNRAND_ASMODEUS);
        break;

    case MONS_GERYON:
        // mv: Probably should be moved out of this switch, but it's not
        // worth it, unless we have more monsters with misc. items.
        item.base_type = OBJ_MISCELLANY;
        item.sub_type  = MISC_HORN_OF_GERYON;
        break;

    case MONS_SALAMANDER: // mv: new 8 Aug 2001
                          // Yes, they've got really nice items, but
                          // it's almost impossible to get them.
        force_item = true;
        item_race  = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = random_choose(WPN_GREAT_SWORD, WPN_TRIDENT,
                                       WPN_SPEAR,       WPN_GLAIVE,
                                       WPN_BOW,         WPN_HALBERD,
                                       -1);

        set_item_ego_type(item, OBJ_WEAPONS, is_range_weapon(item) ?
                          SPWPN_FLAME : SPWPN_FLAMING);

        item.plus   = random2(5);
        item.plus2  = random2(5);
        item.colour = RED;  // forced by force_item above {dlb}
        break;

    case MONS_SPRIGGAN:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        // no quick blades for mooks
        item.sub_type  = random_choose(WPN_DAGGER, WPN_SHORT_SWORD,
                                       WPN_SABRE, -1);
        break;

    case MONS_SPRIGGAN_DRUID:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_QUARTERSTAFF;
        break;

    default:
        break;
    }

    // Only happens if something in above switch doesn't set it. {dlb}
    if (item.base_type == OBJ_UNASSIGNED)
        return (item_race);

    if (!force_item && mons_is_unique( mon->type ))
    {
        if (x_chance_in_y(10 + mon->hit_dice, 100))
            level = MAKE_GOOD_ITEM;
        else if (level != MAKE_GOOD_ITEM)
            level += 5;
    }

    const object_class_type xitc = item.base_type;
    const int xitt = item.sub_type;

    // Note this mess, all the work above doesn't mean much unless
    // force_item is set... otherwise we're just going to take the base
    // and subtype and create a new item. - bwr
    const int thing_created =
        ((force_item) ? get_item_slot() : items(0, xitc, xitt, true,
                                                level, item_race));

    if (thing_created == NON_ITEM)
        return (item_race);

    // Copy temporary item into the item array if were forcing it, since
    // items() won't have done it for us.
    if (force_item)
        mitm[thing_created] = item;

    item_def &i = mitm[thing_created];
    if (melee_only && (i.base_type != OBJ_WEAPONS || is_range_weapon(i)))
    {
        destroy_item(thing_created);
        return (item_race);
    }

    if (force_item)
        item_set_appearance(i);

    _give_monster_item(mon, thing_created, force_item);

    if (give_aux_melee && (i.base_type != OBJ_WEAPONS || is_range_weapon(i)))
        _give_weapon(mon, level, true, false);

    return (item_race);
}

// Hands out ammunition fitting the monster's launcher (if any), or else any
// throwable weapon depending on the monster type.
static void _give_ammo(monsters *mon, int level,
                       item_make_species_type item_race,
                       bool mons_summoned)
{
    // Note that item_race is not reset for this section.
    if (const item_def *launcher = mon->launcher())
    {
        const object_class_type xitc = OBJ_MISSILES;
        int xitt = fires_ammo_type(*launcher);

        if (xitt == MI_STONE && (one_chance_in(15) || mon->type == MONS_JOSEPH))
            xitt = MI_SLING_BULLET;

        const int thing_created = items(0, xitc, xitt, true, level, item_race);

        if (thing_created == NON_ITEM)
            return;

        if (xitt == MI_NEEDLE)
        {
            if (mon->type == MONS_SONJA)
            {
                set_item_ego_type(mitm[thing_created], OBJ_MISSILES,
                                  SPMSL_CURARE);

                mitm[thing_created].quantity = random_range(4, 10);
            }
            else
            {
                set_item_ego_type(mitm[thing_created], OBJ_MISSILES,
                                  got_curare_roll(level) ? SPMSL_CURARE
                                                         : SPMSL_POISONED);

                if (get_ammo_brand( mitm[thing_created] ) == SPMSL_CURARE)
                    mitm[thing_created].quantity = random_range(2, 8);
            }
        }
        else
        {
            // Sanity check to avoid useless brands.
            const int bow_brand  = get_weapon_brand(*launcher);
            const int ammo_brand = get_ammo_brand(mitm[thing_created]);
            if (ammo_brand != SPMSL_NORMAL
                && (bow_brand == SPWPN_FLAME || bow_brand == SPWPN_FROST))
            {
                mitm[thing_created].special = SPMSL_NORMAL;
            }
        }

        switch (mon->type)
        {
            case MONS_DEEP_ELF_MASTER_ARCHER:
                // Master archers get double ammo - archery is their only attack
                mitm[thing_created].quantity *= 2;
                break;

            case MONS_NESSOS:
                mitm[thing_created].special = SPMSL_POISONED;
                break;

            case MONS_JOSEPH:
                mitm[thing_created].quantity += 2 + random2(7);
                mitm[thing_created].plus += 2 + random2(2);
                if (mitm[thing_created].plus > 6)
                    mitm[thing_created].plus = 6;
                break;

            default:
                break;
        }

        _give_monster_item(mon, thing_created);
    }
    else
    {
        // Give some monsters throwing weapons.
        int weap_type = -1;
        object_class_type weap_class = OBJ_WEAPONS;
        int qty = 0;
        switch (mon->type)
        {
        case MONS_KOBOLD:
        case MONS_BIG_KOBOLD:
            if (x_chance_in_y(2, 5))
            {
                item_race  = MAKE_ITEM_NO_RACE;
                weap_class = OBJ_MISSILES;
                weap_type  = MI_DART;
                qty = 1 + random2(5);
            }
            break;

        case MONS_ORC_WARRIOR:
            if (one_chance_in(
                    you.where_are_you == BRANCH_ORCISH_MINES? 9 : 20))
            {
                weap_type = random_choose(WPN_HAND_AXE, WPN_SPEAR, -1);
                qty       = random_range(4, 8);
                item_race = MAKE_ITEM_ORCISH;
            }
            break;

        case MONS_ORC:
            if (one_chance_in(20))
            {
                weap_type = random_choose(WPN_HAND_AXE, WPN_SPEAR, -1);
                qty       = random_range(2, 5);
                item_race = MAKE_ITEM_ORCISH;
            }
            break;

        case MONS_URUG:
            weap_type  = MI_JAVELIN;
            weap_class = OBJ_MISSILES;
            item_race  = MAKE_ITEM_ORCISH;
            qty = random_range(4, 7);
            break;

        case MONS_MERFOLK_JAVELINEER:
            weap_class = OBJ_MISSILES;
            weap_type  = MI_JAVELIN;
            item_race  = MAKE_ITEM_NO_RACE;
            qty        = random_range(9, 23, 2);
            if (one_chance_in(3))
                level = MAKE_GOOD_ITEM;
            break;

        case MONS_MERFOLK:
            if (one_chance_in(3)
                || active_monster_band == BAND_MERFOLK_JAVELINEER)
            {
                item_race  = MAKE_ITEM_NO_RACE;
                weap_class = OBJ_WEAPONS;
                weap_type  = WPN_SPEAR;
                qty        = random_range(4, 8);
                if (active_monster_band == BAND_MERFOLK_JAVELINEER)
                    break;
            }
            if (one_chance_in(6) && !mons_summoned)
            {
                weap_class = OBJ_MISSILES;
                weap_type  = MI_THROWING_NET;
                qty        = 1;
                if (one_chance_in(4))
                    qty += random2(3); // up to three nets
            }
            break;

        case MONS_DRACONIAN_KNIGHT:
        case MONS_GNOLL:
        case MONS_HILL_GIANT:
            if (!one_chance_in(20))
                break;
            // deliberate fall-through

        case MONS_HAROLD: // bounty hunters
        case MONS_JOZEF:  // up to 5 nets
            if (mons_summoned)
                break;

            weap_class = OBJ_MISSILES;
            weap_type  = MI_THROWING_NET;
            qty        = 1;
            if (one_chance_in(3))
                qty++;
            if (mon->type == MONS_HAROLD || mon->type == MONS_JOZEF)
                qty += random2(4);

            break;

        default:
            break;
        }

        if (weap_type == -1)
            return;

        const int thing_created =
            items(0, weap_class, weap_type, true, level, item_race);

        if (thing_created != NON_ITEM)
        {
            item_def& w(mitm[thing_created]);

            // Limit returning brand to only one.
            if (weap_type == OBJ_WEAPONS
                && get_weapon_brand(w) == SPWPN_RETURNING)
            {
                qty = 1;
            }

            w.quantity = qty;
            _give_monster_item(mon, thing_created, false,
                               (weap_class == OBJ_WEAPONS?
                                &monsters::pickup_melee_weapon
                                : &monsters::pickup_throwable_weapon));
        }
    }
}

static bool make_item_for_monster(
    monsters *mons,
    object_class_type base,
    int subtype,
    int level,
    item_make_species_type race = MAKE_ITEM_NO_RACE,
    int allow_uniques = 0)
{
    const int bp = get_item_slot();
    if (bp == NON_ITEM)
        return (false);

    const int thing_created =
        items(allow_uniques, base, subtype, true, level, race);

    if (thing_created == NON_ITEM)
        return (false);

    _give_monster_item(mons, thing_created);
    return (true);
}

void give_shield(monsters *mon, int level)
{
    const item_def *main_weap = mon->mslot_item(MSLOT_WEAPON);
    const item_def *alt_weap  = mon->mslot_item(MSLOT_ALT_WEAPON);

    // If the monster is already wielding/carrying a two-handed weapon,
    // it doesn't get a shield.  (Monsters always prefer raw damage to
    // protection!)
    if (main_weap && hands_reqd(*main_weap, mon->body_size()) == HANDS_TWO
        || alt_weap && hands_reqd(*alt_weap, mon->body_size()) == HANDS_TWO)
    {
        return;
    }

    switch (mon->type)
    {
    case MONS_DAEVA:
        make_item_for_monster(mon, OBJ_ARMOUR, ARM_LARGE_SHIELD,
                              level * 2 + 1, MAKE_ITEM_NO_RACE, 1);
        break;

    case MONS_DEEP_ELF_SOLDIER:
    case MONS_DEEP_ELF_FIGHTER:
        if (one_chance_in(6))
        {
            make_item_for_monster(mon, OBJ_ARMOUR, ARM_BUCKLER,
                                  level, MAKE_ITEM_ELVEN);
        }
        break;
    case MONS_NAGA_WARRIOR:
    case MONS_VAULT_GUARD:
        if (one_chance_in(3))
        {
            make_item_for_monster(mon, OBJ_ARMOUR,
                                  one_chance_in(3) ? ARM_LARGE_SHIELD
                                                   : ARM_SHIELD,
                                  level, MAKE_ITEM_NO_RACE);
        }
        break;
    case MONS_DRACONIAN_KNIGHT:
        if (coinflip())
        {
            make_item_for_monster(mon, OBJ_ARMOUR,
                                  coinflip()? ARM_LARGE_SHIELD : ARM_SHIELD,
                                  level, MAKE_ITEM_NO_RACE);
        }
        break;
    case MONS_DEEP_ELF_KNIGHT:
        if (one_chance_in(3))
        {
            make_item_for_monster(mon, OBJ_ARMOUR, ARM_BUCKLER,
                                  level, MAKE_ITEM_ELVEN);
        }
        break;
    case MONS_SPRIGGAN:
        if (one_chance_in(4))
        {
            make_item_for_monster(mon, OBJ_ARMOUR, ARM_BUCKLER,
                                  level, MAKE_ITEM_NO_RACE);
        }
        break;
    case MONS_NORRIS:
        make_item_for_monster(mon, OBJ_ARMOUR, ARM_BUCKLER,
                              level * 2 + 1, MAKE_ITEM_RANDOM_RACE, 1);
        break;
    case MONS_WAYNE:
        make_item_for_monster(mon, OBJ_ARMOUR, ARM_SHIELD,
                              level * 2 + 1, MAKE_ITEM_DWARVEN, 1);
        break;
    case MONS_NORBERT:
    case MONS_LOUISE:
        make_item_for_monster(mon, OBJ_ARMOUR, ARM_LARGE_SHIELD,
                              level * 2 + 1, MAKE_ITEM_RANDOM_RACE, 1);
        break;
    case MONS_DONALD:
        make_item_for_monster(mon, OBJ_ARMOUR, ARM_SHIELD,
                              level * 2 + 1, MAKE_ITEM_RANDOM_RACE, 1);

        if (coinflip())
        {
            item_def *shield = mon->shield();
            if (shield)
                set_item_ego_type(*shield, OBJ_ARMOUR, SPARM_REFLECTION);
        }

        break;
    case MONS_NIKOLA:
        {
            make_item_for_monster(mon, OBJ_ARMOUR, ARM_GLOVES,
                                  level * 2 + 1, MAKE_ITEM_NO_RACE, 1);

            item_def *gaunt = mon->shield();
            if (gaunt)
                gaunt->plus2 = TGLOV_DESC_GAUNTLETS;
        }
        break;
    default:
        break;
    }
}

void give_armour(monsters *mon, int level)
{
    item_def               item;
    item_make_species_type item_race = MAKE_ITEM_RANDOM_RACE;

    item.base_type = OBJ_UNASSIGNED;
    item.quantity  = 1;

    int force_colour = 0; // mv: Important!!! Items with force_colour = 0
                          // are colored by default after following
                          // switch. Others will get force_colour.

    bool force_item = false;

    switch (mon->type)
    {
    case MONS_DEEP_ELF_BLADEMASTER:
    case MONS_DEEP_ELF_MASTER_ARCHER:
        item_race      = MAKE_ITEM_ELVEN;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_LEATHER_ARMOUR;
        break;

    case MONS_DUVESSA:
    case MONS_DEEP_ELF_ANNIHILATOR:
    case MONS_DEEP_ELF_CONJURER:
    case MONS_DEEP_ELF_DEATH_MAGE:
    case MONS_DEEP_ELF_DEMONOLOGIST:
    case MONS_DEEP_ELF_FIGHTER:
    case MONS_DEEP_ELF_HIGH_PRIEST:
    case MONS_DEEP_ELF_KNIGHT:
    case MONS_DEEP_ELF_MAGE:
    case MONS_DEEP_ELF_PRIEST:
    case MONS_DEEP_ELF_SOLDIER:
    case MONS_DEEP_ELF_SORCERER:
    case MONS_DEEP_ELF_SUMMONER:
        if (item_race == MAKE_ITEM_RANDOM_RACE)
            item_race = MAKE_ITEM_ELVEN;
        // deliberate fall through {dlb}

    case MONS_IJYB:
    case MONS_ORC:
    case MONS_ORC_HIGH_PRIEST:
    case MONS_ORC_PRIEST:
    case MONS_ORC_SORCERER:
        if (item_race == MAKE_ITEM_RANDOM_RACE)
            item_race = MAKE_ITEM_ORCISH;
        // deliberate fall through {dlb}

    case MONS_ERICA:
    case MONS_HAROLD:
    case MONS_JOSEPHINE:
    case MONS_JOZEF:
    case MONS_NORBERT:
    case MONS_PSYCHE:
        if (x_chance_in_y(2, 5))
        {
            item.base_type = OBJ_ARMOUR;

            const int temp_rand = random2(8);
            item.sub_type = ((temp_rand  < 4) ? ARM_LEATHER_ARMOUR :
                             (temp_rand  < 6) ? ARM_RING_MAIL :
                             (temp_rand == 6) ? ARM_SCALE_MAIL
                                              : ARM_CHAIN_MAIL);
        }
        else
            return;
        break;

    case MONS_JOSEPH:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = random_choose_weighted(3, ARM_LEATHER_ARMOUR,
                                                2, ARM_RING_MAIL,
                                                0);
        break;

    case MONS_TERENCE:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = random_choose_weighted(1, ARM_RING_MAIL,
                                                3, ARM_SCALE_MAIL,
                                                2, ARM_CHAIN_MAIL,
                                                0);
        break;

    case MONS_SLAVE:
    case MONS_GRUM:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ANIMAL_SKIN;
        break;

    case MONS_URUG:
        item_race = MAKE_ITEM_ORCISH;
        // deliberate fall through {dlb}

    case MONS_DUANE:
    case MONS_EDMUND:
    case MONS_RUPERT:
    {
        item.base_type = OBJ_ARMOUR;

        const int temp_rand = random2(4);
        item.sub_type = ((temp_rand == 0) ? ARM_LEATHER_ARMOUR :
                         (temp_rand == 1) ? ARM_RING_MAIL :
                         (temp_rand == 2) ? ARM_SCALE_MAIL
                                          : ARM_CHAIN_MAIL);
        break;
    }

    case MONS_WAYNE:
        item_race = MAKE_ITEM_DWARVEN;
        item.base_type = OBJ_ARMOUR;
        if (one_chance_in(3))
            level = MAKE_GOOD_ITEM;

        item.sub_type = random_choose_weighted(3, ARM_CHAIN_MAIL,
                            3, ARM_BANDED_MAIL, 3, ARM_SPLINT_MAIL,
                            10, ARM_PLATE_MAIL, 1, ARM_CRYSTAL_PLATE_MAIL,
                            0);
        break;

    case MONS_ORC_WARLORD:
    case MONS_SAINT_ROKA:
        // Being at the top has its privileges. :)
        if (one_chance_in(3))
            level = MAKE_GOOD_ITEM;
        // deliberate fall through

    case MONS_ORC_KNIGHT:
    case MONS_ORC_WARRIOR:
        item_race = MAKE_ITEM_ORCISH;
        // deliberate fall through {dlb}

    case MONS_FREDERICK:
    case MONS_HELL_KNIGHT:
    case MONS_LOUISE:
    case MONS_MARGERY:
    case MONS_DONALD:
    case MONS_MAUD:
    case MONS_VAMPIRE_KNIGHT:
    case MONS_VAULT_GUARD:
    {
        item.base_type = OBJ_ARMOUR;

        const int temp_rand = random2(4);
        item.sub_type = ((temp_rand == 0) ? ARM_CHAIN_MAIL :
                         (temp_rand == 1) ? ARM_SPLINT_MAIL :
                         (temp_rand == 2) ? ARM_BANDED_MAIL
                                          : ARM_PLATE_MAIL);
        break;
    }

    case MONS_MERFOLK_IMPALER:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type = random_choose_weighted(100, ARM_ROBE,
                                               60, ARM_LEATHER_ARMOUR,
                                               5, ARM_TROLL_LEATHER_ARMOUR,
                                               5, ARM_STEAM_DRAGON_ARMOUR,
                                               0);
        break;

    case MONS_MERFOLK_JAVELINEER:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type = ARM_LEATHER_ARMOUR;
        break;

    case MONS_ANGEL:
    case MONS_SIGMUND:
    case MONS_WIGHT:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ROBE;
        force_colour   = WHITE;
        break;

    // Centaurs sometimes wear barding.
    case MONS_CENTAUR:
    case MONS_CENTAUR_WARRIOR:
    case MONS_YAKTAUR:
    case MONS_YAKTAUR_CAPTAIN:
        if ( one_chance_in( mon->type == MONS_CENTAUR              ? 1000 :
                            mon->type == MONS_CENTAUR_WARRIOR      ?  500 :
                            mon->type == MONS_YAKTAUR              ?  300
                         /* mon->type == MONS_YAKTAUR_CAPTAIN ? */ :  200))
        {
            item.base_type = OBJ_ARMOUR;
            item.sub_type  = ARM_CENTAUR_BARDING;
        }
        else
            return;
        break;

    case MONS_NAGA:
    case MONS_NAGA_MAGE:
    case MONS_NAGA_WARRIOR:
    case MONS_GREATER_NAGA:
        if ( one_chance_in( mon->type == MONS_NAGA         ?  800 :
                            mon->type == MONS_NAGA_WARRIOR ?  300 :
                            mon->type == MONS_NAGA_MAGE    ?  200
                                                           :  100 ))
        {
            item_race      = MAKE_ITEM_NO_RACE;
            item.base_type = OBJ_ARMOUR;
            item.sub_type  = ARM_NAGA_BARDING;
        }
        else if (mon->type == MONS_GREATER_NAGA || one_chance_in(3))
        {
            item_race      = MAKE_ITEM_NO_RACE;
            item.base_type = OBJ_ARMOUR;
            item.sub_type  = ARM_ROBE;
        }
        else
            return;
        break;

    case MONS_GASTRONOK:
        if (one_chance_in(10))
        {
            force_item = true;
            make_item_unrandart(item, UNRAND_PONDERING);
        }
        else
        {
            item_race      = MAKE_ITEM_NO_RACE;
            item.base_type = OBJ_ARMOUR;
            item.sub_type  = ARM_WIZARD_HAT;

            // Not as good as it sounds. Still just +0 a lot of the time.
            level          = MAKE_GOOD_ITEM;
        }
        break;

    case MONS_MAURICE:
        force_colour   = DARKGREY;
        // intentional fall-through
    case MONS_CRAZY_YIUF:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_CLOAK;
        break;

    case MONS_DOWAN:
        item_race = MAKE_ITEM_ELVEN;
        // intentional fall-through
    case MONS_JESSICA:
    case MONS_KOBOLD_DEMONOLOGIST:
    case MONS_OGRE_MAGE:
    case MONS_EROLCHA:
    case MONS_DRACONIAN:
    case MONS_RED_DRACONIAN:
    case MONS_WHITE_DRACONIAN:
    case MONS_GREEN_DRACONIAN:
    case MONS_PALE_DRACONIAN:
    case MONS_MOTTLED_DRACONIAN:
    case MONS_BLACK_DRACONIAN:
    case MONS_YELLOW_DRACONIAN:
    case MONS_PURPLE_DRACONIAN:
    case MONS_DRACONIAN_SHIFTER:
    case MONS_DRACONIAN_SCORCHER:
    case MONS_DRACONIAN_ANNIHILATOR:
    case MONS_DRACONIAN_CALLER:
    case MONS_DRACONIAN_MONK:
    case MONS_DRACONIAN_ZEALOT:
    case MONS_DRACONIAN_KNIGHT:
    case MONS_WIZARD:
    case MONS_ILSUIW:
    case MONS_MARA:
    case MONS_MERFOLK_AQUAMANCER:
    case MONS_SPRIGGAN:
        if (item_race == MAKE_ITEM_RANDOM_RACE)
            item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ROBE;
        break;

    case MONS_SPRIGGAN_DRUID:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ROBE;
        force_colour   = GREEN;
        break;

    case MONS_TIAMAT:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_GOLD_DRAGON_ARMOUR;
        break;

    case MONS_ORC_WIZARD:
    case MONS_BLORK_THE_ORC:
    case MONS_NERGALLE:
        item_race = MAKE_ITEM_ORCISH;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ROBE;
        break;

    case MONS_BORIS:
        level = MAKE_GOOD_ITEM;
        // fall-through
    case MONS_AGNES:
    case MONS_FRANCES:
    case MONS_FRANCIS:
    case MONS_NECROMANCER:
    case MONS_VAMPIRE_MAGE:
    case MONS_PIKEL:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ROBE;
        force_colour   = DARKGREY;
        break;

    case MONS_EUSTACHIO:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type = ARM_LEATHER_ARMOUR;
        break;

    case MONS_NESSOS:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_CENTAUR_BARDING;
        force_colour   = DARKGREY;
        break;

    case MONS_NIKOLA:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_CLOAK;
        force_colour   = LIGHTCYAN;
        break;

    default:
        return;
    }

    // Only happens if something in above switch doesn't set it. {dlb}
    if (item.base_type == OBJ_UNASSIGNED)
        return;

    const object_class_type xitc = item.base_type;
    const int xitt = item.sub_type;

    if (!force_item && mons_is_unique(mon->type) && level != MAKE_GOOD_ITEM)
    {
        if (x_chance_in_y(9 + mon->hit_dice, 100))
            level = MAKE_GOOD_ITEM;
        else
            level = level * 2 + 5;
    }

    // Note this mess, all the work above doesn't mean much unless
    // force_item is set... otherwise we're just going to take the base
    // and subtype and create a new item. - bwr
    const int thing_created =
        ((force_item) ? get_item_slot() : items(0, xitc, xitt, true,
                                                level, item_race));

    if (thing_created == NON_ITEM)
        return;

    // Copy temporary item into the item array if were forcing it, since
    // items() won't have done it for us.
    if (force_item)
        mitm[thing_created] = item;

    item_def &i = mitm[thing_created];

    if (force_item)
        item_set_appearance(i);

    _give_monster_item(mon, thing_created, force_item);

    // mv: All items with force_colour = 0 are colored via items().
    if (force_colour)
        mitm[thing_created].colour = force_colour;

    switch (mon->type)
    {
    case MONS_NIKOLA:
        mitm[thing_created].plus2 = TGLOV_DESC_GAUNTLETS;
        break;
    default:
        break;
    }
}

static void _give_gold(monsters *mon, int level)
{
    const int idx = items(0, OBJ_GOLD, 0, true, level, 0);
    _give_monster_item(mon, idx);
}

void give_item(int mid, int level_number, bool mons_summoned)
{
    monsters *mons = &menv[mid];

    if (mons->type == MONS_MAURICE)
        _give_gold(mons, level_number);

    _give_scroll(mons, level_number);
    _give_wand(mons, level_number);
    _give_potion(mons, level_number);

    const item_make_species_type item_race = _give_weapon(mons, level_number);

    _give_ammo(mons, level_number, item_race, mons_summoned);

    give_armour(mons, 1 + level_number / 2);
    give_shield(mons, 1 + level_number / 2);
}
