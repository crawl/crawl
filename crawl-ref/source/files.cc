/*
 *  File:       files.cc
 *  Summary:    Functions used to save and load levels/games.
 *  Written by: Linley Henzell and Alexey Guzeev
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *   <7>   19 June 2000  GDL    Change handle to FILE *
 *   <6>   11/14/99      cdl    Don't let player ghosts follow you up/down
 *   <5>    7/13/99      BWR    Monsters now regenerate hps off level &
 ghosts teleport
 *   <4>    6/13/99      BWR    Added tmp file pairs to save file.
 *   <3>    6/11/99      DML    Replaced temp file deletion code.
 *
 *   <2>    5/12/99      BWR    Multiuser system support,
 *                                        including appending UID to
 *                                        name, and compressed saves
 *                                        in the SAVE_DIR_PATH directory
 *
 *   <1>   --/--/--      LRH    Created
 */

#include "AppHdr.h"
#include "files.h"
#include "version.h"

#include <string.h>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <algorithm>

#ifdef DOS
#include <conio.h>
#include <file.h>
#endif

#ifdef UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef __MINGW32__
#include <io.h>
#include <sys/types.h>
#endif

#include <dirent.h>

#include "externs.h"

#include "chardump.h"
#include "cloud.h"
#include "clua.h"
#include "debug.h"
#include "dungeon.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "message.h"
#include "misc.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mstuff2.h"
#include "mtransit.h"
#include "notes.h"
#include "player.h"
#include "randart.h"
#include "skills2.h"
#include "stash.h"
#include "stuff.h"
#include "tags.h"
#include "travel.h"
#include "tutorial.h"

void save_level(int level_saved, level_area_type lt,
                branch_type where_were_you);

#define GHOST_MINOR_VERSION 1
#define LEVEL_MINOR_VERSION 1
#define YOU_MINOR_VERSION   1

static void redraw_all(void)
{
    you.redraw_hit_points = 1;
    you.redraw_magic_points = 1;
    you.redraw_strength = 1;
    you.redraw_intelligence = 1;
    you.redraw_dexterity = 1;
    you.redraw_armour_class = 1;
    you.redraw_evasion = 1;
    you.redraw_experience = 1;
    you.redraw_gold = 1;

    you.redraw_status_flags = REDRAW_LINE_1_MASK | REDRAW_LINE_2_MASK | REDRAW_LINE_3_MASK;
}

static bool determine_version( FILE *restoreFile, 
                               char &majorVersion, char &minorVersion );

static void restore_version( FILE *restoreFile, 
                             char majorVersion, char minorVersion );

static bool determine_level_version( FILE *levelFile, 
                                     char &majorVersion, char &minorVersion );

static void restore_level_version( FILE *levelFile, 
                                   char majorVersion, char minorVersion );

static bool determine_ghost_version( FILE *ghostFile, 
                                     char &majorVersion, char &minorVersion );

static void restore_ghost_version( FILE *ghostFile, 
                                   char majorVersion, char minorVersion );

static void restore_tagged_file( FILE *restoreFile, int fileType, 
                                 char minorVersion );

static void load_ghost();

static std::string uid_as_string()
{
#ifdef MULTIUSER
    char struid[20];
    snprintf( struid, sizeof struid, "-%d", static_cast<int>(getuid()) );
    return std::string(struid);
#else
    return std::string();
#endif
}

static bool is_uid_file(const std::string &name, const std::string &ext)
{
    std::string save_suffix = get_savedir_filename("", "", "");
    save_suffix += ext;
#ifdef DOS
    // Grumble grumble. Hang all retarded operating systems.
    uppercase(save_suffix);
#endif

    save_suffix = save_suffix.substr(Options.save_dir.length());

    std::string::size_type suffix_pos = name.find(save_suffix);
    return (suffix_pos != std::string::npos 
            && suffix_pos == name.length() - save_suffix.length()
            && suffix_pos != 0);
}

bool is_save_file_name(const std::string &name)
{
    return is_uid_file(name, ".sav");
}

#ifdef LOAD_UNPACKAGE_CMD
bool is_packed_save(const std::string &name)
{
    return is_uid_file(name, PACKAGE_SUFFIX);
}
#endif

// Returns a full player struct read from the save.
player read_character_info(const std::string &savefile)
{
    player fromfile;
    player backup = you;

    FILE *charf = fopen(savefile.c_str(), "rb");
    if (!charf)
        return fromfile;

    char majorVersion = 0;
    char minorVersion = 0;

    if (determine_version(charf, majorVersion, minorVersion)
        && majorVersion == SAVE_MAJOR_VERSION)
    {
        restore_tagged_file(charf, TAGTYPE_PLAYER_NAME, minorVersion);
        fromfile = you;
        you      = backup;
    }

    fclose(charf);
    return fromfile;
}

// Returns the names of all files in the given directory. Note that the
// filenames returned are relative to the directory.
std::vector<std::string> get_dir_files(const std::string &dirname)
{
    std::vector<std::string> files;

    DIR *dir = opendir(dirname.c_str());
    if (!dir)
        return (files);

    while (dirent *entry = readdir(dir))
    {
        std::string name = entry->d_name;
        if (name == "." || name == "..")
            continue;

        files.push_back(name);
    }
    closedir(dir);

    return (files);
}

std::string get_parent_directory(const std::string &filename)
{
    std::string::size_type pos = filename.rfind(FILE_SEPARATOR);
    if (pos != std::string::npos)
        return filename.substr(0, pos + 1);
#ifdef ALT_FILE_SEPARATOR
    pos = filename.rfind(ALT_FILE_SEPARATOR);
    if (pos != std::string::npos)
        return filename.substr(0, pos + 1);
#endif
    return ("");
}

static bool file_exists(const std::string &name)
{
    FILE *f = fopen(name.c_str(), "r");
    const bool exists = !!f;
    if (f)
        fclose(f);
    return (exists);
}

// Low-tech existence check.
static bool dir_exists(const std::string &dir)
{
    DIR *d = opendir(dir.c_str());
    const bool exists = !!d;
    if (d)
        closedir(d);

    return (exists);
}

static int create_directory(const char *dir)
{
#if defined(MULTIUSER)
    return mkdir(dir, SHARED_FILES_CHMOD_PUBLIC | 0111);
#elif defined(DOS)
    // djgpp doesn't seem to have mkdir.
    return (-1);
#else
    return mkdir(dir);
#endif
}

static bool create_dirs(const std::string &dir)
{
    std::string sep = " ";
    sep[0] = FILE_SEPARATOR;
    std::vector<std::string> segments = 
        split_string(
                sep.c_str(),
                dir,
                false,
                false);

    std::string path;
    for (int i = 0, size = segments.size(); i < size; ++i)
    {
        path += segments[i];
        path += FILE_SEPARATOR;

        // Handle absolute paths correctly.
        if (i == 0 && dir.size() && dir[0] == FILE_SEPARATOR)
            path = FILE_SEPARATOR + path;

        if (!dir_exists(path) && create_directory(path.c_str()))
            return (false);
    }
    return (true);
}

std::string datafile_path(const std::string &basename,
                          bool croak_on_fail,
                          bool test_base_path)
{
    if (test_base_path && file_exists(basename))
        return (basename);
    
    std::string cdir = !SysEnv.crawl_dir.empty()? SysEnv.crawl_dir : "";

    const std::string rawbases[] = {
#ifdef DATA_DIR_PATH
        DATA_DIR_PATH,
#else
        cdir,
#endif
    };

    const std::string prefixes[] = {
        std::string("dat") + FILE_SEPARATOR,
        std::string("docs") + FILE_SEPARATOR,
        std::string("..")+FILE_SEPARATOR+std::string("docs")+FILE_SEPARATOR,
        std::string("..") + FILE_SEPARATOR,
        std::string(".") + FILE_SEPARATOR,
        "",
    };

    std::vector<std::string> bases;
    for (unsigned i = 0; i < sizeof(rawbases) / sizeof(*rawbases); ++i)
    {
        std::string base = rawbases[i];
        if (base.empty())
            continue;

        if (base[base.length() - 1] != FILE_SEPARATOR)
            base += FILE_SEPARATOR;
        bases.push_back(base);
    }

#ifndef DATA_DIR_PATH
    if (!SysEnv.crawl_base.empty())
        bases.push_back(SysEnv.crawl_base);
    bases.push_back("");
#endif

    for (unsigned b = 0, size = bases.size(); b < size; ++b)
    {
        for (unsigned p = 0; p < sizeof(prefixes) / sizeof(*prefixes); ++p)
        {
            std::string name = bases[b] + prefixes[p] + basename;
            if (file_exists(name))
                return (name);
        }
    }

    // Die horribly.
    if (croak_on_fail)
        end(1, false, "Cannot find data file '%s' anywhere, aborting\n", 
            basename.c_str());

    return ("");
}

bool check_dir(const std::string &whatdir, std::string &dir)
{
    if (dir.empty())
        return (true);

    std::string sep = " ";
    sep[0] = FILE_SEPARATOR;

    dir = replace_all_of(dir, "/", sep);
    dir = replace_all_of(dir, "\\", sep);

    // Suffix the separator if necessary
    if (dir[dir.length() - 1] != FILE_SEPARATOR)
        dir += FILE_SEPARATOR;

    if (!dir_exists(dir) && !create_dirs(dir))
    {
        fprintf(stderr, "%s \"%s\" does not exist "
                        "and I can't create it.\n",
                whatdir.c_str(), dir.c_str());
        return (false);
    }

    return (true);
}

// Given a simple (relative) name of a save file, returns the full path of 
// the file in the Crawl saves directory.
std::string get_savedir_path(const std::string &shortpath)
{
    return (Options.save_dir + shortpath);
}

/*
 * Returns a list of the names of characters that are already saved for the
 * current user.
 */
std::vector<player> find_saved_characters()
{
    std::vector<player> chars;
#ifndef DISABLE_SAVEGAME_LISTS
    std::string searchpath = Options.save_dir;

    if (searchpath.empty())
        searchpath = ".";

    std::vector<std::string> allfiles = get_dir_files(searchpath);
    for (int i = 0, size = allfiles.size(); i < size; ++i)
    {
        std::string filename = allfiles[i];
#ifdef LOAD_UNPACKAGE_CMD
        if (!is_packed_save(filename))
            continue;

        std::string basename = 
            filename.substr(
                    0,
                    filename.length() - strlen(PACKAGE_SUFFIX));

        const std::string zipname = get_savedir_path(basename);

        // This is the filename we actually read ourselves.
        filename = basename + ".sav";

        const std::string dir = get_savedir();

        char cmd_buff[1024];
        snprintf( cmd_buff, sizeof(cmd_buff), UNPACK_SPECIFIC_FILE_CMD,
                  zipname.c_str(),
                  dir.c_str(),
                  filename.c_str() );

        if (system(cmd_buff) != 0)
            continue;
#endif
        if (is_save_file_name(filename))
        {
            player p = read_character_info(get_savedir_path(filename));
            if (p.is_valid())
                chars.push_back(p);
        }

#ifdef LOAD_UNPACKAGE_CMD
        // If we unpacked the .sav file, throw it away now.
        unlink( get_savedir_path(filename).c_str() );
#endif
    }

    std::sort(chars.begin(), chars.end());
#endif // !DISABLE_SAVEGAME_LISTS
    return (chars);
}

std::string get_savedir()
{
    const std::string &dir = Options.save_dir;
    return (dir.empty()? "." : dir);
}

std::string get_savedir_filename(const std::string &prefix,
                                 const std::string &suffix, 
                                 const std::string &extension,
                                 bool suppress_uid)
{
    std::string result = Options.save_dir;

    // Shorten string as appropriate
    result += strip_filename_unsafe_chars(prefix).substr(0, kFileNameLen);

    // Technically we should shorten the string first.  But if
    // MULTIUSER is set we'll have long filenames anyway. Caveat
    // emptor.
    if ( !suppress_uid )
        result += uid_as_string();

    result += suffix;

    if (!extension.empty())
    {
        result += '.';
        result += extension;
    }

#ifdef DOS
    uppercase(result);
#endif
    return result;
}

std::string get_prefs_filename()
{
    return get_savedir_filename("start", "ns", "prf");
}

static std::string get_level_suffix(int level, branch_type where,
                                    level_area_type lt)
{
    switch (lt)
    {
    default:
    case LEVEL_DUNGEON:
        return (make_stringf("%02d%c", subdungeon_depth(where, level),
                             where + 'a'));
    case LEVEL_LABYRINTH:
        return ("lab");
    case LEVEL_ABYSS:
        return ("abs");
    case LEVEL_PANDEMONIUM:
        return ("pan");
    }
}

std::string make_filename( const char *prefix, int level, branch_type where,
                           level_area_type ltype, bool isGhost )
{
    return get_savedir_filename( prefix, "",
                                 get_level_suffix(level, where, ltype),
                                 isGhost );
}

static void write_tagged_file( FILE *dataFile, char majorVersion,
                               char minorVersion, int fileType )
{
    struct tagHeader th;

    // find all relevant tags
    char tags[NUM_TAGS];
    tag_set_expected(tags, fileType);

    // write version
    struct tagHeader versionTag;
    versionTag.offset = 0;
    versionTag.tagID = TAG_VERSION;
    marshallByte(versionTag, majorVersion);
    marshallByte(versionTag, minorVersion);
    tag_write(versionTag, dataFile);

    // all other tags
    for(int i=1; i<NUM_TAGS; i++)
    {
        if (tags[i] == 1)
        {
            tag_construct(th, i);
            tag_write(th, dataFile);
        }
    }
}

bool travel_load_map( branch_type branch, int absdepth )
{
    // Try to open level savefile.
    FILE *levelFile = fopen(make_filename(you.your_name, absdepth, branch,
                                          LEVEL_DUNGEON, false).c_str(), "rb");
    if (!levelFile)
        return false;

    // BEGIN -- must load the old level : pre-load tasks

    // LOAD various tags
    char majorVersion;
    char minorVersion;

    if (!determine_level_version( levelFile, majorVersion, minorVersion )
            || majorVersion != SAVE_MAJOR_VERSION)
    {
        fclose(levelFile);
        return false;
    }
    
    tag_read(levelFile, minorVersion);

    fclose( levelFile );
    
    return true;
}

static coord_def find_nearby_stair(int stair_to_find, bool find_closest)
{
    // scan around the player's position first
    int basex = you.x_pos;
    int basey = you.y_pos;

    // check for illegal starting point
    if ( !in_bounds(basex, basey) )
    {
        basex = 0;
        basey = 0;
    }

    coord_def result;

    int found = 0;
    int best_dist = 1 + GXM*GXM + GYM*GYM;

    // XXX These passes should be rewritten to use an iterator of STL
    // algorithm of some kind.

    // First pass: look for an exact match
    for (int xcode = 0; xcode < GXM; ++xcode )
    {
        const int xsign = ((xcode % 2) ? 1 : -1);
        const int xdiff = xsign * (xcode + 1)/2;
        const int xpos  = (basex + xdiff + GXM) % GXM;

        for (int ycode = 0; ycode < GYM; ++ycode)
        {
            const int ysign = ((ycode % 2) ? 1 : -1);
            const int ydiff = ysign * (ycode + 1)/2;
            const int ypos  = (basey + ydiff + GYM) % GYM;

            // note that due to the wrapping above, we can't just use
            // xdiff*xdiff + ydiff*ydiff
            const int dist = (xpos-basex)*(xpos-basex) +
                (ypos-basey)*(ypos-basey);

            if (grd[xpos][ypos] == stair_to_find)
            {
                found++;
                if (find_closest)
                {
                    if (dist < best_dist)
                    {
                        best_dist = dist;
                        result.x = xpos;
                        result.y = ypos;
                    }
                }
                else if (one_chance_in( found ))
                {
                    result.x = xpos;
                    result.y = ypos;
                }
            }
        }
    }

    if ( found )
        return result;

    best_dist = 1 + GXM*GXM + GYM*GYM;

    // Second pass: find a staircase in the proper direction
    for (int xcode = 0; xcode < GXM; ++xcode )
    {
        const int xsign = ((xcode % 2) ? 1 : -1);
        const int xdiff = xsign * (xcode + 1)/2;
        const int xpos  = (basex + xdiff + GXM) % GXM;

        for (int ycode = 0; ycode < GYM; ++ycode)
        {
            const int ysign = ((ycode % 2) ? 1 : -1);
            const int ydiff = ysign * (ycode + 1)/2;
            const int ypos  = (basey + ydiff + GYM) % GYM;

            bool good_stair;
            const int looking_at = grd[xpos][ypos];

            if (stair_to_find <= DNGN_ROCK_STAIRS_DOWN )
                good_stair =
                    (looking_at >= DNGN_STONE_STAIRS_DOWN_I) &&
                    (looking_at <= DNGN_ROCK_STAIRS_DOWN);
            else
                good_stair =
                    (looking_at >= DNGN_STONE_STAIRS_UP_I) &&
                    (looking_at <= DNGN_ROCK_STAIRS_UP);

            const int dist = (xpos-basex)*(xpos-basex) +
                (ypos-basey)*(ypos-basey);

            if ( good_stair )
            {
                found++;
                if (find_closest && dist < best_dist)
                {
                    best_dist = dist;
                    result.x = xpos;
                    result.y = ypos;
                }
                else if (one_chance_in( found ))
                {
                    result.x = xpos;
                    result.y = ypos;
                }
            }
        }
    }

    if ( found )
        return result;

    // Third pass: look for any clear terrain (shouldn't happen).
    // We abandon the idea of looking nearby now.
    for (int xpos = 0; xpos < GXM; xpos++)
    {
        for (int ypos = 0; ypos < GYM; ypos++)
        {
            if (grd[xpos][ypos] >= DNGN_FLOOR)
            {
                found++;
                if (one_chance_in( found ))
                {
                    result.x = xpos;
                    result.y = ypos;
                }
            }
        }
    }

    ASSERT( found );
    return result;
}

static void sanity_test_monster_inventory()
{
    // Sanity forcing of monster inventory items (required?)
    for (int i = 0; i < MAX_MONSTERS; i++)
    {
        if (menv[i].type == -1)
            continue;

        for (int j = 0; j < NUM_MONSTER_SLOTS; j++)
        {
            if (menv[i].inv[j] == NON_ITEM)
                continue;

            // items carried by monsters shouldn't be linked
            if (mitm[menv[i].inv[j]].link != NON_ITEM)
                mitm[menv[i].inv[j]].link = NON_ITEM;
        }
    }
}

static void fixup_pandemonium_stairs()
{
    for (int i = 0; i < GXM; i++)
    {
        for (int j = 0; j < GYM; j++)
        {
            if (grd[i][j] >= DNGN_STONE_STAIRS_UP_I
                && grd[i][j] <= DNGN_ROCK_STAIRS_UP)
            {
                if (one_chance_in( you.mutation[MUT_PANDEMONIUM] ? 5 : 50 ))
                    grd[i][j] = DNGN_EXIT_PANDEMONIUM;
                else
                    grd[i][j] = DNGN_FLOOR;
            }
            
            if (grd[i][j] >= DNGN_ENTER_LABYRINTH
                && grd[i][j] <= DNGN_ROCK_STAIRS_DOWN)
            {
                grd[i][j] = DNGN_TRANSIT_PANDEMONIUM;
            }
        }
    }
}

static void place_player_on_stair(branch_type old_branch, int stair_taken)
{
    bool find_first = true;

    // Order is important here:
    if (you.level_type == LEVEL_DUNGEON 
        && old_branch == BRANCH_VESTIBULE_OF_HELL
        && stair_taken == DNGN_STONE_STAIRS_UP_I)
    {
        // leaving hell - look for entry portal first
        stair_taken = DNGN_ENTER_HELL;
        find_first = false;
    }
    else if (stair_taken == DNGN_EXIT_PANDEMONIUM)
    {
        stair_taken = DNGN_ENTER_PANDEMONIUM;
        find_first = false;
    }
    else if (stair_taken == DNGN_EXIT_ABYSS)
    {
        stair_taken = DNGN_ENTER_ABYSS;
        find_first = false;
    }
    else if (stair_taken == DNGN_ENTER_HELL 
             || stair_taken == DNGN_ENTER_LABYRINTH)
    {
        // the vestibule and labyrith always start from this stair
        stair_taken = DNGN_STONE_STAIRS_UP_I;
    }
    else if (stair_taken >= DNGN_STONE_STAIRS_DOWN_I 
             && stair_taken <= DNGN_ROCK_STAIRS_DOWN)
    {
        // look for coresponding up stair
        stair_taken += (DNGN_STONE_STAIRS_UP_I - DNGN_STONE_STAIRS_DOWN_I);
    }
    else if (stair_taken >= DNGN_STONE_STAIRS_UP_I 
             && stair_taken <= DNGN_ROCK_STAIRS_UP)
    {
        // look for coresponding down stair
        stair_taken += (DNGN_STONE_STAIRS_DOWN_I - DNGN_STONE_STAIRS_UP_I);
    }
    else if (stair_taken >= DNGN_RETURN_FROM_ORCISH_MINES 
             && stair_taken < 150) // 20 slots reserved
    {
            // find entry point to subdungeon when leaving
        stair_taken += (DNGN_ENTER_ORCISH_MINES - DNGN_RETURN_FROM_ORCISH_MINES);
    }
    else if (stair_taken >= DNGN_ENTER_ORCISH_MINES
             && stair_taken < DNGN_RETURN_FROM_ORCISH_MINES)
    {
        // find exit staircase from subdungeon when entering 
        stair_taken += (DNGN_RETURN_FROM_ORCISH_MINES - DNGN_ENTER_ORCISH_MINES);
    }
    else if (stair_taken >= DNGN_ENTER_DIS 
             && stair_taken <= DNGN_TRANSIT_PANDEMONIUM)
    {
        // when entering a hell or pandemonium
        stair_taken = DNGN_STONE_STAIRS_UP_I;
    }
    else // Note: stair_taken can equal things like DNGN_FLOOR
    {
        // just find a nice empty square
        stair_taken = DNGN_FLOOR;
        find_first = false;
    }
    
    const coord_def where_to_go = find_nearby_stair(stair_taken, find_first);
    you.x_pos = where_to_go.x;
    you.y_pos = where_to_go.y;
}

static void close_level_gates()
{
    for ( int i = 0; i < GXM; ++i )
    {
        for ( int j = 0; j < GYM; ++j )
        {
            if (you.char_direction == DIR_ASCENDING
                && you.level_type != LEVEL_PANDEMONIUM)
            {
                if (grd[i][j] == DNGN_ENTER_HELL
                    || grd[i][j] == DNGN_ENTER_ABYSS
                    || grd[i][j] == DNGN_ENTER_PANDEMONIUM)
                {
                    grd[i][j] = DNGN_STONE_ARCH;
                }
            }
        }
    }
}

static void clear_env_map()
{
    for (int i = 0; i < GXM; i++)
    {
        for (int j = 0; j < GYM; j++)
        {
            env.map[i][j] = 0;
            env.map_col[i][j].clear();
        }
    }
}

static void clear_clouds()
{
    for (int clouty = 0; clouty < MAX_CLOUDS; ++clouty)
        delete_cloud( clouty );
    for (int i = 0; i < GXM; i++)
        for (int j = 0; j < GYM; j++)
            env.cgrid[i][j] = EMPTY_CLOUD;
}

static void grab_followers(std::vector<follower>& followers)
{
    for (int i = you.x_pos - 1; i < you.x_pos + 2; i++)
    {
        for (int j = you.y_pos - 1; j < you.y_pos + 2; j++)
        {
            if (i == you.x_pos && j == you.y_pos)
                continue;

            if (mgrd[i][j] == NON_MONSTER)
                continue;

            monsters *fmenv = &menv[mgrd[i][j]];

            if (fmenv->type == MONS_PLAYER_GHOST
                && fmenv->hit_points < fmenv->max_hit_points / 2)
            {
                mpr("The ghost fades into the shadows.");
                monster_teleport(fmenv, true);
                continue;
            }

            // monster has to be already tagged in order to follow:
            if (!testbits( fmenv->flags, MF_TAKING_STAIRS ))
                continue;

#if DEBUG_DIAGNOSTICS
            mprf( "%s is following.", str_monam(*fmenv, DESC_CAP_THE).c_str());
#endif

            follower f(*fmenv);
            followers.push_back(f);
            fmenv->destroy_inventory();
            monster_cleanup(fmenv);
        }
    }
}

bool load( int stair_taken, load_mode_type load_mode, bool was_a_labyrinth,
           int old_level, branch_type old_branch )
{
    std::vector<follower> followers;

    bool just_created_level = false;

    std::string cha_fil = make_filename( you.your_name, you.your_level,
                                         you.where_are_you,
                                         you.level_type,
                                         false );

    if (you.level_type == LEVEL_DUNGEON)
    {
        if (tmp_file_pairs[you.your_level][you.where_are_you] == false)
        {
            // make sure old file is gone
            unlink(cha_fil.c_str());

            // save the information for later deletion -- DML 6/11/99
            tmp_file_pairs[you.your_level][you.where_are_you] = true;
        }
    }

    you.prev_targ = MHITNOT;

    // We clear twice - on save and on load.
    // Once would be enough...
    if (load_mode != LOAD_RESTART_GAME)
        clear_clouds();

    // This block is to grab followers and save the old level to disk.
    if (load_mode == LOAD_ENTER_LEVEL)
    {
        grab_followers(followers);

        if (!was_a_labyrinth)
            save_level( old_level, LEVEL_DUNGEON, old_branch );

        was_a_labyrinth = false;
    }

    // Try to open level savefile.
    FILE *levelFile = fopen(cha_fil.c_str(), "rb");

    // GENERATE new level when the file can't be opened:
    if (levelFile == NULL)
    {
        builder( you.your_level, you.level_type );
        just_created_level = true;

        if ((you.your_level > 1 || you.level_type != LEVEL_DUNGEON)
            && one_chance_in(3)
            && you.level_type != LEVEL_LABYRINTH)
        {
            load_ghost();       // no ghosts in Labyrinth
        }
        env.turns_on_level = 0;
    }
    else
    {
        // BEGIN -- must load the old level : pre-load tasks

        // LOAD various tags
        char majorVersion = 0;
        char minorVersion = 0;

        if (!determine_level_version( levelFile, majorVersion, minorVersion ))
        {
            end(-1, false, "\nLevel file appears to be invalid.\n");
        }

        restore_level_version( levelFile, majorVersion, minorVersion );

        // sanity check - EOF
        if (!feof( levelFile ))
        {
            end(-1, false, "\nIncomplete read of \"%s\" - aborting.\n",
                cha_fil.c_str());
        }

        fclose( levelFile );

        // POST-LOAD tasks :
        link_items();
        redraw_all();
    }

    // closes all the gates if you're on the way out
    if (you.char_direction == DIR_ASCENDING &&
        you.level_type != LEVEL_PANDEMONIUM)
        close_level_gates();

    if (just_created_level)
        clear_env_map();

    // Here's the second cloud clearing, on load (see above)
    if (load_mode != LOAD_RESTART_GAME)
        clear_clouds();

    if (load_mode != LOAD_RESTART_GAME && you.level_type != LEVEL_ABYSS)
    {
        place_player_on_stair(old_branch, stair_taken);
    }
    else if (load_mode != LOAD_RESTART_GAME && you.level_type == LEVEL_ABYSS)
    {
        you.x_pos = 45;
        you.y_pos = 35;
    }

    // This should fix the "monster occuring under the player" bug?
    if (mgrd[you.x_pos][you.y_pos] != NON_MONSTER)
        monster_teleport(&menv[mgrd[you.x_pos][you.y_pos]], true);

    // actually "move" the followers if applicable
    if ((you.level_type == LEVEL_DUNGEON
         || you.level_type == LEVEL_PANDEMONIUM)
        && load_mode == LOAD_ENTER_LEVEL)
    {
        while (!followers.empty())
        {
            follower f = followers.front();
            followers.erase(followers.begin());
            f.place(true);
        }
    }                       // end of moving followers

    // Load monsters in transit.
    place_transiting_monsters();

    redraw_all();

    sanity_test_monster_inventory();

    // Translate stairs for pandemonium levels:
    if (you.level_type == LEVEL_PANDEMONIUM)
        fixup_pandemonium_stairs();

    // Things to update for player entering level
    if (load_mode == LOAD_ENTER_LEVEL)
    {
        // update corpses and fountains
        if (env.elapsed_time != 0.0)
            update_level( you.elapsed_time - env.elapsed_time );

        // Centaurs have difficulty with stairs
        int timeval = ((you.species != SP_CENTAUR) ? player_movement_speed()
                                                   : 15); 

        // new levels have less wary monsters: 
        if (just_created_level)
            timeval /= 2;

        timeval -= (stepdown_value( check_stealth(), 50, 50, 150, 150 ) / 10);

#if DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "arrival time: %d", timeval );
#endif

        if (timeval > 0)
        {
            you.time_taken = timeval;
            handle_monsters();
        }
    }

    // Save the created/updated level out to disk:
    save_level( you.your_level, you.level_type, you.where_are_you );

    setup_environment_effects();

    return just_created_level;
}                               // end load()

void save_level(int level_saved, level_area_type old_ltype,
                branch_type where_were_you)
{
    std::string cha_fil = make_filename( you.your_name, level_saved,
                                         where_were_you, old_ltype,
                                         false );

    you.prev_targ = MHITNOT;

    FILE *saveFile = fopen(cha_fil.c_str(), "wb");

    if (saveFile == NULL)
    {
        end(-1, true, "Unable to open \"%s\" for writing",
                 cha_fil.c_str());
    }

    // nail all items to the ground
    fix_item_coordinates();

    write_tagged_file( saveFile, SAVE_MAJOR_VERSION,
                       LEVEL_MINOR_VERSION, TAGTYPE_LEVEL );

    fclose(saveFile);

    DO_CHMOD_PRIVATE(cha_fil.c_str());
}                               // end save_level()


void save_game(bool leave_game)
{
    unwind_bool saving_game(crawl_state.saving_game, true);
    
    /* Stashes */
    std::string stashFile = get_savedir_filename( you.your_name, "", "st" );
    FILE *stashf = fopen(stashFile.c_str(), "wb");
    if (stashf) {
        stashes.save(stashf);
        fclose(stashf);
        DO_CHMOD_PRIVATE(stashFile.c_str());
    }

#ifdef CLUA_BINDINGS
    /* lua */
    std::string luaFile = get_savedir_filename( you.your_name, "", "lua" );
    clua.save(luaFile.c_str());
    // note that luaFile may not exist
    DO_CHMOD_PRIVATE(luaFile.c_str());
#endif

    /* kills */
    std::string killFile = get_savedir_filename( you.your_name, "", "kil" );
    FILE *killf = fopen(killFile.c_str(), "wb");
    if (killf) {
        you.kills.save(killf);
        fclose(killf);
        DO_CHMOD_PRIVATE(killFile.c_str());
    }

    /* travel cache */
    std::string travelCacheFile = get_savedir_filename(you.your_name,"","tc");
    FILE *travelf = fopen(travelCacheFile.c_str(), "wb");
    if (travelf) {
        travel_cache.save(travelf);
        fclose(travelf);
        DO_CHMOD_PRIVATE(travelCacheFile.c_str());
    }
    
    /* notes */
    std::string notesFile = get_savedir_filename(you.your_name, "", "nts");
    FILE *notesf = fopen(notesFile.c_str(), "wb");
    if (notesf) {
        save_notes(notesf);
        fclose(notesf);
        DO_CHMOD_PRIVATE(notesFile.c_str());
    }

    /* tutorial */
    
    std::string tutorFile = get_savedir_filename(you.your_name, "", "tut");
    FILE *tutorf = fopen(tutorFile.c_str(), "wb");
    if (tutorf) {
        save_tutorial(tutorf);
        fclose(tutorf);
        DO_CHMOD_PRIVATE(tutorFile.c_str());
    }

    std::string charFile = get_savedir_filename(you.your_name, "", "sav");
    FILE *charf = fopen(charFile.c_str(), "wb");
    if (!charf)
        end(-1, true, "Unable to open \"%s\" for writing!\n", charFile.c_str());

    write_tagged_file( charf, SAVE_MAJOR_VERSION,
                       YOU_MINOR_VERSION, TAGTYPE_PLAYER );

    fclose(charf);
    DO_CHMOD_PRIVATE(charFile.c_str());

    // if just save, early out
    if (!leave_game)
        return;

    // must be exiting -- save level & goodbye!
    save_level(you.your_level, you.level_type, you.where_are_you);

    clrscr();

#ifdef SAVE_PACKAGE_CMD
    std::string basename = get_savedir_filename(you.your_name, "", "");
    char cmd_buff[1024];

    snprintf( cmd_buff, sizeof(cmd_buff), 
              SAVE_PACKAGE_CMD, basename.c_str(), basename.c_str() );

    if (system( cmd_buff ) != 0) {
        cprintf( EOL "Warning: Zip command (SAVE_PACKAGE_CMD) returned non-zero value!" EOL );
    }
    DO_CHMOD_PRIVATE ( (basename + PACKAGE_SUFFIX).c_str() );
#endif

    cprintf( "See you soon, %s!" EOL , you.your_name );
#ifdef DGL_WHEREIS
    whereis_record("saved");
#endif
    end(0);
}                               // end save_game()

// Saves the game without exiting.
void save_game_state()
{
    save_game(false);
    if (crawl_state.seen_hups)
        save_game(true);
}

void load_ghost(void)
{
    char majorVersion;
    char minorVersion;

    std::string cha_fil = make_filename("bones", you.your_level,
                                        you.where_are_you,
                                        you.level_type,
                                        true );

    FILE *gfile = fopen(cha_fil.c_str(), "rb");

    if (gfile == NULL)
        return;                 // no such ghost.

    if (!determine_ghost_version(gfile, majorVersion, minorVersion))
    {
        fclose(gfile);
#if DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS,
             "Ghost file \"%s\" seems to be invalid.", cha_fil.c_str());
        more();
#endif
        return;
    }

    if (majorVersion != SAVE_MAJOR_VERSION
        || minorVersion != GHOST_MINOR_VERSION)
    {
        
        fclose(gfile);
        unlink(cha_fil.c_str());
        return;
    }

    ghosts.clear();
    restore_ghost_version(gfile, majorVersion, minorVersion);

    // sanity check - EOF
    if (!feof(gfile))
    {
        fclose(gfile);
#if DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Incomplete read of \"%s\".",
                  cha_fil.c_str() );
        more();
#endif
        return;
    }

    fclose(gfile);

#if DEBUG_DIAGNOSTICS
        mpr( "Loaded ghost.", MSGCH_DIAGNOSTICS );
#endif

    // remove bones file - ghosts are hardly permanent.
    unlink(cha_fil.c_str());

    // translate ghost to monster and place.
    for (int imn = 0; imn < MAX_MONSTERS - 10 && !ghosts.empty(); imn++)
    {
        if (menv[imn].type != -1)
            continue;

        menv[imn].set_ghost(ghosts[0]);
        menv[imn].ghost_init();
        
        ghosts.erase(ghosts.begin());        
    }
}

void restore_game(void)
{
    std::string charFile = get_savedir_filename(you.your_name, "", "sav");
    FILE *charf = fopen(charFile.c_str(), "rb");
    if (!charf )
        end(-1, true, "Unable to open %s for reading!\n", charFile.c_str() );

    char majorVersion = 0;
    char minorVersion = 0;

    if (!determine_version(charf, majorVersion, minorVersion))
    {
        perror("\nSavefile appears to be invalid.\n");
        end(-1);
    }

    restore_version(charf, majorVersion, minorVersion);

    // sanity check - EOF
    if (!feof(charf))
        end(-1, false, "\nIncomplete read of \"%s\" - aborting.\n",
            charFile.c_str());

    fclose(charf);

    std::string stashFile = get_savedir_filename( you.your_name, "", "st" );
    FILE *stashf = fopen(stashFile.c_str(), "rb");
    if (stashf) {
        stashes.load(stashf);
        fclose(stashf);
    }

#ifdef CLUA_BINDINGS
    std::string luaFile = get_savedir_filename( you.your_name, "", "lua" );
    clua.execfile( luaFile.c_str() );
#endif

    std::string killFile = get_savedir_filename( you.your_name, "", "kil" );
    FILE *killf = fopen(killFile.c_str(), "rb");
    if (killf) {
        you.kills.load(killf);
        fclose(killf);
    }

    std::string travelCacheFile = get_savedir_filename(you.your_name,"","tc");
    FILE *travelf = fopen(travelCacheFile.c_str(), "rb");
    if (travelf) {
        travel_cache.load(travelf);
        fclose(travelf);
    }

    std::string notesFile = get_savedir_filename(you.your_name, "", "nts");
    FILE *notesf = fopen(notesFile.c_str(), "rb");
    if (notesf) {
        load_notes(notesf);
        fclose(notesf);
    }

    /* tutorial */
    
    std::string tutorFile = get_savedir_filename(you.your_name, "", "tut");
    FILE *tutorf = fopen(tutorFile.c_str(), "rb");
    if (tutorf) {
        load_tutorial(tutorf);
        fclose(tutorf);
    }
}

static bool determine_version( FILE *restoreFile, 
                               char &majorVersion, char &minorVersion )
{
    // read first two bytes.
    char buf[2];
    if (read2(restoreFile, buf, 2) != 2)
        return false;               // empty file?

    // otherwise, read version and validate.
    majorVersion = buf[0];
    minorVersion = buf[1];

    if (majorVersion == SAVE_MAJOR_VERSION)
        return true;

    return false;   // if it's not 0, no idea
}

static void restore_version( FILE *restoreFile, 
                             char majorVersion, char minorVersion )
{
    // assuming the following check can be removed once we can read all
    // savefile versions.
    if (majorVersion != SAVE_MAJOR_VERSION)
    {
        end(-1, false, "\nSorry, this release cannot read a v%d.%d savefile.\n",
            majorVersion, minorVersion);
    }

    switch(majorVersion)
    {
        case SAVE_MAJOR_VERSION:
            restore_tagged_file(restoreFile, TAGTYPE_PLAYER, minorVersion);
            break;
        default:
            break;
    }
}

// generic v4 restore function
static void restore_tagged_file( FILE *restoreFile, int fileType, 
                                 char minorVersion )
{
    int i;

    char tags[NUM_TAGS];
    tag_set_expected(tags, fileType);

    while(1)
    {
        i = tag_read(restoreFile, minorVersion);
        if (i == 0)                 // no tag!
            break;
        tags[i] = 0;                // tag read
        if (fileType == TAGTYPE_PLAYER_NAME)
            break;
    }

    // go through and init missing tags
    for(i=0; i<NUM_TAGS; i++)
    {
        if (tags[i] == 1)           // expected but never read
            tag_missing(i, minorVersion);
    }
}

static bool determine_level_version( FILE *levelFile, 
                                     char &majorVersion, char &minorVersion )
{
    // read first two bytes.
    char buf[2];
    if (read2(levelFile, buf, 2) != 2)
        return false;               // empty file?

    // otherwise, read version and validate.
    majorVersion = buf[0];
    minorVersion = buf[1];

    if (majorVersion == SAVE_MAJOR_VERSION)
        return true;

    return false;   // if its not SAVE_MAJOR_VERSION, no idea
}

static void restore_level_version( FILE *levelFile, 
                                   char majorVersion, char minorVersion )
{
    // assuming the following check can be removed once we can read all
    // savefile versions.
    if (majorVersion != SAVE_MAJOR_VERSION)
    {
        end(-1, false,
            "\nSorry, this release cannot read a v%d.%d level file.\n",
            majorVersion, minorVersion);
    }

    switch(majorVersion)
    {
        case SAVE_MAJOR_VERSION:
            restore_tagged_file(levelFile, TAGTYPE_LEVEL, minorVersion);
            break;
        default:
            break;
    }
}

static bool determine_ghost_version( FILE *ghostFile, 
                                     char &majorVersion, char &minorVersion )
{
    // read first two bytes.
    char buf[2];
    if (read2(ghostFile, buf, 2) != 2)
        return false;               // empty file?

    // check for pre-v4 -- simply started right in with ghost name.
    if (isprint(buf[0]) && buf[0] > 4)
    {
        majorVersion = 0;
        minorVersion = 0;
        rewind(ghostFile);
        return true;
    }

    // otherwise, read version and validate.
    majorVersion = buf[0];
    minorVersion = buf[1];

    if (majorVersion == SAVE_MAJOR_VERSION)
        return true;

    return false;   // if its not SAVE_MAJOR_VERSION, no idea!
}

static void restore_ghost_version( FILE *ghostFile, 
                                   char majorVersion, char minorVersion )
{
    switch(majorVersion)
    {
    case SAVE_MAJOR_VERSION:
        restore_tagged_file(ghostFile, TAGTYPE_GHOST, minorVersion);
        break;
    default:
        break;
    }
}

void save_ghost( bool force )
{
    if (!force && (you.your_level < 2 || you.is_undead))
        return;

    std::string cha_fil = make_filename( "bones", you.your_level,
                                         you.where_are_you,
                                         you.level_type,
                                         true );

    FILE *gfile = fopen(cha_fil.c_str(), "rb");

    // don't overwrite existing bones!
    if (gfile != NULL)
    {
        fclose(gfile);
        return;
    }

    ghosts = ghost_demon::find_ghosts();
    
    gfile = lk_open("wb", cha_fil);

    if (gfile == NULL)
    {
        mprf("Error creating ghost file: %s", cha_fil.c_str());
        more();
        return;
    }

    write_tagged_file( gfile, SAVE_MAJOR_VERSION,
                       GHOST_MINOR_VERSION, TAGTYPE_GHOST );

    lk_close(gfile, "wb", cha_fil);

#if DEBUG_DIAGNOSTICS
    mpr( "Saved ghost.", MSGCH_DIAGNOSTICS );
#endif

    DO_CHMOD_PRIVATE(cha_fil.c_str());
}                               // end save_ghost()


void generate_random_demon()
{
    int rdem = 0;
    for (rdem = 0; rdem < MAX_MONSTERS + 1; rdem++)
    {
        if (rdem == MAX_MONSTERS)
            return;

        if (menv[rdem].type == MONS_PANDEMONIUM_DEMON)
            break;
    }

    ghost_demon pandemon;
    pandemon.init_random_demon();
    menv[rdem].set_ghost(pandemon);
    menv[rdem].pandemon_init();
}                               // end generate_random_demon()

// Largest string we'll save
#define STR_CAP 1000

void writeShort(FILE *file, short s)
{
    char data[2];
    // High byte first - network order
    data[0] = static_cast<char>((s >> 8) & 0xFF);
    data[1] = static_cast<char>(s & 0xFF);

    write2(file, data, sizeof(data));
}

short readShort(FILE *file)
{
    unsigned char data[2];
    read2(file, (char *) data, 2);

    // High byte first
    return (((short) data[0]) << 8) | (short) data[1];
}

void writeByte(FILE *file, unsigned char byte)
{
    write2(file, (char *) &byte, sizeof byte);
}

unsigned char readByte(FILE *file)
{
    unsigned char byte;
    read2(file, (char *) &byte, sizeof byte);
    return byte;
}

void writeString(FILE* file, const std::string &s, int cap)
{
    int length = s.length();
    if (length > cap)
        length = cap;
    writeShort(file, length);
    write2(file, s.c_str(), length);
}

std::string readString(FILE *file)
{
    short length = readShort(file);
    if (length > 0)
    {
        if (length <= STR_CAP)
        {
            char buf[STR_CAP + 1];
            read2(file, buf, length);
            buf[length] = 0;
            return (buf);
        }

        end(1, false, "String too long: %d bytes\n", length);
    }

    return ("");
}

void writeLong(FILE* file, long num)
{
    // High word first, network order
    writeShort(file, (short) ((num >> 16) & 0xFFFFL));
    writeShort(file, (short) (num & 0xFFFFL));
}

long readLong(FILE *file)
{
    // We need the unsigned short cast even for the high word because we
    // might be on a system where long is more than 4 bytes, and we don't want
    // to sign extend the high short.
    return ((long) (unsigned short) readShort(file)) << 16 | 
        (long) (unsigned short) readShort(file);
}

////////////////////////////////////////////////////////////////////////////
// Locking for multiuser systems

// first, some file locking stuff for multiuser crawl
#ifdef USE_FILE_LOCKING

bool lock_file_handle( FILE *handle, int type )
{
    struct flock  lock;
    int           status;

    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_type = type;

#ifdef USE_BLOCKING_LOCK
    status = fcntl( fileno( handle ), F_SETLKW, &lock );
#else
    for (int i = 0; i < 30; i++)
    {
        status = fcntl( fileno( handle ), F_SETLK, &lock );

        // success
        if (status == 0)
            break;

        // known failure
        if (status == -1 && (errno != EACCES && errno != EAGAIN))
            break;

        perror( "Problems locking file... retrying..." );
        delay( 1000 );
    }
#endif

    return (status == 0);
}

bool unlock_file_handle( FILE *handle )
{
    struct flock  lock;
    int           status;

    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_type = F_UNLCK;

#ifdef USE_BLOCKING_LOCK

    status = fcntl( fileno( handle ), F_SETLKW, &lock );

#else

    for (int i = 0; i < 30; i++)
    {
        status = fcntl( fileno( handle ), F_SETLK, &lock );

        // success
        if (status == 0)
            break;

        // known failure
        if (status == -1 && (errno != EACCES && errno != EAGAIN))
            break;

        perror( "Problems unlocking file... retrying..." );
        delay( 1000 );
    }

#endif

    return (status == 0);
}

#endif

FILE *lk_open(const char *mode, const std::string &file)
{
    FILE *handle = fopen(file.c_str(), mode);
#ifdef SHARED_FILES_CHMOD_PUBLIC
    chmod(file.c_str(), SHARED_FILES_CHMOD_PUBLIC);
#endif

#ifdef USE_FILE_LOCKING
    int locktype = F_RDLCK;
    if (mode && mode[0] != 'r')
        locktype = F_WRLCK;

    if (handle && !lock_file_handle( handle, locktype ))
    {
        perror( "Could not lock file... " );
        fclose( handle );
        handle = NULL;
    }
#endif
    return handle;
}

void lk_close(FILE *handle, const char *mode, const std::string &file)
{
    UNUSED( mode );

    if (handle == NULL || handle == stdin)
        return;

#ifdef USE_FILE_LOCKING
    unlock_file_handle( handle );
#endif

    // actually close
    fclose(handle);

#ifdef SHARED_FILES_CHMOD_PUBLIC
    if (mode && mode[0] == 'w')
        chmod(file.c_str(), SHARED_FILES_CHMOD_PUBLIC);
#endif
}

/////////////////////////////////////////////////////////////////////////////
// file_lock
//
// Locks a named file (usually an empty lock file), creating it if necessary.

file_lock::file_lock(const std::string &s, const char *_mode, bool die_on_fail)
    : handle(NULL), mode(_mode), filename(s)
{
#ifdef USE_FILE_LOCKING
    if (!(handle = lk_open(mode, filename)) && die_on_fail)
        end(1, true, "Unable to open lock file \"%s\"", filename.c_str());
#endif
}

file_lock::~file_lock()
{
#ifdef USE_FILE_LOCKING
    if (handle)
        lk_close(handle, mode, filename);
#endif
}
