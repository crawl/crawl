/**
 * @file
 * @brief Traps related functions.
**/

#ifndef TRAPS_H
#define TRAPS_H

#include "enum.h"
#include "externs.h"

#define NEWLY_TRAPPED_KEY "newly_trapped"

struct bolt;
class monster;
struct trap_def;

void disarm_trap(const coord_def& where);
void free_self_from_net();
void mons_clear_trapping_net(monster* mon);
void free_stationary_net(int item_index);

void handle_traps(trap_type trt, int i, bool trap_known);
int get_trapping_net(const coord_def& where, bool trapped = true);
bool monster_caught_in_net(monster* mon, actor *agent);
bool player_caught_in_net();
void clear_trapping_net();
void check_net_will_hold_monster(monster* mon);
vector<coord_def> find_golubria_on_level();

dungeon_feature_type trap_category(trap_type type);

int reveal_traps(const int range);
void destroy_trap(const coord_def& pos);
trap_def* find_trap(const coord_def& where);
trap_type get_trap_type(const coord_def& where);

bool     is_valid_shaft_level(const level_id &place = level_id::current());
level_id generic_shaft_dest(coord_def pos, bool known);
void     handle_items_on_shaft(const coord_def& where, bool open_shaft);

int       num_traps_for_place();
trap_type random_trap_for_place();

int count_traps(trap_type ttyp);
void place_webs(int num);
bool maybe_destroy_web(actor *oaf);
bool ensnare(actor *fly);
#endif
