/**
 * @file
 * @brief Procedurally generated dungeon layouts.
**/

#ifndef PROC_LAYOUTS_H
#define PROC_LAYOUTS_H

#include "AppHdr.h"

#include <vector>

#include "cellular.h"
#include "enum.h"
#include "externs.h"
#include "perlin.h"

class ProceduralSample
{
  public:
    ProceduralSample(const coord_def c, const dungeon_feature_type f, const uint32_t cp) : 
      _coord(c), _feat(f), _changepoint(cp) {}
    coord_def coord() const { return _coord; }
    dungeon_feature_type feat() const { return _feat; }
    uint32_t changepoint() const { return _changepoint; }
    bool operator< (const ProceduralSample &rhs) const
    {
      // This is for priority_queue, which is a max heap, so reverse the comparison.
      return _changepoint > rhs._changepoint && _coord < rhs._coord && _feat < rhs._feat;
    }
  private:
    coord_def _coord;
    dungeon_feature_type _feat;
    uint32_t _changepoint;
};

class ProceduralLayout
{
  public:
    virtual ProceduralSample operator()(const coord_def &p, const uint32_t offset = 0) = 0;
};

class ColumnLayout : public ProceduralLayout
{
  public:
    ColumnLayout(int cw, int cs = -1, int rw = -1, int rs = -1)
    {
      cs = (cs < 0 ? cw : cs);
      _col_width = cw;
      _col_space = cs;
      _row_width = (rw < 0 ? cw : rw);
      _row_space = (rs < 0 ? cs : rs);
    }
    
    ProceduralSample operator()(const coord_def &p, const uint32_t offset = 0);
  private:
    int _col_width, _col_space, _row_width, _row_space;
};

#endif /* PROC_LAYOUTS_H */
