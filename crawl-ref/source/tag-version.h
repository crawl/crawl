#pragma once

#include <set>

// Character info has its own top-level tag, mismatching majors don't break
// compatibility there.
// DO NOT BUMP THIS UNLESS YOU KNOW WHAT YOU'RE DOING. This would break
// the save browser across versions, possibly leading to overwritten games.
// It's only there in case there's no way out.
#define TAG_CHR_FORMAT 0
COMPILE_CHECK(TAG_CHR_FORMAT < 256);

// Let CDO updaters know if the syntax changes.
// Really, really, REALLY _never_ ever bump this and clean up old #ifdefs
// in a single commit, please. Making clean-up and actual code changes,
// especially of this size, separated is vital for sanity.
#ifndef TAG_MAJOR_VERSION
#define TAG_MAJOR_VERSION 34
#endif
COMPILE_CHECK(TAG_MAJOR_VERSION < 256);

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
    TAG_MINOR_LUA_DUMMY_0,         // dummy to unbreak util/gen-luatags.pl
    TAG_MINOR_LUA_DUMMY_1,         // dummy to unbreak util/gen-luatags.pl
    TAG_MINOR_LUA_DUMMY_2,         // dummy to unbreak util/gen-luatags.pl
    TAG_MINOR_LUA_DUMMY_3,         // dummy to unbreak util/gen-luatags.pl
    TAG_MINOR_0_11,                 // 0.11 final saves
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
    TAG_MINOR_NO_POTION_HEAL,      // Made MUT_NO_POTION_HEAL a normal bad mutation.
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
    TAG_MINOR_TENTACLE_MID,        // Use mids for tentacle code
    TAG_MINOR_CORPSE_COLOUR,       // Fix corpses with invalid colour.
    TAG_MINOR_MANGLE_CORPSES,      // Turn NEVER_HIDE corpses into MANGLED_CORPSEs
    TAG_MINOR_ZOT_OPEN,            // Don't store whether you opened Zot
    TAG_MINOR_EXPLORE_MODE,        // Store whether you are in explore mode
    TAG_MINOR_RANDLICHES,          // Liches are now GHOST_DEMONs
    TAG_MINOR_ISFLAG_HANDLED,      // Game tracks which items player has handled
    TAG_MINOR_SHOP_HACK,           // The shop hack is dead!
    TAG_MINOR_STACKABLE_EVOKERS,   // XP evokers stack
    TAG_MINOR_REALLY_16_BIT_VEC,   // CrawlVector size really saved as 16-bit
    TAG_MINOR_FIX_8_BIT_VEC_MAX,   // Fix up bad CrawlVector max size
    TAG_MINOR_TRACK_BANISHER,      // Persist the name of the last thing that banished the player
    TAG_MINOR_SHOALS_LITE,         // Remove deep water from old Shoals
    TAG_MINOR_FIX_EXPLORE_MODE,    // Fix char selection screen on old versions
    TAG_MINOR_UNSTACKABLE_EVOKERS, // XP evokers no longer stack
    TAG_MINOR_NO_NEGATIVE_VULN,    // Remove negative energy vulnerability
    TAG_MINOR_MAX_XL,              // Store max XL instead of hardcoding it
    TAG_MINOR_NO_RPOIS_MINUS,      // Remove rPois- artefacts
    TAG_MINOR_XP_PENANCE,          // Let gods other than Ash use xp penance
    TAG_MINOR_SPIT_POISON,         // Give Nagas MUT_SPIT_POISON
    TAG_MINOR_REAL_MUTS,           // Give some species proper mutations
    TAG_MINOR_NO_FORLORN,          // Remove Forlorn mutation
    TAG_MINOR_MP_WANDS,            // Make MP wands a single-level mutation
    TAG_MINOR_TELEPORTITIS,        // Rescale teleportitis on artefacts
    TAG_MINOR_ROTTING,             // Remove rot-over-time
    TAG_MINOR_STAT_ZERO_DURATION,  // Stat zero uses a duration
    TAG_MINOR_INT_REGEN,           // hp/mp regen are ints
    TAG_MINOR_NAGA_METABOLISM,     // nagas have slow metabolism
    TAG_MINOR_BOOL_FLIGHT,         // flight is just a bool
    TAG_MINOR_STAT_LOSS_XP,        // Stat loss recovers with XP
    TAG_MINOR_DETERIORATION,       // 2-level deterioration mutation
    TAG_MINOR_RU_DELAY_STACKING,   // Let Ru delay timers stack again
    TAG_MINOR_NO_TWISTER,          // Remove ARTP_TWISTER
    TAG_MINOR_NO_ZOTDEF,           // remove zotdef, along with zot_points and zotdef_wave_name
    TAG_MINOR_SAVED_PIETY,         // allowed good-god piety to survive through an atheist period
    TAG_MINOR_GHOST_SINV,          // marshall ghost_demon sinv
    TAG_MINOR_ID_STATES,           // turn item_type_id_state_type into a bool
    TAG_MINOR_MON_HD_INFO,         // store player-known monster HD info
    TAG_MINOR_NO_LEVEL_FLAGS,      // remove a field of env
    TAG_MINOR_EXORCISE,            // liches, a. liches, & spellforged servitors are no longer ghost_demons
    TAG_MINOR_BLINK_MUT,           // 1-level blink mutation
    TAG_MINOR_RUNE_TYPE,           // runes became a base type
    TAG_MINOR_ZIGFIGS,             // let characters from before ziggurat changes continue zigging
    TAG_MINOR_RU_PIETY_CONSISTENCY,// make Ru piety constant once determined.
    TAG_MINOR_SAC_PIETY_LEN,       // marshall length with sacrifice piety
    TAG_MINOR_MULTI_HOLI,          // Turn monster holiness into a bitfield.
    TAG_MINOR_SHOPINFO,            // ShopInfo has a real shop
    TAG_MINOR_UNSHOPINFO,          // Fixup after revert of previous
    TAG_MINOR_UNUNSHOPINFO,        // Restoration of the tag two before
    TAG_MINOR_MESSAGE_REPEATS,     // Rewrite the way message repeats work
    TAG_MINOR_GHOST_NOSINV,        // don't marshall ghost_demon sinv
    TAG_MINOR_NO_DRACO_TYPE,       // don't marshall mon-info draco_type
    TAG_MINOR_DEMONIC_SPELLS,      // merge demonic spells into magical spells
    TAG_MINOR_MUMMY_RESTORATION,   // remove mummy self-restoration ability
    TAG_MINOR_DECUSTOM_CLOUDS,     // remove support for custom clouds
    TAG_MINOR_PAKELLAS_WRATH,      // fix Pakellas passive wrath not expiring
    TAG_MINOR_GLOBAL_BR_INFO,      // move global branch info to a reserved location
    TAG_MINOR_SPIT_POISON_AGAIN,   // Make Naga poison spit a 2-level mutation.
    TAG_MINOR_HIDE_TO_SCALE,       // Rename dragon hides to scales.
    TAG_MINOR_NO_PRIORITY,         // Remove CHANCE priority in map definitions.
    TAG_MINOR_MOTTLED_REMOVAL,     // Mottled dracos get breathe fire
    TAG_MINOR_NEMELEX_WRATH,       // Nemelex loses the passive wrath component
    TAG_MINOR_SLIME_WALL_CLEAR,    // Turn existing Slime:$ walls clear, so they'll be removed on TRJ death.
    TAG_MINOR_FOOD_PURGE_RELOADED, // The exciting sequel, removing pizza/jerky.
    TAG_MINOR_ELYVILON_WRATH,      // Make Elyvilon wrath expire with XP gain.
    TAG_MINOR_DESOLATION_GLOBAL,   // Recover from saves where desolation is incorrectly marked as global
    TAG_MINOR_NO_MORE_LORC,        // Don't save lava orc temperature (or anything else). LO/Dj removal.
    TAG_MINOR_NO_ITEM_TRANSIT,     // Remove code to transit items across levels.
    TAG_MINOR_TOMB_HATCHES,        // Use fixed-destination hatches in Tomb.
    TAG_MINOR_TRANSPORTERS,        // Transporters and position marker changes.
    TAG_MINOR_SPIT_POISON_AGAIN_AGAIN, // save compat issues for TAG_MINOR_SPIT_POISON_AGAIN.
    TAG_MINOR_TRANSPORTER_LANDING, // Transporters landing site features.
    TAG_MINOR_STATLOCKED_GNOLLS,   // Gnolls have stats locked at 7/7/7.
    TAG_MINOR_LIGHTNING_ROD_XP_FIX, // Set XP debt for partially used l. rods.
    TAG_MINOR_LEVEL_XP_INFO,       // Track XP gain by level.
    TAG_MINOR_LEVEL_XP_INFO_FIX,   // Fix orb spawn XP tracking.
    TAG_MINOR_FOLLOWER_TRANSIT_TIME, // Handle updating lost_ones after placing.
    TAG_MINOR_GNOLLS_REDUX,        // Handle Gnolls that always train all skills and no stat lock.
    TAG_MINOR_TRAINING_TARGETS,    // training targets for skills
    TAG_MINOR_XP_SCALING,          // scale exp_available and total_experience
    TAG_MINOR_NO_ACTOR_HELD,       // Remove actor.held.
    TAG_MINOR_GOLDIFY_BOOKS,       // Spellbooks disintegrate when picked up, like gold/runes/orbs
    TAG_MINOR_VETO_DISINT,         // Replace veto_disintegrate map markers
    TAG_MINOR_LEVEL_XP_VAULTS,     // XP tracking now tracks vaults, not spawns.
    TAG_MINOR_REVEAL_TRAPS,        // All traps generate known
    TAG_MINOR_GAUNTLET_TRAPPED,    // It was briefly possible to get trapped in a specific gauntlet map.
    TAG_MINOR_REMOVE_DECKS,        // Decks are no more
    TAG_MINOR_GAMESEEDS,           // Game seeds + rng state saved
#endif
    NUM_TAG_MINORS,
    TAG_MINOR_VERSION = NUM_TAG_MINORS - 1
};

// Marshalled as a byte in several places.
COMPILE_CHECK(TAG_MINOR_VERSION <= 0xff);

// tags that affect loading bones files. If you do save compat that affects
// ghosts, these must be updated in addition to the enum above.
const set<int> bones_minor_tags =
        {TAG_MINOR_RESET,
#if TAG_MAJOR_VERSION == 34
         TAG_MINOR_NO_GHOST_SPELLCASTER,
         TAG_MINOR_MON_COLOUR_LOOKUP,
         TAG_MINOR_GHOST_ENERGY,
         TAG_MINOR_BOOL_FLIGHT,
#endif
        };

struct save_version
{
    save_version(int _major, int _minor) : major{_major}, minor{_minor}
    {
    }

    save_version() : save_version(-1,-1)
    {
    }

    static save_version current()
    {
        return save_version(TAG_MAJOR_VERSION, TAG_MINOR_VERSION);
    }

    static save_version current_bones()
    {
        return save_version(TAG_MAJOR_VERSION, *bones_minor_tags.crbegin());
    }

    bool valid() const
    {
        return major > 0 && minor > -1;
    }

    inline friend bool operator==(const save_version& lhs,
                                                    const save_version& rhs)
    {
        return lhs.major == rhs.major && lhs.minor == rhs.minor;
    }

    inline friend bool operator!=(const save_version& lhs,
                                                    const save_version& rhs)
    {
        return !operator==(lhs, rhs);
    }

    inline friend bool operator< (const save_version& lhs,
                                                    const save_version& rhs)
    {
        return lhs.major < rhs.major || lhs.major == rhs.major &&
                                                    lhs.minor < rhs.minor;
    }
    inline friend bool operator> (const save_version& lhs,
                                                    const save_version& rhs)
    {
        return  operator< (rhs, lhs);
    }
    inline friend bool operator<=(const save_version& lhs,
                                                    const save_version& rhs)
    {
        return !operator> (lhs, rhs);
    }
    inline friend bool operator>=(const save_version& lhs,
                                                    const save_version& rhs)
    {
        return !operator< (lhs,rhs);
    }


    bool is_future() const
    {
        return valid() && *this > save_version::current();
    }

    bool is_past() const
    {
        return valid() && *this < save_version::current();
    }

    bool is_ancient() const
    {
        return valid() &&
#if TAG_MAJOR_VERSION == 34
            (major < 33 && minor < 17);
#else
            major < TAG_MAJOR_VERSION;
#endif
    }

    bool is_compatible() const
    {
        return valid() && !is_ancient() && !is_future();
    }

    bool is_current() const
    {
        return valid() && *this == save_version::current();
    }

    bool is_bones_version() const
    {
        return valid() && is_compatible() && bones_minor_tags.count(minor) > 0;
    }

    int major;
    int minor;
};
