/**
 * @file
 * @brief Monster explosion functionality.
**/

#pragma once

#include "killer-type.h"

class monster;
struct bolt;
struct dice_def;

bool monster_explodes(const monster &mons);
bool mon_explodes_on_death(monster_type mc);
dice_def mon_explode_dam(monster_type mc, int hd);
void setup_spore_explosion(bolt & beam, const monster& origin);
bool explode_monster(monster* mons, killer_type killer, bool pet_kill);
dice_def ball_lightning_damage(int hd, bool random = true);
dice_def prism_damage(int hd, bool fully_powered);
