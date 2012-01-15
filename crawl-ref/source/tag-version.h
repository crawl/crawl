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
    TAG_MINOR_SPECIES_HP_NO_MUT,   // Species HP modifier is independent from frail/robust.
    TAG_MINOR_GOLDIFIED_RUNES,     // Runes are undroppable and don't take space.
    TAG_MINOR_ZIG_COUNT,           // Count completed Ziggurats and max level of partials.
    TAG_MINOR_ZIG_FIX,             // Zero out junk ziggurat variable values.
    TAG_MINOR_XP_POOL_FIX,         // Clamp possibly junk values of XP pool.
    TAG_MINOR_DECK_RARITY,         // Make item.special=0 not clash with real rarities.
    TAG_MINOR_PIETY_MAX,           // Remember the maximum piety attained with each god.
    TAG_MINOR_CHR_NAMES,           // Store species and god names as strings.
    TAG_MINOR_SKILL_TRAINING,      // New skill training system.
    TAG_MINOR_FOCUS_SKILL,         // Add focus skills to skill training system.
    TAG_MINOR_MANUAL,              // New manuals.
    TAG_MINOR_MONSTER_TILES,       // Throw away monster tiles when their number changes.
    TAG_MINOR_SKILL_MENU_STATES,   // Move the saved states out of props.
    TAG_MINOR_MONS_THREAT_LEVEL,   // Save threat level in monster_info.
    TAG_MINOR_SPELL_USAGE,         // Spell usage counts in char dumps.
    TAG_MINOR_UNIQUE_NOTES,        // Automatic annotations for uniques.
    TAG_MINOR_NEW_MIMICS,          // Wider map mask and new mimics.
    TAG_MINOR_POLEARMS_REACH,      // Purge reaching from polearms (built-in now).
    TAG_MINOR_ABYSS_STATE,         // Save the Worley noise state for the Abyss.
    TAG_MINOR_ABYSS_SPEED,         // Save the abyss speed which is variable.
    TAG_MINOR_UNPONDERIFY,         // Give Cheibriadites back their old gear.
    TAG_MINOR_SKILL_RESTRICTIONS,  // Add restrictions to which skills can be trained.
    TAG_MINOR_NUM_LEVEL_CONN,      // Fix level connectivity data hard-coding NUM_BRANCHES.
    TAG_MINOR_FOOD_MUTATIONS,      // Food reform racial mutation upgrades.
    TAG_MINOR_DISABLED_SKILLS,     // Track practise events of disabled skills.
    TAG_MINOR_CHERUB_ATTACKS,      // Give Cherubs a new attack flavour.
    TAG_MINOR_FOOD_MUTATIONS_BACK, // Revert the food experiment mutations.
    TAG_MINOR_TEMPORARY_CLOUDS,    // Clouds are now marked if they're temporary or not.
                                   // XXX: When clearing, check dat/dlua/lm_fog.lua
    TAG_MINOR_PHOENIX_ATTITUDE,    // Store the attitudes of dead phoenixes
    TAG_MINOR_LESS_TILE_DATA,      // mcache and tile_bk is not saved, but more stuff in map_knowledge
    TAG_MINOR_CONSTRICTION,        // Constriction available
    TAG_MINOR_ACTION_COUNTS,       // Count actions made by type.
    TAG_MINOR_ABYSS_PHASE,         // Jerky abyss.
    TAG_MINOR_SKILL_MODE_STATE,    // Auto and manual modes have separate skill activation status.
    TAG_MINOR_OCTO_RING,           // Ring of the Octopus King.
    TAG_MINOR_ABILITY_COUNTS,      // Count ability usage better.
    NUM_TAG_MINORS,
    TAG_MINOR_VERSION = NUM_TAG_MINORS - 1
};

#endif
