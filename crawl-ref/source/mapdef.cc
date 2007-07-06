/*
 *  File:       mapdef.cc
 *  Summary:    Support code for Crawl des files.
 *
 *  Modified for Crawl Reference by $Author: dshaligram $ on $Date: 2007-06-30T15:49:18.688054Z $
 */

#include <iostream>
#include <cstdarg>
#include <cstdio>
#include <cctype>
#include <algorithm>

#include "AppHdr.h"
#include "branch.h"
#include "describe.h"
#include "direct.h"
#include "files.h"
#include "invent.h"
#include "items.h"
#include "libutil.h"
#include "mapdef.h"
#include "maps.h"
#include "misc.h"
#include "monplace.h"
#include "mon-util.h"
#include "stuff.h"
#include "dungeon.h"

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

const char *map_section_name(int msect)
{
    if (msect < 0 || msect >= MAP_NUM_SECTION_TYPES)
        return "";

    return map_section_names[msect];
}

// Returns true if s contains tag 'tag', and strips out tag from s.
bool strip_tag(std::string &s, const std::string &tag)
{
    if (s == tag)
    {
        s.clear();
        return (true);
    }

    std::string::size_type pos;
    if ((pos = s.find(" " + tag + " ")) != std::string::npos)
    {
        // Leave one space intact.
        s.erase(pos, tag.length() + 1);
        return (true);
    }

    if ((pos = s.find(tag + " ")) != std::string::npos
        || (pos = s.find(" " + tag)) != std::string::npos)
    {
        s.erase(pos, tag.length() + 1);
        return (true);
    }

    return (false);
}

#define TAG_UNFOUND -20404
int strip_number_tag(std::string &s, const std::string &tagprefix)
{
    std::string::size_type pos = s.find(tagprefix);
    if (pos == std::string::npos)
        return (TAG_UNFOUND);

    std::string::size_type ns = s.find(" ", pos);
    if (ns == std::string::npos)
        ns = s.length();

    std::string argument =
        s.substr(pos + tagprefix.length(), ns - pos - tagprefix.length());

    s.erase(pos, ns - pos + 1);

    return atoi(argument.c_str());
}

static int find_weight(std::string &s)
{
    int weight = strip_number_tag(s, "weight:");
    if (weight == TAG_UNFOUND)
        weight = strip_number_tag(s, "w:");
    return (weight);
}

static std::string split_key_item(const std::string &s,
                                  std::string *key,
                                  int *separator,
                                  std::string *arg,
                                  int key_max_len = 1)
{
    std::string::size_type
        norm = s.find("=", 1),
        fixe = s.find(":", 1);

    const std::string::size_type sep = norm < fixe? norm : fixe;
    if (sep == std::string::npos)
        return ("malformed declaration - must use = or :");

    *key = trimmed_string(s.substr(0, sep));
    std::string substitute    = trimmed_string(s.substr(sep + 1));

    if (key->empty() || (int) key->length() > key_max_len)
        return make_stringf(
            "selector '%s' must be exactly one character in '%s'",
            key->c_str(), s.c_str());

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
    : branch(br), shallowest(), deepest(), deny(false)
{
    set(s, d);
}

level_range::level_range(const raw_range &r)
    : branch(r.branch), shallowest(r.shallowest), deepest(r.deepest),
      deny(r.deny)
{
}

void level_range::write(FILE *outf) const
{
    writeShort(outf, branch);
    writeShort(outf, shallowest);
    writeShort(outf, deepest);
    writeByte(outf, deny);
}

void level_range::read(FILE *inf)
{
    branch     = static_cast<branch_type>( readShort(inf) );
    shallowest = readShort(inf);
    deepest    = readShort(inf);
    deny       = readByte(inf);
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
    else
    {
        if ((branch = str_to_branch(br)) == NUM_BRANCHES)
            throw make_stringf("Unknown branch: '%s'", br.c_str());
    }

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
        *l = *h = atoi(s.c_str());
        if (!*l)
            throw std::string("Bad depth: ") + s;
    }
    else
    {
        *l = atoi(s.substr(0, hy).c_str());

        std::string tail = s.substr(hy + 1);
        if (tail.empty())
            *h = 100;
        else
            *h = atoi(tail.c_str());

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
}

bool level_range::matches(const level_id &lid) const
{
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
    return (deny == lr.deny && shallowest == lr.shallowest
            && deepest == lr.deepest && branch == lr.branch);
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
    : transforms(), markers(), lines(), map_width(0), solid_north(false),
      solid_east(false), solid_south(false), solid_west(false),
      solid_checked(false)
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
    clear_transforms();
    clear_markers();
    lines         = map.lines;
    map_width     = map.map_width;
    solid_north   = map.solid_north;
    solid_east    = map.solid_east;
    solid_south   = map.solid_south;
    solid_west    = map.solid_west;
    solid_checked = map.solid_checked;

    for (int i = 0, size = map.transforms.size(); i < size; ++i)
        transforms.push_back( map.transforms[i]->clone() );

    for (int i = 0, size = map.markers.size(); i < size; ++i)
        markers.push_back( map.markers[i]->clone() );
}

template <typename V>
void map_lines::clear_vector(V &vect)
{
    for (int i = 0, size = vect.size(); i < size; ++i)
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
    std::string err = split_key_item(s, &key, &sep, &arg);
    if (!err.empty())
        return (err);

    const dungeon_feature_type feat = dungeon_feature_by_name(arg);
    if (feat == DNGN_UNSEEN)
        return make_stringf("unknown feature: %s", arg.c_str());

    transforms.push_back(new map_feat_marker_spec(key[0], feat));    
    return ("");
}

void map_lines::apply_markers(const coord_def &c)
{
    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        markers[i]->pos += c;
        env.add_marker(markers[i]);
    }
    // *not* clear_markers() since we've offloaded marker ownership to
    // the crawl env.
    markers.clear();
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
    
    for (int i = 1, size = segs.size(); i < size; ++i)
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

    for (int i = 0, size = segs.size(); i < size; ++i)
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

std::string map_lines::add_subst(const std::string &sub)
{
    std::string s = trimmed_string(sub);

    if (s.empty())
        return ("");
    
    int sep = 0;
    std::string key;
    std::string substitute;

    std::string err = split_key_item(sub, &key, &sep, &substitute);
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
    std::string err = split_key_item(s, &key, &sep, &arg);
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

    std::string err = split_key_item(s, &key, &sep, &arg);
    if (!err.empty())
        return (err);
    
    std::vector<std::string> segs = split_string("/", arg);
    for (int i = 0, size = segs.size(); i < size; ++i)
    {
        std::string &ns = segs[i];
        if (ns.find('=') == std::string::npos
            && ns.find(':') == std::string::npos)
        {
            if (i < size - 1)
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
        for (int i = 0, size = transforms.size(); i < size; ++i)
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

        for (int i = 0, size = transforms.size(); i < size; ++i)
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
    clear_transforms();
    clear_markers();
    lines.clear();
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
    for (int i = 0, size = spec.specs.size(); i < size && pcount < psize; ++i)
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
    return comma_separated_line(segs.begin(), segs.end(), "/", "/");
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
    
    for (int i = 0, size = lines.size(); i < size; ++i)
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
    for (int i = 0, size = transforms.size(); i < size; ++i)
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
    for (int i = 0, size = lines.size(); i < size; ++i)
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

    map_width = lines.size();
    lines     = newlines;
    rotate_markers(clockwise);
    solid_checked = false;
}

void map_lines::translate_marker(
    void (map_lines::*xform)(map_marker *, int),
    int par)
{
    for (int i = 0, size = markers.size(); i < size; ++i)
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
    const int size = lines.size();
    const int midpoint = size / 2;

    for (int i = 0; i < midpoint; ++i)
    {
        std::string temp = lines[i];
        lines[i] = lines[size - 1 - i];
        lines[size - 1 - i] = temp;
    }
    vmirror_markers();
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
    hmirror_markers();
    solid_checked = false;
}

std::vector<std::string> map_lines::get_shuffle_strings() const
{
    std::vector<std::string> shuffles;
    for (int i = 0, size = transforms.size(); i < size; ++i)
        if (transforms[i]->type() == map_transformer::TT_SHUFFLE)
            shuffles.push_back( transforms[i]->describe() );
    return (shuffles);
}

std::vector<std::string> map_lines::get_subst_strings() const
{
    std::vector<std::string> substs;
    for (int i = 0, size = transforms.size(); i < size; ++i)
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
    : name(), tags(), place(), depths(), orient(), chance(),
      map(), mons(), items(), keyspecs(), prelude("dlprelude"), main("dlmain"),
      validate("dlvalidate"), veto("dlveto"), index_only(false),
      cache_offset(0L)
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
    reinit();
}

void map_def::reinit()
{
    items.clear();
    keyspecs.clear();

    // Base chance; this is not a percentage.
    chance = 10;

    // Clearing the map also zaps map transforms.
    map.clear();
    mons.clear();
}

int map_def::glyph_at(const coord_def &c) const
{
    return map(c);
}

void map_def::write_full(FILE *outf)
{
    cache_offset = ftell(outf);
    writeShort(outf, MAP_CACHE_VERSION);   // Level indicator.
    writeString(outf, name);
    prelude.write(outf);
    main.write(outf);
    validate.write(outf);
    veto.write(outf);
}

void map_def::read_full(FILE *inf)
{
    // There's a potential race-condition here:
    // - If someone modifies a .des file while there are games in progress,
    // - a new Crawl process will overwrite the .dsc.
    // - older Crawl processes trying to reading the new .dsc will be hosed.
    // We could try to recover from the condition (by locking and
    // reloading the index), but it's easier to save the game at this
    // point and let the player reload.

    if (readShort(inf) != MAP_CACHE_VERSION || readString(inf) != name)
        save_game(true,
                  make_stringf("Level file cache for %s is out-of-sync! "
                               "Please reload your game.",
                               file.c_str()).c_str());

    prelude.read(inf);
    main.read(inf);
    validate.read(inf);
    veto.read(inf);
}

void map_def::load()
{
    if (!index_only)
        return;
    
    const std::string loadfile = get_descache_path(file, ".dsc");
    FILE *inf = fopen(loadfile.c_str(), "r");
    fseek(inf, cache_offset, SEEK_SET);
    read_full(inf);
    fclose(inf);

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

void map_def::write_index(FILE *outf) const
{
    if (!cache_offset)
        end(1, false, "Map %s: can't write index - cache offset not set!",
            name.c_str());
    writeString(outf, name);
    writeShort(outf, orient);
    writeLong(outf, chance);
    writeLong(outf, cache_offset);
    writeString(outf, tags);
    writeString(outf, place);
    write_depth_ranges(outf);
    prelude.write(outf);
}

void map_def::read_index(FILE *inf)
{
    name         = readString(inf);
    orient       = static_cast<map_section_type>( readShort(inf) );
    chance       = readLong(inf);
    cache_offset = readLong(inf);
    tags         = readString(inf);
    place        = readString(inf);
    read_depth_ranges(inf);
    prelude.read(inf);
    index_only   = true;
}

void map_def::write_depth_ranges(FILE *outf) const
{
    writeShort(outf, depths.size());
    for (int i = 0, size = depths.size(); i < size; ++i)
        depths[i].write(outf);
}

void map_def::read_depth_ranges(FILE *inf)
{
    depths.clear();
    const int nranges = readShort(inf);
    for (int i = 0; i < nranges; ++i)
        depths[i].read(inf);
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
            mprf(MSGCH_WARN, "Lua error: %s", validate.orig_error().c_str());
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
            mprf(MSGCH_WARN, "Lua error: %s",
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

    if (map.height() == 0)
        return ("Must define map.");

    switch (orient)
    {
    case MAP_NORTH: case MAP_SOUTH:
        if (map.height() >= GYM * 2 / 3)
            return make_stringf("Map too large - height %d (max %d)",
                                map.height(), GYM * 2 / 3);
        break;
    case MAP_EAST: case MAP_WEST:
        if (map.width() >= GXM * 2 / 3)
            return make_stringf("Map too large - width %d (max %d)",
                                map.width(), GXM * 2 / 3);
        break;
    case MAP_NORTHEAST: case MAP_SOUTHEAST:
    case MAP_NORTHWEST: case MAP_SOUTHWEST:
        if (map.width() >= GXM * 2 / 3 || map.height() > GYM * 2 / 3)
            return make_stringf("Map too large - %dx%d (max %dx%d)",
                                map.width(), map.height(),
                                GXM * 2 / 3, GYM * 2 / 3);
        break;
    default:
        break;
    }

    return (map.apply_transforms());
}

bool map_def::is_usable_in(const level_id &lid) const
{
    bool any_matched = false;
    for (int i = 0, size = depths.size(); i < size; ++i)
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
    // Minivaults are padded out with floor tiles, normal maps are
    // padded out with rock walls.
    map.normalise(is_minivault()? '.' : 'x');
}

std::string map_def::resolve()
{
    return map.apply_transforms();
}

void map_def::fixup()
{
    normalise();
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

    std::string err = split_key_item(s, &key, &separator, &arg);
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
    if (demon == DEMON_RANDOM)
        demon = random2(DEMON_RANDOM);
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
        if (random2(totweight += weight) < weight)
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
        mons_spec mspec;

        mspec.genweight = find_weight(s);
        if (mspec.genweight == TAG_UNFOUND || mspec.genweight <= 0)
            mspec.genweight = 10;

        mspec.fix_mons = strip_tag(s, "fix_mons");
        mspec.generate_awake = strip_tag(s, "generate_awake");

        trim_string(s);

        if (s == "8")
            mspec.mlevel = -8;
        else if (s == "9")
            mspec.mlevel = -9;
        else if (check_mimic(s, &mspec.mid, &mspec.fix_mons))
            ;
        else if (s != "0")
        {
            const mons_spec nspec = mons_by_name(s);

            if (nspec.mid == MONS_PROGRAM_BUG)
            {
                error = make_stringf("unrecognised monster \"%s\"", s.c_str());
                return (slot);
            }

            mspec.mid = nspec.mid;
            mspec.monnum = nspec.monnum;
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
        " zombie", " skeleton", " simulacrum", NULL
    };

    // This order must match zombie_types, indexed from one. 
    static const monster_type zombie_montypes[][2] =
    {
        { MONS_PROGRAM_BUG, MONS_PROGRAM_BUG },
        { MONS_ZOMBIE_SMALL, MONS_ZOMBIE_LARGE },
        { MONS_SKELETON_SMALL, MONS_SKELETON_LARGE },
        { MONS_SIMULACRUM_SMALL, MONS_SIMULACRUM_LARGE }
    };

    const int mod = ends_with(s, zombie_types);
    if (!mod)
    {
        const std::string &spectre = "spectral ";
        if (s.find(spectre) == 0)
        {
            s = s.substr(spectre.length());
            spec.mid = MONS_SPECTRAL_THING;
            spec.monnum = get_monster_by_name(s, true);
            if (!mons_zombie_size(spec.monnum))
                spec.mid = MONS_PROGRAM_BUG;
            return;
        }
        spec.mid = MONS_PROGRAM_BUG;
        return;
    }

    s = s.substr(0, s.length() - strlen(zombie_types[mod - 1]));
    trim_string(s);
    
    spec.monnum = get_monster_by_name(s, true);

    const int zombie_size = mons_zombie_size(spec.monnum);
    if (!zombie_size)
    {
        spec.mid = MONS_PROGRAM_BUG;
        return;
    }

    spec.mid = zombie_montypes[mod][zombie_size - 1];
}

mons_spec mons_list::get_hydra_spec(const std::string &name) const
{
    int nheads = atoi(name.c_str());
    if (nheads < 1)
        nheads = 250; // Choose randomly, doesn't mean a 250-headed hydra.
    else if (nheads > 19)
        nheads = 19;

    return mons_spec(MONS_HYDRA, nheads);
}

mons_spec mons_list::mons_by_name(std::string name) const
{
    lowercase(name);

    name = replace_all_of( name, "_", " " );

    if (name == "nothing")
        return (-1);
    
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

    if (ends_with(name, "-headed hydra"))
        return get_hydra_spec(name);

    mons_spec spec;
    get_zombie_type(name, spec);
    if (spec.mid != MONS_PROGRAM_BUG)
        return (spec);

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
        if (random2(cumulative += weight) < weight)
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

    if (strip_tag(s, "good_item"))
        result.level = MAKE_GOOD_ITEM;
    else
    {
        int number = strip_number_tag(s, "level:");
        if (number != TAG_UNFOUND)
        {
            if (number <= 0)
                error = make_stringf("Bad item level: %d", number);

            result.level = number;
        }
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
                error = make_stringf("Bad uniq level: %d", uniq);
            result.allow_uniques = uniq;
        }
    }

    // Clean up after any tag brain damage.
    trim_string(s);
    
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

    if (s == "nothing")
    {
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
    parse_raw_name(s, result);

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
    {
        if (random2(cumulative += repl[i].second) < repl[i].second)
            chosen = repl[i].first;
    }

    if (fix)
        frozen_value = chosen;

    return (chosen);
}

std::string subst_spec::apply_transform(map_lines &map)
{
    map.subst(*this);
    return ("");
}

map_transformer *subst_spec::clone() const
{
    return new subst_spec(*this);
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

map_transformer *nsubst_spec::clone() const
{
    return new nsubst_spec(key, specs);
}

std::string nsubst_spec::describe() const
{
    return ("");
}

//////////////////////////////////////////////////////////////////////////
// shuffle_spec

std::string shuffle_spec::apply_transform(map_lines &map)
{
    map.resolve_shuffle(shuffle);
    return ("");
}

map_transformer *shuffle_spec::clone() const
{
    return new shuffle_spec(*this);
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
// map_feat_marker_spec

std::string map_feat_marker_spec::apply_transform(map_lines &map)
{
    std::vector<coord_def> positions = map.find_glyph(key);
    if (positions.size() == 1)
    {
        map.add_marker(new map_feature_marker(positions[0], feat));
        return ("");
    }
    else if (positions.empty())
        return make_stringf("cant find key '%c' for marker", key);
    else
        return make_stringf("too many matches for key '%c' for marker", key);
}

map_transformer::transform_type map_feat_marker_spec::type() const
{
    return (TT_MARKER);
}

std::string map_feat_marker_spec::describe() const
{
    return map_feature_marker(coord_def(), feat).describe();
}

map_transformer *map_feat_marker_spec::clone() const
{
    return new map_feat_marker_spec(key, feat);
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
    
    std::vector<dungeon_feature_type> feats =
        features_by_desc( glob_pattern(s, true) );
    if (!feats.empty())
        list.push_back( feature_spec(feats[0], weight) );

    if (feats.empty())
        err = make_stringf("no features matching \"%s\"",
                           str.c_str());
    
    return (list);
}

std::string keyed_mapspec::set_mons(const std::string &s, bool fix)
{
    err.clear();
    mons.clear();

    return (mons.add_mons(s, fix));
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

feature_spec keyed_mapspec::get_feat()
{
    return feat.get_feat(key_glyph);
}

mons_list &keyed_mapspec::get_monsters()
{
    return (mons);
}

item_list &keyed_mapspec::get_items()
{
    return (item);
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
        if (random2(tweight += feat.genweight) < feat.genweight)
            chosen_feat = feat;
    }

    if (fix_slot)
    {
        feats.clear();
        feats.push_back( chosen_feat );
    }
    return (chosen_feat);
}
