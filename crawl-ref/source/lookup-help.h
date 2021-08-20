/**
 * @file
 * @brief Let the player search for descriptions of monsters, items, etc.
 **/

#pragma once

#include "lookup-help-type.h"

void keyhelp_query_descriptions();

string lookup_help_type_name(lookup_help_type lht);
char lookup_help_type_shortcut(lookup_help_type lht);
bool find_description_of_type(lookup_help_type lht);
