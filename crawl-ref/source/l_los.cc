/*
 * File:        l_los.cc
 * Summary:     Lua bindings for LOS.
 * Created by:  Robert Vollmert
 */

#include "AppHdr.h"

#include "l_los.h"

#include "los.h"
#include "luadgn.h"
#include "ray.h"

#define RAY_METATABLE "dgn.ray"

void lua_push_ray(lua_State *ls, ray_def *ray)
{
    ray_def **rayref = clua_new_userdata<ray_def *>(ls, RAY_METATABLE);
    *rayref = ray;
}

LUAFN(los_find_ray)
{
    GETCOORD(a, 1, 2, map_bounds);
    GETCOORD(b, 3, 4, map_bounds);
    ray_def *ray = new ray_def;
    if (find_ray(a, b, *ray, 0, true))
    {
        lua_push_ray(ls, ray);
        return (1);
    }
    delete ray;
    return (0);
}

const struct luaL_reg los_lib[] =
{
    { "findray", los_find_ray },
    { NULL, NULL }
};

#define RAY(ls, n, var) \
    ray_def *var = *(ray_def **) luaL_checkudata(ls, n, RAY_METATABLE)

LUAFN(ray_accx)
{
    RAY(ls, 1, ray);
    lua_pushnumber(ls, ray->accx);
    return (1);
}

LUAFN(ray_accy)
{
    RAY(ls, 1, ray);
    lua_pushnumber(ls, ray->accy);
    return (1);
}

LUAFN(ray_slope)
{
    RAY(ls, 1, ray);
    lua_pushnumber(ls, ray->slope);
    return (1);
}

LUAFN(ray_advance)
{
    RAY(ls, 1, ray);
    lua_pushnumber(ls, ray->advance());
    return (1);
}

LUAFN(ray_pos)
{
    RAY(ls, 1, ray);
    coord_def p = ray->pos();
    lua_pushnumber(ls, p.x);
    lua_pushnumber(ls, p.y);
    return (2);
}

static const struct luaL_reg ray_lib[] =
{
    { "accx", ray_accx },
    { "accy", ray_accy },
    { "slope", ray_slope },
    { "advance", ray_advance },
    { "pos", ray_pos },
    { NULL, NULL }
};

void luaopen_ray(lua_State *ls)
{
//    luaopen_setmeta(ls, "ray", ray_lib, RAY_METATABLE);
    clua_register_metatable(ls, RAY_METATABLE, ray_lib, lua_object_gc<ray_def>);
}
