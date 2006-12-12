/*
 *  File:       misc.cc
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
#include "travel.h"

// last updated 08jan2001 {gdl}
/* ***********************************************************************
 * called from: ability - decks - fight - it_use2 - spells1
 * *********************************************************************** */
bool go_berserk(bool intentional);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void search_around(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void disarm_trap(struct dist &disa);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - effects - spells3
 * *********************************************************************** */
void down_stairs(bool remove_stairs, int old_level, bool force = false);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool fall_into_a_pool( int entry_x, int entry_y, bool allow_shift, 
                       unsigned char terrain );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - misc
 * *********************************************************************** */
void handle_traps(char trt, int i, bool trap_known);


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
 * called from: misc - mstuff2
 * *********************************************************************** */
void itrap(struct bolt &pbolt, int trapped);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - misc - player - stuff
 * *********************************************************************** */
void new_level(void);

void init_new_level();

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: delay
 * *********************************************************************** */
void turn_corpse_into_chunks( item_def &item );


// last updated 3jun2000 {dlb}
/* ***********************************************************************
 * called from: acr - misc - mstuff2 - spells3
 * *********************************************************************** */
int trap_at_xy(int which_x, int which_y);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void up_stairs(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - effects
 * *********************************************************************** */
void weird_colours(unsigned char coll, char wc[30]);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: it_use3
 * *********************************************************************** */
void weird_writing(char stringy[40]);


// last updated 3jun2000 {dlb}
/* ***********************************************************************
 * called from: acr - misc - mstuff2 - spells2 - spells3
 * *********************************************************************** */
unsigned char trap_category(unsigned char trap_type);

bool grid_is_wall(int grid);
bool grid_is_opaque(int grid);
bool grid_is_solid(int grid);
bool grid_is_solid(int x, int y);
bool grid_is_solid(const coord_def &c);

bool grid_is_water(int grid);
bool grid_is_watery( int grid );
god_type grid_altar_god( unsigned char grid );
int altar_for_god( god_type god );
bool grid_is_branch_stairs( unsigned char grid );
int grid_secret_door_appearance( int gx, int gy );
bool grid_destroys_items( int grid );

const char *grid_item_destruction_message( unsigned char grid );

void curare_hits_player(int agent, int degree);

bool i_feel_safe();

void setup_environment_effects();

// Lava smokes, swamp water mists.
void run_environment_effects();

//////////////////////////////////////////////////////////////////////
// Places and names
//
unsigned short get_packed_place();

unsigned short get_packed_place( unsigned char branch, int subdepth,
                          char level_type );

std::string short_place_name(unsigned short place);
std::string short_place_name(level_id id);
std::string place_name( unsigned short place, bool long_name = false,
                        bool include_number = true );
std::string branch_name( int branch, bool terse );

// Prepositional form of branch level name.  For example, "in the
// Abyss" or "on level 3 of the Main Dungeon".
std::string prep_branch_level_name(unsigned short packed_place);
std::string prep_branch_level_name();

// Get displayable depth in the current branch, given the absolute
// depth.
int subdungeon_depth(unsigned char branch, int depth);

// Get absolute depth given the displayable depth in the branch.
int absdungeon_depth(unsigned char branch, int subdepth);
//////////////////////////////////////////////////////////////////////

#endif
