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
#include "stuff.h"

#include "levcomp.h"

static int write_vault(const map_def &mdef, map_type mt, 
                        FixedVector<int,7> &marray,
                        vault_placement &);
static int apply_vault_definition(
                        const map_def &def,
                        map_type map,
                        FixedVector<int,7> &marray,
                        vault_placement &);

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
        FixedVector<int, 7>& mons_array, 
        vault_placement &place,
        int which_vault, 
        int many_many )
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
    return write_vault( vdefs[which_vault], vgrid, mons_array, place );
}          // end vault_main()

static int write_vault(const map_def &mdef, map_type map, 
                       FixedVector<int, 7> &marray,
                       vault_placement &place)
{
    place.map = &mdef;

    // Copy the map so we can monkey with it.
    map_def def = mdef;
    resolve_map(def);

    return apply_vault_definition(def, map, marray, place);
}

// Mirror the map if appropriate, resolve substitutable symbols (?),
static void resolve_map(map_def &map)
{
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

static void apply_monsters(
        const map_def &def,
        FixedVector<int,7> &marray)
{
    const std::vector<int> &mids = def.mons.get_ids();
    size_t ndx = 0;
    for (int i = 0, size = mids.size(); i < size; ++i)
    {
        int mid = mids[i];
        if (mid < 0)
        {
            mid = -100 - mid;
            if (mid == DEMON_RANDOM)
                mid = random2(DEMON_RANDOM);
            mid = summon_any_demon( mid );
        }

        if (ndx < marray.size())
            marray[ndx++] = mid;
    }
}

static void apply_vault_grid(const map_def &def, map_type map, 
                             vault_placement &place)
{
    const map_lines &ml = def.map;
    const int orient = def.orient;
    const int width = ml.width();
    const int height = ml.height();

    int startx = 0, starty = 0;

    if (orient == MAP_SOUTH || orient == MAP_SOUTHEAST
            || orient == MAP_SOUTHWEST || orient == MAP_SOUTH_DIS)
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
}

static int apply_vault_definition(
        const map_def &def,
        map_type map,
        FixedVector<int,7> &marray,
        vault_placement &place)
{
    apply_monsters(def, marray);
    apply_vault_grid(def, map, place);

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

int find_map_named(const std::string &name)
{
    if (name.empty())
        return (-1);

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Looking for map named \"%s\"", name.c_str());
#endif
    for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
        if (vdefs[i].name == name)
            return (i);

    return (-1);
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
                && vdefs[i].tags.empty()
                && vdefs[i].is_minivault() == want_minivault)
        {
            rollsize += vdefs[i].chance;

            if (rollsize && random2(rollsize) < vdefs[i].chance)
                mapindex = i;
        }

    return (mapindex);
}

int random_map_for_tag(const std::string &tag)
{
    int mapindex = -1;
    int rollsize = 0;

    for (unsigned i = 0, size = vdefs.size(); i < size; ++i)
    {
        if (vdefs[i].has_tag(tag))
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
    else
        mprf(MSGCH_DIAGNOSTICS, "No map for tag '%s'", tag.c_str());
#endif

    return (mapindex);
}

/////////////////////////////////////////////////////////////////////////////
// Reading maps from .des files.

static void parse_maps(const std::string &s)
{
    reset_map_parser();

    FILE *dat = fopen(s.c_str(), "r");
    if (!dat)
    {
        fprintf(stderr, "Failed to open %s for reading: %d\n", s.c_str(),
                errno);
        exit(1);
    }

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
}

void add_parsed_map( const map_def &md )
{
    map_def map = md;

    map.fixup();
    vdefs.push_back( map );
}

//////////////////////////////////////////////////////////////////
// map_lines


