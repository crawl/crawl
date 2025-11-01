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

// If present on a held actor (even if false), it indicates they are held by a
// net. If true, this is a 'real' net that should drop on the floor after they
// escape.
#define NET_IS_REAL_KEY "net_is_real"

#define NET_STARTING_DURABILITY 8

struct bolt;
class monster;
struct trap_def;

bool chaos_lace_criteria(monster* mon);

const char* held_status(actor *act = nullptr);
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

bool is_regular_trap(trap_type trap);
#if TAG_MAJOR_VERSION == 34
bool is_removed_trap(trap_type trap);
#endif
