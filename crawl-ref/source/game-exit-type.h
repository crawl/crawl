#pragma once

// What caused the game to exit?
// utility functions in end.cc, cf. kill_method_type in ouch.h
enum class game_exit
{
    unknown, // for ordinary games, no previous game this session.
    win,
    leave,
    quit,
    death,
    save,
    abort, // used when a game is aborted before it starts, e.g.
           // when exiting character selection, or aborting a text
           // entry.
    crash
};

