#include "catch_amalgamated.hpp"

#include "AppHdr.h"

#include "tag-version.h"

TEST_CASE( "Test current minor tag enums have the correct value", "[single-file]" ) {
    // This test is intended to catch accidental changes to the values of minor
    // tag enums during the course of removing support for old minor versions.
    // I've added assertions for multiple values to attempt to guard against
    // accidental alteations in the middle of the table.
    //
    // If this test case fails because the first assertion references an enum
    // that you've just removed, no worries; simply delete that line, and add a
    // new line at the end for the current minor version.
    REQUIRE (TAG_MINOR_LUA_DUMMY_1 == 14);
    REQUIRE (TAG_MINOR_ORIG_MONNUM == 28);
    REQUIRE (TAG_MINOR_SHORT_SPELL_TYPE == 43);
    REQUIRE (TAG_MINOR_NEMELEX_DUNGEONS == 58);
    REQUIRE (TAG_MINOR_ATTACK_DESCS == 73);
    REQUIRE (TAG_MINOR_DS_CLOUD_MUTATIONS == 88);
    REQUIRE (TAG_MINOR_MONSTER_SPELL_SLOTS == 103);
    REQUIRE (TAG_MINOR_CORPSE_COLOUR == 118);
    REQUIRE (TAG_MINOR_MAX_XL == 133);
    REQUIRE (TAG_MINOR_RU_DELAY_STACKING == 148);
    REQUIRE (TAG_MINOR_SHOPINFO == 163);
    REQUIRE (TAG_MINOR_NEMELEX_WRATH == 178);
    REQUIRE (TAG_MINOR_FOLLOWER_TRANSIT_TIME == 193);
    REQUIRE (TAG_MINOR_SINGULAR_THEY == 208);
    REQUIRE (TAG_MINOR_GHOST_MAGIC == 213); // keep this, or update `bones_minor_tags`
    REQUIRE (TAG_MINOR_TRACK_REGEN_ITEMS == 216);
}
