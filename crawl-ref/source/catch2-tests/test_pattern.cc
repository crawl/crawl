#include "catch_amalgamated.hpp"

#include "AppHdr.h"
#include "pattern.h"

TEST_CASE( "Pattern matches", "[single-file]" ) {
    // match substring
    text_pattern pattern1("[A-Za-z]+:[0-9]+");
    CHECK( pattern1.matches("Dungeon:1") );
    CHECK( pattern1.matches("You are on Dungeon:1") );

    // match whole string
    text_pattern pattern2("^[A-Za-z]+:[0-9]+$");
    CHECK( pattern2.matches("Dungeon:1") );
    CHECK( !pattern2.matches("You are on Dungeon:1") );

    // repeat count
    text_pattern pattern3("^[A-Za-z]{2}$");
    CHECK( pattern3.matches("Mi") );
    CHECK( !pattern3.matches("MiFi") );

    // test UTF-8 handling (crawl-pcre fails this test because UTF-8 support is disabled)
    //text_pattern single_char_patt("^.$");
    //CHECK (single_char_patt.matches("の") );
}
