/*
 *  File:       state.cc
 *  Summary:    Game state functions.
 *  Written by: Matthew Cline
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *   <1>    09/18/07      MPC    Created
 */

#include "AppHdr.h"
#include "externs.h"

#include "delay.h"
#include "direct.h"
#include "macro.h"
#include "menu.h" // For print_formatted_paragraph()
#include "message.h"
#include "mon-util.h"
#include "player.h"
#include "state.h"
#include "tutorial.h"
#include "view.h"

game_state::game_state()
    : mouse_enabled(false), waiting_for_command(false),
      terminal_resized(false), io_inited(false), need_save(false),
      saving_game(false), updating_scores(false), seen_hups(0),
      map_stat_gen(false), unicode_ok(false), glyph2strfn(NULL),
      multibyte_strlen(NULL), terminal_resize_handler(NULL),
      terminal_resize_check(NULL), doing_prev_cmd_again(false),
      prev_cmd(CMD_NO_CMD), repeat_cmd(CMD_NO_CMD), cmd_repeat_count(0),
      cmd_repeat_goal(0), prev_repetition_turn(0),
      cmd_repeat_started_unsafe(false), input_line_curr(0)
{
    reset_cmd_repeat();
    reset_cmd_again();
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

    if (reason != "")
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

    if (reason != "")
        mpr(reason.c_str());
}

void game_state::cant_cmd_repeat(std::string reason)
{
    if (reason == "")
        reason = "Can't repeat that command.";

    cancel_cmd_repeat(reason);
}

void game_state::cant_cmd_again(std::string reason)
{
    if (reason == "")
        reason = "Can't redo that command.";

    cancel_cmd_again(reason);
}

// The mehtod is called to prevent the "no repeating zero turns
// commands" message that input() generates (in the abscence of
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
        crawl_state.cancel_cmd_repeat("Command repetition interrupted.");
        return true;

    default:
        break;
    }

    if (ai == AI_SEE_MONSTER)
    {
        const monsters* mon = static_cast<const monsters*>(at.data);
        if (!mon->visible())
            return false;

        if (crawl_state.cmd_repeat_started_unsafe
            && at.context != "newly seen")
        {
            return false;
        }

        crawl_state.cancel_cmd_repeat();

#ifndef DEBUG_DIAGNOSTICS
        if (at.context == "newly seen")
        {
            std::string text = get_monster_desc(mon, false);
            text += " comes into view.";
            print_formatted_paragraph(text, get_number_of_cols(), MSGCH_WARN);
        }

        if (Options.tutorial_left)
        {
            // enforce that this message comes first
            tutorial_first_monster(*mon);
            if (get_mons_colour(mon) != mon->colour)
                learned_something_new(TUT_MONSTER_BRAND);
        }
#else
        formatted_string fs( channel_to_colour(MSGCH_WARN) );
        fs.cprintf("%s (", mon->name(DESC_PLAIN, true).c_str());
        fs.add_glyph( mon );
        fs.cprintf(") in view: (%d,%d), see_grid: %s",
             mon->x, mon->y,
             see_grid(mon->x, mon->y)? "yes" : "no");
        formatted_mpr(fs, MSGCH_WARN);
#endif

        return true;
    }
        
    // If command repitition is being used to immitate the rest command,
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
        return true;
    }

    if (crawl_state.cmd_repeat_started_unsafe)
        return false;

    if (ai == AI_HIT_MONSTER)
    {
        // This check is for when command repetition is used to
        // whack away at a 0xp monster, since the player feels safe
        // when the only monsters around are 0xp.
        const monsters* mon = static_cast<const monsters*>(at.data);

        if (mons_class_flag(mon->type, M_NO_EXP_GAIN)
            && player_monster_visible(mon))
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
