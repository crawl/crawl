#ifndef MGEN_ENUM_H
#define MGEN_ENUM_H

enum band_type
{
    BAND_NO_BAND                = 0,
    BAND_KOBOLDS,
    BAND_ORCS,
    BAND_ORC_WARRIOR,
    BAND_ORC_KNIGHT,
    BAND_KILLER_BEES,           // 5
    BAND_FLYING_SKULLS,
    BAND_SLIME_CREATURES,
    BAND_YAKS,
    BAND_UGLY_THINGS,
    BAND_HELL_HOUNDS,           // 10
    BAND_JACKALS,
    BAND_HELL_KNIGHTS,
    BAND_ORC_HIGH_PRIEST,
    BAND_GNOLLS,
    BAND_WIGHTS,                // 15
    BAND_BUMBLEBEES,
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
    BAND_SKELETAL_WARRIORS,
    BAND_DRACONIAN,             // 45
    BAND_PANDEMONIUM_DEMON,
    BAND_HARPIES,
    BAND_ILSUIW,
    BAND_AZRAEL,
    BAND_DUVESSA,               // 50
    BAND_KHUFU,
    BAND_GOLDEN_EYE,
    BAND_PIKEL,
    BAND_MERFOLK_AQUAMANCER,
    BAND_MERFOLK_IMPALER,
    BAND_MERFOLK_JAVELINEER,
    BAND_ALLIGATOR,
    BAND_ELEPHANT,
    BAND_TROLLKONOR,
    BAND_DEEP_DWARF,
    NUM_BANDS                   // always last
};

enum demon_class_type
{
    DEMON_LESSER,                      //    0: Class V
    DEMON_COMMON,                      //    1: Class II-IV
    DEMON_GREATER,                     //    2: Class I
    DEMON_RANDOM,                      //    any of the above
};

enum holy_being_class_type
{
    HOLY_BEING_WARRIOR,                //    0: Daeva or Angel
};

enum dragon_class_type
{
    DRAGON_LIZARD,
    DRAGON_DRACONIAN,
    DRAGON_DRAGON,
};

enum proximity_type   // proximity to player to create monster
{
    PROX_ANYWHERE,
    PROX_CLOSE_TO_PLAYER,
    PROX_AWAY_FROM_PLAYER,
    PROX_NEAR_STAIRS,
};

enum mgen_flag_type
{
    MG_PERMIT_BANDS = 0x01,
    MG_FORCE_PLACE  = 0x02,
    MG_FORCE_BEH    = 0x04,
    MG_PLAYER_MADE  = 0x08,
    MG_PATROLLING   = 0x10,
    MG_BAND_MINION  = 0x20,
};

#endif
