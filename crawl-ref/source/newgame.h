/*
 *  File:       newgame.h
 *  Summary:    Functions used when starting a new game.
 *  Written by: Linley Henzell
 */


#ifndef NEWGAME_H
#define NEWGAME_H

#include "newgame_def.h"

undead_state_type get_undead_state(const species_type sp);

bool choose_game(newgame_def *ng, newgame_def* choice,
                 const newgame_def& defaults);

void make_rod(item_def &item, stave_type rod_type, int ncharges);
int start_to_wand(int wandtype, bool& is_rod);
int start_to_book(int firstbook, int booktype);

#endif
