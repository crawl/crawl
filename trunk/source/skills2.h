/*
 *  File:       skills2.cc
 *  Summary:    More skill related functions.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <2>     -/--/--        WL              Extensive mods from Wladimir van der Laan.
 *               <1>     -/--/--        LRH             Created
 */


#ifndef SKILLS2_H
#define SKILLS2_H

#include <stddef.h>  // For NULL

#define MAX_SKILL_ORDER         100

// last_updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: chardump - it_use3 - itemname - skills
 * *********************************************************************** */
const char *skill_name(unsigned char which_skill);


// last_updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: describe
 * *********************************************************************** */
const char * skill_title( unsigned char best_skill, unsigned char skill_lev, 
                 // these used for ghosts and hiscores:
                 int species = -1, int str = -1, int dex = -1, int god = -1 );  

// last_updated Sept 20 -- bwr
/* ***********************************************************************
 * called from: acr - chardump - player - skills - stuff
 * *********************************************************************** */
const char *player_title( void );


// last_updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: acr - chardump - effects - files - player - skills -
 *              skills2 - stuff
 * *********************************************************************** */
unsigned char best_skill(unsigned char min_skill, unsigned char max_skill, unsigned char excl_skill);

void init_skill_order( void );


// last_updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: acr - it_use2 - item_use - newgame - ouch - player - skills
 * *********************************************************************** */
int calc_mp(void);


// last_updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - acr - food - it_use2 - misc - mutation -
 *              newgame - ouch - player - skills - spells1 - transfor
 * *********************************************************************** */
int calc_hp(void);


// last_updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: newgame - skills - skills2
 * *********************************************************************** */
int species_skills(char skill, char species);


// last_updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: newgame - skills - skills2
 * *********************************************************************** */
unsigned int skill_exp_needed(int lev);


// last_updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void show_skills(void);


// last_updated 14jan2001 {gdl}
/* ***********************************************************************
 * called from: item_use
 * *********************************************************************** */
void wield_warning(bool newWeapon = true); 


#endif
