#include "catch.hpp"

#include "AppHdr.h"

#include "describe.h"
#include "mon-info.h"
#include "monster-type.h"

TEST_CASE("_monster_habitat_description outputs correct descriptions", "[single-file]")
{
    init_monsters();

    SECTION("Amphibious monsters append correct string")
    {
        const auto info = monster_info(MONS_FRILLED_LIZARD, MONS_FRILLED_LIZARD);

        const string habitat_info = _monster_habitat_description(info);

        REQUIRE(habitat_info == "It can travel through water.\n");
    }

    SECTION("Lava monsters output correct string")
    {
        const auto info = monster_info(MONS_SALAMANDER, MONS_SALAMANDER);

        const string habitat_info = _monster_habitat_description(info);

        REQUIRE(habitat_info == "It can travel through lava.\n");
    }

    SECTION("Monsters with other pronouns output correct string")
    {
        const auto info = monster_info(MONS_CEREBOV, MONS_CEREBOV);

        const string habitat_info = _monster_habitat_description(info);

        REQUIRE(habitat_info == "They can travel through water.\n");
    }

    SECTION("Other monsters output nothing")
    {
        const auto info = monster_info(MONS_KOBOLD, MONS_KOBOLD);

        const string habitat_info = _monster_habitat_description(info);

        REQUIRE(habitat_info == "");
    }
}
