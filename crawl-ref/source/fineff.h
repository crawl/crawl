/*
 *  File:       fineff.h
 *  Summary:    Brand/ench/etc effects that might alter something in an
 *              unexpected way.
 *  Written by: Adam Borowski
 */

#ifndef FINEFF_H
#define FINEFF_H

void add_final_effect(final_effect_flavour flavour,
                      const actor *attacker = 0,
                      const actor *defender = 0,
                      coord_def pos = coord_def(0,0),
                      int x = 0);
void fire_final_effects();

#endif
