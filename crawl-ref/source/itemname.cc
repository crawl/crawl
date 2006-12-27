/*
 *  File:       itemname.cc
 *  Summary:    Misc functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <4>      9/09/99        BWR             Added hands_required function
 *      <3>      6/13/99        BWR             Upped the base AC for heavy armour
 *      <2>      5/20/99        BWR             Extended screen lines support
 *      <1>      -/--/--        LRH             Created
 */

#include "AppHdr.h"
#include "itemname.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "invent.h"
#include "itemprop.h"
#include "macro.h"
#include "mon-util.h"
#include "notes.h"
#include "randart.h"
#include "skills2.h"
#include "stuff.h"
#include "view.h"
#include "items.h"

char id[NUM_IDTYPE][MAX_SUBTYPES];

static bool is_random_name_space( char let );
static bool is_random_name_vowel( char let);
static const char *item_name_2(
        const item_def &item, char buff[ ITEMNAME_SIZE ], bool terse );

static char retvow(int sed);
static char retlet(int sed);

// [dshaligram] For calls to item_name without a pre-allocated buffer.
static char default_itembuf[ITEMNAME_SIZE];

static bool is_tried_type( int basetype, int subtype )
{
    switch ( basetype )
    {
    case OBJ_SCROLLS:
        return id[IDTYPE_SCROLLS][subtype] == ID_TRIED_TYPE;
    case OBJ_POTIONS:
        return id[IDTYPE_POTIONS][subtype] == ID_TRIED_TYPE;
    case OBJ_WANDS:
        return id[IDTYPE_WANDS][subtype] == ID_TRIED_TYPE;
    default:
        return false;
    }
}


bool is_vowel( const char chr )
{
    const char low = tolower( chr );

    return (low == 'a' || low == 'e' || low == 'i' || low == 'o' || low == 'u');
}

bool item_type_known( const item_def &item )
{
    return item_ident(item, ISFLAG_KNOW_TYPE)
        || (item.base_type == OBJ_JEWELLERY
                && id[IDTYPE_JEWELLERY][item.sub_type] == ID_KNOWN_TYPE);
}

// it_name() and in_name() are now somewhat obsolete now that itemname
// takes item_def, so consider them deprecated.
const char *it_name( int itn, char des, char *buff, bool terse )
{
    return item_name( mitm[itn], des, buff, terse );
}                               // end it_name()


const char *in_name( int inn, char des, char *buff, bool terse )
{
    return item_name( you.inv[inn], des, buff, terse );
}                               // end in_name()

// quant_name is usful since it prints out a different number of items
// than the item actually contains.
const char *quant_name( const item_def &item, int quant, char des,
                 char buff[ ITEMNAME_SIZE ], bool terse )
{
    // item_name now requires a "real" item, so we'll mangle a tmp
    item_def tmp = item;
    tmp.quantity = quant;

    return item_name( tmp, des, buff, terse );
}                               // end quant_name()

// buff must be at least ITEMNAME_SIZE if non-NULL. If NULL, a static
// item buffer will be used.
const char *item_name( const item_def &item, char descrip, 
                       char *buff, bool terse )
{
    const int item_clas = item.base_type;
    const int item_typ = item.sub_type;
    const int it_quant = item.quantity;
    
    char tmp_quant[20];
    char itm_name[  ITEMNAME_SIZE  ] = "";

    if (!buff)
        buff = default_itembuf;

    item_name_2( item, itm_name, terse );

    buff[0] = '\0';

    if (descrip == DESC_INVENTORY_EQUIP || descrip == DESC_INVENTORY) 
    {
        if (in_inventory(item)) // actually in inventory
            snprintf( buff, ITEMNAME_SIZE, (terse) ? "%c) " : "%c - ",
                      index_to_letter( item.link ) );
        else 
            descrip = DESC_CAP_A;
    }

    if (terse)
        descrip = DESC_PLAIN;

    if (item_clas == OBJ_ORBS
        || (item_ident( item, ISFLAG_KNOW_TYPE )
            && ((item_clas == OBJ_MISCELLANY
                    && item_typ == MISC_HORN_OF_GERYON)
                || (is_fixed_artefact( item )
                || (is_random_artefact( item ))))))
    {
        // artefacts always get "the" unless we just want the plain name
        switch (descrip)
        {
        case DESC_CAP_A:
        case DESC_CAP_YOUR:
        case DESC_CAP_THE:
            strncat(buff, "The ", ITEMNAME_SIZE );
            break;
        case DESC_NOCAP_A:
        case DESC_NOCAP_YOUR:
        case DESC_NOCAP_THE:
        case DESC_NOCAP_ITS:
        case DESC_INVENTORY_EQUIP:
        case DESC_INVENTORY:
            strncat(buff, "the ", ITEMNAME_SIZE );
            break;
        default:
        case DESC_PLAIN:
            break;
        }
    }
    else if (it_quant > 1)
    {
        switch (descrip)
        {
        case DESC_CAP_THE:
            strncat(buff, "The ", ITEMNAME_SIZE );
            break;
        case DESC_NOCAP_THE:
            strncat(buff, "the ", ITEMNAME_SIZE );
            break;
        case DESC_CAP_A:
        case DESC_NOCAP_A:
        case DESC_INVENTORY_EQUIP:
        case DESC_INVENTORY:
            break;
        case DESC_CAP_YOUR:
            strncat(buff, "Your ", ITEMNAME_SIZE );
            break;
        case DESC_NOCAP_YOUR:
            strncat(buff, "your ", ITEMNAME_SIZE );
            break;
        case DESC_NOCAP_ITS:
            strncat(buff, "its ", ITEMNAME_SIZE );
            break;
        case DESC_PLAIN:
        default:
            break;
        }

        itoa(it_quant, tmp_quant, 10);
        strncat(buff, tmp_quant, ITEMNAME_SIZE );
        strncat(buff, " ", ITEMNAME_SIZE );
    }
    else
    {
        switch (descrip)
        {
        case DESC_CAP_THE:
            strncat(buff, "The ", ITEMNAME_SIZE );
            break;
        case DESC_NOCAP_THE:
            strncat(buff, "the ", ITEMNAME_SIZE );
            break;
        case DESC_CAP_A:
            strncat(buff, "A", ITEMNAME_SIZE );

            if (itm_name[0] == 'a' || itm_name[0] == 'e' || itm_name[0] == 'i'
                || itm_name[0] == 'o' || itm_name[0] == 'u')
            {
                strncat(buff, "n", ITEMNAME_SIZE );
            }

            strncat(buff, " ", ITEMNAME_SIZE );
            break;              // A/An

        case DESC_NOCAP_A:
        case DESC_INVENTORY_EQUIP:
        case DESC_INVENTORY:
            strncat(buff, "a", ITEMNAME_SIZE );

            if (itm_name[0] == 'a' || itm_name[0] == 'e' || itm_name[0] == 'i'
                || itm_name[0] == 'o' || itm_name[0] == 'u')
            {
                strncat(buff, "n", ITEMNAME_SIZE );
            }

            strncat(buff, " ", ITEMNAME_SIZE );
            break;              // a/an

        case DESC_CAP_YOUR:
            strncat(buff, "Your ", ITEMNAME_SIZE );
            break;
        case DESC_NOCAP_YOUR:
            strncat(buff, "your ", ITEMNAME_SIZE );
            break;
        case DESC_NOCAP_ITS:
            strncat(buff, "its ", ITEMNAME_SIZE );
            break;
        case DESC_PLAIN:
        default:
            break;
        }
    }                           // end of else

    strncat(buff, itm_name, ITEMNAME_SIZE );

    if (descrip == DESC_INVENTORY_EQUIP && item.x == -1 && item.y == -1)
    {
        ASSERT( item.link != -1 );

        if (item.link == you.equip[EQ_WEAPON])
        {
            if (you.inv[ you.equip[EQ_WEAPON] ].base_type == OBJ_WEAPONS
                || item_is_staff( you.inv[ you.equip[EQ_WEAPON] ] ))
            {
                strncat( buff, " (weapon)", ITEMNAME_SIZE );
            }
            else
            {
                strncat( buff, " (in hand)", ITEMNAME_SIZE );
            }
        }
        else if (item.link == you.equip[EQ_CLOAK]
                || item.link == you.equip[EQ_HELMET]
                || item.link == you.equip[EQ_GLOVES]
                || item.link == you.equip[EQ_BOOTS]
                || item.link == you.equip[EQ_SHIELD]
                || item.link == you.equip[EQ_BODY_ARMOUR])
        {
            strncat( buff, " (worn)", ITEMNAME_SIZE );
        }
        else if (item.link == you.equip[EQ_LEFT_RING])
        {
            strncat( buff, " (left hand)", ITEMNAME_SIZE );
        }
        else if (item.link == you.equip[EQ_RIGHT_RING])
        {
            strncat( buff, " (right hand)", ITEMNAME_SIZE );
        }
        else if (item.link == you.equip[EQ_AMULET])
        {
            strncat( buff, " (around neck)", ITEMNAME_SIZE );
        }
    }

    if ( !item.inscription.empty() )
    {
        strncat( buff, " {", 2 );
        if ( is_tried_type(item.base_type, item.sub_type) )
            strncat(buff, "tried, ", 10);
        strncat( buff, item.inscription.c_str(), ITEMNAME_SIZE );
        strncat( buff, "}", 1 );
    }
    else if ( is_tried_type(item.base_type, item.sub_type) )
        strncat(buff, " {tried}", 10);

    return (buff);
}                               // end item_name()


// Note that "terse" is only currently used for the "in hand" listing on
// the game screen.
static const char *item_name_2(
        const item_def &item, 
        char buff[ITEMNAME_SIZE],
        bool terse )
{
    const int item_clas = item.base_type;
    const int item_typ = item.sub_type;
    const int it_plus = item.plus;
    const int item_plus2 = item.plus2;
    const int it_quant = item.quantity;

    char tmp_quant[20];
    char tmp_buff[ITEMNAME_SIZE];
    int  brand;
    unsigned char sparm;

    buff[0] = 0;

    switch (item_clas)
    {
    case OBJ_WEAPONS:
        if (item_ident( item, ISFLAG_KNOW_CURSE ) && !terse)
        {
            // We don't bother printing "uncursed" if the item is identified
            // for pluses (it's state should be obvious), this is so that
            // the weapon name is kept short (there isn't a lot of room
            // for the name on the main screen).  If you're going to change
            // this behaviour, *please* make it so that there is an option
            // that maintains this behaviour. -- bwr
            if (item_cursed( item ))
                strncat(buff, "cursed ", ITEMNAME_SIZE );
            else if (Options.show_uncursed 
                    && !item_ident( item, ISFLAG_KNOW_PLUSES ))
            {
                strncat(buff, "uncursed ", ITEMNAME_SIZE );
            }
        }

        if (item_ident( item, ISFLAG_KNOW_PLUSES ))
        {
            if (it_plus == 0 && item_plus2 == 0)
                strncat(buff, "+0 ", ITEMNAME_SIZE );
            else
            {
                if (it_plus >= 0)
                    strncat( buff, "+" , ITEMNAME_SIZE );

                itoa( it_plus, tmp_quant, 10 );
                strncat( buff, tmp_quant , ITEMNAME_SIZE );

                if (it_plus != item_plus2 || !terse)
                {
                    strncat( buff, "," , ITEMNAME_SIZE );

                    if (item_plus2 >= 0)
                        strncat(buff, "+", ITEMNAME_SIZE );

                    itoa( item_plus2, tmp_quant, 10 );
                    strncat( buff, tmp_quant , ITEMNAME_SIZE );
                }

                strncat( buff, " " , ITEMNAME_SIZE );
            }
        }

        if (is_random_artefact( item ))
        {
            strncat( buff, randart_name(item), ITEMNAME_SIZE );
            break;
        }

        if (is_fixed_artefact( item ))
        {
            if (item_ident( item, ISFLAG_KNOW_TYPE ))
            {
                strncat(buff, 
                       (item.special == SPWPN_SINGING_SWORD) ? "Singing Sword" :
                       (item.special == SPWPN_WRATH_OF_TROG) ? "Wrath of Trog" :
                       (item.special == SPWPN_SCYTHE_OF_CURSES) ? "Scythe of Curses" :
                       (item.special == SPWPN_MACE_OF_VARIABILITY) ? "Mace of Variability" :
                       (item.special == SPWPN_GLAIVE_OF_PRUNE) ? "Glaive of Prune" :
                       (item.special == SPWPN_SCEPTRE_OF_TORMENT) ? "Sceptre of Torment" :
                       (item.special == SPWPN_SWORD_OF_ZONGULDROK) ? "Sword of Zonguldrok" :
                       (item.special == SPWPN_SWORD_OF_CEREBOV) ? "Sword of Cerebov" :
                       (item.special == SPWPN_STAFF_OF_DISPATER) ? "Staff of Dispater" :
                       (item.special == SPWPN_SCEPTRE_OF_ASMODEUS) ? "Sceptre of Asmodeus" :
                       (item.special == SPWPN_SWORD_OF_POWER) ? "Sword of Power" :
                       (item.special == SPWPN_KNIFE_OF_ACCURACY) ? "Knife of Accuracy" :
                       (item.special == SPWPN_STAFF_OF_OLGREB) ? "Staff of Olgreb" :
                       (item.special == SPWPN_VAMPIRES_TOOTH) ? "Vampire's Tooth" :
                       (item.special == SPWPN_STAFF_OF_WUCAD_MU) ? "Staff of Wucad Mu"
                                                   : "Brodale's Buggy Bola", 
                                                   ITEMNAME_SIZE );
            }
            else
            {
                strncat(buff, 
                       (item.special == SPWPN_SINGING_SWORD) ? "golden long sword" :
                       (item.special == SPWPN_WRATH_OF_TROG) ? "bloodstained battleaxe" :
                       (item.special == SPWPN_SCYTHE_OF_CURSES) ? "warped scythe" :
                       (item.special == SPWPN_MACE_OF_VARIABILITY) ? "shimmering mace" :
                       (item.special == SPWPN_GLAIVE_OF_PRUNE) ? "purple glaive" :
                       (item.special == SPWPN_SCEPTRE_OF_TORMENT) ? "jeweled golden mace" :
                       (item.special == SPWPN_SWORD_OF_ZONGULDROK) ? "bone long sword" :
                       (item.special == SPWPN_SWORD_OF_CEREBOV) ? "great serpentine sword" :
                       (item.special == SPWPN_STAFF_OF_DISPATER) ? "golden staff" :
                       (item.special == SPWPN_SCEPTRE_OF_ASMODEUS) ? "ruby sceptre" :
                       (item.special == SPWPN_SWORD_OF_POWER) ? "chunky great sword" :
                       (item.special == SPWPN_KNIFE_OF_ACCURACY) ? "thin dagger" :
                       (item.special == SPWPN_STAFF_OF_OLGREB) ? "green glowing staff" :
                       (item.special == SPWPN_VAMPIRES_TOOTH) ? "ivory dagger" :
                       (item.special == SPWPN_STAFF_OF_WUCAD_MU) ? "ephemeral quarterstaff"
                                                           : "buggy bola", 
                                                           ITEMNAME_SIZE );
            }
            break;
        }

        // Now that we can have "glowing elven" weapons, it's 
        // probably a good idea to cut out the descriptive
        // term once it's become obsolete. -- bwr
        if (!item_ident( item, ISFLAG_KNOW_PLUSES ) && !terse)
        {
            switch (get_equip_desc( item ))
            {
            case ISFLAG_RUNED:
                strncat(buff, "runed ", ITEMNAME_SIZE );
                break;
            case ISFLAG_GLOWING:
                strncat(buff, "glowing ", ITEMNAME_SIZE );
                break;
            }
        } 

        // always give racial type (it does have game effects)

        switch (get_equip_race( item ))
        {
        case ISFLAG_ORCISH:
            strncat( buff, (terse) ? "orc " : "orcish ", ITEMNAME_SIZE );
            break;
        case ISFLAG_ELVEN:
            strncat( buff, (terse) ? "elf " : "elven ", ITEMNAME_SIZE );
            break;
        case ISFLAG_DWARVEN:
            strncat( buff, (terse) ? "dwarf " : "dwarven ", ITEMNAME_SIZE );
            break;
        }

        brand = get_weapon_brand( item );

        if (item_ident( item, ISFLAG_KNOW_TYPE ) && !terse)
        {
            if (brand == SPWPN_VAMPIRICISM)
                strncat(buff, "vampiric ", ITEMNAME_SIZE );
        }                       // end if

        standard_name_weap( item_typ, tmp_buff );
        strncat( buff, tmp_buff, ITEMNAME_SIZE );

        if (item_ident( item, ISFLAG_KNOW_TYPE ))
        {
            switch (brand)
            {
            case SPWPN_NORMAL:
                break;
            case SPWPN_FLAMING:
                strncat(buff, (terse) ? " (flame)" : " of flaming", ITEMNAME_SIZE );
                break;
            case SPWPN_FREEZING:
                strncat(buff, (terse) ? " (freeze)" : " of freezing", ITEMNAME_SIZE );
                break;
            case SPWPN_HOLY_WRATH:
                strncat(buff, (terse) ? " (holy)" : " of holy wrath", ITEMNAME_SIZE );
                break;
            case SPWPN_ELECTROCUTION:
                strncat(buff, (terse) ? " (elec)" : " of electrocution", ITEMNAME_SIZE );
                break;
            case SPWPN_ORC_SLAYING:
                strncat(buff, (terse) ? " (slay orc)" : " of orc slaying", ITEMNAME_SIZE );
                break;
            case SPWPN_VENOM:
                strncat(buff, (terse) ? " (venom)" : " of venom", ITEMNAME_SIZE );
                break;
            case SPWPN_PROTECTION:
                strncat(buff, (terse) ? " (protect)" : " of protection", ITEMNAME_SIZE );
                break;
            case SPWPN_DRAINING:
                strncat(buff, (terse) ? " (drain)" : " of draining", ITEMNAME_SIZE );
                break;
            case SPWPN_SPEED:
                strncat(buff, (terse) ? " (speed)" : " of speed", ITEMNAME_SIZE );
                break;
            case SPWPN_VORPAL:
                if (is_range_weapon( item ))
                {
                    strncat(buff, (terse) ? " (velocity)" : " of velocity", 
                            ITEMNAME_SIZE );
                    break;
                }

                switch (get_vorpal_type(item))
                {
                case DVORP_CRUSHING:
                    strncat(buff, (terse) ? " (crush)" : " of crushing", ITEMNAME_SIZE );
                    break;
                case DVORP_SLICING:
                    strncat(buff, (terse) ? " (slice)" : " of slicing", ITEMNAME_SIZE );
                    break;
                case DVORP_PIERCING:
                    strncat(buff, (terse) ? " (pierce)" : " of piercing", ITEMNAME_SIZE );
                    break;
                case DVORP_CHOPPING:
                    strncat(buff, (terse) ? " (chop)" : " of chopping", ITEMNAME_SIZE );
                    break;
                case DVORP_SLASHING:
                    strncat(buff, (terse) ? " (slash)" : " of slashing", ITEMNAME_SIZE );
                    break;
                case DVORP_STABBING:
                    strncat(buff, (terse) ? " (stab)" : " of stabbing", ITEMNAME_SIZE );
                    break;
                }
                break;

            case SPWPN_FLAME:
                strncat(buff, (terse) ? " (flame)" : " of flame", ITEMNAME_SIZE );
                break;          // bows/xbows

            case SPWPN_FROST:
                strncat(buff, (terse) ? " (frost)" : " of frost", ITEMNAME_SIZE );
                break;          // bows/xbows

            case SPWPN_VAMPIRICISM:
                if (terse)
                    strncat( buff, " (vamp)", ITEMNAME_SIZE );
                break;

            case SPWPN_DISRUPTION:
                strncat(buff, (terse) ? " (disrupt)" : " of disruption", ITEMNAME_SIZE );
                break;
            case SPWPN_PAIN:
                strncat(buff, (terse) ? " (pain)" : " of pain", ITEMNAME_SIZE );
                break;
            case SPWPN_DISTORTION:
                strncat(buff, (terse) ? " (distort)" : " of distortion", ITEMNAME_SIZE );
                break;

            case SPWPN_REACHING:
                strncat(buff, (terse) ? " (reach)" : " of reaching", ITEMNAME_SIZE );
                break;

                /* 25 - 29 are randarts */
            }
        }

        if (item_ident(item, ISFLAG_KNOW_CURSE) && item_cursed(item) && terse)
            strncat( buff, " (curse)", ITEMNAME_SIZE );
        break;

    case OBJ_MISSILES:
        brand = get_ammo_brand( item );  

        if (brand == SPMSL_POISONED)
            strncat( buff, (terse) ? "poison " : "poisoned ", ITEMNAME_SIZE );
        
        if (brand == SPMSL_CURARE)
        {
            strncat( buff, (terse) ? "curare " : "curare-tipped ", ITEMNAME_SIZE);
        }

        if (item_ident( item, ISFLAG_KNOW_PLUSES ))
        {
            if (it_plus >= 0)
                strncat(buff, "+", ITEMNAME_SIZE );

            itoa( it_plus, tmp_quant, 10 );

            strncat(buff, tmp_quant, ITEMNAME_SIZE );
            strncat(buff, " ", ITEMNAME_SIZE );
        }

        if (get_equip_race( item ))
        {
            int dwpn = get_equip_race( item );

            strncat(buff, 
                   (dwpn == ISFLAG_ORCISH)  ? ((terse) ? "orc " : "orcish ") :
                   (dwpn == ISFLAG_ELVEN)   ? ((terse) ? "elf " : "elven ") :
                   (dwpn == ISFLAG_DWARVEN) ? ((terse) ? "dwarf " : "dwarven ")
                                            : "buggy ",
                   ITEMNAME_SIZE);
        }

        strncat(buff, (item_typ == MI_STONE) ? "stone" :
               (item_typ == MI_ARROW) ? "arrow" :
               (item_typ == MI_BOLT) ? "bolt" :
               (item_typ == MI_DART) ? "dart" :
               (item_typ == MI_NEEDLE) ? "needle" :
               (item_typ == MI_LARGE_ROCK) ? "large rock" : 
                                        "hysterical raisin", ITEMNAME_SIZE);
               // this should probably be "" {dlb}

        if (it_quant > 1)
            strncat(buff, "s", ITEMNAME_SIZE );

        if (item_ident( item, ISFLAG_KNOW_TYPE ))
        {
            strncat( buff,
              (brand == SPMSL_FLAME)   ? ((terse) ? " (flame)" : " of flame") :
              (brand == SPMSL_ICE)     ? ((terse) ? " (ice)" : " of ice") :
              (brand == SPMSL_NORMAL)      ? "" :
              (brand == SPMSL_POISONED)    ? "" :
              (brand == SPMSL_CURARE)      ? "" :
                                             " (buggy)", ITEMNAME_SIZE );
        }
        break;

    case OBJ_ARMOUR:
        if (item_ident( item, ISFLAG_KNOW_CURSE ) && !terse)
        {
            if (item_cursed( item ))
                strncat(buff, "cursed ", ITEMNAME_SIZE );
            else if (Options.show_uncursed 
                    && !item_ident( item, ISFLAG_KNOW_PLUSES ))
            {
                strncat(buff, "uncursed ", ITEMNAME_SIZE );
            }
        }

        if (item_ident( item, ISFLAG_KNOW_PLUSES ))
        {
            if (it_plus >= 0)
                strncat(buff, "+", ITEMNAME_SIZE );

            itoa( it_plus, tmp_quant, 10 );

            strncat(buff, tmp_quant, ITEMNAME_SIZE );
            strncat(buff, " ", ITEMNAME_SIZE );
        }

        if (item_typ == ARM_GLOVES || item_typ == ARM_BOOTS)
        {
            strncat( buff, "pair of ", ITEMNAME_SIZE );
        }

        if (is_random_artefact( item ))
        {
            strncat(buff, randart_armour_name(item), ITEMNAME_SIZE);
            break;
        }

        // Now that we can have "glowing elven" armour, it's 
        // probably a good idea to cut out the descriptive
        // term once it's become obsolete. -- bwr
        if (!item_ident( item, ISFLAG_KNOW_PLUSES ) && !terse)
        {
            switch (get_equip_desc( item ))
            {
            case ISFLAG_EMBROIDERED_SHINY:
                if (item_typ == ARM_ROBE || item_typ == ARM_CLOAK
                    || item_typ == ARM_GLOVES || item_typ == ARM_BOOTS)
                {
                    strncat(buff, "embroidered ", ITEMNAME_SIZE );
                }
                else if (item_typ != ARM_LEATHER_ARMOUR 
                        && item_typ != ARM_ANIMAL_SKIN)
                {
                    strncat(buff, "shiny ", ITEMNAME_SIZE );
                }
                break;

            case ISFLAG_RUNED:
                strncat(buff, "runed ", ITEMNAME_SIZE );
                break;

            case ISFLAG_GLOWING:
                strncat(buff, "glowing ", ITEMNAME_SIZE );
                break;
            }
        }

        // always give racial description (has game effects)
        switch (get_equip_race( item ))
        {
        case ISFLAG_ELVEN:
            strncat(buff, (terse) ? "elf " :"elven ", ITEMNAME_SIZE );
            break;
        case ISFLAG_DWARVEN:
            strncat(buff, (terse) ? "dwarf " : "dwarven ", ITEMNAME_SIZE );
            break;
        case ISFLAG_ORCISH:
            strncat(buff, (terse) ? "orc " : "orcish ", ITEMNAME_SIZE );
            break;
        }               // end switch

        standard_name_armour( item, tmp_buff );  // in randart.cc
        strncat( buff, tmp_buff, ITEMNAME_SIZE );

        sparm = get_armour_ego_type( item );

        if (item_ident( item, ISFLAG_KNOW_TYPE ) && sparm != SPARM_NORMAL)
        {
            if (!terse)
            {
                strncat(buff, " of ", ITEMNAME_SIZE );

                strncat(buff, (sparm == SPARM_RUNNING) ? "running" :
                       (sparm == SPARM_FIRE_RESISTANCE) ? "fire resistance" :
                       (sparm == SPARM_COLD_RESISTANCE) ? "cold resistance" :
                       (sparm == SPARM_POISON_RESISTANCE) ? "poison resistance" :
                       (sparm == SPARM_SEE_INVISIBLE) ? "see invisible" :
                       (sparm == SPARM_DARKNESS) ? "darkness" :
                       (sparm == SPARM_STRENGTH) ? "strength" :
                       (sparm == SPARM_DEXTERITY) ? "dexterity" :
                       (sparm == SPARM_INTELLIGENCE) ? "intelligence" :
                       (sparm == SPARM_PONDEROUSNESS) ? "ponderousness" :
                       (sparm == SPARM_LEVITATION) ? "levitation" :
                       (sparm == SPARM_MAGIC_RESISTANCE) ? "magic resistance" :
                       (sparm == SPARM_PROTECTION) ? "protection" :
                       (sparm == SPARM_STEALTH) ? "stealth" :
                       (sparm == SPARM_RESISTANCE) ? "resistance" :
                       (sparm == SPARM_POSITIVE_ENERGY) ? "positive energy" :
                       (sparm == SPARM_ARCHMAGI) ? "the Archmagi" :
                       (sparm == SPARM_PRESERVATION) ? "preservation"
                                                       : "bugginess",
                       ITEMNAME_SIZE);
            }
            else
            {
                strncat(buff, (sparm == SPARM_RUNNING) ? " (run)" :
                       (sparm == SPARM_FIRE_RESISTANCE) ? " (R-fire)" :
                       (sparm == SPARM_COLD_RESISTANCE) ? " (R-cold)" :
                       (sparm == SPARM_POISON_RESISTANCE) ? " (R-poison)" :
                       (sparm == SPARM_SEE_INVISIBLE) ? " (see invis)" :
                       (sparm == SPARM_DARKNESS) ? " (darkness)" :
                       (sparm == SPARM_STRENGTH) ? " (str)" :
                       (sparm == SPARM_DEXTERITY) ? " (dex)" :
                       (sparm == SPARM_INTELLIGENCE) ? " (int)" :
                       (sparm == SPARM_PONDEROUSNESS) ? " (ponderous)" :
                       (sparm == SPARM_LEVITATION) ? " (levitate)" :
                       (sparm == SPARM_MAGIC_RESISTANCE) ? " (R-magic)" :
                       (sparm == SPARM_PROTECTION) ? " (protect)" :
                       (sparm == SPARM_STEALTH) ? " (stealth)" :
                       (sparm == SPARM_RESISTANCE) ? " (resist)" :
                       (sparm == SPARM_POSITIVE_ENERGY) ? " (R-neg)" : // ha ha
                       (sparm == SPARM_ARCHMAGI) ? " (Archmagi)" :
                       (sparm == SPARM_PRESERVATION) ? " (preserve)"
                                                       : " (buggy)",
                       ITEMNAME_SIZE);
            }
        }

        if (item_ident(item, ISFLAG_KNOW_CURSE) && item_cursed(item) && terse)
            strncat( buff, " (curse)", ITEMNAME_SIZE );
        break;

    // compacted 15 Apr 2000 {dlb}:
    case OBJ_WANDS:
        if (id[ IDTYPE_WANDS ][item_typ] == ID_KNOWN_TYPE
            || item_ident( item, ISFLAG_KNOW_TYPE ))
        {
            strncat(buff, "wand of ", ITEMNAME_SIZE );
            strncat(buff, (item_typ == WAND_FLAME) ? "flame" :
                   (item_typ == WAND_FROST) ? "frost" :
                   (item_typ == WAND_SLOWING) ? "slowing" :
                   (item_typ == WAND_HASTING) ? "hasting" :
                   (item_typ == WAND_MAGIC_DARTS) ? "magic darts" :
                   (item_typ == WAND_HEALING) ? "healing" :
                   (item_typ == WAND_PARALYSIS) ? "paralysis" :
                   (item_typ == WAND_FIRE) ? "fire" :
                   (item_typ == WAND_COLD) ? "cold" :
                   (item_typ == WAND_CONFUSION) ? "confusion" :
                   (item_typ == WAND_INVISIBILITY) ? "invisibility" :
                   (item_typ == WAND_DIGGING) ? "digging" :
                   (item_typ == WAND_FIREBALL) ? "fireball" :
                   (item_typ == WAND_TELEPORTATION) ? "teleportation" :
                   (item_typ == WAND_LIGHTNING) ? "lightning" :
                   (item_typ == WAND_POLYMORPH_OTHER) ? "polymorph other" :
                   (item_typ == WAND_ENSLAVEMENT) ? "enslavement" :
                   (item_typ == WAND_DRAINING) ? "draining" :
                   (item_typ == WAND_RANDOM_EFFECTS) ? "random effects" :
                   (item_typ == WAND_DISINTEGRATION) ? "disintegration"
                                                   : "bugginess",
                   ITEMNAME_SIZE );
        }
        else
        {
            char primary = (item.special % 12);
            char secondary = (item.special / 12);

            strncat(buff,(secondary == 0) ? "" :        // hope this works {dlb}
                   (secondary == 1) ? "jeweled" :
                   (secondary == 2) ? "curved" :
                   (secondary == 3) ? "long" :
                   (secondary == 4) ? "short" :
                   (secondary == 5) ? "twisted" :
                   (secondary == 6) ? "crooked" :
                   (secondary == 7) ? "forked" :
                   (secondary == 8) ? "shiny" :
                   (secondary == 9) ? "blackened" :
                   (secondary == 10) ? "tapered" :
                   (secondary == 11) ? "glowing" :
                   (secondary == 12) ? "worn" :
                   (secondary == 13) ? "encrusted" :
                   (secondary == 14) ? "runed" :
                   (secondary == 15) ? "sharpened" : "buggily", ITEMNAME_SIZE);

            if (secondary != 0)
                strncat(buff, " ", ITEMNAME_SIZE );

            strncat(buff, (primary == 0) ? "iron" :
                   (primary == 1) ? "brass" :
                   (primary == 2) ? "bone" :
                   (primary == 3) ? "wooden" :
                   (primary == 4) ? "copper" :
                   (primary == 5) ? "gold" :
                   (primary == 6) ? "silver" :
                   (primary == 7) ? "bronze" :
                   (primary == 8) ? "ivory" :
                   (primary == 9) ? "glass" :
                   (primary == 10) ? "lead" :
                   (primary == 11) ? "plastic" : "buggy", ITEMNAME_SIZE);

            strncat(buff, " wand", ITEMNAME_SIZE );
        }

        if (item_ident( item, ISFLAG_KNOW_PLUSES ))
        {
            strncat(buff, " (", ITEMNAME_SIZE );
            itoa( it_plus, tmp_quant, 10 );
            strncat(buff, tmp_quant, ITEMNAME_SIZE );
            strncat(buff, ")", ITEMNAME_SIZE);
        }
        break;

    // NB: potions, food, and scrolls stack on the basis of class and
    // type ONLY !!!

    // compacted 15 Apr 2000 {dlb}:
    case OBJ_POTIONS:
        if (id[ IDTYPE_POTIONS ][item_typ] == ID_KNOWN_TYPE
            || item_ident( item, ISFLAG_KNOW_TYPE ))
        {
            strncat(buff, "potion", ITEMNAME_SIZE );
            strncat(buff, (it_quant == 1) ? " " : "s ", ITEMNAME_SIZE);
            strncat(buff, "of ", ITEMNAME_SIZE );
            strncat(buff, (item_typ == POT_HEALING) ? "healing" :
                   (item_typ == POT_HEAL_WOUNDS) ? "heal wounds" :
                   (item_typ == POT_SPEED) ? "speed" :
                   (item_typ == POT_MIGHT) ? "might" :
                   (item_typ == POT_GAIN_STRENGTH) ? "gain strength" :
                   (item_typ == POT_GAIN_DEXTERITY) ? "gain dexterity" :
                   (item_typ == POT_GAIN_INTELLIGENCE) ? "gain intelligence" :
                   (item_typ == POT_LEVITATION) ? "levitation" :
                   (item_typ == POT_POISON) ? "poison" :
                   (item_typ == POT_SLOWING) ? "slowing" :
                   (item_typ == POT_PARALYSIS) ? "paralysis" :
                   (item_typ == POT_CONFUSION) ? "confusion" :
                   (item_typ == POT_INVISIBILITY) ? "invisibility" :
                   (item_typ == POT_PORRIDGE) ? "porridge" :
                   (item_typ == POT_DEGENERATION) ? "degeneration" :
                   (item_typ == POT_DECAY) ? "decay" :
                   (item_typ == POT_WATER) ? "water" :
                   (item_typ == POT_EXPERIENCE) ? "experience" :
                   (item_typ == POT_MAGIC) ? "magic" :
                   (item_typ == POT_RESTORE_ABILITIES) ? "restore abilities" :
                   (item_typ == POT_STRONG_POISON) ? "strong poison" :
                   (item_typ == POT_BERSERK_RAGE) ? "berserk rage" :
                   (item_typ == POT_CURE_MUTATION) ? "cure mutation" :
                   (item_typ == POT_MUTATION) ? "mutation" : "bugginess",
                   ITEMNAME_SIZE);
        }
        else
        {
            int pqual   = PQUAL(item.special);
            int pcolour = PCOLOUR(item.special);

            static const char *potion_qualifiers[] = {
                "",  "bubbling ", "fuming ", "fizzy ", "viscous ", "lumpy ",
                "smoky ", "glowing ", "sedimented ", "metallic ", "murky ",
                "gluggy ", "oily ", "slimy ", "emulsified "
            };
            ASSERT( sizeof(potion_qualifiers) / sizeof(*potion_qualifiers)
                        == PDQ_NQUALS );

            static const char *potion_colours[] = {
                "clear", "blue", "black", "silvery", "cyan", "purple",
                "orange", "inky", "red", "yellow", "green", "brown", "pink",
                "white"
            };
            ASSERT( sizeof(potion_colours) / sizeof(*potion_colours)
                        == PDC_NCOLOURS );

            const char *qualifier = 
                (pqual < 0 || pqual >= PDQ_NQUALS)? "bug-filled "
                                    : potion_qualifiers[pqual];

            const char *colour =
                (pcolour < 0 || pcolour >= PDC_NCOLOURS)? "bogus"
                                    : potion_colours[pcolour];

            strncat(buff, qualifier, ITEMNAME_SIZE - strlen(buff));
            strncat(buff, colour, ITEMNAME_SIZE - strlen(buff));
            strncat(buff, " potion", ITEMNAME_SIZE - strlen(buff) );

            if (it_quant > 1)
                strncat(buff, "s", ITEMNAME_SIZE );
        }
        break;

    // NB: adding another food type == must set for carnivorous chars
    // (Kobolds and mutants)
    case OBJ_FOOD:
        switch (item_typ)
        {
        case FOOD_MEAT_RATION:
            strncat(buff, "meat ration", ITEMNAME_SIZE );
            break;
        case FOOD_BREAD_RATION:
            strncat(buff, "bread ration", ITEMNAME_SIZE );
            break;
        case FOOD_PEAR:
            strncat(buff, "pear", ITEMNAME_SIZE );
            break;
        case FOOD_APPLE:        // make this less common
            strncat(buff, "apple", ITEMNAME_SIZE );
            break;
        case FOOD_CHOKO:
            strncat(buff, "choko", ITEMNAME_SIZE );
            break;
        case FOOD_HONEYCOMB:
            strncat(buff, "honeycomb", ITEMNAME_SIZE );
            break;
        case FOOD_ROYAL_JELLY:
            strncat(buff, "royal jell", ITEMNAME_SIZE );
            break;
        case FOOD_SNOZZCUMBER:
            strncat(buff, "snozzcumber", ITEMNAME_SIZE );
            break;
        case FOOD_PIZZA:
            strncat(buff, "slice of pizza", ITEMNAME_SIZE );
            break;
        case FOOD_APRICOT:
            strncat(buff, "apricot", ITEMNAME_SIZE );
            break;
        case FOOD_ORANGE:
            strncat(buff, "orange", ITEMNAME_SIZE );
            break;
        case FOOD_BANANA:
            strncat(buff, "banana", ITEMNAME_SIZE );
            break;
        case FOOD_STRAWBERRY:
            strncat(buff, "strawberr", ITEMNAME_SIZE );
            break;
        case FOOD_RAMBUTAN:
            strncat(buff, "rambutan", ITEMNAME_SIZE );
            break;
        case FOOD_LEMON:
            strncat(buff, "lemon", ITEMNAME_SIZE );
            break;
        case FOOD_GRAPE:
            strncat(buff, "grape", ITEMNAME_SIZE );
            break;
        case FOOD_SULTANA:
            strncat(buff, "sultana", ITEMNAME_SIZE );
            break;
        case FOOD_LYCHEE:
            strncat(buff, "lychee", ITEMNAME_SIZE );
            break;
        case FOOD_BEEF_JERKY:
            strncat(buff, "beef jerk", ITEMNAME_SIZE );
            break;
        case FOOD_CHEESE:
            strncat(buff, "cheese", ITEMNAME_SIZE );
            break;
        case FOOD_SAUSAGE:
            strncat(buff, "sausage", ITEMNAME_SIZE );
            break;
        case FOOD_CHUNK:
            moname( it_plus, true, DESC_PLAIN, tmp_buff );

            if (item.special < 100)
                strncat(buff, "rotting ", ITEMNAME_SIZE );

            strncat(buff, "chunk", ITEMNAME_SIZE );

            if (it_quant > 1)
                strncat(buff, "s", ITEMNAME_SIZE );

            strncat(buff, " of ", ITEMNAME_SIZE );
            strncat(buff, tmp_buff, ITEMNAME_SIZE );
            strncat(buff, " flesh", ITEMNAME_SIZE );
            break;

        }

        if (item_typ == FOOD_ROYAL_JELLY || item_typ == FOOD_STRAWBERRY
            || item_typ == FOOD_BEEF_JERKY)
            strncat(buff, (it_quant > 1) ? "ie" : "y", ITEMNAME_SIZE );
        break;

    // compacted 15 Apr 2000 {dlb}:
    case OBJ_SCROLLS:
        strncat(buff, "scroll", ITEMNAME_SIZE );
        strncat(buff, (it_quant == 1) ? " " : "s ", ITEMNAME_SIZE);

        if (id[ IDTYPE_SCROLLS ][item_typ] == ID_KNOWN_TYPE
            || item_ident( item, ISFLAG_KNOW_TYPE ))
        {
            strncat(buff, "of ", ITEMNAME_SIZE );
            strncat(buff, (item_typ == SCR_IDENTIFY) ? "identify" :
                   (item_typ == SCR_TELEPORTATION) ? "teleportation" :
                   (item_typ == SCR_FEAR) ? "fear" :
                   (item_typ == SCR_NOISE) ? "noise" :
                   (item_typ == SCR_REMOVE_CURSE) ? "remove curse" :
                   (item_typ == SCR_DETECT_CURSE) ? "detect curse" :
                   (item_typ == SCR_SUMMONING) ? "summoning" :
                   (item_typ == SCR_ENCHANT_WEAPON_I) ? "enchant weapon I" :
                   (item_typ == SCR_ENCHANT_ARMOUR) ? "enchant armour" :
                   (item_typ == SCR_TORMENT) ? "torment" :
                   (item_typ == SCR_RANDOM_USELESSNESS) ? "random uselessness" :
                   (item_typ == SCR_CURSE_WEAPON) ? "curse weapon" :
                   (item_typ == SCR_CURSE_ARMOUR) ? "curse armour" :
                   (item_typ == SCR_IMMOLATION) ? "immolation" :
                   (item_typ == SCR_BLINKING) ? "blinking" :
                   (item_typ == SCR_PAPER) ? "paper" :
                   (item_typ == SCR_MAGIC_MAPPING) ? "magic mapping" :
                   (item_typ == SCR_FORGETFULNESS) ? "forgetfulness" :
                   (item_typ == SCR_ACQUIREMENT) ? "acquirement" :
                   (item_typ == SCR_ENCHANT_WEAPON_II) ? "enchant weapon II" :
                   (item_typ == SCR_VORPALISE_WEAPON) ? "vorpalise weapon" :
                   (item_typ == SCR_RECHARGING) ? "recharging" :
                   //(item_typ == 23)                   ? "portal travel" :
                   (item_typ == SCR_ENCHANT_WEAPON_III) ? "enchant weapon III"
                                                       : "bugginess", 
                    ITEMNAME_SIZE);
        }
        else
        {
            strncat(buff, "labeled ", ITEMNAME_SIZE );
            char buff3[ ITEMNAME_SIZE ];

            const unsigned long sseed = 
                item.special 
                + static_cast<unsigned long>(it_plus)
                + (static_cast<unsigned long>(item_clas) << 16);
            make_name( sseed, true, buff3 );
            strncat( buff, buff3 , ITEMNAME_SIZE );
        }
        break;

    // compacted 15 Apr 2000 {dlb}: -- on hold ... what a mess!
    case OBJ_JEWELLERY:
        // not using {tried} here because there are some confusing 
        // issues to work out with how we want to handle jewellery 
        // artefacts and base type id. -- bwr
        if (item_ident( item, ISFLAG_KNOW_CURSE ))
        {
            if (item_cursed( item ))
                strncat(buff, "cursed ", ITEMNAME_SIZE );
            else if (Options.show_uncursed
                    && !terse
                    && (!ring_has_pluses(item)
                        || !item_ident(item, ISFLAG_KNOW_PLUSES))

                    // If the item is worn, its curse status is known,
                    // no need to belabour the obvious.
                    && get_equip_slot( &item ) == -1)
            {
                strncat(buff, "uncursed ", ITEMNAME_SIZE );
            }
        }

        if (is_random_artefact( item ))
        {
            strncat(buff, randart_ring_name(item), ITEMNAME_SIZE);
            break;
        }

        if (id[ IDTYPE_JEWELLERY ][item_typ] == ID_KNOWN_TYPE
            || item_ident( item, ISFLAG_KNOW_TYPE ))
        {

            if (item_ident( item, ISFLAG_KNOW_PLUSES )
                && (item_typ == RING_PROTECTION || item_typ == RING_STRENGTH
                    || item_typ == RING_SLAYING || item_typ == RING_EVASION
                    || item_typ == RING_DEXTERITY
                    || item_typ == RING_INTELLIGENCE))
            {
                char gokh = it_plus;

                if (gokh >= 0)
                    strncat( buff, "+" , ITEMNAME_SIZE );

                itoa( gokh, tmp_quant, 10 );
                strncat( buff, tmp_quant , ITEMNAME_SIZE );

                if (item_typ == RING_SLAYING)
                {
                    strncat( buff, "," , ITEMNAME_SIZE );

                    if (item_plus2 >= 0)
                        strncat(buff, "+", ITEMNAME_SIZE );

                    itoa( item_plus2, tmp_quant, 10 );
                    strncat( buff, tmp_quant , ITEMNAME_SIZE );
                }

                strncat(buff, " ", ITEMNAME_SIZE );
            }

            switch (item_typ)
            {
            case RING_REGENERATION:
                strncat(buff, "ring of regeneration", ITEMNAME_SIZE );
                break;
            case RING_PROTECTION:
                strncat(buff, "ring of protection", ITEMNAME_SIZE );
                break;
            case RING_PROTECTION_FROM_FIRE:
                strncat(buff, "ring of protection from fire", ITEMNAME_SIZE );
                break;
            case RING_POISON_RESISTANCE:
                strncat(buff, "ring of poison resistance", ITEMNAME_SIZE );
                break;
            case RING_PROTECTION_FROM_COLD:
                strncat(buff, "ring of protection from cold", ITEMNAME_SIZE );
                break;
            case RING_STRENGTH:
                strncat(buff, "ring of strength", ITEMNAME_SIZE );
                break;
            case RING_SLAYING:
                strncat(buff, "ring of slaying", ITEMNAME_SIZE );
                break;
            case RING_SEE_INVISIBLE:
                strncat(buff, "ring of see invisible", ITEMNAME_SIZE );
                break;
            case RING_INVISIBILITY:
                strncat(buff, "ring of invisibility", ITEMNAME_SIZE );
                break;
            case RING_HUNGER:
                strncat(buff, "ring of hunger", ITEMNAME_SIZE );
                break;
            case RING_TELEPORTATION:
                strncat(buff, "ring of teleportation", ITEMNAME_SIZE );
                break;
            case RING_EVASION:
                strncat(buff, "ring of evasion", ITEMNAME_SIZE );
                break;
            case RING_SUSTAIN_ABILITIES:
                strncat(buff, "ring of sustain abilities", ITEMNAME_SIZE );
                break;
            case RING_SUSTENANCE:
                strncat(buff, "ring of sustenance", ITEMNAME_SIZE );
                break;
            case RING_DEXTERITY:
                strncat(buff, "ring of dexterity", ITEMNAME_SIZE );
                break;
            case RING_INTELLIGENCE:
                strncat(buff, "ring of intelligence", ITEMNAME_SIZE );
                break;
            case RING_WIZARDRY:
                strncat(buff, "ring of wizardry", ITEMNAME_SIZE );
                break;
            case RING_MAGICAL_POWER:
                strncat(buff, "ring of magical power", ITEMNAME_SIZE );
                break;
            case RING_LEVITATION:
                strncat(buff, "ring of levitation", ITEMNAME_SIZE );
                break;
            case RING_LIFE_PROTECTION:
                strncat(buff, "ring of life protection", ITEMNAME_SIZE );
                break;
            case RING_PROTECTION_FROM_MAGIC:
                strncat(buff, "ring of protection from magic", ITEMNAME_SIZE );
                break;
            case RING_FIRE:
                strncat(buff, "ring of fire", ITEMNAME_SIZE );
                break;
            case RING_ICE:
                strncat(buff, "ring of ice", ITEMNAME_SIZE );
                break;
            case RING_TELEPORT_CONTROL:
                strncat(buff, "ring of teleport control", ITEMNAME_SIZE );
                break;
            case AMU_RAGE:
                strncat(buff, "amulet of rage", ITEMNAME_SIZE );
                break;
            case AMU_RESIST_SLOW:
                strncat(buff, "amulet of resist slowing", ITEMNAME_SIZE );
                break;
            case AMU_CLARITY:
                strncat(buff, "amulet of clarity", ITEMNAME_SIZE );
                break;
            case AMU_WARDING:
                strncat(buff, "amulet of warding", ITEMNAME_SIZE );
                break;
            case AMU_RESIST_CORROSION:
                strncat(buff, "amulet of resist corrosion", ITEMNAME_SIZE );
                break;
            case AMU_THE_GOURMAND:
                strncat(buff, "amulet of the gourmand", ITEMNAME_SIZE );
                break;
            case AMU_CONSERVATION:
                strncat(buff, "amulet of conservation", ITEMNAME_SIZE );
                break;
            case AMU_CONTROLLED_FLIGHT:
                strncat(buff, "amulet of controlled flight", ITEMNAME_SIZE );
                break;
            case AMU_INACCURACY:
                strncat(buff, "amulet of inaccuracy", ITEMNAME_SIZE );
                break;
            case AMU_RESIST_MUTATION:
                strncat(buff, "amulet of resist mutation", ITEMNAME_SIZE );
                break;
            }
            /* ? of imputed learning - 100% exp from tames/summoned kills */
            break;
        }

        if (item_typ < AMU_RAGE)        // rings
        {
            if (is_random_artefact( item ))
            {
                strncat(buff, randart_ring_name(item), ITEMNAME_SIZE);
                break;
            }

            switch (item.special / 13)       // secondary characteristic of ring
            {
            case 1:
                strncat(buff, "encrusted ", ITEMNAME_SIZE );
                break;
            case 2:
                strncat(buff, "glowing ", ITEMNAME_SIZE );
                break;
            case 3:
                strncat(buff, "tubular ", ITEMNAME_SIZE );
                break;
            case 4:
                strncat(buff, "runed ", ITEMNAME_SIZE );
                break;
            case 5:
                strncat(buff, "blackened ", ITEMNAME_SIZE );
                break;
            case 6:
                strncat(buff, "scratched ", ITEMNAME_SIZE );
                break;
            case 7:
                strncat(buff, "small ", ITEMNAME_SIZE );
                break;
            case 8:
                strncat(buff, "large ", ITEMNAME_SIZE );
                break;
            case 9:
                strncat(buff, "twisted ", ITEMNAME_SIZE );
                break;
            case 10:
                strncat(buff, "shiny ", ITEMNAME_SIZE );
                break;
            case 11:
                strncat(buff, "notched ", ITEMNAME_SIZE );
                break;
            case 12:
                strncat(buff, "knobbly ", ITEMNAME_SIZE );
                break;
            }

            switch (item.special % 13)
            {
            case 0:
                strncat(buff, "wooden ring", ITEMNAME_SIZE );
                break;
            case 1:
                strncat(buff, "silver ring", ITEMNAME_SIZE );
                break;
            case 2:
                strncat(buff, "golden ring", ITEMNAME_SIZE );
                break;
            case 3:
                strncat(buff, "iron ring", ITEMNAME_SIZE );
                break;
            case 4:
                strncat(buff, "steel ring", ITEMNAME_SIZE );
                break;
            case 5:
                strncat(buff, "bronze ring", ITEMNAME_SIZE );
                break;
            case 6:
                strncat(buff, "brass ring", ITEMNAME_SIZE );
                break;
            case 7:
                strncat(buff, "copper ring", ITEMNAME_SIZE );
                break;
            case 8:
                strncat(buff, "granite ring", ITEMNAME_SIZE );
                break;
            case 9:
                strncat(buff, "ivory ring", ITEMNAME_SIZE );
                break;
            case 10:
                strncat(buff, "bone ring", ITEMNAME_SIZE );
                break;
            case 11:
                strncat(buff, "marble ring", ITEMNAME_SIZE );
                break;
            case 12:
                strncat(buff, "jade ring", ITEMNAME_SIZE );
                break;
            case 13:
                strncat(buff, "glass ring", ITEMNAME_SIZE );
                break;
            }
        }                       // end of rings
        else                    // ie is an amulet
        {
            if (is_random_artefact( item ))
            {
                strncat(buff, randart_ring_name(item), ITEMNAME_SIZE);
                break;
            }

            if (item.special > 13)
            {
                switch (item.special / 13)   // secondary characteristic of amulet
                {
                case 0:
                    strncat(buff, "dented ", ITEMNAME_SIZE );
                    break;
                case 1:
                    strncat(buff, "square ", ITEMNAME_SIZE );
                    break;
                case 2:
                    strncat(buff, "thick ", ITEMNAME_SIZE );
                    break;
                case 3:
                    strncat(buff, "thin ", ITEMNAME_SIZE );
                    break;
                case 4:
                    strncat(buff, "runed ", ITEMNAME_SIZE );
                    break;
                case 5:
                    strncat(buff, "blackened ", ITEMNAME_SIZE );
                    break;
                case 6:
                    strncat(buff, "glowing ", ITEMNAME_SIZE );
                    break;
                case 7:
                    strncat(buff, "small ", ITEMNAME_SIZE );
                    break;
                case 8:
                    strncat(buff, "large ", ITEMNAME_SIZE );
                    break;
                case 9:
                    strncat(buff, "twisted ", ITEMNAME_SIZE );
                    break;
                case 10:
                    strncat(buff, "tiny ", ITEMNAME_SIZE );
                    break;
                case 11:
                    strncat(buff, "triangular ", ITEMNAME_SIZE );
                    break;
                case 12:
                    strncat(buff, "lumpy ", ITEMNAME_SIZE );
                    break;
                }
            }

            switch (item.special % 13)
            {
            case 0:
                strncat(buff, "zirconium amulet", ITEMNAME_SIZE );
                break;
            case 1:
                strncat(buff, "sapphire amulet", ITEMNAME_SIZE );
                break;
            case 2:
                strncat(buff, "golden amulet", ITEMNAME_SIZE );
                break;
            case 3:
                strncat(buff, "emerald amulet", ITEMNAME_SIZE );
                break;
            case 4:
                strncat(buff, "garnet amulet", ITEMNAME_SIZE );
                break;
            case 5:
                strncat(buff, "bronze amulet", ITEMNAME_SIZE );
                break;
            case 6:
                strncat(buff, "brass amulet", ITEMNAME_SIZE );
                break;
            case 7:
                strncat(buff, "copper amulet", ITEMNAME_SIZE );
                break;
            case 8:
                strncat(buff, "ruby amulet", ITEMNAME_SIZE );
                break;
            case 9:
                strncat(buff, "ivory amulet", ITEMNAME_SIZE );
                break;
            case 10:
                strncat(buff, "bone amulet", ITEMNAME_SIZE );
                break;
            case 11:
                strncat(buff, "platinum amulet", ITEMNAME_SIZE );
                break;
            case 12:
                strncat(buff, "jade amulet", ITEMNAME_SIZE );
                break;
            case 13:
                strncat(buff, "plastic amulet", ITEMNAME_SIZE );
                break;
            }
        }                       // end of amulets
        break;

    // compacted 15 Apr 2000 {dlb}:
    case OBJ_MISCELLANY:
        switch (item_typ)
        {
        case MISC_RUNE_OF_ZOT:
            strncat(buff, (it_plus == RUNE_DIS)          ? "iron" :
                          (it_plus == RUNE_GEHENNA)      ? "obsidian" :
                          (it_plus == RUNE_COCYTUS)      ? "icy" :
                          (it_plus == RUNE_TARTARUS)     ? "bone" :
                          (it_plus == RUNE_SLIME_PITS)   ? "slimy" :
                          (it_plus == RUNE_VAULTS)       ? "silver" :
                          (it_plus == RUNE_SNAKE_PIT)    ? "serpentine" :
                          (it_plus == RUNE_ELVEN_HALLS)  ? "elven" :
                          (it_plus == RUNE_TOMB)         ? "golden" :
                          (it_plus == RUNE_SWAMP)        ? "decaying" :
 
                          // pandemonium and abyss runes:
                          (it_plus == RUNE_DEMONIC)      ? "demonic" :
                          (it_plus == RUNE_ABYSSAL)      ? "abyssal" :

                          // special pandemonium runes:
                          (it_plus == RUNE_MNOLEG)       ? "glowing" :
                          (it_plus == RUNE_LOM_LOBON)    ? "magical" :
                          (it_plus == RUNE_CEREBOV)      ? "fiery" :
                          (it_plus == RUNE_GLOORX_VLOQ)  ? "dark"
                                                         : "buggy", 
                     ITEMNAME_SIZE);

            strncat(buff, " ", ITEMNAME_SIZE );
            strncat(buff, "rune", ITEMNAME_SIZE );

            if (it_quant > 1)
                strncat(buff, "s", ITEMNAME_SIZE );

            if (item_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, " of Zot", ITEMNAME_SIZE );
            break;

        case MISC_DECK_OF_POWER:
        case MISC_DECK_OF_SUMMONINGS:
        case MISC_DECK_OF_TRICKS:
        case MISC_DECK_OF_WONDERS:
            strncat(buff, "deck of ", ITEMNAME_SIZE );
            strncat(buff, !item_ident( item, ISFLAG_KNOW_TYPE )  ? "cards"  :
                   (item_typ == MISC_DECK_OF_WONDERS)      ? "wonders" :
                   (item_typ == MISC_DECK_OF_SUMMONINGS)   ? "summonings" :
                   (item_typ == MISC_DECK_OF_TRICKS)       ? "tricks" :
                   (item_typ == MISC_DECK_OF_POWER)        ? "power" 
                                                           : "bugginess",
                   ITEMNAME_SIZE);
            break;

        case MISC_CRYSTAL_BALL_OF_ENERGY:
        case MISC_CRYSTAL_BALL_OF_FIXATION:
        case MISC_CRYSTAL_BALL_OF_SEEING:
            strncat(buff, "crystal ball", ITEMNAME_SIZE );
            if (item_ident( item, ISFLAG_KNOW_TYPE ))
            {
                strncat(buff, " of ", ITEMNAME_SIZE );
                strncat(buff,
                       (item_typ == MISC_CRYSTAL_BALL_OF_SEEING) ? "seeing" :
                       (item_typ == MISC_CRYSTAL_BALL_OF_ENERGY) ? "energy" :
                       (item_typ == MISC_CRYSTAL_BALL_OF_FIXATION) ? "fixation"
                                                               : "bugginess",
                       ITEMNAME_SIZE);
            }
            break;

        case MISC_BOX_OF_BEASTS:
            if (item_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, "box of beasts", ITEMNAME_SIZE );
            else
                strncat(buff, "small ebony casket", ITEMNAME_SIZE );
            break;

        case MISC_EMPTY_EBONY_CASKET:
            if (item_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, "empty ebony casket", ITEMNAME_SIZE );
            else
                strncat(buff, "small ebony casket", ITEMNAME_SIZE );
            break;

        case MISC_AIR_ELEMENTAL_FAN:
            if (item_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, "air elemental ", ITEMNAME_SIZE );
            strncat(buff, "fan", ITEMNAME_SIZE );
            break;

        case MISC_LAMP_OF_FIRE:
            strncat(buff, "lamp", ITEMNAME_SIZE );
            if (item_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, " of fire", ITEMNAME_SIZE );
            break;

        case MISC_LANTERN_OF_SHADOWS:
            if (!item_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, "bone ", ITEMNAME_SIZE );
            strncat(buff, "lantern", ITEMNAME_SIZE );

            if (item_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, " of shadows", ITEMNAME_SIZE );
            break;

        case MISC_HORN_OF_GERYON:
            if (!item_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, "silver ", ITEMNAME_SIZE );
            strncat(buff, "horn", ITEMNAME_SIZE );

            if (item_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, " of Geryon", ITEMNAME_SIZE );
            break;

        case MISC_DISC_OF_STORMS:
            if (!item_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, "grey ", ITEMNAME_SIZE );
            strncat(buff, "disc", ITEMNAME_SIZE );

            if (item_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, " of storms", ITEMNAME_SIZE );
            break;

        case MISC_STONE_OF_EARTH_ELEMENTALS:
            if (!item_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, "nondescript ", ITEMNAME_SIZE );
            strncat(buff, "stone", ITEMNAME_SIZE );

            if (item_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, " of earth elementals", ITEMNAME_SIZE );
            break;

        case MISC_BOTTLED_EFREET:
            strncat(buff, (!item_ident( item, ISFLAG_KNOW_TYPE )) 
                                ? "sealed bronze flask" : "bottled efreet",
                                ITEMNAME_SIZE );
            break;

        case MISC_PORTABLE_ALTAR_OF_NEMELEX:
            strncat(buff, "portable altar of Nemelex", ITEMNAME_SIZE );
            break;

        default:
            strncat(buff, "buggy miscellaneous item", ITEMNAME_SIZE );
            break;
        }
        break;

    // compacted 15 Apr 2000 {dlb}:
    case OBJ_BOOKS:
        if (!item_ident( item, ISFLAG_KNOW_TYPE ))
        {
            char primary = (item.special / 10);
            char secondary = (item.special % 10);

            strncat(buff, (primary == 0) ? "" :
                   (primary == 1) ? "chunky " :
                   (primary == 2) ? "thick " :
                   (primary == 3) ? "thin " :
                   (primary == 4) ? "wide " :
                   (primary == 5) ? "glowing " :
                   (primary == 6) ? "dog-eared " :
                   (primary == 7) ? "oblong " :
                   (primary == 8) ? "runed " :

                   // these last three were single spaces {dlb}
                   (primary == 9) ? "" :
                   (primary == 10) ? "" : (primary == 11) ? "" : "buggily ",
                   ITEMNAME_SIZE);

            strncat(buff, (secondary == 0) ? "paperback " :
                   (secondary == 1) ? "hardcover " :
                   (secondary == 2) ? "leatherbound " :
                   (secondary == 3) ? "metal-bound " :
                   (secondary == 4) ? "papyrus " :
                   // these two were single spaces, too {dlb}
                   (secondary == 5) ? "" :
                   (secondary == 6) ? "" : "buggy ", ITEMNAME_SIZE);

            strncat(buff, "book", ITEMNAME_SIZE );
        }
        else if (item_typ == BOOK_MANUAL)
        {
            strncat(buff, "manual of ", ITEMNAME_SIZE );
            strncat(buff, skill_name(it_plus), ITEMNAME_SIZE );
        }
        else if (item_typ == BOOK_NECRONOMICON)
            strncat(buff, "Necronomicon", ITEMNAME_SIZE );
        else if (item_typ == BOOK_DESTRUCTION)
            strncat(buff, "tome of Destruction", ITEMNAME_SIZE );
        else if (item_typ == BOOK_YOUNG_POISONERS)
            strncat(buff, "Young Poisoner's Handbook", ITEMNAME_SIZE );
        else if (item_typ == BOOK_BEASTS)
            strncat(buff, "Monster Manual", ITEMNAME_SIZE );
        else
        {
            strncat(buff, "book of ", ITEMNAME_SIZE );
            strncat(buff, (item_typ == BOOK_MINOR_MAGIC_I
                          || item_typ == BOOK_MINOR_MAGIC_II
                          || item_typ == BOOK_MINOR_MAGIC_III) ? "Minor Magic" :
                          (item_typ == BOOK_CONJURATIONS_I
                          || item_typ == BOOK_CONJURATIONS_II) ? "Conjurations" :
                          (item_typ == BOOK_FLAMES) ? "Flames" :
                          (item_typ == BOOK_FROST) ? "Frost" :
                          (item_typ == BOOK_SUMMONINGS) ? "Summonings" :
                          (item_typ == BOOK_FIRE) ? "Fire" :
                          (item_typ == BOOK_ICE) ? "Ice" :
                          (item_typ == BOOK_SURVEYANCES) ? "Surveyances" :
                          (item_typ == BOOK_SPATIAL_TRANSLOCATIONS) ? "Spatial Translocations" :
                          (item_typ == BOOK_ENCHANTMENTS) ? "Enchantments" :
                          (item_typ == BOOK_TEMPESTS) ? "the Tempests" :
                          (item_typ == BOOK_DEATH) ? "Death" :
                          (item_typ == BOOK_HINDERANCE) ? "Hinderance" :
                          (item_typ == BOOK_CHANGES) ? "Changes" :
                          (item_typ == BOOK_TRANSFIGURATIONS) ? "Transfigurations" :
                          (item_typ == BOOK_PRACTICAL_MAGIC) ? "Practical Magic" :
                          (item_typ == BOOK_WAR_CHANTS) ? "War Chants" :
                          (item_typ == BOOK_CLOUDS) ? "Clouds" :
                          (item_typ == BOOK_HEALING) ? "Healing" :
                          (item_typ == BOOK_NECROMANCY) ? "Necromancy" :
                          (item_typ == BOOK_CALLINGS) ? "Callings" :
                          (item_typ == BOOK_CHARMS) ? "Charms" :
                          (item_typ == BOOK_DEMONOLOGY) ? "Demonology" :
                          (item_typ == BOOK_AIR) ? "Air" :
                          (item_typ == BOOK_SKY) ? "the Sky" :
                          (item_typ == BOOK_DIVINATIONS) ? "Divinations" :
                          (item_typ == BOOK_WARP) ? "the Warp" :
                          (item_typ == BOOK_ENVENOMATIONS) ? "Envenomations" :
                          (item_typ == BOOK_ANNIHILATIONS) ? "Annihilations" :
                          (item_typ == BOOK_UNLIFE) ? "Unlife" :
                          (item_typ == BOOK_CONTROL) ? "Control" :
                          (item_typ == BOOK_MUTATIONS) ? "Morphology" :
                          (item_typ == BOOK_TUKIMA) ? "Tukima" :
                          (item_typ == BOOK_GEOMANCY) ? "Geomancy" :
                          (item_typ == BOOK_EARTH) ? "the Earth" :
                          (item_typ == BOOK_WIZARDRY) ? "Wizardry" :
                          (item_typ == BOOK_POWER) ? "Power" :
                          (item_typ == BOOK_CANTRIPS) ? "Cantrips" :
                          (item_typ == BOOK_PARTY_TRICKS) ? "Party Tricks" :
                          (item_typ == BOOK_STALKING) ? "Stalking"
                                                           : "Bugginess",
                        ITEMNAME_SIZE );
        }
        break;

    // compacted 15 Apr 2000 {dlb}:
    case OBJ_STAVES:
        if (!item_ident( item, ISFLAG_KNOW_TYPE ))
        {
            strncat(buff, (item.special == 0) ? "curved" :
                   (item.special == 1) ? "glowing" :
                   (item.special == 2) ? "thick" :
                   (item.special == 3) ? "thin" :
                   (item.special == 4) ? "long" :
                   (item.special == 5) ? "twisted" :
                   (item.special == 6) ? "jeweled" :
                   (item.special == 7) ? "runed" :
                   (item.special == 8) ? "smoking" :
                   (item.special == 9) ? "gnarled" :    // was "" {dlb}
                   (item.special == 10) ? "" :
                   (item.special == 11) ? "" :
                   (item.special == 12) ? "" :
                   (item.special == 13) ? "" :
                   (item.special == 14) ? "" :
                   (item.special == 15) ? "" :
                   (item.special == 16) ? "" :
                   (item.special == 17) ? "" :
                   (item.special == 18) ? "" :
                   (item.special == 19) ? "" :
                   (item.special == 20) ? "" :
                   (item.special == 21) ? "" :
                   (item.special == 22) ? "" :
                   (item.special == 23) ? "" :
                   (item.special == 24) ? "" :
                   (item.special == 25) ? "" :
                   (item.special == 26) ? "" :
                   (item.special == 27) ? "" :
                   (item.special == 28) ? "" :
                   (item.special == 29) ? "" : "buggy", ITEMNAME_SIZE);
            strncat(buff, " ", ITEMNAME_SIZE );
        }

        strncat( buff, (item_is_rod( item ) ? "rod" : "staff"), ITEMNAME_SIZE );

        if (item_ident( item, ISFLAG_KNOW_TYPE ))
        {
            strncat(buff, " of ", ITEMNAME_SIZE );

            strncat(buff, (item_typ == STAFF_WIZARDRY) ? "wizardry" :
                   (item_typ == STAFF_POWER) ? "power" :
                   (item_typ == STAFF_FIRE) ? "fire" :
                   (item_typ == STAFF_COLD) ? "cold" :
                   (item_typ == STAFF_POISON) ? "poison" :
                   (item_typ == STAFF_ENERGY) ? "energy" :
                   (item_typ == STAFF_DEATH) ? "death" :
                   (item_typ == STAFF_CONJURATION) ? "conjuration" :
                   (item_typ == STAFF_ENCHANTMENT) ? "enchantment" :
                   (item_typ == STAFF_SMITING) ? "smiting" :
                   (item_typ == STAFF_STRIKING) ? "striking" :
                   (item_typ == STAFF_WARDING) ? "warding" :
                   (item_typ == STAFF_DISCOVERY) ? "discovery" :
                   (item_typ == STAFF_DEMONOLOGY) ? "demonology" :
                   (item_typ == STAFF_AIR) ? "air" :
                   (item_typ == STAFF_EARTH) ? "earth" :
                   (item_typ == STAFF_SUMMONING
                    || item_typ == STAFF_SPELL_SUMMONING) ? "summoning" :
                   (item_typ == STAFF_DESTRUCTION_I
                    || item_typ == STAFF_DESTRUCTION_II
                    || item_typ == STAFF_DESTRUCTION_III
                    || item_typ == STAFF_DESTRUCTION_IV) ? "destruction" :
                   (item_typ == STAFF_CHANNELING) ? "channeling"
                   : "bugginess", ITEMNAME_SIZE );
        }

        if (item_is_rod( item )
            && item_ident( item, ISFLAG_KNOW_TYPE ))
        {
            strncat( buff, " (", ITEMNAME_SIZE );
            itoa( item.plus / ROD_CHARGE_MULT, tmp_quant, 10 );
            strncat( buff, tmp_quant, ITEMNAME_SIZE );
            strncat( buff, "/", ITEMNAME_SIZE );
            itoa( item.plus2 / ROD_CHARGE_MULT, tmp_quant, 10 );
            strncat( buff, tmp_quant, ITEMNAME_SIZE );
            strncat( buff, ")", ITEMNAME_SIZE );
        }

        break;


    // rearranged 15 Apr 2000 {dlb}:
    case OBJ_ORBS:
        strncpy( buff, "Orb of ", ITEMNAME_SIZE );
        strncat( buff, (item_typ == ORB_ZOT) ? "Zot" :
/* ******************************************************************
                     (item_typ ==  1)      ? "Zug" :
                     (item_typ ==  2)      ? "Xob" :
                     (item_typ ==  3)      ? "Ix" :
                     (item_typ ==  4)      ? "Xug" :
                     (item_typ ==  5)      ? "Zob" :
                     (item_typ ==  6)      ? "Ik" :
                     (item_typ ==  7)      ? "Grolp" :
                     (item_typ ==  8)      ? "fo brO ehT" :
                     (item_typ ==  9)      ? "Plob" :
                     (item_typ == 10)      ? "Zuggle-Glob" :
                     (item_typ == 11)      ? "Zin" :
                     (item_typ == 12)      ? "Qexigok" :
                     (item_typ == 13)      ? "Bujuk" :
                     (item_typ == 14)      ? "Uhen Tiquritu" :
                     (item_typ == 15)      ? "Idohoxom Sovuf" :
                     (item_typ == 16)      ? "Voc Vocilicoso" :
                     (item_typ == 17)      ? "Chanuaxydiviha" :
                     (item_typ == 18)      ? "Ihexodox" :
                     (item_typ == 19)      ? "Rynok Pol" :
                     (item_typ == 20)      ? "Nemelex" :
                     (item_typ == 21)      ? "Sif Muna" :
                     (item_typ == 22)      ? "Okawaru" :
                     (item_typ == 23)      ? "Kikubaaqudgha" :
****************************************************************** */
               "Bugginess", ITEMNAME_SIZE );    // change back to "Zot" if source of problems cannot be found {dlb}
        break;

    case OBJ_GOLD:
        strncat(buff, "gold piece", ITEMNAME_SIZE );
        break;

    // still not implemented, yet:
    case OBJ_GEMSTONES:
        break;

    // rearranged 15 Apr 2000 {dlb}:
    case OBJ_CORPSES:
        if (item_typ == CORPSE_BODY && item.special < 100)
            strncat(buff, "rotting ", ITEMNAME_SIZE );

        moname( it_plus, true, DESC_PLAIN, tmp_buff );

        strncat(buff, tmp_buff, ITEMNAME_SIZE );
        strncat(buff, " ", ITEMNAME_SIZE );
        strncat(buff, (item_typ == CORPSE_BODY) ? "corpse" :
               (item_typ == CORPSE_SKELETON) ? "skeleton" : "corpse bug", 
               ITEMNAME_SIZE );
        break;

    default:
        strncat(buff, "!", ITEMNAME_SIZE );
    }                           // end of switch?

    // Disambiguation
    if (!terse && item_ident(item, ISFLAG_KNOW_TYPE))
    {
#define name_append(x) strncat(buff, x, ITEMNAME_SIZE)
        switch (item_clas)
        {
        case OBJ_STAVES:
            switch (item_typ)
            {
            case STAFF_DESTRUCTION_I:
                name_append(" [fire]");
                break;
            case STAFF_DESTRUCTION_II:
                name_append(" [ice]");
                break;
            case STAFF_DESTRUCTION_III:
                name_append(" [iron,fireball,lightning]");
                break;
            case STAFF_DESTRUCTION_IV:
                name_append(" [inacc,magma,cold]");
                break;
            }
            break;
        case OBJ_BOOKS:
            switch (item_typ)
            {
            case BOOK_MINOR_MAGIC_I:
                name_append(" [flame]");
                break;
            case BOOK_MINOR_MAGIC_II:
                name_append(" [frost]");
                break;
            case BOOK_MINOR_MAGIC_III:
                name_append(" [summ]");
                break;
            case BOOK_CONJURATIONS_I:
                name_append(" [fire]");
                break;
            case BOOK_CONJURATIONS_II:
                name_append(" [ice]");
                break;
            }
        }
#undef name_append
    }

    // debugging output -- oops, I probably block it above ... dang! {dlb}
    if (strlen(buff) < 3)
    {
        char ugug[20];

        strncat(buff, "bad item (cl:", ITEMNAME_SIZE );
        itoa(item_clas, ugug, 10);
        strncat(buff, ugug, ITEMNAME_SIZE );
        strncat(buff, ",ty:", ITEMNAME_SIZE );
        itoa(item_typ, ugug, 10);
        strncat(buff, ugug, ITEMNAME_SIZE );
        strncat(buff, ",pl:", ITEMNAME_SIZE );
        itoa(it_plus, ugug, 10);
        strncat(buff, ugug, ITEMNAME_SIZE );
        strncat(buff, ",pl2:", ITEMNAME_SIZE );
        itoa(item_plus2, ugug, 10);
        strncat(buff, ugug, ITEMNAME_SIZE );
        strncat(buff, ",sp:", ITEMNAME_SIZE );
        itoa(item.special, ugug, 10);
        strncat(buff, ugug, ITEMNAME_SIZE );
        strncat(buff, ",qu:", ITEMNAME_SIZE );
        itoa(it_quant, ugug, 10);
        strncat(buff, ugug, ITEMNAME_SIZE );
        strncat(buff, ")", ITEMNAME_SIZE );
    }

    // hackish {dlb}
    if (it_quant > 1
        && item_clas != OBJ_MISSILES
        && item_clas != OBJ_SCROLLS
        && item_clas != OBJ_POTIONS
        && item_clas != OBJ_MISCELLANY
        && (item_clas != OBJ_FOOD || item_typ != FOOD_CHUNK))
    {
        strncat(buff, "s", ITEMNAME_SIZE );
    }

    return (buff);
}                               // end item_name_2()

void save_id(char identy[4][50])
{
    int x = 0, jx = 0;

    for (x = 0; x < 4; x++)
    {
        for (jx = 0; jx < 50; jx++)
        {
            identy[x][jx] = id[x][jx];
        }
    }
}                               // end save_id()

void clear_ids(void)
{

    int i = 0, j = 0;

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 50; j++)
        {
            id[i][j] = ID_UNKNOWN_TYPE;
        }
    }

}                               // end clear_ids()


void set_ident_type( char cla, int ty, char setting, bool force )
{
    // Don't allow overwriting of known type with tried unless forced.
    if (!force 
        && setting == ID_TRIED_TYPE
        && get_ident_type( cla, ty ) == ID_KNOWN_TYPE)
    {
        return;
    }

    switch (cla)
    {
    case OBJ_WANDS:
        id[ IDTYPE_WANDS ][ty] = setting;
        break;

    case OBJ_SCROLLS:
        id[ IDTYPE_SCROLLS ][ty] = setting;
        break;

    case OBJ_JEWELLERY:
        id[ IDTYPE_JEWELLERY ][ty] = setting;
        break;

    case OBJ_POTIONS:
        id[ IDTYPE_POTIONS ][ty] = setting;
        break;

    default:
        break;
    }
}                               // end set_ident_type()

char get_ident_type(char cla, int ty)
{
    switch (cla)
    {
    case OBJ_WANDS:
        return id[ IDTYPE_WANDS ][ty];

    case OBJ_SCROLLS:
        return id[ IDTYPE_SCROLLS ][ty];

    case OBJ_JEWELLERY:
        return id[ IDTYPE_JEWELLERY ][ty];

    case OBJ_POTIONS:
        return id[ IDTYPE_POTIONS ][ty];

    default:
        return (ID_UNKNOWN_TYPE);
    }
}                               // end get_ident_type()

static MenuEntry *discoveries_item_mangle(MenuEntry *me)
{
    InvEntry *ie = dynamic_cast<InvEntry*>(me);
    MenuEntry *newme = new MenuEntry;
    std::string txt = item_name(*ie->item, DESC_PLAIN);
    newme->text = " " + txt;
    newme->quantity = 0;
    delete me;

    return (newme);
}

void check_item_knowledge()
{
    int i,j;

    std::vector<const item_def*> items;

    int idx_to_objtype[4] = { OBJ_WANDS, OBJ_SCROLLS,
                              OBJ_JEWELLERY, OBJ_POTIONS };
    int idx_to_maxtype[4] = { NUM_WANDS, NUM_SCROLLS,
                              NUM_JEWELLERY, NUM_POTIONS };

    for (i = 0; i < 4; i++) {
        for (j = 0; j < idx_to_maxtype[i]; j++) {
            if (id[i][j] == ID_KNOWN_TYPE) {
                item_def* ptmp = new item_def;
                if ( ptmp != 0 ) {
                    ptmp->base_type = idx_to_objtype[i];
                    ptmp->sub_type  = j;
                    ptmp->colour    = 1;
                    ptmp->quantity  = 1;
                    items.push_back(ptmp);
                }
            }
        }
    }

    if (items.empty())
        mpr("You don't recognise anything yet!");
    else {    
        InvMenu menu;
        menu.set_title("You recognise:");
        menu.load_items(items, discoveries_item_mangle);
        menu.set_flags(MF_NOSELECT);
        menu.show();
        redraw_screen();

        for ( std::vector<const item_def*>::iterator iter = items.begin();
              iter != items.end(); ++iter )
            delete *iter;
    }
}                               // end check_item_knowledge()


// Used for: Pandemonium demonlords, shopkeepers, scrolls, random artefacts
int make_name( unsigned long seed, bool all_cap, char buff[ ITEMNAME_SIZE ] )
{
    char name[ITEMNAME_SIZE];
    int  numb[17];

    int i = 0;
    bool want_vowel = false;
    bool has_space = false;

    for (i = 0; i < ITEMNAME_SIZE; i++)
        name[i] = '\0';

    const int var1 = (seed & 0xFF);
    const int var2 = ((seed >>  8) & 0xFF);
    const int var3 = ((seed >> 16) & 0xFF);
    const int var4 = ((seed >> 24) & 0xFF);

    numb[0]  = 373 * var1 + 409 * var2 + 281 * var3;
    numb[1]  = 163 * var4 + 277 * var2 + 317 * var3;
    numb[2]  = 257 * var1 + 179 * var4 +  83 * var3;
    numb[3]  =  61 * var1 + 229 * var2 + 241 * var4;
    numb[4]  =  79 * var1 + 263 * var2 + 149 * var3;
    numb[5]  = 233 * var4 + 383 * var2 + 311 * var3;
    numb[6]  = 199 * var1 + 211 * var4 + 103 * var3;
    numb[7]  = 139 * var1 + 109 * var2 + 349 * var4;
    numb[8]  =  43 * var1 + 389 * var2 + 359 * var3;
    numb[9]  = 367 * var4 + 101 * var2 + 251 * var3;
    numb[10] = 293 * var1 +  59 * var4 + 151 * var3;
    numb[11] = 331 * var1 + 107 * var2 + 307 * var4;
    numb[12] =  73 * var1 + 157 * var2 + 347 * var3;
    numb[13] = 379 * var4 + 353 * var2 + 227 * var3;
    numb[14] = 181 * var1 + 173 * var4 + 193 * var3;
    numb[15] = 131 * var1 + 167 * var2 +  53 * var4;
    numb[16] = 313 * var1 + 127 * var2 + 401 * var3 + 337 * var4;

    int len = 3 + numb[0] % 5 + ((numb[1] % 5 == 0) ? numb[2] % 6 : 1);

    if (all_cap)
        len += 6;

    int j = numb[3] % 17;
    const int k = numb[4] % 17;
    int count = 0;

    for (i = 0; i < len; i++)
    {
        j = (j + 1) % 17;
        if (j == 0)
        {
            count++;
            if (count > 9)
                break;
        }

        if (!has_space && i > 5 && i < len - 4
            && (numb[(k + 10 * j) % 17] % 5) != 3)
        {
            want_vowel = true;
            name[i] = ' ';
        }
        else if (i > 0
            && (want_vowel 
                || (i > 1 
                    && is_random_name_vowel( name[i - 1] )
                    && !is_random_name_vowel( name[i - 2] )
                    && (numb[(k + 4 * j) % 17] % 5) <= 1 )))
        {
            want_vowel = true;
            name[i] = retvow( numb[(k + 7 * j) % 17] );

            if (is_random_name_space( name[i] ))
            {
                if (i == 0)
                {
                    want_vowel = false;
                    name[i] = retlet( numb[(k + 14 * j) % 17] );
                }
                else if (len < 7
                        || i <= 2 || i >= len - 3 
                        || is_random_name_space( name[i - 1] )
                        || (i > 1 && is_random_name_space( name[i - 2] ))
                        || (i > 2 
                            && !is_random_name_vowel( name[i - 1] )
                            && !is_random_name_vowel( name[i - 2] )))
                {
                    i--;
                    continue;
                }
            }
            else if (i > 1 
                    && name[i] == name[i - 1] 
                    && (name[i] == 'y' || name[i] == 'i'
                        || (numb[(k + 12 * j) % 17] % 5) <= 1))
            {
                i--;
                continue;
            }
        }
        else
        {
            if ((len > 3 || i != 0)
                && (numb[(k + 13 * j) % 17] % 7) <= 1
                && (i < len - 2 || (i > 0 && !is_random_name_space(name[i - 1]))))
            {
                const bool beg = ((i < 1) || is_random_name_space(name[i - 1]));
                const bool end = (i >= len - 2);

                const int first = (beg ?  0 : (end ? 14 :  0));
                const int last  = (beg ? 27 : (end ? 56 : 67));

                const int num = last - first;

                i++;

                switch (numb[(k + 11 * j) % 17] % num + first)
                {
                // start, middle
                case  0: strcat(name, "kl"); break;
                case  1: strcat(name, "gr"); break;
                case  2: strcat(name, "cl"); break;
                case  3: strcat(name, "cr"); break;
                case  4: strcat(name, "fr"); break;
                case  5: strcat(name, "pr"); break;
                case  6: strcat(name, "tr"); break;
                case  7: strcat(name, "tw"); break;
                case  8: strcat(name, "br"); break;
                case  9: strcat(name, "pl"); break;
                case 10: strcat(name, "bl"); break;
                case 11: strcat(name, "str"); i++; len++; break;
                case 12: strcat(name, "shr"); i++; len++; break;
                case 13: strcat(name, "thr"); i++; len++; break;
                // start, middle, end 
                case 14: strcat(name, "sm"); break;
                case 15: strcat(name, "sh"); break;
                case 16: strcat(name, "ch"); break;
                case 17: strcat(name, "th"); break;
                case 18: strcat(name, "ph"); break;
                case 19: strcat(name, "pn"); break;
                case 20: strcat(name, "kh"); break;
                case 21: strcat(name, "gh"); break;
                case 22: strcat(name, "mn"); break;
                case 23: strcat(name, "ps"); break;
                case 24: strcat(name, "st"); break;
                case 25: strcat(name, "sk"); break;
                case 26: strcat(name, "sch"); i++; len++; break;
                // middle, end
                case 27: strcat(name, "ts"); break;
                case 28: strcat(name, "cs"); break;
                case 29: strcat(name, "xt"); break;
                case 30: strcat(name, "nt"); break;
                case 31: strcat(name, "ll"); break;
                case 32: strcat(name, "rr"); break;
                case 33: strcat(name, "ss"); break;
                case 34: strcat(name, "wk"); break;
                case 35: strcat(name, "wn"); break;
                case 36: strcat(name, "ng"); break;
                case 37: strcat(name, "cw"); break;
                case 38: strcat(name, "mp"); break;
                case 39: strcat(name, "ck"); break;
                case 40: strcat(name, "nk"); break;
                case 41: strcat(name, "dd"); break;
                case 42: strcat(name, "tt"); break;
                case 43: strcat(name, "bb"); break;
                case 44: strcat(name, "pp"); break;
                case 45: strcat(name, "nn"); break;
                case 46: strcat(name, "mm"); break;
                case 47: strcat(name, "kk"); break;
                case 48: strcat(name, "gg"); break;
                case 49: strcat(name, "ff"); break;
                case 50: strcat(name, "pt"); break;
                case 51: strcat(name, "tz"); break;
                case 52: strcat(name, "dgh"); i++; len++; break;
                case 53: strcat(name, "rgh"); i++; len++; break;
                case 54: strcat(name, "rph"); i++; len++; break;
                case 55: strcat(name, "rch"); i++; len++; break;
                // middle only
                case 56: strcat(name, "cz"); break;
                case 57: strcat(name, "xk"); break;
                case 58: strcat(name, "zx"); break;
                case 59: strcat(name, "xz"); break;
                case 60: strcat(name, "cv"); break;
                case 61: strcat(name, "vv"); break;
                case 62: strcat(name, "nl"); break;
                case 63: strcat(name, "rh"); break;
                case 64: strcat(name, "dw"); break;
                case 65: strcat(name, "nw"); break;
                case 66: strcat(name, "khl"); i++; len++; break;
                default:
                    i--;
                    break;
                }
            }
            else
            {
                if (i == 0)
                {
                    name[i] = 'a' + (numb[(k + 8 * j) % 17] % 26);
                    want_vowel = is_random_name_vowel( name[i] );
                }
                else
                {
                    name[i] = retlet( numb[(k + 3 * j) % 17] );
                }
            }
        }

        if (name[i] == '\0')
        {
            i--;
            continue;
        }

        if (want_vowel && !is_random_name_vowel( name[i] )
            || (!want_vowel && is_random_name_vowel( name[i] )))
        {
            i--;
            continue;
        }

        if (is_random_name_space( name[i] ))
            has_space = true;

        if (!is_random_name_vowel( name[i] ))
            want_vowel = true;
        else
            want_vowel = false;
    }

    // catch break and try to give a final letter
    if (i > 0 
        && !is_random_name_space( name[i - 1] )
        && name[i - 1] != 'y'  
        && is_random_name_vowel( name[i - 1] )
        && (count > 9 || (i < 8 && numb[16] % 3)))
    {
        name[i] = retlet( numb[j] );
    }
    
    len = strlen( name );

    if (len)
    {
        for (i = len - 1; i > 0; i--)
        {
            if (!isspace( name[i] ))
                break;
            else 
            {
                name[i] = '\0';
                len--;
            }
        }
    }

    if (len >= 3)
        strncpy( buff, name, ITEMNAME_SIZE );
    else
    {
        strncpy( buff, "plog", ITEMNAME_SIZE );
        len = 4;
    }

    buff[ ITEMNAME_SIZE - 1 ] = '\0';

    for (i = 0; i < len; i++)
    {
        if (all_cap || i == 0 || buff[i - 1] == ' ')
            buff[i] = toupper( buff[i] );
    }

    return (len); 
}                               // end make_name()

bool is_random_name_space(char let)
{
    return (let == ' ');
}

static bool is_random_name_vowel( char let )
{
    return (let == 'a' || let == 'e' || let == 'i' || let == 'o' || let == 'u'
            || let == 'y' || let == ' ');
}                               // end is_random_name_vowel()

static char retvow( int sed )
{
    static const char vowels[] = "aeiouaeiouaeiouy  ";
    return (vowels[ sed % (sizeof(vowels) - 1) ]);
}                               // end retvow()

static char retlet( int sed )
{
    static const char consonants[] = "bcdfghjklmnpqrstvwxzcdfghlmnrstlmnrst";
    return (consonants[ sed % (sizeof(consonants) - 1) ]);
}

bool is_interesting_item( const item_def& item ) {
    if ( is_random_artefact(item) ||
         is_unrandom_artefact(item) ||
         is_fixed_artefact(item) )
        return true;

    char name[ITEMNAME_SIZE];
    item_name(item, DESC_PLAIN, name, false);
    std::string iname(name);
    for (unsigned i = 0; i < Options.note_items.size(); ++i)
        if (Options.note_items[i].matches(iname))
            return true;
    return false;
}

bool fully_identified( const item_def& item ) {
    long flagset = ISFLAG_IDENT_MASK;
    switch ( item.base_type ) {
    case OBJ_BOOKS:
    case OBJ_MISCELLANY:
    case OBJ_ORBS:
    case OBJ_SCROLLS:
    case OBJ_POTIONS:
        flagset = ISFLAG_KNOW_TYPE;
        break;
    case OBJ_FOOD:
        flagset = 0;
        break;
    case OBJ_WANDS:
        flagset = (ISFLAG_KNOW_TYPE | ISFLAG_KNOW_PLUSES);
        break;
    case OBJ_JEWELLERY:
        flagset = (ISFLAG_KNOW_CURSE | ISFLAG_KNOW_TYPE);
        if ( ring_has_pluses(item) )
            flagset |= ISFLAG_KNOW_PLUSES;
        break;
    default:
        break;
    }
    if ( is_random_artefact(item) ||
         is_fixed_artefact(item) ||
         is_unrandom_artefact(item) )
        flagset |= ISFLAG_KNOW_PROPERTIES;
    
    return item_ident( item, flagset );
}
