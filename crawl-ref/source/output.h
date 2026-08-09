/**
 * @file
 * @brief Functions used to print player related info.
**/

#pragma once

struct item_def;

#ifdef DGL_SIMPLE_MESSAGING
void update_message_status();
#endif

void reset_hud();

void update_turn_count();

void print_stats();
void print_stats_level();
void draw_border();

int wielded_weapon_colour(const item_def &weapon);

#ifndef USE_TILE_LOCAL
void smallterm_warning();
#endif

void redraw_screen(bool show_updates = true);

string mpr_monster_list(bool past = false);
int update_monster_pane();

int equip_slot_by_name(const char *s);

int stealth_pips();

void print_overview_screen();

string dump_overview_screen();
