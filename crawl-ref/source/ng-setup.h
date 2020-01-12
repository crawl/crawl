/**
 * @file
 * @brief Setup "you" for a new game.
**/

#pragma once

#include "item-prop-enum.h"

struct item_def;
item_def* newgame_make_item(object_class_type base,
                            int sub_type,
                            int qty = 1, int plus = 0, int force_ego = 0,
                            bool force_tutorial = false);

struct newgame_def;
void setup_game(const newgame_def& ng, bool normal_dungeon_setup=true);
void initial_dungeon_setup();

void give_items_skills(const newgame_def& ng);
