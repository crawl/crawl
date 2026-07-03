/**
 * @file
 * @brief Handling for lurker monsters.
**/

#include "AppHdr.h"

#include "mon-lurk.h"

#include "act-iter.h"
#include "coordit.h"
#include "defines.h"
#include "env.h"
#include "fixedarray.h"
#include "god-passive.h"
#include "message.h"
#include "mgen-data.h"
#include "monster.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-pathfind.h"
#include "mon-place.h"
#include "player-notices.h"
#include "religion.h"
#include "stringutil.h"
#include "terrain.h"

// Quick lookup for whether a lurker exists in a given cell.
FixedArray< bool, GXM, GYM >  lgrid;

// Take a real monster and convert it into a lurker, adding it to lurker data
// and deleting the original monster from the map.
void start_lurking_at(monster& mon, const coord_def& pos)
{
    env.lurkers.push_back({follower(mon), pos, false, 0, false});
    lgrid[pos.x][pos.y] = true;

    monster_die(mon, KILL_RESET, NON_MONSTER);
}

void start_lurking(monster& mon)
{
    // Find a spot within 10-30 tiles where the monster is now, which is
    // reachable by it, to lurk at. (Essentially fuzzing position for wandering
    // monsters that resume lurking.)

    const int dist = random_range(10, 30);
    monster_pathfind mp;
    mp.fill_traversability(&mon, dist, false);

    int valid = 0;
    coord_def spot;
    for (distance_iterator di(mon.pos(), false, false, dist); di; ++di)
    {
        if (mp.is_reachable(*di))
            if (one_chance_in(++valid))
                spot = *di;
    }

    if (spot.origin())
        return;

    start_lurking_at(mon, spot);

}

// Update lgrid after unmarshalling.
void init_lurker_map()
{
    lgrid.init(false);

    for (lurker_data &data : env.lurkers)
        lgrid[data.pos.x][data.pos.y] = true;
}

static void _erase_lurker(int index)
{
    const coord_def pos = env.lurkers[index].pos;
    env.lurkers.erase(env.lurkers.begin() + index);

    // Update the lgrid if nothing else is lurking here.
    for (const lurker_data& lurker : env.lurkers)
        if (lurker.pos == pos)
            return;

    lgrid[pos.x][pos.y] = false;
}

static bool _try_to_place_lurker(lurker_data& lurker)
{
    coord_def spot = find_newmons_square(lurker.mon.mons.type, you.pos(),
                                         2, 5);

    if (spot.origin())
        return false;

    monster* real_mon = lurker.mon.place();
    if (!real_mon)
        return false;

    real_mon->move_to(spot, MV_INTERNAL);
    real_mon->behaviour = BEH_SEEK;
    real_mon->target = you.pos();
    real_mon->foe = MHITYOU;

    queue_monster_announcement(*real_mon, SC_LURKER_AMBUSH);
    notice_queued_monsters();

    return true;
}

// Ash can 'reveal' lurkers by essentially forcing them to manifest at their
// lurking position, visible and asleep.
static monster* _try_to_reveal_lurker(lurker_data& lurker)
{
    // Prefer to be at exactly our lurking spot, but allow slight repositioning.
    coord_def spot = find_newmons_square(lurker.mon.mons.type, lurker.pos, 0);

    if (spot.origin())
        return nullptr;

    monster* real_mon = lurker.mon.place();
    if (!real_mon)
        return nullptr;

    real_mon->move_to(spot, MV_INTERNAL);
    real_mon->behaviour = BEH_SLEEP;
    real_mon->target = real_mon->pos();
    real_mon->foe = MHITNOT;

    if (you.can_see(*real_mon))
    {
        string msg = make_stringf(" warns you of the presence of a lurking %s.",
                                    real_mon->name(DESC_PLAIN).c_str());
        simple_god_message(msg.c_str());
    }

    return real_mon;
}

// Potentially alert a lurker at a given spot, due either to noise or proximity.
void alert_lurker_at(const coord_def& pos, bool do_pathfind_check)
{
    if (!lgrid[pos.x][pos.y])
        return;

    for (int i = env.lurkers.size() - 1; i >= 0; --i)
    {
        lurker_data& lurker = env.lurkers[i];
        if (lurker.pos == pos && !lurker.alerted)
        {
            // Even with Ash, lurkers alerted by noise appear awake instead of
            // asleep (though they still cannot ambush you, even if they fail to
            // place this way for some reason.)
            if (have_passive(passive_t::see_unseen))
            {
                if (monster* mon = _try_to_reveal_lurker(lurker))
                {
                    behaviour_event(mon, ME_ALERT, &you);
                    _erase_lurker(i);
                }
            }
            else
            {
                // Noise tests that the player is reachable (and not, say, on
                // the other side of a runed door).
                if (do_pathfind_check)
                {
                    // If pathfinding has recently failed, skip until later.
                    if (lurker.timer > you.elapsed_time)
                        continue;

                    monster_pathfind mp;
                    mp.init_pathfind(lurker.pos, you.pos(), false);
                    mp.set_range(grid_distance(lurker.pos, you.pos()) + 5);

                    // If pathfinding failed, use the timer as a way to avoid
                    // doing additional pathfinding every single turn.
                    if (!mp.start_pathfind())
                    {
                        lurker.timer = you.elapsed_time + random_range(70, 120);
                        continue;
                    }
                }

                lurker.alerted = true;
                lurker.timer = you.elapsed_time + random_range(40, 120);
            }
        }
    }
}

// A very simple test to try and make lurkers spawn at relevant times.
// Checks that there are is at least 1 tough enemy (or more than 3 easy enemies)
// in the player's LoS, unless fewer of those exist on the entire floor.
//
// If 3 or more lurkers are already alerted and ready, they consider *themselves*
// adequate to the situation.
static bool _monster_threat_is_sufficient()
{
    vector<lurker_data*> ready_lurkers;
    for (lurker_data& lurker : env.lurkers)
        if (lurker.alerted && lurker.timer <= you.elapsed_time + 50)
            ready_lurkers.push_back(&lurker);

    // XXX: This is a bit of an approximation, since it's not guaranteed we'll
    //      be able to place all of these when we try, but should usually be
    //      good enough.
    //
    //      Note that we have to tell the other lurkers not to perform the
    //      threat check or *they* often won't find enough things still in the
    //      queue.
    if (ready_lurkers.size() >= 3)
    {
        for (lurker_data* lurker : ready_lurkers)
            lurker->ignore_threat = true;
        return true;
    }

    int nearby_count = 0;
    int total_count = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (!mi->wont_attack() && !mi->is_firewood() && !mi->is_summoned()
            && mons_threat_level(**mi) > MTHRT_TRIVIAL)
        {
            if (!you.see_cell_no_trans(mi->pos()))
            {
                total_count++;
                continue;
            }

            if (mons_threat_level(**mi) >= MTHRT_TOUGH)
                return true;

            if (++nearby_count >= 3)
                return true;
        }
    }

    return total_count <= 1;
}

void handle_lurkers()
{
    // Handle Ash revealing nearby lurkers.
    // (This works even through walls so that monster detection can pick them up)
    if (have_passive(passive_t::see_unseen))
    {
        for (int i = env.lurkers.size() - 1; i >= 0; --i)
        {
            lurker_data &lurker = env.lurkers[i];
            if (grid_distance(lurker.pos, you.pos()) <= you.current_vision)
                if (_try_to_reveal_lurker(lurker))
                    _erase_lurker(i);
        }
    }

    // Check if there are any lurkers in LoS and alert them.
    for (lurker_data& lurker : env.lurkers)
    {
        if (!lurker.alerted && you.see_cell_no_trans(lurker.pos))
            alert_lurker_at(lurker.pos);
    }

    bool did_threat_calc = false;
    bool threat_ok = false;

    // Now attempt to place any alerted lurkers whose timers are up.
    for (int i = env.lurkers.size() - 1; i >= 0; --i)
    {
        lurker_data &data = env.lurkers[i];
        if (!data.alerted || data.timer > you.elapsed_time)
            continue;

        // Calculate threat (just once for the whole batch)
        if (!did_threat_calc)
            threat_ok = _monster_threat_is_sufficient();

        // Add a random delay when we fail to place a monster so that queued
        // lurkers don't all show up at the exact same time, as soon as nearby
        // threat changes.
        if (!threat_ok && !data.ignore_threat)
        {
            data.timer = you.elapsed_time + random_range(40, 100);
            continue;
        }

        if (_try_to_place_lurker(data))
        {
            _erase_lurker(i);

            // Increase timer of all other lurkers slightly,
            // to avoid placing too many at once.
            for (lurker_data& lurker : env.lurkers)
                lurker.timer += roll_dice(5, 12);
        }
        // If we can't place this monster, wait a short time before trying again.
        else
        {
            data.timer = you.elapsed_time + random_range(40, 100);
            data.ignore_threat = false;
        }
    }
}

// Reset timers on alerted lurkers when the player leaves the floor.
void cancel_pending_lurkers()
{
    for (lurker_data &lurker : env.lurkers)
    {
        lurker.timer = 0;
        lurker.ignore_threat = false;
        lurker.alerted = false;
    }
}
