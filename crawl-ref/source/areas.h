#ifndef AREAS_H
#define AREAS_H

void create_sanctuary(const coord_def& center, int time);
bool remove_sanctuary(bool did_attack = false);
void decrease_sanctuary_radius();

bool silenced(const coord_def& p);

#endif

