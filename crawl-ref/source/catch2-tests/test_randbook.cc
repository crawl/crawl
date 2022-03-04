#include <random>

#include "catch.hpp"

#include "AppHdr.h"

#include "randbook.h"
#include "spl-book.h"
#include "spl-util.h"

TEST_CASE( "When setting book spell list", "[single-file]" ) {

    item_def book;

    SECTION("spells will be padded with SPELL_NO_SPELL") {
        init_spell_descs();
        vector<spell_type> spells = {SPELL_MAGIC_DART, SPELL_CAUSE_FEAR};

        _set_book_spell_list(book, spells);

        const auto& book_spells = book.props[SPELL_LIST_KEY].get_vector();
        REQUIRE(book_spells.size() == RANDBOOK_SIZE);
        for (auto i = 2; i < RANDBOOK_SIZE; i++ )
            REQUIRE(book_spells[i].get_int() == SPELL_NO_SPELL);
    }

    SECTION("the spell list will be truncated to RANDBOOK_SIZE entries") {
        init_spell_descs();
        vector<spell_type> spells = {
            SPELL_FREEZE,
            SPELL_NECROTIZE,
            SPELL_APPORTATION,
            SPELL_SUMMON_SMALL_MAMMAL,
            SPELL_MAGIC_DART,
            SPELL_SHOCK,
            SPELL_SANDBLAST,
            SPELL_FOXFIRE,
            SPELL_BEASTLY_APPENDAGE,
            SPELL_STING,
        };

        _set_book_spell_list(book, spells);

        const auto& book_spells = book.props[SPELL_LIST_KEY].get_vector();
        REQUIRE(book_spells.size() == RANDBOOK_SIZE);
    }
}
