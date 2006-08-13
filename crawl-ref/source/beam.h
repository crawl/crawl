/*
 *  File:       beam.cc
 *  Summary:    Functions related to ranged attacks.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef BEAM_H
#define BEAM_H


#include "externs.h"

dice_def calc_dice( int num_dice, int max_damage );


/* ***********************************************************************
 * called from: bang - it_use2 - monstuff - mstuff2
 * *********************************************************************** */
void fire_beam( struct bolt &pbolt, item_def *item = NULL );

// last updated 19apr2001 {gdl}
/* ***********************************************************************
 * called from: beam
 * *********************************************************************** */
bool nasty_beam( struct monsters *mon, struct bolt &beam );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: ability - it_use3 - item_use - mstuff2 - religion -
 *              spells - spells4
 * *********************************************************************** */
void explosion( struct bolt &pbolt, bool hole_in_the_middle = false );


// last updated 22jan2001 {gdl}
/* ***********************************************************************
 * called from: effects - spells2 - spells4
 * *********************************************************************** */
int mons_adjust_flavoured( struct monsters *monster, struct bolt &pbolt,
                           int hurted, bool doFlavouredEffects = true );


/* ***********************************************************************
 * called from: ability - item_use - spell
 * returns true if messages were generated during the enchantment
 * *********************************************************************** */
bool mass_enchantment( int wh_enchant, int pow, int who );


/* ***********************************************************************
 * called from: fight - monstuff - mstuff2
 * *********************************************************************** */
int mons_ench_f2( struct monsters *monster, struct bolt &pbolt );


/* ***********************************************************************
 * called from: fight - monstuff - spells2
 * *********************************************************************** */
void poison_monster( struct monsters *monster, bool fromPlayer, int levels = 1,
                     bool force = false );


/* ***********************************************************************
 * called from: fight - monstuff - spells - spells1 - spells2
 * *********************************************************************** */
#if 0
void delete_cloud( int cloud );
void new_cloud( int cloud, int type, int x, int y, int decay );

void place_cloud(unsigned char cl_type, unsigned char ctarget_x, unsigned char ctarget_y, unsigned char cl_range);
#endif


/* ***********************************************************************
 * called from: monstuff
 * *********************************************************************** */
void fire_tracer( struct monsters *monster, struct bolt &pbolt );


/* ***********************************************************************
 * called from: monstuff
 * *********************************************************************** */
void mimic_alert( struct monsters *mimic );


void zapping( char ztype, int power, struct bolt &pbolt );

#endif
