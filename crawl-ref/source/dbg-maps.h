/*
 *  File:       dbd-maps.h
 *  Summary:    Map generation statistics/testing.
 *  Written by: Linley Henzell and Jesse Jones
 */

#ifndef DBGDGN_H
#define DBGDGN_H

#ifdef DEBUG_DIAGNOSTICS
void generate_map_stats();

class map_def;
void mapgen_report_map_try(const map_def &map);
void mapgen_report_map_use(const map_def &map);
void mapgen_report_error(const map_def &map, const std::string &err);
void mapgen_report_map_build_start();
void mapgen_report_map_veto();
#endif

#endif
