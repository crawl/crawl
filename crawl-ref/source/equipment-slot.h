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

    END_OF_SLOTS,

    // Range check on 'standard' equipment slots
    SLOT_FIRST_STANDARD = SLOT_WEAPON,
    SLOT_LAST_STANDARD = SLOT_GIZMO,

    // Range check on normal 'armour' slots
    SLOT_MIN_ARMOUR = SLOT_OFFHAND,
    SLOT_MAX_ARMOUR = SLOT_CLOAK,
};
