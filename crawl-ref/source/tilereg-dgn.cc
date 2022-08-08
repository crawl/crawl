#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-dgn.h"

#include <algorithm> // any_of

#include "cio.h"
#include "cloud.h"
#include "command.h"
#include "coord.h"
#include "describe.h"
#include "directn.h"
#include "dgn-height.h"
#include "dungeon.h"
#include "env.h"
#include "tile-env.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "jobs.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "mon-util.h"
#include "nearby-danger.h"
#include "options.h"
#include "output.h"
#include "prompt.h"
#include "religion.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "stash.h"
#include "stringutil.h"
#include "terrain.h"
#include "rltiles/tiledef-dngn.h"
#include "rltiles/tiledef-icons.h"
#include "rltiles/tiledef-main.h"
#include "tag-version.h"
#include "tilefont.h"
#include "tilepick.h"
#include "tiles-build-specific.h"
#include "traps.h"
#include "travel.h"
#include "viewgeom.h"
#include "viewchar.h"

static VColour _flash_colours[NUM_TERM_COLOURS] =
{
    VColour(  0,   0,   0,   0), // BLACK (transparent)
    VColour(  0,   0, 128, 100), // BLUE
    VColour(  0, 128,   0, 100), // GREEN
    VColour(  0, 128, 128, 100), // CYAN
    VColour(128,   0,   0, 100), // RED
    VColour(150,   0, 150, 100), // MAGENTA
    VColour(165,  91,   0, 100), // BROWN
    VColour( 50,  50,  50, 150), // LIGHTGRAY
    VColour(  0,   0,   0, 150), // DARKGRAY
    VColour( 64,  64, 255, 100), // LIGHTBLUE
    VColour( 64, 255,  64, 100), // LIGHTGREEN
    VColour(  0, 255, 255, 100), // LIGHTCYAN
    VColour(255,  64,  64, 100), // LIGHTRED
    VColour(255,  64, 255, 100), // LIGHTMAGENTA
    VColour(150, 150,   0, 100), // YELLOW
    VColour(255, 255, 255, 100), // WHITE
};

DungeonRegion::DungeonRegion(const TileRegionInit &init) :
    TileRegion(init),
    m_cx_to_gx(0),
    m_cy_to_gy(0),
    m_last_clicked_grid(coord_def()),
    m_buf_dngn(init.im),
    m_font_dx(0), m_font_dy(0)
{
    for (int i = 0; i < CURSOR_MAX; i++)
        m_cursor[i] = NO_CURSOR;
    config_glyph_font();
}

DungeonRegion::~DungeonRegion()
{
}

void DungeonRegion::load_dungeon(const crawl_view_buffer &vbuf,
                                 const coord_def &gc_at_vbuf_centre)
{
    m_dirty = true;

    m_cx_to_gx = gc_at_vbuf_centre.x - mx / 2;
    m_cy_to_gy = gc_at_vbuf_centre.y - my / 2;

    m_vbuf = vbuf;

    for (int y = 0; y < m_vbuf.size().y; ++y)
        for (int x = 0; x < m_vbuf.size().x; ++x)
        {
            coord_def gc(x + m_cx_to_gx, y + m_cy_to_gy);

            if (map_bounds(gc))
                pack_cell_overlays(coord_def(x, y), m_vbuf);
        }

    place_cursor(CURSOR_TUTORIAL, m_cursor[CURSOR_TUTORIAL]);
}

void DungeonRegion::pack_cursor(cursor_type type, unsigned int tile)
{
    const coord_def &gc = m_cursor[type];
    if (gc == NO_CURSOR || !on_screen(gc))
        return;

    const coord_def ep(gc.x - m_cx_to_gx, gc.y - m_cy_to_gy);
    m_buf_dngn.add_icons_tile(tile, ep.x, ep.y);
}

// XX code duplication
static unsigned _get_highlight(int col)
{
    return (col & COLFLAG_FRIENDLY_MONSTER) ? Options.friend_highlight :
           (col & COLFLAG_NEUTRAL_MONSTER)  ? Options.neutral_highlight :
           (col & COLFLAG_ITEM_HEAP)        ? Options.heap_highlight :
           (col & COLFLAG_WILLSTAB)         ? Options.stab_highlight :
           (col & COLFLAG_MAYSTAB)          ? Options.may_stab_highlight :
           (col & COLFLAG_FEATURE_ITEM)     ? Options.feature_item_highlight :
           (col & COLFLAG_TRAP_ITEM)        ? Options.trap_item_highlight :
           (col & COLFLAG_REVERSE)          ? unsigned{CHATTR_REVERSE}
                                            : unsigned{CHATTR_NORMAL};
}

void DungeonRegion::pack_glyph_at(screen_cell_t *vbuf_cell, int vx, int vy)
{
    // need to do some footwork to decode glyph colours
    const int bg = _get_highlight(vbuf_cell->colour);
    const int fg = vbuf_cell->colour & 0xF;
    const int advance = m_buf_dngn.get_glyph_font()->char_width(true);
    const int real_x = vx * dx + sx + ox + (dx - advance) / 2;
    const int real_y = vy * dy + sy + oy;
    int tiles_bg = CHATTR_NORMAL;
    int tiles_fg = fg;

    switch (bg & CHATTR_ATTRMASK)
    {
        case CHATTR_REVERSE:
            tiles_bg = fg;
            tiles_fg = BLACK; // XX not right in general
            break;
        case CHATTR_STANDOUT:
        case CHATTR_BOLD:
            if (fg < 8)
                tiles_fg = fg + 8;
            break;
        case CHATTR_DIM:
            if (fg >= 8)
                tiles_fg = fg - 8;
            break;
        case CHATTR_HILITE:
            // hilite colour stored in high bits
            tiles_bg = (bg & CHATTR_COLMASK) >> 8;
            break;
        case CHATTR_BLINK: // ignore
        case CHATTR_UNDERLINE: // ignore
        case CHATTR_NORMAL:
        default:
            break;
    }
    tiles_fg = macro_colour(tiles_fg);
    if (tiles_bg != CHATTR_NORMAL)
        tiles_bg = macro_colour(tiles_bg);
    ASSERT(tiles_fg < NUM_TERM_COLOURS);
    if (tiles_bg == CHATTR_NORMAL)
        m_buf_dngn.add_glyph(vbuf_cell->glyph, term_colours[tiles_fg], real_x, real_y);
    else
    {
        ASSERT(tiles_bg < NUM_TERM_COLOURS);
        m_buf_dngn.add_glyph(vbuf_cell->glyph,
            term_colours[tiles_fg], term_colours[tiles_bg], real_x, real_y);
    }
}

void DungeonRegion::pack_buffers()
{
    m_buf_dngn.clear();
    m_buf_flash.clear();
    const bool pack_tiles = Options.tile_display_mode != "glyphs";
    const bool pack_glyphs = Options.tile_display_mode != "tiles";

    if (m_vbuf.empty())
        return;

    coord_def m_vbuf_sz = m_vbuf.size();
    ASSERT(m_vbuf_sz.x == crawl_view.viewsz.x);
    ASSERT(m_vbuf_sz.y == crawl_view.viewsz.y);

    screen_cell_t *vbuf_cell = m_vbuf;
    for (int y = 0; y < crawl_view.viewsz.y; ++y)
        for (int x = 0; x < crawl_view.viewsz.x; ++x)
        {
            if (pack_tiles)
                m_buf_dngn.add(vbuf_cell->tile, x, y);
            if (pack_glyphs)
                pack_glyph_at(vbuf_cell, x, y);

            const int fcol = vbuf_cell->flash_colour;
            if (fcol)
                m_buf_flash.add(x, y, x + 1, y + 1, _flash_colours[fcol]);

            vbuf_cell++;
        }

    // TODO: these cursors are a bit thick for glyphs mode
    pack_cursor(CURSOR_TUTORIAL, TILEI_TUTORIAL_CURSOR);
    const bool mouse_curs_vis = you.see_cell(m_cursor[CURSOR_MOUSE]);
    pack_cursor(CURSOR_MOUSE, mouse_curs_vis ? TILEI_CURSOR : TILEI_CURSOR2);
    pack_cursor(CURSOR_MAP, TILEI_CURSOR);
}

// #define DEBUG_TILES_REDRAW
void DungeonRegion::render()
{
#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering DungeonRegion\n");
#endif
    if (m_dirty)
    {
        pack_buffers();
        m_dirty = false;
    }

    set_transform();
    glmanager->set_scissor(0, 0, tile_iw, tile_ih);
    if (Options.tile_display_mode == "tiles")
        m_buf_dngn.draw_tiles();
    else
    {
        glmanager->reset_transform();
        m_buf_dngn.draw_glyphs();
        set_transform();
    }

    // even glyph mode needs the icon layer: it is used for cursors.
    m_buf_dngn.draw_icons();
    if (Options.tile_display_mode != "glyphs")
        draw_minibars();

    m_buf_flash.draw();

    glmanager->reset_scissor();

    unordered_set<coord_def> tag_coords;

    for (int t = TAG_MAX - 1; t >= 0; t--)
    {
        for (const TextTag &tag : m_tags[t])
        {
            if (!crawl_view.in_los_bounds_g(tag.gc)
                && !tiles.get_map_display())
            {
                continue;
            }
            if (tag_coords.count(tag.gc))
                continue;
            tag_coords.insert(tag.gc);

            coord_def pc;
            to_screen_coords(tag.gc, &pc);
            // center this coord, which is at the top left of gc's cell
            pc.x += dx / 2;

            const auto text = formatted_string(tag.tag.c_str(), WHITE);
            m_tag_font->render_hover_string(pc.x, pc.y, text);
        }

        //XXX: Why hide unique monster tags when showing e'x'amine tags?
        if (tag_coords.size())
            break;
    }
}

/**
 * Draws miniature health and magic points bars on top of the player tile.
 *
 * Drawing of either is governed by options tile_show_minihealthbar and
 * tile_show_minimagicbar. By default, both are on.
 *
 * Intended behaviour is to display both if either is not full. (Unless
 * the bar is toggled off by options.) --Eino & felirx
 */
void DungeonRegion::draw_minibars()
{
    if (Options.tile_show_minihealthbar && you.hp < you.hp_max
        || Options.tile_show_minimagicbar
           && you.magic_points < you.max_magic_points)
    {
        // Tiles are 32x32 pixels; 1/32 = 0.03125.
        // The bars are two pixels high each.
        const float bar_height = 0.0625;
        float healthbar_offset = 0.875;

        ShapeBuffer buff;

        if (!you.on_current_level || !on_screen(you.pos()))
            return;

        // FIXME: to_screen_coords could be made into two versions: one
        // that gives coords by pixel (the current one), one that gives
        // them by grid.
        coord_def player_on_screen;
        to_screen_coords(you.pos(), &player_on_screen);
        player_on_screen.x = (player_on_screen.x-sx)/dx;
        player_on_screen.y = (player_on_screen.y-sy)/dy;

        if (Options.tile_show_minimagicbar && you.max_magic_points > 0)
        {
            static const VColour magic(0, 114, 159, 207);      // lightblue
            static const VColour magic_spent(0, 0, 0, 255);  // black

            const float magic_divider = (float) you.magic_points
                                        / (float) you.max_magic_points;

            buff.add(player_on_screen.x,
                     player_on_screen.y + healthbar_offset + bar_height,
                     player_on_screen.x + magic_divider,
                     player_on_screen.y + 1,
                     magic);
            buff.add(player_on_screen.x + magic_divider,
                     player_on_screen.y + healthbar_offset + bar_height,
                     player_on_screen.x + 1,
                     player_on_screen.y + 1,
                     magic_spent);
        }
        else
            healthbar_offset += bar_height;

        if (Options.tile_show_minihealthbar)
        {
            const float min_hp = max(0, you.hp);
            const float health_divider = min_hp / (float) you.hp_max;

            const int hp_percent = (you.hp * 100) / you.hp_max;

            int hp_colour = GREEN;
            for (const auto &entry : Options.hp_colour)
                if (hp_percent <= entry.first)
                    hp_colour = entry.second;

            static const VColour healthy(   0, 255, 0, 255); // green
            static const VColour damaged( 255, 255, 0, 255); // yellow
            static const VColour wounded( 150,   0, 0, 255); // darkred
            static const VColour hp_spent(255,   0, 0, 255); // red

            buff.add(player_on_screen.x,
                     player_on_screen.y + healthbar_offset,
                     player_on_screen.x + health_divider,
                     player_on_screen.y + healthbar_offset + bar_height,
                     hp_colour == RED    ? wounded :
                     hp_colour == YELLOW ? damaged
                                         : healthy);

            buff.add(player_on_screen.x + health_divider,
                     player_on_screen.y + healthbar_offset,
                     player_on_screen.x + 1,
                     player_on_screen.y + healthbar_offset + bar_height,
                     hp_spent);
        }

        buff.draw();
    }
}

void DungeonRegion::clear()
{
    m_vbuf.clear();
}

void DungeonRegion::config_glyph_font()
{
    if (dx != m_font_dx || dy != m_font_dy)
    {
        if (Options.tile_display_mode == "glyphs")
        {
            // we won't get the exact font size asked for (or at least, I have
            // not figured out how), so just post facto adjust dx and dy.
            // Cache changed dx/dy values so that successive calls don't
            // further change this without an actual real resize.
            m_buf_dngn.get_glyph_font()->resize(dy);
            // grid size uses exact advances
            dx = m_buf_dngn.get_glyph_font()->char_width(true);
            dy = m_buf_dngn.get_glyph_font()->char_height(true);
        }
        else
            m_buf_dngn.get_glyph_font()->resize(dy * 3 /4); // somewhat heuristic
        // XX this will break if mode can be changed in-game, need to reset dx

        m_font_dx = dx;
        m_font_dy = dy;
    }
}

void DungeonRegion::on_resize()
{
    // TODO enne
}

void DungeonRegion::recalculate()
{
    // needs to happen before superclass recalculate, and therefore can't
    // happen in on_resize
    config_glyph_font();

    Region::recalculate();
}

bool DungeonRegion::inside(int x, int y)
{
    return x >= 0 && y >= 0 && x <= tile_iw && y <= tile_ih;
}

static bool _handle_distant_monster(monster* mon, unsigned char mod)
{
    const bool shift = (mod & TILES_MOD_SHIFT);
    const bool ctrl  = (mod & TILES_MOD_CTRL);
    const bool alt   = (shift && ctrl || (mod & TILES_MOD_ALT));

    // TODO: is see_cell_no_trans too strong?
    if (!mon || mon->friendly() || !you.see_cell_no_trans(mon->pos()))
        return false;

    // TODO: unify code with tooltip construction?
    const item_def* weapon = you.weapon();
    const bool primary_ranged = weapon && is_range_weapon(*weapon);
    const int melee_dist = weapon ? weapon_reach(*weapon) : 1;

    if (!ctrl && !shift && !alt
        && (primary_ranged || (mon->pos() - you.pos()).rdist() <= melee_dist))
    {
        dist t;
        t.target = mon->pos();
        quiver::get_primary_action()->trigger(t);
        return true;
    }

    if (!ctrl && shift && quiver::get_secondary_action()->is_valid())
    {
        dist t;
        t.target = mon->pos();
        quiver::get_secondary_action()->trigger(t);
        return true;
    }

    return false;
}

int DungeonRegion::handle_mouse(wm_mouse_event &event)
{
    tiles.clear_text_tags(TAG_CELL_DESC);

    if (!inside(event.px, event.py))
        return 0;

#ifdef TOUCH_UI
    if (event.event == wm_mouse_event::WHEEL && (event.mod & TILES_MOD_CTRL))
        zoom(event.button == wm_mouse_event::SCROLL_UP);
#endif

    if (mouse_control::current_mode() == MOUSE_MODE_NORMAL
        && event.event == wm_mouse_event::PRESS
        && event.button == wm_mouse_event::LEFT)
    {
        m_last_clicked_grid = m_cursor[CURSOR_MOUSE];

        int cx, cy;
        mouse_pos(event.px, event.py, cx, cy);
        const coord_def gc(cx + m_cx_to_gx, cy + m_cy_to_gy);
        tiles.place_cursor(CURSOR_MOUSE, gc);

        return CK_MOUSE_CLICK;
    }

    if (mouse_control::current_mode() == MOUSE_MODE_MACRO
        || mouse_control::current_mode() == MOUSE_MODE_MORE
        || mouse_control::current_mode() == MOUSE_MODE_PROMPT
        || mouse_control::current_mode() == MOUSE_MODE_YESNO)
    {
        return 0;
    }

    int cx;
    int cy;

    bool on_map = mouse_pos(event.px, event.py, cx, cy);

    const coord_def gc(cx + m_cx_to_gx, cy + m_cy_to_gy);
    tiles.place_cursor(CURSOR_MOUSE, gc);

    if (event.event == wm_mouse_event::MOVE)
    {
        string desc = get_terse_square_desc(gc);
        // Suppress floor description
        if (desc == "floor")
            desc = "";

        if (you.see_cell(gc))
        {
            if (cloud_struct* cloud = cloud_at(gc))
            {
                string terrain_desc = desc;
                desc = cloud->cloud_name(true);

                if (!terrain_desc.empty())
                    desc += "\n" + terrain_desc;
            }
        }

        if (!desc.empty())
            tiles.add_text_tag(TAG_CELL_DESC, desc, gc);
    }

    if (mouse_control::current_mode() == MOUSE_MODE_NORMAL)
        return 0;

    if (!on_map)
        return 0;

    if (mouse_control::current_mode() == MOUSE_MODE_TARGET
        || mouse_control::current_mode() == MOUSE_MODE_TARGET_PATH
        || mouse_control::current_mode() == MOUSE_MODE_TARGET_DIR)
    {
        if (event.event == wm_mouse_event::MOVE)
            return CK_MOUSE_MOVE;
        else if (event.event == wm_mouse_event::PRESS
                 && event.button == wm_mouse_event::LEFT && on_screen(gc))
        {
            m_last_clicked_grid = m_cursor[CURSOR_MOUSE];
            return CK_MOUSE_CLICK;
        }

        return 0;
    }

    if (event.event != wm_mouse_event::PRESS)
        return 0;

    m_last_clicked_grid = m_cursor[CURSOR_MOUSE];

    if (you.pos() == gc)
    {
        switch (event.button)
        {
        case wm_mouse_event::LEFT:
        {
            // if there's an item, pick it up, otherwise wait 1 turn
            if (!(event.mod & TILES_MOD_SHIFT))
            {
                const int o = you.visible_igrd(you.pos());
                if (o == NON_ITEM)
                {
                    // if on stairs, travel them
                    const dungeon_feature_type feat = env.grid(gc);
                    switch (feat_stair_direction(feat))
                    {
                    case CMD_GO_DOWNSTAIRS:
                    case CMD_GO_UPSTAIRS:
                        return command_to_key(feat_stair_direction(feat));
                    default:
                        // otherwise wait
                        return command_to_key(CMD_WAIT);
                    }
                }
                else
                {

                    // pick up menu
                    // More than a single item -> open menu right away.
                    if (count_movable_items(o) > 1)
                    {
                        pickup_menu(o);
                        flush_prev_message();
                        redraw_screen();
                        update_screen();
                        return CK_MOUSE_CMD;
                    }
                    return command_to_key(CMD_PICKUP);
                }
            }

            const dungeon_feature_type feat = env.grid(gc);
            switch (feat_stair_direction(feat))
            {
            case CMD_GO_DOWNSTAIRS:
            case CMD_GO_UPSTAIRS:
                return command_to_key(feat_stair_direction(feat));
            default:
                return 0;
            }
        }
        case wm_mouse_event::RIGHT:
            if (!(event.mod & TILES_MOD_SHIFT))
                return command_to_key(CMD_RESISTS_SCREEN); // Character overview.
            if (!you_worship(GOD_NO_GOD))
                return command_to_key(CMD_DISPLAY_RELIGION); // Religion screen.

            // fall through...
        default:
            return 0;
        }

    }
    // else not on player...
    if (event.button == wm_mouse_event::RIGHT)
    {
        if (map_bounds(gc) && env.map_knowledge(gc).known())
        {
            full_describe_square(gc);
            return CK_MOUSE_CMD;
        }
        else
            return 0;
    }

    if (event.button != wm_mouse_event::LEFT)
        return 0;

    return tile_click_cell(gc, event.mod);
}

int tile_click_cell(const coord_def &gc, unsigned char mod)
{
    monster* mon = monster_at(gc);
    if (mon && you.can_see(*mon))
    {
        if (_handle_distant_monster(mon, mod))
            return CK_MOUSE_CMD;
    }

    if ((mod & TILES_MOD_CTRL) && adjacent(you.pos(), gc))
    {
        const int cmd = click_travel(gc, mod & TILES_MOD_CTRL);
        if (cmd != CK_MOUSE_CMD)
            process_command((command_type) cmd);

        return CK_MOUSE_CMD;
    }

    // Don't move if we've tried to fire/cast/evoke when there's nothing
    // available.
    if (mod & (TILES_MOD_SHIFT | TILES_MOD_CTRL | TILES_MOD_ALT))
        return CK_MOUSE_CMD;

    dprf("click_travel");
    const int cmd = click_travel(gc, mod & TILES_MOD_CTRL);
    if (cmd != CK_MOUSE_CMD)
        process_command((command_type) cmd);

    return CK_MOUSE_CMD;
}

void DungeonRegion::to_screen_coords(const coord_def &gc, coord_def *pc) const
{
    int cx = gc.x - m_cx_to_gx;
    int cy = gc.y - m_cy_to_gy;

    pc->x = sx + ox + cx * dx;
    pc->y = sy + oy + cy * dy;
}

bool DungeonRegion::on_screen(const coord_def &gc) const
{
    int x = gc.x - m_cx_to_gx;
    int y = gc.y - m_cy_to_gy;

    return x >= 0 && x < mx && y >= 0 && y < my;
}

void DungeonRegion::place_cursor(cursor_type type, const coord_def &gc)
{
    coord_def result = gc;

    // If we're only looking for a direction, put the mouse
    // cursor next to the player to let them know that their
    // spell/wand will only go one square.
    if (mouse_control::current_mode() == MOUSE_MODE_TARGET_DIR
        && type == CURSOR_MOUSE && gc != NO_CURSOR)
    {
        coord_def delta = gc - you.pos();

        int ax = abs(delta.x);
        int ay = abs(delta.y);

        result = you.pos();
        if (1000 * ay < 414 * ax)
            result += (delta.x > 0) ? coord_def(1, 0) : coord_def(-1, 0);
        else if (1000 * ax < 414 * ay)
            result += (delta.y > 0) ? coord_def(0, 1) : coord_def(0, -1);
        else if (delta.x > 0)
            result += (delta.y > 0) ? coord_def(1, 1) : coord_def(1, -1);
        else if (delta.x < 0)
            result += (delta.y > 0) ? coord_def(-1, 1) : coord_def(-1, -1);
    }

    if (m_cursor[type] != result)
    {
        m_dirty = true;
        m_cursor[type] = result;
        if (type == CURSOR_MOUSE)
            m_last_clicked_grid = coord_def();
    }
}

bool DungeonRegion::update_tip_text(string &tip)
{
    // TODO enne - it would be really nice to use the tutorial
    // descriptions here for features, monsters, etc...
    // Unfortunately, that would require quite a bit of rewriting
    // and some parsing of formatting to get that to work.

    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return false;

    if (m_cursor[CURSOR_MOUSE] == NO_CURSOR)
        return false;
    const coord_def gc = m_cursor[CURSOR_MOUSE];
    if (!map_bounds(gc) || !crawl_view.in_viewport_g(gc))
        return false;

    bool ret = (tile_dungeon_tip(gc, tip));

#ifdef WIZARD
    if (you.wizard)
    {
        if (!tip.empty())
            tip += "\n\n";

        if (you.see_cell(gc))
        {
            const coord_def ep = grid2show(gc);

            tip += make_stringf("GC(%d, %d) EP(%d, %d)\n",
                                gc.x, gc.y, ep.x, ep.y);

            if (env.heightmap)
                tip += make_stringf("HEIGHT(%d)\n", dgn_height_at(gc));

            tip += "\n";
            tip += tile_debug_string(tile_env.fg(ep), tile_env.bg(ep), ' ');
        }
        else
        {
            tip += make_stringf("GC(%d, %d) [out of sight]\n", gc.x, gc.y);
            if (env.heightmap)
                tip += make_stringf("HEIGHT(%d)\n", dgn_height_at(gc));
            tip += "\n";
        }

        const int map_index = env.level_map_ids(gc);
        if (map_index != INVALID_MAP_INDEX)
        {
            const vault_placement &vp(*env.level_vaults[map_index]);
            const coord_def br = vp.pos + vp.size - 1;
            tip += make_stringf("Vault: %s (%d,%d)-(%d,%d) (%dx%d)\n\n",
                                 vp.map_name_at(gc).c_str(),
                                 vp.pos.x, vp.pos.y,
                                 br.x, br.y,
                                 vp.size.x, vp.size.y);
        }

        tip += tile_debug_string(tile_env.bk_fg(gc), tile_env.bk_bg(gc), 'B');

        if (!m_vbuf.empty())
        {
            const screen_cell_t *vbuf = m_vbuf;
            const coord_def vc(gc.x - m_cx_to_gx, gc.y - m_cy_to_gy);
            const screen_cell_t &cell = vbuf[crawl_view.viewsz.x * vc.y + vc.x];
            tip += tile_debug_string(cell.tile.fg, cell.tile.bg, 'V');
        }

        tip += make_stringf("\nFLV: floor: %d (%s) (%d)"
                            "\n     wall:  %d (%s) (%d)"
                            "\n     feat:  %d (%s) (%d)"
                            "\n  special:  %d",
                            tile_env.flv(gc).floor,
                            tile_dngn_name(tile_env.flv(gc).floor),
                            tile_env.flv(gc).floor_idx,
                            tile_env.flv(gc).wall,
                            tile_dngn_name(tile_env.flv(gc).wall),
                            tile_env.flv(gc).wall_idx,
                            tile_env.flv(gc).feat,
                            tile_dngn_name(tile_env.flv(gc).feat),
                            tile_env.flv(gc).feat_idx,
                            tile_env.flv(gc).special);

        ret = true;
    }
#endif

    return ret;
}

static void _add_tip(string &tip, string text)
{
    if (!tip.empty())
        tip += "\n";
    tip += text;
}

bool tile_dungeon_tip(const coord_def &gc, string &tip)
{
    // TODO: these are not formatted very nicely
    const item_def *weapon = you.weapon();
    const bool primary_ranged = weapon && is_range_weapon(*weapon);
    const bool primary_is_secondary = primary_ranged &&
        quiver::get_primary_action() == quiver::get_secondary_action();
    const int melee_dist = you.weapon() ? weapon_reach(*you.weapon()) : 1;

    vector<command_type> cmd;
    tip = "";
    bool has_monster = false;

    // Left-click first.
    if (gc == you.pos())
    {
        tip = you.your_name;
        tip += " (";
        tip += species::get_abbrev(you.species);
        tip += get_job_abbrev(you.char_class);
        tip += ")";
    }
    else // non-player squares
    {
        const monster* mon = monster_at(gc);
        if (mon && you.can_see(*mon))
        {
            has_monster = true;
            // TODO: is see_cell_no_trans too strong?
            if (mon->friendly())
                _add_tip(tip, "[L-Click] Move");
            else if (you.see_cell_no_trans(mon->pos()))
            {
                tip = mon->name(DESC_A);
                if (primary_ranged)
                {
                    if (!primary_is_secondary)
                    {
                        _add_tip(tip, "[L-Click] "
                            + quiver::get_primary_action()->quiver_description().tostring()
                            + " (%)");
                        cmd.push_back(CMD_PRIMARY_ATTACK);
                    }
                    // else case: tip handled below
                }
                else if ((gc - you.pos()).rdist() <= melee_dist)
                    _add_tip(tip, "[L-Click] Attack"); // show weapon?
                else
                    _add_tip(tip, "[L-Click] Move towards");

                if (quiver::get_secondary_action()->is_valid())
                {
                    // this doesn't show the CMD_PRIMARY_ATTACK key
                    const string clickdesc = primary_is_secondary
                        ? "[L-Click / Shift + L-Click] "
                        : "[Shift + L-Click] ";
                    _add_tip(tip, clickdesc
                        + quiver::get_secondary_action()->quiver_description().tostring()
                        + " (%)");
                    cmd.push_back(CMD_FIRE);
                }
            }
        }
        else if (!cell_is_solid(gc)) // no monster or player
        {
            if (adjacent(gc, you.pos()))
                _add_tip(tip, "[L-Click] Move");
            else if (env.map_knowledge(gc).feat() != DNGN_UNSEEN)
            {
                if (click_travel_safe(gc))
                    _add_tip(tip, "[L-Click] Travel");
                else
                    _add_tip(tip, "[L-Click] Move towards");
            }
        }
        else if (feat_is_closed_door(env.grid(gc)))
        {
            if (!adjacent(gc, you.pos()))
            {
                if (click_travel_safe(gc))
                    _add_tip(tip, "[L-Click] Travel");
                else
                    _add_tip(tip, "[L-Click] Move towards");
            }
            else
            {
                _add_tip(tip, "[L-Click] Open door (%)");
                cmd.push_back(CMD_OPEN_DOOR);
            }
        }
        // all other solid features prevent l-click actions
    }

    // These apply both on the same square as the player's and elsewhere.
    if (!has_monster)
    {
        if (you.see_cell(gc) && env.map_knowledge(gc).item())
        {
            const item_def * const item = env.map_knowledge(gc).item();
            if (item && !item_is_stationary(*item))
            {
                _add_tip(tip, "[L-Click] Pick up items (%)");
                cmd.push_back(CMD_PICKUP);
            }
        }

        const dungeon_feature_type feat = env.map_knowledge(gc).feat();
        const command_type dir = feat_stair_direction(feat);
        if (dir != CMD_NO_CMD)
        {
            _add_tip(tip, "[Shift + L-Click] ");
            if (feat == DNGN_ENTER_SHOP)
                tip += "enter shop";
            else if (feat_is_altar(feat)
                     && player_can_join_god(feat_altar_god(feat)))
            {
                tip += "pray at altar";
            }
            else if (feat_is_gate(feat))
                tip += "enter gate";
            else
                tip += "use stairs";

            tip += " (%)";
            cmd.push_back(dir);
        }
    }

    // Right-click.
    if (gc == you.pos())
    {
        const int o = you.visible_igrd(you.pos());
        if (o == NON_ITEM)
        {
            // if on stairs, travel them
            const dungeon_feature_type feat = env.grid(gc);
            if (feat_stair_direction(feat) == CMD_GO_DOWNSTAIRS
                || feat_stair_direction(feat) == CMD_GO_UPSTAIRS)
            {
                // XXX: wrong for golubria, shops?
                _add_tip(tip, "[L-Click] Use stairs (%)");
                cmd.push_back(feat_stair_direction(feat));
            }
            else if (feat_is_altar(feat)
                     && player_can_join_god(feat_altar_god(feat)))
            {
                _add_tip(tip, "[L-Click] Pray at altar (%)");
                cmd.push_back(feat_stair_direction(feat));
            }
            else
            {
                // otherwise wait
                _add_tip(tip, "[L-Click] Wait one turn (%)");
                cmd.push_back(CMD_WAIT);
            }
        }
        else
        {
            // pick up menu
            // this is already added by the code above
        }

        // Character overview.
        _add_tip(tip, "[R-Click] Overview (%)");
        cmd.push_back(CMD_RESISTS_SCREEN);

        // Religion.
        if (!you_worship(GOD_NO_GOD))
        {
            _add_tip(tip, "[Shift + R-Click] Religion (%)");
            cmd.push_back(CMD_DISPLAY_RELIGION);
        }
    }
    else if (you.see_cell(gc)
             && env.map_knowledge(gc).feat() != DNGN_UNSEEN)
    {
        _add_tip(tip, "[R-Click] Describe");
    }

    if (!tip.empty())
        insert_commands(tip, cmd, false);

    return true;
}

bool DungeonRegion::update_alt_text(string &alt)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return false;

    const coord_def &gc = m_cursor[CURSOR_MOUSE];

    if (gc == NO_CURSOR)
        return false;
    if (!map_bounds(gc))
        return false;
    if (!env.map_knowledge(gc).seen())
        return false;
    if (m_last_clicked_grid == gc)
        return false;

    describe_info inf;
    dungeon_feature_type feat = env.map_knowledge(gc).feat();
    if (you.see_cell(gc))
        get_square_desc(gc, inf);
    else if (feat != DNGN_FLOOR && !feat_is_wall(feat) && !feat_is_tree(feat))
        get_feature_desc(gc, inf);
    else
    {
        // For plain floor, output the stash description.
        const string stash = get_stash_desc(gc);
        if (!stash.empty())
            inf.body << "\n" << stash;
    }

    alt = process_description(inf);

    // Suppress floor description
    if (alt == "Floor.")
    {
        alt.clear();
        return false;
    }
    return true;
}

void DungeonRegion::clear_text_tags(text_tag_type type)
{
    m_tags[type].clear();
}

void DungeonRegion::add_text_tag(text_tag_type type, const string &tag,
                                 const coord_def &gc)
{
    TextTag t;
    t.tag = tag;
    t.gc  = gc;

    m_tags[type].push_back(t);
}

#endif
