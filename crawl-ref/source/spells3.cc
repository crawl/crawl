/*
 *  File:       spells3.cc
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <2>     9/11/99        LRH    Teleportation takes longer in the Abyss
 *      <2>     8/05/99        BWR    Added allow_control_teleport
 *      <1>     -/--/--        LRH    Created
 */

#include "AppHdr.h"
#include "spells3.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <algorithm>

#include "externs.h"

#include "abyss.h"
#include "beam.h"
#include "branch.h"
#include "cloud.h"
#include "directn.h"
#include "debug.h"
#include "delay.h"
#include "effects.h" // holy word
#include "food.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "mon-pick.h"
#include "monstuff.h"
#include "mon-util.h"
#include "place.h"
#include "player.h"
#include "quiver.h"
#include "randart.h"
#include "religion.h"
#include "spells1.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stash.h"
#include "stuff.h"
#include "traps.h"
#include "view.h"
#include "xom.h"

bool cast_selective_amnesia(bool force)
{
    char ep_gain = 0;
    unsigned char keyin = 0;

    if (you.spell_no == 0)
        mpr("You don't know any spells.");      // re: sif muna {dlb}
    else
    {
        // query - conditional ordering is important {dlb}:
        while (true)
        {
            mpr( "Forget which spell ([?*] list [ESC] exit)? ", MSGCH_PROMPT );

            keyin = get_ch();

            if (keyin == ESCAPE)
                return (false);        // early return {dlb}

            if (keyin == '?' || keyin == '*')
            {
                // this reassignment is "key" {dlb}
                keyin = (unsigned char) list_spells(false);

                redraw_screen();
            }

            if (!isalpha( keyin ))
                mesclr( true );
            else
                break;
        }

        // Actual handling begins here {dlb}:
        const spell_type spell = get_spell_by_letter( keyin );
        const int slot  = get_spell_slot_by_letter( keyin );

        if (spell == SPELL_NO_SPELL)
            mpr( "You don't know that spell." );
        else
        {
            if (!force && you.religion != GOD_SIF_MUNA
                && random2(you.skills[SK_SPELLCASTING])
                    < random2(spell_difficulty( spell )))
            {
                mpr("Oops! This spell sure is a blunt instrument.");
                forget_map(20 + random2(50));
            }
            else
            {
                ep_gain = spell_mana( spell );
                del_spell_from_memory_by_slot( slot );

                if (ep_gain > 0)
                {
                    inc_mp(ep_gain, false);
                    mpr( "The spell releases its latent energy back to you as "
                         "it unravels." );
                }
            }
        }
    }

    return (true);
}

bool remove_curse(bool suppress_msg)
{
    int loopy = 0;              // general purpose loop variable {dlb}
    bool success = false;       // whether or not curse(s) removed {dlb}

    // Special "wield slot" case - see if you can figure out why {dlb}:
    // ... because only cursed *weapons* in hand count as cursed -- bwr
    if (you.equip[EQ_WEAPON] != -1
        && you.inv[you.equip[EQ_WEAPON]].base_type == OBJ_WEAPONS)
    {
        if (item_cursed( you.inv[you.equip[EQ_WEAPON]] ))
        {
            // Also sets wield_change.
            do_uncurse_item( you.inv[you.equip[EQ_WEAPON]] );
            success = true;
        }
    }

    // Everything else uses the same paradigm - are we certain?
    // What of artefact rings and amulets? {dlb}:
    for (loopy = EQ_CLOAK; loopy < NUM_EQUIP; loopy++)
    {
        if (you.equip[loopy] != -1 && item_cursed(you.inv[you.equip[loopy]]))
        {
            do_uncurse_item( you.inv[you.equip[loopy]] );
            success = true;
        }
    }

    // Messaging output. {dlb}:
    if (!suppress_msg)
    {
        if (success)
            mpr("You feel as if something is helping you.");
        else
            canned_msg(MSG_NOTHING_HAPPENS);
    }

    return (success);
}

bool detect_curse(bool suppress_msg)
{
    int loopy = 0;              // general purpose loop variable {dlb}
    bool success = false;       // whether or not any curses found {dlb}

    for (loopy = 0; loopy < ENDOFPACK; loopy++)
    {
        if (you.inv[loopy].quantity
            && (you.inv[loopy].base_type == OBJ_WEAPONS
                || you.inv[loopy].base_type == OBJ_ARMOUR
                || you.inv[loopy].base_type == OBJ_JEWELLERY))
        {
            if (!item_ident( you.inv[loopy], ISFLAG_KNOW_CURSE ))
                success = true;

            set_ident_flags( you.inv[loopy], ISFLAG_KNOW_CURSE );
        }
    }

    // messaging output {dlb}:
    if (!suppress_msg)
    {
        if (success)
            mpr("You sense the presence of curses on your possessions.");
        else
            canned_msg(MSG_NOTHING_HAPPENS);
    }

    return (success);
}

int cast_smiting(int power, dist &beam)
{
    bool success = false;

    if (mgrd[beam.tx][beam.ty] == NON_MONSTER || beam.isMe)
        canned_msg(MSG_SPELL_FIZZLES);
    else
    {
        monsters *monster = &menv[mgrd[beam.tx][beam.ty]];

        god_conduct_trigger conducts[3];
        disable_attack_conducts(conducts);

        success = !stop_attack_prompt(monster, false, false);

        if (success)
        {
            set_attack_conducts(conducts, monster);

            mprf("You smite %s!", monster->name(DESC_NOCAP_THE).c_str());

            behaviour_event(monster, ME_ANNOY, MHITYOU);
            if (mons_is_mimic( monster->type ))
                mimic_alert(monster);
        }

        enable_attack_conducts(conducts);

        if (success)
        {
            // Maxes out at around 40 damage at 27 Invocations, which is
            // plenty in my book (the old max damage was around 70,
            // which seems excessive).
            hurt_monster(monster, 7 + (random2(power) * 33 / 191));

            if (monster->hit_points < 1)
                monster_die(monster, KILL_YOU, 0);
            else
                print_wounds(monster);
        }
    }

    return (success);
}

int airstrike(int power, dist &beam)
{
    bool success = false;

    if (mgrd[beam.tx][beam.ty] == NON_MONSTER || beam.isMe)
        canned_msg(MSG_SPELL_FIZZLES);
    else
    {
        monsters *monster = &menv[mgrd[beam.tx][beam.ty]];

        god_conduct_trigger conducts[3];
        disable_attack_conducts(conducts);

        success = !stop_attack_prompt(monster, false, false);

        if (success)
        {
            set_attack_conducts(conducts, monster);

            mprf("The air twists around and strikes %s!",
                 monster->name(DESC_NOCAP_THE).c_str());

            behaviour_event(monster, ME_ANNOY, MHITYOU);
            if (mons_is_mimic( monster->type ))
                mimic_alert(monster);
        }

        enable_attack_conducts(conducts);

        if (success)
        {
            int hurted = 8 + random2(random2(4) + (random2(power) / 6)
                           + (random2(power) / 7));

            if (mons_flies(monster))
            {
                hurted *= 3;
                hurted /= 2;
            }

            hurted -= random2(1 + monster->ac);

            if (hurted < 0)
                hurted = 0;

            hurt_monster(monster, hurted);

            if (monster->hit_points < 1)
                monster_die(monster, KILL_YOU, 0);
            else
                print_wounds(monster);
        }
    }

    return (success);
}

bool cast_bone_shards(int power, bolt &beam)
{
    bool success = false;

    if (you.equip[EQ_WEAPON] == -1
        || you.inv[you.equip[EQ_WEAPON]].base_type != OBJ_CORPSES)
    {
        canned_msg(MSG_SPELL_FIZZLES);
    }
    else if (you.inv[you.equip[EQ_WEAPON]].sub_type != CORPSE_SKELETON)
        mpr("The corpse collapses into a mass of pulpy flesh.");
    else
    {
        // Practical max of 100 * 15 + 3000 = 4500.
        // Actual max of    200 * 15 + 3000 = 6000.
        power *= 15;
        power += mons_weight( you.inv[you.equip[EQ_WEAPON]].plus );

        if (!player_tracer(ZAP_BONE_SHARDS, power, beam))
            return (false);

        mpr("The skeleton explodes into sharp fragments of bone!");

        dec_inv_item_quantity( you.equip[EQ_WEAPON], 1 );
        zapping(ZAP_BONE_SHARDS, power, beam);

        success = true;
    }

    return (success);
}

bool cast_sublimation_of_blood(int pow)
{
    bool success = false;

    int wielded = you.equip[EQ_WEAPON];

    if (wielded != -1)
    {
        if (you.inv[wielded].base_type == OBJ_FOOD
            && you.inv[wielded].sub_type == FOOD_CHUNK)
        {
            mpr("The chunk of flesh you are holding crumbles to dust.");

            mpr("A flood of magical energy pours into your mind!");

            inc_mp(7 + random2(7), false);

            dec_inv_item_quantity(wielded, 1);
        }
        else if (is_blood_potion(you.inv[wielded]))
        {
            mprf("The blood within %s frothes and boils.",
                 you.inv[wielded].quantity > 1 ? "one of your flasks"
                                               : "the flask you are holding");

            mpr("A flood of magical energy pours into your mind!");

            inc_mp(7 + random2(7), false);

            split_potions_into_decay(wielded, 1, false);
        }
        else
            wielded = -1;
    }

    if (wielded == -1)
    {
        if (you.duration[DUR_DEATHS_DOOR])
        {
            mpr( "A conflicting enchantment prevents the spell from "
                 "coming into effect." );
        }
        else if (you.species == SP_VAMPIRE && you.hunger_state <= HS_SATIATED)
        {
            mpr("You don't have enough blood to draw power from your "
                "own body.");
        }
        else if (!enough_hp(2, true))
             mpr("Your attempt to draw power from your own body fails.");
        else
        {
            // For vampires.
            int food = 0;

            mpr("You draw magical energy from your own body!");

            while (you.magic_points < you.max_magic_points && you.hp > 1
                   && (you.species != SP_VAMPIRE || you.hunger - food >= 7000))
            {
                success = true;

                inc_mp(1, false);
                dec_hp(1, false);

                if (you.species == SP_VAMPIRE)
                    food += 15;

                for (int loopy = 0; loopy < (you.hp > 1 ? 3 : 0); ++loopy)
                    if (x_chance_in_y(6, pow))
                        dec_hp(1, false);

                if (x_chance_in_y(6, pow))
                    break;
            }

            make_hungry(food, false);
        }
    }

    return (success);
}

bool cast_call_imp(int pow, god_type god)
{
    bool success = false;

    monster_type mon = (one_chance_in(3)) ? MONS_WHITE_IMP :
                       (one_chance_in(7)) ? MONS_SHADOW_IMP
                                          : MONS_IMP;

    const int dur = std::min(2 + (random2(pow) / 4), 6);

    const int monster =
        create_monster(
            mgen_data(mon, BEH_FRIENDLY, dur, you.pos(),
                      you.pet_target,
                      MG_FORCE_BEH, god));

    if (monster != -1)
    {
        success = true;

        mpr((mon == MONS_WHITE_IMP)  ? "A beastly little devil appears in a puff of frigid air." :
            (mon == MONS_SHADOW_IMP) ? "A shadowy apparition takes form in the air."
                                     : "A beastly little devil appears in a puff of flame.");

        player_angers_monster(&menv[monster]);
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return (success);
}

static bool _summon_demon_wrapper(int pow, god_type god, monster_type mon,
                                  int dur, bool friendly, bool charmed,
                                  bool quiet)
{
    bool success = false;

    const int monster =
        create_monster(
            mgen_data(mon,
                      friendly ? BEH_FRIENDLY :
                          charmed ? BEH_CHARMED : BEH_HOSTILE,
                      dur, you.pos(),
                      friendly ? you.pet_target : MHITYOU,
                      MG_FORCE_BEH, god));

    if (monster != -1)
    {
        success = true;

        mpr("A demon appears!");

        if (!player_angers_monster(&menv[monster]) && !friendly)
        {
            mpr(charmed ? "You don't feel so good about this..."
                        : "It doesn't look very happy.");
        }
    }

    return (success);
}

static bool _summon_demon_wrapper(int pow, god_type god, demon_class_type dct,
                                  int dur, bool friendly, bool charmed,
                                  bool quiet)
{
    monster_type mon = summon_any_demon(dct);

    return _summon_demon_wrapper(pow, god, mon, dur, friendly, charmed, quiet);
}

bool summon_lesser_demon(int pow, god_type god,
                         bool quiet)
{
    return _summon_demon_wrapper(pow, god, DEMON_LESSER,
                                 std::min(2 + (random2(pow) / 4), 6),
                                 random2(pow) > 3, false, quiet);
}

bool summon_common_demon(int pow, god_type god,
                         bool quiet)
{
    return _summon_demon_wrapper(pow, god, DEMON_COMMON,
                                 std::min(2 + (random2(pow) / 4), 6),
                                 random2(pow) > 3, false, quiet);
}

bool summon_greater_demon(int pow, god_type god,
                          bool quiet)
{
    return _summon_demon_wrapper(pow, god, DEMON_GREATER,
                                 5, false, random2(pow) > 5, quiet);
}

// Makhleb or Kikubaaqudgha sends a demonic buddy (or enemy) for a
// follower.
bool summon_demon_type(monster_type mon, int pow, god_type god)
{
    return _summon_demon_wrapper(pow, god, mon,
                                 std::min(2 + (random2(pow) / 4), 6),
                                 random2(pow) > 3, false, false);
}

bool cast_summon_demon(int pow, god_type god)
{
    mpr("You open a gate to Pandemonium!");

    bool success = summon_common_demon(pow, god);

    if (!success)
        canned_msg(MSG_NOTHING_HAPPENS);

    return (success);
}

bool cast_demonic_horde(int pow, god_type god)
{
    bool success = false;

    const int how_many = 7 + random2(5);

    mpr("You open a gate to Pandemonium!");

    for (int i = 0; i < how_many; ++i)
    {
        if (summon_lesser_demon(pow, god, true))
            success = true;
    }

    if (!success)
        canned_msg(MSG_NOTHING_HAPPENS);

    return (success);
}

bool cast_summon_greater_demon(int pow, god_type god)
{
    mpr("You open a gate to Pandemonium!");

    bool success = summon_greater_demon(pow, god);

    if (!success)
        canned_msg(MSG_NOTHING_HAPPENS);

    return (success);
}

bool cast_shadow_creatures(god_type god)
{
    mpr("Wisps of shadow whirl around you...");

    const int monster =
        create_monster(
            mgen_data(RANDOM_MONSTER, BEH_FRIENDLY, 2,
                      you.pos(), you.pet_target,
                      MG_FORCE_BEH, god), false);

    if (monster == -1)
    {
        mpr("The shadows disperse without effect.");
        return (false);
    }

    player_angers_monster(&menv[monster]);
    return (true);
}

bool cast_summon_horrible_things(int pow, god_type god)
{
    if (one_chance_in(3)
        && !lose_stat(STAT_INTELLIGENCE, 1, true, "summoning horrible things"))
    {
        mpr("Your call goes unanswered.");
        return (false);
    }

    int how_many_small =
        stepdown_value(2 + (random2(pow) / 10) + (random2(pow) / 10),
                       2, 2, 6, -1);
    int how_many_big = 0;

    // No more than 2 tentacled monstrosities.
    while (how_many_small > 2 && how_many_big < 2 && one_chance_in(3))
    {
        how_many_small -= 2;
        how_many_big++;
    }

    // No more than 8 summons.
    how_many_small = std::min(8, how_many_small);
    how_many_big = std::min(8, how_many_big);

    int count = 0;

    while (how_many_big > 0)
    {
        if (create_monster(
               mgen_data(MONS_TENTACLED_MONSTROSITY, BEH_FRIENDLY, 6,
                         you.pos(), you.pet_target,
                         0, god)) != -1)
        {
            count++;
        }

        how_many_big--;
    }

    while (how_many_small > 0)
    {
        if (create_monster(
               mgen_data(MONS_ABOMINATION_LARGE, BEH_FRIENDLY, 6,
                         you.pos(), you.pet_target,
                         0, god)) != -1)
        {
            count++;
        }

        how_many_small--;
    }

    if (count > 0)
    {
        mprf("Some thing%s answered your call!",
             count > 1 ? "s" : "");
        return (true);
    }

    mpr("Your call goes unanswered.");
    return (false);
}

static bool _is_animatable_corpse(const item_def& item)
{
    return (item.base_type == OBJ_CORPSES
        && mons_zombie_size(item.plus) != Z_NOZOMBIE);
}

// Try to equip the zombie/skeleton with the objects it died with.
// This excludes items which were dropped by the player onto the corpse,
// and corpses which were picked up and moved by the player, so the player
// can't equip their undead slaves with items of their choice.
//
// The item selection logic has one problem: if a first monster without
// any items dies and leaves a corpse, and then a second monster with
// items dies on the same spot but doesn't leave a corpse, then the
// undead can be equipped with the second monster's items if the second
// monster is either of the same type as the first, or if the second
// monster wasn't killed by the player or a player's pet.
static void _equip_undead(const coord_def &a, int corps, int monster,
                          int monnum)
{
// Delay this until after 0.4
#if 0
    monsters* mon = &menv[monster];

    monster_type type = static_cast<monster_type>(monnum);

    if (mons_itemuse(monnum) < MONUSE_STARTING_EQUIPMENT)
        return;

    // If the player picked up and dropped the corpse then all its
    // original equipment fell off.
    if (mitm[corps].flags & ISFLAG_DROPPED)
        return;

    // A monster's corpse is last in the linked list after its items,
    // so (for example) the first item after the second-to-last corpse
    // is the first item belonging to the last corpse.
    int objl      = igrd(a);
    int first_obj = NON_ITEM;

    while (objl != NON_ITEM && objl != corps)
    {
        item_def item(mitm[objl]);

        if (item.base_type == OBJ_CORPSES)
        {
            first_obj = NON_ITEM;
            continue;
        }

        if (first_obj == NON_ITEM)
            first_obj = objl;

        objl = item.link;
    }

    ASSERT(objl == corps);

    if (first_obj == NON_ITEM)
        return;

    // Iterate backwards over the list, since the items earlier in the
    // linked list were dropped most recently and hence more likely to
    // be items the monster didn't die with.
    std::vector<int> item_list;
    objl = first_obj;
    while (objl != NON_ITEM && objl != corps)
    {
        item_list.push_back(objl);
        objl = mitm[objl].link;
    }

    for (int i = item_list.size() - 1; i >= 0; --i)
    {
        objl = item_list[i];
        item_def &item(mitm[objl]);

        // Stop equipping monster if the item probably didn't originally
        // belong to the monster.
        if ( (origin_known(item) && (item.orig_monnum - 1) != monnum)
            || (item.flags & (ISFLAG_DROPPED | ISFLAG_THROWN))
            || item.base_type == OBJ_CORPSES)
        {
            return;
        }

        mon_inv_type mslot;

        switch(item.base_type)
        {
        case OBJ_WEAPONS:
            if (mon->inv[MSLOT_WEAPON] != NON_ITEM)
            {
                if (mons_wields_two_weapons(type))
                    mslot = MSLOT_ALT_WEAPON;
                else
                {
                    if (is_range_weapon(mitm[mon->inv[MSLOT_WEAPON]])
                        == is_range_weapon(item))
                    {
                        // Two different items going into the same
                        // slot indicate that this and further items
                        // weren't equipment the monster died with.
                        return;
                    }
                    else
                        // The undead are too stupid to switch between weapons.
                        continue;
                }
            }
            else
                mslot = MSLOT_WEAPON;
            break;
        case OBJ_ARMOUR:
            mslot = equip_slot_to_mslot(get_armour_slot(item));

            // A piece of armour which can't be worn indicates that this
            // and further items weren't the equipment the monster died
            // with.
            if (mslot == NUM_MONSTER_SLOTS)
                return;
            break;

        case OBJ_MISSILES:
            mslot = MSLOT_MISSILE;
            break;

        case OBJ_GOLD:
            mslot = MSLOT_GOLD;
            break;

        // The undead are too stupid to use these.
        case OBJ_WANDS:
        case OBJ_SCROLLS:
        case OBJ_POTIONS:
        case OBJ_MISCELLANY:
            continue;

        default:
            continue;
        } // switch

        // Two different items going into the same slot indicate that
        // this and further items weren't equipment the monster died
        // with.
        if (mon->inv[mslot] != NON_ITEM)
            return;

        unlink_item(objl);
        mon->inv[mslot] = objl;

        if (mslot != MSLOT_ALT_WEAPON || mons_wields_two_weapons(mon))
            mon->equip(item, mslot, 0);
    } // while
#endif
}

static bool _raise_corpse(const coord_def &a, int corps, beh_type beha,
                          unsigned short hitting, god_type god, bool actual)
{
    const item_def& item = mitm[corps];

    if (!_is_animatable_corpse(item))
        return (false);

    if (!actual)
        return (true);

    const monster_type zombie_type =
        static_cast<monster_type>(item.plus);

    const int number = (item.props.exists(MONSTER_NUMBER)) ?
                            item.props[MONSTER_NUMBER].get_short() : 0;

    // Headless hydras cannot be raised, sorry.
    if (zombie_type == MONS_HYDRA && number == 0)
        return (false);

    monster_type mon = MONS_PROGRAM_BUG;

    if (item.sub_type == CORPSE_BODY)
    {
        mon = (mons_zombie_size(item.plus) == Z_SMALL) ?
                MONS_ZOMBIE_SMALL : MONS_ZOMBIE_LARGE;
    }
    else
    {
        mon = (mons_zombie_size(item.plus) == Z_SMALL) ?
                MONS_SKELETON_SMALL : MONS_SKELETON_LARGE;
    }

    const int monster = create_monster(
                            mgen_data(mon, beha, 0,
                                      a, hitting,
                                      0, god,
                                      zombie_type, number));

    if (monster != -1)
    {
        const int monnum = item.orig_monnum - 1;

        if (mons_is_unique(monnum))
        {
            menv[monster].mname = origin_monster_name(item);

            // Special case for Blork the orc: shorten his name to "Blork"
            // to avoid mentions of "Blork the orc the orc skeleton".
            if (monnum == MONS_BLORK_THE_ORC)
                menv[monster].mname = "Blork";
        }

        _equip_undead(a, corps, monster, monnum);

        destroy_item(corps);

        return (true);
    }

    return (false);
}

bool animate_a_corpse(const coord_def &a, corpse_type class_allowed,
                      beh_type beha, unsigned short hitting,
                      god_type god, bool actual,
                      bool quiet)
{
    if (is_sanctuary(a.x, a.y))
        return (false);

    bool success = false;

    // Search all the items on the ground for a corpse.
    for (stack_iterator si(a); si; ++si)
    {
        if (_is_animatable_corpse(*si)
            && (class_allowed == CORPSE_BODY
                || si->sub_type == CORPSE_SKELETON))
        {
            const bool was_butchering = is_being_butchered(*si);

            success = _raise_corpse(a, si.link(), beha, hitting, god, actual);

            if (actual && success)
            {
                if (!quiet)
                {
                    if (was_butchering)
                        mpr("The corpse you are butchering rises to attack!");

                    if (is_terrain_seen(a))
                        mpr("The dead are walking!");
                }

                if (was_butchering)
                    xom_is_stimulated(255);
            }
            break;
        }
    }

    return (success);
}

int animate_dead(actor *caster, int pow, beh_type beha, unsigned short hitting,
                 god_type god, bool actual)
{
    UNUSED(pow);

    static env_show_grid losgrid;

    const coord_def c(caster->pos());

    int minx = c.x - 6;
    int maxx = c.x + 7;
    int miny = c.y - 6;
    int maxy = c.y + 7;
    int xinc = 1;
    int yinc = 1;

    int number_raised = 0;
    int number_seen   = 0;

    if (coinflip())
    {
        minx = c.x + 6;
        maxx = c.x - 7;
        xinc = -1;
    }

    if (coinflip())
    {
        miny = c.y + 6;
        maxy = c.y - 7;
        yinc = -1;
    }

    if (caster != &you)
        losight(losgrid, grd, c.x, c.y, true);

    env_show_grid &los(caster == &you? env.no_trans_show : losgrid);

    bool was_butchering = false;

    coord_def a;

    for (a.x = minx; a.x != maxx; a.x += xinc)
    {
        for (a.y = miny; a.y != maxy; a.y += yinc)
        {
            if (!in_bounds(a) || !see_grid(los, c, a))
                continue;

            // Search all the items on the ground for a corpse.  Only
            // one of a stack will be raised.
            for (stack_iterator si(a); si; ++si)
            {
                if (is_being_butchered(*si, false))
                    was_butchering = true;

                if (animate_a_corpse(a, CORPSE_BODY, beha, hitting, god,
                                     actual, true))
                {
                    number_raised++;

                    if (see_grid(env.show, you.pos(), a))
                         number_seen++;

                    break;
                }
            }
        }
    }

    if (actual)
    {
        if (was_butchering)
            mpr("The corpse you are butchering rises to attack!");

        if (number_seen > 0)
            mpr("The dead are walking!");
    }

    return (number_raised);
}

// Simulacrum
//
// This spell extends creating undead to Ice mages, as such it's high
// level, requires wielding of the material component, and the undead
// aren't overly powerful (they're also vulnerable to fire).  I've put
// back the abjuration level in order to keep down the army sizes again.
//
// As for what it offers necromancers considering all the downsides
// above... it allows the turning of a single corpse into an army of
// monsters (one per food chunk)... which is also a good reason for
// why it's high level.
//
// Hides and other "animal part" items are intentionally left out, it's
// unrequired complexity, and fresh flesh makes more "sense" for a spell
// reforming the original monster out of ice anyways.
bool cast_simulacrum(int pow, god_type god)
{
    int how_many_max = std::min(8, 4 + random2(pow) / 20);

    const int chunk = you.equip[EQ_WEAPON];

    if (chunk != -1
        && is_valid_item(you.inv[chunk])
        && (you.inv[chunk].base_type == OBJ_CORPSES
            || (you.inv[chunk].base_type == OBJ_FOOD
                && you.inv[chunk].sub_type == FOOD_CHUNK)))
    {
        const monster_type mon =
            static_cast<monster_type>(you.inv[chunk].plus);

        // Can't create more than the available chunks.
        if (you.inv[chunk].quantity < how_many_max)
            how_many_max = you.inv[chunk].quantity;

        dec_inv_item_quantity(chunk, how_many_max);

        int count = 0;

        for (int i = 0; i < how_many_max; ++i)
        {
            if (create_monster(
                    mgen_data(MONS_SIMULACRUM_SMALL, BEH_FRIENDLY,
                              6, you.pos(), you.pet_target,
                              0, god, mon)) != -1)
            {
                count++;
            }
        }

        if (count > 0)
        {
            mprf("%s icy figure%s form%s before you!",
                count > 1 ? "Some" : "An", count > 1 ? "s" : "",
                count > 1 ? "" : "s");
            return (true);
        }

        mpr("You feel cold for a second.");
        return (false);
    }

    mpr("You need to wield a piece of raw flesh for this spell to be "
        "effective!");
    return (false);
}

bool cast_twisted_resurrection(int pow, god_type god)
{
    if (igrd[you.x_pos][you.y_pos] == NON_ITEM)
    {
        mpr("There's nothing here!");
        return (false);
    }

    int objl = igrd[you.x_pos][you.y_pos];
    int next;

    int how_many_corpses = 0;
    int total_mass = 0;
    int rotted = 0;

    while (objl != NON_ITEM)
    {
        next = mitm[objl].link;

        if (mitm[objl].base_type == OBJ_CORPSES
                && mitm[objl].sub_type == CORPSE_BODY)
        {
            total_mass += mons_weight(mitm[objl].plus);

            how_many_corpses++;

            if (food_is_rotten(mitm[objl]))
                rotted++;

            destroy_item(objl);
        }

        objl = next;
    }

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Mass for abomination: %d", total_mass);
#endif

    // This is what the old statement pretty much boils down to,
    // the average will be approximately 10 * pow (or about 1000
    // at the practical maximum).  That's the same as the mass
    // of a hippogriff, a spiny frog, or a steam dragon.  Thus,
    // material components are far more important to this spell. -- bwr
    total_mass += roll_dice(20, pow);

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Mass including power bonus: %d", total_mass);
#endif

    if (total_mass < 400 + roll_dice(2, 500)
        || how_many_corpses < (coinflip() ? 3 : 2))
    {
        mprf("The corpse%s collapse%s into a pulpy mess.",
             how_many_corpses > 1 ? "s": "", how_many_corpses > 1 ? "": "s");
        return (false);
    }

    monster_type mon =
        (total_mass > 500 + roll_dice(3, 1000)) ? MONS_ABOMINATION_LARGE
                                                : MONS_ABOMINATION_SMALL;

    char colour = (rotted == how_many_corpses)          ? BROWN :
                  (rotted >= random2(how_many_corpses)) ? RED
                                                        : LIGHTRED;

    const int monster =
        create_monster(
            mgen_data(mon, BEH_FRIENDLY,
                      0, you.pos(), you.pet_target,
                      MG_FORCE_BEH, god,
                      MONS_PROGRAM_BUG, 0, colour));

    if (monster == -1)
    {
        mpr("The corpses collapse into a pulpy mess.");
        return (false);
    }

    mpr("The heap of corpses melds into an agglomeration of writhing flesh!");

    if (mon == MONS_ABOMINATION_LARGE)
    {
        menv[monster].hit_dice = 8 + total_mass / ((colour == LIGHTRED) ? 500 :
                                                   (colour == RED)      ? 1000
                                                                        : 2500);

        menv[monster].hit_dice = MIN(30, menv[monster].hit_dice);

        // XXX: No convenient way to get the hit dice size right now.
        menv[monster].hit_points = hit_points(menv[monster].hit_dice, 2, 5);
        menv[monster].max_hit_points = menv[monster].hit_points;

        if (colour == LIGHTRED)
            menv[monster].ac += total_mass / 1000;
    }

    player_angers_monster(&menv[monster]);

    return (true);
}

bool cast_summon_wraiths(int pow, god_type god)
{
    bool success = false;

    const int chance = random2(25);
    monster_type mon = ((chance > 8) ? MONS_WRAITH :           // 64%
                        (chance > 3) ? MONS_FREEZING_WRAITH    // 20%
                                     : MONS_SPECTRAL_WARRIOR); // 16%

    bool friendly = (random2(pow) > 5);

    const int monster =
        create_monster(
            mgen_data(mon,
                      friendly ? BEH_FRIENDLY : BEH_HOSTILE,
                      5, you.pos(),
                      friendly ? you.pet_target : MHITYOU,
                      MG_FORCE_BEH, god));

    if (monster != -1)
    {
        success = true;

        if (player_angers_monster(&menv[monster]))
            friendly = false;

        mpr(friendly ? "An insubstantial figure forms in the air."
                     : "You sense a hostile presence.");
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    //jmf: Kiku sometimes deflects this
    if (you.religion != GOD_KIKUBAAQUDGHA
        || player_under_penance() || you.piety < piety_breakpoint(3)
        || !x_chance_in_y(you.piety, MAX_PIETY))
    {
        disease_player(25 + random2(50));
    }

    return (success);
}

bool cast_death_channel(int pow, god_type god)
{
    bool success = false;

    if (you.duration[DUR_DEATH_CHANNEL] < 30)
    {
        success = true;

        mpr("Malign forces permeate your being, awaiting release.");

        you.duration[DUR_DEATH_CHANNEL] += 15 + random2(1 + (pow / 3));

        if (you.duration[DUR_DEATH_CHANNEL] > 100)
            you.duration[DUR_DEATH_CHANNEL] = 100;

        if (god != GOD_NO_GOD)
            you.attribute[ATTR_DIVINE_DEATH_CHANNEL] = static_cast<int>(god);
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return (success);
}

// This function returns true if the player can use controlled teleport
// here.
bool allow_control_teleport(bool quiet)
{
    bool retval = !(testbits(env.level_flags, LFLAG_NO_TELE_CONTROL)
                      || testbits(get_branch_flags(), BFLAG_NO_TELE_CONTROL));

    // Tell the player why if they have teleport control.
    if (!quiet && !retval && player_control_teleport())
        mpr("A powerful magic prevents control of your teleportation.");

    return (retval);
}

void you_teleport(void)
{
    if (scan_randarts(RAP_PREVENT_TELEPORTATION))
        mpr("You feel a weird sense of stasis.");
    else if (you.duration[DUR_TELEPORT])
    {
        mpr("You feel strangely stable.");
        you.duration[DUR_TELEPORT] = 0;
    }
    else
    {
        mpr("You feel strangely unstable.");

        you.duration[DUR_TELEPORT] = 3 + random2(3);

        if (you.level_type == LEVEL_ABYSS && !one_chance_in(5))
        {
            mpr("You have a feeling this translocation may take a while to kick in...");
            you.duration[DUR_TELEPORT] += 5 + random2(10);
        }
    }
}

static bool _teleport_player( bool allow_control, bool new_abyss_area )
{
    bool is_controlled = (allow_control && !you.duration[DUR_CONF]
                          && player_control_teleport()
                          && allow_control_teleport());

    if (scan_randarts(RAP_PREVENT_TELEPORTATION))
    {
        mpr("You feel a strange sense of stasis.");
        return (false);
    }

    // After this point, we're guaranteed to teleport. Kill the appropriate
    // delays.
    interrupt_activity( AI_TELEPORT );

    // Update what we can see at the current location as well as its stash,
    // in case something happened in the exact turn that we teleported
    // (like picking up/dropping an item).
    viewwindow(true, false);
    StashTrack.update_stash();

    if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
    {
        you.duration[DUR_CONDENSATION_SHIELD] = 0;
        you.redraw_armour_class = true;
    }

    if (you.level_type == LEVEL_ABYSS)
    {
        abyss_teleport( new_abyss_area );
        if (you.pet_target != MHITYOU)
            you.pet_target = MHITNOT;

        return (true);
    }

    coord_def pos(1, 0);
    bool      large_change = false;

    if (is_controlled)
    {
        mpr("You may choose your destination (press '.' or delete to select).");
        mpr("Expect minor deviation.");
        more();

        show_map(pos, false);

        redraw_screen();

#if DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Target square (%d,%d)", pos.x, pos.y );
#endif

        pos.x += random2(3) - 1;
        pos.y += random2(3) - 1;

        if (one_chance_in(4))
        {
            pos.x += random2(3) - 1;
            pos.y += random2(3) - 1;
        }

        if (!in_bounds(pos))
        {
            mpr("Nearby solid objects disrupt your rematerialisation!");
            is_controlled = false;
        }

#if DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS,
             "Scattered target square (%d,%d)", pos.x, pos.y );
#endif

        if (is_controlled)
        {
            // no longer held in net
            if (pos.x != you.x_pos || pos.y != you.y_pos)
                clear_trapping_net();

            if (!see_grid(pos))
                large_change = true;

            you.moveto(pos.x, pos.y);

            if (grd[you.x_pos][you.y_pos] != DNGN_FLOOR
                    && grd[you.x_pos][you.y_pos] != DNGN_SHALLOW_WATER
                || mgrd[you.x_pos][you.y_pos] != NON_MONSTER
                || env.cgrid[you.x_pos][you.y_pos] != EMPTY_CLOUD)
            {
                is_controlled = false;
                large_change  = false;
            }
            else
            {
                // Controlling teleport contaminates the player. -- bwr
                contaminate_player(1, true);
            }
        }
    }   // end "if is_controlled"

    if (!is_controlled)
    {
        int newx, newy;

        do
        {
            newx = random_range(X_BOUND_1 + 1, X_BOUND_2 - 1);
            newy = random_range(Y_BOUND_1 + 1, Y_BOUND_2 - 1);
        }
        while (grd[newx][newy] != DNGN_FLOOR
                   && grd[newx][newy] != DNGN_SHALLOW_WATER
               || mgrd[newx][newy] != NON_MONSTER
               || env.cgrid[newx][newy] != EMPTY_CLOUD);

        // no longer held in net
        if (newx != you.x_pos || newy != you.y_pos)
            clear_trapping_net();

        if ( newx == you.x_pos && newy == you.y_pos )
            mpr("Your surroundings flicker for a moment.");
        else if ( see_grid(newx, newy) )
            mpr("Your surroundings seem slightly different.");
        else
        {
            mpr("Your surroundings suddenly seem different.");
            large_change = true;
        }

        you.x_pos = newx;
        you.y_pos = newy;

        // Necessary to update the view centre.
        you.moveto(you.pos());
    }

    if (large_change)
        handle_interrupted_swap(true);

    return !is_controlled;
}

void you_teleport_now( bool allow_control, bool new_abyss_area )
{
    const bool randtele = _teleport_player(allow_control, new_abyss_area);

    // Xom is amused by uncontrolled teleports that land you in a
    // dangerous place, unless the player is in the Abyss and
    // teleported to escape from all the monsters chasing him/her,
    // since in that case the new dangerous area is almost certainly
    // *less* dangerous than the old dangerous area.
    // Teleporting in a labyrinth is also funny, but only for non-minotaurs.
    if (randtele
        && (you.level_type == LEVEL_LABYRINTH && you.species != SP_MINOTAUR
            || you.level_type != LEVEL_ABYSS && player_in_a_dangerous_place()))
    {
        xom_is_stimulated(255);
    }
}

bool entomb(int powc)
{
    // power guidelines:
    // powc is roughly 50 at Evoc 10 with no godly assistance, ranging
    // up to 300 or so with godly assistance or end-level, and 1200
    // as more or less the theoretical maximum.
    int number_built = 0;

    const dungeon_feature_type safe_to_overwrite[] = {
        DNGN_FLOOR, DNGN_SHALLOW_WATER, DNGN_OPEN_DOOR,
        DNGN_TRAP_MECHANICAL, DNGN_TRAP_MAGICAL, DNGN_TRAP_NATURAL,
        DNGN_UNDISCOVERED_TRAP,
        DNGN_FLOOR_SPECIAL
    };

    for (int srx = you.x_pos - 1; srx < you.x_pos + 2; srx++)
        for (int sry = you.y_pos - 1; sry < you.y_pos + 2; sry++)
        {
            // Tile already occupied by monster or yourself {dlb}:
            if (mgrd[srx][sry] != NON_MONSTER
                || srx == you.x_pos && sry == you.y_pos)
            {
                continue;
            }

            if ( one_chance_in(powc/5) )
                continue;

            bool proceed = false;
            for (unsigned int i = 0; i < ARRAYSZ(safe_to_overwrite); ++i)
            {
                if (grd[srx][sry] == safe_to_overwrite[i])
                {
                    proceed = true;
                    break;
                }
            }

            // checkpoint one - do we have a legitimate tile? {dlb}
            if (!proceed)
                continue;

            int objl = igrd[srx][sry];
            int hrg = 0;

            while (objl != NON_ITEM)
            {
                // hate to see the orb get destroyed by accident {dlb}:
                if (mitm[objl].base_type == OBJ_ORBS)
                {
                    proceed = false;
                    break;
                }

                hrg = mitm[objl].link;
                objl = hrg;
            }

            // checkpoint two - is the orb resting in the tile? {dlb}:
            if (!proceed)
                continue;

            objl = igrd[srx][sry];
            hrg = 0;

            while (objl != NON_ITEM)
            {
                hrg = mitm[objl].link;
                destroy_item(objl);
                objl = hrg;
            }

            // deal with clouds {dlb}:
            if (env.cgrid[srx][sry] != EMPTY_CLOUD)
                delete_cloud( env.cgrid[srx][sry] );

            // mechanical traps are destroyed {dlb}:
            int which_trap;
            if ((which_trap = trap_at_xy(srx, sry)) != -1)
            {
                if (trap_category(env.trap[which_trap].type)
                                                    == DNGN_TRAP_MECHANICAL)
                {
                    env.trap[which_trap].type = TRAP_UNASSIGNED;
                    env.trap[which_trap].x = 1;
                    env.trap[which_trap].y = 1;
                }
            }

            // Finally, place the wall {dlb}:
            grd[srx][sry] = DNGN_ROCK_WALL;
            number_built++;
        }

    if (number_built > 0)
    {
        mpr("Walls emerge from the floor!");

        for (int i = you.beheld_by.size() - 1; i >= 0; i--)
        {
            const monsters* mon = &menv[you.beheld_by[i]];
            const coord_def pos = mon->pos();
            int walls = num_feats_between(you.x_pos, you.y_pos,
                                          pos.x, pos.y, DNGN_UNSEEN,
                                          DNGN_MAXWALL, true, true);

            if (walls > 0)
            {
                update_beholders(mon, true);
                if (you.beheld_by.empty())
                {
                    you.duration[DUR_BEHELD] = 0;
                    break;
                }
                continue;
            }
        }
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return (number_built > 0);
}                               // end entomb()

// Is (posx, posy) inside a circle within LOS, with the given radius,
// centered on the player's position?  If so, return the distance to it.
// Otherwise, return -1.
static int _inside_circle(int posx, int posy, int radius)
{
    if (!inside_level_bounds(posx, posy))
        return -1;

    const coord_def ep = grid2view(coord_def(posx, posy));
    if (!in_los_bounds(ep.x, ep.y))
        return -1;

    int dist = distance(posx, posy, you.x_pos, you.y_pos);
    if (dist > radius*radius)
        return -1;

    return (dist);
}

bool remove_sanctuary(bool did_attack)
{
    if (env.sanctuary_time)
        env.sanctuary_time = 0;

    if (!inside_level_bounds(env.sanctuary_pos))
        return (false);

    const int radius = 5;
    bool seen_change = false;
    for (int x = -radius; x <= radius; x++)
       for (int y = -radius; y <= radius; y++)
       {
          int posx = env.sanctuary_pos.x + x;
          int posy = env.sanctuary_pos.y + y;

          if (posx <= 0 || posx > GXM || posy <= 0 || posy > GYM)
              continue;

          if (is_sanctuary(posx, posy))
          {
              env.map[posx][posy].property = FPROP_NONE;
              if (see_grid(coord_def(posx,posy)))
                  seen_change = true;
          }
       }

//  do not reset so as to allow monsters to see if their fleeing source
//  used to be the centre of a sanctuary
//    env.sanctuary_pos.x = env.sanctuary_pos.y = -1;

    if (did_attack)
    {
        if (seen_change)
            simple_god_message(" revokes the gift of sanctuary.", GOD_ZIN);
        did_god_conduct(DID_FRIEND_DIED, 3);
    }

    if (is_resting())
        stop_running();

    return (true);
}

// For the last (radius) counter turns the sanctuary will slowly shrink.
void decrease_sanctuary_radius()
{
    int radius = 5;

    // For the last (radius-1) turns 33% chance of not decreasing.
    if (env.sanctuary_time < radius && one_chance_in(3))
        return;

    int size = --env.sanctuary_time;
    if (size >= radius)
        return;

    if (you.running && is_sanctuary(you.x_pos, you.y_pos))
    {
        mpr("The sanctuary starts shrinking.", MSGCH_DURATION);
        stop_running();
    }

    radius = size+1;
    for (int x = -radius; x <= radius; x++)
        for (int y = -radius; y <= radius; y++)
        {
            int posx = env.sanctuary_pos.x + x;
            int posy = env.sanctuary_pos.y + y;

            if (!inside_level_bounds(posx,posy))
                continue;

            int dist = distance(posx, posy, env.sanctuary_pos.x,
                                env.sanctuary_pos.y);

            // If necessary overwrite sanctuary property.
            if (dist > size*size)
                env.map[posx][posy].property = FPROP_NONE;
        }

    // Special case for time-out of sanctuary.
    if (!size)
    {
        env.map[env.sanctuary_pos.x][env.sanctuary_pos.y].property = FPROP_NONE;
        if (see_grid(coord_def(env.sanctuary_pos.x,env.sanctuary_pos.y)))
            mpr("The sanctuary disappears.", MSGCH_DURATION);
    }
}

bool cast_sanctuary(const int power)
{
    // Casting is disallowed while previous sanctuary in effect.
    // (Checked in abl-show.cc.)
    if (env.sanctuary_time)
        return (false);

    // Yes, shamelessly stolen from NetHack...
    if (!silenced(you.x_pos, you.y_pos)) // How did you manage that?
        mpr("You hear a choir sing!", MSGCH_SOUND);
    else
        mpr("You are suddenly bathed in radiance!");

    you.flash_colour = WHITE;
    viewwindow( true, false );
    holy_word(100, HOLY_WORD_ZIN, you.x_pos, you.y_pos, true);
#ifndef USE_TILE
    delay(1000);
#endif

    env.sanctuary_pos.x = you.x_pos;
    env.sanctuary_pos.y = you.y_pos;
    env.sanctuary_time = 7 + you.skills[SK_INVOCATIONS]/2;

    // Pets stop attacking and converge on you.
    you.pet_target = MHITYOU;

    // radius could also be influenced by Inv
    // and would then have to be stored globally.
    const int radius      = 5;
    const int pattern     = random2(4);
    int       blood_count = 0;
    int       trap_count  = 0;
    int       scare_count = 0;
    int       cloud_count = 0;
    monsters *seen_mon    = NULL;

    for (int x = -radius; x <= radius; x++)
        for (int y = -radius; y <= radius; y++)
        {
            int posx = you.x_pos + x;
            int posy = you.y_pos + y;
            int dist = _inside_circle(posx, posy, radius);

            if (dist == -1)
                continue;

            coord_def pos(posx, posy);
            if (env.map(pos).property == FPROP_BLOODY
                && see_grid(pos))
            {
                blood_count++;
            }

            if (trap_type_at_xy(posx, posy) != NUM_TRAPS
                && grd(pos) == DNGN_UNDISCOVERED_TRAP)
            {
                const dungeon_feature_type type =
                    trap_category( trap_type_at_xy(posx, posy) );
                grd(pos) = type;
                set_envmap_obj(posx, posy, type);
                trap_count++;
            }

            // forming patterns
            if (pattern == 0    // outward rays
                  && (x == 0 || y == 0 || x == y || x == -y)
                || pattern == 1 // circles
                  && (dist >= (radius-1)*(radius-1) && dist <= radius*radius
                      || dist >= (radius/2-1)*(radius/2-1)
                         && dist <= radius*radius/4)
               || pattern == 2 // latticed
                  && (x%2 == 0 || y%2 == 0)
                || pattern == 3 // cross-like
                  && (abs(x)+abs(y) < 5 && x != y && x != -y))
            {
                env.map[posx][posy].property = FPROP_SANCTUARY_1; // yellow
            }
            else
                env.map[posx][posy].property = FPROP_SANCTUARY_2; // white

            // scare all attacking monsters inside sanctuary, and make
            // all friendly monsters inside sanctuary stop attacking and
            // move towards the player.
            int monster = mgrd[posx][posy];
            if (monster != NON_MONSTER)
            {
                monsters* mon = &menv[monster];

                if (mons_friendly(mon))
                {
                   mon->foe       = MHITYOU;
                   mon->target_x  = you.x_pos;
                   mon->target_y  = you.y_pos;
                   mon->behaviour = BEH_SEEK;
                   behaviour_event(mon, ME_EVAL, MHITYOU);
                }
                else if (!mons_wont_attack(mon))
                {
                    if (mons_is_mimic(mon->type))
                    {
                        mimic_alert(mon);
                        if(you.can_see(mon))
                        {
                            scare_count++;
                            seen_mon = mon;
                        }
                    }
                    else if (mon->add_ench(mon_enchant(ENCH_FEAR, 0,
                             KC_YOU)))
                    {
                        behaviour_event(mon, ME_SCARE, MHITYOU);

                        // Check to see that monster is actually fleeing,
                        // since plants can't flee.
                        if (mons_is_fleeing(mon) && you.can_see(mon))
                        {
                            scare_count++;
                            seen_mon = mon;
                        }
                    }
                }
            } // if (monster != NON_MONSTER)

            if (!is_harmless_cloud(cloud_type_at(pos)))
            {
                delete_cloud(env.cgrid[posx][posy]);
                if (see_grid(pos))
                    cloud_count++;
            }
        } // radius loop

    if (trap_count > 0)
    {
        mpr("By Zin's power hidden traps are revealed to you.",
            MSGCH_GOD);
    }

    if (cloud_count == 1)
    {
        mpr("By Zin's power the foul cloud within the sanctuary is "
            "swept away.", MSGCH_GOD);
    }
    else if (cloud_count > 1)
    {
        mpr("By Zin's power all foul fumes within the sanctuary are "
            "swept away.", MSGCH_GOD);
    }

    if (blood_count > 0)
    {
        mpr("By Zin's power all blood is cleared from the sanctuary.",
            MSGCH_GOD);
    }

    if (scare_count == 1 && seen_mon != NULL)
        simple_monster_message(seen_mon, " turns to flee the light!");
    else if (scare_count > 0)
        mpr("The monsters scatter in all directions!");

    return (true);
}

int halo_radius()
{
    if (you.religion == GOD_SHINING_ONE && you.piety >= piety_breakpoint(0)
        && !you.penance[GOD_SHINING_ONE])
    {
        return std::min(LOS_RADIUS, you.piety / 20);
    }

    return 0;
}

bool inside_halo(int posx, int posy)
{
    if (!halo_radius())
        return (false);

    return (_inside_circle(posx, posy, halo_radius()) != -1);
}

void cast_poison_ammo(void)
{
    const int ammo = you.equip[EQ_WEAPON];

    if (ammo == -1
        || you.inv[ammo].base_type != OBJ_MISSILES
        || get_ammo_brand( you.inv[ammo] ) != SPMSL_NORMAL
        || you.inv[ammo].sub_type == MI_STONE
        || you.inv[ammo].sub_type == MI_SLING_BULLET
        || you.inv[ammo].sub_type == MI_LARGE_ROCK
        || you.inv[ammo].sub_type == MI_THROWING_NET)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return;
    }

    {
        preserve_quiver_slots q;
        const char *old_desc = you.inv[ammo].name(DESC_CAP_YOUR).c_str();
        if (set_item_ego_type( you.inv[ammo], OBJ_MISSILES, SPMSL_POISONED ))
        {
            mprf("%s %s covered in a thin film of poison.", old_desc,
                 (you.inv[ammo].quantity == 1) ? "is" : "are");

            if (ammo == you.equip[EQ_WEAPON])
                you.wield_change = true;
        }
        else
            canned_msg(MSG_NOTHING_HAPPENS);
    }
}

bool project_noise(void)
{
    bool success = false;

    coord_def pos(1, 0);

    mpr( "Choose the noise's source (press '.' or delete to select)." );
    more();
    show_map(pos, false);

    redraw_screen();

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Target square (%d,%d)", pos.x, pos.y );
#endif

    if (!silenced( pos.x, pos.y ))
    {
        if (in_bounds(pos) && !grid_is_solid(grd(pos)))
        {
            noisy(30, pos.x, pos.y);
            success = true;
        }

        if (!silenced( you.x_pos, you.y_pos ))
        {
            if (success)
            {
                mprf(MSGCH_SOUND, "You hear a %svoice call your name.",
                     (!see_grid( pos.x, pos.y ) ? "distant " : "") );
            }
            else
                mprf(MSGCH_SOUND, "You hear a dull thud.");
        }
    }

    return (success);
}

// Type recalled:
// 0 = anything
// 1 = undead only (Kiku/Yred religion ability)
// 2 = orcs only (Beogh religion ability)
bool recall(char type_recalled)
{
    int loopy          = 0;      // general purpose looping variable {dlb}
    bool success       = false;  // more accurately: "apparent success" {dlb}
    int start_count    = 0;
    int step_value     = 1;
    int end_count      = (MAX_MONSTERS - 1);

    FixedVector < char, 2 > empty;
    struct monsters *monster = 0;       // NULL {dlb}

    empty[0] = empty[1] = 0;

    // someone really had to make life difficult {dlb}:
    // sometimes goes through monster list backwards
    if (coinflip())
    {
        start_count = (MAX_MONSTERS - 1);
        end_count   = 0;
        step_value  = -1;
    }

    for (loopy = start_count; loopy != end_count + step_value;
         loopy += step_value)
    {
        monster = &menv[loopy];

        if (monster->type == -1)
            continue;

        if (!mons_friendly(monster))
            continue;

        if (!monster_habitable_grid(monster, DNGN_FLOOR))
            continue;

        if (type_recalled == 1) // undead
        {
            if (monster->type != MONS_REAPER
                && mons_holiness(monster) != MH_UNDEAD)
            {
                continue;
            }
        }
        else if (type_recalled == 2) // Beogh
        {
            if (!is_orcish_follower(monster))
                continue;
        }

        if (empty_surrounds(you.x_pos, you.y_pos, DNGN_FLOOR, 3, false, empty)
            && monster->move_to_pos( coord_def(empty[0], empty[1])) )
        {
            // only informed if monsters recalled are visible {dlb}:
            if (simple_monster_message(monster, " is recalled."))
                success = true;
        }
        else
            break;              // no more room to place monsters {dlb}
    }

    if (!success)
        mpr("Nothing appears to have answered your call.");

    return (success);
}                               // end recall()

// Restricted to main dungeon for historical reasons, probably for
// balance: otherwise you have an instant teleport from anywhere.
int portal()
{
    if (!player_in_branch( BRANCH_MAIN_DUNGEON ))
    {
        mpr("This spell doesn't work here.");
        return (-1);
    }
    else if (grd[you.x_pos][you.y_pos] != DNGN_FLOOR)
    {
        mpr("You must find a clear area in which to cast this spell.");
        return (-1);
    }
    else if (you.char_direction == GDT_ASCENDING)
    {
        // be evil if you've got the Orb
        mpr("An empty arch forms before you, then disappears.");
        return 1;
    }

    mpr("Which direction ('<' for up, '>' for down, 'x' to quit)?",
        MSGCH_PROMPT);

    int dir_sign = 0;
    while (dir_sign == 0)
    {
        const int keyin = getch();
        switch ( keyin )
        {
        case '<':
            if (you.your_level == 0)
                mpr("You can't go any further upwards with this spell.");
            else
                dir_sign = -1;
            break;

        case '>':
            if (you.your_level + 1 == your_branch().depth)
                mpr("You can't go any further downwards with this spell.");
            else
                dir_sign = 1;
            break;

        case 'x':
            canned_msg(MSG_OK);
            return (-1);

        default:
            break;
        }
    }

    mpr("How many levels (1 - 9, 'x' to quit)?", MSGCH_PROMPT);

    int amount = 0;
    while (amount == 0)
    {
        const int keyin = getch();
        if ( isdigit(keyin) )
            amount = (keyin - '0') * dir_sign;
        else if (keyin == 'x')
        {
            canned_msg(MSG_OK);
            return (-1);
        }
    }

    mpr( "You fall through a mystic portal, and materialise at the "
         "foot of a staircase." );
    more();

    const int old_level = you.your_level;
    you.your_level = std::max(0, std::min(26, you.your_level + amount)) - 1;
    down_stairs( old_level, DNGN_STONE_STAIRS_DOWN_I );

    return (1);
}
