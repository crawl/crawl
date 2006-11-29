/*
 *  File:       chardump.cc
 *  Summary:    Dumps character info out to the morgue file.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     4/20/99        JDJ             Reformatted, uses string objects, split out
 *                                                                      7 functions from dump_char, dumps artifact info.
 */


#ifndef CHARDUMP_H
#define CHARDUMP_H

#include <string>

bool dump_char(const std::string &fname, 
               bool show_prices, 
               bool full_id = false);

void resists_screen();
void display_notes();

std::string munge_description(const std::string &inStr);

const char *hunger_level(void);

#endif
