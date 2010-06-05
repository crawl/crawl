/*
 *  File:       abyss.cc
 *  Summary:    Misc abyss specific functions.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "abyss.h"

#include <cstdlib>
#include <algorithm>

#include "areas.h"
#include "artefact.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "flood_find.h"
#include "los.h"
#include "makeitem.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "mon-iter.h"
#include "mon-util.h"
#include "mon-place.h"
#include "mgen_data.h"
#include "coord.h"
#include "mon-transit.h"
#include "player.h"
#include "dungeon.h"
#include "items.h"
#include "l_defs.h"
#include "lev-pand.h"
#include "los.h"
#include "random.h"
#include "religion.h"
#include "showsymb.h"
#include "sprint.h"
#include "state.h"
#include "stuff.h"
#include "env.h"
#include "spells3.h"
#include "terrain.h"
#ifdef USE_TILE
 #include "tiledef-dngn.h"
 #include "tileview.h"
#endif
#include "traps.h"
#include "travel.h"
#include "view.h"
#include "xom.h"

const coord_def ABYSS_CENTRE(GXM / 2, GYM / 2);
const int ABYSSAL_RUNE_MAX_ROLL = 200;

// If not_seen is true, don't place the feature where it can be seen from
// the centre.
static bool _place_feature_near(const coord_def &centre,
                                int radius,
                                dungeon_feature_type candidate,
                                dungeon_feature_type replacement,
                                int tries, bool not_seen = false)
{
    const int radius2 = radius * radius + 1;
    for (int i = 0; i < tries; ++i)
    {
        const coord_def &cp =
            centre + coord_def(random_range(-radius, radius),
                               random_range(-radius, radius));

        if (cp == centre || (cp - centre).abs() > radius2 || !in_bounds(cp))
            continue;

        if (not_seen && cell_see_cell(cp, centre))
            continue;

        if (grd(cp) == candidate)
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Placing %s at (%d,%d)",
                 dungeon_feature_name(replacement),
                 cp.x, cp.y);
#endif
            grd(cp) = replacement;
            return (true);
        }
    }
    return (false);
}

//#define DEBUG_ABYSS

// Public for abyss generation.
void generate_abyss()
{
    extern std::string dgn_Build_Method;

    dgn_Build_Method += " abyss";
    dgn_Layout_Type   = "abyss";

#ifdef DEBUG_ABYSS
    mprf(MSGCH_DIAGNOSTICS,
         "generate_abyss(); turn_on_level: %d", env.turns_on_level);
#endif

    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        grd(*ri) =
            static_cast<dungeon_feature_type>(
                random_choose_weighted(3000, DNGN_FLOOR,
                                        600, DNGN_ROCK_WALL,
                                        300, DNGN_STONE_WALL,
                                        100, DNGN_METAL_WALL,
                                          1, DNGN_CLOSED_DOOR,
                                          0));
    }


    // If we're starting out in the Abyss, make sure the starting grid is
    // an altar to Lugonu and there's an exit near-by.
    // Otherwise, we start out on floor and there's a chance there's an
    // altar near-by.
    if (you.char_direction == GDT_GAME_START)
    {
        grd(ABYSS_CENTRE) = DNGN_ALTAR_LUGONU;
        _place_feature_near( ABYSS_CENTRE, LOS_RADIUS + 2,
                             DNGN_FLOOR, DNGN_EXIT_ABYSS, 50, true );
    }
    else
    {
        grd(ABYSS_CENTRE) = DNGN_FLOOR;
        if (one_chance_in(5))
        {
            _place_feature_near( ABYSS_CENTRE, LOS_RADIUS,
                                 DNGN_FLOOR, DNGN_ALTAR_LUGONU, 50 );
        }
    }
}

// Returns the roll to use to check if we want to create an abyssal rune.
static int _abyssal_rune_roll()
{
    if (you.attribute[ATTR_ABYSSAL_RUNES])
        return (-1);

    // The longer the player's hung around in the Abyss, the more
    // likely the rune. Never generate a new rune if the player
    // already found one, but make the Abyssal rune eligible for
    // generation again if the player loses it.

    // If the player leaves the Abyss turns_on_level resets to 0. So
    // hang in there if you want your Abyssal rune fix quick. :P

    // Worshippers of Lugonu with decent piety will attract the rune
    // to themselves.

    // In general, the base chance of an abyssal rune is 1/200 for
    // every item roll (the item roll is 1/200 for every floor square
    // in the abyss). Once the player has spent more than 500 turns on
    // level, the abyss rune chance increases linearly every 228 turns
    // up to a maximum of 34/200 (17%) per item roll after ~8000
    // turns.

    // For Lugonu worshippers, the base chance of an abyssal rune is 1
    // in 200 as for other players, but the rune chance increases
    // linearly after 50 turns on the level, up to the same 34/200
    // (17%) chance after ~2000 turns.

    const bool lugonu_favoured =
        (you.religion == GOD_LUGONU && !player_under_penance()
         && you.piety > 120);

    const int cutoff = lugonu_favoured ? 50 : 500;
    const int scale = lugonu_favoured ? 57 : 228;

    const int odds =
        std::min(1 + std::max((env.turns_on_level - cutoff) / scale, 0), 34);
#ifdef DEBUG_ABYSS
    mprf(MSGCH_DIAGNOSTICS, "Abyssal rune odds: %d in %d (%.2f%%)",
         odds, ABYSSAL_RUNE_MAX_ROLL, odds * 100.0 / ABYSSAL_RUNE_MAX_ROLL);
#endif
    return (odds);
}

static void _abyss_create_room(const map_mask &abyss_genlevel_mask)
{
    const int largest_room_dimension = 10;

    // Pick the corners
    const coord_def tl(
        random_range(MAPGEN_BORDER,
                     GXM - 1 - (MAPGEN_BORDER + largest_room_dimension)),
        random_range(MAPGEN_BORDER,
                     GYM - 1 - (MAPGEN_BORDER + largest_room_dimension)));
    const coord_def roomsize(random_range(1, largest_room_dimension),
                             random_range(1, largest_room_dimension));
    const coord_def br = tl + roomsize;

    // Check if the room is taken.
    for (rectangle_iterator ri(tl, br); ri; ++ri)
        if (!abyss_genlevel_mask(*ri))
            return;

    // Make the room.
    for (rectangle_iterator ri(tl, br); ri; ++ri)
        grd(*ri) = DNGN_FLOOR;
}

static void _abyss_create_rooms(const map_mask &abyss_genlevel_mask,
                                int rooms_to_do)
{
    for (int i = 0; i < rooms_to_do; ++i)
    {
        if (one_chance_in(100))
            break;
        _abyss_create_room(abyss_genlevel_mask);
    }
}

static bool _abyss_place_rune(const map_mask &abyss_genlevel_mask)
{
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
                                  MISC_RUNE_OF_ZOT, true,
                                  DEPTH_ABYSS, 0);
        if (thing_created != NON_ITEM)
        {
            mitm[thing_created].plus = RUNE_ABYSSAL;
            item_colour(mitm[thing_created]);
        }
        move_item_to_grid( &thing_created, chosen_spot );
        return (thing_created != NON_ITEM);
    }

    return (false);
}

static int _abyss_create_items(const map_mask &abyss_genlevel_mask,
                               bool placed_abyssal_rune)
{
    // During game start, number and level of items mustn't be higher than
    // that on level 1. Abyss in sprint games has no items.
    int num_items = 150, items_level = DEPTH_ABYSS;
    int items_placed = 0;

    if (crawl_state.game_is_sprint())
    {
        num_items   = 0;
        items_level = 0;
    }
    else if (you.char_direction == GDT_GAME_START)
    {
        num_items   = 3 + roll_dice( 3, 11 );
        items_level = 0;
    }

    const int abyssal_rune_roll = _abyssal_rune_roll();
    bool should_place_abyssal_rune = false;
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        if (abyss_genlevel_mask(*ri) && grd(*ri) == DNGN_FLOOR)
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

                int thing_created = items(1, OBJ_RANDOM, OBJ_RANDOM,
                                          true, items_level, 250);
                move_item_to_grid( &thing_created, *ri );

                if (thing_created != NON_ITEM)
                    items_placed++;
            }
        }
    }

    if (!placed_abyssal_rune && should_place_abyssal_rune)
    {
        if (_abyss_place_rune(abyss_genlevel_mask))
            ++items_placed;
    }

    return (items_placed);
}

static std::vector<dungeon_feature_type> _abyss_pick_terrain_elements()
{
    std::vector<dungeon_feature_type> terrain_elements;

    const int n_terrain_elements = 5;

    // Generate level composition vector.
    for (int i = 0; i < n_terrain_elements; i++)
    {
        // Weights are in hundredths of a percentage; i.e. 5073 =
        // 50.73%, 16 = 0.16%, etc.
        terrain_elements.push_back(
            static_cast<dungeon_feature_type>(
                random_choose_weighted(5073, DNGN_ROCK_WALL,
                                       2008, DNGN_STONE_WALL,
                                       914, DNGN_METAL_WALL,
                                       722, DNGN_LAVA,
                                       666, DNGN_SHALLOW_WATER,
                                       601, DNGN_DEEP_WATER,
                                       16, DNGN_CLOSED_DOOR,
                                       0)));
    }
    return (terrain_elements);
}

// Returns N so that the chance of placing an abyss exit on any given
// square is 1 in N.
static int _abyss_exit_chance()
{
    int exit_chance = 7500;
    if (crawl_state.game_is_sprint())
    {
        exit_chance = sprint_modify_abyss_exit_chance(exit_chance);
    }
    return exit_chance;
}

static bool _abyss_check_place_feat(coord_def p,
                                    const int feat_chance,
                                    int *feats_wanted,
                                    dungeon_feature_type which_feat)
{
    if (!which_feat)
        return (false);

    const bool place_feat = feat_chance && one_chance_in(feat_chance);

    if (place_feat && feats_wanted)
        ++*feats_wanted;

    // There's no longer a need to check for features under items,
    // since we're working on fresh grids that are guaranteed
    // item-free.
    if (place_feat || (feats_wanted && *feats_wanted > 0))
    {
        dprf("Placing abyss feature: %s.", dungeon_feature_name(which_feat));
        grd(p) = which_feat;
        if (feats_wanted)
            --*feats_wanted;
        return (true);
    }
    return (false);
}

static dungeon_feature_type _abyss_pick_altar()
{
    dungeon_feature_type altar = DNGN_UNSEEN;

    // Lugonu has a flat 50% chance of corrupting the altar.
    if (coinflip())
        return DNGN_ALTAR_LUGONU;

    do
    {
        altar = static_cast<dungeon_feature_type>(
            DNGN_ALTAR_FIRST_GOD + random2(NUM_GODS - 1));
    }
    while (is_good_god(feat_altar_god(altar))
           || is_unavailable_god(feat_altar_god(altar)));

    return (altar);
}

static void _abyss_apply_terrain(const map_mask &abyss_genlevel_mask)
{
    const std::vector<dungeon_feature_type> terrain_elements =
        _abyss_pick_terrain_elements();

    if (one_chance_in(3))
        _abyss_create_rooms(abyss_genlevel_mask, random_range(1, 10));

    const int exit_chance = _abyss_exit_chance();

    // Except for the altar on the starting position, don't place any
    // altars.
    const int altar_chance =
        you.char_direction != GDT_GAME_START? 10000 : 0;

    const int n_terrain_elements = terrain_elements.size();
    int exits_wanted  = 0;
    int altars_wanted = 0;

    const int floor_density = random_range(30, 95);
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        const coord_def p(*ri);

        if (!abyss_genlevel_mask(p))
            continue;

        if (x_chance_in_y(floor_density, 100))
            grd(p) = DNGN_FLOOR;
        else if (grd(p) == DNGN_UNSEEN)
            grd(p) = terrain_elements[random2(n_terrain_elements)];

        // Place abyss exits, stone arches, and altars to liven up the scene:
        (_abyss_check_place_feat(p, exit_chance, &exits_wanted, DNGN_EXIT_ABYSS)
         ||
         _abyss_check_place_feat(p, altar_chance, &altars_wanted,
                                 _abyss_pick_altar())
         ||
         _abyss_check_place_feat(p, 10000, NULL, DNGN_STONE_ARCH));
    }
}

static void _generate_area(map_mask &abyss_genlevel_mask,
                           bool spatter = false)
{
    // Any rune on the floor prevents the abyssal rune from being generated.
    const bool placed_abyssal_rune =
        find_floor_item(OBJ_MISCELLANY, MISC_RUNE_OF_ZOT);

#ifdef DEBUG_ABYSS
    mprf(MSGCH_DIAGNOSTICS,
         "_generate_area(). turns_on_level: %d, rune_on_floor: %s",
         env.turns_on_level, placed_abyssal_rune? "yes" : "no");
#endif

    // Nuke map knowledge and properties.
    env.map_knowledge.init(map_cell());
    env.pgrid.init(0);
    _abyss_apply_terrain(abyss_genlevel_mask);
    _abyss_create_items(abyss_genlevel_mask, placed_abyssal_rune);
    generate_random_blood_spatter_on_level(&abyss_genlevel_mask);
    setup_environment_effects();
}

class xom_abyss_feature_amusement_check
{
private:
    int exit_was_near;
    int rune_was_near;

private:
    int abyss_exit_nearness() const
    {
        int nearness = INFINITE_DISTANCE;
        // is_terrain_known() doesn't work on unmappable levels because
        // mapping flags are not set on such levels.
        for (radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri)
            if (grd(*ri) == DNGN_EXIT_ABYSS && get_screen_glyph(*ri) != ' ')
                nearness = std::min(nearness, grid_distance(you.pos(), *ri));

        return (nearness);
    }

    int abyss_rune_nearness() const
    {
        int nearness = INFINITE_DISTANCE;
        // See above comment about is_terrain_known().
        for (radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri)
        {
            if (get_screen_glyph(*ri) != ' ')
            {
                for (stack_iterator si(*ri); si; ++si)
                    if (is_rune(*si) && si->plus == RUNE_ABYSSAL)
                        nearness = std::min(nearness,
                                            grid_distance(you.pos(),*ri));
            }
        }
        return (nearness);
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
        viewwindow(false);

        const int exit_is_near = abyss_exit_nearness();
        const int rune_is_near = abyss_rune_nearness();

        if (exit_was_near < INFINITE_DISTANCE
            && exit_is_near == INFINITE_DISTANCE
            || rune_was_near < INFINITE_DISTANCE
            && rune_is_near == INFINITE_DISTANCE
            && you.attribute[ATTR_ABYSSAL_RUNES] == 0)
        {
            xom_is_stimulated(255, "Xom snickers loudly.", true);
        }

        if (rune_was_near == INFINITE_DISTANCE
            && rune_is_near < INFINITE_DISTANCE
            && you.attribute[ATTR_ABYSSAL_RUNES] == 0
            || exit_was_near == INFINITE_DISTANCE
            && exit_is_near < INFINITE_DISTANCE)
        {
            xom_is_stimulated(255);
        }
    }
};

static void _abyss_lose_monster(monsters &mons)
{
    if (mons.needs_transit())
        mons.set_transit( level_id(LEVEL_ABYSS) );

    mons.destroy_inventory();
    mons.reset();
}

typedef
FixedArray<terrain_property_t, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER>
los_env_props_t;

// Get env props in player LOS.
static los_env_props_t _env_props_in_los()
{
    los_env_props_t los_env_props(0);
    const coord_def losoffset_youpos = you.pos() + ENV_SHOW_OFFSET;
    for (radius_iterator ri(you.get_los()); ri; ++ri)
        los_env_props(losoffset_youpos - *ri) = env.pgrid(*ri);
    return los_env_props;
}

static void _env_props_apply_to_los(const los_env_props_t &los_env_props)
{
    const coord_def losoffset_youpos = you.pos() + ENV_SHOW_OFFSET;
    for (radius_iterator ri(you.get_los()); ri; ++ri)
        env.pgrid(*ri) = los_env_props(losoffset_youpos - *ri);
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
            env.sanctuary_pos += (abyss_shift_end_centre -
                                  abyss_shift_start_centre);
        else
            remove_sanctuary(false);
    }
}

// Removes monsters, clouds, dungeon features, and items from the
// level. If a non-negative preservation radius is specified, things
// within that radius (as measured in movement distance from the
// player's current position) will not be affected.
static void _abyss_wipe_area(int preserved_radius_around_player,
                             map_mask &abyss_destruction_mask)
{
    const coord_def youpos = you.pos();

    // Remove non-nearby monsters.
    for (monster_iterator mi; mi; ++mi)
    {
        if (grid_distance(mi->pos(), youpos) > preserved_radius_around_player)
            _abyss_lose_monster(**mi);
    }

    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        // Don't modify terrain by player.
        if (grid_distance(*ri, you.pos()) <= preserved_radius_around_player)
            continue;

        abyss_destruction_mask(*ri) = true;

        // Nuke terrain.
        grd(*ri) = DNGN_UNSEEN;

        // Nuke items.
#ifdef DEBUG_ABYSS
        if (igrd(*ri) != NON_ITEM)
        {
            const coord_def &p(*ri);
            mprf(MSGCH_DIAGNOSTICS, "Nuke item stack at (%d, %d)", p.x, p.y);
        }
#endif
        lose_item_stack( *ri );
    }

    for (int i = 0; i < MAX_CLOUDS; i++)
    {
        if (env.cloud[i].type == CLOUD_NONE)
            continue;

        if (grid_distance(youpos, env.cloud[i].pos) >
            preserved_radius_around_player)
        {
            delete_cloud( i );
        }
    }
}

// Moves the player, monsters, terrain and items in the square (circle
// in movement distance) around the player with the given radius to
// the square centred on target_centre.
//
// Assumes:
// a) both areas are fully in bounds
// b) source and target areas do not overlap
// c) the target area is free of monsters, clouds and items.
static void _abyss_move_entities(int radius, coord_def target_centre)
{
    const coord_def source_centre = you.pos();

    // Shift all monsters and items to new area.
    for (radius_iterator ri(source_centre, radius, C_SQUARE); ri; ++ri)
    {
        const coord_def newpos = target_centre + *ri - source_centre;

        // Move terrain.
        grd(newpos) = grd(*ri);

        // Move item.
#ifdef DEBUG_ABYSS
        if (igrd(*ri) != NON_ITEM)
        {
            mprf(MSGCH_DIAGNOSTICS,
                 "Move item stack from (%d, %d) to (%d, %d)",
                 ri->x, ri->y, newpos.x, newpos.y);
        }
#endif
        move_item_stack_to_grid(*ri, newpos);

        // Move monster.
        if (monster_at(*ri))
        {
            menv[mgrd(*ri)].moveto(newpos);
            mgrd(newpos) = mgrd(*ri);
            mgrd(*ri) = NON_MONSTER;
        }

        // Move cloud,
        if (env.cgrid(*ri) != EMPTY_CLOUD)
            move_cloud( env.cgrid(*ri), newpos );
    }
    you.shiftto(target_centre);
}

// Moves everything in the given radius around the player (where radius=0 =>
// only the player) to another part of the level, centred on target_centre.
// Everything not in the given radius is wiped to DNGN_UNSEEN and the provided
// map_mask is set to true for all wiped squares.
//
// Assumes:
// a) radius >= LOS_RADIUS
// b) All points in the source and target areas are in bounds.
static void _abyss_shift_level_contents_around_player(
    int radius,
    coord_def target_centre,
    map_mask &abyss_destruction_mask)
{
    const coord_def source_centre = you.pos();

    ASSERT(radius >= LOS_RADIUS);
    ASSERT(map_bounds_with_margin(source_centre, radius));
    ASSERT(map_bounds_with_margin(target_centre, radius));

    // Shift sanctuary centre if it's close.
    _abyss_move_sanctuary(you.pos(), target_centre);

    // Zap everything except the area we're shifting, so that there's
    // nothing in the way of moving stuff.
    _abyss_wipe_area(radius, abyss_destruction_mask);

    // Move stuff to its new home. This will also move the player.
    _abyss_move_entities(radius, target_centre);

    // Keep track of wiped areas. This discards the mask set by the
    // last wipe, which is obsolete.
    abyss_destruction_mask.init(false);

    // [ds] Rezap everything except the shifted area. NOTE: the old
    // code did not do this, leaving a repeated swatch of Abyss behind
    // at the old location for every shift; discussions between Linley
    // and dpeg on ##crawl confirm that this (repeated swatch of
    // terrain left behind) was not intentional.
    _abyss_wipe_area(radius, abyss_destruction_mask);
}

static void _abyss_generate_monsters(int nmonsters)
{
    mgen_data mons;
    mons.level_type = LEVEL_ABYSS;
    mons.proximity  = PROX_AWAY_FROM_PLAYER;

    for (int mcount = 0; mcount < nmonsters; mcount++)
        mons_place(mons);
}

void abyss_area_shift(void)
{
#ifdef DEBUG_ABYSS
    mprf(MSGCH_DIAGNOSTICS, "area_shift() - player at pos (%d, %d)",
         you.pos().x, you.pos().y);
#endif

    {
        xom_abyss_feature_amusement_check xomcheck;

        // Preserve floor props around the player, primarily so that
        // blood-splatter doesn't appear out of nowhere when doing an
        // area shift.
        const los_env_props_t los_env_props = _env_props_in_los();

        // Use a map mask to track the areas that the shift destroys and
        // that must be regenerated by _generate_area.
        map_mask abyss_genlevel_mask;
        _abyss_shift_level_contents_around_player(
            ABYSS_AREA_SHIFT_RADIUS, ABYSS_CENTRE, abyss_genlevel_mask);
        _generate_area(abyss_genlevel_mask, true);

        // Update LOS at player's new abyssal vacation retreat.
        los_changed();
        _env_props_apply_to_los(los_env_props);
    }

    // Place some monsters to keep the abyss party going.
    _abyss_generate_monsters(15);

    // And allow monsters in transit another chance to return.
    place_transiting_monsters();
    place_transiting_items();
}

void save_abyss_uniques()
{
    for (monster_iterator mi; mi; ++mi)
        if (mi->needs_transit())
            mi->set_transit(level_id(LEVEL_ABYSS));
}

static void _abyss_generate_new_area()
{
    remove_sanctuary(false);

    // Get new monsters and colours.
    init_pandemonium();
#ifdef USE_TILE
    tile_init_flavour();
#endif

    map_mask abyss_genlevel_mask;
    _abyss_wipe_area(-1, abyss_genlevel_mask);
    ASSERT( env.cloud_no == 0 );

    you.moveto(ABYSS_CENTRE);
    _generate_area(abyss_genlevel_mask, true);
    grd(you.pos()) = DNGN_FLOOR;
    if (one_chance_in(5))
    {
        _place_feature_near( you.pos(), LOS_RADIUS,
                             DNGN_FLOOR, DNGN_ALTAR_LUGONU, 50 );
    }

    los_changed();
    place_transiting_monsters();
    place_transiting_items();
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
#ifdef DEBUG_ABYSS
            mprf(MSGCH_DIAGNOSTICS,
                 "Abyss same-area teleport to (%d,%d).",
                 newspot.x, newspot.y);
#endif
            you.moveto(newspot);
            return (true);
        }
    }
    return (false);
}

void abyss_teleport( bool new_area )
{
    xom_abyss_feature_amusement_check xomcheck;

    if (!new_area && _abyss_teleport_within_level())
        return;

#ifdef DEBUG_ABYSS
    mpr("New area Abyss teleport.", MSGCH_DIAGNOSTICS);
#endif

    // Teleport to a new area of the abyss.
    _abyss_generate_new_area();
}

//////////////////////////////////////////////////////////////////////////////
// Abyss effects in other levels, courtesy Lugonu.

static void _place_corruption_seed(const coord_def &pos, int duration)
{
    env.markers.add(new map_corruption_marker(pos, duration));
}

static void _initialise_level_corrupt_seeds(int power)
{
    const int low = power * 40 / 100, high = power * 140 / 100;
    const int nseeds = random_range(-1, std::min(2 + power / 110, 4), 2);

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
            if (grd(where) == DNGN_FLOOR
                && !env.markers.find(where, MAT_ANY))
            {
                break;
            }
            where.reset();
        }

        if (!where.origin())
            _place_corruption_seed(where, random_range(low, high, 2) + 300);
    }
}

// Create a corruption spawn at the given position. Returns false if further
// monsters should not be placed near this spot (overcrowding), true if
// more monsters can fit in.
static bool _spawn_corrupted_servant_near(const coord_def &pos)
{
    const beh_type beh =
        one_chance_in(2 + you.skills[SK_INVOCATIONS] / 4) ? BEH_HOSTILE
        : BEH_NEUTRAL;

    // [ds] No longer summon hostiles -- don't create the monster if
    // it would be hostile.
    if (beh == BEH_HOSTILE)
        return (true);

    // Thirty tries for a place.
    for (int i = 0; i < 30; ++i)
    {
        const int offsetX = random2avg(4, 3) + random2(3);
        const int offsetY = random2avg(4, 3) + random2(3);
        const coord_def p( pos.x + (coinflip()? offsetX : -offsetX),
                           pos.y + (coinflip()? offsetY : -offsetY) );
        if (!in_bounds(p) || actor_at(p)
            || !feat_compatible(DNGN_FLOOR, grd(p)))
        {
            continue;
        }

        // Got a place, summon the beast.
        monster_type mons = pick_random_monster(level_id(LEVEL_ABYSS));
        if (mons == MONS_PROGRAM_BUG)
            return (false);

        mgen_data mg(mons, beh, 0, 5, 0, p);
        mg.non_actor_summoner = "Lugonu's corruption";

        const int mid = create_monster(mg);
        return !invalid_monster_index(mid);
    }

    return (false);
}

static void _apply_corruption_effect(map_marker *marker, int duration)
{
    if (!duration)
        return;

    map_corruption_marker *cmark = dynamic_cast<map_corruption_marker*>(marker);
    if (cmark->duration < 1)
        return;

    const coord_def centre = cmark->pos;
    const int neffects = std::max(div_rand_round(duration, 5), 1);

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
    std::vector<map_marker*> markers =
        env.markers.get_all(MAT_CORRUPTION_NEXUS);

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
        return (false);

    const dungeon_feature_type feat = grd(c);

    // Stairs and portals cannot be corrupted.
    if (feat_stair_direction(feat) != CMD_NO_CMD)
        return (false);

    switch (feat)
    {
    case DNGN_PERMAROCK_WALL:
    case DNGN_GREEN_CRYSTAL_WALL:
        return (false);

    case DNGN_METAL_WALL:
        return (one_chance_in(5));

    case DNGN_STONE_WALL:
        return (one_chance_in(3));

    case DNGN_ROCK_WALL:
        return (!one_chance_in(3));

    default:
        return (true);
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
                return (false);
        }

    return (true);
}

// Returns true if the square has all opaque neighbours.
static bool _is_sealed_square(const coord_def &c)
{
    for (adjacent_iterator ai(c); ai; ++ai)
        if ( !feat_is_opaque(grd(*ai)) )
            return (false);

    return (true);
}

static void _corrupt_square(const crawl_environment &oenv, const coord_def &c)
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
        feat = oenv.grid(c);

    if (feat_is_trap(feat, true)
        || feat == DNGN_SECRET_DOOR || feat == DNGN_UNSEEN)
    {
        return;
    }

    if (feat_is_traversable(grd(c)) && !feat_is_traversable(feat)
        && _is_crowded_square(c))
    {
        return;
    }

    if (!feat_is_traversable(grd(c)) && feat_is_traversable(feat) && _is_sealed_square(c))
        return;

    // What's this supposed to achieve? (jpeg)
    // I mean, won't exits from the Abyss only turn up in the Abyss itself?
    if (feat == DNGN_EXIT_ABYSS)
        feat = DNGN_ENTER_ABYSS;

    dungeon_terrain_changed(c, feat, true, preserve_feat, true);
    if (feat == DNGN_ROCK_WALL)
        env.grid_colours(c) = oenv.rock_colour;
    else if (feat == DNGN_FLOOR)
        env.grid_colours(c) = oenv.floor_colour;

#ifdef USE_TILE
    if (feat == DNGN_ROCK_WALL)
    {
        env.tile_flv(c).wall = TILE_WALL_UNDEAD
            + random2(tile_dngn_count(TILE_WALL_UNDEAD));
    }
    else if (feat == DNGN_FLOOR)
    {
        env.tile_flv(c).floor = TILE_FLOOR_NERVES
            + random2(tile_dngn_count(TILE_FLOOR_NERVES));
    }
#endif
}

static void _corrupt_level_features(const crawl_environment &oenv)
{
    std::vector<coord_def> corrupt_seeds;
    std::vector<map_marker*> corrupt_markers =
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
            std::max(1, 100 - (idistance2 - ground_zero_radius2) * 70 / 57);

        if (random2(100) < corrupt_perc_chance && _is_grid_corruptible(*ri))
            _corrupt_square(oenv, *ri);
    }
}

static bool _is_level_corrupted()
{
    if (you.level_type == LEVEL_ABYSS)
        return (true);

    return (!!env.markers.find(MAT_CORRUPTION_NEXUS));
}

bool is_level_incorruptible()
{
    if (_is_level_corrupted())
    {
        mpr("This place is already infused with evil and corruption.");
        return (true);
    }

    return (false);
}

static void _corrupt_choose_colours()
{
    int colour = BLACK;
    do
        colour = random_uncommon_colour();
    while (colour == env.rock_colour || colour == LIGHTGREY || colour == WHITE);
    env.rock_colour = colour;

    do
        colour = random_uncommon_colour();
    while (colour == env.floor_colour || colour == LIGHTGREY
           || colour == WHITE);
    env.floor_colour = colour;
}

bool lugonu_corrupt_level(int power)
{
    if (is_level_incorruptible())
        return (false);

    mpr("Lugonu's Hand of Corruption reaches out!", MSGCH_GOD);

    flash_view(MAGENTA);

    _initialise_level_corrupt_seeds(power);

    std::auto_ptr<crawl_environment> backup(new crawl_environment(env));

    // Set up genlevel mask and fill the level with DNGN_UNSEEN so
    // _generate_area does its thing.
    map_mask abyss_genlevel_mask;
    abyss_genlevel_mask.init(true);
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
        grd(*ri) = DNGN_UNSEEN;
    _generate_area(abyss_genlevel_mask);
    los_changed();

    _corrupt_choose_colours();

    std::auto_ptr<crawl_environment> abyssal(new crawl_environment(env));
    env = *backup;
    backup.reset(NULL);
    dungeon_events.clear();
    env.markers.activate_all(false);

    _corrupt_level_features(*abyssal);
    run_corruption_effects(300);

#ifndef USE_TILE
    // Allow extra time for the flash to linger.
    delay(1000);
#endif

    return (true);
}
