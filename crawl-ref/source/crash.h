/**
 * @file
 * @brief Platform specific crash handling functions.
**/

#pragma once

#include <cstdio>

void init_crash_handler();
string crash_signal_info();
void write_stack_trace(FILE* file);
void call_gdb(FILE *file);
void disable_other_crashes();
void do_crash_dump();

void watchdog();
