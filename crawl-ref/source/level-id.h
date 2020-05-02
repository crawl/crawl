/**
 * @file
 * @brief Level ID.
**/

#pragma once

#include "coord-def.h"

// Identifies a level. Should never include virtual methods or
// dynamically allocated memory (see code to push level_id onto Lua
// stack in l-dgn.cc)
class level_id
{
public:
    branch_type branch;     // The branch in which the level is.
    int depth;              // What depth (in this branch - starting from 1)

public:
    // Returns the level_id of the current level.
    static level_id current();

    // Returns the level_id of the level that the stair/portal/whatever at
    // 'pos' on the current level leads to.
    static level_id get_next_level_id(const coord_def &pos);

    // Important that if run after this, ::valid() is false.
    level_id()
        : branch(BRANCH_DUNGEON), depth(-1)
    {
    }
    level_id(branch_type br, int dep = 1)
        : branch(br), depth(dep)
    {
    }

    /// @throws bad_level_id if s could not be parsed.
    static level_id parse_level_id(const string &s);
#if TAG_MAJOR_VERSION == 34
    static level_id from_packed_place(const unsigned short place);
#endif

    string describe(bool long_name = false, bool with_number = true) const;

    void clear()
    {
        branch = BRANCH_DUNGEON;
        depth  = -1;
    }

    // Returns the absolute depth in the dungeon for the level_id;
    // non-dungeon branches (specifically Abyss and Pan) will return
    // depths suitable for use in monster and item generation.
    int absdepth() const;

    bool is_valid() const
    {
        return branch < NUM_BRANCHES && depth > 0;
    }

    bool operator == (const level_id &id) const
    {
        return branch == id.branch && depth == id.depth;
    }

    bool operator != (const level_id &id) const
    {
        return !operator == (id);
    }

    bool operator <(const level_id &id) const
    {
        return branch < id.branch || (branch==id.branch && depth < id.depth);
    }

    void save(writer&) const;
    void load(reader&);
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
        return id < lp.id || (id == lp.id && pos < lp.pos);
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

    void save(writer&) const;
    void load(reader&);
};
