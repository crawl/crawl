/**
 * @file
 * @brief Misc abyss specific functions.
**/

#include "AppHdr.h"

#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <queue>

#include "abyss.h"
#include "areas.h"
#include "artefact.h"
#include "branch.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "dbg-scan.h"
#include "dgn-proclayouts.h"
#include "dungeon.h"
#include "env.h"
#include "files.h"
#include "itemprop.h"
#include "items.h"
#include "l_defs.h"
#include "libutil.h"
#include "los.h"
#include "makeitem.h"
#include "mapmark.h"
#include "maps.h"
#include "message.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-abil.h"
#include "mon-iter.h"
#include "mon-pathfind.h"
#include "mon-pick.h"
#include "mon-place.h"
#include "mon-transit.h"
#include "mon-util.h"
#include "notes.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "stash.h"
#include "state.h"
#include "terrain.h"
#include "tiledef-dngn.h"
#include "tileview.h"
#include "traps.h"
#include "travel.h"
#include "view.h"
#include "viewgeom.h"
#include "xom.h"

const coord_def ABYSS_CENTRE(GXM / 2, GYM / 2);

static const int ABYSSAL_RUNE_MAX_ROLL = 200;
static const int ABYSSAL_RUNE_MIN_LEVEL = 3;

abyss_state abyssal_state;

static ProceduralLayout *abyssLayout = nullptr, *levelLayout = nullptr;

typedef priority_queue<ProceduralSample, vector<ProceduralSample>, ProceduralSamplePQCompare> sample_queue;

static sample_queue abyss_sample_queue;
static vector<dungeon_feature_type> abyssal_features;
static list<monster*> displaced_monsters;

static void abyss_area_shift(void);
static void _push_items(void);

// If not_seen is true, don't place the feature where it can be seen from
// the centre.  Returns the chosen location, or INVALID_COORD if it
// could not be placed.
static coord_def _place_feature_near(const coord_def &centre,
                                     int radius,
                                     dungeon_feature_type candidate,
                                     dungeon_feature_type replacement,
                                     int tries, bool not_seen = false)
{
    coord_def cp = INVALID_COORD;
    const int radius2 = radius * radius + 1;
    for (int i = 0; i < tries; ++i)
    {
        cp = centre + coord_def(random_range(-radius, radius),
                                random_range(-radius, radius));

        if (cp == centre || (cp - centre).abs() > radius2 || !in_bounds(cp))
            continue;

        if (not_seen && cell_see_cell_nocache(cp, centre))
            continue;

        if (grd(cp) == candidate)
        {
            dprf("Placing %s at (%d,%d)",
                 dungeon_feature_name(replacement),
                 cp.x, cp.y);
            grd(cp) = replacement;
            return cp;
        }
    }
    return INVALID_COORD;
}

// Returns a feature suitable for use in the proto-Abyss level.
static dungeon_feature_type _abyss_proto_feature()
{
    return random_choose_weighted(3000, DNGN_FLOOR,
                                   600, DNGN_ROCK_WALL,
                                   300, DNGN_STONE_WALL,
                                   100, DNGN_METAL_WALL,
                                     1, DNGN_CLOSED_DOOR,
                                     0);
}

static void _write_abyssal_features()
{
    if (abyssal_features.empty())
        return;

    const int count = abyssal_features.size();
    ASSERT(count == 213);
    const int scalar = 0xFF;
    int index = 0;
    for (int x = -LOS_RADIUS; x <= LOS_RADIUS; x++)
    {
        for (int y = -LOS_RADIUS; y <= LOS_RADIUS; y++)
        {
            coord_def p(x, y);
            const int dist = p.abs();
            if (dist > LOS_RADIUS * LOS_RADIUS + 1)
                continue;
            p += ABYSS_CENTRE;

            int chance = pow(0.98, dist) * scalar;
            if (!map_masked(p, MMT_VAULT))
            {
                if (dist < 4 || x_chance_in_y(chance, scalar))
                {
                    if (abyssal_features[index] != DNGN_UNSEEN)
                    {
                        grd(p) = abyssal_features[index];
                        env.level_map_mask(p) = MMT_VAULT;
                    }
                }
                else
                {
                    //Entombing the player is lame.
                    grd(p) = DNGN_FLOOR;
                }
            }

            ++index;
        }
    }

    _push_items();
    abyssal_features.clear();
}

// Returns the roll to use to check if we want to create an abyssal rune.
static int _abyssal_rune_roll()
{
    if (you.runes[RUNE_ABYSSAL] || you.depth < ABYSSAL_RUNE_MIN_LEVEL)
        return -1;
    const bool lugonu_favoured =
        (you_worship(GOD_LUGONU) && !player_under_penance()
         && you.piety >= piety_breakpoint(4));

    const double depth = you.depth + lugonu_favoured;

    return (int) pow(100.0, depth/(1 + brdepth[BRANCH_ABYSS]));
}

static void _abyss_fixup_vault(const vault_placement *vp)
{
    for (vault_place_iterator vi(*vp); vi; ++vi)
    {
        const coord_def p(*vi);
        const dungeon_feature_type feat(grd(p));
        if (feat_is_stair(feat)
            && feat != DNGN_EXIT_ABYSS
            && feat != DNGN_ENTER_PORTAL_VAULT)
        {
            grd(p) = DNGN_FLOOR;
        }

        tile_init_flavour(p);
    }
}

static bool _abyss_place_map(const map_def *mdef)
{
    // This is to prevent the player position from being updated by vaults
    // until after everything is done.
    unwind_bool gen(crawl_state.generating_level, true);

    const bool did_place = dgn_safe_place_map(mdef, true, false, INVALID_COORD);
    if (did_place)
        _abyss_fixup_vault(env.level_vaults[env.level_vaults.size() - 1]);

    return did_place;
}

static bool _abyss_place_vault_tagged(const map_bitmask &abyss_genlevel_mask,
                                      const string &tag)
{
    const map_def *map = random_map_for_tag(tag, false, true);
    if (map)
    {
        unwind_vault_placement_mask vaultmask(&abyss_genlevel_mask);
        return _abyss_place_map(map);
    }
    return false;
}

static void _abyss_postvault_fixup()
{
    fixup_misplaced_items();
    link_items();
    env.markers.activate_all();
}

static bool _abyss_place_rune_vault(const map_bitmask &abyss_genlevel_mask)
{
    // Make sure we're not about to link bad items.
    debug_item_scan();

    const bool result = _abyss_place_vault_tagged(abyss_genlevel_mask,
                                                  "abyss_rune");

    // Make sure the rune is linked.
    _abyss_postvault_fixup();
    return result;
}

static bool _abyss_place_rune(const map_bitmask &abyss_genlevel_mask,
                              bool use_vaults)
{
    // Use a rune vault if there's one.
    if (use_vaults && _abyss_place_rune_vault(abyss_genlevel_mask))
        return true;

    coord_def chosen_spot;
    int places_found = 0;

    // Pick a random spot to drop the rune. We specifically do not use
    // random_in_bounds and similar, because we may be dealing with a
    // non-rectangular region, and we want to place the rune fairly.
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        const coord_def p(*ri);
        if (abyss_genlevel_mask(p)
            && grd(p) == DNGN_FLOOR && igrd(p) == NON_ITEM
            && one_chance_in(++places_found))
        {
            chosen_spot = p;
        }
    }

    if (places_found)
    {
        dprf("Placing abyssal rune at (%d,%d)", chosen_spot.x, chosen_spot.y);
        int thing_created = items(1, OBJ_MISCELLANY,
                                  MISC_RUNE_OF_ZOT, true, 0, 0);
        if (thing_created != NON_ITEM)
        {
            mitm[thing_created].plus = RUNE_ABYSSAL;
            item_colour(mitm[thing_created]);
        }
        move_item_to_grid(&thing_created, chosen_spot);
        return (thing_created != NON_ITEM);
    }

    return false;
}

// Returns true if items can be generated on the given square.
static bool _abyss_square_accepts_items(const map_bitmask &abyss_genlevel_mask,
                                        coord_def p)
{
    return (abyss_genlevel_mask(p)
            && grd(p) == DNGN_FLOOR
            && igrd(p) == NON_ITEM
            && !map_masked(p, MMT_VAULT));
}

static int _abyss_create_items(const map_bitmask &abyss_genlevel_mask,
                               bool placed_abyssal_rune,
                               bool use_vaults)
{
    // During game start, number and level of items mustn't be higher than
    // that on level 1.
    int num_items = 150, items_level = 52;
    int items_placed = 0;

    if (you.char_direction == GDT_GAME_START)
    {
        num_items   = 3 + roll_dice(3, 11);
        items_level = 0;
    }

    const int abyssal_rune_roll = _abyssal_rune_roll();
    bool should_place_abyssal_rune = false;
    vector<coord_def> chosen_item_places;
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        if (_abyss_square_accepts_items(abyss_genlevel_mask, *ri))
        {
            if (items_placed < num_items && one_chance_in(200))
            {
                // [ds] Don't place abyssal rune in this loop to avoid
                // biasing rune placement toward the north-west of the
                // abyss level. Instead, make a note of whether we
                // decided to place the abyssal rune at all, and if we
                // did, place it randomly somewhere in the map at the
                // end of the item-gen pass. We may as a result create
                // (num_items + 1) items instead of num_items, which
                // is acceptable.
                if (!placed_abyssal_rune && !should_place_abyssal_rune
                    && abyssal_rune_roll != -1
                    && you.char_direction != GDT_GAME_START
                    && x_chance_in_y(abyssal_rune_roll, ABYSSAL_RUNE_MAX_ROLL))
                {
                    should_place_abyssal_rune = true;
                }

                chosen_item_places.push_back(*ri);
            }
        }
    }

    if (!placed_abyssal_rune && should_place_abyssal_rune)
    {
        if (_abyss_place_rune(abyss_genlevel_mask, use_vaults))
            ++items_placed;
    }

    for (int i = 0, size = chosen_item_places.size(); i < size; ++i)
    {
        const coord_def place(chosen_item_places[i]);
        if (_abyss_square_accepts_items(abyss_genlevel_mask, place))
        {
            int thing_created = items(1, OBJ_RANDOM, OBJ_RANDOM,
                                      true, items_level, 250);
            move_item_to_grid(&thing_created, place);
            if (thing_created != NON_ITEM)
            {
                items_placed++;
                if (one_chance_in(ITEM_MIMIC_CHANCE))
                    mitm[thing_created].flags |= ISFLAG_MIMIC;
            }
        }
    }

    return items_placed;
}

void push_features_to_abyss()
{
    abyssal_features.clear();

    for (int x = -LOS_RADIUS; x <= LOS_RADIUS; x++)
    {
        for (int y = -LOS_RADIUS; y <= LOS_RADIUS; y++)
        {
            coord_def p(x, y);
            if (p.abs() > LOS_RADIUS * LOS_RADIUS + 1)
                continue;

            p += you.pos();

            dungeon_feature_type feature = map_bounds(p) ? grd(p) : DNGN_UNSEEN;
            feature = sanitize_feature(feature);
                 abyssal_features.push_back(feature);
        }
    }
}

static bool _abyss_check_place_feat(coord_def p,
                                    const int feat_chance,
                                    int *feats_wanted,
                                    bool *use_map,
                                    dungeon_feature_type which_feat,
                                    const map_bitmask &abyss_genlevel_mask)
{
    if (!which_feat)
        return false;

    const bool place_feat = feat_chance && one_chance_in(feat_chance);

    if (place_feat && feats_wanted)
        ++*feats_wanted;

    // Don't place features in bubbles.
    int wall_count = 0;
    for (adjacent_iterator ai(p); ai; ++ai)
        wall_count += feat_is_solid(grd(p));
    if (wall_count > 6)
        return false;

    // There's no longer a need to check for features under items,
    // since we're working on fresh grids that are guaranteed
    // item-free.
    if (place_feat || (feats_wanted && *feats_wanted > 0))
    {
        dprf("Placing abyss feature: %s.", dungeon_feature_name(which_feat));

        // When placing Abyss exits, try to use a vault if we have one.
        if (which_feat == DNGN_EXIT_ABYSS
            && use_map && *use_map
            && _abyss_place_vault_tagged(abyss_genlevel_mask, "abyss_exit"))
        {
            *use_map = false;

            // Link the vault-placed items.
            _abyss_postvault_fixup();
        }
        else
            grd(p) = which_feat;

        if (feats_wanted)
            --*feats_wanted;
        return true;
    }
    return false;
}

static dungeon_feature_type _abyss_pick_altar()
{
    // Lugonu has a flat 50% chance of corrupting the altar.
    if (coinflip())
        return DNGN_ALTAR_LUGONU;

    god_type god;

    do
        god = random_god();
    while (is_good_god(god));

    return altar_for_god(god);
}

static bool _abyssal_rune_at(const coord_def p)
{
    for (stack_iterator si(p); si; ++si)
        if (item_is_rune(*si, RUNE_ABYSSAL))
            return true;
    return false;
}

class xom_abyss_feature_amusement_check
{
private:
    bool exit_was_near;
    bool rune_was_near;

private:
    bool abyss_exit_nearness() const
    {
        // env.map_knowledge().known() doesn't work on unmappable levels because
        // mapping flags are not set on such levels.
        for (radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri)
            if (grd(*ri) == DNGN_EXIT_ABYSS && env.map_knowledge(*ri).seen())
                return true;

        return false;
    }

    bool abyss_rune_nearness() const
    {
        // See above comment about env.map_knowledge().known().
        for (radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri)
            if (env.map_knowledge(*ri).seen() && _abyssal_rune_at(*ri))
                return true;
        return false;
    }

public:
    xom_abyss_feature_amusement_check()
    {
        exit_was_near = abyss_exit_nearness();
        rune_was_near = abyss_rune_nearness();
    }

    // If the player was almost to the exit when it disappeared, Xom
    // is extremely amused. He's also extremely amused if the player
    // winds up right next to an exit when there wasn't one there
    // before. The same applies to Abyssal runes.
    ~xom_abyss_feature_amusement_check()
    {
        // Update known terrain
        viewwindow();

        const bool exit_is_near = abyss_exit_nearness();
        const bool rune_is_near = abyss_rune_nearness();

        if (exit_was_near && !exit_is_near || rune_was_near && !rune_is_near)
            xom_is_stimulated(200, "Xom snickers loudly.", true);

        if (!rune_was_near && rune_is_near || !exit_was_near && exit_is_near)
            xom_is_stimulated(200);
    }
};

static void _abyss_lose_monster(monster& mons)
{
    if (mons.needs_abyss_transit())
        mons.set_transit(level_id(BRANCH_ABYSS));

    mons.destroy_inventory();
    monster_cleanup(&mons);
}

// If a sanctuary exists and is in LOS, moves it to keep it in the
// same place relative to the player's location after a shift. If the
// sanctuary is not in player LOS, removes it.
static void _abyss_move_sanctuary(const coord_def abyss_shift_start_centre,
                                  const coord_def abyss_shift_end_centre)
{
    if (env.sanctuary_time > 0 && in_bounds(env.sanctuary_pos))
    {
        if (you.see_cell(env.sanctuary_pos))
        {
            env.sanctuary_pos += (abyss_shift_end_centre -
                                  abyss_shift_start_centre);
        }
        else
            remove_sanctuary(false);
    }
}

static void _push_displaced_monster(monster* mon)
{
    displaced_monsters.push_back(mon);
}

static void _place_displaced_monsters()
{
    list<monster*>::iterator mon_itr;

    for (mon_itr = displaced_monsters.begin();
         mon_itr != displaced_monsters.end(); ++mon_itr)
    {
        monster* mon = *mon_itr;
        if (mon->alive() && !mon->find_home_near_place(mon->pos()))
        {
            maybe_bloodify_square(mon->pos());
            if (you.can_see(mon))
            {
                simple_monster_message(mon, " is pulled into the abyss.",
                        MSGCH_BANISHMENT);
            }
            _abyss_lose_monster(*mon);

        }
    }

    displaced_monsters.clear();
}

static bool _pushy_feature(dungeon_feature_type feat)
{
    // Only completely impassible features and lava will push items.
    // In particular, deep water will not push items, because the item
    // will eventually become accessible again through abyss morphing.

    // Perhaps this should instead be merged with (the complement of)
    // _item_safe_square() in terrain.cc.  Unlike this function, that
    // one treats traps as unsafe, but closed doors as safe.
    return (feat_is_solid(feat) || feat == DNGN_LAVA);
}

static void _push_items()
{
    for (int i = 0; i < MAX_ITEMS; i++)
    {
        item_def& item(mitm[i]);
        if (!item.defined() || !in_bounds(item.pos) || item.held_by_monster())
            continue;

        if (!_pushy_feature(grd(item.pos)))
            continue;

        for (distance_iterator di(item.pos); di; ++di)
            if (!_pushy_feature(grd(*di)))
            {
                int j = i;
                move_item_to_grid(&j, *di, true);
                break;
            }
    }
}

// Deletes everything on the level at the given position.
// Things that are wiped:
// 1. Dungeon terrain (set to DNGN_UNSEEN)
// 2. Monsters (the player is unaffected)
// 3. Items
// 4. Clouds
// 5. Terrain properties
// 6. Terrain colours
// 7. Vault (map) mask
// 8. Vault id mask
// 9. Map markers

static void _abyss_wipe_square_at(coord_def p, bool saveMonsters=false)
{
    // Nuke terrain.
    destroy_shop_at(p);
    destroy_trap(p);

    // Nuke vault flag.
    if (map_masked(p, MMT_VAULT))
        env.level_map_mask(p) &= ~MMT_VAULT;

    grd(p) = DNGN_UNSEEN;

    // Nuke items.
    if (igrd(p) != NON_ITEM)
        dprf(DIAG_ABYSS, "Nuke item stack at (%d, %d)", p.x, p.y);
    lose_item_stack(p);

    // Nuke monster.
    if (monster* mon = monster_at(p))
    {
        if (saveMonsters)
            _push_displaced_monster(mon);
        else
            _abyss_lose_monster(*mon);
    }

    // Delete cloud.
    delete_cloud_at(p);

    env.pgrid(p)        = 0;
    env.grid_colours(p) = 0;
#ifdef USE_TILE
    env.tile_bk_fg(p)   = 0;
    env.tile_bk_bg(p)   = 0;
    env.tile_bk_cloud(p)= 0;
#endif
    tile_clear_flavour(p);
    tile_init_flavour(p);

    env.level_map_mask(p) = 0;
    env.level_map_ids(p)  = INVALID_MAP_INDEX;

    remove_markers_and_listeners_at(p);

    env.map_knowledge(p).clear();
    if (env.map_forgotten.get())
        (*env.map_forgotten.get())(p).clear();
    StashTrack.update_stash(p);
}

// Removes monsters, clouds, dungeon features, and items from the
// level, torching all squares for which the supplied mask is false.
static void _abyss_wipe_unmasked_area(const map_bitmask &abyss_preserve_mask)
{
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
        if (!abyss_preserve_mask(*ri))
            _abyss_wipe_square_at(*ri);
}

// Moves everything at src to dst.
static void _abyss_move_entities_at(coord_def src, coord_def dst)
{
    dgn_move_entities_at(src, dst, true, true, true);
}

// Move all vaults within the mask by the specified delta.
static void _abyss_move_masked_vaults_by_delta(const coord_def delta)
{
    set<int> vault_indexes;
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        const int vi = env.level_map_ids(*ri);
        if (vi != INVALID_MAP_INDEX)
            vault_indexes.insert(vi);
    }

    for (set<int>::const_iterator i = vault_indexes.begin();
         i != vault_indexes.end(); ++i)
    {
        vault_placement &vp(*env.level_vaults[*i]);
#ifdef DEBUG_DIAGNOSTICS
        const coord_def oldp = vp.pos;
#endif
        vp.pos += delta;
        dprf("Moved vault (%s) from (%d,%d)-(%d,%d)",
             vp.map.name.c_str(), oldp.x, oldp.y, vp.pos.x, vp.pos.y);
    }
}

// Moves the player, monsters, terrain and items in the square (circle
// in movement distance) around the player with the given radius to
// the square centred on target_centre.
//
// Assumes:
// a) target can be truncated if not fully in bounds
// b) source and target areas may overlap
//
static void _abyss_move_entities(coord_def target_centre,
                                 map_bitmask *shift_area_mask)
{
    const coord_def source_centre = you.pos();
    const coord_def delta = (target_centre - source_centre).sgn();

    // When moving a region, walk backward to handle overlapping
    // ranges correctly.
    coord_def direction = -delta;

    if (!direction.x)
        direction.x = 1;
    if (!direction.y)
        direction.y = 1;

    coord_def start(MAPGEN_BORDER, MAPGEN_BORDER);
    coord_def end(GXM - 1 - MAPGEN_BORDER, GYM - 1 - MAPGEN_BORDER);

    if (direction.x == -1)
        swap(start.x, end.x);
    if (direction.y == -1)
        swap(start.y, end.y);

    end += direction;

    for (int y = start.y; y != end.y; y += direction.y)
    {
        for (int x = start.x; x != end.x; x += direction.x)
        {
            const coord_def src(x, y);
            if (!shift_area_mask->get(src))
                continue;

            shift_area_mask->set(src, false);

            const coord_def dst = src - source_centre + target_centre;
            if (map_bounds_with_margin(dst, MAPGEN_BORDER))
            {
                shift_area_mask->set(dst);
                // Wipe the dstination clean before dropping things on it.
                _abyss_wipe_square_at(dst);
                _abyss_move_entities_at(src, dst);
            }
            else
            {
                // Wipe the source clean even if the dst is not in bounds.
                _abyss_wipe_square_at(src);
            }
        }
    }

    _abyss_move_masked_vaults_by_delta(target_centre - source_centre);
}

static void _abyss_expand_mask_to_cover_vault(map_bitmask *mask,
                                              int map_index)
{
    dprf("Expanding mask to cover vault %d (nvaults: %u)",
         map_index, (unsigned int)env.level_vaults.size());
    const vault_placement &vp = *env.level_vaults[map_index];
    for (vault_place_iterator vpi(vp); vpi; ++vpi)
        mask->set(*vpi);
}

// Identifies the smallest square around the given source that can be
// shifted without breaking up any vaults.
static void _abyss_identify_area_to_shift(coord_def source, int radius,
                                          map_bitmask *mask)
{
    mask->reset();

    set<int> affected_vault_indexes;
    for (radius_iterator ri(source, radius, C_SQUARE); ri; ++ri)
    {
        if (!map_bounds_with_margin(*ri, MAPGEN_BORDER))
            continue;

        mask->set(*ri);

        const int map_index = env.level_map_ids(*ri);
        if (map_index != INVALID_MAP_INDEX)
            affected_vault_indexes.insert(map_index);
    }

    for (set<int>::const_iterator i = affected_vault_indexes.begin();
         i != affected_vault_indexes.end(); ++i)
    {
        _abyss_expand_mask_to_cover_vault(mask, *i);
    }
}

static void _abyss_invert_mask(map_bitmask *mask)
{
    for (rectangle_iterator ri(0); ri; ++ri)
        mask->set(*ri, !mask->get(*ri));
}

// Moves everything in the given radius around the player (where radius=0 =>
// only the player) to another part of the level, centred on target_centre.
// Everything not in the given radius is wiped to DNGN_UNSEEN and the provided
// map_bitmask is set to true for all wiped squares.
//
// Things that are moved:
// 1. Dungeon terrain
// 2. Actors (player + monsters)
// 3. Items
// 4. Clouds
// 5. Terrain properties
// 6. Terrain colours
// 7. Vaults
// 8. Vault (map) mask
// 9. Vault id mask
// 10. Map markers
//
// After the shift, any vaults that are no longer referenced in the id
// mask will be discarded. If those vaults had any unique tags or
// names, the tag/name will NOT be unregistered.
//
// Assumes:
// a) radius >= LOS_RADIUS
// b) All points in the source and target areas are in bounds.
static void _abyss_shift_level_contents_around_player(
    int radius,
    coord_def target_centre,
    map_bitmask &abyss_destruction_mask)
{
    const coord_def source_centre = you.pos();

    abyssal_state.major_coord += (source_centre - ABYSS_CENTRE);

    ASSERT(radius >= LOS_RADIUS);
    // This should only really happen due to wizmode blink/movement.
    // 1KB: ... yet at least Xom "swap with monster" triggers it.
    //ASSERT(map_bounds_with_margin(source_centre, radius));
    //ASSERT(map_bounds_with_margin(target_centre, radius));

    _abyss_identify_area_to_shift(source_centre, radius,
                                  &abyss_destruction_mask);

    // Shift sanctuary centre if it's close.
    _abyss_move_sanctuary(you.pos(), target_centre);

    // Zap everything except the area we're shifting, so that there's
    // nothing in the way of moving stuff.
    _abyss_wipe_unmasked_area(abyss_destruction_mask);

    // Move stuff to its new home. This will also move the player.
    _abyss_move_entities(target_centre, &abyss_destruction_mask);

    // [ds] Rezap everything except the shifted area. NOTE: the old
    // code did not do this, leaving a repeated swatch of Abyss behind
    // at the old location for every shift; discussions between Linley
    // and dpeg on ##crawl confirm that this (repeated swatch of
    // terrain left behind) was not intentional.
    _abyss_wipe_unmasked_area(abyss_destruction_mask);

    // So far we've used the mask to track the portions of the level we're
    // preserving. The inverse of the mask represents the area to be filled
    // with brand new abyss:
    _abyss_invert_mask(&abyss_destruction_mask);

    // Update env.level_vaults to discard any vaults that are no longer in
    // the picture.
    dgn_erase_unused_vault_placements();
}

static void _abyss_generate_monsters(int nmonsters)
{
    if (crawl_state.disables[DIS_SPAWNS])
        return;

    mgen_data mg;
    mg.proximity = PROX_ANYWHERE;

    for (int mcount = 0; mcount < nmonsters; mcount++)
    {
        mg.cls = pick_random_monster(level_id::current());
        if (!invalid_monster_type(mg.cls))
            mons_place(mg);
    }
}

static bool _abyss_teleport_within_level()
{
    // Try to find a good spot within the shift zone.
    for (int i = 0; i < 100; i++)
    {
        const coord_def newspot =
            dgn_random_point_in_margin(MAPGEN_BORDER
                                       + ABYSS_AREA_SHIFT_RADIUS
                                       + 1);

        if ((grd(newspot) == DNGN_FLOOR
             || grd(newspot) == DNGN_SHALLOW_WATER)
            && !monster_at(newspot)
            && env.cgrid(newspot) == EMPTY_CLOUD)
        {
            dprf(DIAG_ABYSS, "Abyss same-area teleport to (%d,%d).",
                 newspot.x, newspot.y);
            you.moveto(newspot);
            return true;
        }
    }
    return false;
}

void maybe_shift_abyss_around_player()
{
    ASSERT(player_in_branch(BRANCH_ABYSS));
    if (map_bounds_with_margin(you.pos(),
                               MAPGEN_BORDER + ABYSS_AREA_SHIFT_RADIUS + 1))
    {
        return;
    }

    dprf("Shifting abyss at (%d,%d)", you.pos().x, you.pos().y);

    abyss_area_shift();
    if (you.pet_target != MHITYOU)
        you.pet_target = MHITNOT;

#ifdef DEBUG_DIAGNOSTICS
    int j = 0;
    for (int i = 0; i < MAX_ITEMS; ++i)
        if (mitm[i].defined())
            ++j;

    dprf("Number of items present: %d", j);

    j = 0;
    for (monster_iterator mi; mi; ++mi)
        ++j;

    dprf("Number of monsters present: %d", j);
    dprf("Number of clouds present: %d", env.cloud_no);
#endif
}

void save_abyss_uniques()
{
    for (monster_iterator mi; mi; ++mi)
        if (mi->needs_abyss_transit()
            && !testbits(mi->flags, MF_TAKING_STAIRS))
        {
            mi->set_transit(level_id(BRANCH_ABYSS));
        }
}

static bool _in_wastes(const coord_def &p)
{
    return (p.x > 0 && p.x < 0x7FFFFFF && p.y > 0 && p.y < 0x7FFFFFF);
}

static level_id _get_real_level()
{
    vector<level_id> levels;
    for (int i = BRANCH_MAIN_DUNGEON; i < NUM_BRANCHES; ++i)
    {
        if (i == BRANCH_SHOALS || i == BRANCH_ABYSS)
            continue;
        for (int j = 0; j < brdepth[i]; ++j)
        {
            const level_id id(static_cast<branch_type>(i), j);
            if (is_existing_level(id))
                levels.push_back(id);
        }
    }
    if (levels.empty())
    {
        // Let this fail later on.
        return level_id(static_cast<branch_type>(BRANCH_MAIN_DUNGEON), 1);
    }

    return levels[hash_rand(levels.size(), abyssal_state.seed)];
}

/**************************************************************/
/* Fixed layouts (ie, those that depend only on abyss coords) */
/**************************************************************/
const static WastesLayout wastes;
const static DiamondLayout diamond30(3,0);
const static DiamondLayout diamond21(2,1);
const static ColumnLayout column2(2);
const static ColumnLayout column26(2,6);
const static ProceduralLayout* regularLayouts[] =
{
    &diamond30, &diamond21, &column2, &column26,
};
const static vector<const ProceduralLayout*> layout_vec(regularLayouts,
    regularLayouts + ARRAYSZ(regularLayouts));
const static WorleyLayout worleyL(123456, layout_vec);
const static RoilingChaosLayout chaosA(8675309, 450);
const static RoilingChaosLayout chaosB(7654321, 400);
const static RoilingChaosLayout chaosC(24324,   380);
const static RoilingChaosLayout chaosD(24816,   500);
const static NewAbyssLayout newAbyssLayout(7629);
const static ProceduralLayout* mixedLayouts[] =
{
    &chaosA, &worleyL, &chaosB, &chaosC, &chaosD, &newAbyssLayout,
};
const static vector<const ProceduralLayout*> mixed_vec(mixedLayouts,
    mixedLayouts + ARRAYSZ(mixedLayouts));
const static WorleyLayout layout(4321, mixed_vec);
const static ProceduralLayout* baseLayouts[] = { &newAbyssLayout, &layout };
const static vector<const ProceduralLayout*> base_vec(baseLayouts,
    baseLayouts + ARRAYSZ(baseLayouts));
const static WorleyLayout baseLayout(314159, base_vec, 5.0);
const static RiverLayout rivers(1800, baseLayout);
// This one is not fixed: [0] is a level pulled from the current game
static vector<const ProceduralLayout*> complex_vec(2);

static ProceduralSample _abyss_grid(const coord_def &p)
{
    const coord_def pt = p + abyssal_state.major_coord;

    if (_in_wastes(pt))
    {
        ProceduralSample sample = wastes(pt, abyssal_state.depth);
        abyss_sample_queue.push(sample);
        return sample;
    }

    if (abyssLayout == NULL)
    {
        const level_id lid = _get_real_level();
        levelLayout = new LevelLayout(lid, 5, rivers);
        complex_vec[0] = levelLayout;
        complex_vec[1] = &rivers; // const
        abyssLayout = new WorleyLayout(23571113, complex_vec, 6.1);
    }

    const ProceduralSample sample = (*abyssLayout)(pt, abyssal_state.depth);
    ASSERT(sample.feat() > DNGN_UNSEEN);

    abyss_sample_queue.push(sample);
    return sample;
}

static cloud_type _cloud_from_feat(const dungeon_feature_type &ft)
{
    switch (ft)
    {
        case DNGN_CLOSED_DOOR:
        case DNGN_METAL_WALL:
            return CLOUD_GREY_SMOKE;
        case DNGN_GREEN_CRYSTAL_WALL:
        case DNGN_ROCK_WALL:
        case DNGN_SLIMY_WALL:
        case DNGN_STONE_WALL:
        case DNGN_PERMAROCK_WALL:
            return (coinflip() ? CLOUD_BLUE_SMOKE : CLOUD_PURPLE_SMOKE);
        case DNGN_CLEAR_ROCK_WALL:
        case DNGN_CLEAR_STONE_WALL:
        case DNGN_CLEAR_PERMAROCK_WALL:
        case DNGN_GRATE:
            return CLOUD_MIST;
        case DNGN_ORCISH_IDOL:
        case DNGN_GRANITE_STATUE:
        case DNGN_LAVA:
            return CLOUD_BLACK_SMOKE;
        case DNGN_DEEP_WATER:
        case DNGN_SHALLOW_WATER:
        case DNGN_FOUNTAIN_BLUE:
            return (one_chance_in(5) ? CLOUD_RAIN : CLOUD_BLUE_SMOKE);
        case DNGN_FOUNTAIN_SPARKLING:
            return CLOUD_RAIN;
        default:
            return CLOUD_NONE;
    }
}

static dungeon_feature_type _veto_dangerous_terrain(dungeon_feature_type feat)
{
    if (feat == DNGN_DEEP_WATER)
        return DNGN_SHALLOW_WATER;
    if (feat == DNGN_LAVA)
        return DNGN_FLOOR;

    return feat;
}

static void _update_abyss_terrain(const coord_def &p,
    const map_bitmask &abyss_genlevel_mask, bool morph)
{
    const coord_def rp = p - abyssal_state.major_coord;
    // ignore dead coordinates
    if (!in_bounds(rp))
        return;

    const dungeon_feature_type currfeat = grd(rp);

    // Don't decay vaults.
    if (map_masked(rp, MMT_VAULT))
        return;

    switch (currfeat)
    {
        case DNGN_EXIT_ABYSS:
        case DNGN_ABYSSAL_STAIR:
            return;
        default:
            break;
    }

    if (feat_is_altar(currfeat))
        return;

    if (!abyss_genlevel_mask(rp))
        return;

    if (currfeat != DNGN_UNSEEN && !morph)
        return;

    // What should have been there previously?  It might not be because
    // of external changes such as digging.
    const ProceduralSample sample = _abyss_grid(rp);

    // Enqueue the update, but don't morph.
    if (_abyssal_rune_at(rp))
        return;

    dungeon_feature_type feat = sample.feat();

    // Don't replace open doors with closed doors!
    if (feat_is_door(currfeat) && feat_is_door(feat))
        return;

    // Veto dangerous terrain.
    if (you.pos() == rp)
        feat = _veto_dangerous_terrain(feat);
    // Veto morph when there's a submerged monster (or a plant) below you.
    if (you.pos() == rp && mgrd(rp) != NON_MONSTER)
        feat = currfeat;

    // If the selected grid is already there, *or* if we're morphing and
    // the selected grid should have been there, do nothing.
    if (feat != currfeat)
    {
        grd(rp) = feat;
        if (feat == DNGN_FLOOR && in_los_bounds_g(rp))
        {
            cloud_type cloud = _cloud_from_feat(currfeat);
            int cloud_life = (_in_wastes(abyssal_state.major_coord) ? 5 : 2) + random2(2);
            if (cloud != CLOUD_NONE)
                check_place_cloud(_cloud_from_feat(currfeat), rp, cloud_life, 0, 3);
        }
        else if (feat_is_solid(feat))
        {
            int cloud = env.cgrid(rp);
            if (cloud != EMPTY_CLOUD)
                delete_cloud(cloud);
        }
        monster* mon = monster_at(rp);
        if (mon && !monster_habitable_grid(mon, feat))
            _push_displaced_monster(mon);
    }
}

static int _abyssal_stair_chance()
{
    return (you.char_direction == GDT_GAME_START ? 0 : 2800 - (200 * you.depth / 3));
}

static void _nuke_all_terrain(bool vaults)
{
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        if (vaults && env.level_map_mask(*ri) & MMT_VAULT)
            continue;
        env.level_map_mask(*ri) = MMT_NUKED;
    }
}

static void _ensure_player_habitable(bool dig_instead)
{
    dungeon_feature_type feat = grd(you.pos());
    if (!you.can_pass_through_feat(feat)
        || is_feat_dangerous(feat) && !(you.is_wall_clinging()
                                        && cell_is_clingable(you.pos())))
    {
        bool shoved = you.shove();
        if (!shoved)
        {
            // legal only if we just placed a vault
            ASSERT(dig_instead);
            grd(you.pos()) = DNGN_FLOOR;
        }
    }
}

static void _abyss_apply_terrain(const map_bitmask &abyss_genlevel_mask,
                                 bool morph = false, bool now = false)
{
    // The chance is reciprocal to these numbers.
    const int exit_chance = you.runes[RUNE_ABYSSAL] ? 1250
                            : 7500 - 1250 * (you.depth - 1);

    // Except for the altar on the starting position, don't place any altars.
    const int altar_chance = you.char_direction != GDT_GAME_START ? 10000 : 0;

    int exits_wanted  = 0;
    int altars_wanted = 0;
    bool use_abyss_exit_map = true;
    bool used_queue = false;
    if (morph && !abyss_sample_queue.empty())
    {
        int ii = 0;
        used_queue = true;
        while (!abyss_sample_queue.empty()
            && abyss_sample_queue.top().changepoint() < abyssal_state.depth)
        {
            ++ii;
            coord_def p = abyss_sample_queue.top().coord();
            _update_abyss_terrain(p, abyss_genlevel_mask, morph);
            abyss_sample_queue.pop();
        }
/*
        if (ii)
            dprf(DIAG_ABYSS, "Examined %d features.", ii);
*/
    }

    int ii = 0;
    int delta = you.time_taken * (you.abyss_speed + 40) / 200;
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        const coord_def p(*ri);
        const coord_def abyss_coord = p + abyssal_state.major_coord;
        bool nuked = map_masked(p, MMT_NUKED);
        if (used_queue && !nuked)
            continue;

        if (nuked && (now || x_chance_in_y(delta, 50)) || !nuked && !used_queue)
        {
            ++ii;
            _update_abyss_terrain(abyss_coord, abyss_genlevel_mask, morph);
            env.level_map_mask(p) &= ~MMT_NUKED;
        }
        if (morph)
            continue;

        // Place abyss exits, stone arches, and altars to liven up the scene
        // (only on area creation, not on morphing).
        (_abyss_check_place_feat(p, exit_chance,
                                &exits_wanted,
                                &use_abyss_exit_map,
                                DNGN_EXIT_ABYSS,
                                abyss_genlevel_mask)
        ||
        _abyss_check_place_feat(p, altar_chance,
                                &altars_wanted,
                                NULL,
                                _abyss_pick_altar(),
                                abyss_genlevel_mask)
        ||
        (level_id::current().depth < brdepth[BRANCH_ABYSS] &&
        _abyss_check_place_feat(p, _abyssal_stair_chance(), NULL, NULL,
                                DNGN_ABYSSAL_STAIR,
                                abyss_genlevel_mask)));
    }
    if (ii)
        dprf(DIAG_ABYSS, "Nuked %d features", ii);
    _ensure_player_habitable(false);
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
        ASSERT_RANGE(grd(*ri), DNGN_UNSEEN + 1, NUM_FEATURES);
}

static int _abyss_place_vaults(const map_bitmask &abyss_genlevel_mask)
{
    unwind_vault_placement_mask vaultmask(&abyss_genlevel_mask);

    int vaults_placed = 0;

    const int maxvaults = 6;
    int tries = 0;
    while (vaults_placed < maxvaults)
    {
        const map_def *map = random_map_for_tag("abyss", false, true);
        if (!map)
            break;

        if (!_abyss_place_map(map) || map->has_tag("extra"))
        {
            if (tries++ >= 100)
                break;

            continue;
        }

        if (!one_chance_in(2 + (++vaults_placed)))
            break;
    }

    return vaults_placed;
}

static void _generate_area(const map_bitmask &abyss_genlevel_mask)
{
    // Any rune on the floor prevents the abyssal rune from being generated.
    const bool placed_abyssal_rune =
        find_floor_item(OBJ_MISCELLANY, MISC_RUNE_OF_ZOT);

    dprf(DIAG_ABYSS, "_generate_area(). turns_on_level: %d, rune_on_floor: %s",
         env.turns_on_level, placed_abyssal_rune? "yes" : "no");

    _abyss_apply_terrain(abyss_genlevel_mask);

    bool use_vaults = you.char_direction != GDT_GAME_START;

    if (use_vaults)
    {
        // Make sure we're not about to link bad items.
        debug_item_scan();
        _abyss_place_vaults(abyss_genlevel_mask);

        // Link the vault-placed items.
        _abyss_postvault_fixup();
    }
    _abyss_create_items(abyss_genlevel_mask, placed_abyssal_rune, use_vaults);
    setup_environment_effects();

    _ensure_player_habitable(true);

    // Abyss has a constant density.
    env.density = 0;
}

static void _initialize_abyss_state()
{
    abyssal_state.major_coord.x = random_int() & 0x7FFFFFFF;
    abyssal_state.major_coord.y = random_int() & 0x7FFFFFFF;
    abyssal_state.seed = random_int() & 0x7FFFFFFF;
    abyssal_state.phase = 0.0;
    abyssal_state.depth = random_int() & 0x7FFFFFFF;
    abyssal_state.nuke_all = false;
    level_id level;
    // TODO: Select this rather than setting it to a fixed value.
    level.branch = BRANCH_MAIN_DUNGEON;
    level.depth = 19;
    abyssal_state.level = level;
    abyss_sample_queue = sample_queue(ProceduralSamplePQCompare());
}

void set_abyss_state(coord_def coord, uint32_t depth)
{
    abyssal_state.major_coord = coord;
    abyssal_state.depth = depth;
    abyssal_state.seed = random_int() & 0x7FFFFFFF;
    abyssal_state.phase = 0.0;
    abyssal_state.nuke_all = true;
    abyss_sample_queue = sample_queue(ProceduralSamplePQCompare());
    you.moveto(ABYSS_CENTRE);
    map_bitmask abyss_genlevel_mask(true);
    _abyss_apply_terrain(abyss_genlevel_mask, true, true);
}

static void abyss_area_shift(void)
{
    dprf(DIAG_ABYSS, "area_shift() - player at pos (%d, %d)",
         you.pos().x, you.pos().y);

    {
        xom_abyss_feature_amusement_check xomcheck;

        // A teleport may move you back to the center, resulting in a (0,0)
        // shift.  The code can't handle those.  We still to forget the map,
        // spawn new monsters or allow return from transit, though.
        if (you.pos() != ABYSS_CENTRE)
        {
            // Use a map mask to track the areas that the shift destroys and
            // that must be regenerated by _generate_area.
            map_bitmask abyss_genlevel_mask;
            _abyss_shift_level_contents_around_player(
                ABYSS_AREA_SHIFT_RADIUS, ABYSS_CENTRE, abyss_genlevel_mask);
            _generate_area(abyss_genlevel_mask);
        }
        forget_map(true);

        // Update LOS at player's new abyssal vacation retreat.
        los_changed();
    }

    // Place some monsters to keep the abyss party going.
    int num_monsters = 15 + you.depth * (1 + coinflip());
    _abyss_generate_monsters(num_monsters);

    // And allow monsters in transit another chance to return.
    place_transiting_monsters();
    place_transiting_items();

    check_map_validity();
}

void destroy_abyss()
{
    if (abyssLayout)
    {
        delete abyssLayout;
        abyssLayout = nullptr;
        delete levelLayout;
        levelLayout = nullptr;
    }
}

static colour_t _roll_abyss_floor_colour()
{
    return random_choose_weighted(
         108, BLUE,
         632, GREEN,
         // no CYAN (silence)
         932, RED,
         488, MAGENTA,
         433, BROWN,
        3438, LIGHTGRAY,
         // no DARKGREY (out of LOS)
         766, LIGHTBLUE,
         587, LIGHTGREEN,
         794, LIGHTCYAN,
         566, LIGHTRED,
         313, LIGHTMAGENTA,
         // no YELLOW (halo)
         890, WHITE,
          50, ETC_FIRE,
    0);
}

static colour_t _roll_abyss_rock_colour()
{
    return random_choose_weighted(
         130, BLUE,
         409, GREEN,
         // no CYAN (metal)
         770, RED,
         522, MAGENTA,
        1292, BROWN,
         // no LIGHTGRAY (stone)
         // no DARKGREY (out of LOS)
         570, LIGHTBLUE,
         705, LIGHTGREEN,
         // no LIGHTCYAN (glass)
        1456, LIGHTRED,
         377, LIGHTMAGENTA,
         105, YELLOW,
         101, WHITE,
          60, ETC_FIRE,
    0);
}

static void _abyss_generate_new_area()
{
    _initialize_abyss_state();
    dprf("Abyss Coord (%d, %d)", abyssal_state.major_coord.x, abyssal_state.major_coord.y);
    remove_sanctuary(false);

    env.floor_colour = _roll_abyss_floor_colour();
    env.rock_colour = _roll_abyss_rock_colour();
    tile_init_flavour();

    map_bitmask abyss_genlevel_mask;
    _abyss_wipe_unmasked_area(abyss_genlevel_mask);
    dgn_erase_unused_vault_placements();

    you.moveto(ABYSS_CENTRE);
    abyss_genlevel_mask.init(true);
    _generate_area(abyss_genlevel_mask);
    if (one_chance_in(5))
    {
        _place_feature_near(you.pos(), LOS_RADIUS,
                            DNGN_FLOOR, DNGN_ALTAR_LUGONU, 50);
    }

    los_changed();
    place_transiting_monsters();
    place_transiting_items();
}

// Check if there is a path between the abyss centre and an exit location.
static bool _abyss_has_path(const coord_def &to)
{
    ASSERT(grd(to) == DNGN_EXIT_ABYSS);

    monster_pathfind pf;
    return pf.init_pathfind(ABYSS_CENTRE, to);
}

// Generate the initial (proto) Abyss level. The proto Abyss is where
// the player lands when they arrive in the Abyss from elsewhere.
// _generate_area generates all other Abyss areas.
void generate_abyss()
{
    env.level_build_method += " abyss";
    env.level_layout_types.insert("abyss");
    destroy_abyss();

retry:
    _initialize_abyss_state();

    dprf("generate_abyss(); turn_on_level: %d", env.turns_on_level);

    // Generate the initial abyss without vaults. Vaults are horrifying.
    _abyss_generate_new_area();
    _write_abyssal_features();
    map_bitmask abyss_genlevel_mask(true);
    _abyss_apply_terrain(abyss_genlevel_mask);

    grd(you.pos()) = _veto_dangerous_terrain(grd(you.pos()));

    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
        ASSERT(grd(*ri) > DNGN_UNSEEN);
    check_map_validity();

    // If we're starting out in the Abyss, make sure the starting grid is
    // an altar to Lugonu and there's an exit near-by.
    // Otherwise, we start out on floor and there's a chance there's an
    // altar near-by.
    if (you.char_direction == GDT_GAME_START)
    {
        grd(ABYSS_CENTRE) = DNGN_ALTAR_LUGONU;
        const coord_def eloc = _place_feature_near(ABYSS_CENTRE, LOS_RADIUS + 2,
                                                   DNGN_FLOOR, DNGN_EXIT_ABYSS,
                                                   50, true);
        // Now make sure there is a path from the abyss centre to the exit.
        // If for some reason an exit could not be placed, don't bother.
        if (eloc == INVALID_COORD || !_abyss_has_path(eloc))
            goto retry;
    }
    else
    {
        grd(ABYSS_CENTRE) = DNGN_FLOOR;
        if (one_chance_in(5))
        {
            _place_feature_near(ABYSS_CENTRE, LOS_RADIUS,
                                DNGN_FLOOR, DNGN_ALTAR_LUGONU, 50);
        }
    }

    setup_environment_effects();
}

static void _increase_depth()
{
    int delta = you.time_taken * (you.abyss_speed + 40) / 200;
    if (!you_worship(GOD_CHEIBRIADOS) || you.penance[GOD_CHEIBRIADOS])
        delta *= 2;
    if (you.duration[DUR_TELEPORT])
        delta *= 5;
    const double theta = abyssal_state.phase;
    double depth_change = delta * (0.2 + 2.8 * pow(sin(theta/2), 10.0));
    abyssal_state.depth += depth_change;
    abyssal_state.phase += delta / 100.0;
    if (abyssal_state.phase > M_PI)
        abyssal_state.phase -= M_PI;
}

void abyss_morph(double duration)
{
    if (abyssal_state.nuke_all)
    {
        _nuke_all_terrain(false);
        abyssal_state.nuke_all = false;
    }
    if (!player_in_branch(BRANCH_ABYSS))
        return;
    _increase_depth();
    map_bitmask abyss_genlevel_mask(true);
    dgn_erase_unused_vault_placements();
    _abyss_apply_terrain(abyss_genlevel_mask, true);
    _place_displaced_monsters();
    _push_items();
    los_changed();
}

void abyss_teleport(bool new_area)
{
    xom_abyss_feature_amusement_check xomcheck;
    if (!new_area)
    {
        _abyss_teleport_within_level();
        abyss_area_shift();
        _initialize_abyss_state();
        _nuke_all_terrain(true);
        forget_map(false);
        clear_excludes();
        more();
        return;
    }

    dprf(DIAG_ABYSS, "New area Abyss teleport.");
    _abyss_generate_new_area();
    _write_abyssal_features();
    grd(you.pos()) = _veto_dangerous_terrain(grd(you.pos()));
    forget_map(false);
    clear_excludes();
    more();
}

//////////////////////////////////////////////////////////////////////////////
// Abyss effects in other levels, courtesy Lugonu.

struct corrupt_env
{
    int rock_colour, floor_colour;
    corrupt_env(): rock_colour(BLACK), floor_colour(BLACK) { }
};

static void _place_corruption_seed(const coord_def &pos, int duration)
{
    env.markers.add(new map_corruption_marker(pos, duration));
    // Corruption markers don't need activation, though we might
    // occasionally miss other unactivated markers by clearing.
    env.markers.clear_need_activate();
}

static void _initialise_level_corrupt_seeds(int power)
{
    const int low = power * 40 / 100, high = power * 140 / 100;
    const int nseeds = random_range(-1, min(2 + power / 110, 4), 2);

    const int aux_seed_radius = 4;

    dprf("Placing %d corruption seeds (power: %d)", nseeds, power);

    // The corruption centreed on the player is free.
    _place_corruption_seed(you.pos(), high + 300);

    for (int i = 0; i < nseeds; ++i)
    {
        coord_def where;
        int tries = 100;
        while (tries-- > 0)
        {
            where = dgn_random_point_from(you.pos(), aux_seed_radius, 2);
            if (grd(where) == DNGN_FLOOR && !env.markers.find(where, MAT_ANY))
                break;
            where.reset();
        }

        if (!where.origin())
            _place_corruption_seed(where, random_range(low, high, 2) + 300);
    }
}

static bool _incorruptible(monster_type mt)
{
    return mons_is_abyssal_only(mt) || mons_class_holiness(mt) == MH_HOLY;
}

// Create a corruption spawn at the given position. Returns false if further
// monsters should not be placed near this spot (overcrowding), true if
// more monsters can fit in.
static bool _spawn_corrupted_servant_near(const coord_def &pos)
{
    const beh_type beh =
        x_chance_in_y(100, 200 + you.skill(SK_INVOCATIONS, 25)) ? BEH_HOSTILE
        : BEH_NEUTRAL;

    // [ds] No longer summon hostiles -- don't create the monster if
    // it would be hostile.
    if (beh == BEH_HOSTILE)
        return true;

    // Thirty tries for a place.
    for (int i = 0; i < 30; ++i)
    {
        const int offsetX = random2avg(4, 3) + random2(3);
        const int offsetY = random2avg(4, 3) + random2(3);
        const coord_def p(pos.x + (coinflip()? offsetX : -offsetX),
                          pos.y + (coinflip()? offsetY : -offsetY));
        if (!in_bounds(p) || actor_at(p)
            || !feat_compatible(DNGN_FLOOR, grd(p)))
        {
            continue;
        }

        monster_type mons = pick_monster(level_id(BRANCH_ABYSS), _incorruptible);
        ASSERT(mons);
        mgen_data mg(mons, beh, 0, 5, 0, p);
        mg.non_actor_summoner = "Lugonu's corruption";
        mg.place = BRANCH_ABYSS;
        return create_monster(mg);
    }

    return false;
}

static void _apply_corruption_effect(map_marker *marker, int duration)
{
    if (!duration)
        return;

    map_corruption_marker *cmark = dynamic_cast<map_corruption_marker*>(marker);
    if (cmark->duration < 1)
        return;

    const int neffects = max(div_rand_round(duration, 5), 1);

    for (int i = 0; i < neffects; ++i)
    {
        if (x_chance_in_y(cmark->duration, 4000)
            && !_spawn_corrupted_servant_near(cmark->pos))
        {
            break;
        }
    }
    cmark->duration -= duration;
}

void run_corruption_effects(int duration)
{
    vector<map_marker*> markers = env.markers.get_all(MAT_CORRUPTION_NEXUS);

    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        map_marker *mark = markers[i];
        if (mark->get_type() != MAT_CORRUPTION_NEXUS)
            continue;

        _apply_corruption_effect(mark, duration);
    }
}

static bool _is_grid_corruptible(const coord_def &c)
{
    if (c == you.pos())
        return false;

    const dungeon_feature_type feat = grd(c);

    // Stairs and portals cannot be corrupted.
    if (feat_stair_direction(feat) != CMD_NO_CMD)
        return false;

    switch (feat)
    {
    case DNGN_PERMAROCK_WALL:
    case DNGN_CLEAR_PERMAROCK_WALL:
        return false;

    case DNGN_METAL_WALL:
    case DNGN_GREEN_CRYSTAL_WALL:
        return one_chance_in(4);

    case DNGN_STONE_WALL:
    case DNGN_CLEAR_STONE_WALL:
        return one_chance_in(3);

    case DNGN_ROCK_WALL:
    case DNGN_CLEAR_ROCK_WALL:
        return !one_chance_in(3);

    default:
        return true;
    }
}

// Returns true if the square has <= 4 traversable neighbours.
static bool _is_crowded_square(const coord_def &c)
{
    int neighbours = 0;
    for (int xi = -1; xi <= 1; ++xi)
        for (int yi = -1; yi <= 1; ++yi)
        {
            if (!xi && !yi)
                continue;

            const coord_def n(c.x + xi, c.y + yi);
            if (!in_bounds(n) || !feat_is_traversable(grd(n)))
                continue;

            if (++neighbours > 4)
                return false;
        }

    return true;
}

// Returns true if the square has all opaque neighbours.
static bool _is_sealed_square(const coord_def &c)
{
    for (adjacent_iterator ai(c); ai; ++ai)
        if (!feat_is_opaque(grd(*ai)))
            return false;

    return true;
}

static void _corrupt_square(const corrupt_env &cenv, const coord_def &c)
{
    // To prevent the destruction of, say, branch entries.
    bool preserve_feat = true;
    dungeon_feature_type feat = DNGN_UNSEEN;
    if (feat_altar_god(grd(c)) != GOD_NO_GOD)
    {
        // altars may be safely overwritten, ha!
        preserve_feat = false;
        if (!one_chance_in(3))
            feat = DNGN_ALTAR_LUGONU;
    }
    else
        feat = _abyss_proto_feature();

    if (feat_is_trap(feat, true)
        || feat == DNGN_UNSEEN
        || (feat_is_traversable(grd(c)) && !feat_is_traversable(feat)
            && coinflip()))
    {
        feat = DNGN_FLOOR;
    }

    if (feat_is_traversable(grd(c)) && !feat_is_traversable(feat)
        && _is_crowded_square(c))
    {
        return;
    }

    if (!feat_is_traversable(grd(c)) && feat_is_traversable(feat)
        && _is_sealed_square(c))
    {
        return;
    }

    if (feat == DNGN_EXIT_ABYSS)
        feat = DNGN_ENTER_ABYSS;

    // If we are trying to place a wall on top of a creature or item, try to
    // move it aside. If this fails, simply place floor instead.
    actor* act = actor_at(c);
    if (feat_is_solid(feat) && (igrd(c) != NON_ITEM || act))
    {
        coord_def newpos;
        get_push_space(c, newpos, act, true);
        if (!newpos.origin())
        {
            move_items(c, newpos);
            if (act)
                actor_at(c)->move_to_pos(newpos);
        }
        else
            feat = DNGN_FLOOR;
    }

    dungeon_terrain_changed(c, feat, true, preserve_feat, true);
    if (feat == DNGN_ROCK_WALL)
        env.grid_colours(c) = cenv.rock_colour;
    else if (feat == DNGN_FLOOR)
        env.grid_colours(c) = cenv.floor_colour;

    if (feat == DNGN_ROCK_WALL)
    {
        tileidx_t idx = tile_dngn_coloured(TILE_WALL_ABYSS,
                                           cenv.floor_colour);
        env.tile_flv(c).wall = idx + random2(tile_dngn_count(idx));
    }
    else if (feat == DNGN_FLOOR)
    {
        tileidx_t idx = tile_dngn_coloured(TILE_FLOOR_NERVES,
                                           cenv.floor_colour);
        env.tile_flv(c).floor = idx + random2(tile_dngn_count(idx));
    }
}

static void _corrupt_level_features(const corrupt_env &cenv)
{
    vector<coord_def> corrupt_seeds;
    vector<map_marker*> corrupt_markers =
        env.markers.get_all(MAT_CORRUPTION_NEXUS);

    for (int i = 0, size = corrupt_markers.size(); i < size; ++i)
        corrupt_seeds.push_back(corrupt_markers[i]->pos);

    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        int idistance2 = GXM * GXM + GYM * GYM;
        for (int i = 0, size = corrupt_seeds.size(); i < size; ++i)
        {
            const int idist2 = (*ri - corrupt_seeds[i]).abs();
            if (idist2 < idistance2)
                idistance2 = idist2;
        }

        const int ground_zero_radius2 = 7;

        // Corruption odds are 100% within about 2 squares, decaying to 30%
        // at LOS range (radius 8). Even if the corruption roll is made,
        // the feature still gets a chance to resist if it's a wall.
        const int corrupt_perc_chance =
            idistance2 <= ground_zero_radius2 ? 100 :
            max(1, 100 - (idistance2 - ground_zero_radius2) * 70 / 57);

        if (random2(100) < corrupt_perc_chance && _is_grid_corruptible(*ri))
            _corrupt_square(cenv, *ri);
    }
}

static bool _is_level_corrupted()
{
    if (player_in_branch(BRANCH_ABYSS))
        return true;

    return !!env.markers.find(MAT_CORRUPTION_NEXUS);
}

bool is_level_incorruptible(bool quiet)
{
    if (_is_level_corrupted())
    {
        if (!quiet)
            mpr("This place is already infused with evil and corruption.");
        return true;
    }

    return false;
}

static void _corrupt_choose_colours(corrupt_env *cenv)
{
    colour_t colour = BLACK;
    do
        colour = random_uncommon_colour();
    while (colour == env.rock_colour || colour == LIGHTGREY || colour == WHITE);
    cenv->rock_colour = colour;

    do
        colour = random_uncommon_colour();
    while (colour == env.floor_colour || colour == LIGHTGREY
           || colour == WHITE);
    cenv->floor_colour = colour;
}

bool lugonu_corrupt_level(int power)
{
    if (is_level_incorruptible())
        return false;

    simple_god_message("'s Hand of Corruption reaches out!");
    take_note(Note(NOTE_MESSAGE, 0, 0, make_stringf("Corrupted %s",
              level_id::current().describe().c_str()).c_str()));

    flash_view(MAGENTA);

    _initialise_level_corrupt_seeds(power);

    corrupt_env cenv;
    _corrupt_choose_colours(&cenv);
    _corrupt_level_features(cenv);
    run_corruption_effects(300);

#ifndef USE_TILE_LOCAL
    // Allow extra time for the flash to linger.
    delay(1000);
#endif

    return true;
}
