/**
 * @file
 * @brief Handle shutdown.
 **/

#pragma once

#include "hiscores.h" // scorefile_entry
#include "game-exit-type.h"
#include "ouch.h"

bool crawl_should_restart(game_exit exit);

NORETURN void end(int exit_code, bool print_err = false, PRINTF(2, = nullptr));
NORETURN void end_game(scorefile_entry &se, int hiscore_index = -1);
NORETURN void game_ended(game_exit exit, const string &message = "");
NORETURN void game_ended_with_error(const string &message);
NORETURN void screen_end_game(string text);
void cio_cleanup();
bool fatal_error_notification(string error_msg);

struct game_ended_condition : public exception
{
    game_ended_condition(game_exit exit, string msg = "") : exit_reason(exit), message(msg) {}
    game_exit exit_reason;
    string message;
};
