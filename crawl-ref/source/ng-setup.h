/**
 * @file
 * @brief Setup "you" for a new game.
**/

#ifndef NG_SETUP_H
#define NG_SETUP_H

#include "itemprop-enum.h"

void give_basic_mutations(species_type speci);
void autopickup_starting_ammo(missile_type missile);

void newgame_make_item(int slot, equipment_type eqslot,
                       object_class_type base,
                       int sub_type, int replacement = -1,
                       int qty = 1, int plus = 0, int plus2 = 0,
                       bool force_tutorial = false);

struct newgame_def;
void setup_game(const newgame_def& ng);
void unfocus_stats();
#endif
