/**
 * @file
 * @brief Wang Tiles, functions for generating aperiodic tilings.
 *
 * This library implements deterministic, coherent, aperiodic domino tilings
 * of the plane.
 *
 * Citations:
 * 'Aperiodic Set of Square Tiles with Colored Corners' (Lagae 2006)
 * 'An Alternative for Wang Tiles: Colored Edges versus Color Corners'
 * (Lagae 2006)
 * 'Wang Tiles for Image and Texture Generation' (Cohen 2003)
 * 'Recursive Wang Tiles for Real-Time Blue Noise' (Kopf 2006)
 **/

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

#pragma once

#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>
#include <stdint.h>
#include <vector>

namespace domino {

// Number of attempts the stochastic solver should attempt.
constexpr int num_trials = 10;

// Helper function to set handle intersections.
template <class T>
void intersection(std::set<T>& output, const std::set<T>& input)
{
    std::set<T> diff;
    set_intersection(
            output.begin(), output.end(),
            input.begin(), input.end(),
            inserter(diff, diff.begin()));
    output = diff;
}

/**
 * CornerDomino Layout
 *
 * A#B
 * ###
 * C#D
 *
 * EdgeDomino Layout
 *
 * #N#
 * W#E
 * #S#
 */
typedef uint32_t colour;
struct CornerColours
{
    colour nw;
    colour ne;
    colour sw;
    colour se;
};

struct EdgeColours
{
    colour n;
    colour e;
    colour s;
    colour w;
};

enum polarity
{
    NEGATIVE = -1,
    POSITIVE = 1,
};

struct OrientedColour
{
    colour c;
    polarity o;
};

static OrientedColour operator-(const OrientedColour x)
{
  return { x.c, x.o == NEGATIVE ? POSITIVE : NEGATIVE };
}

struct OrientedColours
{
    OrientedColour n;
    OrientedColour e;
    OrientedColour s;
    OrientedColour w;
};


enum Direction
{
    FIRST_DIRECTION = 0,
    NORTH           = FIRST_DIRECTION,
    NORTH_EAST      = 1,
    EAST            = 2,
    SOUTH_EAST      = 3,
    SOUTH           = 4,
    SOUTH_WEST      = 5,
    WEST            = 6,
    NORTH_WEST      = 7,
    LAST_DIRECTION  = NORTH_WEST,
    NO_DIR          = -1,
};

static const Direction direction_arr[8] =
{
    NORTH,
    NORTH_EAST,
    EAST,
    SOUTH_EAST,
    SOUTH,
    SOUTH_WEST,
    WEST,
    NORTH_WEST,
};

struct Point
{
    int32_t x;
    int32_t y;
};

bool operator<(const Point& lhs, const Point& rhs);

bool operator==(const Point& lhs, const Point& rhs);

Point operator+(const Point& lhs, const Point& rhs);

static inline bool asDirection(const Point& pt, Direction& dir)
{
    dir = NO_DIR;
    if (pt.x == 0 && pt.y == -1) { dir = NORTH; }
    if (pt.x == 1 && pt.y == -1) { dir = NORTH_EAST; }
    if (pt.x == 1 && pt.y == 0) { dir = EAST; }
    if (pt.x == 1 && pt.y == 1) { dir = SOUTH_EAST; }
    if (pt.x == 0 && pt.y == 1) { dir = SOUTH; }
    if (pt.x == -1 && pt.y == 1) { dir = SOUTH_WEST; }
    if (pt.x == -1 && pt.y == 0) { dir = WEST; }
    if (pt.x == -1 && pt.y == -1) { dir = NORTH_WEST; }
    return dir != NO_DIR;
}

class Adjacency
{
    public:
        Adjacency();
        bool adjacent(Direction d, std::set<uint32_t>& open);
        bool permitted(Direction d, uint32_t id);
        void add(uint32_t adjacency, const std::set<Direction>& dir);
    private:
        std::map<Direction, std::set<uint32_t> > permitted_;
};

class Domino
{
    public:
        void set_id(uint32_t i)
        {
            id_ = i;
        }
        uint32_t id()
        {
            return id_;
        }
    protected:
        uint32_t id_;
};

class CornerDomino : public Domino
{
    public:
        CornerDomino()
        {
            id_ = -1;
        }
        ~CornerDomino() {}

        bool matches(const CornerDomino& o, Direction dir) const;
        void intersect(const CornerDomino& other, std::set<Direction>& directions) const;

        CornerDomino(const CornerColours colours)
            : colours_(colours)
        {}

        CornerDomino(uint32_t nw, uint32_t ne, uint32_t se, uint32_t sw)
            : colours_{nw, ne, sw, se}
        {}

        colour nw_colour() const
        {
            return colours_.nw;
        }

        colour ne_colour() const
        {
            return colours_.ne;
        }

        colour se_colour() const
        {
            return colours_.se;
        }

        colour sw_colour() const
        {
            return colours_.sw;
        }

        friend std::ostream& operator<< (std::ostream& stream, const CornerDomino& dir);
    private:
        CornerColours colours_;
};

std::ostream& operator<< (std::ostream& stream, const CornerDomino& dir);

class EdgeDomino : public Domino
{
    public:
        EdgeDomino()
        {
            id_ = -1;
        }
        EdgeDomino(const EdgeColours colours)
            : colours_(colours)
        {}
        EdgeDomino(colour n, colour e, colour s, colour w)
            : colours_{n, e, s, w}
        {}
        ~EdgeDomino() {}

        bool matches(const EdgeDomino& o, Direction dir) const;
        void intersect(const EdgeDomino& other, std::set<Direction>& directions) const;

        colour n_colour() const
        {
            return colours_.n;
        }

        colour e_colour() const
        {
            return colours_.e;
        }

        colour s_colour() const
        {
            return colours_.s;
        }

        colour w_colour() const
        {
            return colours_.w;
        }

        friend std::ostream& operator<< (std::ostream& stream, const EdgeDomino& dir);
    private:
        EdgeColours colours_;
};

std::ostream& operator<< (std::ostream& stream, const EdgeDomino& dir);

class OrientedDomino : public Domino
{
    public:
        OrientedDomino()
        {
            id_ = -1;
        }

        OrientedDomino(const OrientedColours colours)
            : colours_(colours)
        {}

        OrientedDomino(const OrientedColour n, const OrientedColour e,
              const OrientedColour s, const OrientedColour w)
            : colours_{n, e, s, w}
        {}

        OrientedDomino(int32_t n, int32_t e, int32_t s, int32_t w)
        {
            #define O_COL(x) { static_cast<colour>(abs(x)), \
                               (x < 0 ? NEGATIVE : POSITIVE) }
            colours_.n = O_COL(n);
            colours_.e = O_COL(e);
            colours_.s = O_COL(s);
            colours_.w = O_COL(w);
            #undef O_COL
        }

        ~OrientedDomino() {}

        OrientedDomino rotateCW()
        {
            return { colours_.w, colours_.n, colours_.e, colours_.s };
        }

        OrientedDomino rotateCCW()
        {
            return { colours_.e, colours_.s, colours_.w, colours_.n  };
        }

        OrientedDomino mirrorH ()
        {
            return { -colours_.n, -colours_.w, -colours_.s, -colours_.e };
        }

        OrientedDomino mirrorV()
        {
            return { -colours_.s, -colours_.e, -colours_.n, -colours_.w };
        }

        bool matches(const OrientedDomino& o, Direction dir) const;
        void intersect(const OrientedDomino& other, std::set<Direction>& directions) const;

        OrientedColour n_colour() const
        {
            return colours_.n;
        }

        OrientedColour e_colour() const
        {
            return colours_.e;
        }

        OrientedColour s_colour() const
        {
            return colours_.s;
        }

        OrientedColour w_colour() const
        {
            return colours_.w;
        }

    private:
        OrientedColours colours_;
};

template <class T>
class DominoSet
{
    public:
        DominoSet(T* dominoes, uint32_t sz)
        {
            for (uint32_t i = 0; i < sz; ++i)
            {
                T d = dominoes[i];
                d.set_id(i);
                dominoes_[i] = d;
            }

            for (uint32_t i = 0; i < sz; ++i)
            {
                Adjacency* adj = new Adjacency();
                adjacencies_[i] = adj;
                T domino = dominoes_[i];
                for (size_t j = 0; j < sz; ++j)
                {
                    std::set<Direction> directions(direction_arr, direction_arr + 8);
                    T other = dominoes_[j];
                    other.intersect(domino, directions);
                    adj->add(j, directions);
                }
            }
        }

        ~DominoSet()
        {
            for (uint32_t i = 0; i < adjacencies_.size(); ++i)
                delete adjacencies_[i];
        }

        uint32_t size() { return dominoes_.size(); }
        void print();

        T get(uint32_t id) const
        {
            return dominoes_.find(id)->second;
        }

        template <class R>
        bool Generate(int32_t x, int32_t y, std::vector<uint32_t>& output, R& rng)
        {
            std::set<uint32_t> all_set;
            for (uint32_t i = 0; i < dominoes_.size(); ++i)
                all_set.insert(i);

            std::vector<Point> all_points;
            for (int32_t j = 0; j < y; ++j)
            {
                for (int32_t i = 0; i < x; ++i)
                {
                    Point pt = {i, j};
                    all_points.push_back(pt);
                }
            }

            bool has_conflicts = false;
            std::map<Point, uint32_t> tiling;
            // Init all the tiles
            for (auto pt : all_points)
            {
                std::vector<uint32_t> choices;
                Best(pt, tiling, choices);
                if (!choices.empty())
                {
                    shuffle(choices.begin(), choices.end(), rng);
                    tiling[pt] = choices[0];
                }
                else
                    tiling[pt] = rng() % adjacencies_.size();

                has_conflicts |= Conflicts(pt, tiling);
            }

            // If we were unable to constructively tile the plane
            // attempt to stochastically solve it.
            if (has_conflicts)
            {
                int trials = 0;
                bool did_shuffle = false;
                uint32_t last_conflicts = -1;
                int sz = 1;
                do
                {
                    std::set<Point> stuck;
                    uint32_t conflict_count = 0;
                    has_conflicts = false;
                    shuffle(all_points.begin(), all_points.end(), rng);
                    for (auto pt : all_points)
                    {
                        int conflicts = Conflicts(pt, tiling);
                        if (conflicts)
                        {
                            has_conflicts = true;
                            ++conflict_count;
                            std::vector<uint32_t> choices;
                            Best(pt, tiling, choices);
                            if (!choices.empty())
                            {
                                shuffle(choices.begin(), choices.end(), rng);
                                tiling[pt] = choices[0];
                            }
                            else
                                tiling[pt] = rng() % adjacencies_.size();

                            int after = Conflicts(pt, tiling);
                            if (after >= conflicts)
                                stuck.insert(pt);
                        }
                    }
                    if (conflict_count == last_conflicts && !did_shuffle)
                    {
                        did_shuffle = true;
                        Randomise(stuck, tiling, ++sz, rng);
                    }
                    else
                    {
                        did_shuffle = false;
                        sz = 1;
                    }
                    stuck.clear();
                    last_conflicts = conflict_count;
                } while (has_conflicts && ++trials < num_trials);
            }
            for (int32_t j = 0; j < y; ++j)
            {
                for (int32_t i = 0; i < x; ++i)
                {
                    Point pt = {i, j};
                    output.push_back(tiling[pt]);
                }
            }
            return !has_conflicts;
        }

    private:
        int Best(
                Point pt,
                const std::map<Point, uint32_t>& tiling,
                std::vector<uint32_t>& result) const
        {
            std::set<uint32_t> all_set;
            for (uint32_t i = 0; i < dominoes_.size(); ++i)
                all_set.insert(i);

            std::map<uint32_t, int> result_map;
            uint32_t neighbors = 0;
            int mx = 0;
            for (int x = -1; x <= 1; ++x)
            {
                for (int y = -1; y <= 1; ++y)
                {
                    if (x == 0 && y == 0)
                        continue;

                    Point nb = {pt.x + x, pt.y + y};
                    Point offset = {x, y};
                    if (tiling.find(nb) != tiling.end())
                    {
                        ++neighbors;
                        std::set<uint32_t> allowed = all_set;
                        T other = dominoes_.find(tiling.find(nb)->second)->second;
                        Direction dir;
                        asDirection(offset, dir);
                        auto res = adjacencies_.find(other.id());
                        ASSERT(res != adjacencies_.end());
                        Adjacency* adj = res->second;
                        adj->adjacent(dir, allowed);
                        for (auto itr : allowed)
                        {
                            result_map[itr] += 1;
                            mx = std::max(mx, result_map[itr]);
                        }
                    }
                }
            }
            if (!neighbors)
            {
                for (uint32_t v : all_set)
                    result.push_back(v);
                return 0;
            }
            for (auto itr : result_map)
            {
                if (itr.second == mx)
                    result.push_back(itr.first);
            }
            return 8 - mx;
        }

        template <typename R>
            void Randomise(std::set<Point> pts, std::map<Point, uint32_t>& tiling,
                    int sz, R& rng) const
            {
                std::set<Point> shuffle;
                for (auto pt : pts)
                {
                    for (int x = -sz; x <= sz; ++x)
                    {
                        for (int y = -sz; y <= sz; ++y)
                        {
                            Point nb = {pt.x + x, pt.y + y};
                            if (tiling.find(nb) != tiling.end() && rng() % 2)
                                shuffle.insert(nb);
                        }
                    }
                }
                for (auto itr : shuffle)
                    tiling[itr] = rng() % adjacencies_.size();
            }

        int Conflicts(Point pt, const std::map<Point, uint32_t>& tiling) const
        {
            int conflicts = 0;
            int neighbors = 0;
            uint32_t id = tiling.find(pt)->second;
            T domino = dominoes_.find(id)->second;
            const Point offsets[] = {
                { -1,  0 },
                {  1,  0 },
                {  0, -1 },
                {  0,  1 },
            };

            for (size_t i = 0; i < 4; ++i)
            {
                Point nb = pt + offsets[i];
                if (tiling.find(nb) != tiling.end())
                {
                    ++neighbors;
                    T other = dominoes_.find(tiling.find(nb)->second)->second;
                    Direction dir;
                    asDirection(offsets[i], dir);
                    if (!domino.matches(other, dir))
                        ++conflicts;
                }
            }
            return conflicts;
        }

        std::map<uint32_t, T> dominoes_;
        std::map<uint32_t, Adjacency*> adjacencies_;
};

}  // namespace domino
