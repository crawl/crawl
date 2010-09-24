/*
 *  File:       fineff.h
 *  Summary:    Brand/ench/etc effects that might alter something in an
 *              unexpected way.
 *  Written by: Adam Borowski
 */

#ifndef FINEFF_H
#define FINEFF_H

void add_final_effect(final_effect_flavour flavour,
                      actor *attacker,
                      actor *defender,
                      coord_def pos);
void fire_final_effects();

#endif
