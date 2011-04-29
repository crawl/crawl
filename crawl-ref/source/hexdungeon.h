/*
 *  File:       hexdungeon.cc
 *  Summary:    Hex-specific functions used when building new levels.
 *  Written by: Martin Bays
 *
 */

void dissolved_cavern(int level_number, bool dry=false);
char stone_circle(int level_number);
void builder_basic_hex_trails(int level_number);
void builder_basic_hex_rooms(int level_number);
bool join_the_dots_hex(const hexcoord &from, const hexcoord &to,
                          unsigned mmask, bool early_exit = false);
bool join_the_dots_hex_pathfind(const hexcoord &from, const hexcoord &to,
                          unsigned mmask, bool early_exit = false);
