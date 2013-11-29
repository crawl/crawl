/**
 * @file
 * @brief Functions used when placing monsters in the dungeon.
**/


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
 * Returns -1 on failure, index into env.mons otherwise.
 * *********************************************************************** */
monster* create_monster(mgen_data mg, bool fail_msg = true);

/* ***********************************************************************
 * Primary function to create monsters. See mgen_data for details on monster
 * placement.
 * *********************************************************************** */
monster* mons_place(mgen_data mg);

/* ***********************************************************************
 * This isn't really meant to be a public function.  It is a low level
 * monster placement function used by dungeon building routines and
 * mons_place().  If you need to put a monster somewhere, use mons_place().
 * Summoned creatures can be created with create_monster().
 * *********************************************************************** */
monster* place_monster(mgen_data mg, bool force_pos = false, bool dont_place = false);

monster_type pick_random_zombie();

/* ***********************************************************************
 * Returns a monster class type of a zombie for generation
 * on the player's current level.
 * cs:         Restrict to monster types that fit this zombie type
 *             (e.g. monsters with skeletons for MONS_SKELETON_SMALL)
 * pos:        Check habitat at position.
 * *********************************************************************** */
monster_type pick_local_zombifiable_monster(level_id place,
                                            monster_type cs = MONS_NO_MONSTER,
                                            const coord_def& pos = coord_def());

void roll_zombie_hp(monster* mon);

void define_zombie(monster* mon, monster_type ztype, monster_type cs);

bool downgrade_zombie_to_skeleton(monster* mon);

class level_id;

monster_type pick_random_monster(level_id place,
                                 monster_type kind = RANDOM_MONSTER,
                                 level_id *final_place = nullptr);

conduct_type player_will_anger_monster(monster_type type);
conduct_type player_will_anger_monster(monster* mon);
bool player_angers_monster(monster* mon);

bool find_habitable_spot_near(const coord_def& where, monster_type mon_type,
                              int radius, bool allow_centre, coord_def& empty,
                              const monster* viable_mon = nullptr);

monster_type summon_any_demon(monster_type dct);

monster_type summon_any_dragon(dragon_class_type dct);

bool drac_colour_incompatible(int drac, int colour);

void mark_interesting_monst(monster* mons,
                            beh_type behaviour = BEH_SLEEP);

bool feat_compatible(dungeon_feature_type grid_wanted,
                     dungeon_feature_type actual_grid);
bool monster_habitable_grid(const monster* mon,
                            dungeon_feature_type actual_grid);
bool monster_habitable_grid(
    monster_type mt,
    dungeon_feature_type actual_grid,
    dungeon_feature_type wanted_grid_feature = DNGN_UNSEEN,
    int flies = -1,
    bool paralysed = false);
bool monster_can_submerge(const monster* mon, dungeon_feature_type grid);
coord_def find_newmons_square(monster_type mons_class, const coord_def &p,
                              const monster* viable_mon = nullptr);
coord_def find_newmons_square_contiguous(monster_type mons_class,
                                         const coord_def &start,
                                         int maxdistance = 3);
bool can_spawn_mushrooms(coord_def where);

void spawn_random_monsters();

void set_vault_mon_list(const vector<mons_spec> &list);

void setup_vault_mon_list();

monster* get_free_monster();

bool can_place_on_trap(monster_type mon_type, trap_type trap);
bool mons_airborne(monster_type mcls, int flies, bool paralysed);
void mons_add_blame(monster* mon, const string &blame_string);

// Active monster band may influence gear generation on band followers.
extern band_type active_monster_band;

#endif  // MONPLACE_H
