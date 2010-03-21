#ifndef AREAS_H
#define AREAS_H

void invalidate_agrid();

class actor;
void areas_actor_moved(const actor* act);

void create_sanctuary(const coord_def& center, int time);
bool remove_sanctuary(bool did_attack = false);
void decrease_sanctuary_radius();

bool silenced(const coord_def& p);

// Does the given point lie within a halo?
bool haloed(const coord_def& p);

#endif
