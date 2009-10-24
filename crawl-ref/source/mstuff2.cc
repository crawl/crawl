/*
 *  File:       mstuff2.cc
 *  Summary:    Misc monster related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <6>       april 2009   Cha      gave useful abilities to friendly silver
                                         statues and friendly orange crystal
                                         statues
 *      <5>     31 July 2000   JDJ      Fixed mon_throw to use lnchType.
 *      <4>     29 July 2000   JDJ      Tweaked mons_throw so it doesn't index past
 *                                      the end of the array when monsters don't have
 *                                      a weapon equipped.
 *      <3>     25 July 2000   GDL      Fixed Manticores
 *      <2>     28 July 2000   GDL      Revised monster throwing
 *      <1>     -/--/--        LRH      Created
 */

#include "AppHdr.h"
#include "mstuff2.h"

#include <string>
#include <string.h>
#include <stdio.h>
#include <algorithm>

#include "externs.h"

#include "beam.h"
#include "debug.h"
#include "effects.h"
#include "item_use.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mon-util.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "spells3.h"
#include "spells4.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"
#include "traps.h"
#include "view.h"

static int _monster_abjuration(const monsters *caster, bool actual);

// NB: only works because grid location already verified
//     to be some sort of trap prior to function call: {dlb}
void mons_trap(monsters *monster)
{
    if (!is_trap_square(grd[monster->x][monster->y]))
        return;

    int temp_rand = 0;          // probability determination {dlb}

    // single calculation permissible {dlb}
    bool monsterNearby = mons_near(monster);

    // new function call {dlb}
    int which_trap = trap_at_xy(monster->x, monster->y);
    if (which_trap == -1)
        return;

    bool trapKnown = (grd[monster->x][monster->y] != DNGN_UNDISCOVERED_TRAP);
    bool revealTrap = false;    // more sophisticated trap uncovering {dlb}
    bool projectileFired = false;       // <sigh> I had to do it, I swear {dlb}
    int damage_taken = -1;      // must initialize at -1 for this f(x) {dlb}

    struct bolt beem;


    // flying monsters neatly avoid mechanical traps
    // and may actually exit this function early: {dlb}
    if (mons_flies(monster))
    {
        if (trap_category(env.trap[which_trap].type) == DNGN_TRAP_MECHANICAL)
        {
            if (trapKnown)
                simple_monster_message(monster, " flies safely over a trap.");

            return;             // early return {dlb}
        }
    }

    // Trap damage to monsters is not a function of level, because they
    // are fairly stupid and tend to have fewer hp than players -- this
    // choice prevents traps from easily killing large monsters fairly
    // deep within the dungeon.
    switch (env.trap[which_trap].type)
    {
    case TRAP_DART:
        projectileFired = true;
        beem.name = " dart";
        beem.damage = dice_def( 1, 4 );
        beem.colour = OBJ_MISSILES;
        beem.type = MI_DART;
        break;
    case TRAP_NEEDLE:
        projectileFired = true;
        beem.name = " needle";
        beem.damage = dice_def( 1, 0 );
        beem.colour = OBJ_MISSILES;
        beem.type = MI_NEEDLE;
        break;
    case TRAP_ARROW:
        projectileFired = true;
        beem.name = "n arrow";
        beem.damage = dice_def( 1, 7 );
        beem.colour = OBJ_MISSILES;
        beem.type = MI_ARROW;
        break;
    case TRAP_SPEAR:
        projectileFired = true;
        beem.name = " spear";
        beem.damage = dice_def( 1, 10 );
        beem.colour = OBJ_WEAPONS;
        beem.type = WPN_SPEAR;
        break;
    case TRAP_BOLT:
        projectileFired = true;
        beem.name = " bolt";
        beem.damage = dice_def( 1, 13 );
        beem.colour = OBJ_MISSILES;
        beem.type = MI_BOLT;
        break;
    case TRAP_AXE:
        projectileFired = true;
        beem.name = "n axe";
        beem.damage = dice_def( 1, 15 );
        beem.colour = OBJ_WEAPONS;
        beem.type = WPN_HAND_AXE;
        break;
    // teleport traps are *never* revealed through
    // the triggering action of a monster, as any
    // number of factors could have been in play: {dlb}
    case TRAP_TELEPORT:
        monster_teleport(monster, true);
        break;
    // Alarm traps aren't set off by hostile monsters, because that would
    // be way too nasty for the player.
    case TRAP_ALARM:
        if (!mons_friendly(monster) || silenced(monster->x, monster->y))
        {
            if (trapKnown && you.can_see(monster)
                && !silenced(you.x_pos, you.y_pos))
            {
                mpr("The alarm trap makes no noise.");
            }
            return;
        }

        noisy(12, monster->x, monster->y);

        if (!silenced(you.x_pos, you.y_pos))
        {
            if (monsterNearby)
            {
                mpr("You hear a blaring wail!", MSGCH_SOUND);
                if (you.can_see(monster))
                    revealTrap = true;
            }
            else
                mpr("You hear a distant blaring wail!", MSGCH_SOUND);
        }

        break;
    // blade traps sometimes fail to trigger altogether,
    // resulting in an "early return" from this f(x) for
    // some - otherwise, blade *always* revealed: {dlb}
    case TRAP_BLADE:
        if (one_chance_in(5)
            || trapKnown && intelligent_ally(monster) && coinflip())
        {
            if (trapKnown)
            {
                simple_monster_message(monster,
                                        " fails to trigger a blade trap.");
            }
            return;             // early return {dlb}
        }

        if (random2(monster->ev) > 8
            || trapKnown && intelligent_ally(monster)
               && random2(monster->ev) > 8)
        {
            if (monsterNearby && !simple_monster_message(monster,
                                           " avoids a huge, swinging blade."))
            {
                mpr("A huge blade swings out!");
            }

            damage_taken = -1;  // just to be certain {dlb}
        }
        else
        {
            if (monsterNearby)
            {
                std::string msg = "A huge blade swings out";
                if (player_monster_visible( monster ))
                {
                    msg += " and slices into ";
                    msg += monster->name(DESC_NOCAP_THE);
                }
                msg += "!";
                mpr(msg.c_str());
            }

            damage_taken = 10 + random2avg(29, 2);
            damage_taken -= random2(1 + monster->ac);

            if (damage_taken < 0)
                damage_taken = 0;

            if (!mons_is_summoned(monster))
            {
                bleed_onto_floor(monster->x, monster->y, monster->type,
                                 damage_taken, true);
            }
        }

        revealTrap = true;
        break;

    case TRAP_NET:
    {
        if (one_chance_in(3)
            || trapKnown && intelligent_ally(monster) && coinflip())
        {
            if (trapKnown)
            {
                simple_monster_message(monster,
                                        " fails to trigger a net trap.");
            }
            return;
        }

        if (random2(monster->ev) > 8
            || trapKnown && intelligent_ally(monster)
               && random2(monster->ev) > 8)
        {
            if (monsterNearby && !simple_monster_message(monster,
                                 " nimbly jumps out of the way of a falling net."))
            {
                mpr("A large net falls down!");
            }
        }
        else
        {
            if (monsterNearby)
            {
                std::string msg = "A large net falls down";
                if (player_monster_visible( monster ))
                {
                    msg += " onto ";
                    msg += monster->name(DESC_NOCAP_THE);
                }
                msg += "!";
                mpr(msg.c_str());
                monster_caught_in_net(monster, beem);
            }
        }
        trap_item( OBJ_MISSILES, MI_THROWING_NET,
                   env.trap[which_trap].x, env.trap[which_trap].y );

        if (mons_is_caught(monster))
            mark_net_trapping(monster->x, monster->y);

        grd[env.trap[which_trap].x][env.trap[which_trap].y] = DNGN_FLOOR;
        env.trap[which_trap].type = TRAP_UNASSIGNED;
        break;
    }
    // zot traps are out to get *the player*! Hostile monsters
    // benefit and friendly monsters suffer - such is life - on
    // rare occasion, the trap affects nearby players, triggering
    // an "early return" - zot traps are *never* revealed - instead,
    // enchantment messages serve as clues to the trap's presence: {dlb}
    case TRAP_ZOT:
        if (monsterNearby)
        {
            if (one_chance_in(5))
            {
                mpr("The power of Zot is invoked against you!");
                miscast_effect( SPTYP_RANDOM, 10 + random2(30),
                                75 + random2(100), 0, "the power of Zot" );

                return;         // early return {dlb}
            }
        }

        // output triggering message to player, where appropriate: {dlb}
        if (!silenced(monster->x, monster->y)
                && !silenced(you.x_pos, you.y_pos))
        {
            if (monsterNearby)
                mpr("You hear a loud \"Zot\"!", MSGCH_SOUND);
            else
                mpr("You hear a distant \"Zot\"!", MSGCH_SOUND);
        }

        // Determine trap effects upon monster, based upon
        // whether it is naughty or nice to the player. {dlb}

        // NB: beem[].colour values are mislabeled as colours (?) -
        //     cf. mons_ench_f2() [which are also mislabeled] {dlb}
        temp_rand = random2(16);

        beem.thrower = KILL_MON;        // probably unnecessary
        beem.aux_source.clear();

        if (mons_friendly(monster))
        {
            beem.colour = ((temp_rand < 3) ? CYAN :  //paralyze - 3 in 16
                           (temp_rand < 7) ? RED     // confuse - 4 in 16
                                           : BLACK); //    slow - 9 in 16
        }
        else
        {
            beem.colour = ((temp_rand < 3) ? BLUE :  //haste - 3 in 16 {dlb}
                           (temp_rand < 7) ? MAGENTA //invis - 4 in 16 {dlb}
                                           : GREEN); // heal - 9 in 16 {dlb}
        }

        mons_ench_f2(monster, beem);
        damage_taken = 0;      // just to be certain {dlb}
        break;

    case TRAP_SHAFT:
        // Paranoia
        if (!is_valid_shaft_level())
        {
            if (trapKnown && monsterNearby)
                mpr("The shaft disappears in a puff of logic!");

            grd[env.trap[which_trap].x][env.trap[which_trap].y] = DNGN_FLOOR;
            env.trap[which_trap].type = TRAP_UNASSIGNED;
            return;
        }

        if (!monster->will_trigger_shaft()
            || trapKnown && intelligent_ally(monster))
        {
            if (trapKnown && !monster->airborne())
                simple_monster_message(monster,
                                        " doesn't fall through the shaft.");
            return;
        }

        if (monster->do_shaft())
            revealTrap = true;
        break;

    default:
        break;
    }


    // go back and handle projectile traps: {dlb}
    bool apply_poison = false;

    if (projectileFired)
    {
        // projectile traps *always* revealed after "firing": {dlb}
        revealTrap = true;

        // determine whether projectile hits, calculate damage: {dlb}
        if (((20 + (you.your_level * 2)) * random2(200)) / 100
                                                    >= monster->ev)
        {
            damage_taken = roll_dice( beem.damage );
            damage_taken -= random2(1 + monster->ac);

            if (damage_taken < 0)
                damage_taken = 0;

            if (beem.colour == OBJ_MISSILES
                && beem.type == MI_NEEDLE
                && x_chance_in_y(50 - (3 * monster->ac)/ 2, 100))
            {
                apply_poison = true;
            }
        }
        else
        {
            damage_taken = -1;  // negative damage marks a miss
        }

        if (monsterNearby)
        {
            mprf("A%s %s %s%s!",
                      beem.name.c_str(),
                      (damage_taken >= 0) ? "hits" : "misses",
                      monster->name(DESC_NOCAP_THE).c_str(),
                      (damage_taken == 0) ? ", but does no damage" : "");
        }

        if (apply_poison)
            poison_monster( monster, KC_OTHER );

        // Generate "fallen" projectile, where appropriate. {dlb}
        if (x_chance_in_y(7, 10))
        {
            beem.target_x = monster->x;
            beem.target_y = monster->y;
            itrap(beem, which_trap);
        }
    }


    // reveal undiscovered traps, where appropriate: {dlb}
    if (monsterNearby && !trapKnown && revealTrap)
    {
        grd[env.trap[which_trap].x][env.trap[which_trap].y]
                     = trap_category(env.trap[which_trap].type);
    }

    // apply damage and handle death, where appropriate: {dlb}
    if (damage_taken > 0)
    {
        hurt_monster(monster, damage_taken);

        if (monster->hit_points < 1)
        {
            monster_die(monster, KILL_MISC, 0);
            monster->speed_increment = 1;
        }
    }
}

static bool _mons_abjured(monsters *monster, bool nearby)
{
    if (nearby && _monster_abjuration(monster, false) > 0
        && coinflip())
    {
        _monster_abjuration(monster, true);
        return (true);
    }

    return (false);
}

static monster_type _pick_random_wraith()
{
    static monster_type wraiths[] =
    {
        MONS_WRAITH, MONS_SHADOW_WRAITH, MONS_FREEZING_WRAITH,
        MONS_SPECTRAL_WARRIOR
    };

    return wraiths[random2(sizeof(wraiths) / sizeof(*wraiths))];
}

static monster_type _pick_horrible_thing()
{
    return (one_chance_in(4) ? MONS_TENTACLED_MONSTROSITY
                             : MONS_ABOMINATION_LARGE);
}

static monster_type _pick_undead_summon()
{
    static monster_type undead[] =
    {
        MONS_NECROPHAGE, MONS_GHOUL, MONS_HUNGRY_GHOST, MONS_FLAYED_GHOST,
        MONS_ZOMBIE_SMALL, MONS_SKELETON_SMALL, MONS_SIMULACRUM_SMALL,
        MONS_FLYING_SKULL, MONS_MUMMY, MONS_VAMPIRE, MONS_WIGHT, MONS_WRAITH,
        MONS_SHADOW_WRAITH, MONS_FREEZING_WRAITH, MONS_SPECTRAL_WARRIOR,
        MONS_ZOMBIE_LARGE, MONS_SKELETON_LARGE, MONS_SIMULACRUM_LARGE,
        MONS_SHADOW
    };

    return undead[random2(sizeof(undead) / sizeof(*undead))];
}

static void _do_high_level_summon(monsters *monster, bool monsterNearby,
                                  monster_type (*mpicker)(),
                                  int nsummons)
{
    if (_mons_abjured(monster, monsterNearby))
        return;

    const int duration = std::min(2 + monster->hit_dice / 5, 6);

    for (int i = 0; i < nsummons; ++i)
    {
        monster_type which_mons = mpicker();

        if (which_mons == MONS_PROGRAM_BUG)
            continue;

        create_monster(
            mgen_data(which_mons, SAME_ATTITUDE(monster), duration,
                      monster->pos(), monster->foe));
    }
}

void mons_cast(monsters *monster, bolt &pbolt, spell_type spell_cast)
{
    // Always do setup.  It might be done already, but it doesn't
    // hurt to do it again (cheap).
    setup_mons_cast(monster, pbolt, spell_cast);

    // single calculation permissible {dlb}
    bool monsterNearby = mons_near(monster);

    int sumcount = 0;
    int sumcount2;
    int duration = 0;

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Mon #%d casts %s (#%d)",
         monster_index(monster), spell_title(spell_cast), spell_cast);
#endif

    if (spell_cast == SPELL_HELLFIRE_BURST || spell_cast == SPELL_BRAIN_FEED
        || spell_cast == SPELL_SMITING)
    {
        if (monster->foe == MHITYOU || monster->foe == MHITNOT)
        {
            if (monsterNearby)
                direct_effect(pbolt);
            return;
        }

        mons_direct_effect(pbolt, monster_index(monster));
        return;
    }

    switch (spell_cast)
    {
    default:
        break;

    case SPELL_GREATER_HEALING:
        if (heal_monster(monster, 50 + random2avg(monster->hit_dice * 10, 2),
                         false))
        {
            simple_monster_message(monster, " is healed.");
        }
        return;

    case SPELL_BERSERKER_RAGE:
        monster->go_berserk(true);
        return;

    case SPELL_SUMMON_SMALL_MAMMALS:
    case SPELL_VAMPIRE_SUMMON:
        if (spell_cast == SPELL_SUMMON_SMALL_MAMMALS)
            sumcount2 = 1 + random2(4);
        else
            sumcount2 = 3 + random2(3) + monster->hit_dice / 5;

        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            monster_type mon = MONS_GIANT_BAT;

            if (!one_chance_in(3))
            {
                int temp_rand = random2(4);

                mon = (temp_rand == 0) ? MONS_ORANGE_RAT :
                      (temp_rand == 1) ? MONS_GREEN_RAT :
                      (temp_rand == 2) ? MONS_GREY_RAT
                                       : MONS_RAT;

            }

            create_monster(
                mgen_data(mon, SAME_ATTITUDE(monster),
                          5, monster->pos(), monster->foe));
        }
        return;

    case SPELL_SHADOW_CREATURES:       // summon anything appropriate for level
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(4) + random2(monster->hit_dice / 7 + 1);

        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(RANDOM_MONSTER, SAME_ATTITUDE(monster), 5,
                          monster->pos(), monster->foe));
        }
        return;

    case SPELL_FAKE_RAKSHASA_SUMMON:
        sumcount2 = (coinflip() ? 2 : 3);

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster(
                mgen_data(MONS_RAKSHASA_FAKE, SAME_ATTITUDE(monster), 3,
                          monster->pos(), monster->foe));
        }
        return;

    case SPELL_SUMMON_DEMON: // class 2-4 demons
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(2) + random2(monster->hit_dice / 10 + 1);

        duration  = std::min(2 + monster->hit_dice / 10, 6);
        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster(
                mgen_data(summon_any_demon(DEMON_COMMON),
                          SAME_ATTITUDE(monster), duration,
                          monster->pos(), monster->foe));
        }
        return;

    case SPELL_SUMMON_UGLY_THING:
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(2) + random2(monster->hit_dice / 10 + 1);

        duration  = std::min(2 + monster->hit_dice / 10, 6);
        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            const int chance = std::max(6 - (monster->hit_dice / 6), 1);
            monster_type mon = (one_chance_in(chance) ? MONS_VERY_UGLY_THING
                                                      : MONS_UGLY_THING);

            create_monster(
                mgen_data(mon, SAME_ATTITUDE(monster), duration,
                          monster->pos(), monster->foe));
        }
        return;

    case SPELL_ANIMATE_DEAD:
        // see special handling in monstuff::handle_spell() {dlb}
        animate_dead(monster, 5 + random2(5), SAME_ATTITUDE(monster),
                     monster->foe);
        return;

    case SPELL_CALL_IMP: // class 5 demons
        sumcount2 = 1 + random2(3) + random2(monster->hit_dice / 5 + 1);

        duration  = std::min(2 + monster->hit_dice / 5, 6);
        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(summon_any_demon(DEMON_LESSER),
                          SAME_ATTITUDE(monster), duration,
                          monster->pos(), monster->foe));
        }
        return;

    case SPELL_SUMMON_SCORPIONS:
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(3) + random2( monster->hit_dice / 5 + 1);

        duration  = std::min(2 + monster->hit_dice / 5, 6);
        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(MONS_SCORPION, SAME_ATTITUDE(monster), duration,
                          monster->pos(), monster->foe));
        }
        return;

    case SPELL_SUMMON_UFETUBUS:
        sumcount2 = 2 + random2(2) + random2(monster->hit_dice / 5 + 1);

        duration  = std::min(2 + monster->hit_dice / 5, 6);

        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(MONS_UFETUBUS, SAME_ATTITUDE(monster), duration,
                          monster->pos(), monster->foe));
        }
        return;

    case SPELL_SUMMON_BEAST:       // Geryon
        create_monster(
            mgen_data(MONS_BEAST, SAME_ATTITUDE(monster), 4,
                      monster->pos(), monster->foe));
        return;

    case SPELL_SUMMON_ICE_BEAST:
        create_monster(
            mgen_data(MONS_ICE_BEAST, SAME_ATTITUDE(monster), 5,
                      monster->pos(), monster->foe));
        return;

    case SPELL_SUMMON_MUSHROOMS:   // Summon swarms of icky crawling fungi.
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(2) + random2(monster->hit_dice / 4 + 1);

        duration  = std::min(2 + monster->hit_dice / 5, 6);
        for (int i = 0; i < sumcount2; ++i)
        {
            create_monster(
                mgen_data(MONS_WANDERING_MUSHROOM, SAME_ATTITUDE(monster),
                          duration, monster->pos(), monster->foe));
        }
        return;

    case SPELL_SUMMON_WRAITHS:
        _do_high_level_summon(monster, monsterNearby, _pick_random_wraith,
                              random_range(3, 6));
        return;

    case SPELL_SUMMON_HORRIBLE_THINGS:
        _do_high_level_summon(monster, monsterNearby, _pick_horrible_thing,
                              random_range(3, 5));
        return;

    case SPELL_SUMMON_UNDEAD:      // Summon undead around player.
        _do_high_level_summon(monster, monsterNearby, _pick_undead_summon,
                              2 + random2(2)
                                + random2(monster->hit_dice / 4 + 1));
        return;

    case SPELL_SYMBOL_OF_TORMENT:
        if (!monsterNearby || mons_friendly(monster))
            return;

        simple_monster_message(monster, " calls on the powers of Hell!");

        torment(monster_index(monster), monster->x, monster->y);
        return;

    case SPELL_SUMMON_GREATER_DEMON:
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(monster->hit_dice / 10 + 1);

        duration  = std::min(2 + monster->hit_dice / 10, 6);
        for (sumcount = 0; sumcount < sumcount2; ++sumcount)
        {
            create_monster(
                mgen_data(summon_any_demon(DEMON_GREATER),
                          SAME_ATTITUDE(monster), duration,
                          monster->pos(), monster->foe));
        }
        return;

    // Journey -- Added in Summon Lizards and Draconian
    case SPELL_SUMMON_DRAKES:
        if (_mons_abjured(monster, monsterNearby))
            return;

        sumcount2 = 1 + random2(3) + random2(monster->hit_dice / 5 + 1);

        duration  = std::min(2 + monster->hit_dice / 10, 6);

        {
            std::vector<monster_type> monsters;

            for (sumcount = 0; sumcount < sumcount2; sumcount++)
            {
                monster_type mon = summon_any_dragon(DRAGON_LIZARD);

                if (mon == MONS_DRAGON)
                {
                    monsters.clear();
                    monsters.push_back(summon_any_dragon(DRAGON_DRAGON));
                    break;
                }

                monsters.push_back(mon);
            }

            for (int i = 0, size = monsters.size(); i < size; ++i)
            {
                create_monster(
                    mgen_data(monsters[i], SAME_ATTITUDE(monster), duration,
                              monster->pos(), monster->foe));
            }
        }
        return;

    case SPELL_CANTRIP:
    {
        const bool friendly      = mons_friendly(monster);
        const bool buff_only     = !friendly && is_sanctuary(you.x_pos,
                                                             you.y_pos);
        bool need_friendly_stub  = false;
        const msg_channel_type channel = (friendly) ? MSGCH_FRIEND_ENCHANT
                                                    : MSGCH_MONSTER_ENCHANT;

        // Monster spell of uselessness, just prints a message.
        // This spell exists so that some monsters with really strong
        // spells (ie orc priest) can be toned down a bit. -- bwr
        //
        // XXX: Needs expansion, and perhaps different priest/mage flavours.
        switch (random2(buff_only ? 4 : 7))
        {
        case 0:
            simple_monster_message(monster, " glows brightly for a moment.",
                                   channel);
            break;
        case 1:
            simple_monster_message(monster, " looks stronger.",
                                   channel);
            break;
        case 2:
            simple_monster_message(monster, " becomes somewhat translucent.",
                                   channel);
            break;
        case 3:
            simple_monster_message(monster, "'s eyes start to glow.",
                                   channel);
            break;
        case 4:
            if (friendly)
                need_friendly_stub = true;
            else
                mpr("You feel troubled.");
            break;
        case 5:
            if (friendly)
                need_friendly_stub = true;
            else
                mpr("You feel a wave of unholy energy pass over you.");
            break;
        case 6:
        default:
            if (friendly)
                need_friendly_stub = true;
            else if (one_chance_in(20))
                mpr("You resist (whatever that was supposed to do).");
            else
                mpr("You resist.");
            break;
        }

        if (need_friendly_stub)
        {
            simple_monster_message(monster, " shimmers for a moment.",
                                   channel);
        }

        return;
    }
    }

    fire_beam(pbolt);
}

// Set up bolt structure for monster spell casting.
void setup_mons_cast(const monsters *monster, bolt &pbolt, int spell_cast)
{
    // always set these -- used by things other than fire_beam()

    // [ds] Used to be 12 * MHD and later buggily forced to -1 downstairs.
    // Setting this to a more realistic number now that that bug is
    // squashed.
    pbolt.ench_power = 4 * monster->hit_dice;

    if (spell_cast == SPELL_TELEPORT_SELF)
        pbolt.ench_power = 2000;
    else if (spell_cast == SPELL_PAIN) // this is cast by low HD monsters
        pbolt.ench_power *= 2;

    pbolt.beam_source = monster_index(monster);

    // set bolt type
    if (spell_cast == SPELL_HELLFIRE_BURST
        || spell_cast == SPELL_BRAIN_FEED
        || spell_cast == SPELL_SMITING)
    {                           // etc.
        switch (spell_cast)
        {
        case SPELL_HELLFIRE_BURST:
            pbolt.type = DMNBM_HELLFIRE;
            break;
        case SPELL_BRAIN_FEED:
            pbolt.type = DMNBM_BRAIN_FEED;
            break;
        case SPELL_SMITING:
            pbolt.type = DMNBM_SMITING;
            break;
        }
        return;
    }

    // The below are no-ops since they don't involve direct_effect,
    // fire_tracer, or beam.
    switch (spell_cast)
    {
    case SPELL_SUMMON_SMALL_MAMMALS:
    case SPELL_GREATER_HEALING:
    case SPELL_VAMPIRE_SUMMON:
    case SPELL_SHADOW_CREATURES:       // summon anything appropriate for level
    case SPELL_FAKE_RAKSHASA_SUMMON:
    case SPELL_SUMMON_DEMON:
    case SPELL_SUMMON_UGLY_THING:
    case SPELL_ANIMATE_DEAD:
    case SPELL_CALL_IMP:
    case SPELL_SUMMON_SCORPIONS:
    case SPELL_SUMMON_UFETUBUS:
    case SPELL_SUMMON_BEAST:       // Geryon
    case SPELL_SUMMON_UNDEAD:      // summon undead around player
    case SPELL_SUMMON_ICE_BEAST:
    case SPELL_SUMMON_MUSHROOMS:
    case SPELL_SUMMON_DRAKES:
    case SPELL_SUMMON_HORRIBLE_THINGS:
    case SPELL_SUMMON_WRAITHS:
    case SPELL_SYMBOL_OF_TORMENT:
    case SPELL_SUMMON_GREATER_DEMON:
    case SPELL_CANTRIP:
    case SPELL_BERSERKER_RAGE:
        return;
    default:
        break;
    }

    // Need to correct this for power of spellcaster
    int power = 12 * monster->hit_dice;

    bolt theBeam         = mons_spells(spell_cast, power);

    pbolt.colour         = theBeam.colour;
    pbolt.range          = theBeam.range;
    pbolt.rangeMax       = theBeam.rangeMax;
    pbolt.hit            = theBeam.hit;
    pbolt.damage         = theBeam.damage;

    if (theBeam.ench_power != -1)
        pbolt.ench_power = theBeam.ench_power;

    pbolt.type           = theBeam.type;
    pbolt.flavour        = theBeam.flavour;
    pbolt.thrower        = theBeam.thrower;
    pbolt.aux_source.clear();
    pbolt.name           = theBeam.name;
    pbolt.is_beam        = theBeam.is_beam;
    pbolt.source_x       = monster->x;
    pbolt.source_y       = monster->y;
    pbolt.is_tracer      = false;
    pbolt.is_explosion   = theBeam.is_explosion;

    if (pbolt.name.length() && pbolt.name[0] != '0')
        pbolt.aux_source = pbolt.name;
    else
        pbolt.aux_source.clear();

    if (spell_cast == SPELL_HASTE
        || spell_cast == SPELL_INVISIBILITY
        || spell_cast == SPELL_LESSER_HEALING
        || spell_cast == SPELL_TELEPORT_SELF)
    {
        pbolt.target_x = monster->x;
        pbolt.target_y = monster->y;
    }

    // Convenience for the hapless innocent who assumes that this
    // damn function does all possible setup. [ds]
    if (!pbolt.target_x && !pbolt.target_y)
    {
        pbolt.target_x = monster->target_x;
        pbolt.target_y = monster->target_y;
    }
}

void monster_teleport(monsters *monster, bool instan, bool silent)
{
    if (!instan)
    {
        if (monster->del_ench(ENCH_TP))
        {
            if (!silent)
                simple_monster_message(monster, " seems more stable.");
        }
        else
        {
            if (!silent)
                simple_monster_message(monster, " looks slightly unstable.");

            monster->add_ench( mon_enchant(ENCH_TP, 0, KC_OTHER,
                                           random_range(20, 30)) );
        }

        return;
    }

    bool was_seen = player_monster_visible(monster) && mons_near(monster)
                    && !mons_is_lurking(monster);

    if (!silent)
        simple_monster_message(monster, " disappears!");

    const coord_def oldplace = monster->pos();

    // Pick the monster up.
    mgrd(oldplace) = NON_MONSTER;

    mons_clear_trapping_net(monster);

    int newx, newy;
    while (true)
    {
        newx = 10 + random2(GXM - 20);
        newy = 10 + random2(GYM - 20);

        // Don't land on top of another monster.
        if (mgrd[newx][newy] != NON_MONSTER
            || newx == you.x_pos && newy == you.y_pos)
        {
            continue;
        }

        if (is_sanctuary(newx, newy) && !mons_wont_attack(monster))
            continue;

        if (monster_habitable_grid(monster, grd[newx][newy]))
            break;
    }

    monster->x = newx;
    monster->y = newy;

    mgrd[monster->x][monster->y] = monster_index(monster);

    // Mimics change form/colour when teleported.
    if (mons_is_mimic( monster->type ))
    {
        int old_type    = monster->type;
        monster->type   = MONS_GOLD_MIMIC + random2(5);
        monster->colour = get_mimic_colour( monster );

        // If it's changed form, you won't recognize it.
        // This assumes that a non-gold mimic turning into another item of
        // the same description is really, really unlikely.
        if (old_type != MONS_GOLD_MIMIC || monster->type != MONS_GOLD_MIMIC)
            was_seen = false;
    }

    const bool now_visible = mons_near(monster);
    if (!silent && now_visible)
    {
        if (was_seen)
            simple_monster_message(monster, " reappears nearby!");
        else
            simple_monster_message(monster, " appears out of thin air!",
                                   MSGCH_PLAIN, 0, DESC_CAP_A);
    }

    if (player_monster_visible(monster) && now_visible)
        seen_monster(monster);

    monster->check_redraw(oldplace);
    monster->apply_location_effects();

    // Teleporting mimics change form - if they reappear out of LOS, they are
    // no longer known.
    if (mons_is_mimic(monster->type))
    {
        if (now_visible)
            monster->flags |= MF_KNOWN_MIMIC;
        else
            monster->flags &= ~MF_KNOWN_MIMIC;
    }
}                               // end monster_teleport()

void setup_dragon(struct monsters *monster, struct bolt &pbolt)
{
    const int type = (mons_genus( monster->type ) == MONS_DRACONIAN)
                            ? draco_subspecies( monster ) : monster->type;
    int scaling = 100;

    pbolt.name.clear();
    switch (type)
    {
    case MONS_FIREDRAKE:
    case MONS_HELL_HOUND:
    case MONS_DRAGON:
    case MONS_LINDWURM:
    case MONS_XTAHUA:
        pbolt.name      += "blast of flame";
        pbolt.aux_source = "blast of fiery breath";
        pbolt.flavour    = BEAM_FIRE;
        pbolt.colour     = RED;
        break;

    case MONS_ICE_DRAGON:
        pbolt.name      += "blast of cold";
        pbolt.aux_source = "blast of icy breath";
        pbolt.flavour    = BEAM_COLD;
        pbolt.colour     = WHITE;
        break;

    case MONS_RED_DRACONIAN:
        pbolt.name      += "searing blast";
        pbolt.aux_source = "blast of searing breath";
        pbolt.flavour    = BEAM_FIRE;
        pbolt.colour     = RED;
        scaling          = 65;
        break;

    case MONS_WHITE_DRACONIAN:
        pbolt.name      += "chilling blast";
        pbolt.aux_source = "blast of chilling breath";
        pbolt.flavour    = BEAM_COLD;
        pbolt.colour     = WHITE;
        scaling = 65;
        break;

    case MONS_PLAYER_GHOST: // draconians only
        pbolt.name      += "blast of negative energy";
        pbolt.aux_source = "blast of draining breath";
        pbolt.flavour    = BEAM_NEG;
        pbolt.colour     = DARKGREY;
        scaling = 65;
        break;

    default:
        DEBUGSTR("Bad monster class in setup_dragon()");
        break;
    }

#ifdef DEBUG_DIAGNOSTICS
    mprf( MSGCH_DIAGNOSTICS, "bolt name: '%s'", pbolt.name.c_str() );
#endif

    pbolt.range       = 4;
    pbolt.rangeMax    = 13;
    pbolt.damage      = dice_def( 3, (monster->hit_dice * 2) );
    pbolt.damage.size = scaling * pbolt.damage.size / 100;
    pbolt.type        = dchar_glyph(DCHAR_FIRED_ZAP);
    pbolt.hit         = 30;
    pbolt.beam_source = monster_index(monster);
    pbolt.thrower     = KILL_MON;
    pbolt.is_beam     = true;

    // Accuracy is lowered by one quarter if the dragon is attacking
    // a target that is wielding a weapon of dragon slaying (which
    // makes the dragon/draconian avoid looking at the foe).
    // FIXME: This effect is not yet implemented for player draconians
    // or characters in dragon form breathing at monsters wielding a
    // weapon with this brand.
    if (mons_genus(monster->type) == MONS_DRAGON
        || mons_genus(monster->type) == MONS_DRACONIAN)
    {
        if (actor *foe = monster->get_foe())
        {
            if (const item_def *weapon = foe->weapon())
            {
                if (get_weapon_brand(*weapon) == SPWPN_DRAGON_SLAYING)
                {
                    pbolt.hit *= 3;
                    pbolt.hit /= 4;
                }
            }
        }
    }
}

void setup_generic_throw(struct monsters *monster, struct bolt &pbolt)
{
    pbolt.range = 9;
    pbolt.rangeMax = 9;
    pbolt.beam_source = monster_index(monster);

    pbolt.type    = dchar_glyph(DCHAR_FIRED_MISSILE);
    pbolt.flavour = BEAM_MISSILE;
    pbolt.thrower = KILL_MON_MISSILE;
    pbolt.aux_source.clear();
    pbolt.is_beam = false;
}

bool mons_throw(struct monsters *monster, struct bolt &pbolt, int hand_used)
{
    // XXX: Ugly hack, but avoids adding dynamic allocation to this code.
    char throw_buff[ ITEMNAME_SIZE ];

    bool returning = (get_weapon_brand(mitm[hand_used]) == SPWPN_RETURNING
                      || get_ammo_brand(mitm[hand_used]) == SPMSL_RETURNING);

    int baseHit = 0, baseDam = 0;       // from thrown or ammo
    int ammoHitBonus = 0, ammoDamBonus = 0;     // from thrown or ammo
    int lnchHitBonus = 0, lnchDamBonus = 0;     // special add from launcher
    int exHitBonus   = 0, exDamBonus = 0; // 'extra' bonus from skill/dex/str
    int lnchBaseDam  = 0;

    int hitMult  = 0;
    int damMult  = 0;
    int diceMult = 100;

    // Some initial convenience & initializations.
    int wepClass  = mitm[hand_used].base_type;
    int wepType   = mitm[hand_used].sub_type;

    int weapon    = monster->inv[MSLOT_WEAPON];
    int lnchType  = (weapon != NON_ITEM) ? mitm[weapon].sub_type  :  0;

    const bool skilled = mons_class_flag(monster->type, M_FIGHTER);

    monster->lose_energy(EUT_MISSILE);
    const int throw_energy = monster->action_energy(EUT_MISSILE);

    // Dropping item copy, since the launched item might be different.
    item_def item = mitm[hand_used];
    item.quantity = 1;
    if (mons_friendly(monster))
        item.flags |= ISFLAG_DROPPED_BY_ALLY;

    pbolt.range       = 9;
    pbolt.beam_source = monster_index(monster);
    pbolt.type        = dchar_glyph(DCHAR_FIRED_MISSILE);
    pbolt.colour      = item.colour;
    pbolt.flavour     = BEAM_MISSILE;
    pbolt.thrower     = KILL_MON_MISSILE;
    pbolt.aux_source.clear();

    const launch_retval projected =
        is_launched(monster, monster->mslot_item(MSLOT_WEAPON),
                    mitm[hand_used]);

    // extract launcher bonuses due to magic
    if (projected == LRET_LAUNCHED)
    {
        lnchHitBonus = mitm[ weapon ].plus;
        lnchDamBonus = mitm[ weapon ].plus2;
        lnchBaseDam  = property(mitm[weapon], PWPN_DAMAGE);
    }

    // extract weapon/ammo bonuses due to magic
    ammoHitBonus = item.plus;
    ammoDamBonus = item.plus2;

    // Archers get a boost from their melee attack.
    if (mons_class_flag(monster->type, M_ARCHER))
    {
        const mon_attack_def attk = mons_attack_spec(monster, 0);
        if (attk.type == AT_SHOOT)
            ammoDamBonus += random2avg(attk.damage, 2);
    }

    if (projected == LRET_THROWN)
    {
        // Darts are easy.
        if (wepClass == OBJ_MISSILES && wepType == MI_DART)
        {
            baseHit = 11;
            hitMult = 40;
            damMult = 25;
        }
        else
        {
            baseHit = 6;
            hitMult = 30;
            damMult = 25;
        }

        baseDam = property( item, PWPN_DAMAGE );

        if (wepClass == OBJ_MISSILES)   // throw missile
        {
            // ammo damage needs adjusting here - OBJ_MISSILES
            // don't get separate tohit/damage bonuses!
            ammoDamBonus = ammoHitBonus;

            // [dshaligram] Thrown stones/darts do only half the damage of
            // launched stones/darts. This matches 4.0 behaviour.
            if (wepType == MI_DART || wepType == MI_STONE
                || wepType == MI_SLING_BULLET)
            {
                baseDam = div_rand_round(baseDam, 2);
            }
        }

        // give monster "skill" bonuses based on HD
        exHitBonus = (hitMult * monster->hit_dice) / 10 + 1;
        exDamBonus = (damMult * monster->hit_dice) / 10 + 1;
    }

    // Monsters no longer gain unfair advantages with weapons of fire/ice
    // and incorrect ammo.  They now have same restriction as players.

    int bow_brand = SPWPN_NORMAL;
    const int ammo_brand = get_ammo_brand( item );
    const bool poison = (ammo_brand == SPMSL_POISONED);

    if (projected == LRET_LAUNCHED)
    {
        bow_brand = get_weapon_brand(mitm[monster->inv[MSLOT_WEAPON]]);

        switch (lnchType)
        {
        case WPN_BLOWGUN:
            baseHit = 12;
            hitMult = 60;
            damMult = 0;
            lnchDamBonus = 0;
            break;
        case WPN_BOW:
        case WPN_LONGBOW:
            baseHit = 0;
            hitMult = 60;
            damMult = 35;
            // monsters get half the launcher damage bonus,
            // which is about as fair as I can figure it.
            lnchDamBonus = (lnchDamBonus + 1) / 2;
            break;
        case WPN_CROSSBOW:
            baseHit = 4;
            hitMult = 70;
            damMult = 30;
            break;
        case WPN_HAND_CROSSBOW:
            baseHit = 2;
            hitMult = 50;
            damMult = 20;
            break;
        case WPN_SLING:
            baseHit = 10;
            hitMult = 40;
            damMult = 20;
            // monsters get half the launcher damage bonus,
            // which is about as fair as I can figure it.
            lnchDamBonus /= 2;
            break;
        }

        // Launcher is now more important than ammo for base damage.
        baseDam = property(item, PWPN_DAMAGE);
        if (lnchBaseDam)
            baseDam = lnchBaseDam + random2(1 + baseDam);

        // missiles don't have pluses2;  use hit bonus
        ammoDamBonus = ammoHitBonus;

        exHitBonus = (hitMult * monster->hit_dice) / 10 + 1;
        exDamBonus = (damMult * monster->hit_dice) / 10 + 1;

        if (!baseDam && elemental_missile_beam(bow_brand, ammo_brand))
            baseDam = 4;

        // [dshaligram] This is a horrible hack - we force beam.cc to consider
        // this beam "needle-like".
        if (wepClass == OBJ_MISSILES && wepType == MI_NEEDLE)
            pbolt.ench_power = AUTOMATIC_HIT;

        // elven bow w/ elven arrow, also orcish
        if (get_equip_race( mitm[monster->inv[MSLOT_WEAPON]] )
                == get_equip_race( mitm[monster->inv[MSLOT_MISSILE]] ))
        {
            baseHit++;
            baseDam++;

            if (get_equip_race(mitm[monster->inv[MSLOT_WEAPON]]) == ISFLAG_ELVEN)
                pbolt.hit++;
        }

        // POISON brand launchers poison ammo
        if (bow_brand == SPWPN_VENOM && ammo_brand == SPMSL_NORMAL)
            set_item_ego_type( item, OBJ_MISSILES, SPMSL_POISONED );

        // Vorpal brand increases damage dice size.
        if (bow_brand == SPWPN_VORPAL)
            diceMult = diceMult * 130 / 100;

        // Note: we already have throw_energy taken off.  -- bwr
        int speed_delta = 0;
        if (lnchType == WPN_CROSSBOW)
        {
            if (bow_brand == SPWPN_SPEED)
            {
                // Speed crossbows take 50% less time to use than
                // ordinary crossbows.
                speed_delta = div_rand_round(throw_energy * 2, 5);
            }
            else
            {
                // Ordinary crossbows take 20% more time to use
                // than ordinary bows.
                speed_delta = -div_rand_round(throw_energy, 5);
            }
        }
        else if (bow_brand == SPWPN_SPEED)
        {
            // Speed bows take 50% less time to use than
            // ordinary bows.
            speed_delta = div_rand_round(throw_energy, 2);
        }

        monster->speed_increment += speed_delta;
    }

    // WEAPON or AMMO of FIRE
    if (bow_brand == SPWPN_FLAME && ammo_brand != SPMSL_ICE
        || ammo_brand == SPMSL_FLAME && bow_brand != SPWPN_FROST)
    {
        baseHit += 2;
        exDamBonus += 6;

        pbolt.flavour  = BEAM_FIRE;
        pbolt.name     = "bolt of ";

        if (poison)
            pbolt.name += "poison ";

        pbolt.name     += "flame";
        pbolt.colour    = RED;
        pbolt.type      = dchar_glyph(DCHAR_FIRED_ZAP);
    }
    // WEAPON or AMMO of FROST
    else if (bow_brand == SPWPN_FROST && ammo_brand != SPMSL_FLAME
             || ammo_brand == SPMSL_ICE && bow_brand != SPWPN_FLAME)
    {
        baseHit += 2;
        exDamBonus += 6;

        pbolt.flavour   = BEAM_COLD;
        pbolt.name      = "bolt of ";

        if (poison)
            pbolt.name += "poison ";

        pbolt.name     += "frost";
        pbolt.colour    = WHITE;
        pbolt.type      = dchar_glyph(DCHAR_FIRED_ZAP);
    }

    // monster intelligence bonus
    if (mons_intel(monster->type) == I_HIGH)
        exHitBonus += 10;

    // Now, if a monster is, for some reason, throwing something really
    // stupid, it will have baseHit of 0 and damage of 0.  Ah well.
    std::string msg = monster->name(DESC_CAP_THE);
    msg += ((projected == LRET_LAUNCHED) ? " shoots " : " throws ");

    if (!pbolt.name.empty() && projected == LRET_LAUNCHED)
    {
        msg += "a ";
        msg += pbolt.name;
    }
    else
    {
        // build shoot message
        msg += item.name(DESC_NOCAP_A);

        // build beam name
        pbolt.name = item.name(DESC_PLAIN, false, false, false);
    }
    msg += ".";

    if (monster->visible())
    {
        mpr(msg.c_str());

        if (projected == LRET_LAUNCHED
               && item_type_known(mitm[monster->inv[MSLOT_WEAPON]])
            || projected == LRET_THROWN
               && mitm[hand_used].base_type == OBJ_MISSILES)
        {
            set_ident_flags(mitm[hand_used], ISFLAG_KNOW_TYPE);
        }
    }

    // [dshaligram] When changing bolt names here, you must edit
    // hiscores.cc (scorefile_entry::terse_missile_cause()) to match.
    if (projected == LRET_LAUNCHED)
    {
        snprintf( throw_buff, sizeof(throw_buff), "Shot with a%s %s by %s",
                  (is_vowel(pbolt.name[0]) ? "n" : ""), pbolt.name.c_str(),
                  monster->name(DESC_NOCAP_A).c_str() );
    }
    else
    {
        snprintf( throw_buff, sizeof(throw_buff), "Hit by a%s %s thrown by %s",
                  (is_vowel(pbolt.name[0]) ? "n" : ""), pbolt.name.c_str(),
                  monster->name(DESC_NOCAP_A).c_str() );
    }

    pbolt.aux_source = throw_buff;

    // add everything up.
    pbolt.hit = baseHit + random2avg(exHitBonus, 2) + ammoHitBonus;
    pbolt.damage =
        dice_def( 1, baseDam + random2avg(exDamBonus, 2) + ammoDamBonus );

    if (projected == LRET_LAUNCHED)
    {
        pbolt.damage.size += lnchDamBonus;
        pbolt.hit += lnchHitBonus;
    }
    pbolt.damage.size = diceMult * pbolt.damage.size / 100;

    if (monster->has_ench(ENCH_BATTLE_FRENZY))
    {
        const mon_enchant ench = monster->get_ench(ENCH_BATTLE_FRENZY);

#ifdef DEBUG_DIAGNOSTICS
        const dice_def orig_damage = pbolt.damage;
#endif

        pbolt.damage.size = pbolt.damage.size * (115 + ench.degree * 15) / 100;

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "%s frenzy damage: %dd%d -> %dd%d",
             monster->name(DESC_PLAIN).c_str(),
             orig_damage.num, orig_damage.size,
             pbolt.damage.num, pbolt.damage.size);
#endif
    }

    // Skilled archers get better to-hit and damage.
    if (skilled)
    {
        pbolt.hit         = pbolt.hit * 120 / 100;
        pbolt.damage.size = pbolt.damage.size * 120 / 100;
    }

    scale_dice(pbolt.damage);

    // decrease inventory
    bool really_returns;
    if (returning && !one_chance_in(mons_power(monster->type) + 3))
        really_returns = true;
    else
        really_returns = false;

    fire_beam( pbolt, &item, !really_returns );

    // The item can be destroyed before returning.
    if (really_returns && mons_thrown_object_destroyed(&item, pbolt.target_x,
                                                       pbolt.target_y, true,
                                                       pbolt.beam_source))
    {
        really_returns = false;
    }

    if (really_returns)
    {
        // Fire beam in reverse.
        pbolt.setup_retrace();
        viewwindow(true, false);
        fire_beam(pbolt, &item, false);
        msg::stream << "The weapon returns "
                    << (player_monster_visible(monster)?
                          ("to " + monster->name(DESC_NOCAP_THE))
                        : "whence it came from")
                    << "!" << std::endl;

        // Player saw the item return.
        if (!is_artefact(item))
        {
            // Since this only happens for non-artefacts, also mark properties
            // as known.
            set_ident_flags(mitm[hand_used],
                            ISFLAG_KNOW_TYPE | ISFLAG_KNOW_PROPERTIES);
        }
    }
    else if (dec_mitm_item_quantity( hand_used, 1 ))
        monster->inv[returning ? hand_used : MSLOT_MISSILE] = NON_ITEM;

    return (true);
}                               // end mons_throw()

bool mons_thrown_object_destroyed( item_def *item, int x, int y,
                                   bool returning, int midx )
{
    ASSERT( item != NULL );

    bool destroyed = (item->base_type == OBJ_MISSILES
                      && item->sub_type != MI_THROWING_NET && coinflip());
    bool hostile_grid = grid_destroys_items(grd[x][y]);

    // Non-returning items thrown into item-destroying grids are always
    // destroyed.  Returning items are only destroyed if they would have
    // been randomly destroyed anyway.
    if (returning && !destroyed)
        hostile_grid = false;

    // Summoned items go poof if they're not returning.
    if (hostile_grid || !returning && (item->flags & ISFLAG_SUMMONED))
    {
        // No destruction sound here.  Too much message spam otherwise.
        item_was_destroyed(*item, midx);
        destroyed = true;
    }

    return destroyed;
}

// XXX: Should really do something about mons_hit, but can't be bothered.
void spore_goes_pop(monsters *monster)
{
    bolt beam;
    const int type = monster->type;

    if (monster == NULL)
        return;

    beam.is_tracer    = false;
    beam.is_explosion = true;
    beam.beam_source  = monster_index(monster);
    beam.type         = dchar_glyph(DCHAR_FIRED_BURST);
    beam.target_x     = monster->x;
    beam.target_y     = monster->y;
    beam.thrower      = KILL_MON;    // someone else's explosion
    beam.aux_source.clear();

    const char* msg       = NULL;
    const char* sanct_msg = NULL;
    if (type == MONS_GIANT_SPORE)
    {
        beam.flavour = BEAM_SPORE;
        beam.name    = "explosion of spores";
        beam.colour  = LIGHTGREY;
        beam.damage  = dice_def( 3, 15 );
        beam.ex_size = 2;
        msg = "The giant spore explodes!";
        sanct_msg = "By Zin's power, the giant spore's explosion is contained.";
    }
    else if (type == MONS_BALL_LIGHTNING)
    {
        beam.flavour = BEAM_ELECTRICITY;
        beam.name    = "blast of lightning";
        beam.colour  = LIGHTCYAN;
        beam.damage  = dice_def( 3, 20 );
        beam.ex_size = coinflip() ? 3 : 2;
        msg = "The ball lightning explodes!";
        sanct_msg = "By Zin's power, the ball lightning's explosion "
                    "is contained.";
    }
    else
    {
        msg::streams(MSGCH_DIAGNOSTICS) << "Unknown spore type: "
                                        << static_cast<int>(type)
                                        << std::endl;
        return;
    }

    if (you.can_see(monster))
    {
        viewwindow(true, false);
        if (is_sanctuary(monster->x, monster->y))
            mpr(sanct_msg, MSGCH_GOD);
        else
            mpr(msg);
    }

    if (is_sanctuary(monster->x, monster->y))
        return;

    explosion(beam, false, false, true, true, mons_near(monster));
}

bolt mons_spells( int spell_cast, int power )
{
    ASSERT(power > 0);

    bolt beam;

    // Initialize to some bogus values so we can catch problems.
    beam.name         = "****";
    beam.colour       = 1000;
    beam.range        = beam.rangeMax = 8;
    beam.hit          = -1;
    beam.damage       = dice_def( 1, 0 );
    beam.ench_power   = -1;
    beam.type         = 0;
    beam.flavour      = BEAM_NONE;
    beam.thrower      = KILL_MISC;
    beam.is_beam      = false;
    beam.is_explosion = false;

    switch (spell_cast)
    {
    case SPELL_MAGIC_DART:
        beam.colour   = LIGHTMAGENTA;     // inv_colour [throw_2];
        beam.name     = "magic dart";     // inv_name [throw_2]);
        beam.range    = 6;
        beam.rangeMax = 10;
        beam.damage   = dice_def( 3, 4 + (power / 100) );
        beam.hit      = AUTOMATIC_HIT;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON_MISSILE;
        beam.flavour  = BEAM_MMISSILE;
        beam.is_beam  = false;
        break;

    case SPELL_THROW_FLAME:
        beam.colour   = RED;
        beam.name     = "puff of flame";
        beam.range    = 6;
        beam.rangeMax = 10;

        // should this be the same as magic missile?
        // No... magic missile is special in that it has a really
        // high to-hit value, so these should do more damage -- bwr
        beam.damage   = dice_def( 3, 5 + (power / 40) );

        beam.hit      = 25 + power / 40;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON_MISSILE;
        beam.flavour  = BEAM_FIRE;
        beam.is_beam  = false;
        break;

    case SPELL_THROW_FROST:
        beam.colour   = WHITE;
        beam.name     = "puff of frost";
        beam.range    = 6;
        beam.rangeMax = 10;

        // should this be the same as magic missile?
        // see SPELL_FLAME -- bwr
        beam.damage   = dice_def( 3, 5 + (power / 40) );

        beam.hit      = 25 + power / 40;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON_MISSILE;
        beam.flavour  = BEAM_COLD;
        beam.is_beam  = false;
        break;

    case SPELL_DISPEL_UNDEAD:
        beam.name     = "0";
        beam.flavour  = BEAM_DISPEL_UNDEAD;
        beam.thrower  = KILL_MON_MISSILE;
        beam.range    = 7 + random2(8);
        beam.rangeMax = 9;
        beam.damage   = dice_def( 3, std::min(6 + power / 10, 40) );
        beam.is_beam  = true;
        break;

    case SPELL_PARALYSE:
        beam.name     = "0";
        beam.range    = 5;
        beam.rangeMax = 9;
        beam.type     = 0;
        beam.flavour  = BEAM_PARALYSIS;
        beam.thrower  = KILL_MON_MISSILE;
        beam.is_beam  = true;
        break;

    case SPELL_SLOW:
        beam.name     = "0";
        beam.range    = 5;
        beam.rangeMax = 9;
        beam.type     = 0;
        beam.flavour  = BEAM_SLOW;
        beam.thrower  = KILL_MON_MISSILE;
        beam.is_beam  = true;
        break;

    case SPELL_HASTE:              // (self)
        beam.name     = "0";
        beam.range    = 5;
        beam.rangeMax = 9;
        beam.type     = 0;
        beam.flavour  = BEAM_HASTE;
        beam.thrower  = KILL_MON_MISSILE;
        beam.is_beam  = true;
        break;

    case SPELL_BACKLIGHT:
        beam.name     = "0";
        beam.range    = 5;
        beam.rangeMax = 9;
        beam.type     = 0;
        beam.flavour  = BEAM_BACKLIGHT;
        beam.thrower  = KILL_MON_MISSILE;
        beam.is_beam  = true;
        break;

    case SPELL_CONFUSE:
        beam.name     = "0";
        beam.range    = 5;
        beam.rangeMax = 9;
        beam.type     = 0;
        beam.flavour  = BEAM_CONFUSION;
        beam.thrower  = KILL_MON_MISSILE;
        beam.is_beam  = true;
        break;

    case SPELL_SLEEP:
        beam.name     = "0";
        beam.range    = 5;
        beam.rangeMax = 9;
        beam.type     = 0;
        beam.flavour  = BEAM_SLEEP;
        beam.thrower  = KILL_MON_MISSILE;
        beam.is_beam  = true;
        break;

    case SPELL_POLYMORPH_OTHER:
        beam.name     = "0";
        beam.range    = 6;
        beam.rangeMax = 9;
        beam.type     = 0;
        beam.flavour  = BEAM_POLYMORPH;
        beam.thrower  = KILL_MON_MISSILE;
        beam.is_beam  = true;
        break;

    case SPELL_VENOM_BOLT:
        beam.name     = "bolt of poison";
        beam.range    = 7;
        beam.rangeMax = 16;
        beam.damage   = dice_def( 3, 6 + power / 13 );
        beam.colour   = LIGHTGREEN;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON;
        beam.flavour  = BEAM_POISON;
        beam.hit      = 19 + power / 20;
        beam.is_beam  = true;
        break;

    case SPELL_POISON_ARROW:
        beam.name     = "poison arrow";
        beam.damage   = dice_def( 3, 7 + power / 12 );
        beam.colour   = LIGHTGREEN;
        beam.type     = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.thrower  = KILL_MON;
        beam.flavour  = BEAM_POISON_ARROW;
        beam.hit      = 20 + power / 25;
        beam.range    = beam.rangeMax = 8;
        break;

    case SPELL_BOLT_OF_MAGMA:
        beam.name     = "bolt of magma";
        beam.range    = 5;
        beam.rangeMax = 13;
        beam.damage   = dice_def( 3, 8 + power / 11 );
        beam.colour   = RED;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON;
        beam.flavour  = BEAM_LAVA;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        break;

    case SPELL_BOLT_OF_FIRE:
        beam.name     = "bolt of fire";
        beam.range    = 5;
        beam.rangeMax = 13;
        beam.damage   = dice_def( 3, 8 + power / 11 );
        beam.colour   = RED;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON;
        beam.flavour  = BEAM_FIRE;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        break;

    case SPELL_ICE_BOLT:
        beam.name     = "bolt of ice";
        beam.range    = 5;
        beam.rangeMax = 13;
        beam.damage   = dice_def( 3, 8 + power / 11 );
        beam.colour   = WHITE;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON;
        beam.flavour  = BEAM_ICE;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        break;

    case SPELL_BOLT_OF_COLD:
        beam.name     = "bolt of cold";
        beam.range    = 5;
        beam.rangeMax = 13;
        beam.damage   = dice_def( 3, 8 + power / 11 );
        beam.colour   = WHITE;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON;
        beam.flavour  = BEAM_COLD;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        break;

    case SPELL_FREEZING_CLOUD:
        beam.name     = "freezing blast";
        beam.range    = 5;
        beam.rangeMax = 12;
        beam.damage   = dice_def( 2, 9 + power / 11 );
        beam.colour   = WHITE;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON;
        beam.flavour  = BEAM_COLD;
        beam.hit      = 17 + power / 25;
        beam.is_beam  = true;
        beam.is_big_cloud = true;
        break;

    case SPELL_SHOCK:
        beam.name     = "zap";
        beam.range    = 8;
        beam.rangeMax = 16;
        beam.damage   = dice_def( 1, 8 + (power / 20) );
        beam.colour   = LIGHTCYAN;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON;
        beam.flavour  = BEAM_ELECTRICITY;
        beam.hit      = 17 + power / 20;
        beam.is_beam  = true;
        break;

    case SPELL_LIGHTNING_BOLT:
        beam.name     = "bolt of lightning";
        beam.range    = 7;
        beam.rangeMax = 16;
        beam.damage   = dice_def( 3, 10 + power / 17 );
        beam.colour   = LIGHTCYAN;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON;
        beam.flavour  = BEAM_ELECTRICITY;
        beam.hit      = 16 + power / 40;
        beam.is_beam  = true;
        break;

    case SPELL_INVISIBILITY:
        beam.name     = "0";
        beam.range    = 5;
        beam.rangeMax = 9;
        beam.type     = 0;
        beam.flavour  = BEAM_INVISIBILITY;
        beam.thrower  = KILL_MON;
        beam.is_beam  = true;
        break;

    case SPELL_FIREBALL:
        beam.colour   = RED;
        beam.name     = "fireball";
        beam.range    = 6;
        beam.rangeMax = 10;
        beam.damage   = dice_def( 3, 7 + power / 10 );
        beam.hit      = 40;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON;
        beam.flavour  = BEAM_FIRE;  // why not BEAM_FIRE? {dlb}
        beam.is_beam  = false;
        beam.is_explosion = true;
        break;

    case SPELL_LESSER_HEALING:
        beam.name     = "0";
        beam.range    = 5;
        beam.rangeMax = 9;
        beam.type     = 0;
        beam.flavour  = BEAM_HEALING;
        beam.thrower  = KILL_MON;
        beam.hit      = 25 + (power / 5);
        beam.is_beam  = true;
        break;

    case SPELL_TELEPORT_SELF:
        beam.name     = "0";
        beam.range    = 5;
        beam.rangeMax = 9;
        beam.type     = 0;
        beam.flavour  = BEAM_TELEPORT;        // 6 is used by digging
        beam.thrower  = KILL_MON;
        beam.is_beam  = true;
        break;

    case SPELL_TELEPORT_OTHER:
        beam.name     = "0";
        beam.range    = 5;
        beam.rangeMax = 9;
        beam.type     = 0;
        beam.flavour  = BEAM_TELEPORT;        // 6 is used by digging
        beam.thrower  = KILL_MON;
        beam.is_beam  = true;
        break;

    case SPELL_BLINK:
        beam.is_beam  = false;
        break;

    case SPELL_LEHUDIBS_CRYSTAL_SPEAR:      // was splinters
        beam.name     = "crystal spear";
        beam.range    = 7;
        beam.rangeMax = 16;
        beam.damage   = dice_def( 3, 16 + power / 10 );
        beam.colour   = WHITE;
        beam.type     = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.thrower  = KILL_MON;
        beam.flavour  = BEAM_MMISSILE;
        beam.hit      = 22 + power / 20;
        beam.is_beam  = false;
        break;

    case SPELL_DIG:
        beam.name     = "0";
        beam.range    = 3;
        beam.rangeMax = 7 + random2(power) / 10;
        beam.type     = 0;
        beam.flavour  = BEAM_DIGGING;
        beam.thrower  = KILL_MON;
        beam.is_beam  = true;
        break;

    case SPELL_BOLT_OF_DRAINING:      // negative energy
        beam.name     = "bolt of negative energy";
        beam.range    = 7;
        beam.rangeMax = 16;
        beam.damage   = dice_def( 3, 6 + power / 13 );
        beam.colour   = DARKGREY;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON;
        beam.flavour  = BEAM_NEG;
        beam.hit      = 16 + power / 35;
        beam.is_beam  = true;
        break;

    case SPELL_ISKENDERUNS_MYSTIC_BLAST: // mystic blast
        beam.colour   = LIGHTMAGENTA;
        beam.name     = "orb of energy";
        beam.range    = 6;
        beam.rangeMax = 10;
        beam.damage   = dice_def( 3, 7 + (power / 14) );
        beam.hit      = 20 + (power / 20);
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON_MISSILE;
        beam.flavour  = BEAM_MMISSILE;
        beam.is_beam  = false;
        break;

    case SPELL_STEAM_BALL:
        beam.colour   = LIGHTGREY;
        beam.name     = "ball of steam";
        beam.range    = 6;
        beam.rangeMax = 10;
        beam.damage   = dice_def( 3, 7 + (power / 15) );
        beam.hit      = 20 + power / 20;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON_MISSILE;
        beam.flavour  = BEAM_STEAM;
        beam.is_beam  = false;
        break;

    case SPELL_PAIN:
        beam.name       = "0";
        beam.range      = 7;
        beam.rangeMax   = 14;
        beam.type       = 0;
        beam.flavour    = BEAM_PAIN;     // pain
        beam.thrower    = KILL_MON;
        // beam.damage = dice_def( 1, 50 );
        beam.damage     = dice_def( 1, 7 + (power / 20) );
        beam.ench_power = 50;
        beam.is_beam    = true;
        break;

    case SPELL_STICKY_FLAME:
        beam.colour   = RED;
        beam.name     = "sticky flame";
        beam.range    = 6;
        beam.rangeMax = 10;
        beam.damage   = dice_def( 3, 3 + power / 50 );
        beam.hit      = 18 + power / 15;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON_MISSILE;
        beam.flavour  = BEAM_FIRE;
        beam.is_beam  = false;
        break;

    case SPELL_POISONOUS_CLOUD:       // demon
        beam.name     = "blast of poison";
        beam.range    = 7;
        beam.rangeMax = 16;
        beam.damage   = dice_def( 3, 3 + power / 25 );
        beam.colour   = LIGHTGREEN;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON;
        beam.flavour  = BEAM_POISON;
        beam.hit      = 18 + power / 25;
        beam.is_beam  = true;
        beam.is_big_cloud = true;
        break;

    case SPELL_ENERGY_BOLT:        // eye of devastation
        beam.colour   = YELLOW;
        beam.name     = "bolt of energy";
        beam.range    = 9;
        beam.rangeMax = 23;
        beam.damage   = dice_def( 3, 20 );
        beam.hit      = 15 + power / 30;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON_MISSILE;
        beam.flavour  = BEAM_NUKE;     // a magical missile which destroys walls
        beam.is_beam  = true;
        break;

    case SPELL_STING:              // sting
        beam.colour   = GREEN;
        beam.name     = "sting";
        beam.range    = 8;
        beam.rangeMax = 12;
        beam.damage   = dice_def( 1, 6 + power / 25 );
        beam.hit      = 60;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON_MISSILE;
        beam.flavour  = BEAM_POISON;
        beam.is_beam  = false;
        break;

    case SPELL_BOLT_OF_IRON:
        beam.colour   = LIGHTCYAN;
        beam.name     = "iron bolt";
        beam.range    = 4;
        beam.rangeMax = 8;
        beam.damage   = dice_def( 3, 8 + (power / 9) );
        beam.hit      = 20 + (power / 25);
        beam.type     = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.thrower  = KILL_MON_MISSILE;
        beam.flavour  = BEAM_MMISSILE;   // similarly unresisted thing
        beam.is_beam  = false;
        break;

    case SPELL_STONE_ARROW:
        beam.colour   = LIGHTGREY;
        beam.name     = "stone arrow";
        beam.range    = 8;
        beam.rangeMax = 12;
        beam.damage   = dice_def( 3, 5 + (power / 10) );
        beam.hit      = 14 + power / 35;
        beam.type     = dchar_glyph(DCHAR_FIRED_MISSILE);
        beam.thrower  = KILL_MON_MISSILE;
        beam.flavour  = BEAM_MMISSILE;   // similarly unresisted thing
        beam.is_beam  = false;
        break;

    case SPELL_POISON_SPLASH:
        beam.colour   = GREEN;
        beam.name     = "splash of poison";
        beam.range    = 5;
        beam.rangeMax = 10;
        beam.damage   = dice_def( 1, 4 + power / 10 );
        beam.hit      = 16 + power / 20;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON_MISSILE;
        beam.flavour  = BEAM_POISON;
        beam.is_beam  = false;
        break;

    case SPELL_DISINTEGRATE:
        beam.name       = "0";
        beam.range      = 7;
        beam.rangeMax   = 14;
        beam.type       = 0;
        beam.flavour    = BEAM_DISINTEGRATION;
        beam.thrower    = KILL_MON;
        beam.ench_power = 50;
        // beam.hit = 30 + (power / 10);
        beam.damage     = dice_def( 1, 30 + (power / 10) );
        beam.is_beam    = true;
        break;

    case SPELL_MEPHITIC_CLOUD:          // swamp drake
        beam.name     = "foul vapour";
        beam.range    = 7;
        beam.rangeMax = 16;
        beam.damage   = dice_def( 3, 2 + power / 25 );
        beam.colour   = GREEN;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON;
        beam.flavour  = BEAM_POISON;
        beam.hit      = 14 + power / 30;
        beam.is_beam  = true;
        beam.is_big_cloud = true;
        break;

    case SPELL_MIASMA:            // death drake
        beam.name     = "foul vapour";
        beam.range    = beam.rangeMax = 8;
        beam.damage   = dice_def( 3, 5 + power / 24 );
        beam.colour   = DARKGREY;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON;
        beam.flavour  = BEAM_MIASMA;
        beam.hit      = 17 + power / 20;
        beam.is_beam  = true;
        beam.is_big_cloud = true;
        break;

    case SPELL_QUICKSILVER_BOLT:   // Quicksilver dragon
        beam.colour   = random_colour();
        beam.name     = "bolt of energy";
        beam.range    = 9;
        beam.rangeMax = 23;
        beam.damage   = dice_def( 3, 25 );
        beam.hit      = 16 + power / 25;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON_MISSILE;
        beam.flavour  = BEAM_MMISSILE;
        beam.is_beam  = false;
        break;

    case SPELL_HELLFIRE:           // fiend's hellfire
        beam.name         = "hellfire";
        beam.aux_source   = "blast of hellfire";
        beam.colour       = RED;
        beam.range        = 4;
        beam.rangeMax     = 13;
        beam.damage       = dice_def( 3, 25 );
        beam.hit          = 24;
        beam.type         = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower      = KILL_MON;
        beam.flavour      = BEAM_HELLFIRE;
        beam.is_beam      = true;
        beam.is_explosion = true;
        break;

    case SPELL_METAL_SPLINTERS:
        beam.name     = "spray of metal splinters";
        beam.range    = 7;
        beam.rangeMax = 16;
        beam.damage   = dice_def( 3, 20 + power / 20 );
        beam.colour   = CYAN;
        beam.type     = dchar_glyph(DCHAR_FIRED_ZAP);
        beam.thrower  = KILL_MON;
        beam.flavour  = BEAM_FRAG;
        beam.hit      = 19 + power / 30;
        beam.is_beam  = true;
        break;

    case SPELL_BANISHMENT:
        beam.name     = "0";
        beam.range    = 5;
        beam.rangeMax = 9;
        beam.type     = 0;
        beam.flavour  = BEAM_BANISH;
        beam.thrower  = KILL_MON_MISSILE;
        beam.is_beam  = true;
        break;

    case SPELL_BLINK_OTHER:
        beam.name       = "0";
        beam.type       = 0;
        beam.flavour    = BEAM_BLINK;
        beam.thrower    = KILL_MON;
        beam.is_beam    = true;
        beam.is_enchant = true;
        beam.range      = 8;
        beam.rangeMax   = 8;
        break;

    default:
        DEBUGSTR("Unknown spell");
    }

    return (beam);
}                               // end mons_spells()

static int _monster_abjure_square(const coord_def &pos,
                                  int pow, int actual,
                                  int wont_attack)
{
    const int mindex = mgrd(pos);
    if (mindex == NON_MONSTER)
        return (0);

    monsters *target = &menv[mindex];
    if (!target->alive() || ((bool)wont_attack == mons_wont_attack(target)))
        return (0);

    mon_enchant abj = target->get_ench(ENCH_ABJ);
    if (abj.ench == ENCH_NONE)
        return (0);

    pow = std::max(20, fuzz_value(pow, 40, 25));

    if (!actual)
        return (pow > 40 || pow >= abj.duration);

    // TSO and Trog's abjuration protection.
    if (you.religion == GOD_SHINING_ONE)
    {
        pow = pow * (30 - target->hit_dice) / 30;
        if (pow < abj.duration)
            simple_god_message(" protects your fellow warrior from evil magic!");
    }
    else if (you.religion == GOD_TROG)
    {
        pow = pow * 4 / 5;
        if (pow < abj.duration)
            simple_god_message(" shields your ally from puny magic!");
    }
    else if (is_sanctuary(target->x, target->y))
    {
        mpr("Zin's power protects your fellow warrior from evil magic!",
            MSGCH_GOD);
        return (0);
    }

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Abj: dur: %d, pow: %d, ndur: %d",
         abj.duration, pow, abj.duration - pow);
#endif

    if (!target->lose_ench_duration(abj, pow))
        simple_monster_message(target, " shudders.");

    return (1);
}

static int _apply_radius_around_square( const coord_def &c, int radius,
                                int (*fn)(const coord_def &, int, int, int),
                                int pow, int par1, int par2)
{
    int res = 0;
    for (int yi = -radius; yi <= radius; ++yi)
    {
        const coord_def c1(c.x - radius, c.y + yi);
        const coord_def c2(c.x + radius, c.y + yi);
        if (in_bounds(c1))
            res += fn(c1, pow, par1, par2);
        if (in_bounds(c2))
            res += fn(c2, pow, par1, par2);
    }

    for (int xi = -radius + 1; xi < radius; ++xi)
    {
        const coord_def c1(c.x + xi, c.y - radius);
        const coord_def c2(c.x + xi, c.y + radius);
        if (in_bounds(c1))
            res += fn(c1, pow, par1, par2);
        if (in_bounds(c2))
            res += fn(c2, pow, par1, par2);
    }
    return (res);
}

static int _monster_abjuration(const monsters *caster, bool actual)
{
    const bool wont_attack = mons_wont_attack(caster);
    int maffected = 0;

    if (actual)
        mpr("Send 'em back where they came from!");

    int pow = std::min(caster->hit_dice * 90, 2500);

    // Abjure radius.
    for (int rad = 1; rad < 5 && pow >= 30; ++rad)
    {
        int number_hit =
            _apply_radius_around_square(caster->pos(), rad,
                                        _monster_abjure_square,
                                        pow, actual, wont_attack);

        maffected += number_hit;

        // Each affected monster drops power.
        //
        // We could further tune this by the actual amount of abjuration
        // damage done to each summon, but the player will probably never
        // notice. :-)
        //
        while (number_hit-- > 0)
            pow = pow * 90 / 100;

        pow /= 2;
    }
    return (maffected);
}

bool silver_statue_effects(monsters *mons)
{
    if ((mons->attitude == ATT_HOSTILE) && ((mons_player_visible(mons) || one_chance_in(3)) && !one_chance_in(3)))
    {
        const std::string msg =
            "'s eyes glow " + weird_glowing_colour() + '.';
        simple_monster_message(mons, msg.c_str(), MSGCH_WARN);

        create_monster(
            mgen_data(
                summon_any_demon((coinflip() ? DEMON_COMMON
                                             : DEMON_LESSER)),
                BEH_HOSTILE, 5, you.pos(), MHITYOU));
        return (true);
    }
    if ((mons->attitude == ATT_FRIENDLY) && one_chance_in(3))
    {
        const std::string msg =
            "'s eyes glow " + weird_glowing_colour() + '.';
        simple_monster_message(mons, msg.c_str(), MSGCH_WARN);

        create_monster(
            mgen_data(
                summon_any_demon((coinflip() ? DEMON_COMMON
                                             : DEMON_LESSER)),
                BEH_FRIENDLY, 5, mons->pos() )); // //
        return (true);
    }
    return (false);
}

bool orange_statue_effects(monsters *mons)
{
    if ((mons->attitude == ATT_HOSTILE) && ((mons_player_visible(mons) || one_chance_in(3))
					  && !one_chance_in(3)))
    {
        mpr("A hostile presence attacks your mind!", MSGCH_WARN);

        miscast_effect(SPTYP_DIVINATION, random2(15), random2(150), 100,
                       "an orange crystal statue");
        return (true);
    }

    // // this is new
    if (mons->attitude == ATT_FRIENDLY)
    {
        for (int stupidcounter = 0; stupidcounter < 20; stupidcounter++)
	{
            int randomindex = random_range(1,MAX_MONSTERS-1);
            if (((abs(menv[randomindex].x - mons->x) < 8) && (abs(menv[randomindex].y - mons->y) < 8)) && ((mons_class_is_confusable(menv[randomindex].type)) && menv[randomindex].attitude == ATT_HOSTILE))
	    {
	      if (menv[randomindex].add_ench( mon_enchant(ENCH_CONFUSION, 10)) && mons_player_visible(mons))
	      {
                  simple_monster_message( &menv[randomindex], " appears confused.");
              }
	    }

	}
    }
    return (false);
}

bool orc_battle_cry(monsters *chief)
{
    const actor *foe = chief->get_foe();
    int affected = 0;

    if (foe
        && (foe != &you || !mons_friendly(chief))
        && !silenced(chief->x, chief->y)
        && chief->can_see(foe)
        && coinflip())
    {
        const int boss_index = monster_index(chief);
        const int level = chief->hit_dice > 12? 2 : 1;
        std::vector<monsters*> seen_affected;
        for (int i = 0; i < MAX_MONSTERS; ++i)
        {
            monsters *mon = &menv[i];
            if (mon != chief
                && mon->alive()
                && mons_species(mon->type) == MONS_ORC
                && mons_aligned(boss_index, i)
                && mon->hit_dice < chief->hit_dice
                && !mon->has_ench(ENCH_BERSERK)
                && !mon->cannot_move()
                && !mon->confused()
                && chief->can_see(mon))
            {
                mon_enchant ench = mon->get_ench(ENCH_BATTLE_FRENZY);
                if (ench.ench == ENCH_NONE || ench.degree < level)
                {
                    const int dur =
                        random_range(12, 20) * speed_to_duration(mon->speed);

                    if (ench.ench != ENCH_NONE)
                    {
                        ench.degree   = level;
                        ench.duration = std::max(ench.duration, dur);
                        mon->update_ench(ench);
                    }
                    else
                    {
                        mon->add_ench(mon_enchant(ENCH_BATTLE_FRENZY, level,
                                                  KC_OTHER, dur));
                    }

                    affected++;
                    if (you.can_see(mon))
                        seen_affected.push_back(mon);

                    if (mon->asleep())
                    {
                        behaviour_event(mon, ME_DISTURB, MHITNOT,
                                        chief->x, chief->y);
                    }
                }
            }
        }

        if (affected)
        {
            if (you.can_see(chief) && player_can_hear(chief->x, chief->y))
            {
                mprf(MSGCH_SOUND, "%s roars a battle-cry!",
                     chief->name(DESC_CAP_THE).c_str());
            }

            // The yell happens whether you happen to see it or not.
            noisy(15, chief->x, chief->y);

            // Disabling detailed frenzy announcement because it's so spammy.
            const msg_channel_type channel =
                        mons_friendly(chief) ? MSGCH_MONSTER_ENCHANT
                                             : MSGCH_FRIEND_ENCHANT;

            if (!seen_affected.empty())
            {
                std::string who;
                if (seen_affected.size() == 1)
                {
                    who = seen_affected[0]->name(DESC_CAP_THE);
                    mprf(channel, "%s goes into a battle-frenzy!", who.c_str());
                }
                else
                {
                    int type = seen_affected[0]->type;
                    for (unsigned int i = 0; i < seen_affected.size(); i++)
                    {
                        if (seen_affected[i]->type != type)
                        {
                            // just mention plain orcs
                            type = MONS_ORC;
                            break;
                        }
                    }
                    who = get_monster_data(type)->name;

                    mprf(channel, "%s %s go into a battle-frenzy!",
                         mons_friendly(chief) ? "Your" : "The",
                         pluralise(who).c_str());
                }
            }
        }
    }
    // Orc battle cry doesn't cost the monster an action.
    return (false);
}

static bool _make_monster_angry(const monsters *mon, monsters *targ)
{
    if (mons_friendly(mon) != mons_friendly(targ))
        return (false);

    // targ is guaranteed to have a foe (needs_berserk checks this).
    // Now targ needs to be closer to *its* foe than mon is (otherwise
    // mon might be in the way).

    coord_def victim;
    if (targ->foe == MHITYOU)
        victim = you.pos();
    else if (targ->foe != MHITNOT)
    {
        const monsters *vmons = &menv[targ->foe];
        if (!vmons->alive())
            return (false);
        victim = vmons->pos();
    }
    else
    {
        // Should be impossible. needs_berserk should find this case.
        ASSERT(false);
        return (false);
    }

    // If mon may be blocking targ from its victim, don't try.
    if (victim.distance_from(targ->pos()) > victim.distance_from(mon->pos()))
        return (false);

    if (mons_near(mon) && player_monster_visible(mon))
    {
        mprf("%s goads %s on!", mon->name(DESC_CAP_THE).c_str(),
             targ->name(DESC_NOCAP_THE).c_str());
    }

    targ->go_berserk(false);

    return (true);
}

bool moth_incite_monsters(const monsters *mon)
{
    if (is_sanctuary(you.x_pos, you.y_pos)
        || is_sanctuary(mon->x, mon->y))
    {
        return 0;
    }

    int goaded = 0;
    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters *targ = &menv[i];
        if (targ == mon || !targ->alive() || !targ->needs_berserk())
            continue;

        if (mon->pos().distance_from(targ->pos()) > 3)
            continue;

        if (is_sanctuary(targ->x, targ->y))
            continue;

        // Cannot goad other moths of wrath!
        if (targ->type == MONS_MOTH_OF_WRATH)
            continue;

        if (_make_monster_angry(mon, targ) && !one_chance_in(3 * ++goaded))
            return (true);
    }

    return (false);
}

void mons_clear_trapping_net(monsters *mon)
{
    if (!mons_is_caught(mon))
        return;

    const int net = get_trapping_net(mon->x, mon->y);
    if (net != NON_ITEM)
        remove_item_stationary(mitm[net]);

    mon->del_ench(ENCH_HELD, true);
}
