#include "catch_amalgamated.hpp"

#include "AppHdr.h"

#include "artefact.h"
#include "art-enum.h"
#include "items.h"
#include "item-name.h"
#include "item-use.h"
#include "item-prop.h"
#include "item-prop-enum.h"
#include "item-status-flag-type.h"
#include "invent.h"
#include "player-equip.h"
#include "potion-type.h"
#include "species.h"
#include "tags.h"
#include "makeitem.h"
#include "env.h"
#include "uncancellable-type.h"

#include "test_player_fixture.h"

TEST_CASE( "Create item def", "[single-file]" ) {
    item_def foo;
}

TEST_CASE( "Create a specific item def", "[single-file]" ) {
    item_def scroll_of_fear;

    get_item_by_exact_name(scroll_of_fear, "scroll of fear");

    REQUIRE(scroll_of_fear.base_type == OBJ_SCROLLS);
    REQUIRE(scroll_of_fear.sub_type == SCR_FEAR);
}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Check mock player has no items", "[single-file]" ) {
    for (int i=0; i < ENDOFPACK; i++)
        REQUIRE(you.inv[i].base_type == OBJ_UNASSIGNED);

    REQUIRE(!any_items_of_type(OSEL_ANY));

}

static item_def simple_create_item(object_class_type base_type,
                                   int sub_type, int plus = 0,
                                   special_armour_type ego = SPARM_NORMAL);

static item_def simple_create_item(object_class_type base_type,
                                   int sub_type, int plus,
                                   special_armour_type ego){
    item_def item;

    item.clear();

    item.base_type = base_type;
    item.sub_type = sub_type;
    item.quantity = 1;
    item.plus = plus;
    item.brand = ego;

    return item;
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Brand Weapon saves become Bless Item", "[single-file][scroll-compat]")
{
    const int subtype = GENERATE(SCR_BRAND_WEAPON, SCR_BLESS_ITEM);
    item_def original = simple_create_item(OBJ_SCROLLS, subtype);
    original.rnd = 1;
    original.quantity = 3;
    original.inscription = "saved scrolls";
    vector<unsigned char> bytes;
    writer output(&bytes);
    marshallItem(output, original);
    reader input(bytes, TAG_MINOR_VERSION);
    item_def restored;
    unmarshallItem(input, restored);
    REQUIRE(restored.is_type(OBJ_SCROLLS, SCR_BLESS_ITEM));
    REQUIRE(restored.quantity == 3);
    REQUIRE(restored.inscription == original.inscription);
    REQUIRE(item_type_removed(OBJ_SCROLLS, SCR_BRAND_WEAPON));
    REQUIRE_FALSE(item_type_removed(OBJ_SCROLLS, SCR_BLESS_ITEM));
    REQUIRE(consumable_rarity(OBJ_SCROLLS, SCR_BRAND_WEAPON) == RARITY_NONE);
    REQUIRE(consumable_rarity(OBJ_SCROLLS, SCR_BLESS_ITEM) != RARITY_NONE);
}

TEST_CASE("Saved scroll action IDs remain stable", "[single-file][scroll-compat]")
{
    REQUIRE(UNC_BRAND_WEAPON == 6);
    REQUIRE(UNC_BLESS_ITEM == 7);
    REQUIRE(UNC_AMNESIA == 8);
    REQUIRE(UNC_BLINKING == 9);
    REQUIRE(UNC_IDENTIFY == 10);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Legacy Brand Weapon requests create Bless Item", "[single-file][scroll-compat]")
{
    const int index = items(false, OBJ_SCROLLS, SCR_BRAND_WEAPON, 1);
    REQUIRE(index != NON_ITEM);
    CHECK(env.item[index].is_type(OBJ_SCROLLS, SCR_BLESS_ITEM));
    destroy_item(index);
}

TEST_CASE_METHOD(MockPlayerYouTestsFixture,
                 "Felids can enchant boots with known scrolls", "[single-file]")
{
    you.species = SP_FELID;
    give_basic_mutations(SP_FELID);
    REQUIRE(you.has_mutation(MUT_NO_GRASPING));

    item_def scroll = simple_create_item(OBJ_SCROLLS, SCR_ENCHANT_ARMOUR);
    you.inv[0] = simple_create_item(OBJ_ARMOUR, ARM_BOOTS);
    item_def &boots = you.inv[0];
    boots.flags |= ISFLAG_IDENTIFIED;
    REQUIRE(can_equip_item(boots));

    SECTION("Unknown and known scrolls both allow enchanting")
    {
        const bool known = GENERATE(false, true);
        you.type_ids[OBJ_SCROLLS][SCR_ENCHANT_ARMOUR] = known;
        REQUIRE(cannot_read_item_reason(&scroll).empty());
        REQUIRE(any_items_of_type(OSEL_ENCHANTABLE_ARMOUR));
        REQUIRE(enchant_armour(boots, true));
        REQUIRE(boots.plus == 1);
    }

    SECTION("Known scrolls still require an enchantable target")
    {
        you.type_ids[OBJ_SCROLLS][SCR_ENCHANT_ARMOUR] = true;
        boots.plus = armour_max_enchant(boots);
        REQUIRE_FALSE(any_items_of_type(OSEL_ENCHANTABLE_ARMOUR));
        REQUIRE_FALSE(cannot_read_item_reason(&scroll).empty());
        REQUIRE_FALSE(enchant_armour(boots, true));
    }

    SECTION("Weapon enhancement remains blocked")
    {
        scroll.sub_type = SCR_ENCHANT_WEAPON;
        REQUIRE(cannot_read_item_reason(&scroll, false, true)
                == "There's no point in enhancing weapons you can't use!");
    }
}

static int find_inv_index_with_exact_item(object_class_type base_type, int sub_type){

    for (int i=0; i < ENDOFPACK; i++)
    {
        item_def item = you.inv[i];
        if (item.base_type == base_type
               && item.sub_type == sub_type)
        {
            return i;
        }
    }

    return -1;
}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Give mock player item", "[single-file]" ) {
    item_def scroll_of_fear;

    get_item_by_exact_name(scroll_of_fear, "scroll of fear");

    move_item_to_inv(scroll_of_fear);

    REQUIRE(any_items_of_type(OSEL_ANY));

    int num_fear_stacks_in_invent = 0;

    for (int i=0; i < ENDOFPACK; i++)
    {
        item_def item = you.inv[i];
        if (item.base_type == OBJ_SCROLLS
               && item.sub_type == SCR_FEAR)
        {
            num_fear_stacks_in_invent++;
        }
    }

    REQUIRE(num_fear_stacks_in_invent == 1);
}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Mock player armour skill", "[single-file]" ) {
    REQUIRE(you.skill(SK_ARMOUR,20) == 0);
    REQUIRE(you.strength() == 11);
}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Give mock armour", "[single-file]" ) {

    item_def armour = simple_create_item(OBJ_ARMOUR, ARM_SCALE_MAIL);

    move_item_to_inv(armour);

    REQUIRE(you.base_ac_from(armour, 100) == 600);

    REQUIRE(any_items_of_type(OSEL_ANY));

    int num_armours = 0;

    for (int i=0; i < ENDOFPACK; i++)
    {
        item_def item = you.inv[i];
        if (item.base_type == OBJ_ARMOUR
               && item.sub_type == ARM_SCALE_MAIL)
        {
            num_armours++;
        }
    }

    REQUIRE(num_armours == 1);

    REQUIRE(you.base_ac(100) == 0);
}

static void make_and_equip_item(object_class_type base_type, int sub_type,
                                int plus = 0,
                                special_armour_type ego = SPARM_NORMAL);

static void find_and_equip_exact_item(item_def item){
    move_item_to_inv(item);

    int index =
        find_inv_index_with_exact_item(item.base_type, item.sub_type);

    REQUIRE(index != -1);

    equip_item(get_all_item_slots(item)[0], index);
}

static void make_and_equip_item(object_class_type base_type, int sub_type,
                                int plus, special_armour_type ego){

    item_def item = simple_create_item(base_type, sub_type, plus,
                                       ego);
    find_and_equip_exact_item(item);

}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Give mock armour and check after after putting it on",
                  "[single-file]" ) {

    make_and_equip_item(OBJ_ARMOUR, ARM_SCALE_MAIL);

    REQUIRE(you.base_ac(100) == 600);
}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Give mock robe and check after after putting it on",
                  "[single-file]" ) {

    make_and_equip_item(OBJ_ARMOUR, ARM_ROBE);

    REQUIRE(you.base_ac(100) == 200);
}

TEST_CASE_METHOD( MockPlayerYouTestsFixture,
                  "Give multiple mock items and check after putting it on",
                  "[single-file]" ) {

    make_and_equip_item(OBJ_ARMOUR, ARM_FIRE_DRAGON_ARMOUR);
    make_and_equip_item(OBJ_ARMOUR, ARM_HELMET);
    make_and_equip_item(OBJ_ARMOUR, ARM_BOOTS);
    make_and_equip_item(OBJ_ARMOUR, ARM_GLOVES);
    make_and_equip_item(OBJ_ARMOUR, ARM_CLOAK);

    REQUIRE(you.base_ac(100) == 1200);
}

TEST_CASE("armour_prop_test", "[single-file]"){
    REQUIRE(armour_prop(ARM_SCALE_MAIL, PARM_AC) == 6);
}

TEST_CASE("Test all_item_subtypes() does not include removed items",
          "[single-file]") {
    const auto items = all_item_subtypes(OBJ_POTIONS);

    const auto has_removed_item = find(items.begin(), items.end(), POT_POISON) != items.end();

    REQUIRE(has_removed_item == false);
}

TEST_CASE("Test all_item_subtypes() does include items for each category",
          "[single-file]") {
    // Note: CORPSES and RODS do not have any sub-items, so are excluded.
    REQUIRE(all_item_subtypes(OBJ_WEAPONS).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_MISSILES).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_ARMOUR).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_WANDS).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_SCROLLS).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_JEWELLERY).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_POTIONS).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_BOOKS).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_STAVES).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_ORBS).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_MISCELLANY).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_GOLD).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_RUNES).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_TALISMANS).size() > 0);
    REQUIRE(all_item_subtypes(OBJ_GEMS).size() > 0);
}
