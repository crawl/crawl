#include "AppHdr.h"

#include "end.h"
#include "game-type.h"
#include "items.h"
#include "mutation.h"
#include "newgame-def.h"
#include "ng-setup.h"
#include "player.h"
#include "player-equip.h"
#include "item-prop.h"

#include "test_player_fixture.h"

// The way this test fixture generates a "mock" player object is a total
// hack which makes crawl's startup code even worse. That said, it is
// very useful for making safer changes to player class, which makes it
// a net gain for code cleanliness. Ideally somebody will refactor
// crawl's startup code to cleanly isolate globals for testing purposes.

// This TestFixture has not been verified to work for anything involving
// items, and definitely doesn't work for anything which requires any
// dungeon or branch levels to exist.
template<typename T>
MockPlayerYouTestsFixture<T>::MockPlayerYouTestsFixture() {
    you = player();
    newgame_def game_choices = (T()).game_options();
    // TODO: this is a hack, but without this, giving items doesn't work at all
    // you.char_class = game_choices.job;
    // TODO: make non-monk classes work
    setup_game(game_choices, false);

    if (item_def* armour = you.body_armour())
        unequip_item(*armour);

    destroy_items_in_player_inventory();

    init_mut_index();
    init_properties();
}

template<typename T>
MockPlayerYouTestsFixture<T>::~MockPlayerYouTestsFixture() {
    delete_files();
}

void destroy_items_in_player_inventory(){

    vector<item_def*> eq = you.equipment.get_slot_items(SLOT_ALL_EQUIPMENT, true);
    for (item_def* item : eq)
        unequip_item(*item);

    // XXX: This is apparently how you destroy items in inventory?
    for (int i=0; i < ENDOFPACK; i++)
    {
        you.inv[i].base_type = OBJ_UNASSIGNED;
        you.inv[i].quantity = 0;
        you.inv[i].pos.reset();
        you.inv[i].props.clear();
    }
}

newgame_def HumanMonkOptions::game_options()
{
    newgame_def game_choices;
    game_choices.name = "TestChar";
    game_choices.type = GAME_TYPE_NORMAL;
    game_choices.filename = "this_should_never_be_a_name_of_a_file"
                            "_in_a_directory_check"
                            "_catch2_tests_slash_test_player_cc";

    game_choices.species = SP_HUMAN;
    game_choices.job = JOB_MONK;
    game_choices.weapon = WPN_UNARMED;

    return game_choices;
}
// Explicitly instantiate template, so that we don't have to pollute the header.
template class MockPlayerYouTestsFixture<HumanMonkOptions>;

newgame_def CoglinMonkOptions::game_options()
{
    newgame_def game_choices;
    game_choices.name = "TestCharCo";
    game_choices.type = GAME_TYPE_NORMAL;
    game_choices.filename = "this_should_never_be_a_name_of_a_file"
                            "_in_a_directory_check"
                            "_catch2_tests_slash_test_player_cc2";

    game_choices.species = SP_COGLIN;
    game_choices.job = JOB_MONK;
    game_choices.weapon = WPN_SHORTBOW;
    return game_choices;
}

// Explicitly instantiate template, so that we don't have to pollute the header.
template class MockPlayerYouTestsFixture<CoglinMonkOptions>;

newgame_def OctopodeMonkOptions::game_options()
{
    newgame_def game_choices;
    game_choices.name = "TestCharOp";
    game_choices.type = GAME_TYPE_NORMAL;
    game_choices.filename = "this_should_never_be_a_name_of_a_file"
                            "_in_a_directory_check"
                            "_catch2_tests_slash_test_player_cc3";

    game_choices.species = SP_OCTOPODE;
    game_choices.job = JOB_MONK;
    return game_choices;
}

// Explicitly instantiate template, so that we don't have to pollute the header.
template class MockPlayerYouTestsFixture<OctopodeMonkOptions>;
