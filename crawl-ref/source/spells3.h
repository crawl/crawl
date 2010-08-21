/*
 *  File:       spells3.h
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
 */


#ifndef SPELLS3_H
#define SPELLS3_H

#include "itemprop-enum.h"

class dist;
struct bolt;

int airstrike(int pow, const dist &beam);
bool cast_bone_shards(int power, bolt &);

bool cast_selective_amnesia(bool force);
bool cast_smiting(int power, const coord_def& where);
bool entomb(int pow);
bool cast_imprison(int pow, monsters *monster, int source);
bool project_noise();
bool detect_curse(int scroll, bool suppress_msg);

bool recall(int type_recalled);
bool remove_curse(bool suppress_msg);
bool cast_sublimation_of_blood(int pow);

bool cast_death_channel(int pow, god_type god = GOD_NO_GOD);

#endif
