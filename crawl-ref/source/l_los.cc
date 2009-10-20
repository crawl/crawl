/*
 * File:        l_los.cc
 * Summary:     Lua bindings for LOS.
 * Created by:  Robert Vollmert
 */

#include "AppHdr.h"

#include "dlua.h"
#include "l_libs.h"

#include "los.h"
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
    if (find_ray(a, b, *ray))
    {
        lua_push_ray(ls, ray);
        return (1);
    }
    delete ray;
    return (0);
}

LUAFN(los_cell_see_cell)
{
    COORDS(p, 1, 2);
    COORDS(q, 3, 4);
    PLUARET(number, cell_see_cell(p, q));
}

const struct luaL_reg los_dlib[] =
{
    { "findray", los_find_ray },
    { "cell_see_cell", los_cell_see_cell },
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

static const struct luaL_reg ray_dlib[] =
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
    clua_register_metatable(ls, RAY_METATABLE, ray_dlib, lua_object_gc<ray_def>);
}
