#include <sstream>

#include "catch.hpp"

#include "AppHdr.h"

#include "describe.h"

#include "mon-info.h"

#include "monster-type.h"

TEST_CASE("_describe_monster_habitat outputs correct descriptions", "[single-file") {

    init_monsters();

    SECTION ("Amphibious monsters append correct string") {
        monster_info info = monster_info(MONS_FRILLED_LIZARD,MONS_FRILLED_LIZARD);

        std::ostringstream stream = std::ostringstream();

        _describe_monster_habitat(info,stream);

        REQUIRE(stream.str() == "It can travel through water.\n");

    }

    SECTION ("Lava monsters output correct string") {
        monster_info info = monster_info(MONS_SALAMANDER,MONS_SALAMANDER);

        std::ostringstream stream = std::ostringstream();


        _describe_monster_habitat(info,stream);

        REQUIRE(stream.str() == "It can travel through lava.\n");
    }

    SECTION ("Other monsters output nothing") {
        monster_info info = monster_info(MONS_KOBOLD,MONS_KOBOLD);

        std::ostringstream stream = std::ostringstream();

        stream.str("");
        stream.clear();

        _describe_monster_habitat(info,stream);

        REQUIRE(stream.str() == "");
    }
}
