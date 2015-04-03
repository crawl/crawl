/**
 * @file
 * @brief Chimeric beasties
**/

#ifndef MONCHIMERA_H
#define MONCHIMERA_H

#define NUM_CHIMERA_HEADS 3

#define CHIMERA_BATTY_KEY "chimera_batty"
#define CHIMERA_WING_KEY "chimera_wings"
#define CHIMERA_LEGS_KEY "chimera_legs"
#define CHIMERA_PT2_KEY "chimera_part_2"
#define CHIMERA_PT3_KEY "chimera_part_3"

#define ASSERTPART(partnum)                                       \
    ASSERTM(is_valid_chimera_part(parts[partnum]),                \
            "Invalid chimera part %d: %s",                        \
            partnum, mons_class_name(parts[partnum]))

void define_chimera(monster* mon, monster_type parts[]);
monster_type chimera_part_for_place(level_id place, monster_type chimera_type);

monster_type get_chimera_part(const monster* mon, int partnum);
monster_type get_chimera_part(const monster_info* mi, int partnum);
monster_type random_chimera_part(const monster* mon);
monster_type get_chimera_wings(const monster* mon);
monster_type get_chimera_legs(const monster* mon);
bool chimera_is_batty(const monster* mon);

#endif
