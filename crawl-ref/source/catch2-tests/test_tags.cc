#include <random>

#include "catch.hpp"

#include "AppHdr.h"

#include "map-cell.h"
#include "random.h"
#include "tags.h"

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

TEST_CASE( "Player spells can be decoded", "[single-file]" ) {

    SECTION ("spells stored as shorts can be read") {
        vector<unsigned char> input = {
            0x02, // count
            0x00, 0x03, // SPELL_MAGIC_DART
            0x00, 0x04, // SPELL_FIREBALL
        };
        auto r = reader(input);
        r.setMinorVersion(TAG_MINOR_SHORT_SPELL_TYPE);

        FixedVector<spell_type, MAX_KNOWN_SPELLS> expected_spells(SPELL_NO_SPELL);
        expected_spells[0] = SPELL_MAGIC_DART;
        expected_spells[1] = SPELL_FIREBALL;

        const auto result = unmarshall_player_spells(r);

        for (size_t i = 0; i < result.size(); i++)
            REQUIRE(result[i] == expected_spells[i]);
        REQUIRE(r.valid() == false);
    }

    SECTION ("removed spells are filtered out") {
        vector<unsigned char> input = {
            0x02, // count
            0x00, 0x07, // SPELL_STRIKING
            0x00, 0x04, // SPELL_FIREBALL
        };
        auto r = reader(input);
        r.setMinorVersion(TAG_MINOR_SHORT_SPELL_TYPE);

        FixedVector<spell_type, MAX_KNOWN_SPELLS> expected_spells(SPELL_NO_SPELL);
        expected_spells[0] = SPELL_NO_SPELL;
        expected_spells[1] = SPELL_FIREBALL;

        const auto result = unmarshall_player_spells(r);

        for (size_t i = 0; i < result.size(); i++)
            REQUIRE(result[i] == expected_spells[i]);
        REQUIRE(r.valid() == false);
    }
}

TEST_CASE( "Removed spells remover can filter spell library", "[single-file]" ) {

    SECTION ("removed spells are removed from the library") {
        FixedBitVector<NUM_SPELLS> library;
        library.set(SPELL_STRIKING, true);

        remove_removed_library_spells(library);

        REQUIRE(library[SPELL_STRIKING] == false);
    }

    SECTION ("non-removed spells are not removed from the library") {
        FixedBitVector<NUM_SPELLS> library;
        library.set(SPELL_MAGIC_DART, true);

        remove_removed_library_spells(library);

        REQUIRE(library[SPELL_MAGIC_DART] == true);
    }

    SECTION ("non-present spells are not added to the library") {
        FixedBitVector<NUM_SPELLS> library;
        library.set(SPELL_MAGIC_DART, false);

        remove_removed_library_spells(library);

        REQUIRE(library[SPELL_MAGIC_DART] == false);
    }
}

TEST_CASE( "Monster spell unmarshalling can remove removed spells", "[single-file]" ) {

    SECTION ("removed spells are removed from the spell list") {
        vector<unsigned char> input = {
            0x01, // count
            0x00, 0x07, // SPELL_STRIKING
            0x01, // frequency
            0x00, 0x10, // MON_SPELL_WIZARD
        };
        auto r = reader(input);
        r.setMinorVersion(TAG_MINOR_TRACK_REGEN_ITEMS);
        const auto hd = 30;

        monster_spells spells;
        unmarshallSpells(r, spells, hd);

        REQUIRE(spells.empty());
        REQUIRE(r.valid() == false);
    }

    SECTION ("non-removed spells are not removed from the spell list") {
        vector<unsigned char> input = {
            0x01, // count
            0x00, 0x03, // SPELL_MAGIC_DART
            0x01, // frequency
            0x00, 0x10, // MON_SPELL_WIZARD
        };
        auto r = reader(input);
        r.setMinorVersion(TAG_MINOR_TRACK_REGEN_ITEMS);
        const auto hd = 30;

        monster_spells spells;
        unmarshallSpells(r, spells, hd);

        REQUIRE(spells.size() == 1);
        REQUIRE(r.valid() == false);
    }
}

TEST_CASE( "Basic marshalling/unmarshalling works correctly.", "[single-file]" ) {

    void marshallMapCell (writer &, const map_cell &);
    void unmarshallMapCell (reader &, map_cell& cell);

    SECTION ("Short integers can be roundtripped.") {
        rng::subgenerator subgen(0, 0);

        for (auto i = 0; i < 1000; i++)
        {
            short number = random_range(INT16_MIN, INT16_MAX);

            vector<unsigned char> buf;
            auto w = writer(&buf);
            marshallShort(w, number);

            auto r = reader(buf);
            const auto roundtrip_number = unmarshallShort(r);

            REQUIRE(number == roundtrip_number);
            REQUIRE(r.valid() == false);
        }
    }

    SECTION ("Map cells can be roundtripped.") {
        auto roundtrip_map_cell = [](const map_cell cell) {
            vector<unsigned char> buf;
            auto w = writer(&buf);
            marshallMapCell(w, cell);

            auto r = reader(buf);
            map_cell roundtrip_cell;
            unmarshallMapCell(r, roundtrip_cell);

            // TODO: Compare other members.
            REQUIRE(cell.flags == roundtrip_cell.flags);
            REQUIRE(r.valid() == false);
        };

        map_cell cell;

        cell.flags = 129;
        roundtrip_map_cell(cell);

        cell.flags = 32769;
        roundtrip_map_cell(cell);

        random_device rd;
        mt19937 generator(rd());
        uniform_int_distribution<uint32_t> distribution(0, UINT32_MAX);
        for (auto i = 0; i < 1000; i++)
        {
            cell.flags = distribution(generator);
            roundtrip_map_cell(cell);
        }
    }
}
