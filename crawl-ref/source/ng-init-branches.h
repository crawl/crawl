#pragma once

#include "branch.h"

void initialise_brentry();
bool skip_branch_brentry(branch_iterator branch_iter);
FixedVector<level_id, NUM_BRANCHES> create_brentry();
