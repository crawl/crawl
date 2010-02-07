/*
 *  File:       mon-place.h
 *  Summary:    Functions used when placing monsters in the dungeon.
 *  Written by: Linley Henzell
 */


#ifndef MONPLACE_H
#define MONPLACE_H

#include "mgen_enum.h"

class mons_spec;
struct mgen_data;

/* ***********************************************************************
 * Creates a monster near the place specified in the mgen_data, producing
 * a "puff of smoke" message if the monster cannot be placed. This is usually
 * used for summons and other monsters that want to appear near a given
 * position like a summon.
 * *********************************************************************** */
int create_monster(mgen_data mg, bool fail_msg = true);

/* ***********************************************************************
 * Primary function to create monsters. See mgen_data for details on monster
 * placement.
 * *********************************************************************** */
int mons_place(mgen_data mg);

/* ***********************************************************************
 * This isn't really meant to be a public function.  It is a low level
 * monster placement function used by dungeon building routines and
 * mons_place().  If you need to put a monster somewhere, use mons_place().
 * Summoned creatures can be created with create_monster().
 * *********************************************************************** */
int place_monster(mgen_data mg, bool force_pos = false);

monster_type pick_random_zombie();

/* ***********************************************************************
 * Returns a monster class type of a zombie that would be generated
 * on the player's current level.
 * *********************************************************************** */
monster_type pick_local_zombifiable_monster_type(int power);

class level_id;

monster_type pick_random_monster(const level_id &place,
                                 bool *chose_ood_monster = NULL);

monster_type pick_random_monster(const level_id &place,
                                 int power,
                                 int &lev_mons,
                                 bool *chose_ood_monster);

bool player_will_anger_monster(monster_type type, bool *holy = NULL,
                               bool *unholy = NULL, bool *lawful = NULL,
                               bool *antimagical = NULL);

bool player_will_anger_monster(monsters *mon, bool *holy = NULL,
                               bool *unholy = NULL, bool *lawful = NULL,
                               bool *antimagical = NULL);

bool player_angers_monster(monsters *mon);

bool empty_surrounds( const coord_def& where, dungeon_feature_type spc_wanted,
                      int radius, bool allow_centre, coord_def& empty );

monster_type summon_any_demon(demon_class_type dct);

monster_type summon_any_holy_being(holy_being_class_type hbct);

monster_type summon_any_dragon(dragon_class_type dct);

bool drac_colour_incompatible(int drac, int colour);

void mark_interesting_monst(monsters* monster,
                            beh_type behaviour = BEH_SLEEP);

bool feat_compatible(dungeon_feature_type grid_wanted,
                     dungeon_feature_type actual_grid);
bool monster_habitable_grid(const monsters *m,
                            dungeon_feature_type actual_grid);
bool monster_habitable_grid(monster_type montype,
                            dungeon_feature_type actual_grid,
                            int flies = -1,
                            bool paralysed = false);
bool monster_can_submerge(const monsters *mons, dungeon_feature_type grid);
coord_def find_newmons_square(int mons_class, const coord_def &p);
coord_def find_newmons_square_contiguous(monster_type mons_class,
                                         const coord_def &start,
                                         int maxdistance = 3);

void spawn_random_monsters();

void set_vault_mon_list(const std::vector<mons_spec> &list);

void get_vault_mon_list(std::vector<mons_spec> &list);

void setup_vault_mon_list();

monsters* get_free_monster();

bool can_place_on_trap(int mon_type, trap_type trap);
bool mons_airborne(int mcls, int flies, bool paralysed);
void mons_add_blame(monsters *mon, const std::string &blame_string);

// Active monster band may influence gear generation on band followers.
extern band_type active_monster_band;

#endif  // MONPLACE_H
