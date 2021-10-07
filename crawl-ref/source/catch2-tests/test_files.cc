#include "catch.hpp"

#include "AppHdr.h"

#include "files.h"
#include "random.h"
#include "tags.h"

TEST_CASE( "Test save version reading/writing works", "[single-file]" ) {

    SECTION ("Major version is stored as a single unsigned byte") {

        SECTION ("reading") {
            vector<unsigned char> input = { 0xff, 0x00, };
            auto r = reader(input);

            const auto version = get_save_version(r);

            REQUIRE(version.major == 255);
            REQUIRE(r.valid() == false);
        }

        SECTION ("writing") {
            vector<unsigned char> output;
            vector<unsigned char> expected_output = { 0xff, 0x00, };
            const auto version = save_version(255, 0);
            auto w = writer(&output);

            write_save_version(w, version);

            REQUIRE(output == expected_output);
        }
    }

    SECTION ("Minor version < 255 is stored as a single unsigned byte") {

        SECTION ("reading") {
            vector<unsigned char> input = { 0x00, 0xfe, };
            auto r = reader(input);

            const auto version = get_save_version(r);

            REQUIRE(version.minor == 254);
            REQUIRE(r.valid() == false);
        }

        SECTION ("writing") {
            vector<unsigned char> output;
            vector<unsigned char> expected_output = { 0x00, 0xfe, };
            const auto version = save_version(0, 254);
            auto w = writer(&output);

            write_save_version(w, version);

            REQUIRE(output == expected_output);
        }
    }

    SECTION ("Minor version >= 255 is stored as a big-endian 4-byte integer") {

        SECTION ("reading") {
            vector<unsigned char> input = { 0x00, 0xff, 0x01, 0x02, 0x03, 0x04 };
            auto r = reader(input);
            const auto expected_minor = (1 << 24) + (2 << 16) + (3 << 8) + 4;

            const auto version = get_save_version(r);

            REQUIRE(version.minor == expected_minor);
            REQUIRE(r.valid() == false);
        }

        SECTION ("writing") {
            vector<unsigned char> output;
            vector<unsigned char> expected_output = { 0x00, 0xff, 0x01, 0x02, 0x03, 0x04 };
            const auto version = save_version(0, (1 << 24) + (2 << 16) + (3 << 8) + 4);
            auto w = writer(&output);

            write_save_version(w, version);

            REQUIRE(output == expected_output);
        }
    }

    SECTION ("Saving and loading is a reversible operation") {
        rng::subgenerator subgen(0, 0);

        for (auto i = 0; i < 1000; i++)
        {
            const auto version = save_version(random2(255), random2(1000));

            vector<unsigned char> buf;
            auto w = writer(&buf);
            write_save_version(w, version);

            auto r = reader(buf);
            const auto roundtrip_version = get_save_version(r);

            REQUIRE(version == roundtrip_version);
            REQUIRE(r.valid() == false);
        }
    }
}
