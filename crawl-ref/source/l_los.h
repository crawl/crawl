/*
 * File:        l_los.h
 * Summary:     Lua bindings for LOS.
 * Created by:  Robert Vollmert
 */

#ifndef L_LOS_H
#define L_LOS_H

#include "clua.h"

extern const struct luaL_reg los_lib[];
void luaopen_ray(lua_State *ls);

#endif

