/*
 *  File:       maps.cc
 *  Summary:    Functions used to create vaults.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef MAPS_H
#define MAPS_H

#include "FixVec.h"
#include "dungeon.h"
#include "mapdef.h"

#include <vector>

class map_def;
struct vault_placement
{
    coord_def pos;
    coord_def size;

    int orient;
    map_def map;
    std::vector<coord_def> exits;

    vault_placement()
        : pos(-1, -1), size(0, 0), orient(0), map(),
          exits()
    {
    }
};

int vault_main(map_type vgrid, 
               vault_placement &vp,
               int vault_force,
               bool check_place = false,
               bool clobber = false);

// Given a rectangular region, slides it to fit into the map. size must be
// smaller than (GXM,GYM).
void fit_region_into_map_bounds(coord_def &pos, const coord_def &size);

const map_def *map_by_index(int index);
int map_count();
int find_map_by_name(const std::string &name);
int random_map_for_place(const level_id &place, bool mini = false);
int random_map_in_depth(const level_id &lid, bool want_minivault = false);
int random_map_for_tag(const std::string &tag, bool want_minivault,
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

const int              MAP_CACHE_VERSION = 1007;


#ifdef DEBUG_DIAGNOSTICS
void mg_report_random_maps(FILE *outf, const level_id &place);
#endif

#endif
