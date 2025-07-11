#pragma once

#include <cstdint>
#include <vector>

#include "cloud-info.h"
#include "coord-def.h"
#include "defines.h"
#include "fixedarray.h"
#include "item-def.h"
#include "mon-info.h"
#include "trap-type.h"

#define MAP_MAGIC_MAPPED_FLAG   0x01
#define MAP_SEEN_FLAG           0x02
#define MAP_CHANGED_FLAG        0x04 // FIXME: this doesn't belong here
#define MAP_DETECTED_MONSTER    0x08
#define MAP_INVISIBLE_MONSTER   0x10
#define MAP_DETECTED_ITEM       0x20
#define MAP_VISIBLE_FLAG        0x40
#define MAP_GRID_KNOWN          0xFF

#define MAP_EMPHASIZE          0x100
#define MAP_MORE_ITEMS         0x200
#define MAP_HALOED             0x400
#define MAP_SILENCED           0x800
#define MAP_BLOODY            0x1000
#define MAP_CORRODING         0x2000
#define MAP_INVISIBLE_UPDATE  0x4000 // Used for invis redraws by show_init()
#define MAP_ICY               0x8000

/* these flags require more space to serialize: put infrequently used ones there */
#define MAP_EXCLUDED_STAIRS  0x10000
#define MAP_SANCTUARY_1      0x80000
#define MAP_SANCTUARY_2     0x100000
#define MAP_WITHHELD        0x200000
#define MAP_LIQUEFIED       0x400000
#define MAP_ORB_HALOED      0x800000
#define MAP_UMBRAED        0x1000000
#define MAP_QUAD_HALOED    0X4000000
#define MAP_DISJUNCT       0X8000000
#define MAP_BLASPHEMY     0X10000000
#define MAP_BFB_CORPSE    0X20000000

class MapKnowledge
{
public:
    MapKnowledge();
    MapKnowledge(const MapKnowledge&);
    MapKnowledge(MapKnowledge&&) noexcept = default;

    MapKnowledge& operator=(const MapKnowledge&);
    MapKnowledge& operator=(MapKnowledge&&) noexcept = default;

    const item_def* item(coord_def pos) const;
    item_def* item(coord_def pos);
    bool detected_item(coord_def pos) const;
    void set_item(coord_def pos, const item_def& ii, bool more_items);
    void set_detected_item(coord_def pos);
    void clear_item(coord_def pos);

    monster_type monster(coord_def pos) const;
    const monster_info* monsterinfo(coord_def pos) const;
    monster_info* monsterinfo(coord_def pos);
    bool detected_monster(coord_def pos) const;
    bool invisible_monster(coord_def pos) const;
    void set_monster(coord_def pos, const monster_info& mi);
    void set_detected_monster(coord_def pos, monster_type mons);
    void set_invisible_monster(coord_def pos);
    void clear_monster(coord_def pos);

    cloud_type cloud(coord_def pos) const;
    // TODO: should this be colour_t?
    unsigned cloud_colour(coord_def pos) const;
    const cloud_info* cloudinfo(coord_def pos) const;
    cloud_info* cloudinfo(coord_def pos);
    void set_cloud(coord_def pos, const cloud_info& ci);
    void clear_cloud(coord_def pos);

    uint32_t flags(coord_def pos) const;
    uint32_t& flags(coord_def pos);

    bool changed(coord_def pos) const;
    bool known(coord_def pos) const;
    bool visible(coord_def pos) const;
    bool seen(coord_def pos) const;
    bool mapped(coord_def pos) const;

    dungeon_feature_type feat(coord_def pos) const;
    unsigned feat_colour(coord_def pos) const;
    void set_feature(coord_def pos, dungeon_feature_type nfeat,
                     unsigned colour = 0, trap_type tr = TRAP_UNASSIGNED);

    trap_type trap(coord_def pos) const;

    bool update_cloud_state(coord_def pos);

    void clear(coord_def pos);

    // Clear prior to show update. Need to retain at least "seen" flag.
    void clear_data(coord_def pos);

    void copy_at(coord_def pos, const MapKnowledge& other,
                 coord_def other_pos);

    map_feature get_map_feature(coord_def pos) const;

    int size() const
    {
        return GXM * GYM;
    }

    void reset();
private:
    void init_grids();
    void reset_for_overwrite();
    void copy_from(const MapKnowledge& other);

    void init_cloud(coord_def pos, const cloud_info& ci);
    void init_item(coord_def pos, const item_def& ii);
    void init_monster(coord_def pos, const monster_info& mi);

    FixedArray< uint32_t, GXM, GYM > m_flags_grid;
    FixedArray< uint16_t, GXM, GYM > m_clouds_grid;
    FixedArray< uint16_t, GXM, GYM > m_items_grid;
    FixedArray< uint16_t, GXM, GYM > m_mons_grid;
    COMPILE_CHECK(NUM_FEATURES <= 256);
    FixedArray< uint8_t, GXM, GYM > m_feats_grid;
    FixedArray< colour_t, GXM, GYM > m_feat_colours_grid;
    COMPILE_CHECK(NUM_TRAPS <= 256);
    FixedArray< uint8_t, GXM, GYM > m_traps_grid;

    vector<cloud_info> m_clouds;
    vector<item_def> m_items;
    vector<monster_info> m_monsters;
    uint16_t m_free_cloud_index = UINT16_MAX;
    uint16_t m_free_item_index = UINT16_MAX;
    uint16_t m_free_mons_index = UINT16_MAX;
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
