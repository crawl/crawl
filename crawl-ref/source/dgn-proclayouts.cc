/**
 * @file
 * @brief Procedurally generated dungeon layouts.
**/  

#include <cmath>
#include "dgn-proclayouts.h"

ProceduralSample
ColumnLayout::operator()(const coord_def &p, const uint32_t offset)
{
  int x = abs(p.x) % (_col_width + _col_space);
  int y = abs(p.y) % (_row_width + _row_space);
  if (x < _col_width && y < _row_width)
    return ProceduralSample(p, DNGN_ROCK_WALL, -1);
  return ProceduralSample(p, DNGN_FLOOR, -1); 
}
