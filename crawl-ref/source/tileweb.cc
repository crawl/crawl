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
#include "tilemcache.h"
#include "tilepick-p.h"
#include "tilesdl.h"
#include "tileview.h"
#include "travel.h"
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
    fprintf(stderr, "initialize()\n");
    std::string title = CRAWL " " + Version::Long();

    // Do our initialization here.
    m_active_layer = LAYER_CRT;

    cgotoxy(1, 1, GOTO_CRT);

    resize();

    return (true);
}

void _send_doll(int x, int y, const dolls_data &doll)
{
    // OTTODO: This is largely copied from tiledoll.cc; unify.
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

    for (int i = 0; i < TILEP_PART_MAX; i++)
    {
        int p = p_order[i];

        if (!doll.parts[p] || flags[p] == TILEP_FLAG_HIDE)
            continue;

        //        if (p == TILEP_PART_SHADOW && (submerged || ghost))
        //            continue;

        int ymax = TILE_Y;

        if (flags[p] == TILEP_FLAG_CUT_CENTAUR
            || flags[p] == TILEP_FLAG_CUT_NAGA)
        {
            ymax = 18;
        }

        // TODO: Use ymax
        fprintf(stdout, "p(%d,%d,%d);", x, y, doll.parts[p]);
    }

}

void _send_cell(int x, int y, const screen_cell_t *vbuf_cell, const coord_def &gc)
{
    // TODO: Refactor, unify with tiledgnbuf.cc stuff
    packed_cell cell = packed_cell(vbuf_cell->tile);
    if (map_bounds(gc))
    {
        cell.flv = env.tile_flv(gc);
        //pack_cell_overlays(gc, &cell);
    }
    else
    {
        cell.flv.floor   = 0;
        cell.flv.wall    = 0;
        cell.flv.special = 0;
        cell.flv.feat    = 0;
    }

    const tileidx_t bg_idx = cell.bg & TILE_FLAG_MASK;
    const tileidx_t fg_idx = cell.fg & TILE_FLAG_MASK;

    // send bg
    if (bg_idx >= TILE_DNGN_WAX_WALL)
        fprintf(stdout, "bg(%d,%d,%d);", x, y, cell.flv.floor);
    fprintf(stdout, "bg(%d,%d,%d);", x, y, bg_idx);

    if (fg_idx >= TILEP_MCACHE_START)
    {
        mcache_entry *entry = mcache.get(fg_idx);
        if (!entry)
        {
            // TODO
            return;
        }
        // TODO: This is mainly copied from pack_mcache; unify
        const dolls_data *doll = entry->doll();
        if (doll)
            _send_doll(x, y, *doll);

        tile_draw_info dinfo[mcache_entry::MAX_INFO_COUNT];
        int draw_info_count = entry->info(&dinfo[0]);
        for (int i = 0; i < draw_info_count; i++)
        {
            fprintf(stdout, "p(%d,%d,%d,%d,%d);",
                    x, y, dinfo[i].idx, dinfo[i].ofs_x, dinfo[i].ofs_y);
        }
    }
    else if (fg_idx == TILEP_PLAYER)
    {
        dolls_data result = player_doll;
        fill_doll_equipment(result);
        _send_doll(x, y, result);
    }
    else if (fg_idx >= TILE_MAIN_MAX)
    {
        fprintf(stdout, "p(%d,%d,%d);", x, y, fg_idx);
    }
    else if (fg_idx > 0)
    {
        fprintf(stdout, "fg(%d,%d,%d);", x, y, fg_idx);
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
        // OTTODO: This will have to account for scrolling to be really effective
        const screen_cell_t *cell = (const screen_cell_t *) vbuf;
        screen_cell_t *old_cell = (screen_cell_t *) m_current_view;
        for (int y = 0; y < m_current_view.size().y; y++)
            for (int x = 0; x < m_current_view.size().x; x++)
            {
                // TODO: Implement != for screen_cell_t
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

        fprintf(stdout, "\n");
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
    fprintf(stdout, "textAreaSize(stats,%d,%d);\n",
            crawl_view.hudsz.x, crawl_view.hudsz.y);

    fprintf(stdout, "textAreaSize(crt,%d,%d);\n",
            crawl_view.termsz.x, crawl_view.termsz.y);

    fprintf(stdout, "textAreaSize(messages,%d,%d);\n",
            crawl_view.msgsz.x, crawl_view.msgsz.y);
}

int TilesFramework::getch_ck()
{
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
    fprintf(stdout, "clrscr();\n");
}

int TilesFramework::get_number_of_lines()
{
    return 24;
}

int TilesFramework::get_number_of_cols()
{
    return 80;
}

void TilesFramework::cgotoxy(int x, int y, GotoRegion region)
{
    m_cursor_region = region;
    fprintf(stdout, "cgotoxy(%d,%d,%d);\n", x, y, region);
}

GotoRegion TilesFramework::get_cursor_region() const
{
    return m_cursor_region;
}

void TilesFramework::redraw()
{
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
    fprintf(stderr, "get_cursor()\n");
    return coord_def(0,0);
}

void TilesFramework::add_overlay(const coord_def &gc, tileidx_t idx)
{
}

void TilesFramework::clear_overlays()
{
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
#endif
