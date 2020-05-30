#include "catch.hpp"

#include "AppHdr.h"

#include "describe.h"
#include "mon-info.h"
#include "monster-type.h"

TEST_CASE("_monster_habitat_description outputs correct descriptions", "[single-file]")
{

    init_monsters();
    monster_info info;

    SECTION ("Amphibious monsters append correct string")
    {
        info = monster_info(MONS_FRILLED_LIZARD, MONS_FRILLED_LIZARD);

        string habitat_info = _monster_habitat_description(info);

        REQUIRE(habitat_info == "It can travel through water.\n");

    }

    SECTION ("Lava monsters output correct string")
    {
        info = monster_info(MONS_SALAMANDER, MONS_SALAMANDER);

        string habitat_info = _monster_habitat_description(info);

        REQUIRE(habitat_info == "It can travel through lava.\n");
    }

    SECTION ("Other monsters output nothing")
    {
        info = monster_info(MONS_KOBOLD, MONS_KOBOLD);

        string habitat_info = _monster_habitat_description(info);

        REQUIRE(habitat_info == "");
    }
}
