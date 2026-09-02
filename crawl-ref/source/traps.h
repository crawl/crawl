/**
 * @file
 * @brief Traps related functions.
**/

#pragma once

#include <vector>

#include "enum.h"

using std::vector;

#define NEWLY_TRAPPED_KEY "newly_trapped"

// If present on a held actor (even if false), it indicates they are held by a
// net. If true, this is a 'real' net that should drop on the floor after they
// escape.
#define NET_IS_REAL_KEY "net_is_real"

#define NET_STARTING_DURABILITY 8

struct bolt;
class monster;

bool trap_is_bad_for_player(dungeon_feature_type trap);
bool trap_is_safe(dungeon_feature_type trap, const actor* act = 0);
bool trap_is_safe_from_afar(dungeon_feature_type trap, const actor* act = 0);
void trigger_trap(actor& triggerer);

bool chaos_lace_criteria(monster* mon);

const char* held_status(actor *act = nullptr);

void destroy_trap(const coord_def& pos);

bool is_valid_shaft_level(bool respect_brflags = true);
void set_shafted();
void roll_trap_effects();
void do_trap_effects();
level_id generic_shaft_dest(level_id place);

int       trap_rate_for_place();
dungeon_feature_type random_trap_for_place(bool dispersal_ok = true);

void place_webs(int num);
