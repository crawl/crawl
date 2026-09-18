#pragma once

enum equipment_slot
{

    SLOT_UNUSED,

    SLOT_WEAPON,
    SLOT_OFFHAND,
    SLOT_BODY_ARMOUR,
    SLOT_HELMET,
    SLOT_GLOVES,
    SLOT_BOOTS,
    SLOT_BARDING,
    SLOT_CLOAK,
    SLOT_RING,
    SLOT_AMULET,
    SLOT_GIZMO,

    // Flexible slots.
    SLOT_WEAPON_OR_OFFHAND,     // Coglins
    SLOT_HAUNTED_AUX,           // Poltergeist

    SLOT_TWOHANDER_ONLY,        // Fortress Crab's slot for a two-hander (but
                                // not a shield)

    // End of the 'real' equip slots.
    NUM_EQUIP_SLOTS,

    // These are 'fake' slots that exist as convenient shorthand to group
    // actual slots together.

    SLOT_LOWER_BODY,            // Boots *or* Bardings
    SLOT_ALL_ARMOUR,            // Body armour + all aux slots
    SLOT_ALL_AUX_ARMOUR,        // Aux armour only
    SLOT_ALL_JEWELLERY,         // Ring + amulet slot
    SLOT_ALL_EQUIPMENT,         // All equipped items

    SLOT_WEAPON_STRICT,         // Strictly matches SLOT_WEAPON and not also
                                // SLOT_WEAPON_OR_OFFHAND (to handle Coglin
                                // 2-hander interactions properly)

    SLOT_TWOHANDER_OFFHAND,     // A slot that can hold the second half of a
                                // two-hander (ie: shield slot, Coglin second
                                // weapon slot, or fortress crab.)

    END_OF_SLOTS,

    // Range check on 'standard' equipment slots
    SLOT_FIRST_STANDARD = SLOT_WEAPON,
    SLOT_LAST_STANDARD = SLOT_GIZMO,

    // Range check on normal 'armour' slots
    SLOT_MIN_ARMOUR = SLOT_OFFHAND,
    SLOT_MAX_ARMOUR = SLOT_CLOAK,

    // Range check on 'aux' armour
    SLOT_MIN_AUX_ARMOUR = SLOT_HELMET,
    SLOT_MAX_AUX_ARMOUR = SLOT_CLOAK,
};
