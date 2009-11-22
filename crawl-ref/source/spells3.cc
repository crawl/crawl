/*
 *  File:       spells3.cc
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
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
#include "artefact.h"
#include "beam.h"
#include "branch.h"
#include "cloud.h"
#include "coordit.h"
#include "directn.h"
#include "debug.h"
#include "delay.h"
#include "effects.h"
#include "fprop.h"
#include "food.h"
#include "goditem.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-iter.h"
#include "mon-place.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "player.h"
#include "religion.h"
#include "spells1.h"
#include "spells4.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stash.h"
#include "stuff.h"
#include "areas.h"
#include "traps.h"
#include "travel.h"
#include "view.h"
#include "shout.h"
#include "viewgeom.h"
#include "xom.h"

bool cast_selective_amnesia(bool force)
{
    if (you.spell_no == 0)
    {
        mpr("You don't know any spells.");
        return (false);
    }

    int keyin = 0;

    // Pick a spell to forget.
    while (true)
    {
        mpr("Forget which spell ([?*] list [ESC] exit)? ", MSGCH_PROMPT);
        keyin = get_ch();

        if (keyin == ESCAPE)
            return (false);

        if (keyin == '?' || keyin == '*')
        {
            keyin = list_spells(false);
            redraw_screen();
        }

        if (!isalpha(keyin))
            mesclr(true);
        else
            break;
    }

    const spell_type spell = get_spell_by_letter(keyin);
    const int slot = get_spell_slot_by_letter(keyin);

    if (spell == SPELL_NO_SPELL)
    {
        mpr("You don't know that spell.");
        return (false);
    }

    if (!force
        && random2(you.skills[SK_SPELLCASTING])
           < random2(spell_difficulty(spell)))
    {
        mpr("Oops! This spell sure is a blunt instrument.");
        forget_map(20 + random2(50));
    }
    else
    {
        const int ep_gain = spell_mana(spell);
        del_spell_from_memory_by_slot(slot);

        if (ep_gain > 0)
        {
            inc_mp(ep_gain, false);
            mpr("The spell releases its latent energy back to you as "
                "it unravels.");
        }
    }

    return (true);
}

bool remove_curse(bool suppress_msg)
{
    bool success = false;

    // Only cursed *weapons* in hand count as cursed. - bwr
    if (you.weapon()
        && you.weapon()->base_type == OBJ_WEAPONS
        && item_cursed(*you.weapon()))
    {
        // Also sets wield_change.
        do_uncurse_item(*you.weapon());
        success = true;
    }

    // Everything else uses the same paradigm - are we certain?
    // What of artefact rings and amulets? {dlb}:
    for (int i = EQ_WEAPON + 1; i < NUM_EQUIP; i++)
    {
        // Melded equipment can also get uncursed this way.
        if (you.equip[i] != -1 && item_cursed(you.inv[you.equip[i]]))
        {
            do_uncurse_item(you.inv[you.equip[i]]);
            success = true;
        }
    }

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
    bool success = false;       // whether or not any curses found {dlb}

    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def& item = you.inv[i];

        if (item.is_valid()
            && (item.base_type == OBJ_WEAPONS
                || item.base_type == OBJ_ARMOUR
                || item.base_type == OBJ_JEWELLERY))
        {
            if (!item_ident(item, ISFLAG_KNOW_CURSE))
                success = true;

            set_ident_flags(item, ISFLAG_KNOW_CURSE);
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

bool cast_smiting(int power, const coord_def& where)
{
    monsters *m = monster_at(where);

    if (m == NULL)
    {
        mpr("There's nothing there!");
        // Counts as a real cast, due to victory-dancing and
        // invisible/submerged monsters.
        return (true);
    }

    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    const bool success = !stop_attack_prompt(m, false, you.pos());

    if (success)
    {
        set_attack_conducts(conducts, m);

        mprf("You smite %s!", m->name(DESC_NOCAP_THE).c_str());

        behaviour_event(m, ME_ANNOY, MHITYOU);
        if (mons_is_mimic(m->type))
            mimic_alert(m);
    }

    enable_attack_conducts(conducts);

    if (success)
    {
        // Maxes out at around 40 damage at 27 Invocations, which is
        // plenty in my book (the old max damage was around 70,
        // which seems excessive).
        m->hurt(&you, 7 + (random2(power) * 33 / 191));
        if (m->alive())
            print_wounds(m);
    }

    return (success);
}

int airstrike(int power, dist &beam)
{
    bool success = false;

    monsters *monster = monster_at(beam.target);

    if (monster == NULL)
        canned_msg(MSG_SPELL_FIZZLES);
    else
    {
        god_conduct_trigger conducts[3];
        disable_attack_conducts(conducts);

        success = !stop_attack_prompt(monster, false, you.pos());

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

            monster->hurt(&you, hurted);
            if (monster->alive())
                print_wounds(monster);
        }
    }

    return (success);
}

bool cast_bone_shards(int power, bolt &beam)
{
    if (!you.weapon() || you.weapon()->base_type != OBJ_CORPSES)
    {
        canned_msg(MSG_SPELL_FIZZLES);
        return (false);
    }

    bool success = false;

    const bool was_orc = (mons_species(you.weapon()->plus) == MONS_ORC);

    if (you.weapon()->sub_type != CORPSE_SKELETON)
    {
        mpr("The corpse collapses into a pulpy mess.");

        dec_inv_item_quantity(you.equip[EQ_WEAPON], 1);

        if (was_orc)
            did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2);
    }
    else
    {
        // Practical max of 100 * 15 + 3000 = 4500.
        // Actual max of    200 * 15 + 3000 = 6000.
        power *= 15;
        power += mons_weight(you.weapon()->plus);

        if (!player_tracer(ZAP_BONE_SHARDS, power, beam))
            return (false);

        mpr("The skeleton explodes into sharp fragments of bone!");

        dec_inv_item_quantity(you.equip[EQ_WEAPON], 1);

        if (was_orc)
            did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2);

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

            if (mons_species(you.inv[wielded].plus) == MONS_ORC)
                did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2);
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
            mpr("A conflicting enchantment prevents the spell from "
                "coming into effect.");
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
            mgen_data(mon, BEH_FRIENDLY, &you, dur, SPELL_CALL_IMP,
                      you.pos(), MHITYOU, MG_FORCE_BEH, god));

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

static bool _summon_demon_wrapper(int pow, god_type god, int spell,
                                  monster_type mon, int dur, bool friendly,
                                  bool charmed, bool quiet)
{
    bool success = false;

    const int monster =
        create_monster(
            mgen_data(mon,
                      friendly ? BEH_FRIENDLY :
                          charmed ? BEH_CHARMED : BEH_HOSTILE, &you,
                      dur, spell, you.pos(), MHITYOU, MG_FORCE_BEH, god));

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

static bool _summon_demon_wrapper(int pow, god_type god, int spell,
                                  demon_class_type dct, int dur, bool friendly,
                                  bool charmed, bool quiet)
{
    monster_type mon = summon_any_demon(dct);

    return _summon_demon_wrapper(pow, god, spell, mon, dur, friendly, charmed,
                                 quiet);
}

bool summon_lesser_demon(int pow, god_type god, int spell,
                         bool quiet)
{
    return _summon_demon_wrapper(pow, god, spell, DEMON_LESSER,
                                 std::min(2 + (random2(pow) / 4), 6),
                                 random2(pow) > 3, false, quiet);
}

bool summon_common_demon(int pow, god_type god, int spell,
                         bool quiet)
{
    return _summon_demon_wrapper(pow, god, spell, DEMON_COMMON,
                                 std::min(2 + (random2(pow) / 4), 6),
                                 random2(pow) > 3, false, quiet);
}

bool summon_greater_demon(int pow, god_type god, int spell,
                          bool quiet)
{
    return _summon_demon_wrapper(pow, god, spell, DEMON_GREATER,
                                 5, false, random2(pow) > 5, quiet);
}

bool summon_demon_type(monster_type mon, int pow, god_type god,
                       int spell)
{
    return _summon_demon_wrapper(pow, god, spell, mon,
                                 std::min(2 + (random2(pow) / 4), 6),
                                 random2(pow) > 3, false, false);
}

bool cast_summon_demon(int pow, god_type god)
{
    mpr("You open a gate to Pandemonium!");

    bool success = summon_common_demon(pow, god, SPELL_SUMMON_DEMON);

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
        if (summon_lesser_demon(pow, god, SPELL_DEMONIC_HORDE, true))
            success = true;
    }

    if (!success)
        canned_msg(MSG_NOTHING_HAPPENS);

    return (success);
}

bool cast_summon_greater_demon(int pow, god_type god)
{
    mpr("You open a gate to Pandemonium!");

    bool success = summon_greater_demon(pow, god, SPELL_SUMMON_GREATER_DEMON);

    if (!success)
        canned_msg(MSG_NOTHING_HAPPENS);

    return (success);
}

bool cast_shadow_creatures(god_type god)
{
    mpr("Wisps of shadow whirl around you...");

    const int monster =
        create_monster(
            mgen_data(RANDOM_MONSTER, BEH_FRIENDLY, &you,
                      2, SPELL_SHADOW_CREATURES,
                      you.pos(), MHITYOU,
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
        canned_msg(MSG_NOTHING_HAPPENS);
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
    how_many_big   = std::min(8, how_many_big);

    int count = 0;

    while (how_many_big-- > 0)
    {
        const int monster =
            create_monster(
               mgen_data(MONS_TENTACLED_MONSTROSITY, BEH_FRIENDLY, &you,
                         6, SPELL_SUMMON_HORRIBLE_THINGS,
                         you.pos(), MHITYOU,
                         MG_FORCE_BEH, god));

        if (monster != -1)
        {
            count++;
            player_angers_monster(&menv[monster]);
        }
    }

    while (how_many_small-- > 0)
    {
        const int monster =
            create_monster(
               mgen_data(MONS_ABOMINATION_LARGE, BEH_FRIENDLY, &you,
                         6, SPELL_SUMMON_HORRIBLE_THINGS,
                         you.pos(), MHITYOU,
                         MG_FORCE_BEH, god));

        if (monster != -1)
        {
            count++;
            player_angers_monster(&menv[monster]);
        }
    }

    if (count == 0)
        canned_msg(MSG_NOTHING_HAPPENS);

    return (count > 0);
}

bool receive_corpses(int pow, coord_def where)
{
#if DEBUG_DIAGNOSTICS
    // pow = invocations * 4, ranges from 0 to 108
    mprf(MSGCH_DIAGNOSTICS, "receive_corpses() power: %d", pow);
#endif

    // Kiku gives branch-appropriate corpses (like shadow creatures).
    int expected_extra_corpses = 3 + pow / 18; // 3 at 0 Inv, 9 at 27 Inv.
    int corpse_delivery_radius = 1;

    // We should get the same number of corpses
    // in a hallway as in an open room.
    int spaces_for_corpses = 0;
    for (radius_iterator ri(where, corpse_delivery_radius, C_ROUND,
                            &you.get_los(), true);
         ri; ++ri)
    {
        if (mons_class_can_pass(MONS_HUMAN, grd(*ri)))
            spaces_for_corpses++;
    }

    int percent_chance_a_square_receives_extra_corpse = // can be > 100
        int(float(expected_extra_corpses) / float(spaces_for_corpses) * 100.0);

    int corpses_created = 0;

    for (radius_iterator ri(where, corpse_delivery_radius, C_ROUND,
                            &you.get_los());
         ri; ++ri)
    {
        bool square_is_walkable = mons_class_can_pass(MONS_HUMAN, grd(*ri));
        bool square_is_player_square = (*ri == where);
        bool square_gets_corpse =
            (random2(100) < percent_chance_a_square_receives_extra_corpse)
            || (square_is_player_square && random2(100) < 97);

        if (!square_is_walkable || !square_gets_corpse)
            continue;

        corpses_created++;

        // Find an appropriate monster corpse for level and power.
        monster_type mon_type = MONS_PROGRAM_BUG;
        int adjusted_power = 0;
        for (int i = 0; i < 200 && !mons_class_can_be_zombified(mon_type); ++i)
        {
            adjusted_power = std::min(pow / 4, random2(random2(pow)));
            mon_type = pick_local_zombifiable_monster_type(adjusted_power);
        }

        // Create corpse object.
        monsters dummy;
        dummy.type = mon_type;
        int index_of_corpse_created = get_item_slot();

        if (mons_genus(mon_type) == MONS_HYDRA)
            dummy.number = random2(20) + 1;

        int valid_corpse = fill_out_corpse(&dummy,
                                           mitm[index_of_corpse_created],
                                           false);
        if (valid_corpse == -1)
        {
            mitm[index_of_corpse_created].clear();
            continue;
        }

        mitm[index_of_corpse_created].props["DoNotDropHide"] = true;

        ASSERT(valid_corpse >= 0);

        // Higher piety means fresher corpses.  One out of ten corpses
        // will always be rotten.
        int rottedness = 200 -
            (!one_chance_in(10) ? random2(200 - you.piety)
                                : random2(100 + random2(75)));
        mitm[index_of_corpse_created].special = rottedness;

        // Place the corpse.
        move_item_to_grid(&index_of_corpse_created, *ri);
    }

    if (corpses_created)
    {
        if (you.religion == GOD_KIKUBAAQUDGHA)
        {
            simple_god_message(corpses_created > 1 ? " delivers you corpses!"
                                                   : " delivers you a corpse!");
        }
        maybe_update_stashes();
        return (true);
    }
    else
    {
        if (you.religion == GOD_KIKUBAAQUDGHA)
            simple_god_message(" can find no cadavers for you!");
        return (false);
    }
}

static bool _animatable_remains(const item_def& item)
{
    return (item.base_type == OBJ_CORPSES
        && mons_class_can_be_zombified(item.plus));
}

// Try to equip the skeleton/zombie/spectral thing with the objects it
// died with.  This excludes items which were dropped by the player onto
// the corpse, and corpses which were picked up and moved by the player,
// so the player can't equip their undead slaves with items of their
// choice.
//
// The item selection logic has one problem: if a first monster without
// any items dies and leaves a corpse, and then a second monster with
// items dies on the same spot but doesn't leave a corpse, then the
// undead can be equipped with the second monster's items if the second
// monster is either of the same type as the first, or if the second
// monster wasn't killed by the player or a player's pet.
void equip_undead(const coord_def &a, int corps, int monster, int monnum)
{
    monsters* mon = &menv[monster];

    if (mons_class_itemuse(monnum) < MONUSE_STARTING_EQUIPMENT)
        return;

    // If the player picked up and dropped the corpse, then all its
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

        if (item.base_type != OBJ_CORPSES && first_obj == NON_ITEM)
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

    // This handles e.g. spectral things that are Yredelemnul's enslaved
    // intact souls.
    const bool smart_undead = mons_intel(mon) >= I_NORMAL;

    for (int i = item_list.size() - 1; i >= 0; --i)
    {
        objl = item_list[i];
        item_def &item(mitm[objl]);

        // Stop equipping monster if the item probably didn't originally
        // belong to the monster.
        if ((origin_known(item) && (item.orig_monnum - 1) != monnum)
            || (item.flags & (ISFLAG_DROPPED | ISFLAG_THROWN))
            || item.base_type == OBJ_CORPSES)
        {
            return;
        }

        // Don't equip the undead with holy items.
        if (is_holy_item(item))
            continue;

        mon_inv_type mslot;

        switch (item.base_type)
        {
        case OBJ_WEAPONS:
        {
            const bool weapon = mon->inv[MSLOT_WEAPON] != NON_ITEM;
            const bool alt_weapon = mon->inv[MSLOT_ALT_WEAPON] != NON_ITEM;

            if ((weapon && !alt_weapon) || (!weapon && alt_weapon))
                mslot = !weapon ? MSLOT_WEAPON : MSLOT_ALT_WEAPON;
            else
                mslot = MSLOT_WEAPON;

            // Stupid undead can't use ranged weapons.
            if (smart_undead || !is_range_weapon(item))
                break;

            continue;
        }

        case OBJ_ARMOUR:
            mslot = equip_slot_to_mslot(get_armour_slot(item));

            // A piece of armour which can't be worn indicates that this
            // and further items weren't the equipment the monster died
            // with.
            if (mslot == NUM_MONSTER_SLOTS)
                return;
            break;

        // Stupid undead can't use missiles.
        case OBJ_MISSILES:
            if (smart_undead)
            {
                mslot = MSLOT_MISSILE;
                break;
            }
            continue;

        case OBJ_GOLD:
            mslot = MSLOT_GOLD;
            break;

        // Stupid undead can't use wands.
        case OBJ_WANDS:
            if (smart_undead)
            {
                mslot = MSLOT_WAND;
                break;
            }
            continue;

        // Stupid undead can't use scrolls.
        case OBJ_SCROLLS:
            if (smart_undead)
            {
                mslot = MSLOT_SCROLL;
                break;
            }
            continue;

        // Stupid undead can't use potions.
        case OBJ_POTIONS:
            if (smart_undead)
            {
                mslot = MSLOT_POTION;
                break;
            }
            continue;

        // Stupid undead can't use miscellaneous objects.
        case OBJ_MISCELLANY:
            if (smart_undead)
            {
                mslot = MSLOT_MISCELLANY;
                break;
            }
            continue;

        default:
            continue;
        }

        // Two different items going into the same slot indicate that
        // this and further items weren't equipment the monster died
        // with.
        if (mon->inv[mslot] != NON_ITEM)
            return;

        unwind_var<int> save_speedinc(mon->speed_increment);
        mon->pickup_item(mitm[objl], false, true);
    }
}

static bool _raise_remains(const coord_def &pos, int corps, beh_type beha,
                           unsigned short hitting, actor *as, std::string nas,
                           god_type god, bool actual, bool force_beh,
                           int* mon_index)
{
    if (mon_index != NULL)
        *mon_index = -1;

    const item_def& item = mitm[corps];

    if (!_animatable_remains(item))
        return (false);

    if (!actual)
        return (true);

    const monster_type zombie_type =
        static_cast<monster_type>(item.plus);

    const int number = (item.props.exists(MONSTER_NUMBER)) ?
                           item.props[MONSTER_NUMBER].get_short() : 0;

    // Headless hydras cannot be raised, sorry.
    if (zombie_type == MONS_HYDRA && number == 0)
    {
        if (you.see_cell(pos))
        {
            mpr("The zero-headed hydra corpse sways and immediately "
                "collapses!");
        }
        return (false);
    }

    monster_type mon = MONS_PROGRAM_BUG;

    if (item.sub_type == CORPSE_BODY)
    {
        mon = (mons_zombie_size(item.plus) == Z_SMALL) ? MONS_ZOMBIE_SMALL
                                                       : MONS_ZOMBIE_LARGE;
    }
    else
    {
        mon = (mons_zombie_size(item.plus) == Z_SMALL) ? MONS_SKELETON_SMALL
                                                       : MONS_SKELETON_LARGE;
    }

    mgen_data mg(mon, beha, as, 0, 0, pos, hitting, MG_FORCE_BEH, god,
                 zombie_type, number);

    mg.non_actor_summoner = nas;

    int monster = create_monster(mg);

    if (mon_index != NULL)
        *mon_index = monster;

    if (monster == -1)
        return (false);

    const int monnum = item.orig_monnum - 1;

    if (is_named_corpse(item))
    {
        unsigned long name_type;
        std::string name = get_corpse_name(item, &name_type);

        if (name_type == 0 || name_type == MF_NAME_REPLACE)
            name_zombie(&menv[monster], monnum, name);
    }

    equip_undead(pos, corps, monster, monnum);

    destroy_item(corps);

    if (!force_beh)
        player_angers_monster(&menv[monster]);

    return (true);
}

// Note that quiet will *not* suppress the message about a corpse
// you are butchering being animated.
int animate_remains(const coord_def &a, corpse_type class_allowed,
                    beh_type beha, unsigned short hitting,
                    actor *as, std::string nas,
                    god_type god, bool actual,
                    bool quiet, bool force_beh,
                    int* mon_index)
{
    if (is_sanctuary(a))
        return (0);

    int number_found = 0;
    bool success = false;

    // Search all the items on the ground for a corpse.
    for (stack_iterator si(a); si; ++si)
    {
        if (si->base_type == OBJ_CORPSES
            && (class_allowed == CORPSE_BODY
                || si->sub_type == CORPSE_SKELETON))
        {
            number_found++;

            if (!_animatable_remains(*si))
                continue;

            const bool was_butchering = is_being_butchered(*si);

            success = _raise_remains(a, si.link(), beha, hitting, as, nas,
                                     god, actual, force_beh, mon_index);

            if (actual && success)
            {
                // Ignore quiet.
                if (was_butchering)
                {
                    mprf("The corpse you are butchering rises to %s!",
                         beha == BEH_FRIENDLY ? "join your ranks"
                                              : "attack");
                }

                if (!quiet && you.see_cell(a))
                    mpr("The dead are walking!");

                if (was_butchering)
                    xom_is_stimulated(255);
            }

            break;
        }
    }

    if (number_found == 0)
        return (-1);

    if (!success)
        return (0);

    return (1);
}

int animate_dead(actor *caster, int pow, beh_type beha, unsigned short hitting,
                 actor *as, std::string nas, god_type god, bool actual)
{
    UNUSED(pow);

    int number_raised = 0;
    int number_seen   = 0;

    radius_iterator ri(caster->pos(), 6, C_SQUARE,
                       &caster->get_los_no_trans());

    for (; ri; ++ri)
    {
        // This will produce a message if the corpse you are butchering
        // is raised.
        if (animate_remains(*ri, CORPSE_BODY, beha, hitting, as, nas, god,
                            actual, true) > 0)
        {
            number_raised++;
            if (you.see_cell(*ri))
                number_seen++;
        }
    }

    if (actual && number_seen > 0)
        mpr("The dead are walking!");

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
    int count = 0;

    const item_def* weapon = you.weapon();

    if (weapon
        && (weapon->base_type == OBJ_CORPSES
            || (weapon->base_type == OBJ_FOOD
                && weapon->sub_type == FOOD_CHUNK)))
    {
        const monster_type type = static_cast<monster_type>(weapon->plus);
        const monster_type sim_type = mons_zombie_size(type) == Z_BIG ?
            MONS_SIMULACRUM_LARGE : MONS_SIMULACRUM_SMALL;

        // Can't create more than the available chunks.
        int how_many = std::min(8, 4 + random2(pow) / 20);
        how_many = std::min<int>(how_many, weapon->quantity);

        for (int i = 0; i < how_many; ++i)
        {
            const int monster =
                create_monster(
                    mgen_data(sim_type, BEH_FRIENDLY, &you,
                              6, SPELL_SIMULACRUM,
                              you.pos(), MHITYOU,
                              MG_FORCE_BEH, god, type));

            if (monster != -1)
            {
                count++;

                dec_inv_item_quantity(you.equip[EQ_WEAPON], 1);

                player_angers_monster(&menv[monster]);
            }
        }

        if (count == 0)
            canned_msg(MSG_NOTHING_HAPPENS);
    }
    else
    {
        mpr("You need to wield a piece of raw flesh for this spell to be "
            "effective!");
    }

    return (count > 0);
}

bool cast_twisted_resurrection(int pow, god_type god)
{
    int how_many_corpses = 0;
    int how_many_orcs = 0;
    int total_mass = 0;
    int rotted = 0;

    for (stack_iterator si(you.pos()); si; ++si)
    {
        if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY)
        {
            total_mass += mons_weight(si->plus);
            how_many_corpses++;
            if (mons_species(si->plus) == MONS_ORC)
                how_many_orcs++;
            if (food_is_rotten(*si))
                rotted++;
            destroy_item(si->index());
        }
    }

    if (how_many_corpses == 0)
    {
        mpr("There are no corpses here!");
        return (false);
    }

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Mass for abomination: %d", total_mass);
#endif

    // This is what the old statement pretty much boils down to,
    // the average will be approximately 10 * pow (or about 1000
    // at the practical maximum).  That's the same as the mass
    // of a hippogriff, a spiny frog, or a steam dragon.  Thus,
    // material components are far more important to this spell. - bwr
    total_mass += roll_dice(20, pow);

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Mass including power bonus: %d", total_mass);
#endif

    if (total_mass < 400 + roll_dice(2, 500)
        || how_many_corpses < (coinflip() ? 3 : 2))
    {
        mprf("The corpse%s collapse%s into a pulpy mess.",
             how_many_corpses > 1 ? "s": "", how_many_corpses > 1 ? "": "s");

        if (how_many_orcs > 0)
            did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2 * how_many_orcs);

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
            mgen_data(mon, BEH_FRIENDLY, &you,
                      0, 0,
                      you.pos(), MHITYOU,
                      MG_FORCE_BEH, god,
                      MONS_NO_MONSTER, 0, colour));

    if (monster == -1)
    {
        mpr("The corpses collapse into a pulpy mess.");

        if (how_many_orcs > 0)
            did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2 * how_many_orcs);

        return (false);
    }

    // Mark this abomination as undead.
    menv[monster].flags |= MF_HONORARY_UNDEAD;

    mpr("The heap of corpses melds into an agglomeration of writhing flesh!");

    if (how_many_orcs > 0)
        did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2 * how_many_orcs);

    if (mon == MONS_ABOMINATION_LARGE)
    {
        menv[monster].hit_dice = 8 + total_mass / ((colour == LIGHTRED) ? 500 :
                                                   (colour == RED)      ? 1000
                                                                        : 2500);

        menv[monster].hit_dice = std::min(30, menv[monster].hit_dice);

        // XXX: No convenient way to get the hit dice size right now.
        menv[monster].hit_points = hit_points(menv[monster].hit_dice, 2, 5);
        menv[monster].max_hit_points = menv[monster].hit_points;

        if (colour == LIGHTRED)
            menv[monster].ac += total_mass / 1000;
    }

    player_angers_monster(&menv[monster]);

    return (true);
}

bool cast_haunt(int pow, const coord_def& where, god_type god)
{
    monsters *m = monster_at(where);

    if (m == NULL)
    {
        mpr("An evil force gathers, but it quickly dissipates.");
        return (true);
    }

    int mi = m->mindex();
    ASSERT(!invalid_monster_index(mi));

    if (stop_attack_prompt(m, false, you.pos()))
        return (false);

    bool friendly = true;
    int success = 0;
    int to_summon = stepdown_value(2 + (random2(pow) / 10) + (random2(pow) / 10),
                                   2, 2, 6, -1);

    while (to_summon--)
    {
        const int chance = random2(25);
        monster_type mon = ((chance > 22) ? MONS_PHANTOM :          //  8%
                            (chance > 20) ? MONS_HUNGRY_GHOST :     //  8%
                            (chance > 18) ? MONS_FLAYED_GHOST :     //  8%
                            (chance >  7) ? MONS_WRAITH :           // 44%/40%
                            (chance >  2) ? MONS_FREEZING_WRAITH    // 20%/16%
                                          : MONS_SPECTRAL_WARRIOR); // 12%

        if ((chance == 3 || chance == 8) && you.can_see_invisible())
            mon = MONS_SHADOW_WRAITH;                               //  0%/8%

        const int monster =
            create_monster(
                mgen_data(mon,
                          BEH_FRIENDLY, &you,
                          5, SPELL_HAUNT,
                          where, mi, MG_FORCE_BEH, god));

        if (monster != -1)
        {
            success++;

            if (player_angers_monster(&menv[monster]))
                friendly = false;
        }
    }

    if (success > 1)
        mpr(friendly ? "Insubstantial figures form in the air."
                     : "You sense hostile presences.");
    else if (success)
        mpr(friendly ? "An insubstantial figure forms in the air."
                     : "You sense a hostile presence.");
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    //jmf: Kiku sometimes deflects this
    if (you.religion != GOD_KIKUBAAQUDGHA
        || player_under_penance() || you.piety < piety_breakpoint(3)
        || !x_chance_in_y(you.piety, MAX_PIETY))
    {
        you.sicken(25 + random2(50));
    }

    return (success);
}

bool cast_death_channel(int pow, god_type god)
{
    bool success = false;

    if (you.duration[DUR_DEATH_CHANNEL] < 30 * BASELINE_DELAY)
    {
        success = true;

        mpr("Malign forces permeate your being, awaiting release.");

        you.increase_duration(DUR_DEATH_CHANNEL, 15 + random2(1 + pow/3), 100);

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
    if (scan_artefacts(ARTP_PREVENT_TELEPORTATION))
        mpr("You feel a weird sense of stasis.");
    else if (you.duration[DUR_TELEPORT])
    {
        mpr("You feel strangely stable.");
        you.duration[DUR_TELEPORT] = 0;
    }
    else
    {
        mpr("You feel strangely unstable.");

        int teleport_delay = 3 + random2(3);

        if (you.level_type == LEVEL_ABYSS && !one_chance_in(5))
        {
            mpr("You have a feeling this translocation may take a while to kick in...");
            teleport_delay += 5 + random2(10);
        }

        you.set_duration(DUR_TELEPORT, teleport_delay);
    }
}

static bool _teleport_player(bool allow_control, bool new_abyss_area)
{
    bool is_controlled = (allow_control && !you.confused()
                          && player_control_teleport()
                          && allow_control_teleport());

    if (scan_artefacts(ARTP_PREVENT_TELEPORTATION))
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
    viewwindow(false, true);
    StashTrack.update_stash();

    if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
        remove_condensation_shield();

    if (you.level_type == LEVEL_ABYSS)
    {
        abyss_teleport( new_abyss_area );
        if (you.pet_target != MHITYOU)
            you.pet_target = MHITNOT;

        return (true);
    }

    coord_def pos(1, 0);
    bool      large_change  = false;
    bool      check_ring_TC = false;
#ifdef USE_TILE
    const dungeon_feature_type old_grid = grd(you.pos());
#endif

    if (is_controlled)
    {
        mpr("You may choose your destination (press '.' or delete to select).");
        mpr("Expect minor deviation.");
        check_ring_TC = true;
        more();

        while (true)
        {
            level_pos lpos;
            show_map(lpos, false, true);
            pos = lpos.pos;
            redraw_screen();

#if defined(USE_UNIX_SIGNALS) && defined(SIGHUP_SAVE) && defined(USE_CURSES)
            // If we've received a HUP signal then the user can't choose a
            // location, so cancel the teleport.
            if (crawl_state.seen_hups)
            {
                mpr("Controlled teleport interrupted by HUP signal, "
                    "cancelling teleport.", MSGCH_ERROR);
                you.turn_is_over = false;
                return (false);
            }
#endif

#if DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Target square (%d,%d)", pos.x, pos.y );
#endif

            if (pos == you.pos() || pos == coord_def(-1,-1))
            {
                if (!yesno("Are you sure you want to cancel this teleport?",
                           true, 'n'))
                    continue;
                you.turn_is_over = false;
                return (false);
            }

            monsters *beholder = you.get_beholder(pos);
            if (beholder)
            {
                mprf("You cannot teleport away from %s!",
                     beholder->name(DESC_NOCAP_THE, true).c_str());
                mpr("Choose another destination (press '.' or delete to select).");
                more();
                continue;
            }

            break;
        }

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
             "Scattered target square (%d, %d)", pos.x, pos.y);
#endif

        if (is_controlled)
        {
            if (!you.see_cell(pos))
                large_change = true;

            // Merfolk should be able to control-tele into deep water.
            if (grd(pos) != DNGN_FLOOR
                    && grd(pos) != DNGN_SHALLOW_WATER
                    && (you.species != SP_MERFOLK
                        || grd(pos) != DNGN_DEEP_WATER)
                || monster_at(pos)
                || env.cgrid(pos) != EMPTY_CLOUD)
            {
                is_controlled = false;
                large_change  = false;
            }
            else
            {
                // Leave a purple cloud.
                place_cloud(CLOUD_TLOC_ENERGY, you.pos(), 1 + random2(3), KC_YOU);

                // Controlling teleport contaminates the player. - bwr
                move_player_to_grid(pos, false, true, true);
                contaminate_player(1, true);
            }
        }
    }

    if (!is_controlled)
    {
        coord_def newpos;

        // If in a labyrinth, always teleport well away from the centre.
        // (Check done for the straight line, no pathfinding involved.)
        bool need_distance_check = false;
        coord_def centre;
        if (you.level_type == LEVEL_LABYRINTH)
        {
            bool success = false;
            for (int xpos = 0; xpos < GXM; xpos++)
            {
                for (int ypos = 0; ypos < GYM; ypos++)
                {
                    centre = coord_def(xpos, ypos);
                    if (!in_bounds(centre))
                        continue;

                    if (grd(centre) == DNGN_ESCAPE_HATCH_UP)
                    {
                        success = true;
                        break;
                    }
                }
                if (success)
                    break;
            }
            need_distance_check = success;
        }

        do
            newpos = random_in_bounds();
        while (grd(newpos) != DNGN_FLOOR
                   && grd(newpos) != DNGN_SHALLOW_WATER
                   && (you.species != SP_MERFOLK
                       || grd(newpos) != DNGN_DEEP_WATER)
               || monster_at(newpos)
               || env.cgrid(newpos) != EMPTY_CLOUD
               || need_distance_check && (newpos - centre).abs() < 34*34
               || testbits(env.pgrid(newpos), FPROP_NO_RTELE_INTO));

        if (newpos == you.pos())
            mpr("Your surroundings flicker for a moment.");
        else if (you.see_cell(newpos))
            mpr("Your surroundings seem slightly different.");
        else
        {
            mpr("Your surroundings suddenly seem different.");
            large_change = true;
        }

        // Leave a purple cloud.
        place_cloud(CLOUD_TLOC_ENERGY, you.pos(), 1 + random2(3), KC_YOU);

        move_player_to_grid(newpos, false, true, true);
    }

    if (large_change)
        handle_interrupted_swap(true);

    // Might identify unknown ring of teleport control.
    if (check_ring_TC)
        maybe_id_ring_TC();

#ifdef USE_TILE
    if (you.species == SP_MERFOLK)
    {
        const dungeon_feature_type new_grid = grd(you.pos());
        if (feat_is_water(old_grid) && !feat_is_water(new_grid)
            || !feat_is_water(old_grid) && feat_is_water(new_grid))
        {
            init_player_doll();
        }
    }
#endif

    return (!is_controlled);
}

void you_teleport_now(bool allow_control, bool new_abyss_area)
{
    const bool randtele = _teleport_player(allow_control, new_abyss_area);

    // Xom is amused by uncontrolled teleports that land you in a
    // dangerous place, unless the player is in the Abyss and
    // teleported to escape from all the monsters chasing him/her,
    // since in that case the new dangerous area is almost certainly
    // *less* dangerous than the old dangerous area.
    // Teleporting in a labyrinth is also funny, more so for non-minotaurs.
    if (randtele
        && (you.level_type == LEVEL_LABYRINTH
            || you.level_type != LEVEL_ABYSS && player_in_a_dangerous_place()))
    {
        if (you.level_type == LEVEL_LABYRINTH && you.species == SP_MINOTAUR)
            xom_is_stimulated(128);
        else
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

    for ( adjacent_iterator ai(you.pos()); ai; ++ai )
    {
        // Tile already occupied by monster
        if (monster_at(*ai))
            continue;

        // This is where power comes in.
        if ( one_chance_in(powc/5) )
            continue;

        bool proceed = false;
        for (unsigned int i=0; i < ARRAYSZ(safe_to_overwrite) && !proceed; ++i)
            if (grd(*ai) == safe_to_overwrite[i])
                proceed = true;

        // checkpoint one - do we have a legitimate tile? {dlb}
        if (!proceed)
            continue;

        // hate to see the orb get destroyed by accident {dlb}:
        for ( stack_iterator si(*ai); si && proceed; ++si )
            if (si->base_type == OBJ_ORBS)
                proceed = false;

        // checkpoint two - is the orb resting in the tile? {dlb}:
        if (!proceed)
            continue;

        // Destroy all items on the square.
        for ( stack_iterator si(*ai); si; ++si )
            destroy_item(si->index());

        // deal with clouds {dlb}:
        if (env.cgrid(*ai) != EMPTY_CLOUD)
            delete_cloud( env.cgrid(*ai) );

        // All traps are destroyed
        if (trap_def* ptrap = find_trap(*ai))
            ptrap->destroy();

        // Finally, place the wall {dlb}:
        grd(*ai) = DNGN_ROCK_WALL;
        number_built++;
    }

    if (number_built > 0)
    {
        mpr("Walls emerge from the floor!");
        you.update_beholders();
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    return (number_built > 0);
}

bool cast_sanctuary(const int power)
{
    // Casting is disallowed while previous sanctuary in effect.
    // (Checked in abl-show.cc.)
    if (env.sanctuary_time)
        return (false);

    // Yes, shamelessly stolen from NetHack...
    if (!silenced(you.pos())) // How did you manage that?
        mpr("You hear a choir sing!", MSGCH_SOUND);
    else
        mpr("You are suddenly bathed in radiance!");

    flash_view(WHITE);

    holy_word(100, HOLY_WORD_ZIN, you.pos(), true);

#ifndef USE_TILE
    // Allow extra time for the flash to linger.
    delay(1000);
#endif

    // Pets stop attacking and converge on you.
    you.pet_target = MHITYOU;

    create_sanctuary(you.pos(), 7 + you.skills[SK_INVOCATIONS] / 2);

    return (true);
}

bool project_noise(void)
{
    bool success = false;

    coord_def pos(1, 0);
    level_pos lpos;

    mpr( "Choose the noise's source (press '.' or delete to select)." );
    more();
    show_map(lpos, false);
    pos = lpos.pos;

    redraw_screen();

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Target square (%d,%d)", pos.x, pos.y );
#endif

    if (!silenced( pos ))
    {
        if (in_bounds(pos) && !feat_is_solid(grd(pos)))
        {
            noisy(30, pos);
            success = true;
        }

        if (!silenced( you.pos() ))
        {
            if (success)
            {
                mprf(MSGCH_SOUND, "You hear a %svoice call your name.",
                     (!you.see_cell(pos) ? "distant " : "") );
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

    monsters *monster = 0;

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

        if (monster->type == MONS_NO_MONSTER)
            continue;

        if (!monster->friendly())
            continue;

        if (mons_class_is_stationary(monster->type))
            continue;

        if (!monster_habitable_grid(monster, DNGN_FLOOR))
            continue;

        if (type_recalled == 1) // undead
        {
            if (monster->holiness() != MH_UNDEAD)
                continue;
        }
        else if (type_recalled == 2) // Beogh
        {
            if (!is_orcish_follower(monster))
                continue;
        }

        coord_def empty;
        if (empty_surrounds(you.pos(), DNGN_FLOOR, 3, false, empty)
            && monster->move_to_pos( empty ) )
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
}

// Restricted to main dungeon for historical reasons, probably for
// balance: otherwise you have an instant teleport from anywhere.
int portal()
{
    if (!player_in_branch(BRANCH_MAIN_DUNGEON))
    {
        mpr("This spell doesn't work here.");
        return (-1);
    }
    else if (grd(you.pos()) != DNGN_FLOOR)
    {
        mpr("You must find a clear area in which to cast this spell.");
        return (-1);
    }
    else if (you.char_direction == GDT_ASCENDING)
    {
        // Be evil if you've got the Orb.
        mpr("An empty arch forms before you, then disappears.");
        return (1);
    }

    mpr("Which direction ('<' for up, '>' for down, 'x' to quit)? ",
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

    mpr("How many levels (1-9, 'x' to quit)? ", MSGCH_PROMPT);

    int amount = 0;
    while (amount == 0)
    {
        const int keyin = getch();
        if (isdigit(keyin))
            amount = (keyin - '0') * dir_sign;
        else if (keyin == 'x')
        {
            canned_msg(MSG_OK);
            return (-1);
        }
    }

    mpr("You fall through a mystic portal, and materialise at the "
        "foot of a staircase.");
    more();

    const int old_level = you.your_level;
    you.your_level = std::max(0, std::min(26, you.your_level + amount)) - 1;
    down_stairs(old_level, DNGN_STONE_STAIRS_DOWN_I);

    return (1);
}
