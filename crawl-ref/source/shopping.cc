/*
 *  File:       shopping.cc
 *  Summary:    Shop keeper functions.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "shopping.h"
#include "message.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "externs.h"
#include "options.h"
#include "artefact.h"
#include "cio.h"
#include "describe.h"
#include "decks.h"
#include "dgn-overview.h"
#include "files.h"
#include "food.h"
#include "invent.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "kills.h"
#include "macro.h"
#include "menu.h"
#include "notes.h"
#include "place.h"
#include "player.h"
#include "spl-book.h"
#include "stash.h"
#include "stuff.h"
#include "travel.h"
#include "env.h"

#define SHOPPING_LIST_COST_KEY "shopping_list_cost_key"

ShoppingList shopping_list;

static bool _in_shop_now = false;

static bool _purchase( int shop, int item_got, int cost, bool id);

static void _shop_print( const char *shoppy, int line )
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
    if (_in_shop_now)
    {
        textcolor(channel_to_colour(MSGCH_PROMPT));
        _shop_print(prompt, 1);

        return yesno(NULL, true, safeanswer, false, false, true);
    }
    else
        return yesno(prompt, true, safeanswer, false, false, false);
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

static std::string _hyphenated_suffix(char prev, char last)
{
    std::string s;
    if (prev > last + 2)
        s += "</w>-<w>";
    else if (prev == last + 2)
        s += (char) (last + 1);

    if (prev != last)
        s += prev;
    return (s);
}

static std::string _purchase_keys(const std::string &s)
{
    if (s.empty())
        return "";

    std::string list = "<w>" + s.substr(0, 1);
    char last = s[0];
    for (unsigned int i = 1; i < s.length(); ++i)
    {
        if (s[i] == s[i - 1] + 1)
            continue;

        char prev = s[i - 1];
        list += _hyphenated_suffix(prev, last);
        list += (last = s[i]);
    }

    list += _hyphenated_suffix( s[s.length() - 1], last );
    list += "</w>";
    return (list);
}

static void _list_shop_keys(const std::string &purchasable, bool viewing,
                            int total_stock, int num_selected,
                            int num_in_list)
{
    ASSERT(total_stock > 0);

    const int numlines = get_number_of_lines();
    formatted_string fs;

    std::string shop_list = "";
    if (!viewing && you.level_type == LEVEL_DUNGEON)
    {
        shop_list = "[<w>$</w>] ";
        if (num_selected > 0)
            shop_list += "Selected -> shopping list";
        else if (num_in_list > 0)
            shop_list += "Shopping list -> selected";
        else
            shop_list = "";
    }
    if (!shop_list.empty())
    {
        cgotoxy(1, numlines - 2, GOTO_CRT);
        fs = formatted_string::parse_string(shop_list);
        fs.cprintf("%*s", get_number_of_cols() - fs.length() - 1, "");
        fs.display();
    }

    cgotoxy(1, numlines - 1, GOTO_CRT);

    std::string pkeys = "";
    if (viewing)
    {
        pkeys = "<w>a</w>";
        if (total_stock > 1)
        {
            pkeys += "-<w>";
            pkeys += 'a' + total_stock - 1;
            pkeys += "</w>";
        }
    }
    else
        pkeys = _purchase_keys(purchasable);

    if (!pkeys.empty())
    {
        pkeys = "[" + pkeys + "] Select Item to "
                + (viewing ? "Examine" : "Buy");
    }
    fs = formatted_string::parse_string(make_stringf(
            "[<w>x</w>/<w>Esc</w>"
#ifdef USE_TILE
            "/<w>R-Click</w>"
#endif
            "] exit            [<w>!</w>] %s   %s",
            (viewing ? "to select items " : "to examine items"),
            pkeys.c_str()));

    fs.cprintf("%*s", get_number_of_cols() - fs.length() - 1, "");
    fs.display();
    cgotoxy(1, numlines, GOTO_CRT);

    fs = formatted_string::parse_string(
            "[<w>Enter</w>"
#ifdef USE_TILE
            "/<w>L-Click</w>"
#endif
            "] make purchase   [<w>\\</w>] list known items   "
            "[<w>?</w>/<w>*</w>] inventory");

    fs.cprintf("%*s", get_number_of_cols() - fs.length() - 1, "");
    fs.display();
}

static std::vector<int> _shop_get_stock(int shopidx)
{
    std::vector<int> result;
    // Shop items are heaped up at this cell.
    const coord_def stack_location(0, 5 + shopidx);
    for (stack_iterator si(stack_location); si; ++si)
        result.push_back(si.link());
    return result;
}

static int _shop_get_item_value(const item_def& item, int greed, bool id,
                                bool ignore_bargain = false)
{
    int result = (greed * item_value(item, id) / 10);
    if (you.duration[DUR_BARGAIN] && !ignore_bargain) // 20% discount
    {
        result *= 8;
        result /= 10;
    }

    if (result < 1)
        result = 1;

    return result;
}

static std::string _shop_print_stock( const std::vector<int>& stock,
                                      const std::vector<bool>& selected,
                                      const std::vector<bool>& in_list,
                                      const shop_struct& shop,
                                      int total_cost )
{
    ShopInfo &si  = StashTrack.get_shop(shop.pos);
    const bool id = shoptype_identifies_stock(shop.type);
    std::string purchasable;
    for (unsigned int i = 0; i < stock.size(); ++i)
    {
        const item_def& item = mitm[stock[i]];
        const int gp_value = _shop_get_item_value(item, shop.greed, id);
        const bool can_afford = (you.gold >= gp_value);

        cgotoxy(1, i+1, GOTO_CRT);
        const char c = i + 'a';
        if (can_afford)
            purchasable += c;

        // Colour stock as follows:
        //  * lightcyan, if on the shopping list.
        //  * lightred, if you can't buy all you selected.
        //  * lightgreen, if this item is purchasable along with your selections
        //  * red, if this item is not purchasable even by itself.
        //  * yellow, if this item would be purchasable if you deselected
        //            something else.

        // Is this too complicated? (jpeg)

        if (in_list[i])
            textcolor(LIGHTCYAN);
        else if (total_cost > you.gold && selected[i])
            textcolor(LIGHTRED);
        else if (gp_value <= you.gold - total_cost || selected[i] && can_afford)
            textcolor(LIGHTGREEN);
        else if (!can_afford)
            textcolor(RED);
        else
            textcolor(YELLOW);

        if (in_list[i])
            cprintf("%c $ ", c);
        else if (selected[i])
            cprintf("%c + ", c);
        else
            cprintf("%c - ", c);

        if (Options.menu_colour_shops)
        {
            // Colour stock according to menu colours.
            const std::string colprf = menu_colour_item_prefix(item);
            const int col = menu_colour(item.name(DESC_NOCAP_A),
                                        colprf, "shop");
            textcolor(col != -1 ? col : LIGHTGREY);
        }
        else
            textcolor(i % 2 ? LIGHTGREY : WHITE);

        cprintf("%-56s%5d gold",
                item.name(DESC_NOCAP_A, false, id).substr(0, 56).c_str(),
                gp_value);

        si.add_item(item, gp_value);
    }
    textcolor(LIGHTGREY);

    return (purchasable);
}

static int _count_identical(const std::vector<int>& stock,
                            const item_def& item)
{
    int count = 0;
    for (unsigned int i = 0; i < stock.size(); i++)
    {
        const item_def &other = mitm[stock[i]];

        if (ShoppingList::items_are_same(item, other))
            count++;
    }
    return (count);
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
//  * \ shows discovered items, as now
//  * x exits (also Esc), as now
//  * ! toggles examination mode (where letter keys view items)
//  * *, ? lists inventory
//
//  For the ? key, the text should read:
//  [!] switch to examination mode
//  [!] switch to selection mode
static bool _in_a_shop( int shopidx, int &num_in_list )
{
    const shop_struct& shop = env.shop[shopidx];

    unwind_bool in_shop(_in_shop_now, true);

    cursor_control coff(false);

    clrscr();

    const std::string hello = "Welcome to " + shop_name(shop.pos) + "!";
    bool first = true;
    int total_cost = 0;

    std::vector<int> stock = _shop_get_stock(shopidx);

    // Autoinscribe randarts in the shop.
    for (unsigned int i = 0; i < stock.size(); i++)
    {
        item_def& item = mitm[stock[i]];
        if (Options.autoinscribe_artefacts && is_artefact(item))
            item.inscription = artefact_auto_inscription(item);
    }

    std::vector<bool> selected;
    std::vector<bool> in_list;

    const bool id_stock         = shoptype_identifies_stock(shop.type);
          bool bought_something = false;
          bool viewing          = false;
          bool first_iter       = true;

    while (true)
    {
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
            return (bought_something);
        }

        const std::string purchasable = _shop_print_stock(stock, selected,
                                                          in_list, shop,
                                                          total_cost);
        _list_shop_keys(purchasable, viewing, stock.size(), num_selected,
                        num_in_list);

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
                                                             (long) cost);
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

            textcolor(YELLOW);
        }
        else if (total_cost > you.gold)
        {
            snprintf(info, INFO_SIZE, "You have %d gold piece%s. "
                           "You are short %d gold piece%s for the purchase.",
                     you.gold,
                     you.gold != 1 ? "s" : "",
                     total_cost - you.gold,
                     (total_cost - you.gold != 1) ? "s" : "");

            textcolor(LIGHTRED);
        }
        else
        {
            snprintf(info, INFO_SIZE, "You have %d gold piece%s. "
                     "After the purchase, you will have %d gold piece%s.",
                     you.gold,
                     you.gold != 1 ? "s" : "",
                     you.gold - total_cost,
                     (you.gold - total_cost != 1) ? "s" : "");

            textcolor(YELLOW);
        }

        _shop_print(info, 0);

        if (first)
        {
            first = false;
            snprintf( info, INFO_SIZE, "%s What would you like to do? ",
                      hello.c_str() );
        }
        else
            snprintf(info, INFO_SIZE, "What would you like to do? ");

        textcolor(CYAN);
        _shop_print(info, 1);

        textcolor(LIGHTGREY);

        mouse_control mc(MOUSE_MODE_MORE);
        int key = getchm();

        if (key == '\\')
        {
            if (!check_item_knowledge(true))
            {
                _shop_print("You don't recognise anything yet!", 1);
                _shop_more();
            }
        }
        else if (key == 'x' || key == ESCAPE || key == CK_MOUSE_CMD)
            break;
        else if (key == '\r' || key == CK_MOUSE_CLICK)
        {
            std::vector<bool> to_buy;
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
            }
            else if (!total_purchase) // Nothing selected.
                continue;
            else
            {
                snprintf(info, INFO_SIZE, "Purchase for %d gold? (y/n) ",
                         total_purchase);

                if ( _shop_yesno(info, 'n') )
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
                                           item.name(DESC_NOCAP_A).c_str()));

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
                        mprf( "I'll put %s outside for you.",
                              num_items == 1             ? "it" :
                              num_items == outside_items ? "them"
                                                         : "part of them" );
                    }
                    bought_something = true;
                }
            }
            //_shop_more();
            continue;
        }
        else if (key == '!')
        {
            // Toggle between browsing and shopping.
            viewing = !viewing;
        }
        else if (key == '?' || key == '*')
            browse_inventory(false);
        else if (key == '$')
        {
            if (viewing || (num_selected == 0 && num_in_list == 0)
                || you.level_type != LEVEL_DUNGEON)
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
                            // Ignore Bargaining.
                            const int cost = _shop_get_item_value(item,
                                        shop.greed, id_stock, false);
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
            _shop_print("Huh?", 1);
            _shop_more();
        }
        else
        {
            key = tolower(key) - 'a';
            if (key >= static_cast<int>(stock.size()) )
            {
                _shop_print("No such item.", 1);
                _shop_more();
                continue;
            }

            item_def& item = mitm[stock[key]];
            if (viewing)
            {
                // A hack to make the description more useful.
                // In theory, the user could kill the process at this
                // point and end up with valid ID for the item.
                // That's not very useful, though, because it doesn't set
                // type-ID and once you can access the item (by buying it)
                // you have its full ID anyway. Worst case, it won't get
                // noted when you buy it.
                const unsigned long old_flags = item.flags;
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
                    if (gp_value > you.gold)
                    {
                        if ( _shop_yesno("Remove from shopping list? (y/N)",
                                         'n') )
                        {
                            shopping_list.del_thing(item);
                            in_list[key]  = false;
                            selected[key] = false;
                        }
                        continue;
                    }
                    else
                    {
                        if ( _shop_yesno("Remove item from shopping list and "
                                         "buy it? (Y/n)",  'y') )
                        {
                            shopping_list.del_thing(item);
                            in_list[key] = false;
                            // Will be toggled to true later
                            selected[key] = false;
                        }
                        else
                            continue;
                    }
                }

                selected[key] = !selected[key];
                if (selected[key])
                    total_cost += gp_value;
                else
                    total_cost -= gp_value;

                ASSERT(total_cost >= 0);
            }
        }
    }

    return (bought_something);
}

bool shoptype_identifies_stock(shop_type type)
{
    return (type != SHOP_WEAPON_ANTIQUE
            && type != SHOP_ARMOUR_ANTIQUE
            && type != SHOP_GENERAL_ANTIQUE);
}

static bool _purchase( int shop, int item_got, int cost, bool id )
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

    const int quant = item.quantity;
    // Note that item will be invalidated if num == item.quantity.
    const int num = move_item_to_player( item_got, item.quantity, true );

    // Shopkeepers will now place goods you can't carry outside the shop.
    if (num < quant)
    {
        move_item_to_grid( &item_got, env.shop[shop].pos );
        return (false);
    }
    return (true);
}

// This probably still needs some work.  Rings used to be the only
// artefacts which had a change in price, and that value corresponds
// to returning 50 from this function.  Good artefacts will probably
// be returning just over 30 right now.  Note that this isn't used
// as a multiple, its used in the old ring way: 7 * ret is added to
// the price of the artefact. -- bwr
int artefact_value( const item_def &item )
{
    ASSERT( is_artefact( item ) );

    int ret = 10;
    artefact_properties_t prop;
    artefact_wpn_properties( item, prop );

    // Brands are already accounted for via existing ego checks

    // This should probably be more complex... but this isn't so bad:
    ret += 3 * prop[ ARTP_AC ] + 3 * prop[ ARTP_EVASION ]
            + 3 * prop[ ARTP_ACCURACY ] + 3 * prop[ ARTP_DAMAGE ]
            + 6 * prop[ ARTP_STRENGTH ] + 6 * prop[ ARTP_INTELLIGENCE ]
            + 6 * prop[ ARTP_DEXTERITY ];

    // These resistances have meaningful levels
    if (prop[ ARTP_FIRE ] > 0)
        ret += 5 + 5 * (prop[ ARTP_FIRE ] * prop[ ARTP_FIRE ]);
    else if (prop[ ARTP_FIRE ] < 0)
        ret -= 10;

    if (prop[ ARTP_COLD ] > 0)
        ret += 5 + 5 * (prop[ ARTP_COLD ] * prop[ ARTP_COLD ]);
    else if (prop[ ARTP_COLD ] < 0)
        ret -= 10;

    if (prop[ARTP_PONDEROUS])
        ret -= 10;

    // These normally come alone or in resist/susceptible pairs...
    // we're making items a bit more expensive if they have both positive.
    if (prop[ ARTP_FIRE ] > 0 && prop[ ARTP_COLD ] > 0)
        ret += 20;

    if (prop[ ARTP_NEGATIVE_ENERGY ] > 0)
        ret += 5 + 5 * (prop[ARTP_NEGATIVE_ENERGY] * prop[ARTP_NEGATIVE_ENERGY]);

    // only one meaningful level:
    if (prop[ ARTP_POISON ])
        ret += 15;

    // only one meaningful level (hard to get):
    if (prop[ ARTP_ELECTRICITY ])
        ret += 30;

    // magic resistance is from 35-100
    if (prop[ ARTP_MAGIC ])
        ret += 5 + prop[ ARTP_MAGIC ] / 15;

    if (prop[ ARTP_EYESIGHT ])
        ret += 10;

    // abilities:
    if (prop[ ARTP_LEVITATE ])
        ret += 3;

    if (prop[ ARTP_BLINK ])
        ret += 3;

    if (prop[ ARTP_BERSERK ])
        ret += 5;

    if (prop[ ARTP_INVISIBLE ])
        ret += 20;

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

    // ranges from 2-5
    if (prop[ ARTP_MUTAGENIC ])
        ret -= (5 + 3 * prop[ ARTP_MUTAGENIC ]);

    // ranges from 1-3
    if (prop[ ARTP_METABOLISM ])
        ret -= (2 * prop[ ARTP_METABOLISM ]);

    return ((ret > 0) ? ret : 0);
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
            return (entry->value);
    }

    int valued = 0;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        switch (item.sub_type)
        {
        case WPN_CLUB:
        case WPN_KNIFE:
            valued += 10;
            break;

        case WPN_SLING:
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

        case WPN_WHIP:
        case WPN_BLOWGUN:
            valued += 25;
            break;

        case WPN_HAND_AXE:
            valued += 28;
            break;

        case WPN_HAMMER:
        case WPN_FALCHION:
        case WPN_MACE:
        case WPN_SCYTHE:
            valued += 30;
            break;

        case WPN_BOW:
            valued += 31;
            break;

        case WPN_QUARTERSTAFF:
        case WPN_SHORT_SWORD:
        case WPN_SPEAR:
            valued += 32;
            break;

        case WPN_FLAIL:
            valued += 35;
            break;

        case WPN_ANKUS:
        case WPN_WAR_AXE:
        case WPN_MORNINGSTAR:
        case WPN_SABRE:
            valued += 40;
            break;

        case WPN_CROSSBOW:
            valued += 41;
            break;

        case WPN_TRIDENT:
            valued += 42;
            break;

        case WPN_LONG_SWORD:
        case WPN_LONGBOW:
        case WPN_SCIMITAR:
        case WPN_BLESSED_FALCHION:
            valued += 45;
            break;

        case WPN_SPIKED_FLAIL:
        case WPN_BLESSED_LONG_SWORD:
        case WPN_BLESSED_SCIMITAR:
            valued += 50;

        case WPN_HALBERD:
            valued += 52;
            break;

        case WPN_GLAIVE:
            valued += 55;
            break;

        case WPN_BROAD_AXE:
        case WPN_GREAT_SWORD:
            valued += 60;
            break;

        case WPN_BATTLEAXE:
        case WPN_GREAT_MACE:
        case WPN_EVENINGSTAR:
            valued += 65;
            break;

        case WPN_DIRE_FLAIL:
        case WPN_BARDICHE:
            valued += 90;
            break;

        case WPN_EXECUTIONERS_AXE:
            valued += 100;
            break;

        case WPN_DOUBLE_SWORD:
            valued += 100;
            break;

        case WPN_DEMON_WHIP:
            valued += 130;
            break;

        case WPN_QUICK_BLADE:
        case WPN_DEMON_TRIDENT:
            valued += 150;
            break;

        case WPN_KATANA:
        case WPN_DEMON_BLADE:
        case WPN_TRIPLE_SWORD:
        case WPN_BLESSED_KATANA:
        case WPN_HOLY_BLADE:
        case WPN_BLESSED_DOUBLE_SWORD:
        case WPN_BLESSED_GREAT_SWORD:
        case WPN_BLESSED_TRIPLE_SWORD:
        case WPN_HOLY_SCOURGE:
        case WPN_LAJATANG:
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

            case SPWPN_DRAINING:
                valued *= 64;
                break;

            case SPWPN_VAMPIRICISM:
                valued *= 60;
                break;

            case SPWPN_FLAME:
            case SPWPN_FROST:
            case SPWPN_HOLY_WRATH:
            case SPWPN_REACHING:
            case SPWPN_RETURNING:
                valued *= 50;
                break;

            case SPWPN_CHAOS:
            case SPWPN_SPEED:
                valued *= 40;
                break;

            case SPWPN_DISTORTION:
            case SPWPN_ELECTROCUTION:
            case SPWPN_PAIN:
                valued *= 30;
                break;

            case SPWPN_FLAMING:
            case SPWPN_FREEZING:
            case SPWPN_DRAGON_SLAYING:
                valued *= 25;
                break;

            case SPWPN_VENOM:
                valued *= 23;
                break;

            case SPWPN_ORC_SLAYING:
                valued *= 21;
                break;

            case SPWPN_VORPAL:
            case SPWPN_PROTECTION:
            case SPWPN_EVASION:
                valued *= 20;
                break;
            }

            valued /= 10;
        }

        if (get_equip_race(item) == ISFLAG_ELVEN
            || get_equip_race(item) == ISFLAG_DWARVEN)
        {
            valued *= 12;
            valued /= 10;
        }

        if (get_equip_race(item) == ISFLAG_ORCISH)
        {
            valued *= 8;
            valued /= 10;
        }

        if (item_ident(item, ISFLAG_KNOW_PLUSES))
        {
            if (item.plus >= 0)
            {
                valued += item.plus * 2;
                valued *= 10 + 3 * item.plus;
                valued /= 10;
            }

            if (item.plus2 >= 0)
            {
                valued += item.plus2 * 2;
                valued *= 10 + 3 * item.plus2;
                valued /= 10;
            }

            if (item.plus < 0)
            {
                valued -= 5;
                valued += (item.plus * item.plus * item.plus);

                if (valued < 1)
                    valued = 1;
            }

            if (item.plus2 < 0)
            {
                valued -= 5;
                valued += (item.plus2 * item.plus2 * item.plus2);

                if (valued < 1)
                    valued = 1;
            }
        }

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
        {
            valued *= 6;
            valued /= 10;
        }
        break;

    case OBJ_MISSILES:          // ammunition
        switch (item.sub_type)
        {
        case MI_DART:
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

            case SPMSL_RETURNING:
                valued *= 50;
                break;

            case SPMSL_CHAOS:
                valued *= 40;
                break;

            case SPMSL_CURARE:
            case SPMSL_PENETRATION:
            case SPMSL_REAPING:
            case SPMSL_SILVER:
            case SPMSL_STEEL:
            case SPMSL_DISPERSAL:
            case SPMSL_EXPLODING:
                valued *= 30;
                break;

            case SPMSL_FLAME:
            case SPMSL_FROST:
                valued *= 25;
                break;

            case SPMSL_POISONED:
            case SPMSL_PARALYSIS:
            case SPMSL_SLOW:
            case SPMSL_SLEEP:
            case SPMSL_CONFUSION:
            case SPMSL_SICKNESS:
            case SPMSL_RAGE:
                valued *= 23;
                break;
            }

            valued /= 10;
        }

        if (get_equip_race(item) == ISFLAG_ELVEN
            || get_equip_race(item) == ISFLAG_DWARVEN)
        {
            valued *= 12;
            valued /= 10;
        }

        if (get_equip_race(item) == ISFLAG_ORCISH)
        {
            valued *= 8;
            valued /= 10;
        }

        if (item_ident(item, ISFLAG_KNOW_PLUSES))
        {
            if (item.plus >= 0)
                valued += (item.plus * 2);

            if (item.plus < 0)
            {
                valued += item.plus * item.plus * item.plus;

                if (valued < 1)
                    valued = 1;
            }
        }
        break;

    case OBJ_ARMOUR:
        switch (item.sub_type)
        {
        case ARM_GOLD_DRAGON_ARMOUR:
            valued += 1600;
            break;

        case ARM_GOLD_DRAGON_HIDE:
            valued += 1400;
            break;

        case ARM_STORM_DRAGON_ARMOUR:
            valued += 1050;
            break;

        case ARM_STORM_DRAGON_HIDE:
            valued += 900;
            break;

        case ARM_DRAGON_ARMOUR:
        case ARM_ICE_DRAGON_ARMOUR:
            valued += 750;
            break;

        case ARM_SWAMP_DRAGON_ARMOUR:
            valued += 650;
            break;

        case ARM_DRAGON_HIDE:
        case ARM_CRYSTAL_PLATE_MAIL:
        case ARM_TROLL_LEATHER_ARMOUR:
        case ARM_ICE_DRAGON_HIDE:
            valued += 500;
            break;

        case ARM_MOTTLED_DRAGON_ARMOUR:
        case ARM_SWAMP_DRAGON_HIDE:
            valued += 400;
            break;

        case ARM_STEAM_DRAGON_ARMOUR:
        case ARM_MOTTLED_DRAGON_HIDE:
            valued += 300;
            break;

        case ARM_PLATE_MAIL:
            valued += 230;
            break;

        case ARM_STEAM_DRAGON_HIDE:
            valued += 200;
            break;

        case ARM_BANDED_MAIL:
        case ARM_CENTAUR_BARDING:
        case ARM_NAGA_BARDING:
            valued += 150;
            break;

        case ARM_SPLINT_MAIL:
            valued += 140;
            break;

        case ARM_TROLL_HIDE:
            valued += 130;
            break;

        case ARM_CHAIN_MAIL:
            valued += 110;
            break;

        case ARM_SCALE_MAIL:
            valued += 83;
            break;

        case ARM_LARGE_SHIELD:
            valued += 75;
            break;

        case ARM_SHIELD:
            valued += 45;
            break;

        case ARM_RING_MAIL:
            valued += 40;
            break;

        case ARM_HELMET:
        case ARM_CAP:
        case ARM_WIZARD_HAT:
        case ARM_BUCKLER:
            valued += 25;
            break;

        case ARM_LEATHER_ARMOUR:
            valued += 20;
            break;

        case ARM_BOOTS:
            valued += 15;
            break;

        case ARM_GLOVES:
            valued += 12;
            break;

        case ARM_CLOAK:
            valued += 10;
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
            case SPARM_NORMAL:
            default:
                valued *= 10;
                break;

            case SPARM_ARCHMAGI:
                valued *= 100;
                break;

            case SPARM_DARKNESS:
            case SPARM_RESISTANCE:
            case SPARM_REFLECTION:
                valued *= 60;
                break;

            case SPARM_POSITIVE_ENERGY:
                valued *= 50;
                break;

            case SPARM_MAGIC_RESISTANCE:
            case SPARM_PROTECTION:
            case SPARM_RUNNING:
                valued *= 40;
                break;

            case SPARM_COLD_RESISTANCE:
            case SPARM_DEXTERITY:
            case SPARM_FIRE_RESISTANCE:
            case SPARM_SEE_INVISIBLE:
            case SPARM_INTELLIGENCE:
            case SPARM_LEVITATION:
            case SPARM_PRESERVATION:
            case SPARM_STEALTH:
            case SPARM_STRENGTH:
                valued *= 30;
                break;

            case SPARM_POISON_RESISTANCE:
                valued *= 20;
                break;

            case SPARM_PONDEROUSNESS:
                valued *= 5;
                break;
            }

            valued /= 10;
        }

        if (get_equip_race(item) == ISFLAG_ELVEN
            || get_equip_race(item) == ISFLAG_DWARVEN)
        {
            valued *= 12;
            valued /= 10;
        }

        if (get_equip_race(item) == ISFLAG_ORCISH)
        {
            valued *= 8;
            valued /= 10;
        }

        if (item_ident(item, ISFLAG_KNOW_PLUSES))
        {
            valued += 5;
            if (item.plus >= 0)
            {
                valued += item.plus * 30;
                valued *= 10 + 4 * item.plus;
                valued /= 10;
            }

            if (item.plus < 0)
            {
                valued += item.plus * item.plus * item.plus;

                if (valued < 1)
                    valued = 1;
            }
        }

        if (is_artefact(item))
        {
            if (item_type_known(item))
                valued += (7 * artefact_value(item));
            else
                valued += 50;
        }
        else if (item_type_known(item) && get_equip_desc(item) != 0)
        {
            valued += 20;
        }

        if (item_known_cursed(item))
        {
            valued *= 6;
            valued /= 10;
        }
        break;

    case OBJ_WANDS:
        if (!item_type_known(item))
            valued += 200;
        else
        {
            switch (item.sub_type)
            {
            case WAND_HASTING:
            case WAND_HEALING:
                valued += 300;
                break;

            case WAND_TELEPORTATION:
                valued += 250;
                break;

            case WAND_COLD:
            case WAND_FIRE:
            case WAND_FIREBALL:
                valued += 200;
                break;

            case WAND_INVISIBILITY:
            case WAND_DRAINING:
            case WAND_LIGHTNING:
                valued += 175;
                break;

            case WAND_DISINTEGRATION:
                valued += 160;
                break;

            case WAND_DIGGING:
            case WAND_PARALYSIS:
                valued += 100;
                break;

            case WAND_FLAME:
            case WAND_FROST:
                valued += 75;
                break;

            case WAND_ENSLAVEMENT:
            case WAND_POLYMORPH_OTHER:
                valued += 90;
                break;

            case WAND_CONFUSION:
            case WAND_SLOWING:
                valued += 70;
                break;

            case WAND_MAGIC_DARTS:
            case WAND_RANDOM_EFFECTS:
            default:
                valued += 45;
                break;
            }

            if (item_ident(item, ISFLAG_KNOW_PLUSES))
            {
                if (item.plus == 0)
                    valued -= 50;
                else
                    valued = (valued * (item.plus + 45)) / 50;
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

            case POT_GAIN_DEXTERITY:
            case POT_GAIN_INTELLIGENCE:
            case POT_GAIN_STRENGTH:
                valued += 350;
                break;

            case POT_CURE_MUTATION:
                valued += 150;
                break;

            case POT_MAGIC:
            case POT_RESISTANCE:
                valued += 70;
                break;

            case POT_INVISIBILITY:
                valued += 55;
                break;

            case POT_MUTATION:
            case POT_RESTORE_ABILITIES:
                valued += 50;
                break;

            case POT_BERSERK_RAGE:
            case POT_HEAL_WOUNDS:
                valued += 30;
                break;

            case POT_MIGHT:
            case POT_AGILITY:
            case POT_BRILLIANCE:
            case POT_SPEED:
                valued += 25;
                break;

            case POT_HEALING:
            case POT_LEVITATION:
                valued += 20;
                break;

            case POT_BLOOD:
            case POT_PORRIDGE:
                valued += 10;
                break;

            case POT_BLOOD_COAGULATED:
                valued += 5;
                break;

            case POT_CONFUSION:
            case POT_DECAY:
            case POT_DEGENERATION:
            case POT_PARALYSIS:
            case POT_POISON:
            case POT_SLOWING:
            case POT_STRONG_POISON:
            case POT_WATER:
                valued++;
                break;
            }
        }
        break;

    case OBJ_FOOD:
        switch (item.sub_type)
        {
        case FOOD_ROYAL_JELLY:
            valued = 120;
            break;

        case FOOD_MEAT_RATION:
        case FOOD_BREAD_RATION:
            valued = 40;
            break;

        case FOOD_HONEYCOMB:
            valued = 25;
            break;

        case FOOD_BEEF_JERKY:
        case FOOD_PIZZA:
            valued = 18;
            break;

        case FOOD_CHEESE:
        case FOOD_SAUSAGE:
            valued = 15;
            break;

        case FOOD_LEMON:
        case FOOD_ORANGE:
        case FOOD_BANANA:
            valued = 12;
            break;

        case FOOD_APPLE:
        case FOOD_APRICOT:
        case FOOD_PEAR:
            valued = 8;
            break;

        case FOOD_CHUNK:
            if (food_is_rotten(item))
                break;

        case FOOD_CHOKO:
        case FOOD_LYCHEE:
        case FOOD_RAMBUTAN:
        case FOOD_SNOZZCUMBER:
            valued = 4;
            break;

        case FOOD_STRAWBERRY:
        case FOOD_GRAPE:
        case FOOD_SULTANA:
            valued = 1;
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

            case SCR_ENCHANT_WEAPON_III:
            case SCR_VORPALISE_WEAPON:
                valued += 200;
                break;

            case SCR_SUMMONING:
                valued += 95;
                break;

            case SCR_TORMENT:
            case SCR_HOLY_WORD:
            case SCR_SILENCE:
            case SCR_VULNERABILITY:
                valued += 75;
                break;

            case SCR_ENCHANT_WEAPON_II:
                valued += 55;
                break;

            case SCR_RECHARGING:
                valued += 50;
                break;

            case SCR_ENCHANT_ARMOUR:
            case SCR_ENCHANT_WEAPON_I:
                valued += 48;
                break;

            case SCR_FEAR:
                valued += 45;
                break;

            case SCR_MAGIC_MAPPING:
                valued += 35;
                break;

            case SCR_BLINKING:
            case SCR_REMOVE_CURSE:
            case SCR_TELEPORTATION:
                valued += 30;
                break;

            case SCR_DETECT_CURSE:
            case SCR_IDENTIFY:
                valued += 20;
                break;

            case SCR_FOG:
                valued += 10;
                break;

            case SCR_NOISE:
            case SCR_RANDOM_USELESSNESS:
                valued += 2;
                break;

            case SCR_CURSE_ARMOUR:
            case SCR_CURSE_WEAPON:
            case SCR_PAPER:
            case SCR_IMMOLATION:
                valued++;
                break;
            }
        }
        break;

    case OBJ_JEWELLERY:
        if (item_known_cursed(item))
            valued -= 10;

        if (!item_type_known(item))
            valued += 50;
        else
        {
            if (item_ident(item, ISFLAG_KNOW_PLUSES)
                && (item.sub_type == RING_PROTECTION
                    || item.sub_type == RING_STRENGTH
                    || item.sub_type == RING_EVASION
                    || item.sub_type == RING_DEXTERITY
                    || item.sub_type == RING_INTELLIGENCE
                    || item.sub_type == RING_SLAYING))
            {
                if (item.plus > 0)
                    valued += 10 * item.plus;

                if (item.sub_type == RING_SLAYING && item.plus2 > 0)
                    valued += 10 * item.plus2;

                if (item.plus < 0)
                    valued -= 50;

                if (item.sub_type == RING_SLAYING && item.plus2 < 0)
                    valued -= 50;
            }

            switch (item.sub_type)
            {
            case RING_INVISIBILITY:
                valued += 100;
                break;

            case RING_REGENERATION:
                valued += 75;
                break;

            case RING_FIRE:
            case RING_ICE:
                valued += 62;
                break;

            case RING_LIFE_PROTECTION:
                valued += 60;
                break;

            case RING_TELEPORT_CONTROL:
                valued += 42;
                break;

            case RING_MAGICAL_POWER:
            case RING_PROTECTION_FROM_MAGIC:
                valued += 40;
                break;

            case RING_WIZARDRY:
                valued += 35;
                break;

            case RING_LEVITATION:
            case RING_POISON_RESISTANCE:
            case RING_PROTECTION_FROM_COLD:
            case RING_PROTECTION_FROM_FIRE:
            case RING_SLAYING:
                valued += 30;
                break;

            case RING_SUSTAIN_ABILITIES:
            case RING_SUSTENANCE:
            case RING_TELEPORTATION: // usually cursed
                valued += 25;
                break;

            case RING_SEE_INVISIBLE:
                valued += 20;
                break;

            case RING_DEXTERITY:
            case RING_EVASION:
            case RING_INTELLIGENCE:
            case RING_PROTECTION:
            case RING_STRENGTH:
                valued += 10;
                break;

            case RING_HUNGER:
                valued -= 50;
                break;

            case AMU_THE_GOURMAND:
            case AMU_GUARDIAN_SPIRIT:
            case AMU_FAITH:
                valued += 35;
                break;

            case AMU_CLARITY:
            case AMU_RESIST_CORROSION:
            case AMU_RESIST_MUTATION:
            case AMU_WARDING:
                valued += 30;
                break;

            case AMU_CONSERVATION:
            case AMU_CONTROLLED_FLIGHT:
                valued += 25;
                break;

            case AMU_RAGE:
            case AMU_STASIS:
                valued += 20;
                break;

            case AMU_INACCURACY:
                valued -= 50;
                break;
                // got to do delusion!
            }

            if (is_artefact(item))
            {
                // in this branch we're guaranteed to know
                // the item type!
                if (valued < 0)
                    valued = artefact_value(item) - 5;
                else
                    valued += artefact_value(item);
            }

            valued *= 7;
        }
        break;

    case OBJ_MISCELLANY:
        if (item_type_known(item))
        {
            switch (item.sub_type)
            {
            case MISC_RUNE_OF_ZOT:  // upped from 1200 to encourage collecting
                valued += 10000;
                break;

            case MISC_HORN_OF_GERYON:
                valued += 5000;
                break;

            case MISC_DISC_OF_STORMS:
                valued += 2000;
                break;

            case MISC_CRYSTAL_BALL_OF_SEEING:
                valued += 500;
                break;

            case MISC_BOTTLED_EFREET:
                valued += 400;
                break;

            case MISC_CRYSTAL_BALL_OF_FIXATION:
            case MISC_EMPTY_EBONY_CASKET:
                valued += 20;
                break;
            default:
                if (is_deck(item))
                    valued += 200 + item.special * 150;
                else
                    valued += 500;
            }
        }
        else
        {
            switch (item.sub_type)
            {
            case MISC_RUNE_OF_ZOT:
                valued += 5000;
                break;

            case MISC_HORN_OF_GERYON:
                valued += 1000;
                break;

            case MISC_CRYSTAL_BALL_OF_SEEING:
                valued += 450;
                break;

            case MISC_BOTTLED_EFREET:
                valued += 350;
                break;

            default:
                valued += 400;
            }
        }
        break;

    case OBJ_BOOKS:
        valued = 150;

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

                std::sort(rarities, rarities + SPELLBOOK_SIZE);
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
        if (!item_type_known(item))
            valued = 120;
        else if (item.sub_type == STAFF_SMITING
                || item.sub_type == STAFF_STRIKING
                || item.sub_type == STAFF_WARDING
                || item.sub_type == STAFF_DISCOVERY)
        {
            valued = 150;
        }
        else
            valued = 250;

        if (item_is_rod(item) && item_ident(item, ISFLAG_KNOW_PLUSES))
            valued += 50 * (item.plus2 / ROD_CHARGE_MULT);
        break;

    case OBJ_ORBS:
        valued = 250000;
        break;

    default:
        break;
    }                           // end switch

    if (valued < 1)
        valued = 1;

    valued *= item.quantity;

    return (valued);
}                               // end item_value()

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
        mpr("Help! Non-existent shop.", MSGCH_ERROR);
        return;
    }

    // Quick out, if no inventory
    if ( _shop_get_stock(i).empty() )
    {
        const shop_struct& shop = env.shop[i];
        mprf("%s appears to be closed.", shop_name(shop.pos).c_str());
        _delete_shop(i);
        return;
    }

          int  num_in_list      = 0;
    const bool bought_something = _in_a_shop(i, num_in_list);
    const std::string shopname = shop_name(env.shop[i].pos);

    // If the shop is now empty, erase it from the overview.
    if (_shop_get_stock(i).empty())
        _delete_shop(i);

    burden_change();
    redraw_screen();

    if (bought_something)
        mprf("Thank you for shopping at %s!", shopname.c_str());

    if (num_in_list > 0)
        mpr("You can access your shopping list by pressing '$'.");
}

shop_struct *get_shop(const coord_def& where)
{
    if (grd(where) != DNGN_ENTER_SHOP)
        return (NULL);

    // Check all shops for one at the correct position.
    for (int i = 0; i < MAX_SHOPS; i ++)
    {
        shop_struct& shop = env.shop[i];
        // A little bit of paranoia.
        if (shop.pos == where && shop.type != SHOP_UNASSIGNED)
            return (&shop);
    }
    return (NULL);
}

std::string shop_name(const coord_def& where, bool add_stop)
{
    std::string name(shop_name(where));
    if (add_stop)
        name += ".";
    return (name);
}

std::string shop_name(const coord_def& where)
{
    const shop_struct *cshop = get_shop(where);

    // paranoia
    if (grd(where) != DNGN_ENTER_SHOP)
        return ("");

    if (!cshop)
    {
        mpr("Help! Non-existent shop.");
        return ("Buggy Shop");
    }

    const shop_type type = cshop->type;

    unsigned long seed = static_cast<unsigned long>( cshop->keeper_name[0] )
        | (static_cast<unsigned long>( cshop->keeper_name[1] ) << 8)
        | (static_cast<unsigned long>( cshop->keeper_name[1] ) << 16);

    std::string sh_name = apostrophise(make_name(seed, false)) + " ";

    if (type == SHOP_WEAPON_ANTIQUE || type == SHOP_ARMOUR_ANTIQUE)
        sh_name += "Antique ";

    sh_name +=
        (type == SHOP_WEAPON
         || type == SHOP_WEAPON_ANTIQUE) ? "Weapon" :
        (type == SHOP_ARMOUR
         || type == SHOP_ARMOUR_ANTIQUE) ? "Armour" :

        (type == SHOP_JEWELLERY)         ? "Jewellery" :
        (type == SHOP_WAND)              ? "Magical Wand" :
        (type == SHOP_BOOK)              ? "Book" :
        (type == SHOP_FOOD)              ? "Food" :
        (type == SHOP_SCROLL)            ? "Magic Scroll" :
        (type == SHOP_GENERAL_ANTIQUE)   ? "Assorted Antiques" :
        (type == SHOP_DISTILLERY)        ? "Distillery" :
        (type == SHOP_GENERAL)           ? "General Store"
                                         : "Bug";

    if (type != SHOP_GENERAL
        && type != SHOP_GENERAL_ANTIQUE
        && type != SHOP_DISTILLERY)
    {
        const char* suffixnames[] = {"Shoppe", "Boutique", "Emporium", "Shop"};
        const int temp = (where.x + where.y) % 4;
        sh_name += ' ';
        sh_name += suffixnames[temp];
    }

    return (sh_name);
}

bool is_shop_item(const item_def &item)
{
    return (item.pos.x == 0 && item.pos.y >= 5 && item.pos.y < (MAX_SHOPS + 5));
}

////////////////////////////////////////////////////////////////////////

// Setup shopping list after restoring savefile.
static void _callback(bool saving)
{
    if (!saving)
        shopping_list.refresh();
}
static SavefileCallback _register_callback(_callback);

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
    if (list == NULL) \
    { \
        mpr("SavefileCallback global constructor weirdness!", MSGCH_ERROR); \
        return (false); \
    } \
    level_pos pos;                  \
    if (_pos != NULL)               \
        pos = *_pos;                \
    else                            \
        pos = level_pos::current(); \
    ASSERT(pos.is_valid());

#define SETUP_THING()                             \
    CrawlHashTable *thing = new CrawlHashTable();  \
    (*thing)[SHOPPING_THING_COST_KEY] = (long) cost; \
    (*thing)[SHOPPING_THING_POS_KEY]  = pos;

bool ShoppingList::add_thing(const item_def &item, int cost,
                             const level_pos* _pos)
{
    ASSERT(item.is_valid());
    ASSERT(cost > 0);

    SETUP_POS();

    if (pos.id.level_type != LEVEL_DUNGEON)
    {
        mprf("The shopping list can only contain things in the dungeon.",
             MSGCH_ERROR);
        return (false);
    }

    if (find_thing(item, pos) != -1)
    {
        mprf(MSGCH_ERROR, "%s is already on the shopping list.",
             item.name(DESC_CAP_THE).c_str());
        return (false);
    }

    SETUP_THING();
    (*thing)[SHOPPING_THING_ITEM_KEY] = item;
    list->push_back(*thing);
    refresh();

    return (true);
}

bool ShoppingList::add_thing(std::string desc, std::string buy_verb, int cost,
                             const level_pos* _pos)
{
    ASSERT(!desc.empty());
    ASSERT(!buy_verb.empty());
    ASSERT(cost > 0);

    SETUP_POS();

    if (pos.id.level_type != LEVEL_DUNGEON)
    {
        mprf("The shopping list can only contain things in the dungeon.",
             MSGCH_ERROR);
        return (false);
    }

    if (find_thing(desc, pos) != -1)
    {
        mprf(MSGCH_ERROR, "%s is already on the shopping list.",
             desc.c_str());
        return (false);
    }

    SETUP_THING();
    (*thing)[SHOPPING_THING_DESC_KEY] = desc;
    (*thing)[SHOPPING_THING_VERB_KEY] = buy_verb;
    list->push_back(*thing);
    refresh();

    return (true);
}

#undef SETUP_THING

bool ShoppingList::is_on_list(const item_def &item,
                              const level_pos* _pos) const
{
    SETUP_POS();

    return (find_thing(item, pos) != -1);
}

bool ShoppingList::is_on_list(std::string desc,
                              const level_pos* _pos) const
{
    SETUP_POS();

    return (find_thing(desc, pos) != -1);
}

bool ShoppingList::del_thing(const item_def &item,
                             const level_pos* _pos)
{
    SETUP_POS();

    int idx = find_thing(item, pos);

    if (idx == -1)
    {
        mprf(MSGCH_ERROR, "%s isn't on shopping list, can't delete it.",
             item.name(DESC_CAP_THE).c_str());
        return (false);
    }

    list->erase(idx);
    refresh();

    return (true);
}

bool ShoppingList::del_thing(std::string desc, const level_pos* _pos)
{
    SETUP_POS();

    int idx = find_thing(desc, pos);

    if (idx == -1)
    {
        mprf(MSGCH_ERROR, "%s isn't on shopping list, can't delete it.",
             desc.c_str());
        return (false);
    }

    list->erase(idx);
    refresh();

    return (true);
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
                                                long cost)
{
    // Can't put items in Bazaar shops in the shopping list, so
    // don't bother transfering shopping list items to Bazaar shops.
    if (cost != -1 && you.level_type != LEVEL_DUNGEON)
        return (0);

    switch (item.base_type)
    {
    case OBJ_JEWELLERY:
    case OBJ_BOOKS:
    case OBJ_STAVES:
        // Only these are really interchangable.
        break;

    default:
        return (0);
    }

    if (!item_type_known(item) || is_artefact(item))
        return (0);

    // Ignore stat-modification rings which reduce a stat, since they're
    // worthless.
    if (item.base_type == OBJ_JEWELLERY)
    {
        if (item.sub_type == RING_SLAYING && item.plus < 0 && item.plus2 < 0)
            return (0);

        if (item.plus < 0)
            return (0);
    }

    // Item is already on shopping-list.
    const bool on_list = find_thing(item, level_pos::current()) != -1;

    const bool do_prompt =
        (item.base_type == OBJ_JEWELLERY && !jewellery_is_amulet(item)
         && ring_has_stackable_effect(item))
     // Manuals and tomes of destruction are consumable.
     || (item.base_type == OBJ_BOOKS
         && (item.sub_type == BOOK_MANUAL
             || item.sub_type == BOOK_DESTRUCTION));

    bool add_item = false;

    std::vector<level_pos> to_del;

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

        const level_pos list_pos = thing_pos(thing);

        // cost = -1, we just found a shop item which is cheaper than
        // one on the shopping list.
        if (cost != -1)
        {
            long list_cost = thing_cost(thing);

            if (cost >= list_cost)
                continue;

            // Only prompt once.
            if (thing.exists(REPLACE_PROMPTED_KEY))
                continue;
            thing[REPLACE_PROMPTED_KEY] = (bool)true;

            std::string prompt =
                make_stringf("Shopping-list: replace %dgp %s with cheaper "
                             "one? (Y/n)", list_cost,
                             describe_thing(thing).c_str());

            if (_shop_yesno(prompt.c_str(), 'y'))
            {
                add_item = true;
                to_del.push_back(list_pos);
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

            std::string prompt =
                make_stringf("Shopping-list: remove %s? (Y/n)",
                             describe_thing(thing, DESC_NOCAP_A).c_str());

            if (_shop_yesno(prompt.c_str(), 'y'))
                to_del.push_back(list_pos);
        }
        else
        {
            std::string str =
                make_stringf("Shopping-list: removing %s",
                             describe_thing(thing, DESC_NOCAP_A).c_str());

            _shop_mpr(str.c_str());
            to_del.push_back(list_pos);
        }
    }

    for (unsigned int i = 0; i < to_del.size(); i++)
        del_thing(item, &to_del[i]);

    if (add_item && !on_list)
        add_thing(item, cost);

    return (to_del.size());
}

int ShoppingList::size() const
{
    if (list == NULL)
    {
        mpr("SavefileCallback global constructor weirdness!", MSGCH_ERROR);
        return (0);
    }

    return ( list->size() );
}

bool ShoppingList::items_are_same(const item_def& item_a,
                                  const item_def& item_b)
{
    return (item_name_simple(item_a) == item_name_simple(item_b));
}

void ShoppingList::move_things(const coord_def &_src, const coord_def &_dst)
{
    if (crawl_state.map_stat_gen || crawl_state.test)
        // Shopping list is unitialized and uneeded.
        return;

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
        // Shopping list is unitialized and uneeded.
        return;

    for (unsigned int i = 0; i < list->size(); i++)
    {
        const CrawlHashTable &thing = (*list)[i];

        if (thing_pos(thing) == pos)
        {
            list->erase(i);
            i--;
        }
    }
}

void ShoppingList::gold_changed(int old_amount, int new_amount)
{
    if (list == NULL)
    {
        mpr("SavefileCallback global constructor weirdness!", MSGCH_ERROR);
        return;
    }

    if (new_amount > old_amount && new_amount >= min_unbuyable_cost)
    {
        ASSERT(min_unbuyable_idx < list->size());

        std::vector<std::string> descs;
        for (unsigned int i = min_unbuyable_idx; i < list->size(); i++)
        {
            const CrawlHashTable &thing = (*list)[i];
            const long           cost   = thing_cost(thing);

            if (cost > new_amount)
            {
                ASSERT(i > (unsigned int) min_unbuyable_idx);
                break;
            }

            std::string desc;

            if (thing.exists(SHOPPING_THING_VERB_KEY))
                desc += thing[SHOPPING_THING_VERB_KEY].get_string();
            else
                desc = "buy";
            desc += " ";

            desc += describe_thing(thing, DESC_NOCAP_A);

            descs.push_back(desc);
        }
        ASSERT(!descs.empty());

        mpr_comma_separated_list("You now have enough gold to ", descs,
                                 ", or ");
        mpr("You can access your shopping list by pressing '$'.");

        // Reset max_buyable and min_unbuyable info
        refresh();
    }
    else if (new_amount < old_amount && new_amount < max_buyable_cost)
    {
        // Reset max_buyable and min_unbuyable info
        refresh();
    }
}

class ShoppingListMenu : public Menu
{
public:
    ShoppingListMenu()
        : Menu()
    { }

protected:
    void draw_title();
};

void ShoppingListMenu::draw_title()
{
    if (title)
    {
        const long total_cost = you.props[SHOPPING_LIST_COST_KEY];

        cgotoxy(1, 1);
        textcolor(title->colour);
        cprintf("%d %s%s, total cost %ld gp",
                title->quantity, title->text.c_str(),
                title->quantity > 1? "s" : "",
                total_cost);

        const char *verb = menu_action == ACT_EXECUTE ? "travel" :
                           menu_action == ACT_EXAMINE ? "examine" :
                                                        "delete";
        draw_title_suffix(formatted_string::parse_string(make_stringf(
            "<lightgrey>  [<w>a-z</w>: %s  <w>?</w>/<w>!</w>: change action]",
            verb)), false);
    }
}

void ShoppingList::fill_out_menu(Menu& shopmenu)
{
    menu_letter hotkey;
    for (unsigned i = 0; i < list->size(); ++i, ++hotkey)
    {
        CrawlHashTable &thing = (*list)[i];
        level_pos      pos    = thing_pos(thing);
        long           cost   = thing_cost(thing);

        std::string etitle =
            make_stringf("[%s] %s (%d gp)", short_place_name(pos.id).c_str(),
                         name_thing(thing, DESC_NOCAP_A).c_str(),
                         cost);

        MenuEntry *me = new MenuEntry(etitle, MEL_ITEM, 1, hotkey);
        me->data = &thing;

        if (cost > you.gold)
            me->colour = DARKGREY;
        else if (thing_is_item(thing) && Options.menu_colour_shops)
        {
            // Colour shopping list item according to menu colours.
            const item_def &item = get_thing_item(thing);

            const std::string colprf = menu_colour_item_prefix(item);
            const int col = menu_colour(item.name(DESC_NOCAP_A),
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

    const bool travelable = can_travel_interlevel();

    ShoppingListMenu shopmenu;
    shopmenu.set_tag("shop");
    shopmenu.menu_action  = travelable ? Menu::ACT_EXECUTE : Menu::ACT_EXAMINE;
    shopmenu.action_cycle = travelable ? Menu::CYCLE_CYCLE : Menu::CYCLE_NONE;
    std::string title     = "thing";

    MenuEntry *mtitle = new MenuEntry(title, MEL_TITLE);
    // Abuse of the quantity field.
    mtitle->quantity = list->size();
    shopmenu.set_title(mtitle);

    // Don't make a menu so tall that we recycle hotkeys on the same page.
    if (list->size() > 52
        && (shopmenu.maxpagesize() > 52 || shopmenu.maxpagesize() == 0))
    {
        shopmenu.set_maxpagesize(52);
    }

    std::string more_str = make_stringf("<yellow>You have %d gp</yellow>",
                                        you.gold);
    shopmenu.set_more(formatted_string::parse_string(more_str));

    shopmenu.set_flags( MF_SINGLESELECT | MF_ALWAYS_SHOW_MORE
                        | MF_ALLOW_FORMATTING );

    fill_out_menu(shopmenu);

    std::vector<MenuEntry*> sel;
    while (true)
    {
        redraw_screen();
        sel = shopmenu.show();

        if (sel.empty())
            break;

        const CrawlHashTable* thing =
            static_cast<const CrawlHashTable *>(sel[0]->data);

        const bool is_item = thing_is_item(*thing);

        if (shopmenu.menu_action == Menu::ACT_EXECUTE)
        {
            const long cost = thing_cost(*thing);

            if (cost > you.gold)
            {
                std::string prompt =
                   make_stringf("You cannot afford %s; travel there "
                                "anyway? (y/N)",
                                describe_thing(*thing, DESC_NOCAP_A).c_str());
                clrscr();
                if (!yesno(prompt.c_str(), true, 'n'))
                    continue;
            }

            const travel_target lp(thing_pos(*thing), false);
            start_translevel_travel(lp);
            break;
        }
        else if (shopmenu.menu_action == Menu::ACT_EXAMINE)
        {
            clrscr();
            if (is_item)
            {
                const item_def &item = get_thing_item(*thing);
                describe_item( const_cast<item_def&>(item) );
            }
            else // not an item, so we only stored a description.
            {
                // HACK: Assume it's some kind of portal vault.
                snprintf(info, INFO_SIZE,
                         "%s with an entry fee of %d gold pieces.",
                         describe_thing(*thing, DESC_CAP_A).c_str(),
                         (int) thing_cost(*thing));

                print_description(info);

                if (getchm() == 0)
                    getchm();
            }
        }
        else if (shopmenu.menu_action == Menu::ACT_MISC)
        {
            std::string prompt =
                make_stringf("Delete %s from shopping list? (y/N)",
                             describe_thing(*thing, DESC_NOCAP_A).c_str());
            clrscr();
            if (!yesno(prompt.c_str(), true, 'n'))
                continue;

            const int index = shopmenu.get_entry_index(sel[0]);
            if (index == -1)
            {
                mpr("ERROR: Unable to delete thing from shopping list!",
                    MSGCH_ERROR);
                more();
                continue;
            }

            list->erase(index);
            if (list->size() == 0)
                break;

            shopmenu.clear();
            fill_out_menu(shopmenu);
        }
        else
            DEBUGSTR("Invalid menu action type");
    }
    redraw_screen();
}

bool _compare_shopping_things(const CrawlStoreValue& a,
                              const CrawlStoreValue& b)
{
    const CrawlHashTable& hash_a = a.get_table();
    const CrawlHashTable& hash_b = b.get_table();

    const long a_cost = hash_a[SHOPPING_THING_COST_KEY];
    const long b_cost = hash_b[SHOPPING_THING_COST_KEY];

    return (a_cost < b_cost);
}

void ShoppingList::refresh()
{
    if (!you.props.exists(SHOPPING_LIST_KEY))
        you.props[SHOPPING_LIST_KEY].new_vector(SV_HASH, SFLAG_CONST_TYPE);
    list = &you.props[SHOPPING_LIST_KEY].get_vector();

    std::sort(list->begin(), list->end(), _compare_shopping_things);

    min_unbuyable_cost = INT_MAX;
    min_unbuyable_idx  = -1;
    max_buyable_cost   = -1;
    max_buyable_idx    = -1;

    long total_cost = 0;

    for (unsigned int i = 0; i < list->size(); i++)
    {
        const CrawlHashTable &thing = (*list)[i];

        const long cost = thing_cost(thing);

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
    you.props[SHOPPING_LIST_COST_KEY] = (long) total_cost;
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
            return (i);
    }

    return (-1);
}

int ShoppingList::find_thing(const std::string &desc,
                             const level_pos &pos) const
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
            return (i);
    }

    return (-1);
}

bool ShoppingList::thing_is_item(const CrawlHashTable& thing)
{
    return thing.exists(SHOPPING_THING_ITEM_KEY);
}

const item_def& ShoppingList::get_thing_item(const CrawlHashTable& thing)
{
    ASSERT(thing.exists(SHOPPING_THING_ITEM_KEY));

    const item_def &item = thing[SHOPPING_THING_ITEM_KEY].get_item();
    ASSERT(item.is_valid());

    return (item);
}

std::string ShoppingList::get_thing_desc(const CrawlHashTable& thing)
{
    ASSERT(thing.exists(SHOPPING_THING_DESC_KEY));

    std::string desc = thing[SHOPPING_THING_DESC_KEY].get_string();
    return (desc);
}

long ShoppingList::thing_cost(const CrawlHashTable& thing)
{
    ASSERT(thing.exists(SHOPPING_THING_COST_KEY));
    return (thing[SHOPPING_THING_COST_KEY].get_long());
}

level_pos ShoppingList::thing_pos(const CrawlHashTable& thing)
{
    ASSERT(thing.exists(SHOPPING_THING_POS_KEY));
    return (thing[SHOPPING_THING_POS_KEY].get_level_pos());
}

std::string ShoppingList::name_thing(const CrawlHashTable& thing,
                                     description_level_type descrip)
{
    if (thing_is_item(thing))
    {
        const item_def &item = get_thing_item(thing);
        return item.name(descrip);
    }
    else
    {
        std::string desc = get_thing_desc(thing);
        return apply_description(descrip, desc);
    }
}

std::string ShoppingList::describe_thing(const CrawlHashTable& thing,
                                         description_level_type descrip)
{
    const level_pos pos = thing_pos(thing);

    std::string desc = name_thing(thing, descrip) + " on ";

    if (pos.id == level_id::current())
        desc += "this level";
    else
        desc += pos.id.describe();

    return (desc);
}

// Item name without curse-status or inscription.
std::string ShoppingList::item_name_simple(const item_def& item, bool ident)
{
    return item.name(DESC_PLAIN, false, ident, false, false,
                     ISFLAG_KNOW_CURSE);
}
