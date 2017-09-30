/**
 * @file
 * @brief Skill exercising functions.
**/

#include "AppHdr.h"

#include "skills.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <sstream>

#include "ability.h"
#include "describe-god.h"
#include "evoke.h"
#include "exercise.h"
#include "god-abil.h"
#include "god-conduct.h"
#include "god-passive.h"
#include "hints.h"
#include "item-prop.h"
#include "libutil.h"
#include "message.h"
#include "misc.h"
#include "notes.h"
#include "output.h"
#include "random.h"
#include "religion.h"
#include "skill-menu.h"
#include "sprint.h"
#include "state.h"
#include "stringutil.h"
#include "unwind.h"

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

// Basic goals for titles:
// The higher titles must come last.
// Referring to the skill itself is fine ("Transmuter") but not impressive.
// No overlaps, high diversity.

// See the map "replacements" below for what @Genus@, @Adj@, etc. do.
// NOTE: Even though @foo@ could be used with most of these, remember that
// the character's race will be listed on the next line. It's only really
// intended for cases where things might be really awkward without it. -- bwr

// NOTE: If a skill name is changed, remember to also adapt the database entry.
static const char *skill_titles[NUM_SKILLS][6] =
{
  //  Skill name        levels 1-7       levels 8-14        levels 15-20       levels 21-26      level 27
    {"Fighting",       "Skirmisher",    "Fighter",         "Warrior",         "Slayer",         "Conqueror"},
    {"Short Blades",   "Cutter",        "Slicer",          "Swashbuckler",    "Cutthroat",      "Politician"},
    {"Long Blades",    "Slasher",       "Carver",          "Fencer",          "@Adj@ Blade",    "Swordmaster"},
    {"Axes",           "Chopper",       "Cleaver",         "Severer",         "Executioner",    "Axe Maniac"},
    {"Maces & Flails", "Cudgeler",      "Basher",          "Bludgeoner",      "Shatterer",      "Skullcrusher"},
    {"Polearms",       "Poker",         "Spear-Bearer",    "Impaler",         "Phalangite",     "@Adj@ Porcupine"},
    {"Staves",         "Twirler",       "Cruncher",        "Stickfighter",    "Pulveriser",     "Chief of Staff"},
    {"Slings",         "Vandal",        "Slinger",         "Whirler",         "Slingshot",      "@Adj@ Catapult"},
    {"Bows",           "Shooter",       "Archer",          "Marks@genus@",    "Crack Shot",     "Merry @Genus@"},
    {"Crossbows",      "Bolt Thrower",  "Quickloader",     "Sharpshooter",    "Sniper",         "@Adj@ Arbalest"},
    {"Throwing",       "Chucker",       "Thrower",         "Deadly Accurate", "Hawkeye",        "@Adj@ Ballista"},
    {"Armour",         "Covered",       "Protected",       "Tortoise",        "Impregnable",    "Invulnerable"},
    {"Dodging",        "Ducker",        "Nimble",          "Spry",            "Acrobat",        "Intangible"},
    {"Stealth",        "Sneak",         "Covert",          "Unseen",          "Imperceptible",  "Ninja"},
#if TAG_MAJOR_VERSION == 34
    {"Stabbing",       "Miscreant",     "Blackguard",      "Backstabber",     "Cutthroat",      "Politician"},
#endif
    {"Shields",        "Shield-Bearer", "Blocker",         "Peltast",         "Hoplite",        "@Adj@ Barricade"},
#if TAG_MAJOR_VERSION == 34
    {"Traps",          "Scout",         "Disarmer",        "Vigilant",        "Perceptive",     "Dungeon Master"},
#endif
    // STR based fighters, for DEX/martial arts titles see below. Felids get their own category, too.
    {"Unarmed Combat", "Ruffian",       "Grappler",        "Brawler",         "Wrestler",       "@Weight@weight Champion"},

    {"Spellcasting",   "Magician",      "Thaumaturge",     "Eclecticist",     "Sorcerer",       "Archmage"},
    {"Conjurations",   "Conjurer",      "Destroyer",       "Devastator",      "Ruinous",        "Annihilator"},
    {"Hexes",          "Vexing",        "Jinx",            "Bewitcher",       "Maledictor",     "Spellbinder"},
    {"Charms",         "Charmwright",   "Infuser",         "Anointer",        "Gracecrafter",   "Miracle Worker"},
    {"Summonings",     "Caller",        "Summoner",        "Convoker",        "Demonologist",   "Hellbinder"},
    {"Necromancy",     "Grave Robber",  "Reanimator",      "Necromancer",     "Thanatomancer",  "@Genus_Short@ of Death"},
    {"Translocations", "Grasshopper",   "Placeless @Genus@", "Blinker",       "Portalist",      "Plane @Walker@"},
    {"Transmutations", "Changer",       "Transmogrifier",  "Alchemist",       "Malleable",      "Shapeless @Genus@"},

    {"Fire Magic",     "Firebug",       "Arsonist",        "Scorcher",        "Pyromancer",     "Infernalist"},
    {"Ice Magic",      "Chiller",       "Frost Mage",      "Gelid",           "Cryomancer",     "Englaciator"},
    {"Air Magic",      "Gusty",         "Zephyrmancer",    "Stormcaller",     "Cloud Mage",     "Meteorologist"},
    {"Earth Magic",    "Digger",        "Geomancer",       "Earth Mage",      "Metallomancer",  "Petrodigitator"},
    {"Poison Magic",   "Stinger",       "Tainter",         "Polluter",        "Contaminator",   "Envenomancer"},

    // These titles apply to atheists only, worshippers of the various gods
    // use the god titles instead, depending on piety or, in Gozag's case, gold.
    // or, in U's case, invocations skill.
    {"Invocations",    "Unbeliever",    "Agnostic",        "Dissident",       "Heretic",        "Apostate"},
    {"Evocations",     "Charlatan",     "Prestidigitator", "Fetichist",       "Evocator",       "Talismancer"},
};

static const char *martial_arts_titles[6] =
    {"Unarmed Combat", "Insei", "Martial Artist", "Black Belt", "Sensei", "Grand Master"};
static const char *claw_and_tooth_titles[6] =
    {"Unarmed Combat", "Scratcher", "Gouger", "Ripper", "Eviscerator", "Sabretooth"};

struct species_skill_aptitude
{
    species_type species;
    skill_type   skill;
    int aptitude;          // -50..50, with 0 for humans

    species_skill_aptitude(species_type _species,
                           skill_type _skill,
                           int _aptitude)
        : species(_species), skill(_skill), aptitude(_aptitude)
    {
    }
};

#include "aptitudes.h"

// Traditionally, Spellcasting and In/Evocations formed the exceptions here:
// Spellcasting skill was more expensive with about 130%, the other two got
// a discount with about 75%.
static int _spec_skills[NUM_SPECIES][NUM_SKILLS];

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

static const int MAX_SKILL_COST_LEVEL = 27;

// skill_cost_level makes skills more expensive for more experienced characters
int calc_skill_cost(int skill_cost_level)
{
    const int cost[] = { 1, 2, 3, 4, 5,            // 1-5
                         7, 8, 9, 13, 22,         // 6-10
                         37, 48, 73, 98, 125,      // 11-15
                         145, 170, 190, 212, 225,  // 16-20
                         240, 255, 260, 265, 265,  // 21-25
                         265, 265 };
    COMPILE_CHECK(ARRAYSZ(cost) == MAX_SKILL_COST_LEVEL);

    ASSERT_RANGE(skill_cost_level, 1, MAX_SKILL_COST_LEVEL + 1);
    return cost[skill_cost_level - 1];
}

/**
 * The baseline skill cost for the 'cost' interface on the m screen.
 *
 * @returns the XP needed to go from level 0 to level 1 with +0 apt.
 */
int skill_cost_baseline()
{
    return skill_exp_needed(1, SK_FIGHTING, SP_HUMAN)
           - skill_exp_needed(0, SK_FIGHTING, SP_HUMAN);
}

/**
 * The skill cost to increase the given skill from its current level by one.
 *
 * @param sk the skill to check the player's level of
 * @returns the XP needed to increase from floor(level) to ceiling(level)
 */
int one_level_cost(skill_type sk)
{
    if (you.skills[sk] >= MAX_SKILL_LEVEL)
        return 0;
    return skill_exp_needed(you.skills[sk] + 1, sk)
           - skill_exp_needed(you.skills[sk], sk);
}

/**
 * The number displayed in the 'cost' interface on the m screen.
 *
 * @param sk the skill to compute the cost of
 * @returns the cost of raising sk from floor(level) to ceiling(level),
 *          as a multiple of skill_cost_baseline()
 */
float scaled_skill_cost(skill_type sk)
{
    if (you.skills[sk] == MAX_SKILL_LEVEL || is_useless_skill(sk))
        return 0;
    int baseline = skill_cost_baseline();
    int next_level = one_level_cost(sk);
    if (skill_has_manual(sk))
        baseline *= 2;

    return (float)next_level / baseline;
}

// Characters are actually granted skill points, not skill levels.
// Here we take racial aptitudes into account in determining final
// skill levels.
void reassess_starting_skills()
{
    // go backwards, need to do Dodging before Armour
    // "sk >= SK_FIRST_SKILL" might be optimised away, so do this differently.
    for (skill_type next = NUM_SKILLS; next > SK_FIRST_SKILL; )
    {
        skill_type sk = --next;
        ASSERT(you.skills[sk] == 0 || !is_useless_skill(sk));

        // Grant the amount of skill points required for a human.
        you.skill_points[sk] = you.skills[sk] ?
            skill_exp_needed(you.skills[sk], sk, SP_HUMAN) + 1 : 0;

        if (sk == SK_DODGING && you.skills[SK_ARMOUR]
            && (is_useless_skill(SK_ARMOUR)
                || you_can_wear(EQ_BODY_ARMOUR) != MB_TRUE))
        {
            // No one who can't wear mundane heavy armour should start with
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

    you.skills[exsk] = max(0, you.skills[exsk] + n);

    take_note(Note(n > 0 ? NOTE_GAIN_SKILL : NOTE_LOSE_SKILL,
                   exsk, you.skills[exsk]));

    // are you drained/crosstrained/ash'd in the relevant skill?
    const bool specify_base = you.skill(exsk, 1) != you.skill(exsk, 1, true);
    if (you.skills[exsk] == MAX_SKILL_LEVEL)
        mprf(MSGCH_INTRINSIC_GAIN, "You have mastered %s!", skill_name(exsk));
    else if (abs(n) == 1 && you.num_turns)
    {
        mprf(MSGCH_INTRINSIC_GAIN, "Your %s%s skill %s to level %d!",
             specify_base ? "base " : "",
             skill_name(exsk), (n > 0) ? "increases" : "decreases",
             you.skills[exsk]);
    }
    else if (you.num_turns)
    {
        mprf(MSGCH_INTRINSIC_GAIN, "Your %s%s skill %s %d levels and is now "
             "at level %d!",
             specify_base ? "base " : "",
             skill_name(exsk),
             (n > 0) ? "gained" : "lost",
             abs(n), you.skills[exsk]);
    }

    if (you.skills[exsk] == n && n > 0)
        hints_gained_new_skill(exsk);

    if (n > 0 && you.num_turns)
        learned_something_new(HINT_SKILL_RAISE);

    if (you.skills[exsk] - n == MAX_SKILL_LEVEL)
    {
        you.train[exsk] = TRAINING_ENABLED;
        need_reset = true;
    }

    if (exsk == SK_SPELLCASTING && you.skills[exsk] == n && n > 0)
        learned_something_new(HINT_GAINED_SPELLCASTING);

    if (need_reset)
        reset_training();

    // calc_hp() has to be called here because it currently doesn't work
    // right if you.skills[] hasn't been updated yet.
    if (exsk == SK_FIGHTING)
        recalc_and_scale_hp();
}

// Called whenever a skill is trained.
void redraw_skill(skill_type exsk, skill_type old_best_skill)
{
    if (exsk == SK_FIGHTING)
        recalc_and_scale_hp();

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
    // at its new level.   See skills.cc::init_skill_order()
    // for more details.  -- bwr
    you.skill_order[exsk] = 0;
    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
    {
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

int calc_skill_level_change(skill_type sk, int starting_level, int sk_points)
{
    int new_level = starting_level;
    while (1)
    {
        if (new_level < MAX_SKILL_LEVEL
            && sk_points >= (int) skill_exp_needed(new_level + 1, sk))
        {
            ++new_level;
        }
        else if (sk_points < (int) skill_exp_needed(new_level, sk))
        {
            new_level--;
            ASSERT(new_level >= 0);
        }
        else
            break;
    }
    return new_level;
}

void check_skill_level_change(skill_type sk, bool do_level_up)
{
    const int new_level = calc_skill_level_change(sk, you.skills[sk], you.skill_points[sk]);

    if (new_level != you.skills[sk])
    {
        if (do_level_up)
            _change_skill_level(sk, new_level - you.skills[sk]);
        else
            you.skills[sk] = new_level;
    }
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

static void _erase_from_stop_train(const skill_set &can_train)
{
    for (skill_type sk : can_train)
        you.stop_train.erase(sk);
}

/*
 * Check the inventory to see what skills the player can train,
 * among the ones in you.stop_train.
 * Trainable skills are removed from the set.
 */
static void _check_inventory_skills()
{
    for (const auto &item : you.inv)
    {
        // Exit early if there's no more skill to check.
        if (you.stop_train.empty())
            return;

        skill_set skills;
        if (!item.defined() || !item_skills(item, skills))
            continue;

        _erase_from_stop_train(skills);
    }
}

static void _check_spell_skills()
{
    for (spell_type spell : you.spells)
    {
        // Exit early if there's no more skill to check.
        if (you.stop_train.empty())
            return;

        if (spell == SPELL_NO_SPELL)
            continue;

        skill_set skills;
        spell_skills(spell, skills);
        _erase_from_stop_train(skills);
    }
}

static void _check_abil_skills()
{
    for (ability_type abil : get_god_abilities())
    {
        // Exit early if there's no more skill to check.
        if (you.stop_train.empty())
            return;

        you.stop_train.erase(abil_skill(abil));
    }
}

string skill_names(const skill_set &skills)
{
    return comma_separated_fn(begin(skills), end(skills), skill_name);
}

static void _check_start_train()
{
    skill_set skills;
    for (skill_type sk : you.start_train)
    {
        if (is_invalid_skill(sk) || is_useless_skill(sk))
            continue;

        if (!you.can_train[sk] && you.train[sk])
            skills.insert(sk);
        you.can_train.set(sk);
    }

    reset_training();

    // We're careful of not invalidating the iterator when erasing.
    for (auto it = skills.begin(); it != skills.end();)
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
    // Gnolls can't stop training skills.
    if (you.species == SP_GNOLL)
        return;

    _check_inventory_skills();
    _check_spell_skills();
    _check_abil_skills();

    if (you.stop_train.empty())
        return;

    skill_set skills;
    for (skill_type sk : you.stop_train)
    {
        if (is_invalid_skill(sk))
            continue;
        if (skill_has_manual(sk))
            continue;

        if (skill_trained(sk) && you.training[sk])
            skills.insert(sk);
        you.can_train.set(sk, false);
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
    if (you.species == SP_GNOLL)
        return false;

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
    const bool is_gnoll = you.species == SP_GNOLL;

    for (int i = 0; i < NUM_SKILLS; ++i)
        if (you.can_train[i] && you.skill_points[i])
            you.train[i] = you.train_alt[i] = TRAINING_ENABLED;
        else
        {
            // Skills are on by default in auto mode and off in manual.
            you.train[i] = (training_status) (is_gnoll || you.auto_training);
            you.train_alt[i] =
                (training_status) (is_gnoll || !you.auto_training);
        }
}

static bool _cmp_rest(const pair<skill_type, int64_t>& a,
                      const pair<skill_type, int64_t>& b)
{
    return a.second < b.second;
}

/**
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
                rests.emplace_back(skill_type(i), rest);
            array[i] = (int)(result / total);
            scaled_total += array[i];
        }

    ASSERT(scaled_total <= scale);

    if (!exact || scaled_total == scale)
        return;

    // We ensure that the percentage always add up to 100 by increasing the
    // training for skills which had the higher rest from the above scaling.
    sort(rests.begin(), rests.end(), _cmp_rest);
    for (auto &rest : rests)
    {
        if (scaled_total >= scale)
            break;

        ++array[rest.first];
        ++scaled_total;
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
            || you.skill_points[sk] >= skill_exp_needed(MAX_SKILL_LEVEL, sk))
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

/**
 * Reset the training array. Disabled skills are skipped.
 * In automatic mode, we use values from the exercise queue.
 * In manual mode, all enabled skills are set to the same value.
 * Result is scaled back to 100.
 */
void reset_training()
{
    // Disable this here since we don't want any autotraining related skilling
    // changes for Gnolls.
    if (you.species == SP_GNOLL)
        you.auto_training = false;

    // We clear the values in the training array. In auto mode they are set
    // to 0 (and filled later with the content of the queue), in manual mode,
    // the trainable ones are set to 1 (or 2 for focus).
    for (int i = 0; i < NUM_SKILLS; ++i)
        // Gnolls always train all non-useless skills, even if they wouldn't
        // normally be trainable.
        if (is_useless_skill((skill_type) i))
            you.training[i] = 0;
        else if (you.species == SP_GNOLL)
            you.training[i] = 1;
        else if (you.auto_training || !skill_trained(i))
            you.training[i] = 0;
        else
            you.training[i] = you.train[i];

    bool empty = true;
    // In automatic mode, we fill the array with the content of the queue.
    if (you.auto_training)
    {
        for (auto sk : you.exercises)
            if (skill_trained(sk))
            {
                you.training[sk] += you.train[sk];
                empty = false;
            }

        // We count the practise events in the other queue.
        FixedVector<unsigned int, NUM_SKILLS> exer_all;
        exer_all.init(0);
        for (auto sk : you.exercises_all)
            if (skill_trained(sk))
            {
                exer_all[sk] += you.train[sk];
                empty = false;
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
    if (you.skills[exsk] >= MAX_SKILL_LEVEL)
        return;

    dprf(DIAG_SKILLS, "Exercise %s by %d.", skill_name(exsk), deg);

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
    if (you.skill_points[sk] >= skill_exp_needed(MAX_SKILL_LEVEL, sk))
    {
        you.training[sk] = 0;
        if (!simu)
        {
            you.train[sk] = TRAINING_DISABLED;
            you.train_alt[sk] = TRAINING_DISABLED;
        }
        return true;
    }

    return false;
}

bool is_magic_skill(skill_type sk)
{
    return sk > SK_LAST_MUNDANE && sk <= SK_LAST_MAGIC;
}

int _gnoll_total_skill_cost();

void train_skills(bool simu)
{
    int cost, exp;
    if (you.species == SP_GNOLL)
    {
        do
        {
            exp = you.exp_available;
            cost = _gnoll_total_skill_cost();
            if (exp >= cost)
                _train_skills(exp, calc_skill_cost(you.skill_cost_level), simu);
        }
        while (exp != you.exp_available);
    }
    else
    {
        do
        {
            cost = calc_skill_cost(you.skill_cost_level);
            exp = you.exp_available;
            if (you.skill_cost_level == MAX_SKILL_COST_LEVEL)
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
    }

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
    dprf(DIAG_SKILLS,
         "skill cost level: %d, cost: %dxp/10skp, max XP usable: %d.",
         you.skill_cost_level, cost, exp);
#endif

    // pre-check training targets -- may disable some skills.
    if (!simu)
        check_training_targets();

    // We scale the training array to the amount of XP available in the pool.
    // That gives us the amount of XP available to train each skill.
    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        if (you.training[i] > 0)
        {
            sk_exp[i] = you.training[i] * exp / 100;
            if (sk_exp[i] < cost && you.species != SP_GNOLL)
            {
                // One skill has a too low training to be trained at all.
                // We skip the first phase and go directly to the random
                // phase so it has a chance to be trained.
                skip_first_phase = true;
                break;
            }
            training_order.push_back(static_cast<skill_type>(i));
        }
    }

    if (!skip_first_phase)
    {
        // We randomize the order, to avoid a slight bias to first skills.
        // Being trained first can make a difference if skill cost increases.
        shuffle_array(training_order);
        for (auto sk : training_order)
        {
            int gain = 0;

            if (you.species == SP_GNOLL)
                sk_exp[sk] = exp;
            while (sk_exp[sk] >= cost && you.training[sk])
            {
                exp -= sk_exp[sk];
                gain += _train(sk, sk_exp[sk], simu);
                exp += sk_exp[sk];
                ASSERT(exp >= 0);
                if (_level_up_check(sk, simu))
                    sk_exp[sk] = 0;
                if (you.species == SP_GNOLL)
                    break;
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
        if (you.species == SP_GNOLL)
            break;
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

    // clean up any cross-training effects
    if (!simu)
        check_training_targets();

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
            {
                dprf(DIAG_SKILLS, "Trained %s by %d.",
                     skill_name(sk), total_gain[sk]);
            }
#ifdef DEBUG_TRAINING_COST
            total += total_gain[sk];
        }
        dprf(DIAG_SKILLS, "Total skill points gained: %d, cost: %d XP.",
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
        did_god_conduct(DID_SPELL_PRACTISE, div_rand_round(magic_gain, 10));
}

bool skill_trained(int i)
{
    return you.can_train[i] && you.train[i];
}

/**
 * Is the training target, if any, met or exceeded for skill sk?
 *
 * @param sk the skill to check. This checks crosstraining and ash bonuses,
 * but not other skill modifiers.
 *
 * @return whether the skill target has been met.
 */
bool target_met(skill_type sk)
{
    return you.skill(sk, 10, false, false, false) >=
                                        (int) you.training_targets[sk];
}

/**
 * Check the training target (if any) for skill sk, and change state
 * appropriately. If the target has been met or exceeded, this will turn off
 * targeting for that skill, and stop training it. This does *not* reset the
 * training percentages, though, so if it's used mid-training, you need to take
 * care of that.
 *
 * @param sk the skill to check.
 * @return whether a target was reached.
 */
bool check_training_target(skill_type sk)
{
    if (you.training_targets[sk] && target_met(sk))
    {
        bool base = (you.skill(sk, 10, false, false, false) !=
                        you.skill(sk, 10, false, true, true));
        mprf("%sraining target %d.%d for %s reached!",
            base ? "Base t" : "T",
            you.training_targets[sk] / 10,
            you.training_targets[sk] % 10, skill_name(sk));

        you.training_targets[sk] = 0;
        you.train[sk] = TRAINING_DISABLED;
        you.train_alt[sk] = TRAINING_DISABLED;
        return true;
    }
    return false;
}

/**
 * Check the training target (if any) for all skills, and change state
 * appropriately.
 *
 * @return whether any target was reached.
 *
 * @see check_training_target
 */
bool check_training_targets()
{
    bool change = false;
    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
        change |= check_training_target(sk);
    if (change)
        reset_training();
    return change;
}

static int _calc_skill_cost_level(int xp, int start)
{
    while (start < MAX_SKILL_COST_LEVEL
           && xp >= (int) skill_cost_needed(start + 1))
    {
        ++start;
    }
    while (start > 0
           && xp < (int) skill_cost_needed(start))
    {
        --start;
    }
    return start;
}

void check_skill_cost_change()
{
#ifdef DEBUG_TRAINING_COST
    int initial_cost = you.skill_cost_level;
#endif

    you.skill_cost_level = _calc_skill_cost_level(you.total_experience, you.skill_cost_level);

#ifdef DEBUG_TRAINING_COST
    if (initial_cost != you.skill_cost_level)
        dprf("Adjusting skill cost level to %d", you.skill_cost_level);
#endif
}

static int _useless_skill_count()
{
    int count = 0;
    for (skill_type skill = SK_FIRST_SKILL; skill < NUM_SKILLS; ++skill)
    {
#if TAG_MAJOR_VERSION == 34
        if (skill == SK_STABBING || skill == SK_TRAPS)
            continue;
#endif
        if (is_useless_skill(skill))
            count++;
    }
    return count;
}

static int _total_skill_count()
{
    int count = 0;
    for (skill_type skill = SK_FIRST_SKILL; skill < NUM_SKILLS; ++skill)
    {
#if TAG_MAJOR_VERSION == 34
        if (skill == SK_STABBING || skill == SK_TRAPS)
            continue;
#endif
        count++;
    }
    return count;
}

// The current cost of raising each skill by one skill point, taking the
// gnoll penalty for useless skills into account and rounding up for all
// computations. Used to ensure that gnoll skills rise evenly - we don't
// train anything unless we have this much xp to spend.
int _gnoll_total_skill_cost()
{
    int this_cost;
    int total_cost = 0;
    int cur_cost_level = you.skill_cost_level;
    const int useless_count = _useless_skill_count();
    const int total_count = _total_skill_count();
    const int num = total_count;
    const int denom = 10 * (total_count - useless_count);
    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        if (!you.training[i])
            continue;
        cur_cost_level = _calc_skill_cost_level(you.total_experience + total_cost, cur_cost_level);
        this_cost = calc_skill_cost(cur_cost_level);
        this_cost = (num * this_cost + denom - 1) / denom;
        total_cost += this_cost;
    }
    return total_cost;
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

    if (you.species == SP_GNOLL)
    {
        skill_inc = 1;
        int useless_count = _useless_skill_count();
        int total_count = _total_skill_count();
        int num = total_count;
        int denom = 10 * (total_count - useless_count);
        cost = div_rand_round(num * cost, denom);
    }
    else
    {
        // Scale cost and skill_inc to available experience.
        const int spending_limit = min(MAX_SPENDING_LIMIT, max_exp);
        if (cost > spending_limit)
        {
            int frac = spending_limit * 10 / cost;
            cost = spending_limit;
            skill_inc = skill_inc * frac / 10;
        }
    }

    if (skill_inc <= 0 || cost > max_exp)
        return 0;

    // Bonus from manual
    int slot;
    int bonus_left = skill_inc;
    while (bonus_left > 0 && (slot = manual_slot_for_skill(exsk)) != -1)
    {
        item_def& manual(you.inv[slot]);
        const int bonus = min<int>(bonus_left, manual.skill_points);
        skill_inc += bonus;
        bonus_left -= bonus;
        manual.skill_points -= bonus;
        if (!manual.skill_points && !simu)
            finish_manual(slot);
    }

    const skill_type old_best_skill = best_skill(SK_FIRST_SKILL, SK_LAST_SKILL);
    you.skill_points[exsk] += skill_inc;
    you.exp_available -= cost;
    you.total_experience += cost;
    max_exp -= cost;

    if (!simu)
    {
        // TODO should check_training_targets be called here, to halt training
        // and clean up cross-training immediately?
        check_training_target(exsk);
        redraw_skill(exsk, old_best_skill);
    }

    check_skill_cost_change();
    ASSERT(you.exp_available >= 0);
    ASSERT(max_exp >= 0);
    you.redraw_experience = true;

    return skill_inc;
}

/**
 * Calculate the difference in skill points and xp for new skill level `amount`.
 *
 * If `base_only` is false, this will produce an estimate only. Otherwise, it is
 * exact. This function is used in both estimating skill targets, and in
 * actually changing skills. It will work both for cases where the new skill
 * level is greater, and where it is less, than the current level.
 *
 * @param skill the skill to calculate for.
 * @param amount the new skill level for `skill`.
 * @param scaled_training how to scale the training values (for estimating what
 *          happens when other skills are being trained).
 * @param base_only whether to calculate on actual unmodified skill levels, or
 *          use various bonuses that apply on top of these (crosstraining, ash,
 *          manuals).
 *
 * @return a pair consisting of the difference in skill points, and the
 *          difference in xp.
 */
skill_diff skill_level_to_diffs(skill_type skill, double amount,
                            int scaled_training,
                            bool base_only)
{
    // TODO: should this use skill_state?
    // TODO: can `amount` be converted to fixed point?
    double level;
    double fractional = modf(amount, &level);
    if (level >= MAX_SKILL_LEVEL)
    {
        level = MAX_SKILL_LEVEL;
        fractional = 0;
    }

    unsigned int target = skill_exp_needed(level, skill);
    if (fractional)
    {
        target += (skill_exp_needed(level + 1, skill)
                  - skill_exp_needed(level, skill)) * fractional + 1;
    }

    // We're calculating you.skill_points[skill] and calculating the new
    // you.total_experience to update skill cost.

    unsigned int you_skill = you.skill_points[skill];

    if (!base_only)
    {
        // Factor in crosstraining bonus at the time of the query.
        // This will not address the case where some cross-training skills are
        // also being trained.
        you_skill += get_crosstrain_points(skill);

        // Estimate the ash bonus, based on current skill levels and piety.
        // This isn't perfectly accurate, because the boost changes as
        // skill increases. TODO: exact solution.
        // It also assumes that piety won't change.
        if (ash_has_skill_boost(skill))
            you_skill += ash_skill_point_boost(skill, you.skills[skill] * 10);

        if (skill_has_manual(skill))
            target = you_skill + (target - you_skill) / 2;
    }

    if (target == you_skill)
        return skill_diff();

    // Do we need to increase or decrease skill points/xp?
    // XXX: reducing with ash bonuses in play could lead to weird results.
    const bool decrease_skill = target < you_skill;

    int you_xp = you.total_experience;
    int you_skill_cost_level = you.skill_cost_level;

#ifdef DEBUG_TRAINING_COST
    dprf(DIAG_SKILLS, "target skill points: %d.", target);
#endif
    while (you_skill != target)
    {
        // each loop is the max skill points that can be gained at the
        // current skill cost level, up to `target`.

        // If we are decreasing, find the xp needed to get to the current skill
        // cost level. Otherwise, find the xp needed to get to the next one.
        const int next_level = skill_cost_needed(you_skill_cost_level +
                                                    (decrease_skill ? 0 : 1));

        // max xp that can be added (or subtracted) in one pass of the loop
        int max_xp = abs(next_level - you_xp);

        // When reducing, we don't want to stop right at the limit, unless
        // we're at skill cost level 0.
        if (decrease_skill && you_skill_cost_level)
            ++max_xp;

        const int cost = calc_skill_cost(you_skill_cost_level);
        // Maximum number of skill points to transfer in one go.
        // It's max_xp*10/cost rounded up.
        const int max_skp = max((max_xp * 10 + cost - 1) / cost, 1);

        skill_diff delta;
        delta.skill_points = min<int>(abs((int)(target - you_skill)),
                                 max_skp);
        delta.experience = (delta.skill_points * cost + 9) / 10;

        if (decrease_skill)
        {
            // We are decreasing skill points / xp to reach the target. Ensure
            // that the delta is negative but won't result in negative skp or xp
            delta.skill_points = -min<int>(delta.skill_points, you_skill);
            delta.experience = -min<int>(delta.experience, you_xp);
        }

#ifdef DEBUG_TRAINING_COST
        dprf(DIAG_SKILLS, "cost level: %d, total experience: %d, "
             "next level: %d, skill points: %d, delta_skp: %d, delta_xp: %d.",
             you_skill_cost_level, you_xp, next_level,
             you_skill, delta.skill_points, delta.experience);
#endif
        you_skill += (delta.skill_points * scaled_training
                                        + (decrease_skill ? -99 : 99)) / 100;
        you_xp += delta.experience;
        you_skill_cost_level = _calc_skill_cost_level(you_xp, you_skill_cost_level);
    }

    return skill_diff(you_skill - you.skill_points[skill],
                                you_xp - you.total_experience);
}

void set_skill_level(skill_type skill, double amount)
{
    double level;
    modf(amount, &level);

    you.ct_skill_points[skill] = 0;

    skill_diff diffs = skill_level_to_diffs(skill, amount);

    you.skills[skill] = level;
    you.skill_points[skill] += diffs.skill_points;
    you.total_experience += diffs.experience;
#ifdef DEBUG_TRAINING_COST
    dprf("Change (total): %d skp (%d), %d xp (%d)",
        diffs.skill_points, you.skill_points[skill],
        diffs.experience, you.total_experience);
#endif

    check_skill_cost_change();

    // need to check them all, to handle crosstraining.
    check_training_targets();
}

int get_skill_progress(skill_type sk, int level, int points, int scale)
{
    if (level >= MAX_SKILL_LEVEL)
        return 0;

    const int needed = skill_exp_needed(level + 1, sk);
    const int prev_needed = skill_exp_needed(level, sk);
    if (needed == 0) // invalid race, legitimate at startup
        return 0;
    // A scale as small as 92 would overflow with 31 bits if skill_rdiv()
    // is involved: needed can be 91985, skill_rdiv() multiplies by 256.
    const int64_t amt_done = points - prev_needed;
    int prog = amt_done * scale / (needed - prev_needed);

    ASSERT(prog >= 0);

    return prog;
}

int get_skill_progress(skill_type sk, int scale)
{
    return get_skill_progress(sk, you.skills[sk], you.skill_points[sk], scale);
}

int get_skill_percentage(const skill_type x)
{
    return get_skill_progress(x, 100);
}

/**
 * Get the training target for a skill.
 *
 * @param sk the skill to set
 * @return the current target, scaled by 10 -- so between 0 and 270.
 *         0 means no target.
 */
int player::get_training_target(const skill_type sk) const
{
    ASSERT_RANGE(training_targets[sk], 0, 271);
    return training_targets[sk];
}

/**
 * Set the training target for a skill.
 *
 * @param sk the skill to set
 * @param target the new target, between 0.0 and 27.0.  0.0 means no target.
 */
bool player::set_training_target(const skill_type sk, const double target, bool announce)
{
    return set_training_target(sk, (int) round(target * 10), announce);
}

void player::clear_training_targets()
{
    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
        set_training_target(sk, 0);
}

/**
 * Set the training target for a skill, scaled by 10.
 *
 * @param sk the skill to set
 * @param target the new target, scaled by ten, so between 0 and 270.  0 means
 *               no target.
 *
 * @return whether setting the target succeeded.
 */
bool player::set_training_target(const skill_type sk, const int target, bool announce)
{
    const int ranged_target = min(max((int) target, 0), 270);
    if (announce && ranged_target != (int) training_targets[sk])
    {
        if (you.species == SP_GNOLL)
            mprf("Gnolls can't set training targets!");
        else if (ranged_target == 0)
            mprf("Clearing the skill training target for %s.", skill_name(sk));
        else
        {
            mprf("Setting a skill training target for %s at %d.%d.", skill_name(sk),
                                    ranged_target / 10, ranged_target % 10);
        }
    }
    if (!can_enable_skill(sk)) // checks for gnolls
    {
        training_targets[sk] = 0;
        return false;
    }
    training_targets[sk] = ranged_target;
    return true;
}

const char *skill_name(skill_type which_skill)
{
    return skill_titles[which_skill][0];
}

/**
 * Get a skill_type from an (exact, case-insensitive) skill name.
 *
 * @return a valid skill_type, or SK_NONE on failure.
 *
 * @see skill_from_name for a non-exact version.
 */
skill_type str_to_skill(const string &skill)
{
    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
        if (lowercase_string(skill) == lowercase_string(skill_titles[sk][0]))
            return sk;

    return SK_NONE;
}

/**
 * Get a skill_type from a skill name.
 *
 * @return a valid skill_type, or SK_FIGHTING on failure.
 */
skill_type str_to_skill_safe(const string &skill)
{
    // legacy behavior -- ensure that a valid skill is returned.
    skill_type sk = str_to_skill(skill);
    if (sk == SK_NONE)
        return SK_FIGHTING;
    else
        return sk;
}

static string _stk_weight(species_type species)
{
    if (species_size(species) == SIZE_LARGE)
        return "Heavy";
    else if (species_size(species, PSIZE_BODY) == SIZE_LARGE)
        return "Cruiser";
    else if (species_size(species) == SIZE_SMALL || species == SP_TENGU)
        return "Feather";
    else if (species_size(species) == SIZE_LITTLE)
        return "Fly";
    else if (species_is_elven(species))
        return "Light";
    else
        return "Middle";
}

unsigned get_skill_rank(unsigned skill_lev)
{
    // Translate skill level into skill ranking {dlb}:
    return (skill_lev <= 7)  ? 0 :
                           (skill_lev <= 14) ? 1 :
                           (skill_lev <= 20) ? 2 :
                           (skill_lev <= 26) ? 3
                           /* level 27 */    : 4;
}

/**
 * What title will the player get at the given rank of the given skill?
 *
 * @param best_skill    The skill used to determine the title.
 * @param skill_rank    The player's rank in the given skill.
 * @param species       The player's species.
 * @param dex_better    Whether the player's dexterity is higher than strength.
 * @param god           The god_type of the god the player follows.
 * @param piety         The player's piety with the given god.
 * @return              An appropriate and/or humorous title.
 */
string skill_title_by_rank(skill_type best_skill, uint8_t skill_rank,
                           species_type species, bool dex_better,
                           god_type god, int piety)
{

    // paranoia
    if (is_invalid_skill(best_skill))
        return "Adventurer";

    // Increment rank by one to "skip" skill name in array {dlb}:
    ++skill_rank;

    string result;

    if (best_skill < NUM_SKILLS)
    {
        switch (best_skill)
        {
        case SK_SUMMONINGS:
            // don't call good disciples hellbinders or demonologists
            if (is_good_god(god))
            {
                if (skill_rank == 4)
                    result = "Worldbinder";
                else if (skill_rank == 5)
                    result = "Planerender";
            }
            break;

        case SK_UNARMED_COMBAT:
            if (species == SP_FELID)
            {
                result = claw_and_tooth_titles[skill_rank];
                break;
            }
            result = dex_better ? martial_arts_titles[skill_rank]
                                : skill_titles[best_skill][skill_rank];

            break;

        case SK_SHORT_BLADES:
            if (species_is_elven(species) && skill_rank == 5)
            {
                result = "Blademaster";
                break;
            }
            break;

        case SK_INVOCATIONS:
            if (species == SP_DEMONSPAWN
                && skill_rank == 5
                && is_evil_god(god))
            {
                result = "Blood Saint";
                break;
            }
            else if (god != GOD_NO_GOD)
                result = god_title(god, species, piety);
            break;

        case SK_BOWS:
            if (species_is_elven(species) && skill_rank == 5)
            {
                result = "Master Archer";
                break;
            }
            break;

        case SK_SPELLCASTING:
            if (species == SP_OGRE)
                result = "Ogre Mage";
            break;

        case SK_NECROMANCY:
            if (species == SP_SPRIGGAN && skill_rank == 5)
                result = "Petite Mort";
            else if (god == GOD_KIKUBAAQUDGHA)
                result = god_title(god, species, piety);
            break;

        case SK_EVOCATIONS:
            if (god == GOD_PAKELLAS)
                result = god_title(god, species, piety);
            break;

        default:
            break;
        }
        if (result.empty())
            result = skill_titles[best_skill][skill_rank];
    }

    const map<string, string> replacements =
    {
        { "Adj", species_name(species, SPNAME_ADJ) },
        { "Genus", species_name(species, SPNAME_GENUS) },
        { "genus", lowercase_string(species_name(species, SPNAME_GENUS)) },
        { "Genus_Short", species == SP_DEMIGOD ? "God" :
                           species_name(species, SPNAME_GENUS) },
        { "Walker", species_walking_verb(species) + "er" },
        { "Weight", _stk_weight(species) },
    };

    return replace_keys(result, replacements);
}

/** What is the player's current title.
 *
 *  @param the whether to prepend a definite article.
 *  @returns the title.
 */
string player_title(bool the)
{
    const skill_type best = best_skill(SK_FIRST_SKILL, SK_LAST_SKILL);
    const string title =
            skill_title_by_rank(best, get_skill_rank(you.skills[best]));
    const string article = !the ? "" : title == "Petite Mort" ? "La " : "the ";
    return article + title;
}

skill_type best_skill(skill_type min_skill, skill_type max_skill,
                      skill_type excl_skill)
{
    ASSERT(min_skill < NUM_SKILLS);
    ASSERT(max_skill < NUM_SKILLS);
    skill_type ret = SK_FIGHTING;
    unsigned int best_skill_level = 0;
    unsigned int best_position = 1000;

    for (int i = min_skill; i <= max_skill; i++)
    {
        skill_type sk = static_cast<skill_type>(i);
        if (sk == excl_skill)
            continue;

        const unsigned int skill_level = you.skill(sk, 10, true);
        if (skill_level > best_skill_level)
        {
            ret = sk;
            best_skill_level = skill_level;
            best_position = you.skill_order[sk];

        }
        else if (skill_level == best_skill_level
                 && you.skill_order[sk] < best_position)
        {
            ret = sk;
            best_position = you.skill_order[sk];
        }
    }

    return ret;
}

// Calculate the skill_order array from scratch.
//
// The skill order array is used for breaking ties in best_skill.
// This is done by ranking each skill by the order in which it
// has attained its current level (the values are the number of
// skills at or above that level when the current skill reached it).
//
// In this way, the skill which has been at a level for the longest
// is judged to be the best skill (thus, nicknames are sticky)...
// other skills will have to attain the next level higher to be
// considered a better skill (thus, the first skill to reach level 27
// becomes the characters final nickname). -- bwr
void init_skill_order()
{
    for (skill_type si = SK_FIRST_SKILL; si < NUM_SKILLS; ++si)
    {
        const unsigned int i_points = you.skill_points[si]
                                      / species_apt_factor(si);

        you.skill_order[si] = 0;

        for (skill_type sj = SK_FIRST_SKILL; sj < NUM_SKILLS; ++sj)
        {
            if (si == sj)
                continue;

            const unsigned int j_points = you.skill_points[sj]
                                          / species_apt_factor(sj);

            if (you.skills[sj] == you.skills[si]
                && (j_points > i_points
                    || (j_points == i_points && sj > si)))
            {
                you.skill_order[si]++;
            }
        }
    }
}

bool is_useless_skill(skill_type skill)
{
#if TAG_MAJOR_VERSION == 34
    if (skill == SK_STABBING || skill == SK_TRAPS)
        return true;
#endif

    if ((skill == SK_AIR_MAGIC && you.get_mutation_level(MUT_NO_AIR_MAGIC))
        || (skill == SK_CHARMS && you.get_mutation_level(MUT_NO_CHARM_MAGIC))
        || (skill == SK_CONJURATIONS
            && you.get_mutation_level(MUT_NO_CONJURATION_MAGIC))
        || (skill == SK_EARTH_MAGIC
            && you.get_mutation_level(MUT_NO_EARTH_MAGIC))
        || (skill == SK_FIRE_MAGIC && you.get_mutation_level(MUT_NO_FIRE_MAGIC))
        || (skill == SK_HEXES && you.get_mutation_level(MUT_NO_HEXES_MAGIC))
        || (skill == SK_ICE_MAGIC && you.get_mutation_level(MUT_NO_ICE_MAGIC))
        || (skill == SK_NECROMANCY
            && you.get_mutation_level(MUT_NO_NECROMANCY_MAGIC))
        || (skill == SK_POISON_MAGIC
            && you.get_mutation_level(MUT_NO_POISON_MAGIC))
        || (skill == SK_SUMMONINGS
            && you.get_mutation_level(MUT_NO_SUMMONING_MAGIC))
        || (skill == SK_TRANSLOCATIONS
            && you.get_mutation_level(MUT_NO_TRANSLOCATION_MAGIC))
        || (skill == SK_TRANSMUTATIONS
            && you.get_mutation_level(MUT_NO_TRANSMUTATION_MAGIC))
        || (skill == SK_DODGING && you.get_mutation_level(MUT_NO_DODGING))
        || (skill == SK_ARMOUR && you.get_mutation_level(MUT_NO_ARMOUR))
        || (skill == SK_SHIELDS && you.get_mutation_level(MUT_MISSING_HAND))
        || (skill == SK_EVOCATIONS && you.get_mutation_level(MUT_NO_ARTIFICE))
        || (skill == SK_STEALTH && you.get_mutation_level(MUT_NO_STEALTH))
    )
    {
        return true;
    }

    return species_apt(skill) == UNUSABLE_SKILL;
}

bool is_harmful_skill(skill_type skill)
{
    return is_magic_skill(skill) && you_worship(GOD_TROG);
}

/**
 * Has the player maxed out all skills?
 *
 * @param really_all If true, also consider skills that are harmful and/or
 *        currently untrainable. Useless skills are never considered.
 */
bool all_skills_maxed(bool really_all)
{
    for (skill_type i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
    {
        if (you.skills[i] < MAX_SKILL_LEVEL && !is_useless_skill(i)
            && (really_all || you.can_train[i] && !is_harmful_skill(i)))
        {
            return false;
        }
    }

    return true;
}

int skill_bump(skill_type skill, int scale)
{
    const int sk = you.skill_rdiv(skill, scale);
    return sk + min(sk, 3 * scale);
}

// What aptitude value corresponds to doubled skill learning
// (i.e., old-style aptitude 50).
#define APT_DOUBLE 4

float apt_to_factor(int apt)
{
    return 1 / exp(log(2) * apt / APT_DOUBLE);
}

unsigned int skill_exp_needed(int lev, skill_type sk, species_type sp)
{
    const int exp[28] =
          { 0, 50, 150, 300, 500, 750,          // 0-5
            1050, 1400, 1800, 2250, 2800,       // 6-10
            3450, 4200, 5050, 6000, 7050,       // 11-15
            8200, 9450, 10800, 12300, 13950,    // 16-20
            15750, 17700, 19800, 22050, 24450,  // 21-25
            27000, 29750 };

    ASSERT_RANGE(lev, 0, MAX_SKILL_LEVEL + 1);
    return exp[lev] * species_apt_factor(sk, sp);
}

int species_apt(skill_type skill, species_type species)
{
    static bool spec_skills_initialised = false;
    if (!spec_skills_initialised)
    {
        // Setup sentinel values to find errors more easily.
        const int sentinel = -20; // this gives cost 3200
        for (int sp = 0; sp < NUM_SPECIES; ++sp)
            for (int sk = 0; sk < NUM_SKILLS; ++sk)
                _spec_skills[sp][sk] = sentinel;
        for (const species_skill_aptitude &ssa : species_skill_aptitudes)
        {
            ASSERT(_spec_skills[ssa.species][ssa.skill] == sentinel);
            _spec_skills[ssa.species][ssa.skill] = ssa.aptitude;
        }
        spec_skills_initialised = true;
    }

    return max(UNUSABLE_SKILL, _spec_skills[species][skill]
                               - you.get_mutation_level(MUT_UNSKILLED));
}

float species_apt_factor(skill_type sk, species_type sp)
{
    return apt_to_factor(species_apt(sk, sp));
}

vector<skill_type> get_crosstrain_skills(skill_type sk)
{
    // Gnolls do not have crosstraining.
    if (you.species == SP_GNOLL)
        return {};

    switch (sk)
    {
    case SK_SHORT_BLADES:
        return { SK_LONG_BLADES };
    case SK_LONG_BLADES:
        return { SK_SHORT_BLADES };
    case SK_AXES:
    case SK_STAVES:
        return { SK_POLEARMS, SK_MACES_FLAILS };
    case SK_MACES_FLAILS:
    case SK_POLEARMS:
        return { SK_AXES, SK_STAVES };
    case SK_SLINGS:
        return { SK_THROWING };
    case SK_THROWING:
        return { SK_SLINGS };
    default:
        return {};
    }
}

/**
 * Calculate the current crosstraining bonus for skill `sk`, in skill points.
 */
int get_crosstrain_points(skill_type sk)
{
    int points = 0;
    for (skill_type cross : get_crosstrain_skills(sk))
        points += you.skill_points[cross] * 2 / 5;
    return points;

}

/**
 * Is the provided skill one of the elemental spellschools?
 *
 * @param sk    The skill in question.
 * @return      Whether it is fire, ice, earth, or air.
 */
static bool _skill_is_elemental(skill_type sk)
{
    return sk == SK_FIRE_MAGIC || sk == SK_EARTH_MAGIC
           || sk == SK_AIR_MAGIC || sk == SK_ICE_MAGIC;
}

/**
 * How skilled is the player at the elemental components of a spell?
 *
 * @param spell     The type of spell in question.
 * @param scale     Scaling factor for skill.
 * @return          The player's skill at the elemental parts of a given spell.
 */
int elemental_preference(spell_type spell, int scale)
{
    skill_set skill_list;
    spell_skills(spell, skill_list);
    int preference = 0;
    for (skill_type sk : skill_list)
        if (_skill_is_elemental(sk))
            preference += you.skill(sk, scale);
    return preference;
}

/**
 * Compare skill levels
 *
 * It compares the level of 2 skills, and breaks ties by using skill order.
 *
 * @param sk1 First skill.
 * @param sk2 Second skill.
 * @return Whether first skill is higher than second skill.
 */
bool compare_skills(skill_type sk1, skill_type sk2)
{
    if (is_invalid_skill(sk1))
        return false;
    else if (is_invalid_skill(sk2))
        return true;
    else
        return you.skill(sk1, 10, true) > you.skill(sk2, 10, true)
               || you.skill(sk1, 10, true) == you.skill(sk2, 10, true)
                  && you.skill_order[sk1] < you.skill_order[sk2];
}

void dump_skills(string &text)
{
    for (uint8_t i = 0; i < NUM_SKILLS; i++)
    {
        int real = you.skill((skill_type)i, 10, true);
        int cur  = you.skill((skill_type)i, 10);
        if (real > 0 || (!you.auto_training && you.train[i] > 0))
        {
            text += make_stringf(" %c Level %.*f%s %s\n",
                                 real == 270       ? 'O' :
                                 !you.can_train[i] ? ' ' :
                                 you.train[i] == 2 ? '*' :
                                 you.train[i]      ? '+' :
                                                     '-',
                                 real == 270 ? 0 : 1,
                                 real * 0.1,
                                 real != cur
                                     ? make_stringf("(%.*f)",
                                           cur == 270 ? 0 : 1,
                                           cur * 0.1).c_str()
                                     : "",
                                 skill_name(static_cast<skill_type>(i)));
        }
    }
}

int skill_transfer_amount(skill_type sk)
{
    ASSERT(!is_invalid_skill(sk));
    if (you.skill_points[sk] < 1000)
        return you.skill_points[sk];
    else
        return max<int>(1000, you.skill_points[sk] / 2);
}

// Transfer skill points from one skill to another (Ashenzari transfer
// knowledge ability). If simu, it just simulates the transfer and don't
// change anything. It returns the new level of tsk.
int transfer_skill_points(skill_type fsk, skill_type tsk, int skp_max,
                          bool simu, bool boost)
{
    ASSERT(you.species != SP_GNOLL);
    ASSERT(!is_invalid_skill(fsk) && !is_invalid_skill(tsk));

    const int penalty = 90; // 10% XP penalty
    int total_skp_lost   = 0; // skill points lost in fsk.
    int total_skp_gained = 0; // skill points gained in tsk.
    int fsk_level = you.skills[fsk];
    int tsk_level = you.skills[tsk];
    int fsk_points = you.skill_points[fsk];
    int tsk_points = you.skill_points[tsk];
    int fsk_ct_points = you.ct_skill_points[fsk];
    int tsk_ct_points = you.ct_skill_points[tsk];

    if (!simu && you.ct_skill_points[fsk] > 0)
    {
        dprf(DIAG_SKILLS, "ct_skill_points[%s]: %d",
             skill_name(fsk), you.ct_skill_points[fsk]);
    }

    // We need to transfer by small steps and update skill levels each time
    // so that cross-training is handled properly.
    while (total_skp_lost < skp_max
           && (simu || total_skp_lost < (int)you.transfer_skill_points))
    {
        int skp_lost = min(20, skp_max - total_skp_lost);
        int skp_gained = skp_lost * penalty / 100;

        ASSERT(you.skill_points[fsk] > you.ct_skill_points[fsk]);

        int ct_penalty = skp_lost * you.ct_skill_points[fsk]
                          / (you.skill_points[fsk] - you.ct_skill_points[fsk]);
        ct_penalty = min<int>(ct_penalty, you.ct_skill_points[fsk]);
        you.ct_skill_points[fsk] -= ct_penalty;
        skp_lost += ct_penalty;

        if (!simu)
        {
            skp_lost = min<int>(skp_lost, you.transfer_skill_points
                                          - total_skp_lost);
        }

        total_skp_lost += skp_lost;
        change_skill_points(fsk, -skp_lost, false);

        // If reducing fighting would reduce your maxHP to 0 or below,
        // we cancel the last step and end the transfer.
        if (fsk == SK_FIGHTING && get_real_hp(false, true) <= 0)
        {
            change_skill_points(fsk, skp_lost, false);
            total_skp_lost -= skp_lost;
            if (!simu)
                you.transfer_skill_points = total_skp_lost;
            break;
        }

        total_skp_gained += skp_gained;

        if (fsk != tsk)
        {
            change_skill_points(tsk, skp_gained, false);
            if (you.skills[tsk] == MAX_SKILL_LEVEL)
                break;
        }
    }

    int new_level = you.skill(tsk, 10, !boost);
    // Restore the level
    you.skills[fsk] = fsk_level;
    you.skills[tsk] = tsk_level;

    if (simu)
    {
        you.skill_points[fsk] = fsk_points;
        you.skill_points[tsk] = tsk_points;
        you.ct_skill_points[fsk] = fsk_ct_points;
        you.ct_skill_points[tsk] = tsk_ct_points;
    }
    else
    {
        // Perform the real level up
        check_skill_level_change(fsk);
        check_skill_level_change(tsk);
        if ((int)you.transfer_skill_points < total_skp_lost)
            you.transfer_skill_points = 0;
        else
            you.transfer_skill_points -= total_skp_lost;

        dprf(DIAG_SKILLS, "skill %s lost %d points",
             skill_name(fsk), total_skp_lost);
        dprf(DIAG_SKILLS, "skill %s gained %d points",
             skill_name(tsk), total_skp_gained);
        if (you.ct_skill_points[fsk] > 0)
        {
            dprf(DIAG_SKILLS, "ct_skill_points[%s]: %d",
                 skill_name(fsk), you.ct_skill_points[fsk]);
        }

        if (you.transfer_skill_points == 0
            || you.skills[tsk] == MAX_SKILL_LEVEL)
        {
            ashenzari_end_transfer(true);
        }
        else
        {
            dprf(DIAG_SKILLS, "%d skill points left to transfer",
                 you.transfer_skill_points);
        }
    }
    return new_level;
}

void skill_state::save()
{
    can_train          = you.can_train;
    skills             = you.skills;
    train              = you.train;
    training           = you.training;
    skill_points       = you.skill_points;
    training_targets   = you.training_targets;
    ct_skill_points    = you.ct_skill_points;
    skill_cost_level   = you.skill_cost_level;
    skill_order        = you.skill_order;
    auto_training      = you.auto_training;
    exp_available      = you.exp_available;
    total_experience   = you.total_experience;
    get_all_manual_charges(manual_charges);
    for (int i = 0; i < NUM_SKILLS; i++)
    {
        real_skills[i] = you.skill((skill_type)i, 10, true);
        changed_skills[i] = you.skill((skill_type)i, 10);
    }
}

void skill_state::restore_levels()
{
    you.skills                      = skills;
    you.skill_points                = skill_points;
    you.ct_skill_points             = ct_skill_points;
    you.skill_cost_level            = skill_cost_level;
    you.skill_order                 = skill_order;
    you.exp_available               = exp_available;
    you.total_experience            = total_experience;
    set_all_manual_charges(manual_charges);
}

void skill_state::restore_training()
{
    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
    {
        if (you.skills[sk] < MAX_SKILL_LEVEL)
            you.train[sk] = train[sk];
    }

    you.can_train                   = can_train;
    you.auto_training               = auto_training;
    reset_training();
}

// Sanitize skills after an upgrade, racechange, etc.
void fixup_skills()
{
    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
    {
        if (is_useless_skill(sk))
            you.skill_points[sk] = 0;
        you.skill_points[sk] = min(you.skill_points[sk],
                                   skill_exp_needed(MAX_SKILL_LEVEL, sk));
        check_skill_level_change(sk);
    }
    init_can_train();

    if (you.exp_available >= calc_skill_cost(you.skill_cost_level))
        skill_menu(SKMF_EXPERIENCE);

    check_training_targets();
}

/** Can the player enable training for this skill?
 *
 * @param sk The skill to check.
 * @returns True if the skill can be enabled for training, false otherwise.
 */
bool can_enable_skill(skill_type sk)
{
    return you.species != SP_GNOLL && you.can_train[sk];
}
