#include "catch.hpp"

#include "AppHdr.h"
#include "format.h"

string roundtrip(string str)
{
    return formatted_string::parse_string(str, COLOUR_INHERIT).to_colour_string();
};

void check_roundtrip(string str)
{
    REQUIRE( str == roundtrip(str) );
};

TEST_CASE( "formatted_string::to_colour_string always close tags", "[single-file]" ) {
    REQUIRE( roundtrip("<red>") == "<red></red>" );
}

TEST_CASE( "formatted_string round-trip preserves close tags", "[single-file]" ) {
    check_roundtrip("");
    check_roundtrip("test");
    check_roundtrip("<<");
    check_roundtrip(" \r\n test  \n\r ");
    check_roundtrip("<red><green></green></red>");
    check_roundtrip("<green> </green>");
    check_roundtrip("<red> <green> </green> </red>");
}

TEST_CASE( "formatted_string::parse_string() warns on malformed input", "[single-file]" ) {
    REQUIRE( roundtrip("<inherit>") == "<lightred><<inherit></lightred>" );
    REQUIRE( roundtrip("<not-a-colour>") == "<lightred><<not-a-colour></lightred>" );
    REQUIRE( roundtrip("</not-a-colour>") == "<lightred><</not-a-colour></lightred>" );
    REQUIRE( roundtrip("<red> </blue> </red>") == "<red> <lightred><</blue></lightred> </red>" );
    REQUIRE( roundtrip("<red> </blue> ") == "<red> <lightred><</blue></lightred> </red>" );
    REQUIRE( roundtrip("<red >") == "<lightred><<red ></lightred>" );
}

TEST_CASE( "formatted_string::parse_string() parses unclosed/empty tags as text", "[single-file]" ) {
    REQUIRE( roundtrip("<red ") == "<<red " );
    REQUIRE( roundtrip("<red") == "<<red" );
    REQUIRE( roundtrip("</red ") == "<</red " );
    REQUIRE( roundtrip("</red") == "<</red" );
    REQUIRE( roundtrip("</>") == "<</>" );
    REQUIRE( roundtrip("</") == "<</" );
    REQUIRE( roundtrip("<") == "<<" );
    REQUIRE( roundtrip(">") == ">" );
}

TEST_CASE( "formatted_string::parse_string_to_multiple() preserves nesting over newlines", "[single-file]" ) {
    {
        vector<formatted_string> out;
        formatted_string::parse_string_to_multiple("<red>01", out, 1);
        REQUIRE( out[0].to_colour_string() == "<red>0</red>" );
        REQUIRE( out[1].to_colour_string() == "<red>1</red>" );
    }
    {
        vector<formatted_string> out;
        formatted_string::parse_string_to_multiple("<red><green>0</green>1", out, 1);
        REQUIRE( out[0].to_colour_string() == "<red><green>0</green></red>" );
        REQUIRE( out[1].to_colour_string() == "<red>1</red>" );
    }
}
