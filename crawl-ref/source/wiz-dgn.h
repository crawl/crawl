/**
 * @file
 * @brief Dungeon related wizard functions.
**/

#pragma once

#include <string>

// Define this constant to put the builder into a debug mode where on a veto
// triggered by a wizmode travel command or level reload, instead of retrying
// it immediately exits the builder so that you can see what happened.
//#define DEBUG_VETO_RESUME

#include "player.h"

#define WIZ_LAST_FEATURE_TYPE_PROP "last_created_feature"
dungeon_feature_type wizard_select_feature(bool mimic, bool allow_fprop=false);
bool wizard_create_feature(const coord_def& pos = you.pos(),
                                    dungeon_feature_type feat=DNGN_UNSEEN,
                                    bool mimic=false);
bool wizard_create_feature(dist &target, dungeon_feature_type feat, bool mimic);
void wizard_list_branches();
void wizard_map_level();
void wizard_place_stairs(bool down);
void wizard_level_travel(bool down);
void wizard_interlevel_travel();
void wizard_list_levels();
void wizard_recreate_level();
void wizard_clear_used_vaults();
bool debug_make_trap(const coord_def& pos = you.pos());
bool debug_make_shop(const coord_def& pos = you.pos());
void debug_place_map(bool primary);
void wizard_primary_vault();
void debug_test_explore();
void wizard_abyss_speed();

bool is_wizard_travel_target(const level_id l);
