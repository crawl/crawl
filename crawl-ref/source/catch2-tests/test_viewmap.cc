#include <random>

#include "catch.hpp"

#include "AppHdr.h"

#include "viewmap.h"

vector<coord_def> _search_path_around_point(coord_def centre);

TEST_CASE( "Test map search path generation works", "[single-file]" ) {
    const auto search_path = _search_path_around_point(coord_def(50, 50));

    SECTION ("Search path has the right size.") {
        // The current location is not included.
        REQUIRE(search_path.size() == (78*68 - 1));
    }
}
