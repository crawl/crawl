/*
 *  File:       spells1.h
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
 */


#ifndef SPELLS1_H
#define SPELLS1_H

bool cast_sure_blade(int power);

void cast_confusing_touch(int power);
void cast_cure_poison(int pow);
int allowed_deaths_door_hp(void);
void cast_deaths_door(int pow);
bool cast_revivification(int pow);
void cast_berserk(void);
void extension(int pow);
void cast_fly(int power);

void cast_insulation(int power);
void remove_regen(bool divine_ability = false);
void cast_regen(int pow, bool divine_ability = false);
void cast_resist_poison(int power);
void cast_swiftness(int power);
void cast_teleport_control(int power);
void deflection(int pow);
void remove_ice_armour();
void ice_armour(int pow, bool extending);
void missile_prot(int pow);

#endif
