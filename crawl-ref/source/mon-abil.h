/*
 *  File:       mon-abil.h
 *  Summary:    Monster abilities.
 *  Written by: Linley Henzell
 */

#ifndef MONABIL_H
#define MONABIL_H

class monster;
struct bolt;

bool mon_special_ability(monster* mons, bolt & beem);
void mon_nearby_ability(monster* mons);

void draconian_change_colour(monster* drac);

bool ugly_thing_mutate(monster* ugly, bool proximity = false);

void ballisto_on_move(monster* mons, const coord_def & pos);
void activate_ballistomycetes(monster* mons, const coord_def & origin,
                              bool player_kill);

bool valid_kraken_connection(monster* mons);
void move_kraken_tentacles(monster * kraken);
bool valid_demonic_connection(monster* mons);
void move_demon_tentacle(monster * tentacle);

#endif
