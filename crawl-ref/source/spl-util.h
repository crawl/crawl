/**
 * @file
 * @brief data handlers for player spell list
**/

#pragma once

#include <functional>
#include <vector>

#include "enum.h"
#include "mon-info.h"

using std::vector;

enum class spschool
{
  none           = 0,
  conjuration    = 1<<0,
  hexes          = 1<<1,
  fire           = 1<<2,
  ice            = 1<<3,
  transmutation  = 1<<4,
  necromancy     = 1<<5,
  summoning      = 1<<6,
  translocation  = 1<<7,
  poison         = 1<<8,
  earth          = 1<<9,
  air            = 1<<10,
  LAST_SCHOOL    = spschool::air,
  random         = spschool::LAST_SCHOOL << 1,
};
DEF_BITFIELD(spschools_type, spschool, 10);
const int SPSCHOOL_LAST_EXPONENT = spschools_type::last_exponent;
COMPILE_CHECK(spschools_type::exponent(SPSCHOOL_LAST_EXPONENT)
              == spschool::LAST_SCHOOL);
// Moved from mapdef.cc:5318, needed to ensure randbook spschools are short ints
COMPILE_CHECK(static_cast<int>(spschool::LAST_SCHOOL) < SHRT_MAX);

struct bolt;
class dist;
struct direction_chooser_args;

enum spell_highlight_colours
{
    COL_UNKNOWN      = LIGHTGRAY,   // spells for which no known brand applies.
    COL_UNMEMORIZED  = LIGHTBLUE,   // spell hasn't been memorised (used reading spellbooks)
    COL_MEMORIZED    = LIGHTGRAY,   // spell has been memorised
    COL_USELESS      = DARKGRAY,    // ability would have no useful effect
    COL_INAPPLICABLE = COL_USELESS, // ability cannot be meaningfully applied (e.g., no targets)
    COL_FORBIDDEN    = LIGHTRED,    // The player's god hates this ability
    COL_DANGEROUS    = LIGHTRED,    // ability/spell use could be dangerous
};

bool is_valid_spell(spell_type spell);
void init_spell_descs();
void init_spell_name_cache();
bool spell_data_initialized();
spell_type spell_by_name(string name, bool partial_match = false);

spschool school_by_name(string name);

int get_spell_slot_by_letter(char letter);
int get_spell_letter(spell_type spell);
spell_type get_spell_by_letter(char letter);

bool add_spell_to_memory(spell_type spell);
bool del_spell_from_memory_by_slot(int slot);
bool del_spell_from_memory(spell_type spell);

int spell_mana(spell_type which_spell, bool real_spell = true);
int spell_difficulty(spell_type which_spell);
int spell_power_cap(spell_type spell);
int spell_range(spell_type spell, int pow, bool allow_bonus = true,
                bool ignore_shadows = false);
int spell_noise(spell_type spell);
int spell_effect_noise(spell_type spell, bool random = true);

const char *get_spell_target_prompt(spell_type which_spell);
tileidx_t get_spell_tile(spell_type which_spell);

bool spell_is_direct_explosion(spell_type spell);
bool spell_harms_target(spell_type spell);
bool spell_harms_area(spell_type spell);
bool spell_is_direct_attack(spell_type spell);
int spell_levels_required(spell_type which_spell);

spell_flags get_spell_flags(spell_type which_spell);

bool spell_typematch(spell_type which_spell, spschool which_disc);
spschools_type get_spell_disciplines(spell_type which_spell);
int count_bits(uint64_t bits);

template <class E, int Exp>
int count_bits(enum_bitfield<E, Exp> bits)
{
    return count_bits(bits.flags);
}

const char *spell_title(spell_type which_spell);
const char* spelltype_short_name(spschool which_spelltype);
const char* spelltype_long_name(spschool which_spelltype);

typedef function<int (coord_def where)> cell_func;
typedef function<int (coord_def where, int pow, int spreadrate,
                       cloud_type type, const actor* agent, int excl_rad)>
        cloud_func;

int apply_area_visible(cell_func cf, const coord_def& where);

int apply_random_around_square(cell_func cf, const coord_def& where,
                               bool hole_in_middle, int max_targs);

void apply_area_cloud(cloud_func func, const coord_def& where,
                      int pow, int number, cloud_type ctype,
                      const actor *agent, int spread_rate = -1,
                      int excl_rad = -1);

bool spell_direction(dist &spelld, bolt &pbolt,
                     direction_chooser_args *args = nullptr);

skill_type spell_type2skill(spschool spelltype);
spschool skill2spell_type(skill_type spell_skill);

skill_type arcane_mutation_to_skill(mutation_type mutation);
bool cannot_use_schools(spschools_type schools);

bool spell_is_form(spell_type spell) PURE;

bool casting_is_useless(spell_type spell, bool temp);
string casting_uselessness_reason(spell_type spell, bool temp);
bool spell_is_useless(spell_type spell, bool temp = true,
                      bool prevent = false, bool fake_spell = false) PURE;
string spell_uselessness_reason(spell_type spell, bool temp = true,
                                bool prevent = false,
                                bool fake_spell = false) PURE;

int spell_highlight_by_utility(spell_type spell,
                                int default_colour = COL_UNKNOWN,
                                bool transient = false,
                                bool memcheck = false);
bool spell_no_hostile_in_range(spell_type spell);

bool spell_is_soh_breath(spell_type spell);
const vector<spell_type> *soh_breath_spells(spell_type spell);

bool spell_removed(spell_type spell);
