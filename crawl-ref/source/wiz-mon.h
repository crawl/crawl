/**
 * @file
 * @brief Monster related wizard functions.
**/

#pragma once

#ifdef WIZARD

void wizard_create_spec_monster_name();
void wizard_spawn_control();
void wizard_detect_creatures();
void wizard_dismiss_all_monsters(bool force_all = false);
void debug_list_monsters();
void debug_stethoscope(int mon);
void debug_miscast(int target);
void debug_ghosts();

class monster;
struct coord_def;

void wizard_gain_monster_level(monster* mon);
void wizard_apply_monster_blessing(monster* mon);
void wizard_give_monster_item(monster* mon);
void wizard_move_player_or_monster(const coord_def& where);
void wizard_make_monster_summoned(monster* mon);
void wizard_polymorph_monster(monster* mon);
void debug_make_monster_shout(monster* mon);

void debug_pathfind(int mid);

#endif

