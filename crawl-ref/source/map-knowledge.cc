#include "AppHdr.h"

#include "map-knowledge.h"

#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "directn.h"
#include "env.h"
#include "level-state-type.h"
#include "map-feature.h"
#include "notes.h"
#include "religion.h"
#include "terrain.h"
#ifdef USE_TILE
 #include "tilepick.h"
 #include "tileview.h"
#endif
#include "tiles-build-specific.h"
#include "travel.h"
#include "view.h"

MapKnowledge::MapKnowledge()
{
    init_grids();
}

MapKnowledge::MapKnowledge(const MapKnowledge& other)
{
    copy_from(other);
}

MapKnowledge& MapKnowledge::operator=(const MapKnowledge& other)
{
    if (this == &other)
        return *this;
    reset_for_overwrite();
    copy_from(other);
    return *this;
}

const item_def* MapKnowledge::item(coord_def pos) const
{
    uint16_t index = m_items_grid(pos);
    if (index < m_items.size())
        return &m_items[index];
    return nullptr;
}

item_def* MapKnowledge::item(coord_def pos)
{
    uint16_t index = m_items_grid(pos);
    if (index < m_items.size())
        return &m_items[index];
    return nullptr;
}

bool MapKnowledge::detected_item(coord_def pos) const
{
    const bool ret = !!(m_flags_grid(pos) & MAP_DETECTED_ITEM);
    // TODO: change to an ASSERT when the underlying crash goes away
    if (ret && m_items_grid(pos) == UINT16_MAX)
    {
        //clear_item();
        return false;
    }
    return ret;
}

void MapKnowledge::set_item(coord_def pos, const item_def& ii, bool more_items)
{
    clear_item(pos);
    init_item(pos, ii);
    if (more_items)
        m_flags_grid(pos) |= MAP_MORE_ITEMS;
}

void MapKnowledge::set_detected_item(coord_def pos)
{
    clear_item(pos);
    item_def item;
    item.base_type = OBJ_DETECTED;
    item.rnd = 1;
    init_item(pos, item);
    m_flags_grid(pos) |= MAP_DETECTED_ITEM;
}

void MapKnowledge::clear_item(coord_def pos)
{
    uint16_t index = m_items_grid(pos);
    if (index != UINT16_MAX)
    {
        m_items[index].next_free = m_free_item_index;
        m_free_item_index = index;

        m_items_grid(pos) = UINT16_MAX;
        m_flags_grid(pos) &= ~(MAP_DETECTED_ITEM | MAP_MORE_ITEMS);
    }
}

monster_type MapKnowledge::monster(coord_def pos) const
{
    uint16_t index = m_mons_grid(pos);
    if (index < m_monsters.size())
        return m_monsters[index].type;
    return MONS_NO_MONSTER;
}

const monster_info* MapKnowledge::monsterinfo(coord_def pos) const
{
    uint16_t index = m_mons_grid(pos);
    if (index < m_monsters.size())
        return &m_monsters[index];
    return nullptr;
}

monster_info* MapKnowledge::monsterinfo(coord_def pos)
{
    uint16_t index = m_mons_grid(pos);
    if (index < m_monsters.size())
        return &m_monsters[index];
    return nullptr;
}

bool MapKnowledge::detected_monster(coord_def pos) const
{
    return !!(m_flags_grid(pos) & MAP_DETECTED_MONSTER);
}

bool MapKnowledge::invisible_monster(coord_def pos) const
{
    return !!(m_flags_grid(pos) & MAP_INVISIBLE_MONSTER);
}

void MapKnowledge::set_monster(coord_def pos, const monster_info& mi)
{
    clear_monster(pos);
    init_monster(pos, mi);
}

void MapKnowledge::set_detected_monster(coord_def pos, monster_type mons)
{
    clear_monster(pos);
    monster_info new_mons(MONS_SENSED);
    new_mons.base_type = mons;
    init_monster(pos, new_mons);
    m_flags_grid(pos) |= MAP_DETECTED_MONSTER;
}

void MapKnowledge::set_invisible_monster(coord_def pos)
{
    clear_monster(pos);
    m_flags_grid(pos) |= MAP_INVISIBLE_MONSTER;
}

void MapKnowledge::clear_monster(coord_def pos)
{
    uint16_t index = m_mons_grid(pos);
    if (index < m_monsters.size())
    {
        m_monsters[index].next_free = m_free_mons_index;
        m_free_mons_index = index;

        m_mons_grid(pos) = UINT16_MAX;
        m_flags_grid(pos) &= ~(MAP_DETECTED_MONSTER | MAP_INVISIBLE_MONSTER);
    }
}

cloud_type MapKnowledge::cloud(coord_def pos) const
{
    uint16_t index = m_clouds_grid(pos);
    if (index < m_clouds.size())
        return m_clouds[index].type;
    return CLOUD_NONE;
}

// TODO: should this be colour_t?
unsigned MapKnowledge::cloud_colour(coord_def pos) const
{
    uint16_t index = m_clouds_grid(pos);
    if (index < m_clouds.size())
        return m_clouds[index].colour;
    return static_cast<colour_t>(0);
}

const cloud_info* MapKnowledge::cloudinfo(coord_def pos) const
{
    uint16_t index = m_clouds_grid(pos);
    if (index < m_clouds.size())
        return &m_clouds[index];
    return nullptr;
}

cloud_info* MapKnowledge::cloudinfo(coord_def pos)
{
    uint16_t index = m_clouds_grid(pos);
    if (index < m_clouds.size())
        return &m_clouds[index];
    return nullptr;
}

void MapKnowledge::set_cloud(coord_def pos, const cloud_info& ci)
{
    clear_cloud(pos);
    init_cloud(pos, ci);
}

void MapKnowledge::clear_cloud(coord_def pos)
{
    uint16_t index = m_clouds_grid(pos);
    if (index != UINT16_MAX)
    {
        m_clouds[index].next_free = m_free_cloud_index;
        m_free_cloud_index = index;

        m_clouds_grid(pos) = UINT16_MAX;
    }
}

uint32_t MapKnowledge::flags(coord_def pos) const
{
    return m_flags_grid(pos);
}

uint32_t& MapKnowledge::flags(coord_def pos)
{
    return m_flags_grid(pos);
}

bool MapKnowledge::changed(coord_def pos) const
{
    return !!(m_flags_grid(pos) & MAP_CHANGED_FLAG);
}

bool MapKnowledge::known(coord_def pos) const
{
    return !!(m_flags_grid(pos) & MAP_GRID_KNOWN);
}

bool MapKnowledge::visible(coord_def pos) const
{
    return !!(m_flags_grid(pos) & MAP_VISIBLE_FLAG);
}

bool MapKnowledge::seen(coord_def pos) const
{
    return !!(m_flags_grid(pos) & MAP_SEEN_FLAG);
}

bool MapKnowledge::mapped(coord_def pos) const
{
    return !!(m_flags_grid(pos) & MAP_MAGIC_MAPPED_FLAG);
}

dungeon_feature_type MapKnowledge::feat(coord_def pos) const
{
    return static_cast<dungeon_feature_type>(m_feats_grid(pos));
}

unsigned MapKnowledge::feat_colour(coord_def pos) const
{
    return m_feat_colours_grid(pos);
}

void MapKnowledge::set_feature(coord_def pos, dungeon_feature_type nfeat,
                               unsigned colour, trap_type tr)
{
    m_feats_grid(pos) = static_cast<uint8_t>(nfeat);
    m_feat_colours_grid(pos) = colour;
    m_traps_grid(pos) = static_cast<uint8_t>(tr);
}

trap_type MapKnowledge::trap(coord_def pos) const
{
    return static_cast<trap_type>(m_traps_grid(pos));
}

void MapKnowledge::clear(coord_def pos)
{
    clear_cloud(pos);
    clear_item(pos);
    clear_monster(pos);
    m_flags_grid(pos) = 0;
    m_feats_grid(pos) = (uint8_t)DNGN_UNSEEN;
    m_feat_colours_grid(pos) = 0;
    m_traps_grid(pos) = (uint8_t)TRAP_UNASSIGNED;
}

// Clear prior to show update. Need to retain at least "seen" flag.
void MapKnowledge::clear_data(coord_def pos)
{
    constexpr uint32_t kept_flags = MAP_SEEN_FLAG | MAP_CHANGED_FLAG
                                    | MAP_INVISIBLE_UPDATE;
    const uint32_t f = m_flags_grid(pos) & kept_flags;
    clear(pos);
    m_flags_grid(pos) = f;
}

void MapKnowledge::copy_at(coord_def pos,
                           const MapKnowledge& other,
                           coord_def other_pos)
{
    if (this == &other && pos == other_pos)
        return;

    const cloud_info* cloud = other.cloudinfo(other_pos);
    if (cloud)
        set_cloud(pos, *cloud);
    else
        clear_cloud(pos);

    const item_def* item = other.item(other_pos);
    if (item)
        set_item(pos, *item, false);
    else
        clear_item(pos);

    const monster_info* mons = other.monsterinfo(other_pos);
    if (mons)
        set_monster(pos, *mons);
    else
        clear_monster(pos);

    m_flags_grid(pos) = other.m_flags_grid(other_pos);
    m_feats_grid(pos) = other.m_feats_grid(other_pos);
    m_feat_colours_grid(pos) = other.m_feat_colours_grid(other_pos);
    m_traps_grid(pos) = other.m_traps_grid(other_pos);
}

void MapKnowledge::reset()
{
    reset_for_overwrite();
    init_grids();
}

void MapKnowledge::init_grids()
{
    m_flags_grid.init(0);
    m_clouds_grid.init(UINT16_MAX);
    m_items_grid.init(UINT16_MAX);
    m_mons_grid.init(UINT16_MAX);
    m_feats_grid.init((uint8_t)DNGN_UNSEEN);
    m_feat_colours_grid.init(0);
    m_traps_grid.init((uint8_t)TRAP_UNASSIGNED);
}

void MapKnowledge::reset_for_overwrite()
{
    m_clouds.clear();
    m_items.clear();
    m_monsters.clear();
    m_free_cloud_index = UINT16_MAX;
    m_free_item_index = UINT16_MAX;
    m_free_mons_index = UINT16_MAX;
}

void MapKnowledge::copy_from(const MapKnowledge& other)
{
    ASSERT(this != &other);

    m_clouds_grid = other.m_clouds_grid;
    m_items_grid = other.m_items_grid;
    m_mons_grid = other.m_mons_grid;
    for (int y = 0; y < GYM; ++y)
    {
        for (int x = 0; x < GXM; ++x)
        {
            coord_def gc{ x, y };

            uint16_t other_cloud_index = other.m_clouds_grid(gc);
            if (other_cloud_index != UINT16_MAX)
                init_cloud(gc, other.m_clouds[other_cloud_index]);

            uint16_t other_item_index = other.m_items_grid(gc);
            if (other_item_index != UINT16_MAX)
                init_item(gc, other.m_items[other_item_index]);

            uint16_t other_mons_index = other.m_mons_grid(gc);
            if (other_mons_index != UINT16_MAX)
                init_monster(gc, other.m_monsters[other_mons_index]);
        }
    }

    m_flags_grid = other.m_flags_grid;
    m_feats_grid = other.m_feats_grid;
    m_feat_colours_grid = other.m_feat_colours_grid;
    m_traps_grid = other.m_traps_grid;
}

void MapKnowledge::init_cloud(coord_def pos, const cloud_info& ci)
{
    uint16_t result_cloud;
    if (m_free_cloud_index < m_clouds.size())
    {
        result_cloud = m_free_cloud_index;
        m_free_cloud_index = m_clouds[result_cloud].next_free;
        m_clouds[result_cloud] = ci;
    }
    else
    {
        result_cloud = (uint16_t)m_clouds.size();
        m_clouds.push_back(ci);
    }
    m_clouds_grid(pos) = result_cloud;
    m_clouds[result_cloud].pos = pos;
}

void MapKnowledge::init_item(coord_def pos, const item_def& ii)
{
    uint16_t result_item;
    if (m_free_item_index < m_items.size())
    {
        result_item = m_free_item_index;
        m_free_item_index = m_items[result_item].next_free;
        m_items[result_item] = ii;
    }
    else
    {
        result_item = (uint16_t)m_items.size();
        m_items.push_back(ii);
    }
    m_items_grid(pos) = result_item;
    m_items[result_item].pos = pos;
}

void MapKnowledge::init_monster(coord_def pos, const monster_info& mi)
{
    uint16_t result_mons;
    if (m_free_mons_index < m_monsters.size())
    {
        result_mons = m_free_mons_index;
        m_free_mons_index = m_monsters[result_mons].next_free;
        m_monsters[result_mons] = mi;
    }
    else
    {
        result_mons = (uint16_t)m_monsters.size();
        m_monsters.push_back(mi);
    }
    m_mons_grid(pos) = result_mons;
    m_monsters[result_mons].pos = pos;
}

void set_terrain_mapped(const coord_def gc)
{
    uint32_t& flags = env.map_knowledge.flags(gc);
    flags &= (~MAP_CHANGED_FLAG);
    flags |= MAP_MAGIC_MAPPED_FLAG;
#ifdef USE_TILE
    // This may have changed the explore horizon, so update adjacent minimap
    // squares as well.
    for (adjacent_iterator ai(gc, false); ai; ++ai)
        tiles.update_minimap(*ai);
#endif
}

int count_detected_mons()
{
    int count = 0;
    for (rectangle_iterator ri(BOUNDARY_BORDER - 1); ri; ++ri)
    {
        // Don't expose new dug out areas:
        // Note: assumptions are being made here about how
        // terrain can change (eg it used to be solid, and
        // thus monster/item free).
        if (env.map_knowledge.changed(*ri))
            continue;

        if (env.map_knowledge.detected_monster(*ri))
            count++;
    }

    return count;
}

void clear_map(bool clear_items, bool clear_mons)
{
    for (rectangle_iterator ri(BOUNDARY_BORDER - 1); ri; ++ri)
    {
        const coord_def p = *ri;
        MapKnowledge& map = env.map_knowledge;
        if (!map.known(p) || map.visible(p))
            continue;

        map.clear_cloud(p);

        if (clear_items)
            map.clear_item(p);

        if (clear_mons && !mons_class_is_stationary(map.monster(p)))
            map.clear_monster(p);

#ifdef USE_TILE
        tile_reset_fg(p);
#endif
    }
}

void clear_map_or_travel_trail()
{
    if (Options.show_travel_trail && env.travel_trail.size())
    {
        mpr("Clearing travel trail.");
        clear_travel_trail();
    }
    else
    {
        mpr("Clearing monster memory.");
        /* Items (other than corpses) cannot in general be moved or destroyed
         * once they have been seen, so there is no need to clear them.
         */
        clear_map();
        crawl_view.set_player_at(you.pos());
    }
}

static void _automap_from(int x, int y, int level)
{
    if (level)
    {
        magic_mapping(LOS_MAX_RANGE * level, 25,
                      true, false, true, false, true,
                      coord_def(x,y));
    }
}

static int _map_quality()
{
    return you.get_mutation_level(MUT_PASSIVE_MAPPING) * 2;
}

void reautomap_level()
{
    int passive = _map_quality();

    for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
        for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
            if (env.map_knowledge.flags(coord_def(x, y)) & MAP_SEEN_FLAG)
                _automap_from(x, y, passive);
}

void set_terrain_seen(const coord_def pos)
{
    const dungeon_feature_type feat = env.grid(pos);
    MapKnowledge& map = env.map_knowledge;

    // First time we've seen a notable feature.
    if (!(map.flags(pos) & MAP_SEEN_FLAG))
    {
        _automap_from(pos.x, pos.y, _map_quality());

        if (!is_boring_terrain(feat)
            && (!env.map_forgotten
                || ~env.map_forgotten->flags(pos) & MAP_SEEN_FLAG))
        {
            string desc = feature_description_at(pos, false, DESC_A) + ".";
            take_note(Note(NOTE_SEEN_FEAT, 0, 0, desc));
        }
    }

    map.flags(pos) &= (~MAP_CHANGED_FLAG);
    map.flags(pos) |= MAP_SEEN_FLAG;

#ifdef USE_TILE
    // This may have changed the explore horizon, so update adjacent minimap
    // squares as well.
    for (adjacent_iterator ai(pos, false); ai; ++ai)
        tiles.update_minimap(*ai);
#endif
}

void set_terrain_visible(const coord_def c)
{
    MapKnowledge& map = env.map_knowledge;
    set_terrain_seen(c);
    if (!(map.flags(c) & MAP_VISIBLE_FLAG))
    {
        map.flags(c) |= MAP_VISIBLE_FLAG;
        env.visible.insert(c);
    }
    map.flags(c) &= ~(MAP_DETECTED_MONSTER | MAP_DETECTED_ITEM);
}

void clear_terrain_visibility()
{
    for (auto c : env.visible)
        env.map_knowledge.flags(c) &= ~MAP_VISIBLE_FLAG;
    env.visible.clear();
}

static bool _floor_mf(map_feature mf)
{
    return mf == MF_FLOOR || mf == MF_WATER || mf == MF_DEEP_WATER
           || mf == MF_LAVA;
}

bool is_explore_horizon(const coord_def& c)
{
    // Not useful when there's maprot destroying your exploration anyway.
    if (player_in_branch(BRANCH_ABYSS))
        return false;

    const MapKnowledge& map = env.map_knowledge;

    if (map.feat(c) != DNGN_UNSEEN)
        return false;

    // Note: c might be on map edge, walkable squares not really.
    for (adjacent_iterator ai(c); ai; ++ai)
        if (in_bounds(*ai))
        {
            dungeon_feature_type feat = map.feat(*ai);
            if (feat != DNGN_UNSEEN
                && (!feat_is_solid(feat) || feat_is_door(feat))
                && !(map.flags(*ai) & MAP_MAGIC_MAPPED_FLAG))
            {
                return true;
            }
        }

    return false;
}

/**
 * What map feature is present in the given coordinate, for minimap purposes?
 *
 * @param gc    The grid coordinate in question.
 * @return      The most important feature in the given coordinate.
 */
map_feature get_cell_map_feature(const coord_def& gc)
{
    if (is_explore_horizon(gc))
        return MF_EXPLORE_HORIZON;

    return env.map_knowledge.get_map_feature(gc);
}

map_feature MapKnowledge::get_map_feature(coord_def pos) const
{
    // known but not seen monster
    if (detected_monster(pos))
        return MF_MONS_HOSTILE; // hostile by default

    // known but not seen item
    if (detected_item(pos))
        return MF_ITEM;

    const map_feature base_feature = get_feature_def(feat(pos)).minimap;

    // handle magic mapping etc effects (known but not seen)
    if ((base_feature == MF_WALL || base_feature == MF_FLOOR)
        && known(pos) && !seen(pos))
    {
        return (base_feature == MF_WALL) ? MF_MAP_WALL : MF_MAP_FLOOR;
    }

    // exciting features get precedence...
    if (!_floor_mf(base_feature))
        return base_feature;

    // ... but some things can take precedence over the floor

    // first clouds...
    // XXX: should items have higher priority? (pro: easier to spot un-grabbed
    // items, con: harder to spot what's blocking auto-travel)
    if (cloud(pos))
    {
        show_type show;
        show.cls = SH_CLOUD;
        const map_feature cloud_feature = get_feature_def(show).minimap;
        if (cloud_feature != MF_SKIP) // XXX: does this ever happen?
            return cloud_feature;
    }

    // then items...
    if (item(pos))
    {
        const map_feature item_feature = get_feature_def(*item(pos)).minimap;
        if (item_feature != MF_SKIP) // can this happen?
            return item_feature;
    }

    // then firewood.
    if (monster(pos) != MONS_NO_MONSTER
        && mons_class_is_firewood(monster(pos)))
    {
        return MF_MONS_NO_EXP;
    }

    if (base_feature == MF_SKIP) // can this happen?
        return MF_UNSEEN;

    return base_feature;
}

/// Iter over all known-but-unseen clouds & remove known-gone clouds.
void update_cloud_knowledge()
{
    for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
    {
        for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        {
            coord_def gc(x, y);
            if (env.map_knowledge.update_cloud_state(gc))
            {
#ifdef USE_TILE
                tile_draw_map_cell(gc, true);
#endif
#ifdef USE_TILE_WEB
                tiles.mark_for_redraw(gc);
#endif
            }
        }
    }
}

/// If there's a cloud in this cell that we know should be gone, remove it.
/// Returns true if a cloud was removed.
bool MapKnowledge::update_cloud_state(coord_def pos)
{
    if (visible(pos))
        return false; // we're already up-to-date

    const cloud_info* cloud = cloudinfo(pos);
    if (!cloud)
        return false;

    // player non-opaque clouds vanish instantly out of los
    if (cloud->killer == KILL_YOU_MISSILE
        && !is_opaque_cloud(cloud->type))
    {
        clear_cloud(pos);
        return true;
    }

    // still winds KOs all clouds, even those out of LOS
    if (env.level_state & LSTATE_STILL_WINDS)
    {
        clear_cloud(pos);
        return true;
    }

    // TODO: track decay & vanish appropriately (based on some worst case?)
    return false;
}

/**
 * Find the known map bounds as a pair of coordinates. This is a minimal
 * bounding box containing all areas of the map known by the player.
 * @return A pair of coord_defs. This is { (-1,-1), (-1,-1) } if the map is not
 *         known at all (this state shouldn't arise during normal gameplay, but
 *         may arise in weird limiting cases, like during levelgen or in
 *         tests).
 */
std::pair<coord_def, coord_def> known_map_bounds() {
    int min_x = GXM, max_x = 0, min_y = 0, max_y = 0;
    bool found_y = false;

    for (int j = 0; j < GYM; j++)
        for (int i = 0; i < GXM; i++)
        {
            if (env.map_knowledge.known(coord_def(i, j)))
            {
                if (!found_y)
                {
                    found_y = true;
                    min_y = j;
                }

                max_y = j;

                if (i < min_x)
                    min_x = i;

                if (i > max_x)
                    max_x = i;
            }
        }
    if (!found_y)
        min_x = max_x = min_y = max_y = -1;

    return std::make_pair(coord_def(min_x, min_y), coord_def(max_x, max_y));
}

/**
 * Are the given coordinates in the minimal bounding box of the known map?
 * @param p A coord_def to test.
 * @return True if p is in the bounding box, false otherwise.
 */
bool in_known_map_bounds(const coord_def& p)
{
    std::pair<coord_def, coord_def> b = known_map_bounds();
    return p.x >= b.first.x && p.y >= b.first.y
        && p.x <= b.second.x && p.y <= b.second.y;
}
