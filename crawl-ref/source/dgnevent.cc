/**
 * @file
 * @brief General dungeon events.
**/

#include "AppHdr.h"

#include "dgnevent.h"

#include <algorithm>

#include "coord.h"

dgn_event_dispatcher dungeon_events;

void dgn_event_dispatcher::clear()
{
    global_event_mask = 0;
    listeners.clear();
    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
            grid_triggers[x][y].reset(NULL);
}

void dgn_event_dispatcher::clear_listeners_at(const coord_def &pos)
{
    grid_triggers[pos.x][pos.y].reset(NULL);
}

void dgn_event_dispatcher::move_listeners(
    const coord_def &from, const coord_def &to)
{
    // Any existing listeners at to will be discarded. YHBW.
    grid_triggers[to.x][to.y] = move(grid_triggers[from.x][from.y]);
}

bool dgn_event_dispatcher::has_listeners_at(const coord_def &pos) const
{
    return grid_triggers[pos.x][pos.y].get();
}

bool dgn_event_dispatcher::fire_vetoable_position_event(
    dgn_event_type et, const coord_def &pos)
{
    const dgn_event event(et, pos);
    return fire_vetoable_position_event(event, pos);
}

bool dgn_event_dispatcher::fire_vetoable_position_event(
    const dgn_event &et, const coord_def &pos)
{
    dgn_square_alarm *alarm = grid_triggers[pos.x][pos.y].get();
    if (alarm && (alarm->eventmask & et.type))
    {
        dgn_square_alarm alcopy(*alarm);
        for (auto listener : alcopy.listeners)
            if (!listener->notify_dgn_event(et))
                return false;
    }
    return true;
}

void dgn_event_dispatcher::fire_position_event(
    dgn_event_type event, const coord_def &pos)
{
    const dgn_event et(event, pos);
    fire_position_event(et, pos);
}

void dgn_event_dispatcher::fire_position_event(
    const dgn_event &et, const coord_def &pos)
{
    dgn_square_alarm *alarm = grid_triggers[pos.x][pos.y].get();
    if (alarm && (alarm->eventmask & et.type))
    {
        dgn_square_alarm alcopy = *alarm;
        for (auto listener : alcopy.listeners)
            listener->notify_dgn_event(et);
    }
}

void dgn_event_dispatcher::fire_event(const dgn_event &e)
{
    if (global_event_mask & e.type)
    {
        auto copy = listeners;
        for (const auto &ldef : copy)
            if (ldef.eventmask & e.type)
                ldef.listener->notify_dgn_event(e);
    }
}

void dgn_event_dispatcher::fire_event(dgn_event_type et)
{
    fire_event(dgn_event(et));
}

void dgn_event_dispatcher::register_listener(unsigned mask,
                                             dgn_event_listener *listener,
                                             const coord_def &pos)
{
    if (in_bounds(pos))
        register_listener_at(mask, pos, listener);
    else
    {
        global_event_mask |= mask;
        for (auto &ldef : listeners)
        {
            if (ldef.listener == listener)
            {
                ldef.eventmask |= mask;
                return;
            }
        }
        listeners.push_back(dgn_listener_def(mask, listener));
    }
}

void dgn_event_dispatcher::register_listener_at(unsigned mask,
                                                const coord_def &c,
                                                dgn_event_listener *listener)
{
    if (!grid_triggers[c.x][c.y].get())
        grid_triggers[c.x][c.y].reset(new dgn_square_alarm);

    dgn_square_alarm *alarm = grid_triggers[c.x][c.y].get();
    alarm->eventmask |= mask;
    if (find(alarm->listeners.begin(), alarm->listeners.end(), listener)
        == alarm->listeners.end())
    {
        alarm->listeners.push_back(listener);
    }
}

void dgn_event_dispatcher::remove_listener(dgn_event_listener *listener,
                                           const coord_def &pos)
{
    if (in_bounds(pos))
        remove_listener_at(pos, listener);
    else
    {
        for (auto i = listeners.begin(); i != listeners.end(); ++i)
        {
            if (i->listener == listener)
            {
                listeners.erase(i);
                return;
            }
        }
    }
}

void dgn_event_dispatcher::remove_listener_at(const coord_def &pos,
                                              dgn_event_listener *listener)
{
    if (dgn_square_alarm *alarm = grid_triggers[pos.x][pos.y].get())
    {
        list<dgn_event_listener*>::iterator i = find(alarm->listeners.begin(),
                                                     alarm->listeners.end(),
                                                     listener);
        if (i != alarm->listeners.end())
            alarm->listeners.erase(i);
    }
}

/////////////////////////////////////////////////////////////////////////////
// dgn_event_listener

dgn_event_listener::~dgn_event_listener()
{
}
