/*
 *  File:       food.h
 *  Summary:    Functions for eating and butchering.
 *  Written by: Linley Henzell
 */


#ifndef FOOD_H
#define FOOD_H

enum food_type
{
    FOOD_MEAT_RATION,                  //    0
    FOOD_BREAD_RATION,
    FOOD_PEAR,
    FOOD_APPLE,
    FOOD_CHOKO,
    FOOD_HONEYCOMB,                    //    5
    FOOD_ROYAL_JELLY,
    FOOD_SNOZZCUMBER,
    FOOD_PIZZA,
    FOOD_APRICOT,
    FOOD_ORANGE,                       //   10
    FOOD_BANANA,
    FOOD_STRAWBERRY,
    FOOD_RAMBUTAN,
    FOOD_LEMON,
    FOOD_GRAPE,                        //   15
    FOOD_SULTANA,
    FOOD_LYCHEE,
    FOOD_BEEF_JERKY,
    FOOD_CHEESE,
    FOOD_SAUSAGE,                      //   20
    FOOD_CHUNK,
    FOOD_AMBROSIA,
    NUM_FOODS                          //   23
};

int count_corpses_in_pack(bool blood_only = false);
bool butchery(int which_corpse = -1, bool bottle_blood = false);

bool eat_food(int slot = -1);

void make_hungry(int hunger_amount, bool suppress_msg,
                 bool allow_reducing = false);

void lessen_hunger(int statiated_amount, bool suppress_msg);

void set_hunger(int new_hunger_level, bool suppress_msg);

void weapon_switch(int targ);

bool is_bad_food(const item_def &food);
bool is_poisonous(const item_def &food);
bool is_mutagenic(const item_def &food);
bool is_contaminated(const item_def &food);
bool causes_rot(const item_def &food);
bool is_inedible(const item_def &item);
bool is_preferred_food(const item_def &food);
bool is_forbidden_food(const item_def &food);
bool check_amu_the_gourmand(bool reqid);

bool can_ingest(int what_isit, int kindof_thing, bool suppress_msg,
                bool reqid = false, bool check_hunger = true);

void eat_floor_item(int item_link);

int eat_from_floor(bool skip_chunks = true);
bool eat_from_inventory();
int prompt_eat_chunks();

bool food_change(bool suppress_message = false);
void eat_inventory_item(int which_inventory_slot);

bool prompt_eat_inventory_item(int slot = -1);

void chunk_nutrition_message(int nutrition);

void vampire_nutrition_per_turn(const item_def &corpse, int feeding = 0);

void finished_eating_message(int food_type);

int you_max_hunger();
int you_min_hunger();

void handle_starvation();

#endif
