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
#include "map_knowledge.h"
#include "menu.h"
#include "message.h"
#include "mon-util.h"
#include "options.h"
#include "player.h"
#include "state.h"
#include "tiledef-dngn.h"
#include "tiledef-gui.h"
#include "tiledef-main.h"
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
#include "tilereg-menu.h"
#include "tilereg-mon.h"
#include "tilereg-msg.h"
#include "tilereg-popup.h"
#include "tilereg-skl.h"
#include "tilereg-spl.h"
#include "tilereg-stat.h"
#include "tilereg-tab.h"
#include "tilereg-text.h"
#include "tilereg-title.h"
#include "tileview.h"
#include "travel.h"
#include "unwind.h"
#include "version.h"
#include "viewgeom.h"
#include "view.h"
#include "windowmanager.h"

#ifdef __ANDROID__
#include <android/log.h>
#include <GLES/gl.h>
//#include <SDL_android.h>
#endif

#ifdef TARGET_OS_WINDOWS
# include <windows.h>
#endif

// Default Screen Settings
// width, height, map, crt, stat, msg, tip, lbl
#ifdef TOUCH_UI
static int _screen_sizes[6][8] =
#else
static int _screen_sizes[4][8] =
#endif
{
    // Default
    {1024, 700, 3, 15, 16, 14, 15, 14},
    // Eee PC 900+
    {1024, 600, 2, 14, 14, 12, 13, 12},
    // Small screen
    {800, 600, 2, 14, 11, 12, 13, 12},
    // Eee PC
    {800, 480, 2, 13, 12, 10, 13, 11}
#ifdef TOUCH_UI
    // puny mobile screens
    ,{480, 320, 2, 9, 8, 8, 9, 8}
    ,{320, 240, 1, 8, 8, 6, 8, 6} // :(
#endif
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
    // Windows has an annoying habbit of disconnecting GUI apps from
    // consoles at startup, and insisting that console apps start with
    // consoles.
    //
    // We build tiles as a GUI app, so we work around this by
    // reconnecting ourselves to the parent process's console (if
    // any), on capable systems.

    // The AttachConsole() function is XP/2003 Server and up, so we
    // need to do the GetModuleHandle()/GetProcAddress() dance.
    typedef BOOL (WINAPI *ac_func)(DWORD);
    ac_func attach_console = (ac_func)GetProcAddress(
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
    delete m_region_inv;
    delete m_region_spl;
    delete m_region_mem;
    delete m_region_abl;
    delete m_region_mon;
    delete m_region_crt;
    delete m_region_menu;

    m_region_tile  = nullptr;
    m_region_stat  = nullptr;
    m_region_msg   = nullptr;
    m_region_map   = nullptr;
    m_region_tab   = nullptr;
    m_region_inv   = nullptr;
    m_region_spl   = nullptr;
    m_region_mem   = nullptr;
    m_region_abl   = nullptr;
    m_region_mon   = nullptr;
    m_region_crt   = nullptr;
    m_region_menu  = nullptr;

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
void TilesFramework::update_title_msg(string message)
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

void TilesFramework::set_map_display(const bool display)
{
    m_map_mode_enabled = display;
    if (!display && !tiles.is_using_small_layout())
        m_region_tab->activate_tab(TAB_ITEM);
}

bool TilesFramework::get_map_display()
{
    return m_map_mode_enabled;
}

void TilesFramework::do_map_display()
{
    m_map_mode_enabled = true;
    m_region_tab->activate_tab(TAB_NAVIGATION);
}

int TilesFramework::draw_popup(Popup *popup)
{
    PopupRegion* reg = new PopupRegion(m_image, m_fonts[m_crt_font].font);
    // place popup region to cover screen
    reg->place(0, 0, 0);
    reg->resize_to_fit(m_windowsz.x, m_windowsz.y);

    // get menu items to draw
    int col = 0;
    while (MenuEntry *me = popup->next_entry())
    {
        col++;
        reg->set_entry(col, me->get_text(true), me->colour, me, false);
    }
    // fetch a return value
    use_control_region(reg,false);
    int retval = reg->get_retval();

    // clean up and return
    delete reg;
    return retval;
}

void TilesFramework::use_control_region(ControlRegion *reg,
                                        bool use_control_layer)
{
    LayerID new_layer = use_control_layer ? LAYER_TILE_CONTROL : m_active_layer;
    LayerID old_layer = m_active_layer;
    m_layers[new_layer].m_regions.push_back(reg);
    m_active_layer = new_layer;
    set_need_redraw();
    reg->run();
    m_layers[new_layer].m_regions.pop_back();
    m_active_layer = old_layer;
}

void TilesFramework::calculate_default_options()
{
    // Find which set of _screen_sizes to use.
    int auto_size = 0;
    int num_screen_sizes = ARRAYSZ(_screen_sizes);
    do
    {
        if (m_windowsz.x >= _screen_sizes[auto_size][0]
            && m_windowsz.y >= _screen_sizes[auto_size][1])
        {
            break;
        }
    }
    while (++auto_size < num_screen_sizes - 1);

    m_map_pixels = Options.tile_map_pixels;
    // Auto pick map and font sizes if option is zero.
#define AUTO(x,y) (x = (x) ? (x) : _screen_sizes[auto_size][(y)])
    AUTO(m_map_pixels, 2);
    AUTO(Options.tile_font_crt_size, 3);
    AUTO(Options.tile_font_stat_size, 4);
    AUTO(Options.tile_font_msg_size, 5);
    AUTO(Options.tile_font_tip_size, 6);
    AUTO(Options.tile_font_lbl_size, 7);
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
#ifndef __ANDROID__
    DATA_DIR_PATH
#endif
#endif
#ifdef TARGET_OS_MACOSX
    "dat/tiles/stone_soup_icon-512x512.png";
#else
    "dat/tiles/stone_soup_icon-32x32.png";
#endif

    string title = string(CRAWL " ") + Version::Long;

    // Do our initialization here.

    // Create an instance of UIWrapper for the library we were compiled for
    WindowManager::create();
    if (!wm)
        return false;

    // Initialize the wrapper
    if (!wm->init(&m_windowsz, &densityNum, &densityDen))
        return false;

    wm->set_window_title(title.c_str());
    wm->set_window_icon(icon_name);

    // Copy over constants that need to have been set by the wrapper
    m_screen_width = wm->screen_width();
    m_screen_height = wm->screen_height();

    GLStateManager::init();

    m_image = new ImageManager();

    // If the window size is less than the view height, the textures will
    // have to be shrunk. If this isn't the case, then don't create mipmaps,
    // as this appears to make things blurry on some users machines.
    bool need_mips = (m_windowsz.y < 32 * VIEW_MIN_HEIGHT);
    if (!m_image->load_textures(need_mips))
        return false;

    calculate_default_options();

    m_crt_font    = load_font(Options.tile_font_crt_file.c_str(),
                              Options.tile_font_crt_size, true, true);
    m_msg_font    = load_font(Options.tile_font_msg_file.c_str(),
                              Options.tile_font_msg_size, true, false);
    int stat_font = load_font(Options.tile_font_stat_file.c_str(),
                              Options.tile_font_stat_size, true, false);
    m_tip_font    = load_font(Options.tile_font_tip_file.c_str(),
                              Options.tile_font_tip_size, true, false);
    m_lbl_font    = load_font(Options.tile_font_lbl_file.c_str(),
                              Options.tile_font_lbl_size, true, true);

    if (m_crt_font == -1 || m_msg_font == -1 || stat_font == -1
        || m_tip_font == -1 || m_lbl_font == -1)
    {
        return false;
    }

    if (tiles.is_using_small_layout())
        m_init = TileRegionInit(m_image, m_fonts[m_crt_font].font, TILE_X, TILE_Y);
    else
        m_init = TileRegionInit(m_image, m_fonts[m_lbl_font].font, TILE_X, TILE_Y);
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

#ifdef TOUCH_UI
    if (tiles.is_using_small_layout())
        m_region_tab->push_tab_button(CMD_EXPLORE, TILEG_CMD_EXPLORE);
    TAB_ITEM    = m_region_tab->push_tab_region(m_region_inv, TILEG_TAB_ITEM);
    TAB_SPELL   = m_region_tab->push_tab_region(m_region_spl, TILEG_TAB_SPELL);
    TAB_ABILITY = m_region_tab->push_tab_region(m_region_abl, TILEG_TAB_ABILITY);
    m_region_tab->push_tab_region(m_region_mon, TILEG_TAB_MONSTER);
    TAB_COMMAND = m_region_tab->push_tab_region(m_region_cmd, TILEG_TAB_COMMAND);
    m_region_tab->push_tab_region(m_region_cmd_meta,
                                  TILEG_TAB_COMMAND2);
    TAB_NAVIGATION = m_region_tab->push_tab_region(m_region_cmd_map,
                                  TILEG_TAB_NAVIGATION);
    m_region_tab->activate_tab(TAB_COMMAND);
#else
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
#endif

    m_region_msg  = new MessageRegion(m_fonts[m_msg_font].font);
    m_region_stat = new StatRegion(m_fonts[stat_font].font);
    m_region_crt  = new CRTRegion(m_fonts[m_crt_font].font);

    m_region_menu = new MenuRegion(m_image, m_fonts[m_crt_font].font);

    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_tile);
    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_msg);
    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_stat);
    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_tab);

    m_layers[LAYER_CRT].m_regions.push_back(m_region_crt);
    m_layers[LAYER_CRT].m_regions.push_back(m_region_menu);

    cgotoxy(1, 1, GOTO_CRT);

    resize();

    return true;
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

    if (!font->load_font(font_file, font_size, outline, densityNum, densityDen))
    {
        delete font;
        if (default_on_fail)
            return load_font(MONOSPACED_FONT, 12, false, outline);
        else
            return -1;
    }

    font_info finfo;
    finfo.font = font;
    finfo.name = font_file;
    finfo.size = font_size;
    finfo.outline = outline;
    m_fonts.push_back(finfo);

    return m_fonts.size() - 1;
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

    if (m_region_map)
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

static unsigned int _timer_callback(unsigned int ticks, void *param)
{
    UNUSED(param);

    // force the event loop to break
    wm->raise_custom_event();

    unsigned int res = Options.tile_tooltip_ms;
    return res;
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

    unsigned int res = Options.tile_tooltip_ms;
    unsigned int timer_id = wm->set_timer(res, &_timer_callback);

    m_tooltip.clear();
    m_region_msg->alt_text().clear();

    if (need_redraw())
        redraw();

    while (!key)
    {
        if (crawl_state.seen_hups)
            return ESCAPE;

        unsigned int ticks = 0;

        if (wm->wait_event(&event))
        {
            ticks = wm->get_ticks();
            if (!mouse_target_mode && event.type != WME_CUSTOMEVENT)
            {
                tiles.clear_text_tags(TAG_CELL_DESC);
                m_region_msg->alt_text().clear();
            }

            switch (event.type)
            {
            case WME_ACTIVEEVENT:
#ifdef __ANDROID__
                // short-term: when crawl is 'iconified' in android,
                // close it
                if (/*event.active.state == 0x04 SDL_APPACTIVE &&*/ event.active.gain == 0)
                {
                    crawl_state.seen_hups++;
                    return ESCAPE;
                }
                // long-term pseudo-code:
                /*
                if (event.active.state == SDL_APPACTIVE)
                {
                    if (event.active.gain == 0)
                    {
                        if (crawl_state.need_save)
                            save_game(true);
                        do_no_SDL_or_GL_calls();
                    }
                    else
                    {
                        reload_gl_textures();
                        reset_gl_state();
                        wm->set_mod_state(MOD_NONE);
                        set_need_redraw();
                    }
                }
                 */
#else
                // When game gains focus back then set mod state clean
                // to get rid of stupid Windows/SDL bug with Alt-Tab.
                if (event.active.gain != 0)
                {
                    wm->set_mod_state(MOD_NONE);
                    set_need_redraw();
                }
#endif
                break;
            case WME_KEYDOWN:
                m_key_mod |= event.key.keysym.key_mod;
                key        = event.key.keysym.sym;
                m_region_tile->place_cursor(CURSOR_MOUSE, NO_CURSOR);

                // If you hit a key, disable tooltips until the mouse
                // is moved again.
                m_last_tick_moved = UINT_MAX;
                break;

            case WME_KEYUP:
                m_key_mod &= ~event.key.keysym.key_mod;
                m_last_tick_moved = UINT_MAX;
                break;

            case WME_KEYPRESS:
                key = event.key.keysym.sym;
                m_region_tile->place_cursor(CURSOR_MOUSE, NO_CURSOR);

                m_last_tick_moved = UINT_MAX;
                break;

            case WME_MOUSEMOTION:
                {
                    // Record mouse pos for tooltip timer
                    if (m_mouse.x != (int)event.mouse_event.px
                        || m_mouse.y != (int)event.mouse_event.py)
                    {
                        m_last_tick_moved = ticks;
                    }
                    m_mouse.x = event.mouse_event.px;
                    m_mouse.y = event.mouse_event.py;

                    event.mouse_event.held = m_buttons_held;
                    event.mouse_event.mod  = m_key_mod;
                    key = handle_mouse(event.mouse_event);

                    m_region_msg->alt_text().clear();
                    // Find the new mouse location
                    for (Region *reg : m_layers[m_active_layer].m_regions)
                    {
                        if (reg->mouse_pos(m_mouse.x, m_mouse.y,
                                           m_cur_loc.cx, m_cur_loc.cy))
                        {
                            m_cur_loc.reg = reg;
                            m_cur_loc.mode = mouse_control::current_mode();
                            reg->update_alt_text(m_region_msg->alt_text());
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
                    unsigned int count = wm->get_event_count(WME_MOUSEMOTION);
                    ASSERT(count >= 0);
                    if (count > 0)
                        continue;

                }
               break;

            case WME_MOUSEBUTTONUP:
                {
                    m_buttons_held  &= ~(event.mouse_event.button);
                    event.mouse_event.held = m_buttons_held;
                    event.mouse_event.mod  = m_key_mod;
                    key = handle_mouse(event.mouse_event);
                    m_last_tick_moved = UINT_MAX;
                }
                break;

            case WME_MOUSEBUTTONDOWN:
                {
                    m_buttons_held  |= event.mouse_event.button;
                    event.mouse_event.held = m_buttons_held;
                    event.mouse_event.mod  = m_key_mod;
                    key = handle_mouse(event.mouse_event);
                    m_last_tick_moved = UINT_MAX;
                }
                break;

            case WME_QUIT:
                crawl_state.seen_hups++;
                return ESCAPE;

            case WME_RESIZE:
                m_windowsz.x = event.resize.w;
                m_windowsz.y = event.resize.h;
                resize();
                set_need_redraw();
                return CK_REDRAW;

            case WME_CUSTOMEVENT:
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

    wm->remove_timer(timer_id);

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

/**
 * Calculates and sets the layout of the main game screen, based on the
 * available screen estate (window or screensize) and options.
 */
void TilesFramework::do_layout()
{
    // View size in pixels is (m_viewsc * crawl_view.viewsz)
    m_viewsc.x = Options.tile_cell_pixels;
    m_viewsc.y = Options.tile_cell_pixels;

    crawl_view.viewsz.x = Options.view_max_width;
    crawl_view.viewsz.y = Options.view_max_height;

    // Initial sizes.
    m_region_tile->dx = m_viewsc.x;
    m_region_tile->dy = m_viewsc.y;
    int message_y_divider = 0;

    // if the screen estate is very small, or if the option is set, choose
    // a layout that is optimal for very small screens
    bool use_small_layout = is_using_small_layout();
    bool message_overlay = Options.tile_force_overlay ? true : use_small_layout;

    if (use_small_layout)
    {
        // for now assuming that width > height:
        //   * dungeon view, on left, is full height of screen and square
        //   * message area is overlaid on dungeon view
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
        m_stat_x_divider = m_windowsz.x - (m_region_tab->ox*m_region_tab->dx/32) - get_crt_font()->char_width()*10;
        // old logic, if we're going to impinge upon a nice square dregion
        if (available_height_in_tiles * m_region_tile->dx > m_stat_x_divider)
            m_stat_x_divider = available_height_in_tiles * m_region_tile->dx;
        // always overlay message area on dungeon
        message_y_divider = m_windowsz.y;

        //printf("window x = %d; x div = %d; x button = +%d; x dreg = %d; font w = %d; y div = %d\n",m_windowsz.x,m_stat_x_divider,m_region_tab->ox,available_height_in_tiles*m_region_tile->dx,Options.tile_font_stat_size,message_y_divider);
        //printf("m_region_tab dx = %d; ox = %d; wx = %d; sx = %d\n",m_region_tab->dx,m_region_tab->ox,m_region_tab->wx,m_region_tab->sx);
    }
    else
    {
        // normal layout code

        // Locations in pixels. stat_x_divider is the dividing vertical line
        // between dungeon view on the left and status area on the right.
        // message_y_divider is the horizontal line between dungeon view on
        // the top and message window at the bottom.
        m_stat_x_divider = 0;

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

        m_stat_x_divider = available_width_in_tiles * m_region_tile->dx;

        // Calculate message_y_divider. First off, if we have already decided to
        // use the overlay, we can place the divider to the bottom of the screen.
        if (message_overlay)
            message_y_divider = m_windowsz.y;

        // Then, the optimal situation without the overlay - we can fit both
        // Options.view_max_height and at least Options.msg_min_height in the space.
        if (max(Options.view_max_height, ENV_SHOW_DIAMETER)
            * m_region_tile->dy + Options.msg_min_height
            * m_region_msg->dy
            <= m_windowsz.y && !message_overlay)
        {
            message_y_divider = max(Options.view_max_height, ENV_SHOW_DIAMETER)
                * m_region_tile->dy;
            message_y_divider = max(message_y_divider, m_windowsz.y -
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
                message_y_divider = available_height_in_tiles * m_region_tile->dy;
        }
    }

    // Resize and place the dungeon region.
    m_region_tile->resize_to_fit(m_stat_x_divider, message_y_divider);
    m_region_tile->place(0, 0, 0);

    crawl_view.viewsz.x = m_region_tile->mx;
    crawl_view.viewsz.y = m_region_tile->my;

    // Resize and place the message window.
    if (message_overlay)
    {
        m_region_msg->place(0, 0, 0); // TODO: Maybe add an option to place
                                      // overlay at the bottom.
        m_region_msg->resize_to_fit(m_stat_x_divider, Options.msg_min_height
                                    * m_region_msg->dy);
        m_region_msg->set_overlay(message_overlay);
    }
    else
    {
        m_region_msg->resize_to_fit(m_stat_x_divider, m_windowsz.y
                                    - message_y_divider);
        m_region_msg->place(0, m_region_tile->ey, 0);
    }

    crawl_view.msgsz.x = m_region_msg->mx;
    crawl_view.msgsz.y = m_region_msg->my;

    if (use_small_layout)
        m_stat_col = m_stat_x_divider;
    else
        m_stat_col = m_stat_x_divider + map_stat_margin;
    m_region_stat->resize_to_fit(m_windowsz.x - m_stat_x_divider, m_windowsz.y);
    m_region_stat->place(m_stat_col, 0, 0);
    m_region_stat->resize(m_region_stat->mx, min_stat_height);

    layout_statcol();

    m_region_crt->place(0, 0, 0);
    m_region_crt->resize_to_fit(m_windowsz.x, m_windowsz.y);

    m_region_menu->place(0, 0, 0);
    m_region_menu->resize_to_fit(m_windowsz.x, m_windowsz.y);

    crawl_view.init_view();
}

bool TilesFramework::is_using_small_layout()
{
    // automatically use small layout at low resolutions if TOUCH_UI enabled,
    // otherwise only if forced to
#ifdef TOUCH_UI
    switch (Options.tile_use_small_layout)
    {
    case MB_TRUE:
        return true;
    case MB_FALSE:
        return false;
    case MB_MAYBE:
    default:
        Options.tile_use_small_layout = (m_windowsz.x<=480) ? MB_TRUE : MB_FALSE;
        return Options.tile_use_small_layout == MB_TRUE;
    }
#else
    return Options.tile_use_small_layout == MB_TRUE;
#endif
}
void TilesFramework::zoom_dungeon(bool in)
{
    m_region_tile->zoom(in);
}

bool TilesFramework::zoom_to_minimap()
{
    // don't zoom to the minimap if it's already on the screen
    if (m_region_map || !tiles.is_using_small_layout())
        return false;

    m_region_map  = new MapRegion(m_map_pixels);
    m_region_map->dx = m_region_map->dy = min((m_windowsz.x-2*map_margin)/GXM,(m_windowsz.y-2*map_margin)/GYM);
    m_region_map->resize(GXM, GYM);
    m_region_map->place(0, 0, map_margin);
    // put the minimap at the beginning so that menus get drawn over it
    m_layers[LAYER_NORMAL].m_regions.insert(m_layers[LAYER_NORMAL].m_regions.begin(),m_region_map);

    // move the dregion out of the way
    m_region_tile->place(m_region_tile->sx,m_windowsz.y,0);

    set_need_redraw();

    // force the minimap to be redrawn properly
    //  - not sure why this is necessary :(
    clear_map();

    // force UI into map mode
//    set_map_display(true);
//    process_command(CMD_DISPLAY_MAP);
    return true;
}

bool TilesFramework::zoom_from_minimap()
{
    // don't try to zap the overlaid minimap twice
    if (!m_region_map || !tiles.is_using_small_layout())
        return false;
    delete m_region_map;
    m_region_map = nullptr;

    // remove minimap from layers again (was at top of vector)
    m_layers[LAYER_NORMAL].m_regions.erase(m_layers[LAYER_NORMAL].m_regions.begin());

    // take UI out of map mode again
//    set_map_display(false);

    // put the dregion back (not at 0,0 because we scaled it!)
    // NB. this assumes that we can work out sy again based on the ratio of wx to wy :O
    m_region_tile->place(m_region_tile->sx,m_region_tile->sx*m_region_tile->wy/m_region_tile->wx,0);

    set_need_redraw();
    return true;
}

void TilesFramework::deactivate_tab()
{
    m_region_tab->deactivate_tab();
}

void TilesFramework::autosize_minimap()
{
    const int horiz = (m_windowsz.x - m_stat_col - map_margin * 2) / GXM;
    const int vert = (m_statcol_bottom - (m_region_map->sy ? m_region_map->sy
                                                           : m_statcol_top)
                     - map_margin * 2) / GYM;
    m_region_map->dx = m_region_map->dy = min(horiz, vert);
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
    if (lines == 0)
        return;

    int prev_size = m_region_tab->wy;

    m_region_tab->resize(m_region_tab->mx, min_inv_height + lines);
    m_region_tab->place(m_stat_col, m_windowsz.y - m_region_tab->wy);

    int delta_y = m_region_tab->wy - prev_size;
    for (tab_iterator it = m_tabs.begin(); it != m_tabs.end(); ++it)
        it->second->place(it->second->sx, it->second->sy - delta_y);

    m_statcol_bottom -= delta_y;
}

void TilesFramework::place_gold_turns()
{
    if (m_statcol_bottom - m_statcol_top < m_region_stat->dy)
        return;

    ++crawl_view.hudsz.y;
    m_region_stat->resize(m_region_stat->mx, crawl_view.hudsz.y);
    if (m_region_map)
        m_region_map->place(m_region_stat->sx, m_region_stat->ey);

    m_statcol_top += m_region_stat->dy;
}

void TilesFramework::layout_statcol()
{
    bool use_small_layout = is_using_small_layout();

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

        // place tabs (covering whole screen)
        m_region_tab->set_small_layout(true, m_windowsz);
        m_region_tab->resize_to_fit(m_windowsz.x, m_windowsz.y);

        // place tabs waay to the right (all offsets will be negative)
        m_region_tab->place(m_windowsz.x-m_region_tab->ox, 0);

        // close any open tab
        m_region_tab->deactivate_tab();

        // resize stats to be up to beginning of command tabs
        //  ... this works because the margin (ox) on m_region_tab contains the tabs themselves
        m_region_stat->resize_to_fit((m_windowsz.x - m_stat_x_divider) - m_region_tab->ox*m_region_tab->dx/32, m_statcol_bottom-m_statcol_top);
        crawl_view.hudsz.y = m_region_stat->my;
        crawl_view.hudsz.x = m_region_stat->mx;
    }
    else
    {
        crawl_view.hudsz.y = min_stat_height;
        m_region_stat->resize(m_region_stat->mx, crawl_view.hudsz.y);

        m_statcol_top = m_region_stat->ey;

        // Set the inventory region to minimal size.
        m_region_tab->set_small_layout(false, m_windowsz);
        m_region_tab->place(m_stat_col, m_statcol_top);
        m_region_tab->resize_to_fit(m_windowsz.x - m_region_tab->sx,
                                    m_windowsz.y - m_region_tab->sy);
        m_region_tab->resize(m_region_tab->mx, min_inv_height);
        m_region_tab->place(m_stat_col, m_windowsz.y - m_region_tab->wy);

        m_statcol_bottom = m_region_tab->sy - m_tab_margin;

#if TAG_MAJOR_VERSION == 34
        // Lava orc temperature bar
        if (you.species == SP_LAVA_ORC)
            ++crawl_view.hudsz.y;
#endif
        m_region_stat->resize(m_region_stat->mx, crawl_view.hudsz.y);
        m_statcol_top += m_region_stat->dy;

        for (const string &str : Options.tile_layout_priority)
        {
            if (str == "inventory")
                resize_inventory();
            else if (str == "minimap" || str == "map")
                place_minimap();
            else if (str == "gold_turn" || str == "gold_turns")
                place_gold_turns();
            else
                place_tab(m_region_tab->find_tab(str));
        }
        // We stretch the minimap so it is centered in the space left.
        if (m_region_map)
        {
            if (!Options.tile_map_pixels)
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
        die("invalid cgotoxy region in tiles: %d", region);
        break;
    }

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

// #define DEBUG_TILES_REDRAW
void TilesFramework::redraw()
{
#ifdef DEBUG_TILES_REDRAW
    cprintf("\nredrawing tiles");
#endif
    m_need_redraw = false;

    glmanager->reset_view_for_redraw(m_viewsc.x, m_viewsc.y);

    for (Region *region : m_layers[m_active_layer].m_regions)
        region->render();

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

#ifdef __ANDROID__
    glmanager->fixup_gl_state();
#endif

    m_last_tick_redraw = wm->get_ticks();
}

void TilesFramework::update_minimap(const coord_def& gc)
{
    if (!m_region_map)
        return;

    map_feature mf;

    if (!crawl_state.game_is_arena() && gc == you.pos() && you.on_current_level)
        mf = MF_PLAYER;
    else
        mf = get_cell_map_feature(env.map_knowledge(gc));

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
    if (Options.tile_show_items.empty() || crawl_state.game_is_arena())
        return;

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
    m_region_tile->place_cursor(type, gc);
}

void TilesFramework::clear_text_tags(text_tag_type type)
{
    m_region_tile->clear_text_tags(type);
}

void TilesFramework::add_text_tag(text_tag_type type, const string &tag,
                                  const coord_def &gc)
{
    m_region_tile->add_text_tag(type, tag, gc);
}

void TilesFramework::add_text_tag(text_tag_type type, const monster_info& mon)
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
        // Beautification hack.  "Foo's ghost" is a little bit
        // verbose as a tag.  "Foo" on its own should be sufficient.
        tiles.add_text_tag(TAG_NAMED_MONSTER, mon.mname, gc);
    }
    else
        tiles.add_text_tag(TAG_NAMED_MONSTER, mon.proper_name(DESC_PLAIN), gc);
}

const coord_def &TilesFramework::get_cursor() const
{
    return m_region_tile->get_cursor();
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

int TilesFramework::to_lines(int num_tiles, int tile_height)
{
    return num_tiles * tile_height / get_crt_font()->char_height();
}
#endif
