/*
 *  File:       state.h
 *  Summary:    Game state.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 * <1>     7/11/07    MPC   Split off from externs.h
 */

#ifndef STATE_H
#define STATE_H

// Track various aspects of Crawl game state.
struct game_state
{
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

    std::vector<std::string> input_line_strs;
    unsigned int             input_line_curr;

protected:
    void reset_cmd_repeat();
    void reset_cmd_again();

public:
    game_state();

    bool is_replaying_keys() const;

    bool is_repeating_cmd() const;

    void cancel_cmd_repeat(std::string reason = "");
    void cancel_cmd_again(std::string reason = "");

    void cant_cmd_repeat(std::string reason = "");
    void cant_cmd_again(std::string reason = "");

    void zero_turns_taken();

    void check_term_size() const
    {
        if (terminal_resize_check)
            (*terminal_resize_check)();
    }
};
extern game_state crawl_state;

#endif
