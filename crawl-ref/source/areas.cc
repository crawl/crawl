/**
 * @file
 * @brief Tracking effects that affect areas for durations.
 *           Silence, sanctuary, halos, ...
**/

#include "AppHdr.h"

#include "areas.h"

#include "act-iter.h"
#include "art-enum.h"
#include "beam.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "fprop.h"
#include "libutil.h"
#include "losglobal.h"
#include "mon-behv.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "monster.h"
#include "player.h"
#include "religion.h"
#include "godconduct.h"
#include "stuff.h"
#include "terrain.h"
#include "traps.h"
#include "travel.h"

enum areaprop_flag
{
    APROP_SANCTUARY_1   = (1 << 0),
    APROP_SANCTUARY_2   = (1 << 1),
    APROP_SILENCE       = (1 << 2),
    APROP_HALO          = (1 << 3),
    APROP_LIQUID        = (1 << 4),
    APROP_ACTUAL_LIQUID = (1 << 5),
    APROP_ORB           = (1 << 6),
    APROP_UMBRA         = (1 << 7),
    APROP_SUPPRESSION   = (1 << 8),
    APROP_QUAD          = (1 << 9),
    APROP_DISJUNCTION   = (1 << 10),
    APROP_SOUL_AURA     = (1 << 11),
    APROP_HOT           = (1 << 12),
};

struct area_centre
{
    area_centre_type type;
    coord_def centre;
    int radius;

    explicit area_centre (area_centre_type t, coord_def c, int r) : type(t), centre(c), radius(r) { }
};

typedef FixedArray<uint32_t, GXM, GYM> propgrid_t;

static vector<area_centre> _agrid_centres;

static propgrid_t _agrid;
static bool _agrid_valid = false;
static bool no_areas = false;

static void _set_agrid_flag(const coord_def& p, areaprop_flag f)
{
    _agrid(p) |= f;
}

static bool _check_agrid_flag(const coord_def& p, areaprop_flag f)
{
    return (_agrid(p) & f);
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
         || act->halo_radius2() > -1 || act->silence_radius2() > -1
         || act->liquefying_radius2() > -1 || act->umbra_radius2() > -1
         || act->suppression_radius2() > -1 || act->heat_radius2() > -1))
    {
        // Not necessarily new, but certainly potentially interesting.
        invalidate_agrid(true);
    }
}

static void _actor_areas(actor *a)
{
    int r;

    if ((r = a->silence_radius2()) >= 0)
    {
        _agrid_centres.push_back(area_centre(AREA_SILENCE, a->pos(), r));

        for (radius_iterator ri(a->pos(), r, C_CIRCLE); ri; ++ri)
            _set_agrid_flag(*ri, APROP_SILENCE);
        no_areas = false;
    }

    // Just like silence, suppression goes through walls
    if ((r = a->suppression_radius2()) >= 0)
    {
        _agrid_centres.push_back(area_centre(AREA_SUPPRESSION, a->pos(), r));

        for (radius_iterator ri(a->pos(), r, C_CIRCLE); ri; ++ri)
            _set_agrid_flag(*ri, APROP_SUPPRESSION);
        no_areas = false;
    }

    if ((r = a->halo_radius2()) >= 0)
    {
        _agrid_centres.push_back(area_centre(AREA_HALO, a->pos(), r));

        for (radius_iterator ri(a->pos(), r, C_CIRCLE, LOS_DEFAULT); ri; ++ri)
            _set_agrid_flag(*ri, APROP_HALO);
        no_areas = false;
    }

    if ((r = a->liquefying_radius2()) >= 0)
    {
        _agrid_centres.push_back(area_centre(AREA_LIQUID, a->pos(), r));

        for (radius_iterator ri(a->pos(), r, C_CIRCLE, LOS_SOLID); ri; ++ri)
        {
            dungeon_feature_type f = grd(*ri);

            _set_agrid_flag(*ri, APROP_LIQUID);

            if (feat_has_solid_floor(f) && !feat_is_water(f))
                _set_agrid_flag(*ri, APROP_ACTUAL_LIQUID);
        }
        no_areas = false;
    }

    if ((r = a->umbra_radius2()) >= 0)
    {
        _agrid_centres.push_back(area_centre(AREA_UMBRA, a->pos(), r));

        for (radius_iterator ri(a->pos(), r, C_CIRCLE, LOS_DEFAULT); ri; ++ri)
            _set_agrid_flag(*ri, APROP_UMBRA);
        no_areas = false;
    }

    if ((r = a->heat_radius2()) >= 0)
    {
        _agrid_centres.push_back(area_centre(AREA_HOT, a->pos(), r));

        for (radius_iterator ri(a->pos(), r, C_CIRCLE, LOS_NO_TRANS); ri; ++ri)
            _set_agrid_flag(*ri, APROP_HOT);
        no_areas = false;
    }
}

static void _update_agrid()
{
    if (no_areas)
    {
        _agrid_valid = true;
        return;
    }

    _agrid.init(0);
    _agrid_centres.clear();

    no_areas = true;

    _actor_areas(&you);
    for (monster_iterator mi; mi; ++mi)
        _actor_areas(*mi);

    if (you.char_direction == GDT_ASCENDING && !you.pos().origin())
    {
        const int r = 5;
        _agrid_centres.push_back(area_centre(AREA_ORB, you.pos(), r));
        for (radius_iterator ri(you.pos(), r, C_CIRCLE, LOS_DEFAULT); ri; ++ri)
            _set_agrid_flag(*ri, APROP_ORB);
        no_areas = false;
    }

    if (you.duration[DUR_QUAD_DAMAGE])
    {
        const int r = 5;
        _agrid_centres.push_back(area_centre(AREA_QUAD, you.pos(), r));
        for (radius_iterator ri(you.pos(), r, C_CIRCLE);
             ri; ++ri)
        {
            if (cell_see_cell(you.pos(), *ri, LOS_DEFAULT))
                _set_agrid_flag(*ri, APROP_QUAD);
        }
        no_areas = false;
    }

    if (you.duration[DUR_DISJUNCTION])
    {
        const int r = 27;
        _agrid_centres.push_back(area_centre(AREA_DISJUNCTION, you.pos(), r));
        for (radius_iterator ri(you.pos(), r, C_CIRCLE);
             ri; ++ri)
        {
            if (cell_see_cell(you.pos(), *ri, LOS_DEFAULT))
                _set_agrid_flag(*ri, APROP_DISJUNCTION);
        }
        no_areas = false;
    }

    if (!env.sunlight.empty())
    {
        for (size_t i = 0; i < env.sunlight.size(); ++i)
            _set_agrid_flag(env.sunlight[i].first, APROP_HALO);
        no_areas = false;
    }

    // TODO: update sanctuary here.

    _agrid_valid = true;
}

static area_centre_type _get_first_area(const coord_def& f)
{
    uint32_t a = _agrid(f);
    if (a & APROP_SANCTUARY_1)
        return AREA_SANCTUARY;
    if (a & APROP_SANCTUARY_2)
        return AREA_SANCTUARY;
    if (a & APROP_SILENCE)
        return AREA_SILENCE;
    if (a & APROP_HOT)
        return AREA_HOT;
    if (a & APROP_HALO)
        return AREA_HALO;
    if (a & APROP_UMBRA)
        return AREA_UMBRA;
    if (a & APROP_SUPPRESSION)
        return AREA_SUPPRESSION;
    // liquid is always applied; actual_liquid is on top
    // of this. If we find the first, we don't care about
    // the second.
    if (a & APROP_LIQUID)
        return AREA_LIQUID;

    return AREA_NONE;
}

coord_def find_centre_for(const coord_def& f, area_centre_type at)
{
    if (!map_bounds(f))
        return coord_def(-1, -1);

    if (!_agrid_valid)
        _update_agrid();

    if (_agrid(f) == 0)
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

    for (unsigned int i = 0; i < _agrid_centres.size(); i++)
    {
        area_centre a = _agrid_centres[i];
        if (a.type != at)
            continue;

        if (a.centre == f)
            return f;

        int d = distance2(a.centre, f);
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

    const int radius = 5;
    bool seen_change = false;
    for (rectangle_iterator ri(env.sanctuary_pos, radius); ri; ++ri)
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
        mons_stop_fleeing_from_sanctuary(*mi);

    if (is_resting())
        stop_running();

    return true;
}

// For the last (radius) counter turns the sanctuary will slowly shrink.
void decrease_sanctuary_radius()
{
    const int radius = 5;

    // For the last (radius-1) turns 33% chance of not decreasing.
    if (env.sanctuary_time < radius && one_chance_in(3))
        return;

    int size = --env.sanctuary_time;
    if (size >= radius)
        return;

    if (you.running && is_sanctuary(you.pos()))
    {
        mpr("The sanctuary starts shrinking.", MSGCH_DURATION);
        stop_running();
    }

    for (rectangle_iterator ri(env.sanctuary_pos, size+1); ri; ++ri)
    {
        int dist = distance2(*ri, env.sanctuary_pos);

        // If necessary overwrite sanctuary property.
        if (dist > size*size)
            _remove_sanctuary_property(*ri);
    }

    // Special case for time-out of sanctuary.
    if (!size)
    {
        _remove_sanctuary_property(env.sanctuary_pos);
        if (you.see_cell(env.sanctuary_pos))
            mpr("The sanctuary disappears.", MSGCH_DURATION);
    }
}

void create_sanctuary(const coord_def& center, int time)
{
    env.sanctuary_pos  = center;
    env.sanctuary_time = time;

    // radius could also be influenced by Inv
    // and would then have to be stored globally.
    const int radius      = 5;
    int       blood_count = 0;
    int       trap_count  = 0;
    int       scare_count = 0;
    int       cloud_count = 0;
    monster* seen_mon    = NULL;

    // Since revealing mimics can move monsters, we do it first.
    for (radius_iterator ri(center, radius, C_POINTY); ri; ++ri)
        discover_mimic(*ri);

    int shape = random2(4);
    for (radius_iterator ri(center, radius, C_POINTY); ri; ++ri)
    {
        const coord_def pos = *ri;
        const int dist = distance2(center, pos);

        if (testbits(env.pgrid(pos), FPROP_BLOODY) && you.see_cell(pos))
            blood_count++;

        if (trap_def* ptrap = find_trap(pos))
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
            in_yellow = (dist >= (radius-1)*(radius-1)
                         && dist <= radius*radius
                         || dist >= (radius/2-1)*(radius/2-1)
                            && dist <= radius*radius/4);
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
            else if (!mon->wont_attack() && mons_is_influenced_by_sanctuary(mon))
            {
                mons_start_fleeing_from_sanctuary(mon);

                // Check to see that monster is actually fleeing.
                if (mons_is_fleeing(mon) && you.can_see(mon))
                {
                    scare_count++;
                    seen_mon = mon;
                }
            }
        }

        if (!is_harmless_cloud(cloud_type_at(pos)))
        {
            delete_cloud(env.cgrid(pos));
            if (you.see_cell(pos))
                cloud_count++;
        }
    } // radius loop

    // Messaging.
    if (trap_count > 0)
    {
        mpr("By Zin's power, hidden traps are revealed to you.",
            MSGCH_GOD);
    }

    if (cloud_count == 1)
    {
        mpr("By Zin's power, the foul cloud within the sanctuary is "
            "swept away.", MSGCH_GOD);
    }
    else if (cloud_count > 1)
    {
        mpr("By Zin's power, all foul fumes within the sanctuary are "
            "swept away.", MSGCH_GOD);
    }

    if (blood_count > 0)
    {
        mpr("By Zin's power, all blood is cleared from the sanctuary.",
            MSGCH_GOD);
    }

    if (scare_count == 1 && seen_mon != NULL)
        simple_monster_message(seen_mon, " turns to flee the light!");
    else if (scare_count > 0)
        mpr("The monsters scatter in all directions!");
}


/////////////
// Silence

// pre-squared radius, calculated from remaining duration
// dur starts at 10 (low power) and is capped at 100
// maximal range: 6*6 + 1 = 37
// last 6 turns: range 0, hence only the player silenced
static int _silence_range(int dur)
{
    if (dur <= 0)
        return -1;
    dur /= BASELINE_DELAY; // now roughly number of turns
    return max(0, min(dur - 6, 37));
}

int player::silence_radius2() const
{
    return _silence_range(duration[DUR_SILENCE]);
}

int monster::silence_radius2() const
{
    if (type == MONS_SILENT_SPECTRE)
        return 150;

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
    return _check_agrid_flag(p, APROP_SILENCE);
}

/////////////
// Halos

bool haloed(const coord_def& p)
{
    if (!map_bounds(p))
        return false;
    if (!_agrid_valid)
        _update_agrid();
    return _check_agrid_flag(p, APROP_HALO);
}

bool actor::haloed() const
{
    return ::haloed(pos());
}

int player::halo_radius2() const
{
    int size = -1;

    if (religion == GOD_SHINING_ONE && piety >= piety_breakpoint(0)
        && !penance[GOD_SHINING_ONE])
    {
        // Preserve the middle of old radii.
        const int r = piety - 10;
        // The cap is 64, just less than the LOS of 65.
        size = min(LOS_RADIUS*LOS_RADIUS, r * r / 400);
    }

    // Can't check suppression because this function is called from
    // _update_agrid()---we'd get an infinite recursion.
    if (player_equip_unrand(UNRAND_BRILLIANCE))
        size = max(size, 9);

    return size;
}

int monster::halo_radius2() const
{
    item_def* weap = mslot_item(MSLOT_WEAPON);
    int size = -1;

    if (weap && weap->special == UNRAND_BRILLIANCE)
        size = 9;

    if (holiness() != MH_HOLY)
        return size;
    // The values here depend on 1. power, 2. sentience.  Thus, high-ranked
    // sentient celestials have really big haloes, while holy animals get
    // small ones.
    switch (type)
    {
    case MONS_ANGEL:
        return 26;
    case MONS_CHERUB:
        return 29;
    case MONS_DAEVA:
        return 32;
    case MONS_SERAPH:
        return 50;
    case MONS_OPHAN:
        return 64; // highest rank among sentient ones
    case MONS_SHEDU:
        return 10;
    case MONS_SILVER_STAR:
        return 40; // dumb but with an immense power
    case MONS_HOLY_SWINE:
        return 1;  // only notionally holy
    case MONS_MENNAS:
        return 4;  // ???  Low on grace or what?
    default:
        return -1;
    }
}

//////////////////////
// Leda's Liquefaction
//

int player::liquefying_radius2() const
{
    return _silence_range(duration[DUR_LIQUEFYING]);
}

int monster::liquefying_radius2() const
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

    // "actually" liquified (ie, check for movement)
    if (check_actual)
        return _check_agrid_flag(p, APROP_ACTUAL_LIQUID);
    // just recoloured for consistency
    else
        return _check_agrid_flag(p, APROP_LIQUID);
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

    return _check_agrid_flag(p, APROP_ORB);
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

    return _check_agrid_flag(p, APROP_QUAD);
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

    return _check_agrid_flag(p, APROP_DISJUNCTION);
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

    return _check_agrid_flag(p, APROP_UMBRA);
}

// Whether actor is in an umbra.
bool actor::umbraed() const
{
    return ::umbraed(pos());
}

// Stub for player umbra.
int player::umbra_radius2() const
{
    return -1;
}

int monster::umbra_radius2() const
{
    if (holiness() != MH_UNDEAD)
        return -1;

    switch (type)
    {
    case MONS_PROFANE_SERVITOR:
        return 40; // Very unholy!
    default:
        return -1;
    }
}

/////////////
// Suppression

bool suppressed(const coord_def& p)
{
    if (!map_bounds(p))
        return false;
    if (!_agrid_valid)
        _update_agrid();

    return _check_agrid_flag(p, APROP_SUPPRESSION);
}

int monster::suppression_radius2() const
{
    if (type == MONS_MOTH_OF_SUPPRESSION)
        return 150;
    else
        return -1;
}

bool actor::suppressed() const
{
    return ::suppressed(pos());
}

int player::suppression_radius2() const
{
    return -1;
}

/////////////
// Heat aura (lava orcs).

// Player radius
int player::heat_radius2() const
{
    if (species != SP_LAVA_ORC)
        return -1;

    if (!temperature_effect(LORC_HEAT_AURA))
        return -1;

    return 2; // Surrounds you to radius of 1.
}

// Stub for monster radius
int monster::heat_radius2() const
{
    return -1;
}

bool heated(const coord_def& p)
{
    if (!map_bounds(p))
        return false;

    if (!_agrid_valid)
        _update_agrid();

    return _check_agrid_flag(p, APROP_HOT);
}

bool actor::heated() const
{
    return ::heated(pos());
}
