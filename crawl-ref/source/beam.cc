/*
 *  File:       beam.cc
 *  Summary:    Functions related to ranged attacks.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *   <7>    21mar2001    GDL    Replaced all FP arithmetic with integer*100 math
 *   <6>    07jan2001    GDL    complete rewrite.
 *   <5>    22July2000   GDL    allowed 'dummy' missiles from monsters
 *   <4>    11/14/99     cdl    evade beams with random40(ev) vice random2(ev)
 *                              all armour now protects against shrapnel
 *   <3>     6/ 2/99     DML    Added enums
 *   <2>     5/20/99     BWR    Added refreshs for curses
 *   <1>     -/--/--     LRH    Created
 */

#include "AppHdr.h"
#include "beam.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <iostream>
#include <set>

#ifdef DOS
#include <dos.h>
#include <conio.h>
#endif

#include "externs.h"

#include "cio.h"
#include "cloud.h"
#include "effects.h"
#include "enum.h"
#include "it_use2.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mstuff2.h"
#include "mutation.h"
#include "ouch.h"
#include "player.h"
#include "religion.h"
#include "skills.h"
#include "spells1.h"
#include "spells3.h"
#include "spells4.h"
#include "stuff.h"
#include "terrain.h"
#include "traps.h"
#include "view.h"
#include "xom.h"

#define BEAM_STOP       1000        // all beams stopped by subtracting this
                                    // from remaining range
#define MON_RESIST      0           // monster resisted
#define MON_UNAFFECTED  1           // monster unaffected
#define MON_AFFECTED    2           // monster was unaffected

static int spreadx[] = { 0, 0, 1, -1 };
static int spready[] = { -1, 1, 0, 0 };
static int opdir[]   = { 2, 1, 4, 3 };
static FixedArray < bool, 19, 19 > explode_map;

// helper functions (some of these should probably be public):
static void sticky_flame_monster( int mn, kill_category who, int hurt_final );
static bool affects_wall(const bolt &beam, int wall_feature);
static bool isBouncy(bolt &beam, unsigned char gridtype);
static int beam_source(const bolt &beam);
static std::string beam_zapper(const bolt &beam);
static bool beam_term_on_target(bolt &beam, int x, int y);
static void beam_explodes(bolt &beam, int x, int y);
static int  affect_wall(bolt &beam, int x, int y);
static int  affect_place_clouds(bolt &beam, int x, int y);
static void affect_place_explosion_clouds(bolt &beam, int x, int y);
static int  affect_player(bolt &beam);
static void affect_items(bolt &beam, int x, int y);
static int  affect_monster(bolt &beam, monsters *mon);
static int  affect_monster_enchantment(bolt &beam, monsters *mon);
static void beam_paralyses_monster( bolt &pbolt, monsters *monster );
static int  range_used_on_hit(bolt &beam);
static void explosion1(bolt &pbolt);
static void explosion_map(bolt &beam, int x, int y,
    int count, int dir, int r);
static void explosion_cell(bolt &beam, int x, int y, bool drawOnly);

static void ench_animation( int flavour, const monsters *mon = NULL, bool force = false);
static void zappy(zap_type z_type, int power, bolt &pbolt);
static void monster_die(monsters *mons, const bolt &beam);

static std::set<std::string> beam_message_cache;

static bool beam_is_blockable( bolt &pbolt )
{
    // BEAM_ELECTRICITY is added here because chain lighting is not 
    // a true beam (stops at the first target it gets to and redirects
    // from there)... but we don't want it shield blockable.
    return (!pbolt.is_beam && !pbolt.is_explosion 
            && pbolt.flavour != BEAM_ELECTRICITY);
}

// Kludge to suppress multiple redundant messages for a single beam.
static void beam_mpr(msg_channel_type channel, const char *s, ...)
{
    va_list args;
    va_start(args, s);

    char buf[500];
    vsnprintf(buf, sizeof buf, s, args);

    va_end(args);

    std::string message = buf;
    if (beam_message_cache.find(message) == beam_message_cache.end())
        mpr(message.c_str(), channel);

    beam_message_cache.insert( message );
}

static void monster_die(monsters *mons, const bolt &beam)
{
    monster_die(mons, beam.killer(), beam.beam_source);
}

static kill_category whose_kill(const bolt &beam)
{
    if (YOU_KILL(beam.thrower))
        return (KC_YOU);
    else if (MON_KILL(beam.thrower))
    {
        if (beam.beam_source >= 0 && beam.beam_source < MAX_MONSTERS)
        {
            const monsters *mons = &menv[beam.beam_source];
            if (mons_friendly(mons))
                return (KC_FRIENDLY);
        }
    }
    return (KC_OTHER);
}

// simple animated flash from Rupert Smith (and expanded to be more generic):
void zap_animation( int colour, const monsters *mon, bool force )
{
    int x = you.x_pos, y = you.y_pos;

    // default to whatever colour magic is today
    if (colour == -1)
        colour = element_colour( EC_MAGIC );

    if (mon)
    {
        if (!force && !player_monster_visible( mon ))
            return;

        x = mon->x;
        y = mon->y;
    }

    if (!see_grid( x, y ))
        return;
    
    const int drawx = grid2viewX(x);
    const int drawy = grid2viewY(y);

    if (in_los_bounds(drawx, drawy))
    {
        view_update();
        textcolor( colour );
        gotoxy( drawx, drawy );
        putch( SYM_ZAP );
        update_screen();
        delay(50);
    }
}

// special front function for zap_animation to interpret enchantment flavours
static void ench_animation( int flavour, const monsters *mon, bool force )
{
    const int elem = (flavour == BEAM_HEALING)       ? EC_HEAL :
                     (flavour == BEAM_PAIN)          ? EC_UNHOLY :
                     (flavour == BEAM_DISPEL_UNDEAD) ? EC_HOLY :
                     (flavour == BEAM_POLYMORPH)     ? EC_MUTAGENIC :
                     (flavour == BEAM_TELEPORT
                          || flavour == BEAM_BANISH
                          || flavour == BEAM_BLINK)  ? EC_WARP 
                                                     : EC_ENCHANT;
    zap_animation( element_colour( elem ), mon, force );
}

void zapping(zap_type ztype, int power, bolt &pbolt)
{

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "zapping: power=%d", power ); 
#endif

    // GDL: note that rangeMax is set to 0, which means that max range is
    // equal to range.  This is OK, since rangeMax really only matters for
    // stuff monsters throw/zap.

    // all of the following might be changed by zappy():
    pbolt.range = 8 + random2(5);       // default for "0" beams (I think)
    pbolt.rangeMax = 0;
    pbolt.hit = 0;                      // default for "0" beams (I think)
    pbolt.damage = dice_def( 1, 0 );    // default for "0" beams (I think)
    pbolt.type = 0;                     // default for "0" beams
    pbolt.flavour = BEAM_MAGIC;         // default for "0" beams
    pbolt.ench_power = power;
    pbolt.obvious_effect = false;
    pbolt.is_beam = false;               // default for all beams.
    pbolt.is_tracer = false;             // default for all player beams
    pbolt.thrower = KILL_YOU_MISSILE;   // missile from player
    pbolt.aux_source.clear();            // additional source info, unused

    // fill in the bolt structure
    zappy( ztype, power, pbolt );

    if (ztype == ZAP_LIGHTNING && !silenced(you.x_pos, you.y_pos))
        // needs to check silenced at other location, too {dlb}
    {
        mpr("You hear a mighty clap of thunder!");
        noisy( 25, you.x_pos, you.y_pos );
    }

    fire_beam(pbolt);

    return;
}                               // end zapping()

dice_def calc_dice( int num_dice, int max_damage )
{
    dice_def    ret( num_dice, 0 );

    if (num_dice <= 1)
    {
        ret.num  = 1;
        ret.size = max_damage;
    }
    else if (max_damage <= num_dice)
    {
        ret.num  = max_damage;
        ret.size = 1;
    }
    else
    {
        // Divied the damage among the dice, and add one 
        // occasionally to make up for the fractions. -- bwr
        ret.size = max_damage / num_dice;
        ret.size += (random2( num_dice ) < max_damage % num_dice);
    }

    return (ret);
}

// *do not* call this function directly (duh - it's static), need to
// see zapping() for default values not set within this function {dlb}
static void zappy( zap_type z_type, int power, bolt &pbolt )
{
    int temp_rand = 0;          // probability determination {dlb}

    // Note: The incoming power is not linear in the case of spellcasting.
    // The power curve currently allows for the character to reasonably 
    // get up to a power level of about a 100, but more than that will
    // be very hard (and the maximum is 200).  The low level power caps
    // provide the useful feature in that they allow for low level spells
    // to have quick advancement, but don't cause them to obsolete the 
    // higher level spells. -- bwr
    //
    // I've added some example characters below to show how little 
    // people should be concerned about the power caps.  
    //
    // The example characters are simplified to three stats:
    //
    // - Intelligence: This magnifies power, it's very useful.
    //
    // - Skills: This represents the character having Spellcasting
    //   and the average of the component skills at this level.
    //   Although, Spellcasting probably isn't quite as high as
    //   other spell skills for a lot of characters, note that it 
    //   contributes much less to the total power (about 20%).
    //
    // - Enhancers:  These are equipment that the player can use to
    //   apply additional magnifiers (x1.5) to power.  There are
    //   also inhibitors that reduce power (/2.0), but we're not
    //   concerned about those here.  Anyways, the character can 
    //   currently have up to 3 levels (for x1.5, x2.25, x3.375).
    //   The lists below should help to point out the difficulty
    //   and cost of getting more than one level of enhancement.
    //
    //   Here's a list of current magnifiers:
    //   
    //   - rings of fire/cold
    //   - staff of fire/cold/air/earth/poison/death/conjure/enchant/summon
    //   - staff of Olgreb (poison)
    //   - robe of the Archmagi (necro, conjure, enchant, summon)
    //   - Mummy intrinsic (+1 necromancy at level 13, +2 at level 26)
    //   - Necromutation (+1 to necromancy -- note: undead can't use this)
    //   - Ring of Fire (+1 to fire)
    //
    //   The maximum enhancement, by school (but capped at 3):
    //
    //   - Necromancy:  4 (Mummies), 3 (others)
    //   - Fire:        4 
    //   - Cold:        3
    //   - Conjuration: 2
    //   - Enchantment: 2
    //   - Summoning:   2
    //   - Air:         1
    //   - Earth:       1
    //   - Poison:      1
    //   - Translocations, Transmigrations, Divinations intentionally 0

    switch (z_type)
    {
    // level 1
    // 
    // This cap is to keep these easy and very cheap spells from 
    // becoming too powerful. 
    //
    // Example characters with about 25 power:
    //
    // - int  5, skills 20, 0 enhancers
    // - int  5, skills 14, 1 enhancer
    // - int 10, skills 10, 0 enhancers
    // - int 10, skills  7, 1 enhancers
    // - int 15, skills  7, 0 enhancers
    // - int 20, skills  6, 0 enhancers
    case ZAP_STRIKING:
    case ZAP_MAGIC_DARTS:
    case ZAP_STING:
    case ZAP_ELECTRICITY:
    case ZAP_FLAME_TONGUE:
    case ZAP_SMALL_SANDBLAST:
    case ZAP_DISRUPTION:                // ench_power boosted below
    case ZAP_PAIN:                      // ench_power boosted below
        if (power > 25)
            power = 25;
        break;

    // level 2/3
    //
    // The following examples should make it clear that in the 
    // early game this cap is only limiting to serious spellcasters 
    // (they could easily reach the 20-10-0 example).  
    //
    // Example characters with about 50 power:
    //
    // - int 10, skills 20, 0 enhancers
    // - int 10, skills 14, 1 enhancer
    // - int 15, skills 14, 0 enhancers
    // - int 15, skills 10, 1 enhancer
    // - int 20, skills 10, 0 enhancers
    // - int 20, skills  7, 1 enhancer
    // - int 25, skills  8, 0 enhancers
    case ZAP_SANDBLAST:
    case ZAP_FLAME:             // also ability (pow = lev * 2)
    case ZAP_FROST:             // also ability (pow = lev * 2)
    case ZAP_STONE_ARROW:
        if (power > 50)
            power = 50;
        break;

    // Here are some examples that show that its fairly safe to assume
    // that a high level character can easily have 75 power.
    //
    // Example characters with about 75 power:
    // 
    // - int 10, skills 27, 1 enhancer
    // - int 15, skills 27, 0 enhancers
    // - int 15, skills 16, 1 enhancer
    // - int 20, skills 20, 0 enhancers
    // - int 20, skills 14, 1 enhancer
    // - int 25, skills 16, 0 enhancers

    // level 4
    //
    // The following examples should make it clear that this is the 
    // effective maximum power.  Its not easy to get to 100 power, 
    // but 20-20-1 or 25-16-1 is certainly attainable by a high level 
    // spellcaster.  As you can see from the examples at 150 and 200,
    // getting much power beyond this is very difficult.
    //
    // Level 3 and 4 spells cannot be overpowered.
    //
    // Example characters with about 100 power:
    //
    // - int 10, skills 27, 2 enhancers
    // - int 15, skills 27, 1 enhancer
    // - int 20, skills 20, 1 enhancer
    // - int 25, skills 24, 0 enhancers
    // - int 25, skills 16, 1 enhancer
    case ZAP_MYSTIC_BLAST:
    case ZAP_STICKY_FLAME:
    case ZAP_ICE_BOLT:
    case ZAP_DISPEL_UNDEAD:     // ench_power raised below
        if (power > 100)
            power = 100;
        break;

    // levels 5-7
    //
    // These spells used to be capped, but its very hard to raise
    // power over 100, and these examples should show that. 
    // Only the twinkiest of characters are expected to get to 150.
    // 
    // Example characters with about 150 power:
    //
    // - int 15, skills 27, 3 enhancers (actually, only 146)
    // - int 20, skills 27, 2 enhancers (actually, only 137)
    // - int 20, skills 21, 3 enhancers 
    // - int 25, skills 26, 2 enhancers
    // - int 30, skills 21, 2 enhancers
    // - int 40, skills 24, 1 enhancer
    // - int 70, skills 20, 0 enhancers
    case ZAP_FIRE:
    case ZAP_COLD:
    case ZAP_VENOM_BOLT:
    case ZAP_MAGMA:
    case ZAP_AGONY:
    case ZAP_LIGHTNING:                 // also invoc * 6 or lev * 2 (abils)
    case ZAP_NEGATIVE_ENERGY:           // also ability (pow = lev * 6)
    case ZAP_IRON_BOLT:
    case ZAP_DISINTEGRATION:
    case ZAP_FIREBALL:
    case ZAP_ORB_OF_ELECTRICITY:
    case ZAP_ORB_OF_FRAGMENTATION:
    case ZAP_POISON_ARROW:
        // if (power > 150)
        //     power = 150;
        break;

    // levels 8-9
    //
    // These spells are capped at 200 (which is the cap in calc_spell_power).
    // As an example of how little of a cap that is, consider the fact 
    // that a 70-27-3 character has an uncapped power of 251.  Characters
    // are never expected to get to this cap.
    //
    // Example characters with about 200 power:
    //
    // - int 30, skills 27, 3 enhancers (actually, only 190)
    // - int 40, skills 27, 2 enhancers (actually, only 181)
    // - int 40, skills 23, 3 enhancers
    // - int 70, skills 27, 0 enhancers (actually, only 164)
    // - int 70, skills 27, 1 enhancers (actually, only 194)
    // - int 70, skills 20, 2 enhancers
    // - int 70, skills 13, 3 enhancers
    case ZAP_CRYSTAL_SPEAR:
    case ZAP_HELLFIRE:
    case ZAP_ICE_STORM:
    case ZAP_CLEANSING_FLAME:
        // if (power > 200)
        //     power = 200;
        break;

    // unlimited power (needs a good reason)
    case ZAP_BONE_SHARDS:    // incoming power is modified for mass
    case ZAP_BEAM_OF_ENERGY: // inaccuracy (only on staff, hardly hits)
        break;

    // natural/mutant breath/spit powers (power ~= characer level)
    case ZAP_SPIT_POISON:               // lev + mut * 5
    case ZAP_BREATHE_FIRE:              // lev + mut * 4 + 12 (if dragonform)
    case ZAP_BREATHE_FROST:             // lev
    case ZAP_BREATHE_ACID:              // lev (or invoc * 3 from minor destr)
    case ZAP_BREATHE_POISON:            // lev
    case ZAP_BREATHE_POWER:             // lev
    case ZAP_BREATHE_STEAM:             // lev
        if (power > 50)
            power = 50;
        break;

    // enchantments and other resistable effects
    case ZAP_SLOWING:
    case ZAP_HASTING:
    case ZAP_PARALYSIS:
    case ZAP_BACKLIGHT:
    case ZAP_SLEEP:
    case ZAP_CONFUSION:
    case ZAP_INVISIBILITY:
    case ZAP_ENSLAVEMENT:
    case ZAP_TELEPORTATION:
    case ZAP_DIGGING:
    case ZAP_POLYMORPH_OTHER:
    case ZAP_DEGENERATION:
    case ZAP_BANISHMENT:
        // This is the only power that matters.  We magnify it apparently
        // to get values that work better with magic resistance checks...
        // those checks will scale down this value and max it out at 120.
        pbolt.ench_power *= 3;
        pbolt.ench_power /= 2;
        break;

    // anything else we cap to 100
    default:
        if (power > 100)
            power = 100;
        break;
    }

    // Note:  I'm only displaying the top damage and such here, that's
    // because it's really not been known before (since the above caps
    // didn't exist), so they were all pretty much unlimited before.
    // Also note, that the high end damage occurs at the cap, only
    // players that are that powerful can get that damage... and
    // although these numbers might seem small, you should remember
    // that Dragons in this game are 60-90 hp monsters, and very
    // few monsters have more than 100 hp (and that 1d5 damage is
    // still capable of taking a good sized chunk (and possibly killing)
    // any monster you're likely to meet in the first three levels). -- bwr

    // Note: damage > 100 signals that "random2(damage - 100)" will be
    // applied three times, which not only ups the damage but gives
    // a more normal distribution.
    switch (z_type)
    {
    case ZAP_STRIKING:                                  // cap 25
        pbolt.name = "force bolt";
        pbolt.colour = BLACK;
        pbolt.range = 8 + random2(5);
        pbolt.damage = dice_def( 1, 5 );                // dam: 5
        pbolt.hit = 8 + power / 10;                     // 25: 10
        pbolt.type = SYM_SPACE; 
        pbolt.flavour = BEAM_MMISSILE;                  // unresistable
        pbolt.obvious_effect = true;
        break;

    case ZAP_MAGIC_DARTS:                               // cap 25
        pbolt.name = "magic dart";
        pbolt.colour = LIGHTMAGENTA;
        pbolt.range = random2(5) + 8;
        pbolt.damage = dice_def( 1, 3 + power / 5 );    // 25: 1d8
        pbolt.hit = AUTOMATIC_HIT;                      // hits always
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_MMISSILE;                  // unresistable
        pbolt.obvious_effect = true;
        break;

    case ZAP_STING:                                     // cap 25
        pbolt.name = "sting";
        pbolt.colour = GREEN;
        pbolt.range = 8 + random2(5);
        pbolt.damage = dice_def( 1, 3 + power / 5 );    // 25: 1d8
        pbolt.hit = 8 + power / 5;                      // 25: 13
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_POISON;                    // extra damage

        pbolt.obvious_effect = true;
        break;

    case ZAP_ELECTRICITY:                               // cap 20
        pbolt.name = "zap";
        pbolt.colour = LIGHTCYAN;
        pbolt.range = 6 + random2(8);                   // extended in beam
        pbolt.damage = dice_def( 1, 3 + random2(power) / 2 ); // 25: 1d11
        pbolt.hit = 8 + power / 7;                      // 25: 11
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_ELECTRICITY;               // beams & reflects

        pbolt.obvious_effect = true;
        pbolt.is_beam = true;
        break;

    case ZAP_DISRUPTION:                                // cap 25
        pbolt.name = "0";
        pbolt.flavour = BEAM_DISINTEGRATION;
        pbolt.range = 7 + random2(8);
        pbolt.damage = dice_def( 1, 4 + power / 5 );    // 25: 1d9 
        pbolt.ench_power *= 3;
        break;

    case ZAP_PAIN:                                      // cap 25
        pbolt.name = "0";
        pbolt.flavour = BEAM_PAIN;
        pbolt.range = 7 + random2(8);
        pbolt.damage = dice_def( 1, 4 + power / 5 );    // 25: 1d9
        pbolt.ench_power *= 7;
        pbolt.ench_power /= 2;
        break;

    case ZAP_FLAME_TONGUE:                              // cap 25
        pbolt.name = "flame";
        pbolt.colour = RED;

        pbolt.range = 1 + random2(2) + random2(power) / 10;
        if (pbolt.range > 4)
            pbolt.range = 4;

        pbolt.damage = dice_def( 1, 8 + power / 4 );    // 25: 1d14
        pbolt.hit = 7 + power / 6;                      // 25: 11
        pbolt.type = SYM_BOLT;
        pbolt.flavour = BEAM_FIRE;

        pbolt.obvious_effect = true;
        break;

    case ZAP_SMALL_SANDBLAST:                           // cap 25
        pbolt.name = "blast of ";

        temp_rand = random2(4);

        pbolt.name += (temp_rand == 0) ? "dust" :
                           (temp_rand == 1) ? "dirt" :
                           (temp_rand == 2) ? "grit" : "sand";

        pbolt.colour = BROWN;
        pbolt.range = (random2(power) > random2(30)) ? 2 : 1;
        pbolt.damage = dice_def( 1, 8 + power / 4 );    // 25: 1d14
        pbolt.hit = 8 + power / 5;                      // 25: 13
        pbolt.type = SYM_BOLT;
        pbolt.flavour = BEAM_FRAG;                      // extra AC resist

        pbolt.obvious_effect = true;
        break;

    case ZAP_SANDBLAST:                                 // cap 50
        pbolt.name = coinflip() ? "blast of rock" : "rocky blast";
        pbolt.colour = BROWN;

        pbolt.range = 2 + random2(power) / 20;
        if (pbolt.range > 4)
            pbolt.range = 4;

        pbolt.damage = dice_def( 2, 4 + power / 3 );    // 25: 2d12
        pbolt.hit = 13 + power / 10;                    // 25: 15
        pbolt.type = SYM_BOLT;
        pbolt.flavour = BEAM_FRAG;                      // extra AC resist

        pbolt.obvious_effect = true;
        break;

    case ZAP_BONE_SHARDS:
        pbolt.name = "spray of bone shards";
        pbolt.colour = LIGHTGREY;
        pbolt.range = 7 + random2(10);

        // Incoming power is highly dependant on mass (see spells3.cc).
        // Basic function is power * 15 + mass...  with the largest
        // available mass (3000) we get a power of 4500 at a power
        // level of 100 (for 3d20).
        pbolt.damage = dice_def( 3, 2 + (power / 250) );
        pbolt.hit = 8 + (power / 100);                   // max hit: 53
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_MAGIC;                      // unresisted

        pbolt.obvious_effect = true;
        pbolt.is_beam = true;
        break;

    case ZAP_FLAME:                                     // cap 50
        pbolt.name = "puff of flame";
        pbolt.colour = RED;
        pbolt.range = 8 + random2(5);
        pbolt.damage = dice_def( 2, 4 + power / 10 );   // 25: 2d6  50: 2d9
        pbolt.hit = 8 + power / 10;                     // 25: 10   50: 13
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_FIRE;

        pbolt.obvious_effect = true;
        break;

    case ZAP_FROST:                                     // cap 50
        pbolt.name = "puff of frost";
        pbolt.colour = WHITE;
        pbolt.range = 8 + random2(5);
        pbolt.damage = dice_def( 2, 4 + power / 10 );   // 25: 2d6  50: 2d9
        pbolt.hit = 8 + power / 10;                     // 50: 10   50: 13
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_COLD;

        pbolt.obvious_effect = true;
        break;

    case ZAP_STONE_ARROW:                               // cap 100
        pbolt.name = "stone arrow";
        pbolt.colour = LIGHTGREY;
        pbolt.range = 8 + random2(5);
        pbolt.damage = dice_def( 2, 5 + power / 7 );    // 25: 2d8  50: 2d12
        pbolt.hit = 8 + power / 10;                     // 25: 10    50: 13
        pbolt.type = SYM_MISSILE;
        pbolt.flavour = BEAM_MMISSILE;                  // irresistible

        pbolt.obvious_effect = true;
        break;

    case ZAP_STICKY_FLAME:                              // cap 100
        pbolt.name = "sticky flame";        // extra damage
        pbolt.colour = RED;
        pbolt.range = 8 + random2(5);
        pbolt.damage = dice_def( 2, 3 + power / 12 );   // 50: 2d7  100: 2d11
        pbolt.hit = 11 + power / 10;                    // 50: 16   100: 21
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_FIRE;

        pbolt.obvious_effect = true;
        break;

    case ZAP_MYSTIC_BLAST:                              // cap 100
        pbolt.name = "orb of energy";
        pbolt.colour = LIGHTMAGENTA;
        pbolt.range = 8 + random2(5);
        pbolt.damage = calc_dice( 2, 15 + (power * 2) / 5 ); 
        pbolt.hit = 10 + power / 7;                     // 50: 17   100: 24 
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_MMISSILE;                  // unresistable

        pbolt.obvious_effect = true;
        break;

    case ZAP_ICE_BOLT:                                  // cap 100
        pbolt.name = "bolt of ice";
        pbolt.colour = WHITE;
        pbolt.range = 8 + random2(5);
        pbolt.damage = calc_dice( 3, 10 + power / 2 );
        pbolt.hit = 9 + power / 12;                     // 50: 13   100: 17
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_ICE;                       // half resistable
        break;

    case ZAP_DISPEL_UNDEAD:                             // cap 100
        pbolt.name = "0";
        pbolt.flavour = BEAM_DISPEL_UNDEAD;
        pbolt.range = 7 + random2(8);
        pbolt.damage = calc_dice( 3, 20 + (power * 3) / 4 );
        pbolt.ench_power *= 3;
        pbolt.ench_power /= 2;
        break;

    case ZAP_MAGMA:                                     // cap 150
        pbolt.name = "bolt of magma";
        pbolt.colour = RED;
        pbolt.range = 5 + random2(4);
        pbolt.damage = calc_dice( 4, 10 + (power * 3) / 5 );
        pbolt.hit = 8 + power / 25;                     // 50: 10   100: 14
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_LAVA;

        pbolt.obvious_effect = true;
        pbolt.is_beam = true;
        break;

    case ZAP_FIRE:                                      // cap 150
        pbolt.name = "bolt of fire";
        pbolt.colour = RED;
        pbolt.range = 7 + random2(10);
        pbolt.damage = calc_dice( 6, 18 + power * 2 / 3 );
        pbolt.hit = 10 + power / 25;                    // 50: 12   100: 14
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_FIRE;

        pbolt.obvious_effect = true;
        pbolt.is_beam = true;
        break;

    case ZAP_COLD:                                      // cap 150
        pbolt.name = "bolt of cold";
        pbolt.colour = WHITE;
        pbolt.range = 7 + random2(10);
        pbolt.damage = calc_dice( 6, 18 + power * 2 / 3 );
        pbolt.hit = 10 + power / 25;                    // 50: 12   100: 14
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_COLD;

        pbolt.obvious_effect = true;
        pbolt.is_beam = true;
        break;

    case ZAP_VENOM_BOLT:                                // cap 150
        pbolt.name = "bolt of poison";
        pbolt.colour = LIGHTGREEN;
        pbolt.range = 8 + random2(10);
        pbolt.damage = calc_dice( 4, 15 + power / 2 );
        pbolt.hit = 8 + power / 20;                     // 50: 10   100: 13
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_POISON;                    // extra damage

        pbolt.obvious_effect = true;
        pbolt.is_beam = true;
        break;

    case ZAP_NEGATIVE_ENERGY:                           // cap 150
        pbolt.name = pbolt.effect_known ? "bolt of negative energy" : "bolt";
        pbolt.colour = DARKGREY;
        pbolt.range = 7 + random2(10);
        pbolt.damage = calc_dice( 4, 15 + (power * 3) / 5 ); 
        pbolt.hit = 8 + power / 20;                     // 50: 10   100: 13
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_NEG;                       // drains levels

        pbolt.is_beam = true;
        break;

    case ZAP_IRON_BOLT:                                 // cap 150
        pbolt.name = "iron bolt";
        pbolt.colour = LIGHTCYAN;
        pbolt.range = 5 + random2(5);
        pbolt.damage = calc_dice( 9, 15 + (power * 3) / 4 );
        pbolt.hit = 7 + power / 15;                     // 50: 10   100: 13
        pbolt.type = SYM_MISSILE;
        pbolt.flavour = BEAM_MMISSILE;                  // unresistable
        pbolt.obvious_effect = true;
        break;

    case ZAP_POISON_ARROW:                              // cap 150
        pbolt.name = "poison arrow";
        pbolt.colour = LIGHTGREEN;
        pbolt.range = 8 + random2(5);
        pbolt.damage = calc_dice( 4, 15 + power );
        pbolt.hit = 5 + power / 10;                     // 50: 10  100: 15
        pbolt.type = SYM_MISSILE;
        pbolt.flavour = BEAM_POISON_ARROW;              // extra damage
        pbolt.obvious_effect = true;
        break;


    case ZAP_DISINTEGRATION:                            // cap 150
        pbolt.name = "0";
        pbolt.flavour = BEAM_DISINTEGRATION;
        pbolt.range = 7 + random2(8);
        pbolt.damage = calc_dice( 3, 15 + (power * 3) / 4 );
        pbolt.ench_power *= 5;
        pbolt.ench_power /= 2;
        pbolt.is_beam = true;
        break;

    case ZAP_LIGHTNING:                                 // cap 150
        // also for breath (at pow = lev * 2; max dam: 33)
        pbolt.name = "bolt of lightning";
        pbolt.colour = LIGHTCYAN;
        pbolt.range = 8 + random2(10);                  // extended in beam
        pbolt.damage = calc_dice( 1, 10 + (power * 3) / 5 );
        pbolt.hit = 7 + random2(power) / 20;            // 50: 7-9  100: 7-12
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_ELECTRICITY;               // beams & reflects

        pbolt.obvious_effect = true;
        pbolt.is_beam = true;
        break;

    case ZAP_FIREBALL:                                  // cap 150
        pbolt.name = "fireball";
        pbolt.colour = RED;
        pbolt.range = 8 + random2(5);
        pbolt.damage = calc_dice( 3, 10 + power / 2 );
        pbolt.hit = 40;                                 // hit: 40
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_FIRE;                 // fire
        pbolt.is_explosion = true;
        break;

    case ZAP_ORB_OF_ELECTRICITY:                        // cap 150
        pbolt.name = "orb of electricity";
        pbolt.colour = LIGHTBLUE;
        pbolt.range = 9 + random2(12);
        pbolt.damage = calc_dice( 1, 15 + (power * 4) / 5 );
        pbolt.damage.num = 0;                    // only does explosion damage
        pbolt.hit = 40;                                 // hit: 40
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_ELECTRICITY;
        pbolt.is_explosion = true;
        break;

    case ZAP_ORB_OF_FRAGMENTATION:                      // cap 150
        pbolt.name = "metal orb";
        pbolt.colour = CYAN;
        pbolt.range = 9 + random2(7);
        pbolt.damage = calc_dice( 3, 30 + (power * 3) / 4 );
        pbolt.hit = 20;                                 // hit: 20
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_FRAG;                      // extra AC resist
        pbolt.is_explosion = true;
        break;

    case ZAP_CLEANSING_FLAME:
        pbolt.name = "golden flame";
        pbolt.colour = YELLOW;
        pbolt.range = 7;
        pbolt.damage = calc_dice( 3, 20 + (power * 2) / 3 );
        pbolt.hit = 150;
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_HOLY;

        pbolt.obvious_effect = true;
        pbolt.is_explosion = true;
        break;

    case ZAP_CRYSTAL_SPEAR:                             // cap 200
        pbolt.name = "crystal spear";
        pbolt.colour = WHITE;
        pbolt.range = 6 + random2(4);
        pbolt.damage = calc_dice( 10, 23 + power );
        pbolt.hit = 10 + power / 15;                    // 50: 13   100: 16
        pbolt.type = SYM_MISSILE;
        pbolt.flavour = BEAM_MMISSILE;                  // unresistable

        pbolt.obvious_effect = true;
        break;

    case ZAP_HELLFIRE:                                  // cap 200
        pbolt.name = "hellfire";
        pbolt.colour = RED;
        pbolt.range = 7 + random2(10);
        pbolt.damage = calc_dice( 3, 10 + (power * 3) / 4 );
        pbolt.hit = 20 + power / 10;                    // 50: 25   100: 30
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_HELLFIRE;

        pbolt.obvious_effect = true;
        pbolt.is_explosion = true;
        break;

    case ZAP_ICE_STORM:                                 // cap 200
        pbolt.name = "great blast of cold";
        pbolt.colour = BLUE;
        pbolt.range = 9 + random2(5);
        pbolt.damage = calc_dice( 10, 18 + power );
        pbolt.damage.num = 0;                    // only does explosion damage
        pbolt.hit = 20 + power / 10;                    // 50: 25   100: 30
        pbolt.ench_power = power;                       // used for radius
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_ICE;                       // half resisted
        pbolt.is_explosion = true;
        break;

    case ZAP_BEAM_OF_ENERGY:    // bolt of innacuracy
        pbolt.name = "narrow beam of energy";
        pbolt.colour = YELLOW;
        pbolt.range = 7 + random2(10);
        pbolt.damage = calc_dice( 12, 40 + (power * 3) / 2 );
        pbolt.hit = 1;
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_ENERGY;                    // unresisted

        pbolt.obvious_effect = true;
        pbolt.is_beam = true;
        break;

    case ZAP_SPIT_POISON:       // cap 50
        // max pow = lev + mut * 5 = 42
        pbolt.name = "splash of poison";
        pbolt.colour = GREEN;

        pbolt.range = 3 + random2( 1 + power / 2 );
        if (pbolt.range > 9)
            pbolt.range = 9;

        pbolt.damage = dice_def( 1, 4 + power / 2 );    // max dam: 25
        pbolt.hit = 5 + random2( 1 + power / 3 );       // max hit: 19
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_POISON;
        pbolt.obvious_effect = true;
        break;

    case ZAP_BREATHE_FIRE:      // cap 50
        // max pow = lev + mut * 4 + 12 = 51 (capped to 50)
        pbolt.name = "fiery breath";
        pbolt.colour = RED;

        pbolt.range = 3 + random2( 1 + power / 2 );
        if (pbolt.range > 9)
            pbolt.range = 9;

        pbolt.damage = dice_def( 3, 4 + power / 3 );    // max dam: 60
        pbolt.hit = 8 + random2( 1 + power / 3 );       // max hit: 25
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_FIRE;

        pbolt.obvious_effect = true;
        pbolt.is_beam = true;
        break;

    case ZAP_BREATHE_FROST:     // cap 50
        // max power = lev = 27
        pbolt.name = "freezing breath";
        pbolt.colour = WHITE;

        pbolt.range = 3 + random2( 1 + power / 2 );
        if (pbolt.range > 9)
            pbolt.range = 9;

        pbolt.damage = dice_def( 3, 4 + power / 3 );    // max dam: 39
        pbolt.hit = 8 + random2( 1 + power / 3 );
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_COLD;

        pbolt.obvious_effect = true;
        pbolt.is_beam = true;
        break;

    case ZAP_BREATHE_ACID:      // cap 50
        // max power = lev for ability, 50 for minor destruction (max dam: 57)
        pbolt.name = "acid";
        pbolt.colour = YELLOW;

        pbolt.range = 3 + random2( 1 + power / 2 );
        if (pbolt.range > 9)
            pbolt.range = 9;

        pbolt.damage = dice_def( 3, 3 + power / 3 );    // max dam: 36
        pbolt.hit = 5 + random2( 1 + power / 3 );
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_ACID;

        pbolt.obvious_effect = true;
        pbolt.is_beam = true;
        break;

    case ZAP_BREATHE_POISON:    // leaves clouds of gas // cap 50
        // max power = lev = 27
        pbolt.name = "poison gas";
        pbolt.colour = GREEN;

        pbolt.range = 3 + random2( 1 + power / 2 );
        if (pbolt.range > 9)
            pbolt.range = 9;

        pbolt.damage = dice_def( 3, 2 + power / 6 );    // max dam: 18
        pbolt.hit = 6 + random2( 1 + power / 3 );
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_POISON;

        pbolt.obvious_effect = true;
        pbolt.is_beam = true;
        break;

    case ZAP_BREATHE_POWER:     // cap 50
        pbolt.name = "bolt of energy";
        // max power = lev = 27

        pbolt.colour = BLUE;
        if (random2(power) >= 8)
            pbolt.colour = LIGHTBLUE;
        if (random2(power) >= 12)
            pbolt.colour = MAGENTA;
        if (random2(power) >= 17)
            pbolt.colour = LIGHTMAGENTA;

        pbolt.range = 6 + random2( 1 + power / 2 );
        if (pbolt.range > 9)
            pbolt.range = 9;

        pbolt.damage = dice_def( 3, 3 + power / 3 );    // max dam: 36
        pbolt.hit = 5 + random2( 1 + power / 3 );
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_MMISSILE;                  // unresistable

        pbolt.obvious_effect = true;
        pbolt.is_beam = true;
        break;

    case ZAP_BREATHE_STEAM:     // cap 50
        // max power = lev = 27
        pbolt.name = "ball of steam";
        pbolt.colour = LIGHTGREY;

        pbolt.range = 6 + random2(5);
        if (pbolt.range > 9)
            pbolt.range = 9;

        pbolt.damage = dice_def( 3, 4 + power / 5 );    // max dam: 27
        pbolt.hit = 10 + random2( 1 + power / 5 );
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_STEAM;

        pbolt.obvious_effect = true;
        pbolt.is_beam = true;
        break;

    case ZAP_SLOWING:
        pbolt.name = "0";
        pbolt.flavour = BEAM_SLOW;
        // pbolt.is_beam = true;
        break;

    case ZAP_HASTING:
        pbolt.name = "0";
        pbolt.flavour = BEAM_HASTE;
        // pbolt.is_beam = true;
        break;

    case ZAP_PARALYSIS:
        pbolt.name = "0";
        pbolt.flavour = BEAM_PARALYSIS;
        // pbolt.is_beam = true;
        break;

    case ZAP_CONFUSION:
        pbolt.name = "0";
        pbolt.flavour = BEAM_CONFUSION;
        // pbolt.is_beam = true;
        break;

    case ZAP_INVISIBILITY:
        pbolt.name = "0";
        pbolt.flavour = BEAM_INVISIBILITY;
        // pbolt.is_beam = true;
        break;

    case ZAP_HEALING:
        pbolt.name = "0";
        pbolt.flavour = BEAM_HEALING;
        pbolt.damage = dice_def( 1, 7 + power / 3 );
        // pbolt.is_beam = true;
        break;

    case ZAP_DIGGING:
        pbolt.name = "0";
        pbolt.flavour = BEAM_DIGGING;
        // not ordinary "0" beam range {dlb}
        pbolt.range = 3 + random2( power / 5 ) + random2(5);
        pbolt.is_beam = true;
        break;

    case ZAP_TELEPORTATION:
        pbolt.name = "0";
        pbolt.flavour = BEAM_TELEPORT;
        pbolt.range = 9 + random2(5);
        // pbolt.is_beam = true;
        break;

    case ZAP_POLYMORPH_OTHER:
        pbolt.name = "0";
        pbolt.flavour = BEAM_POLYMORPH;
        pbolt.range = 9 + random2(5);
        // pbolt.is_beam = true;
        break;

    case ZAP_ENSLAVEMENT:
        pbolt.name = "0";
        pbolt.flavour = BEAM_CHARM;
        pbolt.range = 7 + random2(5);
        // pbolt.is_beam = true;
        break;

    case ZAP_BANISHMENT:
        pbolt.name = "0";
        pbolt.flavour = BEAM_BANISH;
        pbolt.range = 7 + random2(5);
        // pbolt.is_beam = true;
        break;

    case ZAP_DEGENERATION:
        pbolt.name = "0";
        pbolt.flavour = BEAM_DEGENERATE;
        pbolt.range = 7 + random2(5);
        // pbolt.is_beam = true;
        break;

    case ZAP_ENSLAVE_UNDEAD:
        pbolt.name = "0";
        pbolt.flavour = BEAM_ENSLAVE_UNDEAD;
        pbolt.range = 7 + random2(5);
        // pbolt.is_beam = true;
        break;

    case ZAP_AGONY:
        pbolt.name = "0agony";
        pbolt.flavour = BEAM_PAIN;
        pbolt.range = 7 + random2(8);
        pbolt.ench_power *= 5;
        // pbolt.is_beam = true;
        break;

    case ZAP_CONTROL_DEMON:
        pbolt.name = "0";
        pbolt.flavour = BEAM_ENSLAVE_DEMON;
        pbolt.range = 7 + random2(5);
        pbolt.ench_power *= 3;
        pbolt.ench_power /= 2;
        // pbolt.is_beam = true;
        break;

    case ZAP_SLEEP:             //jmf: added
        pbolt.name = "0";
        pbolt.flavour = BEAM_SLEEP;
        pbolt.range = 7 + random2(5);
        // pbolt.is_beam = true;
        break;

    case ZAP_BACKLIGHT: //jmf: added
        pbolt.name = "0";
        pbolt.flavour = BEAM_BACKLIGHT;
        pbolt.colour = BLUE;
        pbolt.range = 7 + random2(5);
        // pbolt.is_beam = true;
        break;

    case ZAP_DEBUGGING_RAY:
        pbolt.name = "debugging ray";
        pbolt.colour = random_colour();
        pbolt.range = 7 + random2(10);
        pbolt.damage = dice_def( 1500, 1 );             // dam: 1500
        pbolt.hit = 1500;                               // hit: 1500
        pbolt.type = SYM_DEBUG;
        pbolt.flavour = BEAM_MMISSILE;                  // unresistable

        pbolt.obvious_effect = true;
        break;

    default:
        pbolt.name = "buggy beam";
        pbolt.colour = random_colour();
        pbolt.range = 7 + random2(10);
        pbolt.damage = dice_def( 1, 0 );
        pbolt.hit = 60;
        pbolt.type = SYM_DEBUG;
        pbolt.flavour = BEAM_MMISSILE;                  // unresistable

        pbolt.obvious_effect = true;
        break;
    }                           // end of switch

    if ( wearing_amulet(AMU_INACCURACY) )
    {
        pbolt.hit -= 5;
        if ( pbolt.hit < 0 )
            pbolt.hit = 0;
    }
}                               // end zappy()

/*  NEW (GDL):
 *  Now handles all beamed/thrown items and spells, tracers, and their effects.
 *  item is used for items actually thrown/launched
 *
 *  if item is NULL, there is no physical object being thrown that could
 *  land on the ground.
 */


/*
 * Beam pseudo code:
 *
 * 1. Calculate stepx and stepy - algorithms depend on finding a step axis
 *    which results in a line of rise 1 or less (ie 45 degrees or less)
 * 2. Calculate range.  Tracers always have max range, otherwise the beam
 *    will have somewhere between range and rangeMax
 * 3. Loop tracing out the line:
 *      3a. Check for walls and wall affecting beams
 *      3b. If no valid move is found, try a fuzzy move
 *      3c. If no valid move is yet found, try bouncing
 *      3d. If no valid move or bounce is found, break
 *      4. Check for beam termination on target
 *      5. Affect the cell which the beam just moved into -> affect()
 *      6. Decrease remaining range appropriately
 *      7. Check for early out due to aimed_at_feet
 *      8. Draw the beam
 * 9. Drop an object where the beam 'landed'
 *10. Beams explode where the beam 'landed'
 *11. If no message generated yet, send "nothing happens" (enchantments only)
 *
 */


void fire_beam( bolt &pbolt, item_def *item )
{
    bool beamTerminate;     // has beam been 'stopped' by something?
    int &tx(pbolt.pos.x), &ty(pbolt.pos.y);     // test(new) x,y - integer
    int rangeRemaining;
    bool did_bounce = false;
    cursor_control coff(false);

    beam_message_cache.clear();

#if DEBUG_DIAGNOSTICS
    if (pbolt.flavour != BEAM_LINE_OF_SIGHT)
    {
        mprf( MSGCH_DIAGNOSTICS,
             "%s%s%s (%d,%d) to (%d,%d): ty=%d col=%d flav=%d hit=%d dam=%dd%d range=%d",
                 (pbolt.is_beam) ? "beam" : "missile", 
                 (pbolt.is_explosion) ? "*" : 
                     (pbolt.is_big_cloud) ? "+" : "", 
                 (pbolt.is_tracer) ? " tracer" : "",
                 pbolt.source_x, pbolt.source_y, 
                 pbolt.target_x, pbolt.target_y, 
                 pbolt.type, pbolt.colour, pbolt.flavour, 
                 pbolt.hit, pbolt.damage.num, pbolt.damage.size,
                 pbolt.range);
    }
#endif

    // init
    pbolt.aimed_at_feet =
        (pbolt.target_x == pbolt.source_x) &&
        (pbolt.target_y == pbolt.source_y);
    pbolt.msg_generated = false;

    ray_def ray;

    if ( pbolt.chose_ray )
        ray = pbolt.ray;
    else
    {
        ray.fullray_idx = -1;   // to quiet valgrind
        find_ray( pbolt.source_x, pbolt.source_y,
                  pbolt.target_x, pbolt.target_y, true, ray,
                  0, true );
    }

    if ( !pbolt.aimed_at_feet )
        ray.advance_through(pbolt.target());

    // give chance for beam to affect one cell even if aimed_at_feet.
    beamTerminate = false;

    // setup range
    rangeRemaining = pbolt.range;
    if (pbolt.rangeMax > pbolt.range)
    {
        if (pbolt.is_tracer)
            rangeRemaining = pbolt.rangeMax;
        else
            rangeRemaining += random2((pbolt.rangeMax - pbolt.range) + 1);
    }

    // before we start drawing the beam, turn buffering off
#ifdef WIN32CONSOLE
    bool oldValue = true;
    if (!pbolt.is_tracer)
        oldValue = set_buffering(false);
#endif

    while(!beamTerminate)
    {
        tx = ray.x();
        ty = ray.y();

        // shooting through clouds affects accuracy
        if ( env.cgrid[tx][ty] != EMPTY_CLOUD )
            pbolt.hit = std::max(pbolt.hit - 2, 0);

        // see if tx, ty is blocked by something
        if (grid_is_solid(grd[tx][ty]))
        {
            // first, check to see if this beam affects walls.
            if (affects_wall(pbolt, grd[tx][ty]))
            {
                // should we ever get a tracer with a wall-affecting
                // beam (possible I suppose), we'll quit tracing now.
                if (!pbolt.is_tracer)
                    rangeRemaining -= affect(pbolt, tx, ty);

                // if it's still a wall, quit.
                if (grid_is_solid(grd[tx][ty]))
                    break;      // breaks from line tracing
            }
            else
            {
                // BEGIN bounce case
                if (!isBouncy(pbolt, grd[tx][ty]))
                {
                    do
                        ray.regress();
                    while (grid_is_solid(grd(ray.pos())));
                    tx = ray.x();
                    ty = ray.y();
                    break;          // breaks from line tracing
                }

                did_bounce = true;

                // bounce
                do {
                    do
                        ray.regress();
                    while (grid_is_solid(grd(ray.pos())));
                    ray.advance_and_bounce();
                    --rangeRemaining;
                } while ( rangeRemaining > 0 &&
                          grid_is_solid(grd[ray.x()][ray.y()]) );

                if (rangeRemaining < 1)
                    break;
                tx = ray.x();
                ty = ray.y();
            } // end else - beam doesn't affect walls
        } // endif - is tx, ty wall?

        // at this point, if grd[tx][ty] is still a wall, we
        // couldn't find any path: bouncy, fuzzy, or not - so break.
        if (grid_is_solid(grd[tx][ty]))
            break;
        
        // check for "target termination"
        // occurs when beam can be targetted at empty
        // cell (e.g. a mage wants an explosion to happen
        // between two monsters)

        // in this case, don't affect the cell - players and
        // monsters have no chance to dodge or block such
        // a beam, and we want to avoid silly messages.
        if (tx == pbolt.target_x && ty == pbolt.target_y)
            beamTerminate = beam_term_on_target(pbolt, tx, ty);

        // affect the cell, except in the special case noted
        // above -- affect() will early out if something gets
        // hit and the beam is type 'term on target'.
        if (!beamTerminate || !pbolt.is_explosion)
        {
            // random beams: randomize before affect
            bool random_beam = false;
            if (pbolt.flavour == BEAM_RANDOM)
            {
                random_beam = true;
                pbolt.flavour = BEAM_FIRE + random2(7);
            }

            if (!pbolt.affects_nothing)
                rangeRemaining -= affect(pbolt, tx, ty);

            if (random_beam)
            {
                pbolt.flavour = BEAM_RANDOM;
                pbolt.effect_known = false;
            }
        }

        // always decrease range by 1
        rangeRemaining -= 1;

        // check for range termination
        if (rangeRemaining <= 0)
            beamTerminate = true;

        // special case - beam was aimed at feet
        if (pbolt.aimed_at_feet)
            beamTerminate = true;

        // actually draw the beam/missile/whatever,
        // if the player can see the cell.
        if (!pbolt.is_tracer && pbolt.name[0] != '0' && see_grid(tx,ty))
        {
            // we don't clean up the old position.
            // first, most people like seeing the full path,
            // and second, it is hard to do it right with
            // respect to killed monsters, cloud trails, etc.

            // draw new position
            int drawx = grid2viewX(tx);
            int drawy = grid2viewY(ty);
            // bounds check
            if (in_los_bounds(drawx, drawy))
            {
                if (pbolt.colour == BLACK)
                    textcolor(random_colour());
                else
                    textcolor(pbolt.colour);

                gotoxy(drawx, drawy);
                putch(pbolt.type);

                // get curses to update the screen so we can see the beam
                update_screen();

                delay(15);

#ifdef MISSILE_TRAILS_OFF
                if (!pbolt.is_beam || pbolt.name[0] == '0')
                    viewwindow(1,false); // mv: added. It's not optimal but
                                         // is usually enough
#endif
            }

        }

        if (!did_bounce)
            ray.advance_through(pbolt.target());
        else
            ray.advance(true);
    } // end- while !beamTerminate

    // the beam has finished, and terminated at tx, ty

    // leave an object, if applicable
    if (item)
        beam_drop_object( pbolt, item, tx, ty );

    // check for explosion.  NOTE that for tracers, we have to make a copy
    // of target co'ords and then reset after calling this -- tracers should
    // never change any non-tracers fields in the beam structure. -- GDL
    int ox = pbolt.target_x;
    int oy = pbolt.target_y;

    beam_explodes(pbolt, tx, ty);

    if (pbolt.is_tracer)
    {
        pbolt.target_x = ox;
        pbolt.target_y = oy;
    }

    // canned msg for enchantments that affected no-one
    if (pbolt.name[0] == '0' && pbolt.flavour != BEAM_DIGGING)
    {
        if (!pbolt.is_tracer && !pbolt.msg_generated && !pbolt.obvious_effect)
            canned_msg(MSG_NOTHING_HAPPENS);
    }

    if (!pbolt.is_tracer && pbolt.beam_source != NON_MONSTER)
    {
        if (pbolt.foe_hurt == 0 && pbolt.fr_hurt > 0)
            xom_is_stimulated(128);
        else if (pbolt.foe_helped > 0 && pbolt.fr_helped == 0)
            xom_is_stimulated(128);
    }

    // that's it!
#ifdef WIN32CONSOLE
    if (!pbolt.is_tracer)
        set_buffering(oldValue);
#endif
}                               // end fire_beam();


// returns damage taken by a monster from a "flavoured" (fire, ice, etc.)
// attack -- damage from clouds and branded weapons handled elsewhere.
int mons_adjust_flavoured( monsters *monster, bolt &pbolt,
                           int hurted, bool doFlavouredEffects )
{
    // if we're not doing flavored effects, must be preliminary
    // damage check only;  do not print messages or apply any side
    // effects!
    int resist;

    switch (pbolt.flavour)
    {
    case BEAM_FIRE:
    case BEAM_STEAM:
        resist = mons_res_fire(monster);
        if (resist > 1)
        {
            if (doFlavouredEffects)
                simple_monster_message(monster, " appears unharmed.");

            hurted = 0;
        }
        else if (resist == 1)
        {
            if (doFlavouredEffects)
                simple_monster_message(monster, " resists.");

            hurted /= 3;
        }
        else if (resist < 0)
        {
            if (mons_is_icy(monster))
            {
                if (doFlavouredEffects)
                    simple_monster_message(monster, " melts!");
            }
            else
            {
                if (doFlavouredEffects)
                    simple_monster_message(monster, " is burned terribly!");
            }

            hurted *= 15;
            hurted /= 10;
        }
        break;


    case BEAM_COLD:
        resist = mons_res_cold(monster);
        if (resist > 1)
        {
            if (doFlavouredEffects)
                simple_monster_message(monster, " appears unharmed.");

            hurted = 0;
        }
        else if (resist == 1)
        {
            if (doFlavouredEffects)
                simple_monster_message(monster, " resists.");

            hurted /= 3;
        }
        else if (resist < 0)
        {
            if (doFlavouredEffects)
                simple_monster_message(monster, " is frozen!");

            hurted *= 15;
            hurted /= 10;
        }
        break;

    case BEAM_ELECTRICITY:
        if (mons_res_elec(monster) > 0)
        {
            if (doFlavouredEffects)
                simple_monster_message(monster, " appears unharmed.");

            hurted = 0;
        }
        break;

    case BEAM_ACID:
        if (mons_res_acid(monster))
        {
            if (doFlavouredEffects)
                simple_monster_message(monster, " appears unharmed.");

            hurted = 0;
        }
        break;

    case BEAM_POISON:
        if (mons_res_poison(monster) > 0)
        {
            if (doFlavouredEffects)
                simple_monster_message( monster, " appears unharmed." );

            hurted = 0;
        }
        else if (doFlavouredEffects && !one_chance_in(3))
        {
            poison_monster( monster, whose_kill(pbolt) );
        }
        break;

    case BEAM_POISON_ARROW:
        if (mons_res_poison(monster) > 0)
        {
            if (doFlavouredEffects)
            {
                simple_monster_message( monster, " partially resists." );

                // Poison arrow can poison any living thing regardless of 
                // poison resistance. -- bwr
                if (mons_has_lifeforce(monster))
                    poison_monster( monster, whose_kill(pbolt), 2, true );
            }

            hurted /= 2;
        }
        else if (doFlavouredEffects)
        {
            poison_monster( monster, whose_kill(pbolt), 4 );
        }
        break;

    case BEAM_NEG:
        if (mons_res_negative_energy(monster) > 0)
        {
            if (doFlavouredEffects)
                simple_monster_message(monster, " appears unharmed.");

            hurted = 0;
        }
        else
        {
            // early out for tracer/no side effects
            if (!doFlavouredEffects)
                return (hurted);

            simple_monster_message(monster, " is drained.");
            pbolt.obvious_effect = true;

            if (YOU_KILL(pbolt.thrower) && pbolt.effect_known)
                did_god_conduct(DID_NECROMANCY, 2 + random2(3));

            if (one_chance_in(5))
            {
                monster->hit_dice--;
                monster->experience = 0;
            }

            monster->max_hit_points -= 2 + random2(3);
            monster->hit_points -= 2 + random2(3);

            if (monster->hit_points >= monster->max_hit_points)
                monster->hit_points = monster->max_hit_points;

            if (monster->hit_dice < 1)
                monster->hit_points = 0;
        }                       // end else
        break;

    case BEAM_MIASMA:
        if (mons_res_negative_energy( monster ) >= 3)
        {
            if (doFlavouredEffects)
                simple_monster_message(monster, " appears unharmed.");

            hurted = 0;
        }
        else
        {
            // early out for tracer/no side effects
            if (!doFlavouredEffects)
                return (hurted);

            if (mons_res_poison( monster ) <= 0)
                poison_monster( monster, whose_kill(pbolt) );

            if (one_chance_in( 3 + 2 * mons_res_negative_energy(monster) ))
            {
                bolt beam;
                beam.flavour = BEAM_SLOW;
                mons_ench_f2( monster, beam );
            }
        }
        break;

    case BEAM_HOLY:             // flame of cleansing
        if (mons_is_unholy( monster ))
        {
            if (doFlavouredEffects)
                simple_monster_message( monster, " writhes in agony!" );

            hurted = (hurted * 3) / 2;
        }
        else if (!mons_is_evil( monster ))
        {
            if (doFlavouredEffects)
                simple_monster_message( monster, " appears unharmed." );

            hurted = 0;
        }
        break;

    case BEAM_ICE:
        /* ice - about 50% of damage is cold, other 50% is impact and
           can't be resisted (except by AC, of course) */
        resist = mons_res_cold(monster);
        if (resist > 0)
        {
            if (doFlavouredEffects)
                simple_monster_message(monster, " partially resists.");

            hurted /= 2;
        }
        else if (resist < 0)
        {
            if (doFlavouredEffects)
                simple_monster_message(monster, " is frozen!");

            hurted *= 13;
            hurted /= 10;
        }
        break;
    }                           /* end of switch */

    if (pbolt.flavour == BEAM_LAVA)    //jmf: lava != hellfire
    {
        resist = mons_res_fire(monster);
        if (resist > 0)
        {
            if (doFlavouredEffects)
                simple_monster_message(monster, " partially resists.");

            hurted /= 2;
        }
        else if (resist < 0)
        {
            if (mons_is_icy(monster))
            {
                if (doFlavouredEffects)
                    simple_monster_message(monster, " melts!");
            }
            else
            {
                if (doFlavouredEffects)
                    simple_monster_message(monster, " is burned terribly!");
            }

            hurted *= 12;
            hurted /= 10;
        }
    }
    else if (pbolt.name == "hellfire")
    {
        resist = mons_res_fire(monster);
        if (resist > 2)
        {
            if (doFlavouredEffects)
                simple_monster_message(monster, " appears unharmed.");

            hurted = 0;
        }
        else if (resist > 0)
        {
            if (doFlavouredEffects)
                simple_monster_message(monster, " partially resists.");

            hurted /= 2;
        }
        else if (resist < 0)
        {
            if (mons_is_icy(monster))
            {
                if (doFlavouredEffects)
                    simple_monster_message(monster, " melts!");
            }
            else
            {
                if (doFlavouredEffects)
                    simple_monster_message(monster, " is burned terribly!");
            }

            hurted *= 12;       /* hellfire */
            hurted /= 10;
        }
    }

    return (hurted);
}                               // end mons_adjust_flavoured()

static bool monster_resists_mass_enchantment(monsters *monster,
                                             enchant_type wh_enchant,
                                             int pow)
{
    // assuming that the only mass charm is control undead:
    if (wh_enchant == ENCH_CHARM)
    {
        if (mons_friendly(monster))
            return (true);

        if (mons_class_holiness(monster->type) != MH_UNDEAD)
            return (true);

        if (check_mons_resist_magic( monster, pow ))
        {
            simple_monster_message(monster, mons_immune_magic(monster) ?
                                   " is unaffected." : " resists.");
            return (true);
        }
    }
    else if (wh_enchant == ENCH_CONFUSION
             || mons_holiness(monster) == MH_NATURAL)
    {
        if (check_mons_resist_magic( monster, pow ))
        {
            simple_monster_message(monster, mons_immune_magic(monster) ?
                                   " is unaffected." : " resists.");
            return (true);
        }
    }
    else  // trying to enchant an unnatural creature doesn't work
    {
        simple_monster_message(monster, " is unaffected.");
        return (true);
    }

    return (false);
}

// Enchants all monsters in player's sight.
bool mass_enchantment( enchant_type wh_enchant, int pow, int origin )
{
    int i;                      // loop variable {dlb}
    bool msg_generated = false;
    monsters *monster;

    viewwindow(0, false);

    if (pow > 200)
        pow = 200;

    const kill_category kc = origin == MHITYOU? KC_YOU : KC_OTHER;

    for (i = 0; i < MAX_MONSTERS; i++)
    {
        monster = &menv[i];

        if (monster->type == -1 || !mons_near(monster))
            continue;

        if (monster->has_ench(wh_enchant))
            continue;

        if (monster_resists_mass_enchantment(monster, wh_enchant, pow))
            continue;        

        if (monster->add_ench(mon_enchant(wh_enchant, 0, kc)))
        {
            if (player_monster_visible( monster ))
            {
                // turn message on
                msg_generated = true;
                switch (wh_enchant)
                {
                case ENCH_FEAR:
                    simple_monster_message(monster,
                                           " looks frightened!");
                    break;
                case ENCH_CONFUSION:
                    simple_monster_message(monster,
                                           " looks rather confused.");
                    break;
                case ENCH_CHARM:
                    simple_monster_message(monster,
                                           " submits to your will.");
                    break;
                default:
                    // oops, I guess not!
                    msg_generated = false;
                }
            }

            // extra check for fear (monster needs to reevaluate behaviour)
            if (wh_enchant == ENCH_FEAR)
                behaviour_event( monster, ME_SCARE, origin );
        }
    }                           // end "for i"

    if (!msg_generated)
        canned_msg(MSG_NOTHING_HAPPENS);

    return (msg_generated);
}                               // end mass_enchantment()

/*
   Monster has probably failed save, now it gets enchanted somehow.

   returns MON_RESIST if monster is unaffected due to magic resist.
   returns MON_UNAFFECTED if monster is immune to enchantment
   returns MON_AFFECTED in all other cases (already enchanted, etc)
 */
int mons_ench_f2(monsters *monster, bolt &pbolt)
{
    switch (pbolt.flavour)      /* put in magic resistance */
    {
    case BEAM_SLOW:         /* 0 = slow monster */
        // try to remove haste, if monster is hasted
        if (monster->del_ench(ENCH_HASTE))
        {
            if (simple_monster_message(monster,
                                       " is no longer moving quickly."))
                pbolt.obvious_effect = true;

            return (MON_AFFECTED);
        }

        // not hasted, slow it
        if (!monster->has_ench(ENCH_SLOW)
            && !mons_is_stationary(monster)
            && monster->add_ench(mon_enchant(ENCH_SLOW, 0, whose_kill(pbolt))))
        {
            if (!mons_is_paralysed(monster)
                && simple_monster_message(monster, " seems to slow down."))
            {
                pbolt.obvious_effect = true;
            }
        }
        return (MON_AFFECTED);

    case BEAM_HASTE:                  // 1 = haste
        if (monster->del_ench(ENCH_SLOW))
        {
            if (simple_monster_message(monster, " is no longer moving slowly."))
                pbolt.obvious_effect = true;

            return (MON_AFFECTED);
        }

        // not slowed, haste it
        if (!monster->has_ench(ENCH_HASTE)
            && !mons_is_stationary(monster)
            && monster->add_ench(ENCH_HASTE))
        {
            if (!mons_is_paralysed(monster)
                && simple_monster_message(monster, " seems to speed up."))
            {
                pbolt.obvious_effect = true;
            }
        }
        return (MON_AFFECTED);

    case BEAM_HEALING:         /* 2 = healing */
        if (heal_monster( monster, 5 + roll_dice( pbolt.damage ), false ))
        {
            if (monster->hit_points == monster->max_hit_points)
            {
                if (simple_monster_message(monster,
                                           "'s wounds heal themselves!"))
                    pbolt.obvious_effect = true;
            }
            else
            {
                if (simple_monster_message(monster, " is healed somewhat."))
                    pbolt.obvious_effect = true;
            }
        }
        return (MON_AFFECTED);

    case BEAM_PARALYSIS:                  /* 3 = paralysis */
        beam_paralyses_monster(pbolt, monster);
        return (MON_AFFECTED);

    case BEAM_CONFUSION:                   /* 4 = confusion */
        if (monster->add_ench(
                mon_enchant(ENCH_CONFUSION, 0, whose_kill(pbolt))))
        {
            // put in an exception for fungi, plants and other things you won't
            // notice becoming confused.
            if (simple_monster_message(monster, " appears confused."))
                pbolt.obvious_effect = true;
        }
        return (MON_AFFECTED);

    case BEAM_INVISIBILITY:               /* 5 = invisibility */
        // Store the monster name before it becomes an "it" -- bwr
    {
        const std::string monster_name = monster->name(DESC_CAP_THE);
        
        if (!monster->has_ench(ENCH_INVIS)
            && monster->add_ench(ENCH_INVIS))
        {
            // A casting of invisibility erases backlight.
            monster->del_ench(ENCH_BACKLIGHT);
            
            // Can't use simple_monster_message here, since it checks
            // for visibility of the monster (and its now invisible) -- bwr
            if (mons_near( monster ))
            {
                mprf("%s flickers %s",
                     monster_name.c_str(),
                     player_monster_visible(monster) ? "for a moment." 
                                                     : "and vanishes!" );
            }

            pbolt.obvious_effect = true;
        }
        return (MON_AFFECTED);
    }
    case BEAM_CHARM:             /* 9 = charm */
        if (monster->add_ench(ENCH_CHARM))
        {
            // break fleeing and suchlike
            monster->behaviour = BEH_SEEK;

            // put in an exception for fungi, plants and other things you won't
            // notice becoming charmed.
            if (simple_monster_message(monster, " is charmed."))
                pbolt.obvious_effect = true;
        }
        return (MON_AFFECTED);

    default:
        break;
    }                           /* end of switch (beam_colour) */

    return (MON_AFFECTED);
}                               // end mons_ench_f2()

// degree is ignored.
static void slow_monster(monsters *mon, int /* degree */)
{
    bolt beam;
    beam.flavour = BEAM_SLOW;
    mons_ench_f2(mon, beam);
}

static void beam_paralyses_monster(bolt &pbolt, monsters *monster)
{
    if (!monster->has_ench(ENCH_PARALYSIS)
        && monster->add_ench(ENCH_PARALYSIS))
    {
        if (simple_monster_message(monster, " suddenly stops moving!"))
            pbolt.obvious_effect = true;

        mons_check_pool(monster, pbolt.killer(), pbolt.beam_source);
    }
}

// Returns true if the curare killed the monster.
bool curare_hits_monster( const bolt &beam,
                          monsters *monster,
                          kill_category who,
                          int levels )
{
    const bool res_poison = mons_res_poison(monster);
    bool mondied = false;

    poison_monster(monster, who, levels, false);

    if (!mons_res_asphyx(monster))
    {
        int hurted = roll_dice(2, 6);

        // Note that the hurtage is halved by poison resistance.
        if (res_poison)
            hurted /= 2;

        if (hurted)
        {
            simple_monster_message(monster, " convulses.");
            if ((monster->hit_points -= hurted) < 1)
            {
                monster_die(monster, beam);
                mondied = true;
            }
        }

        if (!mondied)
            slow_monster(monster, levels);
    }

    // Deities take notice.
    if (who == KC_YOU)
        did_god_conduct( DID_POISON, 5 + random2(3) );

    return (mondied);
}

// actually poisons a monster (w/ message)
bool poison_monster( monsters *monster,
                     kill_category from_whom,
                     int levels,
                     bool force,
                     bool verbose)
{
    if (!monster->alive())
        return (false);

    if (!force && mons_res_poison(monster) > 0)
        return (false);

    const mon_enchant old_pois = monster->get_ench(ENCH_POISON);
    monster->add_ench( mon_enchant(ENCH_POISON, levels, from_whom) );
    const mon_enchant new_pois = monster->get_ench(ENCH_POISON);

    // actually do the poisoning
    // note: order important here
    if (new_pois.degree > old_pois.degree && verbose)
    {
        simple_monster_message( monster, 
                                !old_pois.degree? " is poisoned." 
                                                : " looks even sicker." );
    }

    // finally, take care of deity preferences
    if (from_whom == KC_YOU)
        did_god_conduct( DID_POISON, 5 + random2(3) );

    return (new_pois.degree > old_pois.degree);
}

// actually napalms a monster (w/ message)
void sticky_flame_monster( int mn, kill_category who, int levels )
{
    monsters *monster = &menv[mn];

    if (!monster->alive())
        return;

    if (mons_res_fire(monster) > 0)
        return;

    if (monster->add_ench(mon_enchant(ENCH_STICKY_FLAME, levels, who)))
        simple_monster_message(monster, " is covered in liquid fire!");
}                               // end sticky_flame_monster

/*
 * Used by monsters in "planning" which spell to cast. Fires off a "tracer"
 * which tells the monster what it'll hit if it breathes/casts etc.
 *
 * The output from this tracer function is four variables in the beam struct:
 * fr_count, foe_count: a count of how many friends and foes will (probably)
 * be hit by this beam
 * fr_power, foe_power: a measure of how many 'friendly' hit dice it will
 * affect, and how many 'unfriendly' hit dice.
 *
 * note that beam properties must be set, as the tracer will take them
 * into account, as well as the monster's intelligence.
 *
 */
void fire_tracer(const monsters *monster, bolt &pbolt)
{
    // don't fiddle with any input parameters other than tracer stuff!
    pbolt.is_tracer = true;
    pbolt.source_x = monster->x;    // always safe to do.
    pbolt.source_y = monster->y;
    pbolt.beam_source = monster_index(monster);
    pbolt.can_see_invis = (mons_see_invis(monster) != 0);
    pbolt.smart_monster = (mons_intel(monster->type) == I_HIGH ||
                          mons_intel(monster->type) == I_NORMAL);
    pbolt.attitude = mons_attitude(monster);

    // init tracer variables
    pbolt.foe_count = pbolt.fr_count = 0;
    pbolt.foe_power = pbolt.fr_power = 0;
    pbolt.fr_helped = pbolt.fr_hurt  = 0;
    pbolt.foe_helped = pbolt.foe_hurt = 0;
    pbolt.foe_ratio = 80;        // default - see mons_should_fire()

    // foe ratio for summon gtr. demons & undead -- they may be
    // summoned, but they're hostile and would love nothing better
    // than to nuke the player and his minions
    if (pbolt.attitude == ATT_FRIENDLY && monster->attitude != ATT_FRIENDLY)
        pbolt.foe_ratio = 25;

    // fire!
    fire_beam(pbolt);

    // unset tracer flag (convenience)
    pbolt.is_tracer = false;
}                               // end tracer_f()

bool check_line_of_sight( int sx, int sy, int tx, int ty )
{
    const int dist = grid_distance( sx, sy, tx, ty );

    // can always see one square away
    if (dist <= 1)
        return (true);

    // currently we limit the range to 8
    if (dist > MONSTER_LOS_RANGE)
        return (false);
    
    // Note that we are guaranteed to be within the player LOS range,
    // so fallback is unnecessary.
    ray_def ray;
    return find_ray( sx, sy, tx, ty, false, ray );
}

/*
   When a mimic is hit by a ranged attack, it teleports away (the slow way)
   and changes its appearance - the appearance change is in monster_teleport
   in mstuff2.
 */
void mimic_alert(monsters *mimic)
{
    if (mimic->has_ench(ENCH_TP))
        return;

    monster_teleport( mimic, !one_chance_in(3) );
}                               // end mimic_alert()

static bool isBouncy(bolt &beam, unsigned char gridtype)
{
    if (beam.name[0] == '0')
        return false;
    if (beam.flavour == BEAM_ELECTRICITY && gridtype != DNGN_METAL_WALL)
        return true;
    if ( (beam.flavour == BEAM_FIRE || beam.flavour == BEAM_COLD) &&
         (gridtype == DNGN_GREEN_CRYSTAL_WALL) )
        return true;
    return (false);
}

static void beam_explodes(bolt &beam, int x, int y)
{
    cloud_type cl_type;

    // this will be the last thing this beam does.. set target_x
    // and target_y to hold explosion co'ords.

    beam.target_x = x;
    beam.target_y = y;

    // generic explosion
    if (beam.is_explosion) // beam.flavour == BEAM_EXPLOSION || beam.flavour == BEAM_HOLY)
    {
        explosion1(beam);
        return;
    }

    if (beam.flavour >= BEAM_POTION_STINKING_CLOUD
        && beam.flavour <= BEAM_POTION_RANDOM)
    {
        switch (beam.flavour)
        {
        case BEAM_POTION_STINKING_CLOUD:
            beam.colour = GREEN;
            break;

        case BEAM_POTION_POISON:
            beam.colour = (coinflip() ? GREEN : LIGHTGREEN);
            break;

        case BEAM_POTION_MIASMA:
        case BEAM_POTION_BLACK_SMOKE:
            beam.colour = DARKGREY;
            break;

        case BEAM_POTION_STEAM:
            beam.colour = LIGHTGREY;
            break;

        case BEAM_POTION_FIRE:
            beam.colour = (coinflip() ? RED : LIGHTRED);
            break;

        case BEAM_POTION_COLD:
            beam.colour = (coinflip() ? BLUE : LIGHTBLUE);
            break;

        case BEAM_POTION_BLUE_SMOKE:
            beam.colour = LIGHTBLUE;
            break;

        case BEAM_POTION_PURP_SMOKE:
            beam.colour = MAGENTA;
            break;

        case BEAM_POTION_RANDOM:
        default:
            // Leave it the colour of the potion, the clouds will colour
            // themselves on the next refresh. -- bwr
            break;
        }

        explosion1(beam);
        return;
    }

    if (beam.is_tracer)
        return;

    // cloud producer -- POISON BLAST
    if (beam.name == "blast of poison")
    {
        big_cloud( CLOUD_POISON, whose_kill(beam), x, y, 0, 7 + random2(5) );
        return;
    }

    // cloud producer -- FOUL VAPOR (SWAMP DRAKE?)
    if (beam.name == "foul vapour")
    {
        cl_type = beam.flavour == BEAM_MIASMA? CLOUD_MIASMA : CLOUD_STINK;
        big_cloud( cl_type, whose_kill(beam), x, y, 0, 9 );
        return;
    }

    if (beam.name == "freezing blast")
    {
        big_cloud( CLOUD_COLD, whose_kill(beam), x, y,
                   random_range(10, 15), 9 );
        return;
    }

    // special cases - orbs & blasts of cold
    if (beam.name == "orb of electricity"
        || beam.name == "metal orb"
        || beam.name == "great blast of cold")
    {
        explosion1( beam );
        return;
    }

    // cloud producer only -- stinking cloud
    if (beam.name == "ball of vapour")
    {
        explosion1( beam );
        return;
    }
}

static bool beam_term_on_target(bolt &beam, int x, int y)
{
    if (beam.flavour == BEAM_LINE_OF_SIGHT)
    {
        beam.foe_count++;
        return (true);
    }

    // generic - all explosion-type beams can be targeted at empty space,
    // and will explode there.  This semantic also means that a creature
    // in the target cell will have no chance to dodge or block, so we
    // DON'T affect() the cell if this function returns true!

    if (beam.is_explosion || beam.is_big_cloud)
        return (true);

    // POISON BLAST
    if (beam.name == "blast of poison")
        return (true);

    // FOUL VAPOR (SWAMP DRAKE)
    if (beam.name == "foul vapour")
        return (true);

    // STINKING CLOUD
    if (beam.name == "ball of vapour")
        return (true);

    if (beam.aimed_at_spot && x == beam.target_x && y == beam.target_y)
        return (true);

    return (false);
}

void beam_drop_object( bolt &beam, item_def *item, int x, int y )
{
    ASSERT( item != NULL );

    // conditions: beam is missile and not tracer.
    if (beam.is_tracer || beam.flavour != BEAM_MISSILE)
        return;

    if ( grid_destroys_items(grd[x][y]) )
    {
        // Too much message spam otherwise
        if ( YOU_KILL(beam.thrower) && player_can_hear(x, y) )
            mprf(MSGCH_SOUND, grid_item_destruction_message(grd[x][y]));

        item_was_destroyed(*item, beam.beam_source);
        return;
    }

    // doesn't get destroyed by throwing
    if (item->sub_type == MI_THROWING_NET)
    {
        copy_item_to_grid( *item, x, y, 1 );
        
        // nobody there
        if ((you.x_pos != x || you.y_pos != y) && mgrd[x][y] == NON_MONSTER)
            return;
            
        // player or monster on position but hasn't been caught
        if (you.x_pos == x && you.y_pos == y && !you.attribute[ATTR_HELD]
            || mgrd[x][y] != NON_MONSTER && !mons_is_caught(&menv[mgrd[x][y]]))
        {
            return;
        }
        
        // somebody on square who HAS been caught, maybe by this net?
        mark_net_trapping(x,y);
        return;
    }

    if (YOU_KILL(beam.thrower)) // you threw it
    {
        int chance;

        // [dshaligram] Removed influence of Throwing on ammo preservation.
        // The effect is nigh impossible to perceive.
        switch (item->sub_type)
        {
        case MI_NEEDLE:
            chance = (get_ammo_brand(*item) == SPMSL_CURARE? 3 : 6);
            break;
        case MI_SLING_BULLET:
        case MI_STONE:  chance = 4; break;
        case MI_DART:   chance = 3; break;
        case MI_ARROW:  chance = 4; break;
        case MI_BOLT:   chance = 4; break;
        case MI_JAVELIN: chance = 10; break;

        case MI_LARGE_ROCK:
        default:
            chance = 25;
            break;
        }

        if (item->base_type != OBJ_MISSILES || !one_chance_in(chance))
            copy_item_to_grid( *item, x, y, 1 );
    }
    else if (MON_KILL(beam.thrower) && coinflip()) // monster threw it.
    {
        copy_item_to_grid( *item, x, y, 1 );
    }                           // if (thing_throw == 2) ...
}

// Returns true if the beam hits the player, fuzzing the beam if necessary
// for monsters without see invis firing tracers at the player.
static bool found_player(const bolt &beam, int x, int y)
{
    const bool needs_fuzz =
        beam.is_tracer && !beam.can_see_invis && you.invisible();
    const int dist = needs_fuzz? 2 : 0;

    return (grid_distance(x, y, you.x_pos, you.y_pos) <= dist);
}

int affect(bolt &beam, int x, int y)
{
    // extra range used by hitting something
    int rangeUsed = 0;

    // line of sight never affects anything
    if (beam.flavour == BEAM_LINE_OF_SIGHT)
        return (0);

    if (grid_is_solid(grd[x][y]))
    {
        if (beam.is_tracer)          // tracers always stop on walls.
            return (BEAM_STOP);

        if (affects_wall(beam, grd[x][y]))
        {
            rangeUsed += affect_wall(beam, x, y);
        }
        // if it's still a wall, quit - we can't do anything else to
        // a wall.  Otherwise effects (like clouds, etc) are still possible.
        if (grid_is_solid(grd[x][y]))
            return (rangeUsed);
    }

    // grd[x][y] will NOT be a wall for the remainder of this function.

    // if not a tracer, place clouds
    if (!beam.is_tracer)
        rangeUsed += affect_place_clouds(beam, x, y);

    // if player is at this location, try to affect unless term_on_target
    if (found_player(beam, x, y))
    {
        // Done this way so that poison blasts affect the target once (via
        // place_cloud) and explosion spells only affect the target once
        // (during the explosion phase, not an initial hit during the 
        // beam phase).
        if (!beam.is_big_cloud
            && (!beam.is_explosion || beam.in_explosion_phase))
        {
            rangeUsed += affect_player( beam );
        }
        
        if (beam_term_on_target(beam, x, y))
            return (BEAM_STOP);
    }

    // if there is a monster at this location, affect it
    // submerged monsters aren't really there -- bwr
    int mid = mgrd[x][y];
    if (mid != NON_MONSTER)
    {
        monsters *mons = &menv[mid];

        // Monsters submerged in shallow water can be targeted by beams
        // aimed at that spot.
        if (mons->alive()
            && (!mons->submerged()
                || (beam.aimed_at_spot && beam.target() == mons->pos()
                    && grd(mons->pos()) == DNGN_SHALLOW_WATER)))
        {
            if (!beam.is_big_cloud
                && (!beam.is_explosion || beam.in_explosion_phase))
            {
                rangeUsed += affect_monster( beam, &menv[mid] );
            }
        
            if (beam_term_on_target(beam, x, y))
                return (BEAM_STOP);
        }
    }

    return (rangeUsed);
}

static bool is_fiery(const bolt &beam)
{
    return (beam.flavour == BEAM_FIRE || beam.flavour == BEAM_HELLFIRE
            || beam.flavour == BEAM_LAVA);
}

static bool is_superhot(const bolt &beam)
{
    if (!is_fiery(beam))
        return (false);

    return beam.name == "bolt of fire"
        || beam.name == "bolt of magma"
        || (beam.name.find("hellfire") != std::string::npos
                && beam.in_explosion_phase);
}

static bool affects_wall(const bolt &beam, int wall)
{
    // digging
    if (beam.flavour == BEAM_DIGGING)
        return (true);

    // Isn't this much nicer than the hack to remove ice bolts, disrupt, 
    // and needles (just because they were also coloured "white") -- bwr
    if (beam.flavour == BEAM_DISINTEGRATION && beam.damage.num >= 3)
        return (true);

    if (is_fiery(beam) && wall == DNGN_WAX_WALL)
        return (true);

    // eye of devastation?
    if (beam.flavour == BEAM_NUKE)
        return (true);

    return (false);
}

// return amount of extra range used up by affectation of this wall.
static int affect_wall(bolt &beam, int x, int y)
{
    int rangeUsed = 0;

    // DIGGING
    if (beam.flavour == BEAM_DIGGING)
    {
        if (grd[x][y] == DNGN_STONE_WALL
            || grd[x][y] == DNGN_METAL_WALL
            || grd[x][y] == DNGN_PERMAROCK_WALL
            || grd[x][y] == DNGN_CLEAR_STONE_WALL
            || grd[x][y] == DNGN_CLEAR_PERMAROCK_WALL
            || !in_bounds(x, y))
        {
            return (0);
        }

        if (grd[x][y] == DNGN_ROCK_WALL || grd[x][y] == DNGN_CLEAR_ROCK_WALL)
        {
            grd[x][y] = DNGN_FLOOR;

            if (!beam.msg_generated)
            {
                if (!silenced(you.x_pos, you.y_pos))
                {
                    mpr("You hear a grinding noise.", MSGCH_SOUND);
                    beam.obvious_effect = true;
                }

                beam.msg_generated = true;
            }
        }

        return (rangeUsed);
    }
    // END DIGGING EFFECT
    
    // FIRE effect
    if (is_fiery(beam))
    {
        const int wgrd = grd[x][y];
        if (wgrd != DNGN_WAX_WALL)
            return (0);

        if (!is_superhot(beam))
        {
            if (beam.flavour != BEAM_HELLFIRE)
            {
                if (see_grid(x, y))
                    beam_mpr(MSGCH_PLAIN, 
                            "The wax appears to soften slightly.");
                else if (player_can_smell())
                    beam_mpr(MSGCH_PLAIN, "You smell warm wax.");
            }

            return (BEAM_STOP);
        }

        grd[x][y] = DNGN_FLOOR;
        if (see_grid(x, y))
            beam_mpr(MSGCH_PLAIN, "The wax bubbles and burns!");
        else if (player_can_smell())
            beam_mpr(MSGCH_PLAIN, "You smell burning wax.");

        place_cloud(CLOUD_FIRE, x, y, random2(10) + 15, whose_kill(beam));

        beam.obvious_effect = true;

        return (BEAM_STOP);
    }

    // NUKE / DISRUPT
    if (beam.flavour == BEAM_DISINTEGRATION || beam.flavour == BEAM_NUKE)
    {
        int targ_grid = grd[x][y];

        if ((targ_grid == DNGN_ROCK_WALL || targ_grid == DNGN_WAX_WALL
             || targ_grid == DNGN_CLEAR_ROCK_WALL)
            && in_bounds(x, y))
        {
            grd[ x ][ y ] = DNGN_FLOOR;
            if (!silenced(you.x_pos, you.y_pos))
            {
                mpr("You hear a grinding noise.", MSGCH_SOUND);
                beam.obvious_effect = true;
            }
        }

        if (targ_grid == DNGN_ORCISH_IDOL 
                || targ_grid == DNGN_GRANITE_STATUE)
        {
            grd[x][y] = DNGN_FLOOR;

            if (!silenced(you.x_pos, you.y_pos))
            {
                if (!see_grid( x, y ))
                    mpr("You hear a hideous screaming!", MSGCH_SOUND);
                else
                    mpr("The statue screams as its substance crumbles away!",
                            MSGCH_SOUND);
            }
            else
            {
                if (see_grid(x,y))
                    mpr("The statue twists and shakes as its substance crumbles away!");
            }

            if (targ_grid == DNGN_ORCISH_IDOL
                && beam.beam_source == NON_MONSTER)
            {
                beogh_idol_revenge();
            }
            beam.obvious_effect = 1;
        }

        return (BEAM_STOP);
    }

    return (rangeUsed);
}

static int affect_place_clouds(bolt &beam, int x, int y)
{
    if (beam.in_explosion_phase)
    {
        affect_place_explosion_clouds( beam, x, y );
        return (0);       // return value irrelevant for explosions
    }

    // check for CLOUD HITS
    if (env.cgrid[x][y] != EMPTY_CLOUD)     // hit a cloud
    {
        // polymorph randomly changes clouds in its path
        if (beam.flavour == BEAM_POLYMORPH)
            env.cloud[ env.cgrid[x][y] ].type =
                static_cast<cloud_type>(1 + random2(8));

        // now exit (all enchantments)
        if (beam.name[0] == '0')
            return (0);

        int clouty = env.cgrid[x][y];

        // fire cancelling cold & vice versa
        if ((env.cloud[clouty].type == CLOUD_COLD
             && (beam.flavour == BEAM_FIRE
                 || beam.flavour == BEAM_LAVA))
            || (env.cloud[clouty].type == CLOUD_FIRE
                && beam.flavour == BEAM_COLD))
        {
            if (!silenced(x, y)
                && !silenced(you.x_pos, you.y_pos))
            {
                mpr("You hear a sizzling sound!", MSGCH_SOUND);
            }

            delete_cloud( clouty );
            return (5);
        }
    }

    // POISON BLAST
    if (beam.name == "blast of poison")
        place_cloud( CLOUD_POISON, x, y, random2(4) + 2, whose_kill(beam) );

    // FIRE/COLD over water/lava
    if ( (grd[x][y] == DNGN_LAVA && beam.flavour == BEAM_COLD)
        || (grid_is_watery(grd[x][y]) && beam.flavour == BEAM_FIRE) )
    {
        place_cloud( CLOUD_STEAM, x, y, 2 + random2(5), whose_kill(beam) );
    }

    if (beam.flavour == BEAM_COLD && grid_is_watery(grd[x][y]))
    {
        place_cloud( CLOUD_COLD, x, y, 2 + random2(5), whose_kill(beam) );
    }

    // ORB OF ENERGY
    if (beam.name == "orb of energy")
        place_cloud( CLOUD_PURP_SMOKE, x, y, random2(5) + 1, whose_kill(beam) );

    // GREAT BLAST OF COLD
    if (beam.name == "great blast of cold")
        place_cloud( CLOUD_COLD, x, y, random2(5) + 3, whose_kill(beam) );


    // BALL OF STEAM
    if (beam.name == "ball of steam")
    {
        place_cloud( CLOUD_STEAM, x, y, random2(5) + 2, whose_kill(beam) );
    }

    if (beam.flavour == BEAM_MIASMA)
    {
        place_cloud( CLOUD_MIASMA, x, y, random2(5) + 2, whose_kill(beam) );
    }

    // STICKY FLAME
    if (beam.name == "sticky flame")
    {
        place_cloud( CLOUD_BLACK_SMOKE, x, y, random2(4) + 2,
                     whose_kill(beam) );
    }

    // POISON GAS
    if (beam.name == "poison gas")
        place_cloud( CLOUD_POISON, x, y, random2(4) + 3, whose_kill(beam) );

    return (0);
}

// following two functions used with explosions:
static void affect_place_explosion_clouds(bolt &beam, int x, int y)
{
    cloud_type cl_type; 
    int duration;

    // first check: FIRE/COLD over water/lava
    if ( (grd[x][y] == DNGN_LAVA && beam.flavour == BEAM_COLD)
        || ((grd[x][y] == DNGN_DEEP_WATER || grd[x][y] == DNGN_SHALLOW_WATER)
              && beam.flavour == BEAM_FIRE) )
    {
        place_cloud( CLOUD_STEAM, x, y, 2 + random2(5), whose_kill(beam) );
        return;
    }

    if (beam.flavour >= BEAM_POTION_STINKING_CLOUD
        && beam.flavour <= BEAM_POTION_RANDOM)
    {
        duration = roll_dice( 2, 3 + beam.ench_power / 20 );

        switch (beam.flavour)
        {
        case BEAM_POTION_STINKING_CLOUD:
            cl_type = CLOUD_STINK;
            break;

        case BEAM_POTION_POISON:
            cl_type = CLOUD_POISON;
            break;

        case BEAM_POTION_MIASMA:
            cl_type = CLOUD_MIASMA;
            break;

        case BEAM_POTION_BLACK_SMOKE:
            cl_type = CLOUD_BLACK_SMOKE;
            break;

        case BEAM_POTION_FIRE:
            cl_type = CLOUD_FIRE;
            break;

        case BEAM_POTION_COLD:
            cl_type = CLOUD_COLD;
            break;

        case BEAM_POTION_BLUE_SMOKE:
            cl_type = CLOUD_BLUE_SMOKE;
            break;

        case BEAM_POTION_PURP_SMOKE:
            cl_type = CLOUD_PURP_SMOKE;
            break;

        case BEAM_POTION_RANDOM:
            switch (random2(10)) 
            {
            case 0:  cl_type = CLOUD_FIRE;           break;
            case 1:  cl_type = CLOUD_STINK;          break;
            case 2:  cl_type = CLOUD_COLD;           break;
            case 3:  cl_type = CLOUD_POISON;         break;
            case 4:  cl_type = CLOUD_BLACK_SMOKE;    break;
            case 5:  cl_type = CLOUD_BLUE_SMOKE;     break;
            case 6:  cl_type = CLOUD_PURP_SMOKE;     break;
            default: cl_type = CLOUD_STEAM;          break;
            }
            break;

        case BEAM_POTION_STEAM:
        default:
            cl_type = CLOUD_STEAM;
            break;
        }

        place_cloud( cl_type, x, y, duration, whose_kill(beam) );
    }

    // then check for more specific explosion cloud types.
    if (beam.name == "ice storm")
    {
        place_cloud( CLOUD_COLD, x, y, 2 + random2avg(5, 2), whose_kill(beam) );
    }

    if (beam.name == "stinking cloud")
    {
        duration =  1 + random2(4) + random2( (beam.ench_power / 50) + 1 );
        place_cloud( CLOUD_STINK, x, y, duration, whose_kill(beam) );
    }

    if (beam.name == "great blast of fire")
    {
        duration = 1 + random2(5) + roll_dice( 2, beam.ench_power / 5 );

        if (duration > 20)
            duration = 20 + random2(4);

        place_cloud( CLOUD_FIRE, x, y, duration, whose_kill(beam) );

        if (grd[x][y] == DNGN_FLOOR && mgrd[x][y] == NON_MONSTER
            && one_chance_in(4))
        {
            mons_place( MONS_FIRE_VORTEX, BEH_HOSTILE, MHITNOT, true, x, y );
        }
    }
}

static void affect_items(bolt &beam, int x, int y)
{
    char objs_vulnerable = -1;

    switch (beam.flavour)
    {
    case BEAM_FIRE:
    case BEAM_LAVA:
        objs_vulnerable = OBJ_SCROLLS;
        break;
    case BEAM_COLD:
        objs_vulnerable = OBJ_POTIONS;
        break;
    case BEAM_SPORE:
        objs_vulnerable = OBJ_FOOD;
        break;
    }

    if (beam.name == "hellfire")
        objs_vulnerable = OBJ_SCROLLS;

    if (igrd[x][y] != NON_ITEM)
    {
        if (objs_vulnerable != -1 &&
            mitm[igrd[x][y]].base_type == objs_vulnerable)
        {
            item_was_destroyed(mitm[igrd[x][y]], beam.beam_source);

            destroy_item( igrd[ x ][ y ] );

            if (objs_vulnerable == OBJ_SCROLLS && see_grid(x,y))
            {
                mpr("You see a puff of smoke.");
            }

            if (objs_vulnerable == OBJ_POTIONS && !silenced(x,y)
                && !silenced(you.x_pos, you.y_pos))
            {
                mpr("You hear glass shatter.");
            }
        }
    }
}

static int beam_ouch_agent(const bolt &beam)
{
    return YOU_KILL(beam.thrower)? 0 : beam.beam_source;
}

// A little helper function to handle the calling of ouch()...
static void beam_ouch( int dam, bolt &beam )
{
    // The order of this is important.
    if (YOU_KILL( beam.thrower ) && beam.aux_source.empty())
    {
        ouch( dam, 0, KILLED_BY_TARGETTING );
    }
    else if (MON_KILL( beam.thrower ))
    {
        if (beam.flavour == BEAM_SPORE)
            ouch( dam, beam.beam_source, KILLED_BY_SPORE );
        else
            ouch( dam, beam.beam_source, KILLED_BY_BEAM, 
                  beam.aux_source.c_str() );
    }
    else // KILL_MISC || (YOU_KILL && aux_source)
    {
        ouch( dam, beam.beam_source, KILLED_BY_WILD_MAGIC, 
              beam.aux_source.c_str() );
    }
}

// [ds] Apply a fuzz if the monster lacks see invisible and is trying to target
// an invisible player. This makes invisibility slightly more powerful.
static bool fuzz_invis_tracer(bolt &beem)
{
    // Did the monster have a rough idea of where you are?
    int dist = grid_distance(beem.target_x, beem.target_y, 
                             you.x_pos, you.y_pos);

    // No, ditch this.
    if (dist > 2)
        return (false);

    const int beam_src = beam_source(beem);
    if (beam_src != MHITNOT && beam_src != MHITYOU)
    {
        // Monsters that can sense invisible 
        const monsters *mon = &menv[beam_src];
        if (mons_sense_invis(mon))
            return (!dist);
    }

    // Apply fuzz now.
    int xfuzz = random_range(-2, 2),
        yfuzz = random_range(-2, 2);

    const int newx = beem.target_x + xfuzz,
              newy = beem.target_y + yfuzz;
    if (in_bounds(newx, newy)
            && (newx != beem.source_x
                || newy != beem.source_y))
    {
        beem.target_x = newx;
        beem.target_y = newy;
    }

    // Fire away!
    return (true);
}

// A first step towards to-hit sanity for beams. We're still being
// very kind to the player, but it should be fairer to monsters than
// 4.0.
bool test_beam_hit(int attack, int defence)
{
#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Beam attack: %d, defence: %d", attack, defence);
#endif
    return (attack == AUTOMATIC_HIT
            || random2(attack) >= random2avg(defence, 2));
}

static std::string beam_zapper(const bolt &beam)
{
    const int bsrc = beam_source(beam);
    if (bsrc == MHITYOU)
        return ("self");
    else if (bsrc == MHITNOT)
        return ("");
    else
        return menv[bsrc].name(DESC_PLAIN);
}

// return amount of extra range used up by affectation of the player
static int affect_player( bolt &beam )
{
    // digging -- don't care.
    if (beam.flavour == BEAM_DIGGING)
        return (0);

    // check for tracer
    if (beam.is_tracer)
    {
        // check can see player 
        if (beam.can_see_invis || !you.invisible()
                || fuzz_invis_tracer(beam))
        {
            if (beam.attitude != ATT_HOSTILE)
            {
                beam.fr_count += 1;
                beam.fr_power += you.experience_level;
            }
            else
            {
                beam.foe_count++;
                beam.foe_power += you.experience_level;
            }
        }
        return (range_used_on_hit(beam));
    }

    // BEGIN real beam code
    beam.msg_generated = true;

    // use beamHit, NOT beam.hit, for modification of tohit.. geez!
    int beamHit = beam.hit;

    // Monsters shooting at an invisible player are very inaccurate.
    if (you.invisible() && !beam.can_see_invis)
        beamHit /= 2;

    if (beam.name[0] != '0') 
    {
        if (!beam.is_explosion && !beam.aimed_at_feet)
        {
            // BEGIN BEAM/MISSILE
            int dodge = player_evasion();

            if (beam.is_beam)
            {
                // beams can be dodged
                if (player_light_armour(true)
                    && !beam.aimed_at_feet && coinflip())
                {
                    exercise(SK_DODGING, 1);
                }

                if (you.duration[DUR_DEFLECT_MISSILES])
                    beamHit = random2(beamHit * 2) / 3;
                else if (you.duration[DUR_REPEL_MISSILES] ||
                         you.mutation[MUT_REPULSION_FIELD] == 3)
                    beamHit -= random2(beamHit / 2);

                if (!test_beam_hit(beamHit, dodge))
                {
                    mprf("The %s misses you.", beam.name.c_str());
                    return (0);           // no extra used by miss!
                }
            }
            else if (beam_is_blockable(beam))
            {
                // non-beams can be blocked or dodged
                if (you.equip[EQ_SHIELD] != -1 
                        && !beam.aimed_at_feet
                        && player_shield_class() > 0)
                {
                    int exer = one_chance_in(3) ? 1 : 0;
                    const int hit = random2( beam.hit * 130 / 100
                                             + you.shield_block_penalty() );

                    const int block = you.shield_bonus();

#ifdef DEBUG_DIAGNOSTICS
                    mprf(MSGCH_DIAGNOSTICS, "Beamshield: hit: %d, block %d",
                         hit, block);
#endif
                    if (hit < block)
                    {
                        you.shield_blocks++;
                        mprf( "You block the %s.", beam.name.c_str() );
                        exercise( SK_SHIELDS, exer + 1 );
                        return (BEAM_STOP);
                    }

                    // some training just for the "attempt"
                    exercise( SK_SHIELDS, exer );
                }

                if (player_light_armour(true) && !beam.aimed_at_feet
                    && coinflip())
                    exercise(SK_DODGING, 1);

                if (you.duration[DUR_DEFLECT_MISSILES])
                    beamHit = random2(beamHit / 2);
                else if (you.duration[DUR_REPEL_MISSILES] ||
                         you.mutation[MUT_REPULSION_FIELD] == 3)
                    beamHit = random2(beamHit);


                // miss message
                if (!test_beam_hit(beamHit, dodge))
                {
                    mprf("The %s misses you.", beam.name.c_str());
                    return (0);
                }
            }
        }
    }
    else
    {
        bool nasty = true, nice = false;

        // BEGIN enchantment beam
        if (beam.flavour != BEAM_HASTE 
            && beam.flavour != BEAM_INVISIBILITY
            && beam.flavour != BEAM_HEALING
            && beam.flavour != BEAM_POLYMORPH
            && ((beam.flavour != BEAM_TELEPORT && beam.flavour != BEAM_BANISH)
                || !beam.aimed_at_feet)
            && you_resist_magic( beam.ench_power ))
        {
            canned_msg(MSG_YOU_RESIST);

            // You *could* have gotten a free teleportation in the Abyss,
            // but no, you resisted.
            if (beam.flavour != BEAM_TELEPORT && you.level_type == LEVEL_ABYSS)
                xom_is_stimulated(255);

            return (range_used_on_hit(beam));
        }

        ench_animation( beam.flavour );
        
        // these colors are misapplied - see mons_ench_f2() {dlb}
        switch (beam.flavour)
        {
        case BEAM_SLEEP:
            you.put_to_sleep(beam.ench_power);
            break;
            
        case BEAM_BACKLIGHT:
            if (!you.duration[DUR_INVIS])
            {
                if (you.duration[DUR_BACKLIGHT])
                    mpr("You glow brighter.");
                else
                    mpr("You are outlined in light.");
                
                you.duration[DUR_BACKLIGHT] += random_range(15, 35);
                if (you.duration[DUR_BACKLIGHT] > 250)
                    you.duration[DUR_BACKLIGHT] = 250;

                beam.obvious_effect = true;
            }
            else
            {
                mpr("You feel strangely conspicuous.");
                if ((you.duration[DUR_BACKLIGHT] += random_range(3, 5)) > 250)
                    you.duration[DUR_BACKLIGHT] = 250;
                beam.obvious_effect = true;
            }
            break;
            
        case BEAM_POLYMORPH:
            if (MON_KILL(beam.thrower))
            {
                mpr("Strange energies course through your body.");
                you.mutate();
                beam.obvious_effect = true;
            }
            else if (get_ident_type(OBJ_WANDS, WAND_POLYMORPH_OTHER)
                     == ID_KNOWN_TYPE)
            {
                mpr("This is polymorph other only!");
            }
            else
            {
                canned_msg( MSG_NOTHING_HAPPENS );
            }
            break;

        case BEAM_SLOW:
            potion_effect( POT_SLOWING, beam.ench_power );
            beam.obvious_effect = true;
            break;     // slow

        case BEAM_HASTE:
            potion_effect( POT_SPEED, beam.ench_power );
            contaminate_player( 1 );
            beam.obvious_effect = true;
            nasty = false;
            nice  = true;
            break;     // haste

        case BEAM_HEALING:
            potion_effect( POT_HEAL_WOUNDS, beam.ench_power );
            beam.obvious_effect = true;
            nasty = false;
            nice  = true;
            break;     // heal (heal wounds potion eff)

        case BEAM_PARALYSIS:
            potion_effect( POT_PARALYSIS, beam.ench_power );
            beam.obvious_effect = true;
            break;     // paralysis

        case BEAM_CONFUSION:
            potion_effect( POT_CONFUSION, beam.ench_power );
            beam.obvious_effect = true;
            break;     // confusion

        case BEAM_INVISIBILITY:
            potion_effect( POT_INVISIBILITY, beam.ench_power );
            contaminate_player( 1 + random2(2) );
            beam.obvious_effect = true;
            nasty = false;
            nice  = true;
            break;     // invisibility

            // 6 is used by digging

        case BEAM_TELEPORT:
            you_teleport();

            // An enemy helping you escape while in the Abyss, or an
            // enemy stabilizing a teleport that was about to happen.
            if (beam.attitude == ATT_HOSTILE && you.level_type == LEVEL_ABYSS)
                xom_is_stimulated(255);

            beam.obvious_effect = true;
            break;

        case BEAM_BLINK:
            random_blink(false);
            beam.obvious_effect = true;
            break;

        case BEAM_CHARM:
            potion_effect( POT_CONFUSION, beam.ench_power );
            beam.obvious_effect = true;
            break;     // enslavement - confusion?

        case BEAM_BANISH:
            if (you.level_type == LEVEL_ABYSS)
            {
                mpr("You feel trapped.");
                break;
            }
            you.banished = true;
            you.banished_by = beam_zapper(beam);
            beam.obvious_effect = true;
            break;     // banishment to the abyss

        case BEAM_PAIN:      // pain
            if (player_res_torment())
            {
                mpr("You are unaffected.");
                break;
            }

            mpr("Pain shoots through your body!");

            if (beam.aux_source.empty())
                beam.aux_source = "by nerve-wracking pain";

            beam_ouch( roll_dice( beam.damage ), beam );
            beam.obvious_effect = true;
            break;

        case BEAM_DISPEL_UNDEAD:
            if (!you.is_undead)
            {
                mpr("You are unaffected.");
                break;
            }

            mpr( "You convulse!" );

            if (beam.aux_source.empty())
                beam.aux_source = "by dispel undead";

            beam_ouch( roll_dice( beam.damage ), beam );
            beam.obvious_effect = true;
            break;

        case BEAM_DISINTEGRATION:
            mpr("You are blasted!");

            if (beam.aux_source.empty())
                beam.aux_source = "disintegration bolt";

            beam_ouch( roll_dice( beam.damage ), beam );
            beam.obvious_effect = true;
            break;

        default:
            // _all_ enchantments should be enumerated here!
            mpr("Software bugs nibble your toes!");
            break;
        }               // end of switch (beam.colour)

        if (nasty)
        {
            if (beam.attitude != ATT_HOSTILE)
            {
                beam.fr_hurt++;
                if (beam.beam_source == NON_MONSTER)
                    // Beam from player rebounded and hit player
                    xom_is_stimulated(255);
                else
                    // Beam from an ally
                    xom_is_stimulated(128);
            }
            else
                beam.foe_hurt++;
        }

        if (nice)
        {
            if (beam.attitude != ATT_HOSTILE)
                beam.fr_helped++;
            else
            {
                beam.foe_helped++;
                xom_is_stimulated(128);
            }
        }

        // regardless of affect, we need to know if this is a stopper
        // or not - it seems all of the above are.
        return (range_used_on_hit(beam));

        // END enchantment beam
    }

    // THE BEAM IS NOW GUARANTEED TO BE A NON-ENCHANTMENT WHICH HIT

    const bool engulfs = (beam.is_explosion || beam.is_big_cloud);
    mprf( "The %s %s you!", 
            beam.name.c_str(), (engulfs) ? "engulfs" : "hits" );

    int hurted = 0;
    int burn_power = (beam.is_explosion) ? 5 : ((beam.is_beam) ? 3 : 2);

    // Roll the damage
    hurted += roll_dice( beam.damage ); 

#if DEBUG_DIAGNOSTICS
    int roll = hurted;
#endif

    int armour_damage_reduction = random2( 1 + player_AC() );
    if (beam.flavour == BEAM_ELECTRICITY)
        armour_damage_reduction /= 2;
    hurted -= armour_damage_reduction;

    // shrapnel
    if (beam.flavour == BEAM_FRAG && !player_light_armour())
    {
        hurted -= random2( 1 + player_AC() );
        hurted -= random2( 1 + player_AC() );
    }

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS,
         "Player damage: rolled=%d; after AC=%d", roll, hurted );
#endif

    if (you.equip[EQ_BODY_ARMOUR] != -1)
    {
        if (!player_light_armour(false) && one_chance_in(4)
            && random2(1000) <= item_mass( you.inv[you.equip[EQ_BODY_ARMOUR]] ))
        {
            exercise( SK_ARMOUR, 1 );
        }
    }

    bool was_affected = false;
    int  old_hp       = you.hp;

    if (hurted < 0)
        hurted = 0;

    hurted = check_your_resists( hurted, beam.flavour );

    if (beam.flavour == BEAM_MIASMA && hurted > 0)
    {
        if (player_res_poison() <= 0)
        {
            poison_player(1);
            was_affected = true;
        }

        if (one_chance_in( 3 + 2 * player_prot_life() ))
        {
            potion_effect( POT_SLOWING, 5 );
            was_affected = true;
        }
    }

    // poisoning
    if (beam.name.find("poison") != std::string::npos
        && beam.flavour != BEAM_POISON 
        && beam.flavour != BEAM_POISON_ARROW
        && !player_res_poison())
    {
        if (hurted || (beam.ench_power == AUTOMATIC_HIT
                        && random2(100) < 90 - (3 * player_AC())))
        {
            poison_player( 1 + random2(3) );
            was_affected = true;
        }
    }
    
    if (beam.name.find("throwing net") != std::string::npos)
    {
        player_caught_in_net();
        was_affected = true;
    }

    if (beam.name.find("curare") != std::string::npos)
    {
        if (random2(100) < 90 - (3 * player_AC()))
        {
            curare_hits_player( beam_ouch_agent(beam), 1 + random2(3) );
            was_affected = true;
        }
    }

    // sticky flame
    if (beam.name == "sticky flame"
        && (you.species != SP_MOTTLED_DRACONIAN
            || you.experience_level < 6))
    {
        if (!player_equip( EQ_BODY_ARMOUR, ARM_MOTTLED_DRAGON_ARMOUR ))
        {
            you.duration[DUR_LIQUID_FLAMES] += random2avg(7, 3) + 1;
            was_affected = true;
        }
    }

    // simple cases for scroll burns
    if (beam.flavour == BEAM_LAVA || beam.name == "hellfire")
        expose_player_to_element(BEAM_LAVA, burn_power);

    // more complex (geez..)
    if (beam.flavour == BEAM_FIRE && beam.name != "ball of steam")
        expose_player_to_element(BEAM_FIRE, burn_power);

    // potions exploding
    if (beam.flavour == BEAM_COLD)
        expose_player_to_element(BEAM_COLD, burn_power);

    if (beam.flavour == BEAM_ACID)
        splash_with_acid(5);

    // spore pops
    if (beam.in_explosion_phase && beam.flavour == BEAM_SPORE)
        expose_player_to_element(BEAM_SPORE, burn_power);

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Damage: %d", hurted );
#endif

    if (hurted > 0 || old_hp < you.hp || was_affected)
    {
        if (beam.attitude != ATT_HOSTILE)
        {
            beam.fr_hurt++;

            // Beam from player rebounded and hit player
            if (beam.beam_source == NON_MONSTER)
                xom_is_stimulated(255);
            // Xom's amusement at the player being damaged is handled
            // elsewhere.
            else if (was_affected)
                xom_is_stimulated(128);
        }
        else
            beam.foe_hurt++;
    }

    beam_ouch( hurted, beam );

    return (range_used_on_hit( beam ));
}

static int beam_source(const bolt &beam)
{
    return MON_KILL(beam.thrower)       ? beam.beam_source : 
           beam.thrower == KILL_MISC    ? MHITNOT          :
                                          MHITYOU;
}

static int name_to_skill_level(const std::string& name)
{
    skill_type type = SK_THROWING;

    if (name.find("dart") != std::string::npos)
        type = SK_DARTS;
    else if (name.find("needle") != std::string::npos)
        type = SK_DARTS;
    else if (name.find("bolt") != std::string::npos)
        type = SK_CROSSBOWS;
    else if (name.find("arrow") != std::string::npos)
        type = SK_BOWS;
    else if (name.find("stone") != std::string::npos)
        type = SK_SLINGS;

    if (type == SK_DARTS || type == SK_SLINGS)
        return (you.skills[type] + you.skills[SK_THROWING]);
        
    return (2 * you.skills[type]);
}

static void update_hurt_or_helped(bolt &beam, monsters *mon)
{
    if (beam.attitude != mons_attitude(mon))
    {
        if (nasty_beam(mon, beam))
            beam.foe_hurt++;
        else if (nice_beam(mon, beam))
            beam.foe_helped++;
    }
    else
    {
        if (nasty_beam(mon, beam))
        {
            beam.fr_hurt++;

            // Harmful beam from this monster rebounded and hit the monster
            int midx = (int) monster_index(mon);
            if (midx == beam.beam_source)
                xom_is_stimulated(128);
        }
        else if (nice_beam(mon, beam))
            beam.fr_helped++;
    }
}

// return amount of range used up by affectation of this monster
static int affect_monster(bolt &beam, monsters *mon)
{
    const int tid = mgrd[mon->x][mon->y];
    const int mons_type = menv[tid].type;
    const int thrower = YOU_KILL(beam.thrower)? KILL_YOU_MISSILE 
                                              : KILL_MON_MISSILE;
    const bool submerged = mon->submerged();

    int hurt;
    int hurt_final;

    // digging -- don't care.
    if (beam.flavour == BEAM_DIGGING)
        return (0);

    // fire storm creates these, so we'll avoid affecting them
    if (beam.name == "great blast of fire"
        && mon->type == MONS_FIRE_VORTEX)
    {
        return (0);
    }

    // check for tracer
    if (beam.is_tracer)
    {
        // check can see other monster
        if (!beam.can_see_invis && menv[tid].invisible())
        {
            // can't see this monster, ignore it
            return 0;
        }
    }
    else if ((beam.flavour == BEAM_DISINTEGRATION || beam.flavour == BEAM_NUKE)
             && mons_is_statue(mons_type) && !mons_is_icy(mons_type))
    {
        if (!silenced(you.x_pos, you.y_pos))
        {
            if (!see_grid( mon->x, mon->y ))
                mpr("You hear a hideous screaming!", MSGCH_SOUND);
            else
                mpr("The statue screams as its substance crumbles away!",
                        MSGCH_SOUND);
        }
        else
        {
            if (see_grid( mon->x, mon->y ))
                mpr("The statue twists and shakes as its substance "
                        "crumbles away!");
        }
        beam.obvious_effect = true;
        update_hurt_or_helped(beam, mon);
        mon->hit_points = 0;
        monster_die(mon, beam);
        return (BEAM_STOP);
    }

    if (beam.name[0] == '0')
    {
        if (beam.is_tracer)
        {
            // enchant case -- enchantments always hit, so update target immed.
            if (beam.attitude != mons_attitude(mon))
            {
                beam.foe_count += 1;
                beam.foe_power += mons_power(mons_type);
            }
            else
            {
                beam.fr_count += 1;
                beam.fr_power += mons_power(mons_type);
            }

            return (range_used_on_hit(beam));
        }

        // BEGIN non-tracer enchantment

        // Submerged monsters are unaffected by enchantments.
        if (submerged)
            return (0);
        
        // nasty enchantments will annoy the monster, and are considered
        // naughty (even if a monster might resist)
        if (nasty_beam(mon, beam))
        {
            if (YOU_KILL( beam.thrower ))
            {
                if (mons_friendly( mon ))
                    did_god_conduct( DID_ATTACK_FRIEND, 5, mon );

                if (mons_holiness( mon ) == MH_HOLY)
                    did_god_conduct( DID_ATTACK_HOLY, mon->hit_dice, mon );
            }

            behaviour_event( mon, ME_ANNOY, beam_source(beam) );
        }
        else
        {
            behaviour_event( mon, ME_ALERT, beam_source(beam) );
        }

        // !@#*( affect_monster_enchantment() has side-effects on
        // the beam structure which screw up range_used_on_hit(),
        // so call it now and store.
        int rangeUsed = range_used_on_hit(beam);

        // Doing this here so that the player gets to see monsters
        // "flicker and vanish" when turning invisible....
        ench_animation( beam.flavour, mon );

        // now do enchantment affect
        int ench_result = affect_monster_enchantment(beam, mon);
        if (mon->alive())
        {
            switch (ench_result)
            {
            case MON_RESIST:
                if (simple_monster_message(mon, " resists."))
                    beam.msg_generated = true;
                break;
            case MON_UNAFFECTED:
                if (simple_monster_message(mon, " is unaffected."))
                    beam.msg_generated = true;
                break;
            default:
                update_hurt_or_helped(beam, mon);
                break;
            }
        }
        return (rangeUsed);

        // END non-tracer enchantment
    }


    // BEGIN non-enchantment (could still be tracer)
    if (submerged && !beam.aimed_at_spot)
        return (0);                   // missed me!

    // we need to know how much the monster _would_ be hurt by this, before
    // we decide if it actually hits.

    // Roll the damage:
    hurt = roll_dice( beam.damage );

    // Water absorbs some of the damage for submerged monsters.
    if (submerged)
    {
        // Can't hurt submerged water creatures with electricity.
        if (beam.flavour == BEAM_ELECTRICITY)
        {
            if (see_grid(mon->x, mon->y))
                mprf("The %s arcs harmlessly into the water.",
                     beam.name.c_str());
            return (BEAM_STOP);
        }

        hurt = hurt * 2 / 3;
    }

    hurt_final = hurt;

    if (beam.is_tracer)
        hurt_final -= mon->ac / 2;
    else
        hurt_final -= random2(1 + mon->ac);

    if (beam.flavour == BEAM_FRAG)
    {
        hurt_final -= random2(1 + mon->ac);
        hurt_final -= random2(1 + mon->ac);
    }

    if (hurt_final < 1)
    {
        hurt_final = 0;
    }

    const int raw_damage = hurt_final;

    // check monster resists, _without_ side effects (since the
    // beam/missile might yet miss!)
    hurt_final = mons_adjust_flavoured( mon, beam, raw_damage, false );

#if DEBUG_DIAGNOSTICS
    if (!beam.is_tracer)
    {
        mprf(MSGCH_DIAGNOSTICS,
             "Monster: %s; Damage: pre-AC: %d; post-AC: %d; post-resist: %d", 
             mon->name(DESC_PLAIN).c_str(), hurt, raw_damage, hurt_final);
    }
#endif

    // now, we know how much this monster would (probably) be
    // hurt by this beam.
    if (beam.is_tracer)
    {
        if (hurt_final != 0)
        {
            // monster could be hurt somewhat, but only apply the
            // monster's power based on how badly it is affected.
            // For example, if a fire giant (power 16) threw a
            // fireball at another fire giant, and it only took
            // 1/3 damage, then power of 5 would be applied to
            // foe_power or fr_power.
            if (beam.attitude != mons_attitude(mon))
            {
                beam.foe_count += 1;
                beam.foe_power += 2 * hurt_final * mons_power(mons_type) / hurt;
            }
            else
            {
                beam.fr_count += 1;
                beam.fr_power += 2 * hurt_final * mons_power(mons_type) / hurt;
            }
        }
        // either way, we could hit this monster, so return range used
        return (range_used_on_hit(beam));
    }
    // END non-enchantment (could still be tracer)

    // BEGIN real non-enchantment beam

    // player beams which hit friendly will annoy them and be
    // considered naughty if they do damage (this is so as not to
    // penalize players that fling fireballs into a melee with fire
    // elementals on their side - the elementals won't give a sh*t,
    // after all)

    if (nasty_beam(mon, beam))
    {
        if (YOU_KILL(beam.thrower) && hurt_final > 0)
        {
            if (mons_friendly(mon))
                did_god_conduct( DID_ATTACK_FRIEND, 5, mon );

            if (mons_holiness( mon ) == MH_HOLY)
                did_god_conduct( DID_ATTACK_HOLY, mon->hit_dice, mon );
        }

        // Don't annoy friendlies if the player's beam did no damage.
        // Hostiles will still take umbrage.
        if (hurt_final > 0 || !mons_friendly(mon) || !YOU_KILL(beam.thrower))
            behaviour_event(mon, ME_ANNOY, beam_source(beam) );
    }

    // explosions always 'hit'
    const bool engulfs = (beam.is_explosion || beam.is_big_cloud);

    int beam_hit = beam.hit;
    if (menv[tid].invisible() && !beam.can_see_invis)
        beam_hit /= 2;

    // FIXME We're randomising mon->evasion, which is further
    // randomised inside test_beam_hit. This is so we stay close to the 4.0
    // to-hit system (which had very little love for monsters).
    if (!engulfs && !test_beam_hit(beam_hit, random2(mon->ev)))
    {
        // if the PLAYER cannot see the monster, don't tell them anything!
        if (player_monster_visible( &menv[tid] ) && mons_near(mon))
        {
            msg::stream << "The " << beam.name << " misses "
                        << mon->name(DESC_NOCAP_THE) << '.' << std::endl;
        }
        return (0);
    }

    update_hurt_or_helped(beam, mon);

    // the beam hit.
    if (mons_near(mon))
    {
        mprf("The %s %s %s.",
             beam.name.c_str(),
             engulfs? "engulfs" : "hits",
             player_monster_visible(&menv[tid])? 
             mon->name(DESC_NOCAP_THE).c_str()
             : "something");
    }
    else
    {
        // the player might hear something,
        // if _they_ fired a missile (not beam)
        if (!silenced(you.x_pos, you.y_pos) && beam.flavour == BEAM_MISSILE
                && YOU_KILL(beam.thrower))
            mprf(MSGCH_SOUND, "The %s hits something.", beam.name.c_str());
    }

    if (beam.name.find("throwing net") != std::string::npos)
        monster_caught_in_net(mon, beam);

    // note that hurt_final was calculated above, so we don't need it again.
    // just need to apply flavoured specials (since we called with
    // doFlavouredEffects = false above)
    hurt_final = mons_adjust_flavoured(mon, beam, raw_damage);

    // now hurt monster
    hurt_monster( mon, hurt_final );

    if (mon->hit_points < 1)
    {
        monster_die(mon, beam);
    }
    else
    {
        if (thrower == KILL_YOU_MISSILE && mons_near(mon))
        {
            const monsters *mons = static_cast<const monsters*>(mon);
            print_wounds(mons);
        }

        // sticky flame
        if (beam.name == "sticky flame")
        {
            int levels = 1 + random2( hurt_final ) / 2;
            if (levels > 4)
                levels = 4;

            sticky_flame_monster( tid, whose_kill(beam), levels );
        }


        /* looks for missiles which aren't poison but
           are poison*ed* */
        if (beam.name.find("poison") != std::string::npos
            && beam.flavour != BEAM_POISON
            && beam.flavour != BEAM_POISON_ARROW)
        {
            int num_levels = 0;
            // ench_power == AUTOMATIC_HIT if this is a poisoned needle.
            if (beam.ench_power == AUTOMATIC_HIT
                && random2(100) < 90 - (3 * mon->ac))
            {
                num_levels = 2;
            }
            else if (random2(hurt_final) - random2(mon->ac) > 0)
            {
                num_levels = 1;
            }

            int num_success = 0;
            if ( YOU_KILL(beam.thrower) )
            {
                const int skill_level = name_to_skill_level(beam.name);
                if ( skill_level + 25 > random2(50) )
                    num_success++;
                if ( skill_level > random2(50) )
                    num_success++;
            }
            else
                num_success = 1;

            if ( num_success )
            {
                if ( num_success == 2 )
                    num_levels++;
                poison_monster( mon, whose_kill(beam), num_levels );
            }
        }

        bool wake_mimic = true;
        if (beam.name.find("curare") != std::string::npos)
        {
            if (curare_hits_monster( beam, mon, whose_kill(beam), 2 ))
                wake_mimic = false;
        }

        if (wake_mimic && mons_is_mimic( mon->type ))
            mimic_alert(mon);
    }

    return (range_used_on_hit(beam));
}

static int affect_monster_enchantment(bolt &beam, monsters *mon)
{
    bool death_check = false;

    if (beam.flavour == BEAM_TELEPORT) // teleportation
    {
        if (check_mons_resist_magic( mon, beam.ench_power )
            && !beam.aimed_at_feet)
        {
            return mons_immune_magic(mon) ? MON_UNAFFECTED : MON_RESIST;
        }

        if (mons_near(mon) && player_monster_visible(mon))
            beam.obvious_effect = true;

        monster_teleport(mon, false);

        return (MON_AFFECTED);
    }

    if (beam.flavour == BEAM_BLINK)
    {
        if (!beam.aimed_at_feet
            && check_mons_resist_magic( mon, beam.ench_power ))
        {
            return mons_immune_magic(mon) ? MON_UNAFFECTED : MON_RESIST;
        }

        if (mons_near( mon ) && player_monster_visible( mon ))
            beam.obvious_effect = true;

        monster_blink( mon );
        return (MON_AFFECTED);
    }

    if (beam.flavour == BEAM_POLYMORPH)
    {
        if (mons_holiness( mon ) != MH_NATURAL)
            return (MON_UNAFFECTED);

        if (check_mons_resist_magic( mon, beam.ench_power ))
            return mons_immune_magic(mon) ? MON_UNAFFECTED : MON_RESIST;

        if (monster_polymorph(mon, RANDOM_MONSTER))
            beam.obvious_effect = true;

        return (MON_AFFECTED);
    }

    if (beam.flavour == BEAM_BANISH)
    {
        if (check_mons_resist_magic( mon, beam.ench_power ))
            return mons_immune_magic(mon) ? MON_UNAFFECTED : MON_RESIST;

        if (you.level_type == LEVEL_ABYSS)
        {
            simple_monster_message(mon, " wobbles for a moment.");
        }
        else
            monster_die(mon, beam);

        beam.obvious_effect = true;
        return (MON_AFFECTED);
    }

    if (beam.flavour == BEAM_DEGENERATE)
    {
        if (mons_holiness(mon) != MH_NATURAL
            || mon->type == MONS_PULSATING_LUMP)
        {
            return (MON_UNAFFECTED);
        }

        if (check_mons_resist_magic( mon, beam.ench_power ))
            return mons_immune_magic(mon) ? MON_UNAFFECTED : MON_RESIST;

        if (monster_polymorph(mon, MONS_PULSATING_LUMP))
            beam.obvious_effect = true;

        return (MON_AFFECTED);
    }

    if (beam.flavour == BEAM_DISPEL_UNDEAD)
    {
        if (mons_holiness(mon) != MH_UNDEAD)
            return (MON_UNAFFECTED);

        if (simple_monster_message(mon, " convulses!"))
            beam.obvious_effect = true;

        hurt_monster( mon, roll_dice( beam.damage ) );

        death_check = true;
    }

    if (beam.flavour == BEAM_ENSLAVE_UNDEAD 
        && mons_holiness(mon) == MH_UNDEAD)
    {
#if DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS,
             "HD: %d; pow: %d", mon->hit_dice, beam.ench_power );
#endif

        if (mon->attitude == ATT_FRIENDLY)
            return (MON_UNAFFECTED);

        if (check_mons_resist_magic( mon, beam.ench_power ))
            return mons_immune_magic(mon) ? MON_UNAFFECTED : MON_RESIST;

        simple_monster_message(mon, " is enslaved.");
        beam.obvious_effect = true;

        // wow, permanent enslaving
        mon->attitude = ATT_FRIENDLY;
        return (MON_AFFECTED);
    }

    if (beam.flavour == BEAM_ENSLAVE_DEMON 
        && mons_holiness(mon) == MH_DEMONIC)
    {
#if DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS,
             "HD: %d; pow: %d", mon->hit_dice, beam.ench_power );
#endif

        if (mon->hit_dice * 11 / 2 >= random2(beam.ench_power)
            || mons_is_unique(mon->type))
        {
            return (MON_RESIST);
        }

        // already friendly
        if (mons_friendly(mon))
            return (MON_UNAFFECTED);
        
        simple_monster_message(mon, " is enslaved.");
        beam.obvious_effect = true;

        // wow, permanent enslaving
        if (one_chance_in(2 + mon->hit_dice / 4))
            mon->attitude = ATT_FRIENDLY;
        else
            mon->add_ench(ENCH_CHARM);
        
        // break fleeing and suchlike
        mon->behaviour = BEH_SEEK;
        return (MON_AFFECTED);
    }

    //
    // Everything past this point must pass this magic resistance test. 
    //
    // Using check_mons_resist_magic here since things like disintegrate
    // are beyond this point. -- bwr

    // death_check should be set true already if one of the beams above
    // did its thing but wants to see if the monster died.
    //
    if (!death_check
        && check_mons_resist_magic( mon, beam.ench_power )
        && beam.flavour != BEAM_HASTE 
        && beam.flavour != BEAM_HEALING
        && beam.flavour != BEAM_INVISIBILITY)
    {
        return mons_immune_magic(mon) ? MON_UNAFFECTED : MON_RESIST;
    }

    if (beam.flavour == BEAM_PAIN)      /* pain/agony */
    {
        if (mons_res_negative_energy( mon ))
            return (MON_UNAFFECTED);

        if (simple_monster_message(mon, " convulses in agony!"))
            beam.obvious_effect = true;

        if (beam.name.find("agony") != std::string::npos)
        {
            // AGONY
            mon->hit_points = mon->hit_points / 2;

            if (mon->hit_points < 1)
                mon->hit_points = 1;
        }
        else
        {
            // PAIN
            hurt_monster( mon, roll_dice( beam.damage ) );
        }

        death_check = true;
    }

    if (beam.flavour == BEAM_DISINTEGRATION)     /* disrupt/disintegrate */
    {
        if (simple_monster_message(mon, " is blasted."))
            beam.obvious_effect = true;

        hurt_monster( mon, roll_dice( beam.damage ) );

        death_check = true;
    }


    if (beam.flavour == BEAM_SLEEP)
    {
        if (mon->has_ench(ENCH_SLEEP_WARY))  // slept recently
            return (MON_RESIST);        

        if (mons_holiness(mon) != MH_NATURAL) // no unnatural 
            return (MON_UNAFFECTED);

        // Cold res monsters resist hibernation (for consistency
        // with mass sleep).
        if (mons_res_cold(mon) > 0)
            return (MON_UNAFFECTED);

        if (simple_monster_message(mon, " looks drowsy..."))
            beam.obvious_effect = true;

        mon->put_to_sleep();

        return (MON_AFFECTED);
    }

    if (beam.flavour == BEAM_BACKLIGHT)
    {
        if (backlight_monsters(mon->x, mon->y, beam.hit, 0))
        {
            beam.obvious_effect = true;
            return (MON_AFFECTED);
        }
        return (MON_UNAFFECTED);
    }

    // everything else?
    if ( !death_check )
        return (mons_ench_f2(mon, beam));

    if (mon->hit_points < 1)
        monster_die(mon, beam);
    else
    {
        print_wounds(mon);

        if (mons_is_mimic( mon->type ))
            mimic_alert(mon);
    }

    return (MON_AFFECTED);
}


// extra range used on hit
static int  range_used_on_hit(bolt &beam)
{
    // non-beams can only affect one thing (player/monster)
    if (!beam.is_beam)
        return (BEAM_STOP);

    // CHECK ENCHANTMENTS
    if (beam.name[0] == '0')
    {
        switch(beam.flavour)
        {
        case BEAM_SLOW:
        case BEAM_HASTE:
        case BEAM_HEALING:
        case BEAM_PARALYSIS:
        case BEAM_CONFUSION:
        case BEAM_INVISIBILITY:
        case BEAM_TELEPORT:
        case BEAM_POLYMORPH:
        case BEAM_CHARM:
        case BEAM_BANISH:
        case BEAM_PAIN:
        case BEAM_DISINTEGRATION:
        case BEAM_DEGENERATE:
        case BEAM_DISPEL_UNDEAD:
        case BEAM_ENSLAVE_UNDEAD:
        case BEAM_ENSLAVE_DEMON:
        case BEAM_SLEEP:
        case BEAM_BACKLIGHT:
            return (BEAM_STOP);
        default:
            break;
        }

        return (0);
    }

    // hellfire stops for nobody!
    if (beam.name == "hellfire")
        return (0);

    // generic explosion
    if (beam.is_explosion || beam.is_big_cloud)
        return (BEAM_STOP);

    // plant spit
    if (beam.flavour == BEAM_ACID)
        return (BEAM_STOP);

    // lava doesn't go far, but it goes through most stuff
    if (beam.flavour == BEAM_LAVA)
        return (1);

    // If it isn't lightning, reduce range by a lot
    if (beam.flavour != BEAM_ELECTRICITY)
        return (random2(4) + 2);

    return (0);
}

/*
   Takes a bolt and refines it for use in the explosion function. Called
   from missile() and beam() in beam.cc. Explosions which do not follow from
   beams (eg scrolls of immolation) bypass this function.
 */
static void explosion1(bolt &pbolt)
{
    int ex_size = 1;
    // convenience
    int x = pbolt.target_x;
    int y = pbolt.target_y;
    const char *seeMsg = NULL;
    const char *hearMsg = NULL;

    // assume that the player can see/hear the explosion, or
    // gets burned by it anyway.  :)
    pbolt.msg_generated = true;

    if (pbolt.name == "hellfire")
    {
        seeMsg = "The hellfire explodes!";
        hearMsg = "You hear a strangely unpleasant explosion.";

        pbolt.type = SYM_BURST;
        pbolt.flavour = BEAM_HELLFIRE;
    }

    if (pbolt.name == "golden flame")
    {
        seeMsg = "The flame explodes!";
        hearMsg = "You feel a deep, resonant explosion.";

        pbolt.type = SYM_BURST;
        pbolt.flavour = BEAM_HOLY;
        ex_size = 2;
    }
    
    if (pbolt.name == "fireball")
    {
        seeMsg = "The fireball explodes!";
        hearMsg = "You hear an explosion.";

        pbolt.type = SYM_BURST;
        pbolt.flavour = BEAM_FIRE;
        ex_size = 1;
    }

    if (pbolt.name == "orb of electricity")
    {
        seeMsg = "The orb of electricity explodes!";
        hearMsg = "You hear a clap of thunder!";

        pbolt.type = SYM_BURST;
        pbolt.flavour = BEAM_ELECTRICITY;
        pbolt.colour = LIGHTCYAN;
        pbolt.damage.num = 1;
        ex_size = 2;
    }

    if (pbolt.name == "orb of energy")
    {
        seeMsg = "The orb of energy explodes.";
        hearMsg = "You hear an explosion.";
    }

    if (pbolt.name == "metal orb")
    {
        seeMsg = "The orb explodes into a blast of deadly shrapnel!";
        hearMsg = "You hear an explosion!";

        pbolt.name = "blast of shrapnel";
        pbolt.type = SYM_ZAP;
        pbolt.flavour = BEAM_FRAG;     // sets it from pure damage to shrapnel (which is absorbed extra by armour)
    }

    if (pbolt.name == "great blast of cold")
    {
        seeMsg = "The blast explodes into a great storm of ice!";
        hearMsg = "You hear a raging storm!";

        pbolt.name = "ice storm";
        pbolt.damage.num = 6;
        pbolt.type = SYM_ZAP;
        pbolt.colour = WHITE;
        ex_size = 2 + (random2( pbolt.ench_power ) > 75);
    }

    if (pbolt.name == "ball of vapour")
    {
        seeMsg = "The ball expands into a vile cloud!";
        hearMsg = "You hear a gentle \'poof\'.";
        pbolt.name = "stinking cloud";
    }

    if (pbolt.name == "potion")
    {
        seeMsg = "The potion explodes!";
        hearMsg = "You hear an explosion!";
        pbolt.name = "cloud";
    }

    if (seeMsg == NULL)
    {
        seeMsg = "The beam explodes into a cloud of software bugs!";
        hearMsg = "You hear the sound of one hand clapping!";
    }


    if (!pbolt.is_tracer && *seeMsg && *hearMsg)
    {
        // check for see/hear/no msg
        if (see_grid(x,y) || (x == you.x_pos && y == you.y_pos))
            mpr(seeMsg);
        else
        {
            if (!(silenced(x,y) || silenced(you.x_pos, you.y_pos)))
                mpr(hearMsg, MSGCH_SOUND);
            else
                pbolt.msg_generated = false;
        }
    }

    pbolt.ex_size = ex_size;
    explosion( pbolt );
}                               // end explosion1()


#define MAX_EXPLOSION_RADIUS 9

// explosion is considered to emanate from beam->target_x, target_y
// and has a radius equal to ex_size.  The explosion will respect
// boundaries like walls, but go through/around statues/idols/etc.

// for each cell affected by the explosion, affect() is called.

void explosion( bolt &beam, bool hole_in_the_middle,
                bool explode_in_wall, bool stop_at_statues,
                bool stop_at_walls)
{
    if (in_bounds(beam.source_x, beam.source_y)
        && !(beam.source_x == beam.target_x
             && beam.source_y == beam.target_y)
        && (!explode_in_wall || stop_at_statues || stop_at_walls))
    {
        ray_def ray;
        int     max_dist = grid_distance(beam.source_x, beam.source_y,
                                     beam.target_x, beam.target_y);

        ray.fullray_idx = -1; // to quiet valgrind
        find_ray( beam.source_x, beam.source_y, beam.target_x, beam.target_y,
                  true, ray, 0, true );

        // Can cast explosions out from statues or walls.
        if (ray.x() == beam.source_x && ray.y() == beam.source_y)
        {
            max_dist--;
            ray.advance(true); 
        }

        int dist = 0;
        while (dist++ <= max_dist && !(ray.x()    == beam.target_x
                                       && ray.y() == beam.target_y))
        {
            if (grid_is_solid(ray.x(), ray.y()))
            {
                bool is_wall = grid_is_wall(grd[ray.x()][ray.y()]);
                if (!stop_at_statues && !is_wall)
                {
#if DEBUG_DIAGNOSTICS
                    mpr("Explosion beam passing over a statue or other "
                        "non-wall solid feature.", MSGCH_DIAGNOSTICS);
#endif
                    continue;
                }
                else if (!stop_at_walls && is_wall)
                {
#if DEBUG_DIAGNOSTICS
                    mpr("Explosion beam passing through a wall.",
                        MSGCH_DIAGNOSTICS);
#endif
                    continue;
                }

#if DEBUG_DIAGNOSTICS
                if (!is_wall && stop_at_statues)
                    mpr("Explosion beam stopped by a statue or other "
                        "non-wall solid feature.", MSGCH_DIAGNOSTICS);
                else if (is_wall && stop_at_walls)
                    mpr("Explosion beam stopped by a by wall.",
                        MSGCH_DIAGNOSTICS);
                else
                    mpr("Explosion beam stopped by someting buggy.",
                        MSGCH_DIAGNOSTICS);
#endif

                break;
            }
            ray.advance(true);
        } // while (dist++ <= max_dist)

        // Backup so we don't explode inside the wall.
        if (!explode_in_wall && grid_is_wall(grd[ray.x()][ray.y()]))
        {
#if DEBUG_DIAGNOSTICS
            int old_x = ray.x();
            int old_y = ray.y();
#endif
            ray.regress();
#if DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS,
                 "Can't explode in a solid wall, backing up a step "
                 "along the beam path from (%d, %d) to (%d, %d)",
                 old_x, old_y, ray.x(), ray.y());
#endif
        }
        beam.target_x = ray.x();
        beam.target_y = ray.y();
    } // if (!explode_in_wall)

    int r = beam.ex_size;

    // beam is now an explosion;  set in_explosion_phase
    beam.in_explosion_phase = true;

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS,
         "explosion at (%d, %d) : t=%d c=%d f=%d hit=%d dam=%dd%d",
         beam.target_x, beam.target_y, 
         beam.type, beam.colour, beam.flavour, 
         beam.hit, beam.damage.num, beam.damage.size );
#endif

    // for now, we don't support explosions greater than 9 radius
    if (r > MAX_EXPLOSION_RADIUS)
        r = MAX_EXPLOSION_RADIUS;

    // make a noise
    noisy( 10 + 5*r, beam.target_x, beam.target_y );

    // set map to false
    explode_map.init(false);

    // discover affected cells - recursion is your friend!
    // this is done to model an explosion's behaviour around
    // corners where a simple 'line of sight' isn't quite
    // enough.   This might be slow for really big explosions,
    // as the recursion runs approximately as R^2
    explosion_map(beam, 0, 0, 0, 0, r);

    // go through affected cells, drawing effect and
    // calling affect() and affect_items() for each.
    // now, we get a bit fancy, drawing all radius 0
    // effects, then radius 1, radius 2, etc.  It looks
    // a bit better that way.

    // turn buffering off
#ifdef WIN32CONSOLE
    bool oldValue = true;
    if (!beam.is_tracer)
        oldValue = set_buffering(false);
#endif

    // --------------------- begin boom ---------------

    bool drawing = true;
    for (int i = 0; i < 2; i++)
    {
        // do center -- but only if its affected
        if (!hole_in_the_middle)
            explosion_cell(beam, 0, 0, drawing);
 
        // do the rest of it
        for(int rad = 1; rad <= r; rad ++)
        {
            // do sides
            for (int ay = 1 - rad; ay <= rad - 1; ay += 1)
            {
                if (explode_map[-rad+9][ay+9])
                    explosion_cell(beam, -rad, ay, drawing);

                if (explode_map[rad+9][ay+9])
                    explosion_cell(beam, rad, ay, drawing);
            }

            // do top & bottom
            for (int ax = -rad; ax <= rad; ax += 1)
            {
                if (explode_map[ax+9][-rad+9])
                    explosion_cell(beam, ax, -rad, drawing);

                if (explode_map[ax+9][rad+9])
                    explosion_cell(beam, ax, rad, drawing);
            }

            // new-- delay after every 'ring' {gdl}
            // If we don't refresh curses we won't
            // guarantee that the explosion is visible
            if (drawing)
                update_screen();
            // only delay on real explosion
            if (!beam.is_tracer && drawing)
                delay(50);
        }

        drawing = false;
    }

    bool seen_anything = false;
    for ( int i = -9; i <= 9; ++i )
        for ( int j = -9; j <= 9; ++j )
            if ( explode_map[i+9][j+9] &&
                 see_grid(beam.target_x + i, beam.target_y + j) )
                seen_anything = true;

    // ---------------- end boom --------------------------

#ifdef WIN32CONSOLE
    if (!beam.is_tracer)
        set_buffering(oldValue);
#endif

    // duplicate old behaviour - pause after entire explosion
    // has been drawn.
    if (!beam.is_tracer && seen_anything)
        more();
}

static void explosion_cell(bolt &beam, int x, int y, bool drawOnly)
{
    bool random_beam = false;
    int realx = beam.target_x + x;
    int realy = beam.target_y + y;

    if (!drawOnly)
    {
        // random beams: randomize before affect
        if (beam.flavour == BEAM_RANDOM)
        {
            random_beam = true;
            beam.flavour = BEAM_FIRE + random2(7);
        }

        affect(beam, realx, realy);

        if (random_beam)
            beam.flavour = BEAM_RANDOM;
    }

    // early out for tracer
    if (beam.is_tracer)
        return;

    // now affect items
    if (!drawOnly)
    {
        affect_items(beam, realx, realy);
        if (affects_wall(beam, grd[realx][realy]))
            affect_wall(beam, realx, realy);
    }

    if (drawOnly)
    {
        int drawx = grid2viewX(realx);
        int drawy = grid2viewY(realy);

        if (see_grid(realx, realy) || (realx == you.x_pos && realy == you.y_pos))
        {
            // bounds check
            if (in_los_bounds(drawx, drawy))
            {
                if (beam.colour == BLACK)
                    textcolor(random_colour());
                else
                    textcolor(beam.colour);

                gotoxy(drawx, drawy);
                putch('#');
            }
        }
    }
}

static void explosion_map( bolt &beam, int x, int y,
                           int count, int dir, int r )
{
    // 1. check to see out of range
    if (x * x + y * y > r * r + r)
        return;

    // 2. check count
    if (count > 10*r)
        return;

    // 3. check to see if we're blocked by something
    //    specifically, we're blocked by WALLS.  Not
    //    statues, idols, etc.
    const int dngn_feat = grd[beam.target_x + x][beam.target_y + y];

    // special case: explosion originates from rock/statue
    // (e.g. Lee's rapid deconstruction) - in this case, ignore
    // solid cells at the center of the explosion.
    if (dngn_feat <= DNGN_MAXWALL)
    {
        if (!(x==0 && y==0) && !affects_wall(beam, dngn_feat))
            return;
    }

    // hmm, I think we're ok
    explode_map[x+9][y+9] = true;

    // now recurse in every direction except the one we
    // came from
    for(int i=0; i<4; i++)
    {
        if (i+1 != dir)
        {
            int cadd = 5;
            if (x * spreadx[i] < 0 || y * spready[i] < 0)
                cadd = 17;

            explosion_map( beam, x + spreadx[i], y + spready[i],
                           count + cadd, opdir[i], r );
        }
    }
}

// returns true if the beam is harmful (ignoring monster
// resists) -- mon is given for 'special' cases where,
// for example, "Heal" might actually hurt undead, or
// "Holy Word" being ignored by holy monsters, etc.
//
// only enchantments should need the actual monster type
// to determine this;  non-enchantments are pretty
// straightforward.
bool nasty_beam(monsters *mon, bolt &beam)
{
    // take care of non-enchantments
    if (beam.name[0] != '0')
        return (true);

    // now for some non-hurtful enchantments

    // degeneration / sleep
    if (beam.flavour == BEAM_DEGENERATE || beam.flavour == BEAM_SLEEP)
        return (mons_holiness(mon) == MH_NATURAL);

    // dispel undead / control undead
    if (beam.flavour == BEAM_DISPEL_UNDEAD || beam.flavour == BEAM_ENSLAVE_UNDEAD)
        return (mons_holiness(mon) == MH_UNDEAD);

    // pain/agony
    if (beam.flavour == BEAM_PAIN)
        return (!mons_res_negative_energy( mon ));

    // control demon
    if (beam.flavour == BEAM_ENSLAVE_DEMON)
        return (mons_holiness(mon) == MH_DEMONIC);

    // haste
    if (beam.flavour == BEAM_HASTE)
        return (false);

    // healing
    if (beam.flavour == BEAM_HEALING || beam.flavour == BEAM_INVISIBILITY)
        return (false);

    // everything else is considered nasty by everyone
    return (true);
}

bool nice_beam( struct monsters *mon, struct bolt &beam )
{
    // haste
    if (beam.flavour == BEAM_HASTE || beam.flavour == BEAM_HEALING
        || beam.flavour == BEAM_INVISIBILITY)
    {
        return (true);
    }

    return (false);
}

////////////////////////////////////////////////////////////////////////////
// bolt

// A constructor for bolt to help guarantee that we start clean (this has
// caused way too many bugs).  Putting it here since there's no good place to
// put it, and it doesn't do anything other than initialize it's members.
// 
// TODO: Eventually it'd be nice to have a proper factory for these things
// (extended from setup_mons_cast() and zapping() which act as limited ones).
bolt::bolt() : range(0), rangeMax(0), type(SYM_ZAP), colour(BLACK),
               flavour(BEAM_MAGIC), source_x(0), source_y(0), damage(0,0),
               ench_power(0), hit(0), target_x(0), target_y(0), pos(),
               thrower(KILL_MISC), ex_size(0), beam_source(MHITNOT), name(),
               is_beam(false), is_explosion(false), is_big_cloud(false),
               is_enchant(false), is_energy(false), is_launched(false),
               is_thrown(false), target_first(false), aimed_at_spot(false),
               aux_source(), affects_nothing(false), obvious_effect(false),
               effect_known(true), fr_count(0), foe_count(0), fr_power(0),
               foe_power(0), fr_hurt(0), foe_hurt(0), fr_helped(0),
               foe_helped(0), is_tracer(false), aimed_at_feet(false),
               msg_generated(false), in_explosion_phase(false),
               smart_monster(false), can_see_invis(false),
               attitude(ATT_HOSTILE), foe_ratio(0), chose_ray(false)
{
}

killer_type bolt::killer() const
{
    if (flavour == BEAM_BANISH)
        return (KILL_RESET);
    
    switch (thrower)
    {
    case KILL_YOU:
    case KILL_YOU_MISSILE:
        return (flavour == BEAM_PARALYSIS? KILL_YOU : KILL_YOU_MISSILE);

    case KILL_MON:
    case KILL_MON_MISSILE:
        return (KILL_MON_MISSILE);

    default:
        return (KILL_MON_MISSILE);
    }
}

void bolt::set_target(const dist &d)
{
    if (!d.isValid)
        return;

    target_x = d.tx;
    target_y = d.ty;

    chose_ray = d.choseRay;
    if ( d.choseRay )
        ray = d.ray;

    if (d.isEndpoint)
        aimed_at_spot = true;
}

void bolt::setup_retrace()
{
    if (pos.x && pos.y)
    {
        target_x = pos.x;
        target_y = pos.y;
    }
    std::swap(source_x, target_x);
    std::swap(source_y, target_y);
    affects_nothing = true;
    aimed_at_spot   = true;    
}
