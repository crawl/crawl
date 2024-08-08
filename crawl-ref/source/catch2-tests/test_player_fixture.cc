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

MockPlayerYouTestsFixture::MockPlayerYouTestsFixture() {
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

    unequip_item(EQ_BODY_ARMOUR);

    destroy_items_in_player_inventory();

    init_mut_index();
    init_properties();
}

MockPlayerYouTestsFixture::~MockPlayerYouTestsFixture() {
    delete_files();
}

void destroy_items_in_player_inventory(){

    for (int eq = EQ_MIN_ARMOUR; eq <= EQ_MAX_ARMOUR; ++eq)
        unequip_item(static_cast<equipment_type>(eq));

    // XXX: This is apparently how you destroy items in inventory?
    for (int i=0; i < ENDOFPACK; i++)
    {
        you.inv[i].base_type = OBJ_UNASSIGNED;
        you.inv[i].quantity = 0;
        you.inv[i].pos.reset();
        you.inv[i].props.clear();
    }
}
