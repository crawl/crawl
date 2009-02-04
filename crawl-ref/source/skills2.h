/*
 *  File:       skills2.h
 *  Summary:    More skill related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef SKILLS2_H
#define SKILLS2_H

#define MAX_SKILL_ORDER         100

#include "enum.h"

// last_updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: chardump - it_use3 - itemname - skills
 * *********************************************************************** */
const char *skill_name(int which_skill);
int str_to_skill(const std::string &skill);

// last_updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: describe
 * *********************************************************************** */
std::string skill_title(
    unsigned char best_skill, unsigned char skill_lev,
    // these used for ghosts and hiscores:
    int species = -1, int str = -1, int dex = -1, int god = -1 );

// last_updated Sept 20 -- bwr
/* ***********************************************************************
 * called from: acr - chardump - player - skills - stuff
 * *********************************************************************** */
std::string player_title( void );


skill_type best_skill(int min_skill, int max_skill, int excl_skill = -1);

void init_skill_order( void );


// last_updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: acr - it_use2 - item_use - newgame - ouch - player - skills
 * *********************************************************************** */
int calc_mp(bool real_mp = false);


// last_updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - acr - food - it_use2 - misc - mutation -
 *              newgame - ouch - player - skills - spells1 - transfor
 * *********************************************************************** */
int calc_hp(bool real_hp = false);


// last_updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: newgame - skills - skills2
 * *********************************************************************** */
int species_skills(int skill, species_type species);


// last_updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: newgame - skills - skills2
 * *********************************************************************** */
unsigned int skill_exp_needed(int lev);


// last_updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void show_skills();


// last_updated 14jan2001 {gdl}
/* ***********************************************************************
 * called from: item_use
 * *********************************************************************** */
void wield_warning(bool newWeapon = true);


#endif
