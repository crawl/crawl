/**
 * @file
 * @brief Map generation statistics/testing.
**/

#ifndef DBGDGN_H
#define DBGDGN_H

#ifdef DEBUG_DIAGNOSTICS

class map_def;
void mapstat_report_map_try(const map_def &map);
void mapstat_report_map_use(const map_def &map);
void mapstat_report_error(const map_def &map, const string &err);
void mapstat_report_map_build_start();
void mapstat_report_map_veto(const string &message);
void mapstat_generate_stats();
void mapstat_build_levels(int niters);
#endif

#endif
