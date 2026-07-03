/**
 * @file
 * @brief Handling for lurker monsters.
**/

#pragma once

#include "mon-transit.h"

struct lurker_data
{
    follower mon;           // The lurker monster itself

    coord_def pos;          // The place the lurker is lurking

    bool alerted;           // Whether a lurker has been alerted (and is pending
                            // actual placement on the level)

    int timer;              // If alerted is true, the timestamp of when to try
                            // placing this lurker.

                            // If alerted is false, the timestamp of when to
                            // allow noise to alert them after a previously
                            // failed pathfinding check (eg: if the monster is
                            // behind a runed door).

    bool ignore_threat;     // Whether to ignore the threat check when placing
                            // this lurker (ie: allow it to appear even if no
                            // other monsters are nearby).
};

void start_lurking_at(monster& mon, const coord_def& pos);
void start_lurking(monster& mon);

void alert_lurker_at(const coord_def& pos, bool do_pathfind_check = false);

void init_lurker_map();

void handle_lurkers();

void cancel_pending_lurkers();
