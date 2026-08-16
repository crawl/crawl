#include "catch_amalgamated.hpp"

#include "AppHdr.h"
#include "pattern.h"

TEST_CASE( "Pattern match", "[single-file]" ) {
    // match substring
    text_pattern pattern1("[A-Za-z]+:[0-9]+");
    REQUIRE( pattern1.matches("Dungeon:1") );
    REQUIRE( pattern1.matches("You are on Dungeon:1") );

    // match wholes string
    text_pattern pattern2("^[A-Za-z]+:[0-9]+$");
    REQUIRE( pattern2.matches("Dungeon:1") );
    REQUIRE( !pattern2.matches("You are on Dungeon:1") );
}

TEST_CASE( "Pattern replace", "[single-file]" ) {
    // simple replace
    text_pattern pattern1("\\bthe [a-z]+\\b");
    CHECK( pattern1.replace("You hit the orc.", "the elf") == "You hit the elf." );

    // replace with back reference
    text_pattern pattern2("([A-Z][a-z]+):[0-9]+");
    CHECK( pattern2.replace("Dungeon:1 and Lair:3", "$1:2") == "Dungeon:2 and Lair:2" );

    // replace with 2 back references
    text_pattern pattern3("([0-9]+)\\.([0-9]+)");
    CHECK( pattern3.replace("Axes: 27.0", "$1,$2") == "Axes: 27,0" );

    // replace with self (make sure we don't get an infinte loop)
    text_pattern pattern4("orc");
    CHECK( pattern4.replace("You see an orc.", "orc" ) == "You see an orc." );
}

TEST_CASE( "Pattern capture", "[single-file]" ) {
    text_pattern pattern1("([A-Z][a-z]+):[0-9]+");
    vector<string> expected1 = { "Dungeon", "Lair" };
    CHECK( pattern1.capture("Dungeon:1 and Lair:3") == expected1 );

    text_pattern pattern2("([0-9]+)\\.([0-9]+)");
    vector<string> expected2 = { "27", "0" };
    CHECK( pattern2.capture("Axes: 27.0") == expected2 );
}
