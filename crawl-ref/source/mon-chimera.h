/**
 * @file
 * @brief Chimeric beasties
**/

#ifndef MONCHIMERA_H
#define MONCHIMERA_H

void make_chimera(monster* mon, monster_type part1, monster_type part2, monster_type part3);
monster_type get_chimera_part(const monster* mon, int partnum);
monster_type get_chimera_part(const monster_info* mi, int partnum);
monster_type get_chimera_wings(const monster* mon);
string chimera_part_names(monster_info mi);
bool chimera_is_batty(const monster* mon);

#endif
