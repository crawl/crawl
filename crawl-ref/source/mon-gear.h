/**
 * @file
 * @brief Monsters' initial equipment.
**/

#pragma once

#include "item-def.h"
#include "monster-type.h"

class monster;

void give_specific_item(monster* mon, const item_def& tpl);
void give_specific_item(monster* mon, int thing);
void give_item(monster *mon, int level_number, bool mons_summoned,
               bool force_archer = false);
int make_mons_weapon(monster_type mtyp, int level,
                     bool melee_only = false, bool force_arcvher = false);
void give_weapon(monster *mon, int level_number);
int make_mons_armour(monster_type mtyp, int level);
void give_armour(monster *mon, int level_number);
void give_shield(monster *mon);
void view_monster_equipment(monster* mon);
bool is_melee_only(monster_type mt);

void give_apostle_equipment(monster* apostle);
