/**
 * @file
 * @brief Chimeric beasties
**/

#ifndef MONCHIMERA_H
#define MONCHIMERA_H

#include "debug.h"

#define NUM_CHIMERA_HEADS 3

#define ASSERTPART(partnum)                                       \
    ASSERTM(is_valid_chimera_part(parts[partnum]),                \
            "Invalid chimera part %d: %s",                        \
            partnum, mons_class_name(parts[partnum]))

void define_chimera(monster* mon, monster_type parts[]);
bool define_chimera_for_place(monster *mon, level_id place, monster_type chimera_type, coord_def pos);
monster_type chimera_part_for_place(level_id place, monster_type chimera_type);

monster_type get_chimera_part(const monster* mon, int partnum);
monster_type get_chimera_part(const monster_info* mi, int partnum);
monster_type random_chimera_part(const monster* mon);
monster_type get_chimera_wings(const monster* mon);
monster_type get_chimera_legs(const monster* mon);
string chimera_part_names(monster_info mi);
bool chimera_is_batty(const monster* mon);

#endif
