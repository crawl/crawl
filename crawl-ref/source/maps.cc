/*
 *  File:       maps.cc
 *  Summary:    Functions used to create vaults.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "maps.h"

#include <cstring>
#include <cstdlib>
#include <algorithm>

#ifndef TARGET_COMPILER_VC
#include <unistd.h>
#endif

#include "coordit.h"
#include "dbg-maps.h"
#include "dungeon.h"
#include "env.h"
#include "enum.h"
#include "files.h"
#include "flood_find.h"
#ifdef USE_TILE
#include "initfile.h"
#endif
#include "libutil.h"
#include "message.h"
#include "mapdef.h"
#include "mon-util.h"
#include "mon-place.h"
#include "coord.h"
#include "random.h"
#include "state.h"
#include "tags.h"
#include "terrain.h"

static int write_vault(map_def &mdef,
                       vault_placement &,
                       bool check_place);
static int apply_vault_definition(
                        map_def &def,
                        vault_placement &,
                        bool check_place);

static bool resolve_map(map_def &def);

// Globals: Use unwind_var to modify!

// Checks whether a map place is valid.
map_place_check_t map_place_valid = map_safe_vault_place;

// If non-empty, any floating vault's @ exit must land on these point.
point_vector map_anchor_points;

//////////////////////////////////////////////////////////////////////////
// New style vault definitions

static map_vector vdefs;

// Parameter array that vault code can use.
string_vector map_parameters;

dgn_map_parameters::dgn_map_parameters(const std::string &astring)
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
int vault_main(vault_placement &place, const map_def *vault,
                bool check_place)
{
#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Generating level: %s (%d,%d)",
         vault->name.c_str(), place.pos.x, place.pos.y);

    if (crawl_state.map_stat_gen)
        mapgen_report_map_try(*vault);
#endif

    // Return value of MAP_NONE forces dungeon.cc to regenerate the
    // level, except for branch entry vaults where dungeon.cc just
    // rejects the vault and places a vanilla entry.

    return (write_vault(const_cast<map_def&>(*vault), place, check_place));
}

static int write_vault(map_def &mdef,
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
        if (!resolve_map(place.map))
            continue;

        // Must set size here, or minivaults will not be placed correctly.
        place.size = place.map.size();
        place.orient = apply_vault_definition(place.map,
                                              place, check_place);

        if (place.orient != MAP_NONE)
            return (place.orient);
    }
    return (MAP_NONE);
}

static bool resolve_map_lua(map_def &map)
{
    map.reinit();
    std::string err = map.run_lua(true);
    if (!err.empty())
    {
#ifdef DEBUG_DIAGNOSTICS
        if (crawl_state.map_stat_gen)
            mapgen_report_error(map, err);
#endif
        mprf(MSGCH_ERROR, "Lua error: %s", err.c_str());
        return (false);
    }

    map.fixup();
    err = map.resolve();
    if (!err.empty())
    {
        mprf(MSGCH_ERROR, "Error: %s", err.c_str());
        return (false);
    }

    if (!map.test_lua_validate(false))
        return (false);

    return (true);
}

// Mirror the map if appropriate, resolve substitutable symbols (?),
static bool resolve_map(map_def &map)
{
    if (!resolve_map_lua(map))
        return (false);

    // Don't bother flipping or rotating 1x1 subvaults.
    // This just cuts down on level generation message spam.
    if (map.map.width() == map.map.height() && map.map.width() == 1)
        return (true);

    // Mirroring is possible for any map that does not explicitly forbid it.
    // Note that mirroring also flips the orientation.
    if (coinflip())
        map.hmirror();

    if (coinflip())
        map.vmirror();

    // The map may also refuse to be rotated.
    if (coinflip())
        map.rotate(coinflip());

    return (true);
}

bool resolve_subvault(map_def &map)
{
    ASSERT(map.is_subvault());
    if (!map.is_subvault())
        return false;

    if (!resolve_map_lua(map))
        return false;

    int width = map.subvault_width();
    int height = map.subvault_height();

    bool can_rot = (map.map.width() <= height && map.map.height() <= width);
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
        if (coinflip())
            map.hmirror();

        if (coinflip())
            map.vmirror();

        // The map may have refused to have been rotated, so verify dimensions.
        bool valid = (map.map.width() <= width && map.map.height() <= height);
        return (valid);
    }

    // Don't bother flipping or rotating 1x1 subvaults.
    // This just cuts down on level generation message spam.
    if (exact_fit && width == height && width == 1)
        return (true);

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

    int min_mismatch = std::min(mismatch[0], mismatch[1]);
    min_mismatch = std::min(min_mismatch, mismatch[2]);
    min_mismatch = std::min(min_mismatch, mismatch[3]);

    // Pick a mirror combination with the minimum number of mismatches.
    int idx = random2(4);
    while (mismatch[idx] != min_mismatch)
        idx = (idx + 1) % 4;

    // Flip the map (currently vmirror'd) to the correct orientation.
    if (idx == 0)
    {
        map.vmirror();
    }
    else if (idx == 1)
    {
        map.vmirror();
        map.hmirror();
    }
    else if (idx == 2)
    {
        map.hmirror();
    }

    ASSERT(map.subvault_mismatch_count(svplace) == min_mismatch);

    // We already know this is an exact fit, so this is a success.
    return (true);
}

void fit_region_into_map_bounds(coord_def &pos, const coord_def &size,
                                int margin)
{
    const int X_1(X_BOUND_1 + margin);
    const int X_2(X_BOUND_2 - margin);
    const int Y_1(Y_BOUND_1 + margin);
    const int Y_2(Y_BOUND_2 - margin);

    ASSERT(size.x <= (X_2 - X_1 + 1) && size.y <= (Y_2 - Y_1 + 1));

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
        return (false);

    // If in the abyss, the placement mask is the only check necessary
    // for terrain; we must still check that we're not overwriting
    // items.
    if (Vault_Placement_Mask && player_in_level_area(LEVEL_ABYSS))
    {
        return (igrd(p) == NON_ITEM);
    }

    const dungeon_feature_type grid = grd(p);

    // Deep water grids may be overwritten if water_ok == true.
    if (grid == DNGN_DEEP_WATER)
        return (water_ok);

    // Handle all other non-LOS blocking grids here.
    if (!feat_is_opaque(grid)
        && grid != DNGN_FLOOR
        && grid != DNGN_SHALLOW_WATER
        && grid != DNGN_OPEN_DOOR
        && grid != DNGN_SECRET_DOOR
        && !feat_is_closed_door(grid))
    {
        return (false);
    }

    if (feat_is_wall(grid) || grid == DNGN_TREE)
        return (wall_ok);

    // Otherwise, feel free to clobber this feature.
    return (true);
}

bool map_safe_vault_place(const map_def &map,
                          const coord_def &c,
                          const coord_def &size)
{
    if (size.zero())
        return (true);

    const bool water_ok =
        map.has_tag("water_ok") || player_in_branch(BRANCH_SWAMP);

    const std::vector<std::string> &lines = map.map.get_lines();
    for (rectangle_iterator ri(c, c + size - 1); ri; ++ri)
    {
        const coord_def cp(*ri);
        const coord_def dp(cp - c);

        if (lines[dp.y][dp.x] == ' ')
            continue;

        // Also check adjacent squares for collisions, because being next
        // to another vault may block off one of this vault's exits.
        for (int y = -1; y <= 1; ++y)
            for (int x = -1; x <= 1; ++x)
            {
                const coord_def vp(x + cp.x, y + cp.y);
                if (map_bounds(vp) && (env.level_map_mask(vp) & MMT_VAULT))
                    return (false);
            }

        // Don't overwrite features other than floor, rock wall, doors,
        // nor water, if !water_ok.
        if (!_may_overwrite_feature(cp, water_ok))
            return (false);

        // Don't overwrite monsters or items, either!
        if (monster_at(cp) || igrd(cp) != NON_ITEM)
            return (false);
    }

    return (true);
}

static bool _connected_minivault_place(const coord_def &c,
                                       const vault_placement &place)
{
    if (place.size.zero())
        return (true);

    // Must not be completely isolated.
    const bool water_ok = place.map.has_tag("water_ok");
    const std::vector<std::string> &lines = place.map.map.get_lines();

    for (rectangle_iterator ri(c, c + place.size - 1); ri; ++ri)
    {
        const coord_def &ci(*ri);

        if (lines[ci.y - c.y][ci.x - c.x] == ' ')
            continue;

        if (_may_overwrite_feature(ci, water_ok, false))
            return (true);
    }

    return (false);
}

static coord_def _find_minivault_place(
    const vault_placement &place,
    bool check_place)
{
    // [ds] The margin around the edges of the map where the minivault
    // won't be placed. Purely arbitrary as far as I can see.
    const int margin = MAPGEN_BORDER * 2;

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
        return (v1);
    }
    return (coord_def(-1, -1));
}

static bool apply_vault_grid(map_def &def,
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
         || orient == MAP_ENCOMPASS)
        && size.x < GXM)
    {
        start.x = (GXM - size.x) / 2;
    }
    if ((orient == MAP_EAST || orient == MAP_WEST
         || orient == MAP_ENCOMPASS)
        && size.y < GYM)
    {
        start.y = (GYM - size.y) / 2;
    }

    // Floating maps can go anywhere, ask the map_def to suggest a place.
    if (orient == MAP_FLOAT)
    {
        const bool minivault = def.has_tag("minivault");
        if (map_bounds(place.pos))
        {
            start = place.pos - size / 2;
            fit_region_into_map_bounds(start, size, minivault ? 2 : 0);
        }
        else if (minivault)
        {
            start = _find_minivault_place(place, check_place);
            if (map_bounds(start))
                fit_region_into_map_bounds(start, size, 2);
        }
        else
            start = def.float_place();
    }

    if (!map_bounds(start))
        return (false);

    if (check_place && !map_place_valid(def, start, size))
    {
        dprf("Bad vault place: (%d,%d) dim (%d,%d)",
             start.x, start.y, size.x, size.y);
        return (false);
    }


    place.pos  = start;
    place.size = size;

    return (true);
}

static int apply_vault_definition(
        map_def &def,
        vault_placement &place,
        bool check_place)
{
    if (!apply_vault_grid(def, place, check_place))
        return (MAP_NONE);

    int orient = def.orient;
    if (orient == MAP_NONE)
        orient = MAP_NORTH;

    return (orient);
}

///////////////////////////////////////////////////////////////////////////
// Map lookups

template <typename I>
static bool map_has_no_tags(const map_def &map, I begin, I end)
{
    for (; begin != end; ++begin)
        if (map.has_tag(*begin))
            return (false);

    return (true);
}

static bool vault_unforbidden(const map_def &map)
{
    return (you.uniq_map_names.find(map.name) == you.uniq_map_names.end()
            && (env.level_uniq_maps.find(map.name) ==
                env.level_uniq_maps.end())
            && map_has_no_tags(map, you.uniq_map_tags.begin(),
                               you.uniq_map_tags.end())
            && map_has_no_tags(map, env.level_uniq_map_tags.begin(),
                               env.level_uniq_map_tags.end()));
}

static bool map_matches_layout_type(const map_def &map)
{
    return (env.level_layout_type.empty()
            || !map.has_tag_prefix("layout_")
            || map.has_tag("layout_" + env.level_layout_type));
}

static bool _map_matches_species(const map_def &map)
{
    if (you.species < 0 || you.species >= NUM_SPECIES)
        return true;
    return !map.has_tag("no_species_"
           + lowercase_string(get_species_abbrev(you.species)));
}

const map_def *find_map_by_name(const std::string &name)
{
    for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
        if (vdefs[i].name == name)
            return (&vdefs[i]);

    return (NULL);
}

// Discards Lua code loaded by all maps to reduce memory use. If any stripped
// map is reused, its data will be reloaded from the .dsc
void strip_all_maps()
{
    for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
        vdefs[i].strip();
}

std::vector<std::string> find_map_matches(const std::string &name)
{
    std::vector<std::string> matches;

    for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
        if (vdefs[i].name.find(name) != std::string::npos)
            matches.push_back(vdefs[i].name);
    return (matches);
}

mapref_vector find_maps_for_tag(const std::string tag,
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
            && (!check_used || vault_unforbidden(mapdef)))
        {
            maps.push_back(&mapdef);
        }
    }
    return (maps);
}

int weight_map_vector (std::vector<map_def> maps)
{
    int weights = 0;

    for (std::vector<map_def>::iterator mi = maps.begin(); mi != maps.end(); ++mi)
        weights += mi->weight;

    return (weights);
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
        return (sel == TAG || place.is_valid());
    }

    static map_selector by_place(const level_id &_place, bool _mini)
    {
        return map_selector(map_selector::PLACE, _place, "", _mini, false);
    }

    static map_selector by_depth(const level_id &_place, bool _mini)
    {
        return map_selector(map_selector::DEPTH, _place, "", _mini, true);
    }

    static map_selector by_depth_chance(const level_id &_place)
    {
        return map_selector(map_selector::DEPTH_AND_CHANCE, _place, "", false,
                            true);
    }

    static map_selector by_tag(const std::string &_tag,
                               bool _check_depth,
                               bool _check_chance,
                               const level_id &_place = level_id::current())
    {
        map_selector msel = map_selector(map_selector::TAG, _place, _tag,
                                         false, _check_depth);
        msel.ignore_chance = !_check_chance;
        return (msel);
    }

private:
    map_selector(select_type _typ, const level_id &_pl,
                 const std::string &_tag,
                 bool _mini, bool _check_depth)
        : ignore_chance(false), preserve_dummy(false),
          sel(_typ), place(_pl), tag(_tag),
          mini(_mini), check_depth(_check_depth),
          check_layout(sel == DEPTH && place == level_id::current())
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
    const std::string tag;
    const bool mini;
    const bool check_depth;
    const bool check_layout;
};

bool map_selector::depth_selectable(const map_def &mapdef) const
{
    return (!mapdef.place.is_valid()
            && mapdef.is_usable_in(place)
            // Some tagged levels cannot be selected as random
            // maps in a specific depth:
            && !mapdef.has_tag_suffix("entry")
            && !mapdef.has_tag("unrand")
            && !mapdef.has_tag("place_unique")
            && !mapdef.has_tag("tutorial")
            && (!mapdef.has_tag_prefix("temple_")
                || mapdef.has_tag_prefix("uniq_altar_"))
            && _map_matches_species(mapdef)
            && (!check_layout || map_matches_layout_type(mapdef)));
}

bool map_selector::accept(const map_def &mapdef) const
{
    switch (sel)
    {
    case PLACE:
        return (mapdef.is_minivault() == mini
                && mapdef.place == place
                && (!mapdef.has_tag("tutorial")
                    || crawl_state.game_is_tutorial())
                && map_matches_layout_type(mapdef)
                && vault_unforbidden(mapdef));

    case DEPTH:
        return (mapdef.is_minivault() == mini
                && (!mapdef.chance || !mapdef.chance_priority)
                && depth_selectable(mapdef)
                && vault_unforbidden(mapdef));

    case DEPTH_AND_CHANCE:
        return (// Only vaults with valid chance and chance priority
                (mapdef.chance > 0 && mapdef.chance_priority > 0)
                && depth_selectable(mapdef)
                && vault_unforbidden(mapdef));

    case TAG:
        return (mapdef.has_tag(tag)
                && (!check_depth || !mapdef.has_depth()
                    || mapdef.is_usable_in(place))
                && _map_matches_species(mapdef)
                && map_matches_layout_type(mapdef)
                && vault_unforbidden(mapdef));

    default:
        return (false);
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
                 "[CHANCE+DEPTH] Found map %s for %s (%d:%d)",
                 vault->name.c_str(), place.describe().c_str(),
                 vault->chance_priority, vault->chance);
        }
        else
        {
            const char *format =
                sel == PLACE? "[PLACE] Found map %s for %s" :
                sel == DEPTH? "[DEPTH] Found random map %s for %s" :
                "[TAG] Found map %s tagged '%s'";

            mprf(MSGCH_DIAGNOSTICS, format,
                 vault->name.c_str(),
                 sel == TAG? tag.c_str() : place.describe().c_str());
        }
    }
#endif
}

static std::string _vault_chance_tag(const map_def &map)
{
    if (map.has_tag_prefix("chance_"))
    {
        const std::vector<std::string> tags = map.get_tags();
        for (int i = 0, size = tags.size(); i < size; ++i)
        {
            if (tags[i].find("chance_") == 0)
                return (tags[i]);
        }
    }
    return ("");
}

typedef std::vector<unsigned> vault_indices;

static vault_indices _eligible_maps_for_selector(const map_selector &sel)
{
    vault_indices eligible;

    if (sel.valid())
    {
        for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
            if (sel.accept(vdefs[i]))
                eligible.push_back(i);
    }

    return (eligible);
}

static const map_def *_random_map_by_selector(const map_selector &sel);

static bool _vault_chance_new(const map_def &map,
                              std::set<std::string> &chance_tags)
{
    if (map.chance > 0)
    {
        // There may be several alternatives for a portal
        // vault that want to be governed by one common
        // CHANCE. In this case each vault will use a
        // CHANCE, and a common chance_xxx tag. Pick the
        // first such vault for the chance roll. Note that
        // at this point we ignore chance_priority.
        const std::string tag = _vault_chance_tag(map);
        if (chance_tags.find(tag) == chance_tags.end())
        {
            if (!tag.empty())
                chance_tags.insert(tag);
            return (true);
        }
    }
    return (false);
}

class vault_chance_roll_iterator
{
public:
    vault_chance_roll_iterator(const mapref_vector &_maps)
        : maps(_maps), current(_maps.begin()), end(_maps.end())
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
        return (*this);
    }

    vault_chance_roll_iterator operator ++ (int)
    {
        vault_chance_roll_iterator copy(*this);
        operator ++ ();
        return (copy);
    }

private:
    void find_valid()
    {
        while (current != end && random2(CHANCE_ROLL) >= (*current)->chance)
            ++current;
    }

private:
    const std::vector<const map_def *> &maps;
    mapref_vector::const_iterator current;
    mapref_vector::const_iterator end;
};

static const map_def *_resolve_chance_vault(const map_selector &sel,
                                            const map_def *map)
{
    const std::string chance_tag = _vault_chance_tag(*map);
    // If this map has a chance_ tag, convert the search into
    // a lookup for that tag.
    if (!chance_tag.empty())
    {
        map_selector msel = map_selector::by_tag(chance_tag,
                                                 sel.check_depth,
                                                 false,
                                                 sel.place);
        return _random_map_by_selector(msel);
    }
    return (map);
}

static mapref_vector
_random_chance_maps_in_list(const map_selector &sel,
                            const vault_indices &filtered)
{
    // Vaults that are eligible and have >0 chance.
    mapref_vector chance;
    mapref_vector chosen_chances;

    typedef std::set<std::string> tag_set;
    tag_set chance_tags;

    for (unsigned f = 0, size = filtered.size(); f < size; ++f)
    {
        const int i = filtered[f];
        if (!sel.ignore_chance && _vault_chance_new(vdefs[i], chance_tags))
            chance.push_back(&vdefs[i]);
    }

    for (vault_chance_roll_iterator vc(chance); vc; ++vc)
        if (const map_def *chosen = _resolve_chance_vault(sel, *vc))
        {
            chosen_chances.push_back(chosen);
            sel.announce(chosen);
        }

    return (chosen_chances);
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

    typedef std::set<std::string> tag_set;
    tag_set chance_tags;

    for (unsigned f = 0, size = filtered.size(); f < size; ++f)
    {
        const int i = filtered[f];
        if (!sel.ignore_chance && vdefs[i].chance)
        {
            if (_vault_chance_new(vdefs[i], chance_tags))
                chance.push_back(&vdefs[i]);
        }
        else
        {
            eligible.push_back(&vdefs[i]);
        }
    }

    for (vault_chance_roll_iterator vc(chance); vc; ++vc)
        if (const map_def *chosen = _resolve_chance_vault(sel, *vc))
        {
            chosen_map = chosen;
            break;
        }

    if (!chosen_map)
    {
        int absdepth = 0;
        if (sel.place.level_type == LEVEL_DUNGEON && sel.place.is_valid())
            absdepth = sel.place.absdepth();

        for (mapref_vector::const_iterator i = eligible.begin();
             i != eligible.end(); ++i)
        {
            const map_def &map(**i);
            const int weight = map.weight
                + absdepth * map.weight_depth_mult / map.weight_depth_div;

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
    return (chosen_map);
}

static const map_def *_random_map_by_selector(const map_selector &sel)
{
    const vault_indices filtered = _eligible_maps_for_selector(sel);
    return _random_map_in_list(sel, filtered);
}

// Returns a map for which PLACE: matches the given place.
const map_def *random_map_for_place(const level_id &place, bool minivault)
{
    return _random_map_by_selector(map_selector::by_place(place, minivault));
}

const map_def *random_map_in_depth(const level_id &place, bool want_minivault)
{
    return _random_map_by_selector(
        map_selector::by_depth(place, want_minivault));
}

mapref_vector random_chance_maps_in_depth(const level_id &place)
{
    map_selector sel = map_selector::by_depth_chance(place);
    const vault_indices eligible = _eligible_maps_for_selector(sel);
    return _random_chance_maps_in_list(sel, eligible);
}

const map_def *random_map_for_tag(const std::string &tag,
                                  bool check_depth,
                                  bool check_chance)
{
    return _random_map_by_selector(
        map_selector::by_tag(tag, check_depth, check_chance));
}

int map_count()
{
    return (vdefs.size());
}

int map_count_for_tag(const std::string &tag,
                      bool check_depth)
{
    return _eligible_maps_for_selector(
        map_selector::by_tag(tag, check_depth, false)).size();
}

/////////////////////////////////////////////////////////////////////////////
// Reading maps from .des files.

// All global preludes.
std::vector<dlua_chunk> global_preludes;

// Map-specific prelude.
dlua_chunk lc_global_prelude("global_prelude");
std::string lc_desfile;
map_def     lc_map;
level_range lc_range;
depth_ranges lc_default_depths;
bool lc_run_global_prelude = true;
map_load_info_t lc_loaded_maps;

std::set<std::string> map_files_read;

extern int yylineno;

void reset_map_parser()
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

static std::string _des_cache_dir(const std::string &relpath = "")
{
    return catpath(savedir_versioned_path("des"), relpath);
}

static void check_des_index_dir()
{
    if (checked_des_index_dir)
        return;

    std::string desdir = _des_cache_dir();
    if (!check_mkdir("Data file cache", &desdir, true))
        end(1, true, "Can't create data file cache: %s", desdir.c_str());

    checked_des_index_dir = true;
}

std::string get_descache_path(const std::string &file,
                              const std::string &ext)
{
    const std::string basename =
        change_file_extension(get_base_filename(file), ext);
    return _des_cache_dir(basename);
}

static bool verify_file_version(const std::string &file)
{
    FILE *fp = fopen(file.c_str(), "rb");
    if (!fp)
        return (false);
    reader inf(fp);
    const long ver = unmarshallInt(inf);
    fclose(fp);

    return (ver == MAP_CACHE_VERSION);
}

static bool verify_map_index(const std::string &base)
{
    return verify_file_version(base + ".idx");
}

static bool verify_map_full(const std::string &base)
{
    return verify_file_version(base + ".dsc");
}

static bool load_map_index(const std::string &base)
{
    // If there's a global prelude, load that first.
    {
        FILE *fp = fopen((base + ".lux").c_str(), "rb");
        if (fp)
        {
            reader inf(fp);
            lc_global_prelude.read(inf);
            fclose(fp);

            global_preludes.push_back(lc_global_prelude);
        }
    }

    FILE* fp = fopen((base + ".idx").c_str(), "rb");
    if (!fp)
        end(1, true, "Unable to read %s", (base + ".idx").c_str());
    reader inf(fp);

    // Discard version (it's been checked by verify_map_index).
    (void) unmarshallInt(inf);
    const int nmaps = unmarshallShort(inf);
    const int nexist = vdefs.size();
    vdefs.resize(nexist + nmaps, map_def());
    for (int i = 0; i < nmaps; ++i)
    {
        map_def &vdef(vdefs[nexist + i]);
        vdef.read_index(inf);
        vdef.description = unmarshallString(inf);

        vdef.set_file(base);
        lc_loaded_maps[vdef.name] = vdef.place_loaded_from;
        vdef.place_loaded_from.clear();
    }
    fclose(fp);

    return (true);
}

static bool load_map_cache(const std::string &filename)
{
    check_des_index_dir();
    const std::string descache_base = get_descache_path(filename, "");

    file_lock deslock(descache_base + ".lk", "rb", false);

    std::string file_idx = descache_base + ".idx";
    std::string file_dsc = descache_base + ".dsc";

    if (is_newer(filename, file_idx) || is_newer(filename, file_dsc))
        return (false);

#ifdef USE_TILE
    // When the executable is rebuilt for tiles, it's possible that
    // the tile enums have changed, breaking old cache files.
    // So, conservatively regenerate cache files after rebuilding.
    {
        std::string exe_path = SysEnv.crawl_base + FILE_SEPARATOR
                               + SysEnv.crawl_exe;
        if (is_newer(exe_path, file_idx) || is_newer(exe_path, file_dsc))
            return (false);
    }
#endif

    if (!verify_map_index(descache_base) || !verify_map_full(descache_base))
        return (false);

    return load_map_index(descache_base);
}

static void write_map_prelude(const std::string &filebase)
{
    const std::string luafile = filebase + ".lux";
    if (lc_global_prelude.empty())
    {
        unlink(luafile.c_str());
        return;
    }

    FILE *fp = fopen(luafile.c_str(), "wb");
    writer outf(luafile, fp);
    lc_global_prelude.write(outf);
    fclose(fp);
}

static void write_map_full(const std::string &filebase, size_t vs, size_t ve)
{
    const std::string cfile = filebase + ".dsc";
    FILE *fp = fopen(cfile.c_str(), "wb");
    if (!fp)
        end(1, true, "Unable to open %s for writing", cfile.c_str());

    writer outf(cfile, fp);
    marshallInt(outf, MAP_CACHE_VERSION);
    for (size_t i = vs; i < ve; ++i)
        vdefs[i].write_full(outf);
    fclose(fp);
}

static void write_map_index(const std::string &filebase, size_t vs, size_t ve)
{
    const std::string cfile = filebase + ".idx";
    FILE *fp = fopen(cfile.c_str(), "wb");
    if (!fp)
        end(1, true, "Unable to open %s for writing", cfile.c_str());

    writer outf(cfile, fp);
    marshallInt(outf, MAP_CACHE_VERSION);
    marshallShort(outf, ve > vs? ve - vs : 0);
    for (size_t i = vs; i < ve; ++i)
    {
        vdefs[i].write_index(outf);
        marshallString(outf, vdefs[i].description);
        vdefs[i].place_loaded_from.clear();
        vdefs[i].strip();
    }
    fclose(fp);
}

static void write_map_cache(const std::string &filename, size_t vs, size_t ve)
{
    check_des_index_dir();

    const std::string descache_base = get_descache_path(filename, "");

    file_lock deslock(descache_base + ".lk", "wb");

    write_map_prelude(descache_base);
    write_map_full(descache_base, vs, ve);
    write_map_index(descache_base, vs, ve);
}

static void parse_maps(const std::string &s)
{
    const std::string base = get_base_filename(s);
    if (map_files_read.find(base) != map_files_read.end())
        return;

    map_files_read.insert(base);

    if (load_map_cache(s))
        return;

    FILE *dat = fopen(s.c_str(), "r");
    if (!dat)
        end(1, true, "Failed to open %s for reading", s.c_str());

    reset_map_parser();

    extern int yyparse(void);
    extern FILE *yyin;
    yyin = dat;

    const size_t file_start = vdefs.size();
    yyparse();
    fclose(dat);

    global_preludes.push_back(lc_global_prelude);
    write_map_cache(s, file_start, vdefs.size());
}

void read_map(const std::string &file)
{
    parse_maps(lc_desfile = datafile_path(file));
    // Clean up cached environments.
    dlua.callfn("dgn_flush_map_environments", 0, 0);
    // Force GC to prevent heap from swelling unnecessarily.
    dlua.gc();
}

void read_maps()
{
    if (dlua.execfile("clua/loadmaps.lua", true, true, true))
        end(1, false, "Lua error: %s", dlua.error.c_str());

    lc_loaded_maps.clear();
    sanity_check_maps();
}

// If a .dsc file has been changed under the running Crawl, discard
// all map knowledge and reload maps. This will not affect maps that
// have already been used, but it might trigger exciting happenings if
// the new maps fail sanity checks or remove maps that the game
// expects to be present.
void reread_maps()
{
    dprf("reread_maps:: discarding %u existing maps",
         vdefs.size());

    // BOOM!
    vdefs.clear();
    map_files_read.clear();
    read_maps();
}

void add_parsed_map(const map_def &md)
{
    map_def map = md;

    map.fixup();
    vdefs.push_back(map);
}

void run_map_preludes()
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
    for (int i = 0, size = vdefs.size(); i < size; ++i)
    {
        if (!vdefs[i].prelude.empty())
        {
            std::string err = vdefs[i].run_lua(true);
            if (!err.empty())
                mprf(MSGCH_ERROR, "Lua error (map %s): %s",
                     vdefs[i].name.c_str(), err.c_str());
        }
    }
}

const map_def *map_by_index(int index)
{
    return (&vdefs[index]);
}

void sanity_check_maps()
{
    dlua.execfile("clua/sanity.lua", true, true);
}

///////////////////////////////////////////////////////////////////////////
// Debugging code

#ifdef DEBUG_DIAGNOSTICS

typedef std::pair<std::string, int> weighted_map_name;
typedef std::vector<weighted_map_name> weighted_map_names;

static weighted_map_names mg_find_random_vaults(
    const level_id &place, bool wantmini)
{
    weighted_map_names wms;

    if (!place.is_valid())
        return (wms);

    typedef std::map<std::string, int> map_count_t;

    map_count_t map_counts;

    map_selector sel = map_selector::by_depth(place, wantmini);
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

    return (wms);
}

static bool weighted_map_more_likely(
    const weighted_map_name &a,
    const weighted_map_name &b)
{
    return (a.second > b.second);
}

static void mg_report_random_vaults(
    FILE *outf, const level_id &place, bool wantmini)
{
    weighted_map_names wms = mg_find_random_vaults(place, wantmini);
    std::sort(wms.begin(), wms.end(), weighted_map_more_likely);
    int weightsum = 0;
    for (int i = 0, size = wms.size(); i < size; ++i)
        weightsum += wms[i].second;

    std::string line;
    for (int i = 0, size = wms.size(); i < size; ++i)
    {
        std::string curr =
            make_stringf("%s (%.2f%%)",
                         wms[i].first.c_str(),
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
    mg_report_random_vaults(outf, place, true);
    fprintf(outf, "------------- Regular\n");
    mg_report_random_vaults(outf, place, false);
}

#endif
