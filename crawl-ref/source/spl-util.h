/*
 *  File:       spl-util.h
 *  Summary:    data handlers for player spell list
 *  Written by: don brodale <dbrodale@bigfootinteractive.com>
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef SPL_UTIL_H
#define SPL_UTIL_H

#include "enum.h"    // just for NUM_SPELL_TYPES and TARG_ENEMY
#include "directn.h"  // just for DIR_NONE

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
  SPTYP_HOLY           = 1<<12, //jmf: moved to accomodate "random" miscast f/x
  SPTYP_LAST_EXPONENT  = 12,    //jmf: ``NUM_SPELL_TYPES'' kinda useless
  NUM_SPELL_TYPES      = 14,
  SPTYP_RANDOM         = 1<<14
};

struct bolt;
struct dist;

bool is_valid_spell(spell_type spell);
void init_spell_descs(void);
void init_spell_name_cache();
spell_type spell_by_name(std::string name, bool partial_match = false);

spschool_flag_type school_by_name(std::string name);

int get_spell_slot_by_letter( char letter );
spell_type get_spell_by_letter( char letter );

bool add_spell_to_memory( spell_type spell );
bool del_spell_from_memory_by_slot( int slot );

int spell_hunger(spell_type which_spell);
int spell_mana(spell_type which_spell);
int spell_difficulty(spell_type which_spell);
int spell_power_cap(spell_type spell);
int spell_range(spell_type spell, int pow, bool real_cast);
int spell_noise(spell_type spell);
int spell_noise(unsigned int disciplines, int level);

const char *get_spell_target_prompt( spell_type which_spell );

bool spell_needs_tracer(spell_type spell);
bool spell_is_direct_explosion(spell_type spell);
bool spell_needs_foe(spell_type spell);
bool spell_harms_target(spell_type spell);
bool spell_harms_area(spell_type spell);
bool spell_sanctuary_castable(spell_type spell);
int spell_levels_required(spell_type which_spell);

unsigned int get_spell_flags( spell_type which_spell );

bool spell_typematch(spell_type which_spell, unsigned int which_discipline);
unsigned int get_spell_disciplines( spell_type which_spell );
bool disciplines_conflict(unsigned int disc1, unsigned int disc2);
int count_bits( unsigned int bits );

const char *spell_title(spell_type which_spell);
const char* spelltype_short_name( int which_spelltype );
const char* spelltype_long_name( int which_spelltype );

typedef int cell_func(coord_def where, int pow, int aux, actor *agent);
typedef int cloud_func(coord_def where, int pow, int spreadrate,
                       cloud_type type, kill_category whose,
                       killer_type killer);

int apply_area_visible(cell_func cf, int power,
                       bool pass_through_trans = false, actor *agent = NULL);

int apply_area_square(cell_func cf, const coord_def& where, int power,
                      actor *agent = NULL);

int apply_area_around_square(cell_func cf, const coord_def& where, int power,
                             actor *agent = NULL);

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
                      int spread_rate = -1);

bool spell_direction( dist &spelld, bolt &pbolt,
                      targeting_type restrict = DIR_NONE,
                      targ_mode_type mode = TARG_ENEMY,
                      // pbolt.range if applicable, otherwise LOS_RADIUS
                      int range = 0,
                      bool needs_path = true, bool may_target_monster = true,
                      bool may_target_self = false, const char *prompt = NULL,
                      bool cancel_at_self = false );

const char *spelltype_name(unsigned int which_spelltype);

int spell_type2skill (unsigned int which_spelltype);
int spell_skill2type (unsigned int which_skill);

#endif
