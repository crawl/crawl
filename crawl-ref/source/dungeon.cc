/**
 * @file
 * @brief Functions used when building new levels.
**/

#include "AppHdr.h"

#include "dungeon.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <list>
#include <map>
#include <set>
#include <sstream>

#include "abyss.h"
#include "acquire.h"
#include "artefact.h"
#include "branch.h"
#include "chardump.h"
#include "cloud.h"
#include "coordit.h"
#include "corpse.h"
#include "describe.h"
#include "directn.h"
#include "dbg-maps.h"
#include "dbg-scan.h"
#include "dgn-delve.h"
#include "dgn-height.h"
#include "dgn-overview.h"
#include "dgn-shoals.h"
#include "end.h"
#include "english.h"
#include "fight.h"
#include "files.h"
#include "flood-find.h"
#include "ghost.h"
#include "god-passive.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "lev-pand.h"
#include "libutil.h"
#include "mapmark.h"
#include "maps.h"
#include "message.h"
#include "misc.h"
#include "mon-death.h"
#include "mon-gear.h"
#include "mon-pick.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "nearby-danger.h"
#include "notes.h"
#include "place.h"
#include "randbook.h"
#include "random.h"
#include "religion.h"
#include "show.h"
#include "spl-transloc.h"
#include "stairs.h"
#include "state.h"
#include "stringutil.h"
#include "rltiles/tiledef-dngn.h"
#include "tag-version.h"
#include "tile-env.h"
#include "tilepick.h"
#include "tileview.h"
#include "timed-effects.h"
#include "traps.h"
#include "unique-creature-list-type.h"
#ifdef WIZARD
#include "wiz-dgn.h"
#endif

#ifdef DEBUG_DIAGNOSTICS
#define DEBUG_TEMPLES
#endif

#ifdef WIZARD
#include "cio.h" // for cancellable_get_line()
#endif

// DUNGEON BUILDERS
static bool _build_level_vetoable(bool enable_random_maps);
static void _build_dungeon_level();
static bool _valid_dungeon_level();

static bool _builder_by_type();
static bool _builder_normal();
static void _builder_items();
static void _builder_monsters();
static coord_def _place_specific_feature(dungeon_feature_type feat);
static void _place_specific_trap(const coord_def& where, trap_spec* spec,
                                 int charges = 0);
static void _place_branch_entrances(bool use_vaults);
static void _place_extra_vaults();
static void _place_chance_vaults();
static void _place_minivaults();
static int _place_uniques();
static void _place_traps();
static void _prepare_water();
static void _check_doors();

static void _add_plant_clumps(int rarity, int clump_sparseness,
                              int clump_radius);

static void _pick_float_exits(vault_placement &place,
                              vector<coord_def> &targets);
static bool _feat_is_wall_floor_liquid(dungeon_feature_type);
static bool _connect_spotty(const coord_def& from,
                            bool (*overwritable)(dungeon_feature_type) = nullptr);
static bool _connect_vault_exit(const coord_def& exit);

// VAULT FUNCTIONS
static const vault_placement *
_build_secondary_vault(const map_def *vault,
                       bool check_collisions = true,
                       bool make_no_exits = false,
                       const coord_def &where = coord_def(-1, -1));

static const vault_placement *_build_primary_vault(const map_def *vault);

static void _build_postvault_level(vault_placement &place);
static const vault_placement *
_build_vault_impl(const map_def *vault,
                  bool build_only = false,
                  bool check_collisions = false,
                  bool make_no_exits = false,
                  const coord_def &where = coord_def(-1, -1));

static void _vault_grid(vault_placement &,
                        int vgrid,
                        const coord_def& where,
                        keyed_mapspec *mapsp);
static void _vault_grid_mons(vault_placement &,
                        int vgrid,
                        const coord_def& where,
                        keyed_mapspec *mapsp);
static void _vault_grid_glyph(vault_placement &place, const coord_def& where,
                              int vgrid);
static void _vault_grid_mapspec(vault_placement &place, const coord_def& where,
                                keyed_mapspec& mapsp);
static dungeon_feature_type _vault_inspect(vault_placement &place,
                                           int vgrid, keyed_mapspec *mapsp);
static dungeon_feature_type _vault_inspect_mapspec(vault_placement &place,
                                                   keyed_mapspec& mapsp);
static dungeon_feature_type _vault_inspect_glyph(int vgrid);

static const map_def *_dgn_random_map_for_place(bool minivault);
static void _dgn_load_colour_grid();
static void _dgn_map_colour_fixup();

static void _dgn_unregister_vault(const map_def &map);
static void _remember_vault_placement(const vault_placement &place);

// Returns true if the given square is okay for use by any character,
// but always false for squares in non-transparent vaults.
static bool _dgn_square_is_passable(const coord_def &c);

static coord_def _dgn_random_point_in_bounds(
    dungeon_feature_type searchfeat,
    uint32_t mapmask = MMT_VAULT,
    dungeon_feature_type adjacent = DNGN_UNSEEN,
    bool monster_free = false,
    int tries = 1500);

// ALTAR FUNCTIONS
static int                  _setup_temple_altars(CrawlHashTable &temple);
static dungeon_feature_type _pick_temple_altar();
static dungeon_feature_type _pick_an_altar();

static vector<god_type> _temple_altar_list;
static CrawlHashTable*       _current_temple_hash = nullptr; // XXX: hack!

// MISC FUNCTIONS
static void _dgn_set_floor_colours();
static bool _fixup_interlevel_connectivity();
static void _slime_connectivity_fixup();

static void _dgn_postprocess_level();
static void _calc_density();
static void _mark_solid_squares();

//////////////////////////////////////////////////////////////////////////
// Static data

// A mask of vaults and vault-specific flags.
vector<vault_placement> Temp_Vaults;
static unique_creature_list temp_unique_creatures;
static FixedVector<unique_item_status_type, MAX_UNRANDARTS> temp_unique_items;

const map_bitmask *Vault_Placement_Mask = nullptr;

static bool use_random_maps = true;
static bool dgn_check_connectivity = false;
static int  dgn_zones = 0;

#ifdef DEBUG_STATISTICS
static vector<string> _you_all_vault_list;
#endif

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
static unique_ptr<dungeon_colour_grid> dgn_colour_grid;

static string branch_epilogues[NUM_BRANCHES];

set<string> &get_uniq_map_tags()
{
    if (you.where_are_you == BRANCH_ABYSS)
        return you.uniq_map_tags_abyss;
    else
        return you.uniq_map_tags;
}

set<string> &get_uniq_map_names()
{
    if (you.where_are_you == BRANCH_ABYSS)
        return you.uniq_map_names_abyss;
    else
        return you.uniq_map_names;
}

// TODO: datify into branch data
int dgn_builder_x()
{
    if (is_hell_subbranch(you.where_are_you) && !at_branch_bottom())
        return GXM / 2;
    else
        return GXM;
}

int dgn_builder_y()
{
    if (is_hell_subbranch(you.where_are_you) && !at_branch_bottom())
        return GYM / 2;
    else
        return GYM;
}

/**********************************************************************
 * builder() - kickoff for the dungeon generator.
 *********************************************************************/
bool builder(bool enable_random_maps)
{
#ifndef DEBUG_FULL_DUNGEON_SPAM
    // hide builder debug spam by default -- this is still collected by a tee
    // and accessible via &ctrl-l without this #define.
    msg::suppress quiet(MSGCH_DIAGNOSTICS);
#endif
    // you can use msg::force_stderr for doing debugging in automated scripts
    // here; usually you want to condition it to a specific level.

    // Re-check whether we're in a valid place, it leads to obscure errors
    // otherwise.
    ASSERT_RANGE(you.where_are_you, 0, NUM_BRANCHES);
    ASSERT_RANGE(you.depth, 0 + 1, brdepth[you.where_are_you] + 1);

    const set<string> uniq_tags = get_uniq_map_tags();
    const set<string> uniq_names = get_uniq_map_names();

    // For normal cases, this should already be taken care of by enter_level.
    // However, we want to be really sure that the builder isn't ever run with
    // the player at a real position on the level, e.g. during debug code or
    // tests, because that can impact levelgen (somehow) and cause seed
    // divergence. (I think it's because actor position can impact item gen,
    // but it's a bit hard to track down.)
    unwind_var<coord_def> saved_position(you.position);
    you.position.reset();

    // TODO: why are these globals?
    // Save a copy of unique creatures for vetoes.
    temp_unique_creatures = you.unique_creatures;
    // And unrands
    temp_unique_items = you.unique_items;

    unwind_bool levelgen(crawl_state.generating_level, true);
    rng::generator levelgen_rng(you.where_are_you);

#ifdef DEBUG_DIAGNOSTICS // no point in enabling unless dprf works
    CrawlHashTable &debug_logs = you.props[DEBUG_BUILDER_LOGS_KEY].get_table();
    string &cur_level_log = debug_logs[level_id::current().describe()].get_string();
    msg::tee debug_messages(cur_level_log);
    debug_messages.append_line(make_stringf("Builder log for %s:",
        level_id::current().describe().c_str()));
#endif

    // N tries to build the level, after which we bail with a capital B.
    int tries = 50;
    while (tries-- > 0)
    {
        // If we're getting low on available retries, disable random vaults
        // and minivaults (special levels will still be placed).
        if (tries < 5)
            enable_random_maps = false;

        try
        {
            // clean slate
            crawl_state.last_builder_error_fatal = false;

            if (_build_level_vetoable(enable_random_maps))
                return true;
#if defined(DEBUG_VETO_RESUME) && defined(WIZARD)
            else if (is_wizard_travel_target(level_id::current()))
            {
                // TODO: as-is, this has no way to look at vetos on D:1. You
                // can manually force this by adding `|| env.absdepth0 == 0` to
                // the condition above.
                mprf(MSGCH_ERROR, "Builder paused after veto; use &ctrl-r to resume.");
                // reset global state preemptively; otherwise wizard reload will
                // quickly deviate from the seed
                get_uniq_map_tags() = uniq_tags;
                get_uniq_map_names() = uniq_names;
                you.unique_creatures = temp_unique_creatures;
                you.unique_items = temp_unique_items;

                // Because vault generation can be interrupted, this potentially
                // leaves unlinked items kicking around, which triggers a lot
                // of debug messaging. For the sanity of the user, clean them
                // up.
                for (int i = 0; i < MAX_ITEMS; i++)
                    if (env.item[i].defined()
                        && !env.item[i].holding_monster()
                        && (!in_bounds(env.item[i].pos)
                            || env.igrid(env.item[i].pos) == NON_ITEM))
                    {
                        dprf("    Cleaning up unfinished item '%s'",
                                        env.item[i].name(DESC_PLAIN).c_str());
                        init_item(i);
                    }

                // Remove any portal entrances after a veto; otherwise they
                // will generate in the pregen code immediately, and can mess
                // up continuing on with the seed. Leave something behind so
                // that you can see where it was supposed to go.
                // This will still usually trigger the announcements, etc, and
                // possibly the flavor tile for the entrance.
                for (rectangle_iterator ri(0); ri; ++ri)
                {
                    dungeon_feature_type feat = env.grid(*ri);
                    if (feat_is_portal_entrance(feat) && !feature_mimic_at(*ri))
                    {
                        level_id whither = stair_destination(feat, "", false);
                        dprf("    Removing portal entrance to %s at %d,%d",
                                whither.describe().c_str(), (*ri).x, (*ri).y);
                        env.grid(*ri) = DNGN_STONE_ARCH;
                    }
                }
                update_portal_entrances();

                return true;
            }
#endif

        }
        catch (map_load_exception &mload)
        {
            mprf(MSGCH_ERROR, "Failed to load map, reloading all maps (%s).",
                 mload.what());
            reread_maps();
        }

        get_uniq_map_tags() = uniq_tags;
        get_uniq_map_names() = uniq_names;
        if (crawl_state.last_builder_error_fatal &&
            (you.props.exists(FORCE_MAP_KEY) || you.props.exists(FORCE_MINIVAULT_KEY)))
        {
            // if there was a fatal lua error and this is a forced levelgen,
            // it's most likely that the same thing will keep happening over
            // and over again (and the dev should want to fix the error
            // anyways.)
            mprf(MSGCH_ERROR, "Aborting &P builder on fatal lua error");
            break;
        }
        else if (crawl_state.last_builder_error_fatal)
        {
            mprf(MSGCH_ERROR,
                "Builder <white>VETO</white> on fatal lua error: %s",
                crawl_state.last_builder_error.c_str());
        }
    }

    if (!crawl_state.map_stat_gen && !crawl_state.obj_stat_gen)
    {
        // Failed to build level, bail out.
        // Don't crash at this point -- this needs to be handled in pregen code
        // in case the builder fails in the middle of a pregen sequence.

        // A failure here indicates that the builder vetoed or otherwise failed
        // 50 times in a row. This can occasionally happen in depths due to the
        // complexity of the placement constraints. In order to not break the
        // player's save, what should happen is that the rng state for the
        // relevant level get saved -- this will allow resuming the builder
        // for another 50 tries with a different level seed. This is handled
        // in `load_level`.

        // TODO: this doesn't show up in the player's log for some reason. But
        // it does show up in the builder log / crashlog at least.
        mprf(MSGCH_ERROR, "Unable to generate level for '%s'!",
            level_id::current().describe().c_str());
    }

    env.level_layout_types.clear(); // is this necessary?

    return false;
}

void dgn_record_veto(const dgn_veto_exception &e)
{
    string error = make_stringf("%s: %s",
         level_id::current().describe().c_str(), e.what());

    crawl_state.last_builder_error = error;

    dprf(DIAG_DNGN, "<white>VETO</white>: %s", error.c_str());

#ifdef DEBUG_STATISTICS
    mapstat_report_map_veto(e.what());
#endif

}

static bool _build_level_vetoable(bool enable_random_maps)
{
#ifdef DEBUG_STATISTICS
    mapstat_report_map_build_start();
#endif

    dgn_reset_level(enable_random_maps);

    if (player_in_branch(BRANCH_TEMPLE))
        _setup_temple_altars(you.props);

    crawl_state.last_builder_error = "";

    try
    {
        _build_dungeon_level();
    }
    catch (dgn_veto_exception& e)
    {
        dgn_record_veto(e);

        // try not to lose any ghosts that have been placed
        save_ghosts(ghost_demon::find_ghosts(false), false);
        return false;
    }

    _dgn_set_floor_colours();

    if (crawl_state.game_has_random_floors()
        && !crawl_state.game_is_descent()
        && !_valid_dungeon_level())
    {
        return false;
    }

#ifdef DEBUG_MONS_SCAN
    // If debug_mons_scan() finds a problem while crawl_state.generating_level is
    // still true then it will announce that a problem was caused
    // during level generation.
    debug_mons_scan();
#endif

    if (!env.level_build_method.empty()
        && env.level_build_method[0] == ' ')
    {
        env.level_build_method = env.level_build_method.substr(1);
    }

    string level_layout_type = comma_separated_line(
        env.level_layout_types.begin(),
        env.level_layout_types.end(), ", ");

    // Save information in the level's properties hash table
    // so we can include it in crash reports.
    env.properties[BUILD_METHOD_KEY] = env.level_build_method;
    env.properties[LAYOUT_TYPE_KEY]  = level_layout_type;

    _dgn_postprocess_level();

    env.level_layout_types.clear();
    env.level_uniq_maps.clear();
    env.level_uniq_map_tags.clear();
    _dgn_map_colour_fixup();

    // Call the branch epilogue, if any.
    if (!branch_epilogues[you.where_are_you].empty())
        if (!dlua.callfn(branch_epilogues[you.where_are_you].c_str(), 0, 0))
        {
            mprf(MSGCH_ERROR, "branch epilogue for %s failed: %s",
                              level_id::current().describe().c_str(),
                              dlua.error.c_str());
            return false;
        }

    // Discard any Lua chunks we loaded.
    strip_all_maps();

    check_map_validity();

#ifdef DEBUG_STATISTICS
    for (auto vault : _you_all_vault_list)
        mapstat_report_map_success(vault);
#endif

    return true;
}

// Things that are bugs where we want to assert rather than to sweep it under
// the rug with a veto.
static void _builder_assertions()
{
#ifdef ASSERTS
    for (rectangle_iterator ri(0); ri; ++ri)
        if (!in_bounds(*ri))
            if (!feat_is_valid_border(env.grid(*ri)))
            {
                die("invalid map border at (%d,%d): %s", ri->x, ri->y,
                    dungeon_feature_name(env.grid(*ri)));
            }
#endif
}

/**
 * Place a transporter and set its destination.
 *
 * @param pos     The position of the transporter
 * @param dest    The position the transporter destination.
 **/
void dgn_place_transporter(const coord_def &pos, const coord_def &dest)
{
    ASSERT(pos != dest);

    env.markers.add(new map_position_marker(pos, DNGN_TRANSPORTER, dest));
    env.markers.clear_need_activate();
    dungeon_terrain_changed(pos, DNGN_TRANSPORTER, false, true);
    dungeon_terrain_changed(dest, DNGN_TRANSPORTER_LANDING, false, true);
}

/**
 * Create transporters on the current level based on transporter markers. This
 * does checks for duplicate transporter destinations, transporters with no
 * destinations, and unused transporter destinations.
 *
 * @returns True if no transporter placement errors were found, false
 *          otherwise.
 **/
bool dgn_make_transporters_from_markers()
{
    bool no_errors = true;

    // Find transporter destination markers and ensure no duplicates.
    const vector<map_marker*> dest_markers =
        find_markers_by_prop(TRANSPORTER_DEST_NAME_PROP);
    map<string, map_marker *> dest_map;
    for (auto dm : dest_markers)
    {
        const string name = dm->property(TRANSPORTER_DEST_NAME_PROP);
        if (dest_map.find(name) != dest_map.end())
        {
            mprf(MSGCH_ERROR, "Multiple locations with transporter "
                 "destination name %s.", name.c_str());
            no_errors = false;
            continue;
        }
        dest_map[name] = dm;
    }

    // Place transporters.
    const vector<map_marker*> trans_markers =
        find_markers_by_prop(TRANSPORTER_NAME_PROP);
    map<string, bool> used_dest_map;
    for (auto tm : trans_markers)
    {
        const string name = tm->property(TRANSPORTER_NAME_PROP);
        if (dest_map.find(name) == dest_map.end())
        {
            mprf(MSGCH_ERROR, "Transporter with name %s has no corresponding "
                 "destination marker.", name.c_str());
            no_errors = false;
            continue;
        }
        dgn_place_transporter(tm->pos, dest_map[name]->pos);
        env.markers.remove(tm);
        used_dest_map[name] = true;
    }

    // Clean up any destination markers.
    for (auto dm : dest_markers)
    {
        const string name = dm->property(TRANSPORTER_DEST_NAME_PROP);
        if (used_dest_map[name])
            env.markers.remove(dm);
        else
        {
            mprf(MSGCH_ERROR, "Unused transporter destination with name %s.",
                 name.c_str());
            no_errors = false;
        }
    }

    return no_errors;
}

// Should be called after a level is constructed to perform any final
// fixups.
static void _dgn_postprocess_level()
{
    shoals_postprocess_level();
    _builder_assertions();
    _calc_density();
    _mark_solid_squares();
}

void dgn_clear_vault_placements()
{
    env.level_vaults.clear();
}

// Removes vaults that are not referenced in the map index mask from
// the level_vaults array.
void dgn_erase_unused_vault_placements()
{
    set<int> referenced_vault_indexes;
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        const int map_index = env.level_map_ids(*ri);
        if (map_index != INVALID_MAP_INDEX)
            referenced_vault_indexes.insert(map_index);
    }

    // Walk backwards and toss unused vaults.
    map<int, int> new_vault_index_map;
    const int nvaults = env.level_vaults.size();
    for (int i = nvaults - 1; i >= 0; --i)
    {
        if (!referenced_vault_indexes.count(i))
        {
            {
                auto &vp = env.level_vaults[i];
                // Unreferenced vault, blow it away
                dprf(DIAG_DNGN, "Removing references to unused map #%d)"
                        " '%s' (%d,%d) (%d,%d)",
                        i, vp->map.name.c_str(), vp->pos.x, vp->pos.y,
                        vp->size.x, vp->size.y);

                if (!vp->seen)
                {
                    dprf(DIAG_DNGN, "Unregistering unseen vault: %s",
                            vp->map.name.c_str());
                    _dgn_unregister_vault(vp->map);
                }
            }

            env.level_vaults.erase(env.level_vaults.begin() + i);

            // Fix new indexes for all higher indexed vaults that are
            // still referenced.
            for (int j = i + 1; j < nvaults; ++j)
                if (int *newidx = map_find(new_vault_index_map, j))
                    --*newidx;
        }
        else
        {
            // Vault is still referenced, make a note of this index.
            new_vault_index_map[i] = i;
        }
    }

    // Finally, update the index map.
    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        const int map_index = env.level_map_ids(*ri);
        if (map_index != INVALID_MAP_INDEX)
        {
            if (int *newidx = map_find(new_vault_index_map, map_index))
                env.level_map_ids(*ri) = *newidx;
        }
    }

#ifdef DEBUG_ABYSS
    dprf(DIAG_DNGN, "Extant vaults on level: %d",
         (int) env.level_vaults.size());
    int i = 0;
    for (auto &vp : env.level_vaults)
    {
        dprf(DIAG_DNGN, "%d) %s (%d,%d) size (%d,%d)",
             i++, vp->map.name.c_str(), vp->pos.x, vp->pos.y,
             vp->size.x, vp->size.y);
    }
#endif
}

void level_clear_vault_memory()
{
    dgn_clear_vault_placements();
    Temp_Vaults.clear();
    env.level_map_mask.init(0);
    env.level_map_ids.init(INVALID_MAP_INDEX);
}

/*
It's probably better in general to just reset `you` instead of calling this.
But that's not so convenient for some users of this function, notably lua
tests. This leaves some state uninitialized, and probably should be immediately
followed by a call to `initial_dungeon_setup` and something that moves the
player to a level or regenerates a level.
*/
void dgn_reset_player_data()
{

    // vaults and map stuff
    you.uniq_map_tags.clear();
    you.uniq_map_names.clear();
    you.uniq_map_tags_abyss.clear();
    you.uniq_map_names_abyss.clear();
    you.vault_list.clear();
    you.branches_left.reset();
    you.zigs_completed = 0;
    you.zig_max = 0;
    you.exploration = 0;
    you.seen_portals = 0; // should be just cosmetic
    reset_portal_entrances();
    // would it be safe to just clear you.props?
    you.props.erase(TEMPLE_SIZE_KEY);
    you.props.erase(TEMPLE_MAP_KEY);
    you.props.erase(OVERFLOW_TEMPLES_KEY);
    you.props.erase(TEMPLE_GODS_KEY);
    you.clear_place_info();
    // the following is supposed to clear any persistent lua state related to
    // the builder. However, it's susceptible to custom dlua doing its own
    // thing...
    dlua.callfn("dgn_clear_persistant_data", "");

    // monsters
    you.unique_creatures.reset();

    // item stuff that can interact with the builder
    you.runes.reset();
    you.obtainable_runes = 15;
    initialise_item_sets(true);
    you.unique_items.init(UNIQ_NOT_EXISTS);
    you.octopus_king_rings = 0x00;
    you.item_description.init(255); // random names need reset after this, e.g.
                                    // via debug.dungeon_setup()
    you.attribute[ATTR_GOLD_GENERATED] = 0;
    // potentially relevant for item placement in e.g. troves:
    you.seen_weapon.init(0);
    you.seen_armour.init(0);
    you.seen_misc.reset();
    you.seen_talisman.reset();
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
                    = coloured_feature(env.grid[x][y], env.grid_colours[x][y]);
            }
}

static void _dgn_map_colour_fixup()
{
    if (!dgn_colour_grid)
        return;

    // If the original coloured feature has been changed, reset the colour.
    const dungeon_colour_grid &dcgrid(*dgn_colour_grid);
    for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
            if (dcgrid[x][y].colour != BLACK
                && env.grid[x][y] != dcgrid[x][y].feature
                && dcgrid[x][y].feature != DNGN_FLOOR)
            {
                env.grid_colours[x][y] = BLACK;
            }

    dgn_colour_grid.reset(nullptr);
}

void dgn_set_grid_colour_at(const coord_def &c, int colour)
{
    if (colour != BLACK)
    {
        env.grid_colours(c) = colour;
        if (!dgn_colour_grid)
            dgn_colour_grid.reset(new dungeon_colour_grid);

        (*dgn_colour_grid)(c) = coloured_feature(env.grid(c), colour);
    }
}

static void _set_grd(const coord_def &c, dungeon_feature_type feat)
{
    // It might be good to clear some pgrid flags as well.
    tile_env.flv(c).feat    = 0;
    tile_env.flv(c).special = 0;
    env.grid_colours(c) = 0;
    env.grid(c) = feat;
}

static void _dgn_register_vault(const string &name, const unordered_set<string> &tags)
{
    if (!tags.count("allow_dup"))
        get_uniq_map_names().insert(name);

    if (tags.count("luniq"))
        env.level_uniq_maps.insert(name);

    vector<string> sorted_tags(tags.begin(), tags.end());
    sort(sorted_tags.begin(), sorted_tags.end());

    for (const string &tag : sorted_tags)
    {
        if (starts_with(tag, "uniq_"))
            get_uniq_map_tags().insert(tag);
        else if (starts_with(tag, "luniq_"))
            env.level_uniq_map_tags.insert(tag);
    }
}

static void _dgn_register_vault(const map_def &map)
{
    _dgn_register_vault(map.name, map.get_tags_unsorted());
}

static void _dgn_register_vault(const string &name, string &spaced_tags)
{
    _dgn_register_vault(name, parse_tags(spaced_tags));
}

static void _dgn_unregister_vault(const map_def &map)
{
    get_uniq_map_names().erase(map.name);
    env.level_uniq_maps.erase(map.name);

    for (const string &tag : map.get_tags_unsorted())
    {
        if (starts_with(tag, "uniq_"))
            get_uniq_map_tags().erase(tag);
        else if (starts_with(tag, "luniq_"))
            env.level_uniq_map_tags.erase(tag);
    }

    for (const subvault_place &sub : map.subvault_places)
        _dgn_unregister_vault(*sub.subvault);
}

bool dgn_square_travel_ok(const coord_def &c)
{
    const dungeon_feature_type feat = env.grid(c);
    if (feat_is_trap(feat))
    {
        const trap_def * const trap = trap_at(c);
        return !(trap && (trap->type == TRAP_TELEPORT_PERMANENT
                          || trap->type == TRAP_DISPERSAL));
    }
    else // the mimic check here relies on full placement operating, e.g. not &L
        return feat_is_traversable(feat) || feature_mimic_at(c);
}

static bool _dgn_square_is_tele_connected(const coord_def &c)
{
    // all solid features get marked with no_tele_into, including doors, so
    // undo this more or less so that this function can be used for connectivity
    // checking; except when the entire vault is notele
    const bool vault_notele = dgn_vault_at(c)
                    && dgn_vault_at(c)->map.has_tag("no_tele_into");
    return (!(env.pgrid(c) & FPROP_NO_TELE_INTO)
                            || !vault_notele && feat_is_solid(env.grid(c)))
        && dgn_square_travel_ok(c);
}

static bool _dgn_square_is_passable(const coord_def &c)
{
    // [enne] Why does this function check these flags?
    //
    // Don't peek inside MMT_OPAQUE vaults (all vaults are opaque by
    // default) because vaults may choose to create isolated regions,
    // or otherwise cause connectivity issues even if the map terrain
    // is travel-passable.
    //
    // For vaults place by Vaults layouts, however, we set MMT_PASSABLE
    // and always assume they are properly passable from one entrance
    // to the other.
    // The V layout code already guarantees that the level is connected
    // up validly and room entrances connected up to the doors.
    return env.level_map_mask(c) & MMT_PASSABLE
        || !(env.level_map_mask(c) & MMT_OPAQUE) && dgn_square_travel_ok(c);
}

static bool _dgn_square_is_boring(const coord_def &c)
{
    // ignore vault squares by the same algorithm as _dgn_square_is_passable,
    // but in this case find only floor squares.
    const dungeon_feature_type feat = env.grid(c);
    return (feat_has_solid_floor(feat) || feat_is_door(feat))
        && (env.mgrid(c) == NON_MONSTER
            || env.mons[env.mgrid(c)].is_firewood())
        && (env.level_map_mask(c) & MMT_PASSABLE
            || !(env.level_map_mask(c) & MMT_OPAQUE));
}

static bool _dgn_square_is_ever_passable(const coord_def &c)
{
    if (!(env.level_map_mask(c) & MMT_OPAQUE))
    {
        const dungeon_feature_type feat = env.grid(c);
        if (feat == DNGN_DEEP_WATER || feat == DNGN_LAVA)
            return true;
    }
    return _dgn_square_is_passable(c);
}

static inline void _dgn_point_record_stub(const coord_def &) { }

template <class point_record>
static bool _dgn_fill_zone(
    const coord_def &start, int zone,
    point_record &record_point,
    bool (*passable)(const coord_def &) = _dgn_square_is_passable,
    bool (*iswanted)(const coord_def &) = nullptr)
{
    bool ret = false;
    list<coord_def> points[2];
    int cur = 0;
#ifdef DEBUG_DIAGNOSTICS
    int found_points = 0;
#endif

    // No bounds checks, assuming the level has at least one layer of
    // rock border.

    for (points[cur].push_back(start); !points[cur].empty();)
    {
        for (const auto &c : points[cur])
        {
            travel_point_distance[c.x][c.y] = zone;
#ifdef DEBUG_DIAGNOSTICS
            found_points++;
#endif

            if (iswanted && iswanted(c))
                ret = true;

            for (adjacent_iterator ai(c); ai; ++ai)
            {
                const coord_def& cp = *ai;
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
    dprf("Zone %d contains %d points from seed %d,%d", zone, found_points,
        start.x, start.y);
    return ret;
}

static bool _is_perm_down_stair(const coord_def &c)
{
    switch (env.grid(c))
    {
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
    case DNGN_EXIT_HELL:
    case DNGN_EXIT_PANDEMONIUM:
    case DNGN_TRANSIT_PANDEMONIUM:
    case DNGN_EXIT_ABYSS:
    case DNGN_ABYSSAL_STAIR:
        return true;
    default:
        return false;
    }
}

static bool _is_upwards_exit_stair(const coord_def &c)
{
    // Is this a valid upwards or exit stair out of a branch? In general,
    // ensure that each region has a stone stair up.

    if (feature_mimic_at(c) || env.grid(c) == DNGN_EXIT_HELL)
        return false;

    if (feat_is_stone_stair_up(env.grid(c))
        || feat_is_branch_exit(env.grid(c)))
    {
        return true;
    }

    switch (env.grid(c))
    {
    case DNGN_EXIT_PANDEMONIUM:
    case DNGN_TRANSIT_PANDEMONIUM:
    case DNGN_EXIT_ABYSS:
        return true;
    default:
        return false;
    }
}

static bool _is_exit_stair(const coord_def &c)
{
    if (feature_mimic_at(c) || env.grid(c) == DNGN_EXIT_HELL)
        return false;

    // Branch entries, portals, and abyss entries are not considered exit
    // stairs here, as they do not provide an exit (in a transitive sense) from
    // the current level.
    if (feat_is_stone_stair(env.grid(c))
        || feat_is_escape_hatch(env.grid(c))
        || feat_is_branch_exit(env.grid(c)))
    {
        return true;
    }

    switch (env.grid(c))
    {
    case DNGN_EXIT_PANDEMONIUM:
    case DNGN_TRANSIT_PANDEMONIUM:
    case DNGN_EXIT_ABYSS:
        return true;
    default:
        return false;
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
// TODO: refactor this to something more usable
static int _process_disconnected_zones(int x1, int y1, int x2, int y2,
                bool choose_stairless,
                dungeon_feature_type fill,
                bool (*passable)(const coord_def &) = _dgn_square_is_passable,
                bool (*fill_check)(const coord_def &) = nullptr,
                int fill_small_zones = 0)
{
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));
    int nzones = 0;
    int ngood = 0;
    for (int y = y1; y <= y2 ; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            if (!map_bounds(x, y)
                || travel_point_distance[x][y]
                || !passable(coord_def(x, y)))
            {
                continue;
            }

            int zone_size = 0;
            auto inc_zone_size = [&zone_size](const coord_def &) { zone_size++; };

            const bool found_exit_stair =
                _dgn_fill_zone(coord_def(x, y), ++nzones,
                               inc_zone_size,
                               passable,
                               choose_stairless ? (at_branch_bottom() ?
                                                   _is_upwards_exit_stair :
                                                   _is_exit_stair) : nullptr);

            // If we want only stairless zones, screen out zones that did
            // have stairs.
            if (choose_stairless && found_exit_stair)
                ++ngood;
            else if (fill
                && (fill_small_zones <= 0 || zone_size <= fill_small_zones))
            {
                // Don't fill in areas connected to vaults.
                // We want vaults to be accessible; if the area is disconnected
                // from the rest of the level, this will cause the level to be
                // vetoed later on.
                bool veto = false;
                vector<coord_def> coords;
                dprf("Filling zone %d", nzones);
                for (int fy = y1; fy <= y2 ; ++fy)
                {
                    for (int fx = x1; fx <= x2; ++fx)
                    {
                        if (travel_point_distance[fx][fy] == nzones)
                        {
                            if (map_masked(coord_def(fx, fy), MMT_VAULT))
                            {
                                veto = true;
                                break;
                            }
                            else if (!fill_check || fill_check(coord_def(fx, fy)))
                                coords.emplace_back(fx, fy);
                        }
                    }
                    if (veto)
                        break;
                }
                if (!veto)
                {
                    for (auto c : coords)
                    {
                        // For normal builder scenarios items shouldn't be
                        // placed yet, but it could (if not careful) happen
                        // in weirder cases, such as the abyss.
                        if (env.igrid(c) != NON_ITEM
                            && (!feat_is_traversable(fill)
                                || feat_destroys_items(fill)))
                        {
                            // Alternatively, could place floor instead?
                            dprf("Nuke item stack at (%d, %d)", c.x, c.y);
                            lose_item_stack(c);
                        }
                        _set_grd(c, fill);
                        if (env.mgrid(c) != NON_MONSTER
                            && !env.mons[env.mgrid(c)].is_habitable_feat(fill))
                        {
                            monster_die(env.mons[env.mgrid(c)],
                                        KILL_RESET, NON_MONSTER, true);
                        }
                    }
                }
            }
        }
    }

    return nzones - ngood;
}

int dgn_count_tele_zones(bool choose_stairless)
{
    dprf("Counting teleport zones");
    return _process_disconnected_zones(0, 0, GXM-1, GYM-1, choose_stairless,
                                    DNGN_UNSEEN, _dgn_square_is_tele_connected);
}

// Count number of mutually isolated zones. If choose_stairless, only count
// zones with no stairs in them. If fill is set to anything other than
// DNGN_UNSEEN, chosen zones will be filled with the provided feature.
int dgn_count_disconnected_zones(bool choose_stairless,
                                 dungeon_feature_type fill)
{
    return _process_disconnected_zones(0, 0, GXM-1, GYM-1, choose_stairless,
                                       fill);
}

static void _fill_small_disconnected_zones()
{
    // debugging tip: change the feature to something like lava that will be
    // very noticeable.
    // TODO: make even more aggressive, up to ~25?
    _process_disconnected_zones(0, 0, GXM-1, GYM-1, true, DNGN_ROCK_WALL,
                                       _dgn_square_is_passable,
                                       _dgn_square_is_boring,
                                       10);
}

static void _fixup_hell_stairs()
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (feat_is_stone_stair_up(env.grid(*ri))
            || env.grid(*ri) == DNGN_ESCAPE_HATCH_UP)
        {
            _set_grd(*ri, branches[you.where_are_you].escape_feature);
        }
    }
}

static void _fixup_pandemonium_stairs()
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (feat_is_stone_stair_up(env.grid(*ri))
            || env.grid(*ri) == DNGN_ESCAPE_HATCH_UP)
        {
            _set_grd(*ri, DNGN_TRANSIT_PANDEMONIUM);
        }
    }
}

static void _fixup_descent_hatches()
{
    for (rectangle_iterator ri(1); ri; ++ri)
        if (env.grid(*ri) == DNGN_ESCAPE_HATCH_UP)
            _set_grd(*ri, DNGN_FLOOR);
}

static void _dgn_place_feature_at_random_floor_square(dungeon_feature_type feat,
                                                      unsigned mask = MMT_VAULT)
{
    coord_def place = _dgn_random_point_in_bounds(DNGN_FLOOR, mask, DNGN_FLOOR);
    if (player_in_branch(BRANCH_SLIME))
    {
        int tries = 100;
        while (!place.origin()  // stop if we fail to find floor.
               && (dgn_has_adjacent_feat(place, DNGN_ROCK_WALL)
                   || dgn_has_adjacent_feat(place, DNGN_SLIMY_WALL))
               && tries-- > 0)
        {
            place = _dgn_random_point_in_bounds(DNGN_FLOOR, mask, DNGN_FLOOR);
        }

        if (tries < 0)  // tries == 0 means we succeeded on the last attempt
            place.reset();
    }
    if (place.origin())
        throw dgn_veto_exception("Cannot place feature at random floor square.");
    else
        _set_grd(place, feat);
}

static void _place_dungeon_exit()
{
    if (!player_in_branch(BRANCH_ZOT) || !at_branch_bottom())
        return;

    _dgn_place_feature_at_random_floor_square(
                static_cast<dungeon_feature_type>(DNGN_EXIT_DUNGEON));
}

static void _mask_vault(const vault_placement &place, unsigned mask,
                        function<bool(const coord_def &)> skip_fun = nullptr)
{
    for (vault_place_iterator vi(place); vi; ++vi)
        if (!skip_fun || !skip_fun(*vi))
            env.level_map_mask(*vi) |= mask;
}

static void _dgn_apply_map_index(const vault_placement &place, int map_index)
{
    for (vault_place_iterator vi(place); vi; ++vi)
        env.level_map_ids(*vi) = map_index;
}

const vault_placement *
dgn_register_place(const vault_placement &place, bool register_vault)
{
    const int  map_index    = env.level_vaults.size();
    const bool overwritable = place.map.is_overwritable_layout();
    const bool transparent  = place.map.has_tag("transparent");

    if (register_vault)
    {
        _dgn_register_vault(place.map);
        for (int i = env.new_subvault_names.size() - 1; i >= 0; i--)
        {
            _dgn_register_vault(env.new_subvault_names[i],
                                env.new_subvault_tags[i]);
        }
        clear_subvault_stack();

        // Identify each square in the map with its map_index.
        if (!overwritable)
            _dgn_apply_map_index(place, map_index);
    }

    if (!overwritable)
    {
        if (place.map.orient == MAP_ENCOMPASS)
        {
            for (rectangle_iterator ri(0); ri; ++ri)
                env.level_map_mask(*ri) |= MMT_VAULT;
        }
        else
            _mask_vault(place, MMT_VAULT);

        if (place.map.has_tag("passable") && player_in_branch(BRANCH_VAULTS))
        {
            // Ignore outside of Vaults -- creates too many bugs otherwise.
            // This tag is mainly to allow transporter vaults to work with
            // Vaults layout code.
            _mask_vault(place, MMT_PASSABLE);
        }
        else if (!transparent)
        {
            // mask everything except for any marked vault exits as opaque.
            // This uses vault exits marked by @, +, = on the vault edge, and
            // in a pinch, spaces in floats that are necessary for exits.
            // (This is overridden by explicit masking, or no_exits.)
            _mask_vault(place, MMT_OPAQUE,
                [&place](const coord_def &c)
                {
                    return !place.map.has_tag("no_exits")
                        && count(place.exits.begin(), place.exits.end(), c);
                });
        }
    }

    // Find tags matching properties.
    for (const auto &tag : place.map.get_tags_unsorted())
    {
        const feature_property_type prop = str_to_fprop(tag);
        if (prop == FPROP_NONE)
            continue;

        for (vault_place_iterator vi(place); vi; ++vi)
            env.pgrid(*vi) |= prop;

    }

    if (place.map.has_tag("no_monster_gen"))
        _mask_vault(place, MMT_NO_MONS);

    if (place.map.has_tag("no_item_gen"))
        _mask_vault(place, MMT_NO_ITEM);

    if (place.map.has_tag("no_pool_fixup"))
        _mask_vault(place, MMT_NO_POOL);

    if (place.map.has_tag("no_wall_fixup"))
        _mask_vault(place, MMT_NO_WALL);

    if (place.map.has_tag("no_trap_gen"))
        _mask_vault(place, MMT_NO_TRAP);

    // Now do per-square by-symbol masking.
    for (vault_place_iterator vi(place); vi; ++vi)
    {
        const keyed_mapspec *spec = place.map.mapspec_at(*vi - place.pos);

        if (spec != nullptr)
        {
            env.level_map_mask(*vi) |= (short)spec->map_mask.flags_set;
            env.level_map_mask(*vi) &= ~((short)spec->map_mask.flags_unset);
        }
    }

    if (place.map.floor_colour != BLACK)
        env.floor_colour = place.map.floor_colour;

    if (place.map.rock_colour != BLACK)
        env.rock_colour = place.map.rock_colour;

    if (!place.map.rock_tile.empty())
    {
        tileidx_t rock;
        if (tile_dngn_index(place.map.rock_tile.c_str(), &rock))
        {
            tile_env.default_flavour.wall_idx =
                store_tilename_get_index(place.map.rock_tile);

            tile_env.default_flavour.wall = rock;
        }
    }

    if (!place.map.floor_tile.empty())
    {
        tileidx_t floor;
        if (tile_dngn_index(place.map.floor_tile.c_str(), &floor))
        {
            tile_env.default_flavour.floor_idx =
                store_tilename_get_index(place.map.floor_tile);

            tile_env.default_flavour.floor = floor;
        }
    }

    vault_placement *new_vault_place = new vault_placement(place);
    env.level_vaults.emplace_back(new_vault_place);
    if (register_vault)
        _remember_vault_placement(place);
    return new_vault_place;
}

static bool _dgn_ensure_vault_placed(bool vault_success,
                                     bool disable_further_vaults,
                                     string desc)
{
    if (!vault_success)
    {
        // the exception message will overwrite last_builder_error, so package
        // this info in here
        string err = make_stringf("Vault placement failure for '%s'.", desc.c_str());
        if (!crawl_state.last_builder_error.empty())
            err += " " + crawl_state.last_builder_error;
        throw dgn_veto_exception(err);
    }
    else if (disable_further_vaults)
        use_random_maps = false;
    return vault_success;
}

static bool _ensure_vault_placed_ex(bool vault_success, const map_def *vault)
{
    return _dgn_ensure_vault_placed(vault_success,
                                    (!vault->is_extra_vault()
                                     && vault->orient == MAP_ENCOMPASS),
                                    vault->name);
}

static coord_def _find_level_feature(int feat)
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (env.grid(*ri) == feat)
            return *ri;
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

    coord_def where = ff.find_first_from(c, env.level_map_mask);
    return where.x || !ff.did_leave_vault();
}

static bool _has_connected_downstairs_from(const coord_def &c)
{
    flood_find<feature_grid, coord_predicate> ff(env.grid, in_bounds);
    ff.add_feat(DNGN_STONE_STAIRS_DOWN_I);
    ff.add_feat(DNGN_STONE_STAIRS_DOWN_II);
    ff.add_feat(DNGN_STONE_STAIRS_DOWN_III);
    ff.add_feat(DNGN_ESCAPE_HATCH_DOWN);

    coord_def where = ff.find_first_from(c, env.level_map_mask);
    return where.x || !ff.did_leave_vault();
}

static bool _is_level_stair_connected(dungeon_feature_type feat)
{
    coord_def up = _find_level_feature(feat);
    if (up.x && up.y)
        return _has_connected_downstairs_from(up);

    return false;
}

static bool _valid_dungeon_level()
{
    // D:1 only.
    // Also, what's the point of this check?  Regular connectivity should
    // do that already.
    if (player_in_branch(BRANCH_DUNGEON) && you.depth == 1)
        return _is_level_stair_connected(branches[BRANCH_DUNGEON].exit_stairs);

    return true;
}

void dgn_reset_level(bool enable_random_maps)
{
    env.level_uniq_maps.clear();
    env.level_uniq_map_tags.clear();
    clear_subvault_stack();

    you.unique_creatures = temp_unique_creatures;
    you.unique_items = temp_unique_items;

#ifdef DEBUG_STATISTICS
    _you_all_vault_list.clear();
#endif
    env.level_build_method.clear();
    env.level_layout_types.clear();
    level_clear_vault_memory();
    dgn_colour_grid.reset(nullptr);

    use_random_maps = enable_random_maps;
    dgn_check_connectivity = false;
    dgn_zones        = 0;

    _temple_altar_list.clear();
    _current_temple_hash = nullptr;

    // Forget level properties.
    env.properties.clear();
    env.heightmap.reset(nullptr);

    env.absdepth0 = absdungeon_depth(you.where_are_you, you.depth);

    if (!crawl_state.test)
        dprf(DIAG_DNGN, "absdepth0 = %d", env.absdepth0);

    // Blank level with DNGN_ROCK_WALL.
    env.grid.init(DNGN_ROCK_WALL);
    env.pgrid.init(terrain_property_t{});
    env.grid_colours.init(BLACK);
    env.map_knowledge.init(map_cell());
    env.map_forgotten.reset();
    env.map_seen.reset();

    // Delete all traps.
    env.trap.clear();

    // Initialise all items.
    for (int i = 0; i < MAX_ITEMS; i++)
        init_item(i);

    // Reset all monsters.
    reset_all_monsters();
    init_anon();

    // ... and Pan/regular spawn lists.
    env.mons_alloc.init(MONS_NO_MONSTER);
    setup_vault_mon_list();

    env.cloud.clear();

    env.mgrid.init(NON_MONSTER);
    env.igrid.init(NON_ITEM);

    // Reset all shops.
    env.shop.clear();

    // Clear all markers.
    env.markers.clear();

    // Lose all listeners.
    dungeon_events.clear();

    env.density = 0;
    env.forest_awoken_until = 0;

    env.floor_colour = BLACK;
    env.rock_colour  = BLACK;

    // Clear exclusions
    clear_excludes();

    // Clear custom tile settings from vaults
    tile_init_default_flavour();
    tile_clear_flavour();
    tile_env.names.clear();

    update_portal_entrances();
}

static int _num_items_wanted(int absdepth0)
{
    if (branches[you.where_are_you].branch_flags & brflag::no_items)
        return 0;
    else if (absdepth0 > 5 && one_chance_in(500 - 5 * absdepth0))
        return 9 + random2avg(80, 2); // rich level!
    else
        return 3 + roll_dice(3, 9);
}

static int _mon_die_size()
{
    // This is a very goofy hack to maintain historical populations.
    // TODO: remove this!
    if (you.where_are_you == BRANCH_LAIR && you.depth == 5)
        return DEFAULT_MON_DIE_SIZE;

    const int size = branches[you.where_are_you].mon_die_size;
    if (you.where_are_you != BRANCH_DUNGEON)
        return size;

    // Dungeon is a very special place and needs a lot of hand-holding.
    // Historically we used weird mysterious MONS_NO_MONSTER weights for
    // this balancing, but now we have technology.
    switch (you.depth)
    {
        case 1:
            return 12;
        case 2:
            return 10;
        case 3:
        case 4:
            return 9;
        case 5:
        case 6:
            return 7;
        case 7:
            return 6;
        case 8:
        case 9:
            return 5;
        case 10:
            return 4;
        case 11:
            return 5;
        case 12:
        case 13:
        case 14:
        case 15:
            return 6;
        default:
            return 12;
    }
}

// Return how many level monster are wanted for level generation.
static int _num_mons_wanted()
{
    const bool in_pan = player_in_branch(BRANCH_PANDEMONIUM);
    // No disconnected branches aside from Pan have level monsters.
    if ((!player_in_connected_branch() && !in_pan)
        // Temple is connected but has no monsters.
        || !branch_has_monsters(you.where_are_you))
    {
        return 0;
    }

    int mon_wanted = roll_dice(3, _mon_die_size());
    if (mon_wanted > 60)
        mon_wanted = 60;
    return mon_wanted;
}

static void _fixup_walls()
{
    // If level part of Dis -> all walls metal.
    // If Vaults:$ -> all walls metal or crystal.
    // If part of crypt -> all walls stone.

    dungeon_feature_type wall_type = DNGN_ROCK_WALL;

    if (!player_in_connected_branch())
        return;

    switch (you.where_are_you)
    {
    case BRANCH_DIS:
        wall_type = DNGN_METAL_WALL;
        break;

    case BRANCH_VAULTS:
    {
        // Everything but the branch end is handled in Lua.
        if (you.depth == branches[BRANCH_VAULTS].numlevels)
        {
            wall_type = random_choose_weighted(1, DNGN_CRYSTAL_WALL,
                                               9, DNGN_METAL_WALL);
        }
        break;
    }

    case BRANCH_CRYPT:
        wall_type = DNGN_STONE_WALL;
        break;

    case BRANCH_SLIME:
        wall_type = DNGN_SLIMY_WALL;
        break;

    default:
        return;
    }

    dgn_replace_area(0, 0, GXM-1, GYM-1, DNGN_ROCK_WALL, wall_type,
                     MMT_NO_WALL);
}

// Remove any items that are on squares that items should not be on.
// link_items() must be called after this function.
void fixup_misplaced_items()
{
    for (auto &item : env.item)
    {
        if (!item.defined() || item.held_by_monster())
            continue;

        if (in_bounds(item.pos))
        {
            dungeon_feature_type feat = env.grid(item.pos);
            if (feat_has_solid_floor(feat))
                continue;

            // We accept items in deep water in the Abyss---they are likely to
            // be revealed eventually by morphing, and having deep water push
            // items away leads to strange results.
            if (feat == DNGN_DEEP_WATER && player_in_branch(BRANCH_ABYSS))
                continue;

            mprf(MSGCH_ERROR, "Item %s buggily placed in feature %s at (%d, %d).",
                 item.name(DESC_PLAIN).c_str(),
                 feature_description_at(item.pos, false, DESC_PLAIN).c_str(),
                 item.pos.x, item.pos.y);
        }
        else
        {
            mprf(MSGCH_ERROR, "Item buggily placed out of bounds at (%d, %d).",
                 item.pos.x, item.pos.y);
        }

        // Can't just unlink item because it might not have been linked yet.
        item.base_type = OBJ_UNASSIGNED;
        item.quantity = 0;
        item.pos.reset();
    }
}

/*
 * At the top or bottom of a branch, adjust or remove illegal stairs:
 *
 * - non-vault escape hatches pointing outside the branch are removed
 * - non-vault stone stairs down from X:$ are removed
 * - stone stairs up from X:1 are replaced with branch exit feature
 *   - if there is more than one such stair, an error is logged
 * - vault-specified hatches or stairs are replaced with appropriate features
 *   - hatches down from X:$ are pointed up instead, and vice versa on X:1
 *   - for single-level branches, all hatches turn into the branch exit feature
 *   - if the branch has an "escape feature", it is used instead of hatches up
 */
static void _fixup_branch_stairs()
{
    const auto& branch = your_branch();
    const bool root = player_in_branch(root_branch);
    const bool top = you.depth == 1;
    const bool bottom = at_branch_bottom();

    const dungeon_feature_type exit =
        root ? DNGN_EXIT_DUNGEON
             : branch.exit_stairs;
    const dungeon_feature_type escape =
        branch.escape_feature == NUM_FEATURES ? DNGN_ESCAPE_HATCH_UP :
                                                branch.escape_feature;
    const dungeon_feature_type up_hatch =
        top && bottom ? exit :
                  top ? DNGN_ESCAPE_HATCH_DOWN :
                        escape;

#ifdef DEBUG_DIAGNOSTICS
    int count = 0;
#endif
    // Just in case we somehow get here with more than one stair placed.
    // Prefer stairs that are placed in vaults for picking an exit at
    // random.
    vector<coord_def> vault_stairs, normal_stairs;
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        const bool vault = map_masked(*ri, MMT_VAULT);
        const auto escape_replacement = vault ? up_hatch : DNGN_FLOOR;
        if (bottom && (feat_is_stone_stair_down(env.grid(*ri))
                       || env.grid(*ri) == DNGN_ESCAPE_HATCH_DOWN))
        {
            _set_grd(*ri, escape_replacement);
        }

        if (top)
        {
            if (env.grid(*ri) == DNGN_ESCAPE_HATCH_UP)
                _set_grd(*ri, escape_replacement);
            else if (feat_is_stone_stair_up(env.grid(*ri)))
            {
#ifdef DEBUG_DIAGNOSTICS
                if (count++ && !root)
                {
                    mprf(MSGCH_ERROR, "Multiple branch exits on %s",
                         level_id::current().describe().c_str());
                }
#endif
                if (root)
                {
                    env.markers.add(new map_feature_marker(*ri, env.grid(*ri)));
                    _set_grd(*ri, exit);
                }
                else
                {
                    if (vault)
                        vault_stairs.push_back(*ri);
                    else
                        normal_stairs.push_back(*ri);
                }
            }
        }
    }
    if (!root)
    {
        vector<coord_def> stairs;
        if (!vault_stairs.empty())
            stairs = vault_stairs;
        else
            stairs = normal_stairs;

        if (!stairs.empty())
        {
            shuffle_array(stairs);
            coord_def coord = *(stairs.begin());
            env.markers.add(new map_feature_marker(coord, env.grid(coord)));
            _set_grd(coord, exit);
            for (auto it = stairs.begin() + 1; it != stairs.end(); it++)
                _set_grd(*it, DNGN_FLOOR);
        }
    }
}

/// List all stone stairs of the indicated type.
static list<coord_def> _find_stone_stairs(bool up_stairs)
{
    list<coord_def> stairs;

    for (rectangle_iterator ri(1); ri; ++ri)
    {
        const coord_def& c = *ri;
        if (feature_mimic_at(c))
            continue;

        const dungeon_feature_type feat = env.grid(c);
        if (feat_is_stone_stair(feat)
            && up_stairs == feat_is_stone_stair_up(feat))
        {
            stairs.push_back(c);
        }
    }

    return stairs;
}

/**
 * Try to turn excess stairs into hatches.
 *
 * @param stairs[in,out]    The list of stairs to be trimmed; any stairs that
 *                          are turned into hatches will be removed.
 * @param needed_stairs     The desired number of stairs.
 * @param preserve_vault_stairs    Don't trapdoorify stairs that are in vaults.
 * @param hatch_type        What sort of hatch to turn excess stairs into.
 */
static void _cull_redundant_stairs(list<coord_def> &stairs,
                                   unsigned int needed_stairs,
                                   bool preserve_vault_stairs,
                                   dungeon_feature_type hatch_type)
{
    // we're going to iterate over the list, looking for redundant stairs.
    // (redundant = can walk from one to the other.) For each of
    // those iterations, we'll iterate over the remaining list checking for
    // stairs redundant with the outer iteration, and hatchify + remove from
    // the stair list any redundant stairs we find.

    for (auto iter1 = stairs.begin();
         iter1 != stairs.end() && stairs.size() <= needed_stairs;
         ++iter1)
    {
        const coord_def s1_loc = *iter1;
        // Ensure we don't search for the feature at s1. XXX: unwind_var?
        const dungeon_feature_type saved_feat = env.grid(s1_loc);
        env.grid(s1_loc) = DNGN_FLOOR;

        auto iter2 = iter1;
        ++iter2;
        while (iter2 != stairs.end() && stairs.size() > needed_stairs)
        {
            const coord_def s2_loc = *iter2;
            auto being_examined = iter2;
            ++iter2;

            if (preserve_vault_stairs && map_masked(s2_loc, MMT_VAULT))
                continue;

            flood_find<feature_grid, coord_predicate> ff(env.grid,
                                                         in_bounds);
            ff.add_feat(env.grid(s2_loc));
            const coord_def where =
                ff.find_first_from(s1_loc, env.level_map_mask);
            if (!where.x) // these stairs aren't in the same zone
                continue;

            dprf(DIAG_DNGN,
                 "Too many stairs -- removing one of a connected pair.");
            env.grid(s2_loc) = hatch_type;
            stairs.erase(being_examined);
        }

        env.grid(s1_loc) = saved_feat;
    }
}

/**
 * Trapdoorify stairs at random, until we reach the specified number.
 * @param stairs[in,out]    The list of stairs to be trimmed; any stairs that
 *                          are turned into hatches will be removed. Order not
 *                          preserved.
 * @param needed_stairs     The desired number of stairs.
 * @param preserve_vault_stairs    Don't remove stairs that are in vaults.
 * @param hatch_type        What sort of hatch to turn excess stairs into.
 */
static void _cull_random_stairs(list<coord_def> &stairs,
                                unsigned int needed_stairs,
                                bool preserve_vault_stairs,
                                dungeon_feature_type hatch_type)
{
    while (stairs.size() > needed_stairs)
    {
        const int remove_index = random2(stairs.size());
        // rotate the list until the desired element is in front
        for (int i = 0; i < remove_index; ++i)
        {
            stairs.push_back(stairs.front());
            stairs.pop_front();
        }

        if (preserve_vault_stairs)
        {
            // try to rotate a non-vault stair to the front, if possible.
            for (size_t i = 0; i < stairs.size(); ++i)
            {
                if (map_masked(stairs.front(), MMT_VAULT))
                {
                    stairs.push_back(stairs.front());
                    stairs.pop_front();
                }
            }

            // If we looped through all possibilities, then it
            // means that there are more than 3 stairs in vaults and
            // we can't preserve vault stairs.
            if (map_masked(stairs.front(), MMT_VAULT))
            {
                dprf(DIAG_DNGN, "Too many stairs inside vaults!");
                return;
            }
        }

        dprf(DIAG_DNGN, "Too many stairs -- removing one blindly.");
        _set_grd(stairs.front(), hatch_type);
        stairs.pop_front();
    }
}

/**
 * Ensure that there is only the correct number of each type of 'stone'
 * (permanent, intra-branch, non-trapdoor) stair on the current level.
 *
 * @param preserve_vault_stairs     Don't delete or trapdoorify stairs that are
 *                                  in vaults.
 * @param checking_up_stairs        Whether we're looking at stairs that lead
 *                                  up. If false, we're looking at down-stairs.
 * @return                          Whether we successfully set the right # of
 *                                  stairs for the level.
 */
static bool _fixup_stone_stairs(bool preserve_vault_stairs,
                                bool checking_up_stairs)
{
    list<coord_def> stairs = _find_stone_stairs(checking_up_stairs);

    unsigned int needed_stairs;
    dungeon_feature_type base;
    dungeon_feature_type replace;
    if (checking_up_stairs)
    {
        replace = DNGN_FLOOR;
        base = DNGN_STONE_STAIRS_UP_I;
        // Pan abuses stair placement for transits, as we want connectivity
        // checks.
        needed_stairs = (you.depth == 1 || player_in_hell())
                        && !player_in_branch(BRANCH_PANDEMONIUM)
                        ? 1 : 3;
    }
    else
    {
        replace = DNGN_FLOOR;
        base = DNGN_STONE_STAIRS_DOWN_I;

        if (at_branch_bottom())
            needed_stairs = 0;
        else if (player_in_hell())
            needed_stairs = 1;
        else
            needed_stairs = 3;
    }

    // In Zot, don't create extra escape hatches, in order to force
    // the player through vaults that use all three down stone stairs.
    // In Hell, don't create extra hatches, the levels are small already.
    if (player_in_branch(BRANCH_ZOT) || player_in_hell())
    {
        if (player_in_branch(BRANCH_GEHENNA) || player_in_branch(BRANCH_TARTARUS))
            replace = random_choose(DNGN_FOUNTAIN_BLOOD, DNGN_DRY_FOUNTAIN);
        else
        {
            replace = random_choose(DNGN_FOUNTAIN_BLUE, DNGN_FOUNTAIN_SPARKLING,
                                    DNGN_FOUNTAIN_BLOOD);
        }
    }

    dprf(DIAG_DNGN, "Before culling: %d/%d %s stairs",
         (int)stairs.size(), needed_stairs, checking_up_stairs ? "up" : "down");

    // Find pairwise stairs that are connected and turn one of them
    // into an escape hatch of the appropriate type.
    if (stairs.size() > needed_stairs)
    {
        _cull_redundant_stairs(stairs, needed_stairs,
                               preserve_vault_stairs, replace);
    }

    // If that doesn't work, remove random stairs.
    if (stairs.size() > needed_stairs)
    {
        _cull_random_stairs(stairs, needed_stairs,
                            preserve_vault_stairs, replace);
    }

    // FIXME: stairs that generate inside random vaults are still
    // protected, resulting in superfluous ones.
    dprf(DIAG_DNGN, "After culling: %d/%d %s stairs",
         (int)stairs.size(), needed_stairs, checking_up_stairs ? "up" : "down");

    // XXX: this logic is exceptionally shady & should be reviewed
    if (stairs.size() > needed_stairs && preserve_vault_stairs
        && (!checking_up_stairs || you.depth != 1
            || !player_in_branch(root_branch)))
    {
        return false;
    }

    // If there are no stairs, it's either a branch entrance or exit.
    // If we somehow have ended up in a catastrophic "no stairs" state,
    // the level will not be validated, so we do not need to catch it here.
    if (stairs.size() == 0)
        return true;

    // Add extra stairs to get to exactly three.
    for (unsigned int s = stairs.size(); s < needed_stairs; s++)
    {
        const uint32_t mask = preserve_vault_stairs ? MMT_VAULT : 0;
        const coord_def gc
            = _dgn_random_point_in_bounds(DNGN_FLOOR, mask, DNGN_UNSEEN);

        if (gc.origin())
            return false;

        dprf(DIAG_DNGN, "Adding stair %d at (%d,%d)", s, gc.x, gc.y);
        // base gets fixed up to be the right stone stair below...
        _set_grd(gc, base);
        stairs.push_back(gc);
    }

    // If we only need one stone stair, make sure it's _I.
    if (needed_stairs != 3)
    {
        ASSERT(checking_up_stairs || player_in_hell());
        ASSERT(needed_stairs == 1);
        ASSERT(stairs.size() == 1 || player_in_branch(root_branch));
        if (stairs.size() == 1)
        {
            env.grid(stairs.front()) =
                checking_up_stairs ? DNGN_STONE_STAIRS_UP_I
                                   : DNGN_STONE_STAIRS_DOWN_I;
        }

        return true;
    }

    // Ensure uniqueness of three stairs.
    ASSERT(needed_stairs == 3);
    for (size_t s = 0; s < needed_stairs + 1; s++)
    {
        const coord_def s1_loc = stairs.front();
        const coord_def s2_loc = stairs.back();
        if (env.grid(s1_loc) == env.grid(s2_loc))
        {
            _set_grd(s2_loc, (dungeon_feature_type)
                     (base + (env.grid(s2_loc)-base+1) % needed_stairs));
        }

        stairs.push_back(stairs.front());
        stairs.pop_front();
    }

    return true;
}

static bool _fixup_stone_stairs(bool preserve_vault_stairs)
{
    // This function ensures that there is exactly one each up and down
    // stone stairs I, II, and III. More than three stairs will result in
    // turning additional stairs into escape hatches (with an attempt to keep
    // level connectivity). Fewer than three stone stairs will result in
    // random placement of new stairs.
    const bool upstairs_fixed = _fixup_stone_stairs(preserve_vault_stairs,
                                                    true);
    const bool downstairs_fixed = _fixup_stone_stairs(preserve_vault_stairs,
                                                      false);
    return upstairs_fixed && downstairs_fixed;
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
                || travel_point_distance[x][y] // already covered previously
                || !_dgn_square_is_passable(gc))
            {
                continue;
            }

            if (_dgn_fill_zone(gc, ++nzones, _dgn_point_record_stub,
                               _dgn_square_is_passable, iswanted))
            {
                continue;
            }

            bool found_feature = false;
            for (rectangle_iterator ri(0); ri; ++ri)
            {
                if (env.grid(*ri) == feat
                    && travel_point_distance[ri->x][ri->y] == nzones)
                {
                    found_feature = true;
                    break;
                }
            }

            if (found_feature)
                continue;

            int i = 0;
            while (i++ < 2000)
            {
                coord_def rnd;
                rnd.x = random2(GXM);
                rnd.y = random2(GYM);
                if (env.grid(rnd) != DNGN_FLOOR)
                    continue;

                if (travel_point_distance[rnd.x][rnd.y] != nzones)
                    continue;

                _set_grd(rnd, feat);
                found_feature = true;
                break;
            }

            if (found_feature)
                continue;

            for (rectangle_iterator ri(0); ri; ++ri)
            {
                if (env.grid(*ri) != DNGN_FLOOR)
                    continue;

                if (travel_point_distance[ri->x][ri->y] != nzones)
                    continue;

                _set_grd(*ri, feat);
                found_feature = true;
                break;
            }

            if (found_feature)
                continue;

#ifdef DEBUG_DIAGNOSTICS
            dump_map("debug.map", true, true);
#endif
            // [ds] Too many normal cases trigger this ASSERT, including
            // rivers that surround a stair with deep water.
            // die("Couldn't find region.");
            return false;
        }

    return true;
}

static bool _add_connecting_escape_hatches()
{
    // For any regions without a down stone stair case, add an
    // escape hatch. This will always allow (downward) progress.

    if (branches[you.where_are_you].branch_flags & brflag::islanded)
        return true;

    // Veto D:1 or Pan if there are disconnected areas.
    // Veto any  non-abyss descent level with disconnected areas
    if (player_in_branch(BRANCH_PANDEMONIUM)
        || (player_in_branch(BRANCH_DUNGEON) && you.depth == 1)
        || (crawl_state.game_is_descent() && !player_in_branch(BRANCH_ABYSS)))
    {
        // Allow == 0 in case the entire level is one opaque vault.
        return dgn_count_disconnected_zones(false) <= 1;
    }

    if (!player_in_connected_branch())
        return true;

    if (at_branch_bottom())
        return dgn_count_disconnected_zones(true) == 0;

    if (!_add_feat_if_missing(_is_perm_down_stair, DNGN_ESCAPE_HATCH_DOWN))
        return false;

    // FIXME: shouldn't depend on branch.
    if (!player_in_branch(BRANCH_ORC))
        return true;

    return _add_feat_if_missing(_is_upwards_exit_stair, DNGN_ESCAPE_HATCH_UP);
}

static bool _branch_entrances_are_connected()
{
    // Returns true if all branch entrances on the level are connected to
    // stone stairs.
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (env.grid(*ri) == DNGN_ENTER_HELL)
            continue;
        if (!feat_is_branch_entrance(env.grid(*ri)))
            continue;
        if (!_has_connected_stone_stairs_from(*ri))
            return false;
    }

    return true;
}

static bool _branch_needs_stairs()
{
    // Irrelevant for branches with a single level and all encompass maps.
    return !player_in_branch(BRANCH_ZIGGURAT)
        && !player_in_branch(BRANCH_TOMB);
}

static void _dgn_verify_connectivity(unsigned nvaults)
{
    // After placing vaults, make sure parts of the level have not been
    // disconnected.
    if (dgn_zones && nvaults != env.level_vaults.size())
    {
        if (!player_in_branch(BRANCH_ABYSS))
            _fill_small_disconnected_zones();

        const int newzones = dgn_count_disconnected_zones(false);

#ifdef DEBUG_STATISTICS
        ostringstream vlist;
        for (unsigned i = nvaults; i < env.level_vaults.size(); ++i)
        {
            if (i > nvaults)
                vlist << ", ";
            vlist << env.level_vaults[i]->map.name;
        }
        dprf(DIAG_DNGN, "Dungeon has %d zones after placing %s.",
             newzones, vlist.str().c_str());
#endif
        if (newzones > dgn_zones)
        {
            throw dgn_veto_exception(make_stringf(
                 "Had %d zones, now has %d%s%s.", dgn_zones, newzones,
#ifdef DEBUG_STATISTICS
                 "; broken by ", vlist.str().c_str()
#else
                 "", ""
#endif
            ));
        }
    }

    // Also check for isolated regions that have no stairs.
    if (player_in_connected_branch()
        && !(branches[you.where_are_you].branch_flags & brflag::islanded)
        && dgn_count_disconnected_zones(true) > 0)
    {
        throw dgn_veto_exception("Isolated areas with no stairs.");
    }

    if (_branch_needs_stairs() && !_fixup_stone_stairs(true))
    {
        dprf(DIAG_DNGN, "Warning: failed to preserve vault stairs.");
        if (!_fixup_stone_stairs(false))
            throw dgn_veto_exception("Failed to fix stone stairs.");
    }

    if (!_branch_entrances_are_connected())
        throw dgn_veto_exception("A disconnected branch entrance.");

    if (!_add_connecting_escape_hatches())
        throw dgn_veto_exception("Failed to add connecting escape hatches.");

    // XXX: Interlevel connectivity fixup relies on being the last
    //      point at which a level may be vetoed.
    if (!_fixup_interlevel_connectivity())
        throw dgn_veto_exception("Failed to ensure interlevel connectivity.");
}

// Structure of OVERFLOW_TEMPLES:
//
// * A vector, with one cell per dungeon level (unset if there's no
//   overflow temples on that level).
//
// * The cell of the previous vector is a vector, with one overflow
//   temple definition per cell.
//
// * The cell of the previous vector is a hash table, containing the
//   list of gods for the overflow temple and (optionally) the name of
//   the vault to use for the temple. If no map name is supplied,
//   it will randomly pick from vaults tagged "temple_overflow_num",
//   where "num" is the number of gods in the temple. Gods are listed
//   in the order their altars are placed.
static void _build_overflow_temples()
{
    // Levels built while in testing mode.
    if (!you.props.exists(OVERFLOW_TEMPLES_KEY))
        return;

    CrawlVector &levels = you.props[OVERFLOW_TEMPLES_KEY].get_vector();

    // Are we deeper than the last overflow temple?
    if (you.depth >= levels.size() + 1)
        return;

    CrawlStoreValue &val = levels[you.depth - 1];

    // Does this level have an overflow temple?
    if (val.get_flags() & SFLAG_UNSET)
        return;

    CrawlVector &temples = val.get_vector();

    if (temples.empty())
        return;

    for (unsigned int i = 0; i < temples.size(); i++)
    {
        CrawlHashTable &temple = temples[i].get_table();

        const int num_gods = _setup_temple_altars(temple);

        const map_def *vault = nullptr;
        string vault_tag = "";
        string name = "";

        if (temple.exists(TEMPLE_MAP_KEY))
        {
            name = temple[TEMPLE_MAP_KEY].get_string();

            vault = find_map_by_name(name);
            if (vault == nullptr)
            {
                mprf(MSGCH_ERROR,
                     "Couldn't find overflow temple map '%s'!",
                     name.c_str());
            }
        }
        else
        {
            // First try to find a temple specialized for this combination of
            // gods.
            if (num_gods > 1 || coinflip())
            {
                vault_tag = make_stringf("temple_overflow_%d", num_gods);

                CrawlVector &god_vec = temple[TEMPLE_GODS_KEY];

                for (int j = 0; j < num_gods; j++)
                {
                    god_type god = (god_type) god_vec[j].get_byte();

                    name = god_name(god);
                    name = replace_all(name, " ", "_");
                    lowercase(name);

                    vault_tag = vault_tag + " temple_overflow_" + name;
                }

                if (num_gods == 1
                    && get_uniq_map_tags().find("uniq_altar_" + name)
                       != get_uniq_map_tags().end())
                {
                    // We've already placed a specialized temple for this
                    // god, so do nothing.
#ifdef DEBUG_TEMPLES
                    mprf(MSGCH_DIAGNOSTICS, "Already placed specialized "
                         "single-altar temple for %s", name.c_str());
#endif
                    continue;
                }

                vault = random_map_for_tag(vault_tag, true);
#ifdef DEBUG_TEMPLES
                if (vault == nullptr)
                {
                    mprf(MSGCH_DIAGNOSTICS, "Couldn't find overflow temple "
                         "for combination of tags %s", vault_tag.c_str());
                }
#endif
            }

            if (vault == nullptr)
            {
                vault_tag = make_stringf("temple_overflow_generic_%d",
                                         num_gods);

                vault = random_map_for_tag(vault_tag, true);
                if (vault == nullptr)
                {
                    mprf(MSGCH_ERROR,
                         "Couldn't find overflow temple tag '%s'!",
                         vault_tag.c_str());
                }
            }
        }

        if (vault == nullptr)
            // Might as well build the rest of the level if we couldn't
            // find the overflow temple map, so don't veto the level.
            return;

        {
            dgn_map_parameters mp(vault_tag);
            if (!_dgn_ensure_vault_placed(
                    _build_secondary_vault(vault),
                    false, vault->name))
            {
#ifdef DEBUG_TEMPLES
                mprf(MSGCH_DIAGNOSTICS, "Couldn't place overflow temple '%s', "
                     "vetoing level.", vault->name.c_str());
#endif
                return;
            }
        }
#ifdef DEBUG_TEMPLES
        mprf(MSGCH_DIAGNOSTICS, "Placed overflow temple %s",
             vault->name.c_str());
#endif
    }
    _current_temple_hash = nullptr; // XXX: hack!
}

struct coord_feat
{
    coord_def pos;
    dungeon_feature_type feat;
    terrain_property_t prop;
    unsigned int mask;

    coord_feat(const coord_def &c, dungeon_feature_type f)
        : pos(c), feat(f), prop(), mask(0)
    {
    }

    void set_from(const coord_def &c)
    {
        feat = env.grid(c);
        // Don't copy mimic-ness.
        mask = env.level_map_mask(c) & ~(MMT_MIMIC);
        // Only copy "static" properties.
        prop = env.pgrid(c) & (FPROP_NO_CLOUD_GEN | FPROP_NO_TELE_INTO
                               | FPROP_NO_TIDE);
    }
};

template <typename Iterator>
static void _ruin_level(Iterator iter,
                        unsigned vault_mask = MMT_VAULT,
                        int ruination = 10,
                        int plant_density = 5,
                        int iterations = 1)
{
    vector<coord_def> replaced;

    for (int i = 0; i < iterations; ++i)
    {
        typedef vector<coord_feat> coord_feats;
        coord_feats to_replace;
        for (Iterator ri = iter; ri; ++ri)
        {
            // don't alter map boundary
            if (!in_bounds(*ri))
                continue;

            // only try to replace wall and door tiles
            if (!feat_is_wall(env.grid(*ri)) && !feat_is_door(env.grid(*ri)))
                continue;

            // don't mess with permarock
            if (env.grid(*ri) == DNGN_PERMAROCK_WALL)
                continue;

            // or vaults
            if (map_masked(*ri, vault_mask))
                continue;

            // Pick a random adjacent non-wall, non-door, non-statue
            // feature, and count the number of such features.
            coord_feat replacement(*ri, DNGN_UNSEEN);
            int floor_count = 0;
            for (adjacent_iterator ai(*ri); ai; ++ai)
            {
                if (!feat_is_wall(env.grid(*ai)) && !feat_is_door(env.grid(*ai))
                    && !feat_is_statuelike(env.grid(*ai))
                    // Shouldn't happen, but just in case.
                    && env.grid(*ai) != DNGN_MALIGN_GATEWAY)
                {
                    if (one_chance_in(++floor_count))
                        replacement.set_from(*ai);
                }
            }

            // chance of removing the tile is dependent on the number of adjacent
            // floor(ish) tiles
            if (x_chance_in_y(floor_count, ruination))
                to_replace.push_back(replacement);
        }

        for (const auto &cfeat : to_replace)
        {
            const coord_def &p(cfeat.pos);
            replaced.push_back(p);
            dungeon_feature_type replacement = cfeat.feat;
            ASSERT(replacement != DNGN_UNSEEN);

            // Don't replace doors with impassable features.
            if (feat_is_door(env.grid(p)))
            {
                if (feat_is_water(replacement))
                    replacement = DNGN_SHALLOW_WATER;
                else
                    replacement = DNGN_FLOOR;
            }
            else if (feat_has_solid_floor(replacement)
                    && replacement != DNGN_SHALLOW_WATER)
            {
                // Exclude traps, shops, stairs, portals, altars, fountains.
                // The first four, especially, are a big deal.
                replacement = DNGN_FLOOR;
            }

            // only remove some doors, to preserve tactical options
            if (feat_is_wall(env.grid(p)) || coinflip() && feat_is_door(env.grid(p)))
            {
                // Copy the mask and properties too, so that we don't make an
                // isolated transparent or rtele_into square.
                env.level_map_mask(p) |= cfeat.mask;
                env.pgrid(p) |= cfeat.prop;
                _set_grd(p, replacement);
            }

            // but remove doors if we've removed all adjacent walls
            for (adjacent_iterator wai(p); wai; ++wai)
            {
                if (feat_is_door(env.grid(*wai)))
                {
                    bool remove = true;
                    for (adjacent_iterator dai(*wai); dai; ++dai)
                    {
                        if (feat_is_wall(env.grid(*dai)))
                            remove = false;
                    }
                    // It's always safe to replace a door with floor.
                    if (remove)
                    {
                        env.level_map_mask(p) |= cfeat.mask;
                        env.pgrid(p) |= cfeat.prop;
                        _set_grd(*wai, DNGN_FLOOR);
                    }
                }
            }
        }
    }

    for (const auto &p : replaced)
    {
        // replace some ruined walls with plants/fungi/bushes
        if (plant_density && one_chance_in(plant_density)
            && feat_has_solid_floor(env.grid(p))
            && !plant_forbidden_at(p))
        {
            mgen_data mg;
            mg.cls = random_choose_weighted( 2, MONS_BUSH,
                                            19, MONS_PLANT,
                                            19, MONS_FUNGUS);
            mg.pos = p;
            mg.flags = MG_FORCE_PLACE;
            mons_place(mgen_data(mg));
        }
    }
}

static bool _mimic_at_level()
{
    return !player_in_branch(BRANCH_TEMPLE)
           && !player_in_branch(BRANCH_VESTIBULE)
           && !player_in_branch(BRANCH_SLIME)
           && !player_in_branch(BRANCH_TOMB)
           && !player_in_branch(BRANCH_PANDEMONIUM)
           && !player_in_hell()
           && !crawl_state.game_is_tutorial();
}

static void _place_feature_mimics()
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        const coord_def pos = *ri;
        const dungeon_feature_type feat = env.grid(pos);

        // Vault tag prevents mimic.
        if (map_masked(pos, MMT_NO_MIMIC))
            continue;

        // Only features valid for mimicing.
        if (!feat_is_mimicable(feat))
            continue;

        if (one_chance_in(FEATURE_MIMIC_CHANCE))
        {
            dprf(DIAG_DNGN, "Placed %s mimic at (%d,%d).",
                 feat_type_name(feat), ri->x, ri->y);
            env.level_map_mask(*ri) |= MMT_MIMIC;

            // If we're mimicing a unique portal vault, give a chance for
            // another one to spawn.
            const char* dst = branches[stair_destination(pos).branch].abbrevname;
            const string tag = "uniq_" + lowercase_string(dst);
            if (get_uniq_map_tags().count(tag))
                get_uniq_map_tags().erase(tag);
        }
    }
}

// Apply modifications (ruination, plant clumps) that should happen
// regardless of game mode.
static void _post_vault_build()
{
    if (player_in_branch(BRANCH_LAIR))
    {
        int depth = you.depth + 1;
        _ruin_level(rectangle_iterator(1), MMT_VAULT,
                    20 - depth, depth / 2 + 4, 1 + (depth / 3));
        do
        {
            _add_plant_clumps(12 - depth, 18 - depth / 4, depth / 4 + 2);
            depth -= 3;
        } while (depth > 0);
    }
}

static void _build_dungeon_level()
{
    bool place_vaults = _builder_by_type();

    if (player_in_branch(BRANCH_SLIME))
        _slime_connectivity_fixup();

    // Now place items, mons, gates, etc.
    // Stairs must exist by this point (except in Shoals where they are
    // yet to be placed). Some items and monsters already exist.

    _check_doors();

    const unsigned nvaults = env.level_vaults.size();

    // Any further vaults must make sure not to disrupt level layout.
    dgn_check_connectivity = true;

    if (player_in_branch(BRANCH_DUNGEON)
        && !crawl_state.game_is_tutorial())
    {
        _build_overflow_temples();
    }

    // Try to place minivaults that really badly want to be placed. Still
    // no guarantees, seeing this is a minivault.
    if (crawl_state.game_has_random_floors())
    {
        if (place_vaults)
        {
            // Moved branch entries to place first so there's a good
            // chance of having room for a vault
            _place_branch_entrances(true);
            _place_chance_vaults();
            _place_minivaults();
            _place_extra_vaults();
        }
        else
        {
            // Place any branch entries vaultlessly
            _place_branch_entrances(false);
            // Still place chance vaults - important things like Abyss,
            // Hell, Pan entries are placed this way
            _place_chance_vaults();
        }

        // Ruination and plant clumps.
        _post_vault_build();

        // XXX: Moved this here from builder_monsters so that
        //      connectivity can be ensured
        _place_uniques();

        if (_mimic_at_level())
            _place_feature_mimics();

        _place_traps();

        // Any vault-placement activity must happen before this check.
        _dgn_verify_connectivity(nvaults);

        _builder_monsters();

        // Place items.
        _builder_items();

        _fixup_walls();
    }
    else
    {
        // Do ruination and plant clumps even in funny game modes, if
        // they happen to have the relevant branch.
        _post_vault_build();
    }

    // Translate stairs for pandemonium levels.
    if (player_in_branch(BRANCH_PANDEMONIUM))
        _fixup_pandemonium_stairs();

    _fixup_branch_stairs();

    if (!dgn_make_transporters_from_markers())
        throw dgn_veto_exception("Transporter placement failed.");

    fixup_misplaced_items();
    link_items();

    if (!player_in_branch(BRANCH_COCYTUS)
        && !player_in_branch(BRANCH_SWAMP)
        && !player_in_branch(BRANCH_SHOALS))
    {
        _prepare_water();
        if (player_in_branch(BRANCH_LAIR) || !one_chance_in(4))
            _prepare_water();
    }

    if (player_in_hell())
        _fixup_hell_stairs();

    if (crawl_state.game_is_descent())
    {
        _fixup_descent_hatches();
        _place_dungeon_exit();
    }
}

static void _dgn_set_floor_colours()
{
    colour_t old_floor_colour = env.floor_colour;
    colour_t old_rock_colour  = env.rock_colour;

    const int youbranch = you.where_are_you;
    env.floor_colour    = branches[youbranch].floor_colour;
    env.rock_colour     = branches[youbranch].rock_colour;

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
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (!feat_is_closed_door(env.grid(*ri)))
            continue;

        int solid_count = 0;

        for (orth_adjacent_iterator rai(*ri); rai; ++rai)
            if (cell_is_solid(*rai))
                solid_count++;

        if (solid_count < 2)
            _set_grd(*ri, DNGN_FLOOR);
    }
}

int count_feature_in_box(int x0, int y0, int x1, int y1,
                         dungeon_feature_type feat)
{
    int result = 0;
    for (int i = x0; i < x1; ++i)
        for (int j = y0; j < y1; ++j)
        {
            if (env.grid[i][j] == feat)
                ++result;
        }

    return result;
}

// Gives water which is next to ground/shallow water a chance of being
// shallow. Checks each water space.
static void _prepare_water()
{
    set<coord_def> fix_positions;
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (map_masked(*ri, MMT_NO_POOL) || env.grid(*ri) != DNGN_DEEP_WATER)
            continue;

        for (adjacent_iterator ai(*ri); ai; ++ai)
        {
            const dungeon_feature_type which_grid = env.grid(*ai);

            if (which_grid == DNGN_SHALLOW_WATER && one_chance_in(10)
                || feat_has_dry_floor(which_grid) && one_chance_in(5))
            {
                fix_positions.insert(*ri);
                break;
            }
        }
    }

    for (coord_def pos : fix_positions)
        _set_grd(pos, DNGN_SHALLOW_WATER);
}

static bool _vault_can_use_layout(const map_def *vault, const map_def *layout)
{
    if (!layout)
        return false;

    bool permissive = false;
    if (!vault->has_tag_prefix("layout_")
        && !(permissive = vault->has_tag_prefix("nolayout_")))
    {
        return true;
    }

    ASSERT(layout->has_tag_prefix("layout_type_"));

    // in principle, tag order can matter here, even though that is probably
    // a vault designer error
    for (const auto &tag : layout->get_tags())
    {
        if (starts_with(tag, "layout_type_"))
        {
            string type = tag_without_prefix(tag, "layout_type_");
            if (vault->has_tag("layout_" + type))
                return true;
            else if (vault->has_tag("nolayout_" + type))
                return false;
        }
    }

    return permissive;
}

static const map_def *_pick_layout(const map_def *vault)
{
    const map_def *layout = nullptr;

    // This is intended for use with primary vaults, so...
    ASSERT(vault);

    // For centred maps, try to pick a central-type layout first.
    if (vault->orient == MAP_CENTRE)
        layout = random_map_for_tag("central", true, true);

    int tries = 100;

    if (!layout)
    {
        do
        {
            if (!tries--)
            {
                die("Couldn't find a layout for %s",
                    level_id::current().describe().c_str());
            }
            layout = random_map_for_tag("layout", true, true);
        }
        while (layout && layout->has_tag("no_primary_vault")
               || (tries > 10 && !_vault_can_use_layout(vault, layout)));
    }

    return layout;
}

static bool _pan_level()
{
    const char *pandemon_level_names[] =
        { "mnoleg", "lom_lobon", "cerebov", "gloorx_vloq", };
    int which_demon = -1;
    const PlaceInfo &place_info = you.get_place_info();
    bool all_demons_generated = true;

    if (you.props.exists(FORCE_MAP_KEY))
    {
        const map_def *vault =
            find_map_by_name(you.props[FORCE_MAP_KEY].get_string());
        ASSERT(vault);

        _dgn_ensure_vault_placed(_build_primary_vault(vault), false, vault->name);
        return vault->orient != MAP_ENCOMPASS;
    }

    for (int i = 0; i < 4; i++)
    {
        if (!get_uniq_map_tags().count(string("uniq_") + pandemon_level_names[i]))
        {
            all_demons_generated = false;
            break;
        }
    }

    // Unique pan lords become more common as you travel through pandemonium.
    // On average it takes about 14 levels to see all four, and on average
    // about 5 levels to see your first.
    if (x_chance_in_y(1 + place_info.levels_seen, 20 + place_info.levels_seen)
        && !all_demons_generated)
    {
        do
        {
            which_demon = random2(4);
        }
        while (get_uniq_map_tags().count(string("uniq_")
                                       + pandemon_level_names[which_demon]));
    }

    const map_def *vault = nullptr;

    if (which_demon >= 0)
    {
        vault = random_map_for_tag(pandemon_level_names[which_demon], false,
                                   false, false);
    }
    else
        vault = random_map_in_depth(level_id::current(), false, false);

    // Every Pan level should have a primary vault.
    ASSERT(vault);
    _dgn_ensure_vault_placed(_build_primary_vault(vault), false, vault->name);
    return vault->orient != MAP_ENCOMPASS;
}

// Returns true if we want the dungeon builder
// to place more vaults after this
static bool _builder_by_type()
{
    if (player_in_branch(BRANCH_ABYSS))
    {
        generate_abyss();
        // Should place some vaults in abyss because
        // there's never an encompass vault
        return true;
    }
    else if (player_in_branch(BRANCH_PANDEMONIUM))
    {
        // Generate a random monster table for Pan.
        init_pandemonium();
        setup_vault_mon_list();
        return _pan_level();
    }
    else
        return _builder_normal();
}

static const map_def *_dgn_random_map_for_place(bool minivault)
{
    if (!minivault && player_in_branch(BRANCH_TEMPLE))
    {
        // Temple vault determined at new game time.
        const string name = you.props.exists(FORCE_MAP_KEY)
                ? you.props[FORCE_MAP_KEY]
                : you.props[TEMPLE_MAP_KEY];

        if (name != you.props[TEMPLE_MAP_KEY].get_string())
            mprf(MSGCH_ERROR, "Overriding seed-determined temple map.");

        // Tolerate this for a little while, for old games.
        if (!name.empty())
        {
            const map_def *vault = find_map_by_name(name);

            if (vault)
                return vault;

            mprf(MSGCH_ERROR, "Unable to find Temple vault '%s'",
                 name.c_str());

            // Fall through and use a different Temple map instead.
        }
    }

    const level_id lid = level_id::current();

#if TAG_MAJOR_VERSION == 34
    if (!minivault
        && player_in_branch(BRANCH_TOMB)
        && you.props[TOMB_STONE_STAIRS_KEY])
    {
        const map_def *vault = random_map_for_tag("tomb_stone_stairs", true);

        if (vault)
            return vault;

        end(1, false, "Couldn't find map with tag tomb_stone_stairs for level "
            "%s.", lid.describe().c_str());
    }
#endif

    const map_def *vault = 0;

    if (you.props.exists(FORCE_MAP_KEY))
        vault = find_map_by_name(you.props[FORCE_MAP_KEY].get_string());
    else if (lid.branch == root_branch && lid.depth == 1
        && (crawl_state.game_is_sprint()
            || crawl_state.game_is_tutorial()))
    {
        vault = find_map_by_name(crawl_state.map);
        if (vault == nullptr)
        {
            end(1, false, "Couldn't find selected map '%s'.",
                crawl_state.map.c_str());
        }
    }

    if (!vault)
        // Pick a normal map
        vault = random_map_for_place(lid, minivault, false);

    if (!vault && lid.branch == root_branch && lid.depth == 1)
        vault = random_map_for_tag("arrival", false, false, false);

    return vault;
}

static int _setup_temple_altars(CrawlHashTable &temple)
{
    _current_temple_hash = &temple; // XXX: hack!

    CrawlVector god_list = temple[TEMPLE_GODS_KEY].get_vector();

    _temple_altar_list.clear();

    for (unsigned int i = 0; i < god_list.size(); i++)
        _temple_altar_list.push_back((god_type) god_list[i].get_byte());

    return (int)god_list.size();
}

struct map_component
{
    int label;

    map_component()
    {
        min_equivalent = nullptr;
    }
    map_component * min_equivalent;

    coord_def min_coord;
    coord_def max_coord;

    coord_def seed_position;

    void start_component(const coord_def & pos, int in_label)
    {
        seed_position = pos;
        min_coord = pos;
        max_coord = pos;

        label = in_label;
        min_equivalent = nullptr;
    }

    void add_coord(const coord_def & pos)
    {
        if (pos.x < min_coord.x)
            min_coord.x = pos.x;
        if (pos.x > max_coord.x)
            max_coord.x = pos.x;
        if (pos.y < min_coord.y)
            min_coord.y = pos.y;
        if (pos.y > max_coord.y)
            max_coord.y = pos.y;
    }

    void merge_region(const map_component & existing)
    {
        add_coord(existing.min_coord);
        add_coord(existing.max_coord);
    }
};

static int _min_transitive_label(map_component & component)
{
    map_component * current = &component;

    int label;
    do
    {
        label = current->label;

        current = current->min_equivalent;
    } while (current);

    return label;
}

// 8-way connected component analysis on the current level map.
template<typename comp>
static void _ccomps_8(FixedArray<int, GXM, GYM > & connectivity_map,
                      vector<map_component> & components, comp & connected)
{
    map<int, map_component> intermediate_components;

    connectivity_map.init(0);
    components.clear();

    unsigned adjacent_size = 4;
    coord_def offsets[4] = {coord_def(-1, 0), coord_def(-1, -1), coord_def(0, -1), coord_def(1, -1)};

    int next_label = 1;

    // Pass 1, for each point, check the upper/left adjacent squares for labels
    // if a labels are found, use the min connected label, else assign a new
    // label and start a new component
    for (rectangle_iterator pos(1); pos; ++pos)
    {
        if (connected(*pos))
        {
            int absolute_min_label = INT_MAX;
            set<int> neighbor_labels;
            for (unsigned i = 0; i < adjacent_size; i++)
            {
                coord_def test = *pos + offsets[i];
                if (in_bounds(test) && connectivity_map(test) != 0)
                {
                    int neighbor_label = connectivity_map(test);
                    if (neighbor_labels.insert(neighbor_label).second)
                    {
                        int trans = _min_transitive_label(intermediate_components[neighbor_label]);

                        if (trans < absolute_min_label)
                            absolute_min_label = trans;
                    }
                }
            }

            int label;
            if (neighbor_labels.empty())
            {
                intermediate_components[next_label].start_component(*pos, next_label);
                label = next_label;
                next_label++;
            }
            else
            {
                label = absolute_min_label;
                map_component * absolute_min = &intermediate_components[absolute_min_label];

                absolute_min->add_coord(*pos);
                for (auto i : neighbor_labels)
                {
                    map_component * current = &intermediate_components[i];

                    while (current && current != absolute_min)
                    {
                        absolute_min->merge_region(*current);
                        map_component * next = current->min_equivalent;
                        current->min_equivalent = absolute_min;
                        current = next;
                    }
                }
            }
            connectivity_map(*pos) = label;
        }
    }

    int reindexed_label = 1;
    // Reindex root labels, and move them to output
    for (auto &entry : intermediate_components)
    {
        if (entry.second.min_equivalent == nullptr)
        {
            entry.second.label = reindexed_label++;
            components.push_back(entry.second);
        }
    }

    // Pass 2, mark new labels on the grid
    for (rectangle_iterator pos(1); pos; ++pos)
    {
        int label = connectivity_map(*pos);
        if (label  != 0)
            connectivity_map(*pos) = _min_transitive_label(intermediate_components[label]);
    }
}

// Is this square a wall, or does it belong to a vault? both are considered to
// block connectivity.
static bool _passable_square(const coord_def & pos)
{
    return !feat_is_wall(env.grid(pos)) && !(env.level_map_mask(pos) & MMT_VAULT);
}

struct adjacency_test
{
    adjacency_test()
    {
        adjacency.init(0);
    }
    FixedArray<int, GXM, GYM> adjacency;
    bool operator()(const coord_def & pos)
    {
        return _passable_square(pos) && adjacency(pos) == 0;
    }
};

struct dummy_estimate
{
    bool operator() (const coord_def &)
    {
        return 0;
    }
};

struct adjacent_costs
{
    FixedArray<int, GXM, GYM> * adjacency;
    int operator()(const coord_def & pos)
    {
        return (*adjacency)(pos);
    }
};

struct label_match
{
    FixedArray<int, GXM, GYM> * labels;
    int target_label;
    bool operator()(const coord_def & pos)
    {
        return (*labels)(pos) == target_label;
    }
};

// Connectivity checks to make sure that the parts of a bubble are not
// obstructed by slime wall adjacent squares
static void _slime_connectivity_fixup()
{
    // Generate a connectivity map considering any non wall, non vault square
    // passable
    FixedArray<int, GXM, GYM> connectivity_map;
    vector<map_component> components;
    _ccomps_8(connectivity_map, components, _passable_square);

    // Next we will generate a connectivity map with the above restrictions,
    // and also considering wall adjacent squares unpassable. But first we
    // build a map of how many walls are adjacent to each square in the level.
    // Walls/vault squares are flagged with DISCONNECT_DIST in this map.
    // This will be used to build the connectivity map, then later the adjacent
    // counts will define the costs of a search used to connect components in
    // the basic connectivity map that are broken apart in the restricted map
    FixedArray<int, GXM, GYM> non_adjacent_connectivity;
    vector<map_component> non_adj_components;
    adjacency_test adjacent_check;

    for (rectangle_iterator ri(1); ri; ++ri)
    {
        int count = 0;
        if (!_passable_square(*ri))
            count = DISCONNECT_DIST;
        else
        {
            for (adjacent_iterator adj(*ri); adj; ++adj)
            {
                if (feat_is_wall(env.grid(*adj)))
                {
                    // Not allowed to damage vault squares, so mark them
                    // inaccessible
                    if (env.level_map_mask(*adj) & MMT_VAULT)
                    {
                        count = DISCONNECT_DIST;
                        break;
                    }
                    else
                        count++;
                }

            }
        }
        adjacent_check.adjacency(*ri) = count;
    }

    _ccomps_8(non_adjacent_connectivity, non_adj_components, adjacent_check);

    // Now that we have both connectivity maps, go over each component in the
    // unrestricted map and connect any separate components in the restricted
    // map that it was broken up into.
    for (map_component &comp : components)
    {
        // Collect the components in the restricted connectivity map that
        // occupy part of the current component
        map<int, map_component *> present;
        for (rectangle_iterator ri(comp.min_coord, comp.max_coord); ri; ++ri)
        {
            int new_label = non_adjacent_connectivity(*ri);
            if (comp.label == connectivity_map(*ri) && new_label != 0)
            {
                // the bit with new_label - 1 is foolish.
                present[new_label] = &non_adj_components[new_label-1];
            }
        }

        // Set one restricted component as the base point, and search to all
        // other restricted components
        auto target_components = present.begin();

        // No non-wall adjacent squares in this component? This probably
        // shouldn't happen, but just move on.
        if (target_components == present.end())
            continue;

        map_component * base_component = target_components->second;
        ++target_components;

        adjacent_costs connection_costs;
        connection_costs.adjacency = &adjacent_check.adjacency;

        label_match valid_label;
        valid_label.labels = &non_adjacent_connectivity;

        dummy_estimate dummy;

        // Now search from our base component to the other components, and
        // clear out the path found
        for ( ; target_components != present.end(); ++target_components)
        {
            valid_label.target_label = target_components->second->label;

            vector<set<position_node>::iterator >path;
            set<position_node> visited;
            search_astar(base_component->seed_position, valid_label,
                         connection_costs, dummy, visited, path);

            // Did the search, now remove any walls adjacent to squares in
            // the path.
            if (!path.size())
                continue;
            const position_node * current = &(*path[0]);

            while (current)
            {
                if (adjacent_check.adjacency(current->pos) > 0)
                {
                    for (adjacent_iterator adj_it(current->pos); adj_it; ++adj_it)
                    {
                        if (feat_is_wall(env.grid(*adj_it)))
                        {
                            // This shouldn't happen since vault adjacent
                            // squares should have adjacency of DISCONNECT_DIST
                            // but oh well
                            if (env.level_map_mask(*adj_it) & MMT_VAULT)
                                mpr("Whoops, nicked a vault in slime connectivity fixup");
                            env.grid(*adj_it) = DNGN_FLOOR;
                        }
                    }
                    adjacent_check.adjacency(current->pos) = 0;
                }
                current = current->last;
            }

        }

    }
}

// Place vaults with CHANCE: that want to be placed on this level.
static void _place_chance_vaults()
{
    const level_id &lid(level_id::current());
    mapref_vector maps = random_chance_maps_in_depth(lid);
    // [ds] If there are multiple CHANCE maps that share an luniq_ or
    // uniq_ tag, only the first such map will be placed. Shuffle the
    // order of chosen maps so we don't have a first-map bias.
    shuffle_array(maps);
    for (const map_def *map : maps)
    {
        bool check_fallback = true;
        if (!map->map_already_used())
        {
            dprf(DIAG_DNGN, "Placing CHANCE vault: %s (%s)",
                 map->name.c_str(), map->chance(lid).describe().c_str());
            check_fallback = !_build_secondary_vault(map);
        }
        if (check_fallback)
        {
            const string chance_tag = vault_chance_tag(*map);
            if (!chance_tag.empty())
            {
                const string fallback_tag =
                    "fallback_" + chance_tag.substr(7); // "chance_"
                const map_def *fallback =
                    random_map_for_tag(fallback_tag, true, false, false);
                if (fallback)
                {
                    dprf(DIAG_DNGN, "Found fallback vault %s for chance tag %s",
                         fallback->name.c_str(), chance_tag.c_str());
                    _build_secondary_vault(fallback);
                }
            }
        }
    }
}

static void _place_minivaults()
{
    const map_def *vault = nullptr;
    // First place the vault requested with &P
    if (you.props.exists(FORCE_MINIVAULT_KEY)
        && (vault = find_map_by_name(you.props[FORCE_MINIVAULT_KEY])))
    {
        _dgn_ensure_vault_placed(_build_secondary_vault(vault), false, vault->name);
    }
    // Always try to place PLACE:X minivaults.
    if ((vault = random_map_for_place(level_id::current(), true)))
        _build_secondary_vault(vault);

    if (use_random_maps)
    {
        int tries = 0;
        do
        {
            vault = random_map_in_depth(level_id::current(), true);
            if (vault)
                _build_secondary_vault(vault);
        } // if ALL maps eligible are "extra" but fail to place, we'd be screwed
        while (vault && vault->is_extra_vault() && tries++ < 10000);
    }
}

static bool _builder_normal()
{
    const map_def *vault = _dgn_random_map_for_place(false);

    if (vault)
    {
        // TODO: figure out a good way to do this only in Temple
        dgn_map_parameters mp(
            you.props.exists(TEMPLE_SIZE_KEY)
            ? make_stringf("temple_altars_%d",
                           you.props[TEMPLE_SIZE_KEY].get_int())
            : "");
        env.level_build_method += " random_map_for_place";
        _ensure_vault_placed_ex(_build_primary_vault(vault), vault);
        // Only place subsequent random vaults on non-encompass maps
        // and not at the branch end
        return vault->orient != MAP_ENCOMPASS;
    }

    if (use_random_maps)
        vault = random_map_in_depth(level_id::current(), false, false);

    if (vault)
    {
        env.level_build_method += " random_map_in_depth";
        _ensure_vault_placed_ex(_build_primary_vault(vault), vault);
        // Only place subsequent random vaults on non-encompass maps
        // and not at the branch end
        return vault->orient != MAP_ENCOMPASS;
    }

    vault = random_map_for_tag("layout", true, true);

    if (!vault)
        die("Couldn't pick a layout.");

    _dgn_ensure_vault_placed(_build_primary_vault(vault), false, vault->name);
    return true;
}

static void _place_traps()
{

    int num_traps = random2avg(2 * trap_rate_for_place(), 2);

    // Snake and Vaults don't have a lot of unique terrain types or open
    // themes compared to their adjacent branches, and have themed weaker
    // trap options to fall back on, so they get extra traps.
    if (player_in_branch(BRANCH_SNAKE))
        num_traps += 2;
    else if (player_in_branch(BRANCH_VAULTS))
        num_traps += 1;

    ASSERT(num_traps >= 0);
    dprf("attempting to place %d traps", num_traps);

    for (int i = 0; i < num_traps; i++)
    {
        trap_def ts;

        int tries;
        for (tries = 0; tries < 200; ++tries)
        {
            ts.pos.x = random2(GXM);
            ts.pos.y = random2(GYM);
            // Don't place random traps under vault monsters; if a vault
            // wants this they have to request it specifically.
            if (in_bounds(ts.pos)
                && env.grid(ts.pos) == DNGN_FLOOR
                && !map_masked(ts.pos, MMT_NO_TRAP)
                && env.mgrid(ts.pos) == NON_MONSTER)
            {
                break;
            }
        }

        if (tries == 200)
        {
            dprf("tried %d times to place a trap & gave up", tries);
            break;
        }

        // Don't place dispersal traps in opaque vaults, they won't
        // be later checked for connectivity and we might break them.
        const trap_type type = random_trap_for_place(
                                   !map_masked(ts.pos, MMT_OPAQUE));
        if (type == NUM_TRAPS)
        {
            dprf("failed to find a trap type to place");
            continue;
        }

        ts.type = type;
        env.grid(ts.pos) = ts.feature();
        ts.prepare_ammo();
        env.trap[ts.pos] = ts;
        dprf("placed %s trap", article_a(trap_name(type)).c_str());
    }

    if (player_in_branch(BRANCH_SPIDER))
    {
        // Max webs ranges from around 35 (Spider:1) to 220 (Spider:5), actual
        // amount will be much lower.
        int max_webs = 35 * pow(2, (you.depth - 1) / 1.5) - num_traps;
        max_webs /= 2;
        place_webs(max_webs + random2(max_webs));
    }
}

// Create randomly-placed stone stairs.
void dgn_place_stone_stairs(bool maybe_place_hatches)
{
    const int stair_start = DNGN_STONE_STAIRS_DOWN_I;
    const int stair_count = DNGN_ESCAPE_HATCH_UP - stair_start + 1;

    FixedVector < bool, stair_count > existing;

    existing.init(false);

    for (rectangle_iterator ri(0); ri; ++ri)
        if (env.grid(*ri) >= stair_start && env.grid(*ri) < stair_start + stair_count)
            existing[env.grid(*ri) - stair_start] = true;

    int pair_count = 3;

    if (maybe_place_hatches && coinflip())
        pair_count++;

    for (int i = 0; i < pair_count; ++i)
    {
        // only place the first stair and hatches in hell
        if (player_in_hell() && i > 0 && i <= 3)
            continue;

        if (!existing[i])
        {
            _dgn_place_feature_at_random_floor_square(
                static_cast<dungeon_feature_type>(DNGN_STONE_STAIRS_DOWN_I + i));
        }

        if (!existing[DNGN_STONE_STAIRS_UP_I - stair_start + i])
        {
            _dgn_place_feature_at_random_floor_square(
                static_cast<dungeon_feature_type>(DNGN_STONE_STAIRS_UP_I + i));
        }
    }
}

bool dgn_has_adjacent_feat(coord_def c, dungeon_feature_type feat)
{
    for (adjacent_iterator ai(c); ai; ++ai)
        if (env.grid(*ai) == feat)
            return true;
    return false;
}

coord_def dgn_random_point_in_margin(int margin)
{
    coord_def res;
    res.x = random_range(margin, GXM - margin - 1);
    res.y = random_range(margin, GYM - margin - 1);
    return res;
}

static inline bool _point_matches_feat(coord_def c,
                                       dungeon_feature_type searchfeat,
                                       uint32_t mapmask,
                                       dungeon_feature_type adjacent_feat,
                                       bool monster_free)
{
    return env.grid(c) == searchfeat
           && (!monster_free || !monster_at(c))
           && !map_masked(c, mapmask)
           && (adjacent_feat == DNGN_UNSEEN ||
               dgn_has_adjacent_feat(c, adjacent_feat));
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
static coord_def _dgn_random_point_in_bounds(dungeon_feature_type searchfeat,
                                     uint32_t mapmask,
                                     dungeon_feature_type adjacent_feat,
                                     bool monster_free,
                                     int tries)
{
    if (tries == -1)
    {
        // Try a quick and dirty random search:
        int n = 10;
        if (searchfeat == DNGN_FLOOR)
            n = 500;
        coord_def chosen = _dgn_random_point_in_bounds(searchfeat,
                                                       mapmask,
                                                       adjacent_feat,
                                                       monster_free,
                                                       n);
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
        return chosen;
    }
    else
    {
        // Random search.
        while (--tries >= 0)
        {
            const coord_def c = random_in_bounds();
            if (_point_matches_feat(c, searchfeat, mapmask, adjacent_feat,
                                    monster_free))
            {
                return c;
            }
        }
        return coord_def(0, 0);
    }
}

static coord_def _place_specific_feature(dungeon_feature_type feat)
{
    /* Only overwrite vaults when absolutely necessary. */
    coord_def c = _dgn_random_point_in_bounds(DNGN_FLOOR, MMT_VAULT, DNGN_UNSEEN, true);
    if (!in_bounds(c))
        c = _dgn_random_point_in_bounds(DNGN_FLOOR, 0, DNGN_UNSEEN, true);

    if (in_bounds(c))
        env.grid(c) = feat;
    else
        throw dgn_veto_exception("Cannot place specific feature.");

    return c;
}

static bool _place_vault_by_tag(const string &tag)
{
    const map_def *vault = random_map_for_tag(tag, true);
    if (!vault)
        return false;
    return _build_secondary_vault(vault);
}

static bool _in_descent_parent(branch_type branch)
{
    vector<branch_type> parents = descent_parents(branch);
    for (branch_type parent : parents)
        if (player_in_branch(parent))
            return true;
    return false;
}

static void _place_branch_entrances(bool use_vaults)
{
    // Find what branch entrances are already placed, and what branch
    // entrances could be placed here.
    bool branch_entrance_placed[NUM_BRANCHES];
    bool could_be_placed = false;
    for (branch_iterator it; it; ++it)
    {
        branch_entrance_placed[it->id] = false;
        if (!could_be_placed
            && !branch_is_unfinished(it->id)
            && !is_hell_subbranch(it->id)
            && ((you.depth >= it->mindepth
                 && you.depth <= it->maxdepth)
                || level_id::current() == brentry[it->id]
                || crawl_state.game_is_descent()))
        {
            could_be_placed = true;
        }
    }

    // If there's nothing to be placed, don't bother.
    if (!could_be_placed)
        return;

    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (env.grid(*ri) == DNGN_ENTER_HELL)
            continue;
        if (!feat_is_branch_entrance(env.grid(*ri)))
            continue;

        for (branch_iterator it; it; ++it)
            if (it->entry_stairs == env.grid(*ri)
                && !feature_mimic_at(*ri))
            {
                branch_entrance_placed[it->id] = true;
                break;
            }
    }

    if (crawl_state.game_is_descent())
    {
        ASSERT(you.props.exists(DESCENT_WATER_BRANCH_KEY));
        ASSERT(you.props.exists(DESCENT_POIS_BRANCH_KEY));
    }

    // Place actual branch entrances.
    for (branch_iterator it; it; ++it)
    {
        // Vestibule and hells are placed by other means.
        // Likewise, if we already have an entrance, keep going.
        if (is_hell_branch(it->id) || branch_entrance_placed[it->id])
            continue;

        bool brentry_allowed = false;

        if (crawl_state.game_is_descent())
        {
            brentry_allowed = it->entry_stairs != NUM_FEATURES
                && _in_descent_parent(it->id)
                && it->id != you.props[DESCENT_WATER_BRANCH_KEY].get_int()
                && it->id != you.props[DESCENT_POIS_BRANCH_KEY].get_int()
                && at_branch_bottom();
        }
        else
        {
            brentry_allowed = it->entry_stairs != NUM_FEATURES
                && player_in_branch(parent_branch(it->id))
                && level_id::current() == brentry[it->id];
        }

        if (brentry_allowed)
        {
            // Placing a stair.
            dprf(DIAG_DNGN, "Placing stair to %s", it->shortname);

            // Attempt to place an entry vault if allowed
            if (use_vaults)
            {
                string entry_tag = string(it->abbrevname);
                entry_tag += "_entry";
                lowercase(entry_tag);

                if (_place_vault_by_tag(entry_tag))
                    // Placed this entrance, carry on to subsequent branches
                    continue;
            }

            // Otherwise place a single stair feature.
            // Try to use designated locations for entrances if possible.
            const coord_def portal_pos = find_portal_place(nullptr, false);
            if (!portal_pos.origin())
            {
                env.grid(portal_pos) = it->entry_stairs;
                env.level_map_mask(portal_pos) |= MMT_VAULT;
                continue;
            }

            const coord_def stair_pos = _place_specific_feature(it->entry_stairs);
            // Don't allow subsequent vaults to overwrite the branch stair
            env.level_map_mask(stair_pos) |= MMT_VAULT;
        }
    }
}

static void _place_extra_vaults()
{
    int tries = 0;
    while (true)
    {
        if (!player_in_branch(BRANCH_DUNGEON) && use_random_maps)
        {
            const map_def *vault = random_map_in_depth(level_id::current(),
                                                       false, true);

            // Encompass vaults can't be used as secondaries.
            if (!vault || vault->orient == MAP_ENCOMPASS)
                break;

            if (_build_secondary_vault(vault))
            {
                const map_def &map(*vault);
                if (map.is_extra_vault())
                    continue;
                use_random_maps = false;
            }
            else if (++tries >= 3)
                break;
        }
        break;
    }
}

// Place uniques on the level.
// Return the number of uniques placed.
static int _place_uniques()
{
#ifdef DEBUG_UNIQUE_PLACEMENT
    FILE *ostat = fopen_u("unique_placement.log", "a");
    fprintf(ostat, "--- Looking to place uniques on %s\n",
                   level_id::current().describe().c_str());
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
        const mapref_vector uniques_available =
            find_maps_for_tag("place_unique", true, true);

        if (!x_chance_in_y((int)uniques_available.size(), B))
            break;

        const map_def *uniq_map = random_map_for_tag("place_unique", true);
        if (!uniq_map)
        {
#ifdef DEBUG_UNIQUE_PLACEMENT
            fprintf(ostat, "Dummy balance or no uniques left.\n");
#endif
            break;
        }

        const bool map_placed = _build_secondary_vault(uniq_map);
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
            dprf(DIAG_DNGN, "Placed %s.", uniq_map->name.c_str());
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

static void _place_aquatic_in(vector<coord_def> &places, const vector<pop_entry>& pop,
                              int level, bool allow_zombies)
{
    if (places.size() < 35)
        return;
    int num = min(random_range(places.size() / 35, places.size() / 18), 15);
    shuffle_array(places);

    for (int i = 0; i < num; i++)
    {
        monster_type mon = pick_monster_from(pop, level);
        if (mon == MONS_0)
            break;

        mgen_data mg;
        mg.behaviour = BEH_SLEEP;
        mg.flags    |= MG_PERMIT_BANDS | MG_FORCE_PLACE;
        mg.map_mask |= MMT_NO_MONS;
        mg.cls = mon;
        mg.pos = places[i];

        // Amphibious creatures placed with water should hang around it
        if (mons_class_primary_habitat(mon) == HT_LAND)
            mg.flags |= MG_PATROLLING;

        if (allow_zombies
            && player_in_hell()
            && mons_class_can_be_zombified(mg.cls))
        {
            mg.base_type = mg.cls;
            const int skel_chance = mons_skeleton(mg.cls) ? 2 : 0;
            mg.cls = random_choose_weighted(skel_chance, MONS_SKELETON,
                                            8,           MONS_ZOMBIE,
                                            1,           MONS_SIMULACRUM);
        }

        place_monster(mg);
    }
}

static void _place_aquatic_monsters()
{
    // Shoals relies on normal monster generation to place its monsters.
    // Abyss's nature discourages random movement-inhibited monsters.
    // Default liquid creatures are harmless in Pan or Zot, and
    // threatening ones are distracting from their sets.
    // Random liquid monster placement is too vicious before D:6.
    //
    if (player_in_branch(BRANCH_SHOALS)
        || player_in_branch(BRANCH_ABYSS)
        || player_in_branch(BRANCH_PANDEMONIUM)
        || player_in_branch(BRANCH_ZOT)
        || player_in_branch(BRANCH_DUNGEON) && you.depth < 6)
    {
        return;
    }

    int level = level_id::current().depth;

    vector<coord_def> water;
    vector<coord_def> lava;

    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (actor_at(*ri) || env.level_map_mask(*ri) & MMT_NO_MONS)
            continue;

        dungeon_feature_type feat = env.grid(*ri);
        if (feat == DNGN_SHALLOW_WATER || feat == DNGN_DEEP_WATER)
            water.push_back(*ri);
        else if (feat == DNGN_LAVA)
            lava.push_back(*ri);
    }

    _place_aquatic_in(water, fish_population(you.where_are_you, false), level,
                      true);
    _place_aquatic_in(lava, fish_population(you.where_are_you, true), level,
                      false);
}

static vector<monster_type> _zombifiables()
{
    vector<monster_type> z;
    for (monster_type mcls = MONS_0; mcls < NUM_MONSTERS; ++mcls)
    {
        if (mons_species(mcls) != mcls
            || !mons_zombie_size(mcls)
            || mons_is_unique(mcls)
            || !(mons_class_holiness(mcls) & MH_NATURAL)
            || mons_class_flag(mcls, M_NO_GEN_DERIVED))
        {
            continue;
        }

        z.push_back(mcls);
    }
    return z;
}

// For Crypt, adds a bunch of skeletons and zombies that do not respect
// absdepth (and thus tend to be varied and include several types that
// would not otherwise spawn there).
static void _place_assorted_zombies()
{
    static const vector<monster_type> zombifiable = _zombifiables();
    int num_zombies = random_range(6, 12, 3);
    for (int i = 0; i < num_zombies; ++i)
    {
        bool skel = coinflip();
        monster_type z_base;
        do
        {
            z_base = zombifiable[random2(zombifiable.size())];
        }
        while (skel && !mons_skeleton(z_base));

        mgen_data mg;
        mg.cls = (skel ? MONS_SKELETON : MONS_ZOMBIE);
        mg.base_type = z_base;
        mg.behaviour = BEH_SLEEP;
        mg.map_mask |= MMT_NO_MONS;
        mg.flags |= MG_PREFER_LAND;

        place_monster(mg);
    }
}

bool door_vetoed(const coord_def pos)
{
    return env.markers.property_at(pos, MAT_ANY, "veto_open") == "veto";
}

static void _builder_monsters()
{
    if (player_in_branch(BRANCH_TEMPLE))
        return;

    int mon_wanted = _num_mons_wanted();

    const bool in_shoals = player_in_branch(BRANCH_SHOALS);
    if (in_shoals)
        dgn_shoals_generate_flora();

    dprf(DIAG_DNGN, "_builder_monsters: Generating %d monsters", mon_wanted);
    for (int i = 0; i < mon_wanted; i++)
    {
        mgen_data mg;

        // On D:1, we want monsters out of LOS distance from the player's
        // starting position, and we don't generate them awake.
        if (env.absdepth0 == 0)
        {
            mg.behaviour = BEH_SLEEP;
            mg.proximity = PROX_AWAY_FROM_ENTRANCE;
        }
        else if (player_in_connected_branch() && one_chance_in(8))
        {
            // Chance to generate the monster awake, but away from all level
            // stairs (including the delver start stairs).
            mg.proximity = PROX_AWAY_FROM_STAIRS;
        }
        else if (!player_in_branch(BRANCH_PANDEMONIUM))
        {
            // Pan monsters always generate awake.

            // For delvers, waking monsters can generate on D:5, but they can't
            // be near the entrance.
            if (env.absdepth0 == starting_absdepth())
                mg.proximity = PROX_AWAY_FROM_ENTRANCE;
            mg.behaviour = BEH_SLEEP;
        }

        mg.flags    |= MG_PERMIT_BANDS;
        mg.map_mask |= MMT_NO_MONS;

        // Try to place Shoals monsters on solid ground where possible, instead
        // of letting half the level spawn at the far reaches of deep water.
        if (in_shoals)
            mg.flags |= MG_PREFER_LAND;

        place_monster(mg);
    }

    if (!player_in_branch(BRANCH_CRYPT)) // No water creatures in the Crypt.
        _place_aquatic_monsters();
    else
        _place_assorted_zombies();
}

/**
 * Randomly place a single item
 *
 * @param item   The item slot of the item being randomly placed
 */
static void _randomly_place_item(int item)
{
    coord_def itempos;
    bool found = false;
    for (int i = 0; i < 500 && !found; ++i)
    {
        itempos = random_in_bounds();
        const monster* mon = monster_at(itempos);
        found = env.grid(itempos) == DNGN_FLOOR
                && !map_masked(itempos, MMT_NO_ITEM)
                // oklobs or statues are ok
                && (!mon || !mon->is_firewood());
    }
    if (!found)
    {
        dprf(DIAG_DNGN, "Builder failed to place %s",
            env.item[item].name(DESC_PLAIN, false, true).c_str());
        // Couldn't find a single good spot!
        destroy_item(item);
    }
    else
    {
        dprf(DIAG_DNGN, "Builder placing %s at %d,%d",
            env.item[item].name(DESC_PLAIN, false, true).c_str(),
            itempos.x, itempos.y);
        move_item_to_grid(&item, itempos);
    }
}

/**
 * Randomly place items on a level. Does not place items in vaults,
 * on monsters, etc. Only normal floor generated items.
 */
static void _builder_items()
{
    int i = 0;
    object_class_type specif_type = OBJ_RANDOM;
    int items_levels = env.absdepth0;
    int items_wanted = _num_items_wanted(items_levels);

    if (player_in_branch(BRANCH_VAULTS))
    {
        items_levels *= 15;
        items_levels /= 10;
    }
    else if (player_in_branch(BRANCH_ORC))
    {
        specif_type = OBJ_GOLD;  // Lots of gold in the orcish mines.
        items_levels *= 2;       // Four levels' worth, in fact.
    }

    for (i = 0; i < items_wanted; i++)
    {
        int item = items(true, specif_type, OBJ_RANDOM, items_levels);

        _randomly_place_item(item);
    }

}

static bool _connect_vault_exit(const coord_def& exit)
{
    flood_find<feature_grid, coord_predicate> ff(env.grid, in_bounds, true,
                                                 false);
    ff.add_feat(DNGN_FLOOR);

    coord_def target = ff.find_first_from(exit, env.level_map_mask);

    if (in_bounds(target))
        return join_the_dots(exit, target, MMT_VAULT);

    return false;
}

static bool _grid_needs_exit(const coord_def& c)
{
    return !cell_is_solid(c)
           || feat_is_closed_door(env.grid(c));
}

static bool _map_feat_is_on_edge(const vault_placement &place,
                                 const coord_def &c)
{
    if (!place.map.in_map(c - place.pos))
        return false;

    for (orth_adjacent_iterator ai(c); ai; ++ai)
        if (!place.map.in_map(*ai - place.pos))
            return true;

    return false;
}

static void _pick_float_exits(vault_placement &place, vector<coord_def> &targets)
{
    vector<coord_def> possible_exits;

    for (rectangle_iterator ri(place.pos, place.pos + place.size - 1); ri; ++ri)
        if (_grid_needs_exit(*ri) && _map_feat_is_on_edge(place, *ri))
            possible_exits.push_back(*ri);

    if (possible_exits.empty())
    {
        // Empty map (a serial vault master, etc).
        if (place.size.origin())
            return;

        // The vault is disconnected, does it have a stair inside?
        for (rectangle_iterator ri(place.pos, place.pos + place.size - 1); ri; ++ri)
            if (feat_is_stair(env.grid(*ri)))
                return;

        mprf(MSGCH_ERROR, "Unable to find exit from %s",
             place.map.name.c_str());
        return;
    }

    const int npoints = possible_exits.size();
    int nexits = npoints < 6? npoints : npoints / 8 + 1;

    if (nexits > 10)
        nexits = 10;

    while (nexits-- > 0)
    {
        int which_exit = random2(possible_exits.size());
        targets.push_back(possible_exits[which_exit]);
        possible_exits.erase(possible_exits.begin() + which_exit);
    }
}

static void _fixup_after_vault()
{
    _dgn_set_floor_colours();

    link_items();
    env.markers.activate_all();

    // Force teleport to place the player somewhere sane.
    you_teleport_now();

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
// check_collision: If true, the newly placed vault cannot clobber existing
//          items and monsters (otherwise, items may be destroyed, monsters may
//          be teleported).
//
// Non-dungeon code should generally use dgn_safe_place_map instead of
// this function to recover from map_load_exceptions.
const vault_placement *dgn_place_map(const map_def *mdef,
                                     bool check_collision,
                                     bool make_no_exits,
                                     const coord_def &where)
{
    if (!mdef)
        return nullptr;

    const dgn_colour_override_manager colour_man;

    if (mdef->orient == MAP_ENCOMPASS && !crawl_state.generating_level)
    {
        if (check_collision)
        {
            mprf(MSGCH_DIAGNOSTICS,
                 "Cannot generate encompass map '%s' with check_collision=true",
                 mdef->name.c_str());

            return nullptr;
        }

        // For encompass maps, clear the entire level.
        unwind_bool levgen(crawl_state.generating_level, true);
        dgn_reset_level();
        dungeon_events.clear();
        const vault_placement *vault_place =
            dgn_place_map(mdef, check_collision, make_no_exits, where);
        if (vault_place)
            _fixup_after_vault();
        return vault_place;
    }

    const vault_placement *vault_place =
        _build_secondary_vault(mdef, check_collision,
                               make_no_exits, where);

    // Activate any markers within the map.
    if (vault_place && !crawl_state.generating_level)
    {
#ifdef ASSERTS
        if (mdef->name != vault_place->map.name)
        {
            die("Placed map '%s', yet vault_placement is '%s'",
                mdef->name.c_str(), vault_place->map.name.c_str());
        }
#endif

        for (vault_place_iterator vpi(*vault_place); vpi; ++vpi)
        {
            const coord_def p = *vpi;
            env.markers.activate_markers_at(p);
            if (!you.see_cell(p))
                set_terrain_changed(p);
        }
        env.markers.clear_need_activate();

        setup_environment_effects();
        _dgn_postprocess_level();
    }

    return vault_place;
}

// Identical to dgn_place_map, but recovers gracefully from
// map_load_exceptions. Prefer this function if placing maps *not*
// during level generation time.
//
// Returns the map actually placed if the map was placed successfully.
// This is usually the same as the map passed in, unless map load
// failed and maps had to be reloaded.
const vault_placement *dgn_safe_place_map(const map_def *mdef,
                                          bool check_collision,
                                          bool make_no_exits,
                                          const coord_def &where)
{
    const string mapname(mdef->name);
    int retries = 10;
    while (true)
    {
        try
        {
            return dgn_place_map(mdef, check_collision, make_no_exits, where);
        }
        catch (map_load_exception &mload)
        {
            if (retries-- > 0)
            {
                mprf(MSGCH_ERROR,
                     "Failed to load map %s in dgn_safe_place_map, "
                     "reloading all maps",
                     mload.what());
                reread_maps();

                mdef = find_map_by_name(mapname);
            }
            else
                return nullptr;
        }
    }
}

vault_placement *dgn_vault_at(coord_def p)
{
    const int map_index = env.level_map_ids(p);
    return map_index == INVALID_MAP_INDEX ? nullptr
                                          : env.level_vaults[map_index].get();
}

vault_placement *dgn_find_layout()
{
    // n.b.: following placement code, this checks the tag "overwritable" to
    // determine whether something is a layout. Weirdly, essentially no code
    // checks the "layout" tag.
    // Given these cases, it may be possible for there to be more than one
    // layout, in which case, this will find the first.
    for (const auto &vp : env.level_vaults)
        if (vp && vp->map.is_overwritable_layout())
            return vp.get();
    return nullptr;
}

void dgn_seen_vault_at(coord_def p)
{
    if (vault_placement *vp = dgn_vault_at(p))
    {
        if (!vp->seen)
        {
            dprf(DIAG_DNGN, "Vault %s (%d,%d)-(%d,%d) seen",
                 vp->map.name.c_str(), vp->pos.x, vp->pos.y,
                 vp->size.x, vp->size.y);
            vp->seen = true;
        }
    }
}

static bool _vault_wants_damage(const vault_placement &vp)
{
    const map_def &map = vp.map;
    if (map.has_tag("ruin"))
        return true;

    // Some vaults may want to be ruined only in certain places with
    // tags like "ruin_abyss" or "ruin_lair"
    string place_desc = level_id::current().describe(false, false);
    lowercase(place_desc);
    return map.has_tag("ruin_" + place_desc);
}

static void _ruin_vault(const vault_placement &vp)
{
    _ruin_level(vault_place_iterator(vp), 0, 12, 0);
}

// Places a vault somewhere in an already built level if possible.
// Returns true if the vault was successfully placed.
static const vault_placement *_build_secondary_vault(const map_def *vault,
                       bool check_collision, bool no_exits,
                       const coord_def &where)
{
    return _build_vault_impl(vault, true, check_collision, no_exits, where);
}

// Builds a primary vault - i.e. a vault that is built before anything
// else on the level. After placing the vault, rooms and corridors
// will be constructed on the level and the vault exits will be
// connected to corridors.
//
// If portions of the level are already generated at this point, use
// _build_secondary_vault or dgn_place_map instead.
//
// NOTE: minivaults can never be placed as primary vaults.
//
static const vault_placement *_build_primary_vault(const map_def *vault)
{
    return _build_vault_impl(vault);
}

// Builds a vault or minivault. Do not use this function directly: always
// prefer _build_secondary_vault or _build_primary_vault.
static const vault_placement *_build_vault_impl(const map_def *vault,
                  bool build_only, bool check_collisions,
                  bool make_no_exits, const coord_def &where)
{
    if (dgn_check_connectivity && !dgn_zones)
    {
        dgn_zones = dgn_count_disconnected_zones(false);
        if (player_in_branch(BRANCH_PANDEMONIUM) && dgn_zones > 1)
            throw dgn_veto_exception("Pan map with disconnected zones");
    }

    if (crawl_state.game_is_descent() && vault->get_tags_unsorted().count("no_descent"))
        throw dgn_veto_exception("Illegal map for descent");

    unwind_var<string> placing(env.placing_vault, vault->name);

    vault_placement place;

    if (map_bounds(where))
        place.pos = where;

    const map_section_type placed_vault_orientation =
        vault_main(place, vault, check_collisions);

    dprf(DIAG_DNGN, "Map: %s; placed: %s; place: (%d,%d), size: (%d,%d)",
         vault->name.c_str(),
         placed_vault_orientation != MAP_NONE ? "yes" : "no",
         place.pos.x, place.pos.y, place.size.x, place.size.y);

    // veto if this was a fatal error. (Vetoing in general here would be
    // extremely costly.)
    // TODO: I'm not sure this is necessary, but it makes the error more
    // prominent
    _dgn_ensure_vault_placed(placed_vault_orientation != MAP_NONE
        || !crawl_state.last_builder_error_fatal, false,
        vault->name);

    if (placed_vault_orientation == MAP_NONE)
        return nullptr;

    const bool is_layout = place.map.is_overwritable_layout();

    if (placed_vault_orientation == MAP_ENCOMPASS && !is_layout)
        env.level_layout_types.insert("encompass");

    if (!build_only
        && (placed_vault_orientation == MAP_ENCOMPASS || is_layout)
        && place.map.border_fill_type != DNGN_ROCK_WALL)
    {
        dgn_replace_area(0, 0, GXM-1, GYM-1, DNGN_ROCK_WALL,
                         place.map.border_fill_type);
    }

    // XXX: Moved this out of dgn_register_place so that vault-set monsters can
    // be accessed with the '9' and '8' glyphs. (due)
    if (!place.map.random_mons.empty())
    {
        dprf(DIAG_DNGN, "Setting the custom random mons list.");
        set_vault_mon_list(place.map.random_mons);
    }

    place.apply_grid();

    if (_vault_wants_damage(place))
        _ruin_vault(place);

    if (place.exits.empty() && placed_vault_orientation != MAP_ENCOMPASS
        && (!place.map.is_minivault() || !place.map.has_tag("no_exits")))
    {
        _pick_float_exits(place, place.exits);
    }

    if (make_no_exits)
        place.exits.clear();

    // Must do this only after target_connections is finalised, or the vault
    // exits will not be correctly set.
    const vault_placement *saved_place = dgn_register_place(place, true);

#ifdef DEBUG_STATISTICS
    if (crawl_state.map_stat_gen)
        mapstat_report_map_use(place.map);
#endif

    if (is_layout && place.map.has_tag_prefix("layout_type_"))
    {
        for (auto &tag : place.map.get_tags())
        {
            if (starts_with(tag, "layout_type_"))
            {
                env.level_layout_types.insert(
                    tag_without_prefix(tag, "layout_type_"));
            }
        }
    }

    // If the map takes the whole screen or we were only requested to
    // build the vault, our work is done.
    if (!build_only && (placed_vault_orientation != MAP_ENCOMPASS || is_layout))
    {
        if (!is_layout)
            _build_postvault_level(place);

        dgn_place_stone_stairs(true);
    }

    if (!build_only && (placed_vault_orientation != MAP_ENCOMPASS || is_layout)
        && player_in_branch(BRANCH_SWAMP))
    {
        _process_disconnected_zones(0, 0, GXM-1, GYM-1, true, DNGN_MANGROVE);
        // do a second pass to remove tele closets consisting of deep water
        // created by the first pass -- which will not fill in deep water
        // because it is treated as impassable.
        // TODO: get zonify to prevent these?
        // TODO: does this come up anywhere outside of swamp?
        _process_disconnected_zones(0, 0, GXM-1, GYM-1, true, DNGN_MANGROVE,
                _dgn_square_is_ever_passable);
    }

    if (!make_no_exits)
    {
        const bool spotty =
            testbits(branches[you.where_are_you].branch_flags, brflag::spotty);
        if (place.connect(spotty) == 0 && place.exits.size() > 0
            && !player_in_branch(BRANCH_ABYSS))
        {
            throw dgn_veto_exception("Failed to connect exits for: "
                                     + place.map.name);
        }
    }

    // Fire any post-place hooks defined for this map; any failure
    // here is an automatic veto. Note that the post-place hook must
    // be run only after _build_postvault_level.
    if (!place.map.run_postplace_hook())
    {
        throw dgn_veto_exception("Post-place hook failed for: "
                                 + place.map.name);
    }

    return saved_place;
}

static void _build_postvault_level(vault_placement &place)
{
    if (player_in_branch(BRANCH_SPIDER))
    {
        int ngb_min = 2;
        int ngb_max = random_range(3, 8);
        if (one_chance_in(10))
            ngb_min = 1, ngb_max = random_range(5, 7);
        if (one_chance_in(20))
            ngb_min = 3, ngb_max = 4;
        const int connchance = random_choose(0, 5, 20, 50, 100);
        const int top = random_choose(1, 20, 125, 500, 999999);
        delve(0, ngb_min, ngb_max, connchance, -1, top);
    }
    else
    {
        const map_def* layout = _pick_layout(&place.map);
        ASSERT(layout);
        {
            dgn_map_parameters mp(place.orient == MAP_CENTRE
                                  ? "central" : "layout");
            _build_secondary_vault(layout, false);
        }
    }
}

static int _dgn_item_corpse(const item_spec &ispec, const coord_def where)
{
    rng::subgenerator corpse_rng;

    mons_spec mspec(ispec.corpse_monster_spec());
    item_def* corpse = nullptr;

    for (int tries = 0; !corpse; tries++)
    {
        if (tries > 200)
            return NON_ITEM;
        monster *mon = dgn_place_monster(mspec, coord_def(), true);
        if (!mon)
            continue;
        mon->position = where;
        corpse = place_monster_corpse(*mon, true);
        // Dismiss the monster we used to place the corpse.
        monster_die(*mon, KILL_RESET, NON_MONSTER, true);
    }

    if (ispec.props.exists(CORPSE_NEVER_DECAYS))
    {
        corpse->props[CORPSE_NEVER_DECAYS].get_bool() =
            ispec.props[CORPSE_NEVER_DECAYS].get_bool();
    }

    if (ispec.base_type == OBJ_CORPSES && ispec.sub_type == CORPSE_SKELETON)
        turn_corpse_into_skeleton(*corpse);

    if (ispec.props.exists(MONSTER_HIT_DICE))
    {
        corpse->props[MONSTER_HIT_DICE].get_short() =
            ispec.props[MONSTER_HIT_DICE].get_short();
    }

    return corpse->index();
}

static bool _apply_item_props(item_def &item, const item_spec &spec,
                              bool allow_useless, bool monster)
{
    const CrawlHashTable props = spec.props;

    if (props.exists(THEME_BOOK_KEY))
    {
        string owner = props[RANDBK_OWNER_KEY].get_string();
        if (owner == "player")
            owner = you.your_name;
        const string title = props[RANDBK_TITLE_KEY].get_string();

        vector<spell_type> spells;
        CrawlVector spell_list = props[RANDBK_SPELLS_KEY].get_vector();
        for (unsigned int i = 0; i < spell_list.size(); ++i)
            spells.push_back((spell_type) spell_list[i].get_int());

        spschool disc1 = (spschool)props[RANDBK_DISC1_KEY].get_short();
        spschool disc2 = (spschool)props[RANDBK_DISC2_KEY].get_short();
        bool disc1_rerollable = false;
        bool disc2_rerollable = false;
        if (disc1 == spschool::none && disc2 == spschool::none)
        {
            if (spells.size())
                disc1 = matching_book_theme(spells);
            else
                disc1_rerollable = true;
            disc2_rerollable = true;
        }
        else if (disc2 == spschool::none)
            disc2 = disc1;
        else
            ASSERT(disc1 != spschool::none); // mapdef should've handled this

        int num_spells = props[RANDBK_NSPELLS_KEY].get_short();
        if (num_spells < 1)
            num_spells = theme_book_size();
        const int max_levels = props[RANDBK_SLVLS_KEY].get_short();

        vector<spell_type> chosen_spells;
        int retries = 0;
        // TODO: retry if the spell list is smaller than requested?
        while (chosen_spells.size() == 0 && retries < 10)
        {
            // allow rerolling these in case the spell schools are too sparse
            // for the vault spec. Most likely to come up in corner cases such
            // as a 1-spell book with 1 spell level, if the same school is
            // rolled twice and it lacks a lvl 1 spll
            if (disc1_rerollable)
                disc1 = random_book_theme();
            if (disc2_rerollable)
                disc2 = random_book_theme();
            theme_book_spells(disc1, disc2,
                          forced_spell_filter(spells,
                                               capped_spell_filter(max_levels)),
                          origin_as_god_gift(item), num_spells, chosen_spells);
            if (!disc1_rerollable && !disc2_rerollable)
                break;
            retries++;
        }
        fixup_randbook_disciplines(disc1, disc2, chosen_spells);
        // if a size 0 spell list gets through here it'll crash in the following
        // call:
        init_book_theme_randart(item, chosen_spells);
        name_book_theme_randart(item, disc1, disc2, owner, title);
        // XXX: changing the signature of build_themed_book()'s get_discipline
        // would allow us to roll much of this ^ into that. possibly clever
        // lambdas could let us do it without even changing the signature?
    }

    // Ew. We should really pass this in to items() somehow.
    if (spec.props.exists(NO_EXCLUDE_KEY) && spec.base_type == OBJ_MISCELLANY)
    {
        rng::subgenerator item_rng;
        item.base_type = OBJ_MISCELLANY;
        const auto typ = get_misc_item_type(spec.sub_type, false);
        item.sub_type = typ;
        item_colour(item);
        item_set_appearance(item);
        ASSERT(item.is_valid());
    }

    // Wipe item origin to remove "this is a god gift!" from there,
    // unless we're dealing with a corpse.
    if (!spec.corpselike())
        origin_reset(item);
    if (is_stackable_item(item) && spec.qty > 0)
        item.quantity = spec.qty;

    if (spec.item_special)
        item.special = spec.item_special;

    if (spec.plus >= 0 && item.is_type(OBJ_BOOKS, BOOK_MANUAL))
    {
        item.plus = spec.plus;
        item_colour(item);
    }

    if (item.base_type == OBJ_RUNES)
    {
        if (you.runes[item.sub_type])
        {
            destroy_item(item, true);
            return false;
        }
        item_colour(item);
    }

    if (item.base_type == OBJ_GEMS)
        item_colour(item);

    if (props.exists(USEFUL_KEY) && is_useless_item(item, false)
        && !allow_useless)
    {
        destroy_item(item, true);
        return false;
    }
    if (item.base_type == OBJ_WANDS && props.exists(CHARGES_KEY))
        item.charges = props[CHARGES_KEY].get_int();
    if ((item.base_type == OBJ_WEAPONS || item.base_type == OBJ_ARMOUR
         || item.base_type == OBJ_JEWELLERY || item.base_type == OBJ_MISSILES)
        && props.exists(PLUS_KEY) && !is_unrandom_artefact(item))
    {
        item.plus = props[PLUS_KEY].get_int();
        item_set_appearance(item);
    }
    if (props.exists(IDENT_KEY))
        item.flags |= ISFLAG_IDENTIFIED;
    if (props.exists(UNOBTAINABLE_KEY))
    {
        item.flags |= ISFLAG_UNOBTAINABLE;
        if (is_unrandom_artefact(item) || is_xp_evoker(item))
        {
            destroy_item(item, true);
            return false;
        }
    }

    if (props.exists(CHAOTIC_ITEM_KEY) && is_unrandom_artefact(item)
        && item.base_type == OBJ_WEAPONS)
    {
        item.flags |= ISFLAG_CHAOTIC;
    }

    if (props.exists(NO_PICKUP_KEY))
        item.flags |= ISFLAG_NO_PICKUP;

    if (props.exists(ITEM_TILE_NAME_KEY))
        item.props[ITEM_TILE_NAME_KEY] = props[ITEM_TILE_NAME_KEY].get_string();
    if (props.exists(WORN_TILE_NAME_KEY))
        item.props[WORN_TILE_NAME_KEY] = props[WORN_TILE_NAME_KEY].get_string();
    bind_item_tile(item);

    if (!monster)
    {
        if (props.exists(MIMIC_KEY))
        {
            const int chance = props[MIMIC_KEY];
            if (chance > 0 && one_chance_in(chance))
                item.flags |= ISFLAG_MIMIC;
        }
    }

    return true;
}

static object_class_type _superb_object_class()
{
    return random_choose_weighted(
            20, OBJ_WEAPONS,
            10, OBJ_ARMOUR,
            10, OBJ_JEWELLERY,
            10, OBJ_BOOKS,
            10, OBJ_STAVES,
            10, OBJ_MISCELLANY,
            1, OBJ_TALISMANS);
}

static int _concretize_level(int spec_level, int dgn_level)
{
    if (spec_level >= 0)
        return spec_level;

    if (dgn_level == INVALID_ABSDEPTH)
        dgn_level = env.absdepth0;

    switch (spec_level)
    {
    case ISPEC_DAMAGED:
    case ISPEC_BAD:
    case ISPEC_RANDART:
        return spec_level;
    case ISPEC_STAR:
        return 5 + dgn_level * 2;
    case ISPEC_SUPERB:
        return ISPEC_GOOD_ITEM;
    default:
        return dgn_level;
    }
}

static object_class_type _concretize_type(const item_spec &spec)
{
    if (spec.base_type != OBJ_RANDOM)
        return spec.base_type;
    if (spec.props.exists(MIMIC_KEY))
        return get_random_item_mimic_type();
    if (spec.level == ISPEC_SUPERB)
    {
        // an unobtainable superb misc item is guaranteed to fail to generate,
        // so prevent it from the start
        // n.b. used in exactly one place, see `grunt_nemelex_the_gamble`
        object_class_type ret;
        do
        {
            ret = _superb_object_class();
        }
        while (spec.props.exists(UNOBTAINABLE_KEY) && ret == OBJ_MISCELLANY);
        return ret;
    }
    if (spec.level == ISPEC_ACQUIREMENT)
        return shuffled_acquirement_classes(false)[0];
    return spec.base_type;
}

static string _get_custom_name(const item_spec &spec)
{
    if (spec.props.exists(ITEM_NAME_KEY))
        return spec.props[ITEM_NAME_KEY].get_string();
    return "";
}

int dgn_place_item(const item_spec &spec,
                   const coord_def &where,
                   int dgn_level)
{
    // Dummy object?
    if (spec.base_type == OBJ_UNASSIGNED)
        return NON_ITEM;

    const int level = _concretize_level(spec.level, dgn_level);
    const object_class_type base_type = _concretize_type(spec);

    int useless_tries = 0;
    while (true)
    {
        int item_made = NON_ITEM;

        if (spec.level == ISPEC_ACQUIREMENT)
        {
            item_made = acquirement_create_item(base_type,
                                                spec.acquirement_source,
                                                true, where);
        }

        // Both normal item generation and the failed "acquire foo" fallback.
        if (item_made == NON_ITEM)
        {
            if (spec.corpselike())
                item_made = _dgn_item_corpse(spec, where);
            else
            {
                CrawlHashTable const *fixed_props = nullptr;
                if (spec.props.exists(FIXED_PROPS_KEY))
                    fixed_props = &spec.props[FIXED_PROPS_KEY].get_table();

                item_made = items(spec.allow_uniques, base_type,
                                  spec.sub_type, level, spec.ego, NO_AGENT,
                                  _get_custom_name(spec), fixed_props);

                if (spec.level == ISPEC_MUNDANE)
                    squash_plusses(item_made);
            }
        }

        if (item_made == NON_ITEM || item_made == -1)
            return NON_ITEM;

        item_def &item(env.item[item_made]);
        item.pos = where;

        if (_apply_item_props(item, spec, useless_tries >= 10, false))
        {
            dprf(DIAG_DNGN, "vault spec: placing %s at %d,%d",
                env.item[item_made].name(DESC_INVENTORY, false, true).c_str(),
                where.x, where.y);
            env.level_map_mask(where) |= MMT_NO_TRAP;
            return item_made;
        }

        // _apply_item_props will not generate a rune you already have,
        // so don't bother looping.
        // also, if the allow_useless flag above fails a bunch, just give up.
        // XX is 20 tries reasonable?
        if (base_type == OBJ_RUNES || useless_tries >= 20)
            return NON_ITEM;
        useless_tries++;
    }

}

void dgn_place_multiple_items(item_list &list, const coord_def& where)
{
    const int size = list.size();
    for (int i = 0; i < size; ++i)
        dgn_place_item(list.get_item(i), where);
}

static void _dgn_place_item_explicit(int index, const coord_def& where,
                                     vault_placement &place)
{
    item_list &sitems = place.map.items;

    if ((index < 0 || index >= static_cast<int>(sitems.size())) &&
        !crawl_state.game_is_sprint())
    {
        return;
    }

    const item_spec spec = sitems.get_item(index);
    dgn_place_item(spec, where);
}

static void _dgn_give_mon_spec_items(mons_spec &mspec, monster *mon)
{
    ASSERT(mspec.place.is_valid());

    unwind_var<int> save_speedinc(mon->speed_increment);

    // Get rid of existing equipment.
    for (mon_inv_iterator ii(*mon); ii; ++ii)
    {
        mon->unequip(*ii, false, true);
        destroy_item(ii->index(), true);
    }

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

        const int item_level = _concretize_level(spec.level,
                                                 mspec.place.absdepth());
        for (int useless_tries = 0; true; useless_tries++)
        {
            int item_made;

            if (spec.corpselike())
                item_made = _dgn_item_corpse(spec, mon->pos());
            else
            {
                CrawlHashTable const *fixed_props = nullptr;
                if (spec.props.exists(FIXED_PROPS_KEY))
                    fixed_props = &spec.props[FIXED_PROPS_KEY].get_table();

                item_made = items(spec.allow_uniques, spec.base_type,
                                  spec.sub_type, item_level, spec.ego, NO_AGENT,
                                  _get_custom_name(spec), fixed_props);

                if (spec.level == ISPEC_MUNDANE)
                    squash_plusses(item_made);
            }

            if (!(item_made == NON_ITEM || item_made == -1))
            {
                item_def &item(env.item[item_made]);

                if (_apply_item_props(item, spec, (useless_tries >= 10), true))
                {
                    // Mark items on summoned monsters as such.
                    if (mspec.summon_duration != 0)
                        item.flags |= ISFLAG_SUMMONED;

                    if (!mon->pickup_item(item, false, true))
                        destroy_item(item_made, true);
                    break;
                }
            }
        }
    }

    // Pre-wield ranged weapons.
    if (mon->inv[MSLOT_WEAPON] == NON_ITEM
        && mon->inv[MSLOT_ALT_WEAPON] != NON_ITEM)
    {
        mon->swap_weapons(false);
    }
}

static bool _should_veto_unique(monster_type type)
{
    // Already generated.
    return mons_is_unique(type) && you.unique_creatures[type];
}

monster* dgn_place_monster(mons_spec &mspec, coord_def where,
                           bool force_pos, bool generate_awake, bool patrolling)
{
#if TAG_MAJOR_VERSION == 34
    if ((int)mspec.type == -1) // or rebuild the des cache
        return 0;
#endif
    if (mspec.type == MONS_NO_MONSTER)
        return 0;

    monster_type type = mspec.type;
    const bool m_generate_awake = (generate_awake || mspec.generate_awake);
    const bool m_patrolling     = (patrolling || mspec.patrolling);
    const bool m_band           = mspec.band;

    if (!mspec.place.is_valid())
        mspec.place = level_id::current();
    bool chose_ood = false;
    bool fuzz_ood  = true;
    const int starting_depth = mspec.place.depth;

    if (type == RANDOM_SUPER_OOD || type == RANDOM_MODERATE_OOD)
    {
        // don't do OOD depth adjustment for portal branches
        if (brdepth[mspec.place.branch] > 1)
        {
            if (type == RANDOM_SUPER_OOD)
            {
                // Super OODs use 2*depth + 4 which goes very deep, generating
                // a guaranteed "boss" for vaultsmiths to use. In dungeon it
                // ramps up slowly from 2*depth in the range D:1-8 (becoming
                // deeper than a moderate OOD a from D:6-) to the full meaning
                // at D:12.
                int plus = 4;
                if (mspec.place.branch == BRANCH_DUNGEON)
                    plus = max(0, min(mspec.place.depth - 8, 4));

                mspec.place.depth = mspec.place.depth * 2 + plus;
            }
            else if (type == RANDOM_MODERATE_OOD)
            {
                mspec.place.depth += 5;
                // From D:1-4 moderate OODs are capped at depth * 2, starting
                // on D:5 this cap no longer does anything.
                if (mspec.place.branch == BRANCH_DUNGEON)
                {
                    mspec.place.depth = min(mspec.place.depth,
                                            starting_depth * 2);
                }
            }
        }
        fuzz_ood = false;
        type = RANDOM_MONSTER;
    }

    if (type < NUM_MONSTERS)
    {
        // Don't place a unique monster a second time.
        // (Boris is handled specially.)
        if (_should_veto_unique(type)
            && !crawl_state.game_is_arena())
        {
            return 0;
        }

        const monster_type montype = mons_class_is_zombified(type)
                                                         ? mspec.monbase
                                                         : type;

        const habitat_type habitat = mons_class_primary_habitat(montype);

        if (in_bounds(where) && !monster_habitable_grid(montype, where))
            dungeon_terrain_changed(where, habitat2grid(habitat));
    }

    if (type == RANDOM_MONSTER)
    {
        if (mons_class_is_zombified(mspec.monbase))
            type = pick_local_zombifiable_monster(mspec.place, mspec.monbase, coord_def());
        else
        {
            level_id place = mspec.place;
            type = pick_random_monster(mspec.place, mspec.monbase, &place,
                                       fuzz_ood);
            if (place.depth > starting_depth + 5)
                chose_ood = true;
        }
        if (!type)
            type = RANDOM_MONSTER;
    }

    mgen_data mg(type);

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
    default:
        break;
    }
    mg.base_type = mspec.monbase;
    mg.colour    = mspec.colour;

    if (mspec.god != GOD_NO_GOD)
        mg.god   = mspec.god;

    mg.mname     = mspec.monname;
    mg.hd        = mspec.hd;
    mg.hp        = mspec.hp;
    mg.props     = mspec.props;

    if (mg.props.exists(MAP_KEY))
        mg.xp_tracking = XP_VAULT;

    // Marking monsters as summoned
    mg.summon_duration     = mspec.summon_duration;
    mg.summon_type         = mspec.summon_type;
    mg.non_actor_summoner  = mspec.non_actor_summoner;

    if (mg.colour == COLOUR_UNDEF)
        mg.colour = random_monster_colour();

    if (!force_pos && actor_at(where)
        && (mg.cls < NUM_MONSTERS || needs_resolution(mg.cls)))
    {
        const monster_type habitat_target =
            mg.cls == RANDOM_MONSTER ? MONS_BAT : mg.cls;
        where = find_newmons_square_contiguous(habitat_target, where, 0);
    }

    mg.pos = where;

    if (mons_class_is_zombified(mg.base_type))
    {
        if (mons_class_is_zombified(mg.cls))
            mg.base_type = MONS_NO_MONSTER;
        else
            swap(mg.base_type, mg.cls);
    }

    if (m_patrolling)
        mg.flags |= MG_PATROLLING;

    if (m_band)
        mg.flags |= MG_PERMIT_BANDS;

    // Store any extra flags here.
    mg.extra_flags |= mspec.extra_monster_flags;

    monster *mons = place_monster(mg, true, force_pos && where.origin());
    if (!mons)
        return 0;

    // Spells before items, so e.g. simulacrum casters can be given chunks.
    // add custom spell prop for spell display
    // XXX limited to the actual spellbook, won't display all
    // spellbooks available for vault monsters
    if (mspec.explicit_spells)
    {
        mons->spells = mspec.spells[random2(mspec.spells.size())];
        mons->props[CUSTOM_SPELLS_KEY] = true;
    }

    // this prop is mainly for the seed explorer.
    // not a great or complete measure of monster danger: the builder can roll
    // on the low side of the ood range, and vaults don't get this set.
    if (chose_ood)
        mons->props[MON_OOD_KEY].get_bool() = true;

    if (!mspec.items.empty())
        _dgn_give_mon_spec_items(mspec, mons);

    if (mspec.props.exists(MONSTER_TILE_KEY))
    {
        mons->props[MONSTER_TILE_KEY] =
            mspec.props[MONSTER_TILE_KEY].get_int();
    }
    if (mspec.props.exists(MONSTER_TILE_NAME_KEY))
    {
        mons->props[MONSTER_TILE_NAME_KEY].get_string() =
            mspec.props[MONSTER_TILE_NAME_KEY].get_string();
    }

    if (mspec.props.exists(ALWAYS_CORPSE_KEY))
        mons->props[ALWAYS_CORPSE_KEY] = true;

    if (mspec.props.exists(NEVER_CORPSE_KEY))
        mons->props[NEVER_CORPSE_KEY] = true;

    if (mspec.props.exists(DBNAME_KEY))
        mons->props[DBNAME_KEY].get_string() = mspec.props[DBNAME_KEY].get_string();

    // These are applied earlier to prevent issues with renamed monsters
    // and "<monster> comes into view" (see delay.cc:_monster_warning).
    //mons->flags |= mspec.extra_monster_flags;

    // Monsters with gods set by the spec aren't god gifts
    // unless they have the "god_gift" tag. place_monster(),
    // by default, marks any monsters with gods as god gifts,
    // so unmark them here.
    if (mspec.god != GOD_NO_GOD && !mspec.god_gift)
        mons->flags &= ~MF_GOD_GIFT;

    if (mons->is_priest() && mons->god == GOD_NO_GOD)
        mons->god = GOD_NAMELESS;

    if (mons_class_is_animated_weapon(mons->type))
    {
        item_def *wpn = mons->mslot_item(MSLOT_WEAPON);
        ASSERT(wpn);
        if (mons->type == MONS_DANCING_WEAPON)
            mons->ghost->init_dancing_weapon(*wpn, 100);
        else if (mons->type == MONS_SPECTRAL_WEAPON)
            mons->ghost->init_spectral_weapon(*wpn);
        mons->ghost_demon_init();
    }

    for (const mon_enchant &ench : mspec.ench)
        mons->add_ench(ench);

    return mons;
}

static bool _dgn_place_monster(const vault_placement &place, mons_spec &mspec,
                               const coord_def& where)
{
    const bool generate_awake
        = mspec.generate_awake
          || place.map.has_tag("generate_awake")
          || player_in_branch(BRANCH_GAUNTLET);

    const bool patrolling
        = mspec.patrolling || place.map.has_tag("patrolling");

    mspec.props[MAP_KEY].get_string() = place.map_name_at(where);
    return dgn_place_monster(mspec, where, false, generate_awake, patrolling);
}

static bool _dgn_place_one_monster(const vault_placement &place,
                                   mons_list &mons, const coord_def& where)
{
    for (int i = 0, size = mons.size(); i < size; ++i)
    {
        mons_spec spec = mons.get_monster(i);
        if (_dgn_place_monster(place, spec, where))
            return true;
    }
    return false;
}

/* "Oddball grids" are handled in _vault_grid. */
static dungeon_feature_type _glyph_to_feat(int glyph)
{
    return (glyph == 'x') ? DNGN_ROCK_WALL :
           (glyph == 'X') ? DNGN_PERMAROCK_WALL :
           (glyph == 'c') ? DNGN_STONE_WALL :
           (glyph == 'v') ? DNGN_METAL_WALL :
           (glyph == 'b') ? DNGN_CRYSTAL_WALL :
           (glyph == 'm') ? DNGN_CLEAR_ROCK_WALL :
           (glyph == 'n') ? DNGN_CLEAR_STONE_WALL :
           (glyph == 'o') ? DNGN_CLEAR_PERMAROCK_WALL :
           // We make 't' correspond to the right tree type by branch.
           (glyph == 't') ?
               player_in_branch(BRANCH_SWAMP)          ? DNGN_MANGROVE :
               player_in_branch(BRANCH_ABYSS)
               || player_in_branch(BRANCH_PANDEMONIUM) ? DNGN_DEMONIC_TREE
                                                       : DNGN_TREE :
           (glyph == '+') ? DNGN_CLOSED_DOOR :
           (glyph == '=') ? DNGN_RUNED_CLEAR_DOOR :
           (glyph == 'w') ? DNGN_DEEP_WATER :
           (glyph == 'W') ? DNGN_SHALLOW_WATER :
           (glyph == 'l') ? DNGN_LAVA :
           (glyph == '>') ? DNGN_ESCAPE_HATCH_DOWN :
           (glyph == '<') ? DNGN_ESCAPE_HATCH_UP :
           (glyph == '}') ? DNGN_STONE_STAIRS_DOWN_I :
           (glyph == '{') ? DNGN_STONE_STAIRS_UP_I :
           (glyph == ')') ? DNGN_STONE_STAIRS_DOWN_II :
           (glyph == '(') ? DNGN_STONE_STAIRS_UP_II :
           (glyph == ']') ? DNGN_STONE_STAIRS_DOWN_III :
           (glyph == '[') ? DNGN_STONE_STAIRS_UP_III :
           (glyph == 'A') ? DNGN_STONE_ARCH :
           (glyph == 'C') ? _pick_an_altar() :   // f(x) elsewhere {dlb}
           (glyph == 'I') ? DNGN_ORCISH_IDOL :
           (glyph == 'G') ? DNGN_GRANITE_STATUE :
           (glyph == 'T') ? DNGN_FOUNTAIN_BLUE :
           (glyph == 'U') ? DNGN_FOUNTAIN_SPARKLING :
           (glyph == 'V') ? DNGN_DRY_FOUNTAIN :
           (glyph == 'Y') ? DNGN_FOUNTAIN_BLOOD :
           (glyph == '\0')? DNGN_ROCK_WALL
                          : DNGN_FLOOR; // includes everything else
}

dungeon_feature_type map_feature_at(map_def *map, const coord_def &c,
                                    int rawfeat)
{
    if (rawfeat == -1)
        rawfeat = map->glyph_at(c);

    if (rawfeat == ' ')
        return NUM_FEATURES;

    keyed_mapspec *mapsp = map? map->mapspec_at(c) : nullptr;
    if (mapsp)
    {
        feature_spec f = mapsp->get_feat();
        if (f.trap)
        {
            if (f.trap->tr_type >= NUM_TRAPS)
                return DNGN_FLOOR;
            else
                return trap_feature(f.trap->tr_type);
        }
        else if (f.feat >= 0)
            return static_cast<dungeon_feature_type>(f.feat);
        else if (f.glyph >= 0)
            return map_feature_at(nullptr, c, f.glyph);
        else if (f.shop)
            return DNGN_ENTER_SHOP;

        return DNGN_FLOOR;
    }

    return _glyph_to_feat(rawfeat);
}

static void _vault_grid_mapspec(vault_placement &place, const coord_def &where,
                                keyed_mapspec& mapsp)
{
    const feature_spec f = mapsp.get_feat();
    if (f.trap)
        _place_specific_trap(where, f.trap.get(), 0);
    else if (f.feat >= 0)
        env.grid(where) = static_cast<dungeon_feature_type>(f.feat);
    else if (f.glyph >= 0)
        _vault_grid_glyph(place, where, f.glyph);
    else if (f.shop)
        place_spec_shop(where, *f.shop);
    else
        env.grid(where) = DNGN_FLOOR;

    if (f.mimic > 0 && one_chance_in(f.mimic))
    {
        ASSERT(feat_is_mimicable(env.grid(where), false));
        env.level_map_mask(where) |= MMT_MIMIC;
    }
    else if (f.no_mimic)
        env.level_map_mask(where) |= MMT_NO_MIMIC;

    item_list &items = mapsp.get_items();
    dgn_place_multiple_items(items, where);
}

static void _vault_grid_glyph(vault_placement &place, const coord_def& where,
                              int vgrid)
{
    // First, set base tile for grids {dlb}:
    if (vgrid != -1)
        env.grid(where) = _glyph_to_feat(vgrid);

    if (feat_is_altar(env.grid(where))
        && is_unavailable_god(feat_altar_god(env.grid(where))))
    {
        env.grid(where) = DNGN_FLOOR;
    }

    // then, handle oddball grids {dlb}:
    switch (vgrid)
    {
    case '@':
    case '=':
    case '+':
        if (_map_feat_is_on_edge(place, where))
            place.exits.push_back(where);
        break;
    case '^':
        place_specific_trap(where, TRAP_RANDOM);
        break;
    case 'B':
        env.grid(where) = _pick_temple_altar();
        break;
    }

    // Then, handle grids that place "stuff" {dlb}:
    if (vgrid == '$' || vgrid == '%' || vgrid == '*' || vgrid == '|')
    {
        int item_made = NON_ITEM;
        object_class_type which_class = OBJ_RANDOM;
        uint8_t which_type = OBJ_RANDOM;
        int which_depth = env.absdepth0;

        if (vgrid == '$')
            which_class = OBJ_GOLD;
        else if (vgrid == '|')
        {
            which_class = _superb_object_class();
            which_depth = ISPEC_GOOD_ITEM;
        }
        else if (vgrid == '*')
            which_depth = 5 + which_depth * 2;

        item_made = items(true, which_class, which_type, which_depth);
        if (item_made != NON_ITEM)
        {
            env.item[item_made].pos = where;
            env.level_map_mask(where) |= MMT_NO_TRAP;
            dprf(DIAG_DNGN, "vault grid: placing %s at %d,%d",
                env.item[item_made].name(DESC_PLAIN, false, true).c_str(),
                env.item[item_made].pos.x, env.item[item_made].pos.y);
        }
    }

    // defghijk - items
    if (map_def::valid_item_array_glyph(vgrid))
    {
        int slot = map_def::item_array_glyph_to_slot(vgrid);
        _dgn_place_item_explicit(slot, where, place);
    }
}

static void _vault_grid(vault_placement &place,
                        int vgrid,
                        const coord_def& where,
                        keyed_mapspec *mapsp)
{
    if (mapsp && mapsp->replaces_glyph())
        _vault_grid_mapspec(place, where, *mapsp);
    else
        _vault_grid_glyph(place, where, vgrid);

    if (cell_is_solid(where))
        delete_cloud(where);
}

static void _vault_grid_glyph_mons(vault_placement &place,
                                   const coord_def &where,
                                   int vgrid)
{
    // Handle grids that place monsters {dlb}:
    if (map_def::valid_monster_glyph(vgrid))
    {
        mons_spec ms(RANDOM_MONSTER);

        if (vgrid == '8')
            ms.type = RANDOM_SUPER_OOD;
        else if (vgrid == '9')
            ms.type = RANDOM_MODERATE_OOD;
        else if (vgrid != '0')
        {
            int slot = map_def::monster_array_glyph_to_slot(vgrid);
            ms = place.map.mons.get_monster(slot);
            monster_type mt = ms.type;
            // Is a map for a specific place trying to place a unique which
            // somehow already got created?
            if (!place.map.place.empty()
                && !invalid_monster_type(mt)
                && mons_is_unique(mt)
                && you.unique_creatures[mt])
            {
                mprf(MSGCH_ERROR, "ERROR: %s already generated somewhere "
                     "else; please file a bug report.",
                     mons_type_name(mt, DESC_THE).c_str());
                // Force it to be generated anyway.
                you.unique_creatures.set(mt, false);
            }
        }

        _dgn_place_monster(place, ms, where);
    }
}

static void _vault_grid_mapspec_mons(vault_placement &place,
                                     const coord_def &where,
                                     keyed_mapspec& mapsp)
{
    const feature_spec f = mapsp.get_feat();
    if (f.glyph >= 0)
        _vault_grid_glyph_mons(place, where, f.glyph);
    mons_list &mons = mapsp.get_monsters();
    _dgn_place_one_monster(place, mons, where);
}

static void _vault_grid_mons(vault_placement &place,
                        int vgrid,
                        const coord_def& where,
                        keyed_mapspec *mapsp)
{
    if (mapsp && mapsp->replaces_glyph())
        _vault_grid_mapspec_mons(place, where, *mapsp);
    else
        _vault_grid_glyph_mons(place, where, vgrid);
}

// Only used for Slime:$ where it will turn the stone walls into floor once the
// Royal Jelly has been killed, or at 6* Jiyva piety.
bool seen_destroy_feat(dungeon_feature_type old_feat)
{
    coord_def p1(0, 0);
    coord_def p2(GXM - 1, GYM - 1);

    bool seen = false;
    for (rectangle_iterator ri(p1, p2); ri; ++ri)
    {
        if (orig_terrain(*ri) == old_feat)
        {
            destroy_wall(*ri);
            if (you.see_cell(*ri))
                seen = true;
        }
    }

    return seen;
}

void dgn_replace_area(int sx, int sy, int ex, int ey,
                      dungeon_feature_type replace,
                      dungeon_feature_type feature,
                      unsigned mmask, bool needs_update)
{
    dgn_replace_area(coord_def(sx, sy), coord_def(ex, ey),
                      replace, feature, mmask, needs_update);
}

void dgn_replace_area(const coord_def& p1, const coord_def& p2,
                       dungeon_feature_type replace,
                       dungeon_feature_type feature, uint32_t mapmask,
                       bool needs_update)
{
    for (rectangle_iterator ri(p1, p2); ri; ++ri)
    {
        if (env.grid(*ri) == replace && !map_masked(*ri, mapmask))
        {
            env.grid(*ri) = feature;
            if (needs_update && env.map_knowledge(*ri).seen())
            {
                env.map_knowledge(*ri).set_feature(feature, 0,
                                                   get_trap_type(*ri));
#ifdef USE_TILE
                tile_env.bk_bg(*ri) = feature;
#endif
            }
        }
    }
}

bool map_masked(const coord_def &c, unsigned mask)
{
    return mask && (env.level_map_mask(c) & mask);
}

struct coord_comparator
{
    coord_def target;
    coord_comparator(const coord_def &t) : target(t) { }

    static int dist(const coord_def &a, const coord_def &b)
    {
        const coord_def del = a - b;
        return abs(del.x) + abs(del.y);
    }

    bool operator () (const coord_def &a, const coord_def &b) const
    {
        return dist(a, target) < dist(b, target);
    }
};

typedef set<coord_def, coord_comparator> coord_set;

static void _jtd_init_surrounds(coord_set &coords, uint32_t mapmask,
                                const coord_def &c)
{
    vector<coord_def> cur;
    for (orth_adjacent_iterator ai(c); ai; ++ai)
    {
        if (!in_bounds(*ai) || travel_point_distance[ai->x][ai->y]
            || map_masked(*ai, mapmask))
        {
            continue;
        }
        // randomize the order in which we visit orthogonal directions
        cur.insert(cur.begin() + random2(cur.size() + 1), *ai);
    }
    for (auto cc : cur)
    {
        coords.insert(cc);

        const coord_def dp = cc - c;
        // pack the orthogonal coord dp into an int in a slightly silly way.
        // 0, -1: 11
        // 0, 1:  9
        // -1, 0: 14
        // 1, 0:  6
        travel_point_distance[cc.x][cc.y] = (-dp.x + 2) * 4 + (-dp.y + 2);
    }
}

// #define DEBUG_JTD

// pathfinding from `from` to `to` using only orthogonal directions, and
// avoiding `mapmask`. Used by many layouts to ensure connectivity with
// rogue-ish hallways. Also used in some layouts where hallways aren't really
// the goal (e.g. shoals), there by a direct call to the pathfinding. Also used
// to connect to vault entrances.

// Resets travel_point_distance
vector<coord_def> dgn_join_the_dots_pathfind(const coord_def &from,
                                             const coord_def &to,
                                             uint32_t mapmask)
{
    // isolate this rng to keep layouts that use it more stable -- it has an
    // arbitrary number of draws, so is quite unpredictable otherwise
    rng::subgenerator jtd_rng;
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));
    const coord_comparator comp(to);
    coord_set coords(comp);

    vector<coord_def> path;
    coord_def curr = from;
    while (true)
    {
        int &tpd = travel_point_distance[curr.x][curr.y];
        // flip the sign of the path we are actually following. Used paths end
        // up negative, unused paths, positive. -1000 is the starting point.
        tpd = !tpd ? -1000 : -tpd;

        if (curr == to)
            break;

        _jtd_init_surrounds(coords, mapmask, curr);

        if (coords.empty())
            break;

        curr = *coords.begin();
        coords.erase(coords.begin());
    }

    if (curr != to)
        return path;

#ifdef DEBUG_JTD
    // dprfs the travel_point_distance grid found by the above loop
    string head;
    coord_def tpd_min(1000,1000);
    coord_def tpd_max(0,0);
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (travel_point_distance[ri->x][ri->y] != 0)
        {
            tpd_min.x = min(ri->x, tpd_min.x);
            tpd_min.y = min(ri->y, tpd_min.y);
            tpd_max.x = max(ri->x, tpd_max.x);
            tpd_max.y = max(ri->y, tpd_max.y);
        }
    }
    for (int ix = tpd_min.x; ix <= tpd_max.x; ix++)
        head += make_stringf("(%2d)", ix);
    dprf("/////%s", head.c_str()); // `/`s are there to prevent a split

    for (int iy = tpd_min.y; iy <= tpd_max.y; iy++)
    {
        string s;
        for (int ix = tpd_min.x; ix <= tpd_max.x; ix++)
        {
            s += (travel_point_distance[ix][iy] == -1000
                    ? "   X" // starting point
                    : make_stringf(" %3d", travel_point_distance[ix][iy]));
        }
        dprf("(%2d)%s", iy, s.c_str());
    }
#endif

    // traverse the path found above (indicated by negative values) *in reverse
    // order*.
    while (curr != from)
    {
        if (!map_masked(curr, mapmask))
        {
            path.push_back(curr);
#ifdef DEBUG_JTD
            env.pgrid(curr) |= FPROP_HIGHLIGHT;
#endif
        }

        const int dist = travel_point_distance[curr.x][curr.y];
        ASSERT(dist < 0);
        ASSERT(dist != -1000);
        // unpack the (orthogonal) direction to move from an int. There's an
        // intentional sign flip here since are moving in reverse. Note that
        // this also relies on some silly int rounding to simplify reversing
        // the packing formula from _jtd_init_surrounds.
        // original => packed => unpacked
        // 0, -1    => 11     => 0,1
        // 0, 1     => 9      => 0,-1
        // -1, 0    => 14     => 1,0
        // 1, 0     => 6      => -1,0
        curr += coord_def(-dist / 4 - 2, (-dist % 4) - 2);

    }
    if (!map_masked(curr, mapmask))
    {
#ifdef DEBUG_JTD
        env.pgrid(curr) |= FPROP_HIGHLIGHT;
#endif
        path.push_back(curr);
    }

    return path;
}

bool join_the_dots(const coord_def &from, const coord_def &to,
                   uint32_t mapmask,
                   bool (*overwritable)(dungeon_feature_type))
{
    if (!overwritable)
        overwritable = _feat_is_wall_floor_liquid;

    const vector<coord_def> path =
        dgn_join_the_dots_pathfind(from, to, mapmask);

    for (auto c : path)
    {
        auto feat = env.grid(c);
        if (!map_masked(c, mapmask) && overwritable(feat))
        {
            env.grid(c) = DNGN_FLOOR;
            dgn_height_set_at(c);
        }
        else
        {
            dprf(DIAG_DNGN, "Failed to path through %s at (%d;%d) for connectivity",
                 get_feature_def(feat).name, c.x, c.y);
        }
    }

    return !path.empty() || from == to;
}

static dungeon_feature_type _pick_temple_altar()
{
    if (_temple_altar_list.empty())
    {
        if (_current_temple_hash != nullptr)
        {
            // Altar god doesn't matter, setting up the whole machinery would
            // be too much work.
            if (crawl_state.map_stat_gen || crawl_state.obj_stat_gen
                || you.props.exists(FORCE_MAP_KEY))
            {
                return DNGN_ALTAR_XOM;
            }

            mprf(MSGCH_ERROR, "Ran out of altars for temple!");
            return DNGN_FLOOR;
        }
        // Randomized altar list for mini-temples.
        _temple_altar_list = temple_god_list();
        shuffle_array(_temple_altar_list);
    }

    const god_type god = _temple_altar_list.back();

    _temple_altar_list.pop_back();

    return altar_for_god(god);
}

//jmf: Generate altar based on where you are, or possibly randomly.
static dungeon_feature_type _pick_an_altar()
{
    god_type god;

    // No extra altars in Temple.
    if (player_in_branch(BRANCH_TEMPLE))
        god = GOD_NO_GOD;

    // Xom can turn up anywhere
    else if (one_chance_in(27))
        god = GOD_XOM;
    else
    {
        switch (you.where_are_you)
        {
        case BRANCH_CRYPT:
            god = random_choose(GOD_KIKUBAAQUDGHA, GOD_YREDELEMNUL);
            break;

        case BRANCH_ORC: // There are a few heretics
            if (one_chance_in(5))
                god = random_choose(GOD_TROG, GOD_MAKHLEB, GOD_VEHUMET);
            else
                god = GOD_BEOGH;
            break;

        case BRANCH_ELF: // magic gods
            god = random_choose(GOD_VEHUMET, GOD_SIF_MUNA, GOD_KIKUBAAQUDGHA);
            break;

       case BRANCH_SNAKE: // barding god, slow god, treasure hoard god
            god = random_choose(GOD_OKAWARU, GOD_CHEIBRIADOS, GOD_GOZAG);
            break;

        case BRANCH_SLIME:
            god = GOD_JIYVA;
            break;

        case BRANCH_TOMB:
            god = GOD_KIKUBAAQUDGHA;
            break;

        case BRANCH_VESTIBULE:
        case BRANCH_DIS:
        case BRANCH_GEHENNA:
        case BRANCH_COCYTUS:
        case BRANCH_TARTARUS:  // particularly destructive / elemental gods
            if (one_chance_in(9))
            {
                god = random_choose(GOD_KIKUBAAQUDGHA, GOD_NEMELEX_XOBEH,
                                    GOD_QAZLAL, GOD_VEHUMET);
            }
            else
                god = GOD_MAKHLEB;
            break;

        case BRANCH_PANDEMONIUM: // DS enemy gods + the & summoner
            god = random_choose(GOD_KIKUBAAQUDGHA, GOD_LUGONU, GOD_TROG,
                                GOD_MAKHLEB, GOD_NEMELEX_XOBEH);
            break;

        default: // Any temple-valid god
            auto temple_gods = temple_god_list();
            god = *random_iterator(temple_gods);
            break;
        }
    }

    if (is_unavailable_god(god))
        god = GOD_NO_GOD;

    return altar_for_god(god);
}

void place_spec_shop(const coord_def& where, shop_type force_type)
{
    shop_spec spec(force_type);
    place_spec_shop(where, spec);
}

int greed_for_shop_type(shop_type shop, int level_number)
{
    if (!shoptype_identifies_stock(shop))
    {
        const int rand = random2avg(19, 2);
        return 15 + rand + random2(level_number);
    }
    const int rand = random2(5);
    return 10 + rand + random2(level_number / 2);
}

/**
 * How greedy should a given shop be? (Applies a multiplier to prices.)
 *
 * @param type              The type of the shop. (E.g. SHOP_WEAPON_ANTIQUE.)
 * @param level_number      The depth in which the shop is placed.
 * @param spec_greed        An override for the greed, based on a vault
 *                          specification; if not -1, will override other
 *                          calculations & give a debug message.
 * @return                  The greed for the shop.
 */
static int _shop_greed(shop_type type, int level_number, int spec_greed)
{
    const int base_greed = greed_for_shop_type(type, level_number);
    int adj_greed = base_greed;

    // Allow bargains in bazaars, prices randomly between 60% and 95%.
    if (player_in_branch(BRANCH_BAZAAR))
    {
        // divided by 20, so each is 5% of original price
        // 12-19 = 60-95%, per above
        const int factor = random2(8) + 12;

        dprf(DIAG_DNGN, "Shop type %d: original greed = %d, factor = %d,"
             " discount = %d%%.",
             type, base_greed, factor, (20-factor)*5);

        adj_greed = factor * adj_greed / 20;
    }

    if (spec_greed != -1)
    {
        dprf(DIAG_DNGN, "Shop spec overrides greed: %d becomes %d.",
             adj_greed, spec_greed);
        return spec_greed;
    }

    return adj_greed;
}

/**
 * How many items should be placed in a given shop?
 *
 * @param spec              A vault shop spec; may override default results.
 * @return                  The number of items the shop should be generated
 *                          to hold.
 */
static int _shop_num_items(const shop_spec &spec)
{
    if (spec.num_items != -1)
    {
        dprf(DIAG_DNGN, "Shop spec overrides number of items to %d.",
             spec.num_items);
        return spec.num_items;
    }

    if (spec.use_all && !spec.items.empty())
    {
        dprf(DIAG_DNGN, "Shop spec wants all items placed: %u of them.",
             (unsigned int)spec.items.size());
        return (int) spec.items.size();
    }

    return 5 + random2avg(8, 3);
}

/**
 * What 'level' should an item from the given shop type be generated at?
 *
 * @param shop_type_        The type of shop the item is to be sold from.
 * @param level_number      The depth of the level the shop is on.
 * @return                  An "item level" to generate an item at.
 */
static int _choose_shop_item_level(shop_type shop_type_, int level_number)
{
    const int shop_multiplier = shoptype_identifies_stock(shop_type_) ? 2 : 3;
    const int base_level = level_number
                            + random2((level_number + 1) * shop_multiplier);

    // Make bazaar items more valuable (up to double value).
    if (!player_in_branch(BRANCH_BAZAAR))
        return base_level;

    const int bazaar_bonus = random2(base_level) + 1;
    return min(base_level + bazaar_bonus, level_number * 5);
}

/**
 * Is the given item valid for placement in the given shop?
 *
 * @param item_index    An index into env.item; may be NON_ITEM.
 * @param shop_type_    The type of shop being generated.
 * @param spec          The specification for the shop.
 * @return              Whether the item is valid.
 */
static bool _valid_item_for_shop(int item_index, shop_type shop_type_,
                                 shop_spec &spec)
{
    if (item_index == NON_ITEM)
        return false;

    const item_def &item = env.item[item_index];
    ASSERT(item.defined());

    // Don't generate gold in shops! This used to be possible with
    // general stores (GDL)
    if (item.base_type == OBJ_GOLD)
        return false;

    // Don't place missiles or books in general antique shops...
    if (shop_type_ == SHOP_GENERAL_ANTIQUE
            && (item.base_type == OBJ_MISSILES
                || item.base_type == OBJ_BOOKS))
    {
        // ...unless they're specified by the item spec.
        return !spec.items.empty();
    }

    return true;
}

/**
 * Create an item and place it in a shop.
 *
 * FIXME: I'm pretty sure this will go into an infinite loop if env.item is full.
 * items() uses get_mitm_slot with culling, so i think this is ok --wheals
 *
 * @param j                 The index of the item being created in the shop's
 *                          inventory.
 * @param shop_type_        The type of shop. (E.g. SHOP_WEAPON_ANTIQUE.)
 * @param stocked[in,out]   An array mapping book types to the # in the shop.
 * @param spec              The specification of the shop.
 * @param shop              The shop.
 * @param shop_level        The effective depth to use for the shop.
 */
static void _stock_shop_item(int j, shop_type shop_type_,
                             int stocked[NUM_BOOKS],
                             shop_spec &spec, shop_struct &shop,
                             int shop_level)
{
    const int level_number = shop_level ? shop_level : env.absdepth0;
    const int item_level = _choose_shop_item_level(shop_type_, level_number);

    int item_index; // index into env.item (global item array)
                    // where the generated item will be stored

    // XXX: this scares the hell out of me. should it be a for (...1000)?
    // also, it'd be nice if it was just a function that returned an
    // item index, maybe
    while (true)
    {
        object_class_type basetype = item_in_shop(shop_type_);
        int subtype = OBJ_RANDOM;

        if (!spec.items.empty() && !spec.use_all)
        {
            // shop spec lists a random set of items; choose one
            item_index = dgn_place_item(spec.items.random_item_weighted(),
                                        coord_def(), item_level);
        }
        else if (!spec.items.empty() && spec.use_all
                 && j < (int)spec.items.size())
        {
            // shop lists ordered items; take the one at the right index
            item_index = dgn_place_item(spec.items.get_item(j), coord_def(),
                                        item_level);
        }
        else
        {
            // make an item randomly
            // gozag shop items are better
            const bool good_item = spec.gozag || one_chance_in(4);
            const int level = good_item ? ISPEC_GOOD_ITEM : item_level;
            item_index = items(true, basetype, subtype, level);
        }

        // Try for a better selection for bookshops.
        if (item_index != NON_ITEM && shop_type_ == SHOP_BOOK)
        {
            // if this book type is already in the shop, maybe discard it
            if (!one_chance_in(stocked[env.item[item_index].sub_type] + 1))
            {
                env.item[item_index].clear();
                item_index = NON_ITEM; // try again
            }
        }

        if (_valid_item_for_shop(item_index, shop_type_, spec))
            break;

        // Reset object and try again.
        if (item_index != NON_ITEM)
            env.item[item_index].clear();
    }

    ASSERT(item_index != NON_ITEM);

    item_def item = env.item[item_index];

    // If this is a book, note it down in the stocked books array
    // (unless it's a randbook)
    if (shop_type_ == SHOP_BOOK && !is_artefact(item))
        stocked[item.sub_type]++;

    // Identify the item, unless we don't do that.
    if (shoptype_identifies_stock(shop_type_))
        item.flags |= ISFLAG_IDENTIFIED;

    // Now move it into the shop!
    dec_mitm_item_quantity(item_index, item.quantity, false);
    item.pos = shop.pos;
    item.link = ITEM_IN_SHOP;
    shop.stock.push_back(item);
    dprf(DIAG_DNGN, "shop spec: placing %s",
                    item.name(DESC_PLAIN, false, true).c_str());
}

static shop_type _random_shop()
{
    return random_choose(SHOP_WEAPON, SHOP_ARMOUR, SHOP_WEAPON_ANTIQUE,
                         SHOP_ARMOUR_ANTIQUE, SHOP_GENERAL_ANTIQUE,
                         SHOP_JEWELLERY, SHOP_BOOK,
                         SHOP_DISTILLERY, SHOP_SCROLL, SHOP_GENERAL);
}


/**
 * Attempt to place a shop in a given location.
 *
 * @param where             The location to place the shop.
 * @param spec              The details of the shop.
 *                          Would be const if not for list method nonsense.
 * @param shop_level        The effective depth to use for the shop.

 */
void place_spec_shop(const coord_def& where, shop_spec &spec, int shop_level)
{
    rng::subgenerator shop_rng; // isolate shop rolls from levelgen
    no_notes nx;

    shop_struct& shop = env.shop[where];

    const int level_number = shop_level ? shop_level : env.absdepth0;

    for (int j = 0; j < 3; j++)
        shop.keeper_name[j] = 1 + random2(200);
    shop.shop_name = spec.name;
    shop.shop_type_name = spec.type;
    shop.shop_suffix_name = spec.suffix;
    shop.level = level_number * 2;
    shop.type = spec.sh_type;
    if (shop.type == SHOP_RANDOM)
        shop.type = _random_shop();
    shop.greed = _shop_greed(shop.type, level_number, spec.greed);
    shop.pos = where;

    _set_grd(where, DNGN_ENTER_SHOP);

    const int num_items = _shop_num_items(spec);

    // For books shops, store how many copies of a given book are on display.
    // This increases the diversity of books in a shop.
    int stocked[NUM_BOOKS] = { 0 };

    shop.stock.clear();
    for (int j = 0; j < num_items; j++)
        _stock_shop_item(j, shop.type, stocked, spec, shop, shop_level);
}

object_class_type item_in_shop(shop_type shop_type)
{
    switch (shop_type)
    {
    case SHOP_WEAPON:
        if (one_chance_in(5))
            return OBJ_MISSILES;
        // *** deliberate fall through here  {dlb} ***
    case SHOP_WEAPON_ANTIQUE:
        return OBJ_WEAPONS;

    case SHOP_ARMOUR:
    case SHOP_ARMOUR_ANTIQUE:
        return OBJ_ARMOUR;

    case SHOP_GENERAL:
    case SHOP_GENERAL_ANTIQUE:
        return OBJ_RANDOM;

    case SHOP_JEWELLERY:
        if (one_chance_in(10))
            return OBJ_TALISMANS;
        return OBJ_JEWELLERY;

    case SHOP_BOOK:
        return OBJ_BOOKS;

    case SHOP_DISTILLERY:
        return OBJ_POTIONS;

    case SHOP_SCROLL:
        return OBJ_SCROLLS;

    default:
        die("unknown shop type %d", shop_type);
    }

    return OBJ_RANDOM;
}

// Keep seeds away from the borders so we don't end up with a
// straight wall.
static bool _spotty_seed_ok(const coord_def& p)
{
    const int margin = 4;
    return p.x >= margin && p.y >= margin
           && p.x < GXM - margin && p.y < GYM - margin;
}

static bool _feat_is_wall_floor_liquid(dungeon_feature_type feat)
{
    return feat_is_water(feat)
           || feat_is_tree(feat)
           || feat_is_lava(feat)
           || feat_is_wall(feat)
           || feat == DNGN_FLOOR;
}

// Connect vault exit "from" to dungeon floor by growing a spotty chamber.
// This tries to be like _spotty_level, but probably isn't quite.
// It might be better to aim for a more open connection -- currently
// it stops pretty much as soon as connectivity is attained.
static set<coord_def> _dgn_spotty_connect_path(const coord_def& from,
            bool (*overwritable)(dungeon_feature_type))
{
    set<coord_def> flatten;
    set<coord_def> border;
    bool success = false;

    if (!overwritable)
        overwritable = _feat_is_wall_floor_liquid;

    for (adjacent_iterator ai(from); ai; ++ai)
        if (!map_masked(*ai, MMT_VAULT) && _spotty_seed_ok(*ai))
            border.insert(*ai);

    while (!success && !border.empty())
    {
        auto it = random_iterator(border);
        coord_def cur = *it;
        border.erase(it);

        // Flatten orthogonal neighbours, and add new neighbours to border.
        flatten.insert(cur);
        for (orth_adjacent_iterator ai(cur); ai; ++ai)
        {
            if (map_masked(*ai, MMT_VAULT))
                continue;

            if (env.grid(*ai) == DNGN_FLOOR)
                success = true; // Through, but let's remove the others, too.

            if (!overwritable(env.grid(*ai)) || flatten.count(*ai))
                continue;

            flatten.insert(*ai);
            for (adjacent_iterator bi(*ai); bi; ++bi)
            {
                if (!map_masked(*bi, MMT_VAULT)
                    && _spotty_seed_ok(*bi)
                    && !flatten.count(*bi))
                {
                    border.insert(*bi);
                }
            }
        }
    }

    if (!success)
        flatten.clear();

    return flatten;
}

static bool _connect_spotty(const coord_def& from,
                            bool (*overwritable)(dungeon_feature_type))
{
    const set<coord_def> spotty_path =
        _dgn_spotty_connect_path(from, overwritable);

    if (!spotty_path.empty())
    {
        for (auto c : spotty_path)
        {
            env.grid(c) = (player_in_branch(BRANCH_SWAMP) && one_chance_in(3))
                   ? DNGN_SHALLOW_WATER
                   : DNGN_FLOOR;
            dgn_height_set_at(c);
        }
    }

    return !spotty_path.empty();
}

void place_specific_trap(const coord_def& where, trap_type spec_type, int charges)
{
    trap_spec spec(spec_type);

    _place_specific_trap(where, &spec, charges);
}

static void _place_specific_trap(const coord_def& where, trap_spec* spec,
                                 int charges)
{
    trap_type spec_type = spec->tr_type;

    if (spec_type == TRAP_SHAFT && !is_valid_shaft_level())
    {
        mprf(MSGCH_ERROR, "Vault %s tried to place a shaft at a branch end",
                env.placing_vault.c_str());
    }

    // find an appropriate trap for TRAP_RANDOM
    if (spec_type == TRAP_RANDOM)
    {
        do
        {
            spec_type = static_cast<trap_type>(random2(NUM_TRAPS));
        }
        while (!is_regular_trap(spec_type)
               || !is_valid_shaft_level() && spec_type == TRAP_SHAFT);
    }

    trap_def t;
    t.type = spec_type;
    t.pos = where;
    env.grid(where) = trap_feature(spec_type);
    t.prepare_ammo(charges);
    env.trap[where] = t;
    dprf("placed %s trap", article_a(trap_name(spec_type)).c_str());
}

/**
 * Sprinkle plants around the level.
 *
 * @param rarity            1/chance of placing clumps in any given place.
 * @param clump_density     1/chance of placing more plants within a clump.
 * @param clump_raidus      Radius of plant clumps.
 */
static void _add_plant_clumps(int rarity,
                              int clump_sparseness,
                              int clump_radius)
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        mgen_data mg;
        mg.flags = MG_FORCE_PLACE;
        if (env.mgrid(*ri) != NON_MONSTER && !map_masked(*ri, MMT_VAULT))
        {
            // clump plants around things that already exist
            monster_type type = env.mons[env.mgrid(*ri)].type;
            if ((type == MONS_PLANT
                     || type == MONS_FUNGUS
                     || type == MONS_BUSH)
                 && one_chance_in(rarity))
            {
                mg.cls = type;
            }
            else
                continue;
        }
        else
            continue;

        vector<coord_def> to_place;
        to_place.push_back(*ri);
        for (int i = 1; i < clump_radius; ++i)
        {
            for (radius_iterator rad(*ri, i, C_ROUND); rad; ++rad)
            {
                if (env.grid(*rad) != DNGN_FLOOR)
                    continue;

                // make sure the iterator stays valid
                vector<coord_def> more_to_place;
                for (auto c : to_place)
                {
                    if (*rad == c)
                        continue;
                    // only place plants next to previously placed plants
                    if (abs(rad->x - c.x) <= 1 && abs(rad->y - c.y) <= 1)
                    {
                        if (one_chance_in(clump_sparseness))
                            more_to_place.push_back(*rad);
                    }
                }
                to_place.insert(to_place.end(), more_to_place.begin(),
                                more_to_place.end());
            }
        }

        for (auto c : to_place)
        {
            if (c == *ri)
                continue;
            if (plant_forbidden_at(c))
                continue;
            mg.pos = c;
            mons_place(mgen_data(mg));
        }
    }
}

static coord_def _find_named_hatch_dest(string hatch_name)
{
    vector <map_marker *> markers;
    markers = find_markers_by_prop(HATCH_DEST_NAME_PROP, hatch_name);
    ASSERT(markers.size() == 1);
    return markers[0]->pos;
}

static coord_def _get_feat_dest(coord_def base_pos, dungeon_feature_type feat,
                                const string &hatch_name)
{
    const bool shaft = feat == DNGN_TRAP_SHAFT;
    map_position_marker *marker = nullptr;

    if (!shaft)
        marker = get_position_marker_at(base_pos, feat);

    if (!marker)
    {
        coord_def dest_pos;

        if (feat_is_escape_hatch(feat) && !hatch_name.empty())
            dest_pos = _find_named_hatch_dest(hatch_name);
        else
        {
            do
            {
                dest_pos = random_in_bounds();
            }
            while (env.grid(dest_pos) != DNGN_FLOOR
                   || env.pgrid(dest_pos) & FPROP_NO_TELE_INTO);
        }

        if (!shaft)
        {
            env.markers.add(new map_position_marker(base_pos, feat, dest_pos));
            env.markers.clear_need_activate();
        }
        return dest_pos;
    }
    else
        return marker->dest;
}

double dgn_degrees_to_radians(int degrees)
{
    return degrees * PI / 180;
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
        if (map_bounds_with_margin(res, margin))
            return res;
    }
    return coord_def();
}

coord_def dgn_find_feature_marker(dungeon_feature_type feat)
{
    for (map_marker *mark : env.markers.get_all(MAT_FEATURE))
        if (dynamic_cast<map_feature_marker*>(mark)->feat == feat)
            return mark->pos;
    return coord_def();
}

// Make hatches and shafts land the player a bit away from the wall.
// Specifically, the adjacent cell with least slime walls next to it.
// XXX: This can still give bad situations if the layout is not bubbly,
//      e.g. when a vault is placed with connecting corridors.
static void _fixup_slime_hatch_dest(coord_def* pos)
{
    int max_walls = 9;
    for (adjacent_iterator ai(*pos, false); ai; ++ai)
    {
        if (!feat_is_traversable(env.grid(*ai)))
            continue;
        const int walls = count_adjacent_slime_walls(*ai);
        if (walls < max_walls)
        {
            *pos = *ai;
            max_walls = walls;
        }
    }
    ASSERT(max_walls < 9);
}

coord_def dgn_find_nearby_stair(dungeon_feature_type stair_to_find,
                                coord_def base_pos, bool find_closest,
                                string hatch_name)
{
    dprf(DIAG_DNGN, "Level entry point on %sstair: %d (%s)",
         find_closest ? "closest " : "",
         stair_to_find, dungeon_feature_name(stair_to_find));

    // Shafts and hatches.
    if (stair_to_find == DNGN_ESCAPE_HATCH_UP
        || stair_to_find == DNGN_ESCAPE_HATCH_DOWN
        || stair_to_find == DNGN_TRAP_SHAFT)
    {
        coord_def pos(_get_feat_dest(base_pos, stair_to_find, hatch_name));
        if (player_in_branch(BRANCH_SLIME))
            _fixup_slime_hatch_dest(&pos);
        if (in_bounds(pos))
            return pos;
    }

    if (stair_to_find == DNGN_STONE_ARCH)
    {
        const coord_def pos(dgn_find_feature_marker(stair_to_find));
        if (in_bounds(pos) && env.grid(pos) == stair_to_find)
            return pos;
    }

    if (stair_to_find == your_branch().exit_stairs)
    {
        const coord_def pos(dgn_find_feature_marker(DNGN_STONE_STAIRS_UP_I));
        if (in_bounds(pos) && env.grid(pos) == stair_to_find)
            return pos;
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
    for (int xcode = 0; xcode < GXM; ++xcode)
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

            if (orig_terrain(coord_def(xpos, ypos)) == stair_to_find
                && !feature_mimic_at(coord_def(xpos, ypos)))
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
                else if (one_chance_in(found))
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
    for (int xcode = 0; xcode < GXM; ++xcode)
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
            const int looking_at = orig_terrain(coord_def(xpos, ypos));

            if (feat_is_stone_stair_down(stair_to_find)
                || stair_to_find == DNGN_ESCAPE_HATCH_DOWN
                || stair_to_find == DNGN_FLOOR)
            {
                good_stair = feat_is_stone_stair_down((dungeon_feature_type)looking_at)
                             || looking_at == DNGN_ESCAPE_HATCH_DOWN;
            }
            else
            {
                good_stair = feat_is_stone_stair_up((dungeon_feature_type)looking_at)
                              || looking_at == DNGN_ESCAPE_HATCH_UP;
            }

            const int dist = (xpos-basex)*(xpos-basex)
                             + (ypos-basey)*(ypos-basey);

            if (good_stair && !feature_mimic_at(coord_def(xpos, ypos)))
            {
                found++;
                if (find_closest && dist < best_dist)
                {
                    best_dist = dist;
                    result.x = xpos;
                    result.y = ypos;
                }
                else if (one_chance_in(found))
                {
                    result.x = xpos;
                    result.y = ypos;
                }
            }
        }
    }

    if (found)
        return result;

    const coord_def pos(dgn_find_feature_marker(stair_to_find));
    if (in_bounds(pos))
        return pos;

    // Look for any clear terrain and abandon the idea of looking nearby now.
    // This is used when taking transit Pandemonium gates. Currently the player
    // can land in vaults, which is considered acceptable.
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (feat_has_dry_floor(env.grid(*ri))
            && !(env.pgrid(*ri) & FPROP_NO_TELE_INTO))
        {
            found++;
            if (one_chance_in(found))
                result = *ri;
        }
    }
    if (found)
        return result;

    // FAIL
    die("Can't find any floor to put the player on.");
}

void dgn_set_branch_epilogue(branch_type branch, string func_name)
{
    branch_epilogues[branch] = func_name;
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
    for (auto reg : regions)
        if (overlaps(reg))
            return true;

    return false;
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
                return true;
        }

    return false;
}

coord_def dgn_region::random_edge_point() const
{
    coord_def res;
    if (x_chance_in_y(size.x, size.x + size.y))
    {
        res.x = pos.x + random2(size.x);
        res.y = random_choose(pos.y, pos.y + size.y - 1);
    }
    else
    {
        res.x = random_choose(pos.x, pos.x + size.x - 1);
        res.y = pos.y + random2(size.y);
    }
    return res;
}

coord_def dgn_region::random_point() const
{
    coord_def res;
    res.x = pos.x + random2(size.x);
    res.y = pos.y + random2(size.y);
    return res;
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
        connected[0] = unmarshallBoolean(th);
        connected[1] = unmarshallBoolean(th);
        connected[2] = unmarshallBoolean(th);
    }

    void write(writer &th)
    {
        marshallByte(th, region[0]);
        marshallByte(th, region[1]);
        marshallByte(th, region[2]);
        marshallBoolean(th, connected[0]);
        marshallBoolean(th, connected[1]);
        marshallBoolean(th, connected[2]);
    }

    int8_t region[3];
    bool connected[3];
};

FixedVector<vector<StairConnectivity>, NUM_BRANCHES> connectivity;

void init_level_connectivity()
{
    for (branch_iterator it; it; ++it)
    {
        int depth = brdepth[it->id] > 0 ? brdepth[it->id] : 0;
        connectivity[it->id].resize(depth);
    }
}

void read_level_connectivity(reader &th)
{
    int nb = unmarshallInt(th);
    ASSERT(nb <= NUM_BRANCHES);
    for (int i = 0; i < nb; i++)
    {
        unsigned int depth = brdepth[i] > 0 ? brdepth[i] : 0;
        unsigned int num_entries = unmarshallInt(th);
        connectivity[i].resize(max(depth, num_entries));

        for (unsigned int e = 0; e < num_entries; e++)
            connectivity[i][e].read(th);
    }
}

void write_level_connectivity(writer &th)
{
    marshallInt(th, NUM_BRANCHES);
    for (int i = 0; i < NUM_BRANCHES; i++)
    {
        marshallInt(th, connectivity[i].size());
        for (unsigned int e = 0; e < connectivity[i].size(); e++)
            connectivity[i][e].write(th);
    }
}

static bool _fixup_interlevel_connectivity()
{
    // Rotate the stairs on this level to attempt to preserve connectivity
    // as much as possible. At a minimum, it ensures a path from the bottom
    // of a branch to the top of a branch. If this is not possible, it
    // returns false.
    //
    // Note: this check is undirectional and assumes that levels below this
    // one have not been created yet. If this is not the case, it will not
    // guarantee or preserve connectivity.
    //
    // XXX: If successful, the previous level's connectedness information
    //      is updated, so we rely on the level not being vetoed after
    //      this check.

    if (!player_in_connected_branch() || brdepth[you.where_are_you] == -1)
        return true;
    if (branches[you.where_are_you].branch_flags & brflag::islanded)
        return true;

    StairConnectivity prev_con;
    if (you.depth > 1)
        prev_con = connectivity[your_branch().id][you.depth - 2];
    StairConnectivity this_con;

    FixedVector<coord_def, 3> up_gc;
    FixedVector<coord_def, 3> down_gc;
    FixedVector<int, 3> up_region;
    FixedVector<int, 3> down_region;
    FixedVector<bool, 3> has_down;
    vector<bool> region_connected;

    up_region[0] = up_region[1] = up_region[2] = -1;
    down_region[0] = down_region[1] = down_region[2] = -1;
    has_down[0] = has_down[1] = has_down[2] = false;

    // Find up stairs and down stairs on the current level.
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));
    int nzones = 0;
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (!map_bounds(ri->x, ri->y)
            || travel_point_distance[ri->x][ri->y]
            || !dgn_square_travel_ok(*ri))
        {
            continue;
        }

        _dgn_fill_zone(*ri, ++nzones, _dgn_point_record_stub,
                       dgn_square_travel_ok, nullptr);
    }

    int max_region = 0;
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (feature_mimic_at(*ri))
            continue;

        dungeon_feature_type feat = env.grid(*ri);
        switch (feat)
        {
        case DNGN_STONE_STAIRS_DOWN_I:
        case DNGN_STONE_STAIRS_DOWN_II:
        case DNGN_STONE_STAIRS_DOWN_III:
        {
            int idx = feat - DNGN_STONE_STAIRS_DOWN_I;
            if (down_region[idx] == -1)
            {
                down_region[idx] = travel_point_distance[ri->x][ri->y];
                down_gc[idx] = *ri;
                max_region = max(down_region[idx], max_region);
            }
            else
            {
                // Too many stairs!
                return false;
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
                up_region[idx] = travel_point_distance[ri->x][ri->y];
                up_gc[idx] = *ri;
                max_region = max(up_region[idx], max_region);
            }
            else
            {
                // Too many stairs!
                return false;
            }
            break;
        }
        default:
            break;
        }
    }

    const int up_region_max = (you.depth == 1 || player_in_hell()) ? 1 : 3;

    // Ensure all up stairs were found.
    for (int i = 0; i < up_region_max; i++)
        if (up_region[i] == -1)
            return false;

    region_connected.resize(max_region + 1);
    fill(begin(region_connected), end(region_connected), false);

    // Which up stairs have a down stair? (These are potentially connected.)
    if (!at_branch_bottom())
    {
        for (int i = 0; i < up_region_max; i++)
            for (int j = 0; j < 3; j++)
            {
                if (down_region[j] == up_region[i])
                    has_down[i] = true;
            }
    }

    bool any_connected = has_down[0] || has_down[1] || has_down[2];
    if (!any_connected && !at_branch_bottom())
        return false;

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
        for (int j = 0; j < up_region_max; j++)
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
        return false;

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
        for (int j = 0; j < up_region_max; j++)
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

        for (int j = 0; j < up_region_max; j++)
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
        for (int j = 0; j < up_region_max; j++)
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
        for (int i = 0; i < up_region_max; i++)
            if (!region_connected[up_region[i]])
                return false;
    }

    // Sanity check that we're not duplicating stairs.
    if (up_region_max > 1)
    {
        bool stairs_unique = (assign_cur[0] != assign_cur[1]
                              && assign_cur[1] != assign_cur[2]);
        ASSERT(stairs_unique);
        if (!stairs_unique)
            return false;
    }

    // Reassign up stair numbers as needed.
    for (int i = 0; i < up_region_max; i++)
    {
        _set_grd(up_gc[i],
            (dungeon_feature_type)(DNGN_STONE_STAIRS_UP_I + assign_cur[i]));
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

    // Save the connectivity.
    if (you.depth > 1)
        connectivity[your_branch().id][you.depth - 2] = prev_con;
    connectivity[your_branch().id][you.depth - 1] = this_con;

    return true;
}

void run_map_epilogues()
{
    // Iterate over level vaults and run each map's epilogue.
    for (auto &vault : env.level_vaults)
        vault->map.run_lua_epilogue();
}

//////////////////////////////////////////////////////////////////////////
// vault_placement

vault_placement::vault_placement()
    : pos(-1, -1), size(0, 0), orient(MAP_NONE), map(), exits(), seen(false)
{
}

string vault_placement::map_name_at(const coord_def &where) const
{
    const coord_def offset = where - pos;
    return map.name_at(offset);
}

void vault_placement::reset()
{
    if (_current_temple_hash != nullptr)
        _setup_temple_altars(*_current_temple_hash);
    else
        _temple_altar_list.clear();
}

void vault_placement::apply_grid()
{
    if (!size.zero())
    {
        bool clear = !map.has_tag("overwrite_floor_cell");

        // NOTE: assumes *no* previous item (I think) or monster (definitely)
        // placement.
        for (rectangle_iterator ri(pos, pos + size - 1); ri; ++ri)
        {
            if (map.is_overwritable_layout() && map_masked(*ri, MMT_VAULT))
                continue;

            const coord_def &rp(*ri);
            const coord_def dp = rp - pos;

            const int feat = map.map.glyph(dp);

            if (feat == ' ')
                continue;

            const dungeon_feature_type oldgrid = env.grid(*ri);

            if (clear)
            {
                env.grid_colours(*ri) = 0;
                env.pgrid(*ri) = terrain_property_t{};
                // what about heightmap?
                tile_clear_flavour(*ri);
            }

            keyed_mapspec *mapsp = map.mapspec_at(dp);
            _vault_grid(*this, feat, *ri, mapsp);

            if (!crawl_state.generating_level)
            {
                // Have to link items each square at a time, or
                // dungeon_terrain_changed could blow up.
                link_items();
                // Init tile flavour -- dungeon_terrain_changed does
                // this too, but only if oldgrid != newgrid, so we
                // make sure here.
                tile_init_flavour(*ri);
                const dungeon_feature_type newgrid = env.grid(*ri);
                env.grid(*ri) = oldgrid;
                dungeon_terrain_changed(*ri, newgrid, true);
                remove_markers_and_listeners_at(*ri);
            }
        }

        // Place monsters in a second pass. Otherwise band followers
        // could be overwritten with subsequent walls.
        for (rectangle_iterator ri(pos, pos + size - 1); ri; ++ri)
        {
            if (map.is_overwritable_layout() && map_masked(*ri, MMT_VAULT))
                continue;

            const coord_def dp = *ri - pos;

            const int feat = map.map.glyph(dp);
            keyed_mapspec *mapsp = map.mapspec_at(dp);

            _vault_grid_mons(*this, feat, *ri, mapsp);
        }

        map.map.apply_overlays(pos, map.is_overwritable_layout());
    }
}

void vault_placement::draw_at(const coord_def &c)
{
    pos = c;
    apply_grid();
}

int vault_placement::connect(bool spotty) const
{
    int exits_placed = 0;

    for (auto c : exits)
    {
        if (spotty && _connect_spotty(c, _feat_is_wall_floor_liquid)
            || player_in_branch(BRANCH_SHOALS) && dgn_shoals_connect_point(c)
            || _connect_vault_exit(c))
        {
            exits_placed++;
        }
        else
        {
            dprf(DIAG_DNGN, "Warning: failed to connect vault exit (%d;%d).",
                 c.x, c.y);
        }
    }

    return exits_placed;
}

// Checks the resultant feature type of the map glyph, after applying KFEAT
// and so forth. Unfortunately there is a certain amount of duplication of
// the code path in apply_grid; but actual modifications to the level
// are so intertwined with that code path it would be actually quite messy
// to try and avoid the duplication.
// TODO: this should be const, but a lot of mapdef stuff is not properly
// marked for this
dungeon_feature_type vault_placement::feature_at(const coord_def &c)
{
    // Can't check outside bounds of vault
    if (size.zero() || c.x > size.x || c.y > size.y)
        return NUM_FEATURES;

    const int feat = map.map.glyph(c);

    //XXX: perhaps this should really be NUM_FEATURES, but there are crashes.
    if (feat == ' ')
        return DNGN_FLOOR;

    keyed_mapspec *mapsp = map.mapspec_at(c);
    return _vault_inspect(*this, feat, mapsp);
}

bool vault_placement::is_space(const coord_def &c) const
{
    // Can't check outside bounds of vault
    if (size.zero() || c.x > size.x || c.y > size.y)
        return false;

    const int feat = map.map.glyph(c);
    return feat == ' ';
}
bool vault_placement::is_exit(const coord_def &c) const
{
    // Can't check outside bounds of vault
    if (size.zero() || c.x > size.x || c.y > size.y)
        return false;

    const int feat = map.map.glyph(c);
    return feat == '@';
}
static dungeon_feature_type _vault_inspect(vault_placement &place,
                        int vgrid, keyed_mapspec *mapsp)
{
    // The two functions called are
    if (mapsp && mapsp->replaces_glyph())
        return _vault_inspect_mapspec(place, *mapsp);
    else
        return _vault_inspect_glyph(vgrid);
}

static dungeon_feature_type _vault_inspect_mapspec(vault_placement &place,
                                                   keyed_mapspec& mapsp)
{
    UNUSED(place);
    dungeon_feature_type found = NUM_FEATURES;
    const feature_spec f = mapsp.get_feat();
    if (f.trap)
        found = trap_feature(f.trap->tr_type);
    else if (f.feat >= 0)
        found = static_cast<dungeon_feature_type>(f.feat);
    else if (f.glyph >= 0)
        found = _vault_inspect_glyph(f.glyph);
    else if (f.shop)
        found = DNGN_ENTER_SHOP;
    else
        found = DNGN_FLOOR;

    return found;
}

static dungeon_feature_type _vault_inspect_glyph(int vgrid)
{
    // Get the base feature according to the glyph
    dungeon_feature_type found = NUM_FEATURES;
    if (vgrid != -1)
        found = _glyph_to_feat(vgrid);

    // If it's an altar for an unavailable god then it will get turned into floor by _vault_grid_glyph
    if (feat_is_altar(found)
        && is_unavailable_god(feat_altar_god(found)))
    {
        found = DNGN_FLOOR;
    }

    return found;
}

static void _remember_vault_placement(const vault_placement &place)
{
    UNUSED(place);
#ifdef DEBUG_STATISTICS
    _you_all_vault_list.push_back(place.map.name);
#endif
}

string dump_vault_maps()
{
    string out = "";

    vector<level_id> levels = all_dungeon_ids();

    for (const level_id &lid : levels)
    {
        // n.b. portal vaults get cleared from here, so won't show up.
        // kind of spammy in wizmode. To test non-wizmode, use &ctrl-y
        if (!you.wizard && (!you.level_visited(lid)
                            || !you.vault_list.count(lid))
            || branch_is_unfinished(lid.branch))
        {
            continue;
        }

        if (you.wizard)
        {
            // because the save is already gone at the point where we are
            // printing a morgue, this check isn't reliable. Ignore it.
            if (!is_existing_level(lid) && you.save)
            {
                out += "[-gen]      ";
                if (!you.vault_list.count(lid))
                {
                    out +=  lid.describe() + "\n";
                    continue;
                }
            }
            else
                out += you.level_visited(lid) ? "[+gen,+vis] " : "[+gen,-vis] ";
        }
        out += lid.describe();
        vector<string> &maps(you.vault_list[lid]);
        if (maps.size() == 0)
        {
            out += "\n";
            continue;
        }

        out += ": " + string(max(8 - int(lid.describe().length()), 0), ' ');

        // TODO: some way of showing no_dump maps in wizmode?

        string vaults = comma_separated_line(maps.begin(), maps.end(), ", ");
        out += wordwrap_line(vaults, you.wizard ? 58 : 70) + "\n";
        while (!vaults.empty())
        {
            out += string(you.wizard ? 22 : 10, ' ')
                    + wordwrap_line(vaults, you.wizard ? 58 : 70, false) + "\n";
        }

    }
    return out;
}

///////////////////////////////////////////////////////////////////////////
// vault_place_iterator

vault_place_iterator::vault_place_iterator(const vault_placement &vp)
    : vault_place(vp), pos(vp.pos), tl(vp.pos), br(vp.pos + vp.size - 1)
{
    --pos.x;
    ++*this;
}

vault_place_iterator::operator bool () const
{
    return pos.y <= br.y && pos.x <= br.x;
}

coord_def vault_place_iterator::operator * () const
{
    return pos;
}

const coord_def *vault_place_iterator::operator -> () const
{
    return &pos;
}

coord_def vault_place_iterator::vault_pos() const
{
    return pos - tl;
}

vault_place_iterator &vault_place_iterator::operator ++ ()
{
    while (pos.y <= br.y)
    {
        if (++pos.x > br.x)
        {
            pos.x = tl.x;
            ++pos.y;
        }
        if (pos.y <= br.y && vault_place.map.in_map(pos - tl))
            break;
    }
    return *this;
}

vault_place_iterator vault_place_iterator::operator ++ (int)
{
    const vault_place_iterator copy = *this;
    ++*this;
    return copy;
}

//////////////////////////////////////////////////////////////////////////
// unwind_vault_placement_mask

unwind_vault_placement_mask::unwind_vault_placement_mask(const map_bitmask *mask)
    : oldmask(Vault_Placement_Mask)
{
    Vault_Placement_Mask = mask;
}

unwind_vault_placement_mask::~unwind_vault_placement_mask()
{
    Vault_Placement_Mask = oldmask;
}

// mark all unexplorable squares, count the rest
static void _calc_density()
{
    int open = 0;
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        // If for some reason a level gets modified afterwards, dug-out
        // places in unmodified parts should not suddenly become explorable.
        if (!testbits(env.pgrid(*ri), FPROP_SEEN_OR_NOEXP))
            for (adjacent_iterator ai(*ri, false); ai; ++ai)
                if (feat_has_solid_floor(env.grid(*ai)))
                {
                    open++;
                    goto out;
                }
        env.pgrid(*ri) |= FPROP_SEEN_OR_NOEXP;
    out:;
    }

    dprf(DIAG_DNGN, "Level density: %d", open);
    env.density = open;
}

// Mark all solid squares as no_tele so that digging doesn't influence
// random teleportation.
static void _mark_solid_squares()
{
    for (rectangle_iterator ri(0); ri; ++ri)
        if (feat_is_solid(env.grid(*ri)))
            env.pgrid(*ri) |= FPROP_NO_TELE_INTO;
}

// Based on their starting class, where does the player start?
int starting_absdepth()
{
    if (you.char_class == JOB_DELVER)
    {
        // makes delver sort of work in descent
        if (crawl_state.game_is_descent())
            return 1;
        return 4;
    }
    return 0; // (absdepth is 0-indexed)
}
