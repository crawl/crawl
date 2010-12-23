/*
 *  File:       externs.h
 *  Summary:    Fixed size 2D vector class that asserts if you do something bad.
 *  Written by: Linley Henzell
 */

#ifndef EXTERNS_H
#define EXTERNS_H

#include <vector>
#include <list>
#include <string>
#include <map>
#include <set>
#include <memory>
#include <cstdlib>
#include <deque>

#include <time.h>
#include <stdint.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "defines.h"
#include "enum.h"
#include "fixedarray.h"
#include "mpr.h"
#include "store.h"

typedef FixedArray<dungeon_feature_type, GXM, GYM> feature_grid;
typedef FixedArray<unsigned short, GXM, GYM> map_mask;

struct coord_def;

template <typename Z> inline Z sgn(Z x)
{
    return (x < 0 ? -1 : (x > 0 ? 1 : 0));
}

inline int dist_range(int x) { return x*x + 1; };

struct coord_def
{
    int         x;
    int         y;

    explicit coord_def(int x_in = 0, int y_in = 0) : x(x_in), y(y_in) { }

    void set(int xi, int yi)
    {
        x = xi;
        y = yi;
    }

    void reset()
    {
        set(0, 0);
    }

    int distance_from(const coord_def &b) const;

    bool operator == (const coord_def &other) const
    {
        return x == other.x && y == other.y;
    }

    bool operator != (const coord_def &other) const
    {
        return !operator == (other);
    }

    bool operator <  (const coord_def &other) const
    {
        return (x < other.x) || (x == other.x && y < other.y);
    }

    bool operator >  (const coord_def &other) const
    {
        return (x > other.x) || (x == other.x && y > other.y);
    }

    const coord_def &operator += (const coord_def &other)
    {
        x += other.x;
        y += other.y;
        return (*this);
    }

    const coord_def &operator += (int offset)
    {
        x += offset;
        y += offset;
        return (*this);
    }

    const coord_def &operator -= (const coord_def &other)
    {
        x -= other.x;
        y -= other.y;
        return (*this);
    }

    const coord_def &operator -= (int offset)
    {
        x -= offset;
        y -= offset;
        return (*this);
    }

    const coord_def &operator /= (int div)
    {
        x /= div;
        y /= div;
        return (*this);
    }

    const coord_def &operator *= (int mul)
    {
        x *= mul;
        y *= mul;
        return (*this);
    }

    coord_def operator + (const coord_def &other) const
    {
        coord_def copy = *this;
        return (copy += other);
    }

    coord_def operator + (int other) const
    {
        coord_def copy = *this;
        return (copy += other);
    }

    coord_def operator - (const coord_def &other) const
    {
        coord_def copy = *this;
        return (copy -= other);
    }

    coord_def operator -() const
    {
        return (coord_def(0, 0) - *this);
    }

    coord_def operator - (int other) const
    {
        coord_def copy = *this;
        return (copy -= other);
    }

    coord_def operator / (int div) const
    {
        coord_def copy = *this;
        return (copy /= div);
    }

    coord_def operator * (int mul) const
    {
        coord_def copy = *this;
        return (copy *= mul);
    }

    coord_def sgn() const
    {
        return coord_def(::sgn(x), ::sgn(y));
    }

    int abs() const
    {
        return (x * x + y * y);
    }

    int rdist() const
    {
        return (std::max(std::abs(x), std::abs(y)));
    }

    bool origin() const
    {
        return (!x && !y);
    }

    bool zero() const
    {
        return origin();
    }

    bool equals(const int xi, const int yi) const
    {
        return (xi == x && yi == y);
    }
};
const coord_def INVALID_COORD(-1, -1);

typedef bool (*coord_predicate)(const coord_def &c);

struct run_check_dir
{
    dungeon_feature_type grid;
    coord_def delta;
};

// Identifies a level. Should never include virtual methods or
// dynamically allocated memory (see code to push level_id onto Lua
// stack in l_dgn.cc)
class level_id
{
public:
    branch_type branch;     // The branch in which the level is.
    int depth;              // What depth (in this branch - starting from 1)
    level_area_type level_type;

public:
    // Returns the level_id of the current level.
    static level_id current();

    // Returns the level_id of the level that the stair/portal/whatever at
    // 'pos' on the current level leads to.
    static level_id get_next_level_id(const coord_def &pos);

    level_id()
        : branch(BRANCH_MAIN_DUNGEON), depth(-1),
          level_type(LEVEL_DUNGEON)
    {
    }
    level_id(branch_type br, int dep, level_area_type ltype = LEVEL_DUNGEON)
        : branch(br), depth(dep), level_type(ltype)
    {
    }
    level_id(const level_id &ot)
        : branch(ot.branch), depth(ot.depth), level_type(ot.level_type)
    {
    }
    level_id(level_area_type ltype)
        : branch(BRANCH_MAIN_DUNGEON), depth(-1), level_type(ltype)
    {
    }

    static level_id parse_level_id(const std::string &s) throw (std::string);
    static level_id from_packed_place(const unsigned short place);

    unsigned short packed_place() const;
    std::string describe(bool long_name = false, bool with_number = true) const;

    void clear()
    {
        branch = BRANCH_MAIN_DUNGEON;
        depth  = -1;
        level_type = LEVEL_DUNGEON;
    }

    // Returns the absolute depth in the dungeon for the level_id;
    // non-dungeon branches (specifically Abyss and Pan) will return
    // depths suitable for use in monster and item generation. If
    // you're looking for a depth to set you.absdepth0 to, use
    // dungeon_absdepth().
    int absdepth() const;

    // Returns the absolute depth in the dungeon for the level_id, corresponding
    // to you.absdepth0.
    int dungeon_absdepth() const;

    bool is_valid() const
    {
        return (branch != NUM_BRANCHES && depth != -1)
            || level_type != LEVEL_DUNGEON;
    }

    const level_id &operator = (const level_id &id)
    {
        branch     = id.branch;
        depth      = id.depth;
        level_type = id.level_type;
        return (*this);
    }

    bool operator == (const level_id &id) const
    {
        return (level_type == id.level_type
                && (level_type != LEVEL_DUNGEON
                    || (branch == id.branch && depth == id.depth)));
    }

    bool operator != (const level_id &id) const
    {
        return !operator == (id);
    }

    bool operator <(const level_id &id) const
    {
        if (level_type != id.level_type)
            return (level_type < id.level_type);

        if (level_type != LEVEL_DUNGEON)
            return (false);

        return (branch < id.branch) || (branch==id.branch && depth < id.depth);
    }

    bool operator == (const branch_type _branch) const
    {
        return (branch == _branch && level_type == LEVEL_DUNGEON);
    }

    bool operator != (const branch_type _branch) const
    {
        return !operator == (_branch);
    }
};

// A position on a particular level.
struct level_pos
{
    level_id      id;
    coord_def     pos;      // The grid coordinates on this level.

    level_pos() : id(), pos()
    {
        pos.x = pos.y = -1;
    }

    level_pos(const level_id &lid, const coord_def &coord)
        : id(lid), pos(coord)
    {
    }

    level_pos(const level_id &lid)
        : id(lid), pos()
    {
        pos.x = pos.y = -1;
    }

    // Returns the level_pos of where the player is standing.
    static level_pos current();

    bool operator == (const level_pos &lp) const
    {
        return id == lp.id && pos == lp.pos;
    }

    bool operator != (const level_pos &lp) const
    {
        return id != lp.id || pos != lp.pos;
    }

    bool operator <  (const level_pos &lp) const
    {
        return (id < lp.id) || (id == lp.id && pos < lp.pos);
    }

    bool is_valid() const
    {
        return id.depth > -1 && pos.x != -1 && pos.y != -1;
    }

    bool is_on(const level_id _id)
    {
        return id == _id;
    }

    void clear()
    {
        id.clear();
        pos = coord_def(-1, -1);
    }
};

#endif // EXTERNS_H
