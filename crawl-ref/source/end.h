/**
 * @file
 * @brief Handle shutdown.
 **/

#ifndef END_H
#define END_H

#include "hiscores.h" // scorefile_entry

NORETURN void end(int exit_code, bool print_err = false, PRINTF(2, = nullptr));
NORETURN void end_game(scorefile_entry &se, int hiscore_index = -1);
NORETURN void game_ended();
NORETURN void game_ended_with_error(const string &message);
NORETURN void screen_end_game(string text);
void cio_cleanup();

struct game_ended_condition : public exception
{
    game_ended_condition(bool saved = false) : was_saved(saved) {}
    bool was_saved;
};


#endif
