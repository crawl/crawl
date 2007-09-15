/*
 *  File:       ouch.cc
 *  Summary:    Functions used when Bad Things happen to the player.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *     <2>     9/29/99     BCR     Added the DEATH_NAME_LENGTH define
 *                                 It determines how many characters of
 *                                 a long player name are printed in
 *                                 the score list.
 *     <1>     -/--/--     LRH     Created
 */


#ifndef OUCH_H
#define OUCH_H


#define DEATH_NAME_LENGTH 10

#include "enum.h"

enum kill_method_type
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
    KILLED_BY_SOMETHING = 27,
    KILLED_BY_FALLING_DOWN_STAIRS,
    KILLED_BY_ACID,
    KILLED_BY_CURARE,                  
    KILLED_BY_MELTING,
    KILLED_BY_BLEEDING,
    KILLED_BY_BEOGH_SMITING,

    NUM_KILLBY
};


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: bang - beam - effects - spells
 * *********************************************************************** */
int check_your_resists(int hurted, int flavour);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: fight
 * *********************************************************************** */
void splash_with_acid(char acid_strength);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: fight
 * *********************************************************************** */
void weapon_acid(char acid_strength);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - bang - beam - effects - fight - misc - spells -
 *              spells2
 * *********************************************************************** */
void scrolls_burn(char burn_strength, char target_class);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - bang - beam - command - effects - fight - misc -
 *              ouch - output - religion - spells - spells2 - spells4
 * *********************************************************************** */
void ouch(int dam, int death_source, kill_method_type death_type,
          const char *aux = NULL);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: decks - ouch
 * *********************************************************************** */
void lose_level(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: decks - fight - item_use - ouch - religion - spells
 * *********************************************************************** */
void drain_exp(void);

void expose_player_to_element( beam_type flavour, int strength = 0 );

#endif
