/**
 * @file
 * @brief General dungeon events.
**/

#pragma once

#include <list>

#include "player.h"

// Keep event names in l-dgnevt.cc in sync.
enum dgn_event_type
{
    DET_NONE            = 0x0000,

    DET_TURN_ELAPSED    = 0x0001,
    DET_MONSTER_MOVED   = 0x0002,
    DET_PLAYER_MOVED    = 0x0004,
    DET_LEAVING_LEVEL   = 0x0008,
    DET_ENTERING_LEVEL  = 0x0010,
    DET_ENTERED_LEVEL   = 0x0020,
    DET_PLAYER_IN_LOS   = 0x0040,   // Player just entered LOS.
    DET_PLAYER_CLIMBS   = 0x0080,   // Player climbing stairs.
    DET_MONSTER_DIED    = 0x0100,
    DET_ITEM_PICKUP     = 0x0200,
    DET_ITEM_MOVED      = 0x0400,
    DET_FEAT_CHANGE     = 0x0800,
    DET_UNUSED          = 0x1000,   // open for grabs
    DET_DOOR_OPENED     = 0x2000,
    DET_DOOR_CLOSED     = 0x4000,
    DET_HP_WARNING      = 0x8000,
    DET_PRESSURE_PLATE  = 0x10000,

    DET_GLOBAL_MASK     = DET_TURN_ELAPSED | DET_LEAVING_LEVEL
                        | DET_ENTERING_LEVEL | DET_ENTERED_LEVEL
                        | DET_MONSTER_DIED | DET_HP_WARNING,

    DET_POSITION_MASK   = DET_MONSTER_MOVED | DET_PLAYER_MOVED
                        | DET_PLAYER_IN_LOS | DET_ITEM_PICKUP
                        | DET_ITEM_MOVED | DET_FEAT_CHANGE
                        | DET_DOOR_OPENED | DET_DOOR_CLOSED
                        | DET_PRESSURE_PLATE,
};

class dgn_event
{
public:
    dgn_event_type type;
    coord_def      place;
    int            elapsed_ticks;
    int            arg1, arg2;
    coord_def      dest;

public:
    dgn_event(dgn_event_type t, const coord_def &p = coord_def(),
              int ticks = you.time_taken, int a1 = 0, int a2 = 0,
              const coord_def &d = coord_def())
        : type(t), place(p), elapsed_ticks(ticks), arg1(a1),
          arg2(a2), dest(d)
    {
    }
};

class dgn_event_listener
{
public:
    virtual ~dgn_event_listener();
    // For vetoable events, return false to veto.
    virtual bool notify_dgn_event(const dgn_event &e) = 0;
};

// Alarm goes off when something enters this square.
struct dgn_square_alarm
{
    dgn_square_alarm() : eventmask(0), listeners() { }

    unsigned eventmask;
    list<dgn_event_listener*> listeners;
};

struct dgn_listener_def
{
    dgn_listener_def(unsigned mask, dgn_event_listener *l)
        : eventmask(mask), listener(l)
    {
    }

    unsigned eventmask;
    dgn_event_listener *listener;
};

// Listeners are not saved here. Map markers have their own
// persistence and activation mechanisms. Other listeners must make
// their own persistence arrangements.
class dgn_event_dispatcher
{
public:
    dgn_event_dispatcher() : global_event_mask(0), grid_triggers()
    {
    }

    void clear();
    void clear_listeners_at(const coord_def &pos);
    bool has_listeners_at(const coord_def &pos) const;
    void move_listeners(const coord_def &from, const coord_def &to);

    // Returns false if the event is vetoed.
    bool fire_vetoable_position_event(const dgn_event &e,
                                      const coord_def &pos);

    bool fire_vetoable_position_event(dgn_event_type et,
                                      const coord_def &pos);

    void fire_position_event(dgn_event_type et, const coord_def &pos);
    void fire_position_event(const dgn_event &e, const coord_def &pos);
    void fire_event(dgn_event_type et);
    void fire_event(const dgn_event &e);

    void register_listener(unsigned evmask, dgn_event_listener *,
                           const coord_def &pos = coord_def());
    void remove_listener(dgn_event_listener *,
                         const coord_def &pos = coord_def());
private:
    void register_listener_at(unsigned mask, const coord_def &pos,
                              dgn_event_listener *l);
    void remove_listener_at(const coord_def &pos, dgn_event_listener *l);

private:
    unsigned global_event_mask;
    unique_ptr<dgn_square_alarm> grid_triggers[GXM][GYM];
    list<dgn_listener_def> listeners;
};

extern dgn_event_dispatcher dungeon_events;
