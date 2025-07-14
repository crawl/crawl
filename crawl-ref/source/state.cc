/**
 * @file
 * @brief Game state functions.
**/

#include "AppHdr.h"

#include "state.h"

#if defined(UNIX) || defined(TARGET_COMPILER_MINGW)
#include <unistd.h>
#endif

#include "dbg-util.h"
#include "delay.h"
#include "directn.h"
#include "hints.h"
#include "initfile.h"
#include "macro.h"
#include "menu.h"
#include "message.h"
#include "monster.h"
#include "player.h"
#include "religion.h"
#include "showsymb.h"
#include "unwind.h"

game_state::game_state()
    : game_crashed(false), crash_debug_scans_safe(true),
      mouse_enabled(false), waiting_for_command(false),
      waiting_for_ui(false),
      terminal_resized(false), last_winch(0),
      seed(0),
      io_inited(false),
      need_save(false), save_after_turn(false),
      game_started(false), saving_game(false),
      updating_scores(false),
      parsing_rc(false),
#ifndef USE_TILE_LOCAL
      smallterm(false),
#endif
      seen_hups(0), map_stat_gen(false), map_stat_dump_disconnect(false),
      obj_stat_gen(false), type(GAME_TYPE_NORMAL),
      last_type(GAME_TYPE_UNSPECIFIED), last_game_exit(game_exit::unknown),
      marked_as_won(false), arena_suspended(false),
      generating_level(false), dump_maps(false), test(false), script(false),
      build_db(false), use_des_cache(true), tests_selected(),
#ifdef DGAMELAUNCH
      throttle(true),
      bypassed_startup_menu(true),
#else
      throttle(false),
      bypassed_startup_menu(false),
#endif
      clua_max_memory_mb(16), show_more_prompt(true),
      skip_autofight_check(false), terminal_resize_handler(nullptr),
      terminal_resize_check(nullptr), doing_prev_cmd_again(false),
      prev_cmd(CMD_NO_CMD), repeat_cmd(CMD_NO_CMD),
      cmd_repeat_started_unsafe(false),
      lua_calls_no_turn(0), lua_script_killed(false),
      lua_ready_throttled(false),
      stat_gain_prompt(false), simulating_xp_gain(false),
      level_annotation_shown(false),
      viewport_monster_hp(false), viewport_weapons(false),
      tiles_disabled(false),
      title_screen(true),
      invisible_targeting(false),
      darken_range(nullptr), unsaved_macros(false), disables(),
      minor_version(-1), save_rcs_version(),
      nonempty_buffer_flush_errors(false),
      last_builder_error_fatal(false),
      mon_act(nullptr)
{
    reset_cmd_repeat();
    reset_cmd_again();
#ifndef UNIX
    no_gdb = "Non-UNIX Platform -> not running gdb.";
#else
    no_gdb = access(GDB_PATH, 1) ? "gdb not executable." : 0;
#endif
}

/**
 * Cleanup for when the game is reset.
 *
 * @see main.cc:_reset_game()
 */
void game_state::reset_game()
{
    game_started = false;
    // need_save is unset by death, but not by saving with restart_after_save.
    need_save = false;
    type = GAME_TYPE_UNSPECIFIED;
    updating_scores = false;
    clear_mon_acting();
    reset_cmd_repeat();
    reset_cmd_again();
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

void game_state::cancel_cmd_repeat(string reason, bool force)
{
    if (!force && !is_repeating_cmd())
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

void game_state::cancel_cmd_again(string reason, bool force)
{
    if (!force && !doing_prev_cmd_again)
        return;

    if (is_replaying_keys() || cmd_repeat_start)
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
// turns. A wrapper around cancel_cmd_repeat(), its only purpose is
// to make it clear why cancel_cmd_repeat() is being called.
void game_state::zero_turns_taken()
{
    ASSERT(!you.turn_is_over);
    cancel_cmd_repeat();
}

bool interrupt_cmd_repeat(activity_interrupt ai,
                           const activity_interrupt_data &at)
{
    if (crawl_state.cmd_repeat_start)
        return false;

    if (crawl_state.repeat_cmd == CMD_WIZARD)
        return false;

    switch (ai)
    {
    case activity_interrupt::teleport:
    case activity_interrupt::force:
    case activity_interrupt::hp_loss:
    case activity_interrupt::monster_attacks:
    case activity_interrupt::mimic:
        crawl_state.cancel_cmd_repeat("Command repetition interrupted.");
        return true;

    default:
        break;
    }

    if (ai == activity_interrupt::see_monster)
    {
        const monster* mon = at.mons_data;
        ASSERT(mon);
        if (!you.can_see(*mon))
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
    if (crawl_state.repeat_cmd == CMD_WAIT)
    {
        if (ai == activity_interrupt::full_mp)
            crawl_state.cancel_cmd_repeat("Magic restored.");
        else if (ai == activity_interrupt::full_hp)
            crawl_state.cancel_cmd_repeat("HP restored");
        else
            crawl_state.cancel_cmd_repeat("Command repetition interrupted.");

        return true;
    }

    if (crawl_state.cmd_repeat_started_unsafe)
        return false;

    if (ai == activity_interrupt::hit_monster)
    {
        // This check is for when command repetition is used to
        // whack away at a 0xp monster, since the player feels safe
        // when the only monsters around are 0xp.
        const monster* mon = at.mons_data;

        if (!mons_is_threatening(*mon) && mon->visible_to(&you))
            return false;

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
    return mon_act != nullptr;
}

monster* game_state::which_mon_acting() const
{
    return mon_act;
}

void game_state::inc_mon_acting(monster* mon)
{
    ASSERT(!invalid_monster(mon));

    if (mon_act != nullptr)
        mon_act_stack.push_back(mon_act);

    mon_act = mon;
}

void game_state::dec_mon_acting(monster* mon)
{
    ASSERT(mon_act == mon);

    mon_act = nullptr;

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
    mon_act = nullptr;
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

    fprintf(stderr, "prev_cmd = %s\n", command_to_name(prev_cmd).c_str());

    if (doing_prev_cmd_again)
    {
        fprintf(stderr, "Doing prev_cmd again with keys: ");
        for (int key : prev_cmd_keys)
            fprintf(stderr, "%d, ", key);
        fprintf(stderr, "\n");
        fprintf(stderr, "As ASCII keys: ");
        for (int key : prev_cmd_keys)
            fprintf(stderr, "%c", (char) key);
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
        for (const god_act_state &godact : god_act_stack)
        {
            fprintf(stderr, "God %s with depth %d\n",
                    god_name(godact.which_god).c_str(), godact.depth);
        }
        fprintf(stderr, "\n\n");
    }

    if (mon_act != nullptr)
    {
        fprintf(stderr, "%s currently acting:\n\n",
                debug_mon_str(mon_act).c_str());
        debug_dump_mon(mon_act, true);
    }

    if (!mon_act_stack.empty())
    {
        fprintf(stderr, "Others monsters acting:\n");
        for (const monster *mon : mon_act_stack)
            fprintf(stderr, "    %s\n", debug_mon_str(mon).c_str());
    }
}

bool game_state::player_is_dead() const
{
    return updating_scores && !need_save;
}

bool game_state::game_has_random_floors() const
{
    return game_is_normal() || game_is_hints() || game_is_descent();
}

bool game_state::game_saves_prefs() const
{
    return game_is_normal() || game_is_hints() || game_is_descent();
}

bool game_state::game_is_valid_type() const
{
    return type < NUM_GAME_TYPE;
}

bool game_state::game_is_normal() const
{
    ASSERT(game_is_valid_type());
    return type == GAME_TYPE_NORMAL || type == GAME_TYPE_CUSTOM_SEED
                                    || type == GAME_TYPE_UNSPECIFIED;
}

bool game_state::game_is_tutorial() const
{
    ASSERT(game_is_valid_type());
    return type == GAME_TYPE_TUTORIAL;
}

bool game_state::game_is_arena() const
{
    ASSERT(game_is_valid_type());
    return type == GAME_TYPE_ARENA;
}

bool game_state::game_is_sprint() const
{
    ASSERT(game_is_valid_type());
    return type == GAME_TYPE_SPRINT;
}

bool game_state::game_is_hints() const
{
    ASSERT(game_is_valid_type());
    return type == GAME_TYPE_HINTS;
}

bool game_state::game_is_descent() const
{
    ASSERT(game_is_valid_type());
    return type == GAME_TYPE_DESCENT;
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
    case GAME_TYPE_CUSTOM_SEED:
        return "Seeded";
    case GAME_TYPE_TUTORIAL:
        return "Tutorial";
    case GAME_TYPE_ARENA:
        return "Arena";
    case GAME_TYPE_SPRINT:
        return "Dungeon Sprint";
    case GAME_TYPE_DESCENT:
        return "Dungeon Descent";
    case NUM_GAME_TYPE:
        return "Unknown";
    }
}

bool game_state::seed_is_known() const
{
#ifdef DGAMELAUNCH
    return player_is_dead()
# ifdef WIZARD
        || you.wizard
# endif
        || type == GAME_TYPE_CUSTOM_SEED;
#else
    //offline: it's visible to do what you want with it.
    return true;
#endif
}

string game_state::game_savedir_path() const
{
    if (!game_is_valid_type())
        return ""; // a game from the future...
    switch (type)
    {
    case GAME_TYPE_SPRINT:
        return "sprint/";
    case GAME_TYPE_DESCENT:
        return "descent/";
    default:
        return "";
    }
}

string game_state::game_type_qualifier() const
{
    switch (type)
    {
    case GAME_TYPE_CUSTOM_SEED:
    case GAME_TYPE_SPRINT:
    case GAME_TYPE_HINTS:
    case GAME_TYPE_TUTORIAL:
    case GAME_TYPE_DESCENT:
        return "-" + gametype_to_str(type);
    default:
        return "";
    }
}
