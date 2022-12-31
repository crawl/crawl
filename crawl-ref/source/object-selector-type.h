#pragma once

enum object_selector
{
    OSEL_ANY                     =  -1,
    OSEL_WIELD                   =  -2,
    OSEL_UNIDENT                 =  -3,
#if TAG_MAJOR_VERSION == 34
    OSEL_RECHARGE                =  -4,
#endif
    OSEL_ENCHANTABLE_ARMOUR      =  -5,
    OSEL_BEOGH_GIFT              =  -6,
#if TAG_MAJOR_VERSION == 34
    OSEL_DRAW_DECK               =  -7,
#endif
    OSEL_LAUNCHING               =  -8,
    OSEL_EVOKABLE                =  -9,
    OSEL_WORN_ARMOUR             = -10,
    OSEL_CURSED_WORN             = -11,
#if TAG_MAJOR_VERSION == 34
    OSEL_UNCURSED_WORN_ARMOUR    = -12,
    OSEL_UNCURSED_WORN_JEWELLERY = -13,
#endif
    OSEL_BRANDABLE_WEAPON        = -14,
    OSEL_ENCHANTABLE_WEAPON      = -15,
    OSEL_BLESSABLE_WEAPON        = -16,
    OSEL_CURSABLE                = -17, // Items that are worn and cursable
#if TAG_MAJOR_VERSION == 34
    OSEL_DIVINE_RECHARGE         = -18,
#endif
    OSEL_UNCURSED_WORN_RINGS     = -19,
    OSEL_QUIVER_ACTION           = -20,
    OSEL_QUIVER_ACTION_FORCE     = -21,
    OSEL_EQUIPABLE               = -22, // armour, jewellery, weapons
    OSEL_WORN_JEWELLERY          = -23,
    OSEL_WORN_EQUIPABLE          = -24,
};
