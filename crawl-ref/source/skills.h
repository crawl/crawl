/**
 * @file
 * @brief Skill exercising functions.
**/

#pragma once

#include <vector>

#include "player.h"

using std::vector;

const int MAX_SKILL_ORDER = 100;
struct skill_state
{
    skill_state();

    FixedBitVector<NUM_SKILLS>            can_currently_train;
    FixedVector<uint8_t, NUM_SKILLS>      skills;
    FixedVector<int, NUM_SKILLS>          real_skills;    // Those two are
    FixedVector<int, NUM_SKILLS>          changed_skills; // scaled by 10.
    FixedVector<training_status, NUM_SKILLS>       train;
    FixedVector<unsigned int, NUM_SKILLS> training;
    FixedVector<unsigned int, NUM_SKILLS> skill_points;
    FixedVector<unsigned int, NUM_SKILLS> training_targets;
    FixedVector<uint8_t, NUM_SKILLS>      skill_order;
    FixedVector<unsigned int, NUM_SKILLS> skill_manual_points;
    int skill_cost_level;
    unsigned int total_experience;
    bool auto_training;
    int exp_available;

    void save();
    bool state_saved() const;
    void restore_levels();
    void restore_training();

private:
    bool saved;
};

struct skill_diff
{
    skill_diff() : skill_points(0), experience(0) { }
    skill_diff(int skp, int xp) : skill_points(skp), experience(xp) { }

    int skill_points;
    int experience;
};

typedef set<skill_type> skill_set;

string skill_names(const skill_set &skills);

int skill_cost_baseline();
int one_level_cost(skill_type sk);
float scaled_skill_cost(skill_type sk);

unsigned int skill_cost_needed(int level);
int calc_skill_cost(int skill_cost_level);
void check_skill_cost_change();

bool skill_default_shown(skill_type sk);
void reassess_starting_skills(bool balance_djinn = true);
bool check_selected_skills();
void init_train();
void init_can_currently_train();
void init_training();
void update_can_currently_train();
void reset_training();
int calc_skill_level_change(skill_type sk, int starting_level, int sk_points);
void check_skill_level_change(skill_type sk, bool do_level_up = true);
void change_skill_level(skill_type exsk, int num_level);
void change_skill_points(skill_type sk, int points, bool do_level_up);

bool is_magic_skill(skill_type sk);

void exercise(skill_type exsk, int deg);
void train_skills(bool simu = false);
bool skill_trained(int i);
static inline bool skill_trained(skill_type sk) { return skill_trained((int) sk); }
void redraw_skill(skill_type exsk, skill_type old_best_skill = SK_NONE, bool recalculate_order = true);
void set_skill_level(skill_type skill, double amount);

int get_skill_progress(skill_type sk, int level, int points, int scale);
int get_skill_progress(skill_type sk, int scale);
int get_skill_percentage(const skill_type x);
const char *skill_name(skill_type which_skill);
const char *skill_abbr(skill_type which_skill);
skill_type str_to_skill(const string &skill);
skill_type str_to_skill_safe(const string &skill);

string skill_title_by_rank(
    skill_type best_skill, uint8_t skill_rank,
    // these used for ghosts and hiscores:
    species_type species = you.species,
    bool dex_better = you.base_stats[STAT_DEX] >= you.base_stats[STAT_STR],
    god_type god = you.religion,
    int piety = you.piety);
unsigned get_skill_rank(unsigned skill_lev);

string player_title(bool the = true);

skill_type best_skill(skill_type min_skill, skill_type max_skill,
                      skill_type excl_skill = SK_NONE);
void init_skill_order();

bool is_removed_skill(skill_type skill);
bool can_sacrifice_skill(mutation_type mut);
bool is_useless_skill(skill_type skill);
bool is_harmful_skill(skill_type skill);
bool can_enable_skill(skill_type sk, bool override = false);
bool trainable_skills(bool check_all = false);
bool skills_being_trained();

int species_apt(skill_type skill, species_type species = you.species);
float species_apt_factor(skill_type sk, species_type sp = you.species);
float apt_to_factor(int apt);
unsigned int skill_exp_needed(int lev, skill_type sk,
                              species_type sp = you.species);
skill_diff skill_level_to_diffs(skill_type skill, double amount,
    int scaled_training=100, bool base_only=true);

vector<skill_type> get_crosstrain_skills(skill_type sk);
int get_crosstrain_points(skill_type sk);

int elemental_preference(spell_type spell, int scale = 1);

void skill_menu(int flag = 0, int exp = 0);
void dump_skills(string &text);
int skill_bump(skill_type skill, int scale = 1, bool allow_random = true);
void fixup_skills();

bool target_met(skill_type sk);
bool target_met(skill_type sk, unsigned int target);
bool check_training_target(skill_type sk);
bool check_training_targets();

void set_training_status(skill_type sk, training_status st);
void set_magic_training(training_status st);
void cleanup_innate_magic_skills();

static const skill_type skill_display_order[] =
{
    SK_TITLE,
    SK_FIGHTING,

    SK_BLANK_LINE,

    // Strength skills.
    SK_MACES_FLAILS, SK_AXES, SK_POLEARMS, SK_STAVES, SK_UNARMED_COMBAT, SK_THROWING,

    SK_BLANK_LINE,

    // Dex skills.
    SK_SHORT_BLADES, SK_LONG_BLADES, SK_RANGED_WEAPONS,

    SK_BLANK_LINE,

    // 'Defensive' skills.
    SK_ARMOUR, SK_DODGING, SK_SHIELDS, SK_STEALTH,

    SK_COLUMN_BREAK, SK_TITLE,

    SK_SPELLCASTING,

    SK_BLANK_LINE,

    SK_CONJURATIONS, SK_HEXES, SK_SUMMONINGS,
    SK_NECROMANCY, SK_FORGECRAFT, SK_TRANSLOCATIONS, SK_ALCHEMY,
    SK_FIRE_MAGIC, SK_ICE_MAGIC, SK_AIR_MAGIC, SK_EARTH_MAGIC,

    SK_BLANK_LINE,

    // Supernatural but nonmagical skills.
    SK_INVOCATIONS, SK_EVOCATIONS, SK_SHAPESHIFTING,

    SK_COLUMN_BREAK,
};

static const int ndisplayed_skills = ARRAYSZ(skill_display_order);

static inline bool is_invalid_skill(skill_type skill)
{
    return skill < SK_FIRST_SKILL || skill >= NUM_SKILLS;
}
