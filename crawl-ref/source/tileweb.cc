#include "AppHdr.h"

#ifdef USE_TILE_WEB

#include "artefact.h"
#include "cio.h"
#include "coord.h"
#include "directn.h"
#include "env.h"
#include "files.h"
#include "libutil.h"
#include "macro.h"
#include "map_knowledge.h"
#include "message.h"
#include "mon-util.h"
#include "options.h"
#include "player.h"
#include "state.h"
#include "stuff.h"
#include "tiledef-dngn.h"
#include "tiledef-gui.h"
#include "tiledef-main.h"
#include "tiledef-player.h"
#include "tiledef-icons.h"
#include "tilemcache.h"
#include "tilepick.h"
#include "tilepick-p.h"
#include "tilesdl.h"
#include "tileview.h"
#include "travel.h"
#include "unicode.h"
#include "view.h"
#include "viewgeom.h"

#include <sys/time.h>

static unsigned int get_milliseconds()
{
    // This is Unix-only, but so is Webtiles at the moment.
    timeval tv;
    gettimeofday(&tv, NULL);

    return ((unsigned int) tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

TilesFramework tiles;

TilesFramework::TilesFramework()
{
}

TilesFramework::~TilesFramework()
{
}

void TilesFramework::shutdown()
{
}

void TilesFramework::draw_doll_edit()
{
    fprintf(stderr, "draw_doll_edit()\n");
}

bool TilesFramework::initialise()
{
    std::string title = CRAWL " " + Version::Long();
    fprintf(stdout, "document.title = \"%s\";", title.c_str());

    // Do our initialization here.
    m_active_layer = LAYER_CRT;

    cgotoxy(1, 1, GOTO_CRT);

    resize();

    return (true);
}

static void _send_doll(const dolls_data &doll, bool submerged, bool ghost)
{
    // Ordered from back to front.
    int p_order[TILEP_PART_MAX] =
    {
        // background
        TILEP_PART_SHADOW,
        TILEP_PART_HALO,
        TILEP_PART_ENCH,
        TILEP_PART_DRCWING,
        TILEP_PART_CLOAK,
        // player
        TILEP_PART_BASE,
        TILEP_PART_BOOTS,
        TILEP_PART_LEG,
        TILEP_PART_BODY,
        TILEP_PART_ARM,
        TILEP_PART_HAIR,
        TILEP_PART_BEARD,
        TILEP_PART_HELM,
        TILEP_PART_HAND1,
        TILEP_PART_HAND2,
        TILEP_PART_DRCHEAD
    };

    int flags[TILEP_PART_MAX];
    tilep_calc_flags(doll, flags);

    // For skirts, boots go under the leg armour.  For pants, they go over.
    if (doll.parts[TILEP_PART_LEG] < TILEP_LEG_SKIRT_OFS)
    {
        p_order[7] = TILEP_PART_BOOTS;
        p_order[6] = TILEP_PART_LEG;
    }

    // Special case bardings from being cut off.
    const bool is_naga = is_player_tile(doll.parts[TILEP_PART_BASE],
                                        TILEP_BASE_NAGA);

    if (doll.parts[TILEP_PART_BOOTS] >= TILEP_BOOTS_NAGA_BARDING
        && doll.parts[TILEP_PART_BOOTS] <= TILEP_BOOTS_NAGA_BARDING_RED)
    {
        flags[TILEP_PART_BOOTS] = is_naga ? TILEP_FLAG_NORMAL : TILEP_FLAG_HIDE;
    }

    const bool is_cent = is_player_tile(doll.parts[TILEP_PART_BASE],
                                        TILEP_BASE_CENTAUR);

    if (doll.parts[TILEP_PART_BOOTS] >= TILEP_BOOTS_CENTAUR_BARDING
        && doll.parts[TILEP_PART_BOOTS] <= TILEP_BOOTS_CENTAUR_BARDING_RED)
    {
        flags[TILEP_PART_BOOTS] = is_cent ? TILEP_FLAG_NORMAL : TILEP_FLAG_HIDE;
    }

    fprintf(stdout, "doll:[");

    for (int i = 0; i < TILEP_PART_MAX; ++i)
    {
        int p = p_order[i];

        if (!doll.parts[p] || flags[p] == TILEP_FLAG_HIDE)
            continue;

        if (p == TILEP_PART_SHADOW && (submerged || ghost))
            continue;

        int ymax = TILE_Y;

        if (flags[p] == TILEP_FLAG_CUT_CENTAUR
            || flags[p] == TILEP_FLAG_CUT_NAGA)
        {
            ymax = 18;
        }

        fprintf(stdout, "[%d,%d],", doll.parts[p], ymax);
    }

    fprintf(stdout, "],");
}

static void _send_mcache(mcache_entry *entry, bool submerged)
{
    bool trans = entry->transparent();
    if (trans)
        fprintf(stdout, "trans:true,");

    const dolls_data *doll = entry->doll();
    if (doll)
        _send_doll(*doll, submerged, trans);

    fprintf(stdout, "mcache:[");

    tile_draw_info dinfo[mcache_entry::MAX_INFO_COUNT];
    int draw_info_count = entry->info(&dinfo[0]);
    for (int i = 0; i < draw_info_count; i++)
    {
        fprintf(stdout, "[%d,%d,%d],", dinfo[i].idx, dinfo[i].ofs_x, dinfo[i].ofs_y);
    }

    fprintf(stdout, "],");
}

static bool _in_water(const packed_cell &cell)
{
    return ((cell.bg & TILE_FLAG_WATER) && !(cell.fg & TILE_FLAG_FLYING));
}

void _send_cell(int x, int y, const screen_cell_t *vbuf_cell, const coord_def &gc)
{
    packed_cell cell = packed_cell(vbuf_cell->tile);
    if (map_bounds(gc))
    {
        cell.flv = env.tile_flv(gc);
        pack_cell_overlays(gc, &cell);
    }
    else
    {
        cell.flv.floor   = 0;
        cell.flv.wall    = 0;
        cell.flv.special = 0;
        cell.flv.feat    = 0;
    }

    fprintf(stdout, "c(%d,%d,{fg:%d,bg:%d,%s%s%s%s%s%s%s",
            x, y, cell.fg, cell.bg,
            // These could obviously be shorter, but I don't think they need to be
            cell.is_bloody ? "bloody:true," : "",
            cell.is_silenced ? "silenced:true," : "",
            cell.is_haloed ? "haloed:true," : "",
            cell.is_moldy ? "moldy:true," : "",
            cell.is_sanctuary ? "sanctuary:true," : "",
            cell.is_liquefied ? "liquefied:true," : "",
            cell.swamp_tree_water ? "swtree:true," : "");


    if (cell.is_bloody)
    {
        fprintf(stdout, "bloodrot:%d,", cell.blood_rotation);
    }

    fprintf(stdout, "flv:{f:%d,", cell.flv.floor);
    if (cell.flv.special)
        fprintf(stdout, "s:%d,", cell.flv.special);
    fprintf(stdout, "},");

    tileidx_t fg_idx = cell.fg & TILE_FLAG_MASK;
    const bool in_water = _in_water(cell);

    if (fg_idx >= TILEP_MCACHE_START)
    {
        mcache_entry *entry = mcache.get(fg_idx);
        if (entry)
        {
            _send_mcache(entry, in_water);
        }
        else
            fprintf(stdout, "doll:[[%d,%d]],", TILEP_MONS_UNKNOWN, TILE_Y);
    }
    else if (fg_idx == TILEP_PLAYER)
    {
        dolls_data result = player_doll;
        fill_doll_equipment(result);
        _send_doll(result, in_water, false);
    }
    else if (fg_idx >= TILE_MAIN_MAX)
    {
        fprintf(stdout, "doll:[[%d,%d]],", fg_idx, TILE_Y);
        // TODO: _transform_add_weapon
    }

    if (cell.num_dngn_overlay > 0)
    {
        fprintf(stdout, "ov:[");
        for (int i = 0; i < cell.num_dngn_overlay; ++i)
            fprintf(stdout, "%d,", cell.dngn_overlay[i]);
        fprintf(stdout, "],");
    }

    // TODO: send flavour?

    fprintf(stdout, "});");
}

static void _shift_view_buffer(crawl_view_buffer &vbuf, coord_def &shift)
{
    if ((abs(shift.x) >= vbuf.size().x)
        || (abs(shift.y) >= vbuf.size().y))
        return; // The whole buffer needs to be redrawn anyway

    fprintf(stdout, "shift(%d,%d);", shift.x, shift.y);

    screen_cell_t *cells = (screen_cell_t *) vbuf;

    int w = vbuf.size().x, h = vbuf.size().y;

    // shift specifies the shift in the view location; i.e.
    // if shift.x is > 0, we need to move all cells to the left
    // Also, if shift.x is < 0, we need to iterate through the cells from right to left,
    // because we would overwrite the cells we haven't shifted yet otherwise.
    // All this is of course analogous for y.
    int start_x = shift.x >= 0 ? 0 : w - 1;
    int start_y = shift.y >= 0 ? 0 : h - 1;
    int end_x = shift.x >= 0 ? w : -1;
    int end_y = shift.y >= 0 ? h : -1;

    // We now iterate over the locations _that are shifted to_, copying their data
    // from the locations they come from.
    for (int y = start_y; y != end_y; shift.y >= 0 ? y++ : y--)
        for (int x = start_x; x != end_x; shift.x >= 0 ? x++ : x--)
        {
            ASSERT((0 <= x) && (x < w) && (0 <= y) && (y < h));

            int sx = x + shift.x, sy = y + shift.y;
            if ((0 > sx) || (sx >= w) || (0 > sy) || (sy >= h))
            {
                cells[y * w + x].tile.bg = 0 + TILE_FLAG_UNSEEN;
            }
            else
            {
                cells[y * w + x] = cells[sy * w + sx];
            }
        }
}

void TilesFramework::load_dungeon(const crawl_view_buffer &vbuf,
                                  const coord_def &gc)
{
    if (vbuf.size().equals(0, 0))
        return; // It seems the view buffer changes to size 0 while using stairs

    if (m_active_layer != LAYER_NORMAL)
    {
        m_active_layer = LAYER_NORMAL;
        fprintf(stdout, "setLayer(\"normal\");\n");
    }

    m_next_view = vbuf;
    m_next_gc = gc;
}

void TilesFramework::load_dungeon(const coord_def &cen)
{
    fprintf(stderr, "load_dungeon(cen)\n");
    unwind_var<coord_def> viewp(crawl_view.viewp, cen - crawl_view.viewhalfsz);
    unwind_var<coord_def> vgrdc(crawl_view.vgrdc, cen);
    unwind_var<coord_def> vlos1(crawl_view.vlos1);
    unwind_var<coord_def> vlos2(crawl_view.vlos2);

    crawl_view.calc_vlos();
    viewwindow(false);
    place_cursor(CURSOR_MAP, cen);
}

void TilesFramework::resize()
{
    m_text_stat.resize(crawl_view.hudsz.x, crawl_view.hudsz.y);
    m_text_message.resize(crawl_view.msgsz.x, crawl_view.msgsz.y);
    m_text_crt.resize(crawl_view.termsz.x, crawl_view.termsz.y);
}

int TilesFramework::getch_ck()
{
    m_text_crt.send();
    m_text_stat.send();
    m_text_message.send();

    if (need_redraw())
        redraw();

    fflush(stdout);

    int key = getchar();
    if (key == '\\')
    {
        // Char encoded as a number
        char data[10];
        fgets(data, 10, stdin);
        return atoi(data);
    }
    else if (key == '^')
    {
        // Control messages. Currently nothing.
    }
    return key;
}

void TilesFramework::clrscr()
{
    // TODO: Clear cursor

    m_text_crt.clear();
    m_text_message.clear();
    m_text_stat.clear();

    cgotoxy(1, 1);

    set_need_redraw();
}

int TilesFramework::get_number_of_lines()
{
    return m_text_crt.my;
}

int TilesFramework::get_number_of_cols()
{
    switch (m_active_layer)
    {
    default:
        return 0;
    case LAYER_NORMAL:
        return m_text_message.mx;
    case LAYER_CRT:
        return m_text_crt.mx;
    }
}

void TilesFramework::cgotoxy(int x, int y, GotoRegion region)
{
    m_print_x = x - 1;
    m_print_y = y - 1;
    switch (region)
    {
    case GOTO_CRT:
        if (m_active_layer != LAYER_CRT)
            fprintf(stdout, "setLayer(\"crt\");\n");
        m_active_layer = LAYER_CRT;
        m_print_area = &m_text_crt;
        break;
    case GOTO_MSG:
        if (m_active_layer != LAYER_NORMAL)
            fprintf(stdout, "setLayer(\"normal\");\n");
        m_active_layer = LAYER_NORMAL;
        m_print_area = &m_text_message;
        break;
    case GOTO_STAT:
        if (m_active_layer != LAYER_NORMAL)
            fprintf(stdout, "setLayer(\"normal\");\n");
        m_active_layer = LAYER_NORMAL;
        m_print_area = &m_text_stat;
        break;
    default:
        die("invalid cgotoxy region in webtiles: %d", region);
        break;
    }
    m_cursor_region = region;
}

GotoRegion TilesFramework::get_cursor_region() const
{
    return m_cursor_region;
}

void TilesFramework::redraw()
{
    fprintf(stderr, "redraw()\n");

    m_text_crt.send();
    m_text_stat.send();
    m_text_message.send();

    if (m_next_view.size().equals(0, 0))
        return; // Nothing yet to draw

    int cx_to_gx = m_next_gc.x - m_next_view.size().x / 2;
    int cy_to_gy = m_next_gc.y - m_next_view.size().y / 2;

    if ((m_next_view.size() != m_current_view.size()))
    {
        // The view buffer size changed, we need to do a full redraw
        m_current_view = m_next_view;

        fprintf(stdout, "viewSize(%d,%d);\n", m_current_view.size().x,
                                              m_current_view.size().y);

        screen_cell_t *cell = (screen_cell_t *) m_current_view;
        for (int y = 0; y < m_current_view.size().y; y++)
            for (int x = 0; x < m_current_view.size().x; x++)
            {
                coord_def cgc(x + cx_to_gx, y + cy_to_gy);

                _send_cell(x, y, cell, cgc);
                cell++;
            }

        fprintf(stdout, "\n");
        fflush(stdout);
    }
    else
    {
        int counter = 0;
        // Find differences

        // Shift the view, so we need to send less cells
        coord_def shift = m_next_gc - m_current_gc;
        m_current_gc = m_next_gc;
        fprintf(stderr, "Shift: %d/%d\n", shift.x, shift.y);
        if ((shift.x != 0) || (shift.y != 0))
        {
            _shift_view_buffer(m_current_view, shift);
        }

        const screen_cell_t *cell = (const screen_cell_t *) m_next_view;
        screen_cell_t *old_cell = (screen_cell_t *) m_current_view;
        for (int y = 0; y < m_current_view.size().y; y++)
            for (int x = 0; x < m_current_view.size().x; x++)
            {
                if (cell->tile != old_cell->tile)
                {
                    coord_def cgc(x + cx_to_gx, y + cy_to_gy);

                    _send_cell(x, y, cell, cgc);
                    *old_cell = *cell;
                    counter++;
                }
                cell++;
                old_cell++;
            }

        fprintf(stderr, "Sent: %d/%d\n", counter,
                m_next_view.size().y * m_next_view.size().x);

        fprintf(stdout, "\n"); // This sends the message to the client
        fflush(stdout);
    }

    m_need_redraw = false;
    m_last_tick_redraw = get_milliseconds();
}

void TilesFramework::update_minimap(const coord_def& gc)
{
}

void TilesFramework::clear_minimap()
{
    fprintf(stderr, "clear_minimap()\n");
}

void TilesFramework::update_minimap_bounds()
{
}

void TilesFramework::update_tabs()
{
    fprintf(stderr, "update_tabs()\n");
}

void TilesFramework::toggle_inventory_display()
{
    fprintf(stderr, "toggle_inventory_display()\n");
}

void TilesFramework::place_cursor(cursor_type type, const coord_def &gc)
{
    // This is mainly copied from DungeonRegion::place_cursor.
    coord_def result = gc;

    // If we're only looking for a direction, put the mouse
    // cursor next to the player to let them know that their
    // spell/wand will only go one square.
    if (mouse_control::current_mode() == MOUSE_MODE_TARGET_DIR
        && type == CURSOR_MOUSE && gc != INVALID_COORD)
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
        m_cursor[type] = result;
        if (type == CURSOR_MOUSE)
            m_last_clicked_grid = coord_def();

        if (result == NO_CURSOR)
        {
            fprintf(stdout, "remove_cursor(%d);\n", type);
            return;
        }
        else
        {
            int cx_to_gx = m_current_gc.x - m_current_view.size().x / 2;
            int cy_to_gy = m_current_gc.y - m_current_view.size().y / 2;

            fprintf(stdout, "place_cursor(%d,%d,%d);\n",
                    type, result.x - cx_to_gx, result.y - cy_to_gy);
        }
    }
}

void TilesFramework::clear_text_tags(text_tag_type type)
{
}

void TilesFramework::add_text_tag(text_tag_type type, const std::string &tag,
                                  const coord_def &gc)
{
}

void TilesFramework::add_text_tag(text_tag_type type, const monster* mon)
{
}

const coord_def &TilesFramework::get_cursor() const
{
    return m_cursor[CURSOR_MOUSE];
}

void TilesFramework::add_overlay(const coord_def &gc, tileidx_t idx)
{
    // overlays must be from the main image and must be in LOS.
    if (!crawl_view.in_los_bounds_g(gc))
    {
        fprintf(stderr, "out of los\n");
        return;
    }

    if (idx >= TILE_MAIN_MAX)
        return;

    m_has_overlays = true;

    int cx_to_gx = m_current_gc.x - m_current_view.size().x / 2;
    int cy_to_gy = m_current_gc.y - m_current_view.size().y / 2;

    fprintf(stdout, "addOverlay(%d,%d,%d);\n",
            idx, gc.x - cx_to_gx, gc.y - cy_to_gy);
}

void TilesFramework::clear_overlays()
{
    if (m_has_overlays)
        fprintf(stdout, "clearOverlays();\n");

    m_has_overlays = false;
}

void TilesFramework::set_need_redraw(unsigned int min_tick_delay)
{
    unsigned int ticks = (get_milliseconds() - m_last_tick_redraw);
    if (min_tick_delay && ticks <= min_tick_delay)
        return;

    m_need_redraw = true;
}

bool TilesFramework::need_redraw() const
{
    return m_need_redraw;
}

int TilesFramework::to_lines(int num_tiles)
{
    return num_tiles;
}

void TilesFramework::layout_statcol()
{
    fprintf(stderr, "layout_statcol()\n");
}

void TilesFramework::textcolor(int col)
{
    m_print_fg = col;
}

void TilesFramework::textbackground(int col)
{
    m_print_bg = col;
}

void TilesFramework::put_string(char *buffer)
{
    // This basically just converts buffer to ucs_t and then uses put_ucs_string
    ucs_t buf2[1024], c;

    int j = 0;

    int clen;
    do
    {
        buffer += clen = utf8towc(&c, buffer);

        // TODO: use wcwidth() to handle widths!=1:
        // *  2 for CJK chars -- add a zero-width blank?
        // *  0 for combining characters -- would need extra support
        // * -1 for non-printable stuff -- assert or ignore
        buf2[j] = c;
        j++;

        if (c == 0 || j == (ARRAYSZ(buf2) - 1))
        {
            if (c != 0)
                buf2[j + 1] = 0;

            if (j - 1 != 0)
            {
                put_ucs_string(buf2);
            }
        }
    } while (clen);
}

void TilesFramework::put_ucs_string(ucs_t *str)
{
    while (*str)
    {
        if (*str == '\r')
            continue;

        if (*str == '\n')
        {
            m_print_x = 0;
            m_print_y++;
            // TODO: Clear end of line?
        }
        else
        {
            if (m_print_x >= m_print_area->mx)
            {
                m_print_x = 0;
                m_print_y++;
            }

            if (m_print_y >= m_print_area->my)
            {
                // TODO Scroll?
                m_print_y = 0;
            }

            m_print_area->put_character(*str, m_print_fg, m_print_bg, m_print_x, m_print_y);
            m_print_x++;
        }

        str++;
    }
}

void TilesFramework::clear_to_end_of_line()
{
    for (int x = m_print_x; x < m_print_area->mx; ++x)
        m_print_area->put_character(' ', m_print_fg, m_print_bg, x, m_print_y);
}
#endif
