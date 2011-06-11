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
#include <stdarg.h>



static unsigned int get_milliseconds()
{
    // This is Unix-only, but so is Webtiles at the moment.
    timeval tv;
    gettimeofday(&tv, NULL);

    return ((unsigned int) tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

TilesFramework tiles;

TilesFramework::TilesFramework()
    : m_print_fg(15)
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
}

bool TilesFramework::initialise()
{
    std::string title = CRAWL " " + Version::Long();
    send_message("document.title = \"%s\";", title.c_str());

    // Do our initialization here.
    m_active_layer = LAYER_CRT;
    send_message("set_layer('crt');");

    cgotoxy(1, 1, GOTO_CRT);

    return (true);
}

void TilesFramework::write_message(const char *format, ...)
{
    va_list  argp;
    va_start(argp, format);
    vfprintf(stdout, format, argp);
    va_end(argp);
}

void TilesFramework::finish_message()
{
    fprintf(stdout, "\n");
    fflush(stdout);
}

void TilesFramework::send_message(const char *format, ...)
{
    va_list  argp;
    va_start(argp, format);
    vfprintf(stdout, format, argp);
    va_end(argp);
    finish_message();
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

    tiles.write_message("doll:[");

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

        tiles.write_message("[%d,%d],", doll.parts[p], ymax);
    }

    tiles.write_message("],");
}

static void _send_mcache(mcache_entry *entry, bool submerged)
{
    bool trans = entry->transparent();
    if (trans)
        tiles.write_message("trans:true,");

    const dolls_data *doll = entry->doll();
    if (doll)
        _send_doll(*doll, submerged, trans);
    else
        tiles.write_message("doll:[],");

    tiles.write_message("mcache:[");

    tile_draw_info dinfo[mcache_entry::MAX_INFO_COUNT];
    int draw_info_count = entry->info(&dinfo[0]);
    for (int i = 0; i < draw_info_count; i++)
    {
        tiles.write_message("[%d,%d,%d],", dinfo[i].idx, dinfo[i].ofs_x, dinfo[i].ofs_y);
    }

    tiles.write_message("],");
}

static bool _in_water(const packed_cell &cell)
{
    return ((cell.bg & TILE_FLAG_WATER) && !(cell.fg & TILE_FLAG_FLYING));
}

static bool _needs_flavour(const packed_cell &cell)
{
    tileidx_t bg_idx = cell.bg & TILE_FLAG_MASK;
    if (bg_idx >= TILE_DNGN_WAX_WALL)
        return true; // Needs flv.floor
    if (cell.is_liquefied || cell.is_bloody ||
        cell.is_moldy || cell.glowing_mold)
        return true; // Needs flv.special
    return false;
}

bool TilesFramework::_send_cell(int x, int y,
                                const screen_cell_t *screen_cell, screen_cell_t *old_screen_cell)
{
    const packed_cell &cell = screen_cell->tile;
    packed_cell &old_cell = old_screen_cell->tile;
    coord_def gc(x, y);

    ASSERT(map_bounds(gc));

    tileidx_t fg_idx = cell.fg & TILE_FLAG_MASK;

    bool is_changed_player_doll = false;
    if (fg_idx == TILEP_PLAYER)
    {
        dolls_data result = player_doll;
        fill_doll_equipment(result);
        if (result != last_player_doll)
        {
            is_changed_player_doll = true;
            last_player_doll = result;
        }
    }
    if (old_cell == cell &&
        screen_cell->flash_colour == old_screen_cell->flash_colour &&
        !is_changed_player_doll)
        return false;

    const bool in_water = _in_water(cell);
    bool fg_changed = false;

    if (m_origin.equals(-1, -1))
        m_origin = gc;

    write_message("c(%d,%d,{", x - m_origin.x, y - m_origin.y);

    if (old_cell.bg == TILE_FLAG_UNSEEN)
    {
        write_message("c:1,"); // Clears the cell on the client side
    }

    if (screen_cell->flash_colour != old_screen_cell->flash_colour)
        write_message("fl:%u,", screen_cell->flash_colour);

    if (cell.fg != old_cell.fg)
    {
        fg_changed = true;

        write_message("fg:%u,", cell.fg);
        if (fg_idx && fg_idx <= TILE_MAIN_MAX)
        {
            write_message("base:%d,", tileidx_known_base_item(fg_idx));
        }
    }

    if (cell.bg != old_cell.bg)
        write_message("bg:%u,", cell.bg);

    if (cell.is_bloody != old_cell.is_bloody)
        write_message("bloody:%u,", cell.is_bloody);

    if (cell.is_silenced != old_cell.is_silenced)
        write_message("silenced:%u,", cell.is_silenced);

    if (cell.is_haloed != old_cell.is_haloed)
        write_message("haloed:%u,", cell.is_haloed);

    if (cell.is_moldy != old_cell.is_moldy)
        write_message("moldy:%u,", cell.is_moldy);

    if (cell.glowing_mold != old_cell.glowing_mold)
        write_message("glowing_mold:%u,", cell.glowing_mold);

    if (cell.is_sanctuary != old_cell.is_sanctuary)
        write_message("sanctuary:%u,", cell.is_sanctuary);

    if (cell.is_liquefied != old_cell.is_liquefied)
        write_message("liquefied:%u,", cell.is_liquefied);

    if (cell.swamp_tree_water != old_cell.swamp_tree_water)
        write_message("swtree:%u,", cell.swamp_tree_water);

    if (cell.blood_rotation != old_cell.blood_rotation)
        write_message("bloodrot:%d,", cell.blood_rotation);

    if (_needs_flavour(cell) &&
        ((cell.flv.floor != old_cell.flv.floor)
         || (cell.flv.special != old_cell.flv.special)
         || !_needs_flavour(old_cell)))
    {
        write_message("flv:{f:%d,", cell.flv.floor);
        if (cell.flv.special)
            write_message("s:%d,", cell.flv.special);
        write_message("},");
    }

    if (fg_idx >= TILEP_MCACHE_START)
    {
        if (fg_changed)
        {
            mcache_entry *entry = mcache.get(fg_idx);
            if (entry)
            {
                _send_mcache(entry, in_water);
            }
            else
                write_message("doll:[[%d,%d]],", TILEP_MONS_UNKNOWN, TILE_Y);
        }
    }
    else if (fg_idx == TILEP_PLAYER)
    {
        if (fg_changed || is_changed_player_doll)
        {
            _send_doll(last_player_doll, in_water, false);
        }
    }
    else if (fg_idx >= TILE_MAIN_MAX)
    {
        if (fg_changed)
        {
            write_message("doll:[[%d,%d]],", fg_idx, TILE_Y);
            // TODO: _transform_add_weapon
        }
    }

    bool overlays_changed = false;

    if (cell.num_dngn_overlay != old_cell.num_dngn_overlay)
        overlays_changed = true;
    else
    {
        for (int i = 0; i < cell.num_dngn_overlay; i++)
        {
            if (cell.dngn_overlay[i] != old_cell.dngn_overlay[i])
            {
                overlays_changed = true;
                break;
            }
        }
    }

    if (overlays_changed)
    {
        write_message("ov:[");
        for (int i = 0; i < cell.num_dngn_overlay; ++i)
            write_message("%d,", cell.dngn_overlay[i]);
        write_message("],");
    }

    write_message("});");

    return true;
}

void TilesFramework::load_dungeon(const crawl_view_buffer &vbuf,
                                  const coord_def &gc)
{
    if (vbuf.size().equals(0, 0))
        return;

    if (m_active_layer != LAYER_NORMAL)
    {
        m_active_layer = LAYER_NORMAL;
        write_message("set_layer(\"normal\");\n");
    }

    m_next_view = vbuf;
    m_next_gc = gc;
}

void TilesFramework::load_dungeon(const coord_def &cen)
{
    unwind_var<coord_def> viewp(crawl_view.viewp, cen - crawl_view.viewhalfsz);
    unwind_var<coord_def> vgrdc(crawl_view.vgrdc, cen);
    unwind_var<coord_def> vlos1(crawl_view.vlos1);
    unwind_var<coord_def> vlos2(crawl_view.vlos2);

    crawl_view.calc_vlos();
    viewwindow(false);
    place_cursor(CURSOR_MAP, cen);
}

static const int min_stat_height = 12;
static const int stat_width = 42;

static void _send_layout_data(bool need_response)
{
    // need_response indicates if the client needs to set a layout
    tiles.send_message("layout({view_max_width:%u,view_max_height:%u,\
force_overlay:%u,show_diameter:%u,msg_min_height:%u,stat_width:%u,   \
min_stat_height:%u,gxm:%u,gym:%u},%u);",
                       Options.view_max_width, Options.view_max_height,
                       Options.tile_force_overlay, ENV_SHOW_DIAMETER,
                       Options.msg_min_height, stat_width, min_stat_height,
                       GXM, GYM,
                       need_response);
}

void TilesFramework::resize()
{
    // Width of status area in characters.
    crawl_view.hudsz.x = stat_width;
    crawl_view.msgsz.y = Options.msg_min_height;
    m_text_message.resize(crawl_view.msgsz.x, crawl_view.msgsz.y);

    // We want to always render the whole map.
    crawl_view.viewsz = coord_def(GXM, GYM);
    crawl_view.init_view();

    // Send the client the necessary data to do the layout
    _send_layout_data(true);

    // Now wait for the response
    getch_ck();
}

int TilesFramework::getch_ck()
{
    m_text_crt.send();
    m_text_stat.send();
    m_text_message.send();

    if (need_redraw())
        redraw();

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
        // Control messages
        // TODO: This would be much nicer if we just sent messages in JSON
        int msg = getchar();
        int num = 0;
        if (msg == 'w' || msg == 'h' || msg == 's'
            || msg == 'W' || msg == 'H' || msg == 'm')
        {
            // Read the number
            char data[10];
            fgets(data, 10, stdin);
            num = atoi(data);
        }
        switch (msg)
        {
        case 's': // Set height of the stats area
            if (num <= 0) num = 1;
            if (num > 400) num = 400;
            crawl_view.hudsz.y = num;
            m_text_stat.resize(crawl_view.hudsz.x, crawl_view.hudsz.y);
            break;
        case 'W': // Set width of CRT
            if (num <= 0) num = 1;
            if (num > 400) num = 400;
            crawl_view.termsz.x = num;
            m_text_crt.resize(crawl_view.termsz.x, crawl_view.termsz.y);
            break;
        case 'H': // Set height of CRT
            if (num <= 0) num = 1;
            if (num > 400) num = 400;
            crawl_view.termsz.y = num;
            m_text_crt.resize(crawl_view.termsz.x, crawl_view.termsz.y);
            break;
        case 'm': // Set width of the message view
            if (num <= 0) num = 1;
            if (num > 400) num = 400;
            crawl_view.msgsz.x = num;
            m_text_message.resize(crawl_view.msgsz.x, crawl_view.msgsz.y);
            break;
        case 'r': // A spectator joined, resend the necessary data
            std::string title = CRAWL " " + Version::Long();
            send_message("document.title = \"%s\";", title.c_str());
            m_text_crt.send(true);
            m_text_stat.send(true);
            m_text_message.send(true);
            _send_layout_data(false);
            send_message("vgrdc(%d,%d);", m_current_gc.x, m_current_gc.y);
            _send_current_view();
            switch (m_active_layer)
            {
            case LAYER_CRT:
                send_message("set_layer('crt');");
                break;
            case LAYER_NORMAL:
                send_message("set_layer('normal');");
                break;
            default:
                // Cannot happen
                break;
            }
            break;
        }

        return getch_ck();
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
            send_message("set_layer(\"crt\");");
        m_active_layer = LAYER_CRT;
        m_print_area = &m_text_crt;
        break;
    case GOTO_MSG:
        if (m_active_layer != LAYER_NORMAL)
            send_message("set_layer(\"normal\");");
        m_active_layer = LAYER_NORMAL;
        m_print_area = &m_text_message;
        break;
    case GOTO_STAT:
        if (m_active_layer != LAYER_NORMAL)
            send_message("set_layer(\"normal\");");
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

void TilesFramework::_send_current_view()
{
    screen_cell_t old_cell_dummy; // _send_cell needs a cell to compare to

    screen_cell_t *cell = (screen_cell_t *) m_current_view;
    for (int y = 0; y < m_current_view.size().y; y++)
        for (int x = 0; x < m_current_view.size().x; x++)
        {
            // Don't send data for default black tiles
            old_cell_dummy.tile.bg |= TILE_FLAG_UNSEEN;
            old_cell_dummy.tile.flv = env.tile_flv(coord_def(x, y));
            
            _send_cell(x, y, cell, &old_cell_dummy);

            old_cell_dummy.tile.clear();
            cell++;
        }

    send_message("display();");
}

void TilesFramework::redraw()
{
    m_text_crt.send();
    m_text_stat.send();
    m_text_message.send();

    if (m_next_view.size().equals(0, 0))
        return; // Nothing yet to draw

    if (m_current_gc != m_next_gc)
    {
        if (m_origin.equals(-1, -1))
            m_origin = m_next_gc;
        write_message("vgrdc(%d,%d);",
                m_next_gc.x - m_origin.x,
                m_next_gc.y - m_origin.y);
        m_current_gc = m_next_gc;
    }

    if ((m_next_view.size() != m_current_view.size()))
    {
        // The view buffer size changed, we need to do a full redraw
        m_current_view = m_next_view;

        write_message("clear_map();");
        
        screen_cell_t *cell = (screen_cell_t *) m_current_view;
        for (int y = 0; y < m_current_view.size().y; y++)
            for (int x = 0; x < m_current_view.size().x; x++)
            {
                coord_def gc(x, y);
                cell->tile.flv = env.tile_flv(gc);
                pack_cell_overlays(gc, &(cell->tile));

                cell++;
            }

        _send_current_view();
    }
    else
    {
        // Find differences

        const screen_cell_t *cell = (const screen_cell_t *) m_next_view;
        screen_cell_t *old_cell = (screen_cell_t *) m_current_view;
        for (int y = 0; y < m_current_view.size().y; y++)
            for (int x = 0; x < m_current_view.size().x; x++)
            {
                screen_cell_t new_cell = *cell;
                coord_def gc(x, y);
                new_cell.tile.flv = env.tile_flv(gc);
                pack_cell_overlays(gc, &(new_cell.tile));
                
                _send_cell(x, y, &new_cell, old_cell);

                *old_cell = new_cell;

                cell++;
                old_cell++;
            }

        send_message("display();");
    }
    
    m_need_redraw = false;
    m_last_tick_redraw = get_milliseconds();
}

void TilesFramework::update_minimap(const coord_def& gc)
{
}

void TilesFramework::clear_minimap()
{
    m_current_view = crawl_view_buffer();
    m_origin = coord_def(-1, -1);
}

void TilesFramework::update_minimap_bounds()
{
}

void TilesFramework::update_tabs()
{
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
            send_message("remove_cursor(%d);", type);
            return;
        }
        else
        {
            send_message("place_cursor(%d,%d,%d);", type,
                    result.x - m_origin.x, result.y - m_origin.y);
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
    if (idx >= TILE_MAIN_MAX)
        return;

    m_has_overlays = true;

    send_message("add_overlay(%d,%d,%d);", idx,
            gc.x - m_origin.x, gc.y - m_origin.y);
}

void TilesFramework::clear_overlays()
{
    if (m_has_overlays)
        send_message("clear_overlays();");

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
}

void TilesFramework::textcolor(int col)
{
    m_print_fg = col & 0xF;
    m_print_bg = (col >> 4) & 0xF;
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
