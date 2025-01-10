/**
 * @file
 * @brief Assertions and crashing.
**/

#include "AppHdr.h"

#include <cerrno>
#include <csignal>

#include "abyss.h"
#include "chardump.h"
#include "coordit.h"
#include "crash.h"
#include "dbg-scan.h"
#include "dbg-util.h"
#include "delay.h"
#include "directn.h"
#include "dlua.h"
#include "env.h"
#include "files.h"
#include "hiscores.h"
#include "initfile.h"
#include "item-name.h"
#include "jobs.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "mutation.h"
#include "religion.h"
#include "skills.h"
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"
#include "tiles-build-specific.h"
#include "travel.h"
#include "version.h"
#include "view.h"

#if defined(TARGET_OS_WINDOWS) || defined(TARGET_COMPILER_MINGW)
#define NOCOMM            /* Comm driver APIs and definitions */
#define NOLOGERROR        /* LogError() and related definitions */
#define NOPROFILER        /* Profiler APIs */
#define NOLFILEIO         /* _l* file I/O routines */
#define NOOPENFILE        /* OpenFile and related definitions */
#define NORESOURCE        /* Resource management */
#define NOATOM            /* Atom management */
#define NOLANGUAGE        /* Character test routines */
#define NOLSTRING         /* lstr* string management routines */
#define NODBCS            /* Double-byte character set routines */
#define NOKEYBOARDINFO    /* Keyboard driver routines */
#define NOCOLOR           /* COLOR_* colour values */
#define NODRAWTEXT        /* DrawText() and related definitions */
#define NOSCALABLEFONT    /* Truetype scalable font support */
#define NOMETAFILE        /* Metafile support */
#define NOSYSTEMPARAMSINFO /* SystemParametersInfo() and SPI_* definitions */
#define NODEFERWINDOWPOS  /* DeferWindowPos and related definitions */
#define NOKEYSTATES       /* MK_* message key state flags */
#define NOWH              /* SetWindowsHook and related WH_* definitions */
#define NOCLIPBOARD       /* Clipboard APIs and definitions */
#define NOICONS           /* IDI_* icon IDs */
#define NOMDI             /* MDI support */
#define NOCTLMGR          /* Control management and controls */
#define NOHELP            /* Help support */
#define WIN32_LEAN_AND_MEAN /* No cryptography etc */
#define NONLS             /* All NLS defines and routines */
#define NOSERVICE         /* All Service Controller routines, SERVICE_ equates, etc. */
#define NOKANJI           /* Kanji support stuff. */
#define NOMCX             /* Modem Configuration Extensions */
#include <windows.h>
#undef max

#ifdef USE_TILE_LOCAL
#ifdef TARGET_COMPILER_VC
# include <SDL_syswm.h>
#else
# include <SDL2/SDL_syswm.h>
#endif
#endif
#endif

#ifdef __ANDROID__
# include <android/log.h>
#endif

static string _assert_msg;

static void _dump_compilation_info(FILE* file)
{
    fprintf(file, "Compilation info:\n");
    fprintf(file, "<<<<<<<<<<<\n");
    fprintf(file, "%s", compilation_info);
    fprintf(file, ">>>>>>>>>>>\n\n");
}

extern abyss_state abyssal_state;

static void _dump_level_info(FILE* file)
{
    fprintf(file, "Place info:\n");

    fprintf(file, "branch = %d, depth = %d\n\n",
            (int)you.where_are_you, you.depth);

    string place = level_id::current().describe();

    fprintf(file, "Level id: %s\n", place.c_str());
    if (player_in_branch(BRANCH_ABYSS))
    {
        fprintf(file, "Abyssal state:\n"
                      "    major_coord = (%d,%d)\n"
                      "    seed = 0x%" PRIx32 "\n"
                      "    depth = %" PRIu32 "\n"
                      "    phase = %g\n"
                      "    destroy_all_terrain = %d\n"
                      "    level = (%d : %d)\n",
                abyssal_state.major_coord.x, abyssal_state.major_coord.y,
                abyssal_state.seed, abyssal_state.depth, abyssal_state.phase,
                abyssal_state.destroy_all_terrain,
                abyssal_state.level.branch, abyssal_state.level.depth);
    }

    debug_dump_levgen();
}

static void _dump_player(FILE *file)
{
    // Only dump player info during arena mode if the player is likely
    // the cause of the crash.
    if ((crawl_state.game_is_arena() || crawl_state.arena_suspended)
        && !in_bounds(you.pos()) && you.hp > 0 && you.hp_max > 0
        && you.strength() > 0 && you.intel() > 0 && you.dex() > 0)
    {
        // Arena mode can change behaviour of the rest of the code and/or lead
        // to asserts.
        crawl_state.type            = GAME_TYPE_NORMAL;
        crawl_state.arena_suspended = false;
        return;
    }

    // Arena mode can change behaviour of the rest of the code and/or lead
    // to asserts.
    crawl_state.arena_suspended = false;

    fprintf(file, "Player:\n");
    fprintf(file, "{{{{{{{{{{{\n");

    fprintf(file, "Name:    [%s]\n", you.your_name.c_str());
    fprintf(file, "Species: %s\n", species::name(you.species).c_str());
    fprintf(file, "Job:     %s\n\n", get_job_name(you.char_class));

    fprintf(file, "HP: %d/%d; mods: %d/%d\n", you.hp, you.hp_max,
            you.hp_max_adj_temp, you.hp_max_adj_perm);
    fprintf(file, "MP: %d/%d; mod: %d\n",
            you.magic_points, you.max_magic_points,
            you.mp_max_adj);
    fprintf(file, "Stats: %d %d %d\n",
            you.strength(false),
            you.intel(false),
            you.dex(false));
    fprintf(file, "Position: %s, god: %s (%d), turn_is_over: %d, "
                  "banished: %d\n",
            debug_coord_str(you.pos()).c_str(),
            god_name(you.religion).c_str(), (int) you.religion,
            (int) you.turn_is_over, (int) you.banished);

    if (in_bounds(you.pos()))
    {
        fprintf(file, "Standing on/in/over feature: %s\n",
                raw_feature_description(you.pos()).c_str());
    }

    debug_dump_constriction(&you);
    fprintf(file, "\n");

    if (you.running.runmode != RMODE_NOT_RUNNING)
    {
        fprintf(file, "Runrest:\n");
        fprintf(file, "    mode: %d\n", you.running.runmode);
        fprintf(file, "      mp: %d\n", you.running.mp);
        fprintf(file, "      hp: %d\n", you.running.hp);
        fprintf(file, "     pos: %s\n\n",
                debug_coord_str(you.running.pos).c_str());
    }

    if (!you.delay_queue.empty())
    {
        fprintf(file, "Delayed (%u):\n",
                (unsigned int)you.delay_queue.size());
        for (const auto &delay : you.delay_queue)
        {
            fprintf(file, "    type:     %s", delay->name());
            fprintf(file, "\n");
            fprintf(file, "    duration: %d\n", delay->duration);
        }
        fprintf(file, "\n");
    }

    fprintf(file, "Skills (mode: %s)\n", you.auto_training ? "auto" : "manual");
    fprintf(file, "Name            | can_currently_train | train | training |"
                  " level | points | progress\n");
    for (size_t i = 0; i < NUM_SKILLS; ++i)
    {
        const skill_type sk = skill_type(i);
        if (is_useless_skill(sk))
            continue;

        int needed_min = 0, needed_max = 0;
        if (sk >= 0 && you.skills[sk] <= 27)
            needed_min = skill_exp_needed(you.skills[sk], sk);
        if (sk >= 0 && you.skills[sk] < 27)
            needed_max = skill_exp_needed(you.skills[sk] + 1, sk);

        fprintf(file, "%-16s|          %c          |   %u   |   %3u    |   %2d  | %6d | %d/%d\n",
                skill_name(sk),
                you.can_currently_train[sk] ? 'X' : ' ',
                you.train[sk],
                you.training[sk],
                you.skills[sk],
                you.skill_points[sk],
                you.skill_points[sk] - needed_min,
                max(needed_max - needed_min, 0));
    }
    fprintf(file, "\n");

    fprintf(file, "Spell bugs:\n");
    for (size_t i = 0; i < you.spells.size(); ++i)
    {
        const spell_type spell = you.spells[i];

        if (spell == SPELL_NO_SPELL)
            continue;

        if (!is_valid_spell(spell))
        {
            fprintf(file, "    spell slot #%d: invalid spell #%d\n",
                    (int)i, (int)spell);
            continue;
        }

        const spell_flags flags = get_spell_flags(spell);

        if (flags & spflag::monster)
        {
            fprintf(file, "    spell slot #%d: monster only spell %s\n",
                    (int)i, spell_title(spell));
        }
        else if (flags & spflag::testing)
            fprintf(file, "    spell slot #%d: testing spell %s\n",
                    (int)i, spell_title(spell));
        else if (count_bits(get_spell_disciplines(spell)) == 0)
            fprintf(file, "    spell slot #%d: school-less spell %s\n",
                    (int)i, spell_title(spell));
    }
    fprintf(file, "\n");

    fprintf(file, "Durations:\n");
    for (int i = 0; i < NUM_DURATIONS; ++i)
        if (you.duration[i] != 0)
            fprintf(file, "    #%d: %d\n", i, you.duration[i]);

    fprintf(file, "\n");

    fprintf(file, "Attributes:\n");
    for (int i = 0; i < NUM_ATTRIBUTES; ++i)
        if (you.attribute[i] != 0)
            fprintf(file, "    #%d: %d\n", i, you.attribute[i]);

    fprintf(file, "\n");

    fprintf(file, "Mutations:\n");
    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        mutation_type mut = static_cast<mutation_type>(i);
        int normal = you.mutation[i];
        int innate = you.innate_mutation[i];
        int temp   = you.temp_mutation[i];

        // Normally innate and temp imply normal, but a crash handler should
        // expect the spanish^Wunexpected.
        if (!normal && !innate && !temp)
            continue;

        if (const char* name = mutation_name(mut))
            fprintf(file, "    %s: %d", name, normal);
        else
            fprintf(file, "    unknown #%d: %d", i, normal);

        if (innate)
        {
            if (innate == normal)
                fprintf(file, " (innate)");
            else
                fprintf(file, " (%d innate)", innate);
        }

        if (temp)
        {
            if (temp == normal)
                fprintf(file, " (temporary)");
            else
                fprintf(file, " (%d temporary)", temp);
        }

        fprintf(file, "\n");
    }

    fprintf(file, "\n");

    fprintf(file, "Inventory bugs:\n");
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        item_def &item(you.inv[i]);

        if (item.base_type == OBJ_UNASSIGNED && item.quantity != 0)
        {
            fprintf(file, "    slot #%d: unassigned item has quant %d\n",
                    i, item.quantity);
            continue;
        }
        else if (item.base_type != OBJ_UNASSIGNED && item.quantity < 1)
        {
            const int orig_quant = item.quantity;
            item.quantity = 1;

            fprintf(file, "    slot #%d: otherwise valid item '%s' has "
                          "invalid quantity %d\n",
                    i, item.name(DESC_PLAIN, false, true).c_str(),
                    orig_quant);
            item.quantity = orig_quant;
            continue;
        }
        else if (!item.defined())
            continue;

        const string name = item.name(DESC_PLAIN, false, true);

        if (item.link != i)
        {
            fprintf(file, "    slot #%d: item '%s' has invalid link %d\n",
                    i, name.c_str(), item.link);
        }

        if (item.slot < 0 || item.slot > 127)
        {
            fprintf(file, "    slot #%d: item '%s' has invalid slot %d\n",
                    i, name.c_str(), item.slot);
        }

        if (!item.pos.equals(-1, -1))
        {
            fprintf(file, "    slot #%d: item '%s' has invalid pos %s\n",
                    i, name.c_str(), debug_coord_str(item.pos).c_str());
        }
    }
    fprintf(file, "\n");

    fprintf(file, "Equipment:\n");
    for (player_equip_entry& entry : you.equipment.items)
    {
        fprintf(file, "    eq slot #%d, inv slot #%d", entry.slot, entry.item);
        if (entry.item < 0 || entry.item >= ENDOFPACK)
        {
            fprintf(file, " <invalid>\n");
            continue;
        }
        fprintf(file, ": %s%s%s\n",
                entry.get_item().name(DESC_PLAIN, false, true).c_str(),
                entry.melded ? "(melded)" : "",
                entry.is_overflow ? "(overflow)" : "");
    }
    fprintf(file, "\n");

    if (in_bounds(you.pos()) && monster_at(you.pos()))
    {
        fprintf(file, "Standing on same square as: ");
        const unsigned short midx = env.mgrid(you.pos());

        if (invalid_monster_index(midx))
            fprintf(file, "invalid monster index %d\n", (int) midx);
        else
        {
            const monster* mon = &env.mons[midx];
            fprintf(file, "%s:\n", debug_mon_str(mon).c_str());
            debug_dump_mon(mon, true);
        }
        fprintf(file, "\n");
    }

    fprintf(file, "}}}}}}}}}}}\n\n");
}

static void _debug_marker_scan()
{
    vector<map_marker*> markers = env.markers.get_all();

    for (unsigned int i = 0; i < markers.size(); ++i)
    {
        map_marker* marker = markers[i];

        if (marker == nullptr)
        {
            mprf(MSGCH_ERROR, "Marker #%d is nullptr", i);
            continue;
        }

        map_marker_type type = marker->get_type();

        if (type < MAT_FEATURE || type >= NUM_MAP_MARKER_TYPES)
        {
            mprf(MSGCH_ERROR, "Marker #%d at (%d, %d) has invalid type %d",
                 i, marker->pos.x, marker->pos.y, (int) type);
        }

        if (!in_bounds(marker->pos))
        {
            mprf(MSGCH_ERROR, "Marker #%d, type %d at (%d, %d) out of bounds",
                 i, (int) type, marker->pos.x, marker->pos.y);
            continue;
        }

        vector<map_marker*> at_pos = env.markers.get_markers_at(marker->pos);
        if (find(begin(at_pos), end(at_pos), marker) == end(at_pos))
        {
            mprf(MSGCH_ERROR, "Marker #%d, type %d at (%d, %d) unlinked",
                 i, (int) type, marker->pos.x, marker->pos.y);
        }
    }

    const coord_def start(MAPGEN_BORDER, MAPGEN_BORDER);
    const coord_def   end(GXM - MAPGEN_BORDER - 2, GYM - MAPGEN_BORDER - 2);
    for (rectangle_iterator ri(start, end); ri; ++ri)
    {
        vector<map_marker*> at_pos = env.markers.get_markers_at(*ri);

        for (unsigned int i = 0; i < at_pos.size(); ++i)
        {
            map_marker *marker = at_pos[i];

            if (marker == nullptr)
            {
                mprf(MSGCH_ERROR, "Marker #%d at (%d, %d) nullptr",
                     i, ri->x, ri->y);
                continue;
            }
            if (marker->pos != *ri)
            {
                mprf(MSGCH_ERROR, "Marker #%d, type %d at (%d, %d) "
                                  "thinks it's at (%d, %d)",
                     i, (int) marker->get_type(), ri->x, ri->y,
                     marker->pos.x, marker->pos.y);

                if (!in_bounds(marker->pos))
                    mprf(MSGCH_ERROR, "Further, it thinks it's out of bounds.");
            }
        }
    }
} // _debug_marker_scan()

static void _debug_dump_markers()
{
    vector<map_marker*> markers = env.markers.get_all();

    for (unsigned int i = 0; i < markers.size(); ++i)
    {
        map_marker* marker = markers[i];

        if (marker == nullptr || marker->get_type() == MAT_LUA_MARKER)
            continue;

        mprf(MSGCH_DIAGNOSTICS, "Marker #%d, type %d at (%d, %d): %s",
             i, marker->get_type(),
             marker->pos.x, marker->pos.y,
             marker->debug_describe().c_str());
    }
}

static void _debug_dump_lua_markers(FILE *file)
{
    vector<map_marker*> markers = env.markers.get_all();

    for (unsigned int i = 0; i < markers.size(); ++i)
    {
        map_marker* marker = markers[i];

        if (marker == nullptr || marker->get_type() != MAT_LUA_MARKER)
            continue;

        map_lua_marker* lua_marker = dynamic_cast<map_lua_marker*>(marker);

        string result = lua_marker->debug_to_string();

        if (!result.empty() && result[result.size() - 1] == '\n')
            result = result.substr(0, result.size() - 1);

        fprintf(file, "Lua marker %u at (%d, %d):\n",
                i, marker->pos.x, marker->pos.y);
        fprintf(file, "{{{{\n");
        fprintf(file, "%s", result.c_str());
        fprintf(file, "}}}}\n");
    }
}

static void _debug_dump_lua_persist(FILE* file)
{
    lua_stack_cleaner cln(dlua);

    string result;
    if (!dlua.callfn("persist_to_string", 0, 1))
    {
        result = make_stringf("error (persist_to_string): %s",
                              dlua.error.c_str());
    }
    else if (lua_isstring(dlua, -1))
        result = lua_tostring(dlua, -1);
    else
        result = "persist_to_string() returned nothing";

    fprintf(file, "%s", result.c_str());
}

static void _dump_ver_stuff(FILE* file)
{
    fprintf(file, "Version: %s %s\n", CRAWL, Version::Long);
#if defined(UNIX)
    fprintf(file, "Platform: unix");
#   if defined(TARGET_OS_MACOSX)
    fprintf(file, " (OS X)");
#   endif
    fprintf(file, "\n");
#elif defined(TARGET_OS_WINDOWS)
    fprintf(file, "Platform: Windows\n");
#elif defined(TARGET_OS_DOS)
    fprintf(file, "Platform: DOS\n");
#endif // UNIX

    fprintf(file, "Bits: %d\n", (int)sizeof(void*)*8);
    fprintf(file, "Game mode: %s\n",
            gametype_to_str(crawl_state.type).c_str());

#if defined(USE_TILE_LOCAL)
    fprintf(file, "Tiles: yes\n\n");
#elif defined(USE_TILE_WEB)
    fprintf(file, "Tiles: online\n\n");
#else
    fprintf(file, "Tiles: no\n\n");
#endif
    if (you.fully_seeded)
    {
        fprintf(file, "Seed: %" PRIu64 ", deterministic pregen: %d\n",
            crawl_state.seed, (int) you.deterministic_levelgen);
    }
    if (Version::history_size() > 1)
        fprintf(file, "Version history:\n%s\n\n", Version::history().c_str());
}

static void _dump_command_line(FILE *file)
{
    fprintf(file, "Command line:");
    for (const string& str : crawl_state.command_line_arguments)
        fprintf(file, " %s", str.c_str());
    if (crawl_state.command_line_arguments.empty())
        fprintf(file, " (unknown)");
    fprintf(file, "\n\n");
}

// Dump any game options that could affect stability.
static void _dump_options(FILE *file)
{
    fprintf(file, "RC options:\n");
    fprintf(file, "restart_after_game = %s\n",
            Options.restart_after_game.to_string().c_str());
    fprintf(file, "\n\n");
}

// Defined in end.cc. Not a part of crawl_state, since that's a
// global C++ instance which is free'd by exit() hooks when exit()
// is called, and we don't want to reference free'd memory.
extern bool CrawlIsExiting;

void do_crash_dump()
{
    if (CrawlIsExiting)
    {
        // We crashed during exit() callbacks, so it's likely that
        // any global C++ instances we could reference would be
        // free'd and invalid, plus their content likely wouldn't help
        // tracking it down anyway. Thus, just do the bare bones
        // info to stderr and quit.
        fprintf(stderr, "Crashed while calling exit()!!!!\n");

        _dump_ver_stuff(stderr);

        fprintf(stderr, "%s\n\n", crash_signal_info().c_str());
        write_stack_trace(stderr);
        call_gdb(stderr);

        return;
    }

    // Want same time for file name and crash milestone.
    const time_t t = time(nullptr);

    string dir = (!Options.morgue_dir.empty() ? Options.morgue_dir :
                  !SysEnv.crawl_dir.empty()   ? SysEnv.crawl_dir
                                              : "");

    if (!dir.empty() && dir[dir.length() - 1] != FILE_SEPARATOR)
        dir += FILE_SEPARATOR;

    char name[180] = {};

    snprintf(name, sizeof(name), "%scrash-%s-%s.txt", dir.c_str(),
            you.your_name.c_str(), make_file_time(t).c_str());

    const string signal_info = crash_signal_info();
    const string cause_msg = _assert_msg.empty() ? signal_info : _assert_msg;

    if (!crawl_state.test && !cause_msg.empty())
        fprintf(stderr, "\n%s", cause_msg.c_str());
    // This message is parsed by the WebTiles server. In particular, if you
    // change the line that prints the crash report filename, you must update
    // CrawlProcessHandler._on_process_error.
    fprintf(stderr,
            "\n\nWe crashed! This is likely due to a bug in " CRAWL_SHORT "."
            "\nPlease submit a bug report to:"
            "\n    " CRAWL_BUG_REPORT
            "\nand include at least:"
            "\n- The crash report: %s"
            "\n- Your save file: %s"
            "\n- Information about your computer and game version: %s %s (%s)"
            "\n- A description of what you were doing when this crash occurred.\n\n",
            name, get_savedir_filename(you.your_name).c_str(),
            CRAWL, Version::Long, CRAWL_BUILD_NAME);
    errno = 0;
    // TODO: this freopen of stderr persists into a recursive crash, making it
    // hard to directly log in webtiles...
    FILE* file = crawl_state.test ? stderr : freopen(name, "a+", stderr);

    // The errno values are only relevant when the function in
    // question has returned a value indicating (possible) failure, so
    // only freak out if freopen() returned nullptr!
    if (!file)
    {
        fprintf(stdout, "\nUnable to open file '%s' for writing: %s\n",
                name, strerror(errno));
        file = stdout;
    }

    // Unbuffer the file, since if we recursively crash buffered lines
    // won't make it to the file.
    setvbuf(file, nullptr, _IONBF, 0);

    set_msg_dump_file(file);

    if (!cause_msg.empty())
        fprintf(file, "%s\n\n", cause_msg.c_str());

    _dump_ver_stuff(file);

    _dump_command_line(file);

    _dump_options(file);

    // First get the immediate cause of the crash and the stack trace,
    // since that's most important and later attempts to get more information
    // might themselves cause crashes.
    if (!signal_info.empty())
        fprintf(file, "%s\n\n", signal_info.c_str());
    write_stack_trace(file);
    fprintf(file, "\n");

    call_gdb(file);
    fprintf(file, "\n");

    // Next information on how the binary was compiled
    _dump_compilation_info(file);

    // Next information about the level the player is on, plus level
    // generation info if the crash happened during level generation.
    _dump_level_info(file);

    // Dumping information on marker inconsistency is unlikely to crash,
    // as is dumping the descriptions of non-Lua markers.
    fprintf(file, "Markers:\n");
    fprintf(file, "<<<<<<<<<<<<<<<<<<<<<<\n");
    _debug_marker_scan();
    _debug_dump_markers();
    fprintf(file, ">>>>>>>>>>>>>>>>>>>>>>\n");

    // Dumping current messages is unlikely to crash.
    if (file != stdout)
    {
        fprintf(file, "\nMessages:\n");
        fprintf(file, "<<<<<<<<<<<<<<<<<<<<<<\n");
        string messages = get_last_messages(NUM_STORED_MESSAGES, true);
        fprintf(file, "%s", messages.c_str());
        fprintf(file, ">>>>>>>>>>>>>>>>>>>>>>\n");
    }

    // Dumping the player state and crawl state is next least likely to cause
    // another crash, so do that next.
    fprintf(file, "\nVersion history:\n%s\n", Version::history().c_str());
    crawl_state.dump();
    _dump_player(file);

    // Next item and monster scans. Any messages will be sent straight to
    // the file because of set_msg_dump_file()
#ifdef DEBUG_ITEM_SCAN
    if (crawl_state.crash_debug_scans_safe)
        debug_item_scan();
    else
        fprintf(file, "\nCrashed while loading a save; skipping debug_item_scan.\n");
#endif
#ifdef DEBUG_MONS_SCAN
    if (crawl_state.crash_debug_scans_safe)
        debug_mons_scan();
    else
        fprintf(file, "\nCrashed while loading a save; skipping debug_mons_scan.\n");
#endif

    // Dump Webtiles message buffer.
#ifdef USE_TILE_WEB
    tiles.dump();
#endif

    // Now a screenshot
    if (crawl_state.generating_level)
    {
        fprintf(file, "\nMap:\n");
        dump_map(file, true);
    }
    else
    {
        fprintf(file, "\nScreenshot:\n");
        fprintf(file, "%s\n", screenshot().c_str());
    }

    // If anything has screwed up the Lua runtime stacks then trying to
    // print those stacks will likely crash, so do this after the others.
    fprintf(file, "clua stack:\n");
    clua.print_stack();

    fprintf(file, "dlua stack:\n");
    dlua.print_stack();

    // Lastly try to dump the Lua persistent data and the contents of the Lua
    // markers, since actually running Lua code has the greatest chance of
    // crashing.
    fprintf(file, "Lua persistent data:\n");
    fprintf(file, "<<<<<<<<<<<<<<<<<<<<<<\n");
    _debug_dump_lua_persist(file);
    fprintf(file, ">>>>>>>>>>>>>>>>>>>>>>\n\n");
    fprintf(file, "Lua marker contents:\n");
    fprintf(file, "<<<<<<<<<<<<<<<<<<<<<<\n");
    _debug_dump_lua_markers(file);
    fprintf(file, ">>>>>>>>>>>>>>>>>>>>>>\n");

    set_msg_dump_file(nullptr);

    mark_milestone("crash", cause_msg, "", t);

    if (file != stderr)
        fclose(file);
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

// Assertions and such

NORETURN static void _BreakStrToDebugger(const char *mesg, bool assert)
{
    UNUSED(assert);
// FIXME: this needs a way to get the SDL_window in windowmanager-sdl.cc
#if 0
#if defined(USE_TILE_LOCAL) && defined(TARGET_OS_WINDOWS)
    SDL_SysWMinfo SysInfo;
    SDL_VERSION(&SysInfo.version);
    if (SDL_GetWindowWMInfo(window, &SysInfo) > 0)
    {
        MessageBoxW(window, OUTW(mesg),
                   assert ? L"Assertion failed!" : L"Error",
                   MB_OK|MB_ICONERROR);
    }
    // Print the message to STDERR in addition to the above message box,
    // so it's in the message history if we call Crawl from a shell.
#endif
#endif
    fprintf(stderr, "%s\n", mesg);

#if defined(TARGET_OS_WINDOWS)
    OutputDebugString(mesg);
    if (IsDebuggerPresent())
        DebugBreak();
#endif

    // MSVCRT's abort() give's a funny message ...
    raise(SIGABRT);
    abort();
}

#ifdef ASSERTS
NORETURN void AssertFailed(const char *expr, const char *file, int line,
                           const char *text, ...)
{
    char mesg[512];
    va_list args;

    const char *fileName = file + strlen(file); // strip off path

    while (fileName > file && fileName[-1] != '\\')
        --fileName;

    snprintf(mesg, sizeof(mesg), "ASSERT(%s) in '%s' at line %d failed.",
             expr, fileName, line);

    _assert_msg = mesg;

    // Compose additional information that was passed
    if (text)
    {
        // Write the args into the format specified by text
        char detail[512];
        va_start(args, text);
        vsnprintf(detail, sizeof(detail), text, args);
        va_end(args);
        // Build the final result
        char final_mesg[1026];
        snprintf(final_mesg, sizeof(final_mesg), "%s (%s)", mesg, detail);
        _assert_msg = final_mesg;
        _BreakStrToDebugger(final_mesg, true);
    }
    else
    {
        _assert_msg = mesg;
        _BreakStrToDebugger(mesg, true);
    }
}
#endif

#undef die
NORETURN void die(const char *file, int line, const char *format, ...)
{
    char tmp[2048] = {};
    char mesg[2071] = {};

    va_list args;

    va_start(args, format);
    vsnprintf(tmp, sizeof(tmp), format, args);
    va_end(args);

    snprintf(mesg, sizeof(mesg), "ERROR in '%s' at line %d: %s",
             file, line, tmp);

    _assert_msg = mesg;

    _BreakStrToDebugger(mesg, false);
}

NORETURN void die_noline(const char *format, ...)
{
    char tmp[2048] = {};
    char mesg[2055] = {};

    va_list args;

    va_start(args, format);
    vsnprintf(tmp, sizeof(tmp), format, args);
    va_end(args);

    snprintf(mesg, sizeof(mesg), "ERROR: %s", tmp);

#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "Crawl", "%s", mesg);
#endif

    _assert_msg = mesg;

    _BreakStrToDebugger(mesg, false);
}
