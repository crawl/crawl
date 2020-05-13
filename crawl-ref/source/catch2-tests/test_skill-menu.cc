#include <random>

#include "catch.hpp"

#include "AppHdr.h"

#include "test_player_fixture.h"

#include "items.h"
#include "skill-menu.h"
#include "skills.h"

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
        "Test skill menu label formatting", "[single-file]" ) {

    SECTION ("No label is shown for skills not being trained") {
        you.training[SK_FIGHTING] = 0;

        const auto label = _skill_training_label(SK_FIGHTING);

        REQUIRE(label.tostring() == "");
    }

    SECTION ("A label is shown for skills being trained") {
        you.training[SK_FIGHTING] = 42;
        REQUIRE(_skill_training_label(SK_FIGHTING).tostring() == "42%");

        you.training[SK_FIGHTING] = 100;
        REQUIRE(_skill_training_label(SK_FIGHTING).tostring() == "100%");

        you.training[SK_FIGHTING] = 1;
        REQUIRE(_skill_training_label(SK_FIGHTING).tostring() == " 1%");
    }

    SECTION ("Aptitudes are shown with a +/- sign, except zero") {
        you.species = SP_CENTAUR;
        REQUIRE(species_apt(SK_STAVES, SP_CENTAUR) == -1);
        REQUIRE(species_apt(SK_FIGHTING, SP_CENTAUR) == 0);
        REQUIRE(species_apt(SK_THROWING, SP_CENTAUR) == 1);

        REQUIRE(_skill_aptitude_label(SK_STAVES).tostring() == "-1");
        REQUIRE(_skill_aptitude_label(SK_FIGHTING).tostring() == " 0");
        REQUIRE(_skill_aptitude_label(SK_THROWING).tostring() == "+1");
    }

    SECTION ("Aptitudes with a manual are shown with a +4") {
        you.species = SP_CENTAUR;
        REQUIRE(species_apt(SK_FIGHTING, SP_CENTAUR) == 0);

        item_def manual;
        manual.base_type = OBJ_BOOKS;
        manual.sub_type = BOOK_MANUAL;
        manual.skill = SK_FIGHTING;
        manual.skill_points = 1;
        move_item_to_inv(manual);

        REQUIRE(_skill_aptitude_label(SK_FIGHTING).tostring() == " 0+4");
    }
}
