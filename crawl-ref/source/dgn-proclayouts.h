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

class ProceduralLayout
{
  public:
    virtual dungeon_feature_type operator()(const coord_def &p, double offset = 0) = 0;
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
    
    dungeon_feature_type operator()(const coord_def &p, double offset = 0);
  private:
    int _col_width, _col_space, _row_width, _row_space;
};

class TurbulentLayout : public ProceduralLayout
{
  public:
    TurbulentLayout(ProceduralLayout &basis, double amplitude, double frequency) :
      _basis(basis), _amplitude(amplitude), _frequency(frequency) {}

    dungeon_feature_type operator()(const coord_def &p, double offset = 0)
    {
      double x = (p.x + 1.3) * _frequency;
      double y = (p.y + 0.5) * _frequency;
      double xt = fBM(x + 1.1, y - 8.9, -2.4 + offset, 4) * _amplitude;
      double yt = fBM(x + 6.7, y + 3.1,  1.9 + offset, 4) * _amplitude;
      return _basis(coord_def((int) (p.x + xt), (int) (p.y + yt)));
    }
  private:
    ProceduralLayout &_basis;
    double _amplitude, _frequency;
};

class ChaoticLayout : public ProceduralLayout
{
  public:
    ChaoticLayout(uint32_t seed, uint32_t density = 490) : _seed(seed), _density(density) {}
    dungeon_feature_type operator()(const coord_def &p, double offset = 0) {
      uint64_t x = hash3(p.x, p.y, 0x343FD);
      uint64_t y = hash3(x, p.y, 0xDEADBEEF); 
      uint32_t bump = round(perlin(x, y, offset / 2000.0) + offset / 2000.0);
      return feature(x^y + bump);
    }
  private:
    const uint32_t _seed, _density;
    dungeon_feature_type feature(uint32_t noise);
};

class WorleyLayout : public ProceduralLayout
{
  public:
    WorleyLayout(uint32_t seed, double scale, 
      ProceduralLayout &a, ProceduralLayout &b)
      : _seed(seed), _scale(scale), _a(a), _b(b) {}
   dungeon_feature_type operator()(const coord_def &p, double offset = 0) {
      double x = p.x * _scale;
      double y = p.y * _scale;
      if (worley::worley(x, y, offset).id[0] % 2)
        return _a(p, offset);
      return _b(p, offset);
    } 
  private:
    uint32_t _seed;
    double _scale;
    ProceduralLayout &_a, &_b;

};

class MixedColumnLayout : public ProceduralLayout {
  public:
    MixedColumnLayout(uint32_t seed) : _seed(seed) {
    }
    dungeon_feature_type operator()(const coord_def &p, double offset) {
      ColumnLayout a(2), b(2,6);
      return WorleyLayout(_seed, 0.07, a, b)(p, offset);
    }
  private:
    uint32_t _seed;
};

#endif /* PROC_LAYOUTS_H */
