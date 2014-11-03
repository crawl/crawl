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
 *
 * TODO Pass around a RNG so this is actually deterministic.
 *
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

#ifndef __WANG_H__
#define __WANG_H__

#include <algorithm>
#include <map>
#include <set>
#include <stdint.h>
#include <vector>

namespace wang {

// Helper function to set handle intersections.
template <class T>
void intersection(std::set<T>& output, const std::set<T>& input) {
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
typedef uint8_t colour;
typedef struct {
  colour nw;
  colour ne;
  colour sw;
  colour se;
} CornerColours;

typedef struct {
  colour n;
  colour e;
  colour s;
  colour w;
} EdgeColours;

enum Direction {
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

static std::ostream& operator<< (std::ostream& stream, const Direction& dir) {
  static std::string directions[8] = {
    "N", "NE", "E", "SE", "S", "SW", "W", "NW"
  };
  stream << directions[dir];
  return stream;
}

static Direction reverse(const Direction& dir) {
  switch (dir) {
    case NORTH: return SOUTH;
    case NORTH_EAST: return SOUTH_WEST;
    case EAST: return WEST;
    case SOUTH_EAST: return NORTH_WEST;
    case SOUTH: return NORTH;
    case SOUTH_WEST: return NORTH_EAST;
    case WEST: return EAST;
    case NORTH_WEST: return SOUTH_EAST;
    case NO_DIR: return NO_DIR;
  }
}

static const Direction direction_arr[8] = {
  NORTH,
  NORTH_EAST,
  EAST,
  SOUTH_EAST,
  SOUTH,
  SOUTH_WEST,
  WEST,
  NORTH_WEST,
};

typedef struct {
  int32_t x;
  int32_t y;
} Point;

bool operator<(const Point& lhs, const Point& rhs);

bool operator==(const Point& lhs, const Point& rhs);

static bool asDirection(const Point& pt, Direction& dir) {
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

class Adjacency {
  public:
    Adjacency();
    ~Adjacency();
    bool adjacent(Direction d, std::set<uint8_t>& open);
    bool permitted(Direction d, uint8_t id);
    void add(uint8_t adjacency, const std::set<Direction>& dir);
  private:
    std::map<Direction, std::set<uint8_t>* > permitted_;
};

class Domino {
  public:
    virtual bool matches(const Domino* o, Direction dir) const = 0;
    void intersect(const Domino* other, std::set<Direction>& directions) const; 
    uint8_t id() {
      return id_;
    }
  protected:
    uint8_t id_;
};

class CornerDomino : public Domino {
  public:
    CornerDomino() {
      id_ = -1;
    }
    bool matches(const Domino* o, Direction dir) const;
    CornerDomino(uint8_t i, const CornerColours colours) {
        id_ = i;
        colours_.nw = colours.nw;
        colours_.ne = colours.ne;
        colours_.se = colours.se;
        colours_.sw = colours.sw;
    }
 
    colour nw_colour() const {
      return colours_.nw;
    }
  
    colour ne_colour() const {
      return colours_.ne;
    }
  
    colour se_colour() const {
      return colours_.se;
    }

    colour sw_colour() const {
      return colours_.sw;
    }

    friend std::ostream& operator<< (std::ostream& stream, const CornerDomino& dir);
  private:
    CornerColours colours_;
};

std::ostream& operator<< (std::ostream& stream, const CornerDomino& dir);

class EdgeDomino : public Domino {
  public:
    EdgeDomino() {
      id_ = -1;
    }
    EdgeDomino(uint8_t i, const EdgeColours colours) {
      id_ = i;
      colours_.n = colours.n;
      colours_.e = colours.e;
      colours_.s = colours.s;
      colours_.w = colours.w;
    }

    bool matches(const Domino* o, Direction dir) const;

    colour n_colour() const {
      return colours_.n;
    }
  
    colour e_colour() const {
      return colours_.e;
    }
  
    colour s_colour() const {
      return colours_.s;
    }

    colour w_colour() const {
      return colours_.w;
    }

    friend std::ostream& operator<< (std::ostream& stream, const EdgeDomino& dir);
  private:
    EdgeColours colours_;
};

std::ostream& operator<< (std::ostream& stream, const EdgeDomino& dir);

class DominoSet {
  public:
    DominoSet(CornerColours* colours, uint8_t sz);
    DominoSet(EdgeColours* colours, uint8_t sz);
    ~DominoSet();
    bool Generate(size_t x, size_t y, std::vector<uint8_t>& output);
    uint8_t num_colours() { return max_colour_ + 1; }
    uint8_t size() { return dominoes_.size(); }
    void print();

    Domino* get(uint8_t id) const {
      return dominoes_.find(id)->second;
    }
  private:
    int Conflicts(Point pt, const std::map<Point, uint8_t>& tiling) const;
    int Best(Point pt, const std::map<Point, uint8_t>& tiling, std::vector<uint8_t>& result) const;
    void Randomise(std::set<Point> pts, std::map<Point, uint8_t>& tiling, int sz) const;

    uint8_t max_colour_;
    std::map<uint8_t, Domino*> dominoes_;
    std::map<uint8_t, Adjacency*> adjacencies_;
};

};

#endif // __WANG_H__
