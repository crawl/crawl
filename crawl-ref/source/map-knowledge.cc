#include "AppHdr.h"

#include "map-knowledge.h"

#include "cloud.h"
#include "coordit.h"
#include "directn.h"
#include "dgn-overview.h"
#include "env.h"
#include "level-state-type.h"
#include "losglobal.h"
#include "message.h"
#include "notes.h"
#include "player-notices.h"
#include "religion.h"
#include "stringutil.h"
#include "terrain.h"
#include "tile-env.h"
#ifdef USE_TILE
 #include "tilesdl.h"
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
    redraw_view_at(gc);
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

        if (clear_mons && !mons_class_is_stationary(cell.mon_type()))
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
    _item->quantity  = 1;
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
    if (cell.mon_type() != MONS_NO_MONSTER
        && mons_class_is_firewood(cell.mon_type()))
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
            if (knowledge.seen()
                || env.map_forgotten && (*env.map_forgotten)(pos).seen())
            {
                update_terrain_knowledge(pos);
                update_grid_colour_knowledge(pos);
                redraw_view_at(pos);
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

        bool open = true;

        if (feat_is_solid(env.grid(pos))
            && !feat_is_closed_door(env.grid(pos)))
        {
            open = false;
            for (adjacent_iterator ai(pos); ai; ++ai)
            {
                // This avoids revealing walls at the 'back' of the minotaur
                // areas in Gauntlets (where the wall is technically not part of
                // the subvault, but still outlines it).
                if (respect_no_automap && (env.pgrid(*ai) & FPROP_NO_AUTOMAP))
                    break;

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
            update_terrain_knowledge(pos, !full_info);
            update_grid_colour_knowledge(pos, !full_info);
            if (is_notable_terrain(knowledge.feat()))
                seen_notable_thing(knowledge.feat(), pos);

            if (emphasise(pos))
                knowledge.flags |= MAP_EMPHASIZE;

            if (full_info)
            {
                set_terrain_seen(pos);
                StashTrack.add_stash(pos);
                show_update_at(pos);
#ifdef USE_TILE
                tile_draw_map_cell(pos);
#endif
            }
            else
            {
                set_terrain_mapped(pos);
                if (get_feature_dchar(knowledge.feat()) == DCHAR_ALTAR)
                    num_altars++;
                else if (get_feature_dchar(knowledge.feat()) == DCHAR_ARCH)
                    num_shops_portals++;
            }

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

void update_terrain_knowledge(coord_def pos,
                              bool partial_knowledge_only)
{
    dungeon_feature_type feat = env.grid(pos);
    tileidx_t feat_tile = tile_env.flv(pos).feat;
    unsigned short feat_tile_idx = tile_env.flv(pos).feat_idx;
    if (partial_knowledge_only)
    {
        feat = magic_map_base_feat(feat);
        if (feat == DNGN_UNKNOWN_PORTAL || feat == DNGN_UNKNOWN_ALTAR)
        {
            feat_tile = 0;
            feat_tile_idx = 0;
        }
    }
    env.map_knowledge(pos).set_feature(feat);
    tile_env.remembered_flavour.set_feat_flavour(pos, feat_tile,
                                                 feat_tile_idx);
    unsigned short door_connect = tile_door_connect(pos);
    env.map_knowledge(pos).set_door_connect(door_connect);
}

void update_grid_colour_knowledge(coord_def pos,
                             bool partial_knowledge_only)
{
    colour_t colour = env.grid_colours(pos);
    if (partial_knowledge_only)
        colour = _feat_default_map_colour(env.map_knowledge(pos).feat());
    env.map_knowledge(pos).set_feat_colour(colour);
}

static void _clear_remembered_invis_mon(const coord_def& pos)
{
    if (in_bounds(pos))
    {
        if (env.map_knowledge(pos).old_invisible_monster())
        {
            env.map_knowledge(pos).clear_monster();
            redraw_view_at(pos);
        }
    }
}

// If the player was aware of this monster, what was the last position in which
// they were spotted?
coord_def invis_monster_knowledge::last_known_pos(const monster& mon)
{
    for (const auto &entry : data)
        if (entry.mid == mon.mid)
            return entry.last_known_pos;

    return coord_def();
}

/**
 * Update invisible knowledge for a given monster.
 * Removes them (and any corresponding map_knowledge) if they are dead or the
 * player can see them, updates their position data otherwise.
 *
 * @param mon           The monster to update.
 * @param reveal_pos    If true, this is a specific hint about an invisible
 *                      monster's current location. If false, this is a general
 *                      hint that they are somewhere nearby.
 * @param forced_pos    If specified, will set this as the monster's
 *                      last-remembered position. (Usually unnecessary, but can
 *                      ensue that monster movement always leaves a hint about
 *                      their previous position, which can rarely get out of
 *                      sync when using some movement methods that never call
 *                      monster::finalise_movement())
 */
void invis_monster_knowledge::update(const monster& mon, bool reveal_pos, coord_def forced_pos)
{
    // If we already have an entry in the list for this monster, update it.
    // Otherwise, add a new one.
    const bool delete_only = !mon.alive() || you.can_see(mon);
    for (size_t i = 0; i < data.size(); ++i)
    {
        invis_mon_data& entry = data[i];
        if (entry.mid == mon.mid)
        {
            if (delete_only)
            {
                _clear_remembered_invis_mon(entry.last_known_pos);
                data.erase(data.begin() + i);
            }
            else
            {
                // Don't erase specific knowledge of a monster with a more
                // general awareness that they exist.
                if (reveal_pos)
                {
                    _clear_remembered_invis_mon(entry.last_known_pos);
                    entry.last_known_pos = mon.pos();
                }
                if (!forced_pos.origin())
                    entry.last_known_pos = forced_pos;

                entry.last_player_pos = you.pos();
                entry.last_seen_time = you.elapsed_time;
            }
            return;
        }
    }

    if (!delete_only)
        data.push_back({mon.mid, you.elapsed_time, reveal_pos ? mon.pos() : coord_def(), you.pos()});
}

void invis_monster_knowledge::handle_time()
{
    // Remove the memory of any monste that was seen too long ago for their
    // indicator to be useful.
    for (int i = data.size() - 1; i >= 0; --i)
    {
        if (data[i].last_seen_time + 150 < you.elapsed_time)
        {
            _clear_remembered_invis_mon(data[i].last_known_pos);
            data.erase(data.begin() + i);
        }
    }

    // Now check whether there are any outstanding invisible monsters whose
    // location the player does not know and which are likely to still be nearby.
    for (const auto &entry : data)
    {
        if (monster* mon = monster_by_mid(entry.mid))
        {
            if (!you.aware_of(*mon)
                && cell_see_cell(you.pos(), entry.last_player_pos, LOS_SOLID))
            {
                unknown_invis_nearby = true;
                return;
            }
        }
    }

    unknown_invis_nearby = false;
}

monster* invis_monster_knowledge::memory_at(const coord_def& pos)
{
    for (const auto &entry : data)
        if (entry.last_known_pos == pos)
            return monster_by_mid(entry.mid);

    return nullptr;
}

void invis_monster_knowledge::clear()
{
    for (size_t i = 0; i < data.size(); ++i)
        _clear_remembered_invis_mon(data[i].last_known_pos);

    data.clear();

    you.gave_invis_clear_prompt = false;
}

void invis_monster_knowledge::suppress_invis_warning()
{
    bool did_work = false;
    for (auto &entry : data)
    {
        if (monster* mon = monster_by_mid(entry.mid))
        {
            if (!you.aware_of(*mon) && !entry.last_player_pos.origin())
            {
                entry.last_player_pos = coord_def();
                did_work = true;
            }
        }
    }

    if (did_work)
    {
        mpr("Marking recently encountered invisible creatures as non-dangerous.");
        you.gave_invis_clear_prompt = false;
    }
}

// Are there any known-invisible monsters which the player doesn't currently
// know the real locations of, but thinks may be nearby?
// (We define 'nearby' as 'The player's position when the invisible monster was
// last detected is still in LoS)
bool invis_monster_knowledge::any_unknown_nearby()
{
    return unknown_invis_nearby;
}

// Returns all remembered invisible monsters whose last-known location was
// either in the player's LoS or completely unknown (but interacted with the
// player while they were near to their current location).
vector<monster_info> invis_monster_knowledge::get_unknown_in_los()
{
    vector<monster_info> unknown;

    for (const auto &entry : data)
    {
        if (cell_see_cell(you.pos(), entry.last_player_pos, LOS_SOLID))
        {
            if (monster* mon = monster_by_mid(entry.mid))
            {
                if (!you.aware_of(*mon))
                {
                    monster_info mi(mon->type, mon->base_monster);
                    mi.mb.set(MB_REMEMBERED_INVIS);
                    mi.pos = entry.last_known_pos;
                    unknown.push_back(mi);
                }
            }
        }
    }

    return unknown;
}

// Returns a string describing any invisible monsters the player is aware of,
// but does not know the location of.
//
// XXX: I *absolutely despise* that this is the best solution I could find for
//      getting this information to the webtiles monster list, but it is deeply
//      entangled with the idea that any monster on the list belongs to some
//      specific tile on the map in a way that seems extremely hard to change
//      without a major rewrite.
string invis_monster_knowledge::get_unknown_monster_description()
{
    vector<monster*> mons;
    for (const auto &entry : data)
        if (!you.see_cell(entry.last_known_pos))
            if (monster* mon = monster_by_mid(entry.mid))
                mons.push_back(mon);

    if (mons.empty())
        return "";

    return multimonster_name_string(mons, true);
}
