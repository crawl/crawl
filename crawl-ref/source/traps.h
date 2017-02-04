/**
 * @file
 * @brief Traps related functions.
**/

#ifndef TRAPS_H
#define TRAPS_H

#include "enum.h"

#define NEWLY_TRAPPED_KEY "newly_trapped"

#define NET_MIN_DURABILITY -7

struct bolt;
class monster;
struct trap_def;

void search_around();
void free_self_from_net();
void mons_clear_trapping_net(monster* mon);
void free_stationary_net(int item_index);

int get_trapping_net(const coord_def& where, bool trapped = true);
const char* held_status(actor *act = nullptr);
bool monster_caught_in_net(monster* mon, actor *agent);
bool player_caught_in_net();
void clear_trapping_net();
void check_net_will_hold_monster(monster* mon);
vector<coord_def> find_golubria_on_level();

dungeon_feature_type trap_category(trap_type type);

int reveal_traps(const int range);
void destroy_trap(const coord_def& pos);
trap_def* trap_at(const coord_def& where);
trap_type get_trap_type(const coord_def& where);

// known is relevant only during level-gen
bool is_valid_shaft_level(bool known = false);
level_id generic_shaft_dest(coord_def pos, bool known);

int       num_traps_for_place();
trap_type random_trap_for_place();
trap_type random_vault_trap();

int count_traps(trap_type ttyp);
void place_webs(int num);
bool ensnare(actor *fly);
void leave_web(bool quiet = false);
void stop_being_held();

#endif
