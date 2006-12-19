/*
 *  File:       files.cc
 *  Summary:    Functions used to save and load levels/games.
 *  Written by: Linley Henzell and Alexey Guzeev
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef FILES_H
#define FILES_H

#include "externs.h"
#include "FixAry.h"
#include <stdio.h>
#include <string>
#include <vector>

// referenced in files - newgame - ouch - overmap:
#define MAX_LEVELS 50
// referenced in files - newgame - ouch - overmap:
#define MAX_BRANCHES 30         // there must be a way this can be extracted from other data


// referenced in files - newgame - ouch:
extern FixedArray<bool, MAX_LEVELS, MAX_BRANCHES> tmp_file_pairs;

std::string datafile_path(const std::string &basename);

bool check_dir(const std::string &what, std::string &dir);

bool travel_load_map( char branch, int absdepth );

std::vector<player> find_saved_characters();

std::string get_savedir();
std::string get_savedir_filename(const char *pre, const char *suf,
                                 const char *ext, bool suppress_uid = false);

std::string get_prefs_filename();

void load( unsigned char stair_taken, int load_mode, bool was_a_labyrinth, 
           char old_level, char where_were_you2 );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - misc
 * *********************************************************************** */
void save_game(bool leave_game);

// Save game without exiting (used when changing levels).
void save_game_state();

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
std::string make_filename( const char *prefix, int level, int where,
                           bool isLabyrinth, bool isGhost );

void writeShort(FILE *file, short s);

short readShort(FILE *file);

void writeByte(FILE *file, unsigned char byte);

unsigned char readByte(FILE *file);

void writeString(FILE* file, const std::string &s);

std::string readString(FILE *file);

void writeLong(FILE* file, long num);

long readLong(FILE *file);

#endif
