#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilesdl.h"

#include "ability.h"
#include "artefact.h"
#include "cio.h"
#include "command.h"
#include "coord.h"
#include "env.h"
#include "files.h"
#include "glwrapper.h"
#include "libutil.h"
#include "map-knowledge.h"
#include "menu.h"
#include "message.h"
#include "mon-util.h"
#include "options.h"
#include "output.h"
#include "player.h"
#include "state.h"
#include "rltiles/tiledef-dngn.h"
#include "rltiles/tiledef-gui.h"
#include "rltiles/tiledef-main.h"
#include "tilefont.h"
#include "tilereg-abl.h"
#include "tilereg-cmd.h"
#include "tilereg-crt.h"
#include "tilereg-dgn.h"
#include "tilereg-doll.h"
#include "tilereg.h"
#include "tilereg-inv.h"
#include "tilereg-map.h"
#include "tilereg-mem.h"
#include "tilereg-mon.h"
#include "tilereg-msg.h"
#include "tilereg-skl.h"
#include "tilereg-spl.h"
#include "tilereg-stat.h"
#include "tilereg-tab.h"
#include "tilereg-text.h"
#include "tileview.h"
#include "travel.h"
#include "ui.h"
#include "unwind.h"
#include "version.h"
#include "viewgeom.h"
#include "view.h"
#include "windowmanager.h"

#ifdef TARGET_OS_WINDOWS
# include <windows.h>
#endif

// Default Screen Settings
// width, height, map, crt, stat, msg, tip, lbl
#ifndef __ANDROID__
static int _screen_sizes[4][8] =
{
    // Default
    {1024, 700, 3, 15, 16, 14, 15, 14},
    // Eee PC 900+
    {1024, 600, 2, 14, 14, 12, 13, 12},
    // Small screen
    {800, 600, 2, 14, 11, 12, 13, 12},
    // Eee PC
    {800, 480, 2, 13, 12, 10, 13, 11}
};
#else
// Extra values for viewport and map scale
// width, height, map, crt, stat, msg, tip, lbl, view scale, map scale
static int _screen_sizes[5][10] =
{
    {800, 800, 1, 15, 17, 17, 15, 15, 150, 100},
    {720, 720, 1, 13, 15, 15, 13, 13, 130, 80},
    {640, 640, 1, 12, 14, 14, 12, 12, 120, 70},
    {540, 540, 1, 10, 12, 12, 10, 10, 100, 50},
    {480, 480, 1, 9, 11, 11, 9, 9, 90, 40}
};
#endif

HiDPIState display_density(1,1,1);

TilesFramework tiles;


/**
 * Update the DPI, e.g. after a window move.
 *
 * @return whether the ratio changed.
 */
bool HiDPIState::update(int ndevice, int nlogical, int ngame_scale)
{
    HiDPIState old = *this;
    game_scale = ngame_scale;
    nlogical = apply_game_scale(nlogical);
    // sanity check: should be impossible for this to happen without changing
    // code.
    ASSERT(nlogical != 0);
    if (nlogical == ndevice)
        logical = device = 1;
    else
    {
        device = ndevice;
        logical = nlogical;
    }

    // check if the ratios remain the same.
    // yes, this is kind of a dumb way to do it.
    return (old.device * 100 / old.logical) != (device * 100 / logical);
}

TilesFramework::TilesFramework() :
    m_windowsz(1024, 768),
    m_fullscreen(false),
    m_need_redraw(false),
    m_active_layer(LAYER_CRT),
    m_mouse(-1, -1),
    m_last_tick_redraw(0)
{
}

TilesFramework::~TilesFramework()
{
}

bool TilesFramework::fonts_initialized()
{
    // TODO should in principle check the m_fonts vector as well
    return m_crt_font && m_msg_font && m_stat_font && m_tip_font && m_lbl_font
        && m_glyph_font;
}

static void _init_consoles()
{
#ifdef TARGET_OS_WINDOWS
    // Windows has an annoying habit of disconnecting GUI apps from
    // consoles at startup, and insisting that console apps start with
    // consoles.
    //
    // We build tiles as a GUI app, so we work around this by
    // reconnecting ourselves to the parent process's console (if
    // any), on capable systems.

    // The AttachConsole() function is XP/2003 Server and up, so we
    // need to do the GetModuleHandle()/GetProcAddress() dance.
    typedef BOOL (WINAPI *ac_func)(DWORD);
    ac_func attach_console = (ac_func)(void *)GetProcAddress(
        GetModuleHandle(TEXT("kernel32.dll")), "AttachConsole");

    if (attach_console)
    {
        // Direct output to the console if it isn't already directed
        // somewhere.

        // Grab our parent's console (if any), unless we somehow
        // already have a console.
        attach_console((DWORD)-1); // ATTACH_PARENT_PROCESS

        // FIXME: this overrides redirection.
        //
        // We can get the current stdout/stderr handles with
        // GetStdHandle, but we can't check their validity by
        // comparing against either INVALID_HANDLE_VALUE or 0 ...
        freopen("CONOUT$", "wb", stdout);
        freopen("CONOUT$", "wb", stderr);
    }
#endif
}

static void _shutdown_console()
{
#ifdef TARGET_OS_WINDOWS
    typedef BOOL (WINAPI *fc_func)();
    fc_func free_console = (fc_func)(void *)GetProcAddress(
        GetModuleHandle(TEXT("kernel32.dll")), "FreeConsole");
    if (free_console)
        free_console();
#endif
}
void TilesFramework::shutdown()
{
    delete m_region_tile;
    delete m_region_stat;
    delete m_region_msg;
    delete m_region_map;
    delete m_region_tab;
    delete m_region_inv;
    delete m_region_abl;
    delete m_region_spl;
    delete m_region_mem;
    delete m_region_mon;
    delete m_region_cmd;
    delete m_region_cmd_meta;
    delete m_region_cmd_map;
    delete m_region_crt;

    m_region_tile  = nullptr;
    m_region_stat  = nullptr;
    m_region_msg   = nullptr;
    m_region_map   = nullptr;
    m_region_tab   = nullptr;
    m_region_inv   = nullptr;
    m_region_abl   = nullptr;
    m_region_spl   = nullptr;
    m_region_mem   = nullptr;
    m_region_mon   = nullptr;
    m_region_cmd   = nullptr;
    m_region_cmd_meta = nullptr;
    m_region_cmd_map  = nullptr;
    m_region_crt   = nullptr;

    for (tab_iterator it = m_tabs.begin(); it != m_tabs.end(); ++it)
        delete it->second;

    m_tabs.clear();

    for (Layer &layer : m_layers)
        layer.m_regions.clear();

    for (font_info &font : m_fonts)
    {
        delete font.font;
        font.font = nullptr;
    }

    delete m_image;

    GLStateManager::shutdown();
    WindowManager::shutdown();

    _shutdown_console();
}

void TilesFramework::draw_doll_edit()
{
    DollEditRegion reg(m_image, m_msg_font);
    reg.run();
}

void TilesFramework::set_map_display(const bool display)
{
    m_map_mode_enabled = display;
    if (!display && !tiles.is_using_small_layout() && m_region_tab)
        m_region_tab->activate_tab(TAB_ITEM);
    do_layout(); // recalculate the viewport setup for zoom levels
    redraw_screen(false);
    update_screen();
}

bool TilesFramework::get_map_display()
{
    return m_map_mode_enabled;
}

void TilesFramework::do_map_display()
{
    m_map_mode_enabled = true;
    do_layout(); // recalculate the viewport setup for zoom levels
    redraw_screen(false);
    update_screen();
    if (!tiles.is_using_small_layout() && m_region_tab)
        m_region_tab->activate_tab(TAB_NAVIGATION);
}

void TilesFramework::calculate_default_options()
{
    // Find which set of _screen_sizes to use.
    // TODO: this whole thing needs refactoring for code clarity
    int auto_size = 0;
    int num_screen_sizes = ARRAYSZ(_screen_sizes);
    do
    {
#ifndef __ANDROID__
        if (m_windowsz.x >= _screen_sizes[auto_size][0]
            && m_windowsz.y >= _screen_sizes[auto_size][1])
#else
        // In android we define screen sizes from 480 to 800. Bigger screens
        // are adjusted with the game scale. E.g. A 1920x1080 device would use:
        //     * game scale  => 2
        //     * screen size => 540x540 (usually it can be rotated)
        int adjust_scale = 1;
        if (Options.game_scale == min(m_windowsz.x, m_windowsz.y)/960+1)
            adjust_scale = Options.game_scale;
        if (m_windowsz.x >= (_screen_sizes[auto_size][0]*adjust_scale)
            && m_windowsz.y >= (_screen_sizes[auto_size][1])*adjust_scale)
#endif
        {
            break;
        }
    }
    while (++auto_size < num_screen_sizes - 1);

    m_map_pixels = Options.tile_map_pixels;
    // Auto pick map and font sizes if option is zero.
    // XX this macro is silly
#define AUTO(x,y) (x = (x) ? (x) : _screen_sizes[auto_size][(y)])
    AUTO(m_map_pixels, 2);
    AUTO(Options.tile_font_crt_size, 3);
    AUTO(Options.tile_font_stat_size, 4);
    AUTO(Options.tile_font_msg_size, 5);
    AUTO(Options.tile_font_tip_size, 6);
    AUTO(Options.tile_font_lbl_size, 7);
# ifdef __ANDROID__
    if (Options.tile_viewport_scale == 0)
        Options.tile_viewport_scale = fixedp<int,100>::from_scaled(_screen_sizes[auto_size][8]);
    if (Options.tile_map_scale == 0)
        Options.tile_map_scale = fixedp<int,100>::from_scaled(_screen_sizes[auto_size][9]);
# endif
#undef AUTO

    m_tab_margin = Options.tile_font_lbl_size + 4;
}

bool TilesFramework::initialise()
{
    if (crawl_state.tiles_disabled)
        return true;
    _init_consoles();

    const char *icon_name =
#ifdef DATA_DIR_PATH
    DATA_DIR_PATH
#endif
    "dat/tiles/stone_soup_icon-512x512.png";

    string title = string(CRAWL " ") + Version::Long;

    // Do our initialization here.

    // Create an instance of UIWrapper for the library we were compiled for
    WindowManager::create();
    if (!wm)
        return false;

    // Initialize the wrapper
    // hidpi initialization happens here
    if (!wm->init(&m_windowsz))
        return false;

    wm->set_window_title(title.c_str());
    wm->set_window_icon(icon_name);

    // Copy over constants that need to have been set by the wrapper
    m_screen_width = wm->screen_width();
    m_screen_height = wm->screen_height();

    GLStateManager::init();

    // TODO: what is the minimal startup here needed so that an error dialog
    // can be shown on not found files?

    m_image = new ImageManager();

    // If the window size is less than the view height, the textures will
    // have to be shrunk. If this isn't the case, then don't create mipmaps,
    // as this appears to make things blurry on some users machines.
    bool need_mips = (m_windowsz.y < 32 * VIEW_MIN_HEIGHT);
    if (!m_image->load_textures(need_mips))
        return false;

    calculate_default_options();

    m_crt_font    = load_font(Options.tile_font_crt_file.c_str(),
                              Options.tile_font_crt_size, true);
    m_msg_font    = load_font(Options.tile_font_msg_file.c_str(),
                              Options.tile_font_msg_size, true);
    m_stat_font   = load_font(Options.tile_font_stat_file.c_str(),
                              Options.tile_font_stat_size, true);
    m_tip_font    = load_font(Options.tile_font_tip_file.c_str(),
                              Options.tile_font_tip_size, true);
    m_lbl_font    = load_font(Options.tile_font_lbl_file.c_str(),
                              Options.tile_font_lbl_size, true);
    // XX separate option?
    m_glyph_font  = load_font(Options.tile_font_crt_file.c_str(),
                              Options.tile_font_crt_size, true, false);

    if (!fonts_initialized())
        return false;

    const auto font = tiles.is_using_small_layout() ? m_crt_font : m_lbl_font;
    m_init = TileRegionInit(m_image, font, Options.tile_sidebar_pixels,
                            Options.tile_sidebar_pixels);
    m_region_tile = new DungeonRegion(m_init);
    m_region_tab  = new TabbedRegion(m_init);
    m_region_inv  = new InventoryRegion(m_init);
    m_region_spl  = new SpellRegion(m_init);
    m_region_mem  = new MemoriseRegion(m_init);
    m_region_abl  = new AbilityRegion(m_init);
    m_region_mon  = new MonsterRegion(m_init);
    m_region_skl  = new SkillRegion(m_init);
    m_region_cmd  = new CommandRegion(m_init, ct_action_commands,
                                      ARRAYSZ(ct_action_commands));
    m_region_cmd_meta = new CommandRegion(m_init, ct_system_commands,
                                          ARRAYSZ(ct_system_commands),
                                          "System Commands",
                                          "Execute system commands");
    m_region_cmd_map = new CommandRegion(m_init, ct_map_commands,
                                         ARRAYSZ(ct_map_commands),
                                         "Navigation", "Navigate around map");

    TAB_ITEM    = m_region_tab->push_tab_region(m_region_inv, TILEG_TAB_ITEM);
    TAB_SPELL   = m_region_tab->push_tab_region(m_region_spl, TILEG_TAB_SPELL);
    m_region_tab->push_tab_region(m_region_mem, TILEG_TAB_MEMORISE);
    TAB_ABILITY = m_region_tab->push_tab_region(m_region_abl, TILEG_TAB_ABILITY);
    m_region_tab->push_tab_region(m_region_mon, TILEG_TAB_MONSTER);
    m_region_tab->push_tab_region(m_region_skl, TILEG_TAB_SKILL);
    TAB_COMMAND = m_region_tab->push_tab_region(m_region_cmd, TILEG_TAB_COMMAND);
    m_region_tab->push_tab_region(m_region_cmd_meta,
                                  TILEG_TAB_COMMAND2);
    TAB_NAVIGATION = m_region_tab->push_tab_region(m_region_cmd_map,
                                                   TILEG_TAB_NAVIGATION);
    m_region_tab->activate_tab(TAB_ITEM);

    m_region_msg  = new MessageRegion(m_msg_font);
    m_region_stat = new StatRegion(m_stat_font);
    m_region_crt  = new CRTRegion(m_crt_font);

    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_tile);
    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_msg);
    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_stat);
    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_tab);

    m_layers[LAYER_CRT].m_regions.push_back(m_region_crt);

    cgotoxy(1, 1, GOTO_CRT);

    resize();

    return true;
}

void TilesFramework::reconfigure_fonts()
{
    // call this if DPI changes to regenerate the font
    for (auto finfo : m_fonts)
        finfo.font->configure_font();
}

FontWrapper* TilesFramework::load_font(const char *font_file, int font_size,
                              bool default_on_fail, bool use_cached)
{
    if (use_cached)
        for (unsigned int i = 0; i < m_fonts.size(); i++)
        {
            font_info &finfo = m_fonts[i];
            if (finfo.name == font_file && finfo.size == font_size)
                return finfo.font;
        }

    FontWrapper *font = FontWrapper::create();

    if (!font->load_font(font_file, font_size))
    {
        delete font;
        if (default_on_fail)
        {
            // TODO: this never happens because load_font will die on a missing
            // font file. This setup needs to be smoothed out, but also maybe
            // MONOSPACE_FONT needs to be validated first.
            mprf(MSGCH_ERROR, "Couldn't find font '%s', falling back on '%s'",
                                            font_file, MONOSPACED_FONT);
            return load_font(MONOSPACED_FONT, 12, false);
        }
        else
            return nullptr;
    }

    font_info finfo;
    finfo.font = font;
    finfo.name = font_file;
    finfo.size = font_size;
    m_fonts.push_back(finfo);

    return font;
}
void TilesFramework::load_dungeon(const crawl_view_buffer &vbuf,
                                  const coord_def &gc)
{
    m_active_layer = LAYER_NORMAL;

    if (m_region_tile)
        m_region_tile->load_dungeon(vbuf, gc);

    if (m_region_map)
    {
        int ox = m_region_tile->mx/2;
        int oy = m_region_tile->my/2;
        coord_def win_start(gc.x - ox, gc.y - oy);
        coord_def win_end(gc.x + ox + 1, gc.y + oy + 1);
        m_region_map->set_window(win_start, win_end);
    }
}

void TilesFramework::load_dungeon(const coord_def &cen)
{
    unwind_var<coord_def> viewp(crawl_view.viewp, cen - crawl_view.viewhalfsz);
    unwind_var<coord_def> vgrdc(crawl_view.vgrdc, cen);
    unwind_var<coord_def> vlos1(crawl_view.vlos1);
    unwind_var<coord_def> vlos2(crawl_view.vlos2);

    crawl_view.calc_vlos();
    viewwindow(false);
    tiles.place_cursor(CURSOR_MAP, cen);
}

bool TilesFramework::update_dpi()
{
    if (crawl_state.tiles_disabled)
        return false;
    if (wm->init_hidpi())
    {
        reconfigure_fonts();
        return true;
    }
    return false;
}

void TilesFramework::resize()
{
    update_dpi();
    calculate_default_options();
    do_layout();
    ui::resize(m_windowsz.x, m_windowsz.y);
    if (wm)
        wm->resize(m_windowsz);
}

void TilesFramework::resize_event(int w, int h)
{
    m_windowsz.x = display_density.apply_game_scale(w);
    m_windowsz.y = display_density.apply_game_scale(h);
    // TODO: does order of this call matter? This is based on a previous
    // version where it was called from outside this function.
    ui::resize(m_windowsz.x, m_windowsz.y);

    update_dpi();
    calculate_default_options();
    do_layout();
    wm->resize(m_windowsz);
}

int TilesFramework::handle_mouse(wm_mouse_event &event)
{
    // Note: the mouse event goes to all regions in the active layer because
    // we want to be able to start some GUI event (e.g. far viewing) and
    // stop if it moves to another region.
    int return_key = 0;
    for (int i = m_layers[m_active_layer].m_regions.size() - 1; i >= 0 ; --i)
    {
        // TODO enne - what if two regions give a key?
        return_key = m_layers[m_active_layer].m_regions[i]->handle_mouse(event);
        // break after first key handled; to (a) prevent UI cues from
        // interfering with one another and (b) make popups work nicely :D
        if (return_key)
            break;
    }
    // more popup botching - i want the top layer to return a key (to prevent
    // other layers from trying to handle mouse event) and then return zero
    // anyway
    if (return_key == CK_NO_KEY)
        return 0;

    // Let regions take priority in any mouse mode.
    if (return_key)
        return return_key;

    // Handle "more" mode globally here, rather than duplicate across regions.
    if ((mouse_control::current_mode() == MOUSE_MODE_MORE
         || mouse_control::current_mode() == MOUSE_MODE_PROMPT
         || mouse_control::current_mode() == MOUSE_MODE_YESNO)
        && event.event == wm_mouse_event::PRESS)
    {
        if (event.button == wm_mouse_event::LEFT)
            return CK_MOUSE_CLICK;
        else if (event.button == wm_mouse_event::RIGHT)
            return CK_MOUSE_CMD;
    }

    return 0;
}

static unsigned int _timer_callback(unsigned int ticks, void *param)
{
    UNUSED(ticks, param);


    return 0;
}

int TilesFramework::getch_ck()
{
    wm_event event;
    cursor_loc last_redraw_loc = m_cur_loc;

    int key = 0;

    // Don't update tool tips etc. in targeting mode.
    const bool mouse_target_mode
                = (mouse_control::current_mode() == MOUSE_MODE_TARGET_PATH
                   || mouse_control::current_mode() == MOUSE_MODE_TARGET_DIR);

    const unsigned int ticks_per_screen_redraw = Options.tile_update_rate;


    m_tooltip.clear();
    m_region_msg->alt_text().clear();
    string prev_msg_alt_text = "";

    if (need_redraw())
        redraw();

    while (!key)
    {
        if (crawl_state.seen_hups)
            return ESCAPE;

        unsigned int ticks = 0;

        if (wm->wait_event(&event, INT_MAX))
        {
            ticks = wm->get_ticks();
            if (!mouse_target_mode && event.type != WME_CUSTOMEVENT)
            {
                tiles.clear_text_tags(TAG_CELL_DESC);
                m_region_msg->alt_text().clear();
            }

            // These WME_* events are also handled, at different times, by a
            // similar bit of code in ui.cc. Roughly, this handling is used
            // during the main game display, and the ui.cc loop is used in the
            // main menu and when there are ui elements on top.
            // TODO: consolidate as much as possible

            switch (event.type)
            {
            case WME_ACTIVEEVENT:
                // When game gains focus back then set mod state clean
                // to get rid of stupid Windows/SDL bug with Alt-Tab.
                if (event.active.gain != 0)
                {
                    wm->set_mod_state(TILES_MOD_NONE);
                    set_need_redraw();
                }
                break;
            case WME_KEYDOWN:
                key        = event.key.keysym.sym;
                m_region_tile->place_cursor(CURSOR_MOUSE, NO_CURSOR);

                // If you hit a key, disable tooltips until the mouse
                // is moved again.
                m_show_tooltip = false;
                wm->remove_timer(m_tooltip_timer_id);
                break;

            case WME_KEYUP:
                m_show_tooltip = false;
                wm->remove_timer(m_tooltip_timer_id);
                break;

            case WME_MOUSEMOTION:
                {
                    // For consecutive mouse events, ignore all but the last,
                    // since these can come in faster than crawl can redraw.
                    if (wm->next_event_is(WME_MOUSEMOTION))
                        continue;

                    // Record mouse pos for tooltip timer
                    m_mouse.x = event.mouse_event.px;
                    m_mouse.y = event.mouse_event.py;

                    // Find the new mouse location
                    m_cur_loc.reg = nullptr;
                    for (Region *reg : m_layers[m_active_layer].m_regions)
                    {
                        // inside() can return false here for DungeonRegion
                        // order is important: either way, cx and cy need to be set
                        if (reg->mouse_pos(m_mouse.x, m_mouse.y, m_cur_loc.cx, m_cur_loc.cy)
                            && reg->inside(m_mouse.x, m_mouse.y)) {
                            m_cur_loc.reg = reg;
                            m_cur_loc.mode = mouse_control::current_mode();
                            break;
                        }
                    }

                    // If the mouse has left a region, clear any cursors left behind
                    // handle_mouse() is called after this, since it might place cursors
                    if (m_cur_loc.reg != last_redraw_loc.reg)
                    {
                        for (int i = 0; i < CURSOR_MAX; i++)
                            tiles.place_cursor((cursor_type)i, NO_CURSOR);
                    }

                    key = handle_mouse(event.mouse_event);

                    // update_alt_text() handlers may depend on data set in handle_mouse() handler
                    m_region_msg->alt_text().clear();
                    if (m_cur_loc.reg)
                        m_cur_loc.reg->update_alt_text(m_region_msg->alt_text());
                    if (prev_msg_alt_text != m_region_msg->alt_text())
                    {
                        prev_msg_alt_text = m_region_msg->alt_text();
                        set_need_redraw();
                    }

                    wm->remove_timer(m_tooltip_timer_id);
                    m_tooltip_timer_id = wm->set_timer(Options.tile_tooltip_ms, &_timer_callback);
                    m_show_tooltip = false;
                }
               break;

            case WME_MOUSEBUTTONUP:
            case WME_MOUSEBUTTONDOWN:
                key = handle_mouse(event.mouse_event);
                break;

            case WME_QUIT:
                crawl_state.seen_hups++;
                return ESCAPE;

            case WME_RESIZE:
                resize_event(event.resize.w, event.resize.h);
                set_need_redraw();
                return CK_REDRAW;

            case WME_MOVE:
                if (update_dpi())
                {
                    resize();
                    set_need_redraw();
                    return CK_REDRAW;
                }
                // intentional fallthrough
            case WME_EXPOSE:
                set_need_redraw();
                // AFAIK no need to return CK_REDRAW, as resize() hasn't been called
                break;

            case WME_CUSTOMEVENT:
            default:
                // This is only used to refresh the tooltip.
                m_show_tooltip = true;
                break;
            }
        }

        if (!mouse_target_mode)
        {
            if (m_show_tooltip)
            {
                tiles.clear_text_tags(TAG_CELL_DESC);
                if (Options.tile_tooltip_ms > 0 && m_tooltip.empty())
                {
                    for (Region *reg : m_layers[m_active_layer].m_regions)
                    {
                        if (!reg->inside(m_mouse.x, m_mouse.y))
                            continue;
                        if (reg->update_tip_text(m_tooltip))
                        {
                            set_need_redraw();
                            break;
                        }
                    }
                }
            }
            else
            {
                if (last_redraw_loc != m_cur_loc)
                    set_need_redraw();

                m_tooltip.clear();
            }

            if (need_redraw()
                || ticks > m_last_tick_redraw
                   && ticks - m_last_tick_redraw > ticks_per_screen_redraw)
            {
                last_redraw_loc = m_cur_loc;
                redraw();
            }
        }
    }

    // We got some input, so we'll probably have to redraw something.
    set_need_redraw();

    return key;
}

static const int map_margin      = 2;
static const int map_stat_margin = 4;
static const int min_stat_height = 12;
static const int min_inv_height  = 4;
static const int max_inv_height  = 6;
static const int max_mon_height  = 3;

// Width of status area in characters.
static const int stat_width      = 42;

static int round_up_to_multiple(int a, int b)
{
    int m = a % b;
    return m == 0 ? a : a + b-m;
}

/**
 * Calculates and sets the layout of the main game screen, based on the
 * available screen estate (window or screensize) and options.
 */
void TilesFramework::do_layout()
{
    /**
     * XXX: don't layout unless we're in a game / arena
     * this is to prevent layout code from accessing `you` while it's invalid.
     */
    if (in_headless_mode()
        || !species::is_valid(you.species))
    {
        /* HACK: some code called while loading the game calls mprf(), so even
         * if we're not ready to do an actual layout, we should still give the
         * message region a size, to prevent a crash. */
        if (m_region_msg)
        {
            m_region_msg->place(0, 0, 0);
            m_region_msg->resize_to_fit(10000, 10000);
        }
        return;
    }

    // View size in pixels is ((dx, dy) * crawl_view.viewsz)
    const fixedp<> scale = m_map_mode_enabled ? Options.tile_map_scale
                                              : Options.tile_viewport_scale;
    ASSERT(scale > 0.0);
    m_region_tile->dx = static_cast<int>(Options.tile_cell_pixels * scale);
    m_region_tile->dy = static_cast<int>(Options.tile_cell_pixels * scale);

    int message_y_divider = 0;
    int sidebar_pw;

    // if the screen estate is very small, or if the option is set, choose
    // a layout that is optimal for very small screens
    bool use_small_layout = is_using_small_layout();
    bool message_overlay = Options.tile_force_overlay;

    const int min_msg_h =
                m_region_msg->grid_height_to_pixels(Options.msg_min_height);
    const int max_tile_h =
                m_region_tile->grid_height_to_pixels(
                            max(Options.view_max_height, ENV_SHOW_DIAMETER));

    if (use_small_layout)
    {
        //   * dungeon view, on left, is full height of screen and square
        //   * command tabs are scaled to height of screen and put to far right
        //   * command boxes are hidden (appear when tabs pressed, covering screen)
        //   * stats region squeezed between dungeon and command tabs

        // scale dungeon region to fit *height* of window
        int available_height_in_tiles = m_windowsz.y / m_region_tile->dy;
        if (available_height_in_tiles < ENV_SHOW_DIAMETER)
        {
            m_region_tile->dy = m_windowsz.y / ENV_SHOW_DIAMETER;
            m_region_tile->dx = m_region_tile->dy;

            available_height_in_tiles = ENV_SHOW_DIAMETER;
        }

        // let's make stats region exactly 9 chars wide, so non 4:3 ratio screens look ok
        //  * this sets the tab region width
        m_region_tab->set_small_layout(true, m_windowsz);
        m_region_tab->resize_to_fit(m_windowsz.x, m_windowsz.y);
        //  * ox tells us the width of screen obscured by the tabs
        sidebar_pw = m_region_tab->grid_width_to_pixels(m_region_tab->ox) / 32
                        + m_region_stat->font().max_width(9);
        m_stat_x_divider = m_windowsz.x - sidebar_pw;
    }
    else
    {
        // normal layout code
        m_region_tab->set_small_layout(false, m_windowsz);
        m_region_tab->resize_to_fit(m_windowsz.x, m_windowsz.y);

        const int sidebar_min_pw = m_region_stat->grid_width_to_pixels(
                                                                stat_width);
        sidebar_pw = m_region_tab->grid_width_to_pixels(14) - 10;
        if (sidebar_pw > m_windowsz.x / 3)
            sidebar_pw = m_region_tab->grid_width_to_pixels(7) - 10;
        while (sidebar_pw < sidebar_min_pw)
            sidebar_pw += m_region_tab->grid_width_to_pixels(1);

        // Locations in pixels. stat_x_divider is the dividing vertical line
        // between dungeon view on the left and status area on the right.
        // message_y_divider is the horizontal line between dungeon view on
        // the top and message window at the bottom.
        m_stat_x_divider = m_windowsz.x - sidebar_pw - map_stat_margin;
    }

    // Then, the optimal situation without the overlay - we can fit both
    // Options.view_max_height and at least Options.msg_min_height in the space.

    if (max_tile_h + min_msg_h <= m_windowsz.y && !message_overlay)
    {
        message_y_divider = max_tile_h;
        message_y_divider = max(message_y_divider, m_windowsz.y -
            m_region_msg->grid_height_to_pixels(Options.msg_max_height));
    }
    else
    {
        int available_height_in_tiles = 0;
        available_height_in_tiles =
            (m_windowsz.y - (message_overlay ? 0 : min_msg_h))
                    / m_region_tile->dy;

        // If we can't fit the full LOS to the available space, try using the
        // message overlay.
        if (available_height_in_tiles < ENV_SHOW_DIAMETER)
        {
            message_y_divider = m_windowsz.y;
            message_overlay = true;

            // If using message_overlay isn't enough, scale the dungeon region
            // tiles to fit full LOS into the available space.
            if (m_windowsz.y / m_region_tile->dy < ENV_SHOW_DIAMETER)
            {
                m_region_tile->dy = m_windowsz.y / ENV_SHOW_DIAMETER;
                m_region_tile->dx = m_region_tile->dy;
            }
        }
        else
            message_y_divider = m_windowsz.y - min_msg_h;
    }

    // Calculate message_y_divider. First off, if we have already decided to
    // use the overlay, we can place the divider to the bottom of the screen.
    if (message_overlay)
        message_y_divider = m_windowsz.y;

    // stick message display to the bottom of the window
    int msg_height = m_windowsz.y-message_y_divider;
    // this is very slightly off on high-dpi displays, but it should be taken
    // care of by resize_to_fit later
    msg_height = msg_height / m_region_msg->dy * m_region_msg->dy;
    message_y_divider = m_windowsz.y - msg_height;

    // Expand dungeon region to cover partial tiles, then offset to keep player centred
    int tile_iw = m_stat_x_divider;
    int tile_ih = message_y_divider;
    int tile_ow = round_up_to_multiple(tile_iw, m_region_tile->dx*2) + m_region_tile->dx;
    int tile_oh = round_up_to_multiple(tile_ih, m_region_tile->dy*2) + m_region_tile->dx;
    m_region_tile->resize_to_fit(tile_ow, tile_oh);
    m_region_tile->place(-(tile_ow - tile_iw)/2, -(tile_oh - tile_ih)/2, 0);
    m_region_tile->tile_iw = tile_iw;
    m_region_tile->tile_ih = tile_ih;

    // Resize and place the message window.
    VColour overlay_col = Options.tile_overlay_col;
    overlay_col.a = (255 * Options.tile_overlay_alpha_percent)/100;
    m_region_msg->set_overlay(message_overlay, overlay_col);
    if (message_overlay)
    {
        m_region_msg->place(0, 0, 0); // TODO: Maybe add an option to place
                                      // overlay at the bottom.
        m_region_msg->resize_to_fit(m_stat_x_divider, min_msg_h);
    }
    else
    {
        m_region_msg->resize_to_fit(m_stat_x_divider, m_windowsz.y
                                    - message_y_divider);
        m_region_msg->place(0, tile_ih, 0);
    }

    if (use_small_layout)
    {
        m_stat_col = m_stat_x_divider;

        // place tabs (covering the map view)
        m_region_tab->set_small_layout(true, m_windowsz);
        m_region_tab->resize_to_fit(m_stat_x_divider+m_region_tab->ox*2, message_y_divider-m_tab_margin);

        // place tabs waay to the right (all offsets will be negative)
        m_region_tab->place(m_windowsz.x-m_region_tab->ox, 0);
    }
    else
        m_stat_col = m_stat_x_divider + map_stat_margin;
    m_region_stat->resize_to_fit(sidebar_pw, m_windowsz.y);
    m_region_stat->place(m_stat_col, 0, 0);
    m_region_stat->resize(m_region_stat->mx, min_stat_height);

    layout_statcol();

    m_region_crt->place(0, 0, 0);
    m_region_crt->resize_to_fit(m_windowsz.x, m_windowsz.y);

    crawl_view.viewsz.x = m_region_tile->mx;
    crawl_view.viewsz.y = m_region_tile->my;
    crawl_view.msgsz.x = m_region_msg->mx;
    crawl_view.msgsz.y = m_region_msg->my;
    if (crawl_view.viewsz.x == 0 || crawl_view.viewsz.y == 0
        || crawl_view.msgsz.y == 0)
    {
        // TODO: if game_scale is too large, it would be better to drop the
        // bad scale first. Also, it is possible to get an unusable but valid
        // layout -- this only really protects against cases that will crash.
        end(1, false,
            "Failed to find a valid window layout:"
            " screen too small or game_scale too large?");
    }
    crawl_view.hudsz.x = m_region_stat->mx;
    crawl_view.hudsz.y = m_region_stat->my;
    crawl_view.init_view();
}

bool TilesFramework::is_using_small_layout()
{
    if (Options.tile_use_small_layout == maybe_bool::maybe)
#ifndef __ANDROID__
        // Rough estimation of the minimum usable window size
        //   - width > stats font width * 45 + msg font width * 45
        //   - height > tabs area size (192) + stats font height * 11
        // Not using Options.tile_font_xxx_size because it's reset on new game
        return m_windowsz.x < (int)(m_stat_font->char_width()*45+m_msg_font->char_width()*45)
            || m_windowsz.y < (int)(192+m_stat_font->char_height()*11);
#else
        return true;
#endif
    else
        return bool(Options.tile_use_small_layout);
}

#define ZOOM_INC 0.1

void TilesFramework::zoom_dungeon(bool in)
{
#if defined(USE_TILE_LOCAL)
    fixedp<> &current_scale = m_map_mode_enabled ?  Options.tile_map_scale
                                                 :  Options.tile_viewport_scale;
    fixedp<> orig = current_scale;
    // max zoom relative to to tile size that keeps LOS in view
    fixedp<> max_zoom = fixedp<>(m_windowsz.y) / Options.tile_cell_pixels
                                      / ENV_SHOW_DIAMETER;
    current_scale = min(ceil(max_zoom*10)/10, max(0.2,
                    current_scale + (in ? ZOOM_INC : -ZOOM_INC)));
    do_layout(); // recalculate the viewport setup
    if (current_scale != orig)
        mprf(MSGCH_PROMPT, "Zooming to %.2f", (float) current_scale);
    redraw_screen(false);
    update_screen();
#endif
}

void TilesFramework::deactivate_tab()
{
    m_region_tab->deactivate_tab();
}

void TilesFramework::toggle_tab_icons()
{
    m_region_tab->toggle_tab_icons();
    resize();
    redraw_screen();
    update_screen();
}

void TilesFramework::autosize_minimap()
{
    const int horiz = (m_windowsz.x - m_stat_col - map_margin * 2) / GXM;
    const int vert = (m_statcol_bottom - (m_region_map->sy ? m_region_map->sy
                                                           : m_statcol_top)
                     - map_margin * 2) / GYM;

    int sz = min(horiz, vert);
    if (Options.tile_map_pixels)
        sz = min(sz, Options.tile_map_pixels);
    m_region_map->dx = m_region_map->dy = sz;
}

void TilesFramework::place_minimap()
{
    m_region_map  = new MapRegion(m_map_pixels);

    if (GXM * m_region_map->dx > m_windowsz.x - m_stat_col)
        autosize_minimap();

    m_region_map->resize(GXM, GYM);
    if (m_region_map->dy * GYM > m_statcol_bottom - m_statcol_top)
    {
        delete m_region_map;
        m_region_map = nullptr;
        return;
    }

    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_map);
    m_region_map->place(m_stat_col, m_region_stat->ey, map_margin);

    m_statcol_top = m_region_map->ey;
}

int TilesFramework::calc_tab_lines(const int num_elements)
{
    // Integer division rounded up
    return (num_elements - 1) / m_region_tab->mx + 1;
}

void TilesFramework::place_tab(int idx)
{
    if (idx == -1)
        return;

    int min_ln = 1, max_ln = 1;
    if (idx == TAB_SPELL)
    {
        if (you.spell_no == 0)
        {
            m_region_tab->enable_tab(TAB_SPELL);
            return;
        }
        max_ln = calc_tab_lines(you.spell_no);
    }
    else if (idx == TAB_ABILITY)
    {
        unsigned int talents = your_talents(false).size();
        if (talents == 0)
        {
            m_region_tab->enable_tab(TAB_ABILITY);
            return;
        }
        max_ln = calc_tab_lines(talents);
    }
    else if (idx == TAB_MONSTER)
        max_ln = max_mon_height;
    else if (idx == TAB_COMMAND)
        min_ln = max_ln = calc_tab_lines(m_region_cmd->n_common_commands);
    else if (idx == TAB_COMMAND2)
        min_ln = max_ln = calc_tab_lines(m_region_cmd_meta->n_common_commands);
    else if (idx == TAB_NAVIGATION)
        min_ln = max_ln = calc_tab_lines(m_region_cmd_map->n_common_commands);
    else if (idx == TAB_SKILL)
        min_ln = max_ln = calc_tab_lines(NUM_SKILLS);

    int lines = min(max_ln, (m_statcol_bottom - m_statcol_top - m_tab_margin)
                            / m_region_tab->dy);
    if (lines >= min_ln)
    {
        TabbedRegion* region_tab = new TabbedRegion(m_init);
        region_tab->push_tab_region(m_region_tab->get_tab_region(idx),
                                    m_region_tab->get_tab_tile(idx));
        m_tabs[idx] = region_tab;
        region_tab->activate_tab(0);
        m_region_tab->disable_tab(idx);
        m_layers[LAYER_NORMAL].m_regions.push_back(region_tab);
        region_tab->place(m_stat_col, m_statcol_bottom
                                     - lines * m_region_tab->dy);
        region_tab->resize(m_region_tab->mx, lines);
        m_statcol_bottom = region_tab->sy - m_tab_margin;
    }
    else
        m_region_tab->enable_tab(idx);
}

void TilesFramework::resize_inventory()
{
    int lines = min(max_inv_height - min_inv_height,
                    (m_statcol_bottom - m_statcol_top) / m_region_tab->dy);

    int prev_size = m_region_tab->wy;

    int tabs_height = m_region_tab->min_height_for_items();
    tabs_height = round_up_to_multiple(tabs_height, m_region_tab->dx)/m_region_tab->dx;

    m_region_tab->resize(m_region_tab->mx, max(min_inv_height + lines, tabs_height));
    m_region_tab->place(m_stat_col, m_windowsz.y - m_region_tab->wy);

    int delta_y = m_region_tab->wy - prev_size;
    for (tab_iterator it = m_tabs.begin(); it != m_tabs.end(); ++it)
        it->second->place(it->second->sx, it->second->sy - delta_y);

    m_statcol_bottom -= delta_y;
}

void TilesFramework::layout_statcol()
{
    bool use_small_layout = is_using_small_layout();
    if (in_headless_mode())
        return;

    for (tab_iterator it = m_tabs.begin(); it != m_tabs.end(); ++it)
    {
        delete it->second;
        m_layers[LAYER_NORMAL].m_regions.pop_back();
    }
    m_tabs.clear();

    if (m_region_map)
    {
        delete m_region_map;
        m_region_map = nullptr;
        m_layers[LAYER_NORMAL].m_regions.pop_back();
    }

    if (use_small_layout)
    {
        // * commands will be on right as tabs
        // * stats will be squeezed in gap between dungeon and commands
        m_statcol_top = 0;
        m_statcol_bottom = m_windowsz.y;

        // resize stats to be up to beginning of command tabs
        //  ... this works because the margin (ox) on m_region_tab contains the tabs themselves
        m_region_stat->resize_to_fit((m_windowsz.x - m_stat_x_divider) - m_region_tab->ox*m_region_tab->dx/32, m_statcol_bottom-m_statcol_top);
    }
    else
    {
        m_region_stat->resize(m_region_stat->mx, min_stat_height);

        m_statcol_top = m_region_stat->ey;

        // Set the inventory region to minimal size.
        m_region_tab->set_small_layout(false, m_windowsz);
        m_region_tab->place(m_stat_col, m_statcol_top);
        m_region_tab->resize_to_fit(m_windowsz.x - m_region_tab->sx,
                                    m_windowsz.y - m_region_tab->sy);
        // region extends ~1/2-tile beyond window (rendered area touches right edge)
        m_region_tab->resize(m_region_tab->mx+1, min_inv_height);
        m_region_tab->place(m_stat_col, m_windowsz.y - m_region_tab->wy);
        m_statcol_bottom = m_region_tab->sy - m_tab_margin;

        m_region_stat->resize(m_region_stat->mx, min_stat_height);
        m_statcol_top += m_region_stat->dy;
        bool resized_inventory = false;

        for (const string &str : Options.tile_layout_priority)
        {
            if (str == "inventory")
            {
                resize_inventory();
                resized_inventory = true;
            }
            else if (str == "minimap" || str == "map")
            {
                if (!resized_inventory)
                {
                    resize_inventory();
                    resized_inventory = true;
                }
                place_minimap();
            }
            else if (!(str == "gold_turn" || str == "gold_turns")) // gold_turns no longer does anything
                place_tab(m_region_tab->find_tab(str));
        }
        // We stretch the minimap so it is centered in the space left.
        if (m_region_map)
        {
            autosize_minimap();

            m_region_map->place(m_region_stat->sx, m_region_stat->ey,
                                m_region_stat->ex, m_statcol_bottom,
                                map_margin);
            tile_new_level(false, false);
        }
    }
}

void TilesFramework::clrscr()
{
    TextRegion::cursor_region = nullptr;

    if (m_region_stat)
        m_region_stat->clear();
    if (m_region_msg)
        m_region_msg->clear();
    if (m_region_crt)
        m_region_crt->clear();

    cgotoxy(1,1);

    set_need_redraw();
}

int TilesFramework::get_number_of_lines()
{
    return m_region_crt->my;
}

int TilesFramework::get_number_of_cols()
{
    switch (m_active_layer)
    {
    default:
        return 0;
    case LAYER_NORMAL:
        return m_region_msg->mx;
    case LAYER_CRT:
        return m_region_crt->mx;
    }
}

void TilesFramework::cgotoxy(int x, int y, GotoRegion region)
{
    set_cursor_region(region);
    TextRegion::cgotoxy(x, y);
}

GotoRegion TilesFramework::get_cursor_region() const
{
    if (TextRegion::text_mode == m_region_crt)
        return GOTO_CRT;
    if (TextRegion::text_mode == m_region_msg)
        return GOTO_MSG;
    if (TextRegion::text_mode == m_region_stat)
        return GOTO_STAT;

    die("Bogus region");
    return GOTO_CRT;
}

void TilesFramework::set_cursor_region(GotoRegion region)
{
    switch (region)
    {
    case GOTO_CRT:
        m_active_layer = LAYER_CRT;
        TextRegion::text_mode = m_region_crt;
        break;
    case GOTO_MSG:
        m_active_layer = LAYER_NORMAL;
        TextRegion::text_mode = m_region_msg;
        break;
    case GOTO_STAT:
        m_active_layer = LAYER_NORMAL;
        TextRegion::text_mode = m_region_stat;
        break;
    default:
        die("invalid cgotoxy region in tiles: %d", region);
        break;
    }
}

// #define DEBUG_TILES_REDRAW
void TilesFramework::redraw()
{
#ifdef DEBUG_TILES_REDRAW
    cprintf("\nredrawing tiles");
#endif
    m_need_redraw = false;
    if (in_headless_mode())
        return;

    glmanager->reset_view_for_redraw();

    for (Region *region : m_layers[m_active_layer].m_regions)
        region->render();

    // Draw tooltip
    if (Options.tile_tooltip_ms > 0 && !m_tooltip.empty())
    {
        const int buffer = 5;
        const coord_def min_pos = coord_def() + buffer;
        const coord_def max_pos = m_windowsz - buffer;
        m_tip_font->render_tooltip(m_mouse.x, m_mouse.y,
                formatted_string(m_tooltip), min_pos, max_pos);
    }
    wm->swap_buffers();

    m_last_tick_redraw = wm->get_ticks();
}

void TilesFramework::maybe_redraw_screen()
{
    // need to call with show_updates=false, which is passed to viewwindow
    if (m_active_layer == LAYER_NORMAL && !crawl_state.game_is_arena())
    {
        redraw_screen(false);
        update_screen();
    }
}

void TilesFramework::render_current_regions()
{
    for (Region *region : m_layers[m_active_layer].m_regions)
        region->render();
}

void TilesFramework::update_minimap(const coord_def& gc)
{
    if (!m_region_map || !map_bounds(gc))
        return;

    map_feature mf;

    if (!crawl_state.game_is_arena() && gc == you.pos() && you.on_current_level)
        mf = MF_PLAYER;
    else
        mf = get_cell_map_feature(gc);

    // XXX: map_cell show have exclusion info
    if (mf == MF_FLOOR || mf == MF_MAP_FLOOR || mf == MF_WATER
        || mf == MF_DEEP_WATER || mf == MF_LAVA)
    {
        if (is_exclude_root(gc))
            mf = MF_EXCL_ROOT;
        else if (is_excluded(gc))
            mf = MF_EXCL;
    }

    m_region_map->set(gc, mf);
}

void TilesFramework::clear_minimap()
{
    if (m_region_map)
        m_region_map->clear();
}

void TilesFramework::update_minimap_bounds()
{
    if (m_region_map)
        m_region_map->update_bounds();
}

void TilesFramework::update_tabs()
{
    if (Options.tile_show_items.empty() || crawl_state.game_is_arena()
        || in_headless_mode())
    {
        return;
    }

    m_region_tab->update();
    for (tab_iterator it = m_tabs.begin(); it != m_tabs.end(); ++it)
        it->second->update();
}

void TilesFramework::toggle_inventory_display()
{
    int idx = m_region_tab->active_tab();
    m_region_tab->activate_tab((idx + 1) % m_region_tab->num_tabs());
}

void TilesFramework::place_cursor(cursor_type type, const coord_def &gc)
{
    if (in_headless_mode())
        return;
    m_region_tile->place_cursor(type, gc);
}

void TilesFramework::grid_to_screen(const coord_def &gc, coord_def *pc) const
{
    m_region_tile->to_screen_coords(gc, pc);
}

void TilesFramework::clear_text_tags(text_tag_type type)
{
    if (in_headless_mode())
        return;
    m_region_tile->clear_text_tags(type);
}

void TilesFramework::add_text_tag(text_tag_type type, const string &tag,
                                  const coord_def &gc)
{
    if (in_headless_mode())
        return;
    m_region_tile->add_text_tag(type, tag, gc);
}

void TilesFramework::add_text_tag(text_tag_type /*type*/, const monster_info& mon)
{
    // HACK. Large-tile monsters don't interact well with name tags.
    if (mons_class_flag(mon.type, M_TALL_TILE)
        || mons_class_flag(mon.base_type, M_TALL_TILE)
        || mon.mb[MB_NO_NAME_TAG])
    {
        return;
    }

    const coord_def &gc = mon.pos;

    if (mons_is_pghost(mon.type))
    {
        // Beautification hack. "Foo's ghost" is a little bit
        // verbose as a tag. "Foo" on its own should be sufficient.
        tiles.add_text_tag(TAG_NAMED_MONSTER, mon.mname, gc);
    }
    else
        tiles.add_text_tag(TAG_NAMED_MONSTER, mon.proper_name(DESC_PLAIN), gc);
}

const coord_def &TilesFramework::get_cursor() const
{
    return m_region_tile->get_cursor();
}

void TilesFramework::set_need_redraw(unsigned int min_tick_delay)
{
    if (in_headless_mode())
        return;
    unsigned int ticks = (wm->get_ticks() - m_last_tick_redraw);
    if (min_tick_delay && ticks <= min_tick_delay)
        return;

    m_need_redraw = true;
}

bool TilesFramework::need_redraw() const
{
    return m_need_redraw;
}

int TilesFramework::to_lines(int num_tiles, int tile_height)
{
    return num_tiles * tile_height / get_crt_font()->char_height();
}
#endif
