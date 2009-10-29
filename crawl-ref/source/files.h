/*
 *  File:       files.h
 *  Summary:    Functions used to save and load levels/games.
 *  Written by: Linley Henzell and Alexey Guzeev
 */


#ifndef FILES_H
#define FILES_H

#include "externs.h"
#include "fixary.h"
#include "player.h"
#include <stdio.h>
#include <string>
#include <vector>
#include <set>

enum load_mode_type
{
    LOAD_START_GAME,            // game has just begun
    LOAD_RESTART_GAME,          // loaded savefile
    LOAD_ENTER_LEVEL,           // entered a level for the first time
    LOAD_VISITOR                // Visitor pattern to see all levels
};

// referenced in files - newgame - ouch - overmap:
#define MAX_LEVELS 50

// referenced in files - newgame - ouch:
typedef std::set<level_id> level_id_set;
extern level_id_set Generated_Levels;

bool file_exists(const std::string &name);
bool dir_exists(const std::string &dir);
bool is_absolute_path(const std::string &path);
bool is_read_safe_path(const std::string &path);
void assert_read_safe_path(const std::string &path) throw (std::string);

std::vector<std::string> get_dir_files(const std::string &dir);
std::vector<std::string> get_dir_files_ext(const std::string &dir,
                                           const std::string &ext);

std::string datafile_path(std::string basename,
                          bool croak_on_fail = true,
                          bool test_base_path = false);

bool get_dos_compatible_file_name(std::string *fname);
std::string get_parent_directory(const std::string &filename);
std::string get_base_filename(const std::string &filename);
std::string get_path_relative_to(const std::string &referencefile,
                                 const std::string &relativepath);
std::string catpath(const std::string &first, const std::string &second);
std::string canonicalise_file_separator(const std::string &path);

bool check_dir(const std::string &what, std::string &dir, bool silent = false);

bool travel_load_map( branch_type branch, int absdepth );

std::vector<player_save_info> find_saved_characters();

std::string get_savedir();
std::string get_savedir_filename(const std::string &pre,
                                 const std::string &suf,
                                 const std::string &ext,
                                 bool suppress_uid = false);
std::string get_savedir_path(const std::string &shortpath);
std::string get_prefs_filename();
std::string change_file_extension(const std::string &file,
                                  const std::string &ext);

time_t file_modtime(const std::string &file);
bool is_newer(const std::string &a, const std::string &b);
void check_newer(const std::string &target,
                 const std::string &dependency,
                 void (*action)());


bool load( dungeon_feature_type stair_taken, load_mode_type load_mode,
           level_area_type old_level_type, char old_level,
           branch_type where_were_you2 );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - misc
 * *********************************************************************** */
void save_game(bool leave_game, const char *bye = NULL);

// Save game without exiting (used when changing levels).
void save_game_state();

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void restore_game(void);

bool apply_to_all_dungeons(bool (*applicator)());

class level_id;

bool apply_to_level(const level_id &level, bool preserve_current,
                    bool (*applicator)());

bool is_existing_level(const level_id &level);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: ouch
 * *********************************************************************** */
void save_ghost( bool force = false );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: files hiscores
 * *********************************************************************** */
std::string make_filename(std::string prefix, int level, branch_type branch,
                          level_area_type lt, bool isGhost );

FILE *lk_open(const char *mode, const std::string &file);
void lk_close(FILE *handle, const char *mode, const std::string &file);

// file locking stuff
#ifdef USE_FILE_LOCKING
bool lock_file_handle( FILE *handle, int type );
bool unlock_file_handle( FILE *handle );
#endif // USE_FILE_LOCKING

#ifdef SHARED_FILES_CHMOD_PRIVATE
#define DO_CHMOD_PRIVATE(x) chmod( (x), SHARED_FILES_CHMOD_PRIVATE )
#else
#define DO_CHMOD_PRIVATE(x) // empty command
#endif

class file_lock
{
public:
    file_lock(const std::string &filename, const char *mode,
              bool die_on_fail = true);
    ~file_lock();
private:
    FILE *handle;
    const char *mode;
    std::string filename;
};

class SavefileCallback
{
public:
    typedef void (*callback)(bool saving);

    SavefileCallback(callback func);

    static void pre_save();
    static void post_restore();
};

#endif
