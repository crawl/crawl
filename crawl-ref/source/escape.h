/**
 * @file
 * @brief Print a message about your character when escaping.
**/

#pragma once

#include <string>
#include "enum.h"

void test_blank_function();

std::string win_messages_religion(god_type god = you.religion);
std::string win_messages_demigod(skill_type sk);


