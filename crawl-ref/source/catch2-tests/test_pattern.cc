#include "catch_amalgamated.hpp"

#include "AppHdr.h"
#include "pattern.h"

TEST_CASE( "Pattern matches", "[single-file]" ) {
    // match substring
    text_pattern pattern1("[A-Za-z]+:[0-9]+");
    REQUIRE( pattern1.matches("Dungeon:1") );
    REQUIRE( pattern1.matches("You are on Dungeon:1") );

    // match whole string
    text_pattern pattern2("^[A-Za-z]+:[0-9]+$");
    REQUIRE( pattern2.matches("Dungeon:1") );
    REQUIRE( !pattern2.matches("You are on Dungeon:1") );
}

TEST_CASE( "Matched text", "[single-file]" ) {
    text_pattern pattern1(":[0-9]+");
    pattern_match match = pattern1.match_location("Lair:5");
    CHECK( match.matched_text() == ":5" );
    CHECK( match.start_pos() == 4 );
    // note: half-open range
    CHECK( match.end_pos() == 6 );
}
