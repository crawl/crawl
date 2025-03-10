/**
 * @file
 * @brief Item related wizard functions.
**/

#include "AppHdr.h"

#include "wiz-item.h"

#include <cerrno>

#include "acquire.h"
#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "art-enum.h"
#include "cio.h"
#include "coordit.h"
#include "decks.h"
#include "dbg-util.h"
#include "dungeon.h"
#include "env.h"
#include "god-passive.h"
#include "invent.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "jobs.h"
#include "libutil.h"
#include "macro.h"
#include "makeitem.h"
#include "mapdef.h"
#include "message.h"
#include "mon-death.h"
#include "options.h"
#include "orb-type.h"
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
#include "syscalls.h"
#include "tag-version.h"
#include "terrain.h"
#include "unicode.h"
#include "view.h"

#ifdef WIZARD
static void _make_all_books()
{
    for (int i = 0; i < NUM_BOOKS; ++i)
    {
        if (!book_exists((book_type)i))
            continue;
        int thing = items(false, OBJ_BOOKS, i, 0, 0, AQ_WIZMODE);
        if (thing == NON_ITEM)
            continue;

        move_item_to_grid(&thing, you.pos());

        if (thing == NON_ITEM)
            continue;

        item_def book(env.item[thing]);

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
    string title = "Which item class (ESC to exit)?";
    vector<WizardEntry> options =
    {
        {')', "weapons"}, {'(', "missiles"}, {'[', "armour"}, {'/', "wands"},
        {'?', "scrolls"}, {'=', "jewellery"}, {'!', "potions"}, {':', "books"},
        {'|', "staves"}, {'}', "miscellany"}, {'%', "talismans"},
        {'X', "corpses"}, {'$', "gold"}, {'0', "the Orb"}
    };
    auto menu = WizardMenu(title, options);
    object_class_type class_wanted = item_class_by_sym(menu.run());
    if (NUM_OBJECT_CLASSES == class_wanted)
        return;

    // Allocate an item to play with.
    int thing_created = get_mitm_slot();
    if (thing_created == NON_ITEM)
    {
        mpr("Could not allocate item.");
        return;
    }
    item_def& item(env.item[thing_created]);

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

        if (!place_monster_corpse(dummy, true))
        {
            mpr("Failed to create corpse.");
            return;
        }
    }
    else
    {
        char specs[80];
        string prompt = make_stringf("What type of %s? ",
                                     base_type_string(class_wanted));
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
            origin_acquired(env.item[thing_created], AQ_WIZMODE);
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

    vector<WizardEntry> choices;
    for (unsigned int i = 0, choice = 'a'; i < ARTP_NUM_PROPERTIES; ++i)
    {
#if TAG_MAJOR_VERSION == 34
        if (i == ARTP_METABOLISM || i == ARTP_ACCURACY || i == ARTP_TWISTER)
            continue;
#endif
        auto prop = (artefact_prop_type)i;
        choices.emplace_back(WizardEntry(choice, artp_name(prop), i));

        if (choice == 'z')
            choice = 'A';
        else if (choice == 'Z')
            choice = '0';
        else if (choice == '@' || !choice)
            choice = 0;
        else
            ++choice;
    }
    auto menu = WizardMenu("Change which field (ESC to exit)?", choices);
    if (!menu.run(true))
        return;
    auto prop = static_cast<artefact_prop_type>(menu.result());

    switch (artp_value_type(prop))
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

    int item = prompt_invent_item("Tweak which item? ", menu_type::invlist, OSEL_ANY);

    if (prompt_failed(item))
        return;

    if (you.weapon() && you.weapon()->link == item)
        you.wield_change = true;

    const bool is_art = is_artefact(you.inv[item]);

    while (true)
    {
        int64_t old_val = 0; // flags are uint64_t, but they don't care
        string c_prop = is_art ? "art props" : "special";
        vector<WizardEntry> options =
        {
            {'a', "plus"}, {'b', "plus2"}, {'c', c_prop}, {'d', "quantity"},
            {'e', "flags"}
        };
        mprf_nocap("%s", you.inv[item].name(DESC_INVENTORY_EQUIP).c_str());
        auto menu = WizardMenu("Which field (Esc to exit)?", options);
        keyin = menu.run();

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
        else if (!menu.success())
            return;


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
            && !you.inv[item].props.exists(ARTEFACT_PROPS_KEY))
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
        ash_id_inventory();
    }
}

static bool _make_book_randart(item_def &book)
{
    int type;

    do
    {
        mprf(MSGCH_PROMPT, "Make book fixed [t]heme or fixed [l]evel? ");
        type = toalower(getch_ck());
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
    const int i = prompt_invent_item("Value of which item?",
                                     menu_type::invlist, OSEL_ANY);

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

/**
 * Generate every unrand (including removed ones).
 *
 * @param override_unique if true, will generate unrands that have already
 * placed in the game. If false, will generate fallback randarts for any
 * unrands that have already placed.
 */
void wizard_create_all_artefacts(bool override_unique)
{
    you.octopus_king_rings = 0x00;
    int octorings = 8;

    // Create all unrandarts.
    for (int i = 0; i < NUM_UNRANDARTS; ++i)
    {
        const int              index = i + UNRAND_START;
        const unrandart_entry* entry = get_unrand_entry(index);

        // Skip dummy entries.
        if (entry->base_type == OBJ_UNASSIGNED)
            continue;

        int islot;

        if (override_unique)
        {
            // force create: use make_item_unrandart to override a bunch of the
            // usual checks on getting randarts.
            islot = get_mitm_slot();
            if (islot == NON_ITEM)
                break;

            item_def &tmp_item = env.item[islot];
            make_item_unrandart(tmp_item, index);
            tmp_item.quantity = 1;
        }
        else
        {
            // mimic the way unrands are created normally, and respect
            // uniqueness. If an unrand has already generated, this will place
            // a relevant fallback randart instead. Useful for testing fallback
            // properties, since various error conditions will print.
            islot = items(true, entry->base_type, 0, 0, -index, -1);
            if (islot == NON_ITEM)
            {
                mprf(MSGCH_ERROR, "Failed to generate item for '%s'",
                    entry->name);
                continue;
            }
        }
        item_def &item = env.item[islot];
        identify_item(item);

        if (!is_artefact(item))
        {
            // for now, staves are ok...
            mprf(item.base_type == OBJ_STAVES ? MSGCH_DIAGNOSTICS : MSGCH_ERROR,
                "Made non-artefact '%s' when trying to make '%s'",
                item.name(DESC_A).c_str(), entry->name);
        }
        else
        {
            msg::streams(MSGCH_DIAGNOSTICS) << "Made " << item.name(DESC_A)
                                            << " (" << debug_art_val_str(item)
                                            << ")" << endl;
        }
        move_item_to_grid(&islot, you.pos());

        // Make all eight.
        if (index == UNRAND_OCTOPUS_KING_RING && --octorings)
            i--;
    }

    // Create Horn of Geryon
    int islot = get_mitm_slot();
    if (islot != NON_ITEM)
    {
        item_def& item = env.item[islot];
        item.clear();
        item.base_type = OBJ_MISCELLANY;
        item.sub_type  = MISC_HORN_OF_GERYON;
        item.quantity  = 1;
        item_colour(item);

        identify_item(item);
        move_item_to_grid(&islot, you.pos());

        msg::streams(MSGCH_DIAGNOSTICS) << "Made " << item.name(DESC_A)
                                        << endl;
    }
}

void wizard_make_object_randart()
{
    int i = prompt_invent_item("Make an artefact out of which item?",
                                menu_type::invlist, OSEL_ANY);

    if (prompt_failed(i))
        return;

    item_def &item(you.inv[i]);

    if (is_unrandom_artefact(item))
    {
        mpr("That item is already an unrandom artefact.");
        return;
    }

    if (!item_type_can_be_artefact(item.base_type))
    {
        mpr("That item cannot be turned into an artefact.");
        return;
    }

    const equipment_slot eq = item_equip_slot(item);
    int invslot = 0;
    if (eq != SLOT_UNUSED)
    {
        invslot = item.link;
        unequip_item(item);
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

    // If it was equipped, requip the item.
    if (eq != SLOT_UNUSED)
        equip_item(eq, invslot);

    mprf_nocap("%s", item.name(DESC_INVENTORY_EQUIP).c_str());
}

void wizard_identify_pack()
{
    mpr("You feel a rush of knowledge.");
    identify_inventory();
    you.wield_change  = true;
    quiver::set_needs_redraw();
}

static void _forget_item(item_def &item)
{
    if (item_type_has_ids(item.base_type))
        you.type_ids[item.base_type][item.sub_type] = false;

    item.flags &= ~(ISFLAG_SEEN | ISFLAG_HANDLED | ISFLAG_THROWN | ISFLAG_IDENTIFIED
                    | ISFLAG_DROPPED | ISFLAG_NOTED_ID | ISFLAG_NOTED_GET);
}

void wizard_unidentify_pack()
{
    mpr("You feel a rush of antiknowledge.");
    for (auto &item : you.inv)
        if (item.defined())
            _forget_item(item);

    you.wield_change  = true;
    quiver::set_needs_redraw();

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
    for (const auto &item : env.item)
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
        int item = env.igrid(*ri);
        if (item != NON_ITEM)
        {
            mprf("%3d at (%2d,%2d): %s%s", item, ri->x, ri->y,
                 env.item[item].name(DESC_PLAIN, false, false, false).c_str(),
                 env.item[item].flags & ISFLAG_MIMIC ? " mimic" : "");
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

static void _debug_acquirement_stats()
{
    int p = get_mitm_slot(11);
    if (p == NON_ITEM)
    {
        mpr("Too many items on level.");
        return;
    }
    env.item[p].base_type = OBJ_UNASSIGNED;

    clear_messages();

    vector<WizardEntry> choices;
    object_class_type list[] =
    {
        OBJ_WEAPONS, OBJ_ARMOUR, OBJ_JEWELLERY, OBJ_BOOKS, OBJ_STAVES,
        OBJ_MISCELLANY, OBJ_TALISMANS
    };
    char c = 'a';
    for (auto typ : list)
        choices.emplace_back(WizardEntry(c++, item_class_name(typ), typ));
    auto menu = WizardMenu("What kind of item would you like to get acquirement"
                           " stats on (ESC to exit)?", choices, OBJ_UNASSIGNED);
    if (!menu.run(true))
        return;
    auto type = static_cast<object_class_type>(menu.result());

    const int num_itrs = prompt_for_int("How many iterations? ", true);
    if (num_itrs < 0)
    {
        canned_msg(MSG_OK);
        return;
    }

    FILE *ostat = fopen_u("acquirement_stats.txt", "a");
    if (!ostat)
    {
        mprf(MSGCH_ERROR, "Can't write to acquirement_stats.txt: %s",
                strerror(errno));
        return;
    }

    int acq_calls    = 0;
    int total_quant  = 0;
    short max_plus   = -127;
    int total_plus   = 0;
    int num_arts     = 0;
    int randbook_spells = 0;

    int subtype_quants[256];
    int ego_quants[256];

    memset(subtype_quants, 0, sizeof(subtype_quants));
    memset(ego_quants, 0, sizeof(ego_quants));

    for (int i = 0; i < num_itrs; ++i)
    {
        if (kbhit())
        {
            getch_ck();
            mpr("Stopping early due to keyboard input.");
            break;
        }

        const int item_index = acquirement_create_item(type, AQ_WIZMODE, true,
                coord_def(0, 0));

        if (item_index == NON_ITEM || !env.item[item_index].defined())
        {
            mpr("Acquirement failed, stopping early.");
            break;
        }

        item_def &item(env.item[item_index]);

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
                    ego_quants[SPSCHOOL_LAST_EXPONENT + level]++;
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

        if (num_itrs >= 10 && (i + 1) % (num_itrs / 10) == 0)
        {
            clear_messages();
            mprf("%d%% done.", 100 * (i + 1) / num_itrs);
            viewwindow();
            update_screen();
        }
    }

    if (total_quant == 0 || acq_calls == 0)
    {
        mpr("No items generated.");
        return;
    }

    // Print acquirement base type.
    fprintf(ostat, "Acquiring %s for:\n\n", item_class_name(type, true));

    // Print player species/profession.
    string godname = "";
    if (!you_worship(GOD_NO_GOD))
        godname += " of " + god_name(you.religion);

    fprintf(ostat, "%s %s, Level %d %s %s%s\n\n",
            you.your_name.c_str(), player_title().c_str(),
            you.experience_level,
            species::name(you.species).c_str(),
            get_job_name(you.char_class), godname.c_str());

    bool naked = true;
    for (player_equip_entry& entry : you.equipment.items)
    {
        // We can't acquire for the gizmo slots. Skip it.
        if (entry.slot = SLOT_GIZMO)
            continue;

        if (entry.is_overflow)
            continue;

        fprintf(ostat, "%-7s: %s %s\n", equip_slot_name(entry.slot),
                entry.get_item().name(DESC_PLAIN, true).c_str(),
                entry.melded ? "(melded)" : "");
        naked = false;
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
        vector<int> total_spells(SPSCHOOL_LAST_EXPONENT + 1);
        vector<int> unseen_spells(SPSCHOOL_LAST_EXPONENT + 1);

        for (int i = 0; i < NUM_SPELLS; ++i)
        {
            const spell_type spell = (spell_type) i;

            if (!is_valid_spell(spell))
                continue;

            if (!you_can_memorise(spell))
                continue;

            // Only use actual player spells.
            if (!is_player_book_spell(spell))
                continue;

            const bool seen = you.spell_library[spell];

            const spschools_type disciplines = get_spell_disciplines(spell);
            for (int d = 0; d <= SPSCHOOL_LAST_EXPONENT; ++d)
            {
                const auto disc = spschools_type::exponent(d);

                if (disciplines & disc)
                {
                    total_spells[d]++;
                    if (!seen)
                        unseen_spells[d]++;
                }
            }
        }
        for (int d = 0; d <= SPSCHOOL_LAST_EXPONENT; ++d)
        {
            const auto disc = spschools_type::exponent(d);

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
        for (int i = 0; i < NUM_SPECIAL_WEAPONS; ++i)
        {
            const brand_type brand = static_cast<brand_type>(i);
            if (ego_quants[i] > 0)
            {
                fprintf(ostat, "%14s: %5.2f\n", brand_type_name(brand, false),
                        100.0 * (float) ego_quants[i] / (float) acq_calls);
            }
        }

        fprintf(ostat, "\n\n");
    }
    else if (type == OBJ_ARMOUR)
    {
        fprintf(ostat, "Maximum plus: %d\n", max_plus);
        fprintf(ostat, "Average plus: %5.2f\n\n",
                (float) total_plus / (float) acq_calls);

        fprintf(ostat, "Egos (excluding artefacts):\n");
        const int non_art = acq_calls - num_arts;
        for (int i = 0; i < NUM_SPECIAL_ARMOURS; ++i)
        {
            const special_armour_type ego = static_cast<special_armour_type>(i);
            if (ego_quants[i] > 0)
            {
                fprintf(ostat, "%17s: %5.2f\n", special_armour_type_name(ego, false),
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

            for (int i = 0; i <= SPSCHOOL_LAST_EXPONENT; ++i)
            {
                const auto school = spschools_type::exponent(i);
                if (ego_quants[i] > 0)
                {
                    fprintf(ostat, "%17s: %5.2f\n",
                            spelltype_long_name(school),
                            100.0 * (float) ego_quants[i] / (float) num_arts);
                }
            }
            // List levels for fixed level randarts.
            for (int i = 1; i < 9; ++i)
            {
                const int k = SPSCHOOL_LAST_EXPONENT + i;
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
    snprintf(format_str, sizeof(format_str), "%%%ds: %%6.2f\n", max_width);

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
    fclose(ostat);
    mpr("Results written into acquirement_stats.txt.");
}

/**
 * Take the median of the provided dataset. Mutates it for efficiency.
 */
static int _median(vector<int> &counts)
{
    nth_element(counts.begin(), counts.begin() + counts.size()/2, counts.end());
    return counts[counts.size()/2];
}

static void _debug_randart_stats()
{
    char buf[1024];
    mprf(MSGCH_PROMPT, "Enter name of item (or ITEM spec): ");
    if (cancellable_get_line_autohist(buf, sizeof buf) || !*buf)
    {
        canned_msg(MSG_OK);
        return;
    }

    item_list ilist;
    const string err = ilist.add_item(buf, false);
    if (!err.empty())
    {
        mprf(MSGCH_ERROR, "Error: %s", err.c_str());
        return;
    }

    // Save an item_spec based on the input so we can re-use it, and make it
    // always be randart.
    item_spec ispec = ilist.get_item(0);
    ispec.level = ISPEC_RANDART;

    const int num_itrs = prompt_for_int("How many iterations? ", true);
    if (num_itrs < 0)
    {
        canned_msg(MSG_OK);
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

    FILE *ostat = fopen_u("randart_stats.txt", "a");
    if (!ostat)
    {
        mprf(MSGCH_ERROR, "Can't write to randart_stats.txt: %s",
                strerror(errno));
        return;
    }
    fprintf(ostat, "Generating randart stats for: %s\n", buf);

    for (int i = 0; i < num_itrs; i++)
    {
        if (kbhit())
        {
            getch_ck();
            mpr("Stopping early due to keyboard input.");
            break;
        }


        int ind, tries = 0;
        do
        {
            ind = dgn_place_item(ispec, coord_def(0, 0));
            if (ind != NON_ITEM
                && (!is_random_artefact(env.item[ind])
                    || env.item[ind].base_type == OBJ_BOOKS))
            {
                destroy_item(env.item[ind], true);
                ind = NON_ITEM;
            }
        } while (ind == NON_ITEM && ++tries < 100);

        if (ind == NON_ITEM)
        {
            mprf(MSGCH_ERROR, "Unable to make artefact from '%s'", buf);
            return;
        }

        const auto &item = env.item[ind];
        artefact_properties_t proprt;
        artefact_properties(item, proprt);

        if (randart_is_bad(item, proprt))
        {
            bad_randarts++;
            destroy_item(env.item[ind], true);
            continue;
        }

        num_randarts++;

        int num_good_props = 0, num_bad_props = 0;
        for (int j = 0; j < ARTP_NUM_PROPERTIES; ++j)
        {
            const auto prop = static_cast<artefact_prop_type>(j);
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

        destroy_item(env.item[ind], true);

        if (num_itrs >= 10 && (i + 1) % (num_itrs / 10) == 0)
        {
            clear_messages();
            mprf("%d%% done.", 100 * (i + 1) / num_itrs);
            viewwindow();
            update_screen();
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

    fprintf(ostat, "Max # of props = %d, mean = %5.2f, median = %d\n",
            max_props, (float) total_props / (float) num_randarts,
            _median(total_prop_counts));
    fprintf(ostat, "Max # of good props = %d, mean = %5.2f, median = %d\n",
            max_good_props, (float) total_good_props / (float) num_randarts,
            _median(good_prop_counts));
    fprintf(ostat, "Max # of bad props = %d, mean = %5.2f, median = %d\n",
            max_bad_props, (float) total_bad_props / (float) num_randarts,
            _median(bad_prop_counts));
    fprintf(ostat, "Max (good - bad) props = %d, avg # = %5.2f\n\n",
            max_balance_props,
            (float) total_balance_props / (float) num_randarts);

    fprintf(ostat, "                                 All    Good   Bad   Max AvgGood Min AvgBad\n");
    fprintf(ostat, "                           ------------------------------------------------\n");
    fprintf(ostat, "%-27s: %6.2f%% %6.2f%% %6.2f%%\n", "Overall", 100.0,
            (float) total_good_props * 100.0 / (float) total_props,
            (float) total_bad_props * 100.0 / (float) total_props);

    for (int i = 0; i < ARTP_NUM_PROPERTIES; ++i)
    {
        const auto prop = static_cast<artefact_prop_type>(i);

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
                artp_name(prop),
                (float) total_props_of_type * 100.0 / (float) num_randarts,
                (float) good_props[i] * 100.0 / (float) num_randarts,
                (float) bad_props[i] * 100.0 / (float) num_randarts,
                max_prop_vals[i], avg_good,
                min_prop_vals[i], avg_bad);
    }

    fprintf(ostat, "\n-----------------------------------------\n\n");
    mpr("Results written into 'randart_stats.txt'.");
    fclose(ostat);
}

void debug_item_statistics()
{
    mpr("Generate stats for: [a] acquirement [b] randart properties");
    flush_prev_message();

    const int keyin = toalower(get_ch());
    switch (keyin)
    {
    case 'a': _debug_acquirement_stats(); break;
    case 'b': _debug_randart_stats(); break;
    default:
        canned_msg(MSG_OK);
        break;
    }
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
            card_effect(c);
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
    for (auto &item : env.item)
        if (item.defined())
            identify_item(item);
    for (auto& entry : env.shop)
        for (auto &item : entry.second.stock)
            identify_item(item);
    for (int ii = 0; ii < NUM_OBJECT_CLASSES; ii++)
    {
        object_class_type i = (object_class_type)ii;
        if (!item_type_has_ids(i))
            continue;
        for (const auto j : all_item_subtypes(i))
            identify_item_type(i, j);
    }
}

void wizard_unidentify_all_items()
{
    wizard_unidentify_pack();
    for (auto &item : env.item)
        if (item.defined())
            _forget_item(item);
    for (auto& entry : env.shop)
        for (auto &item : entry.second.stock)
            _forget_item(item);
    for (int ii = 0; ii < NUM_OBJECT_CLASSES; ii++)
    {
        object_class_type i = (object_class_type)ii;
        if (!item_type_has_ids(i))
            continue;
        for (const auto j : all_item_subtypes(i))
            you.type_ids[i][j] = false;
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
    }
    mpr("Evokers recharged.");
}

void wizard_unobtain_runes_and_orb()
{
    you.runes.reset();

    you.chapter = CHAPTER_ORB_HUNTING;
    invalidate_agrid(true);

    mpr("Unobtained all runes and the Orb of Zot.");
}
#endif
