/*
 *  File:       abyss.cc
 *  Summary:    Misc functions (most of which don't appear to be related to priests).
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *     <2>     10/11/99       BCR     Added Daniel's crash patch
 *     <1>     -/--/--        LRH     Created
 */

#include "AppHdr.h"
#include "abyss.h"

#include <stdlib.h>
#include <algorithm>

#include "externs.h"

#include "cloud.h"
#include "makeitem.h"
#include "mapmark.h"
#include "message.h"
#include "monplace.h"
#include "mtransit.h"
#include "player.h"
#include "dungeon.h"
#include "items.h"
#include "lev-pand.h"
#include "randart.h"
#include "stuff.h"
#include "terrain.h"
#include "tiles.h"
#include "traps.h"
#include "view.h"
#include "xom.h"

static bool place_feature_near( const coord_def &centre,
                                int radius,
                                dungeon_feature_type candidate,
                                dungeon_feature_type replacement,
                                int tries )
{
    const int radius2 = radius * radius + 1;
    for (int i = 0; i < tries; ++i)
    {
        const coord_def &cp =
            centre + coord_def(random_range(-radius, radius),
                               random_range(-radius, radius));
        if (cp == centre || (cp - centre).abs() > radius2 || !in_bounds(cp))
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

// public for abyss generation
void generate_abyss(void)
{
    int i, j;                   // loop variables
    int temp_rand;              // probability determination {dlb}

#if DEBUG_ABYSS
    mpr("generate_abyss().", MSGCH_DIAGNOSTICS);
#endif

    for (i = 5; i < (GXM - 5); i++)
    {
        for (j = 5; j < (GYM - 5); j++)
        {
            temp_rand = random2(4000);

            grd[i][j] = ((temp_rand > 999) ? DNGN_FLOOR :       // 75.0%
                         (temp_rand > 400) ? DNGN_ROCK_WALL :   // 15.0%
                         (temp_rand > 100) ? DNGN_STONE_WALL :  //  7.5%
                         (temp_rand >   0) ? DNGN_METAL_WALL    //  2.5%
                                           : DNGN_CLOSED_DOOR); // 1 in 4000
        }
    }

    grd[45][35] = DNGN_FLOOR;
    if ( one_chance_in(5) )
        place_feature_near( coord_def(45, 35), LOS_RADIUS,
                            DNGN_FLOOR, DNGN_ALTAR_LUGONU, 50 );
}

static void generate_area(int gx1, int gy1, int gx2, int gy2)
{
#if DEBUG_ABYSS
    mpr("generate_area().", MSGCH_DIAGNOSTICS);
#endif

    int items_placed = 0;
    const int thickness = random2(70) + 30;
    int thing_created;

    FixedVector<dungeon_feature_type, 5> replaced;

    // nuke map
    env.map.init(map_cell());

    // generate level composition vector
    for (int i = 0; i < 5; i++)
    {
        const int temp_rand = random2(10000);

        replaced[i] = ((temp_rand > 4926) ? DNGN_ROCK_WALL :    // 50.73%
                       (temp_rand > 2918) ? DNGN_STONE_WALL :   // 20.08%
                       (temp_rand > 2004) ? DNGN_METAL_WALL :   //  9.14%
                       (temp_rand > 1282) ? DNGN_LAVA :         //  7.22%
                       (temp_rand > 616)  ? DNGN_SHALLOW_WATER ://  6.66%
                       (temp_rand > 15)   ? DNGN_DEEP_WATER     //  6.01%
                                          : DNGN_CLOSED_DOOR);  //  0.16%
    }

    if (one_chance_in(3))
    {
        int rooms_to_do = 1 + random2(10);

        for ( int rooms_done = 0; rooms_done < rooms_to_do; ++rooms_done )
        {
            const int x1 = 10 + random2(GXM - 20);
            const int y1 = 10 + random2(GYM - 20);
            const int x2 = x1 + 1 + random2(10);
            const int y2 = y1 + 1 + random2(10);

            if (one_chance_in(100))
                break;

            bool room_ok = true;

            for (int i = x1; room_ok && i < x2; i++)
                for (int j = y1; room_ok && j < y2; j++)
                    if (grd[i][j] != DNGN_UNSEEN)
                        room_ok = false;

            if ( room_ok )
                for (int i = x1; i < x2; i++)
                    for (int j = y1; j < y2; j++)
                        grd[i][j] = DNGN_FLOOR;
        }
    }

    for (int i = gx1; i <= gx2; i++)
    {
        for (int j = gy1; j <= gy2; j++)
        {
            if (grd[i][j] == DNGN_UNSEEN && random2(100) <= thickness)
            {
                grd[i][j] = DNGN_FLOOR;

                if (items_placed < 150 && one_chance_in(200))
                {
                    if (one_chance_in(200))
                    {
                        thing_created = items(1, OBJ_MISCELLANY,
                                              MISC_RUNE_OF_ZOT, true, 51, 51);
#if DEBUG_ABYSS
                        mpr("Placing an Abyssal rune.", MSGCH_DIAGNOSTICS);
#endif
                    }
                    else
                    {
                        thing_created = items(1, OBJ_RANDOM,
                                              OBJ_RANDOM, true, 51, 250);
                    }

                    move_item_to_grid( &thing_created, i, j );

                    if (thing_created != NON_ITEM)
                        items_placed++;
                }
            }
        }
    }

    int exits_wanted  = 0;
    int altars_wanted = 0;

    for (int i = gx1; i <= gx2; i++)
    {
        for (int j = gy1; j <= gy2; j++)
        {
            if (grd[i][j] == DNGN_UNSEEN)
                grd[i][j] = replaced[random2(5)];

            if (one_chance_in(7500)) // place an exit
                exits_wanted++;

            // Don't place exit under items
            if (exits_wanted > 0 && igrd[i][j] == NON_ITEM)
            {
                grd[i][j] = DNGN_EXIT_ABYSS;
                exits_wanted--;
#if DEBUG_ABYSS
                mpr("Placing Abyss exit.", MSGCH_DIAGNOSTICS);
#endif
            }

            if (one_chance_in(10000)) // place an altar
                altars_wanted++;

            // Don't place altars under items.
            if (altars_wanted > 0 && igrd[i][j] == NON_ITEM)
            {
                do
                {
                    grd[i][j] =
                        static_cast<dungeon_feature_type>(
                            DNGN_ALTAR_ZIN + random2(NUM_GODS-1) );
                }
                while (grd[i][j] == DNGN_ALTAR_ZIN
                       || grd[i][j] == DNGN_ALTAR_SHINING_ONE
                       || grd[i][j] == DNGN_ALTAR_ELYVILON);

                // Lugonu has a flat 50% chance of corrupting the altar
                if ( coinflip() )
                    grd[i][j] = DNGN_ALTAR_LUGONU;

                altars_wanted--;
#if DEBUG_ABYSS
                mpr("Placing altar.", MSGCH_DIAGNOSTICS);
#endif
            }
        }
    }
}

static int abyss_exit_nearness()
{
    int nearness = INFINITE_DISTANCE;

    for (int x = you.x_pos - LOS_RADIUS; x < you.x_pos + LOS_RADIUS; x++)
        for (int y = you.y_pos - LOS_RADIUS; y < you.y_pos + LOS_RADIUS; y++)
        {
            if (!in_bounds(x, y))
                continue;

            // HACK: Why doesn't is_terrain_known() work here?
            if (grd[x][y] == DNGN_EXIT_ABYSS
                && get_screen_glyph(x, y) != ' ')
            {
                nearness = MIN(nearness,
                               grid_distance(you.x_pos, you.y_pos,
                                             x, y));
            }
        }

    return (nearness);
}

static int abyss_rune_nearness()
{
    int nearness = INFINITE_DISTANCE;

    for (int x = you.x_pos - LOS_RADIUS; x < you.x_pos + LOS_RADIUS; x++)
        for (int y = you.y_pos - LOS_RADIUS; y < you.y_pos + LOS_RADIUS; y++)
        {
            if (!in_bounds(x, y))
                continue;

            // HACK: Why doesn't is_terrain_known() work here?
            if (get_screen_glyph(x, y) != ' ')
            {
                int i = igrd[x][y];

                while (i != NON_ITEM)
                {
                    item_def& item(mitm[i]);
                    if (is_rune(item) && item.plus == RUNE_ABYSSAL)
                        nearness = MIN(nearness,
                                       grid_distance(you.x_pos, you.y_pos,
                                                     x, y));
                    i = item.link;
                }
            }
        }

    return (nearness);
}

static int exit_was_near;
static int rune_was_near;

static void xom_check_nearness_setup()
{
    exit_was_near = abyss_exit_nearness();
    rune_was_near = abyss_rune_nearness();
}

// If the player was almost to the exit when it disppeared, Xom is
// extremely amused.  He's also extremely amused if the player winds
// up right next to an exit when there wasn't one there before.  The
// same applies to Abyssal runes.
static void xom_check_nearness()
{
    // Update known terrain
    viewwindow(true, false);

    int exit_is_near = abyss_exit_nearness();
    int rune_is_near = abyss_rune_nearness();

    if ((exit_was_near   <  INFINITE_DISTANCE
         && exit_is_near == INFINITE_DISTANCE)
        || (rune_was_near < INFINITE_DISTANCE
            && rune_is_near == INFINITE_DISTANCE
            && you.attribute[ATTR_ABYSSAL_RUNES] == 0))
    {
        xom_is_stimulated(255, "Xom snickers loudly.", true);
    }

    if ((rune_was_near   == INFINITE_DISTANCE
         && rune_is_near <  INFINITE_DISTANCE
         && you.attribute[ATTR_ABYSSAL_RUNES] == 0)
        || (exit_was_near == INFINITE_DISTANCE &&
            exit_is_near  <  INFINITE_DISTANCE))
    {
        xom_is_stimulated(255);
    }
}

static void abyss_lose_monster(monsters &mons)
{
    if (mons.needs_transit())
        mons.set_transit( level_id(LEVEL_ABYSS) );

    mons.reset();
}

void area_shift(void)
/*******************/
{
#if DEBUG_ABYSS
    mpr("area_shift().", MSGCH_DIAGNOSTICS);
#endif

    xom_check_nearness_setup();

    for (unsigned int i = 0; i < MAX_MONSTERS; i++)
    {
        monsters &m = menv[i];

        if (!m.alive())
            continue;

        // remove non-nearby monsters
        if (grid_distance(m.x, m.y, you.x_pos, you.y_pos) > 10)
            abyss_lose_monster(m);
    }

    for (int i = 5; i < (GXM - 5); i++)
    {
        for (int j = 5; j < (GYM - 5); j++)
        {
            // don't modify terrain by player
            if (grid_distance(i, j, you.x_pos, you.y_pos) <= 10)
                continue;

            // nuke terrain otherwise
            grd[i][j] = DNGN_UNSEEN;

            // nuke items
            lose_item_stack( i, j );

            if (mgrd[i][j] != NON_MONSTER)
                abyss_lose_monster( menv[ mgrd[i][j] ] );
        }
    }

    // shift all monsters & items to new area
    for (int i = you.x_pos - 10; i < you.x_pos + 11; i++)
    {
        if (i < 0 || i >= GXM)
            continue;

        for (int j = you.y_pos - 10; j < you.y_pos + 11; j++)
        {
            if (j < 0 || j >= GYM)
                continue;

            const int ipos = 45 + i - you.x_pos;
            const int jpos = 35 + j - you.y_pos;

            // move terrain
            grd[ipos][jpos] = grd[i][j];

            // move item
            move_item_stack_to_grid( i, j, ipos, jpos );

            // move monster
            mgrd[ipos][jpos] = mgrd[i][j];
            if (mgrd[i][j] != NON_MONSTER)
            {
                menv[mgrd[ipos][jpos]].x = ipos;
                menv[mgrd[ipos][jpos]].y = jpos;
                mgrd[i][j] = NON_MONSTER;
            }

            // move cloud
            if (env.cgrid[i][j] != EMPTY_CLOUD)
                move_cloud( env.cgrid[i][j], ipos, jpos );
        }
    }


    for (unsigned int i = 0; i < MAX_CLOUDS; i++)
    {
        if (env.cloud[i].type == CLOUD_NONE)
            continue;

        if (env.cloud[i].x < 35 || env.cloud[i].x > 55
            || env.cloud[i].y < 25 || env.cloud[i].y > 45)
        {
            delete_cloud( i );
        }
    }

    you.moveto(45, 35);

    generate_area(5, 5, (GXM - 5), (GYM - 5));

    xom_check_nearness();

    mgen_data mons;
    mons.level_type = LEVEL_ABYSS;
    mons.proximity  = PROX_AWAY_FROM_PLAYER;
    
    for (unsigned int mcount = 0; mcount < 15; mcount++)
        mons_place(mons);

    // And allow monsters in transit another chance to return.
    place_transiting_monsters();
    place_transiting_items();
}

void save_abyss_uniques()
{
    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters &m = menv[i];
        if (m.alive() && m.needs_transit())
            m.set_transit( level_id(LEVEL_ABYSS) );
    }
}

void abyss_teleport( bool new_area )
/**********************************/
{
    xom_check_nearness_setup();

    int x, y, i, j, k;

    if (!new_area)
    {
        // try to find a good spot within the shift zone:
        for (i = 0; i < 100; i++)
        {
            x = 16 + random2( GXM - 32 );
            y = 16 + random2( GYM - 32 );

            if ((grd[x][y] == DNGN_FLOOR
                    || grd[x][y] == DNGN_SHALLOW_WATER)
                && mgrd[x][y] == NON_MONSTER
                && env.cgrid[x][y] == EMPTY_CLOUD)
            {
                break;
            }
        }

        if (i < 100)
        {
#if DEBUG_ABYSS
            mpr("Non-new area Abyss teleport.", MSGCH_DIAGNOSTICS);
#endif
            you.moveto(x, y);
            xom_check_nearness();
            return;
        }
    }

#if DEBUG_ABYSS
    mpr("New area Abyss teleport.", MSGCH_DIAGNOSTICS);
#endif

    // teleport to a new area of the abyss:

    init_pandemonium();          // get new monsters
    dgn_set_colours_from_monsters(); // and new colours

    for (i = 0; i < MAX_MONSTERS; i++)
    {
        if (menv[i].alive())
            abyss_lose_monster(menv[i]);
    }

    // Orbs and fixed artefacts are marked as "lost in the abyss"
    for (k = 0; k < MAX_ITEMS; k++)
    {
        if (is_valid_item( mitm[k] ))
        {
            item_was_lost( mitm[k] );
            destroy_item( k );
        }
    }

    for (i = 0; i < MAX_CLOUDS; i++)
        delete_cloud( i );

    for (i = 10; i < (GXM - 9); i++)
    {
        for (j = 10; j < (GYM - 9); j++)
        {
            grd[i][j] = DNGN_UNSEEN;    // so generate_area will pick it up
            igrd[i][j] = NON_ITEM;
            mgrd[i][j] = NON_MONSTER;
            env.cgrid[i][j] = EMPTY_CLOUD;
        }
    }

    ASSERT( env.cloud_no == 0 );

    you.moveto(45, 35);

    generate_area( 10, 10, (GXM - 10), (GYM - 10) );

    xom_check_nearness();

    grd[you.x_pos][you.y_pos] = DNGN_FLOOR;
    if ( one_chance_in(5) )
        place_feature_near( you.pos(), LOS_RADIUS,
                            DNGN_FLOOR, DNGN_ALTAR_LUGONU, 50 );

    place_transiting_monsters();
    place_transiting_items();
}

//////////////////////////////////////////////////////////////////////////////
// Abyss effects in other levels, courtesy Lugonu.

static void place_corruption_seed(const coord_def &pos, int duration)
{
    env.markers.add(new map_corruption_marker(pos, duration));
}

static void initialise_level_corrupt_seeds(int power)
{
    const int low = power / 2, high = power * 3 / 2;
    int nseeds = random_range(1, std::min(2 + power / 110, 4));

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Placing %d corruption seeds", nseeds);
#endif

    // The corruption centered on the player is free.
    place_corruption_seed(you.pos(), high + 300);

    for (int i = 0; i < nseeds; ++i)
    {
        coord_def where;
        do
            where = coord_def(random2(GXM), random2(GYM));
        while (!in_bounds(where) || grd(where) != DNGN_FLOOR
               || env.markers.find(where, MAT_ANY));

        place_corruption_seed(where, random_range(low, high, 2) + 300);
    }
}

static bool spawn_corrupted_servant_near(const coord_def &pos)
{
    // Thirty tries for a place
    for (int i = 0; i < 30; ++i)
    {
        const coord_def p( pos.x + random2avg(4, 3) + random2(3),
                           pos.y + random2avg(4, 3) + random2(3) );
        if (!in_bounds(p) || p == you.pos() || mgrd(p) != NON_MONSTER
            || !grid_compatible(DNGN_FLOOR, grd(p), true))
            continue;

        // Got a place, summon the beast.
        int level = 51;
        monster_type mons =
            pick_random_monster(level_id(LEVEL_ABYSS), level, level);
        if (mons == MONS_PROGRAM_BUG)
            return (false);

        const beh_type beh =
            one_chance_in(5 + you.skills[SK_INVOCATIONS] / 4)?
            BEH_HOSTILE : BEH_NEUTRAL;
        const int mid =
            create_monster( mgen_data( mons, beh, 5, p ) );
        return (mid != -1);
    }
    return (false);
}

static void apply_corruption_effect(
    map_marker *marker, int duration)
{
    if (!duration)
        return;

    map_corruption_marker *cmark = dynamic_cast<map_corruption_marker*>(marker);
    const coord_def center = cmark->pos;
    const int neffects = std::max(div_rand_round(duration, 5), 1);

    for (int i = 0; i < neffects; ++i)
    {
        if (random2(4000) < cmark->duration)
        {
            if (!spawn_corrupted_servant_near(cmark->pos))
                break;
        }
    }
    cmark->duration -= duration;
    if (cmark->duration < 1)
        env.markers.remove(cmark);
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

        apply_corruption_effect(mark, duration);
    }
}

static bool is_grid_corruptible(const coord_def &c)
{
    if (c == you.pos())
        return (false);

    const dungeon_feature_type feat = grd(c);

    // Stairs and portals cannot be corrupted.
    if (grid_stair_direction(feat) != CMD_NO_CMD)
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
static bool is_crowded_square(const coord_def &c)
{
    int neighbours = 0;
    for (int xi = -1; xi <= 1; ++xi)
    {
        for (int yi = -1; yi <= 1; ++yi)
        {
            if (!xi && !yi)
                continue;

            const coord_def n(c.x + xi, c.y + yi);
            if (!in_bounds(n) || !is_traversable(grd(n)))
                continue;

            if (++neighbours > 4)
                return (false);
        }
    }
    return (true);
}

// Returns true if the square has all opaque neighbours.
static bool is_sealed_square(const coord_def &c)
{
    for (int xi = -1; xi <= 1; ++xi)
    {
        for (int yi = -1; yi <= 1; ++yi)
        {
            if (!xi && !yi)
                continue;

            const coord_def n(c.x + xi, c.y + yi);
            if (!in_bounds(n))
                continue;

            if (!grid_is_opaque(grd(n)))
                return (false);
        }
    }
    return (true);
}

static void corrupt_square(const crawl_environment &oenv, const coord_def &c)
{
    // To prevent the destruction of, say, branch entries.
    bool preserve_feat = true;
    dungeon_feature_type feat = DNGN_UNSEEN;
    if (grid_altar_god(grd(c)) != GOD_NO_GOD)
    {
        // altars may be safely overwritten, ha!
        preserve_feat = false;
        if (!one_chance_in(3))
            feat = DNGN_ALTAR_LUGONU;
    }
    else
        feat = oenv.grid(c);

    if (is_trap_square(feat) || feat == DNGN_SECRET_DOOR || feat == DNGN_UNSEEN)
        return;

    if (is_traversable(grd(c)) && !is_traversable(feat) && is_crowded_square(c))
        return;

    if (!is_traversable(grd(c)) && is_traversable(feat) && is_sealed_square(c))
        return;

    // What's this supposed to achieve? (jpeg)
    // I mean, won't exits from the Abyss only turn up in the Abyss itself?
    if (feat == DNGN_EXIT_ABYSS)
        feat = DNGN_ENTER_ABYSS;

    dungeon_terrain_changed(c, feat, preserve_feat, true, true);
    if (feat == DNGN_ROCK_WALL)
        env.grid_colours(c) = oenv.rock_colour;
    else if (feat == DNGN_FLOOR)
        env.grid_colours(c) = oenv.floor_colour;

#ifdef USE_TILE
    // Modify tile flavor to use corrupted tiles.
    if (feat == DNGN_ROCK_WALL)
    {
        env.tile_flavor[c.x][c.y].wall =
            TILE_DNGN_WALL_CORRUPT - get_wall_tile_idx() + random2(4);
    }
    else if (feat == DNGN_FLOOR)
    {
        env.tile_flavor[c.x][c.y].floor =
            TILE_DNGN_FLOOR_CORRUPT - get_floor_tile_idx() + random2(4);
    }
#endif
}

static void corrupt_level_features(const crawl_environment &oenv)
{
    std::vector<coord_def> corrupt_seeds;
    std::vector<map_marker*> corrupt_markers =
        env.markers.get_all(MAT_CORRUPTION_NEXUS);

    for (int i = 0, size = corrupt_markers.size(); i < size; ++i)
        corrupt_seeds.push_back(corrupt_markers[i]->pos);

    for (int y = MAPGEN_BORDER; y < GYM - MAPGEN_BORDER; ++y)
        for (int x = MAPGEN_BORDER; x < GXM - MAPGEN_BORDER; ++x)
        {
            const coord_def c(x, y);
            int distance = GXM * GXM + GYM * GYM;
            for (int i = 0, size = corrupt_seeds.size(); i < size; ++i)
            {
                const int dist = (c - corrupt_seeds[i]).rdist();
                if (dist < distance)
                    distance = dist;
            }

            if ((distance < 6 || one_chance_in(1 + distance - 6))
                && is_grid_corruptible(c))
            {
                corrupt_square(oenv, c);
            }
        }
}

static bool is_level_corrupted()
{
    if (you.level_type == LEVEL_ABYSS
        || you.level_type == LEVEL_PANDEMONIUM
        || player_in_hell()
        || player_in_branch(BRANCH_VESTIBULE_OF_HELL))
    {
        return (true);
    }

    return (!!env.markers.find(MAT_CORRUPTION_NEXUS));
}

static bool is_level_incorruptible()
{
    if (is_level_corrupted())
    {
        mpr("This place is already infused with evil and corruption.");
        return (true);
    }

    return (false);
}

static void corrupt_choose_colours()
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

    mprf(MSGCH_GOD, "Lugonu's Hand of Corruption reaches out!");

    you.flash_colour = EC_MUTAGENIC;
    viewwindow(true, false);

    initialise_level_corrupt_seeds(power);

    std::auto_ptr<crawl_environment> backup(new crawl_environment(env));
    generate_abyss();
    generate_area(MAPGEN_BORDER, MAPGEN_BORDER,
                  GXM - MAPGEN_BORDER, GYM - MAPGEN_BORDER);

    corrupt_choose_colours();

    std::auto_ptr<crawl_environment> abyssal(new crawl_environment(env));
    env = *backup;
    backup.reset(NULL);
    dungeon_events.clear();
    env.markers.activate_all(false);

    corrupt_level_features(*abyssal);
    run_corruption_effects(300);

    you.flash_colour = EC_MUTAGENIC;
    viewwindow(true, false);
    // Allow extra time for the flash to linger.
    delay(1000);
    viewwindow(true, false);

    return (true);
}
