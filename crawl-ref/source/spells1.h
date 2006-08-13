/*
 *  File:       spells1.cc
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef SPELLS1_H
#define SPELLS1_H


#include "externs.h"
#include "direct.h"


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
bool cast_sure_blade(int power);


#if 0
// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - spell
 * *********************************************************************** */
char cast_greater_healing(void);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability
 * *********************************************************************** */
char cast_greatest_healing(void);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - spell
 * *********************************************************************** */
char cast_lesser_healing(void);
#endif 

// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - spell
 * *********************************************************************** */
char cast_healing(int power);

// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: beam - it_use3 - spells - spells1
 * *********************************************************************** */
void big_cloud(char clouds, char cl_x, char cl_y, int pow, int size);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: acr (WIZARD only) - item_use - spell
 * *********************************************************************** */
void blink(void);


/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
void cast_big_c(int pow, char cty);
void cast_confusing_touch(int power);
void cast_cure_poison(int mabil);
int  allowed_deaths_door_hp(void);
void cast_deaths_door(int pow);
void cast_fire_storm(int powc);
bool cast_revivification(int power);
void cast_berserk(void);
void cast_ring_of_flames(int power);
void conjure_flame(int pow);
void extension(int pow);
void fireball(int power);
void stinking_cloud(int pow);
void abjuration(int pow);

// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - spell - spells1
 * *********************************************************************** */
void cast_fly(int power);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: spell - spells1
 * *********************************************************************** */
void cast_insulation(int power);
void cast_regen(int pow);
void cast_resist_poison(int power);
void cast_swiftness(int power);
void cast_teleport_control(int power);
void deflection(int pow);
void ice_armour(int pow, bool extending);
void missile_prot(int pow);
void stone_scales(int pow);

// last updated sept 18
/* ***********************************************************************
 * called from: religion
 * *********************************************************************** */
void antimagic(void);

// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: acr (WIZARD only) - item_use - spell
 * *********************************************************************** */
void identify(int power);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: acr - spells1
 * *********************************************************************** */
void manage_fire_shield(void);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - spell
 * *********************************************************************** */
void purification(void);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: ability - decks - fight - spell - spells - spells1
 * *********************************************************************** */
void random_blink(bool);


#endif
