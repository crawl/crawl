/**
 * @file
 * @brief Hierarchical layout system.
**/

#pragma once

#include <stack>

#include "ui.h"

namespace ui {

class ScissorStack
{
public:
    ScissorStack() {}

    void push(Region scissor)
    {
        if (regions.size() > 0)
            scissor = scissor.aabb_intersect(regions.top());
        regions.push(scissor);
#ifdef USE_TILE_LOCAL
        glmanager->set_scissor(scissor.x, scissor.y, scissor.width, scissor.height);
#endif
    }

    void pop()
    {
        ASSERT(regions.size() > 0);
        regions.pop();
#ifdef USE_TILE_LOCAL
        if (regions.size() > 0)
        {
            Region scissor = regions.top();
            glmanager->set_scissor(scissor.x, scissor.y, scissor.width, scissor.height);
        }
        else
            glmanager->reset_scissor();
#endif
    }

    Region top()
    {
        if (regions.size() > 0)
            return regions.top();
        return {0, 0, INT_MAX, INT_MAX};
    }

private:
    stack<Region> regions;
};

}
