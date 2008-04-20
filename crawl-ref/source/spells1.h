/*
 *  File:       spells1.h
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef SPELLS1_H
#define SPELLS1_H


#include "externs.h"
#include "directn.h"

struct bolt;

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
int cast_healing(int pow, int target_x = -1, int target_y = -1);
void cast_revitalisation(int pow);

// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: beam - it_use3 - spells - spells1
 * *********************************************************************** */
void big_cloud(cloud_type cl_type, kill_category whose, int cl_x, int cl_y,
               int pow, int size, int spread_rate = -1);


// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: acr (WIZARD only) - item_use - spell
 * *********************************************************************** */
int blink(int pow, bool high_level_controlled_blink,
          bool wizard_blink = false);


/* ***********************************************************************
 * called from: spell
 * *********************************************************************** */
int cast_big_c(int pow, cloud_type cty, kill_category whose, bolt &beam);
void cast_confusing_touch(int power);
void cast_cure_poison(int mabil);
int  allowed_deaths_door_hp(void);
void cast_deaths_door(int pow);
int cast_fire_storm(int powc, bolt &beam);
bool cast_revivification(int pow);
void cast_berserk(void);
void cast_ring_of_flames(int power);
bool conjure_flame(int pow);
void extension(int pow);
int fireball(int power, bolt &beam);
int stinking_cloud(int pow, bolt &beam);
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
void antimagic();

// last updated 24may2000 {dlb}
/* ***********************************************************************
 * called from: acr (WIZARD only) - item_use - spell
 * *********************************************************************** */
void identify(int power, int item_slot = -1);


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
void random_blink(bool, bool override_abyss = false);


#endif
