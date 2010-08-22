/*
 *  File:       spells4.h
 *  Summary:    Yet More Spell Function Declarations
 *  Written by: Josh Fishman
 */


#ifndef SPELLS4_H
#define SPELLS4_H

#include "externs.h"

class dist;

bool backlight_monsters(coord_def where, int pow, int garbage);
void cast_mass_sleep(int pow);
void cast_see_invisible(int pow);
void cast_silence(int pow);
void cast_tame_beasts(int pow);

//returns true if it slowed the monster
bool do_slow_monster(monsters* mon, kill_category whose_kill);

#endif
