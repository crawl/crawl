#include "AppHdr.h"

#include "map-knowledge.h"

#include "cloud.h"
#include "coordit.h"
#include "directn.h"
#include "dgn-overview.h"
#include "env.h"
#include "level-state-type.h"
#include "message.h"
#include "notes.h"
#include "religion.h"
#include "stringutil.h"
#include "terrain.h"
#ifdef USE_TILE
 #include "tilepick.h"
 #include "tileview.h"
#endif
#include "tiles-build-specific.h"
#include "traps.h"
#include "travel.h"
#include "view.h"
#include "viewchar.h"
#include "viewmap.h"

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

        cell.clear_cloud();

        if (clear_items)
            cell.clear_item();

        if (clear_mons && !mons_class_is_stationary(cell.monster()))
            cell.clear_monster();

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
            if (env.map_knowledge[x][y].flags & MAP_SEEN_FLAG)
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

void map_cell::set_detected_item()
{
    clear_item();
    flags |= MAP_DETECTED_ITEM;
    _item = make_unique<item_def>();
    _item->base_type = OBJ_DETECTED;
    _item->rnd       = 1;
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

    return get_cell_map_feature(env.map_knowledge(gc));
}

map_feature get_cell_map_feature(const map_cell& cell)
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
    if (cell.cloud())
    {
        show_type show;
        show.cls = SH_CLOUD;
        const map_feature cloud_feature = get_feature_def(show).minimap;
        if (cloud_feature != MF_SKIP) // XXX: does this ever happen?
            return cloud_feature;
    }

    // then items...
    if (cell.item())
    {
        const map_feature item_feature = get_feature_def(*cell.item()).minimap;
        if (item_feature != MF_SKIP) // can this happen?
            return item_feature;
    }

    // then firewood.
    if (cell.monster() != MONS_NO_MONSTER
        && mons_class_is_firewood(cell.monster()))
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
            if (env.map_knowledge[x][y].update_cloud_state())
            {
#ifdef USE_TILE
                tile_draw_map_cell({x, y}, true);
#endif
#ifdef USE_TILE_WEB
                tiles.mark_for_redraw({x, y});
#endif
            }
        }
    }
}

/// If there's a cloud in this cell that we know should be gone, remove it.
/// Returns true if a cloud was removed.
bool map_cell::update_cloud_state()
{
    if (visible())
        return false; // we're already up-to-date

    // player non-opaque clouds vanish instantly out of los
    if (_cloud && _cloud->killer == KILL_YOU_MISSILE
        && !is_opaque_cloud(_cloud->type))
    {
        clear_cloud();
        return true;
    }

    // still winds KOs all clouds, even those out of LOS
    if (_cloud && env.level_state & LSTATE_STILL_WINDS)
    {
        clear_cloud();
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
            if (env.map_knowledge[i][j].known())
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

static colour_t _feat_default_map_colour(dungeon_feature_type feat)
{
    if (player_in_branch(BRANCH_SEWER) && feat_is_water(feat))
        return feat == DNGN_DEEP_WATER ? GREEN : LIGHTGREEN;
    return BLACK;
}

// We logically associate a difficulty parameter with each tile on each level,
// to make deterministic passive mapping work. It is deterministic so that the
// reveal order doesn't, for example, change on reload.
//
// This function returns the difficulty parameters for each tile on the current
// level, whose difficulty is less than a certain amount.
//
// Random difficulties are used in the few cases where we want repeated maps
// to give different results; scrolls and cards, since they are a finite
// resource.
static const FixedArray<uint8_t, GXM, GYM>& _tile_difficulties(bool random)
{
    // We will often be called with the same level parameter and cutoff, so
    // cache this (DS with passive mapping autoexploring could be 5000 calls
    // in a second or so).
    static FixedArray<uint8_t, GXM, GYM> cache;
    static int cache_seed = -1;

    if (random)
    {
        cache_seed = -1;
        for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
            for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
                cache[x][y] = random2(100);
        return cache;
    }

    // must not produce the magic value (-1)
    int seed = ((static_cast<int>(you.where_are_you) << 8) + you.depth)
             ^ (you.game_seed & 0x7fffffff);

    if (seed == cache_seed)
        return cache;

    cache_seed = seed;

    for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
            cache[x][y] = hash_with_seed(100, seed, y * GXM + x);

    return cache;
}

// Returns true if it succeeded.
bool magic_mapping(int map_radius, int proportion, bool suppress_msg,
                   bool force, bool deterministic, bool full_info,
                   bool range_falloff, coord_def origin, bool respect_no_automap)
{
    if (!force && !is_map_persistent())
    {
        if (!suppress_msg)
            canned_msg(MSG_DISORIENTED);

        return false;
    }

    if (map_radius < 5)
        map_radius = 5;

    // now gradually weaker with distance:
    const int pfar     = map_radius * 7 / 10;
    const int very_far = map_radius * 9 / 10;

    bool did_map = false;
    int  num_altars        = 0;
    int  num_shops_portals = 0;

    const FixedArray<uint8_t, GXM, GYM>& difficulty =
        _tile_difficulties(!deterministic);

    for (radius_iterator ri(in_bounds(origin) ? origin : you.pos(),
                            map_radius, C_SQUARE);
         ri; ++ri)
    {
        coord_def pos = *ri;

        if (respect_no_automap && env.pgrid(pos) & FPROP_NO_AUTOMAP)
            continue;

        if (range_falloff)
        {
            int threshold = proportion;

            const int dist = grid_distance(you.pos(), pos);

            if (dist > very_far)
                threshold = threshold / 3;
            else if (dist > pfar)
                threshold = threshold * 2 / 3;

            if (difficulty(pos) > threshold)
                continue;
        }

        map_cell& knowledge = env.map_knowledge(pos);

        if (knowledge.changed())
        {
            // If the player has already seen the square, update map
            // knowledge with the new terrain. Otherwise clear what we had
            // before.
            if (knowledge.seen())
            {
                dungeon_feature_type newfeat = env.grid(pos);
                trap_type tr = feat_is_trap(newfeat) ? get_trap_type(pos) : TRAP_UNASSIGNED;
                knowledge.set_feature(newfeat, env.grid_colours(pos), tr);
            }
            else
                knowledge.clear();
        }

        // Don't assume that DNGN_UNSEEN cells ever count as mapped.
        // Because of a bug at one point in map forgetting, cells could
        // spuriously get marked as mapped even when they were completely
        // unseen.
        const bool already_mapped = knowledge.mapped()
                            && knowledge.feat() != DNGN_UNSEEN;

        if (!full_info && (knowledge.seen() || already_mapped))
            continue;

        dungeon_feature_type feat = env.grid(pos);
        if (!full_info)
            feat = magic_map_base_feat(feat);

        bool open = true;

        if (feat_is_solid(feat) && !feat_is_closed_door(feat))
        {
            open = false;
            for (adjacent_iterator ai(pos); ai; ++ai)
            {
                if (map_bounds(*ai) && (!feat_is_opaque(env.grid(*ai))
                                        || feat_is_closed_door(env.grid(*ai))))
                {
                    open = true;
                    break;
                }
            }
        }

        if (open)
        {
            knowledge.set_feature(feat, _feat_default_map_colour(feat),
                feat_is_trap(env.grid(pos)) ? get_trap_type(pos) : TRAP_UNASSIGNED);
            if (is_notable_terrain(feat))
                seen_notable_thing(feat, pos);

            if (emphasise(pos))
                knowledge.flags |= MAP_EMPHASIZE;

            if (full_info)
            {
                set_terrain_seen(pos);
                StashTrack.add_stash(pos);
                show_update_at(pos);
            }
            else
            {
                set_terrain_mapped(pos);
                if (get_feature_dchar(feat) == DCHAR_ALTAR)
                    num_altars++;
                else if (get_feature_dchar(feat) == DCHAR_ARCH)
                    num_shops_portals++;
            }
#ifdef USE_TILE
            tile_draw_map_cell(pos);
#endif

            did_map = true;
        }
    }

    if (!suppress_msg)
    {
        if (did_map)
            mpr("You feel aware of your surroundings.");
        else
            canned_msg(MSG_DISORIENTED);

        vector<string> sensed;

        if (num_altars > 0)
        {
            sensed.push_back(make_stringf("%d altar%s", num_altars,
                                          num_altars > 1 ? "s" : ""));
        }

        if (num_shops_portals > 0)
        {
            const char* plur = num_shops_portals > 1 ? "s" : "";
            sensed.push_back(make_stringf("%d shop%s/portal%s",
                                          num_shops_portals, plur, plur));
        }

        if (!sensed.empty())
            mpr_comma_separated_list("You sensed ", sensed);
    }

    return did_map;
}
