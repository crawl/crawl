#ifndef TAG_VERSION_H
#define TAG_VERSION_H

// Character info has its own top-level tag, mismatching majors don't break
// compatibility there.
#define TAG_CHR_FORMAT 0

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
    TAG_MINOR_BOOK_BEASTS,         // Addition of the book of zoology^Wbeasts.
    TAG_MINOR_CHR_COMPAT,          // Future-compatible chr data.
    TAG_MINOR_ZOT_POINTS,          // Untie ZotDef power points from the xp pool.
    TAG_MINOR_TRAP_KNOWLEDGE,      // Save trap type in map_knowledge.
    TAG_MINOR_NEW_HP,              // New HP formula.
    TAG_MINOR_HP_MP_CALC,          // HP and MP recalculated rather than stored +5000.
    TAG_MINOR_64_MB,               // More than 64 monster info flags.
    TAG_MINOR_SEEN_MISC,           // Record misc items seen.
    TAG_MINOR_SPECIES_HP_NO_MUT,   // Species HP modifier is independant from frail/robust.
    TAG_MINOR_GOLDIFIED_RUNES,     // Runes are undroppable and don't take space.
    TAG_MINOR_ZIG_COUNT,           // Count completed Ziggurats and max level of partials.
    TAG_MINOR_ZIG_FIX,             // Zero out junk ziggurat variable values.
    TAG_MINOR_XP_POOL_FIX,         // Clamp possibly junk values of XP pool.
    TAG_MINOR_DECK_RARITY,         // Make item.special=0 not clash with real rarities.
    TAG_MINOR_PIETY_MAX,           // Remember the maximum piety attained with each god.
    TAG_MINOR_CHR_NAMES,           // Store species and god names as strings.
    TAG_MINOR_SKILL_TRAINING,      // New skill training system.
    TAG_MINOR_FOCUS_SKILL,         // Add focus skills to skill training system.

    NUM_TAG_MINORS,
    TAG_MINOR_VERSION = NUM_TAG_MINORS - 1
};

#endif
