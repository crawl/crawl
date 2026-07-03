#pragma once

#include "coord.h"
#include "enum.h"
#include "feature.h"
#include "externs.h"

class reader;
class writer;

void set_terrain_mapped(const coord_def c);
void set_terrain_seen(const coord_def c);

void set_terrain_visible(const coord_def c);
void clear_terrain_visibility();

int count_detected_mons();

void update_cloud_knowledge();

bool magic_mapping(int map_radius, int proportion, bool suppress_msg,
                   bool force = false, bool deterministic = false,
                   bool full_info = false, bool range_falloff = true,
                   coord_def origin = coord_def(-1, -1),
                   bool respect_no_automap = false);

/**
 * @brief Clear non-terrain knowledge from the map.
 *
 * Cloud knowledge is always cleared. Item and monster knowledge clears depend
 * on @p clear_items and @p clear_mons, respectively.
 *
 * @param clear_items
 *  Clear item knowledge?
 * @param clear_mons
 *  Clear monster knowledge?
 */
void clear_map(bool clear_items = false, bool clear_mons = true);

/**
 * @brief If a travel trail exists, clear it; otherwise clear the map.
 *
 * When clearing the map, all non-terrain knowledge is wiped.
 *
 * @see clear_map() and clear_travel_trail()
 */
void clear_map_or_travel_trail();

map_feature get_cell_map_feature(const coord_def& gc);
map_feature get_cell_map_feature(const map_cell& cell);
bool is_explore_horizon(const coord_def& c);

void reautomap_level();

/**
 * @brief Get the bounding box of the known map.
 *
 * @return pair of {topleft coord, bottomright coord} of bbox.
 */
std::pair<coord_def, coord_def> known_map_bounds();
bool in_known_map_bounds(const coord_def& p);

void update_terrain_knowledge(coord_def pos,
                              bool partial_knowledge_only = false);
void update_grid_colour_knowledge(coord_def pos,
                             bool partial_knowledge_only = false);

struct invis_mon_data
{
    mid_t mid;              // mid of the monster in question

    int last_seen_time;     // Timestamp of the last time knowledge of this
                            // monster was updated.
                            // ('Stale' information is removed after 15 turns
                            // without further updates.)

    coord_def last_known_pos;  // If the monster's position was ever known for
                               // sure, this is the last position the player was
                               // certain they occupied.

    coord_def last_player_pos;  // The position the player was standing in the
                                // last time any knowledge of this monster was
                                // given (even if it did not reveal their exact
                                // position, such as when you are attacked).
                                //
                                // This is used to estimate player safety when
                                // trying to rest with invisible creatures still
                                // alive somewhere.
};
class invis_monster_knowledge
{
private:
    vector<invis_mon_data> data;
    bool unknown_invis_nearby;

public:
    coord_def last_known_pos(const monster& mon);
    monster* memory_at(const coord_def& pos);
    void update(const monster& mon, bool reveal_pos = true,
                coord_def forced_pos = coord_def());
    void handle_time();
    void clear();
    void suppress_invis_warning();
    bool any_unknown_nearby();
    vector<monster_info> get_unknown_in_los();
    string get_unknown_monster_description();

    void marshall(writer &) const;
    void unmarshall(reader &);
};
