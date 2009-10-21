/*
 * File:      l_defs.h
 * Summary:   Functions defined in the Lua bindings but used elsewhere.
 *
 * TODO: Move these where they belong.
 */

#ifndef L_DEFS_H
#define L_DEFS_H

dungeon_feature_type dungeon_feature_by_name(const std::string &name);
std::vector<std::string> dungeon_feature_matches(const std::string &name);
const char *dungeon_feature_name(dungeon_feature_type feat);
std::string dgn_set_default_depth(const std::string &s);
void dgn_reset_default_depth();

#endif

