/**
 * @file
 * @brief Misc function used to render the dungeon.
**/

#include "AppHdr.h"

#include "view.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <sstream>

#include "branch.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "dgn-overview.h"
#include "tile-env.h"
#include "god-conduct.h"
#include "god-passive.h"
#include "hints.h"
#include "macro.h"
#include "map-knowledge.h"
#include "message.h"
#include "religion.h"
#include "showsymb.h"
#include "tag-version.h"
#include "tilemcache.h"
#ifdef USE_TILE
 #include "tile-flags.h"
 #include "tilepick.h"
 #include "tilepick-p.h"
 #include "tileview.h"
#endif
#include "tiles-build-specific.h"
#include "traps.h"
#include "ui.h"
#include "unicode.h"
#include "viewmap.h"

static layers_type _layers = LAYERS_ALL;
static layers_type _layers_saved = Layer::None;

crawl_view_geometry crawl_view;

// Returns a string containing a representation of the map. Leading and
// trailing spaces are trimmed from each line. Leading and trailing empty
// lines are also snipped.
string screenshot()
{
    vector<string> lines(crawl_view.viewsz.y);
    unsigned int lsp = GXM;
    for (int y = 0; y < crawl_view.viewsz.y; y++)
    {
        string line;
        for (int x = 0; x < crawl_view.viewsz.x; x++)
        {
            // in grid coords
            const coord_def gc = view2grid(crawl_view.viewp +
                                     coord_def(x, y));
            char32_t ch =
                  (!map_bounds(gc))             ? ' ' :
                  (gc == you.pos())             ? mons_char(you.symbol)
                                                : get_cell_glyph(gc).ch;
            line += stringize_glyph(ch);
        }
        // right-trim the line
        for (int x = line.length() - 1; x >= 0; x--)
            if (line[x] == ' ')
                line.erase(x);
            else
                break;
        // see how much it can be left-trimmed
        for (unsigned int x = 0; x < line.length(); x++)
            if (line[x] != ' ')
            {
                if (lsp > x)
                    lsp = x;
                break;
            }
        lines[y] = line;
    }

    for (string &line : lines)
        line.erase(0, lsp);     // actually trim from the left
    while (!lines.empty() && lines.back().empty())
        lines.pop_back();       // then from the bottom

    ostringstream ss;
    unsigned int y = 0;
    for (y = 0; y < lines.size() && lines[y].empty(); y++)
        ;                       // ... and from the top
    for (; y < lines.size(); y++)
        ss << lines[y] << "\n";
    return ss.str();
}

colour_t viewmap_flash_colour()
{
    // This only shows the fullscreen colour when we're showing all layers,
    // and hides it for parseability's sake when toggling individual layers.
    if ((_layers & LAYERS_ALL) != LAYERS_ALL)
        return BLACK;

    if (you.berserk())
        return RED;
    else if (you.paralysed())
        return LIGHTBLUE;
    else if (you.petrified())
        return LIGHTGRAY;
    else if (you.duration[DUR_VEXED])
        return MAGENTA;
    else if (you.duration[DUR_DAZED])
        return YELLOW;

    return BLACK;
}

// Updates one square of the view area. Should only be called for square
// in LOS.
void view_update_at(const coord_def &pos)
{
    if (pos == you.pos())
        return;

    show_update_at(pos);
#ifdef USE_TILE
    tile_draw_map_cell(pos, true);
#endif
#ifdef USE_TILE_WEB
    tiles.mark_for_redraw(pos);
#endif

#ifndef USE_TILE_LOCAL
    if (!env.map_knowledge(pos).visible())
        return;
    cglyph_t g = get_cell_glyph(pos);

    int flash_colour = you.flash_colour == BLACK
        ? viewmap_flash_colour()
        : you.flash_colour;
    monster_type mons = env.map_knowledge(pos).monster();
    int cell_colour =
        flash_colour &&
        (mons == MONS_NO_MONSTER || mons_class_is_firewood(mons))
            ? real_colour(flash_colour, pos)
            : g.col;

    const coord_def vp = grid2view(pos);
    // Don't draw off-screen.
    if (crawl_view.in_viewport_v(vp))
    {
        cgotoxy(vp.x, vp.y, GOTO_DNGN);
        put_colour_ch(cell_colour, g.ch);
    }

    // Force colour back to normal, else clrscr() will flood screen
    // with this colour on DOS.
    textcolour(LIGHTGREY);

#endif
#ifdef USE_TILE_WEB
    tiles.mark_for_redraw(pos);
#endif
}

bool view_update()
{
    if (you.num_turns > you.last_view_update)
    {
        viewwindow();
        update_screen();
        return true;
    }
    return false;
}

void animation_delay(unsigned int ms, bool do_refresh)
{
    // this leaves any Options.use_animations & UA_BEAM checks to the caller;
    // but maybe it could be refactored into here
    if (do_refresh)
    {
        viewwindow(false);
        update_screen();
    }
    scaled_delay(ms);
}

static void _flash_view(colour_t colour, targeter* where)
{
#ifndef USE_TILE_LOCAL
    save_cursor_pos save;
#endif

    you.flash_colour = colour;
    you.flash_where = where;
    viewwindow(false);
    update_screen();
}

void flash_view(use_animation_type a, colour_t colour, targeter *where)
{
    if (crawl_state.need_save && Options.use_animations & a)
    {
        _flash_view(colour, where);
#ifdef USE_TILE
        tiles.redraw();
#endif
    }
}

void flash_view_delay(use_animation_type a, colour_t colour, int flash_delay,
                      targeter *where)
{
    if (crawl_state.need_save && Options.use_animations & a)
    {
        _flash_view(colour, where);
        scaled_delay(flash_delay);
        _flash_view(BLACK, nullptr);
    }
}

static void _do_explore_healing()
{
    // Full heal in, on average, 420 tiles. (270 for MP.)
    const int healing = div_rand_round(random2(you.hp_max), 210);
    inc_hp(healing);
    const int mp = div_rand_round(random2(you.max_magic_points), 135);
    inc_mp(mp);
}

enum class update_flag
{
    affect_excludes = (1 << 0),
    added_exclude   = (1 << 1),
};
DEF_BITFIELD(update_flags, update_flag);

// Do various updates when the player sees a cell. Returns whether
// exclusion LOS might have been affected.
static update_flags player_view_update_at(const coord_def &gc)
{
    maybe_remove_autoexclusion(gc);
    update_flags ret;

    // Set excludes in a radius of 1 around harmful clouds generated
    // by neither monsters nor the player.
    const cloud_struct* cloud = cloud_at(gc);
    if (cloud && !crawl_state.game_is_arena())
    {
        const cloud_struct &cl = *cloud;

        bool did_exclude = false;
        if (!cl.temporary() && cloud_damages_over_time(cl.type, false))
        {
            int size = cl.exclusion_radius();

            // Steam clouds are less dangerous than the other ones,
            // so don't exclude the neighbour cells.
            if (cl.type == CLOUD_STEAM && size == 1)
                size = 0;

            bool was_exclusion = is_exclude_root(gc);
            set_exclude(gc, size, true, false, true);
            if (!did_exclude && !was_exclusion)
                ret |= update_flag::added_exclude;
        }
    }

    // Print hints mode messages for features in LOS.
    if (crawl_state.game_is_hints())
        hints_observe_cell(gc);

    if (env.map_knowledge(gc).changed() || !env.map_knowledge(gc).seen())
        ret |= update_flag::affect_excludes;

    set_terrain_visible(gc);

    if (!(env.pgrid(gc) & FPROP_SEEN_OR_NOEXP))
    {
        if (!crawl_state.game_is_arena()
            && !(branches[you.where_are_you].branch_flags & brflag::fully_map)
            && you.has_mutation(MUT_EXPLORE_REGEN))
        {
            _do_explore_healing();
        }
        if (!crawl_state.game_is_arena()
            && cell_triggers_conduct(gc)
            && !(branches[you.where_are_you].branch_flags & brflag::fully_map)
            && !(player_in_branch(BRANCH_SLIME) && you_worship(GOD_JIYVA)))
        {
            // Level is not used.
            did_god_conduct(DID_EXPLORATION, 0);
            const int density = env.density ? env.density : 2000;
            you.exploration += div_rand_round(1<<16, density);
            roll_trap_effects();
        }
        env.pgrid(gc) |= FPROP_SEEN_OR_NOEXP;
    }

    return ret;
}

static void player_view_update()
{
    if (crawl_state.game_is_arena())
    {
        for (rectangle_iterator ri(crawl_view.vgrdc, LOS_MAX_RANGE); ri; ++ri)
            player_view_update_at(*ri);
        // no need to do excludes on the arena
        return;
    }

    vector<coord_def> update_excludes;
    bool need_update = false;

    for (vision_iterator ri(you); ri; ++ri)
    {
        update_flags flags = player_view_update_at(*ri);
        if (flags & update_flag::affect_excludes)
            update_excludes.push_back(*ri);
        if (flags & update_flag::added_exclude)
            need_update = true;
    }
    // Update exclusion LOS for possibly affected excludes.
    update_exclusion_los(update_excludes);
    // Catch up on deferred updates for cloud excludes.
    if (need_update)
        deferred_exclude_update();
}

static void _draw_out_of_bounds(screen_cell_t *cell)
{
    cell->glyph  = ' ';
    cell->colour = DARKGREY;
#ifdef USE_TILE
    cell->tile.fg = 0;
    cell->tile.bg = tileidx_out_of_bounds(you.where_are_you);
#endif
}

static void _draw_outside_los(screen_cell_t *cell, const coord_def &gc)
{
    // Outside the env.show area.
    cglyph_t g = get_cell_glyph(gc);
    cell->glyph  = g.ch;
    cell->colour = g.col;

#ifdef USE_TILE
    // this is just for out-of-los rays, but I don't see a more efficient way..
    if (in_bounds(gc))
        cell->tile.bg = tile_env.bk_bg(gc);

    tileidx_out_of_los(&cell->tile.fg, &cell->tile.bg, &cell->tile.cloud, gc);
#endif
}

static void _draw_player(screen_cell_t *cell,
                         const coord_def &gc,
                         bool anim_updates)
{
    // Player overrides everything in cell.
    cell->glyph  = mons_char(you.symbol);
    cell->colour = mons_class_colour(you.symbol);
    if (you.swimming())
    {
        if (env.grid(gc) == DNGN_DEEP_WATER)
            cell->colour = BLUE;
        else
            cell->colour = CYAN;
    }
#ifndef USE_TILE_LOCAL
    if (Options.use_fake_player_cursor)
#endif
        cell->colour |= COLFLAG_REVERSE;

    cell->colour = real_colour(cell->colour, gc);

#ifdef USE_TILE
    cell->tile.fg = tileidx_player();
    cell->tile.bg = tile_env.bk_bg(gc);
    cell->tile.cloud = tile_env.bk_cloud(gc);
    cell->tile.icons = status_icons_for_player();
    if (anim_updates)
        tile_apply_animations(cell->tile.bg, &tile_env.flv(gc));
#else
    UNUSED(anim_updates);
#endif
}

static void _draw_los(screen_cell_t *cell,
                      const coord_def &gc,
                      bool anim_updates)
{
    cglyph_t g = get_cell_glyph(gc);
    cell->glyph  = g.ch;
    cell->colour = g.col;

#ifdef USE_TILE
    cell->tile.fg = tile_env.bk_fg(gc);
    cell->tile.bg = tile_env.bk_bg(gc);
    cell->tile.cloud = tile_env.bk_cloud(gc);
    if (set<tileidx_t>* icons = map_find(tile_env.icons, gc))
        cell->tile.icons = *icons;
    if (anim_updates)
        tile_apply_animations(cell->tile.bg, &tile_env.flv(gc));
#else
    UNUSED(anim_updates);
#endif
}

class shake_viewport_animation: public animation
{
public:
    shake_viewport_animation() { frames = 5; frame_delay = 40; }

    void init_frame(int /*frame*/) override
    {
        offset = coord_def();
        offset.x = random2(3) - 1;
        offset.y = random2(3) - 1;
    }

    coord_def cell_cb(const coord_def &pos, int &/*colour*/) override
    {
        return pos + offset;
    }

private:
    coord_def offset;
};

class banish_animation: public animation
{
public:
    banish_animation(): remaining(false) { }

    void init_frame(int frame) override
    {
        current_frame = frame;

        if (!frame)
        {
            frames = 10;
            hidden.clear();
            remaining = true;
        }

        if (remaining)
            frames = frame + 2;
        else
            frames = frame;

        remaining = false;
    }

    coord_def cell_cb(const coord_def &pos, int &/*colour*/) override
    {
        if (pos == you.pos())
            return pos;

        if (bool *found = map_find(hidden, pos))
        {
            if (*found)
                return coord_def(-1, -1);
        }

        if (!random2(10 - current_frame))
        {
            hidden.insert(make_pair(pos, true));
            return coord_def(-1, -1);
        }

        remaining = true;
        return pos;
    }

    bool remaining;
    map<coord_def, bool> hidden;
    int current_frame;
};

class orb_animation: public animation
{
public:
    void init_frame(int frame) override
    {
        current_frame = frame;
        range = current_frame > 5
            ? (10 - current_frame)
            : current_frame;
        frame_delay = 3 * (6 - range) * (6 - range);
    }

    coord_def cell_cb(const coord_def &pos, int &colour) override
    {
        const coord_def diff = pos - you.pos();
        const int dist = diff.x * diff.x * 4 / 9 + diff.y * diff.y;
        const int min = range * range;
        const int max = (range + 2) * (range  + 2);
        if (dist >= min && dist < max)
            if (is_tiles())
                colour = MAGENTA;
            else
                colour = DARKGREY;
        else
            colour = 0;

        return pos;
    }

    int range;
    int current_frame;
};

static shake_viewport_animation shake_viewport;
static banish_animation banish;
static orb_animation orb;

static animation *animations[NUM_ANIMATIONS] = {
    &shake_viewport,
    &banish,
    &orb
};

void run_animation(animation_type anim, use_animation_type type, bool cleanup)
{
    if (Options.use_animations & type)
    {
        animation *a = animations[anim];

        viewwindow();
        update_screen();

        for (int i = 0; i < a->frames; ++i)
        {
            a->init_frame(i);
            viewwindow(false, false, a);
            update_screen();
            delay(a->frame_delay);
        }

        if (cleanup)
        {
            viewwindow();
            update_screen();
        }
    }
}

static bool _view_is_updating = false;

crawl_view_buffer view_dungeon(animation *a, bool anim_updates, view_renderer *renderer);

static bool _viewwindow_should_render()
{
    if (you.asleep())
        return false;
    if (mouse_control::current_mode() != MOUSE_MODE_NORMAL)
        return true;
    if (you.running && you.running.is_rest())
        return Options.rest_delay != -1;
    const bool run_dont_draw = you.running && Options.travel_delay < 0
                && (!you.running.is_explore() || Options.explore_delay < 0);
    return !run_dont_draw;
}

/**
 * Draws the main window using the character set returned
 * by get_show_glyph().
 *
 * @param show_updates if true, env.show and dependent structures
 *                     are updated. Should be set if anything in
 *                     view has changed.
 * @param tiles_only if true, only the tile view will be updated. This
 *                   is only relevant for Webtiles.
 * @param a[in] the animation to be showing, if any.
 * @param renderer[in] A view renderer used to inject extra visual elements.
 */
void viewwindow(bool show_updates, bool tiles_only, animation *a, view_renderer *renderer)
{
    if (_view_is_updating)
    {
        // recursive calls to this function can lead to memory corruption or
        // crashes, depending on the circumstance, because some functions
        // called from here (e.g. show_init) will wipe out a whole bunch of
        // map data that will still be lurking around higher on the call stack
        // as references. Because the call paths are so complicated, it's hard
        // to find a principled / non-brute-force way of preventing recursion
        // here -- though it's still better to prevent it by other means if
        // possible.
        dprf("Recursive viewwindow call attempted!");
        return;
    }

    {
        unwind_bool updating(_view_is_updating, true);

#ifndef USE_TILE_LOCAL
        save_cursor_pos save;

        if (crawl_state.smallterm)
        {
            smallterm_warning();
            update_screen();
            return;
        }
#endif

        // The player could be at (0,0) if we are called during level-gen; this can
        // happen via mpr -> interrupt_activity -> stop_delay -> runrest::stop
        if (you.duration[DUR_TIME_STEP] || you.pos().origin())
            return;

        // Update the animation of cells only once per turn.
        const bool anim_updates = (you.last_view_update != you.num_turns);

        if (anim_updates)
            you.frame_no++;

#ifdef USE_TILE
        tiles.clear_text_tags(TAG_NAMED_MONSTER);

        if (show_updates)
            mcache.clear_nonref();
#endif

        if (show_updates || _layers != LAYERS_ALL)
        {
            if (!is_map_persistent())
                ash_detect_portals(false);

            if (you.on_current_level)
            {
                // TODO: why on earth is this called from here? It seems like it
                // should be called directly on changing location, or something
                // like that...
                show_init(_layers);

                if (show_updates)
                    maybe_update_stashes();
            }

#ifdef USE_TILE
            tile_draw_floor();
            tile_draw_map_cells();
#endif
            view_clear_overlays();
        }

        if (show_updates)
            player_view_update();

        if (_viewwindow_should_render())
        {
            const auto vbuf = view_dungeon(a, anim_updates, renderer);

            you.last_view_update = you.num_turns;
#ifndef USE_TILE_LOCAL
            if (!tiles_only)
            {
                puttext(crawl_view.viewp.x, crawl_view.viewp.y, vbuf);
                update_monster_pane();
            }
#else
            UNUSED(tiles_only);
#endif
#ifdef USE_TILE
            tiles.set_need_redraw();
            tiles.load_dungeon(vbuf, crawl_view.vgrdc);
            tiles.update_tabs();
#endif

            // Leaving it this way because short flashes can occur in long ones,
            // and this simply works without requiring a stack.
            you.flash_colour = BLACK;
            you.flash_where = 0;
        }

        // Reset env.show if we munged it.
        if (_layers != LAYERS_ALL)
            show_init();
    }
}

#ifdef USE_TILE
struct tile_overlay
{
    coord_def gc;
    tileidx_t tile;
};
static vector<tile_overlay> tile_overlays;
static unsigned int tile_overlay_i;

void view_add_tile_overlay(const coord_def &gc, tileidx_t tile)
{
    tile_overlays.push_back({gc, tile});
}
#endif

struct glyph_overlay
{
    coord_def gc;
    cglyph_t glyph;
};
static vector<glyph_overlay> glyph_overlays;
static unsigned int glyph_overlay_i;

void view_add_glyph_overlay(const coord_def &gc, cglyph_t glyph)
{
    glyph_overlays.push_back({gc, glyph});
}

// Simple helper function to reduce duplication with repeatedly used animation code
void flash_tile(coord_def p, colour_t colour, int delay, tileidx_t tile)
{
    if (!(Options.use_animations & UA_BEAM))
        return;

    if (!in_los_bounds_v(grid2view(p)))
        return;

#ifdef USE_TILE
        // Use a bolt tile if one is specified. Otherwise, just use the default
        // for the colour provied.
        if (tile > 0)
            view_add_tile_overlay(p, vary_bolt_tile(tile));
        else
            view_add_tile_overlay(p, tileidx_zap(colour, p));
#else
        UNUSED(tile);
#endif
        view_add_glyph_overlay(p, {dchar_glyph(DCHAR_FIRED_ZAP),
                                   static_cast<unsigned short>(colour)});

    if (delay > 0)
        animation_delay(delay, true);
}

void view_clear_overlays()
{
#ifdef USE_TILE
    tile_overlays.clear();
#endif
    glyph_overlays.clear();
}

void draw_ring_animation(const coord_def& center, int radius, colour_t colour,
                         colour_t colour_alt, bool outward, int delay, tileidx_t tile)
{
    const int start = outward ? 0 : radius;
    const int end = outward ? radius + 1 : 0;
    const int step = outward ? 1 : -1;

    for (int i = start; i != end; i += step)
    {
        for (distance_iterator di(center, false, false, i); di; ++di)
        {
            if (grid_distance(center, *di) == i && !feat_is_solid(env.grid(*di))
                && you.see_cell_no_trans(*di))
            {
                colour_t draw_colour = colour_alt != BLACK ? coinflip() ? colour
                                                                        : colour_alt
                                                           : colour;
                flash_tile(*di, draw_colour, 0, tile);
            }
        }

        animation_delay(delay, true);
        view_clear_overlays();
    }
}

/**
 * Comparison function for coord_defs that orders coords based on the ordering
 * used by rectangle_iterator.
 */
static bool _coord_def_cmp(const coord_def& l, const coord_def& r)
{
    return l.y < r.y || (l.y == r.y && l.x < r.x);
}

static void _sort_overlays()
{
    /* Stable sort is needed so that we don't swap draw order within cells. */
#ifdef USE_TILE
    stable_sort(begin(tile_overlays), end(tile_overlays),
                [](const tile_overlay &left, const tile_overlay &right) {
                    return _coord_def_cmp(left.gc, right.gc);
                });
    tile_overlay_i = 0;
#endif
    stable_sort(begin(glyph_overlays), end(glyph_overlays),
                [](const glyph_overlay &left, const glyph_overlay &right) {
                    return _coord_def_cmp(left.gc, right.gc);
                });
    glyph_overlay_i = 0;
}

static void add_overlays(const coord_def& gc, screen_cell_t* cell)
{
#ifdef USE_TILE
    while (tile_overlay_i < tile_overlays.size()
           && _coord_def_cmp(tile_overlays[tile_overlay_i].gc, gc))
    {
        tile_overlay_i++;
    }
    while (tile_overlay_i < tile_overlays.size()
           && tile_overlays[tile_overlay_i].gc == gc)
    {
        const auto &overlay = tile_overlays[tile_overlay_i];
        cell->tile.add_overlay(overlay.tile);
        tile_overlay_i++;
    }
#endif
    while (glyph_overlay_i < glyph_overlays.size()
           && _coord_def_cmp(glyph_overlays[glyph_overlay_i].gc, gc))
    {
        glyph_overlay_i++;
    }
    while (glyph_overlay_i < glyph_overlays.size()
           && glyph_overlays[glyph_overlay_i].gc == gc)
    {
        const auto &overlay = glyph_overlays[glyph_overlay_i];
        cell->glyph = overlay.glyph.ch;
        cell->colour = overlay.glyph.col;
        glyph_overlay_i++;
    }
}

/**
 * Constructs the main dungeon view, rendering it into a new crawl_view_buffer.
 *
 * @param a[in] the animation to be showing, if any.
 * @return A new view buffer with the rendered content.
 */
crawl_view_buffer view_dungeon(animation *a, bool anim_updates, view_renderer *renderer)
{
    crawl_view_buffer vbuf(crawl_view.viewsz);

    screen_cell_t *cell(vbuf);

    cursor_control cs(false);

    _sort_overlays();

    int flash_colour = you.flash_colour;
    if (flash_colour == BLACK)
        flash_colour = viewmap_flash_colour();

    const coord_def tl = coord_def(1, 1);
    const coord_def br = vbuf.size();
    for (rectangle_iterator ri(tl, br); ri; ++ri)
    {
        // in grid coords
        const coord_def gc = a
            ? a->cell_cb(view2grid(*ri), flash_colour)
            : view2grid(*ri);

        if (you.flash_where && you.flash_where->is_affected(gc) <= 0)
            draw_cell(cell, gc, anim_updates, 0);
        else
            draw_cell(cell, gc, anim_updates, flash_colour);

        cell++;
    }

    if (renderer)
        renderer->render(vbuf);

    return vbuf;
}

void draw_cell(screen_cell_t *cell, const coord_def &gc,
               bool anim_updates, int flash_colour)
{
#ifdef USE_TILE
    cell->tile.clear();
#endif

    if (!map_bounds(gc))
        _draw_out_of_bounds(cell);
    else if (!crawl_view.in_los_bounds_g(gc))
        _draw_outside_los(cell, gc);
    else if (gc == you.pos() && you.on_current_level
             && _layers & Layer::PLAYER
             && !crawl_state.game_is_arena()
             && !crawl_state.arena_suspended)
    {
        _draw_player(cell, gc, anim_updates);
    }
    else if (you.see_cell(gc))
        _draw_los(cell, gc, anim_updates);
    else
        _draw_outside_los(cell, gc); // in los bounds but not visible

#ifdef USE_TILE
    cell->tile.map_knowledge = map_bounds(gc) ? env.map_knowledge(gc) : map_cell();
    cell->flash_colour = BLACK;
    cell->flash_alpha = 0;
#endif

    // Don't hide important information by recolouring monsters.
    bool allow_mon_recolour = query_map_knowledge(true, gc, [](const map_cell& m) {
        return m.monster() == MONS_NO_MONSTER || mons_class_is_firewood(m.monster());
    });

    // Is this cell excluded from movement by mesmerise-related statuses?
    // MAP_WITHHELD is set in `show.cc:_update_feat_at`.
    bool mesmerise_excluded = (gc != you.pos() // for fungus form
                               && map_bounds(gc)
                               && you.on_current_level
                               && (env.map_knowledge(gc).flags & MAP_WITHHELD)
                               && !feat_is_solid(env.grid(gc)));

#ifdef USE_TILE
    if (you.duration[DUR_BLIND] && you.see_cell(gc))
    {
        cell->flash_colour = real_colour(you.props[BLIND_COLOUR_KEY].get_int(), gc);
        // Using a square curve for the alpha is nicer on the eyes than a straight multiple.
        // The effect of alpha is already disproportionate depending on the flash colour
        // and may need to be revised: for a white flash the effect is already extreme by
        // around alpha 80, but for darker colours it is way dimmer and needs more like 150.
        const int alpha_base = gc.distance_from(you.pos());
        cell->flash_alpha = max(1, alpha_base * alpha_base);
    }
#endif

    // Alter colour if flashing the characters vision.
    if (flash_colour)
    {
        if (!you.see_cell(gc))
            cell->colour = DARKGREY;
        else if (gc != you.pos() && allow_mon_recolour)
            cell->colour = real_colour(flash_colour, gc);
#ifdef USE_TILE
        if (you.see_cell(gc)) {
            cell->flash_colour = real_colour(flash_colour, gc);
            cell->flash_alpha = 0;
        }
#endif
    }
    else if (crawl_state.darken_range)
    {
        if ((crawl_state.darken_range->obeys_mesmerise && mesmerise_excluded)
            || (!crawl_state.darken_range->valid_aim(gc)))
        {
            if (allow_mon_recolour)
                cell->colour = DARKGREY;
#ifdef USE_TILE
            if (you.see_cell(gc))
                cell->tile.bg |= TILE_FLAG_OOR;
#endif
        }
    }
    else if (crawl_state.flash_monsters)
    {
        bool found = gc == you.pos();

        if (!found)
            for (auto mon : *crawl_state.flash_monsters)
            {
                if (gc == mon->pos())
                {
                    found = true;
                    break;
                }
            }

        if (!found)
            cell->colour = DARKGREY;
    }
    else if (mesmerise_excluded) // but no range limits in place
    {
        if (allow_mon_recolour)
            cell->colour = DARKGREY;

#ifdef USE_TILE
        // Only grey out tiles within LOS; out-of-LOS tiles are already
        // darkened.
        if (you.see_cell(gc))
            cell->tile.bg |= TILE_FLAG_OOR;
#endif
    }

#ifdef USE_TILE
    tile_apply_properties(gc, cell->tile);
#endif

    if ((_layers != LAYERS_ALL || Options.always_show_exclusions)
        && you.on_current_level
        && map_bounds(gc)
        && (_layers == Layer::None
            || gc != you.pos()
               && (env.map_knowledge(gc).monster() == MONS_NO_MONSTER
                   || !you.see_cell(gc)))
        && travel_colour_override(gc))
    {
        if (is_exclude_root(gc))
            cell->colour = Options.tc_excluded;
        else if (is_excluded(gc))
            cell->colour = Options.tc_exclude_circle;
    }

    add_overlays(gc, cell);
}

// Hide view layers. The player can toggle certain layers back on
// and the resulting configuration will be remembered for the
// remainder of the game session.
static void _config_layers_menu()
{
    bool exit = false;

    _layers = _layers_saved;
    crawl_state.viewport_weapons    = !!(_layers & Layer::MONSTER_WEAPONS);
    crawl_state.viewport_monster_hp = !!(_layers & Layer::MONSTER_HEALTH);

    msgwin_set_temporary(true);
    while (!exit)
    {
        viewwindow();
        update_screen();
        mprf(MSGCH_PROMPT, "Select layers to display:\n"
                           "<%s>(m)onsters</%s>|"
                           "<%s>(p)layer</%s>|"
                           "<%s>(i)tems</%s>|"
                           "<%s>(c)louds</%s>"
#ifndef USE_TILE_LOCAL
                           "|"
                           "<%s>monster (w)eapons</%s>|"
                           "<%s>monster (h)ealth</%s>"
#endif
                           ,
           _layers & Layer::MONSTERS        ? "lightgrey" : "darkgrey",
           _layers & Layer::MONSTERS        ? "lightgrey" : "darkgrey",
           _layers & Layer::PLAYER          ? "lightgrey" : "darkgrey",
           _layers & Layer::PLAYER          ? "lightgrey" : "darkgrey",
           _layers & Layer::ITEMS           ? "lightgrey" : "darkgrey",
           _layers & Layer::ITEMS           ? "lightgrey" : "darkgrey",
           _layers & Layer::CLOUDS          ? "lightgrey" : "darkgrey",
           _layers & Layer::CLOUDS          ? "lightgrey" : "darkgrey"
#ifndef USE_TILE_LOCAL
           ,
           _layers & Layer::MONSTER_WEAPONS ? "lightgrey" : "darkgrey",
           _layers & Layer::MONSTER_WEAPONS ? "lightgrey" : "darkgrey",
           _layers & Layer::MONSTER_HEALTH  ? "lightgrey" : "darkgrey",
           _layers & Layer::MONSTER_HEALTH  ? "lightgrey" : "darkgrey"
#endif
        );
        mprf(MSGCH_PROMPT, "Press 'a' to toggle all layers. "
                           "Press any other key to exit.");

        switch (get_ch())
        {
        case 'm': _layers_saved = _layers ^= Layer::MONSTERS;        break;
        case 'p': _layers_saved = _layers ^= Layer::PLAYER;          break;
        case 'i': _layers_saved = _layers ^= Layer::ITEMS;           break;
        case 'c': _layers_saved = _layers ^= Layer::CLOUDS;          break;
#ifndef USE_TILE_LOCAL
        case 'w': _layers_saved = _layers ^= Layer::MONSTER_WEAPONS;
                  if (_layers & Layer::MONSTER_WEAPONS)
                      _layers_saved = _layers |= Layer::MONSTERS;
                  break;
        case 'h': _layers_saved = _layers ^= Layer::MONSTER_HEALTH;
                  if (_layers & Layer::MONSTER_HEALTH)
                      _layers_saved = _layers |= Layer::MONSTERS;
                  break;
#endif
        case 'a': if (_layers)
                      _layers_saved = _layers = Layer::None;
                  else
                  {
#ifndef USE_TILE_LOCAL
                      _layers_saved = _layers = LAYERS_ALL
                                      | Layer::MONSTER_WEAPONS
                                      | Layer::MONSTER_HEALTH;
#else
                      _layers_saved = _layers = LAYERS_ALL;
#endif
                  }
                  break;
        default:
            _layers = LAYERS_ALL;
            crawl_state.viewport_weapons    = !!(_layers & Layer::MONSTER_WEAPONS);
            crawl_state.viewport_monster_hp = !!(_layers & Layer::MONSTER_HEALTH);
            exit = true;
            break;
        }

        crawl_state.viewport_weapons    = !!(_layers & Layer::MONSTER_WEAPONS);
        crawl_state.viewport_monster_hp = !!(_layers & Layer::MONSTER_HEALTH);

        msgwin_clear_temporary();
    }
    msgwin_set_temporary(false);

    canned_msg(MSG_OK);
}

void toggle_show_terrain()
{
    if (_layers == LAYERS_ALL)
        _config_layers_menu();
    else
        reset_show_terrain();
}

void reset_show_terrain()
{
    if (_layers != LAYERS_ALL)
        mprf(MSGCH_PROMPT, "Restoring view layers.");

    _layers = LAYERS_ALL;
    crawl_state.viewport_weapons    = !!(_layers & Layer::MONSTER_WEAPONS);
    crawl_state.viewport_monster_hp = !!(_layers & Layer::MONSTER_HEALTH);
}

////////////////////////////////////////////////////////////////////////////
// Term resize handling (generic).

void handle_terminal_resize()
{
    crawl_state.terminal_resized = false;

    if (crawl_state.terminal_resize_handler)
        (*crawl_state.terminal_resize_handler)();
    else
        crawl_view.init_geometry();

    if (crawl_state.waiting_for_ui)
    {
        ui::resize(crawl_view.termsz.x, crawl_view.termsz.y);
        // We always need a redraw as the console was cleared when resizing
        ui::force_render();
    }
    else
    {
        redraw_screen();
        update_screen();
    }
}
