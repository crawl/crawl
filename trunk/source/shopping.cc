/*
 *  File:       shopping.cc
 *  Summary:    Shop keeper functions.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *      <3>     Jul 30 00      JDJ      in_a_shop uses shoppy instead of i when calling shop_set_id.
 *      <2>     Oct 31 99      CDL      right justify prices
 *      <1>     -/--/--        LRH      Created
 */

#include "AppHdr.h"
#include "shopping.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "describe.h"
#include "invent.h"
#include "items.h"
#include "itemname.h"
#include "macro.h"
#include "player.h"
#include "randart.h"
#include "spl-book.h"
#include "stuff.h"


static char in_a_shop(char shoppy, char id[4][50]);
static char more3(void);
static void purchase( int shop, int item_got, int cost );
static void shop_init_id(int i, FixedArray < int, 4, 50 > &shop_id);
static void shop_print(const char *shoppy, char sh_line);
static void shop_set_ident_type(int i, FixedArray < int, 4, 50 > &shop_id,
                                unsigned char base_type, unsigned char sub_type);
static void shop_uninit_id(int i, FixedArray < int, 4, 50 > &shop_id);

char in_a_shop( char shoppy, char id[4][50] )
{
    // easier to work with {dlb}
    unsigned int greedy = env.shop[shoppy].greed;

    FixedArray < int, 4, 50 > shop_id;
    FixedVector < int, 20 > shop_items;

    char st_pass[ ITEMNAME_SIZE ] = "";
    unsigned int gp_value = 0;
    char i;
    unsigned char ft;

#ifdef DOS_TERM
    char buffer[4800];
    gettext(1, 1, 80, 25, buffer);
#endif

#ifdef DOS_TERM
    window(1, 1, 80, 25);
#endif

    clrscr();
    int itty = 0;

    snprintf( info, INFO_SIZE, "Welcome to %s!", 
             shop_name(env.shop[shoppy].x, env.shop[shoppy].y) );

    shop_print(info, 20);

    more3();
    shop_init_id(shoppy, shop_id);

    /* *************************************
    THINGS TO DO:
      Allow inventory
      Remove id change for antique shops
      selling?
    ************************************* */

    save_id(id);

  print_stock:
    clrscr();
    itty = igrd[0][5 + shoppy];

    if (itty == NON_ITEM)
    {
        shop_print("I'm sorry, my shop is empty now.", 20);
        more3();
        goto goodbye;
    }

    for (i = 1; i < 20; i++)
    {
        shop_items[i - 1] = itty;

        if (itty == NON_ITEM)   //mitm.link [itty] == NON_ITEM)
        {
            shop_items[i - 1] = NON_ITEM;
            continue;
        }

        itty = mitm[itty].link;
    }

    itty = igrd[0][5 + shoppy];

    for (i = 1; i < 18; i++)
    {
        gotoxy(1, i);

        textcolor((i % 2) ? WHITE : LIGHTGREY);

        it_name(itty, DESC_NOCAP_A, st_pass);
        putch(i + 96);
        cprintf(" - ");
        cprintf(st_pass);

        gp_value = greedy * item_value( mitm[itty], id );
        gp_value /= 10;
        if (gp_value <= 1)
            gp_value = 1;

        gotoxy(60, i);
        // cdl - itoa(gp_value, st_pass, 10);
        snprintf(st_pass, sizeof(st_pass), "%5d", gp_value);
        cprintf(st_pass);
        cprintf(" gold");
        if (mitm[itty].link == NON_ITEM)
            break;

        itty = mitm[itty].link;
    }

    textcolor(LIGHTGREY);

    shop_print("Type letter to buy item, x/Esc to leave, ?/* for inventory, v to examine.", 23);

  purchase:
    snprintf( info, INFO_SIZE, "You have %d gold piece%s.", you.gold,
             (you.gold == 1) ? "" : "s" );

    textcolor(YELLOW);
    shop_print(info, 19);

    textcolor(CYAN);
    shop_print("What would you like to purchase?", 20);
    textcolor(LIGHTGREY);

    ft = get_ch();

    if (ft == 'x' || ft == ESCAPE)
        goto goodbye;

    if (ft == 'v')
    {
        textcolor(CYAN);
        shop_print("Examine which item?", 20);
        textcolor(LIGHTGREY);
        ft = get_ch();

        // wonder whether this should be recoded to permit uppercase, too? {dlb}
        if (ft < 'a' || ft > 'z')
            goto huh;

        ft -= 'a';              // see above comment {dlb}

        if (ft > 18)
            goto huh;

        if (shop_items[ft] == NON_ITEM)
        {
            shop_print("I'm sorry, you seem to be confused.", 20);
            more3();
            goto purchase;
        }

        describe_item( mitm[shop_items[ft]] );

        goto print_stock;
    }

    if (ft == '?' || ft == '*')
    {
        shop_uninit_id(shoppy, shop_id);
        invent(-1, false);
        shop_init_id(shoppy, shop_id);
#ifdef DOS_TERM
        window(1, 1, 80, 25);
#endif
        goto print_stock;
    }

    if (ft < 'a' || ft > 'z')   // see earlier comments re: uppercase {dlb}
    {
      huh:
        shop_print("Huh?", 20);
        more3();
        goto purchase;
    }

    ft -= 'a';                  // see earlier comments re: uppercase {dlb}

    if (ft > 18)
        goto huh;

    if (shop_items[ft] == NON_ITEM)
    {
        shop_print("I'm sorry, you seem to be confused.", 20);
        more3();
        goto purchase;
    }

    gp_value = greedy * item_value( mitm[shop_items[ft]], id ) / 10;

    if (gp_value > you.gold)
    {
        shop_print("I'm sorry, you don't seem to have enough money.", 20);
        more3();
        goto purchase;
    }

    shop_set_ident_type( shoppy, shop_id, mitm[shop_items[ft]].base_type,
                         mitm[shop_items[ft]].sub_type );

    purchase( shoppy, shop_items[ft], gp_value );

    goto print_stock;

  goodbye:
    //clear_line();
    shop_print("Goodbye!", 20);
    more3();

#ifdef DOS_TERM
    puttext(1, 1, 80, 25, buffer);
    gotoxy(1, 1);
    cprintf(" ");
#endif

    shop_uninit_id( shoppy, shop_id );
    return 0;
}

void shop_init_id(int i, FixedArray < int, 4, 50 > &shop_id)
{
    unsigned char j = 0;

    if (env.shop[i].type != SHOP_WEAPON_ANTIQUE
        && env.shop[i].type != SHOP_ARMOUR_ANTIQUE
        && env.shop[i].type != SHOP_GENERAL_ANTIQUE)
    {
        for (j = 0; j < 50; j++)
        {
            shop_id[ IDTYPE_WANDS ][j] = get_ident_type(OBJ_WANDS, j);
            set_ident_type(OBJ_WANDS, j, ID_KNOWN_TYPE);

            shop_id[ IDTYPE_SCROLLS ][j] = get_ident_type(OBJ_SCROLLS, j);
            set_ident_type(OBJ_SCROLLS, j, ID_KNOWN_TYPE);

            shop_id[ IDTYPE_JEWELLERY ][j] = get_ident_type(OBJ_JEWELLERY, j);
            set_ident_type(OBJ_JEWELLERY, j, ID_KNOWN_TYPE);

            shop_id[ IDTYPE_POTIONS ][j] = get_ident_type(OBJ_POTIONS, j);
            set_ident_type(OBJ_POTIONS, j, ID_KNOWN_TYPE);
        }
    }
}

void shop_uninit_id(int i, FixedArray < int, 4, 50 > &shop_id)
{
    unsigned char j = 0;

    if (env.shop[i].type != SHOP_WEAPON_ANTIQUE
        && env.shop[i].type != SHOP_ARMOUR_ANTIQUE
        && env.shop[i].type != SHOP_GENERAL_ANTIQUE)
    {
        for (j = 0; j < 50; j++)
        {
            set_ident_type( OBJ_WANDS, j, shop_id[ IDTYPE_WANDS ][j], true );
            set_ident_type( OBJ_SCROLLS, j, shop_id[ IDTYPE_SCROLLS ][j], true );
            set_ident_type( OBJ_JEWELLERY, j, shop_id[ IDTYPE_JEWELLERY ][j], true );
            set_ident_type( OBJ_POTIONS, j, shop_id[ IDTYPE_POTIONS ][j], true );
        }
    }
}

void shop_set_ident_type( int i, FixedArray < int, 4, 50 > &shop_id,
                          unsigned char base_type, unsigned char sub_type )
{
    if (env.shop[i].type != SHOP_WEAPON_ANTIQUE
        && env.shop[i].type != SHOP_ARMOUR_ANTIQUE
        && env.shop[i].type != SHOP_GENERAL_ANTIQUE)
    {
        switch (base_type)
        {
        case OBJ_WANDS:
            shop_id[ IDTYPE_WANDS ][sub_type] = ID_KNOWN_TYPE;
            break;
        case OBJ_SCROLLS:
            shop_id[ IDTYPE_SCROLLS ][sub_type] = ID_KNOWN_TYPE;
            break;
        case OBJ_JEWELLERY:
            shop_id[ IDTYPE_JEWELLERY ][sub_type] = ID_KNOWN_TYPE;
            break;
        case OBJ_POTIONS:
            shop_id[ IDTYPE_POTIONS ][sub_type] = ID_KNOWN_TYPE;
            break;
        }
    }
}

void shop_print( const char *shoppy, char sh_lines )
{
    gotoxy(1, sh_lines);

    cprintf(shoppy);

    for (int i = strlen(shoppy); i < 80; i++)
        cprintf(" ");
}

char more3(void)
{
    char keyin = 0;

    gotoxy(70, 20);
    cprintf("-more-");
    keyin = getch();
    if (keyin == 0)
        getch();
    //clear_line();
    return keyin;
}

static void purchase( int shop, int item_got, int cost )
{
    you.gold -= cost;

    int num = move_item_to_player( item_got, mitm[item_got].quantity, true );

    // Shopkeepers will now place goods you can't carry outside the shop.
    if (num < mitm[item_got].quantity)
    {
        snprintf( info, INFO_SIZE, "I'll put %s outside for you.",
                 (mitm[item_got].quantity == 1) ? "it" :
                 (num > 0)                      ? "the rest" 
                                                : "these" );

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
    FixedVector< char, RA_PROPERTIES >  prop;

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

    // magic resistance is from 20-120
    if (prop[ RAP_MAGIC ])
        ret += 5 + prop[ RAP_MAGIC ] / 10;

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

unsigned int item_value( item_def item, char id[4][50], bool ident )
{
    // Note that we pass item in by value, since we want a local
    // copy to mangle as neccessary... maybe that should be fixed,
    // but this function isn't called too often.
    item.flags = (ident) ? (item.flags | ISFLAG_IDENT_MASK) : (item.flags);

    int valued = 0;
    int charge_value = 0;

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

        case WPN_GREAT_FLAIL:
            valued += 75;
            break;

        case WPN_EVENINGSTAR:
            valued += 65;
            break;

        case WPN_EXECUTIONERS_AXE:
            valued += 100;
            break;

        case WPN_DOUBLE_SWORD:
            valued += 200;
            break;

        case WPN_DEMON_WHIP:
            valued += 230;
            break;

        case WPN_QUICK_BLADE:
        case WPN_DEMON_TRIDENT:
            valued += 250;
            break;

        case WPN_KATANA:
        case WPN_TRIPLE_SWORD:
        case WPN_DEMON_BLADE:
            valued += 300;
            break;
        }

        if (item_ident( item, ISFLAG_KNOW_TYPE ))
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

            case SPWPN_DISRUPTION:
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
        if (cmp_equip_race( item, ISFLAG_ELVEN ) 
                || cmp_equip_race( item, ISFLAG_DWARVEN ))
        {
            valued *= 12;
            valued /= 10;
        }

        // value was "6" but comment read "orc", so I went with comment {dlb}
        if (cmp_equip_race( item, ISFLAG_ORCISH ))
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
            if (item_ident( item, ISFLAG_KNOW_TYPE ))
                valued += (7 * randart_value( item ));
            else 
                valued += 50;
        }
        else if (item_ident( item, ISFLAG_KNOW_TYPE ) 
                && !cmp_equip_desc( item, 0 ))
        {
            valued += 20;
        }

        if (item_cursed( item ))
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
        case MI_EGGPLANT:
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

        if (item_ident( item, ISFLAG_KNOW_TYPE ))
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

        if (cmp_equip_race( item, ISFLAG_ELVEN ) 
                || cmp_equip_race( item, ISFLAG_DWARVEN ))
        {
            valued *= 12;
            valued /= 10;
        }

        if (cmp_equip_race( item, ISFLAG_ORCISH ))
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
            if (item_ident( item, ISFLAG_KNOW_TYPE ))
                valued += (7 * randart_value( item ));
            else
                valued += 50;
        }
        else if (item_ident( item, ISFLAG_KNOW_TYPE ) 
                && !cmp_equip_desc( item, 0 ))
        {
            valued += 20;
        }

        if (item_cursed( item ))
        {
            valued *= 6;
            valued /= 10;
        }
        break;

    case OBJ_WANDS:
        charge_value = 0;
        if (id[0][item.sub_type])
        {
            switch (item.sub_type)
            {
            case WAND_FIREBALL:
            case WAND_LIGHTNING:
                valued += 20;
                charge_value += 5;
                break;

            case WAND_DRAINING:
                valued += 20;
                charge_value += 4;
                break;

            case WAND_DISINTEGRATION:
                valued += 17;
                charge_value += 4;
                break;

            case WAND_POLYMORPH_OTHER:
                valued += 15;
                charge_value += 4;
                break;

            case WAND_COLD:
            case WAND_ENSLAVEMENT:
            case WAND_FIRE:
            case WAND_HASTING:
                valued += 15;
                charge_value += 3;
                break;

            case WAND_INVISIBILITY:
                valued += 15;
                charge_value += 2;
                break;

            case WAND_RANDOM_EFFECTS:
                valued += 13;
                charge_value += 3;
                break;

            case WAND_PARALYSIS:
                valued += 12;
                charge_value += 3;
                break;

            case WAND_SLOWING:
                valued += 10;
                charge_value += 3;
                break;

            case WAND_CONFUSION:
            case WAND_DIGGING:
            case WAND_TELEPORTATION:
                valued += 10;
                charge_value += 2;
                break;

            case WAND_HEALING:
                valued += 7;
                charge_value += 3;
                break;

            case WAND_FLAME:
            case WAND_FROST:
                valued += 5;
                charge_value += 2;
                break;

            case WAND_MAGIC_DARTS:
                valued += 3;
                charge_value++;
                break;

            default:            // no default charge_value ??? 15jan2000 {dlb}
                valued += 10;
                break;
            }

            if (item_ident( item, ISFLAG_KNOW_PLUSES ))
            {
                valued += item.plus * charge_value;
            }

            valued *= 3;

            if (item.plus == 0)
                valued = 3;     // change if wands are rechargeable!
        }
        else
            valued = 35;        // = 10;
        break;

    case OBJ_POTIONS:
        if (!id[3][item.sub_type])
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
        if (!id[1][item.sub_type])
            valued += 10;
        else
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
            case SCR_NOISE:
            case SCR_RANDOM_USELESSNESS:
                valued += 2;
                break;
            case SCR_CURSE_ARMOUR:
            case SCR_CURSE_WEAPON:
            case SCR_FORGETFULNESS:
            case SCR_PAPER:
            case SCR_IMMOLATION:
                valued++;
                break;
            }
        break;

    case OBJ_JEWELLERY:
        if (!id[2][item.sub_type])
            valued += 50;

        if (item_cursed( item ))
            valued -= 10;

        if (id[2][item.sub_type] > 0)
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
                    valued += 10 * item.plus;
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
            case RING_TELEPORTATION:
                valued -= 10;
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
                if (item_ident(item, ISFLAG_KNOW_TYPE))
                {
                    if (valued < 0)
                        valued = randart_value( item ) - 5;
                    else
                        valued += randart_value( item );
                }
                else
                {
                    valued += 50;
                }
            }

            valued *= 7;
        }
        break;

    case OBJ_MISCELLANY:
        if (item_ident( item, ISFLAG_KNOW_TYPE ))
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
            case MISC_DECK_OF_TRICKS:
                valued += 100;
                break;
            default:
                valued += 400;
            }
        }
        break;

    //case 10: break;

    case OBJ_BOOKS:
        valued = 150 + (item_ident( item, ISFLAG_KNOW_TYPE ) 
                                    ? book_rarity(item.sub_type) * 50 : 0);
        break;

    case OBJ_STAVES:
        if (item_not_ident( item, ISFLAG_KNOW_TYPE ))
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
        break;

    case OBJ_ORBS:
        valued = 250000;
        break;
    }                           // end switch

    if (valued < 1)
        valued = 1;

    valued *= item.quantity;

    return (valued);
}                               // end item_value()

void shop(void)
{
    unsigned char i = 0;

    for (i = 0; i < MAX_SHOPS; i++)
    {
        if (env.shop[i].x == you.x_pos && env.shop[i].y == you.y_pos)
            break;
    }

    if (i == MAX_SHOPS)
    {
        mpr("Help! Non-existent shop.");
        return;
    }

    char identy[4][50];

    save_id(identy);

    in_a_shop(i, identy);
    you.redraw_gold = 1;
    burden_change();

    redraw_screen();
}                               // end shop()

const char *shop_name(int sx, int sy)
{
    static char sh_name[80];
    int shoppy;

    // paranoia
    if (grd[sx][sy] != DNGN_ENTER_SHOP)
        return ("");

    // find shop
    for(shoppy = 0; shoppy < MAX_SHOPS; shoppy ++)
    {
        // find shop index plus a little bit of paranoia
        if (env.shop[shoppy].x == sx && env.shop[shoppy].y == sy &&
            env.shop[shoppy].type != SHOP_UNASSIGNED)
        {
            break;
        }
    }

    if (shoppy == MAX_SHOPS)
    {
        mpr("Help! Non-existent shop.");
        return ("Buggy Shop");
    }

    int shop_type = env.shop[shoppy].type;

    char st_p[ITEMNAME_SIZE];

    make_name( env.shop[shoppy].keeper_name[0], env.shop[shoppy].keeper_name[1],
               env.shop[shoppy].keeper_name[2], 3, st_p );

    strcpy(sh_name, st_p);
    strcat(sh_name, "'s ");

    if (shop_type == SHOP_WEAPON_ANTIQUE || shop_type == SHOP_ARMOUR_ANTIQUE)
        strcat( sh_name, "Antique " );

    strcat(sh_name, (shop_type == SHOP_WEAPON
                     || shop_type == SHOP_WEAPON_ANTIQUE) ? "Weapon" :
                    (shop_type == SHOP_ARMOUR
                     || shop_type == SHOP_ARMOUR_ANTIQUE) ? "Armour" :

                    (shop_type == SHOP_JEWELLERY)         ? "Jewellery" :
                    (shop_type == SHOP_WAND)              ? "Magical Wand" :
                    (shop_type == SHOP_BOOK)              ? "Book" :
                    (shop_type == SHOP_FOOD)              ? "Food" :
                    (shop_type == SHOP_SCROLL)            ? "Magic Scroll" :
                    (shop_type == SHOP_GENERAL_ANTIQUE) ? "Assorted Antiques" :
                    (shop_type == SHOP_DISTILLERY)        ? "Distillery" :
                    (shop_type == SHOP_GENERAL)           ? "General Store"
                                                          : "Bug");


    if (shop_type != SHOP_GENERAL
        && shop_type != SHOP_GENERAL_ANTIQUE && shop_type != SHOP_DISTILLERY)
    {
        int temp = sx + sy % 4;
        strcat( sh_name, (temp == 0) ? " Shoppe" :
                         (temp == 1) ? " Boutique" :
                         (temp == 2) ? " Emporium"
                                     : " Shop" );
    }

    return (sh_name);
}
