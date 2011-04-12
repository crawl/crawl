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

#ifdef TARGET_OS_WINDOWS

#include <windows.h>
// WINAPI defines these, but we are already using them in
// wm_event_type enum,
// so we have to undef these to have working input when using
// Windows and Tiles
#undef WM_KEYDOWN
#undef WM_KEYUP
#undef WM_QUIT
#endif

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

/**
 * Creates a new title region and sets it active
 * Remember to call hide_title() when you're done
 * showing the title.
 */
void TilesFramework::draw_title()
{
    fprintf(stderr, "draw_title()\n");
}

/**
 * Updates the loading message text on the title
 * screen
 * Assumes that we only have one region on the layer
 * If at some point it's possible to have multiple regions
 * open while the title screen shows, the .at(0) will need
 * to be changed and saved on a variable somewhere instead
 */
void TilesFramework::update_title_msg(std::string message)
{
    fprintf(stderr, "update_title_msg(message)\n");
    puts(message.c_str());
    printf("\n");
}

/**
 * Deletes the dynamically reserved Titlescreen memory
 * at end. Runs reg->run to get one key input from the user
 * so that the title screen stays ope until any input is given.
 * Assumes that we only have one region on the layer
 * If at some point it's possible to have multiple regions
 * open while the title screen shows, the .at(0) will need
 * to be changed and saved on a variable somewhere instead
 */
void TilesFramework::hide_title()
{
    fprintf(stderr, "hide_title()\n");
}

void TilesFramework::draw_doll_edit()
{
    fprintf(stderr, "draw_doll_edit()\n");
}

void TilesFramework::calculate_default_options()
{
    fprintf(stderr, "calculate_default_options()\n");
}

bool TilesFramework::initialise()
{
    std::string title = CRAWL " " + Version::Long();
    // TODO Send title

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
    int end_x = shift.x >= 0 ? w - shift.x : -1 - shift.x;
    int end_y = shift.y >= 0 ? h - shift.y : -1 - shift.y;

    for (int y = start_y; y != end_y; shift.y >= 0 ? y++ : y--)
        for (int x = start_x; x != end_x; shift.x >= 0 ? x++ : x--)
        {
            ASSERT((y * w + x) < (w*h));
            ASSERT(((y + shift.y) * w + x + shift.x) < (w*h));

            cells[y * w + x] = cells[(y + shift.y) * w + x + shift.x];
            // A kind of hacky way to ensure the cells in the new areas are always sent
            // (even if they by chance are the same as the old cell)
            cells[(y + shift.y) * w + x + shift.x].tile.bg = 0;
        }
}

void TilesFramework::load_dungeon(const crawl_view_buffer &vbuf,
                                  const coord_def &gc)
{
    if (m_active_layer != LAYER_NORMAL)
    {
        m_active_layer = LAYER_NORMAL;
        fprintf(stdout, "setLayer(\"normal\");\n");
    }

    int cx_to_gx = gc.x - vbuf.size().x / 2;
    int cy_to_gy = gc.y - vbuf.size().y / 2;

    if ((vbuf.size() != m_current_view.size()))
    {
        // The view buffer size changed, we need to do a full redraw
        m_current_view = vbuf;

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
        coord_def shift = gc - m_current_gc;
        m_current_gc = gc;
        fprintf(stderr, "Shift: %d/%d\n", shift.x, shift.y);
        if ((shift.x != 0) || (shift.y != 0))
        {
            _shift_view_buffer(m_current_view, shift);
        }

        const screen_cell_t *cell = (const screen_cell_t *) vbuf;
        screen_cell_t *old_cell = (screen_cell_t *) m_current_view;
        for (int y = 0; y < m_current_view.size().y; y++)
            for (int x = 0; x < m_current_view.size().x; x++)
            {
                if ((cell->tile.fg != old_cell->tile.fg)
                    || (cell->tile.bg != old_cell->tile.bg))
                {
                    coord_def cgc(x + cx_to_gx, y + cy_to_gy);

                    _send_cell(x, y, cell, cgc);
                    *old_cell = *cell;
                    counter++;
                }
                cell++;
                old_cell++;
            }

        fprintf(stderr, "Sent: %d/%d\n", counter, vbuf.size().y * vbuf.size().x);

        fprintf(stdout, "\n"); // This sends the message to the client
        fflush(stdout);
    }
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
    tiles.place_cursor(CURSOR_MAP, cen);
}

void TilesFramework::resize()
{
    m_text_stat.resize(crawl_view.hudsz.x, crawl_view.hudsz.y);
    m_text_message.resize(crawl_view.msgsz.x, crawl_view.msgsz.y);
    m_text_crt.resize(crawl_view.termsz.x, crawl_view.termsz.y);
}

int TilesFramework::getch_ck()
{
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
    m_print_area->clear();
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
    die("get_cursor() not implemented!");
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

    int cx_to_gx = m_current_gc.x - m_current_view.size().x / 2;
    int cy_to_gy = m_current_gc.y - m_current_view.size().y / 2;

    fprintf(stdout, "drawMain(%d,%d,%d);\n",
            idx, gc.x - cx_to_gx, gc.y - cy_to_gy);
}

void TilesFramework::clear_overlays()
{
    fprintf(stderr, "clear_overlays\n");
}

void TilesFramework::set_need_redraw(unsigned int min_tick_delay)
{
}

bool TilesFramework::need_redraw() const
{
    return false;
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
            m_print_area->put_character(*str, m_print_fg, m_print_bg, m_print_x, m_print_y);
            m_print_x++;
            if (m_print_x >= m_print_area->mx)
            {
                m_print_x = 0;
                m_print_y++;
            }
        }

        if (m_print_y >= m_print_area->my)
        {
            // TODO Scroll?
            m_print_y = 0;
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
