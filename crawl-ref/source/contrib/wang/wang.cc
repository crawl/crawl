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

#include "wang.h"
#include "wang_data.h"

#include <assert.h>

#include <algorithm>
#include <iostream>
#include <cstdlib>

using namespace std;

namespace wang {

bool operator<(const Point& lhs, const Point& rhs) {
  return lhs.x < rhs.x || lhs.y < rhs.y;
}

bool operator==(const Point& lhs, const Point& rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y;
}


Adjacency::Adjacency() {
  for (size_t i = FIRST_DIRECTION; i <= LAST_DIRECTION; ++i)
  {
    Direction d = static_cast<Direction>(i);
    permitted_[d] = new set<uint8_t>();
  }
}

Adjacency::~Adjacency() {
  for (size_t i = FIRST_DIRECTION; i <= LAST_DIRECTION; ++i)
  {
    Direction d = static_cast<Direction>(i);
    delete permitted_[d];
  }
}

bool Adjacency::adjacent(Direction d, set<uint8_t>& open) {
  intersection(open, *permitted_[d]);
  return !open.empty();
}

bool Adjacency::permitted(Direction d, uint8_t id) {
  return *permitted_[d]->find(id) != *permitted_[d]->end();
}

void Adjacency::add(uint8_t adjacency, const set<Direction>& dir) {
  set<Direction>::iterator itr = dir.begin();
  for (; itr != dir.end(); ++itr) {
    permitted_[*itr]->insert(adjacency);
  }
}

bool CornerDomino::matches(const Domino* other, Direction dir) const {
  // TODO: Correctly use polymorphism
  const CornerDomino* o = static_cast<const CornerDomino*>(other);
    switch (dir) {
      case NORTH:
        return ne_colour() == o->se_colour() && nw_colour() == o->sw_colour();
      case NORTH_EAST:
        return ne_colour() == o->sw_colour();
      case EAST:
        return ne_colour() == o->nw_colour() && se_colour() == o->sw_colour();
      case SOUTH_EAST:
        return se_colour() == o->nw_colour();
      case SOUTH:
        return se_colour() == o->ne_colour() && sw_colour() == o->nw_colour();
      case SOUTH_WEST:
        return sw_colour() == o->ne_colour();
      case WEST:
        return nw_colour() == o->ne_colour() && sw_colour() == o->se_colour();
      case NORTH_WEST:
        return nw_colour() == o->se_colour();
      default:
        return false;
    }
}

void Domino::intersect(const Domino* o, set<Direction>& result) const {
  set<Direction> allowed;
  for (size_t i = FIRST_DIRECTION; i <= LAST_DIRECTION; ++i)
  {
    Direction d = static_cast<Direction>(i);
    if (matches(o, d)) {
      allowed.insert(d);
    }
  }
  wang::intersection(result, allowed);
}

std::ostream& operator<< (std::ostream& stream, const CornerDomino& d) {
  stream
    << (int) d.nw_colour() << "#" << (int) d.ne_colour() << endl
    << "###" << endl
    << (int) d.se_colour() << "#" << (int) d.nw_colour();
  return stream;
}

bool EdgeDomino::matches(const Domino* other, Direction dir) const {
  const EdgeDomino* o = static_cast<const EdgeDomino*>(other);
    switch (dir) {
      case NORTH:
        return n_colour() == o->s_colour();
      case EAST:
        return e_colour() == o->w_colour();
      case SOUTH:
        return s_colour() == o->n_colour();
      case WEST:
        return w_colour() == o->e_colour();
      case NO_DIR:
        return false;
      default:
        return true;
    }
}

std::ostream& operator<< (std::ostream& stream, const EdgeDomino& d) {
  stream
    << "#" << (int) d.n_colour() << "#" << endl
    << (int) d.w_colour() << "#" << (int) d.e_colour() << endl
    << "#" << (int) d.s_colour() << "#";
  return stream;
}

DominoSet::DominoSet(CornerColours* colours, uint8_t sz) {
  max_colour_ = 0;
  for (uint8_t i = 0; i < sz; ++i) {
    CornerDomino* d = new CornerDomino(i, colours[i]);
    dominoes_[i] = d;
    max_colour_ = max(max_colour_,
        max(
          max(d->se_colour(), d->sw_colour()),
          max(d->ne_colour(), d->nw_colour())));
  }

  for (uint8_t i = 0; i < sz; ++i) {
    Adjacency* adj = new Adjacency();
    adjacencies_[i] = adj;
    CornerDomino* domino = static_cast<CornerDomino*>(dominoes_[i]);
    for (size_t j = 0; j < sz; ++j) { 
      set<Direction> directions(direction_arr, direction_arr + 8);
      CornerDomino* other = static_cast<CornerDomino*>(dominoes_[j]);
      other->intersect(domino, directions);
      adj->add(j, directions);
    }
  }
}

DominoSet::DominoSet(EdgeColours* colours, uint8_t sz) {
  max_colour_ = 0;
  for (uint8_t i = 0; i < sz; ++i) {
    EdgeDomino* d = new EdgeDomino(i, colours[i]);
    dominoes_[i] = d;
    max_colour_ = max(max_colour_,
        max(
          max(d->n_colour(), d->e_colour()),
          max(d->s_colour(), d->w_colour())));
  }

  for (uint8_t i = 0; i < sz; ++i) {
    Adjacency* adj = new Adjacency();
    adjacencies_[i] = adj;
    CornerDomino* domino = static_cast<CornerDomino*>(dominoes_[i]);
    for (size_t j = 0; j < sz; ++j) { 
      set<Direction> directions(direction_arr, direction_arr + 8);
      CornerDomino* other = static_cast<CornerDomino*>(dominoes_[j]);
      other->intersect(domino, directions);
      adj->add(j, directions);
    }
  }
}


DominoSet::~DominoSet() {
  for (uint8_t i = 0; i < adjacencies_.size(); ++i) {
    delete adjacencies_[i];
  }
}

int DominoSet::Conflicts(Point pt, const map<Point, uint8_t>& tiling) const {
  int conflicts = 0;
  uint8_t id = tiling.find(pt)->second;
  Domino* domino = dominoes_.find(id)->second;
  for (int x = -1; x <= 1; ++x) {
    for (int y = -1; y <= 1; ++y) {
      if (x == 0 && y == 0) {
        continue;
      }
      Point nb = {pt.x + x, pt.y + y};
      Point offset = {x, y};
      if (tiling.find(nb) != tiling.end()) {
        Domino* other = dominoes_.find(tiling.find(nb)->second)->second;
        Direction dir;
        asDirection(offset, dir);
        if (!domino->matches(other, dir)) {
          ++conflicts;
        }
      }
    }
  }
  return conflicts;
}

void DominoSet::Randomise(set<Point> pts, map<Point, uint8_t>& tiling, int sz) const {
  set<Point> shuffle;
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

int DominoSet::Best(
    Point pt, const map<Point, uint8_t>& tiling,
    vector<uint8_t>& result) const {
  set<uint8_t> all_set;
  for (uint8_t i = 0; i < dominoes_.size(); ++i) {
    all_set.insert(i);
  }
  const set<uint8_t> all = all_set;

  map<uint8_t, int> result_map;
  uint8_t neighbors = 0;
  for (int x = -1; x <= 1; ++x) {
    for (int y = -1; y <= 1; ++y) {
      if (x == 0 && y == 0) {
        continue;
      }
      Point nb = {pt.x + x, pt.y + y};
      Point offset = {x, y};
      if (tiling.find(nb) != tiling.end()) {
        ++neighbors;
        set<uint8_t> allowed = all;
        Domino* other = dominoes_.find(tiling.find(nb)->second)->second;
        Direction dir;
        asDirection(offset, dir);
        Adjacency* adj = adjacencies_.find(other->id())->second;
        adj->adjacent(dir, allowed);
        for (auto itr : allowed) {
          result_map[itr] += 1;
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
  int max = 0;
  for (auto itr : result_map) {
    if (itr.second > max) {
      max = itr.second;
      result.clear();
    }
    if (itr.second == max) {
      result.push_back(itr.first);
    }
  }
  return 8 - max;
}


bool DominoSet::Generate(size_t n, vector<uint8_t>& output) {
  set<uint8_t> all_set;
  for (uint8_t i = 0; i < dominoes_.size(); ++i) {
    all_set.insert(i);
  }
  const set<uint8_t> all = all_set;

  map<Point, uint8_t> tiling;
  vector<Point> all_points;
  for (int32_t i = 0; i < n; ++i) {
    for (int32_t j = 0; j < n; ++j) {
      Point pt = {i, j};
      all_points.push_back(pt);
    }
  }

  // Randomize all the tiles
  for (auto pt : all_points) {
      tiling[pt] = rand() % adjacencies_.size();
  }
  int trials = 10000;
  {
    bool did_shuffle = false;
    uint32_t last_conflicts = -1;
    int sz = 1;
    bool has_conflicts;
    do {
      set<Point> stuck;
      uint32_t conflict_count = 0;
      has_conflicts = false;
      random_shuffle(all_points.begin(), all_points.end());
      for (auto pt : all_points) {
        int conflicts = Conflicts(pt, tiling);
        if (conflicts) {
          has_conflicts = true;
          ++conflict_count;
          vector<uint8_t> choices;
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

  cout << "Trials: " << trials << endl;
  for (int32_t y = 0; y < n; ++y) {
    for (int32_t x = 0; x < n; ++x) {
      Point pt = {x, y};
      Domino* d = dominoes_[tiling[pt]];
      cout << (int) d->id() << "#";
    }
    cout << endl;
  }
  return true;
}

} // namespace wang
