/**
 * @file
 * @brief Functions for eating and butchering.
**/

#pragma once

#include "hunger-state-t.h"
#include "mon-enum.h"

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

// Must match the order of hunger_state_t enums
constexpr int hunger_threshold[HS_ENGORGED + 1] =
{
    HUNGER_FAINTING, HUNGER_STARVING, HUNGER_NEAR_STARVING, HUNGER_VERY_HUNGRY,
    HUNGER_HUNGRY, HUNGER_SATIATED, HUNGER_FULL, HUNGER_VERY_FULL,
    HUNGER_ENGORGED
};

bool eat_food();

void make_hungry(int hunger_amount, bool suppress_msg, bool magic = false);

void lessen_hunger(int statiated_amount, bool suppress_msg, int max = - 1);

void set_hunger(int new_hunger_level, bool suppress_msg);

bool is_inedible(const item_def &item, bool temp = true);

bool can_eat(const item_def &food, bool suppress_msg, bool check_hunger = true,
                                                        bool temp = true);

bool eat_item(item_def &food);

hunger_state_t calc_hunger_state();
bool food_change(bool initial = false);

int you_max_hunger();
int you_min_hunger();
bool apply_starvation_penalties();
bool you_foodless(bool temp = true);
bool you_drinkless(bool temp = true);

void handle_starvation();
int hunger_bars(const int hunger);
string hunger_cost_string(const int hunger);
