/**
 * @file
 * @brief Monsters' starting equipment.
**/

#include "AppHdr.h"

#include <algorithm>

#include "enum.h"
#include "externs.h"
#include "mon-gear.h"

#include "art-enum.h"
#include "artefact.h"
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
#include "state.h"
#include "tilepick.h"
#include "unwind.h"


static void _give_monster_item(monster* mon, int thing,
                               bool force_item = false,
                               bool (monster::*pickupfn)(item_def&, int) = NULL)
{
    if (thing == NON_ITEM || thing == -1)
        return;

    item_def &mthing = mitm[thing];
    ASSERT(mthing.defined());

    dprf(DIAG_MONPLACE, "Giving %s to %s...", mthing.name(DESC_PLAIN).c_str(),
         mon->name(DESC_PLAIN, true).c_str());

    mthing.pos.reset();
    mthing.link = NON_ITEM;

    if ((mon->undead_or_demonic() || mon->god == GOD_YREDELEMNUL)
        && (is_blessed(mthing)
            || get_weapon_brand(mthing) == SPWPN_HOLY_WRATH))
    {
        if (is_blessed(mthing))
            convert2bad(mthing);
        if (get_weapon_brand(mthing) == SPWPN_HOLY_WRATH)
            set_item_ego_type(mthing, OBJ_WEAPONS, SPWPN_NORMAL);
    }

    if (!is_artefact(mthing)
        && (mthing.base_type == OBJ_WEAPONS
         || mthing.base_type == OBJ_ARMOUR
         || mthing.base_type == OBJ_MISSILES))
    {
        bool enchanted = mthing.plus
                      || mthing.base_type == OBJ_WEAPONS && !mthing.plus2;

        // The item could either lose or gain brand after being generated,
        // adjust the glowing flag.
        if (!mthing.special && !enchanted)
            set_equip_desc(mthing, 0);
        else if (mthing.special && !get_equip_desc(mthing))
            set_equip_desc(mthing, ISFLAG_GLOWING);
    }

    unwind_var<int> save_speedinc(mon->speed_increment);
    if (!(pickupfn ? (mon->*pickupfn)(mthing, false)
                   : mon->pickup_item(mthing, false, true)))
    {
        dprf(DIAG_MONPLACE, "Destroying %s because %s doesn't want it!",
             mthing.name(DESC_PLAIN, false, true).c_str(),
             mon->name(DESC_PLAIN, true).c_str());
        destroy_item(thing, true);
        return;
    }
    if (!mthing.defined()) // missiles merged into an existing stack
        return;
    ASSERT(mthing.holding_monster() == mon);

    if (!force_item || mthing.colour == BLACK)
        item_colour(mthing);
}

void give_specific_item(monster* mon, const item_def& tpl)
{
    int thing = get_mitm_slot();
    if (thing == NON_ITEM)
        return;

    mitm[thing] = tpl;
    _give_monster_item(mon, thing, true, NULL);
}

static void _give_scroll(monster* mon, int level)
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

static void _give_wand(monster* mon, int level)
{
    if (!mons_is_unique(mon->type) || mons_class_flag(mon->type, M_NO_WAND))
        return;

    if (!one_chance_in(5) && (mon->type != MONS_MAURICE || !one_chance_in(3)))
        return;

    // Don't give top-tier wands before 5 HD, except to Ijyb and not in sprint.
    const bool no_high_tier =
            (mon->hit_dice < 5 || mons_class_flag(mon->type, M_NO_HT_WAND))
             && (mon->type != MONS_IJYB || crawl_state.game_is_sprint());

    while (1)
    {
        const int idx = items(0, OBJ_WANDS, OBJ_RANDOM, true, level, 0);

        if (idx == NON_ITEM)
            return;

        item_def& wand = mitm[idx];

        if (no_high_tier && is_high_tier_wand(wand.sub_type))
        {
            dprf(DIAG_MONPLACE,
                 "Destroying %s because %s doesn't want a high tier wand.",
                 wand.name(DESC_A).c_str(),
                 mon->name(DESC_THE).c_str());
            destroy_item(idx, true);
        }
        else
        {
            wand.flags = 0;
            _give_monster_item(mon, idx);
            break;
        }
    }
}

static void _give_potion(monster* mon, int level)
{
    if (mons_species(mon->type) == MONS_VAMPIRE
        && (one_chance_in(5) || mon->type == MONS_JORY))
    {
        // This handles initialization of stack timer.
        const int thing_created =
            items(0, OBJ_POTIONS, POT_BLOOD, true, level, 0);

        if (thing_created == NON_ITEM)
            return;

        mitm[thing_created].flags = 0;
        _give_monster_item(mon, thing_created);
    }
    else if (mon->type == MONS_GNOLL_SERGEANT && one_chance_in(3))
    {
        const int thing_created =
            items(0, OBJ_POTIONS, coinflip() ? POT_HEAL_WOUNDS
                                             : POT_CURING, true, level, 0);

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
                           &monster::pickup_potion);
    }
}

static item_def* make_item_for_monster(
    monster* mons,
    object_class_type base,
    int subtype,
    int level,
    item_make_species_type race,
    int allow_uniques,
    iflags_t flags);

static item_make_species_type _give_weapon(monster* mon, int level,
                                           bool melee_only = false,
                                           bool give_aux_melee = true,
                                           bool spectral_orcs = false)
{
    bool force_item = false;

    item_def               item;
    item_make_species_type item_race = MAKE_ITEM_RANDOM_RACE;
    int type = mon->type;

    item.base_type = OBJ_UNASSIGNED;

    if (mon->type == MONS_DANCING_WEAPON)
        level = MAKE_GOOD_ITEM;

    // moved setting of quantity here to keep it in mind {dlb}
    item.quantity = 1;

    if (spectral_orcs)
        type = mon->number;

    switch (type)
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
        else if (one_chance_in(30) && level > 2)
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = WPN_CROSSBOW;
            break;
        }
        else
            return item_race;
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
            return item_race;
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
            item.sub_type  = coinflip() ? WPN_DAGGER : WPN_CLUB;
        }
        else
            return item_race;
        break;

    case MONS_WIGHT:
    case MONS_NORRIS:
        item.base_type = OBJ_WEAPONS;

        if (one_chance_in(6))
        {
            item.sub_type = random_choose(WPN_MORNINGSTAR, WPN_GREAT_MACE,
                                          WPN_WAR_AXE,     WPN_TRIDENT,   -1);
        }
        else
        {
            item.sub_type = random_choose(
                WPN_MACE,      WPN_FLAIL,       WPN_FALCHION,
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
        item.sub_type = one_chance_in(3) ? WPN_DIRE_FLAIL : WPN_FLAIL;
        // "expensive" flail. {due}
        if (item.sub_type == WPN_FLAIL)
            level = MAKE_GOOD_ITEM;

        break;

    case MONS_DEEP_DWARF_DEATH_KNIGHT:
        item_race = MAKE_ITEM_DWARVEN;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = random_choose_weighted(5, WPN_MORNINGSTAR, 5, WPN_GREAT_MACE,
                                               5, WPN_HALBERD,     8, WPN_GLAIVE,
                                               5, WPN_GREAT_SWORD, 10, WPN_BROAD_AXE,
                                               15, WPN_BATTLEAXE, 0);
        if (coinflip())
        {
            force_item  = true;
            item.plus  += 1 + random2(4);
            item.plus2 += 1 + random2(4);
        }
        break;

    case MONS_FORMICID:
    case MONS_FORMICID_DRONE:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        if (one_chance_in(4))
            item.sub_type = coinflip() ? WPN_GIANT_CLUB : WPN_GIANT_SPIKED_CLUB;
        else
        {
            item.sub_type = random_choose_weighted(10, WPN_HAND_AXE,   15, WPN_GLAIVE,
                                                   10, WPN_MACE,       15, WPN_FLAIL,
                                                    5, WPN_GREAT_SWORD, 5, WPN_GREAT_MACE,
                                                    5, WPN_BATTLEAXE,   0);
        }
        if (coinflip())
        {
            force_item  = true;
            item.plus  += random2(3);
            item.plus2 += random2(3);
            if (one_chance_in(20))
                level = MAKE_GOOD_ITEM;
        }
        break;

    case MONS_DWARF:
    case MONS_DEEP_DWARF:
        item_race = MAKE_ITEM_DWARVEN;
        if (one_chance_in(9))
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = WPN_CROSSBOW;
            break;
        }
        // deliberate fall through
    case MONS_DEEP_DWARF_BERSERKER:
        item_race = MAKE_ITEM_DWARVEN;
        item.base_type = OBJ_WEAPONS;

        if (one_chance_in(6))
        {
            item.sub_type = random_choose_weighted(5, WPN_MORNINGSTAR, 5, WPN_GREAT_MACE,
                                                   5, WPN_GREAT_SWORD, 10, WPN_BROAD_AXE,
                                                   15, WPN_BATTLEAXE, 0);
        }
        else
        {
            item.sub_type = random_choose_weighted(5, WPN_FLAIL, 5, WPN_MACE,
                                                   5, WPN_SPEAR, 5, WPN_HALBERD,
                                                   5, WPN_GREAT_SWORD, 10, WPN_WAR_AXE,
                                                   15, WPN_HAND_AXE, 0);
        }

        if (coinflip() || mon->type == MONS_DEEP_DWARF_BERSERKER)
        {
            force_item  = true;
            item.plus  += 1 + random2(4);
            item.plus2 += 1 + random2(4);

            if (one_chance_in(30) && mon->type == MONS_DEEP_DWARF_BERSERKER)
                level = MAKE_GOOD_ITEM;
        }
        break;

    case MONS_GNOLL:
    case MONS_OGRE_MAGE:
    case MONS_NAGA_WARRIOR:
    case MONS_GREATER_NAGA:
        item_race = MAKE_ITEM_NO_RACE;
        if (!one_chance_in(5))
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = random_choose(WPN_SPEAR, WPN_SPEAR, WPN_HALBERD,
                                           WPN_CLUB,  WPN_WHIP,  WPN_FLAIL, -1);
        }
        break;

    case MONS_LAMIA:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = random_choose(WPN_HALBERD, WPN_GLAIVE, WPN_BARDICHE,
                                      -1);
        level = MAKE_GOOD_ITEM;
        break;

    case MONS_GNOLL_SHAMAN:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = coinflip() ? WPN_CLUB : WPN_WHIP;
        break;

    case MONS_GNOLL_SERGEANT:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = one_chance_in(3) ? WPN_TRIDENT : WPN_SPEAR;
        if (one_chance_in(3))
            level = MAKE_GOOD_ITEM;
        break;

    case MONS_PIKEL:
        force_item = true; // guaranteed flaming or elec
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_WHIP;
        set_item_ego_type(item, OBJ_WEAPONS, coinflip() ? SPWPN_ELECTROCUTION : SPWPN_FLAMING);
        item.plus  += random2(3);
        item.plus2 += random2(3);
        break;

    case MONS_GRUM:
        force_item = true; // guaranteed reaching
        item_race  = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = random_choose_weighted(3, WPN_SPEAR,
                                                1, WPN_HALBERD,
                                                1, WPN_GLAIVE,
                                                0);
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
        item.flags       |= ISFLAG_KNOW_TYPE;
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
    case MONS_FORMICID_VENOM_MAGE:
        if (!one_chance_in(5))
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = random_choose_weighted(
                35, WPN_CLUB,        30, WPN_DAGGER,
                30, WPN_FLAIL,       30, WPN_HAND_AXE,
                20, WPN_SHORT_SWORD,
                20, WPN_MACE,        15, WPN_WHIP,
                10, WPN_TRIDENT,     10, WPN_FALCHION,
                6, WPN_WAR_AXE,      3, WPN_MORNINGSTAR,
                0);
        }
        else
            return item_race;
        break;

    case MONS_TERENCE:
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = random_choose_weighted(30, WPN_FLAIL,
                                                20, WPN_HAND_AXE,
                                                20, WPN_SHORT_SWORD,
                                                20, WPN_MACE,
                                                10, WPN_TRIDENT,
                                                10, WPN_FALCHION,
                                                3, WPN_MORNINGSTAR,
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

    case MONS_ELF:
    case MONS_DEEP_ELF_FIGHTER:
    case MONS_DEEP_ELF_HIGH_PRIEST:
    case MONS_DEEP_ELF_KNIGHT:
    case MONS_DEEP_ELF_PRIEST:
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

    case MONS_NAGA:
    case MONS_NAGA_MAGE:
        item_race = MAKE_ITEM_NO_RACE;
        // deliberate fall-through {dlb}

    case MONS_ORC_WARRIOR:
    case MONS_ORC_HIGH_PRIEST:
    case MONS_BLORK_THE_ORC:
        if (item_race == MAKE_ITEM_RANDOM_RACE)
            item_race = MAKE_ITEM_ORCISH;
        // deliberate fall-through {dlb}

    case MONS_DANCING_WEAPON:   // give_level may have been adjusted above
    case MONS_SPECTRAL_WEAPON:  // Necessary for placement by mons spec
    case MONS_FRANCES:
    case MONS_HAROLD:
    case MONS_LOUISE:
    case MONS_SKELETAL_WARRIOR:
    case MONS_PALE_DRACONIAN:
    case MONS_RED_DRACONIAN:
    case MONS_WHITE_DRACONIAN:
    case MONS_GREEN_DRACONIAN:
    case MONS_MOTTLED_DRACONIAN:
    case MONS_BLACK_DRACONIAN:
    case MONS_YELLOW_DRACONIAN:
    case MONS_PURPLE_DRACONIAN:
    case MONS_GREY_DRACONIAN:
    case MONS_TENGU:
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = random_choose_weighted(
            10, WPN_LONG_SWORD, 10, WPN_SHORT_SWORD,
            10, WPN_SCIMITAR,   10, WPN_BATTLEAXE,
            10, WPN_HAND_AXE,   10, WPN_HALBERD,
            10, WPN_GLAIVE,     10, WPN_MACE,
            10, WPN_GREAT_MACE, 10, WPN_TRIDENT,
            9,  WPN_WAR_AXE,    9, WPN_FLAIL,
            1,  WPN_BROAD_AXE,  1, WPN_MORNINGSTAR,
            0);
        break;

    case MONS_TIAMAT:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = random_choose(WPN_BARDICHE, WPN_DEMON_TRIDENT,
                                      WPN_GLAIVE, -1);
        level = MAKE_GOOD_ITEM;
        break;


    case MONS_RUPERT:
        item.base_type = OBJ_WEAPONS;
        // Rupert favours big two-handers with visceral up-close
        // effects, i.e. no polearms.
        item.sub_type = random_choose_weighted(10, WPN_GREAT_MACE,
                                               6, WPN_GREAT_SWORD,
                                               2, WPN_TRIPLE_SWORD,
                                               8, WPN_BATTLEAXE,
                                               2, WPN_EXECUTIONERS_AXE,
                                               0);
        level = MAKE_GOOD_ITEM;
        break;

    case MONS_WIGLAF:
        item_race = MAKE_ITEM_DWARVEN;

        item.base_type = OBJ_WEAPONS;
        // speech references an axe
        item.sub_type  = random_choose(WPN_WAR_AXE, WPN_BROAD_AXE,
                                       WPN_BATTLEAXE, -1);
        break;

    case MONS_JORGRUN:
        force_item = true;
        if (one_chance_in(3))
        {
            item.base_type = OBJ_STAVES;
            item.sub_type = STAFF_EARTH;
        }
        else
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type = WPN_QUARTERSTAFF;
            set_item_ego_type(item, OBJ_WEAPONS, SPWPN_VORPAL);
            set_equip_race(item, ISFLAG_DWARVEN);
        }
        item.flags |= ISFLAG_KNOW_TYPE;
        break;

    case MONS_TENGU_REAVER:
    case MONS_VAULT_WARDEN:
        item_race = MAKE_ITEM_NO_RACE;
    case MONS_ORC_WARLORD:
    case MONS_SAINT_ROKA:
        // being at the top has its privileges
        if (one_chance_in(3))
            level = MAKE_GOOD_ITEM;
        // deliberate fall-through

    case MONS_ORC_KNIGHT:
        if (item_race == MAKE_ITEM_RANDOM_RACE)
            item_race = MAKE_ITEM_ORCISH;
        // deliberate fall-through

    case MONS_TENGU_WARRIOR:
        if (item_race == MAKE_ITEM_RANDOM_RACE)
            item_race = MAKE_ITEM_NO_RACE;
        // Occasionally get crossbows, or a longbow for tengu.
        if (!melee_only && mon->type != MONS_TENGU_REAVER && one_chance_in(9))
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = (mon->type == MONS_TENGU_WARRIOR && coinflip())
                             ? WPN_LONGBOW : WPN_CROSSBOW;
            break;
        }
        // deliberate fall-through

    case MONS_URUG:
        if (item_race == MAKE_ITEM_RANDOM_RACE)
            item_race = MAKE_ITEM_ORCISH;
        // deliberate fall-through

    case MONS_VAULT_GUARD:
    case MONS_VAMPIRE_KNIGHT:
    case MONS_DRACONIAN_KNIGHT:
    case MONS_JORY:
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

    case MONS_VAULT_SENTINEL:
        if (!melee_only && one_chance_in(3))
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = WPN_CROSSBOW;
            break;
        }

        item.base_type = OBJ_WEAPONS;

        item.sub_type = random_choose_weighted(
            5, WPN_LONG_SWORD,   4, WPN_FALCHION,
            3, WPN_WAR_AXE,      3, WPN_MORNINGSTAR,
            0);

        break;

    case MONS_IRONBRAND_CONVOKER:
    case MONS_IRONHEART_PRESERVER:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = random_choose_weighted(
            3, WPN_GREAT_MACE,  2, WPN_DIRE_FLAIL,
            2, WPN_FLAIL,       2, WPN_MORNINGSTAR,
            1, WPN_MACE,
            0);

        if (mon->type == MONS_IRONHEART_PRESERVER && one_chance_in(3))
        {
            force_item = true;
            set_item_ego_type(item, OBJ_WEAPONS, SPWPN_PROTECTION);
        }
        break;

    case MONS_CYCLOPS:
    case MONS_STONE_GIANT:
        item.base_type = OBJ_MISSILES;
        item.sub_type  = MI_LARGE_ROCK;
        break;

    case MONS_TWO_HEADED_OGRE:
    case MONS_ETTIN:
    case MONS_IRON_GIANT:
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
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = (one_chance_in(5) ? WPN_DEMON_TRIDENT
                                           : WPN_TRIDENT);
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
        force_item     = true;
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_TRIDENT;
        item.plus      = random_range(-1, 6, 2);
        item.plus2     = random_range(-1, 6, 2);
        item.flags    |= ISFLAG_KNOW_TYPE;
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_FREEZING);
        break;

    case MONS_MERFOLK_IMPALER:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = random_choose_weighted(100, WPN_TRIDENT,
                                               15, WPN_DEMON_TRIDENT,
                                               0);
        if (!one_chance_in(3))
            level = MAKE_GOOD_ITEM;
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

    case MONS_OCTOPODE:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = random_choose_weighted(100, WPN_TRIDENT,
                                               15, WPN_DEMON_TRIDENT,
                                               50, WPN_SPEAR,
                                               30, WPN_FALCHION,
                                               80, WPN_LONG_SWORD,
                                               30, WPN_SCIMITAR,
                                               20, WPN_FLAIL,
                                               20, WPN_MORNINGSTAR,
                                               0);
        if (one_chance_in(3))
            level = MAKE_GOOD_ITEM;
        break;

    case MONS_CENTAUR:
    case MONS_CENTAUR_WARRIOR:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_BOW;
        if (mon->type == MONS_CENTAUR_WARRIOR && one_chance_in(3))
            item.sub_type = WPN_LONGBOW;
        break;

    case MONS_FAUN:
    case MONS_SATYR:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        if (!melee_only)
        {
            switch (mon->type)
            {
                case MONS_FAUN:
                default:
                    item.sub_type = WPN_SLING;
                    break;
                case MONS_SATYR:
                    item.sub_type = random_choose_weighted(3, WPN_LONGBOW,
                                                           1, WPN_BOW,
                                                           1, WPN_SLING,
                                                           0);
                    break;
            }
        }
        else
        {
            item.sub_type = random_choose(WPN_SPEAR,
                                          WPN_CLUB,
                                          WPN_QUARTERSTAFF,
                                          -1);
        }
        break;

    case MONS_NESSOS:
        force_item     = true;
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_LONGBOW;
        item.plus     += 1 + random2(3);
        item.plus2    += 1 + random2(3);
        item.flags    |= ISFLAG_KNOW_TYPE;
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_FLAME);
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
        item.flags    |= ISFLAG_KNOW_TYPE;
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_FLAMING);
        break;

    case MONS_ANGEL:
        force_item     = true;
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;

        item.sub_type  = (one_chance_in(4) ? WPN_SACRED_SCOURGE
                                           : WPN_WHIP);

        set_equip_desc(item, ISFLAG_GLOWING);
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_HOLY_WRATH);
        item.plus  = 1 + random2(3);
        item.plus2 = 1 + random2(3);
        item.flags |= ISFLAG_KNOW_TYPE;
        break;

    case MONS_CHERUB:
        item_race  = MAKE_ITEM_NO_RACE;
        if (!melee_only)
        {
            item.base_type  = OBJ_WEAPONS;
            item.sub_type  = random_choose(WPN_SLING,
                                           WPN_BOW,
                                           WPN_LONGBOW,
                                           -1);
            break;
        }
        force_item     = true;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = random_choose(WPN_FLAIL,
                                       WPN_LONG_SWORD,
                                       WPN_SCIMITAR,
                                       WPN_FALCHION,
                                       -1);
        item.plus  = random2(5);
        item.plus2 = random2(5);
        // flaming instead of holy wrath
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_FLAMING);
        item.flags |= ISFLAG_KNOW_TYPE;
        break;

    case MONS_SERAPH:
        item_race  = MAKE_ITEM_NO_RACE;
        force_item     = true;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_GREAT_SWORD;
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_FLAMING);
        // highly enchanted, we're top rank
        item.plus  = 3 + random2(6);
        item.plus2 = 3 + random2(6);
        item.flags |= ISFLAG_KNOW_TYPE;
        break;

    case MONS_DAEVA:
    case MONS_MENNAS:
        force_item     = true;
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;

        item.sub_type  = random_choose(WPN_EUDEMON_BLADE,
                                       WPN_BLESSED_LONG_SWORD,
                                       WPN_BLESSED_SCIMITAR,
                                       WPN_BLESSED_FALCHION,
                                       -1);

        set_equip_desc(item, ISFLAG_GLOWING);
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_HOLY_WRATH);
        item.plus  = 1 + random2(3);
        item.plus2 = 1 + random2(3);
        item.flags |= ISFLAG_KNOW_TYPE;
        break;

    case MONS_PROFANE_SERVITOR:
        force_item     = true;
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;

        item.sub_type  = (one_chance_in(4) ? WPN_DEMON_WHIP
                                           : WPN_WHIP);

        set_equip_desc(item, ISFLAG_GLOWING);
        item.plus  = 1 + random2(3);
        item.plus2 = 1 + random2(3);
        item.flags |= ISFLAG_KNOW_TYPE;
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
                              random_choose_weighted(13, SPWPN_FLAMING,
                                                     4, SPWPN_DRAINING,
                                                     4, SPWPN_VORPAL,
                                                     2, SPWPN_DISTORTION,
                                                     2, SPWPN_PAIN,
                                                     0));
        }

        item.plus  += random2(6);
        item.plus2 += random2(6);
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
                              random_choose_weighted(13, SPWPN_FLAMING,
                                                     4, SPWPN_DRAINING,
                                                     4, SPWPN_VORPAL,
                                                     2, SPWPN_DISTORTION,
                                                     2, SPWPN_PAIN,
                                                     0));
        }

        item.plus  += random2(6);
        item.plus2 += random2(6);
        break;

    case MONS_FIRE_GIANT:
        force_item = true;
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_GREAT_SWORD;
        item.plus      = 0;
        item.plus2     = 0;
        item.flags    |= ISFLAG_KNOW_TYPE;
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_FLAMING);
        break;

    case MONS_FROST_GIANT:
        force_item = true;
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_BATTLEAXE;
        item.plus      = 0;
        item.plus2     = 0;
        item.flags    |= ISFLAG_KNOW_TYPE;
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_FREEZING);
        break;

    case MONS_ORC_WIZARD:
    case MONS_ORC_SORCERER:
    case MONS_NERGALLE:
        item_race = MAKE_ITEM_ORCISH;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_DAGGER;
        break;

    case MONS_UNBORN:
        if (one_chance_in(6))
            level = MAKE_GOOD_ITEM;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_HAND_AXE;
        break;

    case MONS_DOWAN:
        item_race = MAKE_ITEM_ELVEN;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_DAGGER;
        break;

    case MONS_FANNAR:
        force_item = true;
        if (one_chance_in(3))
        {
            item.base_type = OBJ_STAVES;
            item.sub_type = STAFF_COLD;
        }
        else
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type = WPN_QUARTERSTAFF;
            set_item_ego_type(item, OBJ_WEAPONS, SPWPN_FREEZING);
            set_equip_race(item, ISFLAG_ELVEN);
            // this might not be the best place for this logic, but:
            make_item_for_monster(mon, OBJ_JEWELLERY, RING_ICE,
                                  0, MAKE_ITEM_NO_RACE, 1, ISFLAG_KNOW_TYPE);
        }
        item.flags |= ISFLAG_KNOW_TYPE;
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
            item.plus  = random2(5);
            item.plus2 = random2(5);
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
        item.flags    |= ISFLAG_KNOW_TYPE;
        break;

    case MONS_ARACHNE:
        force_item = true;
        item.base_type = OBJ_STAVES;
        item.sub_type = STAFF_POISON;
        item.flags    |= ISFLAG_KNOW_TYPE;
        if (one_chance_in(100) && !get_unique_item_status(UNRAND_OLGREB))
            make_item_unrandart(item, UNRAND_OLGREB);
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
        item.flags |= ISFLAG_KNOW_TYPE;
        break;

    case MONS_SPRIGGAN:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        // no quick blades for mooks
        item.sub_type  = random_choose(WPN_DAGGER, WPN_SHORT_SWORD,
                                       WPN_SABRE, -1);
        break;

    case MONS_SPRIGGAN_ASSASSIN:
        if (!melee_only)
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = WPN_BLOWGUN;
            item_race = MAKE_ITEM_NO_RACE;
            break;
        }
        // deliberate fall-through
    case MONS_SPRIGGAN_ENCHANTER:
        item_race = MAKE_ITEM_NO_RACE;
        if (one_chance_in(3))
            level = MAKE_GOOD_ITEM;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = one_chance_in(12) ? WPN_QUICK_BLADE : WPN_DAGGER;
        break;

    case MONS_SPRIGGAN_RIDER:
        if (!melee_only && one_chance_in(4))
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = WPN_BLOWGUN;
            item_race = MAKE_ITEM_NO_RACE;
            break;
        }
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_SPEAR;
        break;

    case MONS_SPRIGGAN_BERSERKER:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = random_choose(WPN_HAND_AXE,
                                       WPN_MACE,
                                       WPN_SHORT_SWORD,
                                       -1);
        if (one_chance_in(4))
        {
            force_item = true;
            set_item_ego_type(item, OBJ_WEAPONS, SPWPN_ANTIMAGIC);
        }

        break;

    case MONS_SPRIGGAN_DRUID:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_QUARTERSTAFF;
        break;

    case MONS_SPRIGGAN_DEFENDER:
    case MONS_THE_ENCHANTRESS:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = random_choose(WPN_LAJATANG,    // best spriggan weapon
                                       WPN_QUICK_BLADE, // overrated
                                       WPN_SABRE,       // ordinary but popular
                                       WPN_DEMON_WHIP,  // goodness
                                       WPN_FLAIL,       // best ordinary 1-handed
                                       -1);
        level = MAKE_GOOD_ITEM;
        if (mon->type == MONS_THE_ENCHANTRESS && one_chance_in(6))
        {
            force_item = true;
            set_item_ego_type(item, OBJ_WEAPONS, SPWPN_DISTORTION);
            item.plus      = random2(5);
            item.plus2     = random2(5);
        }
        break;

    case MONS_IGNACIO:
        force_item = true;
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_EXECUTIONERS_AXE;
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_PAIN);
        item.plus      = 2 + random2(7);
        item.plus2     = 2 + random2(7);
        item.flags    |= ISFLAG_KNOW_TYPE;
        break;

    case MONS_HELLBINDER:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type  = WPN_DEMON_BLADE;
        break;

    case MONS_ANCIENT_CHAMPION:
        force_item = true;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = random_choose(WPN_GREAT_MACE,
                                      WPN_BATTLEAXE,
                                      WPN_GREAT_SWORD,
                                      -1);

        if (x_chance_in_y(2, 3))
        {
            set_item_ego_type(item, OBJ_WEAPONS,
                              random_choose_weighted(12, SPWPN_DRAINING,
                                                      7, SPWPN_VORPAL,
                                                      4, SPWPN_FREEZING,
                                                      4, SPWPN_FLAMING,
                                                      2, SPWPN_PAIN,
                                                      0));
        }

        item.plus  += random2(4);
        item.plus2 += random2(4);
        break;

    case MONS_SOJOBO:
        item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_WEAPONS;
        item.sub_type = one_chance_in(6) ? WPN_TRIPLE_SWORD
                                         : WPN_GREAT_SWORD;
        if (x_chance_in_y(2, 3))
        {
            force_item = true;
            set_item_ego_type(item, OBJ_WEAPONS, SPWPN_ELECTROCUTION);
        }
        level = MAKE_GOOD_ITEM;
        break;

    default:
        break;
    }

    // Only happens if something in above switch doesn't set it. {dlb}
    if (item.base_type == OBJ_UNASSIGNED)
        return item_race;

    if (!force_item && mons_is_unique(mon->type))
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
        ((force_item) ? get_mitm_slot() : items(0, xitc, xitt, true,
                                                level, item_race));

    if (thing_created == NON_ITEM)
        return item_race;

    // Copy temporary item into the item array if were forcing it, since
    // items() won't have done it for us.
    if (force_item)
        mitm[thing_created] = item;

    item_def &i = mitm[thing_created];
    if (melee_only && (i.base_type != OBJ_WEAPONS || is_range_weapon(i)))
    {
        destroy_item(thing_created);
        return item_race;
    }

    if (force_item)
        item_set_appearance(i);

    _give_monster_item(mon, thing_created, force_item);

    if (give_aux_melee &&
        ((i.base_type != OBJ_WEAPONS && i.base_type != OBJ_STAVES)
        || is_range_weapon(i)))
    {
        _give_weapon(mon, level, true, false);
    }

    return item_race;
}

// Hands out ammunition fitting the monster's launcher (if any), or else any
// throwable missiles depending on the monster type.
static void _give_ammo(monster* mon, int level,
                       item_make_species_type item_race,
                       bool mons_summoned)
{
    // Note that item_race is not reset for this section.
    if (const item_def *launcher = mon->launcher())
    {
        const object_class_type xitc = OBJ_MISSILES;
        int xitt = fires_ammo_type(*launcher);

        if (xitt == MI_STONE
            && (mon->type == MONS_JOSEPH
                || mon->type == MONS_SATYR
                || (mon->type == MONS_FAUN && one_chance_in(3))
                || one_chance_in(15)))
        {
                xitt = MI_SLING_BULLET;
        }

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
            else if (mon->type == MONS_SPRIGGAN_ASSASSIN)
            {

                special_missile_type brand = random_choose_weighted(5, SPMSL_SLEEP,
                                                              3, SPMSL_CONFUSION,
                                                              2, SPMSL_PARALYSIS,
                                                              2, SPMSL_CURARE,
                                                              1, SPMSL_FRENZY,
                                                              0);
                switch (brand)
                {
                    case SPMSL_CONFUSION:
                    case SPMSL_PARALYSIS:
                    case SPMSL_FRENZY:
                        mitm[thing_created].quantity = random_range(3, 7);
                        break;
                    case SPMSL_CURARE:
                        mitm[thing_created].quantity = random_range(2, 5);
                        break;
                    case SPMSL_SLEEP: default:
                        mitm[thing_created].quantity = random_range(4, 9);
                        break;
                }
                set_item_ego_type(mitm[thing_created], OBJ_MISSILES,
                                    brand);

            }
            else
            {
                set_item_ego_type(mitm[thing_created], OBJ_MISSILES,
                                  got_curare_roll(level) ? SPMSL_CURARE
                                                         : SPMSL_POISONED);

                if (get_ammo_brand(mitm[thing_created]) == SPMSL_CURARE)
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
                break;

            default:
                break;
        }

        _give_monster_item(mon, thing_created);
    }
    else
    {
        // Give some monsters throwables.
        int weap_type = -1;
        int qty = 0;
        switch (mon->type)
        {
        case MONS_KOBOLD:
        case MONS_BIG_KOBOLD:
            if (x_chance_in_y(2, 5))
            {
                item_race  = MAKE_ITEM_NO_RACE;
                weap_type  = MI_DART;
                qty = 1 + random2(5);
            }
            break;

        case MONS_ORC_WARRIOR:
            if (one_chance_in(
                    player_in_branch(BRANCH_ORC)? 9 : 20))
            {
                weap_type = MI_TOMAHAWK;
                qty       = random_range(4, 8);
            }
            break;

        case MONS_ORC:
            if (one_chance_in(20))
            {
                weap_type = MI_TOMAHAWK;
                qty       = random_range(2, 5);
            }
            break;

        case MONS_URUG:
            weap_type  = MI_JAVELIN;
            qty = random_range(4, 7);
            break;

        case MONS_CHUCK:
            weap_type  = MI_LARGE_ROCK;
            qty = 2;
            break;

        case MONS_POLYPHEMUS:
            weap_type  = MI_LARGE_ROCK;
            qty        = random_range(8, 12);
            break;

        case MONS_MERFOLK_JAVELINEER:
            weap_type  = MI_JAVELIN;
            qty        = random_range(9, 23, 2);
            if (one_chance_in(3))
                level = MAKE_GOOD_ITEM;
            break;

        case MONS_MERFOLK:
            if (one_chance_in(3)
                || active_monster_band == BAND_MERFOLK_JAVELINEER)
            {
                weap_type  = MI_TOMAHAWK;
                qty        = random_range(4, 8);
                if (active_monster_band == BAND_MERFOLK_JAVELINEER)
                    break;
            }
            if (one_chance_in(6) && !mons_summoned)
            {
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

        case MONS_HAROLD: // bounty hunter, up to 5 nets
            if (mons_summoned)
                break;

            weap_type  = MI_THROWING_NET;
            qty        = 1;
            if (one_chance_in(3))
                qty++;
            if (mon->type == MONS_HAROLD)
                qty += random2(4);

            break;

        default:
            break;
        }

        if (weap_type == -1)
            return;

        const int thing_created =
            items(0, OBJ_MISSILES, weap_type, true, level, item_race);

        if (thing_created != NON_ITEM)
        {
            item_def& w(mitm[thing_created]);

            if (mon->type == MONS_CHUCK)
                set_item_ego_type(w, OBJ_MISSILES, SPMSL_RETURNING);

            w.quantity = qty;
            _give_monster_item(mon, thing_created, false,
                               &monster::pickup_missile);
        }
    }
}

static item_def* make_item_for_monster(
    monster* mons,
    object_class_type base,
    int subtype,
    int level,
    item_make_species_type race = MAKE_ITEM_NO_RACE,
    int allow_uniques = 0,
    iflags_t flags = 0)
{
    const int bp = get_mitm_slot();
    if (bp == NON_ITEM)
        return 0;

    const int thing_created =
        items(allow_uniques, base, subtype, true, level, race);

    if (thing_created == NON_ITEM)
        return 0;

    mitm[thing_created].flags |= flags;

    _give_monster_item(mons, thing_created);
    return &mitm[thing_created];
}

static void _give_shield(monster* mon, int level)
{
    const item_def *main_weap = mon->mslot_item(MSLOT_WEAPON);
    const item_def *alt_weap  = mon->mslot_item(MSLOT_ALT_WEAPON);
    item_def *shield;

    // If the monster is already wielding/carrying a two-handed weapon,
    // it doesn't get a shield.  (Monsters always prefer raw damage to
    // protection!)
    if (main_weap && mon->hands_reqd(*main_weap) == HANDS_TWO
        || alt_weap && mon->hands_reqd(*alt_weap) == HANDS_TWO)
    {
        return;
    }

    switch (mon->type)
    {
    case MONS_DAEVA:
    case MONS_MENNAS:
        make_item_for_monster(mon, OBJ_ARMOUR, ARM_LARGE_SHIELD,
                              level * 2 + 1, MAKE_ITEM_NO_RACE, 1);
        break;

    case MONS_CHERUB:
        if (main_weap && main_weap->base_type == OBJ_WEAPONS
            && main_weap->sub_type == WPN_SLING)
        {
            // Big shields interfere with ranged combat, at least theme-wise.
            make_item_for_monster(mon, OBJ_ARMOUR, ARM_BUCKLER,
                                  level, MAKE_ITEM_NO_RACE, 1);
        }
        break;

    case MONS_DEEP_ELF_FIGHTER:
        if (one_chance_in(6))
        {
            make_item_for_monster(mon, OBJ_ARMOUR, ARM_BUCKLER,
                                  level, MAKE_ITEM_ELVEN);
        }
        break;
    case MONS_NAGA_WARRIOR:
    case MONS_VAULT_GUARD:
    case MONS_VAULT_WARDEN:
        if (one_chance_in(3))
        {
            make_item_for_monster(mon, OBJ_ARMOUR,
                                  one_chance_in(3) ? ARM_LARGE_SHIELD
                                                   : ARM_SHIELD,
                                  level, MAKE_ITEM_NO_RACE);
        }
        break;
    case MONS_OCTOPODE:
        if (coinflip())
        {
            make_item_for_monster(mon, OBJ_ARMOUR, ARM_SHIELD,
                                  level, MAKE_ITEM_NO_RACE);
        }
        break;
    case MONS_DRACONIAN_KNIGHT:
    case MONS_IRONHEART_PRESERVER:
        if (coinflip())
        {
            make_item_for_monster(mon, OBJ_ARMOUR,
                                  coinflip() ? ARM_LARGE_SHIELD : ARM_SHIELD,
                                  level, MAKE_ITEM_NO_RACE);
        }
        break;
    case MONS_TENGU_WARRIOR:
        if (one_chance_in(3))
            level = MAKE_GOOD_ITEM;
        // deliberate fall-through
    case MONS_TENGU:
    case MONS_GNOLL_SERGEANT:
        if (mon->type != MONS_TENGU_WARRIOR && !one_chance_in(3))
            break;
        make_item_for_monster(mon, OBJ_ARMOUR,
                              coinflip() ? ARM_BUCKLER : ARM_SHIELD,
                              level, MAKE_ITEM_NO_RACE);
        break;
    case MONS_TENGU_REAVER:
        if (one_chance_in(3))
            level = MAKE_GOOD_ITEM;
        // deliberate fall-through
    case MONS_TENGU_CONJURER:
        if (mon->type == MONS_TENGU_REAVER || one_chance_in(3))
        {
            make_item_for_monster(mon, OBJ_ARMOUR, ARM_BUCKLER,
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
    case MONS_SPRIGGAN_RIDER:
    case MONS_SPRIGGAN_ASSASSIN:
    case MONS_SPRIGGAN_ENCHANTER:
        if (!one_chance_in(4))
            break;
    case MONS_SPRIGGAN_DEFENDER:
    case MONS_THE_ENCHANTRESS:
        shield = make_item_for_monster(mon, OBJ_ARMOUR, ARM_BUCKLER,
                      mon->type == MONS_THE_ENCHANTRESS ? MAKE_GOOD_ITEM :
                      mon->type == MONS_SPRIGGAN_DEFENDER ? level * 2 + 1 :
                      level, MAKE_ITEM_NO_RACE);
        if (shield && !is_artefact(*shield)) // ineligible...
        {
            shield->props["item_tile_name"] = "buckler_spriggan";
            shield->props["worn_tile_name"] = "buckler_spriggan";
            bind_item_tile(*shield);
        }
        break;
    case MONS_NORRIS:
        make_item_for_monster(mon, OBJ_ARMOUR, ARM_BUCKLER,
                              level * 2 + 1, MAKE_ITEM_RANDOM_RACE, 1);
        break;
    case MONS_WIGLAF:
        make_item_for_monster(mon, OBJ_ARMOUR, ARM_SHIELD,
                              level * 2 + 1, MAKE_ITEM_DWARVEN, 1);
        break;
    case MONS_LOUISE:
        shield = make_item_for_monster(mon, OBJ_ARMOUR, ARM_LARGE_SHIELD,
                              level * 2 + 1, MAKE_ITEM_RANDOM_RACE, 1);
        if (shield && !is_artefact(*shield))
        {
            shield->props["item_tile_name"] = "lshield_louise";
            shield->props["worn_tile_name"] = "lshield_louise";
            bind_item_tile(*shield);
        }
        break;
    case MONS_DONALD:
        shield = make_item_for_monster(mon, OBJ_ARMOUR, ARM_SHIELD,
                              level * 2 + 1, MAKE_ITEM_RANDOM_RACE, 1);

        if (shield)
        {
            if (coinflip())
            {
                set_item_ego_type(*shield, OBJ_ARMOUR, SPARM_REFLECTION);
                set_equip_desc(*shield, ISFLAG_GLOWING);
            }
            if (!is_artefact(*shield))
            {
                shield->props["item_tile_name"] = "shield_donald";
                shield->props["worn_tile_name"] = "shield_donald";
                bind_item_tile(*shield);
            }
        }

        break;
    case MONS_NIKOLA:
        shield = make_item_for_monster(mon, OBJ_ARMOUR, ARM_GLOVES,
                              level * 2 + 1, MAKE_ITEM_NO_RACE, 1);

        if (shield) // gauntlets
        {
            if (get_armour_ego_type(*shield) == SPARM_ARCHERY)
                set_item_ego_type(*shield, OBJ_ARMOUR, SPARM_NORMAL);
            shield->plus2 = TGLOV_DESC_GAUNTLETS;
        }
        break;

    case MONS_FORMICID:
        if (!one_chance_in(4))
            break;
    case MONS_FORMICID_DRONE:
        if (one_chance_in(3))
        {
            armour_type shield_type = coinflip() ? ARM_BUCKLER : ARM_SHIELD;

            shield = make_item_for_monster(mon, OBJ_ARMOUR, shield_type,
                                           level, MAKE_ITEM_NO_RACE);
        }
        break;
    default:
        break;
    }
}

static void _give_armour(monster* mon, int level, bool spectral_orcs)
{
    item_def               item;
    item_make_species_type item_race = MAKE_ITEM_RANDOM_RACE;

    item.base_type = OBJ_UNASSIGNED;
    item.quantity  = 1;

    bool force_item = false;
    int type = mon->type;

    if (spectral_orcs)
        type = mon->number;

    switch (type)
    {
    case MONS_DEEP_ELF_BLADEMASTER:
    case MONS_DEEP_ELF_MASTER_ARCHER:
        item_race      = MAKE_ITEM_ELVEN;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_LEATHER_ARMOUR;
        break;

    case MONS_DUVESSA:
    case MONS_ELF:
    case MONS_DEEP_ELF_ANNIHILATOR:
    case MONS_DEEP_ELF_CONJURER:
    case MONS_DEEP_ELF_DEATH_MAGE:
    case MONS_DEEP_ELF_DEMONOLOGIST:
    case MONS_DEEP_ELF_FIGHTER:
    case MONS_DEEP_ELF_HIGH_PRIEST:
    case MONS_DEEP_ELF_KNIGHT:
    case MONS_DEEP_ELF_MAGE:
    case MONS_DEEP_ELF_PRIEST:
    case MONS_DEEP_ELF_SORCERER:
    case MONS_DEEP_ELF_SUMMONER:
        if (item_race == MAKE_ITEM_RANDOM_RACE)
            item_race = MAKE_ITEM_ELVEN;
        // deliberate fall through {dlb}

    case MONS_IJYB:
    case MONS_ORC:
    case MONS_ORC_HIGH_PRIEST:
    case MONS_ORC_PRIEST:
        if (item_race == MAKE_ITEM_RANDOM_RACE)
            item_race = MAKE_ITEM_ORCISH;
        if (x_chance_in_y(2, 5))
        {
            item.base_type = OBJ_ARMOUR;
            item.sub_type  = random_choose_weighted(4, ARM_LEATHER_ARMOUR,
                                                    2, ARM_RING_MAIL,
                                                    1, ARM_SCALE_MAIL,
                                                    1, ARM_CHAIN_MAIL,
                                                    0);
        }
        else
            return;
        break;

    case MONS_ERICA:
    case MONS_JOSEPHINE:
    case MONS_PSYCHE:
        if (one_chance_in(5))
            level = MAKE_GOOD_ITEM;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ROBE;
        item_race      = MAKE_ITEM_NO_RACE;
        break;

    case MONS_HAROLD:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_RING_MAIL;
        item_race      = MAKE_ITEM_NO_RACE;
        break;

    case MONS_GNOLL_SHAMAN:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = coinflip() ? ARM_ROBE : ARM_LEATHER_ARMOUR;
        break;

    case MONS_SOJOBO:
        level = MAKE_GOOD_ITEM;
        // deliberate fall-through
    case MONS_GNOLL_SERGEANT:
    case MONS_TENGU_REAVER:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = coinflip() ? ARM_RING_MAIL : ARM_SCALE_MAIL;
        if (type == MONS_TENGU_REAVER && one_chance_in(3))
            level = MAKE_GOOD_ITEM;
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
    case MONS_SPRIGGAN_BERSERKER:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ANIMAL_SKIN;
        break;

    case MONS_URUG:
        item_race = MAKE_ITEM_ORCISH;
        // deliberate fall through {dlb}

    case MONS_EDMUND:
    case MONS_RUPERT:
    {
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = random_choose(ARM_LEATHER_ARMOUR, ARM_RING_MAIL,
                                       ARM_SCALE_MAIL,     ARM_CHAIN_MAIL, -1);
        break;
    }

    case MONS_WIGLAF:
        if (one_chance_in(3))
            level = MAKE_GOOD_ITEM;
        item_race      = MAKE_ITEM_DWARVEN;
        item.base_type = OBJ_ARMOUR;
        item.sub_type = random_choose_weighted(8, ARM_CHAIN_MAIL,
                                               10, ARM_PLATE_ARMOUR,
                                               1, ARM_CRYSTAL_PLATE_ARMOUR,
                                               0);
        break;

    case MONS_JORGRUN:
        if (one_chance_in(3))
            level = MAKE_GOOD_ITEM;
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ROBE;
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

    case MONS_HELL_KNIGHT:
    case MONS_LOUISE:
    case MONS_DONALD:
    case MONS_MAUD:
    case MONS_VAMPIRE_KNIGHT:
    case MONS_JORY:
    case MONS_VAULT_GUARD:
    case MONS_VAULT_WARDEN:
    case MONS_IRONHEART_PRESERVER:
    case MONS_ANCIENT_CHAMPION:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = random_choose(ARM_CHAIN_MAIL, ARM_PLATE_ARMOUR, -1);
        break;

    case MONS_VAULT_SENTINEL:
    case MONS_IRONBRAND_CONVOKER:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = random_choose(ARM_RING_MAIL,   ARM_SCALE_MAIL, -1);
        break;

    case MONS_FREDERICK:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = random_choose(ARM_SCALE_MAIL,   ARM_CHAIN_MAIL, -1);
        break;

    case MONS_MARGERY:
        item.base_type = OBJ_ARMOUR;
        item.sub_type = random_choose_weighted(3, ARM_MOTTLED_DRAGON_ARMOUR,
                                               1, ARM_SWAMP_DRAGON_ARMOUR,
                                               6, ARM_FIRE_DRAGON_ARMOUR,
                                               0);
        break;

    case MONS_UNBORN:
        if (one_chance_in(6))
            level = MAKE_GOOD_ITEM;

    // deliberate fall through
    case MONS_FORMICID_VENOM_MAGE:
    case MONS_HELLBINDER:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ROBE;
        break;

    case MONS_FORMICID:
    case MONS_FORMICID_DRONE:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = random_choose_weighted(5, ARM_SCALE_MAIL,
                                                3, ARM_CHAIN_MAIL,
                                                1, ARM_PLATE_ARMOUR,
                                                0);
        break;

    case MONS_DWARF:
    case MONS_DEEP_DWARF:
    case MONS_DEEP_DWARF_DEATH_KNIGHT:
    case MONS_DEEP_DWARF_BERSERKER:
        item_race      = MAKE_ITEM_DWARVEN;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = random_choose_weighted(7, ARM_CHAIN_MAIL,
                                                1, ARM_PLATE_ARMOUR,
                                                0);
        break;

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
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_LEATHER_ARMOUR;
        break;

    case MONS_OCTOPODE:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = coinflip() ? ARM_WIZARD_HAT : ARM_CAP;
        break;

    case MONS_ANGEL:
    case MONS_CHERUB:
    case MONS_SIGMUND:
    case MONS_WIGHT:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ROBE;
        break;

    case MONS_SERAPH:
        level          = MAKE_GOOD_ITEM;
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        // obscenely good, don't ever place them randomly
        item.sub_type  = coinflip() ? ARM_PEARL_DRAGON_ARMOUR
                                    : ARM_FIRE_DRAGON_ARMOUR;
        break;

    // Centaurs sometimes wear barding.
    case MONS_CENTAUR:
    case MONS_CENTAUR_WARRIOR:
    case MONS_YAKTAUR:
    case MONS_YAKTAUR_CAPTAIN:
        if (one_chance_in(mon->type == MONS_CENTAUR              ? 1000 :
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
        if (one_chance_in(mon->type == MONS_NAGA         ?  800 :
                          mon->type == MONS_NAGA_WARRIOR ?  300 :
                          mon->type == MONS_NAGA_MAGE    ?  200
                                                         :  100))
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

    case MONS_TENGU_WARRIOR:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = coinflip() ? ARM_LEATHER_ARMOUR : ARM_RING_MAIL;
        break;

    case MONS_LAMIA:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_NAGA_BARDING;
        level          = MAKE_GOOD_ITEM;
        break;

    case MONS_GASTRONOK:
        if (one_chance_in(10) && !get_unique_item_status(UNRAND_PONDERING))
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
    case MONS_CRAZY_YIUF:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_CLOAK;
        break;

    case MONS_FANNAR:
    {
        force_item = true;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ROBE;
        item.plus = 1 + coinflip();
        set_item_ego_type(item, OBJ_ARMOUR, SPARM_COLD_RESISTANCE);
        set_equip_race(item, ISFLAG_ELVEN);
        item.flags |= ISFLAG_KNOW_TYPE;
        break;
    }

    case MONS_DOWAN:
        item_race = MAKE_ITEM_ELVEN;
        // intentional fall-through
    case MONS_JESSICA:
    case MONS_KOBOLD_DEMONOLOGIST:
    case MONS_OGRE_MAGE:
    case MONS_EROLCHA:
    case MONS_WIZARD:
    case MONS_ILSUIW:
    case MONS_MARA:
    case MONS_MERFOLK_AQUAMANCER:
    case MONS_SPRIGGAN:
    case MONS_SPRIGGAN_AIR_MAGE:
    case MONS_SPRIGGAN_ASSASSIN:
    case MONS_SPRIGGAN_ENCHANTER:
    case MONS_SPRIGGAN_DEFENDER:
        if (item_race == MAKE_ITEM_RANDOM_RACE)
            item_race = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ROBE;
        break;

    case MONS_DRACONIAN:
    case MONS_RED_DRACONIAN:
    case MONS_WHITE_DRACONIAN:
    case MONS_GREEN_DRACONIAN:
    case MONS_PALE_DRACONIAN:
    case MONS_MOTTLED_DRACONIAN:
    case MONS_BLACK_DRACONIAN:
    case MONS_YELLOW_DRACONIAN:
    case MONS_PURPLE_DRACONIAN:
    case MONS_GREY_DRACONIAN:
    case MONS_DRACONIAN_SHIFTER:
    case MONS_DRACONIAN_SCORCHER:
    case MONS_DRACONIAN_ANNIHILATOR:
    case MONS_DRACONIAN_CALLER:
    case MONS_DRACONIAN_MONK:
    case MONS_DRACONIAN_ZEALOT:
    case MONS_DRACONIAN_KNIGHT:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_CLOAK;
        break;

    case MONS_SPRIGGAN_DRUID:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ROBE;
        break;

    case MONS_THE_ENCHANTRESS:
        force_item = true;
        make_item_unrandart(item, UNRAND_FAERIE);
        break;

    case MONS_TIAMAT:
        force_item = true;
        make_item_unrandart(item, UNRAND_DRAGONSKIN);
        break;

    case MONS_ORC_SORCERER:
        if (one_chance_in(3))
            level = MAKE_GOOD_ITEM;
    case MONS_ORC_WIZARD:
    case MONS_BLORK_THE_ORC:
    case MONS_NERGALLE:
        item_race      = MAKE_ITEM_ORCISH;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ROBE;
        break;

    case MONS_BORIS:
        level = MAKE_GOOD_ITEM;
        // fall-through
    case MONS_AGNES:
    case MONS_FRANCES:
    case MONS_NECROMANCER:
    case MONS_VAMPIRE_MAGE:
    case MONS_PIKEL:
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_ROBE;
        break;

    case MONS_EUSTACHIO:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_LEATHER_ARMOUR;
        break;

    case MONS_NESSOS:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_CENTAUR_BARDING;
        break;

    case MONS_NIKOLA:
        item_race      = MAKE_ITEM_NO_RACE;
        item.base_type = OBJ_ARMOUR;
        item.sub_type  = ARM_CLOAK;
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
        ((force_item) ? get_mitm_slot() : items(0, xitc, xitt, true,
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
}

static void _give_gold(monster* mon, int level)
{
    const int it = items(0, OBJ_GOLD, 0, true, level, 0);
    _give_monster_item(mon, it);
}

void give_weapon(monster *mons, int level_number, bool mons_summoned, bool spectral_orcs)
{
    _give_weapon(mons, level_number, false, true, spectral_orcs);
}

void give_item(monster *mons, int level_number, bool mons_summoned, bool spectral_orcs)
{
    ASSERT(level_number > -1); // debugging absdepth0 changes

    if (mons->type == MONS_MAURICE)
        _give_gold(mons, level_number);

    _give_scroll(mons, level_number);
    _give_wand(mons, level_number);
    _give_potion(mons, level_number);

    const item_make_species_type item_race = _give_weapon(mons, level_number, false, true, spectral_orcs);

    _give_ammo(mons, level_number, item_race, mons_summoned);

    _give_armour(mons, 1 + level_number / 2, spectral_orcs);
    _give_shield(mons, 1 + level_number / 2);
}
