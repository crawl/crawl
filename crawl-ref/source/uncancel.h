/**
 * @file
 * @brief User interactions that need to be restarted if the game is forcibly
 *        saved (via SIGHUP/window close).
**/

#pragma once

#include "uncancellable-type.h"

void add_uncancel(uncancellable_type kind, int arg = 0);
void run_uncancels();

static inline void run_uncancel(uncancellable_type kind, int arg = 0)
{
    add_uncancel(kind, arg);
    run_uncancels();
}

