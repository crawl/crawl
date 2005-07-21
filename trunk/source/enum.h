/*
 *  File:       enum.h
 *  Summary:    Global (ick) enums.
 *  Written by: Daniel Ligon
 *
 *  Change History (most recent first):
 *
 *      <11>   7 Aug 01  MV     Changed MSLOT_UNASSIGNED_I to MSLOT_MISCELLANY
 *                              added NUM_MISCELLANY, changed MONS_ANOTHER_
 *                              LAVA_THING to MONS_SALAMANDER
 *      <10>   7/29/00   JDJ    Changed NUM_SPELL_TYPES to 14 (from 32767!).
 *             24jun2000 jmf    Changed comment spacing so stuff fit in 80
 *                              columns; deleted some leading numbers in
 *                              comments (reasoning as above).
 *                              Also removed many "must be last" comments,
 *                              esp. where less-than-accurate.
 *      <9>    10jan2000 dlb    extensive - see changes.340             S
 *      <8>    04nov1999 cdl    added killed_by
 *      <7>    29sep1999 BCR    Added comments showing where uniques are
 *      <6>    25sep1999 CDL    Added commands
 *      <5>    09sep1999 BWR    Removed Great Swords skill
 *      <4>    06aug1999 BWR    added branch and level types
 *      <3>    02jun1999 DML    beams, clouds, ench, ms, kill,
 *                              other minor changes
 *      <2>    26may1999 JDJ    Added a header guard.
 *      <1>    --/--/--  CDL    Created
 */


#ifndef ENUM_H
#define ENUM_H

enum ABILITIES
{
    ABIL_NON_ABILITY = -1,
    ABIL_SPIT_POISON = 1,              //    1
    ABIL_GLAMOUR,
    ABIL_MAPPING,
    ABIL_TELEPORTATION,
    ABIL_BREATHE_FIRE,                 //    5
    ABIL_BLINK,
    ABIL_BREATHE_FROST,
    ABIL_BREATHE_POISON,
    ABIL_BREATHE_LIGHTNING,
    ABIL_SPIT_ACID,                    //   10
    ABIL_BREATHE_POWER,
    ABIL_EVOKE_BERSERK,
    ABIL_BREATHE_STICKY_FLAME,
    ABIL_BREATHE_STEAM,
    ABIL_FLY,                          //   15
    ABIL_SUMMON_MINOR_DEMON,
    ABIL_SUMMON_DEMON,
    ABIL_HELLFIRE,
    ABIL_TORMENT,
    ABIL_RAISE_DEAD,                   //   20
    ABIL_CONTROL_DEMON,
    ABIL_TO_PANDEMONIUM,
    ABIL_CHANNELING,
    ABIL_THROW_FLAME,
    ABIL_THROW_FROST,                  //   25
    ABIL_BOLT_OF_DRAINING,
    ABIL_BREATHE_HELLFIRE,
    ABIL_FLY_II,
    ABIL_DELAYED_FIREBALL,
    ABIL_MUMMY_RESTORATION,            //   30
    ABIL_EVOKE_MAPPING,
    ABIL_EVOKE_TELEPORTATION,
    ABIL_EVOKE_BLINK,                  //   33
    ABIL_EVOKE_TURN_INVISIBLE = 51,    //   51
    ABIL_EVOKE_TURN_VISIBLE,
    ABIL_EVOKE_LEVITATE,
    ABIL_EVOKE_STOP_LEVITATING,
    ABIL_END_TRANSFORMATION,           //   55
    ABIL_ZIN_REPEL_UNDEAD = 110,       //  110
    ABIL_ZIN_HEALING,
    ABIL_ZIN_PESTILENCE,
    ABIL_ZIN_HOLY_WORD,
    ABIL_ZIN_SUMMON_GUARDIAN,          //  114
    ABIL_TSO_REPEL_UNDEAD = 120,       //  120
    ABIL_TSO_SMITING,
    ABIL_TSO_ANNIHILATE_UNDEAD,
    ABIL_TSO_THUNDERBOLT,
    ABIL_TSO_SUMMON_DAEVA,             //  124
    ABIL_KIKU_RECALL_UNDEAD_SLAVES = 130,   //  130
    ABIL_KIKU_ENSLAVE_UNDEAD = 132,         //  132
    ABIL_KIKU_INVOKE_DEATH,                 //  133
    ABIL_YRED_ANIMATE_CORPSE = 140,         //  140
    ABIL_YRED_RECALL_UNDEAD,
    ABIL_YRED_ANIMATE_DEAD,
    ABIL_YRED_DRAIN_LIFE,
    ABIL_YRED_CONTROL_UNDEAD,               //  144
    ABIL_VEHUMET_CHANNEL_ENERGY = 160,         //  160
    ABIL_OKAWARU_MIGHT = 170,                //  170
    ABIL_OKAWARU_HEALING,
    ABIL_OKAWARU_HASTE,                        //  172
    ABIL_MAKHLEB_MINOR_DESTRUCTION = 180,      //  180
    ABIL_MAKHLEB_LESSER_SERVANT_OF_MAKHLEB,
    ABIL_MAKHLEB_MAJOR_DESTRUCTION,
    ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB,   //  183
    ABIL_SIF_MUNA_FORGET_SPELL = 190,           //  190
    ABIL_TROG_BERSERK = 200,          //  200
    ABIL_TROG_MIGHT,
    ABIL_TROG_HASTE_SELF,                   //  202
    ABIL_ELYVILON_LESSER_HEALING = 220,         //  220
    ABIL_ELYVILON_PURIFICATION,
    ABIL_ELYVILON_HEALING,
    ABIL_ELYVILON_RESTORATION,
    ABIL_ELYVILON_GREATER_HEALING,              //  224
    ABIL_CHARM_SNAKE,
    ABIL_TRAN_SERPENT_OF_HELL,
    ABIL_ROTTING,
    ABIL_TORMENT_II,
    ABIL_SHUGGOTH_SEED,
    ABIL_RENOUNCE_RELIGION = 250       //  250
};

enum ABILITY_FLAGS
{
    ABFLAG_NONE         = 0x00000000,
    ABFLAG_BREATH       = 0x00000001, // ability uses DUR_BREATH_WEAPON 
    ABFLAG_DELAY        = 0x00000002, // ability has its own delay (ie glamour)
    ABFLAG_PAIN         = 0x00000004, // ability must hurt player (ie torment)
    ABFLAG_EXHAUSTION   = 0x00000008, // fails if you.exhausted
    ABFLAG_INSTANT      = 0x00000010, // doesn't take time to use
    ABFLAG_PERMANENT_HP = 0x00000020, // costs permanent HPs
    ABFLAG_PERMANENT_MP = 0x00000040  // costs permanent MPs
};

enum AMMUNITION_DESCRIPTIONS
{
    DAMMO_ORCISH = 3,                  //    3
    DAMMO_ELVEN,
    DAMMO_DWARVEN                      //    5
};

enum ARMOUR
{
    ARM_ROBE,                          //    0
    ARM_LEATHER_ARMOUR,
    ARM_RING_MAIL,
    ARM_SCALE_MAIL,
    ARM_CHAIN_MAIL,
    ARM_SPLINT_MAIL,                   //    5
    ARM_BANDED_MAIL,
    ARM_PLATE_MAIL,
    ARM_SHIELD,
    ARM_CLOAK,
    ARM_HELMET,                        //   10
    ARM_GLOVES,
    ARM_BOOTS,
    ARM_BUCKLER,
    ARM_LARGE_SHIELD,
    ARM_DRAGON_HIDE,                   //   15
    ARM_TROLL_HIDE,
    ARM_CRYSTAL_PLATE_MAIL,
    ARM_DRAGON_ARMOUR,
    ARM_TROLL_LEATHER_ARMOUR,
    ARM_ICE_DRAGON_HIDE,               //   20
    ARM_ICE_DRAGON_ARMOUR,
    ARM_STEAM_DRAGON_HIDE,
    ARM_STEAM_DRAGON_ARMOUR,
    ARM_MOTTLED_DRAGON_HIDE,
    ARM_MOTTLED_DRAGON_ARMOUR,         //   25
    ARM_STORM_DRAGON_HIDE,
    ARM_STORM_DRAGON_ARMOUR,
    ARM_GOLD_DRAGON_HIDE,
    ARM_GOLD_DRAGON_ARMOUR,
    ARM_ANIMAL_SKIN,                   //   30
    ARM_SWAMP_DRAGON_HIDE,
    ARM_SWAMP_DRAGON_ARMOUR,
    NUM_ARMOURS
};

enum ARMOUR_DESCRIPTIONS
{
    DARM_PLAIN,                 // added for the heck of it, 15 Apr 2000 {dlb}
    DARM_EMBROIDERED_SHINY = 1, // which it is dependent upon armour subtype {dlb}
    DARM_RUNED,
    DARM_GLOWING,
    DARM_ELVEN,
    DARM_DWARVEN,                      //    5
    DARM_ORCISH
};

enum ARMOUR_PROPERTIES
{
    PARM_AC,                           //    0
    PARM_EVASION
};

// Note:  currently the size of the attr array is hard coded at 30! ick! -- bwr
enum ATTRIBUTES
{
    ATTR_DIVINE_LIGHTNING_PROTECTION,  //    0
    // ATTR_SPEC_AIR,                  // don't use this!
    // ATTR_SPEC_EARTH,
    ATTR_CONTROL_TELEPORT = 3,     
    ATTR_WALK_SLOWLY,
    ATTR_TRANSFORMATION,               //    5
    ATTR_CARD_COUNTDOWN,
    ATTR_CARD_TABLE,
    ATTR_NUM_DEMONIC_POWERS,
    ATTR_WAS_SILENCED,          //jmf: added for silenced messages
    ATTR_GOD_GIFT_COUNT,        //jmf: added to help manage god gift giving
    ATTR_EXPENSIVE_FLIGHT,      //jmf: flag for "manual flight" (ie wings)
    ATTR_DEMONIC_SCALES,        //jmf: remember which kind of scales to improve
    ATTR_WALLS,
    ATTR_LAST_WALLS,
    ATTR_DELAYED_FIREBALL,      // bwr: reserve fireballs
    NUM_ATTRIBUTES              // must always remain last member {dlb}
};

enum BANDS
{
    BAND_NO_BAND                = 0,
    BAND_KOBOLDS                = 1,
    BAND_ORCS,
    BAND_ORC_KNIGHT,
    BAND_KILLER_BEES,
    BAND_FLYING_SKULLS,         // 5
    BAND_SLIME_CREATURES,
    BAND_YAKS,
    BAND_UGLY_THINGS,
    BAND_HELL_HOUNDS,
    BAND_JACKALS,               // 10
    BAND_HELL_KNIGHTS,
    BAND_ORC_HIGH_PRIEST,
    BAND_GNOLLS,                // 13
    BAND_BUMBLEBEES             = 16,
    BAND_CENTAURS,
    BAND_YAKTAURS,
    BAND_INSUBSTANTIAL_WISPS,
    BAND_OGRE_MAGE,             // 20
    BAND_DEATH_YAKS,
    BAND_NECROMANCER,
    BAND_BALRUG,
    BAND_CACODEMON,
    BAND_EXECUTIONER,           // 25
    BAND_HELLWING,
    BAND_DEEP_ELF_FIGHTER,
    BAND_DEEP_ELF_KNIGHT,
    BAND_DEEP_ELF_HIGH_PRIEST,
    BAND_KOBOLD_DEMONOLOGIST,   // 30
    BAND_NAGAS,
    BAND_WAR_DOGS,
    BAND_GREY_RATS,
    BAND_GREEN_RATS,
    BAND_ORANGE_RATS,           // 35
    BAND_SHEEP,
    BAND_GHOULS,
    BAND_DEEP_TROLLS,
    BAND_HOGS,
    BAND_HELL_HOGS,             // 40
    BAND_GIANT_MOSQUITOES,
    BAND_BOGGARTS,
    BAND_BLINK_FROGS,
    BAND_SKELETAL_WARRIORS,     // 44
    NUM_BANDS                   // always last
};

enum BEAMS                        // beam[].flavour
{
    BEAM_MISSILE,                 //    0
    BEAM_MMISSILE,                //    1 - and similarly unresistable things
    BEAM_FIRE,
    BEAM_COLD,
    BEAM_MAGIC,
    BEAM_ELECTRICITY,             //    5
    BEAM_POISON,
    BEAM_NEG,
    BEAM_ACID,                    //    8
    BEAM_EXPLOSION = 10,          //   10
    BEAM_SPORE,                   //   11
    BEAM_POISON_ARROW,            //   12
    BEAM_HELLFIRE,                //   13 - found 11jan2000 {dlb}
    BEAM_ENERGY = 17,
    BEAM_HOLY = 18,               //   18 - aka beam of cleansing, golden flame
    BEAM_FRAG,
    BEAM_LAVA,                    //   20
    BEAM_BACKLIGHT,
    BEAM_SLEEP,
    BEAM_ICE,                     //   23
    BEAM_NUKE = 27,               //   27
    BEAM_RANDOM,                  //   currently translates into FIRE..ACID

    // These used to be handled in the colour field:
    BEAM_SLOW,                  // BLACK
    BEAM_HASTE,                 // BLUE
    BEAM_HEALING,               // GREEN
    BEAM_PARALYSIS,             // CYAN
    BEAM_CONFUSION,             // RED
    BEAM_INVISIBILITY,          // MAGENTA
    BEAM_DIGGING,               // BROWN
    BEAM_TELEPORT,              // LIGHTGREY
    BEAM_POLYMORPH,             // DARKGREY
    BEAM_CHARM,                 // LIGHTBLUE
    BEAM_BANISH,                // LIGHTGREEN
    BEAM_DEGENERATE,            // LIGHTCYAN
    BEAM_ENSLAVE_UNDEAD,        // LIGHTRED
    BEAM_PAIN,                  // LIGHTMAGENTA
    BEAM_DISPEL_UNDEAD,         // YELLOW
    BEAM_DISINTEGRATION,        // WHITE
    BEAM_ENSLAVE_DEMON,         // colour "16"

    // new beams for evaporate
    BEAM_POTION_STINKING_CLOUD,
    BEAM_POTION_POISON,
    BEAM_POTION_MIASMA,
    BEAM_POTION_STEAM,
    BEAM_POTION_FIRE,
    BEAM_POTION_COLD,
    BEAM_POTION_BLACK_SMOKE,
    BEAM_POTION_BLUE_SMOKE,
    BEAM_POTION_PURP_SMOKE,
    BEAM_POTION_RANDOM
};

enum BOOKS
{
    BOOK_MINOR_MAGIC_I,                //    0
    BOOK_MINOR_MAGIC_II,
    BOOK_MINOR_MAGIC_III,
    BOOK_CONJURATIONS_I,
    BOOK_CONJURATIONS_II,
    BOOK_FLAMES,                       //    5
    BOOK_FROST,
    BOOK_SUMMONINGS,
    BOOK_FIRE,
    BOOK_ICE,
    BOOK_SURVEYANCES,                  //   10
    BOOK_SPATIAL_TRANSLOCATIONS,
    BOOK_ENCHANTMENTS,
    BOOK_YOUNG_POISONERS,
    BOOK_TEMPESTS,
    BOOK_DEATH,                        //   15
    BOOK_HINDERANCE,
    BOOK_CHANGES,
    BOOK_TRANSFIGURATIONS,
    BOOK_PRACTICAL_MAGIC,
    BOOK_WAR_CHANTS,                   //   20
    BOOK_CLOUDS,
    BOOK_HEALING,
    BOOK_NECROMANCY,
    BOOK_NECRONOMICON,
    BOOK_CALLINGS,                     //   25
    BOOK_CHARMS,
    BOOK_DEMONOLOGY,
    BOOK_AIR,
    BOOK_SKY,
    BOOK_DIVINATIONS,                  //   30
    BOOK_WARP,
    BOOK_ENVENOMATIONS,
    BOOK_ANNIHILATIONS,
    BOOK_UNLIFE,
    BOOK_DESTRUCTION,                  //   35
    BOOK_CONTROL,
    BOOK_MUTATIONS,
    BOOK_TUKIMA,
    BOOK_GEOMANCY,
    BOOK_EARTH,                        //   40
    BOOK_MANUAL,
    BOOK_WIZARDRY,
    BOOK_POWER,
    BOOK_CANTRIPS,                     //jmf: 04jan2000
    BOOK_PARTY_TRICKS,           // 45 //jmf: 04jan2000
    BOOK_BEASTS,
    BOOK_STALKING,         // renamed -- assassination was confusing  -- bwr
    NUM_BOOKS
};

enum BRANCHES                          // you.where_are_you
{
    BRANCH_MAIN_DUNGEON,               //    0
    BRANCH_DIS,
    BRANCH_GEHENNA,
    BRANCH_VESTIBULE_OF_HELL,
    BRANCH_COCYTUS,
    BRANCH_TARTARUS,                   //    5
    BRANCH_INFERNO,                             // unimplemented
    BRANCH_THE_PIT,                    //    7  // unimplemented
    BRANCH_ORCISH_MINES = 10,          //   10
    BRANCH_HIVE,
    BRANCH_LAIR,
    BRANCH_SLIME_PITS,
    BRANCH_VAULTS,
    BRANCH_CRYPT,                      //   15
    BRANCH_HALL_OF_BLADES,
    BRANCH_HALL_OF_ZOT,
    BRANCH_ECUMENICAL_TEMPLE,
    BRANCH_SNAKE_PIT,
    BRANCH_ELVEN_HALLS,                //   20
    BRANCH_TOMB,
    BRANCH_SWAMP
};

enum BRANCH_STAIRS // you.branch_stairs[] - 10 less than BRANCHES {dlb}
{
    STAIRS_ORCISH_MINES,               //    0
    STAIRS_HIVE,
    STAIRS_LAIR,
    STAIRS_SLIME_PITS,
    STAIRS_VAULTS,
    STAIRS_CRYPT,                      //    5
    STAIRS_HALL_OF_BLADES,
    STAIRS_HALL_OF_ZOT,
    STAIRS_ECUMENICAL_TEMPLE,
    STAIRS_SNAKE_PIT,
    STAIRS_ELVEN_HALLS,                //   10
    STAIRS_TOMB,
    STAIRS_SWAMP
};

enum BURDEN_STATES                     // you.burden_state
{
    BS_UNENCUMBERED,                   //    0
    BS_ENCUMBERED = 2,                 //    2
    BS_OVERLOADED = 5                  //    5
};

enum CANNED_MESSAGES                   // canned_msg() - unsigned char
{
    MSG_SOMETHING_APPEARS,             //    0
    MSG_NOTHING_HAPPENS,
    MSG_YOU_RESIST,
    MSG_TOO_BERSERK,
    MSG_NOTHING_CARRIED,
    MSG_CANNOT_DO_YET,
    MSG_OK,
    MSG_UNTHINKING_ACT,
    MSG_SPELL_FIZZLES,
    MSG_HUH,
    MSG_EMPTY_HANDED
};

enum CLOUD_TYPES // cloud_type[], place_cloud(), big_cloud()
{
    CLOUD_NONE,                        //    0
    CLOUD_FIRE,                        //    1
    CLOUD_STINK,                       //    2
    CLOUD_COLD,                        //    3
    CLOUD_POISON,                      //    4
    CLOUD_GREY_SMOKE = 5,              //    5: found 11jan2000 {dlb}
    CLOUD_BLUE_SMOKE = 6,              //    6: found 11jan2000 {dlb}
    CLOUD_PURP_SMOKE = 7, // was: CLOUD_ENERGY and wrong 19jan2000 {dlb}
    CLOUD_STEAM,                       //    8
    CLOUD_MIASMA = 9,                  //    9: found 11jan2000 {dlb}
    CLOUD_BLACK_SMOKE = 10, //was: CLOUD_STICKY_FLAME and wrong 19jan2000 {dlb}
    CLOUD_DEBUGGING = 99,   //   99: used once as 'nonexistent cloud' {dlb}
// if env.cloud_type > 100, it is a monster's cloud {dlb}
    CLOUD_FIRE_MON = 101,              //  101: found 11jan2000 {dlb}
    CLOUD_STINK_MON = 102,             //  102: found 11jan2000 {dlb}
    CLOUD_COLD_MON = 103,              //  103: added 11jan2000 {dlb}
    CLOUD_POISON_MON = 104,            //  104
    CLOUD_GREY_SMOKE_MON = 105,        //  105: found 11jan2000 {dlb}
    CLOUD_BLUE_SMOKE_MON = 106,        //  106: found 11jan2000 {dlb}
    CLOUD_PURP_SMOKE_MON = 107,        //  107:
    CLOUD_STEAM_MON = 108,             //  108: added 11jan2000 {dlb}
    CLOUD_MIASMA_MON = 109,            //  109: added 11jan2000 {dlb}
    CLOUD_BLACK_SMOKE_MON = 110        //  110: added 19jan2000 {dlb}
};

enum COMMANDS
{
    CMD_NO_CMD = 1000,                 // 1000
    CMD_MOVE_NOWHERE,
    CMD_MOVE_LEFT,
    CMD_MOVE_DOWN,
    CMD_MOVE_UP,
    CMD_MOVE_RIGHT,
    CMD_MOVE_UP_LEFT,
    CMD_MOVE_DOWN_LEFT,
    CMD_MOVE_UP_RIGHT,
    CMD_MOVE_DOWN_RIGHT,
    CMD_RUN_LEFT,                      // 1000 +  10
    CMD_RUN_DOWN,
    CMD_RUN_UP,
    CMD_RUN_RIGHT,
    CMD_RUN_UP_LEFT,
    CMD_RUN_DOWN_LEFT,
    CMD_RUN_UP_RIGHT,
    CMD_RUN_DOWN_RIGHT,
    CMD_OPEN_DOOR_LEFT,
    CMD_OPEN_DOOR_DOWN,
    CMD_OPEN_DOOR_UP,                  // 1000 +  20
    CMD_OPEN_DOOR_RIGHT,
    CMD_OPEN_DOOR_UP_LEFT,
    CMD_OPEN_DOOR_DOWN_LEFT,
    CMD_OPEN_DOOR_UP_RIGHT,
    CMD_OPEN_DOOR_DOWN_RIGHT,
    CMD_OPEN_DOOR,
    CMD_CLOSE_DOOR,
    CMD_REST,
    CMD_GO_UPSTAIRS,
    CMD_GO_DOWNSTAIRS,                 // 1000 +  30
    CMD_TOGGLE_AUTOPICKUP,
    CMD_PICKUP,
    CMD_DROP,
    CMD_BUTCHER,
    CMD_INSPECT_FLOOR,
    CMD_EXAMINE_OBJECT,
    CMD_EVOKE,
    CMD_WIELD_WEAPON,
    CMD_WEAPON_SWAP,
    CMD_THROW,                         // 1000 +  40
    CMD_FIRE,
    CMD_WEAR_ARMOUR,
    CMD_REMOVE_ARMOUR,
    CMD_WEAR_JEWELLERY,
    CMD_REMOVE_JEWELLERY,
    CMD_LIST_WEAPONS,
    CMD_LIST_ARMOUR,
    CMD_LIST_JEWELLERY,
    CMD_ZAP_WAND,
    CMD_CAST_SPELL,                    // 1000 +  50
    CMD_MEMORISE_SPELL,
    CMD_USE_ABILITY,
    CMD_PRAY,
    CMD_EAT,
    CMD_QUAFF,
    CMD_READ,
    CMD_LOOK_AROUND,
    CMD_SEARCH,
    CMD_SHOUT,
    CMD_DISARM_TRAP,                   // 1000 +  60
    CMD_CHARACTER_DUMP,
    CMD_DISPLAY_COMMANDS,
    CMD_DISPLAY_INVENTORY,
    CMD_DISPLAY_KNOWN_OBJECTS,
    CMD_DISPLAY_MUTATIONS,
    CMD_DISPLAY_SKILLS,
    CMD_DISPLAY_MAP,
    CMD_DISPLAY_OVERMAP,
    CMD_DISPLAY_RELIGION,
    CMD_DISPLAY_CHARACTER_STATUS,      // 1000 +  70
    CMD_EXPERIENCE_CHECK,
    CMD_GET_VERSION,
    CMD_ADJUST_INVENTORY,
    CMD_REPLAY_MESSAGES,
    CMD_REDRAW_SCREEN,
    CMD_MACRO_ADD,
    CMD_MACRO_SAVE,
    CMD_SAVE_GAME,
    CMD_SAVE_GAME_NOW,
    CMD_SUSPEND_GAME,                  // 1000 +  80
    CMD_QUIT,
    CMD_WIZARD,
    CMD_DESTROY_ITEM,
    CMD_OBSOLETE_INVOKE
};

enum CONFIRM_LEVEL
{
    CONFIRM_NONE_EASY,
    CONFIRM_SAFE_EASY,
    CONFIRM_ALL_EASY
};

enum CORPSE_EFFECTS
{
    CE_NOCORPSE,                       //    0
    CE_CLEAN,                          //    1
    CE_CONTAMINATED,                   //    2
    CE_POISONOUS,                      //    3
    CE_HCL,                            //    4
    CE_MUTAGEN_RANDOM,                 //    5
    CE_MUTAGEN_GOOD, //    6 - may be worth implementing {dlb}
    CE_MUTAGEN_BAD, //    7 - may be worth implementing {dlb}
    CE_RANDOM, //    8 - not used, but may be worth implementing {dlb}
    CE_ROTTEN = 50 //   50 - must remain at 50 for now {dlb}
};

enum CORPSES
{
    CORPSE_BODY,                       //    0
    CORPSE_SKELETON
};

enum DEATH_KNIGHT_CHOICES
{
    DK_NO_SELECTION,
    DK_NECROMANCY,
    DK_YREDELEMNUL,
    DK_RANDOM
};

enum DECKS
{
    DECK_OF_WONDERS,                   //    0
    DECK_OF_SUMMONING,
    DECK_OF_TRICKS,
    DECK_OF_POWER,
    DECK_OF_PUNISHMENT
};

enum DELAY
{
    DELAY_NOT_DELAYED,                  
    DELAY_EAT,                         
    DELAY_ARMOUR_ON,
    DELAY_ARMOUR_OFF,
    DELAY_MEMORIZE,
    DELAY_BUTCHER,
    DELAY_AUTOPICKUP,
    DELAY_WEAPON_SWAP,                 // for easy_butcher
    DELAY_PASSWALL,
    DELAY_DROP_ITEM,
    DELAY_ASCENDING_STAIRS,
    DELAY_DESCENDING_STAIRS,
    DELAY_INTERUPTABLE        = 100,   // simple interuptable delay
    DELAY_UNINTERUPTABLE               // simple uninteruptable delay
};

enum DEMON_BEAMS
{
    DMNBM_HELLFIRE,                    //    0
    DMNBM_SMITING,
    DMNBM_BRAIN_FEED,
    DMNBM_MUTATION
};

enum DEMON_CLASSES                     // summon_any_demon()
{
    DEMON_LESSER,                      //    0: Class V
    DEMON_COMMON,                      //    1: Class II-IV
    DEMON_GREATER                      //    2: Class I
};

enum DESCRIPTION_LEVEL
{
    DESC_CAP_THE,                      // 0
    DESC_NOCAP_THE,                    // 1
    DESC_CAP_A,                        // 2
    DESC_NOCAP_A,                      // 3
    DESC_CAP_YOUR,                     // 4
    DESC_NOCAP_YOUR,                   // 5
    DESC_PLAIN,                        // 6
    DESC_NOCAP_ITS,                    // 7
    DESC_INVENTORY_EQUIP,                    // 8
    DESC_INVENTORY               // 8
};

enum DIRECTION                         // (unsigned char) you.char_direction
{
    DIR_DESCENDING = 0, //    0 - change and lose savefile compatibility (!!!)
    DIR_ASCENDING = 1   //    1 - change and lose savefile compatibility (!!!)
};

enum DUNGEON_FEATURES                  // (unsigned char) grd[][]
{
    DNGN_UNSEEN,                       //    0
    DNGN_ROCK_WALL,
    DNGN_STONE_WALL,
    DNGN_CLOSED_DOOR,
    DNGN_METAL_WALL,
    DNGN_SECRET_DOOR,                  //    5
    DNGN_GREEN_CRYSTAL_WALL,
    DNGN_ORCISH_IDOL,
    DNGN_WAX_WALL,                     //    8
    DNGN_PERMAROCK_WALL,               //    9 - for undiggable walls
    DNGN_LAST_SOLID_TILE = 10,         //   10 - just here temporarily {dlb}

    DNGN_LAVA_X = 11,                  //   11
    DNGN_WATER_X,                      //   12
    DNGN_SILVER_STATUE = 21,           //   21
    DNGN_GRANITE_STATUE,
    DNGN_ORANGE_CRYSTAL_STATUE,        //   23
    DNGN_STATUE_39 = 39,               //   39

    DNGN_LAVA = 61,                    //   61
    DNGN_DEEP_WATER,                   //   62
    DNGN_SHALLOW_WATER = 65,           //   65
    DNGN_WATER_STUCK,

    DNGN_FLOOR,                        //   67
    DNGN_ENTER_HELL = 69,              //   69
    DNGN_OPEN_DOOR,                    //   70
    DNGN_BRANCH_STAIRS,                //   71
    DNGN_TRAP_MECHANICAL = 75,         //   75
    DNGN_TRAP_MAGICAL,
    DNGN_TRAP_III,
    DNGN_UNDISCOVERED_TRAP,            //   78

    DNGN_ENTER_SHOP = 80,              //   80
    DNGN_ENTER_LABYRINTH,

    DNGN_STONE_STAIRS_DOWN_I,
    DNGN_STONE_STAIRS_DOWN_II,
    DNGN_STONE_STAIRS_DOWN_III,
    DNGN_ROCK_STAIRS_DOWN,   //   85 - was this supposed to be a ladder? {dlb}

    DNGN_STONE_STAIRS_UP_I,
    DNGN_STONE_STAIRS_UP_II,
    DNGN_STONE_STAIRS_UP_III,
    DNGN_ROCK_STAIRS_UP,    //   89 - was this supposed to be a ladder? {dlb}

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

    DNGN_BUILDER_SPECIAL_WALL = 105,   //  105; builder() only
    DNGN_BUILDER_SPECIAL_FLOOR,        //  106; builder() only

    DNGN_ENTER_ORCISH_MINES = 110,     //  110
    DNGN_ENTER_HIVE,
    DNGN_ENTER_LAIR,
    DNGN_ENTER_SLIME_PITS,
    DNGN_ENTER_VAULTS,
    DNGN_ENTER_CRYPT,                //  115
    DNGN_ENTER_HALL_OF_BLADES,
    DNGN_ENTER_ZOT,
    DNGN_ENTER_TEMPLE,
    DNGN_ENTER_SNAKE_PIT,
    DNGN_ENTER_ELVEN_HALLS,            //  120
    DNGN_ENTER_TOMB,
    DNGN_ENTER_SWAMP,                  //  122

    DNGN_RETURN_FROM_ORCISH_MINES = 130, //  130
    DNGN_RETURN_FROM_HIVE,
    DNGN_RETURN_FROM_LAIR,
    DNGN_RETURN_FROM_SLIME_PITS,
    DNGN_RETURN_FROM_VAULTS,
    DNGN_RETURN_FROM_CRYPT,            //  135
    DNGN_RETURN_FROM_HALL_OF_BLADES,
    DNGN_RETURN_FROM_ZOT,
    DNGN_RETURN_FROM_TEMPLE,
    DNGN_RETURN_FROM_SNAKE_PIT,
    DNGN_RETURN_FROM_ELVEN_HALLS,      //  140
    DNGN_RETURN_FROM_TOMB,
    DNGN_RETURN_FROM_SWAMP,               //  142

    DNGN_ALTAR_ZIN = 180,              //  180
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

    DNGN_BLUE_FOUNTAIN = 200,          //  200
    DNGN_DRY_FOUNTAIN_I,
    DNGN_SPARKLING_FOUNTAIN,           // aka 'Magic Fountain' {dlb}
    DNGN_DRY_FOUNTAIN_II,
    DNGN_DRY_FOUNTAIN_III,
    DNGN_DRY_FOUNTAIN_IV,              //  205
    DNGN_DRY_FOUNTAIN_V,
    DNGN_DRY_FOUNTAIN_VI,
    DNGN_DRY_FOUNTAIN_VII,
    DNGN_DRY_FOUNTAIN_VIII,
    DNGN_PERMADRY_FOUNTAIN = 210  // added (from dungeon.cc/maps.cc) 22jan2000 {dlb}
};

enum DURATIONS                         // you.duration[]
{
    DUR_LIQUID_FLAMES,                 //    0
    DUR_ICY_ARMOUR,
    DUR_REPEL_MISSILES,
    DUR_PRAYER,
    DUR_REGENERATION,
    DUR_SWIFTNESS,                     //    5
    DUR_INSULATION,
    DUR_STONEMAIL,
    DUR_CONTROLLED_FLIGHT,
    DUR_TELEPORT,
    DUR_CONTROL_TELEPORT,              //   10
    DUR_RESIST_POISON,
    DUR_BREATH_WEAPON,
    DUR_TRANSFORMATION,
    DUR_DEATH_CHANNEL,
    DUR_DEFLECT_MISSILES,              //   15
//jmf: new durations:
    DUR_FORESCRY,
    DUR_SEE_INVISIBLE,
    DUR_WEAPON_BRAND, // general "branding" spell counter
    DUR_SILENCE,
    DUR_GLAMOUR,                       //   20
    DUR_SHUGGOTH_SEED_RELOAD,
    DUR_INFECTED_SHUGGOTH_SEED,
    DUR_CONDENSATION_SHIELD,           //   23
    DUR_STONESKIN,
    DUR_REPEL_UNDEAD,                  //   25
    DUR_LAST_DUR, //jmf: for asserts
    NUM_DURATIONS = 30
    // set at 30 to prevent savefile incompatibilities 12mar2000{dlb}
};

enum ENCHANTMENT                       // menv[].enchantment[]
{
    ENCH_NONE = 0,                     //    0
    ENCH_SLOW,
    ENCH_HASTE,                        //    2
    ENCH_FEAR = 4,                     //    4
    ENCH_CONFUSION,                    //    5
    ENCH_INVIS,
    ENCH_YOUR_POISON_I,
    ENCH_YOUR_POISON_II,
    ENCH_YOUR_POISON_III,
    ENCH_YOUR_POISON_IV,               //   10
    ENCH_YOUR_SHUGGOTH_I,              //jmf: Shuggothim!
    ENCH_YOUR_SHUGGOTH_II,
    ENCH_YOUR_SHUGGOTH_III,
    ENCH_YOUR_SHUGGOTH_IV,
    ENCH_YOUR_ROT_I, //   15 //jmf: rotting effect for monsters
    ENCH_YOUR_ROT_II,
    ENCH_YOUR_ROT_III,
    ENCH_YOUR_ROT_IV,
    ENCH_SUMMON = 19,                  //   19
    ENCH_ABJ_I,                        //   20
    ENCH_ABJ_II,
    ENCH_ABJ_III,
    ENCH_ABJ_IV,
    ENCH_ABJ_V,
    ENCH_ABJ_VI,                       //   25
    ENCH_BACKLIGHT_I,                  //jmf: backlight for Corona spell
    ENCH_BACKLIGHT_II,
    ENCH_BACKLIGHT_III,
    ENCH_BACKLIGHT_IV,
    ENCH_CHARM = 30,                   //   30
    ENCH_YOUR_STICKY_FLAME_I,
    ENCH_YOUR_STICKY_FLAME_II,
    ENCH_YOUR_STICKY_FLAME_III,
    ENCH_YOUR_STICKY_FLAME_IV,         //   34
    ENCH_GLOWING_SHAPESHIFTER = 38,    //   38
    ENCH_SHAPESHIFTER,
    ENCH_TP_I,                         //   40
    ENCH_TP_II,
    ENCH_TP_III,
    ENCH_TP_IV,                        //   43
    ENCH_POISON_I = 57,                //   57
    ENCH_POISON_II,
    ENCH_POISON_III,
    ENCH_POISON_IV,                    //   60
    ENCH_STICKY_FLAME_I,
    ENCH_STICKY_FLAME_II,
    ENCH_STICKY_FLAME_III,
    ENCH_STICKY_FLAME_IV,
    ENCH_FRIEND_ABJ_I,                 //   no longer used
    ENCH_FRIEND_ABJ_II,                //   no longer used
    ENCH_FRIEND_ABJ_III,               //   no longer used
    ENCH_FRIEND_ABJ_IV,                //   no longer used
    ENCH_FRIEND_ABJ_V,                 //   no longer used
    ENCH_FRIEND_ABJ_VI,                //   no longer used
    ENCH_CREATED_FRIENDLY,             //   no longer used
    ENCH_SLEEP_WARY,
    ENCH_SUBMERGED,                    //   73 (includes air elementals in air)
    ENCH_SHORT_LIVED,                  //   74 for ball lightning 
    NUM_ENCHANTMENTS
};

enum ENCHANT_STATS
{
    ENCHANT_TO_HIT,
    ENCHANT_TO_DAM
};

enum EQUIPMENT
{
    EQ_WEAPON,                         //    0
    EQ_CLOAK,
    EQ_HELMET,
    EQ_GLOVES,
    EQ_BOOTS,
    EQ_SHIELD,                         //    5
    EQ_BODY_ARMOUR,
    EQ_LEFT_RING,
    EQ_RIGHT_RING,
    EQ_AMULET,
    NUM_EQUIP,

    // these aren't actual equipment slots, they're categories for functions
    EQ_STAFF            = 100,         // weapon with base_type OBJ_STAVES
    EQ_RINGS,                          // check both rings
    EQ_RINGS_PLUS,                     // check both rings and sum plus
    EQ_RINGS_PLUS2,                    // check both rings and sum plus2
    EQ_ALL_ARMOUR                      // check all armour types
};

enum FIRE_TYPES
{
    FIRE_NONE,
    FIRE_LAUNCHER,
    FIRE_DART,
    FIRE_STONE,
    FIRE_DAGGER,
    FIRE_SPEAR,
    FIRE_HAND_AXE,
    FIRE_CLUB,
    NUM_FIRE_TYPES
};

enum FLUSH_REASONS
{
    FLUSH_ON_FAILURE,                  // spell/ability failed to cast
    FLUSH_BEFORE_COMMAND,              // flush before getting a command
    FLUSH_ON_MESSAGE,                  // flush when printing a message
    NUM_FLUSH_REASONS
};

enum FOODS                             // mitm[].sub_type[]
{
    FOOD_MEAT_RATION,                  //    0
    FOOD_BREAD_RATION,
    FOOD_PEAR,
    FOOD_APPLE,
    FOOD_CHOKO,
    FOOD_HONEYCOMB,                    //    5
    FOOD_ROYAL_JELLY,
    FOOD_SNOZZCUMBER,
    FOOD_PIZZA,
    FOOD_APRICOT,
    FOOD_ORANGE,                       //   10
    FOOD_BANANA,
    FOOD_STRAWBERRY,
    FOOD_RAMBUTAN,
    FOOD_LEMON,
    FOOD_GRAPE,                        //   15
    FOOD_SULTANA,
    FOOD_LYCHEE,
    FOOD_BEEF_JERKY,
    FOOD_CHEESE,
    FOOD_SAUSAGE,                      //   20
    FOOD_CHUNK,
    NUM_FOODS
};

enum GENUS_PLAYER                      // see player::player_genus()
{
    GENPC_DRACONIAN,                   //    0
    GENPC_ELVEN,                       //    1
    GENPC_DWARVEN                      //    2
};

enum GENDER
{
    GENDER_NEUTER,
    GENDER_MALE,
    GENDER_FEMALE
};

enum GHOST_VALUES
{
    GVAL_MAX_HP,        // 0
    GVAL_EV,
    GVAL_AC,
    GVAL_SEE_INVIS,
    GVAL_RES_FIRE,
    GVAL_RES_COLD,      // 5
    GVAL_RES_ELEC,
    GVAL_DAMAGE,
    GVAL_BRAND,
    GVAL_SPECIES,
    GVAL_BEST_SKILL,    // 10
    GVAL_SKILL_LEVEL,
    GVAL_EXP_LEVEL,
    GVAL_CLASS,
    GVAL_SPELL_1,       // 14
    GVAL_SPELL_2,
    GVAL_SPELL_3,
    GVAL_SPELL_4,
    GVAL_SPELL_5,
    GVAL_SPELL_6,       // 19
    NUM_GHOST_VALUES,   // should always be last value

    // these values are for demonlords, which override the above:
    GVAL_DEMONLORD_SPELLCASTER = 9,
    GVAL_DEMONLORD_FLY,                 // 10
    GVAL_DEMONLORD_UNUSED,              // 11
    GVAL_DEMONLORD_HIT_DICE,            // 12
    GVAL_DEMONLORD_CYCLE_COLOUR         // 13
};

enum GODS                              //  you.religion
{
    GOD_NO_GOD,                        //    0
    GOD_ZIN,
    GOD_SHINING_ONE,
    GOD_KIKUBAAQUDGHA,
    GOD_YREDELEMNUL,
    GOD_XOM,                           //    5
    GOD_VEHUMET,
    GOD_OKAWARU,
    GOD_MAKHLEB,
    GOD_SIF_MUNA,
    GOD_TROG,                          //   10
    GOD_NEMELEX_XOBEH,
    GOD_ELYVILON,
    NUM_GODS,                          // always after last god

    GOD_RANDOM  = 100
};

enum GOOD_THINGS
{
    GOOD_KILLED_LIVING = 1, //    1 - killed a living monster in god's name
    GOOD_KILLED_UNDEAD,     //    2 - killed an undead in god's name
    GOOD_KILLED_DEMON,      //    3 - killed a demon in god's name
    GOOD_KILLED_ANGEL_I,    //    4 - killed an angel (any time)
    GOOD_KILLED_ANGEL_II,   //    5 - killed an angel in god's name
    // (all above pass HD of monster as pgain)
    GOOD_HACKED_CORPSE,     //    6 - hacked up a corpse in god's name
    GOOD_OFFER_STUFF,       //    7 - offered inanimate stuff at an altar
    GOOD_OFFER_CORPSE,      // as above,including at least one corpse
    GOOD_SLAVES_KILL_LIVING,//    9 - undead slaves killed a living thing
    GOOD_SERVANTS_KILL,     //   10 - any servants kill anything
    GOOD_CARDS,             //   11 - cards (Nemelex)
    GOOD_KILLED_WIZARD,
    GOOD_KILLED_PRIEST,
    GOOD_POISON,
    GOOD_ATTACKED_FRIEND,
    NUM_GOOD_THINGS
};

enum HANDS_REQUIRED
{
    HANDS_ONE_HANDED = 1,              //    1
    HANDS_TWO_HANDED,
    HANDS_ONE_OR_TWO_HANDED
};

enum HELMET_TYPES  // used in pluses2
{
    THELM_HELMET        = 0x0000,
    THELM_HELM          = 0x0001,
    THELM_CAP           = 0x0002,
    THELM_WIZARD_HAT    = 0x0003,
    THELM_NUM_TYPES     = 4,

    THELM_SPECIAL       = 0x0004,  // type used only for artefacts (mask, hat)
    THELM_TYPE_MASK     = 0x00ff,


    THELM_DESC_PLAIN    = 0x0000,
    THELM_DESC_WINGED   = 0x0100,
    THELM_DESC_HORNED   = 0x0200,
    THELM_DESC_CRESTED  = 0x0300,
    THELM_DESC_PLUMED   = 0x0400,
    THELM_DESC_SPIKED   = 0x0500,
    THELM_DESC_VISORED  = 0x0600,
    THELM_DESC_JEWELLED = 0x0700,

    THELM_DESC_MASK     = 0xff00
};

#if 0
enum HELMET_DESCRIPTIONS
{
    DHELM_WINGED = 1,                  //    1
    DHELM_HORNED,
    DHELM_CRESTED,
    DHELM_PLUMED,
    DHELM_SPIKED,                      //    5
    DHELM_VISORED,
    DHELM_JEWELLED
};

enum HELMET_TYPES  // used in pluses2
{
    THELM_HELMET = 0,
    THELM_HELM,
    THELM_CAP,
    THELM_WIZARD_HAT,
    NUM_HELMET_TYPES,
    THELM_SPECIAL               // type used only for artefacts (mask, hat)
};
#endif

enum BOOT_TYPES  // used in pluses2
{
    TBOOT_BOOTS = 0,
    TBOOT_NAGA_BARDING,
    TBOOT_CENTAUR_BARDING,
    NUM_BOOT_TYPES
};


enum HUNGER_STATES                     // you.hunger_state
{
    HS_RAVENOUS,                       //    0: not used within code, really
    HS_STARVING,
    HS_HUNGRY,
    HS_SATIATED,                       // "not hungry" state
    HS_FULL,
    HS_ENGORGED                        //    5
};

enum ITEM_STATUS_FLAGS      // per item flags: ie. ident status, cursed status
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
    ISFLAG_RESERVED_1        = 0x00000200,  // reserved (re-curses on wield?)
    ISFLAG_RESERVED_2        = 0x00000400,  // reserved (heavy cursed?)
    ISFLAG_RESERVED_3        = 0x00000800,  // reserved (perma-cursed?)
    ISFLAG_CURSE_MASK        = 0x00000F00,  // mask of all curse related flags

    ISFLAG_RANDART           = 0x00001000,  // special value is seed
    ISFLAG_UNRANDART         = 0x00002000,  // is an unrandart
    ISFLAG_ARTEFACT_MASK     = 0x00003000,  // randart or unrandart

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

    ISFLAG_DEBUG_MARK        = 0x80000000   // used for testing item structure
};

enum ITEM_DESCRIPTIONS
{
    IDESC_WANDS,
    IDESC_POTIONS,
    IDESC_SCROLLS,                      // special field (like the others)
    IDESC_RINGS,
    IDESC_SCROLLS_II                    // pluses field
};

enum ITEM_MAKE_SPECIES                  // used only for race during creation
{
    MAKE_ITEM_ELVEN       = 1,
    MAKE_ITEM_DWARVEN     = 2,
    MAKE_ITEM_ORCISH      = 3,

    MAKE_ITEM_NO_RACE     = 100,
    MAKE_ITEM_RANDOM_RACE = 250
};

enum ITEM_TYPE_ID       // used for first index of id[4][50]
{
    IDTYPE_WANDS = 0,
    IDTYPE_SCROLLS,
    IDTYPE_JEWELLERY,
    IDTYPE_POTIONS
};

enum ITEM_TYPE_ID_STATE  // used for values in id[4][50]
{
    ID_UNKNOWN_TYPE = 0,
    ID_KNOWN_TYPE,
    ID_TRIED_TYPE
};

enum JEWELLERY
{
    RING_REGENERATION,                 //    0
    RING_PROTECTION,
    RING_PROTECTION_FROM_FIRE,
    RING_POISON_RESISTANCE,
    RING_PROTECTION_FROM_COLD,
    RING_STRENGTH,                     //    5
    RING_SLAYING,
    RING_SEE_INVISIBLE,
    RING_INVISIBILITY,
    RING_HUNGER,
    RING_TELEPORTATION,                //   10
    RING_EVASION,
    RING_SUSTAIN_ABILITIES,
    RING_SUSTENANCE,
    RING_DEXTERITY,
    RING_INTELLIGENCE,                 //   15
    RING_WIZARDRY,
    RING_MAGICAL_POWER,
    RING_LEVITATION,
    RING_LIFE_PROTECTION,
    RING_PROTECTION_FROM_MAGIC,        //   20
    RING_FIRE,
    RING_ICE,
    RING_TELEPORT_CONTROL,             //   23
    AMU_RAGE = 35,                     //   35
    AMU_RESIST_SLOW,
    AMU_CLARITY,
    AMU_WARDING,
    AMU_RESIST_CORROSION,
    AMU_THE_GOURMAND,                  //   40
    AMU_CONSERVATION,
    AMU_CONTROLLED_FLIGHT,
    AMU_INACCURACY,
    AMU_RESIST_MUTATION,
    NUM_JEWELLERY
};

enum JOB
{
    JOB_FIGHTER,                       //    0
    JOB_WIZARD,
    JOB_PRIEST,
    JOB_THIEF,
    JOB_GLADIATOR,
    JOB_NECROMANCER,                   //    5
    JOB_PALADIN,
    JOB_ASSASSIN,
    JOB_BERSERKER,
    JOB_HUNTER,
    JOB_CONJURER,                      //   10
    JOB_ENCHANTER,
    JOB_FIRE_ELEMENTALIST,
    JOB_ICE_ELEMENTALIST,
    JOB_SUMMONER,
    JOB_AIR_ELEMENTALIST,              //   15
    JOB_EARTH_ELEMENTALIST,
    JOB_CRUSADER,
    JOB_DEATH_KNIGHT,
    JOB_VENOM_MAGE,
    JOB_CHAOS_KNIGHT,                  //   20
    JOB_TRANSMUTER,
    JOB_HEALER,                        //   22
    JOB_QUITTER,                       //   23 -- this is job 'x', don't use
    JOB_REAVER,                        //   24
    JOB_STALKER,                       //   25
    JOB_MONK,
    JOB_WARPER,
    JOB_WANDERER,                      //   23
    NUM_JOBS,                          // always after the last job

    JOB_UNKNOWN = 100
};

enum KILLBY
{
    KILLED_BY_MONSTER,                 //    0
    KILLED_BY_POISON,
    KILLED_BY_CLOUD,
    KILLED_BY_BEAM,                    //    3
    KILLED_BY_DEATHS_DOOR,  // should be deprecated, but you never know {dlb}
    KILLED_BY_LAVA,                    //    5
    KILLED_BY_WATER,
    KILLED_BY_STUPIDITY,
    KILLED_BY_WEAKNESS,
    KILLED_BY_CLUMSINESS,
    KILLED_BY_TRAP,                    //   10
    KILLED_BY_LEAVING,
    KILLED_BY_WINNING,
    KILLED_BY_QUITTING,
    KILLED_BY_DRAINING,
    KILLED_BY_STARVATION,              //   15
    KILLED_BY_FREEZING,
    KILLED_BY_BURNING,
    KILLED_BY_WILD_MAGIC,
    KILLED_BY_XOM,
    KILLED_BY_STATUE,                  //   20
    KILLED_BY_ROTTING,
    KILLED_BY_TARGETTING,
    KILLED_BY_SPORE,
    KILLED_BY_TSO_SMITING,
    KILLED_BY_PETRIFICATION,           // 25
    KILLED_BY_SHUGGOTH,
    KILLED_BY_SOMETHING,
    KILLED_BY_FALLING_DOWN_STAIRS,
    KILLED_BY_ACID,
    NUM_KILLBY
};

enum KILLER                            // monster_die(), thing_thrown
{
    KILL_YOU = 1,                      //    1
    KILL_MON,
    KILL_YOU_MISSILE,
    KILL_MON_MISSILE,
    KILL_MISC,                         //    5
    KILL_RESET                         // abjuration, etc.
};

#define YOU_KILL(x) ((x) == KILL_YOU || (x) == KILL_YOU_MISSILE)
#define MON_KILL(x) ((x) == KILL_MON || (x) == KILL_MON_MISSILE)

enum LEVEL_TYPES                       // you.level_type
{
    LEVEL_DUNGEON,                     //    0
    LEVEL_LABYRINTH,
    LEVEL_ABYSS,
    LEVEL_PANDEMONIUM
};

enum LOAD_MODE
{
    LOAD_START_GAME,
    LOAD_RESTART_GAME,
    LOAD_ENTER_LEVEL
};

enum MAP_SECTIONS                      // see maps.cc and dungeon.cc {dlb}
{
    MAP_NORTH = 1,                     //    1
    MAP_NORTHWEST,
    MAP_NORTHEAST,
    MAP_SOUTHWEST,
    MAP_SOUTHEAST,                     //    5
    MAP_ENCOMPASS,
    MAP_NORTH_DIS
};

// if you mess with this list, you'll need to make changes in initfile.cc
enum MESSAGE_CHANNEL 
{
    MSGCH_PLAIN,          // regular text
    MSGCH_PROMPT,         // various prompts
    MSGCH_GOD,            // god/religion (param is god)
    MSGCH_DURATION,       // effect down/warnings
    MSGCH_DANGER,         // serious life threats (ie very large HP attacks)
    MSGCH_WARN,           // much less serious threats
    MSGCH_FOOD,           // hunger notices
    MSGCH_RECOVERY,       // recovery from disease/stat/poison condition
    MSGCH_TALK,           // monster talk (param is monster type)
    MSGCH_INTRINSIC_GAIN, // player level/stat/species-power gains
    MSGCH_MUTATION,       // player gain/lose mutations
    MSGCH_MONSTER_SPELL,  // monsters casting spells
    MSGCH_MONSTER_ENCHANT,// monsters enchantments up and down
    MSGCH_MONSTER_DAMAGE, // monster damage reports (param is level)
    MSGCH_ROTTEN_MEAT,    // messages about chunks/corpses becoming rotten  
    MSGCH_EQUIPMENT,      // equipment listing messages
    MSGCH_DIAGNOSTICS,     // various diagnostic messages 
    NUM_MESSAGE_CHANNELS  // always last
};

enum MESSAGE_COLOURS 
{
    MSGCOL_BLACK        = 0,    // the order of these colours is important
    MSGCOL_BLUE,
    MSGCOL_GREEN,
    MSGCOL_CYAN,
    MSGCOL_RED,
    MSGCOL_MAGENTA,
    MSGCOL_BROWN,
    MSGCOL_LIGHTGRAY,
    MSGCOL_DARKGRAY,
    MSGCOL_LIGHTBLUE,
    MSGCOL_LIGHTGREEN,
    MSGCOL_LIGHTCYAN,
    MSGCOL_LIGHTMAGENTA,
    MSGCOL_YELLOW,
    MSGCOL_WHITE,
    MSGCOL_DEFAULT,             // use default colour
    MSGCOL_ALTERNATE,           // use secondary default colour scheme
    MSGCOL_MUTED,               // don't print messages
    MSGCOL_PLAIN                // same as plain channel
};

enum MISCELLANY                        // mitm[].sub_type
{
    MISC_BOTTLED_EFREET,               //    0
    MISC_CRYSTAL_BALL_OF_SEEING,
    MISC_AIR_ELEMENTAL_FAN,
    MISC_LAMP_OF_FIRE,
    MISC_STONE_OF_EARTH_ELEMENTALS,
    MISC_LANTERN_OF_SHADOWS,           //    5
    MISC_HORN_OF_GERYON,
    MISC_BOX_OF_BEASTS,
    MISC_DECK_OF_WONDERS,
    MISC_DECK_OF_SUMMONINGS,
    MISC_CRYSTAL_BALL_OF_ENERGY,       //   10
    MISC_EMPTY_EBONY_CASKET,
    MISC_CRYSTAL_BALL_OF_FIXATION,
    MISC_DISC_OF_STORMS,
    MISC_RUNE_OF_ZOT,
    MISC_DECK_OF_TRICKS,               //   15
    MISC_DECK_OF_POWER,
    MISC_PORTABLE_ALTAR_OF_NEMELEX,
    NUM_MISCELLANY // mv: used for random generation
};

enum MISSILES                          // (unsigned char)
{
    MI_STONE,                          //    0
    MI_ARROW,
    MI_BOLT,
    MI_DART,
    MI_NEEDLE,
    MI_LARGE_ROCK, //jmf: it'd be nice to move MI_LARGE_ROCK to DEBRIS_ROCK
    NUM_MISSILES,
    MI_EGGPLANT
};

enum MON_TARG_MODE
{
    TARG_ANY, 
    TARG_ENEMY,
    TARG_FRIEND,
    TARG_NUM_MODES
};

enum MONSTERS                          // (int) menv[].type
{
    MONS_GIANT_ANT,                    //    0
    MONS_GIANT_BAT,
    MONS_CENTAUR,
    MONS_RED_DEVIL,
    MONS_ETTIN,
    MONS_FUNGUS,                       //    5
    MONS_GOBLIN,
    MONS_HOUND,
    MONS_IMP,
    MONS_JACKAL,
    MONS_KILLER_BEE,                   //   10
    MONS_KILLER_BEE_LARVA,
    MONS_MANTICORE,
    MONS_NECROPHAGE,
    MONS_ORC,
    MONS_PHANTOM,                      //   15
    MONS_QUASIT,
    MONS_RAT,
    MONS_SCORPION,                     //   18
    //MONS_TUNNELING_WORM,      // deprecated and now officially removed {dlb}
    MONS_UGLY_THING = 20,              //   20
    MONS_FIRE_VORTEX,
    MONS_WORM,
    MONS_ABOMINATION_SMALL,
    MONS_YELLOW_WASP,
    MONS_ZOMBIE_SMALL,                 //   25
    MONS_ANGEL,
    MONS_GIANT_BEETLE,
    MONS_CYCLOPS,
    MONS_DRAGON,
    MONS_TWO_HEADED_OGRE,              //   30
    MONS_FIEND,
    MONS_GIANT_SPORE,
    MONS_HOBGOBLIN,
    MONS_ICE_BEAST,
    MONS_JELLY,                        //   35
    MONS_KOBOLD,
    MONS_LICH,
    MONS_MUMMY,
    MONS_GUARDIAN_NAGA,
    MONS_OGRE,                         //   40
    MONS_PLANT,
    MONS_QUEEN_BEE,
    MONS_RAKSHASA,
    MONS_SNAKE,
    MONS_TROLL,                        //   45
    MONS_UNSEEN_HORROR,
    MONS_VAMPIRE,
    MONS_WRAITH,
    MONS_ABOMINATION_LARGE,
    MONS_YAK,                          //   50
    MONS_ZOMBIE_LARGE,
    MONS_ORC_WARRIOR,
    MONS_KOBOLD_DEMONOLOGIST,
    MONS_ORC_WIZARD,
    MONS_ORC_KNIGHT,                   //   55
    //MONS_WORM_TAIL = 56, // deprecated and now officially removed {dlb}
    MONS_WYVERN = 57,                  //   57
    MONS_BIG_KOBOLD,
    MONS_GIANT_EYEBALL,
    MONS_WIGHT,                        //   60
    MONS_OKLOB_PLANT,
    MONS_WOLF_SPIDER,
    MONS_SHADOW,
    MONS_HUNGRY_GHOST,
    MONS_EYE_OF_DRAINING,              //   65
    MONS_BUTTERFLY,
    MONS_WANDERING_MUSHROOM,
    MONS_EFREET,
    MONS_BRAIN_WORM,
    MONS_GIANT_ORANGE_BRAIN,           //   70
    MONS_BOULDER_BEETLE,
    MONS_FLYING_SKULL,
    MONS_HELL_HOUND,
    MONS_MINOTAUR,
    MONS_ICE_DRAGON,                   //   75
    MONS_SLIME_CREATURE,
    MONS_FREEZING_WRAITH,
    MONS_RAKSHASA_FAKE,
    MONS_GREAT_ORB_OF_EYES,
    MONS_HELLION,                      //   80
    MONS_ROTTING_DEVIL,
    MONS_TORMENTOR,
    MONS_REAPER,
    MONS_SOUL_EATER,
    MONS_HAIRY_DEVIL,                  //   85
    MONS_ICE_DEVIL,
    MONS_BLUE_DEVIL,
    MONS_BEAST,
    MONS_IRON_DEVIL,                   //   89
    MONS_GLOWING_SHAPESHIFTER = 98,    //   98
    MONS_SHAPESHIFTER,
    MONS_GIANT_MITE,                   //  100
    MONS_STEAM_DRAGON,
    MONS_VERY_UGLY_THING,
    MONS_ORC_SORCERER,
    MONS_HIPPOGRIFF,
    MONS_GRIFFON,                      //  105
    MONS_HYDRA,
    MONS_SKELETON_SMALL,
    MONS_SKELETON_LARGE,
    MONS_HELL_KNIGHT,
    MONS_NECROMANCER,                  //  110
    MONS_WIZARD,
    MONS_ORC_PRIEST,
    MONS_ORC_HIGH_PRIEST,
    MONS_HUMAN,
    MONS_GNOLL,                        //  115
    MONS_CLAY_GOLEM,
    MONS_WOOD_GOLEM,
    MONS_STONE_GOLEM,
    MONS_IRON_GOLEM,
    MONS_CRYSTAL_GOLEM,                //  120
    MONS_TOENAIL_GOLEM,
    MONS_MOTTLED_DRAGON,
    MONS_EARTH_ELEMENTAL,
    MONS_FIRE_ELEMENTAL,
    MONS_AIR_ELEMENTAL,                //  125
    MONS_ICE_FIEND,
    MONS_SHADOW_FIEND,
    MONS_BROWN_SNAKE,
    MONS_GIANT_LIZARD,
    MONS_SPECTRAL_WARRIOR,             //  130
    MONS_PULSATING_LUMP,
    MONS_STORM_DRAGON,
    MONS_YAKTAUR,
    MONS_DEATH_YAK,
    MONS_ROCK_TROLL,                   //  135
    MONS_STONE_GIANT,
    MONS_FLAYED_GHOST,
    MONS_BUMBLEBEE,
    MONS_REDBACK,
    MONS_INSUBSTANTIAL_WISP,           //  140
    MONS_VAPOUR,
    MONS_OGRE_MAGE,
    MONS_SPINY_WORM,
    MONS_DANCING_WEAPON,
    MONS_TITAN,                        //  145
    MONS_GOLDEN_DRAGON,
    MONS_ELF,
    MONS_LINDWURM,
    MONS_ELEPHANT_SLUG,
    MONS_WAR_DOG,                      //  150
    MONS_GREY_RAT,
    MONS_GREEN_RAT,
    MONS_ORANGE_RAT,
    MONS_BLACK_SNAKE,
    MONS_SHEEP,                        //  155
    MONS_GHOUL,
    MONS_HOG,
    MONS_GIANT_MOSQUITO,
    MONS_GIANT_CENTIPEDE,
    MONS_IRON_TROLL,                   //  160
    MONS_NAGA,
    MONS_FIRE_GIANT,
    MONS_FROST_GIANT,
    MONS_FIREDRAKE,
    MONS_SHADOW_DRAGON,                //  165
    MONS_YELLOW_SNAKE,
    MONS_GREY_SNAKE,
    MONS_DEEP_TROLL,
    MONS_GIANT_BLOWFLY,
    MONS_RED_WASP,                     //  170
    MONS_SWAMP_DRAGON,
    MONS_SWAMP_DRAKE,
    MONS_SOLDIER_ANT,
    MONS_HILL_GIANT,
    MONS_QUEEN_ANT,                    //  175
    MONS_ANT_LARVA,
    MONS_GIANT_FROG,
    MONS_GIANT_BROWN_FROG,
    MONS_SPINY_FROG,
    MONS_BLINK_FROG,                   //  180
    MONS_GIANT_COCKROACH,
    MONS_SMALL_SNAKE,                  //  182
    //jmf: new monsters
    MONS_SHUGGOTH, //jmf: added for evil spells
    MONS_WOLF,     //jmf: added
    MONS_WARG,     //jmf: added for orc mines
    MONS_BEAR,     //jmf: added bears!
    MONS_GRIZZLY_BEAR,
    MONS_POLAR_BEAR,
    MONS_BLACK_BEAR,  // 189
    MONS_SIMULACRUM_SMALL,
    MONS_SIMULACRUM_LARGE,
    //jmf: end new monsters
    MONS_WHITE_IMP = 220,              //  220
    MONS_LEMURE,
    MONS_UFETUBUS,
    MONS_MANES,
    MONS_MIDGE,
    MONS_NEQOXEC,                      //  225
    MONS_ORANGE_DEMON,
    MONS_HELLWING,
    MONS_SMOKE_DEMON,
    MONS_YNOXINUL,
    MONS_EXECUTIONER,                  //  230
    MONS_GREEN_DEATH,
    MONS_BLUE_DEATH,
    MONS_BALRUG,
    MONS_CACODEMON,
    MONS_DEMONIC_CRAWLER,              //  235
    MONS_SUN_DEMON,
    MONS_SHADOW_IMP,
    MONS_SHADOW_DEMON,
    MONS_LOROCYPROCA,
    MONS_SHADOW_WRAITH,                //  240
    MONS_GIANT_AMOEBA,
    MONS_GIANT_SLUG,
    MONS_GIANT_SNAIL,
    MONS_SPATIAL_VORTEX,
    MONS_PIT_FIEND,                    //  245
    MONS_BORING_BEETLE,
    MONS_GARGOYLE,
    MONS_METAL_GARGOYLE,
    MONS_MOLTEN_GARGOYLE,
    MONS_PROGRAM_BUG,                  //  250
// BCR - begin first batch of uniques.
    MONS_MNOLEG,
    MONS_LOM_LOBON,
    MONS_CEREBOV,
    MONS_GLOORX_VLOQ,                  //  254
    MONS_MOLLUSC_LORD, //  255 - deprecated, but still referenced in code {dlb}
// BCR - End first batch of uniques.
    MONS_NAGA_MAGE = 260,              //  260
    MONS_NAGA_WARRIOR,
    MONS_ORC_WARLORD,
    MONS_DEEP_ELF_SOLDIER,
    MONS_DEEP_ELF_FIGHTER,
    MONS_DEEP_ELF_KNIGHT,              //  265
    MONS_DEEP_ELF_MAGE,
    MONS_DEEP_ELF_SUMMONER,
    MONS_DEEP_ELF_CONJURER,
    MONS_DEEP_ELF_PRIEST,
    MONS_DEEP_ELF_HIGH_PRIEST,         //  270
    MONS_DEEP_ELF_DEMONOLOGIST,
    MONS_DEEP_ELF_ANNIHILATOR,
    MONS_DEEP_ELF_SORCERER,
    MONS_DEEP_ELF_DEATH_MAGE,
    MONS_BROWN_OOZE,                   //  275
    MONS_AZURE_JELLY,
    MONS_DEATH_OOZE,
    MONS_ACID_BLOB,
    MONS_ROYAL_JELLY,
// BCR - begin second batch of uniques.
    MONS_TERENCE,                      //  280
    MONS_JESSICA,
    MONS_IJYB,
    MONS_SIGMUND,
    MONS_BLORK_THE_ORC,
    MONS_EDMUND,                       //  285
    MONS_PSYCHE,
    MONS_EROLCHA,
    MONS_DONALD,
    MONS_URUG,
    MONS_MICHAEL,                      //  290
    MONS_JOSEPH,
    MONS_SNORG, // was Anita - Snorg is correct 16jan2000 {dlb}
    MONS_ERICA,
    MONS_JOSEPHINE,
    MONS_HAROLD,                       //  295
    MONS_NORBERT,
    MONS_JOZEF,
    MONS_AGNES,
    MONS_MAUD,
    MONS_LOUISE,                       //  300
    MONS_FRANCIS,
    MONS_FRANCES,
    MONS_RUPERT,
    MONS_WAYNE,
    MONS_DUANE,                        //  305
    MONS_XTAHUA,
    MONS_NORRIS,
    MONS_ADOLF,
    MONS_MARGERY,
    MONS_BORIS,                        //  310
// BCR - end second batch of uniques.

    // The Lords of Hell:
    MONS_GERYON = 340,                 //  340
    MONS_DISPATER,
    MONS_ASMODEUS,
    MONS_ANTAEUS,
    MONS_ERESHKIGAL,                   //  344

    MONS_ANCIENT_LICH = 356,           //  356
    MONS_OOZE,                         //  357

    MONS_VAULT_GUARD = 360,            //  360
    MONS_CURSE_SKULL,
    MONS_VAMPIRE_KNIGHT,
    MONS_VAMPIRE_MAGE,
    MONS_SHINING_EYE,
    MONS_ORB_GUARDIAN,                 //  365
    MONS_DAEVA,
    MONS_SPECTRAL_THING,
    MONS_GREATER_NAGA,
    MONS_SKELETAL_DRAGON,
    MONS_TENTACLED_MONSTROSITY,        //  370
    MONS_SPHINX,
    MONS_ROTTING_HULK,
    MONS_GUARDIAN_MUMMY,
    MONS_GREATER_MUMMY,
    MONS_MUMMY_PRIEST,                 //  375
    MONS_CENTAUR_WARRIOR,
    MONS_YAKTAUR_CAPTAIN,
    MONS_KILLER_KLOWN,
    MONS_ELECTRIC_GOLEM, // replacing the guardian robot -- bwr
    MONS_BALL_LIGHTNING, // replacing the dorgi -- bwr
    MONS_ORB_OF_FIRE,    // Swords renamed to fit -- bwr
    MONS_QUOKKA,         // Quokka are a type of wallaby, returned -- bwr 382
    

    MONS_EYE_OF_DEVASTATION = 385,     //  385
    MONS_MOTH_OF_WRATH,
    MONS_DEATH_COB,
    MONS_CURSE_TOE,
    MONS_GOLD_MIMIC,
    MONS_WEAPON_MIMIC,                 //  390
    MONS_ARMOUR_MIMIC,
    MONS_SCROLL_MIMIC,
    MONS_POTION_MIMIC,
    MONS_HELL_HOG,
    MONS_SERPENT_OF_HELL,              //  395
    MONS_BOGGART,
    MONS_QUICKSILVER_DRAGON,
    MONS_IRON_DRAGON,
    MONS_SKELETAL_WARRIOR,             //  399
    MONS_PLAYER_GHOST,                 //  400
    MONS_PANDEMONIUM_DEMON,            //  401

    MONS_GIANT_NEWT,                   //  402
    MONS_GIANT_GECKO,                  //  403
    MONS_GIANT_IGUANA,                 //  404
    MONS_GILA_MONSTER,                 //  405
    MONS_KOMODO_DRAGON,                //  406

    // Lava monsters:
    MONS_LAVA_WORM = 420,              //  420
    MONS_LAVA_FISH,
    MONS_LAVA_SNAKE,
    MONS_SALAMANDER,                   //  423 mv: was another lava thing

    // Water monsters:
    MONS_BIG_FISH = 430,               //  430
    MONS_GIANT_GOLDFISH,
    MONS_ELECTRICAL_EEL,
    MONS_JELLYFISH,
    MONS_WATER_ELEMENTAL,
    MONS_SWAMP_WORM,                   //  435

    NUM_MONSTERS,                      // used for polymorph 
    RANDOM_MONSTER = 1000, // used to distinguish between a random monster and using program bugs for error trapping {dlb}
    WANDERING_MONSTER = 2500 // only used in monster placement routines - forced limit checks {dlb}

};

enum MONSTER_BEHAVIOUR                  // create_monster()
{
    BEH_SLEEP,                         //    0
    BEH_WANDER,
    BEH_SEEK,
    BEH_FLEE,
    BEH_CORNERED,
    NUM_BEHAVIOURS,                    //  max # of legal states
    BEH_CHARMED,                       //  hostile-but-charmed; create only
    BEH_FRIENDLY,                      //  used during creation only
    BEH_HOSTILE,                       //  creation only
    BEH_GOD_GIFT                       //  creation only
};

enum MONSTER_ATTITUDES
{
    ATT_HOSTILE,                       // 0, default in most cases
    ATT_FRIENDLY,                      // created friendly (or tamed?)
    ATT_NEUTRAL
};

enum MONSTER_EVENTS
{
    ME_EVAL,                            // 0, evaluate monster AI state
    ME_DISTURB,                         // noisy
    ME_ANNOY,                           // annoy at range
    ME_ALERT,                           // alert to presence
    ME_WHACK,                           // physical attack
    ME_SCARE,                           // frighten monster
    ME_CORNERED                         // cannot flee
};

#if 0
// Obsolete... use mons_charclass()
enum MONSTER_CATEGORIES
{
    MC_MIMIC, //    0
    NUM_MC,
    MC_UNSPECIFIED = 255 // keep at end !!! mind the upper limit of 255 {dlb}
};
#endif

// Note: These are currently stored in chars!!!
// Need to fix struct monsters and the savefile if you want more.
enum MONSTER_FLAGS
{
    MF_CREATED_FRIENDLY   = 0x01,  // no benefit from killing
    MF_GOD_GIFT           = 0x02,  // player not penalized by its death
    MF_BATTY              = 0x04,  // flutters like a bat
    MF_JUST_SUMMONED      = 0x08,  // monster skips next available action
    MF_TAKING_STAIRS      = 0x10,  // is following player through stairs

    MF_UNUSED_I           = 0x20,
    MF_UNUSED_II          = 0x40,
    MF_UNUSED_III         = 0x80
};

enum MONSTER_DAMAGE
{
    MDAM_OKAY,
    MDAM_LIGHTLY_DAMAGED,
    MDAM_MODERATELY_DAMAGED,
    MDAM_HEAVILY_DAMAGED,
    MDAM_HORRIBLY_DAMAGED,
    MDAM_ALMOST_DEAD,
    MDAM_DEAD
};

enum MONSTER_DESCRIPTORS // things that cross categorical lines {dlb}
{
    MDSC_LEAVES_HIDE,                  //    0
    MDSC_REGENERATES,
    MDSC_NOMSG_WOUNDS
};

enum MONSTER_HOLINESS // matches (char) H_foo in mon-util.h, see: monster_holiness()
{
    MH_HOLY,                           //    0 - was -1
    MH_NATURAL,                        //    1 - was 0
    MH_UNDEAD,                         //    2 - was 1
    MH_DEMONIC,                        //    3 - was 2
    MH_NONLIVING,                      //    golems and other constructs
    MH_PLANT                           //    plants
};

enum MONSTER_INVENTORY_SLOTS           // (int) menv[].inv[]
{
    MSLOT_WEAPON,
    MSLOT_MISSILE, // although it is a second weapon for MONS_TWO_HEADED_OGRE - how to reconcile cleanly? {dlb}
    MSLOT_ARMOUR,
    MSLOT_MISCELLANY, //mv: used for misc. obj. (7 Aug 2001)
    MSLOT_POTION, // mv: now used only for potions (7 Aug 2001)
    MSLOT_WAND, //
    MSLOT_SCROLL,
    MSLOT_GOLD, //mv: used for money :) (7 Aug 2001)
    NUM_MONSTER_SLOTS = 8 // value must remain 8 for savefile compatibility {dlb}
};

enum MONSTER_ITEM_USE
{
    MONUSE_NOTHING,
    MONUSE_EATS_ITEMS,
    MONUSE_OPEN_DOORS,
    MONUSE_STARTING_EQUIPMENT,
    MONUSE_WEAPONS_ARMOUR
};

enum MONSTER_SPELLS   // mons_cast(), mspell_list[], mons_spells()
{
    MS_MMISSILE,                       //    0
    MS_FLAME,
    MS_FROST,
    MS_PARALYSIS,
    MS_SLOW,
    MS_HASTE,                          //    5
    MS_CONFUSE,    //    6 - do not deprecate!!! 13jan2000 {dlb}
    MS_VENOM_BOLT,
    MS_FIRE_BOLT,
    MS_COLD_BOLT,
    MS_LIGHTNING_BOLT,                 //   10
    MS_INVIS,
    MS_FIREBALL,
    MS_HEAL,
    MS_TELEPORT,
    MS_TELEPORT_OTHER,                 //   15
    MS_BLINK,
    MS_CRYSTAL_SPEAR,
    MS_DIG,
    MS_NEGATIVE_BOLT,
    MS_HELLFIRE_BURST,                 //   20
    MS_VAMPIRE_SUMMON,
    MS_ORB_ENERGY,
    MS_BRAIN_FEED,
    MS_LEVEL_SUMMON,
    MS_FAKE_RAKSHASA_SUMMON,           //   25
    MS_STEAM_BALL,
    MS_SUMMON_DEMON,
    MS_ANIMATE_DEAD,
    MS_PAIN,
    MS_SMITE,                          //   30
    MS_STICKY_FLAME,
    MS_POISON_BLAST,
    MS_SUMMON_DEMON_LESSER,
    MS_SUMMON_UFETUBUS,
    MS_PURPLE_BLAST,                   //   35
    MS_SUMMON_BEAST, // MS_GERYON was not descriptive - renamed 13jan2000 {dlb}
    MS_ENERGY_BOLT,
    MS_STING,
    MS_IRON_BOLT,
    MS_STONE_ARROW,                    //   40
    MS_POISON_SPLASH,
    MS_SUMMON_UNDEAD,
    MS_MUTATION,                       //   43
    MS_CANTRIP,
    MS_DISINTEGRATE,                   //   45
    MS_MARSH_GAS,
    MS_QUICKSILVER_BOLT,
    MS_TORMENT,
    MS_HELLFIRE,
    MS_METAL_SPLINTERS,                //   50
    MS_SUMMON_DEMON_GREATER, // [foo]_1 was confusing - renamed 13jan2000 {dlb}
    MS_BANISHMENT,
    NUM_MONSTER_SPELLS,
    MS_NO_SPELL = 100
};

// XXX: These still need to be applied in mon-data.h
enum MONSTER_SPELL_TEMPLATES
{
    MST_ORC_WIZARD_I     = 0,
    MST_ORC_WIZARD_II,
    MST_ORC_WIZARD_III,
    MST_GUARDIAN_NAGA    = 10,
    MST_LICH_I           = 20,
    MST_LICH_II,
    MST_LICH_III,
    MST_LICH_IV,
    MST_BURNING_DEVIL    = 30,
    MST_VAMPIRE          = 40,
    MST_VAMPIRE_KNIGHT,
    MST_VAMPIRE_MAGE,
    MST_EFREET           = 50,
    MST_BRAIN_WORM       = 52,
    MST_GIANT_ORANGE_BRAIN,
    MST_RAKSHASA,
    MST_GREAT_ORB_OF_EYES,              // 55
    MST_ORC_SORCERER,
    MST_STEAM_DRAGON,
    MST_HELL_KNIGHT_I,
    MST_HELL_KNIGHT_II,
    MST_NECROMANCER_I,                  // 60
    MST_NECROMANCER_II,
    MST_WIZARD_I,
    MST_WIZARD_II,
    MST_WIZARD_III,
    MST_WIZARD_IV,                      // 65
    MST_WIZARD_V,
    MST_ORC_PRIEST,
    MST_ORC_HIGH_PRIEST,
    MST_MOTTLED_DRAGON,
    MST_ICE_FIEND,                      // 70
    MST_SHADOW_FIEND,
    MST_TORMENTOR,
    MST_STORM_DRAGON,
    MST_WHITE_IMP,
    MST_YNOXINUL,                       // 75
    MST_NEQOXEC,
    MST_HELLWING,
    MST_SMOKE_DEMON,
    MST_CACODEMON,
    MST_GREEN_DEATH,                    // 80
    MST_BALRUG,
    MST_BLUE_DEATH,
    MST_GERYON,
    MST_DISPATER,
    MST_ASMODEUS,                       // 85
    MST_ERESHKIGAL,
    MST_ANTAEUS,                        // 87
    MST_MNOLEG                = 90,
    MST_LOM_LOBON,
    MST_CEREBOV,
    MST_GLOORX_VLOQ,
    MST_TITAN,
    MST_GOLDEN_DRAGON,                  // 95
    MST_DEEP_ELF_SUMMONER,
    MST_DEEP_ELF_CONJURER_I,
    MST_DEEP_ELF_CONJURER_II,
    MST_DEEP_ELF_PRIEST,
    MST_DEEP_ELF_HIGH_PRIEST,           // 100
    MST_DEEP_ELF_DEMONOLOGIST,
    MST_DEEP_ELF_ANNIHILATOR,
    MST_DEEP_ELF_SORCERER,
    MST_DEEP_ELF_DEATH_MAGE,
    MST_KOBOLD_DEMONOLOGIST,            // 105
    MST_NAGA,
    MST_NAGA_MAGE,
    MST_CURSE_SKULL,
    MST_SHINING_EYE,
    MST_FROST_GIANT,                    // 110
    MST_ANGEL,
    MST_DAEVA,
    MST_SHADOW_DRAGON,
    MST_SPHINX,
    MST_MUMMY,                          // 115
    MST_ELECTRIC_GOLEM,
    MST_ORB_OF_FIRE,
    MST_SHADOW_IMP,
    MST_GHOST,
    MST_HELL_HOG,                       // 120
    MST_SWAMP_DRAGON,
    MST_SWAMP_DRAKE,
    MST_SERPENT_OF_HELL,
    MST_BOGGART,
    MST_EYE_OF_DEVASTATION,             // 125
    MST_QUICKSILVER_DRAGON,
    MST_IRON_DRAGON,                    // 127
    MST_SKELETAL_WARRIOR,
    NUM_MSTYPES,
    MST_NO_SPELLS = 250
};

enum MUTATIONS
{
    MUT_TOUGH_SKIN,                    //    0
    MUT_STRONG,
    MUT_CLEVER,
    MUT_AGILE,
    MUT_GREEN_SCALES,
    MUT_BLACK_SCALES,                  //    5
    MUT_GREY_SCALES,
    MUT_BONEY_PLATES,
    MUT_REPULSION_FIELD,
    MUT_POISON_RESISTANCE,
    MUT_CARNIVOROUS,                   //   10
    MUT_HERBIVOROUS,
    MUT_HEAT_RESISTANCE,
    MUT_COLD_RESISTANCE,
    MUT_SHOCK_RESISTANCE,
    MUT_REGENERATION,                  //   15
    MUT_FAST_METABOLISM,
    MUT_SLOW_METABOLISM,
    MUT_WEAK,
    MUT_DOPEY,
    MUT_CLUMSY,                        //   20
    MUT_TELEPORT_CONTROL,
    MUT_TELEPORT,
    MUT_MAGIC_RESISTANCE,
    MUT_FAST,
    MUT_ACUTE_VISION,                  //   25
    MUT_DEFORMED,
    MUT_TELEPORT_AT_WILL,
    MUT_SPIT_POISON,
    MUT_MAPPING,
    MUT_BREATHE_FLAMES,                //   30
    MUT_BLINK,
    MUT_HORNS,
    MUT_STRONG_STIFF,
    MUT_FLEXIBLE_WEAK,
    MUT_LOST,                          //   35
    MUT_CLARITY,
    MUT_BERSERK,
    MUT_DETERIORATION,
    MUT_BLURRY_VISION,
    MUT_MUTATION_RESISTANCE,           //   40
    MUT_FRAIL,
    MUT_ROBUST,
    MUT_TORMENT_RESISTANCE,
    MUT_NEGATIVE_ENERGY_RESISTANCE,
    MUT_SUMMON_MINOR_DEMONS,           //   45
    MUT_SUMMON_DEMONS,
    MUT_HURL_HELLFIRE,
    MUT_CALL_TORMENT,
    MUT_RAISE_DEAD,
    MUT_CONTROL_DEMONS,                //   50
    MUT_PANDEMONIUM,
    MUT_DEATH_STRENGTH,
    MUT_CHANNEL_HELL,
    MUT_DRAIN_LIFE,
    MUT_THROW_FLAMES,                  //   55
    MUT_THROW_FROST,
    MUT_SMITE,                         //   57
    MUT_CLAWS,                         //jmf: added
    MUT_HOOVES,                        //jmf: etc.
    MUT_BREATHE_POISON,                //   60
    MUT_STINGER,
    MUT_BIG_WINGS,
    MUT_BLUE_MARKS, //   63 - decorative, as in "mark of the devil"
    MUT_GREEN_MARKS,                   //   64
    MUT_RED_SCALES = 70,               //   70
    MUT_NACREOUS_SCALES,
    MUT_GREY2_SCALES,
    MUT_METALLIC_SCALES,
    MUT_BLACK2_SCALES,
    MUT_WHITE_SCALES,                  //   75
    MUT_YELLOW_SCALES,
    MUT_BROWN_SCALES,
    MUT_BLUE_SCALES,
    MUT_PURPLE_SCALES,
    MUT_SPECKLED_SCALES,               //   80
    MUT_ORANGE_SCALES,
    MUT_INDIGO_SCALES,
    MUT_RED2_SCALES,
    MUT_IRIDESCENT_SCALES,
    MUT_PATTERNED_SCALES,              //   85
    NUM_MUTATIONS
};

enum NAUGHTY_THINGS
{
    NAUGHTY_NECROMANCY = 1, //    1 - using necromancy (spell or device)
    NAUGHTY_UNHOLY, //    2 - using unholy stuff (call imp, summon things)
    NAUGHTY_KILLING, //    3 - killing in the name of a peaceful deity
    NAUGHTY_ATTACK_HOLY,               //    4 - attacking holy things
    NAUGHTY_ATTACK_FRIEND,             //    5 - attacking friendly things
    NAUGHTY_FRIEND_DIES, //    6 - allowing friendly things to die
    NAUGHTY_BUTCHER, //    7 - butchering in the name of a peaceful deity
    NAUGHTY_STABBING,                  //    8 - stabbing
    NAUGHTY_SPELLCASTING,              //    9 - spellcasting
    NAUGHTY_POISON,                    //   10 - poisoning
    NAUGHTY_STIMULANTS, //jmf: next three new, some not yet used
    NAUGHTY_ATE_MEAT,
    NAUGHTY_CREATED_LIFE,
    NUM_NAUGHTY_THINGS
};

enum OBJECT_CLASSES                    // (unsigned char) mitm[].base_type
{
    OBJ_WEAPONS,                       //    0
    OBJ_MISSILES,
    OBJ_ARMOUR,
    OBJ_WANDS,
    OBJ_FOOD,                          //    4
    OBJ_UNKNOWN_I = 5, // (use unknown) labeled as books in invent.cc {dlb}
    OBJ_SCROLLS = 6,                   //    6
    OBJ_JEWELLERY,
    OBJ_POTIONS,                       //    8
    OBJ_UNKNOWN_II = 9, // (use unknown, stackable) labeled as gems in invent.cc {dlb}
    OBJ_BOOKS = 10,                    //   10
    OBJ_STAVES,
    OBJ_ORBS,
    OBJ_MISCELLANY,
    OBJ_CORPSES,
    OBJ_GOLD, // important role as upper limit to chardump::dump_inventory() {dlb}
    OBJ_GEMSTONES, // found in itemname.cc, labeled as miscellaneous in invent.cc {dlb}
    NUM_OBJECT_CLASSES,
    OBJ_UNASSIGNED = 100,              // must remain set to 100 {dlb}
    OBJ_RANDOM = 255 // must remain set to 255 {dlb} - also used
                     // for blanket random sub_type .. see dungeon::items()
};

enum ORBS
{
    ORB_ZOT                            //    0
};

enum POTIONS
{
    POT_HEALING,                       //    0
    POT_HEAL_WOUNDS,
    POT_SPEED,
    POT_MIGHT,
    POT_GAIN_STRENGTH,
    POT_GAIN_DEXTERITY,                //    5
    POT_GAIN_INTELLIGENCE,
    POT_LEVITATION,
    POT_POISON,
    POT_SLOWING,
    POT_PARALYSIS,                     //   10
    POT_CONFUSION,
    POT_INVISIBILITY,
    POT_PORRIDGE,
    POT_DEGENERATION,
    POT_DECAY,                         //   15
    POT_WATER,
    POT_EXPERIENCE,
    POT_MAGIC,
    POT_RESTORE_ABILITIES,
    POT_STRONG_POISON,                 //   20
    POT_BERSERK_RAGE,
    POT_CURE_MUTATION,
    POT_MUTATION,
    NUM_POTIONS
};

enum PRONOUN_TYPE
{
    PRONOUN_CAP,                        // 0
    PRONOUN_NOCAP,                      // 1
    PRONOUN_CAP_POSSESSIVE,             // 2
    PRONOUN_NOCAP_POSSESSIVE,           // 3
    PRONOUN_REFLEXIVE                   // 4 (reflexive is always lowercase)
};

enum PROXIMITY   // proximity to player to create monster
{
    PROX_ANYWHERE,
    PROX_CLOSE_TO_PLAYER,
    PROX_AWAY_FROM_PLAYER,
    PROX_NEAR_STAIRS
};

enum RANDART_PROP
{
    RAP_BRAND,                         //    0
    RAP_AC,
    RAP_EVASION,
    RAP_STRENGTH,
    RAP_INTELLIGENCE,
    RAP_DEXTERITY,                     //    5
    RAP_FIRE,
    RAP_COLD,
    RAP_ELECTRICITY,
    RAP_POISON,
    RAP_NEGATIVE_ENERGY,               //   10
    RAP_MAGIC,
    RAP_EYESIGHT,
    RAP_INVISIBLE,
    RAP_LEVITATE,
    RAP_BLINK,                         //   15
    RAP_CAN_TELEPORT,
    RAP_BERSERK,
    RAP_MAPPING,
    RAP_NOISES,
    RAP_PREVENT_SPELLCASTING,          //   20
    RAP_CAUSE_TELEPORTATION,
    RAP_PREVENT_TELEPORTATION,
    RAP_ANGRY,
    RAP_METABOLISM,
    RAP_MUTAGENIC,                     //   25
    RAP_ACCURACY,
    RAP_DAMAGE,
    RAP_CURSED,
    RAP_STEALTH
};

enum READ_BOOK_ACTION
{
    RBOOK_USE_STAFF,
    RBOOK_MEMORIZE,
    RBOOK_READ_SPELL
};

enum RUN_DIR
{
    RDIR_UP = 0,
    RDIR_UP_RIGHT,
    RDIR_RIGHT,
    RDIR_DOWN_RIGHT,
    RDIR_DOWN,
    RDIR_DOWN_LEFT,
    RDIR_LEFT,
    RDIR_UP_LEFT,
    RDIR_REST
};

enum RUNE_TYPES
{
    // Note: that runes DIS-SWAMP have the same numberic value as the branch
    RUNE_DIS                    = 1,
    RUNE_GEHENNA,
    RUNE_COCYTUS                = 4,
    RUNE_TARTARUS,                  
    RUNE_SLIME_PITS             = 13,
    RUNE_VAULTS,
    RUNE_SNAKE_PIT              = 19,
    RUNE_ELVEN_HALLS,                   // unused  
    RUNE_TOMB,
    RUNE_SWAMP,

    // Runes 50 and 51 are for Pandemonium (general demon) and the Abyss
    RUNE_DEMONIC                = 50,
    RUNE_ABYSSAL,

    // Runes 60-63 correspond to the Pandemonium demonlords, 
    // and are equal to the corresponding vault.
    RUNE_MNOLEG                 = 60,
    RUNE_LOM_LOBON,
    RUNE_CEREBOV,
    RUNE_GLOORX_VLOQ,
    NUM_RUNE_TYPES              // should always be last
};

enum SCORE_FORMAT
{
    SCORE_TERSE,                // one line
    SCORE_REGULAR,              // two lines (name, cause, blank)
    SCORE_VERBOSE               // everything (dates, times, god, etc)
};

enum SCROLLS
{
    SCR_IDENTIFY,                      //    0
    SCR_TELEPORTATION,
    SCR_FEAR,
    SCR_NOISE,
    SCR_REMOVE_CURSE,
    SCR_DETECT_CURSE,                  //   5
    SCR_SUMMONING,
    SCR_ENCHANT_WEAPON_I,
    SCR_ENCHANT_ARMOUR,
    SCR_TORMENT,
    SCR_RANDOM_USELESSNESS,            //   10
    SCR_CURSE_WEAPON,
    SCR_CURSE_ARMOUR,
    SCR_IMMOLATION,
    SCR_BLINKING,
    SCR_PAPER,                         //   15
    SCR_MAGIC_MAPPING,
    SCR_FORGETFULNESS,
    SCR_ACQUIREMENT,
    SCR_ENCHANT_WEAPON_II,
    SCR_VORPALISE_WEAPON,              //   20
    SCR_RECHARGING,
    SCR_ENCHANT_WEAPON_III,
    NUM_SCROLLS
};

enum SHOPS // (unsigned char) env.sh_type[], item_in_shop(), in_a_shop()
{
    SHOP_WEAPON,                       //    0
    SHOP_ARMOUR,
    SHOP_WEAPON_ANTIQUE,
    SHOP_ARMOUR_ANTIQUE,
    SHOP_GENERAL_ANTIQUE,
    SHOP_JEWELLERY,                    //    5
    SHOP_WAND,
    SHOP_BOOK,
    SHOP_FOOD,
    SHOP_DISTILLERY,
    SHOP_SCROLL,                       //   10
    SHOP_GENERAL,
    NUM_SHOPS, // must remain last 'regular' member {dlb}
    SHOP_UNASSIGNED = 100,             // keep set at 100 for now {dlb}
    SHOP_RANDOM = 255                  // keep set at 255 for now {dlb}
};

enum SKILLS
{
    SK_FIGHTING,                       //    0
    SK_SHORT_BLADES,
    SK_LONG_SWORDS,
    SK_UNUSED_1,                       // SK_GREAT_SWORDS - now unused
    SK_AXES,
    SK_MACES_FLAILS,                   //    5
    SK_POLEARMS,
    SK_STAVES,
    SK_SLINGS,
    SK_BOWS,
    SK_CROSSBOWS,                      //   10
    SK_DARTS,
    SK_THROWING,
    SK_ARMOUR,
    SK_DODGING,
    SK_STEALTH,                        //   15
    SK_STABBING,
    SK_SHIELDS,
    SK_TRAPS_DOORS,
    SK_UNARMED_COMBAT,                 //   19
    SK_SPELLCASTING = 25,              //   25
    SK_CONJURATIONS,
    SK_ENCHANTMENTS,
    SK_SUMMONINGS,
    SK_NECROMANCY,
    SK_TRANSLOCATIONS,                 //   30
    SK_TRANSMIGRATION,
    SK_DIVINATIONS,
    SK_FIRE_MAGIC,
    SK_ICE_MAGIC,
    SK_AIR_MAGIC,                      //   35
    SK_EARTH_MAGIC,
    SK_POISON_MAGIC,
    SK_INVOCATIONS,
    SK_EVOCATIONS,
    NUM_SKILLS                         // must remain last member {dlb}
};

enum SPECIAL_ARMOR
{
    SPARM_NORMAL,                      //    0
    SPARM_RUNNING,
    SPARM_FIRE_RESISTANCE,
    SPARM_COLD_RESISTANCE,
    SPARM_POISON_RESISTANCE,
    SPARM_SEE_INVISIBLE,               //    5
    SPARM_DARKNESS,
    SPARM_STRENGTH,
    SPARM_DEXTERITY,
    SPARM_INTELLIGENCE,
    SPARM_PONDEROUSNESS,               //   10
    SPARM_LEVITATION,
    SPARM_MAGIC_RESISTANCE,
    SPARM_PROTECTION,
    SPARM_STEALTH,
    SPARM_RESISTANCE,                  //   15
    SPARM_POSITIVE_ENERGY,
    SPARM_ARCHMAGI,
    SPARM_PRESERVATION,                //   18
    SPARM_RANDART_I = 25, // must remain at 25 for now - how high do they go? {dlb}
    SPARM_RANDART_II = 26,             //   26
    SPARM_RANDART_III = 27,            //   27
    SPARM_RANDART_IV = 28,             //   28
    SPARM_RANDART_V = 29 //   29 - highest value found thus far {dlb}
};

enum SPECIAL_MISSILES // to separate from weapons in general {dlb}
{
    SPMSL_NORMAL,                      //    0
    SPMSL_FLAME,                       //    1
    SPMSL_ICE,                         //    2
    SPMSL_POISONED, //    3 - from poison_ammo() enchantment {dlb}
    SPMSL_POISONED_II,                 //    4
};

enum SPECIAL_ROOMS
{
    SROOM_LAIR_ORC,                    //    0
    SROOM_LAIR_KOBOLD,
    SROOM_TREASURY,
    SROOM_BEEHIVE,
    SROOM_MORGUE,
    NUM_SPECIAL_ROOMS                  //    5 - must remain final member {dlb}
};

enum SPECIAL_RINGS  // jewellery mitm[].special values 
{
    SPRING_RANDART = 200,
    SPRING_UNRANDART = 201
};

enum SPECIAL_WEAPONS // equivalent to (you.inv[].special or mitm[].special) % 30
{
    SPWPN_NORMAL,                      //    0
    SPWPN_FLAMING,
    SPWPN_FREEZING,
    SPWPN_HOLY_WRATH,
    SPWPN_ELECTROCUTION,
    SPWPN_ORC_SLAYING,                 //    5
    SPWPN_VENOM,
    SPWPN_PROTECTION,
    SPWPN_DRAINING,
    SPWPN_SPEED,
    SPWPN_VORPAL,                      //   10
    SPWPN_FLAME,
    SPWPN_FROST,
    SPWPN_VAMPIRICISM,
    SPWPN_DISRUPTION,
    SPWPN_PAIN,                        //   15
    SPWPN_DISTORTION,
    SPWPN_REACHING,                    //   17
    SPWPN_CONFUSE,
    SPWPN_RANDART_I = 25,              //   25
    SPWPN_RANDART_II,
    SPWPN_RANDART_III,
    SPWPN_RANDART_IV,
    SPWPN_RANDART_V,
    NUM_SPECIAL_WEAPONS,
    SPWPN_DUMMY_CRUSHING,        // ONLY TEMPORARY USAGE -- converts to VORPAL

    // everything above this point is a special artefact wield:
    SPWPN_SINGING_SWORD = 181,          //  181
    SPWPN_WRATH_OF_TROG,
    SPWPN_SCYTHE_OF_CURSES,
    SPWPN_MACE_OF_VARIABILITY,
    SPWPN_GLAIVE_OF_PRUNE,              //  185
    SPWPN_SCEPTRE_OF_TORMENT,
    SPWPN_SWORD_OF_ZONGULDROK,

    // these three are not generated randomly {dlb}
    SPWPN_SWORD_OF_CEREBOV,
    SPWPN_STAFF_OF_DISPATER,
    SPWPN_SCEPTRE_OF_ASMODEUS,          //  190

    SPWPN_SWORD_OF_POWER,
    SPWPN_KNIFE_OF_ACCURACY,
    SPWPN_STAFF_OF_OLGREB,
    SPWPN_VAMPIRES_TOOTH,
    SPWPN_STAFF_OF_WUCAD_MU             //  195
};

enum SPECIAL_WIELD                     // you.special_wield
{
    SPWLD_NONE,                        //    0
    SPWLD_SING,
    SPWLD_TROG,
    SPWLD_CURSE,
    SPWLD_VARIABLE,                    //    4
    SPWLD_PRUNE, //    5 - implicit in it_use3::special_wielded() {dlb}
    SPWLD_TORMENT,                     //    6
    SPWLD_ZONGULDROK,
    SPWLD_POWER,
    SPWLD_WUCAD_MU,                    //    9
    SPWLD_OLGREB,                      //   10
    SPWLD_SHADOW = 50,                 //   50
    SPWLD_HUM, //   51 - see it_use3::special_wielded() {dlb}
    SPWLD_CHIME, //   52 - see it_use3::special_wielded() {dlb}
    SPWLD_BECKON, //   53 - see it_use3::special_wielded() {dlb}
    SPWLD_SHOUT //   54 - see it_use3::special_wielded() {dlb}
};

enum SPECIES
{
    SP_HUMAN = 1,                      //    1
    SP_ELF,
    SP_HIGH_ELF,
    SP_GREY_ELF,
    SP_DEEP_ELF,                       //    5
    SP_SLUDGE_ELF,
    SP_HILL_DWARF,
    SP_MOUNTAIN_DWARF,
    SP_HALFLING,
    SP_HILL_ORC,                       //   10
    SP_KOBOLD,
    SP_MUMMY,
    SP_NAGA,
    SP_GNOME,
    SP_OGRE,                           //   15
    SP_TROLL,
    SP_OGRE_MAGE,
    SP_RED_DRACONIAN,
    SP_WHITE_DRACONIAN,
    SP_GREEN_DRACONIAN,                //   20
    SP_GOLDEN_DRACONIAN,
    SP_GREY_DRACONIAN,
    SP_BLACK_DRACONIAN,
    SP_PURPLE_DRACONIAN,
    SP_MOTTLED_DRACONIAN,              //   25
    SP_PALE_DRACONIAN,
    SP_UNK0_DRACONIAN,
    SP_UNK1_DRACONIAN,
    SP_UNK2_DRACONIAN,
    SP_CENTAUR,                        //   30
    SP_DEMIGOD,
    SP_SPRIGGAN,
    SP_MINOTAUR,
    SP_DEMONSPAWN,
    SP_GHOUL,                          //   35
    SP_KENKU,
    SP_MERFOLK,
    NUM_SPECIES,                       // always after the last species

    SP_UNKNOWN  = 100
};

enum SPELLS
{
    SPELL_IDENTIFY,                    //    0
    SPELL_TELEPORT_SELF,
    SPELL_CAUSE_FEAR,
    SPELL_CREATE_NOISE,
    SPELL_REMOVE_CURSE,
    SPELL_MAGIC_DART,                  //    5
    SPELL_FIREBALL,
    SPELL_SWAP,
    SPELL_APPORTATION,
    SPELL_TWIST,
    SPELL_FAR_STRIKE,                  //   10
    SPELL_DELAYED_FIREBALL,
    SPELL_STRIKING,
    SPELL_CONJURE_FLAME,
    SPELL_DIG,
    SPELL_BOLT_OF_FIRE,                //   15
    SPELL_BOLT_OF_COLD,
    SPELL_LIGHTNING_BOLT,
    SPELL_BOLT_OF_MAGMA,               //   18
    SPELL_POLYMORPH_OTHER = 20,        //   20
    SPELL_SLOW,
    SPELL_HASTE,
    SPELL_PARALYZE,
    SPELL_CONFUSE,
    SPELL_INVISIBILITY,                //   25
    SPELL_THROW_FLAME,
    SPELL_THROW_FROST,
    SPELL_CONTROLLED_BLINK,
    SPELL_FREEZING_CLOUD,
    SPELL_MEPHITIC_CLOUD,              //   30
    SPELL_RING_OF_FLAMES,
    SPELL_RESTORE_STRENGTH,
    SPELL_RESTORE_INTELLIGENCE,
    SPELL_RESTORE_DEXTERITY,
    SPELL_VENOM_BOLT,                  //   35
    SPELL_OLGREBS_TOXIC_RADIANCE,
    SPELL_TELEPORT_OTHER,
    SPELL_LESSER_HEALING,
    SPELL_GREATER_HEALING,
    SPELL_CURE_POISON_I,               //   40
    SPELL_PURIFICATION,
    SPELL_DEATHS_DOOR,
    SPELL_SELECTIVE_AMNESIA,
    SPELL_MASS_CONFUSION,
    SPELL_SMITING,                     //   45
    SPELL_REPEL_UNDEAD,
    SPELL_HOLY_WORD,
    SPELL_DETECT_CURSE,
    SPELL_SUMMON_SMALL_MAMMAL,
    SPELL_ABJURATION_I,                //   50
    SPELL_SUMMON_SCORPIONS,
    SPELL_LEVITATION,
    SPELL_BOLT_OF_DRAINING,
    SPELL_LEHUDIBS_CRYSTAL_SPEAR,
    SPELL_BOLT_OF_INACCURACY,          //   55
    SPELL_POISONOUS_CLOUD,
    SPELL_FIRE_STORM,
    SPELL_DETECT_TRAPS,
    SPELL_BLINK,
    SPELL_ISKENDERUNS_MYSTIC_BLAST,    //   60
    SPELL_SWARM,
    SPELL_SUMMON_HORRIBLE_THINGS,
    SPELL_ENSLAVEMENT,
    SPELL_MAGIC_MAPPING,
    SPELL_HEAL_OTHER,                  //   65
    SPELL_ANIMATE_DEAD,
    SPELL_PAIN,
    SPELL_EXTENSION,
    SPELL_CONTROL_UNDEAD,
    SPELL_ANIMATE_SKELETON,            //   70
    SPELL_VAMPIRIC_DRAINING,
    SPELL_SUMMON_WRAITHS,
    SPELL_DETECT_ITEMS,
    SPELL_BORGNJORS_REVIVIFICATION,
    SPELL_BURN,                        //   75
    SPELL_FREEZE,
    SPELL_SUMMON_ELEMENTAL,
    SPELL_OZOCUBUS_REFRIGERATION,
    SPELL_STICKY_FLAME,
    SPELL_SUMMON_ICE_BEAST,            //   80
    SPELL_OZOCUBUS_ARMOUR,
    SPELL_CALL_IMP,
    SPELL_REPEL_MISSILES,
    SPELL_BERSERKER_RAGE,
    SPELL_DISPEL_UNDEAD,               //   85
    SPELL_GUARDIAN,
    SPELL_PESTILENCE,
    SPELL_THUNDERBOLT,
    SPELL_FLAME_OF_CLEANSING,
    SPELL_SHINING_LIGHT,               //   90
    SPELL_SUMMON_DAEVA,
    SPELL_ABJURATION_II,
    SPELL_FULSOME_DISTILLATION,        //   93
    SPELL_POISON_ARROW,                //   94
    SPELL_TWISTED_RESURRECTION = 110,  //  110
    SPELL_REGENERATION,
    SPELL_BONE_SHARDS,
    SPELL_BANISHMENT,
    SPELL_CIGOTUVIS_DEGENERATION,
    SPELL_STING,                       //  115
    SPELL_SUBLIMATION_OF_BLOOD,
    SPELL_TUKIMAS_DANCE,
    SPELL_HELLFIRE,
    SPELL_SUMMON_DEMON,
    SPELL_DEMONIC_HORDE,               //  120
    SPELL_SUMMON_GREATER_DEMON,
    SPELL_CORPSE_ROT,
    SPELL_TUKIMAS_VORPAL_BLADE,
    SPELL_FIRE_BRAND,
    SPELL_FREEZING_AURA,               //  125
    SPELL_LETHAL_INFUSION,
    SPELL_CRUSH,
    SPELL_BOLT_OF_IRON,
    SPELL_STONE_ARROW,
    SPELL_TOMB_OF_DOROKLOHE,           //  130
    SPELL_STONEMAIL,
    SPELL_SHOCK,
    SPELL_SWIFTNESS,
    SPELL_FLY,
    SPELL_INSULATION,                  //  135
    SPELL_ORB_OF_ELECTROCUTION,
    SPELL_DETECT_CREATURES,
    SPELL_CURE_POISON_II,
    SPELL_CONTROL_TELEPORT,
    SPELL_POISON_AMMUNITION,           //  140
    SPELL_POISON_WEAPON,
    SPELL_RESIST_POISON,
    SPELL_PROJECTED_NOISE,
    SPELL_ALTER_SELF,
    SPELL_DEBUGGING_RAY,               //  145
    SPELL_RECALL,
    SPELL_PORTAL,
    SPELL_AGONY,
    SPELL_SPIDER_FORM,
    SPELL_DISRUPT,                     //  150
    SPELL_DISINTEGRATE,
    SPELL_BLADE_HANDS,
    SPELL_STATUE_FORM,
    SPELL_ICE_FORM,
    SPELL_DRAGON_FORM,                 //  155
    SPELL_NECROMUTATION,
    SPELL_DEATH_CHANNEL,
    SPELL_SYMBOL_OF_TORMENT,
    SPELL_DEFLECT_MISSILES,
    SPELL_ORB_OF_FRAGMENTATION,        //  160
    SPELL_ICE_BOLT,
    SPELL_ICE_STORM,
    SPELL_ARC,
    SPELL_AIRSTRIKE,
    SPELL_SHADOW_CREATURES,            //  165
    SPELL_CONFUSING_TOUCH,
    SPELL_SURE_BLADE,
//jmf: new spells
    SPELL_FLAME_TONGUE,
    SPELL_PASSWALL,
    SPELL_IGNITE_POISON,               //  170
    SPELL_STICKS_TO_SNAKES,
    SPELL_SUMMON_LARGE_MAMMAL,         // e.g. hound
    SPELL_SUMMON_DRAGON,
    SPELL_TAME_BEASTS,                 // charm/enslave but only animals
    SPELL_SLEEP,                       //  175
    SPELL_MASS_SLEEP,
    SPELL_DETECT_MAGIC,                //jmf: unfinished, perhaps useless
    SPELL_DETECT_SECRET_DOORS,
    SPELL_SEE_INVISIBLE,
    SPELL_FORESCRY,                    //  180
    SPELL_SUMMON_BUTTERFLIES,
    SPELL_WARP_BRAND,
    SPELL_SILENCE,
    SPELL_SHATTER,
    SPELL_DISPERSAL,                   //  185
    SPELL_DISCHARGE,
    SPELL_BEND,
    SPELL_BACKLIGHT,
    SPELL_INTOXICATE,   // confusion but only "smart" creatures
    SPELL_GLAMOUR,      // charm/confuse/sleep but only "smart" creatures 190
    SPELL_EVAPORATE,    // turn a potion into a cloud
    SPELL_ERINGYAS_SURPRISING_BOUQUET, // turn sticks into herbivore food
    SPELL_FRAGMENTATION,               // replacement for "orb of frag"
    SPELL_AIR_WALK,                    // "dematerialize" (air/transmigration)
    SPELL_SANDBLAST,     // mini-frag; can use stones for material comp   195
    SPELL_ROTTING,       // evil god power or necromantic transmigration
    SPELL_SHUGGOTH_SEED, // evil god power or necromantic summoning
    SPELL_MAXWELLS_SILVER_HAMMER,      // vorpal-brand maces etc.
    SPELL_CONDENSATION_SHIELD,         // "shield" of icy vapour
    SPELL_SEMI_CONTROLLED_BLINK,       //jmf: to test effect              200
    SPELL_STONESKIN,
    SPELL_SIMULACRUM,
    SPELL_CONJURE_BALL_LIGHTNING,     // 203 (be wary of 210, see below)
    NUM_SPELLS,
    SPELL_NO_SPELL = 210              //  210 - added 22jan2000 {dlb}
};

enum SPELL_TYPES //jmf: 24jul2000: changed from integer-list to bitfield
{
  SPTYP_NONE           = 0, // "0" is reserved for no type at all {dlb}
  SPTYP_CONJURATION    = 1, // was 11, but only for old typematch routine {dlb}
  SPTYP_ENCHANTMENT    = 1<<1,
  SPTYP_FIRE           = 1<<2,
  SPTYP_ICE            = 1<<3,
  SPTYP_TRANSMIGRATION = 1<<4,
  SPTYP_NECROMANCY     = 1<<5,
  SPTYP_SUMMONING      = 1<<6,
  SPTYP_DIVINATION     = 1<<7,
  SPTYP_TRANSLOCATION  = 1<<8,
  SPTYP_POISON         = 1<<9,
  SPTYP_EARTH          = 1<<10,
  SPTYP_AIR            = 1<<11,
  SPTYP_HOLY           = 1<<12, //jmf: moved to accomodate "random" miscast f/x
  SPTYP_LAST_EXPONENT  = 12,    //jmf: ``NUM_SPELL_TYPES'' kinda useless
  NUM_SPELL_TYPES      = 14,
  SPTYP_RANDOM         = 1<<14
};

enum STATS
{
  STAT_STRENGTH,                     //    0
  STAT_DEXTERITY,
  STAT_INTELLIGENCE,
  NUM_STATS, // added for increase_stats() {dlb}
  STAT_ALL, // must remain after NUM_STATS -- added to handle royal jelly, etc. {dlb}
  STAT_RANDOM = 255 // leave at 255, added for increase_stats() handling {dlb}
};

enum STATUE_TYPES
{
    STATUE_SILVER,
    STATUE_ORANGE_CRYSTAL,
    NUM_STATUE_TYPES
};

enum STATUS_REDRAW_FLAGS
{
    REDRAW_HUNGER         = 0x00000001,
    REDRAW_BURDEN         = 0x00000002,
    REDRAW_LINE_1_MASK    = 0x00000003,

    REDRAW_PRAYER         = 0x00000100,
    REDRAW_REPEL_UNDEAD   = 0x00000200,
    REDRAW_BREATH         = 0x00000400,
    REDRAW_REPEL_MISSILES = 0x00000800,
    REDRAW_REGENERATION   = 0x00001000,
    REDRAW_INSULATION     = 0x00002000,
    REDRAW_FLY            = 0x00004000,
    REDRAW_INVISIBILITY   = 0x00008000,
    REDRAW_LINE_2_MASK    = 0x0000ff00,

    REDRAW_CONFUSION      = 0x00010000,
    REDRAW_POISONED       = 0x00020000,
    REDRAW_LIQUID_FLAMES  = 0x00040000,
    REDRAW_DISEASED       = 0x00080000,
    REDRAW_CONTAMINATED   = 0x00100000,
    REDRAW_SWIFTNESS      = 0x00200000,
    REDRAW_SPEED          = 0x00400000,
    REDRAW_LINE_3_MASK    = 0x007f0000
};

enum STAVES
{
    STAFF_WIZARDRY,                    //    0
    STAFF_POWER,
    STAFF_FIRE,
    STAFF_COLD,
    STAFF_POISON,
    STAFF_ENERGY,                      //    5
    STAFF_DEATH,
    STAFF_CONJURATION,
    STAFF_ENCHANTMENT,
    STAFF_SUMMONING,
    STAFF_SMITING,                     //   10
    STAFF_SPELL_SUMMONING,
    STAFF_DESTRUCTION_I,
    STAFF_DESTRUCTION_II,
    STAFF_DESTRUCTION_III,
    STAFF_DESTRUCTION_IV,              //   15
    STAFF_WARDING,
    STAFF_DISCOVERY,
    STAFF_DEMONOLOGY,                  //   18
    STAFF_STRIKING,                    //   19
    STAFF_AIR = 25,                    //   25
    STAFF_EARTH,
    STAFF_CHANNELING,
    NUM_STAVES                         // must remain last member {dlb}
};

enum SYMBOLS // beam[].type - note that this (and its variants) also accepts values from other enums - confusing {dlb}
{
    SYM_SPACE = ' ',                   //   32
    SYM_FLASK = '!',                   //   33
    SYM_BOLT = '#',                    //   35
    SYM_CHUNK = '%',                   //   37
    SYM_OBJECT = '(',                  //   40 - actually used for books, but... {dlb}
    SYM_WEAPON = ')',                  //   41
    SYM_ZAP = '*',                     //   42
    SYM_BURST = '+',                   //   43
    SYM_STICK = '/',                   //   47
    SYM_TRINKET = '=',                 //   61
    SYM_SCROLL = '?',                  //   63
    SYM_DEBUG = 'X',                   //   88
    SYM_ARMOUR = '[',                  //   91
    SYM_MISSILE = '`'                  //   96
};

enum TAGS   // used during save/load process to identify data blocks
{
    TAG_VERSION = 0,                    // should NEVER be read in!
    TAG_YOU = 1,                        // 'you' structure
    TAG_YOU_ITEMS,                      // your items
    TAG_YOU_DUNGEON,                    // dungeon specs (stairs, branches, features)
    TAG_LEVEL,                          // various grids & clouds
    TAG_LEVEL_ITEMS,                    // items/traps
    TAG_LEVEL_MONSTERS,                 // monsters
    TAG_GHOST,                          // ghost
    TAG_LEVEL_ATTITUDE,                 // monster attitudes
    NUM_TAGS
};

enum TAGTYPES   // file types supported by tag system
{
    TAGTYPE_PLAYER=0,           // Foo.sav
    TAGTYPE_LEVEL,              // Foo.00a, .01a, etc.
    TAGTYPE_GHOST               // bones.xxx
};


enum TRANSFORMATIONS
{
    TRAN_NONE,                         //    0
    TRAN_SPIDER,
    TRAN_BLADE_HANDS,
    TRAN_STATUE,
    TRAN_ICE_BEAST,
    TRAN_DRAGON,                       //    5
    TRAN_LICH,
    TRAN_SERPENT_OF_HELL,
    TRAN_AIR,
    NUM_TRANSFORMATIONS                // must remain last member {dlb}
};

enum TRAPS                             // env.trap_type[]
{
    TRAP_DART,                         //    0
    TRAP_ARROW,
    TRAP_SPEAR,
    TRAP_AXE,
    TRAP_TELEPORT,
    TRAP_AMNESIA,                      //    5
    TRAP_BLADE,
    TRAP_BOLT,
    TRAP_ZOT,
    TRAP_NEEDLE,
    NUM_TRAPS,                         // must remain last 'regular' member {dlb}
    TRAP_UNASSIGNED = 100,             // keep set at 100 for now {dlb}
    TRAP_RANDOM = 255                  // set at 255 to avoid potential conflicts {dlb}
};

enum UNARMED_ATTACKS
{
    UNAT_NO_ATTACK,                    //    0
    UNAT_KICK,
    UNAT_HEADBUTT,
    UNAT_TAILSLAP,
    UNAT_PUNCH
};

enum UNDEAD_STATES                     // you.is_undead
{
    US_ALIVE,                          //    0
    US_HUNGRY_DEAD,
    US_UNDEAD
};

enum UNIQUE_ITEM_STATUS
{
    UNIQ_NOT_EXISTS = 0,
    UNIQ_EXISTS = 1,
    UNIQ_LOST_IN_ABYSS = 2
};

enum VORPAL_DESCRIPTIONS
{
    DVORP_CRUSHING,                    //    0
    DVORP_SLICING,
    DVORP_PIERCING,
    DVORP_CHOPPING
};

// NOTE:  This order is very special!  Its basically the same as ZAP_*, 
// and there are bits of the code that still use that fact.. see zap_wand().
enum WANDS                             // mitm[].subtype
{
    WAND_FLAME,                        //    0
    WAND_FROST,
    WAND_SLOWING,
    WAND_HASTING,
    WAND_MAGIC_DARTS,
    WAND_HEALING,                      //    5
    WAND_PARALYSIS,
    WAND_FIRE,
    WAND_COLD,
    WAND_CONFUSION,
    WAND_INVISIBILITY,                 //   10
    WAND_DIGGING,
    WAND_FIREBALL,
    WAND_TELEPORTATION,
    WAND_LIGHTNING,
    WAND_POLYMORPH_OTHER,              //   15
    WAND_ENSLAVEMENT,
    WAND_DRAINING,
    WAND_RANDOM_EFFECTS,
    WAND_DISINTEGRATION,
    NUM_WANDS                          // must remain last member {dlb}
};

enum WEAPONS
{
// Base weapons
    WPN_CLUB,                          //    0
    WPN_MACE,
    WPN_FLAIL,
    WPN_DAGGER,
    WPN_MORNINGSTAR,
    WPN_SHORT_SWORD,                   //    5
    WPN_LONG_SWORD,
    WPN_GREAT_SWORD,
    WPN_SCIMITAR,
    WPN_HAND_AXE,
    WPN_BATTLEAXE,                     //   10
    WPN_SPEAR,
    WPN_HALBERD,
    WPN_SLING,
    WPN_BOW,
    WPN_CROSSBOW,                      //   15
    WPN_HAND_CROSSBOW,
    WPN_GLAIVE,
    WPN_QUARTERSTAFF,
// these three not created ordinarily
    WPN_SCYTHE,
    WPN_GIANT_CLUB,                    //   20
    WPN_GIANT_SPIKED_CLUB,
// "rare" weapons - some have special cases and are uncommon
    WPN_EVENINGSTAR,
    WPN_QUICK_BLADE,
    WPN_KATANA,
    WPN_EXECUTIONERS_AXE,              //   25
    WPN_DOUBLE_SWORD,
    WPN_TRIPLE_SWORD,
    WPN_HAMMER,
    WPN_ANCUS,
    WPN_WHIP,                          //   30
    WPN_SABRE,
    WPN_DEMON_BLADE,
    WPN_DEMON_WHIP,
    WPN_DEMON_TRIDENT,
    WPN_BROAD_AXE,                     //   35
// base items (continued)
    WPN_WAR_AXE,
    WPN_TRIDENT,
    WPN_SPIKED_FLAIL,
    WPN_GREAT_MACE,
    WPN_GREAT_FLAIL,                   //   40
    WPN_KNIFE,
    WPN_BLOWGUN,
    WPN_FALCHION,
    NUM_WEAPONS,                       //   44 - must remain last regular member {dlb}
// special cases
    WPN_UNARMED = 500,                 //  500
    WPN_UNKNOWN = 1000,                // 1000
    WPN_RANDOM
};

enum WEAPON_DESCRIPTIONS
{
    DWPN_PLAIN = 0,                    //    0 - added to round out enum {dlb}
    DWPN_RUNED = 1,                    //    1
    DWPN_GLOWING,
    DWPN_ORCISH,
    DWPN_ELVEN,
    DWPN_DWARVEN                       //    5
};

enum WEAPON_PROPERTIES
{
    PWPN_DAMAGE,                       //    0
    PWPN_HIT,
    PWPN_SPEED
};

#ifdef WIZARD

enum WIZARD_OPTIONS
{
    WIZ_NEVER,                         // protect player from accidental wiz
    WIZ_NO,                            // don't start character in wiz mode
    WIZ_YES                            // start character in wiz mode
};

#endif 

enum ZAPS                              // zapping(), zappy()
{
    ZAP_FLAME,                         //    0
    ZAP_FROST,
    ZAP_SLOWING,
    ZAP_HASTING,
    ZAP_MAGIC_DARTS,
    ZAP_HEALING,                       //    5
    ZAP_PARALYSIS,
    ZAP_FIRE,
    ZAP_COLD,
    ZAP_CONFUSION,
    ZAP_INVISIBILITY,                  //   10
    ZAP_DIGGING,
    ZAP_FIREBALL,
    ZAP_TELEPORTATION,
    ZAP_LIGHTNING,
    ZAP_POLYMORPH_OTHER,               //   15
    ZAP_VENOM_BOLT,
    ZAP_NEGATIVE_ENERGY,
    ZAP_CRYSTAL_SPEAR,
    ZAP_BEAM_OF_ENERGY,
    ZAP_MYSTIC_BLAST,                  //   20
    ZAP_ENSLAVEMENT,
    ZAP_PAIN,
    ZAP_STICKY_FLAME,
    ZAP_DISPEL_UNDEAD,
    ZAP_CLEANSING_FLAME,               //   25
    ZAP_BONE_SHARDS,
    ZAP_BANISHMENT,
    ZAP_DEGENERATION,
    ZAP_STING,
    ZAP_HELLFIRE,                      //   30
    ZAP_IRON_BOLT,
    ZAP_STRIKING,
    ZAP_STONE_ARROW,
    ZAP_ELECTRICITY,
    ZAP_ORB_OF_ELECTRICITY,            //   35
    ZAP_SPIT_POISON,
    ZAP_DEBUGGING_RAY,
    ZAP_BREATHE_FIRE,
    ZAP_BREATHE_FROST,
    ZAP_BREATHE_ACID,                  //   40
    ZAP_BREATHE_POISON,
    ZAP_BREATHE_POWER,
    ZAP_ENSLAVE_UNDEAD,
    ZAP_AGONY,
    ZAP_DISRUPTION,                    //   45
    ZAP_DISINTEGRATION,                //   46
    ZAP_ISKS_CROSS, //   47: Isk's Cross -- commented out, deprecated {dlb}
    ZAP_BREATHE_STEAM = 48,            //   48
    ZAP_CONTROL_DEMON,
    ZAP_ORB_OF_FRAGMENTATION,          //   50
    ZAP_ICE_BOLT,
    ZAP_ICE_STORM,
    ZAP_BACKLIGHT,                     //jmf: added next bunch 19mar2000
    ZAP_SLEEP,
    ZAP_FLAME_TONGUE,
    ZAP_SANDBLAST,
    ZAP_SMALL_SANDBLAST,
    ZAP_MAGMA,
    ZAP_POISON_ARROW,
    NUM_ZAPS                           // must remain last member {dlb}
};


#endif // ENUM_H
