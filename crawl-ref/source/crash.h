/**
 * @file
 * @brief Platform specific crash handling functions.
**/

#pragma once

#include <cstdio>

void init_crash_handler();
void dump_crash_info(FILE* file);
void write_stack_trace(FILE* file, int ignore_count);
void call_gdb(FILE *file);
void disable_other_crashes();
void do_crash_dump();

void watchdog();
