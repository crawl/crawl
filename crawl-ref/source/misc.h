/*
 *  File:       misc.h
 *  Summary:    Misc functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <2>     april2009      Cha             runes_in_pack now
                                                         declared here
 *               <1>     -/--/--        LRH             Created
 */

#ifndef MISC_H
#define MISC_H

#include "externs.h"

struct bolt;
struct dist;
struct activity_interrupt_data;

// last updated 08jan2001 {gdl}
/* ***********************************************************************
 * called from: ability - decks - fight - it_use2 - spells1
 * *********************************************************************** */
bool go_berserk(bool intentional);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void search_around( bool only_adjacent = false );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - effects - spells3
 * *********************************************************************** */
void down_stairs(int old_level,
                 dungeon_feature_type force_stair = DNGN_UNSEEN,
                 entry_cause_type entry_cause = EC_UNKNOWN);

// Created Sept 1, 2000 -- bwr
/* ***********************************************************************
 * called from: acr misc
 * *********************************************************************** */
void merfolk_start_swimming(void);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - misc - player - stuff
 * *********************************************************************** */
void new_level(void);

void trackers_init_new_level(bool transit);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: delay
 * *********************************************************************** */
void turn_corpse_into_skeleton(item_def &corpse, int time = 200);
void turn_corpse_into_chunks( item_def &item );

void init_stack_blood_potions( item_def &stack, int age = -1 );
void maybe_coagulate_blood_potions_floor( int obj );
bool maybe_coagulate_blood_potions_inv( item_def &blood );
long remove_oldest_blood_potion( item_def &stack );
void remove_newest_blood_potion( item_def &stack, int quant = -1 );
void drop_blood_potions_stack( item_def &stack, int quant, int x = you.x_pos,
                               int y = you.y_pos );
void pick_up_blood_potions_stack( item_def &stack, int quant );

bool can_bottle_blood_from_corpse( int mons_type );
void turn_corpse_into_blood_potions ( item_def &item );
void split_potions_into_decay( int obj, int amount, bool need_msg = true );

bool victim_can_bleed(int montype);
void bleed_onto_floor(int x, int y, int mon, int damage, bool spatter = false);
void generate_random_blood_spatter_on_level();

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool check_annotation_exclusion_warning();
void up_stairs(dungeon_feature_type force_stair = DNGN_UNSEEN,
               entry_cause_type entry_cause = EC_UNKNOWN);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - effects
 * *********************************************************************** */
std::string weird_glowing_colour();

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: it_use3
 * *********************************************************************** */
std::string weird_writing();

std::string weird_smell();

std::string weird_sound();

void curare_hits_player(int agent, int degree);

bool mons_is_safe(const monsters *mon, bool want_move = false);

void get_playervisible_monsters(std::vector<monsters*>& mons,
                                bool want_move = false, bool just_check = false,
                                bool dangerous_only = false, int range = -1);

bool i_feel_safe(bool announce = false, bool want_move = false,
                 bool just_monsters = false, int range = -1);

bool there_are_monsters_nearby();

void setup_environment_effects();

// Lava smokes, swamp water mists.
void run_environment_effects();

int str_to_shoptype(const std::string &s);

bool player_in_a_dangerous_place(bool *invis = NULL);

coord_def pick_adjacent_free_square(const coord_def& p);

int speed_to_duration(int speed);

bool scramble(void);

// // this used to be static 
int runes_in_pack();

bool interrupt_cmd_repeat( activity_interrupt_type ai,
                           const activity_interrupt_data &at );

void reveal_secret_door(int x, int y);

std::string your_hand(bool plural);

bool stop_attack_prompt(const monsters *mon, bool beam_attack,
                        bool beam_target);

#endif
