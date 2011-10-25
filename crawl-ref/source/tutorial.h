/**
 * @file
 * @brief Collection of tutorial related functions.
**/

#ifndef TUTORIAL_H
#define TUTORIAL_H

// Set a few player attributes from the tutorial map.
void set_tutorial_hunger(int hunger);
void set_tutorial_skill(const char *skill, int level);
void tutorial_init_hint(const char* hintstr);

void tutorial_death_message();
#endif
