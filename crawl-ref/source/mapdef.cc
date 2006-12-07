#include <cstdarg>
#include <cstdio>
#include <cctype>

#include "AppHdr.h"
#include "libutil.h"
#include "mapdef.h"
#include "mon-util.h"
#include "stuff.h"

static const char *map_section_names[] = {
    "",
    "north",
    "south",
    "east",
    "west",
    "northwest",
    "northeast",
    "southwest",
    "southeast",
    "encompass",
    "float",
};

static const char *map_section_name(int msect)
{
    if (msect < 0 || msect >= MAP_NUM_SECTION_TYPES)
        return "";

    return map_section_names[msect];
}

///////////////////////////////////////////////
// level_range
//

level_range::level_range(int s, int d)
    : shallowest(), deepest()
{
    set(s, d);
}

void level_range::set(int s, int d)
{
    shallowest = s;
    deepest    = d;
    if (deepest == -1)
        deepest = shallowest;
}

void level_range::reset()
{
    deepest = shallowest = -1;
}

bool level_range::contains(int x) const
{
    // [ds] The level ranges used by the game are zero-based, adjust for that.
    ++x;
    return (x >= shallowest && x <= deepest);
}

bool level_range::valid() const
{
    return (shallowest > 0 && deepest >= shallowest);
}

int level_range::span() const
{
    return (deepest - shallowest);
}

///////////////////////////////////////////////
// map_lines
//

map_lines::map_lines()
    : lines(), map_width(0), solid_north(false), solid_east(false),
      solid_south(false), solid_west(false), solid_checked(false)
{
}

const std::vector<std::string> &map_lines::get_lines() const
{
    return (lines);
}

void map_lines::add_line(const std::string &s)
{
    lines.push_back(s);
    if ((int) s.length() > map_width)
        map_width = s.length();
}

int map_lines::width() const
{
    return map_width;
}

int map_lines::height() const
{
    return lines.size();
}

int map_lines::glyph(int x, int y) const
{
    return lines[y][x];
}

bool map_lines::is_solid(int gly) const
{
    return (gly == 'x' || gly == 'c' || gly == 'b' || gly == 'v');
}

void map_lines::check_borders()
{
    if (solid_checked)
        return;

    const int wide = width(), high = height();

    solid_north = solid_south = true;
    for (int x = 0; x < wide && (solid_north || solid_south); ++x)
    {
        if (solid_north && !is_solid(glyph(x, 0)))
            solid_north = false;
        if (solid_south && !is_solid(glyph(x, high - 1)))
            solid_south = false;
    }

    solid_east = solid_west = true;
    for (int y = 0; y < high && (solid_east || solid_west); ++y)
    {
        if (solid_west && !is_solid(glyph(0, y)))
            solid_west = false;
        if (solid_east && !is_solid(glyph(wide - 1, y)))
            solid_east = false;
    }

    solid_checked = true;
}

bool map_lines::solid_borders(map_section_type border)
{
    check_borders();
    switch (border)
    {
    case MAP_NORTH: return solid_north;
    case MAP_SOUTH: return solid_south;
    case MAP_EAST:  return solid_east;
    case MAP_WEST:  return solid_west;
    case MAP_NORTHEAST: return solid_north && solid_east;
    case MAP_NORTHWEST: return solid_north && solid_west;
    case MAP_SOUTHEAST: return solid_south && solid_east;
    case MAP_SOUTHWEST: return solid_south && solid_west;
    default: return (false);
    }
}

void map_lines::clear()
{
    lines.clear();
    map_width = 0;
}

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

    solid_checked = false;
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

    solid_checked = false;
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

    solid_checked = false;
}

///////////////////////////////////////////////
// map_def
//

void map_def::init()
{
    name.clear();
    tags.clear();
    place.clear();
    depth.reset();
    orient = MAP_NONE;

    // Base chance; this is not a percentage.
    chance = 10;

    // The map designer must explicitly disallow these if unwanted.
    flags = MAPF_MIRROR_VERTICAL | MAPF_MIRROR_HORIZONTAL
            | MAPF_ROTATE;

    random_symbols.clear();

    map.clear();
    mons.clear();
}

bool map_def::is_minivault() const
{
    return (orient == MAP_NONE);
}

// Tries to dock a floating vault - push it to one edge of the level.
// Docking will only succeed if two contiguous edges are all x/c/b/v
// (other walls prevent docking). If the vault's width is > GXM*2/3,
// it's also eligible for north/south docking, and if the height >
// GYM*2/3, it's eligible for east/west docking. Although docking is
// similar to setting the orientation, it doesn't affect 'orient'.
coord_def map_def::float_dock()
{
    const map_section_type orients[] =
        { MAP_NORTH, MAP_SOUTH, MAP_EAST, MAP_WEST,
          MAP_NORTHEAST, MAP_SOUTHEAST, MAP_NORTHWEST, MAP_SOUTHWEST };
    map_section_type which_orient = MAP_NONE;
    int norients = 0;
    
    for (unsigned i = 0; i < sizeof(orients) / sizeof(*orients); ++i)
    {
        if (map.solid_borders(orients[i]) && can_dock(orients[i])
            && one_chance_in(++norients))
        {
            which_orient = orients[i];
        }
    }

    if (which_orient == MAP_NONE || which_orient == MAP_FLOAT)
        return coord_def(-1, -1);

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Docking floating vault to %s",
         map_section_name(which_orient));
#endif
    return dock_pos(which_orient);
}

coord_def map_def::dock_pos(map_section_type norient) const
{
    const int minborder = 6;

    switch (norient)
    {
    case MAP_NORTH:
        return coord_def( (GXM - map.width()) / 2, minborder );
    case MAP_SOUTH:
        return coord_def( (GXM - map.width()) / 2,
                          GYM - minborder - map.height() );
    case MAP_EAST:
        return coord_def( GXM - minborder - map.width(),
                          (GYM - map.height()) / 2 );
    case MAP_WEST:
        return coord_def( minborder,
                          (GYM - map.height()) / 2 );
    case MAP_NORTHEAST:
        return coord_def( GXM - minborder - map.width(), minborder );
    case MAP_NORTHWEST:
        return coord_def( minborder, minborder );
    case MAP_SOUTHEAST:
        return coord_def( GXM - minborder - map.width(),
                          GYM - minborder - map.height() );
    case MAP_SOUTHWEST:
        return coord_def( minborder,
                          GYM - minborder - map.height() );
    default:
        return coord_def(-1, -1);
    }
}

bool map_def::can_dock(map_section_type norient) const
{
    switch (norient)
    {
    case MAP_NORTH: case MAP_SOUTH:
        return map.width() > GXM * 2 / 3;
    case MAP_EAST: case MAP_WEST:
        return map.height() > GYM * 2 / 3;
    default:
        return (true);
    }
}

coord_def map_def::float_random_place() const
{
    // Try to leave enough around the float for roomification.
    int minhborder = MAPGEN_BORDER + 11,
        minvborder = minhborder;

    if (GXM - 2 * minhborder < map.width())
        minhborder = (GXM - map.width()) / 2 - 1;

    if (GYM - 2 * minvborder < map.height())
        minvborder = (GYM - map.height()) / 2 - 1;
    
    return coord_def(
        random_range(minhborder, GXM - minhborder - map.width()),
        random_range(minvborder, GYM - minvborder - map.height()));
}

coord_def map_def::float_place()
{
    ASSERT(orient == MAP_FLOAT);

    coord_def pos(-1, -1);

    if (coinflip())
        pos = float_dock();

    if (pos.x == -1)
        pos = float_random_place();

    return (pos);
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
    case MAP_NORTHEAST: orient = MAP_SOUTHEAST; break;
    case MAP_NORTHWEST: orient = MAP_SOUTHWEST; break;

    case MAP_SOUTH:     orient = MAP_NORTH; break;
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

void map_def::fixup()
{
    normalise();
    resolve();
}

bool map_def::has_tag(const std::string &tagwanted) const
{
    return !tags.empty() && !tagwanted.empty()
        && tags.find(" " + tagwanted + " ") != std::string::npos;
}

///////////////////////////////////////////////////////////////////
// mons_list
//

mons_list::mons_list(int nids, ...) : mons_ids()
{
    va_list args;
    va_start(args, nids);

    for (int i = 0; i < nids; ++i)
        mons_ids.push_back( va_arg(args, int) );

    va_end(args);
}

mons_list::mons_list() : mons_ids()
{
}

const std::vector<int> &mons_list::get_ids() const
{
    return (mons_ids);
}

void mons_list::clear()
{
    mons_ids.clear();
}

bool mons_list::add_mons(const std::string &s)
{
    const int mid = mons_by_name(s);
    if (mid != MONS_PROGRAM_BUG)
        mons_ids.push_back(mid);
    return (mid != MONS_PROGRAM_BUG);
}

int mons_list::mons_by_name(std::string name) const
{
    lowercase(name);

    name = replace_all( name, "_", " " );

    // Special casery:
    if (name == "pandemonium demon")
        return (MONS_PANDEMONIUM_DEMON);

    if (name == "random" || name == "random monster")
        return (RANDOM_MONSTER);

    if (name == "any demon" || name == "demon" || name == "random demon")
        return (-100 - DEMON_RANDOM);

    if (name == "any lesser demon" 
            || name == "lesser demon" || name == "random lesser demon")
        return (-100 - DEMON_LESSER);

    if (name == "any common demon" 
            || name == "common demon" || name == "random common demon")
        return (-100 - DEMON_COMMON);

    if (name == "any greater demon" 
            || name == "greater demon" || name == "random greater demon")
        return (-100 - DEMON_GREATER);

    if (name == "small zombie")
        return (MONS_ZOMBIE_SMALL);
    if (name == "large zombie")
        return (MONS_ZOMBIE_LARGE);

    if (name == "small skeleton")
        return (MONS_SKELETON_SMALL);
    if (name == "large skeleton")
        return (MONS_SKELETON_LARGE);

    if (name == "spectral thing")
        return (MONS_SPECTRAL_THING);

    if (name == "small simulacrum")
        return (MONS_SIMULACRUM_SMALL);
    if (name == "large simulacrum")
        return (MONS_SIMULACRUM_LARGE);

    if (name == "small abomination")
        return (MONS_ABOMINATION_SMALL);

    if (name == "large abomination")
        return (MONS_ABOMINATION_LARGE);

    return (get_monster_by_name(name, true));
}
