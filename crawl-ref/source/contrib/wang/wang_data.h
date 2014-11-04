#ifndef _WANG_DATA_H_
#define _WANG_DATA_H_

#include "wang.h"

namespace wang {

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

#endif // _WANG_DATA_H_
