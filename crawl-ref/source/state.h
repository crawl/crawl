/*
 *  File:       state.h
 *  Summary:    Game state.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author: j-p-e-g $ on $Date: 2007-09-10 11:21:32 -0700 (Mon, 10 Sep 2007) $
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

    game_state() : mouse_enabled(false), waiting_for_command(false),
                   terminal_resized(false), io_inited(false), need_save(false),
                   saving_game(false), updating_scores(false),
                   seen_hups(0), map_stat_gen(false), unicode_ok(false),
                   glyph2strfn(NULL), multibyte_strlen(NULL),
                   terminal_resize_handler(NULL), terminal_resize_check(NULL)
    {
    }

    void check_term_size() const
    {
        if (terminal_resize_check)
            (*terminal_resize_check)();
    }
};
extern game_state crawl_state;

#endif
