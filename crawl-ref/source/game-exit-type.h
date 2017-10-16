#pragma once

// What caused the game to exit?
// utility functions in end.cc, cf. kill_method_type in ouch.h
enum game_exit_type
{
    GAME_EXIT_UNKNOWN, // for ordinary games, no previous game this session.
    GAME_EXIT_WON,
    GAME_EXIT_LEFT,
    GAME_EXIT_QUIT,
    GAME_EXIT_DIED,
    GAME_EXIT_SAVE,
    GAME_EXIT_ABORT,   // used when a game is aborted before it starts, e.g.
                       // when exiting character selection, or aborting a text
                       // entry.
    GAME_EXIT_CRASH
};
