/*
 *  File:       mon-cast.h
 *  Summary:    Monster spell casting.
 *  Written by: Linley Henzell
 */

#ifndef MONCAST_H
#define MONCAST_H

#include "enum.h"

class monsters;
class bolt;

bool handle_mon_spell(monsters *monster, bolt &beem);

bolt mons_spells(monsters *mons, spell_type spell_cast, int power);
void mons_cast(monsters *monster, bolt &pbolt, spell_type spell_cast,
               bool do_noise = true);
void mons_cast_noise(monsters *monster, bolt &pbolt, spell_type spell_cast);
void setup_mons_cast(monsters *monster, bolt &pbolt, spell_type spell_cast);

void mons_cast_haunt(monsters *monster);

#endif
