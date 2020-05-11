#include <random>

#include "catch.hpp"

#include "AppHdr.h"

#include "ui.h"
#include "ui-scissor.h"

TEST_CASE( "Test region methods", "[single-file]" ) {

    SECTION ("Test constructor parameter order is x, y, w, h") {
        const ui::Region region = {1, 2, 3, 4};

        REQUIRE(region.x == 1);
        REQUIRE(region.y == 2);
        REQUIRE(region.width == 3);
        REQUIRE(region.height == 4);
    }

    SECTION ("Test operator== requires all fields to be identical") {
        REQUIRE(ui::Region(0, 0, 0, 0) != ui::Region(0, 0, 0, 1));
        REQUIRE(ui::Region(0, 0, 0, 0) != ui::Region(0, 0, 1, 0));
        REQUIRE(ui::Region(0, 0, 0, 0) != ui::Region(0, 1, 0, 0));
        REQUIRE(ui::Region(0, 0, 0, 0) != ui::Region(1, 0, 0, 0));
    }

    SECTION ("Test emptiness checking checks width and height") {
        REQUIRE(ui::Region(0, 0, 0, 0).empty() == true);
        REQUIRE(ui::Region(0, 0, 1, 0).empty() == true);
        REQUIRE(ui::Region(0, 0, 0, 1).empty() == true);
        REQUIRE(ui::Region(0, 0, 1, 1).empty() == false);
    }

    SECTION ("Test ex() method returns right side") {
        REQUIRE(ui::Region(5, 0, 7, 0).ex() == 12);
    }

    SECTION ("Test ey() method returns bottom side") {
        REQUIRE(ui::Region(0, 3, 0, 5).ey() == 8);
    }

    SECTION ("Test contains_point ") {
        const ui::Region region = {-10, -10, 20, 20};

        // Excludes points wholly outside
        REQUIRE(region.contains_point(-20, 0) == false);
        REQUIRE(region.contains_point(20, 0) == false);
        REQUIRE(region.contains_point(0, -20) == false);
        REQUIRE(region.contains_point(0, 20) == false);

        // Top-left sides are inclusive, right-bottom sides are not.
        REQUIRE(region.contains_point(-10, 0) == true);
        REQUIRE(region.contains_point(0, -10) == true);
        REQUIRE(region.contains_point(10, 0) == false);
        REQUIRE(region.contains_point(0, 10) == false);

        REQUIRE(region.contains_point(0, 0) == true);
    }

    SECTION ("Test AABB intersection") {
        const ui::Region region1 = {21, 0, 20, 42};
        const ui::Region region2 = {-1, 2, 37, 44};

        REQUIRE(region1.aabb_intersect(region2) == ui::Region(21, 2, 15, 40));
    }

    SECTION ("Test AABB union") {
        const ui::Region region1 = {21, 0, 20, 42};
        const ui::Region region2 = {-1, 2, 37, 44};

        REQUIRE(region1.aabb_union(region2) == ui::Region(-1, 0, 42, 46));
    }
}

TEST_CASE( "Test scissor stack", "[single-file]" ) {

    SECTION ("Test that scissor stack starts out with no scissor") {
        ui::ScissorStack s;

        REQUIRE(s.top() == ui::Region(0, 0, INT_MAX, INT_MAX));
    }

#if 0
    // TODO: this can't work right now, because we don't have a GL manager.
    // This can be tested if we switch to dependency injection.
    SECTION ("Test that scissor stack top() returns top region.") {
        ui::ScissorStack s;

        s.push(ui::Region(0, 0, 3, 3));
        REQUIRE(s.top() == ui::Region(0, 0, 3, 3));
        s.push(ui::Region(0, 0, 2, 2));
        REQUIRE(s.top() == ui::Region(0, 0, 2, 2));
        s.push(ui::Region(0, 0, 1, 1));
        REQUIRE(s.top() == ui::Region(0, 0, 1, 1));
    }
#endif
}
