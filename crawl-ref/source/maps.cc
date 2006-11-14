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

#include <string.h>
#include <stdlib.h>

#include "dungeon.h"
#include "enum.h"
#include "monplace.h"
#include "mapdef.h"
#include "stuff.h"

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

static map_def vdefs[] = {
#include "mapdefs.ixx"
};

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
    map.normalise();
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

    for (unsigned i = 0; i < sizeof(vdefs) / sizeof(*vdefs); ++i)
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
    for (unsigned i = 0; i < sizeof(vdefs) / sizeof(*vdefs); ++i)
        if (vdefs[i].name == name)
            return (i);

    return (-1);
}

int random_map_for_depth(int depth, bool want_minivault)
{
    int mapindex = -1;
    int rollsize = 0;

    for (unsigned i = 0; i < sizeof(vdefs) / sizeof(*vdefs); ++i)
        if (vdefs[i].depth.contains(depth) 
                // Tagged levels MUST be selected by tag!
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

    for (unsigned i = 0; i < sizeof(vdefs) / sizeof(*vdefs); ++i)
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
// map_def
// map_def functions that can't be implemented in mapdef.cc because that'll pull
// in functions from lots of other places and make compiling levcomp
// unnecessarily painful

bool map_def::is_minivault() const
{
    return (orient == MAP_NONE);
}

void map_def::hmirror()
{
    if (!(flags & MAPF_MIRROR_HORIZONTAL))
        return;

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Mirroring %s horizontally.", name.c_str());
#endif
    map.hmirror();

    switch (orient)
    {
    case MAP_EAST:      orient = MAP_WEST; break;
    case MAP_NORTHEAST: orient = MAP_NORTHWEST; break;
    case MAP_SOUTHEAST: orient = MAP_SOUTHWEST; break;
    case MAP_WEST:      orient = MAP_EAST; break;
    case MAP_NORTHWEST: orient = MAP_NORTHEAST; break;
    case MAP_SOUTHWEST: orient = MAP_SOUTHEAST; break;
    default: break;
    }
}

void map_def::vmirror()
{
    if (!(flags & MAPF_MIRROR_VERTICAL))
        return;

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Mirroring %s vertically.", name.c_str());
#endif
    map.vmirror();

    switch (orient)
    {
    case MAP_NORTH:     orient = MAP_SOUTH; break;
    case MAP_NORTH_DIS: orient = MAP_SOUTH_DIS; break;
    case MAP_NORTHEAST: orient = MAP_SOUTHEAST; break;
    case MAP_NORTHWEST: orient = MAP_SOUTHWEST; break;

    case MAP_SOUTH:     orient = MAP_NORTH; break;
    case MAP_SOUTH_DIS: orient = MAP_NORTH_DIS; break;
    case MAP_SOUTHEAST: orient = MAP_NORTHEAST; break;
    case MAP_SOUTHWEST: orient = MAP_NORTHWEST; break;
    default: break;
    }
}

void map_def::rotate(bool clock)
{
    if (!(flags & MAPF_ROTATE))
        return;

#define GMINM ((GXM) < (GYM)? (GXM) : (GYM))
    // Make sure the largest dimension fits in the smaller map bound.
    if (map.width() <= GMINM && map.height() <= GMINM)
    {
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Rotating %s %sclockwise.",
                name.c_str(),
                !clock? "anti-" : "");
#endif
        map.rotate(clock);

        // Orientation shifts for clockwise rotation:
        const map_section_type clockrotate_orients[][2] = {
            { MAP_NORTH,        MAP_EAST        },
            { MAP_NORTHEAST,    MAP_SOUTHEAST   },
            { MAP_EAST,         MAP_SOUTH       },
            { MAP_SOUTHEAST,    MAP_SOUTHWEST   },
            { MAP_SOUTH,        MAP_WEST        },
            { MAP_SOUTHWEST,    MAP_NORTHWEST   },
            { MAP_WEST,         MAP_NORTH       },
            { MAP_NORTHWEST,    MAP_NORTHEAST   },
        };
        const int nrots = sizeof(clockrotate_orients) 
                            / sizeof(*clockrotate_orients);

        const int refindex = !clock;
        for (int i = 0; i < nrots; ++i)
            if (orient == clockrotate_orients[i][refindex])
            {
                orient = clockrotate_orients[i][!refindex];
                break;
            }
    }
}

void map_def::normalise()
{
    // Minivaults are padded out with floor tiles, normal maps are
    // padded out with rock walls.
    map.normalise(is_minivault()? '.' : 'x');
}

void map_def::resolve()
{
    map.resolve( random_symbols );
}

bool map_def::has_tag(const std::string &tagwanted) const
{
    return !tags.empty() && !tagwanted.empty()
        && tags.find(" " + tagwanted + " ") != std::string::npos;
}

//////////////////////////////////////////////////////////////////
// map_lines

void map_lines::resolve(std::string &s, const std::string &fill)
{
    std::string::size_type pos;
    while ((pos = s.find('?')) != std::string::npos)
        s[pos] = fill[ random2(fill.length()) ];
}

void map_lines::resolve(const std::string &fillins)
{
    if (fillins.empty() || fillins.find('?') != std::string::npos)
        return;

    for (int i = 0, size = lines.size(); i < size; ++i)
        resolve(lines[i], fillins);
}

void map_lines::normalise(char fillch)
{
    for (int i = 0, size = lines.size(); i < size; ++i)
    {
        std::string &s = lines[i];
        if ((int) s.length() < map_width)
            s += std::string( map_width - s.length(), fillch );
    }
}

// Should never be attempted if the map has a defined orientation, or if one
// of the dimensions is greater than the lesser of GXM,GYM.
void map_lines::rotate(bool clockwise)
{
    std::vector<std::string> newlines;

    // normalise() first for convenience.
    normalise();

    const int xs = clockwise? 0 : map_width - 1,
              xe = clockwise? map_width : -1,
              xi = clockwise? 1 : -1;

    const int ys = clockwise? (int) lines.size() - 1 : 0,
              ye = clockwise? -1 : (int) lines.size(),
              yi = clockwise? -1 : 1;

    for (int i = xs; i != xe; i += xi)
    {
        std::string line;

        for (int j = ys; j != ye; j += yi)
            line += lines[j][i];

        newlines.push_back(line);
    }

    map_width = lines.size();
    lines = newlines;
}

void map_lines::vmirror()
{
    const int size = lines.size();
    const int midpoint = size / 2;

    for (int i = 0; i < midpoint; ++i)
    {
        std::string temp = lines[i];
        lines[i] = lines[size - 1 - i];
        lines[size - 1 - i] = temp;
    }
}

void map_lines::hmirror()
{
    const int midpoint = map_width / 2;
    for (int i = 0, size = lines.size(); i < size; ++i)
    {
        std::string &s = lines[i];
        for (int j = 0; j < midpoint; ++j)
        {
            int c = s[j];
            s[j] = s[map_width - 1 - j];
            s[map_width - 1 - j] = c;
        }
    }
}
