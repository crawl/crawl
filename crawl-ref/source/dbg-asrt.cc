/**
 * @file
 * @brief Assertions and crashing.
**/

#include "AppHdr.h"

#include <errno.h>
#include <signal.h>

#include "abyss.h"
#include "chardump.h"
#include "clua.h"
#include "coord.h"
#include "coordit.h"
#include "crash.h"
#include "dbg-scan.h"
#include "dbg-util.h"
#include "directn.h"
#include "dlua.h"
#include "dungeon.h"
#include "env.h"
#include "initfile.h"
#include "itemname.h"
#include "jobs.h"
#include "libutil.h"
#include "mapmark.h"
#include "message.h"
#include "monster.h"
#include "mon-util.h"
#include "mutation.h"
#include "options.h"
#include "religion.h"
#include "skills2.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "travel.h"
#include "hiscores.h"
#include "version.h"
#include "view.h"
#include "zotdef.h"

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
# include <SDL/SDL_syswm.h>
#endif
#endif
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
                      "    nuke_all = %d\n",
                abyssal_state.major_coord.x, abyssal_state.major_coord.y,
                abyssal_state.seed, abyssal_state.depth, abyssal_state.phase,
                abyssal_state.nuke_all);
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
        // Arena mode can change behavior of the rest of the code and/or lead
        // to asserts.
        crawl_state.type            = GAME_TYPE_NORMAL;
        crawl_state.arena_suspended = false;
        return;
    }

    // Arena mode can change behavior of the rest of the code and/or lead
    // to asserts.
    crawl_state.arena_suspended = false;

    fprintf(file, "Player:\n");
    fprintf(file, "{{{{{{{{{{{\n");

    fprintf(file, "Name:       [%s]\n", you.your_name.c_str());
    fprintf(file, "Species:    %s\n", species_name(you.species).c_str());
    fprintf(file, "Job:        %s\n\n", get_job_name(you.char_class));
    fprintf(file, "class_name: %s\n\n", you.class_name.c_str());

    fprintf(file, "HP: %d/%d; mods: %d/%d\n", you.hp, you.hp_max,
            you.hp_max_temp, you.hp_max_perm);
    fprintf(file, "MP: %d/%d; mods: %d/%d\n",
            you.magic_points, you.max_magic_points,
            you.hp_max_temp, you.mp_max_perm);
    fprintf(file, "Stats: %d (%d) %d (%d) %d (%d)\n",
            you.strength(false), you.max_strength(),
            you.intel(false), you.max_intel(),
            you.dex(false), you.max_dex());
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
        for (unsigned int i = 0; i < you.delay_queue.size(); ++i)
        {
            const delay_queue_item &item = you.delay_queue[i];

            fprintf(file, "    type:     %d", item.type);
            if (item.type <= DELAY_NOT_DELAYED
                || item.type >= NUM_DELAYS)
            {
                fprintf(file, " <invalid>");
            }
            fprintf(file, "\n");
            fprintf(file, "    duration: %d\n", item.duration);
            fprintf(file, "    parm1:    %d\n", item.parm1);
            fprintf(file, "    parm2:    %d\n", item.parm2);
            fprintf(file, "    started:  %d\n\n", (int) item.started);
        }
        fprintf(file, "\n");
    }

    fprintf(file, "Skills (mode: %s)\n", you.auto_training ? "auto" : "manual");
    fprintf(file, "Name            | can_train | train | training | level | points | progress\n");
    for (size_t i = 0; i < NUM_SKILLS; ++i)
    {
        const skill_type sk = skill_type(i);
        int needed_min = 0, needed_max = 0;
        if (sk >= 0 && you.skills[sk] <= 27)
            needed_min = skill_exp_needed(you.skills[sk], sk);
        if (sk >= 0 && you.skills[sk] < 27)
            needed_max = skill_exp_needed(you.skills[sk] + 1, sk);

        fprintf(file, "%-16s|     %c     |   %d   |   %3d    |   %2d  | %6d | %d/%d\n",
                skill_name(sk),
                you.can_train[sk] ? 'X' : ' ',
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

        const unsigned int flags = get_spell_flags(spell);

        if (flags & SPFLAG_MONSTER)
            fprintf(file, "    spell slot #%d: monster only spell %s\n",
                    (int)i, spell_title(spell));
        else if (flags & SPFLAG_TESTING)
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
        int innate = you.innate_mutations[i];
        int temp   = you.temp_mutations[i];

        // Normally innate and temp imply normal, but a crash handler should
        // expect the spanish^Wunexpected.
        if (!normal && !innate && !temp)
            continue;

        if (const char* name = mutation_name(mut))
            fprintf(file, "    %s: %d", name, normal);
        else
            fprintf(file, "    unknown #%d: %d", i, normal);

        if (innate)
            if (innate == normal)
                fprintf(file, " (innate)");
            else
                fprintf(file, " (%d innate)", innate);

        if (temp)
            if (temp == normal)
                fprintf(file, " (temporary)");
            else
                fprintf(file, " (%d temporary)", temp);

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
    for (int i = 0; i < NUM_EQUIP; ++i)
    {
        int8_t eq = you.equip[i];

        if (eq == -1)
            continue;

        fprintf(file, "    eq slot #%d, inv slot #%d", i, (int) eq);
        if (eq < 0 || eq >= ENDOFPACK)
        {
            fprintf(file, " <invalid>\n");
            continue;
        }
        const bool unknown = !item_type_known(you.inv[eq]);
        const bool melded  = you.melded[i];
        string suffix = "";
        if (unknown || melded)
        {
            suffix = " (";
            if (unknown)
            {
                suffix += "unknown";
                if (melded)
                    suffix += ", ";
            }
            if (melded)
                suffix += "melded";
            suffix += ")";
        }
        fprintf(file, ": %s%s\n",
                you.inv[eq].name(DESC_PLAIN, false, true).c_str(), suffix.c_str());
    }
    fprintf(file, "\n");

    if (in_bounds(you.pos()) && monster_at(you.pos()))
    {
        fprintf(file, "Standing on same square as: ");
        const unsigned short midx = mgrd(you.pos());

        if (invalid_monster_index(midx))
            fprintf(file, "invalid monster index %d\n", (int) midx);
        else
        {
            const monster* mon = &menv[midx];
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

        if (marker == NULL)
        {
            mprf(MSGCH_ERROR, "Marker #%d is NULL", i);
            continue;
        }

        map_marker_type type = marker->get_type();

        if (type < MAT_FEATURE || type >= NUM_MAP_MARKER_TYPES)
        {
            mprf(MSGCH_ERROR, "Makrer #%d at (%d, %d) has invalid type %d",
                 i, marker->pos.x, marker->pos.y, (int) type);
        }

        if (!in_bounds(marker->pos))
        {
            mprf(MSGCH_ERROR, "Marker #%d, type %d at (%d, %d) out of bounds",
                 i, (int) type, marker->pos.x, marker->pos.y);
            continue;
        }

        bool found = false;
        vector<map_marker*> at_pos = env.markers.get_markers_at(marker->pos);

        for (unsigned int j = 0; j < at_pos.size(); ++j)
        {
            map_marker* tmp = at_pos[j];

            if (tmp == NULL)
                continue;

            if (tmp == marker)
            {
                found = true;
                break;
            }
        }
        if (!found)
            mprf(MSGCH_ERROR, "Marker #%d, type %d at (%d, %d) unlinked",
                 i, (int) type, marker->pos.x, marker->pos.y);
    }

    const coord_def start(MAPGEN_BORDER, MAPGEN_BORDER);
    const coord_def   end(GXM - MAPGEN_BORDER - 2, GYM - MAPGEN_BORDER - 2);
    for (rectangle_iterator ri(start, end); ri; ++ri)
    {
        vector<map_marker*> at_pos = env.markers.get_markers_at(*ri);

        for (unsigned int i = 0; i < at_pos.size(); ++i)
        {
            map_marker *marker = at_pos[i];

            if (marker == NULL)
            {
                mprf(MSGCH_ERROR, "Marker #%d at (%d, %d) NULL",
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
                {
                    mpr("Further, it thinks it's out of bounds.",
                        MSGCH_ERROR);
                }
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

        if (marker == NULL || marker->get_type() == MAT_LUA_MARKER)
            continue;

        mprf(MSGCH_DIAGNOSTICS, "Marker %d at (%d, %d): %s",
             i, marker->pos.x, marker->pos.y,
             marker->debug_describe().c_str());
    }
}

static void _debug_dump_lua_markers(FILE *file)
{
    vector<map_marker*> markers = env.markers.get_all();

    for (unsigned int i = 0; i < markers.size(); ++i)
    {
        map_marker* marker = markers[i];

        if (marker == NULL || marker->get_type() != MAT_LUA_MARKER)
            continue;

        map_lua_marker* lua_marker = dynamic_cast<map_lua_marker*>(marker);

        string result = lua_marker->debug_to_string();

        if (!result.empty() && result[result.size() - 1] == '\n')
            result = result.substr(0, result.size() - 1);

        fprintf(file, "Lua marker %d at (%d, %d):\n",
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
}

static void _dump_command_line(FILE *file)
{
    fprintf(file, "Command line:");
    for (int i = 0, size = crawl_state.command_line_arguments.size();
         i < size; ++i)
    {
        fprintf(file, " %s", crawl_state.command_line_arguments[i].c_str());
    }
    if (crawl_state.command_line_arguments.empty())
        fprintf(file, " (unknown)");
    fprintf(file, "\n\n");
}

// Dump any game options that could affect stability.
static void _dump_options(FILE *file)
{
    fprintf(file, "RC options:\n");
    fprintf(file, "restart_after_game = %s\n",
            Options.restart_after_game? "true" : "false");
    fprintf(file, "\n\n");
}

// Defined in stuff.cc.  Not a part of crawl_state, since that's a
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
        // tracking it down anyway.  Thus, just do the bare bones
        // info to stderr and quit.
        fprintf(stderr, "Crashed while calling exit()!!!!\n");

        _dump_ver_stuff(stderr);

        dump_crash_info(stderr);
        write_stack_trace(stderr, 0);

        return;
    }

    string dir = (!Options.morgue_dir.empty() ? Options.morgue_dir :
                  !SysEnv.crawl_dir.empty()   ? SysEnv.crawl_dir
                                              : "");

    if (!dir.empty() && dir[dir.length() - 1] != FILE_SEPARATOR)
        dir += FILE_SEPARATOR;

    char name[180];

    // Want same time for file name and crash milestone.
    const time_t t = time(NULL);
    snprintf(name, sizeof(name), "%scrash-%s-%s.txt", dir.c_str(),
            you.your_name.c_str(), make_file_time(t).c_str());

    if (!crawl_state.test && !_assert_msg.empty())
        fprintf(stderr, "\n%s", _assert_msg.c_str());
    fprintf(stderr, "\nWriting crash info to %s\n", name);
    errno = 0;
    FILE* file = crawl_state.test ? stderr : freopen(name, "w+", stderr);

    // The errno values are only relevant when the function in
    // question has returned a value indicating (possible) failure, so
    // only freak out if freopen() returned NULL!
    if (!file)
    {
        fprintf(stdout, "\nUnable to open file '%s' for writing: %s\n",
                name, strerror(errno));
        file = stdout;
    }

    // Unbuffer the file, since if we recursively crash buffered lines
    // won't make it to the file.
    setvbuf(file, NULL, _IONBF, 0);

    set_msg_dump_file(file);

    if (!_assert_msg.empty())
        fprintf(file, "%s\n\n", _assert_msg.c_str());

    _dump_ver_stuff(file);

    _dump_command_line(file);

    _dump_options(file);

    // First get the immediate cause of the crash and the stack trace,
    // since that's most important and later attempts to get more information
    // might themselves cause crashes.
    dump_crash_info(file);
    write_stack_trace(file, 0);

    fprintf(file, "\n");

    // Next information on how the binary was compiled
    _dump_compilation_info(file);

    // Next information about the level the player is on, plus level
    // generation info if the crash happened during level generation.
    _dump_level_info(file);

    // Dumping information on marker inconsistancy is unlikely to crash,
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
    crawl_state.dump();
    _dump_player(file);

    if (crawl_state.game_is_zotdef())
        fprintf(file, "ZotDef wave data: %s\n", zotdef_debug_wave_desc().c_str());

    // Next item and monster scans.  Any messages will be sent straight to
    // the file because of set_msg_dump_file()
#ifdef DEBUG_ITEM_SCAN
    debug_item_scan();
#endif
#ifdef DEBUG_MONS_SCAN
    debug_mons_scan();
#endif

    // Dump Webtiles message buffer.
#ifdef USE_TILE_WEB
    tiles.dump();
#endif

    // Now a screenshot
    if (Generating_Level)
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

    set_msg_dump_file(NULL);

    mark_milestone("crash", _assert_msg, "", t);

    if (file != stderr)
        fclose(file);
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

// Assertions and such

//---------------------------------------------------------------
// BreakStrToDebugger
//---------------------------------------------------------------
NORETURN static void _BreakStrToDebugger(const char *mesg, bool assert)
{
#if defined(USE_TILE_LOCAL) && defined(TARGET_OS_WINDOWS)
    SDL_SysWMinfo SysInfo;
    SDL_VERSION(&SysInfo.version);
    if (SDL_GetWMInfo(&SysInfo) > 0)
    {
        MessageBoxW(SysInfo.window, OUTW(mesg),
                   assert ? L"Assertion failed!" : L"Error",
                   MB_OK|MB_ICONERROR);
    }
    // Print the message to STDERR in addition to the above message box,
    // so it's in the message history if we call Crawl from a shell.
#endif
    fprintf(stderr, "%s\n", mesg);

#if defined(TARGET_OS_WINDOWS)
    OutputDebugString(mesg);
    if (IsDebuggerPresent())
        DebugBreak();
#endif

#if defined(TARGET_OS_MACOSX)
// raise(SIGINT);               // this is what DebugStr() does on OS X according to Tech Note 2030
    int* p = NULL;              // but this gives us a stack crawl...
    *p = 0;
#endif

    // MSVCRT's abort() give's a funny message ...
    raise(SIGABRT);
    abort();
}

#ifdef ASSERTS
//---------------------------------------------------------------
//
// AssertFailed
//
//---------------------------------------------------------------
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
        char final_mesg[1024];
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
    char tmp[2048], mesg[2048];

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
    char tmp[2048], mesg[2048];

    va_list args;

    va_start(args, format);
    vsnprintf(tmp, sizeof(tmp), format, args);
    va_end(args);

    snprintf(mesg, sizeof(mesg), "ERROR: %s", tmp);

    _assert_msg = mesg;

    _BreakStrToDebugger(mesg, false);
}
