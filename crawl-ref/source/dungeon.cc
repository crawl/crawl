/**
 * @file
 * @brief Functions used when building new levels.
**/

#include "AppHdr.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>
#include <cmath>

#include "abyss.h"
#include "acquire.h"
#include "act-iter.h"
#include "artefact.h"
#include "attitude-change.h"
#include "branch.h"
#include "butcher.h"
#include "chardump.h"
#include "cloud.h"
#include "coordit.h"
#include "defines.h"
#include "dgn-delve.h"
#include "dgn-height.h"
#include "dgn-shoals.h"
#include "dgn-labyrinth.h"
#include "dgn-overview.h"
#include "effects.h"
#include "end.h"
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
#include "ghost.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "lev-pand.h"
#include "libutil.h"
#include "makeitem.h"
#include "mapdef.h"
#include "mapmark.h"
#include "maps.h"
#include "message.h"
#include "misc.h"
#include "mon-chimera.h"
#include "mon-death.h"
#include "mon-util.h"
#include "mon-pick.h"
#include "mon-poly.h"
#include "mon-place.h"
#include "mgen_data.h"
#include "mon-pathfind.h"
#include "notes.h"
#include "place.h"
#include "player.h"
#include "random.h"
#include "random-weight.h"
#include "religion.h"
#include "show.h"
#include "spl-book.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "stairs.h"
#include "state.h"
#include "stringutil.h"
#include "tags.h"
#include "terrain.h"
#include "tiledef-dngn.h"
#include "tilepick.h"
#include "tileview.h"
#include "traps.h"
#include "travel.h"
#include "zotdef.h"
#include "hints.h"

#ifdef DEBUG_DIAGNOSTICS
#define DEBUG_TEMPLES
#endif

#ifdef WIZARD
#include "cio.h" // for cancellable_get_line()
#endif

// DUNGEON BUILDERS
static bool _build_level_vetoable(bool enable_random_maps,
                                  dungeon_feature_type dest_stairs_type);
static void _build_dungeon_level(dungeon_feature_type dest_stairs_type);
static bool _valid_dungeon_level();

static bool _builder_by_type();
static bool _builder_normal();
static void _builder_items();
static void _builder_monsters();
static coord_def _place_specific_feature(dungeon_feature_type feat);
static bool _place_specific_trap(const coord_def& where, trap_spec* spec,
                                 int charges = 0);
static void _place_branch_entrances(bool use_vaults);
static void _place_extra_vaults();
static void _place_chance_vaults();
static void _place_minivaults();
static int _place_uniques();
static void _place_gozag_shop(dungeon_feature_type stair);
static void _place_traps();
static void _prepare_water();
static void _check_doors();

static void _add_plant_clumps(int frequency = 10, int clump_density = 12,
                              int clump_radius = 4);

static void _pick_float_exits(vault_placement &place,
                              vector<coord_def> &targets);
static bool _feat_is_wall_floor_liquid(dungeon_feature_type);
static bool _connect_spotty(const coord_def& from,
                            bool (*overwriteable)(dungeon_feature_type) = NULL);
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
static dungeon_feature_type _vault_inspect_glyph(vault_placement &place,
                                                 int vgrid);

static const map_def *_dgn_random_map_for_place(bool minivault);
static void _dgn_load_colour_grid();
static void _dgn_map_colour_fixup();

static void _dgn_unregister_vault(const map_def &map);
static void _remember_vault_placement(const vault_placement &place, bool extra);

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
static dungeon_feature_type _pick_temple_altar(vault_placement &place);
static dungeon_feature_type _pick_an_altar();

static vector<god_type> _temple_altar_list;
static CrawlHashTable*       _current_temple_hash = NULL; // XXX: hack!

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
static FixedBitVector<NUM_MONSTERS> temp_unique_creatures;
static FixedVector<unique_item_status_type, MAX_UNRANDARTS> temp_unique_items;

const map_bitmask *Vault_Placement_Mask = NULL;

static bool use_random_maps = true;
static bool dgn_check_connectivity = false;
static int  dgn_zones = 0;

static vector<string> _you_vault_list;

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
static Unique_ptr<dungeon_colour_grid> dgn_colour_grid;

static string branch_epilogues[NUM_BRANCHES];

static void _count_gold()
{
    vector<item_def *> gold_piles;
    vector<coord_def> gold_places;
    int gold = 0;
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        for (stack_iterator j(*ri); j; ++j)
        {
            if (j->base_type == OBJ_GOLD)
            {
                gold += j->quantity;
                gold_piles.push_back(&(*j));
                gold_places.push_back(*ri);
            }
        }
    }

    you.attribute[ATTR_GOLD_GENERATED] += gold;

    if (player_under_penance(GOD_GOZAG) && x_chance_in_y(gold - 500, 500))
    {
        for (unsigned int i = 0; i < gold_piles.size(); i++)
        {
            gold_piles[i]->clear();
            gold_piles[i]->base_type = OBJ_MISSILES;
            gold_piles[i]->sub_type  = MI_STONE;
            gold_piles[i]->quantity  = 1;
            item_colour(*gold_piles[i]);
        }
        mprf(MSGCH_GOD, GOD_GOZAG, "You feel a great sense of loss.");
        dec_penance(GOD_GOZAG, gold / 200);
    }
    else if (you_worship(GOD_GOZAG))
    {
        for (unsigned int i = 0; i < gold_places.size(); i++)
        {
            bool detected = false;
            int dummy = gold_piles[i]->index();
            coord_def &pos = gold_places[i];
            unlink_item(dummy);
            move_item_to_grid(&dummy, pos, true);
            if (!env.map_knowledge(pos).item()
                || env.map_knowledge(pos).item()->base_type != OBJ_GOLD)
            {
                detected = true;
            }
            update_item_at(pos, true);
            if (detected)
            {
                ASSERT(env.map_knowledge(pos).item());
                env.map_knowledge(pos).flags |= MAP_DETECTED_ITEM;
            }
        }
    }
}

/**********************************************************************
 * builder() - kickoff for the dungeon generator.
 *********************************************************************/
bool builder(bool enable_random_maps, dungeon_feature_type dest_stairs_type)
{
    // Re-check whether we're in a valid place, it leads to obscure errors
    // otherwise.
    ASSERT_RANGE(you.where_are_you, 0, NUM_BRANCHES);
    ASSERT_RANGE(you.depth, 0 + 1, brdepth[you.where_are_you] + 1);

    const set<string> uniq_tags  = you.uniq_map_tags;
    const set<string> uniq_names = you.uniq_map_names;

    // Save a copy of unique creatures for vetoes.
    temp_unique_creatures = you.unique_creatures;
    // And unrands
    temp_unique_items = you.unique_items;

    unwind_bool levelgen(crawl_state.generating_level, true);

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
            if (_build_level_vetoable(enable_random_maps, dest_stairs_type))
            {
                for (monster_iterator mi; mi; ++mi)
                    gozag_set_bribe(*mi);

                if (you.props.exists(GOZAG_ANNOUNCE_SHOP_KEY))
                    unmark_offlevel_shop(level_id::current());

                return true;
            }
        }
        catch (map_load_exception &mload)
        {
            mprf(MSGCH_ERROR, "Failed to load map %s, reloading all maps",
                 mload.what());
            reread_maps();
        }

        you.uniq_map_tags  = uniq_tags;
        you.uniq_map_names = uniq_names;
    }

    if (!crawl_state.map_stat_gen && !crawl_state.obj_stat_gen)
    {
        // Failed to build level, bail out.
        if (crawl_state.need_save)
        {
            save_game(true,
                  make_stringf("Unable to generate level for '%s'!",
                               level_id::current().describe().c_str()).c_str());
        }
        else
        {
            die("Unable to generate level for '%s'!",
                level_id::current().describe().c_str());
        }
    }

    env.level_layout_types.clear();
    return false;
}

static bool _build_level_vetoable(bool enable_random_maps,
                                  dungeon_feature_type dest_stairs_type)
{
#ifdef DEBUG_DIAGNOSTICS
    mapstat_report_map_build_start();
#endif

    dgn_reset_level(enable_random_maps);

    if (player_in_branch(BRANCH_TEMPLE))
        _setup_temple_altars(you.props);

    try
    {
        _build_dungeon_level(dest_stairs_type);
    }
    catch (dgn_veto_exception& e)
    {
        dprf("<white>VETO</white>: %s: %s", level_id::current().describe().c_str(), e.what());
#ifdef DEBUG_DIAGNOSTICS
        mapstat_report_map_veto(e.what());
#endif
        return false;
    }

    _dgn_set_floor_colours();

    if (crawl_state.game_standard_levelgen()
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
    _count_gold();

    if (!_you_vault_list.empty())
    {
        vector<string> &vec(you.vault_list[level_id::current()]);
        vec.insert(vec.end(), _you_vault_list.begin(), _you_vault_list.end());
    }

    return true;
}

// Things that are bugs where we want to assert rather than to sweep it under
// the rug with a veto.
static void _builder_assertions()
{
#ifdef ASSERTS
    for (rectangle_iterator ri(0); ri; ++ri)
        if (!in_bounds(*ri))
            if (!is_valid_border_feat(grd(*ri)))
            {
                die("invalid map border at (%d,%d): %s", ri->x, ri->y,
                    dungeon_feature_name(grd(*ri)));
            }
#endif
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

void dgn_clear_vault_placements(vault_placement_refv &vps)
{
    for (vault_placement_refv::const_iterator i = vps.begin();
         i != vps.end(); ++i)
    {
        delete *i;
    }
    vps.clear();
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
            vault_placement *vp = env.level_vaults[i];
            // Unreferenced vault, blow it away
            dprf("Removing references to unused map #%d) '%s' (%d,%d) (%d,%d)",
                 i, vp->map.name.c_str(), vp->pos.x, vp->pos.y,
                 vp->size.x, vp->size.y);

            if (!vp->seen)
            {
                dprf("Unregistering unseen vault: %s", vp->map.name.c_str());
                _dgn_unregister_vault(vp->map);
            }

            delete vp;
            env.level_vaults.erase(env.level_vaults.begin() + i);

            // Fix new indexes for all higher indexed vaults that are
            // still referenced.
            for (int j = i + 1; j < nvaults; ++j)
            {
                map<int, int>::iterator imap = new_vault_index_map.find(j);
                if (imap != new_vault_index_map.end())
                    --imap->second;
            }
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
            map<int, int>::iterator imap = new_vault_index_map.find(map_index);
            if (imap != new_vault_index_map.end())
                env.level_map_ids(*ri) = imap->second;
        }
    }

#ifdef DEBUG_ABYSS
    dprf("Extant vaults on level: %d", (int) env.level_vaults.size());
    for (int i = 0, size = env.level_vaults.size(); i < size; ++i)
    {
        const vault_placement &vp(*env.level_vaults[i]);
        dprf("%d) %s (%d,%d) size (%d,%d)",
             i, vp.map.name.c_str(), vp.pos.x, vp.pos.y,
             vp.size.x, vp.size.y);
    }
#endif
}

void level_clear_vault_memory()
{
    dgn_clear_vault_placements(env.level_vaults);
    Temp_Vaults.clear();
    env.level_map_mask.init(0);
    env.level_map_ids.init(INVALID_MAP_INDEX);
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

bool set_level_flags(uint32_t flags, bool silent)
{
    bool could_control = allow_control_teleport(true);
    bool could_map     = is_map_persistent();

    uint32_t old_flags = env.level_flags;
    env.level_flags |= flags;

    bool can_control = allow_control_teleport(true);
    bool can_map     = is_map_persistent();

    if (could_control && !can_control && !silent)
    {
        mprf(MSGCH_WARN, "You sense the appearance of a powerful magical force "
                         "which warps space.");
    }

    if (could_map && !can_map && !silent)
    {
        mprf(MSGCH_WARN, "A powerful force appears that prevents you from "
                         "remembering where you've been.");
    }

    return old_flags != env.level_flags;
}

bool unset_level_flags(uint32_t flags, bool silent)
{
    bool could_control = allow_control_teleport(true);
    bool could_map     = is_map_persistent();

    iflags_t old_flags = env.level_flags;
    env.level_flags &= ~flags;

    bool can_control = allow_control_teleport(true);
    bool can_map     = is_map_persistent();

    if (!could_control && can_control && !silent)
    {
        // Isn't really a "recovery", but I couldn't think of where
        // else to send it.
        mprf(MSGCH_RECOVERY, "You sense the disappearance of a powerful "
                             "magical force which warped space.");
    }

    if (!could_map && can_map && !silent)
    {
        // Isn't really a "recovery", but I couldn't think of where
        // else to send it.
        mprf(MSGCH_RECOVERY, "You sense the disappearance of the force that "
                             "prevented you from remembering where you've been.");
    }

    return old_flags != env.level_flags;
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

static void _set_grd(const coord_def &c, dungeon_feature_type feat)
{
    // It might be good to clear some pgrid flags as well.
    env.tile_flv(c).feat    = 0;
    env.tile_flv(c).special = 0;
    env.grid_colours(c) = 0;
    grd(c) = feat;
}

static void _dgn_register_vault(const string name, const string spaced_tags)
{
    if (spaced_tags.find(" allow_dup ") == string::npos)
        you.uniq_map_names.insert(name);

    if (spaced_tags.find(" luniq ") != string::npos)
        env.level_uniq_maps.insert(name);

    vector<string> tags = split_string(" ", spaced_tags);
    for (int t = 0, ntags = tags.size(); t < ntags; ++t)
    {
        const string &tag = tags[t];
        if (tag.find("uniq_") == 0)
            you.uniq_map_tags.insert(tag);
        else if (tag.find("luniq_") == 0)
            env.level_uniq_map_tags.insert(tag);
    }
}

static void _dgn_unregister_vault(const map_def &map)
{
    you.uniq_map_names.erase(map.name);
    env.level_uniq_maps.erase(map.name);

    vector<string> tags = split_string(" ", map.tags);
    for (int t = 0, ntags = tags.size(); t < ntags; ++t)
    {
        const string &tag = tags[t];
        if (tag.find("uniq_") == 0)
            you.uniq_map_tags.erase(tag);
        else if (tag.find("luniq_") == 0)
            env.level_uniq_map_tags.erase(tag);
    }

    for (unsigned int j = 0; j < map.subvault_places.size(); ++j)
        _dgn_unregister_vault(*map.subvault_places[j].subvault);
}

bool dgn_square_travel_ok(const coord_def &c)
{
    const dungeon_feature_type feat = grd(c);
    if (feat_is_trap(feat))
    {
        const trap_def * const trap = find_trap(c);
        return !(trap && trap->type == TRAP_TELEPORT);
    }
    else
        return feat_is_traversable(feat);
}

static bool _dgn_square_is_passable(const coord_def &c)
{
    // [enne] Why does this function check MMT_OPAQUE?
    //
    // Don't peek inside MMT_OPAQUE vaults (all vaults are opaque by
    // default) because vaults may choose to create isolated regions,
    // or otherwise cause connectivity issues even if the map terrain
    // is travel-passable.
    return !(env.level_map_mask(c) & MMT_OPAQUE) && dgn_square_travel_ok(c);
}

static inline void _dgn_point_record_stub(const coord_def &) { }

template <class point_record>
static bool _dgn_fill_zone(
    const coord_def &start, int zone,
    point_record &record_point,
    bool (*passable)(const coord_def &) = _dgn_square_is_passable,
    bool (*iswanted)(const coord_def &) = NULL)
{
    bool ret = false;
    list<coord_def> points[2];
    int cur = 0;

    // No bounds checks, assuming the level has at least one layer of
    // rock border.

    for (points[cur].push_back(start); !points[cur].empty();)
    {
        for (list<coord_def>::const_iterator i = points[cur].begin();
             i != points[cur].end(); ++i)
        {
            const coord_def &c(*i);

            travel_point_distance[c.x][c.y] = zone;

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
    return ret;
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

    if (feature_mimic_at(c))
        return false;

    switch (grd(c))
    {
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
    case DNGN_EXIT_HELL:
#if TAG_MAJOR_VERSION == 34
    case DNGN_RETURN_FROM_DWARF:
    case DNGN_RETURN_FROM_BLADE:
    case DNGN_RETURN_FROM_FOREST:
#endif
    case DNGN_RETURN_FROM_ORC:
    case DNGN_RETURN_FROM_LAIR:
    case DNGN_RETURN_FROM_SLIME:
    case DNGN_RETURN_FROM_VAULTS:
    case DNGN_RETURN_FROM_CRYPT:
    case DNGN_RETURN_FROM_ZOT:
    case DNGN_RETURN_FROM_TEMPLE:
    case DNGN_RETURN_FROM_SNAKE:
    case DNGN_RETURN_FROM_ELF:
    case DNGN_RETURN_FROM_TOMB:
    case DNGN_RETURN_FROM_SWAMP:
    case DNGN_RETURN_FROM_SHOALS:
    case DNGN_RETURN_FROM_SPIDER:
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
    if (feature_mimic_at(c))
        return false;

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
#if TAG_MAJOR_VERSION == 34
    case DNGN_RETURN_FROM_DWARF:
    case DNGN_RETURN_FROM_BLADE:
    case DNGN_RETURN_FROM_FOREST:
#endif
    case DNGN_RETURN_FROM_ORC:
    case DNGN_RETURN_FROM_LAIR:
    case DNGN_RETURN_FROM_SLIME:
    case DNGN_RETURN_FROM_VAULTS:
    case DNGN_RETURN_FROM_CRYPT:
    case DNGN_RETURN_FROM_ZOT:
    case DNGN_RETURN_FROM_TEMPLE:
    case DNGN_RETURN_FROM_SNAKE:
    case DNGN_RETURN_FROM_ELF:
    case DNGN_RETURN_FROM_TOMB:
    case DNGN_RETURN_FROM_SWAMP:
    case DNGN_RETURN_FROM_SHOALS:
    case DNGN_RETURN_FROM_SPIDER:
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
static int _process_disconnected_zones(int x1, int y1, int x2, int y2,
                                       bool choose_stairless,
                                       dungeon_feature_type fill)
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
                || !_dgn_square_is_passable(coord_def(x, y)))
            {
                continue;
            }

            const bool found_exit_stair =
                _dgn_fill_zone(coord_def(x, y), ++nzones,
                               _dgn_point_record_stub,
                               _dgn_square_is_passable,
                               choose_stairless ? (at_branch_bottom() ?
                                                   _is_upwards_exit_stair :
                                                   _is_exit_stair) : NULL);

            // If we want only stairless zones, screen out zones that did
            // have stairs.
            if (choose_stairless && found_exit_stair)
                ++ngood;
            else if (fill)
            {
                // Don't fill in areas connected to vaults.
                // We want vaults to be accessible; if the area is disconneted
                // from the rest of the level, this will cause the level to be
                // vetoed later on.
                bool veto = false;
                vector<coord_def> coords;
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
                            else
                                coords.push_back(coord_def(fx, fy));
                        }
                    }
                    if (veto)
                        break;
                }
                if (!veto)
                    for (vector<coord_def>::iterator it = coords.begin();
                         it != coords.end(); it++)
                    {
                        _set_grd(*it, fill);
                    }
            }
        }
    }

    return nzones - ngood;
}

int dgn_count_disconnected_zones(bool choose_stairless,
                                 dungeon_feature_type fill)
{
    return _process_disconnected_zones(0, 0, GXM-1, GYM-1, choose_stairless,
                                       fill);
}

static void _fixup_hell_stairs()
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (grd(*ri) >= DNGN_STONE_STAIRS_UP_I
            && grd(*ri) <= DNGN_ESCAPE_HATCH_UP)
        {
            _set_grd(*ri, DNGN_ENTER_HELL);
        }
    }
}

static void _fixup_pandemonium_stairs()
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (grd(*ri) >= DNGN_STONE_STAIRS_UP_I
            && grd(*ri) <= DNGN_ESCAPE_HATCH_UP)
        {
            _set_grd(*ri, DNGN_TRANSIT_PANDEMONIUM);
        }
    }
}

static void _mask_vault(const vault_placement &place, unsigned mask)
{
    for (vault_place_iterator vi(place); vi; ++vi)
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
        _dgn_register_vault(place.map.name, place.map.tags);
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

        if (!transparent)
            _mask_vault(place, MMT_OPAQUE);
    }

    // Find tags matching properties.
    vector<string> tags = place.map.get_tags();

    for (vector<string>::const_iterator i = tags.begin(); i != tags.end(); ++i)
    {
        const feature_property_type prop = str_to_fprop(*i);
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

        if (spec != NULL)
        {
            env.level_map_mask(*vi) |= (short)spec->map_mask.flags_set;
            env.level_map_mask(*vi) &= ~((short)spec->map_mask.flags_unset);
        }
    }

    set_level_flags(place.map.level_flags.flags_set, true);
    unset_level_flags(place.map.level_flags.flags_unset, true);

    if (place.map.floor_colour != BLACK)
        env.floor_colour = place.map.floor_colour;

    if (place.map.rock_colour != BLACK)
        env.rock_colour = place.map.rock_colour;

    if (!place.map.rock_tile.empty())
    {
        tileidx_t rock;
        if (tile_dngn_index(place.map.rock_tile.c_str(), &rock))
        {
            env.tile_default.wall_idx =
                store_tilename_get_index(place.map.rock_tile);

            env.tile_default.wall = rock;
        }
    }

    if (!place.map.floor_tile.empty())
    {
        tileidx_t floor;
        if (tile_dngn_index(place.map.floor_tile.c_str(), &floor))
        {
            env.tile_default.floor_idx =
                store_tilename_get_index(place.map.floor_tile);

            env.tile_default.floor = floor;
        }
    }

    vault_placement *new_vault_place = new vault_placement(place);
    env.level_vaults.push_back(new_vault_place);
    if (register_vault)
        _remember_vault_placement(place, place.map.has_tag("extra"));
    return new_vault_place;
}

static bool _dgn_ensure_vault_placed(bool vault_success,
                                     bool disable_further_vaults)
{
    if (!vault_success)
        throw dgn_veto_exception("Vault placement failure.");
    else if (disable_further_vaults)
        use_random_maps = false;
    return vault_success;
}

static bool _ensure_vault_placed_ex(bool vault_success, const map_def *vault)
{
    return _dgn_ensure_vault_placed(vault_success,
                                    (!vault->has_tag("extra")
                                     && vault->orient == MAP_ENCOMPASS));
}

static coord_def _find_level_feature(int feat)
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (grd(*ri) == feat)
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

    _you_vault_list.clear();
    env.level_build_method.clear();
    env.level_layout_types.clear();
    level_clear_vault_memory();
    dgn_colour_grid.reset(NULL);

    use_random_maps = enable_random_maps;
    dgn_check_connectivity = false;
    dgn_zones        = 0;

    _temple_altar_list.clear();
    _current_temple_hash = NULL;

    // Forget level properties.
    env.properties.clear();
    env.heightmap.reset(NULL);

    env.absdepth0 = absdungeon_depth(you.where_are_you, you.depth);

    if (!crawl_state.test)
        dprf("absdepth0 = %d", env.absdepth0);

    // Blank level with DNGN_ROCK_WALL.
    env.grid.init(DNGN_ROCK_WALL);
    env.pgrid.init(0);
    env.grid_colours.init(BLACK);
    env.map_knowledge.init(map_cell());
    env.map_forgotten.reset();
    env.map_seen.reset();

    // Delete all traps.
    for (int i = 0; i < MAX_TRAPS; i++)
        env.trap[i].type = TRAP_UNASSIGNED;

    // Initialise all items.
    for (int i = 0; i < MAX_ITEMS; i++)
        init_item(i);

    // Reset all monsters.
    reset_all_monsters();
    init_anon();

    // ... and Pan/regular spawn lists.
    env.mons_alloc.init(MONS_NO_MONSTER);
    setup_vault_mon_list();

    // Zap clouds
    env.cgrid.init(EMPTY_CLOUD);

    const cloud_struct empty;
    env.cloud.init(empty);
    env.cloud_no = 0;

    mgrd.init(NON_MONSTER);
    igrd.init(NON_ITEM);
    env.tgrid.init(NON_ENTITY);

    // Reset all shops.
    for (int shcount = 0; shcount < MAX_SHOPS; shcount++)
        env.shop[shcount].type = SHOP_UNASSIGNED;

    // Clear all markers.
    env.markers.clear();

    // Lose all listeners.
    dungeon_events.clear();

    // Set default level flags.
    env.level_flags = branches[you.where_are_you].default_level_flags;

    // Set default random monster generation rate (smaller is more often,
    // except that 0 == no random monsters).
    if (player_in_branch(BRANCH_TEMPLE)
        && !player_has_orb() // except for the Orb run
        || crawl_state.game_is_tutorial())
    {
        // No random monsters in tutorial or ecu temple
        env.spawn_random_rate = 0;
    }
    else if (player_in_connected_branch()
             || (player_has_orb() && !player_in_branch(BRANCH_ABYSS)))
        env.spawn_random_rate = 240;
    else if (player_in_branch(BRANCH_ABYSS)
             || player_in_branch(BRANCH_PANDEMONIUM))
    {
        // Abyss spawn rate is set for those characters that start out in the
        // Abyss; otherwise the number is ignored in the Abyss.
        env.spawn_random_rate = 50;
    }
    else
        // No random monsters in Labyrinths and portal vaults if we don't have
        // the orb.
        env.spawn_random_rate = 0;
    env.density = 0;
    env.forest_awoken_until = 0;
    env.sunlight.clear();

    env.floor_colour = BLACK;
    env.rock_colour  = BLACK;

    // Clear exclusions
    clear_excludes();

    // Clear custom tile settings from vaults
    tile_init_default_flavour();
    tile_clear_flavour();
    env.tile_names.clear();
}

static int _num_items_wanted(int absdepth0)
{
    if (branches[you.where_are_you].branch_flags & BFLAG_NO_ITEMS)
        return 0;
    else if (absdepth0 > 5 && one_chance_in(500 - 5 * absdepth0))
        return 10 + random2avg(90, 2); // rich level!
    else
        return 3 + roll_dice(3, 11);
}

static int _num_mons_wanted()
{
    if (player_in_branch(BRANCH_ABYSS))
        return 0;

    if (player_in_branch(BRANCH_PANDEMONIUM))
        return random2avg(28, 3);

    // Except for Abyss and Pan, no other portal gets random monsters.
    if (!player_in_connected_branch())
        return 0;

    if (!branch_has_monsters(you.where_are_you))
        return 0;

    if (player_in_branch(BRANCH_CRYPT))
        return roll_dice(3, 8);

    int mon_wanted = roll_dice(3, 10);

    if (player_in_hell())
        mon_wanted += roll_dice(3, 8);

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
            wall_type = random_choose_weighted(1, DNGN_GREEN_CRYSTAL_WALL,
                                               9, DNGN_METAL_WALL,
                                               0);
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
    for (int i = 0; i < MAX_ITEMS; i++)
    {
        item_def& item(mitm[i]);
        if (!item.defined() || item.pos.x == 0
            || item.held_by_monster())
        {
            continue;
        }

        if (in_bounds(item.pos))
        {
            dungeon_feature_type feat = grd(item.pos);
            if (feat >= DNGN_MINITEM)
                continue;

            // We accept items in deep water in the Abyss---they are likely to
            // be revealed eventually by morphing, and having deep water push
            // items away leads to strange results.
            if (feat == DNGN_DEEP_WATER && you.where_are_you == BRANCH_ABYSS)
                continue;

            mprf(MSGCH_ERROR, "Item %s buggily placed in feature %s at (%d, %d).",
                 item.name(DESC_PLAIN).c_str(),
                 feature_description_at(item.pos, false, DESC_PLAIN,
                                     false, false).c_str(),
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

static void _fixup_branch_stairs()
{
    // Top level of branch levels - replaces up stairs with stairs back to
    // dungeon or wherever:
    if (you.depth == 1)
    {
#ifdef DEBUG_DIAGNOSTICS
        int count = 0;
#endif
        // Just in case we somehow get here with more than one stair placed.
        // Prefer stairs that are placed in vaults for picking an exit at
        // random.
        vector<coord_def> vault_stairs, normal_stairs;
        dungeon_feature_type exit = your_branch().exit_stairs;
        if (you.where_are_you == root_branch) // ZotDef
            exit = DNGN_EXIT_DUNGEON;
        for (rectangle_iterator ri(1); ri; ++ri)
        {
            if (grd(*ri) == DNGN_ESCAPE_HATCH_UP)
                _set_grd(*ri, DNGN_FLOOR);
            else if (grd(*ri) >= DNGN_STONE_STAIRS_UP_I
                     && grd(*ri) <= DNGN_STONE_STAIRS_UP_III)
            {
#ifdef DEBUG_DIAGNOSTICS
                if (count++ && you.where_are_you != root_branch)
                {
                    mprf(MSGCH_ERROR, "Multiple branch exits on %s",
                         level_id::current().describe().c_str());
                }
#endif
                if (you.where_are_you == root_branch)
                {
                    env.markers.add(new map_feature_marker(*ri, grd(*ri)));
                    _set_grd(*ri, exit);
                }
                else
                {
                    if (map_masked(*ri, MMT_VAULT))
                        vault_stairs.push_back(*ri);
                    else
                        normal_stairs.push_back(*ri);
                }
            }
        }
        if (you.where_are_you != root_branch)
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
                env.markers.add(new map_feature_marker(coord, grd(coord)));
                _set_grd(coord, exit);
                for (vector<coord_def>::iterator it = stairs.begin() + 1;
                     it != stairs.end(); it++)
                {
                    _set_grd(*it, DNGN_FLOOR);
                }
            }
        }
    }

    // Bottom level of branch - wipes out down stairs and hatches
    dungeon_feature_type feat = DNGN_FLOOR;

    if (at_branch_bottom())
    {
        for (rectangle_iterator ri(1); ri; ++ri)
        {
            if (grd(*ri) >= DNGN_STONE_STAIRS_DOWN_I
                && grd(*ri) <= DNGN_ESCAPE_HATCH_DOWN)
            {
                _set_grd(*ri, feat);
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

    for (rectangle_iterator ri(1); ri; ++ri)
    {
        const coord_def& c = *ri;
        if (feature_mimic_at(c))
            continue;

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

        unsigned int num_stairs, needed_stairs;
        dungeon_feature_type base;
        dungeon_feature_type replace;
        if (i == 0)
        {
            num_stairs = num_up_stairs;
            replace = DNGN_FLOOR;
            base = DNGN_STONE_STAIRS_UP_I;
            // Pan abuses stair placement for transits, as we want connectivity
            // checks.
            needed_stairs = you.depth == 1
                            && !player_in_branch(BRANCH_PANDEMONIUM)
                            ? 1 : 3;
        }
        else
        {
            num_stairs = num_down_stairs;
            replace = DNGN_FLOOR;
            base = DNGN_STONE_STAIRS_DOWN_I;

            if (at_branch_bottom())
                needed_stairs = 0;
            else
                needed_stairs = 3;
        }

        // In Zot, don't create extra escape hatches, in order to force
        // the player through vaults that use all three down stone stairs.
        if (player_in_branch(BRANCH_ZOT))
            replace = DNGN_GRANITE_STATUE;

        dprf("Before culling: %d/%d %s stairs", num_stairs, needed_stairs,
             i ? "down" : "up");

        if (num_stairs > needed_stairs)
        {
            // Find pairwise stairs that are connected and turn one of them
            // into an escape hatch of the appropriate type.
            for (unsigned int s1 = 0; s1 < num_stairs; s1++)
            {
                if (num_stairs <= needed_stairs)
                    break;

                for (unsigned int s2 = s1 + 1; s2 < num_stairs; s2++)
                {
                    if (num_stairs <= needed_stairs)
                        break;

                    if (preserve_vault_stairs
                        && map_masked(stair_list[s2], MMT_VAULT))
                    {
                        continue;
                    }

                    flood_find<feature_grid, coord_predicate> ff(env.grid,
                                                                 in_bounds);

                    ff.add_feat(grd(stair_list[s2]));

                    // Ensure we're not searching for the feature at s1.
                    dungeon_feature_type save = grd(stair_list[s1]);
                    grd(stair_list[s1]) = DNGN_FLOOR;

                    const coord_def where =
                        ff.find_first_from(stair_list[s1],
                                           env.level_map_mask);
                    if (where.x)
                    {
                        dprf("Too many stairs -- removing one of a connected pair.");
                        grd(stair_list[s2]) = replace;
                        num_stairs--;
                        stair_list[s2] = stair_list[num_stairs];
                        s2--;
                    }

                    grd(stair_list[s1]) = save;
                }
            }

            // If that doesn't work, remove random stairs.
            while (num_stairs > needed_stairs)
            {
                int remove = random2(num_stairs);
                if (preserve_vault_stairs)
                {
                    int tries;
                    for (tries = num_stairs; tries > 0; tries--)
                    {
                        if (!map_masked(stair_list[remove], MMT_VAULT))
                            break;
                        remove = (remove + 1) % num_stairs;
                    }

                    // If we looped through all possibilities, then it
                    // means that there are more than 3 stairs in vaults and
                    // we can't preserve vault stairs.
                    if (!tries)
                    {
                        dprf("Too many stairs inside vaults!");
                        break;
                    }
                }
                dprf("Too many stairs -- removing one blindly.");
                _set_grd(stair_list[remove], replace);

                stair_list[remove] = stair_list[--num_stairs];
            }
        }

        // FIXME: stairs that generate inside random vaults are still
        // protected, resulting in superfluoes ones.
        dprf("After culling: %d/%d %s stairs", num_stairs, needed_stairs,
             i ? "down" : "up");

        if (num_stairs > needed_stairs && preserve_vault_stairs
            && (i || you.depth != 1 || you.where_are_you != root_branch))
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
        for (unsigned int s = num_stairs; s < needed_stairs; s++)
        {
            const uint32_t mask = preserve_vault_stairs ? MMT_VAULT : 0;
            coord_def gc = _dgn_random_point_in_bounds(DNGN_FLOOR, mask, DNGN_UNSEEN);

            if (!gc.origin())
            {
                dprf("Adding stair %d at (%d,%d)", s, gc.x, gc.y);
                // base gets fixed up to be the right stone stair below...
                _set_grd(gc, base);
                stair_list[num_stairs++] = gc;
            }
            else
                success = false;
        }

        // If we only need one stone stair, make sure it's _I.
        if (i == 0 && needed_stairs == 1)
        {
            ASSERT(num_stairs == 1 || you.where_are_you == root_branch);
            if (num_stairs == 1)
            {
                grd(stair_list[0]) = DNGN_STONE_STAIRS_UP_I;
                continue;
            }
        }

        // Ensure uniqueness of three stairs.
        for (int s = 0; s < 4; s++)
        {
            int s1 = s % num_stairs;
            int s2 = (s1 + 1) % num_stairs;
            ASSERT(grd(stair_list[s2]) >= base
                   && grd(stair_list[s2]) < base + 3);

            if (grd(stair_list[s1]) == grd(stair_list[s2]))
            {
                _set_grd(stair_list[s2], (dungeon_feature_type)
                    (base + (grd(stair_list[s2])-base+1) % 3));
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
                if (grd(*ri) == feat
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
                coord_def rnd(random2(GXM), random2(GYM));
                if (grd(rnd) != DNGN_FLOOR)
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
                if (grd(*ri) != DNGN_FLOOR)
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
    // escape hatch.  This will always allow (downward) progress.

    if (branches[you.where_are_you].branch_flags & BFLAG_ISLANDED)
        return true;

    // Veto D:1 or Pan if there are disconnected areas.
    if (player_in_branch(BRANCH_PANDEMONIUM)
        || (player_in_branch(BRANCH_DUNGEON) && you.depth == 1))
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
        if (!feat_is_branch_stairs(grd(*ri)))
            continue;
        if (!_has_connected_stone_stairs_from(*ri))
            return false;
    }

    return true;
}

static bool _branch_needs_stairs()
{
    // Irrelevant for branches with a single level and all encompass maps.
    return you.where_are_you != BRANCH_ZIGGURAT;
}

static void _dgn_verify_connectivity(unsigned nvaults)
{
    // After placing vaults, make sure parts of the level have not been
    // disconnected.
    if (dgn_zones && nvaults != env.level_vaults.size())
    {
        const int newzones = dgn_count_disconnected_zones(false);

#ifdef DEBUG_DIAGNOSTICS
        ostringstream vlist;
        for (unsigned i = nvaults; i < env.level_vaults.size(); ++i)
        {
            if (i > nvaults)
                vlist << ", ";
            vlist << env.level_vaults[i]->map.name;
        }
        mprf(MSGCH_DIAGNOSTICS, "Dungeon has %d zones after placing %s.",
             newzones, vlist.str().c_str());
#endif
        if (newzones > dgn_zones)
        {
            throw dgn_veto_exception(make_stringf(
                 "Had %d zones, now has %d%s%s.", dgn_zones, newzones,
#ifdef DEBUG_DIAGNOSTICS
                 "; broken by ", vlist.str().c_str()
#else
                 "", ""
#endif
            ));
        }
    }

    // Also check for isolated regions that have no stairs.
    if (player_in_connected_branch()
        && !(branches[you.where_are_you].branch_flags & BFLAG_ISLANDED)
        && dgn_count_disconnected_zones(true) > 0)
    {
        throw dgn_veto_exception("Isolated areas with no stairs.");
    }

    if (_branch_needs_stairs() && !_fixup_stone_stairs(true))
    {
        dprf("Warning: failed to preserve vault stairs.");
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
//   the vault to use for the temple.  If no map name is supplied,
//   it will randomly pick from vaults tagged "temple_overflow_num",
//   where "num" is the number of gods in the temple.  Gods are listed
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

        const map_def *vault = NULL;
        string vault_tag = "";
        string name = "";

        if (temple.exists(TEMPLE_MAP_KEY))
        {
            name = temple[TEMPLE_MAP_KEY].get_string();

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
                    && you.uniq_map_tags.find("uniq_altar_" + name)
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

                vault = random_map_for_tag(vault_tag, true);
#ifdef DEBUG_TEMPLES
                if (vault == NULL)
                {
                    mprf(MSGCH_DIAGNOSTICS, "Couldn't find overflow temple "
                         "for combination of tags %s", vault_tag.c_str());
                }
#endif
            }

            if (vault == NULL)
            {
                vault_tag = make_stringf("temple_overflow_generic_%d",
                                         num_gods);

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

        {
            dgn_map_parameters mp(vault_tag);
            if (!_dgn_ensure_vault_placed(
                    _build_secondary_vault(vault),
                    false))
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
    _current_temple_hash = NULL; // XXX: hack!
}

struct coord_feat
{
    coord_def pos;
    dungeon_feature_type feat;
    terrain_property_t prop;
    unsigned int mask;

    coord_feat(const coord_def &c, dungeon_feature_type f)
        : pos(c), feat(f), prop(0), mask(0)
    {
    }

    void set_from(const coord_def &c)
    {
        feat = grd(c);
        // Don't copy mimic-ness.
        mask = env.level_map_mask(c) & ~(MMT_MIMIC);
        // Only copy "static" properties.
        prop = env.pgrid(c) & (FPROP_NO_CLOUD_GEN | FPROP_NO_TELE_INTO
                               | FPROP_NO_TIDE | FPROP_NO_SUBMERGE);
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
            if (!feat_is_wall(grd(*ri)) && !feat_is_door(grd(*ri)))
                continue;

            // don't mess with permarock
            if (grd(*ri) == DNGN_PERMAROCK_WALL)
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
                if (!feat_is_wall(grd(*ai)) && !feat_is_door(grd(*ai))
                    && !feat_is_statue_or_idol(grd(*ai))
                    // Shouldn't happen, but just in case.
                    && grd(*ai) != DNGN_MALIGN_GATEWAY)
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

        for (coord_feats::const_iterator it = to_replace.begin();
            it != to_replace.end();
            ++it)
        {
            const coord_def &p(it->pos);
            replaced.push_back(p);
            dungeon_feature_type replacement = it->feat;
            ASSERT(replacement != DNGN_UNSEEN);

            // Don't replace doors with impassable features.
            if (feat_is_door(grd(p)))
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
            if (feat_is_wall(grd(p)) || coinflip() && feat_is_door(grd(p)))
            {
                // Copy the mask and properties too, so that we don't make an
                // isolated transparent or rtele_into square.
                env.level_map_mask(p) |= it->mask;
                env.pgrid(p) |= it->prop;
                _set_grd(p, replacement);
            }

            // but remove doors if we've removed all adjacent walls
            for (adjacent_iterator wai(p); wai; ++wai)
            {
                if (feat_is_door(grd(*wai)))
                {
                    bool remove = true;
                    for (adjacent_iterator dai(*wai); dai; ++dai)
                    {
                        if (feat_is_wall(grd(*dai)))
                            remove = false;
                    }
                    // It's always safe to replace a door with floor.
                    if (remove)
                    {
                        env.level_map_mask(p) |= it->mask;
                        env.pgrid(p) |= it->prop;
                        _set_grd(*wai, DNGN_FLOOR);
                    }
                }
            }
        }
    }

    for (vector<coord_def>::const_iterator it = replaced.begin();
         it != replaced.end();
         ++it)
    {
        const coord_def &p(*it);

        // replace some ruined walls with plants/fungi/bushes
        if (plant_density && one_chance_in(plant_density)
            && feat_has_solid_floor(grd(p))
            && !plant_forbidden_at(p))
        {
            mgen_data mg;
            mg.cls = one_chance_in(20) ? MONS_BUSH  :
                     coinflip()        ? MONS_PLANT :
                     MONS_FUNGUS;
            mg.pos = p;
            mg.flags = MG_FORCE_PLACE;
            mons_place(mgen_data(mg));
        }
    }
}

static bool _mimic_at_level()
{
    return (!player_in_branch(BRANCH_DUNGEON) || you.depth > 1)
           && !player_in_branch(BRANCH_TEMPLE)
           && !player_in_branch(BRANCH_VESTIBULE)
           && !player_in_branch(BRANCH_SLIME)
           && !player_in_branch(BRANCH_TOMB)
           && !player_in_branch(BRANCH_PANDEMONIUM)
           && !player_in_hell()
           && !crawl_state.game_is_tutorial();
}

static void _place_feature_mimics(dungeon_feature_type dest_stairs_type)
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        const coord_def pos = *ri;
        const dungeon_feature_type feat = grd(pos);

        // Vault tag prevents mimic.
        if (map_masked(pos, MMT_NO_MIMIC))
            continue;

        // Only features valid for mimicing.
        if (!is_valid_mimic_feat(feat))
            continue;

        // Reduce the number of stairs and door mimics since those features
        // are very common.
        if ((feat_is_stone_stair(feat) || feat_is_escape_hatch(feat)
             || feat_is_door(feat)) && !one_chance_in(4))
        {
            continue;
        }

        // Don't mimic the stairs the player is going to be placed on.
        if (feat == dest_stairs_type)
            continue;

        // Don't mimic vetoed doors.
        if (door_vetoed(pos))
            continue;

        // Don't mimic staircases in vaults to avoid trapping the player or
        // breaking vault layouts.
        if (map_masked(pos, MMT_VAULT)
            && (feat_is_escape_hatch(feat) || feat_is_stone_stair(feat)))
        {
            continue;
        }

        // If this is the real branch entry, don't mimic it.
        if (feat_is_branch_stairs(feat)
            && level_id::current() == brentry[get_branch_at(pos)])
        {
            continue;
        }

        if (feat_is_stone_stair(feat) || feat_is_escape_hatch(feat))
        {
            // Don't mimic stairs that are about to get removed.
            if (feat_stair_direction(feat) == CMD_GO_DOWNSTAIRS
                && at_branch_bottom())
            {
                continue;
            }

            if (feat_stair_direction(feat) == CMD_GO_UPSTAIRS
                && you.depth <= 1)
            {
                continue;
            }
        }

        // If it is a branch entry, it's been put there for mimicing.
        if (feat_is_branch_stairs(feat) || one_chance_in(FEATURE_MIMIC_CHANCE))
        {
            // For normal stairs, there is a chance to create another mimics
            // elsewhere instead of turning this one. That way, when the 3
            // stairs are grouped and there is another isolated one, any of
            // the 4 staircase can be the mimic.
            if (feat_is_stone_stair(feat) && one_chance_in(4))
            {
                const coord_def new_pos = _place_specific_feature(feat);
                dprf("Placed %s mimic at (%d,%d).",
                     feat_type_name(feat), new_pos.x, new_pos.y);
                env.level_map_mask(new_pos) |= MMT_MIMIC;
                continue;
            }

            dprf("Placed %s mimic at (%d,%d).",
                 feat_type_name(feat), ri->x, ri->y);
            env.level_map_mask(*ri) |= MMT_MIMIC;

            // If we're mimicing a unique portal vault, give a chance for
            // another one to spawn.
            const char* dst = branches[stair_destination(pos).branch].abbrevname;
            const string tag = "uniq_" + lowercase_string(dst);
            if (you.uniq_map_tags.count(tag))
                you.uniq_map_tags.erase(tag);
        }
    }
}

static void _place_item_mimics()
{
    // No mimics on D:1
    if (!env.absdepth0)
        return;

    for (int i = 0; i < MAX_ITEMS; i++)
    {
        item_def& item(mitm[i]);
        if (!item.defined() || !in_bounds(item.pos)
            || item.flags & ISFLAG_NO_MIMIC
            || !is_valid_mimic_item(item)
            || mimic_at(item.pos))
        {
            continue;
        }

        if (one_chance_in(ITEM_MIMIC_CHANCE))
        {
            item.flags |= ISFLAG_MIMIC;
            dprf("Placed a %s mimic at (%d,%d).",
                 item.name(DESC_BASENAME).c_str(), item.pos.x, item.pos.y);
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

static void _build_dungeon_level(dungeon_feature_type dest_stairs_type)
{
    bool place_vaults = _builder_by_type();

    if (player_in_branch(BRANCH_LABYRINTH))
        return;

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
    if (crawl_state.game_standard_levelgen())
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
            _place_feature_mimics(dest_stairs_type);

        _place_traps();

        // Any vault-placement activity must happen before this check.
        _dgn_verify_connectivity(nvaults);

        // Place monsters.
        if (!crawl_state.game_is_zotdef())
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
    fixup_misplaced_items();

    if (crawl_state.game_standard_levelgen())
        _place_gozag_shop(dest_stairs_type);

    link_items();
    if (_mimic_at_level())
        _place_item_mimics();

    if (!player_in_branch(BRANCH_COCYTUS)
        && !player_in_branch(BRANCH_SWAMP)
        && !player_in_branch(BRANCH_SHOALS))
    {
        _prepare_water();
    }

    if (player_in_hell())
        _fixup_hell_stairs();
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
        if (!feat_is_closed_door(grd(*ri)))
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
            if (grd[i][j] == feat)
                ++result;
        }

    return result;
}

// Count how many neighbours of grd[x][y] are the feature feat.
int count_neighbours(int x, int y, dungeon_feature_type feat)
{
    return count_feature_in_box(x-1, y-1, x+2, y+2, feat);
}

// Gives water which is next to ground/shallow water a chance of being
// shallow. Checks each water space.
static void _prepare_water()
{
    dungeon_feature_type which_grid;   // code compaction {dlb}
    int absdepth0 = env.absdepth0;

    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (map_masked(*ri, MMT_NO_POOL))
            continue;

        if (grd(*ri) == DNGN_DEEP_WATER)
        {
            for (adjacent_iterator ai(*ri); ai; ++ai)
            {
                which_grid = grd(*ai);

                // must come first {dlb}
                if (which_grid == DNGN_SHALLOW_WATER
                    && one_chance_in(8 + absdepth0))
                {
                    grd(*ri) = DNGN_SHALLOW_WATER;
                }
                else if (feat_has_dry_floor(which_grid)
                         && x_chance_in_y(80 - absdepth0 * 4,
                                          100))
                {
                    _set_grd(*ri, DNGN_SHALLOW_WATER);
                }
            }
        }
    }
}

static bool _vault_can_use_layout(const map_def *vault, const map_def *layout)
{
    bool permissive = false;
    if (!vault->has_tag_prefix("layout_")
        && !(permissive = vault->has_tag_prefix("nolayout_")))
    {
        return true;
    }

    ASSERT(layout->has_tag_prefix("layout_type_"));

    vector<string> tags = layout->get_tags();

    for (unsigned int i = 0; i < tags.size(); i++)
    {
        if (starts_with(tags[i], "layout_type_"))
        {
            string type = strip_tag_prefix(tags[i], "layout_type_");
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
    const map_def *layout = NULL;

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
        while (layout->has_tag("no_primary_vault")
               || (tries > 10 && !_vault_can_use_layout(vault, layout)));
    }

    return layout;
}

static bool _pan_level()
{
    const char *pandemon_level_names[] =
        { "mnoleg", "lom_lobon", "cerebov", "gloorx_vloq", };
    int which_demon = -1;
    PlaceInfo &place_info = you.get_place_info();
    bool all_demons_generated = true;

    if (you.props.exists("force_map"))
    {
        const map_def *vault =
            find_map_by_name(you.props["force_map"].get_string());
        ASSERT(vault);

        _dgn_ensure_vault_placed(_build_primary_vault(vault), false);
        return vault->orient != MAP_ENCOMPASS;
    }

    for (int i = 0; i < 4; i++)
    {
        if (!you.uniq_map_tags.count(string("uniq_") + pandemon_level_names[i]))
        {
            all_demons_generated = false;
            break;
        }
    }

    // Unique pan lords become more common as you travel through pandemonium.
    // On average it takes 27 levels to see all four, and you're likely to see
    // your first one after about 10 levels.
    if (x_chance_in_y(1 + place_info.levels_seen, 65 + place_info.levels_seen * 2)
        && !all_demons_generated)
    {
        do
            which_demon = random2(4);
        while (you.uniq_map_tags.count(string("uniq_")
                                       + pandemon_level_names[which_demon]));
    }

    const map_def *vault = NULL;

    if (which_demon >= 0)
    {
        vault = random_map_for_tag(pandemon_level_names[which_demon], false,
                                   false, MB_FALSE);
    }
    else
        vault = random_map_in_depth(level_id::current(), false, MB_FALSE);

    // Every Pan level should have a primary vault.
    ASSERT(vault);
    _dgn_ensure_vault_placed(_build_primary_vault(vault), false);
    return vault->orient != MAP_ENCOMPASS;
}

// Returns true if we want the dungeon builder
// to place more vaults after this
static bool _builder_by_type()
{
    if (player_in_branch(BRANCH_LABYRINTH))
    {
        dgn_build_labyrinth_level();
        // Labs placed their minivaults already
        _fixup_branch_stairs();
        return false;
    }
    else if (player_in_branch(BRANCH_ABYSS))
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
        const string name = you.props[TEMPLE_MAP_KEY];

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

    const map_def *vault = 0;

    if (you.props.exists("force_map"))
        vault = find_map_by_name(you.props["force_map"].get_string());
    else if (lid.branch == root_branch && lid.depth == 1
        && (crawl_state.game_is_sprint()
            || crawl_state.game_is_zotdef()
            || crawl_state.game_is_tutorial()))
    {
        vault = find_map_by_name(crawl_state.map);
        if (vault == NULL)
        {
            end(1, false, "Couldn't find selected map '%s'.",
                crawl_state.map.c_str());
        }
    }

    if (!vault)
        // Pick a normal map
        vault = random_map_for_place(lid, minivault, MB_FALSE);

    if (!vault && lid.branch == root_branch && lid.depth == 1)
        vault = random_map_for_tag("arrival", false, false, MB_FALSE);

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
        min_equivalent = NULL;
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
        min_equivalent = NULL;
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
                for (set<int>::iterator i = neighbor_labels.begin();
                     i != neighbor_labels.end();i++)
                {
                    map_component * current = &intermediate_components[*i];

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
    for (map<int, map_component>::iterator i = intermediate_components.begin();
         i != intermediate_components.end(); ++i)
    {
        if (i->second.min_equivalent == NULL)
        {
            i->second.label = reindexed_label++;
            components.push_back(i->second);
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
    bool operator() (const coord_def & pos)
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
    for (unsigned i = 0; i < components.size(); i++)
    {
        // Collect the components in the restricted connectivity map that
        // occupy part of the current component
        map<int, map_component *> present;
        for (rectangle_iterator ri(components[i].min_coord, components[i].max_coord); ri; ++ri)
        {
            int new_label = non_adjacent_connectivity(*ri);
            if (components[i].label == connectivity_map(*ri) && new_label != 0)
            {
                // the bit with new_label - 1 is foolish.
                present[new_label] = &non_adj_components[new_label-1];
            }
        }

        // Set one restricted component as the base point, and search to all
        // other restricted components
        map<int, map_component * >::iterator target_components = present.begin();

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
    for (int i = 0, size = maps.size(); i < size; ++i)
    {
        bool check_fallback = true;
        const map_def *map = maps[i];
        if (!map->map_already_used())
        {
            dprf("Placing CHANCE vault: %s (%s)",
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
                    random_map_for_tag(fallback_tag, true, false, MB_FALSE);
                if (fallback)
                {
                    dprf("Found fallback vault %s for chance tag %s",
                         fallback->name.c_str(), chance_tag.c_str());
                    _build_secondary_vault(fallback);
                }
            }
        }
    }
}

static void _place_minivaults()
{
    const map_def *vault = NULL;
    // First place the vault requested with &P
    if (you.props.exists("force_minivault")
        && (vault = find_map_by_name(you.props["force_minivault"])))
    {
        _dgn_ensure_vault_placed(_build_secondary_vault(vault), false);
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
        while (vault && vault->has_tag("extra") && tries++ < 10000);
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
        vault = random_map_in_depth(level_id::current(), false, MB_FALSE);

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

    _dgn_ensure_vault_placed(_build_primary_vault(vault), false);
    return true;
}

// Used to nuke shafts placed in corridors on low levels - it's just
// too nasty otherwise.
// Well, actually this just checks if it's next to a non-passable
// square. (jpeg)
static bool _shaft_is_in_corridor(const coord_def& c)
{
    for (orth_adjacent_iterator ai(c); ai; ++ai)
    {
        if (!in_bounds(*ai) || grd(*ai) < DNGN_MINWALK)
            return true;
    }
    return false;
}

static void _place_gozag_shop(dungeon_feature_type stair)
{
    string key = make_stringf(GOZAG_SHOP_KEY,
                              level_id::current().describe().c_str());

    if (!you.props.exists(key))
        return;

    bool encompass = false;
    for (string_set::const_iterator i = env.level_layout_types.begin();
         i != env.level_layout_types.end(); ++i)
    {
        if (*i == "encompass")
        {
            encompass = true;
            break;
        }
    }

    vector<coord_weight> places;
    const int dist_max = distance2(coord_def(0, 0), coord_def(20, 20));
    const coord_def start_pos = dgn_find_nearby_stair(stair, you.pos(), true);
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (grd(*ri) != DNGN_FLOOR
            || !(encompass || !map_masked(*ri, MMT_VAULT)))
        {
            continue;
        }
        const int dist2 = distance2(start_pos, *ri);
        if (dist2 > dist_max)
            continue;
        places.push_back(coord_weight(*ri, dist_max - dist2));
    }
    coord_def *shop_place = random_choose_weighted(places);
    if (!shop_place)
        throw dgn_veto_exception("Cannot find place Gozag shop.");


    // Player may have abandoned Gozag before arriving here; only generate
    // the shop if they're still a follower.
    if (you_worship(GOD_GOZAG))
    {
        string spec = you.props[key].get_string();
        keyed_mapspec kmspec;
        kmspec.set_feat(you.props[key].get_string(), false);
        if (!kmspec.get_feat().shop.get())
            die("Invalid shop spec?");
        place_spec_shop(*shop_place, kmspec.get_feat().shop.get());

        shop_struct *shop = get_shop(*shop_place);
        ASSERT(shop);

        env.map_knowledge(*shop_place).set_feature(grd(*shop_place));
        env.map_knowledge(*shop_place).flags |= MAP_MAGIC_MAPPED_FLAG;
        env.pgrid(*shop_place) |= FPROP_SEEN_OR_NOEXP;
        seen_notable_thing(grd(*shop_place), *shop_place);

        string announce = make_stringf(
            "%s invites you to visit their %s%s%s.",
            shop->shop_name.c_str(),
            shop_type_name(shop->type).c_str(),
            !shop->shop_suffix_name.empty() ? " " : "",
            shop->shop_suffix_name.c_str());

        you.props[GOZAG_ANNOUNCE_SHOP_KEY] = announce;

        env.markers.add(new map_feature_marker(*shop_place,
                                               DNGN_ABANDONED_SHOP));
    }
    else
        grd(*shop_place) = DNGN_ABANDONED_SHOP;
}

// Shafts can be generated visible.
//
// Starts about 50% of the time and approaches 0%
static bool _shaft_known(int depth)
{
    return coinflip() && x_chance_in_y(3, depth);
}

static void _place_traps()
{
    const int num_traps = num_traps_for_place();
    int level_number = env.absdepth0;

    ASSERT_RANGE(num_traps, 0, MAX_TRAPS + 1);

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
                && !map_masked(ts.pos, MMT_NO_TRAP))
            {
                break;
            }
        }

        if (tries == 200)
            break;

        while (ts.type >= NUM_TRAPS)
            ts.type = random_trap_for_place();

        if (ts.type == TRAP_SHAFT && level_number <= 7)
        {
            // Disallow shaft construction in corridors!
            if (_shaft_is_in_corridor(ts.pos))
            {
                // Reroll until we get a different type of trap
                while (ts.type == TRAP_SHAFT || ts.type >= NUM_TRAPS)
                    ts.type = random_trap_for_place();
            }
        }

        // Only teleport, shaft, alarm and Zot traps are interesting enough to
        // be placed randomly.  Until the formula is overhauled, let's just
        // skip creation if the old code would pick a boring one.
        if (trap_category(ts.type) == DNGN_TRAP_MECHANICAL)
        {
            ts.type = TRAP_UNASSIGNED;
            continue;
        }

        grd(ts.pos) = DNGN_UNDISCOVERED_TRAP;
        env.tgrid(ts.pos) = i;
        if (ts.type == TRAP_SHAFT && _shaft_known(level_number))
            ts.reveal();
        ts.prepare_ammo();
    }

    if (player_in_branch(BRANCH_SPIDER))
    {
        // Max webs ranges from around 35 (Spider:1) to 220 (Spider:5), actual
        // amount will be much lower.
        int max_webs = 35 * pow(2, (you.depth - 1) / 1.5) - num_traps;
        max_webs /= 2;
        place_webs(max_webs + random2(max_webs));
    }
    else if (player_in_branch(BRANCH_CRYPT))
        place_webs(random2(20));
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

// Create randomly-placed stone stairs.
void dgn_place_stone_stairs(bool maybe_place_hatches)
{
    const int stair_start = DNGN_STONE_STAIRS_DOWN_I;
    const int stair_count = DNGN_ESCAPE_HATCH_UP - stair_start + 1;

    FixedVector < bool, stair_count > existing;

    existing.init(false);

    for (rectangle_iterator ri(0); ri; ++ri)
        if (grd(*ri) >= stair_start && grd(*ri) < stair_start + stair_count)
            existing[grd(*ri) - stair_start] = true;

    int pair_count = 3;

    if (maybe_place_hatches && coinflip())
        pair_count++;

    for (int i = 0; i < pair_count; ++i)
    {
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
                                       uint32_t mapmask,
                                       dungeon_feature_type adjacent_feat,
                                       bool monster_free)
{
    return grd(c) == searchfeat
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

static void _place_branch_entrances(bool use_vaults)
{
    // Find what branch entrances are already placed, and what branch
    // entrances (or mimics thereof) could be placed here.
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
                || level_id::current() == brentry[it->id]))
        {
            could_be_placed = true;
        }
    }

    // If there's nothing to be placed, don't bother.
    if (!could_be_placed)
        return;

    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (!feat_is_branch_stairs(grd(*ri)))
            continue;

        for (branch_iterator it; it; ++it)
            if (it->entry_stairs == grd(*ri)
                && !feature_mimic_at(*ri))
            {
                branch_entrance_placed[it->id] = true;
                break;
            }
    }

    // Place actual branch entrances.
    for (branch_iterator it; it; ++it)
    {
        // Vestibule and hells are placed by other means.
        // Likewise, if we already have an entrance, keep going.
        if (it->id >= BRANCH_VESTIBULE && it->id <= BRANCH_LAST_HELL
            || branch_entrance_placed[it->id])
        {
            continue;
        }

        const bool mimic = !branch_is_unfinished(it->id)
                           && !is_hell_subbranch(it->id)
                           && you.depth >= it->mindepth
                           && you.depth <= it->maxdepth
                           && one_chance_in(FEATURE_MIMIC_CHANCE);

        if (it->entry_stairs != NUM_FEATURES
            && player_in_branch(parent_branch(it->id))
            && (level_id::current() == brentry[it->id] || mimic))
        {
            // Placing a stair.
            dprf("Placing stair to %s", it->shortname);

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
            const coord_def portal_pos = find_portal_place(NULL, false);
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
                                                       false, MB_TRUE);

            // Encompass vaults can't be used as secondaries.
            if (!vault || vault->orient == MAP_ENCOMPASS)
                break;

            if (_build_secondary_vault(vault))
            {
                const map_def &map(*vault);
                if (map.has_tag("extra"))
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
    // Unique beasties:
    if (!your_branch().has_uniques)
        return 0;

#ifdef DEBUG_UNIQUE_PLACEMENT
    FILE *ostat = fopen("unique_placement.log", "a");
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
            dprf("Placed %s.", uniq_map->name.c_str());
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

static void _place_aquatic_in(vector<coord_def> &places, const pop_entry *pop,
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
            static const monster_type lut[3] =
                { MONS_SKELETON, MONS_ZOMBIE, MONS_SIMULACRUM };

            mg.base_type = mg.cls;
            int s = mons_skeleton(mg.cls) ? 2 : 0;
            mg.cls = lut[random_choose_weighted(s, 0, 8, 1, 1, 2, 0)];
        }

        place_monster(mg);
    }
}

static void _place_aquatic_monsters()
{
    // [ds] Shoals relies on normal monster generation to place its monsters.
    // Given the amount of water area in the Shoals, placing water creatures
    // explicitly explodes the Shoals' xp budget.
    //
    // Also disallow water creatures below D:6.
    //
    if (player_in_branch(BRANCH_SHOALS)
        || player_in_branch(BRANCH_ABYSS)
        || player_in_branch(BRANCH_PANDEMONIUM)
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

        dungeon_feature_type feat = grd(*ri);
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

// For Crypt, adds a bunch of skeletons and zombies that do not respect
// absdepth (and thus tend to be varied and include several types that
// would not otherwise spawn there).
static void _place_assorted_zombies()
{
    int num_zombies = random_range(6, 12, 3);
    for (int i = 0; i < num_zombies; ++i)
    {
        bool skel = coinflip();
        monster_type z_base;
        do
            z_base = pick_random_zombie();
        while (mons_class_flag(z_base, M_NO_GEN_DERIVED)
               || !(skel ? mons_skeleton(z_base) : mons_zombifiable(z_base)));

        mgen_data mg;
        mg.cls = (skel ? MONS_SKELETON : MONS_ZOMBIE);
        mg.base_type = z_base;
        mg.behaviour              = BEH_SLEEP;
        mg.map_mask              |= MMT_NO_MONS;
        mg.preferred_grid_feature = DNGN_FLOOR;

        place_monster(mg);
    }
}

static void _place_lost_souls()
{
    int nsouls = random2avg(you.depth + 2, 3);
    for (int i = 0; i < nsouls; ++i)
    {
        mgen_data mg;
        mg.cls = MONS_LOST_SOUL;
        mg.behaviour              = BEH_HOSTILE;
        mg.preferred_grid_feature = DNGN_FLOOR;
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
    const bool in_pan    = player_in_branch(BRANCH_PANDEMONIUM);
    if (in_shoals)
        dgn_shoals_generate_flora();

    // Try to place Shoals monsters on floor where possible instead of
    // letting all the merfolk be generated in the middle of the
    // water.
    const dungeon_feature_type preferred_grid_feature =
        in_shoals ? DNGN_FLOOR : DNGN_UNSEEN;

    dprf("_builder_monsters: Generating %d monsters", mon_wanted);
    for (int i = 0; i < mon_wanted; i++)
    {
        mgen_data mg;
        if (!in_pan)
            mg.behaviour = BEH_SLEEP;
        mg.flags    |= MG_PERMIT_BANDS;
        mg.map_mask |= MMT_NO_MONS;
        mg.preferred_grid_feature = preferred_grid_feature;

        place_monster(mg);
    }

    if (!player_in_branch(BRANCH_CRYPT)) // No water creatures in the Crypt.
        _place_aquatic_monsters();
    else
    {
        _place_assorted_zombies();
        _place_lost_souls();
    }
}

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
        specif_type = OBJ_GOLD;  // Lots of gold in the orcish mines.

    for (i = 0; i < items_wanted; i++)
        items(1, specif_type, OBJ_RANDOM, false, items_levels, MMT_NO_ITEM);
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
           || feat_is_closed_door(grd(c));
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
            if (feat_is_stair(grd(*ri)))
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
    you_teleport_now(false);

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
        return NULL;

    const dgn_colour_override_manager colour_man;

    if (mdef->orient == MAP_ENCOMPASS && !crawl_state.generating_level)
    {
        if (check_collision)
        {
            mprf(MSGCH_DIAGNOSTICS,
                 "Cannot generate encompass map '%s' with check_collision=true",
                 mdef->name.c_str());

            return NULL;
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
                return NULL;
        }
    }
}

vault_placement *dgn_vault_at(coord_def p)
{
    const int map_index = env.level_map_ids(p);
    return map_index == INVALID_MAP_INDEX ? NULL : env.level_vaults[map_index];
}

void dgn_seen_vault_at(coord_def p)
{
    if (vault_placement *vp = dgn_vault_at(p))
    {
        if (!vp->seen)
        {
            dprf("Vault %s (%d,%d)-(%d,%d) seen",
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

    unwind_var<string> placing(env.placing_vault, vault->name);

    vault_placement place;

    if (map_bounds(where))
        place.pos = where;

    const map_section_type placed_vault_orientation =
        vault_main(place, vault, check_collisions);

    dprf("Map: %s; placed: %s; place: (%d,%d), size: (%d,%d)",
         vault->name.c_str(),
         placed_vault_orientation != MAP_NONE ? "yes" : "no",
         place.pos.x, place.pos.y, place.size.x, place.size.y);

    if (placed_vault_orientation == MAP_NONE)
        return NULL;

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
        dprf("Setting the custom random mons list.");
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

#ifdef DEBUG_DIAGNOSTICS
    if (crawl_state.map_stat_gen)
        mapstat_report_map_use(place.map);
#endif

    if (is_layout && place.map.has_tag_prefix("layout_type_"))
    {
        vector<string> tag_list = place.map.get_tags();
        for (unsigned int i = 0; i < tag_list.size(); i++)
        {
            if (starts_with(tag_list[i], "layout_type_"))
            {
                env.level_layout_types.insert(
                    strip_tag_prefix(tag_list[i], "layout_type_"));
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
        _process_disconnected_zones(0, 0, GXM-1, GYM-1, true, DNGN_TREE);
    }

    if (!make_no_exits)
    {
        const bool spotty = player_in_branch(BRANCH_ORC)
#if TAG_MAJOR_VERSION == 34
                            || player_in_branch(BRANCH_FOREST)
#endif
                            || player_in_branch(BRANCH_SWAMP)
                            || player_in_branch(BRANCH_SLIME);
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
        delve(0, ngb_min, ngb_max,
              random_choose(0, 5, 20, 50, 100, -1),
              -1,
              random_choose(1, 20, 125, 500, 999999, -1));
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

static object_class_type _acquirement_object_class()
{
    static const object_class_type classes[] =
    {
        OBJ_JEWELLERY,
        OBJ_BOOKS,
        OBJ_WANDS,
        OBJ_MISCELLANY, // Felids stop here
        OBJ_WEAPONS,
        OBJ_ARMOUR,
        OBJ_STAVES,
    };

    const int nc = (you.species == SP_FELID) ? 4 : ARRAYSZ(classes);
    return classes[random2(nc)];
}

static int _dgn_item_corpse(const item_spec &ispec, const coord_def where)
{
    mons_spec mspec(ispec.corpse_monster_spec());
    int corpse_index = -1;
    for (int tries = 0; ; tries++)
    {
        if (tries > 200)
            return NON_ITEM;
        monster *mon = dgn_place_monster(mspec, coord_def(), true);
        if (!mon)
            continue;
        mon->position = where;
        if (mons_class_can_leave_corpse(mon->type))
            corpse_index = place_monster_corpse(mon, true, true);
        // Dismiss the monster we used to place the corpse.
        mon->flags |= MF_HARD_RESET;
        monster_die(mon, KILL_DISMISSED, NON_MONSTER, false, true);

        if (corpse_index != -1 && corpse_index != NON_ITEM)
            break;
    }

    item_def &corpse(mitm[corpse_index]);
    corpse.props["cap_sacrifice"].get_bool() = true;

    if (ispec.props.exists(CORPSE_NEVER_DECAYS))
    {
        corpse.props[CORPSE_NEVER_DECAYS].get_bool() =
            ispec.props[CORPSE_NEVER_DECAYS].get_bool();
    }

    if (ispec.base_type == OBJ_CORPSES && ispec.sub_type == CORPSE_SKELETON)
        turn_corpse_into_skeleton(corpse);
    else if (ispec.base_type == OBJ_FOOD && ispec.sub_type == FOOD_CHUNK)
        turn_corpse_into_chunks(corpse, false, false);

    if (ispec.props.exists(MONSTER_HIT_DICE))
    {
        corpse.props[MONSTER_HIT_DICE].get_short() =
            ispec.props[MONSTER_HIT_DICE].get_short();
    }

    if (ispec.qty && ispec.base_type == OBJ_FOOD)
        corpse.quantity = ispec.qty;

    return corpse_index;
}

static bool _apply_item_props(item_def &item, const item_spec &spec,
                              bool allow_useless, bool monster)
{
    const CrawlHashTable props = spec.props;

    if (props.exists("make_book_theme_randart"))
    {
        string owner = props["randbook_owner"].get_string();
        if (owner == "player")
            owner = you.your_name;

        vector<spell_type> spells;
        CrawlVector spell_list = props["randbook_spells"].get_vector();
        for (unsigned int i = 0; i < spell_list.size(); ++i)
            spells.push_back((spell_type) spell_list[i].get_int());

        make_book_theme_randart(item,
            spells,
            props["randbook_disc1"].get_short(),
            props["randbook_disc2"].get_short(),
            props["randbook_num_spells"].get_short(),
            props["randbook_slevels"].get_short(),
            owner,
            props["randbook_title"].get_string());
    }

    // Wipe item origin to remove "this is a god gift!" from there,
    // unless we're dealing with a corpse.
    if (!spec.corpselike())
        origin_reset(item);
    if (is_stackable_item(item) && spec.qty > 0)
        item.quantity = spec.qty;

    if (spec.item_special)
        item.special = spec.item_special;

    if (spec.plus >= 0
        && (item.base_type == OBJ_BOOKS
            && item.sub_type == BOOK_MANUAL)
        || (item.base_type == OBJ_MISCELLANY
            && item.sub_type == MISC_RUNE_OF_ZOT))
    {
        item.plus = spec.plus;
        item_colour(item);
    }

    if (item_is_rune(item) && you.runes[item.plus])
    {
        destroy_item(item, true);
        return false;
    }

    if (props.exists("cursed"))
        do_curse_item(item);
    else if (props.exists("uncursed"))
        do_uncurse_item(item, false);
    if (props.exists("useful") && is_useless_item(item, false)
        && !allow_useless)
    {
        destroy_item(item, true);
        return false;
    }
    if (item.base_type == OBJ_WANDS && props.exists("charges"))
        item.plus = props["charges"].get_int();
    if ((item.base_type == OBJ_WEAPONS || item.base_type == OBJ_ARMOUR
         || item.base_type == OBJ_JEWELLERY || item.base_type == OBJ_MISSILES)
        && props.exists("plus") && !is_unrandom_artefact(item))
    {
        item.plus = props["plus"].get_int();
    }
    if (props.exists("ident"))
        item.flags |= props["ident"].get_int();
    if (props.exists("unobtainable"))
        item.flags |= ISFLAG_UNOBTAINABLE;

    if (props.exists("no_pickup"))
        item.flags |= ISFLAG_NO_PICKUP;

    if (props.exists("item_tile_name"))
        item.props["item_tile_name"] = props["item_tile_name"].get_string();
    if (props.exists("worn_tile_name"))
        item.props["worn_tile_name"] = props["worn_tile_name"].get_string();
    bind_item_tile(item);

    if (!monster)
    {
        if (props.exists("mimic"))
        {
            const int chance = props["mimic"];
            if (chance > 0 && one_chance_in(chance))
                item.flags |= ISFLAG_MIMIC;
        }
        if (props.exists("no_mimic"))
            item.flags |= ISFLAG_NO_MIMIC;
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
            9, OBJ_STAVES,
            1, OBJ_RODS,
            10, OBJ_MISCELLANY,
            0);
}

int dgn_place_item(const item_spec &spec,
                   const coord_def &where,
                   int level)
{
    // Dummy object?
    if (spec.base_type == OBJ_UNASSIGNED)
        return NON_ITEM;

    if (level == INVALID_ABSDEPTH)
        level = env.absdepth0;

    object_class_type base_type = spec.base_type;
    bool acquire = false;

    if (spec.level >= 0)
        level = spec.level;
    else
    {
        bool adjust_type = false;
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
            adjust_type = true;
            level = MAKE_GOOD_ITEM;
            break;
        case ISPEC_ACQUIREMENT:
            adjust_type = true;
            acquire = true;
            break;
        default:
            break;
        }

        if (spec.props.exists("mimic") && base_type == OBJ_RANDOM)
            base_type = get_random_item_mimic_type();
        else if (adjust_type && base_type == OBJ_RANDOM)
        {
            base_type = acquire ? _acquirement_object_class()
                                : _superb_object_class();
        }
    }

    int useless_tries = 0;

    while (true)
    {
        const int item_made =
            (acquire ?
             acquirement_create_item(base_type, spec.acquirement_source,
                                     true, where)
             : spec.corpselike() ? _dgn_item_corpse(spec, where)
             : items(spec.allow_uniques, base_type,
                     spec.sub_type, true, level, 0, spec.ego, -1,
                     spec.level == ISPEC_MUNDANE));

        if (item_made == NON_ITEM || item_made == -1)
            return NON_ITEM;
        else
        {
            item_def &item(mitm[item_made]);
            item.pos = where;

            if (_apply_item_props(item, spec, (useless_tries >= 10), false))
                return item_made;
            else
            {
                if (base_type == OBJ_MISCELLANY
                    && spec.sub_type == MISC_RUNE_OF_ZOT)
                {
                    return NON_ITEM;
                }

                useless_tries++;
            }

        }

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

static void _dgn_give_mon_spec_items(mons_spec &mspec,
                                     monster *mon,
                                     const monster_type type)
{
    ASSERT(mspec.place.is_valid());

    unwind_var<int> save_speedinc(mon->speed_increment);

    // Get rid of existing equipment.
    for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
        if (mon->inv[i] != NON_ITEM)
        {
            item_def &item(mitm[mon->inv[i]]);
            mon->unequip(item, i, 0, true);
            destroy_item(mon->inv[i], true);
            mon->inv[i] = NON_ITEM;
        }

    item_list &list = mspec.items;

    const int size = list.size();
    for (int i = 0; i < size; ++i)
    {
        item_spec spec = list.get_item(i);

        if (spec.base_type == OBJ_UNASSIGNED
            || (spec.base_type == OBJ_MISCELLANY && spec.sub_type == MISC_RUNE_OF_ZOT))
        {
            continue;
        }

        // Don't give monster a randart, and don't randomly give
        // monster an ego item.
        if (spec.base_type == OBJ_ARMOUR || spec.base_type == OBJ_WEAPONS
            || spec.base_type == OBJ_MISSILES)
        {
            spec.allow_uniques = 0;
            if (spec.ego == 0)
                spec.ego = SP_FORBID_EGO;
        }

        int item_level = mspec.place.absdepth();

        if (spec.level >= 0)
            item_level = spec.level;
        else
        {
            // TODO: merge this with the equivalent switch in dgn_place_item,
            // and maybe even handle ISPEC_ACQUIREMENT.
            switch (spec.level)
            {
            case ISPEC_GOOD:
                item_level = 5 + item_level * 2;
                break;
            case ISPEC_SUPERB:
                item_level = MAKE_GOOD_ITEM;
                break;
            case ISPEC_DAMAGED:
            case ISPEC_BAD:
            case ISPEC_RANDART:
                item_level = spec.level;
                break;
            }
        }

        for (int useless_tries = 0; true; useless_tries++)
        {
            int item_made;

            if (spec.corpselike())
                item_made = _dgn_item_corpse(spec, mon->pos());
            else
            {
                item_made = items(spec.allow_uniques, spec.base_type,
                                  spec.sub_type, true, item_level, 0, spec.ego,
                                  -1, spec.level == ISPEC_MUNDANE);
            }

            if (!(item_made == NON_ITEM || item_made == -1))
            {
                item_def &item(mitm[item_made]);

                if (_apply_item_props(item, spec, (useless_tries >= 10), true))
                {
                    // Mark items on summoned monsters as such.
                    if (mspec.abjuration_duration != 0)
                        item.flags |= ISFLAG_SUMMONED;

                    if (!mon->pickup_item(item, 0, true))
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

    if (type == RANDOM_SUPER_OOD || type == RANDOM_MODERATE_OOD)
    {
        if (brdepth[mspec.place.branch] <= 1)
            ; // no OODs here
        else if (type == RANDOM_SUPER_OOD)
            mspec.place.depth += 4 + mspec.place.depth;
        else if (type == RANDOM_MODERATE_OOD)
            mspec.place.depth += 5;
        type = RANDOM_MONSTER;
    }

    if (type != RANDOM_MONSTER && type < NUM_MONSTERS)
    {
        // Don't place a unique monster a second time.
        // (Boris is handled specially.)
        if (mons_is_unique(type) && you.unique_creatures[type]
            && !crawl_state.game_is_arena())
        {
            return 0;
        }

        const monster_type montype = mons_class_is_zombified(type)
                                                         ? mspec.monbase
                                                         : type;

        const habitat_type habitat = mons_class_primary_habitat(montype);

        if (in_bounds(where) && !monster_habitable_grid(montype, grd(where)))
            dungeon_terrain_changed(where, habitat2grid(habitat), !crawl_state.generating_level);
    }

    if (type == RANDOM_MONSTER)
    {
        if (mons_class_is_chimeric(mspec.monbase))
        {
            type = mspec.monbase;
            mspec.chimera_mons.clear();
            for (int n = 0; n < NUM_CHIMERA_HEADS; n++)
            {
                monster_type part = chimera_part_for_place(mspec.place, mspec.monbase);
                if (part != MONS_0)
                    mspec.chimera_mons.push_back(part);
            }
        }
        else
        {
            type = pick_random_monster(mspec.place, mspec.monbase);
            if (!type)
                type = RANDOM_MONSTER;
        }
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
    case ATT_STRICT_NEUTRAL:
        mg.behaviour = BEH_STRICT_NEUTRAL;
        break;
    default:
        break;
    }
    mg.base_type = mspec.monbase;
    mg.number    = mspec.number;
    mg.colour    = mspec.colour;

    if (mspec.god != GOD_NO_GOD)
        mg.god   = mspec.god;

    mg.mname     = mspec.monname;
    mg.hd        = mspec.hd;
    mg.hp        = mspec.hp;
    mg.props     = mspec.props;
    mg.initial_shifter = mspec.initial_shifter;
    mg.chimera_mons = mspec.chimera_mons;

    // Marking monsters as summoned
    mg.abjuration_duration = mspec.abjuration_duration;
    mg.summon_type         = mspec.summon_type;
    mg.non_actor_summoner  = mspec.non_actor_summoner;

    // XXX: hack (also, never hand out darkgrey)
    if (mg.colour == -1)
        mg.colour = random_monster_colour();

    if (!force_pos && monster_at(where)
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
        mons->props["custom_spells"] = true;
    }

    if (!mspec.items.empty())
        _dgn_give_mon_spec_items(mspec, mons, type);

    if (mspec.props.exists("monster_tile"))
    {
        mons->props["monster_tile"] =
            mspec.props["monster_tile"].get_short();
    }
    if (mspec.props.exists("monster_tile_name"))
    {
        mons->props["monster_tile_name"].get_string() =
            mspec.props["monster_tile_name"].get_string();
    }

    if (mspec.props.exists("always_corpse"))
        mons->props["always_corpse"] = true;

    if (mspec.props.exists("never_corpse"))
        mons->props["never_corpse"] = true;

    if (mspec.props.exists("dbname"))
        mons->props["dbname"].get_string() = mspec.props["dbname"].get_string();

    // These are applied earlier to prevent issues with renamed monsters
    // and "<monster> comes into view" (see delay.cc:_monster_warning).
    //mons->flags |= mspec.extra_monster_flags;

    // Monsters with gods set by the spec aren't god gifts
    // unless they have the "god_gift" tag.  place_monster(),
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
            mons->ghost->init_spectral_weapon(*wpn, 100, 270);
        mons->ghost_demon_init();
    }

    for (unsigned int i = 0; i < mspec.ench.size(); i++)
        mons->add_ench(mspec.ench[i]);

    return mons;
}

static bool _dgn_place_monster(const vault_placement &place, mons_spec &mspec,
                               const coord_def& where)
{
    const bool generate_awake
        = mspec.generate_awake || place.map.has_tag("generate_awake");

    const bool patrolling
        = mspec.patrolling || place.map.has_tag("patrolling");

    mspec.props["map"].get_string() = place.map_name_at(where);
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
static dungeon_feature_type _glyph_to_feat(int glyph,
                                           vault_placement *place = NULL)
{
    return (glyph == 'x') ? DNGN_ROCK_WALL :
           (glyph == 'X') ? DNGN_PERMAROCK_WALL :
           (glyph == 'c') ? DNGN_STONE_WALL :
           (glyph == 'v') ? DNGN_METAL_WALL :
           (glyph == 'b') ? DNGN_GREEN_CRYSTAL_WALL :
           (glyph == 'm') ? DNGN_CLEAR_ROCK_WALL :
           (glyph == 'n') ? DNGN_CLEAR_STONE_WALL :
           (glyph == 'o') ? DNGN_CLEAR_PERMAROCK_WALL :
           (glyph == 't') ? DNGN_TREE :
           (glyph == '+') ? DNGN_CLOSED_DOOR :
           (glyph == '=') ? DNGN_RUNED_DOOR :
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

    keyed_mapspec *mapsp = map? map->mapspec_at(c) : NULL;
    if (mapsp)
    {
        feature_spec f = mapsp->get_feat();
        if (f.trap.get())
        {
            // f.feat == 1 means trap is generated known.
            if (f.feat == 1)
                return trap_category(static_cast<trap_type>(f.trap.get()->tr_type));
            else
                return DNGN_UNDISCOVERED_TRAP;
        }
        else if (f.feat >= 0)
            return static_cast<dungeon_feature_type>(f.feat);
        else if (f.glyph >= 0)
            return map_feature_at(NULL, c, f.glyph);
        else if (f.shop.get())
            return DNGN_ENTER_SHOP;

        return DNGN_FLOOR;
    }

    return _glyph_to_feat(rawfeat);
}

static void _vault_grid_mapspec(vault_placement &place, const coord_def &where,
                                keyed_mapspec& mapsp)
{
    const feature_spec f = mapsp.get_feat();
    if (f.trap.get())
    {
        trap_spec* spec = f.trap.get();
        if (spec)
            _place_specific_trap(where, spec);

        // f.feat == 1 means trap is generated known.
        if (f.feat == 1)
            grd(where) = trap_category(spec->tr_type);
    }
    else if (f.feat >= 0)
        grd(where) = static_cast<dungeon_feature_type>(f.feat);
    else if (f.glyph >= 0)
        _vault_grid_glyph(place, where, f.glyph);
    else if (f.shop.get())
        place_spec_shop(where, f.shop.get());
    else
        grd(where) = DNGN_FLOOR;

    if (f.mimic > 0 && one_chance_in(f.mimic))
    {
        ASSERT(!feat_cannot_be_mimic(grd(where)));
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
        grd(where) = _glyph_to_feat(vgrid, &place);

    if (feat_is_altar(grd(where))
        && is_unavailable_god(feat_altar_god(grd(where))))
    {
        grd(where) = DNGN_FLOOR;
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
    case '~':
        place_specific_trap(where, random_trap_for_place());
        break;
    case 'B':
        grd(where) = _pick_temple_altar(place);
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
            which_depth = MAKE_GOOD_ITEM;
        }
        else if (vgrid == '*')
            which_depth = 5 + which_depth * 2;

        item_made = items(1, which_class, which_type, true, which_depth);
        if (item_made != NON_ITEM)
            mitm[item_made].pos = where;
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
        delete_cloud_at(where);
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
            set_terrain_changed(*ri);
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
        if (grd(*ri) == replace && !map_masked(*ri, mapmask))
        {
            grd(*ri) = feature;
            if (needs_update && env.map_knowledge(*ri).seen())
            {
                env.map_knowledge(*ri).set_feature(feature, 0,
                                                   get_trap_type(*ri));
#ifdef USE_TILE
                env.tile_bk_bg(*ri) = feature;
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
        cur.insert(cur.begin() + random2(cur.size()), *ai);
    }
    for (vector<coord_def>::const_iterator ci = cur.begin();
         ci != cur.end(); ci++)
    {
        coords.insert(*ci);

        const coord_def dp = *ci - c;
        travel_point_distance[ci->x][ci->y] = (-dp.x + 2) * 4 + (-dp.y + 2);
    }
}

// Resets travel_point_distance
vector<coord_def> dgn_join_the_dots_pathfind(const coord_def &from,
                                             const coord_def &to,
                                             uint32_t mapmask)
{
    memset(travel_point_distance, 0, sizeof(travel_distance_grid_t));
    const coord_comparator comp(to);
    coord_set coords(comp);

    vector<coord_def> path;
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
        return path;

    while (curr != from)
    {
        if (!map_masked(curr, mapmask))
            path.push_back(curr);

        const int dist = travel_point_distance[curr.x][curr.y];
        ASSERT(dist < 0);
        ASSERT(dist != -1000);
        curr += coord_def(-dist / 4 - 2, (-dist % 4) - 2);
    }
    if (!map_masked(curr, mapmask))
        path.push_back(curr);

    return path;
}

bool join_the_dots(const coord_def &from, const coord_def &to,
                   uint32_t mapmask,
                   bool (*overwriteable)(dungeon_feature_type))
{
    if (!overwriteable)
        overwriteable = _feat_is_wall_floor_liquid;

    const vector<coord_def> path =
        dgn_join_the_dots_pathfind(from, to, mapmask);

    for (vector<coord_def>::const_iterator i = path.begin(); i != path.end();
         ++i)
    {
        if (!map_masked(*i, mapmask) && overwriteable(grd(*i)))
        {
            grd(*i) = DNGN_FLOOR;
            dgn_height_set_at(*i);
        }
    }

    return !path.empty() || from == to;
}

static dungeon_feature_type _pick_temple_altar(vault_placement &place)
{
    if (_temple_altar_list.empty())
    {
        if (_current_temple_hash != NULL)
        {
            // Altar god doesn't matter, setting up the whole machinery would
            // be too much work.
            if (crawl_state.map_stat_gen || crawl_state.obj_stat_gen)
                return DNGN_ALTAR_XOM;

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

    if (player_in_branch(BRANCH_TEMPLE)
        || player_in_branch(BRANCH_LABYRINTH))
    {
        // No extra altars in Temple, none at all in Labyrinth.
        god = GOD_NO_GOD;
    }
    else if (player_in_connected_branch() && !one_chance_in(5))
    {
        switch (you.where_are_you)
        {
        case BRANCH_CRYPT:
            god = (coinflip() ? GOD_KIKUBAAQUDGHA
                              : GOD_YREDELEMNUL);
            break;

        case BRANCH_ORC: // violent gods (50% chance of Beogh)
            if (coinflip())
                god = GOD_BEOGH;
            else
                god = random_choose(GOD_VEHUMET, GOD_MAKHLEB, GOD_OKAWARU,
                                    GOD_TROG,    GOD_XOM,     -1);
            break;

        case BRANCH_VAULTS: // lawful gods
            god = random_choose_weighted(2, GOD_OKAWARU,
                                         2, GOD_ZIN,
                                         1, GOD_ELYVILON,
                                         1, GOD_SIF_MUNA,
                                         1, GOD_SHINING_ONE,
                                         0);
            break;

        case BRANCH_ELF: // magic gods
            god = random_choose(GOD_VEHUMET, GOD_SIF_MUNA, GOD_XOM,
                                GOD_MAKHLEB, -1);
            break;

        case BRANCH_SLIME:
            god = GOD_JIYVA;
            break;

        case BRANCH_TOMB:
            god = GOD_KIKUBAAQUDGHA;
            break;

        default:
            do
                god = random_god();
            while (god == GOD_NEMELEX_XOBEH
                   || god == GOD_LUGONU
                   || god == GOD_BEOGH
                   || god == GOD_JIYVA);
            break;
        }
    }
    else
    {
        // Note: this case includes Pandemonium or the Abyss.
        god = random_choose(GOD_ZIN,      GOD_SHINING_ONE, GOD_KIKUBAAQUDGHA,
                            GOD_XOM,      GOD_OKAWARU,     GOD_MAKHLEB,
                            GOD_SIF_MUNA, GOD_TROG,        GOD_ELYVILON,
                            -1);
    }

    if (is_unavailable_god(god))
        god = GOD_NO_GOD;

    return altar_for_god(god);
}

static bool _need_varied_selection(shop_type shop)
{
    return shop == SHOP_BOOK;
}

void place_spec_shop(const coord_def& where,
                     int force_s_type, bool representative)
{
    shop_spec spec(static_cast<shop_type>(force_s_type));
    place_spec_shop(where, &spec, representative);
}

int greed_for_shop_type(shop_type shop, int level_number)
{
    if (shop == SHOP_FOOD)
        return 10 + random2(5);
    if (shop == SHOP_WEAPON_ANTIQUE
        || shop == SHOP_ARMOUR_ANTIQUE
        || shop == SHOP_GENERAL_ANTIQUE)
    {
        return 15 + random2avg(19, 2) + random2(level_number);
    }
    return 10 + random2(5) + random2(level_number / 2);
}

void place_spec_shop(const coord_def& where,
                     shop_spec* spec, bool representative)
{
    int level_number = env.absdepth0;
    int force_s_type = static_cast<int>(spec->sh_type);

    int orb = 0;
    int i = 0;
    int j = 0;                  // loop variable
    int item_level;

    bool note_status = notes_are_active();
    activate_notes(false);

    for (i = 0; i < MAX_SHOPS; i++)
        if (env.shop[i].type == SHOP_UNASSIGNED)
            break;

    if (i == MAX_SHOPS)
        return;

    for (j = 0; j < 3; j++)
        env.shop[i].keeper_name[j] = 1 + random2(200);

    env.shop[i].shop_name = spec->name;
    env.shop[i].shop_type_name = spec->type;
    env.shop[i].shop_suffix_name = spec->suffix;
    env.shop[i].level = level_number * 2;

    env.shop[i].type = static_cast<shop_type>(
        (force_s_type != SHOP_RANDOM) ? force_s_type
                                      : random2(NUM_SHOPS));

    env.shop[i].greed = greed_for_shop_type(env.shop[i].type, level_number);

    // Allow bargains in bazaars, prices randomly between 60% and 95%.
    if (player_in_branch(BRANCH_BAZAAR))
    {
        // Need to calculate with factor as greed (uint8_t)
        // is capped at 255.
        int factor = random2(8) + 12;

        dprf("Shop type %d: original greed = %d, factor = %d, discount = %d%%.",
             env.shop[i].type, env.shop[i].greed, factor, (20-factor)*5);

        factor *= env.shop[i].greed;
        factor /= 20;
        env.shop[i].greed = factor;
    }

    if (spec->greed != -1)
    {
        dprf("Shop spec overrides greed: %d becomes %d.", env.shop[i].greed, spec->greed);
        env.shop[i].greed = spec->greed;
    }

    int plojy = 5 + random2avg(12, 3);
    if (representative)
        plojy = env.shop[i].type == SHOP_EVOKABLES ? NUM_WANDS : 16;

    if (spec->use_all && !spec->items.empty())
    {
        dprf("Shop spec wants all items placed: %d becomes %u.", plojy,
             (unsigned int)spec->items.size());
        plojy = (int) spec->items.size();
    }

    if (spec->num_items != -1)
    {
        dprf("Shop spec overrides number of items: %d becomes %d.", plojy, spec->num_items);
        plojy = spec->num_items;
    }

    // For books shops, store how many copies of a given book are on display.
    // This increases the diversity of books in a shop.
    int stocked[NUM_BOOKS];
    if (_need_varied_selection(env.shop[i].type))
    {
        for (int k = 0; k < NUM_BOOKS; k++)
             stocked[k] = 0;
    }

    coord_def stock_loc = coord_def(0, 5+i);

    for (j = 0; j < plojy; j++)
    {
        if (env.shop[i].type != SHOP_WEAPON_ANTIQUE
            && env.shop[i].type != SHOP_ARMOUR_ANTIQUE
            && env.shop[i].type != SHOP_GENERAL_ANTIQUE)
        {
            item_level = level_number + random2((level_number + 1) * 2);
        }
        else
            item_level = level_number + random2((level_number + 1) * 3);

        // Make bazaar items more valuable (up to double value).
        if (player_in_branch(BRANCH_BAZAAR))
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
            const object_class_type basetype =
                (representative && env.shop[i].type == SHOP_EVOKABLES)
                ? OBJ_WANDS
                : item_in_shop(env.shop[i].type);
            const int subtype = representative? j : OBJ_RANDOM;

            if (!spec->items.empty() && !spec->use_all)
            {
                orb = dgn_place_item(spec->items.random_item_weighted(),
                        stock_loc, item_level);
            }
            else if (!spec->items.empty() && spec->use_all && j < (int)spec->items.size())
                orb = dgn_place_item(spec->items.get_item(j), stock_loc, item_level);
            else
            {
                orb = items(1, basetype, subtype, true,
                            one_chance_in(4) ? MAKE_GOOD_ITEM : item_level);
            }

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
                        && mitm[orb].base_type != OBJ_FOOD)
                    || !spec->items.empty()))
            {
                break;
            }

            // Reset object and try again.
            if (orb != NON_ITEM)
                mitm[orb].clear();
        }

        if (orb == NON_ITEM)
            break;

        item_def& item(mitm[orb]);

        // Increase stock of this subtype by 1, unless it is an artefact
        // (allow for several artefacts of the same underlying subtype)
        // - the latter is currently unused but would apply to e.g. jewellery.
        if (_need_varied_selection(env.shop[i].type) && !is_artefact(item))
            stocked[item.sub_type]++;

        if (representative && item.base_type == OBJ_WANDS)
            item.plus = 7;

        // Set object 'position' (gah!) & ID status.
        item.pos = stock_loc;

        if (env.shop[i].type != SHOP_WEAPON_ANTIQUE
            && env.shop[i].type != SHOP_ARMOUR_ANTIQUE
            && env.shop[i].type != SHOP_GENERAL_ANTIQUE)
        {
            set_ident_flags(item, ISFLAG_IDENT_MASK);
        }
    }

    env.shop[i].pos = where;
    env.tgrid(where) = i;

    _set_grd(where, DNGN_ENTER_SHOP);

    activate_notes(note_status);
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
        return OBJ_JEWELLERY;

    case SHOP_EVOKABLES:
        if (one_chance_in(10))
            return OBJ_RODS;
        return coinflip() ? OBJ_WANDS : OBJ_MISCELLANY;

    case SHOP_BOOK:
        return OBJ_BOOKS;

    case SHOP_FOOD:
        return OBJ_FOOD;

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
#if TAG_MAJOR_VERSION == 34
           || player_in_branch(BRANCH_FOREST) && feat == DNGN_TREE
#endif
           || player_in_branch(BRANCH_SWAMP) && feat == DNGN_TREE
           || feat_is_lava(feat)
           || feat_is_wall(feat)
           || feat == DNGN_FLOOR;
}

// Connect vault exit "from" to dungeon floor by growing a spotty chamber.
// This tries to be like _spotty_level, but probably isn't quite.
// It might be better to aim for a more open connection -- currently
// it stops pretty much as soon as connectivity is attained.
static set<coord_def> _dgn_spotty_connect_path(const coord_def& from,
            bool (*overwriteable)(dungeon_feature_type))
{
    set<coord_def> flatten;
    set<coord_def> border;
    set<coord_def>::const_iterator it;
    bool success = false;

    if (!overwriteable)
        overwriteable = _feat_is_wall_floor_liquid;

    for (adjacent_iterator ai(from); ai; ++ai)
        if (!map_masked(*ai, MMT_VAULT) && _spotty_seed_ok(*ai))
            border.insert(*ai);

    while (!success && !border.empty())
    {
        coord_def cur;
        int count = 0;
        for (it = border.begin(); it != border.end(); ++it)
            if (one_chance_in(++count))
                cur = *it;
        border.erase(border.find(cur));

        // Flatten orthogonal neighbours, and add new neighbours to border.
        flatten.insert(cur);
        for (orth_adjacent_iterator ai(cur); ai; ++ai)
        {
            if (map_masked(*ai, MMT_VAULT))
                continue;

            if (grd(*ai) == DNGN_FLOOR)
                success = true; // Through, but let's remove the others, too.

            if (!overwriteable(grd(*ai)) || flatten.count(*ai))
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
                            bool (*overwriteable)(dungeon_feature_type))
{
    const set<coord_def> spotty_path =
        _dgn_spotty_connect_path(from, overwriteable);

    if (!spotty_path.empty())
    {
        for (set<coord_def>::const_iterator it = spotty_path.begin();
             it != spotty_path.end(); ++it)
        {
            grd(*it) =
                (player_in_branch(BRANCH_SWAMP) && one_chance_in(3))
                ? DNGN_SHALLOW_WATER
                : DNGN_FLOOR;
            dgn_height_set_at(*it);
        }
    }

    return !spotty_path.empty();
}

bool place_specific_trap(const coord_def& where, trap_type spec_type, int charges)
{
    trap_spec spec(spec_type);

    return _place_specific_trap(where, &spec, charges);
}

static bool _place_specific_trap(const coord_def& where, trap_spec* spec, int charges)
{
    trap_type spec_type = spec->tr_type;

    bool no_tele = spec_type == TRAP_NONTELEPORT;
    bool no_shaft = no_tele || !is_valid_shaft_level();

    while (spec_type >= NUM_TRAPS
#if TAG_MAJOR_VERSION == 34
           || spec_type == TRAP_DART || spec_type == TRAP_GAS
#endif
           || no_tele && spec_type == TRAP_TELEPORT
           || no_shaft && spec_type == TRAP_SHAFT)
    {
        spec_type = static_cast<trap_type>(random2(TRAP_MAX_REGULAR + 1));
    }

    for (int tcount = 0; tcount < MAX_TRAPS; tcount++)
        if (env.trap[tcount].type == TRAP_UNASSIGNED)
        {
            env.trap[tcount].type = spec_type;
            env.trap[tcount].pos  = where;
            grd(where)            = DNGN_UNDISCOVERED_TRAP;
            env.tgrid(where)      = tcount;
            env.trap[tcount].prepare_ammo(charges);
            return true;
        }

    return false;
}

static void _add_plant_clumps(int frequency /* = 10 */,
                              int clump_density /* = 12 */,
                              int clump_radius /* = 4 */)
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        mgen_data mg;
        mg.flags = MG_FORCE_PLACE;
        if (mgrd(*ri) != NON_MONSTER && !map_masked(*ri, MMT_VAULT))
        {
            // clump plants around things that already exist
            monster_type type = menv[mgrd(*ri)].type;
            if ((type == MONS_PLANT
                     || type == MONS_FUNGUS
                     || type == MONS_BUSH)
                 && one_chance_in(frequency))
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
                if (grd(*rad) != DNGN_FLOOR)
                    continue;

                // make sure the iterator stays valid
                vector<coord_def> more_to_place;
                for (vector<coord_def>::const_iterator it = to_place.begin();
                     it != to_place.end();
                     ++it)
                {
                    if (*rad == *it)
                        continue;
                    // only place plants next to previously placed plants
                    if (abs(rad->x - it->x) <= 1 && abs(rad->y - it->y) <= 1)
                    {
                        if (one_chance_in(clump_density))
                            more_to_place.push_back(*rad);
                    }
                }
                to_place.insert(to_place.end(), more_to_place.begin(),
                                more_to_place.end());
            }
        }

        for (vector<coord_def>::const_iterator it = to_place.begin();
             it != to_place.end();
             ++it)
        {
            if (*it == *ri)
                continue;
            if (plant_forbidden_at(*it))
                continue;
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

static coord_def _get_hatch_dest(coord_def base_pos, bool shaft)
{
    map_marker *marker = env.markers.find(base_pos, MAT_POSITION);
    if (!marker || shaft)
    {
        coord_def dest_pos;
        do
            dest_pos = random_in_bounds();
        while (grd(dest_pos) != DNGN_FLOOR
               || env.pgrid(dest_pos) & FPROP_NO_RTELE_INTO);
        if (!shaft)
        {
            env.markers.add(new map_position_marker(base_pos, dest_pos));
            env.markers.clear_need_activate();
        }
        return dest_pos;
    }
    else
    {
        map_position_marker *posm = dynamic_cast<map_position_marker*>(marker);
        return posm->dest;
    }
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
        if (map_bounds_with_margin(res, margin))
            return res;
    }
    return coord_def();
}

coord_def dgn_find_feature_marker(dungeon_feature_type feat)
{
    vector<map_marker*> markers = env.markers.get_all();
    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        map_marker *mark = markers[i];
        if (mark->get_type() == MAT_FEATURE
            && dynamic_cast<map_feature_marker*>(mark)->feat == feat)
        {
            return mark->pos;
        }
    }
    coord_def unfound;
    return unfound;
}

static coord_def _dgn_find_labyrinth_entry_point()
{
    return dgn_find_feature_marker(DNGN_ENTER_LABYRINTH);
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
        int walls = 0;
        for (adjacent_iterator bi(*ai); bi && walls < max_walls; ++bi)
            if (env.grid(*bi) == DNGN_SLIMY_WALL)
                walls++;
        if (walls < max_walls)
        {
            *pos = *ai;
            max_walls = walls;
        }
    }
    ASSERT(max_walls < 9);
}

coord_def dgn_find_nearby_stair(dungeon_feature_type stair_to_find,
                                coord_def base_pos, bool find_closest)
{
    dprf("Level entry point on %sstair: %d (%s)",
         find_closest ? "closest " : "",
         stair_to_find, dungeon_feature_name(stair_to_find));

    // Shafts and hatches.
    if (stair_to_find == DNGN_ESCAPE_HATCH_UP
        || stair_to_find == DNGN_ESCAPE_HATCH_DOWN
        || stair_to_find == DNGN_TRAP_SHAFT)
    {
        coord_def pos(_get_hatch_dest(base_pos, stair_to_find == DNGN_TRAP_SHAFT));
        if (player_in_branch(BRANCH_SLIME))
            _fixup_slime_hatch_dest(&pos);
        if (in_bounds(pos))
            return pos;
    }

    if (stair_to_find == DNGN_STONE_ARCH)
    {
        const coord_def pos(dgn_find_feature_marker(stair_to_find));
        if (in_bounds(pos) && grd(pos) == stair_to_find)
            return pos;
    }

    if (player_in_branch(BRANCH_LABYRINTH))
    {
        const coord_def pos(_dgn_find_labyrinth_entry_point());
        if (in_bounds(pos))
            return pos;

        // Couldn't find a good place, warn, and use old behaviour.
        dprf("Oops, couldn't find labyrinth entry marker.");
        stair_to_find = DNGN_FLOOR;
    }

    if (stair_to_find == your_branch().exit_stairs)
    {
        const coord_def pos(dgn_find_feature_marker(DNGN_STONE_STAIRS_UP_I));
        if (in_bounds(pos) && grd(pos) == stair_to_find)
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

            if (stair_to_find <= DNGN_ESCAPE_HATCH_DOWN)
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

    // Look for any clear terrain and abandon the idea of looking
    // nearby now. This is used when taking transit Pandemonium gates,
    // or landing in Labyrinths. Currently the player can land in vaults,
    // which is considered acceptable.
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (feat_has_dry_floor(grd(*ri)))
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
    ASSERT(!func_name.empty());

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
    for (dgn_region_list::const_iterator i = regions.begin();
         i != regions.end(); ++i)
    {
        if (overlaps(*i))
            return true;
    }
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
    return x_chance_in_y(size.x, size.x + size.y) ?
                  coord_def(pos.x + random2(size.x),
                             coinflip()? pos.y : pos.y + size.y - 1)
                : coord_def(coinflip()? pos.x : pos.x + size.x - 1,
                             pos.y + random2(size.y));
}

coord_def dgn_region::random_point() const
{
    return coord_def(pos.x + random2(size.x), pos.y + random2(size.y));
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
    // as much as possible.  At a minimum, it ensures a path from the bottom
    // of a branch to the top of a branch.  If this is not possible, it
    // returns false.
    //
    // Note: this check is undirectional and assumes that levels below this
    // one have not been created yet.  If this is not the case, it will not
    // guarantee or preserve connectivity.
    //
    // XXX: If successful, the previous level's connectedness information
    //      is updated, so we rely on the level not being vetoed after
    //      this check.

    if (!player_in_connected_branch() || brdepth[you.where_are_you] == -1)
        return true;
    if (branches[you.where_are_you].branch_flags & BFLAG_ISLANDED)
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
                       dgn_square_travel_ok, NULL);
    }

    int max_region = 0;
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (feature_mimic_at(*ri))
            continue;

        dungeon_feature_type feat = grd(*ri);
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

    const int up_region_max = you.depth == 1 ? 1 : 3;

    // Ensure all up stairs were found.
    for (int i = 0; i < up_region_max; i++)
        if (up_region[i] == -1)
            return false;

    region_connected.resize(max_region + 1);
    for (unsigned int i = 0; i < region_connected.size(); i++)
        region_connected[i] = false;

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
    for (unsigned i = 0, size = env.level_vaults.size(); i < size; ++i)
    {
        map_def &map = env.level_vaults[i]->map;
        map.run_lua_epilogue();
    }
}

//////////////////////////////////////////////////////////////////////////
// vault_placement

vault_placement::vault_placement()
    : pos(-1, -1), size(0, 0), orient(MAP_NONE), map(), exits(), seen(false)
{
}

string vault_placement::map_name_at(const coord_def &where) const
{
    const coord_def offset = where - this->pos;
    return this->map.name_at(offset);
}

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

            const dungeon_feature_type oldgrid = grd(*ri);

            if (clear)
            {
                env.grid_colours(*ri) = 0;
                env.pgrid(*ri) = 0;
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
                const dungeon_feature_type newgrid = grd(*ri);
                grd(*ri) = oldgrid;
                dungeon_terrain_changed(*ri, newgrid, true, true);
                remove_markers_and_listeners_at(*ri);
            }
        }

        // Place monsters in a second pass.  Otherwise band followers
        // could be overwritten with subsequent walls.
        for (rectangle_iterator ri(pos, pos + size - 1); ri; ++ri)
        {
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

    for (vector<coord_def>::const_iterator i = exits.begin();
         i != exits.end(); ++i)
    {
        if (spotty && _connect_spotty(*i, _feat_is_wall_floor_liquid)
            || player_in_branch(BRANCH_SHOALS)
               && dgn_shoals_connect_point(*i, _feat_is_wall_floor_liquid)
            || _connect_vault_exit(*i))
        {
            exits_placed++;
        }
        else
            dprf("Warning: failed to connect vault exit (%d;%d).", i->x, i->y);
    }

    return exits_placed;
}

// Checks the resultant feature type of the map glyph, after applying KFEAT
// and so forth. Unfortunately there is a certain amount of duplication of
// the code path in apply_grid; but actual modifications to the level
// are so intertwined with that code path it would be actually quite messy
// to try and avoid the duplication.
dungeon_feature_type vault_placement::feature_at(const coord_def &c)
{
    // Can't check outside bounds of vault
    if (size.zero() || c.x > size.x || c.y > size.y)
        return NUM_FEATURES;

    const int feat = map.map.glyph(c);

    if (feat == ' ')
        return NUM_FEATURES;

    keyed_mapspec *mapsp = map.mapspec_at(c);
    return _vault_inspect(*this, feat, mapsp);
}

bool vault_placement::is_space(const coord_def &c)
{
    // Can't check outside bounds of vault
    if (size.zero() || c.x > size.x || c.y > size.y)
        return NUM_FEATURES;

    const int feat = map.map.glyph(c);
    return feat == ' ';
}
bool vault_placement::is_exit(const coord_def &c)
{
    // Can't check outside bounds of vault
    if (size.zero() || c.x > size.x || c.y > size.y)
        return NUM_FEATURES;

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
        return _vault_inspect_glyph(place, vgrid);
}

static dungeon_feature_type _vault_inspect_mapspec(vault_placement &place,
                                                   keyed_mapspec& mapsp)
{
    dungeon_feature_type found = NUM_FEATURES;
    const feature_spec f = mapsp.get_feat();
    if (f.trap.get())
    {
        trap_spec* spec = f.trap.get();
        if (spec)
            found = trap_category(spec->tr_type);
    }
    else if (f.feat >= 0)
        found = static_cast<dungeon_feature_type>(f.feat);
    else if (f.glyph >= 0)
        found = _vault_inspect_glyph(place, f.glyph);
    else if (f.shop.get())
        found = DNGN_ENTER_SHOP;
    else
        found = DNGN_FLOOR;

    return found;
}

static dungeon_feature_type _vault_inspect_glyph(vault_placement &place,
                                                 int vgrid)
{
    // Get the base feature according to the glyph
    dungeon_feature_type found = NUM_FEATURES;
    if (vgrid != -1)
        found = _glyph_to_feat(vgrid, &place);

    // If it's an altar for an unavailable god then it will get turned into floor by _vault_grid_glyph
    if (feat_is_altar(found)
        && is_unavailable_god(feat_altar_god(found)))
    {
        found = DNGN_FLOOR;
    }

    return found;
}

static void _remember_vault_placement(const vault_placement &place, bool extra)
{
    // Second we setup some info to be saved in the player's properties
    // hash table, so the information can be included in the character
    // dump when the player dies/quits/wins.
    if (!place.map.is_overwritable_layout()
        && !place.map.has_tag_suffix("dummy")
        && !place.map.has_tag("no_dump"))
    {
        // When generating a level, vaults may be vetoed together with the
        // whole level after being placed, thus we need to save them in a
        // temp list.
        if (crawl_state.generating_level)
            _you_vault_list.push_back(place.map.name);
        else
            you.vault_list[level_id::current()].push_back(place.map.name);
    }
}

string dump_vault_maps()
{
    string out = "";

    vector<level_id> levels = all_dungeon_ids();

    for (unsigned int i = 0; i < levels.size(); i++)
    {
        level_id    &lid = levels[i];

        if (!you.vault_list.count(lid))
            continue;

        out += lid.describe() + ": " + string(max(8 - int(lid.describe().length()), 0), ' ');

        vector<string> &maps(you.vault_list[lid]);

        string vaults = comma_separated_line(maps.begin(), maps.end(), ", ");
        out += wordwrap_line(vaults, 70) + "\n";
        while (!vaults.empty())
            out += "          " + wordwrap_line(vaults, 70, false) + "\n";
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
                if (grd(*ai) >= DNGN_MINITEM)
                {
                    open++;
                    goto out;
                }
        env.pgrid(*ri) |= FPROP_SEEN_OR_NOEXP;
    out:;
    }

    dprf("Level density: %d", open);
    env.density = open;
}

// Mark all solid squares as no_rtele so that digging doesn't influence
// random teleportation.
static void _mark_solid_squares()
{
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (grd(*ri) <= DNGN_MAXSOLID)
            env.pgrid(*ri) |= FPROP_NO_RTELE_INTO;
    }
}
