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
#include <unistd.h>

#include "dungeon.h"
#include "enum.h"
#include "files.h"
#include "monplace.h"
#include "mapdef.h"
#include "misc.h"
#include "stuff.h"

static int write_vault(map_def &mdef, map_type mt, 
                       vault_placement &,
                       std::vector<vault_placement> *);
static int apply_vault_definition(
                        map_def &def,
                        map_type map,
                        vault_placement &,
                        std::vector<vault_placement> *);

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
        std::vector<vault_placement> *avoid)
{
#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Generating level: %s", 
                            vdefs[which_vault].name.c_str());
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
    return write_vault( vdefs[which_vault], vgrid, place, avoid );
}

static int write_vault(map_def &mdef, map_type map, 
                       vault_placement &place,
                       std::vector<vault_placement> *avoid)
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
                                              place, avoid);

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
        mprf(MSGCH_WARN, "Lua error: %s", err.c_str());
        return (false);
    }
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

static bool is_grid_clobbered(int sx, int sy, int width, int height)
{
    for (int y = sy; y < sy + height; ++y)
    {
        for (int x = sx; x < sx + width; ++x)
        {
            const dungeon_feature_type grid = grd[x][y];

            if (!grid_is_opaque(grid)
                && grid != DNGN_FLOOR
                && !grid_is_water(grid)
                && grid != DNGN_LAVA
                && grid != DNGN_CLOSED_DOOR
                && grid != DNGN_OPEN_DOOR
                && grid != DNGN_SECRET_DOOR)
            {
                return (true);
            }
        }
    }

    return (false);
}

// Determines if the region specified by (x, y, x + width - 1, y + height - 1)
// is a bad place to build a vault.
static bool bad_map_place(int x, int y, int width, int height,
                         std::vector<vault_placement> *avoid)
{
    if (!avoid)
        return (false);
    
    const dgn_region thisvault(x, y, width, height);

    for (int i = 0, size = avoid->size(); i < size; ++i)
    {
        const vault_placement &vp = (*avoid)[i];
        const dgn_region vault(vp.x, vp.y, vp.width, vp.height);

        if (thisvault.overlaps(vault))
            return (true);
    }

    return (is_grid_clobbered(x, y, width, height));
}

static bool apply_vault_grid(map_def &def, map_type map, 
                             vault_placement &place,
                             std::vector<vault_placement> *avoid)
{
    const map_lines &ml = def.map;
    const int orient = def.orient;
    const int width = ml.width();
    const int height = ml.height();

    int startx = 0, starty = 0;

    if (orient == MAP_SOUTH || orient == MAP_SOUTHEAST
            || orient == MAP_SOUTHWEST)
        starty = GYM - height;

    if (orient == MAP_EAST || orient == MAP_NORTHEAST 
            || orient == MAP_SOUTHEAST)
        startx = GXM - width;

    // Handle maps aligned along cardinals that are smaller than
    // the corresponding map dimension.
    if ((orient == MAP_NORTH || orient == MAP_SOUTH
                || orient == MAP_ENCOMPASS)
            && width < GXM)
        startx = (GXM - width) / 2;

    if ((orient == MAP_EAST || orient == MAP_WEST
                || orient == MAP_ENCOMPASS)
            && height < GYM)
        starty = (GYM - height) / 2;

    // Floating maps can go anywhere, ask the map_def to suggest a place.
    if (orient == MAP_FLOAT)
    {
        coord_def where = def.float_place();

        // No further sanity checks.
        startx = where.x;
        starty = where.y;
    }

    if (bad_map_place(startx, starty, width, height, avoid))
    {
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Bad vault place: (%d,%d) dim (%d,%d)",
             startx, starty, width, height);
#endif
        return (false);
    }

    const std::vector<std::string> &lines = ml.get_lines();
#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Applying %s at (%d,%d), dimensions (%d,%d)",
            def.name.c_str(), startx, starty, width, height);
#endif

    for (int y = starty; y < starty + height; ++y)
    {
        const std::string &s = lines[y - starty];
        strncpy(&map[y][startx], s.c_str(), s.length());
    }

    place.x = startx;
    place.y = starty;
    place.width = width;
    place.height = height;

    return (true);
}

static int apply_vault_definition(
        map_def &def,
        map_type map,
        vault_placement &place,
        std::vector<vault_placement> *avoid)
{
    if (!apply_vault_grid(def, map, place, avoid))
        return (MAP_NONE);

    int orient = def.orient;
    if (orient == MAP_NONE)
        orient = MAP_NORTH;
    
    return (orient);
}

///////////////////////////////////////////////////////////////////////////
// Map lookups

// Returns a map for which PLACE: matches the given place.
int random_map_for_place(const std::string &place, bool want_minivault)
{
    if (place.empty())
        return (-1);

    int mapindex = -1;
    int rollsize = 0;

    for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
    {
        // We also accept tagged levels here.
        if (vdefs[i].place == place
                && vdefs[i].is_minivault() == want_minivault)
        {
            rollsize += vdefs[i].chance;

            if (rollsize && random2(rollsize) < vdefs[i].chance)
                mapindex = i;
        }
    }

#ifdef DEBUG_DIAGNOSTICS
    if (mapindex != -1)
        mprf(MSGCH_DIAGNOSTICS, "Found map %s for %s", 
                vdefs[mapindex].name.c_str(), place.c_str());
#endif

    return (mapindex);
}

int random_map_for_depth(const level_id &place, bool want_minivault)
{
    int mapindex = -1;
    int rollsize = 0;

    for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
        if (vdefs[i].is_minivault() == want_minivault
            && vdefs[i].is_usable_in(place)
            // Tagged levels cannot be selected by depth. This is
            // the only thing preventing Pandemonium demon vaults from
            // showing up in the main dungeon.
            && !vdefs[i].has_tag("entry")
            && !vdefs[i].has_tag("pan"))
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
                || vdefs[i].is_usable_in(level_id::current())))
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
        vdefs[nexist + i].read_index(inf);
        vdefs[nexist + i].set_file(base);
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
    
    for (int i = 0, size = Options.extra_levels.size(); i < size; ++i)
    {
        lc_desfile = datafile_path( Options.extra_levels[i] + ".des", false );
        if (lc_desfile.empty())
            continue;

        parse_maps( lc_desfile );
    }

    // Clean up cached environments.
    dlua.callfn("dgn_flush_map_environments", 0, 0);
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
