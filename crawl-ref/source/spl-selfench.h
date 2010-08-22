#ifndef SPL_SELFENCH_H
#define SPL_SELFENCH_H

int allowed_deaths_door_hp(void);
void cast_deaths_door(int pow);
void extension(int pow);
void remove_ice_armour();
void ice_armour(int pow, bool extending);
void missile_prot(int pow);
void deflection(int pow);

void remove_regen(bool divine_ability = false);
void cast_regen(int pow, bool divine_ability = false);
void cast_berserk(void);

void cast_swiftness(int power);
void cast_fly(int power);
void cast_insulation(int power);

void cast_resist_poison(int power);
void cast_teleport_control(int power);
bool cast_selective_amnesia(bool force);
void cast_see_invisible(int pow);
void cast_silence(int pow);

#endif
