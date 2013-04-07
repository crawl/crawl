/**
 * @file
 * @brief Collaborative diffusion for monster AI.
 **/

#ifndef DIFFUSION_H
#define DIFFUSION_H

#include "AppHdr.h"

#include <limits.h>
#include <queue>
#include <vector>

#include "coordit.h"
#include "dungeon.h"
#include "externs.h"
#include "player.h"
#include "terrain.h"

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
        DiffusionElement(const int _f, const coord_def &_c) :
            f(_f), c(_c) {}
    private:
        int f;
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
            arr = FixedArray< int, GXM, GYM>(INT_MAX);
            processed = false;
        }
        ~DiffusionGrid() {}
        void addPoint(int, coord_def);
        coord_def climb(coord_def);
        void clear();
        void process();
    protected:
        virtual void initialize() = 0;
        virtual DiffusionElement score(int f, coord_def c) = 0;
        FixedArray< int, GXM, GYM > arr;
    private:
        bool processed;
        diffusion_queue diffeq;
};

class PlayerGrid : public DiffusionGrid {
    public:
        PlayerGrid() {}
    protected:
        void initialize();
        DiffusionElement score(int f, coord_def c);
};

#endif /* DIFFUSION_H */
