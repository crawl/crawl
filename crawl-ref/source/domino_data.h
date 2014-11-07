/* Copyright (c) 2014 Brendan Hickey
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

#ifndef _DOMINO_DATA_H_
#define _DOMINO_DATA_H_

#include "domino.h"

namespace domino {

// This set of eight tiles trivially cover the plane.
// See: Wang Tiles for Image and Texture Generation (Cohen, 2003)
#define R 0
#define G 1
#define B 2
#define Y 3
static EdgeDomino cohen_set[8] = {
  {R, Y, G, B}, {G, B, G, B}, {R, Y, R, Y}, {G, B, R, Y},
  {R, B, G, Y}, {G, Y, G, Y}, {R, B, R, B}, {G, Y, R, B},
};

#undef R
#undef G
#undef B
#undef Y

// This is a six-colour set of 44 tiles.
// See: Aperiodic Sets of Square Tiles with Colored Corners (Lagae, 2006)
static CornerDomino aperiodic_set[44] = {
  // Row 1
  {0, 4, 1, 2}, {0, 3, 1, 5},
  {0, 2, 1, 5}, {2, 3, 3, 4},
  {2, 2, 3, 4}, {2, 1, 3, 0},
  {3, 1, 4, 0}, {3, 0, 4, 0},
  {5, 2, 2, 3}, {5, 3, 2, 3},
  {5, 5, 2, 3},
  // Row 2
  {5, 1, 2, 1}, {3, 1, 3, 0},
  {3, 0, 3, 0}, {5, 2, 3, 3},
  {5, 3, 3, 3}, {5, 5, 3, 3},
  {5, 1, 3, 1}, {2, 3, 4, 4},
  {2, 2, 4, 4}, {2, 1, 4, 0},
  {2, 3, 2, 4},
  // Row 3
  {2, 2, 2, 4}, {2, 1, 2, 0},
  {4, 0, 2, 1}, {4, 4, 2, 3},
  {3, 0, 5, 1}, {3, 4, 5, 3},
  {4, 0, 3, 1}, {4, 4, 3, 3},
  {2, 4, 5, 2}, {2, 3, 5, 5},
  {2, 2, 5, 5},
  // Row 4
  {1, 5, 0, 2}, {1, 2, 0, 2},
  {1, 2, 0, 3}, {1, 3, 0, 3},
  {1, 5, 0, 3}, {1, 1, 0, 1},
  {1, 5, 1, 2}, {1, 2, 1, 2},
  {0, 3, 0, 4}, {0, 2, 0, 4},
  {0, 1, 0, 0},
};

// This is a two-colour set of 16 tiles.
// See: An Alternative for Wang Tiles
static CornerDomino periodic_set[16] = {
//nw ne sw se
  {0, 0, 0, 0},
  {1, 0, 0, 0},
  {0, 0, 1, 0},
  {1, 0, 1, 0},

  {0, 0, 0, 1},
  {1, 0, 0, 1},
  {0, 0, 1, 1},
  {1, 0, 1, 1},

  {0, 1, 0, 0},
  {1, 1, 0, 0},
  {0, 1, 1, 0},
  {1, 1, 1, 0},

  {0, 1, 0, 1},
  {1, 1, 0, 1},
  {0, 1, 1, 1},
  {1, 1, 1, 1},
};
} // namespace wang

#endif // _DOMINO_DATA_H_
