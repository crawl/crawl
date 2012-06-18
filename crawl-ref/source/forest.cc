/**
 * @file
 * @brief Misc Enchanted Forest specific functions.
**/

#include "AppHdr.h"

#include "forest.h"

#include <cmath>
#include <cstdlib>
#include <algorithm>

#include "areas.h"
#include "cellular.h"
#include "cloud.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "dbg-scan.h"
#include "dungeon.h"
#include "env.h"
#include "items.h"
#include "mapmark.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-place.h"
#include "mon-transit.h"
#include "monster.h"
#include "shopping.h"
#include "stash.h"
#include "tiledef-dngn.h"
#include "tileview.h"
#include "traps.h"

const coord_def FOREST_CENTRE(GXM / 2, GYM / 2);

static void forest_area_shift(void);
static void _push_items(void);

forest_state for_state;

//#define DEBUG_FOREST

static void _forest_lose_monster(monster& mons)
{
    if (mons.type != MONS_PLANT && mons.type != MONS_BUSH)
        mons.set_transit(level_id(BRANCH_FOREST));

    mons.destroy_inventory();
    monster_cleanup(&mons);
}

#if 0
// If a sanctuary exists and is in LOS, moves it to keep it in the
// same place relative to the player's location after a shift. If the
// sanctuary is not in player LOS, removes it.
static void _forest_move_sanctuary(const coord_def forest_shift_start_centre,
                                  const coord_def forest_shift_end_centre)
{
    if (env.sanctuary_time > 0 && in_bounds(env.sanctuary_pos))
    {
        if (you.see_cell(env.sanctuary_pos))
            env.sanctuary_pos += (forest_shift_end_centre -
                                  forest_shift_start_centre);
        else
            remove_sanctuary(false);
    }
}
#endif

static void _push_items()
{
    for (int i = 0; i < MAX_ITEMS; i++)
    {
        item_def& item(mitm[i]);
        if (!item.defined() || !in_bounds(item.pos) || item.held_by_monster())
            continue;

        if (grd(item.pos) == DNGN_FLOOR)
            continue;

        for (distance_iterator di(item.pos); di; ++di)
            if (grd(*di) == DNGN_FLOOR)
            {
                move_item_to_grid(&i, *di, true);
                break;
            }
    }
}

#if 0
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
static void _forest_wipe_square_at(coord_def p, bool saveMonsters=false)
{
    // Nuke terrain.
    destroy_shop_at(p);
    destroy_trap(p);

    // Nuke vault flag.
    if (map_masked(p, MMT_VAULT))
        env.level_map_mask(p) &= ~MMT_VAULT;

    grd(p) = DNGN_UNSEEN;

    // Nuke items.
#ifdef DEBUG_FOREST
    if (igrd(p) != NON_ITEM)
        dprf("Nuke item stack at (%d, %d)", p.x, p.y);
#endif
    lose_item_stack(p);

    // Nuke monster.
    if (monster* mon = monster_at(p))
    {
        _forest_lose_monster(*mon);
    }

    // Delete cloud.
    delete_cloud_at(p);

    env.pgrid(p)        = 0;
    env.grid_colours(p) = 0;
#ifdef USE_TILE
    env.tile_bk_fg(p)   = 0;
    env.tile_bk_bg(p)   = 0;
#endif
    tile_clear_flavour(p);
    tile_init_flavour(p);

    env.level_map_mask(p) = 0;
    env.level_map_ids(p)  = INVALID_MAP_INDEX;

    remove_markers_and_listeners_at(p);

    env.map_knowledge(p).clear();
    StashTrack.update_stash(p);
}

// Removes monsters, clouds, dungeon features, and items from the
// level, torching all squares for which the supplied mask is false.
static void _forest_wipe_unmasked_area(const map_mask &forest_preserve_mask)
{
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
        if (!forest_preserve_mask(*ri))
            _forest_wipe_square_at(*ri);
}

// Moves everything at src to dst.
static void _forest_move_entities_at(coord_def src, coord_def dst)
{
    dgn_move_entities_at(src, dst, true, true, true);
}

// Move all vaults within the mask by the specified delta.
static void _forest_move_masked_vaults_by_delta(const map_mask &mask,
                                               const coord_def delta)
{
    std::set<int> vault_indexes;
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        const int vi = env.level_map_ids(*ri);
        if (vi != INVALID_MAP_INDEX)
            vault_indexes.insert(vi);
    }

    for (std::set<int>::const_iterator i = vault_indexes.begin();
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
static void _forest_move_entities(coord_def target_centre,
                                 map_mask *shift_area_mask)
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
        std::swap(start.x, end.x);
    if (direction.y == -1)
        std::swap(start.y, end.y);

    end += direction;

    for (int y = start.y; y != end.y; y += direction.y)
    {
        for (int x = start.x; x != end.x; x += direction.x)
        {
            const coord_def src(x, y);
            if (!(*shift_area_mask)(src))
                continue;

            (*shift_area_mask)(src) = false;

            const coord_def dst = src - source_centre + target_centre;
            if (map_bounds_with_margin(dst, MAPGEN_BORDER))
            {
                (*shift_area_mask)(dst) = true;
                // Wipe the dstination clean before dropping things on it.
                _forest_wipe_square_at(dst);
                _forest_move_entities_at(src, dst);
            }
            else
            {
                // Wipe the source clean even if the dst is not in bounds.
                _forest_wipe_square_at(src);
            }
        }
    }

    _forest_move_masked_vaults_by_delta(*shift_area_mask,
                                       target_centre - source_centre);
}
#endif

static void _forest_expand_mask_to_cover_vault(map_mask *mask,
                                              int map_index)
{
    dprf("Expanding mask to cover vault %d (nvaults: %u)",
         map_index, (unsigned int)env.level_vaults.size());
    const vault_placement &vp = *env.level_vaults[map_index];
    for (vault_place_iterator vpi(vp); vpi; ++vpi)
        (*mask)(*vpi) = true;
}

// Identifies the smallest movement circle around the given source that can
// be shifted without breaking up any vaults.
static void _forest_identify_area_to_shift(coord_def source, int radius,
                                          map_mask *mask)
{
    mask->init(false);

    std::set<int> affected_vault_indexes;
    for (radius_iterator ri(source, radius, C_SQUARE); ri; ++ri)
    {
        if (!map_bounds_with_margin(*ri, MAPGEN_BORDER))
            continue;

        (*mask)(*ri) = true;

        const int map_index = env.level_map_ids(*ri);
        if (map_index != INVALID_MAP_INDEX)
            affected_vault_indexes.insert(map_index);
    }

    for (std::set<int>::const_iterator i = affected_vault_indexes.begin();
         i != affected_vault_indexes.end(); ++i)
    {
        _forest_expand_mask_to_cover_vault(mask, *i);
    }
}

static void _forest_invert_mask(map_mask *mask)
{
    for (rectangle_iterator ri(0); ri; ++ri)
        (*mask)(*ri) = !(*mask)(*ri);
}

// Moves everything in the given radius around the player (where radius=0 =>
// only the player) to another part of the level, centred on target_centre.
// Everything not in the given radius is wiped to DNGN_UNSEEN and the provided
// map_mask is set to true for all wiped squares.
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
static void _forest_shift_level_contents(
    int radius,
    coord_def target_centre,
    map_mask &forest_destruction_mask)
{
    const coord_def source_centre = you.pos();

//    for_state.major_coord += (source_centre - FOREST_CENTRE);

    ASSERT(radius >= LOS_RADIUS);
#if 0
#ifdef WIZARD
    // This should only really happen due to wizmode blink/movement.
    if (!map_bounds_with_margin(source_centre, radius))
        mprf("source_centre(%d, %d) outside map radius %d", source_centre.x, source_centre.y, radius);
    if (!map_bounds_with_margin(target_centre, radius))
        mprf("target_centre(%d, %d) outside map radius %d", target_centre.x, target_centre.y, radius);
#else
    ASSERT(map_bounds_with_margin(source_centre, radius));
    ASSERT(map_bounds_with_margin(target_centre, radius));
#endif
#endif

    _forest_identify_area_to_shift(source_centre, radius,
                                  &forest_destruction_mask);

#if 0
    // Shift sanctuary centre if it's close.
    _forest_move_sanctuary(you.pos(), target_centre);

    // Zap everything except the area we're shifting, so that there's
    // nothing in the way of moving stuff.
    _forest_wipe_unmasked_area(forest_destruction_mask);

    // Move stuff to its new home. This will also move the player.
    _forest_move_entities(target_centre, &forest_destruction_mask);

    // [ds] Rezap everything except the shifted area. NOTE: the old
    // code did not do this, leaving a repeated swatch of Abyss behind
    // at the old location for every shift; discussions between Linley
    // and dpeg on ##crawl confirm that this (repeated swatch of
    // terrain left behind) was not intentional.
    _forest_wipe_unmasked_area(forest_destruction_mask);
#endif

    // So far we've used the mask to track the portions of the level we're
    // preserving. The inverse of the mask represents the area to be filled
    // with brand new forest:
    _forest_invert_mask(&forest_destruction_mask);

#if 0
    // Update env.level_vaults to discard any vaults that are no longer in
    // the picture.
    dgn_erase_unused_vault_placements();
#endif
}

static void _forest_generate_plants(int nplants)
{
    mgen_data mons;
    mons.proximity  = PROX_AWAY_FROM_PLAYER;
    mons.map_mask   = MMT_VAULT;

    for (int mcount = 0; mcount < nplants; mcount++)
    {
        mons.cls = coinflip() ? MONS_PLANT : MONS_BUSH;
        mons_place(mons);
    }
}

void maybe_shift_forest()
{
    ASSERT(player_in_branch(BRANCH_FOREST));
    if (map_bounds_with_margin(you.pos(),
                               MAPGEN_BORDER + FOREST_AREA_SHIFT_RADIUS + 1))
    {
        return;
    }

    dprf("Shifting forest at (%d,%d)", you.pos().x, you.pos().y);

    forest_area_shift();
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

static dungeon_feature_type _forest_grid(double x, double y, double depth)
{
    const dungeon_feature_type terrain_elements[] =
    {
        DNGN_TREE,
    };
    const int n_terrain_elements = ARRAYSZ(terrain_elements);

    const double scale = 1.0 / 2.2;
    const double sub_scale_x = 17.0;
    const double sub_scale_y = 31.0;
    const double sub_scale_depth = 0.1;
    const int depth_margin = (10 + 2*(you.depth));

    worley::noise_datum noise = worley::worley(x * scale, y * scale, depth);
    dungeon_feature_type feat = DNGN_FLOOR;

    worley::noise_datum sub_noise = worley::worley(x * sub_scale_x,
                                                   y * sub_scale_y,
                                                   depth * sub_scale_depth);

    int dist = noise.distance[0] * 100;
    bool isWall = (dist > (100+depth_margin) || dist < (100-depth_margin));

    if (noise.id[0] + noise.id[1] % 2  == 0)
        isWall = sub_noise.id[0] % 2;

    if (sub_noise.id[0] % 3 == 0)
        isWall = !isWall;

    if (isWall)
    {
        int fuzz = (sub_noise.id[1] % 3 ? 0 : sub_noise.id[1] % 2 + 1);
        int id = (noise.id[0] + fuzz) % n_terrain_elements;
        feat = terrain_elements[id];
    }

    return feat;
}

static void _forest_apply_terrain(const map_mask &forest_genlevel_mask,
                                 double old_depth = 0.0)
{
    const coord_def major_coord = for_state.major_coord;
    const double forest_depth = for_state.depth;

    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        const coord_def p(*ri);

        if (you.pos() == p || !forest_genlevel_mask(p))
            continue;

        // Don't decay vaults.
        if (map_masked(p, MMT_VAULT))
            continue;

        // Don't morph altars and stairs.
        if (feat_is_staircase(grd(p)) || feat_is_altar(grd(p)))
            continue;

        double x = (p.x + major_coord.x);
        double y = (p.y + major_coord.y);

        // What should have been there previously?  It might not be because
        // of external changes such as digging.
        const dungeon_feature_type oldfeat = _forest_grid(x, y, old_depth);
        const dungeon_feature_type feat = _forest_grid(x, y, forest_depth);

        // If the selected grid is already there, *or* if we're morphing and
        // the selected grid should have been there, do nothing.
        if (feat != grd(p) && (feat != oldfeat))
        {
            grd(p) = feat;

            if (grd(p) == DNGN_TREE)
                dgn_set_grid_colour_at(p, ETC_TREE);
            else if (grd(p) == DNGN_FLOOR)
            {
                if (coinflip())
                {
                    dgn_set_grid_colour_at(p, GREEN);
                    env.tile_flv(p).floor = TILE_FLOOR_WOODGROUND + random2(9);
                }
                else
                {
                    dgn_set_grid_colour_at(p, BROWN);
                    env.tile_flv(p).floor = TILE_FLOOR_DIRT + random2(3);
                }
            }

            monster* mon = monster_at(p);
            if (mon && !monster_habitable_grid(mon, feat))
                _forest_lose_monster(*mon);
        }

    }
}

static void _generate_area(const map_mask &forest_genlevel_mask)
{
    _forest_apply_terrain(forest_genlevel_mask);

    setup_environment_effects();
}

static void forest_area_shift(void)
{
#ifdef DEBUG_FOREST
    dprf("area_shift() - player at pos (%d, %d)",
         you.pos().x, you.pos().y);
#endif

    {
        // Use a map mask to track the areas that the shift destroys and
        // that must be regenerated by _generate_area.
        map_mask forest_genlevel_mask;
        _forest_shift_level_contents(
            FOREST_AREA_SHIFT_RADIUS, FOREST_CENTRE, forest_genlevel_mask);
        forget_map(true);
        _generate_area(forest_genlevel_mask);

        // Update LOS at player's new forestal vacation retreat.
        los_changed();
    }

    // Place some plants to keep up the decor.
    _forest_generate_plants(6);

    // Allow monsters in transit another chance to return.
    place_transiting_monsters();
    place_transiting_items();

    check_map_validity();
}

void initialize_forest_state()
{
    for_state.major_coord.x = random2(0x7FFFFFFF);
    for_state.major_coord.y = random2(0x7FFFFFFF);
    for_state.phase = 0.0;
    for_state.depth = (double)(you.depth - 1);
}

void forest_morph(double duration)
{
    if (!player_in_branch(BRANCH_FOREST))
        return;

    // Between .02 and .07 per ten ticks, half that for Chei worshippers.
    double delta_t = you.time_taken * (you.abyss_speed + 40.0) / 20000.0;
    if (you.religion == GOD_CHEIBRIADOS)
        delta_t /= 2.0;

    const double theta = for_state.phase;
    const double old_depth = for_state.depth;

    // Up to 3 times the old rate of change, as low as 1/5, with an average of
    // 89%.  Period of 2*pi, so from 90 to 314 turns depending on forest speed
    // (double for Chei worshippers).  Starts in the middle of a cool period.
    // Increasing the exponent reduces the lengths of the unstable periods.  It
    // should be an even integer.
    for_state.depth += delta_t * (0.2 + 2.8 * pow(sin(theta/2), 10.0));

    // Phase mod pi.
    for_state.phase += delta_t;
    if (for_state.phase > M_PI)
        for_state.phase -= M_PI;

    map_mask forest_genlevel_mask;
    _forest_identify_area_to_shift(you.pos(), FOREST_AREA_SHIFT_RADIUS,
                                  &forest_genlevel_mask);
    _forest_invert_mask(&forest_genlevel_mask);
    dgn_erase_unused_vault_placements();
    _forest_apply_terrain(forest_genlevel_mask, old_depth);
    _push_items();
    los_changed();
}
