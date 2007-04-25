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

#include "dungeon.h"
#include "enum.h"
#include "files.h"
#include "monplace.h"
#include "mapdef.h"
#include "misc.h"
#include "stuff.h"

#include "levcomp.h"

static int write_vault(const map_def &mdef, map_type mt, 
                       vault_placement &,
                       std::vector<vault_placement> *);
static int apply_vault_definition(
                        map_def &def,
                        map_type map,
                        vault_placement &,
                        std::vector<vault_placement> *);

static void resolve_map(map_def &def);


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

    // NB - a return value of zero is not handled well by dungeon.cc (but there it is) 10mar2000 {dlb}
    return write_vault( vdefs[which_vault], vgrid, place, avoid );
}          // end vault_main()

static int write_vault(const map_def &mdef, map_type map, 
                       vault_placement &place,
                       std::vector<vault_placement> *avoid)
{
    // Copy the map so we can monkey with it.
    place.map = mdef;

    // Try so many times to place the map. This will always succeed
    // unless there are conflicting map placements in 'avoid'.
    int tries = 10;
    do
        resolve_map(place.map);
    while ((place.orient =
            apply_vault_definition(place.map, map, place, avoid)) == MAP_NONE
           && tries-- > 0);

    return (place.orient);
}

// Mirror the map if appropriate, resolve substitutable symbols (?),
static void resolve_map(map_def &map)
{
    map.resolve();
    
    // Mirroring is possible for any map that does not explicitly forbid it.
    // Note that mirroring also flips the orientation.
    if (coinflip())
        map.hmirror();

    if (coinflip())
        map.vmirror();

    // The map may also refuse to be rotated.
    if (coinflip())
        map.rotate( coinflip() );
}

static bool is_grid_clobbered(int sx, int sy, int width, int height)
{
    for (int y = sy; y < sy + height; ++y)
    {
        for (int x = sx; x < sx + width; ++x)
        {
            int grid = grd[x][y];

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

int random_map_for_depth(int depth, bool want_minivault)
{
    int mapindex = -1;
    int rollsize = 0;

    for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
        if (vdefs[i].depth.contains(depth) 
            // Tagged levels cannot be selected by depth. This is
            // the only thing preventing Pandemonium demon vaults from
            // showing up in the main dungeon.
            && !vdefs[i].has_tag("entry")
            && !vdefs[i].has_tag("pan")
            && vdefs[i].is_minivault() == want_minivault)
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
            && (!check_depth || !vdefs[i].depth.valid()
                || vdefs[i].depth.contains(you.your_level)))
        {
            rollsize += vdefs[i].chance;

            if (rollsize && random2(rollsize) < vdefs[i].chance)
                mapindex = i;
        }
    }

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

static void parse_maps(const std::string &s)
{
    FILE *dat = fopen(s.c_str(), "r");
    if (!dat)
        end(1, true, "Failed to open %s for reading", s.c_str());

    reset_map_parser();

    extern int yyparse(void);
    extern FILE *yyin;
    yyin = dat;

    yyparse();
    fclose(dat);
}

void read_maps()
{
    parse_maps( lc_desfile = datafile_path( "splev.des" ) );
    parse_maps( lc_desfile = datafile_path( "vaults.des" ) );

    for (int i = 0, size = Options.extra_levels.size(); i < size; ++i)
    {
        lc_desfile = datafile_path( Options.extra_levels[i] + ".des", false );
        if (lc_desfile.empty())
            continue;

        parse_maps( lc_desfile );
    }
}

void add_parsed_map( const map_def &md )
{
    map_def map = md;

    map.fixup();
    vdefs.push_back( map );
}
