/*
 * File:      areas.cc
 * Summary:   Tracking effects that affect areas for durations.
 *            Silence, sanctuary, halos, ...
 */

#include "AppHdr.h"

#include "areas.h"

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
#include "stuff.h"
#include "traps.h"
#include "travel.h"
#include "viewgeom.h"

// Is where inside a circle within LOS, with the given radius,
// centered on the player's position?  If so, return the distance to it.
// Otherwise, return -1.
static int _inside_circle(const coord_def& where, int radius)
{
    if (!in_bounds(where))
        return -1;

    const coord_def ep = grid2view(where);
    if (!in_los_bounds(ep))
        return -1;

    const int dist = distance(where, you.pos());
    if (dist > radius*radius)
        return -1;

    return (dist);
}

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
        did_god_conduct(DID_FRIEND_DIED, 3);
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
    monsters *seen_mon    = NULL;

    for (radius_iterator ri(center, radius, C_POINTY); ri; ++ri)
    {
        const int dist = _inside_circle(*ri, radius);

        if (dist == -1)
            continue;

        const coord_def pos = *ri;
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
        switch (random2(4))
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
        if (monsters* mon = monster_at(pos))
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
