#ifndef CLUAUTIL_H
#define CLUAUTIL_H

struct lua_State;

struct activity_interrupt_data;
int push_activity_interrupt(lua_State *ls, activity_interrupt_data *t);

class map_def;
void clua_push_map(lua_State *ls, map_def *map);

void clua_push_coord(lua_State *ls, const coord_def &c);

class dgn_event;
void clua_push_dgn_event(lua_State *ls, const dgn_event *devent);

// XXX: currently defined outside cluautil.cc.
class monsters;
void push_monster(lua_State *ls, monsters* mons);
void lua_push_items(lua_State *ls, int link);

#endif
