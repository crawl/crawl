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
void search_around( bool only_adjacent = false );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void disarm_trap(struct dist &disa);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - effects - spells3
 * *********************************************************************** */
void down_stairs(int old_level, dungeon_feature_type force_stair = DNGN_UNSEEN);


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

std::string level_description_string();

void trackers_init_new_level(bool transit);

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
std::string weird_colours(unsigned char coll);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: it_use3
 * *********************************************************************** */
std::string weird_writing();


// last updated 3jun2000 {dlb}
/* ***********************************************************************
 * called from: acr - misc - mstuff2 - spells2 - spells3
 * *********************************************************************** */
dungeon_feature_type trap_category(trap_type type);

int trap_at_xy(int x, int y);
trap_type trap_type_at_xy(int x, int y);

bool grid_is_wall(dungeon_feature_type grid);
bool grid_is_opaque(dungeon_feature_type grid);
bool grid_is_solid(dungeon_feature_type grid);
bool grid_is_stone_stair(dungeon_feature_type grid);
bool grid_is_rock_stair(dungeon_feature_type grid);
bool grid_is_solid(int x, int y);
bool grid_is_solid(const coord_def &c);
bool grid_is_trap(dungeon_feature_type grid);
command_type grid_stair_direction(dungeon_feature_type grid);
bool grid_sealable_portal(dungeon_feature_type grid);

bool grid_is_water(dungeon_feature_type grid);
bool grid_is_watery(dungeon_feature_type grid);
god_type grid_altar_god( dungeon_feature_type grid );
dungeon_feature_type altar_for_god( god_type god );
bool grid_is_branch_stairs( dungeon_feature_type grid );
int grid_secret_door_appearance( int gx, int gy );
bool grid_destroys_items( dungeon_feature_type grid );

bool level_type_exits_up(level_area_type type);
bool level_type_exits_down(level_area_type type);

std::string cloud_name(cloud_type type);
bool is_damaging_cloud(cloud_type type);

const char *grid_item_destruction_message( dungeon_feature_type grid );

void curare_hits_player(int agent, int degree);

bool i_feel_safe(bool announce = false);

void setup_environment_effects();

// Lava smokes, swamp water mists.
void run_environment_effects();

// Terrain changed under 'pos', perform necessary effects.
void dungeon_terrain_changed(const coord_def &pos,
                             dungeon_feature_type feat = DNGN_UNSEEN,
                             bool affect_player = true);

//////////////////////////////////////////////////////////////////////
// Places and names
//
unsigned short get_packed_place();

unsigned short get_packed_place( branch_type branch, int subdepth,
                                 level_area_type level_type );

int place_branch(unsigned short place);
int place_depth(unsigned short place);
std::string short_place_name(unsigned short place);
std::string short_place_name(level_id id);
std::string place_name( unsigned short place, bool long_name = false,
                        bool include_number = true );

// Prepositional form of branch level name.  For example, "in the
// Abyss" or "on level 3 of the Main Dungeon".
std::string prep_branch_level_name(unsigned short packed_place);
std::string prep_branch_level_name();

// Get displayable depth in the current branch, given the absolute
// depth.
int subdungeon_depth(branch_type branch, int depth);

// Get absolute depth given the displayable depth in the branch.
int absdungeon_depth(branch_type branch, int subdepth);

// Get displayable depth in the current branch.
int player_branch_depth();

bool single_level_branch(branch_type branch);
//////////////////////////////////////////////////////////////////////

// Set floor/wall colour based on the mons_alloc array. Used for
// Abyss and Pan.
void set_colours_from_monsters();

int str_to_shoptype(const std::string &s);

bool do_autopray();

bool player_in_a_dangerous_place();

coord_def pick_adjacent_free_square(int x, int y);

int speed_to_duration(int speed);

#endif
