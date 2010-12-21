#include "AppHdr.h"

#ifdef USE_TILE

#include "artefact.h"
#include "cio.h"
#include "coord.h"
#include "directn.h"
#include "env.h"
#include "files.h"
#include "glwrapper.h"
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
#include "tilefont.h"
#include "tilereg.h"
#include "tilereg-crt.h"
#include "tilereg-dgn.h"
#include "tilereg-doll.h"
#include "tilereg-grid.h"
#include "tilereg-inv.h"
#include "tilereg-map.h"
#include "tilereg-mem.h"
#include "tilereg-menu.h"
#include "tilereg-mon.h"
#include "tilereg-msg.h"
#include "tilereg-spl.h"
#include "tilereg-skl.h"
#include "tilereg-stat.h"
#include "tilereg-tab.h"
#include "tilereg-text.h"
#include "tilereg-title.h"
#include "tilesdl.h"
#include "travel.h"
#include "view.h"
#include "viewgeom.h"
#include "windowmanager.h"

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

// Default Screen Settings
// width, height, map, crt, stat, msg, tip, lbl
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

TilesFramework tiles;

TilesFramework::TilesFramework() :
    m_windowsz(1024, 768),
    m_viewsc(0, 0),
    m_fullscreen(false),
    m_need_redraw(false),
    m_active_layer(LAYER_CRT),
    m_buttons_held(0),
    m_key_mod(0),
    m_mouse(-1, -1),
    m_last_tick_moved(0),
    m_last_tick_redraw(0)
{
}

TilesFramework::~TilesFramework()
{
}

static void _init_consoles()
{
#ifdef TARGET_OS_WINDOWS
    typedef BOOL (WINAPI *ac_func)(DWORD);
    ac_func attach_console = (ac_func)GetProcAddress(
        GetModuleHandle(TEXT("kernel32.dll")), "AttachConsole");

    if (attach_console)
    {
        // Redirect output to the console
        attach_console((DWORD)-1);
        freopen("CONOUT$", "wb", stdout);
        freopen("CONOUT$", "wb", stderr);
    }
#endif
}

static void _shutdown_console()
{
#ifdef TARGET_OS_WINDOWS
    typedef BOOL (WINAPI *fc_func)(void);
    fc_func free_console = (fc_func)GetProcAddress(
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
    delete m_region_tsp;
    delete m_region_inv;
    delete m_region_spl;
    delete m_region_mem;
    delete m_region_mon;
    delete m_region_crt;
    delete m_region_menu;

    m_region_tile  = NULL;
    m_region_stat  = NULL;
    m_region_msg   = NULL;
    m_region_map   = NULL;
    m_region_tab   = NULL;
    m_region_tsp   = NULL;
    m_region_inv   = NULL;
    m_region_spl   = NULL;
    m_region_mem   = NULL;
    m_region_mon   = NULL;
    m_region_crt   = NULL;
    m_region_menu  = NULL;

    for (unsigned int i = 0; i < LAYER_MAX; i++)
        m_layers[i].m_regions.clear();

    for (unsigned int i = 0; i < m_fonts.size(); i++)
    {
        delete m_fonts[i].font;
        m_fonts[i].font = NULL;
    }

    delete m_image;

    GLStateManager::shutdown();
    WindowManager::shutdown();

    _shutdown_console();
}

/**
 * Creates a new title region and sets it active
 * Remember to call hide_title() when you're done
 * showing the title.
 */
void TilesFramework::draw_title()
{
    TitleRegion* reg = new TitleRegion(m_windowsz.x, m_windowsz.y,
                                       m_fonts[m_msg_font].font);

    m_layers[LAYER_TILE_CONTROL].m_regions.push_back(reg);
    m_active_layer = LAYER_TILE_CONTROL;
    redraw();
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
    ASSERT(m_layers[LAYER_TILE_CONTROL].m_regions.size() == 1);
    ASSERT(m_active_layer == LAYER_TILE_CONTROL);
    TitleRegion* reg = dynamic_cast<TitleRegion*>(
            m_layers[LAYER_TILE_CONTROL].m_regions.at(0));
    reg->update_message(message);
    redraw();
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
    ASSERT(m_layers[LAYER_TILE_CONTROL].m_regions.size() == 1);
    ASSERT(m_active_layer == LAYER_TILE_CONTROL);
    TitleRegion* reg = dynamic_cast<TitleRegion*>(
            m_layers[LAYER_TILE_CONTROL].m_regions.at(0));
    redraw();
    reg->run();
    delete reg;
    m_layers[LAYER_TILE_CONTROL].m_regions.clear();
}

void TilesFramework::draw_doll_edit()
{
    DollEditRegion* reg = new DollEditRegion(m_image,
                                             m_fonts[m_msg_font].font);
    use_control_region(reg);
    delete reg;
}

void TilesFramework::use_control_region(ControlRegion *reg)
{
    m_layers[LAYER_TILE_CONTROL].m_regions.push_back(reg);
    m_active_layer = LAYER_TILE_CONTROL;
    set_need_redraw();

    reg->run();

    m_layers[LAYER_TILE_CONTROL].m_regions.clear();
}

void TilesFramework::calculate_default_options()
{
    // Find which set of _screen_sizes to use.
    int auto_size = 0;
    int num_screen_sizes = sizeof(_screen_sizes) / sizeof(_screen_sizes[0]);
    do
    {
        if (m_windowsz.x >= _screen_sizes[auto_size][0]
            && m_windowsz.y >= _screen_sizes[auto_size][1])
        {
            break;
        }
    }
    while (++auto_size < num_screen_sizes - 1);

    // Auto pick map and font sizes if option is zero.
#define AUTO(x,y) (x = (x) ? (x) : _screen_sizes[auto_size][(y)])
    AUTO(Options.tile_map_pixels, 2);
    AUTO(Options.tile_font_crt_size, 3);
    AUTO(Options.tile_font_stat_size, 4);
    AUTO(Options.tile_font_msg_size, 5);
    AUTO(Options.tile_font_tip_size, 6);
    AUTO(Options.tile_font_lbl_size, 7);
#undef AUTO
}

bool TilesFramework::initialise()
{
    _init_consoles();

    const char *icon_name =
#ifdef DATA_DIR_PATH
    DATA_DIR_PATH
#endif
#ifdef TARGET_OS_WINDOWS
    "dat/tiles/stone_soup_icon-win32.png";
#else
    "dat/tiles/stone_soup_icon-32x32.png";
#endif

    std::string title = CRAWL " " + Version::Long();

    // Do our initialization here.

    // Create an instance of UIWrapper for the library we were compiled for
    WindowManager::create();
    if (!wm)
        return (false);

    // Initialize the wrapper
    if (!wm->init(&m_windowsz))
        return (false);

    wm->set_window_title(title.c_str());
    wm->set_window_icon(icon_name);

    // Copy over constants that need to have been set by the wrapper
    m_screen_width = wm->screen_width();
    m_screen_height = wm->screen_height();

    GLStateManager::init();

    m_image = new ImageManager();

    // If the window size is less than the view height, the textures will
    // have to be shrunk.  If this isn't the case, then don't create mipmaps,
    // as this appears to make things blurry on some users machines.
    bool need_mips = (m_windowsz.y < 32 * VIEW_MIN_HEIGHT);
    if (!m_image->load_textures(need_mips))
        return (false);

    calculate_default_options();

    m_crt_font    = load_font(Options.tile_font_crt_file.c_str(),
                              Options.tile_font_crt_size, true, true);
    m_msg_font    = load_font(Options.tile_font_msg_file.c_str(),
                              Options.tile_font_msg_size, true, false);
    int stat_font = load_font(Options.tile_font_stat_file.c_str(),
                              Options.tile_font_stat_size, true, false);
    m_tip_font    = load_font(Options.tile_font_tip_file.c_str(),
                              Options.tile_font_tip_size, true, false);
    int lbl_font  = load_font(Options.tile_font_lbl_file.c_str(),
                              Options.tile_font_lbl_size, true, true);

    if (m_crt_font == -1 || m_msg_font == -1 || stat_font == -1
        || m_tip_font == -1 || lbl_font == -1)
    {
        return (false);
    }

    TileRegionInit init(m_image, m_fonts[lbl_font].font, TILE_X, TILE_Y);
    m_region_tile = new DungeonRegion(init);
    m_region_map  = new MapRegion(Options.tile_map_pixels);
    m_region_tab  = new TabbedRegion(init);
    m_region_tsp  = new TabbedRegion(init);
    m_region_inv  = new InventoryRegion(init);
    m_region_spl  = new SpellRegion(init);
    m_region_mem  = new MemoriseRegion(init);
    m_region_mon  = new MonsterRegion(init);
    m_region_skl  = new SkillRegion(init);

    m_region_tab->set_tab_region(TAB_ITEM, m_region_inv, TILEG_TAB_ITEM);
    m_region_tab->set_tab_region(TAB_SPELL, m_region_spl, TILEG_TAB_SPELL);
    m_region_tab->set_tab_region(TAB_MEMORISE, m_region_mem, TILEG_TAB_MEMORISE);
    m_region_tab->set_tab_region(TAB_MONSTER, m_region_mon, TILEG_TAB_MONSTER);
    m_region_tab->set_tab_region(TAB_SKILL, m_region_skl, TILEG_TAB_SKILL);
    m_region_tab->activate_tab(TAB_ITEM);

    m_region_tsp->set_tab_region(0, m_region_spl, TILEG_TAB_SPELL);

    m_region_msg  = new MessageRegion(m_fonts[m_msg_font].font);
    m_region_stat = new StatRegion(m_fonts[stat_font].font);
    m_region_crt  = new CRTRegion(m_fonts[m_crt_font].font);

    m_region_menu = new MenuRegion(m_image, m_fonts[m_crt_font].font);

    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_map);
    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_tile);
    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_tab);
    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_msg);
    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_stat);

    m_layers[LAYER_CRT].m_regions.push_back(m_region_crt);
    m_layers[LAYER_CRT].m_regions.push_back(m_region_menu);

    cgotoxy(1, 1, GOTO_CRT);

    resize();

    return (true);
}

int TilesFramework::load_font(const char *font_file, int font_size,
                              bool default_on_fail, bool outline)
{
    for (unsigned int i = 0; i < m_fonts.size(); i++)
    {
        font_info &finfo = m_fonts[i];
        if (finfo.name == font_file && finfo.size == font_size
            && outline == finfo.outline)
        {
            return i;
        }
    }

    FontWrapper *font = FontWrapper::create();

    if (!font->load_font(font_file, font_size, outline))
    {
        delete font;
        if (default_on_fail)
            return (load_font(MONOSPACED_FONT, 12, false, outline));
        else
            return -1;
    }

    font_info finfo;
    finfo.font = font;
    finfo.name = font_file;
    finfo.size = font_size;
    finfo.outline = outline;
    m_fonts.push_back(finfo);

    return (m_fonts.size() - 1);
}
void TilesFramework::load_dungeon(const crawl_view_buffer &vbuf,
                                  const coord_def &gc)
{
    m_active_layer = LAYER_NORMAL;

    m_region_tile->load_dungeon(vbuf, gc);

    int ox = m_region_tile->mx/2;
    int oy = m_region_tile->my/2;
    coord_def win_start(gc.x - ox, gc.y - oy);
    coord_def win_end(gc.x + ox + 1, gc.y + oy + 1);

    m_region_map->set_window(win_start, win_end);
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

void TilesFramework::resize()
{
    calculate_default_options();
    do_layout();

    wm->resize(m_windowsz);
}

int TilesFramework::handle_mouse(MouseEvent &event)
{
    // Note: the mouse event goes to all regions in the active layer because
    // we want to be able to start some GUI event (e.g. far viewing) and
    // stop if it moves to another region.
    int return_key = 0;
    for (unsigned int i = 0; i < m_layers[m_active_layer].m_regions.size(); i++)
    {
        // TODO enne - what if two regions give a key?
        int key = 0;
        key = m_layers[m_active_layer].m_regions[i]->handle_mouse(event);
        if (key)
            return_key = key;
    }

    // Let regions take priority in any mouse mode.
    if (return_key)
        return return_key;

    // Handle "more" mode globally here, rather than duplicate across regions.
    if (mouse_control::current_mode() == MOUSE_MODE_MORE
        && event.event == MouseEvent::PRESS)
    {
        if (event.button == MouseEvent::LEFT)
            return CK_MOUSE_CLICK;
        else if (event.button == MouseEvent::RIGHT)
            return CK_MOUSE_CMD;
    }

    // TODO enne - in what cases should the buttons be returned?
#if 0
    // If nothing else, return the mouse button that was pressed.
    switch (event.button)
    {
    case MouseEvent::LEFT:
        return CK_MOUSE_B1;
    case MouseEvent::RIGHT:
        return CK_MOUSE_B2;
    case MouseEvent::MIDDLE:
        return CK_MOUSE_B3;
    case MouseEvent::SCROLL_UP:
        return CK_MOUSE_B4;
    case MouseEvent::SCROLL_DOWN:
        return CK_MOUSE_B5;
    default:
    case MouseEvent::NONE:
        return 0;
    }
#endif

    return 0;
}

static unsigned int _timer_callback(unsigned int ticks)
{
    // force the event loop to break
    wm->raise_custom_event();

    unsigned int res = Options.tile_tooltip_ms;
    return (res);
}

int TilesFramework::getch_ck()
{
    wm_event event;
    cursor_loc cur_loc;
    cursor_loc tip_loc;
    cursor_loc last_loc;

    int key = 0;

    // Don't update tool tips etc. in targeting mode.
    const bool mouse_target_mode
                = (mouse_control::current_mode() == MOUSE_MODE_TARGET_PATH
                   || mouse_control::current_mode() == MOUSE_MODE_TARGET_DIR);

    const unsigned int ticks_per_screen_redraw = Options.tile_update_rate;

    unsigned int res = Options.tile_tooltip_ms;
    wm->set_timer(res, &_timer_callback);

    m_tooltip.clear();
    std::string prev_alt = m_region_msg->alt_text();
    m_region_msg->alt_text().clear();

    if (need_redraw())
        redraw();

    while (!key)
    {
        unsigned int ticks = 0;
        last_loc = m_cur_loc;

        if (wm->wait_event(&event))
        {
            ticks = wm->get_ticks();
            if (!mouse_target_mode)
            {
                if (event.type != WM_CUSTOMEVENT)
                {
                    tiles.clear_text_tags(TAG_CELL_DESC);
                    m_region_msg->alt_text().clear();
                }

                if (tip_loc != m_cur_loc)
                {
                    m_region_msg->alt_text().clear();
                    for (unsigned int i = 0;
                         i < m_layers[m_active_layer].m_regions.size(); ++i)
                    {
                        Region *reg = m_layers[m_active_layer].m_regions[i];
                        if (!reg->inside(m_mouse.x, m_mouse.y))
                            continue;
                        if (reg->update_alt_text(m_region_msg->alt_text()))
                        {
                            break;
                        }
                    }

                    if (prev_alt != m_region_msg->alt_text())
                    {
                        prev_alt = m_region_msg->alt_text();
                        set_need_redraw();
                    }
                }
            }

            switch (event.type)
            {
            case WM_ACTIVEEVENT:
                // When game gains focus back then set mod state clean
                // to get rid of stupid Windows/SDL bug with Alt-Tab.
                if (event.active.gain != 0)
                {
                    wm->set_mod_state(MOD_NONE);
                    set_need_redraw();
                }
                break;
            case WM_KEYDOWN:
                m_key_mod |= event.key.keysym.key_mod;
                key        = event.key.keysym.sym;
                m_region_tile->place_cursor(CURSOR_MOUSE, Region::NO_CURSOR);

                // If you hit a key, disable tooltips until the mouse
                // is moved again.
                m_last_tick_moved = ~0;
                break;

            case WM_KEYUP:
                m_key_mod &= ~event.key.keysym.key_mod;
                m_last_tick_moved = ~0;
                break;

            case WM_MOUSEMOTION:
                {
                    // Record mouse pos for tooltip timer
                    // FIXME: This compares signed and unsigned ints
                    if (m_mouse.x != abs(event.mouse_event.px)
                        || m_mouse.y != abs(event.mouse_event.py))
                    {
                        m_last_tick_moved = ticks;
                    }
                    m_mouse.x = event.mouse_event.px;
                    m_mouse.y = event.mouse_event.py;

                    event.mouse_event.held = m_buttons_held;
                    event.mouse_event.mod  = m_key_mod;
                    int mouse_key = handle_mouse(event.mouse_event);

                    // find mouse location
                    for (unsigned int i = 0;
                        i < m_layers[m_active_layer].m_regions.size(); i++)
                    {
                        Region *reg = m_layers[m_active_layer].m_regions[i];
                        if (reg->mouse_pos(m_mouse.x, m_mouse.y,
                                           m_cur_loc.cx, m_cur_loc.cy))
                        {
                            m_cur_loc.reg = reg;
                            m_cur_loc.mode = mouse_control::current_mode();
                            break;
                        }
                    }

                    // Don't break back to Crawl and redraw for every mouse
                    // event, because there's a lot of them.
                    //
                    // If there are other mouse events in the queue
                    // (possibly because redrawing is slow or the user
                    // is moving the mouse really quickly), process those
                    // first, before bothering to redraw the screen.
                    unsigned int count = wm->get_event_count(WM_MOUSEMOTION);
                    ASSERT(count >= 0);
                    if (count > 0)
                        continue;

                    // Stay within this input loop until the mouse moves
                    // to a semantically different location.  Crawl doesn't
                    // care about small mouse movements.
                    if (!need_redraw() && last_loc == m_cur_loc)
                        continue;

                    key = mouse_key;
               }
               break;

            case WM_MOUSEBUTTONUP:
                {
                    m_buttons_held  &= ~(event.mouse_event.button);
                    event.mouse_event.held = m_buttons_held;
                    event.mouse_event.mod  = m_key_mod;
                    key = handle_mouse(event.mouse_event);
                    m_last_tick_moved = ticks;
                }
                break;

            case WM_MOUSEBUTTONDOWN:
                {
                    m_buttons_held  |= event.mouse_event.button;
                    event.mouse_event.held = m_buttons_held;
                    event.mouse_event.mod  = m_key_mod;
                    key = handle_mouse(event.mouse_event);
                    m_last_tick_moved = ticks;
                }
                break;

            case WM_QUIT:
                if (crawl_state.need_save)
                    save_game(true);
                exit(0);
                break;

            case WM_CUSTOMEVENT:
            default:
                // This is only used to refresh the tooltip.
                break;
            }
        }

        if (!mouse_target_mode)
        {
            const bool timeout = (ticks > m_last_tick_moved
                                  && (ticks - m_last_tick_moved
                                      > (unsigned int)Options.tile_tooltip_ms));

            if (timeout)
            {
                tip_loc = m_cur_loc;
                tiles.clear_text_tags(TAG_CELL_DESC);
                if (Options.tile_tooltip_ms > 0 && m_tooltip.empty())
                {
                    for (unsigned int i = 0;
                         i < m_layers[m_active_layer].m_regions.size(); ++i)
                    {
                        Region *reg = m_layers[m_active_layer].m_regions[i];
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
                if (last_loc != m_cur_loc)
                    set_need_redraw();

                m_tooltip.clear();
                tip_loc.reset();
            }

            if (need_redraw()
                || ticks > m_last_tick_redraw
                   && ticks - m_last_tick_redraw > ticks_per_screen_redraw)
            {
                redraw();
            }
        }
    }

    // We got some input, so we'll probably have to redraw something.
    set_need_redraw();

    wm->set_timer(0, NULL);

    return key;
}

static const int map_margin      = 2;
static const int tab_margin      = 5;
static const int map_stat_margin = 4;
static const int crt_width       = 80;
static const int crt_height      = 30;

// Width of status area in characters.
static const int stat_width      = 42;

/**
 * Calculates and sets the layout of the main game screen, based on the
 * available screen estate (window or screensize) and options.
 */
void TilesFramework::do_layout()
{
    // View size in pixels is (m_viewsc * crawl_view.viewsz)
    m_viewsc.x = 32;
    m_viewsc.y = 32;

    crawl_view.viewsz.x = Options.view_max_width;
    crawl_view.viewsz.y = Options.view_max_height;

    // Initial sizes.
    m_region_tile->dx = m_viewsc.x;
    m_region_tile->dy = m_viewsc.y;

    // Locations in pixels. stat_x_divider is the dividing vertical line
    // between dungeon view on the left and status area on the right.
    // message_y_divider is the horizontal line between dungeon view on
    // the top and message window at the bottom.
    int stat_x_divider = 0;
    int message_y_divider = 0;

    bool message_overlay = Options.tile_force_overlay ? true : false;

    // We ignore Options.view_max_width and use the maximum space available.
    int available_width_in_tiles = 0;

    available_width_in_tiles = (m_windowsz.x - stat_width
                                * m_region_stat->dx) / m_region_tile->dx;

    // Scale the dungeon region tiles so we have enough space to
    // display full LOS.
    if (available_width_in_tiles < ENV_SHOW_DIAMETER)
    {
        m_region_tile->dx = (m_windowsz.x - stat_width * m_region_stat->dx)
                            / ENV_SHOW_DIAMETER;
        m_region_tile->dy = m_region_tile->dx;
        available_width_in_tiles = ENV_SHOW_DIAMETER;
    }

    stat_x_divider = available_width_in_tiles * m_region_tile->dx;

    // Calculate message_y_divider. First off, if we have already decided to
    // use the overlay, we can place the divider to the bottom of the screen.
    if (message_overlay)
    {
        message_y_divider = m_windowsz.y;
    }

    // Then, the optimal situation without the overlay - we can fit both
    // Options.view_max_height and at least Options.msg_min_height in the space.
    if (std::max(Options.view_max_height, ENV_SHOW_DIAMETER)
        * m_region_tile->dy + Options.msg_min_height
        * m_region_msg->dy
        <= m_windowsz.y && !message_overlay)
    {
        message_y_divider = std::max(Options.view_max_height, ENV_SHOW_DIAMETER)
                            * m_region_tile->dy;
        message_y_divider = std::max(message_y_divider, m_windowsz.y -
                                    Options.msg_max_height * m_region_msg->dy);
    }
    else
    {
        int available_height_in_tiles = 0;
        available_height_in_tiles = (m_windowsz.y - (message_overlay
                                     ? 0 : (Options.msg_min_height
                                     * m_region_msg->dy)))
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
        {
            message_y_divider = available_height_in_tiles * m_region_tile->dy;
        }
    }

    // Resize and place the dungeon region.
    m_region_tile->resize_to_fit(stat_x_divider, message_y_divider);
    m_region_tile->place(0, 0, 0);

    crawl_view.viewsz.x = m_region_tile->mx;
    crawl_view.viewsz.y = m_region_tile->my;

    // Resize and place the message window.
    if (message_overlay)
    {
       m_region_msg->place(0, 0, 0); // TODO: Maybe add an option to place
                                     // overlay at the bottom.
       m_region_msg->resize_to_fit(stat_x_divider, Options.msg_min_height
                                   * m_region_msg->dy);
       m_region_msg->set_overlay(message_overlay);
    }
    else
    {
        m_region_msg->resize_to_fit(stat_x_divider, m_windowsz.y
                                    - message_y_divider);
        m_region_msg->place(0, m_region_tile->ey, 0);
    }

    crawl_view.msgsz.x = m_region_msg->mx;
    crawl_view.msgsz.y = m_region_msg->my;

    m_region_stat->resize_to_fit(m_windowsz.x - stat_x_divider, m_windowsz.y);
    m_region_stat->place(stat_x_divider + map_stat_margin, 0, 0);

    // Fit the minimap into place.
    m_region_map->dx = m_region_map->dy = Options.tile_map_pixels;

    if (GXM * m_region_map->dx > m_windowsz.x - stat_x_divider)
    {
        m_region_map->dx = (m_windowsz.x - stat_x_divider - map_margin * 2)
                            / GXM;
        m_region_map->dy = m_region_map->dx;
    }

    m_region_map->resize(GXM, GYM);
    m_region_map->place(stat_x_divider + map_stat_margin,
                        m_region_stat->ey, map_margin);

    // If show_gold_turns isn't turned on, try turning it on if there's room.
    if (!Options.show_gold_turns)
    {
        if (layout_statcol(message_overlay, true))
            Options.show_gold_turns = true;
        else
            layout_statcol(message_overlay, false);
    }
    else
    {
        layout_statcol(message_overlay, true);
    }

    m_region_crt->place(0, 0, 0);
    m_region_crt->resize_to_fit(m_windowsz.x, m_windowsz.y);

    m_region_menu->place(0, 0, 0);
    m_region_menu->resize_to_fit(m_windowsz.x, m_windowsz.y);

    crawl_view.init_view();
}

bool TilesFramework::layout_statcol(bool message_overlay, bool show_gold_turns)
{
    // Assumes that the region_stat has already been placed.
    int hud_height = 12 + (show_gold_turns ? 1 : 0);
    m_region_stat->resize(m_region_stat->mx, hud_height);
    crawl_view.hudsz.y = hud_height;
    m_region_map->place(m_region_stat->sx, m_region_stat->ey,
                        m_region_stat->ex, m_region_stat->ey + m_region_map->wy,
                        map_margin);

    int inv_col = std::max(m_region_tile->ex, m_region_msg->ex);
    if (message_overlay)
        inv_col = m_region_stat->sx;

    m_region_tab->place(inv_col, m_region_map->ey);
    m_region_tab->resize_to_fit(m_windowsz.x - m_region_tab->sx,
                                m_windowsz.y - m_region_tab->sy);
    m_region_tab->resize(std::min(13, (int)m_region_tab->mx),
                         std::min(6, (int)m_region_tab->my));

    int self_inv_y = m_windowsz.y - m_region_tab->wy;
    m_region_tab->place(inv_col, self_inv_y);

    // Integer divison rounded up
    int lines = (m_region_spl->get_max_slots() - 1) / m_region_tab->mx + 1;
    int delta_y = m_region_tab->dy * lines + tab_margin;

    if (delta_y < m_region_tab->sy - m_region_map->ey && you.spell_no > 0)
    {
        m_region_tab->disable_tab(TAB_SPELL);
        m_region_tsp->enable_tab(0);
        m_region_tsp->activate_tab(0);
        m_layers[LAYER_NORMAL].m_regions.push_back(m_region_tsp);
        m_region_tsp->place(inv_col, m_region_tab->sy - delta_y);
        m_region_tsp->resize(m_region_tab->mx, lines);
    }
    else
    {
        m_region_tsp->disable_tab(0);
        m_region_tab->enable_tab(TAB_SPELL);
    }


    int num_items = m_region_tab->mx * (m_region_tab->my - 1);
    return (num_items >= ENDOFPACK);
}

void TilesFramework::clrscr()
{
    TextRegion::cursor_region = NULL;

    if (m_region_stat)
        m_region_stat->clear();
    if (m_region_msg)
        m_region_msg->clear();
    if (m_region_crt)
        m_region_crt->clear();
    if (m_region_menu)
        m_region_menu->clear();

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
        DEBUGSTR("invalid cgotoxy region in tiles: %d", region);
        break;
    }

    TextRegion::cgotoxy(x, y);
}

GotoRegion TilesFramework::get_cursor_region() const
{
    if (TextRegion::text_mode == m_region_crt)
        return (GOTO_CRT);
    if (TextRegion::text_mode == m_region_msg)
        return (GOTO_MSG);
    if (TextRegion::text_mode == m_region_stat)
        return (GOTO_STAT);

    ASSERT(!"Bogus region");
    return (GOTO_CRT);
}

// #define DEBUG_TILES_REDRAW
void TilesFramework::redraw()
{
#ifdef DEBUG_TILES_REDRAW
    cprintf("\nredrawing tiles");
#endif
    m_need_redraw = false;

    glmanager->reset_view_for_redraw(m_viewsc.x, m_viewsc.y);

    for (unsigned int i = 0; i < m_layers[m_active_layer].m_regions.size(); ++i)
        m_layers[m_active_layer].m_regions[i]->render();

    // Draw tooltip
    if (Options.tile_tooltip_ms > 0 && !m_tooltip.empty())
    {
        const coord_def min_pos(0, 0);
        FontWrapper *font = m_fonts[m_tip_font].font;

        font->render_string(m_mouse.x, m_mouse.y - 2, m_tooltip.c_str(),
                            min_pos, m_windowsz, WHITE, false, 220, BLUE, 5,
                            true);
    }
    wm->swap_buffers();

    m_last_tick_redraw = wm->get_ticks();
}

static map_feature get_cell_map_feature(const map_cell& cell)
{
    map_feature mf = MF_SKIP;
    if (cell.invisible_monster())
        mf = MF_MONS_HOSTILE;
    else if (cell.monster() != MONS_NO_MONSTER)
    {
        switch (cell.monsterinfo() ? cell.monsterinfo()->attitude : ATT_HOSTILE)
        {
        case ATT_FRIENDLY:
            mf = MF_MONS_FRIENDLY;
            break;
        case ATT_GOOD_NEUTRAL:
            mf = MF_MONS_PEACEFUL;
            break;
        case ATT_NEUTRAL:
        case ATT_STRICT_NEUTRAL:
            mf = MF_MONS_NEUTRAL;
            break;
        case ATT_HOSTILE:
        default:
            if (mons_class_flag(cell.monster(), M_NO_EXP_GAIN))
                mf = MF_MONS_NO_EXP;
            else
                mf = MF_MONS_HOSTILE;
            break;
        }
    }
    else if (cell.cloud())
    {
        show_type show;
        show.cls = SH_CLOUD;
        mf = get_feature_def(show).minimap;
    }

    if (mf == MF_SKIP && cell.item())
        mf = get_feature_def(*cell.item()).minimap;
    if (mf == MF_SKIP)
        mf = get_feature_def(cell.feat()).minimap;
    if (mf == MF_SKIP)
        mf = MF_UNSEEN;

    if (mf == MF_WALL || mf == MF_FLOOR)
    {
        if (cell.known() && !cell.seen()
            || cell.detected_item()
            || cell.detected_monster())
        {
            mf = (mf == MF_WALL) ? MF_MAP_WALL : MF_MAP_FLOOR;
        }
    }

    return mf;
}

void TilesFramework::update_minimap(const coord_def& gc)
{
    if (!player_in_mappable_area())
        return;

    map_feature mf;

    if (!crawl_state.game_is_arena() && gc == you.pos() && you.on_current_level)
        mf = MF_PLAYER;
    else
        mf = get_cell_map_feature(env.map_knowledge(gc));

    // XXX: map_cell show have exclusion info
    if (mf == MF_FLOOR || mf == MF_MAP_FLOOR || mf == MF_WATER)
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
    m_region_map->clear();
}

void TilesFramework::update_minimap_bounds()
{
    m_region_map->update_bounds();
}

void TilesFramework::update_inventory()
{
    if (!Options.tile_show_items || crawl_state.game_is_arena())
        return;

    m_region_tab->update();
}

void TilesFramework::toggle_inventory_display()
{
    int idx = m_region_tab->active_tab();
    m_region_tab->activate_tab((idx + 1) % m_region_tab->num_tabs());
}


void TilesFramework::place_cursor(cursor_type type, const coord_def &gc)
{
    m_region_tile->place_cursor(type, gc);
}

void TilesFramework::clear_text_tags(text_tag_type type)
{
    m_region_tile->clear_text_tags(type);
}

void TilesFramework::add_text_tag(text_tag_type type, const std::string &tag,
                                  const coord_def &gc)
{
    m_region_tile->add_text_tag(type, tag, gc);
}

void TilesFramework::add_text_tag(text_tag_type type, const monster* mon)
{
    // HACK.  Large-tile monsters don't interact well with name tags.
    if (mon->type == MONS_PANDEMONIUM_DEMON
        || mon->type == MONS_LERNAEAN_HYDRA)
    {
        return;
    }

    const coord_def &gc = mon->pos();

    if (mons_is_pghost(mon->type))
    {
        // Beautification hack.  "Foo's ghost" is a little bit
        // verbose as a tag.  "Foo" on its own should be sufficient.
        tiles.add_text_tag(TAG_NAMED_MONSTER, mon->mname, gc);
    }
    else
        tiles.add_text_tag(TAG_NAMED_MONSTER, mon->name(DESC_PLAIN), gc);
}

const coord_def &TilesFramework::get_cursor() const
{
    return (m_region_tile->get_cursor());
}

void TilesFramework::add_overlay(const coord_def &gc, tileidx_t idx)
{
    m_region_tile->add_overlay(gc, idx);
}

void TilesFramework::clear_overlays()
{
    m_region_tile->clear_overlays();
}

void TilesFramework::set_need_redraw(unsigned int min_tick_delay)
{
    unsigned int ticks = (wm->get_ticks() - m_last_tick_redraw);
    if (min_tick_delay && ticks <= min_tick_delay)
        return;

    m_need_redraw = true;
}

bool TilesFramework::need_redraw() const
{
    return m_need_redraw;
}

#endif
