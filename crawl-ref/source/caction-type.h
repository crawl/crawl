#pragma once

enum caction_type    // Primary categorization of counted actions.
{                    // A subtype and auxtype will also be given in each case:
    CACT_MELEE,      // weapon subtype or unrand index
                     //   subtype = -1 for unarmed or aux attacks
                     //   auxtype = -1 for unarmed
                     //   auxtype = unarmed_attack_type for aux attacks
    CACT_FIRE,       // weapon subtype or unrand index
    CACT_THROW,      // auxtype = item basetype, subtype = item subtype
    CACT_CAST,       // spell_type
    CACT_INVOKE,     // ability_type
    CACT_ABIL,       // ability_type
    CACT_EVOKE,      // evoc_type or unrand index
                     //   auxtype = item basetype, subtype = item subtype
    CACT_USE,        // object_class_type
    CACT_STAB,       // stab_type
#if TAG_MAJOR_VERSION == 34
    CACT_EAT,        // food_type, or subtype = -1 for corpse
#endif
    CACT_ARMOUR,     // armour subtype or subtype = -1 for unarmoured
    CACT_DODGE,      // dodge_type
    CACT_BLOCK,      // armour subtype or subtype = -1 and
                     //   auxtype used for special cases
                     //   (reflection, god ability, spell, etc)
    CACT_RIPOSTE,    // as CACT_MELEE
    NUM_CACTIONS,
};
