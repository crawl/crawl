/**
 * @file
 * @brief Support code for Crawl des files.
**/

#include "AppHdr.h"

#include "mapdef.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#include "abyss.h"
#include "artefact.h"
#include "branch.h"
#include "cluautil.h"
#include "colour.h"
#include "coordit.h"
#include "describe.h"
#include "dgn-height.h"
#include "dungeon.h"
#include "end.h"
#include "mpr.h"
#include "tile-env.h"
#include "english.h"
#include "files.h"
#include "ghost.h"
#include "initfile.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "invent.h"
#include "libutil.h"
#include "mapmark.h"
#include "maps.h"
#include "mon-cast.h"
#include "mon-place.h"
#include "mutant-beast.h"
#include "place.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "spl-book.h"
#include "spl-util.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "rltiles/tiledef-dngn.h"
#include "rltiles/tiledef-player.h"

#ifdef DEBUG_TAG_PROFILING
static map<string,int> _tag_profile;

static void _profile_inc_tag(const string &tag)
{
    if (!_tag_profile.count(tag))
        _tag_profile[tag] = 0;
    _tag_profile[tag]++;
}

void tag_profile_out()
{
    long total = 0;
    vector<pair<int, string>> resort;
    fprintf(stderr, "\nTag hits:\n");
    for (auto k : _tag_profile)
    {
        resort.emplace_back(k.second, k.first);
        total += k.second;
    }
    sort(resort.begin(), resort.end());
    for (auto p : resort)
    {
        long percent = ((long) p.first) * 100 / total;
        fprintf(stderr, "%8d (%2ld%%): %s\n", p.first, percent, p.second.c_str());
    }
    fprintf(stderr, "Total: %ld\n", total);
}
#endif

static const char *map_section_names[] =
{
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
    "centre",
};

static string_set Map_Flag_Names;

const char *traversable_glyphs =
    ".+=w@{}()[]<>BC^TUVY$%*|Odefghijk0123456789";

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

static int find_weight(string &s, int defweight = TAG_UNFOUND)
{
    int weight = strip_number_tag(s, "weight:");
    if (weight == TAG_UNFOUND)
        weight = strip_number_tag(s, "w:");
    return weight == TAG_UNFOUND ? defweight : weight;
}

void clear_subvault_stack()
{
    env.new_subvault_names.clear();
    env.new_subvault_tags.clear();
    env.new_used_subvault_names.clear();
    env.new_used_subvault_tags.clear();
}

void map_register_flag(const string &flag)
{
    Map_Flag_Names.insert(flag);
}

static bool _map_tag_is_selectable(const string &tag)
{
    return !Map_Flag_Names.count(tag)
           && tag.find("luniq_") != 0
           && tag.find("uniq_") != 0
           && tag.find("ruin_") != 0
           && tag.find("chance_") != 0;
}

string mapdef_split_key_item(const string &s, string *key, int *separator,
                             string *arg, int key_max_len)
{
    string::size_type
        norm = s.find("=", 1),
        fixe = s.find(":", 1);

    const string::size_type sep = norm < fixe? norm : fixe;
    if (sep == string::npos)
    {
        return make_stringf("malformed declaration - must use = or : in '%s'",
                            s.c_str());
    }

    *key = trimmed_string(s.substr(0, sep));
    string substitute = trimmed_string(s.substr(sep + 1));

    if (key->empty()
        || (key_max_len != -1 && (int) key->length() > key_max_len))
    {
        return make_stringf(
            "selector '%s' must be <= %d characters in '%s'",
            key->c_str(), key_max_len, s.c_str());
    }

    if (substitute.empty())
    {
        return make_stringf("no substitute defined in '%s'",
                            s.c_str());
    }

    *arg = substitute;
    *separator = s[sep];

    return "";
}

int store_tilename_get_index(const string& tilename)
{
    if (tilename.empty())
        return 0;

    // Increase index by 1 to distinguish between first entry and none.
    unsigned int i;
    for (i = 0; i < tile_env.names.size(); ++i)
        if (tilename == tile_env.names[i])
            return i+1;

#ifdef DEBUG_TILE_NAMES
    mprf("adding %s on index %d (%d)", tilename.c_str(), i, i+1);
#endif
    // If not found, add tile name to vector.
    tile_env.names.push_back(tilename);
    return i+1;
}

///////////////////////////////////////////////
// subvault_place
//

subvault_place::subvault_place()
    : tl(), br(), subvault()
{
}

subvault_place::subvault_place(const coord_def &_tl,
                               const coord_def &_br,
                               const map_def &_subvault)
    : tl(_tl), br(_br), subvault(new map_def(_subvault))
{
}

subvault_place::subvault_place(const subvault_place &place)
    : tl(place.tl), br(place.br),
      subvault(place.subvault ? new map_def(*place.subvault) : nullptr)
{
}

subvault_place &subvault_place::operator = (const subvault_place &place)
{
    tl = place.tl;
    br = place.br;
    subvault.reset(place.subvault ? new map_def(*place.subvault)
                                        : nullptr);
    return *this;
}

void subvault_place::set_subvault(const map_def &_subvault)
{
    subvault.reset(new map_def(_subvault));
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

void level_range::write(writer& outf) const
{
    marshallShort(outf, branch);
    marshallShort(outf, shallowest);
    marshallShort(outf, deepest);
    marshallBoolean(outf, deny);
}

void level_range::read(reader& inf)
{
    branch     = static_cast<branch_type>(unmarshallShort(inf));
    shallowest = unmarshallShort(inf);
    deepest    = unmarshallShort(inf);
    deny       = unmarshallBoolean(inf);
}

string level_range::str_depth_range() const
{
    if (shallowest == -1)
        return ":??";

    if (shallowest == BRANCH_END)
        return ":$";

    if (deepest == BRANCH_END)
        return shallowest == 1? "" : make_stringf("%d-", shallowest);

    if (shallowest == deepest)
        return make_stringf(":%d", shallowest);

    return make_stringf(":%d-%d", shallowest, deepest);
}

string level_range::describe() const
{
    return make_stringf("%s%s%s",
                        deny? "!" : "",
                        branch == NUM_BRANCHES ? "Any" :
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
    return r;
}

void level_range::set(const string &br, int s, int d)
{
    if (br == "any" || br == "Any")
        branch = NUM_BRANCHES;
    else if ((branch = branch_by_abbrevname(br)) == NUM_BRANCHES)
        throw bad_level_id_f("Unknown branch: '%s'", br.c_str());

    shallowest = s;
    deepest    = d;

    if (deepest < shallowest || deepest <= 0)
    {
        throw bad_level_id_f("Level-range %s:%d-%d is malformed",
                             br.c_str(), s, d);
    }
}

level_range level_range::parse(string s)
{
    level_range lr;
    trim_string(s);

    if (s == "*")
    {
        lr.set("any", 0, BRANCH_END);
        return lr;
    }

    if (s[0] == '!')
    {
        lr.deny = true;
        s = trimmed_string(s.substr(1));
    }

    string::size_type cpos = s.find(':');
    if (cpos == string::npos)
        parse_partial(lr, s);
    else
    {
        string br    = trimmed_string(s.substr(0, cpos));
        string depth = trimmed_string(s.substr(cpos + 1));
        parse_depth_range(depth, &lr.shallowest, &lr.deepest);

        lr.set(br, lr.shallowest, lr.deepest);
    }

    return lr;
}

void level_range::parse_partial(level_range &lr, const string &s)
{
    if (isadigit(s[0]))
    {
        lr.branch = NUM_BRANCHES;
        parse_depth_range(s, &lr.shallowest, &lr.deepest);
    }
    else
        lr.set(s, 1, BRANCH_END);
}

void level_range::parse_depth_range(const string &s, int *l, int *h)
{
    if (s == "*")
    {
        *l = 1;
        *h = BRANCH_END;
        return;
    }

    if (s == "$")
    {
        *l = BRANCH_END;
        *h = BRANCH_END;
        return;
    }

    string::size_type hy = s.find('-');
    if (hy == string::npos)
    {
        *l = *h = strict_aton<int>(s.c_str());
        if (!*l)
            throw bad_level_id("Bad depth: " + s);
    }
    else
    {
        *l = strict_aton<int>(s.substr(0, hy).c_str());

        string tail = s.substr(hy + 1);
        if (tail.empty() || tail == "$")
            *h = BRANCH_END;
        else
            *h = strict_aton<int>(tail.c_str());

        if (!*l || !*h || *l > *h)
            throw bad_level_id("Bad depth: " + s);
    }
}

void level_range::set(int s, int d)
{
    shallowest = s;
    deepest    = d;

    if (deepest == -1)
        deepest = shallowest;

    if (deepest < shallowest)
        throw bad_level_id_f("Bad depth range: %d-%d", shallowest, deepest);
}

void level_range::reset()
{
    deepest = shallowest = -1;
    branch  = NUM_BRANCHES;
}

bool level_range::matches(const level_id &lid) const
{
    if (branch == NUM_BRANCHES)
        return matches(absdungeon_depth(lid.branch, lid.depth));
    else
    {
        return branch == lid.branch
               && (lid.depth >= shallowest
                   || shallowest == BRANCH_END && lid.depth == brdepth[branch])
               && lid.depth <= deepest;
    }
}

bool level_range::matches(int x) const
{
    // [ds] The level ranges used by the game are zero-based, adjust for that.
    ++x;
    return x >= shallowest && x <= deepest;
}

bool level_range::operator == (const level_range &lr) const
{
    return deny == lr.deny
           && shallowest == lr.shallowest
           && deepest == lr.deepest
           && branch == lr.branch;
}

bool level_range::valid() const
{
    return shallowest > 0 && deepest >= shallowest;
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
    ASSERT_RANGE(h, 0, GYM + 1);

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
    return lines[c.y][c.x];
}

char& map_lines::operator () (const coord_def &c)
{
    return lines[c.y][c.x];
}

char map_lines::operator () (int x, int y) const
{
    return lines[y][x];
}

char& map_lines::operator () (int x, int y)
{
    return lines[y][x];
}

bool map_lines::in_bounds(const coord_def &c) const
{
    return c.x >= 0 && c.y >= 0 && c.x < width() && c.y < height();
}

bool map_lines::in_map(const coord_def &c) const
{
    return in_bounds(c) && lines[c.y][c.x] != ' ';
}

map_lines &map_lines::operator = (const map_lines &map)
{
    if (this != &map)
        init_from(map);
    return *this;
}

map_lines::~map_lines()
{
    clear_markers();
}

void map_lines::init_from(const map_lines &map)
{
    // Markers have to be regenerated, they will not be copied.
    clear_markers();
    overlay.reset(nullptr);
    lines            = map.lines;
    map_width        = map.map_width;
    solid_north      = map.solid_north;
    solid_east       = map.solid_east;
    solid_south      = map.solid_south;
    solid_west       = map.solid_west;
    solid_checked    = map.solid_checked;
}

void map_lines::clear_markers()
{
    deleteAll(markers);
}

void map_lines::add_marker(map_marker *marker)
{
    markers.push_back(marker);
}

string map_lines::add_feature_marker(const string &s)
{
    string key, arg;
    int sep = 0;
    string err = mapdef_split_key_item(s, &key, &sep, &arg, -1);
    if (!err.empty())
        return err;

    map_marker_spec spec(key, arg);
    spec.apply_transform(*this);

    return "";
}

string map_lines::add_lua_marker(const string &key, const lua_datum &function)
{
    map_marker_spec spec(key, function);
    spec.apply_transform(*this);
    return "";
}

void map_lines::apply_markers(const coord_def &c)
{
    for (map_marker *marker : markers)
    {
        marker->pos += c;
        env.markers.add(marker);
    }
    // *not* clear_markers() since we've offloaded marker ownership to
    // the crawl env.
    markers.clear();
}

void map_lines::apply_grid_overlay(const coord_def &c, bool is_layout)
{
    if (!overlay)
        return;

    for (int y = height() - 1; y >= 0; --y)
        for (int x = width() - 1; x >= 0; --x)
        {
            coord_def gc(c.x + x, c.y + y);
            if (is_layout && map_masked(gc, MMT_VAULT))
                continue;

            const int colour = (*overlay)(x, y).colour;
            if (colour)
                dgn_set_grid_colour_at(gc, colour);

            const terrain_property_t property = (*overlay)(x, y).property;
            if (property.flags >= FPROP_BLOODY)
            {
                 // Over-ride whatever property is already there.
                env.pgrid(gc) |= property;
            }

            const int fheight = (*overlay)(x, y).height;
            if (fheight != INVALID_HEIGHT)
            {
                if (!env.heightmap)
                    dgn_initialise_heightmap();
                dgn_height_at(gc) = fheight;
            }

            bool has_floor = false, has_rock = false;
            string name = (*overlay)(x, y).floortile;
            if (!name.empty() && name != "none")
            {
                tile_env.flv(gc).floor_idx =
                    store_tilename_get_index(name);

                tileidx_t floor;
                tile_dngn_index(name.c_str(), &floor);
                if (colour)
                    floor = tile_dngn_coloured(floor, colour);
                int offset = random2(tile_dngn_count(floor));
                tile_env.flv(gc).floor = floor + offset;
                has_floor = true;
            }

            name = (*overlay)(x, y).rocktile;
            if (!name.empty() && name != "none")
            {
                tile_env.flv(gc).wall_idx =
                    store_tilename_get_index(name);

                tileidx_t rock;
                tile_dngn_index(name.c_str(), &rock);
                if (colour)
                    rock = tile_dngn_coloured(rock, colour);
                int offset = random2(tile_dngn_count(rock));
                tile_env.flv(gc).wall = rock + offset;
                has_rock = true;
            }

            name = (*overlay)(x, y).tile;
            if (!name.empty() && name != "none")
            {
                tile_env.flv(gc).feat_idx =
                    store_tilename_get_index(name);

                tileidx_t feat;
                tile_dngn_index(name.c_str(), &feat);

                if (colour)
                    feat = tile_dngn_coloured(feat, colour);

                if (!has_floor && env.grid(gc) == DNGN_FLOOR)
                    tile_env.flv(gc).floor = feat;
                else if (!has_rock && env.grid(gc) == DNGN_ROCK_WALL)
                    tile_env.flv(gc).wall = feat;
                else
                    tile_env.flv(gc).feat = feat;
            }
        }
}

void map_lines::apply_overlays(const coord_def &c, bool is_layout)
{
    apply_markers(c);
    apply_grid_overlay(c, is_layout);
}

const vector<string> &map_lines::get_lines() const
{
    return lines;
}

vector<string> &map_lines::get_lines()
{
    return lines;
}

void map_lines::add_line(const string &s)
{
    lines.push_back(s);
    if (static_cast<int>(s.length()) > map_width)
        map_width = s.length();
}

string map_lines::clean_shuffle(string s)
{
    return replace_all_of(s, " \t", "");
}

string map_lines::check_block_shuffle(const string &s)
{
    const vector<string> segs = split_string("/", s);
    const unsigned seglen = segs[0].length();

    for (const string &seg : segs)
    {
        if (seglen != seg.length())
            return "block shuffle segment length mismatch";
    }

    return "";
}

string map_lines::check_shuffle(string &s)
{
    if (s.find(',') != string::npos)
        return "use / for block shuffle, or multiple SHUFFLE: lines";

    s = clean_shuffle(s);

    if (s.find('/') != string::npos)
        return check_block_shuffle(s);

    return "";
}

string map_lines::check_clear(string &s)
{
    s = clean_shuffle(s);

    if (!s.length())
        return "no glyphs specified for clearing";

    return "";
}

string map_lines::parse_glyph_replacements(string s, glyph_replacements_t &gly)
{
    s = replace_all_of(s, "\t", " ");
    for (const string &is : split_string(" ", s))
    {
        if (is.length() > 2 && is[1] == ':')
        {
            const int glych = is[0];
            int weight;
            if (!parse_int(is.substr(2).c_str(), weight) || weight < 1)
                return "Invalid weight specifier in \"" + s + "\"";
            else
                gly.emplace_back(glych, weight);
        }
        else
        {
            for (int c = 0, cs = is.length(); c < cs; ++c)
                gly.emplace_back(is[c], 10);
        }
    }

    return "";
}

template<class T>
static string _parse_weighted_str(const string &spec, T &list)
{
    for (string val : split_string("/", spec))
    {
        lowercase(val);

        int weight = find_weight(val);
        if (weight == TAG_UNFOUND)
        {
            weight = 10;
            // :number suffix?
            string::size_type cpos = val.find(':');
            if (cpos != string::npos)
            {
                if (!parse_int(val.substr(cpos + 1).c_str(), weight) || weight <= 0)
                    return "Invalid weight specifier in \"" + spec + "\"";
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
    return "";
}

bool map_colour_list::parse(const string &col, int weight)
{
    const int colour = col == "none" ? BLACK : str_to_colour(col, -1, false, true);
    if (colour == -1)
        return false;

    emplace_back(colour, weight);
    return true;
}

string map_lines::add_colour(const string &sub)
{
    string s = trimmed_string(sub);

    if (s.empty())
        return "";

    int sep = 0;
    string key;
    string substitute;

    string err = mapdef_split_key_item(sub, &key, &sep, &substitute, -1);
    if (!err.empty())
        return err;

    map_colour_list colours;
    err = _parse_weighted_str<map_colour_list>(substitute, colours);
    if (!err.empty())
        return err;

    colour_spec spec(key, sep == ':', colours);
    overlay_colours(spec);

    return "";
}

bool map_fprop_list::parse(const string &fp, int weight)
{
    feature_property_type fprop;

    if (fp == "nothing")
        fprop = FPROP_NONE;
    else if (fp.empty())
        return false;
    else if (str_to_fprop(fp) == FPROP_NONE)
        return false;
    else
        fprop = str_to_fprop(fp);

    emplace_back(fprop, weight);
    return true;
}

bool map_featheight_list::parse(const string &fp, int weight)
{
    const int thisheight = strict_aton(fp.c_str(), INVALID_HEIGHT);
    if (thisheight == INVALID_HEIGHT)
        return false;
    emplace_back(thisheight, weight);
    return true;
}

string map_lines::add_fproperty(const string &sub)
{
    string s = trimmed_string(sub);

    if (s.empty())
        return "";

    int sep = 0;
    string key;
    string substitute;

    string err = mapdef_split_key_item(sub, &key, &sep, &substitute, -1);
    if (!err.empty())
        return err;

    map_fprop_list fprops;
    err = _parse_weighted_str<map_fprop_list>(substitute, fprops);
    if (!err.empty())
        return err;

    fprop_spec spec(key, sep == ':', fprops);
    overlay_fprops(spec);

    return "";
}

string map_lines::add_fheight(const string &sub)
{
    string s = trimmed_string(sub);
    if (s.empty())
        return "";

    int sep = 0;
    string key;
    string substitute;

    string err = mapdef_split_key_item(sub, &key, &sep, &substitute, -1);
    if (!err.empty())
        return err;

    map_featheight_list fheights;
    err = _parse_weighted_str(substitute, fheights);
    if (!err.empty())
        return err;

    fheight_spec spec(key, sep == ':', fheights);
    overlay_fheights(spec);

    return "";
}

bool map_string_list::parse(const string &fp, int weight)
{
    emplace_back(fp, weight);
    return !fp.empty();
}

string map_lines::add_subst(const string &sub)
{
    string s = trimmed_string(sub);

    if (s.empty())
        return "";

    int sep = 0;
    string key;
    string substitute;

    string err = mapdef_split_key_item(sub, &key, &sep, &substitute, -1);
    if (!err.empty())
        return err;

    glyph_replacements_t repl;
    err = parse_glyph_replacements(substitute, repl);
    if (!err.empty())
        return err;

    subst_spec spec(key, sep == ':', repl);
    subst(spec);

    return "";
}

string map_lines::parse_nsubst_spec(const string &s, subst_spec &spec)
{
    string key, arg;
    int sep;
    string err = mapdef_split_key_item(s, &key, &sep, &arg, -1);
    if (!err.empty())
        return err;
    int count = 0;
    if (key == "*")
        count = -1;
    else
        parse_int(key.c_str(), count);
    if (!count)
        return make_stringf("Illegal spec: %s", s.c_str());

    glyph_replacements_t repl;
    err = parse_glyph_replacements(arg, repl);
    if (!err.empty())
        return err;

    spec = subst_spec(count, sep == ':', repl);
    return "";
}

string map_lines::add_nsubst(const string &s)
{
    vector<subst_spec> substs;

    int sep;
    string key, arg;

    string err = mapdef_split_key_item(s, &key, &sep, &arg, -1);
    if (!err.empty())
        return err;

    vector<string> segs = split_string("/", arg);
    for (int i = 0, vsize = segs.size(); i < vsize; ++i)
    {
        string &ns = segs[i];
        if (ns.find('=') == string::npos && ns.find(':') == string::npos)
        {
            if (i < vsize - 1)
                ns = "1=" + ns;
            else
                ns = "*=" + ns;
        }
        subst_spec spec;
        err = parse_nsubst_spec(ns, spec);
        if (!err.empty())
        {
            return make_stringf("Bad NSUBST spec: %s (%s)",
                                s.c_str(), err.c_str());
        }
        substs.push_back(spec);
    }

    nsubst_spec spec(key, substs);
    nsubst(spec);

    return "";
}

string map_lines::add_shuffle(const string &raws)
{
    string s = raws;
    const string err = check_shuffle(s);

    if (err.empty())
        resolve_shuffle(s);

    return err;
}

string map_lines::add_clear(const string &raws)
{
    string s = raws;
    const string err = check_clear(s);

    if (err.empty())
        clear(s);

    return err;
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
    min_width = max(1, min_width);
    min_height = max(1, min_height);

    bool dirty = false;
    int old_width = width();
    int old_height = height();

    if (static_cast<int>(lines.size()) < min_height)
    {
        dirty = true;
        while (static_cast<int>(lines.size()) < min_height)
            add_line(string(min_width, fill));
    }

    if (width() < min_width)
    {
        dirty = true;
        lines[0] += string(min_width - width(), fill);
        map_width = max(map_width, min_width);
    }

    if (!dirty)
        return;

    normalise(fill);

    // Extend overlay matrix as well.
    if (overlay)
    {
        auto new_overlay = make_unique<overlay_matrix>(width(), height());

        for (int y = 0; y < old_height; ++y)
            for (int x = 0; x < old_width; ++x)
                (*new_overlay)(x, y) = (*overlay)(x, y);

        overlay = std::move(new_overlay);
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
    return gly == 'x' || gly == 'c' || gly == 'b' || gly == 'v' || gly == 't'
           || gly == 'X';
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
    default: return false;
    }
}

void map_lines::clear()
{
    clear_markers();
    lines.clear();
    keyspecs.clear();
    overlay.reset(nullptr);
    map_width = 0;
    solid_checked = false;

    // First non-legal character.
    next_keyspec_idx = 256;
}

void map_lines::subst(string &s, subst_spec &spec)
{
    string::size_type pos = 0;
    while ((pos = s.find_first_of(spec.key, pos)) != string::npos)
        s[pos++] = spec.value();
}

void map_lines::subst(subst_spec &spec)
{
    ASSERT(!spec.key.empty());
    for (string &line : lines)
        subst(line, spec);
}

void map_lines::bind_overlay()
{
    if (!overlay)
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

void map_lines::fill_mask_matrix(const string &glyphs,
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
            flags(ox, oy) = (strchr(glyphs.c_str(), (*this)(x, y)) != nullptr);
        }
}

map_corner_t map_lines::merge_subvault(const coord_def &mtl,
                                       const coord_def &mbr,
                                       const Matrix<bool> &mask,
                                       const map_def &vmap)
{
    const map_lines &vlines = vmap.map;

    // If vault is bigger than the mask region (mtl, mbr), then it gets
    // randomly centered. (vtl, vbr) stores the vault's region.
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

    if (!overlay)
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
    for (map_marker *mm : vlines.markers)
    {
        coord_def mapc = mm->pos + vtl;
        coord_def maskc = mapc - mtl;

        if (!mask(maskc.x, maskc.y))
            continue;

        map_marker *clone = mm->clone();
        clone->pos = mapc;
        add_marker(clone);
    }

    unsigned mask_tags = 0;

    // TODO: merge the matching of tags to MMTs into a function that is
    // called from both here and dungeon.cc::dgn_register_place.
    if (vmap.has_tag("no_monster_gen"))
        mask_tags |= MMT_NO_MONS;
    if (vmap.has_tag("no_item_gen"))
        mask_tags |= MMT_NO_ITEM;
    if (vmap.has_tag("no_pool_fixup"))
        mask_tags |= MMT_NO_POOL;
    if (vmap.has_tag("no_wall_fixup"))
        mask_tags |= MMT_NO_WALL;
    if (vmap.has_tag("no_trap_gen"))
        mask_tags |= MMT_NO_TRAP;

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
                    // Normal glyph. Turn into a feature keyspec.
                    // This is valid for non-array items and monsters
                    // as well, e.g. '$' and '8'.
                    keyed_mapspec &km = keyspecs[idx];
                    km.key_glyph = SUBVAULT_GLYPH;
                    km.feat.feats.clear();

                    feature_spec spec(-1);
                    spec.glyph = c;
                    km.feat.feats.insert(km.feat.feats.begin(), spec);
                }

                // Add overall tags to the keyspec.
                keyspecs[idx].map_mask.flags_set
                    |= (mask_tags & ~keyspecs[idx].map_mask.flags_unset);
            }

            // Finally, handle merging the cell itself.

            // Glyph becomes SUBVAULT_GLYPH. (The old glyph gets merged into a
            // keyspec, above). This is so that the glyphs that are included
            // from a subvault are immutable by the parent vault. Otherwise,
            // latent transformations (like KMONS or KITEM) from the parent
            // vault might confusingly modify a glyph from the subvault.
            //
            // NOTE: It'd be possible to allow subvaults to be modified by the
            // parent vault, but KMONS/KITEM/KFEAT/MONS/ITEM would have to
            // apply immediately instead of latently. They would also then
            // need to be stored per-coord, rather than per-glyph.
            (*this)(x, y) = SUBVAULT_GLYPH;

            // Merge overlays
            if (vlines.overlay)
                (*overlay)(x, y) = (*vlines.overlay)(vx, vy);
            else
            {
                // Erase any existing overlay, as the vault's doesn't exist.
                (*overlay)(x, y) = overlay_def();
            }

            // Set keyspec index for this subvault.
            (*overlay)(x, y).keyspec_idx = idx;
        }

    dprf(DIAG_DNGN, "Merged subvault '%s' at %d,%d x %d,%d",
        vmap.name.c_str(), vtl.x, vtl.y, vbr.x, vbr.y);

    return map_corner_t(vtl, vbr);
}

void map_lines::overlay_tiles(tile_spec &spec)
{
    if (!overlay)
        overlay.reset(new overlay_matrix(width(), height()));

    for (int y = 0, ysize = lines.size(); y < ysize; ++y)
    {
        string::size_type pos = 0;
        while ((pos = lines[y].find_first_of(spec.key, pos)) != string::npos)
        {
            if (spec.floor)
                (*overlay)(pos, y).floortile = spec.get_tile();
            else if (spec.feat)
                (*overlay)(pos, y).tile      = spec.get_tile();
            else
                (*overlay)(pos, y).rocktile  = spec.get_tile();

            (*overlay)(pos, y).no_random = spec.no_random;
            (*overlay)(pos, y).last_tile = spec.last_tile;
            ++pos;
        }
    }
}

void map_lines::nsubst(nsubst_spec &spec)
{
    vector<coord_def> positions;
    for (int y = 0, ysize = lines.size(); y < ysize; ++y)
    {
        string::size_type pos = 0;
        while ((pos = lines[y].find_first_of(spec.key, pos)) != string::npos)
            positions.emplace_back(pos++, y);
    }
    shuffle_array(positions);

    int pcount = 0;
    const int psize = positions.size();
    for (int i = 0, vsize = spec.specs.size();
         i < vsize && pcount < psize; ++i)
    {
        const int nsubsts = spec.specs[i].count;
        pcount += apply_nsubst(positions, pcount, nsubsts, spec.specs[i]);
    }
}

int map_lines::apply_nsubst(vector<coord_def> &pos, int start, int nsub,
                            subst_spec &spec)
{
    if (nsub == -1)
        nsub = pos.size();
    const int end = min(start + nsub, (int) pos.size());
    int substituted = 0;
    for (int i = start; i < end; ++i)
    {
        const int val = spec.value();
        const coord_def &c = pos[i];
        lines[c.y][c.x] = val;
        ++substituted;
    }
    return substituted;
}

string map_lines::block_shuffle(const string &s)
{
    vector<string> segs = split_string("/", s);
    shuffle_array(segs);
    return comma_separated_line(segs.begin(), segs.end(), "/", "/");
}

string map_lines::shuffle(string s)
{
    string result;

    if (s.find('/') != string::npos)
        return block_shuffle(s);

    // Inefficient brute-force shuffle.
    while (!s.empty())
    {
        const int c = random2(s.length());
        result += s[c];
        s.erase(c, 1);
    }

    return result;
}

void map_lines::resolve_shuffle(const string &shufflage)
{
    string toshuffle = shufflage;
    string shuffled = shuffle(toshuffle);

    if (toshuffle.empty() || shuffled.empty())
        return;

    for (string &s : lines)
    {
        for (char &c : s)
        {
            string::size_type pos = toshuffle.find(c);
            if (pos != string::npos)
                c = shuffled[pos];
        }
    }
}

void map_lines::clear(const string &clearchars)
{
    for (string &s : lines)
    {
        for (char &c : s)
        {
            string::size_type pos = clearchars.find(c);
            if (pos != string::npos)
                c = ' ';
        }
    }
}

void map_lines::normalise(char fillch)
{
    for (string &s : lines)
        if (static_cast<int>(s.length()) < map_width)
            s += string(map_width - s.length(), fillch);
}

// Should never be attempted if the map has a defined orientation, or if one
// of the dimensions is greater than the lesser of GXM,GYM.
void map_lines::rotate(bool clockwise)
{
    vector<string> newlines;

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
        string line;

        for (int j = ys; j != ye; j += yi)
            line += lines[j][i];

        newlines.push_back(line);
    }

    if (overlay)
    {
        auto new_overlay = make_unique<overlay_matrix>(lines.size(), map_width);
        for (int i = xs, y = 0; i != xe; i += xi, ++y)
            for (int j = ys, x = 0; j != ye; j += yi, ++x)
                (*new_overlay)(x, y) = (*overlay)(i, j);
        overlay = std::move(new_overlay);
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
    for (map_marker *marker : markers)
        (this->*xform)(marker, par);
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
        string temp = lines[i];
        lines[i] = lines[vsize - 1 - i];
        lines[vsize - 1 - i] = temp;
    }

    if (overlay)
    {
        for (int i = 0; i < midpoint; ++i)
            for (int j = 0, wide = width(); j < wide; ++j)
                swap((*overlay)(j, i), (*overlay)(j, vsize - 1 - i));
    }

    vmirror_markers();
    solid_checked = false;
}

void map_lines::hmirror()
{
    const int midpoint = map_width / 2;
    for (string &s : lines)
        for (int j = 0; j < midpoint; ++j)
            swap(s[j], s[map_width - 1 - j]);

    if (overlay)
    {
        for (int i = 0, vsize = lines.size(); i < vsize; ++i)
            for (int j = 0; j < midpoint; ++j)
                swap((*overlay)(j, i), (*overlay)(map_width - 1 - j, i));
    }

    hmirror_markers();
    solid_checked = false;
}

keyed_mapspec *map_lines::mapspec_for_key(int key)
{
    return map_find(keyspecs, key);
}

const keyed_mapspec *map_lines::mapspec_for_key(int key) const
{
    return map_find(keyspecs, key);
}

keyed_mapspec *map_lines::mapspec_at(const coord_def &c)
{
    int key = (*this)(c);

    if (key == SUBVAULT_GLYPH)
    {
        // Any subvault should create the overlay.
        ASSERT(overlay);
        if (!overlay)
            return nullptr;

        key = (*overlay)(c.x, c.y).keyspec_idx;
        ASSERT(key);
        if (!key)
            return nullptr;
    }

    return mapspec_for_key(key);
}

const keyed_mapspec *map_lines::mapspec_at(const coord_def &c) const
{
    int key = (*this)(c);

    if (key == SUBVAULT_GLYPH)
    {
        // Any subvault should create the overlay and set the keyspec idx.
        ASSERT(overlay);
        if (!overlay)
            return nullptr;

        key = (*overlay)(c.x, c.y).keyspec_idx;
        ASSERT(key);
        if (!key)
            return nullptr;
    }

    return mapspec_for_key(key);
}

string map_lines::add_key_field(
    const string &s,
    string (keyed_mapspec::*set_field)(const string &s, bool fixed),
    void (keyed_mapspec::*copy_field)(const keyed_mapspec &spec))
{
    int separator = 0;
    string key, arg;

    string err = mapdef_split_key_item(s, &key, &separator, &arg, -1);
    if (!err.empty())
        return err;

    keyed_mapspec &kmbase = keyspecs[key[0]];
    kmbase.key_glyph = key[0];
    err = ((kmbase.*set_field)(arg, separator == ':'));
    if (!err.empty())
        return err;

    size_t len = key.length();
    for (size_t i = 1; i < len; i++)
    {
        keyed_mapspec &km = keyspecs[key[i]];
        km.key_glyph = key[i];
        ((km.*copy_field)(kmbase));
    }

    return err;
}

string map_lines::add_key_item(const string &s)
{
    return add_key_field(s, &keyed_mapspec::set_item,
                         &keyed_mapspec::copy_item);
}

string map_lines::add_key_feat(const string &s)
{
    return add_key_field(s, &keyed_mapspec::set_feat,
                         &keyed_mapspec::copy_feat);
}

string map_lines::add_key_mons(const string &s)
{
    return add_key_field(s, &keyed_mapspec::set_mons,
                         &keyed_mapspec::copy_mons);
}

string map_lines::add_key_mask(const string &s)
{
    return add_key_field(s, &keyed_mapspec::set_mask,
                         &keyed_mapspec::copy_mask);
}

vector<coord_def> map_lines::find_glyph(int gly) const
{
    vector<coord_def> points;
    for (int y = height() - 1; y >= 0; --y)
    {
        for (int x = width() - 1; x >= 0; --x)
        {
            const coord_def c(x, y);
            if ((*this)(c) == gly)
                points.push_back(c);
        }
    }
    return points;
}

vector<coord_def> map_lines::find_glyph(const string &glyphs) const
{
    vector<coord_def> points;
    for (int y = height() - 1; y >= 0; --y)
    {
        for (int x = width() - 1; x >= 0; --x)
        {
            const coord_def c(x, y);
            if (glyphs.find((*this)(c)) != string::npos)
                points.push_back(c);
        }
    }
    return points;
}

coord_def map_lines::find_first_glyph(int gly) const
{
    for (int y = 0, h = height(); y < h; ++y)
    {
        string::size_type pos = lines[y].find(gly);
        if (pos != string::npos)
            return coord_def(pos, y);
    }

    return coord_def(-1, -1);
}

coord_def map_lines::find_first_glyph(const string &glyphs) const
{
    for (int y = 0, h = height(); y < h; ++y)
    {
        string::size_type pos = lines[y].find_first_of(glyphs);
        if (pos != string::npos)
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

        tl.x = min(tl.x, mc.x);
        tl.y = min(tl.y, mc.y);
        br.x = max(br.x, mc.x);
        br.y = max(br.y, mc.y);
    }

    return br.x >= 0;
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
                tl.x = min(tl.x, mc.x);
                tl.y = min(tl.y, mc.y);
                br.x = max(br.x, mc.x);
                br.y = max(br.y, mc.y);
                break;
            }
        }
    }

    return br.x >= 0;
}

bool map_lines::fill_zone(travel_distance_grid_t &tpd, const coord_def &start,
                          const coord_def &tl, const coord_def &br, int zone,
                          const char *wanted, const char *passable) const
{
    // This is the map_lines equivalent of _dgn_fill_zone.
    // It's unfortunately extremely similar, but not close enough to combine.

    bool ret = false;
    list<coord_def> points[2];
    int cur = 0;

    for (points[cur].push_back(start); !points[cur].empty();)
    {
        for (const auto &c : points[cur])
        {
            tpd[c.x][c.y] = zone;

            ret |= (wanted && strchr(wanted, (*this)(c)) != nullptr);

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
    return ret;
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

    return result;
}

bool map_tile_list::parse(const string &s, int weight)
{
    tileidx_t idx = 0;
    if (s != "none" && !tile_dngn_index(s.c_str(), &idx))
        return false;

    emplace_back(s, weight);
    return true;
}

string map_lines::add_tile(const string &sub, bool is_floor, bool is_feat)
{
    string s = trimmed_string(sub);

    if (s.empty())
        return "";

    bool no_random = strip_tag(s, "no_random");
    bool last_tile = strip_tag(s, "last_tile");

    int sep = 0;
    string key;
    string substitute;

    string err = mapdef_split_key_item(s, &key, &sep, &substitute, -1);
    if (!err.empty())
        return err;

    map_tile_list list;
    err = _parse_weighted_str<map_tile_list>(substitute, list);
    if (!err.empty())
        return err;

    tile_spec spec(key, sep == ':', no_random, last_tile, is_floor, is_feat, list);
    overlay_tiles(spec);

    return "";
}

string map_lines::add_rocktile(const string &sub)
{
    return add_tile(sub, false, false);
}

string map_lines::add_floortile(const string &sub)
{
    return add_tile(sub, true, false);
}

string map_lines::add_spec_tile(const string &sub)
{
    return add_tile(sub, false, true);
}

//////////////////////////////////////////////////////////////////////////
// tile_spec

string tile_spec::get_tile()
{
    if (chose_fixed)
        return fixed_tile;

    string chosen = "";
    int cweight = 0;
    for (const map_weighted_tile &tile : tiles)
        if (x_chance_in_y(tile.second, cweight += tile.second))
            chosen = tile.first;

    if (fix)
    {
        chose_fixed = true;
        fixed_tile  = chosen;
    }
    return chosen;
}

//////////////////////////////////////////////////////////////////////////
// map_lines::iterator

map_lines::iterator::iterator(map_lines &_maplines, const string &_key)
    : maplines(_maplines), key(_key), p(0, 0)
{
    advance();
}

void map_lines::iterator::advance()
{
    const int height = maplines.height();
    while (p.y < height)
    {
        string::size_type place = p.x;
        if (place < maplines.lines[p.y].length())
        {
            place = maplines.lines[p.y].find_first_of(key, place);
            if (place != string::npos)
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
    return p.y < maplines.height();
}

coord_def map_lines::iterator::operator *() const
{
    return p;
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
    return here;
}

///////////////////////////////////////////////
// dlua_set_map

dlua_set_map::dlua_set_map(map_def *map)
{
    clua_push_map(dlua, map);
    if (!dlua.callfn("dgn_set_map", 1, 1))
    {
        mprf(MSGCH_ERROR, "dgn_set_map failed for '%s': %s",
             map->name.c_str(), dlua.error.c_str());
    }
    // Save the returned map as a lua_datum
    old_map.reset(new lua_datum(dlua));
}

dlua_set_map::~dlua_set_map()
{
    old_map->push();
    if (!dlua.callfn("dgn_set_map", 1, 0))
        mprf(MSGCH_ERROR, "dgn_set_map failed: %s", dlua.error.c_str());
}

///////////////////////////////////////////////
// map_chance

string map_chance::describe() const
{
    return make_stringf("%d", chance);
}

bool map_chance::roll() const
{
    return random2(CHANCE_ROLL) < chance;
}

void map_chance::write(writer &outf) const
{
    marshallInt(outf, chance);
}

void map_chance::read(reader &inf)
{
#if TAG_MAJOR_VERSION == 34
    if (inf.getMinorVersion() < TAG_MINOR_NO_PRIORITY)
        unmarshallInt(inf); // was chance_priority
#endif
    chance = unmarshallInt(inf);
}

///////////////////////////////////////////////
// depth_ranges

void depth_ranges::write(writer& outf) const
{
    marshallShort(outf, depths.size());
    for (const level_range &depth : depths)
        depth.write(outf);
}

void depth_ranges::read(reader &inf)
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

depth_ranges depth_ranges::parse_depth_ranges(const string &depth_range_string)
{
    depth_ranges ranges;
    for (const string &frag : split_string(",", depth_range_string))
        ranges.depths.push_back(level_range::parse(frag));
    return ranges;
}

bool depth_ranges::is_usable_in(const level_id &lid) const
{
    bool any_matched = false;
    for (const level_range &lr : depths)
    {
        if (lr.matches(lid))
        {
            if (lr.deny)
                return false;
            any_matched = true;
        }
    }
    return any_matched;
}

void depth_ranges::add_depths(const depth_ranges &other_depths)
{
    depths.insert(depths.end(),
                  other_depths.depths.begin(),
                  other_depths.depths.end());
}

string depth_ranges::describe() const
{
    return comma_separated_line(depths.begin(), depths.end(), ", ", ", ");
}

///////////////////////////////////////////////
// map_def
//

const int DEFAULT_MAP_WEIGHT = 10;
map_def::map_def()
    : name(), description(), order(INT_MAX), place(), depths(),
      orient(), _chance(), _weight(DEFAULT_MAP_WEIGHT),
      map(), mons(), items(), random_mons(),
      prelude("dlprelude"), mapchunk("dlmapchunk"), main("dlmain"),
      validate("dlvalidate"), veto("dlveto"), epilogue("dlepilogue"),
      rock_colour(BLACK), floor_colour(BLACK), rock_tile(""),
      floor_tile(""), border_fill_type(DNGN_ROCK_WALL),
      tags(),
      index_only(false), cache_offset(0L), validating_map_flag(false),
      cache_minivault(false), cache_overwritable(false), cache_extra(false)
{
    init();
}

void map_def::init()
{
    orient = MAP_NONE;
    name.clear();
    description.clear();
    order = INT_MAX;
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
    svmask = nullptr;
}

void map_def::reinit()
{
    description.clear();
    order = INT_MAX;
    items.clear();
    random_mons.clear();

    rock_colour = floor_colour = BLACK;
    rock_tile = floor_tile = "";
    border_fill_type = DNGN_ROCK_WALL;

    // Chance of using this level. Nonzero chance should be used
    // sparingly. When selecting vaults for a place, first those
    // vaults with chance > 0 are considered, in the order they were
    // loaded (which is arbitrary). If random2(100) < chance, the
    // vault is picked, and all other vaults are ignored for that
    // random selection. weight is ignored if the vault is chosen
    // based on its chance.
    _chance.clear();

    // Weight for this map. When selecting a map, if no map with a
    // nonzero chance is picked, one of the other eligible vaults is
    // picked with a probability of weight / (sum of weights of all
    // eligible vaults).
    _weight.clear(DEFAULT_MAP_WEIGHT);

    // Clearing the map also zaps map transforms.
    map.clear();
    mons.clear();
    feat_renames.clear();
    subvault_places.clear();
    update_cached_tags();
}

void map_def::reload_epilogue()
{
    if (!epilogue.empty())
        return;
    // reload the epilogue from the current .des cache; this is because it
    // isn't serialized but with pregen orderings could need to be run after
    // vaults have been generated, saved, and reloaded. This can be a tricky
    // situation in save-compat terms, but is exactly the same as how lua code
    // triggered by markers is currently handled.
    const map_def *cache_version = find_map_by_name(name);
    if (cache_version)
    {
        map_def tmp = *cache_version;
        // TODO: I couldn't reliably get the epilogue to be in vdefs, is there
        // a way to cache it and not do a full load here? The original idea
        // was to not clear it out in strip(), but this fails under some
        // circumstances.
        tmp.load();
        epilogue = tmp.epilogue;
    }
    // for save compat reasons, fail silently if the map is no longer around.
    // Probably shouldn't do anything really crucial in the epilogue that you
    // aren't prepared to deal with save compat for somehow...
}

bool map_def::map_already_used() const
{
    return get_uniq_map_names().count(name)
           || env.level_uniq_maps.find(name) !=
               env.level_uniq_maps.end()
           || env.new_used_subvault_names.find(name) !=
               env.new_used_subvault_names.end()
           || has_any_tag(get_uniq_map_tags().begin(),
                          get_uniq_map_tags().end())
           || has_any_tag(env.level_uniq_map_tags.begin(),
                          env.level_uniq_map_tags.end())
           || has_any_tag(env.new_used_subvault_tags.begin(),
                          env.new_used_subvault_tags.end());
}

bool map_def::valid_item_array_glyph(int gly)
{
    return gly >= 'd' && gly <= 'k';
}

int map_def::item_array_glyph_to_slot(int gly)
{
    ASSERT(map_def::valid_item_array_glyph(gly));
    return gly - 'd';
}

bool map_def::valid_monster_glyph(int gly)
{
    return gly >= '0' && gly <= '9';
}

bool map_def::valid_monster_array_glyph(int gly)
{
    return gly >= '1' && gly <= '7';
}

int map_def::monster_array_glyph_to_slot(int gly)
{
    ASSERT(map_def::valid_monster_array_glyph(gly));
    return gly - '1';
}

bool map_def::in_map(const coord_def &c) const
{
    return map.in_map(c);
}

int map_def::glyph_at(const coord_def &c) const
{
    return map(c);
}

string map_def::name_at(const coord_def &c) const
{
    vector<string> names;
    names.push_back(name);
    for (const subvault_place& subvault : subvault_places)
    {
        if (c.x >= subvault.tl.x && c.x <= subvault.br.x &&
            c.y >= subvault.tl.y && c.y <= subvault.br.y &&
            subvault.subvault->in_map(c - subvault.tl))
        {
            names.push_back(subvault.subvault->name_at(c - subvault.tl));
        }
    }
    return comma_separated_line(names.begin(), names.end(), ", ", ", ");
}

string map_def::desc_or_name() const
{
    return description.empty()? name : description;
}

void map_def::write_full(writer& outf) const
{
    cache_offset = outf.tell();
    write_save_version(outf, save_version::current());
    marshallString4(outf, name);
    prelude.write(outf);
    mapchunk.write(outf);
    main.write(outf);
    validate.write(outf);
    veto.write(outf);
    epilogue.write(outf);
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

    const auto version = get_save_version(inf);
    const auto major = version.major, minor = version.minor;

    if (major != TAG_MAJOR_VERSION || minor > TAG_MINOR_VERSION)
    {
        throw map_load_exception(make_stringf(
            "Map was built for a different version of Crawl (%s) "
            "(map: %d.%d us: %d.%d)",
            name.c_str(), int(major), int(minor),
            TAG_MAJOR_VERSION, TAG_MINOR_VERSION));
    }

    string fp_name;
    unmarshallString4(inf, fp_name);

    if (fp_name != name)
    {
        throw map_load_exception(make_stringf(
            "Map fp_name (%s) != name (%s)!",
            fp_name.c_str(), name.c_str()));
    }

    prelude.read(inf);
    mapchunk.read(inf);
    main.read(inf);
    validate.read(inf);
    veto.read(inf);
    epilogue.read(inf);
}

int map_def::weight(const level_id &lid) const
{
    return _weight.depth_value(lid);
}

map_chance map_def::chance(const level_id &lid) const
{
    return _chance.depth_value(lid);
}

string map_def::describe() const
{
    return make_stringf("Map: %s\n%s%s%s%s%s%s",
                        name.c_str(),
                        prelude.describe("prelude").c_str(),
                        mapchunk.describe("mapchunk").c_str(),
                        main.describe("main").c_str(),
                        validate.describe("validate").c_str(),
                        veto.describe("veto").c_str(),
                        epilogue.describe("epilogue").c_str());
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
    feat_renames.clear();
}

void map_def::load()
{
    if (!index_only)
        return;

    const string descache_base = get_descache_path(cache_name, "");
    file_lock deslock(descache_base + ".lk", "rb", false);
    const string loadfile = descache_base + ".dsc";

    reader inf(loadfile, TAG_MINOR_VERSION);
    if (!inf.valid())
    {
        throw map_load_exception(
                make_stringf("Map inf is invalid: %s", name.c_str()));
    }
    inf.advance(cache_offset);
    read_full(inf);

    index_only = false;
}

vector<coord_def> map_def::find_glyph(int glyph) const
{
    return map.find_glyph(glyph);
}

coord_def map_def::find_first_glyph(int glyph) const
{
    return map.find_first_glyph(glyph);
}

coord_def map_def::find_first_glyph(const string &s) const
{
    return map.find_first_glyph(s);
}

void map_def::write_maplines(writer &outf) const
{
    map.write_maplines(outf);
}

static void _marshall_map_chance(writer &th, const map_chance &chance)
{
    chance.write(th);
}

static map_chance _unmarshall_map_chance(reader &th)
{
    map_chance chance;
    chance.read(th);
    return chance;
}

void map_def::write_index(writer& outf) const
{
    if (!cache_offset)
    {
        end(1, false, "Map %s: can't write index - cache offset not set!",
            name.c_str());
    }
    marshallString4(outf, name);
    marshallString4(outf, place_loaded_from.filename);
    marshallInt(outf, place_loaded_from.lineno);
    marshallShort(outf, orient);
    // XXX: This is a hack. See the comment in l-dgn.cc.
    marshallShort(outf, static_cast<short>(border_fill_type));
    _chance.write(outf, _marshall_map_chance);
    _weight.write(outf, marshallInt);
    marshallInt(outf, cache_offset);
    marshallString4(outf, tags_string());
    place.write(outf);
    depths.write(outf);
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
    place_loaded_from.lineno = unmarshallInt(inf);
    orient = static_cast<map_section_type>(unmarshallShort(inf));
    // XXX: Hack. See the comment in l-dgn.cc.
    border_fill_type =
        static_cast<dungeon_feature_type>(unmarshallShort(inf));

    _chance = range_chance_t::read(inf, _unmarshall_map_chance);
    _weight = range_weight_t::read(inf, unmarshallInt);
    cache_offset = unmarshallInt(inf);
    string read_tags;
    unmarshallString4(inf, read_tags);
    set_tags(read_tags);
    place.read(inf);
    depths.read(inf);
    prelude.read(inf);
    index_only = true;
}

void map_def::set_file(const string &s)
{
    prelude.set_file(s);
    mapchunk.set_file(s);
    main.set_file(s);
    validate.set_file(s);
    veto.set_file(s);
    epilogue.set_file(s);
    file = get_base_filename(s);
    cache_name = get_cache_name(s);
}

string map_def::run_lua(bool run_main)
{
    dlua_set_map mset(this);

    int err = prelude.load(dlua);
    if (err == E_CHUNK_LOAD_FAILURE)
        lua_pushnil(dlua);
    else if (err)
        return prelude.orig_error();
    if (!dlua.callfn("dgn_run_map", 1, 0))
        return rewrite_chunk_errors(dlua.error);

    if (run_main)
    {
        // Run the map chunk to set up the vault's map grid.
        err = mapchunk.load(dlua);
        if (err == E_CHUNK_LOAD_FAILURE)
            lua_pushnil(dlua);
        else if (err)
            return mapchunk.orig_error();
        if (!dlua.callfn("dgn_run_map", 1, 0))
            return rewrite_chunk_errors(dlua.error);

        // The vault may be non-rectangular with a ragged-right edge; for
        // transforms to work right at this point, we must pad out the right
        // edge with spaces, so run normalise:
        normalise();

        // Run the main Lua chunk to set up the rest of the vault
        run_hook("pre_main");
        err = main.load(dlua);
        if (err == E_CHUNK_LOAD_FAILURE)
            lua_pushnil(dlua);
        else if (err)
            return main.orig_error();
        if (!dlua.callfn("dgn_run_map", 1, 0))
            return rewrite_chunk_errors(dlua.error);
        run_hook("post_main");
    }

    return dlua.error;
}

void map_def::copy_hooks_from(const map_def &other_map, const string &hook_name)
{
    const dlua_set_map mset(this);
    if (!dlua.callfn("dgn_map_copy_hooks_from", "ss",
                     other_map.name.c_str(), hook_name.c_str()))
    {
        mprf(MSGCH_ERROR, "Lua error copying hook (%s) from '%s' to '%s': %s",
             hook_name.c_str(), other_map.name.c_str(),
             name.c_str(), dlua.error.c_str());
    }
}

// Runs Lua hooks registered by the map's Lua code, if any. Returns true if
// no errors occurred while running hooks.
bool map_def::run_hook(const string &hook_name, bool die_on_lua_error)
{
    const dlua_set_map mset(this);
    if (!dlua.callfn("dgn_map_run_hook", "s", hook_name.c_str()))
    {
        const string error = rewrite_chunk_errors(dlua.error);
        // only show the error message if this isn't a hook map-placement
        // failure, which should just lead to a silent veto.
        if (error.find("Failed to place map") == string::npos)
        {
            if (die_on_lua_error)
            {
                end(1, false, "Lua error running hook '%s' on map '%s': %s",
                    hook_name.c_str(), name.c_str(), error.c_str());
            }
            else
            {
                mprf(MSGCH_ERROR, "Lua error running hook '%s' on map '%s': %s",
                     hook_name.c_str(), name.c_str(), error.c_str());
            }
        }
        return false;
    }
    return true;
}

bool map_def::run_postplace_hook(bool die_on_lua_error)
{
    return run_hook("post_place", die_on_lua_error);
}

bool map_def::test_lua_boolchunk(dlua_chunk &chunk, bool defval,
                                 bool die_on_lua_error)
{
    bool result = defval;
    dlua_set_map mset(this);

    int err = chunk.load(dlua);
    if (err == E_CHUNK_LOAD_FAILURE)
        return result;
    else if (err)
    {
        if (die_on_lua_error)
            end(1, false, "Lua error: %s", chunk.orig_error().c_str());
        else
            mprf(MSGCH_ERROR, "Lua error: %s", chunk.orig_error().c_str());
        return result;
    }
    if (dlua.callfn("dgn_run_map", 1, 1))
        dlua.fnreturns(">b", &result);
    else
    {
        if (die_on_lua_error)
        {
            end(1, false, "Lua error: %s",
                rewrite_chunk_errors(dlua.error).c_str());
        }
        else
        {
            mprf(MSGCH_ERROR, "Lua error: %s",
                 rewrite_chunk_errors(dlua.error).c_str());
        }
    }
    return result;
}

bool map_def::test_lua_validate(bool croak)
{
    return validate.empty() || test_lua_boolchunk(validate, false, croak);
}

bool map_def::test_lua_veto()
{
    return !veto.empty() && test_lua_boolchunk(veto, true);
}

bool map_def::run_lua_epilogue(bool die_on_lua_error)
{
    run_hook("pre_epilogue", die_on_lua_error);
    const bool epilogue_result =
        !epilogue.empty() && test_lua_boolchunk(epilogue, false,
                                                die_on_lua_error);
    run_hook("post_epilogue", die_on_lua_error);
    return epilogue_result;
}

string map_def::rewrite_chunk_errors(const string &s) const
{
    string res = s;
    if (prelude.rewrite_chunk_errors(res))
        return res;
    if (mapchunk.rewrite_chunk_errors(res))
        return res;
    if (main.rewrite_chunk_errors(res))
        return res;
    if (validate.rewrite_chunk_errors(res))
        return res;
    if (veto.rewrite_chunk_errors(res))
        return res;
    epilogue.rewrite_chunk_errors(res);
    return res;
}

string map_def::validate_temple_map()
{
    vector<coord_def> altars = find_glyph('B');

    if (has_tag_prefix("temple_overflow_"))
    {
        if (has_tag_prefix("temple_overflow_generic_"))
        {
            string matching_tag = make_stringf("temple_overflow_generic_%u",
                (unsigned int) altars.size());
            if (!has_tag(matching_tag))
            {
                return make_stringf(
                    "Temple ('%s') has %u altars and a "
                    "'temple_overflow_generic_' tag, but does not match the "
                    "number of altars: should have at least '%s'.",
                                    tags_string().c_str(),
                                    (unsigned int) altars.size(),
                                    matching_tag.c_str());
            }
        }
        else
        {
            // Assume specialised altar vaults are set up correctly.
            return "";
        }
    }

    if (altars.empty())
        return "Temple vault must contain at least one altar.";

    // TODO: check for substitutions and shuffles

    vector<coord_def> b_glyphs = map.find_glyph('B');
    for (auto c : b_glyphs)
    {
        const keyed_mapspec *spec = map.mapspec_at(c);
        if (spec != nullptr && !spec->feat.feats.empty())
            return "Can't change feat 'B' in temple (KFEAT)";
    }

    vector<god_type> god_list = temple_god_list();

    if (altars.size() > god_list.size())
        return "Temple vault has too many altars";

    return "";
}

string map_def::validate_map_placeable()
{
    if (has_depth() || !place.empty())
        return "";

    // Ok, the map wants to be placed by tag. In this case it should have
    // at least one tag that's not a map flag.
    bool has_selectable_tag = false;
    for (const string &piece : tags)
    {
        if (_map_tag_is_selectable(piece))
        {
            has_selectable_tag = true;
            break;
        }
    }

    return has_selectable_tag? "" :
           make_stringf("Map '%s' has no DEPTH, no PLACE and no "
                        "selectable tag in '%s'",
                        name.c_str(), tags_string().c_str());
}

/**
 * Check to see if the vault can connect normally to the rest of the dungeon.
 */
bool map_def::has_exit() const
{
    map_def dup = *this;
    for (int y = 0, cheight = map.height(); y < cheight; ++y)
        for (int x = 0, cwidth = map.width(); x < cwidth; ++x)
        {
            if (!map.in_map(coord_def(x, y)))
                continue;
            const char glyph = map.glyph(x, y);
            dungeon_feature_type feat =
                map_feature_at(&dup, coord_def(x, y), -1);
            // If we have a stair, assume the vault can be disconnected.
            if (feat_is_stair(feat) && !feat_is_escape_hatch(feat))
                return true;
            const bool non_floating =
                glyph == '@' || glyph == '=' || glyph == '+';
            if (non_floating
                || !feat_is_solid(feat) || feat_is_closed_door(feat))
            {
                if (x == 0 || x == cwidth - 1 || y == 0 || y == cheight - 1)
                    return true;
                for (orth_adjacent_iterator ai(coord_def(x, y)); ai; ++ai)
                    if (!map.in_map(*ai))
                        return true;
            }
        }

    return false;
}

string map_def::validate_map_def(const depth_ranges &default_depths)
{
    UNUSED(default_depths);

    unwind_bool valid_flag(validating_map_flag, true);

    string err = run_lua(true);
    if (!err.empty())
        return err;

    fixup();
    resolve();
    test_lua_validate(true);
    run_lua_epilogue(true);

    if (!has_depth() && !lc_default_depths.empty())
        depths.add_depths(lc_default_depths);

    if (place.is_usable_in(level_id(BRANCH_TEMPLE))
        || has_tag_prefix("temple_overflow_"))
    {
        err = validate_temple_map();
        if (!err.empty())
            return err;
    }

    if (has_tag("overwrite_floor_cell") && (map.width() != 1 || map.height() != 1))
        return "Map tagged 'overwrite_floor_cell' must be 1x1";

    // Abyssal vaults have additional size and orientation restrictions.
    if (has_tag("abyss") || has_tag("abyss_rune"))
    {
        if (orient == MAP_ENCOMPASS)
        {
            return make_stringf(
                "Map '%s' cannot use 'encompass' orientation in the abyss",
                name.c_str());
        }

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
            min(max_abyss_map_height, max_abyss_map_width);
        if ((map.width() > dimension_lower_bound
             || map.height() > dimension_lower_bound)
            && !has_tag("no_rotate"))
        {
            add_tags("no_rotate");
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
        if (map.width() > GXM || map.height() > GYM)
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
        {
            return make_stringf("Map too large - height %d (max %d)",
                                map.height(), GYM * 2 / 3);
        }
        break;
    case MAP_EAST: case MAP_WEST:
        if (map.width() > GXM * 2 / 3)
        {
            return make_stringf("Map too large - width %d (max %d)",
                                map.width(), GXM * 2 / 3);
        }
        break;
    case MAP_NORTHEAST: case MAP_SOUTHEAST:
    case MAP_NORTHWEST: case MAP_SOUTHWEST:
    case MAP_FLOAT:     case MAP_CENTRE:
        if (map.width() > GXM * 2 / 3 || map.height() > GYM * 2 / 3)
        {
            return make_stringf("Map too large - %dx%d (max %dx%d)",
                                map.width(), map.height(),
                                GXM * 2 / 3, GYM * 2 / 3);
        }
        break;
    default:
        break;
    }

    // Encompass vaults, pure subvaults, and dummy vaults are exempt from
    // exit-checking.
    if (orient != MAP_ENCOMPASS && !has_tag("unrand") && !has_tag("dummy")
        && !has_tag("no_exits") && map.width() > 0 && map.height() > 0)
    {
        if (!has_exit())
        {
            return make_stringf(
                "Map '%s' has no (possible) exits; use TAGS: no_exits if "
                "this is intentional",
                name.c_str());
        }
    }

    dlua_set_map dl(this);
    return validate_map_placeable();
}

bool map_def::is_usable_in(const level_id &lid) const
{
    return depths.is_usable_in(lid);
}

void map_def::add_depth(const level_range &range)
{
    depths.add_depth(range);
}

bool map_def::has_depth() const
{
    return !depths.empty();
}

void map_def::update_cached_tags()
{
    cache_minivault = has_tag("minivault");
    cache_overwritable = has_tag("overwritable");
    cache_extra = has_tag("extra");
}

bool map_def::is_minivault() const
{
#ifdef DEBUG_TAG_PROFILING
    ASSERT(cache_minivault == has_tag("minivault"));
#endif
    return cache_minivault;
}

// Returns true if the map is a layout that allows other vaults to be
// built on it.
bool map_def::is_overwritable_layout() const
{
    // XX this code apparently does *not* check whether something is a layout.
    // In almost all cases "overwritable" and "layout" coincide, but there are
    // cases where they don't...
#ifdef DEBUG_TAG_PROFILING
    ASSERT(cache_overwritable == has_tag("overwritable"));
#endif
    return cache_overwritable;
}

bool map_def::is_extra_vault() const
{
#ifdef DEBUG_TAG_PROFILING
    ASSERT(cache_extra == has_tag("extra"));
#endif
    return cache_extra;
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

    for (map_section_type sec : orients)
    {
        if (map.solid_borders(sec) && can_dock(sec)
            && one_chance_in(++norients))
        {
            which_orient = sec;
        }
    }

    if (which_orient == MAP_NONE || which_orient == MAP_FLOAT)
        return coord_def(-1, -1);

    dprf(DIAG_DNGN, "Docking floating vault to %s",
         map_section_name(which_orient));

    return dock_pos(which_orient);
}

coord_def map_def::dock_pos(map_section_type norient) const
{
    const int minborder = 6;

    switch (norient)
    {
    case MAP_NORTH:
        return coord_def((GXM - map.width()) / 2, minborder);
    case MAP_SOUTH:
        return coord_def((GXM - map.width()) / 2,
                          GYM - minborder - map.height());
    case MAP_EAST:
        return coord_def(GXM - minborder - map.width(),
                          (GYM - map.height()) / 2);
    case MAP_WEST:
        return coord_def(minborder,
                          (GYM - map.height()) / 2);
    case MAP_NORTHEAST:
        return coord_def(GXM - minborder - map.width(), minborder);
    case MAP_NORTHWEST:
        return coord_def(minborder, minborder);
    case MAP_SOUTHEAST:
        return coord_def(GXM - minborder - map.width(),
                          GYM - minborder - map.height());
    case MAP_SOUTHWEST:
        return coord_def(minborder,
                          GYM - minborder - map.height());
    case MAP_CENTRE:
        return coord_def((GXM - map.width())  / 2,
                         (GYM - map.height()) / 2);
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
        return true;
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

    coord_def result;
    result.x = random_range(minhborder, GXM - minhborder - map.width());
    result.y = random_range(minvborder, GYM - minvborder - map.height());
    return result;
}

point_vector map_def::anchor_points() const
{
    point_vector points;
    for (int y = 0, cheight = map.height(); y < cheight; ++y)
        for (int x = 0, cwidth = map.width(); x < cwidth; ++x)
            if (map.glyph(x, y) == '@')
                points.emplace_back(x, y);
    return points;
}

coord_def map_def::float_aligned_place() const
{
    const point_vector our_anchors = anchor_points();
    const coord_def fail(-1, -1);

    dprf(DIAG_DNGN, "Aligning floating vault with %u points vs %u"
                    " reference points",
                    (unsigned int)our_anchors.size(),
                    (unsigned int)map_anchor_points.size());

    // Mismatch in the number of points we have to align, bail.
    if (our_anchors.size() != map_anchor_points.size())
        return fail;

    // Align first point of both vectors, then check that the others match.
    const coord_def pos = map_anchor_points[0] - our_anchors[0];

    for (int i = 1, psize = map_anchor_points.size(); i < psize; ++i)
        if (pos + our_anchors[i] != map_anchor_points[i])
            return fail;

    // Looking good, check bounds.
    if (!map_bounds(pos) || !map_bounds(pos + size() - 1))
        return fail;

    // Go us!
    return pos;
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

    return pos;
}

void map_def::hmirror()
{
    if (has_tag("no_hmirror"))
        return;

    dprf(DIAG_DNGN, "Mirroring %s horizontally.", name.c_str());
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

    for (subvault_place &sv : subvault_places)
    {

        coord_def old_tl = sv.tl;
        coord_def old_br = sv.br;
        sv.tl.x = map.width() - 1 - old_br.x;
        sv.br.x = map.width() - 1 - old_tl.x;

        sv.subvault->map.hmirror();
    }
}

void map_def::vmirror()
{
    if (has_tag("no_vmirror"))
        return;

    dprf(DIAG_DNGN, "Mirroring %s vertically.", name.c_str());
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

    for (subvault_place& sv : subvault_places)
    {
        coord_def old_tl = sv.tl;
        coord_def old_br = sv.br;
        sv.tl.y = map.height() - 1 - old_br.y;
        sv.br.y = map.height() - 1 - old_tl.y;

        sv.subvault->map.vmirror();
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
        dprf(DIAG_DNGN, "Rotating %s %sclockwise.",
             name.c_str(), !clock? "anti-" : "");
        map.rotate(clock);

        // Orientation shifts for clockwise rotation:
        const map_section_type clockrotate_orients[][2] =
        {
            { MAP_NORTH,        MAP_EAST        },
            { MAP_NORTHEAST,    MAP_SOUTHEAST   },
            { MAP_EAST,         MAP_SOUTH       },
            { MAP_SOUTHEAST,    MAP_SOUTHWEST   },
            { MAP_SOUTH,        MAP_WEST        },
            { MAP_SOUTHWEST,    MAP_NORTHWEST   },
            { MAP_WEST,         MAP_NORTH       },
            { MAP_NORTHWEST,    MAP_NORTHEAST   },
        };
        const int nrots = ARRAYSZ(clockrotate_orients);

        const int refindex = !clock;
        for (int i = 0; i < nrots; ++i)
            if (orient == clockrotate_orients[i][refindex])
            {
                orient = clockrotate_orients[i][!refindex];
                break;
            }

        for (subvault_place& sv : subvault_places)
        {
            coord_def p1, p2;
            if (clock) //Clockwise
            {
                p1 = coord_def(map.width() - 1 - sv.tl.y, sv.tl.x);
                p2 = coord_def(map.width() - 1 - sv.br.y, sv.br.x);
            }
            else
            {
                p1 = coord_def(sv.tl.y, map.height() - 1 - sv.tl.x);
                p2 = coord_def(sv.br.y, map.height() - 1 - sv.br.x);
            }

            sv.tl = coord_def(min(p1.x, p2.x), min(p1.y, p2.y));
            sv.br = coord_def(max(p1.x, p2.x), max(p1.y, p2.y));

            sv.subvault->map.rotate(clock);
        }
    }
}

void map_def::normalise()
{
    // Pad out lines that are shorter than max.
    map.normalise(' ');
}

string map_def::resolve()
{
    dlua_set_map dl(this);
    return "";
}

void map_def::fixup()
{
    normalise();

    // Fixup minivaults into floating vaults tagged "minivault".
    if (orient == MAP_NONE)
    {
        orient = MAP_FLOAT;
        add_tags("minivault");
    }
}

bool map_def::has_all_tags(const string &tagswanted) const
{
    const auto &tags_set = parse_tags(tagswanted);
    return has_all_tags(tags_set.begin(), tags_set.end());
}

bool map_def::has_tag(const string &tagwanted) const
{
#ifdef DEBUG_TAG_PROFILING
    _profile_inc_tag(tagwanted);
#endif
    return tags.count(tagwanted) > 0;
}

bool map_def::has_tag_prefix(const string &prefix) const
{
    if (prefix.empty())
        return false;
    for (const auto &tag : tags)
        if (starts_with(tag, prefix))
            return true;
    return false;
}

bool map_def::has_tag_suffix(const string &suffix) const
{
    if (suffix.empty())
        return false;
    for (const auto &tag : tags)
        if (ends_with(tag, suffix))
            return true;
    return false;
}

const unordered_set<string> map_def::get_tags_unsorted() const
{
    return tags;
}

const vector<string> map_def::get_tags() const
{
    // this might seem inefficient, but get_tags is not called very much; the
    // hotspot revealed by profiling is actually has_tag checks.
    vector<string> result(tags.begin(), tags.end());
    sort(result.begin(), result.end());
    return result;
}

void map_def::add_tags(const string &tag)
{
    auto parsed_tags = parse_tags(tag);
    tags.insert(parsed_tags.begin(), parsed_tags.end());
    update_cached_tags();
}

bool map_def::remove_tags(const string &tag)
{
    bool removed = false;
    auto parsed_tags = parse_tags(tag);
    for (auto &t : parsed_tags)
        removed = tags.erase(t) || removed; // would iterator overload be ok?
    update_cached_tags();
    return removed;
}

void map_def::clear_tags()
{
    tags.clear();
    update_cached_tags();
}

void map_def::set_tags(const string &tag)
{
    clear_tags();
    add_tags(tag);
    update_cached_tags();
}

string map_def::tags_string() const
{
    auto sorted_tags = get_tags();
    return join_strings(sorted_tags.begin(), sorted_tags.end());
}

keyed_mapspec *map_def::mapspec_at(const coord_def &c)
{
    return map.mapspec_at(c);
}

const keyed_mapspec *map_def::mapspec_at(const coord_def &c) const
{
    return map.mapspec_at(c);
}

string map_def::subvault_from_tagstring(const string &sub)
{
    string s = trimmed_string(sub);

    if (s.empty())
        return "";

    int sep = 0;
    string key;
    string substitute;

    string err = mapdef_split_key_item(sub, &key, &sep, &substitute, -1);
    if (!err.empty())
        return err;

    // Randomly picking a different vault per-glyph is not supported.
    if (sep != ':')
        return "SUBVAULT does not support '='. Use ':' instead.";

    map_string_list vlist;
    err = _parse_weighted_str<map_string_list>(substitute, vlist);
    if (!err.empty())
        return err;

    bool fix = false;
    string_spec spec(key, fix, vlist);

    // Although it's unfortunate to not be able to validate subvaults except a
    // run-time, this allows subvaults to reference maps by tag that may not
    // have been loaded yet.
    if (!is_validating())
        err = apply_subvault(spec);

    if (!err.empty())
        return err;

    return "";
}

static void _register_subvault(const string &name, const string &spaced_tags)
{
    auto parsed_tags = parse_tags(spaced_tags);
    if (!parsed_tags.count("allow_dup") || parsed_tags.count("luniq"))
        env.new_used_subvault_names.insert(name);

    for (const string &tag : parsed_tags)
        if (starts_with(tag, "uniq_") || starts_with(tag, "luniq_"))
            env.new_used_subvault_tags.insert(tag);
}

static void _reset_subvault_stack(const int reg_stack)
{
    env.new_subvault_names.resize(reg_stack);
    env.new_subvault_tags.resize(reg_stack);

    env.new_used_subvault_names.clear();
    env.new_used_subvault_tags.clear();
    for (int i = 0; i < reg_stack; i++)
    {
        _register_subvault(env.new_subvault_names[i],
                           env.new_subvault_tags[i]);
    }
}

string map_def::apply_subvault(string_spec &spec)
{
    // Find bounding box for key glyphs
    coord_def tl, br;
    if (!map.find_bounds(spec.key.c_str(), tl, br))
    {
        // No glyphs, so do nothing.
        return "";
    }

    int vwidth = br.x - tl.x + 1;
    int vheight = br.y - tl.y + 1;
    Matrix<bool> flags(vwidth, vheight);
    map.fill_mask_matrix(spec.key, tl, br, flags);

    // Remember the subvault registration pointer, so we can clear it.
    const int reg_stack = env.new_subvault_names.size();
    ASSERT(reg_stack == (int)env.new_subvault_tags.size());
    ASSERT(reg_stack >= (int)env.new_used_subvault_names.size());

    const int max_tries = 100;
    int ntries = 0;

    string tag = spec.get_property();
    while (++ntries <= max_tries)
    {
        // Each iteration, restore tags and names. This is because this vault
        // may successfully load a subvault (registering its tag and name), but
        // then itself fail.
        _reset_subvault_stack(reg_stack);

        const map_def *orig = random_map_for_tag(tag, true);
        if (!orig)
            return make_stringf("No vault found for tag '%s'", tag.c_str());

        map_def vault = *orig;

        vault.load();

        // Temporarily set the subvault mask so this subvault can know
        // that it is being generated as a subvault.
        vault.svmask = &flags;

        if (!resolve_subvault(vault))
        {
            if (crawl_state.last_builder_error_fatal)
                break;
            else
                continue;
        }

        ASSERT(vault.map.width() <= vwidth);
        ASSERT(vault.map.height() <= vheight);

        const map_corner_t subvault_corners =
            map.merge_subvault(tl, br, flags, vault);

        copy_hooks_from(vault, "post_place");
        env.new_subvault_names.push_back(vault.name);
        const string vault_tags = vault.tags_string();
        env.new_subvault_tags.push_back(vault_tags);
        _register_subvault(vault.name, vault_tags);
        subvault_places.emplace_back(subvault_corners.first,
                                     subvault_corners.second, vault);

        return "";
    }

    // Failure, drop subvault registrations.
    _reset_subvault_stack(reg_stack);

    if (crawl_state.last_builder_error_fatal)
    {
        // I think the error should get printed elsewhere?
        return make_stringf("Fatal lua error while resolving subvault '%s'",
            tag.c_str());
    }
    return make_stringf("Could not fit '%s' in (%d,%d) to (%d, %d).",
                        tag.c_str(), tl.x, tl.y, br.x, br.y);
}

bool map_def::is_subvault() const
{
    return svmask != nullptr;
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

    return svmask->width();
}

int map_def::subvault_height() const
{
    if (!svmask)
        return 0;

    return svmask->height();
}

int map_def::subvault_mismatch_count(const coord_def &offset) const
{
    int count = 0;
    if (!is_subvault())
        return count;

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

    return count;
}

///////////////////////////////////////////////////////////////////
// mons_list
//

mons_list::mons_list() : mons()
{
}

mons_spec mons_list::pick_monster(mons_spec_slot &slot)
{
    int totweight = 0;
    mons_spec pick;

    for (const auto &spec : slot.mlist)
    {
        const int weight = spec.genweight;
        if (x_chance_in_y(weight, totweight += weight))
            pick = spec;
    }

#if TAG_MAJOR_VERSION == 34
    // Force rebuild of the des cache to drop this check.
    if ((int)pick.type < -1)
        pick = (monster_type)(-100 - (int)pick.type);
#endif

    if (slot.fix_slot)
    {
        slot.mlist.clear();
        slot.mlist.push_back(pick);
        slot.fix_slot = false;
    }

    return pick;
}

mons_spec mons_list::get_monster(int index)
{
    if (index < 0 || index >= (int)mons.size())
        return mons_spec(RANDOM_MONSTER);

    return pick_monster(mons[index]);
}

mons_spec mons_list::get_monster(int slot_index, int list_index) const
{
    if (slot_index < 0 || slot_index >= (int)mons.size())
        return mons_spec(RANDOM_MONSTER);

    const mons_spec_list &list = mons[slot_index].mlist;

    if (list_index < 0 || list_index >= (int)list.size())
        return mons_spec(RANDOM_MONSTER);

    return list[list_index];
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

void mons_list::parse_mons_spells(mons_spec &spec, vector<string> &spells)
{
    spec.explicit_spells = true;

    for (const string &slotspec : spells)
    {
        monster_spells cur_spells;

        const vector<string> spell_names(split_string(";", slotspec));

        for (unsigned i = 0, ssize = spell_names.size(); i < ssize; ++i)
        {
            cur_spells.emplace_back();
            const string spname(
                lowercase_string(replace_all_of(spell_names[i], "_", " ")));
            if (spname.empty() || spname == "." || spname == "none"
                || spname == "no spell")
            {
                cur_spells[i].spell = SPELL_NO_SPELL;
            }
            else
            {
                const vector<string> slot_vals = split_string(".", spname);
                const spell_type sp(spell_by_name(slot_vals[0]));
                if (sp == SPELL_NO_SPELL)
                {
                    error = make_stringf("Unknown spell name: '%s' in '%s'",
                                         slot_vals[0].c_str(),
                                         slotspec.c_str());
                    return;
                }
                if (!is_valid_mon_spell(sp))
                {
                    error = make_stringf("Not a monster spell: '%s'",
                                         slot_vals[0].c_str());
                    return;
                }
                cur_spells[i].spell = sp;
                int freq = 30;
                if (slot_vals.size() >= 2)
                {
                    freq = atoi(slot_vals[1].c_str());
                    if (freq <= 0)
                    {
                        error = make_stringf("Need a positive spell frequency;"
                                             "got '%s' in '%s'",
                                             slot_vals[1].c_str(),
                                             spname.c_str());
                        return;
                    }
                }
                cur_spells[i].freq = freq;
                for (size_t j = 2; j < slot_vals.size(); j++)
                {
                    if (slot_vals[j] == "emergency")
                        cur_spells[i].flags |= MON_SPELL_EMERGENCY;
                    if (slot_vals[j] == "natural")
                        cur_spells[i].flags |= MON_SPELL_NATURAL;
                    if (slot_vals[j] == "magical")
                        cur_spells[i].flags |= MON_SPELL_MAGICAL;
                    if (slot_vals[j] == "wizard")
                        cur_spells[i].flags |= MON_SPELL_WIZARD;
                    if (slot_vals[j] == "priest")
                        cur_spells[i].flags |= MON_SPELL_PRIEST;
                    if (slot_vals[j] == "vocal")
                        cur_spells[i].flags |= MON_SPELL_VOCAL;
                    if (slot_vals[j] == "breath")
                        cur_spells[i].flags |= MON_SPELL_BREATH;
                    if (slot_vals[j] == "instant")
                        cur_spells[i].flags |= MON_SPELL_INSTANT;
                    if (slot_vals[j] == "noisy")
                        cur_spells[i].flags |= MON_SPELL_NOISY;
                    if (slot_vals[j] == "short range")
                        cur_spells[i].flags |= MON_SPELL_SHORT_RANGE;
                    if (slot_vals[j] == "long range")
                        cur_spells[i].flags |= MON_SPELL_LONG_RANGE;
                }
                if (!(cur_spells[i].flags & MON_SPELL_TYPE_MASK))
                    cur_spells[i].flags |= MON_SPELL_MAGICAL;
            }
        }

        spec.spells.push_back(cur_spells);
    }
}

mon_enchant mons_list::parse_ench(string &ench_str, bool perm)
{
    vector<string> ep = split_string(":", ench_str);
    if (ep.size() > (perm ? 2 : 3))
    {
        error = make_stringf("bad %sench specifier: \"%s\"",
                             perm ? "perm_" : "",
                             ench_str.c_str());
        return mon_enchant();
    }

    enchant_type et = name_to_ench(ep[0].c_str());
    if (et == ENCH_NONE)
    {
        error = make_stringf("unknown ench: \"%s\"", ep[0].c_str());
        return mon_enchant();
    }

    int deg = 0, dur = perm ? INFINITE_DURATION : 0;
    if (ep.size() > 1 && !ep[1].empty())
        if (!parse_int(ep[1].c_str(), deg))
        {
            error = make_stringf("invalid deg in ench specifier \"%s\"",
                                 ench_str.c_str());
            return mon_enchant();
        }
    if (ep.size() > 2 && !ep[2].empty())
        if (!parse_int(ep[2].c_str(), dur))
        {
            error = make_stringf("invalid dur in ench specifier \"%s\"",
                                 ench_str.c_str());
            return mon_enchant();
        }
    return mon_enchant(et, deg, 0, dur);
}

mons_list::mons_spec_slot mons_list::parse_mons_spec(string spec)
{
    mons_spec_slot slot;

    vector<string> specs = split_string("/", spec);

    for (const string &monspec : specs)
    {
        string s(monspec);
        mons_spec mspec;

        vector<string> spells(strip_multiple_tag_prefix(s, "spells:"));
        if (!spells.empty())
        {
            parse_mons_spells(mspec, spells);
            if (!error.empty())
                return slot;
        }

        vector<string> parts = split_string(";", s);

        if (parts.size() == 0)
        {
            error = make_stringf("Not enough non-semicolons for '%s' spec.",
                                 s.c_str());
            return slot;
        }

        string mon_str = parts[0];

        if (parts.size() > 2)
        {
            error = make_stringf("Too many semi-colons for '%s' spec.",
                                 mon_str.c_str());
            return slot;
        }
        else if (parts.size() == 2)
        {
            // TODO: Allow for a "fix_slot" type tag which will cause
            // all monsters generated from this spec to have the
            // exact same equipment.
            string items_str = parts[1];
            items_str = replace_all(items_str, "|", "/");

            vector<string> segs = split_string(".", items_str);

            if (segs.size() > NUM_MONSTER_SLOTS)
            {
                error = make_stringf("More items than monster item slots "
                                     "for '%s'.", mon_str.c_str());
                return slot;
            }

            for (const string &seg : segs)
            {
                error = mspec.items.add_item(seg, false);
                if (!error.empty())
                    return slot;
            }
        }

        mspec.genweight = find_weight(mon_str);
        if (mspec.genweight == TAG_UNFOUND)
            mspec.genweight = 10;
        else if (mspec.genweight <= 0)
            mspec.genweight = 0;

        mspec.generate_awake = strip_tag(mon_str, "generate_awake");
        mspec.patrolling     = strip_tag(mon_str, "patrolling");
        mspec.band           = strip_tag(mon_str, "band");

        const string att = strip_tag_prefix(mon_str, "att:");
        if (att.empty() || att == "hostile")
            mspec.attitude = ATT_HOSTILE;
        else if (att == "friendly")
            mspec.attitude = ATT_FRIENDLY;
        else if (att == "good_neutral" || att == "fellow_slime")
            mspec.attitude = ATT_GOOD_NEUTRAL;
        else if (att == "neutral")
            mspec.attitude = ATT_NEUTRAL;

        // Useful for summoned monsters.
        if (strip_tag(mon_str, "seen"))
            mspec.extra_monster_flags |= MF_SEEN;

        if (strip_tag(mon_str, ALWAYS_CORPSE_KEY))
            mspec.props[ALWAYS_CORPSE_KEY] = true;

        if (strip_tag(mon_str, NEVER_CORPSE_KEY))
            mspec.props[NEVER_CORPSE_KEY] = true;

        if (!mon_str.empty() && isadigit(mon_str[0]))
        {
            // Look for space after initial digits.
            string::size_type pos = mon_str.find_first_not_of("0123456789");
            if (pos != string::npos && mon_str[pos] == ' ')
            {
                const string mcount = mon_str.substr(0, pos);
                const int count = atoi(mcount.c_str()); // safe atoi()
                if (count >= 1 && count <= 99)
                    mspec.quantity = count;

                mon_str = mon_str.substr(pos);
            }
        }

        // place:Elf:$ to choose monsters appropriate for that level,
        // for example.
        const string place = strip_tag_prefix(mon_str, "place:");
        if (!place.empty())
        {
            try
            {
                mspec.place = level_id::parse_level_id(place);
            }
            catch (const bad_level_id &err)
            {
                error = err.what();
                return slot;
            }
        }

        mspec.hd = min(100, strip_number_tag(mon_str, "hd:"));
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

        // Orc apostle power and band power
        int pow = strip_number_tag(mon_str, "pow:");
        if (pow != TAG_UNFOUND)
            mspec.props[APOSTLE_POWER_KEY] = pow;
        int band_pow = strip_number_tag(mon_str, "bandpow:");
        if (band_pow != TAG_UNFOUND)
        {
            if (pow != TAG_UNFOUND)
                band_pow = pow;
            mspec.props[APOSTLE_BAND_POWER_KEY] = band_pow;
        }

        string shifter_name = replace_all_of(strip_tag_prefix(mon_str, "shifter:"), "_", " ");

        if (!shifter_name.empty())
        {
            mspec.initial_shifter = get_monster_by_name(shifter_name);
            if (mspec.initial_shifter == MONS_PROGRAM_BUG)
                mspec.initial_shifter = RANDOM_MONSTER;
        }

        int summon_type = 0;
        string s_type = strip_tag_prefix(mon_str, "sum:");
        if (!s_type.empty())
        {
            // In case of spells!
            s_type = replace_all_of(s_type, "_", " ");
            summon_type = static_cast<int>(str_to_summon_type(s_type));
            if (summon_type == SPELL_NO_SPELL)
            {
                error = make_stringf("bad monster summon type: \"%s\"",
                                s_type.c_str());
                return slot;
            }
            if (mspec.abjuration_duration == 0)
            {
                error = "marked summon with no duration";
                return slot;
            }
        }

        mspec.summon_type = summon_type;

        string non_actor_summoner = strip_tag_prefix(mon_str, "nas:");
        if (!non_actor_summoner.empty())
        {
            non_actor_summoner = replace_all_of(non_actor_summoner, "_", " ");
            mspec.non_actor_summoner = non_actor_summoner;
            if (mspec.abjuration_duration == 0)
            {
                error = "marked summon with no duration";
                return slot;
            }
        }

        string colour = strip_tag_prefix(mon_str, "col:");
        if (!colour.empty())
        {
            if (colour == "any")
                mspec.colour = COLOUR_UNDEF;
            else
            {
                mspec.colour = str_to_colour(colour, COLOUR_UNDEF, false, true);
                if (mspec.colour == COLOUR_UNDEF)
                {
                    error = make_stringf("bad monster colour \"%s\" in \"%s\"",
                                         colour.c_str(), monspec.c_str());
                    return slot;
                }
            }
        }

        string mongod = strip_tag_prefix(mon_str, "god:");
        if (!mongod.empty())
        {
            const string god_name(replace_all_of(mongod, "_", " "));

            mspec.god = str_to_god(god_name);

            if (mspec.god == GOD_NO_GOD)
            {
                error = make_stringf("bad monster god: \"%s\"",
                                     god_name.c_str());
                return slot;
            }

            if (strip_tag(mon_str, "god_gift"))
                mspec.god_gift = true;
        }

        string tile = strip_tag_prefix(mon_str, "tile:");
        if (!tile.empty())
        {
            tileidx_t index;
            if (!tile_player_index(tile.c_str(), &index))
            {
                error = make_stringf("bad tile name: \"%s\".", tile.c_str());
                return slot;
            }
            // Store name along with the tile.
            mspec.props[MONSTER_TILE_NAME_KEY].get_string() = tile;
            mspec.props[MONSTER_TILE_KEY] = int(index);
        }

        string dbname = strip_tag_prefix(mon_str, "dbname:");
        if (!dbname.empty())
        {
            dbname = replace_all_of(dbname, "_", " ");
            mspec.props[DBNAME_KEY].get_string() = dbname;
        }

        string name = strip_tag_prefix(mon_str, "name:");
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

            if (strip_tag(mon_str, "name_definite")
                || strip_tag(mon_str, "n_the"))
            {
                mspec.extra_monster_flags |= MF_NAME_DEFINITE;
            }

            // Reasoning for setting more than one flag: suffixes and
            // adjectives need NAME_DESCRIPTOR to get proper grammar,
            // and definite names do nothing with the description unless
            // NAME_DESCRIPTOR is also set.
            const auto name_flags = mspec.extra_monster_flags & MF_NAME_MASK;
            const bool need_name_desc =
                name_flags == MF_NAME_SUFFIX
                   || name_flags == MF_NAME_ADJECTIVE
                   || (mspec.extra_monster_flags & MF_NAME_DEFINITE);

            if (strip_tag(mon_str, "name_descriptor")
                || strip_tag(mon_str, "n_des")
                || need_name_desc)
            {
                mspec.extra_monster_flags |= MF_NAME_DESCRIPTOR;
            }

            if (strip_tag(mon_str, "name_species")
                || strip_tag(mon_str, "n_spe"))
            {
                mspec.extra_monster_flags |= MF_NAME_SPECIES;
            }

            if (strip_tag(mon_str, "name_zombie")
                || strip_tag(mon_str, "n_zom"))
            {
                mspec.extra_monster_flags |= MF_NAME_ZOMBIE;
            }
            if (strip_tag(mon_str, "name_nocorpse")
                || strip_tag(mon_str, "n_noc"))
            {
                mspec.extra_monster_flags |= MF_NAME_NOCORPSE;
            }
        }

        string ench_str;
        while (!(ench_str = strip_tag_prefix(mon_str, "ench:")).empty())
        {
            mspec.ench.push_back(parse_ench(ench_str, false));
            if (!error.empty())
                return slot;
        }
        while (!(ench_str = strip_tag_prefix(mon_str, "perm_ench:")).empty())
        {
            mspec.ench.push_back(parse_ench(ench_str, true));
            if (!error.empty())
                return slot;
        }

        trim_string(mon_str);

        if (mon_str == "8")
            mspec.type = RANDOM_SUPER_OOD;
        else if (mon_str == "9")
            mspec.type = RANDOM_MODERATE_OOD;
        else if (mspec.place.is_valid())
        {
            // For monster specs such as place:Orc:4 zombie, we may
            // have a monster modifier, in which case we set the
            // modifier in monbase.
            const mons_spec nspec = mons_by_name("orc " + mon_str);
            if (nspec.type != MONS_PROGRAM_BUG)
            {
                // Is this a modified monster?
                if (nspec.monbase != MONS_PROGRAM_BUG
                    && mons_class_is_zombified(static_cast<monster_type>(nspec.type)))
                {
                    mspec.monbase = static_cast<monster_type>(nspec.type);
                }
            }
        }
        else if (mon_str != "0")
        {
            const mons_spec nspec = mons_by_name(mon_str);

            if (nspec.type == MONS_PROGRAM_BUG)
            {
                error = make_stringf("unknown monster: \"%s\"",
                                     mon_str.c_str());
                return slot;
            }

            if (mons_class_flag(nspec.type, M_CANT_SPAWN))
            {
                error = make_stringf("can't place dummy monster: \"%s\"",
                                     mon_str.c_str());
                return slot;
            }

            mspec.type    = nspec.type;
            mspec.monbase = nspec.monbase;
            if (nspec.colour > COLOUR_UNDEF && mspec.colour <= COLOUR_UNDEF)
                mspec.colour = nspec.colour;
            if (nspec.hd != 0)
                mspec.hd = nspec.hd;
#define MAYBE_COPY(x) \
            if (nspec.props.exists((x))) \
            { \
                mspec.props[(x)] \
                    = nspec.props[(x)]; \
            }
            MAYBE_COPY(MUTANT_BEAST_FACETS);
            MAYBE_COPY(MGEN_BLOB_SIZE);
            MAYBE_COPY(MGEN_NUM_HEADS);
            MAYBE_COPY(APOSTLE_TYPE_KEY);
#undef MAYBE_COPY
        }

        if (!mspec.items.empty())
        {
            monster_type type = (monster_type)mspec.type;
            if (type == RANDOM_DRACONIAN
                || type == RANDOM_BASE_DRACONIAN
                || type == RANDOM_NONBASE_DRACONIAN)
            {
                type = MONS_DRACONIAN;
            }

            if (type >= NUM_MONSTERS)
            {
                error = "Can't give spec items to a random monster.";
                return slot;
            }
            else if (mons_class_itemuse(type) < MONUSE_STARTING_EQUIPMENT
                     && (!mons_class_is_animated_object(type)
                         || mspec.items.size() > 1)
                     && (type != MONS_ZOMBIE && type != MONS_SKELETON
                         || invalid_monster_type(mspec.monbase)
                         || mons_class_itemuse(mspec.monbase)
                            < MONUSE_STARTING_EQUIPMENT))
            {
                // TODO: skip this error if the monspec is `nothing`
                error = make_stringf("Monster '%s' can't use items.",
                    mon_str.c_str());
            }
            else if (mons_class_is_animated_object(type))
            {
                auto item = mspec.items.get_item(0);
                const auto *unrand = item.ego < SP_FORBID_EGO
                    ? get_unrand_entry(-item.ego) : nullptr;
                const auto base = unrand && unrand->base_type != OBJ_UNASSIGNED
                    ? unrand->base_type : item.base_type;
                const auto sub = unrand && unrand->base_type != OBJ_UNASSIGNED
                    ? unrand->sub_type : item.sub_type;

                const auto def_slot = mons_class_is_animated_weapon(type)
                    ? EQ_WEAPON
                    : EQ_BODY_ARMOUR;

                if (get_item_slot(base, sub) != def_slot)
                {
                    error = make_stringf("Monster '%s' needs a defining item.",
                                         mon_str.c_str());
                }
            }
        }

        slot.mlist.push_back(mspec);
    }

    return slot;
}

string mons_list::add_mons(const string &s, bool fix)
{
    error.clear();

    mons_spec_slot slotmons = parse_mons_spec(s);
    if (!error.empty())
        return error;

    if (fix)
    {
        slotmons.fix_slot = true;
        pick_monster(slotmons);
    }

    mons.push_back(slotmons);

    return error;
}

string mons_list::set_mons(int index, const string &s)
{
    error.clear();

    if (index < 0)
        return error = make_stringf("Index out of range: %d", index);

    mons_spec_slot slotmons = parse_mons_spec(s);
    if (!error.empty())
        return error;

    if (index >= (int) mons.size())
    {
        mons.reserve(index + 1);
        mons.resize(index + 1, mons_spec_slot());
    }
    mons[index] = slotmons;
    return error;
}

static monster_type _fixup_mon_type(monster_type orig)
{
    if (mons_class_flag(orig, M_CANT_SPAWN))
        return MONS_PROGRAM_BUG;

    if (orig < 0)
        orig = MONS_PROGRAM_BUG;

    monster_type dummy_mons = MONS_PROGRAM_BUG;
    coord_def dummy_pos;
    level_id place = level_id::current();
    return resolve_monster_type(orig, dummy_mons, PROX_ANYWHERE, &dummy_pos, 0,
                                &place);
}

void mons_list::get_zombie_type(string s, mons_spec &spec) const
{
    static const char *zombie_types[] =
    {
        " zombie", " skeleton", " simulacrum", " spectre", nullptr
    };

    // This order must match zombie_types, indexed from one.
    static const monster_type zombie_montypes[] =
    {
        MONS_PROGRAM_BUG, MONS_ZOMBIE, MONS_SKELETON, MONS_SIMULACRUM,
        MONS_SPECTRAL_THING,
    };

    int mod = ends_with(s, zombie_types);
    if (!mod)
    {
        if (starts_with(s, "spectral "))
        {
            mod = ends_with(" spectre", zombie_types);
            s = s.substr(9); // strlen("spectral ")
        }
        else
        {
            spec.type = MONS_PROGRAM_BUG;
            return;
        }
    }
    else
        s = s.substr(0, s.length() - strlen(zombie_types[mod - 1]));

    trim_string(s);

    mons_spec base_monster = mons_by_name(s);
    base_monster.type = _fixup_mon_type(base_monster.type);
    if (base_monster.type == MONS_PROGRAM_BUG)
    {
        spec.type = MONS_PROGRAM_BUG;
        return;
    }

    spec.monbase = static_cast<monster_type>(base_monster.type);
    if (base_monster.props.exists(MGEN_NUM_HEADS))
        spec.props[MGEN_NUM_HEADS] = base_monster.props[MGEN_NUM_HEADS];

    spec.type = zombie_montypes[mod];
    switch (spec.type)
    {
    case MONS_SIMULACRUM:
    case MONS_SPECTRAL_THING:
        if (mons_class_can_be_spectralised(spec.monbase))
            return;
        break;
    case MONS_SKELETON:
        if (!mons_skeleton(spec.monbase))
            break;
        // fallthrough to MONS_ZOMBIE
    case MONS_ZOMBIE:
    default:
        if (mons_class_can_be_zombified(spec.monbase))
            return;
        break;
    }

    spec.type = MONS_PROGRAM_BUG;
}

mons_spec mons_list::get_hydra_spec(const string &name) const
{
    string prefix = name.substr(0, name.find("-"));

    int nheads = atoi(prefix.c_str());
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

    mons_spec spec(MONS_HYDRA);
    spec.props[MGEN_NUM_HEADS] = nheads;
    return spec;
}

mons_spec mons_list::get_slime_spec(const string &name) const
{
    string prefix = name.substr(0, name.find(" slime creature"));

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

    mons_spec spec(MONS_SLIME_CREATURE);
    spec.props[MGEN_BLOB_SIZE] = slime_size;
    return spec;
}

/**
 * Build a monster specification for a specified pillar of salt. The pillar of
 * salt won't crumble over time, since that seems useful for any version of
 * this function.
 *
 * @param name      The description of the pillar of salt; e.g.
 *                  "human-shaped pillar of salt",
 *                  "titanic slime creature-shaped pillar of salt."
 *                  XXX: doesn't currently work with zombie specifiers
 *                  e.g. "zombie-shaped..." (does this matter?)
 * @return          A specifier for a pillar of salt.
 */
mons_spec mons_list::get_salt_spec(const string &name) const
{
    const string prefix = name.substr(0, name.find("-shaped pillar of salt"));
    mons_spec base_mon = mons_by_name(prefix);
    if (base_mon.type == MONS_PROGRAM_BUG)
        return base_mon; // invalid specifier

    mons_spec spec(MONS_PILLAR_OF_SALT);
    spec.monbase = _fixup_mon_type(base_mon.type);
    return spec;
}

// Randomly-generated draconians have fixed colour/job combos - see
// _draconian_combos in mon-util.cc. Vaults can override this, but shouldn't do
// so without good reason (for example an elemental-flavoured vault might use
// classed draconians all of a single colour).
//
// Handle draconians specified as:
// Exactly as in mon-data.h:
//    yellow draconian or draconian knight - the monster specified.
//
// Others:
//    any draconian => any random draconian
//    any base draconian => any unspecialised coloured draconian.
//    any nonbase draconian => any specialised coloured draconian.
//    any <colour> draconian => any draconian of the colour.
//    any nonbase <colour> draconian => any specialised drac of the colour,
//                                      ignoring the standard colour/job
//                                      restrictions.
//
mons_spec mons_list::drac_monspec(string name) const
{
    mons_spec spec;

    spec.type = get_monster_by_name(name);

    // Check if it's a simple drac name, we're done.
    if (spec.type != MONS_PROGRAM_BUG)
        return spec;

    spec.type = RANDOM_DRACONIAN;

    // Request for any draconian?
    if (starts_with(name, "any "))
        name = name.substr(4); // Strip "any "

    if (starts_with(name, "base "))
    {
        // Base dracs need no further work.
        return RANDOM_BASE_DRACONIAN;
    }
    else if (starts_with(name, "nonbase "))
    {
        spec.type = RANDOM_NONBASE_DRACONIAN;
        name = name.substr(8);
    }

    trim_string(name);

    // Match "any draconian"
    if (name == "draconian")
        return spec;

    // Check for recognition again to match any (nonbase) <colour> draconian.
    const monster_type colour = get_monster_by_name(name);
    if (colour != MONS_PROGRAM_BUG)
    {
        spec.monbase = colour;
        return spec;
    }

    // Only legal possibility left is <colour> boss drac.
    string::size_type wordend = name.find(' ');
    if (wordend == string::npos)
        return MONS_PROGRAM_BUG;

    string scolour = name.substr(0, wordend);
    if ((spec.monbase = draconian_colour_by_name(scolour)) == MONS_PROGRAM_BUG)
        return MONS_PROGRAM_BUG;

    name = trimmed_string(name.substr(wordend + 1));
    spec.type = get_monster_by_name(name);

    // We should have a non-base draconian here.
    if (spec.type == MONS_PROGRAM_BUG
        || mons_genus(static_cast<monster_type>(spec.type)) != MONS_DRACONIAN
        || mons_is_base_draconian(spec.type))
    {
        return MONS_PROGRAM_BUG;
    }

    return spec;
}

mons_spec mons_list::soh_monspec(string name) const
{
    // "serpent of hell " is 16 characters
    name = name.substr(16);
    string abbrev =
        uppercase_first(lowercase(name)).substr(0, 3);
    switch (branch_by_abbrevname(abbrev))
    {
        case BRANCH_GEHENNA:
            return MONS_SERPENT_OF_HELL;
        case BRANCH_COCYTUS:
            return MONS_SERPENT_OF_HELL_COCYTUS;
        case BRANCH_DIS:
            return MONS_SERPENT_OF_HELL_DIS;
        case BRANCH_TARTARUS:
            return MONS_SERPENT_OF_HELL_TARTARUS;
        default:
            return MONS_PROGRAM_BUG;
    }
}

/**
 * What mutant beast facet corresponds to the given name?
 *
 * @param name      The name in question (e.g. 'bat')
 * @return          The corresponding facet (e.g. BF_BAT), or BF_NONE.
 */
static int _beast_facet_by_name(const string &name)
{
    for (int bf = BF_FIRST; bf < NUM_BEAST_FACETS; ++bf)
        if (mutant_beast_facet_names[bf] == lowercase_string(name))
            return bf;
    return BF_NONE;
}

/**
 * What HD corresponds to the given mutant beast tier name?
 *
 * XXX: refactor this together with _beast_facet_by_name()?
 *
 * @param tier      The name in question (e.g. 'juvenile')
 * @return          The corresponding tier XL (e.g. 9), or 0 if none is valid.
 */
static int _mutant_beast_xl(const string &tier)
{
    for (int bt = BT_FIRST; bt < NUM_BEAST_TIERS; ++bt)
        if (mutant_beast_tier_names[bt] == lowercase_string(tier))
            return beast_tiers[bt];
    return 0;
}

mons_spec mons_list::mons_by_name(string name) const
{
    name = replace_all_of(name, "_", " ");
    name = replace_all(name, "random", "any");

    if (name == "nothing")
        return MONS_NO_MONSTER;

    // Special casery:
    if (name == "pandemonium lord")
        return MONS_PANDEMONIUM_LORD;

    if (name == "any" || name == "any monster")
        return RANDOM_MONSTER;

    if (name == "any demon")
        return RANDOM_DEMON;

    if (name == "any lesser demon" || name == "lesser demon")
        return RANDOM_DEMON_LESSER;

    if (name == "any common demon" || name == "common demon")
        return RANDOM_DEMON_COMMON;

    if (name == "any greater demon" || name == "greater demon")
        return RANDOM_DEMON_GREATER;

    if (name == "small zombie" || name == "large zombie"
        || name == "small skeleton" || name == "large skeleton"
        || name == "small simulacrum" || name == "large simulacrum")
    {
        return MONS_PROGRAM_BUG;
    }

    if (name == "small abomination")
        return MONS_ABOMINATION_SMALL;
    if (name == "large abomination")
        return MONS_ABOMINATION_LARGE;

    if (ends_with(name, "-headed hydra") && !starts_with(name, "spectral "))
        return get_hydra_spec(name);

    if (ends_with(name, " slime creature"))
        return get_slime_spec(name);

    if (ends_with(name, "-shaped pillar of salt"))
        return get_salt_spec(name);

    if (ends_with(name, " apostle"))
    {
        mons_spec spec = MONS_ORC_APOSTLE;
        const auto m_index = name.find(" apostle");
        const string trimmed = name.substr(0, m_index);
        vector<string> segments = split_string(" ", trimmed);

        // No subtype specified
        if (segments.size() < 2)
            return spec;

        if (segments.size() > 2 || lowercase(segments[0]) != "orc")
            return MONS_PROGRAM_BUG; // non-matching formating

        spec.props[APOSTLE_TYPE_KEY] = apostle_type_by_name(segments[1]);

        return spec;
    }

    const auto m_index = name.find(" mutant beast");
    if (m_index != string::npos)
    {
        mons_spec spec = MONS_MUTANT_BEAST;

        const string trimmed = name.substr(0, m_index);
        const vector<string> segments = split_string(" ", trimmed);
        if (segments.size() > 2)
            return MONS_PROGRAM_BUG; // too many words

        const bool fully_specified = segments.size() == 2;
        spec.hd = _mutant_beast_xl(segments[0]);
        if (spec.hd == 0 && fully_specified)
            return MONS_PROGRAM_BUG; // gave invalid tier spec

        if (spec.hd == 0 || fully_specified)
        {
            const int seg = segments.size() - 1;
            const vector<string> facet_names
                = split_string("-", segments[seg]);
            for (const string &facet_name : facet_names)
            {
                const int facet = _beast_facet_by_name(facet_name);
                if (facet == BF_NONE)
                    return MONS_PROGRAM_BUG; // invalid facet
                spec.props[MUTANT_BEAST_FACETS].get_vector().push_back(facet);
            }
        }

        return spec;
    }

    mons_spec spec;
    if (name.find(" ugly thing") != string::npos)
    {
        const string::size_type wordend = name.find(' ');
        const string first_word = name.substr(0, wordend);

        const int colour = str_to_ugly_thing_colour(first_word);
        if (colour)
        {
            spec = mons_by_name(name.substr(wordend + 1));
            spec.colour = colour;
            return spec;
        }
    }

    get_zombie_type(name, spec);
    if (spec.type != MONS_PROGRAM_BUG)
        return spec;

    if (name.find("draconian") != string::npos)
        return drac_monspec(name);

    // The space is important - it indicates a flavour is being specified.
    if (name.find("serpent of hell ") != string::npos)
        return soh_monspec(name);

    // Allow access to her second form, which shares display names.
    if (name == "bai suzhen dragon")
        return MONS_BAI_SUZHEN_DRAGON;

    return get_monster_by_name(name);
}

//////////////////////////////////////////////////////////////////////
// item_list

item_spec::item_spec(const item_spec &other)
    : _corpse_monster_spec(nullptr)
{
    *this = other;
}

item_spec &item_spec::operator = (const item_spec &other)
{
    if (this != &other)
    {
        genweight = other.genweight;
        base_type = other.base_type;
        sub_type  = other.sub_type;
        plus = other.plus;
        plus2 = other.plus2;
        ego = other.ego;
        allow_uniques = other.allow_uniques;
        level = other.level;
        item_special = other.item_special;
        qty = other.qty;
        acquirement_source = other.acquirement_source;
        place = other.place;
        props = other.props;

        release_corpse_monster_spec();
        if (other._corpse_monster_spec)
            set_corpse_monster_spec(other.corpse_monster_spec());
    }
    return *this;
}

item_spec::~item_spec()
{
    release_corpse_monster_spec();
}

void item_spec::release_corpse_monster_spec()
{
    delete _corpse_monster_spec;
    _corpse_monster_spec = nullptr;
}

bool item_spec::corpselike() const
{
    return base_type == OBJ_CORPSES;
}

const mons_spec &item_spec::corpse_monster_spec() const
{
    ASSERT(_corpse_monster_spec);
    return *_corpse_monster_spec;
}

void item_spec::set_corpse_monster_spec(const mons_spec &spec)
{
    if (&spec != _corpse_monster_spec)
    {
        release_corpse_monster_spec();
        _corpse_monster_spec = new mons_spec(spec);
    }
}

void item_list::clear()
{
    items.clear();
}

item_spec item_list::random_item()
{
    if (items.empty())
    {
        const item_spec none;
        return none;
    }

    return get_item(random2(size()));
}

typedef pair<item_spec, int> item_pair;

item_spec item_list::random_item_weighted()
{
    const item_spec none;

    vector<item_pair> pairs;
    for (int i = 0, sz = size(); i < sz; ++i)
    {
        item_spec item = get_item(i);
        pairs.emplace_back(item, item.genweight);
    }

    item_spec* rn_item = random_choose_weighted(pairs);
    if (rn_item)
        return *rn_item;

    return none;
}

item_spec item_list::pick_item(item_spec_slot &slot)
{
    int cumulative = 0;
    item_spec pick;
    for (const auto &spec : slot.ilist)
    {
        const int weight = spec.genweight;
        if (x_chance_in_y(weight, cumulative += weight))
            pick = spec;
    }

    if (slot.fix_slot)
    {
        slot.ilist.clear();
        slot.ilist.push_back(pick);
        slot.fix_slot = false;
    }

    return pick;
}

item_spec item_list::get_item(int index)
{
    if (index < 0 || index >= (int) items.size())
    {
        const item_spec none;
        return none;
    }

    return pick_item(items[index]);
}

string item_list::add_item(const string &spec, bool fix)
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

        // If the only item here was an excluded item, we'll get an empty list.
        if (!sp.ilist.empty())
            items.push_back(sp);
    }

    return error;
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

// TODO: More checking for inappropriate combinations, like the holy
// wrath brand on a demonic weapon or the running ego on a helmet.
// NOTE: Be sure to update the reference in syntax.txt if this gets moved!
int str_to_ego(object_class_type item_type, string ego_str)
{
    const char* armour_egos[] =
    {
#if TAG_MAJOR_VERSION == 34
        "running",
#endif
        "fire_resistance",
        "cold_resistance",
        "poison_resistance",
        "see_invisible",
        "invisibility",
        "strength",
        "dexterity",
        "intelligence",
        "ponderousness",
        "flying",
        "willpower",
        "protection",
        "stealth",
        "resistance",
        "positive_energy",
        "archmagi",
        "preservation",
        "reflection",
        "spirit_shield",
        "hurling",
#if TAG_MAJOR_VERSION == 34
        "jumping",
#endif
        "repulsion",
#if TAG_MAJOR_VERSION == 34
        "cloud_immunity",
#endif
        "harm",
        "shadows",
        "rampaging",
        "infusion",
        "light",
        "wrath",
        "mayhem",
        "guile",
        "energy",
        nullptr
    };
    COMPILE_CHECK(ARRAYSZ(armour_egos) == NUM_REAL_SPECIAL_ARMOURS);

    const char* weapon_brands[] =
    {
        "flaming",
        "freezing",
        "holy_wrath",
        "electrocution",
#if TAG_MAJOR_VERSION == 34
        "orc_slaying",
        "dragon_slaying",
#endif
        "venom",
        "protection",
        "draining",
        "speed",
        "heavy",
#if TAG_MAJOR_VERSION == 34
        "flame",
        "frost",
#endif
        "vampirism",
        "pain",
        "antimagic",
        "distortion",
#if TAG_MAJOR_VERSION == 34
        "reaching",
        "returning",
#endif
        "chaos",
#if TAG_MAJOR_VERSION == 34
        "evasion",
        "confuse",
#endif
        "penetration",
        "reaping",
        "spectral",
        nullptr
    };
    COMPILE_CHECK(ARRAYSZ(weapon_brands) == NUM_REAL_SPECIAL_WEAPONS);

    const char* missile_brands[] =
    {
        "flame",
        "frost",
        "poisoned",
        "curare",
#if TAG_MAJOR_VERSION == 34
        "returning",
#endif
        "chaos",
#if TAG_MAJOR_VERSION == 34
        "penetration",
#endif
        "dispersal",
        "exploding",
#if TAG_MAJOR_VERSION == 34
        "steel",
#endif
        "silver",
#if TAG_MAJOR_VERSION == 34
        "paralysis",
        "slow",
        "sleep",
        "confusion",
        "sickness",
#endif
        "datura",
        "atropa",
        nullptr
    };
    COMPILE_CHECK(ARRAYSZ(missile_brands) == NUM_REAL_SPECIAL_MISSILES);

    const char** name_lists[3] = {armour_egos, weapon_brands, missile_brands};

    int armour_order[3]  = {0, 1, 2};
    int weapon_order[3]  = {1, 0, 2};
    int missile_order[3] = {2, 0, 1};

    int *order;

    switch (item_type)
    {
    case OBJ_ARMOUR:
        order = armour_order;
        break;

    case OBJ_WEAPONS:
        order = weapon_order;
        break;

    case OBJ_MISSILES:
#if TAG_MAJOR_VERSION == 34
        // HACK to get an old save to load; remove me soon?
        if (ego_str == "sleeping")
            return SPMSL_SLEEP;
#endif
        order = missile_order;
        break;

    default:
        die("Bad base_type for ego'd item.");
        return 0;
    }

    const char** allowed = name_lists[order[0]];

    for (int i = 0; allowed[i] != nullptr; i++)
    {
        if (ego_str == allowed[i])
            return i + 1;
    }

    // Incompatible or non-existent ego type
    for (int i = 1; i <= 2; i++)
    {
        const char** list = name_lists[order[i]];

        for (int j = 0; list[j] != nullptr; j++)
            if (ego_str == list[j])
                // Ego incompatible with base type.
                return -1;
    }

    // Non-existent ego
    return 0;
}

int item_list::parse_acquirement_source(const string &source)
{
    const string god_name(replace_all_of(source, "_", " "));
    const god_type god(str_to_god(god_name));
    if (god == GOD_NO_GOD)
        error = make_stringf("unknown god name: '%s'", god_name.c_str());
    return god;
}

bool item_list::monster_corpse_is_valid(monster_type *mons,
                                        const string &name,
                                        bool skeleton)
{
    if (*mons == RANDOM_NONBASE_DRACONIAN)
    {
        error = "Can't use non-base monster for corpse/chunk items";
        return false;
    }

    // Accept randomised types without further checks:
    if (*mons >= NUM_MONSTERS)
        return true;

    // Convert to the monster species:
    *mons = mons_species(*mons);

    if (!mons_class_can_leave_corpse(*mons))
    {
        error = make_stringf("'%s' cannot leave corpses", name.c_str());
        return false;
    }

    if (skeleton && !mons_skeleton(*mons))
    {
        error = make_stringf("'%s' has no skeleton", name.c_str());
        return false;
    }

    // We're ok.
    return true;
}

bool item_list::parse_corpse_spec(item_spec &result, string s)
{
    const bool never_decay = strip_tag(s, "never_decay");

    if (never_decay)
        result.props[CORPSE_NEVER_DECAYS].get_bool() = true;

    const bool corpse = strip_suffix(s, "corpse");
    const bool skeleton = !corpse && strip_suffix(s, "skeleton");

    result.base_type = OBJ_CORPSES;
    result.sub_type  = static_cast<int>(corpse ? CORPSE_BODY :
                                         CORPSE_SKELETON);

    // The caller wants a specific monster, no doubt with the best of
    // motives. Let's indulge them:
    mons_list mlist;
    const string mons_parse_err = mlist.add_mons(s, true);
    if (!mons_parse_err.empty())
    {
        error = mons_parse_err;
        return false;
    }

    // Get the actual monster spec:
    mons_spec spec = mlist.get_monster(0);
    monster_type mtype = static_cast<monster_type>(spec.type);
    if (!monster_corpse_is_valid(&mtype, s, skeleton))
    {
        error = make_stringf("Requested corpse '%s' is invalid",
                             s.c_str());
        return false;
    }

    // Ok, looking good, the caller can have their requested toy.
    result.set_corpse_monster_spec(spec);
    return true;
}

bool item_list::parse_single_spec(item_spec& result, string s)
{
    // If there's a colon, this must be a generation weight.
    int weight = find_weight(s);
    if (weight != TAG_UNFOUND)
    {
        result.genweight = weight;
        if (result.genweight <= 0)
        {
            error = make_stringf("Bad item generation weight: '%d'",
                                 result.genweight);
            return false;
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

    // When placing corpses, use place:Elf:$ to choose monsters
    // appropriate for that level, as an example.
    const string place = strip_tag_prefix(s, "place:");
    if (!place.empty())
    {
        try
        {
            result.place = level_id::parse_level_id(place);
        }
        catch (const bad_level_id &err)
        {
            error = err.what();
            return false;
        }
    }

    const string acquirement_source = strip_tag_prefix(s, "acquire:");
    if (!acquirement_source.empty() || strip_tag(s, "acquire"))
    {
        if (!acquirement_source.empty())
        {
            result.acquirement_source =
                parse_acquirement_source(acquirement_source);
        }
        // If requesting acquirement, must specify item base type or
        // "any".
        result.level = ISPEC_ACQUIREMENT;
        if (s == "any")
            result.base_type = OBJ_RANDOM;
        else
            parse_random_by_class(s, result);
        return true;
    }

    string ego_str  = strip_tag_prefix(s, "ego:");

    string id_str = strip_tag_prefix(s, "ident:");
    if (id_str == "all")
        result.props[IDENT_KEY].get_int() = ISFLAG_IDENT_MASK;
    else if (!id_str.empty())
    {
        vector<string> ids = split_string("|", id_str);
        int id = 0;
        for (const auto &is : ids)
        {
            if (is == "type")
                id |= ISFLAG_KNOW_TYPE;
            else if (is == "pluses")
                id |= ISFLAG_KNOW_PLUSES;
            else if (is == "properties")
                id |= ISFLAG_KNOW_PROPERTIES;
            else
            {
                error = make_stringf("Bad identify status: %s", id_str.c_str());
                return false;
            }
        }
        result.props[IDENT_KEY].get_int() = id;
    }

    if (strip_tag(s, "good_item"))
        result.level = ISPEC_GOOD_ITEM;
    else
    {
        int number = strip_number_tag(s, "level:");
        if (number != TAG_UNFOUND)
        {
            if (number <= 0 && number != ISPEC_STAR && number != ISPEC_SUPERB
                && number != ISPEC_DAMAGED && number != ISPEC_BAD)
            {
                error = make_stringf("Bad item level: %d", number);
                return false;
            }

            result.level = number;
        }
    }

    if (strip_tag(s, "mundane"))
    {
        result.level = ISPEC_MUNDANE;
        result.ego   = -1;
    }
    if (strip_tag(s, "damaged"))
        result.level = ISPEC_DAMAGED;
    if (strip_tag(s, "randart"))
        result.level = ISPEC_RANDART;
    if (strip_tag(s, "useful"))
        result.props[USEFUL_KEY] = bool(true);
    if (strip_tag(s, "unobtainable"))
        result.props[UNOBTAINABLE_KEY] = true;
    if (strip_tag(s, "no_exclude"))
        result.props[NO_EXCLUDE_KEY] = true;
    if (strip_tag(s, "chaotic"))
        result.props[CHAOTIC_ITEM_KEY] = true;

    const int mimic = strip_number_tag(s, "mimic:");
    if (mimic != TAG_UNFOUND)
        result.props[MIMIC_KEY] = mimic;
    if (strip_tag(s, "mimic"))
        result.props[MIMIC_KEY] = 1;

    if (strip_tag(s, "no_pickup"))
        result.props[NO_PICKUP_KEY] = true;

    const short charges = strip_number_tag(s, "charges:");
    if (charges >= 0)
        result.props[CHARGES_KEY].get_int() = charges;

    const string custom_name = strip_tag_prefix(s, "itemname:");
    if (!custom_name.empty())
        result.props[ITEM_NAME_KEY] = custom_name;

    string original = s;
    const int plus = strip_number_tag(s, "plus:");
    if (plus == TAG_UNFOUND)
    {
        const string num = strip_tag_prefix(original, "plus:");
        if (!num.empty())
        {
            error = make_stringf("Bad item plus: %s", num.c_str());
            return false;
        }
    }
    else
        result.props[PLUS_KEY].get_int() = plus;

    string artprops = strip_tag_prefix(s, "artprops:");
    if (!artprops.empty())
    {
        auto &fixed_artp = result.props[FIXED_PROPS_KEY].get_table();
        vector<string> prop_list = split_string("&", artprops);
        for (auto prop_def : prop_list)
        {
            vector<string> prop_parts = split_string(":", prop_def, true,
                    false, 1);

            const auto prop = artp_type_from_name(prop_parts[0]);
            if (prop == ARTP_NUM_PROPERTIES)
            {
                error = make_stringf("Unknown artefact property: %s",
                        prop_parts[0].c_str());
                return false;
            }

            int val = 1;
            if (prop_parts.size() == 2 && !parse_int(prop_parts[1].c_str(), val))
            {
                error = make_stringf("Bad artefact property value: %s",
                        prop_parts[1].c_str());
                return false;
            }

            if (prop == ARTP_BRAND)
            {
                error = make_stringf("Item brand can only be set with the "
                        "'ego:' tag.");
                return false;
            }

            const string prop_name = artp_name(prop);
            if (!artp_value_is_valid(prop, val))
            {
                error = make_stringf("Bad value for artefact property %s: %d",
                        prop_name.c_str(), val);
                return false;
            }

            fixed_artp[prop_name] = val;
        }

        // Setting fixed properties always make the item randart.
        result.level = ISPEC_RANDART;
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
                return false;
            }
            result.allow_uniques = uniq;
        }
    }

    // XXX: This is nice-ish now, but could probably do with being improved.
    if (strip_tag(s, "randbook"))
    {
        result.props[THEME_BOOK_KEY] = true;
        // build_themed_book requires the following properties:
        // disc: <first discipline>, disc2: <optional second discipline>
        // numspells: <total number of spells>, slevels: <maximum levels>
        // spell: <include this spell>, owner:<name of owner>
        // None of these are required, but if you don't intend on using any
        // of them, use "any fixed theme book" instead.
        spschool disc1 = spschool::none;
        spschool disc2 = spschool::none;

        string st_disc1 = strip_tag_prefix(s, "disc:");
        if (!st_disc1.empty())
        {
            disc1 = school_by_name(st_disc1);
            if (disc1 == spschool::none)
            {
                error = make_stringf("Bad spell school: %s", st_disc1.c_str());
                return false;
            }
        }

        string st_disc2 = strip_tag_prefix(s, "disc2:");
        if (!st_disc2.empty())
        {
            disc2 = school_by_name(st_disc2);
            if (disc2 == spschool::none)
            {
                error = make_stringf("Bad spell school: %s", st_disc2.c_str());
                return false;
            }
        }

        if (disc1 == spschool::none && disc2 != spschool::none)
        {
            // Don't fail, just quietly swap. Any errors in disc1's syntax will
            // have been caught above, anyway.
            swap(disc1, disc2);
        }

        short num_spells = strip_number_tag(s, "numspells:");
        if (num_spells == TAG_UNFOUND)
            num_spells = -1;
        else if (num_spells <= 0 || num_spells > RANDBOOK_SIZE)
        {
            error = make_stringf("Bad spellbook size: %d", num_spells);
            return false;
        }

        short slevels = strip_number_tag(s, "slevels:");
        if (slevels == TAG_UNFOUND)
            slevels = -1;
        else if (slevels == 0)
        {
            error = make_stringf("Bad slevels: %d.", slevels);
            return false;
        }

        const string title = replace_all_of(strip_tag_prefix(s, "title:"),
                                            "_", " ");

        const string spells = strip_tag_prefix(s, "spells:");

        vector<string> spell_list = split_string("&", spells);
        CrawlVector &incl_spells
            = result.props[RANDBK_SPELLS_KEY].new_vector(SV_INT);

        for (const string &spnam : spell_list)
        {
            string spell_name = replace_all_of(spnam, "_", " ");
            spell_type spell = spell_by_name(spell_name);
            if (spell == SPELL_NO_SPELL)
            {
                error = make_stringf("Bad spell: %s", spnam.c_str());
                return false;
            }
            incl_spells.push_back(spell);
        }

        const string owner = replace_all_of(strip_tag_prefix(s, "owner:"),
                                            "_", " ");

        result.props[RANDBK_DISC1_KEY].get_short() = static_cast<short>(disc1);
        result.props[RANDBK_DISC2_KEY].get_short() = static_cast<short>(disc2);
        result.props[RANDBK_NSPELLS_KEY] = num_spells;
        result.props[RANDBK_SLVLS_KEY] = slevels;
        result.props[RANDBK_TITLE_KEY] = title;
        result.props[RANDBK_OWNER_KEY] = owner;

        result.base_type = OBJ_BOOKS;
        // This is changed in build_themed_book().
        result.sub_type = BOOK_MINOR_MAGIC;
        result.plus = -1;

        return true;
    }

    if (s.find("deck") != string::npos)
    {
        error = make_stringf("removed deck: \"%s\".", s.c_str());
        return false;
    }

    string tile = strip_tag_prefix(s, "tile:");
    if (!tile.empty())
    {
        tileidx_t index;
        if (!tile_main_index(tile.c_str(), &index))
        {
            error = make_stringf("bad tile name: \"%s\".", tile.c_str());
            return false;
        }
        result.props[ITEM_TILE_NAME_KEY].get_string() = tile;
    }

    tile = strip_tag_prefix(s, "wtile:");
    if (!tile.empty())
    {
        tileidx_t index;
        if (!tile_player_index(tile.c_str(), &index))
        {
            error = make_stringf("bad tile name: \"%s\".", tile.c_str());
            return false;
        }
        result.props[WORN_TILE_NAME_KEY].get_string() = tile;
    }

    // Clean up after any tag brain damage.
    trim_string(s);

    if (!ego_str.empty())
        error = "Can't set an ego for random items.";

    // Completely random?
    if (s == "random" || s == "any" || s == "%")
        return true;

    if (s == "*" || s == "star_item")
    {
        result.level = ISPEC_STAR;
        return true;
    }
    else if (s == "|" || s == "superb_item")
    {
        result.level = ISPEC_SUPERB;
        return true;
    }
    else if (s == "$" || s == "gold")
    {
        if (!ego_str.empty())
        {
            error = "Can't set an ego for gold.";
            return false;
        }

        result.base_type = OBJ_GOLD;
        result.sub_type  = OBJ_RANDOM;
        return true;
    }

    if (s == "nothing")
    {
        error.clear();
        result.base_type = OBJ_UNASSIGNED;
        return true;
    }

    error.clear();

    // Look for corpses, skeletons:
    if (ends_with(s, "corpse") || ends_with(s, "skeleton"))
        return parse_corpse_spec(result, s);

    const int unrand_id = get_unrandart_num(s.c_str());
    if (unrand_id)
    {
        result.ego = -unrand_id; // lol
        return true;
    }

    // Check for "any objclass"
    if (starts_with(s, "any "))
        parse_random_by_class(s.substr(4), result);
    else if (starts_with(s, "random "))
        parse_random_by_class(s.substr(7), result);
    // Check for actual item names.
    else
        parse_raw_name(s, result);

    // XXX: Ideally we'd have a common function to check validate plus values
    // by item type and subtype.
    if ((result.base_type == OBJ_ARMOUR || result.base_type == OBJ_WEAPONS)
        && plus != TAG_UNFOUND
        && abs(plus) > 30)
    {
        error = make_stringf("Item plus too high: %d", plus);
        return false;
    }

    if (!error.empty())
        return false;

    if (ego_str.empty())
        return true;

    if (result.base_type != OBJ_WEAPONS
        && result.base_type != OBJ_MISSILES
        && result.base_type != OBJ_ARMOUR)
    {
        error = "An ego can only be applied to a weapon, missile or "
            "armour.";
        return false;
    }

    if (ego_str == "none")
    {
        result.ego = -1;
        return true;
    }

    const int ego = str_to_ego(result.base_type, ego_str);

    if (ego == 0)
    {
        error = make_stringf("No such ego as: %s", ego_str.c_str());
        return false;
    }
    else if (ego == -1)
    {
        error = make_stringf("Ego '%s' is invalid for item '%s'.",
                             ego_str.c_str(), s.c_str());
        return false;
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
        return false;
    }
    result.ego = ego;
    return true;
}

void item_list::parse_random_by_class(string c, item_spec &spec)
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
    else if (c == "ring")
    {
        spec.base_type = OBJ_JEWELLERY;
        spec.sub_type = NUM_RINGS;
        return;
    }
    else if (c == "amulet")
    {
        spec.base_type = OBJ_JEWELLERY;
        spec.sub_type = NUM_JEWELLERY;
        return;
    }
    if (c == "hex wand")
    {
        spec.base_type = OBJ_WANDS;
        spec.sub_type = item_for_set(ITEM_SET_HEX_WANDS);
        return;
    }
    if (c == "beam wand")
    {
        spec.base_type = OBJ_WANDS;
        spec.sub_type = item_for_set(ITEM_SET_BEAM_WANDS);
        return;
    }
    if (c == "blast wand")
    {
        spec.base_type = OBJ_WANDS;
        spec.sub_type = item_for_set(ITEM_SET_BLAST_WANDS);
        return;
    }
    if (c == "ally scroll")
    {
        spec.base_type = OBJ_SCROLLS;
        spec.sub_type = item_for_set(ITEM_SET_ALLY_SCROLLS);
        return;
    }

    if (c == "area misc")
    {
        spec.base_type = OBJ_MISCELLANY;
        spec.sub_type = item_for_set(ITEM_SET_AREA_MISCELLANY);
        return;
    }

    if (c == "ally misc")
    {
        spec.base_type = OBJ_MISCELLANY;
        spec.sub_type = item_for_set(ITEM_SET_ALLY_MISCELLANY);
        return;
    }

    if (c == "control misc")
    {
        spec.base_type = OBJ_MISCELLANY;
        spec.sub_type = item_for_set(ITEM_SET_CONTROL_MISCELLANY);
        return;
    }

    error = make_stringf("Bad item class: '%s'", c.c_str());
}

void item_list::parse_raw_name(string name, item_spec &spec)
{
    trim_string(name);
    if (name.empty())
    {
        error = make_stringf("Bad item name: '%s'", name.c_str());
        return ;
    }

    item_kind parsed = item_kind_by_name(name);
    if (parsed.base_type != OBJ_UNASSIGNED)
    {
        spec.base_type = parsed.base_type;
        spec.sub_type  = parsed.sub_type;
        spec.plus      = parsed.plus;
        spec.plus2     = parsed.plus2;
        return;
    }

    error = make_stringf("Bad item name: '%s'", name.c_str());
}

item_list::item_spec_slot item_list::parse_item_spec(string spec)
{
    // lowercase(spec);

    item_spec_slot list;

    for (const string &specifier : split_string("/", spec))
    {
        item_spec parsed_spec;
        if (!parse_single_spec(parsed_spec, specifier))
        {
            dprf(DIAG_DNGN, "Failed to parse: %s", specifier.c_str());
            continue;
        }
        if (parsed_spec.props.exists(NO_EXCLUDE_KEY)
            || !item_excluded_from_set(parsed_spec.base_type, parsed_spec.sub_type))
        {
            list.ilist.push_back(parsed_spec);
        }
    }

    return list;
}

/////////////////////////////////////////////////////////////////////////
// subst_spec

subst_spec::subst_spec(string _k, bool _f, const glyph_replacements_t &g)
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
        return frozen_value;

    int cumulative = 0;
    int chosen = 0;
    for (glyph_weighted_replacement_t rep : repl)
        if (x_chance_in_y(rep.second, cumulative += rep.second))
            chosen = rep.first;

    if (fix)
        frozen_value = chosen;

    return chosen;
}

//////////////////////////////////////////////////////////////////////////
// nsubst_spec

nsubst_spec::nsubst_spec(string _key, const vector<subst_spec> &_specs)
    : key(_key), specs(_specs)
{
}

//////////////////////////////////////////////////////////////////////////
// colour_spec

int colour_spec::get_colour()
{
    if (fixed_colour != BLACK)
        return fixed_colour;

    int chosen = BLACK;
    int cweight = 0;
    for (map_weighted_colour col : colours)
        if (x_chance_in_y(col.second, cweight += col.second))
            chosen = col.first;
    if (fix)
        fixed_colour = chosen;
    return chosen;
}

//////////////////////////////////////////////////////////////////////////
// fprop_spec

feature_property_type fprop_spec::get_property()
{
    if (fixed_prop != FPROP_NONE)
        return fixed_prop;

    feature_property_type chosen = FPROP_NONE;
    int cweight = 0;
    for (map_weighted_fprop fprop : fprops)
        if (x_chance_in_y(fprop.second, cweight += fprop.second))
            chosen = fprop.first;
    if (fix)
        fixed_prop = chosen;
    return chosen;
}

//////////////////////////////////////////////////////////////////////////
// fheight_spec

int fheight_spec::get_height()
{
    if (fixed_height != INVALID_HEIGHT)
        return fixed_height;

    int chosen = INVALID_HEIGHT;
    int cweight = 0;
    for (map_weighted_fheight fh : fheights)
        if (x_chance_in_y(fh.second, cweight += fh.second))
            chosen = fh.first;
    if (fix)
        fixed_height = chosen;
    return chosen;
}

//////////////////////////////////////////////////////////////////////////
// string_spec

string string_spec::get_property()
{
    if (!fixed_str.empty())
        return fixed_str;

    string chosen = "";
    int cweight = 0;
    for (const map_weighted_string &str : strlist)
        if (x_chance_in_y(str.second, cweight += str.second))
            chosen = str.first;
    if (fix)
        fixed_str = chosen;
    return chosen;
}

//////////////////////////////////////////////////////////////////////////
// map_marker_spec

string map_marker_spec::apply_transform(map_lines &map)
{
    vector<coord_def> positions = map.find_glyph(key);

    // Markers with no key are not an error.
    if (positions.empty())
        return "";

    for (coord_def p : positions)
    {
        try
        {
            map_marker *mark = create_marker();
            if (!mark)
            {
                return make_stringf("Unable to parse marker from %s",
                                    marker.c_str());
            }
            mark->pos = p;
            map.add_marker(mark);
        }
        catch (const bad_map_marker &err)
        {
            return err.what();
        }
    }
    return "";
}

map_marker *map_marker_spec::create_marker()
{
    return lua_fn
        ? new map_lua_marker(*lua_fn)
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

map_flags &map_flags::operator |= (const map_flags &o)
{
    flags_set |= o.flags_set;
    flags_unset |= o.flags_unset;

    // In the event of conflict, the later flag set (o here) wins.
    flags_set &= ~o.flags_unset;
    flags_unset &= ~o.flags_set;

    return *this;
}
typedef map<string, unsigned long> flag_map;

map_flags map_flags::parse(const string flag_list[], const string &s)
{
    map_flags mf;

    const vector<string> segs = split_string("/", s);

    flag_map flag_vals;
    for (int i = 0; !flag_list[i].empty(); i++)
        flag_vals[flag_list[i]] = 1 << i;

    for (string flag: segs)
    {
        bool negate = false;

        if (flag[0] == '!')
        {
            flag   = flag.substr(1);
            negate = true;
        }

        if (unsigned long *val = map_find(flag_vals, flag))
        {
            if (negate)
                mf.flags_unset |= *val;
            else
                mf.flags_set |= *val;
        }
        else
            throw bad_map_flag(flag);
    }

    return mf;
}

//////////////////////////////////////////////////////////////////////////
// keyed_mapspec

keyed_mapspec::keyed_mapspec()
    :  key_glyph(-1), feat(), item(), mons()
{
}

string keyed_mapspec::set_feat(const string &s, bool fix)
{
    err.clear();
    parse_features(s);
    feat.fix_slot = fix;

    // Fix this feature.
    if (fix)
        get_feat();

    return err;
}

void keyed_mapspec::copy_feat(const keyed_mapspec &spec)
{
    feat = spec.feat;
}

void keyed_mapspec::parse_features(const string &s)
{
    feat.feats.clear();
    for (const string &spec : split_string("/", s))
    {
        feature_spec_list feats = parse_feature(spec);
        if (!err.empty())
            return;
        feat.feats.insert(feat.feats.end(),
                           feats.begin(),
                           feats.end());
    }
}

/**
 * Convert a trap string into a trap_spec.
 *
 * This function converts an incoming trap specification string from a vault
 * into a trap_spec.
 *
 * @param s       The string to be parsed.
 * @param weight  The weight of this string.
 * @return        A feature_spec with the contained, parsed trap_spec stored via
 *                unique_ptr as feature_spec->trap.
**/
feature_spec keyed_mapspec::parse_trap(string s, int weight)
{
    strip_tag(s, "trap");

    trim_string(s);
    lowercase(s);

    const int trap = str_to_trap(s);
    if (trap == -1)
        err = make_stringf("bad trap name: '%s'", s.c_str());

    feature_spec fspec(1, weight);
    fspec.trap.reset(new trap_spec(static_cast<trap_type>(trap)));
    return fspec;
}

/**
 * Convert a shop string into a shop_spec.
 *
 * This function converts an incoming shop specification string from a vault
 * into a shop_spec.
 *
 * @param s        The string to be parsed.
 * @param weight   The weight of this string.
 * @param mimic    What kind of mimic (if any) to set for this shop.
 * @param no_mimic Whether to prohibit mimics altogether for this shop.
 * @return         A feature_spec with the contained, parsed shop_spec stored
 *                 via unique_ptr as feature_spec->shop.
**/
feature_spec keyed_mapspec::parse_shop(string s, int weight, int mimic,
                                       bool no_mimic)
{
    string orig(s);

    strip_tag(s, "shop");
    trim_string(s);

    bool use_all = strip_tag(s, "use_all");

    const bool gozag = strip_tag(s, "gozag");

    string shop_name = replace_all_of(strip_tag_prefix(s, "name:"), "_", " ");
    string shop_type_name = replace_all_of(strip_tag_prefix(s, "type:"),
                                           "_", " ");
    string shop_suffix_name = replace_all_of(strip_tag_prefix(s, "suffix:"),
                                             "_", " ");

    int num_items = min(20, strip_number_tag(s, "count:"));
    if (num_items == TAG_UNFOUND)
        num_items = -1;

    int greed = strip_number_tag(s, "greed:");
    if (greed == TAG_UNFOUND)
        greed = -1;

    vector<string> parts = split_string(";", s);
    string main_part = parts[0];

    const shop_type shop = str_to_shoptype(main_part);
    if (shop == SHOP_UNASSIGNED)
        err = make_stringf("bad shop type: '%s'", s.c_str());

    if (parts.size() > 2)
        err = make_stringf("too many semi-colons for '%s' spec", orig.c_str());

    item_list items;
    if (err.empty() && parts.size() == 2)
    {
        string item_list = parts[1];
        vector<string> str_items = split_string("|", item_list);
        for (const string &si : str_items)
        {
            err = items.add_item(si);
            if (!err.empty())
                break;
        }
    }

    feature_spec fspec(-1, weight, mimic, no_mimic);
    fspec.shop.reset(new shop_spec(shop, shop_name, shop_type_name,
                                   shop_suffix_name, greed,
                                   num_items, use_all, gozag));
    fspec.shop->items = items;
    return fspec;
}

feature_spec_list keyed_mapspec::parse_feature(const string &str)
{
    string s = str;
    int weight = find_weight(s);
    if (weight == TAG_UNFOUND || weight <= 0)
        weight = 10;

    int mimic = strip_number_tag(s, "mimic:");
    if (mimic == TAG_UNFOUND && strip_tag(s, MIMIC_KEY))
        mimic = 1;
    const bool no_mimic = strip_tag(s, "no_mimic");

    trim_string(s);

    feature_spec_list list;
    if (s.length() == 1)
    {
        feature_spec fsp(-1, weight, mimic, no_mimic);
        fsp.glyph = s[0];
        list.push_back(fsp);
    }
    else if (strip_tag(s, "trap") || s == "web")
        list.push_back(parse_trap(s, weight));
    else if (strip_tag(s, "shop"))
        list.push_back(parse_shop(s, weight, mimic, no_mimic));
    else if (auto ftype = dungeon_feature_by_name(s)) // DNGN_UNSEEN == 0
        list.emplace_back(ftype, weight, mimic, no_mimic);
    else
        err = make_stringf("no features matching \"%s\"", str.c_str());

    return list;
}

string keyed_mapspec::set_mons(const string &s, bool fix)
{
    err.clear();
    mons.clear();

    for (const string &segment : split_string(",", s))
    {
        const string error = mons.add_mons(segment, fix);
        if (!error.empty())
            return error;
    }

    return "";
}

string keyed_mapspec::set_item(const string &s, bool fix)
{
    err.clear();
    item.clear();

    for (const string &seg : split_string(",", s))
    {
        err = item.add_item(seg, fix);
        if (!err.empty())
            return err;
    }

    return err;
}

string keyed_mapspec::set_mask(const string &s, bool /*garbage*/)
{
    err.clear();

    try
    {
        // Be sure to change the order of map_mask_type to match!
        static string flag_list[] =
            {"vault", "no_item_gen", "no_monster_gen", "no_pool_fixup",
             "UNUSED",
             "no_wall_fixup", "opaque", "no_trap_gen", ""};
        map_mask |= map_flags::parse(flag_list, s);
    }
    catch (const bad_map_flag &error)
    {
        err = make_stringf("Unknown flag: '%s'", error.what());
    }

    return err;
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
    return mons;
}

item_list &keyed_mapspec::get_items()
{
    return item;
}

map_flags &keyed_mapspec::get_mask()
{
    return map_mask;
}

bool keyed_mapspec::replaces_glyph()
{
    return !(mons.empty() && item.empty() && feat.feats.empty());
}

//////////////////////////////////////////////////////////////////////////
// feature_spec and feature_slot

feature_spec::feature_spec()
{
    genweight = 0;
    feat = 0;
    glyph = -1;
    shop.reset(nullptr);
    trap.reset(nullptr);
    mimic = 0;
    no_mimic = false;
}

feature_spec::feature_spec(int f, int wt, int _mimic, bool _no_mimic)
{
    genweight = wt;
    feat = f;
    glyph = -1;
    shop.reset(nullptr);
    trap.reset(nullptr);
    mimic = _mimic;
    no_mimic = _no_mimic;
}

feature_spec::feature_spec(const feature_spec &other)
{
    init_with(other);
}

feature_spec& feature_spec::operator = (const feature_spec& other)
{
    if (this != &other)
        init_with(other);
    return *this;
}

void feature_spec::init_with(const feature_spec& other)
{
    genweight = other.genweight;
    feat = other.feat;
    glyph = other.glyph;
    mimic = other.mimic;
    no_mimic = other.no_mimic;

    if (other.trap)
        trap.reset(new trap_spec(*other.trap));
    else
        trap.reset(nullptr);

    if (other.shop)
        shop.reset(new shop_spec(*other.shop));
    else
        shop.reset(nullptr);
}

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

    for (const feature_spec &feat : feats)
        if (x_chance_in_y(feat.genweight, tweight += feat.genweight))
            chosen_feat = feat;

    if (fix_slot)
    {
        feats.clear();
        feats.push_back(chosen_feat);
    }
    return chosen_feat;
}
