/**
 * @file
 * @brief Region system implementations
**/

#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg.h"

#include "glwrapper.h"

Region::Region() :
    ox(0),
    oy(0),
    dx(0),
    dy(0),
    mx(0),
    my(0),
    wx(0),
    wy(0),
    sx(0),
    sy(0),
    ex(0),
    ey(0)
{
}

void Region::resize(int _mx, int _my)
{
    mx = _mx;
    my = _my;

    recalculate();
}

void Region::place(int _sx, int _sy, int margin)
{
    sx = _sx;
    sy = _sy;
    ox = margin;
    oy = margin;

    recalculate();
}

void Region::place(int _sx, int _sy, int _ex, int _ey, int margin)
{
    sx = _sx;
    sy = _sy;
    ex = _ex;
    ey = _ey;
    wx = ex - sx;
    wy = ey - sy;
    ox = margin;
    oy = margin;

    on_resize();
}


void Region::place(int _sx, int _sy)
{
    sx = _sx;
    sy = _sy;

    recalculate();
}

void Region::resize_to_fit(int _wx, int _wy)
{
    if (_wx < 0 || _wy < 0)
    {
        mx = wx = my = wy = 0;
        ey = sy;
        ex = sy;
        return;
    }

    int inner_x = _wx - 2 * ox;
    int inner_y = _wy - 2 * oy;

    mx = dx ? inner_x / dx : 0;
    my = dy ? inner_y / dy : 0;

    recalculate();

    ex = sx + _wx;
    ey = sy + _wy;
}

void Region::recalculate()
{
    wx = ox * 2 + mx * dx;
    wy = oy * 2 + my * dy;
    ex = sx + wx;
    ey = sy + wy;

    on_resize();
}

Region::~Region()
{
}

bool Region::inside(int x, int y)
{
    return x >= sx && y >= sy && x <= ex && y <= ey;
}

bool Region::mouse_pos(int mouse_x, int mouse_y, int &cx, int &cy)
{
    int x = mouse_x - ox - sx;
    int y = mouse_y - oy - sy;

    bool valid = (x >= 0 && y >= 0);

    ASSERT(dx > 0);
    ASSERT(dy > 0);
    x /= dx;
    y /= dy;
    valid &= (x < mx && y < my);

    cx = x;
    cy = y;

    return valid;
}

void Region::set_transform(bool no_scaling)
{
    GLW_3VF trans(sx + ox, sy + oy, 0);
    GLW_3VF scale;

    if (no_scaling)
        scale = GLW_3VF(1, 1, 1);
    else
        scale = GLW_3VF(dx, dy, 1);

    glmanager->set_transform(trans, scale);
}

TileRegion::TileRegion(const TileRegionInit &init)
{
    ASSERT(init.im);
    ASSERT(init.tag_font);

    m_image = init.im;
    dx = init.tile_x;
    dy = init.tile_y;
    m_tag_font = init.tag_font;

    // To quiet Valgrind.
    m_dirty = true;
}

TileRegion::~TileRegion()
{
}

#endif
