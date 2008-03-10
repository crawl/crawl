/*
 *  File:       misc.h
 *  Summary:    Misc functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
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

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void in_a_cloud(void);

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

std::string level_description_string();

void trackers_init_new_level(bool transit);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: delay
 * *********************************************************************** */
void turn_corpse_into_chunks( item_def &item );

void bleed_onto_floor(int x, int y, int mon, int damage, bool spatter = false);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void up_stairs(dungeon_feature_type force_stair = DNGN_UNSEEN,
               entry_cause_type entry_cause = EC_UNKNOWN);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - effects
 * *********************************************************************** */
std::string weird_glow_colour();


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: it_use3
 * *********************************************************************** */
std::string weird_writing();


std::string cloud_name(cloud_type type);
bool is_damaging_cloud(cloud_type type);

void curare_hits_player(int agent, int degree);

bool mons_is_safe(const monsters *mon, bool want_move = false);
bool i_feel_safe(bool announce = false, bool want_move = false);

void setup_environment_effects();

// Lava smokes, swamp water mists.
void run_environment_effects();

//////////////////////////////////////////////////////////////////////

int str_to_shoptype(const std::string &s);

bool do_autopray();

bool player_in_a_dangerous_place();

coord_def pick_adjacent_free_square(int x, int y);

int speed_to_duration(int speed);

bool scramble(void);

bool interrupt_cmd_repeat( activity_interrupt_type ai, 
                           const activity_interrupt_data &at );

void reveal_secret_door(int x, int y);

#endif
