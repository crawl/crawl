/*** Dungeon building line of sight (dlua only).
 * @module los
 */
#include "AppHdr.h"

#include "l-libs.h"

#include "cluautil.h"
#include "coord.h"
#include "losglobal.h"
#include "los.h"
#include "ray.h"
#include "stringutil.h"

#define RAY_METATABLE "dgn.ray"

static void _lua_push_ray(lua_State *ls, ray_def *ray)
{
    ray_def **rayref = clua_new_userdata<ray_def *>(ls, RAY_METATABLE);
    *rayref = ray;
}

LUAFN(los_find_ray)
{
    GETCOORD(a, 1, 2, map_bounds);
    GETCOORD(b, 3, 4, map_bounds);
    ray_def *ray = new ray_def;
    // XXX: opc_no_trans to fit with you.see_cell_no_trans
    //      in findray test case.
    if (find_ray(a, b, *ray, opc_no_trans))
    {
        _lua_push_ray(ls, ray);
        return 1;
    }
    delete ray;
    return 0;
}

#define VECT(v, p1, p2) \
    geom::vector v; \
    v.x = luaL_checknumber(ls, p1); \
    v.y = luaL_checknumber(ls, p2);

LUAFN(los_make_ray)
{
    VECT(s, 1, 2);
    VECT(d, 3, 4);
    ray_def *ray = new ray_def(geom::ray(s.x, s.y, d.x, d.y));
    _lua_push_ray(ls, ray);
    return 1;
}

LUAFN(los_cell_see_cell)
{
    COORDS(p, 1, 2);
    COORDS(q, 3, 4);
    PLUARET(number, cell_see_cell(p, q, LOS_DEFAULT));
}

const struct luaL_Reg los_dlib[] =
{
    { "findray", los_find_ray },
    { "make_ray", los_make_ray },
    { "cell_see_cell", los_cell_see_cell },
    { nullptr, nullptr }
};

#define RAY(ls, n, var) \
    ray_def *var = *(ray_def **) luaL_checkudata(ls, n, RAY_METATABLE); \
    if (!var) \
        return 0;

LUAFN(ray_start)
{
    RAY(ls, 1, ray);
    lua_pushnumber(ls, ray->r.start.x);
    lua_pushnumber(ls, ray->r.start.y);
    return 2;
}

LUAFN(ray_dir)
{
    RAY(ls, 1, ray);
    lua_pushnumber(ls, ray->r.dir.x);
    lua_pushnumber(ls, ray->r.dir.y);
    return 2;
}

LUAFN(ray_advance)
{
    RAY(ls, 1, ray);
    ray->advance();
    return 0;
}

LUAFN(ray_pos)
{
    RAY(ls, 1, ray);
    coord_def p = ray->pos();
    lua_pushnumber(ls, p.x);
    lua_pushnumber(ls, p.y);
    return 2;
}

static const struct luaL_Reg ray_dlib[] =
{
    { "start", ray_start },
    { "dir", ray_dir },
    { "advance", ray_advance },
    { "pos", ray_pos },
    { nullptr, nullptr }
};

void luaopen_ray(lua_State *ls)
{
    clua_register_metatable(ls, RAY_METATABLE, ray_dlib, lua_object_gc<ray_def>);
}
