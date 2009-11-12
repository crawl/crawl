/*
 *  File:       mon-abil.h
 *  Summary:    Monster abilities.
 *  Written by: Linley Henzell
 */

#ifndef MONABIL_H
#define MONABIL_H

class monsters;
class bolt;

bool mon_special_ability(monsters *monster, bolt & beem);
void mon_nearby_ability(monsters *monster);

bool ugly_thing_mutate(monsters *ugly, bool proximity = false);
bool slime_split_merge(monsters *thing);

void ballisto_on_move(monsters * monster, const coord_def & pos);

#endif
