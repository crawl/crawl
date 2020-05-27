#include "catch.hpp"

#include "AppHdr.h"

#include "mon-enum.h"
#include "monster-type.h"
#include "mon-util.h"

TEST_CASE("mons_is_removed() returns correct values", "[single-file]")
{
    init_monsters();

    SECTION("mons_is_removed() returns true for removed monster")
    {
        const bool removed = mons_is_removed(MONS_BUMBLEBEE);

        REQUIRE(removed == true);
    }

    SECTION("mons_is_removed() returns false for current monster")
    {
        const bool removed = mons_is_removed(MONS_BUTTERFLY);

        REQUIRE(removed == false);
    }
}

TEST_CASE("can get names for removed monster types", "[single-file]")
{
    const auto name = mons_type_name(MONS_BUMBLEBEE, DESC_PLAIN);

    REQUIRE(name == "removed bumblebee");
}

TEST_CASE("mons_habitat_type returns correct habitats", "[single-file]")
{
    SECTION("Land monsters should be HT_LAND")
    {
        const auto habitat = mons_habitat_type(MONS_KOBOLD, MONS_KOBOLD);
        REQUIRE(habitat == HT_LAND);
    }

    SECTION("Lava monsters should be HT_LAVA")
    {
        const auto habitat = mons_habitat_type(MONS_LAVA_SNAKE, MONS_LAVA_SNAKE);

        REQUIRE(habitat == HT_LAVA);
    }

    SECTION("Water monsters should be HT_WATER")
    {
        const auto habitat = mons_habitat_type(MONS_KRAKEN, MONS_KRAKEN);

        REQUIRE(habitat == HT_WATER);
    }

    SECTION("Amphibious monsters should be HT_AMPHIBIOUS")
    {
        const auto habitat = mons_habitat_type(MONS_FRILLED_LIZARD, MONS_FRILLED_LIZARD);

        REQUIRE(habitat == HT_AMPHIBIOUS);
    }

    SECTION("Amphibious (lava) monsters should be HT_AMPHIBIOUS_LAVA")
    {
        const auto habitat = mons_habitat_type(MONS_SALAMANDER, MONS_SALAMANDER);

        REQUIRE(habitat == HT_AMPHIBIOUS_LAVA);
    }

    SECTION("Zombies of amphibious monsters should still be HT_AMPHIBIOUS")
    {
        const auto habitat = mons_habitat_type(MONS_ZOMBIE, MONS_FRILLED_LIZARD);

        REQUIRE(habitat == HT_AMPHIBIOUS);
    }

    SECTION("Giants should show up as HT_AMPHIBIOUS unless explicitly told not to")
    {
        const auto habitat = mons_habitat_type(MONS_CYCLOPS, MONS_CYCLOPS);

        REQUIRE(habitat == HT_AMPHIBIOUS);
    }

    SECTION("Giants should not show up as HT_AMPHIBIOUS normally")
    {
        const auto habitat = mons_habitat_type(MONS_CYCLOPS, MONS_CYCLOPS, true);

        REQUIRE(habitat == HT_LAND);
    }

    SECTION("Amphibious giants should always show up as HT_AMPHIBIOUS")
    {
        const auto habitat = mons_habitat_type(MONS_TENTACLED_MONSTROSITY, MONS_TENTACLED_MONSTROSITY, true);

        REQUIRE(habitat == HT_AMPHIBIOUS);
    }

    SECTION("Simulacrum Salamanders should show HT_LAND")
    {
        const auto habitat = mons_habitat_type(MONS_SIMULACRUM, MONS_SALAMANDER);

        REQUIRE(habitat == HT_LAND);
    }
}
