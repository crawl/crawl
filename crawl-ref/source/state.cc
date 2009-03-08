/*
 *  File:       state.cc
 *  Summary:    Game state functions.
 *  Written by: Matthew Cline
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "externs.h"

#include "delay.h"
#include "directn.h"
#include "macro.h"
#include "menu.h" // For print_formatted_paragraph()
#include "message.h"
#include "mon-util.h"
#include "output.h"
#include "player.h"
#include "religion.h"
#include "state.h"
#include "tutorial.h"
#include "view.h"

game_state::game_state()
    : game_crashed(false), mouse_enabled(false), waiting_for_command(false),
      terminal_resized(false), io_inited(false), need_save(false),
      saving_game(false), updating_scores(false), seen_hups(0),
      map_stat_gen(false), arena(false), arena_suspended(false),
      unicode_ok(false), glyph2strfn(NULL), multibyte_strlen(NULL),
      terminal_resize_handler(NULL), terminal_resize_check(NULL),
      doing_prev_cmd_again(false), prev_cmd(CMD_NO_CMD),
      repeat_cmd(CMD_NO_CMD), cmd_repeat_count(0), cmd_repeat_goal(0),
      prev_repetition_turn(0), cmd_repeat_started_unsafe(false),
      input_line_curr(0), level_annotation_shown(false)
{
    reset_cmd_repeat();
    reset_cmd_again();
}

void game_state::add_startup_error(const std::string &err)
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
    return (crawl_state.doing_prev_cmd_again
            || (crawl_state.is_repeating_cmd()
                && !crawl_state.cmd_repeat_start));
}

bool game_state::is_repeating_cmd() const
{
    ASSERT((cmd_repeat_goal == 0 && cmd_repeat_count == 0
            && repeat_cmd == CMD_NO_CMD && !cmd_repeat_start)
           || (cmd_repeat_goal > 0 && cmd_repeat_count <= cmd_repeat_goal
               && repeat_cmd != CMD_NO_CMD));

    return (repeat_cmd != CMD_NO_CMD);
}

void game_state::cancel_cmd_repeat(std::string reason)
{
    if (!is_repeating_cmd())
        return;

    if (is_replaying_keys() || cmd_repeat_start)
        flush_input_buffer(FLUSH_KEY_REPLAY_CANCEL);

    if (is_processing_macro())
        flush_input_buffer(FLUSH_ABORT_MACRO);

    reset_cmd_repeat();

    if (!reason.empty())
        mpr(reason.c_str());
}

void game_state::cancel_cmd_again(std::string reason)
{
    if (!doing_prev_cmd_again)
        return;

    flush_input_buffer(FLUSH_KEY_REPLAY_CANCEL);

    if (is_processing_macro())
        flush_input_buffer(FLUSH_ABORT_MACRO);

    reset_cmd_again();

    if (!reason.empty())
        mpr(reason.c_str());
}

void game_state::cancel_cmd_all(std::string reason)
{
    cancel_cmd_repeat(reason);
    cancel_cmd_again(reason);
}

void game_state::cant_cmd_repeat(std::string reason)
{
    if (reason.empty())
        reason = "Can't repeat that command.";

    cancel_cmd_repeat(reason);
}

void game_state::cant_cmd_again(std::string reason)
{
    if (reason.empty())
        reason = "Can't redo that command.";

    cancel_cmd_again(reason);
}

void game_state::cant_cmd_any(std::string reason)
{
    cant_cmd_repeat(reason);
    cant_cmd_again(reason);
}

// The method is called to prevent the "no repeating zero turns
// commands" message that input() generates (in the absence of
// cancelling the repeition) for a repeated command that took no
// turns.  A wrapper around cancel_cmd_repeat(), its only purpose it
// to make it clear why cancel_cmd_repeat() is being called.
void game_state::zero_turns_taken()
{
    ASSERT(!you.turn_is_over);
    cancel_cmd_repeat();
}

bool interrupt_cmd_repeat( activity_interrupt_type ai,
                           const activity_interrupt_data &at )
{
    if (crawl_state.cmd_repeat_start)
        return (false);

    if (crawl_state.repeat_cmd == CMD_WIZARD)
        return (false);

    switch (ai)
    {
    case AI_STATUE:
    case AI_HUNGRY:
    case AI_TELEPORT:
    case AI_FORCE_INTERRUPT:
    case AI_HP_LOSS:
    case AI_MONSTER_ATTACKS:
        crawl_state.cancel_cmd_repeat("Command repetition interrupted.");
        return (true);

    default:
        break;
    }

    if (ai == AI_SEE_MONSTER)
    {
        const monsters* mon = static_cast<const monsters*>(at.data);
        if (!mon->visible())
            return (false);

        if (crawl_state.cmd_repeat_started_unsafe
            && at.context != "newly seen")
        {
            return (false);
        }

        crawl_state.cancel_cmd_repeat();

#ifndef DEBUG_DIAGNOSTICS
        if (at.context == "newly seen")
        {
            std::string text = get_monster_equipment_desc(mon, false);
            text += " comes into view.";
            print_formatted_paragraph(text, MSGCH_WARN);
        }

        if (Options.tutorial_left)
            tutorial_first_monster(*mon);
#else
        formatted_string fs( channel_to_colour(MSGCH_WARN) );
        fs.cprintf("%s (", mon->name(DESC_PLAIN, true).c_str());
        fs.add_glyph( mon );
        fs.cprintf(") in view: (%d,%d), see_grid: %s",
                   mon->pos().x, mon->pos().y,
                   see_grid(mon->pos())? "yes" : "no");
        formatted_mpr(fs, MSGCH_WARN);
#endif

        return (true);
    }

    // If command repetition is being used to imitate the rest command,
    // then everything interrupts it.
    if (crawl_state.repeat_cmd == CMD_MOVE_NOWHERE
        || crawl_state.repeat_cmd == CMD_SEARCH)
    {
        if (ai == AI_FULL_MP)
            crawl_state.cancel_cmd_repeat("Magic restored.");
        else if (ai == AI_FULL_HP)
            crawl_state.cancel_cmd_repeat("HP restored.");
        else
            crawl_state.cancel_cmd_repeat("Command repetition interrupted.");

        return (true);
    }

    if (crawl_state.cmd_repeat_started_unsafe)
        return (false);

    if (ai == AI_HIT_MONSTER)
    {
        // This check is for when command repetition is used to
        // whack away at a 0xp monster, since the player feels safe
        // when the only monsters around are 0xp.
        const monsters* mon = static_cast<const monsters*>(at.data);

        if (mons_class_flag(mon->type, M_NO_EXP_GAIN)
            && player_monster_visible(mon))
        {
            return (false);
        }

        crawl_state.cancel_cmd_repeat("Command repetition interrupted.");
        return (true);
    }

    return (false);
}

void game_state::reset_cmd_repeat()
{
    repeat_cmd           = CMD_NO_CMD;
    cmd_repeat_count     = 0;
    cmd_repeat_goal      = 0;
    cmd_repeat_start     = false;
    prev_repetition_turn = 0;

    repeat_cmd_keys.clear();
}

void game_state::reset_cmd_again()
{
    doing_prev_cmd_again = false;
    prev_cmd             = CMD_NO_CMD;

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
    ASSERT(!(god_act.depth > 0 && god_act.which_god == GOD_NO_GOD));
    ASSERT(!(god_act.depth == 0 && god_act.which_god != GOD_NO_GOD));
    ASSERT(!(god_act.depth == 0 && god_act_stack.size() > 0));

    return (god_act.depth > 0);
}

bool game_state::is_god_retribution() const
{
    ASSERT(is_god_acting());

    return (god_act.retribution);
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

    if (god_act.which_god != GOD_NO_GOD &&
        god_act.which_god != which_god)
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
        if (god_act_stack.size() > 0)
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
    ASSERT(god_act_stack.size() == 0);

    god_act.reset();
}

std::vector<god_act_state> game_state::other_gods_acting() const
{
    ASSERT(is_god_acting());
    return god_act_stack;
}

bool game_state::is_mon_acting() const
{
    return (mon_act != NULL);
}

monsters* game_state::which_mon_acting() const
{
    return (mon_act);
}

void game_state::inc_mon_acting(monsters* mon)
{
    ASSERT(!invalid_monster(mon));

    if (mon_act != NULL)
        mon_act_stack.push_back(mon_act);

    mon_act = mon;
}

void game_state::dec_mon_acting(monsters* mon)
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

void game_state::mon_gone(monsters* mon)
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
    fprintf(stderr, EOL "Game state:" EOL EOL);

    fprintf(stderr, "mouse_enabled: %d, waiting_for_command: %d, "
                  "terminal_resized: %d" EOL,
            mouse_enabled, waiting_for_command, terminal_resized);
    fprintf(stderr, "io_inited: %d, need_save: %d, saving_game: %d, "
                  "updating_scores: %d:" EOL,
            io_inited, need_save, saving_game, updating_scores);
    fprintf(stderr, "seen_hups: %d, map_stat_gen: %d, arena: %d, "
                  "arena_suspended: %d, unicode_ok: %d" EOL,
            seen_hups, map_stat_gen, arena, arena_suspended, unicode_ok);

    fprintf(stderr, EOL);

    // Arena mode can change behavior of the rest of the code and/or lead
    // to asserts.
    unwind_bool _arena(arena, false);
    unwind_bool _arena_suspended(arena_suspended, false);

    if (!startup_errors.empty())
    {
        fprintf(stderr, "Startup errors:" EOL);
        for (unsigned int i = 0; i < startup_errors.size(); i++)
            fprintf(stderr, "%s" EOL, startup_errors[i].c_str());
        fprintf(stderr, EOL);
    }

    fprintf(stderr, "prev_cmd = %s" EOL, command_to_name(prev_cmd).c_str());

    if (doing_prev_cmd_again)
    {
        fprintf(stderr, "Doing prev_cmd again with keys: ");
        for (unsigned int i = 0; i < prev_cmd_keys.size(); i++)
            fprintf(stderr, "%d, ", prev_cmd_keys[i]);
        fprintf(stderr, EOL);
        fprintf(stderr, "As ASCII keys: ");
        for (unsigned int i = 0; i < prev_cmd_keys.size(); i++)
            fprintf(stderr, "%c", (char) prev_cmd_keys[i]);
        fprintf(stderr, EOL EOL);
    }
    fprintf(stderr, "repeat_cmd = %s" EOL, command_to_name(repeat_cmd).c_str());

    if (cmd_repeat_count > 0 || cmd_repeat_goal > 0)
    {
        fprintf(stderr, "Doing command repetition:" EOL);
        fprintf(stderr, "cmd_repeat_start:%d, cmd_repeat_count: %d, "
                      "cmd_repeat_goal:%d" EOL
                      "prev_cmd_repeat_goal: %d" EOL,
                cmd_repeat_start, cmd_repeat_count, cmd_repeat_goal,
                prev_cmd_repeat_goal);
        fprintf(stderr, "Keys being repeated: ");
        for (unsigned int i = 0; i < repeat_cmd_keys.size(); i++)
            fprintf(stderr, "%d, ", repeat_cmd_keys[i]);
        fprintf(stderr, EOL);
        fprintf(stderr, "As ASCII keys: ");
        for (unsigned int i = 0; i < repeat_cmd_keys.size(); i++)
            fprintf(stderr, "%c", (char) repeat_cmd_keys[i]);
        fprintf(stderr, EOL);
    }

    fprintf(stderr, EOL);

    if (god_act.which_god != GOD_NO_GOD || god_act.depth != 0)
    {
        fprintf(stderr, "God %s currently acting with depth %d" EOL EOL,
                god_name(god_act.which_god).c_str(), god_act.depth);
    }

    if (god_act_stack.size() != 0)
    {
        fprintf(stderr, "Other gods acting:" EOL);
        for (unsigned int i = 0; i < god_act_stack.size(); i++)
            fprintf(stderr, "God %s with depth %d" EOL,
                    god_name(god_act_stack[i].which_god).c_str(),
                    god_act_stack[i].depth);
        fprintf(stderr, EOL EOL);
    }

    if (mon_act != NULL)
    {
        fprintf(stderr, "%s currently acting:" EOL EOL,
                debug_mon_str(mon_act).c_str());
        debug_dump_mon(mon_act, true);
    }

    if (mon_act_stack.size() != 0)
    {
        fprintf(stderr, "Others monsters acting:" EOL);
        for (unsigned int i = 0; i < mon_act_stack.size(); i++)
            fprintf(stderr, "    %s" EOL,
                    debug_mon_str(mon_act_stack[i]).c_str());
    }
}
