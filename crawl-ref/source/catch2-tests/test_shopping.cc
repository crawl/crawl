#include "catch_amalgamated.hpp"

#include "AppHdr.h"

#include "items.h"
#include "item-status-flag-type.h"
#include "player.h"
#include "shopping.h"

#include "test_player_fixture.h"

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Shop owned count sums matching scroll stacks", "[shopping]")
{
    item_def fear_scroll;
    get_item_by_exact_name(fear_scroll, "scroll of fear");
    fear_scroll.quantity = 3;
    you.inv[0] = fear_scroll;

    item_def identify_scroll;
    get_item_by_exact_name(identify_scroll, "scroll of identify");
    identify_scroll.quantity = 2;
    you.inv[1] = identify_scroll;

    item_def another_fear_scroll = fear_scroll;
    another_fear_scroll.quantity = 2;
    you.inv[2] = another_fear_scroll;

    REQUIRE(shop_owned_consumable_count(fear_scroll) == 5);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Shop owned count sums matching potion stacks", "[shopping]")
{
    item_def haste_potion;
    get_item_by_exact_name(haste_potion, "potion of haste");
    haste_potion.quantity = 2;
    you.inv[0] = haste_potion;

    REQUIRE(shop_owned_consumable_count(haste_potion) == 2);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Shop owned count does not reveal unknown item types", "[shopping]")
{
    item_def inventory_scroll;
    get_item_by_exact_name(inventory_scroll, "scroll of fear");
    inventory_scroll.quantity = 3;
    inventory_scroll.flags &= ~ISFLAG_IDENTIFIED;
    you.inv[0] = inventory_scroll;

    item_def shop_scroll;
    get_item_by_exact_name(shop_scroll, "scroll of fear");

    REQUIRE(shop_owned_consumable_count(shop_scroll) == 0);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Shop owned count is limited to potions and scrolls", "[shopping]")
{
    item_def dagger;
    get_item_by_exact_name(dagger, "dagger");
    dagger.quantity = 2;
    you.inv[0] = dagger;

    REQUIRE(shop_owned_consumable_count(dagger) == 0);
}
