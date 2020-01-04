#include "catch.hpp"

#include "AppHdr.h"
#include "branch.h"

TEST_CASE( "is_random_subbranch_test", "[single-file]" ) {

    for (branch_iterator it; it; ++it)
    {
        branch_type branch = it->id;

        CAPTURE(branch);

        bool old_is_random_subbranch = parent_branch(branch) == BRANCH_LAIR
                                        && branch != BRANCH_SLIME;

        REQUIRE(old_is_random_subbranch == is_random_subbranch(branch));
    }
}
