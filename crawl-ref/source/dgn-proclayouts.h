/**
 * @file
 * @brief Procedurally generated dungeon layouts.
**/

#ifndef PROC_LAYOUTS_H
#define PROC_LAYOUTS_H

#include "AppHdr.h"

#include "enum.h"
#include "externs.h"
#include "perlin.h"

class ProceduralLayout
{
  public:
    virtual dungeon_feature_type operator[](const coord_def &p) = 0;
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
    
    dungeon_feature_type operator[](const coord_def &p);
  private:
    int _col_width, _col_space, _row_width, _row_space;
};

class TurbulentLayout : public ProceduralLayout
{
  public:
    TurbulentLayout(ProceduralLayout &basis, double amplitude, double frequency) :
      _basis(basis), _amplitude(amplitude), _frequency(frequency) {}

    dungeon_feature_type operator[](const coord_def &p)
    {
      double x = (p.x + 1.3) * _frequency;
      double y = (p.y + 0.5) * _frequency;
      double xt = fBM(x, y, -2.4, 4) * _amplitude;
      double yt = fBM(x + 6.7, y + 3.1, 1.9, 4) * _amplitude;
      return _basis[coord_def((int) (p.x + xt), (int) (p.y + yt))];
    }
  private:
    ProceduralLayout &_basis;
    double _amplitude, _frequency;
};

class MaxLayout : public ProceduralLayout
{
  public:
    MaxLayout(ProceduralLayout &a, ProceduralLayout &b) :
      _a(a), _b(b) {};
    dungeon_feature_type operator[](const coord_def &p);
  private:
    ProceduralLayout &_a, &_b;
};
 
class MinLayout : public ProceduralLayout
{
  public:
    MinLayout(ProceduralLayout &a, ProceduralLayout &b) :
      _a(a), _b(b) {};
    dungeon_feature_type operator[](const coord_def &p);
  private:
    ProceduralLayout &_a, &_b;
}; 
#endif /* PROC_LAYOUTS_H */
