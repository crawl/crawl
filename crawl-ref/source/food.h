/**
 * @file
 * @brief Functions for eating and butchering.
**/

#ifndef FOOD_H
#define FOOD_H

#include "mon-enum.h"

#define BERSERK_NUTRITION     700

#define HUNGER_FAINTING       400
#define HUNGER_STARVING       900
#define HUNGER_NEAR_STARVING 1433
#define HUNGER_VERY_HUNGRY   1966
#define HUNGER_HUNGRY        2500
#define HUNGER_SATIATED      6900
#define HUNGER_FULL          8900
#define HUNGER_VERY_FULL    10900
#define HUNGER_ENGORGED     39900

#define HUNGER_DEFAULT       5900
#define HUNGER_MAXIMUM      11900

bool eat_food(int slot = -1);

void make_hungry(int hunger_amount, bool suppress_msg, bool magic = false);

void lessen_hunger(int statiated_amount, bool suppress_msg, int max = - 1);

void set_hunger(int new_hunger_level, bool suppress_msg);

bool is_bad_food(const item_def &food);
bool is_poisonous(const item_def &food);
bool is_mutagenic(const item_def &food);
bool causes_rot(const item_def &food);
bool is_inedible(const item_def &item);
bool is_preferred_food(const item_def &food);
bool is_forbidden_food(const item_def &food);
corpse_effect_type determine_chunk_effect(const item_def &carrion,
                                          bool innate_only = false);
corpse_effect_type determine_chunk_effect(corpse_effect_type chunktype,
                                          bool innate_only = false);
mon_intel_type corpse_intelligence(const item_def &corpse);

bool can_eat(const item_def &food, bool suppress_msg, bool check_hunger = true);

bool carrion_is_poisonous(const item_def &food);

bool eat_item(item_def &food);

int eat_from_floor(bool skip_chunks = true);
bool eat_from_inventory();
int prompt_eat_chunks(bool only_auto = false);

bool food_change(bool initial = false);

bool prompt_eat_inventory_item(int slot = -1);

void chunk_nutrition_message(int nutrition);

void vampire_nutrition_per_turn(const item_def &corpse, int feeding = 0);

void finished_eating_message(int food_type);

int you_max_hunger();
int you_min_hunger();
bool you_foodless(bool can_eat = false);
// Is the player always foodless or just because of a temporary change?
bool you_foodless_normally();

void handle_starvation();
int hunger_bars(const int hunger);
string hunger_cost_string(const int hunger);
#endif
