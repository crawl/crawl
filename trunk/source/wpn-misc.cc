/*
 *********************************************************************
 *  File:       wpn-misc.cc                                          *
 *  Summary:    temporary home for weapon f(x) until struct'ed       *
 *  Written by: don brodale <dbrodale@bigfootinteractive.com>        *
 *                                                                   *
 *  Changelog(most recent first):                                    *
 *                                                                   *
 *  <00>     12jun2000     dlb     created after little thought      *
 *********************************************************************
 */

#include "AppHdr.h"
#include "wpn-misc.h"

#include "externs.h"

// all of this will be replaced by a struct and data handlers {dlb}:

/*
 **************************************************
 *                                                *
 *             BEGIN PUBLIC FUNCTIONS             *
 *                                                *
 **************************************************
*/

char damage_type(unsigned char wclass, unsigned char wtype)
{
    char type_damage = DVORP_CRUSHING;  // this is the default, btw {dlb}

    if (wclass == OBJ_WEAPONS)
    {
        switch (wtype)
        {
        case WPN_DAGGER:
        case WPN_DEMON_BLADE:
        case WPN_DOUBLE_SWORD:
        case WPN_GREAT_SWORD:
        case WPN_KATANA:
        case WPN_KNIFE:
        case WPN_LONG_SWORD:
        case WPN_QUICK_BLADE:
        case WPN_SABRE:
        case WPN_FALCHION:
        case WPN_SCIMITAR:
        case WPN_SCYTHE:
        case WPN_SHORT_SWORD:
        case WPN_TRIPLE_SWORD:
            type_damage = DVORP_SLICING;
            break;

        case WPN_DEMON_TRIDENT:
        case WPN_EVENINGSTAR:
        case WPN_GIANT_SPIKED_CLUB:
        case WPN_MORNINGSTAR:
        case WPN_SPEAR:
        case WPN_SPIKED_FLAIL:
        case WPN_TRIDENT:
            type_damage = DVORP_PIERCING;
            break;

        case WPN_WAR_AXE:
        case WPN_BATTLEAXE:
        case WPN_BROAD_AXE:
        case WPN_EXECUTIONERS_AXE:
        case WPN_GLAIVE:
        case WPN_HALBERD:
        case WPN_HAND_AXE:
            type_damage = DVORP_CHOPPING;
            break;
        }
    }

    return (type_damage);
}                               // end damage_type()

bool can_cut_meat(unsigned char wclass, unsigned char wtype)
{
    int type = damage_type( wclass, wtype );

    if (type == DVORP_CHOPPING || type == DVORP_SLICING)
        return (true);

    return (false);
}

int hands_reqd_for_weapon(unsigned char wclass, unsigned char wtype)
{
    int reqd_hands = HANDS_ONE_HANDED;

    switch (wclass)
    {
    case OBJ_WEAPONS:
        switch (wtype)
        {
        case WPN_HALBERD:
        case WPN_SCYTHE:
        case WPN_GLAIVE:
        case WPN_QUARTERSTAFF:
        case WPN_BATTLEAXE:
        case WPN_EXECUTIONERS_AXE:
        case WPN_GREAT_SWORD:
        case WPN_TRIPLE_SWORD:
        case WPN_GREAT_MACE:
        case WPN_GREAT_FLAIL:
        case WPN_GIANT_CLUB:
        case WPN_GIANT_SPIKED_CLUB:
            reqd_hands = HANDS_TWO_HANDED;
            break;

        case WPN_SPEAR:
        case WPN_TRIDENT:
        case WPN_DEMON_TRIDENT:
        case WPN_WAR_AXE:
        case WPN_BROAD_AXE:
        case WPN_KATANA:
        case WPN_DOUBLE_SWORD:
            reqd_hands = HANDS_ONE_OR_TWO_HANDED;
            break;
        }
        break;

    case OBJ_STAVES:
        reqd_hands = HANDS_TWO_HANDED;
        break;
    }

    return (reqd_hands);
}                               // end hands_reqd_for_weapon()

bool is_demonic(unsigned char weapon_subtype)
{
    switch (weapon_subtype)
    {
    case WPN_DEMON_BLADE:
    case WPN_DEMON_WHIP:
    case WPN_DEMON_TRIDENT:
        return true;

    default:
        return false;
    }
}                               // end is_demonic()

bool launches_things( unsigned char weapon_subtype )
{
    switch (weapon_subtype)
    {
    case WPN_SLING:
    case WPN_BOW:
    case WPN_CROSSBOW:
    case WPN_HAND_CROSSBOW:
    case WPN_BLOWGUN:
        return (true);

    default:
        return (false);
    }
}                               // end launches_things()

unsigned char launched_by(unsigned char weapon_subtype)
{
    switch (weapon_subtype)
    {
    case WPN_BLOWGUN:
        return MI_NEEDLE;
    case WPN_SLING:
        return MI_STONE;
    case WPN_BOW:
        return MI_ARROW;
    case WPN_CROSSBOW:
        return MI_BOLT;
    case WPN_HAND_CROSSBOW:
        return MI_DART;
    default:
        return MI_EGGPLANT;     // lame debugging code :P {dlb}
    }
}                               // end launched_by()

// this function returns the skill that the weapon would use in melee
char weapon_skill(unsigned char wclass, unsigned char wtype)
{
    char skill2use = SK_FIGHTING;

    if (wclass == OBJ_STAVES
        && (wtype < STAFF_SMITING || wtype >= STAFF_AIR))
    {
        skill2use = SK_STAVES;
    }
    else if (wclass != OBJ_WEAPONS)
        skill2use = SK_FIGHTING;
    else
    {
        switch (wtype)
        {
        case WPN_CLUB:
        case WPN_MACE:
        case WPN_HAMMER:
        case WPN_ANCUS:
        case WPN_WHIP:
        case WPN_FLAIL:
        case WPN_MORNINGSTAR:
        case WPN_GIANT_CLUB:
        case WPN_GIANT_SPIKED_CLUB:
        case WPN_EVENINGSTAR:
        case WPN_DEMON_WHIP:
        case WPN_SPIKED_FLAIL:
        case WPN_GREAT_FLAIL:
        case WPN_GREAT_MACE:
        case WPN_BOW:
        case WPN_BLOWGUN:
        case WPN_CROSSBOW:
        case WPN_HAND_CROSSBOW:
            skill2use = SK_MACES_FLAILS;
            break;

        case WPN_KNIFE:
        case WPN_DAGGER:
        case WPN_SHORT_SWORD:
        case WPN_QUICK_BLADE:
        case WPN_SABRE:
            skill2use = SK_SHORT_BLADES;
            break;

        case WPN_FALCHION:
        case WPN_LONG_SWORD:
        case WPN_SCIMITAR:
        case WPN_KATANA:
        case WPN_DOUBLE_SWORD:
        case WPN_DEMON_BLADE:
        case WPN_GREAT_SWORD:
        case WPN_TRIPLE_SWORD:
            skill2use = SK_LONG_SWORDS;
            break;

        case WPN_HAND_AXE:
        case WPN_WAR_AXE:
        case WPN_BROAD_AXE:
        case WPN_BATTLEAXE:
        case WPN_EXECUTIONERS_AXE:
            skill2use = SK_AXES;
            break;

        case WPN_SPEAR:
        case WPN_HALBERD:
        case WPN_GLAIVE:
        case WPN_SCYTHE:
        case WPN_TRIDENT:
        case WPN_DEMON_TRIDENT:
            skill2use = SK_POLEARMS;
            break;

        case WPN_QUARTERSTAFF:
            skill2use = SK_STAVES;
            break;
        }
    }

    return (skill2use);
}                               // end weapon_skill()
/*
 **************************************************
 *                                                *
 *              END PUBLIC FUNCTIONS              *
 *                                                *
 **************************************************
*/
