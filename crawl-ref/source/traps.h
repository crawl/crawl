/**
 * @file
 * @brief Traps related functions.
**/

#pragma once

#include <vector>

#include "enum.h"
#include "trap-type.h"

using std::vector;

#define NEWLY_TRAPPED_KEY "newly_trapped"

#define NET_MIN_DURABILITY -7

struct bolt;
class monster;
struct trap_def;

void free_self_from_net();
void mons_clear_trapping_net(monster* mon);
void free_stationary_net(int item_index);

int get_trapping_net(const coord_def& where, bool trapped = true);
const char* held_status(actor *act = nullptr);
bool monster_caught_in_net(monster* mon);
bool player_caught_in_net();
void clear_trapping_net();
void check_net_will_hold_monster(monster* mon);
vector<coord_def> find_golubria_on_level();

dungeon_feature_type trap_feature(trap_type type) IMMUTABLE;
trap_type trap_type_from_feature(dungeon_feature_type type);

void destroy_trap(const coord_def& pos);
trap_def* trap_at(const coord_def& where);
trap_type get_trap_type(const coord_def& where);

bool is_valid_shaft_level(bool respect_brflags = true);
void set_shafted();
void roll_trap_effects();
void do_trap_effects();
level_id generic_shaft_dest(level_id place);

int       trap_rate_for_place();
trap_type random_trap_for_place(bool dispersal_ok = true);

void place_webs(int num);
bool ensnare(actor *fly);
void leave_web(bool quiet = false);
void monster_web_cleanup(const monster &mons, bool quiet = false);
void stop_being_held();

bool is_regular_trap(trap_type trap);
#if TAG_MAJOR_VERSION == 34
bool is_removed_trap(trap_type trap);
#endif
