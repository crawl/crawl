/*
 *  File:       files.cc
 *  Summary:    Functions used to save and load levels/games.
 *  Written by: Linley Henzell and Alexey Guzeev
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef FILES_H
#define FILES_H

#include "FixAry.h"


// referenced in files - newgame - ouch - overmap:
#define MAX_LEVELS 50
// referenced in files - newgame - ouch - overmap:
#define MAX_BRANCHES 30         // there must be a way this can be extracted from other data


// referenced in files - newgame - ouch:
extern FixedArray<bool, MAX_LEVELS, MAX_BRANCHES> tmp_file_pairs;

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - misc
 * *********************************************************************** */
#if 0
void load( unsigned char stair_taken, bool moving_level,
           bool was_a_labyrinth, char old_level, bool want_followers,
           bool is_new_game, char where_were_you2 );
#endif

void load( unsigned char stair_taken, int load_mode, bool was_a_labyrinth, 
           char old_level, char where_were_you2 );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - libmac - misc
 * *********************************************************************** */
void save_game(bool leave_game);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void restore_game(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: ouch
 * *********************************************************************** */
void save_ghost( bool force = false );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: files hiscores
 * *********************************************************************** */
void make_filename( char *buf, const char *prefix, int level, int where,
                    bool isLabyrinth, bool isGhost );

#endif
