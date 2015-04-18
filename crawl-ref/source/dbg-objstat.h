/**
 * @file
 * @brief Objstat: monster and item generation statistics
**/

#ifndef DBGOBJSTAT_H
#define DBGOBJSTAT_H

#ifdef DEBUG_DIAGNOSTICS
void objstat_record_item(item_def &item);
void objstat_generate_stats();
void objstat_record_monster(monster *mons);
void objstat_iteration_stats();
#endif

#endif //DBGOBJSTAT_H
