#include "catch_amalgamated.hpp"

#include "AppHdr.h"
#include "english.h"

TEST_CASE( "English stuff works", "[single-file]" ) {
    REQUIRE( is_vowel('a') );
    REQUIRE( !is_vowel('z') );
}

TEST_CASE( "pluralise_monsters", "[single-file]" ) {
    REQUIRE( pluralise_monster("orc") == "orcs" );
    REQUIRE( pluralise_monster("orc skeleton") == "orc skeletons" );
    REQUIRE( pluralise_monster("merfolk") == "merfolk" );
    REQUIRE( pluralise_monster("jelly") == "jellies" );
    REQUIRE( pluralise_monster("cockroach") == "cockroaches" );
    REQUIRE( pluralise_monster("bush") == "bushes" );
    REQUIRE( pluralise_monster("guardian sphinx") == "guardian sphinxes" );
    REQUIRE( pluralise_monster("elf") == "elves" );
}
