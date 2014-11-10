/**
 * @file
 * @brief Game state functions.
**/

#include "AppHdr.h"

#include "state.h"

#ifndef TARGET_OS_WINDOWS
#include <unistd.h>
#endif

#include "dbg-util.h"
#include "delay.h"
#include "directn.h"
#include "exclude.h"
#include "hints.h"
#include "macro.h"
#include "menu.h"
#include "message.h"
#include "misc.h"
#include "monster.h"
#include "player.h"
#include "religion.h"
#include "showsymb.h"
#include "unwind.h"

game_state::game_state()
    : game_crashed(false),
      mouse_enabled(false), waiting_for_command(false),
      terminal_resized(false), last_winch(0), io_inited(false),
      need_save(false),
      saving_game(false), updating_scores(false), seen_hups(0),
      map_stat_gen(false), type(GAME_TYPE_NORMAL),
      last_type(GAME_TYPE_UNSPECIFIED), arena_suspended(false),
      generating_level(false),
      dump_maps(false), test(false), script(false), build_db(false),
      tests_selected(),
#ifdef DGAMELAUNCH
      throttle(true),
#else
      throttle(false),
#endif
      show_more_prompt(true),
      terminal_resize_handler(NULL), terminal_resize_check(NULL),
      doing_prev_cmd_again(false), prev_cmd(CMD_NO_CMD),
      repeat_cmd(CMD_NO_CMD),cmd_repeat_started_unsafe(false),
      lua_calls_no_turn(0), stat_gain_prompt(false),
      level_annotation_shown(false), viewport_monster_hp(false),
      viewport_weapons(false),
#ifndef USE_TILE_LOCAL
      mlist_targeting(false),
#else
      title_screen(true),
#endif
      darken_range(NULL), unsaved_macros(false), mon_act(NULL)
{
    reset_cmd_repeat();
    reset_cmd_again();
#ifdef TARGET_OS_WINDOWS
    no_gdb = "Non-UNIX Platform -> not running gdb.";
#elif defined DEBUG_DIAGNOSTICS
    no_gdb = "Debug build -> run gdb yourself.";
#else
    no_gdb = access("/usr/bin/gdb", 1) ? "GDB not installed." : 0;
#endif
}

void game_state::add_startup_error(const string &err)
{
    startup_errors.push_back(err);
}

void game_state::show_startup_errors()
{
    formatted_scroller error_menu;
    error_menu.set_flags(MF_NOSELECT | MF_ALWAYS_SHOW_MORE | MF_NOWRAP
                         | MF_EASY_EXIT);
    error_menu.set_more(
        formatted_string::parse_string(
                           "<cyan>[ + : Page down.   - : Page up."
                           "                    Esc or Enter to continue.]"));
    error_menu.set_title(
        new MenuEntry("Warning: Crawl encountered errors during startup:",
                      MEL_TITLE));
    for (int i = 0, size = startup_errors.size(); i < size; ++i)
        error_menu.add_entry(new MenuEntry(startup_errors[i]));
    error_menu.show();
}

///////////////////////////////////////////////////////////////////////////
// Repeating commands and doing the previous command over again.

bool game_state::is_replaying_keys() const
{
    return crawl_state.doing_prev_cmd_again
           || crawl_state.is_repeating_cmd();
}

bool game_state::is_repeating_cmd() const
{
    return repeat_cmd != CMD_NO_CMD;
}

void game_state::cancel_cmd_repeat(string reason)
{
    if (!is_repeating_cmd())
        return;

    if (repeat_cmd == CMD_WIZARD)
    {
        // Don't interrupt wizard testing of religion.
        if (is_god_acting())
            return;

        // Don't interrupt wizard testing just because we can't
        // move.
        if (you.cannot_act())
            return;

        // We've probably just recovered from being unable to act;
        // again, don't interrupt.
        if (you.turn_is_over)
            return;
    }

    if (is_replaying_keys() || cmd_repeat_start)
        flush_input_buffer(FLUSH_KEY_REPLAY_CANCEL);

    if (is_processing_macro())
        flush_input_buffer(FLUSH_ABORT_MACRO);

    reset_cmd_repeat();

    if (!reason.empty())
        mpr(reason);
}

void game_state::cancel_cmd_again(string reason)
{
    if (!doing_prev_cmd_again)
        return;

    flush_input_buffer(FLUSH_KEY_REPLAY_CANCEL);

    if (is_processing_macro())
        flush_input_buffer(FLUSH_ABORT_MACRO);

    reset_cmd_again();

    if (!reason.empty())
        mpr(reason);
}

void game_state::cancel_cmd_all(string reason)
{
    cancel_cmd_repeat(reason);
    cancel_cmd_again(reason);
}

void game_state::cant_cmd_repeat(string reason)
{
    if (reason.empty())
        reason = "Can't repeat that command.";

    cancel_cmd_repeat(reason);
}

void game_state::cant_cmd_again(string reason)
{
    if (reason.empty())
        reason = "Can't redo that command.";

    cancel_cmd_again(reason);
}

void game_state::cant_cmd_any(string reason)
{
    cant_cmd_repeat(reason);
    cant_cmd_again(reason);
}

// The method is called to prevent the "no repeating zero turns
// commands" message that input() generates (in the absence of
// cancelling the repetition) for a repeated command that took no
// turns.  A wrapper around cancel_cmd_repeat(), its only purpose is
// to make it clear why cancel_cmd_repeat() is being called.
void game_state::zero_turns_taken()
{
    ASSERT(!you.turn_is_over);
    cancel_cmd_repeat();
}

bool interrupt_cmd_repeat(activity_interrupt_type ai,
                           const activity_interrupt_data &at)
{
    if (crawl_state.cmd_repeat_start)
        return false;

    if (crawl_state.repeat_cmd == CMD_WIZARD)
        return false;

    switch (ai)
    {
    case AI_STATUE:
    case AI_HUNGRY:
    case AI_TELEPORT:
    case AI_FORCE_INTERRUPT:
    case AI_HP_LOSS:
    case AI_MONSTER_ATTACKS:
    case AI_MIMIC:
        crawl_state.cancel_cmd_repeat("Command repetition interrupted.");
        return true;

    default:
        break;
    }

    if (ai == AI_SEE_MONSTER)
    {
        const monster* mon = static_cast<const monster* >(at.data);
        if (!you.can_see(mon))
            return false;

        if (crawl_state.cmd_repeat_started_unsafe
            && at.context != SC_NEWLY_SEEN)
        {
            return false;
        }

        crawl_state.cancel_cmd_repeat();

#ifndef DEBUG_DIAGNOSTICS
        if (at.context == SC_NEWLY_SEEN)
        {
            monster_info mi(mon);
            set_auto_exclude(mon);

            mprf(MSGCH_WARN, "%s comes into view.",
                 get_monster_equipment_desc(mi, DESC_WEAPON).c_str());
        }

        if (crawl_state.game_is_hints())
            hints_monster_seen(*mon);
#else
        formatted_string fs(channel_to_colour(MSGCH_WARN));
        fs.cprintf("%s (", mon->name(DESC_PLAIN, true).c_str());
        monster_info mi(mon);
        fs.add_glyph(get_mons_glyph(mi));
        fs.cprintf(") in view: (%d,%d), see_cell: %s",
                   mon->pos().x, mon->pos().y,
                   you.see_cell(mon->pos())? "yes" : "no");
        formatted_mpr(fs, MSGCH_WARN);
#endif

        return true;
    }

    // If command repetition is being used to imitate the rest command,
    // then everything interrupts it.
    if (crawl_state.repeat_cmd == CMD_MOVE_NOWHERE
        || crawl_state.repeat_cmd == CMD_WAIT)
    {
        if (ai == AI_FULL_MP)
            crawl_state.cancel_cmd_repeat("Magic restored.");
        else if (ai == AI_FULL_HP)
            crawl_state.cancel_cmd_repeat("HP restored");
        else
            crawl_state.cancel_cmd_repeat("Command repetition interrupted.");

        return true;
    }

    if (crawl_state.cmd_repeat_started_unsafe)
        return false;

    if (ai == AI_HIT_MONSTER)
    {
        // This check is for when command repetition is used to
        // whack away at a 0xp monster, since the player feels safe
        // when the only monsters around are 0xp.
        const monster* mon = static_cast<const monster* >(at.data);

        if (mons_class_flag(mon->type, M_NO_EXP_GAIN)
            && mon->visible_to(&you))
        {
            return false;
        }

        crawl_state.cancel_cmd_repeat("Command repetition interrupted.");
        return true;
    }

    return false;
}

void game_state::reset_cmd_repeat()
{
    repeat_cmd           = CMD_NO_CMD;
    cmd_repeat_start     = false;
}

void game_state::reset_cmd_again()
{
    doing_prev_cmd_again = false;
    prev_cmd             = CMD_NO_CMD;
    prev_cmd_repeat_goal = 0;
    prev_repeat_cmd      = CMD_NO_CMD;

    prev_cmd_keys.clear();
}

///////////////////////////////////////////////////////////
// Keeping track of which god is currently doing something
///////////////////////////////////////////////////////////

god_act_state::god_act_state()
{
    reset();
}

void god_act_state::reset()
{
    which_god   = GOD_NO_GOD;
    retribution = false;
    depth       = 0;
}

bool game_state::is_god_acting() const
{
    ASSERT(god_act.depth >= 0);
    bool god_is_acting = god_act.depth > 0;
    ASSERT(!(god_is_acting && god_act.which_god == GOD_NO_GOD));
    ASSERT(god_is_acting || god_act.which_god == GOD_NO_GOD);
    ASSERT(god_is_acting || god_act_stack.empty());

    return god_is_acting;
}

bool game_state::is_god_retribution() const
{
    ASSERT(is_god_acting());

    return god_act.retribution;
}

god_type game_state::which_god_acting() const
{
    return god_act.which_god;
}

void game_state::inc_god_acting(bool is_retribution)
{
    inc_god_acting(you.religion, is_retribution);
}

void game_state::inc_god_acting(god_type which_god, bool is_retribution)
{
    ASSERT(which_god != GOD_NO_GOD);

    if (god_act.which_god != GOD_NO_GOD
        && god_act.which_god != which_god)
    {
        ASSERT(god_act.depth >= 1);

        god_act_stack.push_back(god_act);
        god_act.reset();
    }

    god_act.which_god   = which_god;
    god_act.retribution = is_retribution || god_act.retribution;
    god_act.depth++;
}

void game_state::dec_god_acting()
{
    dec_god_acting(you.religion);
}

void game_state::dec_god_acting(god_type which_god)
{
    ASSERT(which_god != GOD_NO_GOD);
    ASSERT(god_act.depth > 0);
    ASSERT(god_act.which_god == which_god);

    god_act.depth--;

    if (god_act.depth == 0)
    {
        god_act.reset();
        if (!god_act_stack.empty())
        {
            god_act = god_act_stack[god_act_stack.size() - 1];
            god_act_stack.pop_back();
            ASSERT(god_act.depth >= 1);
            ASSERT(god_act.which_god != GOD_NO_GOD);
            ASSERT(god_act.which_god != which_god);
        }
    }
}

void game_state::clear_god_acting()
{
    ASSERT(!is_god_acting());
    ASSERT(god_act_stack.empty());

    god_act.reset();
}

vector<god_act_state> game_state::other_gods_acting() const
{
    ASSERT(is_god_acting());
    return god_act_stack;
}

bool game_state::is_mon_acting() const
{
    return mon_act != NULL;
}

monster* game_state::which_mon_acting() const
{
    return mon_act;
}

void game_state::inc_mon_acting(monster* mon)
{
    ASSERT(!invalid_monster(mon));

    if (mon_act != NULL)
        mon_act_stack.push_back(mon_act);

    mon_act = mon;
}

void game_state::dec_mon_acting(monster* mon)
{
    ASSERT(mon_act == mon);

    mon_act = NULL;

    const unsigned int size = mon_act_stack.size();
    if (size > 0)
    {
        mon_act = mon_act_stack[size - 1];
        ASSERT(!invalid_monster(mon_act));
        mon_act_stack.pop_back();
    }
}

void game_state::clear_mon_acting()
{
    mon_act = NULL;
    mon_act_stack.clear();
}

void game_state::mon_gone(monster* mon)
{
    for (unsigned int i = 0, size = mon_act_stack.size(); i < size; i++)
    {
        if (mon_act_stack[i] == mon)
        {
            mon_act_stack.erase(mon_act_stack.begin() + i);
            i--;
            size--;
        }
    }

    if (mon_act == mon)
        dec_mon_acting(mon);
}

void game_state::dump()
{
    fprintf(stderr, "\nGame state:\n\n");

    fprintf(stderr, "mouse_enabled: %d, waiting_for_command: %d, "
                  "terminal_resized: %d\n",
            mouse_enabled, waiting_for_command, terminal_resized);
    fprintf(stderr, "io_inited: %d, need_save: %d, saving_game: %d, "
                  "updating_scores: %d:\n",
            io_inited, need_save, saving_game, updating_scores);
    fprintf(stderr, "seen_hups: %d, map_stat_gen: %d, type: %d, "
                  "arena_suspended: %d\n",
            seen_hups, map_stat_gen, type, arena_suspended);
    if (last_winch)
    {
        fprintf(stderr, "Last resize was %" PRId64" seconds ago.\n",
                (int64_t)(time(0) - last_winch));
    }

    fprintf(stderr, "\n");

    // Arena mode can change behavior of the rest of the code and/or lead
    // to asserts.
    unwind_var<game_type> _type(type, GAME_TYPE_NORMAL);
    unwind_bool _arena_suspended(arena_suspended, false);

    if (!startup_errors.empty())
    {
        fprintf(stderr, "Startup errors:\n");
        for (unsigned int i = 0; i < startup_errors.size(); i++)
            fprintf(stderr, "%s\n", startup_errors[i].c_str());
        fprintf(stderr, "\n");
    }

    fprintf(stderr, "prev_cmd = %s\n", command_to_name(prev_cmd).c_str());

    if (doing_prev_cmd_again)
    {
        fprintf(stderr, "Doing prev_cmd again with keys: ");
        for (unsigned int i = 0; i < prev_cmd_keys.size(); i++)
            fprintf(stderr, "%d, ", prev_cmd_keys[i]);
        fprintf(stderr, "\n");
        fprintf(stderr, "As ASCII keys: ");
        for (unsigned int i = 0; i < prev_cmd_keys.size(); i++)
            fprintf(stderr, "%c", (char) prev_cmd_keys[i]);
        fprintf(stderr, "\n\n");
    }
    fprintf(stderr, "repeat_cmd = %s\n", command_to_name(repeat_cmd).c_str());

    fprintf(stderr, "\n");

    if (god_act.which_god != GOD_NO_GOD || god_act.depth != 0)
    {
        fprintf(stderr, "God %s currently acting with depth %d\n\n",
                god_name(god_act.which_god).c_str(), god_act.depth);
    }

    if (!god_act_stack.empty())
    {
        fprintf(stderr, "Other gods acting:\n");
        for (unsigned int i = 0; i < god_act_stack.size(); i++)
        {
            fprintf(stderr, "God %s with depth %d\n",
                    god_name(god_act_stack[i].which_god).c_str(),
                    god_act_stack[i].depth);
        }
        fprintf(stderr, "\n\n");
    }

    if (mon_act != NULL)
    {
        fprintf(stderr, "%s currently acting:\n\n",
                debug_mon_str(mon_act).c_str());
        debug_dump_mon(mon_act, true);
    }

    if (!mon_act_stack.empty())
    {
        fprintf(stderr, "Others monsters acting:\n");
        for (unsigned int i = 0; i < mon_act_stack.size(); i++)
        {
            fprintf(stderr, "    %s\n",
                    debug_mon_str(mon_act_stack[i]).c_str());
        }
    }
}

bool game_state::player_is_dead() const
{
    return updating_scores && !need_save;
}

bool game_state::game_standard_levelgen() const
{
    return game_is_normal() || game_is_hints();
}

bool game_state::game_is_normal() const
{
    ASSERT(type < NUM_GAME_TYPE);
    return type == GAME_TYPE_NORMAL || type == GAME_TYPE_UNSPECIFIED;
}

bool game_state::game_is_tutorial() const
{
    ASSERT(type < NUM_GAME_TYPE);
    return type == GAME_TYPE_TUTORIAL;
}

bool game_state::game_is_arena() const
{
    ASSERT(type < NUM_GAME_TYPE);
    return type == GAME_TYPE_ARENA;
}

bool game_state::game_is_sprint() const
{
    ASSERT(type < NUM_GAME_TYPE);
    return type == GAME_TYPE_SPRINT;
}

bool game_state::game_is_zotdef() const
{
    ASSERT(type < NUM_GAME_TYPE);
    return type == GAME_TYPE_ZOTDEF;
}

bool game_state::game_is_hints() const
{
    ASSERT(type < NUM_GAME_TYPE);
    return type == GAME_TYPE_HINTS;
}

bool game_state::game_is_hints_tutorial() const
{
    return game_is_hints() || game_is_tutorial();
}

string game_state::game_type_name() const
{
    return game_type_name_for(type);
}

string game_state::game_type_name_for(game_type _type)
{
    switch (_type)
    {
    case GAME_TYPE_UNSPECIFIED:
    case GAME_TYPE_NORMAL:
    case GAME_TYPE_HINTS:
    default:
        // No explicit game type name for default game.
        return "";
    case GAME_TYPE_TUTORIAL:
        return "Tutorial";
    case GAME_TYPE_ARENA:
        return "Arena";
    case GAME_TYPE_SPRINT:
        return "Dungeon Sprint";
    case GAME_TYPE_ZOTDEF:
        return "Zot Defence";
    }
}

string game_state::game_savedir_path() const
{
    return game_is_sprint()? "sprint/" :
           game_is_zotdef()? "zotdef/" : "";
}

string game_state::game_type_qualifier() const
{
    if (crawl_state.game_is_sprint())
        return "-sprint";
    if (crawl_state.game_is_zotdef())
        return "-zotdef";
    if (crawl_state.game_is_tutorial())
        return "-tutorial";
    if (crawl_state.game_is_hints())
        return "-hints";
    return "";
}
