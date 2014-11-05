#ifndef TAG_VERSION_H
#define TAG_VERSION_H

// Character info has its own top-level tag, mismatching majors don't break
// compatibility there.
// DO NOT BUMP THIS UNLESS YOU KNOW WHAT YOU'RE DOING. This would break
// the save browser across versions, possibly leading to overwritten games.
// It's only there in case there's no way out.
#define TAG_CHR_FORMAT 0

// Let CDO updaters know if the syntax changes.
// Really, really, REALLY _never_ ever bump this and clean up old #ifdefs
// in a single commit, please.  Making clean-up and actual code changes,
// especially of this size, separated is vital for sanity.
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
    TAG_MINOR_ABIL_1000,           // Start god ability enums at 1000.
    TAG_MINOR_CLASS_HP_0,          // Base class maxhp at 0.
    TAG_MINOR_NOISES,              // Save incompat recovery wrt ATTR_NOISES.
    TAG_MINOR_ABIL_GOD_FIXUP,      // Movement of some non-god-specific abils.
    TAG_MINOR_NEMELEX_DUNGEONS,    // Make nemelex not give/track decks of dungeons.
    TAG_MINOR_DEMONSPAWN,          // Save compat wrt demonspawn enemies.
    TAG_MINOR_EVENT_TIMERS,        // "Every 20 turn" effects are less determinstic.
    TAG_MINOR_EVENT_TIMER_FIX,     // Correct event timers in transferred games
    TAG_MINOR_MONINFO_ENERGY,      // Energy usage in monster_info
    TAG_MINOR_BOOK_ID,             // Track spellbooks you've identified
    TAG_MINOR_MISC_SHOP_CHANGE,    // Wand and gadget shops merged
    TAG_MINOR_HORN_GERYON_CHANGE,  // Horn of geryon changed to an xp-evoker
    TAG_MINOR_NEMELEX_WEIGHTS,     // Nemelex deck weighting removed
    TAG_MINOR_UNSEEN_MONSTER,      // Invis indicators for monsters going from seen to unseen
    TAG_MINOR_MR_ITEM_RESCALE,     // Rescaled MR property on items.
    TAG_MINOR_MANGROVES,           // Turn all mangroves into trees.
    TAG_MINOR_FIX_FEAT_SHIFT,      // Fix feature shifts from the last tag.
    TAG_MINOR_FUNGUS_FORM,         // Removed confusing touch duration from fungus form.
    TAG_MINOR_STEALTH_RESCALE,     // Item properties: rescaled Stealth, removed Hunger.
    TAG_MINOR_ATTACK_DESCS,        // Added attacks to monster_info.
    TAG_MINOR_BRIBE_BRANCH,        // Bribe branch tracking
    TAG_MINOR_CLOUD_OWNER,         // Track owners of clouds in map knowledge
    TAG_MINOR_NO_DEVICE_HEAL,      // Made MUT_NO_DEVICE_HEAL a normal bad mutation.
    TAG_MINOR_DIET_MUT,            // Remove carnivore/herbivore muts from random generation.
    TAG_MINOR_SAGE_REMOVAL,        // Removed the Sage card and status.
    TAG_MINOR_CALC_UNRAND_REACTS,  // Compute you.unrand_reacts on load
    TAG_MINOR_SAPROVOROUS,         // Remove Saprovorous from several species
    TAG_MINOR_CE_HA_DIET,          // Remove intrinsic diet muts from Ce & Ha
    TAG_MINOR_NO_POT_FOOD,         // Remove Royal Jellies & Ambrosia
    TAG_MINOR_ROT_IMMUNITY,        // Make rot immunity an intrinsic mutation.
    TAG_MINOR_FOUL_STENCH,         // Remove Saprovore from the Foul Stench DS Facet
    TAG_MINOR_FOOD_PURGE,          // Cleaning up old types of food.
    TAG_MINOR_FOOD_PURGE_AP_FIX,   // Correctly carry over old fruit autopickup.
    TAG_MINOR_WEIGHTLESS,          // Removal of player burden.
    TAG_MINOR_DS_CLOUD_MUTATIONS,  // Change Ds conservation muts to cloud immunities.
    TAG_MINOR_FRIENDLY_PICKUP,     // Remove the friendly_pickup setting.
    TAG_MINOR_STICKY_FLAME,        // Change the name of you.props "napalmer" & "napalm_aux"
    TAG_MINOR_SLAYRING_PLUSES,     // Combine Acc/Dam on rings of slaying and artefacts.
    TAG_MINOR_MERGE_EW,            // Combine enchant weapons scrolls.
    TAG_MINOR_WEAPON_PLUSES,       // Combine to-hit/to-dam enchantment on weapons.
    TAG_MINOR_SAVE_TERRAIN_COLOUR, // Save colour in terrain-change markers.
    TAG_MINOR_REMOVE_BASE_MP,      // Remove base MP bonus.
    TAG_MINOR_METABOLISM,          // Remove random fast/slow meta mutations
    TAG_MINOR_RU_SACRIFICES,       // Store Ru sacrifices in an array for coloration
    TAG_MINOR_IS_UNDEAD,           // Remove the old "is_undead" player field
    TAG_MINOR_REMOVE_MON_AC_EV,    // Remove the old "ac" & "ev" monster fields
    TAG_MINOR_DISPLAY_MON_AC_EV,   // Marshall & unmarshall ac/ev in monster_info
    TAG_MINOR_PLACE_UNPACK,        // Some packed places are level_ids.
    TAG_MINOR_NO_JUMP,             // Removal of jump-attack.
    TAG_MINOR_MONSTER_SPELL_SLOTS, // Introduce monster spell slot flags/freqs
    TAG_MINOR_ARB_SPELL_SLOTS,     // Arbitrary number of monster spell slots.
    TAG_MINOR_CUT_CUTLASSES,       // Turn crummy cutlasses into real rapiers.
    TAG_MINOR_NO_GHOST_SPELLCASTER,// Remove an unused field in ghost_demon
    TAG_MINOR_MID_BEHOLDERS,       // you.beholders and fearmongers store mids
    TAG_MINOR_REMOVE_ITEM_COLOUR,  // don't store item colour as state
    TAG_MINOR_CORPSE_CRASH,        // don't crash when loading corpses
    TAG_MINOR_INIT_RND,            // initialize rnd in more places
    TAG_MINOR_RING_PLUSSES,        // don't generate +144 rings
    TAG_MINOR_BLESSED_WPNS,        // Remove blessed long blade base types
    TAG_MINOR_MON_COLOUR_LOOKUP,   // monster colour lookup when possible
    TAG_MINOR_CONSUM_APPEARANCE,   // Stop storing item appearance in .plus
    TAG_MINOR_NEG_IDESC,           // Fix a sign conversion error
    TAG_MINOR_GHOST_ENERGY,        // ghost_demon has move_energy field
#endif
    NUM_TAG_MINORS,
    TAG_MINOR_VERSION = NUM_TAG_MINORS - 1
};

#endif
