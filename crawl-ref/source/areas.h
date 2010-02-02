#ifndef AREAS_H
#define AREAS_H

void create_sanctuary(const coord_def& center, int time);
bool remove_sanctuary(bool did_attack = false);
void decrease_sanctuary_radius();

bool silenced(const coord_def& p);

// Actors within whose halo the given point is.
std::list<actor*> haloers(const coord_def &c);

// Does the given point lie within a halo?
bool haloed(const coord_def& p);

#endif
