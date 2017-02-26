/**
 * @file
 * @brief Objstat: monster and item generation statistics
**/

#pragma once

#ifdef DEBUG_STATISTICS
void objstat_record_item(const item_def &item);
void objstat_generate_stats();
void objstat_record_monster(const monster *mons);
void objstat_iteration_stats();
#endif

