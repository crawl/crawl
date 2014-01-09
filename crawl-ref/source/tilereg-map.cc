#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-map.h"

#include "cio.h"
#include "command.h"
#include "food.h"
#include "libutil.h"
#include "misc.h"
#include "options.h"
#include "tilesdl.h"
#include "travel.h"
#include "viewgeom.h"

const VColour map_colours[MAX_MAP_COL] =
{
    VColour(  0,   0,   0, 255), // BLACK
    VColour(128, 128, 128, 255), // DKGREY
    VColour(160, 160, 160, 255), // MDGREY
    VColour(192, 192, 192, 255), // LTGREY
    VColour(255, 255, 255, 255), // WHITE

    VColour(  0,  64, 255, 255), // BLUE  (actually cyan-blue)
    VColour(128, 128, 255, 255), // LTBLUE
    VColour(  0,  32, 128, 255), // DKBLUE (maybe too dark)

    VColour(  0, 255,   0, 255), // GREEN
    VColour(128, 255, 128, 255), // LTGREEN
    VColour(  0, 128,   0, 255), // DKGREEN

    VColour(  0, 255, 255, 255), // CYAN
    VColour( 64, 255, 255, 255), // LTCYAN (maybe too pale)
    VColour(  0, 128, 128, 255), // DKCYAN

    VColour(255,   0,   0, 255), // RED
    VColour(255, 128, 128, 255), // LTRED (actually pink)
    VColour(128,   0,   0, 255), // DKRED

    VColour(192,   0, 255, 255), // MAGENTA (actually blue-magenta)
    VColour(255, 128, 255, 255), // LTMAGENTA
    VColour( 96,   0, 128, 255), // DKMAGENTA

    VColour(255, 255,   0, 255), // YELLOW
    VColour(255, 255,  64, 255), // LTYELLOW (maybe too pale)
    VColour(128, 128,   0, 255), // DKYELLOW

    VColour(165,  91,   0, 255), // BROWN
};

MapRegion::MapRegion(int pixsz) :
    m_buf(NULL),
    m_dirty(true),
    m_far_view(false)
{
    ASSERT(pixsz > 0);

    dx = pixsz;
    dy = pixsz;
    clear();
    init_colours();
}

void MapRegion::on_resize()
{
    delete[] m_buf;

    int size = mx * my;
    m_buf    = new unsigned char[size];
    memset(m_buf, 0, sizeof(unsigned char) * size);
}

void MapRegion::init_colours()
{
    // TODO enne - the options array for colours should be
    // tied to the map feature enumeration to avoid this function.
    m_colours[MF_UNSEEN]        = (map_colour)Options.tile_unseen_col;
    m_colours[MF_FLOOR]         = (map_colour)Options.tile_floor_col;
    m_colours[MF_WALL]          = (map_colour)Options.tile_wall_col;
    m_colours[MF_MAP_FLOOR]     = (map_colour)Options.tile_floor_col; // TODO enne
    m_colours[MF_MAP_WALL]      = (map_colour)Options.tile_mapped_wall_col;
    m_colours[MF_DOOR]          = (map_colour)Options.tile_door_col;
    m_colours[MF_ITEM]          = (map_colour)Options.tile_item_col;
    m_colours[MF_MONS_FRIENDLY] = (map_colour)Options.tile_friendly_col;
    m_colours[MF_MONS_PEACEFUL] = (map_colour)Options.tile_peaceful_col;
    m_colours[MF_MONS_NEUTRAL]  = (map_colour)Options.tile_neutral_col;
    m_colours[MF_MONS_HOSTILE]  = (map_colour)Options.tile_monster_col;
    m_colours[MF_MONS_NO_EXP]   = (map_colour)Options.tile_plant_col;
    m_colours[MF_STAIR_UP]      = (map_colour)Options.tile_upstairs_col;
    m_colours[MF_STAIR_DOWN]    = (map_colour)Options.tile_downstairs_col;
    m_colours[MF_STAIR_BRANCH]  = (map_colour)Options.tile_feature_col;
    m_colours[MF_FEATURE]       = (map_colour)Options.tile_feature_col;
    m_colours[MF_WATER]         = (map_colour)Options.tile_water_col;
    m_colours[MF_LAVA]          = (map_colour)Options.tile_lava_col;
    m_colours[MF_TRAP]          = (map_colour)Options.tile_trap_col;
    m_colours[MF_EXCL_ROOT]     = (map_colour)Options.tile_excl_centre_col;
    m_colours[MF_EXCL]          = (map_colour)Options.tile_excluded_col;
    m_colours[MF_PLAYER]        = (map_colour)Options.tile_player_col;
}

MapRegion::~MapRegion()
{
    delete[] m_buf;
}

void MapRegion::pack_buffers()
{
    m_buf_map.clear();
    m_buf_lines.clear();

    for (int x = m_min_gx; x <= m_max_gx; x++)
        for (int y = m_min_gy; y <= m_max_gy; y++)
        {
            map_feature f = (map_feature)m_buf[x + y * mx];
            map_colour c  = m_colours[f];

            float pos_x = x - m_min_gx;
            float pos_y = y - m_min_gy;
            m_buf_map.add(pos_x, pos_y, pos_x + 1, pos_y + 1, map_colours[c]);
        }

    // Draw window box.
    if (m_win_start.x == -1 && m_win_end.x == -1)
        return;

    int c = (int)Options.tile_window_col;
    float pos_sx = (m_win_start.x - m_min_gx);
    float pos_sy = (m_win_start.y - m_min_gy);
    float pos_ex = (m_win_end.x - m_min_gx) - 1 / (float)dx;
    float pos_ey = (m_win_end.y - m_min_gy) - 1 / (float)dy;

    m_buf_lines.add_square(pos_sx, pos_sy, pos_ex, pos_ey, map_colours[c]);
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
    m_buf_map.draw();
    m_buf_lines.draw();
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
    m_buf[gc.x + gc.y * mx] = f;

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
            map_feature f = (map_feature)m_buf[x + y * mx];
            if (f == MF_UNSEEN)
                continue;

            m_min_gx = min(m_min_gx, x);
            m_max_gx = max(m_max_gx, x);
            m_min_gy = min(m_min_gy, y);
            m_max_gy = max(m_max_gy, y);
        }

    recenter();
#if 0
    // Not needed? (jpeg)
    m_dirty = true;
#endif
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

    if (m_buf)
        memset(m_buf, 0, sizeof(*m_buf) * mx * my);

    m_buf_map.clear();
    m_buf_lines.clear();
}

int MapRegion::handle_mouse(MouseEvent &event)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND
        && !tiles.get_map_display())
    {
        return 0;
    }

    if (!inside(event.px, event.py))
    {
        if (m_far_view)
        {
            m_far_view = false;
            tiles.load_dungeon(crawl_view.vgrdc);
            return 0;
        }
        return 0;
    }

    int cx, cy;
    mouse_pos(event.px, event.py, cx, cy);
    const coord_def gc(m_min_gx + cx, m_min_gy + cy);

    tiles.place_cursor(CURSOR_MOUSE, gc);

    switch (event.event)
    {
    case MouseEvent::MOVE:
        if (m_far_view)
            tiles.load_dungeon(gc);
        return 0;
    case MouseEvent::PRESS:
#ifdef TOUCH_UI
        // ctrl-rolley-wheel on the minimap (this ensures busting out of minimap when zooming in again on very small layouts)
        if ( (event.mod & MOD_CTRL)
        && (event.button == MouseEvent::SCROLL_UP || event.button == MouseEvent::SCROLL_DOWN))
        {
            tiles.zoom_dungeon(event.button == MouseEvent::SCROLL_UP);
            return CK_NO_KEY; // prevents this being handled by the dungeon underneath too(!)
        }
        if (event.button == MouseEvent::LEFT)
        {
            m_far_view = true;
            tiles.load_dungeon(gc);
            if (!tiles.get_map_display())
            {
                process_command(CMD_DISPLAY_MAP);
                m_far_view = false;
                return CK_MOUSE_CMD;
            }
        }
        return 0;
    case MouseEvent::RELEASE:
        if (event.button == MouseEvent::LEFT && m_far_view)
            m_far_view = false;
        return 0;
#else
        if (event.button == MouseEvent::LEFT)
        {
            if (event.mod & MOD_SHIFT)
            {
                // Start autotravel, or give an appropriate message.
                do_explore_cmd();
                return CK_MOUSE_CMD;
            }
            else
            {
                const int cmd = click_travel(gc, event.mod & MOD_CTRL);
                if (cmd != CK_MOUSE_CMD)
                    process_command((command_type) cmd);

                return CK_MOUSE_CMD;
            }
        }
        else if (event.button == MouseEvent::RIGHT)
        {
            m_far_view = true;
            tiles.load_dungeon(gc);
            if (!tiles.get_map_display())
            {
                process_command(CMD_DISPLAY_MAP);
                m_far_view = false;
                return CK_MOUSE_CMD;
            }
        }
        return 0;
    case MouseEvent::RELEASE:
        if (event.button == MouseEvent::RIGHT && m_far_view)
            m_far_view = false;
        return 0;
#endif
    default:
        return 0;
    }
}

bool MapRegion::update_tip_text(string& tip)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return false;

#ifdef TOUCH_UI
    tip = "[L-Click] Enable map mode";
#else
    tip = "[L-Click] Travel / [R-Click] View";
    if (!player_in_branch(BRANCH_LABYRINTH)
        && (you.hunger_state > HS_STARVING || you_min_hunger())
        && i_feel_safe())
    {
        tip += "\n[Shift + L-Click] Autoexplore";
    }
#endif
    return true;
}

#endif
