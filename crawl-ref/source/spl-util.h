/*
 *  File:       spl-util.h
 *  Summary:    data handlers for player spell list
 *  Written by: don brodale <dbrodale@bigfootinteractive.com>
 */


#ifndef SPL_UTIL_H
#define SPL_UTIL_H

#include "enum.h"

enum spschool_flag_type
{
  SPTYP_NONE           = 0, // "0" is reserved for no type at all {dlb}
  SPTYP_CONJURATION    = 1, // was 11, but only for old typematch routine {dlb}
  SPTYP_ENCHANTMENT    = 1<<1,
  SPTYP_FIRE           = 1<<2,
  SPTYP_ICE            = 1<<3,
  SPTYP_TRANSMUTATION  = 1<<4,
  SPTYP_NECROMANCY     = 1<<5,
  SPTYP_SUMMONING      = 1<<6,
  SPTYP_DIVINATION     = 1<<7,
  SPTYP_TRANSLOCATION  = 1<<8,
  SPTYP_POISON         = 1<<9,
  SPTYP_EARTH          = 1<<10,
  SPTYP_AIR            = 1<<11,
  SPTYP_HOLY           = 1<<12, //jmf: moved to accommodate "random" miscast f/x
  SPTYP_LAST_EXPONENT  = 12,    //jmf: ``NUM_SPELL_TYPES'' kinda useless
  NUM_SPELL_TYPES      = 14,
  SPTYP_RANDOM         = 1<<14,
};

struct bolt;
class dist;

enum spell_highlight_colours
{
    COL_UNKNOWN      = LIGHTGRAY,   // spells for which no known brand applies.
    COL_UNMEMORIZED  = LIGHTBLUE,   // spell hasn't been memorized (used reading spellbooks)
    COL_MEMORIZED    = LIGHTGRAY,   // spell has been memorized
    COL_USELESS      = DARKGRAY,    // ability would have no useful effect
    COL_INAPPLICABLE = COL_USELESS, // ability cannot be meanifully applied (eg, no targets)
    COL_FORBIDDEN    = LIGHTRED,    // The player's god hates this abilty

    COL_EMPOWERED    = LIGHTGREEN,  // The ability is made stronger by the player's status
    COL_FAVORED      = GREEN,       // the player's god likes this ability
};

bool is_valid_spell(spell_type spell);
void init_spell_descs(void);
void init_spell_name_cache();
spell_type spell_by_name(std::string name, bool partial_match = false);

spschool_flag_type school_by_name(std::string name);

int get_spell_slot_by_letter(char letter);
int get_spell_slot(spell_type spell);
spell_type get_spell_by_letter(char letter);

bool add_spell_to_memory(spell_type spell);
bool del_spell_from_memory_by_slot(int slot);
bool del_spell_from_memory(spell_type spell);

int spell_hunger(spell_type which_spell, bool rod = false);
int spell_mana(spell_type which_spell);
int spell_difficulty(spell_type which_spell);
int spell_power_cap(spell_type spell);
int spell_range(spell_type spell, int pow, bool real_cast,
                bool player_spell=true);
int spell_noise(spell_type spell);
int spell_noise(unsigned int disciplines, int level);

const char *get_spell_target_prompt(spell_type which_spell);

bool spell_needs_tracer(spell_type spell);
bool spell_is_direct_explosion(spell_type spell);
bool spell_needs_foe(spell_type spell);
bool spell_harms_target(spell_type spell);
bool spell_harms_area(spell_type spell);
bool spell_sanctuary_castable(spell_type spell);
int spell_levels_required(spell_type which_spell);

unsigned int get_spell_flags(spell_type which_spell);

bool spell_typematch(spell_type which_spell, unsigned int which_discipline);
unsigned int get_spell_disciplines(spell_type which_spell);
bool disciplines_conflict(unsigned int disc1, unsigned int disc2);
int count_bits(unsigned int bits);

const char *spell_title(spell_type which_spell);
const char* spelltype_short_name(int which_spelltype);
const char* spelltype_long_name(int which_spelltype);

typedef int cell_func(coord_def where, int pow, int aux, actor *agent);
typedef int monster_func(monster* mon, int pow);
typedef int cloud_func(coord_def where, int pow, int spreadrate,
                       cloud_type type, kill_category whose,
                       killer_type killer, int colour, std::string name,
                       std::string tile);

int apply_area_visible(cell_func cf, int power,
                       bool pass_through_trans = false, actor *agent = NULL);

int apply_area_square(cell_func cf, const coord_def& where, int power,
                      actor *agent = NULL);

int apply_area_around_square(cell_func cf, const coord_def& where, int power,
                             actor *agent = NULL);

int apply_monsters_around_square(monster_func mf, const coord_def& where,
                                 int power);

int apply_random_around_square(cell_func cf, const coord_def& where,
                               bool hole_in_middle, int power, int max_targs,
                               actor *agent = NULL);

int apply_one_neighbouring_square(cell_func cf, int power, actor *agent = NULL);

int apply_area_within_radius(cell_func cf,  const coord_def& where,
                             int pow, int radius, int ctype,
                             actor *agent = NULL);

void apply_area_cloud(cloud_func func, const coord_def& where,
                      int pow, int number, cloud_type ctype,
                      kill_category kc, killer_type killer,
                      int spread_rate = -1, int colour = -1,
                      std::string name = "", std::string tile = "");

bool spell_direction(dist &spelld, bolt &pbolt,
                      targeting_type restrict = DIR_NONE,
                      targ_mode_type mode = TARG_HOSTILE,
                      // pbolt.range if applicable, otherwise LOS_RADIUS
                      int range = 0,
                      bool needs_path = true, bool may_target_monster = true,
                      bool may_target_self = false,
                      const char *target_prefix = NULL,
                      const char *prompt = NULL,
                      bool cancel_at_self = false);

skill_type spell_type2skill (unsigned int which_spelltype);

spell_type zap_type_to_spell(zap_type zap);

bool spell_is_useless(spell_type spell, bool transient = false);
bool spell_is_empowered(spell_type spell);
bool spell_is_useful(spell_type spell);
bool spell_is_risky(spell_type spell);

int spell_highlight_by_utility(spell_type spell,
                                int default_color = COL_UNKNOWN,
                                bool transient = false,
                                bool rod_spell = false);
bool spell_no_hostile_in_range(spell_type spell, int minRange);

#endif
