/**
 * @file
 * @brief Collaborative diffusion for monster AI.
 **/

#ifndef DIFFUSION_H
#define DIFFUSION_H

#include "AppHdr.h"

#include <float.h>
#include <queue>
#include <vector>

#include "coordit.h"
#include "externs.h"

class DECompare;
class DiffusionElement;
class DiffusionGrid;

typedef std::priority_queue<
    DiffusionElement,
    std::vector<DiffusionElement>,
    DECompare> diffusion_queue;

class DiffusionElement
{
    public:
        DiffusionElement(const float _f, const coord_def &_c) :
            f(_f), c(_c) {}
    private:
        float f;
        coord_def c;
    friend class DECompare;
    friend class DiffusionGrid;
};

class DECompare
{
    public:
        bool operator() (const DiffusionElement &lhs,
            const DiffusionElement &rhs)
        {
            return lhs.f > rhs.f;
        }
};

class DiffusionGrid {
  public:
    DiffusionGrid()
    {
        arr = FixedArray< float, GXM, GYM>(DBL_MIN);
    }
    void addPoint(float, coord_def);
    FixedArray< float, GXM, GYM > process();
    void clear();
  protected:
    virtual DiffusionElement score(float f, coord_def c);
  private:
    FixedArray< float, GXM, GYM > arr;
    diffusion_queue diffeq;
};

#endif /* DIFFUSION_H */
