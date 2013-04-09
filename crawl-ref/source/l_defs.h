/**
 * @file
 * @brief Functions defined in the Lua bindings but used elsewhere.
**/
// TODO: Move these where they belong.

#ifndef L_DEFS_H
#define L_DEFS_H

dungeon_feature_type dungeon_feature_by_name(const string &name);
vector<string> dungeon_feature_matches(const string &name);
string dgn_set_default_depth(const string &s);
void dgn_reset_default_depth();
bool in_show_bounds(const coord_def &c);
coord_def player2grid(const coord_def &p);

#endif
