/**
 * @file
 * @brief Platform specific crash handling functions.
**/

#ifndef CRASH_H
#define CRASH_H

#include <stdio.h>

void init_crash_handler();
void dump_crash_info(FILE* file);
void write_stack_trace(FILE* file, int ignore_count);
void disable_other_crashes();
void do_crash_dump();

#ifdef DGAMELAUNCH
void watchdog();
#else
// If there's a local player, he can kill the game himself, so there's no
// false positives.
# define watchdog()
#endif

#endif
