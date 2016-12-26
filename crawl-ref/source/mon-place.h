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

bool needs_resolution(monster_type mon_type);

monster_type resolve_monster_type(monster_type mon_type,
                                  monster_type &base_type,
                                  proximity_type proximity = PROX_ANYWHERE,
                                  coord_def *pos = nullptr,
                                  unsigned mmask = 0,
                                  dungeon_char_type *stair_type = nullptr,
                                  level_id *place = nullptr,
                                  bool *want_band = nullptr,
                                  bool allow_ood = true);

const monster_type fixup_zombie_type(const monster_type cls,
                                     const monster_type base_type);

/* ***********************************************************************
 * This isn't really meant to be a public function. It is a low level
 * monster placement function used by dungeon building routines and
 * mons_place(). If you need to put a monster somewhere, use mons_place().
 * Summoned creatures can be created with create_monster().
 * *********************************************************************** */
monster* place_monster(mgen_data mg, bool force_pos = false, bool dont_place = false);

/* ***********************************************************************
 * Returns a monster class type of a zombie for generation
 * on the player's current level.
 * cs:         Restrict to monster types that fit this zombie type
 *             (e.g. monsters with skeletons for MONS_SKELETON_SMALL)
 * pos:        Check habitat at position.
 * for_corpse: Whether this monster is intended only for use as a potentially
 *             zombifiable corpse. (I.e., whether we care about its speed when
 *             placing in D...)
 * *********************************************************************** */
monster_type pick_local_zombifiable_monster(level_id place,
                                            monster_type cs = MONS_NO_MONSTER,
                                            const coord_def& pos = coord_def(),
                                            bool for_corpse = false);

monster_type pick_local_corpsey_monster(level_id place);

void roll_zombie_hp(monster* mon);

void define_zombie(monster* mon, monster_type ztype, monster_type cs);

bool downgrade_zombie_to_skeleton(monster* mon);

class level_id;

monster_type pick_random_monster(level_id place,
                                 monster_type kind = RANDOM_MONSTER,
                                 level_id *final_place = nullptr,
                                 bool allow_ood = true);

conduct_type player_will_anger_monster(monster_type type);
conduct_type player_will_anger_monster(const monster &mon);
bool player_angers_monster(monster* mon);

bool find_habitable_spot_near(const coord_def& where, monster_type mon_type,
                              int radius, bool allow_centre, coord_def& empty,
                              const monster* viable_mon = nullptr);

monster_type random_demon_by_tier(int tier);
monster_type summon_any_demon(monster_type dct, bool use_local_demons = false);

bool drac_colour_incompatible(int drac, int colour);

bool monster_habitable_grid(const monster* mon,
                            dungeon_feature_type actual_grid);
bool monster_habitable_grid(monster_type mt, dungeon_feature_type actual_grid,
                            dungeon_feature_type wanted_grid = DNGN_UNSEEN,
                            bool flies = false);
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
void mons_add_blame(monster* mon, const string &blame_string);

void debug_bands();

// Active monster band may influence gear generation on band followers.
extern band_type active_monster_band;

#if TAG_MAJOR_VERSION == 34
#define VAULT_MON_TYPES_KEY   "vault_mon_types"
#define VAULT_MON_BASES_KEY   "vault_mon_bases"
#define VAULT_MON_PLACES_KEY  "vault_mon_places"
#define VAULT_MON_WEIGHTS_KEY "vault_mon_weights"
#define VAULT_MON_BANDS_KEY   "vault_mon_bands"
#endif

#endif  // MONPLACE_H
