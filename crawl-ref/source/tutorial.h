/*
 *  File:       tutorial.h
 *  Summary:    Collection of tutorial related functions.
 */

#ifndef TUTORIAL_H
#define TUTORIAL_H

class map_def;
// All available maps.
std::vector<const map_def *> get_tutorial_maps();

// Set and get the current map. Used to transfer
// map choice from game choice to level gen.
void set_tutorial_map(const std::string& map);
std::string get_tutorial_map();

// Set player hunger from the tutorial map.
void set_tutorial_hunger(int hunger);
void set_tutorial_skill(const char *skill, int level);

void tutorial_death_message();
#endif
