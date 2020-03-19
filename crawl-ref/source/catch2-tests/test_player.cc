#include "catch.hpp"

#include "AppHdr.h"
#include "mutation.h"
#include "player.h"

#include "test_player_fixture.h"

// Some of these a characterization tests which should be justifiably
// removed if the behaviour ever changes. Left in for now because it
// should be easy to tell the difference between a intentional change of
// the behaviour being tested and accidentally changing said behaviour.
//
TEST_CASE_METHOD(MockPlayerYouTestsFixture,
          "Test MockPlayerYouTestsFixture", "[single-file]" ) {

    REQUIRE(you.is_player());
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture, "Can mutate player",
        "[single-file]" ) {
    mutate(MUT_ICY_BLUE_SCALES, "testing");
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
          "Check single AC mutation", "[single-file]" ) {
    mutate(MUT_ICY_BLUE_SCALES, "testing");

    REQUIRE(you.base_ac(100) == 200);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
          "Check multiple AC mutation", "[single-file]" ) {

    mutate(MUT_ICY_BLUE_SCALES, "testing");

    mutate(MUT_GELATINOUS_BODY, "testing");

    REQUIRE(you.base_ac(100) == 300);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
          "Check each mutation once", "[single-file]" ) {

    mutate(MUT_ICY_BLUE_SCALES, "testing");

    REQUIRE(you.base_ac(100) == 200);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
          "Check gelatinous_body", "[single-file]" ) {
    mutate(MUT_GELATINOUS_BODY, "testing");

    REQUIRE(you.base_ac(100) == 100);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
          "Check tough_skin", "[single-file]" ) {

    mutate(MUT_TOUGH_SKIN, "testing");

    REQUIRE(you.base_ac(100) == 100);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
          "Check shaggy fur", "[single-file]" ) {

    mutate(MUT_SHAGGY_FUR, "testing");

    REQUIRE(you.base_ac(100) == 100);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
          "Check iridescent scales", "[single-file]" ) {

    mutate(MUT_IRIDESCENT_SCALES, "testing");

    REQUIRE(you.base_ac(100) == 200);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
          "Check no potion heal ac (expect: no change)", "[single-file]" ) {

    mutate(MUT_NO_POTION_HEAL, "testing");

    REQUIRE(you.base_ac(100) == 0);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
          "Check rugged brown scales ac", "[single-file]" ) {

    mutate(MUT_RUGGED_BROWN_SCALES, "testing");

    REQUIRE(you.base_ac(100) == 100);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
          "Check molten scales ac", "[single-file]" ) {

    mutate(MUT_MOLTEN_SCALES, "testing");

    REQUIRE(you.base_ac(100) == 200);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
          "Check physical vuln", "[single-file]" ) {

    mutate(MUT_PHYSICAL_VULNERABILITY, "testing");

    REQUIRE(you.base_ac(100) == -500);

    mutate(MUT_PHYSICAL_VULNERABILITY, "testing");

    REQUIRE(you.base_ac(100) == -1000);

    mutate(MUT_PHYSICAL_VULNERABILITY, "testing");

    REQUIRE(you.base_ac(100) == -1500);
}
