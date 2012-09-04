/**
 * @file
 * @brief Procedurally generated dungeon layouts.
**/  

#include <cmath>
#include "dgn-proclayouts.h"

dungeon_feature_type
ColumnLayout::operator()(const coord_def &p, double offset)
{
  int x = p.x % (_col_width + _col_space);
  int y = p.y % (_row_width + _row_space);
  if (x < _col_width && y < _row_width)
    return DNGN_MINWALL;
  return DNGN_FLOOR; 
}

dungeon_feature_type 
ChaoticLayout::feature(uint32_t noise)
{
  const int NUM_FEATURES = 10;
  const dungeon_feature_type features[] = {
    DNGN_ROCK_WALL,
    DNGN_ROCK_WALL,
    DNGN_ROCK_WALL,
    DNGN_ROCK_WALL,
    DNGN_STONE_WALL,
    DNGN_STONE_WALL,
    DNGN_STONE_WALL,
    DNGN_METAL_WALL,
    DNGN_METAL_WALL,
    DNGN_GREEN_CRYSTAL_WALL
  };
  if (noise % 1000 < _density) {
    return features[noise%NUM_FEATURES];
  }
  return DNGN_FLOOR;
} 
