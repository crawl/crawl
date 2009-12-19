/*
 *  File:       mon-project.cc
 *  Summary:    Slow projectiles, done as monsters.
 *  Written by: Adam Borowski
 */

#ifndef MON_PROJECT_H
#define MON_PROJECT_H

#include "beam.h"

bool mons_is_projectile(monster_type mt);

bool cast_iood(actor *caster, int pow, bolt *beam);
bool iood_act(monsters &mon);

#endif
