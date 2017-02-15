/**
 * @file
 * @brief Item related wizard functions.
**/

#include "AppHdr.h"

#include "wiz-item.h"

#include <cerrno>

#include "acquire.h"
#include "act-iter.h"
#include "artefact.h"
#include "art-enum.h"
#include "cio.h"
#include "coordit.h"
#include "dbg-util.h"
#include "decks.h"
#include "env.h"
#include "god-passive.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "jobs.h"
#include "libutil.h"
#include "macro.h"
#include "makeitem.h"
#include "mapdef.h"
#include "message.h"
#include "misc.h"
#include "mon-death.h"
#include "options.h"
#include "output.h"
#include "player-equip.h"
#include "prompt.h"
#include "randbook.h"
#include "religion.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-util.h"
#include "stash.h"
#include "stringutil.h"
#include "terrain.h"
#include "unicode.h"
#include "view.h"

#ifdef WIZARD
static void _make_all_books()
{
    for (int i = 0; i < NUM_FIXED_BOOKS; ++i)
    {
        if (item_type_removed(OBJ_BOOKS, i))
            continue;
        int thing = items(false, OBJ_BOOKS, i, 0, 0, AQ_WIZMODE);
        if (thing == NON_ITEM)
            continue;

        move_item_to_grid(&thing, you.pos());

        if (thing == NON_ITEM)
            continue;

        item_def book(mitm[thing]);

        mark_had_book(book);
        set_ident_flags(book, ISFLAG_KNOW_TYPE);
        set_ident_flags(book, ISFLAG_IDENT_MASK);

        mpr(book.name(DESC_PLAIN));
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

void wizard_create_spec_object()
{
    char specs[80];
    char32_t keyin;
    object_class_type class_wanted;

    do
    {
        mprf(MSGCH_PROMPT, ") - weapons     ( - missiles  [ - armour  / - wands    ?  - scrolls");
        mprf(MSGCH_PROMPT, "= - jewellery   ! - potions   : - books   | - staves   }  - miscellany");
        mprf(MSGCH_PROMPT, "X - corpses     %% - food      $ - gold    0  - the Orb");
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

    // XXX: hope item_class_by_sym never returns a real class for non-ascii.
    msgwin_reply(string(1, (char) keyin));

    // Allocate an item to play with.
    int thing_created = get_mitm_slot();
    if (thing_created == NON_ITEM)
    {
        mpr("Could not allocate item.");
        return;
    }
    item_def& item(mitm[thing_created]);

    // turn item into appropriate kind:
    if (class_wanted == OBJ_ORBS)
    {
        item.base_type = OBJ_ORBS;
        item.sub_type  = ORB_ZOT;
        item.quantity  = 1;
    }
    else if (class_wanted == OBJ_GOLD)
    {
        int amount = prompt_for_int("How much gold? ", true);
        if (amount <= 0)
        {
            canned_msg(MSG_OK);
            return;
        }

        item.base_type = OBJ_GOLD;
        item.sub_type  = 0;
        item.quantity  = amount;
    }
    // in this case, place_monster_corpse will allocate an item for us, and we
    // don't use item/thing_created.
    else if (class_wanted == OBJ_CORPSES)
    {
        monster_type mon = debug_prompt_for_monster();

        if (mon == MONS_NO_MONSTER || mon == MONS_PROGRAM_BUG)
        {
            mpr("No such monster.");
            return;
        }

        if (!mons_class_can_leave_corpse(mon))
        {
            mpr("That monster doesn't leave corpses.");
            return;
        }

        if (mons_is_draconian_job(mon))
        {
            mpr("You can't make a draconian corpse by its background.");
            mon = MONS_DRACONIAN;
        }

        monster dummy;
        dummy.type = mon;
        dummy.position = you.pos();

        if (mons_genus(mon) == MONS_HYDRA)
        {
            dummy.num_heads
                =  max(1, min(27, prompt_for_int("How many heads? ", false)));
        }

        if (!place_monster_corpse(dummy, false, true))
        {
            mpr("Failed to create corpse.");
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

        if (!get_item_by_name(&item, specs, class_wanted, true))
        {
            mpr("No such item.");

            // Clean up item
            destroy_item(thing_created);
            return;
        }
    }

    item_colour(item);

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

static void _tweak_randart(item_def &item)
{
    if (item_is_equipped(item))
    {
        mprf(MSGCH_PROMPT, "You can't tweak the randart properties of an equipped item.");
        return;
    }
    else
        clear_messages();

    artefact_properties_t props;
    artefact_properties(item, props);

    string prompt = "";

    vector<unsigned int> choice_to_prop;
    for (unsigned int i = 0, choice_num = 0; i < ARTP_NUM_PROPERTIES; ++i)
    {
#if TAG_MAJOR_VERSION == 34
        if (i == ARTP_METABOLISM || i == ARTP_ACCURACY || i == ARTP_TWISTER)
            continue;
#endif
        choice_to_prop.push_back(i);
        if (choice_num % 8 == 0 && choice_num != 0)
            prompt.back() = '\n'; // Replace the space

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

        snprintf(buf, sizeof(buf), "%s) %s%-6s%s ",
                choice == '<' ? "<<" : string(1, choice).c_str(),
                 props[i] ? "<w>" : "",
                 artp_name((artefact_prop_type)choice_to_prop[choice_num]),
                 props[i] ? "</w>" : "");

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

    const artefact_prop_type prop = (artefact_prop_type)choice_to_prop[choice];
    switch (artp_potential_value_types(prop))
    {
    case ARTP_VAL_BOOL:
        mprf(MSGCH_PROMPT, "Toggling %s to %s.", artp_name(prop),
             props[prop] ? "off" : "on");
        artefact_set_property(item, static_cast<artefact_prop_type>(prop),
                             !props[prop]);
        break;

    case ARTP_VAL_POS:
    {
        mprf(MSGCH_PROMPT, "%s was %d.", artp_name(prop), props[prop]);
        const int val = prompt_for_int("New value? ", true);

        if (val < 0)
        {
            mprf(MSGCH_PROMPT, "Value for %s must be non-negative",
                 artp_name(prop));
            return;
        }
        artefact_set_property(item, static_cast<artefact_prop_type>(prop),
                             val);
        break;
    }
    case ARTP_VAL_ANY:
    {
        mprf(MSGCH_PROMPT, "%s was %d.", artp_name(prop), props[prop]);
        const int val = prompt_for_int("New value? ", false);
        artefact_set_property(item, static_cast<artefact_prop_type>(prop),
                             val);
        break;
    }

    case ARTP_VAL_BRAND:
    {
        mprf(MSGCH_PROMPT, "%s was %s.", artp_name(prop),
             props[prop] ? ego_type_string(item, false).c_str() : "normal");

        char specs[80];
        msgwin_get_line("New ego? ", specs, sizeof(specs));
        if (specs[0] == '\0')
        {
            canned_msg(MSG_OK);
            break;
        }

        const int ego = str_to_ego(item.base_type, specs);

        if (ego == 0 && string(specs) != "normal") // this is secretly a hack
        {
            mprf("No such ego as: %s", specs);
            break;
        }
        if (ego == -1)
        {
            mprf("Ego '%s' is invalid for %s.",
                 specs, item.name(DESC_A).c_str());
            break;
        }

        // XXX: validate ego further? (is_weapon_brand_ok etc)

        artefact_set_property(item, static_cast<artefact_prop_type>(prop),
                              ego);
        break;
    }
    }
}

void wizard_tweak_object()
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

            mprf_nocap(MSGCH_PROMPT, "a - plus  b - plus2  c - %s  "
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
        {
            canned_msg(MSG_OK);
            return;
        }

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
        {
            canned_msg(MSG_OK);
            return;
        }

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
    build_themed_book(book);
    return true;
}

/// Prompt for an item in inventory & print its base shop value.
void wizard_value_item()
{
    const int i = prompt_invent_item("Value of which item?", MT_INVLIST, -1);

    if (prompt_failed(i))
        return;

    const item_def& item(you.inv[i]);
    const int real_value = item_value(item, true);
    const int known_value = item_value(item, false);
    if (real_value != known_value)
        mprf("Real value: %d (known: %d)", real_value, known_value);
    else
        mprf("Value: %d", real_value);
}

void wizard_create_all_artefacts()
{
    you.octopus_king_rings = 0;
    int octorings = 8;

    // Create all unrandarts.
    for (int i = 0; i < NUM_UNRANDARTS; ++i)
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

        item.unrand_idx = 0;
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
    if (have_passive(passive_t::want_curses))
        do_curse_item(item, true);
    else
        do_uncurse_item(item);

    // If it was equipped, requip the item.
    if (eq != EQ_NONE)
        equip_item(eq, invslot);

    mprf_nocap("%s", item.name(DESC_INVENTORY_EQUIP).c_str());
}

// Returns whether an item of this type can be cursed.
static bool _item_type_can_be_cursed(int type)
{
    return type == OBJ_WEAPONS || type == OBJ_ARMOUR || type == OBJ_JEWELLERY
           || type == OBJ_STAVES;
}

void wizard_uncurse_item()
{
    const int i = prompt_invent_item("(Un)curse which item?", MT_INVLIST, -1);

    if (!prompt_failed(i))
    {
        item_def& item(you.inv[i]);

        if (item.cursed())
            do_uncurse_item(item);
        else
        {
            if (!_item_type_can_be_cursed(item.base_type))
            {
                mpr("That type of item cannot be cursed.");
                return;
            }
            do_curse_item(item);
        }
        mprf_nocap("%s", item.name(DESC_INVENTORY_EQUIP).c_str());
    }
}

void wizard_identify_pack()
{
    mpr("You feel a rush of knowledge.");
    identify_inventory();
    you.wield_change  = true;
    you.redraw_quiver = true;
}

static void _forget_item(item_def &item)
{
    set_ident_type(item, false);
    unset_ident_flags(item, ISFLAG_IDENT_MASK);
    item.flags &= ~(ISFLAG_SEEN | ISFLAG_HANDLED | ISFLAG_THROWN
                    | ISFLAG_DROPPED | ISFLAG_NOTED_ID | ISFLAG_NOTED_GET);
}

void wizard_unidentify_pack()
{
    mpr("You feel a rush of antiknowledge.");
    for (auto &item : you.inv)
        if (item.defined())
            _forget_item(item);

    you.wield_change  = true;
    you.redraw_quiver = true;

    // Forget things that nearby monsters are carrying, as well.
    // (For use with the "give monster an item" wizard targeting
    // command.)
    for (monster_near_iterator mon(&you); mon; ++mon)
        for (mon_inv_iterator ii(**mon); ii; ++ii)
            _forget_item(*ii);
}

void wizard_list_items()
{
    mpr("Item stacks (by location and top item):");
    for (const auto &item : mitm)
    {
        if (item.defined() && !item.held_by_monster() && item.link != NON_ITEM)
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

static int _subtype_index(int acq_type, const item_def &item)
{
    // Certain acquirement types can acquire different classes of items than
    // they claim to, so... pack them in at the end, as a hack.
    switch (acq_type)
    {
        case OBJ_MISCELLANY:
            if (item.base_type == OBJ_WANDS)
                return NUM_MISCELLANY + item.sub_type;
            break;
        case OBJ_STAVES:
            if (item.base_type == OBJ_WEAPONS) // unrand staff
                return NUM_STAVES;
            break;
        default:
            break;
    }

    return item.sub_type;
}

/// Reverse the _subtype_index() hack.
static void _fill_item_from_subtype(object_class_type acq_type, int subtype,
                                    item_def &item)
{
    switch (acq_type)
    {
        case OBJ_MISCELLANY:
            if (subtype >= NUM_MISCELLANY)
            {
                item.base_type = OBJ_WANDS;
                item.sub_type = subtype - NUM_MISCELLANY;
                return;
            }
            break;
        case OBJ_STAVES:
            if (subtype == NUM_STAVES) // unrand staff
            {
                item.base_type = OBJ_WEAPONS;
                item.sub_type = WPN_STAFF;
                return;
            }
            break;
        default:
            break;
    }

    item.base_type = acq_type;
    item.sub_type = subtype;
}

static void _debug_acquirement_stats(FILE *ostat)
{
    int p = get_mitm_slot(11);
    if (p == NON_ITEM)
    {
        mpr("Too many items on level.");
        return;
    }
    mitm[p].base_type = OBJ_UNASSIGNED;

    clear_messages();
    mpr("[a] Weapons [b] Armours   [c] Jewellery [d] Books");
    mpr("[e] Staves  [f] Evocables [g] Food");
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
    case 'f': type = OBJ_MISCELLANY; break;
    case 'g': type = OBJ_FOOD;       break;
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
    short max_plus   = -127;
    int total_plus   = 0;
    int num_arts     = 0;
    int randbook_spells = 0;

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
        // hack alert: put unrands into the end of staff acq
        // and wands into the end of misc acq
        const int subtype_index = _subtype_index(type, item);
        subtype_quants[subtype_index] += item.quantity;

        max_plus    = max(max_plus, item.plus);
        total_plus += item.plus;

        if (is_artefact(item))
        {
            num_arts++;
            if (type == OBJ_BOOKS)
            {
                randbook_spells += spells_in_book(item).size();
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
            clear_messages();
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

    fprintf(ostat, "%s %s, Level %d %s %s%s\n\n",
            you.your_name.c_str(), player_title().c_str(),
            you.experience_level,
            species_name(you.species).c_str(),
            get_job_name(you.char_class), godname.c_str());

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
    for (int i = EQ_FIRST_EQUIP; i < NUM_EQUIP; i++)
    {
        int eqslot = e_order[i];

        // Only output filled slots.
        if (you.equip[ e_order[i] ] != -1)
        {
            // The player has something equipped.
            const int item_idx   = you.equip[e_order[i]];
            const item_def& item = you.inv[item_idx];

            fprintf(ostat, "%-7s: %s %s\n", equip_slot_to_name(eqslot),
                    item.name(DESC_PLAIN, true).c_str(),
                    you.melded[i] ? "(melded)" : "");
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

            if (!you_can_memorise(spell))
                continue;

            // Only use spells available in books you might find lying about
            // the dungeon.
            if (spell_rarity(spell) == -1)
                continue;

            const bool seen = you.seen_spell[spell];

            const spschools_type disciplines = get_spell_disciplines(spell);
            for (int d = 0; d <= SPTYP_LAST_EXPONENT; ++d)
            {
                const auto disc = spschools_type::exponent(d);
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
            const auto disc = spschools_type::exponent(d);
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
            "dragon slaying",
#endif
            "venom",
            "protection",
            "draining",
            "speed",
            "vorpal",
#if TAG_MAJOR_VERSION == 34
            "flame",
            "frost",
#endif
            "vampirism",
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
#if TAG_MAJOR_VERSION > 34
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
            "invisibility",
            "strength",
            "dexterity",
            "intelligence",
            "ponderous",
            "flight",
            "magic resistance",
            "protection",
            "stealth",
            "resistance",
            "positive energy",
            "archmagi",
#if TAG_MAJOR_VERSION == 34
            "preservation",
#endif
            "reflection",
            "spirit shield",
            "archery",
#if TAG_MAJOR_VERSION == 34
            "jumping",
#endif
            "repulsion",
        };

        const int non_art = acq_calls - num_arts;
        for (int i = 0; i < NUM_SPECIAL_ARMOURS; ++i)
        {
            if (ego_quants[i] > 0)
            {
                fprintf(ostat, "%17s: %5.2f\n", names[i],
                        100.0 * (float) ego_quants[i] / (float) non_art);
            }
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

            fprintf(ostat, "Avg. spells per randbook: %4.3f",
                    (float)randbook_spells / num_arts);
        }

        // Also list skills for manuals.
        if (subtype_quants[BOOK_MANUAL] > 0)
        {
            const int mannum = subtype_quants[BOOK_MANUAL];
            fprintf(ostat, "\nManuals:\n");
            for (skill_type sk = SK_FIRST_SKILL; sk <= SK_LAST_SKILL; ++sk)
            {
                const int k = 200 + sk;
                if (subtype_quants[k] > 0)
                {
                    fprintf(ostat, "%17s: %5.2f\n", skill_name(sk),
                            100.0 * (float) subtype_quants[k] / (float) mannum);
                }
            }
        }
        fprintf(ostat, "\n\n");
    }

    item_def item;
    item.quantity  = 1;
    item.base_type = type;

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
        string name = item.name(DESC_DBNAME, terse, true);

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

        _fill_item_from_subtype(type, i, item);

        const string name = item.name(DESC_DBNAME, terse, true);

        fprintf(ostat, format_str, name.c_str(),
                (float) subtype_quants[i] * 100.0 / (float) total_quant);
    }
    fprintf(ostat, "-----------------------------------------\n\n");

    mpr("Results written into 'items.stat'.");
}

/**
 * Take the median of the provided dataset. Mutates it for efficiency.
 */
static int _median(vector<int> &counts)
{
    nth_element(counts.begin(), counts.begin() + counts.size()/2, counts.end());
    return counts[counts.size()/2];
}

#define MAX_TRIES 27272
static void _debug_rap_stats(FILE *ostat)
{
    const int inv_index
        = prompt_invent_item("Generate randart stats on which item?",
                             MT_INVLIST, -1);

    if (prompt_failed(inv_index))
        return;

    // A copy of the item, rather than a reference to the inventory item,
    // so we can fiddle with the item at will.
    item_def item(you.inv[inv_index]);

    // Start off with a non-artefact item.
    item.flags  &= ~ISFLAG_ARTEFACT_MASK;
    item.unrand_idx = 0;
    item.props.clear();

    if (!make_item_randart(item))
    {
        mpr("Can't make a randart out of that type of item.");
        return;
    }

    FixedVector<int, ARTP_NUM_PROPERTIES> good_props(0);
    FixedVector<int, ARTP_NUM_PROPERTIES> bad_props(0);

    FixedVector<int, ARTP_NUM_PROPERTIES> max_prop_vals(0);
    FixedVector<int, ARTP_NUM_PROPERTIES> min_prop_vals(0);
    FixedVector<int, ARTP_NUM_PROPERTIES> total_good_prop_vals(0);
    FixedVector<int, ARTP_NUM_PROPERTIES> total_bad_prop_vals(0);

    vector<int> good_prop_counts;
    vector<int> bad_prop_counts;
    vector<int> total_prop_counts;

    int max_props         = 0;
    int max_good_props    = 0;
    int max_bad_props     = 0;
    int max_balance_props = 0, total_balance_props = 0;

    int num_randarts = 0, bad_randarts = 0;

    artefact_properties_t proprt;

    for (int i = 0; i < MAX_TRIES; ++i)
    {
        if (kbhit())
        {
            getchk();
            mpr("Stopping early due to keyboard input.");
            break;
        }

        // Generate proprt once and hand it off to randart_is_bad(),
        // so that randart_is_bad() doesn't generate it a second time.
        item.flags  &= ~ISFLAG_ARTEFACT_MASK;
        item.unrand_idx = 0;
        item.props.clear();
        make_item_randart(item);
        artefact_properties(item, proprt);

        if (randart_is_bad(item, proprt))
        {
            bad_randarts++;
            continue;
        }

        num_randarts++;

        int num_good_props = 0, num_bad_props = 0;
        for (int j = 0; j < ARTP_NUM_PROPERTIES; ++j)
        {
            const artefact_prop_type prop = (artefact_prop_type)j;
            const int val = proprt[prop];
            if (!val)
                continue;

            // assumption: all mixed good/bad props are good iff positive
            const bool good = !artp_potentially_bad(prop)
                              || (artp_potentially_good(prop) && val > 0);
            if (good)
            {
                good_props[prop]++;
                num_good_props++;
                total_good_prop_vals[prop] += val;
                max_prop_vals[prop] = max(max_prop_vals[prop], val);
            }
            else
            {
                bad_props[prop]++;
                num_bad_props++;
                total_bad_prop_vals[prop] += val;
                min_prop_vals[prop] = min(min_prop_vals[prop], val);
            }
        }

        const int num_props = num_good_props + num_bad_props;
        const int balance   = num_good_props - num_bad_props;

        good_prop_counts.push_back(num_good_props);
        bad_prop_counts.push_back(num_bad_props);
        total_prop_counts.push_back(num_props);

        max_props         = max(max_props, num_props);
        max_good_props    = max(max_good_props, num_good_props);
        max_bad_props     = max(max_bad_props, num_bad_props);
        max_balance_props = max(max_balance_props, balance);

        total_balance_props += balance;

        if (i % (MAX_TRIES / 100) == 0)
        {
            clear_messages();
            float curr_percent = (float) i * 1000.0
                / (float) MAX_TRIES;
            mprf("%4.1f%% done.", curr_percent / 10.0);
            viewwindow();
        }

    }

    fprintf(ostat, "Randarts generated: %d valid, %d invalid\n\n",
            num_randarts, bad_randarts);

    int total_good_props = 0, total_bad_props = 0;
    for (int i = 0; i < ARTP_NUM_PROPERTIES; ++i)
    {
        total_good_props += good_props[i];
        total_bad_props += bad_props[i];
    }

    // assumption: all props are good or bad
    const int total_props = total_good_props + total_bad_props;

    fprintf(ostat, "max # of props = %d, mean = %5.2f, median = %d\n",
            max_props, (float) total_props / (float) num_randarts,
            _median(total_prop_counts));
    fprintf(ostat, "max # of good props = %d, mean = %5.2f, median = %d\n",
            max_good_props, (float) total_good_props / (float) num_randarts,
            _median(good_prop_counts));
    fprintf(ostat, "max # of bad props = %d, mean = %5.2f, median = %d\n",
            max_bad_props, (float) total_bad_props / (float) num_randarts,
            _median(bad_prop_counts));
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
        "ARTP_MAGIC_RESISTANCE",
        "ARTP_SEE_INVISIBLE",
        "ARTP_INVISIBLE",
        "ARTP_FLY",
#if TAG_MAJOR_VERSION > 34
        "ARTP_FOG",
#endif
        "ARTP_BLINK",
        "ARTP_BERSERK",
        "ARTP_NOISE",
        "ARTP_PREVENT_SPELLCASTING",
        "ARTP_CAUSE_TELEPORTATION",
        "ARTP_PREVENT_TELEPORTATION",
        "ARTP_ANGRY",
#if TAG_MAJOR_VERSION == 34
        "ARTP_METABOLISM",
#endif
        "ARTP_CONTAM",
#if TAG_MAJOR_VERSION == 34
        "ARTP_ACCURACY",
#endif
        "ARTP_SLAYING",
        "ARTP_CURSE",
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
#if TAG_MAJOR_VERSION == 34
        "ARTP_SUSTAT",
#endif
        "ARTP_NO_UPGRADE",
        "ARTP_RCORR",
        "ARTP_RMUT",
#if TAG_MAJOR_VERSION == 34
        "ARTP_TWISTER",
#endif
        "ARTP_CORRODE",
        "ARTP_DRAIN",
        "ARTP_SLOW",
        "ARTP_FRAGILE",
        "ARTP_SHIELDING",
    };
    COMPILE_CHECK(ARRAYSZ(rap_names) == ARTP_NUM_PROPERTIES);

    fprintf(ostat, "                                 All    Good   Bad   Max AvgGood Min AvgBad\n");
    fprintf(ostat, "                           ------------------------------------------------\n");
    fprintf(ostat, "%-27s: %6.2f%% %6.2f%% %6.2f%%\n", "Overall", 100.0,
            (float) total_good_props * 100.0 / (float) total_props,
            (float) total_bad_props * 100.0 / (float) total_props);

    for (int i = 0; i < ARTP_NUM_PROPERTIES; ++i)
    {
        const int total_props_of_type = good_props[i] + bad_props[i];
        if (!total_props_of_type)
            continue;

        const float avg_good = good_props[i] ?
            (float) total_good_prop_vals[i] / (float) good_props[i] :
            0.0;
        const float avg_bad = bad_props[i] ?
            (float) total_bad_prop_vals[i] / (float) bad_props[i] :
            0.0;
        fprintf(ostat, "%-27s: %6.2f%% %6.2f%% %6.2f%% %3d %5.2f %5d %5.2f\n",
                rap_names[i],
                (float) total_props_of_type * 100.0 / (float) num_randarts,
                (float) good_props[i] * 100.0 / (float) num_randarts,
                (float) bad_props[i] * 100.0 / (float) num_randarts,
                max_prop_vals[i], avg_good,
                min_prop_vals[i], avg_bad);
    }

    fprintf(ostat, "\n-----------------------------------------\n\n");
    mpr("Results written into 'items.stat'.");
}

void debug_item_statistics()
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
    case 'b': _debug_rap_stats(ostat); break;
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

void wizard_identify_all_items()
{
    wizard_identify_pack();
    for (auto &item : mitm)
        if (item.defined())
            set_ident_flags(item, ISFLAG_IDENT_MASK);
    for (int ii = 0; ii < NUM_OBJECT_CLASSES; ii++)
    {
        object_class_type i = (object_class_type)ii;
        if (!item_type_has_ids(i))
            continue;
        for (int j = 0; j < get_max_subtype(i); j++)
            set_ident_type(i, j, true);
    }
}

void wizard_unidentify_all_items()
{
    wizard_unidentify_pack();
    for (auto &item : mitm)
        if (item.defined())
            _forget_item(item);
    for (int ii = 0; ii < NUM_OBJECT_CLASSES; ii++)
    {
        object_class_type i = (object_class_type)ii;
        if (!item_type_has_ids(i))
            continue;
        for (int j = 0; j < get_max_subtype(i); j++)
            set_ident_type(i, j, false);
    }
}

void wizard_recharge_evokers()
{
    for (int i = 0; i < NUM_MISCELLANY; ++i)
    {
        item_def dummy;
        dummy.base_type = OBJ_MISCELLANY;
        dummy.sub_type = i;

        if (!is_xp_evoker(dummy))
            continue;

        evoker_debt(dummy.sub_type) = 0;

        if (dummy.sub_type == MISC_LIGHTNING_ROD)
            you.props["thunderbolt_charge"].get_int() = 0;
    }
    mpr("Evokers recharged.");
}
#endif
