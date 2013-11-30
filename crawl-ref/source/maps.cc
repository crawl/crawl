/**
 * @file
 * @brief Functions used to create vaults.
**/

#include "AppHdr.h"

#include "maps.h"

#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <sys/types.h>
#include <sys/param.h>

#ifndef TARGET_COMPILER_VC
#include <unistd.h>
#endif

#include "branch.h"
#include "coordit.h"
#include "dbg-maps.h"
#include "dungeon.h"
#include "endianness.h"
#include "env.h"
#include "enum.h"
#include "files.h"
#include "libutil.h"
#include "message.h"
#include "mapdef.h"
#include "mapmark.h"
#include "mon-util.h"
#include "mon-place.h"
#include "coord.h"
#include "random.h"
#include "state.h"
#include "stuff.h"
#include "syscalls.h"
#include "tags.h"
#include "terrain.h"

#ifndef BYTE_ORDER
# error BYTE_ORDER is not defined
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
# define WORD_LEN (int8_t)sizeof(long)
#else
# define WORD_LEN -(int8_t)sizeof(long)
#endif

static map_section_type _write_vault(map_def &mdef,
                                     vault_placement &,
                                     bool check_place);
static map_section_type _apply_vault_definition(
    map_def &def,
    vault_placement &,
    bool check_place);

static bool _resolve_map(map_def &def);

static bool _map_safe_vault_place(const map_def &map,
                                  const coord_def &c,
                                  const coord_def &size);

// Globals: Use unwind_var to modify!

// Checks whether a map place is valid.
map_place_check_t map_place_valid = _map_safe_vault_place;

// If non-empty, any floating vault's @ exit must land on these point.
point_vector map_anchor_points;

//////////////////////////////////////////////////////////////////////////
// New style vault definitions

static map_vector vdefs;

// Parameter array that vault code can use.
string_vector map_parameters;

dgn_map_parameters::dgn_map_parameters(const string &astring)
    : mpar(map_parameters)
{
    map_parameters.push_back(astring);
}

dgn_map_parameters::dgn_map_parameters(const string_vector &parameters)
    : mpar(map_parameters)
{
    map_parameters = parameters;
}

/* ******************** BEGIN PUBLIC FUNCTIONS ******************* */

// Remember (!!!) - if a member of the monster array isn't specified
// within a vault subroutine, assume that it holds a random monster
// -- only in cases of explicit monsters assignment have I listed
// out random monster insertion {dlb}

// Make sure that vault_n, where n is a number, is a vault which can be put
// anywhere, while other vault names are for specific level ranges, etc.
map_section_type vault_main(vault_placement &place, const map_def *vault,
                            bool check_place)
{
#ifdef DEBUG_DIAGNOSTICS
    if (crawl_state.map_stat_gen)
        mapgen_report_map_try(*vault);
#endif

    // Return value of MAP_NONE forces dungeon.cc to regenerate the
    // level, except for branch entry vaults where dungeon.cc just
    // rejects the vault and places a vanilla entry.

    return _write_vault(const_cast<map_def&>(*vault), place, check_place);
}

static map_section_type _write_vault(map_def &mdef,
                                     vault_placement &place,
                                     bool check_place)
{
    mdef.load();

    // Copy the map so we can monkey with it.
    place.map = mdef;
    place.map.original = &mdef;

    // Try so many times to place the map. This will always succeed
    // unless there are conflicting map placements in 'avoid', or there
    // is a map validate Lua hook that keeps rejecting the map.
    int tries = 25;

    while (tries-- > 0)
    {
        // We're a regular vault, so clear the subvault stack.
        clear_subvault_stack();

        if (place.map.test_lua_veto())
            break;

        if (!_resolve_map(place.map))
            continue;

        // Must set size here, or minivaults will not be placed correctly.
        place.size = place.map.size();
        place.orient = _apply_vault_definition(place.map,
                                               place, check_place);

        if (place.orient != MAP_NONE)
            return place.orient;
    }
    return MAP_NONE;
}

static void _dgn_flush_map_environments()
{
    // Clean up cached environments.
    dlua.callfn("dgn_flush_map_environments", 0, 0);
}

static void _dgn_flush_map_environment_for(const string &mapname)
{
    dlua.callfn("dgn_flush_map_environment_for", "s", mapname.c_str());
}

// Execute the map's Lua, perform substitutions and other transformations,
// and validate the map
static bool _resolve_map_lua(map_def &map)
{
    _dgn_flush_map_environment_for(map.name);
    map.reinit();

    string err = map.run_lua(true);
    if (!err.empty())
    {
#ifdef DEBUG_DIAGNOSTICS
        if (crawl_state.map_stat_gen)
            mapgen_report_error(map, err);
#endif
        mprf(MSGCH_ERROR, "Lua error: %s", err.c_str());
        return false;
    }

    map.fixup();
    err = map.resolve();
    if (!err.empty())
    {
        mprf(MSGCH_ERROR, "Error: %s", err.c_str());
        return false;
    }

    if (!map.test_lua_validate(false))
    {
        dprf("Lua validation for map %s failed.", map.name.c_str());
        return false;
    }

    return true;
}

// Resolve Lua and transformation directives, then mirror and rotate the
// map if allowed
static bool _resolve_map(map_def &map)
{
    if (!_resolve_map_lua(map))
        return false;

    // Don't bother flipping or rotating 1x1 subvaults.
    // This just cuts down on level generation message spam.
    if (map.map.width() == map.map.height() && map.map.width() == 1)
        return true;

    // Mirroring is possible for any map that does not explicitly forbid it.
    // Note that mirroring also flips the orientation.
    if (coinflip())
        map.hmirror();

    if (coinflip())
        map.vmirror();

    // The map may also refuse to be rotated.
    if (coinflip())
        map.rotate(coinflip());

    return true;
}

bool resolve_subvault(map_def &map)
{
    ASSERT(map.is_subvault());
    if (!map.is_subvault())
        return false;

    if (map.test_lua_veto())
        return false;

    if (!_resolve_map_lua(map))
        return false;

    int width = map.subvault_width();
    int height = map.subvault_height();

    bool can_hmirror = !map.has_tag("no_hmirror");
    bool can_vmirror = !map.has_tag("no_vmirror");

    bool can_rot = (map.map.width() <= height && map.map.height() <= width)
                   && !map.has_tag("no_rotate");
    bool must_rot = (map.map.width() > width || map.map.height() > height);

    // Too big, whether or not it is rotated.
    if (must_rot && !can_rot)
        return false;

    if (must_rot || can_rot && coinflip())
    {
        bool dir = coinflip();
        map.rotate(dir);

        // Update post-rotation dimensions.
        width = map.subvault_width();
        height = map.subvault_height();
    }

    // Inexact fits with dimensions flipped will get rotated above.
    bool exact_fit = (map.map.height() == height && map.map.width() == width);
    if (!exact_fit)
    {
        if (can_hmirror && coinflip())
            map.hmirror();

        if (can_vmirror && coinflip())
            map.vmirror();

        // The map may have refused to have been rotated, so verify dimensions.
        bool valid = (map.map.width() <= width && map.map.height() <= height);
        return valid;
    }

    // Don't bother flipping or rotating 1x1 subvaults.
    // This just cuts down on level generation message spam.
    if (exact_fit && width == height && width == 1)
        return true;

    // Count original mismatches.  If mirroring the map causes more cells to
    // not be written, then don't mirror.  This allows oddly shaped subvaults
    // to be fit correctly into a parent vault that specifies the exact same
    // shape.
    const coord_def svplace(0, 0);

    // 0 - normal
    // 1 - flip horz
    // 2 - flip vert + horz
    // 3 - flip vert
    int mismatch[4];

    // Mirror map in all directions to find best fit.
    mismatch[0] = map.subvault_mismatch_count(svplace);
    map.hmirror();
    mismatch[1] = map.subvault_mismatch_count(svplace);
    map.vmirror();
    mismatch[2] = map.subvault_mismatch_count(svplace);
    map.hmirror();
    mismatch[3] = map.subvault_mismatch_count(svplace);

    int min_mismatch = mismatch[0];
    if (can_hmirror)
        min_mismatch = min(min_mismatch, mismatch[1]);
    if (can_hmirror && can_vmirror)
        min_mismatch = min(min_mismatch, mismatch[2]);
    if (can_vmirror)
        min_mismatch = min(min_mismatch, mismatch[3]);

    // Pick a mirror combination with the minimum number of mismatches.
    min_mismatch = min(min_mismatch, mismatch[2]);
    if (!map.has_tag("no_vmirror"))
        min_mismatch = min(min_mismatch, mismatch[3]);

    // Pick a mirror combination with the minimum number of mismatches.
    int idx = random2(4);
    while (mismatch[idx] != min_mismatch)
        idx = (idx + 1) % 4;

    // Flip the map (currently vmirror'd) to the correct orientation.
    if (idx == 0)
        map.vmirror();
    else if (idx == 1)
    {
        map.vmirror();
        map.hmirror();
    }
    else if (idx == 2)
        map.hmirror();

    ASSERT(map.subvault_mismatch_count(svplace) == min_mismatch);

    // We already know this is an exact fit, so this is a success.
    return true;
}

// Given a rectangular region, slides it to fit into the map. size must be
// smaller than (GXM,GYM).
static void _fit_region_into_map_bounds(coord_def &pos, const coord_def &size,
                                        int margin)
{
    const int X_1(X_BOUND_1 + margin);
    const int X_2(X_BOUND_2 - margin);
    const int Y_1(Y_BOUND_1 + margin);
    const int Y_2(Y_BOUND_2 - margin);

    ASSERT(size.x <= (X_2 - X_1 + 1));
    ASSERT(size.y <= (Y_2 - Y_1 + 1));

    if (pos.x < X_1)
        pos.x = X_1;
    if (pos.y < Y_1)
        pos.y = Y_1;
    if (pos.x + size.x - 1 > X_2)
        pos.x = X_2 - size.x + 1;
    if (pos.y + size.y - 1 > Y_2)
        pos.y = Y_2 - size.y + 1;
}

// Used for placement of vaults.
static bool _may_overwrite_feature(const coord_def p,
                                   bool water_ok, bool wall_ok = true)
{
    // If there's a mask specifying where vaults can be placed, don't
    // allow stepping outside it.
    if (Vault_Placement_Mask && !(*Vault_Placement_Mask)(p))
        return false;

    // If in the abyss, the placement mask is the only check necessary
    // for terrain.
    if (Vault_Placement_Mask && player_in_branch(BRANCH_ABYSS))
        return true;

    const dungeon_feature_type grid = grd(p);

    // Deep water grids may be overwritten if water_ok == true.
    if (grid == DNGN_DEEP_WATER)
        return water_ok;

    if (grid == DNGN_TREE && player_in_branch(BRANCH_FOREST))
        return true;

    // Handle all other non-LOS blocking grids here.
    if (!feat_is_opaque(grid)
        && grid != DNGN_FLOOR
        && grid != DNGN_SHALLOW_WATER
        && grid != DNGN_OPEN_DOOR
        && !feat_is_closed_door(grid))
    {
        return false;
    }

    if (feat_is_wall(grid) || feat_is_tree(grid))
        return wall_ok;

    // Otherwise, feel free to clobber this feature.
    return true;
}

static bool _is_portal_place(const coord_def &c)
{
    map_marker* marker = env.markers.find(c, MAT_LUA_MARKER);
    if (!marker)
        return false;

    return marker->property("portal") != "";
}

static bool _map_safe_vault_place(const map_def &map,
                                  const coord_def &c,
                                  const coord_def &size)
{
    if (size.zero())
        return true;

    // Processing for layouts is handled elsewhere.
    if (map.is_overwritable_layout())
        return true;

    const bool water_ok =
        map.has_tag("water_ok") || player_in_branch(BRANCH_SWAMP);

    const bool vault_can_overwrite_other_vaults =
        map.has_tag("can_overwrite");

    const bool vault_can_replace_portals =
        map.has_tag("replace_portal");

    const vector<string> &lines = map.map.get_lines();
    for (rectangle_iterator ri(c, c + size - 1); ri; ++ri)
    {
        const coord_def cp(*ri);
        const coord_def dp(cp - c);

        if (lines[dp.y][dp.x] == ' ')
            continue;

        if (!vault_can_overwrite_other_vaults)
        {
            // Also check adjacent squares for collisions, because being next
            // to another vault may block off one of this vault's exits.
            for (adjacent_iterator ai(cp); ai; ++ai)
            {
                if (map_bounds(*ai) && (env.level_map_mask(*ai) & MMT_VAULT))
                    return false;
            }
        }

        // Don't overwrite features other than floor, rock wall, doors,
        // nor water, if !water_ok.
        if (!_may_overwrite_feature(cp, water_ok)
            && (!vault_can_replace_portals
                || !_is_portal_place(cp)))
        {
            return false;
        }

        // Don't overwrite monsters or items, either!
        if (monster_at(cp) || igrd(cp) != NON_ITEM)
            return false;

        // If in Slime, don't let stairs end up next to minivaults,
        // so that they don't possibly end up next to unsafe walls.
        if (player_in_branch(BRANCH_SLIME))
        {
            for (adjacent_iterator ai(cp); ai; ++ai)
            {
                if (map_bounds(*ai) && feat_is_stair(grd(*ai)))
                    return false;
            }
        }
    }

    return true;
}

static bool _connected_minivault_place(const coord_def &c,
                                       const vault_placement &place)
{
    if (place.size.zero())
        return true;

    // Must not be completely isolated.
    const vector<string> &lines = place.map.map.get_lines();

    for (rectangle_iterator ri(c, c + place.size - 1); ri; ++ri)
    {
        const coord_def &ci(*ri);

        if (lines[ci.y - c.y][ci.x - c.x] == ' ')
            continue;

        if (_may_overwrite_feature(ci, false, false)
            || (place.map.has_tag("replace_portal")
                && _is_portal_place(ci)))
            return true;
    }

    return false;
}

static coord_def _find_minivault_place(
    const vault_placement &place,
    bool check_place)
{
    if (place.map.has_tag("replace_portal"))
    {
        vector<map_marker*> markers = env.markers.get_all(MAT_LUA_MARKER);
        vector<coord_def> candidates;
        for (vector<map_marker*>::iterator it = markers.begin();
             it != markers.end(); it++)
        {
            if ((*it)->property("portal") != "")
            {
                coord_def v1((*it)->pos);
                if ((!check_place || map_place_valid(place.map, v1, place.size))
                    && _connected_minivault_place(v1, place)
                    && !feat_is_gate(grd(v1)))
                    candidates.push_back(v1);
            }
        }
        if (!candidates.empty())
            return candidates[random2(candidates.size())];
    }

    // [ds] The margin around the edges of the map where the minivault
    // won't be placed. Purely arbitrary as far as I can see.
    // The spotty connector in the Shoals needs one more space to work.
    const int margin = MAPGEN_BORDER * 2 + player_in_branch(BRANCH_SHOALS);

    // Find a target area which can be safely overwritten.
    for (int tries = 0; tries < 600; ++tries)
    {
        coord_def v1(random_range(margin, GXM - margin - place.size.x),
                     random_range(margin, GYM - margin - place.size.y));

        if (check_place && !map_place_valid(place.map, v1, place.size))
        {
#ifdef DEBUG_MINIVAULT_PLACEMENT
            mprf(MSGCH_DIAGNOSTICS,
                 "Skipping (%d,%d): not safe to place map",
                 v1.x, v1.y);
#endif
            continue;
        }

        if (!_connected_minivault_place(v1, place))
        {
#ifdef DEBUG_MINIVAULT_PLACEMENT
            mprf(MSGCH_DIAGNOSTICS,
                 "Skipping (%d,%d): not a good minivault place (tags: %s)",
                 v1.x, v1.y, place.map.tags.c_str());
#endif
            continue;
        }
        return v1;
    }
    return coord_def(-1, -1);
}

static bool _apply_vault_grid(map_def &def,
                              vault_placement &place,
                              bool check_place)
{
    const map_lines &ml = def.map;
    const int orient = def.orient;

    const coord_def size(ml.size());

    coord_def start(0, 0);

    if (orient == MAP_SOUTH || orient == MAP_SOUTHEAST
        || orient == MAP_SOUTHWEST)
    {
        start.y = GYM - size.y;
    }
    if (orient == MAP_EAST || orient == MAP_NORTHEAST
        || orient == MAP_SOUTHEAST)
    {
        start.x = GXM - size.x;
    }

    // Handle maps aligned along cardinals that are smaller than
    // the corresponding map dimension.
    if ((orient == MAP_NORTH || orient == MAP_SOUTH
         || orient == MAP_ENCOMPASS || orient == MAP_CENTRE)
        && size.x < GXM)
    {
        start.x = (GXM - size.x) / 2;
    }
    if ((orient == MAP_EAST || orient == MAP_WEST
         || orient == MAP_ENCOMPASS || orient == MAP_CENTRE)
        && size.y < GYM)
    {
        start.y = (GYM - size.y) / 2;
    }

    // Floating maps can go anywhere, ask the map_def to suggest a place.
    if (orient == MAP_FLOAT)
    {
        const bool minivault = def.is_minivault();
        if (map_bounds(place.pos))
        {
            start = place.pos - size / 2;
            _fit_region_into_map_bounds(start, size, minivault ? MAPGEN_BORDER : 0);
        }
        else if (minivault)
        {
            start = _find_minivault_place(place, check_place);
            if (map_bounds(start))
                _fit_region_into_map_bounds(start, size, MAPGEN_BORDER);
        }
        else
            start = def.float_place();
    }

    if (!map_bounds(start))
        return false;

    if (check_place && !map_place_valid(def, start, size))
    {
        dprf("Bad vault place: (%d,%d) dim (%d,%d)",
             start.x, start.y, size.x, size.y);
        return false;
    }


    place.pos  = start;
    place.size = size;

    return true;
}

static map_section_type _apply_vault_definition(
    map_def &def,
    vault_placement &place,
    bool check_place)
{
    if (!_apply_vault_grid(def, place, check_place))
        return MAP_NONE;

    const map_section_type orient = def.orient;
    return orient == MAP_NONE ? MAP_NORTH : orient;
}

///////////////////////////////////////////////////////////////////////////
// Map lookups

static bool _map_matches_layout_type(const map_def &map)
{
    bool permissive = false;
    if (env.level_layout_types.empty()
        || (!map.has_tag_prefix("layout_")
            && !(permissive = map.has_tag_prefix("nolayout_"))))
    {
        return true;
    }

    for (string_set::const_iterator i = env.level_layout_types.begin();
         i != env.level_layout_types.end(); ++i)
    {
        if (map.has_tag("layout_" + *i))
            return true;
        else if (map.has_tag("nolayout_" + *i))
            return false;
    }

    return permissive;
}

static bool _map_matches_species(const map_def &map)
{
    if (you.species < 0 || you.species >= NUM_SPECIES)
        return true;
    return !map.has_tag("no_species_"
           + lowercase_string(get_species_abbrev(you.species)));
}

const map_def *find_map_by_name(const string &name)
{
    for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
        if (vdefs[i].name == name)
            return &vdefs[i];

    return NULL;
}

// Discards Lua code loaded by all maps to reduce memory use. If any stripped
// map is reused, its data will be reloaded from the .dsc
void strip_all_maps()
{
    for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
        vdefs[i].strip();
}

vector<string> find_map_matches(const string &name)
{
    vector<string> matches;

    for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
        if (vdefs[i].name.find(name) != string::npos)
            matches.push_back(vdefs[i].name);
    return matches;
}

mapref_vector find_maps_for_tag(const string tag,
                                bool check_depth,
                                bool check_used)
{
    mapref_vector maps;
    level_id place = level_id::current();

    for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
    {
        const map_def &mapdef = vdefs[i];
        if (mapdef.has_tag(tag)
            && !mapdef.has_tag("dummy")
            && (!check_depth || !mapdef.has_depth()
                || mapdef.is_usable_in(place))
            && (!check_used || !mapdef.map_already_used()))
        {
            maps.push_back(&mapdef);
        }
    }
    return maps;
}

struct map_selector
{
private:
    enum select_type
    {
        PLACE,
        DEPTH,
        DEPTH_AND_CHANCE,
        TAG,
    };

public:
    bool accept(const map_def &md) const;
    void announce(const map_def *map) const;

    bool valid() const
    {
        return sel == TAG || place.is_valid();
    }

    static map_selector by_place(const level_id &_place, bool _mini,
                                 maybe_bool _extra)
    {
        return map_selector(map_selector::PLACE, _place, "", _mini, _extra,
                            false);
    }

    static map_selector by_depth(const level_id &_place, bool _mini,
                                 maybe_bool _extra)
    {
        return map_selector(map_selector::DEPTH, _place, "", _mini, _extra,
                            true);
    }

    static map_selector by_depth_chance(const level_id &_place,
                                        maybe_bool _extra)
    {
        return map_selector(map_selector::DEPTH_AND_CHANCE, _place, "", false,
                            _extra, true);
    }

    static map_selector by_tag(const string &_tag,
                               bool _check_depth,
                               bool _check_chance,
                               maybe_bool _extra,
                               const level_id &_place = level_id::current())
    {
        map_selector msel = map_selector(map_selector::TAG, _place, _tag,
                                         false, _extra, _check_depth);
        msel.ignore_chance = !_check_chance;
        return msel;
    }

private:
    map_selector(select_type _typ, const level_id &_pl,
                 const string &_tag,
                 bool _mini, maybe_bool _extra, bool _check_depth)
        : ignore_chance(false), preserve_dummy(false),
          sel(_typ), place(_pl), tag(_tag),
          mini(_mini), extra(_extra), check_depth(_check_depth),
          check_layout((sel == DEPTH || sel == DEPTH_AND_CHANCE)
                    && place == level_id::current())
    {
        if (_typ == PLACE)
            ignore_chance = true;
    }

    bool depth_selectable(const map_def &) const;

public:
    bool ignore_chance;
    bool preserve_dummy;
    const select_type sel;
    const level_id place;
    const string tag;
    const bool mini;
    const maybe_bool extra;
    const bool check_depth;
    const bool check_layout;
};

bool map_selector::depth_selectable(const map_def &mapdef) const
{
    return mapdef.is_usable_in(place)
           // Some tagged levels cannot be selected as random
           // maps in a specific depth:
           && !mapdef.has_tag_suffix("entry")
           && !mapdef.has_tag("unrand")
           && !mapdef.has_tag("place_unique")
           && !mapdef.has_tag("tutorial")
           && (!mapdef.has_tag_prefix("temple_")
               || mapdef.has_tag_prefix("uniq_altar_"))
           && _map_matches_species(mapdef)
           && (!check_layout || _map_matches_layout_type(mapdef));
}

static bool _is_extra_compatible(maybe_bool want_extra, bool have_extra)
{
    return want_extra == MB_MAYBE
           || (want_extra == MB_TRUE && have_extra)
           || (want_extra == MB_FALSE && !have_extra);
}

bool map_selector::accept(const map_def &mapdef) const
{
    switch (sel)
    {
    case PLACE:
        if (mapdef.has_tag_prefix("tutorial")
            && (!crawl_state.game_is_tutorial()
                || !mapdef.has_tag(crawl_state.map)))
        {
            return false;
        }
        return mapdef.is_minivault() == mini
               && _is_extra_compatible(extra, mapdef.has_tag("extra"))
               && mapdef.place.is_usable_in(place)
               && _map_matches_layout_type(mapdef)
               && !mapdef.map_already_used();

    case DEPTH:
    {
        const map_chance chance(mapdef.chance(place));
        return mapdef.is_minivault() == mini
               && _is_extra_compatible(extra, mapdef.has_tag("extra"))
               && (!chance.valid() || chance.dummy_chance())
               && depth_selectable(mapdef)
               && !mapdef.map_already_used();
    }

    case DEPTH_AND_CHANCE:
    {
        const map_chance chance(mapdef.chance(place));
        // Only vaults with valid chance
        return chance.valid()
               && !chance.dummy_chance()
               && depth_selectable(mapdef)
               && _is_extra_compatible(extra, mapdef.has_tag("extra"))
               && !mapdef.map_already_used();
    }

    case TAG:
        return mapdef.has_tag(tag)
               && (!check_depth
                   || !mapdef.has_depth()
                   || mapdef.is_usable_in(place))
               && _map_matches_species(mapdef)
               && _map_matches_layout_type(mapdef)
               && !mapdef.map_already_used();

    default:
        return false;
    }
}

void map_selector::announce(const map_def *vault) const
{
#ifdef DEBUG_DIAGNOSTICS
    if (vault)
    {
        if (sel == DEPTH_AND_CHANCE)
        {
            mprf(MSGCH_DIAGNOSTICS,
                 "[CHANCE+DEPTH] Found map %s for %s (%s)",
                 vault->name.c_str(), place.describe().c_str(),
                 vault->chance(place).describe().c_str());
        }
        else
        {
            const char *format =
                sel == PLACE ? "[PLACE] Found map %s for %s" :
                sel == DEPTH ? "[DEPTH] Found random map %s for %s" :
                "[TAG] Found map %s tagged '%s'";

            mprf(MSGCH_DIAGNOSTICS, format,
                 vault->name.c_str(),
                 sel == TAG ? tag.c_str() : place.describe().c_str());
        }
    }
#endif
}

string vault_chance_tag(const map_def &map)
{
    if (map.has_tag_prefix("chance_"))
    {
        const vector<string> tags = map.get_tags();
        for (int i = 0, size = tags.size(); i < size; ++i)
        {
            if (tags[i].find("chance_") == 0)
                return tags[i];
        }
    }
    return "";
}

typedef vector<unsigned> vault_indices;

static vault_indices _eligible_maps_for_selector(const map_selector &sel)
{
    vault_indices eligible;

    if (sel.valid())
    {
        for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
            if (sel.accept(vdefs[i]))
                eligible.push_back(i);
    }

    return eligible;
}

static const map_def *_random_map_by_selector(const map_selector &sel);

static bool _vault_chance_new(const map_def &map,
                              const level_id &place,
                              set<string> &chance_tags)
{
    if (map.chance(place).valid())
    {
        // There may be several alternatives for a portal
        // vault that want to be governed by one common
        // CHANCE. In this case each vault will use a
        // CHANCE, and a common chance_xxx tag. Pick the
        // first such vault for the chance roll. Note that
        // at this point we ignore chance_priority.
        const string tag = vault_chance_tag(map);
        if (!chance_tags.count(tag))
        {
            if (!tag.empty())
                chance_tags.insert(tag);
            return true;
        }
    }
    return false;
}

class vault_chance_roll_iterator
{
public:
    vault_chance_roll_iterator(const mapref_vector &_maps)
        : maps(_maps), place(level_id::current()),
          current(_maps.begin()), end(_maps.end())
    {
        find_valid();
    }

    operator bool () const { return current != end; }
    const map_def *operator * () const { return *current; }
    const map_def *operator -> () const { return *current; }

    vault_chance_roll_iterator &operator ++ ()
    {
        ++current;
        find_valid();
        return *this;
    }

    vault_chance_roll_iterator operator ++ (int)
    {
        vault_chance_roll_iterator copy(*this);
        operator ++ ();
        return copy;
    }

private:
    void find_valid()
    {
        while (current != end && !(*current)->chance(place).roll())
            ++current;
    }

private:
    const vector<const map_def *> &maps;
    level_id place;
    mapref_vector::const_iterator current;
    mapref_vector::const_iterator end;
};

static const map_def *_resolve_chance_vault(const map_selector &sel,
                                            const map_def *map)
{
    const string chance_tag = vault_chance_tag(*map);
    // If this map has a chance_ tag, convert the search into
    // a lookup for that tag.
    if (!chance_tag.empty())
    {
        map_selector msel = map_selector::by_tag(chance_tag,
                                                 sel.check_depth,
                                                 false,
                                                 sel.extra,
                                                 sel.place);
        return _random_map_by_selector(msel);
    }
    return map;
}

static mapref_vector
_random_chance_maps_in_list(const map_selector &sel,
                            const vault_indices &filtered)
{
    // Vaults that are eligible and have >0 chance.
    mapref_vector chance;
    mapref_vector chosen_chances;

    typedef set<string> tag_set;
    tag_set chance_tags;

    for (unsigned f = 0, size = filtered.size(); f < size; ++f)
    {
        const int i = filtered[f];
        if (!sel.ignore_chance
            && _vault_chance_new(vdefs[i], sel.place, chance_tags))
        {
            chance.push_back(&vdefs[i]);
        }
    }

    for (vault_chance_roll_iterator vc(chance); vc; ++vc)
        if (const map_def *chosen = _resolve_chance_vault(sel, *vc))
        {
            chosen_chances.push_back(chosen);
            sel.announce(chosen);
        }

    return chosen_chances;
}

static const map_def *
_random_map_in_list(const map_selector &sel,
                    const vault_indices &filtered)
{
    const map_def *chosen_map = NULL;
    int rollsize = 0;

    // First build a list of vaults that could be used:
    mapref_vector eligible;

    // Vaults that are eligible and have >0 chance.
    mapref_vector chance;

    typedef set<string> tag_set;
    tag_set chance_tags;

    for (unsigned f = 0, size = filtered.size(); f < size; ++f)
    {
        const int i = filtered[f];
        if (!sel.ignore_chance && vdefs[i].chance(sel.place).valid())
        {
            if (_vault_chance_new(vdefs[i], sel.place, chance_tags))
                chance.push_back(&vdefs[i]);
        }
        else
            eligible.push_back(&vdefs[i]);
    }

    for (vault_chance_roll_iterator vc(chance); vc; ++vc)
        if (const map_def *chosen = _resolve_chance_vault(sel, *vc))
        {
            chosen_map = chosen;
            break;
        }

    if (!chosen_map)
    {
        const level_id &here(level_id::current());
        for (mapref_vector::const_iterator i = eligible.begin();
             i != eligible.end(); ++i)
        {
            const map_def &map(**i);
            const int weight = map.weight(here);

            if (weight <= 0)
                continue;

            rollsize += weight;

            if (rollsize && x_chance_in_y(weight, rollsize))
                chosen_map = &map;
        }
    }

    if (!sel.preserve_dummy && chosen_map
        && chosen_map->has_tag("dummy"))
    {
        chosen_map = NULL;
    }

    sel.announce(chosen_map);
    return chosen_map;
}

static const map_def *_random_map_by_selector(const map_selector &sel)
{
    const vault_indices filtered = _eligible_maps_for_selector(sel);
    return _random_map_in_list(sel, filtered);
}

// Returns a map for which PLACE: matches the given place.
const map_def *random_map_for_place(const level_id &place, bool minivault,
                                    maybe_bool extra)
{
    return _random_map_by_selector(
        map_selector::by_place(place, minivault, extra));
}

const map_def *random_map_in_depth(const level_id &place, bool want_minivault,
                                   maybe_bool extra)
{
    return _random_map_by_selector(
        map_selector::by_depth(place, want_minivault, extra));
}

mapref_vector random_chance_maps_in_depth(const level_id &place,
                                          maybe_bool extra)
{
    map_selector sel = map_selector::by_depth_chance(place, extra);
    const vault_indices eligible = _eligible_maps_for_selector(sel);
    return _random_chance_maps_in_list(sel, eligible);
}

const map_def *random_map_for_tag(const string &tag,
                                  bool check_depth,
                                  bool check_chance,
                                  maybe_bool extra)
{
    return _random_map_by_selector(
        map_selector::by_tag(tag, check_depth, check_chance, extra));
}

int map_count()
{
    return vdefs.size();
}

/////////////////////////////////////////////////////////////////////////////
// Reading maps from .des files.

// All global preludes.
static vector<dlua_chunk> global_preludes;

// Map-specific prelude.
dlua_chunk lc_global_prelude("global_prelude");
string lc_desfile;
map_def     lc_map;
level_range lc_range;
depth_ranges lc_default_depths;
bool lc_run_global_prelude = true;
map_load_info_t lc_loaded_maps;

static set<string> map_files_read;

extern int yylineno;

static void _reset_map_parser()
{
    lc_map.init();
    lc_range.reset();
    lc_default_depths.clear();
    lc_global_prelude.clear();

    yylineno = 1;
    lc_run_global_prelude = true;
}

////////////////////////////////////////////////////////////////////////////

static bool checked_des_index_dir = false;

static string _des_cache_dir(const string &relpath = "")
{
    return catpath(savedir_versioned_path("des"), relpath);
}

static void _check_des_index_dir()
{
    if (checked_des_index_dir)
        return;

    string desdir = _des_cache_dir();
    if (!check_mkdir("Data file cache", &desdir, true))
        end(1, true, "Can't create data file cache: %s", desdir.c_str());

    checked_des_index_dir = true;
}

string get_descache_path(const string &file, const string &ext)
{
    const string basename = change_file_extension(get_base_filename(file), ext);
    return _des_cache_dir(basename);
}

static bool verify_file_version(const string &file, time_t mtime)
{
    FILE *fp = fopen_u(file.c_str(), "rb");
    if (!fp)
        return false;
    try
    {
        reader inf(fp);
        const uint8_t major = unmarshallUByte(inf);
        const uint8_t minor = unmarshallUByte(inf);
        const int8_t word = unmarshallByte(inf);
        const int64_t t = unmarshallSigned(inf);
        fclose(fp);
        return major == TAG_MAJOR_VERSION
               && minor <= TAG_MINOR_VERSION
               && word == WORD_LEN
               && t == mtime;
    }
    catch (short_read_exception &E)
    {
        fclose(fp);
        return false;
    }
}

static bool _verify_map_index(const string &base, time_t mtime)
{
    return verify_file_version(base + ".idx", mtime);
}

static bool _verify_map_full(const string &base, time_t mtime)
{
    return verify_file_version(base + ".dsc", mtime);
}

static bool _load_map_index(const string& cache, const string &base,
                            time_t mtime)
{
    // If there's a global prelude, load that first.
    if (FILE *fp = fopen_u((base + ".lux").c_str(), "rb"))
    {
        reader inf(fp, TAG_MINOR_VERSION);
        uint8_t major = unmarshallUByte(inf);
        uint8_t minor = unmarshallUByte(inf);
        int8_t word = unmarshallByte(inf);
        int64_t t = unmarshallSigned(inf);
        if (major != TAG_MAJOR_VERSION || minor > TAG_MINOR_VERSION
            || word != WORD_LEN || t != mtime)
        {
            return false;
        }

        lc_global_prelude.read(inf);
        fclose(fp);

        global_preludes.push_back(lc_global_prelude);
    }

    FILE* fp = fopen_u((base + ".idx").c_str(), "rb");
    if (!fp)
        end(1, true, "Unable to read %s", (base + ".idx").c_str());

    reader inf(fp, TAG_MINOR_VERSION);
    // Re-check version, might have been modified in the meantime.
    uint8_t major = unmarshallUByte(inf);
    uint8_t minor = unmarshallUByte(inf);
    int8_t word = unmarshallByte(inf);
    int64_t t = unmarshallSigned(inf);
    if (major != TAG_MAJOR_VERSION || minor > TAG_MINOR_VERSION
        || word != WORD_LEN || t != mtime)
    {
        return false;
    }

#if TAG_MAJOR_VERSION == 34
    // Throw out pre-ORDER: indices entirely.
    if (minor < TAG_MINOR_MAP_ORDER)
        return false;
#endif

    const int nmaps = unmarshallShort(inf);
    const int nexist = vdefs.size();
    vdefs.resize(nexist + nmaps, map_def());
    for (int i = 0; i < nmaps; ++i)
    {
        map_def &vdef(vdefs[nexist + i]);
        vdef.read_index(inf);
        vdef.description = unmarshallString(inf);
        vdef.order = unmarshallInt(inf);

        vdef.set_file(cache);
        lc_loaded_maps[vdef.name] = vdef.place_loaded_from;
        vdef.place_loaded_from.clear();
    }
    fclose(fp);

    return true;
}

static bool _load_map_cache(const string &filename, const string &cachename)
{
    _check_des_index_dir();
    const string descache_base = get_descache_path(cachename, "");

    file_lock deslock(descache_base + ".lk", "rb", false);

    time_t mtime = file_modtime(filename);
    string file_idx = descache_base + ".idx";
    string file_dsc = descache_base + ".dsc";

    // What's the point in checking these twice (here and in load_ma_index)?
    if (!_verify_map_index(descache_base, mtime)
        || !_verify_map_full(descache_base, mtime))
    {
        return false;
    }

    return _load_map_index(cachename, descache_base, mtime);
}

static void _write_map_prelude(const string &filebase, time_t mtime)
{
    const string luafile = filebase + ".lux";
    if (lc_global_prelude.empty())
    {
        unlink_u(luafile.c_str());
        return;
    }

    FILE *fp = fopen_u(luafile.c_str(), "wb");
    writer outf(luafile, fp);
    marshallUByte(outf, TAG_MAJOR_VERSION);
    marshallUByte(outf, TAG_MINOR_VERSION);
    marshallByte(outf, WORD_LEN);
    marshallSigned(outf, mtime);
    lc_global_prelude.write(outf);
    fclose(fp);
}

static void _write_map_full(const string &filebase, size_t vs, size_t ve,
                            time_t mtime)
{
    const string cfile = filebase + ".dsc";
    FILE *fp = fopen_u(cfile.c_str(), "wb");
    if (!fp)
        end(1, true, "Unable to open %s for writing", cfile.c_str());

    writer outf(cfile, fp);
    marshallUByte(outf, TAG_MAJOR_VERSION);
    marshallUByte(outf, TAG_MINOR_VERSION);
    marshallByte(outf, WORD_LEN);
    marshallSigned(outf, mtime);
    for (size_t i = vs; i < ve; ++i)
        vdefs[i].write_full(outf);
    fclose(fp);
}

static void _write_map_index(const string &filebase, size_t vs, size_t ve,
                             time_t mtime)
{
    const string cfile = filebase + ".idx";
    FILE *fp = fopen_u(cfile.c_str(), "wb");
    if (!fp)
        end(1, true, "Unable to open %s for writing", cfile.c_str());

    writer outf(cfile, fp);
    marshallUByte(outf, TAG_MAJOR_VERSION);
    marshallUByte(outf, TAG_MINOR_VERSION);
    marshallByte(outf, WORD_LEN);
    marshallSigned(outf, mtime);
    marshallShort(outf, ve > vs? ve - vs : 0);
    for (size_t i = vs; i < ve; ++i)
    {
        vdefs[i].write_index(outf);
        marshallString(outf, vdefs[i].description);
        marshallInt(outf, vdefs[i].order);
        vdefs[i].place_loaded_from.clear();
        vdefs[i].strip();
    }
    fclose(fp);
}

static void _write_map_cache(const string &filename, size_t vs, size_t ve,
                             time_t mtime)
{
    _check_des_index_dir();

    const string descache_base = get_descache_path(filename, "");

    file_lock deslock(descache_base + ".lk", "wb");

    _write_map_prelude(descache_base, mtime);
    _write_map_full(descache_base, vs, ve, mtime);
    _write_map_index(descache_base, vs, ve, mtime);
}

static void _parse_maps(const string &s)
{
    string cache_name = get_cache_name(s);
    if (map_files_read.count(cache_name))
        return;

    map_files_read.insert(cache_name);

    if (_load_map_cache(s, cache_name))
        return;

    FILE *dat = fopen_u(s.c_str(), "r");
    if (!dat)
        end(1, true, "Failed to open %s for reading", s.c_str());

#ifdef DEBUG_DIAGNOSTICS
    printf("Regenerating des: %s\n", s.c_str());
#endif

    time_t mtime = file_modtime(dat);
    _reset_map_parser();

    extern int yyparse(void);
    extern FILE *yyin;
    yyin = dat;

    const size_t file_start = vdefs.size();
    yyparse();
    fclose(dat);

    global_preludes.push_back(lc_global_prelude);

    _write_map_cache(cache_name, file_start, vdefs.size(), mtime);
}

void read_map(const string &file)
{
    _parse_maps(lc_desfile = datafile_path(file));
    _dgn_flush_map_environments();
    // Force GC to prevent heap from swelling unnecessarily.
    dlua.gc();
}

void read_maps()
{
    if (dlua.execfile("dlua/loadmaps.lua", true, true, true))
        end(1, false, "Lua error: %s", dlua.error.c_str());

    lc_loaded_maps.clear();

    {
        unwind_var<FixedVector<int, NUM_BRANCHES> > depths(brdepth);
        // let the sanity check place maps
        for (int i = 0; i < NUM_BRANCHES; i++)
            brdepth[i] = branches[i].numlevels;
        dlua.execfile("dlua/sanity.lua", true, true);
    }
}

// If a .dsc file has been changed under the running Crawl, discard
// all map knowledge and reload maps. This will not affect maps that
// have already been used, but it might trigger exciting happenings if
// the new maps fail sanity checks or remove maps that the game
// expects to be present.
void reread_maps()
{
    dprf("reread_maps:: discarding %u existing maps",
         (unsigned int)vdefs.size());

    // BOOM!
    vdefs.clear();
    map_files_read.clear();
    read_maps();
}

void dump_map(const map_def &map)
{
    if (crawl_state.dump_maps)
        fprintf(stderr, "\n----------------------------------------\n%s\n",
                map.describe().c_str());
}

void add_parsed_map(const map_def &md)
{
    map_def map = md;

    map.fixup();
    vdefs.push_back(map);
}

void run_map_global_preludes()
{
    for (int i = 0, size = global_preludes.size(); i < size; ++i)
    {
        dlua_chunk &chunk = global_preludes[i];
        if (!chunk.empty())
        {
            if (chunk.load_call(dlua, NULL))
                mprf(MSGCH_ERROR, "Lua error: %s", chunk.orig_error().c_str());
        }
    }
}

void run_map_local_preludes()
{
    for (int i = 0, size = vdefs.size(); i < size; ++i)
    {
        if (!vdefs[i].prelude.empty())
        {
            string err = vdefs[i].run_lua(true);
            if (!err.empty())
                mprf(MSGCH_ERROR, "Lua error (map %s): %s",
                     vdefs[i].name.c_str(), err.c_str());
        }
    }
}

const map_def *map_by_index(int index)
{
    return &vdefs[index];
}

///////////////////////////////////////////////////////////////////////////
// Debugging code

#ifdef DEBUG_DIAGNOSTICS

typedef pair<string, int> weighted_map_name;
typedef vector<weighted_map_name> weighted_map_names;

static weighted_map_names mg_find_random_vaults(
    const level_id &place, bool wantmini)
{
    weighted_map_names wms;

    if (!place.is_valid())
        return wms;

    typedef map<string, int> map_count_t;

    map_count_t map_counts;

    map_selector sel = map_selector::by_depth(place, wantmini, MB_MAYBE);
    sel.preserve_dummy = true;

    no_messages mx;
    vault_indices filtered = _eligible_maps_for_selector(sel);

    for (int i = 0; i < 10000; ++i)
    {
        const map_def *map(_random_map_in_list(sel, filtered));
        if (!map)
            map_counts["(none)"]++;
        else
            map_counts[map->name]++;
    }

    for (map_count_t::const_iterator i = map_counts.begin();
         i != map_counts.end(); ++i)
    {
        wms.push_back(*i);
    }

    return wms;
}

static bool _weighted_map_more_likely(
    const weighted_map_name &a,
    const weighted_map_name &b)
{
    return a.second > b.second;
}

static void _mg_report_random_vaults(
    FILE *outf, const level_id &place, bool wantmini)
{
    weighted_map_names wms = mg_find_random_vaults(place, wantmini);
    sort(wms.begin(), wms.end(), _weighted_map_more_likely);
    int weightsum = 0;
    for (int i = 0, size = wms.size(); i < size; ++i)
        weightsum += wms[i].second;

    string line;
    for (int i = 0, size = wms.size(); i < size; ++i)
    {
        string curr = make_stringf("%s (%.2f%%)", wms[i].first.c_str(),
                                   100.0 * wms[i].second / weightsum);
        if (i < size - 1)
            curr += ", ";
        if (line.length() + curr.length() > 80u)
        {
            fprintf(outf, "%s\n", line.c_str());
            line.clear();
        }

        line += curr;
    }
    if (!line.empty())
        fprintf(outf, "%s\n", line.c_str());
}

void mg_report_random_maps(FILE *outf, const level_id &place)
{
    fprintf(outf, "---------------- Mini\n");
    _mg_report_random_vaults(outf, place, true);
    fprintf(outf, "------------- Regular\n");
    _mg_report_random_vaults(outf, place, false);
}

#endif
