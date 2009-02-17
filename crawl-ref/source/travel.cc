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
REVISION("$Rev$");

#include "files.h"
#include "FixAry.h"
#include "branch.h"
#include "command.h"
#include "cio.h"
#include "cloud.h"
#include "clua.h"
#include "delay.h"
#include "describe.h"
#include "dgnevent.h"
#include "directn.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "message.h"
#include "misc.h"
#include "mon-util.h"
#include "monstuff.h"
#ifdef USE_TILE
 #include "output.h"
#endif
#include "overmap.h"
#include "place.h"
#include "player.h"
#include "stash.h"
#include "stuff.h"
#include "tags.h"
#include "terrain.h"
#ifdef USE_TILE
 #include "tiles.h"
#endif
#include "traps.h"
#include "travel.h"
#include "tutorial.h"
#include "view.h"

#include <algorithm>
#include <set>
#include <cstdarg>
#include <cctype>
#include <cstdio>
#include <memory>
#include <sstream>

#ifdef DOS
#include <dos.h>
#endif

#define TC_MAJOR_VERSION ((unsigned char) 4)
#define TC_MINOR_VERSION ((unsigned char) 9)

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
static std::vector<travel_exclude> curr_excludes;

// This is where we last tried to take a stair during interlevel travel.
// Note that last_stair.depth should be set to -1 before initiating interlevel
// travel.
static level_id last_stair;

// Where travel wants to get to.
static travel_target level_target;

// How many stairs there are between the source and destination of
// interlevel travel, as estimated by level_distance.
static int _Src_Dest_Level_Delta = -1;

// Source level where interlevel travel was last activated.
static level_id _Src_Level;

// Remember the last place explore stopped because autopickup failed.
static coord_def explore_stopped_pos;

// The place in the Vestibule of Hell where all portals to Hell land.
static level_pos travel_hell_entry;

static bool traps_inited = false;

static std::string trans_travel_dest;

// Array of points on the map, each value being the distance the character
// would have to travel to get there. Negative distances imply that the point
// is a) a trap or hostile terrain or b) only reachable by crossing a trap or
// hostile terrain.
travel_distance_grid_t travel_point_distance;

static unsigned char curr_waypoints[GXM][GYM];
#ifdef CLUA_BINDINGS
static signed char curr_traps[GXM][GYM];
#endif

static FixedArray< map_cell, GXM, GYM >  mapshadow;

const signed char TRAVERSABLE = 1;
const signed char IMPASSABLE  = 0;
const signed char FORBIDDEN   = -1;

// Map of terrain types that are traversable.
static signed char traversable_terrain[256];

/*
 * Warn if interlevel travel is going to take you outside levels in
 * the range [src,dest].
 */
class deviant_route_warning
{
private:
    travel_target target;
    bool warned;

public:
    deviant_route_warning(): target(), warned(false)
    {
    }

    void new_dest(const travel_target &dest);
    bool warn_continue_travel(const travel_target &des,
                              const level_id &deviant);
};

void deviant_route_warning::new_dest(const travel_target &dest)
{
    if (target != dest)
    {
        warned = false;
        target = dest;
    }
}

// Returns true if the player wants to continue travelling after the warning.
bool deviant_route_warning::warn_continue_travel(
    const travel_target &dest, const level_id &deviant)
{
    // We've already prompted, don't ask again, on the player's head be it.
    if (target == dest && warned)
        return (true);

    target = dest;
    const std::string prompt =
        make_stringf("Have to go through %s. Continue?",
                     deviant.describe().c_str());
    // If the user says "Yes, shut up and take me there", we won't ask
    // again for that destination. If the user says "No", we will
    // prompt again.
    cursor_control con(true);
    return ((warned = yesno(prompt.c_str(), true, 'n', true, false)));
}

static deviant_route_warning _Route_Warning;

static command_type _trans_negotiate_stairs();
static bool _find_transtravel_square(const level_pos &pos,
                                     bool verbose = true);

static bool _loadlev_populate_stair_distances(const level_pos &target);
static void _populate_stair_distances(const level_pos &target);
static bool _is_greed_inducing_square(const LevelStashes *ls,
                                      const coord_def &c);

bool is_player_seen(int grid_x, int grid_y)
{
    return (is_terrain_seen(grid_x, grid_y));
}

// Returns true if there is a known trap at (x,y). Returns false for non-trap
// squares as also for undiscovered traps.
//
inline bool is_trap(const coord_def& c)
{
    return grid_is_trap(grd(c));
}

// Returns an estimate for the time needed to cross this feature.
// This is done, so traps etc. will usually be circumvented where possible.
inline int feature_traverse_cost(dungeon_feature_type feature)
{
    if (feature == DNGN_SHALLOW_WATER || feature == DNGN_CLOSED_DOOR)
        return 2;
    else if (grid_is_trap(feature))
        return 3;

    return 1;
}

// Returns true if the dungeon feature supplied is an altar.
bool is_altar(dungeon_feature_type grid)
{
    return grid_altar_god(grid) != GOD_NO_GOD;
}

bool is_altar(const coord_def &c)
{
    return is_altar(grd(c));
}

inline bool is_player_altar(dungeon_feature_type grid)
{
    // An ugly hack, but that's what religion.cc does.
    return (you.religion != GOD_NO_GOD
            && grid_altar_god(grid) == you.religion);
}

inline bool is_player_altar(const coord_def &c)
{
    return is_player_altar(grd(c));
}

#ifdef CLUA_BINDINGS
static void _init_traps()
{
    memset(curr_traps, -1, sizeof curr_traps);
    for (int i = 0; i < MAX_TRAPS; ++i)
    {
        int x = env.trap[i].pos.x,
            y = env.trap[i].pos.y;

        if (inside_level_bounds(x,y))
            curr_traps[x][y] = i;
    }
    traps_inited = true;
}

const char *trap_name(const coord_def& c)
{
    if (!traps_inited)
        _init_traps();

    const int ti = curr_traps[c.x][c.y];
    if (ti != -1)
    {
        int type = env.trap[ti].type;
        if (type >= 0 && type < NUM_TRAPS)
            return (trap_name(trap_type(type)));
    }
    return ("");
}
#endif

// Returns true if the character can cross this dungeon feature.
bool is_traversable(dungeon_feature_type grid)
{
    return (traversable_terrain[grid] == TRAVERSABLE);
}

static feature_grid _map_to_grid(coord_def &pos)
{
    feature_grid fgrid = env.grid;

    for (int x = pos.x - LOS_RADIUS; x < pos.x + LOS_RADIUS; x++)
        for (int y = pos.y - LOS_RADIUS; y < pos.y + LOS_RADIUS; y++)
        {
            if (!in_bounds(x,y))
                continue;

            // For unseen terrain, assume transparency.
            if (!is_terrain_seen(x,y))
            {
                fgrid[x][y] = DNGN_FLOOR;
                continue;
            }

            if (!is_terrain_changed(x,y))
                continue;

            if (env.map[x][y].object < NUM_REAL_FEATURES)
                fgrid[x][y] = (dungeon_feature_type) env.map[x][y].object;
            else
            {
                // If you have seen monsters, items or clouds there,
                // it must have been passable.
                fgrid[x][y] = DNGN_FLOOR;
            }
        }

    return fgrid;
}

void travel_exclude::set_exclude_show()
{
    // ignores clouds
    feature_grid fgrid = _map_to_grid(pos);
    losight(show, fgrid, pos, false, true);
    uptodate = true;
}

void init_exclusion_los()
{
    for (unsigned int i = 0; i < curr_excludes.size(); i++)
        curr_excludes[i].set_exclude_show();
}

void update_exclusion_los(const coord_def &p)
{
    for (unsigned int i = 0; i < curr_excludes.size(); i++)
        if (!curr_excludes[i].uptodate
            && (curr_excludes[i].pos - p).abs() <= LOS_RADIUS * LOS_RADIUS)
        {
            curr_excludes[i].set_exclude_show();
        }
}

void mark_all_excludes_non_updated()
{
    for (unsigned int i = 0; i < curr_excludes.size(); i++)
        curr_excludes[i].uptodate = false;
}

static bool _is_excluded(const coord_def &p,
                         const std::vector<travel_exclude> &exc)
{
    for (int i = 0, count = exc.size(); i < count; ++i)
        if ((exc[i].pos - p).abs() < exc[i].radius_sq()
            && see_grid(exc[i].show, exc[i].pos, p))
        {
            return (true);
        }

    return (false);
}

bool is_excluded(const coord_def &p)
{
    return _is_excluded(p, curr_excludes);
}

static travel_exclude *_find_exclude_root(const coord_def &p)
{
    for (int i = 0, count = curr_excludes.size(); i < count; ++i)
        if (curr_excludes[i].pos == p)
            return (&curr_excludes[i]);

    return (NULL);
}

bool is_exclude_root(const coord_def &p)
{
    return (_find_exclude_root(p));
}

#ifdef USE_TILE
// update Gmap for squares surrounding exclude centre
static void _tile_exclude_gmap_update(const coord_def p)
{
    for (int x = -8; x <= 8; x++)
        for (int y = -8; y <= 8; y++)
        {
            int px = p.x+x, py = p.y+y;
            if (in_bounds(coord_def(px,py)))
            {
                tiles.update_minimap(px, py);
            }
        }
}
#endif

const char *run_mode_name(int runmode)
{
    return (runmode == RMODE_TRAVEL         ? "travel" :
            runmode == RMODE_INTERLEVEL     ? "intertravel" :
            runmode == RMODE_EXPLORE        ? "explore" :
            runmode == RMODE_EXPLORE_GREEDY ? "explore_greedy" :
            runmode > 0                     ? "run"
                                            : "");
}

unsigned char is_waypoint(const coord_def &p)
{
    if (!can_travel_interlevel())
        return 0;
    return curr_waypoints[p.x][p.y];
}

inline bool is_stash(const LevelStashes *ls, int x, int y)
{
    if (!ls)
        return (false);

    const Stash *s = ls->find_stash(x, y);
    return s && s->enabled;
}

void clear_excludes()
{
    // Sanity checks
    if (!player_in_mappable_area())
        return;

#ifdef USE_TILE
    for (int i = curr_excludes.size()-1; i >= 0; i--)
         toggle_exclude(curr_excludes[i].pos);
#endif
    curr_excludes.clear();

    if (can_travel_interlevel())
    {
        LevelInfo &li = travel_cache.get_level_info(level_id::current());
        li.update();
    }
}

void cycle_exclude_radius(const coord_def &p)
{
    if (travel_exclude *exc = _find_exclude_root(p))
    {
        int &curr_radius = exc->radius;

        switch (curr_radius)
        {
        case LOS_RADIUS: curr_radius = 1; break;
        case 1         : set_exclude(p, 0); break;
        }

#ifdef USE_TILE
        _tile_exclude_gmap_update(p);
#endif

        if (can_travel_interlevel())
        {
            LevelInfo &li = travel_cache.get_level_info(level_id::current());
            li.update();
        }
    }
}

void toggle_exclude(const coord_def &p)
{
    if (is_exclude_root(p))
        set_exclude(p, 0);
    else
        set_exclude(p, LOS_RADIUS);

#ifdef USE_TILE
    _tile_exclude_gmap_update(p);
#endif
}

void set_exclude(const coord_def &p, int radius)
{
    // Sanity checks; excludes can be set in Pan and regular dungeon
    // levels only.
    if (!player_in_mappable_area())
        return;

    if (!in_bounds(p))
        return;

    if (is_exclude_root(p))
    {
        for (int i = 0, count = curr_excludes.size(); i < count; ++i)
        {
            if (curr_excludes[i].pos == p)
            {
                if (!radius)
                {
                    curr_excludes.erase( curr_excludes.begin() + i );
                    break ;
                }
                else
                {
                    curr_excludes[i].radius = radius;
                    return;
                }
            }
        }
    }
    else
    {
        curr_excludes.push_back(travel_exclude(p, radius));
    }

    if (can_travel_interlevel())
    {
        LevelInfo &li = travel_cache.get_level_info(level_id::current());
        li.update();
    }
}

static bool _is_monster_blocked(const coord_def& c)
{
    const monsters *mons = monster_at(c);
    return (mons
            && player_monster_visible(mons)
            && mons_is_stationary(mons)
            && mons_was_seen(mons)
            && !mons_is_unknown_mimic(mons));
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
static bool _is_reseedable(const coord_def& c)
{
    if (is_excluded(c))
        return (true);

    const dungeon_feature_type grid = grd(c);
    return (grid_is_water(grid)
               || grid == DNGN_LAVA
               || is_trap(c)
               || _is_monster_blocked(c));
}

// Returns true if the square at (x,y) is okay to travel over. If ignore_hostile
// is true, returns true even for dungeon features the character can normally
// not cross safely (deep water, lava, traps).
bool is_travelsafe_square(const coord_def& c, bool ignore_hostile,
                          bool ignore_terrain_knowledge)
{
    if (!ignore_terrain_knowledge && !is_terrain_known(c))
        return (false);

    const bool seen = see_grid(c);
    const int grid = ((seen || ignore_terrain_knowledge) ? grd(c)
                                                         : get_envmap_obj(c));

    // FIXME: this compares to the *real* monster at the square,
    // even if the one we've seen is different.
    if (!ignore_hostile
        && (seen || grid > DNGN_START_OF_MONSTERS)
        && _is_monster_blocked(c))
    {
        return (false);
    }

    // If 'ignore_hostile' is true, we're ignoring hazards that can be
    // navigated over if the player is willing to take damage, or levitate.
    if (ignore_hostile
        && (seen || grid < NUM_REAL_FEATURES)
        && _is_reseedable(c))
    {
        return (true);
    }

    if (grid >= NUM_REAL_FEATURES)
        return (true);

    return (is_traversable(static_cast<dungeon_feature_type>(grid))
#ifdef CLUA_BINDINGS
                || (is_trap(c)
                    && clua.callbooleanfn(false, "ch_cross_trap",
                                          "s", trap_name(c)))
#endif
            )
            && !is_excluded(c);
}

// Returns true if the location at (x,y) is monster-free and contains no clouds.
// Travel uses this to check if the square the player is about to move to is
// safe.
static bool _is_safe_move(const coord_def& c)
{
    if (const monsters *mon = monster_at(c))
    {
        // Stop before wasting energy on plants and fungi.
        if (you.can_see(mon) && mons_class_flag(mon->type, M_NO_EXP_GAIN))
            return (false);

        // If this is any *other* monster, it'll be visible and
        // a) Friendly, in which case we'll displace it, no problem.
        // b) Unfriendly, in which case we're in deep trouble, since travel
        //    should have been aborted already by the checks in view.cc.
    }

    if (is_trap(c)
#ifdef CLUA_BINDINGS
        && !clua.callbooleanfn(false, "ch_cross_trap",
                               "s", trap_name(c))
#endif
        )
    {
        return (false);
    }

    const int cloud = env.cgrid(c);
    if (cloud == EMPTY_CLOUD)
        return (true);

    // We can also safely run through smoke.
    const cloud_type ctype = env.cloud[cloud].type;
    return (!is_damaging_cloud(ctype, true));
}

static void _set_pass_feature(unsigned char grid, signed char pass)
{
    if (traversable_terrain[(unsigned) grid] != FORBIDDEN)
        traversable_terrain[(unsigned) grid] = pass;
}

// Sets traversable terrain based on the character's role and whether or not he
// has permanent levitation
void init_travel_terrain_check(bool check_race_equip)
{
    if (check_race_equip)
    {
        // Swimmers get deep water.
        signed char water = (player_likes_water(true) ? TRAVERSABLE
                                                      : IMPASSABLE);

        // If the player has overridden deep water already, we'll respect that.
        _set_pass_feature(DNGN_DEEP_WATER, water);

        // Permanently levitating players can cross most hostile terrain.
        const signed char trav = (you.permanent_levitation()
                                  || you.permanent_flight() ? TRAVERSABLE
                                                            : IMPASSABLE);

        if (water != TRAVERSABLE)
            _set_pass_feature(DNGN_DEEP_WATER, trav);
        _set_pass_feature(DNGN_LAVA, trav);
        _set_pass_feature(DNGN_TRAP_MECHANICAL, trav);
        // Shafts can also be levitated over.
        _set_pass_feature(DNGN_TRAP_NATURAL, trav);
    }
    else
    {
        _set_pass_feature(DNGN_DEEP_WATER, IMPASSABLE);
        _set_pass_feature(DNGN_LAVA, IMPASSABLE);
        _set_pass_feature(DNGN_TRAP_MECHANICAL, IMPASSABLE);
    }
}

void travel_init_new_level()
{
    // Clear run details, but preserve the runmode, because we might be in
    // the middle of interlevel travel.
    int runmode = you.running;
    you.running.clear();
    you.running = runmode;

    // Zero out last travel coords
    you.travel_x = you.travel_y  = 0;

    traps_inited = false;
    curr_excludes.clear();
    travel_cache.set_level_excludes();
    travel_cache.update_waypoints();

    explore_stopped_pos.reset();
}

// Sets up travel-related stuff.
void initialise_travel()
{
    for (int feat = DNGN_FLOOR_MIN; feat < NUM_REAL_FEATURES; feat++)
    {
        if (feat >= DNGN_TRAP_MECHANICAL && feat <= DNGN_TRAP_NATURAL)
            continue;

        traversable_terrain[feat] = TRAVERSABLE;
    }
    // A few special cases...
    traversable_terrain[DNGN_CLOSED_DOOR] =
    traversable_terrain[DNGN_SHALLOW_WATER] = TRAVERSABLE;
}

// Given a dungeon feature description, returns the feature number. This is a
// crude hack and currently recognises only (deep/shallow) water. (XXX)
//
// Returns -1 if the feature named is not recognised, else returns the feature
// number (guaranteed to be 0-255).
int get_feature_type(const std::string &feature)
{
    if (feature.find("deep water") != std::string::npos)
        return (DNGN_DEEP_WATER);
    if (feature.find("shallow water") != std::string::npos)
        return (DNGN_SHALLOW_WATER);
    return -1;
}

// Given a feature description, prevents travel to locations of that feature
// type.
void prevent_travel_to(const std::string &feature)
{
    int feature_type = get_feature_type(feature);
    if (feature_type != -1)
        traversable_terrain[feature_type] = FORBIDDEN;
}

bool is_branch_stair(const coord_def& pos)
{
    const level_id curr = level_id::current();
    const level_id next = level_id::get_next_level_id(pos);

    return (next.branch != curr.branch);
}

// Returns true if the given dungeon feature is a stair, i.e., a level
// exit.
bool is_stair(dungeon_feature_type gridc)
{
    return (is_travelable_stair(gridc) || is_gate(gridc));
}

// Returns true if the given dungeon feature is a travelable stair, i.e.,
// it's a level exit with a consistent endpoint.
bool is_travelable_stair(dungeon_feature_type gridc)
{
    switch (gridc)
    {
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
    case DNGN_ESCAPE_HATCH_DOWN:
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
    case DNGN_ESCAPE_HATCH_UP:
    case DNGN_ENTER_HELL:
    case DNGN_EXIT_HELL:
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
    case DNGN_ENTER_SHOALS:
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
    case DNGN_RETURN_FROM_SHOALS:
        return (true);
    default:
        return (false);
    }
}

// Returns true if the given dungeon feature can be considered a gate.
bool is_gate(dungeon_feature_type gridc)
{
    switch (gridc)
    {
    case DNGN_ENTER_ABYSS:
    case DNGN_EXIT_ABYSS:
    case DNGN_ENTER_LABYRINTH:
    case DNGN_ENTER_PANDEMONIUM:
    case DNGN_EXIT_PANDEMONIUM:
    case DNGN_TRANSIT_PANDEMONIUM:
    case DNGN_ENTER_PORTAL_VAULT:
    case DNGN_EXIT_PORTAL_VAULT:
    case DNGN_ENTER_ZOT:
    case DNGN_RETURN_FROM_ZOT:
    case DNGN_ENTER_HELL:
    case DNGN_ENTER_DIS:
    case DNGN_ENTER_GEHENNA:
    case DNGN_ENTER_COCYTUS:
    case DNGN_ENTER_TARTARUS:
        return (true);
    default:
        return (false);
    }
}

// Prompts the user to stop explore if necessary for the given
// explore-stop condition, returns true if explore should be stopped.
bool prompt_stop_explore(int es_why)
{
    return (!(Options.explore_stop_prompt & es_why)
            || yesno("Stop exploring?", true, 'y', true, false));
}

#define ES_item   (Options.explore_stop & ES_ITEM)
#define ES_greedy (Options.explore_stop & ES_GREEDY_ITEM)
#define ES_glow   (Options.explore_stop & ES_GLOWING_ITEM)
#define ES_art    (Options.explore_stop & ES_ARTEFACT)
#define ES_rune   (Options.explore_stop & ES_RUNE)
#define ES_shop   (Options.explore_stop & ES_SHOP)
#define ES_stair  (Options.explore_stop & ES_STAIR)
#define ES_altar  (Options.explore_stop & ES_ALTAR)
#define ES_portal (Options.explore_stop & ES_PORTAL)

// Adds interesting stuff on (x, y) to explore_discoveries.
//
// NOTE: These are env.map coords, add +1 to get grid coords.
inline static void _check_interesting_square(int x, int y,
                                             explore_discoveries &ed)
{
    const coord_def pos(x, y);

    if (ES_item || ES_greedy || ES_glow || ES_art || ES_rune)
    {
        if (const monsters *mons = monster_at(pos))
        {
            if (mons_is_unknown_mimic(mons))
            {
                item_def item;
                get_mimic_item(mons, item);
                ed.found_item(pos, item);
            }
        }

        if (igrd(pos) != NON_ITEM)
            ed.found_item( pos, mitm[ igrd(pos) ] );
    }

    ed.found_feature( pos, grd(pos) );
}

static void _userdef_run_stoprunning_hook(void)
{
#ifdef CLUA_BINDINGS
    if (you.running)
        clua.callfn("ch_stop_running", "s", run_mode_name(you.running));
#endif
}

static void _userdef_run_startrunning_hook(void)
{
#ifdef CLUA_BINDINGS
    if (you.running)
        clua.callfn("ch_start_running", "s", run_mode_name(you.running));
#endif
}

bool is_resting()
{
    return you.running.is_rest();
}

static void _start_running()
{
    _userdef_run_startrunning_hook();

    if (you.running < 0)
        start_delay( DELAY_TRAVEL, 1 );
}

// Stops shift+running and all forms of travel.
void stop_running()
{
    you.running.stop();
}

static bool _is_valid_explore_target(const coord_def& where)
{
    // If an adjacent square is unmapped, it's valid.
    for ( adjacent_iterator ai(where); ai; ++ai )
        if (!is_terrain_seen(*ai))
            return (true);

    if (you.running == RMODE_EXPLORE_GREEDY)
    {
        LevelStashes *lev = StashTrack.find_current_level();
        return (lev && lev->needs_visit(where));
    }

    return (false);
}

enum explore_status_type
{
    EST_FULLY_EXPLORED    = 0,

    // Could not explore because of hostile terrain
    EST_PARTLY_EXPLORED   = 1,

    // Could not pick up interesting items because of hostile terrain. Note
    // that this and EST_PARTLY_EXPLORED are not mutually exclusive.
    EST_GREED_UNFULFILLED = 2
};

// Determines if the level is fully explored.
static int _find_explore_status(const travel_pathfind &tp)
{
    int explore_status = 0;

    const coord_def greed = tp.greedy_square();
    if (greed.x || greed.y)
        explore_status |= EST_GREED_UNFULFILLED;

    const coord_def unexplored = tp.unexplored_square();
    if (unexplored.x || unexplored.y)
        explore_status |= EST_PARTLY_EXPLORED;

    return (explore_status);
}

static int prev_travel_moves[2] = {-1, -1};
static int prev_travel_index    = 0;

static int anti_zigzag_dir = -1;

static void _reset_zigzag_info()
{
    prev_travel_moves[0] = -1;
    prev_travel_moves[1] = -1;
    prev_travel_index    =  0;
    anti_zigzag_dir      = -1;
}

static void _set_target_square(const coord_def &target)
{
    you.running.pos = target;
}

static void _explore_find_target_square()
{
    travel_pathfind tp;
    tp.set_floodseed(you.pos(), true);

    coord_def whereto =
        tp.pathfind( static_cast<run_mode_type>(you.running.runmode) );

    if (whereto.x || whereto.y)
    {
        // Make sure this is a square that is reachable, since we asked
        // travel_pathfind to give us even unreachable squares.
        if (travel_point_distance[whereto.x][whereto.y] <= 0)
            whereto.reset();
    }

    if (whereto.x || whereto.y)
    {
        // Anti-zigzag turned off, or found a greedy target so we
        // don't need anti-zigzaging.
        if (!Options.explore_improved || whereto != tp.unexplored_square())
        {
            _set_target_square(whereto);
            _reset_zigzag_info();
            return;
        }

        // If the two previous travel moves are perpendicular to each
        // other...
        if (prev_travel_moves[0] != -1
            && prev_travel_moves[1] != -1
            && (abs(prev_travel_moves[1] - prev_travel_moves[0]) % 4) == 2)
        {
            ASSERT(anti_zigzag_dir == -1);

            // Try moving along the line that bisects the right angle.
            if ((abs(prev_travel_moves[0] - prev_travel_moves[1]) == 6)
                && (prev_travel_moves[0] + prev_travel_moves[1] == 8))
            {
                anti_zigzag_dir = 0;
            }
            else
            {
                anti_zigzag_dir = std::min(prev_travel_moves[0],
                                           prev_travel_moves[1]) + 1;
            }
        }

        // anti_zigzag_dir might have just been set, or might have
        // persisted from the previous call to
        // _explore_find_target_square().
        if (anti_zigzag_dir != -1)
        {
            coord_def target = you.pos();
            coord_def delta  = Compass[anti_zigzag_dir];

            dungeon_feature_type feature;
            do
            {
                target += delta;
                feature = grd(target);
            }
            while (is_travelsafe_square(target)
                   && is_traversable(feature)
                   && feature_traverse_cost(feature) == 1);

            target -= delta;

            // Has moving along the straight line found an unexplored
            // square?
            if (!is_terrain_seen(target + delta) && target != you.pos()
                && target != whereto)
            {
                // Auto-explore is only zigzagging if the prefered
                // target (whereto) and the anti-zigzag target are
                // close together.
                if (grid_distance(target, whereto) <= 5
                    && distance(target, whereto) <= 34)
                {
                    _set_target_square(target);
                    return;
                }
            }
            anti_zigzag_dir = -1;
        }

        _set_target_square(whereto);
    }
    else
    {
        // No place to go? Report to the player.
        const int estatus = _find_explore_status(tp);

        if (!estatus)
        {
            mpr("Done exploring.");
            learned_something_new(TUT_DONE_EXPLORE);
        }
        else
        {
            std::vector<std::string> inacc;
            if (estatus & EST_GREED_UNFULFILLED)
                inacc.push_back("items");
            if (estatus & EST_PARTLY_EXPLORED)
                inacc.push_back("places");

            mprf("Partly explored, can't reach some %s.",
                 comma_separated_line(inacc.begin(),
                                      inacc.end()).c_str());
        }
        stop_running();
    }
}

void explore_pickup_event(int did_pickup, int tried_pickup)
{
    if (!did_pickup && !tried_pickup)
        return;

    if (!you.running.is_explore())
        return;

    if (did_pickup)
    {
        const int estop =
            (you.running == RMODE_EXPLORE_GREEDY) ? ES_GREEDY_PICKUP
                                                  : ES_PICKUP;

        if ((Options.explore_stop & estop) && prompt_stop_explore(estop))
        {
            stop_delay();
            _reset_zigzag_info();
        }
    }

    // Greedy explore has no good way to deal with an item that we can't
    // pick up, so the only thing to do is to stop.
    if (tried_pickup && you.running == RMODE_EXPLORE_GREEDY)
    {
        if (explore_stopped_pos == you.pos() && !Options.pickup_dropped)
        {
            const std::string prompt =
                make_stringf(
                    "Could not pick up %s here; shall I ignore %s?",
                    tried_pickup == 1? "an item" : "some items",
                    tried_pickup == 1? "it" : "them");

            // Make Escape => 'n' and stop run.
            explicit_keymap map;
            map[ESCAPE] = 'n';
            if (yesno(prompt.c_str(), true, 'y', true, false, false, &map))
            {
                mark_items_non_pickup_at(you.pos());
                // Don't stop explore.
                return;
            }
        }
        explore_stopped_pos = you.pos();
        stop_delay();
        _reset_zigzag_info();
    }
}

// Top-level travel control (called from input() in acr.cc).
//
// travel() is responsible for making the individual moves that constitute
// (interlevel) travel and explore and deciding when travel and explore
// end.
//
// Don't call travel() if you.running >= 0.
command_type travel()
{
    char holdx, holdy;
    char *move_x = &holdx;
    char *move_y = &holdy;
    holdx = holdy = 0;

    command_type result = CMD_NO_CMD;

    // Abort travel/explore if you're confused or a key was pressed.
    if (kbhit() || you.confused())
    {
        stop_running();
        return CMD_NO_CMD;
    }

    if (you.running.is_explore())
    {
        // Scan through the shadow map, compare it with the actual map, and if
        // there are any squares of the shadow map that have just been
        // discovered and contain an item, or have an interesting dungeon
        // feature, stop exploring.

        explore_discoveries discoveries;
        for (int y = 0; y < GYM; ++y)
            for (int x = 0; x < GXM; ++x)
            {
                if (!mapshadow[x][y].seen() && is_terrain_seen(x, y))
                    _check_interesting_square(x, y, discoveries);
            }

        if (discoveries.prompt_stop())
            stop_running();

        mapshadow = env.map;
    }

    if (you.running.is_explore())
    {
        // Exploring.
        if (grd(you.pos()) == DNGN_ENTER_SHOP
            && you.running == RMODE_EXPLORE_GREEDY)
        {
            LevelStashes *lev = StashTrack.find_current_level();
            if (lev && lev->shop_needs_visit(you.pos()))
            {
                you.running = 0;
                return (CMD_GO_UPSTAIRS);
            }
        }

        // Speed up explore by not doing a double-floodfill if we have
        // a valid target.
        if (!you.running.pos.x
            || you.running.pos == you.pos()
            || !_is_valid_explore_target(you.running.pos))
        {
            _explore_find_target_square();
        }
    }

    if (you.running == RMODE_INTERLEVEL && !you.running.pos.x)
    {
        // Interlevel travel. Since you.running.x is zero, we've either just
        // initiated travel, or we've just climbed or descended a staircase,
        // and we need to figure out where to travel to next.
        if (!_find_transtravel_square(level_target.p) || !you.running.pos.x)
            stop_running();
    }

    if (you.running < 0)
    {
        // Remember what run-mode we were in so that we can resume
        // explore/interlevel travel correctly.
        int runmode = you.running;

        // Get the next step to make. If the travel command can't find a route,
        // we turn off travel (find_travel_pos does that automatically).
        find_travel_pos(you.pos(), move_x, move_y);

        if (you.running < 0 && (*move_x || *move_y))
        {
            const int delta_to_dir[9] = {
                7,  0, 1,
                6, -1, 2,
                5,  4, 3
            };
            prev_travel_moves[prev_travel_index] =
                delta_to_dir[(*move_x + 1) + 3 * (*move_y + 1)];
            prev_travel_index = !prev_travel_index;
        }

        if ((*move_x || *move_y) && you.running == RMODE_EXPLORE_GREEDY)
        {
            // Greedy explore should cut off on reaching an item. We can't
            // check after reaching the item, because at that point the stash
            // tracker will have verified the stash and say "false" to
            // needs_visit.
            const coord_def newpos = you.pos() + coord_def(*move_x, *move_y);

            if (newpos == you.running.pos)
            {
                const LevelStashes *lev = StashTrack.find_current_level();
                if (lev && lev->needs_visit(newpos)
                    && !lev->shop_needs_visit(newpos))
                {
                    const int estop =
                        (you.running == RMODE_EXPLORE_GREEDY) ?
                            ES_GREEDY_PICKUP : ES_PICKUP;

                    if ((Options.explore_stop & estop)
                        && prompt_stop_explore(estop))
                    {
                        explore_stopped_pos = newpos;
                        stop_running();
                    }
                    return direction_to_command( *move_x, *move_y );
                }
            }
        }

        if (!*move_x && !*move_y)
        {
            // If we've reached the square we were traveling towards, travel
            // should stop if this is simple travel. If we're exploring, we
            // should continue doing so (explore has its own end condition
            // upstairs); if we're traveling between levels and we've reached
            // our travel target, we're on a staircase and should take it.
            if (you.pos() == you.running.pos)
            {
                if (runmode == RMODE_EXPLORE || runmode == RMODE_EXPLORE_GREEDY)
                    you.running = runmode;       // Turn explore back on

                // For interlevel travel, we'll want to take the stairs unless
                // the interlevel travel specified a destination square and
                // we've reached that destination square.
                else if (runmode == RMODE_INTERLEVEL
                         && (level_target.p.pos != you.pos()
                             || level_target.p.id != level_id::current()))
                {
                    if (last_stair.depth != -1
                        && last_stair == level_id::current())
                    {
                        // We're trying to take the same stairs again. Baaad.

                        // We don't directly call stop_running() because
                        // you.running is probably 0, and stop_running() won't
                        // notify Lua hooks if you.running == 0.
                        you.running = runmode;
                        stop_running();
                        return (CMD_NO_CMD);
                    }

                    // Check for entrance-only thang. If we've reached the
                    // entrance, kill travel.
                    if (level_target.entrance_only)
                    {
                        LevelInfo &li =
                            travel_cache.get_level_info(level_id::current());

                        const stair_info *si = li.get_stair(you.pos());
                        if (si && si->destination.id == level_target.p.id)
                        {
                            you.running = runmode;
                            stop_running();
                            return (CMD_NO_CMD);
                        }
                    }

                    you.running = RMODE_INTERLEVEL;
                    result = _trans_negotiate_stairs();

                    // If, for some reason, we fail to use the stairs, we
                    // need to make sure we don't go into an infinite loop
                    // trying to take it again and again. We'll check
                    // last_stair before attempting to take stairs again.
                    last_stair = level_id::current();

                    // This is important, else we'll probably stop traveling
                    // the moment we clear the stairs. That's because the
                    // (running.x, running.y) destination will no longer be
                    // valid on the new level. Setting running.x to zero forces
                    // us to recalculate our travel target next turn (see
                    // previous if block).
                    you.running.pos.reset();
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
        viewwindow(true, false);

    if (!you.running)
        return CMD_NO_CMD;

    if (result != CMD_NO_CMD)
        return result;

    return direction_to_command( *move_x, *move_y );
}

command_type direction_to_command( char x, char y )
{
    if ( x == -1 && y == -1 ) return CMD_MOVE_UP_LEFT;
    if ( x == -1 && y ==  0 ) return CMD_MOVE_LEFT;
    if ( x == -1 && y ==  1 ) return CMD_MOVE_DOWN_LEFT;
    if ( x ==  0 && y == -1 ) return CMD_MOVE_UP;
    if ( x ==  0 && y ==  0 )
        return (you.running == RMODE_EXPLORE_GREEDY ? CMD_INSPECT_FLOOR
                                                    : CMD_NO_CMD);
    if ( x ==  0 && y ==  1 ) return CMD_MOVE_DOWN;
    if ( x ==  1 && y == -1 ) return CMD_MOVE_UP_RIGHT;
    if ( x ==  1 && y ==  0 ) return CMD_MOVE_RIGHT;
    if ( x ==  1 && y ==  1 ) return CMD_MOVE_DOWN_RIGHT;

    ASSERT(0);
    return CMD_NO_CMD;
}

static void _fill_exclude_radius(const travel_exclude &exc)
{
    const int radius = exc.radius;
    const coord_def &c = exc.pos;
    for (int y = c.y - radius; y <= c.y + radius; ++y)
        for (int x = c.x - radius; x <= c.x + radius; ++x)
        {
            if (!map_bounds(x, y) || !is_terrain_known(x, y)
                || travel_point_distance[x][y])
            {
                continue;
            }
            const coord_def p(x, y);

            if (is_exclude_root(p))
                travel_point_distance[x][y] = PD_EXCLUDED;
            else if (is_excluded(p))
                travel_point_distance[x][y] = PD_EXCLUDED_RADIUS;
        }
}

/////////////////////////////////////////////////////////////////////////////
// travel_pathfind

FixedVector<coord_def, GXM * GYM> travel_pathfind::circumference[2];

// already defined in header
// const int travel_pathfind::UNFOUND_DIST;
// const int travel_pathfind::INFINITE_DIST;

travel_pathfind::travel_pathfind()
    : runmode(RMODE_NOT_RUNNING), start(), dest(), next_travel_move(),
      floodout(false), double_flood(false), ignore_hostile(false),
      annotate_map(false), ls(NULL), need_for_greed(false),
      unexplored_place(), greedy_place(), unexplored_dist(0),
      greedy_dist(0), refdist(NULL), reseed_points(),
      features(NULL), point_distance(travel_point_distance),
      points(0), next_iter_points(0), traveled_distance(0),
      circ_index(0)
{
}

travel_pathfind::~travel_pathfind()
{
}

static bool _is_greed_inducing_square(const LevelStashes *ls,
                                      const coord_def &c)
{
    if (ls && ls->needs_visit(c.x, c.y))
        return (true);

    if (const monsters *mons = monster_at(c))
    {
        if (mons_is_unknown_mimic(mons) && mons_was_seen(mons))
        {
            item_def mimic_item;
            get_mimic_item(mons, mimic_item);
            if (item_needs_autopickup(mimic_item))
                return (true);
        }
    }
    return (false);
}

bool travel_pathfind::is_greed_inducing_square(const coord_def &c) const
{
    return _is_greed_inducing_square(ls, c);
}

void travel_pathfind::set_src_dst(const coord_def &src, const coord_def &dst)
{
    // Yes, this is backwards - for travel, we always start from the destination
    // and search outwards for the starting position.
    start = dst;
    dest  = src;

    floodout = double_flood = false;
}

void travel_pathfind::set_floodseed(const coord_def &seed, bool dblflood)
{
    start = seed;
    dest.reset();

    floodout = true;
    double_flood = dblflood;
}

void travel_pathfind::set_annotate_map(bool annotate)
{
    annotate_map = annotate;
}

void travel_pathfind::set_distance_grid(travel_distance_grid_t grid)
{
    point_distance = grid;
}

void travel_pathfind::set_feature_vector(std::vector<coord_def> *feats)
{
    features = feats;

    if (features)
    {
        double_flood = true;
        annotate_map = true;
    }
}

const coord_def travel_pathfind::travel_move() const
{
    return (next_travel_move);
}

const coord_def travel_pathfind::explore_target() const
{
    if (unexplored_dist != UNFOUND_DIST && greedy_dist != UNFOUND_DIST)
    {
        return (unexplored_dist < greedy_dist ? unexplored_place
                                              : greedy_place);
    }
    else if (unexplored_dist != UNFOUND_DIST)
        return (unexplored_place);
    else if (greedy_dist != UNFOUND_DIST)
        return (greedy_place);

    return coord_def(0, 0);
}

const coord_def travel_pathfind::greedy_square() const
{
    return (greedy_place);
}

const coord_def travel_pathfind::unexplored_square() const
{
    return (unexplored_place);
}

// The travel algorithm is based on the NetHack travel code written by Warwick
// Allison - used with his permission.
coord_def travel_pathfind::pathfind(run_mode_type rmode)
{
    if (rmode == RMODE_INTERLEVEL)
        rmode = RMODE_TRAVEL;

    runmode = rmode;

    // Check whether species or levitation permits travel through terrain such
    // as deep water.
    init_travel_terrain_check();

    need_for_greed = (rmode == RMODE_EXPLORE_GREEDY && can_autopickup());

    if (!ls && (annotate_map || need_for_greed))
        ls = StashTrack.find_current_level();

    next_travel_move.reset();

    // For greedy explore, keep track of the closest unexplored territory and
    // the closest greedy square. Exploring to the nearest (unexplored / greedy)
    // square is easier, but it produces unintuitive explore behaviour where
    // grabbing items is not favoured over simple exploring.
    //
    // Greedy explore instead uses the explore_item_greed option to weight
    // greedy explore towards grabbing items over exploring. An
    // explore_item_greed set to 10, for instance, forces explore to prefer
    // items that are less than 10 squares farther away from the player than the
    // nearest unmapped square. Negative explore_item_greed values force greedy
    // explore to favour unexplored territory over picking up items. For the
    // most natural greedy explore behaviour, explore_item_greed should be set
    // to 10 or more.
    //
    unexplored_place = greedy_place = coord_def(0, 0);
    unexplored_dist  = greedy_dist  = UNFOUND_DIST;

    refdist = (Options.explore_item_greed > 0) ? &unexplored_dist
                                               : &greedy_dist;

    // Abort run if we're trying to go someplace evil. Travel to traps is
    // specifically allowed here if the player insists on it.
    if (!floodout
        && !is_travelsafe_square(start, false)
        && !is_trap(start))          // player likes pain
    {
        return coord_def();
    }

    // Nothing to do?
    if (!floodout && start == dest)
        return (start);

    // How many points are we currently considering? We start off with just one
    // point, and spread outwards like a flood-filler.
    points = 1;

    // How many points we'll consider next iteration.
    next_iter_points = 0;

    // How far we've traveled from (start_x, start_y), in moves (a diagonal move
    // is no longer than an orthogonal move).
    traveled_distance = 1;

    // Which index of the circumference array are we currently looking at?
    circ_index = 0;

    ignore_hostile = false;

    // For each round, circumference will store all points that were discovered
    // in the previous round of a given distance. Because we check all grids of
    // a certain distance from the starting point in one round, and move
    // outwards in concentric circles, this is an implementation of Dijkstra.
    // We use an array of size 2, so we can comfortably switch between the list
    // of points to be investigated this round and the slowly growing list of
    // points to be inspected next round. Once we've finished with the current
    // round, i.e. there are no more points to be looked at in the current
    // array, we switch circ_index over to !circ_index (between 0 and 1), so
    // the "next round" becomes the current one, and the old points can be
    // overwritten with newer ones. Since we count the number of points for
    // next round in next_iter_points, we don't even need to reset the array.
    circumference[circ_index][0] = start;

    // Zap out previous distances array
    // point_distance will hold the distance of all points from the starting
    // point, i.e. the distance travelled to get there.
    memset(point_distance, 0, sizeof(travel_distance_grid_t));

    for ( ; points > 0; ++traveled_distance, circ_index = !circ_index,
                        points = next_iter_points, next_iter_points = 0)
    {
        for (int i = 0; i < points; ++i)
        {
            // Look at all neighbours of the current grid.
            // path_examine_point() returns true if the target is reached
            // and marked as such.
            if (path_examine_point(circumference[circ_index][i]))
            {
                return (runmode == RMODE_TRAVEL ? travel_move()
                                                : explore_target());
            }
        }

        // If there are no more points to look at, we're done, but we did
        // not find a path to our target.
        if (next_iter_points == 0)
        {
            // Don't reseed unless we've found no target for explore, OR
            // we're doing map annotation or feature tracking.
            if ((runmode == RMODE_EXPLORE || runmode == RMODE_EXPLORE_GREEDY)
                && double_flood
                && !ignore_hostile
                && !features
                && !annotate_map
                && (unexplored_dist != UNFOUND_DIST
                    || greedy_dist != UNFOUND_DIST))
            {
                break;
            }

            if (double_flood
                && !ignore_hostile
                && !reseed_points.empty())
            {
                // Reseed here
                for (unsigned i = 0, size = reseed_points.size(); i < size; ++i)
                    circumference[!circ_index][i] = reseed_points[i];

                next_iter_points = reseed_points.size();
                ignore_hostile = true;
            }
        }
    } // for ( ; points > 0 ...

    if (features && floodout)
    {
        for (int i = 0, size = curr_excludes.size(); i < size; ++i)
        {
            const travel_exclude &exc = curr_excludes[i];
            // An exclude - wherever it is - is always a feature.
            if (std::find(features->begin(), features->end(), exc.pos)
                    == features->end())
            {
                features->push_back(exc.pos);
            }

            _fill_exclude_radius(exc);
        }
    }

    return (rmode == RMODE_TRAVEL ? travel_move()
                                  : explore_target());
}

bool travel_pathfind::square_slows_movement(const coord_def &c)
{
    // c is a known (explored) location - we never put unknown points in the
    // circumference vector, so we don't need to examine the map array, just the
    // grid array.
    const dungeon_feature_type feature = grd(c);

    // If this is a feature that'll take time to travel past, we simulate that
    // extra turn by taking this feature next turn, thereby artificially
    // increasing traveled_distance.
    //
    // Walking through shallow water and opening closed doors is considered to
    // have the cost of two normal moves for travel purposes.
    const int feat_cost = feature_traverse_cost(feature);
    if (feat_cost > 1
        && point_distance[c.x][c.y] > traveled_distance - feat_cost)
    {
        circumference[!circ_index][next_iter_points++] = c;
        return (true);
    }

    return (false);
}

void travel_pathfind::check_square_greed(const coord_def &c)
{
    if (greedy_dist == UNFOUND_DIST
        && is_greed_inducing_square(c)
        && is_travelsafe_square(c, ignore_hostile))
    {
        greedy_place = c;
        greedy_dist  = traveled_distance;
    }
}

bool travel_pathfind::path_flood(const coord_def &c, const coord_def &dc)
{
    if (!in_bounds(dc))
        return (false);

    if (floodout
        && (runmode == RMODE_EXPLORE || runmode == RMODE_EXPLORE_GREEDY))
    {
        if (!is_terrain_seen(dc))
        {
            if (!need_for_greed)
            {
                // Found explore target!
                unexplored_place = c;
                unexplored_dist  = traveled_distance;
                return (true);
            }

            if (unexplored_dist == UNFOUND_DIST)
            {
                unexplored_place = c;
                unexplored_dist  =
                    traveled_distance + Options.explore_item_greed;
            }
        }

        // Short-circuit if we can. If traveled_distance (the current
        // distance from the center of the floodfill) is greater
        // than the adjusted distance to the nearest greedy explore
        // target, we have a target. Note the adjusted distance is
        // the distance with explore_item_greed applied (if
        // explore_item_greed > 0, it is added to the distance to
        // unexplored terrain, if explore_item_greed < 0, it is
        // added to the distance to interesting items.
        //
        // We never short-circuit if ignore_hostile is true. This is
        // important so we don't need to do multiple floods to work out
        // whether explore is complete.
        if (need_for_greed
            && !ignore_hostile
            && *refdist != UNFOUND_DIST
            && traveled_distance > *refdist)
        {
            if (Options.explore_item_greed > 0)
                greedy_dist = INFINITE_DIST;
            else
                unexplored_dist = INFINITE_DIST;
        }

        // greedy_dist is only ever set in greedy-explore so this check
        // implies greedy-explore.
        if (unexplored_dist != UNFOUND_DIST && greedy_dist != UNFOUND_DIST)
            return (true);
    }

    if (dc == dest)
    {
        // Hallelujah, we're home!
        if (_is_safe_move(c))
            next_travel_move = c;

        return (true);
    }
    else if (!is_travelsafe_square(dc, ignore_hostile))
    {
        // This point is not okay to travel on, but if this is a
        // trap, we'll want to put it on the feature vector anyway.
        if (_is_reseedable(dc)
            && !point_distance[dc.x][dc.y]
            && dc != start)
        {
            if (features && (is_trap(dc) || is_exclude_root(dc)))
                features->push_back(dc);

            if (double_flood)
                reseed_points.push_back(dc);

            // Appropriate mystic number. Nobody else should check
            // this number, since this square is unsafe for travel.
            point_distance[dc.x][dc.y] =
                is_exclude_root(dc)   ? PD_EXCLUDED :
                is_excluded(dc)       ? PD_EXCLUDED_RADIUS
                                      : PD_TRAP;
        }
        return (false);
    }

    if (!point_distance[dc.x][dc.y])
    {
        // This point is going to be on the agenda for the next
        // iteration
        circumference[!circ_index][next_iter_points++] = dc;
        point_distance[dc.x][dc.y] = traveled_distance;

        // Negative distances here so that show_map can colour
        // the map differently for these squares.
        if (ignore_hostile)
        {
            point_distance[dc.x][dc.y] = -point_distance[dc.x][dc.y];
            if (is_exclude_root(dc))
                point_distance[dc.x][dc.y] = PD_EXCLUDED;
            else if (is_excluded(dc))
                point_distance[dc.x][dc.y] = PD_EXCLUDED_RADIUS;
        }

        if (features && !ignore_hostile)
        {
            dungeon_feature_type feature = grd(dc);

            if (dc != start
                && (feature != DNGN_FLOOR
                       && !grid_is_water(feature)
                       && feature != DNGN_LAVA
                    || is_waypoint(dc)
                    || is_stash(ls, dc.x, dc.y)))
            {
                features->push_back(dc);
            }
        }

        if (features && dc != start && is_exclude_root(dc))
            features->push_back(dc);
    }

    return (false);
}

void travel_pathfind::good_square(const coord_def &c)
{
    if (!point_distance[c.x][c.y])
    {
        // This point is going to be on the agenda for the next iteration.
        circumference[!circ_index][next_iter_points++] = c;
        point_distance[c.x][c.y] = traveled_distance;
    }
}

bool travel_pathfind::point_traverse_delay(const coord_def &c)
{
    if (square_slows_movement(c))
        return (true);

    // Greedy explore check should happen on (x,y), not (dx,dy) as for
    // regular explore.
    if (need_for_greed)
        check_square_greed(c);

    return (false);
}

// Checks all neighbours of c, adds them to next round's list of points
// - happens in path_flood() - and returns true if one of them turns out
// to be the target; otherwise, false.
bool travel_pathfind::path_examine_point(const coord_def &c)
{
    if (point_traverse_delay(c))
        return (false);

    // For each point, we look at all surrounding points. Take them orthogonals
    // first so that the travel path doesn't zigzag all over the map. Note the
    // (dir = 1) is intentional assignment.
    for (int dir = 0; dir < 8; (dir += 2) == 8 && (dir = 1))
        if (path_flood(c, c + Compass[dir]))
            return (true);

    return (false);
}

/////////////////////////////////////////////////////////////////////////////

void find_travel_pos(const coord_def& youpos,
                     char *move_x, char *move_y,
                     std::vector<coord_def>* features)
{
    travel_pathfind tp;

    if (move_x && move_y)
        tp.set_src_dst(youpos, you.running.pos);
    else
        tp.set_floodseed(youpos);

    tp.set_feature_vector(features);

    run_mode_type rmode = (move_x && move_y) ? RMODE_TRAVEL
                                             : RMODE_NOT_RUNNING;

    const coord_def dest = tp.pathfind( rmode );

    if (dest.origin())
    {
        if (move_x && move_y)
            you.running = RMODE_NOT_RUNNING;
    }
    else if (move_x && move_y)
    {
        *move_x = dest.x - youpos.x;
        *move_y = dest.y - youpos.y;
    }
}

// Given a branch id, returns the parent branch. If the branch id is not found,
// returns BRANCH_MAIN_DUNGEON.
branch_type find_parent_branch(branch_type br)
{
    return branches[br].parent_branch;
}

extern std::map<branch_type, level_id> stair_level;

void find_parent_branch(branch_type br, int depth,
                        branch_type *pb, int *pd)
{
    *pb = find_parent_branch(br);   // Check depth before using *pb.
    if (stair_level.find(br) == stair_level.end())
        *pd = 0;
    else
        *pd = stair_level[br].depth;
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
               branch_type branch, int subdepth)
{
    if (subdepth < 1 || subdepth > MAX_LEVELS)
        return;

    level_id lid( branch, subdepth );
    vec.push_back(lid);

    if (branch != BRANCH_MAIN_DUNGEON)
    {
        branch_type pb;
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
    cx->branch = BRANCH_MAIN_DUNGEON;
    cx->depth  = -1;

    int us = int(cur.size()) - 1, them = int(targ.size()) - 1;

    for ( ; us >= 0 && them >= 0; us--, them--)
        if (cur[us].branch != targ[them].branch)
            break;

    us++, them++;

    if (us < (int) cur.size() && them < (int) targ.size() && us >= 0
        && them >= 0)
    {
        *cx = targ[them];
    }
}

// Returns the number of stairs the player would need to take to go from
// the 'first' level to the 'second' level. If there's no obvious route between
// 'first' and 'second', returns -1. If first == second, returns 0.
int level_distance(level_id first, level_id second)
{
    if (first == second
        || (first.level_type != LEVEL_DUNGEON
            && first.level_type == second.level_type))
    {
        return 0;
    }

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

std::string get_trans_travel_dest(const travel_target &target,
                                  bool skip_branch = false,
                                  bool skip_coord = false)
{
    const int branch_id = target.p.id.branch;
    const char *branch = branches[branch_id].abbrevname;

    if (!branch)
        return ("");

    std::ostringstream dest;

    if (!skip_branch)
        dest << branch;
    if (branches[branch_id].depth != 1)
    {
        if (!skip_branch)
            dest << ":";
        dest << target.p.id.depth;
    }
    if (target.p.pos.x != -1 && !skip_coord)
        dest << " @ (x,y)";
    else if (target.entrance_only)
        dest << " (entrance)";

    return (dest.str());
}

// Returns the level on the given branch that's closest to the player's
// current location.
static int _get_nearest_level_depth(unsigned char branch)
{
    int depth = 1;

    // Hell needs special treatment, because we can't walk up
    // Hell and its branches to the main dungeon.
    if (branch == BRANCH_MAIN_DUNGEON
        && (player_in_branch(BRANCH_VESTIBULE_OF_HELL)
            || player_in_branch(BRANCH_COCYTUS)
            || player_in_branch(BRANCH_TARTARUS)
            || player_in_branch(BRANCH_DIS)
            || player_in_branch(BRANCH_GEHENNA)))
    {
        return you.hell_exit + 1;
    }

    level_id id = level_id::current();
    do
    {
        find_parent_branch(id.branch, id.depth,
                           &id.branch, &id.depth);
        if (id.depth && id.branch == branch)
        {
            depth = id.depth;
            break;
        }
    }
    while (id.depth);

    return depth;
}

// Returns true if the player character knows of the existence of the given
// branch (which would make the branch a valid target for interlevel travel).
static bool _is_known_branch_id(int branch)
{
    // The main dungeon is always known.
    if (branch == BRANCH_MAIN_DUNGEON)
        return (true);

    // If we're in the branch, it darn well is known.
    if (you.where_are_you == branch)
        return (true);

    // The Vestibule is special: there are no stairs to it, just a
    // portal.
    if (branch == BRANCH_VESTIBULE_OF_HELL)
        return overmap_knows_portal(DNGN_ENTER_HELL);

    // If the overmap knows the stairs to this branch, we know the branch.
    return (stair_level.find(static_cast<branch_type>(branch))
            != stair_level.end());
}

static bool _is_known_branch(const Branch &br)
{
    return (_is_known_branch_id(br.id));
}

// Returns a list of the branches that the player knows the location of the
// stairs to, in the same order as overmap.cc lists them.
static std::vector<branch_type> _get_branches(bool (*selector)(const Branch &))
{
    std::vector<branch_type> result;

    for (int i = 0; i < NUM_BRANCHES; ++i)
        if (selector(branches[i]))
            result.push_back(branches[i].id);

    return result;
}

static bool _is_valid_branch(const Branch &br)
{
    return (br.shortname != NULL && br.depth != -1);
}

static int _prompt_travel_branch(int prompt_flags, bool* to_entrance)
{
    int branch = BRANCH_MAIN_DUNGEON;     // Default
    std::vector<branch_type> br =
        _get_branches(
            (prompt_flags & TPF_SHOW_ALL_BRANCHES) ? _is_valid_branch
                                                   : _is_known_branch );

    // Don't kill the prompt even if the only branch we know is the main dungeon
    // This keeps things consistent for the player.
    if (br.size() < 1)
        return branch;

    const bool allow_waypoints = (prompt_flags & TPF_ALLOW_WAYPOINTS);
    const bool allow_updown    = (prompt_flags & TPF_ALLOW_UPDOWN);
    const bool remember_targ   = (prompt_flags & TPF_REMEMBER_TARGET);

    bool waypoint_list = false;
    const int waycount = allow_waypoints? travel_cache.get_waypoint_count() : 0;

    level_id curr = level_id::current();
    while (true)
    {
        mesclr(true);

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
                line += make_stringf("(%c) %-14s ",
                                     branches[br[i]].travel_shortcut,
                                     branches[br[i]].shortname);
            }
            if (line.length())
                mpr(line.c_str());
        }

        std::string shortcuts = "(";
        {
            std::vector<std::string> segs;
            if (allow_waypoints)
            {
                if (waypoint_list)
                    segs.push_back("* - list branches");
                else if (waycount)
                    segs.push_back("* - list waypoints");
            }

            if (!trans_travel_dest.empty() && remember_targ)
            {
                segs.push_back(
                    make_stringf("Enter - %s", trans_travel_dest.c_str()) );
            }

            segs.push_back("? - help");

            shortcuts += comma_separated_line(segs.begin(), segs.end(),
                                              ", ", ", ");
            shortcuts += ") ";
        }
        mprf(MSGCH_PROMPT, "%s? %s",
             *to_entrance ? "Entrance to where" : "Where to",
             shortcuts.c_str());

        int keyin = get_ch();
        switch (keyin)
        {
        case ESCAPE:
            return (ID_CANCEL);
        case '?':
            show_interlevel_travel_branch_help();
            redraw_screen();
            break;
        case '\n': case '\r':
            return (ID_REPEAT);
        case '<':
            return (allow_updown? ID_UP : ID_CANCEL);
        case '>':
            return (allow_updown? ID_DOWN : ID_CANCEL);
        case CONTROL('P'):
            return find_parent_branch(curr.branch);
        case '.':
            return (curr.branch);
        case '*':
            if (waypoint_list || waycount)
                waypoint_list = !waypoint_list;
            break;
        case '^':
            if (*to_entrance)
                return (ID_CANCEL);
            else
            {
                *to_entrance = true;
                return _prompt_travel_branch(prompt_flags, to_entrance);
            }
        default:
            // Is this a branch hotkey?
            for (int i = 0, count = br.size(); i < count; ++i)
            {
                if (toupper(keyin) == branches[br[i]].travel_shortcut)
                {
#ifdef WIZARD
                    Branch     &target = branches[br[i]];
                    std::string msg;

                    if (target.startdepth == -1
                        && (i == BRANCH_SWAMP || i == BRANCH_SHOALS ))
                    {
                        msg += "Branch not generated this game.  ";
                    }

                    if (target.entry_stairs == NUM_FEATURES
                        && br[i] != BRANCH_MAIN_DUNGEON)
                    {
                        msg += "Branch has no entry stairs.  ";
                    }

                    if (!msg.empty())
                    {
                        msg += "Go there anyway?";
                        if (!yesno(msg.c_str()))
                            return (ID_CANCEL);
                    }
#endif
                    return (br[i]);
                }
            }

            // Possibly a waypoint number?
            if (allow_waypoints && keyin >= '0' && keyin <= '9')
                return (-1 - (keyin - '0'));

            return (ID_CANCEL);
        }
    }
}

static bool _is_easy_exiting_branch(int branch)
{
    return branches[branch].any_upstair_exits;
}

static level_id _find_up_level(level_id curr, bool up_branch = false)
{
    --curr.depth;

    if (up_branch || _is_easy_exiting_branch(curr.branch))
        curr.depth = 0;

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
        return level_id();
    }

    return (curr);
}

static level_id _find_up_level()
{
    return (_find_up_level(level_id::current()));
}

static level_id _find_down_level(level_id curr)
{
    if (curr.depth < branches[curr.branch].depth)
        ++curr.depth;
    return (curr);
}

static level_id _find_deepest_explored(level_id curr)
{
    for (int i = branches[curr.branch].depth; i > 0; --i)
    {
        const level_id lid(curr.branch, i);
        const LevelInfo *linf = travel_cache.find_level_info(lid);
        if (linf && !linf->empty())
            return (lid);
    }
    return (curr);
}

static level_id _find_down_level()
{
    return (_find_down_level(level_id::current()));
}

static int _travel_depth_keyfilter(int &c)
{
    switch (c)
    {
    case '<': case '>': case '?': case '$': case '^':
        return (-1);
    case '-':
    case CONTROL('P'): case 'p':
        c = '-';  // Make uniform.
        return (-1);
    default:
        return (1);
    }
}

static travel_target _parse_travel_target( std::string s, travel_target &targ )
{
    trim_string(s);

    const std::string ekey("(entrance)");
    std::string::size_type epos = s.find(ekey);

    if (!s.empty())
        targ.entrance_only = (epos != std::string::npos);

    if (targ.entrance_only && !s.empty())
        s = trimmed_string(s.substr(0, epos) + s.substr(epos + ekey.length()));

    if (!s.empty())
        targ.p.id.depth = atoi(s.c_str());

    if (!targ.p.id.depth && !s.empty() && s[0] == '0')
    {
        targ.p.id.depth = 1;
        targ.entrance_only = true;
    }

    return (targ);
}

static void _travel_depth_munge(int munge_method, const std::string &s,
                                travel_target &targ)
{
    _parse_travel_target(s, targ);
    level_id lid(targ.p.id);
    switch (munge_method)
    {
    case '?':
        show_interlevel_travel_depth_help();
        redraw_screen();
        return;
    case '<':
        lid = _find_up_level(lid);
        break;
    case '>':
        lid = _find_down_level(lid);
        break;
    case '-':
        lid = _find_up_level(lid, true);
        break;
    case '$':
        lid = _find_deepest_explored(lid);
        break;
    case '^':
        targ.entrance_only = !targ.entrance_only;
        break;
    }
    targ.p.id = lid;
    if (targ.p.id.depth < 1)
        targ.p.id.depth = 1;
}

static travel_target _prompt_travel_depth(const level_id &id,
                                          bool already_entrance)
{
    travel_target target = travel_target(level_pos(id), already_entrance);

    // Handle one-level branches by not prompting.
    if (single_level_branch(target.p.id.branch))
        return travel_target(level_id(target.p.id.branch, 1), already_entrance);

    target.p.id.depth = _get_nearest_level_depth(target.p.id.branch);
    while (true)
    {
        mesclr(true);
        mprf(MSGCH_PROMPT, "What level of %s? "
             "(default %s, ? - help) ",
             branches[target.p.id.branch].longname,
             get_trans_travel_dest(target, true).c_str());

        char buf[100];
        const int response =
            cancelable_get_line( buf, sizeof buf, get_number_of_cols(),
                                 NULL, _travel_depth_keyfilter );

        if (!response)
            return _parse_travel_target(buf, target);

        if (response == ESCAPE)
            return travel_target(level_id(BRANCH_MAIN_DUNGEON, 0));

        _travel_depth_munge(response, buf, target);
    }
}

travel_target prompt_translevel_target(int prompt_flags)
{
    travel_target target;
    bool to_entrance = false;
    int branch = _prompt_travel_branch(prompt_flags, &to_entrance);
    const bool remember_targ = (prompt_flags & TPF_REMEMBER_TARGET);

    if (branch == ID_CANCEL)
        return (target);

    // If user chose to repeat last travel, return that.
    if (branch == ID_REPEAT)
        return (level_target);

    if (branch == ID_UP)
    {
        target.p = _find_up_level();
        if (target.p.id.depth > 0 && remember_targ)
            trans_travel_dest = get_trans_travel_dest(target);
        return (target);
    }

    if (branch == ID_DOWN)
    {
        target.p = _find_down_level();
        if (target.p.id.depth > 0 && remember_targ)
            trans_travel_dest = get_trans_travel_dest(target);
        return (target);
    }

    if (branch < 0)
    {
        travel_cache.travel_to_waypoint(-branch - 1);
        return target;
    }

    target.p.id.branch = static_cast<branch_type>(branch);

    // User's chosen a branch, so now we ask for a level.
    target = _prompt_travel_depth(target.p.id, to_entrance);

    if (target.p.id.depth < 1
        || target.p.id.depth > branches[target.p.id.branch].depth)
    {
        target.p.id.depth = -1;
    }

    if (target.p.id.depth > -1 && remember_targ)
        trans_travel_dest = get_trans_travel_dest(target);

    return target;
}

void start_translevel_travel(const travel_target &pos)
{
    if (!i_feel_safe(true, true))
        return;

    if (!can_travel_to(pos.p.id))
    {
        if (!can_travel_interlevel())
            mpr("Sorry, you can't auto-travel out of here.");
        else
            mpr("Sorry, I don't know how to get there.");
        return;
    }

    if (!can_travel_interlevel())
    {
        start_travel(pos.p.pos);
        return;
    }

    level_target = pos;

    if (pos.p.id != level_id::current())
    {
        if (!_loadlev_populate_stair_distances(pos.p))
        {
            mpr("Level memory is imperfect, aborting.");
            return ;
        }
    }
    else
        _populate_stair_distances(pos.p);

    trans_travel_dest = get_trans_travel_dest(level_target);
    start_translevel_travel(false);
}

void start_translevel_travel(bool prompt_for_destination)
{
    if (!i_feel_safe(true, true))
        return;

    // Update information for this level. We need it even for the prompts, so
    // we can't wait to confirm that the user chose to initiate travel.
    travel_cache.get_level_info(level_id::current()).update();

    if (prompt_for_destination)
    {
        // prompt_translevel_target may actually initiate travel directly if
        // the user chose a waypoint instead of a branch + depth. As far as
        // we're concerned, if the target depth is unset, we need to take no
        // further action.
        travel_target target = prompt_translevel_target();
        if (target.p.id.depth <= 0)
            return;

        level_target = target;
    }

    if (level_id::current() == level_target.p.id
        && (level_target.p.pos.x == -1 || level_target.p.pos == you.pos()))
    {
        mpr("You're already here!");
        return ;
    }

    if (level_target.p.id.depth > 0)
    {
        // Forget interrupted butchering.
        you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = 0;

        you.running = RMODE_INTERLEVEL;
        you.running.pos.reset();
        last_stair.depth = -1;

        _Route_Warning.new_dest(level_target);

        _Src_Level = level_id::current();
        _Src_Dest_Level_Delta = level_distance(_Src_Level,
                                               level_target.p.id);

        _start_running();
    }
}

command_type _trans_negotiate_stairs()
{
    return grid_stair_direction(grd(you.pos()));
}

static int _target_distance_from(const coord_def &pos)
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
 * This function relies on the travel_point_distance array being correctly
 * populated with a floodout call to find_travel_pos starting from the player's
 * location.
 */
static int _find_transtravel_stair( const level_id &cur,
                                    const level_pos &target,
                                    int distance,
                                    // This is actually the current position
                                    // on cur, not necessarily a stair.
                                    const coord_def &stair,
                                    level_id &closest_level,
                                    int &best_level_distance,
                                    coord_def &best_stair,
                                    const bool target_has_excludes )
{
    int local_distance = -1;
    level_id player_level = level_id::current();

    LevelInfo &li = travel_cache.get_level_info(cur);

    // Have we reached the target level?
    if (cur == target.id)
    {
        // Are we in an exclude? If so, bail out.
        if (_is_excluded( stair, li.get_excludes() ))
            return (-1);

        // If there's no target position on the target level, or we're on the
        // target, we're home.
        if (target.pos.x == -1 || target.pos == stair)
            return distance;

        // If there *is* a target position, we need to work out our distance
        // from it.
        int deltadist = _target_distance_from(stair);

        if (deltadist == -1 && cur == player_level)
        {
            // Okay, we don't seem to have a distance available to us, which
            // means we're either (a) not standing on stairs or (b) whoever
            // initiated interlevel travel didn't call
            // _populate_stair_distances.  Assuming we're not on stairs, that
            // situation can arise only if interlevel travel has been triggered
            // for a location on the same level. If that's the case, we can get
            // the distance off the travel_point_distance matrix.
            deltadist = travel_point_distance[target.pos.x][target.pos.y];
            if (!deltadist && stair != target.pos)
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
            if (player_level == target.id && stair == you.pos())
                best_stair = target.pos;

            // The local_distance is already set, but there may actually be
            // stairs we can take that'll get us to the target faster than the
            // direct route, so we also try the stairs.
        }
    }

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

        // Skip placeholders, since there are no real stairs there.
        if (!si.can_travel())
            continue;

        int deltadist = li.distance_between(this_stair, &si);
        if (!this_stair)
        {
            deltadist = travel_point_distance[si.position.x][si.position.y];
            if (!deltadist && you.pos() != si.position)
                deltadist = -1;
        }

        // deltadist == 0 is legal (if this_stair is NULL), since the player
        // may be standing on the stairs. If two stairs are disconnected,
        // deltadist has to be negative.
        if (deltadist < 0)
            continue;

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

            // Never use escape hatches as the last leg of the trip, since
            // that will leave the player unable to retrace their path.
            // This does not apply if we have a destination with a specific
            // position on the target level travel wants to get to.
            if (grid_is_escape_hatch(si.grid)
                && target.pos.x == -1
                && dest.id == target.id)
            {
                continue;
            }

            // We can only short-circuit the stair-following process if we
            // have no exact target location. If there *is* an exact target
            // location, we can't follow stairs for which we have incomplete
            // information.
            //
            // We can also not use incomplete stair information if there are
            // excludes on the target level.
            if (target.pos.x == -1
                && dest.id == target.id
                && !target_has_excludes)
            {
                if (local_distance == -1 || local_distance > dist2stair)
                {
                    local_distance = dist2stair;
                    if (cur == player_level && you.pos() == stair)
                        best_stair = si.position;
                }
                continue;
            }

            if (dest.id.depth > -1) // We have a valid level descriptor.
            {
                int dist = level_distance(dest.id, target.id);
                if (dist != -1 && (dist < best_level_distance
                                   || best_level_distance == -1))
                {
                    best_level_distance = dist;
                    closest_level       = dest.id;
                }
            }

            // If we don't know where these stairs go, we can't take them.
            if (!dest.is_valid())
                continue;

            // We need to get the stairs at the new location and set the
            // distance on them as well.
            LevelInfo &lo = travel_cache.get_level_info(dest.id);
            if (stair_info *so = lo.get_stair(dest.pos))
            {
                if (so->distance == -1 || so->distance > dist2stair)
                    so->distance = dist2stair;
                else
                    continue;   // We've already been here.
            }

            // Okay, take these stairs and keep going.
            const int newdist =
                _find_transtravel_stair(dest.id, target,
                                        dist2stair, dest.pos, closest_level,
                                        best_level_distance, best_stair,
                                        target_has_excludes);
            if (newdist != -1
                && (local_distance == -1 || local_distance > newdist))
            {
                local_distance = newdist;
                if (cur == player_level && you.pos() == stair)
                    best_stair = si.position;
            }
        }
    }
    return local_distance;
}

static bool _loadlev_populate_stair_distances(const level_pos &target)
{
    std::auto_ptr<crawl_environment> tmp(new crawl_environment(env));

    bool loaded = false;
    if (travel_load_map(target.id.branch,
                        absdungeon_depth(target.id.branch, target.id.depth)))
    {
        std::vector<travel_exclude> old_excludes = curr_excludes;

        curr_excludes.clear();
        LevelInfo &li = travel_cache.get_level_info(target.id);
        li.set_level_excludes();

        _populate_stair_distances(target);

        curr_excludes = old_excludes;
        loaded = !curr_stairs.empty();
    }

    env = *tmp;
    // Clear references to freed markers.
    dungeon_events.clear();
    // Reactivate cloned markers.
    env.markers.activate_all(false);

    return (loaded);
}

static void _populate_stair_distances(const level_pos &target)
{
    // Populate travel_point_distance.
    find_travel_pos(target.pos, NULL, NULL, NULL);

    LevelInfo &li = travel_cache.get_level_info(target.id);
    const std::vector<stair_info> &stairs = li.get_stairs();

    curr_stairs.clear();
    for (int i = 0, count = stairs.size(); i < count; ++i)
    {
        stair_info si = stairs[i];
        si.distance = travel_point_distance[si.position.x][si.position.y];
        if (!si.distance && target.pos != si.position
            || si.distance < -1)
        {
            si.distance = -1;
        }

        curr_stairs.push_back(si);
    }
}

static bool _find_transtravel_square(const level_pos &target, bool verbose)
{
    level_id current = level_id::current();

    coord_def best_stair(-1, -1);
    coord_def cur_stair(you.pos());

    level_id closest_level;
    int best_level_distance = -1;
    travel_cache.clear_distances();

    find_travel_pos(you.pos(), NULL, NULL, NULL);

    const LevelInfo &target_level =
        travel_cache.get_level_info( target.id );

    _find_transtravel_stair(current, target,
                            0, cur_stair, closest_level,
                            best_level_distance, best_stair,
                            !target_level.get_excludes().empty());

    if (best_stair.x != -1 && best_stair.y != -1)
    {
        // Is this stair going offlevel?
        if ((level_target.p.id != current
             || level_target.p.pos != best_stair)
            && _Src_Dest_Level_Delta != -1)
        {
            // If so, is the original level closer to the target level than
            // the destination of the stair?
            LevelInfo &li = travel_cache.get_level_info(current);
            const stair_info *dest_stair = li.get_stair(best_stair);

            if (dest_stair && dest_stair->destination.id.is_valid())
            {
                if ((_Src_Dest_Level_Delta <
                     level_distance(dest_stair->destination.id,
                                    level_target.p.id) ||
                     _Src_Dest_Level_Delta <
                     level_distance(dest_stair->destination.id,
                                    _Src_Level))
                    && !_Route_Warning.warn_continue_travel(
                        level_target,
                        dest_stair->destination.id))
                    return (false);
            }
        }

        you.running.pos = best_stair;
        return (true);
    }
    else if (best_level_distance != -1 && closest_level != current
             && target.pos.x == -1)
    {
        int current_dist = level_distance(current, target.id);
        level_pos newlev;
        newlev.id = closest_level;
        if (newlev.id != target.id
            && (current_dist == -1 || best_level_distance < current_dist))
        {
            return _find_transtravel_square(newlev, verbose);
        }
    }

    if (verbose)
    {
        if (target.id != current
            || target.pos.x != -1 && target.pos != you.pos())
        {
            mpr("Sorry, I don't know how to get there.");
        }
    }

    return (false);
}

void start_travel(const coord_def& p)
{
    // Redundant target?
    if (p == you.pos())
        return;

    // Remember where we're going so we can easily go back if interrupted.
    you.travel_x = p.x;
    you.travel_y = p.y;

    if (!i_feel_safe(true, true))
        return;

    // Can we even travel to this square?
    if (!in_bounds(p))
        return;
    if (!is_travelsafe_square(p, true))
        return;

    you.running.pos = p;
    level_target  = level_pos(level_id::current(), p);

    if (!can_travel_interlevel())
    {
        // Start running
        you.running = RMODE_TRAVEL;
        _start_running();
    }
    else
        start_translevel_travel(level_target);
}

void start_explore(bool grab_items)
{
    if (Options.tut_explored)
        Options.tut_explored = 0;

    if (!player_in_mappable_area())
    {
        mpr("It would help if you knew where you were, first.");
        return;
    }

    if (!i_feel_safe(true, true))
        return;

    // Forget interrupted butchering.
    you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = 0;

    you.running = grab_items? RMODE_EXPLORE_GREEDY : RMODE_EXPLORE;
    if (you.running == RMODE_EXPLORE_GREEDY
        && Options.stash_tracking != STM_ALL)
    {
        Options.explore_greedy = false;
        mpr("Greedy explore is available only if stash_tracking = all");
        more();
        you.running = RMODE_EXPLORE;
    }

    // Clone shadow array off map
    mapshadow = env.map;

    you.running.pos.reset();
    _start_running();
}

// Given a feature vector, arranges the features in the order that the player
// is most likely to be interested in. Currently, the only thing it does is to
// put altars of the player's religion at the front of the list.
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

level_id level_id::current()
{
    const level_id id(you.where_are_you,
                      subdungeon_depth(you.where_are_you, you.your_level),
                      you.level_type);
    return id;
}

int level_id::dungeon_absdepth() const
{
    ASSERT(branch != NUM_BRANCHES && depth != -1);
    return absdungeon_depth(branch, depth);
}

int level_id::absdepth() const
{
    switch (level_type)
    {
    case LEVEL_DUNGEON:
        return absdungeon_depth(branch, depth);
    case LEVEL_PANDEMONIUM:
        return 52;
    case LEVEL_ABYSS:
        return 51;
    default:
        // No true notion of depth here.
        return you.your_level;
    }
}

level_id level_id::get_next_level_id(const coord_def &pos)
{
    int gridc = grd(pos);
    level_id id = current();

    for ( int i = 0; i < NUM_BRANCHES; ++i )
    {
        if ( gridc == branches[i].entry_stairs )
        {
            id.branch = static_cast<branch_type>(i);
            id.depth = 1;
            break;
        }
        if ( gridc == branches[i].exit_stairs )
        {
            id.branch = branches[i].parent_branch;
            id.depth = branches[i].startdepth;
            break;
        }
    }

    switch (gridc)
    {
    case DNGN_STONE_STAIRS_DOWN_I:   case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III: case DNGN_ESCAPE_HATCH_DOWN:
        id.depth++;
        break;
    case DNGN_STONE_STAIRS_UP_I:     case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:   case DNGN_ESCAPE_HATCH_UP:
        id.depth--;
        break;
    default:
        break;
    }
    return id;
}

unsigned short level_id::packed_place() const
{
    return get_packed_place(branch, depth, level_type);
}

std::string level_id::describe( bool long_name, bool with_number ) const
{
    return place_name( this->packed_place(), long_name, with_number );
}

level_id level_id::parse_level_id(const std::string &s) throw (std::string)
{
    std::string::size_type cpos = s.find(':');
    std::string branch = (cpos != std::string::npos? s.substr(0, cpos)  : s);
    std::string depth  = (cpos != std::string::npos? s.substr(cpos + 1) : "");

    if (branch == "Abyss")
        return (level_id(LEVEL_ABYSS));
    else if (branch == "Pan")
        return (level_id(LEVEL_PANDEMONIUM));
    else if (branch == "Lab")
        return (level_id(LEVEL_LABYRINTH));
    else if (branch == "Port")
        return (level_id(LEVEL_PORTAL_VAULT));

    const branch_type br = str_to_branch(branch);
    if (br == NUM_BRANCHES)
    {
        throw make_stringf("Invalid branch \"%s\" in spec \"%s\"",
                           branch.c_str(), s.c_str());
    }

    const int dep = (depth.empty() ? 1 :
                     depth == "$"  ? branches[br].depth
                                   : atoi(depth.c_str()));

    if (dep < 0 || dep > branches[br].depth)
    {
        throw make_stringf("Invalid depth for %s in spec \"%s\"",
                           branch.c_str(), s.c_str());
    }

    return level_id(br, dep);
}

level_id level_id::from_packed_place(const unsigned short place)
{
    level_id id;

    id.branch     = (branch_type) place_branch(place);
    id.depth      = place_depth(place);
    id.level_type = (level_area_type) place_type(place);

    return (id);
}

// NOTE: see also marshall_level_id
void level_id::save(writer& outf) const
{
    marshallShort(outf, branch);
    marshallShort(outf, depth);
    marshallShort(outf, level_type);
}

void level_id::load(reader& inf)
{
    branch     = static_cast<branch_type>(unmarshallShort(inf));
    depth      = unmarshallShort(inf);
    level_type = static_cast<level_area_type>(unmarshallShort(inf));
}

// NOTE: see also marshall_level_pos
void level_pos::save(writer& outf) const
{
    id.save(outf);
    marshallCoord(outf, pos);
}

void level_pos::load(reader& inf)
{
    id.load(inf);
    unmarshallCoord(inf, pos);
}

void stair_info::save(writer& outf) const
{
    marshallCoord(outf, position);
    marshallShort(outf, grid);
    destination.save(outf);
    marshallByte(outf, guessed_pos? 1 : 0);
    marshallByte(outf, type);
}

void stair_info::load(reader& inf)
{
    unmarshallCoord(inf, position);
    grid = static_cast<dungeon_feature_type>(unmarshallShort(inf));
    destination.load(inf);
    guessed_pos = unmarshallByte(inf) != 0;
    type = static_cast<stair_type>(unmarshallByte(inf));
}

std::string stair_info::describe() const
{
    if (destination.is_valid())
    {
        const level_pos &lp(destination);
        return make_stringf( " (-> %s@(%d,%d)%s%s)", lp.id.describe().c_str(),
                             lp.pos.x, lp.pos.y,
                             guessed_pos? " guess" : "",
                             type == PLACEHOLDER? " placeholder" : "" );
    }
    else if (destination.id.is_valid())
        return make_stringf( " (->%s (?))", destination.id.describe().c_str() );

    return (" (?)");
}

void LevelInfo::set_level_excludes()
{
    curr_excludes = excludes;
/*
    for (unsigned int i = 0; i < curr_excludes.size(); i++)
    {
        curr_excludes[i] = travel_exclude(curr_excludes[i].pos,
                                          curr_excludes[i].radius);
    }
*/
}

bool LevelInfo::empty() const
{
    return (stairs.empty() && excludes.empty());
}

void LevelInfo::update()
{
    // First, set excludes, so that stair distances will be correctly populated.
    excludes = curr_excludes;

    // First, we get all known stairs.
    std::vector<coord_def> stair_positions;
    get_stairs(stair_positions);

    // Make sure our stair list is correct.
    correct_stair_list(stair_positions);

    sync_all_branch_stairs();
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
        find_travel_pos(stairs[s].position, NULL, NULL, NULL);

        for (int other = 0; other < end; ++other)
        {
            int ox = stairs[other].position.x,
                oy = stairs[other].position.y;
            int dist = travel_point_distance[ox][oy];

            // Note dist == 0 is illegal because we can't have two stairs on
            // the same square.
            if (dist <= 0)
                dist = -1;

            stair_distances[ s * stairs.size() + other ] = dist;
            stair_distances[ other * stairs.size() + s ] = dist;
        }
    }
}

void LevelInfo::update_stair(const coord_def& stairpos, const level_pos &p,
                             bool guess)
{
    stair_info *si = get_stair(stairpos);

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
        {
            travel_hell_entry = p;
        }

        // All branch stairs land on the same place on the destination level,
        // update the cache accordingly (but leave guessed_pos = true). This
        // applies for both branch exits (the usual case) and branch entrances.
        if (si->destination.id.branch != id.branch)
            sync_branch_stairs(si);
    }
    else if (!si && guess)
        create_placeholder_stair(stairpos, p);
}

void LevelInfo::create_placeholder_stair(const coord_def &stair,
                                         const level_pos &dest)
{
    // If there are any existing placeholders with the same 'dest', zap them.
    for (int i = 0, size = stairs.size(); i < size; ++i)
    {
        if (stairs[i].type == stair_info::PLACEHOLDER
            && stairs[i].destination == dest)
        {
            stairs.erase( stairs.begin() + i );
            break;
        }
    }

    stair_info placeholder;
    placeholder.position    = stair;
    placeholder.grid        = DNGN_FLOOR;
    placeholder.destination = dest;
    placeholder.type        = stair_info::PLACEHOLDER;
    stairs.push_back(placeholder);

    resize_stair_distances();
}

// If a stair leading out of or into a branch has a known destination, all
// stairs of the same type on this level should have the same destination set
// as guessed_pos == true.
void LevelInfo::sync_all_branch_stairs()
{
    std::set<int> synced;

    for (int i = 0, size = stairs.size(); i < size; ++i)
    {
        const stair_info &si = stairs[i];
        if (si.destination.id.branch != id.branch && si.destination.is_valid()
            && synced.find(si.grid) == synced.end())
        {
            synced.insert( si.grid );
            sync_branch_stairs( &si );
        }
    }
}

void LevelInfo::sync_branch_stairs(const stair_info *si)
{
    for (int i = 0, size = stairs.size(); i < size; ++i)
    {
        stair_info &sother = stairs[i];
        if (si == &sother || !sother.guessed_pos || si->grid != sother.grid
            || sother.destination.is_valid())
        {
            continue;
        }
        sother.destination = si->destination;
    }
}

void LevelInfo::clear_stairs(dungeon_feature_type grid)
{
    for (int i = 0, size = stairs.size(); i < size; ++i)
    {
        stair_info &si = stairs[i];
        if (si.grid != grid)
            continue;

        si.destination.id.depth = -1;
        si.destination.pos.x = -1;
        si.destination.pos.y = -1;
        si.guessed_pos = true;
    }
}

stair_info *LevelInfo::get_stair(int x, int y)
{
    const coord_def c(x, y);
    return get_stair(c);
}

bool LevelInfo::know_stair(const coord_def &c) const
{
    const int index = get_stair_index(c);
    if (index == -1)
        return (false);

    const level_pos &lp = stairs[index].destination;
    return (lp.is_valid());
}

stair_info *LevelInfo::get_stair(const coord_def &pos)
{
    int index = get_stair_index(pos);
    return index != -1? &stairs[index] : NULL;
}

int LevelInfo::get_stair_index(const coord_def &pos) const
{
    for (int i = static_cast<int>(stairs.size()) - 1; i >= 0; --i)
        if (stairs[i].position == pos)
            return i;

    return -1;
}

void LevelInfo::correct_stair_list(const std::vector<coord_def> &s)
{
    stair_distances.clear();

    // First we kill any stairs in 'stairs' that aren't there in 's'.
    for (int i = ((int) stairs.size()) - 1; i >= 0; --i)
    {
        if (stairs[i].type != stair_info::PHYSICAL)
            continue;

        bool found = false;
        for (int j = s.size() - 1; j >= 0; --j)
        {
            if (s[j] == stairs[i].position)
            {
                found = true;
                break;
            }
        }

        if (!found)
            stairs.erase(stairs.begin() + i);
    }

    // For each stair in 's', make sure we have a corresponding stair
    // in 'stairs'.
    for (int i = 0, sz = s.size(); i < sz; ++i)
    {
        int found = -1;
        for (int j = stairs.size() - 1; j >= 0; --j)
        {
            if (s[i] == stairs[j].position)
            {
                found = j;
                break;
            }
        }

        if (found == -1)
        {
            stair_info si;
            si.position = s[i];
            si.grid     = grd(si.position);
            si.destination.id = level_id::get_next_level_id(s[i]);
            if (si.destination.id.branch == BRANCH_VESTIBULE_OF_HELL
                && id.branch == BRANCH_MAIN_DUNGEON
                && travel_hell_entry.is_valid())
            {
                si.destination = travel_hell_entry;
            }

            // We don't know where on the next level these stairs go to, but
            // that can't be helped. That information will have to be filled
            // in whenever the player takes these stairs.
            stairs.push_back(si);
        }
        else
            stairs[found].type = stair_info::PHYSICAL;
    }

    resize_stair_distances();
}

void LevelInfo::resize_stair_distances()
{
    const int nstairs = stairs.size();
    stair_distances.reserve( nstairs * nstairs );
    stair_distances.resize( nstairs * nstairs, 0 );
}

int LevelInfo::distance_between(const stair_info *s1, const stair_info *s2)
                    const
{
    if (!s1 || !s2)
        return 0;
    if (s1 == s2)
        return 0;

    int i1 = get_stair_index(s1->position),
        i2 = get_stair_index(s2->position);
    if (i1 == -1 || i2 == -1)
        return 0;

    return stair_distances[ i1 * stairs.size() + i2 ];
}

void LevelInfo::get_stairs(std::vector<coord_def> &st)
{
    for ( rectangle_iterator ri(1); ri; ++ri )
    {
        const dungeon_feature_type grid = grd(*ri);
        const int envc = env.map(*ri).object;

        if ((*ri == you.pos() || envc)
            && is_travelable_stair(grid)
            && (is_terrain_seen(*ri) || !is_branch_stair(*ri)))
        {
            st.push_back(*ri);
        }
    }
}

void LevelInfo::clear_distances()
{
    for (int i = 0, count = stairs.size(); i < count; ++i)
        stairs[i].clear_distance();
}

bool LevelInfo::is_known_branch(unsigned char branch) const
{
    for (int i = 0, count = stairs.size(); i < count; ++i)
        if (stairs[i].destination.id.branch == branch)
            return (true);

    return (false);
}

void LevelInfo::save(writer& outf) const
{
    int stair_count = stairs.size();
    // How many stairs do we know of?
    marshallShort(outf, stair_count);
    for (int i = 0; i < stair_count; ++i)
        stairs[i].save(outf);

    if (stair_count)
    {
        // Save stair distances as short ints.
        const int sz = stair_distances.size();
        for (int i = 0, n = stair_count * stair_count; i < n; ++i)
        {
            if (i >= sz)
                marshallShort(outf, -1);
            else
                marshallShort(outf, stair_distances[i]);
        }
    }

    marshallShort(outf, excludes.size());
    if (excludes.size())
    {
        for (int i = 0, count = excludes.size(); i < count; ++i)
        {
            marshallCoord(outf, excludes[i].pos);
            marshallShort(outf, excludes[i].radius);
        }
    }
}

void LevelInfo::load(reader& inf)
{
    stairs.clear();
    int stair_count = unmarshallShort(inf);
    for (int i = 0; i < stair_count; ++i)
    {
        stair_info si;
        si.load(inf);
        stairs.push_back(si);

        if (id.branch == BRANCH_MAIN_DUNGEON
            && si.destination.id.branch == BRANCH_VESTIBULE_OF_HELL
            && !travel_hell_entry.is_valid()
            && si.destination.is_valid())
        {
            travel_hell_entry = si.destination;
        }
    }

    stair_distances.clear();
    if (stair_count)
    {
        stair_distances.reserve(stair_count * stair_count);
        for (int i = stair_count * stair_count - 1; i >= 0; --i)
            stair_distances.push_back( unmarshallShort(inf) );
    }

    excludes.clear();
    int nexcludes = unmarshallShort(inf);
    if (nexcludes)
    {
        for (int i = 0; i < nexcludes; ++i)
        {
            coord_def c;
            unmarshallCoord(inf, c);
            const int radius = unmarshallShort(inf);
            excludes.push_back(travel_exclude(c, radius));
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
        {
            si.destination = travel_hell_entry;
        }
    }
}

bool TravelCache::know_stair(const coord_def &c) const
{
    travel_levels_map::const_iterator i = levels.find(level_id::current());
    return (i == levels.end() ? false : i->second.know_stair(c));
}

void TravelCache::travel_to_waypoint(int num)
{
    if (num < 0 || num >= TRAVEL_WAYPOINT_COUNT)
        return;

    if (waypoints[num].id.depth == -1)
        return;

    start_translevel_travel(waypoints[num]);
}

void TravelCache::list_waypoints() const
{
    std::string line;
    std::string dest;
    char choice[50];
    int count = 0;

    for (int i = 0; i < TRAVEL_WAYPOINT_COUNT; ++i)
    {
        if (waypoints[i].id.depth == -1)
            continue;

        dest = get_trans_travel_dest(waypoints[i], false, true);

        snprintf(choice, sizeof choice, "(%d) %-8s", i, dest.c_str());
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
        if (lp == waypoints[i])
            return '0' + i;

    return 0;
}

void TravelCache::update_waypoints() const
{
    level_pos lp;
    lp.id = level_id::current();

    memset(curr_waypoints, 0, sizeof curr_waypoints);
    for (lp.pos.x = 1; lp.pos.x < GXM; ++lp.pos.x)
        for (lp.pos.y = 1; lp.pos.y < GYM; ++lp.pos.y)
        {
            unsigned char wpc = is_waypoint(lp);
            if (wpc)
                curr_waypoints[lp.pos.x][lp.pos.y] = wpc;
        }
}

void TravelCache::delete_waypoint()
{
    if (!get_waypoint_count())
        return;

    while (get_waypoint_count())
    {
        mesclr();
        mpr("Existing waypoints:");
        list_waypoints();
        mpr("Delete which waypoint? (* - delete all, Esc - exit) ",
            MSGCH_PROMPT);

        int key = getch();
        if (key >= '0' && key <= '9')
        {
            key -= '0';
            if (waypoints[key].is_valid())
            {
                waypoints[key].clear();
                update_waypoints();
                continue;
            }
        }
        else if (key == '*')
        {
            for (int i = 0; i < TRAVEL_WAYPOINT_COUNT; ++i)
                waypoints[i].clear();

            update_waypoints();
            break;
        }

        canned_msg(MSG_OK);
        return;
    }

    mesclr();
    mpr("All waypoints deleted. Have a nice day!");
}

void TravelCache::add_waypoint(int x, int y)
{
    if (!can_travel_interlevel())
    {
        mpr("Sorry, you can't set a waypoint here.");
        return;
    }

    mesclr();

    const bool waypoints_exist = get_waypoint_count();
    if (waypoints_exist)
    {
        mpr("Existing waypoints:");
        list_waypoints();
    }

    mprf(MSGCH_PROMPT, "Assign waypoint to what number? (0-9%s) ",
         waypoints_exist? ", D - delete waypoint" : "");

    int keyin = tolower(get_ch());

    if (waypoints_exist && keyin == 'd')
    {
        delete_waypoint();
        return;
    }

    if (keyin < '0' || keyin > '9')
    {
        canned_msg(MSG_OK);
        return;
    }

    int waynum = keyin - '0';

    coord_def pos(x,y);
    if (x == -1 || y == -1)
        pos = you.pos();

    const level_id &lid = level_id::current();

    const bool overwrite = waypoints[waynum].is_valid();

    std::string old_dest =
        overwrite ? get_trans_travel_dest(waypoints[waynum], false, true) : "";
    level_id old_lid = (overwrite ? waypoints[waynum].id : lid);

    waypoints[waynum].id  = lid;
    waypoints[waynum].pos = pos;

    std::string new_dest = get_trans_travel_dest(waypoints[waynum],
                                                 false, true);
    mesclr();
    if (overwrite)
    {
        if (lid == old_lid) // same level
            mprf("Waypoint %d re-assigned to your current position.", waynum);
        else
        {
            mprf("Waypoint %d re-assigned from %s to %s.",
                 waynum, old_dest.c_str(), new_dest.c_str());
        }
    }
    else
        mprf("Waypoint %d assigned to %s.", waynum, new_dest.c_str());

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

void TravelCache::clear_distances()
{
    std::map<level_id, LevelInfo>::iterator i = levels.begin();
    for ( ; i != levels.end(); ++i)
        i->second.clear_distances();
}

bool TravelCache::is_known_branch(unsigned char branch) const
{
    std::map<level_id, LevelInfo>::const_iterator i = levels.begin();
    for ( ; i != levels.end(); ++i)
        if (i->second.is_known_branch(branch))
            return (true);

    return (false);
}

void TravelCache::save(writer& outf) const
{
    // Travel cache version information
    marshallByte(outf, TC_MAJOR_VERSION);
    marshallByte(outf, TC_MINOR_VERSION);

    int level_count = 0;
    for (travel_levels_map::const_iterator i = levels.begin();
         i != levels.end(); ++i)
    {
        if (i->first.level_type == LEVEL_DUNGEON)
            ++level_count;
    }

    // Write level count:
    marshallShort(outf, level_count);

    // Save all the LEVEL_DUNGEON levels we have
    std::map<level_id, LevelInfo>::const_iterator i = levels.begin();
    for ( ; i != levels.end(); ++i)
    {
        // LevelInfos will also be created for levels in the Abyss and
        // Pandemonium, but they shouldn't be saved because the
        // information in them is useless.
        if (i->first.level_type != LEVEL_DUNGEON)
            continue;

        i->first.save(outf);
        i->second.save(outf);
    }

    for (int wp = 0; wp < TRAVEL_WAYPOINT_COUNT; ++wp)
        waypoints[wp].save(outf);
}

void TravelCache::load(reader& inf)
{
    levels.clear();

    // Check version. If not compatible, we just ignore the file altogether.
    unsigned char major = unmarshallByte(inf),
                  minor = unmarshallByte(inf);
    if (major != TC_MAJOR_VERSION || minor != TC_MINOR_VERSION)
        return ;

    int level_count = unmarshallShort(inf);
    for (int i = 0; i < level_count; ++i)
    {
        level_id id;
        id.load(inf);
        LevelInfo linfo;
        // Must set id before load, or travel_hell_entry will not be
        // correctly set.
        linfo.id = id;
        linfo.load(inf);
        levels[id] = linfo;
    }

    for (int wp = 0; wp < TRAVEL_WAYPOINT_COUNT; ++wp)
        waypoints[wp].load(inf);

    fixup_levels();
}

void TravelCache::set_level_excludes()
{
    if (can_travel_interlevel())
        get_level_info(level_id::current()).set_level_excludes();
}

void TravelCache::update()
{
    if (can_travel_interlevel())
        get_level_info(level_id::current()).update();
}

void TravelCache::fixup_levels()
{
    std::map<level_id, LevelInfo>::iterator i = levels.begin();
    for ( ; i != levels.end(); ++i)
        i->second.fixup();
}

bool can_travel_to(const level_id &id)
{
    return (id.level_type == LEVEL_DUNGEON && can_travel_interlevel()
            || id.level_type == you.level_type && player_in_mappable_area());
}

bool can_travel_interlevel()
{
    return (you.level_type == LEVEL_DUNGEON);
}

/////////////////////////////////////////////////////////////////////////////
// Shift-running and resting.

runrest::runrest()
    : runmode(0), mp(0), hp(0), pos(0,0)
{
}

// Initialize is only called for resting/shift-running. We should eventually
// include travel and wrap it all in.
void runrest::initialise(int dir, int mode)
{
    // Note HP and MP for reference.
    hp = you.hp;
    mp = you.magic_points;

    if (dir == RDIR_REST)
    {
        pos.reset();
        runmode = mode;
    }
    else
    {
        ASSERT( dir >= 0 && dir <= 7 );

        pos = Compass[dir];
        runmode = mode;

        // Get the compass point to the left/right of intended travel:
        const int left  = (dir - 1 < 0) ? 7 : (dir - 1);
        const int right = (dir + 1 > 7) ? 0 : (dir + 1);

        // Record the direction and starting tile type for later reference:
        set_run_check( 0, left );
        set_run_check( 1, dir );
        set_run_check( 2, right );
    }

    if (runmode == RMODE_REST_DURATION)
        start_delay(DELAY_REST, 1);
    else
        start_delay(DELAY_RUN, 1);
}

runrest::operator int () const
{
    return (runmode);
}

const runrest &runrest::operator = (int newrunmode)
{
    runmode = newrunmode;
    return (*this);
}

static dungeon_feature_type _base_grid_type( dungeon_feature_type grid )
{
    // Don't stop for undiscovered traps:
    if ((grid >= DNGN_FLOOR_MIN && grid <= DNGN_FLOOR_MAX)
        || grid == DNGN_UNDISCOVERED_TRAP)
    {
        return (DNGN_FLOOR);
    }

    // Merge walls and secret doors.
    if (grid_is_wall(grid) || grid == DNGN_SECRET_DOOR)
        return (DNGN_ROCK_WALL);

    return (grid);
}

void runrest::set_run_check(int index, int dir)
{
    run_check[index].delta = Compass[dir];

    const coord_def targ = you.pos() + Compass[dir];

    run_check[index].grid = _base_grid_type( grd(targ) );
}

bool runrest::check_stop_running()
{
    if (runmode > 0 && runmode != RMODE_START && run_grids_changed())
    {
        stop();
        return (true);
    }
    return (false);
}

// This function creates "equivalence classes" so that undiscovered
// traps and secret doors aren't running stopping points.
bool runrest::run_grids_changed() const
{
    if (env.cgrid(you.pos() + pos) != EMPTY_CLOUD)
        return (true);

    if (monster_at(you.pos() + pos))
        return (true);

    for (int i = 0; i < 3; i++)
    {
        const coord_def targ = you.pos() + run_check[i].delta;
        const dungeon_feature_type targ_grid = _base_grid_type(grd(targ));

        if (run_check[i].grid != targ_grid)
            return (true);
    }

    return (false);
}

void runrest::stop()
{
    bool need_redraw =
        (runmode > 0 || runmode < 0 && Options.travel_delay == -1);
    _userdef_run_stoprunning_hook();
    runmode = RMODE_NOT_RUNNING;

    // Kill the delay; this is fine because it's not possible to stack
    // run/rest/travel on top of other delays.
    stop_delay();

    if (need_redraw)
        viewwindow(true, false);

    _reset_zigzag_info();
}

bool runrest::is_rest() const
{
    return (runmode > 0 && pos.origin());
}

bool runrest::is_explore() const
{
    return (runmode == RMODE_EXPLORE || runmode == RMODE_EXPLORE_GREEDY);
}

bool runrest::is_any_travel() const
{
    switch (runmode)
    {
    case RMODE_INTERLEVEL:
    case RMODE_EXPLORE_GREEDY:
    case RMODE_EXPLORE:
    case RMODE_TRAVEL:
        return (true);
    default:
        return (false);
    }
}

void runrest::rest()
{
    // stop_running() Lua hooks will never see rest stops.
    if (runmode > 0)
        --runmode;
}

void runrest::clear()
{
    runmode = RMODE_NOT_RUNNING;
    pos.reset();
    mp = hp = 0;

    _reset_zigzag_info();
}

/////////////////////////////////////////////////////////////////////////////
// explore_discoveries

explore_discoveries::explore_discoveries()
    : es_flags(0), current_level(NULL), items(), stairs(),
      portals(), shops(), altars()
{
}

std::string explore_discoveries::cleaned_feature_description(
    const coord_def &pos) const
{
    std::string s = lowercase_first(feature_description(pos));
    if (s.length() && s[s.length() - 1] == '.')
        s.erase(s.length() - 1);
    if (s.find("a ") != std::string::npos)
        s = s.substr(2);
    else if (s.find("an ") != std::string::npos)
        s = s.substr(3);
    return (s);
}

bool explore_discoveries::merge_feature(
    std::vector< explore_discoveries::named_thing<int> > &v,
    const explore_discoveries::named_thing<int> &feat) const
{
    for (int i = 0, size = v.size(); i < size; ++i)
        if (feat == v[i])
        {
            ++v[i].thing;
            return (true);
        }

    return (false);
}

void explore_discoveries::found_feature(const coord_def &pos,
                                        dungeon_feature_type grid)
{
    if (grid == DNGN_ENTER_SHOP && ES_shop)
    {
        shops.push_back( named_thing<int>( shop_name(pos), grid ) );
        es_flags |= ES_SHOP;
    }
    else if (is_stair(grid) && ES_stair)
    {
        const named_thing<int> stair(cleaned_feature_description(pos), 1);
        add_stair(stair);
        es_flags |= ES_STAIR;
    }
    else if (grid_is_portal(grid) && ES_portal)
    {
        const named_thing<int> portal(cleaned_feature_description(pos), 1);
        add_stair(portal);
        es_flags |= ES_PORTAL;
    }
    else if (is_altar(grid)
             && ES_altar
             && !player_in_branch(BRANCH_ECUMENICAL_TEMPLE))
    {
        const named_thing<int> altar(cleaned_feature_description(pos), 1);
        if (!merge_feature(altars, altar))
            altars.push_back(altar);
        es_flags |= ES_ALTAR;
    }
}

void explore_discoveries::add_stair(
    const explore_discoveries::named_thing<int> &stair)
{
    if (merge_feature(stairs, stair) || merge_feature(portals, stair))
        return;

    // Hackadelic
    if (stair.name.find("stair") != std::string::npos)
        stairs.push_back(stair);
    else
        portals.push_back(stair);
}

void explore_discoveries::add_item(const item_def &i)
{
    item_def copy = i;
    copy.quantity = 1;
    const std::string cname = copy.name(DESC_PLAIN);

    // Try to find something to stack it with, stacking by name.
    for (int j = 0, size = items.size(); j < size; ++j)
    {
        const int orig_quantity = items[j].thing.quantity;
        items[j].thing.quantity = 1;
        if (cname == items[j].thing.name(DESC_PLAIN))
        {
            items[j].thing.quantity = orig_quantity + i.quantity;
            items[j].name =
                items[j].thing.name(DESC_NOCAP_A, false, false, true,
                                    !is_stackable_item(i));
            return;
        }
        items[j].thing.quantity = orig_quantity;
    }

    items.push_back( named_thing<item_def>(i.name(DESC_NOCAP_A), i) );

    // First item of this type?
    // XXX: Only works when travelling.
    tutorial_first_item(i);
}

void explore_discoveries::found_item(const coord_def &pos, const item_def &i)
{
    if (you.running == RMODE_EXPLORE_GREEDY)
    {
        // The things we need to do...
        if (!current_level)
            current_level = StashTrack.find_current_level();

        if (current_level)
        {
            const bool greed_inducing =
                _is_greed_inducing_square(current_level, pos);

            if (greed_inducing && (Options.explore_stop & ES_GREEDY_ITEM))
                ; // Stop for this conditions
            else if (!greed_inducing
                     && ((Options.explore_stop & ES_ITEM)
                         || ((Options.explore_stop & ES_GLOWING_ITEM)
                             && (i.flags & ISFLAG_COSMETIC_MASK))
                         || ((Options.explore_stop & ES_ARTEFACT)
                             && (i.flags & ISFLAG_ARTEFACT_MASK))
                         || ((Options.explore_stop & ES_RUNE)
                             && is_rune(i)) ))
            {
                ; // More conditions to stop for
            }
            else
                return; // No conditions met, don't stop for this item
        }
    } // if (you.running == RMODE_EXPLORE_GREEDY)

    add_item(i);
    es_flags |= (you.running == RMODE_EXPLORE_GREEDY) ? ES_GREEDY_PICKUP :
                                                        ES_PICKUP;
}

// Expensive O(n^2) duplicate search, but we can live with that.
template <class citer> bool explore_discoveries::has_duplicates(
    citer begin, citer end) const
{
    for (citer s = begin; s != end; ++s)
        for (citer z = s + 1; z != end; ++z)
        {
            if (*s == *z)
                return (true);
        }

    return (false);
}

template <class C> void explore_discoveries::say_any(
    const C &coll, const char *stub) const
{
    if (coll.empty())
        return;

    if (has_duplicates(coll.begin(), coll.end()))
    {
        mprf(stub, number_in_words(coll.size()).c_str());
        return;
    }

    const std::string message = "Found " +
        comma_separated_line(coll.begin(), coll.end()) + ".";

    if ((int) message.length() >= get_number_of_cols())
        mprf(stub, number_in_words(coll.size()).c_str());
    else
        mprf("%s", message.c_str());
}

std::vector<std::string> explore_discoveries::apply_quantities(
    const std::vector< named_thing<int> > &v) const
{
    static const char *feature_plural_qualifiers[] =
    {
        " leading ", " back to ", " to ", " of ", " in ", NULL
    };

    std::vector<std::string> things;
    for (int i = 0, size = v.size(); i < size; ++i)
    {
        const named_thing<int> &nt = v[i];
        if (nt.thing == 1)
            things.push_back(article_a(nt.name));
        else
        {
            things.push_back(number_in_words(nt.thing)
                             + " "
                             + pluralise(nt.name, feature_plural_qualifiers));
        }
    }
    return (things);
}

bool explore_discoveries::prompt_stop() const
{
    if (!es_flags)
        return (false);

    say_any(items, "Found %s items.");
    say_any(shops, "Found %s shops.");
    say_any(apply_quantities(altars), "Found %s altars.");
    say_any(apply_quantities(portals), "Found %s gates.");
    say_any(apply_quantities(stairs), "Found %s stairs.");

    return ((Options.explore_stop_prompt & es_flags) != es_flags
            || prompt_stop_explore(es_flags));
}
