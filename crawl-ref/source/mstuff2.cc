/*
 *  File:       mstuff2.cc
 *  Summary:    Misc monster related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
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

#include "externs.h"

#include "beam.h"
#include "debug.h"
#include "effects.h"
#include "item_use.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mon-util.h"
#include "player.h"
#include "spells2.h"
#include "spells4.h"
#include "spl-cast.h"
#include "stuff.h"
#include "view.h"

static unsigned char monster_abjuration(int pow, bool test);

// XXX: must fix species abils to not use duration 15
// -- ummm ... who wrote this? {dlb}

// NB: only works because grid location already verified
//     to be some sort of trap prior to function call: {dlb}
void mons_trap(struct monsters *monster)
{
    if (!is_trap_square(monster->x, monster->y))
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

    //
    // Trap damage to monsters is not a function of level, beacuse they
    // are fairly stupid and tend to have fewer hp than players -- this
    // choice prevents traps from easily killing large monsters fairly
    // deep within the dungeon.
    //
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
    // amnesia traps do not affect monsters (yet) and
    // only monsters of normal+ IQ will direct a msg
    // to the player - also, *never* revealed: {dlb}
    case TRAP_AMNESIA:
        if (mons_intel(monster->type) > I_ANIMAL)
            simple_monster_message(monster,
                                   " seems momentarily disoriented.");
        break;
    // blade traps sometimes fail to trigger altogether,
    // resulting in an "early return" from this f(x) for
    // some - otherwise, blade *always* revealed: {dlb}
    case TRAP_BLADE:
        if (one_chance_in(5))
        {
            if (trapKnown)
            {
                simple_monster_message(monster,
                                        " fails to trigger a blade trap.");
            }
            return;             // early return {dlb}
        }
        else if (random2(monster->evasion) > 8)
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
                strcpy(info, "A huge blade swings out");

                if (player_monster_visible( monster ))
                {
                    strcat(info, " and slices into ");
                    strcat(info, ptr_monam( monster, DESC_NOCAP_THE ));
                }

                strcat(info, "!");
                mpr(info);
            }

            damage_taken = 10 + random2avg(29, 2);
            damage_taken -= random2(1 + monster->armour_class);

            if (damage_taken < 0)
                damage_taken = 0;
        }

        revealTrap = true;
        break;

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
                strcpy(info, "You hear a loud \"Zot\"!");
            else
                strcpy(info, "You hear a distant \"Zot\"!");
            mpr(info);
        }

        // determine trap effects upon monster, based upon
        // whether it is naughty or nice to the player: {dlb}

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
    }


    // go back and handle projectile traps: {dlb}
    bool apply_poison = false;

    if (projectileFired)
    {
        // projectile traps *always* revealed after "firing": {dlb}
        revealTrap = true;

        // determine whether projectile hits, calculate damage: {dlb}
        if (((20 + (you.your_level * 2)) * random2(200)) / 100
                                                    >= monster->evasion)
        {
            damage_taken = roll_dice( beem.damage );
            damage_taken -= random2(1 + monster->armour_class);

            if (damage_taken < 0)
                damage_taken = 0;

            if (beem.colour == OBJ_MISSILES
                && beem.type == MI_NEEDLE
                && random2(100) < 50 - (3*monster->armour_class/2))
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
                      ptr_monam( monster, DESC_NOCAP_THE ),
                      (damage_taken == 0) ? ", but does no damage" : "");
        }

        if (apply_poison)
            poison_monster( monster, false );

        // generate "fallen" projectile, where appropriate: {dlb}
        if (random2(10) < 7)
        {
            beem.target_x = monster->x;
            beem.target_y = monster->y;
            itrap(beem, which_trap);
        }
    }


    // reveal undiscovered traps, where appropriate: {dlb}
    if (monsterNearby && !trapKnown && revealTrap)
    {
        grd[monster->x][monster->y] = trap_category(env.trap[which_trap].type);
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

    return;
}                               // end mons_trap()

void mons_cast(struct monsters *monster, struct bolt &pbolt, int spell_cast)
{
    // always do setup.  It might be done already, but it doesn't
    // hurt to do it again (cheap).
    setup_mons_cast(monster, pbolt, spell_cast);

    // single calculation permissible {dlb}
    bool monsterNearby = mons_near(monster);

    int sumcount = 0;
    int sumcount2;
    int summonik = 0;
    int duration = 0;

#if DEBUG_DIAGNOSTICS
    snprintf( info, INFO_SIZE, "Mon #%d casts %s (#%d)", monster_index(monster),
             mons_spell_name( spell_cast ), spell_cast );

    mpr( info, MSGCH_DIAGNOSTICS );
#endif

    if (spell_cast == MS_HELLFIRE_BURST || spell_cast == MS_BRAIN_FEED
        || spell_cast == MS_SMITE || spell_cast == MS_MUTATION)
    {                           // etc.
        if (monster->foe == MHITYOU || monster->foe == MHITNOT)
        {
            if (monsterNearby)
                direct_effect( pbolt );
            return;
        }

        mons_direct_effect( pbolt, monster_index(monster) );
        return;
    }

    switch (spell_cast)
    {
    case MS_SUMMON_SMALL_MAMMALS:
    case MS_VAMPIRE_SUMMON:
	if ( spell_cast == MS_SUMMON_SMALL_MAMMALS )
	    sumcount2 = 1 + random2(4);
	else
	    sumcount2 = 3 + random2(3) + monster->hit_dice / 5;

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            int mons = MONS_GIANT_BAT;

            if (!one_chance_in(3))
            {
                switch (random2(4))
                {
                case 0:
                    mons = MONS_ORANGE_RAT;
                    break;

                case 1:
                    mons = MONS_GREEN_RAT;
                    break;

                case 2:
                    mons = MONS_GREY_RAT;
                    break;

                case 3:
                default:
                    mons = MONS_RAT;
                    break;
                }
            }

            create_monster( mons, ENCH_ABJ_V, SAME_ATTITUDE(monster), 
                            monster->x, monster->y, monster->foe, 250 );
        }
        return;

    case MS_LEVEL_SUMMON:       // summon anything appropriate for level
        if (!mons_friendly(monster) && monsterNearby
            && monster_abjuration(1, true) > 0 && coinflip())
        {
            monster_abjuration( monster->hit_dice * 10, false );
            return;
        }

        sumcount2 = 1 + random2(4) + random2( monster->hit_dice / 7 + 1 );

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster( RANDOM_MONSTER, ENCH_ABJ_V, SAME_ATTITUDE(monster),
                            monster->x, monster->y, monster->foe, 250 );
        }
        return;

    case MS_FAKE_RAKSHASA_SUMMON:
        sumcount2 = (coinflip() ? 2 : 3);

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster( MONS_RAKSHASA_FAKE, ENCH_ABJ_III, 
                            SAME_ATTITUDE(monster), monster->x, monster->y, 
                            monster->foe, 250 );
        }
        return;

    case MS_SUMMON_DEMON: // class 2-4 demons
        if (!mons_friendly(monster) && monsterNearby
            && monster_abjuration(1, true) > 0 && coinflip())
        {
            monster_abjuration(monster->hit_dice * 10, false);
            return;
        }

        sumcount2 = 1 + random2(2) + random2( monster->hit_dice / 10 + 1 );

        duration  = ENCH_ABJ_II + monster->hit_dice / 10;
        if (duration > ENCH_ABJ_VI)
            duration = ENCH_ABJ_VI;

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster( summon_any_demon(DEMON_COMMON), duration,
                            SAME_ATTITUDE(monster), monster->x, monster->y, 
                            monster->foe, 250 );
        }
        return;

    case MS_ANIMATE_DEAD:
        // see special handling in monstuff::handle_spell {dlb}
        animate_dead( 5 + random2(5), SAME_ATTITUDE(monster), monster->foe, 1 );
        return;

    case MS_SUMMON_DEMON_LESSER: // class 5 demons
        sumcount2 = 1 + random2(3) + random2( monster->hit_dice / 5 + 1 );

        duration  = ENCH_ABJ_II + monster->hit_dice / 5;
        if (duration > ENCH_ABJ_VI)
            duration = ENCH_ABJ_VI;

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster( summon_any_demon(DEMON_LESSER), duration,
                            SAME_ATTITUDE(monster), monster->x, monster->y, 
                            monster->foe, 250 );
        }
        return;

    case MS_SUMMON_UFETUBUS:
        sumcount2 = 2 + random2(2) + random2( monster->hit_dice / 5 + 1 );

        duration  = ENCH_ABJ_II + monster->hit_dice / 5;
        if (duration > ENCH_ABJ_VI)
            duration = ENCH_ABJ_VI;

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster( MONS_UFETUBUS, duration, SAME_ATTITUDE(monster),
                            monster->x, monster->y, monster->foe, 250 );
        }
        return;

    case MS_SUMMON_BEAST:       // Geryon
        create_monster( MONS_BEAST, ENCH_ABJ_IV, SAME_ATTITUDE(monster),
                        monster->x, monster->y, monster->foe, 250 );
        return;

    case MS_SUMMON_MUSHROOMS:   // Summon swarms of icky crawling fungi.
        if (!mons_friendly(monster) && monsterNearby
            && monster_abjuration(1, true) > 0 && coinflip())
        {
            monster_abjuration( monster->hit_dice * 10, false );
            return;
        }

        sumcount2 = 1 + random2(2) + random2( monster->hit_dice / 4 + 1 );

        duration  = ENCH_ABJ_II + monster->hit_dice / 5;
        if (duration > ENCH_ABJ_VI)
            duration = ENCH_ABJ_VI;

        for (int i = 0; i < sumcount2; ++i)
            create_monster(MONS_WANDERING_MUSHROOM, duration, 
                    SAME_ATTITUDE(monster),
                    monster->x, monster->y,
                    monster->foe,
                    250);
        return;

    case MS_SUMMON_UNDEAD:      // summon undead around player
        if (!mons_friendly(monster) && monsterNearby
            && monster_abjuration(1, true) > 0 && coinflip())
        {
            monster_abjuration( monster->hit_dice * 10, false );
            return;
        }

        sumcount2 = 2 + random2(2) + random2( monster->hit_dice / 4 + 1 );

        duration  = ENCH_ABJ_II + monster->hit_dice / 5;
        if (duration > ENCH_ABJ_VI)
            duration = ENCH_ABJ_VI;

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            do
            {
                summonik = random2(241);        // hmmmm ... {dlb}
            }
            while (mons_class_holiness(summonik) != MH_UNDEAD);

            create_monster(summonik, duration, SAME_ATTITUDE(monster),
                           you.x_pos, you.y_pos, monster->foe, 250);
        }
        return;

    case MS_TORMENT:
        if (!monsterNearby || mons_friendly(monster))
            return;

        simple_monster_message(monster, " calls on the powers of Hell!");

        torment(monster_index(monster), monster->x, monster->y);
        return;

    case MS_SUMMON_DEMON_GREATER:
        if (!mons_friendly(monster) && !monsterNearby &&
            monster_abjuration(1, true) > 0 && coinflip())
        {
            monster_abjuration(monster->hit_dice * 10, false);
            return;
        }

        sumcount2 = 1 + random2( monster->hit_dice / 10 + 1 );

        duration  = ENCH_ABJ_II + monster->hit_dice / 10;
        if (duration > ENCH_ABJ_VI)
            duration = ENCH_ABJ_VI;

        for (sumcount = 0; sumcount < sumcount2; sumcount++)
        {
            create_monster( summon_any_demon(DEMON_GREATER), duration,
                            SAME_ATTITUDE(monster), monster->x, monster->y, 
                            monster->foe, 250 );
        }
        return;

    // Journey -- Added in Summon Lizards and Draconian
    case MS_SUMMON_DRAKES:
        if (!mons_friendly( monster ) && !monsterNearby &&
            monster_abjuration( 1, true ) > 0 && coinflip())
        {
            monster_abjuration( monster->hit_dice * 10, false );
            return;
        }

        sumcount2 = 1 + random2(3) + random2( monster->hit_dice / 5 + 1 );

        duration  = ENCH_ABJ_II + monster->hit_dice / 10;
        if (duration > ENCH_ABJ_VI)
            duration = ENCH_ABJ_VI;

        {
            std::vector<int> monsters;

            for (sumcount = 0; sumcount < sumcount2; sumcount++)
            {
                const int mons = rand_dragon( DRAGON_LIZARD );
                if (mons == MONS_DRAGON)
                {
                    monsters.clear();
                    monsters.push_back( rand_dragon(DRAGON_DRAGON) );
                    break;
                }
                monsters.push_back( mons );
            }

            for (int i = 0, size = monsters.size(); i < size; ++i)
            {
                create_monster( monsters[i], duration,
                                SAME_ATTITUDE(monster), 
                                monster->x, monster->y, monster->foe, 250 );
            }
        }
        break;

    case MS_CANTRIP:
        // Monster spell of uselessness, just prints a message. 
        // This spell exists so that some monsters with really strong
        // spells (ie orc priest) can be toned down a bit. -- bwr
        //
        // XXX: Needs expansion, and perhaps different priest/mage flavours.
        switch (random2(7))
        {
        case 0:
            simple_monster_message( monster, " glows brightly for a moment.",
                                    MSGCH_MONSTER_ENCHANT );
            break;
        case 1:
            mpr( "You feel troubled." );
            break;
        case 2:
            mpr( "You feel a wave of unholy energy pass over you." );
            break;
        case 3:
            simple_monster_message( monster, " looks stronger.", 
                                    MSGCH_MONSTER_ENCHANT );
            break;
        case 4:
            simple_monster_message( monster, " becomes somewhat translucent.",
                                    MSGCH_MONSTER_ENCHANT );
            break;
        case 5:
            simple_monster_message( monster, "'s eyes start to glow.",
                                    MSGCH_MONSTER_ENCHANT );
            break;
        case 6:
        default:
            if (one_chance_in(20))
                mpr( "You resist (whatever that was supposed to do)." );
            else
                mpr( "You resist." );
            break;
        }
        return;
    }

    fire_beam( pbolt );
}                               // end mons_cast()


/*
 * setup bolt structure for monster spell casting.
 *
 */

void setup_mons_cast(struct monsters *monster, struct bolt &pbolt, int spell_cast)
{
    // always set these -- used by things other than fire_beam()

    // [ds] Used to be 12 * MHD, reduced to 7 * HD.
    pbolt.ench_power = 4 * monster->hit_dice;

    if (spell_cast == MS_TELEPORT)
        pbolt.ench_power = 2000;
    else if (spell_cast == MS_PAIN) // this is cast by low HD monsters
        pbolt.ench_power *= 2;

    pbolt.beam_source = monster_index(monster);

    // set bolt type
    if (spell_cast == MS_HELLFIRE_BURST
        || spell_cast == MS_BRAIN_FEED
        || spell_cast == MS_SMITE || spell_cast == MS_MUTATION)
    {                           // etc.
        switch (spell_cast)
        {
        case MS_HELLFIRE_BURST:
            pbolt.type = DMNBM_HELLFIRE;
            break;
        case MS_BRAIN_FEED:
            pbolt.type = DMNBM_BRAIN_FEED;
            break;
        case MS_SMITE:
            pbolt.type = DMNBM_SMITING;
            break;
        case MS_MUTATION:
            pbolt.type = DMNBM_MUTATION;
            break;
        }
        return;
    }

    // the below are no-ops since they don't involve direct_effect,
    // fire_tracer, or beam.
    switch (spell_cast)
    {
    case MS_SUMMON_SMALL_MAMMALS:
    case MS_VAMPIRE_SUMMON:
    case MS_LEVEL_SUMMON:       // summon anything appropriate for level
    case MS_FAKE_RAKSHASA_SUMMON:
    case MS_SUMMON_DEMON:
    case MS_ANIMATE_DEAD:
    case MS_SUMMON_DEMON_LESSER:
    case MS_SUMMON_UFETUBUS:
    case MS_SUMMON_BEAST:       // Geryon
    case MS_SUMMON_UNDEAD:      // summon undead around player
    case MS_SUMMON_MUSHROOMS:
    case MS_SUMMON_DRAKES:
    case MS_TORMENT:
    case MS_SUMMON_DEMON_GREATER:
    case MS_CANTRIP:
        return;
    default:
        break;
    }

    // Need to correct this for power of spellcaster
    int power = 12 * monster->hit_dice;

    struct bolt theBeam = mons_spells(spell_cast, power);

    pbolt.colour = theBeam.colour;
    pbolt.range = theBeam.range;
    pbolt.rangeMax = theBeam.rangeMax;
    pbolt.hit = theBeam.hit;
    pbolt.damage = theBeam.damage;
    if (theBeam.ench_power != -1)
        pbolt.ench_power = theBeam.ench_power;
    pbolt.type = theBeam.type;
    pbolt.flavour = theBeam.flavour;
    pbolt.thrower = theBeam.thrower;
    pbolt.aux_source.clear();
    pbolt.name = theBeam.name;
    pbolt.is_beam = theBeam.is_beam;
    pbolt.source_x = monster->x;
    pbolt.source_y = monster->y;
    pbolt.is_tracer = false; 
    pbolt.is_explosion = theBeam.is_explosion;

    if (pbolt.name.length() && pbolt.name[0] != '0')
        pbolt.aux_source = pbolt.name;
    else 
        pbolt.aux_source.clear();

    if (spell_cast == MS_HASTE
        || spell_cast == MS_INVIS
        || spell_cast == MS_HEAL || spell_cast == MS_TELEPORT)
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
}                               // end setup_mons_cast()


void monster_teleport(struct monsters *monster, bool instan, bool silent)
{
    if (!instan)
    {
        if (mons_del_ench(monster, ENCH_TP_I, ENCH_TP_IV))
        {
            if (!silent)
                simple_monster_message(monster, " seems more stable.");
        }
        else
            mons_add_ench(monster, (coinflip() ? ENCH_TP_III : ENCH_TP_IV ));

        return;
    }

    bool was_seen = player_monster_visible(monster) && mons_near(monster);

    if (!silent)
        simple_monster_message(monster, " disappears!");

    // pick the monster up
    mgrd[monster->x][monster->y] = NON_MONSTER;

    int newx, newy;
    while(true)
    {
        newx = 10 + random2(GXM - 20);
        newy = 10 + random2(GYM - 20);

        // don't land on top of another monster
        if (mgrd[newx][newy] != NON_MONSTER)
            continue;

        if (monster_habitable_grid(monster, grd[newx][newy]))
            break;
    }

    monster->x = newx;
    monster->y = newy;

    mgrd[monster->x][monster->y] = monster_index(monster);

    /* Mimics change form/colour when t'ported */
    if (mons_is_mimic( monster->type ))
    {
        monster->type = MONS_GOLD_MIMIC + random2(5);
        monster->colour = get_mimic_colour( monster );
    }

    if (!silent)
    {
        if (was_seen)
            simple_monster_message(monster, " reappears nearby!");
        else
            simple_monster_message(monster, " appears out of thin air!");
    }

    if (player_monster_visible(monster) && mons_near(monster))
        seen_monster(monster);
}                               // end monster_teleport()

void setup_dragon(struct monsters *monster, struct bolt &pbolt)
{
    const int type = (mons_genus( monster->type ) == MONS_DRACONIAN) 
                            ? draco_subspecies( monster ) : monster->type;
    int scaling = 100;

    pbolt.name = ptr_monam( monster, DESC_PLAIN );

    switch (type)
    {
    case MONS_FIREDRAKE:
    case MONS_HELL_HOUND:
    case MONS_DRAGON:
    case MONS_LINDWURM:
    case MONS_XTAHUA:
        pbolt.name += "'s blast of flame";
        pbolt.flavour = BEAM_FIRE;
        pbolt.colour = RED;
        pbolt.aux_source = "blast of fiery breath";
        break;

    case MONS_ICE_DRAGON:
        pbolt.name += "'s blast of cold";
        pbolt.flavour = BEAM_COLD;
        pbolt.colour = WHITE;
        pbolt.aux_source = "blast of icy breath";
        break;

    case MONS_RED_DRACONIAN:
        pbolt.name += "'s searing breath";
#ifdef DEBUG_DIAGNOSTICS
        mprf( MSGCH_DIAGNOSTICS, "bolt name: '%s'", pbolt.name.c_str() );
#endif
        pbolt.flavour = BEAM_FIRE;
        pbolt.colour = RED;
        pbolt.aux_source = "blast of searing breath";
        scaling = 65;
        break;

    case MONS_WHITE_DRACONIAN:
        pbolt.name += "'s chilling breath";
#ifdef DEBUG_DIAGNOSTICS
        mprf( MSGCH_DIAGNOSTICS, "bolt name: '%s'", pbolt.name.c_str() );
#endif
        pbolt.flavour = BEAM_COLD;
        pbolt.colour = WHITE;
        pbolt.aux_source = "blast of chilling breath";
        scaling = 65;
        break;
        
    default:
        DEBUGSTR("Bad monster class in setup_dragon()");
        break;
    }

    pbolt.range = 4;
    pbolt.rangeMax = 13;
    pbolt.damage = dice_def( 3, (monster->hit_dice * 2) );
    pbolt.damage.size = scaling * pbolt.damage.size / 100;
    pbolt.type = SYM_ZAP;
    pbolt.hit = 30;
    pbolt.beam_source = monster_index(monster);
    pbolt.thrower = KILL_MON;
    pbolt.is_beam = true;
}                               // end setup_dragon();

void setup_generic_throw(struct monsters *monster, struct bolt &pbolt)
{
    pbolt.range = 9;
    pbolt.rangeMax = 9;
    pbolt.beam_source = monster_index(monster);

    pbolt.type = SYM_MISSILE;
    pbolt.flavour = BEAM_MISSILE;
    pbolt.thrower = KILL_MON_MISSILE;
    pbolt.aux_source.clear();
    pbolt.is_beam = false;
}

// decide if something is launched or thrown
// pass -1 for launcher class & 0 for type if no weapon is wielded

void throw_type( int lnchClass, int lnchType, int wepClass, int wepType,
                 bool &launched, bool &thrown )
{
    if (wepClass == OBJ_MISSILES
        && lnchClass == OBJ_WEAPONS
        && is_range_weapon_type((weapon_type) lnchType)
	&& wepType == fires_ammo_type((weapon_type) lnchType))
    {
        launched = true;
    }

    if (wepClass == OBJ_WEAPONS)
    {
        if (wepType == WPN_DAGGER || wepType == WPN_HAND_AXE || wepType == WPN_SPEAR)
        {
            thrown = true;
        }
    }

    if (wepClass == OBJ_MISSILES)
    {
        if (wepType == MI_DART || wepType == MI_STONE || wepType == MI_LARGE_ROCK)
        {
            thrown = true;
        }
    }

    // launched overrides thrown
    if (launched == true)
        thrown = false;
}

bool mons_throw(struct monsters *monster, struct bolt &pbolt, int hand_used)
{
    // this was assumed during cleanup down below:
    ASSERT( hand_used == monster->inv[MSLOT_MISSILE] );

    // XXX: ugly hack, but avoids adding dynamic allocation to this code
    static char throw_buff[ ITEMNAME_SIZE ];

    int baseHit = 0, baseDam = 0;       // from thrown or ammo
    int ammoHitBonus = 0, ammoDamBonus = 0;     // from thrown or ammo
    int lnchHitBonus = 0, lnchDamBonus = 0;     // special add from launcher
    int exHitBonus = 0, exDamBonus = 0; // 'extra' bonus from skill/dex/str
    int lnchBaseDam = 0;

    int hitMult = 0;
    int damMult = 0;
    int diceMult = 100;

    bool launched = false;      // item is launched
    bool thrown = false;        // item is sensible thrown item

    // some initial convenience & initializations
    int wepClass = mitm[hand_used].base_type;
    int wepType  = mitm[hand_used].sub_type;

    int weapon = monster->inv[MSLOT_WEAPON];

    int lnchClass = (weapon != NON_ITEM) ? mitm[weapon].base_type : -1;
    int lnchType  = (weapon != NON_ITEM) ? mitm[weapon].sub_type  :  0;

    item_def item = mitm[hand_used];  // copy changed for venom launchers 
    item.quantity = 1;

    pbolt.range = 9;
    pbolt.beam_source = monster_index(monster);

    pbolt.type = SYM_MISSILE;
    pbolt.colour = item.colour;
    pbolt.flavour = BEAM_MISSILE;
    pbolt.thrower = KILL_MON_MISSILE;
    pbolt.aux_source.clear();

    // figure out if we're thrown or launched
    throw_type( lnchClass, lnchType, wepClass, wepType, launched, thrown );

    // extract launcher bonuses due to magic
    if (launched)
    {
        lnchHitBonus = mitm[ weapon ].plus;
        lnchDamBonus = mitm[ weapon ].plus2;
        lnchBaseDam  = property(mitm[weapon], PWPN_DAMAGE);
    }

    // extract weapon/ammo bonuses due to magic
    ammoHitBonus = item.plus;
    ammoDamBonus = item.plus2;

    if (thrown)
    {
        // Darts are easy.
        if (wepClass == OBJ_MISSILES && wepType == MI_DART)
        {
            baseHit = 5;
            hitMult = 40;
            damMult = 25;
        }
        else
        {
            baseHit = 0;
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
            baseDam      = div_rand_round(baseDam, 2);
        }

        // give monster "skill" bonuses based on HD
        exHitBonus = (hitMult * monster->hit_dice) / 10 + 1;
        exDamBonus = (damMult * monster->hit_dice) / 10 + 1;
    }

    if (launched)
    {
        switch (lnchType)
        {
        case WPN_BLOWGUN:
            baseHit = 2;
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
            baseHit = 1;
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

        // monsters no longer gain unfair advantages with weapons of fire/ice 
        // and incorrect ammo.  They now have same restriction as players.

        const int bow_brand = 
                get_weapon_brand(mitm[monster->inv[MSLOT_WEAPON]]);
        const int ammo_brand = get_ammo_brand( item );

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

        bool poison = (ammo_brand == SPMSL_POISONED 
                        || ammo_brand == SPMSL_POISONED_II);

        // POISON brand launchers poison ammo
        if (bow_brand == SPWPN_VENOM && ammo_brand == SPMSL_NORMAL)
            set_item_ego_type( item, OBJ_MISSILES, SPMSL_POISONED );

        // Vorpal brand increases damage dice size.
        if (bow_brand == SPWPN_VORPAL)
            diceMult = diceMult * 130 / 100;

        // WEAPON or AMMO of FIRE
        if ((bow_brand == SPWPN_FLAME || ammo_brand == SPMSL_FLAME)
            && bow_brand != SPWPN_FROST && ammo_brand != SPMSL_ICE)
        {
            baseHit += 2;
            exDamBonus += 6;

            pbolt.flavour = BEAM_FIRE;
            pbolt.name = "bolt of ";

            if (poison)
                pbolt.name += "poison ";

            pbolt.name += "flame";
            pbolt.colour = RED;
            pbolt.type = SYM_ZAP;
        }

        // WEAPON or AMMO of FROST
        if ((bow_brand == SPWPN_FROST || ammo_brand == SPMSL_ICE)
            && bow_brand != SPWPN_FLAME && ammo_brand != SPMSL_FLAME)
        {
            baseHit += 2;
            exDamBonus += 6;

            pbolt.flavour = BEAM_COLD;
            pbolt.name = "bolt of ";

            if (poison)
                pbolt.name += "poison ";

            pbolt.name += "frost";
            pbolt.colour = WHITE;
            pbolt.type = SYM_ZAP;
        }

        // Note: we already have 10 energy taken off.  -- bwr
        if (lnchType == WPN_CROSSBOW)
            monster->speed_increment += ((bow_brand == SPWPN_SPEED) ? 4 : -2);
        else if (bow_brand == SPWPN_SPEED)
            monster->speed_increment += 5;
    }

    // monster intelligence bonus
    if (mons_intel(monster->type) == I_HIGH)
        exHitBonus += 10;

    // now, if a monster is, for some reason, throwing something really
    // stupid, it will have baseHit of 0 and damage of 0.  Ah well.
    strcpy(info, ptr_monam( monster, DESC_CAP_THE) );

    strcat(info, (launched) ? " shoots " : " throws ");

    if (pbolt.name.length())
    {
        strcat(info, "a ");
        strcat(info, pbolt.name.c_str());
    }
    else
    {
        // build shoot message
        char str_pass[ ITEMNAME_SIZE ];
        item_name( item, DESC_NOCAP_A, str_pass );
        strcat(info, str_pass);

        // build beam name
        item_name( item, DESC_PLAIN, str_pass );
        pbolt.name = str_pass;
    }

    strcat(info, ".");
    mpr(info);

    // [dshaligram] When changing bolt names here, you must edit 
    // hiscores.cc (scorefile_entry::terse_missile_cause()) to match.
    if (launched) 
    {
        snprintf( throw_buff, sizeof(throw_buff), "Shot with a%s %s by %s",
                  (is_vowel(pbolt.name[0]) ? "n" : ""), pbolt.name.c_str(), 
                  ptr_monam( monster, DESC_NOCAP_A ) );
    }
    else
    {
        snprintf( throw_buff, sizeof(throw_buff), "Hit by a%s %s thrown by %s",
                  (is_vowel(pbolt.name[0]) ? "n" : ""), pbolt.name.c_str(), 
                  ptr_monam( monster, DESC_NOCAP_A ) );
    }

    pbolt.aux_source = throw_buff;

    // add everything up.
    pbolt.hit = baseHit + random2avg(exHitBonus, 2) + ammoHitBonus;
    pbolt.damage = dice_def( 1, baseDam + random2avg(exDamBonus, 2) + ammoDamBonus );

    if (launched)
    {
        pbolt.damage.size += lnchDamBonus;
        pbolt.hit += lnchHitBonus;
    }

    pbolt.damage.size = diceMult * pbolt.damage.size / 100;
    scale_dice(pbolt.damage);

    // decrease inventory
    fire_beam( pbolt, &item );

    if (dec_mitm_item_quantity( hand_used, 1 ))
        monster->inv[MSLOT_MISSILE] = NON_ITEM;


    return (true);
}                               // end mons_throw()

// should really do something about mons_hit, but can't be bothered
void spore_goes_pop(struct monsters *monster)
{
    struct bolt beam;
    int type = monster->type;

    if (monster == NULL)
        return;

    beam.is_tracer = false;
    beam.is_explosion = true;
    beam.beam_source = monster_index(monster);
    beam.type = SYM_BURST;
    beam.target_x = monster->x;
    beam.target_y = monster->y;
    beam.thrower = KILL_MON;    // someone else's explosion
    beam.aux_source.clear();

    if (type == MONS_GIANT_SPORE)
    {
        beam.flavour = BEAM_SPORE;
        beam.name = "explosion of spores";
        beam.colour = LIGHTGREY;
        beam.damage = dice_def( 3, 15 );
        beam.ex_size = 2;
        strcpy( info, "The giant spore explodes!" );
    }
    else
    {
        beam.flavour = BEAM_ELECTRICITY;
        beam.name = "blast of lightning";
        beam.colour = LIGHTCYAN;
        beam.damage = dice_def( 3, 20 );
        beam.ex_size = coinflip() ? 3 : 2;
        strcpy( info, "The ball lightning explodes!" );
    }

    if (mons_near(monster))
    {
        viewwindow(1, false);
        mpr( info );
    }

    explosion(beam);
}                               // end spore_goes_pop()

bolt mons_spells( int spell_cast, int power )
{
    ASSERT(power > 0);

    bolt beam;

    beam.name = "****";         // initialize to some bogus values so we can catch problems
    beam.colour = 1000;
    beam.range = beam.rangeMax = 8;
    beam.hit = -1;
    beam.damage = dice_def( 1, 0 );
    beam.ench_power = -1;
    beam.type = -1;
    beam.flavour = -1;
    beam.thrower = -1;
    beam.is_beam = false;
    beam.is_explosion = false;

    switch (spell_cast)
    {
    case MS_MMISSILE:
        beam.colour = LIGHTMAGENTA;     //inv_colour [throw_2];
        beam.name = "magic dart";       // inv_name [throw_2]);
        beam.range = 6;
        beam.rangeMax = 10;
        beam.damage = dice_def( 3, 4 + (power / 100) );
        beam.hit = 1500;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON_MISSILE;
        beam.flavour = BEAM_MMISSILE;
        beam.is_beam = false;
        break;

    case MS_FLAME:
        beam.colour = RED;
        beam.name = "puff of flame";
        beam.range = 6;
        beam.rangeMax = 10;

        // should this be the same as magic missile?
        // No... magic missile is special in that it has a really
        // high to-hit value, so these should do more damage -- bwr
        beam.damage = dice_def( 3, 5 + (power / 40) );

        beam.hit = 60;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON_MISSILE;
        beam.flavour = BEAM_FIRE;
        beam.is_beam = false;
        break;

    case MS_FROST:
        beam.colour = WHITE;
        beam.name = "puff of frost";
        beam.range = 6;
        beam.rangeMax = 10;

        // should this be the same as magic missile?
        // see MS_FLAME -- bwr
        beam.damage = dice_def( 3, 5 + (power / 40) );

        beam.hit = 60;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON_MISSILE;
        beam.flavour = BEAM_COLD;
        beam.is_beam = false;
        break;

    case MS_PARALYSIS:
        beam.name = "0";
        beam.range = 5;
        beam.rangeMax = 9;
        beam.type = 0;
        beam.flavour = BEAM_PARALYSIS;
        beam.thrower = KILL_MON_MISSILE;
        beam.is_beam = true;
        break;

    case MS_SLOW:
        beam.name = "0";
        beam.range = 5;
        beam.rangeMax = 9;
        beam.type = 0;
        beam.flavour = BEAM_SLOW;
        beam.thrower = KILL_MON_MISSILE;
        beam.is_beam = true;
        break;

    case MS_HASTE:              // (self)
        beam.name = "0";
        beam.range = 5;
        beam.rangeMax = 9;
        beam.type = 0;
        beam.flavour = BEAM_HASTE;
        beam.thrower = KILL_MON_MISSILE;
        beam.is_beam = true;
        break;

    case MS_CONFUSE:
        beam.name = "0";
        beam.range = 5;
        beam.rangeMax = 9;
        beam.type = 0;
        beam.flavour = BEAM_CONFUSION;
        beam.thrower = KILL_MON_MISSILE;
        beam.is_beam = true;
        break;

    case MS_VENOM_BOLT:
        beam.name = "bolt of poison";
        beam.range = 7;
        beam.rangeMax = 16;
        beam.damage = dice_def( 3, 6 + power / 13 );
        beam.colour = LIGHTGREEN;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON;
        beam.flavour = BEAM_POISON;
        beam.hit = 8 + power / 20;
        beam.is_beam = true;
        break;

    case MS_POISON_ARROW:
        beam.name = "poison arrow";
        beam.damage = dice_def( 4, 5 + power / 12 );
        beam.colour = LIGHTGREEN;
        beam.type = SYM_MISSILE;
        beam.thrower = KILL_MON;
        beam.flavour = BEAM_POISON_ARROW;
        beam.hit = 14 + power / 25;
        beam.range = beam.rangeMax = 8;
        break;

    case MS_FIRE_BOLT:
        beam.name = "bolt of fire";
        beam.range = 4;
        beam.rangeMax = 13;
        beam.damage = dice_def( 3, 8 + power / 11 );
        beam.colour = RED;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON;
        beam.flavour = BEAM_FIRE;
        beam.hit = 12 + power / 25;
        beam.is_beam = true;
        break;

    case MS_COLD_BOLT:
        beam.name = "bolt of cold";
        beam.range = 4;
        beam.rangeMax = 13;
        beam.damage = dice_def( 3, 8 + power / 11 );
        beam.colour = WHITE;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON;
        beam.flavour = BEAM_COLD;
        beam.hit = 12 + power / 25;
        beam.is_beam = true;
        break;

    case MS_LIGHTNING_BOLT:
        beam.name = "bolt of lightning";
        beam.range = 7;
        beam.rangeMax = 16;
        beam.damage = dice_def( 3, 10 + power / 9 );
        beam.colour = LIGHTCYAN;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON;
        beam.flavour = BEAM_ELECTRICITY;
        beam.hit = 10 + power / 40;
        beam.is_beam = true;
        break;

    case MS_INVIS:
        beam.name = "0";
        beam.range = 5;
        beam.rangeMax = 9;
        beam.type = 0;
        beam.flavour = BEAM_INVISIBILITY;
        beam.thrower = KILL_MON;
        beam.is_beam = true;
        break;

    case MS_FIREBALL:
        beam.colour = RED;
        beam.name = "fireball";
        beam.range = 6;
        beam.rangeMax = 10;
        beam.damage = dice_def( 3, 7 + power / 10 );
        beam.hit = 40;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON;
        beam.flavour = BEAM_FIRE;  // why not BEAM_FIRE? {dlb}
        beam.is_beam = false;
        beam.is_explosion = true;
        break;

    case MS_HEAL:
        beam.name = "0";
        beam.range = 5;
        beam.rangeMax = 9;
        beam.type = 0;
        beam.flavour = BEAM_HEALING;
        beam.thrower = KILL_MON;
        beam.hit = 5 + (power / 5);
        beam.is_beam = true;
        break;

    case MS_TELEPORT:
        beam.name = "0";
        beam.range = 5;
        beam.rangeMax = 9;
        beam.type = 0;
        beam.flavour = BEAM_TELEPORT;        // 6 is used by digging
        beam.thrower = KILL_MON;
        beam.is_beam = true;
        break;

    case MS_TELEPORT_OTHER:
        beam.name = "0";
        beam.range = 5;
        beam.rangeMax = 9;
        beam.type = 0;
        beam.flavour = BEAM_TELEPORT;        // 6 is used by digging
        beam.thrower = KILL_MON;
        beam.is_beam = true;
        break;

    case MS_BLINK:
        beam.is_beam = false;
        break;

    case MS_CRYSTAL_SPEAR:      // was splinters
        beam.name = "crystal spear";
        beam.range = 7;
        beam.rangeMax = 16;
        beam.damage = dice_def( 3, 12 + power / 10 );
        beam.colour = WHITE;
        beam.type = SYM_MISSILE;
        beam.thrower = KILL_MON;
        beam.flavour = BEAM_MMISSILE;
        beam.hit = 14 + power / 20;
        beam.is_beam = false;
        break;

    case MS_DIG:
        beam.name = "0";
        beam.range = 3;
        beam.rangeMax = 7 + random2(power) / 10;
        beam.type = 0;
        beam.flavour = BEAM_DIGGING;
        beam.thrower = KILL_MON;
        beam.is_beam = true;
        break;

    case MS_NEGATIVE_BOLT:      // negative energy
        beam.name = "bolt of negative energy";
        beam.range = 7;
        beam.rangeMax = 16;
        beam.damage = dice_def( 3, 6 + power / 13 );
        beam.colour = DARKGREY;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON;
        beam.flavour = BEAM_NEG;
        beam.hit = 11 + power / 35;
        beam.is_beam = true;
        break;

    // 20, 21 are used

    case MS_ORB_ENERGY: // mystic blast
        beam.colour = LIGHTMAGENTA;
        beam.name = "orb of energy";
        beam.range = 6;
        beam.rangeMax = 10;
        beam.damage = dice_def( 3, 7 + (power / 14) );
        beam.hit = 12 + (power / 20);
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON_MISSILE;
        beam.flavour = BEAM_MMISSILE;
        beam.is_beam = false;
        break;

    // 23 is brain feed

    case MS_STEAM_BALL:
        beam.colour = LIGHTGREY;
        beam.name = "ball of steam";
        beam.range = 6;
        beam.rangeMax = 10;
        beam.damage = dice_def( 3, 7 + (power / 15) );
        beam.hit = 11 + power / 20;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON_MISSILE;
        beam.flavour = BEAM_STEAM;
        beam.is_beam = false;
        break;

    // 27 is summon devils
    // 28 is animate dead

    case MS_PAIN:
        beam.name = "0";
        beam.range = 7;
        beam.rangeMax = 14;
        beam.type = 0;
        beam.flavour = BEAM_PAIN;     // pain
        beam.thrower = KILL_MON;
        // beam.damage = dice_def( 1, 50 );
        beam.damage = dice_def( 1, 7 + (power / 20) );
        beam.ench_power = 50;
        beam.is_beam = true;
        break;

    // 30 is smiting

    case MS_STICKY_FLAME:
        beam.colour = RED;
        beam.name = "sticky flame";
        beam.range = 6;
        beam.rangeMax = 10;
        beam.damage = dice_def( 3, 3 + power / 50 );
        beam.hit = 10 + power / 15;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON_MISSILE;
        beam.flavour = BEAM_FIRE;
        beam.is_beam = false;
        break;

    case MS_POISON_BLAST:       // demon
        beam.name = "blast of poison";
        beam.range = 7;
        beam.rangeMax = 16;
        beam.damage = dice_def( 3, 3 + power / 25 );
        beam.colour = LIGHTGREEN;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON;
        beam.flavour = BEAM_POISON;
        beam.hit = 11 + power / 25;
        beam.is_beam = true;
        beam.is_big_cloud = true;
        break;

    case MS_PURPLE_BLAST:       // purple bang thing
        beam.colour = LIGHTMAGENTA;
        beam.name = "orb of energy";
        beam.range = 6;
        beam.rangeMax = 10;
        beam.damage = dice_def( 3, 10 + power / 15 );
        beam.hit = 11 + power / 25;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON_MISSILE;
        beam.flavour = BEAM_MMISSILE;
        beam.is_beam = false;
        beam.is_explosion = true;
        break;

    case MS_ENERGY_BOLT:        // eye of devastation
        beam.colour = YELLOW;
        beam.name = "bolt of energy";
        beam.range = 9;
        beam.rangeMax = 23;
        beam.damage = dice_def( 3, 20 );
        beam.hit = 11 + power / 30;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON_MISSILE;
        beam.flavour = BEAM_NUKE;     // a magical missile which destroys walls
        beam.is_beam = true;
        break;

    case MS_STING:              // sting
        beam.colour = GREEN;
        beam.name = "sting";
        beam.range = 8;
        beam.rangeMax = 12;
        beam.damage = dice_def( 1, 6 + power / 25 );
        beam.hit = 60;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON_MISSILE;
        beam.flavour = BEAM_POISON;
        beam.is_beam = false;
        break;

    case MS_IRON_BOLT:
        beam.colour = LIGHTCYAN;
        beam.name = "iron bolt";
        beam.range = 4;
        beam.rangeMax = 8;
        beam.damage = dice_def( 3, 8 + (power / 9) );
        beam.hit = 12 + (power / 25);
        beam.type = SYM_MISSILE;
        beam.thrower = KILL_MON_MISSILE;
        beam.flavour = BEAM_MMISSILE;   // similarly unresisted thing
        beam.is_beam = false;
        break;

    case MS_STONE_ARROW:
        beam.colour = LIGHTGREY;
        beam.name = "stone arrow";
        beam.range = 8;
        beam.rangeMax = 12;
        beam.damage = dice_def( 3, 5 + (power / 10) );
        beam.hit = 8 + power / 35;
        beam.type = SYM_MISSILE;
        beam.thrower = KILL_MON_MISSILE;
        beam.flavour = BEAM_MMISSILE;   // similarly unresisted thing
        beam.is_beam = false;
        break;

    case MS_POISON_SPLASH:
        beam.colour = GREEN;
        beam.name = "splash of poison";
        beam.range = 5;
        beam.rangeMax = 10;
        beam.damage = dice_def( 1, 4 + power / 10 );
        beam.hit = 9 + power / 20;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON_MISSILE;
        beam.flavour = BEAM_POISON;
        beam.is_beam = false;
        break;

    case MS_DISINTEGRATE:
        beam.name = "0";
        beam.range = 7;
        beam.rangeMax = 14;
        beam.type = 0;
        beam.flavour = BEAM_DISINTEGRATION;
        beam.thrower = KILL_MON;
        beam.ench_power = 50;
        // beam.hit = 30 + (power / 10);
        beam.damage = dice_def( 1, 30 + (power / 10) );
        beam.is_beam = true;
        break;

    case MS_MARSH_GAS:          // swamp drake
        beam.name = "foul vapour";
        beam.range = 7;
        beam.rangeMax = 16;
        beam.damage = dice_def( 3, 2 + power / 25 );
        beam.colour = GREEN;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON;
        beam.flavour = BEAM_POISON;
        beam.hit = 12 + power / 30;
        beam.is_beam = true;
        beam.is_big_cloud = true;
        break;

    case MS_MIASMA:            // death drake
        beam.name = "foul vapour";
        beam.damage = dice_def( 3, 5 + power / 24 );
        beam.colour = DARKGREY;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON;
        beam.flavour = BEAM_MIASMA;
        beam.hit = 12 + power / 20;
        beam.is_beam = true;
        beam.is_big_cloud = true;
        beam.range = beam.rangeMax = 8;
        break;
        
    case MS_QUICKSILVER_BOLT:   // Quicksilver dragon
        beam.colour = random_colour();
        beam.name = "bolt of energy";
        beam.range = 9;
        beam.rangeMax = 23;
        beam.damage = dice_def( 3, 25 );
        beam.hit = 9 + power / 25;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON_MISSILE;
        beam.flavour = BEAM_MMISSILE;
        beam.is_beam = false;
        break;

    case MS_HELLFIRE:           // fiend's hellfire
        beam.name = "hellfire";
        beam.colour = RED;
        beam.range = 4;
        beam.rangeMax = 13;
        beam.damage = dice_def( 3, 25 );
        beam.hit = 20;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON;
        beam.flavour = BEAM_HELLFIRE;
        beam.is_beam = true;
        beam.is_explosion = true;
        break;

    case MS_METAL_SPLINTERS:
        beam.name = "spray of metal splinters";
        beam.range = 7;
        beam.rangeMax = 16;
        beam.damage = dice_def( 3, 20 + power / 20 );
        beam.colour = CYAN;
        beam.type = SYM_ZAP;
        beam.thrower = KILL_MON;
        beam.flavour = BEAM_FRAG;
        beam.hit = 15 + power / 30;
        beam.is_beam = true;
        break;

    case MS_BANISHMENT:
        beam.name = "0";
        beam.range = 5;
        beam.rangeMax = 9;
        beam.type = 0;
        beam.flavour = BEAM_BANISH;
        beam.thrower = KILL_MON_MISSILE;
        beam.is_beam = true;
        break;

    case MS_BLINK_OTHER:
        beam.name = "0";
        beam.type = 0;
        beam.flavour = BEAM_BLINK;
        beam.thrower = KILL_MON;
        beam.is_beam = true;
        beam.is_enchant = true;
        beam.range = 8;
        beam.rangeMax = 8;
        break;

    default:
        DEBUGSTR("Unknown spell");
    }

    return (beam);
}                               // end mons_spells()

static unsigned char monster_abjuration(int pow, bool test)
{

    unsigned char result = 0;
    struct monsters *monster = 0;       // NULL {dlb}

    if (!test)
        mpr("Send 'em back where they came from!");

    for (int ab = 0; ab < MAX_MONSTERS; ab++)
    {
        int abjLevel;

        monster = &menv[ab];

        if (monster->type == -1 || !mons_near(monster))
            continue;

        if (!mons_friendly(monster))
            continue;

        abjLevel = mons_has_ench(monster, ENCH_ABJ_I, ENCH_ABJ_VI);
        if (abjLevel == ENCH_NONE)
            continue;

        result++;

        if (test)
            continue;

        if (pow > 60)
            pow = 60;

        abjLevel -= 1 + (random2(pow) / 3);

        if (abjLevel < ENCH_ABJ_I)
            monster_die(monster, KILL_RESET, 0);
        else
        {
            simple_monster_message(monster, " shudders.");
            mons_del_ench(monster, ENCH_ABJ_I, ENCH_ABJ_VI);
            mons_add_ench(monster, abjLevel);
        }
    }

    return result;
}                               // end monster_abjuration()

bool silver_statue_effects(monsters *mons)
{
    if ((mons_player_visible(mons) || one_chance_in(3))
            && !one_chance_in(3))
    {
        char wc[30];

        weird_colours( random2(256), wc );
        snprintf(info, INFO_SIZE, "'s eyes glow %s.", wc);
        simple_monster_message(mons, info, MSGCH_WARN);

        create_monster( summon_any_demon((coinflip() ? DEMON_COMMON
                                                     : DEMON_LESSER)),
                                 ENCH_ABJ_V, BEH_HOSTILE,
                                 you.x_pos, you.y_pos,
                                 MHITYOU, 250 );
        return (true);
    }
    return (false);
}

bool orange_statue_effects(monsters *mons)
{
    if ((mons_player_visible(mons) || one_chance_in(3))
            && !one_chance_in(3))
    {
        mpr("A hostile presence attacks your mind!", MSGCH_WARN);

        miscast_effect( SPTYP_DIVINATION, random2(15), random2(150), 100,
                        "an orange crystal statue" );
        return (true);
    }

    return (false);
}
