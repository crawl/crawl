#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-map.h"

#include "cio.h"
#include "command.h"
#include "libutil.h"
#include "nearby-danger.h"
#include "player.h"
#include "options.h"
#include "tiles-build-specific.h"
#include "travel.h"
#include "viewgeom.h"
#include "viewmap.h"

MapRegion::MapRegion(int pixsz) :
    m_dirty(true)
{
    ASSERT(pixsz > 0);

    dx = pixsz;
    dy = pixsz;
    clear();
    init_colours();
}

void MapRegion::on_resize()
{
    m_buf.fill(0);
}

void MapRegion::init_colours()
{
    // TODO enne - the options array for colours should be
    // tied to the map feature enumeration to avoid this function.
    m_colours[MF_UNSEEN]        = Options.tile_unseen_col;
    m_colours[MF_FLOOR]         = Options.tile_floor_col;
    m_colours[MF_WALL]          = Options.tile_wall_col;
    m_colours[MF_MAP_FLOOR]     = Options.tile_mapped_floor_col;
    m_colours[MF_MAP_WALL]      = Options.tile_mapped_wall_col;
    m_colours[MF_DOOR]          = Options.tile_door_col;
    m_colours[MF_ITEM]          = Options.tile_item_col;
    m_colours[MF_MONS_FRIENDLY] = VColour(238,  92, 238); // buggy
    m_colours[MF_MONS_PEACEFUL] = VColour(238,  92, 238); // buggy
    m_colours[MF_MONS_NEUTRAL]  = VColour(238,  92, 238); // buggy
    m_colours[MF_MONS_HOSTILE]  = Options.tile_monster_col;
    m_colours[MF_MONS_NO_EXP]   = Options.tile_plant_col;
    m_colours[MF_STAIR_UP]      = Options.tile_upstairs_col;
    m_colours[MF_STAIR_DOWN]    = Options.tile_downstairs_col;
    m_colours[MF_STAIR_BRANCH]  = Options.tile_branchstairs_col;
    m_colours[MF_FEATURE]       = Options.tile_feature_col;
    m_colours[MF_WATER]         = Options.tile_water_col;
    m_colours[MF_LAVA]          = Options.tile_lava_col;
    m_colours[MF_TRAP]          = Options.tile_trap_col;
    m_colours[MF_EXCL_ROOT]     = Options.tile_excl_centre_col;
    m_colours[MF_EXCL]          = Options.tile_excluded_col;
    m_colours[MF_PLAYER]        = Options.tile_player_col;
    m_colours[MF_DEEP_WATER]    = Options.tile_deep_water_col;
    m_colours[MF_PORTAL]        = Options.tile_portal_col;
    m_colours[MF_TRANSPORTER]   = Options.tile_transporter_col;
    m_colours[MF_TRANSPORTER_LANDING] = Options.tile_transporter_landing_col;
    m_colours[MF_EXPLORE_HORIZON] = Options.tile_explore_horizon_col;
}

void MapRegion::pack_buffers()
{
    m_buf_map.clear();
    m_buf_lines.clear();

    for (int x = m_min_gx; x <= m_max_gx; x++)
        for (int y = m_min_gy; y <= m_max_gy; y++)
        {
            map_feature f = (map_feature)m_buf[x + y * GXM];

            float pos_x = x - m_min_gx;
            float pos_y = y - m_min_gy;
            m_buf_map.add(pos_x, pos_y, pos_x + 1, pos_y + 1, m_colours[f]);
        }

    // Draw window box.
    if (m_win_start.x == -1 && m_win_end.x == -1)
        return;

    float pos_sx = (m_win_start.x - m_min_gx);
    float pos_sy = (m_win_start.y - m_min_gy);
    float pos_ex = (m_win_end.x - m_min_gx);
    float pos_ey = (m_win_end.y - m_min_gy);

    set_transform(true);
    m_buf_lines.add_square(pos_sx*dx, pos_sy*dy, pos_ex*dx + 1, pos_ey*dy + 1,
                           Options.tile_window_col);
}

void MapRegion::render()
{
    if (m_min_gx > m_max_gx || m_min_gy > m_max_gy)
        return;

#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering MapRegion\n");
#endif
    if (m_dirty)
    {
        pack_buffers();
        m_dirty = false;
    }

    set_transform();
    glmanager->set_scissor(sx, sy, wx, wy);
    m_buf_map.draw();
    set_transform(true);
    m_buf_lines.draw();
    glmanager->reset_scissor();
}

void MapRegion::recenter()
{
    // adjust offsets to center map.
    ox = (wx - dx * (m_max_gx - m_min_gx)) / 2;
    oy = (wy - dy * (m_max_gy - m_min_gy)) / 2;
}

void MapRegion::set(const coord_def &gc, map_feature f)
{
    ASSERT((unsigned int)f <= (unsigned char)~0);
    m_buf[gc.x + gc.y * GXM] = f;

    if (f == MF_UNSEEN)
        return;

    // Get map extents
    m_min_gx = min(m_min_gx, gc.x);
    m_max_gx = max(m_max_gx, gc.x);
    m_min_gy = min(m_min_gy, gc.y);
    m_max_gy = max(m_max_gy, gc.y);

    recenter();
}

void MapRegion::update_bounds()
{
    int min_gx = m_min_gx;
    int max_gx = m_max_gx;
    int min_gy = m_min_gy;
    int max_gy = m_max_gy;

    m_min_gx = GXM;
    m_max_gx = 0;
    m_min_gy = GYM;
    m_max_gy = 0;

    for (int x = min_gx; x <= max_gx; x++)
        for (int y = min_gy; y <= max_gy; y++)
        {
            map_feature f = (map_feature)m_buf[x + y * GXM];
            if (f == MF_UNSEEN)
                continue;

            m_min_gx = min(m_min_gx, x);
            m_max_gx = max(m_max_gx, x);
            m_min_gy = min(m_min_gy, y);
            m_max_gy = max(m_max_gy, y);
        }

    recenter();
}

void MapRegion::set_window(const coord_def &start, const coord_def &end)
{
    m_win_start = start;
    m_win_end   = end;

    m_dirty = true;
}

void MapRegion::clear()
{
    m_min_gx = GXM;
    m_max_gx = 0;
    m_min_gy = GYM;
    m_max_gy = 0;

    m_win_start.x = -1;
    m_win_start.y = -1;
    m_win_end.x = -1;
    m_win_end.y = -1;

    recenter();

    m_buf.fill(0);
    m_buf_map.clear();
    m_buf_lines.clear();
}

int MapRegion::handle_mouse(wm_mouse_event &event)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND
        && !tiles.get_map_display())
    {
        return 0;
    }

    if (!inside(event.px, event.py))
    {
        if (is_far_viewing())
        {
            update_far_viewing(crawl_view.vgrdc);
            stop_far_viewing();
        }

        return 0;
    }

    int cx, cy;
    mouse_pos(event.px, event.py, cx, cy);
    const coord_def gc(m_min_gx + cx, m_min_gy + cy);

    if (!is_far_viewing())
        tiles.place_cursor(CURSOR_MOUSE, gc);

    switch (event.event)
    {
    case wm_mouse_event::MOVE:
        if (is_far_viewing())
            update_far_viewing(gc);
        return 0;
    case wm_mouse_event::PRESS:
        if (event.button == wm_mouse_event::LEFT)
        {
            if (tiles.get_map_display())
                return 0;

            if (event.mod & TILES_MOD_SHIFT)
            {
                // Start autotravel, or give an appropriate message.
                do_explore_cmd();
                return CK_MOUSE_CMD;
            }

            const int cmd = click_travel(gc, event.mod & TILES_MOD_CTRL);
            if (cmd != CK_MOUSE_CMD)
                process_command((command_type)cmd);

            return CK_MOUSE_CMD;
        }
        else if (event.button == wm_mouse_event::RIGHT)
        {
            if (!tiles.get_map_display())
            {
                process_command(CMD_DISPLAY_MAP);
                return CK_MOUSE_CMD;
            }
            start_far_viewing(gc);
            tiles.place_cursor(CURSOR_MOUSE, NO_CURSOR);
        }
        return 0;
    case wm_mouse_event::RELEASE:
        if (event.button == wm_mouse_event::RIGHT)
        {
            if (is_far_viewing())
                stop_far_viewing();
        }
        return 0;
    default:
        return 0;
    }
}

bool MapRegion::update_tip_text(string& tip)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return false;

    tip = "[L-Click] Travel / [R-Click] View";
    if (i_feel_safe())
        tip += "\n[Shift + L-Click] Autoexplore";
    return true;
}

#endif
