#pragma once

// This TestFixture has not been verified to work for anything involving
// items, and definitely doesn't work for anything which requires any
// dungeon or branch levels to exist.
class MockPlayerYouTestsFixture {
    public:
    MockPlayerYouTestsFixture(void);

    ~MockPlayerYouTestsFixture(void);
};

void destroy_items_in_player_inventory();
/* I'm sure there's a name for this, but remember that if you want to
 * refactor some monster method, you can create a copy of it so that you
 * can test your new, refactored version has the same behaviour as the
 * old, spaghetti version.
 *
 * Here is an example. I copied player::base_ac to player::new_base_ac.
 * Then I was able to refactor player::new_base_ac, confident I was
 * preserving behaviour. My final change was to rename new_base_ac to
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
