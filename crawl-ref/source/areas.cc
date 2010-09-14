/*
 * File:      areas.cc
 * Summary:   Tracking effects that affect areas for durations.
 *            Silence, sanctuary, halos, ...
 */

#include "AppHdr.h"

#include "areas.h"

#include "act-iter.h"
#include "beam.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "directn.h"
#include "env.h"
#include "fprop.h"
#include "mon-behv.h"
#include "mon-iter.h"
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
    APROP_SANCTUARY_1 = (1 << 0),
    APROP_SANCTUARY_2 = (1 << 1),
    APROP_SILENCE     = (1 << 2),
    APROP_HALO        = (1 << 3),
};

typedef FixedArray<unsigned long, GXM, GYM> propgrid_t;

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
         || act->halo_radius2() > -1 || act->silence_radius2() > -1))
    {
        // Not necessarily new, but certainly potentially interesting.
        invalidate_agrid(true);
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

    no_areas = true;

    for (actor_iterator ai; ai; ++ai)
    {
        int r;

        if ((r = ai->silence_radius2()) >= 0)
        {
            for (radius_iterator ri(ai->pos(), r, C_CIRCLE); ri; ++ri)
                _set_agrid_flag(*ri, APROP_SILENCE);
            no_areas = false;
        }

        if ((r = ai->halo_radius2()) >= 0)
        {
            for (radius_iterator ri(ai->pos(), r, C_CIRCLE, ai->get_los());
                 ri; ++ri)
            {
                _set_agrid_flag(*ri, APROP_HALO);
            }
            no_areas = false;
        }
    }

    // TODO: update sanctuary here.

    _agrid_valid = true;
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
        return (false);

    const int radius = 5;
    bool seen_change = false;
    for (radius_iterator ri(env.sanctuary_pos, radius, C_SQUARE); ri; ++ri)
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

    return (true);
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

    for (radius_iterator ri(env.sanctuary_pos, size+1, C_SQUARE); ri; ++ri)
    {
        int dist = distance(*ri, env.sanctuary_pos);

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

    int shape = random2(4);
    for (radius_iterator ri(center, radius, C_POINTY); ri; ++ri)
    {
        const coord_def pos = *ri;
        const int dist = distance(center, pos);

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
                behaviour_event(mon, ME_EVAL, MHITYOU);
            }
            else if (!mon->wont_attack())
            {
                if (mons_is_mimic(mon->type))
                {
                    mimic_alert(mon);
                    if (you.can_see(mon))
                    {
                        scare_count++;
                        seen_mon = mon;
                    }
                }
                else if (mons_is_influenced_by_sanctuary(mon))
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
        mpr("By Zin's power hidden traps are revealed to you.",
            MSGCH_GOD);
    }

    if (cloud_count == 1)
    {
        mpr("By Zin's power the foul cloud within the sanctuary is "
            "swept away.", MSGCH_GOD);
    }
    else if (cloud_count > 1)
    {
        mpr("By Zin's power all foul fumes within the sanctuary are "
            "swept away.", MSGCH_GOD);
    }

    if (blood_count > 0)
    {
        mpr("By Zin's power all blood is cleared from the sanctuary.",
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
        return (-1);
    dur /= BASELINE_DELAY; // now roughly number of turns
    return std::max(0, std::min(dur - 6, 37));
}

int player::silence_radius2() const
{
    return (_silence_range(you.duration[DUR_SILENCE]));
}

int monster::silence_radius2() const
{
    if (type == MONS_SILENT_SPECTRE)
        return 150;

    if (!has_ench(ENCH_SILENCE))
        return (-1);
    const int dur = get_ench(ENCH_SILENCE).duration;
    // The below is arbitrarily chosen to make monster decay look reasonable.
    const int moddur = BASELINE_DELAY *
        std::max(7, stepdown_value(dur * 10 - 60, 10, 5, 45, 100));
    return (_silence_range(moddur));
}

bool silenced(const coord_def& p)
{
    if (!map_bounds(p))
        return (false);
    if (!_agrid_valid)
        _update_agrid();
    return (_check_agrid_flag(p, APROP_SILENCE));
}

/////////////
// Halos

bool haloed(const coord_def& p)
{
    if (!map_bounds(p))
        return (false);
    if (!_agrid_valid)
        _update_agrid();
    return (_check_agrid_flag(p, APROP_HALO));
}

bool actor::haloed() const
{
    return (::haloed(pos()));
}

int player::halo_radius2() const
{
    if (you.religion == GOD_SHINING_ONE && you.piety >= piety_breakpoint(0)
        && !you.penance[GOD_SHINING_ONE])
    {
        // Preserve the middle of old radii.
        const int r = you.piety - 10;
        // The cap is 64, just less than the LOS of 65.
        return std::min(LOS_RADIUS*LOS_RADIUS, r * r / 400);
    }

    return (-1);
}

int monster::halo_radius2() const
{
    if (holiness() != MH_HOLY)
        return (-1);
    // The values here depend on 1. power, 2. sentience.  Thus, high-ranked
    // sentient celestials have really big haloes, while holy animals get
    // small ones.
    switch(type)
    {
    case MONS_SPIRIT:
        return (1);
    case MONS_ANGEL:
        return (26);
    case MONS_CHERUB:
        return (29);
    case MONS_DAEVA:
        return (32);
    case MONS_HOLY_DRAGON:
        return (5);
    case MONS_OPHAN:
        return (64); // highest rank among sentient ones
    case MONS_PHOENIX:
        return (10);
    case MONS_SHEDU:
        return (10);
    case MONS_APIS:
        return (4);
    case MONS_PALADIN:
        return (4);  // mere humans
    case MONS_BLESSED_TOE:
        return (17);
    case MONS_SILVER_STAR:
        return (40); // dumb but with an immense power
    default:
        return (4);
    }
}
