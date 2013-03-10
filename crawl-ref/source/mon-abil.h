/**
 * @file
 * @brief Monster abilities.
**/

#ifndef MONABIL_H
#define MONABIL_H

class monster;
struct bolt;

bool mon_special_ability(monster* mons, bolt & beem);
void mon_nearby_ability(monster* mons);

void draconian_change_colour(monster* drac);

bool ugly_thing_mutate(monster* ugly, bool proximity = false);
bool slime_creature_polymorph(monster* slime);

void ballisto_on_move(monster* mons, const coord_def & pos);
void activate_ballistomycetes(monster* mons, const coord_def & origin,
                              bool player_kill);

bool valid_kraken_connection(const monster* mons);
void move_child_tentacles(monster * kraken);
void move_demon_tentacle(monster * tentacle);

void ancient_zyme_sicken(monster* mons);
void starcursed_merge(monster* mon, bool forced);

bool has_push_space(const coord_def& pos, actor* act);
bool get_push_space(const coord_def& pos, coord_def& newpos,
                    actor* act, bool ignore_tension = false);

#endif
