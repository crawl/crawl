/**
 * @file
 * @brief Game state.
**/

#pragma once

#include <vector>

#include "command-type.h"
#include "disable-type.h"
#include "end.h"
#include "game-exit-type.h"
#include "player.h"

class monster;
class mon_acting;
class targeter;
struct activity_interrupt_data;

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
    vector<string> command_line_arguments;

    bool game_crashed;      // The game crashed and is now in the process of
                            // dumping crash info.
    bool crash_debug_scans_safe; // should the crash handler run various debug
                                 // scans?

    bool mouse_enabled;     // True if mouse input is currently relevant.

    bool waiting_for_command; // True when the game is waiting for a command.
    bool terminal_resized;   // True if the term was resized and we need to
                             // take action to handle it.
    time_t last_winch;       // Time of last resize, for crash dumps.

    uint32_t seed;

    bool io_inited;         // Is curses or the equivalent initialised?
    bool need_save;         // Set to true when game can be saved, false when the game ends.
    bool game_started;      // Set to true when a game has started.
    bool saving_game;       // Set to true while in save_game.
    bool updating_scores;   // Set to true while updating hiscores.
    const char* no_gdb;     // reason for not running gdb

    int  seen_hups;         // Set to true if SIGHUP received.

    bool map_stat_gen;      // Set if we're generating stats on maps.
    bool map_stat_dump_disconnect; // Set if we dump disconnected maps and exit
                                   // under mapstat.
    bool obj_stat_gen;      // Set if we're generating object stats.

    string force_map;       // Set if we're forcing a specific map to generate.

    game_type type;
    game_type last_type;
    game_ended_condition last_game_exit;
    bool marked_as_won;
    bool arena_suspended;   // Set if the arena has been temporarily
                            // suspended.
    bool generating_level;

    bool dump_maps;         // Dump map Lua to stderr on fresh parse.
    bool test;              // Set if we want to run self-tests and exit.
    bool test_list;         // Show available tests and exit.
    bool script;            // Set if we want to run a Lua script and exit.
    bool build_db;          // Set if we want to rebuild the db and exit.
    vector<string> tests_selected; // Tests to be run.
    vector<string> script_args;    // Arguments to scripts.

    bool throttle;
    bool bypassed_startup_menu;

    bool show_more_prompt;  // Set to false to disable --more-- prompts.

    string sprint_map;      // Sprint map set on command line, if any.

    string map;             // Map selected in the newgame menu

    void (*terminal_resize_handler)();
    void (*terminal_resize_check)();

    bool            doing_prev_cmd_again;
    command_type    prev_cmd;
    deque<int>      prev_cmd_keys;

    command_type    repeat_cmd;
    bool            cmd_repeat_start;
    command_type    prev_repeat_cmd;
    int             prev_cmd_repeat_goal;
    bool            cmd_repeat_started_unsafe;
    int             lua_calls_no_turn;
    bool            stat_gain_prompt;

    bool            simulating_xp_gain; // is the skill menu in xp potion mode?

    bool level_annotation_shown;
    bool viewport_monster_hp;
    bool viewport_weapons;

    bool tiles_disabled; // ignored unless USE_TILE_LOCAL is defined
    bool title_screen; // ignored unless USE_TILE_LOCAL is defined

    bool invisible_targeting;

    // Area beyond which view should be darkened,  0 = disabled.
    targeter *darken_range;

    // Monsters to highlight on the screen, 0 = disabled.
    vector<monster *> *flash_monsters;

    // Any changes to macros that need to be changed?
    bool unsaved_macros;

    FixedBitVector<NUM_DISABLEMENTS> disables;

    // Version of the last character save.
    int minor_version;

    // Git version of the character save. This will be empty unless the
    // character has been loaded from a previous save.
    std::string save_rcs_version;

    string default_startup_name;

    // Should flushing a nonempty key buffer error or crash? Used for tests.
    bool nonempty_buffer_flush_errors;

protected:
    void reset_cmd_repeat();
    void reset_cmd_again();

    god_act_state         god_act;
    vector<god_act_state> god_act_stack;

    monster*          mon_act;
    vector<monster* > mon_act_stack;

public:
    game_state();

    void reset_game();

    void add_startup_error(const string &error);

    bool is_replaying_keys() const;

    bool is_repeating_cmd() const;

    void cancel_cmd_repeat(string reason = "", bool force=false);
    void cancel_cmd_again(string reason = "", bool force=false);
    void cancel_cmd_all(string reason = "");

    void cant_cmd_repeat(string reason = "");
    void cant_cmd_again(string reason = "");
    void cant_cmd_any(string reason = "");

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

    vector<god_act_state> other_gods_acting() const;

    bool      is_mon_acting() const;
    monster* which_mon_acting() const;
    void      inc_mon_acting(monster* mon);
    void      dec_mon_acting(monster* mon);
    void      clear_mon_acting();
    void      mon_gone(monster* mon);

    void dump();
    bool player_is_dead() const;

    bool game_standard_levelgen() const;
    bool game_is_normal() const;
    bool game_is_tutorial() const;
    bool game_is_arena() const;
    bool game_is_sprint() const;
    bool game_is_hints() const;
    bool game_is_hints_tutorial() const;

    // Save subdirectory used for games such as Sprint.
    string game_type_name() const;
    string game_savedir_path() const;
    string game_type_qualifier() const;

    static string game_type_name_for(game_type gt);

    inline void mark_last_game_won()
    {
        marked_as_won = true;
    }

    friend class mon_acting;
};

#ifdef DEBUG_GLOBALS
#define crawl_state (*real_crawl_state)
#endif
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
    mon_acting(monster* _mon) : mon(_mon)
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
    monster* mon;
};

bool interrupt_cmd_repeat(activity_interrupt_type ai,
                          const activity_interrupt_data &at);
