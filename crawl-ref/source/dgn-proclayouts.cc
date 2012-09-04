/**
 * @file
 * @brief Procedurally generated dungeon layouts.
**/  

#include "dgn-proclayouts.h"

dungeon_feature_type
ColumnLayout::operator[](const coord_def &p)
{
  int x = p.x % (_col_width + _col_space);
  int y = p.y % (_row_width + _row_space);
  if (x < _col_width && y < _row_width)
    return DNGN_MINWALL;
  return DNGN_FLOOR; 
}

dungeon_feature_type 
MaxLayout::operator[](const coord_def &p)
{
  return min(_a[p], _b[p]);
} 

dungeon_feature_type 
MinLayout::operator[](const coord_def &p)
{
  // Dungeon features become 'softer' as they increase in value.
  return max(_a[p], _b[p]);
} 
