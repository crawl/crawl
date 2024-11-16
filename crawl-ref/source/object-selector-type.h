#pragma once

enum object_selector
{
    OSEL_ANY                     =  -1,
    OSEL_WIELD                   =  -2,
    OSEL_UNIDENT                 =  -3,
    OSEL_ENCHANTABLE_ARMOUR      =  -4,
    OSEL_LAUNCHING               =  -5,
    OSEL_EVOKABLE                =  -6,
    OSEL_WORN_ARMOUR             =  -7,
    OSEL_CURSED_WORN             =  -8,
    OSEL_BRANDABLE_WEAPON        =  -9,
    OSEL_ENCHANTABLE_WEAPON      = -10,
    OSEL_BLESSABLE_WEAPON        = -11,
    OSEL_CURSABLE                = -12, // Items that are worn and cursable
    OSEL_UNCURSED_WORN_RINGS     = -13,
    OSEL_QUIVER_ACTION           = -14,
    OSEL_QUIVER_ACTION_FORCE     = -15,
    OSEL_EQUIPABLE               = -16, // armour, jewellery, weapons
    OSEL_WORN_JEWELLERY          = -17,
    OSEL_WORN_EQUIPABLE          = -18,
    OSEL_WEARABLE                = -19,
    OSEL_AMULET                  = -20,
    OSEL_ARTEFACT_WEAPON         = -21,
};
