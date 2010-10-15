/*
 *  File:       ng-setup.h
 *  Summary:    Setup "you" for a new game.
 */

#ifndef NG_SETUP_H
#define NG_SETUP_H

void give_basic_mutations(species_type speci);

void newgame_make_item(int slot, equipment_type eqslot,
                       object_class_type base,
                       int sub_type, int replacement = -1,
                       int qty = 1, int plus = 0, int plus2 = 0,
                       bool force_tutorial = false);

void newgame_give_item(object_class_type base, int sub_type,
                       int qty = 1, int plus = 0, int plus2 = 0);

struct newgame_def;
void setup_game(const newgame_def& ng);
void unfocus_stats();

#endif
