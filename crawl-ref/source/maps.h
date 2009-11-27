/*
 *  File:       maps.cc
 *  Summary:    Functions used to create vaults.
 *  Written by: Linley Henzell
 */


#ifndef MAPS_H
#define MAPS_H

#include <vector>

#include "mapdef.h"  // for typedef depth_ranges

struct level_range;
class map_def;
struct map_file_place;
struct vault_placement;

int vault_main(vault_placement &vp, const map_def *vault,
               bool check_place = false);

// Given a rectangular region, slides it to fit into the map. size must be
// smaller than (GXM,GYM).
void fit_region_into_map_bounds(coord_def &pos, const coord_def &size,
                                int margin = 0);

const map_def *map_by_index(int index);
int map_count();
int map_count_for_tag(const std::string &tag, bool check_depth = false);

const map_def *find_map_by_name(const std::string &name);
const map_def *random_map_for_place(const level_id &place, bool minivault);
const map_def *random_map_in_depth(const level_id &lid,
                                   bool want_minivault = false);
const map_def *random_map_for_tag(const std::string &tag,
                                  bool check_depth = false);
void add_parsed_map(const map_def &md);

std::vector<std::string> find_map_matches(const std::string &name);

void read_maps();
void read_map(const std::string &file);
void run_map_preludes();
void reset_map_parser();
std::string get_descache_path(const std::string &file,
                              const std::string &ext);

typedef std::map<std::string, map_file_place> map_load_info_t;

extern map_load_info_t lc_loaded_maps;
extern std::string     lc_desfile;
extern map_def         lc_map;
extern level_range     lc_range;
extern depth_ranges    lc_default_depths;
extern dlua_chunk      lc_global_prelude;
extern bool            lc_run_global_prelude;

typedef bool (*map_place_check_t)(const map_def &, const coord_def &c,
                                  const coord_def &size);

typedef std::vector<coord_def> point_vector;

extern map_place_check_t map_place_valid;
extern point_vector      map_anchor_points;

const int              MAP_CACHE_VERSION = 1011;


#ifdef DEBUG_DIAGNOSTICS
void mg_report_random_maps(FILE *outf, const level_id &place);
#endif

#endif
