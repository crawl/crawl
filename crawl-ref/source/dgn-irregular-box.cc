/**
 * @file
 * @brief Builds irregular boxes (library "dgn").
**/

#include "AppHdr.h"
#include <vector>

#include "coord.h"
#include "mapdef.h"
#include "random.h"
#include "dgn-irregular-box.h"


// Adds a simple hollow box to the map with the specified
//  coordinates, glyphs, and number of doors.  This is the
//  fallback if we can't place a irregular box.
static void _make_simple_box(map_lines& map, int x1, int y1, int x2, int y2,
                             char floor_glyph, char wall_glyph,
                             char door_glyph, int door_count)
{
    // inside
    for (int x = x1 + 1; x < x2; x++)
        for (int y = y1 + 1; y < y2; y++)
            map(x, y) = floor_glyph;

    // walls
    for (int x = x1; x <= x2; x++)
    {
        map(x, y1) = wall_glyph;
        map(x, y2) = wall_glyph;
    }
    for (int y = y1 + 1; y < y2; y++)
    {
        map(x1, y) = wall_glyph;
        map(x2, y) = wall_glyph;
    }

    // doors
    if (door_count > 0)
    {
        vector<coord_def> cells;
        for (int x = x1; x <= x2; x++)
        {
            cells.push_back(coord_def(x, y1));
            cells.push_back(coord_def(x, y2));
        }
        for (int y = y1 + 1; y < y2; y++)
        {
            cells.push_back(coord_def(x1, y));
            cells.push_back(coord_def(x2, y));
        }

        for (int i = 0; i < door_count && !cells.empty(); i++)
        {
            unsigned int index = random2(cells.size());
            map(cells[index].x, cells[index].y) = door_glyph;
            cells[index] = cells.back();
            cells.pop_back();
        }
    }
}

// Randomly reduce two values until their sum is less than or
//  equal to a third value
static void _randomly_force_sum_below(int& a, int& b, int sum)
{
    if (sum <= 0)
        return;

    while (a + b >= sum)
    {
        if (random2(2) == 0)
            a = random2(a);
        else
            b = random2(b);
    }
}

// Calculates a vector of up to a random number of values
//  between the specified minimum and maximum values.  The
//  minimum and maximum values are always inserted.  All
//  other values inserted will be separated by at least the
//  specified separation.  The values returned are sorted in
//  ascending order.
static vector<int> _calculate_random_values(int min_value, int max_value,
                                            int max_count,
                                            int min_separation)
{
    vector<int> values;

    values.push_back(min_value);
    values.push_back(max_value);
    for (int i = 0; i < max_count; i++)
    {
        int chosen = random_range(min_value, max_value);

        // modified insertion sort
        for (unsigned int j = 0; j < values.size(); j++)
        {
            int permitted_below = values[j] - min_separation;
            int permitted_above = values[j] + min_separation;
            if (chosen < permitted_above)
            {
                if (chosen > permitted_below)
                {
                    // too close to an existing value, so do not insert
                    break;
                }
                else
                {
                    // insert before
                    values.push_back(values.back());
                    // not that by here, values.size() is always >= 3
                    for (unsigned int k = values.size() - 2; k > j; k--)
                        values[k] = values[k - 1];
                    values[j] = chosen;
                    break;
                }
            }
            // else keep looking
        }
    }

    return values;
}

// Calculates a vector of the specified number of random
//  values.  The first and last elements are set to the
//  specified values.  After this, one value at random is
//  set to 0.  If count <= 1, this function returns a vector
//  containg the single value 0.
static vector<int> _calculate_random_wall_distances(int first_value,
                                                    int last_value,
                                                    int max_value,
                                                    int count)
{
    if (count < 1)
        count = 1;

    vector<int> values(count, 0);

    values[0] = first_value;
    for (int i = 1; i < count; i++)
        values[i] = random2(max_value);
    values[count - 1] = (last_value);

    values[random2(values.size())] = 0;

    return values;
}

//
// These functions draw the sides of the box we calculated to
//  our temporary map.  They also add the positions of each
//  non-corner wall cell to our list so we can use them to add
//  doors.
//
// We have 4 similar segemets here, one for each side
//  of the box.  Sadly, they are different enough to
//  make combining them into a single function
//  difficult.  Each segment does the following:
//  -> Build a wall for this division
//  -> Build a wall connectiing the corner of the
//     previous division to the start of this division.
//     The first division does not do this, for obvious
//     reasons.
//

static void _draw_wall_t(vector<vector<char> >& map,
                         vector<coord_def>& wall_cells,
                         const vector<int>& di_t,
                         const vector<int>& in_t,
                         char wall_glyph)
{
    for (unsigned int i = 0; i < in_t.size(); i++)
    {
        if (i > 0)
        {
            // connect to previous division
            int x = di_t[i];
            int ym = in_t[i];
            int yp = in_t[i - 1];
            if (ym > yp)
                swap(ym, yp);
            for (int y = ym + 1; y < yp; y++)
            {
                map[x][y] = wall_glyph;
                wall_cells.push_back(coord_def(x, y));
            }
        }

        // add this division
        int y = in_t[i];
        map[di_t[i    ]][y] = wall_glyph;
        map[di_t[i + 1]][y] = wall_glyph;
        for (int x = di_t[i] + 1; x < di_t[i + 1]; x++)
        {
            map[x][y] = wall_glyph;
            wall_cells.push_back(coord_def(x, y));
        }
    }
}

static void _draw_wall_b(vector<vector<char> >& map,
                         vector<coord_def>& wall_cells,
                         const vector<int>& di_b,
                         const vector<int>& in_b,
                         char wall_glyph,
                         int size_y)
{
    for (unsigned int i = 0; i < in_b.size(); i++)
    {
        if (i > 0)
        {
            // connect to previous division
            int x = di_b[i];
            int ym = size_y - 1 - in_b[i];
            int yp = size_y - 1 - in_b[i - 1];
            if (ym > yp)
                swap(ym, yp);
            for (int y = ym + 1; y < yp; y++)
            {
                map[x][y] = wall_glyph;
                wall_cells.push_back(coord_def(x, y));
            }
        }

        // add this division
        int y = size_y - 1 - in_b[i];
        map[di_b[i    ]][y] = wall_glyph;
        map[di_b[i + 1]][y] = wall_glyph;
        for (int x = di_b[i] + 1; x < di_b[i + 1]; x++)
        {
            map[x][y] = wall_glyph;
            wall_cells.push_back(coord_def(x, y));
        }
    }
}

static void _draw_wall_l(vector<vector<char> >& map,
                         vector<coord_def>& wall_cells,
                         const vector<int>& di_l,
                         const vector<int>& in_l,
                         char wall_glyph)
{
    for (unsigned int i = 0; i < in_l.size(); i++)
    {
        if (i > 0)
        {
            // connect to previous division
            int y = di_l[i];
            int xm = in_l[i];
            int xp = in_l[i - 1];
            if (xm > xp)
                swap(xm, xp);
            for (int x = xm + 1; x < xp; x++)
            {
                map[x][y] = wall_glyph;
                wall_cells.push_back(coord_def(x, y));
            }
        }

        // add this division
        int x = in_l[i];
        map[x][di_l[i    ]] = wall_glyph;
        map[x][di_l[i + 1]] = wall_glyph;
        for (int y = di_l[i] + 1; y < di_l[i + 1]; y++)
        {
            map[x][y] = wall_glyph;
            wall_cells.push_back(coord_def(x, y));
        }
    }
}

static void _draw_wall_r(vector<vector<char> >& map,
                         vector<coord_def>& wall_cells,
                         const vector<int>& di_r,
                         const vector<int>& in_r,
                         char wall_glyph,
                         int size_x)
{
    for (unsigned int i = 0; i < in_r.size(); i++)
    {
        if (i > 0)
        {
            // connect to previous division
            int y = di_r[i];
            int xm = size_x - 1 - in_r[i];
            int xp = size_x - 1 - in_r[i - 1];
            if (xm > xp)
                swap(xm, xp);
            for (int x = xm + 1; x < xp; x++)
            {
                map[x][y] = wall_glyph;
                wall_cells.push_back(coord_def(x, y));
            }
        }

        // add this division
        int x = size_x - 1 - in_r[i];
        map[x][di_r[i    ]] = wall_glyph;
        map[x][di_r[i + 1]] = wall_glyph;
        for (int y = di_r[i] + 1; y < di_r[i + 1]; y++)
        {
            map[x][y] = wall_glyph;
            wall_cells.push_back(coord_def(x, y));
        }
    }
}

// Flood-fills our approximate map starting from the specified
//  location.  All glyphs of the specified value at that point
//  and connected are filled with the specified replacement
//  glyph.
static void _flood_fill(vector<vector<char> >& map, int start_x, int start_y,
                        char old_glyph, char new_glyph)
{
    if (map.empty() || map[0].empty())
        return;
    if (map[start_x][start_y] != old_glyph)
        return;
    if (old_glyph == new_glyph)
        return;

    // We will use a stack for the glyphs still to replace.  We
    //  will replace glyphs as we add them to the stack to
    //  avoid adding them twice.
    vector<coord_def> stack;

    map[start_x][start_y] = new_glyph;
    stack.push_back(coord_def(start_x, start_y));
    while (!stack.empty())
    {
        int x = stack.back().x;
        int y = stack.back().y;
        stack.pop_back();

        // add neighbours
        if (x > 0 && map[x - 1][y] == old_glyph)
        {
            map[x - 1][y] = new_glyph;
            stack.push_back(coord_def(x - 1, y));
        }
        if (x + 1 < (int)(map.size()) && map[x + 1][y] == old_glyph)
        {
            map[x + 1][y] = new_glyph;
            stack.push_back(coord_def(x + 1, y));
        }
        if (y > 0 && map[x][y - 1] == old_glyph)
        {
            map[x][y - 1] = new_glyph;
            stack.push_back(coord_def(x, y - 1));
        }
        if (y + 1 < (int)(map[0].size()) && map[x][y + 1] == old_glyph)
        {
            map[x][y + 1] = new_glyph;
            stack.push_back(coord_def(x, y + 1));
        }
    }
}

//
// Fills the outside of the the box in the specified map with
//  the specified glyphs.  Only glyphs of the specified value
//  are replaced.
//
// The outside is not guaranteed to be connected we fill each
//  point on each edge inwards in a straight line until it hits
//  something other than the glyph to replace (or the outside
//  glyph from before).  We made the irregularities by moving
//  parts of the box edge inwards, so there will be no hidden
//  nooks to look out for.
//
static void _fill_outside(vector<vector<char> >& map,
                          char old_glyph, char outside_glyph)
{
    int max_x = map.size()    - 1;
    int max_y = map[0].size() - 1;

    // top and bottom
    for (int x = 0; x <= max_x; x++)
    {
        for (int y = 0; y <= max_y; y++)
        {
            if (map[x][y] == old_glyph || map[x][y] == outside_glyph)
                map[x][y] = outside_glyph;
            else
                break;
        }
        for (int y = max_y; y >= 0; y--)
        {
            if (map[x][y] == old_glyph || map[x][y] == outside_glyph)
                map[x][y] = outside_glyph;
            else
                break;
        }
    }

    // left and right
    for (int y = 0; y <= max_y; y++)
    {
        for (int x = 0; x <= max_x; x++)
        {
            if (map[x][y] == old_glyph || map[x][y] == outside_glyph)
                map[x][y] = outside_glyph;
            else
                break;
        }
        for (int x = max_x; x >= 0; x--)
        {
            if (map[x][y] == old_glyph || map[x][y] == outside_glyph)
                map[x][y] = outside_glyph;
            else
                break;
        }
    }
}



void make_irregular_box(map_lines& map, int x1, int y1, int x2, int y2,
                        int di_x, int di_y, int in_x, int in_y,
                        char floor_glyph, char wall_glyph,
                        char door_glyph, int door_count)
{
    const int MIN_SEPARATION = 2; // < 2 can block off part of box (bad!)
    const int MIN_IRREGULAR_SIZE = MIN_SEPARATION * 2 + 1;

    const char UNSET_GLYPH = '\0';
    const char OUTSIDE_GLYPH = '\a';

    // if we have no map, just give up
    if (map.width() <= 0 || map.height() <= 0)
        return;

    // enforce preconditions
    if (x1 < 0)
        x1 = 0;
    if (x2 < x1)
        x2 = x1;
    if (x2 >= map.width())
        x2 = map.width() - 1;
    if (y1 < 0)
        y1 = 0;
    if (y2 < y1)
        y2 = y1;
    if (y2 >= map.height())
        y2 = map.height() - 1;
    if (di_x < 0)
        di_x = 0;
    if (di_y < 0)
        di_y = 0;
    if (in_x < 0)
        in_x = 0;
    if (in_y < 0)
        in_y = 0;
    if (door_count < 0)
        door_count = 0;

    // calculate box size
    int size_x = x2 - x1 + 1;
    int size_y = y2 - y1 + 1;

    // limit in_??? values to half box size minus some for inside
    //  -> There must be enough room left for 2 walls and one floor
    if (in_x > (size_x - 1) / 2 - 1)
        in_x = (size_x - 1) / 2 - 1;
    if (in_y > (size_y - 1) / 2 - 1)
        in_y = (size_y - 1) / 2 - 1;

    // irregular boxes do not work if too small
    if (size_x < MIN_IRREGULAR_SIZE || size_y < MIN_IRREGULAR_SIZE)
    {
        _make_simple_box(map, x1, y1, x2, y2,
                         floor_glyph, wall_glyph, door_glyph, door_count);
        return;
    }

    vector<vector<char> > new_glyphs;
    bool regenerate_needed = true;

    // Overlapping corners are really hard to detect beforehand
    //  and quite rare, so we will just regenerate the whole
    //  room if they happen.
    while (regenerate_needed)
    {
        new_glyphs.assign(size_x, vector<char>(size_y, UNSET_GLYPH));

        //
        // Choose corner positions
        //  -> these must leave an actual box in the middle
        //  -> the box may stick out past these
        //

        coord_def in_tl(random2(in_x), random2(in_y));
        coord_def in_tr(random2(in_x), random2(in_y));
        coord_def in_bl(random2(in_x), random2(in_y));
        coord_def in_br(random2(in_x), random2(in_y));
        _randomly_force_sum_below(in_tl.x, in_tr.x, size_x - MIN_IRREGULAR_SIZE);
        _randomly_force_sum_below(in_bl.x, in_br.x, size_x - MIN_IRREGULAR_SIZE);
        _randomly_force_sum_below(in_tl.y, in_bl.y, size_y - MIN_IRREGULAR_SIZE);
        _randomly_force_sum_below(in_tr.y, in_br.y, size_y - MIN_IRREGULAR_SIZE);

        //
        // Place divisions along edges
        //  -> at the ends, we have the corners
        //  -> insert random divisions between
        //  -> no division may be less than MIN_SEPARATION from another
        //  -> we keep the lists sorted
        //

        vector<int> di_t = _calculate_random_values(in_tl.x, size_x - 1 - in_tr.x,
                                                     di_x, MIN_SEPARATION);
        vector<int> di_b = _calculate_random_values(in_bl.x, size_x - 1 - in_br.x,
                                                     di_x, MIN_SEPARATION);
        vector<int> di_l = _calculate_random_values(in_tl.y, size_y - 1 - in_bl.y,
                                                     di_y, MIN_SEPARATION);
        vector<int> di_r = _calculate_random_values(in_tr.y, size_y - 1 - in_br.y,
                                                     di_y, MIN_SEPARATION);

        //
        // Calculate wall distance from maximal box bounds
        //  -> we already have the end segments
        //  -> the rest are random
        //  -> at least one must have a value of 0
        //

        vector<int> in_t = _calculate_random_wall_distances(in_tl.y, in_tr.y,
                                                            in_y,
                                                            di_t.size() - 1);
        vector<int> in_b = _calculate_random_wall_distances(in_bl.y, in_br.y,
                                                            in_y,
                                                            di_b.size() - 1);
        vector<int> in_l = _calculate_random_wall_distances(in_tl.x, in_bl.x,
                                                            in_x,
                                                            di_l.size() - 1);
        vector<int> in_r = _calculate_random_wall_distances(in_tr.x, in_br.x,
                                                            in_x,
                                                            di_r.size() - 1);

        //
        // Reconnect corners if needed
        //  -> they could be wrong if a corner cell was moved
        //     to the box edge above
        //

        di_t.front() = in_l.front();
        di_t.back()  = size_x - 1 - in_r.front();
        di_b.front() = in_l.back();
        di_b.back()  = size_x - 1 - in_r.back();
        di_l.front() = in_t.front();
        di_l.back()  = size_y - 1 - in_b.front();
        di_r.front() = in_t.back();
        di_r.back()  = size_y - 1 - in_b.back();

        //
        // Draw the box we calculated to our temporary map.
        //
        // We do not use the real map directly here because:
        //  -> We need to be able to use the whole grid,
        //     whereas the room might only change some cells
        //  -> The box might be self-intersecting, in which
        //     case we need the original back
        //
        // There is additional complexity because we store the
        //  position of every non-corner wall cell.  Then, we
        //  use these to add doors.
        //

        vector<coord_def> wall_cells;

        _draw_wall_t(new_glyphs, wall_cells, di_t, in_t, wall_glyph);
        _draw_wall_b(new_glyphs, wall_cells, di_b, in_b, wall_glyph, size_y);
        _draw_wall_l(new_glyphs, wall_cells, di_l, in_l, wall_glyph);
        _draw_wall_r(new_glyphs, wall_cells, di_r, in_r, wall_glyph, size_x);

        // add doors
        for (int i = 0; i < door_count && !wall_cells.empty(); i++)
        {
            unsigned int index = random2(wall_cells.size());
            new_glyphs[wall_cells[index].x][wall_cells[index].y] = '+';
            wall_cells[index] = wall_cells.back();
            wall_cells.pop_back();
        }

        //
        // To check if the box is acceptable (i.e. has no
        //  self-intersections), we fill the inside of the box
        //  with floor and the outside with OUTSIDE_GLYPH.  If
        //  that leaves no UNSET_GLYPH cells, the box is good.
        //

        _flood_fill(new_glyphs, in_l[0] + 1, in_t[0] + 1,
                    UNSET_GLYPH, floor_glyph);
        _fill_outside(new_glyphs, UNSET_GLYPH, OUTSIDE_GLYPH);

        regenerate_needed = false;
        for (int x = 0; x < size_x && !regenerate_needed; x++)
            for (int y = 0; y < size_y; y++)
                if (new_glyphs[x][y] == UNSET_GLYPH)
                {
                    regenerate_needed = true;
                    break;
                }

    }  // end of generation loop

    //
    // Copy the finished box onto the map
    //  -> we don't copy GLYPH_OUTSIDE values
    //  -> we are finally finished!
    //

    for (int x = 0; x < size_x; x++)
        for (int y = 0; y < size_y; y++)
        {
            if (new_glyphs[x][y] == UNSET_GLYPH)
                dprf("Error in make_irregular_box: UNSET_GLYPH in final box");
            else if (new_glyphs[x][y] != OUTSIDE_GLYPH)
                map(x1 + x, y1 + y) = new_glyphs[x][y];
        }
}
