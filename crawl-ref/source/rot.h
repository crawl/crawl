/**
 * @file
 * @brief Functions for blood & chunk rot.
 **/

#ifndef ROT_H
#define ROT_H

#include "externs.h"

void init_stack_blood_potions(item_def &stack, int age = -1);
void maybe_coagulate_blood_potions_floor(int obj);
void maybe_coagulate_blood_potions_inv(item_def &blood);
int remove_oldest_blood_potion(item_def &stack);
void remove_newest_blood_potion(item_def &stack, int quant = -1);
void merge_blood_potion_stacks(item_def &source, item_def &dest, int quant);

#endif
