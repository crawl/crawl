/*
 *  File:       dungeon.cc
 *  Summary:    Functions used when building new levels.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <list>
#include <set>
#include <sstream>
#include <algorithm>
#include <cmath>

#include "abyss.h"
#include "artefact.h"
#include "branch.h"
#include "chardump.h"
#include "cloud.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "defines.h"
#include "dgn-shoals.h"
#include "dgn-swamp.h"
#include "effects.h"
#include "env.h"
#include "enum.h"
#include "map_knowledge.h"
#include "flood_find.h"
#include "fprop.h"
#include "externs.h"
#include "dbg-maps.h"
#include "dbg-scan.h"
#include "directn.h"
#include "dungeon.h"
#include "files.h"
#include "itemprop.h"
#include "items.h"
#include "l_defs.h"
#include "libutil.h"
#include "makeitem.h"
#include "mapdef.h"
#include "mapmark.h"
#include "maps.h"
#include "message.h"
#include "misc.h"
#include "mon-util.h"
#include "mon-place.h"
#include "mgen_data.h"
#include "mon-pathfind.h"
#include "notes.h"
#include "place.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "spells3.h"
#include "spl-book.h"
#include "spl-util.h"
#include "sprint.h"
#include "state.h"
#include "stuff.h"
#include "tags.h"
#include "terrain.h"
#include "traps.h"
#include "travel.h"
#include "hints.h"

#ifdef DEBUG_DIAGNOSTICS
#define DEBUG_TEMPLES
#endif

#ifdef WIZARD
#include "cio.h" // for cancelable_get_line()
#endif

#define YOU_DUNGEON_VAULTS_KEY    "you_dungeon_vaults_key"
#define YOU_PORTAL_VAULT_MAPS_KEY "you_portal_vault_maps_key"

spec_room lua_special_room_spec;
int       lua_special_room_level;

struct dist_feat
{
    int dist;
    dungeon_feature_type feat;

    dist_feat(int _d = 0, dungeon_feature_type _f = DNGN_UNSEEN)
        : dist(_d), feat(_f)
        {
        }
};

const struct coord_def OrthCompass[4] =
{
    coord_def(0, -1), coord_def(0, 1), coord_def(-1, 0), coord_def(1, 0)
};

// FIXME: maintaining previous behaviour, but why are we forbidding the
// last row and column?
coord_def spec_room::random_spot() const
{
    return coord_def( random_range(tl.x, br.x-1), random_range(tl.y, br.y-1) );
}

// DUNGEON BUILDERS
static void _build_dungeon_level(int level_number, int level_type);
static bool _valid_dungeon_level(int level_number, int level_type);

static bool _find_in_area(int sx, int sy, int ex, int ey,
                          dungeon_feature_type feature);
static bool _make_box(int room_x1, int room_y1, int room_x2, int room_y2,
                      dungeon_feature_type floor=DNGN_UNSEEN,
                      dungeon_feature_type wall=DNGN_UNSEEN,
                      dungeon_feature_type avoid=DNGN_UNSEEN);

static builder_rc_type _builder_by_type(int level_number, char level_type);
static builder_rc_type _builder_by_branch(int level_number);
static builder_rc_type _builder_normal(int level_number, char level_type,
                                       spec_room &s);
static builder_rc_type _builder_basic(int level_number);
static void _builder_extras(int level_number, int level_type);
static void _builder_items(int level_number, char level_type, int items_wanted);
static void _builder_monsters(int level_number, char level_type, int mon_wanted);
static void _place_specific_stair(dungeon_feature_type stair,
                                  const std::string &tag = "",
                                  int dl = 0, bool vault_only = false);
static void _place_branch_entrances(int dlevel, char level_type);
static void _place_extra_vaults();
static void _place_minivaults(const std::string &tag = "",
                              int fewest = -1, int most = -1,
                              bool force = false);
static int _place_uniques(int level_number, char level_type);
static void _place_traps( int level_number );
static void _place_fog_machines( int level_number );
static void _prepare_water( int level_number );
static void _check_doors();
static void _hide_doors();
static void _make_trail(int xs, int xr, int ys, int yr,int corrlength,
                        int intersect_chance,
                        int no_corr,
                        int &xbegin, int &ybegin,
                        int &xend, int &yend);
static bool _make_room(int sx,int sy,int ex,int ey,int max_doors, int doorlevel);
static void _place_pool(dungeon_feature_type pool_type, unsigned char pool_x1,
                        unsigned char pool_y1, unsigned char pool_x2,
                        unsigned char pool_y2);
static void _many_pools(dungeon_feature_type pool_type);
static bool _join_the_dots_rigorous(const coord_def &from,
                                    const coord_def &to,
                                    unsigned mapmask,
                                    bool early_exit = false);

static void _build_river(dungeon_feature_type river_type); //mv
static void _build_lake(dungeon_feature_type lake_type); //mv
static void _ruin_level(int ruination = 10, int plant_density = 5);
static void _add_plant_clumps(int frequency = 10, int clump_density = 12,
                              int clump_radius = 4);

static void _bigger_room();
static void _plan_main(int level_number, int force_plan);
static bool _plan_1(int level_number);
static bool _plan_2(int level_number);
static bool _plan_3(int level_number);
static bool _plan_4(char forbid_x1, char forbid_y1, char forbid_x2,
                    char forbid_y2, dungeon_feature_type force_wall);
static bool _plan_5();
static bool _plan_6(int level_number);
static void _portal_vault_level(int level_number);
static void _labyrinth_level(int level_number);
static void _box_room(int bx1, int bx2, int by1, int by2,
                      dungeon_feature_type wall_type);
static int  _box_room_doors( int bx1, int bx2, int by1, int by2, int new_doors);
static void _city_level(int level_number);
static void _diamond_rooms(int level_number);

static void _pick_float_exits(vault_placement &place,
                              std::vector<coord_def> &targets);
static void _connect_vault(const vault_placement &vp);

// ITEM & SHOP FUNCTIONS
static void _place_shops(int level_number);
static object_class_type _item_in_shop(unsigned char shop_type);
static bool _treasure_area(int level_number, unsigned char ta1_x,
                           unsigned char ta2_x, unsigned char ta1_y,
                           unsigned char ta2_y);

// SPECIAL ROOM BUILDERS
static void _special_room(int level_number, spec_room &sr,
                          const map_def *vault);
static void _specr_2(spec_room &sr);
static void _big_room(int level_number);
static void _chequerboard(spec_room &sr, dungeon_feature_type target,
                          dungeon_feature_type floor1,
                          dungeon_feature_type floor2);
static void _roguey_level(int level_number, spec_room &sr, bool make_stairs);

// VAULT FUNCTIONS
static bool _build_secondary_vault(int level_number, const map_def *vault,
                                   int rune_subst = -1,
                                   bool clobber = false,
                                   bool make_no_exits = false,
                                   const coord_def &where = coord_def(-1, -1));
static bool _build_vaults(int level_number,
                          const map_def *vault,
                          int rune_subst = -1, bool build_only = false,
                          bool check_collisions = false,
                          bool make_no_exits = false,
                          const coord_def &where = coord_def(-1, -1));
static void _vault_grid(vault_placement &,
                        int vgrid,
                        const coord_def& where,
                        keyed_mapspec *mapsp);
static void _vault_grid(vault_placement &,
                        int vgrid,
                        const coord_def& where);

static const map_def *_dgn_random_map_for_place(bool minivault);
static void _dgn_load_colour_grid();
static void _dgn_map_colour_fixup();

// ALTAR FUNCTIONS
static int                  _setup_temple_altars(CrawlHashTable &temple);
static dungeon_feature_type _pick_temple_altar(vault_placement &place);
static dungeon_feature_type _pick_an_altar();
static void _place_altar();
static void _place_altars();

static std::vector<god_type> _temple_altar_list;
static CrawlHashTable*       _current_temple_hash = NULL; // XXX: hack!

typedef std::list<coord_def> coord_list;

// MISC FUNCTIONS
static void _dgn_set_floor_colours();
static bool _fixup_interlevel_connectivity();

void dgn_postprocess_level();

//////////////////////////////////////////////////////////////////////////
// Static data

// A mask of vaults and vault-specific flags.
map_mask dgn_Map_Mask;
std::vector<vault_placement> Level_Vaults;
std::vector<vault_placement> Temp_Vaults;
FixedVector<bool, NUM_MONSTERS> temp_unique_creatures;
FixedVector<unique_item_status_type, MAX_UNRANDARTS> temp_unique_items;

std::set<std::string> Level_Unique_Maps;
std::set<std::string> Level_Unique_Tags;
dungeon_feature_set dgn_Vault_Excavatable_Feats;
std::string dgn_Build_Method;
std::string dgn_Layout_Type;

bool Generating_Level = false;

static int can_create_vault = true;
static bool dgn_level_vetoed = false;
static bool use_random_maps  = true;
static bool dgn_check_connectivity = false;
static int  dgn_zones = 0;

static CrawlHashTable _you_vault_list;
static std::string    _portal_vault_map_name;

struct coloured_feature
{
    dungeon_feature_type feature;
    int                  colour;

    coloured_feature() : feature(DNGN_UNSEEN), colour(BLACK) { }
    coloured_feature(dungeon_feature_type f, int c)
        : feature(f), colour(c)
    {
    }
};

struct dgn_colour_override_manager
{
    dgn_colour_override_manager()
    {
        _dgn_load_colour_grid();
    }

    ~dgn_colour_override_manager()
    {
        _dgn_map_colour_fixup();
    }
};

typedef FixedArray< coloured_feature, GXM, GYM > dungeon_colour_grid;
static std::auto_ptr<dungeon_colour_grid> dgn_colour_grid;

typedef std::map<std::string, std::string> callback_map;
static callback_map level_type_post_callbacks;

/**********************************************************************
 * builder() - kickoff for the dungeon generator.
 *********************************************************************/
bool builder(int level_number, int level_type, bool enable_random_maps)
{
    const std::set<std::string> uniq_tags  = you.uniq_map_tags;
    const std::set<std::string> uniq_names = you.uniq_map_names;

    // Save a copy of unique creatures for vetoes.
    temp_unique_creatures = you.unique_creatures;
    // And unrands
    temp_unique_items = you.unique_items;

    unwind_bool levelgen(Generating_Level, true);

    // N tries to build the level, after which we bail with a capital B.
    int tries = 50;
    while (tries-- > 0)
    {
#ifdef DEBUG_DIAGNOSTICS
        mapgen_report_map_build_start();
#endif

        dgn_reset_level(enable_random_maps);

        if (player_in_branch(BRANCH_ECUMENICAL_TEMPLE))
            _setup_temple_altars(you.props);

        // If we're getting low on available retries, disable random vaults
        // and minivaults (special levels will still be placed).
        if (tries < 5)
            use_random_maps = false;

        _build_dungeon_level(level_number, level_type);
        _dgn_set_floor_colours();

#ifdef DEBUG_DIAGNOSTICS
        if (dgn_level_vetoed)
            mapgen_report_map_veto();
#endif

        if ((!dgn_level_vetoed &&
             _valid_dungeon_level(level_number, level_type)) ||
            crawl_state.game_is_sprint())
        {
#ifdef DEBUG_MONS_SCAN
            // If debug_mons_scan() finds a problem while Generating_Level is
            // still true then it will announce that a problem was caused
            // during level generation.
            debug_mons_scan();
#endif

            if (dgn_Build_Method.size() > 0 && dgn_Build_Method[0] == ' ')
                dgn_Build_Method = dgn_Build_Method.substr(1);

            // Save information in the level's properties hash table
            // so we can inlcude it in crash reports.
            env.properties[BUILD_METHOD_KEY] = dgn_Build_Method;
            env.properties[LAYOUT_TYPE_KEY]  = dgn_Layout_Type;
            env.properties[LEVEL_ID_KEY]     = level_id::current().describe();

            // Save information in the player's properties has table so
            // we can include it in the character dump.
            if (!_you_vault_list.empty())
            {
                const std::string lev = level_id::current().describe();
                CrawlHashTable &all_vaults =
                    you.props[YOU_DUNGEON_VAULTS_KEY].get_table();

                CrawlHashTable &this_level = all_vaults[lev].get_table();
                this_level = _you_vault_list;
            }
            else if (!_portal_vault_map_name.empty())
            {
                CrawlVector &vault_maps =
                    you.props[YOU_PORTAL_VAULT_MAPS_KEY].get_vector();
                vault_maps.push_back(_portal_vault_map_name);
            }

            if (you.level_type == LEVEL_PORTAL_VAULT)
            {
                CrawlVector &vault_names =
                    you.props[YOU_PORTAL_VAULT_NAMES_KEY].get_vector();
                vault_names.push_back(you.level_type_name);
            }

            dgn_postprocess_level();

            dgn_Layout_Type.clear();
            Level_Unique_Maps.clear();
            Level_Unique_Tags.clear();
            _dgn_map_colour_fixup();

            return (true);
        }

        you.uniq_map_tags  = uniq_tags;
        you.uniq_map_names = uniq_names;
    }

    if (!crawl_state.map_stat_gen)
    {
        // Failed to build level, bail out.
        save_game(true,
                  make_stringf("Unable to generate level for '%s'!",
                               level_id::current().describe().c_str()).c_str());
    }

    dgn_Layout_Type.clear();
    return (false);
}

// Should be called after a level is constructed to perform any final
// fixups.
void dgn_postprocess_level()
{
    shoals_postprocess_level();
}

void level_welcome_messages()
{
    for (int i = 0, size = Level_Vaults.size(); i < size; ++i)
    {
        const std::vector<std::string> &msgs
            = Level_Vaults[i].map.welcome_messages;
        for (int j = 0, msize = msgs.size(); j < msize; ++j)
            mpr(msgs[j].c_str());
    }
}

void level_clear_vault_memory()
{
    Level_Vaults.clear();
    Temp_Vaults.clear();
    dgn_Map_Mask.init(0);
}

void dgn_flush_map_memory()
{
    you.uniq_map_tags.clear();
    you.uniq_map_names.clear();
}

static void _dgn_load_colour_grid()
{
    dgn_colour_grid.reset(new dungeon_colour_grid);
    dungeon_colour_grid &dcgrid(*dgn_colour_grid);
    for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
            if (env.grid_colours[x][y] != BLACK)
            {
                dcgrid[x][y]
                    = coloured_feature(grd[x][y], env.grid_colours[x][y]);
            }
}

static void _dgn_map_colour_fixup()
{
    if (!dgn_colour_grid.get())
        return;

    // If the original coloured feature has been changed, reset the colour.
    const dungeon_colour_grid &dcgrid(*dgn_colour_grid);
    for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
            if (dcgrid[x][y].colour != BLACK
                && grd[x][y] != dcgrid[x][y].feature
                && (grd[x][y] != DNGN_UNDISCOVERED_TRAP
                    || dcgrid[x][y].feature != DNGN_FLOOR))
            {
                env.grid_colours[x][y] = BLACK;
            }

    dgn_colour_grid.reset(NULL);
}

bool set_level_flags(unsigned long flags, bool silent)
{
    bool could_control = allow_control_teleport(true);
    bool could_map     = player_in_mappable_area();

    unsigned long old_flags = env.level_flags;
    env.level_flags |= flags;

    bool can_control = allow_control_teleport(true);
    bool can_map     = player_in_mappable_area();

    if (you.skills[SK_TRANSLOCATIONS] > 0
        && could_control && !can_control && !silent)
    {
        mpr("You sense the appearance of a powerful magical force "
            "which warps space.", MSGCH_WARN);
    }

    if (could_map && !can_map && !silent)
    {
        mpr("A powerful force appears that prevents you from "
            "remembering where you've been.", MSGCH_WARN);
    }

    return (old_flags != env.level_flags);
}

bool unset_level_flags(unsigned long flags, bool silent)
{
    bool could_control = allow_control_teleport(true);
    bool could_map     = player_in_mappable_area();

    unsigned long old_flags = env.level_flags;
    env.level_flags &= ~flags;

    bool can_control = allow_control_teleport(true);
    bool can_map     = player_in_mappable_area();

    if (you.skills[SK_TRANSLOCATIONS] > 0
        && !could_control && can_control && !silent)
    {
        // Isn't really a "recovery", but I couldn't think of where
        // else to send it.
        mpr("You sense the disappearance of a powerful magical force "
            "which warped space.", MSGCH_RECOVERY);
    }

    if (!could_map && can_map && !silent)
    {
        // Isn't really a "recovery", but I couldn't think of where
        // else to send it.
        mpr("You sense the disappearance of the force that prevented you "
            "from remembering where you've been.", MSGCH_RECOVERY);
    }

    return (old_flags != env.level_flags);
}

void dgn_set_grid_colour_at(const coord_def &c, int colour)
{
    if (colour != BLACK)
    {
        env.grid_colours(c) = colour;
        if (!dgn_colour_grid.get())
            dgn_colour_grid.reset(new dungeon_colour_grid);

        (*dgn_colour_grid)(c) = coloured_feature(grd(c), colour);
    }
}

void dgn_register_vault(const map_def &map)
{
    if (!map.has_tag("allow_dup"))
        you.uniq_map_names.insert(map.name);

    if (map.has_tag("luniq"))
        Level_Unique_Maps.insert(map.name);

    std::vector<std::string> tags = split_string(" ", map.tags);
    for (int t = 0, ntags = tags.size(); t < ntags; ++t)
    {
        const std::string &tag = tags[t];
        if (tag.find("uniq_") == 0)
            you.uniq_map_tags.insert(tag);
        else if (tag.find("luniq_") == 0)
            Level_Unique_Tags.insert(tag);
    }
}

bool dgn_square_travel_ok(const coord_def &c)
{
    const dungeon_feature_type feat = grd(c);
    return (feat_is_traversable(feat) || feat_is_trap(feat)
            || feat == DNGN_SECRET_DOOR);
}

bool dgn_square_is_passable(const coord_def &c)
{
    // [enne] Why does this function check MMT_OPAQUE?
    //
    // Don't peek inside MMT_OPAQUE vaults (all vaults are opaque by
    // default) because vaults may choose to create isolated regions,
    // or otherwise cause connectivity issues even if the map terrain
    // is travel-passable.
    return (!(dgn_Map_Mask(c) & MMT_OPAQUE) && (dgn_square_travel_ok(c)));
}

static inline void _dgn_point_record_stub(const coord_def &) { }

template <class point_record>
static bool _dgn_fill_zone(
    const coord_def &start, int zone,
    point_record &record_point,
    bool (*passable)(const coord_def &) = dgn_square_is_passable,
    bool (*iswanted)(const coord_def &) = NULL)
{
    bool ret = false;
    std::list<coord_def> points[2];
    int cur = 0;

    // No bounds checks, assuming the level has at least one layer of
    // rock border.

    for (points[cur].push_back(start); !points[cur].empty(); )
    {
        for (std::list<coord_def>::const_iterator i = points[cur].begin();
             i != points[cur].end(); ++i)
        {
            const coord_def &c(*i);

            travel_point_distance[c.x][c.y] = zone;

            if (iswanted && iswanted(c))
                ret = true;

            for (int yi = -1; yi <= 1; ++yi)
                for (int xi = -1; xi <= 1; ++xi)
                {
                    if (!xi && !yi)
                        continue;

                    const coord_def cp(c.x + xi, c.y + yi);
                    if (!map_bounds(cp)
                        || travel_point_distance[cp.x][cp.y] || !passable(cp))
                    {
                        continue;
                    }

                    travel_point_distance[cp.x][cp.y] = zone;
                    record_point(cp);
                    points[!cur].push_back(cp);
                }
        }

        points[cur].clear();
        cur = !cur;
    }
    return (ret);
}

static bool _is_perm_down_stair(const coord_def &c)
{
    switch (grd(c))
    {
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
    case DNGN_EXIT_HELL:
    case DNGN_EXIT_PANDEMONIUM:
    case DNGN_TRANSIT_PANDEMONIUM:
    case DNGN_EXIT_ABYSS:
        return (true);
    default:
        return (false);
    }
}

static bool _is_bottom_exit_stair(const coord_def &c)
{
    // Is this a valid exit stair from the bottom of a branch? In general,
    // ensure that each region has a stone stair up.
    switch (grd(c))
    {
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
    case DNGN_EXIT_HELL:
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
    case DNGN_EXIT_PANDEMONIUM:
    case DNGN_TRANSIT_PANDEMONIUM:
    case DNGN_EXIT_ABYSS:
        return (true);
    default:
        return (false);
    }
}

static bool _is_exit_stair(const coord_def &c)
{
    // Branch entries, portals, and abyss entries are not considered exit
    // stairs here, as they do not provide an exit (in a transitive sense) from
    // the current level.
    switch (grd(c))
    {
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
    case DNGN_ESCAPE_HATCH_DOWN:
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
    case DNGN_ESCAPE_HATCH_UP:
    case DNGN_EXIT_HELL:
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
    case DNGN_EXIT_PANDEMONIUM:
    case DNGN_TRANSIT_PANDEMONIUM:
    case DNGN_EXIT_ABYSS:
        return (true);
    default:
        return (false);
    }
}

// Counts the number of mutually unreachable areas in the map,
// excluding isolated zones within vaults (we assume the vault author
// knows what she's doing). This is an easy way to check whether a map
// has isolated parts of the level that were not formerly isolated.
//
// All squares within vaults are treated as non-reachable, to simplify
// life, because vaults may change the level layout and isolate
// different areas without changing the number of isolated areas.
// Here's a before and after example of such a vault that would cause
// problems if we considered floor in the vault as non-isolating (the
// vault is represented as V for walls and o for floor squares in the
// vault).
//
// Before:
//
//   xxxxx    xxxxx
//   x<..x    x.2.x
//   x.1.x    xxxxx  3 isolated zones
//   x>..x    x.3.x
//   xxxxx    xxxxx
//
// After:
//
//   xxxxx    xxxxx
//   x<1.x    x.2.x
//   VVVVVVVVVVoooV  3 isolated zones, but the isolated zones are different.
//   x>3.x    x...x
//   xxxxx    xxxxx
//
// If count_stairless is true, returns the number of regions that have no
// stairs in them.
//
// If fill is non-zero, it fills any disconnected regions with fill.
//
int process_disconnected_zones(int x1, int y1, int x2, int y2,
                               bool choose_stairless,
                               dungeon_feature_type fill)
{
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));
    int nzones = 0;
    int ngood = 0;
    for (int y = y1; y <= y2 ; ++y)
        for (int x = x1; x <= x2; ++x)
        {
            if (!map_bounds(x, y)
                || travel_point_distance[x][y]
                || !dgn_square_is_passable(coord_def(x, y)))
            {
                continue;
            }

            bool found_exit_stair =
                _dgn_fill_zone(coord_def(x, y), ++nzones,
                               _dgn_point_record_stub,
                               dgn_square_is_passable,
                               choose_stairless ? (at_branch_bottom() ?
                               _is_bottom_exit_stair :
                               _is_exit_stair) : NULL);

            // If we want only stairless zones, screen out zones that did
            // have stairs.
            if (choose_stairless && found_exit_stair)
                ++ngood;
            else if (fill)
            {
                for (int fy = y1; fy <= y2 ; ++fy)
                    for (int fx = x1; fx <= x2; ++fx)
                        if (travel_point_distance[fx][fy] == nzones)
                            grd[fx][fy] = fill;
            }
        }

    return (nzones - ngood);
}

static int _dgn_count_disconnected_zones(bool choose_stairless)
{
    return process_disconnected_zones(0, 0, GXM-1, GYM-1, choose_stairless,
                                      (dungeon_feature_type)0);
}

static void _fixup_pandemonium_stairs()
{
    for (int i = 0; i < GXM; i++)
        for (int j = 0; j < GYM; j++)
        {
            if (grd[i][j] >= DNGN_STONE_STAIRS_UP_I
                && grd[i][j] <= DNGN_ESCAPE_HATCH_UP)
            {
                if (one_chance_in(50))
                    grd[i][j] = DNGN_EXIT_PANDEMONIUM;
                else
                    grd[i][j] = DNGN_FLOOR;
            }

            if (grd[i][j] >= DNGN_ENTER_LABYRINTH
                && grd[i][j] <= DNGN_ESCAPE_HATCH_DOWN)
            {
                grd[i][j] = DNGN_TRANSIT_PANDEMONIUM;
            }
        }
}

static void _mask_vault(const vault_placement &place, unsigned mask)
{
    for (int y = place.pos.y + place.size.y - 1; y >= place.pos.y; --y)
        for (int x = place.pos.x + place.size.x - 1; x >= place.pos.x; --x)
        {
            if (place.map.in_map(coord_def(x - place.pos.x, y - place.pos.y)))
                dgn_Map_Mask[x][y] |= mask;
        }
}

void dgn_register_place(const vault_placement &place, bool register_vault)
{
    if (register_vault)
        dgn_register_vault(place.map);

    if (!place.map.has_tag("layout"))
    {
        if (place.map.orient == MAP_ENCOMPASS)
        {
            for (rectangle_iterator ri(0); ri; ++ri)
                dgn_Map_Mask(*ri) |= MMT_VAULT | MMT_NO_DOOR;
        }
        else
        {
            _mask_vault(place, MMT_VAULT | MMT_NO_DOOR);
        }

        if (!place.map.has_tag("transparent"))
            _mask_vault(place, MMT_OPAQUE);
    }

    if (place.map.has_tag("no_monster_gen"))
        _mask_vault(place, MMT_NO_MONS);

    if (place.map.has_tag("no_item_gen"))
        _mask_vault(place, MMT_NO_ITEM);

    if (place.map.has_tag("no_pool_fixup"))
        _mask_vault(place, MMT_NO_POOL);

    if (place.map.has_tag("no_wall_fixup"))
        _mask_vault(place, MMT_NO_WALL);

    if (place.map.has_tag("no_shop_gen"))
        _mask_vault(place, MMT_NO_SHOP);

    if (place.map.has_tag("no_trap_gen"))
        _mask_vault(place, MMT_NO_TRAP);

    // Now do per-square by-symbol masking.
    for (int y = place.pos.y + place.size.y - 1; y >= place.pos.y; --y)
        for (int x = place.pos.x + place.size.x - 1; x >= place.pos.x; --x)
            if (place.map.in_map(coord_def(x - place.pos.x, y - place.pos.y)))
            {
                coord_def c(x - place.pos.x, y - place.pos.y);
                const keyed_mapspec* spec = place.map.mapspec_at(c);

                if (spec != NULL)
                {
                    dgn_Map_Mask[x][y] |= (short)spec->map_mask.flags_set;
                    dgn_Map_Mask[x][y] &= ~((short)spec->map_mask.flags_unset);
                }
            }

    set_branch_flags(place.map.branch_flags.flags_set, true);
    unset_branch_flags(place.map.branch_flags.flags_unset, true);

    set_level_flags(place.map.level_flags.flags_set, true);
    unset_level_flags(place.map.level_flags.flags_unset, true);

    if (place.map.floor_colour != BLACK)
        env.floor_colour = place.map.floor_colour;

    if (place.map.rock_colour != BLACK)
        env.rock_colour = place.map.rock_colour;

#ifdef USE_TILE
    if (place.map.rock_tile != 0)
        env.tile_default.wall = place.map.rock_tile;

    if (place.map.floor_tile != 0)
        env.tile_default.floor = place.map.floor_tile;
#endif
}

bool dgn_ensure_vault_placed(bool vault_success,
                             bool disable_further_vaults)
{
    if (!vault_success)
        dgn_level_vetoed = true;
    else if (disable_further_vaults)
        can_create_vault = false;
    return (vault_success);
}

static bool _ensure_vault_placed_ex( bool vault_success, const map_def *vault )
{
    return dgn_ensure_vault_placed( vault_success,
                                    (!vault->has_tag("extra")
                                     && vault->orient == MAP_ENCOMPASS) );
}

static coord_def _find_level_feature(int feat)
{
    for (int y = 1; y < GYM; ++y)
        for (int x = 1; x < GXM; ++x)
        {
            if (grd[x][y] == feat)
                return coord_def(x, y);
        }

    return coord_def(0, 0);
}

static bool _has_connected_stone_stairs_from(const coord_def &c)
{

    flood_find<feature_grid, coord_predicate> ff(env.grid, in_bounds);
    ff.add_feat(DNGN_STONE_STAIRS_DOWN_I);
    ff.add_feat(DNGN_STONE_STAIRS_DOWN_II);
    ff.add_feat(DNGN_STONE_STAIRS_DOWN_III);
    ff.add_feat(DNGN_STONE_STAIRS_UP_I);
    ff.add_feat(DNGN_STONE_STAIRS_UP_II);
    ff.add_feat(DNGN_STONE_STAIRS_UP_III);

    coord_def where = ff.find_first_from(c, dgn_Map_Mask);
    return (where.x || !ff.did_leave_vault());
}

static bool _has_connected_downstairs_from(const coord_def &c)
{
    flood_find<feature_grid, coord_predicate> ff(env.grid, in_bounds);
    ff.add_feat(DNGN_STONE_STAIRS_DOWN_I);
    ff.add_feat(DNGN_STONE_STAIRS_DOWN_II);
    ff.add_feat(DNGN_STONE_STAIRS_DOWN_III);
    ff.add_feat(DNGN_ESCAPE_HATCH_DOWN);

    coord_def where = ff.find_first_from(c, dgn_Map_Mask);
    return (where.x || !ff.did_leave_vault());
}

static bool _is_level_stair_connected()
{
    coord_def up = _find_level_feature(DNGN_STONE_STAIRS_UP_I);
    if (up.x && up.y)
        return _has_connected_downstairs_from(up);

    return (false);
}

static bool _valid_dungeon_level(int level_number, int level_type)
{
    if (level_number == 0 && level_type == LEVEL_DUNGEON)
        return _is_level_stair_connected();

    return (true);
}

static void _dgn_init_vault_excavatable_feats()
{
    dgn_Vault_Excavatable_Feats.clear();
    dgn_Vault_Excavatable_Feats.insert(DNGN_ROCK_WALL);
}

void dgn_veto_level()
{
    dgn_level_vetoed = true;
}

void dgn_reset_level(bool enable_random_maps)
{
    dgn_level_vetoed = false;
    Level_Unique_Maps.clear();
    Level_Unique_Tags.clear();

    you.unique_creatures = temp_unique_creatures;
    you.unique_items = temp_unique_items;

    _portal_vault_map_name.clear();
    _you_vault_list.clear();
    dgn_Build_Method.clear();
    dgn_Layout_Type.clear();
    level_clear_vault_memory();
    dgn_colour_grid.reset(NULL);

    _dgn_init_vault_excavatable_feats();

    can_create_vault = true;
    use_random_maps  = enable_random_maps;
    dgn_check_connectivity = false;
    dgn_zones        = 0;

    _temple_altar_list.clear();
    _current_temple_hash = NULL;

    // Forget level properties.
    env.properties.clear();
    env.heightmap.reset(NULL);

    // Set up containers for storing some level generation info.
    env.properties[LEVEL_VAULTS_KEY].new_table();
    env.properties[TEMP_VAULTS_KEY].new_table();
    env.properties[LEVEL_EXTRAS_KEY].new_vector(SV_STR);

    // Blank level with DNGN_ROCK_WALL.
    env.grid.init(DNGN_ROCK_WALL);
    env.pgrid.init(0);
    env.grid_colours.init(BLACK);
    env.map_knowledge.init(map_cell());

    // Delete all traps.
    for (int i = 0; i < MAX_TRAPS; i++)
        env.trap[i].type = TRAP_UNASSIGNED;

    // Initialise all items.
    for (int i = 0; i < MAX_ITEMS; i++)
        init_item( i );

    // Reset all monsters.
    for (int i = 0; i < MAX_MONSTERS; i++)
        menv[i].reset();

    env.mons_alloc.init(MONS_NO_MONSTER);

    // Zap clouds
    env.cgrid.init(EMPTY_CLOUD);

    const cloud_struct empty;
    env.cloud.init(empty);

    mgrd.init(NON_MONSTER);
    igrd.init(NON_ITEM);

    // Reset all shops.
    for (int shcount = 0; shcount < MAX_SHOPS; shcount++)
        env.shop[shcount].type = SHOP_UNASSIGNED;

    // Clear all markers.
    env.markers.clear();

    // Set default level flags.
    if (you.level_type == LEVEL_DUNGEON)
        env.level_flags = branches[you.where_are_you].default_level_flags;
    else if (you.level_type == LEVEL_LABYRINTH || you.level_type == LEVEL_ABYSS)
    {
        env.level_flags = LFLAG_NO_TELE_CONTROL | LFLAG_NO_MAGIC_MAP;

        // Labyrinths are now mappable, but come with heavy map rot. (jpeg)
        if (you.level_type == LEVEL_ABYSS)
            env.level_flags |= LFLAG_NOT_MAPPABLE;
    }
    else
        env.level_flags = 0;

    // Set default random monster generation rate (smaller is more often,
    // except that 0 == no random monsters).
    if (you.level_type == LEVEL_DUNGEON)
    {
        if (you.where_are_you == BRANCH_ECUMENICAL_TEMPLE
            || crawl_state.game_is_tutorial())
        {
            // No random monsters in tutorial or ecu temple
            env.spawn_random_rate = 0;
        }
        else
            env.spawn_random_rate = 240;
    }
    else if (you.level_type == LEVEL_ABYSS
             || you.level_type == LEVEL_PANDEMONIUM)
    {
        // Abyss spawn rate is set for those characters that start out in the
        // Abyss; otherwise the number is ignored in the Abyss.
        env.spawn_random_rate = 50;
    }
    else
        // No random monsters in Labyrinths and portal vaualts.
        env.spawn_random_rate = 0;

    env.floor_colour = BLACK;
    env.rock_colour  = BLACK;

    // Clear exclusions
    clear_excludes();

#ifdef USE_TILE
    // Clear custom tile settings from vaults
    tile_init_default_flavour();
    tile_clear_flavour();
#endif

    lua_special_room_spec.created = false;
    lua_special_room_spec.tl.set(-1, -1);
    lua_special_room_level = -1;
}

static void _build_layout_skeleton(int level_number, int level_type,
                                   spec_room &sr)
{
    builder_rc_type skip_build = _builder_by_type(level_number, level_type);

    if (skip_build == BUILD_QUIT)       // quit immediately
        return;

    if (skip_build == BUILD_CONTINUE)
    {
        skip_build = _builder_by_branch(level_number);

        if (skip_build == BUILD_QUIT || dgn_level_vetoed)
            return;
    }

    if (!dgn_level_vetoed && skip_build == BUILD_CONTINUE)
    {
        // Do 'normal' building.  Well, except for the swamp and shoals.
        if (!player_in_branch(BRANCH_SWAMP) && !player_in_branch(BRANCH_SHOALS))
            skip_build = _builder_normal(level_number, level_type, sr);

        if (!dgn_level_vetoed && skip_build == BUILD_CONTINUE)
        {
            skip_build = _builder_basic(level_number);
            if (!dgn_level_vetoed && skip_build == BUILD_CONTINUE)
                _builder_extras(level_number, level_type);
        }
    }
}

static int _num_items_wanted(int level_number)
{
    if (level_number > 5 && one_chance_in(500 - 5 * level_number))
        return (10 + random2avg(90, 2)); // rich level!
    else
        return (3 + roll_dice(3, 11));
}


static int _num_mons_wanted(int level_type)
{
    if (level_type == LEVEL_ABYSS
        || player_in_branch(BRANCH_ECUMENICAL_TEMPLE))
    {
        return 0;
    }

    int mon_wanted = roll_dice( 3, 10 );

    if (player_in_hell())
        mon_wanted += roll_dice( 3, 8 );
    else if (player_in_branch(BRANCH_HALL_OF_BLADES))
        mon_wanted += roll_dice( 6, 8 );

    if (mon_wanted > 60)
        mon_wanted = 60;

    return mon_wanted;
}

static void _fixup_walls()
{
    // If level part of Dis -> all walls metal.
    // If part of vaults -> walls depend on level.
    // If part of crypt -> all walls stone.

    if (player_in_branch(BRANCH_DIS)
        || player_in_branch(BRANCH_VAULTS)
        || player_in_branch(BRANCH_CRYPT))
    {
        // Always the case with Dis {dlb}
        dungeon_feature_type vault_wall = DNGN_METAL_WALL;

        if (player_in_branch(BRANCH_VAULTS))
        {
            vault_wall = DNGN_ROCK_WALL;
            const int bdepth = player_branch_depth();

            if (bdepth > 2)
                vault_wall = DNGN_STONE_WALL;

            if (bdepth > 4)
                vault_wall = DNGN_METAL_WALL;

            if (bdepth > 6 && one_chance_in(10))
                vault_wall = DNGN_GREEN_CRYSTAL_WALL;
        }
        else if (player_in_branch(BRANCH_CRYPT))
            vault_wall = DNGN_STONE_WALL;

        dgn_replace_area(0, 0, GXM-1, GYM-1, DNGN_ROCK_WALL, vault_wall,
                         MMT_NO_WALL);
    }
}

// Remove any items that are on squares that items should not be on.
// link_items() must be called after this function.
static void _fixup_misplaced_items()
{
    for (int i = 0; i < MAX_ITEMS; i++)
    {
        item_def& item(mitm[i]);
        if (!item.is_valid() || (item.pos.x == 0)
            || item.held_by_monster())
        {
            continue;
        }

        if (in_bounds(item.pos))
        {
            dungeon_feature_type feat = grd(item.pos);
            if (feat >= DNGN_MINITEM)
                continue;

            dprf("Item buggily placed in feature at (%d, %d).",
                 item.pos.x, item.pos.y);
        }
        else
        {
            dprf("Item buggily placed out of bounds at (%d, %d).",
                 item.pos.x, item.pos.y);
        }

        // Can't just unlink item because it might not have been linked yet.
        item.base_type = OBJ_UNASSIGNED;
        item.quantity = 0;
        item.pos.reset();
    }
}

static void _fixup_branch_stairs()
{
    // Top level of branch levels - replaces up stairs
    // with stairs back to dungeon or wherever:
    if (your_branch().exit_stairs != NUM_FEATURES
        && player_branch_depth() == 1
        && you.level_type == LEVEL_DUNGEON)
    {
        const dungeon_feature_type exit = your_branch().exit_stairs;
        for (rectangle_iterator ri(1); ri; ++ri)
        {
            if (grd(*ri) >= DNGN_STONE_STAIRS_UP_I
                && grd(*ri) <= DNGN_ESCAPE_HATCH_UP)
            {
                if (grd(*ri) == DNGN_STONE_STAIRS_UP_I)
                    env.markers.add(new map_feature_marker(*ri, grd(*ri)));

                grd(*ri) = exit;
            }
        }
    }

    if (at_branch_bottom() && you.level_type == LEVEL_DUNGEON)
    {
        // Bottom level of branch - wipes out down stairs.
        for (rectangle_iterator ri(1); ri; ++ri)
        {
            if (grd(*ri) >= DNGN_STONE_STAIRS_DOWN_I
                && grd(*ri) <= DNGN_ESCAPE_HATCH_DOWN)
            {
                grd(*ri) = DNGN_FLOOR;
            }
        }
    }
}

static bool _fixup_stone_stairs(bool preserve_vault_stairs)
{
    // This function ensures that there is exactly one each up and down
    // stone stairs I, II, and III.  More than three stairs will result in
    // turning additional stairs into escape hatches (with an attempt to keep
    // level connectivity).  Fewer than three stone stairs will result in
    // random placement of new stairs.

    const unsigned int max_stairs = 20;
    FixedVector<coord_def, max_stairs> up_stairs;
    FixedVector<coord_def, max_stairs> down_stairs;
    unsigned int num_up_stairs   = 0;
    unsigned int num_down_stairs = 0;

    for (int x = 1; x < GXM; ++x)
        for (int y = 1; y < GYM; ++y)
        {
            const coord_def c(x,y);
            if (grd(c) >= DNGN_STONE_STAIRS_DOWN_I
                && grd(c) <= DNGN_STONE_STAIRS_DOWN_III
                && num_down_stairs < max_stairs)
            {
                down_stairs[num_down_stairs++] = c;
            }
            else if (grd(c) >= DNGN_STONE_STAIRS_UP_I
                     && grd(c) <= DNGN_STONE_STAIRS_UP_III
                     && num_up_stairs < max_stairs)
            {
                up_stairs[num_up_stairs++] = c;
            }
        }

    bool success = true;

    for (unsigned int i = 0; i < 2; i++)
    {
        FixedVector<coord_def, max_stairs>& stair_list = (i == 0 ? up_stairs
                                                                 : down_stairs);

        unsigned int num_stairs;
        dungeon_feature_type base;
        dungeon_feature_type replace;
        if (i == 0)
        {
            num_stairs = num_up_stairs;
            replace = DNGN_FLOOR;
            base = DNGN_STONE_STAIRS_UP_I;
        }
        else
        {
            num_stairs = num_down_stairs;
            replace = DNGN_FLOOR;
            base = DNGN_STONE_STAIRS_DOWN_I;
        }

        // In Zot, don't create extra escape hatches, in order to force
        // the player through vaults that use all three down stone stairs.
        if (player_in_branch(BRANCH_HALL_OF_ZOT))
            replace = DNGN_GRANITE_STATUE;

        if (num_stairs > 3)
        {
            // Find pairwise stairs that are connected and turn one of them
            // into an escape hatch of the appropriate type.
            for (unsigned int s1 = 0; s1 < num_stairs; s1++)
            {
                if (num_stairs <= 3)
                    break;

                for (unsigned int s2 = s1 + 1; s2 < num_stairs; s2++)
                {
                    if (num_stairs <= 3)
                        break;

                    if (preserve_vault_stairs
                        && (dgn_Map_Mask(stair_list[s2]) & MMT_VAULT))
                    {
                        continue;
                    }

                    flood_find<feature_grid, coord_predicate> ff(env.grid,
                                                                 in_bounds);

                    ff.add_feat(grd(stair_list[s2]));

                    // Ensure we're not searching for the feature at s1.
                    dungeon_feature_type save = grd(stair_list[s1]);
                    grd(stair_list[s1]) = DNGN_FLOOR;

                    coord_def where = ff.find_first_from(stair_list[s1],
                        dgn_Map_Mask);
                    if (where.x)
                    {
                        grd(stair_list[s2]) = replace;
                        num_stairs--;
                        stair_list[s2] = stair_list[num_stairs];
                        s2--;
                    }

                    grd(stair_list[s1]) = save;
                }
            }

            // If that doesn't work, remove random stairs.
            while (num_stairs > 3)
            {
                int remove = random2(num_stairs);
                if (preserve_vault_stairs)
                {
                    int start = remove;
                    do
                    {
                        if (!(dgn_Map_Mask(stair_list[remove]) & MMT_VAULT))
                            break;
                        remove = (remove + 1) % num_stairs;
                    }
                    while (start != remove);

                    // If we looped through all possibilities, then it
                    // means that there are more than 3 stairs in vaults and
                    // we can't preserve vault stairs.
                    if (start == remove)
                        break;
                }
                grd(stair_list[remove]) = replace;

                stair_list[remove] = stair_list[--num_stairs];
            }
        }

        if (num_stairs > 3 && preserve_vault_stairs)
        {
            success = false;
            continue;
        }

        // If there are no stairs, it's either a branch entrance or exit.
        // If we somehow have ended up in a catastrophic "no stairs" state,
        // the level will not be validated, so we do not need to catch it here.
        if (num_stairs == 0)
            continue;

        // Add extra stairs to get to exactly three.
        for (int s = num_stairs; s < 3; s++)
        {
            coord_def gc;
            do
            {
                gc.x = random2(GXM);
                gc.y = random2(GYM);
            }
            while (grd(gc) != DNGN_FLOOR);
            dprf("add stair %d at pos(%d, %d)", s, gc.x, gc.y);
            // base gets fixed up to be the right stone stair below...
            grd(gc) = base;
            stair_list[num_stairs++] = gc;
        }

        ASSERT(num_stairs == 3);

        // Ensure uniqueness of three stairs.
        for (int s = 0; s < 4; s++)
        {
            int s1 = s % num_stairs;
            int s2 = (s1 + 1) % num_stairs;
            ASSERT(grd(stair_list[s2]) >= base
                   && grd(stair_list[s2]) < base + 3);

            if (grd(stair_list[s1]) == grd(stair_list[s2]))
            {
                grd(stair_list[s2]) = (dungeon_feature_type)
                    (base + (grd(stair_list[s2])-base+1) % 3);
            }
        }
    }

    return success;
}

static bool _add_feat_if_missing(bool (*iswanted)(const coord_def &),
    dungeon_feature_type feat)
{
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));
    int nzones = 0;
    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
        {
            // [ds] Use dgn_square_is_passable instead of
            // dgn_square_travel_ok here, for we'll otherwise
            // fail on floorless isolated pocket in vaults (like the
            // altar surrounded by deep water), and trigger the assert
            // downstairs.
            const coord_def gc(x, y);
            if (!map_bounds(x, y)
                || travel_point_distance[x][y]
                || !dgn_square_is_passable(gc))
            {
                continue;
            }

            if (_dgn_fill_zone(gc, ++nzones, _dgn_point_record_stub,
                               dgn_square_is_passable, iswanted))
            {
                continue;
            }

            bool found_feature = false;
            for (int y2 = 0; y2 < GYM && !found_feature; ++y2)
                for (int x2 = 0; x2 < GXM && !found_feature; ++x2)
                {
                    if (grd[x2][y2] == feat)
                        found_feature = true;
                }

            if (found_feature)
                continue;

            int i = 0;
            while (i++ < 2000)
            {
                coord_def rnd(random2(GXM), random2(GYM));
                if (grd(rnd) != DNGN_FLOOR)
                    continue;

                if (travel_point_distance[rnd.x][rnd.y] != nzones)
                    continue;

                grd(rnd) = feat;
                return (true);
            }

            for (int y2 = 0; y2 < GYM; ++y2)
                for (int x2 = 0; x2 < GXM; ++x2)
                {
                    coord_def rnd(x2, y2);
                    if (grd(rnd) != DNGN_FLOOR)
                        continue;

                    if (travel_point_distance[rnd.x][rnd.y] != nzones)
                        continue;

                    grd(rnd) = feat;
                    return (true);
                }

#ifdef DEBUG_DIAGNOSTICS
            dump_map("debug.map", true, true);
#endif
            // [ds] Too many normal cases trigger this ASSERT, including
            // rivers that surround a stair with deep water.
            // ASSERT(!"Couldn't find region.");
            return (false);
        }

    return (true);
}

static bool _add_connecting_escape_hatches()
{
    // For any regions without a down stone stair case, add an
    // escape hatch.  This will always allow (downward) progress.

    if (branches[you.where_are_you].branch_flags & BFLAG_ISLANDED)
        return (true);
    if (you.level_type != LEVEL_DUNGEON)
        return (true);

    if (at_branch_bottom())
        return (_dgn_count_disconnected_zones(true) == 0);

    return (_add_feat_if_missing(_is_perm_down_stair, DNGN_ESCAPE_HATCH_DOWN));
}

static bool _branch_entrances_are_connected()
{
    // Returns true if all branch entrances on the level are connected to
    // stone stairs.
    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
        {
            coord_def gc(x,y);
            if (!feat_is_branch_stairs(grd(gc)))
                continue;
            if (!_has_connected_stone_stairs_from(gc))
                return (false);
        }

    return (true);
}

static void _dgn_verify_connectivity(unsigned nvaults)
{
    if (dgn_level_vetoed)
        return;

    // After placing vaults, make sure parts of the level have not been
    // disconnected.
    if (dgn_zones && nvaults != Level_Vaults.size())
    {
        const int newzones = _dgn_count_disconnected_zones(false);

#ifdef DEBUG_DIAGNOSTICS
        std::ostringstream vlist;
        for (unsigned i = nvaults; i < Level_Vaults.size(); ++i)
        {
            if (i > nvaults)
                vlist << ", ";
            vlist << Level_Vaults[i].map.name;
        }
        mprf(MSGCH_DIAGNOSTICS, "Dungeon has %d zones after placing %s.",
             newzones, vlist.str().c_str());
#endif
        if (newzones > dgn_zones)
        {
            dgn_level_vetoed = true;
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS,
                 "VETO: %s broken by [%s] (had %d zones, "
                 "now have %d zones.",
                 level_id::current().describe().c_str(),
                 vlist.str().c_str(), dgn_zones, newzones);
#endif
            return;
        }
    }

    // Also check for isolated regions that have no stairs.
    if (you.level_type == LEVEL_DUNGEON
        && !(branches[you.where_are_you].branch_flags & BFLAG_ISLANDED)
        && _dgn_count_disconnected_zones(true) > 0)
    {
        dgn_level_vetoed = true;
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS,
             "VETO: %s has isolated areas with no stairs.",
             level_id::current().describe().c_str());
#endif
        return;
    }

    if (!_fixup_stone_stairs(true))
    {
        dprf("Warning: failed to preserve vault stairs.");
        if (!_fixup_stone_stairs(false))
        {
            dgn_level_vetoed = true;
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS,
                 "VETO: Failed to fix stone stairs: %s.",
                 level_id::current().describe().c_str());
#endif
            return;
        }
    }

    if (!_branch_entrances_are_connected())
    {
        dgn_level_vetoed = true;
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS,
             "VETO: %s has a disconnected branch entrance.",
             level_id::current().describe().c_str());
#endif
        return;
    }

    if (!_add_connecting_escape_hatches())
    {
        dgn_level_vetoed = true;
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS,
             "VETO: %s failed to get connecting escape hatches.",
             level_id::current().describe().c_str());
#endif
        return;
    }

    if (!_fixup_interlevel_connectivity())
    {
        dgn_level_vetoed = true;
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS,
             "VETO: %s failed to ensure interlevel connectivity.",
             level_id::current().describe().c_str());
#endif
        return;
    }
}

// Structure of OVERFLOW_TEMPLES:
//
// * A vector, with one cell per dungeon level (unset if there's no
//   overflow temples on that level).
//
// * The cell of the previous vector is a vector, with one overlfow
//   temple definition per cell.
//
// * The cell of the previous vector is a hash table, containing the
//   list of gods for the overflow temple and (optionally) the name of
//   the vault to use for the temple.  If no map name is supplied,
//   it will randomly pick from vaults tagged "temple_overflow_num",
//   where "num" is the number of gods in the temple.  Gods are listed
//   in the order their altars are placed.
static void _build_overflow_temples(int level_number)
{
    if (!you.props.exists(OVERFLOW_TEMPLES_KEY))
        // Levels built while in testing mode.
        return;

    CrawlVector &levels = you.props[OVERFLOW_TEMPLES_KEY].get_vector();

    // Are we deeper than the last overflow temple?
    if (level_number >= levels.size())
        return;

    CrawlStoreValue &val = levels[level_number];

    // Does this level have an overflow temple?
    if (val.get_flags() & SFLAG_UNSET)
        return;

    CrawlVector &temples = val.get_vector();

    if (temples.size() == 0)
        return;

    if (!can_create_vault)
    {
        mpr("WARNING: Overriding can_create_vault",
            MSGCH_DIAGNOSTICS);
        can_create_vault = true;
    }

    for (unsigned int i = 0; i < temples.size(); i++)
    {
        CrawlHashTable &temple = temples[i].get_table();

        const int num_gods = _setup_temple_altars(temple);

        const map_def *vault = NULL;

        if (temple.exists(TEMPLE_MAP_KEY))
        {
            std::string name = temple[TEMPLE_MAP_KEY].get_string();

            vault = find_map_by_name(name);
            if (vault == NULL)
            {
                mprf(MSGCH_ERROR,
                     "Couldn't find overflow temple map '%s'!",
                     name.c_str());
            }
        }
        else
        {
            std::string vault_tag;

            // For a single-altar temple, first try to find a temple specialized
            // for that god.
            if (num_gods == 1 && coinflip())
            {
                CrawlVector &god_vec = temple[TEMPLE_GODS_KEY];
                god_type     god     = (god_type) god_vec[0].get_byte();

                std::string name = god_name(god);
                name = replace_all(name, " ", "_");
                lowercase(name);

                if (you.uniq_map_tags.find("uniq_altar_" + name)
                    != you.uniq_map_tags.end())
                {
                    // We've already placed a specialized temple for this
                    // god, so do nothing.
#ifdef DEBUG_TEMPLES
                    mprf(MSGCH_DIAGNOSTICS, "Already placed specialized "
                         "single-altar temple for %s", name.c_str());
#endif
                    continue;
                }

                vault_tag = make_stringf("temple_overflow_%s", name.c_str());

                vault = random_map_for_tag(vault_tag, true);
#ifdef DEBUG_TEMPLES
                if (vault == NULL)
                    mprf(MSGCH_DIAGNOSTICS, "Couldn't find overflow temple "
                         "for god %s", name.c_str());
#endif
            }

            if (vault == NULL)
            {
                vault_tag = make_stringf("temple_overflow_%d", num_gods);

                vault = random_map_for_tag(vault_tag, true);
                if (vault == NULL)
                {
                    mprf(MSGCH_ERROR,
                         "Couldn't find overflow temple tag '%s'!",
                         vault_tag.c_str());
                }
            }
        }

        if (vault == NULL)
            // Might as well build the rest of the level if we couldn't
            // find the overflow temple map, so don't veto the level.
            return;

        if (!dgn_ensure_vault_placed(_build_vaults(level_number, vault), false))
        {
#ifdef DEBUG_TEMPLES
            mprf(MSGCH_DIAGNOSTICS, "Couldn't place overflow temple '%s', "
                 "vetoing level.", vault->name.c_str());
#endif
            return;
        }
#ifdef DEBUG_TEMPLES
        mprf(MSGCH_DIAGNOSTICS, "Placed overflow temple %s",
             vault->name.c_str());
#endif
    }
    _current_temple_hash = NULL; // XXX: hack!
}

static void _build_dungeon_level(int level_number, int level_type)
{
    spec_room sr;

    _build_layout_skeleton(level_number, level_type, sr);

    if (you.level_type == LEVEL_LABYRINTH
        || you.level_type == LEVEL_PORTAL_VAULT
        || dgn_level_vetoed)
    {
        return;
    }

    // Hook up the special room (if there is one, and it hasn't
    // been hooked up already in roguey_level()).
    if (sr.created && !sr.hooked_up && !crawl_state.game_is_sprint()
        && !crawl_state.game_is_tutorial())
    {
        _specr_2(sr);
    }

    // Now place items, monster, gates, etc.
    // Stairs must exist by this point (except in Shoals where they are
    // yet to be placed). Some items and monsters already exist.

#ifdef OLD_SWAMP_LAYOUT
    // Time to make the swamp or shoals {dlb}:
    if (player_in_branch(BRANCH_SWAMP))
        dgn_prepare_swamp();
#endif

    if (dgn_level_vetoed)
        return;

    _check_doors();

    if (!player_in_branch(BRANCH_DIS) && !player_in_branch(BRANCH_VAULTS))
        _hide_doors();

    if (player_in_branch(BRANCH_LAIR))
    {
        int depth = player_branch_depth() + 1;
        do {
            _ruin_level(20 - depth, depth / 2 + 5);
            _add_plant_clumps(12 - depth, 18 - depth / 4, depth / 4 + 2);
            depth -= 3;
        } while (depth > 0);
    }

    // XXX: right place to do this?
    if (player_in_branch(BRANCH_SLIME_PITS))
    {
        dgn_replace_area(0, 0, GXM-1, GYM-1,
                         DNGN_ROCK_WALL, DNGN_SLIMY_WALL);
    }

    // Change pre-rock to rock, and pre-floor to floor.
    dgn_replace_area(0, 0, GXM-1, GYM-1, DNGN_BUILDER_SPECIAL_WALL,
                     DNGN_ROCK_WALL);
    dgn_replace_area(0, 0, GXM-1, GYM-1, DNGN_BUILDER_SPECIAL_FLOOR,
                     DNGN_FLOOR);

    const unsigned nvaults = Level_Vaults.size();

    // Any further vaults must make sure not to disrupt level layout.
    dgn_check_connectivity = !player_in_branch(BRANCH_SHOALS);

    if (you.where_are_you == BRANCH_MAIN_DUNGEON
        && !crawl_state.game_is_tutorial())
    {
        _build_overflow_temples(level_number);

        if (dgn_level_vetoed)
            return;
    }

    // Try to place minivaults that really badly want to be placed. Still
    // no guarantees, seeing this is a minivault.
    if (!crawl_state.game_is_sprint() && !crawl_state.game_is_tutorial())
    {
        _place_minivaults();
        _place_branch_entrances( level_number, level_type );
        _place_extra_vaults();
    }

    // XXX: Moved this here from builder_monsters so that connectivity can be
    //      ensured
    _place_uniques(level_number, level_type);

    // Place shops, if appropriate. This must be protected by the connectivity
    // check.
    if (level_type == LEVEL_DUNGEON)
        _place_shops(level_number);

    // Any vault-placement activity must happen before this check.
    if (!crawl_state.game_is_sprint() && !crawl_state.game_is_tutorial())
        _dgn_verify_connectivity(nvaults);

    if (dgn_level_vetoed && !crawl_state.game_is_sprint())
        return;

    if (level_type != LEVEL_ABYSS)
        _place_traps(level_number);

    _place_fog_machines(level_number);

    // Place items.
    if (!crawl_state.game_is_sprint() && !crawl_state.game_is_tutorial())
    {
        _builder_items(level_number, level_type,
                       _num_items_wanted(level_number));
    }

    // Place monsters.
    _builder_monsters(level_number, level_type, _num_mons_wanted(level_type));

    if (!crawl_state.game_is_sprint() && !crawl_state.game_is_tutorial())
    {
        _fixup_walls();
        _fixup_branch_stairs();
    }

    _place_altars();

    _fixup_misplaced_items();

    link_items();

    if (!player_in_branch(BRANCH_COCYTUS)
        && !player_in_branch(BRANCH_SWAMP)
        && !player_in_branch(BRANCH_SHOALS))
    {
        _prepare_water( level_number );
    }

    // Translate stairs for pandemonium levels.
    if (level_type == LEVEL_PANDEMONIUM)
        _fixup_pandemonium_stairs();
}                               // end builder()


static char _fix_black_colour(char incol)
{
    if (incol == BLACK)
        return LIGHTGREY;
    else
        return incol;
}

void dgn_set_colours_from_monsters()
{
    if (env.mons_alloc[9] < 0 || env.mons_alloc[9] == MONS_NO_MONSTER
        || env.mons_alloc[9] >= NUM_MONSTERS)
    {
        if (env.floor_colour == BLACK)
            env.floor_colour = LIGHTGREY;
    }
    else
    {
        env.floor_colour =
            _fix_black_colour(mons_class_colour(env.mons_alloc[9]));
    }

    if (env.mons_alloc[8] < 0 || env.mons_alloc[8] == MONS_NO_MONSTER
        || env.mons_alloc[8] >= NUM_MONSTERS)
    {
        if (env.rock_colour == BLACK)
            env.rock_colour = BROWN;
    }
    else
    {
        env.rock_colour =
            _fix_black_colour(mons_class_colour(env.mons_alloc[8]));
    }
}

static void _dgn_set_floor_colours()
{
    unsigned char old_floor_colour = env.floor_colour;
    unsigned char old_rock_colour  = env.rock_colour;

    if (you.level_type == LEVEL_PANDEMONIUM || you.level_type == LEVEL_ABYSS)
        dgn_set_colours_from_monsters();
    else if (you.level_type == LEVEL_DUNGEON)
    {
        // level_type == LEVEL_DUNGEON
        // Hall of Zot colours handled in dat/zot.des
        const int youbranch = you.where_are_you;
        env.floor_colour    = branches[youbranch].floor_colour;
        env.rock_colour     = branches[youbranch].rock_colour;
    }

    if (old_floor_colour != BLACK)
        env.floor_colour = old_floor_colour;
    if (old_rock_colour != BLACK)
        env.rock_colour = old_rock_colour;

    if (env.floor_colour == BLACK)
        env.floor_colour = LIGHTGREY;
    if (env.rock_colour == BLACK)
        env.rock_colour  = BROWN;
}

static void _check_doors()
{
    for (int x = 1; x < GXM-1; x++)
        for (int y = 1; y < GYM-1; y++)
        {
            if (!feat_is_closed_door(grd[x][y]))
                continue;

            int solid_count = 0;

            if (feat_is_solid( grd[x - 1][y] ))
                solid_count++;

            if (feat_is_solid( grd[x + 1][y] ))
                solid_count++;

            if (feat_is_solid( grd[x][y - 1] ))
                solid_count++;

            if (feat_is_solid( grd[x][y + 1] ))
                solid_count++;

            grd[x][y] = (solid_count < 2 ? DNGN_FLOOR
                                         : DNGN_CLOSED_DOOR);
        }
}

static void _hide_doors()
{
    unsigned char dx = 0, dy = 0;     // loop variables
    unsigned char wall_count = 0;     // clarifies inner loop {dlb}

    for (dx = 1; dx < GXM-1; dx++)
        for (dy = 1; dy < GYM-1; dy++)
        {
            // Only one out of four doors are candidates for hiding. {gdl}
            if (grd[dx][dy] == DNGN_CLOSED_DOOR && one_chance_in(4)
                && unforbidden(coord_def(dx, dy), MMT_NO_DOOR))
            {
                wall_count = 0;

                if (grd[dx - 1][dy] == DNGN_ROCK_WALL)
                    wall_count++;

                if (grd[dx + 1][dy] == DNGN_ROCK_WALL)
                    wall_count++;

                if (grd[dx][dy - 1] == DNGN_ROCK_WALL)
                    wall_count++;

                if (grd[dx][dy + 1] == DNGN_ROCK_WALL)
                    wall_count++;

                // If door is attached to more than one wall, hide it. {dlb}
                if (wall_count > 1)
                    grd[dx][dy] = DNGN_SECRET_DOOR;
            }
        }
}

int count_feature_in_box(int x0, int y0, int x1, int y1,
                         dungeon_feature_type feat)
{
    int result = 0;
    for (int i = x0; i < x1; ++i)
        for (int j = y0; j < y1; ++j)
        {
            if (grd[i][j] == feat)
                ++result;
        }

    return result;
}

int count_antifeature_in_box(int x0, int y0, int x1, int y1,
                             dungeon_feature_type feat)
{
    return (x1-x0)*(y1-y0) - count_feature_in_box(x0, y0, x1, y1, feat);
}

// Count how many neighbours of grd[x][y] are the feature feat.
int count_neighbours(int x, int y, dungeon_feature_type feat)
{
    return count_feature_in_box(x-1, y-1, x+2, y+2, feat);
}

static void _connected_flood(int margin, int i, int j, bool taken[GXM][GYM])
{
    if (i < margin || i >= GXM - margin || j < margin || j >= GYM - margin
        || taken[i][j])
    {
        return;
    }

    taken[i][j] = true;
    for (int idelta = -1; idelta <= 1; ++idelta)
        for (int jdelta = -1; jdelta <= 1; ++jdelta)
            _connected_flood(margin, i + idelta, j + jdelta, taken);
}

// Gives water which is next to ground/shallow water a chance of being
// shallow. Checks each water space.
static void _prepare_water( int level_number )
{
    int i, j, k, l;             // loop variables {dlb}
    unsigned char which_grid;   // code compaction {dlb}

    for (i = 1; i < (GXM - 1); i++)
        for (j = 1; j < (GYM - 1); j++)
        {
            if (!unforbidden(coord_def(i, j), MMT_NO_POOL))
                continue;

            if (grd[i][j] == DNGN_DEEP_WATER)
            {
                for (k = -1; k < 2; k++)
                    for (l = -1; l < 2; l++)
                        if (k != 0 || l != 0)
                        {
                            which_grid = grd[i + k][j + l];

                            // must come first {dlb}
                            if (which_grid == DNGN_SHALLOW_WATER
                                && one_chance_in( 8 + level_number ))
                            {
                                grd[i][j] = DNGN_SHALLOW_WATER;
                            }
                            else if (which_grid >= DNGN_FLOOR
                                     && x_chance_in_y(80 - level_number * 4,
                                                      100))
                            {
                                grd[i][j] = DNGN_SHALLOW_WATER;
                            }
                        }
            }
        }
}                               // end prepare_water()

static bool _find_in_area(int sx, int sy, int ex, int ey,
                          dungeon_feature_type feature)
{
    int x,y;

    if (feature != 0)
    {
        for (x = sx; x <= ex; x++)
            for (y = sy; y <= ey; y++)
            {
                if (grd[x][y] == feature)
                    return (true);
            }
    }

    return (false);
}

// Stamp a box. Can avoid a possible type, and walls and floors can
// be different (or not stamped at all).
// Note that the box boundaries are INclusive.
static bool _make_box(int room_x1, int room_y1, int room_x2, int room_y2,
                      dungeon_feature_type floor,
                      dungeon_feature_type wall,
                      dungeon_feature_type avoid)
{
    int bx,by;

    // Check for avoidance.
    if (_find_in_area(room_x1, room_y1, room_x2, room_y2, avoid))
        return (false);

    // Draw walls.
    if (wall != 0)
    {
        for (bx = room_x1; bx <= room_x2; bx++)
        {
            grd[bx][room_y1] = wall;
            grd[bx][room_y2] = wall;
        }
        for (by = room_y1+1; by < room_y2; by++)
        {
            grd[room_x1][by] = wall;
            grd[room_x2][by] = wall;
        }
    }

    // Draw floor.
    if (floor != 0)
    {
        for (bx = room_x1 + 1; bx < room_x2; bx++)
            for (by = room_y1 + 1; by < room_y2; by++)
                grd[bx][by] = floor;
    }

    return (true);
}

// Take care of labyrinth, abyss, pandemonium.
// Returns 1 if we should skip further generation,
// -1 if we should immediately quit, and 0 otherwise.
static builder_rc_type _builder_by_type(int level_number, char level_type)
{
    if (level_type == LEVEL_PORTAL_VAULT)
    {
        _portal_vault_level(level_number);
        return (BUILD_QUIT);
    }

    if (level_type == LEVEL_LABYRINTH)
    {
        _labyrinth_level(level_number);
        return (BUILD_QUIT);
    }

    if (level_type == LEVEL_ABYSS)
    {
        generate_abyss();
        return (BUILD_SKIP);
    }

    if (level_type == LEVEL_PANDEMONIUM)
    {
        int which_demon = -1;
        // Could do spotty_level, but that doesn't always put all paired
        // stairs reachable from each other which isn't a problem in normal
        // dungeon but could be in Pandemonium.
        if (one_chance_in(4))
        {
            do
            {
                which_demon = random2(4);

                // Makes these things less likely as you find more.
                if (one_chance_in(4))
                {
                    which_demon = -1;
                    break;
                }
            }
            while (you.unique_creatures[MONS_MNOLEG + which_demon]);
        }

        if (which_demon >= 0)
        {
            const char *pandemon_level_names[] =
            {
                "mnoleg", "lom_lobon", "cerebov", "gloorx_vloq"
            };

            const map_def *vault =
                random_map_for_tag(pandemon_level_names[which_demon], false);

            ASSERT(vault);
            if (!vault)
            {
                end(1, false, "Failed to find Pandemonium level %s!\n",
                    pandemon_level_names[which_demon]);
            }

            dgn_ensure_vault_placed( _build_vaults(level_number, vault), true );
        }
        else
        {
            _plan_main(level_number, 0);
            const map_def *vault = random_map_for_tag("pan", true);
            ASSERT( vault );

            _build_secondary_vault(level_number, vault);
        }

        return BUILD_SKIP;
    }

    // Must be normal dungeon.
    return BUILD_CONTINUE;
}

static void _portal_vault_level(int level_number)
{
    dgn_Build_Method += make_stringf(" portal_vault_level [%d]",
                                     level_number);
    dgn_Layout_Type   = "portal vault";

    // level_type_tag may contain spaces for human readability, but the
    // corresponding vault tag name cannot use spaces, so force spaces to
    // _ when searching for the tag.
    const std::string trimmed_name =
        replace_all(trimmed_string(you.level_type_tag), " ", "_");

    ASSERT(!trimmed_name.empty());

    const char* level_name = trimmed_name.c_str();

    const map_def *vault = random_map_for_place(level_id::current(), false);

#ifdef WIZARD
    if (!vault && you.wizard && map_count_for_tag(level_name, false) > 1)
    {
        char buf[80];

        do
        {
            mprf(MSGCH_PROMPT, "Which %s (ESC or ENTER for random): ",
                 level_name);
            if (cancelable_get_line(buf, sizeof buf))
                break;

            std::string name = buf;
            trim_string(name);

            if (name.empty())
                break;

            lowercase(name);
            name = replace_all(name, " ", "_");

            vault = find_map_by_name(you.level_type_tag + "_" + name);

            if (!vault)
                mprf(MSGCH_DIAGNOSTICS, "No such %s, try again.",
                     level_name);
        }
        while (!vault);
    }
#endif

    if (!vault)
        vault = random_map_for_tag(level_name, false);

    if (vault)
    {
        // XXX: This is pretty hackish, I confess.
        if (vault->border_fill_type != DNGN_ROCK_WALL)
            dgn_replace_area(0, 0, GXM-1, GYM-1, DNGN_ROCK_WALL,
                             vault->border_fill_type);

        dgn_ensure_vault_placed( _build_vaults(level_number, vault), true );
    }
    else
    {
        _plan_main(level_number, 0);
        _place_minivaults(level_name, 1, 1, true);

        if (Level_Vaults.empty())
        {
            mprf(MSGCH_ERROR, "No maps or tags named '%s'.",
                 level_name);
            ASSERT(false);
            end(-1);
        }
    }

    link_items();

    // TODO: Let portal vault map have arbitrary properties which can
    // be passed onto the callback.
    callback_map::const_iterator
        i = level_type_post_callbacks.find(you.level_type_tag);

    if (i != level_type_post_callbacks.end())
        dlua.callfn(i->second.c_str(), 0, 0);
}

static const map_def *_random_portal_vault(const std::string &tag)
{
    return random_map_for_tag(tag, true);
}

static bool _place_portal_vault(int stair, const std::string &tag, int dlevel)
{
    const map_def *vault = _random_portal_vault(tag);
    if (!vault)
        return (false);

    return _build_secondary_vault(dlevel, vault, stair);
}

static const map_def *_dgn_random_map_for_place(bool minivault)
{
    if (!minivault && player_in_branch(BRANCH_ECUMENICAL_TEMPLE))
    {
        // Temple vault determined at new game tiem.
        std::string name = you.props[TEMPLE_MAP_KEY];

        // Tolerate this for a little while, for old games.
        if (!name.empty())
        {
            const map_def *vault = find_map_by_name(name);

            if (vault == NULL)
            {
                end(1, false, "Unable to place Temple vault '%s'",
                    name.c_str());
            }
            return (vault);
        }
    }

    const level_id lid = level_id::current();

    const map_def *vault = random_map_for_place(lid, minivault);

    if (!vault
        && lid.branch == BRANCH_MAIN_DUNGEON
        && lid.depth == 1)
    {
        if (crawl_state.game_is_sprint())
        {
            vault = find_map_by_name(get_sprint_map());
            if (vault == NULL)
            {
                end(1, false, "Couldn't find selected Sprint map '%s'.",
                    get_sprint_map().c_str());
            }
        }
        else if (crawl_state.game_is_tutorial())
        {
            vault = find_map_by_name("tutorial_basic_1");
            if (vault == NULL)
                end(1, false, "Couldn't find tutorial map.");
        }
        else
            vault = random_map_for_tag("entry");
    }

    return (vault);
}

static int _setup_temple_altars(CrawlHashTable &temple)
{
    _current_temple_hash = &temple; // XXX: hack!

    CrawlVector god_list = temple[TEMPLE_GODS_KEY].get_vector();

    _temple_altar_list.clear();

    for (unsigned int i = 0; i < god_list.size(); i++)
        _temple_altar_list.push_back( (god_type) god_list[i].get_byte() );

    return ( (int) god_list.size() );
}

// Returns BUILD_SKIP if we should skip further generation,
// BUILD_QUIT if we should immediately quit, and BUILD_CONTINUE
// otherwise.
static builder_rc_type _builder_by_branch(int level_number)
{
    const map_def *vault = _dgn_random_map_for_place(false);

    if (vault)
    {
        dgn_Build_Method += " random_map_for_place";
        _ensure_vault_placed_ex( _build_vaults(level_number, vault), vault );
        if (!dgn_level_vetoed && player_in_branch(BRANCH_SWAMP))
            dgn_build_swamp_level(level_number);
        return BUILD_SKIP;
    }

    switch (you.where_are_you)
    {
    case BRANCH_HIVE:
    case BRANCH_SLIME_PITS:
    case BRANCH_ORCISH_MINES:
    {
        int iterations;
        if (at_branch_bottom())
            iterations = 600 + random2(600);
        else
            iterations = 100 + random2(500);
        spotty_level(false, iterations, false);
        return BUILD_SKIP;
    }

    case BRANCH_SHOALS:
        dgn_build_shoals_level(level_number);
        return BUILD_SKIP;

    case BRANCH_SWAMP:
        dgn_build_swamp_level(level_number);
        return BUILD_SKIP;

    default:
        break;
    }
    return BUILD_CONTINUE;
}

static void _place_minivaults(const std::string &tag, int lo, int hi,
                              bool force)
{
    const level_id curr = level_id::current();

    if (lo == -1)
        lo = hi = 1;

    int nvaults = random_range(lo, hi);
    if (!tag.empty())
    {
        for (int i = 0; i < nvaults; ++i)
        {
            const map_def *vault = random_map_for_tag(tag, true);
            if (!vault)
                return;

            _build_secondary_vault(you.absdepth0, vault);
        }
        return;
    }

    if (use_random_maps)
    {
        const map_def *vault = NULL;

        if ((vault = random_map_for_place(level_id::current(), true)))
            _build_secondary_vault(you.absdepth0, vault);

        do
        {
            vault = random_map_in_depth(level_id::current(), true);
            if (vault)
                _build_secondary_vault(you.absdepth0, vault);
        }
        while (vault && vault->has_tag("extra"));
    }
}

// Returns 1 if we should dispense with city building,
// 0 otherwise.  Also sets special_room if one is generated
// so that we can link it up later.
static builder_rc_type _builder_normal(int level_number, char level_type,
                                       spec_room &sr)
{
    UNUSED( level_type );

    bool skipped = false;
    const map_def *vault = _dgn_random_map_for_place(false);

    // Can't have vaults on you.where_are_you != BRANCH_MAIN_DUNGEON levels.
    if (!vault && use_random_maps && can_create_vault)
    {
        vault = random_map_in_depth(level_id::current());

        // We'll accept any kind of primary vault in the main dungeon,
        // but only ORIENT: encompass primary vaults in other
        // branches. Other kinds of vaults can still be placed in
        // other branches as secondary vaults.

        if (vault && !player_in_branch(BRANCH_MAIN_DUNGEON)
            && vault->orient != MAP_ENCOMPASS)
        {
            vault = NULL;
        }
    }

    if (vault)
    {
        dgn_Build_Method += " normal_random_map_for_place";
        _ensure_vault_placed_ex( _build_vaults(level_number, vault), vault );
        return BUILD_SKIP;
    }

    if (player_in_branch( BRANCH_DIS ))
    {
        _city_level(level_number);
        return BUILD_SKIP;
    }

    if (player_in_branch( BRANCH_VAULTS ))
    {
        if (one_chance_in(3))
            _city_level(level_number);
        else
            _plan_main(level_number, 4);
        return BUILD_SKIP;
    }

    if (level_number > 7 && level_number < 23)
    {
        if (one_chance_in(16))
        {
            spotty_level(false, 0, coinflip());
            return BUILD_SKIP;
        }

        if (one_chance_in(16))
        {
            _bigger_room();
            return BUILD_SKIP;
        }
    }

    if (level_number > 2 && level_number < 23 && one_chance_in(3))
    {
        _plan_main(level_number, 0);
        return BUILD_SKIP;
    }

    if (one_chance_in(3))
        skipped = true;

    //V was 3
    if (!skipped && one_chance_in(7))
    {
        // Sometimes do just a rogue level, sometimes override with
        // the basic builder for something more interesting.
        bool just_roguey = coinflip();

        // Sometimes _roguey_level() generates a special room.
        _roguey_level(level_number, sr, just_roguey);

        if (just_roguey)
            return BUILD_SKIP;
    }
    else
    {
        if (!skipped && level_number > 13 && one_chance_in(8))
        {
            if (one_chance_in(3))
                _city_level(level_number);
            else
                _plan_main(level_number, 4);

            return BUILD_SKIP;
        }
    }

    // maybe create a special room, if roguey_level hasn't done it
    // already.
#ifdef DEBUG_SPECIAL_ROOMS
    if (!sr.created)
#else
    if (!sr.created && one_chance_in(5))
#endif
    {
        const map_def *sroom = random_map_for_tag("special_room", true);

        // Might not be any special room definitions appropriate for
        // this branch and depth.
        if (sroom != NULL)
            _special_room(level_number, sr, sroom);
    }

    return BUILD_CONTINUE;
}

// Returns 1 if we should skip extras(), otherwise 0.
static builder_rc_type _builder_basic(int level_number)
{
    dgn_Build_Method += make_stringf(" basic [%d]", level_number);
    dgn_Layout_Type  = "basic";

    int temp_rand;
    int doorlevel  = random2(11);
    int corrlength = 2 + random2(14);
    int roomsize   = 4 + random2(5) + random2(6);
    int no_corr = (one_chance_in(100) ? 500 + random2(500)
                                      : 30 + random2(200));
    int intersect_chance = (one_chance_in(20) ? 400 : random2(20));

    int xbegin = -1, ybegin = -1, xend = -1, yend = -1;

    _make_trail( 35, 30, 35, 20, corrlength, intersect_chance, no_corr,
                 xbegin, ybegin, xend, yend);

    grd[xbegin][ybegin] = DNGN_STONE_STAIRS_DOWN_I;
    grd[xend][yend]     = DNGN_STONE_STAIRS_UP_I;

    xbegin = -1, ybegin = -1, xend = -1, yend = -1;

    _make_trail( 10, 15, 10, 15, corrlength, intersect_chance, no_corr,
                 xbegin, ybegin, xend, yend);

    grd[xbegin][ybegin] = DNGN_STONE_STAIRS_DOWN_III;
    grd[xend][yend]     = DNGN_STONE_STAIRS_UP_III;

    xbegin = -1, ybegin = -1, xend = -1, yend = -1;

    _make_trail( 50, 20, 10, 15, corrlength, intersect_chance, no_corr,
                 xbegin, ybegin, xend, yend);

    grd[xbegin][ybegin] = DNGN_STONE_STAIRS_DOWN_III;
    grd[xend][yend]     = DNGN_STONE_STAIRS_UP_III;

    // Generate a random dead-end that /may/ have a shaft.  Good luck!
    if (is_valid_shaft_level() && !one_chance_in(4)) // 3/4 times
    {
        // This is kinda hack-ish.  We're still early in the dungeon
        // generation process, and we know that there will be no other
        // traps.  If we promise to make /just one/, we can get away
        // with making this trap the first trap.
        // If we aren't careful, we'll trigger an assert in _place_traps().

        xbegin = -1, ybegin = -1, xend = -1, yend = -1;

        _make_trail( 50, 20, 40, 20, corrlength, intersect_chance, no_corr,
                     xbegin, ybegin, xend, yend);

        dprf("Placing shaft trail...");
        if (!one_chance_in(3) && unforbidden(coord_def(xend, yend), MMT_NO_TRAP)) // 2/3 chance it ends in a shaft
        {
            trap_def& ts(env.trap[0]);
            ts.type = TRAP_SHAFT;
            ts.pos.x = xend;
            ts.pos.y = yend;
            grd[xend][yend] = DNGN_UNDISCOVERED_TRAP;
            if (shaft_known(level_number, false))
                ts.reveal();
            dprf("Trail ends in shaft.");
        }
        else
        {
            grd[xend][yend] = DNGN_FLOOR;
            dprf("Trail does not end in shaft..");
        }
    }

    if (level_number > 1 && one_chance_in(16))
        _big_room(level_number);

    if (random2(level_number) > 6 && one_chance_in(3))
        _diamond_rooms(level_number);

    // make some rooms:
    int i, no_rooms, max_doors;
    int sx,sy,ex,ey, time_run;

    temp_rand = random2(750);
    time_run = 0;

    no_rooms = ((temp_rand > 63) ? (5 + random2avg(29, 2)) : // 91.47% {dlb}
                (temp_rand > 14) ? 100                       //  6.53% {dlb}
                                 : 1);                       //  2.00% {dlb}

    max_doors = 2 + random2(8);

    for (i = 0; i < no_rooms; i++)
    {
        sx = 8 + random2(50);
        sy = 8 + random2(40);
        ex = sx + 2 + random2(roomsize);
        ey = sy + 2 + random2(roomsize);

        if (!_make_room(sx,sy,ex,ey,max_doors, doorlevel))
        {
            time_run++;
            i--;
        }

        if (time_run > 30)
        {
            time_run = 0;
            i++;
        }
    }

    // Make some more rooms.
    no_rooms = 1 + random2(3);
    max_doors = 1;

    for (i = 0; i < no_rooms; i++)
    {
        sx = 8 + random2(55);
        sy = 8 + random2(45);
        ex = sx + 5 + random2(6);
        ey = sy + 5 + random2(6);

        if (!_make_room(sx,sy,ex,ey,max_doors, doorlevel))
        {
            time_run++;
            i--;
        }

        if (time_run > 30)
        {
            time_run = 0;
            i++;
        }
    }

    return BUILD_CONTINUE;
}

static void _builder_extras( int level_number, int level_type )
{
    UNUSED( level_type );

    if (level_number > 6 && one_chance_in(10))
    {
        dungeon_feature_type pool_type = (level_number < 11
                                          || coinflip()) ? DNGN_DEEP_WATER
                                                         : DNGN_LAVA;
        if (one_chance_in(15))
            pool_type = DNGN_TREE;

        _many_pools(pool_type);
        return;
    }

    //mv: It's better to be here so other dungeon features are not overridden
    //    by water.
    dungeon_feature_type river_type
        = (one_chance_in( 5 + level_number ) ? DNGN_SHALLOW_WATER
                                             : DNGN_DEEP_WATER);

    if (level_number > 11
        && (one_chance_in(5) || (level_number > 15 && !one_chance_in(5))))
    {
        river_type = DNGN_LAVA;
    }

    if (player_in_branch( BRANCH_GEHENNA ))
    {
        river_type = DNGN_LAVA;

        if (coinflip())
            _build_river( river_type );
        else
            _build_lake( river_type );
    }
    else if (player_in_branch( BRANCH_COCYTUS ))
    {
        river_type = DNGN_DEEP_WATER;

        if (coinflip())
            _build_river( river_type );
        else
            _build_lake( river_type );
    }

    if (level_number > 8 && one_chance_in(16))
        _build_river( river_type );
    else if (level_number > 8 && one_chance_in(12))
    {
        _build_lake( (river_type != DNGN_SHALLOW_WATER) ? river_type
                                                        : DNGN_DEEP_WATER );
    }
}

// Used to nuke shafts placed in corridors on low levels - it's just too
// nasty otherwise.
static bool _shaft_is_in_corridor(const coord_def& c)
{
    const coord_def adjs[] = { coord_def(-1,0), coord_def(1,0),
                               coord_def(0,-1), coord_def(0,1) };

    for (unsigned int i = 0; i < ARRAYSZ(adjs); ++i)
    {
        const coord_def spot = c + adjs[i];
        if (!in_bounds(spot) || grd(spot) < DNGN_SHALLOW_WATER)
            return (true);
    }
    return (false);
}

static void _place_traps(int level_number)
{
    const int num_traps = num_traps_for_place(level_number);

    ASSERT(num_traps >= 0);
    ASSERT(num_traps <= MAX_TRAPS);

    for (int i = 0; i < num_traps; i++)
    {
        trap_def& ts(env.trap[i]);
        if (ts.type != TRAP_UNASSIGNED)
            continue;

        int tries;
        for (tries = 0; tries < 200; ++tries)
        {
            ts.pos.x = random2(GXM);
            ts.pos.y = random2(GYM);
            if (in_bounds(ts.pos)
                && grd(ts.pos) == DNGN_FLOOR
                && unforbidden(ts.pos, MMT_NO_TRAP))
            {
                break;
            }
        }

        if (tries == 200)
            break;

        while (ts.type >= NUM_TRAPS)
            ts.type = random_trap_for_place(level_number);

        if (ts.type == TRAP_SHAFT && level_number <= 7)
        {
            // Disallow shaft construction in corridors!
            if (_shaft_is_in_corridor(ts.pos))
            {
                // Choose again!
                ts.type = random_trap_for_place(level_number);

                // If we get shaft a second time, turn it into an alarm trap, or
                // if we got nothing.
                if (ts.type == TRAP_SHAFT || ts.type >= NUM_TRAPS)
                    ts.type = TRAP_ALARM;
            }
        }

        grd(ts.pos) = DNGN_UNDISCOVERED_TRAP;
        if (ts.type == TRAP_SHAFT && shaft_known(level_number, true))
            ts.reveal();
        ts.prepare_ammo();
    }
}

static void _place_fog_machines(int level_number)
{
    int i;
    int num_fogs = num_fogs_for_place(level_number);

    ASSERT(num_fogs >= 0);

    for (i = 0; i < num_fogs; i++)
    {
        fog_machine_data data = random_fog_for_place(level_number);

        if (!valid_fog_machine_data(data))
        {
            mpr("Invalid fog machine data, bailing.", MSGCH_DIAGNOSTICS);
            return;
        }

        int tries = 200;
        int x, y;
        dungeon_feature_type feat;
        do
        {
            x = random2(GXM);
            y = random2(GYM);
            feat = grd[x][y];
        }
        while (feat <= DNGN_MAXWALL && --tries > 0);

        if (tries <= 0)
            break;

        place_fog_machine(data, x, y);
    }
}

void dgn_place_feature_at_random_floor_square(dungeon_feature_type feat,
                                              unsigned mask = MMT_VAULT)
{
    const coord_def place =
        dgn_random_point_in_bounds(DNGN_FLOOR, mask, DNGN_FLOOR);
    if (place.origin())
        dgn_veto_level();
    else
        grd(place) = feat;
}

// Create randomly-placed stone stairs.
void dgn_place_stone_stairs()
{
    for (int i = 0; i < 3; ++i)
    {
        dgn_place_feature_at_random_floor_square(
            static_cast<dungeon_feature_type>(DNGN_STONE_STAIRS_DOWN_I + i));
        dgn_place_feature_at_random_floor_square(
            static_cast<dungeon_feature_type>(DNGN_STONE_STAIRS_UP_I + i));
    }
}

bool dgn_has_adjacent_feat(coord_def c, dungeon_feature_type feat)
{
    for (adjacent_iterator ai(c); ai; ++ai)
        if (grd(*ai) == feat)
            return true;
    return false;
}

coord_def dgn_random_point_in_margin(int margin)
{
    return coord_def(random_range(margin, GXM - margin - 1),
                     random_range(margin, GYM - margin - 1));
}

static inline bool _point_matches_feat(coord_def c,
                                       dungeon_feature_type searchfeat,
                                       unsigned mapmask,
                                       dungeon_feature_type adjacent_feat,
                                       bool monster_free)
{
    return (grd(c) == searchfeat
            && (!monster_free || !monster_at(c))
            && unforbidden(c, mapmask)
            && (adjacent_feat == DNGN_UNSEEN ||
                dgn_has_adjacent_feat(c, adjacent_feat)));
}

// Returns a random point in map bounds matching the given search feature,
// and respecting the map mask (a map mask of MMT_VAULT ensures that
// positions inside vaults will not be returned).
//
// If adjacent_feat is not DNGN_UNSEEN, the chosen square will be
// adjacent to a square containing adjacent_feat.
//
// If monster_free is true, the chosen square will never be occupied by
// a monster.
//
// If tries is set to anything other than -1, this function will make tries
// attempts to find a suitable square, and may fail if the map is crowded.
// If tries is set to -1, this function will examine the entire map and
// guarantees to find a suitable point if available.
//
// If a suitable point is not available (or was not found in X tries),
// returns coord_def(0,0)
//
coord_def dgn_random_point_in_bounds(dungeon_feature_type searchfeat,
                                     unsigned mapmask,
                                     dungeon_feature_type adjacent_feat,
                                     bool monster_free,
                                     int tries)
{
    if (tries == -1)
    {
        // Try a quick and dirty random search:
        coord_def chosen = dgn_random_point_in_bounds(searchfeat,
                                                      mapmask,
                                                      adjacent_feat,
                                                      monster_free,
                                                      10);
        if (!chosen.origin())
            return chosen;

        // Exhaustive search; will never fail if a suitable place is
        // available, but is also far more expensive.
        int nfound = 0;
        for (rectangle_iterator ri(1); ri; ++ri)
        {
            const coord_def c(*ri);
            if (_point_matches_feat(c, searchfeat, mapmask, adjacent_feat,
                                    monster_free)
                && one_chance_in(++nfound))
            {
                chosen = c;
            }
        }
        return (chosen);
    }
    else
    {
        // Random search.
        while (--tries >= 0)
        {
            const coord_def c = random_in_bounds();
            if (_point_matches_feat(c, searchfeat, mapmask, adjacent_feat,
                                    monster_free))
                return c;
        }
        return (coord_def(0, 0));
    }
}

static void _place_specific_feature(dungeon_feature_type feat)
{
    coord_def c;

    do
        c = random_in_bounds();
    while (grd(c) != DNGN_FLOOR || monster_at(c));

    grd(c) = feat;
}

static void _place_specific_stair(dungeon_feature_type stair,
                                  const std::string &tag,
                                  int dlevel,
                                  bool vault_only)
{
    if ((tag.empty() || !_place_portal_vault(stair, tag, dlevel))
        && !vault_only)
    {
        _place_specific_feature(stair);
    }
}

static void _place_extra_vaults()
{
    while (true)
    {
        if (!player_in_branch(BRANCH_MAIN_DUNGEON)
            && use_random_maps
            && can_create_vault)
        {
            const map_def *vault = random_map_in_depth(level_id::current());

            // Encompass vaults can't be used as secondaries.
            if (!vault || vault->orient == MAP_ENCOMPASS)
                break;

            if (vault && _build_secondary_vault(you.absdepth0, vault, -1))
            {
                const map_def &map(*vault);
                if (map.has_tag("extra"))
                    continue;
                can_create_vault = false;
            }
        }
        break;
    }
}

static void _place_branch_entrances(int dlevel, char level_type)
{
    int sx, sy;

    if (level_type != LEVEL_DUNGEON)
        return;

    if (player_in_branch( BRANCH_MAIN_DUNGEON ))
    {
        // stair to HELL
        if (dlevel >= 20 && dlevel <= 27)
            _place_specific_stair(DNGN_ENTER_HELL, "hell_entry", dlevel);

        // stair to PANDEMONIUM
        if (dlevel >= 20 && dlevel <= 50 && (dlevel == 23 || one_chance_in(4)))
            _place_specific_stair(DNGN_ENTER_PANDEMONIUM, "pan_entry", dlevel);

        // stairs to ABYSS
        if (dlevel >= 20 && dlevel <= 30 && (dlevel == 24 || one_chance_in(3)))
            _place_specific_stair(DNGN_ENTER_ABYSS, "abyss_entry", dlevel);

        // level 26: replaces all down stairs with staircases to Zot:
        if (dlevel == 26)
        {
            for (sx = 1; sx < GXM; sx++)
                for (sy = 1; sy < GYM; sy++)
                    if (grd[sx][sy] >= DNGN_STONE_STAIRS_DOWN_I
                        && grd[sx][sy] <= DNGN_ESCAPE_HATCH_DOWN)
                    {
                        grd[sx][sy] = DNGN_ENTER_ZOT;
                    }
        }
    }

    // Place actual branch entrances.
    for (int i = 0; i < NUM_BRANCHES; ++i)
    {
        if (branches[i].entry_stairs != NUM_FEATURES
            && player_in_branch(branches[i].parent_branch)
            && player_branch_depth() == branches[i].startdepth)
        {
            // Place a stair.
            dprf("Placing stair to %s", branches[i].shortname);

            std::string entry_tag = std::string(branches[i].abbrevname);
            entry_tag += "_entry";
            lowercase(entry_tag);

            _place_specific_stair( branches[i].entry_stairs, entry_tag, dlevel);
        }
    }
}

static void _make_trail(int xs, int xr, int ys, int yr, int corrlength,
                        int intersect_chance, int no_corr,
                        int &xbegin, int &ybegin,
                        int &xend, int &yend)
{
    int x_start, y_start;                   // begin point
    int x_ps, y_ps;                         // end point
    int finish = 0;
    int length = 0;
    int temp_rand;

    // temp positions
    int dir_x = 0;
    int dir_y = 0;
    int dir_x2, dir_y2;

    do
    {
        x_start = xs + random2(xr);
        y_start = ys + random2(yr);
    }
    while (grd[x_start][y_start] != DNGN_ROCK_WALL
           && grd[x_start][y_start] != DNGN_FLOOR);

    // assign begin position
    xbegin = x_start; ybegin = y_start;

    x_ps = x_start;
    y_ps = y_start;

    // wander
    do                          // (while finish < no_corr)
    {
        dir_x2 = ((x_ps < 15) ? 1 : 0);

        if (x_ps > 65)
            dir_x2 = -1;

        dir_y2 = ((y_ps < 15) ? 1 : 0);

        if (y_ps > 55)
            dir_y2 = -1;

        temp_rand = random2(10);

        // Put something in to make it go to parts of map it isn't in now.
        if (coinflip())
        {
            if (dir_x2 != 0 && temp_rand < 6)
                dir_x = dir_x2;

            if (dir_x2 == 0 || temp_rand >= 6)
                dir_x = (coinflip()? -1 : 1);

            dir_y = 0;
        }
        else
        {
            if (dir_y2 != 0 && temp_rand < 6)
                dir_y = dir_y2;

            if (dir_y2 == 0 || temp_rand >= 6)
                dir_y = (coinflip()? -1 : 1);

            dir_x = 0;
        }

        if (dir_x == 0 && dir_y == 0)
            continue;

        if (x_ps < X_BOUND_1 + 3)
        {
            dir_x = 1;
            dir_y = 0;
        }

        if (y_ps < Y_BOUND_1 + 3)
        {
            dir_y = 1;
            dir_x = 0;
        }

        if (x_ps > (X_BOUND_2 - 3))
        {
            dir_x = -1;
            dir_y = 0;
        }

        if (y_ps > (Y_BOUND_2 - 3))
        {
            dir_y = -1;
            dir_x = 0;
        }

        // Corridor length... change only when going vertical?
        if (dir_x == 0 || length == 0)
            length = random2(corrlength) + 2;

        int bi = 0;

        for (bi = 0; bi < length; bi++)
        {
            // Below, I've changed the values of the unimportant variable from
            // 0 to random2(3) - 1 to avoid getting stuck on the "stuck!" bit.
            if (x_ps < X_BOUND_1 + 4)
            {
                dir_y = 0;      //random2(3) - 1;
                dir_x = 1;
            }

            if (x_ps > (X_BOUND_2 - 4))
            {
                dir_y = 0;      //random2(3) - 1;
                dir_x = -1;
            }

            if (y_ps < Y_BOUND_1 + 4)
            {
                dir_y = 1;
                dir_x = 0;      //random2(3) - 1;
            }

            if (y_ps > (Y_BOUND_2 - 4))
            {
                dir_y = -1;
                dir_x = 0;      //random2(3) - 1;
            }

            // Don't interfere with special rooms.
            if (grd[x_ps + dir_x][y_ps + dir_y] == DNGN_BUILDER_SPECIAL_WALL)
                break;

            // See if we stop due to intersection with another corridor/room.
            if (grd[x_ps + 2 * dir_x][y_ps + 2 * dir_y] == DNGN_FLOOR
                && !one_chance_in(intersect_chance))
            {
                break;
            }

            x_ps += dir_x;
            y_ps += dir_y;

            if (grd[x_ps][y_ps] == DNGN_ROCK_WALL)
                grd[x_ps][y_ps] = DNGN_FLOOR;
        }

        if (finish == no_corr - 1 && grd[x_ps][y_ps] != DNGN_FLOOR)
            finish -= 2;

        finish++;
    }
    while (finish < no_corr);

    // assign end position
    xend = x_ps, yend = y_ps;
}

static int _good_door_spot(int x, int y)
{
    if (!feat_is_solid(grd[x][y]) && grd[x][y] < DNGN_ENTER_PANDEMONIUM
        || feat_is_closed_door(grd[x][y]))
    {
        return 1;
    }

    return 0;
}

// Returns TRUE if a room was made successfully.
static bool _make_room(int sx,int sy,int ex,int ey,int max_doors, int doorlevel)
{
    int find_door = 0;
    int diag_door = 0;
    int rx, ry;

    // Check top & bottom for possible doors.
    for (rx = sx; rx <= ex; rx++)
    {
        find_door += _good_door_spot(rx,sy);
        find_door += _good_door_spot(rx,ey);
    }

    // Check left and right for possible doors.
    for (ry = sy + 1; ry < ey; ry++)
    {
        find_door += _good_door_spot(sx,ry);
        find_door += _good_door_spot(ex,ry);
    }

    diag_door += _good_door_spot(sx,sy);
    diag_door += _good_door_spot(ex,sy);
    diag_door += _good_door_spot(sx,ey);
    diag_door += _good_door_spot(ex,ey);

    if ((diag_door + find_door) > 1 && max_doors == 1)
        return (false);

    if (find_door == 0 || find_door > max_doors)
        return (false);

    // Look for 'special' rock walls - don't interrupt them.
    if (_find_in_area(sx, sy, ex, ey, DNGN_BUILDER_SPECIAL_WALL))
        return (false);

    // Convert the area to floor.
    for (rx = sx; rx <= ex; rx++)
        for (ry = sy; ry <= ey; ry++)
        {
            if (grd[rx][ry] <= DNGN_FLOOR)
                grd[rx][ry] = DNGN_FLOOR;
        }

    // Put some doors on the sides (but not in corners),
    // where it makes sense to do so.
    for (ry = sy + 1; ry < ey; ry++)
    {
        // left side
        if (grd[sx-1][ry] == DNGN_FLOOR
            && feat_is_solid(grd[sx-1][ry-1])
            && feat_is_solid(grd[sx-1][ry+1]))
        {
            if (x_chance_in_y(doorlevel, 10))
                grd[sx-1][ry] = DNGN_CLOSED_DOOR;
        }

        // right side
        if (grd[ex+1][ry] == DNGN_FLOOR
            && feat_is_solid(grd[ex+1][ry-1])
            && feat_is_solid(grd[ex+1][ry+1]))
        {
            if (x_chance_in_y(doorlevel, 10))
                grd[ex+1][ry] = DNGN_CLOSED_DOOR;
        }
    }

    // Put some doors on the top & bottom.
    for (rx = sx + 1; rx < ex; rx++)
    {
        // top
        if (grd[rx][sy-1] == DNGN_FLOOR
            && feat_is_solid(grd[rx-1][sy-1])
            && feat_is_solid(grd[rx+1][sy-1]))
        {
            if (x_chance_in_y(doorlevel, 10))
                grd[rx][sy-1] = DNGN_CLOSED_DOOR;
        }

        // bottom
        if (grd[rx][ey+1] == DNGN_FLOOR
            && feat_is_solid(grd[rx-1][ey+1])
            && feat_is_solid(grd[rx+1][ey+1]))
        {
            if (x_chance_in_y(doorlevel, 10))
                grd[rx][ey+1] = DNGN_CLOSED_DOOR;
        }
    }

    return (true);
}

static bool _place_unique_map(const map_def *uniq_map)
{
    // If the unique map is guaranteed to not affect connectivity,
    // allow it to overwrite existing vaults by temporarily
    // suppressing dgn_Map_Mask.
    const bool transparent = uniq_map->has_tag("transparent");
    std::auto_ptr<map_mask> backup_mask;
    if (transparent)
    {
        backup_mask.reset(new map_mask(dgn_Map_Mask));
        dgn_Map_Mask.init(0);
    }

    bool placed = false;
    for (int i = 0; i < 4; i++)
    {
        if (dgn_place_map(uniq_map, false, false))
        {
            placed = true;
            break;
        }
    }

    if (transparent)
        dgn_Map_Mask = *backup_mask;

    return (placed);
}

// Place uniques on the level.
// There is a hidden dependency on the player's actual
// location (through your_branch()).
// Return the number of uniques placed.
static int _place_uniques(int level_number, char level_type)
{
    // Unique beasties:
    if (level_number <= 0 || level_type != LEVEL_DUNGEON
        || !your_branch().has_uniques)
    {
        return 0;
    }

#ifdef DEBUG_UNIQUE_PLACEMENT
    FILE *ostat = fopen("unique_placement.log", "a");
    fprintf(ostat, "--- Looking to place uniques on: Level %d of %s ---\n", level_number, your_branch().shortname);
#endif

    int num_placed = 0;

    // Magic numbers for dpeg's unique system.
    int A = 2;
    const int B = 5;
    while (one_chance_in(A))
    {
        // In dpeg's unique placement system, chances is always 1 in A of even
        // starting to place a unique; reduced if there are less uniques to be
        // placed or available. Then there is a chance of uniques_available /
        // B; this only triggers on levels that have less than B uniques to be
        // placed.
        const std::vector<map_def> uniques_available =
            find_maps_for_tag("place_unique", true, true);

        if (random2(B) >= std::min(B, int(uniques_available.size())))
            break;

        const map_def *uniq_map = random_map_for_tag("place_unique", true);
        if (!uniq_map)
        {
#ifdef DEBUG_UNIQUE_PLACEMENT
            fprintf(ostat, "Dummy balance or no uniques left.\n");
#endif
            break;
        }

        const bool map_placed = _place_unique_map(uniq_map);
        if (map_placed)
        {
            num_placed++;
            // Make the placement chance drop steeply after
            // some have been placed, to reduce chance of
            // many uniques per level.
            if (num_placed >= 3)
                A++;
#ifdef DEBUG_UNIQUE_PLACEMENT
            fprintf(ostat, "Placed valid unique map: %s.\n",
                    uniq_map->name.c_str());
#endif
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Placed %s.",
                 uniq_map->name.c_str());
#endif
        }
#ifdef DEBUG_UNIQUE_PLACEMENT
        else
        {
            fprintf(ostat, "Didn't place valid map: %s\n",
                    uniq_map->name.c_str());
        }
#endif
    }

#ifdef DEBUG_UNIQUE_PLACEMENT
    fprintf(ostat, "--- Finished this set, placed %d uniques.\n", num_placed);
    fclose(ostat);
#endif
    return num_placed;
}

static int _place_monster_vector(std::vector<monster_type> montypes,
                                 int level_number, int num_to_place)
{
    int result = 0;

    mgen_data mg;
    mg.power     = level_number;
    mg.behaviour = BEH_SLEEP;
    mg.flags    |= MG_PERMIT_BANDS;
    mg.map_mask |= MMT_NO_MONS;

    for (int i = 0; i < num_to_place; i++)
    {
        mg.cls = montypes[random2(montypes.size())];

        if (player_in_branch( BRANCH_COCYTUS ) &&
            mons_class_can_be_zombified(mg.cls))
        {
            static const monster_type lut[3][2] =
            {
                { MONS_SKELETON_SMALL, MONS_SKELETON_LARGE },
                { MONS_ZOMBIE_SMALL, MONS_ZOMBIE_LARGE },
                { MONS_SIMULACRUM_SMALL, MONS_SIMULACRUM_LARGE },
            };

            mg.base_type = mg.cls;
            int s = mons_skeleton(mg.cls) ? 2 : 0;
            mg.cls = lut[random_choose_weighted(s, 0, 8, 1, 1, 2, 0)]
                        [mons_zombie_size(mg.base_type) == Z_BIG];
        }

        else
            mg.base_type = MONS_NO_MONSTER;
        if (place_monster(mg) != -1)
            ++result;
    }

    return result;
}


static void _place_aquatic_monsters(int level_number, char level_type)
{
    int lava_spaces = 0, water_spaces = 0;
    std::vector<monster_type> swimming_things(4u, MONS_NO_MONSTER);

    // [ds] Shoals relies on normal monster generation to place its monsters.
    // Given the amount of water area in the Shoals, placing water creatures
    // explicitly explodes the Shoals' xp budget.
    if (player_in_branch(BRANCH_SHOALS))
        return;

    // Count the number of lava and water tiles {dlb}:
    for (int x = 0; x < GXM; x++)
        for (int y = 0; y < GYM; y++)
        {
            if (grd[x][y] == DNGN_LAVA)
                lava_spaces++;

            if (feat_is_water(grd[x][y]))
                water_spaces++;
        }

    if (lava_spaces > 49 && level_number > 6)
    {
        for (int i = 0; i < 4; i++)
        {
            swimming_things[i] = static_cast<monster_type>(
                                     MONS_LAVA_WORM + random2(3) );

            if (one_chance_in(30))
                swimming_things[i] = MONS_SALAMANDER;
        }

        _place_monster_vector(swimming_things, level_number,
                              std::min(random2avg(9, 2)
                                       + (random2(lava_spaces) / 10), 15));
    }

    if (water_spaces > 49)
    {
        // This can probably be done in a better way with something
        // like water_monster_rarity().
        for (int i = 0; i < 4; i++)
        {
            swimming_things[i] =
                static_cast<monster_type>(MONS_BIG_FISH + random2(4));

            if (player_in_branch( BRANCH_SWAMP ) && !one_chance_in(3))
                swimming_things[i] = MONS_SWAMP_WORM;
            else if (player_in_branch( BRANCH_COCYTUS ))
            {
                // Eels are useless when zombified
                if (swimming_things[i] == MONS_ELECTRIC_EEL)
                {
                    swimming_things[i] = one_chance_in(4) ? MONS_KRAKEN :
                                             MONS_WATER_ELEMENTAL;
                }
            }
        }

        // Don't place sharks in the Swamp.
        if (!player_in_branch(BRANCH_SWAMP)
            && level_number >= 9 && one_chance_in(4))
        {
            swimming_things[3] = MONS_SHARK;
        }

        if (level_number >= 25 && one_chance_in(5))
            swimming_things[0] = MONS_WATER_ELEMENTAL;

        _place_monster_vector(swimming_things, level_number,
                              std::min(random2avg(9, 2)
                                + (random2(water_spaces) / 10), 15));
    }
}


static void _builder_monsters(int level_number, char level_type, int mon_wanted)
{
    if (level_type == LEVEL_PANDEMONIUM
        || player_in_branch(BRANCH_ECUMENICAL_TEMPLE))
    {
        return;
    }

    // Try to place Shoals monsters on floor where possible instead of
    // letting all the merfolk be generated in the middle of the
    // water.
    const dungeon_feature_type preferred_grid_feature =
        player_in_branch(BRANCH_SHOALS)? DNGN_FLOOR : DNGN_UNSEEN;

    dprf("_builder_monsters: Generating %d monsters", mon_wanted);
    for (int i = 0; i < mon_wanted; i++)
    {
        mgen_data mg;
        mg.behaviour              = BEH_SLEEP;
        mg.power                  = level_number;
        mg.flags                 |= MG_PERMIT_BANDS;
        mg.map_mask              |= MMT_NO_MONS;
        mg.preferred_grid_feature = preferred_grid_feature;

        place_monster(mg);
    }

    if (!player_in_branch(BRANCH_CRYPT)) // No water creatures in the Crypt.
        _place_aquatic_monsters(level_number, level_type);
    else
    {
        if (one_chance_in(3))
            mons_place(mgen_data(MONS_CURSE_SKULL, BEH_SLEEP));

        if (one_chance_in(7))
            mons_place(mgen_data(MONS_CURSE_SKULL, BEH_SLEEP));
    }
}

static void _builder_items(int level_number, char level_type, int items_wanted)
{
    UNUSED( level_type );

    int i = 0;
    object_class_type specif_type = OBJ_RANDOM;
    int items_levels = level_number;
    int item_no;

    if (player_in_branch( BRANCH_VAULTS ))
    {
        items_levels *= 15;
        items_levels /= 10;
    }
    else if (player_in_branch( BRANCH_ORCISH_MINES ))
        specif_type = OBJ_GOLD;  // Lots of gold in the orcish mines.

    if (player_in_branch( BRANCH_VESTIBULE_OF_HELL )
        || player_in_hell()
        || player_in_branch( BRANCH_SLIME_PITS )
        || player_in_branch( BRANCH_HALL_OF_BLADES )
        || player_in_branch( BRANCH_ECUMENICAL_TEMPLE ))
    {
        // No random items in hell, the slime pits, the temple, the hall.
        return;
    }
    else
    {
        for (i = 0; i < items_wanted; i++)
        {
            items( 1, specif_type, OBJ_RANDOM, false, items_levels, 250,
                   MMT_NO_ITEM );
        }

        // Make sure there's a very good chance of a knife being placed
        // in the first five levels, but not a guarantee of one.  The
        // intent of this is to reduce the advantage that "cutting"
        // starting weapons have.  -- bwr
        if (player_in_branch( BRANCH_MAIN_DUNGEON )
            && level_number < 5 && coinflip())
        {
            item_no = items( 0, OBJ_WEAPONS, WPN_KNIFE, false, 0, 250,
                             MMT_NO_ITEM );

            // Guarantee that the knife is uncursed and non-special.
            if (item_no != NON_ITEM)
            {
                mitm[item_no].plus    = 0;
                mitm[item_no].plus2   = 0;
                mitm[item_no].flags   = 0; // no id, no race/desc, no curse
                mitm[item_no].special = 0; // no ego type
            }
        }
    }
}

// The entire intent of this function is to find a
// hallway from a special room to a floor space somewhere,
// changing the special room wall (DNGN_BUILDER_SPECIAL_WALL)
// to a closed door, and normal rock wall to pre-floor.
// Anything that might otherwise block the hallway is changed
// to pre-floor.
static void _specr_2(spec_room &sr)
{
    coord_def c, delta;
    int i = 0;

    // Paranoia -- how did we get here if there's no actual special room??
    if (!sr.created)
        return;

    bool is_ok = false;
    for (int tries = 0; tries < 100 && !is_ok; ++tries)
    {
        is_ok = true;

        // Set direction.
        switch (random2(4))
        {
        case 0:
            // go up from north edge
            c.set(random_range(sr.tl.x, sr.br.x - 1), sr.tl.y);
            delta.set(0, -1);
            break;
        case 1:
            // go down from south edge
            c.set(random_range(sr.tl.x, sr.br.x - 1), sr.br.y);
            delta.set(0, 1);
            break;
        case 2:
            // go left from west edge
            c.set(sr.tl.x, random_range(sr.tl.y, sr.br.y - 1));
            delta.set(-1, 0);
            break;
        case 3:
            // go right from east edge
            c.set(sr.br.x, random_range(sr.tl.y, sr.br.y - 1));
            delta.set(1, 0);
            break;
        }

        coord_def s = c;

        // Note that we need to remember the value of i when we break out.
        for (i = 0; i < 100; i++)
        {
            s += delta;

            // Quit if we run off the map before finding floor.
            if (!in_bounds(s))
            {
                is_ok = false;
                break;
            }

            if (i > 0)
            {
                // look around for floor
                bool found_floor = false;
                for (int j = 0; j < 4; ++j)
                {
                    const coord_def spot = s + OrthCompass[j];
                    if (!in_bounds(spot))
                        is_ok = false;
                    else if (grd(spot) == DNGN_FLOOR)
                        found_floor = true;
                }

                if ( found_floor )
                    break;
            }
        }
    }

    if (!is_ok)
        return;

    coord_def s = c;

    for (int j = 0; j < i + 2; j++)
    {
        if (grd(s) == DNGN_BUILDER_SPECIAL_WALL)
            grd(s) = DNGN_CLOSED_DOOR;

        if (j > 0
            && grd(s + delta) > DNGN_MINWALL
            && grd(s + delta) < DNGN_FLOOR)
        {
            grd(s) = DNGN_BUILDER_SPECIAL_FLOOR;
        }

        if (grd(s) == DNGN_ROCK_WALL)
            grd(s) = DNGN_BUILDER_SPECIAL_FLOOR;

        s += delta;
    }

    sr.hooked_up = true;
}

static void _special_room(int level_number, spec_room &sr,
                          const map_def *vault)
{
    ASSERT(vault);

    std::string extra = make_stringf("special room [%s %d]",
                                     vault->name.c_str(), level_number);
    env.properties[LEVEL_EXTRAS_KEY].get_vector().push_back(extra);

    // Overwrites anything: this function better be called early on during
    // creation.
    int room_x1 = 8 + random2(55);
    int room_y1 = 8 + random2(45);
    int room_x2 = room_x1 + 4 + random2avg(6,2);
    int room_y2 = room_y1 + 4 + random2avg(6,2);

    // Do special walls & floor.
    _make_box(room_x1, room_y1, room_x2, room_y2,
              DNGN_BUILDER_SPECIAL_FLOOR, DNGN_BUILDER_SPECIAL_WALL);

    // Set up passed in spec_room structure.
    sr.created   = true;
    sr.hooked_up = false;
    sr.tl.set(room_x1 + 1, room_y1 + 1);
    sr.br.set(room_x2 - 1, room_y2 - 1);

    lua_special_room_spec  = sr;
    lua_special_room_level = level_number;

    _build_secondary_vault( level_number, vault, -1, false, false, sr.tl);

    lua_special_room_spec.created = false;
    lua_special_room_spec.tl.set(-1, -1);
    lua_special_room_level = -1;
}                               // end special_room()

// Used for placement of rivers/lakes.
static bool _may_overwrite_pos(coord_def c)
{
    const dungeon_feature_type grid = grd(c);

    // Don't overwrite any stairs or branch entrances.
    if (grid >= DNGN_ENTER_SHOP && grid <= DNGN_EXIT_PORTAL_VAULT
        || grid == DNGN_EXIT_HELL)
    {
        return (false);
    }

    // Don't overwrite feature if there's a monster or item there.
    // Otherwise, items/monsters might end up stuck in deep water.
    return (!monster_at(c) && igrd(c) == NON_ITEM);
}

static void _build_rooms(const dgn_region_list &excluded,
                         const std::vector<coord_def> &connections_needed,
                         int nrooms)
{
    int which_room = 0;
    const bool exclusive = !one_chance_in(10);

    // Where did this magic number come from?
    const int maxrooms = 30;
    dgn_region rom[maxrooms];

    std::vector<coord_def> connections = connections_needed;

    for (int i = 0; i < nrooms; i++)
    {
        dgn_region &myroom = rom[which_room];

        int overlap_tries = 200;
        do
        {
            myroom.size.set(3 + random2(8), 3 + random2(8));
            myroom.pos.set(
                random_range(MAPGEN_BORDER,
                             GXM - MAPGEN_BORDER - 1 - myroom.size.x),
                random_range(MAPGEN_BORDER,
                             GYM - MAPGEN_BORDER - 1 - myroom.size.y));
        }
        while (myroom.overlaps(excluded, dgn_Map_Mask) && overlap_tries-- > 0);

        if (overlap_tries < 0)
            continue;

        if (connections.size())
        {
            const coord_def c = connections[0];
            if (join_the_dots(c, myroom.random_edge_point(), MMT_VAULT))
                connections.erase( connections.begin() );
        }

        if (i > 0 && exclusive)
        {
            const coord_def end = myroom.end();
            bool found_collision = false;
            for (int cnx = myroom.pos.x - 1;
                 cnx < end.x && !found_collision; cnx++)
            {
                for (int cny = myroom.pos.y - 1;
                     cny < end.y; cny++)
                {
                    if (grd[cnx][cny] != DNGN_ROCK_WALL)
                    {
                        found_collision = true;
                        break;
                    }
                }
            }

            if (found_collision)
                continue;
        }

        const coord_def end = myroom.end();
        dgn_replace_area(myroom.pos.x, myroom.pos.y, end.x, end.y,
                         DNGN_ROCK_WALL, DNGN_FLOOR);

        if (which_room > 0)
        {
            _join_the_dots_rigorous(myroom.random_edge_point(),
                                    rom[which_room - 1].random_edge_point(),
                                    MMT_VAULT);
        }

        which_room++;

        if (which_room >= maxrooms)
            break;
    }
}

static int _away_from_edge(int x, int left_edge, int right_edge)
{
    if (x < left_edge)
        return (1);
    else if (x > right_edge)
        return (-1);
    else
        return (coinflip()? 1 : -1);
}

static coord_def _dig_away_dir(const vault_placement &place,
                               const coord_def &pos)
{
    // Figure out which way we need to go to dig our way out of the vault.
    bool x_edge =
        pos.x == place.pos.x || pos.x == place.pos.x + place.size.x - 1;
    bool y_edge =
        pos.y == place.pos.y || pos.y == place.pos.y + place.size.y - 1;

    // Handle exits in non-rectangular areas.
    if (!x_edge && !y_edge)
    {
        const coord_def rel = pos - place.pos;
        for (int yi = -1; yi <= 1; ++yi)
            for (int xi = -1; xi <= 1; ++xi)
            {
                if (!xi == !yi)
                    continue;

                const coord_def mv(rel.x + xi, rel.y + yi);
                if (!place.map.in_map(mv))
                    return (mv - rel);
            }
    }

    if (x_edge && y_edge)
    {
        if (coinflip())
            x_edge = false;
        else
            y_edge = false;
    }

    coord_def dig_dir;
    if (x_edge)
    {
        if (place.size.x == 1)
        {
            dig_dir.x = _away_from_edge(pos.x, MAPGEN_BORDER * 2,
                                        GXM - MAPGEN_BORDER * 2);
        }
        else
            dig_dir.x = pos.x == place.pos.x? -1 : 1;
    }

    if (y_edge)
    {
        if (place.size.y == 1)
        {
            dig_dir.y = _away_from_edge(pos.y, MAPGEN_BORDER * 2,
                                        GYM - MAPGEN_BORDER * 2);
        }
        else
            dig_dir.y = pos.y == place.pos.y? -1 : 1;
    }

    return (dig_dir);
}

// Returns true if the feature can be ovewritten by floor when digging a path
// from a vault to its surroundings.
bool dgn_vault_excavatable_feat(dungeon_feature_type feat)
{
    return (dgn_Vault_Excavatable_Feats.find(feat) !=
            dgn_Vault_Excavatable_Feats.end());
}

coord_def dgn_random_direction()
{
    return Compass[random2(8)];
}

void dgn_excavate(coord_def dig_at, coord_def dig_dir)
{
    bool dug = false;
    for (int i = 0; i < GXM; i++)
    {
        dig_at += dig_dir;

        if (dig_at.x < MAPGEN_BORDER || dig_at.x > (GXM - MAPGEN_BORDER - 1)
            || dig_at.y < MAPGEN_BORDER || dig_at.y > (GYM - MAPGEN_BORDER - 1))
        {
            break;
        }

        const dungeon_feature_type dig_feat(grd(dig_at));
        if (dgn_vault_excavatable_feat(dig_feat))
        {
            grd(dig_at) = DNGN_FLOOR;
            dug = true;
        }
        else if (dig_feat == DNGN_FLOOR && i > 0)
        {
            // If the floor square has at least two neighbouring
            // non-solid squares, we're done.
            int adjacent_count = 0;

            for (int yi = -1; yi <= 1; ++yi)
                for (int xi = -1; xi <= 1; ++xi)
                {
                    if (!xi && !yi)
                        continue;
                    if (!cell_is_solid(dig_at + coord_def(xi, yi))
                        && ++adjacent_count >= 2)
                    {
                        return;
                    }
                }
        }
    }
}

static void _dig_away_from(vault_placement &place, const coord_def &pos)
{
    coord_def dig_dir = _dig_away_dir(place, pos);
    coord_def dig_at  = pos;
    dgn_excavate(dig_at, dig_dir);
}

static void _dig_vault_loose( vault_placement &place,
                              std::vector<coord_def> &targets )
{
    for (int i = 0, size = targets.size(); i < size; ++i)
        _dig_away_from(place, targets[i]);
}

static bool _grid_needs_exit(int x, int y)
{
    return (!cell_is_solid(x, y)
            || feat_is_closed_door(grd[x][y])
            || grd[x][y] == DNGN_SECRET_DOOR);
}

static bool _map_feat_is_on_edge(const vault_placement &place,
                                 const coord_def &c)
{
    for (int xi = c.x - 1; xi <= c.x + 1; ++xi)
        for (int yi = c.y - 1; yi <= c.y + 1; ++yi)
        {
            if (!place.map.in_map(coord_def(xi, yi) - place.pos))
                return (true);
        }

    return (false);
}

static void _pick_internal_float_exits(const vault_placement &place,
                                       std::vector<coord_def> &exits)
{
    for (int y = place.pos.y + 1; y < place.pos.y + place.size.y - 1; ++y)
        for (int x = place.pos.x + 1; x < place.pos.x + place.size.x - 1; ++x)
            if (_grid_needs_exit(x, y)
                && _map_feat_is_on_edge(place, coord_def(x, y)))
            {
                exits.push_back( coord_def(x, y) );
            }
}

static void _pick_float_exits(vault_placement &place,
                              std::vector<coord_def> &targets)
{
    std::vector<coord_def> possible_exits;
    for (int y = place.pos.y; y < place.pos.y + place.size.y; ++y)
    {
        if (_grid_needs_exit(place.pos.x, y))
            possible_exits.push_back( coord_def(place.pos.x, y) );

        if (_grid_needs_exit(place.pos.x + place.size.x - 1, y))
        {
            possible_exits.push_back(
                coord_def(place.pos.x + place.size.x - 1, y) );
        }
    }

    for (int x = place.pos.x + 1; x < place.pos.x + place.size.x - 1; ++x)
    {
        if (_grid_needs_exit(x, place.pos.y))
            possible_exits.push_back( coord_def(x, place.pos.y) );

        if (_grid_needs_exit(x, place.pos.y + place.size.y - 1))
        {
            possible_exits.push_back(
                coord_def(x, place.pos.y + place.size.y - 1) );
        }
    }

    _pick_internal_float_exits(place, possible_exits);

    if (possible_exits.empty())
    {
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_ERROR, "Unable to find exit from %s",
             place.map.name.c_str());
#endif
        return;
    }

    const int npoints = possible_exits.size();
    int nexits = npoints < 6? npoints : npoints / 8 + 1;

    if (nexits > 10)
        nexits = 10;

    while (nexits-- > 0)
    {
        int which_exit = random2( possible_exits.size() );
        targets.push_back( possible_exits[which_exit] );
        possible_exits.erase( possible_exits.begin() + which_exit );
    }
}

static std::vector<coord_def> _external_connection_points(
                               const vault_placement &place,
                               const std::vector<coord_def> &target_connections)
{
    std::vector<coord_def> ex_connection_points;

    // Giving target_connections directly to build_rooms causes
    // problems with long, skinny vaults where paths to the exit
    // tend to cut through the vault. By backing out of the vault
    // one square, we improve connectibility.
    for (int i = 0, size = target_connections.size(); i < size; ++i)
    {
        const coord_def &p = target_connections[i];
        ex_connection_points.push_back(p + _dig_away_dir(place, p));
    }

    return (ex_connection_points);
}

static coord_def _find_random_grid(int grid, unsigned mask)
{
    for (int i = 0; i < 100; ++i)
    {
        coord_def c( random_range(MAPGEN_BORDER,
                                  GXM - MAPGEN_BORDER - 1),
                     random_range(MAPGEN_BORDER,
                                  GYM - MAPGEN_BORDER - 1) );

        if (unforbidden(c, mask) && grd(c) == grid)
            return c;
    }
    return coord_def(0, 0);
}

static void _connect_vault(const vault_placement &vp)
{
    std::vector<coord_def> exc = _external_connection_points(vp, vp.exits);
    for (int i = 0, size = exc.size(); i < size; ++i)
    {
        const coord_def &p = exc[i];
        const coord_def floor = _find_random_grid(DNGN_FLOOR, MMT_VAULT);

        if (!floor.x && !floor.y)
            continue;

        join_the_dots(p, floor, MMT_VAULT, true);
    }
}

static dungeon_feature_type _dgn_find_rune_subst(const std::string &tag)
{
    const std::string suffix("_entry");
    const std::string::size_type psuffix = tag.find(suffix);

    if (psuffix == std::string::npos)
        return (DNGN_FLOOR);

    const std::string key = tag.substr(0, psuffix);

    if (key == "bzr")
        return (DNGN_ENTER_PORTAL_VAULT);
    else if (key == "lab")
        return (DNGN_ENTER_LABYRINTH);
    else if (key == "hell")
        return (DNGN_ENTER_HELL);
    else if (key == "pan")
        return (DNGN_ENTER_PANDEMONIUM);
    else if (key == "abyss")
        return (DNGN_ENTER_ABYSS);
    else
    {
        for (int i = 0; i < NUM_BRANCHES; ++i)
            if (branches[i].entry_stairs != NUM_FEATURES
                && !strcasecmp(branches[i].abbrevname, key.c_str()))
            {
                return (branches[i].entry_stairs);
            }
    }
    return (DNGN_FLOOR);
}

static dungeon_feature_type _dgn_find_rune_subst_tags(const std::string &tags)
{
    std::vector<std::string> words = split_string(" ", tags);
    for (int i = 0, size = words.size(); i < size; ++i)
    {
        const dungeon_feature_type feat = _dgn_find_rune_subst(words[i]);
        if (feat != DNGN_FLOOR)
            return (feat);
    }
    return (DNGN_FLOOR);
}

void _fixup_after_vault()
{
    link_items();
    env.markers.activate_all();

    // Force teleport to place the player somewhere sane.
    you_teleport_now(false, false);

    setup_environment_effects();
}

// Places a map on the current level (minivault or regular vault).
//
// You can specify the centre of the map using "where" for floating vaults
// and minivaults. "where" is ignored for other vaults. XXX: it might be
// nice to specify a square that is not the centre, but is identified by
// a marker in the vault to be placed.
//
// NOTE: encompass maps will destroy the existing level!
//
// clobber: If true, assumes the newly placed vault can clobber existing
//          items and monsters (items may be destroyed, monsters may be
//          teleported).
bool dgn_place_map(const map_def *mdef,
                   bool clobber,
                   bool make_no_exits,
                   const coord_def &where,
                   int rune_subst)
{
    const dgn_colour_override_manager colour_man;

    bool did_map = false;
    if (mdef->orient == MAP_ENCOMPASS && !Generating_Level)
    {
        if (clobber)
        {
            // For encompass maps, clear the entire level.
            unwind_bool levgen(Generating_Level, true);
            dgn_reset_level();
            dungeon_events.clear();
            const bool res = dgn_place_map(mdef, clobber, make_no_exits,
                                           where);
            _fixup_after_vault();
            return (res);
        }
        else
        {
            mprf(MSGCH_DIAGNOSTICS,
                 "Cannot generate encompass map '%s' without clobber=true",
                 mdef->name.c_str());

            return (false);
        }
    }

    if (rune_subst == -1 && mdef->has_tag_suffix("_entry"))
        rune_subst = _dgn_find_rune_subst_tags(mdef->tags);
    if (!crawl_state.game_is_sprint() && !crawl_state.game_is_tutorial())
    {
        did_map = _build_secondary_vault(you.absdepth0, mdef, rune_subst,
                                         clobber, make_no_exits, where);
    }

    // Activate any markers within the map.
    if (did_map && !Generating_Level)
    {
        const vault_placement &vp = Level_Vaults[Level_Vaults.size() - 1];
        for (int y = vp.pos.y; y < vp.pos.y + vp.size.y; ++y)
            for (int x = vp.pos.x; x < vp.pos.x + vp.size.x; ++x)
            {
                std::vector<map_marker *> markers =
                    env.markers.get_markers_at(coord_def(x, y));
                std::vector<map_marker*> to_remove;
                for (int i = 0, size = markers.size(); i < size; ++i)
                {
                    markers[i]->activate();
                    const std::string prop =
                        markers[i]->property("post_activate_remove");
                    if (!prop.empty())
                        to_remove.push_back(markers[i]);
                }
                for (unsigned int i = 0; i < to_remove.size(); i++)
                    env.markers.remove(to_remove[i]);

                if (!you.see_cell(coord_def(x, y)))
                    set_terrain_changed(x, y);
            }

        setup_environment_effects();
        dgn_postprocess_level();
    }

    return (did_map);
}

void dgn_dig_vault_loose(vault_placement &vp)
{
    _dig_vault_loose(vp, vp.exits);
}

// Places a vault somewhere in an already built level if possible.
// Returns true if the vault was successfully placed.
static bool _build_secondary_vault(int level_number, const map_def *vault,
                                   int rune_subst, bool clobber,
                                   bool no_exits, const coord_def &where)
{
    if (_build_vaults(level_number, vault, rune_subst, true, !clobber,
                      no_exits, where))
    {
        const vault_placement &vp = Level_Vaults[ Level_Vaults.size() - 1 ];
        _connect_vault(vp);
        return (true);
    }
    return (false);
}

static bool _build_vaults(int level_number, const map_def *vault,
                          int rune_subst,
                          bool build_only, bool check_collisions,
                          bool make_no_exits, const coord_def &where)
{
    FixedVector < char, 10 > stair_exist;
    char stx, sty;

    if (dgn_check_connectivity && !dgn_zones)
        dgn_zones = _dgn_count_disconnected_zones(false);

    vault_placement place;

    place.level_number = level_number;
    place.rune_subst = rune_subst;

    if (map_bounds(where))
        place.pos = where;

    const int gluggy = vault_main(place, vault, check_collisions);

    if (gluggy == MAP_NONE)
        return (false);

    // XXX: Moved this out of dgn_register_place so that vault-set monsters can
    // be accessed with the '9' and '8' glyphs. (due)
    if (place.map.random_mons.size() > 0)
    {
        ASSERT(you.level_type == LEVEL_PORTAL_VAULT);
        set_vault_mon_list(place.map.random_mons);
    }

    place.apply_grid();
    dgn_register_place(place, true);

    std::vector<coord_def> &target_connections = place.exits;
    if (target_connections.empty() && gluggy != MAP_ENCOMPASS
        && (!place.map.is_minivault() || place.map.has_tag("mini_float")))
    {
        _pick_float_exits(place, target_connections);
    }

    if (make_no_exits)
        target_connections.clear();

    // Must do this only after target_connections is finalised, or the vault
    // exits will not be correctly set.
    Level_Vaults.push_back(place);
    remember_vault_placement(vault->has_tag("extra")
                             ? LEVEL_EXTRAS_KEY: LEVEL_VAULTS_KEY,
                             place);

#ifdef DEBUG_DIAGNOSTICS
    if (crawl_state.map_stat_gen)
        mapgen_report_map_use(place.map);
#endif

    bool is_layout = place.map.has_tag("layout");

    // If the map takes the whole screen or we were only requested to
    // build the vault, our work is done.
    if (gluggy == MAP_ENCOMPASS && !is_layout || build_only)
        return (true);

    // Does this level require Dis treatment (metal wallification)?
    // XXX: Change this so the level definition can explicitly state what
    // kind of wallification it wants.
    const bool dis_wallify = place.map.has_tag("dis");

    const int v1x = place.pos.x;
    const int v1y = place.pos.y;
    const int v2x = place.pos.x + place.size.x - 1;
    const int v2y = place.pos.y + place.size.y - 1;

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS,
            "Vault: (%d,%d)-(%d,%d); Dis: %s",
            v1x, v1y, v2x, v2y,
            dis_wallify? "yes" : "no");
#endif

    if (dis_wallify)
    {
        _plan_4(v1x, v1y, v2x, v2y, DNGN_METAL_WALL);
    }
    else if (!is_layout)
    {
        dgn_region_list excluded_regions;
        excluded_regions.push_back( dgn_region::absolute(v1x, v1y, v2x, v2y) );

        int nrooms = random_range(15, 90);

        // Try harder for floating vaults, which tend to complicate room
        // building somewhat.
        if (gluggy == MAP_FLOAT)
            nrooms += 10;

        std::vector<coord_def> ex_connection_points =
            _external_connection_points(place, target_connections);

        _build_rooms(excluded_regions, ex_connection_points, nrooms);

        // Excavate and connect the vault to the rest of the level.
        _dig_vault_loose(place, target_connections);
    }

    coord_def pos;

    for (stx = 0; stx < 10; stx++)
        stair_exist[stx] = 0;

    for (stx = 0; stx < GXM; stx++)
        for (sty = 0; sty < GYM; sty++)
            if (grd[stx][sty] >= DNGN_STONE_STAIRS_DOWN_I
                && grd[stx][sty] <= DNGN_ESCAPE_HATCH_UP)
            {
                stair_exist[grd[stx][sty] - DNGN_STONE_STAIRS_DOWN_I] = 1;
            }

    for (int j = 0; j < (coinflip()? 4 : 3); j++)
        for (int i = 0; i < 2; i++)
        {
            const dungeon_feature_type stair
                = static_cast<dungeon_feature_type>(
                   j + ((i == 0) ? DNGN_STONE_STAIRS_DOWN_I
                                 : DNGN_STONE_STAIRS_UP_I));

            if (stair_exist[stair - DNGN_STONE_STAIRS_DOWN_I] == 1)
                continue;

            int tries = 10000;
            do
                pos = random_in_bounds();
            while ((grd(pos) != DNGN_FLOOR
                    || (!is_layout && pos.x >= v1x && pos.x <= v2x
                        && pos.y >= v1y && pos.y <= v2y))
                   && tries-- > 0);


            if (tries <= 0)
            {
#ifdef DEBUG_DIAGNOSTICS
                dump_map("debug.map", true);
                end(1, false,
                    "Failed to create level: vault stairs for %s "
                    "(layout: %s) failed",
                    place.map.name.c_str(), is_layout? "yes" : "no");
#endif
                pos = you.pos();
            }

            grd(pos) = stair;
        }

    return (true);
}                               // end build_vaults()

static const object_class_type _acquirement_item_classes[] = {
    OBJ_WEAPONS,
    OBJ_ARMOUR,
    OBJ_WEAPONS,
    OBJ_JEWELLERY,
    OBJ_BOOKS,
    OBJ_STAVES,
    OBJ_MISCELLANY
};

static void _dgn_place_item_explicit(const item_spec &spec,
                                     const coord_def& where,
                                     int level)
{
    // Dummy object?
    if (spec.base_type == OBJ_UNASSIGNED)
        return;

    object_class_type base_type = spec.base_type;
    bool acquire = false;

    if (spec.level >= 0)
        level = spec.level;
    else
    {
        bool adjust_type = true;
        switch (spec.level)
        {
        case ISPEC_DAMAGED:
        case ISPEC_BAD:
        case ISPEC_RANDART:
            level = spec.level;
            break;
        case ISPEC_GOOD:
            level = 5 + level * 2;
            break;
        case ISPEC_SUPERB:
            level = MAKE_GOOD_ITEM;
            break;
        case ISPEC_ACQUIREMENT:
            acquire = true;
            break;
        default:
            adjust_type = false;
            break;
        }

        if (adjust_type && base_type == OBJ_RANDOM)
            base_type = RANDOM_ELEMENT(_acquirement_item_classes);
    }

    const int item_made =
        (acquire ?
         acquirement_create_item(base_type, spec.acquirement_source,
                                 true, where)
         : items( spec.allow_uniques, base_type,
                  spec.sub_type, true, level, spec.race, 0,
                  spec.ego ));

    if (item_made != NON_ITEM && item_made != -1)
    {
        item_def &item(mitm[item_made]);
        item.pos = where;
        CrawlHashTable props = spec.props;

        if (props.exists("make_book_theme_randart"))
        {
            make_book_theme_randart(item,
                props["randbook_disc1"].get_short(),
                props["randbook_disc2"].get_short(),
                props["randbook_num_spells"].get_short(),
                props["randbook_slevels"].get_short(),
                spell_by_name(props["randbook_spell"].get_string()),
                props["randbook_owner"].get_string());
        }

        // Remove unsuitable inscriptions such as {god gift}.
        item.inscription.clear();
        // And wipe item origin to remove "this is a god gift!" from there.
        origin_reset(item);
        if (is_stackable_item(item) && spec.qty > 0)
        {
            item.quantity = spec.qty;
            if (is_blood_potion(item))
                init_stack_blood_potions(item);
        }

        if (spec.plus >= 0
            && (item.base_type == OBJ_BOOKS
                && item.sub_type == BOOK_MANUAL)
            || (item.base_type == OBJ_MISCELLANY
                && item.sub_type == MISC_RUNE_OF_ZOT))
        {
            item.plus = spec.plus;
            item_colour(item);
        }

        if (props.exists("uncursed"))
            do_uncurse_item(item);
    }

    // [ds] Don't modify dungeon to accommodate items - vault
    // designers need more flexibility here.
}

void dgn_place_multiple_items(item_list &list,
                              const coord_def& where, int level)
{
    const int size = list.size();
    for (int i = 0; i < size; ++i)
        _dgn_place_item_explicit(list.get_item(i), where, level);
}

static void _dgn_place_item_explicit(int index, const coord_def& where,
                                     vault_placement &place,
                                     int level)
{
    item_list &sitems = place.map.items;

    if ((index < 0 || index >= static_cast<int>(sitems.size())) &&
        !crawl_state.game_is_sprint())
    {
        // Non-fatal, but we warn even in non-debug mode so there's incentive
        // to fix the problem.
        mprf(MSGCH_DIAGNOSTICS, "Map '%s' requested invalid item index: %d",
             place.map.name.c_str(), index);
        return;
    }

    const item_spec spec = sitems.get_item(index);
    _dgn_place_item_explicit(spec, where, level);
}

static void _dgn_give_mon_spec_items(mons_spec &mspec,
                                     const int mindex,
                                     const int mid,
                                     const int monster_level)
{
    monsters &mon(menv[mindex]);

    unwind_var<int> save_speedinc(mon.speed_increment);

    // Get rid of existing equipment.
    for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
        if (mon.inv[i] != NON_ITEM)
        {
            item_def &item(mitm[mon.inv[i]]);
            mon.unequip(item, i, 0, true);
            destroy_item(mon.inv[i], true);
            mon.inv[i] = NON_ITEM;
        }

    item_make_species_type racial = MAKE_ITEM_RANDOM_RACE;

    if (mons_genus(mid) == MONS_ORC)
        racial = MAKE_ITEM_ORCISH;
    else if (mons_genus(mid) == MONS_ELF)
        racial = MAKE_ITEM_ELVEN;

    item_list &list = mspec.items;

    const int size = list.size();
    for (int i = 0; i < size; ++i)
    {
        item_spec spec = list.get_item(i);

        if (spec.base_type == OBJ_UNASSIGNED)
            continue;

        // Don't give monster a randart, and don't randomly give
        // monster an ego item.
        if (spec.base_type == OBJ_ARMOUR || spec.base_type == OBJ_WEAPONS
            || spec.base_type == OBJ_MISSILES)
        {
            spec.allow_uniques = 0;
            if (spec.ego == 0)
                spec.ego = SP_FORBID_EGO;
        }

        // Gives orcs and elves appropriate racial gear, unless
        // otherwise specified.
        if (spec.race == MAKE_ITEM_RANDOM_RACE)
        {
            // But don't automatically give elves elven boots or
            // elven cloaks.
            if (racial != MAKE_ITEM_ELVEN || spec.base_type != OBJ_ARMOUR
                || (spec.sub_type != ARM_CLOAK
                    && spec.sub_type != ARM_BOOTS))
            {
                spec.race = racial;
            }
        }

        int item_level = monster_level;

        if (spec.level >= 0)
            item_level = spec.level;
        else
        {
            switch (spec.level)
            {
            case ISPEC_GOOD:
                item_level = 5 + item_level * 2;
                break;
            case ISPEC_SUPERB:
                item_level = MAKE_GOOD_ITEM;
                break;
            }
        }

        const int item_made = items( spec.allow_uniques, spec.base_type,
                                     spec.sub_type, true, item_level,
                                     spec.race, 0, spec.ego );

        if (item_made != NON_ITEM && item_made != -1)
        {
            item_def &item(mitm[item_made]);

            // Mark items on summoned monsters as such.
            if (mspec.abjuration_duration != 0)
                item.flags |= ISFLAG_SUMMONED;

            if (!mon.pickup_item(item, 0, true))
                destroy_item(item_made, true);
        }
    }

    // Pre-wield ranged weapons.
    if (mon.inv[MSLOT_WEAPON] == NON_ITEM
        && mon.inv[MSLOT_ALT_WEAPON] != NON_ITEM)
    {
        mon.swap_weapons(false);
    }
}


int dgn_place_monster(mons_spec &mspec,
                      int monster_level, const coord_def& where,
                      bool force_pos, bool generate_awake, bool patrolling)
{
    if (mspec.mid != -1)
    {
        const monster_type mid = static_cast<monster_type>(mspec.mid);
        const bool m_generate_awake = (generate_awake || mspec.generate_awake);
        const bool m_patrolling     = (patrolling || mspec.patrolling);
        const bool m_band           = mspec.band;

        const int mlev = mspec.mlevel;
        if (mlev)
        {
            if (mlev > 0)
                monster_level = mlev;
            else if (mlev == -8)
                monster_level = 4 + monster_level * 2;
            else if (mlev == -9)
                monster_level += 5;
        }

        if (mid != RANDOM_MONSTER && mid < NUM_MONSTERS)
        {
            // Don't place a unique monster a second time.
            // (Boris is handled specially.)
            if (mons_is_unique(mid) && you.unique_creatures[mid]
                && !crawl_state.game_is_arena())
            {
                return (-1);
            }

            const monster_type montype = mons_class_is_zombified(mid)
                                                             ? mspec.monbase
                                                             : mid;

            const habitat_type habitat = mons_class_primary_habitat(montype);

            if (!monster_habitable_grid(montype, grd(where)))
                dungeon_terrain_changed(where, habitat2grid(habitat));
        }

        mgen_data mg(mid);

        if (mg.cls == RANDOM_MONSTER && mspec.place.is_valid())
        {
            int lev = mspec.place.absdepth();

            if (mlev == -8)
                lev = 4 + lev * 2;
            else if (mlev == -9)
                lev += 5;

            int tries = 100;
            do
                mg.cls = pick_random_monster(mspec.place, lev, lev, NULL);
            while (mg.cls != MONS_NO_MONSTER
                     && mons_class_is_zombified(mspec.monbase)
                     && !mons_zombie_size(mg.cls)
                     && tries-- > 0);

            if (mg.cls == MONS_NO_MONSTER
                || (mons_class_is_zombified(mspec.monbase)
                    && !mons_zombie_size(mg.cls)))
            {
                mg.cls = RANDOM_MONSTER;
            }
        }

        mg.power     = monster_level;
        mg.behaviour = (m_generate_awake) ? BEH_WANDER : BEH_SLEEP;
        switch (mspec.attitude)
        {
        case ATT_FRIENDLY:
            mg.behaviour = BEH_FRIENDLY;
            break;
        case ATT_NEUTRAL:
            mg.behaviour = BEH_NEUTRAL;
            break;
        case ATT_GOOD_NEUTRAL:
            mg.behaviour = BEH_GOOD_NEUTRAL;
            break;
        case ATT_STRICT_NEUTRAL:
            mg.behaviour = BEH_STRICT_NEUTRAL;
            break;
        default:
            break;
        }
        mg.base_type = mspec.monbase;
        mg.number    = mspec.number;
        mg.colour    = mspec.colour;
        mg.mname     = mspec.monname;
        mg.hd        = mspec.hd;
        mg.hp        = mspec.hp;
        mg.props     = mspec.props;

        // Marking monsters as summoned
        mg.abjuration_duration = mspec.abjuration_duration;
        mg.summon_type         = mspec.summon_type;
        mg.non_actor_summoner  = mspec.non_actor_summoner;

        // XXX: hack (also, never hand out darkgrey)
        if (mg.colour == -1)
            mg.colour = random_monster_colour();

        coord_def place(where);
        if (!force_pos && monster_at(place)
            && (mg.cls < NUM_MONSTERS || mg.cls == RANDOM_MONSTER))
        {
            const monster_type habitat_target =
                mg.cls == RANDOM_MONSTER ? MONS_GIANT_BAT : mg.cls;
            place = find_newmons_square_contiguous(habitat_target, where, 0);
        }

        mg.pos = place;

        if (mons_class_is_zombified(mg.base_type))
        {
            if (mons_class_is_zombified(mg.cls))
                mg.base_type = MONS_NO_MONSTER;
            else
                std::swap(mg.base_type, mg.cls);
        }

        if (m_patrolling)
            mg.flags |= MG_PATROLLING;

        if (m_band)
            mg.flags |= MG_PERMIT_BANDS;

        // Store any extra flags here.
        mg.extra_flags |= mspec.extra_monster_flags;

        const int mindex = place_monster(mg, true);
        if (mindex != -1)
        {
            monsters &mons(menv[mindex]);
            if (!mspec.items.empty())
                _dgn_give_mon_spec_items(mspec, mindex, mid, monster_level);
            if (mspec.explicit_spells)
                mons.spells = mspec.spells;
            if (mspec.props.exists("monster_tile"))
                mons.props["monster_tile"] =
                    mspec.props["monster_tile"].get_short();
            if (mspec.props.exists("always_corpse"))
                mons.props["always_corpse"] = true;
            // These are applied earlier to prevent issues with renamed monsters
            // and "<monster> comes into view" (see delay.cc:_monster_warning).
            //mons.flags |= mspec.extra_monster_flags;
            if (mons.is_priest() && mons.god == GOD_NO_GOD)
                mons.god = GOD_NAMELESS;
        }
        return (mindex);
    }
    return (-1);
}

static bool _dgn_place_monster( const vault_placement &place, mons_spec &mspec,
                                int monster_level, const coord_def& where)
{
    const bool generate_awake
        = mspec.generate_awake || place.map.has_tag("generate_awake");

    const bool patrolling
        = mspec.patrolling || place.map.has_tag("patrolling");

    return (-1 != dgn_place_monster(mspec, monster_level, where, false,
                                    generate_awake, patrolling));
}

static bool _dgn_place_one_monster( const vault_placement &place,
                                    mons_list &mons, int monster_level,
                                    const coord_def& where)
{
    for (int i = 0, size = mons.size(); i < size; ++i)
    {
        mons_spec spec = mons.get_monster(i);
        if (_dgn_place_monster(place, spec, monster_level, where))
            return (true);
    }
    return (false);
}

// Grr, keep this in sync with vault_grid.
dungeon_feature_type map_feature_at(map_def *map, const coord_def &c,
                                    int rawfeat)
{
    if (rawfeat == -1)
        rawfeat = map->glyph_at(c);

    if (rawfeat == ' ')
        return (NUM_FEATURES);

    keyed_mapspec *mapsp = map? map->mapspec_at(c) : NULL;
    if (mapsp)
    {
        feature_spec f = mapsp->get_feat();
        if (f.trap >= 0)
        {
            // f.feat == 1 means trap is generated known.
            if (f.feat == 1)
                return trap_category(static_cast<trap_type>(f.trap));
            else
                return (DNGN_UNDISCOVERED_TRAP);
        }
        else if (f.feat >= 0)
            return static_cast<dungeon_feature_type>(f.feat);
        else if (f.glyph >= 0)
            return map_feature_at(NULL, c, f.glyph);
        else if (f.shop >= 0)
            return (DNGN_ENTER_SHOP);

        return (DNGN_FLOOR);
    }

    return ((rawfeat == 'x') ? DNGN_ROCK_WALL :
            (rawfeat == 'X') ? DNGN_PERMAROCK_WALL :
            (rawfeat == 'c') ? DNGN_STONE_WALL :
            (rawfeat == 'v') ? DNGN_METAL_WALL :
            (rawfeat == 'b') ? DNGN_GREEN_CRYSTAL_WALL :
            (rawfeat == 'a') ? DNGN_WAX_WALL :
            (rawfeat == 'm') ? DNGN_CLEAR_ROCK_WALL :
            (rawfeat == 'n') ? DNGN_CLEAR_STONE_WALL :
            (rawfeat == 'o') ? DNGN_CLEAR_PERMAROCK_WALL :
            (rawfeat == 't') ? DNGN_TREE :
            (rawfeat == '+') ? DNGN_CLOSED_DOOR :
            (rawfeat == '=') ? DNGN_SECRET_DOOR :
            (rawfeat == 'w') ? DNGN_DEEP_WATER :
            (rawfeat == 'W') ? DNGN_SHALLOW_WATER :
            (rawfeat == 'l') ? DNGN_LAVA :
            (rawfeat == '>') ? DNGN_ESCAPE_HATCH_DOWN :
            (rawfeat == '<') ? DNGN_ESCAPE_HATCH_UP :
            (rawfeat == '}') ? DNGN_STONE_STAIRS_DOWN_I :
            (rawfeat == '{') ? DNGN_STONE_STAIRS_UP_I :
            (rawfeat == ')') ? DNGN_STONE_STAIRS_DOWN_II :
            (rawfeat == '(') ? DNGN_STONE_STAIRS_UP_II :
            (rawfeat == ']') ? DNGN_STONE_STAIRS_DOWN_III :
            (rawfeat == '[') ? DNGN_STONE_STAIRS_UP_III :
            (rawfeat == 'A') ? DNGN_STONE_ARCH :
            (rawfeat == 'B') ? DNGN_ALTAR_ZIN :
            (rawfeat == 'C') ? _pick_an_altar() :   // f(x) elsewhere {dlb}
            (rawfeat == 'F') ? DNGN_GRANITE_STATUE :
            (rawfeat == 'I') ? DNGN_ORCISH_IDOL :
            (rawfeat == 'G') ? DNGN_GRANITE_STATUE :
            (rawfeat == 'T') ? DNGN_FOUNTAIN_BLUE :
            (rawfeat == 'U') ? DNGN_FOUNTAIN_SPARKLING :
            (rawfeat == 'V') ? DNGN_PERMADRY_FOUNTAIN :
            (rawfeat == 'Y') ? DNGN_FOUNTAIN_BLOOD :
            (rawfeat == '\0')? DNGN_ROCK_WALL
                             : DNGN_FLOOR); // includes everything else
}

static void _vault_grid(vault_placement &place,
                        int vgrid,
                        const coord_def& where,
                        keyed_mapspec *mapsp)
{
    if (!mapsp)
    {
        _vault_grid(place, vgrid, where);
        return;
    }

    const feature_spec f = mapsp->get_feat();
    if (f.trap >= 0)
    {
        const trap_type trap =
            (f.trap == TRAP_INDEPTH)
                ? random_trap_for_place(place.level_number)
                : static_cast<trap_type>(f.trap);

        place_specific_trap(where, trap);

        // f.feat == 1 means trap is generated known.
        if (f.feat == 1)
            grd(where) = trap_category(trap);
    }
    else if (f.feat >= 0)
    {
        grd(where) = static_cast<dungeon_feature_type>(f.feat);
        vgrid = -1;
    }
    else if (f.glyph >= 0)
    {
        _vault_grid(place, f.glyph, where);
    }
    else if (f.shop >= 0)
        place_spec_shop(place.level_number, where, f.shop);
    else
        grd(where) = DNGN_FLOOR;

    mons_list &mons = mapsp->get_monsters();
    _dgn_place_one_monster(place, mons, place.level_number, where);

    item_list &items = mapsp->get_items();
    dgn_place_multiple_items(items, where, place.level_number);
}

static void _vault_grid(vault_placement &place,
                         int vgrid,
                         const coord_def& where)
{
    // First, set base tile for grids {dlb}:
    grd(where)  = ((vgrid == -1)  ? grd(where) :
                   (vgrid == 'x') ? DNGN_ROCK_WALL :
                   (vgrid == 'X') ? DNGN_PERMAROCK_WALL :
                   (vgrid == 'c') ? DNGN_STONE_WALL :
                   (vgrid == 'v') ? DNGN_METAL_WALL :
                   (vgrid == 'b') ? DNGN_GREEN_CRYSTAL_WALL :
                   (vgrid == 'a') ? DNGN_WAX_WALL :
                   (vgrid == 'm') ? DNGN_CLEAR_ROCK_WALL :
                   (vgrid == 'n') ? DNGN_CLEAR_STONE_WALL :
                   (vgrid == 'o') ? DNGN_CLEAR_PERMAROCK_WALL :
                   (vgrid == 't') ? DNGN_TREE :
                   (vgrid == '+') ? DNGN_CLOSED_DOOR :
                   (vgrid == '=') ? DNGN_SECRET_DOOR :
                   (vgrid == 'w') ? DNGN_DEEP_WATER :
                   (vgrid == 'W') ? DNGN_SHALLOW_WATER :
                   (vgrid == 'l') ? DNGN_LAVA :
                   (vgrid == '>') ? DNGN_ESCAPE_HATCH_DOWN :
                   (vgrid == '<') ? DNGN_ESCAPE_HATCH_UP :
                   (vgrid == '}') ? DNGN_STONE_STAIRS_DOWN_I :
                   (vgrid == '{') ? DNGN_STONE_STAIRS_UP_I :
                   (vgrid == ')') ? DNGN_STONE_STAIRS_DOWN_II :
                   (vgrid == '(') ? DNGN_STONE_STAIRS_UP_II :
                   (vgrid == ']') ? DNGN_STONE_STAIRS_DOWN_III :
                   (vgrid == '[') ? DNGN_STONE_STAIRS_UP_III :
                   (vgrid == 'A') ? DNGN_STONE_ARCH :
                   (vgrid == 'B') ? _pick_temple_altar(place) :
                   (vgrid == 'C') ? _pick_an_altar() :   // f(x) elsewhere {dlb}
                   (vgrid == 'I') ? DNGN_ORCISH_IDOL :
                   (vgrid == 'G') ? DNGN_GRANITE_STATUE :
                   (vgrid == 'T') ? DNGN_FOUNTAIN_BLUE :
                   (vgrid == 'U') ? DNGN_FOUNTAIN_SPARKLING :
                   (vgrid == 'V') ? DNGN_PERMADRY_FOUNTAIN :
                   (vgrid == 'Y') ? DNGN_FOUNTAIN_BLOOD :
                   (vgrid == '\0')? DNGN_ROCK_WALL
                                  : DNGN_FLOOR); // includes everything else

    if (grd(where) == DNGN_ALTAR_JIYVA && jiyva_is_dead())
        grd(where) = DNGN_FLOOR;

    // then, handle oddball grids {dlb}:
    switch (vgrid)
    {
    case '@':
        place.exits.push_back( where );
        break;
    case '^':
        place_specific_trap(where, TRAP_RANDOM);
        break;
    case '~':
        place_specific_trap(where, random_trap_for_place(place.level_number));
        break;
    case 'O':
        if (place.rune_subst != -1)
            grd(where) = static_cast<dungeon_feature_type>(place.rune_subst);
        break;
    }

    if ((vgrid == '=' || vgrid == '+')
        && (where.x == place.pos.x || where.y == place.pos.y
            || where.x == place.pos.x + place.size.x - 1
            || where.y == place.pos.y + place.size.y - 1))
    {
        place.exits.push_back( where );
    }

    // Then, handle grids that place "stuff" {dlb}:
    // yes, I know this is a bit ugly ... {dlb}
    switch (vgrid)
    {
        case '$':
        case '%':
        case '*':
        case '|':
        {
            int item_made = NON_ITEM;
            object_class_type which_class = OBJ_RANDOM;
            unsigned char which_type = OBJ_RANDOM;
            int which_depth;
            int spec = 250;

            if (vgrid == '$')
            {
                which_class = OBJ_GOLD;
                which_type = OBJ_RANDOM;
            }
            else if (vgrid == '|')
            {
                which_class = RANDOM_ELEMENT(_acquirement_item_classes);
                which_type = OBJ_RANDOM;
            }
            else
            {
                which_class = OBJ_RANDOM;
                which_type = OBJ_RANDOM;
            }

            which_depth = place.level_number;
            if (vgrid == '|')
                which_depth = MAKE_GOOD_ITEM;
            else if (vgrid == '*')
                which_depth = 5 + (place.level_number * 2);

            item_made = items( 1, which_class, which_type, true,
                               which_depth, spec );

            if (item_made != NON_ITEM)
            {
                mitm[item_made].pos = where;
            }
        }
        break;
    }

    // defghijk - items
    if (map_def::valid_item_array_glyph(vgrid))
    {
        int slot = map_def::item_array_glyph_to_slot(vgrid);
        _dgn_place_item_explicit(slot, where, place, place.level_number);
    }

    // Finally, handle grids that place monsters {dlb}:
    if (map_def::valid_monster_glyph(vgrid))
    {
        int monster_level;
        mons_spec monster_type_thing(RANDOM_MONSTER);

        monster_level = place.level_number;
        if (vgrid == '8')
            monster_level = 4 + (place.level_number * 2);
        else if (vgrid == '9')
            monster_level = 5 + place.level_number;

        if (monster_level > 30) // very high level monsters more common here
            monster_level = 30;

        if (vgrid != '8' && vgrid != '9' && vgrid != '0')
        {
            int slot = map_def::monster_array_glyph_to_slot(vgrid);
            monster_type_thing = place.map.mons.get_monster(slot);
            monster_type mt = static_cast<monster_type>(monster_type_thing.mid);
            // Is a map for a specific place trying to place a unique which
            // somehow already got created?
            if (place.map.place.is_valid()
                && !invalid_monster_type(mt)
                && mons_is_unique(mt)
                && you.unique_creatures[mt])
            {
                mprf(MSGCH_ERROR, "ERROR: %s already generated somewhere "
                     "else; please file a bug report.",
                     mons_type_name(mt, DESC_CAP_THE).c_str());
                // Force it to be generated anyway.
                you.unique_creatures[mt] = false;
            }
        }

        _dgn_place_monster(place, monster_type_thing, monster_level, where);
    }
}

// Currently only used for Slime: branch end
// where it will turn the stone walls into clear rock walls
// once the royal jelly has been killed.
bool seen_replace_feat(dungeon_feature_type old_feat,
                       dungeon_feature_type new_feat)
{
    ASSERT(old_feat != new_feat);

    coord_def p1(0, 0);
    coord_def p2(GXM - 1, GYM - 1);

    bool seen = false;
    for (rectangle_iterator ri(p1, p2); ri; ++ri)
    {
        if (grd(*ri) == old_feat)
        {
            grd(*ri) = new_feat;
            if (you.see_cell(*ri))
                seen = true;
        }
    }

    return (seen);
}

void dgn_replace_area(int sx, int sy, int ex, int ey,
                      dungeon_feature_type replace,
                      dungeon_feature_type feature,
                      unsigned mmask, bool needs_update)
{
    dgn_replace_area( coord_def(sx, sy), coord_def(ex, ey),
                      replace, feature, mmask, needs_update );
}

void dgn_replace_area( const coord_def& p1, const coord_def& p2,
                       dungeon_feature_type replace,
                       dungeon_feature_type feature, unsigned mapmask,
                       bool needs_update)
{
    for (rectangle_iterator ri(p1, p2); ri; ++ri)
    {
        if (grd(*ri) == replace && unforbidden(*ri, mapmask))
        {
            grd(*ri) = feature;
            if (needs_update && is_terrain_seen(*ri))
            {
                set_map_knowledge_obj(*ri, feature);
#ifdef USE_TILE
                env.tile_bk_bg(*ri) = feature;
#endif
            }
        }
    }
}


// With apologies to Metallica.
bool unforbidden(const coord_def &c, unsigned mask)
{
    return (!mask || !(dgn_Map_Mask(c) & mask));
}

struct coord_comparator
{
    coord_def target;
    coord_comparator(const coord_def &t) : target(t) { }

    static int dist(const coord_def &a, const coord_def &b)
    {
        const coord_def del = a - b;
        return std::abs(del.x) * GYM + std::abs(del.y);
    }

    bool operator () (const coord_def &a, const coord_def &b) const
    {
        return dist(a, target) < dist(b, target);
    }
};

typedef std::set<coord_def, coord_comparator> coord_set;

static void _jtd_init_surrounds(coord_set &coords, unsigned mapmask,
                                const coord_def &c)
{
    for (int yi = -1; yi <= 1; ++yi)
        for (int xi = -1; xi <= 1; ++xi)
        {
            if (!xi == !yi)
                continue;

            const coord_def cx(c.x + xi, c.y + yi);
            if (!in_bounds(cx) || travel_point_distance[cx.x][cx.y]
                || !unforbidden(cx, mapmask))
            {
                continue;
            }
            coords.insert(cx);

            travel_point_distance[cx.x][cx.y] = (-xi + 2) * 4 + (-yi + 2);
        }
}

static bool _join_the_dots_pathfind(coord_set &coords,
                                    const coord_def &from, const coord_def &to,
                                    unsigned mapmask, bool early_exit)
{
    coord_def curr = from;
    while (true)
    {
        int &tpd = travel_point_distance[curr.x][curr.y];
        tpd = !tpd? -1000 : -tpd;

        if (curr == to)
            break;

        _jtd_init_surrounds(coords, mapmask, curr);

        if (coords.empty())
            break;

        curr = *coords.begin();
        coords.erase(coords.begin());
    }

    if (curr != to)
        return (false);

    while (curr != from)
    {
        if (unforbidden(curr, mapmask))
            grd(curr) = DNGN_FLOOR;

        const int dist = travel_point_distance[curr.x][curr.y];
        ASSERT(dist < 0 && dist != -1000);
        curr += coord_def(-dist / 4 - 2, (-dist % 4) - 2);
    }
    if (unforbidden(curr, mapmask))
        grd(curr) = DNGN_FLOOR;

    return (true);
}

static bool _join_the_dots_rigorous(const coord_def &from,
                                    const coord_def &to,
                                    unsigned mapmask,
                                    bool early_exit)
{
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));

    const coord_comparator comp(to);
    coord_set coords(comp);
    const bool found = _join_the_dots_pathfind(coords, from, to, mapmask,
                                               early_exit);

    return (found);
}

bool join_the_dots(const coord_def &from, const coord_def &to,
                   unsigned mapmask, bool early_exit)
{
    if (from == to)
        return (true);

    int join_count = 0;

    coord_def at = from;
    do
    {
        join_count++;

        const dungeon_feature_type feat = grd(at);
        if (early_exit && at != from && feat_is_traversable(feat))
            return (true);

        if (unforbidden(at, MMT_VAULT) && !feat_is_traversable(feat))
            grd(at) = DNGN_FLOOR;

        if (join_count > 10000) // just insurance
            return (false);

        if (at.x < to.x
            && unforbidden(coord_def(at.x + 1, at.y), mapmask))
        {
            at.x++;
            continue;
        }

        if (at.x > to.x
            && unforbidden(coord_def(at.x - 1, at.y), mapmask))
        {
            at.x--;
            continue;
        }

        if (at.y > to.y
            && unforbidden(coord_def(at.x, at.y - 1), mapmask))
        {
            at.y--;
            continue;
        }

        if (at.y < to.y
            && unforbidden(coord_def(at.x, at.y + 1), mapmask))
        {
            at.y++;
            continue;
        }

        // If we get here, no progress can be made anyway. Why use the
        // join_count insurance?
        break;
    }
    while (at != to && in_bounds(at));

    if (in_bounds(at) && unforbidden(at, mapmask))
        grd(at) = DNGN_FLOOR;

    return (at == to);
}                               // end join_the_dots()

static void _place_pool(dungeon_feature_type pool_type, unsigned char pool_x1,
                        unsigned char pool_y1, unsigned char pool_x2,
                        unsigned char pool_y2)
{
    int i, j;
    unsigned char left_edge, right_edge;

    // Don't place LAVA pools in crypt... use shallow water instead.
    if (pool_type == DNGN_LAVA
        && (player_in_branch(BRANCH_CRYPT) || player_in_branch(BRANCH_TOMB)))
    {
        pool_type = DNGN_SHALLOW_WATER;
    }

    if (pool_x1 >= pool_x2 - 4 || pool_y1 >= pool_y2 - 4)
        return;

    left_edge  = pool_x1 + 2 + random2(pool_x2 - pool_x1);
    right_edge = pool_x2 - 2 - random2(pool_x2 - pool_x1);

    for (j = pool_y1 + 1; j < pool_y2 - 1; j++)
    {
        for (i = pool_x1 + 1; i < pool_x2 - 1; i++)
        {
            if (i >= left_edge && i <= right_edge && grd[i][j] == DNGN_FLOOR)
                grd[i][j] = pool_type;
        }

        if (j - pool_y1 < (pool_y2 - pool_y1) / 2 || one_chance_in(4))
        {
            if (left_edge > pool_x1 + 1)
                left_edge -= random2(3);

            if (right_edge < pool_x2 - 1)
                right_edge += random2(3);
        }

        if (left_edge < pool_x2 - 1
            && (j - pool_y1 >= (pool_y2 - pool_y1) / 2
                || left_edge <= pool_x1 + 2 || one_chance_in(4)))
        {
            left_edge += random2(3);
        }

        if (right_edge > pool_x1 + 1
            && (j - pool_y1 >= (pool_y2 - pool_y1) / 2
                || right_edge >= pool_x2 - 2 || one_chance_in(4)))
        {
            right_edge -= random2(3);
        }
    }
}                               // end place_pool()

static void _many_pools(dungeon_feature_type pool_type)
{
    if (player_in_branch( BRANCH_COCYTUS ))
        pool_type = DNGN_DEEP_WATER;
    else if (player_in_branch( BRANCH_GEHENNA ))
        pool_type = DNGN_LAVA;

    const int num_pools = 20 + random2avg(9, 2);
    int pools = 0;

    std::string extra = make_stringf("many pools [%d %d]", (int) pool_type,
                                     num_pools);
    env.properties[LEVEL_EXTRAS_KEY].get_vector().push_back(extra);

    for (int timeout = 0; pools < num_pools && timeout < 30000; ++timeout)
    {
        const int i = random_range(X_BOUND_1 + 1, X_BOUND_2 - 21);
        const int j = random_range(Y_BOUND_1 + 1, Y_BOUND_2 - 21);
        const int k = i + 2 + roll_dice( 2, 9 );
        const int l = j + 2 + roll_dice( 2, 9 );

        if (count_antifeature_in_box(i, j, k, l, DNGN_FLOOR) == 0)
        {
            _place_pool(pool_type, i, j, k, l);
            pools++;
        }
    }
}                               // end many_pools()

static dungeon_feature_type _pick_temple_altar(vault_placement &place)
{
    if (_temple_altar_list.empty())
    {
        if (_current_temple_hash != NULL)
        {
            mprf("Ran out of altars for temple!", MSGCH_ERROR);
            return (DNGN_FLOOR);
        }
        // Randomized altar list for mini-temples.
        _temple_altar_list = temple_god_list();
        std::random_shuffle(_temple_altar_list.begin(), _temple_altar_list.end());
    }

    const god_type god = _temple_altar_list.back();

    _temple_altar_list.pop_back();

    return altar_for_god(god);
}

//jmf: Generate altar based on where you are, or possibly randomly.
static dungeon_feature_type _pick_an_altar()
{
    dungeon_feature_type altar_type;
    int temp_rand;              // probability determination {dlb}

    if (player_in_branch(BRANCH_ECUMENICAL_TEMPLE)
        || you.level_type == LEVEL_LABYRINTH)
    {
        // No extra altars in Temple, none at all in Labyrinth.
        altar_type = DNGN_FLOOR;
    }
    else if (you.level_type == LEVEL_DUNGEON && !one_chance_in(5))
    {
        switch (you.where_are_you)
        {
        case BRANCH_CRYPT:
            altar_type = (coinflip() ? DNGN_ALTAR_KIKUBAAQUDGHA
                                     : DNGN_ALTAR_YREDELEMNUL);
            break;

        case BRANCH_ORCISH_MINES:    // violent gods
            temp_rand = random2(10); // 50% chance of Beogh

            altar_type = ((temp_rand == 0) ? DNGN_ALTAR_VEHUMET :
                          (temp_rand == 1) ? DNGN_ALTAR_MAKHLEB :
                          (temp_rand == 2) ? DNGN_ALTAR_OKAWARU :
                          (temp_rand == 3) ? DNGN_ALTAR_TROG :
                          (temp_rand == 4) ? DNGN_ALTAR_XOM
                                           : DNGN_ALTAR_BEOGH);
            break;

        case BRANCH_VAULTS: // "lawful" gods
            temp_rand = random2(7);

            altar_type = ((temp_rand == 0) ? DNGN_ALTAR_ELYVILON :
                          (temp_rand == 1) ? DNGN_ALTAR_SIF_MUNA :
                          (temp_rand == 2) ? DNGN_ALTAR_SHINING_ONE :
                          (temp_rand == 3
                              || temp_rand == 4) ? DNGN_ALTAR_OKAWARU
                                                 : DNGN_ALTAR_ZIN);
            break;

        case BRANCH_HALL_OF_BLADES:
            altar_type = DNGN_ALTAR_OKAWARU;
            break;

        case BRANCH_ELVEN_HALLS:    // "magic" gods
            temp_rand = random2(4);

            altar_type = ((temp_rand == 0) ? DNGN_ALTAR_VEHUMET :
                          (temp_rand == 1) ? DNGN_ALTAR_SIF_MUNA :
                          (temp_rand == 2) ? DNGN_ALTAR_XOM
                                           : DNGN_ALTAR_MAKHLEB);
            break;

        case BRANCH_SLIME_PITS:
            altar_type = jiyva_is_dead() ? DNGN_FLOOR : DNGN_ALTAR_JIYVA;
            break;

        case BRANCH_TOMB:
            altar_type = DNGN_ALTAR_KIKUBAAQUDGHA;
            break;

        default:
            do
            {
                altar_type =
                    static_cast<dungeon_feature_type>(DNGN_ALTAR_FIRST_GOD +
                                                      random2(NUM_GODS - 1));
            }
            while (altar_type == DNGN_ALTAR_NEMELEX_XOBEH
                   || altar_type == DNGN_ALTAR_LUGONU
                   || altar_type == DNGN_ALTAR_BEOGH
                   || altar_type == DNGN_ALTAR_JIYVA);
            break;
        }
    }
    else
    {
        // Note: this case includes Pandemonium or the Abyss.
        temp_rand = random2(9);

        altar_type = ((temp_rand == 0) ? DNGN_ALTAR_ZIN :
                      (temp_rand == 1) ? DNGN_ALTAR_SHINING_ONE :
                      (temp_rand == 2) ? DNGN_ALTAR_KIKUBAAQUDGHA :
                      (temp_rand == 3) ? DNGN_ALTAR_XOM :
                      (temp_rand == 4) ? DNGN_ALTAR_OKAWARU :
                      (temp_rand == 5) ? DNGN_ALTAR_MAKHLEB :
                      (temp_rand == 6) ? DNGN_ALTAR_SIF_MUNA :
                      (temp_rand == 7) ? DNGN_ALTAR_TROG
                                       : DNGN_ALTAR_ELYVILON);
    }

    return (altar_type);
}

static void _place_altars()
{
    // No altars before level 5.
    if (you.absdepth0 < 4)
        return;

    if (you.level_type == LEVEL_DUNGEON)
    {
        int prob = your_branch().altar_chance;
        while (prob)
        {
            if (random2(100) >= prob)
                break;

            dprf("Placing an altar");
            _place_altar();
            // Reduce the chance and try to place another.
            prob /= 5;
        }
    }
}

static void _place_altar()
{
    for (int numtry = 0; numtry < 5000; ++numtry)
    {
        int px = 15 + random2(55);
        int py = 15 + random2(45);

        const int numfloors = count_feature_in_box(px-2, py-2, px+3, py+3,
                                                   DNGN_FLOOR);
        const int numgood =
            count_feature_in_box(px-2, py-2, px+3, py+3, DNGN_ROCK_WALL) +
            count_feature_in_box(px-2, py-2, px+3, py+3, DNGN_CLOSED_DOOR) +
            count_feature_in_box(px-2, py-2, px+3, py+3, DNGN_SECRET_DOOR) +
            count_feature_in_box(px-2, py-2, px+3, py+3, DNGN_FLOOR);

        if (numgood < 5*5 || numfloors == 0)
            continue;

        bool mon_there = false;

        for (int i = px - 2; i <= px + 2; i++)
            for (int j = py - 2; j <= py + 2; j++)
            {
                if (monster_at(coord_def(i, j)))
                    mon_there = true;
            }

        if (mon_there)
            continue;

        for (int i = px - 2; i <= px + 2; i++)
            for (int j = py - 2; j <= py + 2; j++)
                grd[i][j] = DNGN_FLOOR;

        grd[px][py] = _pick_an_altar();
        break;
    }
}

// If a level gets shops, how many there are.
// Just one most of the time; expected value is 1.42.
static int _num_shops()
{
    if (x_chance_in_y(5, 6))
        return (1);
    else
        return (random_range(2, MAX_RANDOM_SHOPS));
}

static void _place_shops(int level_number)
{
    coord_def shop_place;

    int nshops;
    bool allow_bazaars = true;

#ifdef DEBUG_SHOPS
    nshops = MAX_SHOPS;
#else
    if (level_number < 3)
        return;
    if (!x_chance_in_y(your_branch().shop_chance, 100))
        return;
    nshops = _num_shops();
#endif

    for (int i = 0; i < nshops; i++)
    {
        int timeout = 0;

        do
        {
            shop_place = random_in_bounds();

            timeout++;

            if (timeout > 10000)
                return;
        }
        while (grd(shop_place) != DNGN_FLOOR
               && unforbidden(shop_place, MMT_NO_SHOP));

        if (allow_bazaars && level_number > 9 && level_number < 27
            && one_chance_in(30 - level_number))
        {
            _place_specific_stair(DNGN_ENTER_PORTAL_VAULT,
                                 "bzr_entry", level_number, true);
            allow_bazaars = false;
        }
        else
            place_spec_shop(level_number, shop_place, SHOP_RANDOM);
    }
}

static bool _need_varied_selection(shop_type shop)
{
    return (shop == SHOP_BOOK);
}

void place_spec_shop( int level_number,
                      const coord_def& where,
                      int force_s_type, bool representative )
{
    int orb = 0;
    int i = 0;
    int j = 0;                  // loop variable
    int item_level;

    bool note_status = notes_are_active();
    activate_notes(false);

    if (!unforbidden(where, MMT_NO_SHOP))
        return;

    for (i = 0; i < MAX_SHOPS; i++)
        if (env.shop[i].type == SHOP_UNASSIGNED)
            break;

    if (i == MAX_SHOPS)
        return;

    for (j = 0; j < 3; j++)
        env.shop[i].keeper_name[j] = 1 + random2(200);

    env.shop[i].level = level_number * 2;

    env.shop[i].type = static_cast<shop_type>(
        (force_s_type != SHOP_RANDOM) ? force_s_type
                                      : random2(NUM_SHOPS));

    if (env.shop[i].type == SHOP_FOOD)
    {
        env.shop[i].greed = 10 + random2(5);
    }
    else if (env.shop[i].type != SHOP_WEAPON_ANTIQUE
             && env.shop[i].type != SHOP_ARMOUR_ANTIQUE
             && env.shop[i].type != SHOP_GENERAL_ANTIQUE)
    {
        env.shop[i].greed = 10 + random2(5) + random2(level_number / 2);
    }
    else
        env.shop[i].greed = 15 + random2avg(19, 2) + random2(level_number);

    // Allow bargains in bazaars, prices randomly between 60% and 95%.
    if (you.level_type == LEVEL_PORTAL_VAULT && you.level_type_tag == "bazaar")
    {
        // Need to calculate with factor as greed (unsigned char)
        // is capped at 255.
        int factor = random2(8) + 12;
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "shop type %d: original greed = %d, factor = %d",
                                env.shop[i].type, env.shop[i].greed, factor);
        mprf(MSGCH_DIAGNOSTICS, "discount at shop %d is %d%%", i, (20-factor)*5);
#endif
        factor *= env.shop[i].greed;
        factor /= 20;
        env.shop[i].greed = factor;
    }

    int plojy = 5 + random2avg(12, 3);
    if (representative)
        plojy = env.shop[i].type == SHOP_WAND? NUM_WANDS : 16;

    // For books shops, store how many copies of a given book are on display.
    // This increases the diversity of books in a shop.
    int stocked[NUM_BOOKS];
    if (_need_varied_selection(env.shop[i].type))
    {
        for (int k = 0; k < NUM_BOOKS; k++)
             stocked[k] = 0;
    }

    for (j = 0; j < plojy; j++)
    {
        if (env.shop[i].type != SHOP_WEAPON_ANTIQUE
            && env.shop[i].type != SHOP_ARMOUR_ANTIQUE
            && env.shop[i].type != SHOP_GENERAL_ANTIQUE)
        {
            item_level = level_number + random2((level_number + 1) * 2);
        }
        else
        {
            item_level = level_number + random2((level_number + 1) * 3);
        }

        // Make bazaar items more valuable (up to double value).
        if (you.level_type == LEVEL_PORTAL_VAULT
            && you.level_type_tag == "bazaar")
        {
            int help = random2(item_level) + 1;
            item_level += help;

            if (item_level > level_number * 5)
                item_level = level_number * 5;
        }

        // Don't generate gold in shops! This used to be possible with
        // general stores (see item_in_shop() below)   (GDL)
        while (true)
        {
            const int subtype = representative? j : OBJ_RANDOM;
            orb = items( 1, _item_in_shop(env.shop[i].type), subtype, true,
                         one_chance_in(4)? MAKE_GOOD_ITEM : item_level,
                         MAKE_ITEM_RANDOM_RACE );

            // Try for a better selection.
            if (orb != NON_ITEM && _need_varied_selection(env.shop[i].type))
            {
                if (!one_chance_in(stocked[mitm[orb].sub_type] + 1))
                {
                    mitm[orb].clear();
                    orb = NON_ITEM; // try again
                }
            }

            if (orb != NON_ITEM
                && mitm[orb].base_type != OBJ_GOLD
                && (env.shop[i].type != SHOP_GENERAL_ANTIQUE
                    || (mitm[orb].base_type != OBJ_MISSILES
                        && mitm[orb].base_type != OBJ_FOOD)))
            {
                break;
            }

            // Reset object and try again.
            if (orb != NON_ITEM)
                mitm[orb].clear();
        }

        if (orb == NON_ITEM)
            break;

        item_def& item( mitm[orb] );

        // Increase stock of this subtype by 1, unless it is an artefact
        // (allow for several artefacts of the same underlying subtype)
        // - the latter is currently unused but would apply to e.g. jewellery.
        if (_need_varied_selection(env.shop[i].type) && !is_artefact(item))
            stocked[item.sub_type]++;

        if (representative && item.base_type == OBJ_WANDS)
            item.plus = 7;

        // Set object 'position' (gah!) & ID status.
        item.pos.set(0, 5+i);

        if (env.shop[i].type != SHOP_WEAPON_ANTIQUE
            && env.shop[i].type != SHOP_ARMOUR_ANTIQUE
            && env.shop[i].type != SHOP_GENERAL_ANTIQUE)
        {
            set_ident_flags( item, ISFLAG_IDENT_MASK );
        }
    }

    env.shop[i].pos = where;

    grd(where) = DNGN_ENTER_SHOP;

    activate_notes(note_status);
}                               // end place_spec_shop()

static object_class_type _item_in_shop(unsigned char shop_type)
{
    switch (shop_type)
    {
    case SHOP_WEAPON:
        if (one_chance_in(5))
            return (OBJ_MISSILES);
        // *** deliberate fall through here  {dlb} ***
    case SHOP_WEAPON_ANTIQUE:
        return (OBJ_WEAPONS);

    case SHOP_ARMOUR:
    case SHOP_ARMOUR_ANTIQUE:
        return (OBJ_ARMOUR);

    case SHOP_GENERAL:
    case SHOP_GENERAL_ANTIQUE:
        return (OBJ_RANDOM);

    case SHOP_JEWELLERY:
        return (OBJ_JEWELLERY);

    case SHOP_WAND:
        return (OBJ_WANDS);

    case SHOP_BOOK:
        return (OBJ_BOOKS);

    case SHOP_FOOD:
        return (OBJ_FOOD);

    case SHOP_DISTILLERY:
        return (OBJ_POTIONS);

    case SHOP_SCROLL:
        return (OBJ_SCROLLS);
    }

    return (OBJ_RANDOM);
}                               // end item_in_shop()

void spotty_level(bool seeded, int iterations, bool boxy)
{
    dgn_Build_Method += make_stringf(" spotty_level [%d%s%s]",
                                     iterations, seeded ? " seeeded" : "",
                                     boxy ? " boxy" : "");
    if (dgn_Layout_Type == "open" || dgn_Layout_Type == "cross")
        dgn_Layout_Type = "open";
    else if (iterations >= 100)
        dgn_Layout_Type  = "caves";
    else if (dgn_Layout_Type.empty())
        dgn_Layout_Type = "spotty";
    else
        dgn_Layout_Type += "_spotty";

    // Assumes starting with a level full of rock walls (1).
    int i, j, k, l;

    if (!seeded)
    {
        for (i = DNGN_STONE_STAIRS_DOWN_I; i < DNGN_ESCAPE_HATCH_UP; i++)
        {
            if (i == DNGN_ESCAPE_HATCH_DOWN
                || i == DNGN_STONE_STAIRS_UP_I
                   && !player_in_branch( BRANCH_SLIME_PITS ))
            {
                continue;
            }

            do
            {
                j = random_range(X_BOUND_1 + 4, X_BOUND_2 - 4);
                k = random_range(Y_BOUND_1 + 4, Y_BOUND_2 - 4);
            }
            while (grd[j][k] != DNGN_ROCK_WALL
                   && grd[j + 1][k] != DNGN_ROCK_WALL);

            grd[j][k] = static_cast<dungeon_feature_type>(i);

            // creating elevators
            if (i == DNGN_STONE_STAIRS_DOWN_I
                && !player_in_branch( BRANCH_SLIME_PITS ))
            {
                grd[j + 1][k] = DNGN_STONE_STAIRS_UP_I;
            }

            if (grd[j][k - 1] == DNGN_ROCK_WALL)
                grd[j][k - 1] = DNGN_FLOOR;
            if (grd[j][k + 1] == DNGN_ROCK_WALL)
                grd[j][k + 1] = DNGN_FLOOR;
            if (grd[j - 1][k] == DNGN_ROCK_WALL)
                grd[j - 1][k] = DNGN_FLOOR;
            if (grd[j + 1][k] == DNGN_ROCK_WALL)
                grd[j + 1][k] = DNGN_FLOOR;
        }
    }                           // end if !seeded

    l = iterations;

    // Boxy levels have more clearing, so they get fewer iterations.
    if (l == 0)
        l = 200 + random2((boxy ? 750 : 1500));

    for (i = 0; i < l; ++i)
    {
        do
        {
            j = random_range(X_BOUND_1 + 4, X_BOUND_2 - 4);
            k = random_range(Y_BOUND_1 + 4, Y_BOUND_2 - 4);
        }
        while (grd[j][k] == DNGN_ROCK_WALL
               && grd[j - 1][k] == DNGN_ROCK_WALL
               && grd[j + 1][k] == DNGN_ROCK_WALL
               && grd[j][k - 1] == DNGN_ROCK_WALL
               && grd[j][k + 1] == DNGN_ROCK_WALL
               && grd[j - 2][k] == DNGN_ROCK_WALL
               && grd[j + 2][k] == DNGN_ROCK_WALL
               && grd[j][k - 2] == DNGN_ROCK_WALL
               && grd[j][k + 2] == DNGN_ROCK_WALL);

        if (grd[j][k] == DNGN_ROCK_WALL)
            grd[j][k] = DNGN_FLOOR;
        if (grd[j][k - 1] == DNGN_ROCK_WALL)
            grd[j][k - 1] = DNGN_FLOOR;
        if (grd[j][k + 1] == DNGN_ROCK_WALL)
            grd[j][k + 1] = DNGN_FLOOR;
        if (grd[j - 1][k] == DNGN_ROCK_WALL)
            grd[j - 1][k] = DNGN_FLOOR;
        if (grd[j + 1][k] == DNGN_ROCK_WALL)
            grd[j + 1][k] = DNGN_FLOOR;

        if (boxy)
        {
            if (grd[j - 1][k - 1] == DNGN_ROCK_WALL)
                grd[j - 1][k - 1] = DNGN_FLOOR;
            if (grd[j + 1][k + 1] == DNGN_ROCK_WALL)
                grd[j + 1][k + 1] = DNGN_FLOOR;
            if (grd[j - 1][k + 1] == DNGN_ROCK_WALL)
                grd[j - 1][k + 1] = DNGN_FLOOR;
            if (grd[j + 1][k - 1] == DNGN_ROCK_WALL)
                grd[j + 1][k - 1] = DNGN_FLOOR;
        }
    }
}                               // end spotty_level()

void smear_feature(int iterations, bool boxy, dungeon_feature_type feature,
                   int x1, int y1, int x2, int y2)
{
    for (int i = 0; i < iterations; ++i)
    {
        int x, y;
        bool diagonals, straights;
        do
        {
            x = random_range(x1 + 1, x2 - 1);
            y = random_range(y1 + 1, y2 - 1);

            diagonals = grd[x + 1][y + 1] == feature
                        || grd[x - 1][y + 1] == feature
                        || grd[x - 1][y - 1] == feature
                        || grd[x + 1][y - 1] == feature;

            straights = grd[x + 1][y] == feature
                        || grd[x - 1][y] == feature
                        || grd[x][y + 1] == feature
                        || grd[x][y - 1] == feature;
        }
        while (grd[x][y] == feature || !straights && (boxy || !diagonals));

        grd[x][y] = feature;
    }
}

static void _bigger_room()
{
    dgn_Build_Method += " bigger_room";
    dgn_Layout_Type   = "open";

    unsigned char i, j;

    for (i = 10; i < (GXM - 10); i++)
        for (j = 10; j < (GYM - 10); j++)
        {
            if (grd[i][j] == DNGN_ROCK_WALL)
                grd[i][j] = DNGN_FLOOR;
        }

    dungeon_feature_type pool_type = DNGN_DEEP_WATER;

    if (one_chance_in(15))
        pool_type = DNGN_TREE;

    _many_pools(pool_type);

    if (one_chance_in(3))
    {
        if (coinflip())
            _build_river( DNGN_DEEP_WATER );
        else
            _build_lake( DNGN_DEEP_WATER );
    }

    int pair_count = coinflip() ? 4 : 3;

    for (j = 0; j < pair_count; j++)
        for (i = 0; i < 2; i++)
        {
            _place_specific_stair( static_cast<dungeon_feature_type>(
                                    j + ((i == 0) ? DNGN_STONE_STAIRS_DOWN_I
                                                  : DNGN_STONE_STAIRS_UP_I)) );
        }
}

// Various plan_xxx functions.
static void _plan_main(int level_number, int force_plan)
{
    dgn_Build_Method += make_stringf(" plan_main [%d%s]", level_number,
                                     force_plan ? " force_plan" : "");
    dgn_Layout_Type   = "rooms";

    bool do_stairs = false;
    dungeon_feature_type special_grid = (one_chance_in(3) ? DNGN_METAL_WALL
                                                          : DNGN_STONE_WALL);
    int i,j;

    if (!force_plan)
        force_plan = 1 + random2(12);

    do_stairs = ((force_plan == 1) ? _plan_1(level_number) :
                 (force_plan == 2) ? _plan_2(level_number) :
                 (force_plan == 3) ? _plan_3(level_number) :
                 (force_plan == 4) ? _plan_4(0, 0, 0, 0, NUM_FEATURES) :
                 (force_plan == 5) ? (one_chance_in(9) ? _plan_5()
                                                       : _plan_3(level_number)) :
                 (force_plan == 6) ? _plan_6(level_number)
                                   : _plan_3(level_number));

    if (do_stairs)
    {
        int pair_count = coinflip()?4:3;

        for (j = 0; j < pair_count; j++)
            for (i = 0; i < 2; i++)
            {
                _place_specific_stair( static_cast<dungeon_feature_type>(
                                       j + ((i == 0)? DNGN_STONE_STAIRS_DOWN_I
                                                    : DNGN_STONE_STAIRS_UP_I)));
            }
    }

    if (one_chance_in(20))
        dgn_replace_area(0, 0, GXM-1, GYM-1, DNGN_ROCK_WALL, special_grid);
}

static bool _plan_1(int level_number)
{
    dgn_Build_Method += make_stringf(" plan_1 [%d]", level_number);
    dgn_Layout_Type   = "open";

    const map_def *vault = find_map_by_name("layout_forbidden_donut");
    ASSERT(vault);

    bool success = _build_vaults(level_number, vault);
    dgn_ensure_vault_placed(success, false);

    return false;
}

static bool _plan_2(int level_number)
{
    dgn_Build_Method += " plan_2";
    dgn_Layout_Type   = "cross";

    const map_def *vault = find_map_by_name("layout_cross");
    ASSERT(vault);

    bool success = _build_vaults(level_number, vault);
    dgn_ensure_vault_placed(success, false);

    return false;
}

static bool _plan_3(int level_number)
{
    dgn_Build_Method += " plan_3";
    dgn_Layout_Type   = "rooms";

    const map_def *vault = find_map_by_name("layout_rooms");
    ASSERT(vault);

    bool success = _build_vaults(level_number, vault);
    dgn_ensure_vault_placed(success, false);

    return true;
}

// A more chaotic version of city level.
static bool _plan_4(char forbid_x1, char forbid_y1, char forbid_x2,
                    char forbid_y2, dungeon_feature_type force_wall)
{
    dgn_Build_Method += make_stringf(" plan_4 [%d,%d %d,%d %d]",
                                     (int) forbid_x1, (int) forbid_y1,
                                     (int) forbid_x2, (int) forbid_y2,
                                     (int) force_wall);
    dgn_Layout_Type   = "city";

    int temp_rand;              // req'd for probability checking

    int number_boxes = 5000;
    dungeon_feature_type drawing = DNGN_ROCK_WALL;
    char b1x, b1y, b2x, b2y;
    int i;

    temp_rand = random2(81);

    number_boxes = ((temp_rand > 48) ? 4000 :   // odds: 32 in 81 {dlb}
                    (temp_rand > 24) ? 3000 :   // odds: 24 in 81 {dlb}
                    (temp_rand >  8) ? 5000 :   // odds: 16 in 81 {dlb}
                    (temp_rand >  0) ? 2000     // odds:  8 in 81 {dlb}
                                     : 1000);   // odds:  1 in 81 {dlb}

    if (force_wall != NUM_FEATURES)
        drawing = force_wall;
    else
    {
        temp_rand = random2(18);

        drawing = ((temp_rand > 7) ? DNGN_ROCK_WALL :   // odds: 10 in 18 {dlb}
                   (temp_rand > 2) ? DNGN_STONE_WALL    // odds:  5 in 18 {dlb}
                                   : DNGN_METAL_WALL);  // odds:  3 in 18 {dlb}
    }

    dgn_replace_area(10, 10, (GXM - 10), (GYM - 10), DNGN_ROCK_WALL,
                     DNGN_FLOOR);

    // replace_area can also be used to fill in:
    for (i = 0; i < number_boxes; i++)
    {
        b1x = 11 + random2(45);
        b1y = 11 + random2(35);

        b2x = b1x + 3 + random2(7) + random2(5);
        b2y = b1y + 3 + random2(7) + random2(5);

        if (forbid_x1 != 0 || forbid_x2 != 0)
        {
            if (b1x <= forbid_x2 && b1x >= forbid_x1
                && b1y <= forbid_y2 && b1y >= forbid_y1)
            {
                continue;
            }

            if (b2x <= forbid_x2 && b2x >= forbid_x1
                && b2y <= forbid_y2 && b2y >= forbid_y1)
            {
                continue;
            }
        }

        if (count_antifeature_in_box(b1x-1, b1y-1, b2x+1, b2y+1, DNGN_FLOOR))
            continue;

        if (force_wall == NUM_FEATURES)
        {
            // NB: comparison reversal here - combined
            temp_rand = random2(1200);

            // probabilities *not meant* to sum to one! {dlb}
            if (temp_rand < 417)        // odds: 261 in 1200 {dlb}
                drawing = DNGN_ROCK_WALL;
            else if (temp_rand < 156)   // odds: 116 in 1200 {dlb}
                drawing = DNGN_STONE_WALL;
            else if (temp_rand < 40)    // odds:  40 in 1200 {dlb}
                drawing = DNGN_METAL_WALL;
        }

        temp_rand = random2(210);

        if (temp_rand > 71)     // odds: 138 in 210 {dlb}
            dgn_replace_area(b1x, b1y, b2x, b2y, DNGN_FLOOR, drawing);
        else                    // odds:  72 in 210 {dlb}
            _box_room(b1x, b2x - 1, b1y, b2y - 1, drawing);
    }

    if (forbid_x1 == 0 && one_chance_in(4))     // a market square
    {
        spec_room sr;
        sr.tl.set(25, 25);
        sr.br.set(55, 45);

        int oblique_max = 0;
        if (!one_chance_in(4))
            oblique_max = 5 + random2(20);      // used elsewhere {dlb}

        dungeon_feature_type feature = DNGN_FLOOR;
        if (one_chance_in(10))
            feature = coinflip()? DNGN_DEEP_WATER : DNGN_LAVA;

        octa_room(sr, oblique_max, feature);
    }

    return true;
}

static bool _plan_5()
{
    dgn_Build_Method += " plan_5";
    dgn_Layout_Type   = "misc"; // XXX: What type of layout is this?

    unsigned char imax = 5 + random2(20);       // value range of [5,24] {dlb}

    for (unsigned char i = 0; i < imax; i++)
    {
        join_the_dots(
            coord_def( random2(GXM - 20) + 10, random2(GYM - 20) + 10 ),
            coord_def( random2(GXM - 20) + 10, random2(GYM - 20) + 10 ),
            MMT_VAULT );
    }

    if (!one_chance_in(4))
        spotty_level(true, 100, coinflip());

    return true;
}

// Octagon with pillars in middle.
static bool _plan_6(int level_number)
{
    dgn_Build_Method += make_stringf(" plan_6 [%d]", level_number);
    dgn_Layout_Type   = "open";

    const map_def *vault = find_map_by_name("layout_big_octagon");
    ASSERT(vault);

    bool success = _build_vaults(level_number, vault);
    dgn_ensure_vault_placed(success, false);

    // This "back door" is often one of the easier ways to get out of
    // pandemonium... the easiest is to use the banish spell.
    //
    // Note, that although "level_number > 20" will work for most
    // trips to pandemonium (through regular portals), it won't work
    // for demonspawn who gate themselves there. -- bwr
    if ((player_in_branch(BRANCH_MAIN_DUNGEON) && level_number > 20
            || you.level_type == LEVEL_PANDEMONIUM)
        && coinflip())
    {
        grd[40][36] = DNGN_ENTER_ABYSS;
        grd[41][36] = DNGN_ENTER_ABYSS;
    }

    return (false);
}

bool octa_room(spec_room &sr, int oblique_max,
               dungeon_feature_type type_floor)
{
    dgn_Build_Method += make_stringf(" octa_room [%d %d]", oblique_max,
                                     (int) type_floor);

    int x,y;

    // Hack - avoid lava in the crypt {gdl}
    if ((player_in_branch( BRANCH_CRYPT ) || player_in_branch( BRANCH_TOMB ))
         && type_floor == DNGN_LAVA)
    {
        type_floor = DNGN_SHALLOW_WATER;
    }

    int oblique = oblique_max;

    // Check octagonal room for special; avoid if exists.
    for (x = sr.tl.x; x < sr.br.x; x++)
    {
        for (y = sr.tl.y + oblique; y < sr.br.y - oblique; y++)
            if (grd[x][y] == DNGN_BUILDER_SPECIAL_WALL)
                return (false);

        if (x > sr.tl.x - oblique_max)
            oblique += 2;

        if (oblique > 0)
            oblique--;
    }

    oblique = oblique_max;


    for (x = sr.tl.x; x < sr.br.x; x++)
    {
        for (y = sr.tl.y + oblique; y < sr.br.y - oblique; y++)
        {
            if (grd[x][y] == DNGN_ROCK_WALL)
                grd[x][y] = type_floor;

            if (grd[x][y] == DNGN_FLOOR && type_floor == DNGN_SHALLOW_WATER)
                grd[x][y] = DNGN_SHALLOW_WATER;

            if (grd[x][y] == DNGN_CLOSED_DOOR && !feat_is_solid(type_floor))
                grd[x][y] = DNGN_FLOOR;       // ick
        }

        if (x > sr.br.x - oblique_max)
            oblique += 2;

        if (oblique > 0)
            oblique--;
    }

    return (true);
}

static void _find_maze_neighbours(const coord_def &c,
                                  const dgn_region &region,
                                  coord_list &ns)
{
    std::vector<coord_def> coords;

    for (int yi = -2; yi <= 2; yi += 2)
        for (int xi = -2; xi <= 2; xi += 2)
        {
            if (!!xi == !!yi)
                continue;

            const coord_def cp(c.x + xi, c.y + yi);
            if (region.contains(cp))
                coords.push_back(cp);
        }

    while (!coords.empty())
    {
        const int n = random2(coords.size());
        ns.push_back(coords[n]);
        coords.erase( coords.begin() + n );
    }
}

static void _labyrinth_maze_recurse(const coord_def &c, const dgn_region &where)
{
    coord_list neighbours;
    _find_maze_neighbours(c, where, neighbours);

    coord_list deferred;
    for (coord_list::iterator i = neighbours.begin();
         i != neighbours.end(); ++i)
    {
        const coord_def &nc = *i;
        if (grd(nc) != DNGN_ROCK_WALL)
            continue;

        grd(nc) = DNGN_FLOOR;
        grd(c + (nc - c) / 2) = DNGN_FLOOR;

        if (!one_chance_in(5))
            _labyrinth_maze_recurse(nc, where);
        else
            deferred.push_back(nc);
    }

    for (coord_list::iterator i = deferred.begin(); i != deferred.end(); ++i)
        _labyrinth_maze_recurse(*i, where);
}

static void _labyrinth_build_maze(coord_def &e, const dgn_region &lab)
{
    _labyrinth_maze_recurse(lab.random_point(), lab);

    do
        e = lab.random_point();
    while (grd(e) != DNGN_FLOOR);
}

static void _labyrinth_place_items(const coord_def &end)
{
    int num_items = 8 + random2avg(12, 2);
    for (int i = 0; i < num_items; i++)
    {
        const object_class_type glopop =
            static_cast<object_class_type>(
                random_choose_weighted(14, OBJ_WEAPONS,
                                       14, OBJ_ARMOUR,
                                       3, OBJ_MISSILES,
                                       3, OBJ_MISCELLANY,
                                       10, OBJ_WANDS,
                                       10, OBJ_SCROLLS,
                                       10, OBJ_JEWELLERY,
                                       8, OBJ_BOOKS,
                                       8, OBJ_STAVES,
                                       0));

        const int treasure_item =
            items( 1, glopop, OBJ_RANDOM, true,
                   one_chance_in(3)? you.absdepth0 * 3 : MAKE_GOOD_ITEM,
                   MAKE_ITEM_RANDOM_RACE );

        if (treasure_item != NON_ITEM)
            mitm[treasure_item].pos = end;
    }
}

static void _labyrinth_place_exit(const coord_def &end)
{
    _labyrinth_place_items(end);
    mons_place(mgen_data::sleeper_at(MONS_MINOTAUR, end, MG_PATROLLING));
    grd(end) = DNGN_ESCAPE_HATCH_UP;
}

// Checks whether a given grid has at least one neighbour surrounded
// entirely by non-floor.
static bool _has_no_floor_neighbours(const coord_def &pos, bool recurse = false)
{
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
        {
            if (x == 0 && y == 0)
                continue;

            const coord_def p = pos + coord_def(x, y);
            if (!in_bounds(p))
                return (true);

            if (recurse)
            {
                if (grd(p) == DNGN_FLOOR)
                    return (false);
            }
            else if (_has_no_floor_neighbours(p, true))
                return (true);
        }

    return (recurse);
}

// Change the borders of the labyrinth to another (undiggable) wall type.
static void _change_labyrinth_border(const dgn_region &region,
                                     const dungeon_feature_type wall)
{
    const coord_def &end = region.pos + region.size;
    for (int y = region.pos.y-1; y <= end.y; ++y)
        for (int x = region.pos.x-1; x <= end.x; ++x)
        {
            const coord_def c(x, y);
            if (!in_bounds(c)) // paranoia
                continue;

            if (grd(c) == wall || !feat_is_wall(grd(c)))
                continue;

            // All border grids have neighbours without any access to floor.
            if (_has_no_floor_neighbours(c))
                grd[x][y] = wall;
        }
}

static void _change_walls_from_centre(const dgn_region &region,
                                      const coord_def &centre,
                                      bool  rectangular,
                                      unsigned mmask,
                                      dungeon_feature_type wall,
                                      const std::vector<dist_feat> &ldist)
{
    if (ldist.empty())
        return;

    const coord_def &end = region.pos + region.size;
    for (int y = region.pos.y; y < end.y; ++y)
        for (int x = region.pos.x; x < end.x; ++x)
        {
            const coord_def c(x, y);
            if (grd(c) != wall || !unforbidden(c, mmask))
                continue;

            const int distance =
                rectangular? (c - centre).rdist() : (c - centre).abs();

            for (int i = 0, size = ldist.size(); i < size; ++i)
            {
                if (distance <= ldist[i].dist)
                {
                    grd(c) = ldist[i].feat;
                    break;
                }
            }
        }
}

// Called as:
// change_walls_from_centre( region_affected, centre, rectangular, wall,
//                           dist1, feat1, dist2, feat2, ..., 0 )
// What it does:
// Examines each square in region_affected, calculates its distance from
// "centre" (the centre need not be in region_affected). If the distance is
// less than or equal to dist1, and the feature == wall, then it is replaced
// by feat1. Otherwise, if the distance <= dist2 and feature == wall, it is
// replaced by feat2, and so on. A distance of 0 indicates the end of the
// list of distances.
//
static void _change_walls_from_centre(const dgn_region &region,
                                      const coord_def &c,
                                      bool  rectangular,
                                      dungeon_feature_type wall,
                                      ...)
{
    std::vector<dist_feat> ldist;

    va_list args;
    va_start(args, wall);

    while (true)
    {
        const int dist = va_arg(args, int);
        if (!dist)
            break;

        const dungeon_feature_type feat =
            static_cast<dungeon_feature_type>( va_arg(args, int) );

        ldist.push_back(dist_feat(dist, feat));
    }

    _change_walls_from_centre(region, c, rectangular, MMT_VAULT, wall, ldist);
}

static void _place_extra_lab_minivaults(int level_number)
{
    std::set<const map_def*> vaults_used;
    while (true)
    {
        const map_def *vault = random_map_for_tag("lab", false);
        if (!vault || vaults_used.find(vault) != vaults_used.end())
            break;

        vaults_used.insert(vault);
        if (!_build_secondary_vault(level_number, vault))
            break;
    }
}

// Checks if there is a square with the given mask within radius of pos.
static bool _has_vault_in_radius(const coord_def &pos, int radius,
                                 unsigned mask)
{
    for (int yi = -radius; yi <= radius; ++yi)
        for (int xi = -radius; xi <= radius; ++xi)
        {
            const coord_def p = pos + coord_def(xi, yi);
            if (!in_bounds(p))
                continue;
            if (!unforbidden(p, mask))
                return (true);
        }

    return (false);
}

// Find an entry point that's:
// * At least 28 squares away from the exit.
// * At least 6 squares away from the nearest vault.
// * Floor (well, obviously).
static coord_def _labyrinth_find_entry_point(const dgn_region &reg,
                                             const coord_def &end)
{
    const int min_distance = 28 * 28;
    // Try many times.
    for (int i = 0; i < 2000; ++i)
    {
        const coord_def place = reg.random_point();
        if (grd(place) != DNGN_FLOOR)
            continue;

        if ((place - end).abs() < min_distance)
            continue;

        if (_has_vault_in_radius(place, 6, MMT_VAULT))
            continue;

        return (place);
    }
    coord_def unfound;
    return (unfound);
}

static void _labyrinth_place_entry_point(const dgn_region &region,
                                         const coord_def &pos)
{
    const coord_def p = _labyrinth_find_entry_point(region, pos);
    if (in_bounds(p))
        env.markers.add(new map_feature_marker(p, DNGN_ENTER_LABYRINTH));
}

static bool _is_deadend(const coord_def pos)
{
    std::vector<coord_def> dirs;
    dirs.push_back(coord_def(0, -1));
    dirs.push_back(coord_def(1,  0));
    dirs.push_back(coord_def(0,  1));
    dirs.push_back(coord_def(-1, 0));

    int count_neighbours = 0;
    for (unsigned int i = 0; i < dirs.size(); i++)
    {
        coord_def p = pos + dirs[i];
        if (!in_bounds(p))
            continue;

        if (grd(p) == DNGN_FLOOR)
            count_neighbours++;
    }

    return (count_neighbours <= 1);
}

static coord_def _find_random_deadend(const dgn_region &region)
{
    int tries = 0;
    coord_def result;
    bool floor_pos = false;
    while (++tries < 50)
    {
        coord_def pos = region.random_point();
        if (grd(pos) != DNGN_FLOOR)
            continue;
        else if (!floor_pos)
        {
            result = pos;
            floor_pos = true;
        }

        if (!_is_deadend(pos))
            continue;

        return (pos);
    }

    return (result);
}

// Adds a bloody trail ending in a dead end with spattered walls.
static void _labyrinth_add_blood_trail(const dgn_region &region)
{
    if (one_chance_in(5))
        return;

    int count_trails = 1 + one_chance_in(5) + one_chance_in(7);

    int tries = 0;
    while (++tries < 20)
    {
        const coord_def start = _find_random_deadend(region);
        const coord_def dest  = region.random_point();
        monster_pathfind mp;
        if (!mp.init_pathfind(dest, start))
            continue;

        const std::vector<coord_def> path = mp.backtrack();

        if (path.size() < 10)
            continue;

        maybe_bloodify_square(start);
#ifdef WIZARD
        env.pgrid(start) |= FPROP_HIGHLIGHT;
#endif
        bleed_onto_floor(start, MONS_HUMAN, 150, true, false);

        for (unsigned int step = 0; step < path.size(); step++)
        {
            coord_def pos = path[step];

            if (step < 2 || step < 12 && coinflip()
                || step >= 12 && one_chance_in(step/4))
            {
                maybe_bloodify_square(pos);
            }
#ifdef WIZARD
            env.pgrid(pos) |= FPROP_HIGHLIGHT;
#endif

            if (step >= 10 && one_chance_in(7))
                break;
        }

        if (--count_trails > 0)
            continue;

        break;
    }
}

static bool _find_random_nonmetal_wall(const dgn_region &region,
                                       coord_def &pos)
{
    int tries = 0;
    while (++tries < 50)
    {
        pos = region.random_point();
        if (!in_bounds(pos))
            continue;

        if (grd(pos) == DNGN_ROCK_WALL || grd(pos) == DNGN_STONE_WALL)
            return (true);
    }
    return (false);
}

static bool _grid_has_wall_neighbours(const coord_def pos, const coord_def dir)
{
    std::vector<coord_def> dirs;
    dirs.push_back(coord_def(0, -1));
    dirs.push_back(coord_def(1,  0));
    dirs.push_back(coord_def(0,  1));
    dirs.push_back(coord_def(-1, 0));

    for (unsigned int i = 0; i < dirs.size(); i++)
    {
        coord_def p = pos + dirs[i];
        if (!in_bounds(p))
            continue;

        if (dirs[i] == dir)
            continue;

        if (grd(p) == DNGN_ROCK_WALL || grd(p) == DNGN_STONE_WALL)
            return (true);
    }
    return (false);
}

static void _vitrify_wall_neighbours(const coord_def pos)
{
    // This hinges on clear wall types having the same order as non-clear ones!
    const int clear_plus = DNGN_CLEAR_ROCK_WALL - DNGN_ROCK_WALL;

    std::vector<coord_def> dirs;
    dirs.push_back(coord_def(0, -1));
    dirs.push_back(coord_def(1,  0));
    dirs.push_back(coord_def(0,  1));
    dirs.push_back(coord_def(-1, 0));

    for (unsigned int i = 0; i < dirs.size(); i++)
    {
        coord_def p = pos + dirs[i];
        if (!in_bounds(p))
            continue;

        // Don't vitrify vault grids
        if (testbits(env.pgrid(p), FPROP_VAULT))
            continue;

        if (grd(p) == DNGN_ROCK_WALL || grd(p) == DNGN_STONE_WALL)
        {
            grd(p) = static_cast<dungeon_feature_type>(grd(p) + clear_plus);
#ifdef WIZARD
            env.pgrid(p) |= FPROP_HIGHLIGHT;
#endif
            if (one_chance_in(3) || _grid_has_wall_neighbours(p, dirs[i]))
                _vitrify_wall_neighbours(p);
        }
    }
}

// Turns some connected rock or stone walls into the transparent versions.
static void _labyrinth_add_glass_walls(const dgn_region &region)
{
    int glass_num = random2(3) + random2(4);
    if (!glass_num)
        return;

    // This hinges on clear wall types having the same order as non-clear ones!
    const int clear_plus = DNGN_CLEAR_ROCK_WALL - DNGN_ROCK_WALL;

    coord_def pos;
    while (0 < glass_num--)
    {
        if (!_find_random_nonmetal_wall(region, pos))
            break;

        if (_has_vault_in_radius(pos, 6, MMT_VAULT))
            continue;

        grd(pos) = static_cast<dungeon_feature_type>(grd(pos) + clear_plus);
#ifdef WIZARD
        env.pgrid(pos) |= FPROP_HIGHLIGHT;
#endif
        _vitrify_wall_neighbours(pos);
    }
}

static void _labyrinth_level(int level_number)
{
    dgn_Build_Method += make_stringf(" labyrinth [%d]", level_number);
    dgn_Layout_Type   = "labyrinth";

    dgn_region lab = dgn_region::absolute( LABYRINTH_BORDER,
                                           LABYRINTH_BORDER,
                                           GXM - LABYRINTH_BORDER - 1,
                                           GYM - LABYRINTH_BORDER - 1 );

    // First decide if we're going to use a Lab minivault.
    const map_def *vault = random_map_for_tag("minotaur", false);
    vault_placement place;

    coord_def end;
    _labyrinth_build_maze(end, lab);

    if (!vault || !_build_secondary_vault(level_number, vault))
    {
        vault = NULL;
        _labyrinth_place_exit(end);
    }
    else
    {
        const vault_placement &rplace = *(Level_Vaults.end() - 1);
        if (rplace.map.has_tag("generate_loot"))
        {
            for (int y = rplace.pos.y;
                 y <= rplace.pos.y + rplace.size.y - 1; ++y)
            {
                for (int x = rplace.pos.x;
                     x <= rplace.pos.x + rplace.size.x - 1; ++x)
                {
                    if (grd[x][y] == DNGN_ESCAPE_HATCH_UP)
                    {
                        _labyrinth_place_items(coord_def(x, y));
                        break;
                    }
                }
            }
        }
        place.pos  = rplace.pos;
        place.size = rplace.size;
    }

    if (vault)
        end = place.pos + place.size / 2;

    _place_extra_lab_minivaults(level_number);

    _change_walls_from_centre(lab, end, false,
                              DNGN_ROCK_WALL,
                              15 * 15, DNGN_METAL_WALL,
                              34 * 34, DNGN_STONE_WALL,
                              0);

    _change_labyrinth_border(lab, DNGN_PERMAROCK_WALL);

    _labyrinth_add_blood_trail(lab);
    _labyrinth_add_glass_walls(lab);

    _labyrinth_place_entry_point(lab, end);

    link_items();
}

static bool _is_wall(int x, int y)
{
    return feat_is_wall(grd[x][y]);
}

static int _box_room_door_spot(int x, int y)
{
    // If there is a door near us embedded in rock, we have to be a door too.
    if (   grd[x-1][y] == DNGN_CLOSED_DOOR
            && _is_wall(x-1,y-1) && _is_wall(x-1,y+1)
        || grd[x+1][y] == DNGN_CLOSED_DOOR
            && _is_wall(x+1,y-1) && _is_wall(x+1,y+1)
        || grd[x][y-1] == DNGN_CLOSED_DOOR
            && _is_wall(x-1,y-1) && _is_wall(x+1,y-1)
        || grd[x][y+1] == DNGN_CLOSED_DOOR
            && _is_wall(x-1,y+1) && _is_wall(x+1,y+1))
    {
        grd[x][y] = DNGN_CLOSED_DOOR;
        return 2;
    }

    // To be a good spot for a door, we need non-wall on two sides and
    // wall on two sides.
    bool nor = _is_wall(x, y-1);
    bool sou = _is_wall(x, y+1);
    bool eas = _is_wall(x-1, y);
    bool wes = _is_wall(x+1, y);

    if (nor == sou && eas == wes && nor != eas)
        return 1;

    return 0;
}

static int _box_room_doors( int bx1, int bx2, int by1, int by2, int new_doors)
{
    int good_doors[200];        // 1 == good spot, 2 == door placed!
    int spot;
    int i, j;
    int doors_placed = new_doors;

    // sanity
    if (2 * (bx2-bx1 + by2-by1) > 200)
        return 0;

    // Go through, building list of good door spots, and replacing wall
    // with door if we're about to block off another door.
    int spot_count = 0;

    // top & bottom
    for (i = bx1 + 1; i < bx2; i++)
    {
        good_doors[spot_count ++] = _box_room_door_spot(i, by1);
        good_doors[spot_count ++] = _box_room_door_spot(i, by2);
    }
    // left & right
    for (i = by1+1; i < by2; i++)
    {
        good_doors[spot_count ++] = _box_room_door_spot(bx1, i);
        good_doors[spot_count ++] = _box_room_door_spot(bx2, i);
    }

    if (new_doors == 0)
    {
        // Count # of doors we HAD to place.
        for (i = 0; i < spot_count; i++)
            if (good_doors[i] == 2)
                doors_placed++;

        return doors_placed;
    }

    // Avoid an infinite loop if there are not enough good spots. --KON
    j = 0;
    for (i = 0; i < spot_count; i++)
        if (good_doors[i] == 1)
            j++;

    if (new_doors > j)
        new_doors = j;

    while (new_doors > 0 && spot_count > 0)
    {
        spot = random2(spot_count);
        if (good_doors[spot] != 1)
            continue;

        j = 0;
        for (i = bx1 + 1; i < bx2; i++)
        {
            if (spot == j++)
            {
                grd[i][by1] = DNGN_CLOSED_DOOR;
                break;
            }
            if (spot == j++)
            {
                grd[i][by2] = DNGN_CLOSED_DOOR;
                break;
            }
        }

        for (i = by1 + 1; i < by2; i++)
        {
            if (spot == j++)
            {
                grd[bx1][i] = DNGN_CLOSED_DOOR;
                break;
            }
            if (spot == j++)
            {
                grd[bx2][i] = DNGN_CLOSED_DOOR;
                break;
            }
        }

        // Try not to put a door in the same place twice.
        good_doors[spot] = 2;
        new_doors --;
    }

    return (doors_placed);
}


static void _box_room(int bx1, int bx2, int by1, int by2,
                      dungeon_feature_type wall_type)
{
    // Hack -- avoid lava in the crypt. {gdl}
    if ((player_in_branch( BRANCH_CRYPT ) || player_in_branch( BRANCH_TOMB ))
         && wall_type == DNGN_LAVA)
    {
        wall_type = DNGN_SHALLOW_WATER;
    }

    int temp_rand, new_doors, doors_placed;

    // Do top & bottom walls.
    dgn_replace_area(bx1, by1, bx2, by1, DNGN_FLOOR, wall_type);
    dgn_replace_area(bx1, by2, bx2, by2, DNGN_FLOOR, wall_type);

    // Do left & right walls.
    dgn_replace_area(bx1, by1+1, bx1, by2-1, DNGN_FLOOR, wall_type);
    dgn_replace_area(bx2, by1+1, bx2, by2-1, DNGN_FLOOR, wall_type);

    // Sometimes we have to place doors, or else we shut in other
    // buildings' doors.
    doors_placed = _box_room_doors(bx1, bx2, by1, by2, 0);

    temp_rand = random2(100);
    new_doors = (temp_rand > 45) ? 2 :
                (temp_rand > 22) ? 1
                                 : 3;

    // Small rooms don't have as many doors.
    if ((bx2-bx1)*(by2-by1) < 36 && new_doors > 1)
        new_doors--;

    new_doors -= doors_placed;
    if (new_doors > 0)
        _box_room_doors(bx1, bx2, by1, by2, new_doors);
}

static void _city_level(int level_number)
{
    dgn_Build_Method += make_stringf(" city_level [%d]", level_number);
    dgn_Layout_Type   = "city";

    const map_def *vault = find_map_by_name("layout_city");
    ASSERT(vault);

    bool success = _build_vaults(level_number, vault);
    dgn_ensure_vault_placed(success, false);
}

static bool _treasure_area(int level_number, unsigned char ta1_x,
                           unsigned char ta2_x, unsigned char ta1_y,
                           unsigned char ta2_y)
{
    ta2_x++;
    ta2_y++;

    if (ta2_x <= ta1_x || ta2_y <= ta1_y)
        return (false);

    if ((ta2_x - ta1_x) * (ta2_y - ta1_y) >= 40)
        return (false);

    // Gah. FIXME.
    coord_def tl(ta1_x, ta1_y);
    coord_def br(ta2_x-1, ta2_y-1);

    for (rectangle_iterator ri(tl, br); ri; ++ri)
    {
        if (grd(*ri) != DNGN_FLOOR || coinflip())
            continue;

        int item_made = items( 1, OBJ_RANDOM, OBJ_RANDOM, true,
                               random2( level_number * 2 ),
                               MAKE_ITEM_RANDOM_RACE );

        if (item_made != NON_ITEM)
            mitm[item_made].pos = *ri;
    }

    return (true);
}

static void _diamond_rooms(int level_number)
{
    char numb_diam = 1 + random2(10);
    dungeon_feature_type type_floor = DNGN_DEEP_WATER;
    int runthru = 0;
    int i, oblique_max;

    // I guess no diamond rooms in either of these places. {dlb}
    if (player_in_branch( BRANCH_DIS ) || player_in_branch( BRANCH_TARTARUS ))
        return;

    if (level_number > 5 + random2(5) && coinflip())
        type_floor = DNGN_SHALLOW_WATER;

    if (level_number > 10 + random2(5) && coinflip())
        type_floor = DNGN_DEEP_WATER;

    if (level_number > 17 && coinflip())
        type_floor = DNGN_LAVA;

    if (level_number > 10 && one_chance_in(15))
        type_floor = (coinflip() ? DNGN_STONE_WALL : DNGN_ROCK_WALL);

    if (level_number > 12 && one_chance_in(20))
        type_floor = DNGN_METAL_WALL;

    if (player_in_branch(BRANCH_GEHENNA))
        type_floor = DNGN_LAVA;
    else if (player_in_branch(BRANCH_COCYTUS))
        type_floor = DNGN_DEEP_WATER;

    for (i = 0; i < numb_diam; i++)
    {
        spec_room sr;

        sr.tl.set(8 + random2(43), 8 + random2(35));
        sr.br.set(5 + random2(15), 5 + random2(10));
        sr.br += sr.tl;

        oblique_max = (sr.br.x - sr.tl.x) / 2;

        if (!octa_room(sr, oblique_max, type_floor))
        {
            runthru++;
            if (runthru > 9)
                runthru = 0;
            else
            {
                i--;
                continue;
            }
        }
    }
}

static dungeon_feature_type _random_wall()
{
    const dungeon_feature_type min_rand = DNGN_METAL_WALL;
    const dungeon_feature_type max_rand = DNGN_STONE_WALL;
    dungeon_feature_type wall;
    do
    {
        wall = static_cast<dungeon_feature_type>(
                   random_range(min_rand, max_rand));
    }
    while (wall == DNGN_SLIMY_WALL);

    return (wall);
}

static void _big_room(int level_number)
{
    dungeon_feature_type type_floor = DNGN_FLOOR;
    dungeon_feature_type type_2 = DNGN_FLOOR;
    int i, j, k, l;

    spec_room sr;
    int oblique;

    if (one_chance_in(4))
    {
        oblique = 5 + random2(20);

        sr.tl.set(8 + random2(30), 8 + random2(22));
        sr.br.set(20 + random2(10), 20 + random2(8));
        sr.br += sr.tl;

        // Usually floor, except at higher levels.
        if (!one_chance_in(5) || level_number < 8 + random2(8))
        {
            octa_room(sr, oblique, DNGN_FLOOR);
            return;
        }

        // Default is lava.
        type_floor = DNGN_LAVA;

        if (level_number > 7)
        {
            type_floor = (x_chance_in_y(14, level_number) ? DNGN_DEEP_WATER
                                                          : DNGN_LAVA);
        }

        octa_room(sr, oblique, type_floor);
    }

    // What now?
    sr.tl.set(8 + random2(30), 8 + random2(22));
    sr.br.set(20 + random2(10), 20 + random2(8));
    sr.br += sr.tl;

    // Check for previous special.
    if (count_feature_in_box(sr.tl, sr.br, DNGN_BUILDER_SPECIAL_WALL) > 0)
        return;

    if (level_number > 7 && one_chance_in(4))
    {
        type_floor = (x_chance_in_y(14, level_number) ? DNGN_DEEP_WATER
                                                      : DNGN_LAVA);
    }

    // Make the big room.
    dgn_replace_area(sr.tl, sr.br, DNGN_ROCK_WALL, type_floor);
    dgn_replace_area(sr.tl, sr.br, DNGN_CLOSED_DOOR, type_floor);

    if (type_floor == DNGN_FLOOR)
        type_2 = _random_wall();

    // No lava in the Crypt or Tomb, thanks!
    if (player_in_branch( BRANCH_CRYPT ) || player_in_branch( BRANCH_TOMB ))
    {
        if (type_floor == DNGN_LAVA)
            type_floor = DNGN_SHALLOW_WATER;

        if (type_2 == DNGN_LAVA)
            type_2 = DNGN_SHALLOW_WATER;
    }

    // Sometimes make it a chequerboard.
    if (one_chance_in(4))
        _chequerboard( sr, type_floor, type_floor, type_2 );
    // Sometimes make an inside room w/ stone wall.
    else if (one_chance_in(6))
    {
        i = sr.tl.x;
        j = sr.tl.y;
        k = sr.br.x;
        l = sr.br.y;

        do
        {
            i += 2 + random2(3);
            j += 2 + random2(3);
            k -= 2 + random2(3);
            l -= 2 + random2(3);
            // check for too small
            if (i >= k - 3)
                break;
            if (j >= l - 3)
                break;

            _box_room(i, k, j, l, DNGN_STONE_WALL);

        }
        while (level_number < 1500);       // ie forever
    }
}                               // end big_room()

// Helper function for chequerboard rooms.
// Note that box boundaries are INclusive.
static void _chequerboard( spec_room &sr, dungeon_feature_type target,
                           dungeon_feature_type floor1,
                           dungeon_feature_type floor2 )
{
    if (sr.br.x < sr.tl.x || sr.br.y < sr.tl.y)
        return;

    for (rectangle_iterator ri(sr.tl, sr.br); ri; ++ri)
        if (grd(*ri) == target)
            grd(*ri) = ((ri->x + ri->y) % 2) ? floor2 : floor1;
}

static void _roguey_level(int level_number, spec_room &sr, bool make_stairs)
{
    dgn_Build_Method += make_stringf(" roguey_level [%d%s]", level_number,
                                     make_stairs ? " make_stairs" : "");

    int bcount_x, bcount_y;
    int cn = 0;
    int i;

    FixedVector < unsigned char, 30 > rox1;
    FixedVector < unsigned char, 30 > rox2;
    FixedVector < unsigned char, 30 > roy1;
    FixedVector < unsigned char, 30 > roy2;

    for (bcount_y = 0; bcount_y < 5; bcount_y++)
        for (bcount_x = 0; bcount_x < 5; bcount_x++)
        {
            // rooms:
            rox1[cn] = bcount_x * 13 + 8 + random2(4);
            roy1[cn] = bcount_y * 11 + 8 + random2(4);

            rox2[cn] = rox1[cn] + 3 + random2(8);
            roy2[cn] = roy1[cn] + 3 + random2(6);

            // bounds
            if (rox2[cn] > GXM-8)
                rox2[cn] = GXM-8;

            cn++;
        }

    cn = 0;

    for (i = 0; i < 25; i++)
    {
        dgn_replace_area( rox1[i], roy1[i], rox2[i], roy2[i],
                          DNGN_ROCK_WALL, DNGN_FLOOR );

        // Inner room?
        if (rox2[i] - rox1[i] > 5 && roy2[i] - roy1[i] > 5
            && x_chance_in_y(3, 100 - level_number))
        {
            if (!one_chance_in(4))
            {
                _box_room( rox1[i] + 2, rox2[i] - 2, roy1[i] + 2,
                           roy2[i] - 2, (coinflip() ? DNGN_STONE_WALL
                                                    : DNGN_ROCK_WALL) );
            }
            else
            {
                _box_room( rox1[i] + 2, rox2[i] - 2, roy1[i] + 2,
                           roy2[i] - 2, DNGN_METAL_WALL );
            }

            if (coinflip())
            {
                _treasure_area( level_number, rox1[i] + 3, rox2[i] - 3,
                                              roy1[i] + 3, roy2[i] - 3 );
            }
        }
    }

    // Now, join them together:

    char last_room = 0;
    int bp;

    for (bp = 0; bp < 2; bp++)
        for (i = 0; i < 25; i++)
        {
            if (bp == 0 && (!(i % 5) || i == 0))
                continue;

            if (bp == 1 && i < 5)
                continue;

            coord_def pos, jpos;

            switch (bp)
            {
            case 0:
                last_room = i - 1;
                pos.x  = rox1[i];      // - 1;
                pos.y  = roy1[i] + random2(roy2[i] - roy1[i]);
                jpos.x = rox2[last_room];      // + 1;
                jpos.y = roy1[last_room]
                            + random2(roy2[last_room] - roy1[last_room]);
                break;

            case 1:
                last_room = i - 5;
                pos.y  = roy1[i];      // - 1;
                pos.x  = rox1[i] + random2(rox2[i] - rox1[i]);
                jpos.y = roy2[last_room];      // + 1;
                jpos.x = rox1[last_room]
                                + random2(rox2[last_room] - rox1[last_room]);
                break;
            }

            while (pos != jpos)
            {
                // make a corridor
                if ( coinflip() )
                {
                    if ( pos.x < jpos.x )
                        pos.x++;
                    else if ( pos.x > jpos.x )
                        pos.x--;
                }
                else
                {
                    if ( pos.y < jpos.y )
                        pos.y++;
                    else if ( pos.y > jpos.y )
                        pos.y--;
                }
                if (grd(pos) == DNGN_ROCK_WALL)
                    grd(pos) = DNGN_FLOOR;
            }

            if (grd(pos) == DNGN_FLOOR)
            {
                if ((grd[pos.x + 1][pos.y] == DNGN_ROCK_WALL
                     && grd[pos.x - 1][pos.y] == DNGN_ROCK_WALL)
                    || (grd[pos.x][pos.y + 1] == DNGN_ROCK_WALL
                        && grd[pos.x][pos.y - 1] == DNGN_ROCK_WALL))
                {
                    grd(pos) = DNGN_GRANITE_STATUE;
                }
            }
        }                       // end "for bp, for i"

    // Is one of them a special room?
    const map_def *sroom = NULL;

    if (
#ifndef DEBUG_SPECIAL_ROOMS
        one_chance_in(10) &&
#endif
        (sroom = random_map_for_tag("special_room", true)) != NULL)
    {
        int spec_room_done = random2(25);

        sr.created = true;
        sr.hooked_up = true;
        sr.tl.set( rox1[spec_room_done], roy1[spec_room_done] );
        sr.br.set( rox2[spec_room_done], roy2[spec_room_done] );
        _special_room( level_number, sr, sroom );

        // Make the room 'special' so it doesn't get overwritten
        // by something else (or put monsters in walls, etc...)

        // top
        dgn_replace_area(sr.tl.x-1, sr.tl.y-1, sr.br.x+1,sr.tl.y-1,
                         DNGN_ROCK_WALL, DNGN_BUILDER_SPECIAL_WALL);
        dgn_replace_area(sr.tl.x-1, sr.tl.y-1, sr.br.x+1,sr.tl.y-1,
                         DNGN_FLOOR, DNGN_BUILDER_SPECIAL_FLOOR);
        dgn_replace_area(sr.tl.x-1, sr.tl.y-1, sr.br.x+1,sr.tl.y-1,
                         DNGN_CLOSED_DOOR, DNGN_BUILDER_SPECIAL_FLOOR);

        // bottom
        dgn_replace_area(sr.tl.x-1, sr.br.y+1, sr.br.x+1,sr.br.y+1,
                         DNGN_ROCK_WALL, DNGN_BUILDER_SPECIAL_WALL);
        dgn_replace_area(sr.tl.x-1, sr.br.y+1, sr.br.x+1,sr.br.y+1,
                         DNGN_FLOOR, DNGN_BUILDER_SPECIAL_FLOOR);
        dgn_replace_area(sr.tl.x-1, sr.br.y+1, sr.br.x+1,sr.br.y+1,
                         DNGN_CLOSED_DOOR, DNGN_BUILDER_SPECIAL_FLOOR);

        // left
        dgn_replace_area(sr.tl.x-1, sr.tl.y-1, sr.tl.x-1, sr.br.y+1,
                         DNGN_ROCK_WALL, DNGN_BUILDER_SPECIAL_WALL);
        dgn_replace_area(sr.tl.x-1, sr.tl.y-1, sr.tl.x-1, sr.br.y+1,
                         DNGN_FLOOR, DNGN_BUILDER_SPECIAL_FLOOR);
        dgn_replace_area(sr.tl.x-1, sr.tl.y-1, sr.tl.x-1, sr.br.y+1,
                         DNGN_CLOSED_DOOR, DNGN_BUILDER_SPECIAL_FLOOR);

        // right
        dgn_replace_area(sr.br.x+1, sr.tl.y-1, sr.br.x+1, sr.br.y+1,
                         DNGN_ROCK_WALL, DNGN_BUILDER_SPECIAL_WALL);
        dgn_replace_area(sr.br.x+1, sr.tl.y-1, sr.br.x+1, sr.br.y+1,
                         DNGN_FLOOR, DNGN_BUILDER_SPECIAL_FLOOR);
        dgn_replace_area(sr.br.x+1, sr.tl.y-1, sr.br.x+1, sr.br.y+1,
                         DNGN_CLOSED_DOOR, DNGN_BUILDER_SPECIAL_FLOOR);
    }

    if (!make_stairs)
        return;

    int stair_count = coinflip() ? 4 : 3;

    for (int j = 0; j < stair_count; j++)
        for (i = 0; i < 2; i++)
        {
            _place_specific_stair( static_cast<dungeon_feature_type>(
                                    j + ((i == 0) ? DNGN_STONE_STAIRS_DOWN_I
                                                  : DNGN_STONE_STAIRS_UP_I)) );
        }
}                               // end roguey_level()

bool place_specific_trap(const coord_def& where, trap_type spec_type)
{
    if (spec_type == TRAP_RANDOM || spec_type == TRAP_NONTELEPORT
        || spec_type == TRAP_SHAFT && !is_valid_shaft_level())
    {
        trap_type forbidden1 = NUM_TRAPS;
        trap_type forbidden2 = NUM_TRAPS;

        if (spec_type == TRAP_NONTELEPORT)
        {
            forbidden1 = TRAP_SHAFT;
            forbidden2 = TRAP_TELEPORT;
        }
        else if (!is_valid_shaft_level())
            forbidden1 = TRAP_SHAFT;

        do
            spec_type = static_cast<trap_type>( random2(NUM_TRAPS) );
        while (spec_type == forbidden1 || spec_type == forbidden2);
    }

    for (int tcount = 0; tcount < MAX_TRAPS; tcount++)
        if (env.trap[tcount].type == TRAP_UNASSIGNED)
        {
            env.trap[tcount].type = spec_type;
            env.trap[tcount].pos  = where;
            grd(where)            = DNGN_UNDISCOVERED_TRAP;
            env.trap[tcount].prepare_ammo();
            return (true);
        }

    return (false);
}

static void _build_river( dungeon_feature_type river_type ) //mv
{
    int i,j;
    int y, width;

    if (player_in_branch( BRANCH_CRYPT ) || player_in_branch( BRANCH_TOMB ))
        return;

    std::string extra = make_stringf("river [%d]", (int) river_type);
    env.properties[LEVEL_EXTRAS_KEY].get_vector().push_back(extra);

    // if (one_chance_in(10))
    //     _build_river(river_type);

    // Made rivers less wide... min width five rivers were too annoying. -- bwr
    width = 3 + random2(4);
    y = 10 - width + random2avg( GYM-10, 3 );

    for (i = 5; i < (GXM - 5); i++)
    {
        if (one_chance_in(3))   y++;
        if (one_chance_in(3))   y--;
        if (coinflip())         width++;
        if (coinflip())         width--;

        if (width < 2) width = 2;
        if (width > 6) width = 6;

        for (j = y; j < y+width ; j++)
            if (j >= 5 && j <= GYM - 5)
            {
                // Note that vaults might have been created in this area!
                // So we'll avoid the silliness of orcs/royal jelly on
                // lava and deep water grids. -- bwr
                if (!one_chance_in(200) && _may_overwrite_pos(coord_def(i, j)))
                {
                    if (width == 2 && river_type == DNGN_DEEP_WATER
                        && coinflip())
                    {
                        grd[i][j] = DNGN_SHALLOW_WATER;
                    }
                    else
                        grd[i][j] = river_type;

                    // Override existing markers.
                    env.markers.remove_markers_at(coord_def(i, j), MAT_ANY);
                }
            }
    }
}

static void _build_lake(dungeon_feature_type lake_type) //mv
{
    int i, j;
    int x1, y1, x2, y2;

    if (player_in_branch(BRANCH_CRYPT) || player_in_branch(BRANCH_TOMB))
        return;

    std::string extra = make_stringf("lake [%d]", (int) lake_type);
    env.properties[LEVEL_EXTRAS_KEY].get_vector().push_back(extra);

    // if (one_chance_in (10))
    //     _build_lake(lake_type);

    x1 = 5 + random2(GXM - 30);
    y1 = 5 + random2(GYM - 30);
    x2 = x1 + 4 + random2(16);
    y2 = y1 + 8 + random2(12);

    for (j = y1; j < y2; j++)
    {
        if (coinflip())  x1 += random2(3);
        if (coinflip())  x1 -= random2(3);
        if (coinflip())  x2 += random2(3);
        if (coinflip())  x2 -= random2(3);

    //  mv: this does much more worse effects
    //    if (coinflip()) x1 = x1 -2 + random2(5);
    //    if (coinflip()) x2 = x2 -2 + random2(5);

        if (j - y1 < (y2 - y1) / 2)
        {
            x2 += random2(3);
            x1 -= random2(3);
        }
        else
        {
            x2 -= random2(3);
            x1 += random2(3);
        }

        for (i = x1; i < x2 ; i++)
            if (j >= 5 && j <= GYM - 5 && i >= 5 && i <= GXM - 5)
            {
                // Note that vaults might have been created in this area!
                // So we'll avoid the silliness of monsters and items
                // on lava and deep water grids. -- bwr
                if (!one_chance_in(200) && _may_overwrite_pos(coord_def(i, j)))
                {
                    grd[i][j] = lake_type;

                    // Override markers. (No underwater portals, please.)
                    env.markers.remove_markers_at(coord_def(i, j), MAT_ANY);
                }
            }
    }
}

static void _ruin_level(int ruination /* = 10 */, int plant_density /* = 5 */)
{
    std::vector<coord_def> to_replace;

    for (rectangle_iterator ri(1); ri; ++ri)
    {
        /* only try to replace wall and door tiles */
        if (!feat_is_wall(grd(*ri)) && !feat_is_door(grd(*ri)))
        {
            continue;
        }

        /* don't mess with permarock */
        if (grd(*ri) == DNGN_PERMAROCK_WALL) {
            continue;
        }

        int floor_count = 0;
        for (adjacent_iterator ai(*ri); ai; ++ai)
        {
            if (!feat_is_wall(grd(*ai)) && !feat_is_door(grd(*ai)))
            {
                floor_count++;
            }
        }

        /* chance of removing the tile is dependent on the number of adjacent
         * floor tiles */
        if (x_chance_in_y(floor_count, ruination))
        {
            to_replace.push_back(*ri);
        }
    }

    for (std::vector<coord_def>::const_iterator it = to_replace.begin();
         it != to_replace.end();
         ++it)
    {
        /* only remove some doors, to preserve tactical options */
        /* XXX: should this pick a random adjacent floor type, rather than
         * just hardcoding DNGN_FLOOR? */
        if (feat_is_wall(grd(*it)) ||
            (coinflip() && feat_is_door(grd(*it))))
        {
            grd(*it) = DNGN_FLOOR;
        }

        /* but remove doors if we've removed all adjacent walls */
        for (adjacent_iterator wai(*it); wai; ++wai)
        {
            if (feat_is_door(grd(*wai)))
            {
                bool remove = true;
                for (adjacent_iterator dai(*wai); dai; ++dai)
                {
                    if (feat_is_wall(grd(*dai)))
                    {
                        remove = false;
                    }
                }
                if (remove)
                {
                    grd(*wai) = DNGN_FLOOR;
                }
            }
        }

        /* replace some ruined walls with plants/fungi/bushes */
        if (one_chance_in(plant_density))
        {
            mgen_data mg;
            mg.cls = one_chance_in(20) ? MONS_BUSH  :
                     coinflip()        ? MONS_PLANT :
                     MONS_FUNGUS;
            mg.pos = *it;
            mons_place(mgen_data(mg));
        }
    }
}

static void _add_plant_clumps(int frequency /* = 10 */,
                              int clump_density /* = 12 */,
                              int clump_radius /* = 4 */)
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        mgen_data mg;
        if (mgrd(*ri) != NON_MONSTER)
        {
            /* clump plants around things that already exist */
            monster_type type = menv[mgrd(*ri)].type;
            if ((type == MONS_PLANT  ||
                 type == MONS_FUNGUS ||
                 type == MONS_BUSH) && one_chance_in(frequency)) {
                mg.cls = type;
            }
            else {
                continue;
            }
        }
        else {
            continue;
        }

        std::vector<coord_def> to_place;
        to_place.push_back(*ri);
        for (int i = 1; i < clump_radius; ++i)
        {
            for (radius_iterator rad(*ri, i, C_SQUARE); rad; ++rad)
            {
                if (grd(*rad) != DNGN_FLOOR)
                {
                    continue;
                }

                /* make sure the iterator stays valid */
                std::vector<coord_def> more_to_place;
                for (std::vector<coord_def>::const_iterator it = to_place.begin();
                     it != to_place.end();
                     ++it)
                {
                    if (*rad == *it)
                    {
                        continue;
                    }
                    /* only place plants next to previously placed plants */
                    if (abs(rad->x - it->x) <= 1 && abs(rad->y - it->y) <= 1)
                    {
                        if (one_chance_in(clump_density)) {
                            more_to_place.push_back(*rad);
                        }
                    }
                }
                to_place.insert(to_place.end(), more_to_place.begin(), more_to_place.end());
            }
        }

        for (std::vector<coord_def>::const_iterator it = to_place.begin();
             it != to_place.end();
             ++it)
        {
            if (*it == *ri)
            {
                continue;
            }
            mg.pos = *it;
            mons_place(mgen_data(mg));
        }
    }
}

struct nearest_point
{
    coord_def target;
    coord_def nearest;
    int       distance;

    nearest_point(const coord_def &t) : target(t), nearest(), distance(-1)
    {
    }
    void operator () (const coord_def &c)
    {
        if (grd(c) == DNGN_FLOOR)
        {
            const int ndist = (c - target).abs();
            if (distance == -1 || ndist < distance)
            {
                distance = ndist;
                nearest  = c;
            }
        }
    }
};

// Fill travel_point_distance out from all stone stairs on the level.
static coord_def _dgn_find_closest_to_stone_stairs(coord_def base_pos)
{
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));
    init_travel_terrain_check(false);
    nearest_point np(base_pos);
    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
        {
            if (!travel_point_distance[x][y] && feat_is_stone_stair(grd[x][y]))
                _dgn_fill_zone(coord_def(x, y), 1, np, dgn_square_travel_ok);
        }

    return (np.nearest);
}


double dgn_degrees_to_radians(int degrees)
{
    return degrees * M_PI / 180;
}

coord_def dgn_random_point_from(const coord_def &c, int radius, int margin)
{
    int attempts = 70;
    while (attempts-- > 0)
    {
        const double angle = dgn_degrees_to_radians(random2(360));
        const coord_def res =
            c + coord_def(static_cast<int>(radius * cos(angle)),
                          static_cast<int>(radius * sin(angle)));
        if (res.x >= margin && res.x < GXM - margin - 1
            && res.y >= margin && res.y < GYM - margin - 1)
        {
            return res;
        }
    }
    return coord_def();
}

coord_def dgn_random_point_visible_from(const coord_def &c,
                                        int radius,
                                        int margin,
                                        int tries)
{
    while (tries-- > 0)
    {
        const coord_def point = dgn_random_point_from(c, radius, margin);
        if (point.origin())
            continue;
        if (!cell_see_cell(c, point))
            continue;
        return point;
    }
    return coord_def();
}

coord_def dgn_find_feature_marker(dungeon_feature_type feat)
{
    std::vector<map_marker*> markers = env.markers.get_all();
    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        map_marker *mark = markers[i];
        if (mark->get_type() == MAT_FEATURE
            && dynamic_cast<map_feature_marker*>(mark)->feat == feat)
        {
            return (mark->pos);
        }
    }
    coord_def unfound;
    return (unfound);
}

static coord_def _dgn_find_labyrinth_entry_point()
{
    return (dgn_find_feature_marker(DNGN_ENTER_LABYRINTH));
}

coord_def dgn_find_nearby_stair(dungeon_feature_type stair_to_find,
                                coord_def base_pos, bool find_closest)
{
#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS,
         "Level entry point on %sstair: %d (%s)",
         find_closest? "closest " : "",
         stair_to_find,
         dungeon_feature_name(stair_to_find));
#endif

    if (stair_to_find == DNGN_EXIT_PORTAL_VAULT)
    {
        const coord_def pos(dgn_find_feature_marker(stair_to_find));
        if (in_bounds(pos))
        {
            if (map_marker *marker = env.markers.find(pos, MAT_FEATURE))
                env.markers.remove(marker);
            return (pos);
        }

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_WARN, "Ouch, no portal vault exit point!");
#endif

        stair_to_find = DNGN_FLOOR;
    }

    if (stair_to_find == DNGN_ESCAPE_HATCH_UP
        || stair_to_find == DNGN_ESCAPE_HATCH_DOWN)
    {
        const coord_def pos(_dgn_find_closest_to_stone_stairs(base_pos));
        if (in_bounds(pos))
            return (pos);
    }

    if (stair_to_find == DNGN_STONE_ARCH)
    {
        const coord_def pos(dgn_find_feature_marker(stair_to_find));
        if (in_bounds(pos) && grd(pos) == stair_to_find)
            return (pos);
    }

    if (stair_to_find == DNGN_ENTER_LABYRINTH)
    {
        const coord_def pos(_dgn_find_labyrinth_entry_point());
        if (in_bounds(pos))
            return (pos);

        // Couldn't find a good place, warn, and use old behaviour.
        dprf("Oops, couldn't find labyrinth entry marker.");
        stair_to_find = DNGN_FLOOR;
    }

    if (stair_to_find == your_branch().exit_stairs)
    {
        const coord_def pos(dgn_find_feature_marker(DNGN_STONE_STAIRS_UP_I));
        if (in_bounds(pos) && grd(pos) == stair_to_find)
            return (pos);
    }

    // Scan around the player's position first.
    int basex = base_pos.x;
    int basey = base_pos.y;

    // Check for illegal starting point.
    if (!in_bounds(basex, basey))
    {
        basex = 0;
        basey = 0;
    }

    coord_def result;

    int found = 0;
    int best_dist = 1 + GXM*GXM + GYM*GYM;

    // XXX These passes should be rewritten to use an iterator of STL
    // algorithm of some kind.

    // First pass: look for an exact match.
    for (int xcode = 0; xcode < GXM; ++xcode )
    {
        if (stair_to_find == DNGN_FLOOR)
            break;

        const int xsign = ((xcode % 2) ? 1 : -1);
        const int xdiff = xsign * (xcode + 1)/2;
        const int xpos  = (basex + xdiff + GXM) % GXM;

        for (int ycode = 0; ycode < GYM; ++ycode)
        {
            const int ysign = ((ycode % 2) ? 1 : -1);
            const int ydiff = ysign * (ycode + 1)/2;
            const int ypos  = (basey + ydiff + GYM) % GYM;

            // Note that due to the wrapping above, we can't just use
            // xdiff*xdiff + ydiff*ydiff.
            const int dist = (xpos-basex)*(xpos-basex)
                             + (ypos-basey)*(ypos-basey);

            if (grd[xpos][ypos] == stair_to_find)
            {
                found++;
                if (find_closest)
                {
                    if (dist < best_dist)
                    {
                        best_dist = dist;
                        result.x = xpos;
                        result.y = ypos;
                    }
                }
                else if (one_chance_in( found ))
                {
                    result.x = xpos;
                    result.y = ypos;
                }
            }
        }
    }

    if (found)
        return result;

    best_dist = 1 + GXM*GXM + GYM*GYM;

    // Second pass: find a staircase in the proper direction.
    for (int xcode = 0; xcode < GXM; ++xcode )
    {
        if (stair_to_find == DNGN_FLOOR)
            break;

        const int xsign = ((xcode % 2) ? 1 : -1);
        const int xdiff = xsign * (xcode + 1)/2;
        const int xpos  = (basex + xdiff + GXM) % GXM;

        for (int ycode = 0; ycode < GYM; ++ycode)
        {
            const int ysign = ((ycode % 2) ? 1 : -1);
            const int ydiff = ysign * (ycode + 1)/2;
            const int ypos  = (basey + ydiff + GYM) % GYM;

            bool good_stair;
            const int looking_at = grd[xpos][ypos];

            if (stair_to_find <= DNGN_ESCAPE_HATCH_DOWN )
            {
                good_stair = (looking_at >= DNGN_STONE_STAIRS_DOWN_I
                              && looking_at <= DNGN_ESCAPE_HATCH_DOWN);
            }
            else
            {
                good_stair =  (looking_at >= DNGN_STONE_STAIRS_UP_I
                               && looking_at <= DNGN_ESCAPE_HATCH_UP);
            }

            const int dist = (xpos-basex)*(xpos-basex)
                             + (ypos-basey)*(ypos-basey);

            if (good_stair)
            {
                found++;
                if (find_closest && dist < best_dist)
                {
                    best_dist = dist;
                    result.x = xpos;
                    result.y = ypos;
                }
                else if (one_chance_in( found ))
                {
                    result.x = xpos;
                    result.y = ypos;
                }
            }
        }
    }

    if (found)
        return result;

    // Third pass: look for any clear terrain and abandon the idea of
    // looking nearby now. This is used when taking transit Pandemonium gates,
    // or landing in Labyrinths. Never land the PC inside a Pan or Lab vault.
    // We can't check vaults for other levels because vault information is
    // not saved, and the player can re-enter other levels.
    for (int xpos = 0; xpos < GXM; xpos++)
        for (int ypos = 0; ypos < GYM; ypos++)
        {
            if (grd[xpos][ypos] >= DNGN_FLOOR
                && (you.level_type == LEVEL_DUNGEON
                    || unforbidden(coord_def(xpos, ypos), MMT_VAULT)))
            {
                found++;
                if (one_chance_in( found ))
                {
                    result.x = xpos;
                    result.y = ypos;
                }
            }
        }

    if (found)
        return (result);

    // Last attempt: look for marker.
    const coord_def pos(dgn_find_feature_marker(stair_to_find));
    if (in_bounds(pos))
        return (pos);

    // Still hosed? If we're in a portal vault, convert to a search for
    // any stone arch.
    if (you.level_type == LEVEL_PORTAL_VAULT
        && stair_to_find != DNGN_STONE_ARCH)
    {
        return dgn_find_nearby_stair(DNGN_STONE_ARCH, base_pos, find_closest);
    }

    // FAIL
    ASSERT( found );
    return result;
}

void dgn_set_lt_callback(std::string level_type_tag,
                         std::string callback_name)
{
    ASSERT(!level_type_tag.empty());
    ASSERT(!callback_name.empty());

    level_type_post_callbacks[level_type_tag] = callback_name;
}

dungeon_feature_type dgn_tree_base_feature_at(coord_def c)
{
    return (player_in_branch(BRANCH_SWAMP)?
            DNGN_SHALLOW_WATER :
            DNGN_FLOOR);
}

////////////////////////////////////////////////////////////////////
// dgn_region

bool dgn_region::overlaps(const dgn_region &other) const
{
    // The old overlap check checked only two corners - top-left and
    // bottom-right. I'm hoping nothing actually *relied* on that stupid bug.

    return (between(pos.x, other.pos.x, other.pos.x + other.size.x - 1)
               || between(pos.x + size.x - 1, other.pos.x,
                          other.pos.x + other.size.x - 1))
            && (between(pos.y, other.pos.y, other.pos.y + other.size.y - 1)
                || between(pos.y + size.y - 1, other.pos.y,
                           other.pos.y + other.size.y - 1));
}

bool dgn_region::overlaps_any(const dgn_region_list &regions) const
{
    for (dgn_region_list::const_iterator i = regions.begin();
         i != regions.end(); ++i)
    {
        if (overlaps(*i))
            return (true);
    }
    return (false);
}

bool dgn_region::overlaps(const dgn_region_list &regions,
                          const map_mask &mask) const
{
    return overlaps_any(regions) && overlaps(mask);
}

bool dgn_region::overlaps(const map_mask &mask) const
{
    const coord_def endp = pos + size;
    for (int y = pos.y; y < endp.y; ++y)
        for (int x = pos.x; x < endp.x; ++x)
        {
            if (mask[x][y])
                return (true);
        }

    return (false);
}

coord_def dgn_region::random_edge_point() const
{
    return x_chance_in_y(size.x, size.x + size.y) ?
                  coord_def( pos.x + random2(size.x),
                             coinflip()? pos.y : pos.y + size.y - 1 )
                : coord_def( coinflip()? pos.x : pos.x + size.x - 1,
                             pos.y + random2(size.y) );
}

coord_def dgn_region::random_point() const
{
    return coord_def( pos.x + random2(size.x), pos.y + random2(size.y) );
}

struct StairConnectivity
{
    StairConnectivity()
    {
        region[0] = region[1] = region[2] = 0;
        connected[0] = connected[1] = connected[2] = true;
    }

    void connect_region(int idx)
    {
        for (int i = 0; i < 3; i++)
            connected[i] |= (region[i] == idx);
    }

    void read(reader &th)
    {
        region[0] = unmarshallByte(th);
        region[1] = unmarshallByte(th);
        region[2] = unmarshallByte(th);
        connected[0] = unmarshallByte(th);
        connected[1] = unmarshallByte(th);
        connected[2] = unmarshallByte(th);
    }

    void write(writer &th)
    {
        marshallByte(th, region[0]);
        marshallByte(th, region[1]);
        marshallByte(th, region[2]);
        marshallByte(th, connected[0]);
        marshallByte(th, connected[1]);
        marshallByte(th, connected[2]);
    }

    char region[3];
    bool connected[3];
};

FixedVector<std::vector<StairConnectivity>, NUM_BRANCHES> connectivity;

void init_level_connectivity()
{
    for (int i = 0; i < NUM_BRANCHES; i++)
    {
        int depth = branches[i].depth > 0 ? branches[i].depth : 0;
        connectivity[i].resize(depth);
    }
}

void read_level_connectivity(reader &th)
{
    for (int i = 0; i < NUM_BRANCHES; i++)
    {
        unsigned int depth = branches[i].depth > 0 ? branches[i].depth : 0;
        unsigned int num_entries = unmarshallLong(th);
        connectivity[i].resize(std::max(depth, num_entries));

        for (unsigned int e = 0; e < num_entries; e++)
            connectivity[i][e].read(th);
    }
}

void write_level_connectivity(writer &th)
{
    for (int i = 0; i < NUM_BRANCHES; i++)
    {
        marshallLong(th, connectivity[i].size());
        for (unsigned int e = 0; e < connectivity[i].size(); e++)
            connectivity[i][e].write(th);
    }
}

static bool _fixup_interlevel_connectivity()
{
    // Rotate the stairs on this level to attempt to preserve connectivity
    // as much as possible.  At a minimum, it ensures a path from the bottom
    // of a branch to the top of a branch.  If this is not possible, it
    // returns false.
    //
    // Note: this check is undirectional and assumes that levels below this
    // one have not been created yet.  If this is not the case, it will not
    // guarantee or preserve connectivity.

    if (you.level_type != LEVEL_DUNGEON || your_branch().depth == -1)
        return (true);
    if (branches[you.where_are_you].branch_flags & BFLAG_ISLANDED)
        return (true);

    StairConnectivity full;
    StairConnectivity &prev_con = (player_branch_depth() == 1) ? full :
        (connectivity[your_branch().id][player_branch_depth() - 2]);
    StairConnectivity &this_con =
        (connectivity[your_branch().id][player_branch_depth() - 1]);

    FixedVector<coord_def, 3> up_gc;
    FixedVector<coord_def, 3> down_gc;
    FixedVector<int, 3> up_region;
    FixedVector<int, 3> down_region;
    FixedVector<bool, 3> has_down;
    std::vector<bool> region_connected;

    up_region[0] = up_region[1] = up_region[2] = -1;
    down_region[0] = down_region[1] = down_region[2] = -1;
    has_down[0] = has_down[1] = has_down[2] = false;

    // Find up stairs and down stairs on the current level.
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));
    int nzones = 0;
    for (int y = 0; y < GYM ; ++y)
        for (int x = 0; x < GXM; ++x)
        {
            coord_def gc(x,y);
            if (!map_bounds(x, y)
                || travel_point_distance[x][y]
                || !dgn_square_travel_ok(gc))
            {
                continue;
            }

            _dgn_fill_zone(gc, ++nzones, _dgn_point_record_stub,
                           dgn_square_travel_ok, NULL);
        }

    int max_region = 0;
    for (int y = 0; y < GYM ; ++y)
        for (int x = 0; x < GXM; ++x)
        {
            coord_def gc(x,y);
            dungeon_feature_type feat = grd(gc);
            switch (feat)
            {
            case DNGN_STONE_STAIRS_DOWN_I:
            case DNGN_STONE_STAIRS_DOWN_II:
            case DNGN_STONE_STAIRS_DOWN_III:
            {
                int idx = feat - DNGN_STONE_STAIRS_DOWN_I;
                if (down_region[idx] == -1)
                {
                    down_region[idx] = travel_point_distance[x][y];
                    down_gc[idx] = gc;
                    max_region = std::max(down_region[idx], max_region);
                }
                else
                {
                    // Too many stairs!
                    return (false);
                }
                break;
            }
            case DNGN_STONE_STAIRS_UP_I:
            case DNGN_STONE_STAIRS_UP_II:
            case DNGN_STONE_STAIRS_UP_III:
            {
                int idx = feat - DNGN_STONE_STAIRS_UP_I;
                if (up_region[idx] == -1)
                {
                    up_region[idx] = travel_point_distance[x][y];
                    up_gc[idx] = gc;
                    max_region = std::max(up_region[idx], max_region);
                }
                else
                {
                    // Too many stairs!
                    return (false);
                }
                break;
            }
            default:
                break;
            }
        }

    // Ensure all up stairs were found.
    for (int i = 0; i < 3; i++)
        if (up_region[i] == -1)
            return (false);

    region_connected.resize(max_region + 1);
    for (unsigned int i = 0; i < region_connected.size(); i++)
        region_connected[i] = false;

    // Which up stairs have a down stair? (These are potentially connected.)
    if (!at_branch_bottom())
    {
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
            {
                if (down_region[j] == up_region[i])
                    has_down[i] = true;
            }
    }

    bool any_connected = has_down[0] || has_down[1] || has_down[2];
    if (!any_connected && !at_branch_bottom())
        return (false);

    // Keep track of what stairs we've assigned.
    int assign_prev[3] = {-1, -1, -1};
    int assign_cur[3] = {-1, -1, -1};

    // Assign one connected down stair from the previous level to an
    // upstair on the current level with a downstair in the same region.
    // This ensures at least a single valid path to the top.
    bool minimal_connectivity = false;
    for (int i = 0; i < 3 && !minimal_connectivity; i++)
    {
        if (!prev_con.connected[i])
            continue;
        for (int j = 0; j < 3; j++)
        {
            if (!has_down[j] && !at_branch_bottom())
                continue;

            minimal_connectivity = true;
            assign_prev[i] = j;
            assign_cur[j] = i;
            region_connected[up_region[j]] = true;
            break;
        }
    }
    if (!minimal_connectivity)
        return (false);

    // For each disconnected stair (in a unique region) on the previous level,
    // try to reconnect to a connected up stair on the current level.
    for (int i = 0; i < 3; i++)
    {
        if (assign_prev[i] != -1 || prev_con.connected[i])
            continue;

        bool unique_region = true;
        for (int j = 0; j < i; j++)
        {
            if (prev_con.region[j] == prev_con.region[i])
                unique_region = false;
        }
        if (!unique_region)
            continue;

        // Try first to assign to any connected regions.
        for (int j = 0; j < 3; j++)
        {
            if (assign_cur[j] != -1 || !region_connected[up_region[j]])
                continue;

            assign_prev[i] = j;
            assign_cur[j] = i;
            prev_con.connect_region(prev_con.region[i]);
            break;
        }
        if (assign_prev[i] != -1)
            continue;

        // If we fail, then assign to any up stair with a down, and we'll
        // try to reconnect this section on the next level.
        for (int j = 0; j < 3; j++)
        {
            if (assign_cur[j] != -1 || !has_down[j])
                continue;

            assign_prev[i] = j;
            assign_cur[j] = i;
            break;
        }
    }

    // Assign any connected down stairs from the previous level to any
    // disconnected stairs on the current level.
    for (int i = 0; i < 3; i++)
    {
        if (!prev_con.connected[i] || assign_prev[i] != -1)
            continue;

        for (int j = 0; j < 3; j++)
        {
            if (has_down[j] || assign_cur[j] != -1)
                continue;
            if (region_connected[up_region[j]])
                continue;

            assign_prev[i] = j;
            assign_cur[j] = i;
            region_connected[up_region[j]] = true;
            break;
        }
    }

    // If there are any remaining stairs, assign those.
    for (int i = 0; i < 3; i++)
    {
        if (assign_prev[i] != -1)
            continue;
        for (int j = 0; j < 3; j++)
        {
            if (assign_cur[j] != -1)
                continue;
            assign_prev[i] = j;
            assign_cur[j] = i;

            if (region_connected[up_region[j]])
                prev_con.connect_region(prev_con.region[i]);
            else if (prev_con.connected[i])
                region_connected[up_region[j]] = true;
            break;
        }
    }

    // At the branch bottom, all up stairs must be connected.
    if (at_branch_bottom())
    {
        for (int i = 0; i < 3; i++)
            if (!region_connected[up_region[i]])
                return (false);
    }

    // Sanity check that we're not duplicating stairs.
    bool stairs_unique = (assign_cur[0] != assign_cur[1]
                          && assign_cur[1] != assign_cur[2]);
    ASSERT(stairs_unique);
    if (!stairs_unique)
        return (false);

    // Reassign up stair numbers as needed.
    for (int i = 0; i < 3; i++)
    {
        grd(up_gc[i]) =
            (dungeon_feature_type)(DNGN_STONE_STAIRS_UP_I + assign_cur[i]);
    }

    // Fill in connectivity and regions.
    for (int i = 0; i < 3; i++)
    {
        this_con.region[i] = down_region[i];
        if (down_region[i] != -1)
            this_con.connected[i] = region_connected[down_region[i]];
        else
            this_con.connected[i] = false;

    }

    return (true);
}

void run_map_epilogues ()
{
    // Iterate over Level_Vaults and run each map's prelude.
    for (unsigned i = 0, size = Level_Vaults.size(); i < size; ++i)
    {
        map_def map = Level_Vaults[i].map;
        bool result = map.run_lua_epilogue();
        dprf("ran lua epilogue for %s: result was %s.", map.name.c_str(), result ? "true" : false);
    }
}

//////////////////////////////////////////////////////////////////////////
// vault_placement

void vault_placement::reset()
{
    if (_current_temple_hash != NULL)
        _setup_temple_altars(*_current_temple_hash);
    else
        _temple_altar_list.clear();
}

void vault_placement::apply_grid()
{
    if (!size.zero())
    {
        // NOTE: assumes *no* previous item (I think) or monster (definitely)
        // placement.
        for (rectangle_iterator ri(pos, pos + size - 1); ri; ++ri)
        {
            const coord_def &rp(*ri);
            const coord_def dp = rp - pos;

            const int feat = map.map.glyph(dp);

            if (feat == ' ')
                continue;

            const dungeon_feature_type oldgrid = grd(*ri);

            keyed_mapspec *mapsp = map.mapspec_at(dp);
            _vault_grid(*this, feat, *ri, mapsp);

            if (!Generating_Level)
            {
                // Have to link items each square at a time, or
                // dungeon_terrain_changed could blow up.
                link_items();
                const dungeon_feature_type newgrid = grd(*ri);
                grd(*ri) = oldgrid;
                dungeon_terrain_changed(*ri, newgrid, true, true);
                env.markers.remove_markers_at(*ri, MAT_ANY);
            }
            env.pgrid(*ri) |= FPROP_VAULT;
        }

        map.map.apply_overlays(pos);
    }
}

void vault_placement::draw_at(const coord_def &c)
{
    pos = c;
    apply_grid();
}

void remember_vault_placement(std::string key, vault_placement &place)
{
    // First we store some info on the vault into the level's properties
    // hash table, so that if there's a crash the crash report can list
    // them all.
    CrawlHashTable &table = env.properties[key].get_table();

    std::string name = make_stringf("%s [%d]", place.map.name.c_str(),
                                    table.size() + 1);

    std::string place_str
        = make_stringf("(%d,%d) (%d,%d) orient: %d lev: %d subst: %d",
                       place.pos.x, place.pos.y, place.size.x, place.size.y,
                       place.orient, place.level_number, place.rune_subst);

    table[name] = place_str;

    // Second we setup some info to be saved in the player's properties
    // hash table, so the information can be included in the character
    // dump when the player dies/quits/wins.
    if (you.level_type == LEVEL_DUNGEON
        && !place.map.has_tag("layout")
        && !place.map.has_tag_suffix("dummy")
        && !place.map.has_tag("no_dump"))
    {
        const std::string type = place.map.has_tag("extra")
            ? "extra" : "normal";

        _you_vault_list[type].get_vector().push_back(place.map.name);
    }
    else if (you.level_type == LEVEL_PORTAL_VAULT
             && place.map.orient == MAP_ENCOMPASS
             && !place.map.has_tag("no_dump"))
    {
        _portal_vault_map_name = place.map.name;
    }
}

std::string dump_vault_maps()
{
    std::string out = "";

    std::vector<level_id> levels = all_dungeon_ids();

    CrawlHashTable &vaults = you.props[YOU_DUNGEON_VAULTS_KEY].get_table();
    for (unsigned int i = 0; i < levels.size(); i++)
    {
        level_id    &lid = levels[i];
        std::string  lev = lid.describe();

        if (!vaults.exists(lev))
            continue;

        out += lid.describe() + ":\n";

        CrawlHashTable &lists = vaults[lev].get_table();

        const char *types[] = {"normal", "extra"};
        for (int j = 0; j < 2; j++)
        {
            if (!lists.exists(types[j]))
                continue;

            out += "  ";
            out += types[j];
            out += ": ";

            CrawlVector &vec = lists[types[j]].get_vector();

            for (unsigned int k = 0, size = vec.size(); k < size; k++)
            {
                out += vec[k].get_string();
                if (k < (size - 1))
                    out += ", ";
            }

            out += "\n";
        }
        out += "\n";
    }
    CrawlVector &portals = you.props[YOU_PORTAL_VAULT_MAPS_KEY].get_vector();

    if (!portals.empty())
    {
        out += "\n";

        out += "Portal vault maps: ";

        for (unsigned int i = 0, size = portals.size(); i < size; i++)
        {
            out += portals[i].get_string();

            if (i < (size - 1))
                out += ", ";
        }

        out += "\n\n";
    }
    return (out);
}
