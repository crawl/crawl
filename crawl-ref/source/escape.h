/**
 * @file
 * @brief Print a message about your character when escaping.
**/

#pragma once

#include <string>
#include "enum.h"
#include "god-type.h"
#include "player.h"
#include "skills.h"

void print_escape_message();

void print_win_message();

std::string win_messages_religion(god_type god = you.religion);

std::string win_messages_demigod(skill_type sk);


