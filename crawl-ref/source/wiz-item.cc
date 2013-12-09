/**
 * @file
 * @brief Item related wizard functions.
**/

#include "AppHdr.h"

#include "wiz-item.h"

#include <errno.h>

#include "acquire.h"
#include "act-iter.h"
#include "art-enum.h"
#include "artefact.h"
#include "coordit.h"
#include "message.h"
#include "cio.h"
#include "dbg-util.h"
#include "decks.h"
#include "effects.h"
#include "env.h"
#include "godpassive.h"
#include "itemprop.h"
#include "items.h"
#include "invent.h"
#include "libutil.h"
#include "makeitem.h"
#include "mapdef.h"
#include "misc.h"
#include "mon-stuff.h"
#include "options.h"
#include "output.h"
#include "player-equip.h"
#include "religion.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-util.h"
#include "stash.h"
#include "stuff.h"
#include "terrain.h"

#ifdef WIZARD
static void _make_all_books()
{
    for (int i = 0; i < NUM_FIXED_BOOKS; ++i)
    {
        int thing = items(0, OBJ_BOOKS, i, true, 0, MAKE_ITEM_NO_RACE,
                          0, 0, AQ_WIZMODE);
        if (thing == NON_ITEM)
            continue;

        move_item_to_grid(&thing, you.pos());

        if (thing == NON_ITEM)
            continue;

        item_def book(mitm[thing]);

        mark_had_book(book);
        set_ident_flags(book, ISFLAG_KNOW_TYPE);
        set_ident_flags(book, ISFLAG_IDENT_MASK);

        mprf("%s", book.name(DESC_PLAIN).c_str());
    }
}

void wizard_create_spec_object_by_name()
{
    char buf[1024];
    mprf(MSGCH_PROMPT, "Enter name of item (or ITEM spec): ");
    if (cancellable_get_line_autohist(buf, sizeof buf) || !*buf)
    {
        canned_msg(MSG_OK);
        return;
    }

    string error;
    create_item_named(buf, you.pos(), &error);
    if (!error.empty())
    {
        mprf(MSGCH_ERROR, "Error: %s", error.c_str());
        return;
    }
}

//---------------------------------------------------------------
//
// create_spec_object
//
//---------------------------------------------------------------
void wizard_create_spec_object()
{
    char           specs[80];
    ucs_t          keyin;
    monster_type   mon;
    object_class_type class_wanted;
    int            thing_created;

    do
    {
        mprf(MSGCH_PROMPT, ") - weapons     ( - missiles  [ - armour  / - wands    ?  - scrolls");
        mprf(MSGCH_PROMPT, "= - jewellery   ! - potions   : - books   | - staves   \\  - rods");
        mprf(MSGCH_PROMPT, "} - miscellany  X - corpses   %% - food    $ - gold     0  - the Orb");
        mprf(MSGCH_PROMPT, "ESC - exit");

        msgwin_prompt("What class of item? ");

        keyin = towupper(get_ch());

        class_wanted = item_class_by_sym(keyin);
        if (key_is_escape(keyin) || keyin == ' '
                || keyin == '\r' || keyin == '\n')
        {
            msgwin_reply("");
            canned_msg(MSG_OK);
            return;
        }
    } while (class_wanted == NUM_OBJECT_CLASSES);

    msgwin_reply(make_stringf("%c", keyin));

    // Allocate an item to play with.
    thing_created = get_mitm_slot();
    if (thing_created == NON_ITEM)
    {
        mpr("Could not allocate item.");
        return;
    }

    // turn item into appropriate kind:
    if (class_wanted == OBJ_ORBS)
    {
        mitm[thing_created].base_type = OBJ_ORBS;
        mitm[thing_created].sub_type  = ORB_ZOT;
        mitm[thing_created].quantity  = 1;
    }
    else if (class_wanted == OBJ_GOLD)
    {
        int amount = prompt_for_int("How much gold? ", true);
        if (amount <= 0)
        {
            canned_msg(MSG_OK);
            return;
        }

        mitm[thing_created].base_type = OBJ_GOLD;
        mitm[thing_created].sub_type  = 0;
        mitm[thing_created].quantity  = amount;
    }
    else if (class_wanted == OBJ_CORPSES)
    {
        mon = debug_prompt_for_monster();

        if (mon == MONS_NO_MONSTER || mon == MONS_PROGRAM_BUG)
        {
            mpr("No such monster.");
            return;
        }

        if (mons_weight(mon) == 0)
        {
            if (!yesno("That monster doesn't leave corpses; make one "
                       "anyway?", true, 'y'))
            {
                canned_msg(MSG_OK);
                return;
            }
        }

        if (mon >= MONS_DRACONIAN_CALLER && mon <= MONS_DRACONIAN_SCORCHER)
        {
            mpr("You can't make a draconian corpse by its background.");
            mon = MONS_DRACONIAN;
        }

        monster dummy;
        dummy.type = mon;

        if (mons_genus(mon) == MONS_HYDRA)
            dummy.number = prompt_for_int("How many heads? ", false);

        if (fill_out_corpse(&dummy, dummy.type,
                            mitm[thing_created], true) == MONS_NO_MONSTER)
        {
            mpr("Failed to create corpse.");
            mitm[thing_created].clear();
            return;
        }
    }
    else
    {
        string prompt = "What type of item? ";
        if (class_wanted == OBJ_BOOKS)
            prompt += "(\"all\" for all) ";
        msgwin_get_line_autohist(prompt, specs, sizeof(specs));

        string temp = specs;
        trim_string(temp);
        lowercase(temp);
        strlcpy(specs, temp.c_str(), sizeof(specs));

        if (class_wanted == OBJ_BOOKS && temp == "all")
        {
            _make_all_books();
            return;
        }

        if (specs[0] == '\0')
        {
            canned_msg(MSG_OK);
            return;
        }

        if (!get_item_by_name(&mitm[thing_created], specs, class_wanted, true))
        {
            mpr("No such item.");

            // Clean up item
            destroy_item(thing_created);
            return;
        }
    }

    // Deck colour (which control rarity) already set.
    if (!is_deck(mitm[thing_created]))
        item_colour(mitm[thing_created]);

    move_item_to_grid(&thing_created, you.pos());

    if (thing_created != NON_ITEM)
    {
        // orig_monnum is used in corpses for things like the Animate
        // Dead spell, so leave it alone.
        if (class_wanted != OBJ_CORPSES)
            origin_acquired(mitm[thing_created], AQ_WIZMODE);
        canned_msg(MSG_SOMETHING_APPEARS);

        // Tell the stash tracker.
        maybe_update_stashes();
    }
}

static const char* _prop_name[] =
{
    "Brand",
    "AC",
    "EV",
    "Str",
    "Int",
    "Dex",
    "rFire",
    "rCold",
    "rElec",
    "rPois",
    "rNeg",
    "MR",
    "SInv",
    "+Inv",
    "+Fly",
#if TAG_MAJOR_VERSION > 34
    "+Fog",
#endif
    "+Blnk",
    "+Rage",
    "Noisy",
    "-Cast",
    "*Tele",
    "-Tele",
    "*Rage",
    "Hungr",
    "Contm",
    "Acc",
    "Dam",
    "Curse",
    "Stlth",
    "MP",
    "Delay",
    "HP",
    "Clar",
    "BAcc",
    "BDam",
    "RMsl",
#if TAG_MAJOR_VERSION == 34
    "+Fog",
#endif
    "Regen",
    "noupg",
};

#define ARTP_VAL_BOOL 0
#define ARTP_VAL_POS  1
#define ARTP_VAL_ANY  2

static int8_t _prop_type[] =
{
    ARTP_VAL_POS,  //BRAND
    ARTP_VAL_ANY,  //AC
    ARTP_VAL_ANY,  //EVASION
    ARTP_VAL_ANY,  //STRENGTH
    ARTP_VAL_ANY,  //INTELLIGENCE
    ARTP_VAL_ANY,  //DEXTERITY
    ARTP_VAL_ANY,  //FIRE
    ARTP_VAL_ANY,  //COLD
    ARTP_VAL_BOOL, //ELECTRICITY
    ARTP_VAL_BOOL, //POISON
    ARTP_VAL_BOOL, //NEGATIVE_ENERGY
    ARTP_VAL_POS,  //MAGIC
    ARTP_VAL_BOOL, //EYESIGHT
    ARTP_VAL_BOOL, //INVISIBLE
    ARTP_VAL_BOOL, //FLIGHT
#if TAG_MAJOR_VERSION > 34
    ARTP_VAL_BOOL, //FOG
#endif
    ARTP_VAL_BOOL, //BLINK
    ARTP_VAL_BOOL, //BERSERK
    ARTP_VAL_POS,  //NOISES
    ARTP_VAL_BOOL, //PREVENT_SPELLCASTING
    ARTP_VAL_BOOL, //CAUSE_TELEPORTATION
    ARTP_VAL_BOOL, //PREVENT_TELEPORTATION
    ARTP_VAL_POS,  //ANGRY
    ARTP_VAL_POS,  //METABOLISM
    ARTP_VAL_POS,  //MUTAGENIC
    ARTP_VAL_ANY,  //ACCURACY
    ARTP_VAL_ANY,  //DAMAGE
    ARTP_VAL_POS,  //CURSED
    ARTP_VAL_ANY,  //STEALTH
    ARTP_VAL_ANY,  //MAGICAL_POWER
    ARTP_VAL_ANY,  //BASE_DELAY
    ARTP_VAL_ANY,  //HP
    ARTP_VAL_BOOL, //CLARITY
    ARTP_VAL_ANY,  //BASE_ACC
    ARTP_VAL_ANY,  //BASE_DAM
    ARTP_VAL_BOOL, //RMSL
#if TAG_MAJOR_VERSION == 34
    ARTP_VAL_BOOL, //FOG
#endif
    ARTP_VAL_ANY,  //REGENERATION
    ARTP_VAL_BOOL, //NO_UPGRADE
};

static void _tweak_randart(item_def &item)
{
    COMPILE_CHECK(ARRAYSZ(_prop_name) == ARTP_NUM_PROPERTIES);
    COMPILE_CHECK(ARRAYSZ(_prop_type) == ARTP_NUM_PROPERTIES);

    if (item_is_equipped(item))
    {
        mprf(MSGCH_PROMPT, "You can't tweak the randart properties of an equipped item.");
        return;
    }
    else
        mesclr();

    artefact_properties_t props;
    artefact_wpn_properties(item, props);

    string prompt = "";

    vector<unsigned int> choice_to_prop;
    for (unsigned int i = 0, choice_num = 0; i < ARTP_NUM_PROPERTIES; ++i)
    {
        if (_prop_name[i] == string("UNUSED"))
            continue;
        choice_to_prop.push_back(i);
        if (choice_num % 8 == 0 && choice_num != 0)
            prompt += "\n";

        char choice;
        char buf[80];

        if (choice_num < 26)
            choice = 'A' + choice_num;
        else if (choice_num < 'A' - '0' + 26)
        {
            // 0-9 then :;<=>?@ . Any higher would collide with letters.
            choice = '0' + choice_num - 26;
        }
        else
            choice = '-'; // Too many choices!

        if (props[i])
            snprintf(buf, sizeof(buf), "%c) <w>%-5s</w> ", choice, _prop_name[i]);
        else
            snprintf(buf, sizeof(buf), "%c) %-5s ", choice, _prop_name[i]);

        prompt += buf;

        choice_num++;
    }
    mprf(MSGCH_PROMPT, "%s", prompt.c_str());

    mprf(MSGCH_PROMPT, "Change which field? ");

    int keyin = toalower(get_ch());
    unsigned int  choice;

    if (isaalpha(keyin))
        choice = keyin - 'a';
    else if (keyin >= '0' && keyin < 'A')
        choice = keyin - '0' + 26;
    else
    {
        canned_msg(MSG_OK);
        return;
    }

    if (choice >= choice_to_prop.size())
    {
        canned_msg(MSG_HUH);
        return;
    }

    unsigned int prop = choice_to_prop[choice];
    ASSERT(prop < ARRAYSZ(_prop_type));

    int val;
    switch (_prop_type[prop])
    {
    case ARTP_VAL_BOOL:
        mprf(MSGCH_PROMPT, "Toggling %s to %s.", _prop_name[prop],
             props[prop] ? "off" : "on");
        artefact_set_property(item, static_cast<artefact_prop_type>(prop),
                             !props[prop]);
        break;

    case ARTP_VAL_POS:
        mprf(MSGCH_PROMPT, "%s was %d.", _prop_name[prop], props[prop]);
        val = prompt_for_int("New value? ", true);

        if (val < 0)
        {
            mprf(MSGCH_PROMPT, "Value for %s must be non-negative",
                 _prop_name[prop]);
            return;
        }
        artefact_set_property(item, static_cast<artefact_prop_type>(prop),
                             val);
        break;
    case ARTP_VAL_ANY:
        mprf(MSGCH_PROMPT, "%s was %d.", _prop_name[prop], props[prop]);
        val = prompt_for_int("New value? ", false);
        artefact_set_property(item, static_cast<artefact_prop_type>(prop),
                             val);
        break;
    }
}

void wizard_tweak_object(void)
{
    char specs[50];
    int keyin;

    int item = prompt_invent_item("Tweak which item? ", MT_INVLIST, -1);

    if (prompt_failed(item))
        return;

    if (item == you.equip[EQ_WEAPON])
        you.wield_change = true;

    const bool is_art = is_artefact(you.inv[item]);

    while (true)
    {
        int64_t old_val = 0; // flags are uint64_t, but they don't care

        while (true)
        {
            mprf_nocap("%s", you.inv[item].name(DESC_INVENTORY_EQUIP).c_str());

            mprf(MSGCH_PROMPT, "a - plus  b - plus2  c - %s  "
                               "d - quantity  e - flags  ESC - exit",
                               is_art ? "art props" : "special");

            mprf(MSGCH_PROMPT, "Which field? ");

            keyin = toalower(get_ch());

            if (keyin == 'a')
                old_val = you.inv[item].plus;
            else if (keyin == 'b')
                old_val = you.inv[item].plus2;
            else if (keyin == 'c')
                old_val = you.inv[item].special;
            else if (keyin == 'd')
                old_val = you.inv[item].quantity;
            else if (keyin == 'e')
                old_val = you.inv[item].flags;
            else if (key_is_escape(keyin) || keyin == ' '
                    || keyin == '\r' || keyin == '\n')
            {
                canned_msg(MSG_OK);
                return;
            }

            if (keyin >= 'a' && keyin <= 'e')
                break;
        }

        if (is_art && keyin == 'c')
        {
            _tweak_randart(you.inv[item]);
            continue;
        }

        if (keyin != 'e')
            mprf("Old value: %" PRId64" (0x%04" PRIx64")", old_val, old_val);
        else
            mprf("Old value: 0x%08" PRIx64, old_val);

        msgwin_get_line("New value? ", specs, sizeof(specs));
        if (specs[0] == '\0')
            return;

        char *end;
        const bool hex = (keyin == 'e');
        int64_t new_val = strtoll(specs, &end, hex ? 16 : 0);

        if (keyin == 'e' && new_val & ISFLAG_ARTEFACT_MASK
            && (!you.inv[item].props.exists(KNOWN_PROPS_KEY)
             || !you.inv[item].props.exists(ARTEFACT_PROPS_KEY)))
        {
            mpr("You can't set this flag on a non-artefact.");
            continue;
        }

        if (end == specs)
            return;

        if (keyin == 'a')
            you.inv[item].plus = new_val;
        else if (keyin == 'b')
            you.inv[item].plus2 = new_val;
        else if (keyin == 'c')
            you.inv[item].special = new_val;
        else if (keyin == 'd')
            you.inv[item].quantity = new_val;
        else if (keyin == 'e')
            you.inv[item].flags = new_val;
        else
            die("unhandled keyin");

        // cursedness might have changed
        ash_check_bondage();
        auto_id_inventory();
    }
}

// Returns whether an item of this type can be an artefact.
static bool _item_type_can_be_artefact(int type)
{
    return type == OBJ_WEAPONS || type == OBJ_ARMOUR || type == OBJ_JEWELLERY
           || type == OBJ_BOOKS;
}

static bool _make_book_randart(item_def &book)
{
    int type;

    do
    {
        mprf(MSGCH_PROMPT, "Make book fixed [t]heme or fixed [l]evel? ");
        type = toalower(getchk());
    }
    while (type != 't' && type != 'l');

    if (type == 'l')
        return make_book_level_randart(book);
    else
        return make_book_theme_randart(book);
}

void wizard_value_artefact()
{
    int i = prompt_invent_item("Value of which artefact?", MT_INVLIST, -1);

    if (!prompt_failed(i))
    {
        const item_def& item(you.inv[i]);
        if (!is_artefact(item))
            mpr("That item is not an artefact!");
        else
            mprf("%s", debug_art_val_str(item).c_str());
    }
}

void wizard_create_all_artefacts()
{
    you.octopus_king_rings = 0;
    int octorings = 8;

    // Create all unrandarts.
    for (int i = 0; i < NO_UNRANDARTS; ++i)
    {
        const int              index = i + UNRAND_START;
        const unrandart_entry* entry = get_unrand_entry(index);

        // Skip dummy entries.
        if (entry->base_type == OBJ_UNASSIGNED)
            continue;

        int islot = get_mitm_slot();
        if (islot == NON_ITEM)
            break;

        item_def& item = mitm[islot];
        make_item_unrandart(item, index);
        item.quantity = 1;
        set_ident_flags(item, ISFLAG_IDENT_MASK);

        msg::streams(MSGCH_DIAGNOSTICS) << "Made " << item.name(DESC_A)
                                        << " (" << debug_art_val_str(item)
                                        << ")" << endl;
        move_item_to_grid(&islot, you.pos());

        // Make all eight.
        if (index == UNRAND_OCTOPUS_KING_RING && --octorings)
            i--;
    }

    // Create Horn of Geryon
    int islot = get_mitm_slot();
    if (islot != NON_ITEM)
    {
        item_def& item = mitm[islot];
        item.clear();
        item.base_type = OBJ_MISCELLANY;
        item.sub_type  = MISC_HORN_OF_GERYON;
        item.quantity  = 1;
        item_colour(item);

        set_ident_flags(item, ISFLAG_IDENT_MASK);
        move_item_to_grid(&islot, you.pos());

        msg::streams(MSGCH_DIAGNOSTICS) << "Made " << item.name(DESC_A)
                                        << endl;
    }
}

void wizard_make_object_randart()
{
    int i = prompt_invent_item("Make an artefact out of which item?",
                                MT_INVLIST, -1);

    if (prompt_failed(i))
        return;

    item_def &item(you.inv[i]);

    if (is_unrandom_artefact(item))
    {
        mpr("That item is already an unrandom artefact.");
        return;
    }

    if (!_item_type_can_be_artefact(item.base_type))
    {
        mpr("That item cannot be turned into an artefact.");
        return;
    }

    const equipment_type eq = item_equip_slot(item);
    int invslot = 0;
    if (eq != EQ_NONE)
    {
        invslot = you.equip[eq];
        unequip_item(eq);
    }

    if (is_random_artefact(item))
    {
        if (!yesno("Is already a randart; wipe and re-use?", true, 'n'))
        {
            canned_msg(MSG_OK);
            return;
        }

        item.special = 0;
        item.flags  &= ~ISFLAG_RANDART;
        item.props.clear();
    }

    mprf(MSGCH_PROMPT, "Fake item as gift from which god (ENTER to leave alone): ");
    char name[80];
    if (!cancellable_get_line(name, sizeof(name)) && name[0])
    {
        god_type god = str_to_god(name, false);
        if (god == GOD_NO_GOD)
            mpr("No such god, leaving item origin alone.");
        else
        {
            mprf("God gift of %s.", god_name(god, false).c_str());
            item.orig_monnum = -god;
        }
    }

    if (item.base_type == OBJ_BOOKS)
    {
        if (!_make_book_randart(item))
        {
            mpr("Failed to turn book into randart.");
            return;
        }
    }
    else if (!make_item_randart(item, true))
    {
        mpr("Failed to turn item into randart.");
        return;
    }

    // Remove curse flag from item, unless worshipping Ashenzari.
    if (you_worship(GOD_ASHENZARI))
        do_curse_item(item, true);
    else
        do_uncurse_item(item, false);

    // If it was equipped, requip the item.
    if (eq != EQ_NONE)
        equip_item(eq, invslot);

    mprf_nocap("%s", item.name(DESC_INVENTORY_EQUIP).c_str());
}

// Returns whether an item of this type can be cursed.
static bool _item_type_can_be_cursed(int type)
{
    return type == OBJ_WEAPONS || type == OBJ_ARMOUR || type == OBJ_JEWELLERY
           || type == OBJ_STAVES || type == OBJ_RODS;
}

void wizard_uncurse_item()
{
    const int i = prompt_invent_item("(Un)curse which item?", MT_INVLIST, -1);

    if (!prompt_failed(i))
    {
        item_def& item(you.inv[i]);

        if (item.cursed())
            do_uncurse_item(item);
        else if (_item_type_can_be_cursed(item.base_type))
            do_curse_item(item);
        else
            mpr("That type of item cannot be cursed.");
    }
}

void wizard_identify_pack()
{
    mpr("You feel a rush of knowledge.");
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        item_def& item = you.inv[i];
        if (item.defined())
        {
            set_ident_type(item, ID_KNOWN_TYPE);
            set_ident_flags(item, ISFLAG_IDENT_MASK);
        }
    }
    you.wield_change  = true;
    you.redraw_quiver = true;
}

void wizard_unidentify_pack()
{
    mpr("You feel a rush of antiknowledge.");
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        item_def& item = you.inv[i];
        if (item.defined())
        {
            set_ident_type(item, ID_UNKNOWN_TYPE);
            unset_ident_flags(item, ISFLAG_IDENT_MASK);
        }
    }
    you.wield_change  = true;
    you.redraw_quiver = true;

    // Forget things that nearby monsters are carrying, as well.
    // (For use with the "give monster an item" wizard targeting
    // command.)
    for (monster_near_iterator mon(&you); mon; ++mon)
    {
        for (int j = 0; j < NUM_MONSTER_SLOTS; ++j)
        {
            if (mon->inv[j] == NON_ITEM)
                continue;

            item_def &item = mitm[mon->inv[j]];

            if (!item.defined())
                continue;

            set_ident_type(item, ID_UNKNOWN_TYPE);
            unset_ident_flags(item, ISFLAG_IDENT_MASK);
        }
    }
}

void wizard_list_items()
{
    bool has_shops = false;

    for (int i = 0; i < MAX_SHOPS; ++i)
        if (env.shop[i].type != SHOP_UNASSIGNED)
        {
            has_shops = true;
            break;
        }

    if (has_shops)
    {
        mpr("Shop items:");

        for (int i = 0; i < MAX_SHOPS; ++i)
            if (env.shop[i].type != SHOP_UNASSIGNED)
            {
                for (stack_iterator si(coord_def(0, i+5)); si; ++si)
                    mpr(si->name(DESC_PLAIN, false, false, false).c_str());
            }

        mpr("");
    }

    mpr("Item stacks (by location and top item):");
    for (int i = 0; i < MAX_ITEMS; ++i)
    {
        item_def &item(mitm[i]);
        if (!item.defined() || item.held_by_monster())
            continue;

        if (item.link != NON_ITEM)
        {
            mprf("(%2d,%2d): %s%s", item.pos.x, item.pos.y,
                 item.name(DESC_PLAIN, false, false, false).c_str(),
                 item.flags & ISFLAG_MIMIC ? " mimic" : "");
        }
    }

    mpr("");
    mpr("Floor items (stacks only show top item):");

    const coord_def start(1,1), end(GXM-1, GYM-1);
    for (rectangle_iterator ri(start, end); ri; ++ri)
    {
        int item = igrd(*ri);
        if (item != NON_ITEM)
        {
            mprf("%3d at (%2d,%2d): %s%s", item, ri->x, ri->y,
                 mitm[item].name(DESC_PLAIN, false, false, false).c_str(),
                 mitm[item].flags & ISFLAG_MIMIC ? " mimic" : "");
        }
    }
}

//---------------------------------------------------------------
//
// debug_item_statistics
//
//---------------------------------------------------------------
static void _debug_acquirement_stats(FILE *ostat)
{
    int p = get_mitm_slot(11);
    if (p == NON_ITEM)
    {
        mpr("Too many items on level.");
        return;
    }
    mitm[p].base_type = OBJ_UNASSIGNED;

    mesclr();
    mpr("[a] Weapons [b] Armours [c] Jewellery      [d] Books");
    mpr("[e] Staves  [f] Wands   [g] Miscellaneous  [h] Food");
    mprf(MSGCH_PROMPT, "What kind of item would you like to get acquirement stats on? ");

    object_class_type type;
    const int keyin = toalower(get_ch());
    switch (keyin)
    {
    case 'a': type = OBJ_WEAPONS;    break;
    case 'b': type = OBJ_ARMOUR;     break;
    case 'c': type = OBJ_JEWELLERY;  break;
    case 'd': type = OBJ_BOOKS;      break;
    case 'e': type = OBJ_STAVES;     break;
    case 'f': type = OBJ_WANDS;      break;
    case 'g': type = OBJ_MISCELLANY; break;
    case 'h': type = OBJ_FOOD;       break;
    default:
        canned_msg(MSG_OK);
        return;
    }

    const int num_itrs = prompt_for_int("How many iterations? ", true);

    if (num_itrs == 0)
    {
        canned_msg(MSG_OK);
        return;
    }

    int last_percent = 0;
    int acq_calls    = 0;
    int total_quant  = 0;
    int max_plus     = -127;
    int total_plus   = 0;
    int num_arts     = 0;

    int subtype_quants[256];
    int ego_quants[NUM_SPECIAL_WEAPONS];

    memset(subtype_quants, 0, sizeof(subtype_quants));
    memset(ego_quants, 0, sizeof(ego_quants));

    for (int i = 0; i < num_itrs; ++i)
    {
        if (kbhit())
        {
            getchk();
            mpr("Stopping early due to keyboard input.");
            break;
        }

        int item_index = NON_ITEM;

        if (!acquirement(type, AQ_WIZMODE, true, &item_index, true)
            || item_index == NON_ITEM
            || !mitm[item_index].defined())
        {
            mpr("Acquirement failed, stopping early.");
            break;
        }

        item_def &item(mitm[item_index]);

        acq_calls++;
        total_quant += item.quantity;
        subtype_quants[item.sub_type] += item.quantity;

        max_plus    = max(max_plus, item.plus + item.plus2);
        total_plus += item.plus + item.plus2;

        if (is_artefact(item))
        {
            num_arts++;
            if (type == OBJ_BOOKS)
            {
                if (item.sub_type == BOOK_RANDART_THEME)
                {
                    const int disc1 = item.plus & 0xFF;
                    ego_quants[disc1]++;
                }
                else if (item.sub_type == BOOK_RANDART_LEVEL)
                {
                    const int level = item.plus;
                    ego_quants[SPTYP_LAST_EXPONENT + level]++;
                }
            }
        }
        else if (type == OBJ_ARMOUR) // Exclude artefacts when counting egos.
            ego_quants[get_armour_ego_type(item)]++;
        else if (type == OBJ_BOOKS && item.sub_type == BOOK_MANUAL)
        {
            // Store skills in subtype array, so as not to overlap
            // with artefact spell disciplines/levels.
            const int skill = item.plus;
            subtype_quants[200 + skill]++;
        }

        // Include artefacts for weapon brands.
        if (type == OBJ_WEAPONS)
            ego_quants[get_weapon_brand(item)]++;

        destroy_item(item_index, true);

        int curr_percent = acq_calls * 100 / num_itrs;
        if (curr_percent > last_percent)
        {
            mesclr();
            mprf("%2d%% done.", curr_percent);
            last_percent = curr_percent;
        }
    }

    if (total_quant == 0 || acq_calls == 0)
    {
        mpr("No items generated.");
        return;
    }

    // Print acquirement base type.
    fprintf(ostat, "Acquiring %s for:\n\n",
            type == OBJ_WEAPONS    ? "weapons" :
            type == OBJ_ARMOUR     ? "armour"  :
            type == OBJ_JEWELLERY  ? "jewellery" :
            type == OBJ_BOOKS      ? "books" :
            type == OBJ_STAVES     ? "staves" :
            type == OBJ_WANDS      ? "wands" :
            type == OBJ_MISCELLANY ? "misc. items" :
            type == OBJ_FOOD       ? "food"
                                   : "buggy items");

    // Print player species/profession.
    string godname = "";
    if (!you_worship(GOD_NO_GOD))
        godname += " of " + god_name(you.religion);

    fprintf(ostat, "%s the %s, Level %d %s %s%s\n\n",
            you.your_name.c_str(), player_title().c_str(),
            you.experience_level,
            species_name(you.species).c_str(),
            you.class_name.c_str(), godname.c_str());

    // Print player equipment.
    const int e_order[] =
    {
        EQ_WEAPON, EQ_BODY_ARMOUR, EQ_SHIELD, EQ_HELMET, EQ_CLOAK,
        EQ_GLOVES, EQ_BOOTS, EQ_AMULET, EQ_RIGHT_RING, EQ_LEFT_RING,
        EQ_RING_ONE, EQ_RING_TWO, EQ_RING_THREE, EQ_RING_FOUR,
        EQ_RING_FIVE, EQ_RING_SIX, EQ_RING_SEVEN, EQ_RING_EIGHT,
        EQ_RING_AMULET
    };

    bool naked = true;
    for (int i = 0; i < NUM_EQUIP; i++)
    {
        int eqslot = e_order[i];

        // Only output filled slots.
        if (you.equip[ e_order[i] ] != -1)
        {
            // The player has something equipped.
            const int item_idx   = you.equip[e_order[i]];
            const item_def& item = you.inv[item_idx];
            const bool melded    = !player_wearing_slot(e_order[i]);

            fprintf(ostat, "%-7s: %s %s\n", equip_slot_to_name(eqslot),
                    item.name(DESC_PLAIN, true).c_str(),
                    melded ? "(melded)" : "");
            naked = false;
        }
    }
    if (naked)
        fprintf(ostat, "Not wearing or wielding anything.\n");

    // Also print the skills, in case they matter.
    string skills = "\nSkills:\n";
    dump_skills(skills);
    fprintf(ostat, "%s\n\n", skills.c_str());

    if (type == OBJ_BOOKS && you.skills[SK_SPELLCASTING])
    {
        // For spellbooks, for each spell discipline, list the number of
        // unseen and total spells available.
        vector<int> total_spells(SPTYP_LAST_EXPONENT + 1);
        vector<int> unseen_spells(SPTYP_LAST_EXPONENT + 1);

        for (int i = 0; i < NUM_SPELLS; ++i)
        {
            const spell_type spell = (spell_type) i;

            if (!is_valid_spell(spell))
                continue;

            if (you_cannot_memorise(spell))
                continue;

            // Only use spells available in books you might find lying about
            // the dungeon.
            if (spell_rarity(spell) == -1)
                continue;

            const bool seen = you.seen_spell[spell];

            const unsigned int disciplines = get_spell_disciplines(spell);
            for (int d = 0; d <= SPTYP_LAST_EXPONENT; ++d)
            {
                const int disc = 1 << d;
                if (disc & SPTYP_DIVINATION)
                    continue;

                if (disciplines & disc)
                {
                    total_spells[d]++;
                    if (!seen)
                        unseen_spells[d]++;
                }
            }
        }
        for (int d = 0; d <= SPTYP_LAST_EXPONENT; ++d)
        {
            const int disc = 1 << d;
            if (disc & SPTYP_DIVINATION)
                continue;

            fprintf(ostat, "%-13s:  %2d/%2d spells unseen\n",
                    spelltype_long_name(disc),
                    unseen_spells[d], total_spells[d]);
        }
    }

    fprintf(ostat, "\nAcquirement called %d times, total quantity = %d\n\n",
            acq_calls, total_quant);

    fprintf(ostat, "%5.2f%% artefacts.\n",
            100.0 * (float) num_arts / (float) acq_calls);

    if (type == OBJ_WEAPONS)
    {
        fprintf(ostat, "Maximum combined pluses: %d\n", max_plus);
        fprintf(ostat, "Average combined pluses: %5.2f\n\n",
                (float) total_plus / (float) acq_calls);

        fprintf(ostat, "Egos (including artefacts):\n");

        const char* names[] =
        {
            "normal",
            "flaming",
            "freezing",
            "holy wrath",
            "electrocution",
#if TAG_MAJOR_VERSION == 34
            "orc slaying",
#endif
            "dragon slaying",
            "venom",
            "protection",
            "draining",
            "speed",
            "vorpal",
            "flame",
            "frost",
            "vampiricism",
            "pain",
            "antimagic",
            "distortion",
#if TAG_MAJOR_VERSION == 34
            "reaching",
            "returning",
#endif
            "chaos",
            "evasion",
#if TAG_MAJOR_VERSION == 34
            "confusion",
#endif
            "penetration",
            "reaping",
            "INVALID",
            "acid",
#if TAG_MAJOR_VERSION != 34
            "confuse",
#endif
            "debug randart",
        };
        COMPILE_CHECK(ARRAYSZ(names) == NUM_SPECIAL_WEAPONS);

        for (int i = 0; i < NUM_SPECIAL_WEAPONS; ++i)
            if (ego_quants[i] > 0)
            {
                fprintf(ostat, "%14s: %5.2f\n", names[i],
                        100.0 * (float) ego_quants[i] / (float) acq_calls);
            }

        fprintf(ostat, "\n\n");
    }
    else if (type == OBJ_ARMOUR)
    {
        fprintf(ostat, "Maximum plus: %d\n", max_plus);
        fprintf(ostat, "Average plus: %5.2f\n\n",
                (float) total_plus / (float) acq_calls);

        fprintf(ostat, "Egos (excluding artefacts):\n");

        const char* names[] =
        {
            "normal",
            "running",
            "fire resistance",
            "cold resistance",
            "poison resistance",
            "see invis",
            "darkness",
            "strength",
            "dexterity",
            "intelligence",
            "ponderous",
            "flight",
            "magic reistance",
            "protection",
            "stealth",
            "resistance",
            "positive energy",
            "archmagi",
            "preservation",
            "reflection",
            "spirit shield",
            "archery",
            "jumping",
        };

        const int non_art = acq_calls - num_arts;
        for (int i = 0; i < NUM_SPECIAL_ARMOURS; ++i)
        {
            if (ego_quants[i] > 0)
                fprintf(ostat, "%17s: %5.2f\n", names[i],
                        100.0 * (float) ego_quants[i] / (float) non_art);
        }
        fprintf(ostat, "\n\n");
    }
    else if (type == OBJ_BOOKS)
    {
        // Print disciplines of artefact spellbooks.
        if (subtype_quants[BOOK_RANDART_THEME]
            + subtype_quants[BOOK_RANDART_LEVEL] > 0)
        {
            fprintf(ostat, "Primary disciplines/levels of randart books:\n");

            const char* names[] =
            {
                "none",
                "conjuration",
                "enchantment",
                "fire magic",
                "ice magic",
                "transmutation",
                "necromancy",
                "summoning",
                "divination",
                "translocation",
                "poison magic",
                "earth magic",
                "air magic",
            };
            COMPILE_CHECK(ARRAYSZ(names) == SPTYP_LAST_EXPONENT + 1);

            for (int i = 0; i <= SPTYP_LAST_EXPONENT; ++i)
            {
                if (ego_quants[i] > 0)
                {
                    fprintf(ostat, "%17s: %5.2f\n", names[i],
                            100.0 * (float) ego_quants[i] / (float) num_arts);
                }
            }
            // List levels for fixed level randarts.
            for (int i = 1; i < 9; ++i)
            {
                const int k = SPTYP_LAST_EXPONENT + i;
                if (ego_quants[k] > 0)
                {
                    fprintf(ostat, "%15s %d: %5.2f\n", "level", i,
                            100.0 * (float) ego_quants[i] / (float) num_arts);
                }
            }
        }

        // Also list skills for manuals.
        if (subtype_quants[BOOK_MANUAL] > 0)
        {
            const int mannum = subtype_quants[BOOK_MANUAL];
            fprintf(ostat, "\nManuals:\n");
            for (int i = SK_FIRST_SKILL; i <= SK_LAST_SKILL; ++i)
            {
                const int k = 200 + i;
                if (subtype_quants[k] > 0)
                {
                    fprintf(ostat, "%17s: %5.2f\n", skill_name((skill_type)i),
                            100.0 * (float) subtype_quants[k] / (float) mannum);
                }
            }
        }
        fprintf(ostat, "\n\n");
    }

    item_def item;
    item.quantity  = 1;
    item.base_type = type;

    const description_level_type desc = (type == OBJ_BOOKS ? DESC_PLAIN
                                                           : DESC_DBNAME);
    const bool terse = (type == OBJ_BOOKS ? false : true);

    // First, get the maximum name length.
    int max_width = 0;
    for (int i = 0; i < 256; ++i)
    {
        if (type == OBJ_BOOKS && i >= 200)
            break;

        if (subtype_quants[i] == 0)
            continue;

        item.sub_type = i;
        string name = item.name(desc, terse, true);

        max_width = max(max_width, strwidth(name));
    }

    // Now output the sub types.
    char format_str[80];
    sprintf(format_str, "%%%ds: %%6.2f\n", max_width);

    for (int i = 0; i < 256; ++i)
    {
        if (type == OBJ_BOOKS && i >= 200)
            break;

        if (subtype_quants[i] == 0)
            continue;

        item.sub_type = i;
        string name = item.name(desc, terse, true);

        fprintf(ostat, format_str, name.c_str(),
                (float) subtype_quants[i] * 100.0 / (float) total_quant);
    }
    fprintf(ostat, "-----------------------------------------\n\n");

    mpr("Results written into 'items.stat'.");
}

#define MAX_TRIES 16777216 /* not special anymore */
static void _debug_rap_stats(FILE *ostat)
{
    int i = prompt_invent_item("Generate randart stats on which item?",
                                MT_INVLIST, -1);

    if (prompt_failed(i))
        return;

    // A copy of the item, rather than a reference to the inventory item,
    // so we can fiddle with the item at will.
    item_def item(you.inv[i]);

    // Start off with a non-artefact item.
    item.flags  &= ~ISFLAG_ARTEFACT_MASK;
    item.special = 0;
    item.props.clear();

    if (!make_item_randart(item))
    {
        mpr("Can't make a randart out of that type of item.");
        return;
    }

    // -1 = always bad, 1 = always good, 0 = depends on value
    const int good_or_bad[] =
    {
         1, //ARTP_BRAND
         0, //ARTP_AC
         0, //ARTP_EVASION
         0, //ARTP_STRENGTH
         0, //ARTP_INTELLIGENCE
         0, //ARTP_DEXTERITY
         0, //ARTP_FIRE
         0, //ARTP_COLD
         1, //ARTP_ELECTRICITY
         1, //ARTP_POISON
         1, //ARTP_NEGATIVE_ENERGY
         0, //ARTP_MAGIC
         1, //ARTP_EYESIGHT
         1, //ARTP_INVISIBLE
         1, //ARTP_FLY
#if TAG_MAJOR_VERSION != 34
         1, //ARTP_FOG,
#endif
         1, //ARTP_BLINK
         1, //ARTP_BERSERK
        -1, //ARTP_NOISES
        -1, //ARTP_PREVENT_SPELLCASTING
        -1, //ARTP_CAUSE_TELEPORTATION
        -1, //ARTP_PREVENT_TELEPORTATION
        -1, //ARTP_ANGRY
         0, //ARTP_METABOLISM
        -1, //ARTP_MUTAGENIC
         0, //ARTP_ACCURACY
         0, //ARTP_DAMAGE
        -1, //ARTP_CURSED
         0, //ARTP_STEALTH
         0, //ARTP_MAGICAL_POWER
         0, //ARTP_BASE_DELAY
         0, //ARTP_HP
         1, //ARTP_CLARITY
         0, //ARTP_BASE_ACC
         0, //ARTP_BASE_DAM
         1, //ARTP_RMSL
#if TAG_MAJOR_VERSION == 34
         1, //ARTP_FOG
#endif
         1, //ARTP_REGENERATION
         0, //ARTP_NO_UPGRADE
    };
    COMPILE_CHECK(ARRAYSZ(good_or_bad) == ARTP_NUM_PROPERTIES);

    // No bounds checking to speed things up a bit.
    int all_props[ARTP_NUM_PROPERTIES];
    int good_props[ARTP_NUM_PROPERTIES];
    int bad_props[ARTP_NUM_PROPERTIES];
    for (i = 0; i < ARTP_NUM_PROPERTIES; ++i)
    {
        all_props[i] = 0;
        good_props[i] = 0;
        bad_props[i] = 0;
    }

    int max_props         = 0, total_props         = 0;
    int max_good_props    = 0, total_good_props    = 0;
    int max_bad_props     = 0, total_bad_props     = 0;
    int max_balance_props = 0, total_balance_props = 0;

    int num_randarts = 0, bad_randarts = 0;

    artefact_properties_t proprt;

    for (i = 0; i < MAX_TRIES; ++i)
    {
        if (kbhit())
        {
            getchk();
            mpr("Stopping early due to keyboard input.");
            break;
        }

        // Generate proprt once and hand it off to randart_is_bad(),
        // so that randart_is_bad() doesn't generate it a second time.
        artefact_wpn_properties(item, proprt);
        if (randart_is_bad(item, proprt))
        {
            bad_randarts++;
            continue;
        }

        num_randarts++;
        proprt[ARTP_CURSED] = 0;

        int num_props = 0, num_good_props = 0, num_bad_props = 0;
        for (int j = 0; j < ARTP_NUM_PROPERTIES; ++j)
        {
            const int val = proprt[j];
            if (val)
            {
                num_props++;
                all_props[j]++;
                switch (good_or_bad[j])
                {
                case -1:
                    num_bad_props++;
                    break;
                case 1:
                    num_good_props++;
                    break;
                case 0:
                    if (val > 0)
                    {
                        good_props[j]++;
                        num_good_props++;
                    }
                    else
                    {
                        bad_props[j]++;
                        num_bad_props++;
                    }
                }
            }
        }

        int balance = num_good_props - num_bad_props;

        max_props         = max(max_props, num_props);
        max_good_props    = max(max_good_props, num_good_props);
        max_bad_props     = max(max_bad_props, num_bad_props);
        max_balance_props = max(max_balance_props, balance);

        total_props         += num_props;
        total_good_props    += num_good_props;
        total_bad_props     += num_bad_props;
        total_balance_props += balance;

        if (i % 16767 == 0)
        {
            mesclr();
            float curr_percent = (float) i * 1000.0
                / (float) MAX_TRIES;
            mprf("%4.1f%% done.", curr_percent / 10.0);
        }

    }

    fprintf(ostat, "Randarts generated: %d valid, %d invalid\n\n",
            num_randarts, bad_randarts);

    fprintf(ostat, "max # of props = %d, avg # = %5.2f\n",
            max_props, (float) total_props / (float) num_randarts);
    fprintf(ostat, "max # of good props = %d, avg # = %5.2f\n",
            max_good_props, (float) total_good_props / (float) num_randarts);
    fprintf(ostat, "max # of bad props = %d, avg # = %5.2f\n",
            max_bad_props, (float) total_bad_props / (float) num_randarts);
    fprintf(ostat, "max (good - bad) props = %d, avg # = %5.2f\n\n",
            max_balance_props,
            (float) total_balance_props / (float) num_randarts);

    const char* rap_names[] =
    {
        "ARTP_BRAND",
        "ARTP_AC",
        "ARTP_EVASION",
        "ARTP_STRENGTH",
        "ARTP_INTELLIGENCE",
        "ARTP_DEXTERITY",
        "ARTP_FIRE",
        "ARTP_COLD",
        "ARTP_ELECTRICITY",
        "ARTP_POISON",
        "ARTP_NEGATIVE_ENERGY",
        "ARTP_MAGIC",
        "ARTP_EYESIGHT",
        "ARTP_INVISIBLE",
        "ARTP_FLY",
#if TAG_MAJOR_VERSION != 34
        "ARTP_FOG",
#endif
        "ARTP_BLINK",
        "ARTP_BERSERK",
        "ARTP_NOISES",
        "ARTP_PREVENT_SPELLCASTING",
        "ARTP_CAUSE_TELEPORTATION",
        "ARTP_PREVENT_TELEPORTATION",
        "ARTP_ANGRY",
        "ARTP_METABOLISM",
        "ARTP_MUTAGENIC",
        "ARTP_ACCURACY",
        "ARTP_DAMAGE",
        "ARTP_CURSED",
        "ARTP_STEALTH",
        "ARTP_MAGICAL_POWER",
        "ARTP_BASE_DELAY",
        "ARTP_HP",
        "ARTP_CLARITY",
        "ARTP_BASE_ACC",
        "ARTP_BASE_DAM",
        "ARTP_RMSL",
#if TAG_MAJOR_VERSION == 34
        "ARTP_FOG",
#endif
        "ARTP_REGENERATION",
        "ARTP_NO_UPGRADE",
    };
    COMPILE_CHECK(ARRAYSZ(rap_names) == ARTP_NUM_PROPERTIES);

    fprintf(ostat, "                            All    Good   Bad\n");
    fprintf(ostat, "                           --------------------\n");

    for (i = 0; i < ARTP_NUM_PROPERTIES; ++i)
    {
        if (all_props[i] == 0)
            continue;

        fprintf(ostat, "%-25s: %5.2f%% %5.2f%% %5.2f%%\n", rap_names[i],
                (float) all_props[i] * 100.0 / (float) num_randarts,
                (float) good_props[i] * 100.0 / (float) num_randarts,
                (float) bad_props[i] * 100.0 / (float) num_randarts);
    }

    fprintf(ostat, "\n-----------------------------------------\n\n");
    mpr("Results written into 'items.stat'.");
}

void debug_item_statistics(void)
{
    FILE *ostat = fopen("items.stat", "a");

    if (!ostat)
    {
        mprf(MSGCH_ERROR, "Can't write items.stat: %s", strerror(errno));
        return;
    }

    mpr("Generate stats for: [a] acquirement [b] randart properties");
    flush_prev_message();

    const int keyin = toalower(get_ch());
    switch (keyin)
    {
    case 'a': _debug_acquirement_stats(ostat); break;
    case 'b': _debug_rap_stats(ostat);
    default:
        canned_msg(MSG_OK);
        break;
    }

    fclose(ostat);
}

void wizard_draw_card()
{
    msg::streams(MSGCH_PROMPT) << "Which card? " << endl;
    char buf[80];
    if (cancellable_get_line_autohist(buf, sizeof buf))
    {
        mpr("Unknown card.");
        return;
    }

    string wanted = buf;
    lowercase(wanted);

    bool found_card = false;
    for (int i = 0; i < NUM_CARDS; ++i)
    {
        const card_type c = static_cast<card_type>(i);
        string card = card_name(c);
        lowercase(card);
        if (card.find(wanted) != string::npos)
        {
            card_effect(c, DECK_RARITY_LEGENDARY);
            found_card = true;
            break;
        }
    }
    if (!found_card)
        mpr("Unknown card.");
}
#endif
