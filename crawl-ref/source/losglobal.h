#ifndef LOSGLOBAL_H
#define LOSGLOBAL_H

enum los_type
{
    LOS_DEFAULT      = (1 << 0),
    LOS_NO_TRANS     = (1 << 1),
    LOS_FLAG_INVALID = (1 << 7),  // internal use
    LOS_ARENA = LOS_FLAG_INVALID, // hack
};

void invalidate_los_around(const coord_def& p);
void invalidate_los();

bool cell_see_cell(const coord_def& p, const coord_def& q, los_type l);

#endif
