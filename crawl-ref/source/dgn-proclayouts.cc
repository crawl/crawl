/**
 * @file
 * @brief Procedurally generated dungeon layouts.
**/  

#include "dgn-proclayouts.h"

dungeon_feature_type
ColumnLayout::get(const coord_def &p)
{
  int x = p.x % (_col_width + _col_space);
  int y = p.y % (_row_width + _row_space);
  if (x < _col_width && y < _row_width)
    return DNGN_MINWALL;
  return DNGN_FLOOR; 
}
