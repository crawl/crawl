/*
 *  File:       spells2.cc
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */

#ifndef SPELLS2_H
#define SPELLS2_H


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
bool brand_weapon(int which_brand, int power);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - spell
 * *********************************************************************** */
int animate_a_corpse(int axps, int ayps, int corps_beh, int corps_hit, int class_allowed);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - it_use3 - monstuff - mstuff2 - spell
 * *********************************************************************** */
int animate_dead(int power, int corps_beh, int corps_hit, int actual);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
char burn_freeze(int pow, char b_f);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
int corpse_rot(int power);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: it_use3 - spell
 * *********************************************************************** */
int summon_elemental(int pow, unsigned char restricted_type, unsigned char unfriendly);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
int vampiric_drain(int pow);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
unsigned char detect_creatures( int pow );


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
unsigned char detect_items( int pow );


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
unsigned char detect_traps( int pow ); 


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: item_use - spell
 * *********************************************************************** */
void cast_refrigeration(int pow);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: item_use - spell
 * *********************************************************************** */
void cast_toxic_radiance(void);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
void cast_twisted(int power, int corps_beh, int corps_hit);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability
 * *********************************************************************** */
void drain_life(int pow);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - spell
 * *********************************************************************** */
void holy_word(int pow);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - food - it_use2 - spell
 * returns TRUE if a stat was restored.
 * *********************************************************************** */
bool restore_stat(unsigned char which_stat, bool suppress_msg);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - spell
 * *********************************************************************** */
void summon_ice_beast_etc(int pow, int ibc);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
void summon_scorpions(int pow);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
void summon_small_mammals(int pow);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - religion - spell
 * *********************************************************************** */
bool summon_swarm( int pow, bool unfriendly, bool god_gift );


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
void summon_things(int pow);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
void summon_undead(int pow);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - spell
 * *********************************************************************** */
void turn_undead(int pow);      // what should I use for pow?


#endif
