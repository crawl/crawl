/*
 *  File:       mapdef.cc
 *  Summary:    Support code for Crawl des files.
 *  Created by: dshaligram on Wed Nov 22 08:41:20 2006 UTC
 */

#include "AppHdr.h"

#include <iostream>
#include <cstdarg>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <algorithm>

#include "abyss.h"
#include "artefact.h"
#include "branch.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "debug.h"
#include "describe.h"
#include "directn.h"
#include "dungeon.h"
#include "dgn-height.h"
#include "exclude.h"
#include "files.h"
#include "food.h"
#include "initfile.h"
#include "invent.h"
#include "items.h"
#include "l_defs.h"
#include "libutil.h"
#include "mapdef.h"
#include "mapmark.h"
#include "maps.h"
#include "misc.h"
#include "mon-cast.h"
#include "mon-place.h"
#include "mon-util.h"
#include "place.h"
#include "random.h"
#include "religion.h"
#include "spl-util.h"
#include "spl-book.h"
#include "stuff.h"
#include "env.h"
#include "tags.h"
#ifdef USE_TILE
#include "tiledef-dngn.h"
#include "tiledef-player.h"
#endif

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

static string_set Map_Flag_Names;

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

void map_register_flag(const std::string &flag)
{
    Map_Flag_Names.insert(flag);
}

bool map_tag_is_selectable(const std::string &tag)
{
    return (Map_Flag_Names.find(tag) == Map_Flag_Names.end()
            && tag.find("luniq_") != 0
            && tag.find("uniq_") != 0
            && tag.find("ruin_") != 0
            && tag.find("chance_") != 0);
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
        return make_stringf("malformed declaration - must use = or : in '%s'",
                            s.c_str());

    *key = trimmed_string(s.substr(0, sep));
    std::string substitute    = trimmed_string(s.substr(sep + 1));

    if (key->empty()
        || (key_max_len != -1 && (int) key->length() > key_max_len))
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

    if (branch != NUM_BRANCHES)
    {
        if (shallowest == -1)
            shallowest = branches[branch].depth;
        if (deepest == -1)
            deepest = branches[branch].depth;
    }

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
        std::string br    = trimmed_string(s.substr(0, cpos));
        std::string depth = trimmed_string(s.substr(cpos + 1));
        parse_depth_range(depth, &lr.shallowest, &lr.deepest);

        lr.set(br, lr.shallowest, lr.deepest);
    }

    return (lr);
}

void level_range::parse_partial(level_range &lr, const std::string &s)
    throw (std::string)
{
    if (isadigit(s[0]))
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

    if (s == "$")
    {
        *l = *h = -1;
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
// map_lines

map_lines::map_lines()
    : markers(), lines(), overlay(),
      map_width(0), solid_north(false), solid_east(false),
      solid_south(false), solid_west(false), solid_checked(false)
{
}

map_lines::map_lines(const map_lines &map)
{
    init_from(map);
}

void map_lines::write_maplines(writer &outf) const
{
    const int h = height();
    marshallShort(outf, h);
    for (int i = 0; i < h; ++i)
        marshallString(outf, lines[i]);
}

void map_lines::read_maplines(reader &inf)
{
    clear();
    const int h = unmarshallShort(inf);
    ASSERT(h >= 0 && h <= GYM);

    for (int i = 0; i < h; ++i)
        add_line(unmarshallString(inf));
}

rectangle_iterator map_lines::get_iter() const
{
    ASSERT(width() > 0);
    ASSERT(height() > 0);

    coord_def tl(0, 0);
    coord_def br(width() - 1, height() - 1);
    return rectangle_iterator(tl, br);
}

char map_lines::operator () (const coord_def &c) const
{
    return (lines[c.y][c.x]);
}

char& map_lines::operator () (const coord_def &c)
{
    return (lines[c.y][c.x]);
}

char map_lines::operator () (int x, int y) const
{
    return (lines[y][x]);
}

char& map_lines::operator () (int x, int y)
{
    return (lines[y][x]);
}

bool map_lines::in_bounds(const coord_def &c) const
{
    return (c.x >= 0 && c.y >= 0 && c.x < width() && c.y < height());
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
    clear_markers();
}

void map_lines::init_from(const map_lines &map)
{
    // Markers have to be regenerated, they will not be copied.
    clear_markers();
    overlay.reset(NULL);
    lines            = map.lines;
    map_width        = map.map_width;
    solid_north      = map.solid_north;
    solid_east       = map.solid_east;
    solid_south      = map.solid_south;
    solid_west       = map.solid_west;
    solid_checked    = map.solid_checked;
}

template <typename V>
void map_lines::clear_vector(V &vect)
{
    for (int i = 0, vsize = vect.size(); i < vsize; ++i)
        delete vect[i];
    vect.clear();
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
    std::string err = mapdef_split_key_item(s, &key, &sep, &arg, -1);
    if (!err.empty())
        return (err);

    map_marker_spec spec(key, arg);
    spec.apply_transform(*this);

    return ("");
}

std::string map_lines::add_lua_marker(const std::string &key,
                                      const lua_datum &function)
{
    map_marker_spec spec(key, function);
    spec.apply_transform(*this);
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

            const int property = (*overlay)(x, y).property;
            if (property >= FPROP_BLOODY)
                 // Over-ride whatever property is already there.
                env.pgrid(gc) |= property;

            const int fheight = (*overlay)(x, y).height;
            if (fheight != INVALID_HEIGHT)
            {
                if (!env.heightmap.get())
                    dgn_initialise_heightmap();
                dgn_height_at(gc) = fheight;
            }

#ifdef USE_TILE
            int floor = (*overlay)(x, y).floortile;
            if (floor)
            {
                if (colour)
                    floor = tile_dngn_coloured(floor, colour);
                int offset = random2(tile_dngn_count(floor));
                env.tile_flv(gc).floor = floor + offset;
            }
            int rock = (*overlay)(x, y).rocktile;
            if (rock)
            {
                if (colour)
                    rock = tile_dngn_coloured(rock, colour);
                int offset = random2(tile_dngn_count(rock));
                env.tile_flv(gc).wall = rock + offset;
            }
            int tile = (*overlay)(x, y).tile;
            if (tile)
            {
                if (colour)
                    tile = tile_dngn_coloured(tile, colour);
                int offset = random2(tile_dngn_count(tile));
                if ((*overlay)(x, y).last_tile)
                    offset = tile_dngn_count(tile) - 1;
                if (grd(gc) == DNGN_FLOOR && !floor)
                    env.tile_flv(gc).floor = tile + offset;
                else if (grd(gc) == DNGN_ROCK_WALL && !rock)
                    env.tile_flv(gc).wall = tile + offset;
                else
                {
                    if ((*overlay)(x, y).no_random)
                        offset = 0;
                    env.tile_flv(gc).feat = tile + offset;
                }
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
    return (replace_all_of(s, " \t", ""));
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
std::string parse_weighted_str(const std::string &spec, T &list)
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
        return (false);

    push_back(map_weighted_colour(colour, weight));
    return (true);
}

std::string map_lines::add_colour(const std::string &sub)
{
    std::string s = trimmed_string(sub);

    if (s.empty())
        return ("");

    int sep = 0;
    std::string key;
    std::string substitute;

    std::string err = mapdef_split_key_item(sub, &key, &sep, &substitute, -1);
    if (!err.empty())
        return (err);

    map_colour_list colours;
    err = parse_weighted_str<map_colour_list>(substitute, colours);
    if (!err.empty())
        return (err);

    colour_spec spec(key, sep == ':', colours);
    overlay_colours(spec);

    return ("");
}

bool map_fprop_list::parse(const std::string &fp, int weight)
{
    unsigned long fprop;

    if (fp == "nothing")
        fprop = FPROP_NONE;
    else if (fp.empty())
        return (false);
    else if (str_to_fprop(fp) == FPROP_NONE)
        return false;
    else
        fprop = str_to_fprop(fp);

    push_back(map_weighted_fprop(fprop, weight));
    return true;
}

bool map_featheight_list::parse(const std::string &fp, int weight)
{
    const int thisheight = strict_aton(fp.c_str(), INVALID_HEIGHT);
    if (thisheight == INVALID_HEIGHT)
        return (false);
    push_back(map_weighted_fheight(thisheight, weight));
    return (true);
}

std::string map_lines::add_fproperty(const std::string &sub)
{
    std::string s = trimmed_string(sub);

    if (s.empty())
        return ("");

    int sep = 0;
    std::string key;
    std::string substitute;

    std::string err = mapdef_split_key_item(sub, &key, &sep, &substitute, -1);
    if (!err.empty())
        return (err);

    map_fprop_list fprops;
    err = parse_weighted_str<map_fprop_list>(substitute, fprops);
    if (!err.empty())
        return (err);

    fprop_spec spec(key, sep == ':', fprops);
    overlay_fprops(spec);

    return ("");
}

std::string map_lines::add_fheight(const std::string &sub)
{
    std::string s = trimmed_string(sub);
    if (s.empty())
        return ("");

    int sep = 0;
    std::string key;
    std::string substitute;

    std::string err = mapdef_split_key_item(sub, &key, &sep, &substitute, -1);
    if (!err.empty())
        return (err);

    map_featheight_list fheights;
    err = parse_weighted_str(substitute, fheights);
    if (!err.empty())
        return (err);

    fheight_spec spec(key, sep == ':', fheights);
    overlay_fheights(spec);

    return ("");
}

bool map_string_list::parse(const std::string &fp, int weight)
{
    push_back(map_weighted_string(fp, weight));
    return (!fp.empty());
}

std::string map_lines::add_subst(const std::string &sub)
{
    std::string s = trimmed_string(sub);

    if (s.empty())
        return ("");

    int sep = 0;
    std::string key;
    std::string substitute;

    std::string err = mapdef_split_key_item(sub, &key, &sep, &substitute, -1);
    if (!err.empty())
        return (err);

    glyph_replacements_t repl;
    err = parse_glyph_replacements(substitute, repl);
    if (!err.empty())
        return (err);

    subst_spec spec(key, sep == ':', repl);
    subst(spec);

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
    const int count = key == "*"? -1 : atoi(key.c_str());
    if (!count)
        return make_stringf("Illegal spec: %s", s.c_str());

    glyph_replacements_t repl;
    err = parse_glyph_replacements(arg, repl);
    if (!err.empty())
        return (err);

    spec = subst_spec(count, sep == ':', repl);
    return ("");
}

std::string map_lines::add_nsubst(const std::string &s)
{
    std::vector<subst_spec> substs;

    int sep;
    std::string key, arg;

    std::string err = mapdef_split_key_item(s, &key, &sep, &arg, -1);
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

    nsubst_spec spec(key, substs);
    nsubst(spec);

    return ("");
}

std::string map_lines::add_shuffle(const std::string &raws)
{
    std::string s = raws;
    const std::string err = check_shuffle(s);

    if (err.empty())
        resolve_shuffle(s);

    return (err);
}

int map_lines::width() const
{
    return map_width;
}

int map_lines::height() const
{
    return lines.size();
}

void map_lines::extend(int min_width, int min_height, char fill)
{
    min_width = std::max(1, min_width);
    min_height = std::max(1, min_height);

    bool dirty = false;
    int old_width = width();
    int old_height = height();

    if (static_cast<int>(lines.size()) < min_height)
    {
        dirty = true;
        while (static_cast<int>(lines.size()) < min_height)
            add_line(std::string(min_width, fill));
    }

    if (width() < min_width)
    {
        dirty = true;
        lines[0] += std::string(min_width - width(), fill);
        map_width = std::max(map_width, min_width);
    }

    if (!dirty)
        return;

    normalise(fill);

    // Extend overlay matrix as well.
    if (overlay.get())
    {
        std::auto_ptr<overlay_matrix> new_overlay(
            new overlay_matrix(width(), height()));

        for (int y = 0; y < old_height; ++y)
            for (int x = 0; x < old_width; ++x)
                (*new_overlay)(x, y) = (*overlay)(x, y);

        overlay = new_overlay;
    }
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
    return (gly == 'x' || gly == 'c' || gly == 'b' || gly == 'v' || gly == 't');
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
    clear_markers();
    lines.clear();
    keyspecs.clear();
    overlay.reset(NULL);
    map_width = 0;
    solid_checked = false;

    // First non-legal character.
    next_keyspec_idx = 256;
}

void map_lines::subst(std::string &s, subst_spec &spec)
{
    std::string::size_type pos = 0;
    while ((pos = s.find_first_of(spec.key, pos)) != std::string::npos)
        s[pos++] = spec.value();
}

void map_lines::subst(subst_spec &spec)
{
    ASSERT(!spec.key.empty());
    for (int y = 0, ysize = lines.size(); y < ysize; ++y)
        subst(lines[y], spec);
}

void map_lines::bind_overlay()
{
    if (!overlay.get())
        overlay.reset(new overlay_matrix(width(), height()));
}

void map_lines::overlay_colours(colour_spec &spec)
{
    bind_overlay();
    for (iterator mi(*this, spec.key); mi; ++mi)
        (*overlay)(*mi).colour = spec.get_colour();
}

void map_lines::overlay_fprops(fprop_spec &spec)
{
    bind_overlay();
    for (iterator mi(*this, spec.key); mi; ++mi)
        (*overlay)(*mi).property |= spec.get_property();
}

void map_lines::overlay_fheights(fheight_spec &spec)
{
    bind_overlay();
    for (iterator mi(*this, spec.key); mi; ++mi)
        (*overlay)(*mi).height = spec.get_height();
}

void map_lines::fill_mask_matrix(const std::string &glyphs,
                                 const coord_def &tl,
                                 const coord_def &br,
                                 Matrix<bool> &flags)
{
    ASSERT(tl.x >= 0);
    ASSERT(tl.y >= 0);
    ASSERT(br.x < width());
    ASSERT(br.y < height());
    ASSERT(tl.x <= br.x);
    ASSERT(tl.y <= br.y);

    for (int y = tl.y; y <= br.y; ++y)
        for (int x = tl.x; x <= br.x; ++x)
        {
            int ox = x - tl.x;
            int oy = y - tl.y;
            flags(ox, oy) = (strchr(glyphs.c_str(), (*this)(x, y)) != NULL);
        }
}

void map_lines::merge_subvault(const coord_def &mtl, const coord_def &mbr,
                               const Matrix<bool> &mask, const map_def &vmap)
{
    const map_lines &vlines = vmap.map;

    // If vault is bigger than the mask region (mtl, mbr), then it gets
    // randomly centered.  (vtl, vbr) stores the vault's region.
    coord_def vtl = mtl;
    coord_def vbr = mbr;

    int width_diff = (mbr.x - mtl.x + 1) - vlines.width();
    int height_diff = (mbr.y - mtl.y + 1) - vlines.height();

    // Adjust vault coords with a random offset.
    int ox = random2(width_diff + 1);
    int oy = random2(height_diff + 1);
    vtl.x += ox;
    vtl.y += oy;
    vbr.x -= (width_diff - ox);
    vbr.y -= (height_diff - oy);

    if (!overlay.get())
        overlay.reset(new overlay_matrix(width(), height()));

    // Clear any markers in the vault's grid
    for (size_t i = 0; i < markers.size(); ++i)
    {
        map_marker *mm = markers[i];
        if (mm->pos.x >= vtl.x && mm->pos.x <= vbr.x
            && mm->pos.y >= vtl.y && mm->pos.y <= vbr.y)
        {
            const coord_def maskc = mm->pos - mtl;
            if (!mask(maskc.x, maskc.y))
                continue;

            // Erase this marker.
            markers[i] = markers[markers.size() - 1];
            markers.resize(markers.size() - 1);
            i--;
        }
    }

    // Copy markers and update locations.
    for (size_t i = 0; i < vlines.markers.size(); ++i)
    {
        map_marker *mm = vlines.markers[i];
        coord_def mapc = mm->pos + vtl;
        coord_def maskc = mapc - mtl;

        if (!mask(maskc.x, maskc.y))
            continue;

        map_marker *clone = mm->clone();
        clone->pos = mapc;
        add_marker(clone);
    }

    // Cache keyspecs we've already pushed into the extended keyspec space.
    // If !ksmap[key], then we haven't seen the 'key' glyph before.
    keyspec_map ksmap(0);

    for (int y = mtl.y; y <= mbr.y; ++y)
        for (int x = mtl.x; x <= mbr.x; ++x)
        {
            int mx = x - mtl.x;
            int my = y - mtl.y;
            if (!mask(mx, my))
                continue;

            // Outside subvault?
            if (x < vtl.x || x > vbr.x || y < vtl.y || y > vbr.y)
                continue;

            int vx = x - vtl.x;
            int vy = y - vtl.y;
            coord_def vc(vx, vy);

            char c = vlines(vc);
            if (c == ' ')
                continue;

            // Merge keyspecs.
            // Push vault keyspecs into extended keyspecs.
            // Push MONS/ITEM into KMONS/KITEM keyspecs.
            // Generate KFEAT keyspecs for any normal glyphs.
            int idx;
            if (ksmap[c])
            {
                // Already generated this keyed_pmapspec.
                idx = ksmap[c];
            }
            else
            {
                idx = next_keyspec_idx++;
                ASSERT(idx > 0);

                if (c != SUBVAULT_GLYPH)
                    ksmap[c] = idx;

                const keyed_mapspec *kspec = vlines.mapspec_at(vc);

                // If c is a SUBVAULT_GLYPH, it came from a sub-subvault.
                // Sub-subvaults should always have mapspecs at this point.
                ASSERT(c != SUBVAULT_GLYPH || kspec);

                if (kspec)
                {
                    // Copy vault keyspec into the extended keyspecs.
                    keyspecs[idx] = *kspec;
                    keyspecs[idx].key_glyph = SUBVAULT_GLYPH;
                }
                else if (map_def::valid_monster_array_glyph(c))
                {
                    // Translate monster array into keyed_mapspec
                    keyed_mapspec &km = keyspecs[idx];
                    km.key_glyph = SUBVAULT_GLYPH;

                    km.feat.feats.clear();
                    feature_spec spec(-1);
                    spec.glyph = '.';
                    km.feat.feats.insert(km.feat.feats.begin(), spec);

                    int slot = map_def::monster_array_glyph_to_slot(c);
                    km.mons.set_from_slot(vmap.mons, slot);
                }
                else if (map_def::valid_item_array_glyph(c))
                {
                    // Translate item array into keyed_mapspec
                    keyed_mapspec &km = keyspecs[idx];
                    km.key_glyph = SUBVAULT_GLYPH;

                    km.feat.feats.clear();
                    feature_spec spec(-1);
                    spec.glyph = '.';
                    km.feat.feats.insert(km.feat.feats.begin(), spec);

                    int slot = map_def::item_array_glyph_to_slot(c);
                    km.item.set_from_slot(vmap.items, slot);
                }
                else
                {
                    // Normal glyph.  Turn into a feature keyspec.
                    // This is valid for non-array items and monsters
                    // as well, e.g. '$' and '8'.
                    keyed_mapspec &km = keyspecs[idx];
                    km.key_glyph = SUBVAULT_GLYPH;
                    km.feat.feats.clear();

                    feature_spec spec(-1);
                    spec.glyph = c;
                    km.feat.feats.insert(km.feat.feats.begin(), spec);
                }
            }

            // Finally, handle merging the cell itself.

            // Glyph becomes SUBVAULT_GLYPH.  (The old glyph gets merged into a
            // keyspec, above).  This is so that the glyphs that are included
            // from a subvault are immutable by the parent vault.  Otherwise,
            // latent transformations (like KMONS or KITEM) from the parent
            // vault might confusingly modify a glyph from the subvault.
            //
            // NOTE: It'd be possible to allow subvaults to be modified by the
            // parent vault, but KMONS/KITEM/KFEAT/MONS/ITEM would have to
            // apply immediately instead of latently.  They would also then
            // need to be stored per-coord, rather than per-glyph.
            (*this)(x, y) = SUBVAULT_GLYPH;

            // Merge overlays
            if (vlines.overlay.get())
            {
                (*overlay)(x, y) = (*vlines.overlay)(vx, vy);
            }
            else
            {
                // Erase any existing overlay, as the vault's doesn't exist.
                (*overlay)(x, y) = overlay_def();
            }

            // Set keyspec index for this subvault.
            (*overlay)(x, y).keyspec_idx = idx;
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
        while ((pos = lines[y].find_first_of(spec.key, pos)) != std::string::npos)
        {
            if (spec.floor)
                (*overlay)(pos, y).floortile = spec.get_tile();
            else if (spec.feat)
                (*overlay)(pos, y).tile = spec.get_tile();
            else
                (*overlay)(pos, y).rocktile = spec.get_tile();

            (*overlay)(pos, y).no_random = spec.no_random;
            (*overlay)(pos, y).last_tile = spec.last_tile;
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
        while ((pos = lines[y].find_first_of(spec.key, pos)) != std::string::npos)
            positions.push_back(coord_def(pos++, y));
    }
    std::random_shuffle(positions.begin(), positions.end(), random2);

    int pcount = 0;
    const int psize = positions.size();
    for (int i = 0, vsize = spec.specs.size();
         i < vsize && pcount < psize; ++i)
    {
        const int nsubsts = spec.specs[i].count;
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

keyed_mapspec *map_lines::mapspec_for_key(int key)
{
    keyed_specs::iterator i = keyspecs.find(key);
    return i != keyspecs.end()? &i->second : NULL;
}

const keyed_mapspec *map_lines::mapspec_for_key(int key) const
{
    keyed_specs::const_iterator i = keyspecs.find(key);
    return i != keyspecs.end()? &i->second : NULL;
}

keyed_mapspec *map_lines::mapspec_at(const coord_def &c)
{
    int key = (*this)(c);

    if (key == SUBVAULT_GLYPH)
    {
        // Any subvault should create the overlay.
        ASSERT(overlay.get());
        if (!overlay.get())
            return NULL;

        key = (*overlay)(c.x, c.y).keyspec_idx;
        ASSERT(key);
        if (!key)
            return NULL;
    }

    return (mapspec_for_key(key));
}

const keyed_mapspec *map_lines::mapspec_at(const coord_def &c) const
{
    int key = (*this)(c);

    if (key == SUBVAULT_GLYPH)
    {
        // Any subvault should create the overlay and set the keyspec idx.
        ASSERT(overlay.get());
        if (!overlay.get())
            return NULL;

        key = (*overlay)(c.x, c.y).keyspec_idx;
        ASSERT(key);
        if (!key)
            return NULL;
    }

    return (mapspec_for_key(key));
}

std::string map_lines::add_key_field(
    const std::string &s,
    std::string (keyed_mapspec::*set_field)(const std::string &s, bool fixed),
    void (keyed_mapspec::*copy_field)(const keyed_mapspec &spec))
{
    int separator = 0;
    std::string key, arg;

    std::string err = mapdef_split_key_item(s, &key, &separator, &arg, -1);
    if (!err.empty())
        return (err);

    keyed_mapspec &kmbase = keyspecs[key[0]];
    kmbase.key_glyph = key[0];
    err = ((kmbase.*set_field)(arg, separator == ':'));
    if (!err.empty())
        return (err);

    size_t len = key.length();
    for (size_t i = 1; i < len; i++)
    {
        keyed_mapspec &km = keyspecs[key[i]];
        km.key_glyph = key[i];
        ((km.*copy_field)(kmbase));
    }

    return (err);
}

std::string map_lines::add_key_item(const std::string &s)
{
    return add_key_field(s, &keyed_mapspec::set_item,
                         &keyed_mapspec::copy_item);
}

std::string map_lines::add_key_feat(const std::string &s)
{
    return add_key_field(s, &keyed_mapspec::set_feat,
                         &keyed_mapspec::copy_feat);
}

std::string map_lines::add_key_mons(const std::string &s)
{
    return add_key_field(s, &keyed_mapspec::set_mons,
                         &keyed_mapspec::copy_mons);
}

std::string map_lines::add_key_mask(const std::string &s)
{
    return add_key_field(s, &keyed_mapspec::set_mask,
                         &keyed_mapspec::copy_mask);
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

std::vector<coord_def> map_lines::find_glyph(const std::string &glyphs) const
{
    std::vector<coord_def> points;
    for (int y = height() - 1; y >= 0; --y)
    {
        for (int x = width() - 1; x >= 0; --x)
        {
            const coord_def c(x, y);
            if (glyphs.find((*this)(c)) != std::string::npos)
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

bool map_lines::find_bounds(int gly, coord_def &tl, coord_def &br) const
{
    tl = coord_def(width(), height());
    br = coord_def(-1, -1);

    if (width() == 0 || height() == 0)
        return false;

    for (rectangle_iterator ri(get_iter()); ri; ++ri)
    {
        const coord_def mc = *ri;
        if ((*this)(mc) != gly)
            continue;

        tl.x = std::min(tl.x, mc.x);
        tl.y = std::min(tl.y, mc.y);
        br.x = std::max(br.x, mc.x);
        br.y = std::max(br.y, mc.y);
    }

    return (br.x >= 0);
}

bool map_lines::find_bounds(const char *str, coord_def &tl, coord_def &br) const
{
    tl = coord_def(width(), height());
    br = coord_def(-1, -1);

    if (width() == 0 || height() == 0)
        return false;

    for (rectangle_iterator ri(get_iter()); ri; ++ri)
    {
        ASSERT(ri);
        const coord_def &mc = *ri;
        const size_t len = strlen(str);
        for (size_t i = 0; i < len; ++i)
        {
            if ((*this)(mc) == str[i])
            {
                tl.x = std::min(tl.x, mc.x);
                tl.y = std::min(tl.y, mc.y);
                br.x = std::max(br.x, mc.x);
                br.y = std::max(br.y, mc.y);
                break;
            }
        }
    }

    return (br.x >= 0);
}

bool map_lines::fill_zone(travel_distance_grid_t &tpd, const coord_def &start,
                          const coord_def &tl, const coord_def &br, int zone,
                          const char *wanted, const char *passable) const
{
    // This is the map_lines equivalent of _dgn_fill_zone.
    // It's unfortunately extremely similar, but not close enough to combine.

    bool ret = false;
    std::list<coord_def> points[2];
    int cur = 0;

    for (points[cur].push_back(start); !points[cur].empty(); )
    {
        for (std::list<coord_def>::const_iterator i = points[cur].begin();
             i != points[cur].end(); ++i)
        {
            const coord_def &c(*i);

            tpd[c.x][c.y] = zone;

            ret |= (wanted && strchr(wanted, (*this)(c)) != NULL);

            for (int yi = -1; yi <= 1; ++yi)
                for (int xi = -1; xi <= 1; ++xi)
                {
                    if (!xi && !yi)
                        continue;

                    const coord_def cp(c.x + xi, c.y + yi);
                    if (cp.x < tl.x || cp.x > br.x
                        || cp.y < tl.y || cp.y > br.y
                        || !in_bounds(cp) || tpd[cp.x][cp.y]
                        || passable && !strchr(passable, (*this)(cp)))
                    {
                        continue;
                    }

                    tpd[cp.x][cp.y] = zone;
                    points[!cur].push_back(cp);
                }
        }

        points[cur].clear();
        cur = !cur;
    }
    return (ret);
}

int map_lines::count_feature_in_box(const coord_def &tl, const coord_def &br,
                                    const char *feat) const
{
    int result = 0;
    for (rectangle_iterator ri(tl, br); ri; ++ri)
    {
        if (strchr(feat, (*this)(*ri)))
            result++;
    }

    return (result);
}

#ifdef USE_TILE
bool map_tile_list::parse(const std::string &s, int weight)
{
    tileidx_t idx = 0;
    if (s != "none" && !tile_dngn_index(s.c_str(), &idx))
        return false;

    push_back(map_weighted_tile(idx, weight));
    return true;
}

std::string map_lines::add_tile(const std::string &sub, bool is_floor, bool is_feat)
{
    std::string s = trimmed_string(sub);

    if (s.empty())
        return ("");

    bool no_random = strip_tag(s, "no_random");
    bool last_tile = strip_tag(s, "last_tile");

    int sep = 0;
    std::string key;
    std::string substitute;

    std::string err = mapdef_split_key_item(s, &key, &sep, &substitute, -1);
    if (!err.empty())
        return (err);

    map_tile_list list;
    err = parse_weighted_str<map_tile_list>(substitute, list);
    if (!err.empty())
        return (err);

    tile_spec spec(key, sep == ':', no_random, last_tile, is_floor, is_feat, list);
    overlay_tiles(spec);

    return ("");
}

std::string map_lines::add_rocktile(const std::string &sub)
{
    return add_tile(sub, false, false);
}

std::string map_lines::add_floortile(const std::string &sub)
{
    return add_tile(sub, true, false);
}

std::string map_lines::add_spec_tile(const std::string &sub)
{
    return add_tile(sub, false, true);
}

//////////////////////////////////////////////////////////////////////////
// tile_spec

tileidx_t tile_spec::get_tile()
{
    if (chose_fixed)
        return fixed_tile;

    tileidx_t chosen = 0;
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

//////////////////////////////////////////////////////////////////////////
// map_lines::iterator

map_lines::iterator::iterator(map_lines &_maplines, const std::string &_key)
    : maplines(_maplines), key(_key), p(0, 0)
{
    advance();
}

void map_lines::iterator::advance()
{
    const int height = maplines.height();
    while (p.y < height)
    {
        std::string::size_type place = p.x;
        if (place < maplines.lines[p.y].length())
        {
            place = maplines.lines[p.y].find_first_of(key, place);
            if (place != std::string::npos)
            {
                p.x = place;
                break;
            }
        }
        ++p.y;
        p.x = 0;
    }
}

map_lines::iterator::operator bool() const
{
    return (p.y < maplines.height());
}

coord_def map_lines::iterator::operator *() const
{
    return (p);
}

coord_def map_lines::iterator::operator ++ ()
{
    p.x++;
    advance();
    return **this;
}

coord_def map_lines::iterator::operator ++ (int)
{
    coord_def here(**this);
    ++*this;
    return (here);
}

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
    : name(), description(), tags(), place(), depths(), orient(), chance(),
      weight(), weight_depth_mult(), weight_depth_div(), welcome_messages(),
      map(), mons(), items(), random_mons(), prelude("dlprelude"),
      mapchunk("dlmapchunk"), main("dlmain"),
      validate("dlvalidate"), veto("dlveto"), epilogue("dlepilogue"),
      rock_colour(BLACK), floor_colour(BLACK), rock_tile(0), floor_tile(0),
      border_fill_type(DNGN_ROCK_WALL), index_only(false), cache_offset(0L),
      validating_map_flag(false)
{
    init();
}

void map_def::init()
{
    orient = MAP_NONE;
    name.clear();
    description.clear();
    tags.clear();
    place.clear();
    depths.clear();
    prelude.clear();
    mapchunk.clear();
    main.clear();
    validate.clear();
    veto.clear();
    epilogue.clear();
    place_loaded_from.clear();
    reinit();

    // Subvault mask set and cleared externally.
    // It should *not* be in reinit.
    svmask = NULL;
}

void map_def::reinit()
{
    description.clear();
    items.clear();
    random_mons.clear();
    level_flags.clear();
    branch_flags.clear();

    welcome_messages.clear();

    rock_colour = floor_colour = BLACK;
    rock_tile = floor_tile = 0;
    border_fill_type = DNGN_ROCK_WALL;

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

bool map_def::valid_item_array_glyph(int gly)
{
    return (gly >= 'd' && gly <= 'k');
}

int map_def::item_array_glyph_to_slot(int gly)
{
    ASSERT(map_def::valid_item_array_glyph(gly));
    return (gly - 'd');
}

bool map_def::valid_monster_glyph(int gly)
{
    return (gly >= '0' && gly <= '9');
}

bool map_def::valid_monster_array_glyph(int gly)
{
    return (gly >= '1' && gly <= '7');
}

int map_def::monster_array_glyph_to_slot(int gly)
{
    ASSERT(map_def::valid_monster_array_glyph(gly));
    return (gly - '1');
}

bool map_def::in_map(const coord_def &c) const
{
    return map.in_map(c);
}

int map_def::glyph_at(const coord_def &c) const
{
    return map(c);
}

std::string map_def::desc_or_name() const
{
    return (description.empty()? name : description);
}

void map_def::write_full(writer& outf) const
{
    cache_offset = outf.tell();
    marshallShort(outf, MAP_CACHE_VERSION);   // Level indicator.
    marshallString4(outf, name);
    prelude.write(outf);
    mapchunk.write(outf);
    main.write(outf);
    validate.write(outf);
    veto.write(outf);
    epilogue.write(outf);
}

void map_def::read_full(reader& inf, bool check_cache_version)
{
    // There's a potential race-condition here:
    // - If someone modifies a .des file while there are games in progress,
    // - a new Crawl process will overwrite the .dsc.
    // - older Crawl processes trying to reading the new .dsc will be hosed.
    // We could try to recover from the condition (by locking and
    // reloading the index), but it's easier to save the game at this
    // point and let the player reload.

    const short fp_version = unmarshallShort(inf);

    if (check_cache_version && fp_version != MAP_CACHE_VERSION)
        throw map_load_exception(name);

    std::string fp_name;
    unmarshallString4(inf, fp_name);

    if (fp_name != name)
        throw map_load_exception(name);

    prelude.read(inf);
    mapchunk.read(inf);
    main.read(inf);
    validate.read(inf);
    veto.read(inf);
    epilogue.read(inf);
}

void map_def::strip()
{
    if (index_only)
        return;

    index_only = true;
    map.clear();
    mons.clear();
    items.clear();
    random_mons.clear();
    prelude.clear();
    mapchunk.clear();
    main.clear();
    validate.clear();
    veto.clear();
    epilogue.clear();
}

void map_def::load()
{
    if (!index_only)
        return;

    const std::string descache_base = get_descache_path(file, "");

    file_lock deslock(descache_base + ".lk", "rb", false);
    const std::string loadfile = descache_base + ".dsc";

    reader inf(loadfile);
    if (!inf.valid())
        throw map_load_exception(name);
    inf.advance(cache_offset);
    read_full(inf, true);

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

void map_def::write_maplines(writer &outf) const
{
    map.write_maplines(outf);
}

void map_def::write_index(writer& outf) const
{
    if (!cache_offset)
        end(1, false, "Map %s: can't write index - cache offset not set!",
            name.c_str());
    marshallString4(outf, name);
    marshallString4(outf, place_loaded_from.filename);
    marshallInt(outf, place_loaded_from.lineno);
    marshallShort(outf, orient);
    // XXX: This is a hack. See the comment in l_dgn.cc.
    marshallShort(outf, static_cast<short>(border_fill_type));
    marshallInt(outf, chance_priority);
    marshallInt(outf, chance);
    marshallInt(outf, weight);
    marshallInt(outf, cache_offset);
    marshallString4(outf, tags);
    place.save(outf);
    write_depth_ranges(outf);
    prelude.write(outf);
}

void map_def::read_maplines(reader &inf)
{
    map.read_maplines(inf);
}

void map_def::read_index(reader& inf)
{
    unmarshallString4(inf, name);
    unmarshallString4(inf, place_loaded_from.filename);
    place_loaded_from.lineno   = unmarshallInt(inf);
    orient       = static_cast<map_section_type>( unmarshallShort(inf) );
    // XXX: Hack. See the comment in l_dgn.cc.
    border_fill_type =
        static_cast<dungeon_feature_type>( unmarshallShort(inf) );
    chance_priority = unmarshallInt(inf);
    chance       = unmarshallInt(inf);
    weight       = unmarshallInt(inf);
    cache_offset = unmarshallInt(inf);
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
    mapchunk.set_file(s);
    main.set_file(s);
    validate.set_file(s);
    veto.set_file(s);
    epilogue.set_file(s);
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
    {
        lua_pushnil(dlua);
        lua_pushnil(dlua);
    }
    else
    {
        err = mapchunk.load(dlua);
        if (err == -1000)
            lua_pushnil(dlua);
        else if (err)
            return (mapchunk.orig_error());

        err = main.load(dlua);
        if (err == -1000)
            lua_pushnil(dlua);
        else if (err)
            return (main.orig_error());
    }

    if (!dlua.callfn("dgn_run_map", 3, 0))
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
            end(1, false, "Lua error: %s", chunk.orig_error().c_str());
        else
            mprf(MSGCH_ERROR, "Lua error: %s", chunk.orig_error().c_str());
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

bool map_def::run_lua_epilogue (bool croak)
{
    return !epilogue.empty() && test_lua_boolchunk(epilogue, false, croak);
}

std::string map_def::rewrite_chunk_errors(const std::string &s) const
{
    std::string res = s;
    if (prelude.rewrite_chunk_errors(res))
        return (res);
    if (mapchunk.rewrite_chunk_errors(res))
        return (res);
    if (main.rewrite_chunk_errors(res))
        return (res);
    if (validate.rewrite_chunk_errors(res))
        return (res);
    if (veto.rewrite_chunk_errors(res))
        return (res);
    epilogue.rewrite_chunk_errors(res);
    return (res);
}

std::string map_def::validate_temple_map()
{
    std::vector<coord_def> altars = find_glyph('B');

    if (has_tag_prefix("temple_overflow_"))
    {
        std::vector<std::string> tag_list = get_tags();
        std::string              temple_tag;

        for (unsigned int i = 0; i < tag_list.size(); i++)
        {
            if (starts_with(tag_list[i], "temple_overflow_"))
            {
                temple_tag = tag_list[i];
                break;
            }
        }

        if (temple_tag.empty())
            return make_stringf("Unknown temple tag.");

        temple_tag = strip_tag_prefix(temple_tag, "temple_overflow_");

        if (temple_tag.empty())
            return ("Malformed temple_overflow_ tag");

        int num = atoi(temple_tag.c_str());

        if (num == 0)
        {
            temple_tag = replace_all(temple_tag, "_", " ");

            god_type god = str_to_god(temple_tag);

            if (god == GOD_NO_GOD)
            {
                return make_stringf("Invalid god name '%s'",
                                    temple_tag.c_str());
            }

            // Assume that specialized single-god temples are set up
            // properly.
            return("");
        }
        else
        {
            if ( ( (unsigned long) num ) != altars.size() )
            {
                return make_stringf("Temple should contain %u altars, but "
                                    "has %d.", altars.size(), num);
            }
        }
    }

    if (altars.empty())
        return ("Temple vault must contain at least one altar.");

    // TODO: check for substitutions and shuffles

    std::vector<coord_def> b_glyphs = map.find_glyph('B');
    for (std::vector<coord_def>::iterator i = b_glyphs.begin();
        i != b_glyphs.end(); ++i)
    {
        const keyed_mapspec *spec = map.mapspec_at(*i);
        if (spec != NULL && spec->feat.feats.size() > 0)
            return ("Can't change feat 'B' in temple (KFEAT)");
    }

    std::vector<god_type> god_list = temple_god_list();

    if (altars.size() > god_list.size())
        return ("Temple vault has too many altars");

    return ("");
}

std::string map_def::validate_map_placeable()
{
    if (has_depth() || place.is_valid())
        return ("");

    // Ok, the map wants to be placed by tag. In this case it should have
    // at least one tag that's not a map flag.
    const std::vector<std::string> tag_pieces = split_string(" ", tags);
    bool has_selectable_tag = false;
    for (int i = 0, tsize = tag_pieces.size(); i < tsize; ++i)
    {
        if (map_tag_is_selectable(tag_pieces[i]))
        {
            has_selectable_tag = true;
            break;
        }
    }

    return (has_selectable_tag? "" :
            make_stringf("Map '%s' has no DEPTH, no PLACE and no "
                         "selectable tag in '%s'",
                         name.c_str(), tags.c_str()));
}

std::string map_def::validate_map_def(const depth_ranges &default_depths)
{
    unwind_bool valid_flag(validating_map_flag, true);

    std::string err = run_lua(true);
    if (!err.empty())
        return (err);

    fixup();
    resolve();
    test_lua_validate(true);

    if (!has_depth() && !lc_default_depths.empty())
        add_depths(lc_default_depths.begin(),
                   lc_default_depths.end());

    if ((place.branch == BRANCH_ECUMENICAL_TEMPLE
         && place.level_type == LEVEL_DUNGEON)
        || has_tag_prefix("temple_overflow_"))
    {
        err = validate_temple_map();
        if (!err.empty())
            return err;
    }

    // Abyssal vaults have additional size and orientation restrictions.
    if (has_tag("abyss") || has_tag("abyss_rune"))
    {
        if (orient == MAP_ENCOMPASS)
            return make_stringf(
                "Map '%s' cannot use 'encompass' orientation in the abyss",
                name.c_str());

        const int max_abyss_map_width =
            GXM / 2 - MAPGEN_BORDER - ABYSS_AREA_SHIFT_RADIUS;
        const int max_abyss_map_height =
            GYM / 2 - MAPGEN_BORDER - ABYSS_AREA_SHIFT_RADIUS;

        if (map.width() > max_abyss_map_width
            || map.height() > max_abyss_map_height)
        {
            return make_stringf(
                "Map '%s' is too big for the Abyss: %dx%d - max %dx%d",
                name.c_str(),
                map.width(), map.height(),
                max_abyss_map_width, max_abyss_map_height);
        }

        // Unless both height and width fit in the smaller dimension,
        // map rotation will be disallowed.
        const int dimension_lower_bound =
            std::min(max_abyss_map_height, max_abyss_map_width);
        if ((map.width() > dimension_lower_bound
             || map.height() > dimension_lower_bound)
            && !has_tag("no_rotate"))
        {
            tags += " no_rotate ";
        }
    }

    if (orient == MAP_FLOAT || is_minivault())
    {
        if (map.width() > GXM - MAPGEN_BORDER * 2
            || map.height() > GYM - MAPGEN_BORDER * 2)
        {
            return make_stringf(
                     "%s '%s' is too big: %dx%d - max %dx%d",
                     is_minivault()? "Minivault" : "Float",
                     name.c_str(),
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
                     "Map '%s' is too big: %dx%d - max %dx%d",
                     name.c_str(),
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
    return (validate_map_placeable());
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

// Returns true if the map is a layout that allows other vaults to be
// built on it.
bool map_def::is_overwritable_layout() const
{
    return (has_tag("layout") && !has_tag("sealed_layout"));
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
         "Aligning floating vault with %u points vs %u reference points",
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
    return ("");
}

void map_def::fixup()
{
    normalise();

    // Fixup minivaults into floating vaults tagged "minivault".
    if (orient == MAP_NONE)
    {
        orient = MAP_FLOAT;
        tags += " minivault ";
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

keyed_mapspec *map_def::mapspec_at(const coord_def &c)
{
    return (map.mapspec_at(c));
}

const keyed_mapspec *map_def::mapspec_at(const coord_def &c) const
{
    return (map.mapspec_at(c));
}

std::string map_def::subvault_from_tagstring(const std::string &sub)
{
    std::string s = trimmed_string(sub);

    if (s.empty())
        return ("");

    int sep = 0;
    std::string key;
    std::string substitute;

    std::string err = mapdef_split_key_item(sub, &key, &sep, &substitute, -1);
    if (!err.empty())
        return (err);

    // Randomly picking a different vault per-glyph is not supported.
    if (sep != ':')
        return ("SUBVAULT does not support '='.  Use ':' instead.");

    map_string_list vlist;
    err = parse_weighted_str<map_string_list>(substitute, vlist);
    if (!err.empty())
        return (err);

    bool fix = false;
    string_spec spec(key, fix, vlist);

    // Although it's unfortunate to not be able to validate subvaults except a
    // run-time, this allows subvaults to reference maps by tag that may not
    // have been loaded yet.
    if (!is_validating())
        err = apply_subvault(spec);

    if (!err.empty())
        return (err);

    return ("");
}

std::string map_def::apply_subvault(string_spec &spec)
{
    // Find bounding box for key glyphs
    coord_def tl, br;
    if (!map.find_bounds(spec.key.c_str(), tl, br))
    {
        // No glyphs, so do nothing.
        return ("");
    }

    int vwidth = br.x - tl.x + 1;
    int vheight = br.y - tl.y + 1;
    Matrix<bool> flags(vwidth, vheight);
    map.fill_mask_matrix(spec.key, tl, br, flags);

    // Backup pre-subvault unique tags and names.
    const std::set<std::string> uniq_tags  = you.uniq_map_tags;
    const std::set<std::string> uniq_names = you.uniq_map_names;

    const int max_tries = 100;
    int ntries = 0;

    std::string tag = spec.get_property();
    while (++ntries <= max_tries)
    {
        // Each iteration, restore tags and names.  This is because this vault
        // may successfully load a subvault (registering its tag and name), but
        // then itself fail.
        you.uniq_map_tags = uniq_tags;
        you.uniq_map_names = uniq_names;

        const map_def *orig = random_map_for_tag(tag);
        if (!orig)
            return (make_stringf("No vault found for tag '%s'", tag.c_str()));

        map_def vault = *orig;

        vault.load();

        // Temporarily set the subvault mask so this subvault can know
        // that it is being generated as a subvault.
        vault.svmask = &flags;

        if (!resolve_subvault(vault))
            continue;

        ASSERT(vault.map.width() <= vwidth);
        ASSERT(vault.map.height() <= vheight);

        map.merge_subvault(tl, br, flags, vault);

        dgn_register_vault(vault);

        return ("");
    }

    // Failure, restore original unique tags and names.
    you.uniq_map_tags = uniq_tags;
    you.uniq_map_names = uniq_names;

    return (make_stringf("Could not fit '%s' in (%d,%d) to (%d, %d).",
                         tag.c_str(), tl.x, tl.y, br.x, br.y));
}

bool map_def::is_subvault() const
{
    return (svmask != NULL);
}

void map_def::apply_subvault_mask()
{
    if (!svmask)
        return;

    map.clear();
    map.extend(subvault_width(), subvault_height(), ' ');

    for (rectangle_iterator ri(map.get_iter()); ri; ++ri)
    {
        const coord_def mc = *ri;
        if (subvault_cell_valid(mc))
            map(mc) = '.';
        else
            map(mc) = ' ';
    }
}

bool map_def::subvault_cell_valid(const coord_def &c) const
{
    if (!svmask)
        return false;

    if (c.x < 0 || c.x >= subvault_width()
        || c.y < 0 || c.y >= subvault_height())
    {
        return false;
    }

    return (*svmask)(c.x, c.y);
}

int map_def::subvault_width() const
{
    if (!svmask)
        return 0;

    return (svmask->width());
}

int map_def::subvault_height() const
{
    if (!svmask)
        return 0;

    return (svmask->height());
}

int map_def::subvault_mismatch_count(const coord_def &offset) const
{
    int count = 0;
    if (!is_subvault())
        return (count);

    for (rectangle_iterator ri(map.get_iter()); ri; ++ri)
    {
        // Coordinate in the subvault
        const coord_def sc = *ri;
        // Coordinate in the mask
        const coord_def mc = sc + offset;

        bool valid_subvault_cell = (map(sc) != ' ');
        bool valid_mask = (*svmask)(mc.x, mc.y);

        if (valid_subvault_cell && !valid_mask)
            count++;
    }

    return (count);
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

    return (summon_any_demon(static_cast<demon_class_type>(demon)));
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
        slot.mlist.push_back(pick);
        slot.fix_slot = false;
    }

    if (pick.mid == MONS_WEAPON_MIMIC && !pick.fix_mons)
        pick.mid = random_range(MONS_GOLD_MIMIC, MONS_POTION_MIMIC);

    return (pick);
}

mons_spec mons_list::get_monster(int index)
{
    if (index < 0 || index >= (int)mons.size())
        return (mons_spec(RANDOM_MONSTER));

    return (pick_monster(mons[index]));
}

mons_spec mons_list::get_monster(int slot_index, int list_index) const
{
    if (slot_index < 0 || slot_index >= (int)mons.size())
        return (mons_spec(RANDOM_MONSTER));

    const mons_spec_list &list = mons[slot_index].mlist;

    if (list_index < 0 || list_index >= (int)list.size())
        return (mons_spec(RANDOM_MONSTER));

    return (list[list_index]);
}

void mons_list::clear()
{
    mons.clear();
}

void mons_list::set_from_slot(const mons_list &list, int slot_index)
{
    clear();

    // Don't set anything if an invalid index.
    // Future calls to get_monster will just return a random monster.
    if (slot_index < 0 || (size_t)slot_index >= list.mons.size())
        return;

    mons.push_back(list.mons[slot_index]);
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

void mons_list::parse_mons_spells(mons_spec &spec, const std::string &spells)
{
    spec.explicit_spells = true;
    spec.extra_monster_flags |= MF_SPELLCASTER;
    const std::vector<std::string> spell_names(split_string(";", spells));
    if (spell_names.size() > NUM_MONSTER_SPELL_SLOTS)
    {
        error = make_stringf("Too many monster spells (max %d) in %s",
                             NUM_MONSTER_SPELL_SLOTS,
                             spells.c_str());
        return;
    }
    for (unsigned i = 0, ssize = spell_names.size(); i < ssize; ++i)
    {
        const std::string spname(
            lowercase_string(replace_all_of(spell_names[i], "_", " ")));
        if (spname.empty() || spname == "." || spname == "none"
            || spname == "no spell")
        {
            spec.spells[i] = SPELL_NO_SPELL;
        }
        else
        {
            const spell_type sp(spell_by_name(spname));
            if (sp == SPELL_NO_SPELL)
            {
                error = make_stringf("Unknown spell name: '%s' in '%s'",
                                     spname.c_str(), spells.c_str());
                return;
            }
            if (!is_valid_mon_spell(sp))
            {
                error = make_stringf("Not a monster spell: '%s'",
                                     spname.c_str());
                return;
             }
            spec.spells[i] = sp;
        }
    }
}

mons_list::mons_spec_slot mons_list::parse_mons_spec(std::string spec)
{
    mons_spec_slot slot;

    slot.fix_slot = strip_tag(spec, "fix_slot");

    std::vector<std::string> specs = split_string("/", spec);

    for (int i = 0, ssize = specs.size(); i < ssize; ++i)
    {
        mons_spec mspec;
        std::string s = specs[i];

        std::string spells(strip_tag_prefix(s, "spells:"));
        if (!spells.empty())
        {
            parse_mons_spells(mspec, spells);
            if (!error.empty())
                return (slot);
        }

        std::vector<std::string> parts = split_string(";", s);
        std::string mon_str = parts[0];

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

        if (strip_tag(mon_str, "priest_spells"))
        {
            mspec.extra_monster_flags &= ~MF_ACTUAL_SPELLS;
            mspec.extra_monster_flags |= MF_PRIEST;
        }

        if (strip_tag(mon_str, "actual_spells"))
        {
            mspec.extra_monster_flags &= ~MF_PRIEST;
            mspec.extra_monster_flags |= MF_ACTUAL_SPELLS;
        }

        mspec.fix_mons       = strip_tag(mon_str, "fix_mons");
        mspec.generate_awake = strip_tag(mon_str, "generate_awake");
        mspec.patrolling     = strip_tag(mon_str, "patrolling");
        mspec.band           = strip_tag(mon_str, "band");

        const std::string att = strip_tag_prefix(mon_str, "att:");
        if (att.empty() || att == "hostile")
            mspec.attitude = ATT_HOSTILE;
        else if (att == "friendly")
            mspec.attitude = ATT_FRIENDLY;
        else if (att == "good_neutral")
            mspec.attitude = ATT_GOOD_NEUTRAL;
        else if (att == "fellow_slime" || att == "strict_neutral")
            mspec.attitude = ATT_STRICT_NEUTRAL;
        else if (att == "neutral")
            mspec.attitude = ATT_NEUTRAL;

        // Useful for summoned monsters.
        if (strip_tag(mon_str, "seen"))
            mspec.extra_monster_flags |= MF_SEEN;

        if (strip_tag(mon_str, "always_corpse"))
            mspec.props["always_corpse"] = true;

        if (!mon_str.empty() && isadigit(mon_str[0]))
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

        mspec.hd = strip_number_tag(mon_str, "hd:");
        if (mspec.hd == TAG_UNFOUND)
            mspec.hd = 0;

        mspec.hp = strip_number_tag(mon_str, "hp:");
        if (mspec.hp == TAG_UNFOUND)
            mspec.hp = 0;

        int dur = strip_number_tag(mon_str, "dur:");
        if (dur == TAG_UNFOUND)
            dur = 0;
        else if (dur < 1 || dur > 6)
            dur = 0;

        mspec.abjuration_duration = dur;

        int summon_type = 0;
        std::string s_type = strip_tag_prefix(mon_str, "sum:");
        if (!s_type.empty())
        {
            // In case of spells!
            s_type = replace_all_of(s_type, "_", " ");
            summon_type = static_cast<int>(str_to_summon_type(s_type));
            if (summon_type == SPELL_NO_SPELL)
            {
                error = make_stringf("bad monster summon type: \"%s\"",
                                s_type.c_str());
                return (slot);
            }
            if (mspec.abjuration_duration == 0)
            {
                error = "marked summon with no duration";
                return (slot);
            }
        }

        mspec.summon_type = summon_type;

        std::string non_actor_summoner = strip_tag_prefix(mon_str, "nas:");
        if (!non_actor_summoner.empty())
        {
            non_actor_summoner = replace_all_of(non_actor_summoner, "_", " ");
            mspec.non_actor_summoner = non_actor_summoner;
            if (mspec.abjuration_duration == 0)
            {
                error = "marked summon with no duration";
                return (slot);
            }
        }

        std::string colour = strip_tag_prefix(mon_str, "col:");
        if (!colour.empty())
        {
            if (colour == "any")
                // XXX: Hack
                mspec.colour = -1;
            else
            {
                mspec.colour = str_to_colour(colour, BLACK);
                if (mspec.colour == BLACK)
                {
                    error = make_stringf("bad monster colour \"%s\" in \"%s\"",
                                         colour.c_str(), specs[i].c_str());
                    return (slot);
                }
            }
        }

        std::string tile = strip_tag_prefix(mon_str, "tile:");
#ifdef USE_TILE
        if (!tile.empty())
        {
            tileidx_t index;
            if (!tile_player_index(tile.c_str(), &index))
            {
                error = make_stringf("bad tile name: \"%s\".", tile.c_str());
                return (slot);
            }
            mspec.props["monster_tile"] = short(index);
        }
#endif

        std::string name = strip_tag_prefix(mon_str, "name:");
        if (!name.empty())
        {
            name = replace_all_of(name, "_", " ");
            mspec.monname = name;

            if (strip_tag(mon_str, "name_suffix")
                || strip_tag(mon_str, "n_suf"))
            {
                mspec.extra_monster_flags |= MF_NAME_SUFFIX;
            }
            else if (strip_tag(mon_str, "name_adjective")
                     || strip_tag(mon_str, "n_adj"))
            {
                mspec.extra_monster_flags |= MF_NAME_ADJECTIVE;
            }
            else if (strip_tag(mon_str, "name_replace")
                     || strip_tag(mon_str, "n_rpl"))
            {
                mspec.extra_monster_flags |= MF_NAME_REPLACE;
            }

            // We should be able to combine this with name_replace.
            if (strip_tag(mon_str, "name_descriptor")
                || strip_tag(mon_str, "n_des"))
            {
                mspec.extra_monster_flags |= MF_NAME_DESCRIPTOR;
            }
            // Reasoning for this setting both flags: it does nothing with the
            // description unless NAME_DESCRIPTOR is also set; thus, you end up
            // with bloated vault description lines akin to: "name:blah_blah
            // name_replace name_descrpitor name_definite".
            if (strip_tag(mon_str, "name_definite")
                || strip_tag(mon_str, "n_the"))
            {
                mspec.extra_monster_flags |= MF_NAME_DEFINITE;
                mspec.extra_monster_flags |= MF_NAME_DESCRIPTOR;
            }

            if (strip_tag(mon_str, "name_species")
                || strip_tag(mon_str, "n_spe"))
            {
                mspec.extra_monster_flags |= MF_NAME_SPECIES;
            }
        }

        std::string serpent_of_hell_flavour = strip_tag_prefix(mon_str, "serpent_of_hell_flavour:");
        if (serpent_of_hell_flavour.empty())
            serpent_of_hell_flavour = strip_tag_prefix(mon_str, "soh_flavour:");
        if (!serpent_of_hell_flavour.empty()) {
            serpent_of_hell_flavour = upcase_first(lowercase(serpent_of_hell_flavour)).substr(0, 3);
            mspec.props["serpent_of_hell_flavour"].get_int() = str_to_branch(serpent_of_hell_flavour, BRANCH_GEHENNA);
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
            if (nspec.colour && !mspec.colour)
                mspec.colour = nspec.colour;
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
    {
        slotmons.fix_slot = true;
        pick_monster(slotmons);
    }

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

    int mod = ends_with(s, zombie_types);
    if (!mod)
    {
        const std::string spectre("spectral ");
        if (s.find(spectre) == 0)
        {
            mod = ends_with(" spectre", zombie_types);
            s = s.substr(spectre.length());
        }
        else
        {
            spec.mid = MONS_PROGRAM_BUG;
            return;
        }
    }
    else
    {
        s = s.substr(0, s.length() - strlen(zombie_types[mod - 1]));
    }

    trim_string(s);

    mons_spec base_monster = mons_by_name(s);
    if (base_monster.mid < 0)
        base_monster.mid = MONS_PROGRAM_BUG;
    spec.monbase = static_cast<monster_type>(base_monster.mid);
    spec.number = base_monster.number;

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
        for (int i = 0; i <= 20; ++i)
            if (number_in_words(i) == prefix)
            {
                nheads = i;
                break;
            }
    }

    if (nheads < 1)
        nheads = 27;  // What can I say? :P
    else if (nheads > 20)
    {
#if defined(DEBUG) || defined(DEBUG_DIAGNOSTICS)
        mprf(MSGCH_DIAGNOSTICS, "Hydra spec wants %d heads, clamping to 20.",
             nheads);
#endif
        nheads = 20;
    }

    return (mons_spec(MONS_HYDRA, MONS_NO_MONSTER, nheads));
}

mons_spec mons_list::get_slime_spec(const std::string &name) const
{
    std::string prefix = name.substr(0, name.find(" slime creature"));

    int slime_size = 1;

    if (prefix == "large")
        slime_size = 2;
    else if (prefix == "very large")
        slime_size = 3;
    else if (prefix == "enormous")
        slime_size = 4;
    else if (prefix == "titanic")
        slime_size = 5;
    else
    {
#if defined(DEBUG) || defined(DEBUG_DIAGNOSTICS)
        mprf(MSGCH_DIAGNOSTICS, "Slime spec wants invalid size '%s'",
             prefix.c_str());
#endif
     }

    return (mons_spec(MONS_SLIME_CREATURE, MONS_NO_MONSTER, slime_size));
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

    if (ends_with(name, "-headed hydra") && !starts_with(name, "spectral "))
        return (get_hydra_spec(name));

    if (ends_with(name, " slime creature"))
        return (get_slime_spec(name));

    if (name.find(" ugly thing") != std::string::npos)
    {
        const std::string::size_type wordend = name.find(' ');
        const std::string first_word = name.substr(0, wordend);

        const int colour = str_to_ugly_thing_colour(first_word);
        if (colour)
        {
            mons_spec spec = mons_by_name(name.substr(wordend + 1));
            spec.colour = colour;
            return (spec);
        }
    }

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

bool item_spec::corpselike() const
{
    return ((base_type == OBJ_CORPSES && (sub_type == CORPSE_BODY
                                          || sub_type == CORPSE_SKELETON))
            || (base_type == OBJ_FOOD && sub_type == FOOD_CHUNK));
}

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
        {
            sp.fix_slot = true;
            pick_item(sp);
        }

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

void item_list::set_from_slot(const item_list &list, int slot_index)
{
    clear();

    // Don't set anything if an invalid index.
    // Future calls to get_item will just return no item.
    if (slot_index < 0 || (size_t)slot_index >= list.items.size())
        return;

    items.push_back(list.items[slot_index]);
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
        "spirit shield",
        "archery",
        NULL
    };
    COMPILE_CHECK(ARRAYSZ(armour_egos) == NUM_SPECIAL_ARMOURS,
                  cc_armour_ego);

    const char* weapon_brands[] = {
        "flaming",
        "freezing",
        "holy_wrath",
        "electrocution",
        "orc_slaying",
        "dragon_slaying",
        "venom",
        "protection",
        "draining",
        "speed",
        "vorpal",
        "flame",
        "frost",
        "vampiricism",
        "pain",
        "anti-magic",
        "distortion",
        "reaching",
        "returning",
        "chaos",
        "evasion",
        "confuse",
        "penetration",
        "reaping",
        NULL
    };
    COMPILE_CHECK(ARRAYSZ(weapon_brands) == NUM_REAL_SPECIAL_WEAPONS,
                  cc_weapon_brands);

    const char* missile_brands[] = {
        "flame",
        "ice",
        "poisoned",
        "curare",
        "returning",
        "chaos",
        "penetration",
        "reaping",
        "dispersal",
        "exploding",
        "steel",
        "silver",
        "paralysis",
        "slow",
        "sleep",
        "confusion",
        "sickness",
        "wrath",
        NULL
    };
    COMPILE_CHECK(ARRAYSZ(missile_brands) == NUM_SPECIAL_MISSILES,
                  cc_missile_brands);

    const char** name_lists[3] = {armour_egos, weapon_brands, missile_brands};

    int armour_order[3]  = {0, 1, 2};
    int weapon_order[3]  = {1, 0, 2};
    int missile_order[3] = {2, 0, 1};

    int *order;

    switch (spec.base_type)
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

int item_list::parse_acquirement_source(const std::string &source)
{
    const std::string god_name(replace_all_of(source, "_", " "));
    const god_type god(str_to_god(god_name));
    if (god == GOD_NO_GOD)
        error = make_stringf("unknown god name: '%s'", god_name.c_str());
    return (god);
}

bool item_list::monster_corpse_is_valid(monster_type *mons,
                                        const std::string &name,
                                        bool corpse,
                                        bool skeleton,
                                        bool chunk)
{
    if (*mons == RANDOM_NONBASE_DRACONIAN)
    {
        error = "Can't use non-base draconian for corpse/chunk items";
        return (false);
    }

    // Accept randomised types without further checks:
    if (*mons >= NUM_MONSTERS)
        return (true);

    // Convert to the monster species:
    *mons = mons_species(*mons);

    if (!mons_class_can_leave_corpse(*mons))
    {
        error = make_stringf("'%s' cannot leave corpses", name.c_str());
        return (false);
    }

    if (skeleton && !mons_skeleton(*mons))
    {
        error = make_stringf("'%s' has no skeleton", name.c_str());
        return (false);
    }

    // We're ok.
    return (true);
}

item_spec item_list::parse_corpse_spec(item_spec &result, std::string s)
{
    const bool never_decay = strip_tag(s, "never_decay");

    if (never_decay)
        result.props[CORPSE_NEVER_DECAYS].get_bool() = true;

    const bool corpse = strip_suffix(s, "corpse");
    const bool skeleton = !corpse && strip_suffix(s, "skeleton");
    const bool chunk = !corpse && !skeleton && strip_suffix(s, "chunk");

    result.base_type = chunk? OBJ_FOOD : OBJ_CORPSES;
    result.sub_type  = (chunk  ? static_cast<int>(FOOD_CHUNK) :
                        static_cast<int>(corpse ? CORPSE_BODY :
                                         CORPSE_SKELETON));

    // [ds] We're stuffing the corpse monster into the .plus field to
    // match what we'll eventually do to the corpse item, in the grand
    // WTF-is-this Crawl tradition.

    // Is the caller happy with any corpse?
    if (s == "any")
    {
        result.plus = RANDOM_MONSTER;
        return (result);
    }

    // The caller wants a specific monster, no doubt with the best of
    // motives. Let's indulge them:
    mons_list mlist;
    const std::string mons_parse_err = mlist.add_mons(s, true);
    if (!mons_parse_err.empty())
    {
        error = mons_parse_err;
        return (result);
    }

    // Get the actual monster spec:
    mons_spec spec = mlist.get_monster(0);
    monster_type mtype = static_cast<monster_type>(spec.mid);
    if (!monster_corpse_is_valid(&mtype, s, corpse, skeleton, chunk))
        return (result);

    // Ok, looking good, the caller can have their requested toy.
    result.plus = mtype;
    if (spec.number)
        result.plus2 = spec.number;

    return (result);
}

// Strips the first word from s and returns it.
static std::string _get_and_discard_word(std::string* s) {
    std::string result;
    const size_t spaceloc = s->find(' ');
    if (spaceloc == std::string::npos) {
        result = *s;
        s->clear();
    }
    else
    {
        result = s->substr(0, spaceloc);
        s->erase(0, spaceloc + 1);
    }

    return result;
}

static deck_rarity_type _rarity_string_to_rarity(const std::string& s) {
    if (s == "common")    return DECK_RARITY_COMMON;
    if (s == "plain")     return DECK_RARITY_COMMON; // synonym
    if (s == "rare")      return DECK_RARITY_RARE;
    if (s == "ornate")    return DECK_RARITY_RARE; // synonym
    if (s == "legendary") return DECK_RARITY_LEGENDARY;
    // FIXME: log an error here.
    return DECK_RARITY_COMMON;
}

static misc_item_type _deck_type_string_to_subtype(const std::string& s) {
    if (s == "escape")      return MISC_DECK_OF_ESCAPE;
    if (s == "destruction") return MISC_DECK_OF_DESTRUCTION;
    if (s == "dungeons")    return MISC_DECK_OF_DUNGEONS;
    if (s == "summoning")   return MISC_DECK_OF_SUMMONING;
    if (s == "summonings")  return MISC_DECK_OF_SUMMONING;
    if (s == "wonders")     return MISC_DECK_OF_WONDERS;
    if (s == "punishment")  return MISC_DECK_OF_PUNISHMENT;
    if (s == "war")         return MISC_DECK_OF_WAR;
    if (s == "changes")     return MISC_DECK_OF_CHANGES;
    if (s == "defence")     return MISC_DECK_OF_DEFENCE;
    // FIXME: log an error here.
    return NUM_MISCELLANY;
}

static misc_item_type _random_deck_subtype()
{
    item_def dummy;
    dummy.base_type = OBJ_MISCELLANY;
    while (true)
    {
        dummy.sub_type = random2(NUM_MISCELLANY);

        if (!is_deck(dummy))
            continue;

        if (dummy.sub_type == MISC_DECK_OF_PUNISHMENT)
            continue;

        if ((dummy.sub_type == MISC_DECK_OF_ESCAPE
             || dummy.sub_type == MISC_DECK_OF_DESTRUCTION
             || dummy.sub_type == MISC_DECK_OF_DUNGEONS
             || dummy.sub_type == MISC_DECK_OF_SUMMONING
             || dummy.sub_type == MISC_DECK_OF_WONDERS)
            && !one_chance_in(5))
        {
            continue;
        }

        return static_cast<misc_item_type>(dummy.sub_type);
    }
}

void item_list::build_deck_spec(std::string s, item_spec* spec)
{
    spec->base_type = OBJ_MISCELLANY;
    std::string word = _get_and_discard_word(&s);

    // The deck description can start with either "[rarity] deck..." or
    // just "deck".
    if (word != "deck")
    {
        spec->item_special = _rarity_string_to_rarity(word);
        word = _get_and_discard_word(&s);
    }
    else
    {
        spec->item_special = random_deck_rarity();
    }

    // Error checking.
    if (word != "deck")
    {
        error = make_stringf("Bad spec: %s", s.c_str());
        return;
    }

    word = _get_and_discard_word(&s);
    if (word == "of")
    {
        std::string sub_type_str = _get_and_discard_word(&s);
        int sub_type =
            _deck_type_string_to_subtype(sub_type_str);

        if (sub_type == NUM_MISCELLANY)
        {
            error = make_stringf("Bad deck type: %s", sub_type_str.c_str());
            return;
        }

        spec->sub_type = sub_type;
    }
    else
    {
        spec->sub_type = _random_deck_subtype();
    }
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

    const int fresh = strip_number_tag(s, "fresh:");
    if (fresh != TAG_UNFOUND)
        result.item_special = fresh;
    const int special = strip_number_tag(s, "special:");
    if (special != TAG_UNFOUND)
        result.item_special = special;

    // When placing corpses, use place:Elf:7 to choose monsters
    // appropriate for that level, as an example.
    const std::string place = strip_tag_prefix(s, "place:");
    if (!place.empty())
    {
        try
        {
            result.place = level_id::parse_level_id(place);
        }
        catch (const std::string &err)
        {
            error = err;
            return (result);
        }
    }

    const std::string acquirement_source = strip_tag_prefix(s, "acquire:");
    if (!acquirement_source.empty() || strip_tag(s, "acquire"))
    {
        if (!acquirement_source.empty())
            result.acquirement_source =
                parse_acquirement_source(acquirement_source);
        // If requesting acquirement, must specify item base type or
        // "any".
        result.level = ISPEC_ACQUIREMENT;
        if (s == "any")
            result.base_type = OBJ_RANDOM;
        else
            parse_random_by_class(s, result);
        return (result);
    }

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

    std::string unrand_str = strip_tag_prefix(s, "unrand:");

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

    if (strip_tag(s, "damaged"))
        result.level = ISPEC_DAMAGED;
    if (strip_tag(s, "cursed"))
        result.level = ISPEC_BAD; // damaged + cursed, actually
    if (strip_tag(s, "randart"))
        result.level = ISPEC_RANDART;
    if (strip_tag(s, "not_cursed"))
        result.props["uncursed"] = bool(true);
    if (strip_tag(s, "useful"))
        result.props["useful"] = bool(true);

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

    // XXX: This is nice-ish now, but could probably do with being improved.
    if (strip_tag(s, "randbook"))
    {
        result.props["make_book_theme_randart"] = true;
        // make_book_theme_randart requires the following properties:
        // disc: <first discipline>, disc2: <optional second discipline>
        // numspells: <total number of spells>, slevels: <maximum levels>
        // spell: <include this spell>, owner:<name of owner>
        // None of these are required, but if you don't intend on using any
        // of them, use "any fixed theme book" instead.
        short disc1 = 0;
        short disc2 = 0;

        std::string st_disc1 = strip_tag_prefix(s, "disc:");
        if (!st_disc1.empty())
        {
            disc1 = school_by_name(st_disc1);
            if (disc1 == SPTYP_NONE)
            {
                error = make_stringf("Bad spell school: %s", st_disc1.c_str());
                return (result);
            }
        }

        std::string st_disc2 = strip_tag_prefix(s, "disc2:");
        if (!st_disc2.empty())
        {
            disc2 = school_by_name(st_disc2);
            if (disc2 == SPTYP_NONE)
            {
                error = make_stringf("Bad spell school: %s", st_disc2.c_str());
                return (result);
            }
        }

        if (disc1 != 0 && disc2 != 0)
        {
            if (disciplines_conflict(disc1, disc2))
            {
                error = make_stringf("Bad combination of spell schools: %s & %s.",
                                    st_disc1.c_str(), st_disc2.c_str());
                return (result);
            }
        }

        if (disc1 == 0 && disc2 != 0)
        {
            // Don't fail, just quietly swap. Any errors in disc1's syntax will
            // have been caught above, anyway.
            disc1 = disc2;
            disc2 = 0;
        }

        short num_spells = strip_number_tag(s, "numspells:");
        if (num_spells == TAG_UNFOUND)
            num_spells = -1;
        else if (num_spells <= 0 || num_spells > SPELLBOOK_SIZE)
        {
            error = make_stringf("Bad spellbook size: %d", num_spells);
            return (result);
        }

        short slevels = strip_number_tag(s, "slevels:");
        if (slevels == TAG_UNFOUND)
            slevels = -1;
        else if (slevels == 0)
        {
            error = make_stringf("Bad slevels: %d.", slevels);
            return (result);
        }

        const std::string spell = replace_all_of(strip_tag_prefix(s, "spell:"),
                                                "_", " ");
        if (!spell.empty() && spell_by_name(spell) == SPELL_NO_SPELL)
        {
            error = make_stringf("Bad spell: %s", spell.c_str());
            return (result);
        }

        const std::string owner = replace_all_of(strip_tag_prefix(s, "owner:"),
                                                "_", " ");
        result.props["randbook_disc1"] = disc1;
        result.props["randbook_disc2"] = disc2;
        result.props["randbook_num_spells"] = num_spells;
        result.props["randbook_slevels"] = slevels;
        result.props["randbook_spell"] = spell;
        result.props["randbook_owner"] = owner;

        result.base_type = OBJ_BOOKS;
        // This is changed in make_book_theme_randart.
        result.sub_type = BOOK_MINOR_MAGIC_I;
        result.plus = -1;

        return (result);
    }

    if (s.find("deck") != std::string::npos)
    {
        build_deck_spec(s, &result);
        return (result);
    }

    // Clean up after any tag brain damage.
    trim_string(s);

    if (!ego_str.empty())
        error = "Can't set an ego for random items.";

    // Completely random?
    if (s == "random" || s == "any" || s == "%")
        return (result);

    if (s == "*" || s == "star_item")
    {
        result.level = ISPEC_GOOD;
        return (result);
    }
    else if (s == "|" || s == "superb_item")
    {
        result.level = ISPEC_SUPERB;
        return (result);
    }
    else if (s == "$" || s == "gold")
    {
        if (!ego_str.empty())
            error = "Can't set an ego for gold.";

        result.base_type = OBJ_GOLD;
        result.sub_type  = OBJ_RANDOM;
        return (result);
    }

    if (s == "nothing")
    {
        error.clear();
        result.base_type = OBJ_UNASSIGNED;
        return (result);
    }

    error.clear();

    // Look for corpses, chunks, skeletons:
    if (ends_with(s, "corpse") || ends_with(s, "chunk")
        || ends_with(s, "skeleton"))
    {
        return parse_corpse_spec(result, s);
    }

    // Check for "any objclass"
    if (s.find("any ") == 0)
        parse_random_by_class(s.substr(4), result);
    else if (s.find("random ") == 0)
        parse_random_by_class(s.substr(7), result);
    // Check for actual item names.
    else
        parse_raw_name(s, result);

    if (!error.empty())
        return (result);

    if (!unrand_str.empty())
    {
        result.ego = get_unrandart_num(unrand_str.c_str());
        if (result.ego == SPWPN_NORMAL)
        {
            error = make_stringf("Unknown unrand art: %s", unrand_str.c_str());
            return result;
        }
        result.ego = -result.ego;
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
        error = make_stringf("Ego '%s' is invalid for item '%s'.",
                             ego_str.c_str(), s.c_str());
        return (result);
    }
    else if (result.sub_type == OBJ_RANDOM)
    {
        // it will be assigned among appropriate ones later
    }
    else if (result.base_type == OBJ_WEAPONS
                && !is_weapon_brand_ok(result.sub_type, ego, false)
             || result.base_type == OBJ_ARMOUR
                && !is_armour_brand_ok(result.sub_type, ego, false)
             || result.base_type == OBJ_MISSILES
                && !is_missile_brand_ok(result.sub_type, ego, false))
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
    // lowercase(spec);

    item_spec_slot list;

    list.fix_slot = strip_tag(spec, "fix_slot");
    std::vector<std::string> specifiers = split_string( "/", spec );

    for (unsigned i = 0; i < specifiers.size() && error.empty(); ++i)
        list.ilist.push_back( parse_single_spec(specifiers[i]) );

    return (list);
}

/////////////////////////////////////////////////////////////////////////
// subst_spec

subst_spec::subst_spec(std::string _k, bool _f, const glyph_replacements_t &g)
    : key(_k), count(-1), fix(_f), frozen_value(0), repl(g)
{
}

subst_spec::subst_spec(int _count, bool dofix, const glyph_replacements_t &g)
    : key(""), count(_count), fix(dofix), frozen_value(0), repl(g)
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

//////////////////////////////////////////////////////////////////////////
// nsubst_spec

nsubst_spec::nsubst_spec(std::string _key, const std::vector<subst_spec> &_specs)
    : key(_key), specs(_specs)
{
}

//////////////////////////////////////////////////////////////////////////
// colour_spec

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
// fprop_spec

unsigned long fprop_spec::get_property()
{
    if (fixed_prop != FPROP_NONE)
        return (fixed_prop);

    unsigned long chosen = FPROP_NONE;
    int cweight = 0;
    for (int i = 0, size = fprops.size(); i < size; ++i)
        if (x_chance_in_y(fprops[i].second, cweight += fprops[i].second))
            chosen = fprops[i].first;
    if (fix)
        fixed_prop = chosen;
    return (chosen);
}

//////////////////////////////////////////////////////////////////////////
// fheight_spec

int fheight_spec::get_height()
{
    if (fixed_height != INVALID_HEIGHT)
        return (fixed_height);

    int chosen = INVALID_HEIGHT;
    int cweight = 0;
    for (int i = 0, size = fheights.size(); i < size; ++i)
        if (x_chance_in_y(fheights[i].second, cweight += fheights[i].second))
            chosen = fheights[i].first;
    if (fix)
        fixed_height = chosen;
    return (chosen);
}

//////////////////////////////////////////////////////////////////////////
// string_spec

std::string string_spec::get_property()
{
    if (!fixed_str.empty())
        return fixed_str;

    std::string chosen = "";
    int cweight = 0;
    for (int i = 0, size = strlist.size(); i < size; ++i)
        if (x_chance_in_y(strlist[i].second, cweight += strlist[i].second))
            chosen = strlist[i].first;
    if (fix)
        fixed_str = chosen;
    return (chosen);
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

    // Fix this feature.
    if (fix)
        get_feat();

    return (err);
}

void keyed_mapspec::copy_feat(const keyed_mapspec &spec)
{
    feat = spec.feat;
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
    const bool known = strip_tag(s, "known");

    trim_string(s);
    lowercase(s);

    const int trap = str_to_trap(s);
    if (trap == -1)
        err = make_stringf("bad trap name: '%s'", s.c_str());

    feature_spec fspec(known ? 1 : -1, weight);
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
             "no_secret_doors", "no_wall_fixup", "opaque", "no_trap_gen",
             "no_shop_gen", ""};
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

void keyed_mapspec::copy_mons(const keyed_mapspec &spec)
{
    mons = spec.mons;
}

void keyed_mapspec::copy_item(const keyed_mapspec &spec)
{
    item = spec.item;
}

void keyed_mapspec::copy_mask(const keyed_mapspec &spec)
{
    map_mask = spec.map_mask;
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
