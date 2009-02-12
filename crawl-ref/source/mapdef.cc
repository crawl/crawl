/*
 *  File:       mapdef.cc
 *  Summary:    Support code for Crawl des files.
 *  Created by: dshaligram on Wed Nov 22 08:41:20 2006 UTC
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include <iostream>
#include <cstdarg>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <algorithm>

#include "branch.h"
#include "describe.h"
#include "directn.h"
#include "dungeon.h"
#include "files.h"
#include "initfile.h"
#include "invent.h"
#include "items.h"
#include "libutil.h"
#include "mapdef.h"
#include "mapmark.h"
#include "maps.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "mon-util.h"
#include "place.h"
#include "randart.h"
#include "stuff.h"
#include "tags.h"

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

// atoi that rejects strings containing non-numeric trailing characters.
// returns defval for invalid input.
template <typename V>
static V strict_aton(const char *s, V defval = 0)
{
    char *end;
    const V res = strtol(s, &end, 10);
    return (!*s || *end) ? defval : res;
}

const char *map_section_name(int msect)
{
    if (msect < 0 || msect >= MAP_NUM_SECTION_TYPES)
        return "";

    return map_section_names[msect];
}

static int find_weight(std::string &s, int defweight = TAG_UNFOUND)
{
    int weight = strip_number_tag(s, "weight:");
    if (weight == TAG_UNFOUND)
        weight = strip_number_tag(s, "w:");
    return (weight == TAG_UNFOUND? defweight : weight);
}

std::string mapdef_split_key_item(const std::string &s,
                                  std::string *key,
                                  int *separator,
                                  std::string *arg,
                                  int key_max_len)
{
    std::string::size_type
        norm = s.find("=", 1),
        fixe = s.find(":", 1);

    const std::string::size_type sep = norm < fixe? norm : fixe;
    if (sep == std::string::npos)
        return ("malformed declaration - must use = or :");

    *key = trimmed_string(s.substr(0, sep));
    std::string substitute    = trimmed_string(s.substr(sep + 1));

    if (key->empty() ||
        (key_max_len != -1 && (int) key->length() > key_max_len))
    {
        return make_stringf(
            "selector '%s' must be <= %d characters in '%s'",
            key->c_str(), key_max_len, s.c_str());
    }

    if (substitute.empty())
        return make_stringf("no substitute defined in '%s'",
                            s.c_str());

    *arg = substitute;
    *separator = s[sep];

    return ("");
}

///////////////////////////////////////////////
// level_range
//

level_range::level_range(branch_type br, int s, int d)
    : level_type(LEVEL_DUNGEON), branch(br), shallowest(),
      deepest(), deny(false)
{
    set(s, d);
}

level_range::level_range(const raw_range &r)
    : branch(r.branch), shallowest(r.shallowest), deepest(r.deepest),
      deny(r.deny)
{
}

void level_range::write(writer& outf) const
{
    marshallShort(outf, level_type);
    marshallShort(outf, branch);
    marshallShort(outf, shallowest);
    marshallShort(outf, deepest);
    marshallByte(outf, deny);
}

void level_range::read(reader& inf)
{
    level_type = static_cast<level_area_type>( unmarshallShort(inf) );
    branch     = static_cast<branch_type>( unmarshallShort(inf) );
    shallowest = unmarshallShort(inf);
    deepest    = unmarshallShort(inf);
    deny       = unmarshallByte(inf);
}

std::string level_range::str_depth_range() const
{
    if (shallowest == -1)
        return (":??");

    if (deepest >= branches[branch].depth)
        return (shallowest == 1? "" : make_stringf("%d-", shallowest));

    if (shallowest == deepest)
        return make_stringf(":%d", shallowest);

    return make_stringf(":%d-%d", shallowest, deepest);
}

std::string level_range::describe() const
{
    return make_stringf("%s%s%s",
                        deny? "!" : "",
                        branch == NUM_BRANCHES? "Any" :
                        branches[branch].abbrevname,
                        str_depth_range().c_str());
}

level_range::operator raw_range () const
{
    raw_range r;
    r.branch     = branch;
    r.shallowest = shallowest;
    r.deepest    = deepest;
    r.deny       = deny;
    return (r);
}

void level_range::set(const std::string &br, int s, int d)
    throw (std::string)
{
    if (br == "any" || br == "Any")
        branch = NUM_BRANCHES;
    else if ((branch = str_to_branch(br)) == NUM_BRANCHES
             && (level_type = str_to_level_area_type(br)) == LEVEL_DUNGEON)
        throw make_stringf("Unknown branch: '%s'", br.c_str());

    shallowest = s;
    deepest    = d;

    if (deepest < shallowest)
        throw make_stringf("Level-range %s:%d-%d is malformed",
                           br.c_str(), s, d);
}

level_range level_range::parse(std::string s) throw (std::string)
{
    level_range lr;
    trim_string(s);

    if (s[0] == '!')
    {
        lr.deny = true;
        s = trimmed_string(s.substr(1));
    }

    std::string::size_type cpos = s.find(':');
    if (cpos == std::string::npos)
        parse_partial(lr, s);
    else
    {
        std::string branch = trimmed_string(s.substr(0, cpos));
        std::string depth  = trimmed_string(s.substr(cpos + 1));
        parse_depth_range(depth, &lr.shallowest, &lr.deepest);

        lr.set(branch, lr.shallowest, lr.deepest);
    }

    return (lr);
}

void level_range::parse_partial(level_range &lr, const std::string &s)
    throw (std::string)
{
    if (isdigit(s[0]))
    {
        lr.branch = NUM_BRANCHES;
        parse_depth_range(s, &lr.shallowest, &lr.deepest);
    }
    else
        lr.set(s, 1, 100);
}

void level_range::parse_depth_range(const std::string &s, int *l, int *h)
    throw (std::string)
{
    if (s == "*")
    {
        *l = 1;
        *h = 100;
        return;
    }

    std::string::size_type hy = s.find('-');
    if (hy == std::string::npos)
    {
        *l = *h = strict_aton<int>(s.c_str());
        if (!*l)
            throw std::string("Bad depth: ") + s;
    }
    else
    {
        *l = strict_aton<int>(s.substr(0, hy).c_str());

        std::string tail = s.substr(hy + 1);
        if (tail.empty())
            *h = 100;
        else
            *h = strict_aton<int>(tail.c_str());

        if (!*l || !*h || *l > *h)
            throw std::string("Bad depth: ") + s;
    }
}

void level_range::set(int s, int d)
{
    shallowest = s;
    deepest    = d;
    if (deepest == -1 || deepest < shallowest)
        deepest = shallowest;
}

void level_range::reset()
{
    deepest = shallowest = -1;
    level_type = LEVEL_DUNGEON;
}

bool level_range::matches(const level_id &lid) const
{
    // Level types must always match.
    if (lid.level_type != level_type)
        return (false);

    if (lid.level_type != LEVEL_DUNGEON)
        return (true);

    if (branch == NUM_BRANCHES)
        return (matches(absdungeon_depth(lid.branch, lid.depth)));
    else
        return (branch == lid.branch
                && lid.depth >= shallowest && lid.depth <= deepest);
}

bool level_range::matches(int x) const
{
    // [ds] The level ranges used by the game are zero-based, adjust for that.
    ++x;
    return (x >= shallowest && x <= deepest);
}

bool level_range::operator == (const level_range &lr) const
{
    return (deny == lr.deny && level_type == lr.level_type
            && (level_type != LEVEL_DUNGEON
                || (shallowest == lr.shallowest
                    && deepest == lr.deepest
                    && branch == lr.branch)));
}

bool level_range::valid() const
{
    return (shallowest > 0 && deepest >= shallowest);
}

int level_range::span() const
{
    return (deepest - shallowest);
}

////////////////////////////////////////////////////////////////////////
// map_transformer

map_transformer::~map_transformer()
{
}

////////////////////////////////////////////////////////////////////////
// map_lines

map_lines::map_lines()
    : transforms(), markers(), lines(), overlay(),
      map_width(0), solid_north(false), solid_east(false),
      solid_south(false), solid_west(false), solid_checked(false)
{
}

map_lines::map_lines(const map_lines &map)
{
    init_from(map);
}

int map_lines::operator () (const coord_def &c) const
{
    return lines[c.y][c.x];
}

bool map_lines::in_map(const coord_def &c) const
{
    return (lines[c.y][c.x] != ' ');
}

map_lines &map_lines::operator = (const map_lines &map)
{
    if (this != &map)
        init_from(map);
    return (*this);
}

map_lines::~map_lines()
{
    clear_transforms();
    clear_markers();
}

void map_lines::init_from(const map_lines &map)
{
    // Transforms and markers have to be regenerated, they will not be copied.
    clear_transforms();
    clear_markers();
    overlay.reset(NULL);
    lines         = map.lines;
    map_width     = map.map_width;
    solid_north   = map.solid_north;
    solid_east    = map.solid_east;
    solid_south   = map.solid_south;
    solid_west    = map.solid_west;
    solid_checked = map.solid_checked;
}

template <typename V>
void map_lines::clear_vector(V &vect)
{
    for (int i = 0, vsize = vect.size(); i < vsize; ++i)
        delete vect[i];
    vect.clear();
}

void map_lines::clear_transforms()
{
    clear_vector(transforms);
}

void map_lines::clear_markers()
{
    clear_vector(markers);
}

void map_lines::add_marker(map_marker *marker)
{
    markers.push_back(marker);
}

std::string map_lines::add_feature_marker(const std::string &s)
{
    std::string key, arg;
    int sep = 0;
    std::string err = mapdef_split_key_item(s, &key, &sep, &arg);
    if (!err.empty())
        return (err);

    transforms.push_back(new map_marker_spec(key[0], arg));
    return ("");
}

std::string map_lines::add_lua_marker(const std::string &key,
                                      const lua_datum &function)
{
    transforms.push_back(new map_marker_spec(key[0], function));
    return ("");
}

void map_lines::apply_markers(const coord_def &c)
{
    for (int i = 0, vsize = markers.size(); i < vsize; ++i)
    {
        markers[i]->pos += c;
        env.markers.add(markers[i]);
    }
    // *not* clear_markers() since we've offloaded marker ownership to
    // the crawl env.
    markers.clear();
}

void map_lines::apply_grid_overlay(const coord_def &c)
{
    if (!overlay.get())
        return;
    for (int y = height() - 1; y >= 0; --y)
        for (int x = width() - 1; x >= 0; --x)
        {
            coord_def gc(c.x + x, c.y + y);

            const int colour = (*overlay)(x, y).colour;
            if (colour)
                dgn_set_grid_colour_at(gc, colour);
#ifdef USE_TILE
            const int floor = (*overlay)(x, y).floortile;
            if (floor)
            {
                int offset = random2(tile_dngn_count(floor));
                env.tile_flv(gc).floor = floor + offset;
            }
            const int rock = (*overlay)(x, y).rocktile;
            if (rock)
            {
                int offset = random2(tile_dngn_count(rock));
                env.tile_flv(gc).wall = rock + offset;
            }
#endif
        }
}

void map_lines::apply_overlays(const coord_def &c)
{
    apply_markers(c);
    apply_grid_overlay(c);
}

const std::vector<std::string> &map_lines::get_lines() const
{
    return (lines);
}

std::vector<std::string> &map_lines::get_lines()
{
    return (lines);
}

void map_lines::add_line(const std::string &s)
{
    lines.push_back(s);
    if (static_cast<int>(s.length()) > map_width)
        map_width = s.length();
}

std::string map_lines::clean_shuffle(std::string s)
{
    return replace_all_of(s, " \t", "");
}

std::string map_lines::check_block_shuffle(const std::string &s)
{
    const std::vector<std::string> segs = split_string("/", s);
    const unsigned seglen = segs[0].length();

    for (int i = 1, vsize = segs.size(); i < vsize; ++i)
    {
        if (seglen != segs[i].length())
            return ("block shuffle segment length mismatch");
    }

    return ("");
}

std::string map_lines::check_shuffle(std::string &s)
{
    if (s.find(',') != std::string::npos)
        return ("use / for block shuffle, or multiple SHUFFLE: lines");

    s = clean_shuffle(s);

    if (s.find('/') != std::string::npos)
        return check_block_shuffle(s);

    return ("");
}

std::string map_lines::parse_glyph_replacements(std::string s,
                                                glyph_replacements_t &gly)
{
    s = replace_all_of(s, "\t", " ");
    std::vector<std::string> segs = split_string(" ", s);

    for (int i = 0, vsize = segs.size(); i < vsize; ++i)
    {
        const std::string &is = segs[i];
        if (is.length() > 2 && is[1] == ':')
        {
            const int glych = is[0];
            int weight = atoi( is.substr(2).c_str() );
            if (weight < 1)
                weight = 10;

            gly.push_back( glyph_weighted_replacement_t(glych, weight) );
        }
        else
        {
            for (int c = 0, cs = is.length(); c < cs; ++c)
                gly.push_back( glyph_weighted_replacement_t(is[c], 10) );
        }
    }

    return ("");
}

template<class T>
std::string map_lines::parse_weighted_str(const std::string &spec, T &list)
{
    std::vector<std::string> speclist = split_string("/", spec);
    for (int i = 0, vsize = speclist.size(); i < vsize; ++i)
    {
        std::string val = speclist[i];
        lowercase(val);

        int weight = find_weight(val);
        if (weight == TAG_UNFOUND)
        {
            weight = 10;
            // :number suffix?
            std::string::size_type cpos = val.find(':');
            if (cpos != std::string::npos)
            {
                weight = atoi(val.substr(cpos + 1).c_str());
                if (weight <= 0)
                    weight = 10;
                val.erase(cpos);
                trim_string(val);
            }
        }

        if (!list.parse(val, weight))
        {
            return make_stringf("bad spec: '%s' in '%s'",
                                val.c_str(), spec.c_str());
        }
    }
    return ("");
}

bool map_colour_list::parse(const std::string &col, int weight)
{
    const int colour = col == "none" ? BLACK : str_to_colour(col, -1);
    if (colour == -1)
        return false;

    push_back(map_weighted_colour(colour, weight));
    return true;
}

std::string map_lines::add_colour(const std::string &sub)
{
    std::string s = trimmed_string(sub);

    if (s.empty())
        return ("");

    int sep = 0;
    std::string key;
    std::string substitute;

    std::string err = mapdef_split_key_item(sub, &key, &sep, &substitute);
    if (!err.empty())
        return (err);

    map_colour_list colours;
    err = parse_weighted_str<map_colour_list>(substitute, colours);
    if (!err.empty())
        return (err);

    transforms.push_back( new colour_spec( key[0], sep == ':', colours ) );
    return ("");
}

std::string map_lines::add_subst(const std::string &sub)
{
    std::string s = trimmed_string(sub);

    if (s.empty())
        return ("");

    int sep = 0;
    std::string key;
    std::string substitute;

    std::string err = mapdef_split_key_item(sub, &key, &sep, &substitute);
    if (!err.empty())
        return (err);

    glyph_replacements_t repl;
    err = parse_glyph_replacements(substitute, repl);
    if (!err.empty())
        return (err);

    transforms.push_back( new subst_spec( key[0], sep == ':', repl ) );

    return ("");
}

std::string map_lines::parse_nsubst_spec(const std::string &s,
                                         subst_spec &spec)
{
    std::string key, arg;
    int sep;
    std::string err = mapdef_split_key_item(s, &key, &sep, &arg, -1);
    if (!err.empty())
        return err;
    const int keyval = key == "*"? -1 : atoi(key.c_str());
    if (!keyval)
        return make_stringf("Illegal spec: %s", s.c_str());

    glyph_replacements_t repl;
    err = parse_glyph_replacements(arg, repl);
    if (!err.empty())
        return (err);

    spec = subst_spec(keyval, sep == ':', repl);
    return ("");
}

std::string map_lines::add_nsubst(const std::string &s)
{
    std::vector<subst_spec> substs;

    int sep;
    std::string key, arg;

    std::string err = mapdef_split_key_item(s, &key, &sep, &arg);
    if (!err.empty())
        return (err);

    std::vector<std::string> segs = split_string("/", arg);
    for (int i = 0, vsize = segs.size(); i < vsize; ++i)
    {
        std::string &ns = segs[i];
        if (ns.find('=') == std::string::npos
            && ns.find(':') == std::string::npos)
        {
            if (i < vsize - 1)
                ns = "1=" + ns;
            else
                ns = "*=" + ns;
        }
        subst_spec spec;
        err = parse_nsubst_spec(ns, spec);
        if (!err.empty())
            return (make_stringf("Bad NSUBST spec: %s (%s)",
                                 s.c_str(), err.c_str()));
        substs.push_back(spec);
    }

    transforms.push_back( new nsubst_spec(key[0], substs) );
    return ("");
}

std::string map_lines::add_shuffle(const std::string &raws)
{
    std::string s = raws;
    const std::string err = check_shuffle(s);

    if (err.empty())
        transforms.push_back( new shuffle_spec(s) );

    return (err);
}

void map_lines::remove_shuffle(const std::string &raw)
{
    std::string s = raw;
    const std::string err = check_shuffle(s);
    if (err.empty())
    {
        const shuffle_spec ss(s);
        for (int i = 0, vsize = transforms.size(); i < vsize; ++i)
        {
            if (transforms[i]->type() == map_transformer::TT_SHUFFLE)
            {
                const shuffle_spec *other =
                    dynamic_cast<shuffle_spec*>(transforms[i]);
                if (ss == *other)
                {
                    delete transforms[i];
                    transforms.erase( transforms.begin() + i );
                    return;
                }
            }
        }
    }
}

void map_lines::remove_subst(const std::string &raw)
{
    // Parsing subst specs is a pain, so we just let add_subst do the
    // work, then pop the subst off the end of the vector.
    if (add_subst(raw).empty())
    {
        map_transformer *sub = *transforms.rbegin();
        subst_spec spec = *dynamic_cast<subst_spec*>(sub);
        delete sub;
        transforms.pop_back();

        for (int i = 0, vsize = transforms.size(); i < vsize; ++i)
        {
            if (transforms[i]->type() == map_transformer::TT_SUBST)
            {
                subst_spec *cand = dynamic_cast<subst_spec*>(transforms[i]);
                if (spec == *cand)
                {
                    delete cand;
                    transforms.erase( transforms.begin() + i );
                    return;
                }
            }
        }
    }
}

void map_lines::clear_transforms(map_transformer::transform_type tt)
{
    if (transforms.empty())
        return;
    for (int i = transforms.size() - 1; i >= 0; --i)
        if (transforms[i]->type() == tt)
        {
            delete transforms[i];
            transforms.erase( transforms.begin() + i );
        }
}

void map_lines::clear_colours()
{
    clear_transforms(map_transformer::TT_COLOUR);
}

#ifdef USE_TILE
void map_lines::clear_rocktiles()
{
    clear_transforms(map_transformer::TT_ROCKTILE);
}

void map_lines::clear_floortiles()
{
    clear_transforms(map_transformer::TT_FLOORTILE);
}
#endif

void map_lines::clear_shuffles()
{
    clear_transforms(map_transformer::TT_SHUFFLE);
}

void map_lines::clear_nsubsts()
{
    clear_transforms(map_transformer::TT_NSUBST);
}

void map_lines::clear_substs()
{
    clear_transforms(map_transformer::TT_SUBST);
}

int map_lines::width() const
{
    return map_width;
}

int map_lines::height() const
{
    return lines.size();
}

coord_def map_lines::size() const
{
    return coord_def(width(), height());
}

int map_lines::glyph(int x, int y) const
{
    return lines[y][x];
}

int map_lines::glyph(const coord_def &c) const
{
    return glyph(c.x, c.y);
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
    clear_transforms();
    clear_markers();
    lines.clear();
    overlay.reset(NULL);
    map_width = 0;
    solid_checked = false;
}

void map_lines::subst(std::string &s, subst_spec &spec)
{
    std::string::size_type pos = 0;
    while ((pos = s.find(spec.key(), pos)) != std::string::npos)
        s[pos++] = spec.value();
}

void map_lines::subst(subst_spec &spec)
{
    for (int y = 0, ysize = lines.size(); y < ysize; ++y)
        subst(lines[y], spec);
}

void map_lines::overlay_colours(colour_spec &spec)
{
    if (!overlay.get())
        overlay.reset(new overlay_matrix(width(), height()));

    for (int y = 0, ysize = lines.size(); y < ysize; ++y)
    {
        std::string::size_type pos = 0;
        while ((pos = lines[y].find(spec.key, pos)) != std::string::npos)
        {
            (*overlay)(pos, y).colour = spec.get_colour();
            ++pos;
        }
    }
}

#ifdef USE_TILE
void map_lines::overlay_tiles(tile_spec &spec)
{
    if (!overlay.get())
        overlay.reset(new overlay_matrix(width(), height()));

    for (int y = 0, ysize = lines.size(); y < ysize; ++y)
    {
        std::string::size_type pos = 0;
        while ((pos = lines[y].find(spec.key, pos)) != std::string::npos)
        {
            if (spec.floor)
                (*overlay)(pos, y).floortile = spec.get_tile();
            else
                (*overlay)(pos, y).rocktile = spec.get_tile();
            ++pos;
        }
    }
}
#endif

void map_lines::nsubst(nsubst_spec &spec)
{
    std::vector<coord_def> positions;
    for (int y = 0, ysize = lines.size(); y < ysize; ++y)
    {
        std::string::size_type pos = 0;
        while ((pos = lines[y].find(spec.key, pos)) != std::string::npos)
            positions.push_back(coord_def(pos++, y));
    }
    std::random_shuffle(positions.begin(), positions.end(), random2);

    int pcount = 0;
    const int psize = positions.size();
    for (int i = 0, vsize = spec.specs.size();
         i < vsize && pcount < psize; ++i)
    {
        const int nsubsts = spec.specs[i].key();
        pcount += apply_nsubst(positions, pcount, nsubsts, spec.specs[i]);
    }
}

int map_lines::apply_nsubst(std::vector<coord_def> &pos,
                             int start, int nsub,
                             subst_spec &spec)
{
    if (nsub == -1)
        nsub = pos.size();
    const int end = std::min(start + nsub, (int) pos.size());
    int substituted = 0;
    for (int i = start; i < end; ++i)
    {
        const int val = spec.value();
        const coord_def &c = pos[i];
        lines[c.y][c.x] = val;
        ++substituted;
    }
    return (substituted);
}

std::string map_lines::block_shuffle(const std::string &s)
{
    std::vector<std::string> segs = split_string("/", s);
    std::random_shuffle(segs.begin(), segs.end(), random2);
    return (comma_separated_line(segs.begin(), segs.end(), "/", "/"));
}

std::string map_lines::shuffle(std::string s)
{
    std::string result;

    if (s.find('/') != std::string::npos)
        return block_shuffle(s);

    // Inefficient brute-force shuffle.
    while (!s.empty())
    {
        const int c = random2( s.length() );
        result += s[c];
        s.erase(c, 1);
    }

    return (result);
}

void map_lines::resolve_shuffle(const std::string &shufflage)
{
    std::string toshuffle = shufflage;
    std::string shuffled = shuffle(toshuffle);

    if (toshuffle.empty() || shuffled.empty())
        return;

    for (int i = 0, vsize = lines.size(); i < vsize; ++i)
    {
        std::string &s = lines[i];

        for (int j = 0, len = s.length(); j < len; ++j)
        {
            const char c = s[j];
            std::string::size_type pos = toshuffle.find(c);
            if (pos != std::string::npos)
                s[j] = shuffled[pos];
        }
    }
}

std::string map_lines::apply_transforms()
{
    std::string error;
    for (int i = 0, vsize = transforms.size(); i < vsize; ++i)
    {
        error = transforms[i]->apply_transform(*this);
        if (!error.empty())
            return (error);
    }

    // Release the transforms so we don't try them again.
    clear_transforms();
    return ("");
}

void map_lines::normalise(char fillch)
{
    for (int i = 0, vsize = lines.size(); i < vsize; ++i)
    {
        std::string &s = lines[i];
        if (static_cast<int>(s.length()) < map_width)
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

    if (overlay.get())
    {
        std::auto_ptr<overlay_matrix> new_overlay(
            new overlay_matrix( lines.size(), map_width ) );
        for (int i = xs, y = 0; i != xe; i += xi, ++y)
            for (int j = ys, x = 0; j != ye; j += yi, ++x)
                (*new_overlay)(x, y) = (*overlay)(i, j);
        overlay = new_overlay;
    }

    map_width = lines.size();
    lines     = newlines;
    rotate_markers(clockwise);
    solid_checked = false;
}

void map_lines::translate_marker(
    void (map_lines::*xform)(map_marker *, int),
    int par)
{
    for (int i = 0, vsize = markers.size(); i < vsize; ++i)
        (this->*xform)(markers[i], par);
}

void map_lines::vmirror_marker(map_marker *marker, int)
{
    marker->pos.y = height() - 1 - marker->pos.y;
}

void map_lines::hmirror_marker(map_marker *marker, int)
{
    marker->pos.x = width() - 1 - marker->pos.x;
}

void map_lines::rotate_marker(map_marker *marker, int clockwise)
{
    const coord_def c = marker->pos;
    if (clockwise)
        marker->pos = coord_def(width() - 1 - c.y, c.x);
    else
        marker->pos = coord_def(c.y, height() - 1 - c.x);
}

void map_lines::vmirror_markers()
{
    translate_marker(&map_lines::vmirror_marker);
}

void map_lines::hmirror_markers()
{
    translate_marker(&map_lines::hmirror_marker);
}

void map_lines::rotate_markers(bool clock)
{
    translate_marker(&map_lines::rotate_marker, clock);
}

void map_lines::vmirror()
{
    const int vsize = lines.size();
    const int midpoint = vsize / 2;

    for (int i = 0; i < midpoint; ++i)
    {
        std::string temp = lines[i];
        lines[i] = lines[vsize - 1 - i];
        lines[vsize - 1 - i] = temp;
    }

    if (overlay.get())
    {
        for (int i = 0; i < midpoint; ++i)
            for (int j = 0, wide = width(); j < wide; ++j)
                std::swap( (*overlay)(j, i),
                           (*overlay)(j, vsize - 1 - i) );
    }

    vmirror_markers();
    solid_checked = false;
}

void map_lines::hmirror()
{
    const int midpoint = map_width / 2;
    for (int i = 0, vsize = lines.size(); i < vsize; ++i)
    {
        std::string &s = lines[i];
        for (int j = 0; j < midpoint; ++j)
        {
            int c = s[j];
            s[j] = s[map_width - 1 - j];
            s[map_width - 1 - j] = c;
        }
    }

    if (overlay.get())
    {
        for (int i = 0, vsize = lines.size(); i < vsize; ++i)
            for (int j = 0; j < midpoint; ++j)
                std::swap( (*overlay)(j, i),
                           (*overlay)(map_width - 1 - j, i) );
    }

    hmirror_markers();
    solid_checked = false;
}

std::vector<std::string> map_lines::get_shuffle_strings() const
{
    std::vector<std::string> shuffles;
    for (int i = 0, vsize = transforms.size(); i < vsize; ++i)
        if (transforms[i]->type() == map_transformer::TT_SHUFFLE)
            shuffles.push_back( transforms[i]->describe() );
    return (shuffles);
}

std::vector<std::string> map_lines::get_subst_strings() const
{
    std::vector<std::string> substs;
    for (int i = 0, vsize = transforms.size(); i < vsize; ++i)
        if (transforms[i]->type() == map_transformer::TT_SUBST)
            substs.push_back( transforms[i]->describe() );
    return (substs);
}

std::vector<coord_def> map_lines::find_glyph(int gly) const
{
    std::vector<coord_def> points;
    for (int y = height() - 1; y >= 0; --y)
    {
        for (int x = width() - 1; x >= 0; --x)
        {
            const coord_def c(x, y);
            if ((*this)(c) == gly)
                points.push_back(c);
        }
    }
    return (points);
}

coord_def map_lines::find_first_glyph(int gly) const
{
    for (int y = 0, h = height(); y < h; ++y)
    {
        std::string::size_type pos = lines[y].find(gly);
        if (pos != std::string::npos)
            return coord_def(pos, y);
    }

    return coord_def(-1, -1);
}

coord_def map_lines::find_first_glyph(const std::string &glyphs) const
{
    for (int y = 0, h = height(); y < h; ++y)
    {
        std::string::size_type pos = lines[y].find_first_of(glyphs);
        if (pos != std::string::npos)
            return coord_def(pos, y);
    }
    return coord_def(-1, -1);
}

#ifdef USE_TILE
bool map_tile_list::parse(const std::string &s, int weight)
{
    unsigned int idx = 0;
    if (s != "none" && !tile_dngn_index(s.c_str(), idx))
        return false;

    push_back(map_weighted_tile((int)idx, weight));
    return true;
}

std::string map_lines::add_tile(const std::string &sub, bool is_floor)
{
    std::string s = trimmed_string(sub);

    if (s.empty())
        return ("");

    int sep = 0;
    std::string key;
    std::string substitute;

    std::string err = mapdef_split_key_item(sub, &key, &sep, &substitute);
    if (!err.empty())
        return (err);

    map_tile_list list;
    err = parse_weighted_str<map_tile_list>(substitute, list);
    if (!err.empty())
        return (err);

    transforms.push_back(new tile_spec(key[0], sep == ':', is_floor, list));
    return ("");
}

std::string map_lines::add_rocktile(const std::string &sub)
{
    return add_tile(sub, false);
}

std::string map_lines::add_floortile(const std::string &sub)
{
    return add_tile(sub, true);
}

//////////////////////////////////////////////////////////////////////////
// tile_spec

std::string tile_spec::apply_transform(map_lines &map)
{
    map.overlay_tiles(*this);
    return ("");
}

std::string tile_spec::describe() const
{
    return ("");
}

int tile_spec::get_tile()
{
    if (chose_fixed)
        return fixed_tile;

    int chosen = 0;
    int cweight = 0;
    for (int i = 0, size = tiles.size(); i < size; ++i)
        if (x_chance_in_y(tiles[i].second, cweight += tiles[i].second))
            chosen = tiles[i].first;

    if (fix)
    {
        chose_fixed = true;
        fixed_tile = chosen;
    }
    return (chosen);
}

#endif

///////////////////////////////////////////////
// dlua_set_map

dlua_set_map::dlua_set_map(map_def *map)
{
    dlua.callfn("dgn_set_map", "m", map);
}

dlua_set_map::~dlua_set_map()
{
    dlua.callfn("dgn_set_map", 0, 0);
}

///////////////////////////////////////////////
// map_def
//

map_def::map_def()
    : name(), tags(), place(), depths(), orient(), chance(), weight(),
      weight_depth_mult(), weight_depth_div(), welcome_messages(), map(),
      mons(), items(), random_mons(), keyspecs(), prelude("dlprelude"),
      main("dlmain"), validate("dlvalidate"), veto("dlveto"),
      rock_colour(BLACK), floor_colour(BLACK), rock_tile(0), floor_tile(0),
      index_only(false), cache_offset(0L)
{
    init();
}

void map_def::init()
{
    orient = MAP_NONE;
    name.clear();
    tags.clear();
    place.clear();
    depths.clear();
    prelude.clear();
    main.clear();
    validate.clear();
    veto.clear();
    place_loaded_from.clear();
    reinit();
}

void map_def::reinit()
{
    items.clear();
    random_mons.clear();
    keyspecs.clear();
    level_flags.clear();
    branch_flags.clear();

    welcome_messages.clear();

    rock_colour = floor_colour = BLACK;
    rock_tile = floor_tile = 0;

    // Chance of using this level. Nonzero chance should be used
    // sparingly. When selecting vaults for a place, first those
    // vaults with chance > 0 are considered, in the order they were
    // loaded (which is arbitrary). If random2(100) < chance, the
    // vault is picked, and all other vaults are ignored for that
    // random selection. weight is ignored if the vault is chosen
    // based on its chance.
    chance = 0;

    // If multiple alternative vaults have a chance, the order in which
    // they're tested is based on chance_priority: higher priority vaults
    // are checked first. Vaults with the same priority are tested in
    // unspecified order.
    chance_priority = 0;

    // Weight for this map. When selecting a map, if no map with a
    // nonzero chance is picked, one of the other eligible vaults is
    // picked with a probability of weight / (sum of weights of all
    // eligible vaults).
    weight = 10;

    // How to modify weight based on absolte dungeon depth.  This
    // needs to be done in the C++ code since the map's lua code doesnt'
    // get called again each time the depth changes.
    weight_depth_mult = 0;
    weight_depth_div  = 1;

    // Clearing the map also zaps map transforms.
    map.clear();
    mons.clear();
}

bool map_def::in_map(const coord_def &c) const
{
    return map.in_map(c);
}

int map_def::glyph_at(const coord_def &c) const
{
    return map(c);
}

void map_def::write_full(writer& outf)
{
    cache_offset = outf.tell();
    marshallShort(outf, MAP_CACHE_VERSION);   // Level indicator.
    marshallString4(outf, name);
    prelude.write(outf);
    main.write(outf);
    validate.write(outf);
    veto.write(outf);
}

void map_def::read_full(reader& inf)
{
    // There's a potential race-condition here:
    // - If someone modifies a .des file while there are games in progress,
    // - a new Crawl process will overwrite the .dsc.
    // - older Crawl processes trying to reading the new .dsc will be hosed.
    // We could try to recover from the condition (by locking and
    // reloading the index), but it's easier to save the game at this
    // point and let the player reload.

    const short fp_version = unmarshallShort(inf);
    std::string fp_name;
    unmarshallString4(inf, fp_name);
    if (fp_version != MAP_CACHE_VERSION || fp_name != name)
    {
        save_game(true,
                  make_stringf("Level file cache for %s is out-of-sync! "
                               "Please reload your game.",
                               file.c_str()).c_str());
    }

    prelude.read(inf);
    main.read(inf);
    validate.read(inf);
    veto.read(inf);
}

void map_def::load()
{
    if (!index_only)
        return;

    const std::string descache_base = get_descache_path(file, "");
    file_lock deslock(descache_base + ".lk", "rb", false);
    const std::string loadfile = descache_base + ".dsc";
    FILE *fp = fopen(loadfile.c_str(), "rb");
    fseek(fp, cache_offset, SEEK_SET);
    reader inf(fp);
    read_full(inf);
    fclose(fp);

    index_only = false;
}

std::vector<coord_def> map_def::find_glyph(int glyph) const
{
    return map.find_glyph(glyph);
}

coord_def map_def::find_first_glyph(int glyph) const
{
    return map.find_first_glyph(glyph);
}

coord_def map_def::find_first_glyph(const std::string &s) const
{
    return map.find_first_glyph(s);
}

void map_def::write_index(writer& outf) const
{
    if (!cache_offset)
        end(1, false, "Map %s: can't write index - cache offset not set!",
            name.c_str());
    marshallString4(outf, name);
    marshallString4(outf, place_loaded_from.filename);
    marshallLong(outf, place_loaded_from.lineno);
    marshallShort(outf, orient);
    marshallLong(outf, chance_priority);
    marshallLong(outf, chance);
    marshallLong(outf, weight);
    marshallLong(outf, cache_offset);
    marshallString4(outf, tags);
    place.save(outf);
    write_depth_ranges(outf);
    prelude.write(outf);
}

void map_def::read_index(reader& inf)
{
    unmarshallString4(inf, name);
    unmarshallString4(inf, place_loaded_from.filename);
    place_loaded_from.lineno   = unmarshallLong(inf);
    orient       = static_cast<map_section_type>( unmarshallShort(inf) );
    chance_priority = unmarshallLong(inf);
    chance       = unmarshallLong(inf);
    weight       = unmarshallLong(inf);
    cache_offset = unmarshallLong(inf);
    unmarshallString4(inf, tags);
    place.load(inf);
    read_depth_ranges(inf);
    prelude.read(inf);
    index_only   = true;
}

void map_def::write_depth_ranges(writer& outf) const
{
    marshallShort(outf, depths.size());
    for (int i = 0, sz = depths.size(); i < sz; ++i)
        depths[i].write(outf);
}

void map_def::read_depth_ranges(reader& inf)
{
    depths.clear();
    const int nranges = unmarshallShort(inf);
    for (int i = 0; i < nranges; ++i)
    {
        level_range lr;
        lr.read(inf);
        depths.push_back(lr);
    }
}

void map_def::set_file(const std::string &s)
{
    prelude.set_file(s);
    main.set_file(s);
    validate.set_file(s);
    veto.set_file(s);
    file = get_base_filename(s);
}

std::string map_def::run_lua(bool run_main)
{
    dlua_set_map mset(this);

    int err = prelude.load(dlua);
    if (err == -1000)
        lua_pushnil(dlua);
    else if (err)
        return (prelude.orig_error());

    if (!run_main)
        lua_pushnil(dlua);
    else
    {
        err = main.load(dlua);
        if (err == -1000)
            lua_pushnil(dlua);
        else if (err)
            return (main.orig_error());
    }

    if (!dlua.callfn("dgn_run_map", 2, 0))
        return rewrite_chunk_errors(dlua.error);

    return (dlua.error);
}

bool map_def::test_lua_boolchunk(dlua_chunk &chunk, bool defval, bool croak)
{
    bool result = defval;
    dlua_set_map mset(this);

    int err = chunk.load(dlua);
    if (err == -1000)
        return (result);
    else if (err)
    {
        if (croak)
            end(1, false, "Lua error: %s", validate.orig_error().c_str());
        else
            mprf(MSGCH_ERROR, "Lua error: %s", validate.orig_error().c_str());
        return (result);
    }
    if (dlua.callfn("dgn_run_map", 1, 1))
        dlua.fnreturns(">b", &result);
    else
    {
        if (croak)
            end(1, false, "Lua error: %s",
                rewrite_chunk_errors(dlua.error).c_str());
        else
            mprf(MSGCH_ERROR, "Lua error: %s",
                 rewrite_chunk_errors(dlua.error).c_str());
    }
    return (result);
}

bool map_def::test_lua_validate(bool croak)
{
    return validate.empty() || test_lua_boolchunk(validate, false, croak);
}

bool map_def::test_lua_veto()
{
    return !veto.empty() && test_lua_boolchunk(veto, true);
}

std::string map_def::rewrite_chunk_errors(const std::string &s) const
{
    std::string res = s;
    if (prelude.rewrite_chunk_errors(res))
        return (res);
    if (main.rewrite_chunk_errors(res))
        return (res);
    if (validate.rewrite_chunk_errors(res))
        return (res);
    veto.rewrite_chunk_errors(res);
    return (res);
}

std::string map_def::validate_map_def()
{
    std::string err = run_lua(true);
    if (!err.empty())
        return (err);

    fixup();
    resolve();
    test_lua_validate(true);

    if (orient == MAP_FLOAT || is_minivault())
    {
        if (map.width() > GXM - MAPGEN_BORDER * 2
            || map.height() > GYM - MAPGEN_BORDER * 2)
        {
            return make_stringf(
                     "%s is too big: %dx%d - max %dx%d",
                     is_minivault()? "Minivault" : "Float",
                     map.width(), map.height(),
                     GXM - MAPGEN_BORDER * 2,
                     GYM - MAPGEN_BORDER * 2);
        }
    }
    else
    {
        if (map.width() > GXM
            || map.height() > GYM)
        {
            return make_stringf(
                     "Map is too big: %dx%d - max %dx%d",
                     map.width(), map.height(),
                     GXM, GYM);
        }
    }

    switch (orient)
    {
    case MAP_NORTH: case MAP_SOUTH:
        if (map.height() > GYM * 2 / 3)
            return make_stringf("Map too large - height %d (max %d)",
                                map.height(), GYM * 2 / 3);
        break;
    case MAP_EAST: case MAP_WEST:
        if (map.width() > GXM * 2 / 3)
            return make_stringf("Map too large - width %d (max %d)",
                                map.width(), GXM * 2 / 3);
        break;
    case MAP_NORTHEAST: case MAP_SOUTHEAST:
    case MAP_NORTHWEST: case MAP_SOUTHWEST:
    case MAP_FLOAT:
        if (map.width() > GXM * 2 / 3 || map.height() > GYM * 2 / 3)
            return make_stringf("Map too large - %dx%d (max %dx%d)",
                                map.width(), map.height(),
                                GXM * 2 / 3, GYM * 2 / 3);
        break;
    default:
        break;
    }

    dlua_set_map dl(this);
    return (map.apply_transforms());
}

bool map_def::is_usable_in(const level_id &lid) const
{
    bool any_matched = false;
    for (int i = 0, sz = depths.size(); i < sz; ++i)
    {
        const level_range &lr = depths[i];
        if (lr.matches(lid))
        {
            if (lr.deny)
                return (false);
            any_matched = true;
        }
    }
    return (any_matched);
}

void map_def::add_depth(const level_range &range)
{
    depths.push_back(range);
}

void map_def::add_depths(depth_ranges::const_iterator s,
                         depth_ranges::const_iterator e)
{
    depths.insert(depths.end(), s, e);
}

bool map_def::has_depth() const
{
    return (!depths.empty());
}

bool map_def::is_minivault() const
{
    return (has_tag("minivault"));
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

point_vector map_def::anchor_points() const
{
    point_vector points;
    for (int y = 0, cheight = map.height(); y < cheight; ++y)
        for (int x = 0, cwidth = map.width(); x < cwidth; ++x)
            if (map.glyph(x, y) == '@')
                points.push_back(coord_def(x, y));
    return (points);
}

coord_def map_def::float_aligned_place() const
{
    const point_vector our_anchors = anchor_points();
    const coord_def fail(-1, -1);

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS,
         "Aligning floating vault with %lu points vs %lu reference points",
         our_anchors.size(), map_anchor_points.size());
#endif

    // Mismatch in the number of points we have to align, bail.
    if (our_anchors.size() != map_anchor_points.size())
        return (fail);

    // Align first point of both vectors, then check that the others match.
    const coord_def pos = map_anchor_points[0] - our_anchors[0];

    for (int i = 1, psize = map_anchor_points.size(); i < psize; ++i)
        if (pos + our_anchors[i] != map_anchor_points[i])
            return (fail);

    // Looking good, check bounds.
    if (!map_bounds(pos) || !map_bounds(pos + size() - 1))
        return (fail);

    // Go us!
    return (pos);
}

coord_def map_def::float_place()
{
    ASSERT(orient == MAP_FLOAT);

    coord_def pos(-1, -1);

    if (!map_anchor_points.empty())
        pos = float_aligned_place();
    else
    {
        if (coinflip())
            pos = float_dock();

        if (pos.x == -1)
            pos = float_random_place();
    }

    return (pos);
}

void map_def::hmirror()
{
    if (has_tag("no_hmirror"))
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
    if (has_tag("no_vmirror"))
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
    if (has_tag("no_rotate"))
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
    // Pad out lines that are shorter than max.
    map.normalise(' ');
}

std::string map_def::resolve()
{
    dlua_set_map dl(this);
    return map.apply_transforms();
}

void map_def::fixup()
{
    normalise();

    // Fixup minivaults into floating vaults tagged "minivault".
    if (orient == MAP_NONE)
    {
        orient = MAP_FLOAT;
        tags += " minivault";
    }
}

bool map_def::has_tag(const std::string &tagwanted) const
{
    return !tags.empty() && !tagwanted.empty()
        && tags.find(" " + tagwanted + " ") != std::string::npos;
}

bool map_def::has_tag_prefix(const std::string &prefix) const
{
    return !tags.empty() && !prefix.empty()
        && tags.find(" " + prefix) != std::string::npos;
}

bool map_def::has_tag_suffix(const std::string &suffix) const
{
    return !tags.empty() && !suffix.empty()
        && tags.find(suffix + " ") != std::string::npos;
}

std::vector<std::string> map_def::get_tags() const
{
    return split_string(" ", tags);
}

const keyed_mapspec *map_def::mapspec_for_key(int key) const
{
    keyed_specs::const_iterator i = keyspecs.find(key);
    return i != keyspecs.end()? &i->second : NULL;
}

keyed_mapspec *map_def::mapspec_for_key(int key)
{
    keyed_specs::iterator i = keyspecs.find(key);
    return i != keyspecs.end()? &i->second : NULL;
}

std::string map_def::add_key_field(
    const std::string &s,
    std::string (keyed_mapspec::*set_field)(const std::string &s, bool fixed))
{
    int separator = 0;
    std::string key, arg;

    std::string err = mapdef_split_key_item(s, &key, &separator, &arg);
    if (!err.empty())
        return (err);

    keyed_mapspec &km = keyspecs[key[0]];
    km.key_glyph = key[0];
    return ((km.*set_field)(arg, separator == ':'));
}

std::string map_def::add_key_item(const std::string &s)
{
    return add_key_field(s, &keyed_mapspec::set_item);
}

std::string map_def::add_key_feat(const std::string &s)
{
    return add_key_field(s, &keyed_mapspec::set_feat);
}

std::string map_def::add_key_mons(const std::string &s)
{
    return add_key_field(s, &keyed_mapspec::set_mons);
}

std::string map_def::add_key_mask(const std::string &s)
{
    return add_key_field(s, &keyed_mapspec::set_mask);
}

std::vector<std::string> map_def::get_shuffle_strings() const
{
    return map.get_shuffle_strings();
}

std::vector<std::string> map_def::get_subst_strings() const
{
    return map.get_subst_strings();
}

///////////////////////////////////////////////////////////////////
// mons_list
//

mons_list::mons_list() : mons()
{
}

int mons_list::fix_demon(int demon) const
{
    if (demon >= -1)
        return (demon);

    demon = -100 - demon;

    return (summon_any_demon( static_cast<demon_class_type>(demon) ));
}

mons_spec mons_list::pick_monster(mons_spec_slot &slot)
{
    int totweight = 0;
    mons_spec pick;

    for (mons_spec_list::iterator i = slot.mlist.begin();
         i != slot.mlist.end(); ++i)
    {
        const int weight = i->genweight;
        if (x_chance_in_y(weight, totweight += weight))
        {
            pick = *i;

            if (pick.mid < 0 && pick.fix_mons)
                pick.mid = i->mid = fix_demon(pick.mid);
        }
    }

    if (pick.mid < 0)
        pick = fix_demon(pick.mid);

    if (slot.fix_slot)
    {
        slot.mlist.clear();
        slot.mlist.push_back( pick );
        slot.fix_slot = false;
    }

    if (pick.mid == MONS_WEAPON_MIMIC && !pick.fix_mons)
        pick.mid = random_range(MONS_GOLD_MIMIC, MONS_POTION_MIMIC);

    return (pick);
}

mons_spec mons_list::get_monster(int index)
{
    if (index < 0 || index >= (int) mons.size())
        return mons_spec(RANDOM_MONSTER);

    return (pick_monster( mons[index] ));
}

mons_spec mons_list::get_monster(int slot_index, int list_index) const
{
    if (slot_index < 0 || slot_index >= (int) mons.size())
        return mons_spec(RANDOM_MONSTER);

    const mons_spec_list &list = mons[slot_index].mlist;

    if (list_index < 0 || list_index >= (int) list.size())
        return mons_spec(RANDOM_MONSTER);

    return list[list_index];
}

void mons_list::clear()
{
    mons.clear();
}

bool mons_list::check_mimic(const std::string &s, int *mid, bool *fix) const
{
    if (s == "mimic")
    {
        *mid = MONS_WEAPON_MIMIC;
        *fix = false;
        return (true);
    }
    else if (s == "gold mimic")
        *mid = MONS_GOLD_MIMIC;
    else if (s == "weapon mimic")
        *mid = MONS_WEAPON_MIMIC;
    else if (s == "armour mimic")
        *mid = MONS_ARMOUR_MIMIC;
    else if (s == "scroll mimic")
        *mid = MONS_SCROLL_MIMIC;
    else if (s == "potion mimic")
        *mid = MONS_POTION_MIMIC;
    else
        return (false);

    *fix = true;
    return (true);
}

mons_list::mons_spec_slot mons_list::parse_mons_spec(std::string spec)
{
    mons_spec_slot slot;

    slot.fix_slot = strip_tag(spec, "fix_slot");

    std::vector<std::string> specs = split_string("/", spec);

    for (int i = 0, ssize = specs.size(); i < ssize; ++i)
    {
        std::string s = specs[i];
        std::vector<std::string> parts = split_string(";", s);
        std::string mon_str = parts[0];

        mons_spec mspec;

        if (parts.size() > 2)
        {
            error = make_stringf("Too many semi-colons for '%s' spec.",
                                 mon_str.c_str());
            return (slot);
        }
        else if (parts.size() == 2)
        {
            // TODO: Allow for a "fix_slot" type tag which will cause
            // all monsters generated from this spec to have the
            // exact same equipment.
            std::string items_str = parts[1];
            items_str = replace_all(items_str, "|", "/");

            std::vector<std::string> segs = split_string(".", items_str);

            if (segs.size() > NUM_MONSTER_SLOTS)
            {
                error = make_stringf("More items than monster item slots "
                                     "for '%s'.", mon_str.c_str());
                return (slot);
            }

            for (int j = 0, isize = segs.size(); j < isize; ++j)
            {
                error = mspec.items.add_item(segs[j], false);
                if (!error.empty())
                    return (slot);
            }
        }

        mspec.genweight = find_weight(mon_str);
        if (mspec.genweight == TAG_UNFOUND || mspec.genweight <= 0)
            mspec.genweight = 10;

        mspec.fix_mons       = strip_tag(mon_str, "fix_mons");
        mspec.generate_awake = strip_tag(mon_str, "generate_awake");
        mspec.patrolling     = strip_tag(mon_str, "patrolling");
        mspec.band           = strip_tag(mon_str, "band");

        if (!mon_str.empty() && isdigit(mon_str[0]))
        {
            // Look for space after initial digits.
            std::string::size_type pos =
                mon_str.find_first_not_of("0123456789");
            if (pos != std::string::npos && mon_str[pos] == ' ')
            {
                const std::string mcount = mon_str.substr(0, pos);
                const int count = atoi(mcount.c_str());
                if (count >= 1 && count <= 99)
                    mspec.quantity = count;

                mon_str = mon_str.substr(pos);
            }
        }

        // place:Elf:7 to choose monsters appropriate for that level,
        // for example.
        const std::string place = strip_tag_prefix(mon_str, "place:");
        if (!place.empty())
        {
            try
            {
                mspec.place = level_id::parse_level_id(place);
            }
            catch (const std::string &err)
            {
                error = err;
                return (slot);
            }
        }

        mspec.mlevel = strip_number_tag(mon_str, "lev:");
        if (mspec.mlevel == TAG_UNFOUND)
            mspec.mlevel = 0;

        std::string colour = strip_tag_prefix(mon_str, "col:");
        if (!colour.empty())
        {
            mspec.colour = str_to_colour(colour, BLACK);
            if (mspec.colour == BLACK)
            {
                error = make_stringf("bad monster colour \"%s\" in \"%s\"",
                                     colour.c_str(), specs[i].c_str());
                return (slot);
            }
        }

        trim_string(mon_str);

        if (mon_str == "8")
            mspec.mlevel = -8;
        else if (mon_str == "9")
            mspec.mlevel = -9;
        else if (check_mimic(mon_str, &mspec.mid, &mspec.fix_mons))
            ;
        else if (mspec.place.is_valid())
        {
            // For monster specs such as place:Orc:4 zombie, we may
            // have a monster modifier, in which case we set the
            // modifier in monbase.
            const mons_spec nspec = mons_by_name("orc " + mon_str);
            if (nspec.mid != MONS_PROGRAM_BUG)
            {
                // Is this a modified monster?
                if (nspec.monbase != MONS_PROGRAM_BUG
                    && mons_class_is_zombified(nspec.mid))
                {
                    mspec.monbase = static_cast<monster_type>(nspec.mid);
                }
            }
        }
        else if (mon_str != "0")
        {
            const mons_spec nspec = mons_by_name(mon_str);

            if (nspec.mid == MONS_PROGRAM_BUG)
            {
                error = make_stringf("unknown monster: \"%s\"",
                                     mon_str.c_str());
                return (slot);
            }

            mspec.mid     = nspec.mid;
            mspec.monbase = nspec.monbase;
            mspec.number  = nspec.number;
        }

        if (mspec.items.size() > 0)
        {
            monster_type mid = (monster_type)mspec.mid;
            if (mid == RANDOM_DRACONIAN
                || mid == RANDOM_BASE_DRACONIAN
                || mid == RANDOM_NONBASE_DRACONIAN)
            {
                mid = MONS_DRACONIAN;
            }

            if (mid >= NUM_MONSTERS)
            {
                error = "Can't give spec items to a random monster.";
                return (slot);
            }
            else if (mons_class_itemuse(mid) < MONUSE_STARTING_EQUIPMENT)
            {
                if (mid != MONS_DANCING_WEAPON || mspec.items.size() > 1)
                    error = make_stringf("Monster '%s' can't use items.",
                                         mon_str.c_str());
            }
        }

        slot.mlist.push_back(mspec);
    }

    return (slot);
}

std::string mons_list::add_mons(const std::string &s, bool fix)
{
    error.clear();

    mons_spec_slot slotmons = parse_mons_spec(s);
    if (!error.empty())
        return (error);

    if (fix)
        slotmons.fix_slot = true;

    mons.push_back( slotmons );

    return (error);
}

std::string mons_list::set_mons(int index, const std::string &s)
{
    error.clear();

    if (index < 0)
        return (error = make_stringf("Index out of range: %d", index));

    mons_spec_slot slotmons = parse_mons_spec(s);
    if (!error.empty())
        return (error);

    if (index >= (int) mons.size())
    {
        mons.reserve(index + 1);
        mons.resize(index + 1, mons_spec_slot());
    }
    mons[index] = slotmons;
    return (error);
}

void mons_list::get_zombie_type(std::string s, mons_spec &spec) const
{
    static const char *zombie_types[] =
    {
        " zombie", " skeleton", " simulacrum", " spectre", NULL
    };

    // This order must match zombie_types, indexed from one.
    static const monster_type zombie_montypes[][2] =
    {     // small               // large
        { MONS_PROGRAM_BUG,      MONS_PROGRAM_BUG },
        { MONS_ZOMBIE_SMALL,     MONS_ZOMBIE_LARGE },
        { MONS_SKELETON_SMALL,   MONS_SKELETON_LARGE },
        { MONS_SIMULACRUM_SMALL, MONS_SIMULACRUM_LARGE },
        { MONS_SPECTRAL_THING,   MONS_SPECTRAL_THING },
    };

    const int mod = ends_with(s, zombie_types);
    if (!mod)
    {
        const std::string &spectre = "spectral ";
        if (s.find(spectre) == 0)
        {
            s = s.substr(spectre.length());
            spec.mid = MONS_SPECTRAL_THING;
            spec.monbase = get_monster_by_name(s, true);
            if (!mons_zombie_size(spec.monbase))
                spec.mid = MONS_PROGRAM_BUG;
            return;
        }
        spec.mid = MONS_PROGRAM_BUG;
        return;
    }

    s = s.substr(0, s.length() - strlen(zombie_types[mod - 1]));
    trim_string(s);

    spec.monbase = get_monster_by_name(s, true);

    const int zombie_size = mons_zombie_size(spec.monbase);
    if (!zombie_size)
    {
        spec.mid = MONS_PROGRAM_BUG;
        return;
    }

    spec.mid = zombie_montypes[mod][zombie_size - 1];
}

mons_spec mons_list::get_hydra_spec(const std::string &name) const
{
    int         nheads = -1;
    std::string prefix = name.substr(0, name.find("-"));

    nheads = atoi(prefix.c_str());
    if (nheads != 0)
        ;
    else if (prefix == "0")
        nheads = 0;
    else
    {
        // Might be "two-headed hydra" type string.
        for (int i = 0; i <= 20; i++)
            if (number_in_words(i) == prefix)
            {
                nheads = i;
                break;
            }
    }

    if (nheads < 1)
        nheads = MONS_PROGRAM_BUG;  // What can I say? :P
    else if (nheads > 20)
    {
#if DEBUG || DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Hydra spec wants %d heads, clamping to 20.",
             nheads);
#endif
        nheads = 20;
    }

    return mons_spec(MONS_HYDRA, MONS_PROGRAM_BUG, nheads);
}

// Handle draconians specified as:
// Exactly as in mon-data.h:
//    yellow draconian or draconian knight - the monster specified.
//
// Others:
//    any draconian => any random draconain
//    any base draconian => any unspecialised coloured draconian.
//    any nonbase draconian => any specialised coloured draconian.
//    any <colour> draconian => any draconian of the colour.
//    any nonbase <colour> draconian => any specialised drac of the colour.
//
mons_spec mons_list::drac_monspec(std::string name) const
{
    mons_spec spec;

    spec.mid = get_monster_by_name(name, true);

    // Check if it's a simple drac name, we're done.
    if (spec.mid != MONS_PROGRAM_BUG)
        return (spec);

    spec.mid = RANDOM_DRACONIAN;

    // Request for any draconian?
    if (starts_with(name, "any "))
        // Strip "any "
        name = name.substr(4);

    if (starts_with(name, "base "))
    {
        // Base dracs need no further work.
        return (RANDOM_BASE_DRACONIAN);
    }
    else if (starts_with(name, "nonbase "))
    {
        spec.mid = RANDOM_NONBASE_DRACONIAN;
        name = name.substr(8);
    }

    trim_string(name);

    // Match "any draconian"
    if (name == "draconian")
        return (spec);

    // Check for recognition again to match any (nonbase) <colour> draconian.
    const monster_type colour = get_monster_by_name(name, true);
    if (colour != MONS_PROGRAM_BUG)
    {
        spec.monbase = colour;
        return (spec);
    }

    // Only legal possibility left is <colour> boss drac.
    std::string::size_type wordend = name.find(' ');
    if (wordend == std::string::npos)
        return (MONS_PROGRAM_BUG);

    std::string scolour = name.substr(0, wordend);
    if ((spec.monbase = draconian_colour_by_name(scolour)) == MONS_PROGRAM_BUG)
        return (MONS_PROGRAM_BUG);

    name = trimmed_string(name.substr(wordend + 1));
    spec.mid = get_monster_by_name(name, true);

    // We should have a non-base draconian here.
    if (spec.mid == MONS_PROGRAM_BUG
        || mons_genus(spec.mid) != MONS_DRACONIAN
        || spec.mid == MONS_DRACONIAN
        || (spec.mid >= MONS_BLACK_DRACONIAN
            && spec.mid <= MONS_PALE_DRACONIAN))
    {
        return (MONS_PROGRAM_BUG);
    }

    return (spec);
}

mons_spec mons_list::mons_by_name(std::string name) const
{
    lowercase(name);

    name = replace_all_of( name, "_", " " );
    name = replace_all( name, "random", "any" );

    if (name == "nothing")
        return (-1);

    // Special casery:
    if (name == "pandemonium demon")
        return (MONS_PANDEMONIUM_DEMON);

    if (name == "any" || name == "any monster")
        return (RANDOM_MONSTER);

    if (name == "any demon")
        return (-100 - DEMON_RANDOM);

    if (name == "any lesser demon" || name == "lesser demon")
        return (-100 - DEMON_LESSER);

    if (name == "any common demon" || name == "common demon")
        return (-100 - DEMON_COMMON);

    if (name == "any greater demon" || name == "greater demon")
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

    if (ends_with(name, "-headed hydra"))
        return get_hydra_spec(name);

    mons_spec spec;
    get_zombie_type(name, spec);
    if (spec.mid != MONS_PROGRAM_BUG)
        return (spec);

    if (name.find("draconian") != std::string::npos)
        return drac_monspec(name);

    return (get_monster_by_name(name, true));
}

//////////////////////////////////////////////////////////////////////
// item_list

void item_list::clear()
{
    items.clear();
}

item_spec item_list::pick_item(item_spec_slot &slot)
{
    int cumulative = 0;
    item_spec spec;
    for (item_spec_list::const_iterator i = slot.ilist.begin();
         i != slot.ilist.end(); ++i)
    {
        const int weight = i->genweight;
        if (x_chance_in_y(weight, cumulative += weight))
            spec = *i;
    }

    if (slot.fix_slot)
    {
        slot.ilist.clear();
        slot.ilist.push_back(spec);
        slot.fix_slot = false;
    }

    return (spec);
}

item_spec item_list::get_item(int index)
{
    if (index < 0 || index >= (int) items.size())
    {
        const item_spec none;
        return (none);
    }

    return (pick_item(items[index]));
}

std::string item_list::add_item(const std::string &spec, bool fix)
{
    error.clear();

    item_spec_slot sp = parse_item_spec(spec);
    if (error.empty())
    {
        if (fix)
            sp.fix_slot = true;
        items.push_back(sp);
    }

    return (error);
}

std::string item_list::set_item(int index, const std::string &spec)
{
    error.clear();
    if (index < 0)
        return (error = make_stringf("Index %d out of range", index));
    item_spec_slot sp = parse_item_spec(spec);
    if (error.empty())
    {
        if (index >= (int) items.size())
        {
            items.reserve(index + 1);
            items.resize(index + 1, item_spec_slot());
        }
        items.push_back(sp);
    }

    return (error);
}

// TODO: More checking for innapropriate combinations, like the holy
// wrath brand on a demonic weapon or the running ego on a helmet.
static int str_to_ego(item_spec &spec, std::string ego_str)
{
    const char* armour_egos[] = {
        "running",
        "fire_resistance",
        "cold_resistance",
        "poison_resistance",
        "see_invisible",
        "darkness",
        "strength",
        "dexterity",
        "intelligence",
        "ponderousness",
        "levitation",
        "magic_resistance",
        "protection",
        "stealth",
        "resistance",
        "positive_energy",
        "archmagi",
        "preservation",
        "reflection",
        NULL
    };

    const char* weapon_brands[] = {
        "flaming",        // 1
        "freezing",
        "holy_wrath",
        "electrocution",
        "orc_slaying",    // 5
        "dragon_slaying",
        "venom",
        "protection",
        "draining",
        "speed",          // 10
        "vorpal",
        "flame",
        "frost",
        "vampiricism",
        "pain",           // 15
        "distortion",
        "reaching",
        "returning",
        "chaos",
        "confuse",        // 20
        NULL
    };

    const char* missile_brands[] = {
        "flame",
        "ice",
        "poisoned",
        "poisoned_ii",
        "curare",
        "returning",
        NULL
    };

    const char** name_lists[3] = {armour_egos, weapon_brands, missile_brands};

    int armour_order[3]  = {0, 1, 2};
    int weapon_order[3]  = {1, 0, 2};
    int missile_order[3] = {2, 0, 1};

    int *order;

    switch(spec.base_type)
    {
    case OBJ_ARMOUR:
        order = armour_order;
        break;

    case OBJ_WEAPONS:
        order = weapon_order;
        break;

    case OBJ_MISSILES:
        order = missile_order;
        break;

    default:
        DEBUGSTR("Bad base_type for ego'd item.");
        return 0;
    }

    const char** allowed = name_lists[order[0]];

    for (int i = 0; allowed[i] != NULL; i++)
    {
        if (ego_str == allowed[i])
            return (i + 1);
    }

    // Incompatible or non-existant ego type
    for (int i = 1; i <= 2; i++)
    {
        const char** list = name_lists[order[i]];

        for (int j = 0; list[j] != NULL; j++)
            if (ego_str == list[j])
                // Ego incompatible with base type.
                return (-1);
    }

    // Non-existant ego
    return 0;
}

item_spec item_list::parse_single_spec(std::string s)
{
    item_spec result;

    // If there's a colon, this must be a generation weight.
    int weight = find_weight(s);
    if (weight != TAG_UNFOUND)
    {
        result.genweight = weight;
        if (result.genweight <= 0)
        {
            error = make_stringf("Bad item generation weight: '%d'",
                                 result.genweight);
            return (result);
        }
    }

    const int qty = strip_number_tag(s, "q:");
    if (qty != TAG_UNFOUND)
        result.qty = qty;

    std::string ego_str  = strip_tag_prefix(s, "ego:");
    std::string race_str = strip_tag_prefix(s, "race:");
    lowercase(ego_str);
    lowercase(race_str);

    if (race_str == "elven")
        result.race = MAKE_ITEM_ELVEN;
    else if (race_str == "dwarven")
        result.race = MAKE_ITEM_DWARVEN;
    else if (race_str == "orcish")
        result.race = MAKE_ITEM_ORCISH;
    else if (race_str == "none" || race_str == "no_race")
        result.race = MAKE_ITEM_NO_RACE;
    else if (!race_str.empty())
    {
        error = make_stringf("Bad race: %s", race_str.c_str());
        return (result);
    }

    std::string fixed_str = strip_tag_prefix(s, "fixed:");

    if (strip_tag(s, "good_item"))
        result.level = MAKE_GOOD_ITEM;
    else
    {
        int number = strip_number_tag(s, "level:");
        if (number != TAG_UNFOUND)
        {
            if (number <= 0 && number != ISPEC_GOOD && number != ISPEC_SUPERB
                && number != ISPEC_DAMAGED && number != ISPEC_BAD)
            {
                error = make_stringf("Bad item level: %d", number);
                return (result);
            }

            result.level = number;
        }
    }

    if (s.find("damaged ") == 0)
    {
        result.level = ISPEC_DAMAGED;
        s = s.substr(8);
    }
    if (s.find("cursed ") == 0)
    {
        result.level = ISPEC_BAD; // damaged + cursed, actually
        s = s.substr(7);
    }

    if (strip_tag(s, "no_uniq"))
        result.allow_uniques = 0;
    if (strip_tag(s, "allow_uniq"))
        result.allow_uniques = 1;
    else
    {
        int uniq = strip_number_tag(s, "uniq:");
        if (uniq != TAG_UNFOUND)
        {
            if (uniq <= 0)
            {
                error = make_stringf("Bad uniq level: %d", uniq);
                return (result);
            }
            result.allow_uniques = uniq;
        }
    }

    // Clean up after any tag brain damage.
    trim_string(s);

    if (!ego_str.empty())
        error = "Can't set an ego for random items.";

    // Completely random?
    if (s == "random" || s == "any" || s == "%")
        return (result);

    if (s == "*")
    {
        result.level = ISPEC_GOOD;
        return (result);
    }
    else if (s == "|")
    {
        result.level = ISPEC_SUPERB;
        return (result);
    }
    else if (s == "$" || s == "gold")
    {
        if (!ego_str.empty())
            error = "Can't set an ego for gold.";

        result.base_type = OBJ_GOLD;
        result.sub_type = OBJ_RANDOM;
        return (result);
    }

    if (s == "nothing")
    {
        error.clear();
        result.base_type = OBJ_UNASSIGNED;
        return (result);
    }

    // Check for "any objclass"
    if (s.find("any ") == 0)
    {
        parse_random_by_class(s.substr(4), result);
        return (result);
    }

    if (s.find("random ") == 0)
    {
        parse_random_by_class(s.substr(7), result);
        return (result);
    }

    // Check for actual item names.
    error.clear();
    parse_raw_name(s, result);

    if (!error.empty())
        return (result);

    if (!fixed_str.empty())
    {
        result.ego = get_fixedart_num(fixed_str.c_str());
        if (result.ego == SPWPN_NORMAL)
        {
            error = make_stringf("Unknown fixed art: %s", fixed_str.c_str());
            return result;
        }
        return result;
    }

    if (ego_str.empty())
        return (result);

    if (result.base_type != OBJ_WEAPONS
        && result.base_type != OBJ_MISSILES
        && result.base_type != OBJ_ARMOUR)
    {
        error = "An ego can only be applied to a weapon, missile or "
            "armour.";
        return (result);
    }

    if (ego_str == "none")
    {
        result.ego = -1;
        return (result);
    }

    const int ego = str_to_ego(result, ego_str);

    if (ego == 0)
    {
        error = make_stringf("No such ego as: %s", ego_str.c_str());
        return (result);
    }
    else if (ego == -1)
    {
        error = make_stringf("Ego '%s' is incompatible with item '%s'.",
                             ego_str.c_str(), s.c_str());
        return (result);
    }
    result.ego = ego;

    return (result);
}

void item_list::parse_random_by_class(std::string c, item_spec &spec)
{
    trim_string(c);
    if (c == "?" || c.empty())
    {
        error = make_stringf("Bad item class: '%s'", c.c_str());
        return;
    }

    for (int type = OBJ_WEAPONS; type < NUM_OBJECT_CLASSES; ++type)
    {
        if (c == item_class_name(type, true))
        {
            spec.base_type = static_cast<object_class_type>(type);
            return;
        }
    }

    // Random manual?
    if (c == "manual")
    {
        spec.base_type = OBJ_BOOKS;
        spec.sub_type  = BOOK_MANUAL;
        spec.plus      = -1;
        return;
    }
    else if (c == "fixed theme book")
    {
        spec.base_type = OBJ_BOOKS;
        spec.sub_type  = BOOK_RANDART_THEME;
        spec.plus      = -1;
        return;
    }
    else if (c == "fixed level book")
    {
        spec.base_type = OBJ_BOOKS;
        spec.sub_type  = BOOK_RANDART_LEVEL;
        spec.plus      = -1;
        return;
    }

    error = make_stringf("Bad item class: '%s'", c.c_str());
}

void item_list::parse_raw_name(std::string name, item_spec &spec)
{
    trim_string(name);
    if (name.empty())
    {
        error = make_stringf("Bad item name: '%s'", name.c_str());
        return ;
    }

    item_def parsed = find_item_type(OBJ_UNASSIGNED, name);
    if (parsed.sub_type != OBJ_RANDOM)
    {
        spec.base_type = parsed.base_type;
        spec.sub_type  = parsed.sub_type;
        spec.plus      = parsed.plus;
        spec.plus2     = parsed.plus2;
        return;
    }

    error = make_stringf("Bad item name: '%s'", name.c_str());
}

item_list::item_spec_slot item_list::parse_item_spec(std::string spec)
{
    lowercase(spec);

    item_spec_slot list;

    list.fix_slot = strip_tag(spec, "fix_slot");
    std::vector<std::string> specifiers = split_string( "/", spec );

    for (unsigned i = 0; i < specifiers.size() && error.empty(); ++i)
    {
        list.ilist.push_back( parse_single_spec(specifiers[i]) );
    }

    return (list);
}

/////////////////////////////////////////////////////////////////////////
// subst_spec

subst_spec::subst_spec(int torepl, bool dofix, const glyph_replacements_t &g)
    : foo(torepl), fix(dofix), frozen_value(0), repl(g)
{
}

int subst_spec::value()
{
    if (frozen_value)
        return (frozen_value);

    int cumulative = 0;
    int chosen = 0;
    for (int i = 0, size = repl.size(); i < size; ++i)
        if (x_chance_in_y(repl[i].second, cumulative += repl[i].second))
            chosen = repl[i].first;

    if (fix)
        frozen_value = chosen;

    return (chosen);
}

std::string subst_spec::apply_transform(map_lines &map)
{
    map.subst(*this);
    return ("");
}

map_transformer::transform_type subst_spec::type() const
{
    return (TT_SUBST);
}

std::string subst_spec::describe() const
{
    std::string subst(1, foo);
    subst += std::string(" ") + (fix? ':' : '=');
    for (int i = 0, size = repl.size(); i < size; ++i)
    {
        const glyph_weighted_replacement_t &gly = repl[i];
        subst += " ";
        subst += static_cast<char>(gly.first);
        if (gly.second != 10)
            subst += make_stringf(":%d", gly.second);
    }
    return (subst);
}

bool subst_spec::operator == (const subst_spec &other) const
{
    if (foo != other.foo || fix != other.fix)
        return (false);

    if (repl.size() != other.repl.size())
        return (false);

    for (int i = 0, size = repl.size(); i < size; ++i)
    {
        if (repl[i] != other.repl[i])
            return (false);
    }
    return (true);
}

//////////////////////////////////////////////////////////////////////////
// nsubst_spec

nsubst_spec::nsubst_spec(int _key, const std::vector<subst_spec> &_specs)
    : key(_key), specs(_specs)
{
}

std::string nsubst_spec::apply_transform(map_lines &map)
{
    map.nsubst(*this);
    return ("");
}

std::string nsubst_spec::describe() const
{
    return ("");
}

//////////////////////////////////////////////////////////////////////////
// colour_spec

std::string colour_spec::apply_transform(map_lines &map)
{
    map.overlay_colours(*this);
    return ("");
}

std::string colour_spec::describe() const
{
    return ("");
}

int colour_spec::get_colour()
{
    if (fixed_colour != BLACK)
        return (fixed_colour);

    int chosen = BLACK;
    int cweight = 0;
    for (int i = 0, size = colours.size(); i < size; ++i)
        if (x_chance_in_y(colours[i].second, cweight += colours[i].second))
            chosen = colours[i].first;
    if (fix)
        fixed_colour = chosen;
    return (chosen);
}

//////////////////////////////////////////////////////////////////////////
// shuffle_spec

std::string shuffle_spec::apply_transform(map_lines &map)
{
    map.resolve_shuffle(shuffle);
    return ("");
}

map_transformer::transform_type shuffle_spec::type() const
{
    return (TT_SHUFFLE);
}

std::string shuffle_spec::describe() const
{
    return (shuffle);
}

//////////////////////////////////////////////////////////////////////////
// map_marker_spec

std::string map_marker_spec::apply_transform(map_lines &map)
{
    std::vector<coord_def> positions = map.find_glyph(key);

    // Markers with no key are not an error.
    if (positions.empty())
        return ("");

    for (int i = 0, size = positions.size(); i < size; ++i)
    {
        try
        {
            map_marker *mark = create_marker();
            if (!mark)
                return make_stringf("Unable to parse marker from %s",
                                    marker.c_str());
            mark->pos = positions[i];
            map.add_marker(mark);
        }
        catch (const std::string &err)
        {
            return (err);
        }
    }
    return ("");
}

map_marker *map_marker_spec::create_marker()
{
    return lua_fn.get()
        ? new map_lua_marker(*lua_fn.get())
        : map_marker::parse_marker(marker);
}

map_transformer::transform_type map_marker_spec::type() const
{
    return (TT_MARKER);
}

std::string map_marker_spec::describe() const
{
    return ("unimplemented");
}

//////////////////////////////////////////////////////////////////////////
// map_flags
map_flags::map_flags()
    : flags_set(0), flags_unset(0)
{
}

void map_flags::clear()
{
    flags_set = flags_unset = 0;
}

typedef std::map<std::string, unsigned long> flag_map;

map_flags map_flags::parse(const std::string flag_list[],
                           const std::string &s) throw(std::string)
{
    map_flags mf;

    const std::vector<std::string> segs = split_string("/", s);

    flag_map flag_vals;
    for (int i = 0; !flag_list[i].empty(); i++)
        flag_vals[flag_list[i]] = 1 << i;

    for (int i = 0, size = segs.size(); i < size; i++)
    {
        std::string flag   = segs[i];
        bool        negate = false;

        if (flag[0] == '!')
        {
            flag   = flag.substr(1);
            negate = true;
        }

        flag_map::const_iterator val = flag_vals.find(flag);
        if (val == flag_vals.end())
            throw make_stringf("Unknown flag: '%s'", flag.c_str());

        if (negate)
            mf.flags_unset |= val->second;
        else
            mf.flags_set |= val->second;
    }

    return mf;
}

//////////////////////////////////////////////////////////////////////////
// keyed_mapspec

keyed_mapspec::keyed_mapspec()
    :  key_glyph(-1), feat(), item(), mons()
{
}

std::string keyed_mapspec::set_feat(const std::string &s, bool fix)
{
    err.clear();
    parse_features(s);
    feat.fix_slot = fix;
    return (err);
}

void keyed_mapspec::parse_features(const std::string &s)
{
    feat.feats.clear();
    std::vector<std::string> specs = split_string("/", s);
    for (int i = 0, size = specs.size(); i < size; ++i)
    {
        const std::string &spec = specs[i];

        feature_spec_list feats = parse_feature(spec);
        if (!err.empty())
            return;
        feat.feats.insert( feat.feats.end(),
                           feats.begin(),
                           feats.end() );
    }
}

feature_spec keyed_mapspec::parse_trap(std::string s, int weight)
{
    strip_tag(s, "trap");
    trim_string(s);
    lowercase(s);

    const int trap = str_to_trap(s);
    if (trap == -1)
        err = make_stringf("bad trap name: '%s'", s.c_str());

    feature_spec fspec(-1, weight);
    fspec.trap = trap;
    return (fspec);
}

feature_spec keyed_mapspec::parse_shop(std::string s, int weight)
{
    strip_tag(s, "shop");
    trim_string(s);
    lowercase(s);

    const int shop = str_to_shoptype(s);
    if (shop == -1)
        err = make_stringf("bad shop type: '%s'", s.c_str());

    feature_spec fspec(-1, weight);
    fspec.shop = shop;
    return (fspec);
}

feature_spec_list keyed_mapspec::parse_feature(const std::string &str)
{
    std::string s = str;
    int weight = find_weight(s);
    if (weight == TAG_UNFOUND || weight <= 0)
        weight = 10;
    trim_string(s);

    feature_spec_list list;
    if (s.length() == 1)
    {
        feature_spec fsp(-1, weight);
        fsp.glyph = s[0];
        list.push_back( fsp );
        return (list);
    }

    if (s.find("trap") != std::string::npos)
    {
        list.push_back( parse_trap(s, weight) );
        return (list);
    }

    if (s.find("shop") != std::string::npos
        || s.find("store") != std::string::npos)
    {
        list.push_back( parse_shop(s, weight) );
        return (list);
    }

    const dungeon_feature_type ftype = dungeon_feature_by_name(s);
    if (ftype == DNGN_UNSEEN)
        err = make_stringf("no features matching \"%s\"",
                           str.c_str());
    else
        list.push_back( feature_spec( ftype, weight ) );

    return (list);
}

std::string keyed_mapspec::set_mons(const std::string &s, bool fix)
{
    err.clear();
    mons.clear();

    std::vector<std::string> segments = split_string(",", s);
    for (int i = 0, size = segments.size(); i < size; ++i)
    {
        const std::string error = mons.add_mons(segments[i], fix);
        if (!error.empty())
            return (error);
    }
    return ("");
}

std::string keyed_mapspec::set_item(const std::string &s, bool fix)
{
    err.clear();
    item.clear();

    std::vector<std::string> segs = split_string(",", s);

    for (int i = 0, size = segs.size(); i < size; ++i)
    {
        err = item.add_item(segs[i], fix);
        if (!err.empty())
            return (err);
    }
    return (err);
}

std::string keyed_mapspec::set_mask(const std::string &s, bool garbage)
{
    UNUSED(garbage);

    err.clear();

    try
    {
        static std::string flag_list[] =
            {"vault", "no_item_gen", "no_monster_gen", "no_pool_fixup",
             "no_secret_doors", "opaque", ""};
        map_mask = map_flags::parse(flag_list, s);
    }
    catch (const std::string &error)
    {
        err = error;
        return (err);
    }

    // If not also a KFEAT...
    if (feat.feats.size() == 0)
    {
        feature_spec fsp(-1, 10);
        fsp.glyph = key_glyph;
        feat.feats.push_back(fsp);
    }

    return (err);
}

feature_spec keyed_mapspec::get_feat()
{
    return feat.get_feat('.');
}

mons_list &keyed_mapspec::get_monsters()
{
    return (mons);
}

item_list &keyed_mapspec::get_items()
{
    return (item);
}

map_flags &keyed_mapspec::get_mask()
{
    return (map_mask);
}

//////////////////////////////////////////////////////////////////////////
// feature_slot

feature_slot::feature_slot() : feats(), fix_slot(false)
{
}

feature_spec feature_slot::get_feat(int def_glyph)
{
    int tweight = 0;
    feature_spec chosen_feat = feature_spec(DNGN_FLOOR);

    if (def_glyph != -1)
    {
        chosen_feat.feat = -1;
        chosen_feat.glyph = def_glyph;
    }

    for (int i = 0, size = feats.size(); i < size; ++i)
    {
        const feature_spec &feat = feats[i];
        if (x_chance_in_y(feat.genweight, tweight += feat.genweight))
            chosen_feat = feat;
    }

    if (fix_slot)
    {
        feats.clear();
        feats.push_back( chosen_feat );
    }
    return (chosen_feat);
}
