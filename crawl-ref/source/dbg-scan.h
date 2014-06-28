/**
 * @file
 * @brief Debugging code to scan the list of items and monsters.
**/

#ifndef DBGSCAN_H
#define DBGSCAN_H

void debug_item_scan();
void debug_mons_scan();
void check_map_validity();

#ifdef DEBUG_DIAGNOSTICS
void objstat_record_item(item_def &item);
void objstat_generate_stats();
void objstat_record_monster(monster *mons);
#endif

#endif
