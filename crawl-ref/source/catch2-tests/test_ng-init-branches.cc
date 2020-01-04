#include "catch.hpp"

#include "AppHdr.h"
#include "branch.h"
#include "ng-init-branches.h"

TEST_CASE( "create_brentry_validity", "[single-file]" ) {
    FixedVector<level_id, NUM_BRANCHES> candidate_brentry;
    candidate_brentry = create_brentry();


    for (branch_iterator it; it; ++it)
    {
        bool correct = candidate_brentry[it->id].is_valid()
                        || is_random_subbranch(it->id)
                        || skip_branch_brentry(it);

        CAPTURE(it->id);

        REQUIRE(correct);
    }
}

TEST_CASE( "disable_exactly_one_of_each_lair_branch", "[single-file]" ) {
    FixedVector<level_id, NUM_BRANCHES> candidate_brentry;

    for (int i=0; i < 100; i++)
    {
        candidate_brentry = create_brentry();

        // != functions as xor here, testing if *exactly* one branch is
        // valid
        REQUIRE(candidate_brentry[BRANCH_SWAMP].is_valid()
                != candidate_brentry[BRANCH_SHOALS].is_valid());

        REQUIRE(candidate_brentry[BRANCH_SPIDER].is_valid()
                != candidate_brentry[BRANCH_SNAKE].is_valid());
    }
}
