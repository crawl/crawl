#pragma once

#include <cstdint>
#include <vector>

#include "coord.h"
#include "fixedarray.h"
#include "item-def.h"
#include "map-cell.h"
#include "map-feature.h"
#include "mon-info.h"

class map_knowledge_type
{
public:
    map_knowledge_type() = default;
    map_knowledge_type(const map_knowledge_type&);
    map_knowledge_type(map_knowledge_type&&) noexcept = default;

    map_knowledge_type& operator=(const map_knowledge_type&);
    map_knowledge_type& operator=(map_knowledge_type&&) noexcept = default;

    const item_def* item(const map_cell& cell) const;
    const item_def* item(coord_def pos) const
    {
        return item(m_cells(pos));
    }

    item_def* item(const map_cell& cell);
    item_def* item(coord_def pos)
    {
        return item(m_cells(pos));
    }

    void set_item(map_cell& cell, const item_def& ii, bool more_items);
    void set_item(const item_def& ii, bool more_items)
    {
        set_item(m_cells(ii.pos), ii, more_items);
    }

    void set_detected_item(coord_def pos);

    void clear_item(map_cell& cell);
    void clear_item(coord_def pos)
    {
        clear_item(m_cells(pos));
    }

    monster_type monster(const map_cell& cell) const;
    monster_type monster(coord_def pos) const
    {
        return monster(m_cells(pos));
    }

    const monster_info* monsterinfo(const map_cell& cell) const;
    const monster_info* monsterinfo(coord_def pos) const
    {
        return monsterinfo(m_cells(pos));
    }

    monster_info* monsterinfo(const map_cell& cell);
    monster_info* monsterinfo(coord_def pos)
    {
        return monsterinfo(m_cells(pos));
    }

    void set_monster(map_cell& cell, const monster_info& mi);
    void set_monster(const monster_info& mi)
    {
        set_monster(m_cells(mi.pos), mi);
    }

    void set_detected_monster(coord_def pos, monster_type mons);

    void set_invisible_monster(map_cell& cell);
    void set_invisible_monster(coord_def pos)
    {
        set_invisible_monster(m_cells(pos));
    }

    void clear_monster(map_cell& cell);
    void clear_monster(coord_def pos)
    {
        clear_monster(m_cells(pos));
    }

    cloud_type cloud(const map_cell& cell) const;
    cloud_type cloud(coord_def pos) const
    {
        return cloud(m_cells(pos));
    }

    // TODO: should this be colour_t?
    unsigned cloud_colour(const map_cell& cell) const;
    // TODO: should this be colour_t?
    unsigned cloud_colour(coord_def pos) const
    {
        return cloud_colour(m_cells(pos));
    }

    const cloud_info* cloudinfo(const map_cell& cell) const;
    const cloud_info* cloudinfo(coord_def pos) const
    {
        return cloudinfo(m_cells(pos));
    }

    cloud_info* cloudinfo(const map_cell& cell);
    cloud_info* cloudinfo(coord_def pos)
    {
        return cloudinfo(m_cells(pos));
    }

    void set_cloud(map_cell& cell, const cloud_info& ci);
    void set_cloud(const cloud_info& ci)
    {
        set_cloud(m_cells(ci.pos), ci);
    }

    void clear_cloud(map_cell& cell);
    void clear_cloud(coord_def pos)
    {
        clear_cloud(m_cells(pos));
    }

    bool update_cloud_state(map_cell& cell);
    bool update_cloud_state(coord_def pos)
    {
        return update_cloud_state(m_cells(pos));
    }

    void clear(map_cell& cell);
    void clear(coord_def pos)
    {
        clear(m_cells(pos));
    }

    // Clear prior to show update. Need to retain at least "seen" flag.
    void clear_data(map_cell& cell);
    void clear_data(coord_def pos)
    {
        clear_data(m_cells(pos));
    }

    void copy_at(coord_def pos, const map_knowledge_type& other,
                 coord_def other_pos);

    map_feature get_map_feature(const map_cell& cell) const;

    map_cell& operator()(coord_def pos)
    {
        return m_cells(pos);
    }

    const map_cell& operator()(coord_def pos) const
    {
        return m_cells(pos);
    }

    int size() const
    {
        return m_cells.size();
    }

    void reset();
private:
    void reset_for_overwrite();
    void copy_from(const map_knowledge_type& other);

    FixedArray< map_cell, GXM, GYM > m_cells;
    vector<cloud_info> m_clouds;
    vector<item_def> m_items;
    vector<monster_info> m_monsters;
    uint16_t free_cloud_index = UINT16_MAX;
    uint16_t free_item_index = UINT16_MAX;
    uint16_t free_mons_index = UINT16_MAX;
};

void set_terrain_mapped(const coord_def c);
void set_terrain_seen(const coord_def c);

void set_terrain_visible(const coord_def c);
void clear_terrain_visibility();

int count_detected_mons();

void update_cloud_knowledge();

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
bool is_explore_horizon(const coord_def& c);

void reautomap_level();

/**
 * @brief Get the bounding box of the known map.
 *
 * @return pair of {topleft coord, bottomright coord} of bbox.
 */
std::pair<coord_def, coord_def> known_map_bounds();
bool in_known_map_bounds(const coord_def& p);
