#ifndef LOSGLOBAL_H
#define LOSGLOBAL_H

#include "los-type.h"

void invalidate_los_around(const coord_def& p);
void invalidate_los();

bool cell_see_cell(const coord_def& p, const coord_def& q, los_type l);

#endif
