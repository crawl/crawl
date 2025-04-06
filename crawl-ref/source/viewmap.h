#pragma once

#include <vector>

#include "coordit.h"
#include "env.h"
#include "files.h"
#include "output.h"
#include "player.h"
#include "terrain.h"
#include "tilesdl.h"
#include "tileweb.h"
#include "tileview.h"
#include "travel.h"
#include "viewchar.h"

using std::vector;

class levelview_excursion : public level_excursion
{
    bool travel_mode;

public:
    levelview_excursion(bool tm)
        : travel_mode(tm) {}

    ~levelview_excursion()
    {
        if (!you.on_current_level)
            go_to(original);
        print_stats_level();
    }

    // Not virtual!
    void go_to(const level_id& next)
    {
#ifdef USE_TILE
        tiles.clear_minimap();
#endif
        level_excursion::go_to(next);
        tile_new_level(false);

        if (travel_mode)
        {
            travel_init_new_level();
            travel_cache.update();
        }
    }
};

class feature_list;

class map_control_state
{
public:
    bool redraw_map;
    bool map_alive;
    bool chose;

    level_pos lpos;
    coord_def search_anchor;
    int search_index;

    vector<coord_def> *features;
    feature_list *feats;
    levelview_excursion *excursion;
    level_id original;

    bool on_level;
    bool allow_offlevel;
    bool travel_mode;
};

class map_view_state
{
public:
    coord_def cursor;
    coord_def start;
};

struct level_pos;
bool travel_colour_override(const coord_def& p);
bool is_feature(char32_t feature, const coord_def& where);
bool show_map(level_pos &spec_place,
              bool travel_mode, bool allow_offlevel);
void process_map_command(command_type cmd);
#ifdef USE_TILE_LOCAL
void start_far_viewing(coord_def viewed_location);
void stop_far_viewing();
bool is_far_viewing();
void update_far_viewing(coord_def viewed_location);
#endif
level_pos map_follow_stairs(bool up, const coord_def &pos);

bool emphasise(const coord_def& where);
vector<coord_def> search_path_around_point(coord_def centre);
