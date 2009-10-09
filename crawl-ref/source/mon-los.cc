/*
 *  File:       mon-los.cc
 *  Summary:    Monster line-of-sight.
 */

#include "AppHdr.h"
REVISION("$Rev$");
#include "mon-los.h"

#include "los.h"
#include "ray.h"
#include "stuff.h"

// Static class members must be initialised outside of the class
// declaration, or gcc won't define them in view.o and we'll get a
// linking error.
const int monster_los::LSIZE     =  _monster_los_LSIZE;
const int monster_los::L_VISIBLE =  1;
const int monster_los::L_UNKNOWN =  0;
const int monster_los::L_BLOCKED = -1;

/////////////////////////////////////////////////////////////////////////////
// monster_los

monster_los::monster_los()
    : gridx(0), gridy(0), mons(), range(LOS_RADIUS), los_field()
{
}

monster_los::~monster_los()
{
}

void monster_los::set_monster(monsters *mon)
{
    mons = mon;
    set_los_centre(mon->pos());
}

void monster_los::set_los_centre(int x, int y)
{
    if (!in_bounds(x, y))
        return;

    gridx = x;
    gridy = y;
}

void monster_los::set_los_range(int r)
{
    ASSERT (r >= 1 && r <= LOS_RADIUS);
    range = r;
}

coord_def monster_los::pos_to_index(coord_def &p)
{
    int ix = LOS_RADIUS + p.x - gridx;
    int iy = LOS_RADIUS + p.y - gridy;

    ASSERT(ix >= 0 && ix < LSIZE);
    ASSERT(iy >= 0 && iy < LSIZE);

    return (coord_def(ix, iy));
}

coord_def monster_los::index_to_pos(coord_def &i)
{
    int px = i.x + gridx - LOS_RADIUS;
    int py = i.y + gridy - LOS_RADIUS;

    ASSERT(in_bounds(px, py));
    return (coord_def(px, py));
}

void monster_los::set_los_value(int x, int y, bool blocked, bool override)
{
    if (!override && !is_unknown(x,y))
        return;

    coord_def c(x,y);
    coord_def lpos = pos_to_index(c);

    int value = (blocked ? L_BLOCKED : L_VISIBLE);

    if (value != los_field[lpos.x][lpos.y])
        los_field[lpos.x][lpos.y] = value;
}

int monster_los::get_los_value(int x, int y)
{
    // Too far away -> definitely out of sight!
    if (distance(x, y, gridx, gridy) > get_los_radius_squared())
        return (L_BLOCKED);

    coord_def c(x,y);
    coord_def lpos = pos_to_index(c);
    return (los_field[lpos.x][lpos.y]);
}

bool monster_los::in_sight(int x, int y)
{
    // Is the path to (x,y) clear?
    return (get_los_value(x,y) == L_VISIBLE);
}

bool monster_los::is_blocked(int x, int y)
{
    // Is the path to (x,y) blocked?
    return (get_los_value(x, y) == L_BLOCKED);
}

bool monster_los::is_unknown(int x, int y)
{
    return (get_los_value(x, y) == L_UNKNOWN);
}

static bool _blocks_movement_sight(monsters *mon, dungeon_feature_type feat)
{
    if (feat < DNGN_MINMOVE)
        return (true);

    if (!mon) // No monster defined?
        return (false);

    if (!mon->can_pass_through_feat(feat))
        return (true);

    return (false);
}

void monster_los::fill_los_field()
{
    int pos_x, pos_y;
    for (int k = 1; k <= range; k++)
        for (int i = -1; i <= 1; i++)
            for (int j = -1; j <= 1; j++)
            {
                if (i == 0 && j == 0) // Ignore centre grid.
                    continue;

                pos_x = gridx + k*i;
                pos_y = gridy + k*j;

                if (!in_bounds(pos_x, pos_y))
                    continue;

                if (!_blocks_movement_sight(mons, grd[pos_x][pos_y]))
                    set_los_value(pos_x, pos_y, false);
                else
                {
                    set_los_value(pos_x, pos_y, true);
                    // Check all beam potentially going through a blocked grid.
                    check_los_beam(pos_x, pos_y);
                }
            }
}

// (cx, cy) is the centre point
// (dx, dy) is the target we're aiming *through*
// target1 and target2 are targets we'll be aiming *at* to fire through (dx,dy)
static bool _set_beam_target(int cx, int cy, int dx, int dy,
                             int &target1_x, int &target1_y,
                             int &target2_x, int &target2_y,
                             int range)
{
    const int xdist = dx - cx;
    const int ydist = dy - cy;

    if (xdist == 0 && ydist == 0)
        return (false); // Nothing to be done.

    if (xdist <= -range || xdist >= range
        || ydist <= -range || ydist >= range)
    {
        // Grids on the edge of a monster's LOS don't block sight any further.
        return (false);
    }

/*
 *   The following code divides the field into eights of different directions.
 *
 *    \  NW | NE  /
 *      \   |   /
 *    WN  \ | /   EN
 *   ----------------
 *    WS  / | \   ES
 *      /   |   \
 *    /  SW | SE  \
 *
 *   target1_x and target1_y mark the base line target, so the base beam ends
 *   on the diagonal line closest to the target (or on one of the straight
 *   lines if cx == dx or dx == dy).
 *
 *   target2_x and target2_y then mark the second target our beam finder should
 *   cycle through. It'll always be target2_x = dx or target2_y = dy, the other
 *   being on the edge of LOS, which one depending on the quadrant.
 *
 *   The beam finder can then cycle from the nearest corner (target1) to the
 *   second edge target closest to (dx,dy).
 */

    if (xdist == 0)
    {
        target1_x = cx;
        target1_y = (ydist > 0 ? cy + range
                               : cy - range);

        target2_x = target1_x;
        target2_y = target1_y;
    }
    else if (ydist == 0)
    {
        target1_x = (xdist > 0 ? cx + range
                               : cx - range);
        target1_y = cy;

        target2_x = target1_x;
        target2_y = target1_y;
    }
    else if (xdist < 0 && ydist < 0 || xdist > 0 && ydist > 0)
    {
        if (xdist < 0)
        {
            target1_x = cx - range;
            target1_y = cy - range;
        }
        else
        {
            target1_x = cx + range;
            target1_y = cy + range;
        }

        if (xdist == ydist)
        {
            target2_x = target1_x;
            target2_y = target1_y;
        }
        else
        {
            if (xdist < 0) // both are negative (upper left corner)
            {
                if (dx > dy)
                {
                    target2_x = dx;
                    target2_y = cy - range;
                }
                else
                {
                    target2_x = cx - range;
                    target2_y = dy;
                }
            }
            else // both are positive (lower right corner)
            {
                if (dx > dy)
                {
                    target2_x = cx + range;
                    target2_y = dy;
                }
                else
                {
                    target2_x = dx;
                    target2_y = cy + range;
                }
            }
        }
    }
    else if (xdist < 0 && ydist > 0 || xdist > 0 && ydist < 0)
    {
        if (xdist < 0) // lower left corner
        {
            target1_x = cx - range;
            target1_y = cy + range;
        }
        else // upper right corner
        {
            target1_x = cx + range;
            target1_y = cy - range;
        }

        if (xdist == -ydist)
        {
            target2_x = target1_x;
            target2_y = target1_y;
        }
        else
        {
            if (xdist < 0) // ydist > 0
            {
                if (-xdist > ydist)
                {
                    target2_x = cx - range;
                    target2_y = dy;
                }
                else
                {
                    target2_x = dx;
                    target2_y = cy + range;
                }
            }
            else // xdist > 0, ydist < 0
            {
                if (-xdist > ydist)
                {
                    target2_x = dx;
                    target2_y = cy - range;
                }
                else
                {
                    target2_x = cx + range;
                    target2_y = dy;
                }
            }
        }
    }
    else
    {
        // Everything should have been handled above.
        ASSERT(false);
    }

    return (true);
}

void monster_los::check_los_beam(int dx, int dy)
{
    ray_def ray;

    int target1_x = 0, target1_y = 0, target2_x = 0, target2_y = 0;
    if (!_set_beam_target(gridx, gridy, dx, dy, target1_x, target1_y,
                          target2_x, target2_y, range))
    {
        // Nothing to be done.
        return;
    }

    if (target1_x > target2_x || target1_y > target2_y)
    {
        // Swap the two targets so our loop will work correctly.
        int help = target1_x;
        target1_x = target2_x;
        target2_x = help;

        help = target1_y;
        target1_y = target2_y;
        target2_y = help;
    }

    const int max_dist = range;
    int dist;
    bool blocked = false;
    for (int tx = target1_x; tx <= target2_x; tx++)
        for (int ty = target1_y; ty <= target2_y; ty++)
        {
            // If (tx, ty) lies outside the level boundaries, there's nothing
            // that shooting a ray into that direction could bring us, esp.
            // as earlier grids in the ray will already have been handled, and
            // out of bounds grids are simply skipped in any LoS check.
            if (!map_bounds(tx, ty))
                continue;

            // Already calculated a beam to (tx, ty), don't do so again.
            if (!is_unknown(tx, ty))
                continue;

            dist = 0;
            find_ray( coord_def(gridx, gridy), coord_def(tx, ty),
                      true, ray, 0, true, true );

            if (ray.x() == gridx && ray.y() == gridy)
                ray.advance(true);

            while (dist++ <= max_dist)
            {
                // The ray brings us out of bounds of the level map.
                // Since we're always shooting outwards there's nothing more
                // to look at in that direction, and we can break the loop.
                if (!map_bounds(ray.x(), ray.y()))
                    break;

                if (blocked)
                {
                    // Earlier grid blocks this beam, set to blocked if
                    // unknown, but don't overwrite visible grids.
                    set_los_value(ray.x(), ray.y(), true);
                }
                else if (_blocks_movement_sight(mons, grd[ray.x()][ray.y()]))
                {
                    set_los_value(ray.x(), ray.y(), true);
                    // The rest of the beam will now be blocked.
                    blocked = true;
                }
                else
                {
                    // Allow overriding in case another beam has marked this
                    // field as blocked, because we've found a solution where
                    // that isn't the case.
                    set_los_value(ray.x(), ray.y(), false, true);
                }
                if (ray.x() == tx && ray.y() == ty)
                    break;

                ray.advance(true);
            }
        }
}
