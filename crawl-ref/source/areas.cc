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

/// Bitmasks for area properties that center on actors
enum class areaprop
{
    // 0 and 1 were sanctuary, now unused
    silence       = (1 << 2),
    halo          = (1 << 3),
    liquified     = (1 << 4),
    // (1 << 5) was actual_liquid, now unused
    orb           = (1 << 6), ///< The glow of the Orb of Zot
    umbra         = (1 << 7),
    quad          = (1 << 8),
    disjunction   = (1 << 9),
    // 10 was lost soul aura, now unused
};
/// Bit field for the area properties
DEF_BITFIELD(areaprops, areaprop);

/// Center of an area effect
struct area_centre
{
    area_centre_type type;
    coord_def centre;
    int radius;

    explicit area_centre (area_centre_type t, coord_def c, int r) : type(t), centre(c), radius(r) { }
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

static void _set_agrid_flag(const coord_def& p, areaprop f)
{
    _agrid(p) |= f;
}

static bool _check_agrid_flag(const coord_def& p, areaprop f)
{
    return bool(_agrid(p) & f);
}

/// \brief Invalidates the area effect cache
/// \details Invalidates the area effect cache, causing the next request for
/// area effects to re-calculate which locations are covered by halos, etc.
/// If \p recheck_new is false, the cache will only be invalidated if the level
/// had existing area effects.
void invalidate_agrid(bool recheck_new)
{
    _agrid_valid = false;
    if (recheck_new)
        no_areas = false;
}

void areas_actor_moved(const actor* act, const coord_def& oldpos)
{
    UNUSED(oldpos);
    if (act->alive() &&
        (you.entering_level
         || act->halo_radius() > -1 || act->silence_radius() > -1
         || act->liquefying_radius() > -1 || act->umbra_radius() > -1
         || act->demon_silence_radius() > -1))
    {
        // Not necessarily new, but certainly potentially interesting.
        invalidate_agrid(true);
    }
}

/// \brief Add some of the actor's area effects to the grid and center caches
/// \param actor The actor
/// \details Adds some but not all of an actor's area effects (e.g. silence)
/// to the area grid (\ref _agrid) and center (\ref _agrid_centres) caches.
/// Sets \ref no_areas to false if the actor generates those area effects.
static void _actor_areas(actor *a)
{
    int r;

    if ((r = a->silence_radius()) >= 0)
    {
        _agrid_centres.emplace_back(area_centre_type::silence, a->pos(), r);

        for (radius_iterator ri(a->pos(), r, C_SQUARE); ri; ++ri)
            _set_agrid_flag(*ri, areaprop::silence);
        no_areas = false;
    }

    if ((r = a->demon_silence_radius()) >= 0)
    {
        _agrid_centres.emplace_back(area_centre_type::silence, a->pos(), r);

        for (radius_iterator ri(a->pos(), r, C_SQUARE, LOS_DEFAULT, true); ri; ++ri)
            _set_agrid_flag(*ri, areaprop::silence);
        no_areas = false;
    }

    if ((r = a->halo_radius()) >= 0)
    {
        _agrid_centres.emplace_back(area_centre_type::halo, a->pos(), r);

        for (radius_iterator ri(a->pos(), r, C_SQUARE, LOS_DEFAULT); ri; ++ri)
            _set_agrid_flag(*ri, areaprop::halo);
        no_areas = false;
    }

    if ((r = a->liquefying_radius()) >= 0)
    {
        _agrid_centres.emplace_back(area_centre_type::liquid, a->pos(), r);

        for (radius_iterator ri(a->pos(), r, C_SQUARE, LOS_SOLID); ri; ++ri)
        {
            dungeon_feature_type f = env.grid(*ri);

            if (feat_has_solid_floor(f) && !feat_is_water(f))
                _set_agrid_flag(*ri, areaprop::liquified);
        }
        no_areas = false;
    }

    if ((r = a->umbra_radius()) >= 0)
    {
        _agrid_centres.emplace_back(area_centre_type::umbra, a->pos(), r);

        for (radius_iterator ri(a->pos(), r, C_SQUARE, LOS_DEFAULT); ri; ++ri)
            _set_agrid_flag(*ri, areaprop::umbra);
        no_areas = false;
    }
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

    if ((player_has_orb() || player_equip_unrand(UNRAND_CHARLATANS_ORB))
         && !you.pos().origin())
    {
        const int r = 2;
        _agrid_centres.emplace_back(area_centre_type::orb, you.pos(), r);
        for (radius_iterator ri(you.pos(), r, C_SQUARE, LOS_DEFAULT); ri; ++ri)
            _set_agrid_flag(*ri, areaprop::orb);
        no_areas = false;
    }

    if (you.duration[DUR_QUAD_DAMAGE])
    {
        const int r = 2;
        _agrid_centres.emplace_back(area_centre_type::quad, you.pos(), r);
        for (radius_iterator ri(you.pos(), r, C_SQUARE);
             ri; ++ri)
        {
            if (cell_see_cell(you.pos(), *ri, LOS_DEFAULT))
                _set_agrid_flag(*ri, areaprop::quad);
        }
        no_areas = false;
    }

    if (you.duration[DUR_DISJUNCTION])
    {
        const int r = 4;
        _agrid_centres.emplace_back(area_centre_type::disjunction,
                                    you.pos(), r);
        for (radius_iterator ri(you.pos(), r, C_SQUARE);
             ri; ++ri)
        {
            if (cell_see_cell(you.pos(), *ri, LOS_DEFAULT))
                _set_agrid_flag(*ri, areaprop::disjunction);
        }
        no_areas = false;
    }

    // TODO: update sanctuary here.

    _agrid_valid = true;
}

static area_centre_type _get_first_area(const coord_def& f)
{
    areaprops a = _agrid(f);
    if (a & areaprop::silence)
        return area_centre_type::silence;
    if (a & areaprop::halo)
        return area_centre_type::halo;
    if (a & areaprop::umbra)
        return area_centre_type::umbra;
    if (a & areaprop::liquified)
        return area_centre_type::liquid;

    return area_centre_type::none;
}

coord_def find_centre_for(const coord_def& f, area_centre_type at)
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

    // Unspecified area type; settle for the first valid one.
    // We checked for no aprop a bit ago.
    if (at == area_centre_type::none)
        at = _get_first_area(f);

    // on the off chance that there is an error, assert here
    ASSERT(at != area_centre_type::none);

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

///////////////
// Sanctuary

static void _remove_sanctuary_property(const coord_def& where)
{
    env.pgrid(where) &= ~(FPROP_SANCTUARY_1 | FPROP_SANCTUARY_2);
}

bool sanctuary_exists()
{
    return in_bounds(env.sanctuary_pos);
}

/*
 * Remove any sanctuary from the level.
 *
 * @param did_attack If true, the sanctuary removal was the result of a player
 *                   attack, so we apply penance. Otherwise the sanctuary is
 *                   removed with no penance.
 * @returns True if we removed an existing sanctuary, false otherwise.
 */
bool remove_sanctuary(bool did_attack)
{
    if (env.sanctuary_time)
        env.sanctuary_time = 0;

    if (!sanctuary_exists())
        return false;

    const int radius = 4;
    bool seen_change = false;
    for (rectangle_iterator ri(env.sanctuary_pos, radius, true); ri; ++ri)
        if (is_sanctuary(*ri))
        {
            _remove_sanctuary_property(*ri);
            if (you.see_cell(*ri))
                seen_change = true;
        }

    env.sanctuary_pos.set(-1, -1);

    if (did_attack)
    {
        if (seen_change)
            simple_god_message(" revokes the gift of sanctuary.", GOD_ZIN);
        did_god_conduct(DID_ATTACK_IN_SANCTUARY, 3);
    }

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

int player::demon_silence_radius() const
{
    if (you.get_mutation_level(MUT_SILENCE_AURA))
        return 1;
    return -1;
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

int monster::demon_silence_radius() const
{
    return -1;
}

/// Check if a coordinate is silenced
bool silenced(const coord_def& p)
{
    if (!map_bounds(p))
        return false;
    if (!_agrid_valid)
        _update_agrid();
    return _check_agrid_flag(p, areaprop::silence);
}

/////////////
// Halos

bool haloed(const coord_def& p)
{
    if (!map_bounds(p))
        return false;
    if (!_agrid_valid)
        _update_agrid();
    return _check_agrid_flag(p, areaprop::halo);
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
        size = min((int)piety, piety_breakpoint(5)) * you.normal_vision
                                                    / piety_breakpoint(5);
    }

    if (player_equip_unrand(UNRAND_EOS))
        size = max(size, 3);
    else if (wearing_ego(EQ_ALL_ARMOUR, SPARM_LIGHT))
        size = max(size, 3);
    else if (you.props.exists(WU_JIAN_HEAVENLY_STORM_KEY))
        size = max(size, 2);

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
    if (alt_wpn && is_unrandom_artefact(*alt_wpn, UNRAND_EOS))
        size = max(size, 3);

    if (wearing_ego(EQ_ALL_ARMOUR, SPARM_LIGHT))
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

    return _check_agrid_flag(p, areaprop::liquified);
}

/////////////
// Orb's glow
//

bool orb_haloed(const coord_def& p)
{
    if (!map_bounds(p))
        return false;
    if (!_agrid_valid)
        _update_agrid();

    return _check_agrid_flag(p, areaprop::orb);
}

/////////////
// Quad damage glow
//

bool quad_haloed(const coord_def& p)
{
    if (!map_bounds(p))
        return false;
    if (!_agrid_valid)
        _update_agrid();

    return _check_agrid_flag(p, areaprop::quad);
}

/////////////
// Disjunction Glow
//

bool disjunction_haloed(const coord_def& p)
{
    if (!map_bounds(p))
        return false;
    if (!_agrid_valid)
        _update_agrid();

    return _check_agrid_flag(p, areaprop::disjunction);
}

/////////////
// Umbra
//

bool umbraed(const coord_def& p)
{
    if (!map_bounds(p))
        return false;
    if (!_agrid_valid)
        _update_agrid();

    return _check_agrid_flag(p, areaprop::umbra);
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
        if (piety >= piety_breakpoint(4))
            size = 4;
        else if (piety >= piety_breakpoint(3))
            size = 3;
        else
            size = 2;
    }

    if (you.has_mutation(MUT_FOUL_SHADOW))
        size = max(size, you.get_mutation_level(MUT_FOUL_SHADOW));

    if ((player_equip_unrand(UNRAND_BRILLIANCE))
         || player_equip_unrand(UNRAND_SHADOWS))
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
    if (alt_wpn && is_unrandom_artefact(*alt_wpn, UNRAND_BRILLIANCE))
        size = max(size, 3);

    item_def* ring = mslot_item(MSLOT_JEWELLERY);
    if (ring && is_unrandom_artefact(*ring, UNRAND_SHADOWS))
        size = max(size, 3);

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
