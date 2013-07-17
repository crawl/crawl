/**
 * @file
 * @brief More skill related functions.
**/


#ifndef SKILLS2_H
#define SKILLS2_H

const int MAX_SKILL_ORDER = 100;

#include "enum.h"
#include "player.h"

struct skill_state
{
    FixedBitVector<NUM_SKILLS>            can_train;
    FixedVector<uint8_t, NUM_SKILLS>      skills;
    FixedVector<int, NUM_SKILLS>          real_skills;    // Those two are
    FixedVector<int, NUM_SKILLS>          changed_skills; // scaled by 10.
    FixedVector<int8_t, NUM_SKILLS>       train;
    FixedVector<unsigned int, NUM_SKILLS> training;
    FixedVector<unsigned int, NUM_SKILLS> skill_points;
    FixedVector<unsigned int, NUM_SKILLS> ct_skill_points;
    FixedVector<uint8_t, NUM_SKILLS>      skill_order;
    int skill_cost_level;
    unsigned int total_experience;
    bool auto_training;
    int exp_available;
    vector<int> manual_charges;

    void save();
    void restore_levels();
    void restore_training();
};

int get_skill_progress(skill_type sk, int level, int points, int scale);
int get_skill_progress(skill_type sk, int scale);
int get_skill_percentage(const skill_type x);
const char *skill_name(skill_type which_skill);
skill_type str_to_skill(const string &skill);

string skill_title(
    skill_type best_skill, uint8_t skill_lev,
    // these used for ghosts and hiscores:
    int species = -1, int str = -1, int dex = -1, int god = -1,
    int piety = you.piety);
string skill_title_by_rank(
    skill_type best_skill, uint8_t skill_rank,
    // these used for ghosts and hiscores:
    int species = -1, int str = -1, int dex = -1, int god = -1,
    int piety = you.piety);
unsigned get_skill_rank(unsigned skill_lev);

string player_title();

skill_type best_skill(skill_type min_skill, skill_type max_skill,
                      skill_type excl_skill = SK_NONE);
void init_skill_order();

void calc_mp();
void calc_hp();
bool is_useless_skill(skill_type skill);
bool is_harmful_skill(skill_type skill);
bool all_skills_maxed(bool inc_harmful = false);

int species_apt(skill_type skill, species_type species = you.species);
float species_apt_factor(skill_type sk, species_type sp = you.species);
unsigned int skill_exp_needed(int lev, skill_type sk,
                              species_type sp = you.species);

float crosstrain_bonus(skill_type sk);
bool crosstrain_other(skill_type sk, bool show_zero);
bool is_antitrained(skill_type sk);
bool antitrain_other(skill_type sk, bool show_zero);

int elemental_preference(spell_type spell, int scale = 1);

void skill_menu(int flag = 0, int exp = 0);
void dump_skills(string &text);
int skill_transfer_amount(skill_type sk);
int transfer_skill_points(skill_type fsk, skill_type tsk, int skp_max,
                          bool simu, bool boost = false);
int skill_bump(skill_type skill, int scale = 1);
void fixup_skills();

static const skill_type skill_display_order[] =
{
    SK_TITLE,
    SK_FIGHTING, SK_SHORT_BLADES, SK_LONG_BLADES, SK_AXES,
    SK_MACES_FLAILS, SK_POLEARMS, SK_STAVES, SK_UNARMED_COMBAT,

    SK_BLANK_LINE,

    SK_BOWS, SK_CROSSBOWS, SK_THROWING, SK_SLINGS,

    SK_BLANK_LINE,

    SK_ARMOUR, SK_DODGING, SK_SHIELDS,

    SK_COLUMN_BREAK, SK_TITLE,

    SK_SPELLCASTING, SK_CONJURATIONS, SK_HEXES, SK_CHARMS, SK_SUMMONINGS,
    SK_NECROMANCY, SK_TRANSLOCATIONS, SK_TRANSMUTATIONS,
    SK_FIRE_MAGIC, SK_ICE_MAGIC, SK_AIR_MAGIC, SK_EARTH_MAGIC, SK_POISON_MAGIC,

    SK_BLANK_LINE,

    SK_INVOCATIONS, SK_EVOCATIONS, SK_STEALTH,

    SK_COLUMN_BREAK,
};

static const int ndisplayed_skills = ARRAYSZ(skill_display_order);

static inline bool is_invalid_skill(skill_type skill)
{
    return (skill < SK_FIRST_SKILL || skill >= NUM_SKILLS);
}

#endif
