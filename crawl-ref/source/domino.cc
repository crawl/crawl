/**
 * Copyright (c) 2014 Brendan Hickey
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **/
#include <AppHdr.h>

#include "domino.h"
#include "domino-data.h"

#include <algorithm>
#include <iostream>

using namespace std;

namespace domino {

bool operator<(const Point& lhs, const Point& rhs)
{
    return lhs.y < rhs.y || (lhs.y == rhs.y && lhs.x < rhs.x);
}

bool operator==(const Point& lhs, const Point& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

Point operator+(const Point& lhs, const Point& rhs)
{
    return { lhs.x + rhs.x, lhs.y + rhs.y };
}

Adjacency::Adjacency()
{
    for (size_t i = FIRST_DIRECTION; i <= LAST_DIRECTION; ++i)
    {
        Direction d = static_cast<Direction>(i);
        permitted_[d] = set<uint32_t>();
    }
}

bool Adjacency::adjacent(Direction d, set<uint32_t>& open)
{
    intersection(open, permitted_[d]);
    return !open.empty();
}

bool Adjacency::permitted(Direction d, uint32_t id)
{
    return permitted_[d].find(id) != permitted_[d].end();
}

void Adjacency::add(uint32_t adjacency, const set<Direction>& dir)
{
    set<Direction>::iterator itr = dir.begin();
    for (; itr != dir.end(); ++itr)
        permitted_[*itr].insert(adjacency);
}

bool CornerDomino::matches(const CornerDomino& o, Direction dir) const
{
    switch (dir)
    {
        case NORTH:
            return ne_colour() == o.se_colour() && nw_colour() == o.sw_colour();
        case NORTH_EAST:
            return ne_colour() == o.sw_colour();
        case EAST:
            return ne_colour() == o.nw_colour() && se_colour() == o.sw_colour();
        case SOUTH_EAST:
            return se_colour() == o.nw_colour();
        case SOUTH:
            return se_colour() == o.ne_colour() && sw_colour() == o.nw_colour();
        case SOUTH_WEST:
            return sw_colour() == o.ne_colour();
        case WEST:
            return nw_colour() == o.ne_colour() && sw_colour() == o.se_colour();
        case NORTH_WEST:
            return nw_colour() == o.se_colour();
        default:
            return false;
    }
}

void CornerDomino::intersect(const CornerDomino& o, set<Direction>& result) const
{
    set<Direction> allowed;
    for (size_t i = FIRST_DIRECTION; i <= LAST_DIRECTION; ++i)
    {
        Direction d = static_cast<Direction>(i);
        if (matches(o, d))
            allowed.insert(d);
    }
    domino::intersection(result, allowed);
}

std::ostream& operator<< (std::ostream& stream, const CornerDomino& d)
{
    stream
        << (int) d.nw_colour() << "#" << (int) d.ne_colour() << endl
        << "###" << endl
        << (int) d.se_colour() << "#" << (int) d.nw_colour();
    return stream;
}

bool EdgeDomino::matches(const EdgeDomino& o, Direction dir) const
{
    switch (dir)
    {
        case NORTH:
            return n_colour() == o.s_colour();
        case EAST:
            return e_colour() == o.w_colour();
        case SOUTH:
            return s_colour() == o.n_colour();
        case WEST:
            return w_colour() == o.e_colour();
        case NO_DIR:
            return false;
        default:
            return true;
    }
}

void EdgeDomino::intersect(const EdgeDomino& o, set<Direction>& result) const
{
    set<Direction> allowed;
    for (size_t i = FIRST_DIRECTION; i <= LAST_DIRECTION; ++i)
    {
        Direction d = static_cast<Direction>(i);
        if (matches(o, d))
            allowed.insert(d);
    }
    domino::intersection(result, allowed);
}

std::ostream& operator<< (std::ostream& stream, const EdgeDomino& d)
{
    stream
        << "#" << (int) d.n_colour() << "#" << endl
        << (int) d.w_colour() << "#" << (int) d.e_colour() << endl
        << "#" << (int) d.s_colour() << "#";
    return stream;
}

bool OrientedDomino::matches(const OrientedDomino& o, Direction dir) const
{
    switch (dir)
    {
        case NORTH:
            return n_colour().c == o.s_colour().c
                && n_colour().o != o.s_colour().o;
        case EAST:
            return e_colour().c == o.w_colour().c
                && e_colour().o != o.w_colour().o;
        case SOUTH:
            return s_colour().c == o.n_colour().c
                && s_colour().o != o.n_colour().o;
        case WEST:
            return w_colour().c == o.e_colour().c
                && w_colour().o != o.e_colour().o;
        case NO_DIR:
            return false;
        default:
            return true;
    }
}

void OrientedDomino::intersect(const OrientedDomino& o, set<Direction>& result) const
{
    set<Direction> allowed;
    for (size_t i = FIRST_DIRECTION; i <= LAST_DIRECTION; ++i)
    {
        Direction d = static_cast<Direction>(i);
        if (matches(o, d))
            allowed.insert(d);
    }
    domino::intersection(result, allowed);
}

}  // namespace domino
