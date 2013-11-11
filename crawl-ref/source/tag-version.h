#ifndef TAG_VERSION_H
#define TAG_VERSION_H

// Character info has its own top-level tag, mismatching majors don't break
// compatibility there.
// DO NOT BUMP THIS UNLESS YOU KNOW WHAT YOU'RE DOING. This would break
// the save browser across versions, possibly leading to overwritten games.
// It's only there in case there's no way out.
#define TAG_CHR_FORMAT 0

// Let CDO updaters know if the syntax changes.
#ifndef TAG_MAJOR_VERSION
#define TAG_MAJOR_VERSION 34
#endif

// Minor version will be reset to zero when major version changes.
enum tag_minor_version
{
    TAG_MINOR_INVALID         = -1,
    TAG_MINOR_RESET           = 0, // Minor tags were reset
#if TAG_MAJOR_VERSION == 34
    TAG_MINOR_BRANCHES_LEFT,       // Note the first time branches are left
    TAG_MINOR_VAULT_LIST,          // Don't try to store you.vault_list as prop
    TAG_MINOR_TRAPS_DETERM,        // Searching for traps is deterministic.
    TAG_MINOR_ACTION_THROW,        // Store base type of throw objects.
    TAG_MINOR_TEMP_MUTATIONS,      // Enable transient mutations
    TAG_MINOR_AUTOINSCRIPTIONS,    // Artefact inscriptions are added on the fly
    TAG_MINOR_UNCANCELLABLES,      // Restart uncancellable questions upon save load
    TAG_MINOR_DEEP_ABYSS,          // Multi-level abyss
    TAG_MINOR_COORD_SERIALIZER,    // Serialize coord_def as int
    TAG_MINOR_REMOVE_ABYSS_SEED,   // Remove the abyss seed.
    TAG_MINOR_REIFY_SUBVAULTS,     // Save subvaults with level for attribution
    TAG_MINOR_VEHUMET_SPELL_GIFT,  // Vehumet gift spells instead of books
    TAG_MINOR_0_11 = 17,           // 0.11 final saves
    TAG_MINOR_0_12,                // (no change)
    TAG_MINOR_BATTLESPHERE_MID,    // Monster battlesphere (mid of creator)
    TAG_MINOR_MALMUTATE,           // Convert Polymorph to Malmutate on old monsters
    TAG_MINOR_VEHUMET_MULTI_GIFTS, // Vehumet can offer multiple spells at once
    TAG_MINOR_ADD_ABYSS_SEED,      // Reinstate abyss seed. Mistakes were made.
    TAG_MINOR_COMPANION_LIST,      // Added companion list
    TAG_MINOR_INCREMENTAL_RECALL,  // Made recall incremental
    TAG_MINOR_GOD_GIFT,            // Remove {god gift} from inscriptions.
    TAG_MINOR_NOME_NO_MORE,        // Remove unused gnome variable.
    TAG_MINOR_NO_SPLINT,           // Remove splint mail
    TAG_MINOR_ORIG_MONNUM,         // orig_monnum is type rather than type+1.
    TAG_MINOR_SPRINT_SCORES,       // Separate score lists for each sprint map
    TAG_MINOR_FOOD_AUTOPICKUP,     // Separate meat, fruit, others in \ menu.
    TAG_MINOR_LORC_TEMPERATURE,    // Save lava orc temperature
    TAG_MINOR_GARGOYLE_DR,         // Gargoyle damage reduction
    TAG_MINOR_TRAVEL_ALLY_PACE,    // Pace travel to slowest ally setting
    TAG_MINOR_AUTOMATIC_MANUALS,   // Manuals are now always studied
    TAG_MINOR_RM_GARGOYLE_DR,      // Gargoyle DR is redundant.
    TAG_MINOR_STAT_ZERO,           // Stat zero doesn't cause death.
    TAG_MINOR_BOX_OF_BEASTS_CHARGES, // Box of Beasts counts its charges.
    TAG_MINOR_WAR_DOG_REMOVAL,     // War dogs become wolves, then disappear
    TAG_MINOR_CANARIES,            // Canaries in save files.
    TAG_MINOR_CHIMERA_GHOST_DEMON, // Use ghost demon
    TAG_MINOR_MONSTER_PARTS,       // Flag the presence of ghost_demon (and more)
    TAG_MINOR_OPTIONAL_PARTS,      // Make three big monster structs optional.
    TAG_MINOR_SHORT_SPELL_TYPE,    // Spell types changed to short instead of byte
    TAG_MINOR_FORGOTTEN_MAP,       // X^F can be undone.
    TAG_MINOR_CONTAM_SCALE,        // Scale the magic contamination by a factor of 1000
    TAG_MINOR_SUMMONER,            // Store summoner data.
    TAG_MINOR_STAT_MUT,            // Flag for converting stat mutations
    TAG_MINOR_MAP_ORDER,           // map_def::order added to des cache
    TAG_MINOR_FIXED_CONSTRICTION,  // Corrected a constricting marshalling bug.
    TAG_MINOR_SEEDS,               // Per-game seeds for deterministic stuff.
    TAG_MINOR_ABYSS_BRANCHES,      // Spawn abyss monsters from other branches.
    TAG_MINOR_BRANCH_ENTRY,        // Store branch entry point (rather than just depth).
    TAG_MINOR_16_BIT_TABLE,        // Increase the limit for CrawlVector/HashTable to 65535.
#endif
    NUM_TAG_MINORS,
    TAG_MINOR_VERSION = NUM_TAG_MINORS - 1
};

#endif
