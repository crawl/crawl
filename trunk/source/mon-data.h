#ifndef MONDATA_H
#define MONDATA_H

/*
   This whole file was very generously condensed from its initial ugly form
   by Wladimir van der Laan ($pellbinder).
 */


/* ******************************************************************

   (see "mon-util.h" for the gory details)

 - ordering does not matter, because seekmonster() searches the entire
   array ... probably not to most efficient thing to do, but so it goes

 - Here are the rows:
    - row 1: monster id, display character, display colour, name
    - row 2: monster flags
    - row 3: mass, experience modifier, charclass, holiness, resist magic
    - row 4: damage for each of four attacks
    - row 5: hit dice, described by four parameters
    - row 6: AC, evasion, speed, speed_inc, sec(spell), corpse_thingy,
             zombie size, shouts, intel
    - row 6: gmon_use class

 - Some further explanations:

    - colour: if BLACK, monster uses value of mons_sec
    - name: if an empty string, name generated automagically (see moname)
    - mass: if zero, the monster never leaves a corpse
    - charclass: base monster "type" for a classed monsters
    - holiness: holy - irritates some gods when killed, immunity from
                       holy wrath weapons
                natural - baseline monster type
                undead - immunity from draining, pain, torment; extra
                         damage from hoyl wrath/disruption; affected by
                         repel undead and holy word
                demonic - similar to undead, but holy wrath does even
                          more damage and disruption and repel undead
                          effects are ignored -- *no* automatic hellfire
                          resistance


   exp_mod
   - the experience given for killing this monster is calculated something
   like this:
   experience = hp_max * HD * HD * exp_mod / 10
   I think.


   resist_magic
   - If -3, = 3 (5?) * hit dice

   damage [4]
   - up to 4 different attacks

   hp_dice [4]
   - hit dice, min hp per HD, extra random hp per HD, fixed HP (unique mons)

   speed
   - less = slower. 5 = half, 20 = double speed.

   speed_inc
   - this is unnecessary and should be removed. 7 for all monsters.

   corpse_thingy
   - err, bad name. Describes effects of eating corpses.

   zombie_size
   - 0 = no zombie possibly, 1 = small zombie (z), 2 = large zombie (Z)

   shouts
   - various things monsters can do upon seeing you
   #define S_RANDOM -1
   #define S_SILENT 0
   #define S_SHOUT 1 //1=shout
   #define S_BARK 2 //2=bark
   #define S_SHOUT2 3 //3=shout twice - eg 2-headed ogres
   #define S_ROAR 4 //4=roar
   #define S_SCREAM 5 //5=scream
   #define S_BELLOW 6 //6=bellow (?)
   #define S_SCREECH 7 //7=screech
   #define S_BUZZ 8 //8=buzz
   #define S_MOAN 9 //9=moan

   intel explanation:
   - How smart it is. So far, differences here have little effects except
   for monster's chance of seeing you if stealthy, and really stupid monsters
   will walk through clouds

   gmon_use explanation:
     0 = uses nothing 
     1 = opens doors 
     2 = gets and can use its starting equipment
     3 = can use weapons/armour

 */


// monster 250: The Thing That Should Not Be(tm)
// do not remove, or seekmonster will crash on unknown mc request
// it is also a good prototype for new monstersst
{
    MONS_PROGRAM_BUG, 'B', LIGHTRED, "program bug",
    M_NO_EXP_GAIN,
    0, 10, MONS_PROGRAM_BUG, MH_NATURAL, -3,
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    0, 0, 0, 0, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

// real monsters begin here {dlb}:
{
    MONS_GIANT_ANT, 'a', DARKGREY, "giant ant",
    M_ED_POISON,
    700, 10, MONS_GIANT_ANT, MH_NATURAL, -3,
    { 8, 0, 0, 0 },
    { 3, 3, 5, 0 },
    4, 10, 12, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,

{
    MONS_GIANT_BAT, 'b', DARKGREY, "giant bat",
    M_FLIES | M_SEE_INVIS | M_WARM_BLOOD,
    150, 4, MONS_GIANT_BAT, MH_NATURAL, -1,
    { 1, 0, 0, 0 },
    { 1, 2, 3, 0 },
    1, 14, 30, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_CENTAUR, 'c', BROWN, "centaur",
    M_WARM_BLOOD,
    1500, 10, MONS_CENTAUR, MH_NATURAL, -3,
    { 10, 0, 0, 0 },
    { 4, 3, 5, 0 },
    3, 7, 15, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_RED_DEVIL, '4', RED, "red devil",
    M_RES_POISON | M_RES_HELLFIRE | M_ED_COLD | M_FLIES,
    0, 10, MONS_RED_DEVIL, MH_DEMONIC, -7,
    { 18, 0, 0, 0 },
    { 5, 3, 5, 0 },
    10, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_ETTIN, 'C', BROWN, "ettin",
    M_WARM_BLOOD,
    0, 10, MONS_ETTIN, MH_NATURAL, -3,
    { 18, 12, 0, 0 },
    { 7, 3, 5, 0 },
    3, 4, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_SHOUT2, I_NORMAL, 
    MONUSE_STARTING_EQUIPMENT
}
,

{
    MONS_FUNGUS, 'f', LIGHTGREY, "fungus",
    M_NO_EXP_GAIN | M_RES_POISON,
    0, 10, MONS_FUNGUS, MH_PLANT, 5000,
    { 0, 0, 0, 0 },
    { 8, 3, 5, 0 },
    1, 0, 0, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_GOBLIN, 'g', LIGHTGREY, "goblin",
    M_WARM_BLOOD,
    400, 10, MONS_GOBLIN, MH_NATURAL, -1,
    { 4, 0, 0, 0 },
    { 1, 2, 4, 0 },
    0, 12, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_HOUND, 'h', BROWN, "hound",
    M_SEE_INVIS | M_WARM_BLOOD,
    300, 10, MONS_HOUND, MH_NATURAL, -3,
    { 6, 0, 0, 0 },
    { 3, 3, 5, 0 },
    2, 13, 15, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_BARK, I_ANIMAL, 
    MONUSE_NOTHING
}
,

// note: these things regenerate
{
    MONS_IMP, '5', RED, "imp",
    M_RES_POISON | M_RES_HELLFIRE | M_ED_COLD | M_FLIES | M_SEE_INVIS | M_SPEAKS,
    0, 13, MONS_IMP, MH_DEMONIC, -9,
    { 4, 0, 0, 0 },
    { 3, 3, 3, 0 },
    3, 14, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_JACKAL, 'j', YELLOW, "jackal",
    M_WARM_BLOOD,
    200, 10, MONS_JACKAL, MH_NATURAL, -1,
    { 3, 0, 0, 0 },
    { 1, 3, 5, 0 },
    2, 12, 14, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_BARK, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_KILLER_BEE, 'k', YELLOW, "killer bee",
    M_ED_POISON | M_FLIES,
    150, 11, MONS_KILLER_BEE, MH_NATURAL, -3,
    { 10, 0, 0, 0 },
    { 3, 3, 5, 0 },
    2, 18, 20, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_BUZZ, I_INSECT, 
    MONUSE_NOTHING
}
,

{
    MONS_KILLER_BEE_LARVA, 'w', LIGHTGREY, "killer bee larva",
    M_ED_POISON | M_NO_SKELETON,
    150, 5, MONS_KILLER_BEE_LARVA, MH_NATURAL, -3,
    { 3, 0, 0, 0 },
    { 1, 3, 5, 0 },
    1, 5, 5, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,

{
    MONS_MANTICORE, 'm', BROWN, "manticore",
    M_WARM_BLOOD,
    1800, 10, MONS_MANTICORE, MH_NATURAL, -3,
    { 14, 8, 8, 0 },
    { 9, 3, 5, 0 },
    5, 7, 7, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

// this thing doesn't have nr. 13 for nothing, has it? ($pellbinder)
{
    MONS_NECROPHAGE, 'n', DARKGREY, "necrophage",
    M_RES_POISON | M_RES_COLD,
    500, 10, MONS_NECROPHAGE, MH_UNDEAD, -5,
    { 8, 0, 0, 0 },
    { 5, 3, 5, 0 },
    2, 10, 10, 7, MST_NO_SPELLS, CE_HCL, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_ORC, 'o', LIGHTRED, "orc",
    M_WARM_BLOOD,
    600, 10, MONS_ORC, MH_NATURAL, -3,
    { 5, 0, 0, 0 },
    { 1, 4, 6, 0 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

// XP modifier is 5 for these, because they really aren't all that
// dangerous, but still come out at 200+ XP
{
    MONS_PHANTOM, 'p', BLUE, "phantom",
    M_RES_POISON | M_RES_COLD,
    0, 5, MONS_PHANTOM, MH_UNDEAD, -4,
    { 10, 0, 0, 0 },
    { 7, 3, 5, 0 },
    3, 13, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_QUASIT, 'q', LIGHTGREY, "quasit",
    M_RES_POISON | M_RES_FIRE | M_RES_COLD,
    0, 10, MONS_QUASIT, MH_DEMONIC, 50,
    { 3, 2, 2, 0 },
    { 3, 2, 6, 0 },
    5, 17, 15, 7, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,

{
    MONS_RAT, 'r', BROWN, "rat",
    M_WARM_BLOOD,
    200, 10, MONS_RAT, MH_NATURAL, -1,
    { 3, 0, 0, 0 },
    { 1, 1, 3, 0 },
    1, 10, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_SCORPION, 's', DARKGREY, "scorpion",
    M_ED_POISON,
    500, 10, MONS_SCORPION, MH_NATURAL, -3,
    { 10, 0, 0, 0 },
    { 3, 3, 5, 0 },
    5, 10, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,

/* ******************************************************************
// the tunneling worm is no more ...
// not until it can be reimplemented safely {dlb}
{
    MONS_TUNNELING_WORM, 't', LIGHTRED, "tunneling worm",
    M_RES_POISON,
    0, 10, 19, MH_NATURAL, 5000,
    { 50, 0, 0, 0 },
    { 10, 5, 5, 0 },
    3, 3, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_REPTILE, 
    MONUSE_NOTHING
}
,
****************************************************************** */

{
    MONS_UGLY_THING, 'u', BROWN, "ugly thing",
    M_WARM_BLOOD | M_AMPHIBIOUS,
    600, 10, MONS_UGLY_THING, MH_NATURAL, -3,
    { 12, 0, 0, 0 },
    { 8, 3, 5, 0 },
    3, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_FIRE_VORTEX, 'v', RED, "fire vortex",
    M_RES_POISON | M_RES_FIRE | M_ED_COLD | M_RES_ELEC | M_LEVITATE | M_CONFUSED,
    0, 5, MONS_FIRE_VORTEX, MH_NONLIVING, 5000,
    { 30, 0, 0, 0 },
    { 3, 3, 5, 0 },
    0, 5, 15, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_WORM, 'w', LIGHTRED, "worm",
    M_NO_SKELETON,
    350, 4, MONS_WORM, MH_NATURAL, -2,
    { 12, 0, 0, 0 },
    { 5, 3, 5, 0 },
    1, 5, 6, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,

// random
{
    MONS_ABOMINATION_SMALL, 'x', BLACK, "abomination",
    M_NO_FLAGS,
    0, 10, MONS_ABOMINATION_SMALL, MH_DEMONIC, -5,
    { 23, 0, 0, 0 },
    { 6, 2, 5, 0 },
    0, 0, 0, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_YELLOW_WASP, 'y', YELLOW, "yellow wasp",
    M_ED_POISON | M_FLIES,
    220, 12, MONS_YELLOW_WASP, MH_NATURAL, -3,
    { 13, 0, 0, 0 },
    { 4, 3, 5, 0 },
    5, 14, 15, 7, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,

// small zombie
{
    MONS_ZOMBIE_SMALL, 'z', BROWN, "",
    M_RES_POISON | M_RES_COLD,
    0, 6, MONS_ZOMBIE_SMALL, MH_UNDEAD, -1,
    { 10, 0, 0, 0 },
    { 1, 5, 5, 0 },
    0, 4, 5, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_ANGEL, 'A', WHITE, "Angel",
    M_RES_POISON | M_FLIES | M_RES_ELEC | M_SPELLCASTER,
    0, 10, MONS_ANGEL, MH_HOLY, -8,
    { 20, 0, 0, 0 },
    { 9, 3, 5, 0 },
    10, 10, 10, 7, MST_ANGEL, CE_NOCORPSE, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_GIANT_BEETLE, 'B', DARKGREY, "giant beetle",
    M_ED_POISON,
    1000, 10, MONS_GIANT_BEETLE, MH_NATURAL, -3,
    { 20, 0, 0, 0 },
    { 5, 7, 6, 0 },
    10, 3, 5, 7, MST_NO_SPELLS, CE_POISONOUS, Z_BIG, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,

{
    MONS_CYCLOPS, 'C', BROWN, "cyclops",
    M_WARM_BLOOD,
    2500, 10, MONS_CYCLOPS, MH_NATURAL, -3,
    { 35, 0, 0, 0 },
    { 9, 3, 5, 0 },
    5, 3, 7, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_STARTING_EQUIPMENT
}
,

{
    MONS_DRAGON, 'D', GREEN, "dragon",
      M_RES_POISON | M_RES_FIRE | M_ED_COLD | M_FLIES, //jmf: warm blood?
    2200, 12, MONS_DRAGON, MH_NATURAL, -4,
    { 20, 13, 13, 0 },
    { 12, 5, 5, 0 },
    10, 8, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

// These guys get understated because the experience code can't see
// that they wield two weapons... I'm raising their xp modifier. -- bwr
{
    MONS_TWO_HEADED_OGRE, 'O', LIGHTRED, "two-headed ogre",
    M_WARM_BLOOD,
    1500, 15, MONS_TWO_HEADED_OGRE, MH_NATURAL, -4,
    { 17, 13, 0, 0 },
    { 6, 3, 5, 0 },
    1, 4, 8, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_SHOUT2, I_NORMAL, 
    MONUSE_STARTING_EQUIPMENT
}
,

{
    MONS_FIEND, '1', LIGHTRED, "Fiend", //jmf: was RED, like Balrog
    M_RES_POISON | M_RES_HELLFIRE | M_ED_COLD | M_FLIES | M_SEE_INVIS,
    0, 18, MONS_FIEND, MH_DEMONIC, -12,
    { 25, 15, 15, 0 },
    { 18, 3, 5, 0 },
      15, 6, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_GIANT_SPORE, 'G', GREEN, "giant spore",
    M_RES_POISON | M_LEVITATE,
    0, 10, MONS_GIANT_SPORE, MH_NATURAL, -3,
    { 1, 0, 0, 0 },
    { 1, 0, 0, 1 },
    0, 10, 15, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_HOBGOBLIN, 'g', BROWN, "hobgoblin",
    M_WARM_BLOOD,
    500, 10, MONS_HOBGOBLIN, MH_NATURAL, -1,
    { 5, 0, 0, 0 },
    { 1, 4, 5, 0 },
    2, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_ICE_BEAST, 'I', WHITE, "ice beast",
    M_RES_POISON | M_ED_FIRE | M_RES_COLD,
    0, 12, MONS_ICE_BEAST, MH_NATURAL, -3,
    { 5, 0, 0, 0 },
    { 5, 3, 5, 0 },
    5, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_ANIMAL_LIKE, 
    MONUSE_NOTHING
}
,

{
    MONS_JELLY, 'J', LIGHTRED, "jelly",
    M_RES_POISON | M_SEE_INVIS | M_SPLITS | M_AMPHIBIOUS,
    0, 13, MONS_JELLY, MH_NATURAL, -3,
    { 8, 0, 0, 0 },
    { 3, 5, 5, 0 },
    0, 2, 9, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_EATS_ITEMS
}
,

{
    MONS_KOBOLD, 'K', BROWN, "kobold",
    M_WARM_BLOOD,
    400, 10, MONS_KOBOLD, MH_NATURAL, -1,
    { 4, 0, 0, 0 },
    { 1, 2, 3, 0 },
    2, 12, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_LICH, 'L', WHITE, "lich",
    M_RES_POISON | M_RES_COLD | M_RES_ELEC | M_SPELLCASTER | M_ACTUAL_SPELLS | M_SEE_INVIS,
    0, 16, MONS_LICH, MH_UNDEAD, -11,
    { 15, 0, 0, 0 },
    { 20, 2, 4, 0 },
    10, 10, 10, 7, MST_LICH_I, CE_NOCORPSE, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_MUMMY, 'M', WHITE, "mummy",
    M_RES_POISON | M_ED_FIRE | M_RES_COLD,
    0, 10, MONS_MUMMY, MH_UNDEAD, -5,
    { 20, 0, 0, 0 },
    { 3, 5, 3, 0 },
    3, 6, 6, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_ANIMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_GUARDIAN_NAGA, 'N', LIGHTGREEN, "guardian naga",
    M_RES_POISON | M_SPELLCASTER | M_SEE_INVIS | M_ACTUAL_SPELLS | M_WARM_BLOOD,
    350, 10, MONS_GUARDIAN_NAGA, MH_NATURAL, -6,
    { 19, 0, 0, 0 },
    { 8, 3, 5, 0 },
    6, 14, 15, 7, MST_GUARDIAN_NAGA, CE_MUTAGEN_RANDOM, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_OGRE, 'O', BROWN, "ogre",
    M_WARM_BLOOD,
    1300, 10, MONS_OGRE, MH_NATURAL, -3,
    { 17, 0, 0, 0 },
    { 5, 3, 5, 0 },
    1, 6, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_STARTING_EQUIPMENT
}
,

{
    MONS_PLANT, 'P', GREEN, "plant",
    M_NO_EXP_GAIN,
    0, 10, MONS_PLANT, MH_PLANT, 5000,
    { 0, 0, 0, 0 },
    { 10, 3, 5, 0 },
    10, 0, 0, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_QUEEN_BEE, 'Q', YELLOW, "queen bee",
    M_ED_POISON | M_FLIES,
    200, 14, MONS_QUEEN_BEE, MH_NATURAL, -3,
    { 20, 0, 0, 0 },
    { 7, 3, 5, 0 },
    10, 10, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,

{
    MONS_RAKSHASA, 'R', YELLOW, "rakshasa",
    M_RES_POISON | M_SPELLCASTER | M_SEE_INVIS,
    0, 15, MONS_RAKSHASA, MH_NATURAL, -10,
    { 20, 0, 0, 0 },
    { 10, 3, 5, 0 },
    10, 14, 10, 7, MST_RAKSHASA, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_SNAKE, 'S', GREEN, "snake",
    M_COLD_BLOOD | M_AMPHIBIOUS,
    200, 10, MONS_SNAKE, MH_NATURAL, -3,
    { 5, 0, 0, 0 },
    { 2, 3, 5, 0 },
    1, 15, 13, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_REPTILE, 
    MONUSE_NOTHING
}
,

{
    MONS_TROLL, 'T', BROWN, "troll",
    M_WARM_BLOOD,
    1500, 10, MONS_TROLL, MH_NATURAL, -3,
    { 20, 15, 15, 0 },
    { 7, 3, 5, 0 },
    3, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_UNSEEN_HORROR, 'x', MAGENTA, "unseen horror",
    M_LEVITATE | M_SEE_INVIS | M_RES_ELEC | M_INVIS,
    0, 12, MONS_UNSEEN_HORROR, MH_NATURAL, -3,
    { 12, 0, 0, 0 },
    { 7, 3, 5, 0 },
    5, 10, 30, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_ANIMAL_LIKE, 
    MONUSE_NOTHING
}
,

{
    MONS_VAMPIRE, 'V', RED, "vampire",
    M_RES_POISON | M_RES_COLD | M_SPELLCASTER | M_SEE_INVIS,
    0, 11, MONS_VAMPIRE, MH_UNDEAD, -6,
    { 22, 0, 0, 0 },
    { 6, 3, 5, 0 },
    10, 10, 10, 7, MST_VAMPIRE, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_WRAITH, 'W', DARKGREY, "wraith",
    M_RES_POISON | M_RES_COLD | M_LEVITATE | M_SEE_INVIS,
    0, 11, MONS_WRAITH, MH_UNDEAD, -7,
    { 13, 0, 0, 0 },
    { 6, 3, 5, 0 },
    10, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

// Large abom: (the previous one was small)
{
    MONS_ABOMINATION_LARGE, 'X', BLACK, "abomination",
    M_NO_FLAGS,
    0, 10, MONS_ABOMINATION_LARGE, MH_DEMONIC, -7,
    { 40, 0, 0, 0 },
    { 11, 2, 5, 0 },
    0, 0, 0, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_YAK, 'Y', BROWN, "yak",
      M_WARM_BLOOD,
    1200, 10, MONS_YAK, MH_NATURAL, -3,
    { 18, 0, 0, 0 },
    { 7, 3, 5, 0 },
    4, 7, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_BELLOW, I_ANIMAL, 
    MONUSE_NOTHING
}
,

// big zombie
{
    MONS_ZOMBIE_LARGE, 'Z', BROWN, "",
    M_RES_POISON | M_RES_COLD,
    0, 6, MONS_ZOMBIE_LARGE, MH_UNDEAD, -1,
    { 23, 0, 0, 0 },
    { 6, 3, 5, 0 },
    8, 5, 5, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_ORC_WARRIOR, 'o', YELLOW, "orc warrior",
    M_WARM_BLOOD,
    0, 10, MONS_ORC, MH_NATURAL, -3,
    { 20, 0, 0, 0 },
    { 4, 4, 6, 0 },
    0, 13, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_KOBOLD_DEMONOLOGIST, 'K', MAGENTA, "kobold demonologist",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD,
    0, 10, MONS_KOBOLD, MH_NATURAL, -5,
    { 4, 0, 0, 0 },
    { 4, 3, 5, 0 },
    2, 13, 10, 7, MST_KOBOLD_DEMONOLOGIST, CE_POISONOUS, Z_NOZOMBIE, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_ORC_WIZARD, 'o', MAGENTA, "orc wizard",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD,
    0, 10, MONS_ORC, MH_NATURAL, -5,
    { 5, 0, 0, 0 },
    { 3, 3, 4, 0 },
    1, 12, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_ORC_KNIGHT, 'o', LIGHTCYAN, "orc knight",
    M_WARM_BLOOD,
    0, 10, MONS_ORC, MH_NATURAL, -3,
    { 25, 0, 0, 0 },
    { 9, 4, 7, 0 },
    2, 13, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

/* ******************************************************************
// the tunneling worm is no more ...
// not until it can be reimplemented safely {dlb}
{
    MONS_WORM_TAIL, '~', LIGHTRED, "worm tail",
    M_NO_EXP_GAIN | M_RES_POISON,
    0, 10, 56, MH_NATURAL, 5000,
    { 0, 0, 0, 0 },
    { 10, 5, 5, 0 },
    3, 3, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,
****************************************************************** */

{
    MONS_WYVERN, 'D', LIGHTRED, "wyvern",
      M_NO_FLAGS, //jmf: warm blood?
    2000, 10, MONS_WYVERN, MH_NATURAL, -3,
    { 20, 0, 0, 0 },
    { 5, 3, 5, 0 },
    5, 10, 15, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_BIG_KOBOLD, 'K', RED, "big kobold",
    M_WARM_BLOOD,
    0, 10, MONS_BIG_KOBOLD, MH_NATURAL, -3,
    { 7, 0, 0, 0 },
    { 5, 3, 5, 0 },
    3, 12, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_GIANT_EYEBALL, 'G', WHITE, "giant eyeball",
    M_NO_SKELETON | M_LEVITATE,
    400, 10, MONS_GIANT_EYEBALL, MH_NATURAL, -3,
    { 0, 0, 0, 0 },
    { 3, 3, 5, 0 },
    0, 1, 3, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_WIGHT, 'W', LIGHTGREY, "wight",
    M_RES_POISON | M_RES_COLD,
    0, 10, MONS_WIGHT, MH_UNDEAD, -4,
    { 8, 0, 0, 0 },
    { 3, 3, 5, 0 },
    4, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_OKLOB_PLANT, 'P', GREEN, "oklob plant",
    M_RES_POISON,
    0, 10, MONS_OKLOB_PLANT, MH_PLANT, -3,
    { 0, 0, 0, 0 },
    { 10, 3, 5, 0 },
    10, 0, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_WOLF_SPIDER, 's', BROWN, "wolf spider",
    M_ED_POISON,
    800, 10, MONS_WOLF_SPIDER, MH_NATURAL, -3,
    { 20, 0, 0, 0 },
    { 8, 3, 5, 0 },
    3, 10, 15, 7, MST_NO_SPELLS, CE_POISONOUS, Z_BIG, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,

{
    MONS_SHADOW, ' ', BLACK, "shadow",
    M_RES_POISON | M_RES_COLD | M_SEE_INVIS,
    0, 10, MONS_SHADOW, MH_UNDEAD, -5,
    { 5, 0, 0, 0 },
    { 3, 3, 5, 0 },
    12, 10, 10, 7, BLACK, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_ANIMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_HUNGRY_GHOST, 'p', GREEN, "hungry ghost",
    M_RES_POISON | M_RES_COLD | M_SEE_INVIS | M_FLIES,
    0, 10, MONS_HUNGRY_GHOST, MH_UNDEAD, -4,
    { 5, 0, 0, 0 },
    { 7, 3, 5, 0 },
    0, 17, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_EYE_OF_DRAINING, 'G', LIGHTGREY, "eye of draining",
    M_NO_SKELETON | M_LEVITATE | M_SEE_INVIS,
    400, 10, MONS_EYE_OF_DRAINING, MH_NATURAL, 5000,
    { 0, 0, 0, 0 },
    { 7, 3, 5, 0 },
    3, 1, 5, 7, MST_NO_SPELLS, CE_MUTAGEN_RANDOM, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_BUTTERFLY, 'b', BLACK, "butterfly",
    M_FLIES | M_ED_POISON | M_CONFUSED,
    150, 10, MONS_BUTTERFLY, MH_NATURAL, -3,
    { 0, 0, 0, 0 },
    { 1, 3, 5, 0 },
    0, 25, 25, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,

{
    MONS_WANDERING_MUSHROOM, 'f', BROWN, "wandering mushroom",
    M_RES_POISON,
    0, 10, MONS_WANDERING_MUSHROOM, MH_PLANT, -3,
    { 20, 0, 0, 0 },
    { 8, 3, 5, 0 },
    5, 0, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_EFREET, 'E', RED, "efreet",
    M_RES_POISON | M_RES_FIRE | M_ED_COLD | M_SPELLCASTER | M_LEVITATE,
    0, 12, MONS_EFREET, MH_DEMONIC, -3,
    { 12, 0, 0, 0 },
    { 7, 3, 5, 0 },
    10, 5, 10, 7, MST_EFREET, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_BRAIN_WORM, 'w', LIGHTMAGENTA, "brain worm",
    M_SPELLCASTER,
    150, 10, MONS_BRAIN_WORM, MH_NATURAL, -3,
    { 0, 0, 0, 0 },
    { 5, 3, 3, 0 },
    1, 5, 10, 7, MST_BRAIN_WORM, CE_POISONOUS, Z_SMALL, S_SILENT, I_REPTILE, 
    MONUSE_NOTHING
}
,

{
    MONS_GIANT_ORANGE_BRAIN, 'G', LIGHTRED, "giant orange brain",
    M_NO_SKELETON | M_SPELLCASTER | M_LEVITATE | M_SEE_INVIS,
    1000, 13, MONS_GIANT_ORANGE_BRAIN, MH_NATURAL, -8,
    { 0, 0, 0, 0 },
    { 10, 3, 5, 0 },
    2, 4, 10, 7, MST_GIANT_ORANGE_BRAIN, CE_MUTAGEN_RANDOM, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_BOULDER_BEETLE, 'B', LIGHTGREY, "boulder beetle",
    M_ED_POISON,
    2500, 10, MONS_BOULDER_BEETLE, MH_NATURAL, -3,
    { 35, 0, 0, 0 },
    { 9, 3, 5, 0 },
    20, 2, 3, 7, MST_NO_SPELLS, CE_POISONOUS, Z_BIG, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,

{
    MONS_FLYING_SKULL, 'z', WHITE, "flying skull",
    M_RES_POISON | M_RES_FIRE | M_RES_COLD | M_RES_ELEC | M_LEVITATE,
    0, 10, MONS_FLYING_SKULL, MH_UNDEAD, -3,
    { 7, 0, 0, 0 },
    { 2, 3, 5, 0 },
    10, 17, 15, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SCREAM, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_HELL_HOUND, 'h', DARKGREY, "hell hound",
    M_RES_POISON | M_RES_HELLFIRE | M_ED_COLD | M_SEE_INVIS,
    0, 10, MONS_HELL_HOUND, MH_DEMONIC, -3,
    { 13, 0, 0, 0 },
    { 5, 3, 5, 0 },
    6, 13, 15, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_BARK, I_NORMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_MINOTAUR, 'm', LIGHTRED, "minotaur",
    M_WARM_BLOOD,
    1500, 10, MONS_MINOTAUR, MH_NATURAL, -3,
    { 35, 0, 0, 0 },
    { 13, 3, 5, 0 },
    5, 7, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_BELLOW, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_ICE_DRAGON, 'D', WHITE, "ice dragon",
    M_RES_POISON | M_ED_FIRE | M_RES_COLD | M_FLIES,
    2200, 10, MONS_ICE_DRAGON, MH_NATURAL, -3,
    { 17, 17, 17, 0 },
    { 12, 5, 5, 0 },
    10, 8, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_SLIME_CREATURE, 'J', GREEN, "slime creature",
    M_RES_POISON | M_AMPHIBIOUS,
    0, 5, MONS_SLIME_CREATURE, MH_NATURAL, -3,
    { 22, 0, 0, 0 },
    { 11, 3, 5, 0 },
    1, 4, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_FREEZING_WRAITH, 'W', LIGHTBLUE, "freezing wraith",
    M_RES_POISON | M_ED_FIRE | M_RES_COLD | M_LEVITATE | M_SEE_INVIS,
    0, 10, MONS_FREEZING_WRAITH, MH_UNDEAD, -4,
    { 19, 0, 0, 0 },
    { 8, 3, 5, 0 },
    12, 10, 8, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

// fake R - conjured by the R's illusion spell.
{
    MONS_RAKSHASA_FAKE, 'R', YELLOW, "rakshasa",
    M_RES_POISON,
    0, 10, MONS_RAKSHASA_FAKE, MH_NATURAL, 5000,
    { 0, 0, 0, 0 },
    { 1, 0, 0, 1 },
    0, 30, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_ROAR, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_GREAT_ORB_OF_EYES, 'G', LIGHTGREEN, "great orb of eyes",
    M_NO_SKELETON | M_RES_POISON | M_SPELLCASTER | M_LEVITATE | M_SEE_INVIS,
    900, 13, MONS_GREAT_ORB_OF_EYES, MH_NATURAL, 5000,
    { 20, 0, 0, 0 },
    { 12, 3, 5, 0 },
    10, 3, 10, 7, MST_GREAT_ORB_OF_EYES, CE_MUTAGEN_RANDOM, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_HELLION, '3', BLACK, "hellion",
    M_RES_POISON | M_RES_HELLFIRE | M_ED_COLD | M_SPELLCASTER | M_ON_FIRE,
    0, 11, MONS_HELLION, MH_DEMONIC, -7,
    { 10, 0, 0, 0 },
    { 7, 3, 5, 0 },
    5, 10, 13, 7, RED, CE_NOCORPSE, Z_NOZOMBIE, S_SCREAM, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_ROTTING_DEVIL, '4', GREEN, "rotting devil",
    M_RES_POISON | M_RES_COLD,
    0, 10, MONS_ROTTING_DEVIL, MH_DEMONIC, -7,
    { 8, 0, 0, 0 },
    { 5, 3, 5, 0 },
    2, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_TORMENTOR, '3', YELLOW, "tormentor",
    M_RES_POISON | M_RES_FIRE | M_SPELLCASTER | M_FLIES | M_SPEAKS,
    0, 10, MONS_TORMENTOR, MH_DEMONIC, -6,
    { 8, 8, 0, 0 },
    { 7, 3, 5, 0 },
    12, 12, 13, 7, MST_TORMENTOR, CE_NOCORPSE, Z_NOZOMBIE, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_REAPER, '2', LIGHTGREY, "reaper",
    M_RES_POISON | M_RES_COLD | M_SEE_INVIS,
    0, 10, MONS_REAPER, MH_DEMONIC, 5000,
    { 32, 0, 0, 0 },
    { 8, 3, 5, 0 },
    15, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_STARTING_EQUIPMENT
}
,

{
    MONS_SOUL_EATER, '2', DARKGREY, "soul eater",
    M_RES_POISON | M_RES_COLD | M_LEVITATE | M_SEE_INVIS,
    0, 12, MONS_SOUL_EATER, MH_DEMONIC, -10,
    { 25, 0, 0, 0 },
    { 11, 3, 5, 0 },
    18, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_HAIRY_DEVIL, '4', LIGHTRED, "hairy devil",
    M_RES_POISON,
    0, 10, MONS_HAIRY_DEVIL, MH_DEMONIC, -4,
    { 9, 9, 0, 0 },
    { 6, 3, 5, 0 },
    7, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_ICE_DEVIL, '2', WHITE, "ice devil",
    M_RES_POISON | M_ED_FIRE | M_RES_COLD | M_SEE_INVIS,
    0, 11, MONS_ICE_DEVIL, MH_DEMONIC, -6,
    { 16, 0, 0, 0 },
    { 11, 3, 5, 0 },
    12, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_BLUE_DEVIL, '3', BLUE, "blue devil",
    M_RES_POISON | M_ED_FIRE | M_RES_COLD | M_FLIES,
    0, 10, MONS_BLUE_DEVIL, MH_DEMONIC, -5,
    { 21, 0, 0, 0 },
    { 7, 3, 5, 0 },
    14, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

// random
{
    MONS_BEAST, '4', BROWN, "beast",
    M_NO_FLAGS,
    0, 10, MONS_BEAST, MH_DEMONIC, -3,
    { 12, 0, 0, 0 },
    { 5, 3, 5, 0 },
    0, 0, 0, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_RANDOM, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_IRON_DEVIL, '3', CYAN, "iron devil",
    M_RES_ELEC | M_RES_POISON | M_RES_HELLFIRE | M_RES_COLD,
    0, 10, MONS_IRON_DEVIL, MH_DEMONIC, -6,
    { 14, 14, 0, 0 },
    { 8, 3, 5, 0 },
    16, 8, 8, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SCREECH, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_GLOWING_SHAPESHIFTER, '@', RED, "glowing shapeshifter",
    M_NO_FLAGS,
    600, 10, MONS_SHAPESHIFTER, MH_NATURAL, -6,
    { 15, 0, 0, 0 },
    { 10, 3, 5, 0 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_MUTAGEN_RANDOM, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_SHAPESHIFTER, '@', LIGHTRED, "shapeshifter",
    M_NO_FLAGS,
    600, 10, MONS_SHAPESHIFTER, MH_NATURAL, -6,
    { 5, 0, 0, 0 },
    { 7, 3, 5, 0 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_MUTAGEN_RANDOM, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_GIANT_MITE, 's', LIGHTRED, "giant mite",
    M_ED_POISON,
    350, 10, MONS_GIANT_MITE, MH_NATURAL, -1,
    { 5, 0, 0, 0 },
    { 2, 3, 5, 0 },
    1, 7, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,

{
    MONS_STEAM_DRAGON, 'd', LIGHTGREY, "steam dragon",
    M_SPELLCASTER | M_FLIES,
    1000, 10, MONS_STEAM_DRAGON, MH_NATURAL, -3,
    { 12, 0, 0, 0 },
    { 4, 5, 5, 0 },
    5, 10, 10, 7, MST_STEAM_DRAGON, CE_CLEAN, Z_BIG, S_SILENT, I_ANIMAL_LIKE, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_VERY_UGLY_THING, 'u', RED, "very ugly thing",
    M_WARM_BLOOD | M_AMPHIBIOUS,
    750, 10, MONS_VERY_UGLY_THING, MH_NATURAL, -3,
    { 17, 0, 0, 0 },
    { 12, 3, 5, 0 },
    4, 8, 8, 7, MST_NO_SPELLS, CE_MUTAGEN_RANDOM, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_ORC_SORCERER, 'o', DARKGREY, "orc sorcerer",
    M_RES_FIRE | M_SPELLCASTER | M_SEE_INVIS | M_SPEAKS | M_ACTUAL_SPELLS | M_WARM_BLOOD,
    600, 12, MONS_ORC, MH_NATURAL, -3,
    { 7, 0, 0, 0 },
    { 8, 2, 3, 0 },
    5, 12, 10, 7, MST_ORC_SORCERER, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_HIPPOGRIFF, 'H', BROWN, "hippogriff",
    M_FLIES | M_WARM_BLOOD,
    1000, 10, MONS_HIPPOGRIFF, MH_NATURAL, -3,
    { 10, 8, 8, 0 },
    { 7, 3, 5, 0 },
    2, 7, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_SCREECH, I_ANIMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_GRIFFON, 'H', YELLOW, "griffon",
    M_FLIES | M_WARM_BLOOD,
    1800, 10, MONS_GRIFFON, MH_NATURAL, -3,
    { 18, 10, 10, 0 },
    { 12, 3, 5, 0 },
    4, 6, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_SCREECH, I_ANIMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_HYDRA, 'D', LIGHTGREEN, "hydra",
    M_RES_POISON | M_AMPHIBIOUS,  // because it likes the swamp -- bwr
    1800, 11, MONS_HYDRA, MH_NATURAL, -3,
    { 18, 0, 0, 0 },
    { 13, 3, 5, 0 },
    0, 5, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_ROAR, I_REPTILE, 
    MONUSE_OPEN_DOORS
}
,

// small skeleton
{
    MONS_SKELETON_SMALL, 'z', LIGHTGREY, "",
    M_RES_POISON | M_RES_COLD,
    0, 10, MONS_SKELETON_SMALL, MH_UNDEAD, -1,
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    0, 0, 0, 0, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

// large skeleton
{
    MONS_SKELETON_LARGE, 'Z', LIGHTGREY, "",
    M_RES_POISON | M_RES_COLD,
    0, 10, MONS_SKELETON_LARGE, MH_UNDEAD, -1,
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    0, 0, 0, 0, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,


{
    MONS_HELL_KNIGHT, '@', RED, "hell knight",
    M_RES_FIRE | M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD,
    550, 10, MONS_HUMAN, MH_NATURAL, -3,
    { 13, 0, 0, 0 },
    { 10, 3, 6, 0 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_NECROMANCER, '@', DARKGREY, "necromancer",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD,
    550, 10, MONS_HUMAN, MH_NATURAL, -4,
    { 6, 0, 0, 0 },
    { 10, 2, 4, 0 },
    0, 13, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_WIZARD, '@', MAGENTA, "wizard",
    M_RES_ELEC | M_SPELLCASTER | M_SPEAKS | M_ACTUAL_SPELLS | M_WARM_BLOOD,
    550, 10, MONS_HUMAN, MH_NATURAL, -4,
    { 6, 0, 0, 0 },
    { 10, 2, 4, 0 },
    0, 13, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_ORC_PRIEST, 'o', LIGHTGREEN, "orc priest",
    M_SPELLCASTER | M_PRIEST | M_WARM_BLOOD,
    600, 10, MONS_ORC, MH_NATURAL, -4,
    { 6, 0, 0, 0 },
    { 3, 3, 4, 0 },
    1, 10, 10, 7, MST_ORC_PRIEST, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_ORC_HIGH_PRIEST, 'o', GREEN, "orc high priest",
    M_RES_HELLFIRE | M_SPELLCASTER | M_SEE_INVIS | M_SPEAKS | M_PRIEST | M_WARM_BLOOD,
      600, 10, MONS_ORC, MH_NATURAL, -4,
    { 7, 0, 0, 0 },
    { 11, 3, 4, 0 },
    1, 12, 10, 7, MST_ORC_HIGH_PRIEST, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

// this is a dummy monster, used for corpses
// mv:but it can be generated by polymorph spells and because IMO it's
// logical polymorph target so complete monster statistics should exist.
// Same thing for elf dummy monster.

{
    MONS_HUMAN, '@', LIGHTGRAY, "human",
    M_WARM_BLOOD,
    550, 10, MONS_HUMAN, MH_NATURAL, -3,
    { 10, 0, 0, 0 },
    { 1, 3, 5, 0 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_GNOLL, 'g', YELLOW, "gnoll",
    M_WARM_BLOOD,
    750, 10, MONS_GNOLL, MH_NATURAL, -3,
    { 9, 0, 0, 0 },
    { 2, 4, 5, 0 },
    2, 9, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_CLAY_GOLEM, '8', BROWN, "clay golem",
    M_RES_POISON | M_RES_FIRE | M_RES_COLD | M_RES_ELEC | M_SEE_INVIS,
    0, 10, MONS_CLAY_GOLEM, MH_NONLIVING, 5000,
    { 11, 11, 0, 0 },
    { 8, 7, 3, 0 },
    7, 5, 8, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_WOOD_GOLEM, '8', YELLOW, "wood golem",
    M_RES_POISON | M_ED_FIRE | M_RES_COLD | M_RES_ELEC,
    0, 10, MONS_WOOD_GOLEM, MH_NONLIVING, 5000,
    { 10, 0, 0, 0 },
    { 6, 6, 3, 0 },
    5, 6, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_STONE_GOLEM, '8', LIGHTGREY, "stone golem",
    M_RES_POISON | M_RES_FIRE | M_RES_COLD | M_RES_ELEC,
    0, 10, MONS_STONE_GOLEM, MH_NONLIVING, 5000,
    { 28, 0, 0, 0 },
    { 12, 7, 4, 0 },
    12, 4, 7, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_IRON_GOLEM, '8', CYAN, "iron golem",
    M_RES_POISON | M_RES_FIRE | M_RES_COLD | M_RES_ELEC | M_SEE_INVIS,
    0, 10, MONS_IRON_GOLEM, MH_NONLIVING, 5000,
    { 35, 0, 0, 0 },
    { 15, 7, 4, 0 },
    15, 3, 7, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_CRYSTAL_GOLEM, '8', WHITE, "crystal golem",
    M_RES_POISON | M_RES_FIRE | M_RES_COLD | M_RES_ELEC | M_SEE_INVIS,
    0, 10, MONS_CRYSTAL_GOLEM, MH_NONLIVING, 5000,
    { 40, 0, 0, 0 },
    { 13, 7, 4, 0 },
    22, 3, 7, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_TOENAIL_GOLEM, '8', LIGHTGREY, "toenail golem",
    M_RES_POISON | M_RES_FIRE | M_RES_COLD | M_RES_ELEC,
    0, 10, MONS_TOENAIL_GOLEM, MH_NONLIVING, 5000,
    { 13, 0, 0, 0 },
    { 9, 5, 3, 0 },
    8, 5, 8, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_MOTTLED_DRAGON, 'd', LIGHTMAGENTA, "mottled dragon",
    M_RES_POISON | M_RES_FIRE | M_SPELLCASTER | M_FLIES,
    1100, 10, MONS_MOTTLED_DRAGON, MH_NATURAL, -3,
    { 15, 0, 0, 0 },
    { 5, 3, 5, 0 },
    5, 10, 10, 7, MST_MOTTLED_DRAGON, CE_POISONOUS, Z_BIG, S_SILENT, I_ANIMAL_LIKE, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_EARTH_ELEMENTAL, '#', BROWN, "earth elemental",
    M_RES_POISON | M_RES_FIRE | M_RES_COLD | M_RES_ELEC,
    0, 10, MONS_EARTH_ELEMENTAL, MH_NONLIVING, 5000,
    { 40, 0, 0, 0 },
    { 6, 5, 5, 0 },
    14, 4, 6, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_FIRE_ELEMENTAL, '#', YELLOW, "fire elemental",
    M_RES_POISON | M_RES_HELLFIRE | M_ED_COLD | M_RES_ELEC | M_FLIES,
    0, 10, MONS_FIRE_ELEMENTAL, MH_NONLIVING, 5000,
    { 5, 0, 0, 0 },
    { 6, 3, 5, 0 },
    4, 12, 13, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_AIR_ELEMENTAL, 'v', LIGHTGREY, "air elemental",
    M_RES_ELEC | M_RES_POISON | M_LEVITATE | M_SEE_INVIS | M_FLIES,
    0, 5, MONS_AIR_ELEMENTAL, MH_NONLIVING, 5000,
    { 15, 0, 0, 0 },
    { 6, 3, 5, 0 },
    2, 18, 25, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS
}
,

// water elementals are later (with the other water monsters)

{
    MONS_ICE_FIEND, '1', WHITE, "Ice Fiend",
    M_RES_POISON | M_ED_FIRE | M_RES_COLD | M_SPELLCASTER | M_FLIES | M_SEE_INVIS | M_FROZEN,
    0, 10, MONS_ICE_FIEND, MH_DEMONIC, -12,
    { 25, 25, 0, 0 },
    { 18, 3, 5, 0 },
    15, 6, 10, 7, MST_ICE_FIEND, CE_CONTAMINATED, Z_NOZOMBIE, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_SHADOW_FIEND, '1', DARKGREY, "Shadow Fiend",
    M_RES_POISON | M_RES_COLD | M_RES_ELEC | M_SPELLCASTER | M_LEVITATE | M_SEE_INVIS,
    0, 10, MONS_SHADOW_FIEND, MH_DEMONIC, -13,
    { 25, 15, 15, 0 },
    { 18, 3, 5, 0 },
    15, 6, 10, 7, MST_SHADOW_FIEND, CE_CONTAMINATED, Z_NOZOMBIE, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_BROWN_SNAKE, 'S', BROWN, "brown snake",
    M_RES_POISON | M_COLD_BLOOD | M_AMPHIBIOUS,
    300, 10, MONS_BROWN_SNAKE, MH_NATURAL, -3,
    { 10, 0, 0, 0 },
    { 4, 3, 5, 0 },
    2, 15, 14, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_HISS, I_REPTILE, 
    MONUSE_NOTHING
}
,

{
    MONS_GIANT_LIZARD, 'l', GREEN, "giant lizard",
    M_COLD_BLOOD,
    600, 10, MONS_GIANT_LIZARD, MH_NATURAL, -3,
    { 20, 0, 0, 0 },
    { 5, 3, 5, 0 },
    4, 10, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_REPTILE, 
    MONUSE_NOTHING
}
,

{
    MONS_SPECTRAL_WARRIOR, 'W', LIGHTGREEN, "spectral warrior",
    M_RES_POISON | M_RES_COLD | M_LEVITATE | M_SEE_INVIS,
    0, 13, MONS_SPECTRAL_WARRIOR, MH_UNDEAD, -6,
    { 18, 0, 0, 0 },
    { 9, 3, 5, 0 },
    12, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_PULSATING_LUMP, 'J', RED, "pulsating lump",
    M_RES_POISON | M_SEE_INVIS,
    0, 3, MONS_PULSATING_LUMP, MH_NATURAL, -3,
    { 13, 0, 0, 0 },
    { 10, 3, 5, 0 },
    2, 6, 5, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_STORM_DRAGON, 'D', LIGHTBLUE, "storm dragon",
    M_RES_ELEC | M_RES_COLD | M_SPELLCASTER | M_FLIES,
    2800, 12, MONS_STORM_DRAGON, MH_NATURAL, -5,
    { 25, 15, 15, 0 },
    { 14, 5, 5, 0 },
    13, 10, 12, 7, MST_STORM_DRAGON, CE_CLEAN, Z_BIG, S_ROAR, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_YAKTAUR, 'c', LIGHTRED, "yaktaur",
    M_WARM_BLOOD,
    2000, 10, MONS_YAKTAUR, MH_NATURAL, -3,
    { 15, 0, 0, 0 },
    { 8, 3, 5, 0 },
    4, 4, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_DEATH_YAK, 'Y', DARKGREY, "death yak",
    M_WARM_BLOOD,
    1500, 10, MONS_DEATH_YAK, MH_NATURAL, -5,
    { 30, 0, 0, 0 },
    { 14, 3, 5, 0 },
    9, 5, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_BIG, S_BELLOW, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_ROCK_TROLL, 'T', LIGHTGREY, "rock troll",
    M_WARM_BLOOD,
    2200, 11, MONS_ROCK_TROLL, MH_NATURAL, -4,
    { 30, 20, 20, 0 },
    { 11, 3, 5, 0 },
    13, 6, 8, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_STONE_GIANT, 'C', LIGHTGREY, "stone giant",
    M_WARM_BLOOD,
    3000, 10, MONS_STONE_GIANT, MH_NATURAL, -4,
    { 45, 0, 0, 0 },
    { 16, 3, 5, 0 },
    12, 2, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_STARTING_EQUIPMENT
}
,

{
    MONS_FLAYED_GHOST, 'p', RED, "flayed ghost",
    M_RES_POISON | M_FLIES,
    0, 10, MONS_FLAYED_GHOST, MH_UNDEAD, -4,
    { 30, 0, 0, 0 },
    { 11, 3, 5, 0 },
    0, 14, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_BUMBLEBEE, 'k', RED, "bumblebee",
    M_ED_POISON | M_FLIES,
    300, 10, MONS_BUMBLEBEE, MH_NATURAL, -3,
    { 20, 0, 0, 0 },
    { 7, 3, 5, 0 },
    4, 15, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_BUZZ, I_INSECT, 
    MONUSE_NOTHING
}
,

{
    MONS_REDBACK, 's', RED, "redback",
    M_ED_POISON,
    1000, 14, MONS_REDBACK, MH_NATURAL, -3,
    { 18, 0, 0, 0 },
    { 6, 3, 5, 0 },
    2, 12, 15, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,

{
    MONS_INSUBSTANTIAL_WISP, 'p', LIGHTGREY, "insubstantial wisp",
    M_RES_ELEC | M_RES_POISON | M_RES_FIRE | M_RES_COLD | M_LEVITATE,
    0, 17, MONS_INSUBSTANTIAL_WISP, MH_NONLIVING, 5000,
    { 12, 0, 0, 0 },
    { 6, 1, 2, 0 },
    20, 20, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_MOAN, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_VAPOUR, '#', LIGHTGREY, "vapour",
    M_RES_ELEC | M_RES_POISON | M_SPELLCASTER | M_LEVITATE | M_SEE_INVIS | M_INVIS | M_CONFUSED,
    0, 21, MONS_VAPOUR, MH_NONLIVING, 5000,
    { 0, 0, 0, 0 },
    { 12, 2, 3, 0 },
    0, 12, 10, 7, MST_STORM_DRAGON, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_OGRE_MAGE, 'O', MAGENTA, "ogre-mage",
    M_RES_ELEC | M_SPELLCASTER | M_ACTUAL_SPELLS | M_SEE_INVIS | M_WARM_BLOOD,
    0, 16, MONS_OGRE, MH_NATURAL, -6,
    { 12, 0, 0, 0 },
    { 10, 3, 5, 0 },
    1, 7, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_SPINY_WORM, 'w', DARKGREY, "spiny worm",
    M_ED_POISON,
    1300, 13, MONS_SPINY_WORM, MH_NATURAL, -3,
    { 32, 0, 0, 0 },
    { 12, 3, 5, 0 },
    10, 6, 9, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

// these are named more explicitly when they attack, also when you use 'x'
//  to examine them.
{
    MONS_DANCING_WEAPON, '(', BLACK, "dancing weapon",
    M_RES_POISON | M_RES_HELLFIRE | M_RES_COLD | M_RES_ELEC | M_LEVITATE,
    0, 10, MONS_DANCING_WEAPON, MH_NONLIVING, 5000,
    { 30, 0, 0, 0 },
    { 15, 0, 0, 15 },
    10, 20, 15, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_TITAN, 'C', MAGENTA, "titan",
    M_RES_ELEC | M_SPELLCASTER | M_WARM_BLOOD | M_SEE_INVIS,
    3500, 12, MONS_TITAN, MH_NATURAL, -7,
    { 55, 0, 0, 0 },
    { 20, 3, 5, 0 },
    10, 3, 10, 7, MST_TITAN, CE_CLEAN, Z_BIG, S_SHOUT, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_GOLDEN_DRAGON, 'D', YELLOW, "golden dragon",
    M_RES_ELEC | M_RES_POISON | M_RES_FIRE | M_RES_COLD | M_SPELLCASTER | M_FLIES | M_SEE_INVIS,
    3000, 17, MONS_GOLDEN_DRAGON, MH_NATURAL, -8,
    { 40, 20, 20, 0 },
    { 18, 4, 4, 0 },
    15, 7, 10, 7, MST_GOLDEN_DRAGON, CE_POISONOUS, Z_BIG, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

// 147 - dummy monster, used for corpses etc.
//mv: have to exist because it's (and should be) valid polymorph target.
{
    MONS_ELF, 'e', DARKGREY, "elf",
    M_WARM_BLOOD,
    450, 10, MONS_ELF, MH_NATURAL, -3,
    { 10, 0, 0, 0 },
    { 3, 3, 3, 0 },
    0, 12, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SILENT, I_PLANT, 
    MONUSE_WEAPONS_ARMOUR
}
,


// Used to be "lindworm" and a GREEN 'l'...  I'm hoping that by 
// making it a 'd' and using an alternate spelling people will 
// more intuitively know that this isn't a regular lizard. -- bwr
{
    MONS_LINDWURM, 'd', LIGHTGREEN, "lindwurm",
    M_NO_FLAGS,
    1000, 11, MONS_LINDWURM, MH_NATURAL, -3,
    { 20, 10, 10, 0 },
    { 9, 3, 5, 0 },
    8, 6, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_ROAR, I_REPTILE, 
    MONUSE_NOTHING
}
,

{
    MONS_ELEPHANT_SLUG, 'm', LIGHTGREY, "elephant slug",
    M_ED_POISON | M_NO_SKELETON,
    1500, 10, MONS_ELEPHANT_SLUG, MH_NATURAL, -3,
    { 40, 0, 0, 0 },
    { 20, 5, 3, 0 },
    2, 1, 4, 10, MST_NO_SPELLS, CE_POISONOUS, Z_BIG, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,

{
    MONS_WAR_DOG, 'h', CYAN, "war dog",
    M_SEE_INVIS | M_WARM_BLOOD,
    350, 10, MONS_WAR_DOG, MH_NATURAL, -3,
    { 12, 0, 0, 0 },
    { 4, 3, 5, 0 },
    4, 15, 17, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_BARK, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_GREY_RAT, 'r', LIGHTGREY, "grey rat",
    M_WARM_BLOOD,
    250, 10, MONS_GREY_RAT, MH_NATURAL, -3,
    { 5, 0, 0, 0 },
    { 1, 3, 6, 0 },
    2, 12, 12, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SILENT, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_GREEN_RAT, 'r', LIGHTGREEN, "green rat",
    M_WARM_BLOOD,
    250, 10, MONS_GREEN_RAT, MH_NATURAL, -3,
    { 10, 0, 0, 0 },
    { 2, 3, 5, 0 },
    5, 11, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_ORANGE_RAT, 'r', LIGHTRED, "orange rat",
    M_WARM_BLOOD,
    250, 10, MONS_ORANGE_RAT, MH_NATURAL, -3,
    { 20, 0, 0, 0 },
    { 3, 3, 5, 0 },
    7, 10, 12, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_ROAR, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_BLACK_SNAKE, 'S', DARKGREY, "black snake",
    M_RES_POISON | M_COLD_BLOOD,
    500, 12, MONS_BLACK_SNAKE, MH_NATURAL, -3,
    { 20, 0, 0, 0 },
    { 7, 3, 5, 0 },
    4, 15, 18, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_HISS, I_REPTILE, 
    MONUSE_NOTHING
}
,

{
    MONS_SHEEP, 'Y', LIGHTGREY, "sheep",
    M_WARM_BLOOD,
    1200, 10, MONS_SHEEP, MH_NATURAL, -3,
    { 13, 0, 0, 0 },
    { 3, 3, 5, 0 },
    2, 7, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_BELLOW, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_GHOUL, 'n', RED, "ghoul",
    M_RES_POISON | M_RES_COLD,
    500, 12, MONS_GHOUL, MH_UNDEAD, -5,
    { 9, 0, 0, 0 },
    { 4, 3, 5, 0 },
    4, 10, 10, 7, MST_NO_SPELLS, CE_HCL, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_HOG, 'h', LIGHTRED, "hog",
    M_WARM_BLOOD,
    700, 10, MONS_HOG, MH_NATURAL, -3,
    { 14, 0, 0, 0 },
    { 6, 3, 5, 0 },
    2, 9, 13, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_GIANT_MOSQUITO, 'y', DARKGREY, "giant mosquito",
    M_ED_POISON | M_FLIES,
    100, 10, MONS_GIANT_MOSQUITO, MH_NATURAL, -3,
    { 10, 0, 0, 0 },
    { 1, 3, 5, 0 },
    0, 13, 12, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_WHINE, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_GIANT_CENTIPEDE, 's', GREEN, "giant centipede",
    M_ED_POISON,
    350, 10, MONS_GIANT_CENTIPEDE, MH_NATURAL, -3,
    { 2, 0, 0, 0 },
    { 2, 3, 3, 0 },
    2, 14, 13, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,



{
    MONS_IRON_TROLL, 'T', CYAN, "iron troll",
    M_RES_FIRE | M_RES_COLD | M_WARM_BLOOD,
    2400, 10, MONS_IRON_TROLL, MH_NATURAL, -5,
    { 35, 25, 25, 0 },
    { 16, 3, 5, 0 },
    20, 4, 7, 7, MST_NO_SPELLS, CE_POISONOUS, Z_BIG, S_ROAR, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_NAGA, 'N', GREEN, "naga",
    M_RES_POISON | M_SPELLCASTER | M_SEE_INVIS | M_WARM_BLOOD,
    750, 10, MONS_NAGA, MH_NATURAL, -6,
    { 6, 0, 0, 0 },
    { 5, 3, 5, 0 },
    6, 10, 8, 7, MST_NAGA, CE_POISONOUS, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_FIRE_GIANT, 'C', RED, "fire giant",
    M_RES_FIRE | M_SPELLCASTER | M_WARM_BLOOD | M_SEE_INVIS,
    2400, 11, MONS_FIRE_GIANT, MH_NATURAL, -4,
    { 30, 0, 0, 0 },
    { 16, 3, 6, 0 },
    8, 4, 10, 7, MST_EFREET, CE_CONTAMINATED, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_FROST_GIANT, 'C', LIGHTBLUE, "frost giant",
    M_RES_COLD | M_SPELLCASTER | M_WARM_BLOOD | M_SEE_INVIS,
    2600, 11, MONS_FROST_GIANT, MH_NATURAL, -4,
    { 35, 0, 0, 0 },
    { 16, 4, 5, 0 },
    9, 3, 10, 7, MST_FROST_GIANT, CE_CONTAMINATED, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_FIREDRAKE, 'd', RED, "firedrake",
    M_RES_FIRE | M_FLIES,
    900, 10, MONS_FIREDRAKE, MH_NATURAL, -3,
    { 8, 0, 0, 0 },
    { 6, 3, 5, 0 },
    3, 12, 12, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SILENT, I_ANIMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_SHADOW_DRAGON, 'D', DARKGREY, "shadow dragon",
    M_RES_POISON | M_RES_COLD | M_SPELLCASTER | M_FLIES | M_SEE_INVIS,
    2000, 12, MONS_SHADOW_DRAGON, MH_NATURAL, -5,
    { 20, 15, 15, 0 },
    { 17, 5, 5, 0 },
    15, 10, 10, 7, MST_SHADOW_DRAGON, CE_CLEAN, Z_BIG, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,
{
    MONS_YELLOW_SNAKE, 'S', YELLOW, "yellow snake",
    M_RES_POISON | M_COLD_BLOOD,
    400, 10, MONS_YELLOW_SNAKE, MH_NATURAL, -3,
    { 15, 0, 0, 0 },
    { 6, 3, 5, 0 },
    4, 14, 13, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_HISS, I_REPTILE, 
    MONUSE_NOTHING
}
,

{
    MONS_GREY_SNAKE, 'S', LIGHTGREY, "grey snake",
    M_COLD_BLOOD,
    600, 10, MONS_GREY_SNAKE, MH_NATURAL, -3,
    { 30, 0, 0, 0 },
    { 11, 3, 5, 0 },
    4, 16, 18, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_HISS, I_REPTILE, 
    MONUSE_NOTHING
}
,

{
    MONS_DEEP_TROLL, 'T', DARKGREY, "deep troll",
    M_WARM_BLOOD | M_SEE_INVIS,
    1500, 12, MONS_DEEP_TROLL, MH_NATURAL, -3,
    { 27, 20, 20, 0 },
    { 10, 3, 5, 0 },
    6, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_GIANT_BLOWFLY, 'y', LIGHTGREY, "giant blowfly",
    M_ED_POISON | M_FLIES,
    200, 10, MONS_GIANT_BLOWFLY, MH_NATURAL, -3,
    { 13, 0, 0, 0 },
    { 5, 3, 5, 0 },
    2, 15, 19, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_BUZZ, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_RED_WASP, 'y', RED, "red wasp",
    M_ED_POISON | M_FLIES,
    400, 14, MONS_RED_WASP, MH_NATURAL, -3,
    { 23, 0, 0, 0 },
    { 8, 3, 5, 0 },
    7, 14, 15, 7, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_BUZZ, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_SWAMP_DRAGON, 'D', BROWN, "swamp dragon",
    M_SPELLCASTER | M_FLIES | M_RES_POISON,
    1900, 11, MONS_SWAMP_DRAGON, MH_NATURAL, -3,
    { 13, 9, 9, 0 },
    { 9, 5, 5, 0 },
    7, 7, 10, 7, MST_SWAMP_DRAGON, CE_CONTAMINATED, Z_BIG, S_ROAR, I_ANIMAL_LIKE, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_SWAMP_DRAKE, 'd', BROWN, "swamp drake",
    M_SPELLCASTER | M_FLIES | M_RES_POISON,
    900, 11, MONS_SWAMP_DRAKE, MH_NATURAL, -3,
    { 11, 0, 0, 0 },
    { 4, 5, 5, 0 },
    3, 11, 11, 7, MST_SWAMP_DRAKE, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_ANIMAL_LIKE, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_SOLDIER_ANT, 'a', LIGHTGREY, "soldier ant",
    M_ED_POISON,
    900, 10, MONS_SOLDIER_ANT, MH_NATURAL, -3,
    { 14, 0, 0, 0 },
    { 6, 3, 5, 0 },
    8, 10, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,

{
    MONS_HILL_GIANT, 'C', LIGHTRED, "hill giant",
    M_WARM_BLOOD,
    1600, 10, MONS_HILL_GIANT, MH_NATURAL, -3,
    { 30, 0, 0, 0 },
    { 11, 3, 5, 0 },
    3, 4, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_QUEEN_ANT, 'Q', DARKGREY, "queen ant",
    M_ED_POISON,
    1200, 10, MONS_QUEEN_ANT, MH_NATURAL, -3,
    { 20, 0, 0, 0 },
    { 13, 3, 5, 0 },
    14, 3, 7, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,

{
    MONS_ANT_LARVA, 'w', LIGHTGREY, "ant larva",
    M_ED_POISON | M_NO_SKELETON,
    350, 5, MONS_ANT_LARVA, MH_NATURAL, -3,
    { 5, 0, 0, 0 },
    { 2, 3, 5, 0 },
    2, 6, 6, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,


{
    MONS_GIANT_FROG, 'F', GREEN, "giant frog",
    M_COLD_BLOOD | M_AMPHIBIOUS,
    500, 10, MONS_GIANT_FROG, MH_NATURAL, -3,
    { 9, 0, 0, 0 },
    { 4, 3, 5, 0 },
    0, 12, 15, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_CROAK, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_GIANT_BROWN_FROG, 'F', BROWN, "giant brown frog",
    M_COLD_BLOOD | M_AMPHIBIOUS,
    890, 10, MONS_GIANT_BROWN_FROG, MH_NATURAL, -3,
    { 14, 0, 0, 0 },
    { 8, 3, 5, 0 },
    2, 11, 13, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_CROAK, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_SPINY_FROG, 'F', YELLOW, "spiny frog",
    M_COLD_BLOOD | M_RES_POISON | M_AMPHIBIOUS,
    1000, 10, MONS_SPINY_FROG, MH_NATURAL, -3,
    { 26, 0, 0, 0 },
    { 7, 3, 5, 0 },
    6, 9, 12, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_CROAK, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_BLINK_FROG, 'F', LIGHTGREEN, "blink frog",
    M_COLD_BLOOD | M_AMPHIBIOUS,
    800, 12, MONS_BLINK_FROG, MH_NATURAL, -5,
    { 20, 0, 0, 0 },
    { 6, 3, 5, 0 },
    3, 12, 14, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_CROAK, I_ANIMAL, 
    MONUSE_NOTHING
}
,
{
    MONS_GIANT_COCKROACH, 'a', BROWN, "giant cockroach",
    M_NO_FLAGS,
    250, 10, MONS_GIANT_COCKROACH, MH_NATURAL, -1,
    { 2, 0, 0, 0 },
    { 1, 3, 4, 0 },
    3, 10, 12, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,
{
    MONS_SMALL_SNAKE, 'S', LIGHTGREEN, "small snake",
    M_COLD_BLOOD,
    100, 13, MONS_SMALL_SNAKE, MH_NATURAL, -1,
    { 2, 0, 0, 0 },
    { 1, 2, 3, 0 },
    0, 11, 12, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_REPTILE, 
    MONUSE_NOTHING
}
,

{
    MONS_WHITE_IMP, '5', WHITE, "white imp",
    M_RES_COLD | M_SPELLCASTER | M_FLIES | M_SPEAKS,
    0, 10, MONS_WHITE_IMP, MH_DEMONIC, -3,
    { 4, 0, 0, 0 },
    { 2, 3, 5, 0 },
    4, 10, 10, 7, MST_WHITE_IMP, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_LEMURE, '5', YELLOW, "lemure",
    M_RES_POISON,
    0, 10, MONS_LEMURE, MH_DEMONIC, -3,
    { 12, 0, 0, 0 },
    { 2, 3, 5, 0 },
    1, 12, 12, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_MOAN, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_UFETUBUS, '5', LIGHTCYAN, "ufetubus",
    M_ED_FIRE | M_RES_COLD,
    0, 10, MONS_UFETUBUS, MH_DEMONIC, -3,
    { 5, 5, 0, 0 },
    { 1, 4, 6, 0 },
    2, 15, 15, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_MANES, '5', LIGHTRED, "manes",
    M_RES_ELEC | M_RES_FIRE | M_RES_COLD | M_RES_POISON,
    0, 10, MONS_MANES, MH_DEMONIC, -3,
    { 5, 3, 3, 0 },
    { 3, 3, 5, 0 },
    2, 8, 8, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_MIDGE, '5', LIGHTGREEN, "midge",
    M_RES_POISON | M_FLIES,
    0, 10, MONS_MIDGE, MH_DEMONIC, -3,
    { 8, 0, 0, 0 },
    { 2, 3, 5, 0 },
    4, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_NEQOXEC, '3', MAGENTA, "neqoxec",
    M_RES_POISON | M_SPELLCASTER | M_LEVITATE,
    0, 12, MONS_NEQOXEC, MH_DEMONIC, -6,
    { 15, 0, 0, 0 },
    { 6, 3, 5, 0 },
    4, 12, 10, 7, MST_NEQOXEC, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_ORANGE_DEMON, '3', LIGHTRED, "orange demon",
    M_NO_FLAGS,
    0, 12, MONS_ORANGE_DEMON, MH_DEMONIC, -6,
    { 10, 5, 0, 0 },
    { 8, 4, 5, 0 },
    3, 7, 7, 7, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_SCREECH, I_NORMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_HELLWING, '3', LIGHTGREY, "hellwing",
    M_RES_POISON | M_SPELLCASTER | M_FLIES,
    0, 12, MONS_HELLWING, MH_DEMONIC, -6,
    { 17, 10, 0, 0 },
    { 7, 4, 5, 0 },
    8, 10, 10, 7, MST_HELLWING, CE_CONTAMINATED, Z_NOZOMBIE, S_MOAN, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_SMOKE_DEMON, '4', LIGHTGREY, "smoke demon",
    M_RES_POISON | M_RES_FIRE | M_SPELLCASTER | M_FLIES,
    0, 12, MONS_SMOKE_DEMON, MH_DEMONIC, -6,
    { 8, 5, 5, 0 },
    { 7, 3, 5, 0 },
    5, 9, 9, 7, MST_SMOKE_DEMON, CE_CONTAMINATED, Z_NOZOMBIE, S_ROAR, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_YNOXINUL, '3', CYAN, "ynoxinul",
    M_RES_ELEC | M_RES_POISON | M_RES_COLD | M_SPELLCASTER | M_FLIES | M_SEE_INVIS,
    0, 12, MONS_YNOXINUL, MH_DEMONIC, -6,
    { 12, 0, 0, 0 },
    { 6, 3, 5, 0 },
    3, 10, 10, 7, MST_YNOXINUL, CE_CONTAMINATED, Z_NOZOMBIE, S_BELLOW, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_EXECUTIONER, '1', LIGHTGREY, "Executioner",
    M_SPELLCASTER | M_RES_ELEC | M_RES_FIRE | M_RES_COLD | M_RES_POISON | M_SEE_INVIS,
    0, 14, MONS_EXECUTIONER, MH_DEMONIC, -9,
    { 30, 10, 10, 0 },
    { 12, 3, 5, 0 },
    10, 15, 20, 7, MST_HELL_KNIGHT_I, CE_CONTAMINATED, Z_NOZOMBIE, S_SCREAM, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_GREEN_DEATH, '1', GREEN, "Green Death",
    M_RES_POISON | M_SPELLCASTER | M_SEE_INVIS,
    0, 14, MONS_GREEN_DEATH, MH_DEMONIC, -9,
    { 32, 0, 0, 0 },
    { 13, 3, 5, 0 },
    5, 7, 12, 7, MST_GREEN_DEATH, CE_POISONOUS, Z_NOZOMBIE, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_BLUE_DEATH, '1', BLUE, "Blue Death",
    M_RES_POISON | M_SPELLCASTER | M_ED_FIRE | M_RES_COLD | M_RES_ELEC | M_FLIES | M_SEE_INVIS,
    0, 14, MONS_BLUE_DEATH, MH_DEMONIC, -9,
    { 20, 20, 0, 0 },
    { 12, 3, 5, 0 },
    10, 10, 12, 7, MST_BLUE_DEATH, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_BALRUG, '1', RED, "Balrug",
    M_RES_POISON | M_RES_HELLFIRE | M_ED_COLD | M_SPELLCASTER | M_FLIES | M_SEE_INVIS,
    0, 14, MONS_BALRUG, MH_DEMONIC, -9,
    { 25, 0, 0, 0 },
    { 14, 3, 5, 0 },
    5, 12, 12, 7, MST_BALRUG, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_CACODEMON, '1', YELLOW, "Cacodemon",
    M_RES_POISON | M_RES_ELEC | M_SPELLCASTER | M_LEVITATE | M_SEE_INVIS,
    0, 14, MONS_CACODEMON, MH_DEMONIC, -9,
    { 22, 0, 0, 0 },
    { 13, 3, 5, 0 },
    11, 10, 10, 7, MST_CACODEMON, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,


{
    MONS_DEMONIC_CRAWLER, '3', DARKGREY, "demonic crawler",
    M_RES_ELEC | M_RES_POISON | M_RES_COLD | M_RES_FIRE | M_SEE_INVIS,
    0, 12, MONS_DEMONIC_CRAWLER, MH_DEMONIC, -6,
    { 13, 13, 13, 13 },
    { 9, 3, 5, 0 },
    10, 6, 9, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SCREAM, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_SUN_DEMON, '2', YELLOW, "sun demon",
    M_RES_ELEC | M_RES_POISON | M_ED_COLD | M_RES_HELLFIRE | M_SEE_INVIS | M_LEVITATE,
    0, 14, MONS_SUN_DEMON, MH_DEMONIC, -6,
    { 30, 0, 0, 0 },
    { 10, 3, 5, 0 },
    10, 12, 12, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_SHADOW_IMP, '5', DARKGREY, "shadow imp",
    M_RES_COLD | M_SPELLCASTER | M_FLIES | M_RES_POISON | M_SPEAKS,
    0, 11, MONS_SHADOW_IMP, MH_DEMONIC, -3,
    { 6, 0, 0, 0 },
    { 2, 3, 5, 0 },
    3, 11, 10, 7, MST_SHADOW_IMP, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_SHADOW_DEMON, '3', DARKGREY, "shadow demon",
    M_RES_POISON | M_RES_COLD | M_SEE_INVIS | M_INVIS,
    0, 12, MONS_SHADOW_DEMON, MH_DEMONIC, -7,
    { 21, 0, 0, 0 },
    { 6, 3, 5, 0 },
    7, 12, 11, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_CROAK, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_LOROCYPROCA, '2', BLUE, "Lorocyproca",
    M_RES_POISON | M_RES_COLD | M_RES_FIRE | M_RES_ELEC | M_SEE_INVIS | M_INVIS,
    0, 12, MONS_LOROCYPROCA, MH_DEMONIC, -7,
    { 25, 25, 0, 0 },
    { 12, 3, 5, 0 },
    10, 12, 9, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_MOAN, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_SHADOW_WRAITH, 'W', BLUE, "shadow wraith",
    M_RES_POISON | M_LEVITATE | M_SEE_INVIS | M_INVIS,
    0, 15, MONS_SHADOW_WRAITH, MH_UNDEAD, -8,
    { 20, 0, 0, 0 },
    { 10, 3, 5, 0 },
    7, 7, 10, 10, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_MOAN, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_GIANT_AMOEBA, 'J', BLUE, "giant amoeba",
    M_RES_POISON | M_NO_SKELETON | M_SEE_INVIS | M_AMPHIBIOUS,
    1000, 10, MONS_GIANT_AMOEBA, MH_NATURAL, -3,
    { 25, 0, 0, 0 },
    { 12, 3, 5, 0 },
    0, 4, 10, 10, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_GIANT_SLUG, 'm', GREEN, "giant slug",
    M_NO_SKELETON | M_AMPHIBIOUS,
    700, 10, MONS_GIANT_SLUG, MH_NATURAL, -3,
    { 23, 0, 0, 0 },
    { 10, 5, 3, 0 },
    0, 2, 6, 10, MST_NO_SPELLS, CE_POISONOUS, Z_BIG, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,

{
    MONS_GIANT_SNAIL, 'm', LIGHTGREEN, "giant snail",
    M_NO_SKELETON | M_AMPHIBIOUS,
    900, 10, MONS_GIANT_SNAIL, MH_NATURAL, -3,
    { 18, 0, 0, 0 },
    { 14, 5, 3, 0 },
    7, 2, 4, 10, MST_NO_SPELLS, CE_POISONOUS, Z_BIG, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,

{
    MONS_SPATIAL_VORTEX, 'v', BLACK, "spatial vortex",
    M_RES_POISON | M_RES_FIRE | M_RES_COLD | M_RES_ELEC | M_LEVITATE | M_CONFUSED,
    0, 5, MONS_SPATIAL_VORTEX, MH_NONLIVING, 5000,
    { 50, 0, 0, 0 },
    { 6, 6, 6, 0 },
    0, 5, 15, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_PIT_FIEND, '1', BROWN, "Pit Fiend",
    M_RES_POISON | M_RES_HELLFIRE | M_RES_COLD | M_FLIES | M_SEE_INVIS | M_RES_ELEC,
    0, 18, MONS_PIT_FIEND, MH_DEMONIC, -12,
    { 28, 21, 21, 0 },
    { 19, 4, 5, 0 },
    17, 5, 8, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_BORING_BEETLE, 'B', BROWN, "boring beetle",
    M_ED_POISON,
    1300, 10, MONS_BORING_BEETLE, MH_NATURAL, -3,
    { 26, 0, 0, 0 },
    { 8, 3, 5, 0 },
    13, 4, 6, 7, MST_NO_SPELLS, CE_POISONOUS, Z_BIG, S_SILENT, I_INSECT, 
    MONUSE_NOTHING
}
,

{
    MONS_GARGOYLE, 'g', DARKGREY, "gargoyle",
    M_RES_POISON | M_RES_ELEC | M_FLIES,
    0, 12, MONS_GARGOYLE, MH_NONLIVING, -6,
    { 10, 6, 6, 0 },
    { 4, 3, 5, 0 },
    18, 6, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

// only appear in Dis castle
{
    MONS_METAL_GARGOYLE, 'g', CYAN, "metal gargoyle",
    M_RES_POISON | M_RES_ELEC | M_FLIES,
    0, 12, MONS_METAL_GARGOYLE, MH_NONLIVING, -6,
    { 19, 10, 10, 0 },
    { 8, 3, 5, 0 },
    20, 4, 7, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

// only appear in Gehenna castle & one minivault
{
    MONS_MOLTEN_GARGOYLE, 'g', RED, "molten gargoyle",
    M_RES_POISON | M_RES_ELEC | M_FLIES | M_RES_FIRE,
    0, 12, MONS_MOLTEN_GARGOYLE, MH_NONLIVING, -6,
    { 12, 8, 8, 0 },
    { 5, 3, 5, 0 },
    14, 7, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,


// 250 can't exist


{
    MONS_MNOLEG, '&', LIGHTGREEN, "Mnoleg",
    M_RES_ELEC | M_RES_POISON | M_RES_FIRE | M_SEE_INVIS | M_SPELLCASTER | M_SPEAKS,
    0, 25, MONS_MNOLEG, MH_DEMONIC, 5000,
    { 23, 23, 0, 0 },
    { 17, 0, 0, 199 },
    10, 13, 13, 7, MST_MNOLEG, CE_CONTAMINATED, Z_NOZOMBIE, S_BUZZ, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_LOM_LOBON, '&', LIGHTBLUE, "Lom Lobon",
    M_RES_POISON | M_RES_FIRE | M_RES_COLD | M_RES_ELEC | M_LEVITATE | M_SEE_INVIS | M_SPELLCASTER | M_SPEAKS,
    0, 25, MONS_LOM_LOBON, MH_DEMONIC, 5000,
    { 40, 0, 0, 0 },
    { 19, 0, 0, 223 },
    10, 7, 8, 7, MST_LOM_LOBON, CE_CONTAMINATED, Z_NOZOMBIE, S_SCREAM, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_CEREBOV, '&', RED, "Cerebov",
    M_RES_ELEC | M_RES_POISON | M_RES_HELLFIRE | M_SPELLCASTER | M_SEE_INVIS | M_SPEAKS,
    0, 25, MONS_CEREBOV, MH_DEMONIC, -6,
    { 50, 0, 0, 0 },
    { 21, 0, 0, 253 },
    15, 8, 10, 7, MST_CEREBOV, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_GLOORX_VLOQ, '&', DARKGREY, "Gloorx Vloq",
    M_RES_POISON | M_RES_COLD | M_RES_ELEC | M_LEVITATE | M_SEE_INVIS | M_SPELLCASTER | M_SPEAKS,
    0, 25, MONS_GLOORX_VLOQ, MH_DEMONIC, -14,
    { 20, 0, 0, 0 },
    { 16, 0, 0, 234 },
    10, 10, 10, 7, MST_GLOORX_VLOQ, CE_CONTAMINATED, Z_NOZOMBIE, S_MOAN, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

/* ******************************************************************
{MONS_MOLLUSC_LORD, 'U', GREEN, "The Mollusc Lord", M_RES_POISON,
   0, 25, 255, MH_DEMONIC, -3, {30,0,0,0},
   {16,0,0,100}, 10, 10, 10, 7, 93, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_HIGH, 1},
****************************************************************** */

{
    MONS_NAGA_MAGE, 'N', LIGHTRED, "naga mage",
    M_RES_POISON | M_SPELLCASTER | M_ACTUAL_SPELLS | M_SEE_INVIS | M_WARM_BLOOD,
    750, 13, MONS_NAGA, MH_NATURAL, -6,
    { 5, 0, 0, 0 },
    { 7, 3, 5, 0 },
    6, 10, 8, 7, MST_NAGA_MAGE, CE_POISONOUS, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_NAGA_WARRIOR, 'N', BLUE, "naga warrior",
    M_RES_POISON | M_SPELLCASTER | M_ACTUAL_SPELLS | M_SEE_INVIS | M_WARM_BLOOD,
    750, 12, MONS_NAGA, MH_NATURAL, -6,
    { 11, 0, 0, 0 },
    { 10, 5, 5, 0 },
    6, 10, 8, 7, MST_NAGA, CE_POISONOUS, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_ORC_WARLORD, 'o', RED, "orc warlord",
    M_WARM_BLOOD,
    600, 15, MONS_ORC, MH_NATURAL, -3,
    { 32, 0, 0, 0 },
    { 15, 4, 7, 0 },
    3, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_DEEP_ELF_SOLDIER, 'e', CYAN, "deep elf soldier",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD,
    450, 10, MONS_ELF, MH_NATURAL, -6,
    { 6, 0, 0, 0 },
    { 3, 3, 3, 0 },
    0, 12, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_DEEP_ELF_FIGHTER, 'e', LIGHTBLUE, "deep elf fighter",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD,
    450, 10, MONS_ELF, MH_NATURAL, -6,
    { 9, 0, 0, 0 },
    { 6, 3, 3, 0 },
    0, 13, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_DEEP_ELF_KNIGHT, 'e', BLUE, "deep elf knight",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD,
    450, 10, MONS_ELF, MH_NATURAL, -6,
    { 14, 0, 0, 0 },
    { 11, 3, 3, 0 },
    0, 15, 11, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_DEEP_ELF_MAGE, 'e', LIGHTRED, "deep elf mage",
    M_RES_ELEC | M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD,
    450, 10, MONS_ELF, MH_NATURAL, -6,
    { 5, 0, 0, 0 },
    { 4, 3, 3, 0 },
    0, 13, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_DEEP_ELF_SUMMONER, 'e', YELLOW, "deep elf summoner",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD,
    450, 10, MONS_ELF, MH_NATURAL, -6,
    { 5, 0, 0, 0 },
    { 6, 3, 3, 0 },
    0, 13, 10, 7, MST_DEEP_ELF_SUMMONER, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_DEEP_ELF_CONJURER, 'e', LIGHTGREEN, "deep elf conjurer",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_RES_ELEC | M_WARM_BLOOD,
    450, 10, MONS_ELF, MH_NATURAL, -6,
    { 5, 0, 0, 0 },
    { 6, 3, 3, 0 },
    0, 13, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_DEEP_ELF_PRIEST, 'e', LIGHTGREY, "deep elf priest",
    M_SPELLCASTER | M_PRIEST | M_WARM_BLOOD,
    450, 10, MONS_ELF, MH_NATURAL, -6,
    { 9, 0, 0, 0 },
    { 5, 3, 3, 0 },
    0, 13, 10, 7, MST_DEEP_ELF_PRIEST, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_DEEP_ELF_HIGH_PRIEST, 'e', DARKGREY, "deep elf high priest",
    M_SPELLCASTER | M_SPEAKS | M_PRIEST | M_WARM_BLOOD | M_SEE_INVIS,
    450, 10, MONS_ELF, MH_NATURAL, -6,
    { 14, 0, 0, 0 },
    { 11, 3, 3, 0 },
    3, 13, 10, 7, MST_DEEP_ELF_HIGH_PRIEST, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_DEEP_ELF_DEMONOLOGIST, 'e', MAGENTA, "deep elf demonologist",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD | M_SEE_INVIS,
    450, 10, MONS_ELF, MH_NATURAL, -6,
    { 12, 0, 0, 0 },
    { 12, 3, 3, 0 },
    0, 13, 10, 7, MST_DEEP_ELF_DEMONOLOGIST, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_DEEP_ELF_ANNIHILATOR, 'e', GREEN, "deep elf annihilator",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD | M_RES_ELEC | M_SEE_INVIS,
    450, 10, MONS_ELF, MH_NATURAL, -6,
    { 12, 0, 0, 0 },
    { 15, 3, 3, 0 },
    0, 13, 10, 7, MST_DEEP_ELF_ANNIHILATOR, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_DEEP_ELF_SORCERER, 'e', RED, "deep elf sorcerer",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD | M_SEE_INVIS | M_SPEAKS,
    450, 10, MONS_ELF, MH_NATURAL, -6,
    { 12, 0, 0, 0 },
    { 14, 3, 3, 0 },
    0, 13, 10, 7, MST_DEEP_ELF_SORCERER, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_DEEP_ELF_DEATH_MAGE, 'e', WHITE, "deep elf death mage",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD | M_SEE_INVIS,
    450, 10, MONS_ELF, MH_NATURAL, -6,
    { 12, 0, 0, 0 },
    { 15, 3, 3, 0 },
    0, 13, 10, 7, MST_DEEP_ELF_DEATH_MAGE, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_BROWN_OOZE, 'J', BROWN, "brown ooze",
    M_RES_POISON | M_NO_SKELETON | M_SEE_INVIS,
    0, 11, MONS_BROWN_OOZE, MH_NATURAL, -7,
    { 25, 0, 0, 0 },
    { 7, 3, 5, 0 },
    10, 1, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_EATS_ITEMS
}
,

{
    MONS_AZURE_JELLY, 'J', LIGHTBLUE, "azure jelly",
    M_RES_POISON | M_RES_COLD | M_ED_FIRE | M_RES_ELEC | M_NO_SKELETON | M_SEE_INVIS,
    0, 11, MONS_AZURE_JELLY, MH_NATURAL, -4,
    { 12, 12, 12, 12 },
    { 15, 3, 5, 0 },
    5, 10, 12, 7, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_EATS_ITEMS
}
,

{
    MONS_DEATH_OOZE, 'J', DARKGREY, "death ooze",
    M_RES_POISON | M_RES_COLD | M_NO_SKELETON | M_SEE_INVIS,
    0, 13, MONS_DEATH_OOZE, MH_UNDEAD, -8,
    { 32, 32, 0, 0 },
    { 11, 3, 3, 0 },
    2, 4, 12, 7, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_EATS_ITEMS
}
,

{
    MONS_ACID_BLOB, 'J', LIGHTGREEN, "acid blob",
    M_RES_POISON | M_NO_SKELETON | M_SEE_INVIS,
    0, 12, MONS_ACID_BLOB, MH_NATURAL, -7,
    { 42, 0, 0, 0 },
    { 18, 3, 5, 0 },
    1, 3, 14, 7, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_EATS_ITEMS
}
,

{
    MONS_ROYAL_JELLY, 'J', YELLOW, "royal jelly",
    M_RES_POISON | M_NO_SKELETON | M_SEE_INVIS,
    0, 20, MONS_ROYAL_JELLY, MH_NATURAL, -7,
    { 50, 0, 0, 0 },
    { 21, 0, 0, 111 },
    8, 4, 12, 7, MST_NO_SPELLS, CE_CLEAN, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_EATS_ITEMS
}
,

{
    MONS_TERENCE, '@', LIGHTCYAN, "Terence",
    M_WARM_BLOOD| M_SPEAKS,
    0, 20, MONS_HUMAN, MH_NATURAL, -3,
    { 3, 0, 0, 0 },
    { 1, 0, 0, 14 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_JESSICA, '@', LIGHTGREY, "Jessica",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD,
    0, 20, MONS_HUMAN, MH_NATURAL, -3,
    { 4, 0, 0, 0 },
    { 1, 0, 0, 10 },
    0, 10, 10, 7, MST_ORC_WIZARD_I, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_IJYB, 'g', BLUE, "Ijyb",
    M_WARM_BLOOD | M_SPEAKS,
    0, 5, MONS_GOBLIN, MH_NATURAL, -3,
    { 4, 0, 0, 0 },
    { 3, 0, 0, 28 },
    2, 12, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_SIGMUND, '@', YELLOW, "Sigmund",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD,
    0, 20, MONS_HUMAN, MH_NATURAL, -3,
    { 5, 0, 0, 0 },
    { 3, 0, 0, 25 },
    0, 11, 10, 7, MST_ORC_WIZARD_II, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_BLORK_THE_ORC, 'o', BROWN, "Blork the orc",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD,
    0, 20, MONS_ORC, MH_NATURAL, -4,
    { 7, 0, 0, 0 },
    { 3, 0, 0, 32 },
    0, 9, 8, 7, MST_ORC_WIZARD_III, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_EDMUND, '@', RED, "Edmund",
    M_WARM_BLOOD | M_SPEAKS,
    0, 20, MONS_HUMAN, MH_NATURAL, -4,
    { 6, 0, 0, 0 },
    { 4, 0, 0, 27 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_PSYCHE, '@', LIGHTMAGENTA, "Psyche",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD,
    0, 20, MONS_HUMAN, MH_NATURAL, -4,
    { 7, 0, 0, 0 },
    { 5, 0, 0, 24 },
    0, 12, 13, 7, MST_ORC_WIZARD_III, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_EROLCHA, 'O', LIGHTBLUE, "Erolcha",
    M_RES_ELEC | M_SPELLCASTER | M_ACTUAL_SPELLS | M_SEE_INVIS | M_WARM_BLOOD | M_SPEAKS,
    0, 20, MONS_OGRE, MH_NATURAL, -7,
    { 20, 0, 0, 0 },
    { 6, 0, 0, 45 },
    3, 7, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_DONALD, '@', BLUE, "Donald",
    M_WARM_BLOOD | M_SPEAKS,
    0, 20, MONS_HUMAN, MH_NATURAL, -5,
    { 8, 0, 0, 0 },
    { 5, 0, 0, 33 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_URUG, 'o', RED, "Urug",
    M_WARM_BLOOD | M_SPEAKS,
    0, 20, MONS_ORC, MH_NATURAL, -5,
    { 12, 0, 0, 0 },
    { 6, 0, 0, 38 },
    0, 11, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_MICHAEL, '@', LIGHTGREY, "Michael",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD,
    0, 20, MONS_HUMAN, MH_NATURAL, -5,
    { 9, 0, 0, 0 },
    { 6, 0, 0, 36 },
    0, 10, 10, 7, MST_ORC_WIZARD_III, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_JOSEPH, '@', CYAN, "Joseph",
    M_WARM_BLOOD | M_SPEAKS,
    0, 20, MONS_HUMAN, MH_NATURAL, -5,
    { 9, 0, 0, 0 },
    { 7, 0, 0, 42 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_SNORG, 'T', GREEN, "Snorg",
    M_WARM_BLOOD | M_SPEAKS,
    0, 20, MONS_TROLL, MH_NATURAL, -6,
    { 20, 15, 15, 0 },
    { 8, 0, 0, 45 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_ERICA, '@', MAGENTA, "Erica",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD,
    0, 20, MONS_HUMAN, MH_NATURAL, -5,
    { 10, 0, 0, 0 },
    { 9, 0, 0, 43 },
    0, 11, 11, 7, MST_WIZARD_II, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_JOSEPHINE, '@', WHITE, "Josephine",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD,
    0, 20, MONS_HUMAN, MH_NATURAL, -5,
    { 11, 0, 0, 0 },
    { 9, 0, 0, 47 },
    0, 10, 10, 7, MST_NECROMANCER_I, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_HAROLD, '@', LIGHTGREEN, "Harold",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD,
    0, 20, MONS_HUMAN, MH_NATURAL, -5,
    { 12, 0, 0, 0 },
    { 9, 0, 0, 51 },
    0, 8, 10, 7, MST_HELL_KNIGHT_II, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_NORBERT, '@', BROWN, "Norbert",
    M_WARM_BLOOD | M_SPEAKS,
    0, 20, MONS_HUMAN, MH_NATURAL, -5,
    { 14, 0, 0, 0 },
    { 10, 0, 0, 53 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_JOZEF, '@', LIGHTMAGENTA, "Jozef",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD,
    0, 20, MONS_HUMAN, MH_NATURAL, -5,
    { 14, 0, 0, 0 },
    { 11, 0, 0, 60 },
    0, 9, 10, 7, MST_GUARDIAN_NAGA, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_AGNES, '@', LIGHTBLUE, "Agnes",
    M_WARM_BLOOD | M_SPEAKS,
    0, 20, MONS_HUMAN, MH_NATURAL, -5,
    { 11, 0, 0, 0 },
    { 11, 0, 0, 64 },
    0, 10, 15, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_MAUD, '@', RED, "Maud",
    M_WARM_BLOOD | M_SPEAKS,
    0, 20, MONS_HUMAN, MH_NATURAL, -5,
    { 14, 0, 0, 0 },
    { 13, 0, 0, 55 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_LOUISE, '@', BLUE, "Louise",
    M_SPELLCASTER | M_RES_ELEC | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD,
    0, 20, MONS_HUMAN, MH_NATURAL, -5,
    { 12, 0, 0, 0 },
    { 13, 0, 0, 52 },
    0, 10, 10, 7, MST_WIZARD_IV, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_FRANCIS, '@', YELLOW, "Francis",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD | M_SEE_INVIS,
    0, 20, MONS_HUMAN, MH_NATURAL, -5,
    { 12, 0, 0, 0 },
    { 14, 0, 0, 67 },
    0, 10, 10, 7, MST_ORC_HIGH_PRIEST, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_FRANCES, '@', YELLOW, "Frances",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD | M_SEE_INVIS,
    0, 20, MONS_HUMAN, MH_NATURAL, -5,
    { 11, 0, 0, 0 },
    { 14, 0, 0, 70 },
    0, 10, 10, 7, MST_ORC_HIGH_PRIEST, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_RUPERT, '@', RED, "Rupert",
    M_SPELLCASTER | M_RES_ELEC | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD | M_SEE_INVIS,
    0, 20, MONS_HUMAN, MH_NATURAL, -5,
    { 13, 0, 0, 0 },
    { 16, 0, 0, 80 },
    0, 10, 10, 7, MST_WIZARD_IV, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_WAYNE, '@', YELLOW, "Wayne",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD | M_SEE_INVIS,
    0, 20, MONS_HUMAN, MH_NATURAL, -5,
    { 14, 0, 0, 0 },
    { 17, 0, 0, 78 },
    1, 10, 7, 7, MST_ORC_PRIEST, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_DUANE, '@', YELLOW, "Duane",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD | M_SEE_INVIS,
    0, 20, MONS_HUMAN, MH_NATURAL, -5,
    { 14, 0, 0, 0 },
    { 18, 0, 0, 83 },
    0, 10, 10, 7, MST_ORC_WIZARD_I, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_XTAHUA, 'D', RED, "Xtahua",
    M_SPEAKS | M_SEE_INVIS | M_RES_POISON | M_RES_FIRE | M_ED_COLD | M_FLIES,
    0, 20, MONS_DRAGON, MH_NATURAL, -7,
    { 29, 17, 17, 0 },
    { 19, 0, 0, 133 },
    15, 7, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_NORRIS, '@', LIGHTRED, "Norris",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD | M_SEE_INVIS,
    0, 20, MONS_HUMAN, MH_NATURAL, -5,
    { 16, 0, 0, 0 },
    { 20, 0, 0, 95 },
    1, 9, 9, 7, MST_HELL_KNIGHT_II, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_ADOLF, '@', DARKGREY, "Adolf",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD | M_SEE_INVIS,
    0, 20, MONS_HUMAN, MH_NATURAL, -5,
    { 17, 0, 0, 0 },
    { 21, 0, 0, 105 },
    0, 10, 10, 7, MST_LICH_IV, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_MARGERY, '@', RED, "Margery",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD | M_SEE_INVIS,
    0, 20, MONS_HUMAN, MH_NATURAL, -5,
    { 18, 0, 0, 0 },
    { 22, 0, 0, 119 },
    0, 10, 10, 7, MST_EFREET, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_BORIS, 'L', RED, "Boris",
    M_RES_POISON | M_RES_COLD | M_RES_ELEC | M_SPELLCASTER | M_ACTUAL_SPELLS | M_SEE_INVIS | M_SPEAKS,
    0, 23, MONS_LICH, MH_UNDEAD, -11,
    { 15, 0, 0, 0 },
    { 22, 0, 0, 99 },
    12, 10, 10, 7, MST_LICH_IV, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_STARTING_EQUIPMENT
}
,


{
    MONS_GERYON, '&', GREEN, "Geryon",
    M_SPELLCASTER | M_SEE_INVIS | M_SPEAKS,
    0, 25, MONS_GERYON, MH_DEMONIC, -6,
    { 30, 0, 0, 0 },
    { 15, 0, 0, 240 },
    15, 6, 10, 7, MST_GERYON, CE_CONTAMINATED, Z_NOZOMBIE, S_ROAR, I_NORMAL, 
    MONUSE_STARTING_EQUIPMENT
}
,

{
    MONS_DISPATER, '&', MAGENTA, "Dispater",
    M_RES_ELEC | M_RES_POISON | M_RES_HELLFIRE | M_RES_COLD | M_SPELLCASTER | M_SEE_INVIS | M_SPEAKS,
    0, 25, MONS_DISPATER, MH_DEMONIC, -10,
    { 15, 0, 0, 0 },
    { 16, 0, 0, 222 },
    15, 3, 6, 7, MST_DISPATER, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_ASMODEUS, '&', LIGHTMAGENTA, "Asmodeus",
    M_RES_ELEC | M_RES_POISON | M_RES_HELLFIRE | M_SPELLCASTER | M_FLIES | M_SEE_INVIS | M_SPEAKS,
    0, 25, MONS_ASMODEUS, MH_DEMONIC, -12,
    { 20, 0, 0, 0 },
    { 17, 0, 0, 245 },
    12, 7, 9, 7, MST_ASMODEUS, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

// Antaeus is now demonic so that he'll resist torment. -- bwr
{
    MONS_ANTAEUS, 'C', LIGHTCYAN, "Antaeus",
    M_RES_ELEC | M_ED_FIRE | M_RES_COLD | M_SPELLCASTER | M_SPEAKS,
    0, 25, MONS_ANTAEUS, MH_DEMONIC, -9,
    { 30, 0, 0, 0 },
    { 22, 0, 0, 250 },
    10, 4, 7, 7, MST_ANTAEUS, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_ERESHKIGAL, '&', WHITE, "Ereshkigal",
    M_RES_ELEC | M_RES_POISON | M_RES_COLD | M_SPELLCASTER | M_SEE_INVIS | M_SPEAKS,
    0, 25, MONS_ERESHKIGAL, MH_DEMONIC, -10,
    { 20, 0, 0, 0 },
    { 18, 0, 0, 238 },
    15, 6, 9, 7, MST_ERESHKIGAL, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_ANCIENT_LICH, 'L', DARKGREY, "ancient lich",
    M_RES_POISON | M_RES_COLD | M_SPELLCASTER | M_ACTUAL_SPELLS | M_SEE_INVIS | M_RES_FIRE | M_RES_ELEC,
    0, 20, MONS_LICH, MH_UNDEAD, -14,
    { 20, 0, 0, 0 },
    { 27, 2, 4, 0 },
    20, 10, 12, 7, MST_LICH_I, CE_NOCORPSE, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,
/* number is set in define_monster */

{
    MONS_OOZE, 'J', LIGHTGREY, "ooze",
    M_RES_POISON | M_NO_SKELETON | M_SEE_INVIS,
    0, 5, MONS_OOZE, MH_NATURAL, -6,
    { 5, 0, 0, 0 },
    { 3, 3, 5, 0 },
    1, 3, 8, 7, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_VAULT_GUARD, '@', CYAN, "vault guard",
    M_WARM_BLOOD | M_SEE_INVIS,
    0, 12, MONS_HUMAN, MH_NATURAL, -3,
    { 20, 0, 0, 0 },
    { 13, 3, 5, 0 },
    1, 13, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

/* These nasties are never randomly generated, only sometimes specially
   placed in the Crypt. */
{
    MONS_CURSE_SKULL, 'z', DARKGREY, "curse skull",
    M_RES_ELEC | M_RES_POISON | M_RES_HELLFIRE | M_RES_COLD | M_LEVITATE | M_SPELLCASTER | M_SEE_INVIS,
    0, 50, MONS_CURSE_SKULL, MH_UNDEAD, 5000,
    { 0, 0, 0, 0 },
    { 13, 0, 0, 66 },
    40, 3, 10, 7, MST_CURSE_SKULL, CE_NOCORPSE, Z_NOZOMBIE, S_MOAN, I_HIGH, 
    MONUSE_NOTHING
}
,

{
    MONS_VAMPIRE_KNIGHT, 'V', CYAN, "vampire knight",
    M_RES_POISON | M_RES_COLD | M_SPELLCASTER | M_SEE_INVIS,
    0, 13, MONS_VAMPIRE, MH_UNDEAD, -6,
    { 33, 0, 0, 0 },
    { 11, 3, 7, 0 },
    10, 10, 10, 7, MST_VAMPIRE_KNIGHT, CE_NOCORPSE, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_VAMPIRE_MAGE, 'V', MAGENTA, "vampire mage",
    M_RES_POISON | M_RES_COLD | M_SPELLCASTER | M_SEE_INVIS | M_FLIES,
    0, 15, MONS_VAMPIRE, MH_UNDEAD, -6,
    { 22, 0, 0, 0 },
    { 8, 3, 4, 0 },
    10, 10, 10, 7, MST_VAMPIRE_MAGE, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_SHINING_EYE, 'G', LIGHTMAGENTA, "shining eye",
    M_NO_SKELETON | M_LEVITATE | M_SPELLCASTER | M_SEE_INVIS,
    0, 14, MONS_SHINING_EYE, MH_NATURAL, 5000,
    { 0, 0, 0, 0 },
    { 10, 3, 5, 0 },
    3, 1, 7, 7, MST_SHINING_EYE, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_ORB_GUARDIAN, 'X', MAGENTA, "Orb Guardian",
    M_NO_SKELETON | M_SEE_INVIS,
    0, 20, MONS_ORB_GUARDIAN, MH_NATURAL, -6,
    { 40, 0, 0, 0 },
    { 15, 3, 5, 0 },
    13, 13, 14, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_DAEVA, 'A', YELLOW, "Daeva",
    M_RES_POISON | M_LEVITATE | M_SPELLCASTER,
    0, 12, MONS_DAEVA, MH_HOLY, -8,
    { 25, 0, 0, 0 },
    { 12, 3, 5, 0 },
    10, 13, 13, 7, MST_DAEVA, CE_NOCORPSE, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

/* spectral thing - similar to zombies/skeletons */
{
    MONS_SPECTRAL_THING, 'W', GREEN, "",
    M_RES_POISON | M_RES_COLD | M_LEVITATE | M_SEE_INVIS,
    0, 11, MONS_SPECTRAL_THING, MH_UNDEAD, 5000,
    { 20, 0, 0, 0 },
    { 8, 3, 5, 0 },
    8, 5, 7, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_GREATER_NAGA, 'N', RED, "greater naga",
    M_RES_POISON | M_SPELLCASTER | M_ACTUAL_SPELLS | M_SEE_INVIS | M_WARM_BLOOD,
    750, 10, MONS_NAGA, MH_NATURAL, 5000,
    { 18, 0, 0, 0 },
    { 15, 3, 5, 0 },
    6, 10, 8, 7, MST_NAGA_MAGE, CE_POISONOUS, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_SKELETAL_DRAGON, 'D', LIGHTGREY, "skeletal dragon",
    M_RES_POISON | M_RES_FIRE | M_RES_COLD | M_RES_ELEC | M_SEE_INVIS,
    0, 12, MONS_SKELETAL_DRAGON, MH_UNDEAD, -4,
    { 30, 20, 20, 0 },
    { 20, 8, 8, 0 },
    20, 4, 8, 7, MST_NO_SPELLS, CE_CLEAN, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_TENTACLED_MONSTROSITY, 'X', GREEN, "tentacled monstrosity",
    M_RES_POISON | M_RES_FIRE | M_RES_COLD | M_RES_ELEC | M_SEE_INVIS | M_AMPHIBIOUS,
    0, 10, MONS_TENTACLED_MONSTROSITY, MH_NATURAL, -5,
    { 22, 17, 13, 19 },
    { 25, 3, 5, 0 },
    5, 5, 9, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_SPHINX, 'H', LIGHTGREY, "sphinx",
    M_FLIES | M_SEE_INVIS | M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD,
    0, 10, MONS_SPHINX, MH_NATURAL, -3,
    { 25, 12, 12, 0 },
    { 16, 3, 5, 0 },
    5, 5, 13, 7, MST_SPHINX, CE_CLEAN, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_ROTTING_HULK, 'n', BROWN, "rotting hulk",
    M_RES_POISON | M_RES_COLD,
    0, 12, MONS_ROTTING_HULK, MH_UNDEAD, -5,
    { 25, 0, 0, 0 },
    { 10, 3, 5, 0 },
    5, 7, 8, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_GUARDIAN_MUMMY, 'M', YELLOW, "guardian mummy",
    M_RES_POISON | M_RES_COLD | M_SEE_INVIS,
    0, 13, MONS_GUARDIAN_MUMMY, MH_UNDEAD, -5,
    { 30, 0, 0, 0 },
    { 7, 5, 3, 0 },
    6, 9, 9, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_GREATER_MUMMY, 'M', DARKGREY, "greater mummy",
    M_RES_POISON | M_RES_COLD | M_RES_ELEC | M_SPELLCASTER | M_ACTUAL_SPELLS | M_SEE_INVIS,
    0, 20, MONS_MUMMY, MH_UNDEAD, 5000,
    { 35, 0, 0, 0 },
    { 15, 5, 3, 100 },
    10, 6, 10, 7, MST_MUMMY, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_MUMMY_PRIEST, 'M', RED, "mummy priest",
    M_RES_POISON | M_RES_COLD | M_RES_ELEC | M_SPELLCASTER | M_PRIEST | M_SEE_INVIS,
    0, 16, MONS_MUMMY, MH_UNDEAD, 5000,
    { 30, 0, 0, 0 },
    { 10, 5, 3, 0 },
    8, 7, 9, 7, MST_MUMMY, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_CENTAUR_WARRIOR, 'c', YELLOW, "centaur warrior",
    M_WARM_BLOOD,
    1500, 12, MONS_CENTAUR, MH_NATURAL, -3,
    { 16, 0, 0, 0 },
    { 9, 3, 5, 0 },
    4, 8, 15, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_YAKTAUR_CAPTAIN, 'c', RED, "yaktaur captain",
    M_WARM_BLOOD,
    2000, 10, MONS_YAKTAUR, MH_NATURAL, -3,
    { 23, 0, 0, 0 },
    { 14, 3, 5, 0 },
    5, 5, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_KILLER_KLOWN, '@', BLACK, "Killer Klown",
    M_RES_FIRE | M_RES_COLD | M_RES_POISON | M_SEE_INVIS | M_SPEAKS | M_WARM_BLOOD,
    0, 15, MONS_KILLER_KLOWN, MH_NATURAL, 5000,
    { 30, 0, 0, 0 },
    { 20, 5, 5, 0 },
    10, 15, 15, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_ELECTRIC_GOLEM, '8', LIGHTCYAN, "electric golem",
    M_SPELLCASTER | M_RES_ELEC | M_RES_POISON | M_RES_FIRE | M_RES_COLD | M_SEE_INVIS,
    0, 10, MONS_ELECTRIC_GOLEM, MH_NONLIVING, 5000,
    { 12, 12, 12, 12 },
    { 15, 7, 4, 0 },
    5, 20, 20, 7, MST_ELECTRIC_GOLEM, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_BALL_LIGHTNING, '*', LIGHTCYAN, "ball lightning",
    M_FLIES | M_RES_ELEC | M_CONFUSED | M_SPELLCASTER,
    0, 20, MONS_BALL_LIGHTNING, MH_NONLIVING, 5000,
    { 5, 0, 0, 0 },
    { 12, 0, 0, 1 },
    0, 10, 20, 7, MST_STORM_DRAGON, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_PLANT, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_ORB_OF_FIRE, '*', RED, "orb of fire",
    M_SPELLCASTER | M_FLIES | M_RES_ELEC | M_RES_FIRE | M_RES_COLD | M_RES_POISON | M_SEE_INVIS,
    0, 10, MONS_ORB_OF_FIRE, MH_NONLIVING, 5000,
    { 0, 0, 0, 0 },
    { 30, 0, 0, 150 },
    20, 20, 20, 7, MST_ORB_OF_FIRE, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_QUOKKA, 'r', LIGHTGREY, "quokka",
    M_WARM_BLOOD,
    300, 10, MONS_QUOKKA, MH_NATURAL, -1,
    { 5, 0, 0, 0 },
    { 1, 3, 5, 0 },
    2, 13, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_NORMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_EYE_OF_DEVASTATION, 'G', YELLOW, "eye of devastation",
    M_NO_SKELETON | M_LEVITATE | M_SPELLCASTER | M_SEE_INVIS,
    0, 11, MONS_EYE_OF_DEVASTATION, MH_NATURAL, 5000,
    { 0, 0, 0, 0 },
    { 10, 3, 5, 0 },
    12, 1, 7, 7, MST_EYE_OF_DEVASTATION, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_MOTH_OF_WRATH, 'y', BROWN, "moth of wrath",
    M_FLIES,
    0, 10, MONS_MOTH_OF_WRATH, MH_NATURAL, -3,
    { 25, 0, 0, 0 },
    { 9, 3, 5, 0 },
    0, 10, 12, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_DEATH_COB, '%', YELLOW, "death cob",
    M_RES_POISON | M_RES_COLD | M_SPEAKS,
    0, 10, MONS_DEATH_COB, MH_UNDEAD, -3,
    { 20, 0, 0, 0 },
    { 10, 4, 5, 0 },
    10, 15, 25, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_MOAN, I_NORMAL, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_CURSE_TOE, 'z', DARKGREY, "curse toe", 
    M_RES_ELEC | M_RES_POISON | M_RES_HELLFIRE | M_RES_COLD | M_LEVITATE | M_SPELLCASTER | M_SEE_INVIS,
    0, 60, MONS_CURSE_TOE, MH_UNDEAD, 5000,
    { 0, 0, 0, 0 },
    { 14, 0, 0, 77 },
    50, 1, 12, 7, MST_CURSE_SKULL, CE_NOCORPSE, Z_NOZOMBIE, S_MOAN, I_HIGH, 
    MONUSE_NOTHING
}
,

{
    // gold mimics are the only mimics that actually use their name -- bwr
    MONS_GOLD_MIMIC, '$', YELLOW, "pile of gold coins",
    M_NO_SKELETON | M_RES_POISON | M_RES_ELEC | M_RES_FIRE | M_RES_COLD,
    0, 13, MONS_GOLD_MIMIC, MH_NONLIVING, -3,
    { 12, 12, 12, 0 },
    { 8, 3, 5, 0 },
    5, 1, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_WEAPON_MIMIC, ')', BLACK, "mimic",
    M_NO_SKELETON | M_RES_POISON | M_RES_ELEC | M_RES_FIRE | M_RES_COLD,
    0, 13, MONS_GOLD_MIMIC, MH_NONLIVING, -3,
    { 17, 17, 17, 0 },
    { 8, 3, 5, 0 },
    5, 1, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_ARMOUR_MIMIC, '[', BLACK, "mimic",
    M_NO_SKELETON | M_RES_POISON | M_RES_ELEC | M_RES_FIRE | M_RES_COLD,
    0, 13, MONS_GOLD_MIMIC, MH_NONLIVING, -3,
    { 12, 12, 12, 0 },
    { 8, 3, 5, 0 },
    15, 1, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_SCROLL_MIMIC, '?', LIGHTGREY, "mimic",
    M_NO_SKELETON | M_RES_POISON | M_RES_ELEC | M_RES_FIRE | M_RES_COLD,
    0, 13, MONS_GOLD_MIMIC, MH_NONLIVING, -3,
    { 12, 12, 12, 0 },
    { 8, 3, 5, 0 },
    5, 1, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_POTION_MIMIC, '!', BLACK, "mimic",
    M_NO_SKELETON | M_RES_POISON | M_RES_ELEC | M_RES_FIRE | M_RES_COLD,
    0, 13, MONS_GOLD_MIMIC, MH_NONLIVING, -3,
    { 12, 12, 12, 0 },
    { 8, 3, 5, 0 },
    5, 1, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_HELL_HOG, 'h', RED, "hell-hog",
    M_SPELLCASTER,
    0, 10, MONS_HELL_HOG, MH_DEMONIC, -3,
    { 20, 0, 0, 0 },
    { 11, 3, 5, 0 },
    2, 9, 14, 7, MST_HELL_HOG, CE_CLEAN, Z_NOZOMBIE, S_SILENT, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_SERPENT_OF_HELL, 'D', RED, "Serpent of Hell",
    M_SPELLCASTER | M_RES_POISON | M_RES_HELLFIRE | M_FLIES | M_SEE_INVIS,
    0, 18, MONS_SERPENT_OF_HELL, MH_DEMONIC, -13,
    { 35, 15, 15, 0 },
    { 20, 4, 4, 0 },
    12, 9, 14, 7, MST_SERPENT_OF_HELL, CE_CLEAN, Z_NOZOMBIE, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_BOGGART, 'g', DARKGREY, "boggart",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SEE_INVIS,
    0, 14, MONS_BOGGART, MH_NATURAL, -7,
    { 5, 0, 0, 0 },
    { 2, 3, 5, 0 },
    0, 12, 12, 7, MST_BOGGART, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,

{
    MONS_QUICKSILVER_DRAGON, 'D', LIGHTCYAN, "quicksilver dragon",
    M_SPELLCASTER | M_FLIES | M_SEE_INVIS,
    0, 14, MONS_QUICKSILVER_DRAGON, MH_NATURAL, -7,
    { 45, 0, 0, 0 },
    { 16, 3, 5, 0 },
    10, 15, 15, 7, MST_QUICKSILVER_DRAGON, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_IRON_DRAGON, 'D', CYAN, "iron dragon",
    M_SPELLCASTER | M_RES_POISON | M_RES_FIRE | M_RES_COLD | M_SEE_INVIS,
    0, 14, MONS_IRON_DRAGON, MH_NATURAL, -7,
    { 25, 25, 25, 0 },
    { 18, 5, 3, 0 },
    20, 6, 8, 7, MST_IRON_DRAGON, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_SKELETAL_WARRIOR, 'z', CYAN, "skeletal warrior",
    M_SPELLCASTER | M_RES_POISON | M_RES_COLD | M_ACTUAL_SPELLS,
    0, 10, MONS_SKELETAL_WARRIOR, MH_UNDEAD, -7,
    { 25, 0, 0, 0 },
    { 10, 5, 3, 0 },
    15, 10, 10, 7, MST_SKELETAL_WARRIOR, CE_CONTAMINATED, Z_SMALL, S_SILENT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR
}
,


/* player ghost - only one per level. stats are stored in ghost struct */
{
    MONS_PLAYER_GHOST, 'p', DARKGREY, "",
    M_RES_POISON | M_SPEAKS | M_SPELLCASTER | M_ACTUAL_SPELLS | M_FLIES,
    0, 15, MONS_PLAYER_GHOST, MH_UNDEAD, -5,
    { 5, 0, 0, 0 },
    { 4, 2, 3, 0 },
    1, 2, 10, 7, MST_GHOST, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

/* random demon in pan - only one per level. stats are stored in ghost struct */
{
    MONS_PANDEMONIUM_DEMON, '&', BLACK, "&",
    M_SPELLCASTER | M_RES_POISON | M_SPEAKS,
    0, 14, MONS_PANDEMONIUM_DEMON, MH_DEMONIC, -5,
    { 5, 0, 0, 0 },
    { 4, 2, 3, 0 },
    1, 2, 10, 7, MST_GHOST, CE_CONTAMINATED, Z_NOZOMBIE, S_RANDOM, I_HIGH, 
    MONUSE_OPEN_DOORS
}
,

// begin lava monsters {dlb}
{
    MONS_LAVA_WORM, 'w', RED, "lava worm",
    M_RES_FIRE | M_ED_COLD,
    0, 10, MONS_LAVA_WORM, MH_NATURAL, -3,
    { 15, 0, 0, 0 },
    { 6, 3, 5, 0 },
    1, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_SMALL, S_SILENT, I_ANIMAL_LIKE, 
    MONUSE_NOTHING
}
,

{
    MONS_LAVA_FISH, ';', RED, "lava fish",
    M_RES_FIRE | M_ED_COLD,
    0, 10, MONS_LAVA_FISH, MH_NATURAL, -3,
    { 10, 0, 0, 0 },
    { 4, 3, 5, 0 },
    4, 15, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_SMALL, S_SILENT, I_ANIMAL_LIKE, 
    MONUSE_NOTHING
}
,

{
    MONS_LAVA_SNAKE, 'S', RED, "lava snake",
    M_RES_FIRE | M_ED_COLD,
    0, 10, MONS_LAVA_SNAKE, MH_NATURAL, -3,
    { 7, 0, 0, 0 },
    { 3, 3, 5, 0 },
    2, 17, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_SMALL, S_HISS, I_ANIMAL_LIKE, 
    MONUSE_NOTHING
}
,

{  // mv: was another lava thing
    MONS_SALAMANDER, 'S', LIGHTRED, "salamander",
    M_RES_FIRE | M_ED_COLD | M_WARM_BLOOD,
    0, 10, MONS_SALAMANDER, MH_NATURAL, -3,
    { 23, 0, 0, 0 },
    { 14, 3, 5, 0 },
    5, 5, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_SMALL, S_SILENT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR
}

,
// end lava monsters {dlb}

// begin water monsters {dlb}
{
    MONS_BIG_FISH, ';', LIGHTGREEN, "big fish",
    M_COLD_BLOOD,
    0, 10, MONS_BIG_FISH, MH_NATURAL, -3,
    { 8, 0, 0, 0 },
    { 4, 3, 5, 0 },
    1, 12, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_SMALL, S_SILENT, I_ANIMAL_LIKE, 
    MONUSE_NOTHING
}
,

{
    MONS_GIANT_GOLDFISH, ';', LIGHTRED, "giant goldfish",
    M_COLD_BLOOD,
    0, 10, MONS_GIANT_GOLDFISH, MH_NATURAL, -3,
    { 15, 0, 0, 0 },
    { 7, 3, 5, 0 },
    5, 7, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_SMALL, S_SILENT, I_ANIMAL_LIKE, 
    MONUSE_NOTHING
}
,

{
    MONS_ELECTRICAL_EEL, ';', LIGHTBLUE, "electrical eel",
    M_RES_ELEC | M_COLD_BLOOD,
    0, 10, MONS_ELECTRICAL_EEL, MH_NATURAL, -3,
    { 0, 0, 0, 0 },
    { 3, 3, 5, 0 },
    1, 15, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_SMALL, S_SILENT, I_ANIMAL_LIKE, 
    MONUSE_NOTHING
}
,

{
    MONS_JELLYFISH, 'J', CYAN, "jellyfish",
    M_RES_POISON,
    0, 10, MONS_JELLYFISH, MH_NATURAL, -3,
    { 1, 1, 0, 0 },
    { 4, 3, 5, 0 },
    0, 5, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_SMALL, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_WATER_ELEMENTAL, '{', LIGHTBLUE, "water elemental",
    M_RES_POISON | M_ED_FIRE | M_FLIES | M_RES_ELEC,
    0, 10, MONS_WATER_ELEMENTAL, MH_NONLIVING, 5000,
    { 25, 0, 0, 0 },
    { 6, 5, 3, 0 },
    0, 7, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS
}
,

{
    MONS_SWAMP_WORM, 'w', BROWN, "swamp worm",
    M_AMPHIBIOUS,
    0, 10, MONS_SWAMP_WORM, MH_NATURAL, -3,
    { 20, 0, 0, 0 },
    { 5, 5, 5, 0 },
    3, 12, 12, 0, MST_NO_SPELLS, CE_NOCORPSE, Z_SMALL, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
},
// end water monsters {dlb}

/* ************************************************************************
Josh added the following, but they just won't work in the game just yet ...
besides, four bear types !?!?! isn't that a *bit* excessive given the
limited diversity of existing monster types?

I'm still far from happy about the inclusion of "Shuggoths" -- I just do
not think it fits into Crawl ... {dlb}
************************************************************************ */
  //jmf: it's never created anywhere yet, so you can save the punctuation.
  //     as to bears & wolves: the lair needs more variety.

#if 0 
{
    MONS_SHUGGOTH, 'A', LIGHTGREEN, "shuggoth",
    M_NO_SKELETON | M_RES_ELEC | M_RES_POISON | M_RES_FIRE | M_RES_COLD | M_SEE_INVIS,
    1000, 10, MONS_SHUGGOTH, MH_DEMONIC, 300,
    { 5, 5, 5, 0 },
    { 10, 4, 4, 0 },
    10, 10, 20, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, -1, I_NORMAL, 
    MONUSE_NOTHING
}
,
#endif

{
    MONS_WOLF, 'h', LIGHTGREY, "wolf",
    M_WARM_BLOOD | M_SEE_INVIS, //jmf: until smell exists
    450, 10, MONS_WOLF, MH_NATURAL, -3,
    { 8, 2, 2, 0 },
    { 4, 3, 5, 0 },
    3, 15, 17, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_BARK, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_WARG, 'h', DARKGREY, "warg",
    M_SEE_INVIS | M_RES_POISON | M_WARM_BLOOD,
    600, 12, MONS_WARG, MH_NATURAL, -6,
    { 12, 3, 3, 0 },
    { 4, 4, 5, 0 },
    4, 12, 13, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_BARK, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_BEAR, 'U', BROWN, "bear",
    M_WARM_BLOOD,
    2000, 10, MONS_BEAR, MH_NATURAL, -3,
    { 10, 6, 6, 0 },
    { 7, 3, 3, 0 },
    4, 4, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_GROWL, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_GRIZZLY_BEAR, 'U', LIGHTGREY, "grizzly bear",
    M_WARM_BLOOD,
    2500, 10, MONS_GRIZZLY_BEAR, MH_NATURAL, -3,
    { 12, 8, 8, 0 },
    { 7, 4, 4, 0 },
    5, 8, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_GROWL, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_POLAR_BEAR, 'U', WHITE, "polar bear",
    M_RES_COLD | M_WARM_BLOOD | M_AMPHIBIOUS,
    2500, 10, MONS_POLAR_BEAR, MH_NATURAL, -3,
    { 20, 5, 5, 0 },    //jmf: polar bears have very strong jaws & necks
    { 7, 5, 3, 0 },
    7, 8, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_GROWL, I_ANIMAL, 
    MONUSE_NOTHING
}
,

{
    MONS_BLACK_BEAR, 'U', DARKGREY, "black bear",
    M_WARM_BLOOD,
    1800, 10, MONS_BLACK_BEAR, MH_NATURAL, -3,
    { 4, 4, 4, 0 },
    { 6, 3, 3, 0 },
    2, 8, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_GROWL, I_ANIMAL, 
    MONUSE_NOTHING
}
,

// small simulacrum
{
    MONS_SIMULACRUM_SMALL, 'z', WHITE, "",
    M_RES_POISON | M_ED_FIRE | M_RES_COLD,
    0, 6, MONS_SIMULACRUM_SMALL, MH_UNDEAD, -1,
    { 6, 0, 0, 0 },
    { 2, 3, 5, 0 },
    10, 4, 7, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

// large simulacrum
{
    MONS_SIMULACRUM_LARGE, 'Z', WHITE, "",
    M_RES_POISON | M_ED_FIRE | M_RES_COLD,
    0, 6, MONS_SIMULACRUM_LARGE, MH_UNDEAD, -1,
    { 14, 0, 0, 0 },
    { 5, 3, 5, 0 },
    10, 5, 7, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING
}
,

{
    MONS_GIANT_NEWT, 'l', LIGHTGREEN, "giant newt",
    M_COLD_BLOOD | M_AMPHIBIOUS,
    150, 10, MONS_GIANT_NEWT, MH_NATURAL, -3,
    { 3, 0, 0, 0 },
    { 1, 1, 2, 0 },
    0, 15, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_REPTILE, 
    MONUSE_NOTHING
}
,

{
    MONS_GIANT_GECKO, 'l', YELLOW, "giant gecko",
    M_COLD_BLOOD,
    250, 10, MONS_GIANT_GECKO, MH_NATURAL, -3,
    { 5, 0, 0, 0 },
    { 1, 3, 5, 0 },
    1, 14, 12, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_REPTILE, 
    MONUSE_NOTHING
}
,

{
    MONS_GIANT_IGUANA, 'l', BLUE, "giant iguana",
    M_COLD_BLOOD,
    400, 10, MONS_GIANT_IGUANA, MH_NATURAL, -3,
    { 15, 0, 0, 0 },
    { 3, 3, 5, 0 },
    5, 9, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_HISS, I_REPTILE, 
    MONUSE_NOTHING
}
,

{
    // gila monsters colours: lightmagenta, magenta, lightred, red, yellow
    MONS_GILA_MONSTER, 'l', BLACK, "gila monster",
    M_COLD_BLOOD,
    500, 10, MONS_GILA_MONSTER, MH_NATURAL, -3,
    { 20, 0, 0, 0 },
    { 5, 4, 4, 0 },
    3, 12, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_BIG, S_HISS, I_REPTILE, 
    MONUSE_NOTHING
}
,

{
    MONS_KOMODO_DRAGON, 'l', BROWN, "komodo dragon",
    M_COLD_BLOOD | M_AMPHIBIOUS,
    800, 10, MONS_KOMODO_DRAGON, MH_NATURAL, -3,
    { 30, 0, 0, 0 },
    { 8, 3, 5, 0 },
    7, 8, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_HISS, I_REPTILE, 
    MONUSE_NOTHING
}
,

#endif
