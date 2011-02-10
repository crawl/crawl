/*
 *  File:       enum.h
 *  Summary:    Global (ick) enums.
 *  Written by: Daniel Ligon
 */


#ifndef ENUM_H
#define ENUM_H

#include "tag-version.h"

enum ability_type
{
    ABIL_NON_ABILITY = -1,
    // Innate abilities and (Demonspawn) mutations.
    ABIL_SPIT_POISON = 1,
    ABIL_BREATHE_FIRE,
    ABIL_BREATHE_FROST,
    ABIL_BREATHE_POISON,
    ABIL_BREATHE_LIGHTNING,
    ABIL_BREATHE_POWER,
    ABIL_BREATHE_STICKY_FLAME,
    ABIL_BREATHE_STEAM,
    ABIL_BREATHE_MEPHITIC,
    ABIL_SPIT_ACID,
    ABIL_TELEPORTATION,
    ABIL_BLINK,

    // Others
    ABIL_DELAYED_FIREBALL,
    ABIL_END_TRANSFORMATION,

    // Species-specific abilities.
    // Demonspawn-only
    ABIL_THROW_FLAME,
    ABIL_THROW_FROST,
    ABIL_HELLFIRE,
    // Kenku, Draconians
    ABIL_FLY,
    ABIL_FLY_II,
    ABIL_STOP_FLYING,
    // Mummies
    ABIL_MUMMY_RESTORATION,
    // Vampires
    ABIL_TRAN_BAT,
    ABIL_BOTTLE_BLOOD,
    // Deep Dwarves
    ABIL_RECHARGING,

    // Evoking items.
    ABIL_EVOKE_BERSERK = 40,
    ABIL_EVOKE_TELEPORTATION,
    ABIL_EVOKE_BLINK,
    ABIL_EVOKE_TURN_INVISIBLE,
    ABIL_EVOKE_TURN_VISIBLE,
    ABIL_EVOKE_LEVITATE,
    ABIL_EVOKE_STOP_LEVITATING,

    // Divine abilities
    // Zin
    ABIL_ZIN_SUSTENANCE = 50,
    ABIL_ZIN_RECITE,
    ABIL_ZIN_VITALISATION,
    ABIL_ZIN_IMPRISON,
    ABIL_ZIN_SANCTUARY,
    ABIL_ZIN_CURE_ALL_MUTATIONS,
    // TSO
    ABIL_TSO_DIVINE_SHIELD = 60,
    ABIL_TSO_CLEANSING_FLAME,
    ABIL_TSO_SUMMON_DIVINE_WARRIOR,
    // Kiku
    ABIL_KIKU_RECEIVE_CORPSES = 70,
    // Yredelemnul
    ABIL_YRED_INJURY_MIRROR = 80,
    ABIL_YRED_ANIMATE_REMAINS,
    ABIL_YRED_RECALL_UNDEAD_SLAVES,
    ABIL_YRED_ANIMATE_DEAD,
    ABIL_YRED_DRAIN_LIFE,
    ABIL_YRED_ENSLAVE_SOUL,
    ABIL_YRED_ANIMATE_REMAINS_OR_DEAD,
    // Xom, Vehumet = 90
    // Okawaru
    ABIL_OKAWARU_HEROISM = 100,
    ABIL_OKAWARU_FINESSE,
    // Makhleb
    ABIL_MAKHLEB_MINOR_DESTRUCTION = 110,
    ABIL_MAKHLEB_LESSER_SERVANT_OF_MAKHLEB,
    ABIL_MAKHLEB_MAJOR_DESTRUCTION,
    ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB,
    // Sif Muna
    ABIL_SIF_MUNA_CHANNEL_ENERGY = 120,
    ABIL_SIF_MUNA_FORGET_SPELL,
    // Trog
    ABIL_TROG_BURN_SPELLBOOKS = 130,
    ABIL_TROG_BERSERK,
    ABIL_TROG_REGEN_MR,
    ABIL_TROG_BROTHERS_IN_ARMS,
    // Elyvilon
    ABIL_ELYVILON_LIFESAVING = 140,
    ABIL_ELYVILON_LESSER_HEALING_SELF,
    ABIL_ELYVILON_LESSER_HEALING_OTHERS,
    ABIL_ELYVILON_PURIFICATION,
    ABIL_ELYVILON_GREATER_HEALING_SELF,
    ABIL_ELYVILON_GREATER_HEALING_OTHERS,
    ABIL_ELYVILON_RESTORATION,
    ABIL_ELYVILON_DIVINE_VIGOUR,
    // Lugonu
    ABIL_LUGONU_ABYSS_EXIT = 150,
    ABIL_LUGONU_BEND_SPACE,
    ABIL_LUGONU_BANISH,
    ABIL_LUGONU_CORRUPT,
    ABIL_LUGONU_ABYSS_ENTER,
    // Nemelex
    ABIL_NEMELEX_DRAW_ONE = 160,
    ABIL_NEMELEX_PEEK_TWO,
    ABIL_NEMELEX_TRIPLE_DRAW,
    ABIL_NEMELEX_MARK_FOUR,
    ABIL_NEMELEX_STACK_FIVE,
    // Beogh
    ABIL_BEOGH_SMITING = 170,
    ABIL_BEOGH_RECALL_ORCISH_FOLLOWERS,
    // Jiyva
    ABIL_JIYVA_CALL_JELLY = 180,
    ABIL_JIYVA_JELLY_PARALYSE,
    ABIL_JIYVA_SLIMIFY,
    ABIL_JIYVA_CURE_BAD_MUTATION,
    // Fedhas
    ABIL_FEDHAS_FUNGAL_BLOOM = 190,
    ABIL_FEDHAS_SUNLIGHT,
    ABIL_FEDHAS_RAIN,
    ABIL_FEDHAS_PLANT_RING,
    ABIL_FEDHAS_SPAWN_SPORES,
    ABIL_FEDHAS_EVOLUTION,
    // Cheibriados
    ABIL_CHEIBRIADOS_PONDEROUSIFY = 200,
    ABIL_CHEIBRIADOS_TIME_STEP,
    ABIL_CHEIBRIADOS_TIME_BEND,
    ABIL_CHEIBRIADOS_SLOUCH,
    // Ashenzari
    ABIL_ASHENZARI_SCRYING = 210,
    ABIL_ASHENZARI_TRANSFER_KNOWLEDGE,
    ABIL_ASHENZARI_END_TRANSFER,

    // General divine (pseudo) abilities.
    ABIL_RENOUNCE_RELIGION,

    // Zot Defence abilities
    ABIL_MAKE_FUNGUS = 230,
    ABIL_MAKE_PLANT,
    ABIL_MAKE_OKLOB_SAPLING,
    ABIL_MAKE_DART_TRAP,
    ABIL_MAKE_ICE_STATUE,
    ABIL_MAKE_OCS,
    ABIL_MAKE_SILVER_STATUE,
    ABIL_MAKE_CURSE_SKULL,
    ABIL_MAKE_TELEPORT,
    ABIL_MAKE_ARROW_TRAP,
    ABIL_MAKE_BOLT_TRAP,
    ABIL_MAKE_SPEAR_TRAP,
    ABIL_MAKE_AXE_TRAP,
    ABIL_MAKE_NEEDLE_TRAP,
    ABIL_MAKE_NET_TRAP,
    ABIL_MAKE_TELEPORT_TRAP,
    ABIL_MAKE_ALARM_TRAP,
    ABIL_MAKE_BLADE_TRAP,
    ABIL_MAKE_OKLOB_CIRCLE,
    ABIL_MAKE_ACQUIRE_GOLD,
    ABIL_MAKE_ACQUIREMENT,
    ABIL_MAKE_WATER,
    ABIL_MAKE_ELECTRIC_EEL,
    ABIL_MAKE_BAZAAR,
    ABIL_MAKE_ALTAR,
    ABIL_MAKE_GRENADES,
    ABIL_MAKE_SAGE,
    ABIL_MAKE_OKLOB_PLANT,
    ABIL_MAKE_BURNING_BUSH,
    ABIL_REMOVE_CURSE,
};

enum activity_interrupt_type
{
    AI_FORCE_INTERRUPT = 0,         // Forcibly kills any activity that can be
                                    // interrupted.
    AI_KEYPRESS,
    AI_FULL_HP,                     // Player is fully healed
    AI_FULL_MP,                     // Player has recovered all mp
    AI_STATUE,                      // Bad statue has come into view
    AI_HUNGRY,                      // Hunger increased
    AI_MESSAGE,                     // Message was displayed
    AI_HP_LOSS,
    AI_BURDEN_CHANGE,
    AI_STAT_CHANGE,
    AI_SEE_MONSTER,
    AI_MONSTER_ATTACKS,
    AI_TELEPORT,
    AI_HIT_MONSTER,                 // Player hit monster (invis or
                                    // mimic) during travel/explore.

    // Always the last.
    NUM_AINTERRUPTS
};

enum actor_type
{
    ACT_NONE = -1,
    ACT_PLAYER,
    ACT_MONSTER,
};

enum attribute_type
{
    ATTR_DIVINE_LIGHTNING_PROTECTION,
    ATTR_DIVINE_REGENERATION,
    ATTR_DIVINE_DEATH_CHANNEL,
    ATTR_CARD_COUNTDOWN,
    ATTR_WAS_SILENCED,          //jmf: added for silenced messages
    ATTR_GOD_GIFT_COUNT,        //jmf: added to help manage god gift giving
    ATTR_DELAYED_FIREBALL,      // bwr: reserve fireballs
    ATTR_HELD,                  // caught in a net
    ATTR_ABYSS_ENTOURAGE,       // maximum number of hostile monsters in
                                // sight of the player while in the Abyss.
    ATTR_DIVINE_VIGOUR,         // strength of Ely's Divine Vigour
    ATTR_DIVINE_STAMINA,        // strength of Zin's Divine Stamina
    ATTR_DIVINE_SHIELD,         // strength of TSO's Divine Shield
    ATTR_UNIQUE_RUNES,
    ATTR_DEMONIC_RUNES,
    ATTR_ABYSSAL_RUNES,
    ATTR_RUNES_IN_ZOT,         // Unused but needed for save file compatibility.
    ATTR_WEAPON_SWAP_INTERRUPTED,
    ATTR_GOLD_FOUND,
    ATTR_PURCHASES,            // Gold amount spent at shops.
    ATTR_DONATIONS,            // Gold amount donated to Zin.
    ATTR_MISC_SPENDING,        // Spending for things like ziggurats.
    ATTR_RND_LVL_BOOKS,        // Bitfield of level-type randart spellbooks
                               // player has seen.
    ATTR_NOISES,               // A noisy artefact is equipped.
    ATTR_SHADOWS,              // Lantern of shadows effect.
    ATTR_FRUIT_FOUND,          // Mask of fruit types found.
    ATTR_LEV_UNCANCELLABLE,    // Potion or spell of levitation is in effect.
    ATTR_INVIS_UNCANCELLABLE,  // Potion/spell/wand of invis is in effect.
    ATTR_PERM_LEVITATION,      // Kenku flight or boots of lev are on.
    ATTR_SEEN_INVIS_TURN,      // Last turn you saw something invisible.
    ATTR_SEEN_INVIS_SEED,      // Random seed for invis monster positions.
    NUM_ATTRIBUTES
};

enum transformation_type
{
    TRAN_NONE,
    TRAN_SPIDER,
    TRAN_BLADE_HANDS,
    TRAN_STATUE,
    TRAN_ICE_BEAST,
    TRAN_DRAGON,
    TRAN_LICH,
    TRAN_BAT,
    TRAN_PIG,
};

enum beam_type                  // beam[].flavour
{
    BEAM_NONE,

    BEAM_MISSILE,
    BEAM_MMISSILE,                //    and similarly irresistible things
    BEAM_FIRE,
    BEAM_COLD,
    BEAM_MAGIC,
    BEAM_ELECTRICITY,
    BEAM_POISON,
    BEAM_NEG,
    BEAM_ACID,
    BEAM_MIASMA,
    BEAM_WATER,

    BEAM_SPORE,
    BEAM_POISON_ARROW,
    BEAM_HELLFIRE,
    BEAM_NAPALM,
    BEAM_STEAM,
    BEAM_ENERGY,
    BEAM_HOLY,
    BEAM_FRAG,
    BEAM_LAVA,
    BEAM_ICE,
    BEAM_NUKE,
    BEAM_LIGHT,
    BEAM_RANDOM,                  // currently translates into FIRE..ACID
    BEAM_CHAOS,

    // Enchantments
    BEAM_SLOW,
    BEAM_FIRST_ENCHANTMENT = BEAM_SLOW,
    BEAM_HASTE,
    BEAM_MIGHT,
    BEAM_HEALING,
    BEAM_PARALYSIS,
    BEAM_CONFUSION,
    BEAM_INVISIBILITY,
    BEAM_DIGGING,
    BEAM_TELEPORT,
    BEAM_POLYMORPH,
    BEAM_CHARM,
    BEAM_BANISH,
    BEAM_DEGENERATE,
#if TAG_MAJOR_VERSION == 32
    BEAM_ENSLAVE_UNDEAD,
#endif
    BEAM_ENSLAVE_SOUL,
    BEAM_PAIN,
    BEAM_DISPEL_UNDEAD,
    BEAM_DISINTEGRATION,
    BEAM_ENSLAVE_DEMON,
    BEAM_BLINK,
    BEAM_BLINK_CLOSE,
    BEAM_PETRIFY,
    BEAM_CORONA,
    BEAM_PORKALATOR,
    BEAM_HIBERNATION,
    BEAM_BERSERK,
    BEAM_SLEEP,
    BEAM_LAST_ENCHANTMENT = BEAM_SLEEP,

    // new beams for evaporate
    BEAM_POTION_STINKING_CLOUD,
    BEAM_POTION_POISON,
    BEAM_POTION_MIASMA,
    BEAM_POTION_STEAM,
    BEAM_POTION_FIRE,
    BEAM_POTION_COLD,
    BEAM_POTION_BLACK_SMOKE,
    BEAM_POTION_GREY_SMOKE,
    BEAM_POTION_MUTAGENIC,
    BEAM_POTION_BLUE_SMOKE,
    BEAM_POTION_PURPLE_SMOKE,
    BEAM_POTION_RAIN,
    BEAM_GLOOM,
    BEAM_INK,
    BEAM_HOLY_FLAME,
    BEAM_HOLY_LIGHT,
    BEAM_POTION_RANDOM,
    BEAM_AIR,

    BEAM_LAST_REAL = BEAM_AIR,

    // For getting the visual effect of a beam.
    BEAM_VISUAL,

    BEAM_TORMENT_DAMAGE,          // Pseudo-beam for damage flavour.
    BEAM_FIRST_PSEUDO = BEAM_TORMENT_DAMAGE,
    BEAM_DEVOUR_FOOD,             // Pseudo-beam for harpies' devouring food.

    NUM_BEAMS
};

enum book_type
{
    BOOK_MINOR_MAGIC_I,
    BOOK_MINOR_MAGIC_II,
    BOOK_MINOR_MAGIC_III,
    BOOK_CONJURATIONS_I,
    BOOK_CONJURATIONS_II,
    BOOK_FLAMES,
    BOOK_FROST,
    BOOK_SUMMONINGS,
    BOOK_FIRE,
    BOOK_ICE,
    BOOK_SPATIAL_TRANSLOCATIONS,
    BOOK_ENCHANTMENTS,
    BOOK_YOUNG_POISONERS,
    BOOK_TEMPESTS,
    BOOK_DEATH,
    BOOK_HINDERANCE,
    BOOK_CHANGES,
    BOOK_TRANSFIGURATIONS,
    BOOK_WAR_CHANTS,
    BOOK_CLOUDS,
    BOOK_NECROMANCY,
    BOOK_CALLINGS,
    BOOK_MALEDICT,
    BOOK_AIR,
    BOOK_SKY,
    BOOK_WARP,
    BOOK_ENVENOMATIONS,
    BOOK_UNLIFE,
    BOOK_CONTROL,
    BOOK_MUTATIONS,
    BOOK_GEOMANCY,
    BOOK_EARTH,
    BOOK_WIZARDRY,
    BOOK_POWER,
    BOOK_CANTRIPS,
    BOOK_PARTY_TRICKS,
    BOOK_STALKING,
    BOOK_BRANDS,
    BOOK_DRAGON,
    BOOK_BURGLARY,
    BOOK_DREAMS,
    BOOK_CHEMISTRY,
    MAX_NORMAL_BOOK = BOOK_CHEMISTRY,

    MIN_GOD_ONLY_BOOK,
    BOOK_ANNIHILATIONS = MIN_GOD_ONLY_BOOK,
    BOOK_DEMONOLOGY,
    BOOK_NECRONOMICON,
    MAX_GOD_ONLY_BOOK = BOOK_NECRONOMICON,

    MAX_FIXED_BOOK = MAX_GOD_ONLY_BOOK,

    BOOK_RANDART_LEVEL,
    BOOK_RANDART_THEME,

    BOOK_MANUAL,
    BOOK_DESTRUCTION,
    NUM_BOOKS
};

#define NUM_NORMAL_BOOKS     (MAX_NORMAL_BOOK + 1)
#define NUM_FIXED_BOOKS      (MAX_FIXED_BOOK + 1)

enum branch_type                // you.where_are_you
{
    BRANCH_MAIN_DUNGEON,
    BRANCH_ECUMENICAL_TEMPLE,
    BRANCH_FIRST_NON_DUNGEON = BRANCH_ECUMENICAL_TEMPLE,
    BRANCH_ORCISH_MINES,
    BRANCH_ELVEN_HALLS,
    BRANCH_LAIR,
    BRANCH_SWAMP,
    BRANCH_SHOALS,
    BRANCH_SLIME_PITS,
    BRANCH_SNAKE_PIT,
    BRANCH_HIVE,
    BRANCH_VAULTS,
    BRANCH_HALL_OF_BLADES,
    BRANCH_CRYPT,
    BRANCH_TOMB,
    BRANCH_VESTIBULE_OF_HELL,
    BRANCH_FIRST_HELL,
    BRANCH_DIS = BRANCH_FIRST_HELL,
    BRANCH_GEHENNA,
    BRANCH_COCYTUS,
    BRANCH_TARTARUS,
    BRANCH_LAST_HELL = BRANCH_TARTARUS,
    BRANCH_HALL_OF_ZOT,
    BRANCH_FOREST,
    BRANCH_SPIDER_NEST,
    BRANCH_DWARVEN_HALL,
    NUM_BRANCHES
};

enum burden_state_type          // you.burden_state
{
    BS_UNENCUMBERED,            //    0
    BS_ENCUMBERED = 2,          //    2
    BS_OVERLOADED = 5,          //    5
};

enum canned_message_type
{
    MSG_SOMETHING_APPEARS,
    MSG_NOTHING_HAPPENS,
    MSG_YOU_UNAFFECTED,
    MSG_YOU_RESIST,
    MSG_YOU_PARTIALLY_RESIST,
    MSG_TOO_BERSERK,
    MSG_PRESENT_FORM,
    MSG_NOTHING_CARRIED,
    MSG_CANNOT_DO_YET,
    MSG_OK,
    MSG_UNTHINKING_ACT,
    MSG_SPELL_FIZZLES,
    MSG_HUH,
    MSG_EMPTY_HANDED,
    MSG_YOU_BLINK,
    MSG_STRANGE_STASIS,
    MSG_NO_SPELLS,
    MSG_MANA_INCREASE,
    MSG_MANA_DECREASE,
    MSG_TOO_HUNGRY,
};

enum char_set_type
{
    CSET_ASCII,         // flat 7-bit ASCII
    CSET_IBM,           // 8-bit ANSI/Code Page 437
    CSET_DEC,           // 8-bit DEC, 0xE0-0xFF shifted for line drawing chars
    CSET_UNICODE,       // Unicode
    NUM_CSET
};

enum cleansing_flame_source_type
{
    CLEANSING_FLAME_GENERIC    = -1,
    CLEANSING_FLAME_SPELL      = -2, // SPELL_FLAME_OF_CLEANSING
    CLEANSING_FLAME_INVOCATION = -3, // ABIL_TSO_CLEANSING_FLAME
    CLEANSING_FLAME_TSO        = -4, // TSO effect
};

enum cloud_type
{
    CLOUD_NONE,
    CLOUD_FIRE,
    CLOUD_STINK,
    CLOUD_COLD,
    CLOUD_POISON,
    CLOUD_BLACK_SMOKE,
    CLOUD_GREY_SMOKE,
    CLOUD_BLUE_SMOKE,
    CLOUD_PURPLE_SMOKE,
    CLOUD_TLOC_ENERGY,
    CLOUD_FOREST_FIRE,
    CLOUD_STEAM,
    CLOUD_GLOOM,
    CLOUD_INK,
    CLOUD_HOLY_FLAMES,
    CLOUD_MIASMA,
    CLOUD_MIST,
    CLOUD_CHAOS,
    CLOUD_RAIN,
    CLOUD_MUTAGENIC,
    CLOUD_MAGIC_TRAIL,
    CLOUD_TORNADO,
    NUM_CLOUD_TYPES,

    CLOUD_OPAQUE_FIRST = CLOUD_BLACK_SMOKE,
    CLOUD_OPAQUE_LAST  = CLOUD_HOLY_FLAMES,

    CLOUD_RANDOM = 98,
    CLOUD_DEBUGGING,
};

enum command_type
{
    CMD_NO_CMD = 1000,
    CMD_NO_CMD_DEFAULT, // hack to allow assignment of keys to CMD_NO_CMD
    CMD_MOVE_NOWHERE,
    CMD_MOVE_LEFT,
    CMD_MOVE_DOWN,
    CMD_MOVE_UP,
    CMD_MOVE_RIGHT,
    CMD_MOVE_UP_LEFT,
    CMD_MOVE_DOWN_LEFT,
    CMD_MOVE_UP_RIGHT,
    CMD_MOVE_DOWN_RIGHT,
    CMD_RUN_LEFT,
    CMD_RUN_DOWN,
    CMD_RUN_UP,
    CMD_RUN_RIGHT,
    CMD_RUN_UP_LEFT,
    CMD_RUN_DOWN_LEFT,
    CMD_RUN_UP_RIGHT,
    CMD_RUN_DOWN_RIGHT,
    CMD_OPEN_DOOR_LEFT,
    CMD_OPEN_DOOR_DOWN,
    CMD_OPEN_DOOR_UP,
    CMD_OPEN_DOOR_RIGHT,
    CMD_OPEN_DOOR_UP_LEFT,
    CMD_OPEN_DOOR_DOWN_LEFT,
    CMD_OPEN_DOOR_UP_RIGHT,
    CMD_OPEN_DOOR_DOWN_RIGHT,
    CMD_OPEN_DOOR,
    CMD_CLOSE_DOOR,
    CMD_REST,
    CMD_GO_UPSTAIRS,
    CMD_GO_DOWNSTAIRS,
    CMD_TOGGLE_AUTOPICKUP,
    CMD_TOGGLE_FRIENDLY_PICKUP,
    CMD_PICKUP,
    CMD_PICKUP_QUANTITY,
    CMD_DROP,
    CMD_BUTCHER,
    CMD_INSPECT_FLOOR,
    CMD_SHOW_TERRAIN,
    CMD_FULL_VIEW,
    CMD_EXAMINE_OBJECT,
    CMD_EVOKE,
    CMD_EVOKE_WIELDED,
    CMD_WIELD_WEAPON,
    CMD_WEAPON_SWAP,
    CMD_FIRE,
    CMD_QUIVER_ITEM,
    CMD_THROW_ITEM_NO_QUIVER,
    CMD_WEAR_ARMOUR,
    CMD_REMOVE_ARMOUR,
    CMD_WEAR_JEWELLERY,
    CMD_REMOVE_JEWELLERY,
    CMD_CYCLE_QUIVER_FORWARD,
    CMD_CYCLE_QUIVER_BACKWARD,
    CMD_LIST_WEAPONS,
    CMD_LIST_ARMOUR,
    CMD_LIST_JEWELLERY,
    CMD_LIST_EQUIPMENT,
    CMD_LIST_GOLD,
    CMD_ZAP_WAND,
    CMD_CAST_SPELL,
    CMD_FORCE_CAST_SPELL,
    CMD_MEMORISE_SPELL,
    CMD_USE_ABILITY,
    CMD_PRAY,
    CMD_EAT,
    CMD_QUAFF,
    CMD_READ,
    CMD_LOOK_AROUND,
    CMD_SEARCH,
    CMD_SHOUT,
    CMD_DISARM_TRAP,
    CMD_CHARACTER_DUMP,
    CMD_DISPLAY_COMMANDS,
    CMD_DISPLAY_INVENTORY,
    CMD_DISPLAY_KNOWN_OBJECTS,
    CMD_DISPLAY_MUTATIONS,
    CMD_DISPLAY_SKILLS,
    CMD_DISPLAY_MAP,
    CMD_DISPLAY_OVERMAP,
    CMD_DISPLAY_RELIGION,
    CMD_DISPLAY_CHARACTER_STATUS,
    CMD_DISPLAY_SPELLS,
    CMD_EXPERIENCE_CHECK,
    CMD_ADJUST_INVENTORY,
    CMD_REPLAY_MESSAGES,
    CMD_REDRAW_SCREEN,
    CMD_MACRO_ADD,
    CMD_SAVE_GAME,
    CMD_SAVE_GAME_NOW,
    CMD_SUSPEND_GAME,
    CMD_QUIT,
    CMD_WIZARD,
    CMD_DESTROY_ITEM,

    CMD_FORGET_STASH,
    CMD_SEARCH_STASHES,
    CMD_EXPLORE,
    CMD_INTERLEVEL_TRAVEL,
    CMD_FIX_WAYPOINT,

    CMD_CLEAR_MAP,
    CMD_INSCRIBE_ITEM,
    CMD_MAKE_NOTE,
    CMD_RESISTS_SCREEN,

    CMD_READ_MESSAGES,

    CMD_MOUSE_MOVE,
    CMD_MOUSE_CLICK,

    CMD_ANNOTATE_LEVEL,

#ifdef CLUA_BINDINGS
    CMD_AUTOFIGHT,
#endif

#ifdef USE_TILE
    CMD_EDIT_PLAYER_TILE,
    CMD_MIN_TILE = CMD_EDIT_PLAYER_TILE,
    CMD_MAX_TILE = CMD_EDIT_PLAYER_TILE,
#endif

    // Repeat previous command
    CMD_PREV_CMD_AGAIN,

    // Repeat next command a given number of times
    CMD_REPEAT_CMD,

    CMD_MAX_NORMAL = CMD_REPEAT_CMD,

    // overmap commands
    CMD_MAP_CLEAR_MAP,
    CMD_MIN_OVERMAP = CMD_MAP_CLEAR_MAP,
    CMD_MAP_ADD_WAYPOINT,
    CMD_MAP_EXCLUDE_AREA,
    CMD_MAP_CLEAR_EXCLUDES,
    CMD_MAP_EXCLUDE_RADIUS,

    CMD_MAP_MOVE_LEFT,
    CMD_MAP_MOVE_DOWN,
    CMD_MAP_MOVE_UP,
    CMD_MAP_MOVE_RIGHT,
    CMD_MAP_MOVE_UP_LEFT,
    CMD_MAP_MOVE_DOWN_LEFT,
    CMD_MAP_MOVE_UP_RIGHT,
    CMD_MAP_MOVE_DOWN_RIGHT,

    CMD_MAP_JUMP_LEFT,
    CMD_MAP_JUMP_DOWN,
    CMD_MAP_JUMP_UP,
    CMD_MAP_JUMP_RIGHT,
    CMD_MAP_JUMP_UP_LEFT,
    CMD_MAP_JUMP_DOWN_LEFT,
    CMD_MAP_JUMP_UP_RIGHT,
    CMD_MAP_JUMP_DOWN_RIGHT,

    CMD_MAP_NEXT_LEVEL,
    CMD_MAP_PREV_LEVEL,
    CMD_MAP_GOTO_LEVEL,

    CMD_MAP_SCROLL_DOWN,
    CMD_MAP_SCROLL_UP,

    CMD_MAP_FIND_UPSTAIR,
    CMD_MAP_FIND_DOWNSTAIR,
    CMD_MAP_FIND_YOU,
    CMD_MAP_FIND_PORTAL,
    CMD_MAP_FIND_TRAP,
    CMD_MAP_FIND_ALTAR,
    CMD_MAP_FIND_EXCLUDED,
    CMD_MAP_FIND_F,
    CMD_MAP_FIND_WAYPOINT,
    CMD_MAP_FIND_STASH,
    CMD_MAP_FIND_STASH_REVERSE,

    CMD_MAP_GOTO_TARGET,

    CMD_MAP_WIZARD_TELEPORT,

    CMD_MAP_HELP,
    CMD_MAP_FORGET,

    CMD_MAP_EXIT_MAP,

    CMD_MAX_OVERMAP = CMD_MAP_EXIT_MAP,

    // targeting commands
    CMD_TARGET_DOWN_LEFT,
    CMD_MIN_TARGET = CMD_TARGET_DOWN_LEFT,
    CMD_TARGET_DOWN,
    CMD_TARGET_DOWN_RIGHT,
    CMD_TARGET_LEFT,
    CMD_TARGET_RIGHT,
    CMD_TARGET_UP_LEFT,
    CMD_TARGET_UP,
    CMD_TARGET_UP_RIGHT,

    CMD_TARGET_DIR_DOWN_LEFT,
    CMD_TARGET_DIR_DOWN,
    CMD_TARGET_DIR_DOWN_RIGHT,
    CMD_TARGET_DIR_LEFT,
    CMD_TARGET_DIR_RIGHT,
    CMD_TARGET_DIR_UP_LEFT,
    CMD_TARGET_DIR_UP,
    CMD_TARGET_DIR_UP_RIGHT,

    CMD_TARGET_DESCRIBE,
    CMD_TARGET_CYCLE_TARGET_MODE,
    CMD_TARGET_PREV_TARGET,
    CMD_TARGET_MAYBE_PREV_TARGET,
    CMD_TARGET_SELECT,
    CMD_TARGET_SELECT_ENDPOINT,
    CMD_TARGET_SELECT_FORCE,
    CMD_TARGET_SELECT_FORCE_ENDPOINT,
    CMD_TARGET_OBJ_CYCLE_BACK,
    CMD_TARGET_OBJ_CYCLE_FORWARD,
    CMD_TARGET_CYCLE_FORWARD,
    CMD_TARGET_CYCLE_BACK,
    CMD_TARGET_CYCLE_BEAM,
    CMD_TARGET_CYCLE_MLIST = 2000, // for indices a-z in the monster list
    CMD_TARGET_CYCLE_MLIST_END = 2025,
    CMD_TARGET_TOGGLE_MLIST,
    CMD_TARGET_TOGGLE_BEAM,
    CMD_TARGET_CANCEL,
    CMD_TARGET_SHOW_PROMPT,
    CMD_TARGET_OLD_SPACE,
    CMD_TARGET_EXCLUDE,
    CMD_TARGET_FIND_TRAP,
    CMD_TARGET_FIND_PORTAL,
    CMD_TARGET_FIND_ALTAR,
    CMD_TARGET_FIND_UPSTAIR,
    CMD_TARGET_FIND_DOWNSTAIR,
    CMD_TARGET_FIND_YOU,
    CMD_TARGET_WIZARD_MAKE_FRIENDLY,
    CMD_TARGET_WIZARD_BLESS_MONSTER,
    CMD_TARGET_WIZARD_MAKE_SHOUT,
    CMD_TARGET_WIZARD_GIVE_ITEM,
    CMD_TARGET_WIZARD_MOVE,
    CMD_TARGET_WIZARD_PATHFIND,
    CMD_TARGET_WIZARD_GAIN_LEVEL,
    CMD_TARGET_WIZARD_MISCAST,
    CMD_TARGET_WIZARD_MAKE_SUMMONED,
    CMD_TARGET_WIZARD_POLYMORPH,
    CMD_TARGET_WIZARD_DEBUG_MONSTER,
    CMD_TARGET_WIZARD_HEAL_MONSTER,
    CMD_TARGET_WIZARD_HURT_MONSTER,
    CMD_TARGET_WIZARD_DEBUG_PORTAL,
    CMD_TARGET_WIZARD_KILL_MONSTER,
    CMD_TARGET_WIZARD_BANISH_MONSTER,
    CMD_TARGET_MOUSE_MOVE,
    CMD_TARGET_MOUSE_SELECT,
    CMD_TARGET_HELP,
    CMD_MAX_TARGET = CMD_TARGET_HELP,

#ifdef USE_TILE
    // Tile doll editing screen
    CMD_DOLL_RANDOMIZE,
    CMD_MIN_DOLL = CMD_DOLL_RANDOMIZE,
    CMD_DOLL_SELECT_NEXT_DOLL,
    CMD_DOLL_SELECT_PREV_DOLL,
    CMD_DOLL_SELECT_NEXT_PART,
    CMD_DOLL_SELECT_PREV_PART,
    CMD_DOLL_CHANGE_PART_NEXT,
    CMD_DOLL_CHANGE_PART_PREV,
    CMD_DOLL_CONFIRM_CHOICE,
    CMD_DOLL_COPY,
    CMD_DOLL_PASTE,
    CMD_DOLL_TAKE_OFF,
    CMD_DOLL_TAKE_OFF_ALL,
    CMD_DOLL_TOGGLE_EQUIP,
    CMD_DOLL_TOGGLE_EQUIP_ALL,
    CMD_DOLL_JOB_DEFAULT,
    CMD_DOLL_CHANGE_MODE,
    CMD_DOLL_SAVE,
    CMD_DOLL_QUIT,
    CMD_MAX_DOLL = CMD_DOLL_QUIT,
#endif

    // Disable/enable -more- prompts.
    CMD_DISABLE_MORE,
    CMD_MIN_SYNTHETIC = CMD_DISABLE_MORE,
    CMD_ENABLE_MORE,

    // [ds] Silently ignored, requests another round of input.
    CMD_NEXT_CMD,

    // Must always be last
    CMD_MAX_CMD
};

enum conduct_type
{
    DID_NOTHING,
    DID_NECROMANCY,                       // vamp/drain/pain/reap, Zong/Curses
    DID_HOLY,                             // holy wrath, holy word scrolls
    DID_UNHOLY,                           // demon weapons, demon spells
    DID_ATTACK_HOLY,
    DID_ATTACK_NEUTRAL,
    DID_ATTACK_FRIEND,
    DID_FRIEND_DIED,
    DID_STABBING,                         // unused
    DID_UNCHIVALRIC_ATTACK,
    DID_POISON,
    DID_DEDICATED_BUTCHERY,               // unused
    // killings need no longer be dedicated (JPEG)
    DID_KILL_LIVING,
    DID_KILL_UNDEAD,
    DID_KILL_DEMON,
    DID_KILL_NATURAL_UNHOLY,              // TSO
    DID_KILL_NATURAL_EVIL,                // TSO
    DID_KILL_UNCLEAN,                     // Zin
    DID_KILL_CHAOTIC,                     // Zin
    DID_KILL_WIZARD,                      // Trog
    DID_KILL_PRIEST,                      // Beogh
    DID_KILL_HOLY,
    DID_KILL_FAST,                        // Cheibriados
    DID_LIVING_KILLED_BY_UNDEAD_SLAVE,
    DID_LIVING_KILLED_BY_SERVANT,
    DID_UNDEAD_KILLED_BY_UNDEAD_SLAVE,
    DID_UNDEAD_KILLED_BY_SERVANT,
    DID_DEMON_KILLED_BY_UNDEAD_SLAVE,
    DID_DEMON_KILLED_BY_SERVANT,
    DID_NATURAL_UNHOLY_KILLED_BY_SERVANT, // TSO
    DID_NATURAL_EVIL_KILLED_BY_SERVANT,   // TSO
    DID_HOLY_KILLED_BY_UNDEAD_SLAVE,
    DID_HOLY_KILLED_BY_SERVANT,
    DID_SPELL_MEMORISE,
    DID_SPELL_CASTING,
    DID_SPELL_PRACTISE,
    DID_SPELL_NONUTILITY,                 // unused
    DID_CARDS,
    DID_STIMULANTS,                       // unused
    DID_DRINK_BLOOD,
    DID_CANNIBALISM,
    DID_EAT_MEAT,                         // unused
    DID_EAT_SOULED_BEING,                 // Zin
    DID_DELIBERATE_MUTATING,              // Zin
    DID_CAUSE_GLOWING,                    // Zin
    DID_UNCLEAN,                          // Zin (used unclean weapon/magic)
    DID_CHAOS,                            // Zin (used chaotic weapon/magic)
    DID_DESECRATE_ORCISH_REMAINS,         // Beogh
    DID_DESTROY_ORCISH_IDOL,              // Beogh
    DID_CREATE_LIFE,                      // unused
    DID_KILL_SLIME,                       // Jiyva
    DID_KILL_PLANT,                       // Fedhas
    DID_PLANT_KILLED_BY_SERVANT,          // Fedhas
    DID_HASTY,                            // Cheibriados
    DID_GLUTTONY,                         // unused, previously Cheibriados
    DID_CORPSE_VIOLATION,                 // Fedhas (Necromancy involving
                                          // corpses/chunks).
    DID_SOULED_FRIEND_DIED,               // Zin
    DID_UNCLEAN_KILLED_BY_SERVANT,        // Zin
    DID_CHAOTIC_KILLED_BY_SERVANT,        // Zin
    DID_ATTACK_IN_SANCTUARY,              // Zin
    DID_KILL_ARTIFICIAL,                  // Yredelemnul
    DID_ARTIFICIAL_KILLED_BY_UNDEAD_SLAVE,// Yredelemnul
    DID_ARTIFICIAL_KILLED_BY_SERVANT,     // Yredelemnul
    DID_DESTROY_SPELLBOOK,                // Sif Muna
    DID_EXPLORATION,                      // Ashenzari, wrath timers

    NUM_CONDUCTS
};

enum confirm_prompt_type
{
    CONFIRM_CANCEL,             // automatically answer 'no', i.e. disallow
    CONFIRM_PROMPT,             // prompt
    CONFIRM_NONE,               // automatically answer 'yes'
};

enum confirm_level_type
{
    CONFIRM_NONE_EASY,
    CONFIRM_SAFE_EASY,
    CONFIRM_ALL_EASY,
};

// When adding new delays, update their names in delay.cc, or bad things will
// happen.
enum delay_type
{
    DELAY_NOT_DELAYED,
    DELAY_EAT,
    DELAY_FEED_VAMPIRE,
    DELAY_ARMOUR_ON,
    DELAY_ARMOUR_OFF,
    DELAY_JEWELLERY_ON,
    DELAY_MEMORISE,
    DELAY_BUTCHER,
    DELAY_BOTTLE_BLOOD,
    DELAY_WEAPON_SWAP,                 // for easy_butcher
    DELAY_PASSWALL,
    DELAY_DROP_ITEM,
    DELAY_MULTIDROP,
    DELAY_ASCENDING_STAIRS,
    DELAY_DESCENDING_STAIRS,
    DELAY_RECITE,  // Zin's Recite invocation

    // [dshaligram] Shift-running, resting, travel and macros are now
    // also handled as delays.
    DELAY_RUN,
    DELAY_REST,
    DELAY_TRAVEL,

    DELAY_MACRO,

    // In a macro delay, a stacked delay to tell Crawl to read and act on
    // one input command.
    DELAY_MACRO_PROCESS_KEY,

    DELAY_INTERRUPTIBLE,                // simple interruptible delay
    DELAY_UNINTERRUPTIBLE,              // simple uninterruptible delay

    NUM_DELAYS
};

enum description_level_type
{
    DESC_CAP_THE,
    DESC_NOCAP_THE,
    DESC_CAP_A,
    DESC_NOCAP_A,
    DESC_CAP_YOUR,
    DESC_NOCAP_YOUR,
    DESC_PLAIN,
    DESC_NOCAP_ITS,
    DESC_INVENTORY_EQUIP,
    DESC_INVENTORY,

    // Partial item names.
    DESC_BASENAME,                     // Base name of item subtype
    DESC_QUALNAME,                     // Name without articles, quantities,
                                       // enchantments.
    DESC_DBNAME,                       // Name with which to look up item
                                       // description in the db.

    DESC_NONE
};

enum game_direction_type
{
    GDT_GAME_START = 0,
    GDT_DESCENDING,
    GDT_ASCENDING,
};

enum game_type
{
    GAME_TYPE_UNSPECIFIED,
    GAME_TYPE_NORMAL,
    GAME_TYPE_TUTORIAL,
    GAME_TYPE_ARENA,
    GAME_TYPE_SPRINT,
    GAME_TYPE_HINTS,
    GAME_TYPE_ZOTDEF,
    GAME_TYPE_INSTRUCTIONS,
    NUM_GAME_TYPE
};

enum level_flag_type
{
    LFLAG_NONE = 0,

    LFLAG_NO_TELE_CONTROL = (1 << 0), // Teleport control not allowed.
    LFLAG_NOT_MAPPABLE    = (1 << 1), // Level not mappable (like Abyss).
    LFLAG_NO_MAGIC_MAP    = (1 << 2), // Level can't be magic mapped.
};

// NOTE: The order of these is very important to their usage!
// [dshaligram] If adding/removing from this list, also update view.cc!
enum dungeon_char_type
{
    DCHAR_WALL,
    DCHAR_WALL_MAGIC,
    DCHAR_FLOOR,
    DCHAR_FLOOR_MAGIC,
    DCHAR_DOOR_OPEN,
    DCHAR_DOOR_CLOSED,
    DCHAR_TRAP,
    DCHAR_STAIRS_DOWN,
    DCHAR_STAIRS_UP,
    DCHAR_GRATE,
    DCHAR_ALTAR,
    DCHAR_ARCH,
    DCHAR_FOUNTAIN,
    DCHAR_WAVY,
    DCHAR_STATUE,
    DCHAR_INVIS_EXPOSED,
    DCHAR_ITEM_DETECTED,
    DCHAR_ITEM_ORB,
    DCHAR_ITEM_WEAPON,
    DCHAR_ITEM_ARMOUR,
    DCHAR_ITEM_WAND,
    DCHAR_ITEM_FOOD,
    DCHAR_ITEM_SCROLL,
    DCHAR_ITEM_RING,
    DCHAR_ITEM_POTION,
    DCHAR_ITEM_MISSILE,
    DCHAR_ITEM_BOOK,
    DCHAR_ITEM_STAVE,
    DCHAR_ITEM_MISCELLANY,
    DCHAR_ITEM_CORPSE,
    DCHAR_ITEM_GOLD,
    DCHAR_ITEM_AMULET,
    DCHAR_CLOUD,
    DCHAR_TREE,

    DCHAR_SPACE,
    DCHAR_FIRED_FLASK,
    DCHAR_FIRED_BOLT,
    DCHAR_FIRED_CHUNK,
    DCHAR_FIRED_BOOK,
    DCHAR_FIRED_WEAPON,
    DCHAR_FIRED_ZAP,
    DCHAR_FIRED_BURST,
    DCHAR_FIRED_STICK,
    DCHAR_FIRED_TRINKET,
    DCHAR_FIRED_SCROLL,
    DCHAR_FIRED_DEBUG,
    DCHAR_FIRED_ARMOUR,
    DCHAR_FIRED_MISSILE,
    DCHAR_EXPLOSION,

    NUM_DCHAR_TYPES
};

// When adding:
//
// * New stairs/portals: update grid_stair_direction.
// * Any: edit view.cc and add a glyph and colour for the feature.
// * Any: edit directn.cc and add a description for the feature.
// * Any: edit dat/descript/features.txt and add a
//        long description if appropriate.
// * Any: check the grid_* functions in misc.cc and make sure
//        they return sane values for your new feature.
// * Any: edit dungeon.cc and add a symbol to map_feature() and
//        vault_grid() for the feature, if you want vault maps to
//        be able to use it.  If you do, also update
//        docs/develop/levels/syntax.txt with the new symbol.
// * Any: edit l_dgngrd.cc and add the feature's name to the dngn_feature_names
//        array, if you want vault map Lua code to be able to use the
//        feature, and/or you want to be able to create the feature
//        using the "create feature by name" wizard command.
// Also take note of MINMOVE and MINSEE above.
//
enum dungeon_feature_type
{
    DNGN_UNSEEN,
    DNGN_CLOSED_DOOR,
    DNGN_DETECTED_SECRET_DOOR,
    DNGN_SECRET_DOOR,
    DNGN_WAX_WALL,
    DNGN_METAL_WALL,
    DNGN_GREEN_CRYSTAL_WALL,
    DNGN_ROCK_WALL,
    DNGN_SLIMY_WALL,
    DNGN_STONE_WALL,
    DNGN_PERMAROCK_WALL,               // for undiggable walls
    DNGN_CLEAR_ROCK_WALL,              // transparent walls
    DNGN_CLEAR_STONE_WALL,
    DNGN_CLEAR_PERMAROCK_WALL,
    DNGN_GRATE,

    // Lowest/highest grid value which is a wall.
    DNGN_MINWALL = DNGN_WAX_WALL,
    DNGN_MAXWALL = DNGN_CLEAR_PERMAROCK_WALL,

    // Highest grid value which is opaque.
    DNGN_MAXOPAQUE = DNGN_PERMAROCK_WALL,

    // Lowest grid value which can be seen through.
    DNGN_MINSEE = DNGN_CLEAR_ROCK_WALL,

    // Highest grid value which can't be reached through.
    DNGN_MAX_NONREACH = DNGN_GRATE,

    DNGN_OPEN_SEA,                     // Shoals equivalent for permarock

    // Can be seen through and reached past.
    DNGN_TREE,
    DNGN_ORCISH_IDOL,
    DNGN_SWAMP_TREE,
    DNGN_GRANITE_STATUE = 21,
    DNGN_STATUE_RESERVED,

    // Highest solid grid value.
    DNGN_MAXSOLID = DNGN_STATUE_RESERVED,

    // Lowest grid value which can be passed by walking etc.
    DNGN_MINMOVE = 31,

    DNGN_LAVA = 61,
    DNGN_DEEP_WATER,

    DNGN_SHALLOW_WATER = 65,
    DNGN_MOVEMENT_MIN = DNGN_SHALLOW_WATER,
    DNGN_WATER_RESERVED,

    // Lowest grid value that an item can be placed on.
    DNGN_MINITEM = DNGN_SHALLOW_WATER,

    DNGN_FLOOR_MIN = 67,
    DNGN_FLOOR = DNGN_FLOOR_MIN,
    DNGN_FLOOR_RESERVED = 69,
    DNGN_FLOOR_MAX = DNGN_FLOOR_RESERVED,

    DNGN_EXIT_HELL,                    //   70
    DNGN_ENTER_HELL,                   //   71
    DNGN_OPEN_DOOR,                    //   72

    DNGN_TRAP_MECHANICAL = 75,         //   75
    DNGN_TRAP_MAGICAL,
    DNGN_TRAP_NATURAL,
    DNGN_UNDISCOVERED_TRAP,            //   78

    DNGN_ENTER_SHOP = 80,              //   80
    DNGN_ENTER_LABYRINTH,

    DNGN_STONE_STAIRS_DOWN_I,
    DNGN_STONE_STAIRS_DOWN_II,
    DNGN_STONE_STAIRS_DOWN_III,
    DNGN_ESCAPE_HATCH_DOWN,            //  85 - was: rock stairs (Stonesoup 0.3)

    // corresponding up stairs (same order as above)
    DNGN_STONE_STAIRS_UP_I,
    DNGN_STONE_STAIRS_UP_II,
    DNGN_STONE_STAIRS_UP_III,
    DNGN_ESCAPE_HATCH_UP,              //  89 - was: rock stairs (Stonesoup 0.3)

    // Various gates
    DNGN_ENTER_DIS = 92,               //   92
    DNGN_ENTER_GEHENNA,
    DNGN_ENTER_COCYTUS,
    DNGN_ENTER_TARTARUS,               //   95
    DNGN_ENTER_ABYSS,
    DNGN_EXIT_ABYSS,
    DNGN_STONE_ARCH,
    DNGN_ENTER_PANDEMONIUM,
    DNGN_EXIT_PANDEMONIUM,             //  100
    DNGN_TRANSIT_PANDEMONIUM,          //  101

    // Entrances to various branches
    DNGN_ENTER_FIRST_BRANCH = 110,     //  110
    DNGN_ENTER_DWARVEN_HALL = DNGN_ENTER_FIRST_BRANCH,
    DNGN_ENTER_ORCISH_MINES,
    DNGN_ENTER_HIVE,
    DNGN_ENTER_LAIR,
    DNGN_ENTER_SLIME_PITS,
    DNGN_ENTER_VAULTS,                 //  115
    DNGN_ENTER_CRYPT,
    DNGN_ENTER_HALL_OF_BLADES,
    DNGN_ENTER_ZOT,
    DNGN_ENTER_TEMPLE,
    DNGN_ENTER_SNAKE_PIT,              //  120
    DNGN_ENTER_ELVEN_HALLS,
    DNGN_ENTER_TOMB,
    DNGN_ENTER_SWAMP,                  //  122
    DNGN_ENTER_SHOALS,
    DNGN_ENTER_SPIDER_NEST,
    DNGN_ENTER_FOREST,
    DNGN_ENTER_LAST_BRANCH = DNGN_ENTER_FOREST,

    // Exits from various branches
    // Order must be the same as above
    DNGN_RETURN_FROM_FIRST_BRANCH = 130, //  130
    DNGN_RETURN_FROM_DWARVEN_HALL = DNGN_RETURN_FROM_FIRST_BRANCH,
    DNGN_RETURN_FROM_ORCISH_MINES,
    DNGN_RETURN_FROM_HIVE,
    DNGN_RETURN_FROM_LAIR,
    DNGN_RETURN_FROM_SLIME_PITS,
    DNGN_RETURN_FROM_VAULTS,           //  135
    DNGN_RETURN_FROM_CRYPT,
    DNGN_RETURN_FROM_HALL_OF_BLADES,
    DNGN_RETURN_FROM_ZOT,
    DNGN_RETURN_FROM_TEMPLE,
    DNGN_RETURN_FROM_SNAKE_PIT,        //  140
    DNGN_RETURN_FROM_ELVEN_HALLS,
    DNGN_RETURN_FROM_TOMB,
    DNGN_RETURN_FROM_SWAMP,            //  142
    DNGN_RETURN_FROM_SHOALS,
    DNGN_RETURN_FROM_SPIDER_NEST,
    DNGN_RETURN_FROM_FOREST,
    DNGN_RETURN_FROM_LAST_BRANCH = DNGN_RETURN_FROM_FOREST,

    // Portals to various places unknown.
    DNGN_ENTER_PORTAL_VAULT = 160,
    DNGN_EXIT_PORTAL_VAULT,
    DNGN_TEMP_PORTAL,

    // Order of altars must match order of gods (god_type)
    DNGN_ALTAR_FIRST_GOD = 180,        // 180
    DNGN_ALTAR_ZIN = DNGN_ALTAR_FIRST_GOD,
    DNGN_ALTAR_SHINING_ONE,
    DNGN_ALTAR_KIKUBAAQUDGHA,
    DNGN_ALTAR_YREDELEMNUL,
    DNGN_ALTAR_XOM,
    DNGN_ALTAR_VEHUMET,                //  185
    DNGN_ALTAR_OKAWARU,
    DNGN_ALTAR_MAKHLEB,
    DNGN_ALTAR_SIF_MUNA,
    DNGN_ALTAR_TROG,
    DNGN_ALTAR_NEMELEX_XOBEH,          //  190
    DNGN_ALTAR_ELYVILON,               //  191
    DNGN_ALTAR_LUGONU,
    DNGN_ALTAR_BEOGH,
    DNGN_ALTAR_JIYVA,
    DNGN_ALTAR_FEDHAS,
    DNGN_ALTAR_CHEIBRIADOS,
    DNGN_ALTAR_ASHENZARI,
    DNGN_ALTAR_LAST_GOD = DNGN_ALTAR_ASHENZARI,

    DNGN_FOUNTAIN_BLUE = 200,          //  200
    DNGN_FOUNTAIN_SPARKLING,           // aka 'Magic Fountain' {dlb}
    DNGN_FOUNTAIN_BLOOD,
    // same order as above!
    DNGN_DRY_FOUNTAIN_BLUE,
    DNGN_DRY_FOUNTAIN_SPARKLING,
    DNGN_DRY_FOUNTAIN_BLOOD,           //  205
    DNGN_PERMADRY_FOUNTAIN,
    DNGN_ABANDONED_SHOP,

    NUM_FEATURES                       //  208
};

enum duration_type
{
    DUR_INVIS,
    DUR_CONF,
    DUR_PARALYSIS,
    DUR_SLOW,
    DUR_MESMERISED,
    DUR_HASTE,
    DUR_MIGHT,
    DUR_BRILLIANCE,
    DUR_AGILITY,
    DUR_LEVITATION,
    DUR_BERSERK,
    DUR_POISONING,

    DUR_CONFUSING_TOUCH,
    DUR_SURE_BLADE,
    DUR_CORONA,
    DUR_DEATHS_DOOR,
    DUR_FIRE_SHIELD,

    DUR_BUILDING_RAGE,          // countdown to starting berserk
    DUR_EXHAUSTED,              // fatigue counter for berserk

    DUR_LIQUID_FLAMES,
    DUR_ICY_ARMOUR,
    DUR_REPEL_MISSILES,
    DUR_JELLY_PRAYER,
    DUR_PIETY_POOL,             // distribute piety over time
    DUR_DIVINE_VIGOUR,          // duration of Ely's Divine Vigour
    DUR_DIVINE_STAMINA,         // duration of Zin's Divine Stamina
    DUR_DIVINE_SHIELD,          // duration of TSO's Divine Shield
    DUR_REGENERATION,
    DUR_SWIFTNESS,
    DUR_STONEMAIL,
    DUR_CONTROLLED_FLIGHT,
    DUR_TELEPORT,
    DUR_CONTROL_TELEPORT,
    DUR_BREATH_WEAPON,
    DUR_TRANSFORMATION,
    DUR_DEATH_CHANNEL,
    DUR_DEFLECT_MISSILES,
    DUR_PHASE_SHIFT,
    DUR_SEE_INVISIBLE,
    DUR_WEAPON_BRAND,                  // general "branding" spell counter
    DUR_DEMONIC_GUARDIAN,              // demonic guardian timeout
    DUR_POWERED_BY_DEATH,
    DUR_SILENCE,
    DUR_CONDENSATION_SHIELD,
    DUR_STONESKIN,
    DUR_GOURMAND,
    DUR_BARGAIN,
    DUR_INSULATION,
    DUR_RESIST_POISON,
    DUR_RESIST_FIRE,
    DUR_RESIST_COLD,
    DUR_SLAYING,
    DUR_STEALTH,
    DUR_MAGIC_SHIELD,
    DUR_SLEEP,
    DUR_SAGE,
    DUR_TELEPATHY,
    DUR_PETRIFIED,
    DUR_LOWERED_MR,
    DUR_REPEL_STAIRS_MOVE,
    DUR_REPEL_STAIRS_CLIMB,
    DUR_COLOUR_SMOKE_TRAIL,
    DUR_SLIMIFY,
    DUR_TIME_STEP,
    DUR_ICEMAIL_DEPLETED,     // Wait this many turns for full Icemail
    DUR_MISLED,
    DUR_QUAD_DAMAGE,
    DUR_AFRAID,
    DUR_MIRROR_DAMAGE,
    DUR_SCRYING,
    DUR_TORNADO,
    DUR_LIQUEFYING,
    DUR_HEROISM,
    DUR_FINESSE,
    DUR_LIFESAVING,
    DUR_PARALYSIS_IMMUNITY,
    NUM_DURATIONS
};

// This list must match the enchant_names array in monster.cc or Crawl
// will CRASH, we kid you not.
// Enchantments that imply other enchantments should come first
// to avoid timeout message confusion. Currently:
//     berserk -> haste, might; fatigue -> slow
enum enchant_type
{
    ENCH_NONE = 0,
    ENCH_BERSERK,
    ENCH_HASTE,
    ENCH_MIGHT,
    ENCH_FATIGUE,        // Post-berserk fatigue.
    ENCH_SLOW,
    ENCH_FEAR,
    ENCH_CONFUSION,
    ENCH_INVIS,
    ENCH_POISON,
    ENCH_ROT,
    ENCH_SUMMON,
    ENCH_ABJ,
    ENCH_CORONA,
    ENCH_CHARM,
    ENCH_STICKY_FLAME,
    ENCH_GLOWING_SHAPESHIFTER,
    ENCH_SHAPESHIFTER,
    ENCH_TP,
    ENCH_SLEEP_WARY,
    ENCH_SUBMERGED,
    ENCH_SHORT_LIVED,
    ENCH_PARALYSIS,
    ENCH_SICK,
    ENCH_SLEEPY,         //   Monster can't wake until this wears off.
    ENCH_HELD,           //   Caught in a net.
    ENCH_BATTLE_FRENZY,  //   Monster is in a battle frenzy.
    ENCH_TEMP_PACIF,
    ENCH_PETRIFYING,
    ENCH_PETRIFIED,
    ENCH_LOWERED_MR,
    ENCH_SOUL_RIPE,
    ENCH_SLOWLY_DYING,
    ENCH_EAT_ITEMS,
    ENCH_AQUATIC_LAND,   // Water monsters lose hp while on land.
    ENCH_SPORE_PRODUCTION,
    ENCH_SLOUCH,
    ENCH_SWIFT,
    ENCH_TIDE,
    ENCH_INSANE,
    ENCH_SILENCE,
    ENCH_AWAKEN_FOREST,
    ENCH_EXPLODING,
    ENCH_BLEED,
    ENCH_PORTAL_TIMER,
    ENCH_SEVERED,
    ENCH_ANTIMAGIC,
    ENCH_FADING_AWAY,
    ENCH_PREPARING_RESURRECT,
    ENCH_REGENERATION,
    ENCH_RAISED_MR,
    ENCH_MIRROR_DAMAGE,
    ENCH_STONESKIN,
    ENCH_FEAR_INSPIRING,
    ENCH_PORTAL_PACIFIED,
    ENCH_WITHDRAWN,
    ENCH_ATTACHED,
    ENCH_LIFE_TIMER,    // Minimum time demonic guardian must exist.
    ENCH_LEVITATION,
    ENCH_HELPLESS,
    ENCH_LIQUEFYING,
    ENCH_PERM_TORNADO,
    ENCH_FAKE_ABJURATION,
    ENCH_DAZED,         // Dazed - less chance of acting each turn.
    ENCH_MUTE,          // Permanently silenced.
    ENCH_BLIND,         // Permanently blind (everything is invisible).
    ENCH_DUMB,          // Permanently dumb (as in, 'struck dumb').
    ENCH_MAD,           // Permanently mad.
    ENCH_SILVER_CORONA, // Zin's silver light.
    ENCH_RECITE_TIMER,  // Was recited against.
    // Update enchantment names in monster.cc when adding or removing
    // enchantments.
    NUM_ENCHANTMENTS
};

enum enchant_retval
{
    ERV_FAIL,
    ERV_NEW,
    ERV_INCREASED,
};

enum energy_use_type
{
    EUT_MOVE,
    EUT_SWIM,
    EUT_ATTACK,
    EUT_MISSILE,
    EUT_SPELL,
    EUT_SPECIAL,
    EUT_ITEM,
    EUT_PICKUP,
};

enum equipment_type
{
    EQ_NONE = -1,

    EQ_WEAPON,
    EQ_CLOAK,
    EQ_HELMET,
    EQ_GLOVES,
    EQ_BOOTS,
    EQ_SHIELD,
    EQ_BODY_ARMOUR,
    EQ_LEFT_RING,
    EQ_RIGHT_RING,
    EQ_AMULET,
    NUM_EQUIP,

    EQ_MIN_ARMOUR = EQ_CLOAK,
    EQ_MAX_ARMOUR = EQ_BODY_ARMOUR,
    EQ_MAX_WORN   = EQ_AMULET,
    // these aren't actual equipment slots, they're categories for functions
    EQ_STAFF            = 100,         // weapon with base_type OBJ_STAVES
    EQ_RINGS,                          // check both rings
    EQ_RINGS_PLUS,                     // check both rings and sum plus
    EQ_RINGS_PLUS2,                    // check both rings and sum plus2
    EQ_ALL_ARMOUR,                     // check all armour types
};

enum feature_flag_type
{
    FFT_NONE          = 0,
    FFT_NOTABLE       = 0x1,           // should be noted for dungeon overview
    FFT_EXAMINE_HINT  = 0x2,           // could get an "examine-this" hint.
};

enum flush_reason_type
{
    FLUSH_ON_FAILURE,                  // spell/ability failed to cast
    FLUSH_BEFORE_COMMAND,              // flush before getting a command
    FLUSH_ON_MESSAGE,                  // flush when printing a message
    FLUSH_ON_WARNING_MESSAGE,          // flush on MSGCH_WARN messages
    FLUSH_ON_DANGER_MESSAGE,           // flush on MSGCH_DANGER messages
    FLUSH_ON_PROMPT,                   // flush on MSGCH_PROMPT messages
    FLUSH_ON_UNSAFE_YES_OR_NO_PROMPT,  // flush when !safe set to yesno()
    FLUSH_LUA,                         // flush when Lua wants to flush
    FLUSH_KEY_REPLAY_CANCEL,           // flush when key replay is cancelled
    FLUSH_ABORT_MACRO,                 // something wrong with macro being
                                       // processed, so stop it
    FLUSH_REPLAY_SETUP_FAILURE,        // setup for key replay failed
    FLUSH_REPEAT_SETUP_DONE,           // command repeat done manipulating
                                       // the macro buffer
    NUM_FLUSH_REASONS
};

// The order of this enum must match the order of DNGN_ALTAR_FOO.
enum god_type
{
    GOD_NO_GOD,
    GOD_ZIN,
    GOD_SHINING_ONE,
    GOD_KIKUBAAQUDGHA,
    GOD_YREDELEMNUL,
    GOD_XOM,
    GOD_VEHUMET,
    GOD_OKAWARU,
    GOD_MAKHLEB,
    GOD_SIF_MUNA,
    GOD_TROG,
    GOD_NEMELEX_XOBEH,
    GOD_ELYVILON,
    GOD_LUGONU,
    GOD_BEOGH,
    GOD_JIYVA,
    GOD_FEDHAS,
    GOD_CHEIBRIADOS,
    GOD_ASHENZARI,
    NUM_GODS,                          // always after last god

    GOD_RANDOM = 100,
    GOD_NAMELESS = 101,                // for monsters with non-player gods
    GOD_VIABLE = 102,
};

enum holy_word_source_type
{
    HOLY_WORD_GENERIC     = -1,
    HOLY_WORD_SCROLL      = -2,
    HOLY_WORD_SPELL       = -3,  // SPELL_HOLY_WORD
    HOLY_WORD_ZIN         = -4,  // Zin effect
    HOLY_WORD_TSO         = -5,  // TSO effect
};

enum hunger_state                  // you.hunger_state
{
    HS_STARVING,
    HS_NEAR_STARVING,
    HS_VERY_HUNGRY,
    HS_HUNGRY,
    HS_SATIATED,                       // "not hungry" state
    HS_FULL,
    HS_VERY_FULL,
    HS_ENGORGED,
};

enum immolation_source_type
{
    IMMOLATION_GENERIC = -1,
    IMMOLATION_SCROLL  = -2,
    IMMOLATION_SPELL   = -3, // effect when fixing fire brand
    IMMOLATION_TOME    = -4, // exploding Tome of Destruction
};

enum item_status_flag_type  // per item flags: ie. ident status, cursed status
{
    ISFLAG_KNOW_CURSE        = 0x00000001,  // curse status
    ISFLAG_KNOW_TYPE         = 0x00000002,  // artefact name, sub/special types
    ISFLAG_KNOW_PLUSES       = 0x00000004,  // to hit/to dam/to AC/charges
    ISFLAG_KNOW_PROPERTIES   = 0x00000008,  // know special artefact properties
    ISFLAG_IDENT_MASK        = 0x0000000F,  // mask of all id related flags

    // these three masks are of the minimal flags set upon using equipment:
    ISFLAG_EQ_WEAPON_MASK    = 0x0000000B,  // mask of flags for weapon equip
    ISFLAG_EQ_ARMOUR_MASK    = 0x0000000F,  // mask of flags for armour equip
    ISFLAG_EQ_JEWELLERY_MASK = 0x0000000F,  // mask of flags for known jewellery

    ISFLAG_CURSED            = 0x00000100,  // cursed
    ISFLAG_BLESSED_WEAPON    = 0x00000200,  // personalized TSO's gift
    ISFLAG_SEEN_CURSED       = 0x00000400,  // was seen being cursed
    ISFLAG_RESERVED_3        = 0x00000800,  // reserved

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

    ISFLAG_NO_RACE           = 0x00000000,  // used for clearing these flags
    ISFLAG_ORCISH            = 0x01000000,  // low quality items
    ISFLAG_DWARVEN           = 0x02000000,  // strong and robust items
    ISFLAG_ELVEN             = 0x04000000,  // light and accurate items
    ISFLAG_RACIAL_MASK       = 0x07000000,  // mask of racial equipment types

    ISFLAG_NOTED_ID          = 0x08000000,
    ISFLAG_NOTED_GET         = 0x10000000,

    ISFLAG_BEEN_IN_INV       = 0x20000000,  // Item has been in inventory
    ISFLAG_SUMMONED          = 0x40000000,  // Item generated on a summon
    ISFLAG_DROPPED_BY_ALLY   = 0x80000000,  // Item was dropped by an ally
};

enum item_type_id_state_type
{
    ID_UNKNOWN_TYPE = 0,
    ID_MON_TRIED_TYPE,
    ID_TRIED_TYPE,
    ID_TRIED_ITEM_TYPE,
    ID_KNOWN_TYPE,
};

enum job_type
{
    JOB_FIGHTER,
    JOB_WIZARD,
    JOB_PRIEST,
    JOB_GLADIATOR,
    JOB_NECROMANCER,
    JOB_PALADIN,
    JOB_ASSASSIN,
    JOB_BERSERKER,
    JOB_HUNTER,
    JOB_CONJURER,
    JOB_ENCHANTER,
    JOB_FIRE_ELEMENTALIST,
    JOB_ICE_ELEMENTALIST,
    JOB_SUMMONER,
    JOB_AIR_ELEMENTALIST,
    JOB_EARTH_ELEMENTALIST,
    JOB_CRUSADER,
    JOB_VENOM_MAGE,
    JOB_CHAOS_KNIGHT,
    JOB_TRANSMUTER,
    JOB_HEALER,
    JOB_REAVER,
    JOB_STALKER,
    JOB_MONK,
    JOB_WARPER,
    JOB_WANDERER,
    JOB_ARTIFICER,                     //   Greenberg/Bane
    JOB_ARCANE_MARKSMAN,
    NUM_JOBS,                          // always after the last job

    JOB_UNKNOWN = 100,
    JOB_RANDOM  = 101,
    JOB_VIABLE  = 102,
};

enum KeymapContext
{
    KMC_DEFAULT,         // For no-arg getchm(), must be zero.
    KMC_LEVELMAP,        // When in the 'X' level map
    KMC_TARGETING,       // Only during 'x' and other targeting modes
    KMC_CONFIRM,         // When being asked y/n/q questions
    KMC_MENU,            // For menus
#ifdef USE_TILE
    KMC_DOLL,            // For the tiles doll menu editing screen
#endif

    KMC_CONTEXT_COUNT,   // Must always be the last real context

    KMC_NONE
};

// This order is *critical*. Don't mess with it (see mon_enchant)
enum kill_category
{
    KC_YOU,
    KC_FRIENDLY,
    KC_OTHER,
    KC_NCATEGORIES
};

enum killer_type                       // monster_die(), thing_thrown
{
    KILL_NONE,                         // no killer
    KILL_YOU,                          // you are the killer
    KILL_MON,                          // no, it was a monster!
    KILL_YOU_MISSILE,                  // in the library, with a dart
    KILL_MON_MISSILE,                  // in the dungeon, with a club
    KILL_YOU_CONF,                     // died while confused as caused by you
    KILL_MISCAST,                      // as a result of a spell miscast
    KILL_MISC,                         // any miscellaneous killing
    KILL_RESET,                        // ???
    KILL_DISMISSED,                    // ???
    KILL_BANISHED,                     // monsters what got banished
    KILL_UNSUMMONED,                   // summoned monsters whose timers ran out
    KILL_TIMEOUT,                      // non-summoned monsters whose times ran out
};

enum flight_type
{
    FL_NONE = 0,
    FL_LEVITATE,                       // doesn't require physical effort
    FL_FLY                             // wings, etc... paralysis == fall
};

enum level_area_type                   // you.level_type
{
    LEVEL_DUNGEON,
    LEVEL_LABYRINTH,
    LEVEL_ABYSS,
    LEVEL_PANDEMONIUM,
    LEVEL_PORTAL_VAULT,

    NUM_LEVEL_AREA_TYPES
};

// Reasons for entering the Abyss.
enum entry_cause_type
{
    EC_UNKNOWN,
    EC_SELF_EXPLICIT,
    EC_SELF_RISKY,     // i.e., wielding an id'd distorion weapon
    EC_SELF_ACCIDENT,  // i.e., wielding an un-id'd distortion weapon
    EC_MISCAST,
    EC_GOD_RETRIBUTION,
    EC_GOD_ACT,        // Xom sending the player somewhere for amusement.
    EC_MONSTER,
    EC_TRAP,          // Zot traps
    EC_ENVIRONMENT,   // Hell effects.
    NUM_ENTRY_CAUSE_TYPES
};

// Can't change this order without breaking saves.
enum map_marker_type
{
    MAT_FEATURE,              // Stock marker.
    MAT_LUA_MARKER,
    MAT_CORRUPTION_NEXUS,
    MAT_WIZ_PROPS,
    MAT_TOMB,
    MAT_MALIGN,
    NUM_MAP_MARKER_TYPES,
    MAT_ANY,
};

enum map_feature
{
    MF_UNSEEN,
    MF_FLOOR,
    MF_WALL,
    MF_MAP_FLOOR,
    MF_MAP_WALL,
    MF_DOOR,
    MF_ITEM,
    MF_MONS_FRIENDLY,
    MF_MONS_PEACEFUL,
    MF_MONS_NEUTRAL,
    MF_MONS_HOSTILE,
    MF_MONS_NO_EXP,
    MF_STAIR_UP,
    MF_STAIR_DOWN,
    MF_STAIR_BRANCH,
    MF_FEATURE,
    MF_WATER,
    MF_LAVA,
    MF_TRAP,
    MF_EXCL_ROOT,
    MF_EXCL,
    MF_PLAYER,
    MF_MAX,

    MF_SKIP,
};

enum menu_type
{
    MT_ANY = -1,

    MT_INVLIST,                        // List inventory
    MT_DROP,
    MT_PICKUP,
    MT_KNOW,
};

enum mon_holy_type
{
    MH_HOLY,
    MH_NATURAL,
    MH_UNDEAD,
    MH_DEMONIC,
    MH_NONLIVING, // golems and other constructs
    MH_PLANT,
};

enum targ_mode_type
{
    TARG_ANY,
    TARG_ENEMY,  // hostile + neutral
    TARG_FRIEND,
    TARG_HOSTILE,
    TARG_HOSTILE_SUBMERGED, // Target hostiles including submerged ones
    TARG_EVOLVABLE_PLANTS,  // Targeting mode for Fedhas' evolution
    TARG_NUM_MODES
};

// NOTE: Changing this order will break saves!
enum monster_type                      // (int) menv[].type
{
    MONS_PROGRAM_BUG,

    MONS_GIANT_NEWT,
    MONS_GIANT_GECKO,
    MONS_IGUANA,
    MONS_GILA_MONSTER,
    MONS_KOMODO_DRAGON,
    MONS_SMALL_SNAKE,
    MONS_SNAKE,
    MONS_WATER_MOCCASIN,
    MONS_VIPER,
    MONS_BLACK_MAMBA,
    MONS_ANACONDA,
    MONS_SEA_SNAKE,
    MONS_RAT,
    MONS_GREY_RAT,
    MONS_GREEN_RAT,
    MONS_ORANGE_RAT,
    MONS_LABORATORY_RAT,
    MONS_QUOKKA,         // Quokka are a type of wallaby, returned -- bwr 382
    MONS_PORCUPINE,
    MONS_MEGABAT,
    MONS_JACKAL,
    MONS_HOUND,
    MONS_WAR_DOG,
    MONS_WOLF,
    MONS_WARG,
    MONS_HELL_HOUND,
    MONS_HOG,
    MONS_HELL_HOG,
    MONS_GIANT_SLUG,
    MONS_AGATE_SNAIL,
    MONS_ELEPHANT_SLUG,
    MONS_GIANT_LEECH,
    MONS_BABY_ALLIGATOR,
    MONS_ALLIGATOR,
    MONS_CROCODILE,
    MONS_HYDRA,
    MONS_SHEEP,
    MONS_YAK,
    MONS_DEATH_YAK,
    MONS_ELEPHANT,
    MONS_DIRE_ELEPHANT,
    MONS_HELLEPHANT,
    MONS_MANTICORE,
    MONS_HIPPOGRIFF,
    MONS_GRIFFON,
    MONS_GIANT_FROG,
    MONS_GIANT_TOAD,
    MONS_SPINY_FROG,
    MONS_BLINK_FROG,
    MONS_BEAR,
    MONS_GRIZZLY_BEAR,
    MONS_POLAR_BEAR,
    MONS_BLACK_BEAR,
    MONS_WORM,
    MONS_BRAIN_WORM,
    MONS_ROCK_WORM,
    MONS_SPINY_WORM,
    MONS_WYVERN,
    MONS_LINDWURM,
    MONS_FIRE_DRAKE,
    MONS_SWAMP_DRAKE,
    MONS_DEATH_DRAKE,
    MONS_STEAM_DRAGON,
    MONS_MOTTLED_DRAGON,
    MONS_SWAMP_DRAGON,
    MONS_DRAGON,
    MONS_ICE_DRAGON,
    MONS_SHADOW_DRAGON,
    MONS_STORM_DRAGON,
    MONS_BONE_DRAGON,
    MONS_QUICKSILVER_DRAGON,
    MONS_IRON_DRAGON,
    MONS_GOLDEN_DRAGON,
    MONS_PEARL_DRAGON,

    MONS_OOZE,
    MONS_JELLY,
    MONS_BROWN_OOZE,
    MONS_GIANT_AMOEBA,
    MONS_AZURE_JELLY,
    MONS_DEATH_OOZE,
    MONS_ACID_BLOB,
    MONS_SLIME_CREATURE,
    MONS_PULSATING_LUMP,
    MONS_GIANT_EYEBALL,
    MONS_EYE_OF_DRAINING,
    MONS_SHINING_EYE,
    MONS_EYE_OF_DEVASTATION,
    MONS_GREAT_ORB_OF_EYES,
    MONS_GIANT_ORANGE_BRAIN,

    MONS_DANCING_WEAPON,
    MONS_HARPY,
    MONS_RAVEN,
    MONS_FIRE_CRAB,
    MONS_HOMUNCULUS,
    MONS_SOUPLING,

    MONS_BUTTERFLY,
    MONS_ANT_LARVA,
    MONS_WORKER_ANT,
    MONS_SOLDIER_ANT,
    MONS_QUEEN_ANT,
    MONS_KILLER_BEE_LARVA,
    MONS_KILLER_BEE,
    MONS_QUEEN_BEE,
    MONS_VAMPIRE_MOSQUITO,
#if TAG_MAJOR_VERSION == 32
    MONS_GIANT_BLOWFLY,
#endif
    MONS_BUMBLEBEE,
    MONS_YELLOW_WASP,
    MONS_RED_WASP,
    MONS_GOLIATH_BEETLE,
    MONS_BORING_BEETLE,
    MONS_BOULDER_BEETLE,
    MONS_GIANT_COCKROACH,
    MONS_GIANT_CENTIPEDE,
    MONS_GIANT_MITE,
    MONS_SPIDER,
    MONS_WOLF_SPIDER,
    MONS_TRAPDOOR_SPIDER,
    MONS_JUMPING_SPIDER,
    MONS_TARANTELLA,
    MONS_REDBACK,
    MONS_SCORPION,
    MONS_EMPEROR_SCORPION,
    MONS_GHOST_MOTH,
    MONS_MOTH_OF_WRATH,
    MONS_DEMONIC_CRAWLER,
    MONS_SNAPPING_TURTLE,
    MONS_ALLIGATOR_SNAPPING_TURTLE,
    MONS_GNOME,                 // single vault
    MONS_HALFLING,              // recolouring + single vault.
    MONS_FELID,                 // recolouring + single vault.  Miaow!
    MONS_VAMPIRE_BAT,           // for recolouring
    MONS_DEMIGOD,               // recolouring + single vault
    MONS_DEMONSPAWN,            // recolouring + single vault... but there are FRs
    MONS_GARGOYLE,
    MONS_METAL_GARGOYLE,
    MONS_MOLTEN_GARGOYLE,
    MONS_UGLY_THING,
    MONS_VERY_UGLY_THING,
    MONS_ICE_BEAST,
    MONS_SKY_BEAST,
    MONS_SPHINX,
    MONS_ORB_GUARDIAN,

    MONS_CLAY_GOLEM,
    MONS_WOOD_GOLEM,
    MONS_STONE_GOLEM,
    MONS_IRON_GOLEM,
    MONS_CRYSTAL_GOLEM,
    MONS_TOENAIL_GOLEM,
    MONS_ELECTRIC_GOLEM, // replacing the guardian robot -- bwr
    MONS_ORB_OF_FIRE,    // Swords renamed to fit -- bwr
    MONS_EARTH_ELEMENTAL,
    MONS_FIRE_ELEMENTAL,
    MONS_AIR_ELEMENTAL,
    MONS_TWISTER,        // air miscasts
    MONS_GOLDEN_EYE,
    MONS_FIRE_VORTEX,
    MONS_SPATIAL_VORTEX,
    MONS_INSUBSTANTIAL_WISP,
    MONS_VAPOUR,

    // Mimics:
    MONS_GOLD_MIMIC,
    MONS_WEAPON_MIMIC,
    MONS_ARMOUR_MIMIC,
    MONS_SCROLL_MIMIC,
    MONS_POTION_MIMIC,
    MONS_DOOR_MIMIC,
    MONS_PORTAL_MIMIC,
    MONS_TRAP_MIMIC,
    MONS_STAIR_MIMIC,
    MONS_SHOP_MIMIC,
    MONS_FOUNTAIN_MIMIC,

    // Plants:
    MONS_TOADSTOOL,
    MONS_FUNGUS,
    MONS_WANDERING_MUSHROOM,
    MONS_PLANT,
    MONS_OKLOB_SAPLING,
    MONS_OKLOB_PLANT,
    MONS_BUSH,
    MONS_BURNING_BUSH,
    MONS_GIANT_SPORE,
    MONS_BALLISTOMYCETE,
    MONS_HYPERACTIVE_BALLISTOMYCETE,

    MONS_GOBLIN,
    MONS_HOBGOBLIN,
    MONS_GNOLL,
    MONS_BOGGART,
    MONS_KOBOLD,
    MONS_BIG_KOBOLD,
    MONS_KOBOLD_DEMONOLOGIST,
    MONS_ORC,
    MONS_ORC_WARRIOR,
    MONS_ORC_PRIEST,
    MONS_ORC_HIGH_PRIEST,
    MONS_ORC_WIZARD,
    MONS_ORC_KNIGHT,
    MONS_ORC_SORCERER,
    MONS_ORC_WARLORD,
    MONS_DWARF,
    MONS_DEEP_DWARF,
    MONS_DEEP_DWARF_SCION,
    MONS_DEEP_DWARF_ARTIFICER,
    MONS_DEEP_DWARF_NECROMANCER,
    MONS_DEEP_DWARF_BERSERKER,
    MONS_DEEP_DWARF_DEATH_KNIGHT,
    MONS_UNBORN_DEEP_DWARF,
    MONS_ELF,
    MONS_DEEP_ELF_SOLDIER,
    MONS_DEEP_ELF_FIGHTER,
    MONS_DEEP_ELF_KNIGHT,
    MONS_DEEP_ELF_MAGE,
    MONS_DEEP_ELF_SUMMONER,
    MONS_DEEP_ELF_CONJURER,
    MONS_DEEP_ELF_PRIEST,
    MONS_DEEP_ELF_HIGH_PRIEST,
    MONS_DEEP_ELF_DEMONOLOGIST,
    MONS_DEEP_ELF_ANNIHILATOR,
    MONS_DEEP_ELF_SORCERER,
    MONS_DEEP_ELF_DEATH_MAGE,
    MONS_DEEP_ELF_BLADEMASTER,
    MONS_DEEP_ELF_MASTER_ARCHER,
    MONS_SPRIGGAN,
    MONS_SPRIGGAN_DRUID,
    MONS_SPRIGGAN_ASSASSIN,
    MONS_SPRIGGAN_RIDER,
    MONS_SPRIGGAN_BERSERKER,
    MONS_SPRIGGAN_DEFENDER,
    MONS_FIREFLY,
    MONS_KENKU,
    MONS_MINOTAUR,
    MONS_NAGA,
    MONS_NAGA_WARRIOR,
    MONS_NAGA_MAGE,
    MONS_GREATER_NAGA,
    MONS_GUARDIAN_SERPENT,
    MONS_MERFOLK,
    MONS_MERMAID,
    MONS_SIREN,
    MONS_MERFOLK_IMPALER,
    MONS_MERFOLK_AQUAMANCER,
    MONS_MERFOLK_JAVELINEER,
    MONS_CENTAUR,
    MONS_CENTAUR_WARRIOR,
    MONS_YAKTAUR,
    MONS_YAKTAUR_CAPTAIN,
    MONS_OGRE,
    MONS_TWO_HEADED_OGRE,
    MONS_OGRE_MAGE,
    MONS_TROLL,
    MONS_ROCK_TROLL,
    MONS_IRON_TROLL,
    MONS_DEEP_TROLL,
    MONS_HILL_GIANT,
    MONS_CYCLOPS,
    MONS_ETTIN,
    MONS_STONE_GIANT,
    MONS_FIRE_GIANT,
    MONS_FROST_GIANT,
    MONS_TITAN,
    MONS_HUMAN,
    MONS_SLAVE,
    MONS_HELL_KNIGHT,
    MONS_NECROMANCER,
    MONS_WIZARD,
    MONS_VAULT_GUARD,
    MONS_KILLER_KLOWN,
    MONS_SHAPESHIFTER,
    MONS_GLOWING_SHAPESHIFTER,

    // Draconians:
    MONS_DRACONIAN,
    MONS_FIRST_DRACONIAN = MONS_DRACONIAN,

    // If adding more drac colours, sync up colour names in
    // mon-util.cc.
    MONS_BLACK_DRACONIAN,               // Should always be first colour.
    MONS_MOTTLED_DRACONIAN,
    MONS_YELLOW_DRACONIAN,
    MONS_GREEN_DRACONIAN,
    MONS_PURPLE_DRACONIAN,
    MONS_RED_DRACONIAN,
    MONS_WHITE_DRACONIAN,
    MONS_GREY_DRACONIAN,
    MONS_PALE_DRACONIAN,                //  Should always be last colour.

    // Sync up with mon-place.cc's draconian selection if adding more.
    MONS_DRACONIAN_CALLER,
    MONS_DRACONIAN_MONK,
    MONS_DRACONIAN_ZEALOT,
    MONS_DRACONIAN_SHIFTER,
    MONS_DRACONIAN_ANNIHILATOR,
    MONS_DRACONIAN_KNIGHT,
    MONS_DRACONIAN_SCORCHER,

    MONS_LAST_DRACONIAN = MONS_DRACONIAN_SCORCHER,

    // Lava monsters:
    MONS_LAVA_WORM,
    MONS_LAVA_FISH,
    MONS_LAVA_SNAKE,
    MONS_SALAMANDER,
    // Water monsters:
    MONS_BIG_FISH,
    MONS_GIANT_GOLDFISH,
    MONS_ELECTRIC_EEL,
    MONS_JELLYFISH,
    MONS_WATER_ELEMENTAL,
    MONS_SWAMP_WORM,
    MONS_SHARK,
    MONS_KRAKEN,
    MONS_KRAKEN_TENTACLE,
    MONS_KRAKEN_TENTACLE_SEGMENT,

    // Statuary
    MONS_ORANGE_STATUE,
    MONS_SILVER_STATUE,
    MONS_ICE_STATUE,
    MONS_STATUE,
    MONS_TRAINING_DUMMY,

    // Demons:
    MONS_IMP,
    MONS_QUASIT,
    MONS_WHITE_IMP,
    MONS_LEMURE,
    MONS_UFETUBUS,
    MONS_IRON_IMP,
    MONS_MIDGE,
    MONS_SHADOW_IMP,
    MONS_RED_DEVIL,
    MONS_ROTTING_DEVIL,
    MONS_HELLWING,
    MONS_SIXFIRHY,
    MONS_NEQOXEC,
    MONS_ORANGE_DEMON,
    MONS_SMOKE_DEMON,
    MONS_YNOXINUL,
    MONS_CHAOS_SPAWN,
    MONS_HELLION,
    MONS_LOROCYPROCA,
    MONS_TORMENTOR,
    MONS_REAPER,
    MONS_SOUL_EATER,
    MONS_HAIRY_DEVIL,
    MONS_ICE_DEVIL,
    MONS_BLUE_DEVIL,
    MONS_BEAST,
    MONS_IRON_DEVIL,
    MONS_EXECUTIONER,
    MONS_GREEN_DEATH,
    MONS_BLUE_DEATH,
    MONS_BALRUG,
    MONS_CACODEMON,
    MONS_SUN_DEMON,
    MONS_SHADOW_DEMON,
    MONS_PIT_FIEND,
    MONS_FIEND,
    MONS_ICE_FIEND,
    MONS_SHADOW_FIEND,
    MONS_PANDEMONIUM_DEMON,
    MONS_EFREET,
    MONS_RAKSHASA,
    MONS_RAKSHASA_FAKE,
    MONS_UNSEEN_HORROR,
    MONS_TENTACLED_STARSPAWN,
    MONS_LURKING_HORROR,
    MONS_THRASHING_HORROR,
    MONS_STARCURSED_MASS,
    MONS_ANCIENT_ZYME,
    MONS_WRETCHED_STAR,
    MONS_ELDRITCH_TENTACLE,
    MONS_ELDRITCH_TENTACLE_SEGMENT,
    MONS_TENTACLED_MONSTROSITY,
    MONS_ABOMINATION_SMALL,
    MONS_ABOMINATION_LARGE,

    // Undead:
    MONS_ROTTING_HULK,
    MONS_NECROPHAGE,
    MONS_GHOUL,
    MONS_FLAMING_CORPSE,
    MONS_MUMMY,
    MONS_BOG_MUMMY,
    MONS_GUARDIAN_MUMMY,
    MONS_GREATER_MUMMY,
    MONS_MUMMY_PRIEST,
    MONS_VAMPIRE,
    MONS_VAMPIRE_KNIGHT,
    MONS_VAMPIRE_MAGE,
    MONS_GHOST,                 // common genus for monster and player ghosts
    MONS_PHANTOM,
    MONS_SHADOW,
    MONS_HUNGRY_GHOST,
    MONS_FLAYED_GHOST,
    MONS_WIGHT,
    MONS_WRAITH,
    MONS_FREEZING_WRAITH,
    MONS_SHADOW_WRAITH,
    MONS_SILENT_SPECTRE,
    MONS_EIDOLON,
    MONS_FLYING_SKULL,
    MONS_SKELETAL_WARRIOR,
    MONS_PHANTASMAL_WARRIOR,
    MONS_LICH,
    MONS_ANCIENT_LICH,
    MONS_DEATH_COB,
    MONS_CURSE_TOE,
    MONS_CURSE_SKULL,
    MONS_SKELETON_SMALL,
    MONS_SKELETON_LARGE,
    MONS_ZOMBIE_SMALL,
    MONS_ZOMBIE_LARGE,
    MONS_SPECTRAL_THING,
    MONS_SIMULACRUM_SMALL,
    MONS_SIMULACRUM_LARGE,

    // Holies:
    MONS_ANGEL,
    MONS_DAEVA,
    MONS_CHERUB,
    MONS_PHOENIX,
    MONS_SILVER_STAR,
    MONS_BLESSED_TOE,
    MONS_SHEDU,
    MONS_OPHAN,
    MONS_SPIRIT,
    MONS_PALADIN,
    MONS_APIS,

    // Fixed uniques:
    MONS_GERYON,
    MONS_DISPATER,
    MONS_ASMODEUS,
    MONS_ANTAEUS,
    MONS_ERESHKIGAL,
    MONS_ROYAL_JELLY,
    MONS_THE_ENCHANTRESS,
    MONS_MNOLEG,
    MONS_LOM_LOBON,
    MONS_CEREBOV,
    MONS_GLOORX_VLOQ,
    MONS_SERPENT_OF_HELL,
    // Random uniques:
    MONS_IJYB,
    MONS_JESSICA,
    MONS_SIGMUND,
    MONS_TERENCE,
    MONS_BLORK_THE_ORC,
    MONS_EDMUND,
    MONS_PSYCHE,
    MONS_EROLCHA,
    MONS_DONALD,
    MONS_URUG,
    MONS_JOSEPH,
    MONS_SNORG, // was Anita - Snorg is correct 16jan2000 {dlb}
    MONS_ERICA,
    MONS_JOSEPHINE,
    MONS_HAROLD,
    MONS_JOZEF,
    MONS_AGNES,
    MONS_MAUD,
    MONS_LOUISE,
    MONS_FRANCES,
    MONS_RUPERT,
    MONS_WIGLAF,
    MONS_XTAHUA,
    MONS_NORRIS,
    MONS_FREDERICK,
    MONS_MARGERY,
    MONS_BORIS,
    MONS_POLYPHEMUS,
    MONS_MURRAY,
    MONS_TIAMAT,
    MONS_ROXANNE,
    MONS_SONJA,
    MONS_EUSTACHIO,
    MONS_AZRAEL,
    MONS_ILSUIW,
    MONS_PRINCE_RIBBIT,
    MONS_NERGALLE,
    MONS_SAINT_ROKA,
    MONS_NESSOS,
    MONS_LERNAEAN_HYDRA,
    MONS_DISSOLUTION,
    MONS_KIRKE,
    MONS_GRUM,
    MONS_PURGY,
    MONS_MENKAURE,
    MONS_DUVESSA,
    MONS_DOWAN,
    MONS_GASTRONOK,
    MONS_MAURICE,
    MONS_KHUFU,
    MONS_NIKOLA,
    MONS_AIZUL,
    MONS_PIKEL,
    MONS_CRAZY_YIUF,
    MONS_MENNAS,
    MONS_MARA,
    MONS_MARA_FAKE,
    MONS_GRINDER,
    MONS_JORY,
    // Sprint uniques:
    MONS_CHUCK,
    MONS_IRON_GIANT,
    MONS_NELLIE,
    MONS_IRON_ELEMENTAL,

    // Specials:
    MONS_PLAYER_ILLUSION,
    MONS_PLAYER_GHOST,
    MONS_BALL_LIGHTNING,
    MONS_ORB_OF_DESTRUCTION,    // a projectile, not a real mon
    MONS_HELL_DEMON,
    MONS_MERGED_SLIME_CREATURE, // used only for recoloring
    MONS_SENSED,                // dummy monster for unspecified sensed mons
    MONS_PLAYER,                // a certain ugly creature
    MONS_TEST_SPAWNER,

    // Add new monsters here:
    MONS_SENSED_TRIVIAL,
    MONS_SENSED_EASY,
    MONS_SENSED_TOUGH,
    MONS_SENSED_NASTY,
    MONS_SALT_PILLAR,
    MONS_TERPSICHORE,

    NUM_MONSTERS,                      // used for polymorph

    // MONS_NO_MONSTER can get put in savefiles, so it shouldn't change
    // when NUM_MONSTERS increases.
    MONS_NO_MONSTER = 1000,

    RANDOM_MONSTER = 2000, // used to distinguish between a random monster and using program bugs for error trapping {dlb}
    RANDOM_MOBILE_MONSTER, // used for monster generation (shadow creatures)

    // A random draconian, either base coloured drac or specialised.
    RANDOM_DRACONIAN,

    // Any random base draconian colour.
    RANDOM_BASE_DRACONIAN,

    // Any random specialised draconian, such as a draconian knight.
    RANDOM_NONBASE_DRACONIAN,

    WANDERING_MONSTER = 3500, // only used in monster placement routines - forced limit checks {dlb}
};

enum beh_type
{
    BEH_SLEEP,
    BEH_WANDER,
    BEH_SEEK,
    BEH_FLEE,
    BEH_CORNERED,
    BEH_PANIC,                         //  like flee but without running away
    BEH_LURK,                          //  stay still until discovered or
                                       //  enemy close by
    NUM_BEHAVIOURS,                    //  max # of legal states
    BEH_CHARMED,                       //  hostile-but-charmed; creation only
    BEH_FRIENDLY,                      //  used during creation only
    BEH_GOOD_NEUTRAL,                  //  creation only
    BEH_STRICT_NEUTRAL,
    BEH_NEUTRAL,                       //  creation only
    BEH_HOSTILE,                       //  creation only
    BEH_GUARD,                         //  creation only - monster is guard
    BEH_COPY,                          //  creation only - copy from summoner
};

enum mon_attitude_type
{
    ATT_HOSTILE,                       // 0, default in most cases
    ATT_NEUTRAL,                       // neutral
    ATT_STRICT_NEUTRAL,                // neutral, won't attack player. Used by Jiyva.
    ATT_GOOD_NEUTRAL,                  // neutral, but won't attack friendlies
    ATT_FRIENDLY,                      // created friendly (or tamed?)
};

// Adding slots breaks saves. YHBW.
enum mon_inv_type           // (int) menv[].inv[]
{
    MSLOT_WEAPON,           // Primary weapon (melee)
    MSLOT_ALT_WEAPON,       // Alternate weapon, ranged or second melee weapon
                            // for monsters that can use two weapons.
    MSLOT_MISSILE,
    MSLOT_ALT_MISSILE,
    MSLOT_ARMOUR,
    MSLOT_SHIELD,

    // [ds] Last monster gear slot that the player can observe by examining
    // the monster; i.e. the last slot that goes into monster_info.
    MSLOT_LAST_VISIBLE_SLOT = MSLOT_SHIELD,

    MSLOT_MISCELLANY,
    MSLOT_POTION,
    MSLOT_WAND,
    MSLOT_SCROLL,
    MSLOT_GOLD,
    NUM_MONSTER_SLOTS
};

// XXX: These still need to be applied in mon-data.h
// The order doesn't matter, these don't get saved.
enum mon_spellbook_type
{
    MST_ORC_WIZARD_I,
    MST_ORC_WIZARD_II,
    MST_ORC_WIZARD_III,
    MST_DEEP_DWARF_NECROMANCER,
    MST_UNBORN_DEEP_DWARF,
    MST_BK_TROG,
    MST_BK_YREDELEMNUL,
    MST_BK_OKAWARU,
    MST_GUARDIAN_SERPENT,
    MST_LICH_I,
    MST_LICH_II,
    MST_LICH_III,
    MST_LICH_IV,
    MST_HELLION,
    MST_VAMPIRE,
    MST_VAMPIRE_KNIGHT,
    MST_VAMPIRE_MAGE,
    MST_EFREET,
    MST_KILLER_KLOWN,
    MST_BRAIN_WORM,
    MST_GIANT_ORANGE_BRAIN,
    MST_RAKSHASA,
    MST_GREAT_ORB_OF_EYES,
    MST_KRAKEN,
    MST_ORC_SORCERER,
    MST_STEAM_DRAGON,
    MST_HELL_KNIGHT_I,
    MST_HELL_KNIGHT_II,
    MST_NECROMANCER_I,
    MST_NECROMANCER_II,
    MST_WIZARD_I,
    MST_WIZARD_II,
    MST_WIZARD_III,
    MST_WIZARD_IV,
    MST_WIZARD_V,
    MST_ORC_PRIEST,
    MST_ORC_HIGH_PRIEST,
    MST_MOTTLED_DRAGON,
    MST_ICE_FIEND,
    MST_SHADOW_FIEND,
    MST_TORMENTOR,
    MST_STORM_DRAGON,
    MST_WHITE_IMP,
    MST_YNOXINUL,
    MST_NEQOXEC,
    MST_HELLWING,
    MST_SMOKE_DEMON,
    MST_CACODEMON,
    MST_GREEN_DEATH,
    MST_BALRUG,
    MST_BLUE_DEATH,
    MST_TITAN,
    MST_GOLDEN_DRAGON,
    MST_DEEP_ELF_SUMMONER,
    MST_DEEP_ELF_CONJURER_I,
    MST_DEEP_ELF_CONJURER_II,
    MST_DEEP_ELF_PRIEST,
    MST_DEEP_ELF_HIGH_PRIEST,
    MST_DEEP_ELF_DEMONOLOGIST,
    MST_DEEP_ELF_ANNIHILATOR,
    MST_DEEP_ELF_SORCERER,
    MST_DEEP_ELF_DEATH_MAGE,
    MST_KOBOLD_DEMONOLOGIST,
    MST_NAGA,
    MST_NAGA_MAGE,
    MST_CURSE_SKULL,
    MST_SHINING_EYE,
    MST_FROST_GIANT,
    MST_ANGEL,
    MST_DAEVA,
    MST_SHADOW_DRAGON,
    MST_SPHINX,
    MST_MUMMY,
    MST_ELECTRIC_GOLEM,
    MST_ORB_OF_FIRE,
    MST_SHADOW_IMP,
    MST_HELL_HOG,
    MST_SWAMP_DRAGON,
    MST_SWAMP_DRAKE,
    MST_BOGGART,
    MST_EYE_OF_DEVASTATION,
    MST_QUICKSILVER_DRAGON,
    MST_IRON_DRAGON,
    MST_SKELETAL_WARRIOR,
    MST_NORRIS,
    MST_DEATH_DRAKE,
    MST_DRAC_SCORCHER, // As Bioster would say.. pig*s
    MST_DRAC_CALLER,
    MST_DRAC_SHIFTER,
    MST_CURSE_TOE,
    MST_ICE_STATUE,
    MST_MERFOLK_AQUAMANCER,
    MST_ALLIGATOR,
    MST_JUMPING_SPIDER,
    MST_CHERUB,
    MST_PHOENIX,
    MST_SILVER_STAR,
    MST_BLESSED_TOE,
    MST_SHEDU,
    MST_OPHAN,
    MST_SPIRIT,
    MST_PALADIN,
    MST_PEARL_DRAGON,
    MST_BOG_MUMMY,
    MST_SPRIGGAN_DRUID,
    MST_TENTACLED_STARSPAWN,
    MST_LURKING_HORROR,
    MST_STARCURSED_MASS,
    MST_ANCIENT_ZYME,
    // unique monsters' "spellbooks"
    MST_RUPERT,
    MST_ROXANNE,
    MST_SONJA,
    MST_EUSTACHIO,
    MST_ILSUIW,
    MST_PRINCE_RIBBIT,
    MST_NESSOS,
    MST_KIRKE,
    MST_MENKAURE,
    MST_DOWAN,
    MST_GERYON,
    MST_DISPATER,
    MST_ASMODEUS,
    MST_ERESHKIGAL,
    MST_ANTAEUS,
    MST_MNOLEG,
    MST_LOM_LOBON,
    MST_CEREBOV,
    MST_GLOORX_VLOQ,
    MST_JESSICA,
    MST_BERSERK_ESCAPE,
    MST_GASTRONOK,
    MST_MAURICE,
    MST_KHUFU,
    MST_NIKOLA,
    MST_DISSOLUTION,
    MST_AIZUL,
    MST_EXECUTIONER,
    MST_HAROLD,
    MST_MARA,
    MST_MARA_FAKE,
    MST_BORIS,
    MST_FREDERICK,
    MST_THE_ENCHANTRESS,
    MST_HELLEPHANT,
    MST_GRINDER,
    MST_IRON_GIANT,
    MST_IRON_ELEMENTAL,
    MST_MENNAS,
    MST_SERPENT_OF_HELL_GEHENNA,
    MST_SERPENT_OF_HELL_COCYTUS,
    MST_SERPENT_OF_HELL_TARTARUS,
    MST_SERPENT_OF_HELL_DIS,
    MST_NERGALLE,
    MST_JORY,
    MST_TERPSICHORE,

    MST_GHOST, // special
    MST_TEST_SPAWNER,
    NUM_MSTYPES,
    MST_NO_SPELLS
};

enum mutation_type
{
    // body slot facets
    MUT_ANTENNAE,       // head
    MUT_BIG_WINGS,
    MUT_BEAK,           // head
    MUT_CLAWS,          // hands
    MUT_FANGS,
    MUT_HOOVES,         // feet
    MUT_HORNS,          // head
    MUT_STINGER,
    MUT_TALONS,         // feet

    // scales
    MUT_DISTORTION_FIELD,
    MUT_ICY_BLUE_SCALES,
    MUT_IRIDESCENT_SCALES,
    MUT_LARGE_BONE_PLATES,
    MUT_MOLTEN_SCALES,
    MUT_ROUGH_BLACK_SCALES,
    MUT_RUGGED_BROWN_SCALES,
    MUT_SLIMY_GREEN_SCALES,
    MUT_THIN_METALLIC_SCALES,
    MUT_THIN_SKELETAL_STRUCTURE,
    MUT_YELLOW_SCALES,

    MUT_ACUTE_VISION,
    MUT_AGILE,
    MUT_BERSERK,
    MUT_BLINK,
    MUT_BLURRY_VISION,
    MUT_BREATHE_FLAMES,
    MUT_BREATHE_POISON,
    MUT_CARNIVOROUS,
    MUT_CLARITY,
    MUT_CLEVER,
    MUT_CLUMSY,
    MUT_COLD_RESISTANCE,
    MUT_CONSERVE_POTIONS,
    MUT_CONSERVE_SCROLLS,
    MUT_DEFORMED,
    MUT_DEMONIC_GUARDIAN,
    MUT_DETERIORATION,
    MUT_DOPEY,
    MUT_HEAT_RESISTANCE,
    MUT_HERBIVOROUS,
    MUT_HURL_HELLFIRE,
    MUT_FAST,
    MUT_FAST_METABOLISM,
    MUT_FLEXIBLE_WEAK,
    MUT_FRAIL,
    MUT_GOURMAND,
    MUT_HIGH_MAGIC,
    MUT_ICEMAIL,
    MUT_LOW_MAGIC,
    MUT_MAGIC_RESISTANCE,
    MUT_MUTATION_RESISTANCE,
    MUT_NEGATIVE_ENERGY_RESISTANCE,
    MUT_NIGHTSTALKER,
    MUT_PASSIVE_FREEZE,
    MUT_PASSIVE_MAPPING,
    MUT_POISON_RESISTANCE,
    MUT_POWERED_BY_DEATH,
#if TAG_MAJOR_VERSION != 32
    MUT_POWERED_BY_PAIN,
#endif
    MUT_REGENERATION,
    MUT_ROBUST,
    MUT_SAPROVOROUS,
    MUT_SCREAM,
    MUT_SHAGGY_FUR,
    MUT_SHOCK_RESISTANCE,
    MUT_SLOW_HEALING,
    MUT_SLOW_METABOLISM,
    MUT_SPINY,
    MUT_SPIT_POISON,
    MUT_STOCHASTIC_TORMENT_RESISTANCE,
    MUT_STRONG,
    MUT_STRONG_STIFF,
    MUT_TELEPORT,
    MUT_TELEPORT_AT_WILL,
    MUT_TELEPORT_CONTROL,
    MUT_THROW_FLAMES,
    MUT_THROW_FROST,
    MUT_TORMENT_RESISTANCE,
    MUT_TOUGH_SKIN,
    MUT_WEAK,
    MUT_SLOW,
    MUT_UNBREATHING,

    // Jiyva-specific mutations
    MUT_ACIDIC_BITE,
    MUT_EYEBALLS,
    MUT_FOOD_JELLY,
    MUT_GELATINOUS_BODY,
    MUT_PSEUDOPODS,
    MUT_TRANSLUCENT_SKIN,

#if TAG_MAJOR_VERSION == 32
    MUT_POWERED_BY_PAIN,
#endif
    NUM_MUTATIONS,

    RANDOM_MUTATION = NUM_MUTATIONS + 1,
    RANDOM_XOM_MUTATION,
    RANDOM_GOOD_MUTATION,
    RANDOM_BAD_MUTATION,
    RANDOM_SLIME_MUTATION,
    RANDOM_NON_SLIME_MUTATION,
};

enum object_class_type                 // mitm[].base_type
{
    OBJ_WEAPONS,
    OBJ_MISSILES,
    OBJ_ARMOUR,
    OBJ_WANDS,
    OBJ_FOOD,
#if TAG_MAJOR_VERSION == 32
    OBJ_UNKNOWN_I, // (use unknown) labeled as books in invent.cc {dlb}
#endif
    OBJ_SCROLLS,
    OBJ_JEWELLERY,
    OBJ_POTIONS,
#if TAG_MAJOR_VERSION == 32
    OBJ_UNKNOWN_II, // (use unknown, stackable) labeled as gems in invent.cc {dlb}
#endif
    OBJ_BOOKS,
    OBJ_STAVES,
    OBJ_ORBS,
    OBJ_MISCELLANY,
    OBJ_CORPSES,
    OBJ_GOLD, // important role as upper limit to chardump::dump_inventory() {dlb}
#if TAG_MAJOR_VERSION == 32
    OBJ_GEMSTONES, // found in itemname.cc, labeled as miscellaneous in invent.cc {dlb}
#endif
    NUM_OBJECT_CLASSES,
    OBJ_UNASSIGNED = 100,
    OBJ_RANDOM,      // used for blanket random sub_type .. see dungeon::items()
    OBJ_DETECTED,    // unknown item; item_info only
};

enum operation_types
{
    OPER_WIELD    = 'w',
    OPER_QUAFF    = 'q',
    OPER_DROP     = 'd',
    OPER_EAT      = 'e',
    OPER_TAKEOFF  = 'T',
    OPER_WEAR     = 'W',
    OPER_PUTON    = 'P',
    OPER_REMOVE   = 'R',
    OPER_READ     = 'r',
    OPER_MEMORISE = 'M',
    OPER_ZAP      = 'Z',
    OPER_EXAMINE  = 'x',
    OPER_FIRE     = 'f',
    OPER_PRAY     = 'p',
    OPER_EVOKE    = 'v',
    OPER_DESTROY  = 'D',
    OPER_QUIVER   = 'Q',
    OPER_ATTACK   = 'a',
    OPER_ANY      = 0,
};

enum orb_type
{
    ORB_ZOT,
};

enum recite_type
{
    RECITE_CHAOTIC,
    RECITE_IMPURE,
    RECITE_HERETIC,
    RECITE_UNHOLY,
    RECITE_ALLY,
    NUM_RECITE_TYPES
};

enum size_part_type
{
    PSIZE_BODY,         // entire body size -- used for EV/size of target
    PSIZE_TORSO,        // torso only (hybrids -- size of parts that use equip)
    PSIZE_PROFILE,      // profile only (for stealth checks)
};

enum potion_type
{
    POT_HEALING,
    POT_HEAL_WOUNDS,
    POT_SPEED,
    POT_MIGHT,
    POT_BRILLIANCE,
    POT_AGILITY,
    POT_GAIN_STRENGTH,
    POT_GAIN_DEXTERITY,
    POT_GAIN_INTELLIGENCE,
    POT_LEVITATION,
    POT_POISON,
    POT_SLOWING,
    POT_PARALYSIS,
    POT_CONFUSION,
    POT_INVISIBILITY,
    POT_PORRIDGE,
    POT_DEGENERATION,
    POT_DECAY,
    POT_WATER,
    POT_EXPERIENCE,
    POT_MAGIC,
    POT_RESTORE_ABILITIES,
    POT_STRONG_POISON,
    POT_BERSERK_RAGE,
    POT_CURE_MUTATION,
    POT_MUTATION,
    POT_RESISTANCE,
    POT_BLOOD,
    POT_BLOOD_COAGULATED,
    POT_FIZZING,
    NUM_POTIONS
};

enum pronoun_type
{
    PRONOUN_CAP,
    PRONOUN_NOCAP,
    PRONOUN_CAP_POSSESSIVE,
    PRONOUN_NOCAP_POSSESSIVE,
    PRONOUN_REFLEXIVE,                  // reflexive is always lowercase
    PRONOUN_OBJECTIVE,                  // objective is always lowercase
};

enum artefact_prop_type
{
    ARTP_BRAND,
    ARTP_AC,
    ARTP_EVASION,
    ARTP_STRENGTH,
    ARTP_INTELLIGENCE,
    ARTP_DEXTERITY,
    ARTP_FIRE,
    ARTP_COLD,
    ARTP_ELECTRICITY,
    ARTP_POISON,
    ARTP_NEGATIVE_ENERGY,
    ARTP_MAGIC,
    ARTP_EYESIGHT,
    ARTP_INVISIBLE,
    ARTP_LEVITATE,
    ARTP_BLINK,
    ARTP_BERSERK,
    ARTP_NOISES,
    ARTP_PREVENT_SPELLCASTING,
    ARTP_CAUSE_TELEPORTATION,
    ARTP_PREVENT_TELEPORTATION,
    ARTP_ANGRY,
    ARTP_METABOLISM,
    ARTP_MUTAGENIC,
    ARTP_ACCURACY,
    ARTP_DAMAGE,
    ARTP_CURSED,
    ARTP_STEALTH,
    ARTP_MAGICAL_POWER,
    ARTP_PONDEROUS,
    ARTP_NUM_PROPERTIES
};

enum score_format_type
{
    SCORE_TERSE,                // one line
    SCORE_REGULAR,              // two lines (name, cause, blank)
    SCORE_VERBOSE,              // everything (dates, times, god, etc.)
};

enum shop_type // (uint8_t) env.sh_type[], item_in_shop(), in_a_shop()
{
    SHOP_WEAPON,
    SHOP_ARMOUR,
    SHOP_WEAPON_ANTIQUE,
    SHOP_ARMOUR_ANTIQUE,
    SHOP_GENERAL_ANTIQUE,
    SHOP_JEWELLERY,
    SHOP_WAND,
    SHOP_BOOK,
    SHOP_FOOD,
    SHOP_DISTILLERY,
    SHOP_SCROLL,
    SHOP_GENERAL,
    NUM_SHOPS, // must remain last 'regular' member {dlb}
    SHOP_UNASSIGNED = 100,
    SHOP_RANDOM,
};

// These are often addressed relative to each other (esp. delta SIZE_MEDIUM).
enum size_type
{
    SIZE_TINY,              // rats/bats
    SIZE_LITTLE,            // spriggans
    SIZE_SMALL,             // halflings/kobolds
    SIZE_MEDIUM,            // humans/elves/dwarves
    SIZE_LARGE,             // trolls/ogres/centaurs/nagas
    SIZE_BIG,               // large quadrupeds
    SIZE_GIANT,             // giants
    SIZE_HUGE,              // dragons
    NUM_SIZE_LEVELS,
    SIZE_CHARACTER,         // transformations that don't change size
};

// [dshaligram] If you add a new skill, update skills2.cc, specifically
// the skills[] array and skill_display_order[]. New skills must go at the
// end of the list or in the unused skill numbers. NEVER rearrange this enum or
// move existing skills to new numbers; save file compatibility depends on this
// order.
enum skill_type
{
    SK_FIGHTING,
    SK_FIRST_SKILL = SK_FIGHTING,
    SK_SHORT_BLADES,
    SK_LONG_BLADES,
    SK_AXES,
    SK_MACES_FLAILS,
    SK_POLEARMS,
    SK_STAVES,
    SK_SLINGS,
    SK_BOWS,
    SK_CROSSBOWS,
    SK_THROWING,
    SK_ARMOUR,
    SK_DODGING,
    SK_STEALTH,
    SK_STABBING,
    SK_SHIELDS,
    SK_TRAPS_DOORS,
    SK_UNARMED_COMBAT,
    SK_LAST_MUNDANE = SK_UNARMED_COMBAT,
    SK_SPELLCASTING,
    SK_CONJURATIONS,
    SK_HEXES,
    SK_CHARMS,
    SK_SUMMONINGS,
    SK_NECROMANCY,
    SK_TRANSLOCATIONS,
    SK_TRANSMUTATIONS,
    SK_FIRE_MAGIC,
    SK_ICE_MAGIC,
    SK_AIR_MAGIC,
    SK_EARTH_MAGIC,
    SK_POISON_MAGIC,
    SK_INVOCATIONS,
    SK_EVOCATIONS,
    SK_LAST_SKILL = SK_EVOCATIONS,
    NUM_SKILLS,                        // must remain last regular member

    SK_BLANK_LINE,                     // used for skill output
    SK_COLUMN_BREAK,                   // used for skill output
    SK_TITLE,                          // used for skill output
    SK_NONE,
};

// order is important on these (see player_speed())
enum speed_type
{
    SPEED_SLOWED,
    SPEED_NORMAL,
    SPEED_HASTED,
};

enum species_type
{
    SP_HUMAN,
    SP_HIGH_ELF,
    SP_DEEP_ELF,
    SP_SLUDGE_ELF,
    SP_MOUNTAIN_DWARF,
    SP_HALFLING,
    SP_HILL_ORC,
    SP_KOBOLD,
    SP_MUMMY,
    SP_NAGA,
    SP_OGRE,
    SP_TROLL,
    SP_RED_DRACONIAN,
    SP_WHITE_DRACONIAN,
    SP_GREEN_DRACONIAN,
    SP_YELLOW_DRACONIAN,
    SP_GREY_DRACONIAN,
    SP_BLACK_DRACONIAN,
    SP_PURPLE_DRACONIAN,
    SP_MOTTLED_DRACONIAN,
    SP_PALE_DRACONIAN,
    SP_BASE_DRACONIAN,
    SP_CENTAUR,
    SP_DEMIGOD,
    SP_SPRIGGAN,
    SP_MINOTAUR,
    SP_DEMONSPAWN,
    SP_GHOUL,
    SP_KENKU,
    SP_MERFOLK,
    SP_VAMPIRE,
    SP_DEEP_DWARF,
    SP_CAT,
    SP_ELF,                            // (placeholder)
    SP_HILL_DWARF,                     // (placeholder)
    SP_OGRE_MAGE,                      // (placeholder)
    SP_GREY_ELF,                       // (placeholder)
    SP_GNOME,                          // (placeholder)
    NUM_SPECIES,                       // always after the last species

    SP_UNKNOWN  = 100,
    SP_RANDOM   = 101,
    SP_VIABLE   = 102,
};

enum spell_type
{
    SPELL_NO_SPELL,
    SPELL_TELEPORT_SELF,
    SPELL_CAUSE_FEAR,
    SPELL_MAGIC_DART,
    SPELL_FIREBALL,
    SPELL_APPORTATION,
    SPELL_DELAYED_FIREBALL,
    SPELL_STRIKING,
    SPELL_CONJURE_FLAME,
    SPELL_DIG,
    SPELL_BOLT_OF_FIRE,
    SPELL_BOLT_OF_COLD,
    SPELL_LIGHTNING_BOLT,
    SPELL_BOLT_OF_MAGMA,
    SPELL_POLYMORPH_OTHER,
    SPELL_SLOW,
    SPELL_HASTE,
    SPELL_PARALYSE,
    SPELL_CONFUSE,
    SPELL_INVISIBILITY,
    SPELL_THROW_FLAME,
    SPELL_THROW_FROST,
    SPELL_CONTROLLED_BLINK,
    SPELL_FREEZING_CLOUD,
    SPELL_MEPHITIC_CLOUD,
    SPELL_RING_OF_FLAMES,
    SPELL_VENOM_BOLT,
    SPELL_OLGREBS_TOXIC_RADIANCE,
    SPELL_TELEPORT_OTHER,
    SPELL_MINOR_HEALING,
    SPELL_MAJOR_HEALING,
    SPELL_DEATHS_DOOR,
    SPELL_MASS_CONFUSION,
    SPELL_SMITING,
    SPELL_SUMMON_SMALL_MAMMALS,
    SPELL_ABJURATION,
    SPELL_SUMMON_SCORPIONS,
    SPELL_LEVITATION,
    SPELL_BOLT_OF_DRAINING,
    SPELL_LEHUDIBS_CRYSTAL_SPEAR,
    SPELL_BOLT_OF_INACCURACY,
    SPELL_POISONOUS_CLOUD,
    SPELL_FIRE_STORM,
    SPELL_DETECT_TRAPS,
    SPELL_BLINK,
    SPELL_ISKENDERUNS_MYSTIC_BLAST,
    SPELL_SUMMON_SWARM,
    SPELL_SUMMON_HORRIBLE_THINGS,
    SPELL_ENSLAVEMENT,
    SPELL_ANIMATE_DEAD,
    SPELL_PAIN,
    SPELL_EXTENSION,
    SPELL_CONTROL_UNDEAD,
    SPELL_ANIMATE_SKELETON,
    SPELL_VAMPIRIC_DRAINING,
    SPELL_HAUNT,
    SPELL_DETECT_ITEMS,
    SPELL_BORGNJORS_REVIVIFICATION,
    SPELL_FREEZE,
    SPELL_SUMMON_ELEMENTAL,
    SPELL_OZOCUBUS_REFRIGERATION,
    SPELL_STICKY_FLAME,
    SPELL_SUMMON_ICE_BEAST,
    SPELL_OZOCUBUS_ARMOUR,
    SPELL_CALL_IMP,
    SPELL_REPEL_MISSILES,
    SPELL_BERSERKER_RAGE,
    SPELL_DISPEL_UNDEAD,
    SPELL_FULSOME_DISTILLATION,
    SPELL_POISON_ARROW,
    SPELL_TWISTED_RESURRECTION,
    SPELL_REGENERATION,
    SPELL_BANISHMENT,
    SPELL_CIGOTUVIS_DEGENERATION,
    SPELL_STING,
    SPELL_SUBLIMATION_OF_BLOOD,
    SPELL_TUKIMAS_DANCE,
    SPELL_HELLFIRE,
    SPELL_SUMMON_DEMON,
    SPELL_DEMONIC_HORDE,
    SPELL_SUMMON_GREATER_DEMON,
    SPELL_CORPSE_ROT,
    SPELL_FIRE_BRAND,
    SPELL_FREEZING_AURA,
    SPELL_LETHAL_INFUSION,
    SPELL_IRON_SHOT,
    SPELL_STONE_ARROW,
    SPELL_STONEMAIL,
    SPELL_SHOCK,
    SPELL_SWIFTNESS,
    SPELL_FLY,
    SPELL_INSULATION,
    SPELL_DETECT_CREATURES,
    SPELL_CURE_POISON,
    SPELL_CONTROL_TELEPORT,
    SPELL_POISON_WEAPON,
    SPELL_RESIST_POISON,
    SPELL_PROJECTED_NOISE,
    SPELL_ALTER_SELF,
    SPELL_DEBUGGING_RAY,
    SPELL_RECALL,
    SPELL_AGONY,
    SPELL_SPIDER_FORM,
    SPELL_DISINTEGRATE,
    SPELL_BLADE_HANDS,
    SPELL_STATUE_FORM,
    SPELL_ICE_FORM,
    SPELL_DRAGON_FORM,
    SPELL_NECROMUTATION,
    SPELL_DEATH_CHANNEL,
    SPELL_SYMBOL_OF_TORMENT,
    SPELL_DEFLECT_MISSILES,
    SPELL_THROW_ICICLE,
    SPELL_ICE_STORM,
    SPELL_AIRSTRIKE,
    SPELL_SHADOW_CREATURES,
    SPELL_CONFUSING_TOUCH,
    SPELL_SURE_BLADE,
    SPELL_FLAME_TONGUE,
    SPELL_PASSWALL,
    SPELL_IGNITE_POISON,
    SPELL_STICKS_TO_SNAKES,
    SPELL_CALL_CANINE_FAMILIAR,
    SPELL_SUMMON_DRAGON,
    SPELL_HIBERNATION,
    SPELL_ENGLACIATION,
    SPELL_DETECT_SECRET_DOORS,
    SPELL_SEE_INVISIBLE,
    SPELL_PHASE_SHIFT,
    SPELL_SUMMON_BUTTERFLIES,
    SPELL_WARP_BRAND,
    SPELL_SILENCE,
    SPELL_SHATTER,
    SPELL_DISPERSAL,
    SPELL_DISCHARGE,
    SPELL_CORONA,
    SPELL_INTOXICATE,
    SPELL_EVAPORATE,
    SPELL_FRAGMENTATION,
    SPELL_SANDBLAST,
    SPELL_MAXWELLS_SILVER_HAMMER,
    SPELL_CONDENSATION_SHIELD,
    SPELL_STONESKIN,
    SPELL_SIMULACRUM,
    SPELL_CONJURE_BALL_LIGHTNING,
    SPELL_CHAIN_LIGHTNING,
    SPELL_EXCRUCIATING_WOUNDS,
    SPELL_PORTAL_PROJECTILE,
    SPELL_SUMMON_UGLY_THING,
    SPELL_PETRIFY,
    SPELL_GOLUBRIAS_PASSAGE,

    // Mostly monster-only spells after this point:
    SPELL_HELLFIRE_BURST,
    SPELL_VAMPIRE_SUMMON,
    SPELL_BRAIN_FEED,
    SPELL_FAKE_RAKSHASA_SUMMON,
    SPELL_STEAM_BALL,
    SPELL_SUMMON_UFETUBUS,
    SPELL_SUMMON_BEAST,
    SPELL_ENERGY_BOLT,
    SPELL_POISON_SPLASH,
    SPELL_SUMMON_UNDEAD,
    SPELL_CANTRIP,
    SPELL_QUICKSILVER_BOLT,
    SPELL_METAL_SPLINTERS,
    SPELL_MIASMA,
    SPELL_SUMMON_DRAKES,
    SPELL_BLINK_OTHER,
    SPELL_SUMMON_MUSHROOMS,
    SPELL_ACID_SPLASH,
    SPELL_STICKY_FLAME_SPLASH,
    SPELL_FIRE_BREATH,
    SPELL_COLD_BREATH,
    SPELL_DRACONIAN_BREATH,
    SPELL_WATER_ELEMENTALS,
    SPELL_PORKALATOR,
    SPELL_KRAKEN_TENTACLES,
    SPELL_TOMB_OF_DOROKLOHE,
    SPELL_SUMMON_EYEBALLS,
    SPELL_HASTE_OTHER,
    SPELL_FIRE_ELEMENTALS,
    SPELL_EARTH_ELEMENTALS,
    SPELL_AIR_ELEMENTALS,
    SPELL_SLEEP,
    SPELL_BLINK_OTHER_CLOSE,
    SPELL_BLINK_CLOSE,
    SPELL_BLINK_RANGE,
    SPELL_BLINK_AWAY,
    SPELL_MISLEAD,
    SPELL_FAKE_MARA_SUMMON,
    SPELL_SUMMON_RAKSHASA,
    SPELL_SUMMON_ILLUSION,
    SPELL_PRIMAL_WAVE,
    SPELL_CALL_TIDE,
    SPELL_IOOD,
    SPELL_INK_CLOUD,
    SPELL_MIGHT,
    SPELL_SUNRAY,
    SPELL_AWAKEN_FOREST,
    SPELL_SUMMON_CANIFORMS,
    SPELL_IRON_ELEMENTALS,
    SPELL_SUMMON_SPECTRAL_ORCS,
    SPELL_RESURRECT,
    SPELL_LIGHT_BEAM,
    SPELL_HOLY_WORD,
    SPELL_SUMMON_HOLIES,
    SPELL_SUMMON_GREATER_HOLY,
    SPELL_HEAL_OTHER,
    SPELL_SACRIFICE,
    SPELL_HOLY_FLAMES,
    SPELL_HOLY_BREATH,
    SPELL_TROGS_HAND,
    SPELL_BROTHERS_IN_ARMS,
    SPELL_MIRROR_DAMAGE,
    SPELL_DRAIN_LIFE,
    SPELL_MIASMA_CLOUD,
    SPELL_POISON_CLOUD,
    SPELL_FIRE_CLOUD,
    SPELL_STEAM_CLOUD,
    SPELL_MALIGN_GATEWAY,
    SPELL_NOXIOUS_CLOUD,
    SPELL_TORNADO,
    SPELL_STICKY_FLAME_RANGE,
    SPELL_LEDAS_LIQUEFACTION,
    SPELL_HOMUNCULUS,
    SPELL_SUMMON_HYDRA,
    SPELL_TUKIMAS_DANCE_PARTY,

    NUM_SPELLS
};

enum slot_select_mode
{
    SS_FORWARD      = 0,
    SS_BACKWARD     = 1,
};

enum stat_type
{
  STAT_STR,                     //    0
  STAT_INT,
  STAT_DEX,
  NUM_STATS, // added for increase_stats() {dlb}
  STAT_ALL, // must remain after NUM_STATS -- added to handle royal jelly, etc. {dlb}
  STAT_RANDOM,
};

enum targeting_type
{
    DIR_NONE,
    DIR_TARGET, // smite targetting
    DIR_DIR,    // needs a clear line to target
    DIR_TARGET_OBJECT, // New as of 27-August-2009, for item-targeting spells
};

enum torment_source_type
{
    TORMENT_GENERIC       = -1,
    TORMENT_CARDS         = -2,   // Symbol of torment
    TORMENT_SPWLD         = -3,   // Special wield torment
    TORMENT_SCROLL        = -4,
    TORMENT_SPELL         = -5,   // SPELL_SYMBOL_OF_TORMENT
    TORMENT_XOM           = -6,   // Xom effect
    TORMENT_KIKUBAAQUDGHA = -7,   // Kikubaaqudgha effect
};

enum trap_type                         // env.trap_type[]
{
    TRAP_DART,
    TRAP_ARROW,
    TRAP_SPEAR,
    TRAP_AXE,
    TRAP_TELEPORT,
    TRAP_ALARM,
    TRAP_BLADE,
    TRAP_BOLT,
    TRAP_NET,
    TRAP_ZOT,
    TRAP_NEEDLE,
    TRAP_SHAFT,
    TRAP_GOLUBRIA,
    TRAP_PLATE,
    NUM_TRAPS,                         // must remain last 'regular' member {dlb}
    TRAP_MAX_REGULAR = TRAP_SHAFT,
    TRAP_UNASSIGNED = 100,
    TRAP_INDEPTH,                      // Level-appropriate trap.
    TRAP_NONTELEPORT,
    TRAP_RANDOM,
};

enum undead_state_type                // you.is_undead
{
    US_ALIVE = 0,
    US_HUNGRY_DEAD,     // Ghouls
    US_UNDEAD,          // Mummies
    US_SEMI_UNDEAD,     // Vampires
};

enum unique_item_status_type
{
    UNIQ_NOT_EXISTS = 0,
    UNIQ_EXISTS = 1,
    UNIQ_LOST_IN_ABYSS = 2,
};

enum friendly_pickup_type
{
    FRIENDLY_PICKUP_NONE = 0,
    FRIENDLY_PICKUP_FRIEND,
    FRIENDLY_PICKUP_PLAYER,
    FRIENDLY_PICKUP_ALL,
};

enum zap_type
{
    ZAP_FLAME,
    ZAP_FROST,
    ZAP_SLOWING,
    ZAP_HASTING,
    ZAP_MAGIC_DARTS,
    ZAP_HEALING,
    ZAP_PARALYSIS,
    ZAP_FIRE,
    ZAP_COLD,
    ZAP_CONFUSION,
    ZAP_INVISIBILITY,
    ZAP_DIGGING,
    ZAP_FIREBALL,
    ZAP_TELEPORTATION,
    ZAP_LIGHTNING,
    ZAP_POLYMORPH_OTHER,
    ZAP_LAST_RANDOM = ZAP_POLYMORPH_OTHER,   // maximal random_effects beam
    ZAP_VENOM_BOLT,
    ZAP_NEGATIVE_ENERGY,
    ZAP_CRYSTAL_SPEAR,
    ZAP_BEAM_OF_ENERGY,
    ZAP_MYSTIC_BLAST,
    ZAP_ENSLAVEMENT,
    ZAP_PAIN,
    ZAP_STICKY_FLAME,
    ZAP_DISPEL_UNDEAD,
    ZAP_BONE_SHARDS,
    ZAP_BANISHMENT,
    ZAP_DEGENERATION,
    ZAP_STING,
    ZAP_HELLFIRE,
    ZAP_IRON_SHOT,
    ZAP_STRIKING,
    ZAP_STONE_ARROW,
    ZAP_ELECTRICITY,
    ZAP_ORB_OF_ELECTRICITY,
    ZAP_SPIT_POISON,
    ZAP_DEBUGGING_RAY,
    ZAP_BREATHE_FIRE,
    ZAP_BREATHE_FROST,
    ZAP_BREATHE_ACID,
    ZAP_BREATHE_POISON,
    ZAP_BREATHE_POWER,
#if TAG_MAJOR_VERSION == 32
    ZAP_ENSLAVE_UNDEAD,
#endif
    ZAP_AGONY,
    ZAP_DISINTEGRATION,
    ZAP_BREATHE_STEAM,
#if TAG_MAJOR_VERSION == 32
    ZAP_CONTROL_DEMON,
#endif
    ZAP_ORB_OF_FRAGMENTATION,
    ZAP_THROW_ICICLE,
    ZAP_ICE_STORM,
    ZAP_CORONA,
    ZAP_HIBERNATION,
    ZAP_FLAME_TONGUE,
    ZAP_LARGE_SANDBLAST,
    ZAP_SANDBLAST,
    ZAP_SMALL_SANDBLAST,
    ZAP_MAGMA,
    ZAP_POISON_ARROW,
    ZAP_BREATHE_STICKY_FLAME,
    ZAP_BREATHE_LIGHTNING,
    ZAP_PETRIFY,
    ZAP_ENSLAVE_SOUL,
    ZAP_CHAOS,
    ZAP_SLIME,
    ZAP_PORKALATOR,
    ZAP_SLEEP,
    ZAP_PRIMAL_WAVE,
    ZAP_IOOD,
    ZAP_SUNRAY,
    ZAP_LIGHT_BEAM,
    ZAP_HOLY_FLAMES,
    ZAP_HOLY_BREATH,
    ZAP_BREATHE_MEPHITIC,

    NUM_ZAPS
};

enum montravel_target_type
{
    MTRAV_NONE = 0,
    MTRAV_PLAYER,      // Travelling to reach the player.
    MTRAV_PATROL,      // Travelling to reach the patrol point.
    MTRAV_SIREN,       // Sirens travelling towards deep water.
    MTRAV_WALL,        // Rock worms travelling towards a wall.
    MTRAV_UNREACHABLE, // Not travelling because target is unreachable.
    MTRAV_KNOWN_UNREACHABLE, // As above, and the player knows this.
};

enum maybe_bool
{
    B_FALSE,
    B_MAYBE,
    B_TRUE,
};

enum reach_type
{
    REACH_NONE,
    REACH_KNIGHT,
    REACH_TWO,
};

enum daction_type
{
    DACT_ALLY_HOLY,
    DACT_ALLY_UNHOLY_EVIL,
    DACT_ALLY_UNCLEAN_CHAOTIC,
    DACT_ALLY_SPELLCASTER,
    DACT_ALLY_YRED_SLAVE,
    DACT_ALLY_BEOGH, // both orcies and demons summoned by sorcerers
    DACT_ALLY_SLIME,
    DACT_ALLY_PLANT,

    NUM_DA_COUNTERS,

    // Leave space for new counters, as they need to be at the start.
    DACT_OLD_ENSLAVED_SOULS_POOF = 16,
    DACT_HOLY_NEW_ATTEMPT,
    DACT_HOLY_PETS_GO_NEUTRAL,
    DACT_ALLY_TROG,

    DACT_SHUFFLE_DECKS,
    DACT_REAUTOMAP,
    DACT_REMOVE_JIYVA_ALTARS,
    NUM_DACTIONS,
};

enum final_effect_flavour
{
    FINEFF_LIGHTNING_DISCHARGE,
    FINEFF_MIRROR_DAMAGE,
};

#ifdef USE_TILE
enum screen_mode
{
    SCREENMODE_WINDOW = 0,
    SCREENMODE_FULL   = 1,
    SCREENMODE_AUTO   = 2,
};

enum cursor_type
{
    CURSOR_MOUSE,
    CURSOR_TUTORIAL,
    CURSOR_MAP,
    CURSOR_MAX,
};

// Ordering of tags is important: higher values cover up lower ones.
enum text_tag_type
{
    TAG_NAMED_MONSTER = 0,
    TAG_TUTORIAL      = 1,
    TAG_CELL_DESC     = 2,
    TAG_MAX,
};

enum tag_pref
{
    TAGPREF_NONE,     // never display text tags
    TAGPREF_TUTORIAL, // display text tags on "new" monsters
    TAGPREF_NAMED,    // display text tags on named monsters (incl. friendlies)
    TAGPREF_ENEMY,    // display text tags on enemy named monsters
    TAGPREF_MAX,
};
enum tile_flags
{
    // Foreground flags

    // 3 mutually exclusive flags for attitude.
    TILE_FLAG_ATT_MASK   = 0x00001800,
    TILE_FLAG_PET        = 0x00000800,
    TILE_FLAG_GD_NEUTRAL = 0x00001000,
    TILE_FLAG_NEUTRAL    = 0x00001800,

    TILE_FLAG_S_UNDER    = 0x00002000,
    TILE_FLAG_FLYING     = 0x00004000,
    TILE_FLAG_STAB       = 0x00008000,
    TILE_FLAG_MAY_STAB   = 0x00010000,
    TILE_FLAG_NET        = 0x00020000,
    TILE_FLAG_POISON     = 0x00040000,
    TILE_FLAG_ANIM_WEP   = 0x00080000,
    TILE_FLAG_MIMIC      = 0x00100000,
    TILE_FLAG_FLAME      = 0x00200000,
    TILE_FLAG_BERSERK    = 0x00400000,

    // MDAM has 5 possibilities, so uses 3 bits.
    TILE_FLAG_MDAM_MASK  = 0x03800000,
    TILE_FLAG_MDAM_LIGHT = 0x00800000,
    TILE_FLAG_MDAM_MOD   = 0x01000000,
    TILE_FLAG_MDAM_HEAVY = 0x01800000,
    TILE_FLAG_MDAM_SEV   = 0x02000000,
    TILE_FLAG_MDAM_ADEAD = 0x02800000,

    // Demon difficulty has 5 possibilities, so uses 3 bits.
    TILE_FLAG_DEMON      = 0x34000000,
    TILE_FLAG_DEMON_5    = 0x04000000,
    TILE_FLAG_DEMON_4    = 0x10000000,
    TILE_FLAG_DEMON_3    = 0x14000000,
    TILE_FLAG_DEMON_2    = 0x20000000,
    TILE_FLAG_DEMON_1    = 0x24000000,

    // Background flags
    TILE_FLAG_RAY        = 0x00000800,
    TILE_FLAG_MM_UNSEEN  = 0x00001000,
    TILE_FLAG_UNSEEN     = 0x00002000,
    TILE_FLAG_CURSOR1    = 0x00004000,
    TILE_FLAG_CURSOR2    = 0x00008000,
    TILE_FLAG_CURSOR3    = 0x0000C000,
    TILE_FLAG_CURSOR     = 0x0000C000,
    TILE_FLAG_TUT_CURSOR = 0x00010000,
    TILE_FLAG_TRAV_EXCL  = 0x00020000,
    TILE_FLAG_EXCL_CTR   = 0x00040000,
    TILE_FLAG_RAY_OOR    = 0x00080000,
    TILE_FLAG_OOR        = 0x00100000,
    TILE_FLAG_WATER      = 0x00200000,
    TILE_FLAG_NEW_STAIR  = 0x00400000,
    TILE_FLAG_WAS_SECRET = 0x00800000,

    // Kraken tentacle overlays.
    TILE_FLAG_KRAKEN_NW  = 0x01000000,
    TILE_FLAG_KRAKEN_NE  = 0x02000000,
    TILE_FLAG_KRAKEN_SE  = 0x04000000,
    TILE_FLAG_KRAKEN_SW  = 0x08000000,

    // Eldritch tentacle overlays.
    TILE_FLAG_ELDRITCH_NW = 0x10000000,
    TILE_FLAG_ELDRITCH_NE = 0x20000000,
    TILE_FLAG_ELDRITCH_SE = 0x40000000,
    TILE_FLAG_ELDRITCH_SW = 0x80000000,

    // General
    TILE_FLAG_MASK       = 0x000007FF,
};

enum tile_inventory_flags
{
    TILEI_FLAG_SELECT  = 0x0100,
    TILEI_FLAG_TRIED   = 0x0200,
    TILEI_FLAG_EQUIP   = 0x0400,
    TILEI_FLAG_FLOOR   = 0x0800,
    TILEI_FLAG_CURSE   = 0x1000,
    TILEI_FLAG_CURSOR  = 0x2000,
    TILEI_FLAG_MELDED  = 0x4000,
    TILEI_FLAG_INVALID = 0x8000,
};

enum tile_player_flags
{
    TILEP_SHOW_EQUIP    = 0x1000,
};

enum tile_player_flag_cut
{
    TILEP_FLAG_HIDE,
    TILEP_FLAG_NORMAL,
    TILEP_FLAG_CUT_CENTAUR,
    TILEP_FLAG_CUT_NAGA,
};

// normal tile size in px
enum
{
    TILE_X = 32,
    TILE_Y = 32,
};

// Don't change this without also modifying the data save/load routines.
enum
{
    NUM_MAX_DOLLS = 10,
};

#endif

#ifdef WIZARD

enum wizard_option_type
{
    WIZ_NEVER,                         // protect player from accidental wiz
    WIZ_NO,                            // don't start character in wiz mode
    WIZ_YES,                           // start character in wiz mode
};

#endif

#endif // ENUM_H
