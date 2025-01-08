/**
 * @file
 * @brief Known items menu.
**/

#include "AppHdr.h"

#include "decks.h"
#include "describe.h"
#include "english.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "item-status-flag-type.h"
#include "known-items.h"
#include "libutil.h"
#include "options.h"
#include "stringutil.h"
#include "tag-version.h"
#include "unicode.h"

class KnownMenu : public InvMenu
{
public:
    KnownMenu(bool show_unknown, bool _all_items_known)
        : InvMenu(MF_QUIET_SELECT | MF_ALLOW_FORMATTING
                    | ((show_unknown) ? MF_NOSELECT
                                      : MF_MULTISELECT | MF_ALLOW_FILTER)),
        all_items_known(_all_items_known)
    {
        set_type(menu_type::know);
        if (!Options.single_column_item_menus)
            set_flags(get_flags() | MF_USE_TWO_COLUMNS);
        set_more(); // force derived class get_keyhelp()
    }


    // This loads items in the order they are put into the list (sequentially)
    menu_letter load_items_seq(const vector<const item_def*> &mitems,
                               MenuEntry *(*procfn)(MenuEntry *me) = nullptr,
                               menu_letter ckey = 'a')
    {
        for (const item_def *item : mitems)
        {
            InvEntry *ie = new InvEntry(*item);
            if (tag == "pickup")
                ie->tag = "pickup";
            // If there's no hotkey, provide one.
            if (ie->hotkeys[0] == ' ')
                ie->hotkeys[0] = ckey++;
            do_preselect(ie);

            add_entry(procfn? (*procfn)(ie) : ie);
        }

        return ckey;
    }

protected:
    string help_key() const override
    {
        return "known-menu";
    }

    bool examine_index(int) override
    {
        // disable behavior from InvMenu
        return true;
    }

    bool process_key(int key) override
    {
        bool resetting = (lastch == CONTROL('D'));
        if (resetting)
        {
            //return the menu title to its previous text.
            set_title(temp_title);
            num = -2;

            // Disarm ^D here, because process_key doesn't always set lastch.
            lastch = ' ';
        }
        else
            num = -1;

        if (ui::key_exits_popup(key))
        {
            if (resetting)
                return true;
            lastch = key;
            return false;
        }

        switch (key)
        {
        case ',':
            return true;
        case '*':
            if (!resetting)
                break;
        case '^':
            key = ',';
            break;

        case '-': // TODO: assimilate to CMD_MENU_CYCLE_MODE?
        case '\\':
            if (all_items_known)
                return true; // skip process_key for '-', it's confusing
            lastch = key;
            return false;

        case CONTROL('D'):
            // If we cannot select anything (e.g. on the unknown items
            // page), ignore Ctrl-D. Likewise if the last key was
            // Ctrl-D (we have already disarmed Ctrl-D for the next
            // keypress by resetting lastch).
            if (flags & (MF_SINGLESELECT | MF_MULTISELECT) && !resetting)
            {
                lastch = CONTROL('D');
                temp_title = title->text;
                set_title("Select to reset item to default ([*] for all): ");
            }

            return true;
        }
        return Menu::process_key(key);
    }

    string get_keyhelp(bool scrollable) const override
    {
        string navigation;
        if (is_set(MF_NOSELECT))
        {
            navigation = "<lightgrey>";
            if (scrollable)
            {
                navigation +=
                    menu_keyhelp_cmd(CMD_MENU_PAGE_DOWN) + " page down  "
                    + menu_keyhelp_cmd(CMD_MENU_PAGE_UP) + " page up  ";
            }
            navigation += "[<w>-</w>] recognised  "
                            + menu_keyhelp_cmd(CMD_MENU_EXIT) + " exit"
                            "</lightgrey>";
        }
        else
        {
            // very similar to the MF_MULTISELECT case for regular Menus, but
            // various differences require an override
            navigation = "<lightgrey>" + menu_keyhelp_select_keys()
                         + " select  ";

            if (scrollable)
            {
                navigation +=
                    menu_keyhelp_cmd(CMD_MENU_PAGE_DOWN) + " page down  "
                    + menu_keyhelp_cmd(CMD_MENU_PAGE_UP) + " page up  ";
            }
            navigation += "</lightgrey>";
            navigation = pad_more_with_esc(navigation);
            navigation +=
                    "\n<lightgrey>"
                    "Letters toggle autopickup  ";
            if (is_set(MF_ARROWS_SELECT))
            {
                navigation += menu_keyhelp_cmd(CMD_MENU_TOGGLE_SELECTED)
                    + " toggle selected  ";
            }

            if (!all_items_known)
            {
                navigation +=
                    "[<w>-</w>] unrecognised"
                    "</lightgrey>";
            }
        }

        return pad_more_with(navigation,
                            "<lightgrey>[<w>XXX</w>]</lightgrey>", MIN_COLS);
    }

    bool all_items_known;
};

class KnownEntry : public InvEntry
{
public:
    KnownEntry(InvEntry* inv) : InvEntry(*inv->item)
    {
        hotkeys[0] = inv->hotkeys[0];
        selected_qty = inv->selected_qty;
    }

    virtual string get_text() const override
    {
        string name;

        if (item->base_type == OBJ_MISCELLANY)
            name = pluralise(item->name(DESC_DBNAME));
#if TAG_MAJOR_VERSION == 34
        else if (item->base_type == OBJ_FOOD)
            name = "removed food";
#endif
        else if (item->is_type(OBJ_BOOKS, BOOK_MANUAL))
            name = "manuals";
        else if (item->is_type(OBJ_BOOKS, 0))
            name = "spellbooks";
        else if (item->base_type == OBJ_GOLD)
        {
            name = lowercase_string(item_class_name(item->base_type));
            name = pluralise(name);
        }
        else if (item->base_type == OBJ_RUNES)
            name = "runes";
        else if (item->base_type == OBJ_GEMS)
            name = "gems";
        else if (item->sub_type == get_max_subtype(item->base_type))
        {
            name = "unknown "
                   + lowercase_string(item_class_name(item->base_type));
        }
        else if (item->base_type == OBJ_JEWELLERY
                 || item->base_type == OBJ_WANDS)
        {
            name = pluralise(item->name(DESC_DBNAME));
        }
        else if (item->base_type == OBJ_SCROLLS
                 || item->base_type == OBJ_POTIONS)
        {
            name = pluralise(item->name(DESC_DBNAME))
                   + " (" + describe_item_rarity(*item) + ")";
        }
        else
        {
            name = item->name(DESC_PLAIN, false, true, false, false);
            name = pluralise(name);
        }

        char symbol;
        if (selected_qty == 0)
            symbol = item_needs_autopickup(*item) ? '+' : '-';
        else if (selected_qty == 1)
            symbol = '+';
        else
            symbol = '-';

        return make_stringf(" %c %c %s", hotkeys[0], symbol, name.c_str());
    }

    virtual int highlight_colour(bool) const override
    {
        if (selected_qty >= 1)
            return WHITE;
        else if (is_useless_item(*item, false))
            return DARKGREY;
        else
            return MENU_ITEM_STOCK_COLOUR;

    }

    virtual bool selected() const override
    {
        return selected_qty != 0 && quantity;
    }

    virtual void select(int qty) override
    {
        // Toggle  grey -> - -> + -> grey  if we autopickup the item by
        // default, or  grey -> + -> - -> grey  if we do not.
        if (qty == -2)
            selected_qty = 0;
        else if (selected_qty == 0)
            selected_qty = item_needs_autopickup(*item, true) ? 2 : 1;
        else if (selected_qty == (item_needs_autopickup(*item, true) ? 2 : 1))
            selected_qty = 3 - selected_qty; // 2 <-> 1
        else
            selected_qty = 0;

        if (selected_qty == 2)
            set_item_autopickup(*item, AP_FORCE_OFF);
        else if (selected_qty == 1)
            set_item_autopickup(*item, AP_FORCE_ON);
        else
            set_item_autopickup(*item, AP_FORCE_NONE);
    }
};

class UnknownEntry : public InvEntry
{
public:
    UnknownEntry(InvEntry* inv) : InvEntry(*inv->item)
    {
    }

    virtual string get_text() const override
    {
        if (item->base_type == OBJ_SCROLLS || item->base_type == OBJ_POTIONS)
        {
            return " " + item->name(DESC_PLAIN, false, true, false)
                   + " (" + describe_item_rarity(*item) + ")";
        }

        description_level_type desctype =
            item->base_type == OBJ_WANDS ? DESC_DBNAME : DESC_PLAIN;

        return " " + item->name(desctype, false, true, false, false);
    }
};

static MenuEntry *known_item_mangle(MenuEntry *me)
{
    unique_ptr<InvEntry> ie(dynamic_cast<InvEntry*>(me));
    KnownEntry *newme = new KnownEntry(ie.get());
    return newme;
}

static MenuEntry *unknown_item_mangle(MenuEntry *me)
{
    unique_ptr<InvEntry> ie(dynamic_cast<InvEntry*>(me));
    UnknownEntry *newme = new UnknownEntry(ie.get());
    return newme;
}

static bool _identified_item_names(const item_def *it1,
                                   const item_def *it2)
{
    description_level_type desc =
        it1->base_type == OBJ_JEWELLERY ? DESC_DBNAME : DESC_PLAIN;

    return it1->name(desc, false, true, false, false)
         < it2->name(desc, false, true, false, false);
}

// Allocate (with new) a new item_def with the given base and sub types,
// add a pointer to it to the items vector, and if it has a force_autopickup
// setting add a corresponding SelItem to selected.
static void _add_fake_item(object_class_type base, int sub,
                           vector<SelItem> &selected,
                           vector<const item_def*> &items,
                           bool force_known_type = false)
{
    item_def* ptmp = new item_def;

    ptmp->base_type = base;
    ptmp->sub_type  = sub;
    ptmp->quantity  = 1;
    ptmp->rnd       = 1;

    if (base == OBJ_WANDS && sub != NUM_WANDS)
        ptmp->charges = wand_charge_value(ptmp->sub_type);
    else if (base == OBJ_GOLD)
        ptmp->quantity = 18;

    if (force_known_type)
        ptmp->flags |= ISFLAG_IDENTIFIED;

    items.push_back(ptmp);

    if (you.force_autopickup[base][sub] == AP_FORCE_ON)
        selected.emplace_back(0, 1, ptmp);
    else if (you.force_autopickup[base][sub] == AP_FORCE_OFF)
        selected.emplace_back(0, 2, ptmp);
}

void check_item_knowledge(bool unknown_items)
{
    // TODO: refactor most of this into the menu class...

    vector<const item_def*> items;
    vector<const item_def*> items_missile; //List of missiles should go after normal items
#if TAG_MAJOR_VERSION == 34
    vector<const item_def*> items_food;    //List of foods should come next
#endif
    vector<const item_def*> items_misc;
    vector<const item_def*> items_talismans;
    vector<const item_def*> items_other;   //List of other items should go after everything
    vector<SelItem> selected_items;

    bool all_items_known = true;
    for (int ii = 0; ii < NUM_OBJECT_CLASSES; ii++)
    {
        object_class_type i = (object_class_type)ii;
        if (!item_type_has_ids(i))
            continue;
        for (const auto j : all_item_subtypes(i))
        {
            if (i == OBJ_JEWELLERY && j >= NUM_RINGS && j < AMU_FIRST_AMULET)
                continue;

            const bool known = you.type_ids[i][j];

            // Don't show items the player knows can't generate.
            // (unless they *have* generated, ha...)
            if (!known && item_known_excluded_from_set((object_class_type)i, j))
                continue;

            if (known != unknown_items) // logical xor
                _add_fake_item(i, j, selected_items, items, !unknown_items);
            else
                all_items_known = false;
        }
    }

    if (unknown_items)
        all_items_known = false;
    else
    {
        // items yet to be known
        for (int ii = 0; ii < NUM_OBJECT_CLASSES; ii++)
        {
            object_class_type i = (object_class_type)ii;
            if (i == OBJ_BOOKS || !item_type_has_ids(i))
                continue;
            _add_fake_item(i, get_max_subtype(i), selected_items, items);
        }
        // Missiles
        for (int i = 0; i < NUM_MISSILES; i++)
        {
#if TAG_MAJOR_VERSION == 34
            switch (i)
            {
            case MI_NEEDLE:
            case MI_ARROW:
            case MI_BOLT:
            case MI_SLING_BULLET:
            case MI_SLUG:
                continue;
            }
#endif
            _add_fake_item(OBJ_MISSILES, i, selected_items, items_missile);
        }

        for (int i = 0; i < NUM_MISCELLANY; i++)
        {
            if (i == MISC_HORN_OF_GERYON
                || i == MISC_ZIGGURAT
                || i == MISC_SHOP_VOUCHER
#if TAG_MAJOR_VERSION == 34
                || is_deck_type(i)
                || i == MISC_BUGGY_EBONY_CASKET
                || i == MISC_BUGGY_LANTERN_OF_SHADOWS
                || i == MISC_BOTTLED_EFREET
                || i == MISC_RUNE_OF_ZOT
                || i == MISC_STONE_OF_TREMORS
                || i == MISC_FAN_OF_GALES
                || i == MISC_LAMP_OF_FIRE
                || i == MISC_CRYSTAL_BALL_OF_ENERGY
                || i == MISC_XOMS_CHESSBOARD
#endif
                || (i == MISC_QUAD_DAMAGE && !crawl_state.game_is_sprint()))
            {
                continue;
            }
            _add_fake_item(OBJ_MISCELLANY, i, selected_items, items_misc);
        }

        for (int i = 0; i < NUM_TALISMANS; i++)
            _add_fake_item(OBJ_TALISMANS, i, selected_items, items_talismans);

        // N.b. NUM_BOOKS drastically exceeds MAX_SUBTYPES, but it doesn't
        // matter for force_autopickup purposes because we only use 0 and
        // BOOK_MANUAL
        COMPILE_CHECK(BOOK_MANUAL < MAX_SUBTYPES);

        // Misc.
        static const pair<object_class_type, int> misc_list[] =
        {
            { OBJ_BOOKS, BOOK_MANUAL },
            { OBJ_GOLD, 1 },
            { OBJ_BOOKS, 0 },
            { OBJ_RUNES, NUM_RUNE_TYPES },
            { OBJ_GEMS, NUM_GEM_TYPES },
        };
        for (auto e : misc_list)
            _add_fake_item(e.first, e.second, selected_items, items_other);
    }

    sort(items.begin(), items.end(), _identified_item_names);
    sort(items_missile.begin(), items_missile.end(), _identified_item_names);
#if TAG_MAJOR_VERSION == 34
    sort(items_food.begin(), items_food.end(), _identified_item_names);
#endif
    sort(items_misc.begin(), items_misc.end(), _identified_item_names);
    // Intentionally don't sort talismans so that they're ordered by tier instead.
    // (This is dubious!)

    KnownMenu menu(unknown_items, all_items_known);
    string stitle;

    if (unknown_items)
        stitle = "Items not yet recognised:";
    else if (!all_items_known)
        stitle = "Recognised items. (Select to toggle autopickup)";
    else
        stitle = "You recognise all items. (Select to toggle autopickup)";

    string prompt = "(_ for help)";
    //TODO: when the menu is opened, the text is not justified properly.
    stitle = stitle + string(max(0, MIN_COLS - strwidth(stitle + prompt)),
                             ' ') + prompt;

    menu.set_preselect(&selected_items);
    menu_letter ml;
    ml = menu.load_items(items, unknown_items ? unknown_item_mangle
                                              : known_item_mangle, 'a', false);

    ml = menu.load_items(items_missile, known_item_mangle, ml, false);
#if TAG_MAJOR_VERSION == 34
    ml = menu.load_items(items_food, known_item_mangle, ml, false);
#endif
    ml = menu.load_items(items_misc, known_item_mangle, ml, false);
    ml = menu.load_items(items_talismans, known_item_mangle, ml, false);
    if (!items_other.empty())
    {
        menu.add_entry(new MenuEntry("Other Items", MEL_SUBTITLE));
        ml = menu.load_items_seq(items_other, known_item_mangle, ml);
    }

    menu.set_title(stitle);
    menu.show(true);

    auto last_char = menu.getkey();

    deleteAll(items);
    deleteAll(items_missile);
#if TAG_MAJOR_VERSION == 34
    deleteAll(items_food);
#endif
    deleteAll(items_misc);
    deleteAll(items_talismans);
    deleteAll(items_other);

    if (!all_items_known && (last_char == '\\' || last_char == '-'))
        check_item_knowledge(!unknown_items);
}
