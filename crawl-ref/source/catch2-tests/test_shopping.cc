#include "catch_amalgamated.hpp"

#include "AppHdr.h"

#include "items.h"
#include "item-status-flag-type.h"
#include "item-name.h"
#include "player.h"
#include "shopping.h"

#include "test_player_fixture.h"

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Shop owned count returns matching potion stack quantity", "[shopping]")
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
                 "Shop owned count ignores nonstackable items", "[shopping]")
{
    item_def dagger;
    get_item_by_exact_name(dagger, "dagger");
    you.inv[0] = dagger;

    REQUIRE(shop_owned_consumable_count(dagger) == 0);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Shop owned count returns charges for a matching known wand",
                 "[shopping]")
{
    item_def flame_wand;
    get_item_by_exact_name(flame_wand, "wand of flame");
    flame_wand.charges = 5;
    flame_wand.flags |= ISFLAG_IDENTIFIED;
    you.type_ids[OBJ_WANDS][flame_wand.sub_type] = true;
    you.inv[0] = flame_wand;

    REQUIRE(shop_owned_consumable_count(flame_wand) == 5);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Shop owned count returns matching throwable quantity",
                 "[shopping]")
{
    item_def javelin;
    get_item_by_exact_name(javelin, "javelin");
    javelin.quantity = 3;
    you.inv[0] = javelin;

    REQUIRE(shop_owned_consumable_count(javelin) == 3);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Shop owned count hides an unknown shop wand", "[shopping]")
{
    item_def shop_wand;
    get_item_by_exact_name(shop_wand, "wand of iceblast");
    shop_wand.flags |= ISFLAG_IDENTIFIED;
    you.type_ids[OBJ_WANDS][shop_wand.sub_type] = false;

    item_def inventory_wand = shop_wand;
    inventory_wand.flags &= ~ISFLAG_IDENTIFIED;
    inventory_wand.charges = 4;
    you.inv[0] = inventory_wand;

    REQUIRE(shop_item_unknown(shop_wand));
    REQUIRE(shop_owned_consumable_count(shop_wand) == 0);
}
