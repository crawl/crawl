/*
 *********************************************************************
 *  File:       wpn-misc.cc                                          *
 *  Summary:    temporary home for weapon f(x) until struct'ed       *
 *  Written by: don brodale <dbrodale@bigfootinteractive.com>        *
 *                                                                   *
 *  Modified for Crawl Reference by $Author$ on $Date$               *
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

int hands_reqd_for_weapon(int wclass, int wtype)
{
    int reqd_hands = HANDS_ONE;

    switch (wclass)
    {
    case OBJ_WEAPONS:
        switch (wtype)
        {
        case WPN_HALBERD:
        case WPN_SCYTHE:
        case WPN_GLAIVE:
        case WPN_QUARTERSTAFF:
        case WPN_LAJATANG:
        case WPN_BATTLEAXE:
        case WPN_EXECUTIONERS_AXE:
        case WPN_GREAT_SWORD:
        case WPN_TRIPLE_SWORD:
        case WPN_GREAT_MACE:
        case WPN_DIRE_FLAIL:
        case WPN_GIANT_CLUB:
        case WPN_GIANT_SPIKED_CLUB:
        case WPN_LOCHABER_AXE:
        case WPN_BOW:
        case WPN_LONGBOW:
        case WPN_CROSSBOW:
            reqd_hands = HANDS_TWO;
            break;

        case WPN_SPEAR:
        case WPN_TRIDENT:
        case WPN_DEMON_TRIDENT:
        case WPN_WAR_AXE:
        case WPN_BROAD_AXE:
        case WPN_KATANA:
        case WPN_DOUBLE_SWORD:
        case WPN_HAND_CROSSBOW:
        case WPN_BLOWGUN:
        case WPN_SLING:
            reqd_hands = HANDS_HALF;
            break;
        }
        break;

    case OBJ_STAVES:
        reqd_hands = HANDS_TWO;
        break;
    }

    return (reqd_hands);
}                               // end hands_reqd_for_weapon()


bool launches_things( unsigned char weapon_subtype )
{
    switch (weapon_subtype)
    {
    case WPN_SLING:
    case WPN_BOW:
    case WPN_LONGBOW:
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
    case WPN_LONGBOW:
        return MI_ARROW;
    case WPN_CROSSBOW:
        return MI_BOLT;
    case WPN_HAND_CROSSBOW:
        return MI_DART;
    default:
        return MI_NONE;     // lame debugging code :P {dlb}
    }
}                               // end launched_by()
