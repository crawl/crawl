/*
 *  File:       mstuff2.h
 *  Summary:    Misc monster related functions.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>     4/24/99        JDJ             mons_spells returns an
 *                                                      SBeam instead of using
 *                                                      func_pass.
 */


#ifndef MSTUFF2_H
#define MSTUFF2_H


#include <string>
#include "externs.h"


struct SBeam
{
    std::string name;
    int         colour;
    int         range;
    int         rangeMax;
    int         hit;
    dice_def    damage;
    int         ench_power;
    int         type;
    int         flavour;
    int         thrown;
    bool        isBeam;
};


/*
   beam_colour = _pass[0];
   beam_range = _pass[1];
   beam_damage = _pass[2];
   beam_hit = _pass[3];
   beam_type = _pass[4];
   beam_flavour = _pass[5];
   thing_thrown = _pass[6];
 */


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: monstuff - mstuff2
 * *********************************************************************** */
struct SBeam mons_spells(int spell_cast, int power);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: monstuff
 * *********************************************************************** */
void setup_dragon(struct monsters *monster, struct bolt &pbolt);


// last updated 13feb2001 {gdl}
/* ***********************************************************************
 * called from: monstuff
 * *********************************************************************** */
void mons_cast(struct monsters *monster, struct bolt &pbolt, int spell_cast);

// last updated 7jan2001 {gdl}
/* ***********************************************************************
 * called from: monstuff
 * *********************************************************************** */
void setup_mons_cast(struct monsters *monster, struct bolt &pbolt, int spell_cast);

// last updated 28july2000 (gdl)
/* ***********************************************************************
 * called from: monstuff
 * *********************************************************************** */
bool mons_throw(struct monsters *monster, struct bolt &pbolt, int hand_used);

// last updated 07jan2001 (gdl)
/* ***********************************************************************
 * called from: monstuff
 * *********************************************************************** */
void setup_generic_throw(struct monsters *monster, struct bolt &pbolt);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: monstuff
 * *********************************************************************** */
void mons_trap(struct monsters *monster);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: beam - fight - files - monstuff - mstuff2 - spells4
 * *********************************************************************** */
void monster_teleport(struct monsters *monster, bool instan);


// last updated Dec17,2000 -- gdl
/* ***********************************************************************
 * called from: monstuff
 * *********************************************************************** */
void spore_goes_pop(struct monsters *monster);


// last updated Jan14,2001 -- gdl
/* ***********************************************************************
 * called from: monstuff
 * *********************************************************************** */
void throw_type(int lnchClass, int lnchType, int wepClass, int wepType,
    bool &launched, bool &thrown);



#endif
