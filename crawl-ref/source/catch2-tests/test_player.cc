#include "catch.hpp"

#include "AppHdr.h"
#include "end.h"
#include "mutation.h"
#include "ng-setup.h"

#include "player.h"

// The way this test fixture generates a "mock" player object is a total
// hack which makes crawl's startup code even worse. That said, it is
// very useful for making safer changes to player class, which makes it
// a net gain for code cleanliness. Ideally somebody will refactor
// crawl's startup code to cleanly isolate globals for testing purposes.

// This TestFixture has not been verified to work for anything involving
// items, and definitely doesn't work for anything which requires any
// dungeon or branch levels to exist.
class MockPlayerYouTestsFixture {
    public:
    MockPlayerYouTestsFixture() {
        you = player();
        newgame_def game_choices;
        game_choices.name = "TestChar";
        game_choices.type = GAME_TYPE_NORMAL;
        game_choices.filename = "this_should_never_be_a_name_of_a_file"
                                "_in_a_directory_check"
                                "_catch2_tests_slash_test_player_cc";

        game_choices.species = SP_HUMAN;
        game_choices.job = JOB_MONK;
        game_choices.weapon = WPN_UNARMED;

        setup_game(game_choices, false);
    }

    ~MockPlayerYouTestsFixture() {
        delete_files();
    }

};

/* I'm sure there's a name for this, but remember that if you want to
 * refactor some monster method, you can create a copy of it so that you
 * can test your new, refactored version has the same behavior as the
 * old, spaghetti version.
 *
 * Here is an example. I copied player::base_ac to player::new_base_ac.
 * Then I was able to refactor player::new_base_ac, confident I was
 * preserving behavior. My final change was to rename new_base_ac to
 * base_ac, delete the old base_ac function, and delete the new_base_ac
 * function header.
 */

/*
TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Monte Carlo mutation ac testing", "[single-file]"){
    mutation_type mut;

    int mutation_number;

    // Ideally we won't use random (Monte Carlo) testing. It would be
    // preferable to somehow iterate through all possible cases and test
    // each of them. But scales mutations conflict in complicated ways,
    // so it was significantly easier just to use Monte Carlo method.
    for (int i=0; i<99999; i++){
        mut = random_choose(MUT_TOUGH_SKIN, MUT_SHAGGY_FUR,
                           MUT_GELATINOUS_BODY, MUT_IRIDESCENT_SCALES,
                           MUT_NO_POTION_HEAL, MUT_RUGGED_BROWN_SCALES,
                           MUT_ICY_BLUE_SCALES, MUT_MOLTEN_SCALES,
                           MUT_SLIMY_GREEN_SCALES, MUT_THIN_METALLIC_SCALES,
                           MUT_YELLOW_SCALES, MUT_PHYSICAL_VULNERABILITY);

        mutation_number = random_choose(1,2,3);

        for (int j=0; j < mutation_number; j++)
            mutate(mut, "testing");

        if (one_chance_in(100)){
            delete_all_mutations("testing");
        }

        REQUIRE(you.base_ac(100) == you.new_base_ac(100));

    }
}
*/

// Some of these a characterization tests which should be justifiably
// removed if the behavior ever changes. Left in for now because it
// should be easy to tell the difference between a intentional change of
// the behavior being tested and accidentally changing said behavior.

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
