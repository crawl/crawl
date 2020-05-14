#include <random>

#include "catch.hpp"

#include "AppHdr.h"

#include "test_player_fixture.h"

#include "skill-menu.h"

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
}
