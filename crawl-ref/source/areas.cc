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
#include "godconduct.h"
#include "godpassive.h" // passive_t::umbra
#include "libutil.h"
#include "losglobal.h"
#include "message.h"
#include "mon-behv.h"
#include "religion.h"
#include "stepdown.h"
#include "terrain.h"
#include "traps.h"
#include "travel.h"

enum class areaprop
{
    SANCTUARY_1   = (1 << 0),
    SANCTUARY_2   = (1 << 1),
    SILENCE       = (1 << 2),
    HALO          = (1 << 3),
    LIQUID        = (1 << 4),
    ACTUAL_LIQUID = (1 << 5),
    ORB           = (1 << 6),
    UMBRA         = (1 << 7),
    QUAD          = (1 << 8),
    DISJUNCTION   = (1 << 9),
    SOUL_AURA     = (1 << 10),
#if TAG_MAJOR_VERSION == 34
    HOT           = (1 << 11),
#endif
};
DEF_BITFIELD(areaprops, areaprop);

struct area_centre
{
    area_centre_type type;
    coord_def centre;
    int radius;

    explicit area_centre (area_centre_type t, coord_def c, int r) : type(t), centre(c), radius(r) { }
};

typedef FixedArray<areaprops, GXM, GYM> propgrid_t;

static vector<area_centre> _agrid_centres;

static propgrid_t _agrid;
static bool _agrid_valid = false;
static bool no_areas = false;

static void _set_agrid_flag(const coord_def& p, areaprop f)
{
    _agrid(p) |= f;
}

static bool _check_agrid_flag(const coord_def& p, areaprop f)
{
    return bool(_agrid(p) & f);
}

void invalidate_agrid(bool recheck_new)
{
    _agrid_valid = false;
    if (recheck_new)
        no_areas = false;
}

void areas_actor_moved(const actor* act, const coord_def& oldpos)
{
    if (act->alive() &&
        (you.entering_level
         || act->halo_radius() > -1 || act->silence_radius() > -1
         || act->liquefying_radius() > -1 || act->umbra_radius() > -1
#if TAG_MAJOR_VERSION == 34
         || act->heat_radius() > -1
#endif
         ))
    {
        // Not necessarily new, but certainly potentially interesting.
        invalidate_agrid(true);
    }
}

static void _actor_areas(actor *a)
{
    int r;

    if ((r = a->silence_radius()) >= 0)
    {
        _agrid_centres.emplace_back(AREA_SILENCE, a->pos(), r);

        for (radius_iterator ri(a->pos(), r, C_SQUARE); ri; ++ri)
            _set_agrid_flag(*ri, areaprop::SILENCE);
        no_areas = false;
    }

    if ((r = a->halo_radius()) >= 0)
    {
        _agrid_centres.emplace_back(AREA_HALO, a->pos(), r);

        for (radius_iterator ri(a->pos(), r, C_SQUARE, LOS_DEFAULT); ri; ++ri)
            _set_agrid_flag(*ri, areaprop::HALO);
        no_areas = false;
    }

    if ((r = a->liquefying_radius()) >= 0)
    {
        _agrid_centres.emplace_back(AREA_LIQUID, a->pos(), r);

        for (radius_iterator ri(a->pos(), r, C_SQUARE, LOS_SOLID); ri; ++ri)
        {
            dungeon_feature_type f = grd(*ri);

            _set_agrid_flag(*ri, areaprop::LIQUID);

            if (feat_has_solid_floor(f) && !feat_is_water(f))
                _set_agrid_flag(*ri, areaprop::ACTUAL_LIQUID);
        }
        no_areas = false;
    }

    if ((r = a->umbra_radius()) >= 0)
    {
        _agrid_centres.emplace_back(AREA_UMBRA, a->pos(), r);

        for (radius_iterator ri(a->pos(), r, C_SQUARE, LOS_DEFAULT); ri; ++ri)
            _set_agrid_flag(*ri, areaprop::UMBRA);
        no_areas = false;
    }

#if TAG_MAJOR_VERSION == 34
    if ((r = a->heat_radius()) >= 0)
    {
        _agrid_centres.emplace_back(AREA_HOT, a->pos(), r);

        for (radius_iterator ri(a->pos(), r, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
            _set_agrid_flag(*ri, areaprop::HOT);
        no_areas = false;
    }
#endif
}

/**
 * Update the area grid cache.
 *
 * Updates the _agrid FixedArray of grid information flags using the
 * areaprop types.
 */
static void _update_agrid()
{
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

    if (player_has_orb() && !you.pos().origin())
    {
        const int r = 2;
        _agrid_centres.emplace_back(AREA_ORB, you.pos(), r);
        for (radius_iterator ri(you.pos(), r, C_SQUARE, LOS_DEFAULT); ri; ++ri)
            _set_agrid_flag(*ri, areaprop::ORB);
        no_areas = false;
    }

    if (you.duration[DUR_QUAD_DAMAGE])
    {
        const int r = 2;
        _agrid_centres.emplace_back(AREA_QUAD, you.pos(), r);
        for (radius_iterator ri(you.pos(), r, C_SQUARE);
             ri; ++ri)
        {
            if (cell_see_cell(you.pos(), *ri, LOS_DEFAULT))
                _set_agrid_flag(*ri, areaprop::QUAD);
        }
        no_areas = false;
    }

    if (you.duration[DUR_DISJUNCTION])
    {
        const int r = 4;
        _agrid_centres.emplace_back(AREA_DISJUNCTION, you.pos(), r);
        for (radius_iterator ri(you.pos(), r, C_SQUARE);
             ri; ++ri)
        {
            if (cell_see_cell(you.pos(), *ri, LOS_DEFAULT))
                _set_agrid_flag(*ri, areaprop::DISJUNCTION);
        }
        no_areas = false;
    }

    if (!env.sunlight.empty())
    {
        for (const auto &entry : env.sunlight)
            _set_agrid_flag(entry.first, areaprop::HALO);
        no_areas = false;
    }

    // TODO: update sanctuary here.

    _agrid_valid = true;
}

static area_centre_type _get_first_area(const coord_def& f)
{
    areaprops a = _agrid(f);
    if (a & areaprop::SANCTUARY_1)
        return AREA_SANCTUARY;
    if (a & areaprop::SANCTUARY_2)
        return AREA_SANCTUARY;
    if (a & areaprop::SILENCE)
        return AREA_SILENCE;
#if TAG_MAJOR_VERSION == 34
    if (a & areaprop::HOT)
        return AREA_HOT;
#endif
    if (a & areaprop::HALO)
        return AREA_HALO;
    if (a & areaprop::UMBRA)
        return AREA_UMBRA;
    // liquid is always applied; actual_liquid is on top
    // of this. If we find the first, we don't care about
    // the second.
    if (a & areaprop::LIQUID)
        return AREA_LIQUID;

    return AREA_NONE;
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
    if (at == AREA_NONE)
        at = _get_first_area(f);

    // on the off chance that there is an error, assert here
    ASSERT(at != AREA_NONE);

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

bool remove_sanctuary(bool did_attack)
{
    if (env.sanctuary_time)
        env.sanctuary_time = 0;

    if (!in_bounds(env.sanctuary_pos))
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
    int       trap_count  = 0;
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

        if (trap_def* ptrap = trap_at(pos))
        {
            if (!ptrap->is_known())
            {
                ptrap->reveal();
                ++trap_count;
            }
        }

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
    if (trap_count > 0)
        mprf(MSGCH_GOD, "By Zin's power, hidden traps are revealed to you.");

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

/////////////
// Silence

// radius, calculated from remaining duration
// dur starts at 10 (low power) and is capped at 100
// maximal range: 5
// last 6 turns: range 0, hence only the player silenced
static int _silence_range(int dur)
{
    if (dur <= 0)
        return -1;
    dur /= BASELINE_DELAY; // now roughly number of turns
    return isqrt(max(0, min(3*(dur - 5)/4, 25)));
}

int player::silence_radius() const
{
    return _silence_range(duration[DUR_SILENCE]);
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
    return _silence_range(moddur);
}

bool silenced(const coord_def& p)
{
    if (!map_bounds(p))
        return false;
    if (!_agrid_valid)
        _update_agrid();
    return _check_agrid_flag(p, areaprop::SILENCE);
}

/////////////
// Halos

bool haloed(const coord_def& p)
{
    if (!map_bounds(p))
        return false;
    if (!_agrid_valid)
        _update_agrid();
    return _check_agrid_flag(p, areaprop::HALO);
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
        size = min((int)piety, piety_breakpoint(5)) * LOS_RADIUS
                                                    / piety_breakpoint(5);
    }

    if (player_equip_unrand(UNRAND_EOS))
        size = max(size, 3);

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
    item_def* weap = mslot_item(MSLOT_WEAPON);
    int size = -1;

    if (weap && is_unrandom_artefact(*weap, UNRAND_EOS))
        size = 3;

    if (weap && (weap->flags & ISFLAG_UNRANDART)
        && weap->unrand_idx >= UNRAND_DIVINE_DEER_HORN_KNIFE
        && weap->unrand_idx <= UNRAND_DIVINE_METEOR_HAMMER)
        size = 1;

    if (!(holiness() & MH_HOLY))
        return size;

    return _mons_class_halo_radius(type);
}

//////////////////////
// Leda's Liquefaction
//

int player::liquefying_radius() const
{
    return _silence_range(duration[DUR_LIQUEFYING]);
}

int monster::liquefying_radius() const
{
    if (!has_ench(ENCH_LIQUEFYING))
        return -1;
    const int dur = get_ench(ENCH_LIQUEFYING).duration;
    // The below is arbitrarily chosen to make monster decay look reasonable.
    const int moddur = BASELINE_DELAY *
        max(7, stepdown_value(dur * 10 - 60, 10, 5, 45, 100));
    return _silence_range(moddur);
}

bool liquefied(const coord_def& p, bool check_actual)
{
    if (!map_bounds(p))
        return false;

    if (!_agrid_valid)
        _update_agrid();

    if (feat_is_water(grd(p)))
        return false;

    // "actually" liquefied (ie, check for movement)
    if (check_actual)
        return _check_agrid_flag(p, areaprop::ACTUAL_LIQUID);
    // just recoloured for consistency
    else
        return _check_agrid_flag(p, areaprop::LIQUID);
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

    return _check_agrid_flag(p, areaprop::ORB);
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

    return _check_agrid_flag(p, areaprop::QUAD);
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

    return _check_agrid_flag(p, areaprop::DISJUNCTION);
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

    return _check_agrid_flag(p, areaprop::UMBRA);
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
        // The cap is reached at piety 160 = ******.
        size = min((int)piety, piety_breakpoint(5)) * LOS_RADIUS
                                                    / piety_breakpoint(5);
    }

    if (player_equip_unrand(UNRAND_SHADOWS))
        size = max(size, 3);

    return size;
}

int monster::umbra_radius() const
{
    item_def* ring = mslot_item(MSLOT_JEWELLERY);
    if (ring && is_unrandom_artefact(*ring, UNRAND_SHADOWS))
        return 3;

    if (!(holiness() & MH_UNDEAD))
        return -1;

    // Enslaved holies get an umbra.
    if (mons_enslaved_soul(*this))
        return _mons_class_halo_radius(base_monster);

    switch (type)
    {
    case MONS_PROFANE_SERVITOR:
        return 5; // Very unholy!
    default:
        return -1;
    }
}

#if TAG_MAJOR_VERSION == 34
/////////////
// Heat aura (lava orcs).

// Player radius
int player::heat_radius() const
{
    if (species != SP_LAVA_ORC)
        return -1;

    if (!temperature_effect(LORC_HEAT_AURA))
        return -1;

    return 1; // Surrounds you to radius of 1.
}

// Stub for monster radius
int monster::heat_radius() const
{
    return -1;
}

bool heated(const coord_def& p)
{
    if (!map_bounds(p))
        return false;

    if (!_agrid_valid)
        _update_agrid();

    return _check_agrid_flag(p, areaprop::HOT);
}

bool actor::heated() const
{
    return ::heated(pos());
}
#endif
