/*
 * File:      exclude.cc
 * Summary:   Code related to travel exclusions.
 */

#include "AppHdr.h"

#include "exclude.h"

#include "coord.h"
#include "coordit.h"
#include "map_knowledge.h"
#include "mon-util.h"
#include "options.h"
#include "overmap.h"
#include "stuff.h"
#include "env.h"
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
    return (mon->asleep());
}

// Check whether a given monster is listed in the auto_exclude option.
bool need_auto_exclude(const monsters *mon, bool sleepy)
{
    // This only works if the name is lowercased.
    std::string name = mon->name(DESC_BASENAME,
                                 mons_is_stationary(mon)
                                     && testbits(mon->flags, MF_SEEN));
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
        // FIXME: If this happens for several monsters in the same turn
        //        (as is possible for some vaults), this could be really
        //        annoying. (jpeg)
        mprf(MSGCH_WARN,
             "Marking area around %s as unsafe for travelling.",
             mon->name(DESC_NOCAP_THE).c_str());

#ifdef USE_TILE
        viewwindow(false);
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
        viewwindow(false);
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
        else if (env.map_knowledge(p).object.cls == SH_FEATURE)
            return _feat_opacity(env.map_knowledge(p).object.feat);
        else
        {
            // If you have seen monsters, items or clouds there,
            // it must have been passable.
            return OPC_CLEAR;
        }
    }
};
static opacity_excl opc_excl;

// Note: circle_def(r, C_ROUND) gives a circle with square radius r*r+1;
// this doesn't work well for radius 0, but then we want to
// skip LOS calculation in that case anyway since it doesn't
// currently short-cut for small bounds. So radius 0, 1 are special-cased.
travel_exclude::travel_exclude(const coord_def &p, int r,
                               bool autoexcl, monster_type mons, bool vaultexcl)
    : pos(p), radius(r),
      los(los_def(p, opc_excl, circle_def(r, C_ROUND))),
      uptodate(false), autoex(autoexcl), mon(mons), vault(vaultexcl)
{
    set_los();
}

// For exclude_map[p] = foo;
travel_exclude::travel_exclude()
    : pos(-1, -1), radius(-1),
      los(coord_def(-1, -1), opc_excl, circle_def(-1, C_ROUND)),
      uptodate(false), autoex(false), mon(MONS_NO_MONSTER), vault(false)
{
}

extern exclude_set curr_excludes; // in travel.cc

void travel_exclude::set_los()
{
    uptodate = true;
    if (radius > 1)
    {
        // Radius might have been changed, and this is cheap.
        los.set_bounds(circle_def(radius, C_ROUND));
        los.update();
    }
}

bool travel_exclude::affects(const coord_def& p) const
{
    if (!uptodate)
    {
        mprf(MSGCH_ERROR, "exclusion not up-to-date: e (%d,%d) p (%d,%d)",
             pos.x, pos.y, p.x, p.y);
    }
    if (radius == 0)
        return (p == pos);
    else if (radius == 1)
        return ((p - pos).rdist() <= 1);
    else
        return (los.see_cell(p));
}

bool travel_exclude::in_bounds(const coord_def &p) const
{
    return (radius == 0 && p == pos
            || radius == 1 && (p - pos).rdist() <= 1
            || los.in_bounds(p));
}

/////////////////////////////////////////////////////////////////////////

exclude_set::exclude_set()
{
}

void exclude_set::clear()
{
    exclude_roots.clear();
    exclude_points.clear();
}

void exclude_set::erase(const coord_def &p)
{
    exclude_set::iterator it = exclude_roots.find(p);

    if (it == exclude_roots.end())
        return;

    travel_exclude old_ex = it->second;
    exclude_roots.erase(it);

    recompute_excluded_points();
}

void exclude_set::add_exclude(travel_exclude &ex)
{
    add_exclude_points(ex);
    exclude_roots[ex.pos] = ex;
}

void exclude_set::add_exclude(const coord_def &p, int radius,
                              bool autoexcl, monster_type mons,
                              bool vaultexcl)
{
    travel_exclude ex(p, radius, autoexcl, mons, vaultexcl);
    add_exclude(ex);
}

void exclude_set::add_exclude_points(travel_exclude& ex)
{
    if (ex.radius == 0)
    {
        exclude_points.insert(ex.pos);
        return;
    }

    if (!ex.uptodate)
        ex.set_los();
    else
        ex.los.update();

    for (radius_iterator ri(ex.pos, ex.radius, C_ROUND); ri; ++ri)
        if (ex.affects(*ri))
            exclude_points.insert(*ri);
}

void exclude_set::update_excluded_points()
{
    for (iterator it = exclude_roots.begin(); it != exclude_roots.end(); ++it)
    {
        travel_exclude &ex = it->second;
        if (!ex.uptodate)
        {
            recompute_excluded_points();
            return;
        }
    }
}

void exclude_set::recompute_excluded_points(bool recompute_los)
{
    exclude_points.clear();
    for (iterator it = exclude_roots.begin(); it != exclude_roots.end(); ++it)
    {
        travel_exclude &ex = it->second;
        if (recompute_los)
            ex.set_los();
        add_exclude_points(ex);
    }
}

bool exclude_set::is_excluded(const coord_def &p) const
{
    return (exclude_points.find(p) != exclude_points.end());
}

bool exclude_set::is_exclude_root(const coord_def &p) const
{
    return (exclude_roots.find(p) != exclude_roots.end());
}

travel_exclude* exclude_set::get_exclude_root(const coord_def &p)
{
    exclude_set::iterator it = exclude_roots.find(p);

    if (it != exclude_roots.end())
        return (&(it->second));

    return  (false);
}

size_t exclude_set::size() const
{
    return exclude_roots.size();
}

bool exclude_set::empty() const
{
    return exclude_roots.empty();
}

exclude_set::const_iterator exclude_set::begin() const
{
    return exclude_roots.begin();
}

exclude_set::const_iterator exclude_set::end() const
{
    return exclude_roots.end();
}

exclude_set::iterator exclude_set::begin()
{
    return exclude_roots.end();
}

exclude_set::iterator exclude_set::end()
{
    return exclude_roots.end();
}

/////////////////////////////////////////////////////////////////////////

void _mark_excludes_non_updated(const coord_def &p)
{
    for (exclude_set::iterator it = curr_excludes.begin();
         it != curr_excludes.end(); ++it)
    {
        travel_exclude &ex = it->second;
        ex.uptodate = ex.uptodate && ex.in_bounds(p);
    }
}

void init_exclusion_los()
{
    curr_excludes.recompute_excluded_points(true);
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

    curr_excludes.update_excluded_points();
}

bool is_excluded(const coord_def &p, const exclude_set &exc)
{
    return exc.is_excluded(p);
}

bool is_exclude_root(const coord_def &p)
{
    return (curr_excludes.get_exclude_root(p));
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
    set_level_exclusion_annotation(curr_excludes.get_exclusion_desc());
}

static void _exclude_update(const coord_def &p)
{
#ifdef USE_TILE
    _tile_exclude_gmap_update(p);
#endif
    _exclude_update();
}

// Catch up exclude updates from set_exclude with defer_updates=true.
//
// Warning: For tiles, this assumes all changed exclude centres
//          are still there, so this won't work as is for
//          del_exclude.
void deferred_exclude_update()
{
    _exclude_update();
#ifdef USE_TILE
    exclude_set::iterator it;
    for (it = curr_excludes.begin(); it != curr_excludes.end(); ++it)
        _tile_exclude_gmap_update(it->second.pos);
#endif
}

void clear_excludes()
{
    // Sanity checks
    if (!player_in_mappable_area())
        return;

#ifdef USE_TILE
    exclude_set::iterator it;
    //for (int i = curr_excludes.size()-1; i >= 0; i--)
    for (it = curr_excludes.begin(); it != curr_excludes.end(); ++it)
        _tile_exclude_gmap_update(it->second.pos);
#endif

    curr_excludes.clear();
    clear_level_exclusion_annotation();

    _exclude_update();
}

// Cycles the radius of an exclusion, including "off" state;
// may start at 0 < radius < LOS_RADIUS, but won't cycle there.
void cycle_exclude_radius(const coord_def &p)
{
    if (travel_exclude *exc = curr_excludes.get_exclude_root(p))
    {
        if (exc->radius == LOS_RADIUS)
            set_exclude(p, 0);
        else
            del_exclude(p);
    }
    else
        set_exclude(p, LOS_RADIUS);
}

// Remove a possible exclude.
void del_exclude(const coord_def &p)
{
    curr_excludes.erase(p);
    _exclude_update(p);
}

// Set or update an exclude.
void set_exclude(const coord_def &p, int radius, bool autoexcl, bool vaultexcl,
                 bool defer_updates)
{
    // Sanity checks; excludes can be set in Pan and regular dungeon
    // levels only.
    if (!player_in_mappable_area())
        return;

    if (!in_bounds(p))
        return;

    if (travel_exclude *exc = curr_excludes.get_exclude_root(p))
    {
        if (exc->radius == radius)
            return;

        exc->radius   = radius;
        exc->uptodate = false;
        curr_excludes.recompute_excluded_points();
    }
    else
    {
        monster_type montype = MONS_NO_MONSTER;
        if (!defer_updates)
        {
            // Don't list a monster in the exclusion annotation if the
            // exclusion was triggered by e.g. the flamethrowers' lua check.
            const monsters *m = monster_at(p);
            if (m && (you.can_see(m) || mons_is_stationary(m)
                                        && testbits(m->flags, MF_SEEN)))
            {
                montype = m->type;
            }
        }

        curr_excludes.add_exclude(p, radius, autoexcl, montype, vaultexcl);
    }

    if (!defer_updates)
        _exclude_update(p);
}

// If a cell that was placed automatically no longer contains the original
// monster (or it is invisible), remove the exclusion.
void maybe_remove_autoexclusion(const coord_def &p)
{
    if (travel_exclude *exc = curr_excludes.get_exclude_root(p))
    {
        const monsters *m = monster_at(p);
        if (exc->autoex && (!m || !you.can_see(m) || m->type != exc->mon
                            || m->attitude != ATT_HOSTILE))
        {
            del_exclude(p);
        }
    }
}

// Lists all exclusions on the current level.
std::string exclude_set::get_exclusion_desc()
{
    std::vector<std::string> monsters;
    int count_other = 0;
    for (exclmap::iterator it = exclude_roots.begin();
         it != exclude_roots.end(); ++it)
    {
        travel_exclude &ex = it->second;
        if (!invalid_monster_type(ex.mon))
            monsters.push_back(mons_type_name(ex.mon, DESC_PLAIN));
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


void marshallExcludes(writer& outf, const exclude_set& excludes)
{
    marshallShort(outf, excludes.size());
    exclude_set::const_iterator it;
    if (excludes.size())
    {
        for (it = excludes.begin(); it != excludes.end(); ++it)
        {
            const travel_exclude &ex = it->second;
            marshallCoord(outf, ex.pos);
            marshallShort(outf, ex.radius);
            marshallBoolean(outf, ex.autoex);
            marshallShort(outf, ex.mon);
            // XXX: marshall travel_exclude::vault?
        }
    }
}

void unmarshallExcludes(reader& inf, char minorVersion, exclude_set &excludes)
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
            autoexcl         = unmarshallBoolean(inf);
            mon              = static_cast<monster_type>(unmarshallShort(inf));

            excludes.add_exclude(c, radius, autoexcl, mon);
        }
    }
}
