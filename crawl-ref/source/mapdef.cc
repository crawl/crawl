#include "mapdef.h"
#include <cstdarg>
#include <cstdio>
#include <cctype>

#define esc(s) escape_string(s, "\"", "\\")

std::string escape_string(std::string s, 
                          const std::string &toesc, 
                          const std::string &escwith)
{
    std::string::size_type start = 0;
    std::string::size_type found;
   
    while ((found = s.find_first_of(toesc, start)) != std::string::npos)
    {
        const char c = s[found];
        s.replace( found, 1, escwith + c );
        start = found + 1 + escwith.length();
    }

    return (s);
}

std::string clean_string(std::string s, const std::string &tokill)
{
    std::string::size_type found;
    while ((found = s.find_first_of(tokill)) != std::string::npos)
        s.erase(found, 1);
    return (s);
}

// [dshaligram] Use sprintf to compile on DOS without having to pull in
// libutil.cc. The buffers used should be more than sufficient.
static std::string to_s(int num)
{
    char buf[100];
    sprintf(buf, "%d", num);
    return (buf);
}

static std::string to_sl(long num)
{
    char buf[100];
    sprintf(buf, "%ld", num);
    return (buf);
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

std::string map_lines::get_initialiser() const
{
    std::string init = "  map_lines(" + to_s(lines.size()) + ",\n";
    for (int i = 0, size = lines.size(); i < size; ++i)
    {
        init += "    \"";
        init += esc(lines[i]);
        init += "\"";
        if (i < size - 1)
        {
            init += ",";
            init += "\n";
        }
    }
    init += ")";
    return (init);
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

std::string map_def::get_initialiser() const
{
    std::string sinit = "{\n";

    sinit += "  \"" + esc(name) + "\",\n";
    sinit += "  \"" + esc(tags) + "\",\n";
    sinit += "  \"" + esc(place) + "\",\n";

    sinit += "  level_range(" 
            + to_s(depth.shallowest) 
            + ", "
            + to_s(depth.deepest)
            + "),\n";

    sinit += "  (map_section_type) " + to_s(orient) + ",\n";

    sinit += "  // Map generation probability weight\n";
    sinit += "  " + to_s(chance) + ",\n";

    sinit += "  // Map flags\n";
    sinit += "  " + to_sl(flags) + "L,\n";
    sinit += map.get_initialiser() + ",\n";
    sinit += mons.get_initialiser() + ",\n";
    sinit += "  \"" + esc(random_symbols) + "\"\n";
    sinit += "},\n";

    return (sinit);
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

std::string mons_list::get_initialiser() const
{
    std::string init = "  mons_list(" + to_s(mons_names.size());
    if (mons_names.size())
        init += ",\n";
    for (int i = 0, size = mons_names.size(); i < size; ++i)
    {
        init += "    ";
        std::string can = canonical(mons_names[i]);
        if (can == "MONS_RANDOM" || can == "MONS_RANDOM_MONSTER")
            can = "RANDOM_MONSTER";

        if (can == "MONS_ANY_DEMON")
            can = "-100 - DEMON_RANDOM";
        else if (can.find("MONS_ANY_DEMON") == 0)
            can = "-100 - " + can.substr(9);

        init += can;
        if (i < size - 1)
            init += ",\n";
    }
    init += ")";

    return (init);
}

std::string mons_list::canonical(std::string name) const
{
    for (int i = 0, len = name.length(); i < len; ++i)
    {
        name[i] = toupper(name[i]);
        if (name[i] == ' ')
            name[i] = '_';
    }
    return "MONS_" + name;
}
