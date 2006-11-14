#include <cstdarg>
#include <cstdio>
#include <cctype>

#include "AppHdr.h"
#include "libutil.h"
#include "mapdef.h"
#include "mon-util.h"
#include "stuff.h"

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

map_lines::map_lines() : lines(), map_width(0)
{
}

map_lines::map_lines(int nlines, ...) : lines(), map_width(0)
{
    va_list args;
    va_start(args, nlines);

    for (int i = 0; i < nlines; ++i)
        add_line( va_arg(args, const char *) );

    va_end(args);
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

void map_def::fixup()
{
    normalise();
    resolve();

    mons.resolve();
}

bool map_def::has_tag(const std::string &tagwanted) const
{
    return !tags.empty() && !tagwanted.empty()
        && tags.find(" " + tagwanted + " ") != std::string::npos;
}

///////////////////////////////////////////////////////////////////
// mons_list
//

mons_list::mons_list(int nids, ...) : mons_names(), mons_ids()
{
    va_list args;
    va_start(args, nids);

    for (int i = 0; i < nids; ++i)
        mons_ids.push_back( va_arg(args, int) );

    va_end(args);
}

mons_list::mons_list() : mons_names(), mons_ids()
{
}

const std::vector<int> &mons_list::get_ids() const
{
    return (mons_ids);
}

void mons_list::clear()
{
    mons_names.clear();
    mons_ids.clear();
}

void mons_list::add_mons(const std::string &s)
{
    mons_names.push_back(s);
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

    if (name == "any demon lesser" || name == "any lesser demon"
            || name == "lesser demon" || name == "random lesser demon")
        return (-100 - DEMON_LESSER);

    if (name == "any demon common" || name == "any common demon"
            || name == "common demon" || name == "random common demon")
        return (-100 - DEMON_COMMON);

    if (name == "any demon greater" || name == "any greater demon"
            || name == "greater demon" || name == "random greater demon")
        return (-100 - DEMON_GREATER);

    if (name == "zombie small" || name == "small zombie")
        return (MONS_ZOMBIE_SMALL);
    if (name == "zombie large" || name == "large zombie")
        return (MONS_ZOMBIE_LARGE);

    if (name == "skeleton small" || name == "small skeleton")
        return (MONS_SKELETON_SMALL);
    if (name == "skeleton large" || name == "large skeleton")
        return (MONS_SKELETON_LARGE);

    if (name == "spectral thing")
        return (MONS_SPECTRAL_THING);

    if (name == "simulacrum small" || name == "small simulacrum")
        return (MONS_SIMULACRUM_SMALL);
    if (name == "simulacrum large" || name == "large simulacrum")
        return (MONS_SIMULACRUM_LARGE);

    if (name == "abomination small" || name == "small abomination")
        return (MONS_ABOMINATION_SMALL);

    if (name == "abomination large" || name == "large abomination")
        return (MONS_ABOMINATION_LARGE);

    return (get_monster_by_name(name, true));
}

void mons_list::resolve()
{
    for (int i = 0, size = mons_names.size(); i < size; ++i)
    {
        int mons = mons_by_name(mons_names[i]);
        if (mons == MONS_PROGRAM_BUG)
        {
            fprintf(stderr, "Unknown monster: '%s'\n", mons_names[i].c_str());
            exit(1);
        }

        mons_ids.push_back( mons );
    }

    mons_names.clear();
}
