/*
 *  File:       itemname.cc
 *  Summary:    Misc functions.
 *  Written by: Linley Henzell
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

#include "macro.h"
#include "mon-util.h"
#include "randart.h"
#include "skills2.h"
#include "stuff.h"
#include "wpn-misc.h"
#include "view.h"


char id[4][50];
int  prop[4][50][3];
FixedArray < int, 20, 50 > mss;

static bool is_random_name_vowel(unsigned char let);
static char item_name_2( const item_def &item, char buff[ ITEMNAME_SIZE ], bool terse );

char reduce(unsigned char reducee);
char retbit(char sed);
char retvow(char sed);

bool is_vowel( const char chr )
{
    const char low = tolower( chr );

    return (low == 'a' || low == 'e' || low == 'i' || low == 'o' || low == 'u');
}

// Some convenient functions to hide the bit operations and create 
// an interface layer between the code and the data in case this 
// gets changed again. -- bwr
bool item_cursed( const item_def &item )
{
    return (item.flags & ISFLAG_CURSED);
}

bool item_uncursed( const item_def &item )
{
    return !(item.flags & ISFLAG_CURSED);
}

bool item_known_cursed( const item_def &item )
{
    return ((item.flags & ISFLAG_KNOW_CURSE) && (item.flags & ISFLAG_CURSED));
}

bool item_known_uncursed( const item_def &item )
{
    return ((item.flags & ISFLAG_KNOW_CURSE) && !(item.flags & ISFLAG_CURSED));
}

#if 0
// currently unused
bool fully_identified( const item_def &item )
{
    return ((item.flags & ISFLAG_IDENT_MASK) == ISFLAG_IDENT_MASK);
}
#endif

bool item_ident( const item_def &item, unsigned long flags )
{
    return (item.flags & flags);
}

bool item_not_ident( const item_def &item, unsigned long flags )
{
    return ( !(item.flags & flags) );
}

void do_curse_item( item_def &item )
{
    item.flags |= ISFLAG_CURSED;
}

void do_uncurse_item( item_def &item )
{
    item.flags &= (~ISFLAG_CURSED);
}

void set_ident_flags( item_def &item, unsigned long flags )
{
    item.flags |= flags;
}

void unset_ident_flags( item_def &item, unsigned long flags )
{
    item.flags &= (~flags);
}

// These six functions might seem silly, but they provide a nice layer
// for later changes to these systems. -- bwr
unsigned long get_equip_race( const item_def &item )
{
    return (item.flags & ISFLAG_RACIAL_MASK);
}

unsigned long get_equip_desc( const item_def &item )
{
    return (item.flags & ISFLAG_COSMETIC_MASK);
}

bool cmp_equip_race( const item_def &item, unsigned long val )
{
    return (get_equip_race( item ) == val);
}

bool cmp_equip_desc( const item_def &item, unsigned long val )
{
    return (get_equip_desc( item ) == val);
}

void set_equip_race( item_def &item, unsigned long flags )
{
    ASSERT( (flags & ~ISFLAG_RACIAL_MASK) == 0 );

    // first check for base-sub pairs that can't ever have racial types
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        if (item.sub_type == WPN_GIANT_CLUB
            || item.sub_type == WPN_GIANT_SPIKED_CLUB
            || item.sub_type == WPN_KATANA
            || item.sub_type == WPN_SLING
            || item.sub_type == WPN_KNIFE
            || item.sub_type == WPN_QUARTERSTAFF
            || item.sub_type == WPN_DEMON_BLADE
            || item.sub_type == WPN_DEMON_WHIP
            || item.sub_type == WPN_DEMON_TRIDENT)
        {
            return;
        }
        break;

    case OBJ_ARMOUR:
        if (item.sub_type >= ARM_DRAGON_HIDE)
        {
            return;
        }
        break;

    case OBJ_MISSILES:
        if (item.sub_type == MI_STONE || item.sub_type == MI_LARGE_ROCK)
        {
            return;
        }
        break;

    default:
        return;
    }

    // check that item is appropriate for racial type
    switch (flags)
    {
    case ISFLAG_ELVEN: 
        if (item.base_type == OBJ_ARMOUR 
            && (item.sub_type == ARM_SPLINT_MAIL
                || item.sub_type == ARM_BANDED_MAIL
                || item.sub_type == ARM_PLATE_MAIL))
        {
            return;
        }
        break;
            
    case ISFLAG_DWARVEN: 
        if (item.base_type == OBJ_ARMOUR 
            && (item.sub_type == ARM_ROBE
                || item.sub_type == ARM_LEATHER_ARMOUR))
        {
            return;
        }
        break;

    case ISFLAG_ORCISH: 
    default:
        break;
    }

    item.flags &= ~ISFLAG_RACIAL_MASK; // delete previous
    item.flags |= flags;
}

void set_equip_desc( item_def &item, unsigned long flags )
{
    ASSERT( (flags & ~ISFLAG_COSMETIC_MASK) == 0 );

    item.flags &= ~ISFLAG_COSMETIC_MASK; // delete previous
    item.flags |= flags;
}

short get_helmet_type( const item_def &item )
{
    ASSERT( item.base_type == OBJ_ARMOUR && item.sub_type == ARM_HELMET );

    return (item.plus2 & THELM_TYPE_MASK);
}

short get_helmet_desc( const item_def &item )
{
    ASSERT( item.base_type == OBJ_ARMOUR && item.sub_type == ARM_HELMET );

    return (item.plus2 & THELM_DESC_MASK);
}

void set_helmet_type( item_def &item, short type )
{
    ASSERT( (type & ~THELM_TYPE_MASK) == 0 );
    ASSERT( item.base_type == OBJ_ARMOUR && item.sub_type == ARM_HELMET );

    item.plus2 &= ~THELM_TYPE_MASK; 
    item.plus2 |= type;
}

void set_helmet_desc( item_def &item, short type )
{
    ASSERT( (type & ~THELM_DESC_MASK) == 0 );
    ASSERT( item.base_type == OBJ_ARMOUR && item.sub_type == ARM_HELMET );

    item.plus2 &= ~THELM_DESC_MASK; 
    item.plus2 |= type;
}

void set_helmet_random_desc( item_def &item )
{
    ASSERT( item.base_type == OBJ_ARMOUR && item.sub_type == ARM_HELMET );

    item.plus2 &= ~THELM_DESC_MASK; 
    item.plus2 |= (random2(8) << 8);
}

bool cmp_helmet_type( const item_def &item, short val )
{
    ASSERT( item.base_type == OBJ_ARMOUR && item.sub_type == ARM_HELMET );

    return (get_helmet_type( item ) == val);
}

bool cmp_helmet_desc( const item_def &item, short val )
{
    ASSERT( item.base_type == OBJ_ARMOUR && item.sub_type == ARM_HELMET );

    return (get_helmet_desc( item ) == val);
}

bool set_item_ego_type( item_def &item, int item_type, int ego_type )
{
    if (item.base_type == item_type 
        && !is_random_artefact( item ) 
        && !is_fixed_artefact( item ))
    {
        item.special = ego_type;
        return (true);
    }

    return (false);
}

int get_weapon_brand( const item_def &item )
{
    // Weapon ego types are "brands", so we do the randart lookup here.
    
    // Staves "brands" handled specially
    if (item.base_type != OBJ_WEAPONS)
        return (SPWPN_NORMAL);

    if (is_fixed_artefact( item ))
    {
        switch (item.special)
        {
        case SPWPN_SWORD_OF_CEREBOV:
            return (SPWPN_FLAMING);

        case SPWPN_STAFF_OF_OLGREB:
            return (SPWPN_VENOM);

        case SPWPN_VAMPIRES_TOOTH:
            return (SPWPN_VAMPIRICISM);

        default:
            return (SPWPN_NORMAL);
        }
    }
    else if (is_random_artefact( item ))
    {
        return (randart_wpn_property( item, RAP_BRAND ));
    }

    return (item.special);
}

int get_ammo_brand( const item_def &item )
{
    // no artefact arrows yet -- bwr
    if (item.base_type != OBJ_MISSILES || is_random_artefact( item ))
        return (SPMSL_NORMAL);

    return (item.special);
}

int get_armour_ego_type( const item_def &item )
{
    // artefact armours have no ego type, must look up powers separately
    if (item.base_type != OBJ_ARMOUR 
        || (is_random_artefact( item ) && !is_unrandom_artefact( item )))
    {
        return (SPARM_NORMAL);
    }

    return (item.special);
}

bool item_is_rod( const item_def &item )
{
    return (item.base_type == OBJ_STAVES 
            && item.sub_type >= STAFF_SMITING && item.sub_type < STAFF_AIR);
}

bool item_is_staff( const item_def &item )
{
    // Isn't De Morgan's law wonderful. -- bwr
    return (item.base_type == OBJ_STAVES 
            && (item.sub_type < STAFF_SMITING || item.sub_type >= STAFF_AIR));
}

// it_name() and in_name() are now somewhat obsolete now that itemname
// takes item_def, so consider them depricated.
void it_name( int itn, char des, char buff[ ITEMNAME_SIZE ], bool terse )
{
    item_name( mitm[itn], des, buff, terse );
}                               // end it_name()


void in_name( int inn, char des, char buff[ ITEMNAME_SIZE ], bool terse )
{
    item_name( you.inv[inn], des, buff, terse );
}                               // end in_name()

// quant_name is usful since it prints out a different number of items
// than the item actually contains.
void quant_name( const item_def &item, int quant, char des,
                 char buff[ ITEMNAME_SIZE ], bool terse )
{
    // item_name now requires a "real" item, so we'll mangle a tmp
    item_def tmp = item;
    tmp.quantity = quant;

    item_name( tmp, des, buff, terse );
}                               // end quant_name()

char item_name( const item_def &item, char descrip, char buff[ ITEMNAME_SIZE ],
                bool terse )
{
    const int item_clas = item.base_type;
    const int item_typ = item.sub_type;
    const int it_quant = item.quantity;
    
    char tmp_quant[20];
    char itm_name[  ITEMNAME_SIZE  ] = "";

    item_name_2( item, itm_name, terse );

    buff[0] = '\0';

    if (descrip == DESC_INVENTORY_EQUIP || descrip == DESC_INVENTORY) 
    {
        if (item.x == -1 && item.y == -1) // actually in inventory
            snprintf( buff,  ITEMNAME_SIZE, (terse) ? "%c) " : "%c - ",
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

    return (1);
}                               // end item_name()


// Note that "terse" is only currently used for the "in hand" listing on
// the game screen.
static char item_name_2( const item_def &item, char buff[ITEMNAME_SIZE],
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

    buff[0] = '\0';

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
                    && item_not_ident( item, ISFLAG_KNOW_PLUSES ))
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
                       (item.special == SPWPN_STAFF_OF_WUCAD_MU) ? "quarterstaff"
                                                           : "buggy bola", 
                                                           ITEMNAME_SIZE );
            }
            break;
        }

        // Now that we can have "glowing elven" weapons, it's 
        // probably a good idea to cut out the descriptive
        // term once it's become obsolete. -- bwr
        if (item_not_ident( item, ISFLAG_KNOW_PLUSES ) && !terse)
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
                switch (damage_type(item_clas, item_typ))
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

        if (brand == SPMSL_POISONED || brand == SPMSL_POISONED_II)
        {
            strncat( buff, (terse) ? "poison " : "poisoned ", ITEMNAME_SIZE );
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
               (item_typ == MI_EGGPLANT) ? "eggplant" :
               (item_typ == MI_LARGE_ROCK) ? "large rock" : "", ITEMNAME_SIZE);
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
              (brand == SPMSL_POISONED_II) ? "" : " (buggy)", ITEMNAME_SIZE );
        }
        break;

    case OBJ_ARMOUR:
        if (item_ident( item, ISFLAG_KNOW_CURSE ) && !terse)
        {
            if (item_cursed( item ))
                strncat(buff, "cursed ", ITEMNAME_SIZE );
            else if (Options.show_uncursed 
                    && item_not_ident( item, ISFLAG_KNOW_PLUSES ))
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

        if (item_typ == ARM_GLOVES
            || (item_typ == ARM_BOOTS && item_plus2 == TBOOT_BOOTS))
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
        if (item_not_ident( item, ISFLAG_KNOW_PLUSES ) && !terse)
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

            if (id[ IDTYPE_WANDS ][item_typ] == ID_TRIED_TYPE)
            {
                strncat( buff, " {tried}" , ITEMNAME_SIZE );
            }
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
            char primary = item.special / 14;
            char secondary = item.special % 14;

            strncat(buff,
                   (primary ==  0) ? "" :
                   (primary ==  1) ? "bubbling " :
                   (primary ==  2) ? "lumpy " :
                   (primary ==  3) ? "fuming " :
                   (primary ==  4) ? "smoky " :
                   (primary ==  5) ? "fizzy " :
                   (primary ==  6) ? "glowing " :
                   (primary ==  7) ? "sedimented " :
                   (primary ==  8) ? "metallic " :
                   (primary ==  9) ? "murky " :
                   (primary == 10) ? "gluggy " :
                   (primary == 11) ? "viscous " :
                   (primary == 12) ? "oily " :
                   (primary == 13) ? "slimy " :
                   (primary == 14) ? "emulsified " : "buggy ", ITEMNAME_SIZE);

            strncat(buff, 
                   (secondary ==  0) ? "clear" :
                   (secondary ==  1) ? "blue" :
                   (secondary ==  2) ? "black" :
                   (secondary ==  3) ? "silvery" :
                   (secondary ==  4) ? "cyan" :
                   (secondary ==  5) ? "purple" :
                   (secondary ==  6) ? "orange" :
                   (secondary ==  7) ? "inky" :
                   (secondary ==  8) ? "red" :
                   (secondary ==  9) ? "yellow" :
                   (secondary == 10) ? "green" :
                   (secondary == 11) ? "brown" :
                   (secondary == 12) ? "pink" :
                   (secondary == 13) ? "white" : "buggy", ITEMNAME_SIZE);

            strncat(buff, " potion", ITEMNAME_SIZE );

            if (it_quant > 1)
                strncat(buff, "s", ITEMNAME_SIZE );

            if (id[ IDTYPE_POTIONS ][item_typ] == ID_TRIED_TYPE)
            {
                strncat( buff, " {tried}" , ITEMNAME_SIZE );
            }
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

            make_name( item.special, it_plus, item_clas, 2, buff3 );
            strncat( buff, buff3 , ITEMNAME_SIZE );

            if (id[ IDTYPE_SCROLLS ][item_typ] == ID_TRIED_TYPE)
            {
                strncat( buff, " {tried}" , ITEMNAME_SIZE );
            }
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
                    && item_not_ident( item, ISFLAG_KNOW_PLUSES ))
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
            strncat( buff, (it_plus == RUNE_DIS)          ? "iron" :
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
            strncat(buff, item_not_ident( item, ISFLAG_KNOW_TYPE )  ? "cards"  :
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
            if (item_not_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, "bone ", ITEMNAME_SIZE );
            strncat(buff, "lantern", ITEMNAME_SIZE );

            if (item_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, " of shadows", ITEMNAME_SIZE );
            break;

        case MISC_HORN_OF_GERYON:
            if (item_not_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, "silver ", ITEMNAME_SIZE );
            strncat(buff, "horn", ITEMNAME_SIZE );

            if (item_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, " of Geryon", ITEMNAME_SIZE );
            break;

        case MISC_DISC_OF_STORMS:
            if (item_not_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, "grey ", ITEMNAME_SIZE );
            strncat(buff, "disc", ITEMNAME_SIZE );

            if (item_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, " of storms", ITEMNAME_SIZE );
            break;

        case MISC_STONE_OF_EARTH_ELEMENTALS:
            if (item_not_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, "nondescript ", ITEMNAME_SIZE );
            strncat(buff, "stone", ITEMNAME_SIZE );

            if (item_ident( item, ISFLAG_KNOW_TYPE ))
                strncat(buff, " of earth elementals", ITEMNAME_SIZE );
            break;

        case MISC_BOTTLED_EFREET:
            strncat(buff, (item_not_ident( item, ISFLAG_KNOW_TYPE )) 
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
        if (item_not_ident( item, ISFLAG_KNOW_TYPE ))
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
        if (item_not_ident( item, ISFLAG_KNOW_TYPE ))
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

    return 1;
}                               // end item_name_2()

void save_id(char identy[4][50])
{
    char x = 0, jx = 0;

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

    char i = 0, j = 0;

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 50; j++)
        {
            id[i][j] = ID_UNKNOWN_TYPE;
        }
    }

}                               // end clear_ids()


void set_ident_type( char cla, char ty, char setting, bool force )
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

char get_ident_type(char cla, char ty)
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

int property( const item_def &item, int prop_type )
{
    switch (item.base_type)
    {
    case OBJ_ARMOUR:
    case OBJ_WEAPONS:
    case OBJ_MISSILES:
        return (prop[ item.base_type ][ item.sub_type ][ prop_type ]);

    case OBJ_STAVES:
        if (item_is_staff( item ))
            return (prop[ OBJ_WEAPONS ][ WPN_QUARTERSTAFF ][ prop_type ]);   
        else if (prop_type == PWPN_SPEED) // item is rod
            return (10);                  // extra protection against speed 0
        else 
            return (0);

    default:
        return (0);
    }
}

int mass_item( const item_def &item )
{
    int unit_mass = 0;

    if (item.base_type == OBJ_GOLD)
    {
        unit_mass = 0;
    }
    else if (item.base_type == OBJ_CORPSES)
    {
        unit_mass = mons_weight( item.plus );

        if (item.sub_type == CORPSE_SKELETON)
            unit_mass /= 2;
    }
    else
    {
        unit_mass = mss[ item.base_type ][ item.sub_type ];
    }

    return (unit_mass > 0 ? unit_mass : 0);
}

void init_properties(void)
{
    prop[OBJ_ARMOUR][ARM_ROBE][PARM_AC] = 1;
    prop[OBJ_ARMOUR][ARM_ROBE][PARM_EVASION] = 0;
    mss[OBJ_ARMOUR][ARM_ROBE] = 60;

    prop[OBJ_ARMOUR][ARM_LEATHER_ARMOUR][PARM_AC] = 2;
    prop[OBJ_ARMOUR][ARM_LEATHER_ARMOUR][PARM_EVASION] = -1;
    mss[OBJ_ARMOUR][ARM_LEATHER_ARMOUR] = 150;

    prop[OBJ_ARMOUR][ARM_RING_MAIL][PARM_AC] = 4;
    prop[OBJ_ARMOUR][ARM_RING_MAIL][PARM_EVASION] = -2;
    mss[OBJ_ARMOUR][ARM_RING_MAIL] = 300;

    prop[OBJ_ARMOUR][ARM_SCALE_MAIL][PARM_AC] = 5;
    prop[OBJ_ARMOUR][ARM_SCALE_MAIL][PARM_EVASION] = -2;
    mss[OBJ_ARMOUR][ARM_SCALE_MAIL] = 400;

    prop[OBJ_ARMOUR][ARM_CHAIN_MAIL][PARM_AC] = 6;
    prop[OBJ_ARMOUR][ARM_CHAIN_MAIL][PARM_EVASION] = -3;
    mss[OBJ_ARMOUR][ARM_CHAIN_MAIL] = 450;

    prop[OBJ_ARMOUR][ARM_SPLINT_MAIL][PARM_AC] = 8;
    prop[OBJ_ARMOUR][ARM_SPLINT_MAIL][PARM_EVASION] = -5;
    mss[OBJ_ARMOUR][ARM_SPLINT_MAIL] = 550;

    prop[OBJ_ARMOUR][ARM_BANDED_MAIL][PARM_AC] = 7;
    prop[OBJ_ARMOUR][ARM_BANDED_MAIL][PARM_EVASION] = -4;
    mss[OBJ_ARMOUR][ARM_BANDED_MAIL] = 500;

    prop[OBJ_ARMOUR][ARM_PLATE_MAIL][PARM_AC] = 9;
    prop[OBJ_ARMOUR][ARM_PLATE_MAIL][PARM_EVASION] = -5;
    mss[OBJ_ARMOUR][ARM_PLATE_MAIL] = 650;

    prop[OBJ_ARMOUR][ARM_DRAGON_HIDE][PARM_AC] = 2;
    prop[OBJ_ARMOUR][ARM_DRAGON_HIDE][PARM_EVASION] = -2;
    mss[OBJ_ARMOUR][ARM_DRAGON_HIDE] = 220;

    prop[OBJ_ARMOUR][ARM_TROLL_HIDE][PARM_AC] = 1;
    prop[OBJ_ARMOUR][ARM_TROLL_HIDE][PARM_EVASION] = -1;
    mss[OBJ_ARMOUR][ARM_TROLL_HIDE] = 180;

    prop[OBJ_ARMOUR][ARM_CRYSTAL_PLATE_MAIL][PARM_AC] = 16;
    prop[OBJ_ARMOUR][ARM_CRYSTAL_PLATE_MAIL][PARM_EVASION] = -8;
    mss[OBJ_ARMOUR][ARM_CRYSTAL_PLATE_MAIL] = 1200;

    prop[OBJ_ARMOUR][ARM_DRAGON_ARMOUR][PARM_AC] = 8;
    prop[OBJ_ARMOUR][ARM_DRAGON_ARMOUR][PARM_EVASION] = -2;
    mss[OBJ_ARMOUR][ARM_DRAGON_ARMOUR] = 220;

    prop[OBJ_ARMOUR][ARM_TROLL_LEATHER_ARMOUR][PARM_AC] = 3;
    prop[OBJ_ARMOUR][ARM_TROLL_LEATHER_ARMOUR][PARM_EVASION] = -1;
    mss[OBJ_ARMOUR][ARM_TROLL_LEATHER_ARMOUR] = 180;

    prop[OBJ_ARMOUR][ARM_ICE_DRAGON_HIDE][PARM_AC] = 2;
    prop[OBJ_ARMOUR][ARM_ICE_DRAGON_HIDE][PARM_EVASION] = -2;
    mss[OBJ_ARMOUR][ARM_ICE_DRAGON_HIDE] = 220;

    prop[OBJ_ARMOUR][ARM_ICE_DRAGON_ARMOUR][PARM_AC] = 9;
    prop[OBJ_ARMOUR][ARM_ICE_DRAGON_ARMOUR][PARM_EVASION] = -2;
    mss[OBJ_ARMOUR][ARM_ICE_DRAGON_ARMOUR] = 220;

    prop[OBJ_ARMOUR][ARM_STEAM_DRAGON_HIDE][PARM_AC] = 0;
    prop[OBJ_ARMOUR][ARM_STEAM_DRAGON_HIDE][PARM_EVASION] = 0;
    mss[OBJ_ARMOUR][ARM_STEAM_DRAGON_HIDE] = 120;

    prop[OBJ_ARMOUR][ARM_STEAM_DRAGON_ARMOUR][PARM_AC] = 3;
    prop[OBJ_ARMOUR][ARM_STEAM_DRAGON_ARMOUR][PARM_EVASION] = 0;
    mss[OBJ_ARMOUR][ARM_STEAM_DRAGON_ARMOUR] = 120;

    prop[OBJ_ARMOUR][ARM_MOTTLED_DRAGON_HIDE][PARM_AC] = 1;
    prop[OBJ_ARMOUR][ARM_MOTTLED_DRAGON_HIDE][PARM_EVASION] = -1;
    mss[OBJ_ARMOUR][ARM_MOTTLED_DRAGON_HIDE] = 150;

    prop[OBJ_ARMOUR][ARM_MOTTLED_DRAGON_ARMOUR][PARM_AC] = 5;
    prop[OBJ_ARMOUR][ARM_MOTTLED_DRAGON_ARMOUR][PARM_EVASION] = -1;
    mss[OBJ_ARMOUR][ARM_MOTTLED_DRAGON_ARMOUR] = 150;

    prop[OBJ_ARMOUR][ARM_STORM_DRAGON_HIDE][PARM_AC] = 2;
    prop[OBJ_ARMOUR][ARM_STORM_DRAGON_HIDE][PARM_EVASION] = -5;
    mss[OBJ_ARMOUR][ARM_STORM_DRAGON_HIDE] = 400;

    prop[OBJ_ARMOUR][ARM_STORM_DRAGON_ARMOUR][PARM_AC] = 10;
    prop[OBJ_ARMOUR][ARM_STORM_DRAGON_ARMOUR][PARM_EVASION] = -5;
    mss[OBJ_ARMOUR][ARM_STORM_DRAGON_ARMOUR] = 400;

    prop[OBJ_ARMOUR][ARM_GOLD_DRAGON_HIDE][PARM_AC] = 2;
    prop[OBJ_ARMOUR][ARM_GOLD_DRAGON_HIDE][PARM_EVASION] = -10;
    mss[OBJ_ARMOUR][ARM_GOLD_DRAGON_HIDE] = 1100;

    prop[OBJ_ARMOUR][ARM_GOLD_DRAGON_ARMOUR][PARM_AC] = 13;
    prop[OBJ_ARMOUR][ARM_GOLD_DRAGON_ARMOUR][PARM_EVASION] = -10;
    mss[OBJ_ARMOUR][ARM_GOLD_DRAGON_ARMOUR] = 1100;

    prop[OBJ_ARMOUR][ARM_ANIMAL_SKIN][PARM_AC] = 1;
    prop[OBJ_ARMOUR][ARM_ANIMAL_SKIN][PARM_EVASION] = 0;
    mss[OBJ_ARMOUR][ARM_ANIMAL_SKIN] = 100;

    prop[OBJ_ARMOUR][ARM_SWAMP_DRAGON_HIDE][PARM_AC] = 1;
    prop[OBJ_ARMOUR][ARM_SWAMP_DRAGON_HIDE][PARM_EVASION] = -2;
    mss[OBJ_ARMOUR][ARM_SWAMP_DRAGON_HIDE] = 200;

    prop[OBJ_ARMOUR][ARM_SWAMP_DRAGON_ARMOUR][PARM_AC] = 7;
    prop[OBJ_ARMOUR][ARM_SWAMP_DRAGON_ARMOUR][PARM_EVASION] = -2;
    mss[OBJ_ARMOUR][ARM_SWAMP_DRAGON_ARMOUR] = 200;

    prop[OBJ_ARMOUR][ARM_SHIELD][PARM_AC] = 0;
    prop[OBJ_ARMOUR][ARM_SHIELD][PARM_EVASION] = 0;
    mss[OBJ_ARMOUR][ARM_SHIELD] = 100;

    prop[OBJ_ARMOUR][ARM_CLOAK][PARM_AC] = 1;
    prop[OBJ_ARMOUR][ARM_CLOAK][PARM_EVASION] = 0;
    mss[OBJ_ARMOUR][ARM_CLOAK] = 20;

    prop[OBJ_ARMOUR][ARM_HELMET][PARM_AC] = 1;
    prop[OBJ_ARMOUR][ARM_HELMET][PARM_EVASION] = 0;
    mss[OBJ_ARMOUR][ARM_HELMET] = 80;

    prop[OBJ_ARMOUR][ARM_GLOVES][PARM_AC] = 1;
    prop[OBJ_ARMOUR][ARM_GLOVES][PARM_EVASION] = 0;
    mss[OBJ_ARMOUR][ARM_GLOVES] = 20;

    prop[OBJ_ARMOUR][ARM_BOOTS][PARM_AC] = 1;
    prop[OBJ_ARMOUR][ARM_BOOTS][PARM_EVASION] = 0;
    mss[OBJ_ARMOUR][ARM_BOOTS] = 40;

    prop[OBJ_ARMOUR][ARM_BUCKLER][PARM_AC] = 0;
    prop[OBJ_ARMOUR][ARM_BUCKLER][PARM_EVASION] = 0;
    mss[OBJ_ARMOUR][ARM_BUCKLER] = 50;

    prop[OBJ_ARMOUR][ARM_LARGE_SHIELD][PARM_AC] = 0;
    prop[OBJ_ARMOUR][ARM_LARGE_SHIELD][PARM_EVASION] = 0;
    mss[OBJ_ARMOUR][ARM_LARGE_SHIELD] = 250;

    int i = 0;

    for (i = 0; i < 50; i++)
    {
        mss[OBJ_WANDS][i] = 100;
        mss[OBJ_FOOD][i] = 100;
        mss[OBJ_UNKNOWN_I][i] = 200;    // labeled as "books" elsewhere

        //jmf: made scrolls, jewellery and potions weigh less.
        mss[OBJ_SCROLLS][i] = 20;
        mss[OBJ_JEWELLERY][i] = 10;
        mss[OBJ_POTIONS][i] = 40;
        mss[OBJ_UNKNOWN_II][i] = 5;     // "gems"
        mss[OBJ_BOOKS][i] = 70;

        mss[OBJ_STAVES][i] = 130;
        mss[OBJ_ORBS][i] = 300;
        mss[OBJ_MISCELLANY][i] = 100;
        mss[OBJ_CORPSES][i] = 100;
    }

    // rods are lighter than staves
    for (i = STAFF_SMITING; i < STAFF_AIR; i++)
    {
        mss[OBJ_STAVES][i] = 70; 
    }

    // this is food, right?
    mss[OBJ_FOOD][FOOD_MEAT_RATION] = 80;
    mss[OBJ_FOOD][FOOD_BREAD_RATION] = 80;
    mss[OBJ_FOOD][FOOD_PEAR] = 20;
    mss[OBJ_FOOD][FOOD_APPLE] = 20;
    mss[OBJ_FOOD][FOOD_CHOKO] = 30;
    mss[OBJ_FOOD][FOOD_HONEYCOMB] = 40;
    mss[OBJ_FOOD][FOOD_ROYAL_JELLY] = 55;
    mss[OBJ_FOOD][FOOD_SNOZZCUMBER] = 50;
    mss[OBJ_FOOD][FOOD_PIZZA] = 40;
    mss[OBJ_FOOD][FOOD_APRICOT] = 15;
    mss[OBJ_FOOD][FOOD_ORANGE] = 20;
    mss[OBJ_FOOD][FOOD_BANANA] = 20;
    mss[OBJ_FOOD][FOOD_STRAWBERRY] = 5;
    mss[OBJ_FOOD][FOOD_RAMBUTAN] = 10;
    mss[OBJ_FOOD][FOOD_LEMON] = 20;
    mss[OBJ_FOOD][FOOD_GRAPE] = 5;
    mss[OBJ_FOOD][FOOD_SULTANA] = 3;
    mss[OBJ_FOOD][FOOD_LYCHEE] = 10;
    mss[OBJ_FOOD][FOOD_BEEF_JERKY] = 20;
    mss[OBJ_FOOD][FOOD_CHEESE] = 40;
    mss[OBJ_FOOD][FOOD_SAUSAGE] = 40;
    mss[OBJ_FOOD][FOOD_CHUNK] = 100;
    /* mss [OBJ_FOOD] [21] = 40;
       mss [OBJ_FOOD] [22] = 50;
       mss [OBJ_FOOD] [23] = 60;
       mss [OBJ_FOOD] [24] = 60;
       mss [OBJ_FOOD] [25] = 100; */

    mss[OBJ_MISCELLANY][MISC_BOTTLED_EFREET] = 250;

    mss[OBJ_MISCELLANY][MISC_CRYSTAL_BALL_OF_SEEING] = 200;
    mss[OBJ_MISCELLANY][MISC_CRYSTAL_BALL_OF_ENERGY] = 200;
    mss[OBJ_MISCELLANY][MISC_CRYSTAL_BALL_OF_FIXATION] = 200;

    // weapons: blunt weapons are first to help grouping them together
    //  note: AC prop can't be 0 or less because of division.
    //        If it's 1, makes no difference

    // NOTE: I have *removed* AC bit for weapons - just doesn't work
    // prop [x] [2] is speed

    prop[OBJ_WEAPONS][WPN_CLUB][PWPN_DAMAGE] = 5;
    prop[OBJ_WEAPONS][WPN_CLUB][PWPN_HIT] = 4;  // helps to get past evasion
    prop[OBJ_WEAPONS][WPN_CLUB][PWPN_SPEED] = 12;
    mss[OBJ_WEAPONS][WPN_CLUB] = 50;

    prop[OBJ_WEAPONS][WPN_MACE][PWPN_DAMAGE] = 8;
    prop[OBJ_WEAPONS][WPN_MACE][PWPN_HIT] = 3;  // helps to get past evasion
    prop[OBJ_WEAPONS][WPN_MACE][PWPN_SPEED] = 14;
    mss[OBJ_WEAPONS][WPN_MACE] = 140;

    prop[OBJ_WEAPONS][WPN_GREAT_MACE][PWPN_DAMAGE] = 16;
    prop[OBJ_WEAPONS][WPN_GREAT_MACE][PWPN_HIT] = -3;
    prop[OBJ_WEAPONS][WPN_GREAT_MACE][PWPN_SPEED] = 18;
    mss[OBJ_WEAPONS][WPN_GREAT_MACE] = 260;

    prop[OBJ_WEAPONS][WPN_FLAIL][PWPN_DAMAGE] = 9;
    prop[OBJ_WEAPONS][WPN_FLAIL][PWPN_HIT] = 2; // helps to get past evasion
    prop[OBJ_WEAPONS][WPN_FLAIL][PWPN_SPEED] = 15;
    mss[OBJ_WEAPONS][WPN_FLAIL] = 150;

    prop[OBJ_WEAPONS][WPN_SPIKED_FLAIL][PWPN_DAMAGE] = 12;
    prop[OBJ_WEAPONS][WPN_SPIKED_FLAIL][PWPN_HIT] = 1;
    prop[OBJ_WEAPONS][WPN_SPIKED_FLAIL][PWPN_SPEED] = 16;
    mss[OBJ_WEAPONS][WPN_SPIKED_FLAIL] = 170;

    prop[OBJ_WEAPONS][WPN_GREAT_FLAIL][PWPN_DAMAGE] = 17;
    prop[OBJ_WEAPONS][WPN_GREAT_FLAIL][PWPN_HIT] = -4;
    prop[OBJ_WEAPONS][WPN_GREAT_FLAIL][PWPN_SPEED] = 19;
    mss[OBJ_WEAPONS][WPN_GREAT_FLAIL] = 300;

    prop[OBJ_WEAPONS][WPN_DAGGER][PWPN_DAMAGE] = 3;
    prop[OBJ_WEAPONS][WPN_DAGGER][PWPN_HIT] = 6;  // helps to get past evasion
    prop[OBJ_WEAPONS][WPN_DAGGER][PWPN_SPEED] = 11;
    mss[OBJ_WEAPONS][WPN_DAGGER] = 20;

    prop[OBJ_WEAPONS][WPN_KNIFE][PWPN_DAMAGE] = 2;
    prop[OBJ_WEAPONS][WPN_KNIFE][PWPN_HIT] = 0; // helps to get past evasion
    prop[OBJ_WEAPONS][WPN_KNIFE][PWPN_SPEED] = 11;
    mss[OBJ_WEAPONS][WPN_KNIFE] = 10;

    prop[OBJ_WEAPONS][WPN_MORNINGSTAR][PWPN_DAMAGE] = 10;
    prop[OBJ_WEAPONS][WPN_MORNINGSTAR][PWPN_HIT] = 2;
    prop[OBJ_WEAPONS][WPN_MORNINGSTAR][PWPN_SPEED] = 15;
    mss[OBJ_WEAPONS][WPN_MORNINGSTAR] = 150;

    prop[OBJ_WEAPONS][WPN_SHORT_SWORD][PWPN_DAMAGE] = 6;
    prop[OBJ_WEAPONS][WPN_SHORT_SWORD][PWPN_HIT] = 5;
    prop[OBJ_WEAPONS][WPN_SHORT_SWORD][PWPN_SPEED] = 12;
    mss[OBJ_WEAPONS][WPN_SHORT_SWORD] = 100;

    prop[OBJ_WEAPONS][WPN_LONG_SWORD][PWPN_DAMAGE] = 10;
    prop[OBJ_WEAPONS][WPN_LONG_SWORD][PWPN_HIT] = 3;
    prop[OBJ_WEAPONS][WPN_LONG_SWORD][PWPN_SPEED] = 14;
    mss[OBJ_WEAPONS][WPN_LONG_SWORD] = 160;

    prop[OBJ_WEAPONS][WPN_GREAT_SWORD][PWPN_DAMAGE] = 16;
    prop[OBJ_WEAPONS][WPN_GREAT_SWORD][PWPN_HIT] = -1;
    prop[OBJ_WEAPONS][WPN_GREAT_SWORD][PWPN_SPEED] = 17;
    mss[OBJ_WEAPONS][WPN_GREAT_SWORD] = 250;

    prop[OBJ_WEAPONS][WPN_FALCHION][PWPN_DAMAGE] = 8;
    prop[OBJ_WEAPONS][WPN_FALCHION][PWPN_HIT] = 2;
    prop[OBJ_WEAPONS][WPN_FALCHION][PWPN_SPEED] = 13;
    mss[OBJ_WEAPONS][WPN_FALCHION] = 130;

    prop[OBJ_WEAPONS][WPN_SCIMITAR][PWPN_DAMAGE] = 11;
    prop[OBJ_WEAPONS][WPN_SCIMITAR][PWPN_HIT] = 1;
    prop[OBJ_WEAPONS][WPN_SCIMITAR][PWPN_SPEED] = 14;
    mss[OBJ_WEAPONS][WPN_SCIMITAR] = 170;

    prop[OBJ_WEAPONS][WPN_HAND_AXE][PWPN_DAMAGE] = 7;
    prop[OBJ_WEAPONS][WPN_HAND_AXE][PWPN_HIT] = 2;
    prop[OBJ_WEAPONS][WPN_HAND_AXE][PWPN_SPEED] = 13;
    mss[OBJ_WEAPONS][WPN_HAND_AXE] = 110;

    prop[OBJ_WEAPONS][WPN_WAR_AXE][PWPN_DAMAGE] = 11;
    prop[OBJ_WEAPONS][WPN_WAR_AXE][PWPN_HIT] = 0;
    prop[OBJ_WEAPONS][WPN_WAR_AXE][PWPN_SPEED] = 16;
    mss[OBJ_WEAPONS][WPN_WAR_AXE] = 150;

    prop[OBJ_WEAPONS][WPN_BROAD_AXE][PWPN_DAMAGE] = 14;
    prop[OBJ_WEAPONS][WPN_BROAD_AXE][PWPN_HIT] = 1;
    prop[OBJ_WEAPONS][WPN_BROAD_AXE][PWPN_SPEED] = 17;
    mss[OBJ_WEAPONS][WPN_BROAD_AXE] = 180;

    prop[OBJ_WEAPONS][WPN_BATTLEAXE][PWPN_DAMAGE] = 17;
    prop[OBJ_WEAPONS][WPN_BATTLEAXE][PWPN_HIT] = -2;
    prop[OBJ_WEAPONS][WPN_BATTLEAXE][PWPN_SPEED] = 18;
    mss[OBJ_WEAPONS][WPN_BATTLEAXE] = 200;

    prop[OBJ_WEAPONS][WPN_SPEAR][PWPN_DAMAGE] = 5;
    prop[OBJ_WEAPONS][WPN_SPEAR][PWPN_HIT] = 3;
    prop[OBJ_WEAPONS][WPN_SPEAR][PWPN_SPEED] = 13;
    mss[OBJ_WEAPONS][WPN_SPEAR] = 50;

    prop[OBJ_WEAPONS][WPN_TRIDENT][PWPN_DAMAGE] = 9;
    prop[OBJ_WEAPONS][WPN_TRIDENT][PWPN_HIT] = -2;
    prop[OBJ_WEAPONS][WPN_TRIDENT][PWPN_SPEED] = 17;
    mss[OBJ_WEAPONS][WPN_TRIDENT] = 160;

    prop[OBJ_WEAPONS][WPN_DEMON_TRIDENT][PWPN_DAMAGE] = 15;
    prop[OBJ_WEAPONS][WPN_DEMON_TRIDENT][PWPN_HIT] = -2;
    prop[OBJ_WEAPONS][WPN_DEMON_TRIDENT][PWPN_SPEED] = 17;
    mss[OBJ_WEAPONS][WPN_DEMON_TRIDENT] = 160;

    prop[OBJ_WEAPONS][WPN_HALBERD][PWPN_DAMAGE] = 13;
    prop[OBJ_WEAPONS][WPN_HALBERD][PWPN_HIT] = -3;
    prop[OBJ_WEAPONS][WPN_HALBERD][PWPN_SPEED] = 19;
    mss[OBJ_WEAPONS][WPN_HALBERD] = 200;

    // sling
    // - the three properties are _not_ irrelevant here
    // - when something hits something else (either may be you)
    //   in melee, these are used.
    prop[OBJ_WEAPONS][WPN_SLING][PWPN_DAMAGE] = 1;
    prop[OBJ_WEAPONS][WPN_SLING][PWPN_HIT] = -1;
    prop[OBJ_WEAPONS][WPN_SLING][PWPN_SPEED] = 11;
    mss[OBJ_WEAPONS][WPN_SLING] = 10;

    prop[OBJ_WEAPONS][WPN_BOW][PWPN_DAMAGE] = 2;
    prop[OBJ_WEAPONS][WPN_BOW][PWPN_HIT] = -3;  // helps to get past evasion
    prop[OBJ_WEAPONS][WPN_BOW][PWPN_SPEED] = 11;
    mss[OBJ_WEAPONS][WPN_BOW] = 100;

    prop[OBJ_WEAPONS][WPN_BLOWGUN][PWPN_DAMAGE] = 1;
    prop[OBJ_WEAPONS][WPN_BLOWGUN][PWPN_HIT] = 0;  // helps to get past evasion
    prop[OBJ_WEAPONS][WPN_BLOWGUN][PWPN_SPEED] = 10;
    mss[OBJ_WEAPONS][WPN_BLOWGUN] = 50;

    prop[OBJ_WEAPONS][WPN_CROSSBOW][PWPN_DAMAGE] = 2;
    prop[OBJ_WEAPONS][WPN_CROSSBOW][PWPN_HIT] = -1;
    prop[OBJ_WEAPONS][WPN_CROSSBOW][PWPN_SPEED] = 15;
    mss[OBJ_WEAPONS][WPN_CROSSBOW] = 250;

    prop[OBJ_WEAPONS][WPN_HAND_CROSSBOW][PWPN_DAMAGE] = 1;
    prop[OBJ_WEAPONS][WPN_HAND_CROSSBOW][PWPN_HIT] = -1;
    prop[OBJ_WEAPONS][WPN_HAND_CROSSBOW][PWPN_SPEED] = 15;
    mss[OBJ_WEAPONS][WPN_HAND_CROSSBOW] = 70;

    prop[OBJ_WEAPONS][WPN_GLAIVE][PWPN_DAMAGE] = 15;
    prop[OBJ_WEAPONS][WPN_GLAIVE][PWPN_HIT] = -3;
    prop[OBJ_WEAPONS][WPN_GLAIVE][PWPN_SPEED] = 18;
    mss[OBJ_WEAPONS][WPN_GLAIVE] = 200;

    // staff - hmmm
    prop[OBJ_WEAPONS][WPN_QUARTERSTAFF][PWPN_DAMAGE] = 7;
    prop[OBJ_WEAPONS][WPN_QUARTERSTAFF][PWPN_HIT] = 6;
    prop[OBJ_WEAPONS][WPN_QUARTERSTAFF][PWPN_SPEED] = 12;
    mss[OBJ_WEAPONS][WPN_QUARTERSTAFF] = 130;

    prop[OBJ_WEAPONS][WPN_SCYTHE][PWPN_DAMAGE] = 14;
    prop[OBJ_WEAPONS][WPN_SCYTHE][PWPN_HIT] = -4;
    prop[OBJ_WEAPONS][WPN_SCYTHE][PWPN_SPEED] = 22;
    mss[OBJ_WEAPONS][WPN_SCYTHE] = 230;

    // these two should have the same speed because of 2-h ogres.
    prop[OBJ_WEAPONS][WPN_GIANT_CLUB][PWPN_DAMAGE] = 15;
    prop[OBJ_WEAPONS][WPN_GIANT_CLUB][PWPN_HIT] = -5;
    prop[OBJ_WEAPONS][WPN_GIANT_CLUB][PWPN_SPEED] = 16;
    mss[OBJ_WEAPONS][WPN_GIANT_CLUB] = 750;

    prop[OBJ_WEAPONS][WPN_GIANT_SPIKED_CLUB][PWPN_DAMAGE] = 18;
    prop[OBJ_WEAPONS][WPN_GIANT_SPIKED_CLUB][PWPN_HIT] = -6;
    prop[OBJ_WEAPONS][WPN_GIANT_SPIKED_CLUB][PWPN_SPEED] = 17;
    mss[OBJ_WEAPONS][WPN_GIANT_SPIKED_CLUB] = 850;
    // these two should have the same speed because of 2-h ogres.

    prop[OBJ_WEAPONS][WPN_EVENINGSTAR][PWPN_DAMAGE] = 12;
    prop[OBJ_WEAPONS][WPN_EVENINGSTAR][PWPN_HIT] = 2;
    prop[OBJ_WEAPONS][WPN_EVENINGSTAR][PWPN_SPEED] = 15;
    mss[OBJ_WEAPONS][WPN_EVENINGSTAR] = 150;

    prop[OBJ_WEAPONS][WPN_QUICK_BLADE][PWPN_DAMAGE] = 5;
    prop[OBJ_WEAPONS][WPN_QUICK_BLADE][PWPN_HIT] = 6;
    prop[OBJ_WEAPONS][WPN_QUICK_BLADE][PWPN_SPEED] = 7;
    mss[OBJ_WEAPONS][WPN_QUICK_BLADE] = 100;

    prop[OBJ_WEAPONS][WPN_KATANA][PWPN_DAMAGE] = 13;
    prop[OBJ_WEAPONS][WPN_KATANA][PWPN_HIT] = 4;
    prop[OBJ_WEAPONS][WPN_KATANA][PWPN_SPEED] = 13;
    mss[OBJ_WEAPONS][WPN_KATANA] = 160;

    prop[OBJ_WEAPONS][WPN_EXECUTIONERS_AXE][PWPN_DAMAGE] = 20;
    prop[OBJ_WEAPONS][WPN_EXECUTIONERS_AXE][PWPN_HIT] = -4;
    prop[OBJ_WEAPONS][WPN_EXECUTIONERS_AXE][PWPN_SPEED] = 20;
    mss[OBJ_WEAPONS][WPN_EXECUTIONERS_AXE] = 320;

    prop[OBJ_WEAPONS][WPN_DOUBLE_SWORD][PWPN_DAMAGE] = 15;
    prop[OBJ_WEAPONS][WPN_DOUBLE_SWORD][PWPN_HIT] = 3;
    prop[OBJ_WEAPONS][WPN_DOUBLE_SWORD][PWPN_SPEED] = 16;
    mss[OBJ_WEAPONS][WPN_DOUBLE_SWORD] = 220;

    prop[OBJ_WEAPONS][WPN_TRIPLE_SWORD][PWPN_DAMAGE] = 19;
    prop[OBJ_WEAPONS][WPN_TRIPLE_SWORD][PWPN_HIT] = -1;
    prop[OBJ_WEAPONS][WPN_TRIPLE_SWORD][PWPN_SPEED] = 19;
    mss[OBJ_WEAPONS][WPN_TRIPLE_SWORD] = 300;

    prop[OBJ_WEAPONS][WPN_HAMMER][PWPN_DAMAGE] = 7;
    prop[OBJ_WEAPONS][WPN_HAMMER][PWPN_HIT] = 2;
    prop[OBJ_WEAPONS][WPN_HAMMER][PWPN_SPEED] = 13;
    mss[OBJ_WEAPONS][WPN_HAMMER] = 130;

    prop[OBJ_WEAPONS][WPN_ANCUS][PWPN_DAMAGE] = 9;
    prop[OBJ_WEAPONS][WPN_ANCUS][PWPN_HIT] = 1;
    prop[OBJ_WEAPONS][WPN_ANCUS][PWPN_SPEED] = 14;
    mss[OBJ_WEAPONS][WPN_ANCUS] = 160;

    prop[OBJ_WEAPONS][WPN_WHIP][PWPN_DAMAGE] = 3;
    prop[OBJ_WEAPONS][WPN_WHIP][PWPN_HIT] = 1;  // helps to get past evasion
    prop[OBJ_WEAPONS][WPN_WHIP][PWPN_SPEED] = 14;
    mss[OBJ_WEAPONS][WPN_WHIP] = 30;

    prop[OBJ_WEAPONS][WPN_SABRE][PWPN_DAMAGE] = 7;
    prop[OBJ_WEAPONS][WPN_SABRE][PWPN_HIT] = 4;
    prop[OBJ_WEAPONS][WPN_SABRE][PWPN_SPEED] = 12;
    mss[OBJ_WEAPONS][WPN_SABRE] = 110;

    prop[OBJ_WEAPONS][WPN_DEMON_BLADE][PWPN_DAMAGE] = 13;
    prop[OBJ_WEAPONS][WPN_DEMON_BLADE][PWPN_HIT] = 2;
    prop[OBJ_WEAPONS][WPN_DEMON_BLADE][PWPN_SPEED] = 15;
    mss[OBJ_WEAPONS][WPN_DEMON_BLADE] = 200;

    prop[OBJ_WEAPONS][WPN_DEMON_WHIP][PWPN_DAMAGE] = 10;
    prop[OBJ_WEAPONS][WPN_DEMON_WHIP][PWPN_HIT] = 1;
    prop[OBJ_WEAPONS][WPN_DEMON_WHIP][PWPN_SPEED] = 14;
    mss[OBJ_WEAPONS][WPN_DEMON_WHIP] = 30;


    // MISSILES:

    prop[OBJ_MISSILES][MI_STONE][PWPN_DAMAGE] = 2;
    prop[OBJ_MISSILES][MI_STONE][PWPN_HIT] = 4;
    mss[OBJ_MISSILES][MI_STONE] = 5;

    prop[OBJ_MISSILES][MI_ARROW][PWPN_DAMAGE] = 2;
    prop[OBJ_MISSILES][MI_ARROW][PWPN_HIT] = 6;
    mss[OBJ_MISSILES][MI_ARROW] = 10;

    prop[OBJ_MISSILES][MI_NEEDLE][PWPN_DAMAGE] = 0;
    prop[OBJ_MISSILES][MI_NEEDLE][PWPN_HIT] = 1;
    mss[OBJ_MISSILES][MI_NEEDLE] = 1;

    prop[OBJ_MISSILES][MI_BOLT][PWPN_DAMAGE] = 2;
    prop[OBJ_MISSILES][MI_BOLT][PWPN_HIT] = 8;
    mss[OBJ_MISSILES][MI_BOLT] = 12;

    prop[OBJ_MISSILES][MI_DART][PWPN_DAMAGE] = 2;
    prop[OBJ_MISSILES][MI_DART][PWPN_HIT] = 4;  //whatever - for hand crossbow
    mss[OBJ_MISSILES][MI_DART] = 5;

    // large rock
    prop[OBJ_MISSILES][MI_LARGE_ROCK][PWPN_DAMAGE] = 20;
    prop[OBJ_MISSILES][MI_LARGE_ROCK][PWPN_HIT] = 10;
    mss[OBJ_MISSILES][MI_LARGE_ROCK] = 1000;
}


unsigned char check_item_knowledge(void)
{
    char st_pass[ITEMNAME_SIZE] = "";
    int i, j;
    char lines = 0;
    unsigned char anything = 0;
    int ft = 0;
    int max = 0;
    int yps = 0;
    int inv_count = 0;
    unsigned char ki = 0;

    const int num_lines = get_number_of_lines();

#ifdef DOS_TERM
    char buffer[2400];

    gettext(35, 1, 80, 25, buffer);
#endif

#ifdef DOS_TERM
    window(35, 1, 80, 25);
#endif

    clrscr();

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 30; j++)
        {
            if (id[i][j] == ID_KNOWN_TYPE)
                inv_count++;
        }
    }

    if (inv_count == 0)
    {
        cprintf("You don't recognise anything yet!");
        if (getch() == 0)
            getch();
        goto putty;
    }

    textcolor(BLUE);
    cprintf("  You recognise:");
    textcolor(LIGHTGREY);
    lines++;

    for (i = 0; i < 4; i++)
    {
        switch (i)
        {
        case IDTYPE_WANDS:
            ft = OBJ_WANDS;
            max = NUM_WANDS;
            break;
        case IDTYPE_SCROLLS:
            ft = OBJ_SCROLLS;
            max = NUM_SCROLLS;
            break;
        case IDTYPE_JEWELLERY:
            ft = OBJ_JEWELLERY;
            max = NUM_JEWELLERY;
            break;
        case IDTYPE_POTIONS:
            ft = OBJ_POTIONS;
            max = NUM_POTIONS;
            break;
        }

        for (j = 0; j < max; j++)
        {
            if (lines > num_lines - 2 && inv_count > 0)
            {
                gotoxy(1, num_lines);
                cprintf("-more-");

                ki = getch();

                if (ki == ESCAPE)
                {
#ifdef DOS_TERM
                    puttext(35, 1, 80, 25, buffer);
#endif
                    return ESCAPE;
                }
                if (ki >= 'A' && ki <= 'z')
                {
#ifdef DOS_TERM
                    puttext(35, 1, 80, 25, buffer);
#endif
                    return ki;
                }

                if (ki == 0)
                    ki = getch();

                lines = 0;
                clrscr();
                gotoxy(1, 1);
                anything = 0;
            }

            int ident_level = get_ident_type( ft, j );

            if (ident_level == ID_KNOWN_TYPE)
            {
                anything++;

                if (lines > 0)
                    cprintf(EOL);
                lines++;
                cprintf(" ");

                yps = wherey();

                // item_name now requires a "real" item, so we'll create a tmp
                item_def tmp = { ft, j, 0, 0, 0, 1, 0, 0, 0, 0, 0 };
                item_name( tmp, DESC_PLAIN, st_pass );

                cprintf(st_pass);

                inv_count--;

                if (wherey() != yps)
                    lines++;
            }
        }                       // end of j loop
    }

    if (anything > 0)
    {
        ki = getch();
        //ki = getch();
        //ki = anything;

        if (ki >= 'A' && ki <= 'z')
        {
#ifdef DOS_TERM
            puttext(35, 1, 80, 25, buffer);
#endif
            return ki;
        }

        if (ki == 0)
            ki = getch();
#ifdef DOS_TERM
        puttext(35, 1, 80, 25, buffer);
#endif
        return anything;
    }

  putty:
#ifdef DOS_TERM
    puttext(35, 1, 80, 25, buffer);
#endif

    return ki;
}                               // end check_item_knowledge()


// must be certain that you are passing the subtype
// to an OBJ_ARMOUR and nothing else, or as they say,
// "Bad Things Will Happen" {dlb}:
bool hide2armour( unsigned char *which_subtype )
{
    switch (*which_subtype)
    {
    case ARM_DRAGON_HIDE:
        *which_subtype = ARM_DRAGON_ARMOUR;
        return true;
    case ARM_TROLL_HIDE:
        *which_subtype = ARM_TROLL_LEATHER_ARMOUR;
        return true;
    case ARM_ICE_DRAGON_HIDE:
        *which_subtype = ARM_ICE_DRAGON_ARMOUR;
        return true;
    case ARM_MOTTLED_DRAGON_HIDE:
        *which_subtype = ARM_MOTTLED_DRAGON_ARMOUR;
        return true;
    case ARM_STORM_DRAGON_HIDE:
        *which_subtype = ARM_STORM_DRAGON_ARMOUR;
        return true;
    case ARM_GOLD_DRAGON_HIDE:
        *which_subtype = ARM_GOLD_DRAGON_ARMOUR;
        return true;
    case ARM_SWAMP_DRAGON_HIDE:
        *which_subtype = ARM_SWAMP_DRAGON_ARMOUR;
        return true;
    case ARM_STEAM_DRAGON_HIDE:
        *which_subtype = ARM_STEAM_DRAGON_ARMOUR;
        return true;
    default:
        return false;
    }
}                               // end hide2armour()


void make_name(unsigned char var1, unsigned char var2, unsigned char var3,
               char ncase, char buff[ITEMNAME_SIZE])
{
    char name[ ITEMNAME_SIZE ] = "";
    FixedVector < unsigned char, 15 > numb;
    char len;
    char i = 0;
    char nexty = 0;
    char j = 0;
    char igo = 0;

    int x = 0;

    numb[0] = var1 * var2;
    numb[1] = var1 * var3;
    numb[2] = var2 * var3;
    numb[3] = var1 * var2 * var3;
    numb[4] = var1 + var2;
    numb[5] = var2 + var3;
    numb[6] = var1 * var2 + var3;
    numb[7] = var1 * var3 + var2;
    numb[8] = var2 * var3 + var1;
    numb[9] = var1 * var2 * var3 - var1;
    numb[10] = var1 + var2 + var2;
    numb[11] = var2 + var3 * var1;
    numb[12] = var1 * var2 * var3;
    numb[13] = var1 * var3 * var1;
    numb[14] = var2 * var3 * var3;

    for (i = 0; i < 15; i++)
    {
        while (numb[i] >= 25)
        {
            numb[i] -= 25;
        }
    }

    j = numb[6];

    len = reduce(numb[reduce(numb[11]) / 2]);

    while (len < 5 && j < 10)
    {
        len += 1 + reduce(1 + numb[j]);
        j++;
    }

    while (len > 14)
    {
        len -= 8;
    }

    nexty = retbit(numb[4]);

    char k = 0;

    j = 0;

    for (i = 0; i < len; i++)
    {
        j++;

        if (j >= 15)
        {
            j = 0;

            k++;

            if (k > 9)
                break;
        }

        if (nexty == 1 || (i > 0 && !is_random_name_vowel(name[i])))
        {
            name[i] = retvow(numb[j]);
            if ((i == 0 || i == len - 1) && name[i] == 32)
            {
                i--;
                continue;
            }
        }
        else
        {
            if (numb[i / 2] <= 1 && i > 3 && is_random_name_vowel(name[i]))
                goto two_letter;
            else
                name[i] = numb[j];

          hello:
            igo++;
        }

        if ((nexty == 0 && is_random_name_vowel(name[i]))
            || (nexty == 1 && !is_random_name_vowel(name[i])))
        {
            if (nexty == 1 && i > 0 && !is_random_name_vowel(name[i - 1]))
                i--;

            i--;
            continue;
        }

        if (!is_random_name_vowel(name[i]))
            nexty = 1;
        else
            nexty = 0;

        x++;
    }

    switch (ncase)
    {
    case 2:
        for (i = 0; i < len + 1; i++)
        {
            if (i > 3 && name[i] == 0 && name[i + 1] == 0)
            {
                name[i] = 0;
                if (name[i - 1] == 32)
                    name[i - 1] = 0;
                break;
            }
            if (name[i] != 32 && name[i] < 30)
                name[i] += 65;
            if (name[i] > 96)
                name[i] -= 32;
        }
        break;

    case 3:
        for (i = 0; i < len + 0; i++)
        {
            if (i != 0 && name[i] >= 65 && name[i] < 97)
            {
                if (name[i - 1] == 32)
                    name[i] += 32;
            }

            if (name[i] > 97)
            {
                if (i == 0 || name[i - 1] == 32)
                    name[i] -= 32;
            }

            if (name[i] < 30)
            {
                if (i == 0 || (name[i] != 32 && name[i - 1] == 32))
                    name[i] += 65;
                else
                    name[i] += 97;
            }
        }
        break;

    case 0:
        for (i = 0; i < len; i++)
        {
            if (name[i] != 32 && name[i] < 30)
                name[i] += 97;
        }
        break;

    case 1:
        name[i] += 65;

        for (i = 1; i < len; i++)
        {
            if (name[i] != 32 && name[i] < 30)
                name[i] += 97;  //97;
        }
        break;
    }

    if (strlen( name ) == 0)
        strncpy( buff, "Plog", 80 );
    else
        strncpy( buff, name, 80 );

    buff[79] = '\0';
    return;

  two_letter:
    if (nexty == 1)
        goto hello;

    if (!is_random_name_vowel(name[i - 1]))
        goto hello;

    i++;

    switch (i * (retbit(j) + 1))
    {
    case 0:
        strcat(name, "sh");
        break;
    case 1:
        strcat(name, "ch");
        break;
    case 2:
        strcat(name, "tz");
        break;
    case 3:
        strcat(name, "ts");
        break;
    case 4:
        strcat(name, "cs");
        break;
    case 5:
        strcat(name, "cz");
        break;
    case 6:
        strcat(name, "xt");
        break;
    case 7:
        strcat(name, "xk");
        break;
    case 8:
        strcat(name, "kl");
        break;
    case 9:
        strcat(name, "cl");
        break;
    case 10:
        strcat(name, "fr");
        break;
    case 11:
        strcat(name, "sh");
        break;
    case 12:
        strcat(name, "ch");
        break;
    case 13:
        strcat(name, "gh");
        break;
    case 14:
        strcat(name, "pr");
        break;
    case 15:
        strcat(name, "tr");
        break;
    case 16:
        strcat(name, "mn");
        break;
    case 17:
        strcat(name, "ph");
        break;
    case 18:
        strcat(name, "pn");
        break;
    case 19:
        strcat(name, "cv");
        break;
    case 20:
        strcat(name, "zx");
        break;
    case 21:
        strcat(name, "xz");
        break;
    case 23:
        strcat(name, "dd");
        break;
    case 24:
        strcat(name, "tt");
        break;
    case 25:
        strcat(name, "ll");
        break;
        //case 26: strcat(name, "sh"); break;
        //case 12: strcat(name, "sh"); break;
        //case 13: strcat(name, "sh"); break;
    default:
        i--;
        goto hello;
    }

    x += 2;

    goto hello;
}                               // end make_name()


char reduce(unsigned char reducee)
{
    while (reducee >= 26)
    {
        reducee -= 26;
    }

    return reducee;
}                               // end reduce()

bool is_random_name_vowel(unsigned char let)
{
    return (let == 0 || let == 4 || let == 8 || let == 14 || let == 20
            || let == 24 || let == 32);
}                               // end is_random_name_vowel()

char retvow(char sed)
{

    while (sed > 6)
        sed -= 6;

    switch (sed)
    {
    case 0:
        return 0;
    case 1:
        return 4;
    case 2:
        return 8;
    case 3:
        return 14;
    case 4:
        return 20;
    case 5:
        return 24;
    case 6:
        return 32;
    }

    return 0;
}                               // end retvow()

char retbit(char sed)
{
    return (sed % 2);
}                               // end retbit()
