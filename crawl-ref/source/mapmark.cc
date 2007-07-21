/*
 *  File:       mapmark.cc
 *  Summary:    Level markers (annotations).
 *
 *  Modified for Crawl Reference by $Author: dshaligram $ on $Date: 2007-07-20T11:40:25.964128Z $
 *  
 */

#include "AppHdr.h"
#include "mapmark.h"

#include "direct.h"
#include "libutil.h"
#include "luadgn.h"
#include "stuff.h"
#include "tags.h"
#include "view.h"

////////////////////////////////////////////////////////////////////////
// Dungeon markers

map_marker::marker_reader map_marker::readers[NUM_MAP_MARKER_TYPES] =
{
    &map_feature_marker::read,
    &map_timed_feature_marker::read,
};

map_marker::marker_parser map_marker::parsers[NUM_MAP_MARKER_TYPES] =
{
    &map_feature_marker::parse,
    &map_timed_feature_marker::parse,
};

map_marker::map_marker(map_marker_type t, const coord_def &p)
    : pos(p), type(t)
{
}

map_marker::~map_marker()
{
}

void map_marker::activate()
{
}

void map_marker::write(tagHeader &outf) const
{
    marshallShort(outf, type);
    marshallCoord(outf, pos);
}

void map_marker::read(tagHeader &inf)
{
    // Don't read type! The type has to be read by someone who knows how
    // to look up the unmarshall function.
    unmarshallCoord(inf, pos);
}

map_marker *map_marker::read_marker(tagHeader &inf)
{
    const map_marker_type type =
        static_cast<map_marker_type>(unmarshallShort(inf));
    return readers[type]? (*readers[type])(inf, type) : NULL;
}

map_marker *map_marker::parse_marker(const std::string &s) throw (std::string)
{
    for (int i = 0; i < NUM_MAP_MARKER_TYPES; ++i)
    {
        if (parsers[i])
        {
            if (map_marker *m = parsers[i](s))
                return (m);
        }
    }
    return (NULL);
}

////////////////////////////////////////////////////////////////////////////
// map_feature_marker

map_feature_marker::map_feature_marker(
    const coord_def &p,
    dungeon_feature_type _feat)
    : map_marker(MAT_FEATURE, p), feat(_feat)
{
}

map_feature_marker::map_feature_marker(
    const map_feature_marker &other)
    : map_marker(MAT_FEATURE, other.pos), feat(other.feat)
{
}

void map_feature_marker::write(tagHeader &outf) const
{
    this->map_marker::write(outf);
    marshallShort(outf, feat);
}

void map_feature_marker::read(tagHeader &inf)
{
    map_marker::read(inf);
    feat = static_cast<dungeon_feature_type>(unmarshallShort(inf));
}

map_marker *map_feature_marker::read(tagHeader &inf, map_marker_type)
{
    map_marker *mapf = new map_feature_marker();
    mapf->read(inf);
    return (mapf);
}

map_marker *map_feature_marker::parse(const std::string &s) throw (std::string)
{
    if (s.find("feat:") != 0)
        return (NULL);
    std::string raw = s;
    strip_tag(raw, "feat:", true);

    const dungeon_feature_type ft = dungeon_feature_by_name(raw);
    if (ft == DNGN_UNSEEN)
        throw make_stringf("Bad feature marker: %s (unknown feature '%s')",
                           s.c_str(), raw.c_str());
    return new map_feature_marker(coord_def(0, 0), ft);
}

std::string map_feature_marker::describe() const
{
    return make_stringf("feature (%s)", dungeon_feature_name(feat));
}

////////////////////////////////////////////////////////////////////////////
// map_feature_marker

map_timed_feature_marker::map_timed_feature_marker(
    const coord_def &_pos,
    int duration_turns,
    dungeon_feature_type _feat)
    : map_feature_marker(_pos, _feat), duration_ticks(duration_turns * 10),
      warn_threshold(-1000)
{
    type = MAT_TIMED_FEATURE;
}

void map_timed_feature_marker::activate()
{
    dungeon_events.register_listener(DET_TURN_ELAPSED, this);

    if (player_can_hear(pos))
    {
        const dungeon_feature_type ft = grd(pos);
        switch (ft)
        {
        case DNGN_ENTER_BAZAAR:
            mprf(MSGCH_SOUND, "You %shear coins being counted.",
                 duration_ticks < 1000? "can faintly " : "");
            break;
        case DNGN_ENTER_LABYRINTH:
            mprf(MSGCH_SOUND, "You hear a faint echoing snort.");
            break;
        default:
            break;
        }
    }
}

void map_timed_feature_marker::write(tagHeader &th) const
{
    map_feature_marker::write(th);
    marshallShort(th, duration_ticks);
    marshallShort(th, warn_threshold);
}

void map_timed_feature_marker::read(tagHeader &th)
{
    map_feature_marker::read(th);
    duration_ticks = unmarshallShort(th);
    warn_threshold = unmarshallShort(th);
}

std::string map_timed_feature_marker::describe() const
{
    return make_stringf("timer: %d ticks (%s)",
                        duration_ticks, dungeon_feature_name(feat));
}

const char *map_timed_feature_marker::bell_urgency(int ticks) const
{
    if (ticks > 5000)
        return "stately ";
    else if (ticks > 4000)
        return "";
    else if (ticks > 2500)
        return "brisk ";
    else if (ticks > 1500)
        return "urgent ";
    else if (ticks > 0)
        return "frantic ";
    else
        return "last, dying notes of the ";
}

const char *map_timed_feature_marker::noise_maker(int ticks) const
{
    switch (grd(pos))
    {
    case DNGN_ENTER_LABYRINTH:
        return (ticks > 0? "tolling of a bell" : "bell");
    case DNGN_ENTER_BAZAAR:
        return (ticks > 0? "ticking of an ancient clock" : "clock");
    default:
        return (ticks > 0?
                "trickling of a stream filled with giant, killer bugs."
                : "stream");
    }
}

void map_timed_feature_marker::notify_dgn_event(const dgn_event &e)
{
    if (!e.elapsed_ticks || e.type != DET_TURN_ELAPSED)
        return;

    if (warn_threshold == -1000)
        warn_threshold = std::max(50, duration_ticks - 500);
    
    duration_ticks -= e.elapsed_ticks;

    if (duration_ticks < warn_threshold || duration_ticks <= 0)
    {
        if (duration_ticks > 900)
            warn_threshold = duration_ticks - 500;
        else
            warn_threshold = duration_ticks - 250;

        if (duration_ticks > 0 && player_can_hear(pos))
            mprf(MSGCH_SOUND, "You hear the %s%s.",
                 bell_urgency(duration_ticks),
                 noise_maker(duration_ticks));

        if (duration_ticks <= 0)
            timeout(true);
    }
}

void map_timed_feature_marker::timeout(bool verbose)
{
    if (verbose)
    {
        if (see_grid(pos))
            mprf("%s disappears!",
                 feature_description(grd(pos), NUM_TRAPS, false,
                                     DESC_CAP_THE, false).c_str());
        else
            mpr("The walls and floor vibrate strangely for a moment.");
    }

    // And it's gone forever.
    grd(pos) = feat;

    dungeon_terrain_changed(pos);

    // Stop listening for further ticks.
    dungeon_events.remove_listener(this);

    // Kill this marker.
    env_remove_marker(this);
}

map_marker *map_timed_feature_marker::read(tagHeader &th, map_marker_type)
{
    map_marker *mt = new map_timed_feature_marker();
    mt->read(th);
    return (mt);
}

map_marker *map_timed_feature_marker::parse(const std::string &s)
    throw (std::string)
{
    if (s.find("timer:") != 0)
        return (NULL);
    std::string raw = s;
    strip_tag(raw, "timer:", true);

    int navg = strip_number_tag(raw, "avg:");
    if (navg == TAG_UNFOUND)
        navg = 1;

    if (navg < 1 || navg > 20)
        throw make_stringf("Bad marker spec '%s' (avg out of bounds)",
                           s.c_str());

    dungeon_feature_type feat = DNGN_FLOOR;
    std::string fname = strip_tag_prefix(raw, "feat:");
    if (!fname.empty()
        && (feat = dungeon_feature_by_name(fname)) == DNGN_UNSEEN)
    {
        throw make_stringf("Bad feature name (%s) in marker spec '%s'",
                           fname.c_str(), s.c_str());
    }
    
    std::vector<std::string> limits = split_string("-", raw);
    const int nlims = limits.size();
    if (nlims < 1 || nlims > 2)
        throw make_stringf("Malformed turn range (%s) in marker '%s'",
                           raw.c_str(), s.c_str());

    const int low = atoi(limits[0].c_str());
    const int high = nlims == 1? low : atoi(limits[1].c_str());

    if (low == 0 || high < low)
        throw make_stringf("Malformed turn range (%s) in marker '%s'",
                           raw.c_str(), s.c_str());

    const int duration = low == high? low : random_range(low, high, navg);
    return new map_timed_feature_marker(coord_def(0, 0),
                                        duration, feat);
}

//////////////////////////////////////////////////////////////////////////
// Map markers in env.

void env_activate_markers()
{
    for (dgn_marker_map::iterator i = env.markers.begin();
         i != env.markers.end(); ++i)
    {
        i->second->activate();
    }
}

void env_add_marker(map_marker *marker)
{
    env.markers.insert(dgn_pos_marker(marker->pos, marker));
}

void env_remove_marker(map_marker *marker)
{
    std::pair<dgn_marker_map::iterator, dgn_marker_map::iterator>
        els = env.markers.equal_range(marker->pos);
    for (dgn_marker_map::iterator i = els.first; i != els.second; ++i)
    {
        if (i->second == marker)
        {
            env.markers.erase(i);
            break;
        }
    }
    delete marker;
}

void env_remove_markers_at(const coord_def &c,
                           map_marker_type type)
{
    std::pair<dgn_marker_map::iterator, dgn_marker_map::iterator>
        els = env.markers.equal_range(c);
    for (dgn_marker_map::iterator i = els.first; i != els.second; )
    {
        dgn_marker_map::iterator todel = i++;
        if (type == MAT_ANY || todel->second->get_type() == type)
        {
            delete todel->second;
            env.markers.erase(todel);
        }
    }
}

map_marker *env_find_marker(const coord_def &c, map_marker_type type)
{
    std::pair<dgn_marker_map::const_iterator, dgn_marker_map::const_iterator>
        els = env.markers.equal_range(c);
    for (dgn_marker_map::const_iterator i = els.first; i != els.second; ++i)
        if (type == MAT_ANY || i->second->get_type() == type)
            return (i->second);
    return (NULL);
}

std::vector<map_marker*> env_get_markers(const coord_def &c)
{
    std::pair<dgn_marker_map::const_iterator, dgn_marker_map::const_iterator>
        els = env.markers.equal_range(c);
    std::vector<map_marker*> rmarkers;
    for (dgn_marker_map::const_iterator i = els.first; i != els.second; ++i)
        rmarkers.push_back(i->second);
    return (rmarkers);
}

void env_clear_markers()
{
    for (dgn_marker_map::iterator i = env.markers.begin();
         i != env.markers.end(); ++i)
        delete i->second;
    env.markers.clear();
}
