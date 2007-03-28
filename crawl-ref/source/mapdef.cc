#include <cstdarg>
#include <cstdio>
#include <cctype>

#include "AppHdr.h"
#include "describe.h"
#include "direct.h"
#include "invent.h"
#include "items.h"
#include "libutil.h"
#include "mapdef.h"
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
    return strip_number_tag(s, "weight:");
}

static std::string split_key_item(const std::string &s,
                                  int *key,
                                  int *separator,
                                  std::string *arg)
{
    std::string::size_type
        norm = s.find("="),
        fixe = s.find(":");

    const std::string::size_type sep = norm < fixe? norm : fixe;
    if (sep == std::string::npos)
        return ("malformed declaration - must use = or :");

    std::string what_to_subst = trimmed_string(s.substr(0, sep));
    std::string substitute    = trimmed_string(s.substr(sep + 1));

    if (what_to_subst.length() != 1)
        return make_stringf("selector '%s' must be exactly one character",
                            what_to_subst.c_str());

    *key = what_to_subst[0];
    *arg = substitute;
    *separator = s[sep];
    
    return ("");
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

////////////////////////////////////////////////////////////////////////
// map_transformer

map_transformer::~map_transformer()
{
}

////////////////////////////////////////////////////////////////////////
// map_lines

map_lines::map_lines()
    : transforms(), lines(), map_width(0), solid_north(false),
      solid_east(false), solid_south(false), solid_west(false),
      solid_checked(false)
{
}

map_lines::map_lines(const map_lines &map)
{
    init_from(map);
}

map_lines &map_lines::operator = (const map_lines &map)
{
    if (this != &map)
        init_from(map);
    return (*this);
}

map_lines::~map_lines()
{
    release_transforms();
}

void map_lines::init_from(const map_lines &map)
{
    release_transforms();
    lines         = map.lines;
    map_width     = map.map_width;
    solid_north   = map.solid_north;
    solid_east    = map.solid_east;
    solid_south   = map.solid_south;
    solid_west    = map.solid_west;
    solid_checked = map.solid_checked;

    for (int i = 0, size = map.transforms.size(); i < size; ++i)
        transforms.push_back( map.transforms[i]->clone() );
}

void map_lines::release_transforms()
{
    for (int i = 0, size = transforms.size(); i < size; ++i)
        delete transforms[i];
    transforms.clear();
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
    int key = 0;
    std::string substitute;

    std::string err = split_key_item(sub, &key, &sep, &substitute);
    if (!err.empty())
        return (err);

    glyph_replacements_t repl;
    err = parse_glyph_replacements(substitute, repl);
    if (!err.empty())
        return (err);

    transforms.push_back( new subst_spec( key, sep == ':', repl ) );

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
    release_transforms();
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

std::string map_lines::block_shuffle(const std::string &s)
{
    std::vector<std::string> segs = split_string("/", s);

    std::vector<std::string> shuffled;
    for (int i = 0, size = segs.size(); i < size; ++i)
    {
        const int sel = random2(segs.size());
        
        shuffled.push_back( segs[ sel ] );
        segs.erase( segs.begin() + sel );
    }

    return comma_separated_line(shuffled.begin(), shuffled.end(), "/", "/");
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

void map_lines::apply_transforms()
{
    for (int i = 0, size = transforms.size(); i < size; ++i)
        transforms[i]->apply_transform(*this);
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
    items.clear();
    keyspecs.clear();
    depth.reset();
    orient = MAP_NONE;

    // Base chance; this is not a percentage.
    chance = 10;

    // The map designer must explicitly disallow these if unwanted.
    flags = MAPF_MIRROR_VERTICAL | MAPF_MIRROR_HORIZONTAL
            | MAPF_ROTATE;

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
    map.apply_transforms();
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

keyed_mapspec *map_def::mapspec_for_key(int key)
{
    keyed_specs::iterator i = keyspecs.find(key);
    return i != keyspecs.end()? &i->second : NULL;
}

std::string map_def::add_key_field(
    const std::string &s,
    std::string (keyed_mapspec::*set_field)(const std::string &s, bool fixed))
{
    int key = 0;
    int separator = 0;
    std::string arg;

    std::string err = split_key_item(s, &key, &separator, &arg);
    if (!err.empty())
        return (err);

    keyed_mapspec &km = keyspecs[key];
    km.key_glyph = key;
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
    return (summon_any_demon( demon ));
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

mons_list::mons_spec_slot mons_list::parse_mons_spec(std::string spec)
{
    mons_spec_slot slot;

    slot.fix_slot = strip_tag(spec, "fix_slot");

    std::vector<std::string> specs = split_string("/", spec);

    for (int i = 0, ssize = specs.size(); i < ssize; ++i)
    {
        std::string s = specs[i];
        int weight = find_weight(s);
        if (weight == TAG_UNFOUND || weight <= 0)
            weight = 10;

        const bool fixmons = strip_tag(s, "fix_mons");
        const bool generate_awake = strip_tag(s, "generate_awake");

        trim_string(s);
        const int mid = mons_by_name(s);

        if (mid == MONS_PROGRAM_BUG)
        {
            error = make_stringf("unrecognised monster \"%s\"", s.c_str());
            return (slot);
        }

        slot.mlist.push_back( mons_spec(mid, weight, fixmons, generate_awake) );
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

int mons_list::mons_by_name(std::string name) const
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
            spec.base_type = type;
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

    item_def parsed = find_item_type(-1, name);
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

void subst_spec::apply_transform(map_lines &map)
{
    map.subst(*this);
}

map_transformer *subst_spec::clone() const
{
    return new subst_spec(*this);
}

//////////////////////////////////////////////////////////////////////////
// shuffle_spec

void shuffle_spec::apply_transform(map_lines &map)
{
    map.resolve_shuffle(shuffle);
}

map_transformer *shuffle_spec::clone() const
{
    return new shuffle_spec(*this);
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
    
    std::vector<dungeon_feature_type> feats = features_by_desc(s);
    for (int i = 0, size = feats.size(); i < size; ++i)
        list.push_back( feature_spec(feats[i], weight) );

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
    
    return (item.add_item(s, fix));
}

feature_spec keyed_mapspec::get_feat()
{
    return feat.get_feat(key_glyph);
}

mons_spec keyed_mapspec::get_mons()
{
    return (mons.size()? mons.get_monster(0) : mons_spec(-1));
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
