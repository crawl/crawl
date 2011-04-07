/**
 * @file
 * @brief Functions related to special abilities.
**/


#ifndef ABLSHOW_H
#define ABLSHOW_H

#include "enum.h"

#include <string>
#include <vector>

struct talent
{
    ability_type which;
    int hotkey;
    int fail;
    bool is_invocation;
};

const std::string make_cost_description(ability_type ability);
const char* ability_name(ability_type ability);
std::vector<const char*> get_ability_names();
int choose_ability_menu(const std::vector<talent>& talents);

bool activate_ability();
std::vector<talent> your_talents(bool check_confused);
bool string_matches_ability_name(const std::string& key);
std::string print_abilities(void);

void set_god_ability_slots(void);


#endif
