/*
 *  File:       files.cc
 *  Summary:    Functions used to save and load levels/games.
 *  Written by: Linley Henzell and Alexey Guzeev
 */

#include "AppHdr.h"

#include "files.h"

#include <errno.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <algorithm>
#include <functional>

#include <fcntl.h>
#ifdef UNIX
#include <unistd.h>
#endif

#ifdef TARGET_COMPILER_MINGW
#include <io.h>
#endif

#ifdef HAVE_UTIMES
#include <sys/time.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include "externs.h"

#include "abyss.h"
#include "act-iter.h"
#include "artefact.h"
#include "chardump.h"
#include "cloud.h"
#include "clua.h"
#include "coord.h"
#include "coordit.h"
#include "debug.h"
#include "delay.h"
#include "dgn-actions.h"
#include "dgn-overview.h"
#include "directn.h"
#include "dungeon.h"
#include "effects.h"
#include "env.h"
#include "errors.h"
#include "ghost.h"
#include "initfile.h"
#include "items.h"
#include "jobs.h"
#include "kills.h"
#include "libutil.h"
#include "macro.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "mon-act.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-iter.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "mon-transit.h"
#include "notes.h"
#include "options.h"
#include "output.h"
#include "place.h"
#include "player.h"
#include "random.h"
#include "show.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "syscalls.h"
#include "tags.h"
#ifdef USE_TILE
 #include "tiledef-player.h"
 #include "tilepick-p.h"
 #include "tileview.h"
#endif
#include "terrain.h"
#include "travel.h"
#include "hints.h"
#include "viewgeom.h"

#include <dirent.h>

static std::vector<SavefileCallback::callback>* _callback_list = NULL;

static void _save_level(const level_id& lid);

static bool _get_and_validate_version(reader &inf, int &major,
                                      int &minor, std::string* reason = 0);

static bool _determine_ghost_version( FILE *ghostFile,
                                      int &majorVersion, int &minorVersion);

static void _restore_ghost(FILE *ghostFile, const std::string filename,
                           int minorVersion);

static bool _restore_tagged_chunk(package *save, const std::string name,
                                  tag_type tag, const char* complaint);

const short GHOST_SIGNATURE = short( 0xDC55 );

static void _redraw_all(void)
{
    you.redraw_hit_points   = true;
    you.redraw_magic_points = true;
    you.redraw_stats.init(true);
    you.redraw_armour_class = true;
    you.redraw_evasion      = true;
    you.redraw_experience   = true;

    you.redraw_status_flags =
        REDRAW_LINE_1_MASK | REDRAW_LINE_2_MASK | REDRAW_LINE_3_MASK;
}

static std::string _uid_as_string()
{
#ifndef MULTIUSER
    return std::string();
#else
#ifndef SAVE_DIR_PATH
    return make_stringf("-%d", static_cast<int>(getuid()));
#else
    if (SAVE_DIR_PATH[0] != '~')
        return make_stringf("-%d", static_cast<int>(getuid()));
    else
        return std::string();
#endif
#endif
}

static bool _is_uid_file(const std::string &name, const std::string &ext)
{
    std::string save_suffix = get_savedir_filename("", "", "");
    save_suffix += ext;
#ifdef TARGET_OS_DOS
    // Grumble grumble. Hang all retarded operating systems.
    uppercase(save_suffix);
#endif

    save_suffix = save_suffix.substr(get_savefile_directory().length());

    std::string::size_type suffix_pos = name.find(save_suffix);
    return (suffix_pos != std::string::npos
            && suffix_pos == name.length() - save_suffix.length()
            && suffix_pos != 0);
}

bool is_save_file_name(const std::string &name)
{
    return _is_uid_file(name, SAVE_SUFFIX);
}

bool save_exists(const std::string& name)
{
    return (file_exists(get_savedir_filename(name, "", "") + SAVE_SUFFIX));
}

// Returns the save_info from the save.
player_save_info read_character_info(package *save)
{
    player_save_info fromfile;

    // Backup before we clobber "you".
    const player backup(you);
    unwind_var<game_type> gtype(crawl_state.type);

    try // need a redundant try block just so we can restore the backup
    {   // (or risk an = operator on you getting misused)
        if (_restore_tagged_chunk(save, "chr", TAG_CHR, 0))
            fromfile = you;
    }
    catch (ext_fail_exception &E) {}

    you.copy_from(backup);

    return fromfile;
}

#if defined(TARGET_OS_DOS)
// Abbreviates a given file name to DOS style "xxxxxx~1.txt".
// Does not take into account files with differing suffixes or files
// with a prepended path with more than one separator.
// (It does handle all files included with the distribution except
//  changes.stone_soup.)
bool get_dos_compatible_file_name(std::string *fname)
{
    std::string::size_type pos1 = fname->find("\\");
    if (pos1 == std::string::npos)
        pos1 = 0;

    const std::string::size_type pos2 = fname->find(".txt");
    // Name already fits DOS requirements, nothing to be done.
    if (fname->substr(pos1, pos2).length() <= 8)
        return (false);

    *fname = fname->substr(0,pos1) + fname->substr(pos1, pos1 + 6) + "~1.txt";

    return (true);
}
#endif

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
std::vector<std::string> get_dir_files_recursive(const std::string &dirname,
                                                 const std::string &ext,
                                                 int recursion_depth,
                                                 bool include_directories)
{
    std::vector<std::string> files;

    const std::vector<std::string> thisdirfiles = get_dir_files(dirname);
    const int next_recur_depth =
        recursion_depth == -1? -1 : recursion_depth - 1;
    const bool recur = recursion_depth == -1 || recursion_depth > 0;

    for (int i = 0, size = thisdirfiles.size(); i < size; ++i)
    {
        const std::string filename(thisdirfiles[i]);
        if (dir_exists(catpath(dirname, filename)))
        {
            if (include_directories
                && (ext.empty() || ends_with(filename, ext)))
            {
                files.push_back(filename);
            }

            if (recur)
            {
                const std::vector<std::string> subdirfiles =
                    get_dir_files_recursive(catpath(dirname, filename),
                                            ext,
                                            next_recur_depth);
                // Each filename in a subdirectory has to be prefixed
                // with the subdirectory name.
                for (int j = 0, ssize = subdirfiles.size(); j < ssize; ++j)
                    files.push_back(catpath(filename, subdirfiles[j]));
            }
        }
        else
        {
            if (ext.empty() || ends_with(filename, ext))
                files.push_back(filename);
        }
    }
    return (files);
}

std::vector<std::string> get_dir_files_ext(const std::string &dir,
                                           const std::string &ext)
{
    return get_dir_files_recursive(dir, ext, 0);
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

std::string get_base_filename(const std::string &filename)
{
    std::string::size_type pos = filename.rfind(FILE_SEPARATOR);
    if (pos != std::string::npos)
        return filename.substr(pos + 1);
#ifdef ALT_FILE_SEPARATOR
    pos = filename.rfind(ALT_FILE_SEPARATOR);
    if (pos != std::string::npos)
        return filename.substr(pos + 1);
#endif
    return (filename);
}

bool is_absolute_path(const std::string &path)
{
    return (!path.empty()
            && (path[0] == FILE_SEPARATOR
#ifdef TARGET_OS_WINDOWS
                || path.find(':') != std::string::npos
#endif
                ));
}

// Concatenates two paths, separating them with FILE_SEPARATOR if necessary.
// Assumes that the second path is not absolute.
//
// If the first path is empty, returns the second unchanged. The second path
// may be absolute in this case.
std::string catpath(const std::string &first, const std::string &second)
{
    if (first.empty())
        return (second);

    std::string directory = first;
    if (directory[directory.length() - 1] != FILE_SEPARATOR
        && (second.empty() || second[0] != FILE_SEPARATOR))
    {
        directory += FILE_SEPARATOR;
    }
    directory += second;

    return (directory);
}

// Given a relative path and a reference file name, returns the relative path
// suffixed to the directory containing the reference file name. Assumes that
// the second path is not absolute.
std::string get_path_relative_to(const std::string &referencefile,
                                 const std::string &relativepath)
{
    return catpath(get_parent_directory(referencefile),
                   relativepath);
}

std::string change_file_extension(const std::string &filename,
                                  const std::string &ext)
{
    const std::string::size_type pos = filename.rfind('.');
    return ((pos == std::string::npos? filename : filename.substr(0, pos))
            + ext);
}

// Sets the access and modification times of the given file to the current
// time. This is not yet implemented for every supported platform.
void file_touch(const std::string &file)
{
#ifdef HAVE_UTIMES
    utimes(file.c_str(), NULL);
#endif
}

time_t file_modtime(const std::string &file)
{
    struct stat filestat;
    if (stat(file.c_str(), &filestat))
        return (0);

    return (filestat.st_mtime);
}

// Returns true if file a is newer than file b.
bool is_newer(const std::string &a, const std::string &b)
{
    return (file_modtime(a) > file_modtime(b));
}

void check_newer(const std::string &target,
                 const std::string &dependency,
                 void (*action)())
{
    if (is_newer(dependency, target))
        action();
}

static int _create_directory(const char *dir)
{
#if defined(MULTIUSER)
    return mkdir_u(dir, SHARED_FILES_CHMOD_PUBLIC | 0111);
#else
    return mkdir_u(dir, 0755);
#endif
}

static bool _create_dirs(const std::string &dir)
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

        // Handle absolute paths correctly.
        if (i == 0 && dir.size() && dir[0] == FILE_SEPARATOR)
            path = FILE_SEPARATOR + path;

        if (!dir_exists(path) && _create_directory(path.c_str()))
            return (false);

        path += FILE_SEPARATOR;
    }
    return (true);
}

// Checks whether the given path is safe to read from. A path is safe if:
// 1. If Unix: It contains no shell metacharacters.
// 2. If DATA_DIR_PATH is set: the path is not an absolute path.
// 3. If DATA_DIR_PATH is set: the path contains no ".." sequence.
void assert_read_safe_path(const std::string &path) throw (std::string)
{
    // Check for rank tomfoolery first:
    if (path.empty())
        throw "Empty file name.";

#ifdef UNIX
    if (!shell_safe(path.c_str()))
        throw make_stringf("\"%s\" contains bad characters.",
                           path.c_str());
#endif

#ifdef DATA_DIR_PATH
    if (is_absolute_path(path))
        throw make_stringf("\"%s\" is an absolute path.", path.c_str());

    if (path.find("..") != std::string::npos)
    {
        throw make_stringf("\"%s\" contains \"..\" sequences.",
                           path.c_str());
    }
#endif

    // Path is okay.
}

bool is_read_safe_path(const std::string &path)
{
    try
    {
        assert_read_safe_path(path);
    }
    catch (const std::string &)
    {
        return (false);
    }
    return (true);
}

std::string canonicalise_file_separator(const std::string &path)
{
    const std::string sep(1, FILE_SEPARATOR);
    return (replace_all_of(replace_all_of(path, "/", sep),
                           "\\", sep));
}

std::string datafile_path(std::string basename,
                          bool croak_on_fail,
                          bool test_base_path,
                          bool (*thing_exists)(const std::string&))
{
    basename = canonicalise_file_separator(basename);

    if (test_base_path && thing_exists(basename))
        return (basename);

    const std::string rawbases[] = {
#ifdef DATA_DIR_PATH
        DATA_DIR_PATH,
#else
        !SysEnv.crawl_dir.empty()? SysEnv.crawl_dir : "",
#endif
#ifdef TARGET_OS_MACOSX
        SysEnv.crawl_base + "../Resources/",
#endif
    };

    const std::string prefixes[] = {
        std::string("dat") + FILE_SEPARATOR,
#ifdef USE_TILE
        std::string("dat/tiles") + FILE_SEPARATOR,
#endif
        std::string("docs") + FILE_SEPARATOR,
        std::string("settings") + FILE_SEPARATOR,
#ifndef DATA_DIR_PATH
        std::string("..") + FILE_SEPARATOR + "docs" + FILE_SEPARATOR,
        std::string("..") + FILE_SEPARATOR + "dat" + FILE_SEPARATOR,
#ifdef USE_TILE
        std::string("..") + FILE_SEPARATOR + "dat/tiles" + FILE_SEPARATOR,
#endif
        std::string("..") + FILE_SEPARATOR + "settings" + FILE_SEPARATOR,
        std::string("..") + FILE_SEPARATOR,
#endif
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
        for (unsigned p = 0; p < sizeof(prefixes) / sizeof(*prefixes); ++p)
        {
            std::string name = bases[b] + prefixes[p] + basename;
            if (thing_exists(name))
                return (name);
        }

    // Die horribly.
    if (croak_on_fail)
    {
        end(1, false, "Cannot find data file '%s' anywhere, aborting\n",
            basename.c_str());
    }

    return ("");
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
bool check_mkdir(const std::string &whatdir, std::string *dir, bool silent)
{
    if (dir->empty())
        return (true);

    *dir = canonicalise_file_separator(*dir);

    // Suffix the separator if necessary
    if ((*dir)[dir->length() - 1] != FILE_SEPARATOR)
        *dir += FILE_SEPARATOR;

    if (!dir_exists(*dir) && !_create_dirs(*dir))
    {
        if (!silent)
            fprintf(stderr, "%s \"%s\" does not exist "
                    "and I can't create it.\n",
                    whatdir.c_str(), dir->c_str());
        return (false);
    }

    return (true);
}

// Get the directory that contains save files for the current game
// type. This will not be the same as get_base_savedir() for game
// types such as Sprint.
std::string get_savefile_directory(bool ignore_game_type)
{
    std::string dir = Options.save_dir;
    if (!ignore_game_type)
        dir = catpath(dir, crawl_state.game_savedir_path());
    check_mkdir("Save directory", &dir, false);
    if (dir.empty())
        dir = ".";
    return (dir);
}

std::string get_bonefile_directory(bool ignore_game_type)
{
    std::string dir = Options.shared_dir;
    if (!ignore_game_type)
        dir = catpath(dir, crawl_state.game_savedir_path());
    check_mkdir("Bones directory", &dir, false);
    if (dir.empty())
        dir = ".";
    return (dir);
}

// Returns a subdirectory of the current savefile directory as returned by
// get_savefile_directory.
std::string get_savedir_path(const std::string &shortpath)
{
    return canonicalise_file_separator(
        catpath(get_savefile_directory(), shortpath));
}

// Returns the base save directory that contains all saves and cache
// directories. Save files for game type != GAME_TYPE_NORMAL may be
// found in a subdirectory of this dir. Use get_savefile_directory()
// if you want the directory that contains save games for the current
// game type.
std::string get_base_savedir()
{
    return Options.save_dir;
}

// Returns a subdirectory of the base save directory as returned by
// get_base_savedir.
std::string get_base_savedir_path(const std::string &shortpath)
{
    return canonicalise_file_separator(catpath(get_base_savedir(), shortpath));
}

// Given a simple (relative) path, returns the path relative to the
// base save directory and a subdirectory named with the game version.
// This is useful when writing cache files and similar output that
// should not be shared between different game versions.
std::string savedir_versioned_path(const std::string &shortpath)
{
#ifdef DGL_VERSIONED_CACHE_DIR
    const std::string versioned_dir =
        get_base_savedir_path("cache." + Version::Long());
#else
    const std::string versioned_dir = get_base_savedir_path();
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

            while (_readln(fdoll, fbuf))
            {
                if (strcmp(fbuf, "net") == 0)
                    p.held_in_net = true;
            }
        }
    }

    if (!success) // Use default doll instead.
    {
        job_type job = get_job_by_name(p.class_name.c_str());
        if (job == -1)
            job = JOB_FIGHTER;

        tilep_job_default(job, &equip_doll);
    }
    p.doll = equip_doll;
}
#endif

std::vector<player_save_info> find_all_saved_characters()
{
    std::set<std::string> dirs;
    std::vector<player_save_info> saved_characters;
    for (int i = 0; i < NUM_GAME_TYPE; ++i)
    {
        unwind_var<game_type> gt(
            crawl_state.type,
            static_cast<game_type>(i));

        const std::string savedir = get_savefile_directory();
        if (dirs.find(savedir) != dirs.end())
            continue;

        dirs.insert(savedir);

        std::vector<player_save_info> chars_in_dir = find_saved_characters();
        saved_characters.insert(saved_characters.end(),
                                chars_in_dir.begin(),
                                chars_in_dir.end());
    }
    return (saved_characters);
}

/*
 * Returns a list of the names of characters that are already saved for the
 * current user.
 */

std::vector<player_save_info> find_saved_characters()
{
    std::vector<player_save_info> chars;
#ifndef DISABLE_SAVEGAME_LISTS
    std::string searchpath = get_savefile_directory();

    if (searchpath.empty())
        searchpath = ".";

    std::vector<std::string> allfiles = get_dir_files(searchpath);
    for (unsigned int i = 0; i < allfiles.size(); ++i)
    {
        std::string filename = allfiles[i];

        std::string::size_type point_pos = filename.find_first_of('.');
        std::string basename = filename.substr(0, point_pos);

        if (is_save_file_name(filename))
        {
            try
            {
                package save(get_savedir_path(filename).c_str(), false);
                player_save_info p = read_character_info(&save);
                if (!p.name.empty())
                {
#ifdef USE_TILE
                    if (Options.tile_menu_icons && save.has_chunk("tdl"))
                        _fill_player_doll(p, &save);
#endif
                    chars.push_back(p);
                }
            }
            catch (ext_fail_exception &E)
            {
                dprf("%s: %s", filename.c_str(), E.msg.c_str());
            }
        }

    }

    std::sort(chars.rbegin(), chars.rend());
#endif // !DISABLE_SAVEGAME_LISTS
    return (chars);
}

std::string get_savedir_filename(const std::string &prefix,
                                 const std::string &suffix,
                                 const std::string &extension,
                                 bool suppress_uid)
{
    std::string result = get_savefile_directory();
    result += get_save_filename(prefix, suffix, extension, suppress_uid);

#ifdef TARGET_OS_DOS
    uppercase(result);
#endif

    return result;
}

std::string get_save_filename(const std::string &prefix,
                              const std::string &suffix,
                              const std::string &extension,
                              bool suppress_uid)
{
    std::string result = "";

    // Shorten string as appropriate
    result += strip_filename_unsafe_chars(prefix).substr(0, kFileNameLen);

    // Technically we should shorten the string first.  But if
    // MULTIUSER is set we'll have long filenames anyway. Caveat
    // emptor.
    if ( !suppress_uid )
        result += _uid_as_string();

    result += suffix;

    if (!extension.empty())
    {
        result += '.';
        result += extension;
    }

#ifdef TARGET_OS_DOS
    uppercase(result);
#endif
    return result;
}

std::string get_prefs_filename()
{
#ifdef DGL_STARTUP_PREFS_BY_NAME
    return get_savedir_filename("start-" + Options.game.name + "-",
                                "ns", "prf", true);
#else
    return get_savedir_filename("start", "-ns", "prf");
#endif
}

std::string get_level_filename(const level_id& lid)
{
    switch (lid.level_type)
    {
    default:
    case LEVEL_DUNGEON:
        return (make_stringf("%02d%c", lid.depth, lid.branch + 'a'));
    case LEVEL_LABYRINTH:
        return ("lab");
    case LEVEL_ABYSS:
        return ("abs");
    case LEVEL_PANDEMONIUM:
        return ("pan");
    case LEVEL_PORTAL_VAULT:
        return ("ptl");
    }
}

static void _write_ghost_version(writer &outf)
{
    marshallByte(outf, TAG_MAJOR_VERSION);
    marshallByte(outf, TAG_MINOR_VERSION);

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

class safe_file_writer
{
public:
    safe_file_writer(const std::string &filename,
                     const char *mode = "wb",
                     bool _lock = false)
        : target_filename(filename), tmp_filename(target_filename),
          filemode(mode), lock(_lock), filep(NULL)
    {
#ifndef SHORT_FILE_NAMES
        tmp_filename = target_filename + ".tmp";
#endif
    }

    ~safe_file_writer()
    {
        close();
        if (tmp_filename != target_filename)
        {
            if (rename_u(tmp_filename.c_str(), target_filename.c_str()))
                end(1, true, "failed to rename %s -> %s",
                    tmp_filename.c_str(), target_filename.c_str());
        }
        DO_CHMOD_PRIVATE(target_filename.c_str());
    }

    FILE *open()
    {
        if (!filep)
        {
            filep = (lock? lk_open(filemode, tmp_filename)
                     : fopen_u(tmp_filename.c_str(), filemode));
            if (!filep)
                end(-1, true,
                    "Failed to open \"%s\" (%s; locking:%s)",
                    tmp_filename.c_str(),
                    filemode,
                    lock? "YES" : "no");
        }
        return (filep);
    }

    void close()
    {
        if (filep)
        {
            if (lock)
                lk_close(filep, filemode, tmp_filename);
            else
                fclose(filep);
            filep = NULL;
        }
    }

private:
    std::string target_filename, tmp_filename;
    const char *filemode;
    bool lock;

    FILE *filep;
};

static void _write_tagged_chunk(const std::string &chunkname, tag_type tag)
{
    writer outf(you.save, chunkname);

    // write version
    marshallByte(outf, TAG_MAJOR_VERSION);
    marshallByte(outf, TAG_MINOR_VERSION);

    tag_write(tag, outf);
}

static void _place_player_on_stair(level_area_type old_level_type,
                                   branch_type old_branch,
                                   int stair_taken, const coord_def& old_pos)
{
    bool find_first = true;

    // Order is important here.
    if (stair_taken == DNGN_EXIT_PANDEMONIUM)
    {
        stair_taken = DNGN_ENTER_PANDEMONIUM;
        find_first = false;
    }
    else if (stair_taken == DNGN_EXIT_ABYSS)
    {
        stair_taken = DNGN_ENTER_ABYSS;
        find_first = false;
    }
    else if (stair_taken == DNGN_EXIT_HELL)
    {
        stair_taken = DNGN_ENTER_HELL;
    }
    else if (stair_taken == DNGN_ENTER_HELL)
    {
        // The vestibule and labyrinth always start from this stair.
        stair_taken = DNGN_EXIT_HELL;
    }
    else if (stair_taken == DNGN_EXIT_PORTAL_VAULT
             || ((old_level_type == LEVEL_LABYRINTH
                  || old_level_type == LEVEL_PORTAL_VAULT)
                 && (stair_taken == DNGN_ESCAPE_HATCH_DOWN
                     || stair_taken == DNGN_ESCAPE_HATCH_UP)))
    {
        stair_taken = DNGN_EXIT_PORTAL_VAULT;
    }
    else if (player_in_hell() &&
             stair_taken >= DNGN_STONE_STAIRS_DOWN_I &&
             stair_taken <= DNGN_STONE_STAIRS_DOWN_III)
    {
        stair_taken = DNGN_ENTER_HELL;
    }
    else if (stair_taken >= DNGN_STONE_STAIRS_DOWN_I
             && stair_taken <= DNGN_ESCAPE_HATCH_DOWN)
    {
        // Look for corresponding up stair.
        stair_taken += (DNGN_STONE_STAIRS_UP_I - DNGN_STONE_STAIRS_DOWN_I);
    }
    else if (stair_taken >= DNGN_STONE_STAIRS_UP_I
             && stair_taken <= DNGN_ESCAPE_HATCH_UP)
    {
        // Look for coresponding down stair.
        stair_taken += (DNGN_STONE_STAIRS_DOWN_I - DNGN_STONE_STAIRS_UP_I);
    }
    else if (stair_taken >= DNGN_RETURN_FROM_FIRST_BRANCH
             && stair_taken < 150) // 20 slots reserved
    {
        // Find entry point to subdungeon when leaving.
        stair_taken += (DNGN_ENTER_FIRST_BRANCH
                        - DNGN_RETURN_FROM_FIRST_BRANCH);
    }
    else if (stair_taken >= DNGN_ENTER_FIRST_BRANCH
             && stair_taken < DNGN_RETURN_FROM_FIRST_BRANCH)
    {
        // Find exit staircase from subdungeon when entering.
        stair_taken += (DNGN_RETURN_FROM_FIRST_BRANCH
                        - DNGN_ENTER_FIRST_BRANCH);
    }
    else if (stair_taken >= DNGN_ENTER_DIS
             && stair_taken <= DNGN_ENTER_TARTARUS)
    {
        // Only when entering a hell - when exiting, go back to the
        // entry stair.
        if (player_in_hell())
            stair_taken = DNGN_ENTER_HELL;
    }
    else if (stair_taken == DNGN_EXIT_ABYSS)
    {
        stair_taken = DNGN_STONE_STAIRS_UP_I;
    }
    else if (stair_taken == DNGN_ENTER_PORTAL_VAULT)
    {
        stair_taken = DNGN_STONE_ARCH;
    }
    else if (stair_taken == DNGN_ENTER_LABYRINTH)
    {
        // dgn_find_nearby_stair uses special logic for labyrinths.
        stair_taken = DNGN_ENTER_LABYRINTH;
    }
    else // Note: stair_taken can equal things like DNGN_FLOOR
    {
        // Just find a nice empty square.
        stair_taken = DNGN_FLOOR;
        find_first = false;
    }

    const coord_def where_to_go =
        dgn_find_nearby_stair(static_cast<dungeon_feature_type>(stair_taken),
                              old_pos, find_first);
    you.moveto(where_to_go);
}

static void _close_level_gates()
{
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (you.char_direction == GDT_ASCENDING
            && you.level_type != LEVEL_PANDEMONIUM)
        {
            if (feat_sealable_portal(grd(*ri)))
            {
                remove_markers_and_listeners_at(*ri);
                grd(*ri) = DNGN_STONE_ARCH;
            }
        }
    }
}

static void _clear_env_map()
{
    env.map_knowledge.init(map_cell());
}

static void _clear_clouds()
{
    for (int clouty = 0; clouty < MAX_CLOUDS; ++clouty)
        delete_cloud( clouty );
    env.cgrid.init(EMPTY_CLOUD);
}

static bool _grab_follower_at(const coord_def &pos)
{
    if (pos == you.pos())
        return (false);

    monster* fmenv = monster_at(pos);
    if (!fmenv || !fmenv->alive())
        return (false);

    // The monster has to already be tagged in order to follow.
    if (!testbits(fmenv->flags, MF_TAKING_STAIRS))
        return (false);

    // If a monster that can't use stairs was marked as a follower,
    // it's because it's an ally and there might be another ally
    // behind it that might want to push through.
    // This means we don't actually send it on transit, but we do
    // return true, so adjacent real followers are handled correctly. (jpeg)
    if (!mons_can_use_stairs(fmenv))
        return (true);

    level_id dest = level_id::current();
    if (you.char_direction == GDT_GAME_START)
        dest.depth = 1;

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "%s is following to %s.",
         fmenv->name(DESC_CAP_THE, true).c_str(),
         dest.describe().c_str());
#endif
    fmenv->set_transit(dest);
    fmenv->destroy_inventory();
    monster_cleanup(fmenv);
    return (true);
}

static void _grab_followers()
{
    const bool can_follow = level_type_allows_followers(you.level_type);

    int non_stair_using_allies = 0;
    monster* dowan = NULL;
    monster* duvessa = NULL;
    monster* pikel = NULL;

    // Handle nearby ghosts.
    for (adjacent_iterator ai(you.pos()); ai; ++ai)
    {
        monster* fmenv = monster_at(*ai);
        if (fmenv == NULL)
            continue;

        if (mons_is_duvessa(fmenv) && fmenv->alive())
            duvessa = fmenv;

        if (mons_is_dowan(fmenv) && fmenv->alive())
            dowan = fmenv;

        if (fmenv->wont_attack() && !mons_can_use_stairs(fmenv))
            non_stair_using_allies++;

        if (fmenv->type == MONS_PLAYER_GHOST
            && fmenv->hit_points < fmenv->max_hit_points / 2)
        {
            if (fmenv->visible_to(&you))
                mpr("The ghost fades into the shadows.");
            monster_teleport(fmenv, true);
        }

        // From here, we can't fail, so check to see if we've got Pikel
        if (mons_is_pikel(fmenv))
            pikel = fmenv;
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
        if (!dowan->props.exists("can_climb"))
            dowan->flags &= ~MF_TAKING_STAIRS;
    }
    else if (!dowan && duvessa)
    {
        if (!duvessa->props.exists("can_climb"))
            duvessa->flags &= ~MF_TAKING_STAIRS;
    }

    if (can_follow)
    {
        if (non_stair_using_allies > 0)
        {
            // XXX: This assumes that the only monsters that are
            // incapable of using stairs are zombified.
            mprf("Your mindless thrall%s stay%s behind.",
                 non_stair_using_allies > 1 ? "s" : "",
                 non_stair_using_allies > 1 ? ""  : "s");
        }
        memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));
        std::vector<coord_def> places[2];
        int place_set = 0;
        places[place_set].push_back(you.pos());
        while (!places[place_set].empty())
        {
            for (int i = 0, size = places[place_set].size(); i < size; ++i)
            {
                const coord_def &p = places[place_set][i];
                for (adjacent_iterator ai(p); ai; ++ai)
                {
                    if (travel_point_distance[ai->x][ai->y])
                        continue;

                    travel_point_distance[ai->x][ai->y] = 1;
                    if (_grab_follower_at(*ai))
                        places[!place_set].push_back(*ai);
                }
            }
            places[place_set].clear();
            place_set = !place_set;
        }
    }

    // Clear flags of monsters that didn't follow.
    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monster* mons = &menv[i];
        if (!mons->alive())
            continue;
        mons->flags &= ~MF_TAKING_STAIRS;
    }

    if (pikel && !pikel->alive())
        pikel_band_neutralise(true);
}

// Should be called after _grab_followers(), so that items carried by
// followers won't be considered lost.
static void _do_lost_items(level_area_type old_level_type)
{
    if (old_level_type == LEVEL_DUNGEON)
        return;

    for (int i = 0; i < MAX_ITEMS; i++)
    {
        item_def& item(mitm[i]);

        if (!item.defined())
            continue;

        // Item is in player inventory, so it's not lost.
        if (item.pos == coord_def(-1,-1))
            continue;

        item_was_lost(item);
    }
}

bool load(dungeon_feature_type stair_taken, load_mode_type load_mode,
          const level_id& old_level)
{
    unwind_var<dungeon_feature_type> stair(
        you.transit_stair, stair_taken, DNGN_UNSEEN);

    unwind_bool ylev(you.entering_level, load_mode != LOAD_VISITOR, false);

#ifdef DEBUG_LEVEL_LOAD
    mprf(MSGCH_DIAGNOSTICS, "Loading... level type: %d, branch: %d, level: %d",
                            you.level_type, you.where_are_you, you.absdepth0);
#endif

    // Save player position for shaft, hatch destination.
    const coord_def old_pos = you.pos();

    // Going up/down stairs, going through a portal, or being banished
    // means the previous x/y movement direction is no longer valid.
    you.reset_prev_move();

    const bool make_changes =
        (load_mode == LOAD_START_GAME || load_mode == LOAD_ENTER_LEVEL);

    bool just_created_level = false;

    std::string level_name = get_level_filename(level_id::current());

    if (you.level_type == LEVEL_DUNGEON && old_level.level_type == LEVEL_DUNGEON
        || load_mode == LOAD_START_GAME && you.char_direction != GDT_GAME_START)
    {
        const level_id current(level_id::current());
        if (Generated_Levels.find(current) == Generated_Levels.end())
        {
            // Make sure the old file is gone.
            you.save->delete_chunk(level_name);

            // Save the information for later deletion -- DML 6/11/99
            Generated_Levels.insert(current);
        }
    }

    you.prev_targ     = MHITNOT;
    you.prev_grd_targ.reset();

    // We clear twice - on save and on load.
    // Once would be enough...
    if (make_changes)
        _clear_clouds();

    // Lose all listeners.
    dungeon_events.clear();

    // This block is to grab followers and save the old level to disk.
    if (load_mode == LOAD_ENTER_LEVEL && old_level.depth != -1)
    {
        _grab_followers();

        if (old_level.level_type == LEVEL_DUNGEON
            || old_level.level_type != you.level_type)
        {
            _save_level(old_level);
        }
    }

    if (load_mode == LOAD_ENTER_LEVEL)
    {
        _do_lost_items(old_level.level_type);

        // The player is now between levels.
        you.position.reset();
    }

#ifdef USE_TILE
    if (load_mode != LOAD_VISITOR)
    {
        tiles.clear_minimap();
        crawl_view_buffer empty_vbuf;
        tiles.load_dungeon(empty_vbuf, crawl_view.vgrdc);
    }
#endif

    // Try to open level savefile.
#ifdef DEBUG_LEVEL_LOAD
    mprf(MSGCH_DIAGNOSTICS, "Try to open file %s", level_name.c_str());
#endif

    // GENERATE new level when the file can't be opened:
    if (!you.save->has_chunk(level_name))
    {
#ifdef DEBUG_LEVEL_LOAD
        mpr("Generating new file...", MSGCH_DIAGNOSTICS);
#endif
        ASSERT( load_mode != LOAD_VISITOR );
        env.turns_on_level = -1;

        if (you.char_direction == GDT_GAME_START
            && you.level_type == LEVEL_DUNGEON)
        {
            // If we're leaving the Abyss for the first time as a Chaos
            // Knight of Lugonu (who start out there), force a return
            // into the first dungeon level and enable normal monster
            // generation.
            you.absdepth0 = 0;
            you.char_direction = GDT_DESCENDING;
            Generated_Levels.insert(level_id::current());
        }

#ifdef USE_TILE
        tile_init_default_flavour();
        tile_clear_flavour();
#endif

        _clear_env_map();
        builder(you.absdepth0, you.level_type);
        just_created_level = true;

        if (!crawl_state.game_is_tutorial()
            && (you.absdepth0 > 1 || you.level_type != LEVEL_DUNGEON)
            && one_chance_in(3))
        {
            load_ghost(true);
        }
        env.turns_on_level = 0;
        // sanctuary
        env.sanctuary_pos  = coord_def(-1, -1);
        env.sanctuary_time = 0;
    }
    else
    {
#ifdef DEBUG_LEVEL_LOAD
        mpr("Loading old file...", MSGCH_DIAGNOSTICS);
#endif
        _restore_tagged_chunk(you.save, level_name, TAG_LEVEL, "Level file is invalid.");

        // POST-LOAD tasks :
        link_items();
        _redraw_all();
    }

    // Clear map knowledge stair emphasis.
    show_update_emphasis();

    los_changed();

    // Closes all the gates if you're on the way out.
    // Before marker activation since it removes some.
    if (make_changes && you.char_direction == GDT_ASCENDING
        && you.level_type != LEVEL_PANDEMONIUM)
    {
        _close_level_gates();
    }

    // Markers must be activated early, since they may rely on
    // events issued later, e.g. DET_ENTERING_LEVEL or
    // the DET_TURN_ELAPSED from update_level.
    if (make_changes || load_mode == LOAD_RESTART_GAME)
        env.markers.activate_all();

    if (make_changes && env.elapsed_time && !just_created_level)
        update_level(you.elapsed_time - env.elapsed_time);

    // Apply all delayed actions, if any.
    if (just_created_level)
        env.dactions_done = you.dactions.size();
    else
        catchup_dactions();

    // Here's the second cloud clearing, on load (see above).
    if (make_changes)
    {
        _clear_clouds();
        if (you.level_type != LEVEL_ABYSS)
        {
            _place_player_on_stair(old_level.level_type,
                                   old_level.branch, stair_taken, old_pos);
        }
        else
            you.moveto(ABYSS_CENTRE);

        // This should fix the "monster occurring under the player" bug.
        if (monster* mon = monster_at(you.pos()))
            monster_teleport(mon, true, true);
    }

    crawl_view.set_player_at(you.pos(), load_mode != LOAD_VISITOR);

    // Actually "move" the followers if applicable.
    if (level_type_allows_followers(you.level_type)
        && load_mode == LOAD_ENTER_LEVEL)
    {
        place_followers();
    }

    // Load monsters in transit.
    if (load_mode == LOAD_ENTER_LEVEL)
    {
        place_transiting_monsters();
        place_transiting_items();
    }

    if (make_changes)
    {
        // Tell stash-tracker and travel that we've changed levels.
        trackers_init_new_level(true);
#ifdef USE_TILE
        tile_new_level(just_created_level);
#endif
    }
    else if (load_mode == LOAD_RESTART_GAME)
    {
        // Travel needs initialize some things on reload, too.
        travel_init_load_level();
    }

    _redraw_all();

    if (load_mode != LOAD_VISITOR)
        dungeon_events.fire_event(DET_ENTERING_LEVEL);

    // Things to update for player entering level
    if (load_mode == LOAD_ENTER_LEVEL)
    {
        if (just_created_level)
            level_welcome_messages();

        // Centaurs have difficulty with stairs
        int timeval = ((you.species != SP_CENTAUR) ? player_movement_speed()
                                                   : 15);

        // new levels have less wary monsters:
        if (just_created_level)
            timeval /= 2;

        timeval -= (stepdown_value( check_stealth(), 50, 50, 150, 150 ) / 10);

        dprf("arrival time: %d", timeval );

        if (timeval > 0)
        {
            you.time_taken = timeval;
            handle_monsters();
        }

        if (just_created_level)
            run_map_epilogues();
    }

    // Save the created/updated level out to disk:
    if (make_changes)
        _save_level(level_id::current());

    setup_environment_effects();

    setup_vault_mon_list();
    setup_feature_descs_short();

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
                && (old_level.branch != you.where_are_you
                    || old_level.level_type != you.level_type)))
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
        curr_PlaceInfo.assert_validity();
    }

    if (just_created_level)
        you.attribute[ATTR_ABYSS_ENTOURAGE] = 0;

    if (load_mode != LOAD_VISITOR)
        dungeon_events.fire_event(DET_ENTERED_LEVEL);

    if (load_mode == LOAD_ENTER_LEVEL)
    {
        // 50% chance of repelling the stair you just came through.
        if (you.duration[DUR_REPEL_STAIRS_MOVE]
            || you.duration[DUR_REPEL_STAIRS_CLIMB])
        {
            dungeon_feature_type feat = grd(you.pos());
            if (feat != DNGN_ENTER_SHOP
                && feat_stair_direction(feat) != CMD_NO_CMD
                && feat_stair_direction(stair_taken) != CMD_NO_CMD)
            {
                std::string stair_str =
                    feature_description(feat, NUM_TRAPS, "",
                                        DESC_CAP_THE, false);
                std::string verb = stair_climb_verb(feat);

                if (coinflip()
                    && slide_feature_over(you.pos(), coord_def(-1, -1), false))
                {
                    mprf("%s slides away from you right after you %s it!",
                         stair_str.c_str(), verb.c_str());
                }

                if (coinflip())
                {
                    // Stairs stop fleeing from you now you actually caught one.
                    mprf("%s settles down.", stair_str.c_str(), verb.c_str());
                    you.duration[DUR_REPEL_STAIRS_MOVE]  = 0;
                    you.duration[DUR_REPEL_STAIRS_CLIMB] = 0;
                }
            }
        }

        // If butchering was interrupted by switching levels (banishment)
        // then switch back from butchering tool if there's no hostiles
        // nearby.
        handle_interrupted_swap(true);

        // Forget about interrupted butchering, since we probably aren't going
        // to get back to the corpse in time to finish things.
        // But do not reset the weapon swap if we swapped weapons
        // because of a transformation.
        maybe_clear_weapon_swap();
    }

    return just_created_level;
}

void _save_level(const level_id& lid)
{
    travel_cache.get_level_info(lid).update();

    // Nail all items to the ground.
    fix_item_coordinates();

    _write_tagged_chunk(get_level_filename(lid), TAG_LEVEL);
}

#define SAVEFILE(file, savefn) \
    do                        \
    {                         \
        writer w(you.save, file);           \
        savefn(w);                \
    } while (false)

// Stack allocated std::string's go in separate function, so Valgrind doesn't
// complain.
static void _save_game_base()
{
    /* Stashes */
    SAVEFILE("st", StashTrack.save);

#ifdef CLUA_BINDINGS
    /* lua */
    SAVEFILE("lua", clua.save);
#endif

    /* kills */
    SAVEFILE("kil", you.kills->save);

    /* travel cache */
    SAVEFILE("tc", travel_cache.save);

    /* notes */
    SAVEFILE("nts", save_notes);

    /* hints mode */
    if (Hints.hints_left)
        SAVEFILE("tut", save_hints);

    /* messages */
    SAVEFILE("msg", save_messages);

    /* tile dolls (empty for ASCII)*/
#ifdef USE_TILE
    // Save the current equipment into a file.
    SAVEFILE("tdl", save_doll_file);
#endif

    _write_tagged_chunk("you", TAG_YOU);
    _write_tagged_chunk("chr", TAG_CHR);
}

// Stack allocated std::string's go in separate function, so Valgrind doesn't
// complain.
static void _save_game_exit()
{
    // Prompt for saving macros.
    if (crawl_state.unsaved_macros
        && !crawl_state.seen_hups
        && !crawl_state.game_wants_emergency_save
        && yesno("Save macros?", true, 'n'))
    {
        macro_save();
    }

    // Must be exiting -- save level & goodbye!
    if (!you.entering_level)
        _save_level(level_id::current());

    clrscr();

#ifdef DGL_WHEREIS
    whereis_record("saved");
#endif

    if (_callback_list != NULL)
    {
        delete _callback_list;
        _callback_list = NULL;
    }

    delete you.save;
    you.save = 0;
}

void save_game(bool leave_game, const char *farewellmsg)
{
    unwind_bool saving_game(crawl_state.saving_game, true);

    SavefileCallback::pre_save();

    // Stack allocated std::string's go in separate function,
    // so Valgrind doesn't complain.
    _save_game_base();

    // If just save, early out.
    if (!leave_game)
    {
        you.save->commit();
        return;
    }

    // Stack allocated std::string's go in separate function,
    // so Valgrind doesn't complain.
    _save_game_exit();

    // Exit unless this is an emergency save, in which case let the
    // crash handler re-raise the crashy signal.
    if (!crawl_state.game_wants_emergency_save)
    {
        end(0, false, farewellmsg? "%s" : "See you soon, %s!",
            farewellmsg? farewellmsg : you.your_name.c_str());
    }
}

// Saves the game without exiting.
void save_game_state()
{
    save_game(false);
    if (crawl_state.seen_hups)
        save_game(true);
}

static std::string _make_portal_vault_ghost_suffix()
{
    return you.level_type_ext.empty()? "ptl" : you.level_type_ext;
}

static std::string _make_ghost_filename()
{
    std::string suffix;
    if (you.level_type == LEVEL_PORTAL_VAULT)
        suffix = _make_portal_vault_ghost_suffix();
    else
        suffix = get_level_filename(level_id::current());
    return get_bonefile_directory() + "bones." + suffix;
}

#define BONES_DIAGNOSTICS (defined(WIZARD) || defined(DEBUG_BONES) | defined(DEBUG_DIAGNOSTICS))

bool load_ghost(bool creating_level)
{
    const bool wiz_cmd = (crawl_state.prev_cmd == CMD_WIZARD);

    ASSERT(you.transit_stair == DNGN_UNSEEN || creating_level);
    ASSERT(!you.entering_level || creating_level);
    ASSERT(!creating_level
           || (you.entering_level && you.transit_stair != DNGN_UNSEEN));
    // Only way to load a ghost without creating a level is via a wizard
    // command.
    ASSERT(creating_level || wiz_cmd);

#if !defined(DEBUG) && !defined(WIZARD)
    UNUSED(creating_level);
#endif

#ifdef BONES_DIAGNOSTICS

    bool do_diagnostics = false;
#ifdef WIZARD
    do_diagnostics = !creating_level;
#endif
#if defined(DEBUG_BONES) || defined(DEBUG_DIAGNOSTICS)
    do_diagnostics = true;
#endif

#endif // BONES_DIAGNOSTICS

    int majorVersion;
    int minorVersion;

    const std::string cha_fil = _make_ghost_filename();
    FILE *gfile = fopen_u(cha_fil.c_str(), "rb");

    if (gfile == NULL)
    {
        if (wiz_cmd && !creating_level)
            mpr("No ghost files for this level.", MSGCH_PROMPT);
        return (false);                 // no such ghost.
    }

    if (!_determine_ghost_version(gfile, majorVersion, minorVersion))
    {
        fclose(gfile);
#ifdef BONES_DIAGNOSTICS
        if (do_diagnostics)
        {
            mprf(MSGCH_DIAGNOSTICS,
                 "Ghost file \"%s\" seems to be invalid.", cha_fil.c_str());
            more();
        }
#endif
        return (false);
    }

    if (majorVersion != TAG_MAJOR_VERSION || minorVersion > TAG_MINOR_VERSION)
    {
#ifdef BONES_DIAGNOSTICS
        if (do_diagnostics)
        {
            if (majorVersion != TAG_MAJOR_VERSION)
                mprf(MSGCH_DIAGNOSTICS,
                     "Ghost file major version mismatch");
            else
                mprf(MSGCH_DIAGNOSTICS,
                     "Ghost file minor version is too high.");
            more();
        }
#endif
        fclose(gfile);
        unlink_u(cha_fil.c_str());
        return (false);
    }

    ghosts.clear();
    _restore_ghost(gfile, cha_fil, minorVersion);
    fclose(gfile);

    // FIXME: This message will have to be shortened again as trunk reaches
    //        0.6 state and players using old bones becomes increasingly less
    //        likely.
    if (!debug_check_ghosts())
    {
        mprf(MSGCH_DIAGNOSTICS,
             "Refusing to load buggy ghost from file \"%s\"! "
             "Note that all bones files from 0.4.x are invalid, so you should "
             "delete them. If this is a newer ghost, please submit a bug "
             "report.",
             cha_fil.c_str());

        return (false);
    }

#ifdef BONES_DIAGNOSTICS
    if (do_diagnostics)
    {
        mprf(MSGCH_DIAGNOSTICS, "Loaded ghost file with %u ghost(s)",
             ghosts.size());
    }
#endif

    // Remove bones file - ghosts are hardly permanent.
    unlink_u(cha_fil.c_str());

#ifdef BONES_DIAGNOSTICS
    unsigned long unplaced_ghosts = ghosts.size();
    bool          ghost_errors    = false;
#endif

    // Translate ghost to monster and place.
    for (int imn = 0; imn < MAX_MONSTERS - 10 && !ghosts.empty(); ++imn)
    {
        if (menv[imn].alive())
            continue;

        menv[imn].set_ghost(ghosts[0]);
        menv[imn].ghost_init();
        if (menv[imn].has_spells())
            menv[imn].bind_spell_flags();

        ghosts.erase(ghosts.begin());
#ifdef BONES_DIAGNOSTICS
        if (do_diagnostics)
        {
            unplaced_ghosts--;
            if (!menv[imn].alive())
            {
                mpr("Placed ghost is not alive.", MSGCH_DIAGNOSTICS);
                ghost_errors = true;
            }
            else if (menv[imn].type != MONS_PLAYER_GHOST)
            {
                mprf(MSGCH_DIAGNOSTICS,
                     "Placed ghost is not MONS_PLAYER_GHOST, but %s",
                     menv[imn].name(DESC_PLAIN, true).c_str());
                ghost_errors = true;
            }
        }
#endif
    }

#ifdef BONES_DIAGNOSTICS
    if (do_diagnostics && unplaced_ghosts > 0)
    {
        mprf(MSGCH_DIAGNOSTICS, "Unable to place %u ghost(s)",
             ghosts.size());
        ghost_errors = true;
    }
    if (ghost_errors)
        more();
#endif

    return (true);
}

void restore_game(const std::string& name)
{
    you.save = new package((get_savedir_filename(name, "", "")
                           + SAVE_SUFFIX).c_str(), true);

    _restore_tagged_chunk(you.save, "chr", TAG_CHR, "Player data is invalid.");
    _restore_tagged_chunk(you.save, "you", TAG_YOU, "Save data is invalid.");

    const int minorVersion = crawl_state.minorVersion;

    if (you.save->has_chunk("st"))
    {
        reader inf(you.save, "st", minorVersion);
        StashTrack.load(inf);
    }

#ifdef CLUA_BINDINGS
    if (you.save->has_chunk("lua"))
    {
        std::vector<char> buf;
        chunk_reader inf(you.save, "lua");
        inf.read_all(buf);
        buf.push_back(0);
        clua.execstring(&buf[0]);
    }
#endif

    if (you.save->has_chunk("kil"))
    {
        reader inf(you.save, "kil", minorVersion);
        you.kills->load(inf);
    }

    if (you.save->has_chunk("tc"))
    {
        reader inf(you.save, "tc", minorVersion);
        travel_cache.load(inf, minorVersion);
    }

    if (you.save->has_chunk("nts"))
    {
        reader inf(you.save, "nts", minorVersion);
        load_notes(inf);
    }

    /* hints mode */
    if (you.save->has_chunk("tut"))
    {
        reader inf(you.save, "tut", minorVersion);
        load_hints(inf);
    }

    /* messages */
    if (you.save->has_chunk("msg"))
    {
        reader inf(you.save, "msg", minorVersion);
        load_messages(inf);
    }

    SavefileCallback::post_restore();
}

static void _load_level(const level_id &level)
{
    // Load the given level.
    you.where_are_you = level.branch;
    you.absdepth0 = level.dungeon_absdepth();
    you.level_type = level.level_type;

    load(DNGN_STONE_STAIRS_DOWN_I, LOAD_VISITOR, level_id());
}

// Given a level returns true if the level has been created already
// in this game.
bool is_existing_level(const level_id &level)
{
    return (Generated_Levels.find(level) != Generated_Levels.end());
}

// This class provides a way to walk the dungeon with a bit more flexibility
// than you used to get with apply_to_all_dungeons.
level_excursion::level_excursion()
    : original(level_id::current()), ever_changed_levels(false)
{
}

void level_excursion::go_to(const level_id& next)
{
    if (level_id::current() != next)
    {
        ever_changed_levels = true;

        _save_level(level_id::current());
        _load_level(next);

        LevelInfo &li = travel_cache.get_level_info(next);
        li.set_level_excludes();
    }

    you.on_current_level = (level_id::current() == original);

    // Don't let uncommitted writes accumulate.
    you.save->commit();
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

        // Reactivate markers.
        env.markers.activate_all();
    }
}

bool get_save_version(reader &file, int &major, int &minor)
{
    // Read first two bytes.
    uint8_t buf[2];
    try
    {
        file.read(buf, 2);
    }
    catch (short_read_exception E)
    {
        // Empty file?
        major = minor = -1;
        return (false);
    }

    major = buf[0];
    minor = buf[1];

    return (true);
}

static bool _get_and_validate_version(reader &inf, int &major,
                                      int &minor, std::string* reason)
{
    std::string dummy;
    if (reason == 0)
        reason = &dummy;

    if (!get_save_version(inf, major, minor))
    {
        *reason = "File is corrupt.";
        return (false);
    }

    if (major != TAG_MAJOR_VERSION)
    {
        if (Version::ReleaseType() == Version::FINAL)
        {
            // FIXME: The actual major version should also be handled
            //        dynamically, but I think <major>.<minor> also
            //        covers 0.6.2. If not, it's not a problem. (jpeg)
            *reason = CRAWL " " + Version::Short() + " is not compatible with "
                      "save files older than 0.7. You can continue your game "
                      "with the appropriate older version, or you can delete "
                      "it and start a new game.";
        }
        else
        {
            *reason = make_stringf("Major version mismatch: %d (want %d).",
                                   major, TAG_MAJOR_VERSION);
        }
        return (false);
    }

    if (minor < 0)
    {
        *reason = make_stringf("Minor version %d is negative!",
                               minor);
        return (false);
    }

    if (minor > TAG_MINOR_VERSION)
    {
        *reason = make_stringf("Minor version mismatch: %d (want <= %d). "
                               "The save is from a newer version.",
                               minor, TAG_MINOR_VERSION);
        return (false);
    }

    return (true);
}

static bool _restore_tagged_chunk(package *save, const std::string name,
                                  tag_type tag, const char* complaint)
{
    reader inf(save, name);

    int majorVersion, minorVersion;
    std::string reason;
    if (!_get_and_validate_version(inf, majorVersion, minorVersion, &reason))
    {
        if (!complaint)
        {
            dprf("chunk %s: %s", name.c_str(), reason.c_str());
            return false;
        }
        else
        {
            char msg[128];
            snprintf(msg, sizeof(msg), "\n%s  %s\n", complaint, reason.c_str());
            print_error_screen(msg);
            end(-1, false, msg);
        }
    }

    crawl_state.minorVersion = minorVersion;

    try
    {
        tag_read(inf, minorVersion, tag);
    }
    catch (short_read_exception &E)
    {
        fail("truncated save chunk (%s)", name.c_str());
    };

    inf.fail_if_not_eof(name);
    return true;
}

static bool _determine_ghost_version( FILE *ghostFile,
                                      int &majorVersion, int &minorVersion )
{
    // Read first two bytes.
    uint8_t buf[2];
    if (read2(ghostFile, buf, 2) != 2)
        return (false);               // empty file?

    // Otherwise, read version and validate.
    majorVersion = buf[0];
    minorVersion = buf[1];

    reader inf(ghostFile, minorVersion);
    // Check for the DCSS ghost signature.
    if (unmarshallShort(inf) != GHOST_SIGNATURE)
        return (false);

    if (majorVersion == TAG_MAJOR_VERSION
        && minorVersion <= TAG_MINOR_VERSION)
    {
        // Discard three more 32-bit words of padding.
        try // don't crash on corrupted ghosts
        {
            inf.read(NULL, 3*4);
        }
        catch (short_read_exception &E)
        {
            return (false);
        }
        return !feof(ghostFile);
    }

    // If its not TAG_MAJOR_VERSION, no idea!
    return (false);
}

static void _restore_ghost(FILE *ghostFile, std::string filename, int minorVersion)
{
    reader inf(ghostFile, minorVersion);
    tag_read(inf, minorVersion, TAG_GHOST);
    inf.fail_if_not_eof(filename);
}

void save_ghost( bool force )
{
#ifdef BONES_DIAGNOSTICS

    bool do_diagnostics = false;
#ifdef WIZARD
    do_diagnostics = you.wizard;
#endif
#if defined(DEBUG_BONES) || defined(DEBUG_DIAGNOSTICS)
    do_diagnostics = true;
#endif

#endif // BONES_DIAGNOSTICS

    // No ghosts on levels 1, 2, or the ET.
    if (!force && (you.absdepth0 < 2
                   || you.where_are_you == BRANCH_ECUMENICAL_TEMPLE))
    {
        return;
    }

    const std::string cha_fil = _make_ghost_filename();
    FILE *gfile = fopen_u(cha_fil.c_str(), "rb");

    // Don't overwrite existing bones!
    if (gfile != NULL)
    {
#ifdef BONES_DIAGNOSTICS
        if (do_diagnostics)
            mpr("Ghost file for this level already exists.",
                MSGCH_DIAGNOSTICS);
#endif
        fclose(gfile);
        return;
    }

    ghosts = ghost_demon::find_ghosts();

    if (ghosts.empty())
    {
#ifdef BONES_DIAGNOSTICS
        if (do_diagnostics)
            mpr("Could not find any ghosts for this level.",
                MSGCH_DIAGNOSTICS);
#endif
        return;
    }

    safe_file_writer sw(cha_fil, "wb", true);
    writer outw(cha_fil, sw.open());

    _write_ghost_version(outw);
    tag_write(TAG_GHOST, outw);

#ifdef BONES_DIAGNOSTICS
    if (do_diagnostics)
        mprf(MSGCH_DIAGNOSTICS, "Saved ghost (%s).", cha_fil.c_str() );
#endif
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
    FILE *handle = fopen_u(file.c_str(), mode);
    if (!handle)
        return NULL;
#ifdef SHARED_FILES_CHMOD_PUBLIC
    fchmod(fileno(handle), SHARED_FILES_CHMOD_PUBLIC);
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

#ifdef SHARED_FILES_CHMOD_PUBLIC
    if (mode && mode[0] == 'w')
        fchmod(fileno(handle), SHARED_FILES_CHMOD_PUBLIC);
#endif

    // actually close
    fclose(handle);
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

/////////////////////////////////////////////////////////////////////////////
// SavefileCallback
//
// Callbacks which are called before a save and after a restore.  Can be used
// to move stuff in and out of you.props, or on a restore to recalculate data
// which isn't stored in the savefile.  Declare a SavefileCallback variable
// using a C++ global constructor to register the callback.
//
// XXX: Due to some weirdness with C++ global constructors (see below) I'm
// not sure if this will work for all compiler/system combos, so make any
// code which uses this fail gracefully if the callbacks aren't called.

SavefileCallback::SavefileCallback(callback func)
{
    ASSERT(func != NULL);

    // XXX: For some reason (at least with GCC 4.3.2 on Linux) if the
    // callback list is made with a global contructor then it gets emptied
    // out by the time that pre_save() or post_restore() is called,
    // probably having something to do with the fact that global
    // contructors are also used to add the callbacks.  Thus we have to do
    // it this way.
    if (_callback_list == NULL)
        _callback_list = new std::vector<SavefileCallback::callback>();

    _callback_list->push_back(func);
}

void SavefileCallback::pre_save()
{
    ASSERT(crawl_state.saving_game);

    if (_callback_list == NULL)
        return;

    for (unsigned int i = 0; i < _callback_list->size(); i++)
    {
        callback func = (*_callback_list)[i];
        (*func)(true);
    }
}

void SavefileCallback::post_restore()
{
    ASSERT(!crawl_state.saving_game);

    if (_callback_list == NULL)
        return;

    for (unsigned int i = 0; i < _callback_list->size(); i++)
    {
        callback func = (*_callback_list)[i];
        (*func)(false);
    }
}

FILE *fopen_replace(const char *name)
{
    int fd;

    // Stave off symlink attacks.  Races will be handled with O_EXCL.
    unlink_u(name);
    fd = open_u(name, O_CREAT|O_EXCL|O_WRONLY, 0666);
    if (fd == -1)
        return 0;
    return fdopen(fd, "w");
}
