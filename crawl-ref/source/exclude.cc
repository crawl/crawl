/**
 * @file
 * @brief Code related to travel exclusions.
**/

#include "AppHdr.h"

#include "exclude.h"

#include <algorithm>
#include <sstream>

#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "dgn-overview.h"
#include "english.h"
#include "env.h"
#include "hints.h"
#include "libutil.h"
#include "mon-util.h"
#include "options.h"
#include "stringutil.h"
#include "tags.h"
#include "terrain.h"
#include "tiles-build-specific.h"
#include "travel.h"
#include "view.h"

// defined in dgn-overview.cc
extern set<pair<string, level_id> > auto_unique_annotations;

static bool _mon_needs_auto_exclude(const monster* mon, bool sleepy = false)
{
    // These include the base monster's name in their name, but we don't
    // want things in the auto_exclude option to match them.
    if (mon->type == MONS_PILLAR_OF_SALT || mon->type == MONS_BLOCK_OF_ICE
        || mon->type == MONS_TEST_STATUE)
    {
        return false;
    }

    if (mon->is_stationary())
        return !sleepy;

    // Auto exclusion only makes sense if the monster is still asleep.
    return mon->asleep();
}

// Check whether a given monster is listed in the auto_exclude option.
static bool _need_auto_exclude(const monster* mon, bool sleepy = false)
{
    // This only works if the name is lowercased.
    string name = mon->name(DESC_BASENAME, mon->is_stationary()
                                           && testbits(mon->flags, MF_SEEN));
    lowercase(name);

    for (const text_pattern &pat : Options.auto_exclude)
    {
        if (pat.matches(name)
            && _mon_needs_auto_exclude(mon, sleepy)
            && (mon->attitude == ATT_HOSTILE))
        {
            return true;
        }
    }

    return false;
}

// Nightstalker reduces LOS, so reducing the maximum exclusion radius
// only makes sense. This is only possible because it's a permanent
// mutation; other sources of LOS reduction should not have this effect.
// Similarly, Barachim's LOS increase.
static int _get_full_exclusion_radius()
{
    // XXX: dedup with update_vision_range()!
    return LOS_DEFAULT_RANGE - you.get_mutation_level(MUT_NIGHTSTALKER)
                             + (you.species == SP_BARACHI ? 1 : 0);
}

/**
 * Adds auto-exclusions for any monsters in LOS that need them.
 */
void add_auto_excludes()
{
    if (!is_map_persistent() || !map_bounds(you.pos()))
        return;

    vector<monster*> mons;
    for (radius_iterator ri(you.pos(), LOS_DEFAULT); ri; ++ri)
    {
        monster *mon = monster_at(*ri);
        if (!mon)
            continue;
        // Something of a speed hack, but some vaults have a TON of plants.
        if (mon->type == MONS_PLANT)
            continue;
        if (_need_auto_exclude(mon) && !is_exclude_root(*ri))
        {
            int radius = _get_full_exclusion_radius();
            set_exclude(*ri, radius, true);
            mons.emplace_back(mon);
        }
    }

    if (mons.empty())
        return;

    mprf(MSGCH_WARN, "Marking area around %s as unsafe for travelling.",
            describe_monsters_condensed(mons).c_str());
    learned_something_new(HINT_AUTO_EXCLUSION);
}

travel_exclude::travel_exclude(const coord_def &p, int r,
                               bool autoexcl, string dsc, bool vaultexcl)
    : pos(p), radius(r),
      uptodate(false), autoex(autoexcl), desc(dsc), vault(vaultexcl)
{
    const monster* m = monster_at(p);
    if (m) {
        // Don't exclude past glass for stationary monsters.
        if (m->is_stationary())
            los = los_def(p, opc_fully_no_trans, circle_def(r, C_SQUARE));
        else
            los = los_def(p, opc_excl, circle_def(r, C_SQUARE));
    }
    else
        los = los_def(p, opc_excl, circle_def(r, C_SQUARE));
    set_los();
}

// For exclude_map[p] = foo;
travel_exclude::travel_exclude()
    : pos(-1, -1), radius(-1),
      los(coord_def(-1, -1), opc_excl, circle_def(-1, C_SQUARE)),
      uptodate(false), autoex(false), desc(""), vault(false)
{
}

extern exclude_set curr_excludes; // in travel.cc

void travel_exclude::set_los()
{
    uptodate = true;
    if (radius > 1)
    {
        // Radius might have been changed, and this is cheap.
        los.set_bounds(circle_def(radius, C_SQUARE));
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
        return p == pos;
    else if (radius == 1)
        return (p - pos).rdist() <= 1;
    else
        return los.see_cell(p);
}

bool travel_exclude::in_bounds(const coord_def &p) const
{
    return radius == 0 && p == pos
           || radius == 1 && (p - pos).rdist() <= 1
           || los.in_bounds(p);
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
    auto it = exclude_roots.find(p);

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
                              bool autoexcl, string desc,
                              bool vaultexcl)
{
    travel_exclude ex(p, radius, autoexcl, desc, vaultexcl);
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

    for (radius_iterator ri(ex.pos, ex.radius, C_SQUARE); ri; ++ri)
        if (ex.affects(*ri))
            exclude_points.insert(*ri);
}

void exclude_set::update_excluded_points(bool recompute_los)
{
    for (iterator it = exclude_roots.begin(); it != exclude_roots.end(); ++it)
    {
        travel_exclude &ex = it->second;
        if (!ex.uptodate)
        {
            recompute_excluded_points(recompute_los);
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
    return exclude_points.count(p);
}

bool exclude_set::is_exclude_root(const coord_def &p) const
{
    return exclude_roots.count(p);
}

travel_exclude* exclude_set::get_exclude_root(const coord_def &p)
{
    return map_find(exclude_roots, p);
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
    return exclude_roots.begin();
}

exclude_set::iterator exclude_set::end()
{
    return exclude_roots.end();
}

/////////////////////////////////////////////////////////////////////////

static void _mark_excludes_non_updated(const coord_def &p)
{
    for (auto &entry : curr_excludes)
    {
        travel_exclude &ex = entry.second;
        ex.uptodate = ex.uptodate && !ex.in_bounds(p);
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
void update_exclusion_los(vector<coord_def> changed)
{
    if (changed.empty())
        return;

    for (coord_def c : changed)
        _mark_excludes_non_updated(c);

    curr_excludes.update_excluded_points(true);
}

bool is_excluded(const coord_def &p, const exclude_set &exc)
{
    return exc.is_excluded(p);
}

bool is_exclude_root(const coord_def &p)
{
    return curr_excludes.get_exclude_root(p);
}

int get_exclusion_radius(const coord_def &p)
{
    if (travel_exclude *exc = curr_excludes.get_exclude_root(p))
    {
        if (!exc->radius)
            return 1;
        else
            return exc->radius;
    }
    return 0;
}

string get_exclusion_desc(const coord_def &p)
{
    if (travel_exclude *exc = curr_excludes.get_exclude_root(p))
        return exc->desc;
    return "";
}

#ifdef USE_TILE
// update Gmap for squares surrounding exclude centre
static void _tile_exclude_gmap_update(const coord_def &p)
{
    for (int x = -8; x <= 8; x++)
        for (int y = -8; y <= 8; y++)
        {
            const coord_def pc(p.x + x, p.y + y);
            tiles.update_minimap(pc);
        }
}
#endif

static void _exclude_update()
{
    set_level_exclusion_annotation(curr_excludes.get_exclusion_desc());
    travel_cache.update_excludes();
}

static void _exclude_update(const coord_def &p)
{
#ifdef USE_TILE
    _tile_exclude_gmap_update(p);
#else
    UNUSED(p);
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
    for (const auto &entry : curr_excludes)
        _tile_exclude_gmap_update(entry.second.pos);
#endif
}

void clear_excludes()
{
#ifdef USE_TILE
    // Tiles needs to update the minimap for each exclusion that is removed,
    // but the exclusions need to be removed first. Therefore, make a copy
    // of the current set of exclusions and iterate through the copy below.
    exclude_set excludes = curr_excludes;
#endif

    curr_excludes.clear();
    clear_level_exclusion_annotation();

#ifdef USE_TILE
    for (const auto &entry : excludes)
        _tile_exclude_gmap_update(entry.second.pos);
#endif

    _exclude_update();
}

static void _exclude_gate(const coord_def &p, bool del = false)
{
    set<coord_def> all_doors;
    find_connected_identical(p, all_doors, true);
    for (const auto &dc : all_doors)
    {
        if (del)
            del_exclude(dc);
        else
            set_exclude(dc, 0);
    }
}

// Cycles the radius of an exclusion, including "off" state;
// may start at 0 < radius < LOS_RADIUS, but won't cycle there.
void cycle_exclude_radius(const coord_def &p)
{
    if (travel_exclude *exc = curr_excludes.get_exclude_root(p))
    {
        if (feat_is_door(grd(p)) && env.map_knowledge(p).known())
        {
            _exclude_gate(p, exc->radius == 0);
            return;
        }
        if (exc->radius > 0)
            set_exclude(p, 0);
        else
            del_exclude(p);
    }
    else
        set_exclude(p, _get_full_exclusion_radius());
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
    if (!is_map_persistent())
        return;

    if (!in_bounds(p))
        return;

    if (travel_exclude *exc = curr_excludes.get_exclude_root(p))
    {
        if (exc->desc.empty() && defer_updates)
        {
            if (cloud_struct* cloud = cloud_at(p))
                exc->desc = cloud->cloud_name(true) + " cloud";
        }
        else if (exc->radius == radius)
            return;

        exc->radius   = radius;
        exc->uptodate = false;
        curr_excludes.recompute_excluded_points();
    }
    else
    {
        string desc = "";
        if (!defer_updates)
        {
            // Don't list a monster in the exclusion annotation if the
            // exclusion was triggered by e.g. the flamethrowers' lua check.
            const map_cell& cell = env.map_knowledge(p);
            if (cell.monster() != MONS_NO_MONSTER)
            {
                desc = mons_type_name(cell.monster(), DESC_PLAIN);
                if (cell.detected_monster())
                    desc += " (detected)";
            }
            else
            {
                // Maybe it's a door or staircase?
                const dungeon_feature_type feat = cell.feat();
                if (feat_is_door(feat))
                    desc = "door";
                else
                {
                    const command_type dir = feat_stair_direction(feat);
                    if (dir == CMD_GO_UPSTAIRS)
                        desc = "upstairs";
                    else if (dir == CMD_GO_DOWNSTAIRS)
                        desc = "downstairs";
                }
            }
        }
        else if (cloud_struct* cloud = cloud_at(p))
            desc = cloud->cloud_name(true) + " cloud";

        curr_excludes.add_exclude(p, radius, autoexcl, desc, vaultexcl);
    }

    if (!defer_updates)
        _exclude_update(p);
}

// If a cell that was placed automatically no longer contains the original
// monster (or it is invisible), or if the player is no longer vulnerable to a
// damaging cloud, then remove the exclusion.
void maybe_remove_autoexclusion(const coord_def &p)
{
    if (travel_exclude *exc = curr_excludes.get_exclude_root(p))
    {
        if (!exc->autoex)
            return;

        const monster* m = monster_at(p);
        // We don't want to remove excluded clouds, check exc desc
        // XXX: This conditional is a mess.
        string desc = exc->desc;
        bool cloudy_exc = ends_with(desc, "cloud");
        if ((!m || !you.can_see(*m)
                || !_need_auto_exclude(m)
                || mons_type_name(m->type, DESC_PLAIN) != desc)
            && !cloudy_exc)
        {
            del_exclude(p);
        }
        else if (cloudy_exc && you.cloud_immune())
            del_exclude(p);
    }
}

// Lists all exclusions on the current level.
string exclude_set::get_exclusion_desc()
{
    vector<string> desc;
    int count_other = 0;
    for (auto &entry : exclude_roots)
    {
        travel_exclude &ex = entry.second;

        // Don't count cloud exclusions.
        if (strstr(ex.desc.c_str(), "cloud"))
            continue;

        // Don't duplicate if there's already an annotation from unique monsters.
        auto ma = auto_unique_annotations.find(make_pair(ex.desc,
                                                         level_id::current()));
        if (ma != auto_unique_annotations.end())
            continue;

        if (!ex.desc.empty())
            desc.push_back(ex.desc);
        else
            count_other++;
    }

    if (desc.size() > 1)
    {
        // Combine identical descriptions.
        sort(desc.begin(), desc.end());
        vector<string> help = desc;
        desc.clear();
        string old_desc = "";
        int count = 1;
        for (unsigned int i = 0; i < help.size(); ++i)
        {
            string tmp = help[i];
            if (i == 0)
                old_desc = tmp;
            else
            {
                if (strcmp(tmp.c_str(), old_desc.c_str()) == 0)
                    count++;
                else
                {
                    if (count == 1)
                        desc.push_back(old_desc);
                    else
                    {
                        desc.push_back(make_stringf("%d %s",
                                       count, pluralise(old_desc).c_str()));
                        count = 1;
                    }
                    old_desc = tmp;
                }
            }
        }
        if (count == 1)
            desc.push_back(old_desc);
        else
        {
            desc.push_back(make_stringf("%d %s",
                           count, pluralise(old_desc).c_str()));
        }
    }

    if (count_other > 0)
    {
        desc.push_back(make_stringf("%d %sexclusion%s",
                                    count_other, desc.empty() ? "" : "more ",
                                    count_other > 1 ? "s" : ""));
    }
    else if (desc.empty())
        return "";

    string desc_str = "";
    if (desc.size() > 1 || count_other == 0)
    {
        desc_str += "exclusion";
        if (desc.size() > 1)
            desc_str += "s";
        desc_str += ": ";
    }
    return desc_str + comma_separated_line(desc.begin(), desc.end(),
                                           " and ", ", ");
}

void marshallExcludes(writer& outf, const exclude_set& excludes)
{
    marshallShort(outf, excludes.size());
    if (excludes.size())
    {
        for (const auto &entry : excludes)
        {
            const travel_exclude &ex = entry.second;
            marshallCoord(outf, ex.pos);
            marshallShort(outf, ex.radius);
            marshallBoolean(outf, ex.autoex);
            marshallString(outf, ex.desc);
            // XXX: marshall travel_exclude::vault?
        }
    }
}

void unmarshallExcludes(reader& inf, int minorVersion, exclude_set &excludes)
{
    UNUSED(minorVersion);
    excludes.clear();
    int nexcludes = unmarshallShort(inf);
    if (nexcludes)
    {
        for (int i = 0; i < nexcludes; ++i)
        {
            const coord_def c      = unmarshallCoord(inf);
            const int radius       = unmarshallShort(inf);
            const bool autoexcl    = unmarshallBoolean(inf);
            const string desc      = unmarshallString(inf);
            excludes.add_exclude(c, radius, autoexcl, desc);
        }
    }
}
