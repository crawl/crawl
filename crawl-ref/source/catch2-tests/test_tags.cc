#include "catch.hpp"

#include "AppHdr.h"

#include "tags.h"

void unmarshall_vehumet_spells(reader &th, set<spell_type>& old_gifts,
        set<spell_type>& gifts);

TEST_CASE( "Vehumet gifts can be decoded", "[single-file]" ) {

    SECTION ("spells stored as bytes can be read") {
        vector<unsigned char> input = {
            0x01, // count
            0x03, // SPELL_MAGIC_DART
            0x01, // count
            0x04, // SPELL_FIREBALL
        };
        auto r = reader(input);
        r.setMinorVersion(TAG_MINOR_SHORT_SPELL_TYPE - 1);

        const set<spell_type> expected_old_gifts = { SPELL_MAGIC_DART };
        const set<spell_type> expected_gifts = { SPELL_FIREBALL };

        set<spell_type> old_gifts, gifts;
        unmarshall_vehumet_spells(r, old_gifts, gifts);

        REQUIRE(old_gifts == expected_old_gifts);
        REQUIRE(gifts == expected_gifts);
        REQUIRE(r.valid() == false);
    }

    SECTION ("spells stored as shorts can be read") {
        vector<unsigned char> input = {
            0x01, // count
            0x00, 0x03, // SPELL_MAGIC_DART
            0x01, // count
            0x00, 0x04, // SPELL_FIREBALL
        };
        auto r = reader(input);
        r.setMinorVersion(TAG_MINOR_SHORT_SPELL_TYPE);

        const set<spell_type> expected_old_gifts = { SPELL_MAGIC_DART };
        const set<spell_type> expected_gifts = { SPELL_FIREBALL };

        set<spell_type> old_gifts, gifts;
        unmarshall_vehumet_spells(r, old_gifts, gifts);

        REQUIRE(old_gifts == expected_old_gifts);
        REQUIRE(gifts == expected_gifts);
        REQUIRE(r.valid() == false);
    }

    SECTION ("removed spells are filtered out") {
        vector<unsigned char> input = {
            0x02, // count
            0x04, // SPELL_FIREBALL
            0x06, // SPELL_DELAYED_FIREBALL
            0x02, // count
            0x03, // SPELL_MAGIC_DART
            0x07, // SPELL_STRIKING
        };
        auto r = reader(input);
        r.setMinorVersion(TAG_MINOR_SHORT_SPELL_TYPE - 1);

        const set<spell_type> expected_old_gifts = { SPELL_FIREBALL };
        const set<spell_type> expected_gifts = { SPELL_MAGIC_DART };

        set<spell_type> old_gifts, gifts;
        unmarshall_vehumet_spells(r, old_gifts, gifts);

        REQUIRE(old_gifts == expected_old_gifts);
        REQUIRE(gifts == expected_gifts);
        REQUIRE(r.valid() == false);
    }
}
