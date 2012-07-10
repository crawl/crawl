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
static void _forest_identify_area_near_player(coord_def source, int radius,
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

// Once upon a time, this shifted the area around the player to the origin,
// a la the equivalent Abyss code.
// Now, it just sets up the mask where Forest tile swapping takes place.
static void _forest_setup_area_gen_mask(
    int radius,
    coord_def target_centre,
    map_mask &forest_destruction_mask)
{
    const coord_def source_centre = you.pos();

    ASSERT(radius >= LOS_RADIUS);

    _forest_identify_area_near_player(source_centre, radius,
                                      &forest_destruction_mask);

    _forest_invert_mask(&forest_destruction_mask);
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

// Swap pairs of directly-adjacent tiles in a manner that preserves
// connectivity.
static void _forest_apply_terrain(const map_mask &forest_genlevel_mask,
                                 double old_depth = 0.0)
{
    const double forest_depth = for_state.depth;
    int count = 0, swapcount = 0;
    std::vector<coord_def> neighbours;
    bool veto = false;

    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        const coord_def p(*ri);
        coord_def target;
        target.reset();

        if (you.pos() == p || !forest_genlevel_mask(p))
            continue;

        // Don't alter vaults.
        if (map_masked(p, MMT_VAULT))
            continue;

        if (cell_is_solid(p))
            continue;

        count = 0;

        neighbours.clear();

        for (adjacent_iterator ai(p); ai; ai++)
            if (!cell_is_solid(*ai))
                neighbours.push_back(*ai);

        for (adjacent_iterator ai(p); ai; ai++)
        {
            if (p.x != (*ai).x && p.y != (*ai).y)
                continue;

            if (you.pos() == *ai || !forest_genlevel_mask(*ai))
                continue;

            // Don't alter vaults.
            if (map_masked(*ai, MMT_VAULT))
                continue;

            if (cell_is_solid(*ai))
                continue;

            veto = false;
            for (unsigned int i = 0; i < neighbours.size(); i++)
                if (!adjacent(neighbours[i], *ai))
                {
                    veto = true;
                    break;
                }

            if (veto)
                continue;

            count++;
            if (!random2(count))
                target = *ai;
        }

        if (!target.origin()
            && x_chance_in_y(abs(1000*(forest_depth-old_depth)), 1000))
        {
            swapcount++;
            swap_features(p, target, true, false);
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
        _forest_setup_area_gen_mask(
            FOREST_AREA_SHIFT_RADIUS, FOREST_CENTRE, forest_genlevel_mask);
        forget_map(true);
        _generate_area(forest_genlevel_mask);

        // Update LOS at player's new forestal vacation retreat.
        los_changed();
    }

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
    _forest_identify_area_near_player(you.pos(), FOREST_AREA_SHIFT_RADIUS,
                                      &forest_genlevel_mask);
    _forest_invert_mask(&forest_genlevel_mask);
    dgn_erase_unused_vault_placements();
    _forest_apply_terrain(forest_genlevel_mask, old_depth);
    _push_items();
    los_changed();
}
