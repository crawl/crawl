#include "catch.hpp"

#include "AppHdr.h"

#include "monster.h"
#include "mon-util.h"

TEST_CASE("mons_habitat() returns correct values", "[single-file]")
{
    init_monsters();

    SECTION("mons_habitat() returns HT_LAND even for giants")
    {
        monster mon;
        mon.type = MONS_ETTIN;
        const auto habitat = mons_habitat(mon);

        REQUIRE(habitat == HT_LAND);
    }
}

TEST_CASE("mons_adapted_to_water() returns correct values", "[single-file]")
{
    init_monsters();

    SECTION("mons_adapted_to_water() returns true for giants")
    {
        monster mon;
        mon.type = MONS_ETTIN;
        const auto water_adapted = mons_adapted_to_water(mon);

        REQUIRE(water_adapted == true);
    }

    SECTION("mons_adapted_to_water() returns true for grey draconians")
    {
        monster mon;
        mon.type = MONS_GREY_DRACONIAN;
        const auto water_adapted = mons_adapted_to_water(mon);

        REQUIRE(water_adapted == true);
    }

    SECTION("mons_adapted_to_water() returns false for other things")
    {
        monster mon;
        mon.type = MONS_KOBOLD; // Just a random test point.
        const auto water_adapted = mons_adapted_to_water(mon);

        REQUIRE(water_adapted == false);
    }
}

TEST_CASE("mons_is_removed() returns correct values", "[single-file]" ) {

    init_monsters();

    SECTION ("mons_is_removed() returns true for removed monster") {
        bool removed = mons_is_removed(MONS_BUMBLEBEE);

        REQUIRE(removed == true);
    }

    SECTION ("mons_is_removed() returns false for current monster") {
        bool removed = mons_is_removed(MONS_BUTTERFLY);

        REQUIRE(removed == false);
    }
}

TEST_CASE("can get names for removed monster types", "[single-file]" ) {
    const auto name = mons_type_name(MONS_BUMBLEBEE, DESC_PLAIN);

    REQUIRE(name == "removed bumblebee");
}
