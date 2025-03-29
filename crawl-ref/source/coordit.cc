/**
 * @file
 * @brief Coordinate iterators.
**/

#include "AppHdr.h"

#include "coordit.h"

#include "coord.h"
#include "libutil.h"
#include "losglobal.h"
#include "los.h"
#include "random.h"

rectangle_iterator::rectangle_iterator(const coord_def& corner1,
                                        const coord_def& corner2)
{
    topleft.x = min(corner1.x, corner2.x);
    topleft.y = min(corner1.y, corner2.y); // not really necessary
    bottomright.x = max(corner1.x, corner2.x);
    bottomright.y = max(corner1.y, corner2.y);
    current = topleft;
}

rectangle_iterator::rectangle_iterator(const coord_def& center, int halfside,
                                       bool clip_to_map)
{
    topleft.x = center.x - halfside;
    topleft.y = center.y - halfside;
    if (clip_to_map)
    {
        topleft.x = max(topleft.x, X_BOUND_1);
        topleft.y = max(topleft.y, Y_BOUND_1);
    }
    bottomright.x = center.x + halfside;
    bottomright.y = center.y + halfside;
    if (clip_to_map)
    {
        bottomright.x = min(bottomright.x, X_BOUND_2);
        bottomright.y = min(bottomright.y, Y_BOUND_2);
    }
    current = topleft;
}

rectangle_iterator::rectangle_iterator(int x_border_dist, int y_border_dist)
{
    if (y_border_dist < 0)
        y_border_dist = x_border_dist;

    topleft.set(x_border_dist, y_border_dist);
    bottomright.set(GXM - x_border_dist - 1, GYM - y_border_dist - 1);
    current = topleft;
}

rectangle_iterator::operator bool() const
{
    return current.y <= bottomright.y;
}

coord_def rectangle_iterator::operator *() const
{
    return current;
}

const coord_def* rectangle_iterator::operator->() const
{
    return &current;
}

void rectangle_iterator::operator ++()
{
    if (current.x == bottomright.x)
    {
        current.x = topleft.x;
        current.y++;
    }
    else
        current.x++;
}

void rectangle_iterator::operator++(int)
{
    ++(*this);
}

random_rectangle_iterator::random_rectangle_iterator(const coord_def& corner1,
                                                     const coord_def& corner2)
{
    int left   = min(corner1.x, corner2.x);
    int right  = max(corner1.x, corner2.x);
    int top    = min(corner1.y, corner2.y);
    int bottom = max(corner1.y, corner2.y);

    top_left.x = left;
    top_left.y = top;

    for (int y = top; y <= bottom; y++)
        for (int x = left; x <= right; x++)
            remaining.emplace_back(x, y);

    if (remaining.empty())
        current = 0;
    else
        current = random2(remaining.size());
}

random_rectangle_iterator::random_rectangle_iterator(int x_border_dist,
                                                     int y_border_dist)
{
    if (y_border_dist < 0)
        y_border_dist = x_border_dist;

    int right  = GXM - x_border_dist - 1;
    int bottom = GYM - y_border_dist - 1;

    top_left.x = x_border_dist;
    top_left.y = y_border_dist;

    for (int y = y_border_dist; y <= bottom; y++)
        for (int x = x_border_dist; x <= right; x++)
            remaining.emplace_back(x, y);

    if (remaining.empty())
        current = 0;
    else
        current = random2(remaining.size());
}

random_rectangle_iterator::operator bool() const
{
    return !remaining.empty();
}

coord_def random_rectangle_iterator::operator *() const
{
    if (remaining.empty())
        return top_left;
    else
        return remaining[current];
}

const coord_def* random_rectangle_iterator::operator->() const
{
    if (remaining.empty())
        return &top_left;
    else
        return &(remaining[current]);
}

void random_rectangle_iterator::operator ++()
{
    if (!remaining.empty())
    {
        remaining[current] = remaining.back();
        remaining.pop_back();
        if (!remaining.empty())
            current = random2(remaining.size());
    }
}

void random_rectangle_iterator::operator++(int)
{
    ++(*this);
}

/*
 *  radius iterator
 */
radius_iterator::radius_iterator(const coord_def _center, int r,
                                 circle_type ctype,
                                 bool _exclude_center)
    : state(RI_START),
      center(_center),
      los(LOS_NONE)
{
    ASSERT(map_bounds(_center));
    switch (ctype)
    {
    case C_CIRCLE: credit = r; break;
    case C_POINTY: credit = r * r; break;
    case C_ROUND:  credit = r * r + 1; break;
    case C_SQUARE: credit = r; break;
    }
    is_square = (ctype == C_SQUARE);
    ++(*this);
    if (_exclude_center)
        ++(*this);
}

radius_iterator::radius_iterator(const coord_def _center,
                                 los_type _los,
                                 bool _exclude_center)
    : state(RI_START),
      center(_center),
      los(_los)
{
    ASSERT(map_bounds(_center));
    credit = get_los_radius();
    is_square = true;
    ++(*this);
    if (_exclude_center)
        ++(*this);
}

radius_iterator::radius_iterator(const coord_def _center,
                                 int r,
                                 circle_type ctype,
                                 los_type _los,
                                 bool _exclude_center)
    : state(RI_START),
      center(_center),
      los(_los)
{
    ASSERT(map_bounds(_center));
    switch (ctype)
    {
    case C_CIRCLE: credit = r; break;
    case C_POINTY: credit = r * r; break;
    case C_ROUND:  credit = r * r + 1; break;
    case C_SQUARE: credit = r; break;
    }
    is_square = (ctype == C_SQUARE);
    ++(*this);
    if (_exclude_center)
        ++(*this);
}

radius_iterator::operator bool() const
{
    return state;
}

coord_def radius_iterator::operator *() const
{
    return current;
}

const coord_def* radius_iterator::operator->() const
{
    return &current;
}

#define coreturn(id) { state = id; return; case id:; }
#define cobegin(id)  switch (state) { case id:
#define coend(id)    coreturn(id); }
#define ret_coord(dx, dy, id) do                                \
    {                                                           \
        current.x = center.x + (dx);                            \
        current.y = center.y + (dy);                            \
        if (!los || cell_see_cell(center, current, los))        \
            coreturn(id);                                       \
    } while (0)

void radius_iterator::operator++()
{
    cobegin(RI_START);

    base_cost = is_square ? 1 : -1;
    inc_cost = is_square ? 0 : 2;

    y = 0;
    cost_y = base_cost;
    credit_y = credit;

    do
    {
        x = 0;
        cost_x = base_cost;
        credit_x = (is_square ? credit : credit_y);

        do
        {
            if (x + center.x < GXM)
            {
                if (y + center.y < GYM)
                    ret_coord( x,  y, RI_SE);
                if (y && y <= center.y)
                    ret_coord( x, -y, RI_NE);
            }
            if (x && x <= center.x)
            {
                if (y + center.y < GYM)
                    ret_coord(-x,  y, RI_SW);
                if (y && y <= center.y)
                    ret_coord(-x, -y, RI_NW);
            }
            x++;
            credit_x -= (cost_x += inc_cost);
        } while (credit_x >= 0);

        y++;
        credit_y -= (cost_y += inc_cost);
    } while (credit_y >= 0);

    coend(RI_DONE);
}

void radius_iterator::operator++(int)
{
    ++(*this);
}

void vision_iterator::operator++()
{
    do
    {
        radius_iterator::operator++();
    }
    while (*this && !who.see_cell(**this));
}

void vision_iterator::operator++(int)
{
    ++(*this);
}

/*
 *  adjacent iterator
 */
extern const struct coord_def Compass[9];

adjacent_iterator::operator bool() const
{
    return i >= 0;
}

coord_def adjacent_iterator::operator *() const
{
    return val;
}

const coord_def* adjacent_iterator::operator->() const
{
    return &val;
}

void adjacent_iterator::operator ++()
{
    while (--i >= 0)
    {
        val = center + Compass[i];
        if (map_bounds(val))
            return;
    }
}

void adjacent_iterator::operator++(int)
{
    ++(*this);
}

/*
 *  spiral iterator
 */
distance_iterator::distance_iterator(const coord_def& _center, bool _fair,
                                 bool exclude_center, int _max_radius) :
    center(_center), current(_center), r(0), max_radius(_max_radius),
    threshold(0), icur(0), iend(0), fair(_fair)
{
    vcur  = lists + 0;
    vnear = lists + 1;
    vfar  = lists + 2;

    for (int dx = -1; dx <= 1; dx++)
        for (int dy = -1; dy <= 1; dy++)
            if (dx || dy)
                vnear->emplace_back(dx, dy);

    if (exclude_center)
        advance();
}

bool distance_iterator::advance()
{
    while (true)
    {
        if (++icur >= vcur->size())
            icur = 0;
        if (icur == iend)
        {
            // Advance to the next radius.
            vector<coord_def> *tmp = vcur;
            vcur = vnear;
            vnear = vfar;
            vfar = tmp;
            tmp->clear();

            if (!vcur->size())
                return false;
            // Randomize the order various directions are returned.
            // Just the initial angle is enough.
            if (fair)
                icur = iend = random2(vcur->size());
            else
                icur = iend = 0; // randomness is costly

            if (r++ >= max_radius)
            {
                vcur->clear();
                return false;
            }
            threshold = r+1;
        }

        coord_def d = (*vcur)[icur];
        if (in_bounds(current = center + d))
        {
            ASSERT(d.x || d.y);
            if (!d.y)
                push_neigh(d, sgn(d.x), 0);
            if (!d.x)
                push_neigh(d, 0, sgn(d.y));
            if (d.x <= 0)
            {
                if (d.y <= 0)
                    push_neigh(d, -1, -1);
                if (d.y >= 0)
                    push_neigh(d, -1, +1);
            }
            if (d.x >= 0)
            {
                if (d.y <= 0)
                    push_neigh(d, +1, -1);
                if (d.y >= 0)
                    push_neigh(d, +1, +1);
            }

            return true;
        }
    }
}

void distance_iterator::push_neigh(coord_def d, int dx, int dy)
{
    d.x += dx;
    d.y += dy;
    ((d.rdist() <= threshold) ? vnear : vfar)->push_back(d);
}

distance_iterator::operator bool() const
{
    return in_bounds(current) && r <= max_radius;
}

coord_def distance_iterator::operator *() const
{
    return current;
}

const coord_def* distance_iterator::operator->() const
{
    return &current;
}

const distance_iterator& distance_iterator::operator++()
{
    advance();
    return *this;
}

void distance_iterator::operator++(int)
{
    ++(*this);
}

int distance_iterator::radius() const
{
    return r;
}

static void _set_number_at_index(std::uint_fast32_t& numbers,
    std::uint_fast8_t index, std::uint_fast8_t value) noexcept
{
    std::uint_fast32_t mask = 15 << (index << 2);
    mask = ~mask;
    numbers = (numbers & mask) | (value << (index << 2));
}

static std::uint_fast8_t _get_number_at_index(std::uint_fast32_t numbers,
    std::uint_fast8_t index) noexcept
{
    return (numbers >> (index << 2)) & 15;
}

static coord_def _unpack_coord(coord_def center, std::uint_fast32_t packed) noexcept
{
    int x = packed & 3;
    x -= 1;
    int y = (packed >> 2) & 3;
    y -= 1;
    return center + coord_def(x, y);
}

fair_adjacent_iterator::fair_adjacent_iterator(coord_def _center) :
    center(_center)
{
    remaining_count = 0;
    remaining = 0;
    for (std::uint_fast8_t x = 0; x < 3; x++)
    {
        for (std::uint_fast8_t y = 0; y < 3; y++)
        {
            if (x == 1 && y == 1)
                continue;
            std::uint_fast32_t value = x | y << 2;
            if (!in_bounds(_unpack_coord(center, value)))
                continue;
            remaining |= value << (remaining_count << 2);
            remaining_count++;
        }
    }

    std::uint_fast8_t rand_index = random2(remaining_count);
    std::uint_fast8_t rand_value = _get_number_at_index(remaining, rand_index);
    std::uint_fast8_t first_value = _get_number_at_index(remaining, 0);
    _set_number_at_index(remaining, rand_index, first_value);
    _set_number_at_index(remaining, 0, rand_value);
}

fair_adjacent_iterator::operator bool() const noexcept
{
    return remaining_count != 0;
}

coord_def fair_adjacent_iterator::operator *() const noexcept
{
    return _unpack_coord(center, remaining);
}

void fair_adjacent_iterator::operator++()
{
    if (!remaining_count)
        return;
    std::uint_fast8_t rand_index = random2(remaining_count) + 1;
    std::uint_fast8_t rand_value = _get_number_at_index(remaining, rand_index);
    _set_number_at_index(remaining, 0, rand_value);
    std::uint_fast8_t last_value = _get_number_at_index(remaining,
        remaining_count);
    _set_number_at_index(remaining, rand_index, last_value);
    remaining_count--;
}

void fair_adjacent_iterator::operator++(int)
{
    ++(*this);
}

/********************/
/* regression tests */
/********************/
#ifdef DEBUG_TESTS
static void _test_ai(const coord_def c, bool exc, size_t expected)
{
    set<coord_def> seen;

    for (adjacent_iterator ai(c, exc); ai; ++ai)
    {
        if (seen.count(*ai))
            die("adjacent_iterator: %d,%d seen twice", ai->x, ai->y);
        seen.insert(*ai);

        if (c == *ai && !exc)
            continue;
        if ((c - *ai).rdist() != 1)
        {
            die("adjacent_iterator: %d,%d not adjacent to %d,%d",
                ai->x, ai->y, c.x, c.y);
        }
    }

    if (seen.size() != expected)
    {
        die("adjacent_iterator(%d,%d): seen %d, expected %d",
            c.x, c.y, (int)seen.size(), (int)expected);
    }
}

void coordit_tests()
{
    // bounding box of our playground
    #define BC   16
    #define BBOX 32
    int los_radius = get_los_radius();
    ASSERT(los_radius < BC - 2);
    coord_def center(BC, BC);

    FixedBitArray<BBOX, BBOX> seen;

    for (int r = 0; r <= los_radius * los_radius + 1; ++r)
    {
        seen.reset();

        for (radius_iterator ri(center, r, C_CIRCLE); ri; ++ri)
        {
            if (seen(*ri))
                die("radius_iterator(C%d): %d,%d seen twice", r, ri->x, ri->y);
            seen.set(*ri);
        }

        for (int x = 0; x < BBOX; x++)
            for (int y = 0; y < BBOX; y++)
            {
                bool in = sqr(x - BC) + sqr(y - BC) <= r;
                if (seen(coord_def(x, y)) != in)
                {
                    die("radius_iterator(C%d) mismatch at %d,%d: %d != %d",
                        r, x, y, seen(coord_def(x, y)), in);
                }
            }
    }

    for (radius_iterator ri(coord_def(2, 2), 5, C_ROUND); ri; ++ri)
        if (!map_bounds(*ri))
            die("radius_iterator(R5) out of bounds at %d, %d", ri->x, ri->y);
    for (radius_iterator ri(coord_def(GXM - 1, GYM - 1), 7, C_ROUND); ri; ++ri)
        if (!map_bounds(*ri))
            die("radius_iterator(R7) out of bounds at %d, %d", ri->x, ri->y);

    seen.reset();
    int rd = 0;
    for (distance_iterator di(center, true, false, BC - 1); di; ++di)
    {
        if (seen(*di))
            die("distance_iterator: %d,%d seen twice", di->x, di->y);
        seen.set(*di);

        int rc = (center - *di).rdist();
        if (rc < rd)
            die("distance_iterator went backwards");
        rd = rc;
    }

    for (int x = 0; x < BBOX; x++)
        for (int y = 0; y < BBOX; y++)
        {
            bool in = max(abs(x - BC), abs(y - BC)) <= BC - 1;
            if (seen(coord_def(x, y)) != in)
            {
                die("distance_iterator mismatch at %d,%d: %d != %d",
                    x, y, seen(coord_def(x, y)), in);
            }
        }

    _test_ai(center, false, 9);
    _test_ai(center, true, 8);
    _test_ai(coord_def(3, 0), false, 6);
    _test_ai(coord_def(3, 0), true, 5);
    _test_ai(coord_def(0, 0), false, 4);
    _test_ai(coord_def(0, 0), true, 3);
    _test_ai(coord_def(GXM, GYM), false, 1);
    _test_ai(coord_def(GXM, GYM), true, 1);
}
#endif
