/**
 * @file
 * @brief Functions for blood & chunk rot.
 **/

#pragma once

// aut / rot_time_factor = units on corpse "special" property
#define ROT_TIME_FACTOR 20

// aut until fresh chunks rot away
#define FRESHEST_CHUNK  2200
// # of special units until fresh corpses rot away
#define FRESHEST_CORPSE (FRESHEST_CHUNK / ROT_TIME_FACTOR)
// aut until fresh blood potions rot away
#define FRESHEST_BLOOD 22500

bool is_perishable_stack(const item_def &item) PURE;

void init_perishable_stack(item_def &stack, int age = -1);
int remove_oldest_perishable_item(item_def &stack);
void remove_newest_perishable_item(item_def &stack, int quant = -1);
void merge_perishable_stacks(const item_def &source, item_def &dest, int quant);

void rot_inventory_food(int time_delta);
void rot_floor_items(int elapsedTime);


