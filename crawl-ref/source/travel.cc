/**
 * @file
 * @brief Travel stuff
**/
/* Known issues:
 *   Hardcoded dungeon features all over the place - this thing is a devil to
 *   refactor.
 */
#include "AppHdr.h"

#include "travel.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <set>
#include <sstream>

#include "branch.h"
#include "cloud.h"
#include "clua.h"
#include "command.h"
#include "coordit.h"
#include "daction-type.h"
#include "dactions.h"
#include "directn.h"
#include "delay.h"
#include "dgn-overview.h"
#include "english.h"
#include "env.h"
#include "files.h"
#include "format.h"
#include "god-abil.h"
#include "god-passive.h"
#include "hints.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "libutil.h"
#include "macro.h"
#include "mapmark.h"
#include "menu.h"
#include "message.h"
#include "mon-death.h"
#include "nearby-danger.h"
#include "output.h"
#include "place.h"
#include "prompt.h"
#include "religion.h"
#include "stairs.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "tiles-build-specific.h"
#include "traps.h"
#include "travel-open-doors-type.h"
#include "ui.h"
#include "unicode.h"
#include "unwind.h"
#include "view.h"
#include "zot.h"

enum IntertravelDestination
{
    // warning: the waypoint prompt menu uses values -1 to -10 for waypoints.
    // if the full set of 100 waypoints were ever available from that menu, the
    // values below would need to be shifted...
    ID_FIRST_UI_WAYPOINT = -1,
    ID_LAST_UI_WAYPOINT = -10,

    // Go down a level
    ID_DOWN     = -100,

    // Go up a level
    ID_UP       = -99,

    // Repeat last travel
    ID_REPEAT   = -101,

    // Altar as target
    ID_ALTAR    = -102,

    // Cancel interlevel travel
    ID_CANCEL   = -1000,
};

TravelCache travel_cache;

// Tracks the distance between the target location on the target level and the
// stairs on the level.
static vector<stair_info> curr_stairs;

// Squares that are not safe to travel to on the current level.
exclude_set curr_excludes;

// This is where we last tried to take a stair during interlevel travel.
// Note that last_stair.id.depth should be set to -1 before initiating interlevel
// travel.
static level_pos last_stair;

// Where travel wants to get to.
static level_pos level_target;

// How many stairs there are between the source and destination of
// interlevel travel, as estimated by level_distance.
static int _Src_Dest_Level_Delta = -1;

// Source level where interlevel travel was last activated.
static level_id _Src_Level;

// Remember the last place explore stopped because autopickup failed.
static coord_def explore_stopped_pos;

// The place in the Vestibule of Hell where all portals to Hell land.
static level_pos travel_hell_entry;

static string trans_travel_dest;

// Array of points on the map, each value being the distance the character
// would have to travel to get there. Negative distances imply that the point
// is a) a trap or hostile terrain or b) only reachable by crossing a trap or
// hostile terrain.
travel_distance_grid_t travel_point_distance;

// Apply slime wall checks when checking if squares are travelsafe.
static bool g_Slime_Wall_Check = true;

static uint8_t curr_waypoints[GXM][GYM];

// If true, feat_is_traversable_now() returns the same as feat_is_traversable().
// FIXME: eliminate this. It's needed for RMODE_CONNECTIVITY.
static bool ignore_player_traversability = false;

static bool _is_valid_waypoint_pos(const level_pos &pos);

// N.b. this #define only adds dprfs and so isn't very useful outside of a
// debug build. It also makes long travel extremely slow when enabled on a
// debug build.
//#define DEBUG_TRAVEL

/*
 * Warn if interlevel travel is going to take you outside levels in
 * the range [src,dest].
 */
class deviant_route_warning
{
private:
    level_pos target;
    bool warned;

public:
    deviant_route_warning(): target(), warned(false)
    {
    }

    void new_dest(const level_pos &dest);
    bool warn_continue_travel(const level_pos &des,
                              const level_id &deviant);
};

void deviant_route_warning::new_dest(const level_pos &dest)
{
    if (target != dest)
    {
        warned = false;
        target = dest;
    }
}

// Returns true if the player wants to continue travelling after the warning.
bool deviant_route_warning::warn_continue_travel(
    const level_pos &dest, const level_id &deviant)
{
    // We've already prompted, don't ask again, on the player's head be it.
    if (target == dest && warned)
        return true;

    target = dest;
    const string prompt = make_stringf("Have to go through %s. Continue?",
                                       deviant.describe().c_str());
    // If the user says "Yes, shut up and take me there", we won't ask
    // again for that destination. If the user says "No", we will
    // prompt again.
    return warned = yesno(prompt.c_str(), true, 'n', true, false);
}

static deviant_route_warning _Route_Warning;

static bool _find_transtravel_square(const level_pos &pos,
                                     bool verbose = true);

static bool _loadlev_populate_stair_distances(const level_pos &target);
static void _populate_stair_distances(const level_pos &target);
static bool _is_greed_inducing_square(const LevelStashes *ls,
                                      const coord_def &c, bool autopickup);

// Returns true if there is a known trap at (x,y). Returns false for non-trap
// squares as also for undiscovered traps.
//
static inline bool is_trap(const coord_def& c)
{
    return feat_is_trap(env.map_knowledge(c).feat());
}

static inline bool _is_safe_cloud(const coord_def& c)
{
    const cloud_type ctype = env.map_knowledge(c).cloud();
    if (ctype == CLOUD_NONE)
        return true;

    // We can also safely run through smoke, or any of our own clouds if
    // following Qazlal.
    return !is_damaging_cloud(ctype, true, YOU_KILL(env.map_knowledge(c).cloudinfo()->killer));
}

// Returns an estimate for the time needed to cross this feature.
// This is done, so traps etc. will usually be circumvented where possible.
static inline int _feature_traverse_cost(dungeon_feature_type feature)
{
    if (feat_is_closed_door(feature)
        // Higher cost for shallow water if species doesn't like water
        || feature == DNGN_SHALLOW_WATER && (!player_likes_water(true)))
    {
        return 2;
    }
    else if (feat_is_trap(feature) && feature != DNGN_TRAP_SHAFT)
        return 3;

    return 1;
}

bool is_unknown_stair(const coord_def &p)
{
    dungeon_feature_type feat = env.map_knowledge(p).feat();

    return feat_is_travelable_stair(feat) && !travel_cache.know_stair(p)
           && feat != DNGN_EXIT_DUNGEON;
}

/**
 * Does the player know this transporter's destination?
 *
 * @param p    The location of the transporter.
 * @returns    True if the feature is a transporter with an unknown
 *             destination, false otherwise.
 **/
bool is_unknown_transporter(const coord_def &p)
{
    dungeon_feature_type feat = env.map_knowledge(p).feat();

    return feat == DNGN_TRANSPORTER && !travel_cache.know_transporter(p);
}

// Returns true if the character can cross this dungeon feature, and
// the player hasn't requested that travel avoid the feature.
bool feat_is_traversable_now(dungeon_feature_type grid, bool try_fallback,
                             bool assume_flight)
{
    if (!ignore_player_traversability)
    {
        // If the feature is in travel_avoid_terrain, respect that.
        if (Options.travel_avoid_terrain[grid])
            return false;

        // Swimmers and water-walkers get deep water.
        if (grid == DNGN_DEEP_WATER
            && (player_likes_water(true) || have_passive(passive_t::water_walk)))
        {
            return true;
        }

        // The player can safely walk over shafts.
        if (grid == DNGN_TRAP_SHAFT)
            return true;

        // Permanently flying players can cross most hostile terrain.
        if (grid == DNGN_DEEP_WATER || grid == DNGN_LAVA)
            return assume_flight || you.permanent_flight();
        // Players casting Toxic Bog can safely traverse it
        else if (grid == DNGN_TOXIC_BOG)
            return you.duration[DUR_NOXIOUS_BOG];
    }

    return feat_is_traversable(grid, try_fallback);
}

// Returns true if a generic character can cross this dungeon feature.
// Ignores swimming, flying, and travel_avoid_terrain.
bool feat_is_traversable(dungeon_feature_type feat, bool try_fallback)
{
    if (feat_is_trap(feat) && feat != DNGN_PASSAGE_OF_GOLUBRIA)
    {
        if (ignore_player_traversability)
            return !(feat == DNGN_TRAP_TELEPORT || feat == DNGN_TRAP_TELEPORT_PERMANENT);
        return false;
    }
#if TAG_MAJOR_VERSION == 34
    else if (feat == DNGN_TELEPORTER) // never ever enter it automatically
        return false;
#endif
    else if (feat_has_solid_floor(feat)
             || feat_is_closed_door(feat)
                && (!feat_is_sealed(feat) || try_fallback))
    {
        return true;
    }
    else
        return false;
}

static const char *_run_mode_name(int runmode)
{
    return runmode == RMODE_TRAVEL         ? "travel" :
           runmode == RMODE_INTERLEVEL     ? "intertravel" :
           runmode == RMODE_EXPLORE        ? "explore" :
           runmode == RMODE_EXPLORE_GREEDY ? "explore_greedy" :
           runmode > 0                     ? "run"
                                           : "";
}

uint8_t is_waypoint(const coord_def &p)
{
    return curr_waypoints[p.x][p.y];
}

static inline bool is_stash(const LevelStashes *ls, const coord_def& p)
{
    return ls && ls->find_stash(p);
}

static bool _monster_blocks_travel(const monster_info *mons)
{
    return mons
           && mons_class_is_stationary(mons->type)
           && !fedhas_passthrough(mons);
}

// Returns true if the feature type "grid" is a closed door which
// autoexplore/travel will not normally approach in order to go through it.
static bool _feat_is_blocking_door(const dungeon_feature_type grid)
{
    if (Options.travel_open_doors == travel_open_doors_type::avoid)
        return feat_is_closed_door(grid);
    else
        return feat_is_runed(grid);
}

// Returns {flag, barrier}.
// "flag" is true if the feature type "grid" is a closed door which autotravel
// will not pass through, false otherwise.
// "barrier" is a description of "grid" if autotravel will not pass through it,
// and no game time has passed since the player pressed (say) "o", "" otherwise.
// This function should only be used for the choice to open the door itself.
static pair<bool, string> _feat_is_blocking_door_strict(
    const dungeon_feature_type grid)
{
    if (Options.travel_open_doors == travel_open_doors_type::open
        ? !feat_is_runed(grid) : !feat_is_closed_door(grid))
    {
        return {false, ""};
    }

    if (you.elapsed_time == you.elapsed_time_at_last_input)
    {
        string barrier;
        if (feat_is_runed(grid))
            return {true, "unopened runed door"};
        else
            return {true, "unopened door"};
    }
    return {true, ""};
}

/*
 * Returns true if the square at (x,y) is a dungeon feature the character
 * can't (under normal circumstances) safely cross.
 *
 * This is used to decide which squares to reseed from when flood-filling
 * outwards, and will affect whether autoexplore considers a level to be fully
 * explored or only partially explored, when attempting to path through a tile
 * that is considered elsewhere to be non-traversable.
 */
static bool _is_reseedable(const coord_def& c, bool ignore_danger = false)
{
    map_cell &cell(env.map_knowledge(c));
    const dungeon_feature_type grid = cell.feat();

    if (feat_is_wall(grid) || feat_is_tree(grid))
        return false;

    if (!ignore_danger && is_excluded(c))
        return true;

    return feat_is_water(grid)
           || grid == DNGN_LAVA
           || grid == DNGN_BINDING_SIGIL
           || _feat_is_blocking_door(grid)
           || is_trap(c)
           || !ignore_danger && _monster_blocks_travel(cell.monsterinfo())
           || g_Slime_Wall_Check && slime_wall_neighbour(c)
           || !_is_safe_cloud(c);
}

struct cell_travel_safety
{
    bool safe;
    bool safe_if_ignoring_hostile_terrain;

    cell_travel_safety() : safe(false), safe_if_ignoring_hostile_terrain(false)
    {
    }
};

typedef FixedArray<cell_travel_safety, GXM, GYM> travel_safe_grid;
static unique_ptr<travel_safe_grid> _travel_safe_grid;

class precompute_travel_safety_grid
{
private:
    bool did_compute;

public:
    precompute_travel_safety_grid() : did_compute(false)
    {
        if (!_travel_safe_grid)
        {
            did_compute = true;
            auto tsgrid = make_unique<travel_safe_grid>();
            travel_safe_grid &safegrid(*tsgrid);
            for (rectangle_iterator ri(1); ri; ++ri)
            {
                const coord_def p(*ri);
                cell_travel_safety &ts(safegrid(p));
                ts.safe = is_travelsafe_square(p, false);
                ts.safe_if_ignoring_hostile_terrain =
                    is_travelsafe_square(p, true);
            }
            _travel_safe_grid = std::move(tsgrid);
        }
    }
    ~precompute_travel_safety_grid()
    {
        if (did_compute)
            _travel_safe_grid.reset(nullptr);
    }
};

bool is_stair_exclusion(const coord_def &p)
{
    const auto f = env.map_knowledge(p).feat();
    if (!feat_is_stair(f) || feat_stair_direction(f) == CMD_NO_CMD)
        return false;

    return get_exclusion_radius(p) == 1;
}

// Returns true if the square at (x,y) is okay to travel over. If ignore_hostile
// is true, returns true even for dungeon features the character can normally
// not cross safely (deep water, lava, traps).
bool is_travelsafe_square(const coord_def& c, bool ignore_hostile,
                                  bool ignore_danger, bool try_fallback)
{
    if (!in_bounds(c))
        return false;

    if (_travel_safe_grid)
    {
        const cell_travel_safety &cell((*_travel_safe_grid)(c));
        return ignore_hostile? cell.safe_if_ignoring_hostile_terrain
               : cell.safe;
    }

    if (!env.map_knowledge(c).known())
        return false;

    const dungeon_feature_type grid = env.map_knowledge(c).feat();

    // Only try pathing through temporary obstructions we remember, not
    // those we can actually see (since the latter are clearly still blockers)
    try_fallback = try_fallback
                    && (!you.see_cell(c) || _feat_is_blocking_door(grid));

    // Also make note of what's displayed on the level map for
    // plant/fungus checks.
    const map_cell& levelmap_cell = env.map_knowledge(c);

    // Travel will not voluntarily cross squares blocked by immobile
    // monsters.
    if (!ignore_danger && !ignore_hostile)
    {
        const monster_info *minfo = levelmap_cell.monsterinfo();
        if (minfo && _monster_blocks_travel(minfo))
            return false;
    }

    // If 'ignore_hostile' is true, we're ignoring hazards that can be
    // navigated over if the player is willing to take damage or fly.
    if (ignore_hostile && _is_reseedable(c, true))
        return true;

    // Excluded squares are only safe if marking stairs, i.e. another level.
    if (!ignore_danger && !ignore_hostile && is_excluded(c)
        && !is_stair_exclusion(c))
    {
        return false;
    }

    if (g_Slime_Wall_Check && slime_wall_neighbour(c)
        && !actor_slime_wall_immune(&you))
    {
        return false;
    }

    if (!_is_safe_cloud(c) && !try_fallback)
        return false;

    if (is_trap(c))
    {
        trap_def trap;
        trap.pos = c;
        trap.type = env.map_knowledge(c).trap();
        trap.ammo_qty = 1;
        if (trap.is_safe())
            return true;
    }

    if (grid == DNGN_BINDING_SIGIL && !you.is_binding_sigil_immune())
        return false;

    if (!try_fallback && _feat_is_blocking_door(levelmap_cell.feat()))
        return false;

    return feat_is_traversable_now(grid, try_fallback);
}

// Returns true if the location at (x,y) is monster-free and contains
// no clouds. Travel uses this to check if the square the player is
// about to move to is safe.
static bool _is_safe_move(const coord_def& c)
{
    if (const monster* mon = monster_at(c))
    {
        // Stop before wasting energy on plants and fungi,
        // unless worshipping Fedhas.
        if (you.can_see(*mon)
            && mons_class_is_firewood(mon->type)
            && !fedhas_passthrough(mon))
        {
            return false;
        }

        // If this is any *other* monster, it'll be visible and
        // a) Friendly, in which case we'll displace it, no problem.
        // b) Unfriendly, in which case we're in deep trouble, since travel
        //    should have been aborted already by the checks in view.cc.
    }

    if (is_trap(c) && !trap_at(c)->is_safe())
        return false;

    return _is_safe_cloud(c);
}

void travel_init_load_level()
{
    curr_excludes.clear();
    travel_cache.set_level_excludes();
    travel_cache.update_waypoints();
}

// This is called after the player changes level.
void travel_init_new_level()
{
    // Clear run details, but preserve the runmode, because we might be in
    // the middle of interlevel travel.
    int runmode = you.running;
    you.running.clear();
    you.running = runmode;

    travel_init_load_level();

    explore_stopped_pos.reset();
}

static bool _is_branch_stair(const coord_def& pos)
{
    const level_id curr = level_id::current();
    const level_id next = level_id::get_next_level_id(pos);

    return next.branch != curr.branch;
}

#define ES_item   (Options.explore_stop & ES_ITEM)
#define ES_greedy (Options.explore_stop & ES_GREEDY_ITEM)
#define ES_glow   (Options.explore_stop & ES_GLOWING_ITEM)
#define ES_art    (Options.explore_stop & ES_ARTEFACT)
#define ES_rune   (Options.explore_stop & ES_RUNE)
#define ES_shop   (Options.explore_stop & ES_SHOP)
#define ES_stair  (Options.explore_stop & ES_STAIR)
#define ES_transporter (Options.explore_stop & ES_TRANSPORTER)
#define ES_altar  (Options.explore_stop & ES_ALTAR)
#define ES_portal (Options.explore_stop & ES_PORTAL)
#define ES_branch (Options.explore_stop & ES_BRANCH)
#define ES_rdoor  (Options.explore_stop & ES_RUNED_DOOR)
#define ES_stack  (Options.explore_stop & ES_GREEDY_VISITED_ITEM_STACK)

// Adds interesting stuff on the point p to explore_discoveries.
static inline void _check_interesting_square(const coord_def pos,
                                             explore_discoveries &ed)
{
    if ((ES_item || ES_greedy || ES_glow || ES_art || ES_rune)
        && you.visible_igrd(pos) != NON_ITEM)
    {
        ed.found_item(pos, env.item[ you.visible_igrd(pos) ]);
    }

    ed.found_feature(pos, env.grid(pos));
}

static void _userdef_run_stoprunning_hook()
{
    if (you.running)
        clua.callfn("ch_stop_running", "s", _run_mode_name(you.running));
}

static void _userdef_run_startrunning_hook()
{
    if (you.running)
        clua.callfn("ch_start_running", "s", _run_mode_name(you.running));
}

bool is_resting()
{
    return you.running.is_rest();
}

static void _start_running()
{
    _userdef_run_startrunning_hook();
    you.running.turns_passed = 0;
    const bool unsafe = Options.travel_one_unsafe_move &&
                        (you.running == RMODE_TRAVEL
                         || you.running == RMODE_INTERLEVEL);

    if (you.running < 0)
        start_delay<TravelDelay>(unsafe);
}

// Stops shift+running and all forms of travel.
void stop_running(bool clear_delays)
{
    you.running.stop(clear_delays);
}

static bool _is_valid_explore_target(const coord_def& where)
{
    // If a square in LOS is unmapped, it's valid.
    for (radius_iterator ri(where, LOS_DEFAULT, true); ri; ++ri)
        if (!env.map_knowledge(*ri).seen())
            return true;

    if (you.running == RMODE_EXPLORE_GREEDY)
    {
        LevelStashes *lev = StashTrack.find_current_level();
        return lev && lev->needs_visit(where, can_autopickup());
    }

    return false;
}

enum explore_status_type
{
    EST_FULLY_EXPLORED    = 0,

    // Could not explore because of hostile terrain
    EST_PARTLY_EXPLORED   = 1,

    // Could not pick up interesting items because of hostile terrain. Note
    // that this and EST_PARTLY_EXPLORED are not mutually exclusive.
    EST_GREED_UNFULFILLED = 2,
};

static bool _level_has_unknown_transporters()
{
    LevelInfo *li = travel_cache.find_level_info(level_id::current());
    ASSERT(li);

    vector<transporter_info> transporters = li->get_transporters();
    for (auto ti : transporters)
    {
        if (ti.destination.origin())
            return true;
    }
    return false;
}

static void _set_target_square(const coord_def &target)
{
    you.running.pos = target;
}

static void _explore_find_target_square()
{
    bool runed_door_pause = false;
    bool closed_door_pause = false;

    travel_pathfind tp;
    tp.set_floodseed(you.pos(), true);

    coord_def whereto =
        tp.pathfind(static_cast<run_mode_type>(you.running.runmode));

    // If we didn't find an explore target the first time, try fallback mode
    // Do the same if the original target was a "hostile" place.
    if (!whereto.x && !whereto.y
       || 0 > travel_point_distance[whereto.x][whereto.y])
    {
        travel_pathfind fallback_tp;
        fallback_tp.set_floodseed(you.pos(), true);
        whereto = fallback_tp.pathfind(static_cast<run_mode_type>(you.running.runmode), true);

        if (whereto.distance_from(you.pos()) == 1)
        {
            if (cell_is_runed(whereto))
            {
                runed_door_pause = true;
                whereto.reset();
            }
            // use orig_terrain() as that's what cell_is_runed() does.
            else if (Options.travel_open_doors == travel_open_doors_type::avoid
                     && feat_is_closed_door(orig_terrain(whereto)))
            {
                closed_door_pause = true;
                whereto.reset();
            }
        }
    }

    if (whereto.x || whereto.y)
    {
        // Make sure this is a square that is reachable, since we asked
        // travel_pathfind to give us even unreachable squares. The
        // player's starting position may in some cases not have its
        // travel_point_distance set, but we know it's reachable, since
        // we're there.
        if (travel_point_distance[whereto.x][whereto.y] <= 0
            && whereto != you.pos())
        {
            whereto.reset();
        }
    }

    if (whereto.x || whereto.y)
    {
        _set_target_square(whereto);
        return;
    }
    else
    {
        // No place to go? Report to the player.
        const int estatus = tp.explore_status();
        const bool unknown_trans = _level_has_unknown_transporters();
        if (!estatus && !unknown_trans)
        {
            mpr("Done exploring.");
            learned_something_new(HINT_DONE_EXPLORE);
        }
        else
        {
            vector<const char *> reasons;
            vector<const char *> inacc;
            string inacc_desc = "";

            if (runed_door_pause)
                reasons.push_back("unopened runed door");

            if (unknown_trans)
                reasons.push_back("unvisited transporter");

            if (closed_door_pause)
                reasons.push_back("unopened door");

            if (estatus & EST_GREED_UNFULFILLED)
                inacc.push_back("items");
            // A runed door already implies an unexplored place.
            if (!runed_door_pause && !closed_door_pause &&
                estatus & EST_PARTLY_EXPLORED)
            {
                inacc.push_back("places");
            }

            if (!inacc.empty())
            {
                inacc_desc = make_stringf("can't reach some %s",
                                 comma_separated_line(inacc.begin(),
                                                      inacc.end()).c_str());
                reasons.push_back(inacc_desc.c_str());
            }
            mprf("Partly explored, %s.",
                 comma_separated_line(reasons.begin(), reasons.end()).c_str());
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
            (you.running == RMODE_EXPLORE_GREEDY) ? ES_GREEDY_PICKUP_MASK
                                                  : ES_NONE;

        if (Options.explore_stop & estop)
            stop_delay();
    }

    // Greedy explore has no good way to deal with an item that we can't
    // pick up, so the only thing to do is to stop.
    if (tried_pickup && you.running == RMODE_EXPLORE_GREEDY)
    {
        if (explore_stopped_pos == you.pos())
        {
            stack_iterator stk(you.pos());
            string wishlist = comma_separated_fn(stk, stack_iterator::end(),
                    [] (const item_def & item) { return item.name(DESC_A); },
                    " and ", ", ", bind(item_needs_autopickup, placeholders::_1,
                                        false));
            // XX [A] doesn't make sense for items being picked up only because
            // of an =g inscription
            const string prompt =
                make_stringf("Could not pick up %s here; ([A]lways) ignore %s?",
                             wishlist.c_str(),
                             tried_pickup == 1 ? "it" : "them");

            // Make Escape => 'n' and stop run.
            explicit_keymap map;
            map[ESCAPE] = 'n';
            map[CONTROL('G')] = 'n';
            map[' '] = 'y';

            // If response is Yes (1) or Always (2), mark items for no pickup
            // If the response is Always, remove the item from autopickup
            // Otherwise, stop autoexplore.
            int response = yesno(prompt.c_str(), true, 'n', true, false,
                                 false, &map, true, true);
            switch (response)
            {
                case 2:
                {
                    vector<string> ap_disabled;
                    for (stack_iterator si(you.pos()); si; ++si)
                    {
                        if (!item_needs_autopickup(*si)
                            || item_autopickup_level(*si) == AP_FORCE_OFF)
                        {
                            continue;
                        }

                        set_item_autopickup(*si, AP_FORCE_OFF);
                        ap_disabled.push_back(pluralise(si->name(DESC_DBNAME)));
                    }

                    if (!ap_disabled.empty())
                    {
                        mprf("Autopickup disabled for %s.",
                             comma_separated_line(ap_disabled.begin(),
                                                  ap_disabled.end()).c_str());
                    }
                }
                // intentional fallthrough
                case 1:
                    mark_items_non_pickup_at(you.pos());
                    return;
                default:
                    break;
            }

            canned_msg(MSG_OK);
        }
        explore_stopped_pos = you.pos();
        stop_delay();
    }
}

/**
 * Run the travel_pathfind algorithm with a destination with the aim of
 * determining the next travel move. Try to avoid to let travel (including
 * autoexplore) move the player right next to a lurking (previously unseen)
 * monster.
 *
 * Pathfinding runs from you.running.pos to youpos, and the move contains the
 * next movement relative to youpos to move closer to you.running.pos. If a
 * runed door (or a closed door, if travel_open_doors isn't open) is encountered
 * or a transporter needs to be taken, these are set to 0, and the caller checks
 * for this.
 *
 * @param      youpos The starting position.
 * @param[out] move_x If we want a travel move, the x coordinate.
 * @param[out] move_y If we want a travel move, the y coordinate.
 */
static void _find_travel_pos(const coord_def& youpos, int *move_x, int *move_y)
{
    travel_pathfind tp;

    tp.set_src_dst(youpos, you.running.pos);

    coord_def dest = tp.pathfind(RMODE_TRAVEL, false);
    if (dest.origin())
        dest = tp.pathfind(RMODE_TRAVEL, true);
    coord_def new_dest = dest;

    // We'd either have to travel through a runed door, in which case we'll be
    // stopping, or a transporter, in which case we need to issue a command to
    // enter.
    pair<bool, string> barrier;
    if ((barrier = _feat_is_blocking_door_strict(env.grid(new_dest))).first
            || env.grid(youpos) == DNGN_TRANSPORTER
               && env.grid(new_dest) == DNGN_TRANSPORTER_LANDING
               && youpos.distance_from(new_dest) > 1)
    {
        *move_x = 0;
        *move_y = 0;
        if (!barrier.second.empty())
        {
            mpr("Could not " + you.running.runmode_name() + ", "
                + barrier.second + ".");
        }
        return;
    }

    // Check whether this step puts us adjacent to any grid we haven't ever
    // seen
    //
    // .tx      Moving onto t puts us adjacent to an unseen grid.
    // ?#@      --> Pick x instead.
    // Only applies to diagonal moves.
    if (!dest.origin() && dest.x != youpos.x && dest.y != youpos.y)
    {
        coord_def unseen = coord_def();
        for (adjacent_iterator ai(dest); ai; ++ai)
            if (!you.see_cell(*ai) && !env.map_knowledge(*ai).seen())
            {
                unseen = *ai;
                break;
            }

        if (unseen != coord_def())
        {
            // If so, try to use an orthogonally adjacent grid that is safe to
            // enter.
            if (youpos.x == unseen.x)
                new_dest.y = youpos.y;
            else if (youpos.y == unseen.y)
                new_dest.x = youpos.x;

            // If the new grid cannot be entered, reset to dest. This means
            // that autoexplore will still sometimes move you next to a
            // previously unseen monster but the same would happen by manual
            // movement, so I don't think we need to worry about this. (jpeg)
            if (!is_travelsafe_square(new_dest)
                || !feat_is_traversable_now(env.map_knowledge(new_dest).feat()))
            {
                new_dest = dest;
            }
#ifdef DEBUG_SAFE_EXPLORE
            mprf(MSGCH_DIAGNOSTICS, "youpos: (%d, %d), dest: (%d, %d), "
                     "unseen: (%d, %d), new_dest: (%d, %d)",
                 youpos.x, youpos.y, dest.x, dest.y, unseen.x, unseen.y,
                 new_dest.x, new_dest.y);
            more();
#endif
        }
    }

    if (new_dest.origin())
        you.running = RMODE_NOT_RUNNING;

    *move_x = new_dest.x - youpos.x;
    *move_y = new_dest.y - youpos.y;
}

// Determine the necessary command when find_travel_pos() indicates that we
// shouldn't move.
static command_type _get_non_move_command()
{
    // Did we fail to get where we were going?
    const bool fell_short = you.pos() != you.running.pos;

    if (you.running == RMODE_EXPLORE)
        return CMD_NO_CMD;

    // Stop exploring if we fell short of our target (because of a runed
    // door), but inspect the floor otherwise (because of an item that
    // could not be picked up).
    if (you.running == RMODE_EXPLORE_GREEDY)
        return fell_short ? CMD_NO_CMD : CMD_INSPECT_FLOOR;

    const level_pos curr = level_pos(level_id::current(), you.pos());

    // We've reached our travel destination.
    if (level_target == curr)
        return CMD_NO_CMD;

    // If we we're not at our running position and we're not travelled to a
    // transporter, simply stop running.
    if (fell_short && env.grid(you.pos()) != DNGN_TRANSPORTER)
        return CMD_NO_CMD;

    // We're trying to take the same stairs again, abort.
    if (last_stair == curr)
        return CMD_NO_CMD;

    // Save the previous stair taken, so we can check that we're not trying to
    // retake them after this command.
    last_stair.id = level_id::current();
    last_stair.pos = you.pos();

    return feat_stair_direction(env.grid(you.pos()));
}

// Top-level travel control (called indirectly from TravelDelay::handle()).
//
// travel() is responsible for making the individual moves that constitute
// (interlevel) travel and explore and deciding when travel and explore
// end.
//
// Don't call travel() if you.running >= 0.
command_type travel()
{
    int holdx, holdy;
    int *move_x = &holdx;
    int *move_y = &holdy;
    holdx = holdy = 0;

    command_type result = CMD_NO_CMD;

    if (Options.travel_key_stop && kbhit())
    {
        mprf("Key pressed, stopping %s.", you.running.runmode_name().c_str());
        stop_running();
        return CMD_NO_CMD;
    }

    // Excluded squares are only safe if marking stairs, i.e. another level.
    if (is_excluded(you.pos()) && !is_stair_exclusion(you.pos()))
    {
        mprf("You're in a travel-excluded area, stopping %s.",
             you.running.runmode_name().c_str());
        stop_running();
        return CMD_NO_CMD;
    }

    if (you.running.is_explore())
    {
        if (Options.explore_auto_rest && !you.is_sufficiently_rested()
            || you.duration[DUR_NO_MOMENTUM])
        {
            return CMD_WAIT;
        }

        // Exploring.
        if (env.grid(you.pos()) == DNGN_ENTER_SHOP
            && you.running == RMODE_EXPLORE_GREEDY)
        {
            LevelStashes *lev = StashTrack.find_current_level();
            if (lev && lev->shop_needs_visit(you.pos()))
            {
                you.running = 0;
                return CMD_GO_UPSTAIRS;
            }
        }

// #define DEBUG_EXPLORE
#ifdef DEBUG_EXPLORE
        if (you.running.pos == you.pos())
        {
            mprf("Stopping explore at target %d,%d", you.running.pos.x, you.running.pos.y);
            stop_running();
            return CMD_NO_CMD;
        }
        else if (!_is_valid_explore_target(you.running.pos))
        {
            mprf("Stopping explore; everything in los of %d,%d is mapped", you.running.pos.x, you.running.pos.y);
            stop_running();
            return CMD_NO_CMD;
        }
#endif

        // Speed up explore by not doing a double-floodfill if we have
        // a valid target.
        if (!you.running.pos.x
            || you.running.pos == you.pos()
            || !_is_valid_explore_target(you.running.pos))
        {
            _explore_find_target_square();
        }
    }

    // Interlevel travel. Since you.running.x is zero, we've either just
    // initiated travel, or we've just climbed or descended a staircase, and we
    // need to figure out where to travel to next.
    if (you.running == RMODE_INTERLEVEL && !you.running.pos.x)
    {
#ifdef DEBUG_TRAVEL
        dprf("continuing translevel travel, branch %d depth %d, pos %d,%d",
            level_target.id.branch, level_target.id.depth, level_target.pos.x,
            level_target.pos.y);
#endif

        if (!_find_transtravel_square(level_target) || !you.running.pos.x)
            stop_running();
    }

    if (you.running < 0)
    {
        // Remember what run-mode we were in so that we can resume
        // explore/interlevel travel correctly.
        int runmode = you.running;

        // Get the next step to make. If the travel command can't find a route,
        // we turn off travel (find_travel_pos does that automatically).
        _find_travel_pos(you.pos(), move_x, move_y);

        // Stop greedy explore when visiting a stash for the first time.
        if ((*move_x || *move_y)
            && you.running == RMODE_EXPLORE_GREEDY
            && ES_stack)
        {
            const coord_def newpos = you.pos() + coord_def(*move_x, *move_y);
            if (newpos == you.running.pos)
            {
                const LevelStashes *lev = StashTrack.find_current_level();
                if (lev && lev->needs_stop(newpos))
                {
                    explore_stopped_pos = newpos;
                    stop_running(false);
                    return direction_to_command(*move_x, *move_y);
                }
            }
        }

        if (!*move_x && !*move_y)
        {
            // Re-apply the runmode, which allows for continue exploration or
            // proper triggering of lua hooks when running ceases. We don't
            // directly call stop_running() without restoring this because
            // you.running is probably 0, and stop_running() won't notify Lua
            // hooks if you.running == 0.
            you.running = runmode;

            result = _get_non_move_command();
            if (result == CMD_NO_CMD)
                stop_running();
            // If taking stairs, the running destination will no longer be
            // valid on the new level. Reset the running pos so travel will
            // search for a new travel square next turn.
            else if (you.running == RMODE_INTERLEVEL)
                you.running.pos.reset();

            return result;

        }
        else if (you.running.is_explore() && Options.explore_delay > -1)
            delay(Options.explore_delay);
        else if (Options.travel_delay > 0)
            delay(Options.travel_delay);
    }

    if (!you.running)
        return CMD_NO_CMD;

    if (result != CMD_NO_CMD)
        return result; // TODO: apparently unreachable?

    return direction_to_command(*move_x, *move_y);
}

command_type direction_to_command(int x, int y)
{
    if (x == -1 && y == -1) return CMD_MOVE_UP_LEFT;
    if (x == -1 && y ==  0) return CMD_MOVE_LEFT;
    if (x == -1 && y ==  1) return CMD_MOVE_DOWN_LEFT;
    if (x ==  0 && y == -1) return CMD_MOVE_UP;
    if (x ==  0 && y ==  1) return CMD_MOVE_DOWN;
    if (x ==  1 && y == -1) return CMD_MOVE_UP_RIGHT;
    if (x ==  1 && y ==  0) return CMD_MOVE_RIGHT;
    if (x ==  1 && y ==  1) return CMD_MOVE_DOWN_RIGHT;

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
            const coord_def p(x, y);
            if (!map_bounds(x, y) || travel_point_distance[x][y])
                continue;

            if (is_exclude_root(p))
                travel_point_distance[x][y] = PD_EXCLUDED;
            else if (is_excluded(p) && env.map_knowledge(p).known())
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
      ignore_danger(false), annotate_map(false), ls(nullptr),
      need_for_greed(false), autopickup(false),
      unexplored_place(), greedy_place(), unexplored_dist(0), greedy_dist(0),
      refdist(nullptr), reseed_points(), features(nullptr), unreachables(),
      point_distance(travel_point_distance), next_iter_points(0),
      traveled_distance(0), circ_index(0)
{
}

travel_pathfind::~travel_pathfind()
{
}

static bool _is_greed_inducing_square(const LevelStashes *ls,
                                      const coord_def &c, bool autopickup)
{
    return ls && ls->needs_visit(c, autopickup);
}

bool travel_pathfind::is_greed_inducing_square(const coord_def &c) const
{
    return _is_greed_inducing_square(ls, c, autopickup);
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

void travel_pathfind::set_feature_vector(vector<coord_def> *feats)
{
    features = feats;

    if (features)
    {
        double_flood = true;
        annotate_map = true;
    }
}

const coord_def travel_pathfind::explore_target() const
{
    if (unexplored_dist != UNFOUND_DIST && greedy_dist != UNFOUND_DIST)
    {
        return unexplored_dist < greedy_dist ? unexplored_place
                                             : greedy_place;
    }
    else if (unexplored_dist != UNFOUND_DIST)
        return unexplored_place;
    else if (greedy_dist != UNFOUND_DIST)
        return greedy_place;

    return coord_def(0, 0);
}

// The travel algorithm is based on the NetHack travel code written by Warwick
// Allison - used with his permission.
coord_def travel_pathfind::pathfind(run_mode_type rmode, bool fallback_explore)
{
    unwind_bool saved_ipt(ignore_player_traversability);

    if (rmode == RMODE_INTERLEVEL)
        rmode = RMODE_TRAVEL;

    runmode = rmode;

    try_fallback = fallback_explore;

    if (runmode == RMODE_CONNECTIVITY)
        ignore_player_traversability = true;
    else
    {
        ASSERTM(crawl_state.need_save,
                "Pathfind with mode %d without a game?", runmode);

        if (runmode == RMODE_EXPLORE_GREEDY)
        {
            autopickup = can_autopickup();
            need_for_greed = autopickup;
        }
    }

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

    refdist = (runmode == RMODE_CONNECTIVITY || Options.explore_item_greed > 0)
                ? &unexplored_dist : &greedy_dist;

    // Zap out previous distances array: this must happen before the
    // early exit checks below, since callers may want to inspect
    // point_distance after this call returns.
    //
    // point_distance will hold the distance of all points from the starting
    // point, i.e. the distance travelled to get there.
    memset(point_distance, 0, sizeof(travel_distance_grid_t));

    if (!in_bounds(start))
        return coord_def();

    // Abort run if we're trying to go someplace evil. Travel to traps is
    // specifically allowed here if the player insists on it.
    if (!floodout
        && !is_travelsafe_square(start, false, ignore_danger, true)
        && !is_trap(start))          // player likes pain
    {
        return coord_def();
    }

    // Nothing to do?
    if (!floodout && start == dest)
        return start;

    unwind_bool slime_wall_check(g_Slime_Wall_Check,
                                 !actor_slime_wall_immune(&you));
    unwind_slime_wall_precomputer slime_neighbours(g_Slime_Wall_Check);

    // How many points we'll consider next iteration.
    next_iter_points = 0;

    // How far we've travelled from (start_x, start_y), in moves (a diagonal move
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

    bool found_target = false;

    for (int points = 1; points > 0; ++traveled_distance,
        circ_index = !circ_index, points = next_iter_points,
        next_iter_points = 0)
    {
        for (int i = 0; i < points; ++i)
        {
            // Look at all neighbours of the current grid.
            // path_examine_point() returns true if the target is reached
            // and marked as such.
            if (path_examine_point(circumference[circ_index][i]))
            {
                if (runmode == RMODE_TRAVEL)
                    return next_travel_move;
                else if (runmode == RMODE_CONNECTIVITY
                         || !Options.explore_wall_bias)
                {
                    return explore_target();
                }
                else
                    found_target = true;
            }
        }

        // Handle exploration with wall bias
        if (next_iter_points == 0 && found_target)
            return explore_target();

        // If there are no more points to look at, we're done, but we did
        // not find a path to our target.
        if (next_iter_points == 0)
        {
            // Don't reseed unless we've found no target for explore, OR
            // we're doing map annotation or feature tracking.
            if ((runmode == RMODE_EXPLORE || runmode == RMODE_EXPLORE_GREEDY
                 || runmode == RMODE_CONNECTIVITY)
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

                next_iter_points  = reseed_points.size();
                ignore_hostile    = true;
            }
        }
    } // for (; points > 0 ...

    if (features && floodout)
    {
        for (const auto &entry : curr_excludes)
        {
            const travel_exclude &exc = entry.second;
            // An exclude - wherever it is - is always a feature.
            if (find(features->begin(), features->end(), exc.pos)
                    == features->end())
            {
                features->push_back(exc.pos);
            }

            _fill_exclude_radius(exc);
        }
    }

    return runmode == RMODE_TRAVEL ? next_travel_move
                                   : explore_target();
}

void travel_pathfind::get_features()
{
    ASSERT(features);

    if (!ls && (annotate_map || need_for_greed))
        ls = StashTrack.find_current_level();

    memset(point_distance, 0, sizeof(travel_distance_grid_t));

    coord_def dc;
    for (dc.x = X_BOUND_1; dc.x <= X_BOUND_2; ++dc.x)
        for (dc.y = Y_BOUND_1; dc.y <= Y_BOUND_2; ++dc.y)
        {
            const dungeon_feature_type feature = env.map_knowledge(dc).feat();

            if ((feature != DNGN_FLOOR
                    && !feat_is_water(feature)
                    && feature != DNGN_LAVA)
                || is_waypoint(dc)
                || is_stash(ls, dc)
                || is_trap(dc))
            {
                features->push_back(dc);
            }
        }

    for (const auto &entry : curr_excludes)
    {
        const travel_exclude &exc = entry.second;

        // An exclude - wherever it is - is always a feature.
        if (find(features->begin(), features->end(), exc.pos) == features->end())
            features->push_back(exc.pos);

        _fill_exclude_radius(exc);
    }
}

bool travel_pathfind::square_slows_movement(const coord_def &c)
{
    // c is a known (explored) location - we never put unknown points in the
    // circumference vector, so we don't need to examine the map array, just the
    // grid array.
    const dungeon_feature_type feature = env.map_knowledge(c).feat();

    // If this is a feature that'll take time to travel past, we simulate that
    // extra turn by taking this feature next turn, thereby artificially
    // increasing traveled_distance.
    //
    // Walking through shallow water and opening closed doors is considered to
    // have the cost of two normal moves for travel purposes.
    const int feat_cost = _feature_traverse_cost(feature);
    if (feat_cost > 1
        && point_distance[c.x][c.y] > traveled_distance - feat_cost)
    {
        circumference[!circ_index][next_iter_points++] = c;
        return true;
    }

    return false;
}

void travel_pathfind::check_square_greed(const coord_def &c)
{
    if (greedy_dist == UNFOUND_DIST
        && is_greed_inducing_square(c)
        && is_travelsafe_square(c, ignore_hostile, ignore_danger))
    {
        int dist = traveled_distance;

        // Penalize distance for negative explore_item_greed
        if (Options.explore_item_greed < 0)
            dist -= Options.explore_item_greed;

        // The addition of explore_wall_bias makes items as interesting
        // as a room's perimeter (with one of four known adjacent walls).
        // XX why?
        if (Options.explore_wall_bias > 0)
            dist += Options.explore_wall_bias * 3;

        greedy_dist = dist;
        greedy_place = c;
    }
}

bool travel_pathfind::path_flood(const coord_def &c, const coord_def &dc)
{
    if (!in_bounds(dc) || unreachables.count(dc))
        return false;

    if (floodout
        && (runmode == RMODE_EXPLORE || runmode == RMODE_EXPLORE_GREEDY))
    {
        if (!env.map_knowledge(dc).seen())
        {
            if (ignore_hostile && !player_in_branch(BRANCH_SHOALS))
            {
                // This point is unexplored but unreachable. Let's find a
                // place from where we can see it.
                for (radius_iterator ri(dc, LOS_DEFAULT, true); ri; ++ri)
                {
                    const int dist = point_distance[ri->x][ri->y];
                    if (dist > 0
                        && (dist < unexplored_dist || unexplored_dist < 0))
                    {
                        unexplored_dist = dist;
                        unexplored_place = *ri;
                    }

                    // We can't do better than that.
                    if (unexplored_dist == 1)
                    {
                        _set_target_square(unexplored_place);
                        return true;
                    }
                }

                // We can't even see the place.
                // Let's store it and look for another.
                if (unexplored_dist < 0)
                    unreachables.insert(dc);
                else
                    _set_target_square(unexplored_place);
            }
            else
            {
                // Found explore target!
                int dist = traveled_distance;

                if (need_for_greed && Options.explore_item_greed > 0)
                {
                    // Penalize distance to favour item pickup
                    dist += Options.explore_item_greed;
                }

                if (Options.explore_wall_bias)
                {
                    // if we are penalizing open space, introduce a default
                    // penalty of 4 walls and then downweight that. If we are
                    // penalizing walls, just add on for each wall.
                    if (Options.explore_wall_bias > 0)
                        dist += Options.explore_wall_bias * 8;

                    // Favour squares directly adjacent to walls
                    // XX for some reason, historically this only looked at
                    // cardinal directions. Why?
                    for (int dir = 0; dir < 8; dir++)
                    {
                        const coord_def ddc = dc + Compass[dir];

                        if (feat_is_wall(env.map_knowledge(ddc).feat()))
                            dist -= Options.explore_wall_bias;
                    }

                    if (Options.explore_wall_bias < 0 &&
                        feat_is_wall(env.map_knowledge(dc).feat()))
                    {
                        // further penalize cases where the unseen square dc
                        // is itself a wall
                        dist -= Options.explore_wall_bias;
                    }
                }

                // Replace old target if nearer (or less penalized)
                // don't let dist get < 0
                if (dist < unexplored_dist || unexplored_dist < 0)
                {
                    unexplored_dist = dist;
                    // somewhat confusing: `c` is probably actually explored,
                    // but is adjacent to the good place we just found.
                    unexplored_place = c;
                }
            }
        }

        // Short-circuit if we can. If traveled_distance (the current
        // distance from the centre of the floodfill) is greater
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
            return true;
    }

    // We don't want to follow the transporter at c if it's excluded. We also
    // don't want to update point_distance for the destination based on
    // taking this transporter.
    if (!ignore_danger
        && is_excluded(c)
        && env.map_knowledge(c).feat() == DNGN_TRANSPORTER
        // We have to actually take the transporter to go from c to dc.
        && !adjacent(c, dc))
    {
        return false;
    }
    else if (dc == dest)
    {
        // Hallelujah, we're home!
        if (_is_safe_move(c))
            next_travel_move = c;

        return true;
    }
    else if (!is_travelsafe_square(dc, ignore_hostile, ignore_danger, try_fallback))
    {
        // This point is not okay to travel on, but if this is a
        // trap, we'll want to put it on the feature vector anyway.
        if (_is_reseedable(dc, ignore_danger)
            && !point_distance[dc.x][dc.y]
            && dc != start)
        {
            if (features && (is_trap(dc) || is_exclude_root(dc))
                && find(features->begin(), features->end(), dc)
                   == features->end())
            {
                features->push_back(dc);
            }

            if (double_flood)
                reseed_points.push_back(dc);

            // Appropriate mystic number. Nobody else should check
            // this number, since this square is unsafe for travel.
            point_distance[dc.x][dc.y] =
                is_exclude_root(dc)   ? PD_EXCLUDED :
                is_excluded(dc)       ? PD_EXCLUDED_RADIUS :
                !_is_safe_cloud(dc)   ? PD_CLOUD
                                      : PD_TRAP;
        }
        return false;
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
            dungeon_feature_type feature = env.map_knowledge(dc).feat();

            if (dc != start
                && (feature != DNGN_FLOOR
                       && !feat_is_water(feature)
                       && feature != DNGN_LAVA
                    || is_waypoint(dc)
                    || is_stash(ls, dc))
                && find(features->begin(), features->end(), dc)
                   == features->end())
            {
                features->push_back(dc);
            }
        }

        if (features && dc != start && is_exclude_root(dc)
            && find(features->begin(), features->end(), dc)
               == features->end())
        {
            features->push_back(dc);
        }
    }

    return false;
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
        return true;

    // Greedy explore check should happen on (x,y), not (dx,dy) as for
    // regular explore.
    if (need_for_greed)
        check_square_greed(c);

    return false;
}

// Checks all neighbours of c, adds them to next round's list of points
// - happens in path_flood() - and returns true if one of them turns out
// to be the target; otherwise, false.
bool travel_pathfind::path_examine_point(const coord_def &c)
{
    // If we've run off the map, or are pathfinding from nowhere in particular
    if (!in_bounds(c))
        return false;

    if (point_traverse_delay(c))
        return false;

    bool found_target = false;

    // For each point, we look at all surrounding points. Take them orthogonals
    // first so that the travel path doesn't zigzag all over the map. Note the
    // (dir = 1) is intentional assignment.
    for (int dir = 0; dir < 8; (dir += 2) == 8 && (dir = 1))
        if (path_flood(c, c + Compass[dir]))
            found_target = true;

    // For travel, we want to pathfind through transporters. Floodout mode
    // proceeds from source, so we take transporters, but for determining moves
    // we work in reverse from destination back to source, so we pathfind
    // through the landing sites.
    if (runmode == RMODE_TRAVEL || runmode == RMODE_NOT_RUNNING)
    {
        if (floodout && env.grid(c) == DNGN_TRANSPORTER)
        {
            LevelInfo &li = travel_cache.get_level_info(level_id::current());
            transporter_info *ti = li.get_transporter(c);
            if (ti && ti->destination != INVALID_COORD)
            {
                if (path_flood(c, ti->destination))
                    found_target = true;
            }
        }
        else if (!floodout && env.grid(c) == DNGN_TRANSPORTER_LANDING)
        {
            LevelInfo &li = travel_cache.get_level_info(level_id::current());
            vector<transporter_info> transporters = li.get_transporters();
            for (auto ti : transporters)
            {
                if (ti.destination == c)
                    if (path_flood(c, ti.position))
                         found_target = true;
            }
        }
    }

    return found_target;
}

// Determines if the level is fully explored.
// This uses data provided by pathfind(), so that needs to be called first.
int travel_pathfind::explore_status()
{
    int status = 0;

    const coord_def greed = greedy_place;
    if (greed.x || greed.y)
        status |= EST_GREED_UNFULFILLED;

    const coord_def unexplored = unexplored_place;
    if (unexplored.x || unexplored.y || !unreachables.empty())
        status |= EST_PARTLY_EXPLORED;

    return status;
}


/**
 * Run the travel_pathfind algorithm, from the given position in floodout mode
 * to populate travel_point_distance relative to that starting point.
 *
 * @param      youpos The starting position.
 * @param[in]  features A vector of features to give to travel_pathfind.
 */
void fill_travel_point_distance(const coord_def& youpos,
                                vector<coord_def>* features)
{
    travel_pathfind tp;
    tp.set_floodseed(youpos);
    tp.set_feature_vector(features);

    // Calling pathfind() twice like this changes the order of *features, but
    // has no effect on travel_point_distance.
    if (features)
        tp.pathfind(RMODE_NOT_RUNNING, false);
    tp.pathfind(RMODE_NOT_RUNNING, true);
}

extern map<branch_type, set<level_id> > stair_level;

static void _find_parent_branch(branch_type br, branch_type *pb, int *pd)
{
    *pb = parent_branch(br);   // Check depth before using *pb.

    if (auto levels = map_find(stair_level, br))
    {
        if (levels->size() > 0)
        {
            *pd = levels->begin()->depth;
            return;
        }
    }
    *pd = 0;
}

// Appends the passed in branch/depth to the given vector, then attempts to
// repeat the operation with the parent branch of the given branch.
//
// As an example of what it does, assume this dungeon structure
//   Stairs to lair on D:11
//   Stairs to snake pit on lair:5
//
// If level 3 of the snake pit is the level we want to track back from,
// we'd call _trackback(vec, BRANCH_SNAKE, 3), and the resulting vector will
// look like:
// { BRANCH_SNAKE, 3 }, { BRANCH_LAIR, 5 }, { BRANCH_DUNGEON, 11 }
// (Assuming, of course, that the vector started out empty.)
//
static void _trackback(vector<level_id> &vec, branch_type branch, int subdepth)
{
    if (subdepth < 1)
        return;
    ASSERT(subdepth <= 27);

    vec.emplace_back(branch, subdepth);

    if (branch != root_branch)
    {
        branch_type pb;
        int pd;
        _find_parent_branch(branch, &pb, &pd);
        if (pd)
            _trackback(vec, pb, pd);
    }
}

static void _track_intersect(vector<level_id> &cur, vector<level_id> &targ,
                             level_id *cx)
{
    cx->branch = BRANCH_DUNGEON;
    cx->depth  = -1;

    int us = int(cur.size()) - 1, them = int(targ.size()) - 1;

    for (; us >= 0 && them >= 0; us--, them--)
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
    if (first == second)
        return 0;

    vector<level_id> fv, sv;

    // If in the same branch, easy.
    if (first.branch == second.branch)
        return abs(first.depth - second.depth);

    // Figure out the dungeon structure between the two levels.
    _trackback(fv, first.branch, first.depth);
    _trackback(sv, second.branch, second.depth);

    level_id intersect;
    _track_intersect(fv, sv, &intersect);

    if (intersect.depth == -1)          // No common ground?
        return -1;

    int distance = 0;
    // If the common branch is not the same as the current branch, we'll
    // have to walk up the branch tree until we get to the common branch.
    while (first.branch != intersect.branch)
    {
        distance += first.depth;

        _find_parent_branch(first.branch, &first.branch, &first.depth);
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

static string _get_trans_travel_dest(const level_pos &target,
                                     bool skip_branch = false,
                                     bool skip_coord = false)
{
    const int branch_id = target.id.branch;
    const char *branch = branches[branch_id].abbrevname;

    if (!branch)
        return "";

    ostringstream dest;

    if (!skip_branch)
        dest << branch;
    if (brdepth[branch_id] != 1)
    {
        if (!skip_branch)
            dest << ":";
        dest << target.id.depth;
    }
    if (target.pos.x != -1 && !skip_coord)
        dest << " @ (x,y)";

    return dest.str();
}

// Returns the level on the given branch that's closest to the player's
// current location.
static int _get_nearest_level_depth(uint8_t branch)
{
    int depth = 1;

    // Hell needs special treatment, because we can't walk up
    // Hell and its branches to the main dungeon.
    if (branch == BRANCH_DEPTHS && is_hell_branch(you.where_are_you))
    {
        // BUG: hell gates in the Lair
        return brentry[BRANCH_VESTIBULE].depth;
    }

    level_id id = level_id::current();
    do
    {
        _find_parent_branch(id.branch, &id.branch, &id.depth);
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
bool is_known_branch_id(branch_type branch)
{
    // The main dungeon is always known
    if (branch == root_branch)
        return true;

    // If we're in the branch, it darn well is known.
    if (player_in_branch(branch))
        return true;

    // The Vestibule is special: there are no stairs to it, just a
    // portal.
    if (branch == BRANCH_VESTIBULE)
        return overview_knows_portal(branch);

    // Guaranteed portal vault, don't show in interlevel travel.
    if (branch == BRANCH_ZIGGURAT)
        return false;

    // If the overview knows the stairs to this branch, we know the branch.
    return stair_level.find(static_cast<branch_type>(branch))
           != stair_level.end() && stair_level[branch].size();
}

static bool _is_known_branch(const Branch &br)
{
    return is_known_branch_id(br.id);
}

// Returns a list of the branches that the player knows the location of the
// stairs to, in the same order as dgn-overview.cc lists them.
static vector<branch_type> _get_branches(bool (*selector)(const Branch &))
{
    vector<branch_type> result;

    for (branch_iterator it; it; ++it)
        if (selector(**it))
            result.push_back(it->id);

    return result;
}

static bool _is_valid_branch(const Branch &br)
{
    return br.shortname != nullptr
        && brdepth[br.id] != -1
        && !branch_is_unfinished(br.id);
}

class TravelPromptMenu : public PromptMenu
{
public:
    enum class Mode { normal, waypoints, altars };

    TravelPromptMenu(int _prompt_flags)
        : PromptMenu(), result(ID_CANCEL), prompt_flags(_prompt_flags),
          travel_mode(Mode::normal)
    {
        prompt_branches = _get_branches((prompt_flags & TPF_SHOW_ALL_BRANCHES)
                ? _is_valid_branch : _is_known_branch);

        tag = "travel";
        if ((prompt_flags & TPF_REMEMBER_TARGET) && !trans_travel_dest.empty())
            def_target = level_target;

        populate_menu();
    }

    bool mode_allowed(Mode m) const
    {
        switch (m)
        {
        case Mode::normal:
            return true;
        case Mode::waypoints:
            return !(prompt_flags & TPF_SHOW_ALL_BRANCHES)
                && (prompt_flags & TPF_ALLOW_WAYPOINTS)
                && travel_cache.get_waypoint_count();
        case Mode::altars:
            return !(prompt_flags & TPF_SHOW_ALL_BRANCHES) && seen_altars();
        }
        return false;
    }

    bool has_default_target() const
    {
        // weirdness: for historical reasons, we use this static string to
        // indicate validity. Sometimes only a branch is set on the travel
        // dest? I'm not entirely sure what's going on here.
        return (prompt_flags & TPF_REMEMBER_TARGET)
            && !trans_travel_dest.empty()
            && def_target.id.branch >= 0;
    }

    void set_waypoint_result(int waypoint)
    {
        if (travel_cache.is_valid_waypoint(waypoint))
            result = ID_FIRST_UI_WAYPOINT - waypoint; // sigh
        else
            result = ID_CANCEL;
    }

    void refresh_prompt()
    {
        vector<string> segs;

        // given limited space, be selective about what is actually shown
        // in the prompt itself
        // XX convert this to keyhints in a more?
        if (travel_mode == Mode::altars)
            segs.emplace_back("_ - list branches");
        else if (travel_mode == Mode::waypoints)
            segs.emplace_back("* - list branches");
        else if (mode_allowed(Mode::waypoints))
            segs.emplace_back("* - list waypoints");

        if (has_default_target())
        {
            segs.push_back(make_stringf("%s - %s",
                    (in_prompt_mode || !is_set(MF_ARROWS_SELECT))
                        ? "Tab/Enter" : "Tab",
                    _get_trans_travel_dest(def_target).c_str()));
        }

        if (!(prompt_flags & TPF_SHOW_ALL_BRANCHES))
            segs.emplace_back("? - help");

        string shortcuts;
        if (!segs.empty())
        {
            shortcuts = "(";
            shortcuts += comma_separated_line(segs.begin(), segs.end(),
                                              ", ", ", ");
            shortcuts += ") ";
        }
        set_title(make_stringf("Where to? %s", shortcuts.c_str()));
    }

    bool seen_altars() const
    {
        extern map<level_pos, god_type> altars_present;

        return !altars_present.empty();
    }

    array<level_pos, NUM_GODS> find_nearest_altars() const
    {
        array<level_pos, NUM_GODS> nearest_altars;

        extern map<level_pos, god_type> altars_present;
        const level_id curr = level_id::current();

        for (const auto &entry : altars_present)
        {
            // This is necessary because faded altars (i.e., GOD_ECUMENICAL)
            // are also recorded in altars_present
            // XX travel to faded altar?
            if (entry.second >= NUM_GODS)
                continue;

            int dist = level_distance(curr, entry.first.id);
            if (dist == -1)
                continue;

            level_pos &best = nearest_altars[entry.second];
            int old_dist = best.id.is_valid()
                ? level_distance(curr, best.id)
                : INT_MAX;

            if (dist < old_dist)
                best = entry.first;
        }
        return nearest_altars;
    }

    void populate_menu()
    {
        clear();
        refresh_prompt();
        int def_choice = 0;

        if (travel_mode == Mode::waypoints)
        {
            vector<string> wdescs = travel_cache.get_waypoint_descs();
            // if this ever shows >10 waypoints, hotkeys would need some
            // work
            for (int i = 0; i < min(10, static_cast<int>(wdescs.size())); i++)
            {
                if (wdescs[i].size() == 0)
                    continue;
                MenuEntry *wp_entry = new MenuEntry(wdescs[i], '0' + i,
                    [this,&i](const MenuEntry&)
                    {
                        set_waypoint_result(i);
                        return false;
                    });
                add_entry(wp_entry);
            }
        }
        else if (travel_mode == Mode::altars)
        {
            const auto nearest_altars = find_nearest_altars();

            // list gods in the same order as dgn-overview.cc lists them.
            vector<god_type> god_list = temple_god_list();
            vector<god_type> nt_god_list = nontemple_god_list();
            god_list.insert(god_list.end(), nt_god_list.begin(), nt_god_list.end());

            for (const god_type god : god_list)
            {
                if (!nearest_altars[god].is_valid() || is_unavailable_god(god))
                    continue;

                // "The Shining One" is too long for the prompt mode version of
                // this to keep 4 columns, which will push it into too many
                // lines. XX: may be better to force menu for this case? (Or
                // remove the mode?)
                string altar_name;
                if (in_prompt_mode && god == GOD_SHINING_ONE)
                    altar_name = "TSO";
                else
                    altar_name  = god_name(god);
                char god_initial = god == GOD_SHINING_ONE ? '1'   : altar_name.at(0);

                level_pos altar_target = nearest_altars[god];

                // XX this menu would be more useful if it showed the target
                // branch
                MenuEntry *altar_entry = new MenuEntry(altar_name, god_initial,
                    [this, altar_target](const MenuEntry&)
                    {
                        result = ID_ALTAR;
                        altar_result = altar_target;
                        return false;
                    });
                // why do we keep the uppercase around? not sure it matters much
                const auto shortcut = tolower_safe(god_initial);
                if (shortcut != god_initial)
                    altar_entry->add_hotkey(shortcut);

                add_entry(altar_entry);
            }
        }
        else // Mode::normal
        {
            for (const branch_type &br : prompt_branches)
            {
                MenuEntry *br_entry = new MenuEntry(branches[br].shortname,
                    branches[br].travel_shortcut,
                    [this,&br](const MenuEntry&)
                    {
                        result = br;
                        return false;
                    });
                // branch shortcuts are stored in uppercase, allow the lowercase form
                // as well. We show the uppercase version as the menu key, though.
                const auto shortcut = tolower_safe(branches[br].travel_shortcut);
                if (shortcut != branches[br].travel_shortcut)
                    br_entry->add_hotkey(shortcut);
                add_entry(br_entry);
                if (has_default_target() && def_target.id.branch == br)
                    def_choice = items.size() - 1;
            }
        }
        update_menu(true);
        if (def_choice >= 0)
            set_hovered(def_choice);
    }

    vector<MenuEntry *> show_in_msgpane() override
    {
        // ensure the prompt is correct for msgpane mode. (Is there a cleaner
        // way to do this?)
        refresh_prompt();
        return PromptMenu::show_in_msgpane();
    }

    bool skip_process_command(int keyin) override
    {
        if (keyin == '!')
            return true; // Gauntlet travel hotkey
        return Menu::skip_process_command(keyin);
    }

    bool process_key(int keyin) override
    {
        const bool allow_updown = (prompt_flags & TPF_ALLOW_UPDOWN);
        const level_id curr = level_id::current();
        // XX a lot of these don't do much mode checking, e.g. they work in
        // wizmode &~
        switch (keyin)
        {
        case '?':
            // standard help doesn't make much sense for this version
            if (prompt_flags & TPF_SHOW_ALL_BRANCHES)
                return true;
            if (travel_mode == Mode::altars)
                show_interlevel_travel_altar_help();
            else
                show_interlevel_travel_branch_help();
            return true;
        case '_':
            if (!mode_allowed(Mode::altars))
                return true;
            travel_mode = travel_mode == Mode::altars ? Mode::normal : Mode::altars;
            populate_menu();
            return true;
        case '\n': case '\r':
            if (ui_is_initialized() && is_set(MF_ARROWS_SELECT))
                break; // awkwardness: this shortcut doesn't work in normal menu form
        case '\t':
            if (has_default_target())
            {
                result = ID_REPEAT;
                return false;
            }
            break;
        case '<':
            result = allow_updown ? ID_UP : ID_CANCEL;
            return false;
        case '>':
            result = allow_updown ? ID_DOWN : ID_CANCEL;
            return false;
        case CONTROL('P'):
            {
                const branch_type parent = parent_branch(curr.branch);
                if (parent < NUM_BRANCHES)
                {
                    result = parent;
                    return false;
                }
            }
            break;
        case '.':
            result = curr.branch;
            return false;
        case '*':
            if (!mode_allowed(Mode::waypoints))
                return true;
            travel_mode = travel_mode == Mode::waypoints ? Mode::normal : Mode::waypoints;
            populate_menu();
            return true;
        }

        // in order to let waypoint hotkeys work in both modes, we short-circuit
        // the superclass key processing here
        if (mode_allowed(Mode::waypoints) && keyin >= '0' && keyin <= '9')
        {
            set_waypoint_result(keyin - '0');
            if (result != ID_CANCEL)
                return false;
        }

        // a menu exit from here is either a cancel, or it should set a
        // selection
        return PromptMenu::process_key(keyin);
    }

    bool cycle_mode(bool forward=true) override
    {
        vector<Mode> modes = { Mode::normal };
        // somewhat non-general code...
        if (mode_allowed(Mode::waypoints))
            modes.push_back(Mode::waypoints);
        if (mode_allowed(Mode::altars))
            modes.push_back(Mode::altars);

        while (travel_mode != modes.front())
            rotate(modes.begin(), modes.begin() + 1, modes.end());

        if (forward)
            rotate(modes.begin(), modes.begin() + 1, modes.end());
        else
            rotate(modes.rbegin(), modes.rbegin() + 1, modes.rend());

        travel_mode = modes.front();
        populate_menu();
        return true;
    }

    int run()
    {
        result = ID_CANCEL;
        show();
        // result should be set by key processing or by one of the menu
        // on_select actions
        return result;
    }

    int result;
    level_pos altar_result; // XX consolidate?
    int prompt_flags;
    Mode travel_mode;
    level_pos def_target;
    string def_target_name;
    vector<branch_type> prompt_branches;
};

level_id find_up_level(level_id curr, bool up_branch)
{
    --curr.depth;

    if (up_branch)
        curr.depth = 0;

    if (curr.depth < 1)
    {
        if (curr.branch != BRANCH_DUNGEON && curr.branch != root_branch)
        {
            level_id parent;
            _find_parent_branch(curr.branch, &parent.branch, &parent.depth);
            if (parent.depth > 0)
                return parent;
            else if (curr.branch == BRANCH_VESTIBULE)
                return brentry[BRANCH_VESTIBULE];
        }
        return level_id();
    }

    return curr;
}

static level_id _find_up_level()
{
    return find_up_level(level_id::current());
}

level_id find_down_level(level_id curr)
{
    if (curr.depth < brdepth[curr.branch])
        ++curr.depth;
    return curr;
}

level_id find_deepest_explored(level_id curr)
{
    ASSERT(curr.branch != NUM_BRANCHES);

    for (int i = brdepth[curr.branch]; i > 0; --i)
    {
        const level_id lid(curr.branch, i);
        LevelInfo *linf = travel_cache.find_level_info(lid);
        if (linf && !linf->empty())
            return lid;
    }

    // If the player's currently in the same place, report their current
    // level_id if the travel cache hasn't been updated.
    const level_id player_level = level_id::current();
    if (player_level == curr.branch)
        return player_level;

    return curr;
}

bool branch_entered(branch_type branch)
{
    const level_id lid(branch, 1);
    LevelInfo *linf = travel_cache.find_level_info(lid);
    return linf && !linf->empty();
}

static level_id _find_down_level()
{
    return find_down_level(level_id::current());
}

static keyfun_action _travel_depth_keyfilter(int &c)
{
    switch (c)
    {
    case '<': case '>': case '?': case '$': case '^':
        return KEYFUN_BREAK;
    case '-':
    case CONTROL('P'): case 'p':
        c = '-';  // Make uniform.
        return KEYFUN_BREAK;
    default:
        return KEYFUN_PROCESS;
    }
}

static level_pos _find_entrance(const level_pos &from)
{
    level_id lid(from.id);
    coord_def pos(-1, -1);
    branch_type target_branch = lid.branch;

    lid.depth = 1;
    level_id new_lid = find_up_level(lid);

    if (new_lid.is_valid())
    {
        LevelInfo &li = travel_cache.get_level_info(new_lid);
        vector<stair_info> &stairs = li.get_stairs();
        for (const auto &stair : stairs)
            if (stair.destination.id.branch == target_branch)
            {
                pos = stair.position;
                break;
            }
#ifdef DEBUG_TRAVEL
        dprf("found entrance in %d depth %d at %d,%d", new_lid.branch,
            new_lid.depth, pos.x, pos.y);
#endif
        return level_pos(new_lid, pos);
    }
    else
    {
        LevelInfo &li = travel_cache.get_level_info(lid);
        vector<stair_info> &stairs = li.get_stairs();
        for (const auto &stair : stairs)
            if (!stair.destination.id.is_valid())
            {
                pos = stair.position;
                break;
            }
#ifdef DEBUG_TRAVEL
        dprf("found entrance in %d depth %d at %d,%d", lid.branch, lid.depth,
            pos.x, pos.y);
#endif
        return level_pos(lid, pos);
    }
}

/*
 * Given a string and a starting target, find a `level_pos` to travel to.
 *
 * @param s a string consisting of a number representing depth, or 0 to go to
 *          the branch entrance.
 * @param targ an initial default `level_pos` to potentially modify.
 *
 * @return the resulting `level_pos`.
 */
static level_pos _parse_travel_target(string s, const level_pos &targ)
{
    trim_string(s);
    level_pos result(targ);

    if (!s.empty())
    {
        result.id.depth = atoi(s.c_str());
        result.pos.x = result.pos.y = -1;
    }

    if (!result.id.depth)
        result = _find_entrance(result);

    return result;
}

/*
 * Interpret the player's input to the interlevel travel prompt.
 * This input consists either of a numeric string, or one of several special
 * characters that manipulate a destination that can be triggered by enter.
 * This function can process both at the same time, though simple numeric input
 * without a special key is handled separately in `_prompt_travel_depth`.
 *
 * @param munge_method  a non-level special key input at the prompt to process.
 * @param s             a numeric depth input at the prompt to process.
 * @param targ          an input `level_pos` representing the current default
 *                      target.
 *
 * @return a `level_pos` indicating the resulting target, potentially the same
 *                       as `targ`.
 */
static level_pos _travel_depth_munge(int munge_method, const string &s,
                                const level_pos &targ)
{
    level_pos result(targ.id); // drop any coords in `targ`.
    result = _parse_travel_target(s, result);

    switch (munge_method)
    {
    case '?':
        show_interlevel_travel_depth_help();
        redraw_screen();
        update_screen();
        return level_pos(targ); // no change
    case '<':
        result.id = find_up_level(result.id);
        break;
    case '>':
        result.id = find_down_level(result.id);
        break;
    case '-':
        result.id = find_up_level(result.id, true);
        break;
    case '$':
        result.id = find_deepest_explored(result.id);
        break;
    case '^':
        result = _find_entrance(result);
        break;
    }
    if (result.id.depth < 1)
        result.id.depth = 1;
    return result;
}

static level_pos _prompt_travel_depth(const level_id &id, bool remember_targ)
{
    // TODO: incorporate into TravelPromptMenu?
    level_pos target = level_pos(id);

    // Handle one-level branches by not prompting.
    if (single_level_branch(target.id.branch))
        return level_pos(level_id(target.id.branch, 1));

    // if there's a previous target, and we are going to that branch again,
    // use the previous target depth as the default
    if (level_target.id.is_valid() && remember_targ
        && level_target.id.branch == target.id.branch)
    {
        target.id.depth = level_target.id.depth;
    }
    else // otherwise, use the nearest level
        target.id.depth = _get_nearest_level_depth(target.id.branch);

    clear_messages();
    msgwin_temporary_mode temp;

    while (true)
    {
        msgwin_clear_temporary();
        mprf(MSGCH_PROMPT, "What level of %s? "
             "(default %s, ? - help) ",
             branches[target.id.branch].longname,
             _get_trans_travel_dest(target, true).c_str());

        char buf[100];
        const int response =
            cancellable_get_line(buf, sizeof buf, nullptr,
                                 _travel_depth_keyfilter, "", "travel_depth");

        if (!response)
            return _parse_travel_target(buf, target);

        if (key_is_escape(response))
            return level_pos(level_id(BRANCH_DUNGEON, 0));

        target = _travel_depth_munge(response, buf, target);
    }
}

level_pos prompt_translevel_target(int prompt_flags, string& dest_name)
{
    level_pos target;
    const bool remember_targ = (prompt_flags & TPF_REMEMBER_TARGET);
    int menu_result = BRANCH_DUNGEON;

    TravelPromptMenu pm(prompt_flags);
    // Don't kill the prompt even if the only branch we know is the main dungeon
    // This keeps things consistent for the player.
    // XX I can't figure out how the above comment relates to this check..
    if (pm.prompt_branches.size() > 0)
        menu_result = pm.run();

    if (menu_result == ID_CANCEL)
        return target;

    if (menu_result == ID_ALTAR)
        return pm.altar_result;

    // If user chose to repeat last travel, return that.
    if (menu_result == ID_REPEAT)
        return level_target;

    if (menu_result == ID_UP)
    {
        target = _find_up_level();
        if (target.id.depth > 0 && remember_targ)
            dest_name = _get_trans_travel_dest(target);
        return target;
    }

    if (menu_result == ID_DOWN)
    {
        target = _find_down_level();
        if (target.id.depth > 0 && remember_targ)
            dest_name = _get_trans_travel_dest(target);
        return target;
    }

    // XX make less hacky
    if (menu_result < 0) // any other negative val should be a waypoint
    {
        target = travel_cache.get_waypoint(-menu_result - 1);
        if (target.id.depth > 0 && remember_targ)
            dest_name = _get_trans_travel_dest(target);
        return target;
    }

    // otherwise, menu_result is a branch
    target.id.branch = static_cast<branch_type>(menu_result);

    // User's chosen a branch, so now we ask for a level.
    target = _prompt_travel_depth(target.id, remember_targ);

    if (target.id.depth < 1
        || target.id.depth > brdepth[target.id.branch])
    {
        target.id.depth = -1;
    }

    if (target.id.depth > -1 && remember_targ)
        dest_name = _get_trans_travel_dest(target);

#ifdef DEBUG_TRAVEL
    dprf("level target is %d depth %d, pos %d,%d", target.id.branch,
        target.id.branch, target.pos.x, target.pos.y);
#endif

    return target;
}

static void _start_translevel_travel()
{
    // Update information for this level.
    travel_cache.get_level_info(level_id::current()).update();

    if (level_id::current() == level_target.id)
    {
        if (level_target.pos.x == -1 &&
            level_target.id.depth == branches[level_target.id.branch].numlevels)
        {
            mpr("You're already at the bottom of this branch!");
            return;
        }
        else if (level_target.pos.x == -1 || level_target.pos == you.pos())
        {
            mpr("You're already here!");
            return;
        }
    }

#ifdef DEBUG_TRAVEL
    dprf("starting translevel travel, branch %d depth %d, pos %d,%d",
        level_target.id.branch, level_target.id.depth, level_target.pos.x,
        level_target.pos.y);
#endif

    if (level_target.id.depth > 0)
    {
        you.running = RMODE_INTERLEVEL;
        you.running.pos.reset();
        last_stair.id.depth = -1;
        last_stair.pos.reset();

        _Route_Warning.new_dest(level_target);

        _Src_Level = level_id::current();
        _Src_Dest_Level_Delta = level_distance(_Src_Level,
                                               level_target.id);

        _start_running();
    }
}

void start_translevel_travel(const level_pos &pos)
{
    if (!can_travel_to(pos.id))
    {
        if (!can_travel_interlevel())
            mpr("Sorry, you can't auto-travel out of here.");
        else
            mpr("Sorry, I don't know how to get there.");
        return;
    }

    if (pos.is_valid() && !in_bounds(pos.pos))
    {
        mpr("Sorry, I don't know how to get there.");
        return;
    }

    // Remember where we're going so we can easily go back if interrupted.
    you.travel_x = pos.pos.x;
    you.travel_y = pos.pos.y;
    you.travel_z = pos.id;

#ifdef DEBUG_TRAVEL
    dprf("going to %d depth %d, pos %d,%d", pos.id.branch, pos.id.depth,
        pos.pos.x, pos.pos.y);
#endif

    if (!can_travel_interlevel())
    {
        start_travel(pos.pos);
        return;
    }

    level_target = pos;

    // Check that it's level + position, not just level.
    if (pos.is_valid())
    {
        if (pos.id != level_id::current())
        {
            if (!_loadlev_populate_stair_distances(pos))
            {
                mpr("Level memory is imperfect, aborting.");
                return ;
            }
        }
        else
            _populate_stair_distances(pos);
    }

    trans_travel_dest = _get_trans_travel_dest(level_target);

    if (!Options.travel_one_unsafe_move && !i_feel_safe(true, true))
        return;
    if (you.confused())
    {
        canned_msg(MSG_TOO_CONFUSED);
        return;
    }

    _start_translevel_travel();
}

static void _start_translevel_travel_prompt()
{
    // Update information for this level. We need it even for the prompts, so
    // we can't wait to confirm that the user chose to initiate travel.
    travel_cache.get_level_info(level_id::current()).update();

    level_pos target = prompt_translevel_target(TPF_DEFAULT_OPTIONS,
            trans_travel_dest);
    if (target.id.depth <= 0)
    {
        canned_msg(MSG_OK);
        return;
    }

    start_translevel_travel(target);
}

static int _target_distance_from(const coord_def &pos)
{
    for (const stair_info &stair : curr_stairs)
        if (stair.position == pos)
            return stair.distance;

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
 *
 * This function has undefined behaviour when the target position is not
 * traversable.
 */
static int _find_transtravel_stair(const level_id &cur,
                                    const level_pos &target,
                                    int distance,
                                    // This is actually the current position
                                    // on cur, not necessarily a stair.
                                    const coord_def &stair,
                                    level_id &closest_level,
                                    int &best_level_distance,
                                    coord_def &best_stair,
                                    int search_depth = 0)
{
    int local_distance = -1;
    level_id player_level = level_id::current();

    LevelInfo &li = travel_cache.get_level_info(cur);

    // Have we reached the target level?
    if (cur == target.id)
    {
        // Are we in an exclude? If so, bail out. Unless it is just a stair exclusion.
        if (is_excluded(stair, li.get_excludes()) && !is_stair_exclusion(stair))
            return -1;

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
            // _populate_stair_distances. Assuming we're not on stairs, that
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

    vector<stair_info> &stairs = li.get_stairs();

    // this_stair being nullptr is perfectly acceptable, since we start with
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

    for (stair_info &si : stairs)
    {
        if (stairs_destination_is_excluded(si))
            continue;

        // Skip placeholders and excluded stairs.
        if (!si.can_travel() || is_excluded(si.position, li.get_excludes()))
            continue;

        int deltadist = li.distance_between(this_stair, &si);

        if (!this_stair)
        {
            deltadist = travel_point_distance[si.position.x][si.position.y];
            if (!deltadist && you.pos() != si.position)
                deltadist = -1;
        }
        // deltadist == 0 is legal (if this_stair is nullptr), since the player
        // may be standing on the stairs. If two stairs are disconnected,
        // deltadist has to be negative.
        if (deltadist < 0)
            continue;

        int dist2stair = distance + deltadist;
        if (si.distance == -1 || si.distance > dist2stair)
        {
            si.distance = dist2stair;

            // Account for the cost of taking the stairs
            dist2stair += 500; // XXX: this seems large?

            // Already too expensive? Short-circuit.
            if (local_distance != -1 && dist2stair >= local_distance)
                continue;

            const level_pos &dest = si.destination;

            // Never use escape hatches as the last leg of the trip, since
            // that will leave the player unable to retrace their path.
            // This does not apply if we have a destination with a specific
            // position on the target level travel wants to get to.
            if (feat_is_escape_hatch(si.grid)
                && target.pos.x == -1
                && dest.id == target.id)
            {
                continue;
            }

            // We can only short-circuit the stair-following process if we
            // have no exact target location. If there *is* an exact target
            // location, we can't follow stairs for which we have incomplete
            // information.
            if (target.pos.x == -1
                && dest.id == target.id)
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

            // Don't try hell branches if we are not already in one or targeting
            // one. When you actually enter the vestibule, the branch entry
            // point is adjusted to be the portal you entered through, but
            // autotravel needs to simulate this somehow, or it can find (fake)
            // paths through hell that are shortcuts in depths, because the
            // vestibule side of the portals do map to particular portals
            // scattered throughout depths, even if those mappings won't be
            // used while exiting from the vestibule.
            if (is_hell_branch(dest.id.branch)
                            && !(is_hell_branch(target.id.branch)
                                 || is_hell_branch(cur.branch)))
            {
                continue;
            }

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
#ifdef DEBUG_TRAVEL
            string indent(search_depth, '.');
            dprf("%strying stairs at %d,%d, dest is %d depth %d, pos %d,%d",
                indent.c_str(), si.position.x, si.position.y, dest.id.branch,
                dest.id.depth, dest.pos.x, dest.pos.y);
#endif

            // Okay, take these stairs and keep going.
            const int newdist =
                _find_transtravel_stair(dest.id, target,
                                        dist2stair, dest.pos, closest_level,
                                        best_level_distance, best_stair,
                                        search_depth + 1);
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
    level_excursion excursion;
    excursion.go_to(target.id);
    _populate_stair_distances(target);
    return true;
}

static void _populate_stair_distances(const level_pos &target)
{
    // Populate travel_point_distance.
    fill_travel_point_distance(target.pos);

    curr_stairs.clear();
    for (stair_info si : travel_cache.get_level_info(target.id).get_stairs())
    {
        si.distance = travel_point_distance[si.position.x][si.position.y];
        if (!si.distance && target.pos != si.position
            || si.distance < -1)
        {
            si.distance = -1;
        }

        curr_stairs.push_back(si);
    }
}

static coord_def _find_closest_adj(coord_def targ)
{
    coord_def closest_pos = coord_def(0,0);
    int closest_dist = INT_MAX;
    for (adjacent_iterator ai(targ); ai; ++ai)
    {
        const int dist = travel_point_distance[ai->x][ai->y];
        if (dist > 0 && dist < closest_dist)
        {
            closest_pos = *ai;
            closest_dist = dist;
        }
    }
    return closest_pos;
}

static bool _find_transtravel_square(const level_pos &target, bool verbose)
{
    level_id current = level_id::current();

    coord_def best_stair(-1, -1);
    coord_def cur_stair(you.pos());

    level_id closest_level;
    int best_level_distance = -1;
    travel_cache.clear_distances();

    fill_travel_point_distance(you.pos());

    // either off-level, or traversable and on-level
    // TODO: actually check this when the square is off-level? The current
    // behaviour is that it will go to the level and then fail.
    const bool maybe_traversable = (target.id != current
                                    || (in_bounds(target.pos)
                                        && feat_is_traversable_now(env.map_knowledge(target.pos).feat())));

    if (maybe_traversable)
    {
        _find_transtravel_stair(current, target,
                                0, cur_stair, closest_level,
                                best_level_distance, best_stair);
        dprf("found stair at %d,%d", best_stair.x, best_stair.y);
    }
    // even without _find_transtravel_stair called, the values are initialized
    // enough for the rest of this to go forward.

    if (best_stair.x != -1 && best_stair.y != -1)
    {
        // Is this stair going offlevel?
        if ((level_target.id != current
             || level_target.pos != best_stair)
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
                                    level_target.id)
                        || _Src_Dest_Level_Delta <
                           level_distance(dest_stair->destination.id,
                                          _Src_Level))
                    && !_Route_Warning.warn_continue_travel(
                        level_target,
                        dest_stair->destination.id))
                {
                    canned_msg(MSG_OK);
                    return false;
                }
            }
        }

        you.running.pos = best_stair;
        return true;
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

    if (!verbose)
        return false;
    if (target.id == current && (target.pos.x == -1 || target.pos == you.pos()))
        return false;
    if (maybe_traversable)
    {
        mpr("Sorry, I don't know how to get there.");
        return false;
    }

    // check if there is a spot adjacent to the place that is reachable
    coord_def closest_alt = _find_closest_adj(target.pos);
    if (closest_alt.origin())
    {
        mpr("Sorry, I don't know how to traverse that place.");
        return false;
    }

    level_pos new_target = target;
    new_target.pos = closest_alt;
    return _find_transtravel_square(new_target, verbose);
}

void start_travel(const coord_def& p)
{
    // Redundant target?
    if (p == you.pos())
        return;

    // Can we even travel to this square?
    if (!in_bounds(p))
        return;

    if (!is_travelsafe_square(p, true))
        return;

    you.travel_x = p.x;
    you.travel_y = p.y;
    you.travel_z = level_id::current();

    you.running.pos = p;
    level_target  = level_pos(level_id::current(), p);

    if (!can_travel_interlevel())
    {
        if (!Options.travel_one_unsafe_move && !i_feel_safe(true, true))
            return;
        if (you.confused())
        {
            canned_msg(MSG_TOO_CONFUSED);
            return;
        }

        // Start running
        you.running = RMODE_TRAVEL;
        _start_running();
    }
    else
        start_translevel_travel(level_target);
}

void start_explore(bool grab_items)
{
    if (Hints.hints_explored)
        Hints.hints_explored = false;

    if (!i_feel_safe(true, true))
        return;

    if (should_fear_zot() && !yesno("Really explore while Zot is near?", false, 'n'))
    {
        canned_msg(MSG_OK);
        return;
    }

    you.running = (grab_items ? RMODE_EXPLORE_GREEDY : RMODE_EXPLORE);

    for (rectangle_iterator ri(0); ri; ++ri)
        if (env.map_knowledge(*ri).seen())
            env.map_seen.set(*ri);

    you.running.pos.reset();
    _start_running();
}

void do_explore_cmd()
{
    if (you.berserk())
        mpr("Calm down first, please.");
    else                        // Start exploring
        start_explore(Options.explore_greedy);
}

//////////////////////////////////////////////////////////////////////////
// Interlevel travel classes

level_id level_id::current()
{
    const level_id id(you.where_are_you, you.depth);
    return id;
}

int level_id::absdepth() const
{
    return absdungeon_depth(branch, depth);
}

level_id level_id::get_next_level_id(const coord_def &pos)
{
    int gridc = env.grid(pos);
    level_id id = current();

    if (gridc == branches[id.branch].exit_stairs)
        return stair_destination(pos);
#if TAG_MAJOR_VERSION == 34
    if (gridc == DNGN_ENTER_PORTAL_VAULT)
        return stair_destination(pos);
#endif
    if (gridc == DNGN_EXIT_THROUGH_ABYSS)
        return level_id(BRANCH_ABYSS, 1);

    for (branch_iterator it; it; ++it)
    {
        if (gridc == it->entry_stairs)
        {
            id.branch = it->id;
            id.depth = 1;
            break;
        }
    }

    switch (gridc)
    {
    case DNGN_STONE_STAIRS_DOWN_I:   case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III: case DNGN_ESCAPE_HATCH_DOWN:
    case DNGN_ABYSSAL_STAIR:
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

string level_id::describe(bool long_name, bool with_number) const
{
    string result = (long_name ? branches[branch].longname
                               : branches[branch].abbrevname);

    if (with_number && brdepth[branch] != 1)
    {
        if (long_name)
        {
            // decapitalise 'the'
            if (starts_with(result, "The"))
                result[0] = 't';
            result = make_stringf("Level %d of %s",
                      depth, result.c_str());
        }
        else if (depth)
            result = make_stringf("%s:%d", result.c_str(), depth);
        else
            result = make_stringf("%s:$", result.c_str());
    }
    return result;
}

level_id level_id::parse_level_id(const string &s)
{
    string::size_type cpos = s.find(':');
    string brname  = (cpos != string::npos? s.substr(0, cpos)  : s);
    string brlev   = (cpos != string::npos? s.substr(cpos + 1) : "");

    branch_type br = branch_by_abbrevname(brname);

    if (br == NUM_BRANCHES)
    {
        throw bad_level_id_f("Invalid branch \"%s\" in spec \"%s\"",
                             brname.c_str(), s.c_str());
    }

    // Branch:$ uses static data -- it never comes from the current game.
    const int dep = (brlev.empty() ? 1 :
                     brlev == "$"  ? branches[br].numlevels
                                   : atoi(brlev.c_str()));

    // The branch might have been longer when the save has been created.
    if (dep < 0 || dep > brdepth[br] && dep > branches[br].numlevels)
    {
        throw bad_level_id_f("Invalid depth for %s in spec \"%s\"",
                             brname.c_str(), s.c_str());
    }

    return level_id(br, dep);
}

void level_id::save(writer& outf) const
{
    marshall_level_id(outf, *this);
}

void level_id::load(reader& inf)
{
    (*this) = unmarshall_level_id(inf);
}

level_pos level_pos::current()
{
    return level_pos(level_id::current(), you.pos());
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
    pos = unmarshallCoord(inf);
}

void transporter_info::save(writer& outf) const
{
    marshallCoord(outf, position);
    marshallCoord(outf, destination);
    marshallByte(outf, type);
}

void stair_info::save(writer& outf) const
{
    marshallCoord(outf, position);
    marshallShort(outf, grid);
    destination.save(outf);
    marshallByte(outf, guessed_pos? 1 : 0);
    marshallByte(outf, type);
}

void transporter_info::load(reader& inf)
{
    position = unmarshallCoord(inf);
    destination = unmarshallCoord(inf);
    type = static_cast<transporter_type>(unmarshallByte(inf));
}

void stair_info::load(reader& inf)
{
    position = unmarshallCoord(inf);
    grid = static_cast<dungeon_feature_type>(unmarshallShort(inf));
    destination.load(inf);
    guessed_pos = unmarshallByte(inf) != 0;
    type = static_cast<stair_type>(unmarshallByte(inf));
}

string stair_info::describe() const
{
    if (destination.is_valid())
    {
        const level_pos &lp(destination);
        return make_stringf(" (-> %s@(%d,%d)%s%s)", lp.id.describe().c_str(),
                             lp.pos.x, lp.pos.y,
                             guessed_pos? " guess" : "",
                             type == PLACEHOLDER ? " placeholder" : "");
    }
    else if (destination.id.is_valid())
        return make_stringf(" (->%s (?))", destination.id.describe().c_str());

    return " (?)";
}

void LevelInfo::set_level_excludes()
{
    curr_excludes = excludes;
    init_exclusion_los();
}

bool LevelInfo::empty() const
{
    return stairs.empty() && excludes.empty();
}

void LevelInfo::update_excludes()
{
    excludes = curr_excludes;
}

void LevelInfo::update()
{
    // First, set excludes, so that stair distances will be correctly populated.
    excludes = curr_excludes;

    // First, we get all known stairs.
    vector<coord_def> stair_positions;
    get_stairs(stair_positions);

    // Make sure our stair list is correct.
    correct_stair_list(stair_positions);

    sync_all_branch_stairs();

    // If the player isn't immune to slimy walls, precalculate
    // neighbours of slimy walls now.
    unwind_slime_wall_precomputer slime_wall_neighbours(
        !actor_slime_wall_immune(&you));
    precompute_travel_safety_grid travel_safety_calc;
    update_stair_distances();

    vector<coord_def> transporter_positions;
    get_transporters(transporter_positions);
    correct_transporter_list(transporter_positions);

    update_daction_counters(this);
}

void LevelInfo::set_distance_between_stairs(int a, int b, int dist)
{
    // Note dist == 0 is illegal because we can't have two stairs on
    // the same square.
    if (dist <= 0 && a != b)
        dist = -1;
    stair_distances[a * stairs.size() + b] = dist;
    stair_distances[b * stairs.size() + a] = dist;
}

void LevelInfo::update_stair_distances()
{
    const int nstairs = stairs.size();
    // Now we update distances for all the stairs, relative to all other
    // stairs.
    for (int s = 0; s < nstairs - 1; ++s)
    {
        set_distance_between_stairs(s, s, 0);

        // For each stair, we need to ask travel to populate the distance
        // array.
        fill_travel_point_distance(stairs[s].position);

        // Assume movement distance between stairs is commutative,
        // i.e. going from a->b is the same distance as b->a.
        for (int other = s + 1; other < nstairs; ++other)
        {
            const coord_def op = stairs[other].position;
            const int dist = travel_point_distance[op.x][op.y];
            set_distance_between_stairs(s, other, dist);
        }
    }
    if (nstairs)
        set_distance_between_stairs(nstairs - 1, nstairs - 1, 0);
}

void LevelInfo::update_transporter(const coord_def& transpos,
                                   const coord_def& dest)
{
    transporter_info *si = get_transporter(transpos);
    if (si)
        si->destination = dest;
    else
        transporters.push_back(transporter_info(transpos, dest,
                               transporter_info::PHYSICAL));
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

        if (!guess && p.id.branch == BRANCH_VESTIBULE
            && id.branch == BRANCH_DEPTHS)
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
    erase_if(stairs, [&dest](const stair_info& old_stair)
                     { return old_stair.type == stair_info::PLACEHOLDER
                              && old_stair.destination == dest; });

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
    set<int> synced;

    for (const stair_info& si : stairs)
    {
        if (si.destination.id.branch != id.branch && si.destination.is_valid()
            && !synced.count(si.grid))
        {
            synced.insert(si.grid);
            sync_branch_stairs(&si);
        }
    }
}

void LevelInfo::sync_branch_stairs(const stair_info *si)
{
    for (stair_info &sother : stairs)
    {
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
    for (stair_info &si : stairs)
    {
        if (si.grid != grid)
            continue;

        si.destination.id.depth = -1;
        si.destination.pos.x = -1;
        si.destination.pos.y = -1;
        si.guessed_pos = true;
    }
}

bool LevelInfo::know_transporter(const coord_def &c) const
{
    const int index = get_transporter_index(c);
    if (index == -1)
        return false;

    return !transporters[index].destination.origin();
}

bool LevelInfo::know_stair(const coord_def &c) const
{
    const int index = get_stair_index(c);
    if (index == -1)
        return false;

    const level_pos &lp = stairs[index].destination;
    return lp.is_valid();
}

transporter_info *LevelInfo::get_transporter(const coord_def &pos)
{
    int index = get_transporter_index(pos);
    return index != -1 ? &transporters[index] : nullptr;
}

stair_info *LevelInfo::get_stair(const coord_def &pos)
{
    int index = get_stair_index(pos);
    return index != -1? &stairs[index] : nullptr;
}

int LevelInfo::get_transporter_index(const coord_def &pos) const
{
    for (int i = static_cast<int>(transporters.size()) - 1; i >= 0; --i)
        if (transporters[i].position == pos)
            return i;

    return -1;
}

int LevelInfo::get_stair_index(const coord_def &pos) const
{
    for (int i = static_cast<int>(stairs.size()) - 1; i >= 0; --i)
        if (stairs[i].position == pos)
            return i;

    return -1;
}

void LevelInfo::correct_stair_list(const vector<coord_def> &s)
{
    stair_distances.clear();

    // Fix up the grid for the placeholder stair.
    for (stair_info &stair : stairs)
        stair.grid = env.grid(stair.position);

    // First we kill any stairs in 'stairs' that aren't there in 's'.
    for (int i = ((int) stairs.size()) - 1; i >= 0; --i)
    {
        if (stairs[i].type == stair_info::PLACEHOLDER)
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
    for (coord_def pos : s)
    {
        int found = -1;
        for (int j = stairs.size() - 1; j >= 0; --j)
        {
            if (pos == stairs[j].position)
            {
                found = j;
                break;
            }
        }

        if (found == -1)
        {
            stair_info si;
            si.position = pos;
            si.grid     = env.grid(si.position);
            si.destination.id = level_id::get_next_level_id(pos);
            if (si.destination.id.branch == BRANCH_VESTIBULE
                && id.branch == BRANCH_DEPTHS
                && travel_hell_entry.is_valid())
            {
                si.destination = travel_hell_entry;
            }
            if (!env.map_knowledge(pos).seen())
                si.type = stair_info::MAPPED;

            // We don't know where on the next level these stairs go to, but
            // that can't be helped. That information will have to be filled
            // in whenever the player takes these stairs.
            stairs.push_back(si);
        }
        else
            stairs[found].type = env.map_knowledge(pos).seen() ? stair_info::PHYSICAL : stair_info::MAPPED;
    }

    resize_stair_distances();
}

void LevelInfo::correct_transporter_list(const vector<coord_def> &t)
{
    // First we kill any transporters in 'transporters' that aren't there in 't'.
    for (int i = ((int) transporters.size()) - 1; i >= 0; --i)
    {
        bool found = false;
        for (int j = t.size() - 1; j >= 0; --j)
        {
            if (t[j] == transporters[i].position)
            {
                found = true;
                break;
            }
        }

        if (!found)
            transporters.erase(transporters.begin() + i);
    }

    // For each transporter in 't', make sure we have a corresponding stair
    // in 'transporters'.
    for (coord_def pos : t)
    {
        int found = -1;
        for (int j = transporters.size() - 1; j >= 0; --j)
        {
            if (pos == transporters[j].position)
            {
                found = j;
                break;
            }
        }

        transporter_info::transporter_type type =
            env.map_knowledge(pos).seen() ? transporter_info::PHYSICAL
                                          : transporter_info::MAPPED;
        if (found == -1)
            transporters.push_back(transporter_info(pos, coord_def(), type));
        else
            transporters[found].type = type;
    }
}

void LevelInfo::resize_stair_distances()
{
    const int nstairs = stairs.size();
    stair_distances.reserve(nstairs * nstairs);
    stair_distances.resize(nstairs * nstairs, 0);
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

void LevelInfo::get_transporters(vector<coord_def> &tr)
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        const dungeon_feature_type feat = env.grid(*ri);

        if (feat == DNGN_TRANSPORTER
            && (*ri == you.pos() || env.map_knowledge(*ri).known())
            && env.map_knowledge(*ri).seen())
        {
            tr.push_back(*ri);
        }
    }
}

void LevelInfo::get_stairs(vector<coord_def> &st)
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        const dungeon_feature_type feat = env.grid(*ri);

        if ((*ri == you.pos() || env.map_knowledge(*ri).known())
            && feat_is_travelable_stair(feat)
            && (env.map_knowledge(*ri).seen() || !_is_branch_stair(*ri)))
        {
            st.push_back(*ri);
        }
    }
}

void LevelInfo::clear_distances()
{
    for (stair_info &stair : stairs)
        stair.clear_distance();
}

bool LevelInfo::is_known_branch(uint8_t branch) const
{
    for (const stair_info &stair : stairs)
        if (stair.destination.id.branch == branch)
            return true;

    return false;
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

    int transporter_count = transporters.size();
    marshallShort(outf, transporter_count);
    for (int i = 0; i < transporter_count; ++i)
        transporters[i].save(outf);

    marshallExcludes(outf, excludes);

    marshallByte(outf, NUM_DACTION_COUNTERS);
    for (int i = 0; i < NUM_DACTION_COUNTERS; i++)
        marshallShort(outf, daction_counters[i]);
}

void LevelInfo::load(reader& inf, int minorVersion)
{
    stairs.clear();
    int stair_count = unmarshallShort(inf);
    for (int i = 0; i < stair_count; ++i)
    {
        stair_info si;
        si.load(inf);
        stairs.push_back(si);

        if (id.branch == BRANCH_DUNGEON
            && si.destination.id.branch == BRANCH_VESTIBULE
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
            stair_distances.push_back(unmarshallShort(inf));
    }

    transporters.clear();
#if TAG_MAJOR_VERSION == 34
    if (minorVersion >= TAG_MINOR_TRANSPORTERS)
    {
#endif
    int transporter_count = unmarshallShort(inf);
    for (int i = 0; i < transporter_count; ++i)
    {
        transporter_info ti;
        ti.load(inf);
        transporters.push_back(ti);
    }
#if TAG_MAJOR_VERSION == 34
    }
#endif

    unmarshallExcludes(inf, minorVersion, excludes);

    int n_count = unmarshallByte(inf);
    ASSERT_RANGE(n_count, 0, NUM_DACTION_COUNTERS + 1);
    for (int i = 0; i < n_count; i++)
        daction_counters[i] = unmarshallShort(inf);
}

void LevelInfo::fixup()
{
    // The only fixup we do now is for the hell entry.
    if (id.branch != BRANCH_DEPTHS || !travel_hell_entry.is_valid())
        return;

    for (stair_info &si : stairs)
    {
        if (si.destination.id.branch == BRANCH_VESTIBULE
            && !si.destination.is_valid())
        {
            si.destination = travel_hell_entry;
        }
    }
}

void TravelCache::update_stone_stair(const coord_def &c)
{
    if (!env.map_knowledge(c).seen())
        return;
    LevelInfo *li = find_level_info(level_id::current());
    if (!li)
        return;
    stair_info *si = li->get_stair(c);
    // Don't bother proceeding further if we already know where the stair goes.
    if (si && si->destination.is_valid())
        return;
    const dungeon_feature_type feat1 = env.grid(c);
    ASSERT(feat_is_stone_stair(feat1));
    // Compute the corresponding feature type on the other side of the stairs.
    const dungeon_feature_type feat2 = (dungeon_feature_type)
          (feat1 + (feat_is_stone_stair_up(feat1) ? 1 : -1)
                   * (DNGN_STONE_STAIRS_DOWN_I - DNGN_STONE_STAIRS_UP_I));
    LevelInfo *li2 = find_level_info(level_id::get_next_level_id(c));
    if (!li2)
        return;
    for (int i = static_cast<int>(li2->stairs.size()) - 1; i >= 0; --i)
    {
        if (li2->stairs[i].grid == feat2)
        {
            if (li2->stairs[i].type == stair_info::MAPPED)
                return;
            // If we haven't added these stairs to our LevelInfo yet, do so
            // before trying to update them.
            if (!si)
            {
                stair_info si2;
                si2.position = c;
                si2.grid = env.grid(si2.position);
                li->stairs.push_back(si2);
            }
            li->update_stair(c,level_pos(li2->id,li2->stairs[i].position));
            // Add the other stair direction too so that X[]ing to the other
            // level will be correct immediately.
            li2->update_stair(li2->stairs[i].position,level_pos(li->id,c));
            return;
        }
    }
}

void TravelCache::update_transporter(const coord_def &c)
{
    const dungeon_feature_type feat = env.grid(c);
    ASSERT(feat == DNGN_TRANSPORTER);

    if (!env.map_knowledge(c).seen())
        return;

    LevelInfo *li = find_level_info(level_id::current());
    if (!li)
        return;

    transporter_info *ti = li->get_transporter(c);
    if (!ti)
    {
        li->transporters.push_back(transporter_info(c, coord_def(),
                                   transporter_info::transporter_type::PHYSICAL));
    }
}

bool TravelCache::know_transporter(const coord_def &c)
{
    if (env.grid(c) == DNGN_TRANSPORTER)
        update_transporter(c);
    auto i = levels.find(level_id::current());
    return i == levels.end() ? false : i->second.know_transporter(c);
}

bool TravelCache::know_stair(const coord_def &c)
{
     if (feat_is_stone_stair(env.grid(c)))
         update_stone_stair(c);
    auto i = levels.find(level_id::current());
    return i == levels.end() ? false : i->second.know_stair(c);
}

vector<string> TravelCache::get_waypoint_descs() const
{
    vector<string> result;
    for (int i = 0; i < TRAVEL_WAYPOINT_COUNT; ++i)
    {
        if (!is_valid_waypoint(i))
        {
            result.push_back("");
            continue;
        }
        result.push_back(_get_trans_travel_dest(waypoints[i], false, true));
    }
    return result;
}

void TravelCache::list_waypoints() const
{
    string line;
    vector<string> wdescs = get_waypoint_descs();
    int count = 0;
    for (int i = 0; i < static_cast<int>(wdescs.size()); i++)
    {
        if (wdescs[i].size() == 0)
            continue;
        line += make_stringf("(%d) %-9s", i, wdescs[i].c_str());
        if (!(++count % 5))
        {
            mpr(line);
            line = "";
        }
    }
    if (!line.empty())
        mpr(line);
}

uint8_t TravelCache::is_waypoint(const level_pos &lp) const
{
    for (int i = 0; i < TRAVEL_WAYPOINT_COUNT; ++i)
        if (lp == waypoints[i] && _is_valid_waypoint_pos(lp))
            return '0' + i;

    return 0;
}

void TravelCache::flush_invalid_waypoints()
{
    for (int i = 0; i < TRAVEL_WAYPOINT_COUNT; ++i)
        if (!is_valid_waypoint(i)
            || !is_connected_branch(waypoints[i].id.branch))
        {
            waypoints[i].clear();
        }
}

void TravelCache::update_waypoints() const
{
    level_pos lp;
    lp.id = level_id::current();

    memset(curr_waypoints, 0, sizeof curr_waypoints);
    for (lp.pos.x = 1; lp.pos.x < GXM; ++lp.pos.x)
        for (lp.pos.y = 1; lp.pos.y < GYM; ++lp.pos.y)
        {
            uint8_t wpc = is_waypoint(lp);
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
        clear_messages();
        mpr("Existing waypoints:");
        list_waypoints();
        mprf(MSGCH_PROMPT, "Delete which waypoint? (* - delete all, Esc - exit) ");

        int key = getchm();
        if (key >= '0' && key <= '9')
        {
            if (is_valid_waypoint(key - '0'))
            {
                waypoints[key - '0'].clear();
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

    clear_messages();
    mpr("All waypoints deleted. Have a nice day!");
}

void TravelCache::add_waypoint(int x, int y)
{
    clear_messages();

    const bool waypoints_exist = get_waypoint_count();
    if (waypoints_exist)
    {
        mpr("Existing waypoints:");
        list_waypoints();
    }

    if (you.where_are_you == BRANCH_ABYSS)
        mprf(MSGCH_PROMPT, "Waypoints on this level may disappear at any time.");
    else if (!is_connected_branch(you.where_are_you))
        mprf(MSGCH_PROMPT, "Waypoints will disappear once you leave this level.");
    mprf(MSGCH_PROMPT, "Assign waypoint to what number? (0-9%s) ",
         waypoints_exist? ", D - delete waypoint" : "");

    int keyin = toalower(get_ch());

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

    set_waypoint(keyin - '0', x, y);

}

void TravelCache::set_waypoint(int waynum, int x, int y)
{
    ASSERT_RANGE(waynum, 0, TRAVEL_WAYPOINT_COUNT);
    coord_def pos(x,y);
    if (x == -1 || y == -1)
        pos = you.pos();

    const level_id &lid = level_id::current();

    const bool overwrite = is_valid_waypoint(waynum);

    string old_dest =
        overwrite ? _get_trans_travel_dest(waypoints[waynum], false, true) : "";
    level_id old_lid = (overwrite ? waypoints[waynum].id : lid);

    waypoints[waynum].id  = lid;
    waypoints[waynum].pos = pos;

    string new_dest = _get_trans_travel_dest(waypoints[waynum], false, true);
    clear_messages();
    if (overwrite)
    {
        if (lid == old_lid) // same level
            mprf("Waypoint %d re-assigned to %s.", waynum, new_dest.c_str());
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

static bool _is_valid_waypoint_pos(const level_pos &pos)
{
    // waypoints in portal branches are only valid while you're there
    return pos.is_valid()
        && (is_connected_branch(pos.id.branch)
            || you.where_are_you == pos.id.branch);
}

bool TravelCache::is_valid_waypoint(int waynum) const
{
    if (waynum < 0 || waynum >= TRAVEL_WAYPOINT_COUNT)
        return false;
    return _is_valid_waypoint_pos(waypoints[waynum]);
}

int TravelCache::get_waypoint_count() const
{
    int count = 0;
    for (int i = 0; i < TRAVEL_WAYPOINT_COUNT; ++i)
        if (is_valid_waypoint(i))
            count++;

    return count;
}

void TravelCache::clear_distances()
{
    for (auto &entry : levels)
        entry.second.clear_distances();
}

bool TravelCache::is_known_branch(uint8_t branch) const
{
    return any_of(begin(levels), end(levels),
            [branch] (const pair<level_id, LevelInfo> &entry)
            { return entry.second.is_known_branch(branch); });
}

void TravelCache::save(writer& outf) const
{
    write_save_version(outf, save_version::current());

    // Write level count.
    marshallShort(outf, levels.size());

    for (const auto &entry : levels)
    {
        entry.first.save(outf);
        entry.second.save(outf);
    }

    for (int wp = 0; wp < TRAVEL_WAYPOINT_COUNT; ++wp)
        waypoints[wp].save(outf);
}

void TravelCache::load(reader& inf, int minorVersion)
{
    levels.clear();

    // Check version. If not compatible, we just ignore the file altogether.
    const auto version = get_save_version(inf);
    const auto major = version.major, minor = version.minor;
    if (major != TAG_MAJOR_VERSION || minor > TAG_MINOR_VERSION)
        return;

    int level_count = unmarshallShort(inf);
    for (int i = 0; i < level_count; ++i)
    {
        level_id id;
        id.load(inf);

        LevelInfo linfo;
        // Must set id before load, or travel_hell_entry will not be
        // correctly set.
        linfo.id = id;
        linfo.load(inf, minorVersion);

        levels[id] = linfo;
    }

#if TAG_MAJOR_VERSION == 34
    if (minor < TAG_MINOR_MORE_WAYPOINTS)
    {
        for (int wp = 0; wp < 10; ++wp)
            waypoints[wp].load(inf);
    }
    else
    {
#endif
        for (int wp = 0; wp < TRAVEL_WAYPOINT_COUNT; ++wp)
            waypoints[wp].load(inf);
#if TAG_MAJOR_VERSION == 34
    }
#endif

    fixup_levels();
}

void TravelCache::set_level_excludes()
{
    get_level_info(level_id::current()).set_level_excludes();
}

void TravelCache::update_excludes()
{
    get_level_info(level_id::current()).update_excludes();
}

void TravelCache::update()
{
    get_level_info(level_id::current()).update();
}

void TravelCache::update_daction_counters()
{
    ::update_daction_counters(&get_level_info(level_id::current()));
}

unsigned int TravelCache::query_daction_counter(daction_type c)
{
    // other levels are up to date, the current one not necessarily so
    update_daction_counters();

    unsigned int sum = 0;

    for (const auto &entry : levels)
        sum += entry.second.daction_counters[c];

    return sum;
}

void TravelCache::clear_daction_counter(daction_type c)
{
    for (auto &entry : levels)
        entry.second.daction_counters[c] = 0;
}

void TravelCache::fixup_levels()
{
    for (auto &entry : levels)
        entry.second.fixup();
}

vector<level_id> TravelCache::known_levels() const
{
    vector<level_id> levs;

    for (const auto &entry : levels)
        levs.push_back(entry.first);

    return levs;
}

bool can_travel_to(const level_id &id)
{
    return is_connected_branch(id) && can_travel_interlevel()
           || id == level_id::current();
}

bool can_travel_interlevel()
{
    return player_in_connected_branch();
}

/////////////////////////////////////////////////////////////////////////////
// Shift-running and resting.

runrest::runrest()
    : runmode(0), mp(0), hp(0), pos(0,0), turns_passed(0)
{
}

// Initialise is only called for resting/shift-running. We should eventually
// include travel and wrap it all in.
void runrest::initialise(int dir, int mode)
{
    // Note HP and MP for reference.
    hp = you.hp;
    mp = you.magic_points;
    direction = dir;
    notified_hp_full = false;
    notified_mp_full = false;
    notified_ancestor_hp_full = false;
    turns_passed = 0;

    if (dir == RDIR_REST)
    {
        pos.reset();
        runmode = mode;
    }
    else
    {
        ASSERT_RANGE(dir, 0, 8);

        pos = Compass[dir];
        runmode = mode;

        // Get the compass point to the left/right of intended travel:
        const int left  = (dir - 1 < 0) ? 7 : (dir - 1);
        const int right = (dir + 1 > 7) ? 0 : (dir + 1);

        // Record the direction and starting tile type for later reference:
        set_run_check(0, left);
        set_run_check(1, dir);
        set_run_check(2, right);
    }

    if (runmode == RMODE_REST_DURATION || runmode == RMODE_WAIT_DURATION)
        start_delay<RestDelay>();
    else
        start_delay<RunDelay>();
}

runrest::operator int () const
{
    return runmode;
}

const runrest &runrest::operator = (int newrunmode)
{
    runmode = newrunmode;
    return *this;
}

static dungeon_feature_type _base_feat_type(dungeon_feature_type grid)
{
    // Merge walls.
    if (feat_is_wall(grid))
        return DNGN_ROCK_WALL;

    return grid;
}

void runrest::set_run_check(int index, int dir)
{
    run_check[index].delta = Compass[dir];

    const coord_def p = you.pos() + Compass[dir];
    run_check[index].grid = _base_feat_type(env.map_knowledge(p).feat());
}

bool runrest::check_stop_running()
{
    if (runmode > 0 && runmode != RMODE_START && run_should_stop())
    {
        stop();
        return true;
    }
    return false;
}

// This function creates "equivalence classes" so that changes
// in wall and floor type aren't running stopping points.
bool runrest::run_should_stop() const
{
    const coord_def targ = you.pos() + pos;
    const map_cell& tcell = env.map_knowledge(targ);

    // XXX: probably this should ignore cosmetic clouds (non-opaque)
    if (tcell.cloud() != CLOUD_NONE
        && !you.cloud_immune())
    {
        return true;
    }

    if (is_excluded(targ) && !is_stair_exclusion(targ))
    {
#ifndef USE_TILE_LOCAL
        // XXX: Remove this once exclusions are visible.
        mprf(MSGCH_WARN, "Stopped running for exclusion.");
#endif
        return true;
    }

    const monster_info* mon = tcell.monsterinfo();
    if (mon && !fedhas_passthrough(tcell.monsterinfo()))
        return true;

    if (count_adjacent_slime_walls(targ))
        return true;

    for (int i = 0; i < 3; i++)
    {
        const coord_def p = you.pos() + run_check[i].delta;
        const dungeon_feature_type feat =
            _base_feat_type(env.map_knowledge(p).feat());

        if (run_check[i].grid != feat)
            return true;
    }

    bool is_running_diag = (direction % 2 == 1);
    if (is_running_diag && diag_run_passes_door())
        return true;

    return false;
}

// Checks whether the player passes a door when running diagonally, since
// in certain situations those could be overlooked by only checking "left"
// and "right".
// Can be extended if other features should lead to stopping as well.
bool runrest::diag_run_passes_door() const
{
    const int diag_left = (direction + 6) % 8;
    const int diag_right = (direction + 2) % 8;
    const int diag_dirs[2] = { diag_left, diag_right };
    for (int dir : diag_dirs)
    {
        const coord_def p = you.pos() + Compass[dir];
        const auto feat = env.map_knowledge(p).feat();
        if (feat_is_door(feat))
            return true;
    }

    return false;
}

void runrest::stop(bool clear_delays)
{
    bool need_redraw =
        (runmode > 0 || runmode < 0 && Options.travel_delay == -1);
    _userdef_run_stoprunning_hook();
    runmode = RMODE_NOT_RUNNING;

    // Kill the delay; this is fine because it's not possible to stack
    // run/rest/travel on top of other delays.
    if (clear_delays)
        stop_delay();

#ifdef USE_TILE_LOCAL
    if (Options.tile_runrest_rate > 0)
        tiles.set_need_redraw();
#endif

    quiver::set_needs_redraw();
    if (need_redraw)
    {
        viewwindow();
        print_stats();
        update_screen();
    }
}

bool runrest::is_rest() const
{
    return runmode > 0 && pos.origin();
}

bool runrest::is_explore() const
{
    return runmode == RMODE_EXPLORE || runmode == RMODE_EXPLORE_GREEDY;
}

bool runrest::is_any_travel() const
{
    switch (runmode)
    {
    case RMODE_INTERLEVEL:
    case RMODE_EXPLORE_GREEDY:
    case RMODE_EXPLORE:
    case RMODE_TRAVEL:
        return true;
    default:
        return false;
    }
}

string runrest::runmode_name() const
{
    switch (runmode)
    {
    case RMODE_EXPLORE:
    case RMODE_EXPLORE_GREEDY:
        return "explore";
    case RMODE_INTERLEVEL:
    case RMODE_TRAVEL:
        return "travel";
    default:
        if (runmode > 0)
            return pos.origin()? "rest" : "run";
        return "";
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
    turns_passed = 0;
    notified_hp_full = false;
    notified_mp_full = false;
    notified_ancestor_hp_full = false;
}

/////////////////////////////////////////////////////////////////////////////
// explore_discoveries

explore_discoveries::explore_discoveries()
    : can_autopickup(::can_autopickup()),
      es_flags(0),
      current_level(nullptr), items(), stairs(), portals(), shops(), altars(),
      runed_doors()
{
}

string explore_discoveries::cleaned_feature_description(
    const coord_def &pos) const
{
    string s = lowercase_first(feature_description_at(pos));
    // TODO: can feature_description_at()'s return value still end in '.'?
    if (s.length() && s[s.length() - 1] == '.')
        s.erase(s.length() - 1);
    if (starts_with(s, "a "))
        s = s.substr(2);
    else if (starts_with(s, "an "))
        s = s.substr(3);
    return s;
}

bool explore_discoveries::merge_feature(
    vector< explore_discoveries::named_thing<int> > &v,
    const explore_discoveries::named_thing<int> &feat) const
{
    for (explore_discoveries::named_thing<int> &nt: v)
        if (feat == nt)
        {
            ++nt.thing;
            return true;
        }

    return false;
}

static bool _feat_is_branchlike(dungeon_feature_type feat)
{
    return feat_is_branch_entrance(feat)
        || feat_is_hell_subbranch_exit(feat)
        || feat == DNGN_ENTER_ABYSS
        || feat == DNGN_EXIT_THROUGH_ABYSS
        || feat == DNGN_ENTER_PANDEMONIUM;
}

void explore_discoveries::found_feature(const coord_def &pos,
                                        dungeon_feature_type feat)
{
    if (feat == DNGN_ENTER_SHOP && ES_shop)
    {
        shops.emplace_back(shop_name(*shop_at(pos)), feat);
        es_flags |= ES_SHOP;
    }
    else if (feat_is_stair(feat) && ES_stair)
    {
        const named_thing<int> stair(cleaned_feature_description(pos), 1);
        add_stair(stair);
        es_flags |= ES_STAIR;
    }
    else if (_feat_is_branchlike(feat) && ES_branch)
    {
        const named_thing<int> stair(cleaned_feature_description(pos), 1);
        add_stair(stair);
        es_flags |= ES_BRANCH;
    }
    else if (feat_is_portal(feat) && ES_portal)
    {
        const named_thing<int> portal(cleaned_feature_description(pos), 1);
        add_stair(portal);
        es_flags |= ES_PORTAL;
    }
    else if (feat_is_runed(feat))
    {
        seen_tracked_feature(feat);
        if (ES_rdoor)
        {
            for (orth_adjacent_iterator ai(pos); ai; ++ai)
            {
                // If any neighbours have been seen (and thus announced)
                // before, skip. For parts seen for the first time this turn,
                // announce only the upper leftmost cell.
                if (feat_is_runed(env.map_knowledge(*ai).feat())
                    && (env.map_seen(*ai) || *ai < pos))
                {
                    return;
                }
            }

            string desc = env.markers.property_at(pos, MAT_ANY, "stop_explore");
            if (desc.empty())
                desc = cleaned_feature_description(pos);
            runed_doors.emplace_back(desc, 1);
            es_flags |= ES_RUNED_DOOR;
        }
    }
    else if (feat == DNGN_TRANSPORTER)
    {
        seen_tracked_feature(feat);
        if (ES_transporter)
        {
            for (orth_adjacent_iterator ai(pos); ai; ++ai)
            {
                // If any neighbours have been seen (and thus announced) before,
                // skip. For parts seen for the first time this turn, announce
                // only the upper leftmost cell.
                if (env.map_knowledge(*ai).feat() == DNGN_TRANSPORTER
                    && (env.map_seen(*ai) || *ai < pos))
                {
                    return;
                }
            }

            string desc = env.markers.property_at(pos, MAT_ANY, "stop_explore");
            if (desc.empty())
                desc = cleaned_feature_description(pos);
            transporters.emplace_back(desc, 1);
            es_flags |= ES_TRANSPORTER;
        }
    }
    else if (feat_is_altar(feat) && ES_altar)
    {
        const named_thing<int> altar(cleaned_feature_description(pos), 1);
        if (!merge_feature(altars, altar))
            altars.push_back(altar);
        es_flags |= ES_ALTAR;
    }
    // Would checking for a marker for all discovered cells slow things
    // down too much?
    else if (feat_is_statuelike(feat))
    {
        const string feat_stop_msg =
            env.markers.property_at(pos, MAT_ANY, "stop_explore_msg");
        if (!feat_stop_msg.empty())
        {
            marker_msgs.push_back(feat_stop_msg);
            return;
        }

        const string feat_stop = env.markers.property_at(pos, MAT_ANY,
                                                         "stop_explore");
        if (!feat_stop.empty())
        {
            string desc = lowercase_first(feature_description_at(pos));
            marked_feats.push_back(desc + ".");
            return;
        }
    }
    else if (feat == DNGN_RUNELIGHT)
    {
        runelights.emplace_back(cleaned_feature_description(pos), 1);
        es_flags |= ES_RUNELIGHT;
    }
}

void explore_discoveries::add_stair(
    const explore_discoveries::named_thing<int> &stair)
{
    if (merge_feature(stairs, stair) || merge_feature(portals, stair))
        return;

    // Hackadelic
    if (stair.name.find("stair") != string::npos)
        stairs.push_back(stair);
    else
        portals.push_back(stair);
}

void explore_discoveries::add_item(const item_def &i)
{
    item_def copy = i;
    copy.quantity = 1;
    const string cname = copy.name(DESC_PLAIN);

    // Try to find something to stack it with, stacking by name.
    for (named_thing<item_def> &item : items)
    {
        const int orig_quantity = item.thing.quantity;
        item.thing.quantity = 1;
        if (cname == item.thing.name(DESC_PLAIN))
        {
            item.thing.quantity = orig_quantity + i.quantity;
            item.name = item.thing.name(DESC_A, false, false, true,
                                        !is_stackable_item(i));
            return;
        }
        item.thing.quantity = orig_quantity;
    }

    string itemname = menu_colour_item_name(i, DESC_A);
    monster* mon = monster_at(i.pos);
    if (mon && mons_species(mon->type) == MONS_BUSH)
        itemname += " (under bush)";
    else if (mon && mon->type == MONS_PLANT)
        itemname += " (under plant)";

    items.emplace_back(itemname, i);

    // First item of this type?
    // XXX: Only works when travelling.
    hints_first_item(i);
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
            const bool greed_inducing = _is_greed_inducing_square(current_level,
                                                                  pos,
                                                                  can_autopickup);

            if (greed_inducing && (Options.explore_stop & ES_GREEDY_ITEM))
                ; // Stop for this condition
            else if (!greed_inducing
                     && (Options.explore_stop & ES_ITEM
                         || Options.explore_stop & ES_GLOWING_ITEM
                            && i.flags & ISFLAG_COSMETIC_MASK
                         || Options.explore_stop & ES_ARTEFACT
                            && i.flags & ISFLAG_ARTEFACT_MASK
                         || Options.explore_stop & ES_RUNE
                            && (i.base_type == OBJ_RUNES
                                || i.base_type == OBJ_GEMS /*enh*/)))
            {
                ; // More conditions to stop for
            }
            else
                return; // No conditions met, don't stop for this item
        }
    } // if (you.running == RMODE_EXPLORE_GREEDY)

    add_item(i);
    es_flags |=
        (you.running == RMODE_EXPLORE_GREEDY) ? ES_GREEDY_PICKUP_MASK :
        (Options.explore_stop & ES_ITEM) ? ES_ITEM : ES_NONE;
}

// Expensive O(n^2) duplicate search, but we can live with that.
template <class citer> bool explore_discoveries::has_duplicates(
    citer begin, citer end) const
{
    for (citer s = begin; s != end; ++s)
        for (citer z = s + 1; z != end; ++z)
        {
            if (*s == *z)
                return true;
        }

    return false;
}

template <class C> void explore_discoveries::say_any(
    const C &coll, const char *category) const
{
    if (coll.empty())
        return;

    const int size = coll.size();

    string plural = pluralise(category);
    if (size != 1)
        category = plural.c_str();

    if (has_duplicates(coll.begin(), coll.end()))
    {
        mprf("Found %s %s.", number_in_words(size).c_str(), category);
        return;
    }

    const auto message = "Found " +
                           comma_separated_line(coll.begin(), coll.end()) + ".";

    if (formatted_string::parse_string(message).width() >= get_number_of_cols())
        mprf("Found %s %s.", number_in_words(size).c_str(), category);
    else
        mpr(message);
}

vector<string> explore_discoveries::apply_quantities(
    const vector< named_thing<int> > &v) const
{
    static const char *feature_plural_qualifiers[] =
    {
        " leading ", " back to ", " to ", " of ", " in ", " out of",
        " from ", " back into ", nullptr
    };

    vector<string> things;
    for (const named_thing<int> &nt : v)
    {
        if (nt.thing == 1)
            things.push_back(article_a(nt.name));
        else
        {
            things.push_back(number_in_words(nt.thing)
                             + " "
                             + pluralise(nt.name, feature_plural_qualifiers));
        }
    }
    return things;
}

bool explore_discoveries::stop_explore() const
{
    const bool marker_stop = !marker_msgs.empty() || !marked_feats.empty();

    for (const string &msg : marker_msgs)
        mpr(msg);

    for (const string &marked : marked_feats)
        mprf("Found %s", marked.c_str());

    if (!es_flags)
        return marker_stop;

    say_any(items, "item");
    say_any(shops, "shop");
    say_any(apply_quantities(altars), "altar");
    say_any(apply_quantities(portals), "portal");
    say_any(apply_quantities(stairs), "stair");
    say_any(apply_quantities(transporters), "transporter");
    say_any(apply_quantities(runed_doors), "runed door");
    say_any(apply_quantities(runelights), "runelights");

    return true;
}

void do_interlevel_travel()
{
    if (Hints.hints_travel)
        Hints.hints_travel = 0;

    if (!can_travel_interlevel())
    {
        if (you.running.pos == you.pos())
        {
            mpr("You're already here!");
            return;
        }
        else if (!you.running.pos.x || !you.running.pos.y)
        {
            mpr("Sorry, you can't auto-travel out of here.");
            return;
        }

        // Don't ask for a destination if you can only travel
        // within level anyway.
        start_travel(you.running.pos);
    }
    else
        _start_translevel_travel_prompt();

    if (you.running)
        clear_messages();
}

#ifdef USE_TILE
// (0,0) = same position is handled elsewhere.
const int dir_dx[8] = {-1, 0, 1, -1, 1, -1,  0,  1};
const int dir_dy[8] = { 1, 1, 1,  0, 0, -1, -1, -1};

const int cmd_array[8] =
{
    CMD_MOVE_DOWN_LEFT,  CMD_MOVE_DOWN,  CMD_MOVE_DOWN_RIGHT,
    CMD_MOVE_LEFT,                       CMD_MOVE_RIGHT,
    CMD_MOVE_UP_LEFT,    CMD_MOVE_UP,    CMD_MOVE_UP_RIGHT,
};

static int _adjacent_cmd(const coord_def &gc, bool force)
{
    const coord_def dir = gc - you.pos();
    for (int i = 0; i < 8; i++)
    {
        if (dir_dx[i] != dir.x || dir_dy[i] != dir.y)
            continue;

        int cmd = cmd_array[i];
        if (!force)
            return cmd;
        const dungeon_feature_type feat = env.grid(gc);
        if ((feat == DNGN_OPEN_DOOR || feat == DNGN_OPEN_CLEAR_DOOR)
            && !env.map_knowledge(gc).monsterinfo())
        {
            return CMD_CLOSE_DOOR_LEFT - CMD_MOVE_LEFT;
        }
        return cmd + CMD_ATTACK_LEFT - CMD_MOVE_LEFT;
    }

    return CK_MOUSE_CMD;
}

bool click_travel_safe(const coord_def &gc)
{
    return (!is_excluded(gc) || is_stair_exclusion(gc))
        && (!is_excluded(you.pos()) || is_stair_exclusion(you.pos()))
        && i_feel_safe(false, false, false, false);
}

int click_travel(const coord_def &gc, bool force)
{
    if (!in_bounds(gc))
        return CK_MOUSE_CMD;

    const int cmd = _adjacent_cmd(gc, force);
    if (cmd != CK_MOUSE_CMD)
        return cmd;

    if (click_travel_safe(gc))
    {
        map_cell &cell(env.map_knowledge(gc));
        // If there's a monster that would block travel,
        // don't start traveling.
        if (!_monster_blocks_travel(cell.monsterinfo()))
        {
            start_travel(gc);
            return CK_MOUSE_CMD;
        }
    }

    // If not safe, then take one step towards the click.
    travel_pathfind tp;
    tp.set_src_dst(you.pos(), gc);
    tp.set_ignore_danger();
    const coord_def dest = tp.pathfind(RMODE_TRAVEL);

    if (!dest.x && !dest.y)
        return CK_MOUSE_CMD;

    return _adjacent_cmd(dest, force);
}
#endif

bool check_for_interesting_features()
{
    // Scan through the shadow map, compare it with the actual map, and if
    // there are any squares of the shadow map that have just been
    // discovered and contain an item, or have an interesting dungeon
    // feature, announce the discovery and return true.
    explore_discoveries discoveries;
    for (vision_iterator ri(you); ri; ++ri)
    {
        const coord_def p(*ri);

        // Find just noticed squares.
        if (env.map_knowledge(p).flags & MAP_SEEN_FLAG
            && !env.map_seen(p))
        {
            // Update the shadow map
            env.map_seen.set(p);

            // But don't stop if we knew about it previously
            if (!env.map_forgotten
                || !((*env.map_forgotten)(p).flags & MAP_SEEN_FLAG))
            {
                _check_interesting_square(p, discoveries);
            }
        }
    }

    return discoveries.stop_explore();
}

void clear_level_target()
{
    level_target.clear();
    trans_travel_dest.clear();
}

void clear_travel_trail()
{
#ifdef USE_TILE_WEB
    for (coord_def c : env.travel_trail)
        tiles.update_minimap(c);
#endif
    env.travel_trail.clear();
}

int travel_trail_index(const coord_def& gc)
{
    unsigned int idx = find(env.travel_trail.begin(), env.travel_trail.end(), gc)
        - env.travel_trail.begin();
    if (idx < env.travel_trail.size())
        return idx;
    else
        return -1;
}

bool stairs_destination_is_excluded(const stair_info &si)
{
    level_pos dest = si.destination;
    if (LevelInfo *dest_li = travel_cache.find_level_info(dest.id))
    {
        if (is_unknown_stair(si.position)
            || !is_excluded(dest.pos, dest_li->get_excludes()))
        {
            return false;
        }

        // Check for exclusions that cover the stair destination, but ignore
        // those that have radius 1: those exclude travel in the _other_
        // direction only (from the destination to here, not from here to the
        // destination)
        const exclude_set &excludes = dest_li->get_excludes();
        for (auto entry : excludes)
        {
            const travel_exclude &ex = entry.second;
            if (ex.in_bounds(dest.pos) && ex.radius > 1)
                return true;
        }
    }

    return false;
}
