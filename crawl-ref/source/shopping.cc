/**
 * @file
 * @brief Shop keeper functions.
**/

#include "AppHdr.h"

#include "shopping.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "artefact.h"
#include "branch.h"
#include "cio.h"
#include "decks.h"
#include "describe.h"
#include "dgn-overview.h"
#include "english.h"
#include "env.h"
#include "files.h"
#include "food.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "macro.h"
#include "menu.h"
#include "message.h"
#include "notes.h"
#include "output.h"
#include "place.h"
#include "player.h"
#include "prompt.h"
#include "rot.h"
#include "spl-book.h"
#include "stash.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#ifdef USE_TILE_LOCAL
#include "tilereg-crt.h"
#endif
#include "travel.h"
#include "unwind.h"

#define SHOPPING_LIST_COST_KEY "shopping_list_cost_key"

ShoppingList shopping_list;

static bool _in_shop_now = false;

enum ordering_mode
{
    ORDER_MODE_DEFAULT,
    ORDER_MODE_PRICE,
    ORDER_MODE_ALPHABETICAL,
    ORDER_MODE_TYPE,
    NUM_ORDER_MODES
};

static ordering_mode operator++(ordering_mode &x)
{
    x = static_cast<ordering_mode>(x + 1);
    if (x == NUM_ORDER_MODES)
        x = ORDER_MODE_DEFAULT;
    return x;
}

static ordering_mode shopping_order = ORDER_MODE_DEFAULT;

static const char * const shopping_order_names[NUM_ORDER_MODES] =
{
    "default", "price", "name", "type"
};

static bool _purchase(int shop, int item_got, int cost, bool id);

static void _shop_print(const char *shoppy, int line)
{
    cgotoxy(1, line + 19, GOTO_CRT);
    cprintf("%s", shoppy);
    clear_to_end_of_line();
}

static void _shop_more()
{
    cgotoxy(65, 20, GOTO_CRT);
    cprintf("-more-");
    get_ch();
}

static bool _shop_yesno(const char* prompt, int safeanswer)
{
#ifdef TOUCH_UI
    return yesno(prompt, true, safeanswer, false, false, false, NULL, GOTO_CRT);
#else
    if (_in_shop_now)
    {
        textcolour(channel_to_colour(MSGCH_PROMPT));
        _shop_print(prompt, 1);

        return yesno(NULL, true, safeanswer, false, false, true);
    }
    else
        return yesno(prompt, true, safeanswer, false, false, false);
#endif
}

static void _shop_mpr(const char* msg)
{
    if (_in_shop_now)
    {
        _shop_print(msg, 1);
        _shop_more();
    }
    else
        mpr(msg);
}

#ifdef USE_TILE_LOCAL
static void _draw_shop_fs(const char hotkey, formatted_string &fs,
                          MenuFreeform* freeform)
{
    TextItem* tmp = new TextItem();

    tmp->set_fg_colour(LIGHTGRAY);
    tmp->set_bg_colour(BLACK);
    tmp->set_highlight_colour(WHITE);
    tmp->set_text("");
    tmp->set_bounds(coord_def(wherex(), wherey()),
                    coord_def(wherex() + fs.tostring().size(), wherey() + 1));
    tmp->add_hotkey(hotkey);
    tmp->set_id(hotkey);
    tmp->set_description_text(fs.tostring());
    freeform->attach_item(tmp);
    fs.display();
    tmp->set_visible(true);
}
#else
static void _draw_shop_fs(const char hotkey, formatted_string &fs,
                          const bool freeform)
{
    fs.display();
}
#endif

static string _hyphenated_letters(int how_many, char first)
{
    string s = "<w>";
    s += first;
    s += "</w>";
    if (how_many > 1)
    {
        s += "-<w>";
        s += first + how_many - 1;
        s += "</w>";
    }
    return s;
}

static bool _can_shoplist(level_id lev = level_id::current())
{
    // TODO: temporary shoplists
    return is_connected_branch(lev.branch);
}

static void _list_shop_keys(bool viewing, int total_stock
#ifdef USE_TILE_LOCAL
                            , MenuFreeform* freeform
#endif
                            )
{
    ASSERT(total_stock > 0);

#ifndef USE_TILE_LOCAL
    const bool freeform = false; // unused
#endif

    const int numlines = get_number_of_lines();
    formatted_string fs;

    // ///////// EXIT //////////
    // set cursor [line 1]
    cgotoxy(1, numlines - 1, GOTO_CRT);

    fs = formatted_string::parse_string(
#if defined(USE_TILE) && !defined(TOUCH_UI)
            "[<w>Esc</w>/<w>R-Click</w>] exit"
#else
            //               "/R-Click"
            "[<w>Esc</w>] exit        "
#endif
            );

    // print menu item
    _draw_shop_fs('x', fs, freeform);

    // ///////// BUY/EXAMINE TOGGLE //////////
    // strlen("[Esc/R-Click] exit") = 18
    // set cursor [18 chars of text + 2 spaces + start at 1]
    cgotoxy(21, numlines - 1, GOTO_CRT);

    fs = formatted_string::parse_string(make_stringf(
            "[<w>!</w>] %s items",
            (viewing ? "<w>examine</w>|buy" : "examine|<w>buy</w>") ));

    _draw_shop_fs('!', fs, freeform);

    // ///////// SELECT ITEM TO BUY/EXAMINE //////////
    // strlen("[!] examine|buy items") = 21
    // set cursor [21 from above, + 21 chars + 5 whitespace]
    cgotoxy(47, numlines - 1, GOTO_CRT);

    // calculate and draw formatted text
    string keys = _hyphenated_letters(total_stock, 'a');

    keys = "[" + keys + "] select item "
            + (viewing ? "to examine" : "for purchase");

    fs = formatted_string::parse_string(keys.c_str());
    fs.display();

    // ///////// SORT MODE //////////
    // strlen("[/] sort (default)") = 18
    cgotoxy(1, numlines, GOTO_CRT);
    ASSERT_RANGE(shopping_order, ORDER_MODE_DEFAULT, NUM_ORDER_MODES);
    string sortmode = make_stringf("[<w>/</w>] sort (%s)",
                                   shopping_order_names[shopping_order]);
    fs = formatted_string::parse_string(sortmode.c_str());
    _draw_shop_fs('/', fs, freeform);

    // ///////// MAKE PURCHASE //////////
    // set cursor [last line], align with 21 from line above
    cgotoxy(21, numlines, GOTO_CRT);
    fs = formatted_string::parse_string(
            "[<w>Enter</w>"
            "] make purchase");

    _draw_shop_fs(' ', fs, freeform);

    // ///////// PUT ITEM ON SHOPPING LIST //////////
    // cursor at 47 to align with line above
    cgotoxy(47, numlines, GOTO_CRT);

    keys = _hyphenated_letters(total_stock, 'A');
    keys = "[" + keys + "] put item on shopping list";

    fs = formatted_string::parse_string(keys.c_str());
    fs.display();
}

static vector<int> _shop_get_stock(int shopidx)
{
    vector<int> result;
    // Shop items are heaped up at this cell.
    const coord_def stack_location(0, 5 + shopidx);
    for (stack_iterator si(stack_location); si; ++si)
        result.push_back(si.link());
    return result;
}

static int _shop_get_item_value(const item_def& item, int greed, bool id)
{
    int result = (greed * item_value(item, id) / 10);

    return max(result, 1);
}

// Comparator for sorting a permutation list according to the shop, the
// original list of item IDs, and the current ordering mode.
class ShopSorter
{
public:
    ShopSorter(const shop_struct &shop, const vector<int>& stocklist)
        : stock(stocklist), id(shoptype_identifies_stock(shop.type)),
          greed(shop.greed)
    {}

    bool operator()(int a_index, int b_index) const
    {
        item_def& a = mitm[stock[a_index]];
        item_def& b = mitm[stock[b_index]];

        switch (shopping_order)
        {
        case ORDER_MODE_PRICE:
            // Greed will affect all items equally (other than rounding
            // error), so don't bother taking the shop's actual greed level.
            return _shop_get_item_value(a, greed, id)
                   < _shop_get_item_value(b, greed, id);
        case ORDER_MODE_ALPHABETICAL:
            return a.name(DESC_PLAIN, false, id)
                   < b.name(DESC_PLAIN, false, id);
        case ORDER_MODE_TYPE:
            if (a.base_type == b.base_type)
                return a.sub_type < b.sub_type;
            else
                return a.base_type < b.base_type;
        case ORDER_MODE_DEFAULT:
        default:
            return a_index < b_index;
        }
    }
private:
    const vector<int>& stock;
    const bool id;
    const int greed;
};

static vector<int> stock_order;

static void _shop_print_stock(const vector<int>& stock,
                              const vector<bool>& selected,
                              const vector<bool>& in_list,
                              const shop_struct& shop,
                              int total_cost, bool viewing
#ifdef USE_TILE_LOCAL
                              , MenuFreeform* freeform
#endif
                              )
{
    ShopInfo &si  = StashTrack.get_shop(shop.pos);
    const bool id = shoptype_identifies_stock(shop.type);
#ifdef USE_TILE_LOCAL
    TextItem* tmp = NULL;
#endif

    stock_order.clear();
    for (unsigned int i = 0; i < stock.size(); ++i)
        stock_order.push_back(i);
    stable_sort(stock_order.begin(), stock_order.end(),
                ShopSorter(shop, stock));
    for (unsigned int index = 0; index < stock_order.size(); ++index)
    {
        unsigned int i = stock_order[index];
        const item_def& item = mitm[stock[i]];
        const int gp_value = _shop_get_item_value(item, shop.greed, id);
        const bool can_afford = (you.gold >= gp_value);

        cgotoxy(1, index + 1, GOTO_CRT);
        const char c = index + 'a';

        // Colour stock as follows:
        //  * lightcyan, if on the shopping list.
        //  * lightred, if you can't buy all you selected.
        //  * lightgreen, if this item is purchasable along with your selections
        //  * red, if this item is not purchasable even by itself.
        //  * yellow, if this item would be purchasable if you deselected
        //            something else.

        // Is this too complicated? (jpeg)

        if (in_list[i])
            textcolour(LIGHTCYAN);
        else if (total_cost > you.gold && selected[i])
            textcolour(LIGHTRED);
        else if (gp_value <= you.gold - total_cost || selected[i] && can_afford)
            textcolour(LIGHTGREEN);
        else if (!can_afford)
            textcolour(RED);
        else
            textcolour(YELLOW);

        if (in_list[i])
            cprintf("%c $ ", c);
        else if (selected[i])
            cprintf("%c + ", c);
        else
            cprintf("%c - ", c);

        // Colour stock according to menu colours.
        const string colprf = item_prefix(item);
        const int col = menu_colour(item.name(DESC_A),
                                    colprf, "shop");
        textcolour(col != -1 ? col : LIGHTGREY);

        string item_name = item.name(DESC_A, false, id);
        if (shop_item_unknown(item))
            item_name += " (unknown)";

        cprintf("%4d gold   %s", gp_value, item_name.c_str());

        si.add_item(item, gp_value);

#ifdef USE_TILE_LOCAL
        const int cols = get_number_of_cols();
        tmp = new TextItem();
        tmp->set_highlight_colour(WHITE);
        tmp->set_text(""); // will print bounding box around formatted text
        tmp->set_bounds(coord_def(1 ,wherey()), coord_def(cols+2, wherey() + 1));
        tmp->add_hotkey(c);
        tmp->set_id(c);
        tmp->set_description_text(item_name);
        freeform->attach_item(tmp);
        tmp->set_visible(true);
#endif
    }
    textcolour(LIGHTGREY);
}

static int _count_identical(const vector<int>& stock, const item_def& item)
{
    int count = 0;
    for (unsigned int i = 0; i < stock.size(); i++)
    {
        const item_def &other = mitm[stock[i]];

        if (ShoppingList::items_are_same(item, other))
            count++;
    }
    return count;
}

//  Rather than prompting for each individual item, shopping now works more
//  like multi-pickup, in that pressing a letter only "selects" an item
//  (changing the '-' next to its name to a '+'). Affordability is shown
//  via colours that are updated every time the contents of your shopping
//  cart change.
//
//  New, suggested shopping keys:
//  * letter keys [a-t] (de)select item, as now
//  * Enter buys (with prompt), as now
//  * x exits (also Esc), as now
//  * ! toggles examination mode (where letter keys view items)
static bool _in_a_shop(int shopidx, int &num_in_list)
{
    const shop_struct& shop = env.shop[shopidx];

    unwind_bool in_shop(_in_shop_now, true);
    unwind_var<ordering_mode> default_order(shopping_order);

    cursor_control coff(false);

#ifdef USE_TILE_WEB
    tiles_crt_control menu(CRT_MENU, "shop");
#endif

    clrscr();

    const string hello = "Welcome to " + shop_name(shop.pos) + "!";
    bool first = true;
    int total_cost = 0;

    vector<int> stock = _shop_get_stock(shopidx);
    vector<bool> selected;
    vector<bool> in_list;

    const bool id_stock         = shoptype_identifies_stock(shop.type);
          bool bought_something = false;
          bool viewing          = false;
          bool first_iter       = true;

    // Store last_pickup in case we need to restore it.
    // Then clear it to fill with items picked up.
    map<int,int> tmp_l_p = you.last_pickup;
    you.last_pickup.clear();

    while (true)
    {
#ifdef USE_TILE_LOCAL
        PrecisionMenu menu;
        menu.set_select_type(PrecisionMenu::PRECISION_SINGLESELECT);
        MenuFreeform* freeform = new MenuFreeform();
        freeform->init(coord_def(1, 1),
                       coord_def(get_number_of_cols(), get_number_of_lines() + 1),
                       "freeform");
        menu.attach_object(freeform);
        menu.set_active_object(freeform);
        BoxMenuHighlighter* highlighter = new BoxMenuHighlighter(&menu);
        highlighter->init(coord_def(0,0), coord_def(0,0), "highlighter");
        menu.attach_object(highlighter);
#endif
        ASSERT(total_cost >= 0);

        StashTrack.get_shop(shop.pos).reset();

        stock = _shop_get_stock(shopidx);

        in_list.clear();
        in_list.resize(stock.size(), false);
        for (unsigned int i = 0; i < stock.size(); i++)
        {
            const item_def& item = mitm[stock[i]];
            in_list[i] = shopping_list.is_on_list(item);
        }

        // If items have been bought...
        if (stock.size() != selected.size())
        {
            total_cost = 0;
            selected.clear();
            selected.resize(stock.size(), false);
        }

        num_in_list  = 0;
        int num_selected = 0;
        for (unsigned int i = 0; i < stock.size(); i++)
        {
            if (in_list[i])
                num_in_list++;
            if (selected[i])
                num_selected++;
        }

        clrscr();
        if (stock.empty())
        {
            _shop_print("I'm sorry, my shop is empty now.", 1);
            _shop_more();
            return bought_something;
        }

        _shop_print_stock(stock, selected, in_list, shop, total_cost, viewing
#ifdef USE_TILE_LOCAL
                          , freeform
#endif
                          );
        _list_shop_keys(viewing, stock.size()
#ifdef USE_TILE_LOCAL
                        , freeform
#endif
                        );

        // Cull shopping list after shop contents have been displayed, but
        // only once.
        if (first_iter)
        {
            first_iter = false;

            unsigned int culled = 0;

            for (unsigned int i = 0; i < stock.size(); i++)
            {
                const item_def& item = mitm[stock[i]];
                const int cost = _shop_get_item_value(item, shop.greed,
                                                      id_stock);

                unsigned int num = shopping_list.cull_identical_items(item,
                                                                      cost);
                if (num > 0)
                {
                    in_list[i] = true;
                    num_in_list++;
                }
                culled += num;
            }
            if (culled > 0)
            {
                // Some shopping list items have been moved to this store,
                // so refresh the display.
                continue;
            }
        }

        if (!total_cost)
        {
            snprintf(info, INFO_SIZE, "You have %d gold piece%s.", you.gold,
                     you.gold != 1 ? "s" : "");

            textcolour(YELLOW);
        }
        else if (total_cost > you.gold)
        {
            snprintf(info, INFO_SIZE, "You have %d gold piece%s. "
                           "You are short %d gold piece%s for the purchase.",
                     you.gold,
                     you.gold != 1 ? "s" : "",
                     total_cost - you.gold,
                     (total_cost - you.gold != 1) ? "s" : "");

            textcolour(LIGHTRED);
        }
        else
        {
            snprintf(info, INFO_SIZE, "You have %d gold piece%s. "
                     "After the purchase, you will have %d gold piece%s.",
                     you.gold,
                     you.gold != 1 ? "s" : "",
                     you.gold - total_cost,
                     (you.gold - total_cost != 1) ? "s" : "");

            textcolour(YELLOW);
        }

        _shop_print(info, 0);

        if (first)
        {
            first = false;
            snprintf(info, INFO_SIZE, "%s What would you like to do? ",
                      hello.c_str());
        }
        else
            snprintf(info, INFO_SIZE, "What would you like to do? ");

        textcolour(CYAN);
        _shop_print(info, 1);

        textcolour(LIGHTGREY);

#ifdef USE_TILE_LOCAL
        //draw menu over the top of the prompt text
        tiles.get_crt()->attach_menu(&menu);
        freeform->set_visible(true);
        highlighter->set_visible(true);
        menu.draw_menu();

        int key = getch_ck();
        if (key != CK_ENTER && menu.process_key(key))
        {
            vector<MenuItem*> selection = menu.get_selected_items();
            if (selection.size() == 1)
                key = (int) selection.at(0)->get_id();
        }

#else
        mouse_control mc(MOUSE_MODE_PROMPT);
        int key = getchm();
#endif

        if (toalower(key) == 'x' || key_is_escape(key) || key == CK_MOUSE_CMD)
            break;
#ifdef USE_TILE_LOCAL
        else if (key == ' ' || key == CK_MOUSE_CLICK || key == CK_ENTER)
#else
        else if (key == '\r')
#endif
        {
            vector<bool> to_buy;
            int total_purchase = 0;

            if (num_selected == 0 && num_in_list > 0)
            {
                if (_shop_yesno("Buy items on shopping list? (Y/n)", 'y'))
                {
                    to_buy = in_list;

                    for (unsigned int i = 0; i < to_buy.size(); i++)
                    {
                        if (to_buy[i])
                        {
                            const item_def& item = mitm[stock[i]];

                            total_purchase +=
                                _shop_get_item_value(item, shop.greed,
                                                     id_stock);
                        }
                    }
                }
            }
            else
            {
                to_buy         = selected;
                total_purchase = total_cost;
            }

            // Do purchase.
            if (total_purchase > you.gold)
            {
                _shop_print("I'm sorry, you don't seem to have enough money.",
                            1);
                _shop_more();
            }
            else if (!total_purchase) // Nothing selected.
                continue;
            else
            {
                snprintf(info, INFO_SIZE, "Purchase for %d gold? (y/n)",
                         total_purchase);

                if (_shop_yesno(info, 'n'))
                {
                    int num_items = 0, outside_items = 0, quant;
                    for (int i = to_buy.size() - 1; i >= 0; --i)
                    {
                        if (to_buy[i])
                        {
                            item_def& item = mitm[stock[i]];

                            // Remove from shopping list if it's unique
                            // (i.e., if the shop has multiple scrolls of
                            // identify, don't remove the other scrolls
                            // from the shopping list if there's any
                            // left).
                            if (in_list[i]
                                && _count_identical(stock, item) == 1)
                            {
                                shopping_list.del_thing(item);
                            }

                            const int gp_value = _shop_get_item_value(item,
                                                        shop.greed, id_stock);

                            // Take a note of the purchase.
                            take_note(Note(NOTE_BUY_ITEM, gp_value, 0,
                                           item.name(DESC_A).c_str()));

                            // But take no further similar notes.
                            item.flags |= ISFLAG_NOTED_GET;

                            if (fully_identified(item))
                                item.flags |= ISFLAG_NOTED_ID;

                            quant = item.quantity;
                            num_items += quant;

                            if (!_purchase(shopidx, stock[i], gp_value,
                                           id_stock))
                            {
                                // The purchased item didn't fit into your
                                // knapsack.
                                outside_items += quant;
                            }
                        }
                    }

                    if (outside_items)
                    {
                        mprf("I'll put %s outside for you.",
                              num_items == 1             ? "it" :
                              num_items == outside_items ? "them"
                                                         : "some of them");
                    }
                    bought_something = true;
                }
            }
            //_shop_more();
            continue;
        }
        else if (key == '/')
            ++shopping_order;
        else if (key == '!' || key == '?')
        {
            // Toggle between browsing and shopping.
            viewing = !viewing;
        }
        else if (key == '$')
        {
            if (viewing || (num_selected == 0 && num_in_list == 0))
            {
                _shop_print("Huh?", 1);
                _shop_more();
                continue;
            }

            if (num_selected > 0)
            {
                // Move selected to shopping list.
                for (unsigned int i = 0; i < stock.size(); i++)
                {
                    const item_def &item = mitm[stock[i]];
                    if (selected[i])
                    {
                        if (!shopping_list.is_on_list(item))
                        {
                            const int cost = _shop_get_item_value(item,
                                        shop.greed, id_stock);
                            shopping_list.add_thing(item, cost);
                        }
                        in_list[i]  = true;
                        selected[i] = false;
                    }
                }
                total_cost = 0;
            }
            else
            {
                // Move shopping list to selected.
                for (unsigned int i = 0; i < stock.size(); i++)
                {
                    const item_def &item = mitm[stock[i]];
                    if (in_list[i])
                    {
                        in_list[i]  = false;
                        selected[i] = true;

                        total_cost += _shop_get_item_value(item, shop.greed,
                                                           id_stock);

                        if (shopping_list.is_on_list(item))
                            shopping_list.del_thing(item);
                    }
                }
            }
        }
        else if (!isaalpha(key))
        {
#ifdef TOUCH_UI
            // do nothing: this should be arrow key presses and the like
#else
            _shop_print("Huh?", 1);
            _shop_more();
#endif
        }
        else
        {
            // Uppercase means toggle the item on the shopping list
            // (unmarking for purchase if it was marked).
            bool to_shoplist = isaupper(key);
            key = toalower(key) - 'a';
            if (key >= static_cast<int>(stock.size()))
            {
                _shop_print("No such item.", 1);
                _shop_more();
                continue;
            }
            key = stock_order[key];

            item_def& item = mitm[stock[key]];
            if (viewing && !to_shoplist)
            {
                // A hack to make the description more useful.
                // In theory, the user could kill the process at this
                // point and end up with valid ID for the item.
                // That's not very useful, though, because it doesn't set
                // type-ID and once you can access the item (by buying it)
                // you have its full ID anyway. Worst case, it won't get
                // noted when you buy it.
                const uint64_t old_flags = item.flags;
                if (id_stock)
                {
                    item.flags |= (ISFLAG_IDENT_MASK | ISFLAG_NOTED_ID
                                   | ISFLAG_NOTED_GET);
                }
                describe_item(item, false, true);
                if (id_stock)
                    item.flags = old_flags;
            }
            else
            {
                const int gp_value = _shop_get_item_value(item, shop.greed,
                                                          id_stock);
                if (in_list[key])
                {
                    if (to_shoplist)
                    {
                        shopping_list.del_thing(item);
                        in_list[key] = false;
                        selected[key] = false;
                        continue;
                    }
                    else
                    {
                        shopping_list.del_thing(item);
                        in_list[key]  = false;
                        // Will be toggled to true later
                        selected[key] = false;
                    }
                }
                else if (to_shoplist)
                {
                    in_list[key] = true;
                    if (selected[key])
                        total_cost -= gp_value;
                    selected[key] = false;
                    const int cost = _shop_get_item_value(item, shop.greed,
                                                          id_stock);
                    shopping_list.add_thing(item, cost);
                    continue;
                }

                // Okay, we are now selecting an item normally, and it
                // is not on the shopping list.
                ASSERT(!in_list[key]);

                selected[key] = !selected[key];
                if (selected[key])
                    total_cost += gp_value;
                else
                    total_cost -= gp_value;

                ASSERT(total_cost >= 0);
            }
        }
    }

    if (you.last_pickup.empty())
        you.last_pickup = tmp_l_p;

    return bought_something;
}

bool shoptype_identifies_stock(shop_type type)
{
    return type != SHOP_WEAPON_ANTIQUE
           && type != SHOP_ARMOUR_ANTIQUE
           && type != SHOP_GENERAL_ANTIQUE;
}

static bool _purchase(int shop, int item_got, int cost, bool id)
{
    you.del_gold(cost);

    you.attribute[ATTR_PURCHASES] += cost;

    item_def& item = mitm[item_got];

    origin_purchased(item);

    if (id)
    {
        // Identify the item and its type.
        // This also takes the ID note if necessary.
        set_ident_type(item, ID_KNOWN_TYPE);
        set_ident_flags(item, ISFLAG_IDENT_MASK);
    }

    // Shopkeepers will now place goods you can't carry outside the shop.
    if (!move_item_to_inv(item_got, item.quantity, false))
    {
        move_item_to_grid(&item_got, env.shop[shop].pos);
        return false;
    }
    return true;
}

// This probably still needs some work.  Rings used to be the only
// artefacts which had a change in price, and that value corresponds
// to returning 50 from this function.  Good artefacts will probably
// be returning just over 30 right now.  Note that this isn't used
// as a multiple, its used in the old ring way: 7 * ret is added to
// the price of the artefact. -- bwr
int artefact_value(const item_def &item)
{
    ASSERT(is_artefact(item));

    int ret = 10;
    artefact_properties_t prop;
    artefact_wpn_properties(item, prop);

    // Brands are already accounted for via existing ego checks

    // This should probably be more complex... but this isn't so bad:
    ret += 6 * prop[ ARTP_AC ] + 6 * prop[ ARTP_EVASION ]
            + 6 * prop[ ARTP_SLAYING ]
            + 3 * prop[ ARTP_STRENGTH ] + 3 * prop[ ARTP_INTELLIGENCE ]
            + 3 * prop[ ARTP_DEXTERITY ];

    // These resistances have meaningful levels
    if (prop[ ARTP_FIRE ] > 0)
        ret += 5 + 5 * (prop[ ARTP_FIRE ] * prop[ ARTP_FIRE ]);
    else if (prop[ ARTP_FIRE ] < 0)
        ret -= 10;

    if (prop[ ARTP_COLD ] > 0)
        ret += 5 + 5 * (prop[ ARTP_COLD ] * prop[ ARTP_COLD ]);
    else if (prop[ ARTP_COLD ] < 0)
        ret -= 10;

    if (prop[ ARTP_MAGIC ] > 0)
        ret += 4 + 4 * prop[ ARTP_MAGIC ];
    else if (prop[ ARTP_MAGIC ] < 0)
        ret -= 6;

    if (prop[ ARTP_NEGATIVE_ENERGY ] > 0)
        ret += 3 + 3 * (prop[ARTP_NEGATIVE_ENERGY] * prop[ARTP_NEGATIVE_ENERGY]);

    // Discount Stlth-, charge for Stlth+
    ret += 2 * prop[ARTP_STEALTH];
    // Stlth+ costs more than Stlth- cheapens
    if (prop[ARTP_STEALTH] > 0)
        ret += 2 * prop[ARTP_STEALTH];

    // only one meaningful level:
    if (prop[ ARTP_POISON ])
        ret += 6;

    // only one meaningful level (hard to get):
    if (prop[ ARTP_ELECTRICITY ])
        ret += 10;

    // only one meaningful level (hard to get):
    if (prop[ ARTP_RCORR ])
        ret += 8;

    // only one meaningful level (hard to get):
    if (prop[ ARTP_RMUT ])
        ret += 8;

    if (prop[ ARTP_EYESIGHT ])
        ret += 6;

    // abilities:
    if (prop[ ARTP_FLY ])
        ret += 3;

    if (prop[ ARTP_BLINK ])
        ret += 10;

    if (prop[ ARTP_BERSERK ])
        ret += 5;

    if (prop[ ARTP_INVISIBLE ])
        ret += 10;

    if (prop[ ARTP_ANGRY ])
        ret -= 3;

    if (prop[ ARTP_CAUSE_TELEPORTATION ])
        ret -= 3;

    if (prop[ ARTP_NOISES ])
        ret -= 5;

    if (prop[ ARTP_PREVENT_TELEPORTATION ])
        ret -= 8;

    if (prop[ ARTP_PREVENT_SPELLCASTING ])
        ret -= 10;

    if (prop[ ARTP_MUTAGENIC ])
        ret -= 8;

    // extremely good
    if (prop[ ARTP_FOG ])
        ret += 50;

    if (prop[ ARTP_RMSL ])
        ret += 20;

    return (ret > 0) ? ret : 0;
}

unsigned int item_value(item_def item, bool ident)
{
    // Note that we pass item in by value, since we want a local
    // copy to mangle as necessary.
    item.flags = (ident) ? (item.flags | ISFLAG_IDENT_MASK) : (item.flags);

    if (is_unrandom_artefact(item)
        && item_ident(item, ISFLAG_KNOW_PROPERTIES))
    {
        const unrandart_entry *entry = get_unrand_entry(item.special);
        if (entry->value != 0)
            return entry->value;
    }

    int valued = 0;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        switch (item.sub_type)
        {

        case WPN_CLUB:
            valued += 10;
            break;

        case WPN_HUNTING_SLING:
        case WPN_STAFF:
            valued += 15;
            break;

        case WPN_GIANT_CLUB:
            valued += 17;
            break;

        case WPN_GIANT_SPIKED_CLUB:
            valued += 19;
            break;

        case WPN_DAGGER:
            valued += 20;
            break;

        case WPN_BLOWGUN:
        case WPN_HAMMER:
        case WPN_WHIP:
            valued += 25;
            break;

        case WPN_SHORTBOW:
        case WPN_HAND_AXE:
        case WPN_FALCHION:
        case WPN_MACE:
        case WPN_SCYTHE:
        case WPN_SHORT_SWORD:
        case WPN_SPEAR:
            valued += 30;
            break;

        case WPN_WAR_AXE:
        case WPN_FLAIL:
        case WPN_LONG_SWORD:
        case WPN_TRIDENT:
        case WPN_HAND_CROSSBOW:
            valued += 35;
            break;

        case WPN_BROAD_AXE:
        case WPN_RAPIER:
        case WPN_DIRE_FLAIL:
        case WPN_HALBERD:
        case WPN_MORNINGSTAR:
        case WPN_QUARTERSTAFF:
        case WPN_SCIMITAR:
            valued += 40;
            break;

        case WPN_LONGBOW:
        case WPN_ARBALEST:
            valued += 45;
            break;

        case WPN_BATTLEAXE:
        case WPN_GLAIVE:
        case WPN_GREAT_MACE:
        case WPN_GREAT_SWORD:
            valued += 65;
            break;

        case WPN_BARDICHE:
            valued += 90;
            break;

        case WPN_TRIPLE_CROSSBOW:
        case WPN_TRIPLE_SWORD:
        case WPN_EXECUTIONERS_AXE:
            valued += 100;
            break;

        case WPN_DOUBLE_SWORD:
        case WPN_DEMON_WHIP:
        case WPN_DEMON_TRIDENT:
        case WPN_DEMON_BLADE:
        case WPN_EVENINGSTAR:
        case WPN_LAJATANG:
        case WPN_QUICK_BLADE:
        case WPN_GREATSLING:
            valued += 150;
            break;

        case WPN_EUDEMON_BLADE:
        case WPN_SACRED_SCOURGE:
        case WPN_TRISHULA:
            valued += 200;
            break;
        }

        if (item_type_known(item))
        {
            switch (get_weapon_brand(item))
            {
            case SPWPN_NORMAL:
            default:            // randart
                valued *= 10;
                break;

            case SPWPN_SPEED:
            case SPWPN_VAMPIRISM:
                valued *= 30;
                break;

            case SPWPN_DISTORTION:
            case SPWPN_ELECTROCUTION:
            case SPWPN_PAIN:
                valued *= 25;
                break;

            case SPWPN_CHAOS:
            case SPWPN_DRAINING:
            case SPWPN_FLAMING:
            case SPWPN_FREEZING:
            case SPWPN_HOLY_WRATH:
                valued *= 18;
                break;

            case SPWPN_VORPAL:
                valued *= 15;
                break;

            case SPWPN_EVASION:
            case SPWPN_PROTECTION:
            case SPWPN_VENOM:
                valued *= 12;
                break;
            }

            valued /= 10;
        }

        if (item_ident(item, ISFLAG_KNOW_PLUSES))
            valued += 50 * item.plus;

        if (is_artefact(item))
        {
            if (item_type_known(item))
                valued += (7 * artefact_value(item));
            else
                valued += 50;
        }
        else if (item_type_known(item)
                 && get_equip_desc(item) != 0)
        {
            valued += 20;
        }

        if (item_known_cursed(item))
            valued -= 30;

        break;

    case OBJ_MISSILES:          // ammunition
        switch (item.sub_type)
        {
        case MI_STONE:
        case MI_NONE:
            valued++;
            break;
        case MI_NEEDLE:
        case MI_ARROW:
        case MI_BOLT:
            valued += 2;
            break;
        case MI_LARGE_ROCK:
            valued += 7;
            break;
        case MI_JAVELIN:
            valued += 8;
            break;
        case MI_THROWING_NET:
            valued += 30;
            break;
        default:
            valued += 5;
            break;
        }

        if (item_type_known(item))
        {
            switch (get_ammo_brand(item))
            {
            case SPMSL_NORMAL:
            default:
                valued *= 10;
                break;

            case SPMSL_CHAOS:
                valued *= 40;
                break;

            case SPMSL_CURARE:
            case SPMSL_PARALYSIS:
            case SPMSL_PENETRATION:
            case SPMSL_SILVER:
            case SPMSL_STEEL:
            case SPMSL_DISPERSAL:
                valued *= 30;
                break;

            case SPMSL_FLAME:
            case SPMSL_FROST:
            case SPMSL_SLEEP:
            case SPMSL_CONFUSION:
                valued *= 25;
                break;

            case SPMSL_EXPLODING:
            case SPMSL_POISONED:
            case SPMSL_RETURNING:
            case SPMSL_SLOW:
#if TAG_MAJOR_VERSION == 34
            case SPMSL_SICKNESS:
#endif
            case SPMSL_FRENZY:
                valued *= 20;
                break;
            }

            valued /= 10;
        }

        if (item_ident(item, ISFLAG_KNOW_PLUSES))
        {
            if (item.plus >= 0)
                valued += (item.plus * 2);

            if (item.plus < 0)
                valued += item.plus * item.plus * item.plus;
        }
        break;

    case OBJ_ARMOUR:
        switch (item.sub_type)
        {
        case ARM_PEARL_DRAGON_ARMOUR:
            valued += 1000;
            break;

        case ARM_PEARL_DRAGON_HIDE:
            valued += 900;
            break;

        case ARM_CRYSTAL_PLATE_ARMOUR:
        case ARM_GOLD_DRAGON_ARMOUR:
        case ARM_STORM_DRAGON_ARMOUR:
        case ARM_SHADOW_DRAGON_ARMOUR:
            valued += 800;
            break;

        case ARM_GOLD_DRAGON_HIDE:
        case ARM_STORM_DRAGON_HIDE:
        case ARM_SHADOW_DRAGON_HIDE:
            valued += 700;
            break;

        case ARM_FIRE_DRAGON_ARMOUR:
        case ARM_ICE_DRAGON_ARMOUR:
        case ARM_QUICKSILVER_DRAGON_ARMOUR:
            valued += 600;
            break;

        case ARM_FIRE_DRAGON_HIDE:
        case ARM_ICE_DRAGON_HIDE:
        case ARM_QUICKSILVER_DRAGON_HIDE:
        case ARM_SWAMP_DRAGON_ARMOUR:
            valued += 500;
            break;

        case ARM_MOTTLED_DRAGON_ARMOUR:
        case ARM_STEAM_DRAGON_ARMOUR:
        case ARM_SWAMP_DRAGON_HIDE:
            valued += 400;
            break;

        case ARM_MOTTLED_DRAGON_HIDE:
        case ARM_STEAM_DRAGON_HIDE:
            valued += 300;
            break;

        case ARM_CENTAUR_BARDING:
        case ARM_NAGA_BARDING:
        case ARM_PLATE_ARMOUR:
            valued += 230;
            break;

        case ARM_TROLL_LEATHER_ARMOUR:
            valued += 150;
            break;

        case ARM_CHAIN_MAIL:
        case ARM_HELMET:
#if TAG_MAJOR_VERSION == 34
        case ARM_CAP:
#endif
        case ARM_BOOTS:
        case ARM_GLOVES:
        case ARM_CLOAK:
        case ARM_LARGE_SHIELD:
        case ARM_SHIELD:
        case ARM_BUCKLER:
            valued += 45;
            break;

        case ARM_SCALE_MAIL:
        case ARM_TROLL_HIDE:
        case ARM_RING_MAIL:
        case ARM_HAT:
            valued += 40;
            break;

        case ARM_LEATHER_ARMOUR:
            valued += 20;
            break;

        case ARM_ROBE:
            valued += 7;
            break;

        case ARM_ANIMAL_SKIN:
            valued += 3;
            break;
        }

        if (item_type_known(item))
        {
            const int sparm = get_armour_ego_type(item);
            switch (sparm)
            {
            case SPARM_RUNNING:
            case SPARM_ARCHMAGI:
            case SPARM_RESISTANCE:
                valued += 250;
                break;

            case SPARM_COLD_RESISTANCE:
            case SPARM_DEXTERITY:
            case SPARM_FIRE_RESISTANCE:
            case SPARM_SEE_INVISIBLE:
            case SPARM_INTELLIGENCE:
            case SPARM_FLYING:
            case SPARM_STEALTH:
            case SPARM_STRENGTH:
            case SPARM_INVISIBILITY:
            case SPARM_MAGIC_RESISTANCE:
            case SPARM_PROTECTION:
                valued += 50;
                break;

            case SPARM_POSITIVE_ENERGY:
            case SPARM_POISON_RESISTANCE:
            case SPARM_REFLECTION:
                valued += 20;
                break;

            case SPARM_PONDEROUSNESS:
                valued -= 250;
                break;
            }
        }

        if (item_ident(item, ISFLAG_KNOW_PLUSES))
            valued += 50 * item.plus;

        if (is_artefact(item))
        {
            if (item_type_known(item))
                valued += (7 * artefact_value(item));
            else
                valued += 50;
        }
        else if (item_type_known(item) && get_equip_desc(item) != 0)
            valued += 20;

        if (item_known_cursed(item))
            valued -= 30;

        break;

    case OBJ_WANDS:
        if (!item_type_known(item))
            valued += 40;
        else
        {
            // true if the wand is of a good type, a type with significant
            // inherent value even when empty. Good wands are less expensive
            // per charge.
            bool good = false;
            switch (item.sub_type)
            {
            case WAND_HASTING:
            case WAND_HEAL_WOUNDS:
                valued += 240;
                good = true;
                break;

            case WAND_TELEPORTATION:
                valued += 120;
                good = true;
                break;

            case WAND_COLD:
            case WAND_FIRE:
            case WAND_FIREBALL:
            case WAND_DIGGING:
                valued += 80;
                good = true;
                break;

            case WAND_INVISIBILITY:
            case WAND_DRAINING:
            case WAND_LIGHTNING:
            case WAND_DISINTEGRATION:
                valued += 40;
                good = true;
                break;

            case WAND_ENSLAVEMENT:
            case WAND_POLYMORPH:
            case WAND_PARALYSIS:
                valued += 20;
                break;

            case WAND_CONFUSION:
            case WAND_SLOWING:
                valued += 15;
                break;

            case WAND_FLAME:
            case WAND_FROST:
            case WAND_RANDOM_EFFECTS:
                valued += 10;
                break;

            case WAND_MAGIC_DARTS:
            default:
                valued += 6;
                break;
            }

            if (item_ident(item, ISFLAG_KNOW_PLUSES))
            {
                if (good) valued += (valued * item.plus) / 4;
                else      valued += (valued * item.plus) / 2;
            }
        }
        break;

    case OBJ_POTIONS:
        if (!item_type_known(item))
            valued += 9;
        else
        {
            switch (item.sub_type)
            {
            case POT_EXPERIENCE:
                valued += 500;
                break;

            case POT_CURE_MUTATION:
#if TAG_MAJOR_VERSION == 34
            case POT_GAIN_DEXTERITY:
            case POT_GAIN_INTELLIGENCE:
            case POT_GAIN_STRENGTH:
#endif
            case POT_BENEFICIAL_MUTATION:
                valued += 350;
                break;

            case POT_RESISTANCE:
            case POT_HASTE:
                valued += 70;
                break;

            case POT_MAGIC:
            case POT_INVISIBILITY:
            case POT_CANCELLATION:
                valued += 55;
                break;

            case POT_BERSERK_RAGE:
            case POT_HEAL_WOUNDS:
            case POT_RESTORE_ABILITIES:
            case POT_FLIGHT:
                valued += 30;
                break;

            case POT_MIGHT:
            case POT_AGILITY:
            case POT_BRILLIANCE:
            case POT_MUTATION:
                valued += 25;
                break;

            case POT_CURING:
            case POT_DECAY:
            case POT_DEGENERATION:
#if TAG_MAJOR_VERSION == 34
            case POT_STRONG_POISON:
#endif
            case POT_LIGNIFY:
                valued += 20;
                break;

            case POT_BLOOD:
            case POT_PORRIDGE:
            case POT_CONFUSION:
            case POT_POISON:
            case POT_SLOWING:
                valued += 10;
                break;

#if TAG_MAJOR_VERSION == 34
            case POT_BLOOD_COAGULATED:
                valued += 5;
                break;
#endif
            }
        }
        break;

    case OBJ_FOOD:
        switch (item.sub_type)
        {
        case FOOD_MEAT_RATION:
        case FOOD_BREAD_RATION:
            valued = 50;
            break;

        case FOOD_ROYAL_JELLY:
            valued = 20;
            break;

        case FOOD_BEEF_JERKY:
        case FOOD_PIZZA:
        case FOOD_FRUIT:
            valued = 15;
            break;

        case FOOD_CHUNK:
        default:
                break;
        }
        break;

    case OBJ_SCROLLS:
        if (!item_type_known(item))
            valued += 10;
        else
        {
            switch (item.sub_type)
            {
            case SCR_ACQUIREMENT:
                valued += 520;
                break;

            case SCR_BRAND_WEAPON:
                valued += 200;
                break;

            case SCR_RECHARGING:
            case SCR_SUMMONING:
                valued += 95;
                break;

            case SCR_BLINKING:
            case SCR_ENCHANT_ARMOUR:
            case SCR_ENCHANT_WEAPON:
            case SCR_TORMENT:
            case SCR_HOLY_WORD:
            case SCR_SILENCE:
            case SCR_VULNERABILITY:
                valued += 75;
                break;

            case SCR_AMNESIA:
            case SCR_FEAR:
            case SCR_IMMOLATION:
            case SCR_MAGIC_MAPPING:
                valued += 35;
                break;

            case SCR_REMOVE_CURSE:
            case SCR_TELEPORTATION:
                valued += 30;
                break;

            case SCR_FOG:
            case SCR_IDENTIFY:
            case SCR_CURSE_ARMOUR:
            case SCR_CURSE_WEAPON:
            case SCR_CURSE_JEWELLERY:
                valued += 20;
                break;

            case SCR_NOISE:
            case SCR_RANDOM_USELESSNESS:
                valued += 10;
                break;
            }
        }
        break;

    case OBJ_JEWELLERY:
        if (item_known_cursed(item))
            valued -= 30;

        if (!item_type_known(item))
            valued += 50;
        else
        {
            // Variable-strength rings.
            if (item_ident(item, ISFLAG_KNOW_PLUSES)
                && (item.sub_type == RING_PROTECTION
                    || item.sub_type == RING_STRENGTH
                    || item.sub_type == RING_EVASION
                    || item.sub_type == RING_DEXTERITY
                    || item.sub_type == RING_INTELLIGENCE
                    || item.sub_type == RING_SLAYING))
            {
                // Formula: price = kn(n+1) / 2, where k depends on the subtype,
                // n is the power. (The base variable is equal to 2n.)
                int base = 0;
                int coefficient = 0;
                if (item.sub_type == RING_SLAYING)
                    base = 3 * item.plus;
                else
                    base = 2 * item.plus;

                switch (item.sub_type)
                {
                case RING_SLAYING:
                case RING_PROTECTION:
                case RING_EVASION:
                    coefficient = 40;
                    break;
                case RING_STRENGTH:
                case RING_DEXTERITY:
                case RING_INTELLIGENCE:
                    coefficient = 30;
                    break;
                default:
                    break;
                }

                if (base <= 0)
                    valued += 25 * base;
                else
                    valued += (coefficient * base * (base + 1)) / 8;
            }
            else
            {
                switch (item.sub_type)
                {
                case RING_TELEPORT_CONTROL:
                    valued += 500;
                    break;

                case AMU_FAITH:
                case AMU_RESIST_MUTATION:
                case AMU_RAGE:
                    valued += 400;
                    break;

                case RING_INVISIBILITY:
                case RING_WIZARDRY:
                case AMU_REGENERATION:
                case AMU_GUARDIAN_SPIRIT:
                case AMU_THE_GOURMAND:
                    valued += 300;
                    break;

                case RING_FIRE:
                case RING_ICE:
                case RING_PROTECTION_FROM_COLD:
                case RING_PROTECTION_FROM_FIRE:
                case RING_PROTECTION_FROM_MAGIC:
                    valued += 250;
                    break;

                case RING_MAGICAL_POWER:
                case RING_LIFE_PROTECTION:
                case RING_POISON_RESISTANCE:
                case AMU_CLARITY:
                case AMU_RESIST_CORROSION:
                    valued += 200;
                    break;

                case RING_SUSTAIN_ABILITIES:
                case RING_STEALTH:
                case RING_TELEPORTATION:
                case RING_FLIGHT:
                case AMU_STASIS:
                    valued += 175;
                    break;

                case RING_SEE_INVISIBLE:
                case AMU_WARDING:
                    valued += 150;
                    break;

                case RING_LOUDNESS:
                    valued += 75;
                    break;

                case AMU_INACCURACY:
                    valued -= 300;
                    break;
                    // got to do delusion!
                }
            }

            if (is_artefact(item))
            {
                // in this branch we're guaranteed to know
                // the item type!
                if (valued < 0)
                    valued = (artefact_value(item) - 5) * 7;
                else
                    valued += artefact_value(item) * 7;
            }

            // Hard minimum, as it's worth 20 to ID a ring.
            valued = max(20, valued);
        }
        break;

    case OBJ_MISCELLANY:
        switch (item.sub_type)
        {
        case MISC_RUNE_OF_ZOT:  // upped from 1200 to encourage collecting
            valued += 10000;
            break;

        case MISC_HORN_OF_GERYON:
            valued += 5000;
            break;

        case MISC_FAN_OF_GALES:
        case MISC_STONE_OF_TREMORS:
        case MISC_PHIAL_OF_FLOODS:
        case MISC_LAMP_OF_FIRE:
            valued += 400;
            break;

        case MISC_PHANTOM_MIRROR:
            valued += 300;
            break;

        case MISC_BOX_OF_BEASTS:
        case MISC_DISC_OF_STORMS:
        case MISC_SACK_OF_SPIDERS:
            valued += 200;
            break;

        default:
            if (is_deck(item))
                valued += 80 + item.special * 60;
            else
                valued += 200;
        }
        break;

    case OBJ_BOOKS:
        valued = 150;
        if (item.sub_type == BOOK_DESTRUCTION)
            break;

        if (item_type_known(item))
        {
            double rarity = 0;
            if (is_random_artefact(item))
            {
                // Consider spellbook as rare as the average of its
                // three rarest spells.
                int rarities[SPELLBOOK_SIZE];
                int count_valid = 0;
                for (int i = 0; i < SPELLBOOK_SIZE; i++)
                {
                    spell_type spell = which_spell_in_book(item, i);
                    if (spell == SPELL_NO_SPELL)
                    {
                        rarities[i] = 0;
                        continue;
                    }

                    rarities[i] = spell_rarity(spell);
                    count_valid++;
                }
                ASSERT(count_valid > 0);

                if (count_valid > 3)
                    count_valid = 3;

                sort(rarities, rarities + SPELLBOOK_SIZE);
                for (int i = SPELLBOOK_SIZE - 1;
                     i >= SPELLBOOK_SIZE - count_valid; i--)
                {
                    rarity += rarities[i];
                }

                rarity /= count_valid;

                // Fixed level randarts get a bonus for the really low and
                // really high level spells.
                if (item.sub_type == BOOK_RANDART_LEVEL)
                    valued += 50 * abs(5 - item.plus);
            }
            else
                rarity = book_rarity(item.sub_type);

            valued += (int)(rarity * 50.0);
        }
        break;

    case OBJ_STAVES:
        valued = item_type_known(item) ? 250 : 120;
        break;

    case OBJ_RODS:
        if (!item_type_known(item))
            valued = 120;
        else if (item.sub_type == ROD_STRIKING)
            valued = 150;
        else
            valued = 250;

        // Both max charges and enchantment.
        if (item_ident(item, ISFLAG_KNOW_PLUSES))
            valued += 50 * (item.charge_cap / ROD_CHARGE_MULT + item.rod_plus);
        break;

    case OBJ_ORBS:
        valued = 250000;
        break;

    default:
        break;
    }                           // end switch

    if (valued < 1)
        valued = 1;

    valued = stepdown_value(valued, 1000, 1000, 10000, 10000);

    return item.quantity * valued;
}

bool is_worthless_consumable(const item_def &item)
{
    switch (item.base_type)
    {
    case OBJ_POTIONS:
        switch (item.sub_type)
        {
        // Blood potions are worthless because they are easy to make.
        case POT_BLOOD:
#if TAG_MAJOR_VERSION == 34
        case POT_BLOOD_COAGULATED:
#endif
        case POT_CONFUSION:
        case POT_DECAY:
        case POT_DEGENERATION:
        case POT_POISON:
        case POT_SLOWING:
            return true;
        default:
            return false;
        }
    case OBJ_SCROLLS:
        switch (item.sub_type)
        {
        case SCR_CURSE_ARMOUR:
        case SCR_CURSE_WEAPON:
        case SCR_CURSE_JEWELLERY:
        case SCR_NOISE:
        case SCR_RANDOM_USELESSNESS:
            return true;
        default:
            return false;
        }

    // Only consumables are worthless.
    default:
        return false;
    }
}

static void _delete_shop(int i)
{
    grd(you.pos()) = DNGN_ABANDONED_SHOP;
    unnotice_feature(level_pos(level_id::current(), you.pos()));
}

void shop()
{
    int i;

    for (i = 0; i < MAX_SHOPS; i++)
        if (env.shop[i].pos == you.pos())
            break;

    if (i == MAX_SHOPS)
    {
        mprf(MSGCH_ERROR, "Help! Non-existent shop.");
        return;
    }

    // Quick out, if no inventory
    if (_shop_get_stock(i).empty())
    {
        const shop_struct& shop = env.shop[i];
        mprf("%s appears to be closed.", shop_name(shop.pos).c_str());
        _delete_shop(i);
        return;
    }

          int  num_in_list      = 0;
    const bool bought_something = _in_a_shop(i, num_in_list);
    const string shopname       = shop_name(env.shop[i].pos);

    // If the shop is now empty, erase it from the overview.
    if (_shop_get_stock(i).empty())
        _delete_shop(i);

    redraw_screen();

    if (bought_something)
        mprf("Thank you for shopping at %s!", shopname.c_str());

    if (num_in_list > 0)
        mpr("You can access your shopping list by pressing '$'.");
}

void destroy_shop_at(coord_def p)
{
    if (shop_struct *shop = get_shop(p))
    {
        unnotice_feature(level_pos(level_id::current(), shop->pos));

        shop->pos  = coord_def(0, 0);
        shop->type = SHOP_UNASSIGNED;
    }
}

shop_struct *get_shop(const coord_def& where)
{
    if (grd(where) != DNGN_ENTER_SHOP)
        return NULL;

    unsigned short t = env.tgrid(where);
    ASSERT(t != NON_ENTITY);
    ASSERT(t < MAX_SHOPS);
    ASSERT(env.shop[t].pos == where);
    ASSERT(env.shop[t].type != SHOP_UNASSIGNED);

    return &env.shop[t];
}

string shop_name(const coord_def& where, bool add_stop)
{
    string name(shop_name(where));
    if (add_stop)
        name += ".";
    return name;
}

string shop_type_name(shop_type type)
{
    switch (type)
    {
        case SHOP_WEAPON_ANTIQUE:
            return "Antique Weapon";
        case SHOP_ARMOUR_ANTIQUE:
            return "Antique Armour";
        case SHOP_WEAPON:
            return "Weapon";
        case SHOP_ARMOUR:
            return "Armour";
        case SHOP_JEWELLERY:
            return "Jewellery";
        case SHOP_EVOKABLES:
            return "Gadget";
        case SHOP_BOOK:
            return "Book";
        case SHOP_FOOD:
            return "Food";
        case SHOP_SCROLL:
            return "Magic Scroll";
        case SHOP_GENERAL_ANTIQUE:
            return "Assorted Antiques";
        case SHOP_DISTILLERY:
            return "Distillery";
        case SHOP_GENERAL:
            return "General Store";
        default:
            return "Bug";
    }
}

static string _shop_type_suffix(shop_type type, const coord_def &where)
{
    if (type == SHOP_GENERAL
        || type == SHOP_GENERAL_ANTIQUE
        || type == SHOP_DISTILLERY)
    {
        return "";
    }

    const char* suffixnames[] = {"Shoppe", "Boutique", "Emporium", "Shop"};
    const int temp = (where.x + where.y) % 4;

    return string(suffixnames[temp]);
}

string shop_name(const coord_def& where)
{
    const shop_struct *cshop = get_shop(where);

    // paranoia
    ASSERT(grd(where) == DNGN_ENTER_SHOP);

    if (!cshop)
    {
        mpr("Help! Non-existent shop.");
        return "Buggy Shop";
    }

    const shop_type type = cshop->type;

    string sh_name = "";

    if (!cshop->shop_name.empty())
        sh_name += apostrophise(cshop->shop_name) + " ";
    else
    {
        uint32_t seed = static_cast<uint32_t>(cshop->keeper_name[0])
            | (static_cast<uint32_t>(cshop->keeper_name[1]) << 8)
            | (static_cast<uint32_t>(cshop->keeper_name[1]) << 16);

        sh_name += apostrophise(make_name(seed, false)) + " ";
    }

    if (!cshop->shop_type_name.empty())
        sh_name += cshop->shop_type_name;
    else
        sh_name += shop_type_name(type);

    if (!cshop->shop_suffix_name.empty())
        sh_name += " " + cshop->shop_suffix_name;
    else
    {
        string sh_suffix = _shop_type_suffix(type, where);
        if (!sh_suffix.empty())
            sh_name += " " + sh_suffix;
    }

    return sh_name;
}

bool is_shop_item(const item_def &item)
{
    return item.pos.x == 0 && item.pos.y >= 5 && item.pos.y < (MAX_SHOPS + 5);
}

bool shop_item_unknown(const item_def &item)
{
    return item_type_has_ids(item.base_type)
           && item_type_known(item)
           && get_ident_type(item) != ID_KNOWN_TYPE
           && !is_artefact(item);
}

////////////////////////////////////////////////////////////////////////

// TODO:
//   * Let shopping list be modified from with the stash lister.
//   * Warn if buying something not on the shopping list would put
//     something on shopping list out of your reach.

#define SHOPPING_LIST_KEY       "shopping_list_key"
#define SHOPPING_THING_COST_KEY "cost_key"
#define SHOPPING_THING_ITEM_KEY "item_key"
#define SHOPPING_THING_DESC_KEY "desc_key"
#define SHOPPING_THING_VERB_KEY "verb_key"
#define SHOPPING_THING_POS_KEY  "pos_key"

ShoppingList::ShoppingList()
{
}

#define SETUP_POS()                 \
    ASSERT(list); \
    level_pos pos;                  \
    if (_pos != NULL)               \
        pos = *_pos;                \
    else                            \
        pos = level_pos::current(); \
    ASSERT(pos.is_valid());

#define SETUP_THING()                             \
    CrawlHashTable *thing = new CrawlHashTable();  \
    (*thing)[SHOPPING_THING_COST_KEY] = cost; \
    (*thing)[SHOPPING_THING_POS_KEY]  = pos;

bool ShoppingList::add_thing(const item_def &item, int cost,
                             const level_pos* _pos)
{
    ASSERT(item.defined());
    ASSERT(cost > 0);

    SETUP_POS();

    if (find_thing(item, pos) != -1)
    {
        mprf(MSGCH_ERROR, "%s is already on the shopping list.",
             item.name(DESC_THE).c_str());
        return false;
    }

    SETUP_THING();
    (*thing)[SHOPPING_THING_ITEM_KEY] = item;
    list->push_back(*thing);
    refresh();

    return true;
}

bool ShoppingList::add_thing(string desc, string buy_verb, int cost,
                             const level_pos* _pos)
{
    ASSERT(!desc.empty());
    ASSERT(!buy_verb.empty());
    ASSERT(cost > 0);

    SETUP_POS();

    if (find_thing(desc, pos) != -1)
    {
        mprf(MSGCH_ERROR, "%s is already on the shopping list.",
             desc.c_str());
        return false;
    }

    SETUP_THING();
    (*thing)[SHOPPING_THING_DESC_KEY] = desc;
    (*thing)[SHOPPING_THING_VERB_KEY] = buy_verb;
    list->push_back(*thing);
    refresh();

    return true;
}

#undef SETUP_THING

bool ShoppingList::is_on_list(const item_def &item, const level_pos* _pos) const
{
    SETUP_POS();

    return find_thing(item, pos) != -1;
}

bool ShoppingList::is_on_list(string desc, const level_pos* _pos) const
{
    SETUP_POS();

    return find_thing(desc, pos) != -1;
}

void ShoppingList::del_thing_at_index(int idx)
{
    ASSERT_RANGE(idx, 0, list->size());
    list->erase(idx);
    refresh();
}

void ShoppingList::del_things_from(const level_id &lid)
{
    for (unsigned int i = 0; i < list->size(); i++)
    {
        const CrawlHashTable &thing = (*list)[i];

        if (thing_pos(thing).is_on(lid))
            list->erase(i--);
    }
    refresh();
}

bool ShoppingList::del_thing(const item_def &item,
                             const level_pos* _pos)
{
    SETUP_POS();

    int idx = find_thing(item, pos);

    if (idx == -1)
    {
        mprf(MSGCH_ERROR, "%s isn't on shopping list, can't delete it.",
             item.name(DESC_THE).c_str());
        return false;
    }

    del_thing_at_index(idx);
    return true;
}

bool ShoppingList::del_thing(string desc, const level_pos* _pos)
{
    SETUP_POS();

    int idx = find_thing(desc, pos);

    if (idx == -1)
    {
        mprf(MSGCH_ERROR, "%s isn't on shopping list, can't delete it.",
             desc.c_str());
        return false;
    }

    del_thing_at_index(idx);
    return true;
}

#undef SETUP_POS

#define REMOVE_PROMPTED_KEY  "remove_prompted_key"
#define REPLACE_PROMPTED_KEY "replace_prompted_key"

// TODO:
//
// * If you get a randart which lets you turn invisible, then remove
//   any ordinary rings of invisibility from the shopping list.
//
// * If you collected enough spellbooks that all the spells in a
//   shopping list book are covered, then auto-remove it.
unsigned int ShoppingList::cull_identical_items(const item_def& item,
                                                int cost)
{
    // Dead men can't update their shopping lists.
    if (!crawl_state.need_save)
        return 0;

    // Can't put items in Bazaar shops in the shopping list, so
    // don't bother transferring shopping list items to Bazaar shops.
    if (cost != -1 && !_can_shoplist())
        return 0;

    switch (item.base_type)
    {
    case OBJ_JEWELLERY:
    case OBJ_BOOKS:
    case OBJ_STAVES:
        // Only these are really interchangeable.
        break;
    case OBJ_MISCELLANY:
        // ... and a few of these.
        switch (item.sub_type)
        {
            case MISC_LANTERN_OF_SHADOWS:
            case MISC_CRYSTAL_BALL_OF_ENERGY:
            case MISC_DISC_OF_STORMS:
                break;
            default:
                return 0;
        }
        break;
    default:
        return 0;
    }

    if (!item_type_known(item) || is_artefact(item))
        return 0;

    // Ignore stat-modification rings which reduce a stat, since they're
    // worthless.
    if (item.base_type == OBJ_JEWELLERY && item.plus < 0)
        return 0;

    // Manuals are consumable, and interesting enough to keep on list.
    if (item.base_type == OBJ_BOOKS && item.sub_type == BOOK_MANUAL)
        return 0;

    // Item is already on shopping-list.
    const bool on_list = find_thing(item, level_pos::current()) != -1;

    const bool do_prompt = item.base_type == OBJ_JEWELLERY
                           && !jewellery_is_amulet(item)
                           && ring_has_stackable_effect(item);

    bool add_item = false;

    typedef pair<item_def, level_pos> list_pair;
    vector<list_pair> to_del;

    // NOTE: Don't modify the shopping list while iterating over it.
    for (unsigned int i = 0; i < list->size(); i++)
    {
        CrawlHashTable &thing = (*list)[i];

        if (!thing_is_item(thing))
            continue;

        const item_def& list_item = get_thing_item(thing);

        if (list_item.base_type != item.base_type
            || list_item.sub_type != item.sub_type)
        {
            continue;
        }

        if (!item_type_known(list_item) || is_artefact(list_item))
            continue;

        // Don't prompt to remove rings with strictly better pluses
        // than the new one.  Also, don't prompt to remove rings with
        // known pluses when the new ring's pluses are unknown.
        if (item.base_type == OBJ_JEWELLERY)
        {
            const bool has_plus = ring_has_pluses(item);
            const int delta_p = item.plus - list_item.plus;
            if (has_plus
                && item_ident(list_item, ISFLAG_KNOW_PLUSES)
                && (!item_ident(item, ISFLAG_KNOW_PLUSES)
                     || delta_p < 0))
            {
                continue;
            }
        }

        // Don't prompt to remove known manuals when the new one is unknown
        // or for a different skill.
        if (item.base_type == OBJ_BOOKS && item.sub_type == BOOK_MANUAL
            && item_type_known(list_item)
            && (!item_type_known(item) || item.plus != list_item.plus))
        {
            continue;
        }

        list_pair listed(list_item, thing_pos(thing));

        // cost = -1, we just found a shop item which is cheaper than
        // one on the shopping list.
        if (cost != -1)
        {
            int list_cost = thing_cost(thing);

            if (cost >= list_cost)
                continue;

            // Only prompt once.
            if (thing.exists(REPLACE_PROMPTED_KEY))
                continue;
            thing[REPLACE_PROMPTED_KEY] = (bool) true;

            string prompt =
                make_stringf("Shopping-list: replace %dgp %s with cheaper "
                             "one? (Y/n)", list_cost,
                             describe_thing(thing).c_str());

            if (_shop_yesno(prompt.c_str(), 'y'))
            {
                add_item = true;
                to_del.push_back(listed);
            }
            continue;
        }

        // cost == -1, we just got an item which is on the shopping list.
        if (do_prompt)
        {
            // Only prompt once.
            if (thing.exists(REMOVE_PROMPTED_KEY))
                continue;
            thing[REMOVE_PROMPTED_KEY] = (bool) true;

            string prompt = make_stringf("Shopping-list: remove %s? (Y/n)",
                                         describe_thing(thing, DESC_A).c_str());

            if (_shop_yesno(prompt.c_str(), 'y'))
            {
                to_del.push_back(listed);
                if (!_in_shop_now)
                {
                    mprf("Shopping-list: removing %s",
                         describe_thing(thing, DESC_A).c_str());
                }
            }
            else if (!_in_shop_now)
                canned_msg(MSG_OK);
        }
        else
        {
            string str = make_stringf("Shopping-list: removing %s",
                                      describe_thing(thing, DESC_A).c_str());

            _shop_mpr(str.c_str());
            to_del.push_back(listed);
        }
    }

    for (unsigned int i = 0; i < to_del.size(); i++)
        del_thing(to_del[i].first, &to_del[i].second);

    if (add_item && !on_list)
        add_thing(item, cost);

    return to_del.size();
}

void ShoppingList::item_type_identified(object_class_type base_type,
                                        int sub_type)
{
    // Dead men can't update their shopping lists.
    if (!crawl_state.need_save)
        return;

    // Only restore the excursion at the very end.
    level_excursion le;

    for (unsigned int i = 0; i < list->size(); i++)
    {
        CrawlHashTable &thing = (*list)[i];

        if (!thing_is_item(thing))
            continue;

        const item_def& item = get_thing_item(thing);

        if (item.base_type != base_type || item.sub_type != sub_type)
            continue;

        const level_pos place = thing_pos(thing);

        le.go_to(place.id);
        const shop_struct *shop = get_shop(place.pos);
        ASSERT(shop);
        if (shoptype_identifies_stock(shop->type))
            continue;

        thing[SHOPPING_THING_COST_KEY] =
            _shop_get_item_value(item, shop->greed, false);
    }

    // Prices could have changed.
    refresh();
}

int ShoppingList::size() const
{
    ASSERT(list);

    return list->size();
}

bool ShoppingList::items_are_same(const item_def& item_a,
                                  const item_def& item_b)
{
    return item_name_simple(item_a) == item_name_simple(item_b);
}

void ShoppingList::move_things(const coord_def &_src, const coord_def &_dst)
{
    if (crawl_state.map_stat_gen
        || crawl_state.obj_stat_gen
        || crawl_state.test)
    {
        return; // Shopping list is unitialized and uneeded.
    }

    const level_pos src(level_id::current(), _src);
    const level_pos dst(level_id::current(), _dst);

    for (unsigned int i = 0; i < list->size(); i++)
    {
        CrawlHashTable &thing = (*list)[i];

        if (thing_pos(thing) == src)
            thing[SHOPPING_THING_POS_KEY] = dst;
    }
}

void ShoppingList::forget_pos(const level_pos &pos)
{
    if (!crawl_state.need_save)
        return; // Shopping list is uninitialized and unneeded.

    for (unsigned int i = 0; i < list->size(); i++)
    {
        const CrawlHashTable &thing = (*list)[i];

        if (thing_pos(thing) == pos)
        {
            list->erase(i);
            i--;
        }
    }

    // Maybe we just forgot about a shop.
    refresh();
}

void ShoppingList::gold_changed(int old_amount, int new_amount)
{
    ASSERT(list);

    if (new_amount > old_amount && new_amount >= min_unbuyable_cost)
    {
        ASSERT(min_unbuyable_idx < list->size());

        vector<string> descs;
        for (unsigned int i = min_unbuyable_idx; i < list->size(); i++)
        {
            const CrawlHashTable &thing = (*list)[i];
            const int            cost   = thing_cost(thing);

            if (cost > new_amount)
            {
                ASSERT(i > (unsigned int) min_unbuyable_idx);
                break;
            }

            string desc;

            if (thing.exists(SHOPPING_THING_VERB_KEY))
                desc += thing[SHOPPING_THING_VERB_KEY].get_string();
            else
                desc = "buy";
            desc += " ";

            desc += describe_thing(thing, DESC_A);

            descs.push_back(desc);
        }
        ASSERT(!descs.empty());

        mpr_comma_separated_list("You now have enough gold to ", descs,
                                 ", or ");
        mpr("You can access your shopping list by pressing '$'.");

        // Our gold has changed, maybe we can buy different things now.
        refresh();
    }
    else if (new_amount < old_amount && new_amount < max_buyable_cost)
    {
        // As above.
        refresh();
    }
}

class ShoppingListMenu : public Menu
{
public:
    ShoppingListMenu()
#ifdef USE_TILE_LOCAL
        : Menu(MF_MULTISELECT,"",false)
#else
        : Menu()
#endif
    { }

protected:
    void draw_title();
};

void ShoppingListMenu::draw_title()
{
    if (title)
    {
        const int total_cost = you.props[SHOPPING_LIST_COST_KEY];

        cgotoxy(1, 1);
        formatted_string fs = formatted_string(title->colour);
        fs.cprintf("%d %s%s, total cost %d gp",
                   title->quantity, title->text.c_str(),
                   title->quantity > 1? "s" : "",
                   total_cost);
        fs.display();

#ifdef USE_TILE_WEB
        webtiles_set_title(fs);
#endif
        string s = "<lightgrey>  [<w>a-z</w>] ";

        switch (menu_action)
        {
        case ACT_EXECUTE:
            s += "<w>travel</w>|examine|delete";
            break;
        case ACT_EXAMINE:
            s += "travel|<w>examine</w>|delete";
            break;
        default:
            s += "travel|examine|<w>delete</w>";
            break;
        }

        s += "  [<w>?</w>/<w>!</w>] change action</lightgrey>";

        draw_title_suffix(formatted_string::parse_string(s), false);
    }
}

void ShoppingList::fill_out_menu(Menu& shopmenu)
{
    menu_letter hotkey;
    for (unsigned i = 0; i < list->size(); ++i, ++hotkey)
    {
        CrawlHashTable &thing  = (*list)[i];
        level_pos      pos     = thing_pos(thing);
        int            cost    = thing_cost(thing);
        bool           unknown = false;

        if (thing_is_item(thing))
            unknown = shop_item_unknown(get_thing_item(thing));

        string etitle =
            make_stringf("[%s] %s%s (%d gp)", pos.id.describe().c_str(),
                         name_thing(thing, DESC_A).c_str(),
                         unknown ? " (unknown)" : "", cost);

        MenuEntry *me = new MenuEntry(etitle, MEL_ITEM, 1, hotkey);
        me->data = &thing;

        if (cost > you.gold)
            me->colour = DARKGREY;
        else if (thing_is_item(thing))
        {
            // Colour shopping list item according to menu colours.
            const item_def &item = get_thing_item(thing);

            const string colprf = item_prefix(item);
            const int col = menu_colour(item.name(DESC_A),
                                        colprf, "shop");

            if (col != -1)
                me->colour = col;
        }

        shopmenu.add_entry(me);
    }
}

void ShoppingList::display()
{
    if (list->empty())
        return;

    ShoppingListMenu shopmenu;
    shopmenu.set_tag("shop");
    shopmenu.menu_action  = Menu::ACT_EXECUTE;
    shopmenu.action_cycle = Menu::CYCLE_CYCLE;
    string title          = "thing";

    MenuEntry *mtitle = new MenuEntry(title, MEL_TITLE);
    shopmenu.set_title(mtitle);

    // Don't make a menu so tall that we recycle hotkeys on the same page.
    if (list->size() > 52
        && (shopmenu.maxpagesize() > 52 || shopmenu.maxpagesize() == 0))
    {
        shopmenu.set_maxpagesize(52);
    }

    string more_str = make_stringf("<yellow>You have %d gp</yellow>", you.gold);
    shopmenu.set_more(formatted_string::parse_string(more_str));

    shopmenu.set_flags(MF_SINGLESELECT | MF_ALWAYS_SHOW_MORE
                        | MF_ALLOW_FORMATTING);

    fill_out_menu(shopmenu);

    vector<MenuEntry*> sel;
    while (true)
    {
        // Abuse of the quantity field.
        mtitle->quantity = list->size();

        redraw_screen();
        sel = shopmenu.show();

        if (sel.empty())
            break;

        const CrawlHashTable* thing =
            static_cast<const CrawlHashTable *>(sel[0]->data);

        const bool is_item = thing_is_item(*thing);

        if (shopmenu.menu_action == Menu::ACT_EXECUTE)
        {
            int cost = thing_cost(*thing);

            if (cost > you.gold)
            {
                string prompt =
                   make_stringf("You cannot afford %s; travel there "
                                "anyway? (y/N)",
                                describe_thing(*thing, DESC_A).c_str());
                clrscr();
                if (!yesno(prompt.c_str(), true, 'n'))
                    continue;
            }

            const level_pos lp(thing_pos(*thing));
            start_translevel_travel(lp);
            break;
        }
        else if (shopmenu.menu_action == Menu::ACT_EXAMINE)
        {
            clrscr();
            if (is_item)
            {
                const item_def &item = get_thing_item(*thing);
                describe_item(const_cast<item_def&>(item));
            }
            else // not an item, so we only stored a description.
            {
                // HACK: Assume it's some kind of portal vault.
                snprintf(info, INFO_SIZE,
                         "%s with an entry fee of %d gold pieces.",
                         describe_thing(*thing, DESC_A).c_str(),
                         (int) thing_cost(*thing));

                print_description(info);
                getchm();
            }
        }
        else if (shopmenu.menu_action == Menu::ACT_MISC)
        {
            const int index = shopmenu.get_entry_index(sel[0]);
            if (index == -1)
            {
                mprf(MSGCH_ERROR, "ERROR: Unable to delete thing from shopping list!");
                more();
                continue;
            }

            del_thing_at_index(index);
            if (list->empty())
            {
                mpr("Your shopping list is now empty.");
                break;
            }

            shopmenu.clear();
            fill_out_menu(shopmenu);
        }
        else
            die("Invalid menu action type");
    }
    redraw_screen();
}

static bool _compare_shopping_things(const CrawlStoreValue& a,
                                     const CrawlStoreValue& b)
{
    const CrawlHashTable& hash_a = a.get_table();
    const CrawlHashTable& hash_b = b.get_table();

    const int a_cost = hash_a[SHOPPING_THING_COST_KEY];
    const int b_cost = hash_b[SHOPPING_THING_COST_KEY];

    return a_cost < b_cost;
}

// Reset max_buyable and min_unbuyable info. Call this anytime any of the
// player's gold, the shopping list, and the prices of the items on it
// change.
void ShoppingList::refresh()
{
    if (!you.props.exists(SHOPPING_LIST_KEY))
        you.props[SHOPPING_LIST_KEY].new_vector(SV_HASH, SFLAG_CONST_TYPE);
    list = &you.props[SHOPPING_LIST_KEY].get_vector();

    sort(list->begin(), list->end(), _compare_shopping_things);

    min_unbuyable_cost = INT_MAX;
    min_unbuyable_idx  = -1;
    max_buyable_cost   = -1;
    max_buyable_idx    = -1;

    int total_cost = 0;

    for (unsigned int i = 0; i < list->size(); i++)
    {
        const CrawlHashTable &thing = (*list)[i];

        const int cost = thing_cost(thing);

        if (cost <= you.gold)
        {
            max_buyable_cost = cost;
            max_buyable_idx  = i;
        }
        else if (min_unbuyable_idx == -1)
        {
            min_unbuyable_cost = cost;
            min_unbuyable_idx  = i;
        }
        total_cost += cost;
    }
    you.props[SHOPPING_LIST_COST_KEY].get_int() = total_cost;
}

int ShoppingList::find_thing(const item_def &item,
                             const level_pos &pos) const
{
    for (unsigned int i = 0; i < list->size(); i++)
    {
        const CrawlHashTable &thing = (*list)[i];
        const level_pos       _pos  = thing_pos(thing);

        if (pos != _pos)
            continue;

        if (!thing_is_item(thing))
            continue;

        const item_def &_item = get_thing_item(thing);

        if (item_name_simple(item) == item_name_simple(_item))
            return i;
    }

    return -1;
}

int ShoppingList::find_thing(const string &desc, const level_pos &pos) const
{
    for (unsigned int i = 0; i < list->size(); i++)
    {
        const CrawlHashTable &thing = (*list)[i];
        const level_pos       _pos  = thing_pos(thing);

        if (pos != _pos)
            continue;

        if (thing_is_item(thing))
            continue;

        if (desc == name_thing(thing))
            return i;
    }

    return -1;
}

bool ShoppingList::thing_is_item(const CrawlHashTable& thing)
{
    return thing.exists(SHOPPING_THING_ITEM_KEY);
}

const item_def& ShoppingList::get_thing_item(const CrawlHashTable& thing)
{
    ASSERT(thing.exists(SHOPPING_THING_ITEM_KEY));

    const item_def &item = thing[SHOPPING_THING_ITEM_KEY].get_item();
    ASSERT(item.defined());

    return item;
}

string ShoppingList::get_thing_desc(const CrawlHashTable& thing)
{
    ASSERT(thing.exists(SHOPPING_THING_DESC_KEY));

    string desc = thing[SHOPPING_THING_DESC_KEY].get_string();
    return desc;
}

int ShoppingList::thing_cost(const CrawlHashTable& thing)
{
    ASSERT(thing.exists(SHOPPING_THING_COST_KEY));
    return thing[SHOPPING_THING_COST_KEY].get_int();
}

level_pos ShoppingList::thing_pos(const CrawlHashTable& thing)
{
    ASSERT(thing.exists(SHOPPING_THING_POS_KEY));
    return thing[SHOPPING_THING_POS_KEY].get_level_pos();
}

string ShoppingList::name_thing(const CrawlHashTable& thing,
                                description_level_type descrip)
{
    if (thing_is_item(thing))
    {
        const item_def &item = get_thing_item(thing);
        return item.name(descrip);
    }
    else
    {
        string desc = get_thing_desc(thing);
        return apply_description(descrip, desc);
    }
}

string ShoppingList::describe_thing(const CrawlHashTable& thing,
                                    description_level_type descrip)
{
    const level_pos pos = thing_pos(thing);

    string desc = name_thing(thing, descrip) + " on ";

    if (pos.id == level_id::current())
        desc += "this level";
    else
        desc += pos.id.describe();

    return desc;
}

// Item name without curse-status or inscription.
string ShoppingList::item_name_simple(const item_def& item, bool ident)
{
    return item.name(DESC_PLAIN, false, ident, false, false,
                     ISFLAG_KNOW_CURSE);
}

static const char *shop_types[] =
{
    "weapon",
    "armour",
    "antique weapon",
    "antique armour",
    "antiques",
    "jewellery",
    "gadget",
    "book",
    "food",
    "distillery",
    "scroll",
    "general",
};

int str_to_shoptype(const string &s)
{
    if (s == "random" || s == "any")
        return SHOP_RANDOM;

    for (unsigned i = 0; i < ARRAYSZ(shop_types); ++i)
    {
        if (s == shop_types[i])
            return i;
    }
    return -1;
}

const char *shoptype_to_str(shop_type type)
{
    return shop_types[type];
}

void list_shop_types()
{
    mpr_nojoin(MSGCH_PLAIN, "Available shop types: ");
    for (unsigned i = 0; i < ARRAYSZ(shop_types); ++i)
        mprf_nocap("%s", shop_types[i]);
}
