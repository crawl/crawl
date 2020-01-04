#include "AppHdr.h"

#include "ng-init-branches.h"

#include "branch.h"
#include "random.h"

void initialise_brentry()
{
    brentry = create_brentry();
}

FixedVector<level_id, NUM_BRANCHES> create_brentry()
{
    FixedVector<level_id, NUM_BRANCHES> candidate_brentry;

    for (branch_iterator it; it; ++it)
    {
        if (!skip_branch_brentry(it))
        {
            candidate_brentry[it->id] = level_id(it->parent_branch,
                                       random_range(it->mindepth,
                                                    it->maxdepth));
        }
    }

    vector<branch_type> disabled_branch = random_choose_disabled_branches();

    for (branch_type disabled : disabled_branch)
    {
        dprf("Disabling branch: %s", branches[disabled].shortname);
        candidate_brentry[disabled].clear();
    }

    for (branch_iterator it; it; ++it)
        brdepth[it->id] = it->numlevels;

    return candidate_brentry;
}

bool skip_branch_brentry(branch_iterator branch_iter)
{
    return branch_is_unfinished(branch_iter->id)
            || branch_iter->parent_branch == NUM_BRANCHES;
}
