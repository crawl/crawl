/**
 * @file
 * @brief Tracking effects that affect areas for durations.
 *           Silence, sanctuary, halos, ...
**/

#include "AppHdr.h"

#include "areas.h"

#include "act-iter.h"
#include "artefact.h"
#include "art-enum.h"
#include "cloud.h"
#include "coordit.h"
#include "env.h"
#include "fprop.h"
#include "god-abil.h"
#include "god-conduct.h"
#include "god-passive.h" // passive_t::umbra
#include "libutil.h"
#include "losglobal.h"
#include "message.h"
#include "mon-behv.h"
#include "mutation.h"
#include "religion.h"
#include "stepdown.h"
#include "terrain.h"
#include "traps.h"
#include "travel.h"

/// Center of an area effect
struct area_centre
{
    area_type type;
    coord_def centre;
    int radius;

    explicit area_centre (area_type t, coord_def c, int r) : type(t), centre(c), radius(r) { }
};

typedef FixedArray<areaprops, GXM, GYM> propgrid_t;

/// The area center cache. Contains centers of all area effects.
static vector<area_centre> _agrid_centres;

static propgrid_t _agrid; ///< The area grid cache
/// \brief Is the area grid cache up-to-date?
/// \details If false, each check for area effects that affect a coordinate
/// would trigger an update of the area grid cache.
static bool _agrid_valid = false;
/// \brief If true, the level has no area effect
static bool no_areas = false;

static void _set_agrid_flag(const coord_def& p, area_type f)
{
    _agrid(p) |= f;
}

static bool _check_agrid_flag(const coord_def& p, area_type f)
{
    return bool(_agrid(p) & f);
}

/// \brief Invalidates the area effect cache
/// \details Invalidates the area effect cache, causing the next request for
/// area effects to re-calculate which locations are covered by halos, etc.
/// If \p force is false, the cache will only be invalidated if the level
/// had existing area effects.
void invalidate_agrid(bool force)
{
    _agrid_valid = false;
    if (force)
        no_areas = false;
}

void areas_actor_moved(const actor* act)
{
    if (act->alive() && act->affects_agrid())
    {
        // Not necessarily new, but certainly potentially interesting.
        invalidate_agrid();
    }
}

static void _apply_area(const coord_def& center, int radius, area_type type,
                        los_type los = LOS_DEFAULT,
                        bool exclude_center = false,
                        bool ground_only = false)
{
    if (radius <= 0)
        return;

    for (radius_iterator ri(center, radius, C_SQUARE, los, exclude_center); ri; ++ri)
    {
        // Leda's only applies on
        if (ground_only && !feat_has_dry_floor(env.grid(*ri)))
            continue;

        _set_agrid_flag(*ri, type);
    }

    _agrid_centres.emplace_back(type, center, radius);

    no_areas = false;
}

/// \brief Add some of the actor's area effects to the grid and center caches
/// \param actor The actor
/// \details Adds all non-player-specific actor area effects (e.g. silence) to
/// the area grid (\ref _agrid) and center (\ref _agrid_centres) caches.
/// Sets \ref no_areas to false if the actor generates those area effects.
static void _actor_areas(actor *a)
{
    _apply_area(a->pos(), a->silence_radius(), area_type::silence, LOS_NONE);
    _apply_area(a->pos(), a->halo_radius(), area_type::halo);
    _apply_area(a->pos(), a->umbra_radius(), area_type::umbra);
    _apply_area(a->pos(), a->liquefying_radius(), area_type::liquified, LOS_SOLID, false, true);
}

/**
 * Update the area grid cache.
 *
 * Updates the _agrid FixedArray of grid information flags using the
 * areaprop types.
 */
static void _update_agrid()
{
    // sanitize rng in case this gets indirectly called by the builder.
    rng::generator gameplay(rng::GAMEPLAY);

    if (no_areas)
    {
        _agrid_valid = true;
        return;
    }

    _agrid.init(areaprops());
    _agrid_centres.clear();

    no_areas = true;

    _actor_areas(&you);
    for (monster_iterator mi; mi; ++mi)
        _actor_areas(*mi);

    if ((player_has_orb() || you.unrand_equipped(UNRAND_CHARLATANS_ORB)))
        _apply_area(you.pos(), 2, area_type::orb);

    if (you.has_mutation(MUT_SILENCE_AURA))
        _apply_area(you.pos(), 1, area_type::silence, LOS_DEFAULT, true);

    if (you.duration[DUR_QUAD_DAMAGE])
        _apply_area(you.pos(), 2, area_type::quad);

    if (you.duration[DUR_DISJUNCTION])
        _apply_area(you.pos(), 4, area_type::disjunction);

    _agrid_valid = true;
}

coord_def find_centre_for(const coord_def& f, area_type at)
{
    if (!map_bounds(f))
        return coord_def(-1, -1);

    if (!_agrid_valid)
        _update_agrid();

    if (!_agrid(f))
        return coord_def(-1, -1);

    if (_agrid_centres.empty())
        return coord_def(-1, -1);

    coord_def possible = coord_def(-1, -1);
    int dist = 0;

    for (const area_centre &a : _agrid_centres)
    {
        if (a.type != at)
            continue;

        if (a.centre == f)
            return f;

        int d = grid_distance(a.centre, f);
        if (d <= a.radius && (d <= dist || dist == 0))
        {
            possible = a.centre;
            dist = d;
        }
    }

    return possible;
}

static bool _update_and_check_agrid(const coord_def& p, area_type type)
{
    if (!map_bounds(p))
        return false;
    if (!_agrid_valid)
        _update_agrid();
    return _check_agrid_flag(p, type);
}

///////////////
// Sanctuary

static void _remove_sanctuary_property(const coord_def& where)
{
    env.pgrid(where) &= ~(FPROP_SANCTUARY_1 | FPROP_SANCTUARY_2);
}

bool sanctuary_exists()
{
    return env.sanctuary_time > 0;
}

/*
 * Remove any sanctuary from the level.
 *
 * @returns True if we removed an existing sanctuary, false otherwise.
 */
bool remove_sanctuary()
{
    if (env.sanctuary_time)
        env.sanctuary_time = 0;

    if (!sanctuary_exists())
        return false;

    const int radius = 4;
    for (rectangle_iterator ri(env.sanctuary_pos, radius, true); ri; ++ri)
        if (is_sanctuary(*ri))
            _remove_sanctuary_property(*ri);

    env.sanctuary_pos.set(-1, -1);

    // Now that the sanctuary is gone, monsters aren't afraid of it
    // anymore.
    for (monster_iterator mi; mi; ++mi)
        mons_stop_fleeing_from_sanctuary(**mi);

    if (is_resting())
        stop_running();

    return true;
}

// For the last (radius) counter turns the sanctuary will slowly shrink.
void decrease_sanctuary_radius()
{
    const int radius = 4;

    // For the last (radius-1) turns 33% chance of not decreasing.
    if (env.sanctuary_time < radius && one_chance_in(3))
        return;

    int size = --env.sanctuary_time;
    if (size >= radius)
        return;

    if (you.running && is_sanctuary(you.pos()))
    {
        mprf(MSGCH_DURATION, "The sanctuary starts shrinking.");
        stop_running();
    }

    for (rectangle_iterator ri(env.sanctuary_pos, size+1, true); ri; ++ri)
    {
        int dist = grid_distance(*ri, env.sanctuary_pos);

        // If necessary overwrite sanctuary property.
        if (dist > size)
            _remove_sanctuary_property(*ri);
    }

    // Special case for time-out of sanctuary.
    if (!size)
    {
        // XX why doesn't this update env.sanctuary_pos to -1,-1?
        _remove_sanctuary_property(env.sanctuary_pos);
        if (you.see_cell(env.sanctuary_pos))
            mprf(MSGCH_DURATION, "The sanctuary disappears.");
    }
}

void create_sanctuary(const coord_def& center, int time)
{
    env.sanctuary_pos  = center;
    env.sanctuary_time = time;

    // radius could also be influenced by Inv
    // and would then have to be stored globally.
    const int radius      = 4;
    int       blood_count = 0;
    int       scare_count = 0;
    int       cloud_count = 0;
    monster* seen_mon    = nullptr;

    int shape = random2(4);
    for (radius_iterator ri(center, radius, C_SQUARE); ri; ++ri)
    {
        const coord_def pos = *ri;
        const int dist = grid_distance(center, pos);

        if (testbits(env.pgrid(pos), FPROP_BLOODY) && you.see_cell(pos))
            blood_count++;

        // forming patterns
        const int x = pos.x - center.x, y = pos.y - center.y;
        bool in_yellow = false;
        switch (shape)
        {
        case 0:                 // outward rays
            in_yellow = (x == 0 || y == 0 || x == y || x == -y);
            break;
        case 1:                 // circles
            in_yellow = (dist == radius
                         || dist == radius/2);
            break;
        case 2:                 // latticed
            in_yellow = (x%2 == 0 || y%2 == 0);
            break;
        case 3:                 // cross-like
            in_yellow = (abs(x)+abs(y) < 5 && x != y && x != -y);
            break;
        default:
            break;
        }

        env.pgrid(pos) |= (in_yellow ? FPROP_SANCTUARY_1
                                     : FPROP_SANCTUARY_2);

        env.pgrid(pos) &= ~(FPROP_BLOODY);

        if (env.grid(pos) == DNGN_FOUNTAIN_BLOOD
            || env.grid(pos) == DNGN_FOUNTAIN_EYES)
        {
            if (you.see_cell(pos))
                blood_count++;

            dungeon_terrain_changed(pos, DNGN_FOUNTAIN_BLUE);
        }

        // Scare all attacking monsters inside sanctuary, and make
        // all friendly monsters inside sanctuary stop attacking and
        // move towards the player.
        if (monster* mon = monster_at(pos))
        {
            if (mon->friendly())
            {
                mon->foe       = MHITYOU;
                mon->target    = center;
                mon->behaviour = BEH_SEEK;
                behaviour_event(mon, ME_EVAL, &you);
            }
            else if (!mon->wont_attack() && mons_is_influenced_by_sanctuary(*mon))
            {
                mons_start_fleeing_from_sanctuary(*mon);

                // Check to see that monster is actually fleeing.
                if (mons_is_fleeing(*mon) && you.can_see(*mon))
                {
                    scare_count++;
                    seen_mon = mon;
                }
            }
        }

        if (!is_harmless_cloud(cloud_type_at(pos)))
        {
            delete_cloud(pos);
            if (you.see_cell(pos))
                cloud_count++;
        }
    } // radius loop

    // Messaging.
    if (cloud_count == 1)
    {
        mprf(MSGCH_GOD, "By Zin's power, the foul cloud within the sanctuary "
                        "is swept away.");
    }
    else if (cloud_count > 1)
    {
        mprf(MSGCH_GOD, "By Zin's power, all foul fumes within the sanctuary "
                        "are swept away.");
    }

    if (blood_count > 0)
        mprf(MSGCH_GOD, "By Zin's power, all blood is cleared from the sanctuary.");

    if (scare_count == 1 && seen_mon != nullptr)
        simple_monster_message(*seen_mon, " turns to flee the light!");
    else if (scare_count > 0)
        mpr("The monsters scatter in all directions!");
}

// Range calculation for spells whose radius shrinks over time with remaining
// duration.
// dur starts at 10 (low power) and is capped at 100
// maximal range: 5
// last 6 turns: range 0, hence only the player affected
int shrinking_aoe_range(int dur)
{
    if (dur <= 0)
        return -1;
    dur /= BASELINE_DELAY; // now roughly number of turns
    return isqrt(max(0, min(3*(dur - 5)/4, 25)));
}

/////////////
// Silence

int player::silence_radius() const
{
    return shrinking_aoe_range(duration[DUR_SILENCE]);
}

int monster::silence_radius() const
{
    if (type == MONS_SILENT_SPECTRE)
        return 10;

    if (!has_ench(ENCH_SILENCE))
        return -1;

    const int dur = get_ench(ENCH_SILENCE).duration;
    // The below is arbitrarily chosen to make monster decay look reasonable.
    const int moddur = BASELINE_DELAY
                       * max(7, stepdown_value(dur * 10 - 60, 10, 5, 45, 100));
    return shrinking_aoe_range(moddur);
}

/// Check if a coordinate is silenced
bool silenced(const coord_def& p)
{
    return _update_and_check_agrid(p, area_type::silence);
}

/////////////
// Halos

bool haloed(const coord_def& p)
{
    return _update_and_check_agrid(p, area_type::halo);
}

bool actor::haloed() const
{
    return ::haloed(pos());
}

int player::halo_radius() const
{
    int size = -1;

    if (have_passive(passive_t::halo))
    {
        // The cap is reached at piety 160 = ******.
        size = min(piety(), piety_breakpoint(5)) * you.normal_vision
                                                    / piety_breakpoint(5);
    }

    if (you.unrand_equipped(UNRAND_EOS))
        size = max(size, 3);
    else if (wearing_ego(OBJ_ARMOUR, SPARM_LIGHT))
        size = max(size, 3);
    else if (you.props.exists(WU_JIAN_HEAVENLY_STORM_KEY))
        size = max(size, 2);
    if (you.unrand_equipped(UNRAND_VAINGLORY))
        size = max(size, 0);

    return size;
}

static int _mons_class_halo_radius(monster_type type)
{
    // The values here depend on 1. power, 2. sentience. Thus, high-ranked
    // sentient celestials have really big haloes, while holy animals get
    // little or none.
    switch (type)
    {
    case MONS_ANGEL:
        return 4;
    case MONS_CHERUB:
        return 4;
    case MONS_DAEVA:
        return 4;
    case MONS_FRAVASHI:
        return 4;
    case MONS_OPHAN:
        return 6;
    case MONS_SERAPH:
        return 7; // highest rank among sentient ones
    case MONS_HOLY_SWINE:
        return 1;  // only notionally holy
    case MONS_MENNAS:
        return 2;  // ???  Low on grace or what?
    default:
        return -1;
    }
}

int monster::halo_radius() const
{
    int size = -1;

    item_def* wpn = mslot_item(MSLOT_WEAPON);
    if (wpn && is_unrandom_artefact(*wpn, UNRAND_EOS))
        size = max(size, 3);

    item_def* alt_wpn = mslot_item(MSLOT_ALT_WEAPON);
    if (mons_wields_two_weapons(*this) && alt_wpn
        && is_unrandom_artefact(*alt_wpn, UNRAND_EOS))
    {
        size = max(size, 3);
    }

    if (wearing_ego(OBJ_ARMOUR, SPARM_LIGHT))
        size = max(size, 3);

    if (!(holiness() & MH_HOLY))
        return size;

    return _mons_class_halo_radius(type);
}

//////////////////////
// Leda's Liquefaction
//

int player::liquefying_radius() const
{
    return shrinking_aoe_range(duration[DUR_LIQUEFYING]);
}

int monster::liquefying_radius() const
{
    if (!has_ench(ENCH_LIQUEFYING))
        return -1;
    const int dur = get_ench(ENCH_LIQUEFYING).duration;
    // The below is arbitrarily chosen to make monster decay look reasonable.
    const int moddur = BASELINE_DELAY *
        max(7, stepdown_value(dur * 10 - 60, 10, 5, 45, 100));
    return shrinking_aoe_range(moddur);
}

bool liquefied(const coord_def& p, bool ledas_only)
{
    if (!map_bounds(p))
        return false;

    if (env.grid(p) == DNGN_MUD && !ledas_only)
        return true;

    if (!_agrid_valid)
        _update_agrid();

    if (feat_is_water(env.grid(p)) || feat_is_lava(env.grid(p)))
        return false;

    return _check_agrid_flag(p, area_type::liquified);
}

/////////////
// Orb's glow
//

bool orb_haloed(const coord_def& p)
{
    return _update_and_check_agrid(p, area_type::orb);
}

/////////////
// Quad damage glow
//

bool quad_haloed(const coord_def& p)
{
    return _update_and_check_agrid(p, area_type::quad);
}

/////////////
// Disjunction Glow
//

bool disjunction_haloed(const coord_def& p)
{
    return _update_and_check_agrid(p, area_type::disjunction);
}

/////////////
// Umbra
//

bool umbraed(const coord_def& p)
{
    return _update_and_check_agrid(p, area_type::umbra);
}

// Whether actor is in an umbra.
bool actor::umbraed() const
{
    return ::umbraed(pos());
}

int player::umbra_radius() const
{
    int size = -1;

    if (have_passive(passive_t::umbra))
    {
        if (piety() >= piety_breakpoint(4))
            size = 4;
        else if (piety() >= piety_breakpoint(3))
            size = 3;
        else
            size = 2;
    }

    if (you.has_mutation(MUT_FOUL_SHADOW))
        size = max(size, you.get_mutation_level(MUT_FOUL_SHADOW));

    if ((you.unrand_equipped(UNRAND_BRILLIANCE))
         || you.unrand_equipped(UNRAND_SHADOWS))
    {
        size = max(size, 3);
    }

    return size;
}

int monster::umbra_radius() const
{
    int size = -1;

    if (mons_is_ghost_demon(type))
        size = ghost_umbra_radius();

    item_def* wpn = mslot_item(MSLOT_WEAPON);
    if (wpn && is_unrandom_artefact(*wpn, UNRAND_BRILLIANCE))
        size = max(size, 3);

    item_def* alt_wpn = mslot_item(MSLOT_ALT_WEAPON);
    if (mons_wields_two_weapons(*this) && alt_wpn
        && is_unrandom_artefact(*alt_wpn, UNRAND_BRILLIANCE))
    {
        size = max(size, 3);
    }

    item_def* ring = mslot_item(MSLOT_JEWELLERY);
    if (ring && is_unrandom_artefact(*ring, UNRAND_SHADOWS))
        size = max(size, 3);

    // Death knights get a small umbra.
    if (type == MONS_DEATH_KNIGHT)
        size += 3;

    if (!(holiness() & MH_UNDEAD))
        return size;

    // Bound holies get an umbra.
    if (type == MONS_BOUND_SOUL)
        return _mons_class_halo_radius(base_monster);

    switch (type)
    {
    case MONS_PROFANE_SERVITOR:
        return 5; // Very unholy!
    default:
        return -1;
    }
}

bool monster::affects_agrid() const
{
    return halo_radius() > -1
           || silence_radius() > -1
           || liquefying_radius() > -1
           || umbra_radius() > -1;
}

bool player::affects_agrid() const
{
    return halo_radius() > -1
           || silence_radius() > -1
           || liquefying_radius() > -1
           || umbra_radius() > -1
           || has_mutation(MUT_SILENCE_AURA)
           || duration[DUR_DISJUNCTION]
           || duration[DUR_QUAD_DAMAGE]
           || player_has_orb()
           || unrand_equipped(UNRAND_CHARLATANS_ORB);
}
