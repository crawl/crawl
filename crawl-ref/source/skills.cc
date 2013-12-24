/**
 * @file
 * @brief Skill exercising functions.
**/

#include "AppHdr.h"

#include "skills.h"

#include <algorithm>
#include <cmath>
#include <string.h>
#include <stdlib.h>

#include "ability.h"
#include "evoke.h"
#include "exercise.h"
#include "externs.h"
#include "godconduct.h"
#include "hints.h"
#include "invent.h"
#include "itemprop.h"
#include "libutil.h"
#include "message.h"
#include "misc.h"
#include "notes.h"
#include "player.h"
#include "random.h"
#include "random-weight.h"
#include "skills2.h"
#include "spl-cast.h"
#include "sprint.h"
#include "state.h"
#include "stuff.h"

// MAX_COST_LIMIT is the maximum XP amount it will cost to raise a skill
//                by 10 skill points (ie one standard practice).
//
// MAX_SPENDING_LIMIT is the maximum XP amount we allow the player to
//                    spend on a skill in a single raise.
//
// Note that they don't have to be equal, but it is important to make
// sure that they're set so that the spending limit will always allow
// for 1 skill point to be earned.
#define MAX_COST_LIMIT           265
#define MAX_SPENDING_LIMIT       265

static int _train(skill_type exsk, int &max_exp, bool simu = false);
static void _train_skills(int exp, const int cost, const bool simu);

// The progress of skill_cost_level depends only on total experience points,
// it's independent of species. We try to keep close to the old system
// and use an experience aptitude of 130 as a reference (Tengu).
// This means that for a species with 130 exp apt, skill_cost_level should be
// the same as XL (unless the player has been drained).

// 130 exp apt is midway between +0 and -1 now. -- elliptic
unsigned int skill_cost_needed(int level)
{
    return (exp_needed(level, 1) * 13) / 10;
}

// skill_cost_level makes skills more expensive for more experienced characters
int calc_skill_cost(int skill_cost_level)
{
    const int cost[] = { 1, 2, 3, 4, 5,            // 1-5
                         7, 8, 9, 13, 22,         // 6-10
                         37, 48, 73, 98, 125,      // 11-15
                         145, 170, 190, 212, 225,  // 16-20
                         240, 255, 260, 265, 265,  // 21-25
                         265, 265 };

    ASSERT_RANGE(skill_cost_level, 1, 27 + 1);
    return cost[skill_cost_level - 1];
}

// Characters are actually granted skill points, not skill levels.
// Here we take racial aptitudes into account in determining final
// skill levels.
void reassess_starting_skills()
{
    // go backwards, need to do Dodging before Armour
    for (int i = NUM_SKILLS - 1; i >= SK_FIRST_SKILL; --i)
    {
        skill_type sk = static_cast<skill_type>(i);
        ASSERT(you.skills[sk] == 0 || !is_useless_skill(sk));

        // Grant the amount of skill points required for a human.
        you.skill_points[sk] = you.skills[sk] ?
            skill_exp_needed(you.skills[sk], sk, SP_HUMAN) + 1 : 0;

        if (sk == SK_DODGING && you.skills[SK_ARMOUR]
            && (is_useless_skill(SK_ARMOUR) || !you_can_wear(EQ_BODY_ARMOUR)))
        {
            // No one who can't wear mundane heavy armour shouldn't start with
            // the Armour skill -- D:1 dragon armour is too unlikely.
            you.skill_points[sk] += skill_exp_needed(you.skills[SK_ARMOUR],
                SK_ARMOUR, SP_HUMAN) + 1;
            you.skills[SK_ARMOUR] = 0;
        }

        if (!you.skill_points[sk])
            continue;

        // Find out what level that earns this character.
        you.skills[sk] = 0;

        for (int lvl = 1; lvl <= 8; ++lvl)
        {
            if (you.skill_points[sk] > skill_exp_needed(lvl, sk))
                you.skills[sk] = lvl;
            else
                break;
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
    bool need_reset = false;

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
        for (int i = you.sage_skills.size() - 1; i >= 0; i--)
        {
            if (you.sage_skills[i] == exsk)
            {
                erase_any(you.sage_skills, i);
                erase_any(you.sage_xp,     i);
                erase_any(you.sage_bonus,  i);
            }
        }
    }
    else if (abs(n) == 1 && you.num_turns)
    {
        mprf(MSGCH_INTRINSIC_GAIN, "Your %s skill %s to level %d!",
             skill_name(exsk), (n > 0) ? "increases" : "decreases",
             you.skills[exsk]);
    }
    else if (you.num_turns)
    {
        mprf(MSGCH_INTRINSIC_GAIN, "Your %s skill %s %d levels and is now at "
             "level %d!", skill_name(exsk), (n > 0) ? "gained" : "lost",
             abs(n), you.skills[exsk]);
    }

    if (you.skills[exsk] == n && n > 0)
        hints_gained_new_skill(exsk);

    if (n > 0 && you.num_turns)
        learned_something_new(HINT_SKILL_RAISE);

    if (you.skills[exsk] - n == 27)
    {
        you.train[exsk] = 1;
        need_reset = true;
    }

    if (exsk == SK_SPELLCASTING && you.skills[exsk] == n && n > 0)
        learned_something_new(HINT_GAINED_SPELLCASTING);

    if (need_reset)
        reset_training();

    // calc_hp() has to be called here because it currently doesn't work
    // right if you.skills[] hasn't been updated yet.
    if (exsk == SK_FIGHTING)
        calc_hp();
}

// Called whenever a skill is trained.
void redraw_skill(skill_type exsk, skill_type old_best_skill)
{
    if (exsk == SK_FIGHTING)
        calc_hp();

    if (exsk == SK_INVOCATIONS || exsk == SK_SPELLCASTING || exsk == SK_EVOCATIONS)
        calc_mp();

    if (exsk == SK_DODGING || exsk == SK_ARMOUR)
        you.redraw_evasion = true;

    if (exsk == SK_ARMOUR || exsk == SK_SHIELDS || exsk == SK_ICE_MAGIC
        || exsk == SK_EARTH_MAGIC || you.duration[DUR_TRANSFORMATION] > 0)
    {
        you.redraw_armour_class = true;
    }

    // Recalculate this skill's order for tie breaking skills
    // at its new level.   See skills2.cc::init_skill_order()
    // for more details.  -- bwr
    you.skill_order[exsk] = 0;
    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
    {
        skill_type sk = static_cast<skill_type>(i);
        if (sk != exsk && you.skill(sk, 10, true) >= you.skill(exsk, 10, true))
            you.skill_order[exsk]++;
    }

    const skill_type best = best_skill(SK_FIRST_SKILL, SK_LAST_SKILL);
    if (best != old_best_skill || old_best_skill == exsk)
    {
        you.redraw_title = true;
        // The player symbol depends on best skill title.
        update_player_symbol();
    }

    // Identify weapon pluses.
    if (exsk <= SK_THROWING)
        auto_id_inventory();
}

void check_skill_level_change(skill_type sk, bool do_level_up)
{
    int new_level = you.skills[sk];
    while (1)
    {
        if (new_level < 27
            && you.skill_points[sk] >= skill_exp_needed(new_level + 1, sk))
        {
            ++new_level;
        }
        else if (you.skill_points[sk] < skill_exp_needed(new_level, sk))
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

// Fill a queue in random order with the values of the array.
template <typename T, int SIZE>
static void _init_queue(list<skill_type> &queue, FixedVector<T, SIZE> &array)
{
    ASSERT(queue.empty());

    while (1)
    {
        skill_type sk = (skill_type)random_choose_weighted(array);
        if (is_invalid_skill(sk))
            break;
        queue.push_back(sk);
        --array[sk];
    }

    ASSERT(queue.size() == (unsigned)EXERCISE_QUEUE_SIZE);
}

static void _erase_from_stop_train(skill_set &can_train)
{
    for (skill_set_iter it = can_train.begin(); it != can_train.end(); ++it)
    {
        skill_set_iter it2 = you.stop_train.find(*it);
        if (it2 != you.stop_train.end())
            you.stop_train.erase(it2);
    }
}

/*
 * Check the inventory to see what skills the player can train,
 * among the ones in you.stop_train.
 * Trainable skills are removed from the set.
 */
static void _check_inventory_skills()
{
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        // Exit early if there's no more skill to check.
        if (you.stop_train.empty())
            return;

        skill_set skills;
        if (!you.inv[i].defined() || !item_skills(you.inv[i], skills))
            continue;

        _erase_from_stop_train(skills);
    }
}

static void _check_equipment_skills()
{
    skill_set_iter it = you.stop_train.find(SK_SHIELDS);
    if (it != you.stop_train.end() && you.slot_item(EQ_SHIELD, true))
        you.stop_train.erase(it);
}

static void _check_spell_skills()
{
    for (int i = 0; i < MAX_KNOWN_SPELLS; i++)
    {
        // Exit early if there's no more skill to check.
        if (you.stop_train.empty())
            return;

        if (you.spells[i] == SPELL_NO_SPELL)
            continue;

        skill_set skills;
        spell_skills(you.spells[i], skills);
        _erase_from_stop_train(skills);
    }
}

static void _check_abil_skills()
{
    vector<ability_type> abilities = get_god_abilities(true);
    for (unsigned int i = 0; i < abilities.size(); ++i)
    {
        // Exit early if there's no more skill to check.
        if (you.stop_train.empty())
            return;

        skill_set_iter it = you.stop_train.find(abil_skill(abilities[i]));
        if (it != you.stop_train.end())
            you.stop_train.erase(it);

        if (abilities[i] == ABIL_TSO_DIVINE_SHIELD
            && you.stop_train.count(SK_SHIELDS))
        {
            you.stop_train.erase(SK_SHIELDS);
        }
    }
}

string skill_names(skill_set &skills)
{
    string s;
    int i = 0;
    int size = skills.size();
    for (skill_set_iter it = skills.begin(); it != skills.end(); ++it)
    {
        ++i;
        s += skill_name(*it);
        if (i < size)
        {
            if (i == size - 1)
                s += " and ";
            else
                s+= ", ";
        }
    }
    return s;
}

static void _check_start_train()
{
    skill_set skills;
    for (skill_set_iter it = you.start_train.begin();
             it != you.start_train.end(); ++it)
    {
        if (is_invalid_skill(*it) || is_useless_skill(*it))
            continue;

        if (!you.can_train[*it] && you.train[*it])
            skills.insert(*it);
        you.can_train.set(*it);
    }

    reset_training();

    // We're careful of not invalidating the iterator when erasing.
    for (skill_set_iter it = skills.begin(); it != skills.end();)
        if (!you.training[*it])
            skills.erase(it++);
        else
            ++it;

    if (!skills.empty())
        mprf("You resume training %s.", skill_names(skills).c_str());

    you.start_train.clear();
}

static void _check_stop_train()
{
    _check_inventory_skills();
    _check_equipment_skills();
    _check_spell_skills();
    _check_abil_skills();

    if (you.stop_train.empty())
        return;

    skill_set skills;
    for (skill_set_iter it = you.stop_train.begin();
         it != you.stop_train.end(); ++it)
    {
        if (is_invalid_skill(*it))
            continue;
        if (skill_has_manual(*it))
            continue;

        if (skill_trained(*it) && you.training[*it])
            skills.insert(*it);
        you.can_train.set(*it, false);
    }

    if (!skills.empty())
    {
        mprf("You stop training %s.", skill_names(skills).c_str());
        check_selected_skills();
    }

    reset_training();
    you.stop_train.clear();
}

void update_can_train()
{
    if (!you.start_train.empty())
        _check_start_train();

    if (!you.stop_train.empty())
        _check_stop_train();
}

bool training_restricted(skill_type sk)
{
    switch (sk)
    {
    case SK_FIGHTING:
    // Requiring missiles would mean disabling the skill when you run out.
    case SK_THROWING:
    case SK_ARMOUR:
    case SK_DODGING:
    case SK_STEALTH:
    case SK_UNARMED_COMBAT:
    case SK_SPELLCASTING:
        return false;
    default:
        return true;
    }
}

/*
 * Init the can_train array by examining inventory and spell list to see which
 * skills can be trained.
 */
void init_can_train()
{
    // Clear everything out, in case this isn't the first game.
    you.start_train.clear();
    you.stop_train.clear();
    you.can_train.reset();

    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        const skill_type sk = skill_type(i);

        if (is_useless_skill(sk))
            continue;

        you.can_train.set(sk);
        if (training_restricted(sk))
            you.stop_train.insert(sk);
    }

    _check_stop_train();
}

void init_train()
{
    for (int i = 0; i < NUM_SKILLS; ++i)
        if (you.can_train[i] && you.skill_points[i])
            you.train[i] = you.train_alt[i] = true;
        else
        {
            // Skills are on by default in auto mode and off in manual.
            you.train[i] = you.auto_training;
            you.train_alt[i] = !you.auto_training;
        }
}

static bool _cmp_rest(const pair<skill_type, int64_t>& a,
                      const pair<skill_type, int64_t>& b)
{
    return a.second < b.second;
}

/*
 * Scale an array.
 *
 * @param array The array to be scaled.
 * @param scale The new scale of the array.
 * @param exact When true, make sure that the sum of the array elements
 *              is equal to the scale.
 */
template <typename T, int SIZE>
static void _scale_array(FixedVector<T, SIZE> &array, int scale, bool exact)
{
    int64_t total = 0;
    // First, we calculate the sum of the values to be scaled.
    for (int i = 0; i < NUM_SKILLS; ++i)
        total += array[i];

    vector<pair<skill_type, int64_t> > rests;
    int scaled_total = 0;

    // All skills disabled, nothing to do.
    if (!total)
        return;

    // Now we scale the values.
    for (int i = 0; i < NUM_SKILLS; ++i)
        if (array[i] > 0)
        {
            int64_t result = (int64_t)array[i] * (int64_t)scale;
            const int64_t rest = result % total;
            if (rest)
                rests.push_back(pair<skill_type, int64_t>(skill_type(i), rest));
            array[i] = (int)(result / total);
            scaled_total += array[i];
        }

    ASSERT(scaled_total <= scale);

    if (!exact || scaled_total == scale)
        return;

    // We ensure that the percentage always add up to 100 by increasing the
    // training for skills which had the higher rest from the above scaling.
    sort(rests.begin(), rests.end(), _cmp_rest);
    vector<pair<skill_type, int64_t> >::iterator it = rests.begin();
    while (scaled_total < scale && it != rests.end())
    {
        ++array[it->first];
        ++scaled_total;
        ++it;
    }

    ASSERT(scaled_total == scale);
}

/*
 * Init the training array by scaling down the skill_points array to 100.
 * Used at game setup, when upgrading saves and when loading dump files.
 */
void init_training()
{
    FixedVector<unsigned int, NUM_SKILLS> skills;
    skills.init(0);
    for (int i = 0; i < NUM_SKILLS; ++i)
        if (skill_trained(i))
            skills[i] = sqr(you.skill_points[i]);

    _scale_array(skills, EXERCISE_QUEUE_SIZE, true);
    _init_queue(you.exercises, skills);

    for (int i = 0; i < NUM_SKILLS; ++i)
        skills[i] = sqr(you.skill_points[i]);

    _scale_array(skills, EXERCISE_QUEUE_SIZE, true);
    _init_queue(you.exercises_all, skills);

    reset_training();
}

// Make sure at least one skill is selected.
// If not, go to the skill menu and return true.
bool check_selected_skills()
{
    bool trainable_skill = false;
    bool could_train = false;
    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        skill_type sk = static_cast<skill_type>(i);
        if (skill_trained(sk))
            return false;
        if (is_useless_skill(sk) || is_harmful_skill(sk)
            || you.skill_points[sk] >= skill_exp_needed(27, sk))
        {
            continue;
        }
        if (!you.can_train[sk])
        {
            could_train = true;
            continue;
        }
        else
            trainable_skill = true;
    }

    if (trainable_skill)
    {
        mpr("You need to enable at least one skill for training.");
        more();
        reset_training();
        skill_menu();
        redraw_screen();
        return true;
    }

    if (could_train && !you.received_noskill_warning)
    {
        you.received_noskill_warning = true;
        mpr("You cannot train any new skill.");
    }

    return false;
    // It's possible to have no selectable skills, if they are all untrainable
    // or level 27, so we don't assert.
}

/*
 * Reset the training array. Disabled skills are skipped.
 * In automatic mode, we use values from the exercise queue.
 * In manual mode, all enabled skills are set to the same value.
 * Result is scaled back to 100.
 */
void reset_training()
{
    // We clear the values in the training array. In auto mode they are set
    // to 0 (and filled later with the content of the queue), in manual mode,
    // the trainable ones are set to 1 (or 2 for focus).
    for (int i = 0; i < NUM_SKILLS; ++i)
        if (you.auto_training || !skill_trained(i))
            you.training[i] = 0;
        else
            you.training[i] = you.train[i];

    bool empty = true;
    // In automatic mode, we fill the array with the content of the queue.
    if (you.auto_training)
    {
        for (list<skill_type>::iterator it = you.exercises.begin();
             it != you.exercises.end(); ++it)
        {
            skill_type sk = *it;
            if (skill_trained(sk))
            {
                you.training[sk] += you.train[sk];
                empty = false;
            }
        }

        // We count the practise events in the other queue.
        FixedVector<unsigned int, NUM_SKILLS> exer_all;
        exer_all.init(0);
        for (list<skill_type>::iterator it = you.exercises_all.begin();
             it != you.exercises_all.end(); ++it)
        {
            skill_type sk = *it;
            if (skill_trained(sk))
            {
                exer_all[sk] += you.train[sk];
                empty = false;
            }
        }

        // We keep the highest of the 2 numbers.
        for (int sk = 0; sk < NUM_SKILLS; ++sk)
            you.training[sk] = max(you.training[sk], exer_all[sk]);

        // The selected skills have not been exercised recently. Give them all
        // a default weight of 1 (or 2 for focus skills).
        if (empty)
        {
            for (int sk = 0; sk < NUM_SKILLS; ++sk)
                if (skill_trained(sk))
                    you.training[sk] = you.train[sk];
        }

        // Focused skills get at least 20% training.
        for (int sk = 0; sk < NUM_SKILLS; ++sk)
            if (you.train[sk] == 2 && you.training[sk] < 20 && you.can_train[sk])
                you.training[sk] += 5 * (5 - you.training[sk] / 4);
    }

    _scale_array(you.training, 100, you.auto_training);
}

void exercise(skill_type exsk, int deg)
{
    if (you.skills[exsk] >= 27)
        return;

    dprf("Exercise %s by %d.", skill_name(exsk), deg);

    // push first in case queues are empty, like during -test
    while (deg > 0)
    {
        if (skill_trained(exsk))
        {
            you.exercises.push_back(exsk);
            you.exercises.pop_front();
        }
        you.exercises_all.push_back(exsk);
        you.exercises_all.pop_front();
        deg--;
    }
    reset_training();
}

// Check if we should stop training this skill immediately.
// We look at skill points because actual level up comes later.
static bool _level_up_check(skill_type sk, bool simu)
{
    // Don't train past level 27.
    if (you.skill_points[sk] >= skill_exp_needed(27, sk))
    {
        you.training[sk] = 0;
        if (!simu)
        {
            you.train[sk] = 0;
            you.train_alt[sk] = 0;
        }
        return true;
    }

    return false;
}

bool is_magic_skill(skill_type sk)
{
    return sk > SK_LAST_MUNDANE && sk <= SK_LAST_MAGIC;
}

void train_skills(bool simu)
{
    int cost, exp;
    do
    {
        cost = calc_skill_cost(you.skill_cost_level);
        exp = you.exp_available;
        if (you.skill_cost_level == 27)
            _train_skills(exp, cost, simu);
        else
        {
            // Amount of experience points needed to reach the next skill cost level
            const int next_level = skill_cost_needed(you.skill_cost_level + 1)
                                   - you.total_experience;
            ASSERT(next_level > 0);
            _train_skills(min(exp, next_level + cost - 1), cost, simu);
        }
    }
    while (you.exp_available >= cost && exp != you.exp_available);

    for (int i = 0; i < NUM_SKILLS; ++i)
        check_skill_level_change(static_cast<skill_type>(i), !simu);

    // We might have disabled some skills on level up.
    reset_training();
}

//#define DEBUG_TRAINING_COST
static void _train_skills(int exp, const int cost, const bool simu)
{
    bool skip_first_phase = false;
    int magic_gain = 0;
    FixedVector<int, NUM_SKILLS> sk_exp;
    sk_exp.init(0);
    vector<skill_type> training_order;
#ifdef DEBUG_DIAGNOSTICS
    FixedVector<int, NUM_SKILLS> total_gain;
    total_gain.init(0);
#endif
#ifdef DEBUG_TRAINING_COST
    int exp_pool = you.exp_available;
    dprf("skill cost level: %d, cost: %dxp/10skp, max XP usable: %d.",
         you.skill_cost_level, cost, exp);
#endif

    // We scale the training array to the amount of XP available in the pool.
    // That gives us the amount of XP available to train each skill.
    for (int i = 0; i < NUM_SKILLS; ++i)
        if (you.training[i] > 0)
        {
            sk_exp[i] = you.training[i] * exp / 100;
            if (sk_exp[i] < cost)
            {
                // One skill has a too low training to be trained at all.
                // We skip the first phase and go directly to the random
                // phase so it has a chance to be trained.
                skip_first_phase = true;
                break;
            }
            training_order.push_back(static_cast<skill_type>(i));
        }

    if (!skip_first_phase)
    {
        // We randomize the order, to avoid a slight bias to first skills.
        // Being trained first can make a difference if skill cost increases.
        shuffle_array(training_order);
        for (vector<skill_type>::iterator it = training_order.begin();
             it != training_order.end(); ++it)
        {
            skill_type sk = *it;
            int gain = 0;

            while (sk_exp[sk] >= cost && you.training[sk])
            {
                exp -= sk_exp[sk];
                gain += _train(sk, sk_exp[sk], simu);
                exp += sk_exp[sk];
                ASSERT(exp >= 0);
                if (_level_up_check(sk, simu))
                    sk_exp[sk] = 0;
            }

            if (gain && is_magic_skill(sk))
                magic_gain += gain;

#ifdef DEBUG_DIAGNOSTICS
           total_gain[sk] += gain;
#endif
        }
    }
    // If there's enough xp in the pool, we use it to train skills selected
    // with random_choose_weighted.
    while (exp >= cost)
    {
        int gain;
        skill_type sk = SK_NONE;
        if (!skip_first_phase)
            sk = static_cast<skill_type>(random_choose_weighted(sk_exp));
        if (is_invalid_skill(sk))
            sk = static_cast<skill_type>(random_choose_weighted(you.training));
        if (!is_invalid_skill(sk))
        {
            gain = _train(sk, exp, simu);
            ASSERT(exp >= 0);
            sk_exp[sk] = 0;
        }
        else
        {
            // No skill to train. Can happen if all skills are at 27.
            break;
        }

        _level_up_check(sk, simu);

        if (gain && is_magic_skill(sk))
            magic_gain += gain;

#ifdef DEBUG_DIAGNOSTICS
        total_gain[sk] += gain;
#endif
    }

#ifdef DEBUG_DIAGNOSTICS
    if (!crawl_state.script)
    {
#ifdef DEBUG_TRAINING_COST
        int total = 0;
#endif
        for (int i = 0; i < NUM_SKILLS; ++i)
        {
            skill_type sk = static_cast<skill_type>(i);
            if (total_gain[sk] && !simu)
                dprf("Trained %s by %d.", skill_name(sk), total_gain[sk]);
#ifdef DEBUG_TRAINING_COST
            total += total_gain[sk];
        }
        dprf("Total skill points gained: %d, cost: %d XP.",
             total, exp_pool - you.exp_available);
#else
        }
#endif
    }
#endif

    // Avoid doubly rewarding spell practise in sprint
    // (by inflated XP and inflated piety gain)
    if (crawl_state.game_is_sprint())
        magic_gain = sprint_modify_exp_inverse(magic_gain);

    if (magic_gain && !simu)
        did_god_conduct(DID_SPELL_PRACTISE, magic_gain);
}

bool skill_trained(int i)
{
    return you.can_train[i] && you.train[i];
}

void train_skill(skill_type skill, int exp)
{
    const int cost = calc_skill_cost(you.skill_cost_level);
    int gain = 0;

    while (exp >= cost)
        gain += _train(skill, exp);

    dprf("Trained %s by %d.", skill_name(skill), gain);
}

void check_skill_cost_change()
{
    while (you.skill_cost_level < 27
           && you.total_experience >= skill_cost_needed(you.skill_cost_level + 1))
    {
        ++you.skill_cost_level;
    }
    while (you.skill_cost_level > 0
           && you.total_experience < skill_cost_needed(you.skill_cost_level))
    {
        --you.skill_cost_level;
    }
}

void change_skill_points(skill_type sk, int points, bool do_level_up)
{
    if (static_cast<int>(you.skill_points[sk]) < -points)
        points = -(int)you.skill_points[sk];

    you.skill_points[sk] += points;

    check_skill_level_change(sk, do_level_up);
}

static int _train(skill_type exsk, int &max_exp, bool simu)
{
    // This will be added to you.skill_points[exsk];
    int skill_inc = 10;

    // This will be deducted from you.exp_available.
    int cost = calc_skill_cost(you.skill_cost_level);

    // Being good at some weapons makes others easier to learn.
    if (exsk < SK_ARMOUR)
        skill_inc *= crosstrain_bonus(exsk);

    if (is_antitrained(exsk))
        cost *= ANTITRAIN_PENALTY;

    // Scale cost and skill_inc to available experience.
    const int spending_limit = min(MAX_SPENDING_LIMIT, max_exp);
    if (cost > spending_limit)
    {
        int frac = spending_limit * 10 / cost;
        cost = spending_limit;
        skill_inc = skill_inc * frac / 10;
    }

    if (skill_inc <= 0)
        return 0;

    // Bonus from manual
    int slot;
    int bonus_left = skill_inc;
    while (bonus_left > 0 && (slot = manual_slot_for_skill(exsk)) != -1)
    {
        item_def& manual(you.inv[slot]);
        const int bonus = min<int>(bonus_left, manual.plus2);
        skill_inc += bonus;
        bonus_left -= bonus;
        manual.plus2 -= bonus;
        if (!manual.plus2 && !simu)
            finish_manual(slot);
    }

    const skill_type old_best_skill = best_skill(SK_FIRST_SKILL, SK_LAST_SKILL);
    you.skill_points[exsk] += skill_inc;
    you.ct_skill_points[exsk] += (1 - 1 / crosstrain_bonus(exsk))
                                 * skill_inc;
    you.exp_available -= cost;
    you.total_experience += cost;
    max_exp -= cost;

    if (!simu)
        redraw_skill(exsk, old_best_skill);

    check_skill_cost_change();
    ASSERT(you.exp_available >= 0);
    ASSERT(max_exp >= 0);
    you.redraw_experience = true;

    return skill_inc;
}

void set_skill_level(skill_type skill, double amount)
{
    double level;
    double fractional = modf(amount, &level);

    you.ct_skill_points[skill] = 0;
    you.skills[skill] = level;

    if (level >= 27)
    {
        level = 27;
        fractional = 0;
    }

    unsigned int target = skill_exp_needed(level, skill);
    if (fractional)
    {
        target += (skill_exp_needed(level + 1, skill)
                  - skill_exp_needed(level, skill)) * fractional + 1;
    }

    if (target == you.skill_points[skill])
        return;

    // We're updating you.skill_points[skill] and calculating the new
    // you.total_experience to update skill cost.

    const bool reduced = target < you.skill_points[skill];

#ifdef DEBUG_TRAINING_COST
    dprf("target: %d.", target);
#endif
    while (you.skill_points[skill] != target)
    {
        int next_level = reduced ? skill_cost_needed(you.skill_cost_level)
                                 : skill_cost_needed(you.skill_cost_level + 1);
        int max_xp = abs(next_level - (int)you.total_experience);

        // When reducing, we don't want to stop right at the limit, unless
        // we're at skill cost level 0.
        if (reduced && you.skill_cost_level)
            ++max_xp;

        int cost = calc_skill_cost(you.skill_cost_level);
        // Maximum number of skill points to transfer in one go.
        // It's max_xp*10/cost rounded up.
        int max_skp = (max_xp * 10 + cost - 1) / cost;
        max_skp = max(max_skp, 1);
        int delta_skp = min<int>(abs((int)(target - you.skill_points[skill])),
                                 max_skp);
        int delta_xp = (delta_skp * cost + 9) / 10;

        if (reduced)
        {
            delta_skp = -min<int>(delta_skp, you.skill_points[skill]);
            delta_xp = -min<int>(delta_xp, you.total_experience);
        }

#ifdef DEBUG_TRAINING_COST
        dprf("cost level: %d, total experience: %d, next level: %d, "
             "skill points: %d, delta_skp: %d, delta_xp: %d.",
             you.skill_cost_level, you.total_experience, next_level,
             you.skill_points[skill], delta_skp, delta_xp);
#endif
        you.skill_points[skill] += delta_skp;
        you.total_experience += delta_xp;
        check_skill_cost_change();
    }
}
