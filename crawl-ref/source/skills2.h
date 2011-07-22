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
    FixedVector<uint8_t, NUM_SKILLS>      skills;
    FixedVector<uint8_t, NUM_SKILLS>      changed_skills;
    FixedVector<char, NUM_SKILLS>         train;
    FixedVector<unsigned int, NUM_SKILLS> training;
    FixedVector<unsigned int, NUM_SKILLS> skill_points;
    FixedVector<unsigned int, NUM_SKILLS> ct_skill_points;
    FixedVector<uint8_t, NUM_SKILLS>      skill_order;
    int skill_cost_level;
    int total_skill_points;
    bool auto_training;
    int exp_available;
    int manual_charges;

    void save();
    void restore_levels();
    void restore_training();
};

int get_skill_percentage(const skill_type x);
const char *skill_name(skill_type which_skill);
skill_type str_to_skill(const std::string &skill);

std::string skill_title(
    skill_type best_skill, uint8_t skill_lev,
    // these used for ghosts and hiscores:
    int species = -1, int str = -1, int dex = -1, int god = -1);
std::string skill_title_by_rank(
    skill_type best_skill, uint8_t skill_rank,
    // these used for ghosts and hiscores:
    int species = -1, int str = -1, int dex = -1, int god = -1);
unsigned get_skill_rank(unsigned skill_lev);

std::string player_title();

skill_type best_skill(skill_type min_skill, skill_type max_skill,
                      skill_type excl_skill = SK_NONE);
void init_skill_order();

void calc_mp();
void calc_hp();
bool is_useless_skill(int skill);

int species_apt(skill_type skill, species_type species = you.species);
float species_apt_factor(skill_type sk, species_type sp = you.species);
unsigned int skill_exp_needed(int lev);
unsigned int skill_exp_needed(int lev, skill_type sk,
                              species_type sp = you.species);

bool compare_skills(skill_type sk1, skill_type sk2);
float crosstrain_bonus(skill_type sk);
bool crosstrain_other(skill_type sk, bool show_zero);
bool is_antitrained(skill_type sk);
bool antitrain_other(skill_type sk, bool show_zero);

void skill_menu(int flag = 0, int exp = 0);
bool is_invalid_skill(skill_type skill);
void dump_skills(std::string &text);
int skill_transfer_amount(skill_type sk);
int transfer_skill_points(skill_type fsk, skill_type tsk, int skp_max,
                          bool simu, bool boost = false);
int skill_bump(skill_type skill);

static const skill_type skill_display_order[] =
{
    SK_TITLE,
    SK_FIGHTING, SK_SHORT_BLADES, SK_LONG_BLADES, SK_AXES,
    SK_MACES_FLAILS, SK_POLEARMS, SK_STAVES, SK_UNARMED_COMBAT,

    SK_BLANK_LINE,

    SK_BOWS, SK_CROSSBOWS, SK_THROWING, SK_SLINGS,

    SK_BLANK_LINE,

    SK_ARMOUR, SK_DODGING, SK_STEALTH, SK_SHIELDS,

    SK_COLUMN_BREAK, SK_TITLE,

    SK_STABBING, SK_TRAPS_DOORS,

    SK_BLANK_LINE,

    SK_SPELLCASTING, SK_CONJURATIONS, SK_HEXES, SK_CHARMS, SK_SUMMONINGS,
    SK_NECROMANCY, SK_TRANSLOCATIONS, SK_TRANSMUTATIONS,
    SK_FIRE_MAGIC, SK_ICE_MAGIC, SK_AIR_MAGIC, SK_EARTH_MAGIC, SK_POISON_MAGIC,

    SK_BLANK_LINE,

    SK_INVOCATIONS, SK_EVOCATIONS,

    SK_COLUMN_BREAK,
};

static const int ndisplayed_skills = ARRAYSZ(skill_display_order);

#endif
