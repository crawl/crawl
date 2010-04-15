#include "AppHdr.h"

#ifdef USE_TILE

#include "artefact.h"
#include "cio.h"
#include "coord.h"
#include "directn.h"
#include "env.h"
#include "food.h"
#include "itemname.h"
#include "itemprop.h"
#include "files.h"
#include "libutil.h"
#include "macro.h"
#include "map_knowledge.h"
#include "message.h"
#include "mon-util.h"
#include "options.h"
#include "player.h"
#include "spl-book.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "tiles.h"
#include "tilesdl.h"
#include "travel.h"
#include "view.h"
#include "viewgeom.h"

#include "tiledef-dngn.h"
#include "tiledef-main.h"
#include "tiledef-gui.h"
#include "tilefont.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>

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
    m_context(NULL),
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
    delete m_region_inv;
    delete m_region_spl;
    delete m_region_mem;
    delete m_region_crt;
    delete m_region_menu;

    m_region_tile  = NULL;
    m_region_stat  = NULL;
    m_region_msg   = NULL;
    m_region_map   = NULL;
    m_region_tab   = NULL;
    m_region_inv   = NULL;
    m_region_spl   = NULL;
    m_region_mem   = NULL;
    m_region_crt = NULL;
    m_region_menu  = NULL;

    for (unsigned int i = 0; i < LAYER_MAX; i++)
        m_layers[i].m_regions.clear();

    for (unsigned int i = 0; i < m_fonts.size(); i++)
    {
        delete m_fonts[i].font;
        m_fonts[i].font = NULL;
    }

    SDL_Quit();

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
    DollEditRegion* reg = new DollEditRegion(&m_image,
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
#ifdef TARGET_OS_WINDOWS
    putenv("SDL_VIDEO_WINDOW_POS=center");
    putenv("SDL_VIDEO_CENTERED=1");
#endif

    _init_consoles();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        printf("Failed to initialise SDL: %s\n", SDL_GetError());
        return (false);
    }

    {
        const SDL_VideoInfo* video_info = SDL_GetVideoInfo();
        m_screen_width  = video_info->current_w;
        m_screen_height = video_info->current_h;
    }

    SDL_EnableUNICODE(true);

    std::string title = CRAWL " " + Version::Long();
    SDL_WM_SetCaption(title.c_str(), CRAWL);
    const char *icon_name =
#ifdef DATA_DIR_PATH
            DATA_DIR_PATH
#endif
#ifdef TARGET_OS_WINDOWS
            "dat/tiles/stone_soup_icon-win32.png";
#else
            "dat/tiles/stone_soup_icon-32x32.png";
#endif

    SDL_Surface *icon = IMG_Load(datafile_path(icon_name, true, true).c_str());
    if (!icon)
    {
        printf("Failed to load icon: %s\n", SDL_GetError());
        return (false);
    }
    SDL_WM_SetIcon(icon, NULL);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    if (Options.tile_key_repeat_delay > 0)
    {
        const int delay    = Options.tile_key_repeat_delay;
        const int interval = SDL_DEFAULT_REPEAT_INTERVAL;
        if (SDL_EnableKeyRepeat(delay, interval) != 0)
            printf("Failed to set key repeat mode: %s\n", SDL_GetError());
    }

    unsigned int flags = SDL_OPENGL;

    bool too_small = (m_screen_width < 1024 || m_screen_height < 800);
    if (Options.tile_full_screen == SCREENMODE_FULL
        || too_small && Options.tile_full_screen == SCREENMODE_AUTO)
    {
        flags |= SDL_FULLSCREEN;
        m_fullscreen = true;
    }

    if (Options.tile_window_width && Options.tile_window_height)
    {
        m_windowsz.x = Options.tile_window_width;
        m_windowsz.y = Options.tile_window_height;
    }
    else if (flags & SDL_FULLSCREEN)
    {
        // By default, fill the whole screen.
        m_windowsz.x = m_screen_width;
        m_windowsz.y = m_screen_height;
    }
    else
    {
        m_windowsz.x = std::max(800, m_screen_width  - 90);
        m_windowsz.y = std::max(480, m_screen_height - 90);
    }

    m_context = SDL_SetVideoMode(m_windowsz.x, m_windowsz.y, 0, flags);
    if (!m_context)
    {
        printf("Failed to set video mode: %s\n", SDL_GetError());
        return (false);
    }

    // If the window size is less than the view height, the textures will
    // have to be shrunk.  If this isn't the case, then don't create mipmaps,
    // as this appears to make things blurry on some users machines.
    bool need_mips = (m_windowsz.y < 32 * VIEW_MIN_HEIGHT);
    if (!m_image.load_textures(need_mips))
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

    m_region_tile = new DungeonRegion(&m_image, m_fonts[lbl_font].font,
                                      TILE_X, TILE_Y);
    m_region_map  = new MapRegion(Options.tile_map_pixels);
    m_region_tab  = new TabbedRegion(&m_image, m_fonts[lbl_font].font,
                                     TILE_X, TILE_Y);
    m_region_inv  = new InventoryRegion(&m_image, m_fonts[lbl_font].font,
                                        TILE_X, TILE_Y);
    m_region_spl  = new SpellRegion(&m_image, m_fonts[lbl_font].font,
                                    TILE_X, TILE_Y);
    m_region_mem  = new MemoriseRegion(&m_image, m_fonts[lbl_font].font,
                                       TILE_X, TILE_Y);

    m_region_tab->set_tab_region(TAB_ITEM, m_region_inv, TILEG_TAB_ITEM);
    m_region_tab->set_tab_region(TAB_SPELL, m_region_spl, TILEG_TAB_SPELL);
    m_region_tab->set_tab_region(TAB_MEMORISE, m_region_mem, TILEG_TAB_MEMORISE);

    switch (Options.tile_display)
    {
    default:
    case TDSP_INVENT:
        m_region_tab->activate_tab(TAB_ITEM);
        break;
    case TDSP_SPELLS:
        m_region_tab->activate_tab(TAB_SPELL);
        break;
    case TDSP_MEMORISE:
        m_region_tab->activate_tab(TAB_MEMORISE);
        break;
    }

    m_region_msg  = new MessageRegion(m_fonts[m_msg_font].font);
    m_region_stat = new StatRegion(m_fonts[stat_font].font);
    m_region_crt  = new CRTRegion(m_fonts[m_crt_font].font);

    m_region_menu = new MenuRegion(&m_image, m_fonts[m_crt_font].font);

    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_map);
    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_tile);
    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_tab);
    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_msg);
    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_stat);

    m_layers[LAYER_CRT].m_regions.push_back(m_region_crt);
    m_layers[LAYER_CRT].m_regions.push_back(m_region_menu);

    cgotoxy(1, 1, GOTO_CRT);

    GLStateManager::init();

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

    FTFont *font = new FTFont();

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

void TilesFramework::load_dungeon(unsigned int *tileb, const coord_def &gc)
{
    m_active_layer = LAYER_NORMAL;

    unsigned int ox = m_region_tile->mx/2;
    unsigned int oy = m_region_tile->my/2;
    m_region_tile->load_dungeon(tileb, gc.x - ox, gc.y - oy);

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
    viewwindow(false, false);
    tiles.place_cursor(CURSOR_MAP, cen);
}

void TilesFramework::resize()
{
    calculate_default_options();
    do_layout();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // For ease, vertex positions are pixel positions.
    glOrtho(0, m_windowsz.x, m_windowsz.y, 0, -1000, 1000);
}

static unsigned char _get_modifiers(SDL_keysym &keysym)
{
    // keysym.mod can't be used to keep track of the modifier state.
    // If shift is hit by itself, this will not include KMOD_SHIFT.
    // Instead, look for the key itself as a separate event.
    switch (keysym.sym)
    {
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
        return MOD_SHIFT;
    case SDLK_LCTRL:
    case SDLK_RCTRL:
        return MOD_CTRL;
    case SDLK_LALT:
    case SDLK_RALT:
        return MOD_ALT;
    default:
        return 0;
    }
}

static int _translate_keysym(SDL_keysym &keysym)
{
    // This function returns the key that was hit.  Returning zero implies that
    // the keypress (e.g. hitting shift on its own) should be eaten and not
    // handled.

    const int shift_offset = CK_SHIFT_UP - CK_UP;
    const int ctrl_offset  = CK_CTRL_UP - CK_UP;

    int mod = 0;
    if (keysym.mod & KMOD_SHIFT)
        mod |= MOD_SHIFT;
    if (keysym.mod & KMOD_CTRL)
        mod |= MOD_CTRL;
    if (keysym.mod & KMOD_LALT)
        mod |= MOD_ALT;

    // This is arbitrary, but here's the current mappings.
    // 0-256: ASCII, Crawl arrow keys
    // 0-1k : Other SDL keys (F1, Windows keys, etc...) and modifiers
    // 1k-3k: Non-ASCII with modifiers other than just shift or just ctrl.
    // 3k+  : ASCII with the left alt modifier.

    int offset = mod ? 1000 + 256 * mod : 0;
    int numpad_offset = 0;
    if (mod == MOD_CTRL)
        numpad_offset = ctrl_offset;
    else if (mod == MOD_SHIFT)
        numpad_offset = shift_offset;
    else
        numpad_offset = offset;

    switch (keysym.sym)
    {
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        return CK_ENTER + offset;
    case SDLK_BACKSPACE:
        return CK_BKSP + offset;
    case SDLK_ESCAPE:
        return CK_ESCAPE + offset;
    case SDLK_DELETE:
        return CK_DELETE + offset;

    case SDLK_NUMLOCK:
    case SDLK_CAPSLOCK:
    case SDLK_SCROLLOCK:
    case SDLK_RMETA:
    case SDLK_LMETA:
    case SDLK_LSUPER:
    case SDLK_RSUPER:
    case SDLK_MODE:
    case SDLK_COMPOSE:
        // Don't handle these.
        return 0;

    case SDLK_F1:
    case SDLK_F2:
    case SDLK_F3:
    case SDLK_F4:
    case SDLK_F5:
    case SDLK_F6:
    case SDLK_F7:
    case SDLK_F8:
    case SDLK_F9:
    case SDLK_F10:
    case SDLK_F11:
    case SDLK_F12:
    case SDLK_F13:
    case SDLK_F14:
    case SDLK_F15:
    case SDLK_HELP:
    case SDLK_PRINT:
    case SDLK_SYSREQ:
    case SDLK_BREAK:
    case SDLK_MENU:
    case SDLK_POWER:
    case SDLK_EURO:
    case SDLK_UNDO:
        ASSERT(keysym.sym >= SDLK_F1 && keysym.sym <= SDLK_UNDO);
        return keysym.sym + (SDLK_UNDO - SDLK_F1 + 1) * mod;

    // Hack.  libw32c overloads clear with '5' too.
    case SDLK_KP5:
        return CK_CLEAR + numpad_offset;

    case SDLK_KP8:
    case SDLK_UP:
        return CK_UP + numpad_offset;
    case SDLK_KP2:
    case SDLK_DOWN:
        return CK_DOWN + numpad_offset;
    case SDLK_KP4:
    case SDLK_LEFT:
        return CK_LEFT + numpad_offset;
    case SDLK_KP6:
    case SDLK_RIGHT:
        return CK_RIGHT + numpad_offset;
    case SDLK_KP0:
    case SDLK_INSERT:
        return CK_INSERT + numpad_offset;
    case SDLK_KP7:
    case SDLK_HOME:
        return CK_HOME + numpad_offset;
    case SDLK_KP1:
    case SDLK_END:
        return CK_END + numpad_offset;
    case SDLK_CLEAR:
        return CK_CLEAR + numpad_offset;
    case SDLK_KP9:
    case SDLK_PAGEUP:
        return CK_PGUP + numpad_offset;
    case SDLK_KP3:
    case SDLK_PAGEDOWN:
        return CK_PGDN + numpad_offset;
    default:
        break;
    }

    // Alt does not get baked into keycodes like shift and ctrl, so handle it.
    const int key_offset = (mod & MOD_ALT) ? 3000 : 0;

    const bool is_ascii = ((keysym.unicode & 0xFF80) == 0);
    return (is_ascii ? (keysym.unicode & 0x7F) + key_offset : 0);
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

static void _translate_event(const SDL_MouseMotionEvent &sdl_event,
                             MouseEvent &tile_event)
{
    tile_event.held   = MouseEvent::NONE;
    tile_event.event  = MouseEvent::MOVE;
    tile_event.button = MouseEvent::NONE;
    tile_event.px     = sdl_event.x;
    tile_event.py     = sdl_event.y;

    // TODO enne - do we want the relative motion?
}

static void _translate_event(const SDL_MouseButtonEvent &sdl_event,
                             MouseEvent &tile_event)
{
    tile_event.held  = MouseEvent::NONE;
    tile_event.event = (sdl_event.type == SDL_MOUSEBUTTONDOWN) ?
                            MouseEvent::PRESS : MouseEvent::RELEASE;
    switch (sdl_event.button)
    {
    case SDL_BUTTON_LEFT:
        tile_event.button = MouseEvent::LEFT;
        break;
    case SDL_BUTTON_RIGHT:
        tile_event.button = MouseEvent::RIGHT;
        break;
    case SDL_BUTTON_MIDDLE:
        tile_event.button = MouseEvent::MIDDLE;
        break;
    case SDL_BUTTON_WHEELUP:
        tile_event.button = MouseEvent::SCROLL_UP;
        break;
    case SDL_BUTTON_WHEELDOWN:
        tile_event.button = MouseEvent::SCROLL_DOWN;
        break;
    default:
        // Unhandled button.
        tile_event.button = MouseEvent::NONE;
        break;
    }
    tile_event.px = sdl_event.x;
    tile_event.py = sdl_event.y;
}

static unsigned int _timer_callback(unsigned int ticks)
{
    // force the event loop to break
    SDL_Event event;
    event.type = SDL_USEREVENT;
    SDL_PushEvent(&event);

    unsigned int res = Options.tile_tooltip_ms;
    return (res);
}

int TilesFramework::getch_ck()
{
    SDL_Event event;
    cursor_loc tip_loc;
    cursor_loc last_loc;

    int key = 0;

    // Don't update tool tips etc. in targeting mode.
    const bool mouse_target_mode
                = (mouse_control::current_mode() == MOUSE_MODE_TARGET_PATH
                   || mouse_control::current_mode() == MOUSE_MODE_TARGET_DIR);

    const unsigned int ticks_per_screen_redraw = Options.tile_update_rate;

    unsigned int res = Options.tile_tooltip_ms;
    SDL_SetTimer(res, &_timer_callback);

    m_tooltip.clear();
    m_region_msg->alt_text().clear();

    if (need_redraw())
        redraw();

    while (!key)
    {
        unsigned int ticks = 0;
        last_loc = m_cur_loc;

        if (SDL_WaitEvent(&event))
        {
            ticks = SDL_GetTicks();
            if (!mouse_target_mode)
            {
                if (event.type != SDL_USEREVENT)
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
                            set_need_redraw();
                            break;
                        }
                    }
                }
            }

            switch (event.type)
            {
            case SDL_ACTIVEEVENT:
                // When game gains focus back then set mod state clean
                // to get rid of stupid Windows/SDL bug with Alt-Tab.
                if (event.active.gain != 0)
                {
                    SDL_SetModState(KMOD_NONE);
                    set_need_redraw();
                }
                break;
            case SDL_KEYDOWN:
                m_key_mod |= _get_modifiers(event.key.keysym);
                key        = _translate_keysym(event.key.keysym);
                m_region_tile->place_cursor(CURSOR_MOUSE, Region::NO_CURSOR);

                // If you hit a key, disable tooltips until the mouse
                // is moved again.
                m_last_tick_moved = ~0;
                break;

            case SDL_KEYUP:
                m_key_mod &= ~_get_modifiers(event.key.keysym);
                m_last_tick_moved = ~0;
                break;

            case SDL_MOUSEMOTION:
                {
                    // Record mouse pos for tooltip timer
                    if (m_mouse.x != event.motion.x
                        || m_mouse.y != event.motion.y)
                    {
                        m_last_tick_moved = ticks;
                    }
                    m_mouse.x = event.motion.x;
                    m_mouse.y = event.motion.y;

                    MouseEvent mouse_event;
                    _translate_event(event.motion, mouse_event);
                    mouse_event.held = m_buttons_held;
                    mouse_event.mod  = m_key_mod;
                    int mouse_key = handle_mouse(mouse_event);

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
                    SDL_Event store;
                    int count = SDL_PeepEvents(&store, 1, SDL_PEEKEVENT,
                                               SDL_EVENTMASK(SDL_MOUSEMOTION));
                    ASSERT(count >= 0);
                    if (count > 0)
                        continue;

                    // Stay within this input loop until the mouse moves
                    // to a semantically different location.  Crawl doesn't
                    // care about small mouse movements.
                    if (last_loc == m_cur_loc)
                        continue;

                    key = mouse_key;
               }
               break;

            case SDL_MOUSEBUTTONUP:
                {
                    MouseEvent mouse_event;
                    _translate_event(event.button, mouse_event);
                    m_buttons_held  &= ~(mouse_event.button);
                    mouse_event.held = m_buttons_held;
                    mouse_event.mod  = m_key_mod;
                    key = handle_mouse(mouse_event);
                    m_last_tick_moved = ticks;
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                {
                    MouseEvent mouse_event;
                    _translate_event(event.button, mouse_event);
                    m_buttons_held  |= mouse_event.button;
                    mouse_event.held = m_buttons_held;
                    mouse_event.mod  = m_key_mod;
                    key = handle_mouse(mouse_event);
                    m_last_tick_moved = ticks;
                }
                break;

            case SDL_QUIT:
                if (crawl_state.need_save)
                    save_game(true);
                exit(0);
                break;

            case SDL_USEREVENT:
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

    SDL_SetTimer(0, NULL);

    return key;
}

static const int map_margin      = 2;
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

    // Calculating stat_x_divider.
    // First, the optimal situation: we have screen estate to satisfy
    // Options.view_max_width, and room for status area with the specified
    // stat font size.
    if (std::max(Options.view_max_width, ENV_SHOW_DIAMETER) * m_region_tile->dx
        + stat_width * m_region_stat->dx <= m_windowsz.x)
    {
        stat_x_divider = (std::max(Options.view_max_width, ENV_SHOW_DIAMETER))
                          * m_region_tile->dx;
    }
    // If we don't have room for Options.view_max_width, use the maximum space
    // available.
    else
    {
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
    }

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
        //TODO: respect Options.msg_max_height
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
    m_region_map->place(m_region_stat->sx, m_region_stat->ey, map_margin);

    int inv_col = std::max(m_region_tile->ex, m_region_msg->ex);
    if (message_overlay)
        inv_col = m_region_stat->sx;

    m_region_tab->place(inv_col, m_region_map->ey);
    m_region_tab->resize_to_fit(m_windowsz.x - m_region_tab->sx,
                                m_windowsz.y - m_region_tab->sy);
    m_region_tab->resize(std::min(13, (int)m_region_tab->mx),
                         std::min( 6, (int)m_region_tab->my));

    int self_inv_y = m_windowsz.y - m_region_tab->wy;
    m_region_tab->place(inv_col, self_inv_y);

    // recenter map above inventory
    int map_cen_x = (m_region_tab->sx + m_region_tab->ex) / 2;
    map_cen_x = std::min(map_cen_x, (int)(m_windowsz.x - m_region_map->wx/2));
    m_region_map->place(map_cen_x - m_region_map->wx/2, m_region_map->sy,
                        map_margin);

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

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glScalef(m_viewsc.x, m_viewsc.y, 1.0f);

    for (unsigned int i = 0; i < m_layers[m_active_layer].m_regions.size(); ++i)
        m_layers[m_active_layer].m_regions[i]->render();

    // Draw tooltip
    if (Options.tile_tooltip_ms > 0 && !m_tooltip.empty())
    {
        const coord_def min_pos(0, 0);
        FTFont *font = m_fonts[m_tip_font].font;

        font->render_string(m_mouse.x, m_mouse.y - 2, m_tooltip.c_str(),
                            min_pos, m_windowsz, WHITE, false, 220, BLUE, 5,
                            true);
    }

    SDL_GL_SwapBuffers();

    m_last_tick_redraw = SDL_GetTicks();
}

void TilesFramework::update_minimap(int gx, int gy)
{
    if (!player_in_mappable_area())
        return;

    show_type object = env.map_knowledge[gx][gy].object;
    map_feature f = (object.cls == SH_MONSTER) ? MF_MONS_HOSTILE :
        get_feature_def(object).minimap;

    if (f == MF_SKIP)
        f = get_feature_def(grd[gx][gy]).minimap;
    ASSERT(f < MF_MAX);

    tiles.update_minimap(gx, gy, f);
}

void TilesFramework::update_minimap(int gx, int gy, map_feature f)
{
    if (!player_in_mappable_area())
        return;

    coord_def gc(gx, gy);

    if (!crawl_state.game_is_arena() && gc == you.pos() && you.on_current_level)
        f = MF_PLAYER;
    else if (monster_at(gc) && f == MF_MONS_HOSTILE)
    {
        const monsters *mon = monster_at(gc);
        if (mon->friendly())
            f = MF_MONS_FRIENDLY;
        else if (mon->good_neutral())
            f = MF_MONS_PEACEFUL;
        else if (mon->neutral())
            f = MF_MONS_NEUTRAL;
        else if (mons_class_flag(mon->type, M_NO_EXP_GAIN))
            f = MF_MONS_NO_EXP;
    }
    else if (f == MF_FLOOR || f == MF_MAP_FLOOR || f == MF_WATER)
    {
        if (is_exclude_root(gc))
            f = MF_EXCL_ROOT;
        else if (is_excluded(gc))
            f = MF_EXCL;
    }

    if (f == MF_WALL || f == MF_FLOOR)
    {
        if (is_terrain_known(gx, gy) && !is_terrain_seen(gx, gy)
            || is_map_knowledge_detected_item(gx, gy)
            || is_map_knowledge_detected_mons(gx, gy))
        {
            f = (f == MF_WALL) ? MF_MAP_WALL : MF_MAP_FLOOR;
        }
    }

    m_region_map->set(gx, gy, f);
}

void TilesFramework::clear_minimap()
{
    m_region_map->clear();
}

void TilesFramework::update_minimap_bounds()
{
    m_region_map->update_bounds();
}

int tile_known_brand(const item_def item)
{
    if (!item_type_known(item))
        return 0;

    if (item.base_type == OBJ_WEAPONS)
    {
        const int brand = get_weapon_brand(item);
        if (brand != SPWPN_NORMAL)
            return (TILE_BRAND_FLAMING + get_weapon_brand(item) - 1);
    }
    else if (item.base_type == OBJ_ARMOUR)
    {
        const int brand = get_armour_ego_type(item);
        if (brand != SPARM_NORMAL)
            return (TILE_BRAND_ARM_RUNNING + get_armour_ego_type(item) - 1);
        else if (is_artefact(item)
                 && artefact_wpn_property(item, ARTP_PONDEROUS))
        {
            return (TILE_BRAND_ARM_PONDEROUSNESS);
        }
    }
    else if (item.base_type == OBJ_MISSILES)
    {
        switch (get_ammo_brand(item))
        {
        case SPMSL_FLAME:
            return TILE_BRAND_FLAME;
        case SPMSL_FROST:
            return TILE_BRAND_FROST;
        case SPMSL_POISONED:
            return TILE_BRAND_POISONED;
        case SPMSL_CURARE:
            return TILE_BRAND_CURARE;
        case SPMSL_RETURNING:
            return TILE_BRAND_RETURNING;
        case SPMSL_CHAOS:
            return TILE_BRAND_CHAOS;
        case SPMSL_PENETRATION:
            return TILE_BRAND_PENETRATION;
        case SPMSL_REAPING:
            return TILE_BRAND_REAPING;
        case SPMSL_DISPERSAL:
            return TILE_BRAND_DISPERSAL;
        case SPMSL_EXPLODING:
            return TILE_BRAND_EXPLOSION;
        case SPMSL_CONFUSION:
            return TILE_BRAND_CONFUSION;
        case SPMSL_PARALYSIS:
            return TILE_BRAND_PARALYSIS;
        case SPMSL_SLOW:
            return TILE_BRAND_SLOWING;
        case SPMSL_SICKNESS:
            return TILE_BRAND_SICKNESS;
        case SPMSL_RAGE:
            return TILE_BRAND_RAGE;
        case SPMSL_SLEEP:
            return TILE_BRAND_SLEEP;
        default:
            break;
        }
    }
    return 0;
}

int tile_corpse_brand(const item_def item)
{
    if (item.base_type != OBJ_CORPSES || item.sub_type != CORPSE_BODY)
        return (0);

    const bool fulsome_dist = you.has_spell(SPELL_FULSOME_DISTILLATION);
    const bool rotten       = food_is_rotten(item);
    const bool saprovorous  = player_mutation_level(MUT_SAPROVOROUS);

    // Brands are mostly meaningless to herbivores.
    // Could still be interesting for Fulsome Distillation, though.
    if (fulsome_dist && player_mutation_level(MUT_HERBIVOROUS) == 3)
        return (0);

    // Vampires are only interested in fresh blood.
    if (you.species == SP_VAMPIRE
        && (rotten || !mons_has_blood(item.plus)))
    {
        return TILE_FOOD_INEDIBLE;
    }

    // Rotten corpses' chunk effects are meaningless if we are neither
    // saprovorous nor have the Fulsome Distillation spell.
    if (rotten && !saprovorous && !fulsome_dist)
        return TILE_FOOD_INEDIBLE;

    // Harmful chunk effects > religious rules > chance of sickness.
    if (is_poisonous(item))
        return TILE_FOOD_POISONED;

    if (is_mutagenic(item))
        return TILE_FOOD_MUTAGENIC;

    if (causes_rot(item))
        return TILE_FOOD_ROTTING;

    if (is_forbidden_food(item))
        return TILE_FOOD_FORBIDDEN;

    if (is_contaminated(item))
        return TILE_FOOD_CONTAMINATED;

    // If no special chunk effects, mark corpse as inedible
    // unless saprovorous.
    if (rotten && !saprovorous)
        return TILE_FOOD_INEDIBLE;

    return 0;
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

void TilesFramework::add_text_tag(text_tag_type type, const monsters* mon)
{
    // HACK.  Names cover up pan demons in a weird way.
    if (mon->type == MONS_PANDEMONIUM_DEMON)
        return;

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

bool TilesFramework::initialise_items()
{
    return (m_image.load_item_texture());
}

const coord_def &TilesFramework::get_cursor() const
{
    return (m_region_tile->get_cursor());
}

void TilesFramework::add_overlay(const coord_def &gc, int idx)
{
    m_region_tile->add_overlay(gc, idx);
}

void TilesFramework::clear_overlays()
{
    m_region_tile->clear_overlays();
}

void TilesFramework::set_need_redraw(unsigned int min_tick_delay)
{
    unsigned int ticks = (SDL_GetTicks() - m_last_tick_redraw);
    if (min_tick_delay && ticks <= min_tick_delay)
        return;

    m_need_redraw = true;
}

bool TilesFramework::need_redraw() const
{
    return m_need_redraw;
}

#endif
