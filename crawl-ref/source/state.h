/*
 *  File:       state.h
 *  Summary:    Game state.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#ifndef STATE_H
#define STATE_H

#include "enum.h"
#include <vector>

class monsters;
class mon_acting;

class crawl_exit_hook
{
public:
    crawl_exit_hook();
    virtual ~crawl_exit_hook();
    virtual void restore_state() = 0;
};

struct god_act_state
{
public:

    god_act_state();
    void reset();

    god_type which_god;
    bool     retribution;
    int      depth;
};

// Track various aspects of Crawl game state.
struct game_state
{
    bool game_crashed;      // The game crashed and is now in the process of
                            // dumping crash info.
    bool mouse_enabled;     // True if mouse input is currently relevant.

    bool waiting_for_command; // True when the game is waiting for a command.
    bool terminal_resized;   // True if the term was resized and we need to
                             // take action to handle it.

    bool io_inited;         // Is curses or the equivalent initialised?
    bool need_save;         // Set to true when game has started.
    bool saving_game;       // Set to true while in save_game.
    bool updating_scores;   // Set to true while updating hiscores.

    int  seen_hups;         // Set to true if SIGHUP received.

    bool map_stat_gen;      // Set if we're generating stats on maps.

    bool arena;             // Set if we're in arena mode.
    bool arena_suspended;   // Set if the arena has been temporarily
                            // suspended.

    bool unicode_ok;        // Is unicode support available?

    std::string (*glyph2strfn)(unsigned glyph);
    int  (*multibyte_strlen)(const std::string &s);
    void (*terminal_resize_handler)();
    void (*terminal_resize_check)();

    bool            doing_prev_cmd_again;
    command_type    prev_cmd;
    std::deque<int> prev_cmd_keys;

    command_type    repeat_cmd;
    std::deque<int> repeat_cmd_keys;
    bool            cmd_repeat_start;
    int             cmd_repeat_count;
    int             cmd_repeat_goal;
    int             prev_cmd_repeat_goal;
    int             prev_repetition_turn;
    bool            cmd_repeat_started_unsafe;

    std::vector<std::string> startup_errors;

    std::vector<std::string> input_line_strs;
    unsigned int             input_line_curr;

    // Hooks to call if get shut down unexpectedly.
    std::vector<crawl_exit_hook*> exit_hooks;

    bool level_annotation_shown;

protected:
    void reset_cmd_repeat();
    void reset_cmd_again();

    god_act_state              god_act;
    std::vector<god_act_state> god_act_stack;

    monsters*              mon_act;
    std::vector<monsters*> mon_act_stack;

public:
    game_state();

    void add_startup_error(const std::string &error);
    void show_startup_errors();

    bool is_replaying_keys() const;

    bool is_repeating_cmd() const;

    void cancel_cmd_repeat(std::string reason = "");
    void cancel_cmd_again(std::string reason = "");
    void cancel_cmd_all(std::string reason = "");

    void cant_cmd_repeat(std::string reason = "");
    void cant_cmd_again(std::string reason = "");
    void cant_cmd_any(std::string reason = "");

    void zero_turns_taken();

    void check_term_size() const
    {
        if (terminal_resize_check)
            (*terminal_resize_check)();
    }

    bool     is_god_acting() const;
    bool     is_god_retribution() const;
    god_type which_god_acting() const;
    void     inc_god_acting(bool is_retribution = false);
    void     inc_god_acting(god_type which_god, bool is_retribution = false);
    void     dec_god_acting();
    void     dec_god_acting(god_type which_god);
    void     clear_god_acting();

    std::vector<god_act_state> other_gods_acting() const;

    bool      is_mon_acting() const;
    monsters* which_mon_acting() const;
    void      inc_mon_acting(monsters* mon);
    void      dec_mon_acting(monsters* mon);
    void      clear_mon_acting();
    void      mon_gone(monsters* mon);

    void dump();

    friend class mon_acting;
};

extern game_state crawl_state;

class god_acting
{
public:
    god_acting(bool is_retribution = false)
        : god(you.religion)
    {
        crawl_state.inc_god_acting(god, is_retribution);
    }
    god_acting(god_type who, bool is_retribution = false)
        : god(who)
    {
        crawl_state.inc_god_acting(god, is_retribution);
    }
    ~god_acting()
    {
        crawl_state.dec_god_acting(god);
    }
private:
    god_type god;
};

class mon_acting
{
public:
    mon_acting(monsters* _mon) : mon(_mon)
    {
        crawl_state.inc_mon_acting(_mon);
    }

    ~mon_acting()
    {
        // Monster might have died in the meantime.
        if (mon == crawl_state.mon_act)
            crawl_state.dec_mon_acting(mon);
    }

private:
    monsters *mon;
};


#endif
