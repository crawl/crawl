#pragma once

#include "tag-version.h"

// This is done to avoid duplicating the Depths list and can be
// changed once TAG_MAJOR_VERSION > 35
#define POP_DEPTHS \
{ /* Depths (OOD cap: 13)*/ \
  { -3,  3,  250, SEMI, MONS_RAKSHASA },\
  { -3,  3,  100, SEMI, MONS_OCCULTIST },\
  { -3,  3,  100, SEMI, MONS_NECROMANCER },\
  { -3,  9,  250, PEAK, MONS_FIRE_GIANT },\
  { -3,  9,  260, PEAK, MONS_FROST_GIANT },\
  { -3,  9,   70, PEAK, MONS_HELL_KNIGHT },\
  { -3,  9,  100, PEAK, MONS_VAMPIRE_KNIGHT },\
  {  0,  2,  300, FALL, MONS_UGLY_THING },\
  {  0,  5,  180, FLAT, MONS_DEEP_TROLL_SHAMAN },\
  {  0,  5,  180, FLAT, MONS_DEEP_TROLL_EARTH_MAGE },\
  {  0,  5,  295, FLAT, MONS_FIRE_DRAGON },\
  {  0,  5,  295, FLAT, MONS_ICE_DRAGON },\
  {  0,  5,  250, FLAT, MONS_VERY_UGLY_THING },\
  {  0,  5,  100, FLAT, MONS_GLOWING_ORANGE_BRAIN },\
  {  0,  6,  100, FALL, MONS_VAMPIRE_MAGE },\
  {  0,  6,  200, FALL, MONS_TENGU_WARRIOR },\
  {  0,  6,  100, FALL, MONS_TENGU_CONJURER },\
  {  0,  7,  600, FALL, MONS_STONE_GIANT },\
  {  0,  7,  370, FALL, MONS_ETTIN },\
  {  0,  9,  100, SEMI, MONS_SPARK_WASP },\
  {  0, 11,   60, FLAT, MONS_LICH },\
  {  0, 11,   40, FLAT, MONS_FLAYED_GHOST },\
  {  0, 13,   80, SEMI, MONS_GLOWING_SHAPESHIFTER },\
  {  0, 13,   80, SEMI, MONS_TENGU_REAVER },\
  {  0, 13,   60, SEMI, MONS_SPHINX },\
  {  1,  7,  135, SEMI, MONS_SPRIGGAN_AIR_MAGE },\
  {  1,  7,  185, SEMI, MONS_SPRIGGAN_BERSERKER },\
  {  1,  9,   45, FLAT, MONS_GLASS_EYE },\
  {  2, 13,   45, FLAT, MONS_SPRIGGAN_DEFENDER },\
  {  3, 13,   70, SEMI, MONS_TENTACLED_MONSTROSITY },\
  {  3, 13,   40, FLAT, MONS_STORM_DRAGON },\
  {  3, 13,   40, FLAT, MONS_SHADOW_DRAGON },\
  {  3, 13,   20, FLAT, MONS_QUICKSILVER_DRAGON },\
  {  3, 13,   40, FLAT, MONS_IRON_DRAGON },\
  {  3, 13,   80, SEMI, MONS_GOLDEN_DRAGON },\
  {  3, 15,   45, SEMI, MONS_WYRMHOLE },\
  {  3, 13,   50, RISE, MONS_FROSTBOUND_TOME },\
  {  3, 13,   50, RISE, MONS_EARTHEN_TOME },\
  {  3, 13,   50, RISE, MONS_CRYSTAL_TOME },\
  {  3, 13,   50, RISE, MONS_DIVINE_TOME },\
  {  4, 13,   50, FLAT, MONS_JUGGERNAUT },\
  {  4, 22,   60, SEMI, MONS_ALDERKING },\
  {  4, 13,   50, FLAT, MONS_CAUSTIC_SHRIKE },\
  {  5, 13,   50, FLAT, MONS_TITAN },\
  {  9, 13,   10, FLAT, MONS_ANCIENT_LICH },\
  {  9, 13,   10, FLAT, MONS_DREAD_LICH },\
}

// This list must be in the same order as the branch-type enum values.
static const vector<pop_entry> population[] =
{

{ // Dungeon (OOD cap: 27)

// The weakest of the weak.
  {  1,  3, 1000, FLAT, MONS_BAT },
  {  1,  3, 1000, FLAT, MONS_RAT },
  {  1,  3,  640, FLAT, MONS_FRILLED_LIZARD},

// Pretty weak, but not quite bottom tier.
  {  1,  4, 1000, FLAT, MONS_BALL_PYTHON },
  {  1,  4,  500, FLAT, MONS_GIANT_COCKROACH },
  {  1,  4, 1000, FLAT, MONS_GOBLIN },
  {  1,  4, 1000, FLAT, MONS_KOBOLD },
  {  1,  4, 1000, FLAT, MONS_HOBGOBLIN },
  {  1,  4, 1000, FLAT, MONS_ENDOPLASM },
  {  1,  3,  350, FLAT, MONS_DART_SLUG },
  {  1,  3,  500, FLAT, MONS_QUOKKA },
  {  1,  3,  200, FLAT, MONS_JACKAL },
  {  4,  5,  200, FALL, MONS_JACKAL },
  {  4,  5,  500, FALL, MONS_QUOKKA },
  {  4,  5,  350, FALL, MONS_DART_SLUG },
  {  1,  8,  200, PEAK, MONS_GNOLL },

// Shouldn't show up on D:1.
  {  2,  5,  500, FALL, MONS_RIBBON_WORM },
  {  2,  6, 1000, FLAT, MONS_ADDER },
  {  3,  8, 1000, FLAT, MONS_ORC },
  {  3,  7,  400, PEAK, MONS_BOMBARDIER_BEETLE},

// These historically don't get kills after D:7ish.
  {  4,  7,  800, PEAK, MONS_SCORPION },
  {  4,  7, 1000, PEAK, MONS_HOUND },
  {  4,  7,  500, PEAK, MONS_OGRE },
  {  4,  7, 1000, PEAK, MONS_IGUANA },
  {  4,  7,  350, PEAK, MONS_PHANTOM },
  {  4,  8,  500, PEAK, MONS_JELLY },
  {  4,  8,  350, PEAK, MONS_SLEEPCAP },
  {  4,  8,  350, PEAK, MONS_BLACK_BEAR },

  {  4, 10,  200, PEAK, MONS_GNOLL_BOUDA },
  {  4, 12,  150, PEAK, MONS_ORC_PRIEST },
  {  4, 14,  500, PEAK, MONS_ORC_WIZARD },
  {  4, 14,  350, PEAK, MONS_HOWLER_MONKEY },
  {  4,  7,  350, PEAK, MONS_UFETUBUS },
  {  4,  7,  300, PEAK, MONS_SHADOW_IMP },
  {  5,  8,  500, PEAK, MONS_WHITE_IMP },
  {  5,  8,  500, PEAK, MONS_ICE_BEAST },
  {  5,  9,  400, PEAK, MONS_WATER_MOCCASIN },
  {  5, 13,  600, PEAK, MONS_CENTAUR },
  {  5, 13,  200, PEAK, MONS_GNOLL_SERGEANT },
  {  5,  9,  100, PEAK, MONS_MARROWCUDA },

  {  6,  8,  500, PEAK, MONS_SKY_BEAST },
  {  6,  9, 1000, PEAK, MONS_BULLFROG },
  {  6, 10,  500, PEAK, MONS_WIGHT },
  {  6, 10,  350, PEAK, MONS_STEAM_DRAGON },
  {  6, 11,  500, PEAK, MONS_KOBOLD_BRIGAND },
  {  6, 12,  600, PEAK, MONS_ORC_WARRIOR },
  {  7, 11,  500, PEAK, MONS_KILLER_BEE },
  {  7, 11,  200, PEAK, MONS_HORNET },
  {  7, 11,  350, PEAK, MONS_WYVERN },
  {  8, 12,  800, PEAK, MONS_YAK },
  {  8, 14,  350, PEAK, MONS_ACID_DRAGON },
  {  8, 14,  600, PEAK, MONS_TWO_HEADED_OGRE },
  {  9, 13,  350, PEAK, MONS_SHADOWGHAST },
  {  9, 14,  200, PEAK, MONS_KOBOLD_DEMONOLOGIST },
  {  9, 14,  500, PEAK, MONS_WRAITH },
  {  9, 14,  800, PEAK, MONS_TROLL },
  {  9, 14,  200, PEAK, MONS_VAMPIRE },
  { 10, 13,  350, PEAK, MONS_UNSEEN_HORROR },
  { 10, 13,  200, PEAK, MONS_KOMODO_DRAGON },
  { 10, 14,  350, PEAK, MONS_GARGOYLE },
  { 10, 15,  500, PEAK, MONS_VAMPIRE_MOSQUITO },
  { 11, 18,  800, PEAK, MONS_UGLY_THING },
  { 11, 21,  520, SEMI, MONS_MANTICORE },
  { 11, 21,  825, SEMI, MONS_CYCLOPS },
  { 11, 21,  285, PEAK, MONS_BASILISK },
  { 11, 21,  192, PEAK, MONS_SKELETAL_WARRIOR },
  { 11, 23,  540, SEMI, MONS_BOULDER_BEETLE},
  { 12, 19,  475, SEMI, MONS_DEEP_ELF_FIRE_MAGE },
  { 12, 22,  440, FALL, MONS_SLIME_CREATURE },
  { 13, 20,  150, SEMI, MONS_MELIAI },
  { 13, 21,  192, PEAK, MONS_DEATH_KNIGHT },
  { 13, 21,   89, PEAK, MONS_RAKSHASA },
  { 13, 23,  530, SEMI, MONS_HYDRA },
  { 13, 17,  925, FLAT, MONS_CENTAUR_WARRIOR },
  { 18, 23,  925, FALL, MONS_CENTAUR_WARRIOR },
  { 13, 18,   70, FLAT, MONS_ARCANIST },
  { 13, 19,  365, SEMI, MONS_TENGU_WARRIOR },
  { 13, 19,  205, SEMI, MONS_TENGU_CONJURER },
  { 13, 19,  285, SEMI, MONS_EFREET },
  { 13, 23,  675, SEMI, MONS_SHAPESHIFTER },
  { 13, 27,   89, FALL, MONS_CATOBLEPAS },
  { 13, 22,  245, SEMI, MONS_LAUGHING_SKULL },
  { 14, 22,  240, SEMI, MONS_FREEZING_WRAITH },
  { 14, 22,  270, SEMI, MONS_SIMULACRUM },
  { 14, 22,  115, SEMI, MONS_BOGGART },
  { 14, 27,  192, PEAK, MONS_FIRE_DRAGON },
  { 15, 19,   70, FLAT, MONS_OCCULTIST },
  { 15, 20,  335, SEMI, MONS_YAKTAUR },
  { 15, 24,  345, FALL, MONS_ORC_KNIGHT },
  { 15, 25,  315, FALL, MONS_OGRE_MAGE },
  { 15, 25,  275, PEAK, MONS_WOLF_SPIDER },
  { 16, 24,   89, PEAK, MONS_REDBACK },
  { 16, 27,   92, PEAK, MONS_ICE_DRAGON },
  { 17, 23,   89, PEAK, MONS_VERY_UGLY_THING },
  { 17, 24,  102, SEMI, MONS_GREAT_ORB_OF_EYES },
  { 17, 24,   52, SEMI, MONS_NECROMANCER },
  { 17, 25,  136, SEMI, MONS_DEEP_ELF_KNIGHT },
  { 17, 25,  136, SEMI, MONS_DEEP_ELF_ARCHER },
  { 17, 25,  136, SEMI, MONS_HORNET },
  { 17, 25,   89, PEAK, MONS_FIRE_CRAB },
  { 17, 25,   89, PEAK, MONS_HARPY },
  { 17, 25,  136, SEMI, MONS_DEEP_ELF_AIR_MAGE },
  { 18, 24,   52, SEMI, MONS_GLOWING_ORANGE_BRAIN },
  { 18, 28,  180, PEAK, MONS_FLAYED_GHOST },
  { 19, 25,   35, PEAK, MONS_ORC_HIGH_PRIEST },
  { 19, 26,  136, SEMI, MONS_DEEP_TROLL },
  { 19, 27,   89, RISE, MONS_SPHINX },
  { 20, 30,  310, FALL, MONS_YAKTAUR_CAPTAIN },
  { 20, 30,  136, SEMI, MONS_FIRE_GIANT },
  { 20, 30,  136, SEMI, MONS_FROST_GIANT },
  { 21, 27,   25, PEAK, MONS_ORC_SORCERER },
  { 21, 27,  310, FALL, MONS_STONE_GIANT },
  { 21, 31,  192, PEAK, MONS_ETTIN },
  { 22, 30,  136, SEMI, MONS_DEEP_TROLL_EARTH_MAGE },
  { 23, 27,   89, RISE, MONS_STORM_DRAGON },
  { 23, 27,   28, SEMI, MONS_ORC_WARLORD },
  { 23, 29,   25, PEAK, MONS_HELL_KNIGHT },
  { 23, 31,   89, PEAK, MONS_PHANTASMAL_WARRIOR },
  { 23, 27,  136, FLAT, MONS_GLOWING_SHAPESHIFTER },
  { 23, 27,   22, RISE, MONS_THERMIC_DYNAMO },
  { 24, 27,   22, RISE, MONS_DEEP_ELF_ANNIHILATOR },
  { 24, 27,   22, RISE, MONS_DEEP_ELF_HIGH_PRIEST },
  { 24, 27,   22, RISE, MONS_VAMPIRE_KNIGHT },
  { 24, 27,  136, RISE, MONS_TENGU_REAVER },
  { 24, 27,    8, RISE, MONS_DANCING_WEAPON },
  { 24, 27,   25, FLAT, MONS_WAR_GARGOYLE },
  { 27, 27,   18, FLAT, MONS_SHADOW_DRAGON },
  { 27, 27,    8, FLAT, MONS_IRON_DRAGON },
},

{ // Temple
},

{ // Orcish Mines
  {  1,  4,  190, FLAT, MONS_ORC_WARRIOR },
  {  1,  4,   24, FLAT, MONS_ORC_PRIEST },
  {  1,  4,   24, FLAT, MONS_ORC_WIZARD },
  {  1,  4,   24, FLAT, MONS_ORC_KNIGHT },
  {  1,  4,    8, FLAT, MONS_ORC_HIGH_PRIEST },
  {  1,  4,   24, FLAT, MONS_ORC_SORCERER },
  {  1,  4,    8, FLAT, MONS_ORC_WARLORD },
  {  1,  4,   40, FLAT, MONS_WARG },
  {  1,  4,   18, FLAT, MONS_OBSIDIAN_BAT },
  {  1,  4,   32, FLAT, MONS_KOBOLD_BLASTMINER },
  {  1,  4,   80, FLAT, MONS_OGRE },
  {  1,  4,    8, FLAT, MONS_TWO_HEADED_OGRE },
  {  1,  4,    2, FLAT, MONS_OGRE_MAGE },
  {  1,  4,   32, FLAT, MONS_TROLL },
  {  1,  4,    4, FLAT, MONS_CYCLOPS },
  {  1,  4,    4, FLAT, MONS_ETTIN },
  {  3,  4,    4, FLAT, MONS_STONE_GIANT },
},

{ // Elven Halls (OOD cap: 7)
  {  1,  5,   50, FLAT, MONS_ORC_HIGH_PRIEST },
  {  1,  6,   50, FLAT, MONS_ORC_SORCERER },
  {  1,  7, 1360, FLAT, MONS_DEEP_ELF_AIR_MAGE },
  {  1,  7, 1360, FLAT, MONS_DEEP_ELF_FIRE_MAGE },
  {  1,  7, 1360, FLAT, MONS_DANCING_WEAPON },
  {  1,  7,  925, FLAT, MONS_DEEP_ELF_KNIGHT },
  {  1,  7,  925, FLAT, MONS_DEEP_ELF_ARCHER },
  {  1,  7,  320, FLAT, MONS_FIRE_ELEMENTAL },
  {  1,  7,  320, FLAT, MONS_WATER_ELEMENTAL },
  {  1,  7,  320, FLAT, MONS_AIR_ELEMENTAL },
  {  1,  7,  320, FLAT, MONS_EARTH_ELEMENTAL },
  { -1,  8,  340, SEMI, MONS_THERMIC_DYNAMO },
  {  1,  8,   65, SEMI, MONS_DEEP_ELF_SORCERER },
  {  1,  7,   40, SEMI, MONS_DEEP_ELF_ANNIHILATOR },
  {  1,  8,   65, SEMI, MONS_DEEP_ELF_DEMONOLOGIST },
  {  1,  7,   40, SEMI, MONS_DEEP_ELF_HIGH_PRIEST },
  {  1,  8,   65, SEMI, MONS_DEEP_ELF_ELEMENTALIST },
  {  0,  7,   45, SEMI, MONS_DEEP_ELF_DEATH_MAGE },
  {  6,  7,   10, RISE, MONS_DEEP_ELF_BLADEMASTER },
  {  6,  7,   10, RISE, MONS_DEEP_ELF_MASTER_ARCHER },
  {  1,  6,  150, FLAT, MONS_SHAPESHIFTER },
  {  1,  6,   65, SEMI, MONS_GLOWING_SHAPESHIFTER },
},
#if TAG_MAJOR_VERSION == 34
{ // Dwarven Hall
  {  1,  1, 1000, FLAT, MONS_DEEP_DWARF },
  {  1,  1,  690, FLAT, MONS_DEATH_KNIGHT },
  {  1,  1,    3, FLAT, MONS_DEEP_TROLL },
  {  1,  1,    3, FLAT, MONS_DEEP_TROLL_EARTH_MAGE },
  {  1,  1,    3, FLAT, MONS_DEEP_TROLL_SHAMAN },
  {  1,  1,    8, FLAT, MONS_STONE_GIANT },
  {  1,  1,    8, FLAT, MONS_FIRE_GIANT },
  {  1,  1,    8, FLAT, MONS_FROST_GIANT },
  {  1,  1,  192, FLAT, MONS_WRAITH },
  {  1,  1,    3, FLAT, MONS_SHADOW_WRAITH },
  {  1,  1,    8, FLAT, MONS_EIDOLON },
  {  1,  1,    8, FLAT, MONS_PHANTASMAL_WARRIOR },
},
#endif

{ // Lair (OOD cap: 11)
  { -2,  4,  100, SEMI, MONS_BLACK_BEAR },
  { -2,  6,  310, SEMI, MONS_BASILISK },
  { -1,  3,   24, PEAK, MONS_SCORPION },
  { -1,  5,  310, SEMI, MONS_WOLF },
  { -1,  8,  290, PEAK, MONS_RIME_DRAKE },
  { -1, 10,  380, SEMI, MONS_BLACK_MAMBA },
  {  0,  4, 1000, FLAT, MONS_BULLFROG },
  {  0,  5,  990, FLAT, MONS_WATER_MOCCASIN },
  {  0,  5,  565, FLAT, MONS_YAK },
  {  0,  6,  340, PEAK, MONS_WYVERN },
  {  0,  7,  380, SEMI, MONS_BLINK_FROG },
  {  0,  5,  900, FLAT, MONS_CANE_TOAD },
  {  0,  5,  615, FLAT, MONS_KOMODO_DRAGON },
  {  0,  7,   50, PEAK, MONS_HELL_RAT },
  {  0,  7,  150, SEMI, MONS_POLAR_BEAR },
  {  1,  4,   16, PEAK, MONS_HORNET },
  {  1,  4,   50, PEAK, MONS_STEAM_DRAGON },
  {  1,  6,   25, PEAK, MONS_OKLOB_PLANT },
  {  1,  8,  290, PEAK, MONS_ELEPHANT },
  {  1,  9,  210, RISE, MONS_DEATH_YAK },
  {  1, 10,   70, SEMI, MONS_CATOBLEPAS },
  {  1, 15,   70, SEMI, MONS_BOULDER_BEETLE },
  {  1, 15,   85, SEMI, MONS_TORPOR_SNAIL },
  {  2,  7,   75, SEMI, MONS_POLAR_BEAR },
  {  2,  7,  200, SEMI, MONS_SKYSHARK },
  {  2,  9,   50, PEAK, MONS_LINDWURM },
  {  2, 10,  400, SEMI, MONS_HYDRA },
  {  3, 11,  140, SEMI, MONS_DREAM_SHEEP },
  {  3, 15,   36, RISE, MONS_FIRE_CRAB },
  {  6,  9,  160, FLAT, MONS_CANE_TOAD },
  {  6,  9,  160, FLAT, MONS_KOMODO_DRAGON },
  { 10, 11,   32, FLAT, MONS_DIRE_ELEPHANT },
  { 10, 11,   32, FLAT, MONS_ALLIGATOR },
  { 10, 11,   32, FLAT, MONS_MANTICORE },
  { 10, 11,   32, FLAT, MONS_ANACONDA },
  { 10, 11,   32, FLAT, MONS_WOLF_SPIDER },
},

{ // Swamp
  // Trying to keep total around 10000 on each floor, roughly.
  // Easy enemies:
  {  1,  4,   500, FALL, MONS_VAMPIRE_MOSQUITO },
  {  1,  4,   500, FALL, MONS_SWAMP_DRAKE },
  {  1,  4,   500, FALL, MONS_SPRIGGAN_RIDER },
  {  1,  4,   500, FLAT, MONS_SPRIGGAN_RIDER },
  {  1,  4,   600, FALL, MONS_TYRANT_LEECH },
  {  1,  4,   500, FLAT, MONS_TYRANT_LEECH },
  {  1,  4,   700, FALL, MONS_GOLIATH_FROG  },
  {  1,  4,   500, FLAT, MONS_GOLIATH_FROG },
  {  1,  4,   300, FALL, MONS_BLOATED_HUSK },
  {  1,  4,   300, FLAT, MONS_BLOATED_HUSK },
  {  1,  4,   800, FALL, MONS_BOG_BODY },
  {  1,  4,   200, FLAT, MONS_BOG_BODY },
  // Harder enemies:
  {  1,  4,   400, FLAT, MONS_SPRIGGAN_DRUID },
  {  1,  4,   500, RISE, MONS_SPRIGGAN_DRUID },
  {  1,  4,   200, FLAT, MONS_GHOST_CRAB },
  {  1,  4,   300, RISE, MONS_GHOST_CRAB },
  {  1,  4,   500, FLAT, MONS_WILL_O_THE_WISP },
  {  1,  4,   400, RISE, MONS_WILL_O_THE_WISP },
  {  1,  4,   600, FLAT, MONS_ELEIONOMA },
  {  1,  4,   400, RISE, MONS_ELEIONOMA },
  {  1,  4,   800, FLAT, MONS_HYDRA },
  {  1,  4,   500, RISE, MONS_HYDRA },
  {  1,  4,   400, FLAT, MONS_SWAMP_DRAGON },
  {  1,  4,   600, RISE, MONS_SWAMP_DRAGON },
  {  1,  4,   600, FLAT, MONS_BUNYIP },
  {  1,  4,   400, RISE, MONS_BUNYIP },
  {  1,  4,   500, FLAT, MONS_ALLIGATOR },
  {  1,  4,   400, RISE, MONS_ALLIGATOR },
  // Top-tier threats:
  {  1,  4,   33, FLAT, MONS_THORN_HUNTER },
  {  1,  4,  133, RISE, MONS_THORN_HUNTER },
  {  1,  4,   33, FLAT, MONS_SHAMBLING_MANGROVE },
  {  1,  4,  133, RISE, MONS_SHAMBLING_MANGROVE },
  {  1,  4,   29, FLAT, MONS_FENSTRIDER_WITCH },
  {  1,  4,  129, RISE, MONS_FENSTRIDER_WITCH },
  {  1,  4,    5, FLAT, MONS_TENTACLED_MONSTROSITY },
  {  1,  4,    5, RISE, MONS_TENTACLED_MONSTROSITY },
  // OODs
  {  5,  7, 1000, FLAT, MONS_FENSTRIDER_WITCH },
  {  5,  7, 1500, FLAT, MONS_SHAMBLING_MANGROVE },
  {  5,  7, 1500, FLAT, MONS_THORN_HUNTER },
  {  5,  7, 1500, FLAT, MONS_TENTACLED_MONSTROSITY },
  {  5,  7, 1500, FLAT, MONS_GHOUL },
  {  5,  7, 1000, FLAT, MONS_SPRIGGAN_AIR_MAGE },
  {  5,  7, 1000, FLAT, MONS_SPRIGGAN_BERSERKER },
  {  5,  7, 1000, FLAT, MONS_DEATH_DRAKE }, // not that bad but very mean
},

{ // Shoals
  {  0,  3,   89, SEMI, MONS_CENTAUR_WARRIOR },
  {  0,  3,  355, SEMI, MONS_FAUN },
  {  0,  3,  400, SEMI, MONS_SKYSHARK },
  {  0,  6,  300, SEMI, MONS_WATER_NYMPH },
  {  0,  6,  170, SEMI, MONS_MERFOLK_AVATAR },
  {  0,  6,  110, SEMI, MONS_CYCLOPS },
  {  0,  6,   73, SEMI, MONS_KRAKEN },
  {  0,  7,  265, SEMI, MONS_MERFOLK_IMPALER },
  {  0,  7,  125, SEMI, MONS_MERFOLK_AQUAMANCER },
  {  1,  4,  735, FLAT, MONS_MERFOLK },
  {  1,  4,  500, FLAT, MONS_SEA_SNAKE },
  {  1,  4,  410, FLAT, MONS_SNAPPING_TURTLE },
  {  1,  4,  510, FLAT, MONS_MANTICORE },
  {  1,  4,  300, FLAT, MONS_MERFOLK_SIREN },
  {  1,  4,  195, FLAT, MONS_WATER_ELEMENTAL },
  {  1,  4,  275, FLAT, MONS_WIND_DRAKE },
  {  1,  4,  200, FLAT, MONS_HARPY },
  {  1,  7,  135, PEAK, MONS_MERFOLK_JAVELINEER },
  {  1,  7,  110, PEAK, MONS_ALLIGATOR_SNAPPING_TURTLE },
  {  2,  4,  190, SEMI, MONS_SATYR },
  {  3,  7,   30, PEAK, MONS_FORMLESS_JELLYFISH },
},

{ // Snake Pit
  { -4,  4,  750, SEMI, MONS_NAGA },
  {  0,  4,  215, SEMI, MONS_SALAMANDER },
  {  0,  6,  315, SEMI, MONS_ANACONDA },
  {  0,  8, 1125, SEMI, MONS_BLACK_MAMBA },
  {  0,  8,  340, SEMI, MONS_NAGA_WARRIOR },
  {  0,  8,  550, SEMI, MONS_NAGA_MAGE },
  {  0,  8,  225, SEMI, MONS_NAGA_RITUALIST },
  {  0,  8,  315, SEMI, MONS_NAGA_SHARPSHOOTER },
  {  0,  8,  415, PEAK, MONS_SHOCK_SERPENT },
  {  1,  4,  200, FLAT, MONS_MANA_VIPER },
  {  1,  7,  225, PEAK, MONS_GUARDIAN_SERPENT },
  {  2,  5,  110, PEAK, MONS_SALAMANDER_MYSTIC },
  {  2,  8,  145, SEMI, MONS_NAGARAJA },
  {  2,  8,  100, SEMI, MONS_SALAMANDER_TYRANT },
},

{ // Spider Nest
  // Easy enemies:
  {  1,  4,   200, FLAT, MONS_BOULDER_BEETLE },
  {  1,  4,   100, FALL, MONS_BOULDER_BEETLE },
  {  1,  4,   800, FLAT, MONS_REDBACK },
  {  1,  4,   800, FALL, MONS_REDBACK },
  {  1,  4,   500, FLAT, MONS_JUMPING_SPIDER },
  {  1,  4,   900, FALL, MONS_JUMPING_SPIDER },
  {  1,  4,   600, FLAT, MONS_TARANTELLA },
  {  1,  4,   500, FALL, MONS_TARANTELLA },
  {  1,  4,   500, FLAT, MONS_CULICIVORA },
  {  1,  4,   500, FALL, MONS_CULICIVORA },
  {  1,  4,   700, FLAT, MONS_DEMONIC_CRAWLER },
  {  1,  4,   300, FALL, MONS_DEMONIC_CRAWLER },
  // Harder enemies:
  {  1,  4,   625, FLAT, MONS_WOLF_SPIDER },
  {  1,  4,   525, RISE, MONS_WOLF_SPIDER },
  {  1,  4,   400, FLAT, MONS_TORPOR_SNAIL },
  {  1,  4,   200, RISE, MONS_TORPOR_SNAIL },
  {  1,  4,   325, FLAT, MONS_ENTROPY_WEAVER },
  {  1,  4,   325, RISE, MONS_ENTROPY_WEAVER },
  {  1,  4,   200, FLAT, MONS_RADROACH },
  {  1,  4,   200, RISE, MONS_RADROACH },
  {  1,  4,   525, FLAT, MONS_PHARAOH_ANT },
  {  1,  4,   425, RISE, MONS_PHARAOH_ANT },
  {  1,  4,   200, FLAT, MONS_MOTH_OF_WRATH },
  {  1,  4,   250, RISE, MONS_MOTH_OF_WRATH },
  {  1,  4,   525, FLAT, MONS_STEELBARB_WORM },
  {  1,  4,   325, RISE, MONS_STEELBARB_WORM },
  {  1,  4,   200, FLAT, MONS_ORB_SPIDER },
  {  1,  4,   350, RISE, MONS_ORB_SPIDER },
  // Top-tier threats:
  {  1,  4,   100, FLAT, MONS_GHOST_MOTH },
  {  1,  4,   175, RISE, MONS_GHOST_MOTH },
  {  1,  4,   100, FLAT, MONS_EMPEROR_SCORPION },
  {  1,  4,   175, RISE, MONS_EMPEROR_SCORPION },
  {  1,  4,   100, FLAT, MONS_BROODMOTHER },
  {  1,  4,   150, RISE, MONS_BROODMOTHER },
  {  1,  4,   75,  FLAT, MONS_JOROGUMO },
  {  1,  4,   150, RISE, MONS_JOROGUMO },
  {  1,  4,   50,  FLAT, MONS_MELIAI },
  {  1,  4,   150, RISE, MONS_MELIAI },
  {  1,  4,   100, FLAT, MONS_SUN_MOTH },
  {  1,  4,   150, RISE, MONS_SUN_MOTH },
  {  1,  4,   75,  FLAT, MONS_SPARK_WASP },
  {  1,  4,   150, RISE, MONS_SPARK_WASP },
  // OODs
  {  5,  7,   1500,  FLAT, MONS_SPARK_WASP },
  {  5,  7,   1500,  FLAT, MONS_JOROGUMO },
  {  5,  7,   1500,  FLAT, MONS_RADROACH },
  {  5,  7,   1500,  FLAT, MONS_BROODMOTHER },
  {  5,  7,   1750,  FLAT, MONS_GHOST_MOTH },
  {  5,  7,   1750,  FLAT, MONS_EMPEROR_SCORPION },
  {  5,  7,   500,   FLAT, MONS_DEATH_SCARAB },
},

{ // Slime Pits
  {  1,  5, 2000, FLAT, MONS_SLIME_CREATURE },
  {  1,  5, 1000, FLAT, MONS_ACID_BLOB },
  {  1,  5,  515, FLAT, MONS_AZURE_JELLY },
  {  1,  5,  515, FLAT, MONS_ROCKSLIME },
  {  1,  5,  515, FLAT, MONS_VOID_OOZE },
  {  1,  5,  515, FLAT, MONS_SHINING_EYE },
  {  1,  5,  200, FLAT, MONS_GOLDEN_EYE },
  {  1,  5,  200, FLAT, MONS_FORMLESS_JELLYFISH },
  {  1,  8,  300, SEMI, MONS_EYE_OF_DEVASTATION },
  {  1,  8,  265, SEMI, MONS_GREAT_ORB_OF_EYES },
  {  2,  5,  100, RISE, MONS_GLOWING_ORANGE_BRAIN },
  {  2,  8,  315, SEMI, MONS_GLASS_EYE },
},

{ // The Vaults (OOD cap: 12)
  // Vaults is essentially split in two: Vaults 1-4, the 'normal' part,
  // and Vaults:5, the final challenge.
  // Trying to keep total around 10,000 on each floor 1-4, roughly.

  // Easy enemies:
  {  1,  4,  400, FALL, MONS_YAKTAUR },
  {  1,  4,  350, FALL, MONS_CENTAUR_WARRIOR },
  {  1,  4,  200, FALL, MONS_BOULDER_BEETLE },
  {  1,  4,  175, FALL, MONS_HARPY },
  {  1,  4,  100, FALL, MONS_BOGGART },
  {  1,  4,  600, FALL, MONS_LINDWURM },

  {  1,  4,  200, FLAT, MONS_DIRE_ELEPHANT },
  {  1,  4,  200, FALL, MONS_DIRE_ELEPHANT },
  {  1,  4,  400, FLAT, MONS_SLIME_CREATURE },
  {  1,  4,  400, FALL, MONS_SLIME_CREATURE },
  // Harder enemies:
  {  1,  4, 1250, FLAT, MONS_VAULT_SENTINEL },
  {  1,  4,  150, FLAT, MONS_ENTROPY_WEAVER },
  {  1,  4,  100, FLAT, MONS_POLTERGUARDIAN },
  {  1,  4, 1200, FLAT, MONS_IRONBOUND_CONVOKER },
  {  1,  4, 1200, FLAT, MONS_IRONBOUND_PRESERVER },
  {  1,  4,  725, FLAT, MONS_IRONBOUND_FROSTHEART },
  {  1,  4,  525, FLAT, MONS_VAULT_WARDEN },
  {  1,  4,  525, RISE, MONS_VAULT_WARDEN },
  {  1,  4,  500, FLAT, MONS_IRONBOUND_THUNDERHULK },
  {  1,  4,  200, RISE, MONS_IRONBOUND_THUNDERHULK },
  {  1,  4,  325, FLAT, MONS_GREAT_ORB_OF_EYES },
  {  1,  4,  325, RISE, MONS_GREAT_ORB_OF_EYES },
  {  1,  4,   50, FLAT, MONS_GLOWING_ORANGE_BRAIN },
  {  1,  4,   50, RISE, MONS_GLOWING_ORANGE_BRAIN },
  {  1,  4,  150, FLAT, MONS_FORMLESS_JELLYFISH },
  {  1,  4,  100, RISE, MONS_FORMLESS_JELLYFISH },
  {  1,  4,  350, FLAT, MONS_ARCANIST },
  {  1,  4,  350, RISE, MONS_ARCANIST },
  {  1,  4,   75, FLAT, MONS_NECROMANCER },
  {  1,  4,   75, RISE, MONS_NECROMANCER },
  {  1,  4,  500, FLAT, MONS_YAKTAUR_CAPTAIN },
  {  1,  4,  400, RISE, MONS_YAKTAUR_CAPTAIN },
  {  1,  4,  100, FLAT, MONS_ORC_WARLORD },
  {  1,  4,   50, RISE, MONS_ORC_WARLORD },
  // Top-tier threats:
  {  1,  4,  350, RISE, MONS_PEACEKEEPER },
  {  1,  4,  300, RISE, MONS_SPHINX },

  // Vaults:5 enemies. These weights are very roughly based on
  // the monster frequency as of 0.26.
  // V:5 humans:
  {  5, 12, 1000, FALL, MONS_VAULT_WARDEN },
  {  5, 12,  600, FALL, MONS_IRONBOUND_PRESERVER },
  {  5, 12,  520, FALL, MONS_IRONBOUND_CONVOKER },
  {  5, 12,  520, FALL, MONS_VAULT_SENTINEL },
  {  5, 12,  130, FALL, MONS_VAULT_GUARD },
  {  5, 12,   90, FALL, MONS_NECROMANCER },
  {  5, 12,   90, FALL, MONS_HELL_KNIGHT },
  // V:5 undead friends:
  {  5, 12,  520, FALL, MONS_PHANTASMAL_WARRIOR },
  {  5, 12,  190, FALL, MONS_FREEZING_WRAITH },
  {  5, 12,  110, FALL, MONS_LICH },
  {  5, 12,   45, FALL, MONS_ENTROPY_WEAVER },
  {  5, 12,   45, FALL, MONS_SHADOW_WRAITH },
  {  5, 12,   45, FALL, MONS_FLAYED_GHOST },
  // V:5 elves:
  {  5, 12,  300, FALL, MONS_DEEP_ELF_HIGH_PRIEST },
  {  5, 12,  150, FALL, MONS_DEEP_ELF_DEATH_MAGE },
  {  5, 12,  150, FALL, MONS_DEEP_ELF_ANNIHILATOR },
  {  5, 12,  150, FALL, MONS_DEEP_ELF_DEMONOLOGIST },
  {  5, 12,  150, FALL, MONS_DEEP_ELF_SORCERER },
  // V:5 giants
  {  5, 12,  700, FALL, MONS_STONE_GIANT },
  {  5, 12,  640, FALL, MONS_ETTIN },
  {  5, 12,  350, FALL, MONS_FIRE_GIANT },
  {  5, 12,  350, FALL, MONS_FROST_GIANT },
  {  5, 12,  220, FALL, MONS_OGRE_MAGE },
  {  5, 12,  120, FALL, MONS_DEEP_TROLL_EARTH_MAGE },
  // V:5 misc:
  {  5, 12,  800, FALL, MONS_YAKTAUR_CAPTAIN },
  {  5, 12,  750, FALL, MONS_GLOWING_SHAPESHIFTER },
  {  5, 12,  600, FALL, MONS_TENGU_REAVER },
  {  5, 12,  330, FALL, MONS_VERY_UGLY_THING },
  {  5, 12,  260, FALL, MONS_SPHINX },
  {  5, 12,  260, FALL, MONS_WAR_GARGOYLE },
  {  5, 12,  300, FALL, MONS_POLTERGUARDIAN },
  // V:5 chaff from earlier floors
  {  5,  5,  650, FLAT, MONS_ORC_KNIGHT },
  {  5,  5,  180, FLAT, MONS_FORMLESS_JELLYFISH },
  {  5,  5,   60, FLAT, MONS_DANCING_WEAPON },
  {  5,  5,   60, FLAT, MONS_GREAT_ORB_OF_EYES },

  // Out-of-depth enemies.
  // These total 860 weight, to avoid being too common
  // relative to the 'normal' enemies.
  {  5, 12,  190, RISE, MONS_TITAN },
  {  5, 12,  155, RISE, MONS_SHADOW_DRAGON },
  {  5, 12,  150, RISE, MONS_UNDYING_ARMOURY },
  {  5, 12,  100, RISE, MONS_STORM_DRAGON },
  {  5, 12,   70, RISE, MONS_GOLDEN_DRAGON },
  {  5, 12,   50, RISE, MONS_QUICKSILVER_DRAGON },
  {  5, 12,   45, RISE, MONS_IRON_DRAGON },
  {  5, 12,   45, RISE, MONS_ANCIENT_LICH },
  {  5, 12,   45, RISE, MONS_DREAD_LICH },
},

#if TAG_MAJOR_VERSION == 34
{ // Hall of Blades
  {  1,  1, 1000, FLAT, MONS_DANCING_WEAPON },
},
#endif

{ // Crypt (OOD cap: 5)
  { -4,  3,  125, SEMI, MONS_LAUGHING_SKULL },
  { -4,  3,   75, SEMI, MONS_NECROMANCER },
  { -3,  3,   75, SEMI, MONS_DEATH_KNIGHT },
  { -2,  3,  125, SEMI, MONS_WRAITH },
  { -1,  5,   75, PEAK, MONS_JIANGSHI },
  { -1,  5,  125, PEAK, MONS_PHANTASMAL_WARRIOR },
  { -1,  9,  120, PEAK, MONS_ANCIENT_CHAMPION },
  {  0,  2,   65, SEMI, MONS_FREEZING_WRAITH },
  {  0,  5,  125, FLAT, MONS_VAMPIRE_MAGE },
  {  0,  5,   93, PEAK, MONS_GHOUL },
  {  0,  5,  105, FLAT, MONS_VAMPIRE_KNIGHT },
  {  1,  4,   75, FLAT, MONS_SKELETAL_WARRIOR },
  {  1,  5,   55, SEMI, MONS_SOUL_EATER },
  {  1,  6,  145, SEMI, MONS_EIDOLON },
  {  1,  6,   80, SEMI, MONS_DEEP_ELF_DEATH_MAGE },
  {  1,  7,   85, SEMI, MONS_REVENANT },
  {  1,  7,   75, SEMI, MONS_CURSE_SKULL },
  {  2,  5,  145, SEMI, MONS_FLAYED_GHOST },
  {  2,  5,   95, SEMI, MONS_SHADOW_WRAITH },
  {  2,  7,   40, SEMI, MONS_REAPER },
  {  2,  7,   55, PEAK, MONS_LICH },
  {  3,  7,   15, PEAK, MONS_ANCIENT_LICH },
  {  3,  7,   15, PEAK, MONS_DREAD_LICH },
},

{ // Tomb (OOD cap: 5)
  {  1,  5,  300, FLAT, MONS_SKELETON },
  {  1,  5,  235, FLAT, MONS_ZOMBIE },
  {  1,  5,   80, SEMI, MONS_SIMULACRUM },
  {  1,  5, 1050, FLAT, MONS_MUMMY },
  {  1,  5,  600, FLAT, MONS_GUARDIAN_MUMMY },
  { -1,  9,  335, SEMI, MONS_MUMMY_PRIEST },
  {  4,  5,    3, RISE, MONS_ROYAL_MUMMY },
  {  3,  5,    6, FLAT, MONS_LICH },
  {  3,  5,    3, SEMI, MONS_ANCIENT_LICH },
  {  3,  5,    3, SEMI, MONS_DREAD_LICH },
  {  1,  5,  250, FLAT, MONS_USHABTI },
  {  1,  5,  150, FLAT, MONS_DEATH_SCARAB },
  {  3,  5,   12, SEMI, MONS_BENNU },
},
#if TAG_MAJOR_VERSION > 34
POP_DEPTHS,
#endif

{ // Hell
  {  1,  1,   89, FLAT, MONS_HELL_RAT },
  {  1,  1,   89, FLAT, MONS_DEMONIC_CRAWLER },
  {  1,  1,   89, FLAT, MONS_HELL_HOUND },
  {  1,  1,  145, FLAT, MONS_HELL_HOG },
  {  1,  1,  136, FLAT, MONS_HELL_KNIGHT },
  {  1,  1,  136, FLAT, MONS_NECROMANCER },
  {  1,  1,   25, FLAT, MONS_HELLEPHANT },
  {  1,  1,   25, FLAT, MONS_TENTACLED_MONSTROSITY },
  {  1,  1,   89, FLAT, MONS_RED_DEVIL },
  {  1,  1,   89, FLAT, MONS_ICE_DEVIL },
  {  1,  1,  515, FLAT, MONS_SUN_DEMON },
  {  1,  1,  515, FLAT, MONS_SOUL_EATER },
  {  1,  1,  545, FLAT, MONS_REAPER },
  {  1,  1,   25, FLAT, MONS_HELLION },
  {  1,  1,   25, FLAT, MONS_TORMENTOR },
  {  1,  1,   89, FLAT, MONS_RUST_DEVIL },
  {  1,  1,   89, FLAT, MONS_GREEN_DEATH },
  {  1,  1,   62, FLAT, MONS_BLIZZARD_DEMON },
  {  1,  1,   62, FLAT, MONS_BALRUG },
  {  1,  1,   52, FLAT, MONS_CACODEMON },
},

// Hell branches: OOD Cap 14
// "cross-hell" threats (good for regular rolls, fall off and
// let branch flavour take over as depth goes 8-12)
#define CROSS_HELL_POP \
  {  1,  7,  100, FLAT, MONS_HELLION },\
  {  8, 12,  100, FALL, MONS_HELLION },\
  {  1,  7,  100, FLAT, MONS_TORMENTOR },\
  {  8, 12,  100, FALL, MONS_TORMENTOR },\
  {  1, 12,   10, FLAT, MONS_ANCIENT_LICH },\
  {  1, 12,   10, FLAT, MONS_DREAD_LICH }

{ // Dis
  CROSS_HELL_POP,
  // "basic" monsters
  {  1,  5,  500, FALL, MONS_ANCIENT_CHAMPION },
  {  1,  7,  125, FLAT, MONS_ANCIENT_CHAMPION },
  {  1,  5,  400, FALL, MONS_WAR_GARGOYLE },
  {  1,  7,  100, FLAT, MONS_WAR_GARGOYLE },
  {  1,  5,  100, FALL, MONS_IRON_DRAGON },
  {  1,  7,   25, FLAT, MONS_IRON_DRAGON },
  // "branch flavour" threats
  {  1,  7,  300, SEMI, MONS_IRON_GOLEM },
  {  7, 14,  300, FALL, MONS_IRON_GOLEM },
  {  1,  7,  200, SEMI, MONS_CAUSTIC_SHRIKE },
  {  7, 14,  200, FALL, MONS_CAUSTIC_SHRIKE },
  {  1,  7,  300, SEMI, MONS_QUICKSILVER_ELEMENTAL },
  {  7, 14,  300, FALL, MONS_QUICKSILVER_ELEMENTAL },
  {  1,  7,  200, SEMI, MONS_CRYSTAL_ECHIDNA },
  {  7, 14,  200, FALL, MONS_CRYSTAL_ECHIDNA },
  // "top tier" signature threats
  {  1,  7,  400, RISE, MONS_HELL_SENTINEL },
  {  8, 14,  400, FLAT, MONS_HELL_SENTINEL },
  {  1, 14,  400, RISE, MONS_IRON_GIANT },
},

{ // Gehenna
  CROSS_HELL_POP,
  // "basic" monsters
  {  1,  5,  400, FALL, MONS_HELL_HOG },
  {  1,  7,  100, FLAT, MONS_HELL_HOG },
  {  1,  5,  400, FALL, MONS_FIRE_GIANT },
  {  1,  7,  100, FLAT, MONS_FIRE_GIANT },
  {  1,  5,  200, FALL, MONS_SALAMANDER_TYRANT },
  {  1,  7,   50, FLAT, MONS_SALAMANDER_TYRANT },
  // "branch flavour" threats
  {  1,  7,  450, SEMI, MONS_BALRUG },
  {  7, 14,  450, FALL, MONS_BALRUG },
  {  1,  7,   30, FLAT, MONS_CREEPING_INFERNO },
  {  1,  7,  450, SEMI, MONS_SEARING_WRETCH },
  {  7, 14,  450, FALL, MONS_SEARING_WRETCH },
  {  1,  7,  300, SEMI, MONS_HELLEPHANT },
  {  8, 14,  300, FALL, MONS_HELLEPHANT },
  // cut these off for deep super-ood
  {  1, 10,   70, FLAT, MONS_STOKER },
  // "top tier" signature threats
  {  1,  8,  225, RISE, MONS_ONI_INCARCERATOR },
  {  9, 14,  225, FLAT, MONS_ONI_INCARCERATOR },
  {  1, 14,  400, RISE, MONS_BRIMSTONE_FIEND },
},

{ // Cocytus
  CROSS_HELL_POP,
  // "basic" monsters
  {  1,  5,  800, FALL, MONS_SIMULACRUM },
  {  1,  7,  200, FLAT, MONS_SIMULACRUM },
  {  1,  7,  100, FALL, MONS_FREEZING_WRAITH },
  {  1,  7,   25, FLAT, MONS_FREEZING_WRAITH },
  {  1,  7,  100, FALL, MONS_FROST_GIANT },
  {  1,  7,   25, FLAT, MONS_FROST_GIANT },
  // "branch flavour" threats
  {  1,  7,  200, PEAK, MONS_TITAN }, // Antaeus brought pals
  {  1,  7,   50, FLAT, MONS_AZURE_JELLY },
  // don't include our guests in ood rolls
  {  1,  7,  375, SEMI, MONS_WENDIGO },
  {  7, 14,  500, FALL, MONS_WENDIGO },
  {  1,  7,  375, SEMI, MONS_NARGUN },
  {  7, 14,  500, FALL, MONS_NARGUN },
  // "top tier" signature threats
  {  1,  7,  400, RISE, MONS_ICE_FIEND },
  {  8, 14,  400, FLAT, MONS_ICE_FIEND },
  {  1, 14,  400, RISE, MONS_SHARD_SHRIKE },
},

{ // Tartarus
  CROSS_HELL_POP,
  // "basic" monsters
  {  1,  5,  800, FALL, MONS_SPECTRAL_THING },
  {  1,  7,  200, FLAT, MONS_SPECTRAL_THING },
  {  1,  5,  100, FALL, MONS_SHADOW_WRAITH },
  {  1,  7,   25, FLAT, MONS_SHADOW_WRAITH },
  {  1,  5,  100, FALL, MONS_EIDOLON },
  {  1,  7,   25, FLAT, MONS_EIDOLON },
  // "branch flavour" threats
  {  1,  7,  200, SEMI, MONS_PROFANE_SERVITOR },
  {  1,  7,  150, SEMI, MONS_BONE_DRAGON },
  {  7, 14,  400, FALL, MONS_BONE_DRAGON },
  {  1,  7,  300, SEMI, MONS_DOOM_HOUND },
  {  7, 14,  300, FALL, MONS_DOOM_HOUND },
  {  1,  7,  300, SEMI, MONS_PUTRID_MOUTH },
  {  7, 14,  300, FALL, MONS_PUTRID_MOUTH },
  {  1, 12,   50, FLAT, MONS_SILENT_SPECTRE },
  // "top tier" signature threats
  {  1,  7,  400, RISE, MONS_TAINTED_LEVIATHAN },
  {  8, 14,  400, FLAT, MONS_TAINTED_LEVIATHAN },
  {  1, 14,  400, RISE, MONS_TZITZIMITL },
},

{ // Zot
  {  1,  5,  970, FLAT, MONS_MOTH_OF_WRATH },
  {  1,  5,  192, RISE, MONS_GHOST_MOTH },
  {  1,  5,  100, FLAT, MONS_BLACK_DRACONIAN },
  {  1,  5,  100, FLAT, MONS_YELLOW_DRACONIAN },
  {  1,  5,  100, FLAT, MONS_GREEN_DRACONIAN },
  {  1,  5,  100, FLAT, MONS_PURPLE_DRACONIAN },
  {  1,  5,  100, FLAT, MONS_RED_DRACONIAN },
  {  1,  5,  100, FLAT, MONS_WHITE_DRACONIAN },
  {  1,  5,   67, FLAT, MONS_DRACONIAN_STORMCALLER },
  {  1,  5,   67, FLAT, MONS_DRACONIAN_MONK },
  {  1,  5,   67, FLAT, MONS_DRACONIAN_SHIFTER },
  {  1,  5,   67, FLAT, MONS_DRACONIAN_ANNIHILATOR },
  {  1,  5,   67, FLAT, MONS_DRACONIAN_KNIGHT },
  {  1,  5,   67, FLAT, MONS_DRACONIAN_SCORCHER },
  {  1,  5,  200, FLAT, MONS_QUICKSILVER_DRAGON },
  {  1,  5,  200, FLAT, MONS_SHADOW_DRAGON },
  { -4,  5,  515, RISE, MONS_STORM_DRAGON },
  {  1, 11,  365, SEMI, MONS_GOLDEN_DRAGON },
  {  1,  5,  112, FLAT, MONS_PROTEAN_PROGENITOR },
  {  2,  8,   52, SEMI, MONS_KILLER_KLOWN },
  {  1,  5,  335, FLAT, MONS_DEATH_COB },
  {  1,  5,  150, RISE, MONS_CURSE_TOE },
  {  1,  5,  515, FLAT, MONS_TENTACLED_MONSTROSITY },
  {  1,  5,   89, FALL, MONS_ELECTRIC_GOLEM },
  {  1,  5,   42, FLAT, MONS_ORB_OF_FIRE },
},
#if TAG_MAJOR_VERSION == 34
{ // Forest
  {  1,  5,  120, FALL, MONS_WOLF },
  {  1,  5,   35, FALL, MONS_BLACK_BEAR },
  {  1,  5,   50, FLAT, MONS_YAK },
  {  1,  7,  145, SEMI, MONS_DIRE_ELEPHANT },
  {  1,  5,   45, FLAT, MONS_HORNET },
  {  1,  6,   75, FALL, MONS_REDBACK },
  {  2,  7,   35, SEMI, MONS_WOLF_SPIDER },
  {  1,  9,   75, SEMI, MONS_OKLOB_PLANT },
  {  1,  5,  170, FLAT, MONS_DRYAD },
  {  1,  5,  120, FLAT, MONS_WIND_DRAKE },
  { -1,  5,   75, SEMI, MONS_FAUN },
  {  0,  9,  105, SEMI, MONS_SATYR },
  {  2,  8,   55, SEMI, MONS_SPRIGGAN_DRUID },
  {  1,  6,  155, SEMI, MONS_SPRIGGAN_RIDER },
  {  1,  9,  235, SEMI, MONS_SPRIGGAN_BERSERKER },
  {  1,  8,  155, SEMI, MONS_SPRIGGAN_AIR_MAGE },
  {  3,  5,  115, RISE, MONS_SPRIGGAN_DEFENDER },
  {  1,  7,   85, PEAK, MONS_APIS },
  {  2,  7,  165, SEMI, MONS_SHAMBLING_MANGROVE },
  {  1,  6,   85, SEMI, MONS_ANACONDA },
  {  1,  9,  100, PEAK, MONS_THORN_HUNTER },
  {  1,  5,  125, FLAT, MONS_BUTTERFLY },
},
#endif

{ // Abyss
  // Sorted by 'home depth', somewhat arbitrarily defined.
  // Abyss:1
  { -1,  6, 1150, FALL, MONS_ABOMINATION_SMALL },
  { -1,  6,  150, FALL, MONS_BRAIN_WORM },
  { -1,  6,  200, FALL, MONS_WEEPING_SKULL },

  {  1,  4,   25, FALL, MONS_CRIMSON_IMP },
  {  1,  4,  180, FALL, MONS_WHITE_IMP },
  {  1,  4,  180, FALL, MONS_QUASIT },
  {  1,  4,  180, FALL, MONS_UFETUBUS },
  {  1,  4,  180, FALL, MONS_IRON_IMP },
  {  1,  4,   25, FALL, MONS_SHADOW_IMP },
  {  1,  5,    8, FALL, MONS_RED_DEVIL },
  {  1,  5,   25, FALL, MONS_ICE_DEVIL },
  {  1,  5,   36, FALL, MONS_RUST_DEVIL },
  {  1,  8,   73, FALL, MONS_HELLWING },
  {  1,  8,   91, FALL, MONS_SIXFIRHY },
  {  1,  5,  125, FALL, MONS_ORANGE_DEMON },
  {  1,  8,  300, FALL, MONS_YNOXINUL },
  {  1,  6,   65, FALL, MONS_OBSIDIAN_BAT },

  {  1,  5,  130, FALL, MONS_SKELETON },
  {  1,  5,  124, FALL, MONS_ZOMBIE },
  {  1,  4,   35, FALL, MONS_WIGHT },
  {  1,  8,   70, FALL, MONS_WRAITH },
  {  1,  8,   10, FALL, MONS_SHADOWGHAST },
  {  1,  8,   25, FALL, MONS_VAMPIRE },

  {  1,  5,    4, FALL, MONS_BASILISK },
  {  1,  5,   21, FALL, MONS_JELLY },
  {  1,  4,   21, FALL, MONS_ICE_BEAST },
  {  1,  4,   21, FALL, MONS_SKY_BEAST },
  {  1,  5,   14, FALL, MONS_KOBOLD_DEMONOLOGIST },
  {  1,  5,   46, FALL, MONS_FIRE_BAT },
  // Abyss:3
  {  1,  3,   50, FLAT, MONS_SHAPESHIFTER },
  {  4,  5,   50, FALL, MONS_SHAPESHIFTER },
  {  1,  3,   35, FLAT, MONS_UNSEEN_HORROR },
  {  4,  5,   35, FALL, MONS_UNSEEN_HORROR },
  {  1,  3,   42, FLAT, MONS_SIMULACRUM },
  {  4,  5,   42, FALL, MONS_SIMULACRUM },
  {  1,  3,   33, FLAT, MONS_GREAT_ORB_OF_EYES },
  {  4,  5,   33, FALL, MONS_GREAT_ORB_OF_EYES },
  {  1,  5,    8, FLAT, MONS_ORC_SORCERER },
  {  1,  5,   29, FLAT, MONS_BLINK_FROG },
  {  1,  5,    8, FLAT, MONS_MANA_VIPER },
  {  1,  5,    8, FLAT, MONS_ORB_SPIDER },
  {  1,  5,   18, FLAT, MONS_EARTH_ELEMENTAL },
  {  1,  5,    6, FLAT, MONS_FIRE_ELEMENTAL },
  {  1,  5,    6, FLAT, MONS_AIR_ELEMENTAL },
  {  1,  5,    6, FLAT, MONS_WATER_ELEMENTAL },
  {  1,  5,    5, FLAT, MONS_BLACK_DRACONIAN },
  {  1,  5,    5, FLAT, MONS_YELLOW_DRACONIAN },
  {  1,  5,    5, FLAT, MONS_GREEN_DRACONIAN },
  {  1,  5,    5, FLAT, MONS_PURPLE_DRACONIAN },
  {  1,  5,    5, FLAT, MONS_RED_DRACONIAN },
  {  1,  5,    5, FLAT, MONS_WHITE_DRACONIAN },
  {  1,  5,    8, FLAT, MONS_EYE_OF_DEVASTATION },
  {  1,  5,   12, FLAT, MONS_VAMPIRE_MAGE },
  {  1,  5,    8, FLAT, MONS_DEATH_KNIGHT },
  {  1,  5,    8, FLAT, MONS_DEATH_DRAKE },

  {  1,  3,  192, FLAT, MONS_CHAOS_SPAWN },
  {  4,  6,  192, FALL, MONS_CHAOS_SPAWN },
  {  1,  3,   22, FLAT, MONS_SUN_DEMON },
  {  4,  6,   22, FALL, MONS_SUN_DEMON },
  {  1,  3,   14, FLAT, MONS_SOUL_EATER },
  {  4,  6,   14, FALL, MONS_SOUL_EATER },
  {  1,  3,   63, FLAT, MONS_EFREET },
  {  4,  6,   63, FALL, MONS_EFREET },
  {  1,  3,   66, FLAT, MONS_RAKSHASA },
  {  4,  6,   66, FALL, MONS_RAKSHASA },
  {  1,  3,   25, FLAT, MONS_DEMONIC_CRAWLER },
  {  4,  6,   25, FALL, MONS_DEMONIC_CRAWLER },
  {  1,  3,   40, FLAT, MONS_FREEZING_WRAITH },
  {  4,  6,   40, FALL, MONS_FREEZING_WRAITH },
  {  1,  3,   25, FLAT, MONS_GUARDIAN_SERPENT },
  {  4,  6,   25, FALL, MONS_GUARDIAN_SERPENT },
  {  1,  3,   18, FLAT, MONS_FLAYED_GHOST },
  {  4,  6,   18, FALL, MONS_FLAYED_GHOST },
  {  1,  6,    8, FLAT, MONS_HELL_KNIGHT },
  {  1,  6,    8, FLAT, MONS_NECROMANCER },
  {  1,  6,    8, FLAT, MONS_OCCULTIST },

  {  1,  5,  150, FLAT, MONS_LAUGHING_SKULL },
  {  6,  7,  150, FALL, MONS_LAUGHING_SKULL },
  {  1,  5,  850, FLAT, MONS_ABOMINATION_LARGE },
  {  6,  7,  850, FALL, MONS_ABOMINATION_LARGE },
  {  1,  5,   52, FLAT, MONS_VERY_UGLY_THING },
  {  6,  7,   52, FALL, MONS_VERY_UGLY_THING },
  {  1,  5,  300, FLAT, MONS_RAIJU },
  {  6,  7,  300, FALL, MONS_RAIJU },
  {  1,  8,  165, FALL, MONS_WORLDBINDER },
  {  1,  8,   30, FALL, MONS_HELL_HOUND },
  {  1,  7,   35, SEMI, MONS_WILL_O_THE_WISP },
  // Abyss:5
  {  1, 10,   10, SEMI, MONS_GLOWING_SHAPESHIFTER },
  {  1, 10,   16, SEMI, MONS_TENTACLED_MONSTROSITY },
  {  1, 10,   16, SEMI, MONS_SHADOW_DEMON },
  {  1, 10,   40, SEMI, MONS_HELL_HOG },
  {  5,  7,   21, FLAT, MONS_ACID_BLOB },
  {  1, 10,    4, SEMI, MONS_GHOST_MOTH },
  {  1,  4,    2, FLAT, MONS_CRYSTAL_GUARDIAN },
  {  5,  7,   20, FLAT, MONS_CRYSTAL_GUARDIAN },
  {  4, 10,    5, SEMI, MONS_FROSTBOUND_TOME },
  {  4, 10,    5, SEMI, MONS_EARTHEN_TOME },
  {  4, 10,    5, SEMI, MONS_CRYSTAL_TOME },
  {  4, 10,    5, SEMI, MONS_DIVINE_TOME },
  // Abyss:7
  {  1, 10,  180, RISE, MONS_SPATIAL_MAELSTROM },
  {  1, 10,   16, SEMI, MONS_HELLEPHANT },
  {  6, 10,   50, RISE, MONS_HELLEPHANT },
  {  1, 10,   22, SEMI, MONS_PROFANE_SERVITOR },
  {  6, 10,   50, RISE, MONS_PROFANE_SERVITOR },

  {  1,  5,    2, FLAT, MONS_SHADOW_DRAGON },
  {  6, 10,   20, RISE, MONS_SHADOW_DRAGON },
  {  1,  5,    2, FLAT, MONS_QUICKSILVER_DRAGON },
  {  6, 10,   20, RISE, MONS_QUICKSILVER_DRAGON },
  {  1,  5,    2, FLAT, MONS_PEARL_DRAGON },
  {  6, 10,   20, RISE, MONS_PEARL_DRAGON },
  {  6, 10,   10, RISE, MONS_CRYSTAL_ECHIDNA },
  {  6, 10,   10, RISE, MONS_QUICKSILVER_ELEMENTAL },
  {  6, 10,   10, RISE, MONS_NARGUN },
  {  6, 10,   10, RISE, MONS_PUTRID_MOUTH },
  // Generic
  {  1,  7,  340, FLAT, MONS_WRETCHED_STAR },
  {  1,  7,  340, FLAT, MONS_TENTACLED_STARSPAWN },
  {  1,  7,  340, FLAT, MONS_ANCIENT_ZYME },
  {  1,  7,  335, FLAT, MONS_STARCURSED_MASS },
  {  1,  7,  335, FLAT, MONS_THRASHING_HORROR },
  {  1,  7,   90, FLAT, MONS_LURKING_HORROR },
  {  1,  7,   90, FLAT, MONS_APOCALYPSE_CRAB },

  {  1,  7,  225, FLAT, MONS_NEQOXEC },
  {  1,  7,  300, FLAT, MONS_SMOKE_DEMON },
  {  1,  7,   25, FLAT, MONS_HELLION },
  {  1,  7,   21, FLAT, MONS_TORMENTOR },
  {  1,  7,   15, FLAT, MONS_REAPER },
  {  1,  7,   25, FLAT, MONS_GREEN_DEATH },
  {  1,  7,   22, FLAT, MONS_BLIZZARD_DEMON },
  {  1,  7,   22, FLAT, MONS_BALRUG },
  {  1,  7,   25, FLAT, MONS_CACODEMON },
  {  1,  7,   25, FLAT, MONS_EXECUTIONER },
  {  1,  7,   10, FLAT, MONS_HELL_SENTINEL },

  {  1,  7,   23, FLAT, MONS_SHADOW_WRAITH },
  {  1,  7,   10, FLAT, MONS_SILENT_SPECTRE },
  {  1,  7,    8, FLAT, MONS_PHANTASMAL_WARRIOR },
  {  1,  7,   80, FLAT, MONS_BONE_DRAGON },
  {  1,  7,    9, FLAT, MONS_REVENANT },
  {  1,  7,   46, FLAT, MONS_LICH },
  {  1,  7,    8, FLAT, MONS_ANCIENT_LICH },
  {  1,  7,    8, FLAT, MONS_DREAD_LICH },

  {  1,  7,    8, FLAT, MONS_DEEP_ELF_SORCERER },
  {  1,  7,    5, FLAT, MONS_DEEP_ELF_DEMONOLOGIST },
  {  1,  7,    8, FLAT, MONS_DEEP_ELF_ELEMENTALIST },
  {  1,  7,   17, FLAT, MONS_EIDOLON },
  {  1,  7,    3, FLAT, MONS_DANCING_WEAPON },
  {  1,  7,    1, FLAT, MONS_TOENAIL_GOLEM },
  {  1,  7,    8, FLAT, MONS_SHINING_EYE },
  {  1,  7,   33, FLAT, MONS_GOLDEN_EYE },
  {  1,  7,   52, FLAT, MONS_GLOWING_ORANGE_BRAIN },
  {  1,  7,    3, FLAT, MONS_DRACONIAN_STORMCALLER },
  {  1,  7,    3, FLAT, MONS_DRACONIAN_MONK },
  {  1,  7,    5, FLAT, MONS_DRACONIAN_SHIFTER },
  {  1,  7,    3, FLAT, MONS_DRACONIAN_ANNIHILATOR },
  {  1,  7,    3, FLAT, MONS_DRACONIAN_KNIGHT },
  {  1,  7,    3, FLAT, MONS_DRACONIAN_SCORCHER },
  {  1,  7,    5, FLAT, MONS_BUNYIP },

  {  3,  7,    1, FLAT, MONS_DEMONSPAWN_CORRUPTER },
  {  5,  7,    2, FLAT, MONS_FRAVASHI },
  {  1,  7,   11, FLAT, MONS_ANGEL },
  {  1,  7,   14, FLAT, MONS_DAEVA },
  {  1,  7,    8, FLAT, MONS_OPHAN },
},

{ // Pandemonium - only used for 1 in 40 random Pan spawns (the rest are drawn
  // from each floor's monster list), and for the Orb run (where monsters are
  // picked randomly without checking rarity).
  {  1,  1,    4, FLAT, MONS_GOLDEN_EYE },
  {  1,  1,    4, FLAT, MONS_GREAT_ORB_OF_EYES },
  {  1,  1,    4, FLAT, MONS_GLOWING_ORANGE_BRAIN },
  {  1,  1,    8, FLAT, MONS_TOENAIL_GOLEM },
  {  1,  1,  192, FLAT, MONS_DEMONIC_CRAWLER },
  {  1,  1,   25, FLAT, MONS_HELL_HOUND },
  {  1,  1,   25, FLAT, MONS_HELL_HOG },
  {  1,  1, 1000, FLAT, MONS_ABOMINATION_LARGE },
  {  1,  1,   89, FLAT, MONS_EFREET },
  {  1,  1,   89, FLAT, MONS_RAKSHASA },
  {  1,  1,   25, FLAT, MONS_HELLEPHANT },
  {  1,  1, 1000, FLAT, MONS_ORANGE_DEMON },
  {  1,  1, 1000, FLAT, MONS_HELLWING },
  {  1,  1, 1000, FLAT, MONS_SIXFIRHY },
  {  1,  1,  670, FLAT, MONS_RED_DEVIL },
  {  1,  1,  335, FLAT, MONS_ICE_DEVIL },
  {  1,  1,  335, FLAT, MONS_RUST_DEVIL },
  {  1,  1, 1000, FLAT, MONS_YNOXINUL },
  {  1,  1,  400, FLAT, MONS_NEQOXEC },
  {  1,  1,  900, FLAT, MONS_SMOKE_DEMON },
  {  1,  1,  535, FLAT, MONS_SUN_DEMON },
  {  1,  1,  535, FLAT, MONS_SOUL_EATER },
  {  1,  1,  192, FLAT, MONS_CHAOS_SPAWN },
  {  1,  1, 1000, FLAT, MONS_BALRUG },
  {  1,  1, 1000, FLAT, MONS_BLIZZARD_DEMON },
  {  1,  1, 1000, FLAT, MONS_GREEN_DEATH },
  {  1,  1, 1000, FLAT, MONS_CACODEMON },
  {  1,  1,  335, FLAT, MONS_HELLION },
  {  1,  1,  335, FLAT, MONS_TORMENTOR },
  {  1,  1,  535, FLAT, MONS_REAPER },
  {  1,  1,  535, FLAT, MONS_SHADOW_DEMON },
  {  1,  1,  675, FLAT, MONS_SIN_BEAST },
  {  1,  1, 1000, FLAT, MONS_EXECUTIONER },
  {  1,  1,  335, FLAT, MONS_BRIMSTONE_FIEND },
  {  1,  1,  335, FLAT, MONS_ICE_FIEND },
  {  1,  1,  335, FLAT, MONS_HELL_SENTINEL },
  {  1,  1,  335, FLAT, MONS_TZITZIMITL },
  {  1,  1,    8, FLAT, MONS_PROFANE_SERVITOR },
  {  1,  1,  675, FLAT, MONS_DEMONSPAWN_BLOOD_SAINT },
  {  1,  1,  675, FLAT, MONS_DEMONSPAWN_WARMONGER },
  {  1,  1,  675, FLAT, MONS_DEMONSPAWN_CORRUPTER },
  {  1,  1,  675, FLAT, MONS_DEMONSPAWN_SOUL_SCHOLAR },
  {  1,  1,   50, FLAT, MONS_ANGEL },
  {  1,  1,   50, FLAT, MONS_FRAVASHI },
  {  1,  1,   40, FLAT, MONS_CHERUB },
  {  1,  1,   25, FLAT, MONS_DAEVA },
},

{ // Ziggurat
},

#if TAG_MAJOR_VERSION == 34
{ // Labyrinth
},
#endif

{ // Bazaar
},

{ // Trove
},

{ // Sewer
  {  1,  1, 1000, FLAT, MONS_FRILLED_LIZARD },
  {  1,  1,  315, FLAT, MONS_QUOKKA },
  {  1,  1, 1000, FLAT, MONS_BAT },
  {  1,  1,  315, FLAT, MONS_DART_SLUG },
  {  1,  1,  515, FLAT, MONS_BALL_PYTHON },
  {  1,  1,  515, FLAT, MONS_ADDER },
  {  1,  1,  515, FLAT, MONS_RIBBON_WORM },
  {  1,  1,  515, FLAT, MONS_ENDOPLASM },
  {  1,  1,  515, FLAT, MONS_GIANT_COCKROACH },
  {  1,  1,   55, FLAT, MONS_BRAIN_WORM },
  {  1,  1,   55, FLAT, MONS_CROCODILE },
},

{ // Ossuary
  {  1,  1,   40, FLAT, MONS_WEEPING_SKULL },
  {  1,  1,   90, FLAT, MONS_MUMMY },
  {  1,  1,  515, FLAT, MONS_SKELETON },
  {  1,  1,  515, FLAT, MONS_ZOMBIE },
},

{ // Bailey
  {  1,  1,  515, FLAT, MONS_GNOLL },
  {  1,  1,  515, FLAT, MONS_ORC },
  {  1,  1,  260, FLAT, MONS_ORC_WARRIOR },
  {  1,  1,   25, FLAT, MONS_ORC_KNIGHT },
},

#if TAG_MAJOR_VERSION > 34
{ // Gauntlet
},
#endif

{ // Ice Cave
  {  1,  1,  120, FLAT, MONS_YAK },
  {  1,  1,   90, FLAT, MONS_WOLF },
  {  1,  1,  400, FLAT, MONS_POLAR_BEAR },
  {  1,  1,   90, FLAT, MONS_ICE_BEAST },
  {  1,  1,  515, FLAT, MONS_WHITE_IMP },
  {  1,  1,  515, FLAT, MONS_ICE_DEVIL },
  {  1,  1,  515, FLAT, MONS_RIME_DRAKE },
  {  1,  1,  260, FLAT, MONS_FREEZING_WRAITH },
  {  1,  1, 1030, FLAT, MONS_SIMULACRUM },
},

{ // Volcano
  {  1,  1,  555, FLAT, MONS_SALAMANDER },
  {  1,  1,  515, FLAT, MONS_HELL_HOUND },
  {  1,  1,  385, FLAT, MONS_RED_DEVIL },
  {  1,  1,  515, FLAT, MONS_EFREET },
  {  1,  1,  515, FLAT, MONS_LINDWURM },
  {  1,  1,   55, FLAT, MONS_TOENAIL_GOLEM },
  {  1,  1,   55, FLAT, MONS_OBSIDIAN_BAT },
  {  1,  1,   55, FLAT, MONS_FIRE_CRAB },
  {  1,  1,  555, FLAT, MONS_FIRE_ELEMENTAL },
  {  1,  1,  385, FLAT, MONS_GARGOYLE },
  {  1,  1,  515, FLAT, MONS_MOLTEN_GARGOYLE },
},

{ // Wizlab
},

#if TAG_MAJOR_VERSION == 34
POP_DEPTHS,
#endif

{ // Desolation
  {  1,  1, 1200, FLAT, MONS_SALTLING },
  {  1,  1,   50, FLAT, MONS_DANCING_WEAPON },
  {  1,  1,   50, FLAT, MONS_MOLTEN_GARGOYLE },
  {  1,  1,   50, FLAT, MONS_CRYSTAL_GUARDIAN },
  {  1,  1,   50, FLAT, MONS_IMPERIAL_MYRMIDON },
},

#if TAG_MAJOR_VERSION == 34
{ // Gauntlet
},
#endif

{ // Arena
},

{ // Crucible
  {  1,  1,  100, FLAT, MONS_ICE_DEVIL },
  {  1,  1,  100, FLAT, MONS_ORANGE_DEMON },
  {  1,  1,  100, FLAT, MONS_RUST_DEVIL },
  {  1,  1,  100, FLAT, MONS_RED_DEVIL },
  {  1,  1,   75, FLAT, MONS_HELLWING },
  {  1,  1,  100, FLAT, MONS_SOUL_EATER },
  {  1,  1,  120, FLAT, MONS_YNOXINUL },
  {  1,  1,  120, FLAT, MONS_SMOKE_DEMON },
  {  1,  1,  120, FLAT, MONS_SUN_DEMON },
  {  1,  1,  120, FLAT, MONS_SIXFIRHY },
  {  1,  1,  100, FLAT, MONS_BLIZZARD_DEMON },
  {  1,  1,  100, FLAT, MONS_GREEN_DEATH },
  {  1,  1,   65, FLAT, MONS_CACODEMON },
  {  1,  1,   50, FLAT, MONS_BALRUG },
  {  1,  1,   50, FLAT, MONS_EXECUTIONER },
},

};

COMPILE_CHECK(ARRAYSZ(population) == NUM_BRANCHES);

static const vector<pop_entry> pop_generic_late_zombie =
{ // Extended generic zombie bases. Pop range caps at 15 (for the deepest hell)
  // Due to how zombie picking works, this starts with Crypt:1 monsters
  // being picked from depth 5. Hells start picking from depth 7, though
  // usually depth 8.
  // Constrictors
  {  5,  15,  500, FLAT, MONS_ANACONDA },
  {  5,  15,  500, FLAT, MONS_NAGA_WARRIOR },
  {  5,  15,  500, FLAT, MONS_NAGARAJA },
  // Draggers and tramplers
  {  5,   8,  222, FALL, MONS_ALLIGATOR },
  {  5,   8,  111, FALL, MONS_FIRE_DRAGON },
  {  5,   8,  111, FALL, MONS_ICE_DRAGON },
  {  5,   8,  222, FALL, MONS_SWAMP_DRAGON },
  {  5,   8,  222, FLAT, MONS_SHADOW_DRAGON },
  {  5,   8,  222, FLAT, MONS_STORM_DRAGON },
  {  5,  15,  222, FLAT, MONS_IRON_DRAGON },
  {  5,  15,  222, RISE, MONS_GOLDEN_DRAGON },
  {  5,  15,  222, RISE, MONS_PEARL_DRAGON },
  {  5,  15,  666, FLAT, MONS_DIRE_ELEPHANT },
  {  5,  15,  666, FLAT, MONS_HELLEPHANT },
  // Hard hitters
  {  7,  15,  500, RISE, MONS_IRON_GIANT },
  {  5,  15,  500, FLAT, MONS_TITAN },
  {  5,  15,  500, FLAT, MONS_ETTIN },
  {  5,  15,  500, FALL, MONS_STONE_GIANT },
  {  5,  15,  250, FLAT, MONS_HYDRA },
  {  5,   8,  100, FALL, MONS_EMPEROR_SCORPION },
  {  5,   8,  100, FALL, MONS_CATOBLEPAS },
  {  5,  10,  250, FLAT, MONS_ALLIGATOR_SNAPPING_TURTLE },
  // Mostly enemy variety for Crypt (rough player parallels for some species
  // not represented elsewhere in this list; classed monsters for hard hits)
  {  5,   9,   72, FALL, MONS_DRACONIAN_MONK },
  {  5,   9,   72, FALL, MONS_ORC_WARLORD },
  {  5,   9,   72, FALL, MONS_TENGU_REAVER },
  {  5,   9,   72, FALL, MONS_MINOTAUR },
  {  5,   9,   72, FALL, MONS_DEMONSPAWN },
  {  5,   9,   72, FALL, MONS_DEEP_ELF_BLADEMASTER },
  {  5,   9,   72, FALL, MONS_MERFOLK_IMPALER },
  {  5,   9,   72, FALL, MONS_VAULT_WARDEN },
  {  5,   9,   72, FALL, MONS_DEEP_TROLL },
  {  5,   9,   72, FALL, MONS_TWO_HEADED_OGRE },
  // Fast mons, base move speed >=13 (more weight here)
  {  5,  15,  722, RISE, MONS_QUICKSILVER_DRAGON },
  {  5,  15, 1000, RISE, MONS_JUGGERNAUT },
  {  5,   9,  500, FALL, MONS_FENSTRIDER_WITCH },
  {  5,   9,  500, FALL, MONS_BLACK_MAMBA },
  {  5,  15, 1500, RISE, MONS_CAUSTIC_SHRIKE },
  {  8,  15, 1500, RISE, MONS_SHARD_SHRIKE },
  {  5,   9, 1000, FLAT, MONS_CENTAUR_WARRIOR },
  {  5,  15, 1500, FLAT, MONS_SPRIGGAN_DEFENDER },
  {  5,  15, 1500, FLAT, MONS_HARPY },
  {  5,   9,  500, FLAT, MONS_BUNYIP },
};

#define GENERIC_WATER_POP { \
  {  1,  27,  150, FLAT, MONS_ELECTRIC_EEL }, \
  {  1,  27,  500, FLAT, MONS_NO_MONSTER }, \
}
#define HELL_WATER_POP {\
  {  0,  8,   100, FLAT, MONS_SIMULACRUM },\
  {  0,  8,   100, RISE, MONS_ELEMENTAL_WELLSPRING },\
  {  0,  6,   200, FALL, MONS_NO_MONSTER },\
}
// This is done to avoid duplicating the Depths list and can be
// changed once TAG_MAJOR_VERSION > 35
#define DEPTHS_WATER_POP {\
  {  1,  6,   600, FLAT, MONS_WATER_ELEMENTAL },\
  {  1,  6,    45, FLAT, MONS_MERFOLK_IMPALER },\
  {  1,  6,    45, FLAT, MONS_MERFOLK_JAVELINEER },\
  {  1,  6,   200, FALL, MONS_NO_MONSTER },\
}

// This list must be in the same order as the branch-type enum values.
// Shoals, Abyss, Pan, Zot, D:1-5 liquid monsters are blocked in dungeon.cc
static const vector<pop_entry> population_water[] =
{
    { // Dungeon water monsters
      {  5,  13,   90, SEMI, MONS_MARROWCUDA },
      {  5,  16,   60, FLAT, MONS_ELECTRIC_EEL },
      {  7,  16,  185, PEAK, MONS_ELECTRIC_EEL },
      {  11, 27,  600, RISE, MONS_WATER_ELEMENTAL },
      {  5,  22,  110, FLAT, MONS_NO_MONSTER },
      {  9,  32,  250, SEMI, MONS_NO_MONSTER },
    },
    GENERIC_WATER_POP, // Temple
    GENERIC_WATER_POP, // Orc
    GENERIC_WATER_POP, // Elf
#if TAG_MAJOR_VERSION == 34
    GENERIC_WATER_POP, // Dwarf
#endif
    GENERIC_WATER_POP, // Lair
    { // Swamp water monsters
      {  1,  4,   400, FLAT, MONS_SWAMP_WORM },
      {  1,  4,   100, FLAT, MONS_TYRANT_LEECH },
      {  1,  4,   100, FLAT, MONS_ALLIGATOR },
      {  1,  4,  1050, FLAT, MONS_NO_MONSTER },
    },
    GENERIC_WATER_POP, // Shoals
    { // Snake water monsters
      {  1,   4,  100, FALL, MONS_ELECTRIC_EEL },
      {  0,   4,  200, RISE, MONS_SEA_SNAKE },
    },
    GENERIC_WATER_POP, // Spider
    GENERIC_WATER_POP, // Slime
    GENERIC_WATER_POP, // Vaults
#if TAG_MAJOR_VERSION == 34
    GENERIC_WATER_POP, // Blade
#endif
    GENERIC_WATER_POP, // Crypt
    GENERIC_WATER_POP, // Tomb
#if TAG_MAJOR_VERSION > 34
    DEPTHS_WATER_POP,
#endif
    HELL_WATER_POP, // Vestibule
    HELL_WATER_POP, // Dis
    HELL_WATER_POP, // Geh
    HELL_WATER_POP, // Coc
    HELL_WATER_POP, // Tar
    GENERIC_WATER_POP, // Zot
#if TAG_MAJOR_VERSION == 34
    GENERIC_WATER_POP, // Forest
#endif
    GENERIC_WATER_POP, // Abyss
    GENERIC_WATER_POP, // Pan
    GENERIC_WATER_POP, // Zig
#if TAG_MAJOR_VERSION == 34
    GENERIC_WATER_POP, // Lab
#endif
    GENERIC_WATER_POP, // Bazaar
    GENERIC_WATER_POP, // Trove
    GENERIC_WATER_POP, // Sewer
    GENERIC_WATER_POP, // Ossuary
    GENERIC_WATER_POP, // Bailey
#if TAG_MAJOR_VERSION > 34
    GENERIC_WATER_POP, // Gauntlet
#endif
    GENERIC_WATER_POP, // IceCv
    GENERIC_WATER_POP, // Volcano
    GENERIC_WATER_POP, // WizLab
#if TAG_MAJOR_VERSION == 34
    DEPTHS_WATER_POP,
#endif
    GENERIC_WATER_POP, // Desolation
#if TAG_MAJOR_VERSION == 34
    GENERIC_WATER_POP, // Gauntlet
#endif
    GENERIC_WATER_POP, // Arena
    GENERIC_WATER_POP, // Crucible
};
COMPILE_CHECK(ARRAYSZ(population_water) == NUM_BRANCHES);

#define GENERIC_LAVA_POP {\
  {  1,  27,  100, FLAT, MONS_FIRE_BAT },\
  {  1,  27,  100, FLAT, MONS_FIRE_ELEMENTAL },\
  {  1,  27,   50, FLAT, MONS_MOLTEN_GARGOYLE },\
  {  1,  27,  145, FLAT, MONS_LAVA_SNAKE },\
  {  1,  27,   15, FLAT, MONS_SALAMANDER },\
  {  1,  27,  340, FLAT, MONS_NO_MONSTER },\
}

#define HELL_LAVA_POP {\
  {  1,  8,   300, FALL, MONS_NO_MONSTER }, \
  {  1,  7,    50, RISE, MONS_STOKER },\
  {  1,  7,    60, FLAT, MONS_CREEPING_INFERNO },\
  {  1,  7,    20, FLAT, MONS_SEARING_WRETCH },\
  {  1,  7,    70, FLAT, MONS_NO_MONSTER },\
}

// This is done to avoid duplicating the Depths list and can be
// changed once TAG_MAJOR_VERSION > 35
#define DEPTHS_LAVA_POP {\
  {  1,  6,   22, FALL, MONS_FIRE_ELEMENTAL },\
  {  1,  6,   22, FALL, MONS_FIRE_BAT },\
  {  1,  6,   11, FALL, MONS_MOLTEN_GARGOYLE },\
  {  1,  6,   60, FLAT, MONS_SALAMANDER },\
  {  1,  8,   85, SEMI, MONS_SALAMANDER_MYSTIC },\
  {  1,  8,   40, RISE, MONS_SALAMANDER_TYRANT },\
  {  1,  6,  400, FLAT, MONS_NO_MONSTER },\
}

// This list must be in the same order as the branch-type enum values.
static const vector<pop_entry> population_lava[] =
{
    { // Dungeon lava monsters
      {  7,  27,  145, FLAT, MONS_LAVA_SNAKE },
      {  11, 27,  360, RISE, MONS_FIRE_ELEMENTAL },
      {  11, 27,  145, RISE, MONS_MOLTEN_GARGOYLE },
      {  11, 27,   75, RISE, MONS_NO_MONSTER },
      {  7,  27,  290, FLAT, MONS_NO_MONSTER },
    },
    GENERIC_LAVA_POP, // Temple
    GENERIC_LAVA_POP, // Orc
    GENERIC_LAVA_POP, // Elf
#if TAG_MAJOR_VERSION == 34
    GENERIC_LAVA_POP, // Dwarf
#endif
    GENERIC_LAVA_POP, // Lair
    GENERIC_LAVA_POP, // Swamp
    GENERIC_LAVA_POP, // Shoals
    { // Snake lava monsters
      {  1,   4,  200, FLAT, MONS_LAVA_SNAKE },
      {  1,   4,  200, FLAT, MONS_SALAMANDER },
      {  0,   6,  65,  SEMI, MONS_SALAMANDER_MYSTIC },
      {  0,   6,  25,  RISE, MONS_SALAMANDER_TYRANT },
    },
    GENERIC_LAVA_POP, // Spider
    GENERIC_LAVA_POP, // Slime
    GENERIC_LAVA_POP, // Vaults
#if TAG_MAJOR_VERSION == 34
    GENERIC_LAVA_POP, // Blade
#endif
    GENERIC_LAVA_POP, // Crypt
    GENERIC_LAVA_POP, // Tomb
#if TAG_MAJOR_VERSION > 34
    DEPTHS_LAVA_POP,
#endif
    HELL_LAVA_POP, // Vestibule
    HELL_LAVA_POP, // Dis
    HELL_LAVA_POP, // Geh
    HELL_LAVA_POP, // Coc
    HELL_LAVA_POP, // Tar
    GENERIC_LAVA_POP, // Zot
#if TAG_MAJOR_VERSION == 34
    GENERIC_LAVA_POP, // Forest
#endif
    GENERIC_LAVA_POP, // Abyss
    GENERIC_LAVA_POP, // Pan
    GENERIC_LAVA_POP, // Zig
#if TAG_MAJOR_VERSION == 34
    GENERIC_LAVA_POP, // Lab
#endif
    GENERIC_LAVA_POP, // Bazaar
    GENERIC_LAVA_POP, // Trove
    GENERIC_LAVA_POP, // Sewer
    GENERIC_LAVA_POP, // Ossuary
    GENERIC_LAVA_POP, // Bailey
#if TAG_MAJOR_VERSION > 34
    GENERIC_LAVA_POP, // Gauntlet
#endif
    GENERIC_LAVA_POP, // IceCv
    GENERIC_LAVA_POP, // Volcano
    GENERIC_LAVA_POP, // WizLab
#if TAG_MAJOR_VERSION == 34
    DEPTHS_LAVA_POP,
#endif
    GENERIC_LAVA_POP, // Desolation
#if TAG_MAJOR_VERSION == 34
    GENERIC_LAVA_POP, // Gauntlet
#endif
    GENERIC_LAVA_POP, // Arena
    GENERIC_LAVA_POP, // Crucible
};

COMPILE_CHECK(ARRAYSZ(population_lava) == NUM_BRANCHES);
