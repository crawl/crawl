/*
 *  File:       shopping.cc
 *  Summary:    Shop keeper functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <3>     Jul 30 00      JDJ      in_a_shop uses shoppy instead of i when calling shop_set_id.
 *      <2>     Oct 31 99      CDL      right justify prices
 *      <1>     -/--/--        LRH      Created
 */

#include "AppHdr.h"
#include "chardump.h"
#include "shopping.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"
#include "cio.h"
#include "describe.h"
#include "food.h"
#include "invent.h"
#include "it_use2.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "macro.h"
#include "notes.h"
#include "player.h"
#include "randart.h"
#include "spl-book.h"
#include "stash.h"
#include "stuff.h"
#include "view.h"

static void in_a_shop(int shopidx);
static void more3();
static void purchase( int shop, int item_got, int cost, bool id);
static void shop_print(const char *shoppy, int sh_line);

static std::string hyphenated_suffix(char prev, char last)
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

static std::string purchase_keys(const std::string &s)
{
    if (s.empty())
        return "";

    std::string list = "<w>" + s.substr(0, 1);
    char last = s[0];
    for (int i = 1; i < (int) s.length(); ++i)
    {
        if (s[i] == s[i - 1] + 1)
            continue;

        char prev = s[i - 1];
        list += hyphenated_suffix(prev, last);
        list += (last = s[i]);
    }

    list += hyphenated_suffix( s[s.length() - 1], last );
    list += "</w>";
    return (list);
}

static void list_shop_keys(const std::string &purchasable)
{
    char buf[200];
    const int numlines = get_number_of_lines();
    cgotoxy(1, numlines - 1);

    std::string pkeys = purchase_keys(purchasable);
    if (!pkeys.empty())
        pkeys = "[" + pkeys + "] Buy Item";

    snprintf(buf, sizeof buf,
            "[<w>x</w>/<w>Esc</w>] Exit       [<w>v</w>] Examine Items  %s",
            pkeys.c_str());

    formatted_string fs = formatted_string::parse_string(buf);
    fs.cprintf("%*s", get_number_of_cols() - fs.length() - 1, "");
    fs.display();
    cgotoxy(1, numlines);

    fs = formatted_string::parse_string(
            "[<w>?</w>/<w>*</w>]   Inventory  "
            "[<w>\\</w>] Known Items");
    fs.cprintf("%*s", get_number_of_cols() - fs.length() - 1, "");
    fs.display();
}

static std::vector<int> shop_stock(int shopidx)
{
    std::vector<int> result;
    
    int itty = igrd[0][5 + shopidx];

    while ( itty != NON_ITEM )
    {
        result.push_back( itty );
        itty = mitm[itty].link;
    }
    return result;
}

static int shop_item_value(const item_def& item, int greed, bool id)
{
    int result = (greed * item_value(item, id) / 10);
    if ( you.duration[DUR_BARGAIN] ) // 20% discount
    {
        result *= 8;
        result /= 10;
    }
    
    if (result < 1)
        result = 1;

    return result;
}

static std::string shop_print_stock( const std::vector<int>& stock,
                                     const shop_struct& shop )
{
    ShopInfo &si = stashes.get_shop(shop.x, shop.y);
    const bool id = shoptype_identifies_stock(shop.type);
    std::string purchasable;
    for (unsigned int i = 0; i < stock.size(); ++i)
    {
        const int gp_value = shop_item_value(mitm[stock[i]], shop.greed, id);
        const bool can_afford = (you.gold >= gp_value);

        cgotoxy(1, i+1);
        const char c = i + 'a';
        if (can_afford)
            purchasable += c;

        textcolor( can_afford ? LIGHTGREEN : LIGHTRED );
        cprintf("%c - ", c);

        textcolor((i % 2) ? LIGHTGREY : WHITE);
        cprintf("%-56s%5d gold",
                mitm[stock[i]].name(DESC_NOCAP_A, false, id).c_str(),
                gp_value);
        si.add_item(mitm[stock[i]], gp_value);
    }
    textcolor(LIGHTGREY);
    return purchasable;
}

static void in_a_shop( int shopidx )
{
    const shop_struct& shop = env.shop[shopidx];

    cursor_control coff(false);

#ifdef USE_TILE
    clrscr();
#endif

    const std::string hello = "Welcome to " + shop_name(shop.x, shop.y) + "!";
    shop_print(hello.c_str(), 20);

    more3();    
    
    const bool id_stock = shoptype_identifies_stock(shop.type);
 
    while ( true )
    {
        stashes.get_shop(shop.x, shop.y).reset();
        
        std::vector<int> stock = shop_stock(shopidx);

        clrscr();
        if ( stock.empty() )
        {
            shop_print("I'm sorry, my shop is empty now.", 20);
            more3();
            return;
        }
            
        const std::string purchasable = shop_print_stock(stock, shop);
        list_shop_keys(purchasable);
            
        snprintf( info, INFO_SIZE, "You have %d gold piece%s.", you.gold,
                  (you.gold == 1) ? "" : "s" );
        textcolor(YELLOW);
        shop_print(info, 19);

        snprintf( info, INFO_SIZE, "What would you like to %s?",
                  purchasable.length()? "purchase" : "do");
        textcolor(CYAN);
        shop_print(info, 20);
            
        textcolor(LIGHTGREY);

        int ft = get_ch();
        
        if ( ft == '\\' )
            check_item_knowledge();
        else if (ft == 'x' || ft == ESCAPE)
            break;
        else if (ft == 'v')
        {
            textcolor(CYAN);
            shop_print("Examine which item?", 20);
            textcolor(LIGHTGREY);

            bool is_ok = true;

            ft = get_ch();
            if ( !isalpha(ft) )
            {
                is_ok = false;
            }
            else
            {
                ft = tolower(ft) - 'a';
                if ( ft >= static_cast<int>(stock.size()) )
                    is_ok = false;
            }

            if ( !is_ok )
            {
                shop_print("Huh?", 20);
                more3();
                continue;
            }

            // A hack to make the description more useful.
            // In theory, the user could kill the process at this
            // point and end up with valid ID for the item.
            // That's not very useful, though, because it doesn't set
            // type-ID and once you can access the item (by buying it)
            // you have its full ID anyway. Worst case, it won't get
            // noted when you buy it.
            item_def& item = mitm[stock[ft]];
            const unsigned long old_flags = item.flags;
            if ( id_stock )
                item.flags |= (ISFLAG_IDENT_MASK | ISFLAG_NOTED_ID |
                               ISFLAG_NOTED_GET);
            describe_item(item);
            if ( id_stock )
                item.flags = old_flags;
        }
        else if (ft == '?' || ft == '*')
            invent(-1, false);
        else if ( !isalpha(ft) )
        {
            shop_print("Huh?", 20);
            more3();
        }
        else
        {
            ft = tolower(ft) - 'a';
            if (ft >= static_cast<int>(stock.size()) )
            {
                shop_print("No such item.", 20);
                more3();
                continue;
            }

            item_def& item = mitm[stock[ft]];
            
            const int gp_value = shop_item_value(item, shop.greed, id_stock);
            if (gp_value > you.gold)
            {
                shop_print("I'm sorry, you don't seem to have enough money.",
                           20);
                more3();
            }
            else
            {
                snprintf(info, INFO_SIZE, "Purchase %s (%d gold)? [y/n]",
                         item.name(DESC_NOCAP_A).c_str(), gp_value);
                shop_print(info, 20);

                if ( yesno(NULL, true, 'n', false, false, true) )
                    purchase( shopidx, stock[ft], gp_value, id_stock );
            }
        }
    }
}

bool shoptype_identifies_stock(shop_type type)
{
    return
        type != SHOP_WEAPON_ANTIQUE &&
        type != SHOP_ARMOUR_ANTIQUE &&
        type != SHOP_GENERAL_ANTIQUE;
}

static void shop_print( const char *shoppy, int sh_lines )
{
    cgotoxy(1, sh_lines, GOTO_CRT);
    cprintf("%s", shoppy);
    clear_to_end_of_line();
}

static void more3()
{
    cgotoxy(70, 20, GOTO_CRT);
    cprintf("-more-");
    get_ch();
    return;
}

static void purchase( int shop, int item_got, int cost, bool id )
{
    you.gold -= cost;

    item_def& item = mitm[item_got];

    origin_purchased(item);

    if ( id )
    {
        // Identify the item and its type.
        // This also takes the ID note if necessary.
        set_ident_type(item.base_type, item.sub_type, ID_KNOWN_TYPE);
        set_ident_flags(item, ISFLAG_IDENT_MASK);
    }

    const int quant = item.quantity;
    // note that item will be invalidated if num == item.quantity
    const int num = move_item_to_player( item_got, item.quantity, true );

    // Shopkeepers will now place goods you can't carry outside the shop.
    if (num < quant)
    {
        snprintf( info, INFO_SIZE, "I'll put %s outside for you.",
                 (quant == 1) ? "it" :
                 (num > 0) ? "the rest" : "these" );

        shop_print( info, 20 );
        more3();

        move_item_to_grid( &item_got, env.shop[shop].x, env.shop[shop].y );
    }
}                               // end purchase()

// This probably still needs some work.  Rings used to be the only 
// artefacts which had a change in price, and that value corresponds
// to returning 50 from this function.  Good artefacts will probably
// be returning just over 30 right now.  Note that this isn't used
// as a multiple, its used in the old ring way: 7 * ret is added to
// the price of the artefact. -- bwr
int randart_value( const item_def &item )
{
    ASSERT( is_random_artefact( item ) );

    int ret = 10;
    randart_properties_t prop;
    randart_wpn_properties( item, prop );

    // Brands are already accounted for via existing ego checks

    // This should probably be more complex... but this isn't so bad:
    ret += 3 * prop[ RAP_AC ] + 3 * prop[ RAP_EVASION ] 
            + 3 * prop[ RAP_ACCURACY ] + 3 * prop[ RAP_DAMAGE ] 
            + 6 * prop[ RAP_STRENGTH ] + 6 * prop[ RAP_INTELLIGENCE ] 
            + 6 * prop[ RAP_DEXTERITY ];

    // These resistances have meaningful levels
    if (prop[ RAP_FIRE ] > 0)
        ret += 5 + 5 * (prop[ RAP_FIRE ] * prop[ RAP_FIRE ]);
    else if (prop[ RAP_FIRE ] < 0)
        ret -= 10;

    if (prop[ RAP_COLD ] > 0)
        ret += 5 + 5 * (prop[ RAP_COLD ] * prop[ RAP_COLD ]);
    else if (prop[ RAP_COLD ] < 0)
        ret -= 10;

    // These normally come alone or in resist/susceptible pairs...
    // we're making items a bit more expensive if they have both positive.
    if (prop[ RAP_FIRE ] > 0 && prop[ RAP_COLD ] > 0)
        ret += 20;

    if (prop[ RAP_NEGATIVE_ENERGY ] > 0)
        ret += 5 + 5 * (prop[RAP_NEGATIVE_ENERGY] * prop[RAP_NEGATIVE_ENERGY]);

    // only one meaningful level:
    if (prop[ RAP_POISON ])
        ret += 15;

    // only one meaningful level (hard to get):
    if (prop[ RAP_ELECTRICITY ])
        ret += 30;

    // magic resistance is from 35-100
    if (prop[ RAP_MAGIC ])
        ret += 5 + prop[ RAP_MAGIC ] / 15;

    if (prop[ RAP_EYESIGHT ])
        ret += 10;

    // abilities:
    if (prop[ RAP_LEVITATE ])
        ret += 3;

    if (prop[ RAP_BLINK ])
        ret += 3;

    if (prop[ RAP_CAN_TELEPORT ])
        ret += 5;

    if (prop[ RAP_BERSERK ])
        ret += 5;

    if (prop[ RAP_MAPPING ])
        ret += 15;

    if (prop[ RAP_INVISIBLE ])
        ret += 20;

    if (prop[ RAP_ANGRY ])
        ret -= 3;

    if (prop[ RAP_CAUSE_TELEPORTATION ])
        ret -= 3;

    if (prop[ RAP_NOISES ])
        ret -= 5;

    if (prop[ RAP_PREVENT_TELEPORTATION ])
        ret -= 8;

    if (prop[ RAP_PREVENT_SPELLCASTING ])
        ret -= 10;

    // ranges from 2-5
    if (prop[ RAP_MUTAGENIC ])
        ret -= (5 + 3 * prop[ RAP_MUTAGENIC ]);

    // ranges from 1-3
    if (prop[ RAP_METABOLISM ])
        ret -= (2 * prop[ RAP_METABOLISM ]);

    return ((ret > 0) ? ret : 0);
}

unsigned int item_value( item_def item, bool ident )
{
    // Note that we pass item in by value, since we want a local
    // copy to mangle as necessary... maybe that should be fixed,
    // but this function isn't called too often.
    item.flags = (ident) ? (item.flags | ISFLAG_IDENT_MASK) : (item.flags);

    int valued = 0;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        if (is_fixed_artefact( item ))
        {
            if (item_ident( item, ISFLAG_KNOW_PROPERTIES ))
            {
                switch (item.special)
                {
                case SPWPN_SWORD_OF_CEREBOV:
                    valued += 2000;
                    break;

                case SPWPN_SCEPTRE_OF_ASMODEUS:
                    valued += 1500;
                    break;

                case SPWPN_SWORD_OF_ZONGULDROK:
                    valued += 1250;
                    break;

                case SPWPN_SCEPTRE_OF_TORMENT:
                case SPWPN_SINGING_SWORD:
                case SPWPN_STAFF_OF_DISPATER:
                    valued += 1200;
                    break;

                case SPWPN_GLAIVE_OF_PRUNE:
                case SPWPN_WRATH_OF_TROG:
                    valued += 1000;
                    break;

                case SPWPN_SCYTHE_OF_CURSES:
                    valued += 800;
                    break;

                case SPWPN_MACE_OF_VARIABILITY:
                    valued += 700;
                    break;

                default:
                    valued += 1000;
                    break;
                }
                break;
            }

        }                       // end uniques

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

        case WPN_ANCUS:
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
            valued += 45;
            break;

        case WPN_SPIKED_FLAIL:
            valued += 50;

        case WPN_HAND_CROSSBOW:
            valued += 51;
            break;

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
            valued += 65;
            break;

        case WPN_DIRE_FLAIL:
        case WPN_BARDICHE:
            valued += 90;
            break;

        case WPN_EVENINGSTAR:
            valued += 65;
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
        case WPN_TRIPLE_SWORD:
        case WPN_DEMON_BLADE:
        case WPN_BLESSED_BLADE:
        case WPN_LAJATANG:
            valued += 200;
            break;
        }

        if (item_type_known(item))
        {
            switch (get_weapon_brand( item ))
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
                valued *= 50;
                break;

            case SPWPN_SPEED:
                valued *= 40;
                break;

            case SPWPN_DISTORTION:
            case SPWPN_ELECTROCUTION:
            case SPWPN_PAIN:
            case SPWPN_VORPAL:
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

            case SPWPN_PROTECTION:
                valued *= 20;
                break;
            }

            valued /= 10;
        }

        // elf/dwarf
        if (get_equip_race(item) == ISFLAG_ELVEN
                || get_equip_race(item) == ISFLAG_DWARVEN)
        {
            valued *= 12;
            valued /= 10;
        }

        // value was "6" but comment read "orc", so I went with comment {dlb}
        if (get_equip_race(item) == ISFLAG_ORCISH)
        {
            valued *= 8;
            valued /= 10;
        }

        if (item_ident( item, ISFLAG_KNOW_PLUSES ))
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
                //break;
            }

            if (item.plus2 < 0)
            {
                valued -= 5;
                valued += (item.plus2 * item.plus2 * item.plus2);

                if (valued < 1)
                    valued = 1;
            }
        }

        if (is_random_artefact( item ))
        {
            if (item_type_known(item))
                valued += (7 * randart_value( item ));
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
        if (item_ident( item, ISFLAG_KNOW_PLUSES ))
        {
            // assume not cursed (can they be anyway?)
            if (item.plus < 0)
                valued -= 11150;

            if (item.plus >= 0)
                valued += (item.plus * 2);
        }

        switch (item.sub_type)
        {
        case MI_DART:
        case MI_LARGE_ROCK:
        case MI_STONE:
        case MI_NONE:
            valued++;
            break;
        case MI_ARROW:
        case MI_BOLT:
        case MI_NEEDLE:
            valued += 2;
            break;
        default:
            // was: cases 6 through 16 with empty strcat()'s 15jan2000 {dlb}
            valued += 5;
            break;              //strcat(glog , ""); break;
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
        case ARM_HELM:
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
            const int sparm = get_armour_ego_type( item );
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

        if (item_ident( item, ISFLAG_KNOW_PLUSES ))
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

        if (is_random_artefact( item ))
        {
            if (item_type_known(item))
                valued += (7 * randart_value( item ));
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
        if ( !item_type_known(item) )
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

            if (item_ident( item, ISFLAG_KNOW_PLUSES ))
            {
                if (item.plus == 0)
                    valued -= 50;
                else
                    valued = (valued * (item.plus + 45)) / 50;
            }
        }
        break;

    case OBJ_POTIONS:
        if ( !item_type_known(item) )
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
                valued += 120;
                break;
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
            case POT_SPEED:
                valued += 25;
                break;
            case POT_BLOOD:
            case POT_HEALING:
            case POT_LEVITATION:
                valued += 20;
                break;
            case POT_PORRIDGE:
                valued += 10;
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

        case FOOD_CHOKO:
        case FOOD_LYCHEE:
        case FOOD_RAMBUTAN:
        case FOOD_SNOZZCUMBER:
        case FOOD_CHUNK:
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
        if ( !item_type_known(item) )
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

        if ( !item_type_known(item) )
            valued += 50;
        else
        {
            if (item_ident( item, ISFLAG_KNOW_PLUSES ) 
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
                valued += 35;
                break;
            case AMU_CLARITY:
            case AMU_RESIST_CORROSION:
            case AMU_RESIST_MUTATION:
            case AMU_RESIST_SLOW:
            case AMU_WARDING:
                valued += 30;
                break;
            case AMU_CONSERVATION:
            case AMU_CONTROLLED_FLIGHT:
                valued += 25;
                break;
            case AMU_RAGE:
                valued += 20;
                break;
            case AMU_INACCURACY:
                valued -= 50;
                break;
                // got to do delusion!
            }

            if (is_random_artefact(item))
            {
                // in this branch we're guaranteed to know
                // the item type!
                if (valued < 0)
                    valued = randart_value( item ) - 5;
                else
                    valued += randart_value( item );
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
            valued += book_rarity(item.sub_type) * 50;
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

        if (item_is_rod( item ) && item_ident( item, ISFLAG_KNOW_PLUSES ))
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

void shop()
{
    int i;

    for (i = 0; i < MAX_SHOPS; i++)
        if (env.shop[i].x == you.x_pos && env.shop[i].y == you.y_pos)
            break;

    if (i == MAX_SHOPS)
    {
        mpr("Help! Non-existent shop.", MSGCH_DANGER);
        return;
    }

    in_a_shop(i);
    burden_change();
    redraw_screen();
}                               // end shop()

shop_struct *get_shop(int sx, int sy)
{
    if (grd[sx][sy] != DNGN_ENTER_SHOP)
        return (NULL);

    // find shop
    for (int shoppy = 0; shoppy < MAX_SHOPS; shoppy ++)
    {
        // find shop index plus a little bit of paranoia
        if (env.shop[shoppy].x == sx && env.shop[shoppy].y == sy &&
            env.shop[shoppy].type != SHOP_UNASSIGNED)
        {
            return (&env.shop[shoppy]);
        }
    }
    return (NULL);
}

std::string shop_name(int sx, int sy, bool add_stop)
{
    std::string name(shop_name(sx, sy));
    if (add_stop)
        name += ".";
    return (name);
}

std::string shop_name(int sx, int sy)
{
    const shop_struct *cshop = get_shop(sx, sy);

    // paranoia
    if (grd[sx][sy] != DNGN_ENTER_SHOP)
        return ("");

    if (!cshop)
    {
        mpr("Help! Non-existent shop.");
        return ("Buggy Shop");
    }

    int shop_type = cshop->type;

    unsigned long seed = static_cast<unsigned long>( cshop->keeper_name[0] )
        | (static_cast<unsigned long>( cshop->keeper_name[1] ) << 8)
        | (static_cast<unsigned long>( cshop->keeper_name[1] ) << 16);

    std::string sh_name = make_name(seed, false);
    sh_name += "'s ";

    if (shop_type == SHOP_WEAPON_ANTIQUE || shop_type == SHOP_ARMOUR_ANTIQUE)
        sh_name += "Antique ";

    sh_name +=
        (shop_type == SHOP_WEAPON
         || shop_type == SHOP_WEAPON_ANTIQUE) ? "Weapon" :
        (shop_type == SHOP_ARMOUR
         || shop_type == SHOP_ARMOUR_ANTIQUE) ? "Armour" :
        
        (shop_type == SHOP_JEWELLERY)         ? "Jewellery" :
        (shop_type == SHOP_WAND)              ? "Magical Wand" :
        (shop_type == SHOP_BOOK)              ? "Book" :
        (shop_type == SHOP_FOOD)              ? "Food" :
        (shop_type == SHOP_SCROLL)            ? "Magic Scroll" :
        (shop_type == SHOP_GENERAL_ANTIQUE)   ? "Assorted Antiques" :
        (shop_type == SHOP_DISTILLERY)        ? "Distillery" :
        (shop_type == SHOP_GENERAL)           ? "General Store"
                                              : "Bug";

    if (shop_type != SHOP_GENERAL
        && shop_type != SHOP_GENERAL_ANTIQUE && shop_type != SHOP_DISTILLERY)
    {
        int temp = sx + sy % 4;
        sh_name += (temp == 0) ? " Shoppe" :
                   (temp == 1) ? " Boutique" :
                   (temp == 2) ? " Emporium"
                               : " Shop";
    }

    return (sh_name);
}

