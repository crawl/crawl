#include "clua.h"

extern const struct luaL_reg crawl_lib[];
extern const struct luaL_reg dgn_lib[];
extern const struct luaL_reg dgnevent_lib[];
extern const struct luaL_reg file_lib[];
extern const struct luaL_reg los_lib[];
extern const struct luaL_reg mapmarker_lib[];
extern const struct luaL_reg you_lib[];

void luaopen_mapmarker(lua_State *ls);
void luaopen_ray(lua_State *ls);

void register_mapdef_tables(lua_State *ls);
    

