/**
 * @file
 * @brief Monsters' initial equipment.
**/

#pragma once

#include "item-def.h"
#include "monster-type.h"

constexpr const char* UNIQUE_SCROLL_DROP_KEY = "unique_scroll_drop";
constexpr const char* UNIQUE_CURARE_DROP_KEY = "unique_curare_drop";
constexpr const char* UNIQUE_POTION_DROP_KEY = "unique_potion_drop";
constexpr const char* PEREGRINE_INVIS_POTIONS_KEY = "peregrine_invis_potions";
constexpr const char* UNIQUE_JEWELLERY_DROP_KEY = "unique_jewellery_drop";
constexpr const char* UNIQUE_PARCHMENT_DROP_KEY = "unique_parchment_drop";

class monster;

void give_specific_item(monster* mon, const item_def& tpl);
void give_specific_item(monster* mon, int thing);
void give_item(monster *mon, int level_number);
int make_mons_weapon(monster_type mtyp, int level, bool melee_only = false);
void give_weapon(monster *mon, int level_number);
int make_mons_armour(monster_type mtyp, int level);
void give_shield(monster *mon);
void view_monster_equipment(monster* mon);

void give_apostle_equipment(monster* apostle);
