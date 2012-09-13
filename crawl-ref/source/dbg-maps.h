/**
 * @file
 * @brief Map generation statistics/testing.
**/

#ifndef DBGDGN_H
#define DBGDGN_H

#ifdef DEBUG_DIAGNOSTICS
void generate_map_stats();

class map_def;
void mapgen_report_map_try(const map_def &map);
void mapgen_report_map_use(const map_def &map);
void mapgen_report_error(const map_def &map, const string &err);
void mapgen_report_map_build_start();
void mapgen_report_map_veto();
#endif

#endif
