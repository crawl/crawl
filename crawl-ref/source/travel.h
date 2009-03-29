/*
 *  File:       travel.h
 *  Summary:    Travel stuff
 *  Written by: Darshan Shaligram
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */
#ifndef TRAVEL_H
#define TRAVEL_H

#include "externs.h"
#include <stdio.h>
#include <string>
#include <vector>
#include <map>

class reader;
class writer;

enum run_check_type
{
    RCHECK_LEFT,
    RCHECK_FRONT,
    RCHECK_RIGHT
};

enum run_dir_type
{
    RDIR_UP = 0,
    RDIR_UP_RIGHT,
    RDIR_RIGHT,
    RDIR_DOWN_RIGHT,
    RDIR_DOWN,
    RDIR_DOWN_LEFT,
    RDIR_LEFT,
    RDIR_UP_LEFT,
    RDIR_REST
};

enum run_mode_type
{
    RMODE_INTERLEVEL     = -4, // Interlevel travel (Ctrl+G)
    RMODE_EXPLORE_GREEDY = -3, // Explore + grab items (Tab/Ctrl+I)
    RMODE_EXPLORE        = -2, // Exploring (Ctrl+O)
    RMODE_TRAVEL         = -1, // Classic or Plain Old travel
    RMODE_NOT_RUNNING    = 0,  // must remain equal to 0
    RMODE_CONTINUE,
    RMODE_START,
    RMODE_REST_DURATION = 100
};

/* ***********************************************************************
 * Initialises the travel subsystem.
 *
 * ***********************************************************************
 * called from: initfile (what's a better place to initialise stuff?)
 * *********************************************************************** */
void initialise_travel();
void init_travel_terrain_check(bool check_race_equip = true);
void stop_running(void);
void travel_init_new_level();
void cycle_exclude_radius(const coord_def &p);
void toggle_exclude(const coord_def &p);
void set_exclude(const coord_def &p, int radius2);
void clear_excludes();
unsigned char is_waypoint(const coord_def &p);
bool is_exclude_root(const coord_def &p);
bool is_stair(dungeon_feature_type gridc);
bool is_travelable_stair(dungeon_feature_type gridc);
bool is_gate(dungeon_feature_type gridc);
command_type direction_to_command( char x, char y );
bool is_altar(dungeon_feature_type gridc);
bool is_resting( void );
#ifdef CLUA_BINDINGS
const char *trap_name(const coord_def &p);
#endif
bool is_traversable(dungeon_feature_type grid);
void explore_pickup_event(int did_pickup, int tried_pickup);
bool is_excluded(const coord_def &p);

void find_travel_pos(const coord_def& youpos, char *move_x, char *move_y,
                     std::vector<coord_def>* coords = NULL);

bool is_travelsafe_square(const coord_def& c, bool ignore_hostile = false,
                          bool ignore_terrain_knowledge = false);

/* ***********************************************************************
 * Initiates explore - the character runs around the level to map it. Note
 * that the caller has to ensure that the level is mappable before calling
 * start_explore. start_explore may lock up the game on unmappable levels.
 * If grab_items is true, greedy explore is triggered - in greedy mode, explore
 * grabs items that are eligible for autopickup and visits (previously
 * unvisited) shops.
 *
 * ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void start_explore(bool grab_items = false);

struct level_pos;
class level_id;
struct travel_target;

void start_translevel_travel(const travel_target &pos);

void start_translevel_travel(bool prompt_for_destination = true);

void start_travel(const coord_def& p);

command_type travel();

int travel_direction(unsigned char branch, int subdungeondepth);

void prevent_travel_to(const std::string &dungeon_feature_name);

// Sort dungeon features as appropriate.
void arrange_features(std::vector<coord_def> &features);
int level_distance(level_id first, level_id second);

bool can_travel_to(const level_id &lid);
bool can_travel_interlevel();
bool prompt_stop_explore(int es_why);

enum translevel_prompt_flags
{
    TPF_NO_FLAGS          = 0,

    TPF_ALLOW_WAYPOINTS   = 0x1,
    TPF_ALLOW_UPDOWN      = 0x2,
    TPF_REMEMBER_TARGET   = 0x4,
    TPF_SHOW_ALL_BRANCHES = 0x8,

    TPF_DEFAULT_OPTIONS   = TPF_ALLOW_WAYPOINTS | TPF_ALLOW_UPDOWN
                                                | TPF_REMEMBER_TARGET
};

travel_target prompt_translevel_target(int prompt_flags = TPF_DEFAULT_OPTIONS);

// Magic numbers for point_distance:

// This square is a trap
const int PD_TRAP = -42;

// The user never wants to travel this square
const int PD_EXCLUDED = -20099;

// This square is within LOS radius of an excluded square
const int PD_EXCLUDED_RADIUS = -20100;

typedef int travel_distance_col[GYM];
typedef travel_distance_col travel_distance_grid_t[GXM];

/* ***********************************************************************
 * Array of points on the map, each value being the distance the character
 * would have to travel to get there. Negative distances imply that the point
 * is a) a trap or hostile terrain or b) only reachable by crossing a trap or
 * hostile terrain.
 * ***********************************************************************
 * referenced in: travel - view
 * *********************************************************************** */
extern travel_distance_grid_t travel_point_distance;

enum explore_stop_type
{
    ES_NONE               = 0x000,
    ES_ITEM               = 0x001,
    ES_PICKUP             = 0x002,
    ES_GREEDY_PICKUP      = 0x004,
    ES_GREEDY_ITEM        = 0x008,
    ES_STAIR              = 0x010,
    ES_SHOP               = 0x020,
    ES_ALTAR              = 0x040,
    ES_PORTAL             = 0x080,
    ES_GLOWING_ITEM       = 0x100,
    ES_ARTEFACT           = 0x200,
    ES_RUNE               = 0x400
};

////////////////////////////////////////////////////////////////////////////
// Structs for interlevel travel.


// A position on a particular level.
struct level_pos
{
    level_id      id;
    coord_def     pos;      // The grid coordinates on this level.

    level_pos() : id(), pos()
    {
        pos.x = pos.y = -1;
    }

    level_pos(const level_id &lid, const coord_def &coord)
        : id(lid), pos(coord)
    {
    }

    level_pos(const level_id &lid)
        : id(lid), pos()
    {
        pos.x = pos.y = -1;
    }

    bool operator == ( const level_pos &lp ) const
    {
        return id == lp.id && pos == lp.pos;
    }

    bool operator != ( const level_pos &lp ) const
    {
        return id != lp.id || pos != lp.pos;
    }

    bool operator <  ( const level_pos &lp ) const
    {
        return (id < lp.id) || (id == lp.id && pos < lp.pos);
    }

    bool is_valid() const
    {
        return id.depth > -1 && pos.x != -1 && pos.y != -1;
    }

    void clear()
    {
        id.clear();
        pos = coord_def(-1, -1);
    }

    void save(writer&) const;
    void load(reader&);
};

struct travel_target
{
    level_pos p;
    bool      entrance_only;

    travel_target(const level_pos &_pos, bool entry = false)
        : p(_pos), entrance_only(entry)
    {
    }
    travel_target()
        : p(), entrance_only(false)
    {
    }
    void clear()
    {
        p.clear();
        entrance_only = false;
    }
    bool operator == (const travel_target &other) const
    {
        return p == other.p && entrance_only == other.entrance_only;
    }
    bool operator != (const travel_target &other) const
    {
        return !operator == (other);
    }
};

// Tracks items discovered by explore in this turn.
class LevelStashes;
class explore_discoveries
{
public:
    explore_discoveries();

    void found_feature(const coord_def &pos, dungeon_feature_type grid);
    void found_item(const coord_def &pos, const item_def &item);

    // Reports discoveries and prompts the player to stop (if necessary).
    bool prompt_stop() const;

private:
    template <class Z> struct named_thing {
        std::string name;
        Z thing;

        named_thing(const std::string &n, Z t) : name(n), thing(t) { }
        operator std::string () const { return name; }

        bool operator == (const named_thing<Z> &other) const
        {
            return (name == other.name);
        }
    };

    int es_flags;
    const LevelStashes *current_level;
    std::vector< named_thing<item_def> > items;
    std::vector< named_thing<int> > stairs;
    std::vector< named_thing<int> > portals;
    std::vector< named_thing<int> > shops;
    std::vector< named_thing<int> > altars;

private:
    template <class C> void say_any(const C &coll, const char *stub) const;
    template <class citer> bool has_duplicates(citer, citer) const;

    std::string cleaned_feature_description(const coord_def &) const;
    void add_item(const item_def &item);
    void add_stair(const named_thing<int> &stair);
    std::vector<std::string> apply_quantities(
        const std::vector< named_thing<int> > &v) const;
    bool merge_feature(
        std::vector< named_thing<int> > &v,
        const named_thing<int> &feat) const;
};

struct stair_info
{
public:
    enum stair_type
    {
        PHYSICAL,
        PLACEHOLDER
    };

public:
    coord_def position;     // Position of stair
    dungeon_feature_type grid; // Grid feature of the stair.
    level_pos destination;  // The level and the position on the level this
                            // stair leads to. This may be a guess.
    int       distance;     // The distance traveled to reach this stair.
    bool      guessed_pos;  // true if we're not sure that 'destination' is
                            // correct.
    stair_type type;

    stair_info()
        : position(-1, -1), grid(DNGN_FLOOR), destination(),
          distance(-1), guessed_pos(true), type(PHYSICAL)
    {
    }

    void clear_distance() { distance = -1; }

    void save(writer&) const;
    void load(reader&);

    std::string describe() const;

    bool can_travel() const { return (type == PHYSICAL); }
};

void init_exclusion_los();
void update_exclusion_los(const coord_def &p);
void mark_all_excludes_non_updated();

struct travel_exclude
{
    coord_def     pos;       // exclusion centre
    int           radius;    // exclusion radisu
    env_show_grid show;      // los from exclusion centre
    bool          uptodate;  // Is show up to date?

    void set_exclude_show();

    travel_exclude(const coord_def &p, int r = LOS_RADIUS)
        : pos(p), radius(r)
    {
        set_exclude_show();
    }

    int radius_sq() const
    {
        return (radius * radius * 17 / 16);
    }
};

// Information on a level that interlevel travel needs.
struct LevelInfo
{
    LevelInfo() : stairs(), excludes(), stair_distances(), id()
    {
    }

    void save(writer&) const;
    void load(reader&);

    std::vector<stair_info> &get_stairs()
    {
        return stairs;
    }

    stair_info *get_stair(int x, int y);
    stair_info *get_stair(const coord_def &pos);
    bool empty() const;
    bool know_stair(const coord_def &pos) const;
    int get_stair_index(const coord_def &pos) const;

    void clear_distances();
    void set_level_excludes();

    const std::vector<travel_exclude> &get_excludes() const
    {
        return excludes;
    }

    // Returns the travel distance between two stairs. If either stair is NULL,
    // or does not exist in our list of stairs, returns 0.
    int distance_between(const stair_info *s1, const stair_info *s2) const;

    void update();              // Update LevelInfo to be correct for the
                                // current level.

    // Updates/creates a StairInfo for the stair at stairpos in grid coordinates
    void update_stair(const coord_def& stairpos, const level_pos &p,
                      bool guess = false);

    // Clears all stair info for stairs matching this grid type.
    void clear_stairs(dungeon_feature_type grid);

    // Returns true if the given branch is known to be accessible from the
    // current level.
    bool is_known_branch(unsigned char branch) const;

private:
    // Gets a list of coordinates of all player-known stairs on the current
    // level.
    static void get_stairs(std::vector<coord_def> &stairs);

    void correct_stair_list(const std::vector<coord_def> &s);
    void update_stair_distances();
    void sync_all_branch_stairs();
    void sync_branch_stairs(const stair_info *si);
    void fixup();

private:
    std::vector<stair_info> stairs;

    // Squares that are not safe to travel to.
    std::vector<travel_exclude> excludes;

    std::vector<short> stair_distances;  // Dist between stairs
    level_id id;

    friend class TravelCache;

private:
    void create_placeholder_stair(const coord_def &, const level_pos &);
    void resize_stair_distances();
};

const int TRAVEL_WAYPOINT_COUNT = 10;
// Tracks all levels that the player has seen.
class TravelCache
{
public:
    void clear_distances();

    LevelInfo& get_level_info(const level_id &lev)
    {
        LevelInfo &li = levels[lev];
        li.id = lev;
        return li;
    }

    LevelInfo *find_level_info(const level_id &lev)
    {
        std::map<level_id, LevelInfo>::iterator i = levels.find(lev);
        return (i != levels.end()? &i->second : NULL);
    }

    bool know_stair(const coord_def &c) const;
    bool know_level(const level_id &lev) const
    {
        return levels.find(lev) != levels.end();
    }

    const level_pos &get_waypoint(int number) const
    {
        return waypoints[number];
    }

    int get_waypoint_count() const;

    void set_level_excludes();

    void add_waypoint(int x = -1, int y = -1);
    void delete_waypoint();
    unsigned char is_waypoint(const level_pos &lp) const;
    void list_waypoints() const;
    void travel_to_waypoint(int number);
    void update_waypoints() const;


    void update();

    void save(writer&) const;
    void load(reader&);

    bool is_known_branch(unsigned char branch) const;

private:
    void fixup_levels();

private:
    typedef std::map<level_id, LevelInfo> travel_levels_map;
    travel_levels_map levels;
    level_pos waypoints[TRAVEL_WAYPOINT_COUNT];
};

// Handles travel and explore floodfill pathfinding. Does not do interlevel
// travel pathfinding directly (but is used internally by interlevel travel).
// * All coordinates are grid coords.
// * Do not reuse one travel_pathfind for different runmodes.
class travel_pathfind
{
public:
    travel_pathfind();
    virtual ~travel_pathfind();

    // Finds travel direction or explore target.
    coord_def pathfind(run_mode_type rt);

    // For flood-fills (explore), sets starting (seed) square.
    void set_floodseed(const coord_def &seed, bool double_flood = false);

    // For regular travel, set starting point (usually the character's current
    // position) and destination.
    void set_src_dst(const coord_def &src, const coord_def &dst);

    // Request that the point distance array be annotated with magic numbers for
    // excludes and waypoints.
    void set_annotate_map(bool annotate);

    // Sets the travel_distance_grid_t to use instead of travel_point_distance.
    void set_distance_grid(travel_distance_grid_t distgrid);

    // Set feature vector to use; if non-NULL, also sets annotate_map to true.
    void set_feature_vector(std::vector<coord_def> *features);

    // The next square to go to to move towards the travel destination. Return
    // value is undefined if pathfind was not called with RMODE_TRAVEL.
    const coord_def travel_move() const;

    // Square to go to for (greedy) explore. Return value is undefined if
    // pathfind was not called with RMODE_EXPLORE or RMODE_EXPLORE_GREEDY.
    const coord_def explore_target() const;

    // Nearest greed-inducing square. Return value is undefined if
    // pathfind was not called with RMODE_EXPLORE_GREEDY.
    const coord_def greedy_square() const;

    // Nearest unexplored territory. Return value is undefined if
    // pathfind was not called with RMODE_EXPLORE or
    // RMODE_EXPLORE_GREEDY.
    const coord_def unexplored_square() const;

protected:
    bool is_greed_inducing_square(const coord_def &c) const;
    bool path_examine_point(const coord_def &c);
    virtual bool point_traverse_delay(const coord_def &c);
    virtual bool path_flood(const coord_def &c, const coord_def &dc);
    bool square_slows_movement(const coord_def &c);
    void check_square_greed(const coord_def &c);
    void good_square(const coord_def &c);

protected:
    static const int UNFOUND_DIST  = -30000;
    static const int INFINITE_DIST =  INFINITE_DISTANCE;

protected:
    run_mode_type runmode;

    // Where pathfinding starts, and the destination. Note that dest is not
    // relevant for explore!
    coord_def start, dest;

    // This is the square adjacent to the starting position to move
    // along the shortest path to the destination. Does *not* apply
    // for explore!
    coord_def next_travel_move;

    // True if flooding outwards from start square for explore.
    bool floodout, double_flood;

    // Set true in the second part of a double floodfill to completely ignore
    // hostile squares.
    bool ignore_hostile;

    // If true, use magic numbers in point distance array which can be
    // used to colour the level-map.
    bool annotate_map;

    // Stashes on this level (needed for greedy explore and to populate the
    // feature vector with stashes on the X level-map).
    const LevelStashes *ls;

    // Are we greedy exploring?
    bool need_for_greed;

    // Targets for explore and greedy explore.
    coord_def unexplored_place, greedy_place;

    // How far from player's location unexplored_place and greedy_place are.
    int unexplored_dist, greedy_dist;

    const int *refdist;

    // For double-floods, the points to restart floodfill from at the end of
    // the first flood.
    std::vector<coord_def> reseed_points;

    std::vector<coord_def> *features;

    travel_distance_col *point_distance;

    // How many points are we currently considering? We start off with just one
    // point, and spread outwards like a flood-filler.
    int points;

    // How many points we'll consider next iteration.
    int next_iter_points;

    // How far we've traveled from (start_x, start_y), in moves (a diagonal move
    // is no longer than an orthogonal move).
    int traveled_distance;

    // Which index of the circumference array are we currently looking at?
    int circ_index;

    // Used by all instances of travel_pathfind. Happily, we do not need to be
    // re-entrant or thread-safe.
    static FixedVector<coord_def, GXM * GYM> circumference[2];
};

extern TravelCache travel_cache;

#endif // TRAVEL_H
