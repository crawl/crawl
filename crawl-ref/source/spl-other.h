#ifndef SPL_OTHER_H
#define SPL_OTHER_H

void cast_cure_poison(int pow);

bool cast_sublimation_of_blood(int pow);
bool cast_death_channel(int pow, god_type god = GOD_NO_GOD);
bool recall(int type_recalled);

void cast_phase_shift(int pow);
bool cast_passwall(const coord_def& delta, int pow);
void cast_intoxicate(int pow);
bool cast_fulsome_distillation(int pow, bool check_range = true);
void remove_condensation_shield();
void cast_condensation_shield(int pow);
void cast_stoneskin(int pow);

#endif
