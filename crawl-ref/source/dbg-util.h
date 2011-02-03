/*
 *  File:       dbg-util.h
 *  Summary:    Miscellaneous debugging functions.
 *  Written by: Linley Henzell and Jesse Jones
 */

#ifndef DBGUTIL_H
#define DBGUTIL_H

monster_type debug_prompt_for_monster(void);
skill_type   debug_prompt_for_skill(const char *prompt);

int debug_cap_stat(int stat);

void error_message_to_player(void);

void debug_dump_levgen();

struct item_def;
std::string debug_art_val_str(const item_def& item);

class monster;
struct coord_def;

std::string debug_coord_str(const coord_def &pos);

void debug_dump_mon(const monster* mon, bool recurse);

std::string debug_mon_str(const monster* mon);

#endif
