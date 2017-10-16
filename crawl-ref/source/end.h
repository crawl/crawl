/**
 * @file
 * @brief Handle shutdown.
 **/

#pragma once

#include "hiscores.h" // scorefile_entry
#include "game-exit-type.h"
#include "ouch.h"

bool crawl_should_restart(game_exit_type exit);
game_exit_type kill_method_to_exit(kill_method_type kill);
string exit_type_to_string(game_exit_type e);

NORETURN void end(int exit_code, bool print_err = false, PRINTF(2, = nullptr));
NORETURN void end_game(scorefile_entry &se, int hiscore_index = -1);
NORETURN void game_ended(game_exit_type exit);
NORETURN void game_ended_with_error(const string &message);
NORETURN void screen_end_game(string text);
void cio_cleanup();

struct game_ended_condition : public exception
{
    game_ended_condition(game_exit_type exit=GAME_EXIT_UNKNOWN) : game_exit(exit) {}
    game_exit_type game_exit;
};
