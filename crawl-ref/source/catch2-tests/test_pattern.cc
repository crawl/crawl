#include "catch_amalgamated.hpp"

#include "AppHdr.h"
#include "pattern.h"

TEST_CASE( "Pattern matches", "[single-file]" ) {
    // Required to allow POSIX regex to correctly handle UTF-8 strings.
    // (C programs always start with the default "C" locale, which only
    //  understands ASCII strings. This call sets the locale to the user's
    //  configured one, which on Linux, would normally be a UTF-8 one).
    setlocale(LC_ALL, "");

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

    // test that multi-byte UTF-8 character is recognised as a single character
    text_pattern single_char_patt("^.$");
#ifndef REGEX_PCRE
    // PCRE v1 will currently fail this test because we're building it with
    // UTF-8 support disabled
    CHECK( single_char_patt.matches(u8"\u20AC") ); // euro symbol
#endif
    CHECK( !single_char_patt.matches(u8"\u20AC\u20AC") );
}
