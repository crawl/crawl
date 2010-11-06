/*
 *  File:       skills.cc
 *  Summary:    Skill exercising functions.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "skills.h"

#include <algorithm>
#include <string.h>
#include <stdlib.h>

#include "externs.h"
#include "hints.h"
#include "itemprop.h"
#include "notes.h"
#include "output.h"
#include "player.h"
#include "random.h"
#include "skills2.h"
#include "spl-cast.h"
#include "sprint.h"
#include "state.h"


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

static int _exercise2(skill_type exsk);

// These values were calculated by running a simulation of gaining skills.
// The goal is to try and match the old cost system which used the player's
// experience level (which has a number of problems) so things shouldn't
// seem too different to the player... but we still try to err on the
// high side for the lower levels. -- bwr
int skill_cost_needed(int level)
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

void calc_total_skill_points(void)
{
    int i;

    you.total_skill_points = 0;

    for (i = 0; i < NUM_SKILLS; i++)
        you.total_skill_points += you.skill_points[i];

    for (i = 1; i <= 27; i++)
        if (you.total_skill_points < skill_cost_needed((skill_type)i))
            break;

    you.skill_cost_level = i - 1;

#ifdef DEBUG_DIAGNOSTICS
    you.redraw_experience = true;
#endif
}

// skill_cost_level makes skills more expensive for more experienced characters
// skill_level      makes higher skills more expensive
static int _calc_skill_cost(int skill_cost_level, int skill_level)
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

static void _change_skill_level(skill_type exsk, int n)
{
    ASSERT(n != 0);
    skill_type old_best_skill = best_skill(SK_FIRST_SKILL, SK_LAST_SKILL);

    if (-n > you.skills[exsk])
        n = -you.skills[exsk];
    you.skills[exsk] += n;

    if (n > 0)
        take_note(Note(NOTE_GAIN_SKILL, exsk, you.skills[exsk]));
    else
        take_note(Note(NOTE_LOSE_SKILL, exsk, you.skills[exsk]));

    if (you.skills[exsk] == 27)
    {
        mprf(MSGCH_INTRINSIC_GAIN, "You have mastered %s!", skill_name(exsk));
    }
    else if (you.skills[exsk] == 1 && n > 0)
    {
        mprf(MSGCH_INTRINSIC_GAIN, "You have gained %s skill!", skill_name(exsk));
        hints_gained_new_skill(exsk);
    }
    else if (abs(n) == 1)
    {
        mprf(MSGCH_INTRINSIC_GAIN, "Your %s skill %s to level %d!",
             skill_name(exsk), (n > 0) ? "increases" : "decreases",
             you.skills[exsk]);
    }
    else
    {
        mprf(MSGCH_INTRINSIC_GAIN, "Your %s skill %s %d levels and is now at "
             "level %d!", skill_name(exsk), (n > 0) ? "gained" : "lost",
             abs(n), you.skills[exsk]);
    }

    if (n > 0)
        learned_something_new(HINT_SKILL_RAISE);

    // Recalculate this skill's order for tie breaking skills
    // at its new level.   See skills2.cc::init_skill_order()
    // for more details.  -- bwr
    you.skill_order[exsk] = 0;
    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
    {
        skill_type sk = static_cast<skill_type>(i);
        if (sk != exsk && you.skills[sk] >= you.skills[exsk])
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
        || you.duration[DUR_TRANSFORMATION] > 0)
    {
        you.redraw_armour_class = true;
    }

    const skill_type best_spell = best_skill(SK_SPELLCASTING,
                                             SK_POISON_MAGIC);
    if (exsk == SK_SPELLCASTING && you.skills[exsk] == 1
        && best_spell == SK_SPELLCASTING && n > 0)
    {
        mpr("You're starting to get the hang of this magic thing.");
    }

    const skill_type best = best_skill(SK_FIRST_SKILL, SK_LAST_SKILL);
    if (best != old_best_skill || old_best_skill == exsk)
        redraw_skill(you.your_name, player_title());

    if (you.weapon() && item_is_staff(*you.weapon()))
        maybe_identify_staff(*you.weapon());

    // TODO: also identify rings of wizardry.
}

static void _check_skill_level_change(skill_type sk)
{
    int new_level = you.skills[sk];
    while (1)
    {
        const unsigned int prev = skill_exp_needed(new_level, sk);
        const unsigned int next = skill_exp_needed(new_level + 1, sk);

        if (you.skill_points[sk] >= next)
            new_level++;
        else if (you.skill_points[sk] < prev)
            new_level--;
        else
            break;
    }

    if (new_level != you.skills[sk])
        _change_skill_level(sk, new_level - you.skills[sk]);
}

// returns total number of skill points gained
int exercise(skill_type exsk, int deg, bool change_level)
{
    int ret = 0;

    if (crawl_state.game_is_sprint())
    {
        deg = sprint_modify_skills(deg);
    }

    while (deg > 0)
    {
        if ((you.exp_available <= 0 || you.skills[exsk] >= 27))
            break;

        if (you.practise_skill[exsk] || one_chance_in(3 + you.skills[exsk]))
            ret += _exercise2(exsk);

        deg--;
    }

    if (ret)
        dprf("Exercised %s (deg: %d) by %d", skill_name(exsk), deg, ret);

    if (change_level)
        _check_skill_level_change(exsk);

    return (ret);
}

static bool _check_crosstrain(skill_type exsk, skill_type sk1, skill_type sk2)
{
    return ((exsk == sk1 || exsk == sk2)
            && (you.skills[sk1] > you.skills[exsk]
                || you.skills[sk2] > you.skills[exsk]));
}

static float _weap_crosstrain_bonus(skill_type exsk)
{
    int bonus = 1;

    if (_check_crosstrain(exsk, SK_SHORT_BLADES, SK_LONG_BLADES))
        bonus *= 2;
    if (_check_crosstrain(exsk, SK_AXES,         SK_POLEARMS))
        bonus *= 2;
    if (_check_crosstrain(exsk, SK_POLEARMS,     SK_STAVES))
        bonus *= 2;
    if (_check_crosstrain(exsk, SK_AXES,         SK_MACES_FLAILS))
        bonus *= 2;
    if (_check_crosstrain(exsk, SK_MACES_FLAILS, SK_STAVES))
        bonus *= 2;
    if (_check_crosstrain(exsk, SK_SLINGS,       SK_THROWING))
        bonus *= 2;

    return (bonus);
}

static bool _skip_exercise(skill_type exsk)
{
    if (exsk < SK_SPELLCASTING || exsk > SK_EVOCATIONS)
        return (false);

    // Being good at elemental magic makes other elements harder to
    // learn.
    if (exsk >= SK_FIRE_MAGIC && exsk <= SK_EARTH_MAGIC
        && (you.skills[SK_FIRE_MAGIC] > you.skills[exsk]
            || you.skills[SK_ICE_MAGIC] > you.skills[exsk]
            || you.skills[SK_AIR_MAGIC] > you.skills[exsk]
            || you.skills[SK_EARTH_MAGIC] > you.skills[exsk]))
    {
        if (one_chance_in(3))
            return (true);
    }

    // Some are direct opposites.
    if ((exsk == SK_FIRE_MAGIC || exsk == SK_ICE_MAGIC)
        && (you.skills[SK_FIRE_MAGIC] > you.skills[exsk]
            || you.skills[SK_ICE_MAGIC] > you.skills[exsk]))
    {
        // Of course, this is cumulative with the one above.
        if (!one_chance_in(3))
            return (true);
    }

    if ((exsk == SK_AIR_MAGIC || exsk == SK_EARTH_MAGIC)
        && (you.skills[SK_AIR_MAGIC] > you.skills[exsk]
           || you.skills[SK_EARTH_MAGIC] > you.skills[exsk]))
    {
        if (!one_chance_in(3))
            return (true);
    }

    // Experimental restriction (too many spell schools). -- bwr
    // XXX: This also hinders Spellcasting and Inv/Evo.

    // Count better non-elemental spell skills (up to 6)
    int skill_rank = 1;
    for (int i = SK_CONJURATIONS; i < SK_FIRE_MAGIC; ++i)
        if (you.skills[exsk] < you.skills[i])
            skill_rank++;

    // Things get progressively harder, but not harder than
    // the Fire-Air or Ice-Earth level.
    // Note: 1 <= skill_rank <= 7, so (1 in 6) to (1 in 3).
    if (skill_rank > 3 && one_chance_in(10 - skill_rank))
        return (true);

    return (false);
}

// These get a discount in the late game -- still required?
static bool _discounted_throwing_skill(skill_type exsk)
{
    return (exsk == SK_THROWING || exsk == SK_BOWS || exsk == SK_CROSSBOWS);
}

static int _stat_mult(skill_type exsk, int skill_inc)
{
    int stat = 10;

    if ((exsk >= SK_FIGHTING && exsk <= SK_STAVES) || exsk == SK_ARMOUR)
    {
        // These skills are Easier for the strong.
        stat = you.strength();
    }
    else if (exsk >= SK_SLINGS && exsk <= SK_UNARMED_COMBAT)
    {
        // These skills are easier for the dexterous.
        // Note: Armour is handled above.
        stat = you.dex();
    }
    else if (exsk >= SK_SPELLCASTING && exsk <= SK_POISON_MAGIC)
    {
        // These skills are easier for the smart.
        stat = you.intel();
    }

    return (skill_inc * std::max<int>(5, stat) / 10);
}

static void _check_skill_cost_change()
{
    if (you.skill_cost_level < 27
        && you.total_skill_points
           >= skill_cost_needed(you.skill_cost_level + 1))
    {
        you.skill_cost_level++;
    }
    else if (you.skill_cost_level > 0
        && you.total_skill_points
           < skill_cost_needed(you.skill_cost_level))
    {
        you.skill_cost_level--;
    }
}

void change_skill_points(skill_type sk, int points, bool change_level)
{
    if (you.skill_points[sk] + points < 0)
        points = -you.skill_points[sk];

    you.skill_points[sk] += points;
    you.total_skill_points += points;

    _check_skill_cost_change();

    if (change_level)
        _check_skill_level_change(sk);
}

static int _exercise2(skill_type exsk)
{
    // Being better at some magic skills makes others less likely to train.
    // Note: applies to Invocations and Evocations, too! [rob]
    if (_skip_exercise(exsk))
        return (0);

    // Don't train past level 27, even if the level hasn't been updated yet.
    if (you.skill_points[exsk] >= skill_exp_needed(27, exsk))
        return 0;

    // This will be added to you.skill_points[exsk];
    int skill_inc = 10;

    // This will be deducted from you.exp_available.
    int cost = _calc_skill_cost(you.skill_cost_level, you.skills[exsk]);

    // Being good at some weapons makes others easier to learn.
    if (exsk < SK_ARMOUR)
        skill_inc *= _weap_crosstrain_bonus(exsk);

    // Starting to learn skills is easier if the appropriate stat is high.
    if (you.skills[exsk] == 0)
        skill_inc = _stat_mult(exsk, skill_inc);

    // Spellcasting and Inv/Evo is cheaper early on.
    if (exsk >= SK_SPELLCASTING && exsk <= SK_EVOCATIONS)
    {
        if (you.skill_cost_level < 5)
            cost /= 2;
        else if (you.skill_cost_level < 15)
        {
            cost *= (10 + (you.skill_cost_level - 5));
            cost /= 20;
        }
    }

    // Scale cost and skill_inc to available experience.
    const int spending_limit = std::min(MAX_SPENDING_LIMIT, you.exp_available);
    if (cost > spending_limit)
    {
        int frac = (spending_limit * 10) / cost;

        // This system is a bit hard on missile weapons in the late
        // game, since they require expendable ammo in order to
        // practise.  Increasing skill_inc would make
        // missile weapons too easy earlier on, so, instead, we're
        // giving them a special case here.
        if (_discounted_throwing_skill(exsk)
            && cost <= you.exp_available)
        {
            // MAX_SPENDING_LIMIT < cost <= you.exp_available
            frac = ((cost / 2) > MAX_SPENDING_LIMIT) ? 5 : 10;
        }

        cost = spending_limit;
        skill_inc = (skill_inc * frac) / 10;
    }

    if (skill_inc <= 0)
        return (0);

    cost -= random2(5);            // XXX: what's this for?
    cost = std::max<int>(cost, 1); // No free lunch.

    you.skill_points[exsk] += skill_inc;
    you.ct_skill_points[exsk] += (1 - 1 / _weap_crosstrain_bonus(exsk))
                                 * skill_inc;
    you.exp_available -= cost;
    you.total_skill_points += skill_inc;

    _check_skill_cost_change();
    you.exp_available = std::max(0, you.exp_available);
    you.redraw_experience = true;

    return (skill_inc);
}
