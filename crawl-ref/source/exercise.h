/**
 * @file
 * @brief Collects all calls to skills.cc:exercise for
 *            easier changes to the training modell.
**/

#ifndef EXERCISE_H
#define EXERCISE_H

#include "itemprop-enum.h" // missile_type

void practise_hitting(const item_def *weapon);
void practise_launching(const item_def &weapon);
void practise_throwing(missile_type mi_type);
void practise_stabbing();

void practise_using_ability(ability_type abil);
void practise_casting(spell_type spell, bool success);
void practise_evoking(int degree = 1);
void practise_using_deck();

void practise_being_hit();
void practise_being_attacked();
void practise_being_shot();
void practise_being_shot_at();
void practise_shield_block(bool successful = true);

void practise_sneaking(bool invis);
void practise_waiting();


#endif
