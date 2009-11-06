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

void beogh_follower_convert(monsters *monster, bool orc_hit = false);
void slime_convert(monsters *monster);
bool mons_near(const monsters *monster);
bool mon_enemies_around(const monsters *monster);

void find_features(const std::vector<coord_def>& features,
        unsigned char feature, std::vector<coord_def> *found);

bool magic_mapping(int map_radius, int proportion, bool suppress_msg,
                   bool force = false, bool deterministic = false,
                   bool circular = false,
                   coord_def origin = coord_def(-1, -1));
void reautomap_level();

bool noisy(int loudness, const coord_def& where, int who,
           bool mermaid = false);
bool noisy(int loudness, const coord_def& where, const char *msg = NULL,
           int who = -1, bool mermaid = false);
void blood_smell( int strength, const coord_def& where);
void handle_monster_shouts(monsters* monster, bool force = false);

class level_pos;
void show_map( level_pos &spec_place, bool travel_mode, bool allow_esc = false );
bool check_awaken(monsters* monster);
bool is_feature(int feature, const coord_def& where);
bool inside_level_bounds(int x, int y);
bool inside_level_bounds(const coord_def &p);
void clear_feature_overrides();
void add_feature_override(const std::string &text);

std::string screenshot(bool fullscreen = false);

bool view_update();
void view_update_at(const coord_def &pos);
#ifndef USE_TILE
void flash_monster_colour(const monsters *mon, unsigned char fmc_colour,
                          int fmc_delay);
#endif
void viewwindow(bool draw_it, bool do_updates);
void update_monsters_in_view();
void handle_seen_interrupt(monsters* monster);
void flush_comes_into_view();

void handle_terminal_resize(bool redraw = true);

#endif
