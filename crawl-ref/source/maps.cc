/*
 *  File:       maps.cc
 *  Summary:    Functions used to create vaults.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 * <2>      5/20/99        BWR Added stone lining to Zot vault,
 *                             this should prevent digging?
 * <1>      -/--/--        LRH             Created
 */

#include "AppHdr.h"
#include "maps.h"

#include <cstring>
#include <cstdlib>
#include <errno.h>
#if !_MSC_VER
#include <unistd.h>
#endif

#include "dungeon.h"
#include "enum.h"
#include "files.h"
#include "monplace.h"
#include "mapdef.h"
#include "misc.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"

static int write_vault(map_def &mdef, map_type mt, 
                       vault_placement &,
                       bool check_place, bool clobber);
static int apply_vault_definition(
                        map_def &def,
                        map_type map,
                        vault_placement &,
                        bool check_place, bool clobber);

static bool resolve_map(map_def &def, const map_def &original);


//////////////////////////////////////////////////////////////////////////
// New style vault definitions

static std::vector<map_def> vdefs;

/* ******************** BEGIN PUBLIC FUNCTIONS ******************* */

// remember (!!!) - if a member of the monster array isn't specified
// within a vault subroutine, assume that it holds a random monster
// -- only in cases of explicit monsters assignment have I listed
// out random monster insertion {dlb}

// make sure that vault_n, where n is a number, is a vault which can be put
// anywhere, while other vault names are for specific level ranges, etc.
int vault_main( 
        map_type vgrid, 
        vault_placement &place,
        int which_vault,
        bool check_place,
        bool clobber)
{
#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Generating level: %s (%d,%d)", 
         vdefs[which_vault].name.c_str(),
         place.pos.x, place.pos.y);
    if (crawl_state.map_stat_gen)
        mapgen_report_map_try(vdefs[which_vault]);
#endif

    // first, fill in entirely with walls and null-terminate {dlb}:
    for (int vx = 0; vx < MAP_SIDE; vx++)
    {
        for (int vy = 0; vy < MAP_SIDE; vy++)
          vgrid[vx][vy] = 'x';

        vgrid[MAP_SIDE][vx] = 0;
        vgrid[vx][MAP_SIDE] = 0;
    }

    // Return value of zero forces dungeon.cc to regenerate the level, except
    // for branch entry vaults where dungeon.cc just rejects the vault and
    // places a vanilla entry.
    return (write_vault( vdefs[which_vault], vgrid, place,
                         check_place, clobber ));
}

static int write_vault(map_def &mdef, map_type map, 
                       vault_placement &place,
                       bool check_place, bool clobber)
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
        if (!resolve_map(place.map, mdef))
            continue;

        place.orient = apply_vault_definition(place.map, map,
                                              place, check_place, clobber);

        if (place.orient != MAP_NONE)
            break;
    }
    return (place.orient);
}

// Mirror the map if appropriate, resolve substitutable symbols (?),
static bool resolve_map(map_def &map, const map_def &original)
{
    map.reinit();
    std::string err = map.run_lua(true);
    if (!err.empty())
    {
#ifdef DEBUG_DIAGNOSTICS
        if (crawl_state.map_stat_gen)
            mapgen_report_error(map, err);
#endif
        mprf(MSGCH_WARN, "Lua error: %s", err.c_str());
        return (false);
    }

    map.fixup();
    err = map.resolve();
    if (!err.empty())
    {
        mprf(MSGCH_WARN, "Error: %s", err.c_str());
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

// Determines if the region specified by (x, y, x + width - 1, y + height - 1)
// is a bad place to build a vault.
static bool bad_map_place(const map_def &map,
                          int sx, int sy, int width, int height,
                          bool check_place, bool clobber)
{
    if (!check_place || clobber)
        return (false);
    
    const std::vector<std::string> &lines = map.map.get_lines();
    for (int y = sy; y < sy + height; ++y)
    {
        for (int x = sx; x < sx + width; ++x)
        {
            if (lines[y - sy][x - sx] == ' ')
                continue;

            if (dgn_map_mask[x][y])
                return (true);

            if (igrd[x][y] != NON_ITEM || mgrd[x][y] != NON_MONSTER)
                return (true);
            
            const dungeon_feature_type grid = grd[x][y];

            if (!grid_is_opaque(grid)
                && grid != DNGN_FLOOR
                && !grid_is_water(grid)
                && grid != DNGN_LAVA
                && grid != DNGN_CLOSED_DOOR
                && grid != DNGN_OPEN_DOOR
                && grid != DNGN_SECRET_DOOR)
            {
#ifdef DEBUG_DIAGNOSTICS
                mprf(MSGCH_DIAGNOSTICS,
                     "Rejecting place because of %s at (%d,%d)",
                     dungeon_feature_name(grid), x, y);
#endif
                return (true);
            }
        }
    }

    return (false);
}

void fit_region_into_map_bounds(coord_def &pos, const coord_def &size)
{
    ASSERT(size.x <= GXM && size.y <= GYM);
    if (pos.x < X_BOUND_1)
        pos.x = X_BOUND_1;
    if (pos.y < Y_BOUND_1)
        pos.y = Y_BOUND_1;
    if (pos.x + size.x - 1 > X_BOUND_2)
        pos.x = X_BOUND_2 - size.x + 1;
    if (pos.y + size.y - 1 > Y_BOUND_2)
        pos.y = Y_BOUND_2 - size.y + 1;
}

static bool apply_vault_grid(map_def &def, map_type map, 
                             vault_placement &place,
                             bool check_place, bool clobber)
{
    const map_lines &ml = def.map;
    const int orient = def.orient;
    const int width = ml.width();
    const int height = ml.height();

    coord_def start(0, 0);

    if (orient == MAP_SOUTH || orient == MAP_SOUTHEAST
            || orient == MAP_SOUTHWEST)
        start.y = GYM - height;

    if (orient == MAP_EAST || orient == MAP_NORTHEAST 
            || orient == MAP_SOUTHEAST)
        start.x = GXM - width;

    // Handle maps aligned along cardinals that are smaller than
    // the corresponding map dimension.
    if ((orient == MAP_NORTH || orient == MAP_SOUTH
                || orient == MAP_ENCOMPASS)
            && width < GXM)
        start.x = (GXM - width) / 2;

    if ((orient == MAP_EAST || orient == MAP_WEST
                || orient == MAP_ENCOMPASS)
            && height < GYM)
        start.y = (GYM - height) / 2;

    // Floating maps can go anywhere, ask the map_def to suggest a place.
    if (orient == MAP_FLOAT)
    {
        if (map_bounds(place.pos))
        {
            start = place.pos - coord_def(width, height) / 2;
            fit_region_into_map_bounds(start, coord_def(width, height));
        }
        else
            start = def.float_place();
    }

    if (bad_map_place(def, start.x, start.y, width, height, check_place,
                      clobber))
    {
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Bad vault place: (%d,%d) dim (%d,%d)",
             start.x, start.y, width, height);
#endif
        return (false);
    }

    const std::vector<std::string> &lines = ml.get_lines();
#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Applying %s at (%d,%d), dimensions (%d,%d)",
            def.name.c_str(), start.x, start.y, width, height);
#endif

    for (int y = start.y; y < start.y + height; ++y)
    {
        const std::string &s = lines[y - start.y];
        strncpy(&map[y][start.x], s.c_str(), s.length());
    }

    place.pos  = start;
    place.size = coord_def(width, height);

    return (true);
}

static int apply_vault_definition(
        map_def &def,
        map_type map,
        vault_placement &place,
        bool check_place,
        bool clobber)
{
    if (!apply_vault_grid(def, map, place, check_place, clobber))
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
            && map_has_no_tags(map, you.uniq_map_tags.begin(),
                               you.uniq_map_tags.end()));
}

int find_map_by_name(const std::string &name)
{
    for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
        if (vdefs[i].name == name)
            return (i);
    return (-1);
}

std::vector<std::string> find_map_matches(const std::string &name)
{
    std::vector<std::string> matches;
    
    for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
        if (vdefs[i].name.find(name) != std::string::npos)
            matches.push_back(vdefs[i].name);
    return (matches);
}

// Returns a map for which PLACE: matches the given place.
int random_map_for_place(const level_id &place, bool want_minivault)
{
    if (!place.is_valid())
        return (-1);

    int mapindex = -1;
    int rollsize = 0;

    for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
    {
        // We also accept tagged levels here.
        if (vdefs[i].place == place
            && vdefs[i].is_minivault() == want_minivault
            && vault_unforbidden(vdefs[i]))
        {
            rollsize += vdefs[i].chance;

            if (rollsize && random2(rollsize) < vdefs[i].chance)
                mapindex = i;
        }
    }

    if (mapindex != -1 && vdefs[mapindex].has_tag("dummy"))
        mapindex = -1;
    
#ifdef DEBUG_DIAGNOSTICS
    if (mapindex != -1)
        mprf(MSGCH_DIAGNOSTICS, "Found map %s for %s", 
             vdefs[mapindex].name.c_str(), place.describe().c_str());
#endif

    return (mapindex);
}

int random_map_in_depth(const level_id &place, bool want_minivault)
{
    int mapindex = -1;
    int rollsize = 0;

    for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
        if (vdefs[i].is_minivault() == want_minivault
            && !vdefs[i].place.is_valid()
            && vdefs[i].is_usable_in(place)
            // Some tagged levels cannot be selected by depth. This is
            // the only thing preventing Pandemonium demon vaults from
            // showing up in the main dungeon.
            && !vdefs[i].has_tag_suffix("entry")
            && !vdefs[i].has_tag("pan")
            && !vdefs[i].has_tag("unrand")
            && !vdefs[i].has_tag("bazaar")
            && vault_unforbidden(vdefs[i]))
        {
            rollsize += vdefs[i].chance;

            if (rollsize && random2(rollsize) < vdefs[i].chance)
                mapindex = i;
        }

    if (mapindex != -1 && vdefs[mapindex].has_tag("dummy"))
        mapindex = -1;
    
    return (mapindex);
}

int random_map_for_tag(const std::string &tag,
                       bool want_minivault,
                       bool check_depth)
{
    int mapindex = -1;
    int rollsize = 0;

    for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
    {
        if (vdefs[i].has_tag(tag) && vdefs[i].is_minivault() == want_minivault
            && (!check_depth || !vdefs[i].has_depth()
                || vdefs[i].is_usable_in(level_id::current()))
            && vault_unforbidden(vdefs[i]))
        {
            rollsize += vdefs[i].chance;

            if (rollsize && random2(rollsize) < vdefs[i].chance)
                mapindex = i;
        }
    }

    if (mapindex != -1 && vdefs[mapindex].has_tag("dummy"))
        mapindex = -1;
    
#ifdef DEBUG_DIAGNOSTICS
    if (mapindex != -1)
        mprf(MSGCH_DIAGNOSTICS, "Found map %s tagged '%s'",
                vdefs[mapindex].name.c_str(), tag.c_str());
#endif

    return (mapindex);
}

const map_def *map_by_index(int index)
{
    if (index < 0 || index >= (int) vdefs.size())
        return (NULL);
    
    return &vdefs[index];
}

int map_count()
{
    return (vdefs.size());
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
    FILE *inf = fopen(file.c_str(), "rb");
    if (!inf)
        return (false);

    const long ver = readLong(inf);
    fclose(inf);

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
    FILE *inf = fopen((base + ".lux").c_str(), "rb");
    if (inf)
    {
        lc_global_prelude.read(inf);
        fclose(inf);

        global_preludes.push_back( lc_global_prelude );
    }
    
    inf = fopen((base + ".idx").c_str(), "rb");
    if (!inf)
        end(1, true, "Unable to read %s", (base + ".idx").c_str());
    // Discard version (it's been checked by verify_map_index).
    readLong(inf);
    const int nmaps = readShort(inf);
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
    fclose(inf);

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

    FILE *outf = fopen(luafile.c_str(), "wb");
    lc_global_prelude.write(outf);
    fclose(outf);
}

static void write_map_full(const std::string &filebase, size_t vs, size_t ve)
{
    const std::string cfile = filebase + ".dsc";
    FILE *outf = fopen(cfile.c_str(), "wb");
    if (!outf)
        end(1, true, "Unable to open %s for writing", cfile.c_str());

    writeLong(outf, MAP_CACHE_VERSION);
    for (size_t i = vs; i < ve; ++i)
        vdefs[i].write_full(outf);
    fclose(outf);
}

static void write_map_index(const std::string &filebase, size_t vs, size_t ve)
{
    const std::string cfile = filebase + ".idx";
    FILE *outf = fopen(cfile.c_str(), "wb");
    if (!outf)
        end(1, true, "Unable to open %s for writing", cfile.c_str());

    writeLong(outf, MAP_CACHE_VERSION);
    writeShort(outf, ve > vs? ve - vs : 0);
    for (size_t i = vs; i < ve; ++i)
        vdefs[i].write_index(outf);
    fclose(outf);
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
                mprf(MSGCH_WARN, "Lua error: %s", chunk.orig_error().c_str());
        }
    }
    for (int i = 0, size = vdefs.size(); i < size; ++i)
    {
        if (!vdefs[i].prelude.empty())
        {
            std::string err = vdefs[i].run_lua(true);
            if (!err.empty())
                mprf(MSGCH_WARN, "Lua error (map %s): %s",
                     vdefs[i].name.c_str(), err.c_str());
        }
    }
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

    for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
    {
        if (vdefs[i].is_minivault() == wantmini
            && !vdefs[i].place.is_valid()
            && vdefs[i].is_usable_in(place)
            // Some tagged levels cannot be selected by depth. This is
            // the only thing preventing Pandemonium demon vaults from
            // showing up in the main dungeon.
            && !vdefs[i].has_tag_suffix("entry")
            && !vdefs[i].has_tag("pan")
            && !vdefs[i].has_tag("unrand")
            && !vdefs[i].has_tag("bazaar"))        
        {
            wms.push_back(
                weighted_map_name( vdefs[i].name, vdefs[i].chance ) );
        }
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
