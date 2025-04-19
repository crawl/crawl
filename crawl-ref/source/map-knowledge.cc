#include "AppHdr.h"

#include "map-knowledge.h"

#include "cloud.h"
#include "coordit.h"
#include "directn.h"
#include "env.h"
#include "level-state-type.h"
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

MapKnowledge::MapKnowledge(const MapKnowledge& other)
{
    copy_from(other);
}

MapKnowledge& MapKnowledge::operator=(
                                               const MapKnowledge& other)
{
    if (this == &other)
        return *this;
    reset_for_overwrite();
    copy_from(other);
    return *this;
}

const item_def* MapKnowledge::item(const map_cell& cell) const
{
    uint16_t index = cell._item_index;
    if (index < m_items.size())
        return &m_items[index];
    return nullptr;
}

item_def* MapKnowledge::item(const map_cell& cell)
{
    uint16_t index = cell._item_index;
    if (index < m_items.size())
        return &m_items[index];
    return nullptr;
}

void MapKnowledge::set_item(map_cell& cell, const item_def& ii,
                                  bool more_items)
{
    clear_item(cell);

    if (free_item_index < m_items.size())
    {
        uint16_t result_item = free_item_index;
        free_item_index = m_items[result_item].next_free;
        m_items[result_item] = ii;
        cell._item_index = result_item;
    }
    else
    {
        cell._item_index = (uint16_t)m_items.size();
        m_items.push_back(ii);
    }

    if (more_items)
        cell.flags |= MAP_MORE_ITEMS;
}

void MapKnowledge::set_detected_item(coord_def pos)
{
    map_cell& cell = m_cells(pos);
    clear_item(cell);
    cell.flags |= MAP_DETECTED_ITEM;

    uint16_t result_item;
    if (free_item_index < m_items.size())
    {
        result_item = free_item_index;
        free_item_index = m_items[result_item].next_free;
    }
    else
    {
        result_item = (uint16_t)m_items.size();
        m_items.emplace_back();
    }
    cell._item_index = result_item;

    item_def& item = m_items[result_item];
    item.base_type = OBJ_DETECTED;
    item.rnd = 1;
    item.pos = pos;
}

void MapKnowledge::clear_item(map_cell& cell)
{
    uint16_t index = cell._item_index;
    if (index < m_items.size())
    {
        m_items[index].next_free = free_item_index;
        free_item_index = index;

        cell._item_index = UINT16_MAX;
        cell.flags &= ~(MAP_DETECTED_ITEM | MAP_MORE_ITEMS);
    }
}

monster_type MapKnowledge::monster(const map_cell& cell) const
{
    uint16_t index = cell._mons_index;
    if (index < m_monsters.size())
        return m_monsters[index].type;
    return MONS_NO_MONSTER;
}

const monster_info* MapKnowledge::monsterinfo(const map_cell& cell) const
{
    uint16_t index = cell._mons_index;
    if (index < m_monsters.size())
        return &m_monsters[index];
    return nullptr;
}

monster_info* MapKnowledge::monsterinfo(const map_cell& cell)
{
    uint16_t index = cell._mons_index;
    if (index < m_monsters.size())
        return &m_monsters[index];
    return nullptr;
}

void MapKnowledge::set_monster(map_cell& cell, const monster_info& mi)
{
    clear_monster(cell);
    if (free_mons_index < m_monsters.size())
    {
        uint16_t result_mons = free_mons_index;
        free_mons_index = m_monsters[result_mons].next_free;
        m_monsters[result_mons] = mi;
        cell._mons_index = result_mons;
        return;
    }
    cell._mons_index = m_monsters.size();
    m_monsters.push_back(mi);
}

void MapKnowledge::set_detected_monster(coord_def pos, monster_type mons)
{
    map_cell& cell = m_cells(pos);
    clear_monster(cell);

    uint16_t result_mons;
    if (free_mons_index < m_monsters.size())
    {
        result_mons = free_mons_index;
        free_mons_index = m_monsters[result_mons].next_free;
        m_monsters[result_mons].~monster_info();
        new (&m_monsters[result_mons]) monster_info(MONS_SENSED);
    }
    else
    {
        result_mons = m_monsters.size();
        m_monsters.emplace_back(MONS_SENSED);
    }
    cell._mons_index = result_mons;

    monster_info& new_mons = m_monsters[result_mons];
    new_mons.base_type = mons;
    new_mons.pos = pos;
    cell.flags |= MAP_DETECTED_MONSTER;
}

void MapKnowledge::set_invisible_monster(map_cell& cell)
{
    clear_monster(cell);
    cell.flags |= MAP_INVISIBLE_MONSTER;
}

void MapKnowledge::clear_monster(map_cell& cell)
{
    uint16_t index = cell._mons_index;
    if (index < m_monsters.size())
    {
        m_monsters[index].next_free = free_mons_index;
        free_mons_index = index;

        cell._mons_index = UINT16_MAX;
        cell.flags &= ~(MAP_DETECTED_MONSTER | MAP_INVISIBLE_MONSTER);
    }
}

cloud_type MapKnowledge::cloud(const map_cell& cell) const
{
    uint16_t index = cell._cloud_index;
    if (index < m_clouds.size())
        return m_clouds[index].type;
    return CLOUD_NONE;
}

// TODO: should this be colour_t?
unsigned MapKnowledge::cloud_colour(const map_cell& cell) const
{
    uint16_t index = cell._cloud_index;
    if (index < m_clouds.size())
        return m_clouds[index].colour;
    return static_cast<colour_t>(0);
}

const cloud_info* MapKnowledge::cloudinfo(const map_cell& cell) const
{
    uint16_t index = cell._cloud_index;
    if (index < m_clouds.size())
        return &m_clouds[index];
    return nullptr;
}

cloud_info* MapKnowledge::cloudinfo(const map_cell& cell)
{
    uint16_t index = cell._cloud_index;
    if (index < m_clouds.size())
        return &m_clouds[index];
    return nullptr;
}

void MapKnowledge::set_cloud(map_cell& cell, const cloud_info& ci)
{
    clear_cloud(cell);
    if (free_cloud_index < m_clouds.size())
    {
        uint16_t result_cloud = free_cloud_index;
        free_cloud_index = m_clouds[result_cloud].next_free;
        m_clouds[result_cloud] = ci;
        cell._cloud_index = result_cloud;
        return;
    }
    cell._cloud_index = m_clouds.size();
    m_clouds.push_back(ci);
}

void MapKnowledge::clear_cloud(map_cell& cell)
{
    uint16_t index = cell._cloud_index;
    if (index < m_clouds.size())
    {
        m_clouds[index].next_free = free_cloud_index;
        free_cloud_index = index;

        cell._cloud_index = UINT16_MAX;
    }
}

void MapKnowledge::clear(map_cell& cell)
{
    clear_cloud(cell);
    clear_item(cell);
    clear_monster(cell);
    cell = map_cell();
}

// Clear prior to show update. Need to retain at least "seen" flag.
void MapKnowledge::clear_data(map_cell& cell)
{
    constexpr uint32_t kept_flags = MAP_SEEN_FLAG | MAP_CHANGED_FLAG
                                    | MAP_INVISIBLE_UPDATE;
    const uint32_t f = cell.flags & kept_flags;
    clear(cell);
    cell.flags = f;
}

void MapKnowledge::copy_at(coord_def pos,
                                 const MapKnowledge& other,
                                 coord_def other_pos)
{
    map_cell& cell = m_cells(pos);
    const map_cell& other_cell = other(other_pos);

    clear_item(cell);
    clear_monster(cell);

    const cloud_info* cloud = other.cloudinfo(other_cell);
    if (cloud)
        set_cloud(cell, *cloud);
    else
        clear_cloud(cell);

    const item_def* item = other.item(other_cell);
    if (item)
        set_item(cell, *item, false);
    else
        clear_item(cell);

    const monster_info* mons = other.monsterinfo(other_cell);
    if (mons)
        set_monster(cell, *mons);
    else
        clear_monster(cell);

    cell.flags = other_cell.flags;
    cell._feat = other_cell._feat;
    cell._feat_colour = other_cell._feat_colour;
    cell._trap = other_cell._trap;
}

void MapKnowledge::reset()
{
    reset_for_overwrite();
    m_cells.init(map_cell());
}

void MapKnowledge::reset_for_overwrite()
{
    m_clouds.clear();
    m_items.clear();
    m_monsters.clear();
    free_cloud_index = UINT16_MAX;
    free_item_index = UINT16_MAX;
    free_mons_index = UINT16_MAX;
}

void MapKnowledge::copy_from(const MapKnowledge& other)
{
    for (int y = 0; y < GYM; ++y)
    {
        for (int x = 0; x < GXM; ++x)
        {
            coord_def gc{ x, y };
            map_cell& cell = m_cells(gc);
            const map_cell& other_cell = other.m_cells(gc);
            cell = other_cell;
            const cloud_info* cloud = other.cloudinfo(other_cell);
            if (cloud)
            {
                cell._cloud_index = m_clouds.size();
                m_clouds.push_back(*cloud);
            }
            const item_def* item = other.item(other_cell);
            if (item)
            {
                cell._item_index = m_items.size();
                m_items.push_back(*item);
            }
            const monster_info* mons = other.monsterinfo(other_cell);
            if (mons)
            {
                cell._mons_index = m_monsters.size();
                m_monsters.push_back(*mons);
            }
        }
    }
}

void set_terrain_mapped(const coord_def gc)
{
    map_cell* cell = &env.map_knowledge(gc);
    cell->flags &= (~MAP_CHANGED_FLAG);
    cell->flags |= MAP_MAGIC_MAPPED_FLAG;
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
        if (env.map_knowledge(*ri).changed())
            continue;

        if (env.map_knowledge(*ri).detected_monster())
            count++;
    }

    return count;
}

void clear_map(bool clear_items, bool clear_mons)
{
    for (rectangle_iterator ri(BOUNDARY_BORDER - 1); ri; ++ri)
    {
        const coord_def p = *ri;
        map_cell& cell = env.map_knowledge(p);
        if (!cell.known() || cell.visible())
            continue;

        env.map_knowledge.clear_cloud(cell);

        if (clear_items)
            env.map_knowledge.clear_item(cell);

        if (clear_mons
            && !mons_class_is_stationary(env.map_knowledge.monster(cell)))
        {
            env.map_knowledge.clear_monster(cell);
        }

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
    return you.get_mutation_level(MUT_PASSIVE_MAPPING);
}

void reautomap_level()
{
    int passive = _map_quality();

    for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
        for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
            if (env.map_knowledge(coord_def(x, y)).flags & MAP_SEEN_FLAG)
                _automap_from(x, y, passive);
}

void set_terrain_seen(const coord_def pos)
{
    const dungeon_feature_type feat = env.grid(pos);
    map_cell* cell = &env.map_knowledge(pos);

    // First time we've seen a notable feature.
    if (!(cell->flags & MAP_SEEN_FLAG))
    {
        _automap_from(pos.x, pos.y, _map_quality());

        if (!is_boring_terrain(feat)
            && (!env.map_forgotten
                || ~(*env.map_forgotten)(pos).flags & MAP_SEEN_FLAG))
        {
            string desc = feature_description_at(pos, false, DESC_A) + ".";
            take_note(Note(NOTE_SEEN_FEAT, 0, 0, desc));
        }
    }

    cell->flags &= (~MAP_CHANGED_FLAG);
    cell->flags |= MAP_SEEN_FLAG;

#ifdef USE_TILE
    // This may have changed the explore horizon, so update adjacent minimap
    // squares as well.
    for (adjacent_iterator ai(pos, false); ai; ++ai)
        tiles.update_minimap(*ai);
#endif
}

void set_terrain_visible(const coord_def c)
{
    map_cell* cell = &env.map_knowledge(c);
    set_terrain_seen(c);
    if (!(cell->flags & MAP_VISIBLE_FLAG))
    {
        cell->flags |= MAP_VISIBLE_FLAG;
        env.visible.insert(c);
    }
    cell->flags &= ~(MAP_DETECTED_MONSTER | MAP_DETECTED_ITEM);
}

void clear_terrain_visibility()
{
    for (auto c : env.visible)
        env.map_knowledge(c).flags &= ~MAP_VISIBLE_FLAG;
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

    if (env.map_knowledge(c).feat() != DNGN_UNSEEN)
        return false;

    // Note: c might be on map edge, walkable squares not really.
    for (adjacent_iterator ai(c); ai; ++ai)
        if (in_bounds(*ai))
        {
            const auto& cell = env.map_knowledge(*ai);
            dungeon_feature_type feat = cell.feat();
            if (feat != DNGN_UNSEEN
                && (!feat_is_solid(feat) || feat_is_door(feat))
                && !(cell.flags & MAP_MAGIC_MAPPED_FLAG))
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

    const map_cell& cell = env.map_knowledge(gc);
    return env.map_knowledge.get_map_feature(cell);
}

map_feature MapKnowledge::get_map_feature(const map_cell& cell) const
{
    // known but not seen monster
    if (cell.detected_monster())
        return MF_MONS_HOSTILE; // hostile by default

    // known but not seen item
    if (cell.detected_item())
        return MF_ITEM;

    const map_feature base_feature = get_feature_def(cell.feat()).minimap;

    // handle magic mapping etc effects (known but not seen)
    if ((base_feature == MF_WALL || base_feature == MF_FLOOR)
        && cell.known() && !cell.seen())
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
    if (cloud(cell))
    {
        show_type show;
        show.cls = SH_CLOUD;
        const map_feature cloud_feature = get_feature_def(show).minimap;
        if (cloud_feature != MF_SKIP) // XXX: does this ever happen?
            return cloud_feature;
    }

    // then items...
    if (item(cell))
    {
        const map_feature item_feature = get_feature_def(*item(cell)).minimap;
        if (item_feature != MF_SKIP) // can this happen?
            return item_feature;
    }

    // then firewood.
    if (monster(cell) != MONS_NO_MONSTER
        && mons_class_is_firewood(monster(cell)))
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
bool MapKnowledge::update_cloud_state(map_cell& cell)
{
    if (cell.visible())
        return false; // we're already up-to-date

    if (cell._cloud_index >= m_clouds.size())
        return false;
    cloud_info& cloud = m_clouds[cell._cloud_index];

    // player non-opaque clouds vanish instantly out of los
    if (cloud.killer == KILL_YOU_MISSILE
        && !is_opaque_cloud(cloud.type))
    {
        clear_cloud(cell);
        return true;
    }

    // still winds KOs all clouds, even those out of LOS
    if (env.level_state & LSTATE_STILL_WINDS)
    {
        clear_cloud(cell);
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
            if (env.map_knowledge(coord_def(i, j)).known())
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
