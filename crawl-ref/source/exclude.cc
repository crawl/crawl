/*
 * File:      exclude.cc
 * Summary:   Code related to travel exclusions.
 */

#include "AppHdr.h"

#include "exclude.h"

#include "mon-util.h"
#include "overmap.h"
#include "stuff.h"
#include "tags.h"
#include "terrain.h"
#include "travel.h"
#include "tutorial.h"
#include "view.h"

static bool _mon_needs_auto_exclude(const monsters *mon, bool sleepy = false)
{
    if (mons_is_stationary(mon))
    {
        if (sleepy)
            return (false);

        // Don't give away mimics unless already known.
        return (!mons_is_mimic(mon->type)
                || testbits(mon->flags, MF_KNOWN_MIMIC));
    }
    // Auto exclusion only makes sense if the monster is still asleep.
    return (mons_is_sleeping(mon));
}

// Check whether a given monster is listed in the auto_exclude option.
bool need_auto_exclude(const monsters *mon, bool sleepy)
{
    // This only works if the name is lowercased.
    std::string name = mon->name(DESC_BASENAME);
    lowercase(name);

    for (unsigned i = 0; i < Options.auto_exclude.size(); ++i)
        if (Options.auto_exclude[i].matches(name)
            && _mon_needs_auto_exclude(mon, sleepy)
            && mon->attitude == ATT_HOSTILE)
        {
            return (true);
        }

    return (false);
}

// If the monster is in the auto_exclude list, automatically set an
// exclusion.
void set_auto_exclude(const monsters *mon)
{
    if (need_auto_exclude(mon) && !is_exclude_root(mon->pos()))
    {
        set_exclude(mon->pos(), LOS_RADIUS, true);
#ifdef USE_TILE
        viewwindow(true, false);
#endif
        learned_something_new(TUT_AUTO_EXCLUSION, mon->pos());
    }
}
        
// Clear auto exclusion if the monster is killed or wakes up with the
// player in sight. If sleepy is true, stationary monsters are ignored.
void remove_auto_exclude(const monsters *mon, bool sleepy)
{
    if (need_auto_exclude(mon, sleepy))
    {
        del_exclude(mon->pos());
#ifdef USE_TILE
        viewwindow(true, false);
#endif
    }
}

opacity_type _feat_opacity(dungeon_feature_type feat)
{
    return (feat_is_opaque(feat) ? OPC_OPAQUE : OPC_CLEAR);
}

// A cell is considered clear unless the player knows it's
// opaque.
struct opacity_excl : opacity_func
{
    CLONE(opacity_excl)

    opacity_type operator()(const coord_def& p) const
    {
        if (!is_terrain_seen(p))
            return OPC_CLEAR;
        else if (!is_terrain_changed(p))
            return _feat_opacity(env.grid(p));
        else if (env.map(p).object < NUM_REAL_FEATURES)
            return _feat_opacity((dungeon_feature_type) env.map(p).object);
        else
        {
            // If you have seen monsters, items or clouds there,
            // it must have been passable.
            return OPC_CLEAR;
        }
    }
};
static opacity_excl opc_excl;

// Note: bounds_radius gives a circle with square radius r*r+1;
// this doesn't work well for radius 0, but then we want to
// skip LOS calculation in that case anyway since it doesn't
// currently short-cut for small bounds. So radius 0 is special-cased.

travel_exclude::travel_exclude(const coord_def &p, int r,
                               bool autoexcl, monster_type mons, bool vaultexcl)
: pos(p), radius(r),
los(los_def(p, opc_excl, bounds_radius(r))),
uptodate(false), autoex(autoex), mon(mons), vault(vaultexcl)
{
    set_los();
}

void travel_exclude::set_los()
{
    uptodate = true;
    if (radius > 0)
    {
        // Radius might have been changed, and this is cheap.
        los.set_bounds(bounds_radius(radius));
        los.update();
    }
}

bool travel_exclude::affects(const coord_def& p) const
{
    if (!uptodate)
        mprf(MSGCH_ERROR, "exclusion not up-to-date: e (%d,%d) p (%d,%d)",
             pos.x, pos.y, p.x, p.y);
    if (radius == 0)
        return (p == pos);
    return (los.see_cell(p));
}

bool travel_exclude::in_bounds(const coord_def &p) const
{
    return (radius == 0 && p == pos
            || los.in_bounds(p));
}

void _mark_excludes_non_updated(const coord_def &p)
{
    for (exclvec::iterator it = curr_excludes.begin();
         it != curr_excludes.end(); ++it)
    {
        it->uptodate = it->uptodate && it->in_bounds(p);
    }
}

void _update_exclusion_los(bool all=false)
{
    for (exclvec::iterator it = curr_excludes.begin();
         it != curr_excludes.end(); ++it)
    {
        if (all || !it->uptodate)
            it->set_los();
    }
}

void init_exclusion_los()
{
    _update_exclusion_los(true);
}

/*
 * Update exclusions' LOS to reflect changes within their range.
 * "changed" is a list of coordinates that have been changed.
 * Only exclusions that might have one of the changed points
 * in view are updated.
 */
void update_exclusion_los(std::vector<coord_def> changed)
{
    if (changed.empty())
        return;

    for (unsigned int i = 0; i < changed.size(); ++i)
        _mark_excludes_non_updated(changed[i]);
    _update_exclusion_los();
}

bool is_excluded(const coord_def &p, const exclvec &exc)
{
    for (unsigned int i = 0; i < exc.size(); ++i)
        if (exc[i].affects(p))
            return (true);
    return (false);
}

static travel_exclude *_find_exclude_root(const coord_def &p)
{
    for (unsigned int i = 0; i < curr_excludes.size(); ++i)
        if (curr_excludes[i].pos == p)
            return (&curr_excludes[i]);
    return (NULL);
}

bool is_exclude_root(const coord_def &p)
{
    return (_find_exclude_root(p));
}

#ifdef USE_TILE
// update Gmap for squares surrounding exclude centre
static void _tile_exclude_gmap_update(const coord_def &p)
{
    for (int x = -8; x <= 8; x++)
        for (int y = -8; y <= 8; y++)
        {
            int px = p.x+x, py = p.y+y;
            if (in_bounds(coord_def(px,py)))
            {
                tiles.update_minimap(px, py);
            }
        }
}
#endif

static void _exclude_update()
{
    if (can_travel_interlevel())
    {
        LevelInfo &li = travel_cache.get_level_info(level_id::current());
        li.update();
    }
    set_level_exclusion_annotation(get_exclusion_desc());
}

static void _exclude_update(const coord_def &p)
{
#ifdef USE_TILE
    _tile_exclude_gmap_update(p);
#endif
    _exclude_update();
}

void clear_excludes()
{
    // Sanity checks
    if (!player_in_mappable_area())
        return;

#ifdef USE_TILE
    for (int i = curr_excludes.size()-1; i >= 0; i--)
        _tile_exclude_gmap_update(curr_excludes[i].pos);
#endif

    curr_excludes.clear();
    clear_level_exclusion_annotation();

    _exclude_update();
}

// Cycles the radius of an exclusion, including "off" state.
void cycle_exclude_radius(const coord_def &p)
{
    // XXX: scanning through curr_excludes twice
    if (travel_exclude *exc = _find_exclude_root(p))
    {
        if (exc->radius == LOS_RADIUS)
            set_exclude(p, 0);
        else
        {
            ASSERT(exc->radius == 0);
            del_exclude(p);
        }
    }
    else
        set_exclude(p, LOS_RADIUS);
}

// Remove a possible exclude.
void del_exclude(const coord_def &p)
{
    for (unsigned int i = 0; i < curr_excludes.size(); ++i)
        if (curr_excludes[i].pos == p)
        {
            curr_excludes.erase(curr_excludes.begin() + i);
            break;
        }
    _exclude_update(p);
}

// Set or update an exclude.
void set_exclude(const coord_def &p, int radius, bool autoexcl, bool vaultexcl)
{
    // Sanity checks; excludes can be set in Pan and regular dungeon
    // levels only.
    if (!player_in_mappable_area())
        return;

    if (!in_bounds(p))
        return;

    if (travel_exclude *exc = _find_exclude_root(p))
    {
        exc->radius = radius;
        exc->set_los();
    }
    else
    {
        monster_type montype = MONS_NO_MONSTER;
        const monsters *m = monster_at(p);
        if (m && you.can_see(m))
            montype = m->type;

        curr_excludes.push_back(travel_exclude(p, radius, autoexcl,
                                               montype, vaultexcl));
    }

    _exclude_update(p);
}

// If a grid that was placed automatically no longer contains the original
// monster (or it is invisible), remove the exclusion.
void maybe_remove_autoexclusion(const coord_def &p)
{
    if (travel_exclude *exc = _find_exclude_root(p))
    {
        const monsters *m = monster_at(p);
        if (exc->autoex && (!m || !you.can_see(m) || m->type != exc->mon))
            del_exclude(p);
    }
}

// Lists all exclusions on the current level.
std::string get_exclusion_desc()
{
    std::vector<std::string> monsters;
    int count_other = 0;
    for (unsigned int i = 0; i < curr_excludes.size(); ++i)
    {
        if (!invalid_monster_type(curr_excludes[i].mon))
            monsters.push_back(mons_type_name(curr_excludes[i].mon, DESC_PLAIN));
        else
            count_other++;
    }

    if (count_other > 0)
    {
        snprintf(info, INFO_SIZE, "%d %sexclusion%s",
                 count_other, monsters.empty() ? "" : "more ",
                 count_other > 1 ? "s" : "");
        monsters.push_back(info);
    }
    else if (monsters.empty())
        return "";

    std::string desc = "";
    if (monsters.size() > 1 || count_other == 0)
    {
        snprintf(info, INFO_SIZE, "exclusion%s: ",
                 monsters.size() > 1 ? "s" : "");
        desc += info;
    }
    return (desc + comma_separated_line(monsters.begin(), monsters.end(),
                                        ", and ", ", "));
}


void marshallExcludes(writer& outf, const exclvec& excludes)
{
    marshallShort(outf, excludes.size());
    if (excludes.size())
    {
        for (int i = 0, count = excludes.size(); i < count; ++i)
        {
            marshallCoord(outf, excludes[i].pos);
            marshallShort(outf, excludes[i].radius);
            marshallBoolean(outf, excludes[i].autoex);
            marshallShort(outf, excludes[i].mon);
            // XXX: marshall travel_exclude::vault?
        }
    }
}

void unmarshallExcludes(reader& inf, char minorVersion, exclvec &excludes)
{ 
    excludes.clear();
    int nexcludes = unmarshallShort(inf);
    if (nexcludes)
    {
        for (int i = 0; i < nexcludes; ++i)
        {
            coord_def c;
            unmarshallCoord(inf, c);
            const int radius = unmarshallShort(inf);
            bool autoexcl    = false;
            monster_type mon = MONS_NO_MONSTER;
            if (minorVersion >= TAG_ANNOTATE_EXCL)
            {
                autoexcl = unmarshallBoolean(inf);
                mon      = static_cast<monster_type>(unmarshallShort(inf));
            }
            excludes.push_back(travel_exclude(c, radius, autoexcl, mon));
        }
    }
}
