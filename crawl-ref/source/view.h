/**
 * @file
 * @brief Misc function used to render the dungeon.
**/

#pragma once

#include <vector>

#include "defines.h"
#include "options.h"
#include "player.h" // player_stealth()
#include "viewgeom.h"

using std::vector;

bool mon_enemies_around(const monster* mons);
void seen_monsters_react(int stealth = player_stealth());

string describe_monsters_condensed(const vector<monster*>& monsters);

bool magic_mapping(int map_radius, int proportion, bool suppress_msg,
                   bool force = false, bool deterministic = false,
                   bool full_info = false, bool range_falloff = true,
                   coord_def origin = coord_def(-1, -1),
                   bool respect_no_automap = false);

string screenshot();

colour_t viewmap_flash_colour();
bool view_update();
void view_update_at(const coord_def &pos);
class targeter;

static inline void scaled_delay(unsigned int ms)
{
    delay(ms * Options.view_delay / DEFAULT_VIEW_DELAY);
}

void animation_delay(unsigned int ms, bool do_refresh);

// Flashes the view until the player regains the ability to take actions
void flash_view(use_animation_type a, colour_t colour);
// Flashes the view for the given number of milliseconds
void flash_view_delay(use_animation_type a, colour_t colour, int delay,
                      targeter *where = nullptr);

enum animation_type
{
    ANIMATION_SHAKE_VIEWPORT,
    ANIMATION_BANISH,
    ANIMATION_ORB,
    NUM_ANIMATIONS
};

class animation
{
public:
    animation(): frames(10), frame_delay(50) { }
    virtual ~animation() { }

    virtual void init_frame(int frame)
    {
        UNUSED(frame);
    }
    virtual coord_def cell_cb(const coord_def &pos,
                              colour_t &colour) const = 0;

    int frames;
    int frame_delay;
};

/**
 * A renderer callback that can be injected into viewwindow(),
 * allowing the addition of new visual elements without
 * adding code directly to viewwindow() itself.
 */
class view_renderer
{
public:
    view_renderer() {}
    virtual ~view_renderer();

    virtual void render(crawl_view_buffer &vbuf, bool draw_tiles) = 0;
};

#ifdef USE_TILE
struct view_window_render_data
{
    coord_def saved_viewp;
    coord_def saved_vgrdc;

    animation* anim;
    view_renderer* renderer;
    targeter* flash_where;
    colour_t flash_colour;

    view_window_render_data() noexcept;
    void reset() noexcept;
    void update(animation* a, view_renderer* r, targeter* new_flash_where,
                colour_t new_flash_colour);
};

const view_window_render_data& current_view_window_render_data();
#endif

#ifdef USE_TILE
void view_add_tile_overlay(const coord_def &gc, tileidx_t tile);
#endif
void view_add_glyph_overlay(const coord_def &gc, cglyph_t glyph);

void flash_tile(coord_def p, colour_t colour = WHITE, int delay = 50,
                tileidx_t tile = 0);
void draw_ring_animation(const coord_def& center, int radius, colour_t colour,
                         colour_t colour_alt = BLACK, bool outward = false,
                         int delay = 50);

void view_clear_overlays();

void run_animation(animation_type anim, use_animation_type type,
                   bool cleanup = true);
void viewwindow(bool show_updates = true, bool tiles_only = false,
                animation *a = nullptr, view_renderer *renderer = nullptr,
                colour_t flash_colour = BLACK,
                targeter *flash_where = nullptr);
#ifdef USE_TILE
void render_view_window();
#endif
void draw_cell_glyph(glyph_screen_cell *cell, const coord_def &gc,
                     int flash_colour);
#ifdef USE_TILE
void draw_cell_tile(tile_screen_cell *cell, const coord_def &gc,
                    int flash_colour);
#endif

void update_monsters_in_view();
bool handle_seen_interrupt(monster* mons, vector<string>* msgs_buf = nullptr);
void flush_comes_into_view();
void toggle_show_terrain();
void reset_show_terrain();

void handle_terminal_resize();
