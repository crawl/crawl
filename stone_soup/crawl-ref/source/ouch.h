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
void ouch(int dam, int death_source, char death_type, const char *aux = NULL);


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


#endif
