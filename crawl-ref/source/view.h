/**
 * @file
 * @brief Misc function used to render the dungeon.
**/

#ifndef VIEW_H
#define VIEW_H

#include "viewgeom.h"

void init_monsters_seens();

bool mons_near(const monster* mons);
bool mon_enemies_around(const monster* mons);
void seen_monsters_react();

bool magic_mapping(int map_radius, int proportion, bool suppress_msg,
                   bool force = false, bool deterministic = false,
                   coord_def origin = coord_def(-1, -1));
void reautomap_level();
void fully_map_level();

string screenshot();

int viewmap_flash_colour();
bool view_update();
void view_update_at(const coord_def &pos);
class targetter;
// beware, flash_view is broken for USE_TILE_LOCAL
void flash_view(colour_t colour, targetter *where = NULL);
void flash_view_delay(colour_t colour, int delay, targetter *where = NULL);
#ifndef USE_TILE_LOCAL
void flash_monster_colour(const monster* mon, colour_t fmc_colour,
                          int fmc_delay);
#endif

void viewwindow(bool show_updates = true, bool tiles_only = false);
void draw_cell(screen_cell_t *cell, const coord_def &gc,
               bool anim_updates, int flash_colour);

void update_monsters_in_view();
bool handle_seen_interrupt(monster* mons, vector<string>* msgs_buf = NULL);
void flush_comes_into_view();

void toggle_show_terrain();
void reset_show_terrain();

void handle_terminal_resize(bool redraw = true);

#endif
