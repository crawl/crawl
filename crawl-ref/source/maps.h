/*
 *  File:       maps.cc
 *  Summary:    Functions used to create vaults.
 *  Written by: Linley Henzell
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
    int x, y;
    int width, height;
    int orient;
    map_def map;
    std::vector<coord_def> exits;

    vault_placement()
        : x(-1), y(-1), width(0), height(0), orient(0), map(),
          exits()
    {
    }
};

int vault_main(map_type vgrid, 
               vault_placement &vp,
               int vault_force,
               std::vector<vault_placement> *avoid_vaults = NULL);

const map_def *map_by_index(int index);
int map_count();
int random_map_for_place(const level_id &place, bool mini = false);
int random_map_in_depth(const level_id &lid, bool want_minivault = false);
int random_map_for_tag(const std::string &tag, bool want_minivault,
                       bool check_depth = false);
void add_parsed_map(const map_def &md);

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

const int              MAP_CACHE_VERSION = 1005;

#endif
