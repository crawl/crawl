/*
 *  File:       dbg-asrt.cc
 *  Summary:    Assertions and crashing.
 *  Written by: Linley Henzell and Jesse Jones
 */

#include "AppHdr.h"

#include "debug.h"

#include <errno.h>

#include "clua.h"
#include "coord.h"
#include "coordit.h"
#include "crash.h"
#include "dbg-crsh.h"
#include "dbg-scan.h"
#include "dbg-util.h"
#include "directn.h"
#include "dlua.h"
#include "dungeon.h"
#include "env.h"
#include "initfile.h"
#include "jobs.h"
#include "libutil.h"
#include "mapmark.h"
#include "message.h"
#include "monster.h"
#include "mon-util.h"
#include "options.h"
#include "religion.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "state.h"
#include "travel.h"

#ifdef DEBUG
static std::string _assert_msg;
#endif

static void _dump_compilation_info(FILE* file)
{
    std::string comp_info = compilation_info();
    if (!comp_info.empty())
    {
        fprintf(file, "Compilation info:" EOL);
        fprintf(file, "<<<<<<<<<<<" EOL);
        fprintf(file, "%s", comp_info.c_str());
        fprintf(file, ">>>>>>>>>>>" EOL EOL);
    }
}

static void _dump_level_info(FILE* file)
{
    CrawlHashTable &props = env.properties;

    fprintf(file, "Place info:" EOL);

    fprintf(file, "your_level = %d, branch = %d, level_type = %d, "
                  "type_name = %s" EOL EOL,
            you.your_level, (int) you.where_are_you, (int) you.level_type,
            you.level_type_name.c_str());

    std::string place = level_id::current().describe();
    std::string orig_place;

    if (!props.exists(LEVEL_ID_KEY))
        orig_place = "ABSENT";
    else
        orig_place = props[LEVEL_ID_KEY].get_string();

    fprintf(file, "Level id: %s" EOL, place.c_str());
    if (place != orig_place)
        fprintf(file, "Level id when level was generated: %s" EOL,
                orig_place.c_str());

    debug_dump_levgen();
}

static void _dump_player(FILE *file)
{
    // Only dump player info during arena mode if the player is likely
    // the cause of the crash.
    if ((crawl_state.arena || crawl_state.arena_suspended)
        && !in_bounds(you.pos()) && you.hp > 0 && you.hp_max > 0
        && you.strength > 0 && you.intel > 0 && you.dex > 0)
    {
        // Arena mode can change behavior of the rest of the code and/or lead
        // to asserts.
        crawl_state.arena          = false;
        crawl_state.arena_suspended = false;
        return;
    }

    // Arena mode can change behavior of the rest of the code and/or lead
    // to asserts.
    crawl_state.arena          = false;
    crawl_state.arena_suspended = false;

    fprintf(file, "Player:" EOL);
    fprintf(file, "{{{{{{{{{{{" EOL);

    bool name_overrun = true;
    for (int i = 0; i < 30; ++i)
    {
        if (you.class_name[i] == '\0')
        {
            name_overrun = false;
            break;
        }
    }

    if (name_overrun)
    {
        fprintf(file, "job_name runs past end of buffer." EOL);
        you.class_name[29] = '\0';
    }

    fprintf(file, "Name:       [%s]" EOL, you.your_name.c_str());
    fprintf(file, "Species:    %s" EOL, species_name(you.species, 27).c_str());
    fprintf(file, "Job:        %s" EOL EOL, get_job_name(you.char_class));
    fprintf(file, "class_name: %s" EOL EOL, you.class_name);

    fprintf(file, "HP: %d/%d; base: %d/%d" EOL, you.hp, you.hp_max,
            you.base_hp, you.base_hp2);
    fprintf(file, "MP: %d/%d; base: %d/%d" EOL,
            you.magic_points, you.max_magic_points,
            you.base_magic_points, you.base_magic_points2);
    fprintf(file, "Stats: %d (%d) %d (%d) %d (%d)" EOL,
            you.strength, you.max_strength, you.intel, you.max_intel,
            you.dex, you.max_dex);
    fprintf(file, "Position: %s, god:%s (%d), turn_is_over: %d, "
                  "banished: %d" EOL,
            debug_coord_str(you.pos()).c_str(),
            god_name(you.religion).c_str(), (int) you.religion,
            (int) you.turn_is_over, (int) you.banished);

    if (in_bounds(you.pos()))
    {
        const dungeon_feature_type feat = grd(you.pos());
        fprintf(file, "Standing on/in/over feature: %s" EOL,
                raw_feature_description(feat, NUM_TRAPS, true).c_str());
    }
    fprintf(file, EOL);

    if (you.running.runmode != RMODE_NOT_RUNNING)
    {
        fprintf(file, "Runrest:" EOL);
        fprintf(file, "    mode: %d" EOL, you.running.runmode);
        fprintf(file, "      mp: %d" EOL, you.running.mp);
        fprintf(file, "      hp: %d" EOL, you.running.hp);
        fprintf(file, "     pos: %s" EOL EOL,
                debug_coord_str(you.running.pos).c_str());
    }

    if (you.delay_queue.size() > 0)
    {
        fprintf(file, "Delayed (%lu):" EOL,
                (unsigned long) you.delay_queue.size());
        for (unsigned int i = 0; i < you.delay_queue.size(); ++i)
        {
            const delay_queue_item &item = you.delay_queue[i];

            fprintf(file, "    type:     %d", item.type);
            if (item.type <= DELAY_NOT_DELAYED
                || item.type >= NUM_DELAYS)
            {
                fprintf(file, " <invalid>");
            }
            fprintf(file, EOL);
            fprintf(file, "    duration: %d" EOL, item.duration);
            fprintf(file, "    parm1:    %d" EOL, item.parm1);
            fprintf(file, "    parm2:    %d" EOL, item.parm2);
            fprintf(file, "    started:  %d" EOL EOL, (int) item.started);
        }
        fprintf(file, EOL);
    }

    fprintf(file, "Spell bugs:" EOL);
    for (size_t i = 0; i < you.spells.size(); ++i)
    {
        const spell_type spell = you.spells[i];

        if (spell == SPELL_NO_SPELL)
            continue;

        if (!is_valid_spell(spell))
        {
            fprintf(file, "    spell slot #%d: invalid spell #%d" EOL,
                    (int)i, (int)spell);
            continue;
        }

        const unsigned int flags = get_spell_flags(spell);

        if (flags & SPFLAG_MONSTER)
            fprintf(file, "    spell slot #%d: monster only spell %s" EOL,
                    (int)i, spell_title(spell));
        else if (flags & SPFLAG_TESTING)
            fprintf(file, "    spell slot #%d: testing spell %s" EOL,
                    (int)i, spell_title(spell));
        else if (count_bits(get_spell_disciplines(spell)) == 0)
            fprintf(file, "    spell slot #%d: school-less spell %s" EOL,
                    (int)i, spell_title(spell));
    }
    fprintf(file, EOL);

    fprintf(file, "Durations:" EOL);
    for (int i = 0; i < NUM_DURATIONS; ++i)
        if (you.duration[i] != 0)
            fprintf(file, "    #%d: %d" EOL, i, you.duration[i]);

    fprintf(file, EOL);

    fprintf(file, "Attributes:" EOL);
    for (int i = 0; i < NUM_ATTRIBUTES; ++i)
        if (you.attribute[i] != 0)
            fprintf(file, "    #%d: %lu" EOL, i, you.attribute[i]);

    fprintf(file, EOL);

    fprintf(file, "Mutations:" EOL);
    for (int i = 0; i < NUM_MUTATIONS; ++i)
        if (you.mutation[i] > 0)
            fprintf(file, "    #%d: %d" EOL, i, you.mutation[i]);

    fprintf(file, EOL);

    fprintf(file, "Demon mutations:" EOL);
    for (int i = 0; i < NUM_MUTATIONS; ++i)
        if (you.demon_pow[i] > 0)
            fprintf(file, "    #%d: %d" EOL, i, you.demon_pow[i]);

    fprintf(file, EOL);

    fprintf(file, "Inventory bugs:" EOL);
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        item_def &item(you.inv[i]);

        if (item.base_type == OBJ_UNASSIGNED && item.quantity != 0)
        {
            fprintf(file, "    slot #%d: unassigned item has quant %d" EOL,
                    i, item.quantity);
            continue;
        }
        else if (item.base_type != OBJ_UNASSIGNED && item.quantity < 1)
        {
            const int orig_quant = item.quantity;
            item.quantity = 1;

            fprintf(file, "    slot #%d: otherwise valid item '%s' has "
                          "invalid quantity %d" EOL,
                    i, item.name(DESC_PLAIN, false, true).c_str(),
                    orig_quant);
            item.quantity = orig_quant;
            continue;
        }
        else if (!item.is_valid())
            continue;

        const std::string name = item.name(DESC_PLAIN, false, true);

        if (item.link != i)
        {
            fprintf(file, "    slot #%d: item '%s' has invalid link %d" EOL,
                    i, name.c_str(), item.link);
        }

        if (item.slot < 0 || item.slot > 127)
        {
            fprintf(file, "    slot #%d: item '%s' has invalid slot %d" EOL,
                    i, name.c_str(), item.slot);
        }

        if (!item.pos.equals(-1, -1))
        {
            fprintf(file, "    slot #%d: item '%s' has invalid pos %s" EOL,
                    i, name.c_str(), debug_coord_str(item.pos).c_str());
        }
    }
    fprintf(file, EOL);

    fprintf(file, "Equipment:" EOL);
    for (int i = 0; i < NUM_EQUIP; ++i)
    {
        char eq = you.equip[i];

        if (eq == -1)
            continue;

        fprintf(file, "    eq slot #%d, inv slot #%d", i, (int) eq);
        if (eq < 0 || eq >= ENDOFPACK)
        {
            fprintf(file, " <invalid>" EOL);
            continue;
        }
        fprintf(file, ": %s" EOL,
                you.inv[eq].name(DESC_PLAIN, false, true).c_str());
    }
    fprintf(file, EOL);

    if (in_bounds(you.pos()) && monster_at(you.pos()))
    {
        fprintf(file, "Standing on same square as: ");
        const unsigned short midx = mgrd(you.pos());

        if (invalid_monster_index(midx))
            fprintf(file, "invalid monster index %d" EOL, (int) midx);
        else
        {
            const monsters *mon = &menv[midx];
            fprintf(file, "%s:" EOL, debug_mon_str(mon).c_str());
            debug_dump_mon(mon, true);
        }
        fprintf(file, EOL);
    }

    fprintf(file, "}}}}}}}}}}}" EOL EOL);
}

static void _debug_marker_scan()
{
    std::vector<map_marker*> markers = env.markers.get_all();

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
        std::vector<map_marker*> at_pos
            = env.markers.get_markers_at(marker->pos);

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
        std::vector<map_marker*> at_pos = env.markers.get_markers_at(*ri);

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
    std::vector<map_marker*> markers = env.markers.get_all();

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
    std::vector<map_marker*> markers = env.markers.get_all();

    for (unsigned int i = 0; i < markers.size(); ++i)
    {
        map_marker* marker = markers[i];

        if (marker == NULL || marker->get_type() != MAT_LUA_MARKER)
            continue;

        map_lua_marker* lua_marker = dynamic_cast<map_lua_marker*>(marker);

        std::string result = lua_marker->debug_to_string();

        if (result.size() > 0 && result[result.size() - 1] == '\n')
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

    std::string result;
    if (!dlua.callfn("persist_to_string", 0, 1))
        result = make_stringf("error (persist_to_string): %s",
                              dlua.error.c_str());
    else if (lua_isstring(dlua, -1))
        result = lua_tostring(dlua, -1);
    else
        result = "persist_to_string() returned nothing";

    fprintf(file, "%s", result.c_str());
}

static void _dump_ver_stuff(FILE* file)
{
    fprintf(file, "Version: %s %s" EOL, CRAWL, Version::Long().c_str());
#if defined(UNIX)
    fprintf(file, "Platform: unix");
#   if defined(TARGET_OS_MACOSX)
    fprintf(file, " (OS X)");
#   endif
    fprintf(file, EOL);
#elif defined(TARGET_OS_WINDOWS)
    fprintf(file, "Platform: Windows" EOL);
#elif defined(TARGET_OS_DOS)
    fprintf(file, "Platform: DOS" EOL);
#endif // UNIX

#if TARGET_CPU_BITS == 64
    fprintf(file, "Bits: 64" EOL);
#else
    fprintf(file, "Bits: 32" EOL);
#endif

#ifdef USE_TILE
    fprintf(file, "Tiles: yes" EOL EOL);
#else
    fprintf(file, "Tiles: no" EOL EOL);
#endif
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
        // tracking it down anyways.  Thus, just do the bare bones
        // info to stderr and quit.
        fprintf(stderr, "Crashed while calling exit()!!!!" EOL);

        _dump_ver_stuff(stderr);

        dump_crash_info(stderr);
        write_stack_trace(stderr, 0);

        return;
    }

    std::string dir = (!Options.morgue_dir.empty() ? Options.morgue_dir :
                       !SysEnv.crawl_dir.empty()   ? SysEnv.crawl_dir
                                                   : "");

    if (!dir.empty() && dir[dir.length() - 1] != FILE_SEPARATOR)
        dir += FILE_SEPARATOR;

    char name[180];

    sprintf(name, "%scrash-%s-%s.txt", dir.c_str(),
            you.your_name.c_str(), make_file_time(time(NULL)).c_str());

    fprintf(stderr, EOL "Writing crash info to %s" EOL, name);
    errno = 0;
    FILE* file = crawl_state.test ? stderr : freopen(name, "w+", stderr);

    if (file == NULL || errno != 0)
    {
        fprintf(stdout, EOL "Unable to open file '%s' for writing: %s" EOL,
                name, strerror(errno));
        file = stdout;
    }

    // Unbuffer the file, since if we recursively crash buffered lines
    // won't make it to the file.
    setvbuf(file, NULL, _IONBF, 0);

    set_msg_dump_file(file);

#ifdef DEBUG
    if (!_assert_msg.empty())
        fprintf(file, "%s" EOL EOL, _assert_msg.c_str());
#endif

    _dump_ver_stuff(file);

    // First get the immediate cause of the crash and the stack trace,
    // since that's most important and later attempts to get more information
    // might themselves cause crashes.
    dump_crash_info(file);
    write_stack_trace(file, 0);

    fprintf(file, EOL);

    // Next information on how the binary was compiled
    _dump_compilation_info(file);

    // Next information about the level the player is on, plus level
    // generation info if the crash happened during level generation.
    _dump_level_info(file);

    // Dumping information on marker inconsistancy is unlikely to crash,
    // as is dumping the descriptions of non-Lua markers.
    fprintf(file, "Markers:" EOL);
    fprintf(file, "<<<<<<<<<<<<<<<<<<<<<<" EOL);
    _debug_marker_scan();
    _debug_dump_markers();
    fprintf(file, ">>>>>>>>>>>>>>>>>>>>>>" EOL);

    // Dumping current messages is unlikely to crash.
    if (file != stdout)
    {
        fprintf(file, EOL "Messages:" EOL);
        fprintf(file, "<<<<<<<<<<<<<<<<<<<<<<" EOL);
        std::string messages = get_last_messages(NUM_STORED_MESSAGES);
        fprintf(file, "%s", messages.c_str());
        fprintf(file, ">>>>>>>>>>>>>>>>>>>>>>" EOL);
    }

    // Dumping the player state and crawl state is next least likely to cause
    // another crash, so do that next.
    crawl_state.dump();
    _dump_player(file);

    // Next item and monster scans.  Any messages will be sent straight to
    // the file because of set_msg_dump_file()
#if DEBUG_ITEM_SCAN
    debug_item_scan();
#endif
#if DEBUG_MONS_SCAN
    debug_mons_scan();
#endif

    // If anything has screwed up the Lua runtime stacks then trying to
    // print those stacks will likely crash, so do this after the others.
    fprintf(file, "clua stack:" EOL);
    clua.print_stack();

    fprintf(file, "dlua stack:" EOL);
    dlua.print_stack();

    // Lastly try to dump the Lua persistent data and the contents of the Lua
    // markers, since actually running Lua code has the greatest chance of
    // crashing.
    fprintf(file, "Lua persistent data:" EOL);
    fprintf(file, "<<<<<<<<<<<<<<<<<<<<<<" EOL);
    _debug_dump_lua_persist(file);
    fprintf(file, ">>>>>>>>>>>>>>>>>>>>>>" EOL EOL);
    fprintf(file, "Lua marker contents:" EOL);
    fprintf(file, "<<<<<<<<<<<<<<<<<<<<<<" EOL);
    _debug_dump_lua_markers(file);
    fprintf(file, ">>>>>>>>>>>>>>>>>>>>>>" EOL);

    set_msg_dump_file(NULL);

    if (file != stderr)
        fclose(file);
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

// Assertions and such

#ifdef DEBUG
//---------------------------------------------------------------
// BreakStrToDebugger
//---------------------------------------------------------------
static void _BreakStrToDebugger(const char *mesg)
{
#if defined(TARGET_OS_MACOSX) || defined(TARGET_COMPILER_MINGW)
    fprintf(stderr, "%s", mesg);
// raise(SIGINT);               // this is what DebugStr() does on OS X according to Tech Note 2030
    int* p = NULL;              // but this gives us a stack crawl...
    *p = 0;

#else
    fprintf(stderr, "%s\n", mesg);
    abort();
#endif
}

//---------------------------------------------------------------
//
// AssertFailed
//
//---------------------------------------------------------------
void AssertFailed(const char *expr, const char *file, int line)
{
    char mesg[512];

    const char *fileName = file + strlen(file); // strip off path

    while (fileName > file && fileName[-1] != '\\')
        --fileName;

    sprintf(mesg, "ASSERT(%s) in '%s' at line %d failed.", expr, fileName,
            line);

    _assert_msg = mesg;

    _BreakStrToDebugger(mesg);
}

//---------------------------------------------------------------
//
// DEBUGSTR
//
//---------------------------------------------------------------
void DEBUGSTR(const char *format, ...)
{
    char mesg[2048];

    va_list args;

    va_start(args, format);
    vsprintf(mesg, format, args);
    va_end(args);

    _BreakStrToDebugger(mesg);
}

#endif
