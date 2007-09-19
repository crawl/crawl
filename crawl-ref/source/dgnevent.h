/*
 *  File:       dgnevent.h
 *  Summary:    General dungeon events.
 *  
 *  Modified for Crawl Reference by $Author: dshaligram $ on $Date: 2007-07-20T11:40:25.964128Z $
 *  
 */

#ifndef __DGNEVENT_H__
#define __DGNEVENT_H__

#include "externs.h"
#include <list>

// Keep event names in luadgn.cc in sync.
enum dgn_event_type
{
    DET_NONE            = 0x0000,
    
    DET_TURN_ELAPSED    = 0x0001,
    DET_MONSTER_MOVED   = 0x0002,
    DET_PLAYER_MOVED    = 0x0004,
    DET_LEAVING_LEVEL   = 0x0008,
    DET_ENTERING_LEVEL  = 0x0010,
    DET_PLAYER_IN_LOS   = 0x0020,   // Player just entered LOS.
    DET_PLAYER_CLIMBS   = 0x0040,   // Player climbing stairs.
    DET_MONSTER_DIED    = 0x0080,
    DET_ITEM_PICKUP     = 0x0100,
    DET_FEAT_CHANGE     = 0x0200
};

class dgn_event
{
public:
    dgn_event_type type;
    coord_def      place;
    int            elapsed_ticks;
    long           arg1, arg2;

public:
    dgn_event(dgn_event_type t, const coord_def &p = coord_def(),
              int ticks = you.time_taken, long a1 = 0, long a2 = 0)
        : type(t), place(p), elapsed_ticks(ticks), arg1(a1), arg2(a2)
    {
    }
};

class dgn_event_listener
{
public:
    virtual ~dgn_event_listener();
    virtual void notify_dgn_event(const dgn_event &e) = 0;
};

// Alarm goes off when something enters this square.
struct dgn_square_alarm
{
public:
    dgn_square_alarm() : eventmask(0), listeners() { }
    
public:
    unsigned eventmask;
    std::list<dgn_event_listener*> listeners;
};

struct dgn_listener_def
{
public:
    dgn_listener_def(unsigned mask, dgn_event_listener *l)
        : eventmask(mask), listener(l)
    {
    }

public:
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
    std::auto_ptr<dgn_square_alarm> grid_triggers[GXM][GYM];
    std::list<dgn_listener_def> listeners;
};

extern dgn_event_dispatcher dungeon_events;

#endif
