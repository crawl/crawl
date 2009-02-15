/*
 *  File:       skills.cc
 *  Summary:    Skill exercising functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "skills.h"

#include <algorithm>
#include <string.h>
#include <stdlib.h>

#include "externs.h"

#include "itemprop.h"
#include "macro.h"
#include "notes.h"
#include "output.h"
#include "player.h"
#include "skills2.h"
#include "spl-cast.h"
#include "stuff.h"
#include "tutorial.h"


// MAX_COST_LIMIT is the maximum XP amount it will cost to raise a skill
//                by 10 skill points (ie one standard practice).
//
// MAX_SPENDING_LIMIT is the maximum XP amount we allow the player to
//                    spend on a skill in a single raise.
//
// Note that they don't have to be equal, but it is important to make
// sure that they're set so that the spending limit will always allow
// for 1 skill point to be earned.
#define MAX_COST_LIMIT           250
#define MAX_SPENDING_LIMIT       250

static int _exercise2( int exsk );

// These values were calculated by running a simulation of gaining skills.
// The goal is to try and match the old cost system which used the player's
// experience level (which has a number of problems) so things shouldn't
// seem too different to the player... but we still try to err on the
// high side for the lower levels. -- bwr
int skill_cost_needed( int level )
{
    // The average starting skill total is actually lower, but
    // some classes get about 2200, and they would probably be
    // start around skill cost level 3 if we used the average.  -- bwr
    int ret = 2200;

    switch (level)
    {
    case 1: ret = 0; break;

    case 2:  ret +=   250; break; //  250 -- big because of initial 25 pool
    case 3:  ret +=   350; break; //  100
    case 4:  ret +=   550; break; //  200
    case 5:  ret +=   900; break; //  350
    case 6:  ret +=  1300; break; //  400
    case 7:  ret +=  1900; break; //  600
    case 8:  ret +=  2800; break; //  900
    case 9:  ret +=  4200; break; // 1400
    case 10: ret +=  5900; break; // 1700
    case 11: ret +=  9000; break; // 3100

    default:
        ret += 9000 + (4000 * (level - 11));
        break;
    }

    return (ret);
}

void calc_total_skill_points( void )
{
    int i;

    you.total_skill_points = 0;

    for (i = 0; i < NUM_SKILLS; i++)
        you.total_skill_points += you.skill_points[i];

    for (i = 1; i <= 27; i++)
        if (you.total_skill_points < skill_cost_needed(i))
            break;

    you.skill_cost_level = i - 1;

#if DEBUG_DIAGNOSTICS
    you.redraw_experience = true;
#endif
}

// skill_cost_level makes skills more expensive for more experienced characters
// skill_level      makes higher skills more expensive
static int _calc_skill_cost( int skill_cost_level, int skill_level )
{
    int ret = 1 + skill_level;

    // does not yet allow for loss of skill levels.
    if (skill_level > 9)
    {
        ret *= (skill_level - 7);
        ret /= 3;
    }

    if (skill_cost_level > 4)
        ret += skill_cost_level - 4;
    if (skill_cost_level > 7)
        ret += skill_cost_level - 7;
    if (skill_cost_level > 10)
        ret += skill_cost_level - 10;
    if (skill_cost_level > 13)
        ret += skill_cost_level - 13;
    if (skill_cost_level > 16)
        ret += skill_cost_level - 16;

    if (skill_cost_level > 10)
    {
        ret *= (skill_cost_level - 5);
        ret /= 5;
    }

    if (skill_level > 7)
        ret += 1;
    if (skill_level > 9)
        ret += 2;
    if (skill_level > 11)
        ret += 3;
    if (skill_level > 13)
        ret += 4;
    if (skill_level > 15)
        ret += 5;

    if (ret > MAX_COST_LIMIT)
        ret = MAX_COST_LIMIT;

    return (ret);
}

// returns total number of skill points gained
int exercise(int exsk, int deg)
{
    int ret = 0;

    while (deg > 0)
    {
        if (you.exp_available <= 0 || you.skills[exsk] >= 27)
            break;

        if (you.practise_skill[exsk] || one_chance_in(4))
            ret += _exercise2( exsk );

        deg--;
    }

#ifdef DEBUG_DIAGNOSTICS
    if (ret)
    {
        mprf(MSGCH_DIAGNOSTICS, "Exercised %s (deg: %d) by %d",
                                skill_name(exsk), deg, ret);
    }
#endif

    return (ret);
}                               // end exercise()

static int _exercise2(int exsk)
{
    int deg = 10;
    int bonus = 0;
    char old_best_skill = best_skill(SK_FIGHTING, (NUM_SKILLS - 1), 99);

    // Being good at some weapons makes others easier to learn.
    if (exsk < SK_ARMOUR)
    {
        // Short Blades and Long Blades.
        if ((exsk == SK_SHORT_BLADES || exsk == SK_LONG_BLADES)
            && (you.skills[SK_SHORT_BLADES] > you.skills[exsk]
                || you.skills[SK_LONG_BLADES] > you.skills[exsk]))
        {
            bonus += random2(30);
        }

        // Axes and Polearms.
        if ((exsk == SK_AXES || exsk == SK_POLEARMS)
            && (you.skills[SK_AXES] > you.skills[exsk]
                || you.skills[SK_POLEARMS] > you.skills[exsk]))
        {
            bonus += random2(30);
        }

        // Polearms and Staves.
        if ((exsk == SK_POLEARMS || exsk == SK_STAVES)
            && (you.skills[SK_POLEARMS] > you.skills[exsk]
                || you.skills[SK_STAVES] > you.skills[exsk]))
        {
            bonus += random2(30);
        }

        // Axes and Maces.
        if ((exsk == SK_AXES || exsk == SK_MACES_FLAILS)
            && (you.skills[SK_AXES] > you.skills[exsk]
                || you.skills[SK_MACES_FLAILS] > you.skills[exsk]))
        {
            bonus += random2(30);
        }

        // Slings and Throwing.
        if ((exsk == SK_SLINGS || exsk == SK_THROWING)
            && (you.skills[SK_SLINGS] > you.skills[exsk]
                || you.skills[SK_THROWING] > you.skills[exsk]))
        {
            bonus += random2(30);
        }
    }

    // Quick fix for the fact that stealth can't be gained fast enough
    // to keep up with the monster levels.  This should speed its
    // advancement.
    if (exsk == SK_STEALTH)
        bonus += random2(30);

    int skill_change = _calc_skill_cost(you.skill_cost_level, you.skills[exsk]);

    // Spellcasting is cheaper early on, and elementals hinder each
    // other.
    if (exsk >= SK_SPELLCASTING)
    {
        if (you.skill_cost_level < 5)
            skill_change /= 2;
        else if (you.skill_cost_level < 15)
        {
            skill_change *= (10 + (you.skill_cost_level - 5));
            skill_change /= 20;
        }

        // Being good at elemental magic makes other elements harder to
        // learn.
        if (exsk >= SK_FIRE_MAGIC && exsk <= SK_EARTH_MAGIC
            && (you.skills[SK_FIRE_MAGIC] > you.skills[exsk]
                || you.skills[SK_ICE_MAGIC] > you.skills[exsk]
                || you.skills[SK_AIR_MAGIC] > you.skills[exsk]
                || you.skills[SK_EARTH_MAGIC] > you.skills[exsk]))
        {
            if (one_chance_in(3))
                return (0);
        }

        // Some are direct opposites.
        if ((exsk == SK_FIRE_MAGIC || exsk == SK_ICE_MAGIC)
            && (you.skills[SK_FIRE_MAGIC] > you.skills[exsk]
                || you.skills[SK_ICE_MAGIC] > you.skills[exsk]))
        {
            // Of course, this is cumulative with the one above.
            if (!one_chance_in(3))
                return (0);
        }

        if ((exsk == SK_AIR_MAGIC || exsk == SK_EARTH_MAGIC)
            && (you.skills[SK_AIR_MAGIC] > you.skills[exsk]
                || you.skills[SK_EARTH_MAGIC] > you.skills[exsk]))
        {
            if (!one_chance_in(3))
                return (0);
        }

        // Experimental restriction (too many spell schools). -- bwr
        int skill_rank = 1;

        for (int i = SK_CONJURATIONS; i <= SK_DIVINATIONS; ++i)
        {
            if (you.skills[exsk] < you.skills[i])
                skill_rank++;
        }

        // Things get progressively harder, but not harder than
        // the Fire-Air or Ice-Earth level.
        if (skill_rank > 3 && one_chance_in(10 - skill_rank))
            return (0);
    }

    int spending_limit = std::min(MAX_SPENDING_LIMIT, you.exp_available);

    // Handle fractional learning.
    if (skill_change > spending_limit)
    {
        // This system is a bit hard on missile weapons in the late
        // game, since they require expendable ammo in order to
        // practise.  Increasing the "deg"ree of exercise would make
        // missile weapons too easy earlier on, so, instead, we're
        // giving them a special case here.
        if (exsk != SK_DARTS && exsk != SK_BOWS && exsk != SK_CROSSBOWS
            || skill_change > you.exp_available)
        {
            int fraction = (spending_limit * 10) / skill_change;

            deg = (deg * fraction) / 10;

            if (deg == 0)
                bonus = (bonus * fraction) / 10;
        }
        else
            deg = ((skill_change / 2) > MAX_SPENDING_LIMIT) ? 5 : 10;

        skill_change = spending_limit;
    }

    skill_change -= random2(5);

    if (skill_change <= 0)
    {
        // No free lunch.  This is a problem now that we don't have
        // overspending.
        skill_change = (deg > 0 || bonus > 0) ? 1 : 0;
    }

    // We can safely return at any stage before this.
    int skill_inc = deg + bonus;

    // Starting to learn skills is easier if the appropriate stat is
    // high.
    if (you.skills[exsk] == 0)
    {
        if ((exsk >= SK_FIGHTING && exsk <= SK_STAVES) || exsk == SK_ARMOUR)
        {
            // These skills are easier for the strong.
            skill_inc *= std::max<int>(5, you.strength);
            skill_inc /= 10;
        }
        else if (exsk >= SK_SLINGS && exsk <= SK_UNARMED_COMBAT)
        {
            // These skills are easier for the dexterous.
            // Note: Armour is handled above.
            skill_inc *= std::max<int>(5, you.dex);
            skill_inc /= 10;
        }
        else if (exsk >= SK_SPELLCASTING && exsk <= SK_POISON_MAGIC)
        {
            // These skills are easier for the smart.
            skill_inc *= std::max<int>(5, you.intel);
            skill_inc /= 10;
        }
    }

    if (skill_inc <= 0)
        return (0);

    you.skill_points[exsk] += skill_inc;
    you.exp_available -= skill_change;

    you.total_skill_points += skill_inc;
    if (you.skill_cost_level < 27
        && you.total_skill_points
               >= skill_cost_needed(you.skill_cost_level + 1))
    {
        you.skill_cost_level++;
    }

    you.exp_available = std::max(0, you.exp_available);

    you.redraw_experience = true;

/*
    New (LH): debugging bit: when you exercise a skill, displays the skill
    exercised and how much you spent on it. Too irritating to be a regular
    WIZARD feature.


#if DEBUG_DIAGNOSTICS
    mprf( MSGCH_DIAGNOSTICS, "Exercised %s * %d for %d xp.",
          skill_name(exsk), skill_inc, skill_change );
#endif
*/

    if (you.skill_points[exsk] >
           (skill_exp_needed(you.skills[exsk] + 1)
            * species_skills(exsk, you.species) / 100))
    {

        you.skills[exsk]++;
        take_note(Note(NOTE_GAIN_SKILL, exsk, you.skills[exsk]));

        if (you.skills[exsk] == 27)
        {
            mprf(MSGCH_INTRINSIC_GAIN,
                 "You have mastered %s!", skill_name( exsk ) );
        }
        else if (you.skills[exsk] == 1)
        {
            mprf(MSGCH_INTRINSIC_GAIN,
                 "You have gained %s skill!", skill_name( exsk ) );

            tut_gained_new_skill(exsk);
        }
        else
        {
            mprf(MSGCH_INTRINSIC_GAIN, "Your %s skill increases to level %d!",
                 skill_name( exsk ), you.skills[exsk] );
        }

        learned_something_new(TUT_SKILL_RAISE);

        // Recalculate this skill's order for tie breaking skills
        // at its new level.   See skills2.cc::init_skill_order()
        // for more details.  -- bwr
        you.skill_order[exsk] = 0;

        for (int i = SK_FIGHTING; i < NUM_SKILLS; ++i)
        {
            if (i == exsk)
                continue;

            if (you.skills[i] >= you.skills[exsk])
                you.skill_order[exsk]++;
        }

        if (exsk == SK_FIGHTING)
            calc_hp();

        if (exsk == SK_INVOCATIONS || exsk == SK_SPELLCASTING)
            calc_mp();

        if (exsk == SK_DODGING || exsk == SK_ARMOUR)
            you.redraw_evasion = true;

        if (exsk == SK_ARMOUR || exsk == SK_SHIELDS
            || exsk == SK_ICE_MAGIC || exsk == SK_EARTH_MAGIC
            || you.duration[ DUR_TRANSFORMATION ] > 0)
        {
            you.redraw_armour_class = true;
        }

        const unsigned char best = best_skill( SK_FIGHTING,
                                               (NUM_SKILLS - 1), 99 );

        const unsigned char best_spell = best_skill( SK_SPELLCASTING,
                                                     SK_POISON_MAGIC, 99 );

        if (exsk == SK_SPELLCASTING
            && you.skills[exsk] == 1 && best_spell == SK_SPELLCASTING)
        {
            mpr("You're starting to get the hang of this magic thing.");
        }

        if (best != old_best_skill || old_best_skill == exsk)
            redraw_skill( you.your_name, player_title() );

        if (you.weapon() && item_is_staff( *you.weapon() ))
            maybe_identify_staff(*you.weapon());
    }

    return (skill_inc);
}
