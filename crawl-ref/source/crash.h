/*
 *  File:       crash.h
 *  Summary:    Platform specific crash handling functions.
 *  Written by: Matthew cline
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#ifndef CRASH_H
#define CRASH_H

#include <stdio.h>

void init_crash_handler();
void dump_crash_info(FILE* file);
void write_stack_trace(FILE* file, int ignore_count);

#endif
