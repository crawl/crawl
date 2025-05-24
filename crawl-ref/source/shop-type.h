#pragma once

#include "tag-version.h"

enum shop_type
{
    SHOP_WEAPON,
    SHOP_ARMOUR,
    SHOP_WEAPON_ANTIQUE,
    SHOP_ARMOUR_ANTIQUE,
    SHOP_GENERAL_ANTIQUE,
    SHOP_JEWELLERY,
    SHOP_GADGET, // wands, evokers, and other miscellany
    SHOP_BOOK,
#if TAG_MAJOR_VERSION == 34
    SHOP_FOOD,
#endif
    SHOP_DISTILLERY,
    SHOP_SCROLL,
    SHOP_GENERAL,
    NUM_SHOPS, // must remain last 'regular' member {dlb}
    SHOP_UNASSIGNED = 100,
    SHOP_RANDOM,
};
