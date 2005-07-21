/*
 *  File:       religion.cc
 *  Summary:    Misc religion related functions.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef RELIGION_H
#define RELIGION_H

#include "enum.h"

// last updated 03jun2000 {dlb}
/* ***********************************************************************
 * called from: ouch - religion
 * *********************************************************************** */
void simple_god_message( const char *event, int which_deity = GOD_NO_GOD );


// last updated 11jan2001 {mv}
/* ***********************************************************************
 * called from: chardump - overmap - religion
 * *********************************************************************** */
char *god_name(int which_god,bool long_name=false); //mv


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: religion - spell
 * *********************************************************************** */
void dec_penance(int val);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: acr - decks - fight - player - religion - spell
 * *********************************************************************** */
void Xom_acts(bool niceness, int sever, bool force_sever);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: beam - decks - fight - religion
 * *********************************************************************** */
void done_good(char thing_done, int pgain);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - religion
 * *********************************************************************** */
void excommunication(void);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: acr - religion - spell
 * *********************************************************************** */
void gain_piety(char pgn);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell - religion
 * *********************************************************************** */
void god_speaks( int god, const char *mesg );


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - religion
 * *********************************************************************** */
void lose_piety(char pgn);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: acr - beam - fight - it_use2 - item_use - religion - spell -
 *              spellbook - spells4
 * *********************************************************************** */
void naughty(char type_naughty, int naughtiness);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: food
 * *********************************************************************** */
void offer_corpse(int corpse);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void pray(void);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: items
 * *********************************************************************** */
void handle_god_time(void);

// created 5jan2001 {mv}
/* ***********************************************************************
 * called from: message, describe
 * *********************************************************************** */
char god_colour(char god);

void god_pitch(unsigned char which_god);

#endif
