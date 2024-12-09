#pragma once

#include "tag-version.h"

enum item_status_flag_type  // per item flags: ie. ident status, cursed status
{
    ISFLAG_IDENTIFIED        = 0x00000001,  // Item is fully identified
                             //0x00000002,  // was: ISFLAG_KNOW_TYPE
                             //0x00000004,  // was: ISFLAG_KNOW_PLUSES
                             //0x00000008,  // was: ISFLAG_KNOW_PROPERTIES
                             //0x0000000F,  // was: ISFLAG_IDENT_MASK

    ISFLAG_CURSED            = 0x00000100,  // cursed
    ISFLAG_HANDLED           = 0x00000200,  // player has handled this item
                             //0x00000400,  // was: ISFLAG_SEEN_CURSED
                             //0x00000800,  // was: ISFLAG_TRIED

    ISFLAG_RANDART           = 0x00001000,  // special value is seed
    ISFLAG_UNRANDART         = 0x00002000,  // is an unrandart
    ISFLAG_ARTEFACT_MASK     = 0x00003000,  // randart or unrandart
    ISFLAG_DROPPED           = 0x00004000,  // dropped item (no autopickup)
    ISFLAG_THROWN            = 0x00008000,  // thrown missile weapon

    // these don't have to remain as flags
    ISFLAG_NO_DESC           = 0x00000000,  // used for clearing these flags
    ISFLAG_GLOWING           = 0x00010000,  // weapons or armour
    ISFLAG_RUNED             = 0x00020000,  // weapons or armour
    ISFLAG_EMBROIDERED_SHINY = 0x00040000,  // armour: depends on sub-type
    ISFLAG_COSMETIC_MASK     = 0x00070000,  // mask of cosmetic descriptions

    ISFLAG_UNOBTAINABLE      = 0x00080000,  // vault on display

    ISFLAG_MIMIC             = 0x00100000,  // mimic

    ISFLAG_CHAOTIC           = 0x00200000,  // is a chaotic artefact (Xom-only)

    ISFLAG_NO_PICKUP         = 0x00400000,  // Monsters won't pick this up

#if TAG_MAJOR_VERSION == 34
    ISFLAG_UNUSED1           = 0x01000000,  // was ISFLAG_ORCISH
    ISFLAG_UNUSED2           = 0x02000000,  // was ISFLAG_DWARVEN
    ISFLAG_UNUSED3           = 0x04000000,  // was ISFLAG_ELVEN
    ISFLAG_RACIAL_MASK       = 0x07000000,  // mask of racial equipment types
#endif
    ISFLAG_NOTED_ID          = 0x08000000,
    ISFLAG_NOTED_GET         = 0x10000000,

    ISFLAG_SEEN              = 0x20000000,  // has it been seen
    ISFLAG_SUMMONED          = 0x40000000,  // Item generated on a summon

    ISFLAG_REPLICA           = 0x80000000,  // Cosmetic descriptor for Paragon items

};
