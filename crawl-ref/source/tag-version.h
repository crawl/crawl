#ifndef TAG_VERSION_H
#define TAG_VERSION_H

// Let CDO updaters know if the syntax changes.
#define TAG_MAJOR_VERSION  32

// Minor version will be reset to zero when major version changes.
enum tag_minor_version
{
    TAG_MINOR_INVALID         = -1,
    TAG_MINOR_RESET           = 0, // Minor tags were reset
    TAG_MINOR_DETECTED_MONSTER,    // Detected monsters keep more than a flag.
    TAG_MINOR_FIRING_POS,          // Store firing position for monsters.
    TAG_MINOR_FOE_MEMORY,          // Save monster's foe_memory.
    TAG_MINOR_SHOPS,               // Store shop_name and shop_type_name in shop_struct.
    TAG_MINOR_MON_TIER_STATS,      // Gather stats about monsters people kill.
    TAG_MINOR_MFLAGS64,            // Extend mon.flags to 64 bits.
    TAG_MINOR_ENCH_MID,            // Store sources of monster enchantments.
    TAG_MINOR_CLOUD_BUG,           // Shim to recover bugged saves.
    TAG_MINOR_MINFO_PROP,          // Add a props hash to monster_info.
    TAG_MINOR_MON_INV_ORDER,       // Change the order of the monster's inventory.
    TAG_MINOR_ASH_PENANCE,         // Ashenzari's wrath counter.

    NUM_TAG_MINORS,
    TAG_MINOR_VERSION = NUM_TAG_MINORS - 1
};

#endif
