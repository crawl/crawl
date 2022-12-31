#include <random>
#include <set>

#include "catch_amalgamated.hpp"

#include "AppHdr.h"

#include "coordit.h"

TEST_CASE("rectangle_iterator", "[single-file]")
{
    SECTION("Generates w*h unique points from top left, bottom right pair")
    {
        const auto x = GENERATE(10, 20, 30, 40);
        const auto y = GENERATE(10, 20, 30, 40);
        const auto w = GENERATE(10, 20, 30, 40);
        const auto h = GENERATE(10, 20, 30, 40);

        CAPTURE(x, y, w, h);

        set<coord_def> points;
        for (rectangle_iterator ri(coord_def(x,y), coord_def(x+w,y+h)); ri; ++ri)
            points.insert(*ri);

        REQUIRE((w >= 0 && h >= 0));
        REQUIRE(points.size() == static_cast<size_t>((w+1)*(h+1)));  // Inclusive.
    }

    SECTION("Generates w*h unique points from centre and halfside")
    {
        const auto x = GENERATE(10, 20, 30, 40);
        const auto y = GENERATE(10, 20, 30, 40);
        const auto halfside = GENERATE(10, 20, 30, 40);

        CAPTURE(x, y, halfside);

        set<coord_def> points;
        for (rectangle_iterator ri(coord_def(x,y), halfside); ri; ++ri)
            points.insert(*ri);

        REQUIRE(halfside >= 0);
        const size_t side_length = static_cast<size_t>(halfside)*2 + 1;
        REQUIRE(points.size() == side_length*side_length);  // Inclusive.
    }
}

TEST_CASE("random_rectangle_iterator", "[single-file]")
{
    SECTION("Generates w*h unique points")
    {
        auto x = GENERATE(10, 20, 30, 40);
        auto y = GENERATE(10, 20, 30, 40);
        auto w = GENERATE(10, 20, 30, 40);
        auto h = GENERATE(10, 20, 30, 40);

        CAPTURE(x, y, w, h);

        set<coord_def> points;
        for (random_rectangle_iterator ri(coord_def(x,y), coord_def(x+w,y+h)); ri; ++ri)
            points.insert(*ri);

        REQUIRE((w >= 0 && h >= 0));
        REQUIRE(points.size() == static_cast<size_t>((w+1)*(h+1)));  // Inclusive.
    }
}

TEST_CASE("orth_adjacent_iterator", "[single-file]")
{
    SECTION("Includes only orthagonally adjacent locations")
    {
        const coord_def where(50, 50);

        vector<coord_def> adjacent;
        for (orth_adjacent_iterator ai(where); ai; ++ai)
            adjacent.push_back(*ai);

        // Right, left, bottom, top.
        const vector<coord_def> expected = {
            coord_def(51, 50),
            coord_def(49, 50),
            coord_def(50, 51),
            coord_def(50, 49),
        };

        REQUIRE(adjacent == expected);
    }

    SECTION("Includes the centre if specified")
    {
        const coord_def where(50, 50);

        vector<coord_def> adjacent;
        for (orth_adjacent_iterator ai(where, false); ai; ++ai)
            adjacent.push_back(*ai);

        // Centre, right, left, bottom, top.
        const vector<coord_def> expected = {
            coord_def(50, 50),
            coord_def(51, 50),
            coord_def(49, 50),
            coord_def(50, 51),
            coord_def(50, 49),
        };

        REQUIRE(adjacent == expected);
    }
}

TEST_CASE("adjacent_iterator", "[single-file]")
{
    SECTION("Includes all adjacent locations")
    {
        const coord_def where(50, 50);

        vector<coord_def> adjacent;
        for (adjacent_iterator ai(where); ai; ++ai)
            adjacent.push_back(*ai);

        // Starts at top-left and rotates counter-clockwise.
        const vector<coord_def> expected = {
            coord_def(49, 49),
            coord_def(49, 50),
            coord_def(49, 51),
            coord_def(50, 51),
            coord_def(51, 51),
            coord_def(51, 50),
            coord_def(51, 49),
            coord_def(50, 49),
        };

        REQUIRE(adjacent == expected);
    }

    SECTION("Includes the centre if specified")
    {
        const coord_def where(50, 50);

        vector<coord_def> adjacent;
        for (adjacent_iterator ai(where, false); ai; ++ai)
            adjacent.push_back(*ai);

        // Centre first, then starts at top-left and rotates counter-clockwise.
        const vector<coord_def> expected = {
            coord_def(50, 50),
            coord_def(49, 49),
            coord_def(49, 50),
            coord_def(49, 51),
            coord_def(50, 51),
            coord_def(51, 51),
            coord_def(51, 50),
            coord_def(51, 49),
            coord_def(50, 49),
        };

        REQUIRE(adjacent == expected);
    }
}

TEST_CASE("distance_iterator", "[single-file]")
{
    SECTION("Includes all adjacent locations for d = 1")
    {
        const coord_def where(50, 50);

        vector<coord_def> points;
        for (distance_iterator di(where, false, true, 1); di; ++di)
            points.push_back(*di);

        // Top to bottom, left to right.
        const vector<coord_def> expected = {
            coord_def(49, 49),
            coord_def(49, 50),
            coord_def(49, 51),
            coord_def(50, 49),
            coord_def(50, 51),
            coord_def(51, 49),
            coord_def(51, 50),
            coord_def(51, 51),
        };

        REQUIRE(points == expected);
    }

    SECTION("Includes the centre if specified")
    {
        const coord_def where(50, 50);

        vector<coord_def> points;
        for (distance_iterator di(where, false, false, 1); di; ++di)
            points.push_back(*di);

        // Centre first, then top to bottom, left to right.
        const vector<coord_def> expected = {
            coord_def(50, 50),
            coord_def(49, 49),
            coord_def(49, 50),
            coord_def(49, 51),
            coord_def(50, 49),
            coord_def(50, 51),
            coord_def(51, 49),
            coord_def(51, 50),
            coord_def(51, 51),
        };

        REQUIRE(points == expected);
    }
}
