/**
 * @file
 * @brief Map generation statistics/testing.
**/

#pragma once

#ifdef DEBUG_STATISTICS

class map_def;
void mapstat_report_map_try(const map_def &map);
void mapstat_report_map_use(const map_def &map);
void mapstat_report_map_success(const string &map_name);
void mapstat_report_error(const map_def &map, const string &err);
void mapstat_report_map_build_start();
void mapstat_report_map_veto(const string &message);
void mapstat_generate_stats();
bool mapstat_build_levels();
bool mapstat_find_forced_map();
#endif
