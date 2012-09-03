/**
 * @file
 * @brief Procedurally generated dungeon layouts.
**/

#ifndef PROC_LAYOUTS_H
#define PROC_LAYOUTS_H

#include "AppHdr.h"

#include "enum.h"
#include "externs.h"

class ProceduralLayout
{
  public:
    virtual dungeon_feature_type get(const coord_def &p) = 0;
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
    dungeon_feature_type get(const coord_def &p);
  private:
    int _col_width, _col_space, _row_width, _row_space;
};

#endif /* PROC_LAYOUTS_H */
