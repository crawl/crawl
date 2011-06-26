/**
 * @file
 * @brief Skill exercising functions.
**/

#include "AppHdr.h"

#include "skills.h"

#include <algorithm>
#include <string.h>
#include <stdlib.h>

#include "externs.h"
#include "godabil.h"
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

int skill_cost_needed(int level)
{
    static bool init = true;
    static int scn[27];

    if (init)
    {
        // The progress of skill_cost_level depends only on total skill points,
        // it's independent of species. We try to keep close to the old system
        // and use minotaur as a reference (exp apt: 140). This means that for
        // a species with 140 exp apt, skill_cost_level will be about the same
        // as XL (a bit lower in the beginning).
        species_type sp = you.species;
        you.species = SP_MINOTAUR;

        // The average starting skill total is actually lower, but monks get
        // about 1200, and they would start around skill cost level 4 if we
        // used the average.
        scn[0] = 1200;

        for (int i = 1; i < 27; ++i)
        {
            scn[i] = scn[i - 1] + (exp_needed(i + 1) - exp_needed(i)) * 10
                                  / calc_skill_cost(i);
        }

        scn[0] = 0;
        you.species = sp;
        init = false;
    }

    return scn[level - 1];
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
int calc_skill_cost(int skill_cost_level)
{
    const int cost[] = { 1, 2, 3, 4, 5,            // 1-5
                         7, 9, 12, 15, 18,         // 6-10
                         28, 40, 56, 76, 100,      // 11-15
                         130, 165, 195, 215, 230,  // 16-20
                         240, 248, 250, 250, 250,  // 21-25
                         250, 250 };

    if (crawl_state.game_is_zotdef())
        return cost[skill_cost_level - 1] / 3 + 1;
    else
        return cost[skill_cost_level - 1];
}

// Characters are actually granted skill points, not skill levels.
// Here we take racial aptitudes into account in determining final
// skill levels.
void reassess_starting_skills()
{
    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
    {
        skill_type sk = static_cast<skill_type>(i);
        if (you.skills[sk] == 0
            && (you.species != SP_VAMPIRE || sk != SK_UNARMED_COMBAT))
        {
            continue;
        }

        // Grant the amount of skill points required for a human.
        you.skill_points[sk] = skill_exp_needed(you.skills[sk], sk,
        static_cast<species_type>(SP_HUMAN)) + 1;

        // Find out what level that earns this character.
        you.skills[sk] = 0;

        for (int lvl = 1; lvl <= 8; ++lvl)
        {
            if (you.skill_points[sk] > skill_exp_needed(lvl, sk))
                you.skills[sk] = lvl;
            else
                break;
        }

        // Vampires should always have Unarmed Combat skill.
        if (you.species == SP_VAMPIRE && sk == SK_UNARMED_COMBAT
            && you.skills[sk] < 1)
        {
            you.skill_points[sk] = skill_exp_needed(1, sk);
            you.skills[sk] = 1;
        }

        // Wanderers get at least 1 level in their skills.
        if (you.char_class == JOB_WANDERER && you.skills[sk] < 1)
        {
            you.skill_points[sk] = skill_exp_needed(1, sk);
            you.skills[sk] = 1;
        }

        // Spellcasters should always have Spellcasting skill.
        if (sk == SK_SPELLCASTING && you.skills[sk] < 1)
        {
            you.skill_points[sk] = skill_exp_needed(1, sk);
            you.skills[sk] = 1;
        }
    }
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
        mprf(MSGCH_INTRINSIC_GAIN, "You have mastered %s!", skill_name(exsk));
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
        learned_something_new(HINT_GAINED_SPELLCASTING);
    }

    const skill_type best = best_skill(SK_FIRST_SKILL, SK_LAST_SKILL);
    if (best != old_best_skill || old_best_skill == exsk)
        redraw_skill(you.your_name, player_title());

    // TODO: also identify rings of wizardry.
}

void check_skill_level_change(skill_type sk, bool do_level_up)
{
    int new_level = you.skills[sk];
    while (1)
    {
        const unsigned int prev = skill_exp_needed(new_level, sk);
        const unsigned int next = skill_exp_needed(new_level + 1, sk);

        if (you.skill_points[sk] >= next)
        {
            if (++new_level >= 27)
            {
                new_level = 27;
                break;
            }
        }
        else if (you.skill_points[sk] < prev)
        {
            new_level--;
            ASSERT(new_level >= 0);
        }
        else
            break;
    }

    if (new_level != you.skills[sk])
        if (do_level_up)
            _change_skill_level(sk, new_level - you.skills[sk]);
        else
            you.skills[sk] = new_level;
}

// returns total number of skill points gained
int exercise(skill_type exsk, int deg)
{
    int ret = 0;

#ifdef DEBUG_DIAGNOSTICS
    unsigned int exp_pool = you.exp_available;
#endif

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

    if (ret && !crawl_state.script)
    {
        // Disabled in script mode because it slows down the training simulator
        dprf("Exercised %s (deg: %d) by %d", skill_name(exsk), deg, ret);
        dprf("Cost %d experience points", exp_pool - you.exp_available);
    }

    check_skill_level_change(exsk);

    return (ret);
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

void change_skill_points(skill_type sk, int points, bool do_level_up)
{
    if (static_cast<int>(you.skill_points[sk]) < -points)
        points = -you.skill_points[sk];

    you.skill_points[sk] += points;
    you.total_skill_points += points;

    _check_skill_cost_change();

    check_skill_level_change(sk, do_level_up);
}

static int _exercise2(skill_type exsk)
{
    // Don't train past level 27, even if the level hasn't been updated yet.
    if (you.skill_points[exsk] >= skill_exp_needed(27, exsk))
        return 0;

    // This will be added to you.skill_points[exsk];
    int skill_inc = 10;

    // skill cost of level 1 has been reduced from 200 to 50. XP cost has
    // been raised to compensate, and this is to increase the number of needed
    // exercises to 10 (was 20).
    if (you.skills[exsk] == 0)
        skill_inc /= 2;

    // This will be deducted from you.exp_available.
    int cost = calc_skill_cost(you.skill_cost_level);

    // Being good at some weapons makes others easier to learn.
    if (exsk < SK_ARMOUR)
        skill_inc *= crosstrain_bonus(exsk);

    // Starting to learn skills is easier if the appropriate stat is high.
        // We check skill points in case skill level hasn't been updated yet
    if (you.skill_points[exsk] < skill_exp_needed(1, exsk))
        skill_inc = _stat_mult(exsk, skill_inc);

    if (is_antitrained(exsk))
        cost *= ANTITRAIN_PENALTY;

    // Scale cost and skill_inc to available experience.
    const int spending_limit = std::min(MAX_SPENDING_LIMIT, you.exp_available);
    if (cost > spending_limit)
    {
        int frac = (spending_limit * 10) / cost;
        cost = spending_limit;
        skill_inc = (skill_inc * frac) / 10;
    }

    if (skill_inc <= 0)
        return (0);

    you.skill_points[exsk] += skill_inc;
    you.ct_skill_points[exsk] += (1 - 1 / crosstrain_bonus(exsk))
                                 * skill_inc;
    you.exp_available -= cost;
    you.total_skill_points += skill_inc;

    _check_skill_cost_change();
    you.exp_available = std::max(0, you.exp_available);
    you.redraw_experience = true;

    return (skill_inc);
}
