/*
 *  File:       travel.cc
 *  Summary:    Travel stuff
 *  Written by: Darshan Shaligram
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Known issues:
 *   Hardcoded dungeon features all over the place - this thing is a devil to
 *   refactor.
 */
#include "AppHdr.h"
#include "files.h"
#include "FixAry.h"
#include "clua.h"
#include "describe.h"
#include "mon-util.h"
#include "player.h"
#include "stash.h"
#include "stuff.h"
#include "travel.h"
#include "view.h"
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>

#ifdef DOS
#include <dos.h>
#endif

#define TC_MAJOR_VERSION ((unsigned char) 4)
#define TC_MINOR_VERSION ((unsigned char) 4)

enum IntertravelDestination
{
    // Go down a level
    ID_DOWN     = -100,

    // Go up a level
    ID_UP       = -99,

    // Repeat last travel
    ID_REPEAT   = -101,

    // Cancel interlevel travel
    ID_CANCEL   = -1000
};

TravelCache travel_cache;

// Tracks the distance between the target location on the target level and the 
// stairs on the level.
static std::vector<stair_info> curr_stairs;

// Squares that are not safe to travel to on the current level.
static std::vector<coord_def> curr_excludes;

// This is where we last tried to take a stair during interlevel travel.
// Note that last_stair.depth should be set to -1 before initiating interlevel
// travel.
static level_id last_stair;

// Where travel wants to get to.
static level_pos travel_target;

// The place in the Vestibule of Hell where all portals to Hell land.
static level_pos travel_hell_entry;

static bool traps_inited = false;

// TODO: Do we need this or would a different header be better?
inline int sgn(int x)
{
    return x < 0? -1 : (x > 0);
}

// Array of points on the map, each value being the distance the character
// would have to travel to get there. Negative distances imply that the point
// is a) a trap or hostile terrain or b) only reachable by crossing a trap or
// hostile terrain.
short point_distance[GXM][GYM];

unsigned char curr_waypoints[GXM][GYM];

signed char curr_traps[GXM][GYM];

static FixedArray< unsigned short, GXM, GYM >  mapshadow;

// Clockwise, around the compass from north (same order as enum RUN_DIR)
// Copied from acr.cc
static const struct coord_def Compass[8] = 
{ 
    {  0, -1 }, {  1, -1 }, {  1,  0 }, {  1,  1 },
    {  0,  1 }, { -1,  1 }, { -1,  0 }, { -1, -1 },
};

#define TRAVERSABLE  1
#define IMPASSABLE   0
#define FORBIDDEN   -1

// Map of terrain types that are traversable.
static signed char traversable_terrain[256];

static command_type trans_negotiate_stairs();
static int find_transtravel_square(const level_pos &pos, bool verbose = true);

static bool loadlev_populate_stair_distances(const level_pos &target);
static void populate_stair_distances(const level_pos &target);

// Determines whether the player has seen this square, given the user-visible
// character. 
//
// The player is assumed to have seen the square if:
// a. The square is mapped (the env map char is not zero)
// b. The square was *not* magic-mapped.
//
// FIXME: There's better ways of doing this with the new view.cc.
bool is_player_mapped(unsigned char envch)
{
    // Note that we're relying here on mapch(DNGN_FLOOR) != mapch2(DNGN_FLOOR)
    // and that no *other* dungeon feature renders as mapch(DNGN_FLOOR).
    // The check for a ~ is to ensure explore stops for items turned up by
    // detect items.
    return envch && envch != get_magicmap_char(DNGN_FLOOR) && envch != '~';
}

inline bool is_trap(unsigned char grid)
{
    return (grid == DNGN_TRAP_MECHANICAL || grid == DNGN_TRAP_MAGICAL
              || grid == DNGN_TRAP_III);
}

// Returns true if there is a known trap at (x,y). Returns false for non-trap
// squares as also for undiscovered traps.
//
inline bool is_trap(int x, int y)
{
    return is_trap( grd[x][y] );
}

// Returns true if this feature takes extra time to cross.
inline int feature_traverse_cost(unsigned char feature)
{
    return (feature == DNGN_SHALLOW_WATER || feature == DNGN_CLOSED_DOOR? 2 :
            is_trap(feature) ? 3 : 1);
}

// Returns true if the dungeon feature supplied is an altar.
inline bool is_altar(unsigned char grid)
{
    return grid >= DNGN_ALTAR_ZIN && grid <= DNGN_ALTAR_ELYVILON;
}

inline bool is_altar(const coord_def &c)
{
    return is_altar(grd[c.x][c.y]);
}

inline bool is_player_altar(unsigned char grid)
{
    // An ugly hack, but that's what religion.cc does.
    return you.religion 
        && grid == DNGN_ALTAR_ZIN - 1 + you.religion;
}

inline bool is_player_altar(const coord_def &c)
{
    return is_player_altar(grd[c.x][c.y]);
}

// Copies FixedArray src to FixedArray dest. 
//
inline void copy(const FixedArray<unsigned short, GXM, GYM> &src, 
                 FixedArray<unsigned short, GXM, GYM> &dest)
{
    dest = src;
}

#ifdef CLUA_BINDINGS
static void init_traps()
{
    memset(curr_traps, -1, sizeof curr_traps);
    for (int i = 0; i < MAX_TRAPS; ++i)
    {
        int x = env.trap[i].x,
            y = env.trap[i].y;
        if (x > 0 && x < GXM && y > 0 && y < GYM)
            curr_traps[x][y] = i;
    }
    traps_inited = true;
}

static const char *trap_name(int x, int y)
{
    if (!traps_inited)
        init_traps();
    
    const int ti = curr_traps[x][y];
    if (ti != -1)
    {
        int type = env.trap[ti].type;
        if (type >= 0 && type < NUM_TRAPS)
            return (trap_name(trap_type(type)));
    }
    return ("");
}
#endif

/*
 * Returns true if the character can cross this dungeon feature.
 */
inline bool is_traversable(unsigned char grid)
{
    return traversable_terrain[(int) grid] == TRAVERSABLE;
}

static bool is_excluded(int x, int y)
{
    for (int i = 0, count = curr_excludes.size(); i < count; ++i)
    {
        const coord_def &c = curr_excludes[i];
        int dx = c.x - x,
            dy = c.y - y;
        if (dx * dx + dy * dy <= Options.travel_exclude_radius2)
            return true;
    }
    return false;
}

static bool is_exclude_root(int x, int y)
{
    for (int i = 0, count = curr_excludes.size(); i < count; ++i)
    {
        const coord_def &c = curr_excludes[i];
        if (c.x == x && c.y == y)
            return true;
    }
    return false;
}

const char *run_mode_name(int runmode)
{
    return runmode == RUN_TRAVEL?       "travel" :
           runmode == RUN_INTERLEVEL?   "intertravel" :
           runmode == RUN_EXPLORE?      "explore" :
           runmode > 0?                 "run" :
                                        "";
}

unsigned char is_waypoint(int x, int y)
{
    if (you.level_type == LEVEL_LABYRINTH || you.level_type == LEVEL_ABYSS
            || you.level_type == LEVEL_PANDEMONIUM)
        return 0;
    return curr_waypoints[x][y];
}

#ifdef STASH_TRACKING
inline bool is_stash(LevelStashes *ls, int x, int y)
{
    if (!ls)
        return (false);
    Stash *s = ls->find_stash(x, y);
    return s && s->enabled;
}
#endif

void clear_excludes()
{
    // Sanity checks
    if (you.level_type == LEVEL_LABYRINTH || you.level_type == LEVEL_ABYSS)
        return;

    curr_excludes.clear();

    if (can_travel_interlevel())
    {
        LevelInfo &li = travel_cache.get_level_info(
                            level_id::get_current_level_id());
        li.update();
    }
}

void toggle_exclude(int x, int y)
{
    // Sanity checks
    if (you.level_type == LEVEL_LABYRINTH || you.level_type == LEVEL_ABYSS)
        return;

    if (x <= 0 || x >= GXM || y <= 0 || y >= GYM) return;
    if (!env.map[x - 1][y - 1]) return;
    
    if (is_exclude_root(x, y))
    {
        for (int i = 0, count = curr_excludes.size(); i < count; ++i)
        {
            const coord_def &c = curr_excludes[i];
            if (c.x == x && c.y == y)
            {
                curr_excludes.erase( curr_excludes.begin() + i );
                break ;
            }
        }
    }
    else
    {
        coord_def c = { x, y };
        curr_excludes.push_back(c);
    }

    if (can_travel_interlevel())
    {
        LevelInfo &li = travel_cache.get_level_info(
                            level_id::get_current_level_id());
        li.update();
    }
}

void update_excludes()
{
    // Sanity checks
    if (you.level_type == LEVEL_LABYRINTH || you.level_type == LEVEL_ABYSS)
        return;
    for (int i = curr_excludes.size() - 1; i >= 0; --i)
    {
        int x = curr_excludes[i].x,
            y = curr_excludes[i].y;
        if (!env.map[x - 1][y - 1])
            curr_excludes.erase( curr_excludes.begin() + i );
    }
}

void forget_square(int x, int y)
{
    if (you.level_type == LEVEL_LABYRINTH || you.level_type == LEVEL_ABYSS)
        return;
    
    if (is_exclude_root(x, y))
        toggle_exclude(x, y);
}

/*
 * Returns true if the square at (x,y) is a dungeon feature the character
 * can't (under normal circumstances) safely cross.
 *
 * Note: is_reseedable can return true for dungeon features that is_traversable
 *       also returns true for. This is okay, because is_traversable always
 *       takes precedence over is_reseedable. is_reseedable is used only to
 *       decide which squares to reseed from when flood-filling outwards to
 *       colour the level map. It does not affect pathing of actual
 *       travel/explore.
 */
static bool is_reseedable(int x, int y)
{
    if (is_excluded(x, y))
        return true;
    unsigned char grid = grd[x][y];
    return (grid == DNGN_DEEP_WATER || grid == DNGN_SHALLOW_WATER ||
             grid == DNGN_LAVA || is_trap(x, y));
}

/*
 * Returns true if the square at (x,y) is okay to travel over. If ignore_hostile
 * is true, returns true even for dungeon features the character can normally
 * not cross safely (deep water, lava, traps).
 */
static bool is_travel_ok(int x, int y, bool ignore_hostile)
{
    unsigned char grid = grd[x][y];

    unsigned char envc = (unsigned char) env.map[x - 1][y - 1];
    if (!envc) return false;

    // Special-case secret doors so that we don't run into awkwardness when
    // a monster opens a secret door without the hero seeing it, but the travel
    // code paths through the secret door because it looks at the actual grid,
    // rather than the env overmap. Hopefully there won't be any more such
    // cases.
    // FIXME: is_terrain_changed ought to do this with the view.cc changes.
    if (envc == get_sightmap_char(DNGN_SECRET_DOOR)) return false;

    unsigned char mon = mgrd[x][y];
    if (mon != NON_MONSTER)
    {
        // Kludge warning: navigating around zero-exp beasties uses knowledge
        //                 that the player may not have (the player may not
        //                 know that there's a plant at any given (x,y), but we
        //                 know, because we're looking directly at the grid).
        //                 Arguably the utility of this feature is greater than
        //                 the information we're giving the player for free.
        // Navigate around plants and fungi. Yet another tasty hack.
        if (player_monster_visible(&menv[mon]) && 
                mons_class_flag( menv[mon].type, M_NO_EXP_GAIN ))
        {
            extern short point_distance[GXM][GYM];

            // We have to set the point_distance array if the level map is
            // to be properly coloured. The caller isn't going to do it because
            // we say this square is inaccessible, so in a horrible hack, we 
            // do it ourselves. Ecch.
            point_distance[x][y] = ignore_hostile? -42 : 42;
            return false;
        }
    }

    // If 'ignore_hostile' is true, we're ignoring hazards that can be
    // navigated over if the player is willing to take damage, or levitate.
    if (ignore_hostile && is_reseedable(x, y))
        return true;

    return (is_traversable(grid) 
#ifdef CLUA_BINDINGS
             || 
                (is_trap(x, y) && 
                 clua.callbooleanfn(false, "ch_cross_trap", 
                                    "s", trap_name(x, y)))
#endif
            )
            && !is_excluded(x, y);
}

// Returns true if the location at (x,y) is monster-free and contains no clouds.
static bool is_safe(int x, int y)
{
    unsigned char mon = mgrd[x][y];
    if (mon != NON_MONSTER)
    {
        // If this is an invisible critter, say we're safe to get here, but 
        // turn off travel - the result should be that the player bashes into
        // the monster and stops travelling right there. Same treatment applies
        // to mimics.
        if (!player_monster_visible(&menv[mon]) || 
                mons_is_mimic( menv[mon].type ))
        {
            you.running = 0;
            return true;
        }

        // Stop before wasting energy on plants and fungi.
        if (mons_class_flag( menv[mon].type, M_NO_EXP_GAIN ))
            return false;

        // If this is any *other* monster, it'll be visible and
        // a) Friendly, in which case we'll displace it, no problem.
        // b) Unfriendly, in which case we're in deep trouble, since travel
        //    should have been aborted already by the checks in view.cc.
    }
    const char cloud = env.cgrid[x][y];
    if (cloud == EMPTY_CLOUD)
        return true;

    // We can also safely run through smoke.
    const char cloud_type = env.cloud[ cloud ].type;
    return cloud_type == CLOUD_GREY_SMOKE ||
        cloud_type == CLOUD_GREY_SMOKE_MON ||
        cloud_type == CLOUD_BLUE_SMOKE     ||
        cloud_type == CLOUD_BLUE_SMOKE_MON ||
        cloud_type == CLOUD_PURP_SMOKE     ||
        cloud_type == CLOUD_PURP_SMOKE_MON ||
        cloud_type == CLOUD_BLACK_SMOKE    ||
        cloud_type == CLOUD_BLACK_SMOKE_MON;
}

static bool player_is_permalevitating()
{
    return you.levitation > 1 &&
            ((you.species == SP_KENKU && you.experience_level >= 15)
             || player_equip_ego_type( EQ_BOOTS, SPARM_LEVITATION ));
}

static void set_pass_feature(unsigned char grid, signed char pass)
{
    if (traversable_terrain[(unsigned) grid] != FORBIDDEN)
        traversable_terrain[(unsigned) grid] = pass;
}

/*
 * Sets traversable terrain based on the character's role and whether or not he
 * has permanent levitation
 */
static void init_terrain_check()
{
    // Merfolk get deep water.
    signed char water = you.species == SP_MERFOLK? TRAVERSABLE : IMPASSABLE;
    // If the player has overridden deep water already, we'll respect that.
    set_pass_feature(DNGN_DEEP_WATER, water);

    // Permanently levitating players can cross most hostile terrain.
    signed char trav = player_is_permalevitating()? 
                    TRAVERSABLE : IMPASSABLE;
    if (you.species != SP_MERFOLK)
        set_pass_feature(DNGN_DEEP_WATER, trav);
    set_pass_feature(DNGN_LAVA, trav);
    set_pass_feature(DNGN_TRAP_MECHANICAL, trav);
}

void travel_init_new_level()
{
    // Zero out last travel coords
    you.run_x       = you.run_y     = 0;
    you.travel_x    = you.travel_y  = 0;

    traps_inited    = false;
    curr_excludes.clear();
    travel_cache.set_level_excludes();
    travel_cache.update_waypoints();
}

/*
 * Sets up travel-related stuff.
 */
void initialise_travel()
{
    // Need a better way to do this. :-(
    traversable_terrain[DNGN_FLOOR] =
    traversable_terrain[DNGN_ENTER_HELL] =
    traversable_terrain[DNGN_OPEN_DOOR] =
    traversable_terrain[DNGN_UNDISCOVERED_TRAP] =
    traversable_terrain[DNGN_ENTER_SHOP] =
    traversable_terrain[DNGN_ENTER_LABYRINTH] =
    traversable_terrain[DNGN_STONE_STAIRS_DOWN_I] =
    traversable_terrain[DNGN_STONE_STAIRS_DOWN_II] =
    traversable_terrain[DNGN_STONE_STAIRS_DOWN_III] =
    traversable_terrain[DNGN_ROCK_STAIRS_DOWN] =
    traversable_terrain[DNGN_STONE_STAIRS_UP_I] =
    traversable_terrain[DNGN_STONE_STAIRS_UP_II] =
    traversable_terrain[DNGN_STONE_STAIRS_UP_III] =
    traversable_terrain[DNGN_ROCK_STAIRS_UP] =
    traversable_terrain[DNGN_ENTER_DIS] =
    traversable_terrain[DNGN_ENTER_GEHENNA] =
    traversable_terrain[DNGN_ENTER_COCYTUS] =
    traversable_terrain[DNGN_ENTER_TARTARUS] =
    traversable_terrain[DNGN_ENTER_ABYSS] =
    traversable_terrain[DNGN_EXIT_ABYSS] =
    traversable_terrain[DNGN_STONE_ARCH] =
    traversable_terrain[DNGN_ENTER_PANDEMONIUM] =
    traversable_terrain[DNGN_EXIT_PANDEMONIUM] =
    traversable_terrain[DNGN_TRANSIT_PANDEMONIUM] =
    traversable_terrain[DNGN_ENTER_ORCISH_MINES] =
    traversable_terrain[DNGN_ENTER_HIVE] =
    traversable_terrain[DNGN_ENTER_LAIR] =
    traversable_terrain[DNGN_ENTER_SLIME_PITS] =
    traversable_terrain[DNGN_ENTER_VAULTS] =
    traversable_terrain[DNGN_ENTER_CRYPT] =
    traversable_terrain[DNGN_ENTER_HALL_OF_BLADES] =
    traversable_terrain[DNGN_ENTER_ZOT] =
    traversable_terrain[DNGN_ENTER_TEMPLE] =
    traversable_terrain[DNGN_ENTER_SNAKE_PIT] =
    traversable_terrain[DNGN_ENTER_ELVEN_HALLS] =
    traversable_terrain[DNGN_ENTER_TOMB] =
    traversable_terrain[DNGN_ENTER_SWAMP] =
    traversable_terrain[DNGN_RETURN_FROM_ORCISH_MINES] =
    traversable_terrain[DNGN_RETURN_FROM_HIVE] =
    traversable_terrain[DNGN_RETURN_FROM_LAIR] =
    traversable_terrain[DNGN_RETURN_FROM_SLIME_PITS] =
    traversable_terrain[DNGN_RETURN_FROM_VAULTS] =
    traversable_terrain[DNGN_RETURN_FROM_CRYPT] =
    traversable_terrain[DNGN_RETURN_FROM_HALL_OF_BLADES] =
    traversable_terrain[DNGN_RETURN_FROM_ZOT] =
    traversable_terrain[DNGN_RETURN_FROM_TEMPLE] =
    traversable_terrain[DNGN_RETURN_FROM_SNAKE_PIT] =
    traversable_terrain[DNGN_RETURN_FROM_ELVEN_HALLS] =
    traversable_terrain[DNGN_RETURN_FROM_TOMB] =
    traversable_terrain[DNGN_RETURN_FROM_SWAMP] =
    traversable_terrain[DNGN_ALTAR_ZIN] =
    traversable_terrain[DNGN_ALTAR_SHINING_ONE] =
    traversable_terrain[DNGN_ALTAR_KIKUBAAQUDGHA] =
    traversable_terrain[DNGN_ALTAR_YREDELEMNUL] =
    traversable_terrain[DNGN_ALTAR_XOM] =
    traversable_terrain[DNGN_ALTAR_VEHUMET] =
    traversable_terrain[DNGN_ALTAR_OKAWARU] =
    traversable_terrain[DNGN_ALTAR_MAKHLEB] =
    traversable_terrain[DNGN_ALTAR_SIF_MUNA] =
    traversable_terrain[DNGN_ALTAR_TROG] =
    traversable_terrain[DNGN_ALTAR_NEMELEX_XOBEH] =
    traversable_terrain[DNGN_ALTAR_ELYVILON] =
    traversable_terrain[DNGN_BLUE_FOUNTAIN] =
    traversable_terrain[DNGN_DRY_FOUNTAIN_I] =
    traversable_terrain[DNGN_SPARKLING_FOUNTAIN] =
    traversable_terrain[DNGN_DRY_FOUNTAIN_II] =
    traversable_terrain[DNGN_DRY_FOUNTAIN_III] =
    traversable_terrain[DNGN_DRY_FOUNTAIN_IV] =
    traversable_terrain[DNGN_DRY_FOUNTAIN_V] =
    traversable_terrain[DNGN_DRY_FOUNTAIN_VI] =
    traversable_terrain[DNGN_DRY_FOUNTAIN_VII] =
    traversable_terrain[DNGN_DRY_FOUNTAIN_VIII] =
    traversable_terrain[DNGN_PERMADRY_FOUNTAIN] =
    traversable_terrain[DNGN_CLOSED_DOOR] =
    traversable_terrain[DNGN_SHALLOW_WATER] =
                            TRAVERSABLE;
}

/*
 * Given a dungeon feature description, returns the feature number. This is a
 * crude hack and currently recognises only (deep/shallow) water.
 *
 * Returns -1 if the feature named is not recognised, else returns the feature
 * number (guaranteed to be 0-255).
 */
int get_feature_type(const std::string &feature)
{
    if (feature.find("deep water") != std::string::npos)
        return DNGN_DEEP_WATER;
    if (feature.find("shallow water") != std::string::npos)
        return DNGN_SHALLOW_WATER;
    return -1;
}

/*
 * Given a feature description, prevents travel to locations of that feature
 * type.
 */
void prevent_travel_to(const std::string &feature)
{
    int feature_type = get_feature_type(feature);
    if (feature_type != -1)
        traversable_terrain[feature_type] = FORBIDDEN;
}

bool is_stair(unsigned gridc)
{
    return (is_travelable_stair(gridc)
                                || gridc == DNGN_ENTER_ABYSS
                                || gridc == DNGN_ENTER_LABYRINTH
                                || gridc == DNGN_ENTER_PANDEMONIUM
                                || gridc == DNGN_EXIT_PANDEMONIUM
                                || gridc == DNGN_TRANSIT_PANDEMONIUM);
}

/*
 * Returns true if the given dungeon feature can be considered a stair.
 */
bool is_travelable_stair(unsigned gridc)
{
    switch (gridc)
    {
    case DNGN_ENTER_HELL:
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
    case DNGN_ROCK_STAIRS_DOWN:
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
    case DNGN_ROCK_STAIRS_UP:
    case DNGN_ENTER_DIS:
    case DNGN_ENTER_GEHENNA:
    case DNGN_ENTER_COCYTUS:
    case DNGN_ENTER_TARTARUS:
    case DNGN_ENTER_ORCISH_MINES:
    case DNGN_ENTER_HIVE:
    case DNGN_ENTER_LAIR:
    case DNGN_ENTER_SLIME_PITS:
    case DNGN_ENTER_VAULTS:
    case DNGN_ENTER_CRYPT:
    case DNGN_ENTER_HALL_OF_BLADES:
    case DNGN_ENTER_ZOT:
    case DNGN_ENTER_TEMPLE:
    case DNGN_ENTER_SNAKE_PIT:
    case DNGN_ENTER_ELVEN_HALLS:
    case DNGN_ENTER_TOMB:
    case DNGN_ENTER_SWAMP:
    case DNGN_RETURN_FROM_ORCISH_MINES:
    case DNGN_RETURN_FROM_HIVE:
    case DNGN_RETURN_FROM_LAIR:
    case DNGN_RETURN_FROM_SLIME_PITS:
    case DNGN_RETURN_FROM_VAULTS:
    case DNGN_RETURN_FROM_CRYPT:
    case DNGN_RETURN_FROM_HALL_OF_BLADES:
    case DNGN_RETURN_FROM_ZOT:
    case DNGN_RETURN_FROM_TEMPLE:
    case DNGN_RETURN_FROM_SNAKE_PIT:
    case DNGN_RETURN_FROM_ELVEN_HALLS:
    case DNGN_RETURN_FROM_TOMB:
    case DNGN_RETURN_FROM_SWAMP:
        return true;
    default:
        return false;
    }
}

#define ES_item  (Options.explore_stop & ES_ITEM)
#define ES_shop  (Options.explore_stop & ES_SHOP)
#define ES_stair (Options.explore_stop & ES_STAIR)
#define ES_altar (Options.explore_stop & ES_ALTAR)

/*
 * Given a square that has just become visible during explore, returns true
 * if the player might consider the square worth stopping explore for.
 */
static bool is_interesting_square(int x, int y)
{
    if (ES_item && igrd[x + 1][y + 1] != NON_ITEM)
        return true;

    unsigned char grid = grd[x + 1][y + 1];
    return (ES_shop && grid == DNGN_ENTER_SHOP)
            || (ES_stair && is_stair(grid))
            || (ES_altar && is_altar(grid) 
                    && you.where_are_you != BRANCH_ECUMENICAL_TEMPLE);
}

static void userdef_run_stoprunning_hook(void)
{
#ifdef CLUA_BINDINGS
    if (you.running)
        clua.callfn("ch_stop_running", "s", run_mode_name(you.running));
#endif
}

static void userdef_run_startrunning_hook(void)
{
#ifdef CLUA_BINDINGS
    if (you.running)
        clua.callfn("ch_start_running", "s", run_mode_name(you.running));
#endif
}

/*
 * Stops shift+running and all forms of travel.
 */
void stop_running(void)
{
    userdef_run_stoprunning_hook();
    you.running = 0;
}

bool is_resting( void )
{
    return (you.running > 0 && !you.run_x && !you.run_y);
}

void start_running(void)
{
    userdef_run_startrunning_hook();
}

/*
 * Top-level travel control (called from input() in acr.cc).
 *
 * travel() is responsible for making the individual moves that constitute
 * (interlevel) travel and explore and deciding when travel and explore 
 * end.
 *
 * Don't call travel() if you.running >= 0.
 */
command_type travel()
{
    char holdx, holdy;
    char *move_x = &holdx;
    char *move_y = &holdy;
    holdx = holdy = 0;
    
    command_type result = CMD_NO_CMD;

    // Abort travel/explore if you're confused or a key was pressed.
    if (kbhit() || you.conf)
    {
        stop_running();
        if (Options.travel_delay == -1)
            redraw_screen();
        return CMD_NO_CMD;
    }

    if (Options.explore_stop && you.running == RUN_EXPLORE)
    {
        // Scan through the shadow map, compare it with the actual map, and if
        // there are any squares of the shadow map that have just been
        // discovered and contain an item, or have an interesting dungeon
        // feature, stop exploring.
        for (int y = 0; y < GYM - 1; ++y)
        {
            for (int x = 0; x < GXM - 1; ++x)
            {
                if (!is_player_mapped(mapshadow[x][y])
                        && is_player_mapped((unsigned char) env.map[x][y])
                        && is_interesting_square(x, y))
                {
                    stop_running();
                    y = GYM;
                    break;
                }
            }
        }
        copy(env.map, mapshadow);
    }

    if (you.running == RUN_EXPLORE)
    {
        // Exploring
        you.run_x = 0;
        find_travel_pos(you.x_pos, you.y_pos, NULL, NULL);
        // No place to go?
        if (!you.run_x)
            stop_running();
    }

    if (you.running == RUN_INTERLEVEL && !you.run_x)
    {
        // Interlevel travel. Since you.run_x is zero, we've either just
        // initiated travel, or we've just climbed or descended a staircase,
        // and we need to figure out where to travel to next.
        if (!find_transtravel_square(travel_target) || !you.run_x)
            stop_running();
    }

    if (you.running < 0)
    {
        // Remember what run-mode we were in so that we can resume explore/
        // interlevel travel correctly.
        int runmode = you.running;

        // Get the next step to make. If the travel command can't find a route,
        // we turn off travel (find_travel_pos does that automatically).
        find_travel_pos(you.x_pos, you.y_pos, move_x, move_y);

        if (!*move_x && !*move_y)
        {
            // If we've reached the square we were traveling towards, travel
            // should stop if this is simple travel. If we're exploring, we
            // should continue doing so (explore has its own end condition
            // upstairs); if we're traveling between levels and we've reached
            // our travel target, we're on a staircase and should take it.
            if (you.x_pos == you.run_x && you.y_pos == you.run_y)
            {
                if (runmode == RUN_EXPLORE)
                    you.running = RUN_EXPLORE;       // Turn explore back on

                // For interlevel travel, we'll want to take the stairs unless
                // the interlevel travel specified a destination square and 
                // we've reached that destination square.
                else if (runmode == RUN_INTERLEVEL
                        && (travel_target.pos.x != you.x_pos
                            || travel_target.pos.y != you.y_pos
                            || travel_target.id != 
                                        level_id::get_current_level_id()))
                {
                    if (last_stair.depth != -1 
                        && last_stair == level_id::get_current_level_id()) 
                    {
                        // We're trying to take the same stairs again. Baaad.

                        // We don't directly call stop_running() because 
                        // you.running is probably 0, and stop_running() won't
                        // notify Lua hooks if you.running == 0.
                        you.running = runmode;
                        stop_running();
                        return CMD_NO_CMD;
                    }
                    you.running = RUN_INTERLEVEL;
                    result = trans_negotiate_stairs();

                    // If, for some reason, we fail to use the stairs, we
                    // need to make sure we don't go into an infinite loop
                    // trying to take it again and again. We'll check
                    // last_stair before attempting to take stairs again.
                    last_stair = level_id::get_current_level_id();

                    // This is important, else we'll probably stop traveling
                    // the moment we clear the stairs. That's because the 
                    // (run_x, run_y) destination will no longer be valid on 
                    // the new level. Setting run_x to zero forces us to
                    // recalculate our travel target next turn (see previous
                    // if block).
                    you.run_x = you.run_y = 0;
                }
                else
                {
                    you.running = runmode;
                    stop_running();
                }
            }
            else
            {
                you.running = runmode;
                stop_running();
            }
        }
        else if (Options.travel_delay > 0)
            delay(Options.travel_delay);
    }

    if (!you.running && Options.travel_delay == -1)
        redraw_screen();

    if (!you.running)
	return CMD_NO_CMD;

    if ( result != CMD_NO_CMD )
	return result;

    return direction_to_command( *move_x, *move_y );
}

command_type direction_to_command( char x, char y ) {
    if ( x == -1 && y == -1 ) return CMD_MOVE_UP_LEFT;
    if ( x == -1 && y ==  0 ) return CMD_MOVE_LEFT;
    if ( x == -1 && y ==  1 ) return CMD_MOVE_DOWN_LEFT;
    if ( x ==  0 && y == -1 ) return CMD_MOVE_UP;
    if ( x ==  0 && y ==  0 ) return CMD_NO_CMD;
    if ( x ==  0 && y ==  1 ) return CMD_MOVE_DOWN;
    if ( x ==  1 && y == -1 ) return CMD_MOVE_UP_RIGHT;
    if ( x ==  1 && y ==  0 ) return CMD_MOVE_RIGHT;
    if ( x ==  1 && y ==  1 ) return CMD_MOVE_DOWN_RIGHT;

    ASSERT(0);
    return CMD_NO_CMD;
}

/*
 * The travel algorithm is based on the NetHack travel code written by Warwick
 * Allison - used with his permission.
 */
void find_travel_pos(int youx, int youy, 
                     char *move_x, char *move_y,
                     std::vector<coord_def>* features)
{
    init_terrain_check();

    int start_x = you.run_x, start_y = you.run_y;
    int dest_x  = youx, dest_y  = youy;
    bool floodout = false;
    unsigned char feature;
#ifdef STASH_TRACKING
    LevelStashes *lev = features? stashes.find_current_level() : NULL;
#endif

    // Normally we start from the destination and floodfill outwards, looking
    // for the character's current position. If we're merely trying to populate
    // the point_distance array (or exploring), we'll want to start from the 
    // character's current position and fill outwards
    if (!move_x || !move_y)
    {
        start_x = youx;
        start_y = youy;

        dest_x = dest_y = -1;

        floodout = true;
    }

    // Abort run if we're trying to go someplace evil
    if (dest_x != -1 && !is_travel_ok(start_x, start_y, false) &&
            !is_trap(start_x, start_y))
    {
        you.running = 0;
        return ;
    }

    // Abort run if we're going nowhere.
    if (start_x == dest_x && start_y == dest_y)
    {
        you.running = 0;
        return ;
    }

    // How many points are we currently considering? We start off with just one
    // point, and spread outwards like a flood-filler.
    int points                  = 1;

    // How many points we'll consider next iteration.
    int next_iter_points        = 0;

    // How far we've traveled from (start_x, start_y), in moves (a diagonal move
    // is no longer than an orthogonal move).
    int traveled_distance       = 1;

    // Which index of the circumference array are we currently looking at?
    int circ_index              = 0;

    // The circumference points of the floodfilled area, for this iteration
    // and the next (this iteration's points being circ_index amd the next one's
    // being !circ_index).
    static FixedVector<coord_def, GXM * GYM> circumference[2];

    // Coordinates of all discovered traps. If we're exploring instead of
    // travelling, we'll reseed from these points after we've explored the map
    std::vector<coord_def> trap_seeds;

    // When set to true, the travel code ignores features, traps and hostile
    // terrain, and simply tries to map contiguous floorspace. Will only be set
    // to true if we're exploring, instead of travelling.
    bool ignore_hostile = false;

    // Set the seed point
    circumference[circ_index][0].x = start_x;
    circumference[circ_index][0].y = start_y;

    // Zap out previous distances array
    memset(point_distance, 0, sizeof point_distance);

    for ( ; points > 0; ++traveled_distance, circ_index = !circ_index,
                        points = next_iter_points, next_iter_points = 0)
    {
        for (int i = 0; i < points; ++i)
        {
            int x = circumference[circ_index][i].x,
                y = circumference[circ_index][i].y;

            // (x,y) is a known (explored) location - we never put unknown 
            // points in the circumference vector, so we don't need to examine 
            // the map array, just the grid array.
            feature = grd[x][y];

            // If this is a feature that'll take time to travel past, we
            // simulate that extra turn by taking this feature next turn,
            // thereby artificially increasing traveled_distance. 
            //
            // Note: I don't know how slow walking through shallow water and
            //       opening closed doors is - right now it's considered to have
            //       the cost of two normal moves.
            int feat_cost = feature_traverse_cost(feature);
            if (feat_cost > 1
                    && point_distance[x][y] > traveled_distance - feat_cost)
            {
                circumference[!circ_index][next_iter_points].x = x;
                circumference[!circ_index][next_iter_points].y = y;
                next_iter_points++;
                continue;
            }

            // For each point, we look at all surrounding points. Take them
            // orthogonals first so that the travel path doesn't zigzag all over
            // the map. Note the (dir = 1) is intentional assignment.
            for (int dir = 0; dir < 8; (dir += 2) == 8 && (dir = 1))
            {
                int dx = x + Compass[dir].x, dy = y + Compass[dir].y;
               
                if (dx <= 0 || dx >= GXM || dy <= 0 || dy >= GYM) continue;

                unsigned char envf = env.map[dx - 1][dy - 1];

                if (floodout && you.running == RUN_EXPLORE 
                        && !is_player_mapped(envf)) 
                {
                    // Setting run_x and run_y here is evil - this function
                    // should ideally not modify game state in any way.
                    you.run_x = x;
                    you.run_y = y;

                    return;
                }

                if ((dx != dest_x || dy != dest_y) 
                        && !is_travel_ok(dx, dy, ignore_hostile)) 
                {
                    // This point is not okay to travel on, but if this is a 
                    // trap, we'll want to put it on the feature vector anyway.
                    if (is_reseedable(dx, dy) 
                            && !point_distance[dx][dy]
                            && (dx != start_x || dy != start_y))
                    {
                        if (features)
                        {
                            coord_def c = { dx, dy };
                            if (is_trap(dx, dy) || is_exclude_root(dx, dy))
                                features->push_back(c);
                            trap_seeds.push_back(c);
                        }

                        // Appropriate mystic number. Nobody else should check
                        // this number, since this square is unsafe for travel.
                        point_distance[dx][dy] = 
                            is_exclude_root(dx, dy)? PD_EXCLUDED :
                            is_excluded(dx, dy)    ? PD_EXCLUDED_RADIUS :
                                                     PD_TRAP;
                    }
                    continue;
                }

                if (dx == dest_x && dy == dest_y)
                {
                    // Hallelujah, we're home!
                    if (is_safe(x, y) && move_x && move_y)
                    {
                        *move_x = sgn(x - dest_x);
                        *move_y = sgn(y - dest_y);
                    }
                    return ;
                }
                else if (!point_distance[dx][dy])
                {
                    // This point is going to be on the agenda for the next
                    // iteration
                    circumference[!circ_index][next_iter_points].x = dx;
                    circumference[!circ_index][next_iter_points].y = dy;
                    next_iter_points++;

                    point_distance[dx][dy] = traveled_distance;

                    // Negative distances here so that show_map can colour
                    // the map differently for these squares.
                    if (ignore_hostile)
                    {
                        point_distance[dx][dy] = -point_distance[dx][dy];
                        if (is_exclude_root(dx, dy))
                            point_distance[dx][dy] = PD_EXCLUDED;
                        else if (is_excluded(dx, dy))
                            point_distance[dx][dy] = PD_EXCLUDED_RADIUS;
                    }

                    feature = grd[dx][dy];
                    if (features && !ignore_hostile 
                            && ((feature != DNGN_FLOOR 
                                && feature != DNGN_SHALLOW_WATER
                                && feature != DNGN_DEEP_WATER
                                && feature != DNGN_LAVA)
                                || is_waypoint(dx, dy)
#ifdef STASH_TRACKING
                                || is_stash(lev, dx, dy)
#endif
                                )
                            && (dx != start_x || dy != start_y))
                    {
                        coord_def c = { dx, dy };
                        features->push_back(c);
                    }

                    if (features && is_exclude_root(dx, dy) && dx != start_x
                            && dy != start_y)
                    {
                        coord_def c = { dx, dy };
                        features->push_back(c);
                    }
                }
            } // for (dir = 0; dir < 8 ...
        } // for (i = 0; i < points ...

        if (!next_iter_points && features && !move_x && !ignore_hostile 
                && trap_seeds.size())
        {
            // Reseed here
            for (unsigned i = 0; i < trap_seeds.size(); ++i)
                circumference[!circ_index][i] = trap_seeds[i];
            next_iter_points = trap_seeds.size();
            ignore_hostile = true;
        }
    } // for ( ; points > 0 ...
}

// Mappings of which branches spring from which other branches, essential to
// walk backwards up the tree. Yes, this is a horrible abuse of coord_def.
static coord_def branch_backout[] =
{
    { BRANCH_SWAMP, BRANCH_LAIR },
    { BRANCH_SLIME_PITS, BRANCH_LAIR },
    { BRANCH_SNAKE_PIT, BRANCH_LAIR },

    { BRANCH_HALL_OF_BLADES, BRANCH_VAULTS },
    { BRANCH_CRYPT, BRANCH_VAULTS },

    { BRANCH_TOMB, BRANCH_CRYPT },

    { BRANCH_ELVEN_HALLS, BRANCH_ORCISH_MINES },

    { BRANCH_ORCISH_MINES, BRANCH_MAIN_DUNGEON },
    { BRANCH_HIVE, BRANCH_MAIN_DUNGEON },
    { BRANCH_LAIR, BRANCH_MAIN_DUNGEON },
    { BRANCH_VAULTS, BRANCH_MAIN_DUNGEON },
    { BRANCH_HALL_OF_ZOT, BRANCH_MAIN_DUNGEON },
    { BRANCH_ECUMENICAL_TEMPLE, BRANCH_MAIN_DUNGEON },
};

/*
 * Given a branch id, returns the parent branch. If the branch id is not found,
 * returns BRANCH_MAIN_DUNGEON.
 */
unsigned char find_parent_branch(unsigned char br)
{
    for (unsigned i = 0; 
            i < sizeof(branch_backout) / sizeof(branch_backout[0]); 
            i++)
    {
        if (branch_backout[i].x == br)
            return (unsigned char) branch_backout[i].y;
    }
    return 0;
}

extern FixedVector<char, MAX_BRANCHES> stair_level;

void find_parent_branch(unsigned char br, int depth, 
                        unsigned char *pb, int *pd)
{
    int lev = stair_level[br];
    if (lev <= 0)
    {
        *pb = 0;
        *pd = 0;        // Check depth before using *pb.
        return ;
    }

    *pb = find_parent_branch(br);
    *pd = subdungeon_depth(*pb, lev);
}

// Appends the passed in branch/depth to the given vector, then attempts to 
// repeat the operation with the parent branch of the given branch.
// 
// As an example of what it does, assume this dungeon structure
//   Stairs to lair on D:11
//   Stairs to snake pit on lair:5
//
// If level 3 of the snake pit is the level we want to track back from,
// we'd call trackback(vec, BRANCH_SNAKE_PIT, 3), and the resulting vector will 
// look like:
// { BRANCH_SNAKE_PIT, 3 }, { BRANCH_LAIR, 5 }, { BRANCH_MAIN_DUNGEON, 11 }
// (Assuming, of course, that the vector started out empty.)
//
void trackback(std::vector<level_id> &vec, 
               unsigned char branch, int subdepth)
{
    if (subdepth < 1 || subdepth > MAX_LEVELS) return;

    level_id lid( branch, subdepth );
    vec.push_back(lid);

    if (branch != BRANCH_MAIN_DUNGEON)
    {
        unsigned char pb;
        int pd;
        find_parent_branch(branch, subdepth, &pb, &pd);
        if (pd)
            trackback(vec, pb, pd);
    }
}

void track_intersect(std::vector<level_id> &cur, 
                     std::vector<level_id> &targ,
                     level_id *cx)
{
    cx->branch = 0;
    cx->depth  = -1;
    
    int us = cur.size() - 1, them = targ.size() - 1;

    for ( ; us >= 0 && them >= 0; us--, them--)
    {
        if (cur[us].branch != targ[them].branch)
            break;
    }

    us++, them++;

    if (us < (int) cur.size() && them < (int) targ.size() && us >= 0 &&
            them >= 0)
        *cx = targ[them];
}

/*
 * Returns the number of stairs the player would need to take to go from 
 * the 'first' level to the 'second' level. If there's no obvious route between
 * 'first' and 'second', returns -1. If first == second, returns 0.
 */
int level_distance(level_id first, level_id second)
{
    if (first == second) return 0;

    std::vector<level_id> fv, sv;

    // If in the same branch, easy.
    if (first.branch == second.branch)
        return abs(first.depth - second.depth);

    // Figure out the dungeon structure between the two levels.
    trackback(fv, first.branch, first.depth);
    trackback(sv, second.branch, second.depth);

    level_id intersect;
    track_intersect(fv, sv, &intersect);

    if (intersect.depth == -1)          // No common ground?
        return -1;

    int distance = 0;
    // If the common branch is not the same as the current branch, we'll
    // have to walk up the branch tree until we get to the common branch.
    while (first.branch != intersect.branch)
    {
        distance += first.depth;
        
        find_parent_branch(first.branch, first.depth, 
                &first.branch, &first.depth);
        if (!first.depth)
            return -1;
    }

    // Now first.branch == intersect.branch
    distance += abs(first.depth - intersect.depth);

    bool ignore_end = true;
    for (int i = sv.size() - 1; i >= 0; --i)
    {
        if (ignore_end)
        {
            if (sv[i].branch == intersect.branch)
                ignore_end = false;
            continue;
        }
        distance += sv[i].depth;
    }
    return distance;
}

static struct
{
    const char *branch_name, *full_name;
    char hotkey;
} branches [] =
{
    { "Dungeon",        "the Main Dungeon", 'D' },
    { "Dis",            "Dis",              'I' },
    { "Gehenna",        "Gehenna",          'W' },
    { "Hell",           "Hell",             'U' },
    { "Cocytus",        "Cocytus",          'X' },
    { "Tartarus",       "Tartarus",         'Y' },
    { "Inferno",        "",                 'R' },
    { "The Pit",        "",                 '0' },
    { "------------",   "",                 '-' },
    { "------------",   "",                 '-' },
    { "Orcish Mines",   "the Orcish Mines", 'O' },
    { "Hive",           "the Hive",         'H' },
    { "Lair",           "the Lair",         'L' },
    { "Slime Pits",     "the Slime Pits",   'M' },
    { "Vaults",         "the Vaults",       'V' },
    { "Crypt",          "the Crypt",        'C' },
    { "Hall of Blades", "the Hall of Blades", 'B' },
    { "Zot",            "the Realm of Zot", 'Z' },
    { "Temple",         "the Ecumenical Temple",  'T' },
    { "Snake Pit",      "the Snake Pit",    'P' },
    { "Elven Halls",    "the Elven Halls",  'E' },
    { "Tomb",           "the Tomb",         'G' },
    { "Swamp",          "the Swamp",        'S' }
};

static struct
{
    const char *abbr;
    unsigned char branch;
} branch_abbrvs[] =
{
    { "D",      BRANCH_MAIN_DUNGEON },
    { "Dis",    BRANCH_DIS },
    { "Geh",     BRANCH_GEHENNA },
    { "Hell",   BRANCH_VESTIBULE_OF_HELL },
    { "Coc",    BRANCH_COCYTUS },
    { "Tar",    BRANCH_TARTARUS },
    { "inf",    BRANCH_INFERNO },
    { "pit",    BRANCH_THE_PIT },
    { "Orc",    BRANCH_ORCISH_MINES },
    { "Hive",   BRANCH_HIVE },
    { "Lair",   BRANCH_LAIR },
    { "Slime",    BRANCH_SLIME_PITS },
    { "Vault",  BRANCH_VAULTS },
    { "Crypt",  BRANCH_CRYPT },
    { "Blades", BRANCH_HALL_OF_BLADES },
    { "Zot",    BRANCH_HALL_OF_ZOT },
    { "Temple", BRANCH_ECUMENICAL_TEMPLE },
    { "Snake",  BRANCH_SNAKE_PIT },
    { "Elf",    BRANCH_ELVEN_HALLS },
    { "Tomb",   BRANCH_TOMB },
    { "Swamp",  BRANCH_SWAMP },
};

void set_trans_travel_dest(char *buffer, int maxlen, const level_pos &target)
{
    if (!buffer) return;

    const char *branch = NULL;
    for (unsigned i = 0; i < sizeof(branch_abbrvs) / sizeof(branch_abbrvs[0]); 
            ++i)
    {
        if (branch_abbrvs[i].branch == target.id.branch)
        {
            branch = branch_abbrvs[i].abbr;
            break;
        }
    }
    
    if (!branch) return;

    unsigned char branch_id = target.id.branch;
    // Show level+depth information and tack on an @(x,y) if the player
    // wants to go to a specific square on the target level. We don't use 
    // actual coordinates since that will give away level information we
    // don't want the player to have.
    if (branch_id != BRANCH_ECUMENICAL_TEMPLE 
            && branch_id != BRANCH_HALL_OF_BLADES
            && branch_id != BRANCH_VESTIBULE_OF_HELL)
        snprintf(buffer, maxlen, "%s:%d%s", branch, target.id.depth,
            target.pos.x != -1? " @ (x,y)" : "");
    else
        snprintf(buffer, maxlen, "%s%s", branch,
            target.pos.x != -1? " @ (x,y)" : "");
}

// Returns the level on the given branch that's closest to the player's
// current location.
static int get_nearest_level_depth(unsigned char branch)
{
    int depth = 1;

    // Hell needs special treatment, because we can't walk up
    // Hell and its branches to the main dungeon.
    if (branch == BRANCH_MAIN_DUNGEON &&
            (player_in_branch( BRANCH_VESTIBULE_OF_HELL ) ||
             player_in_branch( BRANCH_COCYTUS ) ||
             player_in_branch( BRANCH_TARTARUS ) ||
             player_in_branch( BRANCH_DIS ) ||
             player_in_branch( BRANCH_GEHENNA )))
        return you.hell_exit + 1;

    level_id id = level_id::get_current_level_id();
    do
    {
        find_parent_branch(id.branch, id.depth, 
                &id.branch, &id.depth);
        if (id.depth && id.branch == branch)
        {
            depth = id.depth;
            break;
        }
    } while (id.depth);

    return depth;
}

static char trans_travel_dest[30];

// Returns true if the player character knows of the existence of the given
// branch (which would make the branch a valid target for interlevel travel).
static bool is_known_branch(unsigned char branch)
{
    // The main dungeon is always known.
    if (branch == BRANCH_MAIN_DUNGEON) return true;

    // If we're in the branch, it darn well is known.
    if (you.where_are_you == branch) return true;

    // If the overmap knows the stairs to this branch, we know the branch.
    unsigned char par;
    int pdep;
    find_parent_branch(branch, 1, &par, &pdep);
    if (pdep)
        return true;

    // Do a brute force search in the travel cache for this branch.
    return travel_cache.is_known_branch(branch);

    // Doing this to check for the reachability of a branch is slow enough to
    // be noticeable.

    // Ask interlevel travel if it knows how to go there. If it knows how to
    // get (partway) there, return true.

    // level_pos pos;
    // pos.id.branch = branch;
    // pos.id.depth  = 1;
    
    // return find_transtravel_square(pos, false) != 0;
}

/*
 * Returns a list of the branches that the player knows the location of the
 * stairs to, in the same order as overmap.cc lists them.
 */
static std::vector<unsigned char> get_known_branches()
{
    // Lifted from overmap.cc. XXX: Move this to a better place?
    const unsigned char list_order[] =
    {
        BRANCH_MAIN_DUNGEON, 
        BRANCH_ECUMENICAL_TEMPLE,
        BRANCH_ORCISH_MINES, BRANCH_ELVEN_HALLS,
        BRANCH_LAIR, BRANCH_SWAMP, BRANCH_SLIME_PITS, BRANCH_SNAKE_PIT, 
        BRANCH_HIVE,
        BRANCH_VAULTS, BRANCH_HALL_OF_BLADES, BRANCH_CRYPT, BRANCH_TOMB,
        BRANCH_VESTIBULE_OF_HELL,
        BRANCH_DIS, BRANCH_GEHENNA, BRANCH_COCYTUS, BRANCH_TARTARUS, 
        BRANCH_HALL_OF_ZOT
    };

    std::vector<unsigned char> result;
    for (unsigned i = 0; i < sizeof list_order / sizeof(*list_order); ++i)
    {
        if (is_known_branch(list_order[i]))
            result.push_back(list_order[i]);
    }

    return result;
}

static int prompt_travel_branch()
{
    unsigned char branch = BRANCH_MAIN_DUNGEON;     // Default
    std::vector<unsigned char> br = get_known_branches();

    // Don't kill the prompt even if the only branch we know is the main dungeon
    // This keeps things consistent for the player.
    if (br.size() < 1) return branch;

    bool waypoint_list = false;
    int waycount = travel_cache.get_waypoint_count();
    for ( ; ; )
    {
        char buf[100];
        if (waypoint_list)
            travel_cache.list_waypoints();
        else
        {
            int linec = 0;
            std::string line;
            for (int i = 0, count = br.size(); i < count; ++i, ++linec)
            {
                if (linec == 4)
                {
                    linec = 0;
                    mpr(line.c_str());
                    line = "";
                }
                snprintf(buf, sizeof buf, "(%c) %-14s ", branches[br[i]].hotkey,
                        branches[br[i]].branch_name);
                line += buf;
            }
            if (line.length())
                mpr(line.c_str());
        }

        char shortcuts[100];
        *shortcuts = 0;
        if (*trans_travel_dest || waycount || waypoint_list)
        {
            strncpy(shortcuts, "(", sizeof shortcuts);
            if (waypoint_list)
                strncat(shortcuts, "[*] lists branches", sizeof shortcuts);
            else if (waycount)
                strncat(shortcuts, "[*] lists waypoints", sizeof shortcuts);
            
            if (*trans_travel_dest)
            {
                char travel_dest[60];
                snprintf(travel_dest, sizeof travel_dest, "[Enter] for %s",
                        trans_travel_dest);
                if (waypoint_list || waycount)
                    strncat( shortcuts, ", ", sizeof shortcuts);
                strncat(shortcuts, travel_dest, sizeof shortcuts);
            }
            strncat(shortcuts, ") ", sizeof shortcuts);
        }
        snprintf(buf, sizeof buf, "Where do you want to go? %s", shortcuts);
        mpr(buf, MSGCH_PROMPT);

        int keyin = get_ch();
        switch (keyin)
        {
        case ESCAPE:
            return (ID_CANCEL);
        case '\n': case '\r':
            return (ID_REPEAT);
        case '<':
            return (ID_UP);
        case '>':
            return (ID_DOWN);
        case '*':
            if (waypoint_list || waycount)
            {
                waypoint_list = !waypoint_list;
                mesclr();
                continue;
            }
            break;
        default:
            // Is this a branch hotkey?
            for (int i = 0, count = br.size(); i < count; ++i)
            {
                if (toupper(keyin) == branches[br[i]].hotkey)
                    return (br[i]);
            }

            // Possibly a waypoint number?
            if (keyin >= '0' && keyin <= '9')
                return (-1 - (keyin - '0'));
            return (ID_CANCEL);
        }
    }
}

static int prompt_travel_depth(unsigned char branch)
{
    // Handle one-level branches by not prompting.
    if (branch == BRANCH_ECUMENICAL_TEMPLE || 
            branch == BRANCH_VESTIBULE_OF_HELL ||
            branch == BRANCH_HALL_OF_BLADES)
        return 1;

    char buf[100];
    int depth = get_nearest_level_depth(branch);

    snprintf(buf, sizeof buf, "What level of %s do you want to go to? "
            "[default %d] ", branches[branch].full_name, depth);
    mesclr();
    mpr(buf, MSGCH_PROMPT);

    if (!cancelable_get_line( buf, sizeof buf ))
        return 0;

    if (*buf)
        depth = atoi(buf);

    return depth;
}

static bool is_hell_branch(int branch)
{
    return branch == BRANCH_DIS || branch == BRANCH_TARTARUS 
        || branch == BRANCH_COCYTUS || branch == BRANCH_GEHENNA;

}

static level_pos find_up_level()
{
    level_id curr = level_id::get_current_level_id();
    curr.depth--;

    if (is_hell_branch(curr.branch))
    {
        curr.branch = BRANCH_VESTIBULE_OF_HELL;
        curr.depth  = 1;
        return (curr);
    }

    if (curr.depth < 1)
    {
        if (curr.branch != BRANCH_MAIN_DUNGEON)
        {
            level_id parent;
            find_parent_branch(curr.branch, curr.depth,
                        &parent.branch, &parent.depth);
            if (parent.depth > 0)
                return (parent);
            else if (curr.branch == BRANCH_VESTIBULE_OF_HELL)
            {
                parent.branch = BRANCH_MAIN_DUNGEON;
                parent.depth  = you.hell_exit + 1;
                return (parent);
            }
        }
        return level_pos();
    }

    return (curr);
}

static level_pos find_down_level()
{
    level_id curr = level_id::get_current_level_id();
    curr.depth++;
    return (curr);
}

static level_pos prompt_translevel_target()
{
    level_pos target;
    int branch = prompt_travel_branch();
  
    if (branch == ID_CANCEL)
        return (target);

    // If user chose to repeat last travel, return that.
    if (branch == ID_REPEAT)
        return (travel_target);
    
    if (branch == ID_UP)
    {
        target = find_up_level();
        if (target.id.depth > -1)
            set_trans_travel_dest(trans_travel_dest, sizeof trans_travel_dest,
                                    target);
        return (target);
    }

    if (branch == ID_DOWN)
    {
        target = find_down_level();
        if (target.id.depth > -1)
            set_trans_travel_dest(trans_travel_dest, sizeof trans_travel_dest,
                                    target);
        return (target);
    }

    if (branch < 0)
    {
        travel_cache.travel_to_waypoint(-branch - 1);
        return target;
    }
    
    target.id.branch = branch;

    // User's chosen a branch, so now we ask for a level.
    target.id.depth = prompt_travel_depth(target.id.branch);

    if (target.id.depth < 1 || target.id.depth >= MAX_LEVELS)
        target.id.depth = -1;

    if (target.id.depth > -1)
        set_trans_travel_dest(trans_travel_dest, sizeof trans_travel_dest, 
                target);

    return target;
}

void start_translevel_travel(const level_pos &pos)
{
    travel_target = pos;

    if (pos.id != level_id::get_current_level_id())
    {
        if (!loadlev_populate_stair_distances(pos))
        {
            mpr("Level memory is imperfect, aborting.");
            return ;
        }
    }
    else
        populate_stair_distances(pos);
    
    set_trans_travel_dest(trans_travel_dest, sizeof trans_travel_dest,
                          travel_target);
    start_translevel_travel(false);
}

void start_translevel_travel(bool prompt_for_destination)
{
    // Update information for this level. We need it even for the prompts, so
    // we can't wait to confirm that the user chose to initiate travel.
    travel_cache.get_level_info(level_id::get_current_level_id()).update();

    if (prompt_for_destination)
    {
        // prompt_translevel_target may actually initiate travel directly if
        // the user chose a waypoint instead of a branch + depth. As far as
        // we're concerned, if the target depth is unset, we need to take no
        // further action.
        level_pos target = prompt_translevel_target();
        if (target.id.depth == -1) return;

        travel_target = target;
    }

    if (level_id::get_current_level_id() == travel_target.id &&
            (travel_target.pos.x == -1 || 
             (travel_target.pos.x == you.x_pos &&
             travel_target.pos.y == you.y_pos)))
    {
        mpr("You're already here!");
        return ;
    }

    if (travel_target.id.depth > -1)
    {
        you.running = RUN_INTERLEVEL;
        you.run_x = you.run_y = 0;
        last_stair.depth = -1;
        start_running();
    }
}

command_type stair_direction(int stair)
{
    return ((stair < DNGN_STONE_STAIRS_UP_I
                || stair > DNGN_ROCK_STAIRS_UP)
                && (stair < DNGN_RETURN_FROM_ORCISH_MINES 
                || stair > DNGN_RETURN_FROM_SWAMP))
        ? CMD_GO_DOWNSTAIRS : CMD_GO_UPSTAIRS;
}

command_type trans_negotiate_stairs()
{
    return stair_direction(grd[you.x_pos][you.y_pos]);
}

int absdungeon_depth(unsigned char branch, int subdepth)
{
    int realdepth = subdepth - 1;

    if (branch >= BRANCH_ORCISH_MINES && branch <= BRANCH_SWAMP)
        realdepth = subdepth + you.branch_stairs[branch - 10];

    if (branch >= BRANCH_DIS && branch <= BRANCH_THE_PIT)
        realdepth = subdepth + 26;

    return realdepth;
}

int subdungeon_depth(unsigned char branch, int depth)
{
    int curr_subdungeon_level = depth + 1;

    // maybe last part better expresssed as <= PIT {dlb}
    if (branch >= BRANCH_DIS && branch <= BRANCH_THE_PIT)
        curr_subdungeon_level = depth - 26;

    /* Remember, must add this to the death_string in ouch */
    if (branch >= BRANCH_ORCISH_MINES && branch <= BRANCH_SWAMP)
        curr_subdungeon_level = depth
                                - you.branch_stairs[branch - 10];

    return curr_subdungeon_level;
}

static int target_distance_from(const coord_def &pos)
{
    for (int i = 0, count = curr_stairs.size(); i < count; ++i)
        if (curr_stairs[i].position == pos)
            return curr_stairs[i].distance;
    return -1;
}

/*
 * Sets best_stair to the coordinates of the best stair on the player's current
 * level to take to get to the 'target' level. Should be called with 'distance'
 * set to 0, 'stair' set to (you.x_pos, you.y_pos) and 'best_distance' set to 
 * -1. 'cur' should be the player's current level.
 *
 * If best_stair remains unchanged when this function returns, there is no
 * travel-safe path between the player's current level and the target level OR
 * the player's current level *is* the target level.
 *
 * This function relies on the point_distance array being correctly populated
 * with a floodout call to find_travel_pos starting from the player's location.
 */
static int find_transtravel_stair(  const level_id &cur,
                                    const level_pos &target,
                                    int distance,
                                    const coord_def &stair,
                                    level_id &closest_level,
                                    int &best_level_distance,
                                    coord_def &best_stair)
{
    int local_distance = -1;
    level_id player_level = level_id::get_current_level_id();

    // Have we reached the target level?
    if (cur == target.id)
    {
        // If there's no target position on the target level, or we're on the 
        // target, we're home.
        if (target.pos.x == -1 || target.pos == stair)
            return distance;

        // If there *is* a target position, we need to work out our distance
        // from it.
        int deltadist = target_distance_from(stair);

        if (deltadist == -1 && cur == player_level)
        {
            // Okay, we don't seem to have a distance available to us, which
            // means we're either (a) not standing on stairs or (b) whoever
            // initiated interlevel travel didn't call
            // populate_stair_distances.  Assuming we're not on stairs, that
            // situation can arise only if interlevel travel has been triggered
            // for a location on the same level. If that's the case, we can get
            // the distance off the point_distance matrix.
            deltadist = point_distance[target.pos.x][target.pos.y];
            if (!deltadist && 
                    (stair.x != target.pos.x || stair.y != target.pos.y))
                deltadist = -1;
        }

        if (deltadist != -1)
        {
            local_distance = distance + deltadist;

            // See if this is a degenerate case of interlevel travel:
            // A degenerate case of interlevel travel decays to normal travel;
            // we identify this by checking if:
            // a. The current level is the target level.
            // b. The target square is reachable from the 'current' square.
            // c. The current square is where the player is.
            //
            // Note that even if this *is* degenerate, interlevel travel may
            // still be able to find a shorter route, since it can consider
            // routes that leave and reenter the current level.
            if (player_level == target.id && stair.x == you.x_pos
                    && stair.y == you.y_pos)
                best_stair = target.pos;

            // The local_distance is already set, but there may actually be
            // stairs we can take that'll get us to the target faster than the
            // direct route, so we also try the stairs.
        }
    }

    LevelInfo &li = travel_cache.get_level_info(cur);
    std::vector<stair_info> &stairs = li.get_stairs();

    // this_stair being NULL is perfectly acceptable, since we start with
    // coords as the player coords, and the player need not be standing on
    // stairs.
    stair_info *this_stair = li.get_stair(stair);

    if (!this_stair && cur != player_level)
    {
        // Whoops, there's no stair in the travel cache for the current
        // position, and we're not on the player's current level (i.e., there
        // certainly *should* be a stair here). Since we can't proceed in any
        // reasonable way, bail out.
        return local_distance;
    }

    for (int i = 0, count = stairs.size(); i < count; ++i)
    {
        stair_info &si = stairs[i];
        
        int deltadist = li.distance_between(this_stair, &si);
        if (!this_stair)
        {
            deltadist = point_distance[si.position.x][si.position.y];
            if (!deltadist && 
                    (you.x_pos != si.position.x || you.y_pos != si.position.y))
                deltadist = -1;
        }

        // deltadist == 0 is legal (if this_stair is NULL), since the player
        // may be standing on the stairs. If two stairs are disconnected,
        // deltadist has to be negative.
        if (deltadist < 0) continue;

        int dist2stair = distance + deltadist;
        if (si.distance == -1 || si.distance > dist2stair)
        {
            si.distance = dist2stair;

            // Account for the cost of taking the stairs
            dist2stair += Options.travel_stair_cost;

            // Already too expensive? Short-circuit.
            if (local_distance != -1 && dist2stair >= local_distance)
                continue;

            const level_pos &dest = si.destination;

            // We can only short-circuit the stair-following process if we
            // have no exact target location. If there *is* an exact target
            // location, we can't follow stairs for which we have incomplete
            // information.
            if (target.pos.x == -1 && dest.id == target.id)
            {
                if (local_distance == -1 || local_distance > dist2stair)
                {
                    local_distance = dist2stair;
                    if (cur == player_level && you.x_pos == stair.x &&
                            you.y_pos == stair.y)
                        best_stair = si.position;
                }
                continue;
            }

            if (dest.id.depth > -1) // We have a valid level descriptor.
            {
                int dist = level_distance(dest.id, target.id);
                if (dist != -1 &&
                        (dist < best_level_distance || 
                                best_level_distance == -1)) 
                {
                    best_level_distance = dist;
                    closest_level       = dest.id;
                }
            }

            // If we don't know where these stairs go, we can't take them.
            if (!dest.is_valid()) continue;

            // We need to get the stairs at the new location and set the 
            // distance on them as well.
            LevelInfo &lo = travel_cache.get_level_info(dest.id);
            stair_info *so = lo.get_stair(dest.pos);

            if (so)
            {
                if (so->distance == -1 || so->distance > dist2stair)
                    so->distance = dist2stair;
                else
                    continue;   // We've already been here.
            }

            // Okay, take these stairs and keep going.
            int newdist = find_transtravel_stair(dest.id, target,
                    dist2stair, dest.pos, closest_level,
                    best_level_distance, best_stair);
            if (newdist != -1 && 
                    (local_distance == -1 || local_distance > newdist))
            {
                local_distance = newdist;
                if (cur == player_level && you.x_pos == stair.x &&
                        you.y_pos == stair.y)
                    best_stair = si.position;
            }
        }
    }
    return local_distance;
}

static bool loadlev_populate_stair_distances(const level_pos &target)
{
    crawl_environment tmp = env;
    if (!travel_load_map(target.id.branch, 
                    absdungeon_depth(target.id.branch, target.id.depth)))
    {
        env = tmp;
        return false;
    }

    std::vector<coord_def> old_excludes = curr_excludes;

    curr_excludes.clear();
    LevelInfo &li = travel_cache.get_level_info(target.id);
    li.set_level_excludes();

    populate_stair_distances(target);

    env = tmp;
    curr_excludes = old_excludes;
    return !curr_stairs.empty();
}

static void populate_stair_distances(const level_pos &target)
{
    // Populate point_distance.
    find_travel_pos(target.pos.x, target.pos.y, NULL, NULL, NULL);

    LevelInfo &li = travel_cache.get_level_info(target.id);
    const std::vector<stair_info> &stairs = li.get_stairs();

    curr_stairs.clear();
    for (int i = 0, count = stairs.size(); i < count; ++i)
    {
        stair_info si = stairs[i];
        si.distance = point_distance[si.position.x][si.position.y];
        if (!si.distance && target.pos != si.position)
            si.distance = -1;
        if (si.distance < -1)
            si.distance = -1;

        curr_stairs.push_back(si);
    }
}

static int find_transtravel_square(const level_pos &target, bool verbose)
{
    level_id current = level_id::get_current_level_id();

    coord_def best_stair = { -1, -1 };
    coord_def cur_stair  = { you.x_pos, you.y_pos };
    level_id closest_level;
    int best_level_distance = -1;
    travel_cache.reset_distances();

    find_travel_pos(you.x_pos, you.y_pos, NULL, NULL, NULL);

    find_transtravel_stair(current, target,
                           0, cur_stair, closest_level,
                           best_level_distance, best_stair);

    if (best_stair.x != -1 && best_stair.y != -1)
    {
        you.run_x = best_stair.x;
        you.run_y = best_stair.y;
        return 1;
    }
    else if (best_level_distance != -1 && closest_level != current
            && target.pos.x == -1)
    {
        int current_dist = level_distance(current, target.id);
        level_pos newlev;
        newlev.id = closest_level;
        if (current_dist == -1 || best_level_distance < current_dist)
            return find_transtravel_square(newlev, verbose);
    }

    if (verbose && target.id != current)
        mpr("Sorry, I don't know how to get there.");
    return 0;
}

void start_travel(int x, int y)
{
    // Redundant target?
    if (x == you.x_pos && y == you.y_pos) return ;

    // Start running
    you.running = RUN_TRAVEL;
    you.run_x   = x;
    you.run_y   = y;

    // Remember where we're going so we can easily go back if interrupted.
    you.travel_x = x;
    you.travel_y = y;

    // Check whether we can get to the square.
    find_travel_pos(you.x_pos, you.y_pos, NULL, NULL, NULL);
    
    if (point_distance[x][y] == 0 &&
            (x != you.x_pos || you.run_y != you.y_pos) &&
            is_travel_ok(x, y, false))
    {
        // We'll need interlevel travel to get here.
        travel_target.id = level_id::get_current_level_id();
        travel_target.pos.x = x;
        travel_target.pos.y = y;

        you.running = RUN_INTERLEVEL;
        you.run_x = you.run_y = 0;
        last_stair.depth = -1;

        // We need the distance of the target from the various stairs around.
        populate_stair_distances(travel_target);

        set_trans_travel_dest(trans_travel_dest, sizeof trans_travel_dest, 
                              travel_target);
    }

    start_running();
}

void start_explore()
{
    you.running   = RUN_EXPLORE;
    if (Options.explore_stop)
    {
        // Clone shadow array off map
        copy(env.map, mapshadow);
    }
    start_running();
}

/*
 * Given a feature vector, arranges the features in the order that the player
 * is most likely to be interested in. Currently, the only thing it does is to
 * put altars of the player's religion at the front of the list.
 */
void arrange_features(std::vector<coord_def> &features)
{
    for (int i = 0, count = features.size(); i < count; ++i)
    {
        if (is_player_altar(features[i]))
        {
            int place = i;
            // Shuffle this altar as far up the list as possible.
            for (int j = place - 1; j >= 0; --j)
            {
                if (is_altar(features[j]))
                {
                    if (is_player_altar(features[j]))
                        break;

                    coord_def temp = features[j];
                    features[j] = features[place];
                    features[place] = temp;

                    place = j;
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
// Interlevel travel classes

static void writeCoord(FILE *file, const coord_def &pos)
{
    writeShort(file, pos.x);
    writeShort(file, pos.y);
}

static void readCoord(FILE *file, coord_def &pos)
{
    pos.x = readShort(file);
    pos.y = readShort(file);
}

level_id level_id::get_current_level_id()
{
    level_id id;
    id.branch = you.where_are_you;
    id.depth  = subdungeon_depth(you.where_are_you, you.your_level);

    return id;
}

level_id level_id::get_next_level_id(const coord_def &pos)
{
    short gridc = grd[pos.x][pos.y];
    level_id id = get_current_level_id();

    switch (gridc)
    {
    case DNGN_STONE_STAIRS_DOWN_I:   case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III: case DNGN_ROCK_STAIRS_DOWN:
        id.depth++;
        break;
    case DNGN_STONE_STAIRS_UP_I:     case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:   case DNGN_ROCK_STAIRS_UP:
        id.depth--;
        break;
    case DNGN_ENTER_HELL:
        id.branch = BRANCH_VESTIBULE_OF_HELL;
        id.depth  = 1;
        break;
    case DNGN_ENTER_DIS:
        id.branch = BRANCH_DIS;
        id.depth  = 1;
        break;
    case DNGN_ENTER_GEHENNA:
        id.branch = BRANCH_GEHENNA;
        id.depth  = 1;
        break;
    case DNGN_ENTER_COCYTUS:
        id.branch = BRANCH_COCYTUS;
        id.depth  = 1;
        break;
    case DNGN_ENTER_TARTARUS:
        id.branch = BRANCH_TARTARUS;
        id.depth  = 1;
        break;
    case DNGN_ENTER_ORCISH_MINES:
        id.branch = BRANCH_ORCISH_MINES;
        id.depth  = 1;
        break;
    case DNGN_ENTER_HIVE:
        id.branch = BRANCH_HIVE;
        id.depth  = 1;
        break;
    case DNGN_ENTER_LAIR:
        id.branch = BRANCH_LAIR;
        id.depth  = 1;
        break;
    case DNGN_ENTER_SLIME_PITS:
        id.branch = BRANCH_SLIME_PITS;
        id.depth  = 1;
        break;
    case DNGN_ENTER_VAULTS:
        id.branch = BRANCH_VAULTS;
        id.depth  = 1;
        break;
    case DNGN_ENTER_CRYPT:
        id.branch = BRANCH_CRYPT;
        id.depth  = 1;
        break;
    case DNGN_ENTER_HALL_OF_BLADES:
        id.branch = BRANCH_HALL_OF_BLADES;
        id.depth  = 1;
        break;
    case DNGN_ENTER_ZOT:
        id.branch = BRANCH_HALL_OF_ZOT;
        id.depth  = 1;
        break;
    case DNGN_ENTER_TEMPLE:
        id.branch = BRANCH_ECUMENICAL_TEMPLE;
        id.depth  = 1;
        break;
    case DNGN_ENTER_SNAKE_PIT:
        id.branch = BRANCH_SNAKE_PIT;
        id.depth  = 1;
        break;
    case DNGN_ENTER_ELVEN_HALLS:
        id.branch = BRANCH_ELVEN_HALLS;
        id.depth  = 1;
        break;
    case DNGN_ENTER_TOMB:
        id.branch = BRANCH_TOMB;
        id.depth  = 1;
        break;
    case DNGN_ENTER_SWAMP:
        id.branch = BRANCH_SWAMP;
        id.depth  = 1;
        break;
    case DNGN_RETURN_FROM_ORCISH_MINES:     case DNGN_RETURN_FROM_HIVE:
    case DNGN_RETURN_FROM_LAIR:             case DNGN_RETURN_FROM_SLIME_PITS:
    case DNGN_RETURN_FROM_VAULTS:           case DNGN_RETURN_FROM_CRYPT:
    case DNGN_RETURN_FROM_HALL_OF_BLADES:   case DNGN_RETURN_FROM_ZOT:
    case DNGN_RETURN_FROM_TEMPLE:           case DNGN_RETURN_FROM_SNAKE_PIT:
    case DNGN_RETURN_FROM_ELVEN_HALLS:      case DNGN_RETURN_FROM_TOMB:
    case DNGN_RETURN_FROM_SWAMP:
        find_parent_branch(id.branch, id.depth, &id.branch, &id.depth);
        if (!id.depth)
        {
            id.branch = find_parent_branch(you.where_are_you);
            id.depth  = -1;
        }
        break;
    }
    return id;
}

void level_id::save(FILE *file) const
{
    writeByte(file, branch);
    writeShort(file, depth);
}

void level_id::load(FILE *file)
{
    branch = readByte(file);
    depth  = readShort(file);
}

void level_pos::save(FILE *file) const
{
    id.save(file);
    writeCoord(file, pos);
}

void level_pos::load(FILE *file)
{
    id.load(file);
    readCoord(file, pos);
}

void stair_info::save(FILE *file) const
{
    writeCoord(file, position);
    destination.save(file);
    writeByte(file, guessed_pos? 1 : 0);
}

void stair_info::load(FILE *file)
{
    readCoord(file, position);
    destination.load(file);
    guessed_pos = readByte(file) != 0;
}

LevelInfo::LevelInfo(const LevelInfo &other)
{
    stairs = other.stairs;
    excludes = other.excludes;
    int sz = stairs.size() * stairs.size();
    stair_distances = new short [ sz ];
    if (other.stair_distances)
        memcpy(stair_distances, other.stair_distances, sz * sizeof(int));
}

const LevelInfo &LevelInfo::operator = (const LevelInfo &other)
{
    if (&other == this)
        return *this;

    stairs = other.stairs;
    excludes = other.excludes;
    int sz = stairs.size() * stairs.size();
    delete [] stair_distances;
    stair_distances = new short [ sz ];
    if (other.stair_distances)
        memcpy(stair_distances, other.stair_distances, sz * sizeof(short));
    return *this;
}

LevelInfo::~LevelInfo()
{
    delete [] stair_distances;
}

void LevelInfo::set_level_excludes()
{
    curr_excludes = excludes;
}

void LevelInfo::update()
{
    // We need to update all stair information and distances on this level.
    update_excludes();

    // First, set excludes, so that stair distances will be correctly populated.
    excludes = curr_excludes;

    // First, we get all known stairs
    std::vector<coord_def> stair_positions;
    get_stairs(stair_positions);

    // Make sure our stair list is correct.
    correct_stair_list(stair_positions);

    update_stair_distances();
}

void LevelInfo::update_stair_distances()
{
    // Now we update distances for all the stairs, relative to all other
    // stairs.
    for (int s = 0, end = stairs.size(); s < end; ++s)
    {
        // For each stair, we need to ask travel to populate the distance
        // array.
        find_travel_pos(stairs[s].position.x, stairs[s].position.y, 
                NULL, NULL, NULL);

        for (int other = 0; other < end; ++other)
        {
            int ox = stairs[other].position.x,
                oy = stairs[other].position.y;
            int dist = point_distance[ox][oy];

            // Note dist == 0 is illegal because we can't have two stairs on
            // the same square.
            if (dist <= 0) dist = -1;
            stair_distances[ s * stairs.size() + other ] = dist;
            stair_distances[ other * stairs.size() + s ] = dist;
        }
    }
}

void LevelInfo::update_stair(int x, int y, const level_pos &p, bool guess)
{
    stair_info *si = get_stair(x, y);

    // What 'guess' signifies: whenever you take a stair from A to B, the
    // travel code knows that the stair takes you from A->B. In that case,
    // update_stair() is called with guess == false.
    //
    // Unfortunately, Crawl doesn't guarantee that A->B implies B->A, but the
    // travel code has to assume that anyway (because that's what the player
    // will expect), and call update_stair() again with guess == true.
    //
    // The idea of using 'guess' is that we'll update the stair's destination
    // with a guess only if we know that the currently set destination is
    // itself a guess.
    //
    if (si && (si->guessed_pos || !guess))
    {
        si->destination = p;
        si->guessed_pos = guess;

        if (!guess && p.id.branch == BRANCH_VESTIBULE_OF_HELL
                && id.branch == BRANCH_MAIN_DUNGEON)
            travel_hell_entry = p;
    }
}

stair_info *LevelInfo::get_stair(int x, int y)
{
    coord_def c = { x, y };
    return get_stair(c);
}

stair_info *LevelInfo::get_stair(const coord_def &pos)
{
    int index = get_stair_index(pos);
    return index != -1? &stairs[index] : NULL;
}

int LevelInfo::get_stair_index(const coord_def &pos) const
{
    for (int i = stairs.size() - 1; i >= 0; --i)
    {
        if (stairs[i].position == pos)
            return i;
    }
    return -1;
}

void LevelInfo::add_waypoint(const coord_def &pos)
{
    if (pos.x < 0 || pos.y < 0) return;
        
    // First, make sure we don't already have this position in our stair list.
    for (int i = 0, sz = stairs.size(); i < sz; ++i)
        if (stairs[i].position == pos)
            return;

    stair_info si;
    si.position = pos;
    si.destination.id.depth = -2;   // Magic number for waypoints.

    stairs.push_back(si);

    delete [] stair_distances;
    stair_distances = new short [ stairs.size() * stairs.size() ];

    update_stair_distances();
}

void LevelInfo::remove_waypoint(const coord_def &pos)
{
    for (std::vector<stair_info>::iterator i = stairs.begin(); 
            i != stairs.end(); ++i)
    {
        if (i->position == pos && i->destination.id.depth == -2)
        {
            stairs.erase(i);
            break;
        }
    }

    delete [] stair_distances;
    stair_distances = new short [ stairs.size() * stairs.size() ];

    update_stair_distances();
}

void LevelInfo::correct_stair_list(const std::vector<coord_def> &s)
{
    // If we have a waypoint on this level, we'll always delete stair_distances 
    delete [] stair_distances;
    stair_distances = NULL;

    // First we kill any stairs in 'stairs' that aren't there in 's'.
    for (std::vector<stair_info>::iterator i = stairs.begin(); 
            i != stairs.end(); ++i)
    {
        // Waypoints are not stairs, so we skip them.
        if (i->destination.id.depth == -2) continue;
        
        bool found = false;
        for (int j = s.size() - 1; j >= 0; --j)
        {
            if (s[j] == i->position)
            {
                found = true;
                break;
            }
        }

        if (!found)
            stairs.erase(i--);
    }

    // For each stair in 's', make sure we have a corresponding stair
    // in 'stairs'.
    for (int i = 0, sz = s.size(); i < sz; ++i)
    {
        bool found = false;
        for (int j = stairs.size() - 1; j >= 0; --j)
        {
            if (s[i] == stairs[j].position)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            stair_info si;
            si.position = s[i];
            si.destination.id = level_id::get_next_level_id(s[i]);
            if (si.destination.id.branch == BRANCH_VESTIBULE_OF_HELL
                    && id.branch == BRANCH_MAIN_DUNGEON
                    && travel_hell_entry.is_valid())
                si.destination = travel_hell_entry;

            // We don't know where on the next level these stairs go to, but 
            // that can't be helped. That information will have to be filled
            // in whenever the player takes these stairs.
            stairs.push_back(si);
        }
    }

    stair_distances = new short [ stairs.size() * stairs.size() ];
}

int LevelInfo::distance_between(const stair_info *s1, const stair_info *s2) 
                    const 
{
    if (!s1 || !s2) return 0;
    if (s1 == s2) return 0;

    int i1 = get_stair_index(s1->position),
        i2 = get_stair_index(s2->position);
    if (i1 == -1 || i2 == -1) return 0;
    
    return stair_distances[ i1 * stairs.size() + i2 ];
}

void LevelInfo::get_stairs(std::vector<coord_def> &st)
{
    // These are env map coords, not grid coordinates.
    for (int y = 0; y < GYM - 1; ++y)
    {
        for (int x = 0; x < GXM - 1; ++x)
        {
            unsigned char grid = grd[x + 1][y + 1];
            unsigned char envc = (unsigned char) env.map[x][y];

            if (envc && is_travelable_stair(grid))
            {
                // Convert to grid coords, because that's what we use
                // everywhere else.
                coord_def stair = { x + 1, y + 1 };
                st.push_back(stair);
            }
        }
    }
}

void LevelInfo::reset_distances()
{
    for (int i = 0, count = stairs.size(); i < count; ++i)
    {
        stairs[i].reset_distance();
    }
}

bool LevelInfo::is_known_branch(unsigned char branch) const
{
    for (int i = 0, count = stairs.size(); i < count; ++i)
    {
        if (stairs[i].destination.id.branch == branch)
            return true;
    }
    return false;
}

void LevelInfo::travel_to_waypoint(const coord_def &pos)
{
    stair_info *target = get_stair(pos);
    if (!target) return;

    curr_stairs.clear();
    for (int i = 0, sz = stairs.size(); i < sz; ++i)
    {
        if (stairs[i].destination.id.depth == -2) continue;
        
        stair_info si = stairs[i];
        si.distance   = distance_between(target, &stairs[i]);

        curr_stairs.push_back(si);
    }
    
    start_translevel_travel(false);
}

void LevelInfo::save(FILE *file) const
{
    int stair_count = stairs.size();
    // How many stairs do we know of?
    writeShort(file, stair_count);
    for (int i = 0; i < stair_count; ++i)
        stairs[i].save(file);

    if (stair_count)
    {
        // XXX Assert stair_distances != NULL?
        // Save stair distances as short ints.
        for (int i = stair_count * stair_count - 1; i >= 0; --i)
            writeShort(file, stair_distances[i]);
    }

    writeShort(file, excludes.size());
    if (excludes.size())
    {
        for (int i = 0, count = excludes.size(); i < count; ++i)
        {
            writeShort(file, excludes[i].x);
            writeShort(file, excludes[i].y);
        }
    }
}

void LevelInfo::load(FILE *file)
{
    stairs.clear();
    int stair_count = readShort(file);
    for (int i = 0; i < stair_count; ++i)
    {
        stair_info si;
        si.load(file);
        stairs.push_back(si);

        if (id.branch == BRANCH_MAIN_DUNGEON &&
                si.destination.id.branch == BRANCH_VESTIBULE_OF_HELL &&
                !travel_hell_entry.is_valid() &&
                si.destination.is_valid())
            travel_hell_entry = si.destination;
    }

    if (stair_count)
    {
        delete [] stair_distances;
        stair_distances = new short [ stair_count * stair_count ];
        for (int i = stair_count * stair_count - 1; i >= 0; --i)
            stair_distances[i] = readShort(file);
    }

    excludes.clear();
    int nexcludes = readShort(file);
    if (nexcludes)
    {
        for (int i = 0; i < nexcludes; ++i)
        {
            coord_def c;
            c.x = readShort(file);
            c.y = readShort(file);
            excludes.push_back(c);
        }
    }
}

void LevelInfo::fixup()
{
    // The only fixup we do now is for the hell entry.
    if (id.branch != BRANCH_MAIN_DUNGEON || !travel_hell_entry.is_valid())
        return;
    for (int i = 0, count = stairs.size(); i < count; ++i)
    {
        stair_info &si = stairs[i];
        if (si.destination.id.branch == BRANCH_VESTIBULE_OF_HELL
                && !si.destination.is_valid())
            si.destination = travel_hell_entry;
    }
}

void TravelCache::travel_to_waypoint(int num)
{
    if (num < 0 || num >= TRAVEL_WAYPOINT_COUNT) return;
    if (waypoints[num].id.depth == -1) return;

    travel_target = waypoints[num];
    set_trans_travel_dest(trans_travel_dest, sizeof trans_travel_dest, 
                        travel_target);
    LevelInfo &li = get_level_info(travel_target.id);
    li.travel_to_waypoint(travel_target.pos);
}

void TravelCache::list_waypoints() const
{
    std::string line;
    char dest[30];
    char choice[50];
    int count = 0;
    
    for (int i = 0; i < TRAVEL_WAYPOINT_COUNT; ++i)
    {
        if (waypoints[i].id.depth == -1) continue;

        set_trans_travel_dest(dest, sizeof dest, waypoints[i]);
        // All waypoints will have @ (x,y), remove that.
        char *at = strchr(dest, '@');
        if (at)
            *--at = 0;

        snprintf(choice, sizeof choice, "(%d) %-8s", i, dest);
        line += choice;
        if (!(++count % 5))
        {
            mpr(line.c_str());
            line = "";
        }
    }
    if (line.length())
        mpr(line.c_str());
}

unsigned char TravelCache::is_waypoint(const level_pos &lp) const
{
    for (int i = 0; i < TRAVEL_WAYPOINT_COUNT; ++i)
    {
        if (lp == waypoints[i])
            return '0' + i;
    }
    return 0;
}

void TravelCache::update_waypoints() const
{
    level_pos lp;
    lp.id = level_id::get_current_level_id();

    memset(curr_waypoints, 0, sizeof curr_waypoints);
    for (lp.pos.x = 1; lp.pos.x < GXM; ++lp.pos.x)
    {
        for (lp.pos.y = 1; lp.pos.y < GYM; ++lp.pos.y)
        {
            unsigned char wpc = is_waypoint(lp);
            if (wpc)
                curr_waypoints[lp.pos.x][lp.pos.y] = wpc;
        }
    }
}

void TravelCache::add_waypoint(int x, int y)
{
    if (you.level_type == LEVEL_LABYRINTH || you.level_type == LEVEL_ABYSS
            || you.level_type == LEVEL_PANDEMONIUM)
    {
        mpr("Sorry, you can't set a waypoint here.");
        return;
    }
    
    mesclr();
    if (get_waypoint_count())
    {
        mpr("Existing waypoints");
        list_waypoints();
    }
    
    mpr("Assign waypoint to what number? (0-9) ", MSGCH_PROMPT);
    int keyin = get_ch();
    
    if (keyin < '0' || keyin > '9') return;

    int waynum = keyin - '0';

    if (waypoints[waynum].is_valid())
    {
        bool unique_waypoint = true;
        for (int i = 0; i < TRAVEL_WAYPOINT_COUNT; ++i)
        {
            if (i == waynum) continue;
            if (waypoints[waynum] == waypoints[i])
            {
                unique_waypoint = false;
                break;
            }
        }

        if (unique_waypoint)
        {
            LevelInfo &li = get_level_info(waypoints[waynum].id);
            li.remove_waypoint(waypoints[waynum].pos);
        }
    }
    
    if (x == -1 || y == -1)
    {
        x = you.x_pos;
        y = you.y_pos;
    }
    coord_def pos = { x, y };
    const level_id &lid = level_id::get_current_level_id();

    LevelInfo &li = get_level_info(lid);
    li.add_waypoint(pos);

    waypoints[waynum].id  = lid;
    waypoints[waynum].pos = pos;

    update_waypoints();
}

int TravelCache::get_waypoint_count() const
{
    int count = 0;
    for (int i = 0; i < TRAVEL_WAYPOINT_COUNT; ++i)
        if (waypoints[i].is_valid())
            count++;
    return count;
}

void TravelCache::reset_distances()
{
    std::map<level_id, LevelInfo, level_id::less_than>::iterator i =
                    levels.begin();
    for ( ; i != levels.end(); ++i)
        i->second.reset_distances();
}

bool TravelCache::is_known_branch(unsigned char branch) const
{
    std::map<level_id, LevelInfo, level_id::less_than>::const_iterator i =
                    levels.begin();
    for ( ; i != levels.end(); ++i)
        if (i->second.is_known_branch(branch))
            return true;
    return false;
}

void TravelCache::save(FILE *file) const
{
    // Travel cache version information
    writeByte(file, TC_MAJOR_VERSION);
    writeByte(file, TC_MINOR_VERSION);

    // How many levels do we have?
    writeShort(file, levels.size());

    // Save all the levels we have
    std::map<level_id, LevelInfo, level_id::less_than>::const_iterator i =
                levels.begin();
    for ( ; i != levels.end(); ++i)
    {
        i->first.save(file);
        i->second.save(file);
    }

    for (int wp = 0; wp < TRAVEL_WAYPOINT_COUNT; ++wp)
        waypoints[wp].save(file);
}

void TravelCache::load(FILE *file)
{
    levels.clear();

    // Check version. If not compatible, we just ignore the file altogether.
    unsigned char major = readByte(file),
                  minor = readByte(file);
    if (major != TC_MAJOR_VERSION || minor != TC_MINOR_VERSION) return ;

    int level_count = readShort(file);
    for (int i = 0; i < level_count; ++i)
    {
        level_id id;
        id.load(file);
        LevelInfo linfo;
        // Must set id before load, or travel_hell_entry will not be
        // correctly set.
        linfo.id = id;
        linfo.load(file);
        levels[id] = linfo;
    }

    for (int wp = 0; wp < TRAVEL_WAYPOINT_COUNT; ++wp)
        waypoints[wp].load(file);

    fixup_levels();
}

void TravelCache::set_level_excludes()
{
    if (can_travel_interlevel())
        get_level_info(level_id::get_current_level_id()).set_level_excludes();
}

void TravelCache::update()
{
    if (can_travel_interlevel())
        get_level_info(level_id::get_current_level_id()).update();
    else
        update_excludes();
}

void TravelCache::fixup_levels()
{
    std::map<level_id, LevelInfo, level_id::less_than>::iterator i =
                levels.begin();
    for ( ; i != levels.end(); ++i)
        i->second.fixup();
}

bool can_travel_interlevel()
{
    return (player_in_mappable_area() && you.level_type != LEVEL_PANDEMONIUM);
}
