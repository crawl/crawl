/*
 *  File:       view.h
 *  Summary:    Misc function used to render the dungeon.
 *  Written by: Linley Henzell
 */


#ifndef VIEW_H
#define VIEW_H

#include "externs.h"
#include "show.h"

void init_monsters_seens();

bool mons_near(const monsters *monster);
bool mon_enemies_around(const monsters *monster);

void find_features(const std::vector<coord_def>& features,
        unsigned char feature, std::vector<coord_def> *found);

bool magic_mapping(int map_radius, int proportion, bool suppress_msg,
                   bool force = false, bool deterministic = false,
                   bool circular = false,
                   coord_def origin = coord_def(-1, -1));
void reautomap_level();

bool is_feature(int feature, const coord_def& where);

void clear_feature_overrides();
void add_feature_override(const std::string &text);

std::string screenshot(bool fullscreen = false);

bool view_update();
void view_update_at(const coord_def &pos);
void flash_view(int colour = BLACK); // inside #ifndef USE_TILE?
void flash_view_delay(int colour = BLACK, long delay = 150);
#ifndef USE_TILE
void flash_monster_colour(const monsters *mon, unsigned char fmc_colour,
                          int fmc_delay);
#endif

void viewwindow(bool monster_updates, bool show_updates = true);
void update_monsters_in_view();
void handle_seen_interrupt(monsters* monster);
void flush_comes_into_view();

void toggle_show_terrain();
void reset_show_terrain();

void handle_terminal_resize(bool redraw = true);

#endif
