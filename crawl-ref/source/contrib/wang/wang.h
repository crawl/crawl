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
      void set_id(uint8_t id) {
        id_ = id;
      }
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
      ~CornerDomino() {}

      bool matches(const CornerDomino& o, Direction dir) const;
      void intersect(const CornerDomino& other, std::set<Direction>& directions) const; 

      CornerDomino(const CornerColours colours) {
        colours_.nw = colours.nw;
        colours_.ne = colours.ne;
        colours_.se = colours.se;
        colours_.sw = colours.sw;
      }

      CornerDomino(uint8_t nw, uint8_t ne, uint8_t se, uint8_t sw) {
        colours_.nw = nw;
        colours_.ne = ne;
        colours_.se = se;
        colours_.sw = sw;
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
      EdgeDomino(const EdgeColours colours) {
        colours_.n = colours.n;
        colours_.e = colours.e;
        colours_.s = colours.s;
        colours_.w = colours.w;
      }

      EdgeDomino(uint8_t n, uint8_t e, uint8_t s, uint8_t w) {
        colours_.n = n;
        colours_.e = e;
        colours_.s = s;
        colours_.w = w;
      }
      ~EdgeDomino() {}

      bool matches(const EdgeDomino& o, Direction dir) const;
      void intersect(const EdgeDomino& other, std::set<Direction>& directions) const; 

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

  template <class T>
    class DominoSet {
      public:
        DominoSet(T* dominoes, uint8_t sz) {
          for (uint8_t i = 0; i < sz; ++i) {
            T d = dominoes[i];
            d.set_id(i);
            dominoes_[i] = d;
          }

          for (uint8_t i = 0; i < sz; ++i) {
            Adjacency* adj = new Adjacency();
            adjacencies_[i] = adj;
            T domino = dominoes_[i];
            for (size_t j = 0; j < sz; ++j) { 
              std::set<Direction> directions(direction_arr, direction_arr + 8);
              T other = dominoes_[j];
              other.intersect(domino, directions);
              adj->add(j, directions);
            }
          }
        }

        ~DominoSet() {
          for (uint8_t i = 0; i < adjacencies_.size(); ++i) {
            delete adjacencies_[i];
          }
        }

        uint8_t size() { return dominoes_.size(); }
        void print();

        T get(uint8_t id) const {
          return dominoes_.find(id)->second;
        }

        bool Generate(size_t x, size_t y, std::vector<uint8_t>& output) {
          std::set<uint8_t> all_set;
          for (uint8_t i = 0; i < dominoes_.size(); ++i) {
            all_set.insert(i);
          }
          const std::set<uint8_t> all = all_set;

          std::vector<Point> all_points;
          for (int32_t j = 0; j < y; ++j) {
            for (int32_t i = 0; i < x; ++i) {
              Point pt = {i, j};
              all_points.push_back(pt);
            }
          }

          bool has_conflicts = false;
          std::map<Point, uint8_t> tiling;
          // Init all the tiles
          for (auto pt : all_points) {
            std::vector<uint8_t> choices;
            Best(pt, tiling, choices);
            if (!choices.empty()) {
              random_shuffle(choices.begin(), choices.end());
              tiling[pt] = choices[0];
            } else {
              tiling[pt] = rand() % adjacencies_.size();
            }
            has_conflicts |= Conflicts(pt, tiling);
          }

          // If we were unable to constructively tile the plane
          // attempt to stochastically solve it.
          if (has_conflicts) {
            int trials = 10000;
            bool did_shuffle = false;
            uint32_t last_conflicts = -1;
            int sz = 1;
            do {
              std::set<Point> stuck;
              uint32_t conflict_count = 0;
              has_conflicts = false;
              random_shuffle(all_points.begin(), all_points.end());
              for (auto pt : all_points) {
                int conflicts = Conflicts(pt, tiling);
                if (conflicts) {
                  has_conflicts = true;
                  ++conflict_count;
                  std::vector<uint8_t> choices;
                  Best(pt, tiling, choices);
                  if (!choices.empty()) {
                    random_shuffle(choices.begin(), choices.end());
                    tiling[pt] = choices[0];
                  } else {
                    tiling[pt] = rand() % adjacencies_.size();
                  }
                  int after = Conflicts(pt, tiling);
                  if (after >= conflicts) {
                    stuck.insert(pt);
                  }
                }
              }
              if (conflict_count == last_conflicts && !did_shuffle) {
                did_shuffle = true;
                Randomise(stuck, tiling, ++sz);
              } else {
                did_shuffle = false;
                sz = 1;
              }
              stuck.clear();
              last_conflicts = conflict_count;
            } while (has_conflicts && trials--);
          }
          for (int32_t j = 0; j < y; ++j) {
            for (int32_t i = 0; i < x; ++i) {
              Point pt = {i, j};
              output.push_back(tiling[pt]);
            }
          }
          return !has_conflicts;
        }


      private:
        int Best(
            Point pt,
            const std::map<Point, uint8_t>& tiling,
            std::vector<uint8_t>& result) const {
          std::set<uint8_t> all_set;
          for (uint8_t i = 0; i < dominoes_.size(); ++i) {
            all_set.insert(i);
          }
          const std::set<uint8_t> all = all_set;

          std::map<uint8_t, int> result_map;
          uint8_t neighbors = 0;
          uint8_t mx = 0;
          for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
              if (x == 0 && y == 0) {
                continue;
              }
              Point nb = {pt.x + x, pt.y + y};
              Point offset = {x, y};
              if (tiling.find(nb) != tiling.end()) {
                ++neighbors;
                std::set<uint8_t> allowed = all;
                T other = dominoes_.find(tiling.find(nb)->second)->second;
                Direction dir;
                asDirection(offset, dir);
                Adjacency* adj = adjacencies_.find(other.id())->second;
                adj->adjacent(dir, allowed);
                for (auto itr : allowed) {
                  result_map[itr] += 1;
                  int val = result_map[itr];
                  if (val > mx) {
                    mx = val;
                  }
                }
              }
            }
          }
          if (!neighbors) {
            for (uint8_t v : all_set) {
              result.push_back(v);
            }
            return 0;
          }
          for (auto itr : result_map) {
            if (itr.second == mx) {
              result.push_back(itr.first);
            }
          }
          return 8 - mx;
        }


        void Randomise(std::set<Point> pts, std::map<Point, uint8_t>& tiling, int sz) const {
          std::set<Point> shuffle;
          for (auto pt : pts) {
            for (int x = -sz; x <= sz; ++x) {
              for (int y = -sz; y <= sz; ++y) {
                Point nb = {pt.x + x, pt.y + y};
                if (tiling.find(nb) != tiling.end() && rand() % 2) {
                  shuffle.insert(nb);
                }
              }
            }
          }
          for (auto itr : shuffle) {
            tiling[itr] = rand() % adjacencies_.size();
          }
        }

        int Conflicts(Point pt, const std::map<Point, uint8_t>& tiling) const {
          int conflicts = 0;
          int neighbors = 0;
          uint8_t id = tiling.find(pt)->second;
          T domino = dominoes_.find(id)->second;
          for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
              if (x == 0 && y == 0) {
                continue;
              }
              Point nb = {pt.x + x, pt.y + y};
              Point offset = {x, y};
              if (tiling.find(nb) != tiling.end()) {
                ++neighbors;
                T other = dominoes_.find(tiling.find(nb)->second)->second;
                Direction dir;
                asDirection(offset, dir);
                if (!domino.matches(other, dir)) {
                  ++conflicts;
                }
              }
            }
          }
          return conflicts;
        }

        std::map<uint8_t, T> dominoes_;
        std::map<uint8_t, Adjacency*> adjacencies_;
    };

};

#endif // __WANG_H__
