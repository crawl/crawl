#include "AppHdr.h"

#include "losglobal.h"

#include "coord.h"
#include "coordit.h"
#include "libutil.h"
#include "los_def.h"

#define LOS_KNOWN 4

typedef uint8_t losfield_t;
typedef losfield_t halflos_t[LOS_MAX_RANGE+1][2*LOS_MAX_RANGE+1];
static const int o_half_x = 0;
static const int o_half_y = LOS_MAX_RANGE;
typedef halflos_t globallos_t[GXM][GYM];

static globallos_t globallos;

static losfield_t* _lookup_globallos(const coord_def& p, const coord_def& q)
{
    COMPILE_CHECK(LOS_KNOWN * 2 <= sizeof(losfield_t) * 8);

    if (!map_bounds(p) || !map_bounds(q))
        return nullptr;
    coord_def diff = q - p;
    if (diff.rdist() > LOS_RADIUS)
        return nullptr;
    // p < q iff p.x < q.x || p.x == q.x && p.y < q.y
    if (diff < coord_def(0, 0))
        return &globallos[q.x][q.y][-diff.x + o_half_x][-diff.y + o_half_y];
    else
        return &globallos[p.x][p.y][ diff.x + o_half_x][ diff.y + o_half_y];
}

static void _save_los(los_def* los, los_type l)
{
    const coord_def o = los->get_center();
    int y1 = o.y - LOS_MAX_RANGE;
    int y2 = o.y + LOS_MAX_RANGE;
    int x1 = o.x - LOS_MAX_RANGE;
    int x2 = o.x + LOS_MAX_RANGE;
    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++)
        {
            if (max(abs(o.x - x), abs(o.y - y)) > LOS_MAX_RANGE)
                continue;

            coord_def ri(x, y);
            losfield_t* flags = _lookup_globallos(o, ri);
            if (!flags)
                continue;
            *flags |= l << LOS_KNOWN;
            if (los->see_cell(ri))
                *flags |= l;
            else
                *flags &= ~l;
        }
}

// Opacity at p has changed.
void invalidate_los_around(const coord_def& p)
{
    int x1 = max(p.x - LOS_MAX_RANGE, 0);
    int y1 = max(p.y - LOS_MAX_RANGE, 0);
    int x2 = min(p.x, GXM - 1);
    int y2 = min(p.y + LOS_MAX_RANGE, GYM - 1);
    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++)
            memset(globallos[x][y], 0, sizeof(halflos_t));
}

void invalidate_los()
{
    for (rectangle_iterator ri(0); ri; ++ri)
        memset(globallos[ri->x][ri->y], 0, sizeof(halflos_t));
}

static void _update_globallos_at(const coord_def& p, los_type l)
{
    switch (l)
    {
    case LOS_DEFAULT:
        {
            los_def los(p, opc_default);
            los.update();
            _save_los(&los, l);
            break;
        }
    case LOS_NO_TRANS:
        {
            los_def los(p, opc_no_trans);
            los.update();
            _save_los(&los, l);
            break;
        }
    case LOS_SOLID:
        {
            los_def los(p, opc_solid);
            los.update();
            _save_los(&los, l);
            break;
        }
    case LOS_SOLID_SEE:
        {
            los_def los(p, opc_solid_see);
            los.update();
            _save_los(&los, l);
            break;
        }
    default:
        die("invalid opacity");
    }
}

bool cell_see_cell(const coord_def& p, const coord_def& q, los_type l)
{
    if (l == LOS_NONE)
        return true;

    losfield_t* flags = _lookup_globallos(p, q);

    if (!flags)
        return false; // outside range

    if (!(*flags & (l << LOS_KNOWN)))
        _update_globallos_at(p, l);

    //if (!(*flags & (l << LOS_KNOWN)))
    //    die("cell_see_cell %d,%d %d,%d", p.x,p.y,q.x,q.y);
    ASSERT(*flags & (l << LOS_KNOWN));

    return *flags & l;
}
