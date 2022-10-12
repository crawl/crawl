/**
 * @file
 * @brief Functions used to save and load levels/games.
**/

// old compiler compatibility for CAO/CBRO stdint.h. cstdint doesn't work
// on these gcc versions to provide UINT8_MAX.
#ifndef  __STDC_LIMIT_MACROS
#define  __STDC_LIMIT_MACROS 1
#endif
#include <stdint.h>

#include "AppHdr.h"

#include "files.h"

#include "json.h"
#include "json-wrapper.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef HAVE_UTIMES
#include <sys/time.h>
#endif
#include <sys/types.h>
#ifdef UNIX
#include <unistd.h>
#endif

#include "abyss.h"
#include "act-iter.h"
#include "areas.h"
#include "branch.h"
#include "chardump.h"
#include "cloud.h"
#include "coordit.h"
#include "dactions.h"
#include "dbg-util.h"
#include "dgn-overview.h"
#include "directn.h"
#include "dungeon.h"
#include "end.h"
#include "tile-env.h"
#include "errors.h"
#include "player-save-info.h"
#include "fineff.h"
#include "ghost.h"
#include "god-abil.h"
#include "god-companions.h"
#include "god-passive.h"
#include "hints.h"
#include "initfile.h"
#include "item-name.h"
#include "items.h"
#include "jobs.h"
#include "kills.h"
#include "level-state-type.h"
#include "libutil.h"
#include "macro.h"
#include "mapmark.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-place.h"
#include "nearby-danger.h"
#include "notes.h"
#include "place.h"
#include "prompt.h"
#include "skills.h"
#include "species.h"
#include "spl-summoning.h"
#include "spl-transloc.h" // cell_vetoes_teleport
#include "stairs.h"
#include "state.h"
#include "stringutil.h"
#include "syscalls.h"
#include "tag-version.h"
#include "teleport.h"
#include "terrain.h"
#ifdef USE_TILE
 // TODO -- dolls
 #include "rltiles/tiledef-player.h"
 #include "tilepick-p.h"
#endif
#include "tileview.h"
#include "tiles-build-specific.h"
#include "timed-effects.h"
#include "ui.h"
#include "unwind.h"
#include "version.h"
#include "view.h"
#include "xom.h"
#include "zot.h"

#ifdef __ANDROID__
#include <android/log.h>
#endif

#ifndef F_OK // MSVC for example
#define F_OK 0
#endif

#ifdef __HAIKU__
#include <FindDirectory.h>
#endif

#define BONES_DIAGNOSTICS (defined(WIZARD) || defined(DEBUG_BONES) || defined(DEBUG_DIAGNOSTICS))

#ifdef BONES_DIAGNOSTICS
/// show diagnostics following a wizard command, even if not a debug build
static void _ghost_dprf(const char *format, ...)
{
    va_list argp;
    va_start(argp, format);

#ifndef DEBUG_DIAGNOSTICS
    const bool wiz_cmd = (crawl_state.prev_cmd == CMD_WIZARD);
    if (wiz_cmd)
#endif
        do_message_print(MSGCH_DIAGNOSTICS, 0, false, false, format, argp);

    va_end(argp);
}
#else
# define _ghost_dprf(...) ((void)0)
#endif

static bool _ghost_version_compatible(const save_version &version);

static bool _restore_tagged_chunk(package *save, const string &name,
                                  tag_type tag, const char* complaint);
static player_save_info _read_character_info(package *save);

static bool _convert_obsolete_species();

const short GHOST_SIGNATURE = short(0xDC55);

const int GHOST_LIMIT = 27; // max number of ghost files per level

static void _redraw_all()
{
    you.redraw_hit_points    = true;
    you.redraw_magic_points  = true;
    you.redraw_stats.init(true);
    you.redraw_armour_class  = true;
    you.redraw_evasion       = true;
    you.redraw_experience    = true;
    you.redraw_status_lights = true;
}

static bool is_save_file_name(const string &name)
{
    int off = name.length() - strlen(SAVE_SUFFIX);
    if (off <= 0)
        return false;
    return !strcasecmp(name.c_str() + off, SAVE_SUFFIX);
}

vector<string> get_dir_files_sorted(const string &dirname)
{
    auto result = get_dir_files(dirname);
    sort(result.begin(), result.end());
    return result;
}

// Returns a vector of files (including directories if requested) in
// the given directory, recursively. All filenames returned are
// relative to the start directory. If an extension is supplied, all
// filenames (and directory names if include_directories is set)
// returned must be suffixed with the extension (the extension is not
// modified in any way, so if you want, say, ".des", you must include
// the "." as well).
//
// If recursion_depth is -1, the recursion is infinite, as far as the
// directory structure and filesystem allows. If recursion_depth is 0,
// only files in the start directory are returned.
vector<string> get_dir_files_recursive(const string &dirname, const string &ext,
                                       int recursion_depth,
                                       bool include_directories)
{
    vector<string> files;

    const int next_recur_depth =
        recursion_depth == -1? -1 : recursion_depth - 1;
    const bool recur = recursion_depth == -1 || recursion_depth > 0;

    for (const string &filename : get_dir_files_sorted(dirname))
    {
        if (dir_exists(catpath(dirname, filename)))
        {
            if (include_directories
                && (ext.empty() || ends_with(filename, ext)))
            {
                files.push_back(filename);
            }

            if (recur)
            {
                // Each filename in a subdirectory has to be prefixed
                // with the subdirectory name.
                for (const string &subdirfile
                        : get_dir_files_recursive(catpath(dirname, filename),
                                                  ext, next_recur_depth))
                {
                    files.push_back(catpath(filename, subdirfile));
                }
            }
        }
        else
        {
            if (ext.empty() || ends_with(filename, ext))
                files.push_back(filename);
        }
    }
    return files;
}

vector<string> get_dir_files_ext(const string &dir, const string &ext)
{
    return get_dir_files_recursive(dir, ext, 0);
}

string get_parent_directory(const string &filename)
{
    string::size_type pos = filename.rfind(FILE_SEPARATOR);
    if (pos != string::npos)
        return filename.substr(0, pos + 1);
#ifdef ALT_FILE_SEPARATOR
    pos = filename.rfind(ALT_FILE_SEPARATOR);
    if (pos != string::npos)
        return filename.substr(0, pos + 1);
#endif
    return "";
}

string get_base_filename(const string &filename)
{
    string::size_type pos = filename.rfind(FILE_SEPARATOR);
    if (pos != string::npos)
        return filename.substr(pos + 1);
#ifdef ALT_FILE_SEPARATOR
    pos = filename.rfind(ALT_FILE_SEPARATOR);
    if (pos != string::npos)
        return filename.substr(pos + 1);
#endif
    return filename;
}

string get_cache_name(const string &filename)
{
    string::size_type pos = filename.rfind(FILE_SEPARATOR);
    while (pos != string::npos && filename.find("/des", pos) != pos)
        pos = filename.rfind(FILE_SEPARATOR, pos - 1);
    if (pos != string::npos)
        return replace_all_of(filename.substr(pos + 5), " /\\:", "_");
#ifdef ALT_FILE_SEPARATOR
    pos = filename.rfind(ALT_FILE_SEPARATOR);
    while (pos != string::npos && filename.find("/des", pos) != pos)
        pos = filename.rfind(ALT_FILE_SEPARATOR, pos - 1);
    if (pos != string::npos)
        return replace_all_of(filename.substr(pos + 5), " /\\:", "_");
#endif
    return filename;
}

bool is_absolute_path(const string &path)
{
    return !path.empty()
           && (path[0] == FILE_SEPARATOR
#ifdef TARGET_OS_WINDOWS
               || path.find(':') != string::npos
#endif
             );
}

// Concatenates two paths, separating them with FILE_SEPARATOR if necessary.
// Assumes that the second path is not absolute.
//
// If the first path is empty, returns the second unchanged. The second path
// may be absolute in this case.
string catpath(const string &first, const string &second)
{
    if (first.empty())
        return second;

    string directory = first;
    if (directory[directory.length() - 1] != FILE_SEPARATOR
        && (second.empty() || second[0] != FILE_SEPARATOR))
    {
        directory += FILE_SEPARATOR;
    }
    directory += second;

    return directory;
}

// Given a relative path and a reference file name, returns the relative path
// suffixed to the directory containing the reference file name. Assumes that
// the second path is not absolute.
string get_path_relative_to(const string &referencefile,
                            const string &relativepath)
{
    return catpath(get_parent_directory(referencefile),
                   relativepath);
}

string change_file_extension(const string &filename, const string &ext)
{
    const string::size_type pos = filename.rfind('.');
    return (pos == string::npos? filename : filename.substr(0, pos)) + ext;
}

time_t file_modtime(const string &file)
{
    struct stat filestat;
    if (stat(file.c_str(), &filestat))
        return 0;

    return filestat.st_mtime;
}

time_t file_modtime(FILE *f)
{
    struct stat filestat;
    if (fstat(fileno(f), &filestat))
        return 0;

    return filestat.st_mtime;
}

static bool _create_directory(const char *dir)
{
    if (!mkdir_u(dir, 0755))
        return true;
    if (errno == EEXIST || errno == EROFS) // might be not a directory
        return dir_exists(dir);
    return false;
}

static bool _create_dirs(const string &dir)
{
    string sep = " ";
    sep[0] = FILE_SEPARATOR;
    vector<string> segments = split_string(sep, dir, false, false);

    string path;
    for (int i = 0, size = segments.size(); i < size; ++i)
    {
        path += segments[i];

        // Handle absolute paths correctly.
        if (i == 0 && dir.size() && dir[0] == FILE_SEPARATOR)
            path = FILE_SEPARATOR + path;

        if (!_create_directory(path.c_str()))
            return false;

        path += FILE_SEPARATOR;
    }
    return true;
}

// Checks whether the given path is safe to read from. A path is safe if:
// 1. If Unix: It contains no shell metacharacters.
// 2. If DATA_DIR_PATH is set: the path is not an absolute path.
// 3. If DATA_DIR_PATH is set: the path contains no ".." sequence.
void assert_read_safe_path(const string &path)
{
    // Check for rank tomfoolery first:
    if (path.empty())
        throw unsafe_path("Empty file name.");

#ifdef UNIX
    if (!shell_safe(path.c_str()))
        throw unsafe_path_f("\"%s\" contains bad characters.", path.c_str());
#endif

#ifdef DATA_DIR_PATH
    if (is_absolute_path(path))
        throw unsafe_path_f("\"%s\" is an absolute path.", path.c_str());

    if (path.find("..") != string::npos)
        throw unsafe_path_f("\"%s\" contains \"..\" sequences.", path.c_str());
#endif

    // Path is okay.
}

string canonicalise_file_separator(const string &path)
{
    const string sep(1, FILE_SEPARATOR);
    return replace_all_of(replace_all_of(path, "/", sep),
                          "\\", sep);
}

static vector<string> _get_base_dirs()
{
#ifdef __HAIKU__
    char path[B_PATH_NAME_LENGTH];
    find_path(B_APP_IMAGE_SYMBOL,
            B_FIND_PATH_DATA_DIRECTORY,
            "crawl/",
            path,
            B_PATH_NAME_LENGTH);
#endif
    const string rawbases[] =
    {
#ifdef DATA_DIR_PATH
        DATA_DIR_PATH,
#else
        !SysEnv.crawl_dir.empty()? SysEnv.crawl_dir : "",
        !SysEnv.crawl_base.empty()? SysEnv.crawl_base : "",
#endif
#ifdef TARGET_OS_MACOSX
        SysEnv.crawl_base + "../Resources/",
#endif
#ifdef __ANDROID__
        ANDROID_ASSETS,
        "/sdcard/Android/data/org.develz.crawl/files/",
#endif
#ifdef __HAIKU__
        std::string(path),
#endif
    };

    const string prefixes[] =
    {
        string("dat") + FILE_SEPARATOR,
#ifdef USE_TILE_LOCAL
        string("dat/tiles") + FILE_SEPARATOR,
#endif
        string("docs") + FILE_SEPARATOR,
        string("settings") + FILE_SEPARATOR,
#ifndef DATA_DIR_PATH
        string("..") + FILE_SEPARATOR + "docs" + FILE_SEPARATOR,
        string("..") + FILE_SEPARATOR + "dat" + FILE_SEPARATOR,
#ifdef USE_TILE_LOCAL
        string("..") + FILE_SEPARATOR + "dat/tiles" + FILE_SEPARATOR,
#endif
        string("..") + FILE_SEPARATOR + "settings" + FILE_SEPARATOR,
        string("..") + FILE_SEPARATOR,
#endif
        "",
    };

    vector<string> bases;
    for (string base : rawbases)
    {
        if (base.empty())
            continue;

        base = canonicalise_file_separator(base);

        if (base[base.length() - 1] != FILE_SEPARATOR)
            base += FILE_SEPARATOR;

        for (unsigned p = 0; p < ARRAYSZ(prefixes); ++p)
            bases.push_back(base + prefixes[p]);
    }

    return bases;
}
/**
 * check if `d` is a complete crawl data directory.
 *
 * @return MB_TRUE if yes, otherwise no. Returns MB_FALSE if there are some
 * but not all data subfolders.
 */
maybe_bool validate_data_dir(const string &d)
{
    // there are a few others, but this should be enough to minimally run something
    static const vector<string> data_subfolders =
    {
        "clua",
        "database",
        "defaults",
        "des",
        "descript",
        "dlua"
#ifdef USE_TILE_LOCAL
        , "tiles"
#endif
    };

    if (!dir_exists(d))
        return MB_FALSE;

    bool everything = true;
    bool something = false;
    for (auto subdir : data_subfolders)
    {
        if (dir_exists(catpath(d, subdir)))
            something = true;
        else
            everything = false;
    }
    return everything ? MB_TRUE : something ? MB_MAYBE : MB_FALSE;
}

void validate_basedirs()
{
    // TODO: could use this to pick a single data directory? Right now the
    // behavior is that files in directories earliest on this list get
    // priority.
    vector<string> bases(_get_base_dirs());
    bool found = false;

    for (const string &d : bases)
    {
        maybe_bool status = validate_data_dir(d);
        if (status == MB_FALSE)
            continue; // empty or non-existent, ignore
        else if (status == MB_MAYBE)
        {
            // give an error for this case because this incomplete data
            // directory will be checked before others, possibly leading
            // to a weird mix of data files.
            if (!found)
            {
                mprf(MSGCH_ERROR,
                    "Incomplete or corrupted data directory '%s'",
                            d.c_str());
            }
        }
        else // MB_TRUE -- found a complete data directory
        {
            if (!found)
                mprf(MSGCH_PLAIN, "Data directory '%s' found.", d.c_str());
            found = true;
        }
    }

    // can't proceed if nothing complete was found.
    if (!found)
    {
        string err = "Missing DCSS data directory; tried: \n";
        err += comma_separated_line(bases.begin(), bases.end());

        end(1, false, "%s", err.c_str());
    }
}

string datafile_path(string basename, bool croak_on_fail, bool test_base_path,
                     bool (*thing_exists)(const string&))
{
    basename = canonicalise_file_separator(basename);

    if (test_base_path && thing_exists(basename))
        return basename;

    for (const string &basedir : _get_base_dirs())
    {
        string name = basedir + basename;
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO,"Crawl","Looking for %s as '%s'",basename.c_str(),name.c_str());
#endif
        if (thing_exists(name))
            return name;
    }

    // Die horribly.
    if (croak_on_fail)
    {
        end(1, false, "Cannot find data file '%s' anywhere, aborting\n",
            basename.c_str());
    }

    return "";
}

// Checks if directory 'dir' exists and tries to create it if it
// doesn't exist, modifying 'dir' to its canonical form.
//
// If given an empty 'dir', returns true without modifying 'dir' or
// performing any other checks.
//
// Otherwise, returns true if the directory already exists or was just
// created. 'dir' will be modified to a canonical representation,
// guaranteed to have the file separator appended to it, and with any
// / and \ separators replaced with the one true FILE_SEPARATOR.
//
bool check_mkdir(const string &whatdir, string *dir, bool silent)
{
    if (dir->empty())
        return true;

    *dir = canonicalise_file_separator(*dir);

    // Suffix the separator if necessary
    if ((*dir)[dir->length() - 1] != FILE_SEPARATOR)
        *dir += FILE_SEPARATOR;

    if (!dir_exists(*dir) && !_create_dirs(*dir))
    {
        if (!silent)
        {
#ifdef __ANDROID__
            __android_log_print(ANDROID_LOG_INFO, "Crawl",
                                "%s \"%s\" does not exist and I can't create it.",
                                whatdir.c_str(), dir->c_str());
#endif
            fprintf(stderr, "%s \"%s\" does not exist "
                    "and I can't create it.\n",
                    whatdir.c_str(), dir->c_str());
        }
        return false;
    }

    return true;
}

// Get the directory that contains save files for the current game
// type. This will not be the same as get_base_savedir() for game
// types such as Sprint.
static string _get_savefile_directory()
{
    string dir = catpath(Options.save_dir, crawl_state.game_savedir_path());
    check_mkdir("Save directory", &dir, false);
    if (dir.empty())
        dir = ".";
    return dir;
}


/**
 * Location of legacy ghost files. (The save directory.)
 *
 * @return The path to the directory for old ghost files.
 */
static string _get_old_bonefile_directory()
{
    string dir = catpath(Options.shared_dir, crawl_state.game_savedir_path());
    check_mkdir("Bones directory", &dir, false);
    if (dir.empty())
        dir = ".";
    return dir;
}

/**
 * Location of ghost files.
 *
 * @return The path to the directory for ghost files.
 */
static string _get_bonefile_directory()
{
    string dir = catpath(Options.shared_dir, crawl_state.game_savedir_path());
    dir = catpath(dir, "bones");
    check_mkdir("Bones directory", &dir, false);
    if (dir.empty())
        dir = ".";
    return dir;
}

// Returns a subdirectory of the current savefile directory as returned by
// _get_savefile_directory.
static string _get_savedir_path(const string &shortpath)
{
    return canonicalise_file_separator(
        catpath(_get_savefile_directory(), shortpath));
}

// Returns a subdirectory of the base save directory that contains all saves
// and cache directories. Save files for game type != GAME_TYPE_NORMAL may
// be found in a subdirectory of this dir. Use _get_savefile_directory() if
// you want the directory that contains save games for the current game
// type.
static string _get_base_savedir_path(const string &subpath = "")
{
    return canonicalise_file_separator(catpath(Options.save_dir, subpath));
}

// Given a simple (relative) path, returns the path relative to the
// base save directory and a subdirectory named with the game version.
// This is useful when writing cache files and similar output that
// should not be shared between different game versions.
string savedir_versioned_path(const string &shortpath)
{
#ifdef VERSIONED_CACHE_DIR
    const string versioned_dir =
        _get_base_savedir_path(string("cache.") + Version::Long);
#else
    const string versioned_dir = _get_base_savedir_path();
#endif
    return catpath(versioned_dir, shortpath);
}

#ifdef USE_TILE
#define LINEMAX 1024
static bool _readln(chunk_reader &rd, char *buf)
{
    for (int space = LINEMAX - 1; space; space--)
    {
        if (!rd.read(buf, 1))
            return false;
        if (*buf == '\n')
            break;
        buf++;
    }
    *buf = 0;
    return true;
}

static void _fill_player_doll(player_save_info &p, package *save)
{
    dolls_data equip_doll;
    for (unsigned int j = 0; j < TILEP_PART_MAX; ++j)
        equip_doll.parts[j] = TILEP_SHOW_EQUIP;

    equip_doll.parts[TILEP_PART_BASE]
        = tilep_species_to_base_tile(p.species, p.experience_level);

    bool success = false;

    chunk_reader fdoll(save, "tdl");
    {
        char fbuf[LINEMAX];
        if (_readln(fdoll,fbuf))
        {
            tilep_scan_parts(fbuf, equip_doll, p.species, p.experience_level);
            tilep_race_default(p.species, p.experience_level, &equip_doll);
            success = true;
        }
    }

    if (!success) // Use default doll instead.
    {
        job_type job = get_job_by_name(p.class_name.c_str());
        if (job == JOB_UNKNOWN)
            job = JOB_FIGHTER;

        tilep_job_default(job, &equip_doll);
    }
    p.doll = equip_doll;
}
#endif

/*
 * Returns a list of the names of characters that are already saved for the
 * current user.
 */

static vector<player_save_info> _find_saved_characters()
{
    vector<player_save_info> chars;

    if (Options.no_save)
        return chars;

#ifndef DISABLE_SAVEGAME_LISTS
    string searchpath = _get_savefile_directory();

    if (searchpath.empty())
        searchpath = ".";

    for (const string &filename : get_dir_files_sorted(searchpath))
    {
        if (is_save_file_name(filename))
        {
            try
            {
                package save(_get_savedir_path(filename).c_str(), false);
                player_save_info p = _read_character_info(&save);
                if (!p.name.empty())
                {
                    p.filename = filename;
#ifdef USE_TILE
                    if (Options.tile_menu_icons && save.has_chunk("tdl"))
                        _fill_player_doll(p, &save);
#endif
                    chars.push_back(p);
                }
            }
            catch (ext_fail_exception &E)
            {
                dprf("%s: %s", filename.c_str(), E.what());
            }
            catch (game_ended_condition &E) // another process is using the save
            {
                if (E.exit_reason != game_exit::abort)
                    throw;
            }
        }
    }

    sort(chars.begin(), chars.end());
#endif // !DISABLE_SAVEGAME_LISTS
    return chars;
}

vector<player_save_info> find_all_saved_characters()
{
    set<string> dirs;
    vector<player_save_info> saved_characters;
    for (int i = 0; i < NUM_GAME_TYPE; ++i)
    {
        unwind_var<game_type> gt(
            crawl_state.type,
            static_cast<game_type>(i));

        const string savedir = _get_savefile_directory();
        if (dirs.count(savedir))
            continue;

        dirs.insert(savedir);

        vector<player_save_info> chars_in_dir = _find_saved_characters();
        saved_characters.insert(saved_characters.end(),
                                chars_in_dir.begin(),
                                chars_in_dir.end());
    }
    return saved_characters;
}

bool save_exists(const string& filename)
{
    return file_exists(_get_savefile_directory() + filename);
}

string get_savedir_filename(const string &name)
{
    return _get_savefile_directory() + get_save_filename(name);
}

#define MAX_FILENAME_LENGTH 250
string get_save_filename(const string &name)
{
    return chop_string(strip_filename_unsafe_chars(name), MAX_FILENAME_LENGTH,
                       false) + SAVE_SUFFIX;
}

static bool _game_type_has_saves(const game_type g)
{
    // TODO: this may be useful elsewhere too?
    switch (g)
    {
    case GAME_TYPE_ARENA:
    case GAME_TYPE_HIGH_SCORES:
    case GAME_TYPE_INSTRUCTIONS:
    case GAME_TYPE_UNSPECIFIED:
        return false;
    default:
        return true;
    }
}

static bool _game_type_removed(const game_type g)
{
    return g == GAME_TYPE_ZOTDEF;
}

static bool _append_save_info(JsonWrapper &json, const char *filename,
                                        game_type intended_gt=NUM_GAME_TYPE)
{
    if (!file_exists(filename))
        return false;
    try
    {
        package save(filename, false);
        player_save_info p = _read_character_info(&save);

        // TODO: some json for the non-loadable case? I think this comes up
        // for save compat mismatches so shouldn't be relevant for webtiles
        // except in case of bugs...
        if (p.name.empty() || !p.save_loadable)
            return false;

        auto *game_json = json_mkobject();

        // TODO: version info might be useful?
        json_append_member(game_json, "loadable", json_mkbool(true));
        json_append_member(game_json, "name", json_mkstring(p.name));
        json_append_member(game_json, "game_type",
            json_mkstring(gametype_to_str(p.saved_game_type)));
        json_append_member(game_json, "short_desc",
            json_mkstring(p.short_desc(false)));
        json_append_member(game_json, "really_short_desc",
            json_mkstring(p.really_short_desc()));

        // for the case where we are querying just one file, we don't have
        // info on what the save slot is (if any -- could be an arbitrary
        // file) so just use the file's value. This is really only here so
        // that there is a consistent format to the json.
        json_append_member(json.node, intended_gt == NUM_GAME_TYPE
                                ? gametype_to_str(p.saved_game_type).c_str()
                                : gametype_to_str(intended_gt).c_str(),
                            game_json);
        return true;
    }
    catch (game_ended_condition &E) // another process is using the save
    {
        if (E.exit_reason != game_exit::abort)
            end(1); // something has gone fairly wrong in this case

        auto *game_json = json_mkobject();

        json_append_member(game_json, "loadable", json_mkbool(false));
        json_append_member(game_json, "name", json_mkstring(""));
        json_append_member(game_json, "game_type", json_mkstring(""));
        json_append_member(game_json, "short_desc",
                                                json_mkstring("Save in use"));
        json_append_member(game_json, "really_short_desc",
                                                json_mkstring(""));

        // May give "none" in the case of querying a save file by name
        // that is currently in use.
        json_append_member(json.node, gametype_to_str(intended_gt).c_str(),
                                                                    game_json);
        return true;
    }
}

static bool _append_player_save_info(JsonWrapper &json, const char *name, game_type gt)
{
    // requires init file to have been read, otherwise the correct savedir
    // paths may not have been initialized
    unwind_var<game_type> temp_gt(crawl_state.type, gt);
    return _append_save_info(json, get_savedir_filename(name).c_str(), gt);
}

/**
 * Print information about save files associated with `name` in JSON format.
 * The JSON format is a map (JSON Object) from (saveable) game types to save
 * information.
 *
 * If `name` is a filename, the map will have one element in it for just that
 * file; if it is a player name, the map will have one entry for every
 * game type that has a save. (Keep in mind that most game types share a single
 * save slot.)
 *
 * If a save file is currently in use by some other process, it will get
 * `loadable': false, as well as most other info missing. If a save is queried
 * by filename and is in use, it will additionally be mapped from game type
 * `none`.
 */
NORETURN void print_save_json(const char *name)
{
    // TODO: The overall call is quite heavy. Can the overhead to get to this
    // point be simplified at all? On my local machine it's about 80-100ms per
    // call if things go well.
    try
    {
        JsonWrapper json(json_mkobject());
        // Check for the exact filename first, then go by char name.
        // TODO: based on other CLOs, but maybe these shouldn't be collapsed
        // into a single option?
        if (file_exists(name))
        {
            if (!_append_save_info(json, name))
            {
                fprintf(stderr, "Could not load '%s'\n", name);
                end(1);
            }
        }
        else
        {
            // Ensure that the savedir option is set correctly on the first
            // parse_args pass.
            // TODO: read initfile for local games?
            Options.reset_paths();

            // treat `name` as a character name. Prints an empty json dict
            // if this is wrong (or if the character has no saves).
            // TODO: this code (and much other code) could be a lot smarter
            // about shared save slots. (Everything but sprint shares just
            // one slot...)
            for (int i = 0; i < NUM_GAME_TYPE; ++i)
            {
                auto gt = static_cast<game_type>(i);
                if (_game_type_has_saves(gt) && !_game_type_removed(gt))
                    _append_player_save_info(json, name, gt);
            }
        }

        fprintf(stdout, "%s", json.to_string().c_str());
        end(0);
    }
    catch (ext_fail_exception &fe)
    {
        fprintf(stderr, "Error: %s\n", fe.what());
        end(1);
    }
}

static string _get_prefs_path()
{
#ifdef DGL_STARTUP_PREFS_BY_NAME
    // if prfs are being organized by name, the save directory per se is most
    // likely versioned, so store them in a subdirectory of the shared
    // directory instead.
    string dir = catpath(catpath(
            Options.shared_dir,
            crawl_state.game_savedir_path()),
        "prefs");
    check_mkdir("Preferences directory", &dir, false);
    return dir;
#else
    return _get_savefile_directory();
#endif
}

string get_prefs_filename()
{
#ifdef DGL_STARTUP_PREFS_BY_NAME
    // in the early startup sequence we need to use Options.game.name, but this
    // becomes empty by the time the game is actually starting. (...)
    const string player_name = Options.game.name.length()
        ? Options.game.name : you.your_name;
    const string filename =
        "start-" + strip_filename_unsafe_chars(player_name) + "-ns.prf";
#else
    const string filename = "start-ns.prf";
#endif
    return catpath(_get_prefs_path(), filename);
}

void write_ghost_version(writer &outf)
{
    // this may be distinct from the current save version
    write_save_version(outf, save_version::current_bones());

    // extended_version just pads the version out to four 32-bit words.
    // This makes the bones file compatible with Hearse with no extra
    // munging needed.

    // Use a single signature 16-bit word to indicate that this is
    // Stone Soup and to disambiguate this (unmunged) bones file
    // from the munged bones files offered by the old Crawl-aware
    // hearse.pl. Crawl-aware hearse.pl will prefix the bones file
    // with the first 16-bits of the Crawl version, and the following
    // 7 16-bit words set to 0.
    marshallShort(outf, GHOST_SIGNATURE);

    // Write the three remaining 32-bit words of padding.
    for (int i = 0; i < 3; ++i)
        marshallInt(outf, 0);
}

static void _write_tagged_chunk(const string &chunkname, tag_type tag)
{
    writer outf(you.save, chunkname);

    write_save_version(outf, save_version::current());
    tag_write(tag, outf);
}

static int _get_dest_stair_type(dungeon_feature_type stair_taken,
                                bool &find_first)
{
    // Order is important here.
    if (stair_taken == DNGN_EXIT_ABYSS)
    {
        find_first = false;
        return DNGN_EXIT_DUNGEON;
    }

    if (feat_is_hell_subbranch_exit(stair_taken))
        return DNGN_EXIT_HELL;

    if (player_in_hell() && feat_is_stone_stair_down(stair_taken))
    {
        find_first = false;
        return branches[you.where_are_you].exit_stairs;
    }

    if (feat_is_stone_stair(stair_taken))
    {
        switch (stair_taken)
        {
        case DNGN_STONE_STAIRS_UP_I: return DNGN_STONE_STAIRS_DOWN_I;
        case DNGN_STONE_STAIRS_UP_II: return DNGN_STONE_STAIRS_DOWN_II;
        case DNGN_STONE_STAIRS_UP_III: return DNGN_STONE_STAIRS_DOWN_III;

        case DNGN_STONE_STAIRS_DOWN_I: return DNGN_STONE_STAIRS_UP_I;
        case DNGN_STONE_STAIRS_DOWN_II: return DNGN_STONE_STAIRS_UP_II;
        case DNGN_STONE_STAIRS_DOWN_III: return DNGN_STONE_STAIRS_UP_III;

        default: die("unknown stone stair %d", stair_taken);
        }
    }

    if (feat_is_escape_hatch(stair_taken) || stair_taken == DNGN_TRAP_SHAFT)
        return stair_taken;

    if (stair_taken == DNGN_ENTER_DIS
        || stair_taken == DNGN_ENTER_GEHENNA
        || stair_taken == DNGN_ENTER_COCYTUS
        || stair_taken == DNGN_ENTER_TARTARUS)
    {
        return player_in_hell() ? branches[you.where_are_you].exit_stairs
                                : stair_taken;
    }

    if (feat_is_branch_exit(stair_taken))
    {
        for (branch_iterator it; it; ++it)
            if (it->exit_stairs == stair_taken)
                return it->entry_stairs;
        die("entrance corresponding to exit %d not found", stair_taken);
    }

    if (feat_is_branch_entrance(stair_taken))
    {
        for (branch_iterator it; it; ++it)
            if (it->entry_stairs == stair_taken)
                return it->exit_stairs;
        die("return corresponding to entry %d not found", stair_taken);
    }
#if TAG_MAJOR_VERSION == 34
    if (stair_taken == DNGN_ENTER_LABYRINTH)
    {
        // dgn_find_nearby_stair uses special logic for labyrinths.
        return DNGN_ENTER_LABYRINTH;
    }
#endif

    if (feat_is_portal_entrance(stair_taken))
        return DNGN_STONE_ARCH;

    // Note: stair_taken can equal things like DNGN_FLOOR
    // Just find a nice empty square.
    find_first = false;
    return DNGN_FLOOR;
}

static bool _nonfriendly_nearby(coord_def p)
{
    for (monster_near_iterator mi(p); mi; ++mi)
        if (!mi->friendly())
            return true;
    return false;
}

static bool _shaft_safely()
{
    // Loosely modelled on bring_to_safety(). Perhaps should be unified.
    for (int tries = 0; tries < 1000; ++tries)
    {
        coord_def pos;
        pos.x = random2(GXM);
        pos.y = random2(GYM);

        if (!in_bounds(pos)
            || is_feat_dangerous(env.grid(pos), true)
            || cloud_at(pos) // XXX: ignore if is_harmless_cloud?
            || monster_at(pos)
            || env.pgrid(pos) & FPROP_NO_TELE_INTO
            || slime_wall_neighbour(pos)
            || _nonfriendly_nearby(pos))
        {
            continue;
        }

        you.moveto(pos);
        return true;
    }

    return false;
}

static void _place_player_on_stair(int stair_taken, const coord_def& dest_pos,
                                   const string &hatch_name)

{
    bool find_first = true;
    dungeon_feature_type stair_type = static_cast<dungeon_feature_type>(
            _get_dest_stair_type(static_cast<dungeon_feature_type>(stair_taken),
                                 find_first));

    if (stair_type == DNGN_TRAP_SHAFT && you.where_are_you == BRANCH_DUNGEON)
    {
        // Shafts are scary enough in D without putting you near mons.
        if (_shaft_safely())
            return;
        // If we can't find a safe place, fall through to default random placement.
    }
    you.moveto(dgn_find_nearby_stair(stair_type, dest_pos, find_first,
                                     hatch_name));
}

static void _clear_env_map()
{
    env.map_knowledge.init(map_cell());
    env.map_forgotten.reset();
}

static bool _grab_follower_at(const coord_def &pos, bool can_follow)
{
    if (pos == you.pos())
        return false;

    monster* fol = monster_at(pos);
    if (!fol || !fol->alive() || fol->incapacitated())
        return false;

    // only H's ancestors can follow into portals & similar.
    if (!can_follow && !mons_is_hepliaklqana_ancestor(fol->type))
        return false;

    // The monster has to already be tagged in order to follow.
    if (!testbits(fol->flags, MF_TAKING_STAIRS))
        return false;

    // If a monster that can't use stairs was marked as a follower,
    // it's because it's an ally and there might be another ally
    // behind it that might want to push through.
    // This means we don't actually send it on transit, but we do
    // return true, so adjacent real followers are handled correctly. (jpeg)
    if (!mons_can_use_stairs(*fol))
        return true;

    level_id dest = level_id::current();

    dprf("%s is following to %s.", fol->name(DESC_THE, true).c_str(),
         dest.describe().c_str());
    bool could_see = you.can_see(*fol);
    fol->set_transit(dest);
    fol->destroy_inventory();
    monster_cleanup(fol);
    if (could_see)
        view_update_at(pos);
    return true;
}

static void _grab_followers()
{
    const bool can_follow = branch_allows_followers(you.where_are_you);

    int non_stair_using_allies = 0;
    int non_stair_using_summons = 0;

    monster* dowan = nullptr;
    monster* duvessa = nullptr;

    // Handle some hacky cases
    for (adjacent_iterator ai(you.pos()); ai; ++ai)
    {
        monster* fol = monster_at(*ai);
        if (fol == nullptr)
            continue;

        if (mons_is_mons_class(fol, MONS_DUVESSA) && fol->alive())
            duvessa = fol;

        if (mons_is_mons_class(fol, MONS_DOWAN) && fol->alive())
            dowan = fol;

        if (fol->wont_attack() && !mons_can_use_stairs(*fol))
        {
            non_stair_using_allies++;
            if (fol->is_summoned() || mons_is_conjured(fol->type))
                non_stair_using_summons++;
        }
    }

    // Deal with Dowan and Duvessa here.
    if (dowan && duvessa)
    {
        if (!testbits(dowan->flags, MF_TAKING_STAIRS)
            || !testbits(duvessa->flags, MF_TAKING_STAIRS))
        {
            dowan->flags &= ~MF_TAKING_STAIRS;
            duvessa->flags &= ~MF_TAKING_STAIRS;
        }
    }
    else if (dowan && !duvessa)
    {
        if (!dowan->props.exists(CAN_CLIMB_KEY))
            dowan->flags &= ~MF_TAKING_STAIRS;
    }
    else if (!dowan && duvessa)
    {
        if (!duvessa->props.exists(CAN_CLIMB_KEY))
            duvessa->flags &= ~MF_TAKING_STAIRS;
    }

    if (can_follow && non_stair_using_allies > 0)
    {
        // Summons won't follow and will time out.
        if (non_stair_using_summons > 0)
        {
            mprf("Your summoned %s left behind.",
                 non_stair_using_allies > 1 ? "allies are" : "ally is");
        }
        else
        {
            // Permanent undead are left behind but stay.
            mprf("Your mindless puppet%s behind to rot.",
                 non_stair_using_allies > 1 ? "s stay" : " stays");
        }
    }

    bool visited[GXM][GYM];
    memset(&visited, 0, sizeof(visited));

    vector<coord_def> places[2] = { { you.pos() }, {} };
    int place_set = 0;
    while (!places[place_set].empty())
    {
        for (const coord_def &p : places[place_set])
        {
            for (adjacent_iterator ai(p); ai; ++ai)
            {
                if (visited[ai->x][ai->y])
                    continue;

                visited[ai->x][ai->y] = true;
                if (_grab_follower_at(*ai, can_follow))
                    places[!place_set].push_back(*ai);
            }
        }
        places[place_set].clear();
        place_set = !place_set;
    }

    // Clear flags of monsters that didn't follow.
    for (auto &mons : menv_real)
    {
        if (!mons.alive())
            continue;
        if (mons.type == MONS_BATTLESPHERE)
            end_battlesphere(&mons, false);
        if (mons.type == MONS_SPECTRAL_WEAPON)
            end_spectral_weapon(&mons, false);
        mons.flags &= ~MF_TAKING_STAIRS;
    }
}

static void _do_lost_monsters()
{
    // Uniques can be considered wandering Pan just like you, so they're not
    // gone forever. The likes of Cerebov won't be generated elsewhere, but
    // there's no need to special-case that.
    if (player_in_branch(BRANCH_PANDEMONIUM))
        for (monster_iterator mi; mi; ++mi)
            if (mons_is_unique(mi->type) && !(mi->flags & MF_TAKING_STAIRS))
                you.unique_creatures.set(mi->type, false);
}

// Should be called after _grab_followers(), so that items carried by
// followers won't be considered lost.
static void _do_lost_items()
{
    for (const auto &item : env.item)
        if (item.defined() && item.pos != ITEM_IN_INVENTORY)
            item_was_lost(item);
}

/**
 * Perform cleanup when leaving a level.
 *
 * If returning to the previous level on the level stack (e.g. when leaving the
 * abyss), pop it off the stack. Delete non-permanent levels. Also check to be
 * sure no loops have formed in the level stack, and, for Fedhasites, rots any
 * corpses left behind.
 *
 * @param stair_taken   The means used to leave the last level.
 * @param old_level     The ID of the previous level.
 * @param return_pos    Set to the level entrance, if popping a stack level.
 * @return Whether the level was popped onto the stack.
 */
static bool _leave_level(dungeon_feature_type stair_taken,
                         const level_id& old_level, coord_def *return_pos)
{
    bool popped = false;

    if (!you.level_stack.empty()
        && you.level_stack.back().id == level_id::current())
    {
        *return_pos = you.level_stack.back().pos;
        you.level_stack.pop_back();
        env.level_state |= LSTATE_DELETED;
        popped = true;
    }
    else if (stair_taken == DNGN_TRANSIT_PANDEMONIUM
             || stair_taken == DNGN_EXIT_THROUGH_ABYSS
             || stair_taken == DNGN_STONE_STAIRS_DOWN_I
             && old_level.branch == BRANCH_ZIGGURAT
             || old_level.branch == BRANCH_ABYSS)
    {
        env.level_state |= LSTATE_DELETED;
    }

    if (is_level_on_stack(level_id::current())
        && !player_in_branch(BRANCH_ABYSS))
    {
        vector<string> stack;
        for (level_pos lvl : you.level_stack)
            stack.push_back(lvl.id.describe());
        if (you.wizard)
        {
            // warn about breakage so testers know it's an abnormal situation.
            mprf(MSGCH_ERROR, "Error: you smelly wizard, how dare you enter "
                 "the same level (%s) twice! It will be trampled upon return.\n"
                 "The stack has: %s.",
                 level_id::current().describe().c_str(),
                 comma_separated_line(stack.begin(), stack.end(),
                                      ", ", ", ").c_str());
        }
        else
        {
            die("Attempt to enter a portal (%s) twice; stack: %s",
                level_id::current().describe().c_str(),
                comma_separated_line(stack.begin(), stack.end(),
                                     ", ", ", ").c_str());
        }
    }

    return popped;
}

static void _place_player_randomly()
{
    // This copies the logic in the core of you_teleport_now().
    coord_def newpos;
    int tries = 500;
    do
    {
        newpos = random_in_bounds();
    }
    while (--tries > 0
           && (cell_vetoes_teleport(newpos, false)
               || testbits(env.pgrid(newpos), FPROP_NO_TELE_INTO)));
    if (tries == 0) // yikes!
        die("couldn't find a rising flame destination");

    // outta the way!
    monster* const mons = monster_at(newpos);
    if (mons)
        mons->teleport(true);
    you.moveto(newpos);
}

/**
 * Move the player to the appropriate entrance location in a level.
 *
 * @param stair_taken   The means used to leave the last level.
 * @param return_pos    The location of the entrance portal, if applicable.
 * @param dest_pos      The player's location on the last level.
 */
static void _place_player(dungeon_feature_type stair_taken,
                          const coord_def &return_pos,
                          const coord_def &dest_pos, const string &hatch_name)
{
    if (player_in_branch(BRANCH_ABYSS))
        you.moveto(ABYSS_CENTRE);
    else if (!return_pos.origin())
        you.moveto(return_pos);
    else if (stair_taken == DNGN_ALTAR_IGNIS) // hack: we're rocketeers!
        _place_player_randomly();
    else
        _place_player_on_stair(stair_taken, dest_pos, hatch_name);

    // Don't return the player into walls, deep water, or a trap.
    for (distance_iterator di(you.pos(), true, false); di; ++di)
        if (you.is_habitable_feat(env.grid(*di))
            && !is_feat_dangerous(env.grid(*di), true)
            && !feat_is_trap(env.grid(*di)))
        {
            if (you.pos() != *di)
                you.moveto(*di);
            break;
        }

    // This should fix the "monster occurring under the player" bug.
    monster *mon = monster_at(you.pos());
    if (mon && !fedhas_passthrough(mon))
    {
        for (distance_iterator di(you.pos()); di; ++di)
        {
            if (!monster_at(*di) && mon->is_habitable(*di))
            {
                mon->move_to_pos(*di);
                return;
            }
        }

        dprf("%s under player and can't be moved anywhere; killing",
             mon->name(DESC_PLAIN).c_str());
        monster_die(*mon, KILL_DISMISSED, NON_MONSTER);
        // XXX: do we need special handling for uniques...?
    }
}

// Update the trackers after the player changed level.
// note: also run on load for some reason in startup.cc
void trackers_init_new_level()
{
    travel_init_new_level();
}

static string _get_hatch_name()
{
    vector <map_marker *> markers;
    markers = find_markers_by_prop(HATCH_NAME_PROP);
    for (auto m : markers)
    {
        if (m->pos == you.pos())
        {
            string name = m->property(HATCH_NAME_PROP);
            ASSERT(!name.empty());
            return name;
        }
    }
    return "";
}

static const string VISITED_LEVELS_KEY = "visited_levels";

#if TAG_MAJOR_VERSION == 34
// n.b. these functions are in files.cc largely because this is where the fixup
// needs to happen.
// before pregeneration, whether the level had been visited was synonymous with
// whether it had been visited, but after, we need to track this information
// more directly. It is also inferrable from turns_on_level, but you can't get
// at that very easily without fully loading the level.
// no need for a minor version here, though there will be a brief window of
// offline pregen games that this doesn't handle right -- they will get things
// like broken runelock. (In principle this fixup could be done by loading
// each level and checking turns, but it's not worth the trouble for these few
// games.)
static void _fixup_visited_from_package()
{
    // for games started later than this fixup, this prop is initialized in
    // player::player
    CrawlHashTable &visited = you.props[VISITED_LEVELS_KEY].get_table();
    if (visited.size()) // only 0 for upgrades, or before entering D:1
        return;
    vector<level_id> levels = all_dungeon_ids();
    for (const level_id &lid : levels)
        if (is_existing_level(lid))
            visited[lid.describe()] = true;
}
#endif

void player::set_level_visited(const level_id &level)
{
    auto &visited = props[VISITED_LEVELS_KEY].get_table();
    visited[level.describe()] = true;
}

/**
 * Has the player visited the level currently stored in the save under the id
 * `level`, if there is one? Returns false if there isn't one. This stores
 * *token level* visited state, not type-level -- it does not answer questions
 * like, e.g. has the player ever visited a trove? For that, see place_info.
 * This distinction matters mainly for portal branches, especially ones that can
 * be revisited, e.g. Pan levels and zigs.
 */
bool player::level_visited(const level_id &level)
{
    // `is_existing_level` is not reliable after the game end, because the
    // save no longer exists, so we ignore it for printing morgues
    if (!is_existing_level(level) && you.save)
        return false;
    const auto &visited = props[VISITED_LEVELS_KEY].get_table();
    return visited.exists(level.describe());
}

static void _generic_level_reset()
{
    // TODO: can more be pulled into here?

    you.prev_targ = MHITNOT;
    you.prev_grd_targ.reset();

    // Lose all listeners.
    dungeon_events.clear();
    clear_travel_trail();
}


// used to resolve generation order for cases where a single level has multiple
// portals. This currently should only include portals that can appear at most
// once.
static const vector<branch_type> portal_generation_order =
{
    BRANCH_SEWER,
    BRANCH_OSSUARY,
    BRANCH_ICE_CAVE,
    BRANCH_VOLCANO,
    BRANCH_BAILEY,
    BRANCH_GAUNTLET,
#if TAG_MAJOR_VERSION == 34
    BRANCH_LABYRINTH,
#endif
    // do not pregenerate bazaar (TODO: this is non-ideal)
    // do not pregenerate trove
    BRANCH_WIZLAB,
    BRANCH_DESOLATION,
};

void update_portal_entrances()
{
    unordered_set<branch_type, std::hash<int>> seen_portals;
    auto const cur_level = level_id::current();
    // add any portals not currently registered
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        dungeon_feature_type feat = env.grid(*ri);
        // excludes pan, hell, abyss.
        if (feat_is_portal_entrance(feat) && !feature_mimic_at(*ri))
        {
            level_id whither = stair_destination(feat, "", false);
            if (whither.branch == BRANCH_ZIGGURAT // not (quite) pregenerated
                || whither.branch == BRANCH_TROVE // not pregenerated
                || whither.branch == BRANCH_BAZAAR) // multiple bazaars possible
            {
                continue; // handle these differently
            }
            dprf("Setting up entry for %s.", whither.describe().c_str());
            ASSERT(count(portal_generation_order.begin(),
                         portal_generation_order.end(),
                         whither.branch) == 1);
            if (brentry[whither.branch] != level_id())
            {
                mprf(MSGCH_ERROR, "Second portal entrance for %s!",
                    whither.describe().c_str());
            }
            brentry[whither.branch] = cur_level;
            seen_portals.insert(whither.branch);
        }
    }
    // clean up any portals that aren't actually here -- comes up for wizmode
    // and test mode cases.
    for (auto b : portal_generation_order)
        if (!seen_portals.count(b) && brentry[b] == cur_level)
            brentry[b] = level_id();
}

void reset_portal_entrances()
{
    for (auto b : portal_generation_order)
        if (brentry[b].is_valid())
            brentry[b] = level_id();
}

/**
 * Generate portals relative to the current level. This function does not clean
 * up builder state.
 *
 * @return the number of levels that generated, or -1 if the builder failed.
 */
static int _generate_portal_levels()
{
    // find any portals that branch off of the current level.
    level_id here = level_id::current();
    vector<level_id> to_build;
    for (auto b : portal_generation_order)
        if (brentry[b] == here)
            for (int i = 1; i <= brdepth[b]; i++)
                to_build.push_back(level_id(b, i));

    int count = 0;
    for (auto lid : to_build)
    {
        if (!generate_level(lid))
        {
            // Should this crash? Reaching this case means that multiple
            // entrances to a non-reusable portal generated.
            if (you.save->has_chunk(lid.describe()))
                mprf(MSGCH_ERROR, "Portal %s already exists!", lid.describe().c_str());
            else
                return -1;
        }
        count++;
    }
    return count;
}

/**
 * Ensure that the level given by `l` is generated. This does not do much in
 * the way of cleanup, and the caller must ensure the player ends up somewhere
 * sensible afterwards (this will not place the player, and will wipe out their
 * current location state if a level is built). Does not do anything if the
 * save already contains the relevant level.
 *
 * This function may generate multiple levels: any necessary portal levels
 * needed for `l` are built also.
 *
 * @param l the level to try to build.
 * @return whether the required builder steps succeeded, if there are any;
 * false means that either there was a builder error, or the level already
 * exists. This can be checked by looking at whether the save chunk exists.
 */
bool generate_level(const level_id &l)
{
    const string level_name = l.describe();
    if (you.save->has_chunk(level_name))
        return false;

    unwind_var<int> you_depth(you.depth, l.depth);
    unwind_var<branch_type> you_branch(you.where_are_you, l.branch);
    unwind_var<coord_def> you_saved_position(you.position);
    you.position.reset();

    // simulate a reasonable stair to enter the level with
    const dungeon_feature_type stair_taken =
          you.depth == 1
        ? (you.where_are_you == BRANCH_DUNGEON
           ? DNGN_UNSEEN
           : branches[you.where_are_you].entry_stairs)
        : DNGN_STONE_STAIRS_DOWN_I;

    unwind_var<dungeon_feature_type> stair(you.transit_stair, stair_taken);
    // TODO how necessary is this?
    unwind_bool ylev(you.entering_level, true);
    // n.b. crawl_state.generating_level is handled in builder

    _generic_level_reset();
    delete_all_clouds();
    los_changed(); // invalidate the los cache, which impacts monster placement

    // initialize env for builder
    env.turns_on_level = -1;
    tile_init_default_flavour();
    tile_clear_flavour();
    tile_env.names.clear();
    _clear_env_map();

    // finally -- everything is set up, call the builder.
    dprf("Generating new level for '%s'.", level_name.c_str());
    if (!builder(true))
        return false;

    auto &vault_list =  you.vault_list[level_id::current()];
#ifdef DEBUG
    // places where a level can generate multiple times.
    // could add portals to this list for debugging purposes?
    if (   you.where_are_you == BRANCH_ABYSS
        || you.where_are_you == BRANCH_PANDEMONIUM
        || you.where_are_you == BRANCH_BAZAAR
        || you.where_are_you == BRANCH_ZIGGURAT)
    {
        vault_list.push_back("[gen]");
    }
#endif
    const auto &level_vaults = level_vault_names();
    vault_list.insert(vault_list.end(),
                        level_vaults.begin(), level_vaults.end());

    // initialize env for a new level
    env.turns_on_level = 0;
    env.sanctuary_pos  = coord_def(-1, -1);
    env.sanctuary_time = 0;
    env.markers.init_all(); // init first, activation happens when entering
    show_update_emphasis(); // Clear map knowledge stair emphasis in env.
    update_portal_entrances();

    // save the level and associated env state
    save_level(level_id::current());

    const string save_name = level_id::current().describe(); // should be same as level_name...

    // generate levels for all portals that branch off from here
    int portal_level_count = _generate_portal_levels();
    if (portal_level_count == -1)
        return false; // something failed, bail immediately
    else if (portal_level_count > 0)
    {
        // if portals were generated, we're currently elsewhere. Switch back to
        // the level generated before the portals.
        ASSERT(you.save->has_chunk(save_name));
        dprf("Reloading new level '%s'.", save_name.c_str());
        _restore_tagged_chunk(you.save, save_name, TAG_LEVEL,
            "Level file is invalid.");
    }
    // Did the generation process actually manage to place the player? This is
    // a useful sanity check, and also is necessary for the initial loading
    // process.
    you.on_current_level = (you_depth.original_value() == l.depth
                            && you_branch.original_value() == l.branch);
    return true;
}

// bel's original proposal generated D to lair depth, then lair, then D
// to orc depth, then orc, then the rest of D. I have simplified this to
// just generate whole branches at a time -- I am not sure how much real
// impact this has, though it does mean a pregen popup when the player enters
// lair, typically.
//
// Portals are handled via `portal_generation_order`, and generated as-needed
// with the level they appear on.
//
// We generate temple first so as to save the player a popup when they find it
// in mid-dungeon; it's fully decided in game setup and shouldn't interact with
// rng for other branches anyways.
//
// How should this relate to logical_branch_order etc?
static const vector<branch_type> branch_generation_order =
{
    BRANCH_TEMPLE,
    BRANCH_DUNGEON,
    BRANCH_LAIR,
    BRANCH_ORC,
    BRANCH_SPIDER,
    BRANCH_SNAKE,
    BRANCH_SHOALS,
    BRANCH_SWAMP,
    BRANCH_VAULTS,
    BRANCH_CRYPT,
    BRANCH_DEPTHS,
    BRANCH_VESTIBULE,
    BRANCH_ELF,
    BRANCH_ZOT,
    BRANCH_SLIME,
    BRANCH_TOMB,
    BRANCH_TARTARUS,
    BRANCH_COCYTUS,
    BRANCH_DIS,
    BRANCH_GEHENNA,
    BRANCH_PANDEMONIUM,
    BRANCH_ZIGGURAT,
    NUM_BRANCHES,
};

static bool _branch_pregenerates(branch_type b)
{
    if (!you.deterministic_levelgen)
        return false;
    if (b == NUM_BRANCHES || !brentry[b].is_valid() && is_random_subbranch(b))
        return false;
    return count(branch_generation_order.begin(),
        branch_generation_order.end(), b) > 0;
}

/**
* Generate dungeon branches in a stable order until the level `stopping_point`
* is found; `stopping_point` will be generated if it doesn't already exist. If
* it does exist, the function is a noop.
*
* If `stopping_point` is not in the generation order, it will be generated on
* its own.
*
* To generate all generatable levels, pass a level_id with NUM_BRANCHES as the
* branch.
*
* @return whether stopping_point generated; if stopping_point is NUM_BRANCHES,
* whether the full pregen list completed. This will return false if all needed
* levels are already generated, so the caller should check whether false is an
* error case or trivial success (using the save chunk).
*/
bool pregen_dungeon(const level_id &stopping_point)
{
    // TODO: the is_valid() check here doesn't look quite right to me, but so
    // far I can't get it to break anything...
    if (stopping_point.is_valid()
        || stopping_point.branch != NUM_BRANCHES &&
           is_random_subbranch(stopping_point.branch) && you.wizard)
    {
        if (you.save->has_chunk(stopping_point.describe()))
            return false;

        if (!_branch_pregenerates(stopping_point.branch))
            return generate_level(stopping_point);
    }

    vector<level_id> to_generate;
    bool at_end = false;
    for (auto br : branch_generation_order)
    {
        if (br == BRANCH_ZIGGURAT &&
            stopping_point.branch == BRANCH_ZIGGURAT)
        {
            // zigs delete levels as they go, so don't catchup when we're
            // already in one. Zigs are only handled this way so that everything
            // else generates first.
            to_generate.push_back(stopping_point);
            continue;
        }
        // TODO: why is dungeon invalid? it's not set up properly in
        // `initialise_branch_depths` for some reason. The vestibule is invalid
        // because its depth isn't set until the player actually enters a
        // portal, similarly for other portal branches.
        if (br < NUM_BRANCHES &&
            (brentry[br].is_valid()
             || br == BRANCH_DUNGEON || br == BRANCH_VESTIBULE
             || !is_connected_branch(br)))
        {
            for (int i = 1; i <= brdepth[br]; i++)
            {
                level_id new_level = level_id(br, i);
                // skip any levels that have already generated.
                if (you.save->has_chunk(new_level.describe()))
                    continue;
                to_generate.push_back(new_level);

                if (br == stopping_point.branch
                    && (i == stopping_point.depth || i == brdepth[br]))
                {
                    at_end = true;
                    break;
                }
            }
        }
        if (at_end)
            break;
    }

    if (to_generate.size() == 0)
    {
        dprf("levelgen: No valid levels to generate.");
        return false;
    }
    // TODO: some levels are very slow (typically in depths), and a popup might
    // be helpful to the player. But is there a good way to tell?
    else if (to_generate.size() == 1)
        return generate_level(to_generate[0]); // no popup for this case
    else
    {
        // be sure that AK start doesn't interfere with the builder
        unwind_var<game_chapter> chapter(you.chapter, CHAPTER_ORB_HUNTING);

        ui::progress_popup progress("Generating dungeon...\n\n", 35);
        progress.advance_progress();

        for (const level_id &new_level : to_generate)
        {
            string status = "\nbuilding ";

            switch (new_level.branch)
            {
            case BRANCH_SPIDER:
            case BRANCH_SNAKE:
                status += "a lair branch";
                break;
            case BRANCH_SHOALS:
            case BRANCH_SWAMP:
                status += "another lair branch";
                break;
            default:
                status += branches[new_level.branch].longname;
                break;
            }
            progress.set_status_text(status);
            dprf("Pregenerating %s:%d",
                branches[new_level.branch].abbrevname, new_level.depth);
            progress.advance_progress();

            // (save chunk existence is checked above, so isn't relevant here)
            if (!generate_level(new_level))
                return false; // level failed to generate -- bail immediately
        }

        return true;
    }
}

static void _rescue_player_from_wall()
{
    // n.b. you.wizmode_teleported_into_rock would be better, but it is not
    // actually saved.
    if (cell_is_solid(you.pos()) && !you.wizard)
    {
        // if the player has somehow gotten into a wall, there may have been
        // a fairly non-trivial crash, putting the player at some arbitrary
        // position relative to where they were. Rescue them by trying to find
        // a seen staircase, with a clear space near the wall as just a
        // a fallback.
        mprf(MSGCH_ERROR, "Emergency fixup: removing player from wall "
                          "at %d,%d. Please report this as a bug!",
                          you.pos().x, you.pos().y);
        vector<coord_def> upstairs;
        vector<coord_def> downstairs;
        coord_def backup_clear_pos(-1,-1);
        for (distance_iterator di(you.pos()); di; ++di)
        {
            // just find any clear square as a backup for really weird cases.
            if (!in_bounds(backup_clear_pos) && !cell_is_solid(*di))
                backup_clear_pos = *di;
            // TODO: in principle this should use env.map_forgotten if it
            // exists, but I'm not sure that is worth the trouble.
            if (feat_is_stair(env.grid(*di)) && env.map_seen(*di))
            {
                const command_type dir = feat_stair_direction(env.grid(*di));
                if (dir == CMD_GO_UPSTAIRS)
                    upstairs.push_back(*di);
                else if (dir == CMD_GO_DOWNSTAIRS)
                    downstairs.push_back(*di);
            }
        }
        coord_def target = backup_clear_pos;
        if (upstairs.size())
            target = upstairs[0];
        else if (downstairs.size())
            target = downstairs[0];
        if (!in_bounds(target) && player_in_branch(BRANCH_ABYSS))
        {
            // something is *seriously* messed up. This can happen if the game
            // crashed on an AK start where the initial map wasn't saved.
            // Because it is abyss, it is relatively safe to just move a player
            // to an arbitrary point and let abyss shift take over. (AK starts,
            // the main case, can also escape from the abyss at this point.)
            const coord_def emergency(1,1);
            env.grid(emergency) = DNGN_FLOOR;
            target = emergency;
        }
        // if things get this messed up, don't make them worse
        ASSERT(in_bounds(target));
        you.moveto(target);
    }
}

/**
 * Load the current level.
 *
 * @param stair_taken   The means used to enter the level.
 * @param load_mode     Whether the level is being entered, examined, etc.
 * @return Whether a new level was created.
 */
bool load_level(dungeon_feature_type stair_taken, load_mode_type load_mode,
                const level_id& old_level)
{
    const string level_name = level_id::current().describe();
    if (!you.save->has_chunk(level_name) && load_mode == LOAD_VISITOR)
        return false;

    const bool fast = load_mode == LOAD_ENTER_LEVEL_FAST;
    if (fast)
        load_mode = LOAD_ENTER_LEVEL;

    const bool make_changes =
        (load_mode == LOAD_START_GAME || load_mode == LOAD_ENTER_LEVEL);

#if TAG_MAJOR_VERSION == 34
    // fixup saves that don't have this prop initialized.
    if (load_mode == LOAD_RESTART_GAME)
        _fixup_visited_from_package();
#endif

    coord_def return_pos; //TODO: initialize to null

    string hatch_name = "";
    if (feat_is_escape_hatch(stair_taken))
        hatch_name = _get_hatch_name();

    // Did we get here by popping the level stack?
    bool popped = false;
    if (load_mode != LOAD_VISITOR)
        popped = _leave_level(stair_taken, old_level, &return_pos);

    unwind_var<dungeon_feature_type> stair(
        you.transit_stair, stair_taken, DNGN_UNSEEN);
    unwind_bool ylev(you.entering_level, load_mode != LOAD_VISITOR, false);

#ifdef DEBUG_LEVEL_LOAD
    mprf(MSGCH_DIAGNOSTICS, "Loading... branch: %d, level: %d",
                            you.where_are_you, you.depth);
#endif

    // Save position for hatches to place a marker on the destination level.
    coord_def dest_pos = you.pos();

    _generic_level_reset();

    // We clear twice - on save and on load.
    // Once would be enough...
    if (make_changes)
        delete_all_clouds();

    // This block is to grab followers and save the old level to disk.
    if (load_mode == LOAD_ENTER_LEVEL)
    {
        dprf("stair_taken = %s", dungeon_feature_name(stair_taken));
        // Not the case normally, but can happen during recovery of damaged
        // games.
        if (old_level.depth != -1)
        {
            _grab_followers();

            if (env.level_state & LSTATE_DELETED)
                delete_level(old_level), dprf("<lightmagenta>Deleting level.</lightmagenta>");
            else
                save_level(old_level);
        }

        // The player is now between levels.
        you.position.reset();

        update_companions();
    }

#ifdef USE_TILE
    if (load_mode != LOAD_VISITOR)
    {
        tiles.clear_minimap();
        crawl_view_buffer empty_vbuf;
        tiles.load_dungeon(empty_vbuf, crawl_view.vgrdc);
    }
#endif

    // GENERATE new level(s) when the file can't be opened:
    if (pregen_dungeon(level_id::current()))
    {
        // sanity check: did the pregenerator leave us on the requested level? If
        // this fails via a bug, and this ASSERT isn't here, something incorrect
        // will get saved under the chunk for the current level (typically the
        // last level in the pregen sequence, which is zig 27).
        ASSERT(you.on_current_level);
    }
    else
    {
        if (!you.save->has_chunk(level_name))
        {
            // The builder has failed somewhere along the way, and couldn't get
            // to the stopping point. The most likely (only?) cause is that
            // there were too many vetoes, which can occasionally happen in
            // Depths. To deal with this we force save and crash.
            //
            // Basically this will ensure that the rng state after the
            // attempt is saved, making resuming likely to be possible. Setting
            // `you.on_current_level` means that the save has the player on a
            // non-generated level. Reloading a save in this state triggers
            // the levelgen sequence needed to put them there.

            // ensure these props can't be saved, otherwise the save is likely
            // to become unloadable
            if (you.props.exists(FORCE_MAP_KEY)
                || you.props.exists(FORCE_MINIVAULT_KEY))
            {
                // TODO: is there a good way of doing this without the crash?
                mprf(MSGCH_ERROR, "&P with '%s' failed; clearing force props and trying with random generation next.",
                    you.props.exists(FORCE_MAP_KEY)
                    ? you.props[FORCE_MAP_KEY].get_string().c_str()
                    : you.props[FORCE_MINIVAULT_KEY].get_string().c_str());
                // without a flush this mprf doesn't get saved
                flush_prev_message();
                you.props.erase(FORCE_MINIVAULT_KEY);
                you.props.erase(FORCE_MAP_KEY);
            }

            if (crawl_state.need_save)
            {
                you.on_current_level = true;
                save_game(false);
            }

            die("Builder failure while generating '%s'!\nLast builder error: '%s'",
                level_id::current().describe().c_str(),
                crawl_state.last_builder_error.c_str());
        }

        dprf("Loading old level '%s'.", level_name.c_str());
        _restore_tagged_chunk(you.save, level_name, TAG_LEVEL, "Level file is invalid.");
        if (load_mode != LOAD_VISITOR)
            you.on_current_level = true;
        _redraw_all(); // TODO why is there a redraw call here?
    }

    const bool just_created_level = !you.level_visited(level_id::current());

    // Clear map knowledge stair emphasis.
    show_update_emphasis();

    // Shouldn't happen, but this is too unimportant to assert.
    deleteAll(env.final_effects);

    los_changed();

    if (load_mode != LOAD_VISITOR)
        you.set_level_visited(level_id::current());

    // Markers must be activated early, since they may rely on
    // events issued later, e.g. DET_ENTERING_LEVEL or
    // the DET_TURN_ELAPSED from update_level.
    if (make_changes || load_mode == LOAD_RESTART_GAME)
        env.markers.activate_all();

    if (make_changes && env.elapsed_time && !just_created_level)
        update_level(you.elapsed_time - env.elapsed_time);

    // Apply all delayed actions, if any. TODO: logic for marshalling this is
    // kind of odd.
    // TODO: does this need make_changes?
    if (just_created_level)
        env.dactions_done = 0;

    // Here's the second cloud clearing, on load (see above).
    if (make_changes)
    {
        // this includes various things that are irrelevant for new levels, but
        // also some things that aren't, such as bribe branch.
        catchup_dactions();

        delete_all_clouds();

        _place_player(stair_taken, return_pos, dest_pos, hatch_name);
    }

    crawl_view.set_player_at(you.pos(), load_mode != LOAD_VISITOR);

    // Actually "move" the followers if applicable.
    if (load_mode == LOAD_ENTER_LEVEL)
        place_followers();

    // Load monsters in transit.
    if (load_mode == LOAD_ENTER_LEVEL)
        place_transiting_monsters();

    if (just_created_level && make_changes)
        replace_boris();

    if (make_changes)
    {
        // Tell stash-tracker and travel that we've changed levels.
        trackers_init_new_level();
        travel_cache.flush_invalid_waypoints();
        tile_new_level(just_created_level);
    }
    else if (load_mode == LOAD_RESTART_GAME)
    {
        _rescue_player_from_wall();
        // Travel needs initialize some things on reload, too.
        travel_init_load_level();
    }

    _redraw_all();

    if (load_mode != LOAD_VISITOR)
        dungeon_events.fire_event(DET_ENTERING_LEVEL);

    // Things to update for player entering level
    if (load_mode == LOAD_ENTER_LEVEL)
    {
        // new stairs have less wary monsters, and we don't
        // want them to attack players quite as soon.
        // (just_created_level only relevant if we crashed.)
        you.time_taken *= fast || just_created_level ? 1 : 2;

        you.time_taken = div_rand_round(you.time_taken * 3, 4);

        dprf("arrival time: %d", you.time_taken);

        if (just_created_level)
            run_map_epilogues();
    }

    // Save the created/updated level out to disk:
    if (make_changes)
        save_level(level_id::current());

    setup_environment_effects();

    setup_vault_mon_list();

    // Inform user of level's annotation.
    if (load_mode != LOAD_VISITOR
        && !get_level_annotation().empty()
        && !crawl_state.level_annotation_shown)
    {
        mprf(MSGCH_PLAIN, YELLOW, "Level annotation: %s",
             get_level_annotation().c_str());
    }

    if (load_mode != LOAD_VISITOR)
        crawl_state.level_annotation_shown = false;

    if (make_changes)
    {
        // Update PlaceInfo entries
        PlaceInfo& curr_PlaceInfo = you.get_place_info();
        PlaceInfo  delta;

        if (load_mode == LOAD_START_GAME
            || (load_mode == LOAD_ENTER_LEVEL
                && old_level.branch != you.where_are_you
                && !popped))
        {
            delta.num_visits++;
        }

        if (just_created_level)
            delta.levels_seen++;

        you.global_info += delta;
#ifdef DEBUG_LEVEL_LOAD
        mprf(MSGCH_DIAGNOSTICS,
             "global_info:: num_visits: %d, levels_seen: %d",
             you.global_info.num_visits, you.global_info.levels_seen);
#endif
        you.global_info.assert_validity();

        curr_PlaceInfo += delta;
#ifdef DEBUG_LEVEL_LOAD
        mprf(MSGCH_DIAGNOSTICS,
             "curr_PlaceInfo:: num_visits: %d, levels_seen: %d",
             curr_PlaceInfo.num_visits, curr_PlaceInfo.levels_seen);
#endif
#if TAG_MAJOR_VERSION == 34
        // TODO: place_info crashes are extremely brittle and unrecoverable,
        // it might be good to generalize this fixup.
        // TODO: this fixup triggers on &ctrl-r for a depth 1 branch (e.g.
        // the vestibule of hell), would be nice to avoid. (Currently,
        // `wizard_recreate_level` has a workaround.) But this check can't be
        // skipped for that case, because otherwise the ASSERT is tripped.
        //
        // this fixup is for a bug where turns_on_level==0 was used to set
        // just_created_level, and there were some obscure ways to have 0
        // turns on a level that you had entered previously. It only applies
        // to a narrow version range (basically 0.23.0) but there's no way to
        // do a sensible minor version check here and the fixup can't happen
        // on load.
        if (is_connected_branch(curr_PlaceInfo.branch)
            && brdepth[curr_PlaceInfo.branch] > 0
            && static_cast<int>(curr_PlaceInfo.levels_seen)
                                        > brdepth[curr_PlaceInfo.branch])
        {
            if (crawl_state.prev_cmd != CMD_WIZARD)
            {
                mprf(MSGCH_ERROR,
                    "Fixing up corrupted PlaceInfo for %s (levels_seen is %d)",
                    branches[curr_PlaceInfo.branch].shortname,
                    curr_PlaceInfo.levels_seen);
            }
            curr_PlaceInfo.levels_seen = brdepth[curr_PlaceInfo.branch];
        }
#endif
        curr_PlaceInfo.assert_validity();
    }

    if (just_created_level && make_changes)
    {
        you.attribute[ATTR_ABYSS_ENTOURAGE] = 0;
        gozag_detect_level_gold(true);
    }


    if (load_mode != LOAD_VISITOR)
    {
        dungeon_events.fire_event(
                        dgn_event(DET_ENTERED_LEVEL, coord_def(), you.time_taken,
                                  load_mode == LOAD_RESTART_GAME));
    }

    if (load_mode == LOAD_ENTER_LEVEL)
    {
        // 50% chance of repelling the stair you just came through.
        if (you.duration[DUR_REPEL_STAIRS_MOVE]
            || you.duration[DUR_REPEL_STAIRS_CLIMB])
        {
            dungeon_feature_type feat = env.grid(you.pos());
            if (feat != DNGN_ENTER_SHOP
                && feat_stair_direction(feat) != CMD_NO_CMD
                && feat_stair_direction(stair_taken) != CMD_NO_CMD)
            {
                string stair_str = feature_description(feat, NUM_TRAPS, "",
                                                       DESC_THE);
                string verb = stair_climb_verb(feat);

                if (coinflip()
                    && slide_feature_over(you.pos()))
                {
                    mprf("%s slides away from you right after you %s it!",
                         stair_str.c_str(), verb.c_str());
                }

                if (coinflip())
                {
                    // Stairs stop fleeing from you now you actually caught one.
                    mprf("%s settles down.", stair_str.c_str());
                    you.duration[DUR_REPEL_STAIRS_MOVE]  = 0;
                    you.duration[DUR_REPEL_STAIRS_CLIMB] = 0;
                }
            }
        }

        ash_detect_portals(is_map_persistent());

        if (just_created_level)
            xom_new_level_noise_or_stealth();
    }

    if (just_created_level && (load_mode == LOAD_ENTER_LEVEL
                               || load_mode == LOAD_START_GAME))
    {
        decr_zot_clock();
    }

    // Initialize halos, etc.
    invalidate_agrid(true);

    // Maybe make a note if we reached a new level.
    // Don't do so if we are just moving around inside Pan, though.
    if (just_created_level && make_changes
        && stair_taken != DNGN_TRANSIT_PANDEMONIUM)
    {
        take_note(Note(NOTE_DUNGEON_LEVEL_CHANGE));
    }

    // If the player entered the level from a different location than they last
    // exited it, have monsters lose track of where they are
    if (make_changes && you.position != env.old_player_pos)
       shake_off_monsters(you.as_player());

#if TAG_MAJOR_VERSION == 34
    if (make_changes && you.props.exists("zig-fixup")
        && you.where_are_you == BRANCH_TOMB
        && you.depth == brdepth[BRANCH_TOMB])
    {
        if (!just_created_level)
        {
            int obj = items(false, OBJ_MISCELLANY, MISC_ZIGGURAT, 0);
            ASSERT(obj != NON_ITEM);
            bool success = move_item_to_grid(&obj, you.pos(), true);
            ASSERT(success);
        }
        you.props.erase("zig-fixup");
    }
#endif

    return just_created_level;
}

void save_level(const level_id& lid)
{
    if (you.level_visited(lid))
        travel_cache.get_level_info(lid).update();

    // Nail all items to the ground.
    fix_item_coordinates();

    _write_tagged_chunk(lid.describe(), TAG_LEVEL);
}

#if TAG_MAJOR_VERSION == 34
# define CHUNK(short, long) short
#else
# define CHUNK(short, long) long
#endif

#define SAVEFILE(short, long, savefn)           \
    do                                          \
    {                                           \
        writer w(you.save, CHUNK(short, long)); \
        savefn(w);                              \
    } while (false)

// Stack allocated string's go in separate function, so Valgrind doesn't
// complain.
static void _save_game_base()
{
    /* Stashes */
    SAVEFILE("st", "stashes", StashTrack.save);

    /* lua */
    SAVEFILE("lua", "lua", clua.save); // what goes in here?

    /* kills */
    SAVEFILE("kil", "kills", you.kills.save);

    /* travel cache */
    SAVEFILE("tc", "travel_cache", travel_cache.save);

    /* notes */
    SAVEFILE("nts", "notes", save_notes);

    /* tutorial/hints mode */
    if (crawl_state.game_is_hints_tutorial())
        SAVEFILE("tut", "tutorial", save_hints);

    /* messages */
    SAVEFILE("msg", "messages", save_messages);

    /* tile dolls (empty for ASCII)*/
#ifdef USE_TILE
    // Save the current equipment into a file.
    SAVEFILE("tdl", "tiles_doll", save_doll_file);
#endif

    _write_tagged_chunk("you", TAG_YOU);
    _write_tagged_chunk("chr", TAG_CHR);
}

// Stack allocated string's go in separate function, so Valgrind doesn't
// complain.
static void _save_game_exit()
{
    clua.save_persist();

    // Prompt for saving macros.
    if (crawl_state.unsaved_macros)
        macro_save();

    // Must be exiting -- save level & goodbye!
    if (!you.entering_level)
        save_level(level_id::current());

    clrscr();

    save_game_prefs();
    update_whereis("saved");

#ifdef USE_TILE_WEB
    tiles.send_exit_reason("saved");
#endif

    delete you.save;
    you.save = 0;
}

void save_game(bool leave_game, const char *farewellmsg)
{
    unwind_bool saving_game(crawl_state.saving_game, true);
    // Should you.no_save disable more here? Currently it entails an empty
    // package, and persists won't save, but there's a bunch of other stuff
    // that can.
    ASSERT(you.on_current_level || Options.no_save);


    if (leave_game && Options.dump_on_save)
    {
        if (!dump_char(you.your_name, true))
        {
            mpr("Char dump unsuccessful! Sorry about that.");
            if (!crawl_state.seen_hups)
                more();
        }
#ifdef USE_TILE_WEB
        else
            tiles.send_dump_info("save", you.your_name);
#endif
    }

    // Stack allocated string's go in separate function,
    // so Valgrind doesn't complain.
    _save_game_base();

    // If just save, early out.
    if (!leave_game)
    {
        if (!crawl_state.disables[DIS_SAVE_CHECKPOINTS])
        {
            you.save->commit();
            save_game_prefs();
        }
        return;
    }

    // Stack allocated string's go in separate function,
    // so Valgrind doesn't complain.
    _save_game_exit();

    game_ended(game_exit::save, farewellmsg ? farewellmsg
                                : "See you soon, " + you.your_name + "!");
}

// Saves the game without exiting.
void save_game_state()
{
    save_game(false);
    if (crawl_state.seen_hups)
        save_game(true);
}

static bool _bones_save_individual_levels(bool store)
{
    // Only use level-numbered bones files for places where players die a lot.
    // For the permastore, go even coarser (just D and Lair use level numbers).
    // n.b. some branches here may not currently generate ghosts.
    // TODO: further adjustments? Make Zot coarser?
    return store ? player_in_branch(BRANCH_DUNGEON) ||
                   player_in_branch(BRANCH_LAIR)
                 : !(player_in_branch(BRANCH_ZIGGURAT) ||
                     player_in_branch(BRANCH_CRYPT) ||
                     player_in_branch(BRANCH_TOMB) ||
                     player_in_branch(BRANCH_ABYSS) ||
                     player_in_branch(BRANCH_SLIME));
}

static string _make_ghost_filename(bool store=false)
{
    const bool with_number = _bones_save_individual_levels(store);
    // Players die so rarely in hell in practice that it doesn't even make
    // sense to have per-hell bones. (Maybe vestibule should be separate?)
    const string level_desc = player_in_hell(true) ? "Hells" :
        replace_all(level_id::current().describe(false, with_number), ":", "-");
    return string("bones.") + (store ? "store." : "") + level_desc;
}

static string _bones_permastore_file()
{
    string filename = _make_ghost_filename(true);
    string full_path = _get_bonefile_directory() + filename;
    if (file_exists(full_path))
        return full_path;

    string dist_full_path = datafile_path(
            string("dist_bones") + FILE_SEPARATOR + filename, false, false);
    if (dist_full_path.empty())
        return dist_full_path;

    // no matching permastore is in the player's bones file, but one exists in
    // the crawl distribution. Install it.

    FILE *src = fopen(dist_full_path.c_str(), "rb");
    if (!src)
    {
        mprf(MSGCH_ERROR, "Bones file exists but can't be opened: %s",
            dist_full_path.c_str());
        return "";
    }
    FILE *target = lk_open("wb", full_path);
    if (!target)
    {
        mprf(MSGCH_ERROR, "Unable to open bones file %s for writing",
            full_path.c_str());
        fclose(src);
        return "";
    }

    _ghost_dprf("Copying %s to %s", dist_full_path.c_str(), full_path.c_str());

    char buf[BUFSIZ];

    size_t size;
    while ((size = fread(buf, sizeof(char), BUFSIZ, src)) > 0)
        fwrite(buf, sizeof(char), size, target);

    lk_close(target);

    if (!feof(src))
    {
        mprf(MSGCH_ERROR, "Error installing bones file to %s",
                                                    full_path.c_str());
        if (unlink(full_path.c_str()) != 0)
        {
            mprf(MSGCH_ERROR,
                "Failed to unlink probably corrupt bones file: %s",
                full_path.c_str());
        }
        fclose(src);
        return "";
    }
    fclose(src);
    return full_path;
}

// Bones files
//
// There are two kinds of bones files: temporary bones files and the
// permastore. Temporary bones files are ephemeral: ghosts will be reused only
// if they are on the floor where the player dies. The permastore is a more
// permanent stock of ghosts (per level) to use as a backup in case the
// temporary bones files are depleted.

/**
 * Lists all bonefiles for the current level.
 *
 * @return A vector containing absolute paths to 0+ bonefiles.
 */
static vector<string> _list_bones()
{
    string bonefile_dir = _get_bonefile_directory();
    string base_filename = _make_ghost_filename();
    string underscored_filename = base_filename + "_";

    vector<string> filenames = get_dir_files_sorted(bonefile_dir);
    vector<string> bonefiles;
    for (const auto &filename : filenames)
        if (starts_with(filename, underscored_filename)
                                            && !ends_with(filename, ".backup"))
        {
            bonefiles.push_back(bonefile_dir + filename);
            _ghost_dprf("bonesfile %s", (bonefile_dir + filename).c_str());
        }

    string old_bonefile = _get_old_bonefile_directory() + base_filename;
    if (access(old_bonefile.c_str(), F_OK) == 0)
    {
        _ghost_dprf("Found old bonefile %s", old_bonefile.c_str());
        bonefiles.push_back(old_bonefile);
    }

    return bonefiles;
}

/**
 * Attempts to find a file containing ghost(s) appropriate for the player.
 *
 * @return The filename of an appropriate bones file; may be "".
 */
static string _find_ghost_file()
{
    vector<string> bonefiles = _list_bones();
    if (bonefiles.empty())
        return "";
    return bonefiles[random2(bonefiles.size())];
}

static string _old_bones_filename(string ghost_filename, const save_version &v)
{
    // TODO: a way of looking for any backup with a version earlier than v
    if (ends_with(ghost_filename, ".backup"))
        return ghost_filename; // already an old bones file

    string new_filename = make_stringf("%s-v%d.%d.backup", ghost_filename.c_str(),
                                        v.major, v.minor);
    return new_filename;
}

static bool _backup_bones_for_upgrade(string ghost_filename, save_version &v)
{
    // Copy the bones file to a versioned name, so that non-upgraded saves can
    // load it. Copying would be cleaner with c++ ios stuff, but we need to
    // interact with the lock system.

    if (ghost_filename.empty())
        return false;
    if (ends_with(ghost_filename, ".backup"))
        return false; // already an old bones file

    string upgrade_filename = _old_bones_filename(ghost_filename, v);
    if (file_exists(upgrade_filename))
        return false;
    _ghost_dprf("Backing up bones file %s to %s before upgrade to %d.%d",
                            ghost_filename.c_str(), upgrade_filename.c_str(),
                            save_version::current_bones().major,
                            save_version::current_bones().minor);

    FILE *backup_src = lk_open("rb", ghost_filename);
    if (!backup_src)
    {
        mprf(MSGCH_ERROR, "Bones file to back up doesn't exist: %s",
            ghost_filename.c_str());
        return false;
    }
    FILE *backup_target = lk_open("wb", upgrade_filename);
    if (!backup_target)
    {
        mprf(MSGCH_ERROR, "Unable to open bones backup file %s for writing",
            upgrade_filename.c_str());
        lk_close(backup_src);
        return false;
    }

    char buf[BUFSIZ];

    size_t size;
    while ((size = fread(buf, sizeof(char), BUFSIZ, backup_src)) > 0)
        fwrite(buf, sizeof(char), size, backup_target);

    lk_close(backup_target);

    if (!feof(backup_src))
    {
        mprf(MSGCH_ERROR, "Error backing up bones file to %s",
                                                    upgrade_filename.c_str());
        if (unlink(upgrade_filename.c_str()) != 0)
        {
            mprf(MSGCH_ERROR,
                "Failed to unlink probably corrupt bones file: %s",
                upgrade_filename.c_str());
        }
        lk_close(backup_src);
        return false;
    }
    lk_close(backup_src);
    return true;
}

save_version read_ghost_header(reader &inf)
{
    auto version = get_save_version(inf);
    if (!version.valid())
        return version;

#if TAG_MAJOR_VERSION == 34
    // downgrade bones files saved before the bones sub-versioning system
    if (version > save_version::current_bones() && version.is_compatible())
    {
        _ghost_dprf("Setting bones file version from %d.%d to %d.%d on load",
            version.major, version.minor,
            save_version::current_bones().major,
            save_version::current_bones().minor);
        version = save_version::current_bones();
    }
#endif

    try
    {
        // Check for the DCSS ghost signature.
        if (unmarshallShort(inf) != GHOST_SIGNATURE)
            return save_version(); // version was valid, but this isn't a bones file

        // Discard three more 32-bit words of padding.
        inf.read(nullptr, 3*4);
    }
    catch (short_read_exception &E)
    {
        mprf(MSGCH_ERROR,
             "Ghost file \"%s\" seems to be invalid (short read); deleting it.",
             inf.filename().c_str());
        return save_version();
    }

    return version;
}

vector<ghost_demon> load_bones_file(string ghost_filename, bool backup)
{
    vector<ghost_demon> result;

    if (ghost_filename.empty())
        return result; // no such ghost.

    reader inf(ghost_filename);
    if (!inf.valid())
    {
        // file doesn't exist
        _ghost_dprf("Ghost file '%s' invalid before read.", ghost_filename.c_str());
        return result;
    }

    inf.set_safe_read(true); // don't die on 0-byte bones
    save_version version = read_ghost_header(inf);
    if (!_ghost_version_compatible(version))
    {
        string error = "Incompatible bones file: " + ghost_filename;
        throw corrupted_save(error, version);
    }
    inf.setMinorVersion(version.minor);
    if (backup && version < save_version::current_bones())
        _backup_bones_for_upgrade(ghost_filename, version);

    try
    {
        result = tag_read_ghosts(inf);
        inf.fail_if_not_eof(ghost_filename);
    }
    catch (short_read_exception &short_read)
    {
        string error = "Broken bones file: " + ghost_filename;
        throw corrupted_save(error, version);
    }
    inf.close();

    if (!debug_check_ghosts(result))
    {
        string error = "Bones file is buggy: " + ghost_filename;
        throw corrupted_save(error, version);
    }

    return result;
}


static vector<ghost_demon> _load_ghosts_core(string filename,
                                                        bool backup_on_upgrade)
{
    vector<ghost_demon> results;
    try
    {
        results = load_bones_file(filename, backup_on_upgrade);
    }
    catch (corrupted_save &err)
    {
        // not a corrupted save per se, just from the future. Try to load the
        // versioned bones file if it exists.
        if (err.version.valid() && err.version.is_future())
        {
            string old_bones =
                        _old_bones_filename(filename, save_version::current());
            if (old_bones != filename)
            {
                _ghost_dprf("Loading ghost from backup bones file %s",
                                                        old_bones.c_str());
                return load_bones_file(old_bones, false);
            }
            else
                mprf(MSGCH_ERROR, "Mismatch between bones backup "
                    "filename '%s' and version %d.%d!", filename.c_str(),
                    err.version.major, err.version.minor);
            // intentional fallthrough -- unlink the misnamed file
        }
        else
            mprf(MSGCH_ERROR, "%s", err.what());
        string report;
        // if we get to this point the bones file is unreadable and needs to
        // be scrapped
        if (unlink(filename.c_str()) != 0)
            report = "Failed to unlink bad bones file";
        else
            report = "Clearing bad bones file";
        mprf(MSGCH_ERROR, "%s: %s", report.c_str(), filename.c_str());
    }
    return results;

}

static vector<ghost_demon> _load_ephemeral_ghosts()
{
    vector<ghost_demon> results;

    string ghost_filename = _find_ghost_file();
    if (ghost_filename.empty())
    {
        _ghost_dprf("%s", "No ephemeral ghost files for this level.");
        return results; // no such ghost.
    }

    results = _load_ghosts_core(ghost_filename, true);

    if (unlink(ghost_filename.c_str()) != 0)
    {
        mprf(MSGCH_ERROR, "Failed to unlink bones file: %s",
                ghost_filename.c_str());
    }
    return results;
}

static vector<ghost_demon> _load_permastore_ghosts(bool backup_on_upgrade=false)
{
    return _load_ghosts_core(_bones_permastore_file(), backup_on_upgrade);
}

/**
 * Attempt to fill in a monster based on bones files.
 *
 * @param mons the monster to fill in
 *
 * @return whether there was a saved ghost that could be used.
 */
bool define_ghost_from_bones(monster& mons)
{
    rng::generator rng(rng::SYSTEM_SPECIFIC);

    bool used_permastore = false;

    vector<ghost_demon> loaded_ghosts = _load_ephemeral_ghosts();
    if (loaded_ghosts.empty())
    {
        loaded_ghosts = _load_permastore_ghosts();
        if (loaded_ghosts.empty())
            return false;
        used_permastore = true;
    }

    int place_i = random2(loaded_ghosts.size());
    _ghost_dprf("Loaded ghost file with %u ghost(s), placing %s",
         (unsigned int)loaded_ghosts.size(), loaded_ghosts[place_i].name.c_str());

    mons.set_ghost(loaded_ghosts[place_i]);
    mons.type = MONS_PLAYER_GHOST;
    mons.ghost_init(false);

    if (!mons.alive())
        mprf(MSGCH_ERROR, "Placed ghost is not alive.");
    else if (mons.type != MONS_PLAYER_GHOST)
    {
        mprf(MSGCH_ERROR, "Placed ghost is not MONS_PLAYER_GHOST, but %s",
             mons.name(DESC_PLAIN, true).c_str());
    }

    if (!used_permastore)
    {
        loaded_ghosts.erase(loaded_ghosts.begin() + place_i);

        if (!loaded_ghosts.empty())
            save_ghosts(loaded_ghosts);
    }
    return true;
}

/**
 * Attempt to load one or more ghosts into the level.
 *
 * @param max_ghosts        A maximum number of ghosts to creat.
 *                          Set to <= 0 to load as many as possible.
 * @param creating_level    Whether a level is currently being generated.
 * @return                  Whether ghosts were actually generated.
 */
bool load_ghosts(int max_ghosts, bool creating_level)
{
    ASSERT(you.transit_stair == DNGN_UNSEEN || creating_level);
    ASSERT(!you.entering_level || creating_level);
    ASSERT(!creating_level
           || (you.entering_level && you.transit_stair != DNGN_UNSEEN));
    // Only way to load a ghost without creating a level is via a wizard
    // command.
    ASSERT(creating_level || (crawl_state.prev_cmd == CMD_WIZARD));

#ifdef BONES_DIAGNOSTICS
    // this is pretty hacky, but arguably cleaner than what it is replacing.
    // The effect is to show bones diagnostic messages on wizmode builds during
    // level building
    unwind_var<command_type> last_cmd(crawl_state.prev_cmd, creating_level ?
        CMD_WIZARD : crawl_state.prev_cmd);
#endif

    vector<ghost_demon> loaded_ghosts = _load_ephemeral_ghosts();

    _ghost_dprf("Loaded ghost file with %u ghost(s), will attempt to place %d of them",
             (unsigned int)loaded_ghosts.size(), max_ghosts);

    bool ghost_errors = false;

    max_ghosts = max_ghosts <= 0 ? loaded_ghosts.size()
                                 : min(max_ghosts, (int) loaded_ghosts.size());
    int placed_ghosts = 0;

    // Translate ghost to monster and place.
    while (!loaded_ghosts.empty() && placed_ghosts < max_ghosts)
    {
        monster * const mons = get_free_monster();
        if (!mons)
            break;

        mons->set_new_monster_id();
        mons->set_ghost(loaded_ghosts[0]);
        mons->type = MONS_PLAYER_GHOST;
        mons->ghost_init();

        loaded_ghosts.erase(loaded_ghosts.begin());
        placed_ghosts++;

        if (!mons->alive())
        {
            _ghost_dprf("Placed ghost is not alive.");
            ghost_errors = true;
        }
        else if (mons->type != MONS_PLAYER_GHOST)
        {
            _ghost_dprf("Placed ghost is not MONS_PLAYER_GHOST, but %s",
                 mons->name(DESC_PLAIN, true).c_str());
            ghost_errors = true;
        }
    }

    if (placed_ghosts < max_ghosts)
    {
        _ghost_dprf("Unable to place %u ghost(s)", max_ghosts - placed_ghosts);
        ghost_errors = true;
    }
#ifdef BONES_DIAGNOSTICS
    if (ghost_errors)
        more();
#endif

    // resave any unused ghosts
    if (!loaded_ghosts.empty())
        save_ghosts(loaded_ghosts);

    return true;
}

static string _type_name_processed(game_type t)
{
    string name = game_state::game_type_name_for(t);
    return name.size() ? name : "regular";
}

// returns false if a new game should start instead
static bool _restore_game(const string& filename)
{
    if (Options.no_save)
        return false;

    // In webtiles, a more before the player is loaded will crash when it tries
    // to send enough information to the webtiles client to render the display.
    // This is just cosmetic for other build targets.
    unwind_bool save_more(crawl_state.show_more_prompt, false);
    game_type menu_game_type = crawl_state.type;

    clear_message_store();

    you.save = new package((_get_savefile_directory() + filename).c_str(), true);

    player_save_info save_info = _read_character_info(you.save);
    if (!save_info.save_loadable)
    {
        // Note: if we are here, the save info was properly read, it would
        // raise an exception otherwise.
        if (yesno(("There is an existing game for name '" + save_info.name +
                   "' from an incompatible version of Crawl ("
                   + save_info.prev_save_version + ").\n"
                   "Unless you reinstall that version, you can't load it.\n"
                   "Do you want to DELETE that game and start a new one?"
                  ).c_str(),
                  true, 'n'))
        {
            you.save->unlink();
            you.save = 0;
            return false;
        }
        if (Options.remember_name)
            crawl_state.default_startup_name = save_info.name; // for main menu
        you.save->abort();
        delete you.save;
        you.save = 0;
        game_ended(game_exit::abort,
                save_info.name
                + " is from an incompatible version and can't be loaded.");
    }

    if (!crawl_state.bypassed_startup_menu
        && menu_game_type != crawl_state.type)
    {
        if (!yesno(("You already have a "
                        + _type_name_processed(save_info.saved_game_type) +
                    " game saved under the name '" + save_info.name + "';\n"
                    "do you want to load that instead?").c_str(),
                   true, 'n'))
        {
            you.save->abort(); // don't even rewrite the header
            delete you.save;
            you.save = 0;
            // explicitly don't set default startup name here
            game_ended(game_exit::abort,
                "Please use a different name to start a new " +
                _type_name_processed(menu_game_type) + " game, then.");
        }
    }
    if (Options.remember_name)
        crawl_state.default_startup_name = save_info.name; // for main menu

    if (numcmp(save_info.prev_save_version.c_str(), Version::Long, 2) == -1
        && version_is_stable(save_info.prev_save_version.c_str()))
    {
        if (!yesno(("This game comes from a previous release of Crawl (" +
                    save_info.prev_save_version + ").\n\nIf you load it now,"
                    " you won't be able to go back. Continue?").c_str(),
                    true, 'n'))
        {
            you.save->abort(); // don't even rewrite the header
            delete you.save;
            you.save = 0;
            game_ended(game_exit::abort, "Please use version " +
                save_info.prev_save_version
                + " to load " + save_info.name + " then.");
        }
    }
    you.init_from_save_info(save_info);

    you.on_current_level = false; // we aren't on the current level until
                                  // everything is fully loaded
    _restore_tagged_chunk(you.save, "you", TAG_YOU, "Save data is invalid.");

    _convert_obsolete_species();

    const int minorVersion = crawl_state.minor_version;

    if (you.save->has_chunk(CHUNK("st", "stashes")))
    {
        reader inf(you.save, CHUNK("st", "stashes"), minorVersion);
        StashTrack.load(inf);
    }

#ifdef CLUA_BINDINGS
    if (you.save->has_chunk("lua"))
    {
        vector<char> buf;
        chunk_reader inf(you.save, "lua");
        inf.read_all(buf);
        buf.push_back(0);
        clua.execstring(&buf[0]);
    }
#endif

    if (you.save->has_chunk(CHUNK("kil", "kills")))
    {
        reader inf(you.save, CHUNK("kil", "kills"),minorVersion);
        you.kills.load(inf);
    }

    if (you.save->has_chunk(CHUNK("tc", "travel_cache")))
    {
        reader inf(you.save, CHUNK("tc", "travel_cache"), minorVersion);
        travel_cache.load(inf, minorVersion);
    }

    if (you.save->has_chunk(CHUNK("nts", "notes")))
    {
        reader inf(you.save, CHUNK("nts", "notes"), minorVersion);
        load_notes(inf);
    }

    /* hints mode */
    if (you.save->has_chunk(CHUNK("tut", "tutorial")))
    {
        reader inf(you.save, CHUNK("tut", "tutorial"), minorVersion);
        load_hints(inf);
    }

    /* messages */
    if (you.save->has_chunk(CHUNK("msg", "messages")))
    {
        reader inf(you.save, CHUNK("msg", "messages"), minorVersion);
        load_messages(inf);
    }

    // Handle somebody SIGHUP'ing out of the skill menu with every skill
    // disabled. Doing this here rather in tags code because it can trigger
    // UI, which may not be safe if everything isn't fully loaded.
    check_selected_skills();

    return true;
}

// returns false if a new game should start instead
bool restore_game(const string& filename)
{
    try
    {
        return _restore_game(filename);
    }
    catch (corrupted_save &err)
    {
        if (yesno(make_stringf(
                   "There exists a save by that name but it appears to be invalid.\n"
                   "Do you want to delete it?\n"
                   "Error: %s", err.what()).c_str(), // TODO linebreak error
                  true, 'n'))
        {
            if (you.save)
                you.save->unlink();
            you.save = 0;
            return false;
        }
        // Shouldn't crash probably...
        fail("Aborting; you may try to recover it somehow.");
    }
}

static void _load_level(const level_id &level)
{
    // Load the given level.
    you.where_are_you = level.branch;
    you.depth =         level.depth;

    load_level(DNGN_STONE_STAIRS_DOWN_I, LOAD_VISITOR, level_id());
}

// Given a level returns true if the level has been created already
// in this game. Warning: after a game has ended, there is a phase where the
// save has been deleted and this check isn't usable, and this is when a moruge
// is generated.
bool is_existing_level(const level_id &level)
{
    return you.save && you.save->has_chunk(level.describe());
}

void delete_level(const level_id &level)
{
    travel_cache.erase_level_info(level);
    StashTrack.remove_level(level);
    shopping_list.del_things_from(level);

    clear_level_exclusion_annotation(level);
    clear_level_annotations(level);

    if (you.save)
        you.save->delete_chunk(level.describe());

    auto &visited = you.props[VISITED_LEVELS_KEY].get_table();
    visited.erase(level.describe());

    if (level.branch == BRANCH_ABYSS)
    {
        save_abyss_uniques();
        destroy_abyss();
    }
    _do_lost_monsters();
    _do_lost_items();
}


static bool &_get_excursions_allowed()
{
    static bool _allowed = true;
    return _allowed;
}

bool level_excursions_allowed()
{
    return _get_excursions_allowed();
}

no_excursions::no_excursions()
    : prev(level_excursions_allowed())
{
    _get_excursions_allowed() = false;
}

no_excursions::~no_excursions()
{
    _get_excursions_allowed() = prev;
}

// This class provides a way to walk the dungeon with a bit more flexibility
// than you used to get with apply_to_all_dungeons.
level_excursion::level_excursion()
    : original(level_id::current()), ever_changed_levels(false)
{
    // could put an excursions allowed check here?
}

void level_excursion::go_to(const level_id& next)
{
    // This ASSERT is here because level excursions are often triggered as
    // side effects, e.g. in shopping list code, and we really don't want this
    // happening during normal levelgen (weird interactions with seeding,
    // potential crashes if items or monsters are incomplete, etc). However,
    // the abyss purposefully does level excursions in order to pick up
    // features from other levels and place them in the abyss: this is
    // basically safe to do, and seeding isn't a concern.
    // TODO: reimplement with no_excursions?
    ASSERT(!crawl_state.generating_level || original.branch == BRANCH_ABYSS);

    if (level_id::current() != next)
    {
        ASSERT(level_excursions_allowed());

        if (!you.level_visited(level_id::current()))
            travel_cache.erase_level_info(level_id::current());

        ever_changed_levels = true;

        save_level(level_id::current());
        _load_level(next);

        if (you.level_visited(next))
        {
            LevelInfo &li = travel_cache.get_level_info(next);
            li.set_level_excludes();
        }
        // TODO: this won't clear excludes on an excursion to an unvisited
        // level. Does this matter? Not right now, this case is only used for
        // abyss procgen.
    }

    you.on_current_level = (level_id::current() == original);
}

level_excursion::~level_excursion()
{
    // Go back to original level and reactivate markers if we ever
    // left the level.
    if (ever_changed_levels)
    {
        // This may be a no-op if the level-excursion subsequently
        // returned to the original level. However, at this point
        // markers will still not be activated.
        go_to(original);

        // Quietly reactivate markers.
        env.markers.activate_all(false);
    }
}

save_version get_save_version(reader &file)
{
    int major, minor;
    try
    {
        major = unmarshallUByte(file);
        minor = unmarshallUByte(file);
        if (minor == UINT8_MAX)
            minor = unmarshallInt(file);
    }
    catch (short_read_exception& E)
    {
        // Empty file?
        return save_version(-1, -1);
    }
    return save_version(major, minor);
}

void write_save_version(writer &outf, save_version version)
{
    marshallUByte(outf, version.major);
    if (version.minor < UINT8_MAX)
        marshallUByte(outf, version.minor);
    else
    {
        marshallUByte(outf, UINT8_MAX);
        marshallInt(outf, version.minor);
    }
}

static bool _convert_obsolete_species()
{
    // At this point the character has been loaded but not resaved, but the grid, lua, stashes, etc have not been.
#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_LAVA_ORC)
    {
        if (!yesno(
            "This Lava Orc save game cannot be loaded as-is. If you load it now,\n"
            "your character will be converted to a Hill Orc. Continue?",
                       false, 'N'))
        {
            you.save->abort(); // don't even rewrite the header
            delete you.save;
            you.save = 0;
            game_ended(game_exit::abort,
                "Please load the save in an earlier version "
                "if you want to keep it as a Lava Orc.");
        }
        change_species_to(SP_HILL_ORC);
        // No need for conservation
        you.innate_mutation[MUT_CONSERVE_SCROLLS]
                                = you.mutation[MUT_CONSERVE_SCROLLS] = 0;
        // This is not an elegant way to deal with lava, but at this point the
        // level isn't loaded so we can't check the grid features. In
        // addition, even if the player isn't over lava, they might still get
        // trapped.
        fly_player(100);
        return true;
    }
#endif
    return false;
}

static bool _loadable_save_ver(int major, int minor)
{
#if TAG_MAJOR_VERSION == 34
    if (major == 33 && minor == TAG_MINOR_0_11)
        return true;
#endif
    return major == TAG_MAJOR_VERSION && minor <= TAG_MINOR_VERSION;
}

static player_save_info _read_character_info(package *save)
{
    reader inf(save, "chr");

    try
    {
        player_save_info result;
        const auto version = get_save_version(inf);
        const auto major = version.major, minor = version.minor;
        uint8_t format;

        unsigned int len = unmarshallInt(inf);
        if (len > 1024) // something is fishy
            fail("Save file `%s` corrupted (info > 1KB)", save->get_filename().c_str());
        vector<unsigned char> buf;
        buf.resize(len);
        inf.read(&buf[0], len);
        inf.fail_if_not_eof("chr");
        reader th(buf);

        // 0.8 trunks (30.0 .. 32.12) were format 0 but without the marker.
        if (major > 32 || major == 32 && minor >= 13)
            th.read(&format, 1);
        else
            format = 0;

        if (format > TAG_CHR_FORMAT)
        {
            fail("Incompatible character data from the future in `%s`",
                                        save->get_filename().c_str());
        }

        result = tag_read_char_info(th, format, major, minor);

        // Check if we read everything only on the exact same version,
        // but that's the common case.
        if (major == TAG_MAJOR_VERSION && minor == TAG_MINOR_VERSION)
            inf.fail_if_not_eof("chr");

        result.save_loadable = _loadable_save_ver(major, minor);
        return result;
    }
    catch (short_read_exception &E)
    {
        fail("Save file `%s` corrupted (short read)", save->get_filename().c_str());
    };
}

static bool _tagged_chunk_version_compatible(reader &inf, string* reason)
{
    ASSERT(reason);

    const save_version version = get_save_version(inf);

    if (!version.valid())
    {
        *reason = make_stringf("File is corrupt (found version %d,%d).",
                                version.major, version.minor);
        return false;
    }

    if (!version.is_compatible())
    {
        if (version.is_ancient())
        {
            const auto min_supported = save_version::minimum_supported();
            *reason = make_stringf("This save is from an older version.\n"
                    "\n"
                    CRAWL " %s is not compatible with save files this old. You can:\n"
                    " • continue your game with an older version of " CRAWL "\n"
                    " • delete it and start a new game\n"
                    "\n"
                    "This save's version: (%d.%d) (must be >= %d.%d)",
                    Version::Short,
                    version.major, version.minor,
                    min_supported.major, min_supported.minor);
        }
        else if (version.is_future())
        {
            const auto current = save_version::current();
            *reason = make_stringf("This save is from a newer version.\n"
                    "\n"
                    CRAWL " cannot load saves from newer versions. You can:\n"
                    " • continue your game with a newer version of " CRAWL "\n"
                    " • delete it and start a new game\n"
                    "\n"
                    "This save's version: (%d.%d) (must be <= %d.%d)",
                    version.major, version.minor,
                    current.major, current.minor);
        }
        return false;
    }

    inf.setMinorVersion(version.minor);
    return true;
}

static bool _restore_tagged_chunk(package *save, const string &name,
                                  tag_type tag, const char* complaint)
{
    reader inf(save, name);
    string reason;
    if (!_tagged_chunk_version_compatible(inf, &reason))
    {
        if (!complaint)
        {
            dprf("chunk %s: %s", name.c_str(), reason.c_str());
            return false;
        }
        else
            end(-1, false, "\n%s %s\n", complaint, reason.c_str());
    }

    crawl_state.minor_version = inf.getMinorVersion();
    try
    {
        tag_read(inf, tag);
    }
    catch (short_read_exception &E)
    {
        fail("truncated save chunk (%s)", name.c_str());
    };

    inf.fail_if_not_eof(name);
    return true;
}

static bool _ghost_version_compatible(const save_version &version)
{
    if (!version.valid())
        return false;
    if (!version.is_compatible())
    {
        _ghost_dprf("Ghost version mismatch: ghost was %d.%d; current is %d.%d",
             version.major, version.minor,
             save_version::current().major, save_version::current().minor);
        return false;
    }
    return true;
}

/**
 * Attempt to open a new bones file for saving ghosts.
 *
 * @param[out] return_gfilename     The name of the file created, if any.
 * @return                          A FILE object, or nullptr.
 **/
static FILE* _make_bones_file(string * return_gfilename)
{
    const string bone_dir = _get_bonefile_directory();
    const string base_filename = _make_ghost_filename(false);

    for (int i = 0; i < GHOST_LIMIT; i++)
    {
        const string g_file_name = make_stringf("%s%s_%d", bone_dir.c_str(),
                                                base_filename.c_str(), i);
        FILE *ghost_file = lk_open_exclusive(g_file_name);
        // need to check file size, so can't open 'wb' - would truncate!

        if (!ghost_file)
        {
            dprf("Could not open %s", g_file_name.c_str());
            continue;
        }

        dprf("found %s", g_file_name.c_str());

        *return_gfilename = g_file_name;
        return ghost_file;
    }

    return nullptr;
}

#define GHOST_PERMASTORE_SIZE 10
#define GHOST_PERMASTORE_REPLACE_CHANCE 5

static size_t _ghost_permastore_size()
{
    if (_bones_save_individual_levels(true))
        return GHOST_PERMASTORE_SIZE;
    else
        return GHOST_PERMASTORE_SIZE * 2;
}

static vector<ghost_demon> _update_permastore(const vector<ghost_demon> &ghosts)
{
    rng::generator rng(rng::SYSTEM_SPECIFIC);
    if (ghosts.empty())
        return ghosts;

    // this read is not locked...
    vector<ghost_demon> permastore = _load_permastore_ghosts();
    vector<ghost_demon> leftovers;

    bool rewrite = false;
    unsigned int i = 0;
    const size_t max_ghosts = _ghost_permastore_size();
    while (permastore.size() < max_ghosts && i < ghosts.size())
    {
        // TODO: heuristics to make this as distinct as possible; maybe
        // create a new name?
        permastore.push_back(ghosts[i]);
#ifdef DGAMELAUNCH
        // randomize name for online play
        permastore.back().name = make_name();
#endif
        i++;
        rewrite = true;
    }
    if (i > 0)
        _ghost_dprf("Permastoring %d ghosts", i);
    if (!rewrite && x_chance_in_y(GHOST_PERMASTORE_REPLACE_CHANCE, 100)
                                                        && i < ghosts.size())
    {
        int rewrite_i = random2(permastore.size());
        permastore[rewrite_i] = ghosts[i];
#ifdef DGAMELAUNCH
        permastore[rewrite_i].name = make_name();
#endif
        rewrite = true;
    }
    while (i < ghosts.size())
    {
        leftovers.push_back(ghosts[i]);
        i++;
    }

    if (rewrite)
    {
        string permastore_file = _bones_permastore_file();

        // the following is to ensure that an old game doesn't overwrite a
        // permastore that has a version in the future relative to that game.
        {
            reader inf(permastore_file);
            if (inf.valid())
            {
                inf.set_safe_read(true); // don't die on 0-byte bones
                save_version version = read_ghost_header(inf);
                if (version.valid() && version.is_future())
                {
                    permastore_file = _old_bones_filename(permastore_file,
                                                    save_version::current());
                }
                inf.close();
            }
        }

        FILE *ghost_file = lk_open("wb", permastore_file);

        if (!ghost_file)
        {
            // this will fail silently if the lock fails, seems safest
            // TODO: better lock system for servers?
            _ghost_dprf("Could not open ghost permastore: %s",
                                                    permastore_file.c_str());
            return ghosts;
        }

        _ghost_dprf("Rewriting ghost permastore %s with %u ghosts",
                    permastore_file.c_str(), (unsigned int) permastore.size());
        writer outw(permastore_file, ghost_file);
        write_ghost_version(outw);
        tag_write_ghosts(outw, permastore);

        lk_close(ghost_file);
    }
    return leftovers;
}

/**
 * Attempt to save all ghosts from the current level.
 *
 * Including the player, if they're not undead. Doesn't save ghosts from D:1-2
 * or Temple.
 *
 * @param force   Forces ghost generation even in otherwise-disallowed levels.
 **/
void save_ghosts(const vector<ghost_demon> &ghosts, bool force, bool use_store)
{
    // n.b. this is not called in the normal course of events for wizmode
    // chars, so for debugging anything to do with deaths in wizmode, you will
    // need to edit a conditional at the end of ouch.cc:ouch.
    _ghost_dprf("Trying to save ghosts.");
    if (ghosts.empty())
    {
        _ghost_dprf("Could not find any ghosts for this level to save.");
        return;
    }

    if (!force && !ghost_demon::ghost_eligible())
    {
        _ghost_dprf("No eligible ghosts.");
        return;
    }

    vector<ghost_demon> leftovers;
    if (use_store)
        leftovers = _update_permastore(ghosts);
    else
        leftovers = ghosts;
    if (leftovers.size() == 0)
        return;

    if (_list_bones().size() >= static_cast<size_t>(GHOST_LIMIT))
    {
        _ghost_dprf("Too many ghosts for this level already!");
        return;
    }

    string g_file_name = "";
    FILE* ghost_file = _make_bones_file(&g_file_name);

    if (!ghost_file)
    {
        _ghost_dprf("Could not open file to save ghosts.");
        return;
    }

    writer outw(g_file_name, ghost_file);

    write_ghost_version(outw);
    tag_write_ghosts(outw, leftovers);

    lk_close(ghost_file);

    _ghost_dprf("Saved ghosts (%s).", g_file_name.c_str());
}

////////////////////////////////////////////////////////////////////////////
// Locking

bool lock_file_handle(FILE *handle, bool write)
{
    return lock_file(fileno(handle), write, true);
}

bool unlock_file_handle(FILE *handle)
{
    return unlock_file(fileno(handle));
}

/**
 * Attempts to open & lock a file.
 *
 * @param mode      The file access mode. ('r', 'ab+', etc)
 * @param file      The path to the file to be opened.
 * @return          A handle for the specified file, if successful; else nullptr.
 */
FILE *lk_open(const char *mode, const string &file)
{
    ASSERT(mode);

    FILE *handle = fopen_u(file.c_str(), mode);
    if (!handle)
        return nullptr;

    const bool write_lock = mode[0] != 'r' || strchr(mode, '+');
    if (!lock_file_handle(handle, write_lock))
    {
        mprf(MSGCH_ERROR, "ERROR: Could not lock file %s", file.c_str());
        fclose(handle);
        handle = nullptr;
    }

    return handle;
}

/**
 * Attempts to open and lock a file for exclusive write access; fails if
 * the file already exists.
 *
 * @param file The path to the file to be opened.
 * @return     A locked file handle for the specified file, if
 *             successful; else nullptr.
 */
FILE *lk_open_exclusive(const string &file)
{
    int fd = open_u(file.c_str(), O_WRONLY|O_BINARY|O_EXCL|O_CREAT, 0666);
    if (fd < 0)
        return nullptr;

    if (!lock_file(fd, true))
    {
        mprf(MSGCH_ERROR, "ERROR: Could not lock file %s", file.c_str());
        close(fd);
        return nullptr;
    }

    return fdopen(fd, "wb");
}

void lk_close(FILE *handle)
{
    if (handle == nullptr || handle == stdin)
        return;

    unlock_file_handle(handle);

    // actually close
    fclose(handle);
}

/////////////////////////////////////////////////////////////////////////////
// file_lock
//
// Locks a named file (usually an empty lock file), creating it if necessary.

file_lock::file_lock(const string &s, const char *_mode, bool die_on_fail)
    : handle(nullptr), mode(_mode), filename(s)
{
    if (!(handle = lk_open(mode, filename)) && die_on_fail)
        end(1, true, "Unable to open lock file \"%s\"", filename.c_str());
}

file_lock::~file_lock()
{
    if (handle)
        lk_close(handle);
}

/////////////////////////////////////////////////////////////////////////////

FILE *fopen_replace(const char *name)
{
    int fd;

    // Stave off symlink attacks. Races will be handled with O_EXCL.
    unlink_u(name);
    fd = open_u(name, O_CREAT|O_EXCL|O_WRONLY, 0666);
    if (fd == -1)
        return 0;
    return fdopen(fd, "w");
}

// Returns the size of the opened file with the give FILE* handle.
off_t file_size(FILE *handle)
{
#ifdef __ANDROID__
    off_t pos = ftello(handle);
    if (fseeko(handle, 0, SEEK_END) < 0)
        return 0;
    off_t ret = ftello(handle);
    fseeko(handle, pos, SEEK_SET);
    return ret;
#else
    struct stat fs;
    const int err = fstat(fileno(handle), &fs);
    return err? 0 : fs.st_size;
#endif
}

#ifdef USE_TILE_LOCAL
vector<string> get_title_files()
{
    vector<string> titles;
    for (const string &dir : _get_base_dirs())
        for (const string &file : get_dir_files_sorted(dir))
            if (file.substr(0, 6) == "title_")
                titles.push_back(file);
    return titles;
}
#endif
