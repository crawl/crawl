/*
 *  File:       maps.cc
 *  Summary:    Functions used to create vaults.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "maps.h"
#include "tags.h"

#include <cstring>
#include <cstdlib>
#include <algorithm>

#if !_MSC_VER
#include <unistd.h>
#endif

#include "dungeon.h"
#include "enum.h"
#include "files.h"
#include "message.h"
#include "monplace.h"
#include "mapdef.h"
#include "misc.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"

static bool _safe_vault_place(const map_def &md,
                              const coord_def &c,
                              const coord_def &size);

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
map_place_check_t map_place_valid = _safe_vault_place;

// If non-empty, any floating vault's @ exit must land on these point.
point_vector map_anchor_points;

//////////////////////////////////////////////////////////////////////////
// New style vault definitions

typedef std::vector<map_def> map_vector;
static map_vector vdefs;

/* ******************** BEGIN PUBLIC FUNCTIONS ******************* */

// Remember (!!!) - if a member of the monster array isn't specified
// within a vault subroutine, assume that it holds a random monster
// -- only in cases of explicit monsters assignment have I listed
// out random monster insertion {dlb}

// Make sure that vault_n, where n is a number, is a vault which can be put
// anywhere, while other vault names are for specific level ranges, etc.
int vault_main( vault_placement &place, const map_def *vault,
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

    return (write_vault( const_cast<map_def&>(*vault), place, check_place ));
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

        place.orient = apply_vault_definition(place.map,
                                              place, check_place);

        if (place.orient != MAP_NONE)
            break;
    }
    return (place.orient);
}

// Mirror the map if appropriate, resolve substitutable symbols (?),
static bool resolve_map(map_def &map)
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

    // Mirroring is possible for any map that does not explicitly forbid it.
    // Note that mirroring also flips the orientation.
    if (coinflip())
        map.hmirror();

    if (coinflip())
        map.vmirror();

    // The map may also refuse to be rotated.
    if (coinflip())
        map.rotate( coinflip() );

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
static bool _may_overwrite_feature(const dungeon_feature_type grid,
                                   bool water_ok, bool wall_ok = true)
{
    // Deep water grids may be overwritten if water_ok == true.
    if (grid == DNGN_DEEP_WATER)
        return (water_ok);

    // Handle all other non-LOS blocking grids here.
    if (!grid_is_opaque(grid)
        && grid != DNGN_FLOOR
        && grid != DNGN_SHALLOW_WATER
        && grid != DNGN_CLOSED_DOOR
        && grid != DNGN_OPEN_DOOR
        && grid != DNGN_SECRET_DOOR)
    {
        return (false);
    }

    if (grid_is_wall(grid))
        return (wall_ok);

    // Otherwise, feel free to clobber this feature.
    return (true);
}

static bool _safe_vault_place(const map_def &map,
                              const coord_def &c,
                              const coord_def &size)
{
    if (size.zero())
        return (true);

    const bool water_ok = map.has_tag("water_ok");
    const std::vector<std::string> &lines = map.map.get_lines();

    for (rectangle_iterator ri(c, c + size - 1); ri; ++ri)
    {
        const coord_def &cp(*ri);
        const coord_def &dp(cp - c);

        if (lines[dp.y][dp.x] == ' ')
            continue;

        if (dgn_Map_Mask[cp.x][cp.y])
            return (false);

        const dungeon_feature_type dfeat = grd(cp);

        // Don't overwrite features other than floor, rock wall, doors,
        // nor water, if !water_ok.
        if (!_may_overwrite_feature(dfeat, water_ok))
            return (false);

        // Don't overwrite items or monsters, either!
        if (igrd(cp) != NON_ITEM || mgrd(cp) != NON_MONSTER)
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

        if (_may_overwrite_feature(grd(ci), water_ok, false))
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
        coord_def v1(random_range( margin, GXM - margin - place.size.x ),
                     random_range( margin, GYM - margin - place.size.y ));

        if (check_place && !map_place_valid(place.map, v1, place.size))
            continue;

        if (_connected_minivault_place(v1, place))
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

    if (check_place && !_safe_vault_place(def, start, size))
    {
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Bad vault place: (%d,%d) dim (%d,%d)",
             start.x, start.y, size.x, size.y);
#endif
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
    for ( ; begin != end; ++begin)
        if (map.has_tag(*begin))
            return (false);

    return (true);
}

static bool vault_unforbidden(const map_def &map)
{
    return (you.uniq_map_names.find(map.name) == you.uniq_map_names.end()
            && Level_Unique_Maps.find(map.name) == Level_Unique_Maps.end()
            && map_has_no_tags(map, you.uniq_map_tags.begin(),
                               you.uniq_map_tags.end())
            && map_has_no_tags(map, Level_Unique_Tags.begin(),
                               Level_Unique_Tags.end()));
}

static bool map_matches_layout_type(const map_def &map)
{
    if (dgn_Layout_Type.empty() || !map.has_tag_prefix("layout_"))
        return (true);

    return map.has_tag("layout_" + dgn_Layout_Type);
}

const map_def *find_map_by_name(const std::string &name)
{
    for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
        if (vdefs[i].name == name)
            return (&vdefs[i]);

    return (NULL);
}

std::vector<std::string> find_map_matches(const std::string &name)
{
    std::vector<std::string> matches;

    for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
        if (vdefs[i].name.find(name) != std::string::npos)
            matches.push_back(vdefs[i].name);
    return (matches);
}

struct map_selector {
private:
    enum select_type
    {
        PLACE,
        DEPTH,
        TAG
    };

public:
    bool accept(const map_def &md) const;
    void announce(int vault) const;

    bool valid() const
    {
        return (sel == TAG || place.is_valid());
    }

    static map_selector by_place(const level_id &place)
    {
        return map_selector(map_selector::PLACE, place, "", false, false);
    }

    static map_selector by_depth(const level_id &place, bool mini)
    {
        return map_selector(map_selector::DEPTH, place, "", mini, true);
    }

    static map_selector by_tag(const std::string &tag,
                               bool check_depth,
                               const level_id &place = level_id::current())
    {
        return map_selector(map_selector::TAG, place, tag,
                            false, check_depth);
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
    }

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

bool map_selector::accept(const map_def &mapdef) const
{
    switch (sel)
    {
    case PLACE:
        return (mapdef.place == place
                && !mapdef.has_tag("layout")
                && map_matches_layout_type(mapdef)
                && vault_unforbidden(mapdef));
    case DEPTH:
        return (mapdef.is_minivault() == mini
                && !mapdef.place.is_valid()
                && mapdef.is_usable_in(place)
                // Some tagged levels cannot be selected by depth. This is
                // the only thing preventing Pandemonium demon vaults from
                // showing up in the main dungeon.
                && !mapdef.has_tag_suffix("entry")
                && !mapdef.has_tag("pan")
                && !mapdef.has_tag("unrand")
                && !mapdef.has_tag("bazaar")
                && !mapdef.has_tag("layout")
                && (!check_layout || map_matches_layout_type(mapdef))
                && vault_unforbidden(mapdef));
    case TAG:
        return (mapdef.has_tag(tag)
                && (!check_depth || !mapdef.has_depth()
                    || mapdef.is_usable_in(place))
                && map_matches_layout_type(mapdef)
                && vault_unforbidden(mapdef));
    default:
        return (false);
    }
}

void map_selector::announce(int vault) const
{
#ifdef DEBUG_DIAGNOSTICS
    if (vault != -1)
    {
        const char *format =
            sel == PLACE? "[PLACE] Found map %s for %s" :
            sel == DEPTH? "[DEPTH] Found random map %s for %s" :
                          "[TAG] Found map %s tagged '%s'";

        mprf(MSGCH_DIAGNOSTICS, format,
             vdefs[vault].name.c_str(),
             sel == TAG? tag.c_str() : place.describe().c_str());
    }
#endif
}

static bool _compare_vault_chance_priority(int a, int b)
{
    return (vdefs[b].chance_priority < vdefs[a].chance_priority);
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
static const map_def *_random_map_in_list(const map_selector &sel,
                                          const vault_indices &filtered)
{
    int mapindex = -1;
    int rollsize = 0;

    // First build a list of vaults that could be used:
    vault_indices eligible;

    // Vaults that are eligible and have >0 chance.
    vault_indices chance;

    typedef std::set<std::string> tag_set;
    tag_set chance_tags;

    for (unsigned f = 0, size = filtered.size(); f < size; ++f)
    {
        const int i = filtered[f];
        if (!sel.ignore_chance && vdefs[i].chance > 0)
        {
            // There may be several alternatives for a portal
            // vault that want to be governed by one common
            // CHANCE. In this case each vault will use a
            // CHANCE, and a common chance_xxx tag. Pick the
            // first such vault for the chance roll. Note that
            // at this point we ignore chance_priority.
            const std::string tag = _vault_chance_tag(vdefs[i]);
            if (chance_tags.find(tag) == chance_tags.end())
            {
                if (!tag.empty())
                    chance_tags.insert(tag);
                chance.push_back(i);
            }
        }
        else
        {
            eligible.push_back(i);
        }
    }

    // Sort chances by priority, high priority first.
    std::sort(chance.begin(), chance.end(), _compare_vault_chance_priority);

    // Check CHANCEs.
    for (vault_indices::const_iterator i = chance.begin();
         i != chance.end(); ++i)
    {
        const map_def &map(vdefs[*i]);
        if (random2(CHANCE_ROLL) < map.chance)
        {
            const std::string chance_tag = _vault_chance_tag(map);
            // If this map has a chance_ tag, convert the search into
            // a lookup for that tag.
            if (!chance_tag.empty())
            {
                map_selector msel = map_selector::by_tag(chance_tag,
                                                         sel.check_depth,
                                                         sel.place);
                msel.ignore_chance = true;
                return _random_map_by_selector(msel);
            }

            mapindex = *i;
            break;
        }
    }

    if (mapindex == -1)
    {
        int absdepth = 0;
        if (sel.place.level_type == LEVEL_DUNGEON && sel.place.is_valid())
            absdepth = sel.place.absdepth();

        for (vault_indices::const_iterator i = eligible.begin();
             i != eligible.end(); ++i)
        {
            const map_def &map(vdefs[*i]);
            const int weight = map.weight
                + absdepth * map.weight_depth_mult / map.weight_depth_div;

            if (weight <= 0)
                continue;

            rollsize += weight;

            if (rollsize && x_chance_in_y(weight, rollsize))
                mapindex = *i;
        }
    }

    if (!sel.preserve_dummy && mapindex != -1
        && vdefs[mapindex].has_tag("dummy"))
    {
        mapindex = -1;
    }

    sel.announce(mapindex);

    return (mapindex == -1? NULL : &vdefs[mapindex]);
}

static const map_def *_random_map_by_selector(const map_selector &sel)
{
    const vault_indices filtered = _eligible_maps_for_selector(sel);
    return _random_map_in_list(sel, filtered);
}

// Returns a map for which PLACE: matches the given place.
const map_def *random_map_for_place(const level_id &place)
{
    return _random_map_by_selector(map_selector::by_place(place));
}

const map_def *random_map_in_depth(const level_id &place, bool want_minivault)
{
    return _random_map_by_selector(
        map_selector::by_depth(place, want_minivault));
}

const map_def *random_map_for_tag(const std::string &tag,
                                  bool check_depth)
{
    return _random_map_by_selector(
        map_selector::by_tag(tag, check_depth));
}

int map_count()
{
    return (vdefs.size());
}

int map_count_for_tag(const std::string &tag,
                      bool check_depth)
{
    return _eligible_maps_for_selector(
        map_selector::by_tag(tag, check_depth)).size();
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

static void check_des_index_dir()
{
    if (checked_des_index_dir)
        return;

    std::string desdir = get_savedir_path("des");
    if (!check_dir("Data file cache", desdir, true))
        end(1, true, "Can't create data file cache: %s", desdir.c_str());

    checked_des_index_dir = true;
}

std::string get_descache_path(const std::string &file,
                              const std::string &ext)
{
    const std::string basename =
        change_file_extension(get_base_filename(file), ext);
    return get_savedir_path("des/" + basename);
}

static bool verify_file_version(const std::string &file)
{
    FILE *fp = fopen(file.c_str(), "rb");
    if (!fp)
        return (false);
    reader inf(fp);
    const long ver = unmarshallLong(inf);
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

            global_preludes.push_back( lc_global_prelude );
        }
    }

    FILE* fp = fopen((base + ".idx").c_str(), "rb");
    if (!fp)
        end(1, true, "Unable to read %s", (base + ".idx").c_str());
    reader inf(fp);

    // Discard version (it's been checked by verify_map_index).
    (void) unmarshallLong(inf);
    const int nmaps = unmarshallShort(inf);
    const int nexist = vdefs.size();
    vdefs.resize( nexist + nmaps, map_def() );
    for (int i = 0; i < nmaps; ++i)
    {
        map_def &vdef(vdefs[nexist + i]);
        vdef.read_index(inf);
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

    if (is_newer(filename, descache_base + ".idx")
        || is_newer(filename, descache_base + ".dsc"))
        return (false);

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
    writer outf(fp);
    lc_global_prelude.write(outf);
    fclose(fp);
}

static void write_map_full(const std::string &filebase, size_t vs, size_t ve)
{
    const std::string cfile = filebase + ".dsc";
    FILE *fp = fopen(cfile.c_str(), "wb");
    if (!fp)
        end(1, true, "Unable to open %s for writing", cfile.c_str());

    writer outf(fp);
    marshallLong(outf, MAP_CACHE_VERSION);
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

    writer outf(fp);
    marshallLong(outf, MAP_CACHE_VERSION);
    marshallShort(outf, ve > vs? ve - vs : 0);
    for (size_t i = vs; i < ve; ++i)
        vdefs[i].write_index(outf);
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
        end(1, false, "Map file %s has already been read.", base.c_str());

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

    size_t file_start = vdefs.size();
    yyparse();
    fclose(dat);

    global_preludes.push_back( lc_global_prelude );
    write_map_cache(s, file_start, vdefs.size());
}

void read_map(const std::string &file)
{
    parse_maps( lc_desfile = datafile_path(file) );
}

void read_maps()
{
    if (dlua.execfile("clua/loadmaps.lua", true, true))
        end(1, false, "Lua error: %s", dlua.error.c_str());

    // Clean up cached environments.
    dlua.callfn("dgn_flush_map_environments", 0, 0);
    lc_loaded_maps.clear();
}

void add_parsed_map( const map_def &md )
{
    map_def map = md;

    map.fixup();
    vdefs.push_back( map );
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
