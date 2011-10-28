#ifndef SPL_OTHER_H
#define SPL_OTHER_H

#include "spl-cast.h"

spret_type cast_cure_poison(int pow, bool fail);

spret_type cast_sublimation_of_blood(int pow, bool fail);
spret_type cast_death_channel(int pow, god_type god, bool fail);
spret_type cast_recall(bool fail);
bool recall(int type_recalled);

spret_type cast_phase_shift(int pow, bool fail = false);
spret_type cast_passwall(const coord_def& delta, int pow, bool fail);
spret_type cast_intoxicate(int pow, bool fail);
spret_type cast_fulsome_distillation(int pow, bool check_range, bool fail);
void remove_condensation_shield();
spret_type cast_condensation_shield(int pow, bool fail);
spret_type cast_stoneskin(int pow, bool fail = false);
spret_type cast_darkness(int pow, bool fail);

#endif
