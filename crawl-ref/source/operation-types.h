#pragma once

enum operation_types
{
    OPER_WIELD    = 'w',
    OPER_QUAFF    = 'q',
    OPER_DROP     = 'd',
    OPER_TAKEOFF  = 'T',
    OPER_WEAR     = 'W',
    OPER_PUTON    = 'P',
    OPER_REMOVE   = 'R',
    OPER_READ     = 'r',
    OPER_MEMORISE = 'M',
    OPER_ZAP      = 'Z',
    OPER_FIRE     = 'f',
    OPER_EVOKE    = 'v',
    OPER_QUIVER   = 'Q',
    OPER_ATTACK   = 'a',
    OPER_EQUIP    = 'e',
    OPER_UNEQUIP  = 'u', // note: does not match current binding
    OPER_ANY      = 0,
    OPER_NONE     = -1, // could this be 0?
};
