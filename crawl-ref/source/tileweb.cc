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

void TilesFramework::load_dungeon(const crawl_view_buffer &vbuf,
                                  const coord_def &gc)
{
    fprintf(stderr, "load_dungeon(vbuf, gc)\n");

    if (m_active_layer != LAYER_NORMAL)
    {
        m_active_layer = LAYER_NORMAL;
        fprintf(stdout, "setLayer(\"normal\");\n");
    }

    if ((vbuf.size() != m_current_view.size()) || true)
    {
        // The view buffer size changed, we need to do a full redraw
        m_current_view = vbuf;
        fprintf(stdout, "viewSize(%d,%d);\n", m_current_view.size().x,
                                              m_current_view.size().y);
        screen_cell_t *cell = (screen_cell_t *) m_current_view;
        for (int y = 0; y < m_current_view.size().y; y++)
            for (int x = 0; x < m_current_view.size().x; x++)
            {
                fprintf(stdout, "c(%d,%d,%d,%d);", x, y,
                        cell->tile.fg, cell->tile.bg);
                cell++;
            }
        fflush(stdout);
    }
    else
    {
        // Find differences
        // OTTODO
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
    fprintf(stderr, "resize()\n");
}

int TilesFramework::getch_ck()
{
    fflush(stdout);
    int key = getchar();
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
    fprintf(stdout, "cgotoxy(%d,%d);\n", x, y);
}

GotoRegion TilesFramework::get_cursor_region() const
{
    return GOTO_CRT;
}

void TilesFramework::redraw()
{
    fprintf(stderr, "redraw()\n");
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
    fprintf(stderr, "set_need_redraw(min_tick_delay)\n");
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
