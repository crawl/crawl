/*
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#ifndef MONDATA_H
#define MONDATA_H

#include "enum.h"

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
    - row 3: monster resistance flags
    - row 4: mass, experience modifier, charclass, holiness, resist magic
    - row 5: damage for each of four attacks
    - row 6: hit dice, described by four parameters
    - row 7: AC, evasion, speed, speed_inc, sec(spell), corpse_thingy,
             zombie size, shouts, intel
    - row 8: gmon_use class, body size
    - row 9: long description

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
    MR_NO_FLAGS,
    0, 10, MONS_PROGRAM_BUG, MONS_PROGRAM_BUG, MH_NATURAL, -3,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 0, 0, 0, 0 },
    0, 0, 0, 0, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_HUGE,
    "Program bug"
}
,

// real monsters begin here {dlb}:
{
    MONS_GIANT_ANT, 'a', DARKGREY, "giant ant",
    M_NO_FLAGS,
    MR_VUL_POISON,
    700, 10, MONS_GIANT_ANT, MONS_GIANT_ANT, MH_NATURAL, -3,
    { {AT_BITE, AF_POISON, 8}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 3, 5, 0 },
    4, 10, 12, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_LITTLE,
    "A black ant with poisonous pincers, about the size of a large dog."
}
,

{
    MONS_GIANT_BAT, 'b', DARKGREY, "giant bat",
    M_FLIES | M_SENSE_INVIS | M_WARM_BLOOD,
    MR_NO_FLAGS,
    150, 4, MONS_GIANT_BAT, MONS_GIANT_BAT, MH_NATURAL, -1,
    { {AT_HIT, AF_PLAIN, 1}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 2, 3, 0 },
    1, 14, 30, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_TINY,
    "A huge black bat."
}
,

{
    MONS_CENTAUR, 'c', BROWN, "centaur",
    M_WARM_BLOOD,
    MR_NO_FLAGS,
    1500, 10, MONS_CENTAUR, MONS_CENTAUR, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 10}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 4, 3, 5, 0 },
    3, 7, 15, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_BIG,
    "A hybrid with the torso of a human atop the body of a large horse."
}
,

{
    MONS_RED_DEVIL, '4', RED, "red devil",
    M_FIGHTER | M_FLIES | M_EVIL,
    MR_RES_POISON | MR_RES_HELLFIRE | MR_VUL_COLD,
    0, 10, MONS_RED_DEVIL, MONS_RED_DEVIL, MH_DEMONIC, -7,
    { {AT_HIT, AF_PLAIN, 18}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 3, 5, 0 },
    10, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "The Red Devil is slightly shorter than a human, but muscular and covered in spikes and horns. Two short wings sprout from its shoulders."
}
,

{
    MONS_ETTIN, 'C', BROWN, "ettin",
    M_WARM_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    0, 10, MONS_HILL_GIANT, MONS_ETTIN, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 18}, {AT_HIT, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    3, 4, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_SHOUT2, I_NORMAL, 
    MONUSE_STARTING_EQUIPMENT, SIZE_GIANT,
    "A large, two headed humanoid. Most often seen wielding two weapons, so that the heads will have one less thing to bicker about."
}
,

{
    MONS_FUNGUS, 'f', LIGHTGREY, "fungus",
    M_NO_EXP_GAIN,
    MR_RES_POISON,
    0, 10, MONS_PLANT, MONS_FUNGUS, MH_PLANT, MAG_IMMUNE,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 5, 0 },
    1, 0, 0, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_TINY,
    "A lumpy grey fungus, growing well in the dank underground dungeon."
}
,

{
    MONS_GOBLIN, 'g', LIGHTGREY, "goblin",
    M_WARM_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    400, 10, MONS_GOBLIN, MONS_GOBLIN, MH_NATURAL, -1,
    { {AT_HIT, AF_PLAIN, 4}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 2, 4, 0 },
    0, 12, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_SMALL,
    "A short, ugly and unfriendly humanoid:  squat, broad, flat-nosed, sallow-skinned, with a wide mouth and pistachio-shaped eyes.$$\
    Clap! Snap! the black crack!$\
    Grip, grab! Pinch, nab!$\
    And down down to Goblin-town$\
    You go, my lad!$$\
    Clash, crash! Crush, smash!$\
    Hammer and tongs! Knocker and gongs!$\
    Pound, pound, far underground!$\
    Ho, ho! my lad!$$\
    Swish, smack! Whip crack!$\
    Batter and beat! Yammer and bleat!$\
    Work, work! Nor dare to shirk,$\
    While Goblins quaff, and Goblins laugh,$\
    Round and round far underground$\
    Below, my lad!$\
         J.R.R. Tolkien, _The Hobbit_"
}
,

{
    MONS_HOUND, 'h', BROWN, "hound",
    M_SENSE_INVIS | M_WARM_BLOOD,
    MR_NO_FLAGS,
    300, 10, MONS_HOUND, MONS_HOUND, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 6}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 3, 5, 0 },
    2, 13, 15, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_BARK, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_SMALL,
    "A fearsome hunting dog, lithe and alert.  Its master must be long gone, for it looks dissheveled, and has a lean and hungry look.$$\
    A traveller, by the faithful hound,$\
    Half-buried in the snow was found,$\
    Still grasping in his hand of ice$\
    That banner with the strange device,$\
    Excelsior!$\
         Henry Wadsworth Longfellow, \"Excelsior\""
}
,

// note: these things regenerate
{
    MONS_IMP, '5', RED, "imp",
    M_FLIES | M_SEE_INVIS | M_SPEAKS | M_EVIL | M_SPECIAL_ABILITY,
    MR_RES_POISON | MR_RES_HELLFIRE | MR_VUL_COLD,
    0, 13, MONS_IMP, MONS_IMP, MH_DEMONIC, -9,
    { {AT_HIT, AF_PLAIN, 4}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 3, 3, 0 },
    3, 14, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_LITTLE,
    "A small, ugly minor demon."
}
,

{
    MONS_JACKAL, 'j', YELLOW, "jackal",
    M_WARM_BLOOD,
    MR_NO_FLAGS,
    200, 10, MONS_HOUND, MONS_JACKAL, MH_NATURAL, -1,
    { {AT_BITE, AF_PLAIN, 3}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 3, 5, 0 },
    2, 12, 14, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_BARK, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_SMALL,
    "A small, dog-like scavenger. Packs of these creatures roam the underworld, searching for carrion to devour."
}
,

{
    MONS_KILLER_BEE, 'k', YELLOW, "killer bee",
    M_FLIES,
    MR_VUL_POISON,
    150, 11, MONS_KILLER_BEE, MONS_KILLER_BEE, MH_NATURAL, -3,
    { {AT_STING, AF_POISON, 10}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 3, 5, 0 },
    2, 18, 20, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_BUZZ, I_INSECT, 
    MONUSE_NOTHING, SIZE_TINY,
    "A giant bee, bearing a deadly barb which can sting repeatedly."
}
,

{
    MONS_KILLER_BEE_LARVA, 'w', LIGHTGREY, "killer bee larva",
    M_NO_SKELETON,
    MR_VUL_POISON,
    150, 5, MONS_KILLER_BEE_LARVA, MONS_KILLER_BEE_LARVA, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 3}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 3, 5, 0 },
    1, 5, 5, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_TINY,
    "A small, powerless larva of killer bee."
}
,

{
    MONS_MANTICORE, 'm', BROWN, "manticore",
    M_WARM_BLOOD | M_SPECIAL_ABILITY,
    MR_NO_FLAGS,
    1800, 10, MONS_MANTICORE, MONS_MANTICORE, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 14}, {AT_CLAW, AF_PLAIN, 8}, {AT_CLAW, AF_PLAIN, 8}, {AT_NONE, AF_PLAIN, 0} },
    { 9, 3, 5, 0 },
    5, 7, 7, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_BIG,
    "A hideous cross-breed, bearing the features of a human and a lion, with great bat-like wings. Its tail bristles with spikes that can be loosed at potential prey."
}
,

// this thing doesn't have nr. 13 for nothing, has it? ($pellbinder)
{
    MONS_NECROPHAGE, 'n', DARKGREY, "necrophage",
    M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    500, 10, MONS_GHOUL, MONS_NECROPHAGE, MH_UNDEAD, -5,
    { {AT_HIT, AF_ROT, 8}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 3, 5, 0 },
    2, 10, 10, 7, MST_NO_SPELLS, CE_HCL, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A vile undead creation of the most unholy necromancy, these creatures are made from the decaying corpses of humanoid creatures.  They exist to spread disease and decay, and gain power from the decaying corpses of other beings."
}
,

{
    MONS_ORC, 'o', LIGHTRED, "orc",
    M_WARM_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    600, 10, MONS_ORC, MONS_ORC, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 4, 6, 0 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "An ugly subterranean race, orcs combine the worst features of humans, pigs, and several other unpleasant creatures."
}
,

// XP modifier is 5 for these, because they really aren't all that
// dangerous, but still come out at 200+ XP
{
    MONS_PHANTOM, 'p', BLUE, "phantom",
    M_EVIL | M_SPECIAL_ABILITY,
    MR_RES_POISON | MR_RES_COLD,
    0, 5, MONS_PHANTOM, MONS_PHANTOM, MH_UNDEAD, -4,
    { {AT_HIT, AF_BLINK, 10}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    3, 13, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A transparent man-like undead spirit."
}
,

{
    MONS_QUASIT, 'q', LIGHTGREY, "quasit",
    M_EVIL,
    MR_RES_POISON | MR_RES_FIRE | MR_RES_COLD,
    0, 10, MONS_QUASIT, MONS_QUASIT, MH_DEMONIC, 5,
    { {AT_BITE, AF_DRAIN_DEX, 3}, {AT_CLAW, AF_DRAIN_DEX, 2}, {AT_CLAW, AF_DRAIN_DEX, 2}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 2, 6, 0 },
    5, 17, 13, 7, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_SMALL,
    "A small twisted demon with long sharply pointed tail."
}
,

{
    MONS_RAT, 'r', BROWN, "rat",
    M_WARM_BLOOD,
    MR_NO_FLAGS,
    200, 10, MONS_RAT, MONS_RAT, MH_NATURAL, -1,
    { {AT_BITE, AF_PLAIN, 3}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 1, 3, 0 },
    1, 10, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_TINY,
    "Wherever there are men, there are rats -- rattus rattus.  This rat has grown large and aggressive in the pestilential dungeon environment.$$\
    How now? a rat? Dead, for a ducat, dead!$\
         -William Shakespeare, Hamlet, Act III, 4"
}
,

{
    MONS_SCORPION, 's', DARKGREY, "scorpion",
    M_NO_FLAGS,
    MR_VUL_POISON,
    500, 10, MONS_SCORPION, MONS_SCORPION, MH_NATURAL, -3,
    { {AT_STING, AF_POISON_MEDIUM, 10}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 3, 5, 0 },
    5, 10, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_TINY,
    "A giant black scorpion, its body covered in thick armour plating, and its tail tipped by a nasty venomous sting."
}
,

/* ******************************************************************
// the tunneling worm is no more ...
// not until it can be reimplemented safely {dlb}
{
    MONS_TUNNELING_WORM, 't', LIGHTRED, "tunneling worm",
    M_NO_FLAGS,
    MR_RES_POISON,
    0, 10, 19, MH_NATURAL, MAG_IMMUNE,
    { 50, 0, 0, 0 },
    { 10, 5, 5, 0 },
    3, 3, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_REPTILE, 
    MONUSE_NOTHING, SIZE_HUGE,
    ""
}
,
****************************************************************** */

{
    MONS_UGLY_THING, 'u', BROWN, "ugly thing",
    M_WARM_BLOOD | M_AMPHIBIOUS,
    MR_NO_FLAGS,
    600, 10, MONS_UGLY_THING, MONS_UGLY_THING, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 5, 0 },
    3, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "An ugly thing. Yuck."
}
,

{
    MONS_FIRE_VORTEX, 'v', RED, "fire vortex",
    M_LEVITATE | M_CONFUSED,
    MR_RES_POISON | MR_RES_FIRE | MR_VUL_COLD | MR_RES_ELEC,
    0, 5, MONS_FIRE_VORTEX, MONS_FIRE_VORTEX, MH_NONLIVING, MAG_IMMUNE,
    { {AT_HIT, AF_FIRE, 30}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 3, 5, 0 },
    0, 5, 15, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_LARGE,
    "A swirling cloud of flame."
}
,

{
    MONS_WORM, 'w', LIGHTRED, "worm",
    M_NO_SKELETON,
    MR_NO_FLAGS,
    350, 4, MONS_WORM, MONS_WORM, MH_NATURAL, -2,
    { {AT_BITE, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 3, 5, 0 },
    1, 5, 6, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_MEDIUM,
    "A giant worm, with unusually large teeth."
}
,

// random
{
    MONS_ABOMINATION_SMALL, 'x', BLACK, "abomination",
    M_EVIL,
    MR_NO_FLAGS,
    0, 10, MONS_ABOMINATION_SMALL, MONS_ABOMINATION_SMALL, MH_DEMONIC, -5,
    { {AT_HIT, AF_PLAIN, 23}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 2, 5, 0 },
    0, 0, 0, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_LARGE,
    "A hideous form, created or summoned by some arcane process."
}
,

{
    MONS_YELLOW_WASP, 'y', YELLOW, "yellow wasp",
    M_FLIES,
    MR_VUL_POISON,
    220, 12, MONS_YELLOW_WASP, MONS_YELLOW_WASP, MH_NATURAL, -3,
    { {AT_STING, AF_PARALYSE, 13}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 4, 3, 5, 0 },
    5, 14, 15, 7, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_TINY,
    "A giant wasp covered with thick plates of yellow chitinous armour."
}
,

// small zombie
{
    MONS_ZOMBIE_SMALL, 'z', BROWN, "",
    M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    0, 6, MONS_ZOMBIE_SMALL, MONS_ZOMBIE_SMALL, MH_UNDEAD, -1,
    { {AT_HIT, AF_PLAIN, 10}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 5, 5, 0 },
    0, 4, 5, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_SMALL,
    "A corpse raised to undeath by necromancy."
}
,

{
    MONS_ANGEL, 'A', WHITE, "Angel",
    M_FIGHTER | M_FLIES | M_SPELLCASTER | M_SEE_INVIS,
    MR_RES_POISON | MR_RES_ELEC,
    0, 10, MONS_ANGEL, MONS_ANGEL, MH_HOLY, -8,
    { {AT_HIT, AF_PLAIN, 25}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 12, 6, 5, 0 },
    10, 20, 15, 7, MST_ANGEL, CE_NOCORPSE, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A winged holy being of unnatural beauty. It has an aura of brilliant white light. "
}
,

{
    MONS_GIANT_BEETLE, 'B', DARKGREY, "giant beetle",
    M_NO_FLAGS,
    MR_VUL_POISON,
    1000, 10, MONS_GIANT_BEETLE, MONS_GIANT_BEETLE, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 7, 6, 0 },
    10, 3, 5, 7, MST_NO_SPELLS, CE_POISONOUS, Z_BIG, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_MEDIUM,
    "A huge black beetle with great crunching mandibles and very hard chitinous armour."
}
,

{
    MONS_CYCLOPS, 'C', BROWN, "cyclops",
    M_WARM_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    2500, 10, MONS_HILL_GIANT, MONS_CYCLOPS, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 35}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 9, 3, 5, 0 },
    5, 3, 7, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_STARTING_EQUIPMENT, SIZE_GIANT,
    "A giant with one eye in the centre of its forehead.  Despite their lack of binocular vision, cyclopes throw boulders with fearsome accuracy."
}
,

{
    MONS_DRAGON, 'D', GREEN, "dragon",
    M_FLIES | M_SPECIAL_ABILITY, //jmf: warm blood?
    MR_RES_POISON | MR_RES_FIRE | MR_VUL_COLD,
    2200, 12, MONS_DRAGON, MONS_DRAGON, MH_NATURAL, -4,
    { {AT_BITE, AF_PLAIN, 20}, {AT_CLAW, AF_PLAIN, 13}, {AT_CLAW, AF_PLAIN, 13}, {AT_NONE, AF_PLAIN, 0} },
    { 12, 5, 5, 0 },
    10, 8, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_HUGE,
    "A great reptilian beast, covered in thick green scales and with two huge bat-like wings. Little trails of smoke spill from its toothy maw."
}
,

// These guys get understated because the experience code can't see
// that they wield two weapons... I'm raising their xp modifier. -- bwr
{
    MONS_TWO_HEADED_OGRE, 'O', LIGHTRED, "two-headed ogre",
    M_WARM_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    1500, 15, MONS_OGRE, MONS_TWO_HEADED_OGRE, MH_NATURAL, -4,
    { {AT_HIT, AF_PLAIN, 17}, {AT_HIT, AF_PLAIN, 13}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 5, 0 },
    1, 4, 8, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_SHOUT2, I_NORMAL, 
    MONUSE_STARTING_EQUIPMENT, SIZE_LARGE,
    "A huge ogre with two heads on top of a bloated ogre body. It is capable of holding a weapon in each giant hand."
}
,

{
    MONS_FIEND, '1', LIGHTRED, "Fiend", //jmf: was RED, like Balrog
    M_FLIES | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_HELLFIRE | MR_VUL_COLD,
    0, 18, MONS_FIEND, MONS_FIEND, MH_DEMONIC, -12,
    { {AT_HIT, AF_PLAIN, 25}, {AT_HIT, AF_PLAIN, 15}, {AT_HIT, AF_PLAIN, 15}, {AT_NONE, AF_PLAIN, 0} },
    { 18, 3, 5, 0 },
      15, 6, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "One of the most fearsome denizens of any Hell. A huge and powerful demon wreathed in hellfire, with great scaly wings."
}
,

{
    MONS_GIANT_SPORE, 'G', GREEN, "giant spore",
    M_LEVITATE,
    MR_RES_POISON | MR_RES_ASPHYX,
    0, 10, MONS_PLANT, MONS_GIANT_SPORE, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 1}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 0, 0, 1 },
    0, 10, 15, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_SMALL,
    "A volatile floating ball of spores, covered in knobbly rhizome growths."
}
,

{
    MONS_HOBGOBLIN, 'g', BROWN, "hobgoblin",
    M_WARM_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    500, 10, MONS_GOBLIN, MONS_HOBGOBLIN, MH_NATURAL, -1,
    { {AT_HIT, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 4, 5, 0 },
    2, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A larger and stronger relative of the goblin.$$\
\"Consistency is the hobgoblin of small minds\"$\
-Ralph Waldo Emerson"\
}
,

{
    MONS_ICE_BEAST, 'I', WHITE, "ice beast",
    M_NO_FLAGS,
    MR_RES_POISON | MR_RES_ASPHYX | MR_VUL_FIRE | MR_RES_COLD,
    0, 12, MONS_ICE_BEAST, MONS_ICE_BEAST, MH_NATURAL, -3,
    { {AT_CLAW, AF_COLD, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 3, 5, 0 },
    5, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_ANIMAL_LIKE, 
    MONUSE_NOTHING, SIZE_LARGE,
    "A terrible creature, formed of snow and crystalline ice. Its feet leave puddles of icy water on the floor."
}
,

{
    MONS_JELLY, 'J', LIGHTRED, "jelly",
    M_SEE_INVIS | M_SPLITS | M_AMPHIBIOUS | M_ACID_SPLASH,
    MR_RES_POISON | MR_RES_ASPHYX | MR_RES_ACID,
    0, 13, MONS_JELLY, MONS_JELLY, MH_NATURAL, -3,
    { {AT_HIT, AF_ACID, 8}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 5, 5, 0 },
    0, 2, 9, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_EATS_ITEMS, SIZE_MEDIUM,
    "A pulsating mass of acidic protoplasm. It can and will eat almost anything, and grows a little each time..."
}
,

{
    MONS_KOBOLD, 'K', BROWN, "kobold",
    M_WARM_BLOOD,
    MR_NO_FLAGS,
    400, 10, MONS_KOBOLD, MONS_KOBOLD, MH_NATURAL, -1,
    { {AT_HIT, AF_PLAIN, 4}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 2, 3, 0 },
    2, 12, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_SMALL,
    "Reputedly the creation of an ancient demon-god, kobolds are small goblin-like creatures with canine heads."
}
,

{
    MONS_LICH, 'L', WHITE, "lich",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_COLD | MR_RES_ELEC,
    0, 16, MONS_LICH, MONS_LICH, MH_UNDEAD, -11,
    { {AT_TOUCH, AF_DRAIN_XP, 15}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 20, 2, 4, 0 },
    10, 10, 10, 7, MST_LICH_I, CE_NOCORPSE, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A wizard who didn't want to die, a Lich is a skeletal, desiccated corpse kept alive by a mighty exercise of necromancy.  These undead creatures can wield great magic and are best avoided by all but the most confident."
}
,

{
    MONS_MUMMY, 'M', WHITE, "mummy",
    M_EVIL,
    MR_RES_POISON | MR_VUL_FIRE | MR_RES_COLD,
    0, 10, MONS_MUMMY, MONS_MUMMY, MH_UNDEAD, -5,
    { {AT_HIT, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 5, 3, 0 },
    3, 6, 6, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_ANIMAL, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "An undead figure covered in bandages and embalming fluids, compelled to walk by an ancient curse. It radiates a malign aura to those who intrude on its domain."        
}
,

{
    MONS_GUARDIAN_NAGA, 'N', LIGHTGREEN, "guardian naga",
    M_SPELLCASTER | M_SEE_INVIS | M_ACTUAL_SPELLS | M_WARM_BLOOD,
    MR_RES_POISON,
    350, 10, MONS_NAGA, MONS_GUARDIAN_NAGA, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 19}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 5, 0 },
    6, 14, 15, 7, MST_GUARDIAN_NAGA, CE_MUTAGEN_RANDOM, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_BIG,
    "A hybrid; human from the chest up, with a scaly, muscular torso trailing off like that of a snake. "
}
,

{
    MONS_OGRE, 'O', BROWN, "ogre",
    M_WARM_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    1300, 10, MONS_OGRE, MONS_OGRE, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 17}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 3, 5, 0 },
    1, 6, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_STARTING_EQUIPMENT, SIZE_LARGE,
    "A larger, uglier and fatter relative of orcs and goblins."
}
,

{
    MONS_PLANT, 'P', GREEN, "plant",
    M_NO_EXP_GAIN,
    MR_RES_POISON,
    0, 10, MONS_PLANT, MONS_PLANT, MH_PLANT, MAG_IMMUNE,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 3, 5, 0 },
    10, 0, 0, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_SMALL,
    "Few plants can grow in the unpleasant dungeon environment, but some have managed to adapt and even thrive underground in the absence of the sun."
}
,

{
    MONS_QUEEN_BEE, 'Q', YELLOW, "queen bee",
    M_FLIES,
    MR_VUL_POISON,
    200, 14, MONS_KILLER_BEE, MONS_QUEEN_BEE, MH_NATURAL, -3,
    { {AT_STING, AF_POISON_NASTY, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    10, 10, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_LITTLE,
    "Even larger and more dangerous-looking than its offspring, this creature wants you out of its hive. Now!"
}
,

{
    MONS_RAKSHASA, 'R', YELLOW, "rakshasa",
    M_SPELLCASTER | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON,
    0, 15, MONS_RAKSHASA, MONS_RAKSHASA, MH_NATURAL, -10,
    { {AT_HIT, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 3, 5, 0 },
    10, 14, 10, 7, MST_RAKSHASA, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A type of demon who comes to the material world in search of power and knowledge. Rakshasas are experts in the art of illusion, among other things."
}
,

{
    MONS_SNAKE, 'S', GREEN, "snake",
    M_COLD_BLOOD | M_AMPHIBIOUS,
    MR_NO_FLAGS,
    200, 10, MONS_SNAKE, MONS_SNAKE, MH_NATURAL, -3,
    { {AT_BITE, AF_POISON, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 2, 3, 5, 0 },
    1, 15, 13, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_REPTILE, 
    MONUSE_NOTHING, SIZE_SMALL,
    "The common dungeon snake."
}
,

{
    MONS_TROLL, 'T', BROWN, "troll",
    M_WARM_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    1500, 10, MONS_TROLL, MONS_TROLL, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 20}, {AT_CLAW, AF_PLAIN, 15}, {AT_CLAW, AF_PLAIN, 15}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    3, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "A huge, nasty-looking creature. Its thick and knobbly hide seems to heal almost instantly from most wounds."
}
,

{
    MONS_UNSEEN_HORROR, 'x', MAGENTA, "unseen horror",
    M_LEVITATE | M_SEE_INVIS | M_INVIS,
    MR_NO_FLAGS,
    0, 12, MONS_UNSEEN_HORROR, MONS_UNSEEN_HORROR, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    5, 10, 30, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_ANIMAL_LIKE, 
    MONUSE_NOTHING, SIZE_MEDIUM,
    "These creatures are usually unseen by the eyes of most, and those few who have seen them would rather not have."
}
,

{
    MONS_VAMPIRE, 'V', RED, "vampire",
    M_SPELLCASTER | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    0, 11, MONS_VAMPIRE, MONS_VAMPIRE, MH_UNDEAD, -6,
    { {AT_BITE, AF_VAMPIRIC, 22}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 5, 0 },
    10, 10, 10, 7, MST_VAMPIRE, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A powerful undead."
}
,

{
    MONS_WRAITH, 'W', DARKGREY, "wraith",
    M_LEVITATE | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    0, 11, MONS_WRAITH, MONS_WRAITH, MH_UNDEAD, -7,
    { {AT_HIT, AF_DRAIN_XP, 13}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 5, 0 },
    10, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "This undead spirit appears as a cloud of black mist surrounding an insubstantial skeletal form. Its eyes burn bright with unholy malevolence."
}
,

// Large abom: (the previous one was small)
{
    MONS_ABOMINATION_LARGE, 'X', BLACK, "abomination",
    M_EVIL,
    MR_NO_FLAGS,
    0, 10, MONS_ABOMINATION_SMALL, MONS_ABOMINATION_LARGE, MH_DEMONIC, -7,
    { {AT_HIT, AF_PLAIN, 40}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 11, 2, 5, 0 },
    0, 0, 0, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_BIG,
    "A huge and hideous form, created or summoned by some arcane process."
}
,

{
    MONS_YAK, 'Y', BROWN, "yak",
    M_WARM_BLOOD,
    MR_NO_FLAGS,
    1200, 10, MONS_YAK, MONS_YAK, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 18}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    4, 7, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_BELLOW, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_BIG,
    "The common dungeon yak, covered in shaggy yak hair and bearing a nasty pair of yak horns."
}
,

// big zombie
{
    MONS_ZOMBIE_LARGE, 'Z', BROWN, "",
    M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    0, 6, MONS_ZOMBIE_SMALL, MONS_ZOMBIE_LARGE, MH_UNDEAD, -1,
    { {AT_HIT, AF_PLAIN, 23}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 5, 0 },
    8, 5, 5, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_LARGE,
    "A large corpse raised to undeath by necromancy."
}
,

{
    MONS_ORC_WARRIOR, 'o', YELLOW, "orc warrior",
    M_FIGHTER | M_WARM_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    0, 10, MONS_ORC, MONS_ORC, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 4, 4, 6, 0 },
    0, 13, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "An armoured orc, obviously experienced in the ways of hacking other creatures apart."
}
,

{
    MONS_KOBOLD_DEMONOLOGIST, 'K', MAGENTA, "kobold demonologist",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    0, 10, MONS_KOBOLD, MONS_KOBOLD, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 4}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 4, 3, 5, 0 },
    2, 13, 10, 7, MST_KOBOLD_DEMONOLOGIST, CE_POISONOUS, Z_NOZOMBIE, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_SMALL,
    "A kobold who has learned to summon and direct demons."
}
,

{
    MONS_ORC_WIZARD, 'o', MAGENTA, "orc wizard",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    0, 10, MONS_ORC, MONS_ORC, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 3, 4, 0 },
    1, 12, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "While orcs are generally quite stupid, occasionally one develops an aptitude for magic."
}
,

{
    MONS_ORC_KNIGHT, 'o', LIGHTCYAN, "orc knight",
    M_FIGHTER | M_WARM_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    0, 10, MONS_ORC, MONS_ORC, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 25}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 9, 4, 7, 0 },
    2, 13, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A heavily armoured orc, covered in scars from many past battles."
}
,

/* ******************************************************************
// the tunneling worm is no more ...
// not until it can be reimplemented safely {dlb}
{
    MONS_WORM_TAIL, '~', LIGHTRED, "worm tail",
    M_NO_EXP_GAIN,
    MR_RES_POISON,
    0, 10, 56, MH_NATURAL, MAG_IMMUNE,
    { 0, 0, 0, 0 },
    { 10, 5, 5, 0 },
    3, 3, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_HUGE,
    ""
}
,
****************************************************************** */

{
    MONS_WYVERN, 'D', LIGHTRED, "wyvern",
    M_NO_FLAGS, //jmf: warm blood?
    MR_NO_FLAGS,
    2000, 10, MONS_WYVERN, MONS_WYVERN, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 3, 5, 0 },
    5, 10, 15, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_BIG,
    "A dragon-like creature with long sharply pointed tail.  Although smaller and less formidable than true dragons, wyverns are nonetheless a foe to be reckoned with."
}
,

{
    MONS_BIG_KOBOLD, 'K', RED, "big kobold",
    M_WARM_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    0, 10, MONS_KOBOLD, MONS_BIG_KOBOLD, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 7}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 3, 5, 0 },
    3, 12, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_SMALL,
    "An unusually large kobold."
}
,

{
    MONS_GIANT_EYEBALL, 'G', WHITE, "giant eyeball",
    M_NO_SKELETON | M_LEVITATE,
    MR_RES_ASPHYX,
    400, 10, MONS_GIANT_EYEBALL, MONS_GIANT_EYEBALL, MH_NATURAL, -3,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 3, 5, 0 },
    0, 1, 3, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_MEDIUM,
    "A giant eyeball, with a captivating stare."
}
,

{
    MONS_WIGHT, 'W', LIGHTGREY, "wight",
    M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    0, 10, MONS_WRAITH, MONS_WIGHT, MH_UNDEAD, -4,
    { {AT_HIT, AF_DRAIN_XP, 8}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 3, 5, 0 },
    4, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "An ancient warrior, kept in a state of undeath by its will to live."
}
,

{
    MONS_OKLOB_PLANT, 'P', GREEN, "oklob plant",
    M_SPECIAL_ABILITY,
    MR_RES_POISON,
    0, 10, MONS_PLANT, MONS_OKLOB_PLANT, MH_PLANT, -3,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 3, 5, 0 },
    10, 0, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_SMALL,
    "A vicious plant, dripping with vitriol."
}
,

{
    MONS_WOLF_SPIDER, 's', BROWN, "wolf spider",
    M_NO_FLAGS,
    MR_VUL_POISON,
    800, 10, MONS_WOLF_SPIDER, MONS_WOLF_SPIDER, MH_NATURAL, -3,
    { {AT_BITE, AF_POISON_MEDIUM, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 5, 0 },
    3, 10, 15, 7, MST_NO_SPELLS, CE_POISONOUS, Z_BIG, S_HISS, I_INSECT, 
    MONUSE_NOTHING, SIZE_TINY,
    "A large hairy spider with vicious mandibles, roaming the dungeon in search of food."
}
,

{
    MONS_SHADOW, ' ', BLACK, "shadow",
    M_SEE_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    0, 10, MONS_WRAITH, MONS_SHADOW, MH_UNDEAD, -5,
    { {AT_HIT, AF_DRAIN_STR, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 3, 5, 0 },
    12, 10, 10, 7, BLACK, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_ANIMAL, 
    MONUSE_OPEN_DOORS, SIZE_SMALL,
    "An wisp of unliving shadow, drifting on the edge of vision."
}
,

{
    MONS_HUNGRY_GHOST, 'p', GREEN, "hungry ghost",
    M_SENSE_INVIS | M_FLIES | M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    0, 10, MONS_PHANTOM, MONS_HUNGRY_GHOST, MH_UNDEAD, -4,
    { {AT_HIT, AF_HUNGER, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    0, 17, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "The undead form of someone who died of starvation, this creature wants the same thing to happen to you!"
}
,

{
    MONS_EYE_OF_DRAINING, 'G', LIGHTGREY, "eye of draining",
    M_NO_SKELETON | M_LEVITATE | M_SEE_INVIS,
    MR_RES_ASPHYX,
    400, 10, MONS_GIANT_EYEBALL, MONS_EYE_OF_DRAINING, MH_NATURAL, MAG_IMMUNE,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    3, 1, 5, 7, MST_NO_SPELLS, CE_MUTAGEN_RANDOM, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_MEDIUM,
    "These hovering horrors are especially loathed by wizards."
}
,

{
    MONS_BUTTERFLY, 'b', BLACK, "butterfly",
    M_FLIES | M_CONFUSED,
    MR_VUL_POISON | MR_RES_ASPHYX,
    150, 10, MONS_BUTTERFLY, MONS_BUTTERFLY, MH_NATURAL, -3,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 3, 5, 0 },
    0, 25, 25, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_TINY,
    "A large multicoloured butterfly with beautifully patterned wings."
}
,

{
    MONS_WANDERING_MUSHROOM, 'f', BROWN, "wandering mushroom",
    M_NO_FLAGS,
    MR_RES_POISON,
    0, 10, MONS_PLANT, MONS_WANDERING_MUSHROOM, MH_PLANT, -3,
    { {AT_SPORE, AF_CONFUSE, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 5, 0 },
    5, 0, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_TINY,
    "A large, fat mushroom."
}
,

{
    MONS_EFREET, 'E', RED, "efreet",
    M_SPELLCASTER | M_LEVITATE | M_EVIL,
    MR_RES_POISON | MR_RES_FIRE | MR_VUL_COLD,
    0, 12, MONS_EFREET, MONS_EFREET, MH_DEMONIC, -3,
    { {AT_HIT, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    10, 5, 10, 7, MST_EFREET, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_LARGE,
    "A huge and muscular figure engulfed in a cloud of searing flame."
}
,

{
    MONS_BRAIN_WORM, 'w', LIGHTMAGENTA, "brain worm",
    M_SPELLCASTER,
    MR_NO_FLAGS,
    150, 10, MONS_WORM, MONS_BRAIN_WORM, MH_NATURAL, -3,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 3, 3, 0 },
    1, 5, 10, 7, MST_BRAIN_WORM, CE_POISONOUS, Z_SMALL, S_SILENT, I_REPTILE, 
    MONUSE_NOTHING, SIZE_SMALL,
    "A slimy mauve worm with a greatly distended head."
}
,

{
    MONS_GIANT_ORANGE_BRAIN, 'G', LIGHTRED, "giant orange brain",
    M_NO_SKELETON | M_SPELLCASTER | M_LEVITATE | M_SEE_INVIS,
    MR_RES_ASPHYX,
    1000, 13, MONS_GIANT_ORANGE_BRAIN, MONS_GIANT_ORANGE_BRAIN, MH_NATURAL, -8,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 3, 5, 0 },
    2, 4, 10, 7, MST_GIANT_ORANGE_BRAIN, CE_MUTAGEN_RANDOM, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_SMALL,
    "A huge wrinkled brain, floating just off the floor. Every now and then it seems to pulsate."
}
,

{
    MONS_BOULDER_BEETLE, 'B', LIGHTGREY, "boulder beetle",
    M_NO_FLAGS,
    MR_VUL_POISON,
    2500, 10, MONS_GIANT_BEETLE, MONS_BOULDER_BEETLE, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 35}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 9, 3, 5, 0 },
    20, 2, 3, 7, MST_NO_SPELLS, CE_POISONOUS, Z_BIG, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_LARGE,
    "A huge grey beetle with an almost impenetrable rocky carapace."
}
,

{
    MONS_FLYING_SKULL, 'z', WHITE, "flying skull",
    M_LEVITATE,
    MR_RES_POISON | MR_RES_FIRE | MR_RES_COLD | MR_RES_ELEC,
    0, 10, MONS_SKELETON_SMALL, MONS_FLYING_SKULL, MH_UNDEAD, -3,
    { {AT_HIT, AF_PLAIN, 7}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 2, 3, 5, 0 },
    10, 17, 15, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SCREAM, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_TINY,
    "Unholy magic keeps a disembodied undead skull hovering above the floor. It has a nasty set of teeth."
}
,

{
    MONS_HELL_HOUND, 'h', DARKGREY, "hell hound",
    M_SENSE_INVIS | M_EVIL | M_SPECIAL_ABILITY,
    MR_RES_POISON | MR_RES_HELLFIRE | MR_VUL_COLD,
    0, 10, MONS_HOUND, MONS_HELL_HOUND, MH_DEMONIC, -3,
    { {AT_BITE, AF_PLAIN, 13}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 3, 5, 0 },
    6, 13, 15, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_BARK, I_NORMAL, 
    MONUSE_NOTHING, SIZE_MEDIUM,
    "A huge black dog, with glowing red eyes and smoke pouring from its fanged mouth."
}
,

{
    MONS_MINOTAUR, 'm', LIGHTRED, "minotaur",
    M_FIGHTER | M_WARM_BLOOD,
    MR_NO_FLAGS,
    1500, 10, MONS_MINOTAUR, MONS_MINOTAUR, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 35}, {AT_BUTT, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 13, 3, 5, 0 },
    5, 7, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_BELLOW, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A large muscular human with the head of a bull. It makes its home in secluded labyrinths."
}
,

{
    MONS_ICE_DRAGON, 'D', WHITE, "ice dragon",
    M_FLIES | M_SPECIAL_ABILITY,
    MR_RES_POISON | MR_VUL_FIRE | MR_RES_COLD,
    2200, 10, MONS_DRAGON, MONS_ICE_DRAGON, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 17}, {AT_CLAW, AF_PLAIN, 17}, {AT_CLAW, AF_PLAIN, 17}, {AT_NONE, AF_PLAIN, 0} },
    { 12, 5, 5, 0 },
    10, 8, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_HUGE,
    "Like a normal dragon, only white and covered in frost."
}
,

{
    MONS_SLIME_CREATURE, 'J', GREEN, "slime creature",
    M_AMPHIBIOUS,
    MR_RES_POISON,
    0, 5, MONS_SLIME_CREATURE, MONS_SLIME_CREATURE, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 22}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 11, 3, 5, 0 },
    1, 4, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_SMALL,
    "An icky glob of slime, which slithers along the ground."
}
,

{
    MONS_FREEZING_WRAITH, 'W', LIGHTBLUE, "freezing wraith",
    M_LEVITATE | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON | MR_VUL_FIRE | MR_RES_COLD,
    0, 10, MONS_WRAITH, MONS_FREEZING_WRAITH, MH_UNDEAD, -4,
    { {AT_HIT, AF_COLD, 19}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 5, 0 },
    12, 10, 8, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A cloud of freezing air surrounding an incorporeal skeletal form."
}
,

// fake R - conjured by the R's illusion spell.
{
    MONS_RAKSHASA_FAKE, 'R', YELLOW, "rakshasa",
    M_EVIL,
    MR_RES_POISON,
    0, 10, MONS_RAKSHASA_FAKE, MONS_RAKSHASA_FAKE, MH_NATURAL, MAG_IMMUNE,
    { {AT_HIT, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 0, 0, 1 },
    0, 30, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_ROAR, I_PLANT, 
    MONUSE_NOTHING, SIZE_MEDIUM,
    "A type of demon who comes to the material world in search of power and knowledge. Rakshasas are experts in the art of illusion, among other things."
}
,

{
    MONS_GREAT_ORB_OF_EYES, 'G', LIGHTGREEN, "great orb of eyes",
    M_NO_SKELETON | M_SPELLCASTER | M_LEVITATE | M_SEE_INVIS,
    MR_RES_POISON,
    900, 13, MONS_GIANT_EYEBALL, MONS_GREAT_ORB_OF_EYES, MH_NATURAL, MAG_IMMUNE,
    { {AT_BITE, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 12, 3, 5, 0 },
    10, 3, 10, 7, MST_GREAT_ORB_OF_EYES, CE_MUTAGEN_RANDOM, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "A levitating ball, covered in malignant eyes."
}
,

{
    MONS_HELLION, '3', EC_FIRE, "hellion",
    M_SPELLCASTER | M_EVIL,
    MR_RES_POISON | MR_RES_HELLFIRE | MR_VUL_COLD,
    0, 11, MONS_HELLION, MONS_HELLION, MH_DEMONIC, -7,
    { {AT_HIT, AF_PLAIN, 10}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    5, 10, 13, 7, MST_BURNING_DEVIL, CE_NOCORPSE, Z_NOZOMBIE, S_SCREAM, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A frightful demon, covered in roaring hellfire."
}
,

{
    MONS_ROTTING_DEVIL, '4', GREEN, "rotting devil",
    M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    0, 10, MONS_ROTTING_DEVIL, MONS_ROTTING_DEVIL, MH_DEMONIC, -7,
    { {AT_HIT, AF_ROT, 8}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 3, 5, 0 },
    2, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A hideous decaying form."
}
,

{
    MONS_TORMENTOR, '3', YELLOW, "tormentor",
    M_SPELLCASTER | M_FLIES | M_SPEAKS | M_EVIL,
    MR_RES_POISON | MR_RES_FIRE,
    0, 10, MONS_TORMENTOR, MONS_TORMENTOR, MH_DEMONIC, -6,
    { {AT_HIT, AF_PLAIN, 8}, {AT_HIT, AF_PLAIN, 8}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    12, 12, 13, 7, MST_TORMENTOR, CE_NOCORPSE, Z_NOZOMBIE, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "This malign devil is covered in all manner of claws, spines and cruel hooks."
}
,

{
    MONS_REAPER, '2', LIGHTGREY, "reaper",
    M_FIGHTER | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    0, 10, MONS_REAPER, MONS_REAPER, MH_DEMONIC, MAG_IMMUNE,
    { {AT_HIT, AF_PLAIN, 32}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 5, 0 },
    15, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_STARTING_EQUIPMENT, SIZE_MEDIUM,
    "A skeletal form wielding a giant scythe."
}
,

{
    MONS_SOUL_EATER, '2', DARKGREY, "soul eater",
    M_LEVITATE | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    0, 12, MONS_SOUL_EATER, MONS_SOUL_EATER, MH_DEMONIC, -10,
    { {AT_HIT, AF_DRAIN_XP, 25}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 11, 3, 5, 0 },
    18, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "This greater demon looks like a shadow gliding through the air towards you. It radiates an intense aura of negative power."
}
,

{
    MONS_HAIRY_DEVIL, '4', LIGHTRED, "hairy devil",
    M_EVIL,
    MR_RES_POISON,
    0, 10, MONS_HAIRY_DEVIL, MONS_HAIRY_DEVIL, MH_DEMONIC, -4,
    { {AT_HIT, AF_PLAIN, 9}, {AT_HIT, AF_PLAIN, 9}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 5, 0 },
    7, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_SMALL,
    "A small humanoid demon covered in brown hair. Watch out - it may have fleas!"
}
,

{
    MONS_ICE_DEVIL, '2', WHITE, "ice devil",
    M_EVIL,
    MR_RES_POISON | MR_VUL_FIRE | MR_RES_COLD,
    0, 11, MONS_ICE_DEVIL, MONS_ICE_DEVIL, MH_DEMONIC, -6,
    { {AT_HIT, AF_COLD, 16}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 11, 3, 5, 0 },
    12, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A man-sized demon covered in glittering ice."
}
,

{
    MONS_BLUE_DEVIL, '3', BLUE, "blue devil",
    M_FLIES | M_EVIL,
    MR_RES_POISON | MR_VUL_FIRE | MR_RES_COLD,
    0, 10, MONS_BLUE_DEVIL, MONS_BLUE_DEVIL, MH_DEMONIC, -5,
    { {AT_HIT, AF_PLAIN, 21}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    14, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A strange and nasty blue thing. It looks cold."
}
,

// random
{
    MONS_BEAST, '4', BROWN, "beast",
    M_EVIL,
    MR_NO_FLAGS,
    0, 10, MONS_BEAST, MONS_BEAST, MH_DEMONIC, -3,
    { {AT_CLAW, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 3, 5, 0 },
    0, 0, 0, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_RANDOM, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "A weird and hideous cross between beast and human."
}
,

{
    MONS_IRON_DEVIL, '3', CYAN, "iron devil",
    M_EVIL,
    MR_RES_ELEC | MR_RES_POISON | MR_RES_HELLFIRE | MR_RES_COLD,
    0, 10, MONS_IRON_DEVIL, MONS_IRON_DEVIL, MH_DEMONIC, -6,
    { {AT_HIT, AF_PLAIN, 14}, {AT_HIT, AF_PLAIN, 14}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 5, 0 },
    16, 8, 8, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SCREECH, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A hideous humanoid figure with metal skin."
}
,

{
    MONS_GLOWING_SHAPESHIFTER, '@', RED, "glowing shapeshifter",
    M_NO_FLAGS,
    MR_NO_FLAGS,
    600, 10, MONS_SHAPESHIFTER, MONS_GLOWING_SHAPESHIFTER, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 15}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 3, 5, 0 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_MUTAGEN_RANDOM, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A shapeshifter who has lost control over its transformations, and is constantly changing form."
}
,

{
    MONS_SHAPESHIFTER, '@', LIGHTRED, "shapeshifter",
    M_NO_FLAGS,
    MR_NO_FLAGS,
    600, 10, MONS_SHAPESHIFTER, MONS_SHAPESHIFTER, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_MUTAGEN_RANDOM, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A weird creature with the power to change its form. It is very rarely observed alive in its natural state."
}
,

{
    MONS_GIANT_MITE, 's', LIGHTRED, "giant mite",
    M_NO_FLAGS,
    MR_VUL_POISON,
    350, 10, MONS_GIANT_MITE, MONS_GIANT_MITE, MH_NATURAL, -1,
    { {AT_BITE, AF_POISON, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 2, 3, 5, 0 },
    1, 7, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_TINY,
    "A large arachnid with vicious poisoned mouth-parts."
}
,

{
    MONS_STEAM_DRAGON, 'D', LIGHTGREY, "steam dragon",
    M_SPELLCASTER | M_FLIES,
    MR_NO_FLAGS,
    1000, 10, MONS_DRAGON, MONS_STEAM_DRAGON, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 12}, {AT_CLAW, AF_PLAIN, 6}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 4, 5, 5, 0 },
    5, 10, 10, 7, MST_STEAM_DRAGON, CE_CLEAN, Z_BIG, S_SILENT, I_ANIMAL_LIKE, 
    MONUSE_OPEN_DOORS, SIZE_GIANT,
    "A relatively small grey dragon, with steam pouring from its mouth."
}
,

{
    MONS_VERY_UGLY_THING, 'u', RED, "very ugly thing",
    M_WARM_BLOOD | M_AMPHIBIOUS,
    MR_NO_FLAGS,
    750, 10, MONS_UGLY_THING, MONS_VERY_UGLY_THING, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 17}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 12, 3, 5, 0 },
    4, 8, 8, 7, MST_NO_SPELLS, CE_MUTAGEN_RANDOM, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "A very ugly thing. Double yuck."
}
,

{
    MONS_ORC_SORCERER, 'o', DARKGREY, "orc sorcerer",
    M_SPELLCASTER | M_SEE_INVIS | M_SPEAKS | M_ACTUAL_SPELLS | M_WARM_BLOOD | M_EVIL,
    MR_RES_FIRE,
    600, 12, MONS_ORC, MONS_ORC, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 7}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 2, 3, 0 },
    5, 12, 10, 7, MST_ORC_SORCERER, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "An orc who draws magical power from Hell."
}
,

{
    MONS_HIPPOGRIFF, 'H', BROWN, "hippogriff",
    M_FLIES | M_WARM_BLOOD,
    MR_NO_FLAGS,
    1000, 10, MONS_HIPPOGRIFF, MONS_HIPPOGRIFF, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 10}, {AT_CLAW, AF_PLAIN, 8}, {AT_CLAW, AF_PLAIN, 8}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    2, 7, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_SCREECH, I_ANIMAL, 
    MONUSE_OPEN_DOORS, SIZE_BIG,
    "A large creature with the hindquarters of a horse and the wings, head, and talons of a great eagle."
}
,

{
    MONS_GRIFFON, 'H', YELLOW, "griffon",
    M_FLIES | M_WARM_BLOOD,
    MR_NO_FLAGS,
    1800, 10, MONS_GRIFFON, MONS_GRIFFON, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 18}, {AT_CLAW, AF_PLAIN, 10}, {AT_CLAW, AF_PLAIN, 10}, {AT_NONE, AF_PLAIN, 0} },
    { 12, 3, 5, 0 },
    4, 6, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_SCREECH, I_ANIMAL, 
    MONUSE_OPEN_DOORS, SIZE_BIG,
        "A large creature with the hindquarters of a lion and the wings, head, and talons of a great eagle."
}
,

{
    MONS_HYDRA, 'D', LIGHTGREEN, "hydra",
    M_AMPHIBIOUS,  // because it likes the swamp -- bwr
    MR_RES_POISON,
    1800, 11, MONS_HYDRA, MONS_HYDRA, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 18}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 13, 3, 5, 0 },
    0, 5, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_ROAR, I_REPTILE, 
    MONUSE_OPEN_DOORS, SIZE_BIG,
    "A great reptilian beast, distantly related to the dragon. It has many heads, and the potential to grow many more!"
}
,

// small skeleton
{
    MONS_SKELETON_SMALL, 'z', LIGHTGREY, "",
    M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    0, 10, MONS_SKELETON_SMALL, MONS_SKELETON_SMALL, MH_UNDEAD, -1,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 0, 0, 0, 0 },
    0, 0, 0, 0, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_SMALL,
    "A skeleton compelled to unlife by the exercise of necromancy."
}
,

// large skeleton
{
    MONS_SKELETON_LARGE, 'Z', LIGHTGREY, "",
    M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    0, 10, MONS_SKELETON_SMALL, MONS_SKELETON_LARGE, MH_UNDEAD, -1,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 0, 0, 0, 0 },
    0, 0, 0, 0, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_LARGE,
    "A skeleton compelled to unlife by the exercise of necromancy."
}
,


{
    MONS_HELL_KNIGHT, '@', RED, "hell knight",
    M_FIGHTER | M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD | M_EVIL,
    MR_RES_FIRE,
    550, 10, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 13}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 3, 6, 0 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A heavily armoured warrior, in league with the powers of Hell."
}
,

{
    MONS_NECROMANCER, '@', DARKGREY, "necromancer",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    550, 10, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -4,
    { {AT_HIT, AF_PLAIN, 6}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 2, 4, 0 },
    0, 13, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A wizard specializing in the practices of necromantic magic."
}
,

{
    MONS_WIZARD, '@', MAGENTA, "wizard",
    M_SPELLCASTER | M_SPEAKS | M_ACTUAL_SPELLS | M_WARM_BLOOD,
    MR_RES_ELEC,
    550, 10, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -4,
    { {AT_HIT, AF_PLAIN, 6}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 2, 4, 0 },
    0, 13, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "An rather eccentric person, dabbling in all sorts of arcanities"
}
,

{
    MONS_ORC_PRIEST, 'o', LIGHTGREEN, "orc priest",
    M_SPELLCASTER | M_PRIEST | M_WARM_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    600, 10, MONS_ORC, MONS_ORC, MH_NATURAL, -4,
    { {AT_HIT, AF_PLAIN, 6}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 3, 4, 0 },
    1, 10, 10, 7, MST_ORC_PRIEST, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A servant of the ancient and cruel gods of the orcs, dressed in a long robe. It continually mumbles strange prayers. You should hope that they remain unheard."
}
,

{
    MONS_ORC_HIGH_PRIEST, 'o', GREEN, "orc high priest",
    M_SPELLCASTER | M_SEE_INVIS | M_SPEAKS | M_PRIEST | M_WARM_BLOOD | M_EVIL,
      MR_RES_HELLFIRE,
      600, 10, MONS_ORC, MONS_ORC, MH_NATURAL, -4,
    { {AT_HIT, AF_PLAIN, 7}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 11, 3, 4, 0 },
    1, 12, 10, 7, MST_ORC_HIGH_PRIEST, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "An exalted servant of the orc god."
}
,

// this is a dummy monster, used for corpses
// mv:but it can be generated by polymorph spells and because IMO it's
// logical polymorph target so complete monster statistics should exist.
// Same thing for elf dummy monster.

{
    MONS_HUMAN, '@', LIGHTGRAY, "human",
    M_WARM_BLOOD,
    MR_NO_FLAGS,
    550, 10, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 10}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 3, 5, 0 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A remarkably nondescript person.  How odd!"
}
,

{
    MONS_GNOLL, 'g', YELLOW, "gnoll",
    M_WARM_BLOOD,
    MR_NO_FLAGS,
    750, 10, MONS_GNOLL, MONS_GNOLL, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 9}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 2, 4, 5, 0 },
    2, 9, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A taller and better equipped relative of goblins and orcs, somewhat doglike in appearance."
}
,

{
    MONS_CLAY_GOLEM, '8', BROWN, "clay golem",
    M_SEE_INVIS,
    MR_RES_POISON | MR_RES_FIRE | MR_RES_COLD | MR_RES_ELEC,
    0, 10, MONS_CLAY_GOLEM, MONS_CLAY_GOLEM, MH_NONLIVING, MAG_IMMUNE,
    { {AT_HIT, AF_PLAIN, 11}, {AT_HIT, AF_PLAIN, 11}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 7, 3, 0 },
    7, 5, 8, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "A huge animated clay statue."
}
,

{
    MONS_WOOD_GOLEM, '8', YELLOW, "wood golem",
    M_NO_FLAGS,
    MR_RES_POISON | MR_VUL_FIRE | MR_RES_COLD | MR_RES_ELEC,
    0, 10, MONS_CLAY_GOLEM, MONS_WOOD_GOLEM, MH_NONLIVING, MAG_IMMUNE,
    { {AT_HIT, AF_PLAIN, 10}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 6, 3, 0 },
    5, 6, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "An animated wooden statue."
}
,

{
    MONS_STONE_GOLEM, '8', LIGHTGREY, "stone golem",
    M_NO_FLAGS,
    MR_RES_POISON | MR_RES_FIRE | MR_RES_COLD | MR_RES_ELEC,
    0, 10, MONS_CLAY_GOLEM, MONS_STONE_GOLEM, MH_NONLIVING, MAG_IMMUNE,
    { {AT_HIT, AF_PLAIN, 28}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 12, 7, 4, 0 },
    12, 4, 7, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "An animated stone statue."
}
,

{
    MONS_IRON_GOLEM, '8', CYAN, "iron golem",
    M_SEE_INVIS,
    MR_RES_POISON | MR_RES_FIRE | MR_RES_COLD | MR_RES_ELEC,
    0, 10, MONS_CLAY_GOLEM, MONS_IRON_GOLEM, MH_NONLIVING, MAG_IMMUNE,
    { {AT_HIT, AF_PLAIN, 35}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 15, 7, 4, 0 },
    15, 3, 7, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "An animated metal statue."
}
,

{
    MONS_CRYSTAL_GOLEM, '8', WHITE, "crystal golem",
    M_SEE_INVIS,
    MR_RES_POISON | MR_RES_FIRE | MR_RES_COLD | MR_RES_ELEC,
    0, 10, MONS_CLAY_GOLEM, MONS_CRYSTAL_GOLEM, MH_NONLIVING, MAG_IMMUNE,
    { {AT_HIT, AF_PLAIN, 40}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 13, 7, 4, 0 },
    22, 3, 7, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "An animated crystal statue."
}
,

{
    MONS_TOENAIL_GOLEM, '8', LIGHTGREY, "toenail golem",
    M_NO_FLAGS,
    MR_RES_POISON | MR_RES_FIRE | MR_RES_COLD | MR_RES_ELEC,
    0, 10, MONS_CLAY_GOLEM, MONS_TOENAIL_GOLEM, MH_NONLIVING, MAG_IMMUNE,
    { {AT_HIT, AF_PLAIN, 13}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 9, 5, 3, 0 },
    8, 5, 8, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "A huge animated statue made entirely from toenail clippings. Some people just have too much time on their hands."
}
,

{
    MONS_MOTTLED_DRAGON, 'D', RED, "mottled dragon",
    M_SPELLCASTER | M_FLIES,
    MR_RES_POISON | MR_RES_FIRE,
    1100, 10, MONS_DRAGON, MONS_MOTTLED_DRAGON, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 15}, {AT_CLAW, AF_PLAIN, 6}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 3, 5, 0 },
    5, 10, 10, 7, MST_MOTTLED_DRAGON, CE_POISONOUS, Z_BIG, S_SILENT, I_ANIMAL_LIKE, 
    MONUSE_OPEN_DOORS, SIZE_GIANT,
    "A small dragon with strangely mottled scales."
}
,

{
    MONS_EARTH_ELEMENTAL, '#', BROWN, "earth elemental",
    M_NO_FLAGS,
    MR_RES_POISON | MR_RES_FIRE | MR_RES_COLD | MR_RES_ELEC,
    0, 10, MONS_EARTH_ELEMENTAL, MONS_EARTH_ELEMENTAL, MH_NONLIVING, MAG_IMMUNE,
    { {AT_HIT, AF_PLAIN, 40}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 5, 5, 0 },
    14, 4, 6, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS, SIZE_BIG,
    "A spirit drawn from the elemental plane of earth, which exists in this world by inhabiting a lump of earth and rocks."
}
,

{
    MONS_FIRE_ELEMENTAL, '#', YELLOW, "fire elemental",
    M_FLIES,
    MR_RES_POISON | MR_RES_HELLFIRE | MR_VUL_COLD | MR_RES_ELEC,
    0, 10, MONS_EARTH_ELEMENTAL, MONS_FIRE_ELEMENTAL, MH_NONLIVING, MAG_IMMUNE,
    { {AT_HIT, AF_FIRE, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 5, 0 },
    4, 12, 13, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS, SIZE_BIG,
    "A spirit drawn from the elemental plane of fire, which exists in this world as a brilliant column of raging flames."
}
,

{
    MONS_AIR_ELEMENTAL, 'v', LIGHTGREY, "air elemental",
    M_LEVITATE | M_SEE_INVIS | M_FLIES,
    MR_RES_ELEC | MR_RES_POISON,
    0, 5, MONS_EARTH_ELEMENTAL, MONS_AIR_ELEMENTAL, MH_NONLIVING, MAG_IMMUNE,
    { {AT_HIT, AF_PLAIN, 15}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 5, 0 },
    2, 18, 25, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS, SIZE_BIG,
    "A spirit drawn from the elemental plane of air. It exists in this world as a swirling vortex of air, often dissipating and reforming."
}
,

// water elementals are later (with the other water monsters)

{
    MONS_ICE_FIEND, '1', WHITE, "Ice Fiend",
    M_SPELLCASTER | M_FLIES | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON | MR_VUL_FIRE | MR_RES_COLD,
    0, 10, MONS_FIEND, MONS_ICE_FIEND, MH_DEMONIC, -12,
    { {AT_CLAW, AF_COLD, 25}, {AT_CLAW, AF_COLD, 25}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 18, 3, 5, 0 },
    15, 6, 10, 7, MST_ICE_FIEND, CE_CONTAMINATED, Z_NOZOMBIE, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "One of the most terrible denizens of the many Hells, the Ice Fiend is a huge icy figure, covered in frost and wreathed in freezing air."
}
,

{
    MONS_SHADOW_FIEND, '1', DARKGREY, "Shadow Fiend",
    M_SPELLCASTER | M_LEVITATE | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_COLD | MR_RES_ELEC,
    0, 10, MONS_FIEND, MONS_SHADOW_FIEND, MH_DEMONIC, -13,
    { {AT_HIT, AF_DRAIN_XP, 25}, {AT_HIT, AF_DRAIN_XP, 15}, {AT_HIT, AF_DRAIN_XP, 15}, {AT_NONE, AF_PLAIN, 0} },
    { 18, 3, 5, 0 },
    15, 6, 10, 7, MST_SHADOW_FIEND, CE_CONTAMINATED, Z_NOZOMBIE, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "One of the most terrible denizens of the many Hells, this horrible being appears as a great mass of writhing shadows which occasionally reveal a huge, horned skeleton."
}
,

{
    MONS_BROWN_SNAKE, 'S', BROWN, "brown snake",
    M_COLD_BLOOD | M_AMPHIBIOUS,
    MR_RES_POISON,
    300, 10, MONS_SNAKE, MONS_BROWN_SNAKE, MH_NATURAL, -3,
    { {AT_BITE, AF_POISON_MEDIUM, 10}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 4, 3, 5, 0 },
    2, 15, 14, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_HISS, I_REPTILE, 
    MONUSE_NOTHING, SIZE_MEDIUM,
    "A large brown snake."
}
,

{
    MONS_GIANT_LIZARD, 'l', GREEN, "giant lizard",
    M_COLD_BLOOD,
    MR_NO_FLAGS,
    600, 10, MONS_GIANT_LIZARD, MONS_GIANT_LIZARD, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 3, 5, 0 },
    4, 10, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_REPTILE, 
    MONUSE_NOTHING, SIZE_LITTLE,
    "A huge lizard with great crunching jaws."
}
,

{
    MONS_SPECTRAL_WARRIOR, 'W', LIGHTGREEN, "spectral warrior",
    M_LEVITATE | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    0, 13, MONS_WRAITH, MONS_SPECTRAL_WARRIOR, MH_UNDEAD, -6,
    { {AT_HIT, AF_DRAIN_XP, 18}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 9, 3, 5, 0 },
    12, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A hideous translucent green undead spirit."
}
,

{
    MONS_PULSATING_LUMP, 'J', RED, "pulsating lump",
    M_SENSE_INVIS,
    MR_RES_POISON | MR_RES_ASPHYX,
    0, 3, MONS_JELLY, MONS_PULSATING_LUMP, MH_NATURAL, -3,
    { {AT_HIT, AF_MUTATE, 13}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 3, 5, 0 },
    2, 6, 5, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_LITTLE,
    "A revolting glob of writhing flesh."
}
,

{
    MONS_STORM_DRAGON, 'D', LIGHTBLUE, "storm dragon",
    M_SPELLCASTER | M_FLIES,
    MR_RES_ELEC | MR_RES_COLD,
    2800, 12, MONS_DRAGON, MONS_STORM_DRAGON, MH_NATURAL, -5,
    { {AT_BITE, AF_PLAIN, 25}, {AT_CLAW, AF_PLAIN, 15}, {AT_CLAW, AF_PLAIN, 15}, {AT_NONE, AF_PLAIN, 0} },
    { 14, 5, 5, 0 },
    13, 10, 12, 7, MST_STORM_DRAGON, CE_CLEAN, Z_BIG, S_ROAR, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_HUGE,
    "A huge and very powerful dragon. Sparks crackle along its enormous scaly wings."
}
,

{
    MONS_YAKTAUR, 'c', LIGHTRED, "yaktaur",
    M_WARM_BLOOD,
    MR_NO_FLAGS,
    2000, 10, MONS_YAKTAUR, MONS_YAKTAUR, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 15}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 5, 0 },
    4, 4, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_BIG,
    "Like a centaur, but half yak."
}
,

{
    MONS_DEATH_YAK, 'Y', DARKGREY, "death yak",
    M_WARM_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    1500, 10, MONS_YAK, MONS_DEATH_YAK, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 30}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 14, 3, 5, 0 },
    9, 5, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_BIG, S_BELLOW, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_BIG,
    "A larger and beefier relative of the common dungeon yak. Its little red eyes gleam with hunger for living flesh."
}
,

{
    MONS_ROCK_TROLL, 'T', LIGHTGREY, "rock troll",
    M_WARM_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    2200, 11, MONS_TROLL, MONS_ROCK_TROLL, MH_NATURAL, -4,
    { {AT_BITE, AF_PLAIN, 30}, {AT_CLAW, AF_PLAIN, 20}, {AT_CLAW, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0} },
    { 11, 3, 5, 0 },
    13, 6, 8, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "An enormous and very nasty-looking humanoid creature. Its rocky hide seems to heal almost instantaneously from most wounds."
}
,

{
    MONS_STONE_GIANT, 'C', LIGHTGREY, "stone giant",
    M_WARM_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    3000, 10, MONS_HILL_GIANT, MONS_STONE_GIANT, MH_NATURAL, -4,
    { {AT_HIT, AF_PLAIN, 45}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 16, 3, 5, 0 },
    12, 2, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_STARTING_EQUIPMENT, SIZE_GIANT,
    "A gigantic humanoid with grey skin almost as hard as rock. It carries several boulders - are you up for a game of 'catch'?"
}
,

{
    MONS_FLAYED_GHOST, 'p', RED, "flayed ghost",
    M_FLIES | M_EVIL,
    MR_RES_POISON,
    0, 10, MONS_PHANTOM, MONS_FLAYED_GHOST, MH_UNDEAD, -4,
    { {AT_HIT, AF_PLAIN, 30}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 11, 3, 5, 0 },
    0, 14, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A hideous undead creature, with torn skin hanging from an emaciated body."
}
,

{
    MONS_BUMBLEBEE, 'k', RED, "bumblebee",
    M_FLIES,
    MR_VUL_POISON,
    300, 10, MONS_KILLER_BEE, MONS_BUMBLEBEE, MH_NATURAL, -3,
    { {AT_STING, AF_POISON_MEDIUM, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    4, 15, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_BUZZ, I_INSECT, 
    MONUSE_NOTHING, SIZE_TINY,
    "A very large and fat hairy bee."
}
,

{
    MONS_REDBACK, 's', RED, "redback",
    M_NO_FLAGS,
    MR_VUL_POISON,
    1000, 14, MONS_WOLF_SPIDER, MONS_REDBACK, MH_NATURAL, -3,
    { {AT_BITE, AF_POISON_STRONG, 18}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 5, 0 },
    2, 12, 15, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_TINY,
    "A vicious black spider with a splash of red on its swollen abdomen. Its mandibles drip with lethal poison."
}
,

{
    MONS_INSUBSTANTIAL_WISP, 'p', LIGHTGREY, "insubstantial wisp",
    M_LEVITATE | M_SPECIAL_ABILITY,
    MR_RES_ELEC | MR_RES_POISON | MR_RES_FIRE | MR_RES_COLD,
    0, 17, MONS_INSUBSTANTIAL_WISP, MONS_INSUBSTANTIAL_WISP, MH_NONLIVING, MAG_IMMUNE,
    { {AT_HIT, AF_BLINK, 12}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 1, 2, 0 },
    20, 20, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_MOAN, I_PLANT, 
    MONUSE_NOTHING, SIZE_TINY,
    "A thin wisp of floating gas."
}
,

{
    MONS_VAPOUR, '#', LIGHTGREY, "vapour",
    M_SPELLCASTER | M_LEVITATE | M_SEE_INVIS | M_INVIS | M_CONFUSED,
    MR_RES_ELEC | MR_RES_POISON,
    0, 21, MONS_VAPOUR, MONS_VAPOUR, MH_NONLIVING, MAG_IMMUNE,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 12, 2, 3, 0 },
    0, 12, 10, 7, MST_STORM_DRAGON, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_LITTLE,
    "A normally invisible cloud of weird-looking vapour."
}
,

{
    MONS_OGRE_MAGE, 'O', MAGENTA, "ogre-mage",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SEE_INVIS | M_WARM_BLOOD | M_EVIL,
    MR_RES_ELEC,
    0, 16, MONS_OGRE, MONS_OGRE, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 3, 5, 0 },
    1, 7, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_LARGE,
    "A rare breed of ogre, skilled in the use of magic."
}
,

{
    MONS_SPINY_WORM, 'w', DARKGREY, "spiny worm",
    M_NO_FLAGS,
    MR_VUL_POISON | MR_RES_ACID,
    1300, 13, MONS_WORM, MONS_SPINY_WORM, MH_NATURAL, -3,
    { {AT_STING, AF_ACID, 32}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 12, 3, 5, 0 },
    10, 6, 9, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_BIG,
    "A great black worm, its many-segmented body covered in spiky plates of chitinous armour. Acidic venom drips from its toothy maw."
}
,

// these are named more explicitly when they attack, also when you use 'x'
//  to examine them.
{
    MONS_DANCING_WEAPON, '(', BLACK, "dancing weapon",
    M_FIGHTER | M_LEVITATE,
    MR_RES_POISON | MR_RES_HELLFIRE | MR_RES_COLD | MR_RES_ELEC,
    0, 10, MONS_DANCING_WEAPON, MONS_DANCING_WEAPON, MH_NONLIVING, MAG_IMMUNE,
    { {AT_HIT, AF_PLAIN, 30}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 15, 0, 0, 15 },
    10, 20, 15, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_SMALL,
    "A weapon dancing in the air."
}
,

{
    MONS_TITAN, 'C', MAGENTA, "titan",
    M_FIGHTER | M_SPELLCASTER | M_WARM_BLOOD | M_SENSE_INVIS | M_EVIL,
    MR_RES_ELEC,
    3500, 12, MONS_HILL_GIANT, MONS_TITAN, MH_NATURAL, -7,
    { {AT_HIT, AF_PLAIN, 55}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 20, 3, 5, 0 },
    10, 3, 10, 7, MST_TITAN, CE_CLEAN, Z_BIG, S_SHOUT, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_GIANT,
    "This lightning-limned humanoid is unusually large and powerful, even among giants."
}
,

{
    MONS_GOLDEN_DRAGON, 'D', YELLOW, "golden dragon",
    M_SPELLCASTER | M_FLIES | M_SENSE_INVIS,
    MR_RES_ELEC | MR_RES_POISON | MR_RES_FIRE | MR_RES_COLD,
    3000, 17, MONS_DRAGON, MONS_GOLDEN_DRAGON, MH_NATURAL, -8,
    { {AT_BITE, AF_PLAIN, 40}, {AT_CLAW, AF_PLAIN, 20}, {AT_CLAW, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0} },
    { 18, 4, 4, 0 },
    15, 7, 10, 7, MST_GOLDEN_DRAGON, CE_POISONOUS, Z_BIG, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_HUGE,
    "A great dragon covered in shining golden scales."
}
,

// 147 - dummy monster, used for corpses etc.
//mv: have to exist because it's (and should be) valid polymorph target.
{
    MONS_ELF, 'e', DARKGREY, "elf",
    M_WARM_BLOOD,
    MR_NO_FLAGS,
    450, 10, MONS_ELF, MONS_ELF, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 10}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 3, 3, 0 },
    0, 12, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SILENT, I_NORMAL,
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "One of the race of elves which inhabits this dreary cave. "
}
,


// Used to be "lindworm" and a GREEN 'l'...  I'm hoping that by 
// making it a 'd' and using an alternate spelling people will 
// more intuitively know that this isn't a regular lizard. -- bwr
{
    MONS_LINDWURM, 'd', LIGHTGREEN, "lindwurm",
    M_SPECIAL_ABILITY,
    MR_NO_FLAGS,
    1000, 11, MONS_DRAGON, MONS_LINDWURM, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 20}, {AT_CLAW, AF_PLAIN, 10}, {AT_CLAW, AF_PLAIN, 10}, {AT_NONE, AF_PLAIN, 0} },
    { 9, 3, 5, 0 },
    8, 6, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_ROAR, I_REPTILE, 
    MONUSE_NOTHING, SIZE_GIANT,
    "A small serpentine dragon with a pair of strong forelimbs.  Its thick scales give off an eerie green glow."
}
,

{
    MONS_ELEPHANT_SLUG, 'm', LIGHTGREY, "elephant slug",
    M_NO_SKELETON,
    MR_VUL_POISON,
    1500, 10, MONS_GIANT_SLUG, MONS_ELEPHANT_SLUG, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 40}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 20, 5, 3, 0 },
    2, 1, 4, 10, MST_NO_SPELLS, CE_POISONOUS, Z_BIG, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_BIG,
    "A huge grey slug with folds of wrinkled skin."
}
,

{
    MONS_WAR_DOG, 'h', CYAN, "war dog",
    M_SENSE_INVIS | M_WARM_BLOOD,
    MR_NO_FLAGS,
    350, 10, MONS_HOUND, MONS_WAR_DOG, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 4, 3, 5, 0 },
    4, 15, 17, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_BARK, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_MEDIUM,
    "A vicious dog, trained to kill.  Its neck is protected by massive spiked collar."
}
,

{
    MONS_GREY_RAT, 'r', LIGHTGREY, "grey rat",
    M_WARM_BLOOD,
    MR_NO_FLAGS,
    250, 10, MONS_RAT, MONS_GREY_RAT, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 3, 6, 0 },
    2, 12, 12, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SILENT, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_TINY,
    "A very large grey rat."
}
,

{
    MONS_GREEN_RAT, 'r', LIGHTGREEN, "green rat",
    M_WARM_BLOOD,
    MR_NO_FLAGS,
    250, 10, MONS_RAT, MONS_GREEN_RAT, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 10}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 2, 3, 5, 0 },
    5, 11, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_TINY,
    "A very large rat, with hair and skin of a most peculiar green colour."
}
,

{
    MONS_ORANGE_RAT, 'r', LIGHTRED, "orange rat",
    M_WARM_BLOOD,
    MR_NO_FLAGS,
    250, 10, MONS_RAT, MONS_ORANGE_RAT, MH_NATURAL, -3,
    { {AT_BITE, AF_DRAIN_XP, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 3, 5, 0 },
    7, 10, 12, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_ROAR, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_LITTLE,
    "A huge rat, with weird knobbly orange skin.  It glows with unholy energies."
}
,

{
    MONS_BLACK_SNAKE, 'S', DARKGREY, "black snake",
    M_COLD_BLOOD,
    MR_RES_POISON,
    500, 12, MONS_SNAKE, MONS_BLACK_SNAKE, MH_NATURAL, -3,
    { {AT_BITE, AF_POISON_MEDIUM, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    4, 15, 18, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_HISS, I_REPTILE, 
    MONUSE_NOTHING, SIZE_MEDIUM,
    "A large black snake."
}
,

{
    MONS_SHEEP, 'Y', LIGHTGREY, "sheep",
    M_WARM_BLOOD,
    MR_NO_FLAGS,
    1200, 10, MONS_SHEEP, MONS_SHEEP, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 13}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 3, 5, 0 },
    2, 7, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_BELLOW, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_SMALL,
    "A stupid woolly animal, with murder in its eyes.  It looks surprised."
}
,

{
    MONS_GHOUL, 'n', RED, "ghoul",
    M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    500, 12, MONS_GHOUL, MONS_GHOUL, MH_UNDEAD, -5,
    { {AT_CLAW, AF_ROT, 9}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 4, 3, 5, 0 },
    4, 10, 10, 7, MST_NO_SPELLS, CE_HCL, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "An undead humanoid creature created from the decaying corpse by some unholy means of necromancy. It exists to spread disease and decay, and gains power from the decaying corpses same way as necrophage does."
}
,

{
    MONS_HOG, 'h', LIGHTRED, "hog",
    M_WARM_BLOOD,
    MR_NO_FLAGS,
    700, 10, MONS_HOG, MONS_HOG, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 14}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 5, 0 },
    2, 9, 13, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_SMALL,
    "A large, fat and very ugly pig."
}
,

{
    MONS_GIANT_MOSQUITO, 'y', DARKGREY, "giant mosquito",
    M_FLIES,
    MR_VUL_POISON,
    100, 10, MONS_GIANT_MOSQUITO, MONS_GIANT_MOSQUITO, MH_NATURAL, -3,
    { {AT_BITE, AF_DISEASE, 10}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 3, 5, 0 },
    0, 13, 12, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_WHINE, I_PLANT, 
    MONUSE_NOTHING, SIZE_TINY,
    "A huge, bloated mosquito. It looks diseased."
}
,

{
    MONS_GIANT_CENTIPEDE, 's', GREEN, "giant centipede",
    M_NO_FLAGS,
    MR_VUL_POISON,
    350, 10, MONS_GIANT_CENTIPEDE, MONS_GIANT_CENTIPEDE, MH_NATURAL, -3,
    { {AT_STING, AF_POISON_NASTY, 2}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 2, 3, 3, 0 },
    2, 14, 13, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_TINY,
    "It has a lot of legs."
}
,

{
    MONS_IRON_TROLL, 'T', CYAN, "iron troll",
    M_WARM_BLOOD | M_EVIL,
    MR_RES_FIRE | MR_RES_COLD,
    2400, 10, MONS_TROLL, MONS_IRON_TROLL, MH_NATURAL, -5,
    { {AT_BITE, AF_PLAIN, 35}, {AT_CLAW, AF_PLAIN, 25}, {AT_CLAW, AF_PLAIN, 25}, {AT_NONE, AF_PLAIN, 0} },
    { 16, 3, 5, 0 },
    20, 4, 7, 7, MST_NO_SPELLS, CE_POISONOUS, Z_BIG, S_ROAR, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "A great troll, plated with thick scales of rusty iron."
}
,

{
    MONS_NAGA, 'N', GREEN, "naga",
    M_SPELLCASTER | M_SEE_INVIS | M_WARM_BLOOD,
    MR_RES_POISON,
    750, 10, MONS_NAGA, MONS_NAGA, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 13}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 3, 5, 0 },
    6, 10, 8, 7, MST_NAGA, CE_POISONOUS, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_BIG,
    "A hybrid; human from the chest up, with a scaly, muscular torso trailing off like that of a snake. "
}
,

{
    MONS_FIRE_GIANT, 'C', RED, "fire giant",
    M_FIGHTER | M_SPELLCASTER | M_WARM_BLOOD | M_SENSE_INVIS | M_EVIL,
    MR_RES_FIRE,
    2400, 11, MONS_HILL_GIANT, MONS_FIRE_GIANT, MH_NATURAL, -4,
    { {AT_HIT, AF_PLAIN, 30}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 16, 3, 6, 0 },
    8, 4, 10, 7, MST_EFREET, CE_CONTAMINATED, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_GIANT,
    "A huge ruddy humanoid with bright hair. "
}
,

{
    MONS_FROST_GIANT, 'C', LIGHTBLUE, "frost giant",
    M_FIGHTER | M_SPELLCASTER | M_WARM_BLOOD | M_SENSE_INVIS | M_EVIL,
    MR_RES_COLD,
    2600, 11, MONS_HILL_GIANT, MONS_FROST_GIANT, MH_NATURAL, -4,
    { {AT_HIT, AF_PLAIN, 35}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 16, 4, 5, 0 },
    9, 3, 10, 7, MST_FROST_GIANT, CE_CONTAMINATED, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_GIANT,
    "A huge blue humanoid with hoarfrost hair."
}
,

{
    MONS_FIREDRAKE, 'l', RED, "firedrake",
    M_FLIES | M_SPECIAL_ABILITY,
    MR_RES_FIRE,
    900, 10, MONS_FIREDRAKE, MONS_FIREDRAKE, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 8}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 5, 0 },
    3, 12, 12, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SILENT, I_ANIMAL, 
    MONUSE_OPEN_DOORS, SIZE_HUGE,
    "A small dragon, puffing clouds of smoke."
}
,

{
    MONS_SHADOW_DRAGON, 'D', DARKGREY, "shadow dragon",
    M_SPELLCASTER | M_FLIES | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    2000, 12, MONS_DRAGON, MONS_SHADOW_DRAGON, MH_NATURAL, -5,
    { {AT_BITE, AF_DRAIN_XP, 20}, {AT_CLAW, AF_PLAIN, 15}, {AT_CLAW, AF_PLAIN, 15}, {AT_NONE, AF_PLAIN, 0} },
    { 17, 5, 5, 0 },
    15, 10, 10, 7, MST_SHADOW_DRAGON, CE_CLEAN, Z_BIG, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_HUGE,
    "A great shadowy shape, radiating evil and death."
}
,
{
    MONS_YELLOW_SNAKE, 'S', YELLOW, "yellow snake",
    M_COLD_BLOOD,
    MR_RES_POISON,
    400, 10, MONS_SNAKE, MONS_YELLOW_SNAKE, MH_NATURAL, -3,
    { {AT_BITE, AF_POISON_MEDIUM, 15}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 5, 0 },
    4, 14, 13, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_HISS, I_REPTILE, 
    MONUSE_NOTHING, SIZE_MEDIUM,
    "A large yellow tubular reptile."
}
,

{
    MONS_GREY_SNAKE, 'S', LIGHTGREY, "grey snake",
    M_COLD_BLOOD,
    MR_NO_FLAGS,
    600, 10, MONS_SNAKE, MONS_GREY_SNAKE, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 30}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 11, 3, 5, 0 },
    4, 16, 18, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_HISS, I_REPTILE, 
    MONUSE_NOTHING, SIZE_LARGE,
    "A very large grey python."
}
,

{
    MONS_DEEP_TROLL, 'T', DARKGREY, "deep troll",
    M_WARM_BLOOD | M_SENSE_INVIS | M_EVIL,
    MR_NO_FLAGS,
    1500, 12, MONS_TROLL, MONS_DEEP_TROLL, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 27}, {AT_CLAW, AF_PLAIN, 20}, {AT_CLAW, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 3, 5, 0 },
    6, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_BIG,
    "A stooped troll."
}
,

{
    MONS_GIANT_BLOWFLY, 'y', LIGHTGREY, "giant blowfly",
    M_FLIES,
    MR_VUL_POISON,
    200, 10, MONS_GIANT_BLOWFLY, MONS_GIANT_BLOWFLY, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 13}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 3, 5, 0 },
    2, 15, 19, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_BUZZ, I_PLANT, 
    MONUSE_NOTHING, SIZE_TINY,
    "A huge and irritating fly."
}
,

{
    MONS_RED_WASP, 'y', RED, "red wasp",
    M_FLIES,
    MR_VUL_POISON,
    400, 14, MONS_YELLOW_WASP, MONS_RED_WASP, MH_NATURAL, -3,
    { {AT_STING, AF_PARALYSE, 23}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 5, 0 },
    7, 14, 15, 7, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_BUZZ, I_PLANT, 
    MONUSE_NOTHING, SIZE_TINY,
    "A huge red wasp with a viciously barbed stinger."
}
,

{
    MONS_SWAMP_DRAGON, 'D', BROWN, "swamp dragon",
    M_SPELLCASTER | M_FLIES,
    MR_RES_POISON,
    1900, 11, MONS_DRAGON, MONS_SWAMP_DRAGON, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 18}, {AT_CLAW, AF_PLAIN, 9}, {AT_CLAW, AF_PLAIN, 9}, {AT_NONE, AF_PLAIN, 0} },
    { 9, 5, 5, 0 },
    7, 7, 10, 7, MST_SWAMP_DRAGON, CE_CONTAMINATED, Z_BIG, S_ROAR, I_ANIMAL_LIKE, 
    MONUSE_OPEN_DOORS, SIZE_GIANT,
    "A slimy dragon, covered in swamp muck. Poisonous gasses dribble from its snout."
}
,

{
    MONS_SWAMP_DRAKE, 'l', BROWN, "swamp drake",
    M_SPELLCASTER | M_FLIES | M_EVIL,
    MR_RES_POISON,
    900, 11, MONS_DRAGON, MONS_SWAMP_DRAKE, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 14}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 4, 5, 5, 0 },
    3, 11, 11, 7, MST_SWAMP_DRAKE, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_ANIMAL_LIKE, 
    MONUSE_OPEN_DOORS, SIZE_BIG,
    "A small and slimy dragon, covered in swamp muck. "
}
,

{
    MONS_DEATH_DRAKE, 'l', DARKGREY, "death drake",
    M_SPELLCASTER | M_FLIES | M_EVIL,
    MR_RES_POISON,
    900, 11, MONS_DRAGON, MONS_DEATH_DRAKE, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 9, 5, 7, 0 },
    6, 14, 13, 10, MST_DEATH_DRAKE, CE_HCL, Z_BIG, S_ROAR, I_ANIMAL_LIKE,
    MONUSE_OPEN_DOORS, SIZE_BIG,
    "A small dragon, radiating evil."
}
,

{
    MONS_SOLDIER_ANT, 'a', LIGHTGREY, "soldier ant",
    M_NO_FLAGS,
    MR_VUL_POISON,
    900, 10, MONS_GIANT_ANT, MONS_SOLDIER_ANT, MH_NATURAL, -3,
    { {AT_STING, AF_POISON_NASTY, 14}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 5, 0 },
    8, 10, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_LITTLE,
    "A giant ant with large mandibles and a vicious sting."
}
,

{
    MONS_HILL_GIANT, 'C', LIGHTRED, "hill giant",
    M_WARM_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    1600, 10, MONS_HILL_GIANT, MONS_HILL_GIANT, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 30}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 11, 3, 5, 0 },
    3, 4, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_GIANT,
    "Although one of the smaller giant varieties, this hill giant is still big enough to be dangerous."
}
,

{
    MONS_QUEEN_ANT, 'Q', DARKGREY, "queen ant",
    M_NO_FLAGS,
    MR_VUL_POISON,
    1200, 10, MONS_GIANT_ANT, MONS_QUEEN_ANT, MH_NATURAL, -3,
    { {AT_STING, AF_POISON_NASTY, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 13, 3, 5, 0 },
    14, 3, 7, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_SMALL,
    "A bloated insect, covered in thick chitinous armour.  Now you know where all those ants keep coming from!"
}
,

{
    MONS_ANT_LARVA, 'w', LIGHTGREY, "ant larva",
    M_NO_SKELETON,
    MR_VUL_POISON,
    350, 5, MONS_GIANT_ANT, MONS_ANT_LARVA, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 2, 3, 5, 0 },
    2, 6, 6, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_TINY,
    "A baby ant. Isn't it cute?"
}
,


{
    MONS_GIANT_FROG, 'F', GREEN, "giant frog",
    M_COLD_BLOOD | M_AMPHIBIOUS,
    MR_NO_FLAGS,
    500, 10, MONS_GIANT_FROG, MONS_GIANT_FROG, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 9}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 4, 3, 5, 0 },
    0, 12, 15, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_CROAK, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_SMALL,
    "It probably didn't get this big by eating little insects."
}
,

{
    MONS_GIANT_BROWN_FROG, 'F', BROWN, "giant brown frog",
    M_COLD_BLOOD | M_AMPHIBIOUS,
    MR_NO_FLAGS,
    890, 10, MONS_GIANT_FROG, MONS_GIANT_BROWN_FROG, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 14}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 5, 0 },
    2, 11, 13, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_CROAK, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_MEDIUM,
    "A very large and vicious-looking carnivorous frog. Its knobbly brown skin blends in with the rough rock of your surroundings."
}
,

{
    MONS_SPINY_FROG, 'F', YELLOW, "spiny frog",
    M_COLD_BLOOD | M_AMPHIBIOUS,
    MR_RES_POISON,
    1000, 10, MONS_GIANT_FROG, MONS_SPINY_FROG, MH_NATURAL, -3,
    { {AT_STING, AF_POISON_MEDIUM, 26}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    6, 9, 12, 7, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_CROAK, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_SMALL,
    "Although slightly smaller than its cousin, the giant brown frog, the spiny frog makes up for lack of size by being covered in wickedly barbed spines and spurs."
}
,

{
    MONS_BLINK_FROG, 'F', LIGHTGREEN, "blink frog",
    M_COLD_BLOOD | M_AMPHIBIOUS | M_SPECIAL_ABILITY,
    MR_NO_FLAGS,
    800, 12, MONS_GIANT_FROG, MONS_BLINK_FROG, MH_NATURAL, -5,
    { {AT_HIT, AF_BLINK, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 5, 0 },
    3, 12, 14, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_CROAK, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_LITTLE,
    "A weird-looking frog, constantly blinking in and out of reality."
}
,
{
    MONS_GIANT_COCKROACH, 'a', BROWN, "giant cockroach",
    M_NO_FLAGS,
    MR_NO_FLAGS,
    250, 10, MONS_GIANT_COCKROACH, MONS_GIANT_COCKROACH, MH_NATURAL, -1,
    { {AT_BITE, AF_PLAIN, 2}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 3, 4, 0 },
    3, 10, 12, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_TINY,
    "A large brown cockroach.  I call him 'Gregor'."
}
,
{
    MONS_SMALL_SNAKE, 'S', LIGHTGREEN, "small snake",
    M_COLD_BLOOD,
    MR_NO_FLAGS,
    100, 13, MONS_SNAKE, MONS_SMALL_SNAKE, MH_NATURAL, -1,
    { {AT_BITE, AF_POISON, 2}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 2, 3, 0 },
    0, 11, 12, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_REPTILE, 
    MONUSE_NOTHING, SIZE_LITTLE,
    "The lesser dungeon snake."
}
,

{
    MONS_WHITE_IMP, '5', WHITE, "white imp",
    M_SPELLCASTER | M_FLIES | M_SPEAKS | M_EVIL,
    MR_RES_COLD,
    0, 10, MONS_IMP, MONS_WHITE_IMP, MH_DEMONIC, -3,
    { {AT_HIT, AF_COLD, 4}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 2, 3, 5, 0 },
    4, 10, 10, 7, MST_WHITE_IMP, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_LITTLE,
    "A small and mischievous minor demon."
}
,

{
    MONS_LEMURE, '5', YELLOW, "lemure",
    M_EVIL,
    MR_RES_POISON,
    0, 10, MONS_LEMURE, MONS_LEMURE, MH_DEMONIC, -3,
    { {AT_HIT, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 2, 3, 5, 0 },
    1, 12, 12, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_MOAN, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_SMALL,
    "A vaguely humanoid blob of putrid white flesh. "
}
,

{
    MONS_UFETUBUS, '5', LIGHTCYAN, "ufetubus",
    M_EVIL,
    MR_VUL_FIRE | MR_RES_COLD,
    0, 10, MONS_UFETUBUS, MONS_UFETUBUS, MH_DEMONIC, -3,
    { {AT_HIT, AF_PLAIN, 5}, {AT_HIT, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 4, 6, 0 },
    2, 15, 15, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A chattering and shrieking minor demon."
}
,

{
    MONS_MANES, '5', LIGHTRED, "manes",
    M_EVIL,
    MR_RES_ELEC | MR_RES_FIRE | MR_RES_COLD | MR_RES_POISON,
    0, 10, MONS_MANES, MONS_MANES, MH_DEMONIC, -3,
    { {AT_HIT, AF_PLAIN, 5}, {AT_HIT, AF_PLAIN, 3}, {AT_HIT, AF_PLAIN, 3}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 3, 5, 0 },
    2, 8, 8, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_SMALL,
    "An ugly, twisted little minor demon."
}
,

{
    MONS_MIDGE, '5', LIGHTGREEN, "midge",
    M_FLIES | M_EVIL,
    MR_RES_POISON,
    0, 10, MONS_MIDGE, MONS_MIDGE, MH_DEMONIC, -3,
    { {AT_HIT, AF_BLINK, 8}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 2, 3, 5, 0 },
    4, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_SMALL,
    "A small flying demon."
}
,

{
    MONS_NEQOXEC, '3', MAGENTA, "neqoxec",
    M_SPELLCASTER | M_LEVITATE | M_EVIL,
    MR_RES_POISON,
    0, 12, MONS_NEQOXEC, MONS_NEQOXEC, MH_DEMONIC, -6,
    { {AT_HIT, AF_PLAIN, 15}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 5, 0 },
    4, 12, 10, 7, MST_NEQOXEC, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A weirdly shaped demon."
}
,

{
    MONS_ORANGE_DEMON, '3', LIGHTRED, "orange demon",
    M_EVIL,
    MR_NO_FLAGS,
    0, 12, MONS_ORANGE_DEMON, MONS_ORANGE_DEMON, MH_DEMONIC, -6,
    { {AT_STING, AF_POISON_STR, 10}, {AT_HIT, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 4, 5, 0 },
    3, 7, 7, 7, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_SCREECH, I_NORMAL, 
    MONUSE_NOTHING, SIZE_MEDIUM,
    "A bright orange demon with a venomous stinger."
}
,

{
    MONS_HELLWING, '3', LIGHTGREY, "hellwing",
    M_SPELLCASTER | M_FLIES | M_EVIL,
    MR_RES_POISON,
    0, 12, MONS_HELLWING, MONS_HELLWING, MH_DEMONIC, -6,
    { {AT_HIT, AF_PLAIN, 17}, {AT_HIT, AF_PLAIN, 10}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 4, 5, 0 },
    8, 10, 10, 7, MST_HELLWING, CE_CONTAMINATED, Z_NOZOMBIE, S_MOAN, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "A hideous skeletal demon, with wings of ancient withered skin."
}
,

{
    MONS_SMOKE_DEMON, '4', LIGHTGREY, "smoke demon",
    M_SPELLCASTER | M_FLIES | M_EVIL,
    MR_RES_POISON | MR_RES_FIRE,
    0, 12, MONS_SMOKE_DEMON, MONS_SMOKE_DEMON, MH_DEMONIC, -6,
    { {AT_HIT, AF_PLAIN, 8}, {AT_HIT, AF_PLAIN, 5}, {AT_HIT, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    5, 9, 9, 7, MST_SMOKE_DEMON, CE_CONTAMINATED, Z_NOZOMBIE, S_ROAR, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_SMALL,
    "A writhing cloud of smoke hanging in the air. "
}
,

{
    MONS_YNOXINUL, '3', CYAN, "ynoxinul",
    M_SPELLCASTER | M_FLIES | M_SENSE_INVIS | M_EVIL,
    MR_RES_ELEC | MR_RES_POISON | MR_RES_COLD,
    0, 12, MONS_YNOXINUL, MONS_YNOXINUL, MH_DEMONIC, -6,
    { {AT_HIT, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 5, 0 },
    3, 10, 10, 7, MST_YNOXINUL, CE_CONTAMINATED, Z_NOZOMBIE, S_BELLOW, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A demon with shiny metallic scales. "
}
,

{
    MONS_EXECUTIONER, '1', LIGHTGREY, "Executioner",
    M_FIGHTER | M_SPELLCASTER | M_SEE_INVIS | M_EVIL,
    MR_RES_ELEC | MR_RES_FIRE | MR_RES_COLD | MR_RES_POISON,
    0, 14, MONS_EXECUTIONER, MONS_EXECUTIONER, MH_DEMONIC, -9,
    { {AT_HIT, AF_PLAIN, 30}, {AT_HIT, AF_PLAIN, 10}, {AT_HIT, AF_PLAIN, 10}, {AT_NONE, AF_PLAIN, 0} },
    { 12, 3, 5, 0 },
    10, 15, 20, 7, MST_HELL_KNIGHT_I, CE_CONTAMINATED, Z_NOZOMBIE, S_SCREAM, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "A horribly powerful demon. "
}
,

{
    MONS_GREEN_DEATH, '1', GREEN, "Green Death",
    M_SPELLCASTER | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON,
    0, 14, MONS_GREEN_DEATH, MONS_GREEN_DEATH, MH_DEMONIC, -9,
    { {AT_HIT, AF_PLAIN, 32}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 13, 3, 5, 0 },
    5, 7, 12, 7, MST_GREEN_DEATH, CE_POISONOUS, Z_NOZOMBIE, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "A bloated form covered in oozing sores and exhaling clouds of lethal poison."
}
,

{
    MONS_BLUE_DEATH, '1', BLUE, "Blue Death",
    M_SPELLCASTER | M_FLIES | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON | MR_VUL_FIRE | MR_RES_COLD | MR_RES_ELEC,
    0, 14, MONS_BLUE_DEATH, MONS_BLUE_DEATH, MH_DEMONIC, -9,
    { {AT_HIT, AF_PLAIN, 20}, {AT_HIT, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 12, 3, 5, 0 },
    10, 10, 12, 7, MST_BLUE_DEATH, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "A blue greater demon."
}
,

{
    MONS_BALRUG, '1', RED, "Balrug",
    M_FIGHTER | M_SPELLCASTER | M_FLIES | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_HELLFIRE | MR_VUL_COLD,
    0, 14, MONS_BALRUG, MONS_BALRUG, MH_DEMONIC, -9,
    { {AT_HIT, AF_FIRE, 25}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 14, 3, 5, 0 },
    5, 12, 12, 7, MST_BALRUG, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_BIG,
    "A huge and very powerful demon, wreathed in fire and shadows."
}
,

{
    MONS_CACODEMON, '1', YELLOW, "Cacodemon",
    M_SPELLCASTER | M_LEVITATE | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_ELEC,
    0, 14, MONS_CACODEMON, MONS_CACODEMON, MH_DEMONIC, -9,
    { {AT_HIT, AF_PLAIN, 22}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 13, 3, 5, 0 },
    11, 10, 10, 7, MST_CACODEMON, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "A hideously ugly demon of rage and legendary power."
}
,


{
    MONS_DEMONIC_CRAWLER, '3', DARKGREY, "demonic crawler",
    M_SEE_INVIS | M_EVIL,
    MR_RES_ELEC | MR_RES_POISON | MR_RES_COLD | MR_RES_FIRE,
    0, 12, MONS_DEMONIC_CRAWLER, MONS_DEMONIC_CRAWLER, MH_DEMONIC, -6,
    { {AT_HIT, AF_PLAIN, 13}, {AT_HIT, AF_PLAIN, 13}, {AT_HIT, AF_PLAIN, 13} },
    { 9, 3, 5, 0 },
    10, 6, 9, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SCREAM, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_BIG,
    "A long and bloated body, supported by dozens of short legs and topped with an evil-looking head."
}
,

{
    MONS_SUN_DEMON, '2', YELLOW, "sun demon",
    M_SENSE_INVIS | M_LEVITATE | M_EVIL,
    MR_RES_ELEC | MR_RES_POISON | MR_VUL_COLD | MR_RES_HELLFIRE,
    0, 14, MONS_SUN_DEMON, MONS_SUN_DEMON, MH_DEMONIC, -6,
    { {AT_HIT, AF_FIRE, 30}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 3, 5, 0 },
    10, 12, 12, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A demonic figure shining with the light and fury of a fallen star."
}
,

{
    MONS_SHADOW_IMP, '5', DARKGREY, "shadow imp",
    M_SPELLCASTER | M_FLIES | M_SPEAKS | M_EVIL,
    MR_RES_COLD | MR_RES_POISON,
    0, 11, MONS_IMP, MONS_SHADOW_IMP, MH_DEMONIC, -3,
    { {AT_HIT, AF_PLAIN, 6}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 2, 3, 5, 0 },
    3, 11, 10, 7, MST_SHADOW_IMP, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_LITTLE,
    "A small and shadowy minor demon."
}
,

{
    MONS_SHADOW_DEMON, '3', DARKGREY, "shadow demon",
    M_SEE_INVIS | M_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    0, 12, MONS_SHADOW_DEMON, MONS_SHADOW_DEMON, MH_DEMONIC, -7,
    { {AT_HIT, AF_PLAIN, 21}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 5, 0 },
    7, 12, 11, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_CROAK, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_SMALL,
    "A mysterious demonic figure, constantly blurring into multiple shadows of itself."
}
,

{
    MONS_LOROCYPROCA, '2', BLUE, "Lorocyproca",
    M_SENSE_INVIS | M_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_COLD | MR_RES_FIRE | MR_RES_ELEC,
    0, 12, MONS_LOROCYPROCA, MONS_LOROCYPROCA, MH_DEMONIC, -7,
    { {AT_HIT, AF_PLAIN, 25}, {AT_HIT, AF_PLAIN, 25}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 12, 3, 5, 0 },
    10, 12, 9, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_MOAN, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A tall and gaunt figure, draped in long robes which flow as if alive."
}
,

{
    MONS_SHADOW_WRAITH, 'W', BLUE, "shadow wraith",
    M_LEVITATE | M_SEE_INVIS | M_INVIS | M_EVIL,
    MR_RES_POISON,
    0, 15, MONS_WRAITH, MONS_SHADOW_WRAITH, MH_UNDEAD, -8,
    { {AT_HIT, AF_DRAIN_XP, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 3, 5, 0 },
    7, 7, 10, 10, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_MOAN, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A mist-wreathed skeletal shadow hanging in mid-air, this creature is almost invisible even to your enhanced sight."
}
,

{
    MONS_GIANT_AMOEBA, 'J', BLUE, "giant amoeba",
    M_NO_SKELETON | M_SENSE_INVIS | M_AMPHIBIOUS,
    MR_RES_POISON,
    1000, 10, MONS_GIANT_AMOEBA, MONS_GIANT_AMOEBA, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 25}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 12, 3, 5, 0 },
    0, 4, 10, 10, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_LITTLE,
    "A pulsating lump of protoplasm."
}
,

{
    MONS_GIANT_SLUG, 'm', GREEN, "giant slug",
    M_NO_SKELETON | M_AMPHIBIOUS,
    MR_NO_FLAGS,
    700, 10, MONS_GIANT_SLUG, MONS_GIANT_SLUG, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 23}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 5, 3, 0 },
    0, 2, 6, 10, MST_NO_SPELLS, CE_POISONOUS, Z_BIG, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_LARGE,
    "A huge and disgusting gastropod. "
}
,

{
    MONS_GIANT_SNAIL, 'm', LIGHTGREEN, "giant snail",
    M_NO_SKELETON | M_AMPHIBIOUS,
    MR_NO_FLAGS,
    900, 10, MONS_GIANT_SLUG, MONS_GIANT_SNAIL, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 18}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 14, 5, 3, 0 },
    7, 2, 4, 10, MST_NO_SPELLS, CE_POISONOUS, Z_BIG, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_LARGE,
    "A huge and disgusting gastropod with light green shell."
}
,

{
    MONS_SPATIAL_VORTEX, 'v', BLACK, "spatial vortex",
    M_LEVITATE | M_CONFUSED,
    MR_RES_POISON | MR_RES_FIRE | MR_RES_COLD | MR_RES_ELEC,
    0, 5, MONS_FIRE_VORTEX, MONS_SPATIAL_VORTEX, MH_NONLIVING, MAG_IMMUNE,
    { {AT_HIT, AF_DISTORT, 30}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 6, 6, 0 },
    0, 5, 15, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_LARGE,
    "A crazily shifting twist in the fabric of reality."
}
,

{
    MONS_PIT_FIEND, '1', BROWN, "Pit Fiend",
    M_FLIES | M_SEE_INVIS | M_EVIL | M_SPECIAL_ABILITY,
    MR_RES_POISON | MR_RES_HELLFIRE | MR_RES_COLD | MR_RES_ELEC,
    0, 18, MONS_FIEND, MONS_PIT_FIEND, MH_DEMONIC, -12,
    { {AT_HIT, AF_PLAIN, 28}, {AT_HIT, AF_PLAIN, 21}, {AT_HIT, AF_PLAIN, 21}, {AT_NONE, AF_PLAIN, 0} },
    { 19, 4, 5, 0 },
    17, 5, 8, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "A huge winged fiend with incredibly tough skin."
}
,

{
    MONS_BORING_BEETLE, 'B', BROWN, "boring beetle",
    M_NO_FLAGS,
    MR_VUL_POISON,
    1300, 10, MONS_GIANT_BEETLE, MONS_BORING_BEETLE, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 26}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 5, 0 },
    13, 4, 6, 7, MST_NO_SPELLS, CE_POISONOUS, Z_BIG, S_SILENT, I_INSECT, 
    MONUSE_NOTHING, SIZE_LARGE,
    "A large brown beetle with huge, rock-crushing mandibles."
}
,

{
    MONS_GARGOYLE, 'g', DARKGREY, "gargoyle",
    M_FLIES,
    MR_RES_POISON | MR_RES_ELEC,
    0, 12, MONS_GARGOYLE, MONS_GARGOYLE, MH_NONLIVING, -6,
    { {AT_BITE, AF_PLAIN, 10}, {AT_CLAW, AF_PLAIN, 6}, {AT_CLAW, AF_PLAIN, 6}, {AT_NONE, AF_PLAIN, 0} },
    { 4, 3, 5, 0 },
    18, 6, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A hideous stone statue come to life."
}
,

// only appear in Dis castle
{
    MONS_METAL_GARGOYLE, 'g', CYAN, "metal gargoyle",
    M_FLIES,
    MR_RES_POISON | MR_RES_ELEC,
    0, 12, MONS_GARGOYLE, MONS_METAL_GARGOYLE, MH_NONLIVING, -6,
    { {AT_BITE, AF_PLAIN, 19}, {AT_CLAW, AF_PLAIN, 10}, {AT_CLAW, AF_PLAIN, 10}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 5, 0 },
    20, 4, 7, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A hideous metal statue come to life."
}
,

// only appear in Gehenna castle & one minivault
{
    MONS_MOLTEN_GARGOYLE, 'g', RED, "molten gargoyle",
    M_FLIES,
    MR_RES_POISON | MR_RES_ELEC | MR_RES_FIRE,
    0, 12, MONS_GARGOYLE, MONS_MOLTEN_GARGOYLE, MH_NONLIVING, -6,
    { {AT_BITE, AF_FIRE, 12}, {AT_CLAW, AF_PLAIN, 8}, {AT_CLAW, AF_PLAIN, 8}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 3, 5, 0 },
    14, 7, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A hideous molten stone statue come to life."
}
,


// 250 can't exist


{
    MONS_MNOLEG, '&', LIGHTGREEN, "Mnoleg",
    M_FIGHTER | M_SEE_INVIS | M_SPELLCASTER | M_SPEAKS | M_EVIL | M_UNIQUE,
    MR_RES_ELEC | MR_RES_POISON | MR_RES_FIRE,
    0, 25, MONS_MNOLEG, MONS_MNOLEG, MH_DEMONIC, MAG_IMMUNE,
    { {AT_HIT, AF_PLAIN, 35}, {AT_HIT, AF_PLAIN, 23}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 17, 0, 0, 199 },
    10, 13, 13, 7, MST_MNOLEG, CE_CONTAMINATED, Z_NOZOMBIE, S_BUZZ, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "A weirdly glowing figure, dancing through the twisted air of Pandemonium."
}
,

{
    MONS_LOM_LOBON, '&', LIGHTBLUE, "Lom Lobon",
    M_FIGHTER | M_LEVITATE | M_SEE_INVIS | M_SPELLCASTER | M_SPEAKS | M_EVIL | M_UNIQUE,
    MR_RES_POISON | MR_RES_FIRE | MR_RES_COLD | MR_RES_ELEC,
    0, 25, MONS_LOM_LOBON, MONS_LOM_LOBON, MH_DEMONIC, MAG_IMMUNE,
    { {AT_HIT, AF_PLAIN, 40}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 19, 0, 0, 223 },
    10, 7, 8, 7, MST_LOM_LOBON, CE_CONTAMINATED, Z_NOZOMBIE, S_SCREAM, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "An ancient and strangely serene demon. It regards you coldly from the huge glowing eye in the centre of its forehead."
}
,

{
    MONS_CEREBOV, '&', RED, "Cerebov",
    M_FIGHTER | M_SPELLCASTER | M_SEE_INVIS | M_SPEAKS | M_EVIL | M_UNIQUE,
    MR_RES_ELEC | MR_RES_POISON | MR_RES_HELLFIRE,
    0, 25, MONS_CEREBOV, MONS_CEREBOV, MH_DEMONIC, -6,
    { {AT_HIT, AF_PLAIN, 60}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 21, 0, 0, 253 },
    15, 8, 10, 7, MST_CEREBOV, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_GIANT,
    "A violent and wrathful demon, Cerebov appears as a giant human covered in shining golden armour and wielding a huge twisted sword."
}
,

{
    MONS_GLOORX_VLOQ, '&', DARKGREY, "Gloorx Vloq",
    M_FIGHTER | M_LEVITATE | M_SEE_INVIS | M_SPELLCASTER | M_SPEAKS | M_EVIL | M_UNIQUE,
    MR_RES_POISON | MR_RES_COLD | MR_RES_ELEC,
    0, 25, MONS_GLOORX_VLOQ, MONS_GLOORX_VLOQ, MH_DEMONIC, -14,
    { {AT_HIT, AF_PLAIN, 40}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 16, 0, 0, 234 },
    10, 10, 10, 7, MST_GLOORX_VLOQ, CE_CONTAMINATED, Z_NOZOMBIE, S_MOAN, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "A shadowy figure clothed in profound darkness."
}
,

/* ******************************************************************
{MONS_MOLLUSC_LORD, 'U', GREEN, "The Mollusc Lord", M_NO_FLAGS,
   MR_RES_POISON,
   0, 25, 255, MH_DEMONIC, -3, {30,0,0,0},
   {16,0,0,100}, 10, 10, 10, 7, 93, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_HIGH, 1},
****************************************************************** */

{
    MONS_NAGA_MAGE, 'N', LIGHTRED, "naga mage",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SEE_INVIS | M_WARM_BLOOD,
    MR_RES_POISON,
    750, 13, MONS_NAGA, MONS_NAGA, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 10}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    6, 10, 8, 7, MST_NAGA_MAGE, CE_POISONOUS, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_BIG,
    "A hybrid; human from the chest up, with a scaly, muscular torso trailing off like that of a snake. "
}
,

{
    MONS_NAGA_WARRIOR, 'N', BLUE, "naga warrior",
    M_FIGHTER | M_SPELLCASTER | M_SEE_INVIS | M_WARM_BLOOD,
    MR_RES_POISON,
    750, 12, MONS_NAGA, MONS_NAGA, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 5, 5, 0 },
    6, 10, 8, 7, MST_NAGA, CE_POISONOUS, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_BIG,
    "A hybrid; human from the chest up, with a scaly, muscular torso trailing off like that of a snake. "
}
,

{
    MONS_ORC_WARLORD, 'o', RED, "orc warlord",
    M_FIGHTER | M_WARM_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    600, 15, MONS_ORC, MONS_ORC, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 32}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 15, 4, 7, 0 },
    3, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A very large and strong looking orc."
}
,

{
    MONS_DEEP_ELF_SOLDIER, 'e', CYAN, "deep elf soldier",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD | M_FIGHTER,
    MR_NO_FLAGS,
    450, 10, MONS_ELF, MONS_ELF, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 6}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 3, 3, 0 },
    0, 12, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "One of the race of elves which inhabits this dreary cave. "
}
,

{
    MONS_DEEP_ELF_FIGHTER, 'e', LIGHTBLUE, "deep elf fighter",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD | M_FIGHTER,
    MR_NO_FLAGS,
    450, 10, MONS_ELF, MONS_ELF, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 9}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 3, 0 },
    0, 13, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "One of the race of elves which inhabits this dreary cave. "
}
,

{
    MONS_DEEP_ELF_KNIGHT, 'e', BLUE, "deep elf knight",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD | M_FIGHTER,
    MR_NO_FLAGS,
    450, 10, MONS_ELF, MONS_ELF, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 14}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 11, 3, 3, 0 },
    0, 15, 11, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "One of the race of elves which inhabits this dreary cave. "
}
,

{
    MONS_DEEP_ELF_MAGE, 'e', LIGHTRED, "deep elf mage",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD,
    MR_RES_ELEC,
    450, 10, MONS_ELF, MONS_ELF, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 4, 3, 3, 0 },
    0, 13, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "One of the race of elves which inhabits this dreary cave. "
}
,

{
    MONS_DEEP_ELF_SUMMONER, 'e', YELLOW, "deep elf summoner",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD,
    MR_NO_FLAGS,
    450, 10, MONS_ELF, MONS_ELF, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 3, 0 },
    0, 13, 10, 7, MST_DEEP_ELF_SUMMONER, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    ""
}
,

{
    MONS_DEEP_ELF_CONJURER, 'e', LIGHTGREEN, "deep elf conjurer",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD,
    MR_RES_ELEC,
    450, 10, MONS_ELF, MONS_ELF, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 3, 0 },
    0, 13, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "One of the race of elves which inhabits this dreary cave. "
}
,

{
    MONS_DEEP_ELF_PRIEST, 'e', LIGHTGREY, "deep elf priest",
    M_SPELLCASTER | M_PRIEST | M_WARM_BLOOD,
    MR_NO_FLAGS,
    450, 10, MONS_ELF, MONS_ELF, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 9}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 3, 3, 0 },
    0, 13, 10, 7, MST_DEEP_ELF_PRIEST, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "One of the race of elves which inhabits this dreary cave. "
}
,

{
    MONS_DEEP_ELF_HIGH_PRIEST, 'e', DARKGREY, "deep elf high priest",
    M_SPELLCASTER | M_SPEAKS | M_PRIEST | M_WARM_BLOOD | M_SEE_INVIS | M_EVIL,
    MR_NO_FLAGS,
    450, 10, MONS_ELF, MONS_ELF, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 14}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 11, 3, 3, 0 },
    3, 13, 10, 7, MST_DEEP_ELF_HIGH_PRIEST, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "One of the race of elves which inhabits this dreary cave. "
}
,

{
    MONS_DEEP_ELF_DEMONOLOGIST, 'e', MAGENTA, "deep elf demonologist",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD | M_SEE_INVIS | M_EVIL,
    MR_NO_FLAGS,
    450, 10, MONS_ELF, MONS_ELF, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 12, 3, 3, 0 },
    0, 13, 10, 7, MST_DEEP_ELF_DEMONOLOGIST, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "One of the race of elves which inhabits this dreary cave. "
}
,

{
    MONS_DEEP_ELF_ANNIHILATOR, 'e', GREEN, "deep elf annihilator",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD | M_SEE_INVIS,
    MR_RES_ELEC,
    450, 10, MONS_ELF, MONS_ELF, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 15, 3, 3, 0 },
    0, 13, 10, 7, MST_DEEP_ELF_ANNIHILATOR, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "One of the race of elves which inhabits this dreary cave. "
}
,

{
    MONS_DEEP_ELF_SORCERER, 'e', RED, "deep elf sorcerer",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD | M_SEE_INVIS | M_SPEAKS,
    MR_NO_FLAGS,
    450, 10, MONS_ELF, MONS_ELF, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 14, 3, 3, 0 },
    0, 13, 10, 7, MST_DEEP_ELF_SORCERER, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "One of the race of elves which inhabits this dreary cave. "
}
,

{
    MONS_DEEP_ELF_DEATH_MAGE, 'e', WHITE, "deep elf death mage",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD | M_SEE_INVIS | M_EVIL,
    MR_NO_FLAGS,
    450, 10, MONS_ELF, MONS_ELF, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 15, 3, 3, 0 },
    0, 13, 10, 7, MST_DEEP_ELF_DEATH_MAGE, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "One of the race of elves which inhabits this dreary cave. "
}
,

{
    MONS_BROWN_OOZE, 'J', BROWN, "brown ooze",
    M_NO_SKELETON | M_SENSE_INVIS | M_ACID_SPLASH,
    MR_RES_POISON | MR_RES_ASPHYX | MR_RES_ACID,
    0, 11, MONS_JELLY, MONS_BROWN_OOZE, MH_NATURAL, -7,
    { {AT_HIT, AF_ACID, 25}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    10, 1, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_EATS_ITEMS, SIZE_LITTLE,
    "A viscous liquid, flowing along the floor in search of organic matter to corrode."
}
,

{
    MONS_AZURE_JELLY, 'J', LIGHTBLUE, "azure jelly",
    M_NO_SKELETON | M_SENSE_INVIS,
    MR_RES_POISON | MR_RES_COLD | MR_VUL_FIRE | MR_RES_ELEC | MR_RES_ASPHYX
         | MR_RES_ACID,
    0, 11, MONS_JELLY, MONS_AZURE_JELLY, MH_NATURAL, -4,
    { {AT_HIT, AF_COLD, 12}, {AT_HIT, AF_COLD, 12}, {AT_HIT, AF_PLAIN, 12}, {AT_HIT, AF_PLAIN, 12} },
    { 15, 3, 5, 0 },
    5, 10, 12, 7, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_EATS_ITEMS, SIZE_LITTLE,
    "A frosty blob of bright blue cytoplasm."
}
,

{
    MONS_DEATH_OOZE, 'J', DARKGREY, "death ooze",
    M_NO_SKELETON | M_SENSE_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_COLD | MR_RES_ASPHYX | MR_RES_ACID,
    0, 13, MONS_JELLY, MONS_DEATH_OOZE, MH_UNDEAD, -8,
    { {AT_HIT, AF_ROT, 32}, {AT_HIT, AF_PLAIN, 32}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 11, 3, 3, 0 },
    2, 4, 12, 7, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_EATS_ITEMS, SIZE_LITTLE,
    "A putrid mass of decaying flesh."
}
,

{
    MONS_ACID_BLOB, 'J', LIGHTGREEN, "acid blob",
    M_NO_SKELETON | M_SENSE_INVIS | M_SPECIAL_ABILITY | M_ACID_SPLASH,
    MR_RES_POISON | MR_RES_ASPHYX | MR_RES_ACID,
    0, 12, MONS_JELLY, MONS_ACID_BLOB, MH_NATURAL, -7,
    { {AT_HIT, AF_ACID, 42}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 18, 3, 5, 0 },
    1, 3, 14, 7, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_EATS_ITEMS, SIZE_LITTLE,
    "A lump of sickly green flesh, dripping with lethal acid."
}
,

{
    MONS_ROYAL_JELLY, 'J', YELLOW, "royal jelly",
    M_NO_SKELETON | M_SENSE_INVIS | M_ACID_SPLASH,
    MR_RES_POISON | MR_RES_ASPHYX | MR_RES_ACID,
    0, 20, MONS_JELLY, MONS_ROYAL_JELLY, MH_NATURAL, -7,
    { {AT_HIT, AF_ACID, 50}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 21, 0, 0, 111 },
    8, 4, 12, 7, MST_NO_SPELLS, CE_CLEAN, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_EATS_ITEMS, SIZE_SMALL,
    "A particularly rich and golden gelatinous thing."
}
,

{
    MONS_TERENCE, '@', LIGHTCYAN, "Terence",
    M_WARM_BLOOD | M_SPEAKS | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 0, 0, 20 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "An evil human fighter."
}
,

{
    MONS_JESSICA, '@', LIGHTGREY, "Jessica",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 0, 0, 10 },
    0, 10, 10, 7, MST_ORC_WIZARD_I, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "An evil apprentice sorceress."
}
,

{
    MONS_IJYB, 'g', BLUE, "Ijyb",
    M_WARM_BLOOD | M_SPEAKS | M_UNIQUE,
    MR_NO_FLAGS,
    0, 5, MONS_GOBLIN, MONS_GOBLIN, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 4}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 0, 0, 28 },
    2, 12, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_SMALL,
    "A small and twisted goblin, wearing some ugly blue rags."
}
,

{
    MONS_SIGMUND, '@', YELLOW, "Sigmund",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 0, 0, 30 },
    0, 11, 10, 7, MST_ORC_WIZARD_II, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "An evil and spry old human, whose eyes twinkle with madness. Sigmund wields a nasty looking scythe."
}
,

{
    MONS_BLORK_THE_ORC, 'o', BROWN, "Blork the orc",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_ORC, MONS_ORC, MH_NATURAL, -4,
    { {AT_HIT, AF_PLAIN, 7}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 0, 0, 32 },
    0, 9, 8, 7, MST_ORC_WIZARD_III, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A particularly fat and ugly orc."
}
,

{
    MONS_EDMUND, '@', RED, "Edmund",
    M_FIGHTER | M_WARM_BLOOD | M_SPEAKS | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -4,
    { {AT_HIT, AF_PLAIN, 6}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 4, 0, 0, 44 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A lightly armoured warrior."
}
,

{
    MONS_PSYCHE, '@', LIGHTMAGENTA, "Psyche",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -4,
    { {AT_HIT, AF_PLAIN, 7}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 0, 0, 39 },
    0, 12, 13, 7, MST_ORC_WIZARD_III, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A fair-haired mage."
}
,

{
    MONS_EROLCHA, 'O', LIGHTBLUE, "Erolcha",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SEE_INVIS | M_WARM_BLOOD 
        | M_SPEAKS | M_EVIL | M_UNIQUE,
    MR_RES_ELEC,
    0, 20, MONS_OGRE, MONS_OGRE, MH_NATURAL, -7,
    { {AT_HIT, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 0, 0, 54 },
    3, 7, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_LARGE,
    "An especially cunning ogre mage."
}
,

{
    MONS_DONALD, '@', BLUE, "Donald",
    M_FIGHTER | M_WARM_BLOOD | M_SPEAKS | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 8}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 0, 0, 54 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "An adventurer like you, trying to find the Orb."
}
,

{
    MONS_URUG, 'o', RED, "Urug",
    M_FIGHTER | M_WARM_BLOOD | M_SPEAKS | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_ORC, MONS_ORC, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 0, 0, 66 },
    0, 11, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A rude orc."
}
,

{
    MONS_MICHAEL, '@', LIGHTGREY, "Michael",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 9}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 0, 0, 50 },
    0, 10, 10, 7, MST_ORC_WIZARD_III, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A powerful spellcaster, dressed in a long robe."
}
,

{
    MONS_JOSEPH, '@', CYAN, "Joseph",
    M_FIGHTER | M_WARM_BLOOD | M_SPEAKS | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 9}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 0, 0, 66 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "Looks like a mercenary."
}
,

{
    MONS_SNORG, 'T', GREEN, "Snorg",
    M_WARM_BLOOD | M_SPEAKS | M_EVIL | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_TROLL, MONS_TROLL, MH_NATURAL, -6,
    { {AT_BITE, AF_PLAIN, 20}, {AT_CLAW, AF_PLAIN, 15}, {AT_CLAW, AF_PLAIN, 15}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 0, 0, 96 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "A hairy troll."
}
,

{
    MONS_ERICA, '@', MAGENTA, "Erica",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 10}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 9, 0, 0, 64 },
    0, 11, 11, 7, MST_WIZARD_II, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    " comely spellweaver."
}
,

{
    MONS_JOSEPHINE, '@', WHITE, "Josephine",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 11}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 9, 0, 0, 69 },
    0, 10, 10, 7, MST_NECROMANCER_I, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "n ugly elderly figure, dressed in Druidic clothes."
}
,

{
    MONS_HAROLD, '@', LIGHTGREEN, "Harold",
    M_FIGHTER | M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 9, 0, 0, 76 },
    0, 8, 10, 7, MST_HELL_KNIGHT_II, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "An evil human bounty hunter."
}
,

{
    MONS_NORBERT, '@', BROWN, "Norbert",
    M_FIGHTER | M_WARM_BLOOD | M_SPEAKS | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 25}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 0, 0, 105 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A skilled warrior."
}
,

{
    MONS_JOZEF, '@', LIGHTMAGENTA, "Jozef",
    M_FIGHTER | M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 18}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 11, 0, 0, 90 },
    0, 9, 10, 7, MST_GUARDIAN_NAGA, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A tall bounty hunter."
}
,

{
    MONS_AGNES, '@', LIGHTBLUE, "Agnes",
    M_FIGHTER | M_WARM_BLOOD | M_SPEAKS | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 25}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 11, 0, 0, 140 },
    0, 10, 15, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A lanky warrior."
}
,

{
    MONS_MAUD, '@', RED, "Maud",
    M_FIGHTER | M_WARM_BLOOD | M_SPEAKS | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 24}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 13, 0, 0, 118 },
    0, 10, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "An evil warrior who looks inexplicably like a rodent.  Looking in to her eyes, you feel yourself forgetting everything else."
}
,

{
    MONS_LOUISE, '@', BLUE, "Louise",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD | M_UNIQUE,
    MR_RES_ELEC,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 17}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 13, 0, 0, 106 },
    0, 10, 10, 7, MST_WIZARD_IV, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "An unusually heavily armoured spellcaster."
}
,

{
    MONS_FRANCIS, '@', YELLOW, "Francis",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD 
        | M_SEE_INVIS | M_EVIL | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 19}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 14, 0, 0, 110 },
    0, 10, 10, 7, MST_ORC_HIGH_PRIEST, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A wizened spellcaster."
}
,

{
    MONS_FRANCES, '@', YELLOW, "Frances",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD 
        | M_SEE_INVIS | M_EVIL | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 29}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 14, 0, 0, 121 },
    0, 10, 10, 7, MST_ORC_HIGH_PRIEST, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A stout warrior, bearing a deep facial scar."
}
,

{
    MONS_RUPERT, '@', RED, "Rupert",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD 
        | M_SEE_INVIS | M_UNIQUE,
    MR_RES_ELEC,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 21}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 16, 0, 0, 123 },
    0, 10, 10, 7, MST_RUPERT, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "An evil berserker."
}
,

{
    MONS_WAYNE, '@', YELLOW, "Wayne",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD 
        | M_SEE_INVIS | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 22}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 17, 0, 0, 140 },
    1, 10, 7, 7, MST_ORC_PRIEST, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A fat, evil dwarf in a stupid looking hat."
}
,

{
    MONS_DUANE, '@', YELLOW, "Duane",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD 
        | M_SEE_INVIS | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 22}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 18, 0, 0, 136 },
    0, 10, 10, 7, MST_ORC_WIZARD_I, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "An evil mercenary with unusually large ears."
}
,

{
    MONS_XTAHUA, 'D', RED, "Xtahua",
    M_SEE_INVIS | M_FLIES | M_SPECIAL_ABILITY | M_UNIQUE,
    MR_RES_POISON | MR_RES_FIRE | MR_VUL_COLD,
    0, 20, MONS_DRAGON, MONS_DRAGON, MH_NATURAL, -7,
    { {AT_BITE, AF_PLAIN, 35}, {AT_CLAW, AF_PLAIN, 20}, {AT_CLAW, AF_PLAIN, 17}, {AT_NONE, AF_PLAIN, 0} },
    { 19, 0, 0, 133 },
    15, 7, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_HUGE,
    "An ancient and mighty dragon."
}
,

{
    MONS_NORRIS, '@', LIGHTRED, "Norris",
    M_FIGHTER | M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD 
        | M_SEE_INVIS | M_EVIL | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 36}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 20, 0, 0, 214 },
    1, 9, 9, 7, MST_MYSTIC, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A tan, fit and thoroughly evil surfer."
}
,

{
    MONS_FREDERICK, '@', DARKGREY, "Frederick",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD
        | M_SEE_INVIS | M_EVIL | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 27}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 21, 0, 0, 159 },
    0, 10, 10, 7, MST_LICH_IV, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A svelte fighter-mage, wearing a gold-rimmed monocle."
}
,

{
    MONS_MARGERY, '@', RED, "Margery",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SPEAKS | M_WARM_BLOOD 
        | M_SEE_INVIS | M_UNIQUE,
    MR_NO_FLAGS,
    0, 20, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 30}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 22, 0, 0, 164 },
    0, 10, 10, 7, MST_EFREET, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A lithe spellcaster."
}
,

{
    MONS_BORIS, 'L', RED, "Boris",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SEE_INVIS | M_SPEAKS | M_EVIL
        | M_UNIQUE,
    MR_RES_POISON | MR_RES_COLD | MR_RES_ELEC,
    0, 23, MONS_LICH, MONS_LICH, MH_UNDEAD, -11,
    { {AT_HIT, AF_PLAIN, 25}, {AT_TOUCH, AF_DRAIN_XP, 15}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 22, 0, 0, 154 },
    12, 10, 10, 7, MST_LICH_IV, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_STARTING_EQUIPMENT, SIZE_MEDIUM,
    "An ancient lich. The air around his shrouded form crackles with evil energy."
}
,


{
    MONS_GERYON, '&', GREEN, "Geryon",
    M_FIGHTER | M_SPELLCASTER | M_SEE_INVIS | M_SPEAKS | M_EVIL | M_UNIQUE,
    MR_NO_FLAGS,
    0, 25, MONS_GERYON, MONS_GERYON, MH_DEMONIC, -6,
    { {AT_HIT, AF_PLAIN, 35}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 15, 0, 0, 240 },
    15, 6, 10, 7, MST_GERYON, CE_CONTAMINATED, Z_NOZOMBIE, S_ROAR, I_NORMAL, 
    MONUSE_STARTING_EQUIPMENT, SIZE_GIANT,
    "A huge and slithery arch-demon, guarding the gates of Hell."
}
,

{
    MONS_DISPATER, '&', MAGENTA, "Dispater",
    M_FIGHTER | M_SPELLCASTER | M_SEE_INVIS | M_SPEAKS | M_EVIL | M_UNIQUE,
    MR_RES_ELEC | MR_RES_POISON | MR_RES_HELLFIRE | MR_RES_COLD,
    0, 25, MONS_DISPATER, MONS_DISPATER, MH_DEMONIC, -10,
    { {AT_HIT, AF_PLAIN, 50}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 16, 0, 0, 222 },
    15, 3, 6, 7, MST_DISPATER, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_LARGE,
    "The lord of the Iron City of Dis."
}
,

{
    MONS_ASMODEUS, '&', LIGHTMAGENTA, "Asmodeus",
    M_FIGHTER | M_SPELLCASTER | M_FLIES | M_SEE_INVIS | M_SPEAKS | M_EVIL | M_UNIQUE,
    MR_RES_ELEC | MR_RES_POISON | MR_RES_HELLFIRE,
    0, 25, MONS_ASMODEUS, MONS_ASMODEUS, MH_DEMONIC, -12,
    { {AT_HIT, AF_PLAIN, 50}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 17, 0, 0, 245 },
    12, 7, 9, 7, MST_ASMODEUS, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_LARGE,
    "One of the arch-demons who dwell in the depths of Hell."
}
,

// Antaeus is now demonic so that he'll resist torment. -- bwr
{
    MONS_ANTAEUS, 'C', LIGHTCYAN, "Antaeus",
    M_FIGHTER | M_SPELLCASTER | M_SPEAKS | M_UNIQUE,
    MR_RES_ELEC | MR_VUL_FIRE | MR_RES_COLD,
    0, 25, MONS_HILL_GIANT, MONS_ANTAEUS, MH_DEMONIC, -9,
    { {AT_HIT, AF_COLD, 55}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 22, 0, 0, 250 },
    10, 4, 7, 7, MST_ANTAEUS, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_GIANT,
    "A great titan who lives in the depths of Cocytus."
}
,

{
    MONS_ERESHKIGAL, '&', WHITE, "Ereshkigal",
    M_SPELLCASTER | M_SEE_INVIS | M_SPEAKS | M_EVIL | M_UNIQUE,
    MR_RES_ELEC | MR_RES_POISON | MR_RES_COLD,
    0, 25, MONS_ERESHKIGAL, MONS_ERESHKIGAL, MH_DEMONIC, -10,
    { {AT_HIT, AF_PLAIN, 40}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 18, 0, 0, 238 },
    15, 6, 9, 7, MST_ERESHKIGAL, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_LARGE,
    "A fearsome arch-fiend who rules the deathly netherworld of Tartarus."
}
,

{
    MONS_ANCIENT_LICH, 'L', DARKGREY, "ancient lich",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_COLD | MR_RES_FIRE | MR_RES_ELEC,
    0, 20, MONS_LICH, MONS_LICH, MH_UNDEAD, -14,
    { {AT_TOUCH, AF_DRAIN_XP, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 27, 2, 4, 0 },
    20, 10, 12, 7, MST_LICH_I, CE_NOCORPSE, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A lich who has grown mighty over countless years."
}
,
/* number is set in define_monster */

{
    MONS_OOZE, 'J', LIGHTGREY, "ooze",
    M_NO_SKELETON | M_SENSE_INVIS,
    MR_RES_POISON | MR_RES_ASPHYX | MR_RES_ACID,
    0, 5, MONS_JELLY, MONS_OOZE, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 3, 5, 0 },
    1, 3, 8, 7, MST_NO_SPELLS, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_LITTLE,
    "A disgusting glob of grey sludge."
}
,

{
    MONS_VAULT_GUARD, '@', CYAN, "vault guard",
    M_FIGHTER | M_WARM_BLOOD | M_SENSE_INVIS,
    MR_NO_FLAGS,
    0, 12, MONS_HUMAN, MONS_HUMAN, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 13, 3, 5, 0 },
    1, 13, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A heavily armed and armoured guardian of the Vaults."
}
,

/* These nasties are never randomly generated, only sometimes specially
   placed in the Crypt. */
{
    MONS_CURSE_SKULL, 'z', DARKGREY, "curse skull",
    M_LEVITATE | M_SPELLCASTER | M_SEE_INVIS | M_EVIL,
    MR_RES_ELEC | MR_RES_POISON | MR_RES_HELLFIRE | MR_RES_COLD,
    0, 50, MONS_LICH, MONS_CURSE_SKULL, MH_UNDEAD, MAG_IMMUNE,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 13, 0, 0, 66 },
    40, 3, 10, 7, MST_CURSE_SKULL, CE_NOCORPSE, Z_NOZOMBIE, S_MOAN, I_HIGH, 
    MONUSE_NOTHING, SIZE_TINY,
    "A charred skull floating in the air and rotating slowly.  Mystic symbols carved into its blackened surface indicate its resistance to almost any form of attack."
}
,

{
    MONS_VAMPIRE_KNIGHT, 'V', CYAN, "vampire knight",
    M_FIGHTER | M_SPELLCASTER | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    0, 13, MONS_VAMPIRE, MONS_VAMPIRE, MH_UNDEAD, -6,
    { {AT_HIT, AF_PLAIN, 33}, {AT_BITE, AF_VAMPIRIC, 15}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 11, 3, 7, 0 },
    10, 10, 10, 7, MST_VAMPIRE_KNIGHT, CE_NOCORPSE, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A powerful warrior, with skills undiminished by undeath."
}
,

{
    MONS_VAMPIRE_MAGE, 'V', MAGENTA, "vampire mage",
    M_SPELLCASTER | M_SEE_INVIS | M_FLIES | M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    0, 15, MONS_VAMPIRE, MONS_VAMPIRE, MH_UNDEAD, -6,
    { {AT_BITE, AF_VAMPIRIC, 22}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 4, 0 },
    10, 10, 10, 7, MST_VAMPIRE_MAGE, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "Undeath has not lessened this powerful mage."
}
,

{
    MONS_SHINING_EYE, 'G', LIGHTMAGENTA, "shining eye",
    M_NO_SKELETON | M_LEVITATE | M_SPELLCASTER | M_SEE_INVIS,
    MR_RES_ASPHYX,
    0, 14, MONS_SHINING_EYE, MONS_SHINING_EYE, MH_NATURAL, MAG_IMMUNE,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 3, 5, 0 },
    3, 1, 7, 7, MST_SHINING_EYE, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_MEDIUM,
    "A huge and strangely deformed eyeball, pulsating with light.  Beauty is certainly nowhere to be found in this beholder."
}
,

{
    MONS_ORB_GUARDIAN, 'X', MAGENTA, "Orb Guardian",
    M_FIGHTER | M_NO_SKELETON | M_SEE_INVIS,
    MR_NO_FLAGS,
    0, 20, MONS_ORB_GUARDIAN, MONS_ORB_GUARDIAN, MH_NATURAL, -6,
    { {AT_HIT, AF_PLAIN, 45}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 15, 3, 5, 0 },
    13, 13, 14, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS, SIZE_GIANT,
    "A huge and glowing purple creature, created by the Orb to defend itself."
}
,

{
    MONS_DAEVA, 'A', YELLOW, "Daeva",
    M_FIGHTER | M_LEVITATE | M_SPELLCASTER | M_SEE_INVIS,
    MR_RES_POISON,
    0, 12, MONS_ANGEL, MONS_DAEVA, MH_HOLY, -8,
    { {AT_HIT, AF_PLAIN, 25}, {AT_HIT, AF_PLAIN, 10}, {AT_HIT, AF_PLAIN, 10}, {AT_NONE, AF_PLAIN, 0} },
    { 14, 6, 5, 0 },
    10, 13, 10, 7, MST_DAEVA, CE_NOCORPSE, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A divine agent of the Shining One, it is a towering winged figure with an aura of brilliant golden light."
}
,

/* spectral thing - similar to zombies/skeletons */
{
    MONS_SPECTRAL_THING, 'W', GREEN, "",
    M_LEVITATE | M_SEE_INVIS,
    MR_RES_POISON | MR_RES_COLD,
    0, 11, MONS_WRAITH, MONS_SPECTRAL_THING, MH_UNDEAD, MAG_IMMUNE,
    { {AT_HIT, AF_DRAIN_XP, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 5, 0 },
    8, 5, 7, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A hideous glowing apparition."
}
,

{
    MONS_GREATER_NAGA, 'N', RED, "greater naga",
    M_FIGHTER | M_SPELLCASTER | M_ACTUAL_SPELLS | M_SEE_INVIS | M_WARM_BLOOD,
    MR_RES_POISON,
    750, 10, MONS_NAGA, MONS_NAGA, MH_NATURAL, MAG_IMMUNE,
    { {AT_HIT, AF_PLAIN, 24}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 15, 3, 5, 0 },
    6, 10, 8, 7, MST_NAGA_MAGE, CE_POISONOUS, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_BIG,
    "A hybrid; human from the chest up, with a scaly, muscular torso trailing off like that of a snake. "
}
,

{
    MONS_SKELETAL_DRAGON, 'D', LIGHTGREY, "skeletal dragon",
    M_SENSE_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_FIRE | MR_RES_COLD | MR_RES_ELEC,
    0, 12, MONS_SKELETAL_WARRIOR, MONS_SKELETAL_DRAGON, MH_UNDEAD, -4,
    { {AT_BITE, AF_PLAIN, 30}, {AT_CLAW, AF_PLAIN, 20}, {AT_CLAW, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0} },
    { 20, 8, 8, 0 },
    20, 4, 8, 7, MST_NO_SPELLS, CE_CLEAN, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS, SIZE_HUGE,
    "A huge undead abomination, pieced together from the broken bones of many dragons."
}
,

{
    MONS_TENTACLED_MONSTROSITY, 'X', GREEN, "tentacled monstrosity",
    M_SEE_INVIS | M_AMPHIBIOUS,
    MR_RES_POISON | MR_RES_FIRE | MR_RES_COLD | MR_RES_ELEC,
    0, 10, MONS_TENTACLED_MONSTROSITY, MONS_TENTACLED_MONSTROSITY, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 22}, {AT_HIT, AF_PLAIN, 17}, {AT_HIT, AF_PLAIN, 13} },
    { 25, 3, 5, 0 },
    5, 5, 9, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_GIANT,
    "A writhing mass of tentacles, all covered in putrid mucous"
}
,

{
    MONS_SPHINX, 'H', LIGHTGREY, "sphinx",
    M_FLIES | M_SENSE_INVIS | M_SPELLCASTER | M_ACTUAL_SPELLS | M_WARM_BLOOD,
    MR_NO_FLAGS,
    0, 10, MONS_SPHINX, MONS_SPHINX, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 25}, {AT_HIT, AF_PLAIN, 12}, {AT_HIT, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0} },
    { 16, 3, 5, 0 },
    5, 5, 13, 7, MST_SPHINX, CE_CLEAN, Z_NOZOMBIE, S_SHOUT, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_BIG,
    "A large creature with a human head, the body of a lion, and the wings of a huge bird."
}
,

{
    MONS_ROTTING_HULK, 'n', BROWN, "rotting hulk",
    M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    0, 12, MONS_GHOUL, MONS_ROTTING_HULK, MH_UNDEAD, -5,
    { {AT_HIT, AF_DISEASE, 25}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 3, 5, 0 },
    5, 7, 8, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "A shambling undead creature, related to the ghoul."
}
,

{
    MONS_GUARDIAN_MUMMY, 'M', YELLOW, "guardian mummy",
    M_FIGHTER | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    0, 13, MONS_MUMMY, MONS_GUARDIAN_MUMMY, MH_UNDEAD, -5,
    { {AT_HIT, AF_PLAIN, 30}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 5, 3, 0 },
    6, 9, 9, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_WEAPONS_ARMOUR, SIZE_BIG,
    "An ancient warrior, embalmed and cursed to walk in undeath for eternity."
}
,

{
    MONS_GREATER_MUMMY, 'M', DARKGREY, "greater mummy",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_COLD | MR_RES_ELEC,
    0, 20, MONS_MUMMY, MONS_MUMMY, MH_UNDEAD, MAG_IMMUNE,
    { {AT_HIT, AF_PLAIN, 35}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 15, 5, 3, 100 },
    10, 6, 10, 7, MST_MUMMY, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    ""
}
,

{
    MONS_MUMMY_PRIEST, 'M', RED, "mummy priest",
    M_SPELLCASTER | M_PRIEST | M_SEE_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_COLD | MR_RES_ELEC,
    0, 16, MONS_MUMMY, MONS_MUMMY, MH_UNDEAD, MAG_IMMUNE,
    { {AT_HIT, AF_PLAIN, 30}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 5, 3, 0 },
    8, 7, 9, 7, MST_MUMMY, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    ""
}
,

{
    MONS_CENTAUR_WARRIOR, 'c', YELLOW, "centaur warrior",
    M_WARM_BLOOD | M_FIGHTER,
    MR_NO_FLAGS,
    1500, 12, MONS_CENTAUR, MONS_CENTAUR, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 16}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 9, 3, 5, 0 },
    4, 8, 15, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_BIG,
    "A hybrid with the torso of a human atop the body of a large horse.  It looks strong and aggressive."
}
,

{
    MONS_YAKTAUR_CAPTAIN, 'c', RED, "yaktaur captain",
    M_WARM_BLOOD | M_FIGHTER,
    MR_NO_FLAGS,
    2000, 10, MONS_YAKTAUR, MONS_YAKTAUR, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 23}, {AT_HIT, AF_PLAIN, 0}, {AT_HIT, AF_PLAIN, 0} },
    { 14, 3, 5, 0 },
    5, 5, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_SHOUT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "Like a centaur, but half yak. It looks very strong and aggressive."
}
,

{   // Base draconian -- for use like MONS_HUMAN, MONS_ELF although we 
    // now store the draconian subspecies in the high byte of mon->number
    // for those listed as species MONS_DRACONIAN.
    MONS_DRACONIAN, 'd', BROWN, "draconian",
    M_HUMANOID | M_COLD_BLOOD,
    MR_NO_FLAGS,
    900, 10, MONS_DRACONIAN, MONS_DRACONIAN, MH_NATURAL, -1,
    { {AT_HIT, AF_PLAIN, 15}, {AT_HIT, AF_PLAIN, 0}, {AT_HIT, AF_PLAIN, 0} },
    { 3, 6, 4, 0 },
    7, 8, 10, 10, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_HIGH, 
    MONUSE_STARTING_EQUIPMENT, SIZE_MEDIUM,
    "Before you stands a draconian.  "
}
,

{
    MONS_BLACK_DRACONIAN, 'd', DARKGREY, "black draconian",
    M_HUMANOID | M_COLD_BLOOD,
    MR_RES_ELEC,
    900, 10, MONS_DRACONIAN, MONS_BLACK_DRACONIAN, MH_NATURAL, -2,
    { {AT_HIT, AF_PLAIN, 20}, {AT_HIT, AF_PLAIN, 0}, {AT_HIT, AF_PLAIN, 0} },
    { 14, 5, 4, 0 },
    9, 10, 10, 10, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_HIGH, 
    MONUSE_STARTING_EQUIPMENT, SIZE_MEDIUM,
    "Before you stands a draconian.  "
}
,

{ 
    MONS_YELLOW_DRACONIAN, 'd', YELLOW, "yellow draconian", 
    M_HUMANOID | M_COLD_BLOOD | M_SPECIAL_ABILITY,
    MR_RES_ACID,
    900, 10, MONS_DRACONIAN, MONS_YELLOW_DRACONIAN, MH_NATURAL, -2,
    { {AT_HIT, AF_PLAIN, 20}, {AT_HIT, AF_PLAIN, 0}, {AT_HIT, AF_PLAIN, 0} },
    { 14, 5, 4, 0 },
    9, 10, 10, 10, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_HIGH, 
    MONUSE_STARTING_EQUIPMENT, SIZE_MEDIUM,
    "Before you stands a draconian.  "
}
,

{
    MONS_PALE_DRACONIAN, 'd', LIGHTGREY, "pale draconian", 
    M_HUMANOID | M_COLD_BLOOD,
    MR_RES_FIRE, 
    900, 10, MONS_DRACONIAN, MONS_PALE_DRACONIAN, MH_NATURAL, -2,
    { {AT_HIT, AF_PLAIN, 20}, {AT_HIT, AF_PLAIN, 0}, {AT_HIT, AF_PLAIN, 0} },
    { 14, 5, 4, 0 },
    9, 14, 10, 10, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_HIGH, 
    MONUSE_STARTING_EQUIPMENT, SIZE_MEDIUM,
    "Before you stands a draconian.  "
}
,

{
    MONS_GREEN_DRACONIAN, 'd', LIGHTGREEN, "green draconian", 
    M_HUMANOID | M_COLD_BLOOD,
    MR_RES_POISON,
    900, 10, MONS_DRACONIAN, MONS_GREEN_DRACONIAN, MH_NATURAL, -2,
    { {AT_HIT, AF_PLAIN, 20}, {AT_HIT, AF_PLAIN, 0}, {AT_HIT, AF_PLAIN, 0} },
    { 14, 5, 4, 0 },
    9, 10, 10, 10, MST_NO_SPELLS, CE_POISONOUS, Z_SMALL, S_ROAR, I_HIGH, 
    MONUSE_STARTING_EQUIPMENT, SIZE_MEDIUM,
    "Before you stands a draconian.  "
}
,

{
    MONS_PURPLE_DRACONIAN, 'd', MAGENTA, "purple draconian", 
    M_HUMANOID | M_COLD_BLOOD,
    MR_NO_FLAGS,
    900, 10, MONS_DRACONIAN, MONS_PURPLE_DRACONIAN, MH_NATURAL, -2,
    { {AT_HIT, AF_PLAIN, 20}, {AT_HIT, AF_PLAIN, 0}, {AT_HIT, AF_PLAIN, 0} },
    { 14, 5, 4, 0 },
    8, 10, 10, 10, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_HIGH, 
    MONUSE_STARTING_EQUIPMENT, SIZE_MEDIUM,
    "Before you stands a draconian.  "
}
,

{ 
    MONS_RED_DRACONIAN, 'd', RED, "red draconian", 
    M_HUMANOID | M_COLD_BLOOD | M_SPECIAL_ABILITY,
    MR_RES_FIRE,
    900, 10, MONS_DRACONIAN, MONS_RED_DRACONIAN, MH_NATURAL, -2,
    { {AT_HIT, AF_PLAIN, 20}, {AT_HIT, AF_PLAIN, 0}, {AT_HIT, AF_PLAIN, 0} },
    { 14, 5, 4, 0 },
    9, 10, 10, 10, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_HIGH, 
    MONUSE_STARTING_EQUIPMENT, SIZE_MEDIUM,
    "Before you stands a draconian.  "
}
,

{ 
    MONS_WHITE_DRACONIAN, 'd', WHITE, "white draconian",
    M_HUMANOID | M_COLD_BLOOD | M_SPECIAL_ABILITY,
    MR_RES_COLD,
    900, 10, MONS_DRACONIAN, MONS_WHITE_DRACONIAN, MH_NATURAL, -2,
    { {AT_HIT, AF_PLAIN, 20}, {AT_HIT, AF_PLAIN, 0}, {AT_HIT, AF_PLAIN, 0} },
    { 14, 5, 4, 0 },
    9, 10, 10, 10, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_HIGH, 
    MONUSE_STARTING_EQUIPMENT, SIZE_MEDIUM,
    "Before you stands a draconian.  "
}
,

{
    MONS_MOTTLED_DRACONIAN, 'd', LIGHTMAGENTA, "mottled draconian", 
    M_HUMANOID | M_COLD_BLOOD,
    MR_RES_FIRE,
    900, 10, MONS_DRACONIAN, MONS_MOTTLED_DRACONIAN, MH_NATURAL, -2,
    { {AT_HIT, AF_PLAIN, 20}, {AT_HIT, AF_PLAIN, 0}, {AT_HIT, AF_PLAIN, 0} },
    { 14, 5, 4, 0 },
    9, 10, 10, 10, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_HIGH,
    MONUSE_STARTING_EQUIPMENT, SIZE_MEDIUM,
    "Before you stands a draconian.  "
}
,

{
    MONS_DRACONIAN_CALLER, 'd', BROWN, "draconian caller",  
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_HUMANOID | M_COLD_BLOOD,
    MR_NO_FLAGS,
    900, 10, MONS_DRACONIAN, MONS_DRACONIAN, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 20}, {AT_HIT, AF_PLAIN, 0}, {AT_HIT, AF_PLAIN, 0} },
    { 16, 4, 3, 0 },
    9, 10, 10, 10, MST_DRAC_CALLER, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_HIGH, 
    MONUSE_STARTING_EQUIPMENT, SIZE_MEDIUM,
    "Before you stands a draconian.  "
}
,

{
    MONS_DRACONIAN_MONK, 'd', BLUE, "draconian monk", 
    M_FIGHTER | M_HUMANOID | M_COLD_BLOOD,
    MR_NO_FLAGS,
    900, 10, MONS_DRACONIAN, MONS_DRACONIAN, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 35}, {AT_HIT, AF_PLAIN, 20},
                              {AT_TAIL_SLAP, AF_PLAIN, 15} },
    { 16, 6, 3, 0 },
    6, 20, 10, 10, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_HIGH, 
    MONUSE_STARTING_EQUIPMENT, SIZE_MEDIUM,
    "Before you stands a draconian.  "
}
,

{ 
    MONS_DRACONIAN_ZEALOT, 'd', LIGHTBLUE, "draconian zealot", 
    M_SPELLCASTER | M_HUMANOID | M_PRIEST | M_COLD_BLOOD | M_EVIL,
    MR_NO_FLAGS,
    900, 10, MONS_DRACONIAN, MONS_DRACONIAN, MH_NATURAL, -3,
    { {AT_HIT, AF_PLAIN, 15}, {AT_HIT, AF_PLAIN, 0}, {AT_HIT, AF_PLAIN, 0} },
    { 16, 4, 2, 0 },
    12, 10, 10, 10, MST_DEEP_ELF_HIGH_PRIEST, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_HIGH, 
    MONUSE_STARTING_EQUIPMENT, SIZE_MEDIUM,
    "Before you stands a draconian.  "
}
,

{
    MONS_DRACONIAN_SHIFTER, 'd', LIGHTCYAN, "draconian shifter", 
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_HUMANOID | M_COLD_BLOOD,
    MR_NO_FLAGS,
    900, 10, MONS_DRACONIAN, MONS_DRACONIAN, MH_NATURAL, -4,
    { {AT_HIT, AF_PLAIN, 15}, {AT_HIT, AF_PLAIN, 0}, {AT_HIT, AF_PLAIN, 0} },
    { 16, 4, 4, 0 },
    8, 16, 10, 10, MST_DRAC_SHIFTER, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_HIGH, 
    MONUSE_STARTING_EQUIPMENT, SIZE_MEDIUM,
    "Before you stands a draconian.  "
}
,

{
    MONS_DRACONIAN_ANNIHILATOR, 'd', GREEN, "draconian annihilator", 
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_HUMANOID | M_COLD_BLOOD,
    MR_NO_FLAGS,
    900, 10, MONS_DRACONIAN, MONS_DRACONIAN, MH_NATURAL, -4,
    { {AT_HIT, AF_PLAIN, 15}, {AT_HIT, AF_PLAIN, 0}, {AT_HIT, AF_PLAIN, 0} },
    { 16, 4, 2, 0 },
    8, 10, 10, 10, MST_DEEP_ELF_ANNIHILATOR, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_HIGH, 
    MONUSE_STARTING_EQUIPMENT, SIZE_MEDIUM,
    "Before you stands a draconian.  "
}
,

{
    MONS_DRACONIAN_KNIGHT, 'd', CYAN, "draconian knight", 
    M_SPELLCASTER | M_HUMANOID | M_FIGHTER | M_COLD_BLOOD,
    MR_NO_FLAGS,
    900, 10, MONS_DRACONIAN, MONS_DRACONIAN, MH_NATURAL, -4,
    { {AT_HIT, AF_PLAIN, 15}, {AT_HIT, AF_PLAIN, 0}, {AT_HIT, AF_PLAIN, 0} },
    { 16, 6, 4, 0 },
    12, 12, 10, 6, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_HIGH, 
    MONUSE_STARTING_EQUIPMENT, SIZE_MEDIUM,
    "Before you stands a draconian.  "
}
,

{
    MONS_DRACONIAN_SCORCHER, 'd', LIGHTRED, "draconian scorcher", 
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_HUMANOID | M_COLD_BLOOD | M_EVIL,
    MR_RES_FIRE | MR_RES_HELLFIRE,
    900, 10, MONS_DRACONIAN, MONS_DRACONIAN, MH_NATURAL, -4,
    { {AT_HIT, AF_PLAIN, 15}, {AT_HIT, AF_PLAIN, 0}, {AT_HIT, AF_PLAIN, 0} },
    { 16, 4, 2, 0 },
    8, 12, 10, 10, MST_DRAC_SCORCHER, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_HIGH, 
    MONUSE_STARTING_EQUIPMENT, SIZE_MEDIUM,
    "Before you stands a draconian.  "
}
,

{
    MONS_KILLER_KLOWN, '@', BLACK, "Killer Klown",
    M_SEE_INVIS | M_SPEAKS | M_WARM_BLOOD | M_SPECIAL_ABILITY,
    MR_NO_FLAGS,
    0, 15, MONS_HUMAN, MONS_KILLER_KLOWN, MH_NATURAL, -6,
    { {AT_HIT, AF_KLOWN, 30}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 20, 5, 5, 0 },
    10, 15, 15, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    "A comical figure full of life and laughter.  It looks very happy to see you... but is there a slightly malicious cast to its features?  Is that red facepaint or something altogether less pleasant?  Join in the fun, and maybe you'll find out!"
}
,

{
    MONS_ELECTRIC_GOLEM, '8', LIGHTCYAN, "electric golem",
    M_SPELLCASTER | M_SEE_INVIS,
    MR_RES_ELEC | MR_RES_POISON | MR_RES_FIRE | MR_RES_COLD,
    0, 10, MONS_CLAY_GOLEM, MONS_ELECTRIC_GOLEM, MH_NONLIVING, -8,
    { {AT_HIT, AF_ELEC, 15}, {AT_HIT, AF_ELEC, 15}, {AT_HIT, AF_PLAIN, 15}, {AT_HIT, AF_PLAIN, 15} },
    { 15, 7, 4, 0 },
    5, 20, 20, 7, MST_ELECTRIC_GOLEM, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "An animated figure made completely of electricity."
}
,

{
    MONS_BALL_LIGHTNING, '*', LIGHTCYAN, "ball lightning",
    M_FLIES | M_CONFUSED | M_SPELLCASTER | M_SPECIAL_ABILITY,
    MR_RES_ELEC,
    0, 20, MONS_BALL_LIGHTNING, MONS_BALL_LIGHTNING, MH_NONLIVING, MAG_IMMUNE,
    { {AT_HIT, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 12, 0, 0, 1 },
    0, 10, 20, 7, MST_STORM_DRAGON, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_PLANT, 
    MONUSE_OPEN_DOORS, SIZE_LITTLE,
    "An oddity of nature, ball lightning bounces around behaving almost, but not quite, entirely unlike regular lightning."
}
,

{
    MONS_ORB_OF_FIRE, '*', RED, "orb of fire",
    M_SPELLCASTER | M_FLIES | M_SEE_INVIS,
    MR_RES_ELEC | MR_RES_FIRE | MR_RES_HELLFIRE | MR_RES_COLD | MR_RES_POISON,
    0, 10, MONS_ORB_OF_FIRE, MONS_ORB_OF_FIRE, MH_NONLIVING, MAG_IMMUNE,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 30, 0, 0, 150 },
    20, 20, 20, 7, MST_ORB_OF_FIRE, CE_CONTAMINATED, Z_NOZOMBIE, S_SHOUT, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_LITTLE,
    "A globe of raw primordial fire, capable of impressive pyrotechnics."
}
,

{
    MONS_QUOKKA, 'r', LIGHTGREY, "quokka",
    M_WARM_BLOOD,
    MR_NO_FLAGS,
    300, 10, MONS_QUOKKA, MONS_QUOKKA, MH_NATURAL, -1,
    { {AT_BITE, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 3, 5, 0 },
    2, 13, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_NORMAL, 
    MONUSE_NOTHING, SIZE_TINY,
    "A small marsupial.  Don't call it a rat."
}
,

{
    MONS_EYE_OF_DEVASTATION, 'G', YELLOW, "eye of devastation",
    M_NO_SKELETON | M_LEVITATE | M_SPELLCASTER | M_SEE_INVIS,
    MR_RES_ASPHYX,
    0, 11, MONS_GIANT_EYEBALL, MONS_EYE_OF_DEVASTATION, MH_NATURAL, MAG_IMMUNE,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 3, 5, 0 },
    12, 1, 7, 7, MST_EYE_OF_DEVASTATION, CE_POISONOUS, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_BIG,
    "A huge eyeball, encased in a levitating globe of incandescent energy."
}
,

{
    MONS_MOTH_OF_WRATH, 'y', BROWN, "moth of wrath",
    M_FLIES | M_SPECIAL_ABILITY,
    MR_NO_FLAGS,
    0, 10, MONS_MOTH_OF_WRATH, MONS_MOTH_OF_WRATH, MH_NATURAL, -3,
    { {AT_BITE, AF_RAGE, 25}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 9, 3, 5, 0 },
    0, 10, 12, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SHOUT, I_HIGH,
    MONUSE_NOTHING, SIZE_MEDIUM,
    "A huge moth, as violent as it is hairy."
}
,

{
    MONS_DEATH_COB, '%', YELLOW, "death cob",
    M_SPEAKS | M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    0, 10, MONS_DEATH_COB, MONS_DEATH_COB, MH_UNDEAD, -3,
    { {AT_HIT, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 4, 5, 0 },
    10, 15, 25, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_MOAN, I_NORMAL, 
    MONUSE_OPEN_DOORS, SIZE_TINY,
    "A dreadful undead cob of corn."
}
,

{
    MONS_CURSE_TOE, 'z', YELLOW, "curse toe", 
    M_LEVITATE | M_SPELLCASTER | M_SEE_INVIS | M_EVIL,
    MR_RES_ELEC | MR_RES_POISON | MR_RES_HELLFIRE | MR_RES_COLD,
    0, 60, MONS_LICH, MONS_CURSE_TOE, MH_UNDEAD, MAG_IMMUNE,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 14, 0, 0, 77 },
    50, 1, 12, 7, MST_CURSE_TOE, CE_NOCORPSE, Z_NOZOMBIE, S_MOAN, I_HIGH, 
    MONUSE_NOTHING, SIZE_TINY,
    "A disembodied toe, hanging in the air and radiating an intense field of negative energy."
}
,

{
    // gold mimics are the only mimics that actually use their name -- bwr
    MONS_GOLD_MIMIC, '$', YELLOW, "pile of gold coins",
    M_NO_SKELETON,
    MR_RES_POISON | MR_RES_ELEC | MR_RES_FIRE | MR_RES_COLD,
    0, 13, MONS_GOLD_MIMIC, MONS_GOLD_MIMIC, MH_NONLIVING, -3,
    { {AT_HIT, AF_POISON, 12}, {AT_HIT, AF_PLAIN, 12}, {AT_HIT, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 5, 0 },
    5, 1, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_NOTHING, SIZE_TINY,
    "An apparently harmless pile of gold coins hides a nasty venomous shapechanging predator."
}
,

{
    MONS_WEAPON_MIMIC, ')', BLACK, "mimic",
    M_NO_SKELETON,
    MR_RES_POISON | MR_RES_ELEC | MR_RES_FIRE | MR_RES_COLD,
    0, 13, MONS_GOLD_MIMIC, MONS_GOLD_MIMIC, MH_NONLIVING, -3,
    { {AT_HIT, AF_POISON, 17}, {AT_HIT, AF_PLAIN, 17}, {AT_HIT, AF_PLAIN, 17}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 5, 0 },
    5, 1, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_NOTHING, SIZE_LITTLE,
    "An apparently abandoned weapon, actually a vicious little beast in disguise."
}
,

{
    MONS_ARMOUR_MIMIC, '[', BLACK, "mimic",
    M_NO_SKELETON,
    MR_RES_POISON | MR_RES_ELEC | MR_RES_FIRE | MR_RES_COLD,
    0, 13, MONS_GOLD_MIMIC, MONS_GOLD_MIMIC, MH_NONLIVING, -3,
    { {AT_HIT, AF_POISON, 12}, {AT_HIT, AF_PLAIN, 12}, {AT_HIT, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 5, 0 },
    15, 1, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_NOTHING, SIZE_SMALL,
    "An apparently abandoned suit of finely-made armour, actually  a vicious little beast in disguise."
}
,

{
    MONS_SCROLL_MIMIC, '?', LIGHTGREY, "mimic",
    M_NO_SKELETON,
    MR_RES_POISON | MR_RES_ELEC | MR_RES_FIRE | MR_RES_COLD,
    0, 13, MONS_GOLD_MIMIC, MONS_GOLD_MIMIC, MH_NONLIVING, -3,
    { {AT_HIT, AF_POISON, 12}, {AT_HIT, AF_PLAIN, 12}, {AT_HIT, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 5, 0 },
    5, 1, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_NOTHING, SIZE_TINY,
    "An ancient parchment covered in arcane runes. Did it just twitch?"
}
,

{
    MONS_POTION_MIMIC, '!', BLACK, "mimic",
    M_NO_SKELETON,
    MR_RES_POISON | MR_RES_ELEC | MR_RES_FIRE | MR_RES_COLD,
    0, 13, MONS_GOLD_MIMIC, MONS_GOLD_MIMIC, MH_NONLIVING, -3,
    { {AT_HIT, AF_POISON, 12}, {AT_HIT, AF_PLAIN, 12}, {AT_HIT, AF_PLAIN, 12}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 5, 0 },
    5, 1, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_NORMAL, 
    MONUSE_NOTHING, SIZE_TINY,
    "A delicious looking magical drink. Go on, pick it up!"
}
,

{
    MONS_HELL_HOG, 'h', RED, "hell-hog",
    M_SPELLCASTER | M_THICK_SKIN | M_EVIL,
    MR_NO_FLAGS,
    0, 10, MONS_HELL_HOG, MONS_HELL_HOG, MH_DEMONIC, -3,
    { {AT_BITE, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 11, 3, 5, 0 },
    2, 9, 14, 7, MST_HELL_HOG, CE_CLEAN, Z_NOZOMBIE, S_SILENT, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_MEDIUM,
    "A large, fat and very ugly pig, suckled in the pits of Hell.  Roast suckling pig technology will never be the same."
}
,

{
    MONS_SERPENT_OF_HELL, 'D', RED, "Serpent of Hell",
    M_SPELLCASTER | M_FLIES | M_SENSE_INVIS | M_EVIL,
    MR_RES_POISON | MR_RES_HELLFIRE,
    0, 18, MONS_SERPENT_OF_HELL, MONS_SERPENT_OF_HELL, MH_DEMONIC, -13,
    { {AT_BITE, AF_PLAIN, 35}, {AT_CLAW, AF_PLAIN, 15}, {AT_CLAW, AF_PLAIN, 15}, {AT_NONE, AF_PLAIN, 0} },
    { 20, 4, 4, 0 },
    12, 9, 14, 7, MST_SERPENT_OF_HELL, CE_CLEAN, Z_NOZOMBIE, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_HUGE,
    "A huge red glowing dragon, burning with hellfire."
}
,

{
    MONS_BOGGART, 'g', DARKGREY, "boggart",
    M_SPELLCASTER | M_ACTUAL_SPELLS | M_SEE_INVIS,
    MR_NO_FLAGS,
    0, 14, MONS_BOGGART, MONS_BOGGART, MH_NATURAL, -7,
    { {AT_HIT, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 2, 3, 5, 0 },
    0, 12, 12, 7, MST_BOGGART, CE_CONTAMINATED, Z_SMALL, S_SHOUT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_LITTLE,
    "A twisted little sprite-goblin. Beware of its magical tricks!"
}
,

{
    MONS_QUICKSILVER_DRAGON, 'D', LIGHTCYAN, "quicksilver dragon",
    M_SPELLCASTER | M_FLIES | M_SENSE_INVIS,
    MR_NO_FLAGS,
    0, 14, MONS_DRAGON, MONS_QUICKSILVER_DRAGON, MH_NATURAL, -7,
    { {AT_BITE, AF_PLAIN, 25}, {AT_CLAW, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 16, 3, 5, 0 },
    10, 15, 15, 7, MST_QUICKSILVER_DRAGON, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_GIANT,
    "A long and sinuous dragon, seemingly more neck and tail than anything else. Its skin shines like molten mercury, and magical energies arc from its pointed snout."
}
,

{
    MONS_IRON_DRAGON, 'D', CYAN, "iron dragon",
    M_SPELLCASTER | M_SENSE_INVIS,
    MR_RES_POISON | MR_RES_FIRE | MR_RES_COLD,
    0, 14, MONS_DRAGON, MONS_IRON_DRAGON, MH_NATURAL, -7,
    { {AT_BITE, AF_PLAIN, 25}, {AT_CLAW, AF_PLAIN, 25}, {AT_CLAW, AF_PLAIN, 25}, {AT_NONE, AF_PLAIN, 0} },
    { 18, 5, 3, 0 },
    20, 6, 8, 7, MST_IRON_DRAGON, CE_CONTAMINATED, Z_SMALL, S_ROAR, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_HUGE,
    "A very heavy and apparently flightless dragon."
}
,

{
    MONS_SKELETAL_WARRIOR, 'z', CYAN, "skeletal warrior",
    M_FIGHTER | M_SPELLCASTER | M_ACTUAL_SPELLS | M_EVIL,
    MR_RES_POISON | MR_RES_COLD,
    0, 10, MONS_SKELETAL_WARRIOR, MONS_SKELETAL_WARRIOR, MH_UNDEAD, -7,
    { {AT_HIT, AF_PLAIN, 25}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 5, 3, 0 },
    15, 10, 10, 7, MST_SKELETAL_WARRIOR, CE_CONTAMINATED, Z_SMALL, S_SILENT, I_NORMAL, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "The vicious and heavily armed skeleton of a humanoid creature, animated by unholy power."
}
,


/* player ghost - only one per level. stats are stored in ghost struct */
{
    MONS_PLAYER_GHOST, 'p', DARKGREY, "",
    M_FIGHTER | M_SPEAKS | M_SPELLCASTER | M_ACTUAL_SPELLS | M_FLIES | M_UNIQUE,
    MR_RES_POISON,
    0, 15, MONS_PHANTOM, MONS_PLAYER_GHOST, MH_UNDEAD, -5,
    { {AT_HIT, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 4, 2, 3, 0 },
    1, 2, 10, 7, MST_GHOST, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_MEDIUM,
    ""
}
,

/* random demon in pan - only one per level. stats are stored in ghost struct */
{
    MONS_PANDEMONIUM_DEMON, '&', BLACK, "&",
    M_FIGHTER | M_SPELLCASTER | M_SPEAKS | M_EVIL,
    MR_RES_POISON,
    0, 14, MONS_PANDEMONIUM_DEMON, MONS_PANDEMONIUM_DEMON, MH_DEMONIC, -5,
    { {AT_HIT, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 4, 2, 3, 0 },
    1, 2, 10, 7, MST_GHOST, CE_CONTAMINATED, Z_NOZOMBIE, S_RANDOM, I_HIGH, 
    MONUSE_OPEN_DOORS, SIZE_LARGE,
    "You feel the a lump in the pit of your stomach.  "
}
,

// begin lava monsters {dlb}
{
    MONS_LAVA_WORM, 'w', RED, "lava worm",
    M_NO_FLAGS,
    MR_RES_FIRE | MR_VUL_COLD,
    0, 10, MONS_LAVA_WORM, MONS_LAVA_WORM, MH_NATURAL, -3,
    { {AT_BITE, AF_FIRE, 15}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 5, 0 },
    1, 10, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_SMALL, S_SILENT, I_ANIMAL_LIKE, 
    MONUSE_NOTHING, SIZE_LARGE,
    "A vicious red worm which swims through molten rock."
}
,

{
    MONS_LAVA_FISH, ';', RED, "lava fish",
    M_NO_FLAGS,
    MR_RES_FIRE | MR_VUL_COLD,
    0, 10, MONS_BIG_FISH, MONS_LAVA_FISH, MH_NATURAL, -3,
    { {AT_BITE, AF_FIRE, 10}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 4, 3, 5, 0 },
    4, 15, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_SMALL, S_SILENT, I_ANIMAL_LIKE, 
    MONUSE_NOTHING, SIZE_LITTLE,
    "A fish which lives in lava."
}
,

{
    MONS_LAVA_SNAKE, 'S', RED, "lava snake",
    M_SPECIAL_ABILITY,
    MR_RES_FIRE | MR_VUL_COLD,
    0, 10, MONS_SNAKE, MONS_LAVA_SNAKE, MH_NATURAL, -3,
    { {AT_BITE, AF_FIRE, 7}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 3, 5, 0 },
    2, 17, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_SMALL, S_HISS, I_ANIMAL_LIKE, 
    MONUSE_NOTHING, SIZE_MEDIUM,
    "A burning red snake which rears up from pools of lava and tries to bite you."
}
,

{  // mv: was another lava thing
    MONS_SALAMANDER, 'S', LIGHTRED, "salamander",
    M_FIGHTER | M_WARM_BLOOD,
    MR_RES_FIRE | MR_VUL_COLD,
    0, 10, MONS_SALAMANDER, MONS_SALAMANDER, MH_NATURAL, -3,
    { {AT_HIT, AF_FIRE, 23}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 14, 3, 5, 0 },
    5, 5, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_SMALL, S_SILENT, I_HIGH, 
    MONUSE_WEAPONS_ARMOUR, SIZE_MEDIUM,
    "A strange half-human half-snake creature covered in thick red scales and thorns."
}

,
// end lava monsters {dlb}

// begin water monsters {dlb}
{
    MONS_BIG_FISH, ';', LIGHTGREEN, "big fish",
    M_COLD_BLOOD,
    MR_NO_FLAGS,
    0, 10, MONS_BIG_FISH, MONS_BIG_FISH, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 8}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 4, 3, 5, 0 },
    1, 12, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_SMALL, S_SILENT, I_ANIMAL_LIKE, 
    MONUSE_NOTHING, SIZE_SMALL,
    "A fish of unusual size."
}
,

{
    MONS_GIANT_GOLDFISH, ';', LIGHTRED, "giant goldfish",
    M_COLD_BLOOD,
    MR_NO_FLAGS,
    0, 10, MONS_BIG_FISH, MONS_GIANT_GOLDFISH, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 15}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 5, 0 },
    5, 7, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_SMALL, S_SILENT, I_ANIMAL_LIKE, 
    MONUSE_NOTHING, SIZE_LITTLE,
    "This is what happens when you give your pet goldfish too much food!"
}
,

{
    MONS_ELECTRICAL_EEL, ';', LIGHTBLUE, "electrical eel",
    M_COLD_BLOOD | M_SPECIAL_ABILITY,
    MR_RES_ELEC,
    0, 10, MONS_ELECTRICAL_EEL, MONS_ELECTRICAL_EEL, MH_NATURAL, -3,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 3, 5, 0 },
    1, 15, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_SMALL, S_SILENT, I_ANIMAL_LIKE, 
    MONUSE_NOTHING, SIZE_SMALL,
    "A small and slimy eel, crackling with electrical discharge."
}
,

{
    MONS_JELLYFISH, 'J', CYAN, "jellyfish",
    M_NO_FLAGS,
    MR_RES_POISON,
    0, 10, MONS_JELLYFISH, MONS_JELLYFISH, MH_NATURAL, -3,
    { {AT_STING, AF_POISON_STR, 1}, {AT_HIT, AF_PLAIN, 1}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 4, 3, 5, 0 },
    0, 5, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_SMALL, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_LITTLE,
    "A pulsating glob of transparent flesh, waiting just below the surface to sting you with its many tentacles."
}
,

{
    MONS_WATER_ELEMENTAL, '{', LIGHTBLUE, "water elemental",
    M_FLIES,
    MR_RES_POISON | MR_VUL_FIRE | MR_RES_ELEC,
    0, 10, MONS_EARTH_ELEMENTAL, MONS_WATER_ELEMENTAL, MH_NONLIVING, MAG_IMMUNE,
    { {AT_HIT, AF_PLAIN, 25}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 5, 3, 0 },
    0, 7, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_OPEN_DOORS, SIZE_BIG,
    "A spirit drawn from the elemental plane of water. It exists on this world as part of a body of water."
}
,

{
    MONS_SWAMP_WORM, 'w', BROWN, "swamp worm",
    M_AMPHIBIOUS,
    MR_NO_FLAGS,
    0, 10, MONS_WORM, MONS_SWAMP_WORM, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 5, 5, 0 },
    3, 12, 12, 0, MST_NO_SPELLS, CE_NOCORPSE, Z_SMALL, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_LARGE,
    "A large slimy worm, adept at swimming through the muck of this foul swamp."
},
// end water monsters {dlb}

{
    MONS_WOLF, 'h', LIGHTGREY, "wolf",
    M_WARM_BLOOD | M_SENSE_INVIS,
    MR_NO_FLAGS,
    450, 10, MONS_HOUND, MONS_WOLF, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 8}, {AT_CLAW, AF_PLAIN, 2}, {AT_CLAW, AF_PLAIN, 2}, {AT_NONE, AF_PLAIN, 0} },
    { 4, 3, 5, 0 },
    3, 15, 17, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_BARK, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_SMALL,
    "A large and strong grey canine."
}
,

{
    MONS_WARG, 'h', DARKGREY, "warg",
    M_SENSE_INVIS | M_WARM_BLOOD,
    MR_RES_POISON,
    600, 12, MONS_HOUND, MONS_WARG, MH_NATURAL, -6,
    { {AT_BITE, AF_PLAIN, 12}, {AT_CLAW, AF_PLAIN, 3}, {AT_CLAW, AF_PLAIN, 3}, {AT_NONE, AF_PLAIN, 0} },
    { 4, 4, 5, 0 },
    4, 12, 13, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_SMALL, S_BARK, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_LARGE,
    "A particularly large and evil looking wolf, usually found in the company of orcs."
}
,

{
    MONS_BEAR, 'U', BROWN, "bear",
    M_WARM_BLOOD,
    MR_NO_FLAGS,
    2000, 10, MONS_BEAR, MONS_BEAR, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 10}, {AT_CLAW, AF_PLAIN, 6}, {AT_CLAW, AF_PLAIN, 6}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 3, 3, 0 },
    4, 4, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_GROWL, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_LARGE,
    "A common cave bear."
}
,

{
    MONS_GRIZZLY_BEAR, 'U', LIGHTGREY, "grizzly bear",
    M_WARM_BLOOD,
    MR_NO_FLAGS,
    2500, 10, MONS_BEAR, MONS_GRIZZLY_BEAR, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 12}, {AT_CLAW, AF_PLAIN, 8}, {AT_CLAW, AF_PLAIN, 8}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 4, 4, 0 },
    5, 8, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_GROWL, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_LARGE,
    "A large, nasty bear with grey fur."
}
,

{
    MONS_POLAR_BEAR, 'U', WHITE, "polar bear",
    M_WARM_BLOOD | M_AMPHIBIOUS,
    MR_RES_COLD,
    2500, 10, MONS_BEAR, MONS_POLAR_BEAR, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 20}, {AT_CLAW, AF_PLAIN, 5}, {AT_CLAW, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0} },
    { 7, 5, 3, 0 },
    7, 8, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_BIG, S_GROWL, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_BIG,
    "A large and very strong bear covered in glistening white fur"
}
,

{
    MONS_BLACK_BEAR, 'U', DARKGREY, "black bear",
    M_WARM_BLOOD,
    MR_NO_FLAGS,
    1800, 10, MONS_BEAR, MONS_BLACK_BEAR, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 4}, {AT_CLAW, AF_PLAIN, 4}, {AT_CLAW, AF_PLAIN, 4}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 3, 3, 0 },
    2, 8, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_GROWL, I_ANIMAL, 
    MONUSE_NOTHING, SIZE_MEDIUM,
    "A small black bear."
}
,

// small simulacrum
{
    MONS_SIMULACRUM_SMALL, 'z', WHITE, "",
    M_EVIL,
    MR_RES_POISON | MR_VUL_FIRE | MR_RES_COLD,
    0, 6, MONS_SIMULACRUM_SMALL, MONS_SIMULACRUM_SMALL, MH_UNDEAD, -1,
    { {AT_HIT, AF_PLAIN, 6}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 2, 3, 5, 0 },
    10, 4, 7, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_SMALL,
    "An ice replica of a monster that is animated by the powers of necromancy."
}
,

// large simulacrum
{
    MONS_SIMULACRUM_LARGE, 'Z', WHITE, "",
    M_EVIL,
    MR_RES_POISON | MR_VUL_FIRE | MR_RES_COLD,
    0, 6, MONS_SIMULACRUM_SMALL, MONS_SIMULACRUM_LARGE, MH_UNDEAD, -1,
    { {AT_HIT, AF_PLAIN, 14}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 3, 5, 0 },
    10, 5, 7, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_NOZOMBIE, S_SILENT, I_PLANT, 
    MONUSE_NOTHING, SIZE_LARGE,
    "An ice replica of a monster that is animated by the powers of necromancy."
}
,

{
    MONS_GIANT_NEWT, 'l', LIGHTGREEN, "giant newt",
    M_COLD_BLOOD | M_AMPHIBIOUS,
    MR_NO_FLAGS,
    150, 10, MONS_GIANT_LIZARD, MONS_GIANT_NEWT, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 3}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 1, 2, 0 },
    0, 15, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_REPTILE, 
    MONUSE_NOTHING, SIZE_TINY,
    "Several times the size of a normal newt, but still not really impressive."
}
,

{
    MONS_GIANT_GECKO, 'l', YELLOW, "giant gecko",
    M_COLD_BLOOD,
    MR_NO_FLAGS,
    250, 10, MONS_GIANT_LIZARD, MONS_GIANT_GECKO, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 5}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 1, 3, 5, 0 },
    1, 14, 12, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_SILENT, I_REPTILE, 
    MONUSE_NOTHING, SIZE_TINY,
    "A lizard with pads on its toes allowing it to cling to walls and ceilings.  It's much larger than a normal gecko... perhaps it's something in the water?"
}
,

{
    MONS_GIANT_IGUANA, 'l', BLUE, "giant iguana",
    M_COLD_BLOOD,
    MR_NO_FLAGS,
    400, 10, MONS_GIANT_LIZARD, MONS_GIANT_IGUANA, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 15}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 3, 3, 5, 0 },
    5, 9, 10, 7, MST_NO_SPELLS, CE_CLEAN, Z_SMALL, S_HISS, I_REPTILE, 
    MONUSE_NOTHING, SIZE_LITTLE,
    "A huge lizard with great crunching jaws."
}
,

{
    // gila monsters colours: lightmagenta, magenta, lightred, red, yellow
    MONS_GILA_MONSTER, 'l', BLACK, "gila monster",
    M_COLD_BLOOD,
    MR_NO_FLAGS,
    500, 10, MONS_GIANT_LIZARD, MONS_GILA_MONSTER, MH_NATURAL, -3,
    { {AT_BITE, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 5, 4, 4, 0 },
    3, 12, 10, 7, MST_NO_SPELLS, CE_POISONOUS, Z_BIG, S_HISS, I_REPTILE, 
    MONUSE_NOTHING, SIZE_LITTLE,
    "A large lizard with brightly coloured stripes and splotches."
}
,

{
    MONS_KOMODO_DRAGON, 'l', LIGHTRED, "komodo dragon",
    M_COLD_BLOOD | M_AMPHIBIOUS,
    MR_NO_FLAGS,
    800, 10, MONS_GIANT_LIZARD, MONS_KOMODO_DRAGON, MH_NATURAL, -3,
    { {AT_BITE, AF_DISEASE, 30}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 3, 5, 0 },
    7, 8, 10, 7, MST_NO_SPELLS, CE_CONTAMINATED, Z_BIG, S_HISS, I_REPTILE, 
    MONUSE_NOTHING, SIZE_MEDIUM,
    "An enormous monitor lizard.  It's more than capable of preying on large animals.  Bits of fetid and rotting flesh from its last few meals are stuck in its teeth."
}
,

{
    MONS_ORANGE_STATUE, '8', LIGHTRED, "orange crystal statue",
    M_SPECIAL_ABILITY,
    MR_RES_POISON | MR_RES_FIRE | MR_RES_COLD | MR_RES_ELEC,
    0, 10, MONS_CLAY_GOLEM, MONS_ORANGE_STATUE, MH_NONLIVING, MAG_IMMUNE,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 10, 0, 0, 160 },
    20, 1, 6, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_NOTHING, SIZE_LARGE,
    "An intricately carved statue of glittering orange crystal. Its eyes fix on yours with a piercing gaze."
}
,

{
    MONS_SILVER_STATUE, '8', WHITE, "silver statue",
    M_SPECIAL_ABILITY,
    MR_RES_POISON | MR_RES_FIRE | MR_RES_COLD | MR_RES_ELEC,
    0, 10, MONS_CLAY_GOLEM, MONS_SILVER_STATUE, MH_NONLIVING, MAG_IMMUNE,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 6, 0, 0, 150 },
    15, 1, 10, 7, MST_NO_SPELLS, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_NOTHING, SIZE_LARGE,
    "A beautiful statue of silver. Its eyes glow with an otherworldly radiance."
}
,

{
    MONS_ICE_STATUE, '8', LIGHTBLUE, "ice statue",
    M_SPELLCASTER,
    MR_RES_POISON | MR_VUL_FIRE | MR_RES_COLD | MR_RES_ELEC,
    0, 10, MONS_CLAY_GOLEM, MONS_ICE_STATUE, MH_NONLIVING, MAG_IMMUNE,
    { {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 8, 0, 0, 140 },
    12, 1, 10, 7, MST_ICE_STATUE, CE_NOCORPSE, Z_NOZOMBIE, S_SILENT, I_HIGH, 
    MONUSE_NOTHING, SIZE_LARGE,
    "An imposing statue carved in glistening ice. Freezing vapours swirl about it."
}
,

{
    MONS_MURRAY, 'z', LIGHTRED, "Murray",
    M_SPELLCASTER | M_SEE_INVIS | M_EVIL | M_SPEAKS | M_UNIQUE,
    MR_RES_ELEC | MR_RES_POISON | MR_RES_HELLFIRE | MR_RES_COLD,
    0, 50, MONS_LICH, MONS_CURSE_SKULL, MH_UNDEAD, MAG_IMMUNE,
    { {AT_BITE, AF_PLAIN, 20}, {AT_BITE, AF_PLAIN, 20}, {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 14, 0, 0, 180 },
    30, 10, 10, 7, MST_CURSE_SKULL, CE_NOCORPSE, Z_NOZOMBIE, S_MOAN, I_HIGH, 
    MONUSE_NOTHING, SIZE_TINY,
    "A demonic skull rolling along the dungeon floor."
}
,
{
    MONS_TIAMAT, 'd', MAGENTA, "Tiamat", 
    M_HUMANOID | M_SEE_INVIS | M_EVIL | M_UNIQUE | M_FLIES,
    MR_RES_POISON,
    0, 10, MONS_DRACONIAN, MONS_DRACONIAN, MH_NATURAL, -5,
    { {AT_HIT, AF_PLAIN, 35}, {AT_TAIL_SLAP, AF_PLAIN, 25},
      {AT_NONE, AF_PLAIN, 0}, {AT_NONE, AF_PLAIN, 0} },
    { 22, 0, 0, 200 },
    8, 10, 10, 10, MST_NO_SPELLS, CE_NOCORPSE, Z_SMALL, S_ROAR, I_HIGH, 
    MONUSE_STARTING_EQUIPMENT, SIZE_MEDIUM,
    "A powerful dragon with mighty wings.  Her scales seem to shimmer and change colour as you watch."
},

#endif
