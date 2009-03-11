#include "AppHdr.h"
REVISION("$Rev$");

#include "cio.h"
#include "itemname.h"
#include "items.h"
#include "itemprop.h"
#include "files.h"
#include "message.h"
#include "mon-util.h"
#include "player.h"
#include "randart.h"
#include "state.h"
#include "stuff.h"
#include "tiles.h"
#include "tilesdl.h"
#include "travel.h"
#include "version.h"
#include "view.h"

#include "tiledef-dngn.h"
#include "tilefont.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>

// Default Screen Settings
// width, height, map, crt, stat, msg, tip, lbl
static int _screen_sizes[4][8] =
{
    // Default
    {1024, 700, 4, 15, 16, 14, 15, 14},
    // Eee PC 900+
    {1024, 600, 3, 14, 14, 12, 13, 12},
    // Small screen
    {800, 600, 3, 14, 11, 12, 13, 12},
    // Eee PC
    {800, 480, 3, 13, 12, 10, 13, 11}
};

// Note: these defaults should match the OpenGL defaults
GLState::GLState() :
    array_vertex(false),
    array_texcoord(false),
    array_colour(false),
    blend(false),
    texture(false)
{
}

void GLStateManager::init()
{
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0, 0.0, 0.0, 1.0f);
}

void GLStateManager::set(const GLState& state)
{
    if (state.array_vertex)
        glEnableClientState(GL_VERTEX_ARRAY);
    else
        glDisableClientState(GL_VERTEX_ARRAY);

    if (state.array_texcoord)
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    else
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    if (state.array_colour)
    {
        glEnableClientState(GL_COLOR_ARRAY);
    }
    else
    {
        glDisableClientState(GL_COLOR_ARRAY);

        // [enne] This should *not* be necessary, but the Linux OpenGL
        // driver that I'm using sets this to the last colour of the
        // colour array.  So, we need to unset it here.
        glColor3f(1.0f, 1.0f, 1.0f);
    }

    if (state.texture)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);

    if (state.blend)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}

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
    m_last_tick_moved(0)
{
}

TilesFramework::~TilesFramework()
{
}

static void _init_consoles()
{
#ifdef WIN32TILES
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
#ifdef WIN32TILES
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
    delete m_region_inv;
    delete m_region_crt;
    delete m_region_menu;

    m_region_tile = NULL;
    m_region_stat = NULL;
    m_region_msg = NULL;
    m_region_map = NULL;
    m_region_inv = NULL;
    m_region_crt = NULL;
    m_region_menu = NULL;

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
    } while (++auto_size < num_screen_sizes - 1);

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
#ifdef WIN32TILES
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
        m_screen_width = video_info->current_w;
        m_screen_height = video_info->current_h;
    }

    SDL_EnableUNICODE(true);

    SDL_WM_SetCaption(CRAWL " " VERSION, CRAWL);
#ifdef WIN32TILES
    const char *icon_name = "dat/tiles/stone_soup_icon-win32.png";
#else
    const char *icon_name = "dat/tiles/stone_soup_icon-32x32.png";
#endif
    SDL_Surface *icon = IMG_Load(icon_name);
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

    if (Options.tile_key_repeat)
    {
        int delay = SDL_DEFAULT_REPEAT_DELAY;
        int interval = SDL_DEFAULT_REPEAT_INTERVAL;
        if (SDL_EnableKeyRepeat(delay, interval) != 0)
            printf("Failed to set key repeat mode: %s\n", SDL_GetError());
    }

    unsigned int flags = SDL_OPENGL;

    bool too_small = (m_screen_width < 1024 || m_screen_height < 800);
    if (Options.tile_full_screen == SCREENMODE_FULL
        || too_small && Options.tile_full_screen == SCREENMODE_AUTO)
    {
        flags |= SDL_FULLSCREEN;
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
        m_windowsz.x = std::max(800, m_screen_width - 100);
        m_windowsz.y = std::max(480, m_screen_height - 100);
    }

    m_context = SDL_SetVideoMode(m_windowsz.x, m_windowsz.y, 0, flags);
    if (!m_context)
    {
        printf("Failed to set video mode: %s\n", SDL_GetError());
        return (false);
    }

    if (!m_image.load_textures())
        return (false);

    calculate_default_options();

    int crt_font  = load_font(Options.tile_font_crt_file.c_str(),
                              Options.tile_font_crt_size, true, true);
    int msg_font  = load_font(Options.tile_font_msg_file.c_str(),
                              Options.tile_font_msg_size, true, false);
    int stat_font = load_font(Options.tile_font_stat_file.c_str(),
                              Options.tile_font_stat_size, true, false);
    m_tip_font    = load_font(Options.tile_font_tip_file.c_str(),
                              Options.tile_font_tip_size, true, false);
    int lbl_font  = load_font(Options.tile_font_lbl_file.c_str(),
                              Options.tile_font_lbl_size, true, true);

    if (crt_font == -1 || msg_font == -1 || stat_font == -1
        || m_tip_font == -1 || lbl_font == -1)
    {
        return (false);
    }

    m_region_tile = new DungeonRegion(&m_image, m_fonts[lbl_font].font,
                                      TILE_X, TILE_Y);
    m_region_map  = new MapRegion(Options.tile_map_pixels);
    m_region_inv  = new InventoryRegion(&m_image, m_fonts[lbl_font].font,
                                        TILE_X, TILE_Y);

    m_region_msg  = new MessageRegion(m_fonts[msg_font].font);
    m_region_stat = new StatRegion(m_fonts[stat_font].font);
    m_region_crt  = new CRTRegion(m_fonts[crt_font].font);
    m_region_menu = new MenuRegion(&m_image, m_fonts[crt_font].font);

    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_map);
    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_tile);
    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_inv);
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
    FTFont *font = new FTFont();

    for (unsigned int i = 0; i < m_fonts.size(); i++)
    {
        font_info &finfo = m_fonts[i];
        if (finfo.name == font_file && finfo.size == font_size
            && outline == finfo.outline)
        {
            return i;
        }
    }

    if (!font->load_font(font_file, font_size, outline))
    {
        delete font;
        if (default_on_fail)
            return (load_font("VeraMono.ttf", 12, false, outline));
        else
            return -1;
    }

    font_info finfo;
    finfo.name = font_file;
    finfo.size = font_size;
    finfo.font = font;
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
    int wx = m_region_tile->mx;
    int wy = m_region_tile->my;
    unsigned int *tb = (unsigned int*)alloca(sizeof(unsigned int) *
                                             wy * wx * 2);

    int count = 0;
    for (int y = 0; y < wy; y++)
    {
        for (int x = 0; x < wx; x++)
        {
            unsigned int fg;
            unsigned int bg;

            const coord_def gc(cen.x + x - wx/2,
                               cen.y + y - wy/2);
            const coord_def ep = view2show(grid2view(gc));

            // mini "viewwindow" routine
            if (!map_bounds(gc))
            {
                fg = 0;
                bg = TILE_DNGN_UNSEEN;
            }
            else if (!crawl_view.in_grid_los(gc) || !env.show(ep))
            {
                fg = env.tile_bk_fg(gc);
                bg = env.tile_bk_bg(gc);
                if (!fg && !bg)
                    tileidx_unseen(fg, bg, get_envmap_char(gc), gc);
                bg |=  tile_unseen_flag(gc);
            }
            else
            {
                fg = env.tile_fg[ep.x-1][ep.y-1];
                bg = env.tile_bg[ep.x-1][ep.y-1];
            }

            if (gc == cen)
                bg |= TILE_FLAG_CURSOR1;

            tb[count++] = fg;
            tb[count++] = bg;
        }
    }

    load_dungeon(tb, cen);
    tiles.redraw();
}

void TilesFramework::resize()
{
    calculate_default_options();
    do_layout();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // For ease, vertex positions are pixel positions.
    glOrtho(0, m_windowsz.x, m_windowsz.y, 0, 0, 100);
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
    const int ctrl_offset = CK_CTRL_UP - CK_UP;

    int mod = 0;
    if (keysym.mod & KMOD_SHIFT)
        mod |= MOD_SHIFT;
    if (keysym.mod & KMOD_CTRL)
        mod |= MOD_CTRL;
    if (keysym.mod & KMOD_ALT)
        mod |= MOD_ALT;

    // This is arbitrary, but here's the current mappings.
    // 0-256: ASCII, Crawl arrow keys
    // 0-1k : Other SDL keys (F1, Windows keys, etc...) and modifiers
    // 1k-3k: Non-ASCII with modifiers other than just shift or just ctrl.
    // 3k+  : ASCII with the alt modifier.

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
    int key_offset = (mod & MOD_ALT) ? 3000 : 0;

    bool is_ascii = ((keysym.unicode & 0xFF80) == 0);
    return is_ascii ? (keysym.unicode & 0x7F) + key_offset : 0;
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

    unsigned int res = std::max(30, Options.tile_tooltip_ms);
    return res;
}

// Convenience struct for holding mouse location on screen.
struct cursor_loc
{
    cursor_loc() { reset(); }
    void reset() { reg = NULL; cx = cy = -1; }
    bool operator==(const cursor_loc &rhs) const
    {
        return (rhs.reg == reg
                && rhs.cx == cx
                && rhs.cy == cy
                && reg);
    }

    Region *reg;
    int cx, cy;
};


int TilesFramework::getch_ck()
{
    flush_prev_message();
    SDL_Event event;
    cursor_loc cur_loc;
    cursor_loc tip_loc;

    int key = 0;

    const unsigned int ticks_per_redraw = 16; // 60 FPS = 16.6 ms/frame
    unsigned int last_redraw_tick = 0;

    unsigned int res = std::max(30, Options.tile_tooltip_ms);
    SDL_SetTimer(res, &_timer_callback);

    if (m_need_redraw)
    {
        redraw();
        last_redraw_tick = SDL_GetTicks();
    }

    while (!key)
    {
        unsigned int ticks = 0;

        if (SDL_WaitEvent(&event))
        {
            ticks = SDL_GetTicks();

            if (event.type != SDL_USEREVENT)
            {
                tiles.clear_text_tags(TAG_CELL_DESC);
                m_region_msg->alt_text().clear();
            }

            // TODO enne - need to find a better time to decide when
            // to generate a tip or some way to say "yes, but unchanged".
            if (ticks > m_last_tick_moved)
            {
                m_region_msg->alt_text().clear();
                for (unsigned int i = 0;
                    i < m_layers[m_active_layer].m_regions.size(); i++)
                {
                    Region *reg = m_layers[m_active_layer].m_regions[i];
                    if (!reg->inside(m_mouse.x, m_mouse.y))
                        continue;
                    if (reg->update_alt_text(m_region_msg->alt_text()))
                        break;
                }
            }

            switch (event.type)
            {
            case SDL_KEYDOWN:
                m_key_mod |= _get_modifiers(event.key.keysym);
                key = _translate_keysym(event.key.keysym);
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
                    mouse_event.mod = m_key_mod;
                    key = handle_mouse(mouse_event);

                    // find mouse location
                    for (unsigned int i = 0;
                        i < m_layers[m_active_layer].m_regions.size(); i++)
                    {
                        Region *reg = m_layers[m_active_layer].m_regions[i];
                        if (reg->mouse_pos(m_mouse.x, m_mouse.y,
                                           cur_loc.cx, cur_loc.cy))
                        {
                            cur_loc.reg = reg;
                            break;
                        }
                    }
                }
                break;

            case SDL_MOUSEBUTTONUP:
                {
                    MouseEvent mouse_event;
                    _translate_event(event.button, mouse_event);
                    m_buttons_held &= ~(mouse_event.button);
                    mouse_event.held = m_buttons_held;
                    mouse_event.mod = m_key_mod;
                    key = handle_mouse(mouse_event);
                    m_last_tick_moved = ticks;
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                {
                    MouseEvent mouse_event;
                    _translate_event(event.button, mouse_event);
                    m_buttons_held |= mouse_event.button;
                    mouse_event.held = m_buttons_held;
                    mouse_event.mod = m_key_mod;
                    key = handle_mouse(mouse_event);
                    m_last_tick_moved = ticks;
                }
                break;

            case SDL_QUIT:
                save_game(true);
                ASSERT(!"Shouldn't get here");
                break;

            case SDL_USEREVENT:
            default:
                // This is only used to refresh the tooltip.
                break;
            }
        }

        bool timeout = ((ticks - m_last_tick_moved
                         > (unsigned int)Options.tile_tooltip_ms)
                         && ticks > m_last_tick_moved);
        if (timeout)
            tip_loc = cur_loc;

        if (tip_loc == cur_loc)
        {
            tiles.clear_text_tags(TAG_CELL_DESC);
            if (m_tooltip.empty())
            {
                for (unsigned int i = 0;
                    i < m_layers[m_active_layer].m_regions.size(); i++)
                {
                    Region *reg = m_layers[m_active_layer].m_regions[i];
                    if (!reg->inside(m_mouse.x, m_mouse.y))
                        continue;
                    if (reg->update_tip_text(m_tooltip))
                        break;
                }
            }
        }
        else
        {
            m_tooltip.clear();
            tip_loc.reset();
        }

        if (ticks - last_redraw_tick > ticks_per_redraw)
        {
            redraw();
            last_redraw_tick = ticks;
        }
    }

    SDL_SetTimer(0, NULL);

    return key;
}

static const int map_margin = 2;
static const int map_stat_buffer = 4;
static const int crt_width = 80;
static const int crt_height = 30;
static const int margin = 4;

void TilesFramework::do_layout()
{
    // View size in pixels is (m_viewsc * crawl_view.viewsz)
    m_viewsc.x = 32;
    m_viewsc.y = 32;

    crawl_view.viewsz.x = Options.view_max_width;
    crawl_view.viewsz.y = Options.view_max_height;
    crawl_view.msgsz.x = crt_width;

    // Initial sizes.
    m_region_tile->dx = m_viewsc.x;
    m_region_tile->dy = m_viewsc.y;
    m_region_tile->resize(crawl_view.viewsz.x, crawl_view.viewsz.y);
    m_region_crt->resize(crt_width, crt_height);
    m_region_stat->resize(crawl_view.hudsz.x, crawl_view.hudsz.y);
    m_region_msg->resize(crawl_view.msgsz.x, crawl_view.msgsz.y);
    m_region_map->resize(GXM, GYM);

    // Place regions for normal layer

    m_region_tile->place(0, 0, margin);
    m_region_msg->place(0, m_region_tile->ey - margin, margin);

    bool message_overlay = false;

    if (m_windowsz.y < m_region_tile->dy * VIEW_MIN_HEIGHT)
    {
        crawl_view.viewsz.x = VIEW_MIN_WIDTH;
        crawl_view.viewsz.y = VIEW_MIN_HEIGHT;
        m_region_tile->place(0, 0, 0);
        int factor = m_windowsz.y / crawl_view.viewsz.y;
        m_viewsc.x = m_viewsc.y = std::min(32, factor);
        m_region_tile->dx = m_viewsc.x;
        m_region_tile->dy = m_viewsc.y;
        m_region_tile->resize(crawl_view.viewsz.x, crawl_view.viewsz.y);
        m_region_msg->place(0, 0, 0);
        message_overlay = true;
    }
    else
    {
        // Shrink viewsz if too wide:
        while (m_region_tile->wx + m_region_stat->wx > m_windowsz.x
               && crawl_view.viewsz.x > VIEW_MIN_WIDTH)
        {
            crawl_view.viewsz.x -= 2;
            m_region_tile->mx = crawl_view.viewsz.x;
            m_region_tile->place(0, 0, margin);
            m_region_msg->place(0, m_region_tile->ex, margin);
        }

        // Shrink viewsz if too tall:
        while (m_region_tile->wy + m_region_msg->wy > m_windowsz.y
               && crawl_view.viewsz.y > VIEW_MIN_HEIGHT)
        {
            crawl_view.viewsz.y -= 2;
            m_region_tile->my = crawl_view.viewsz.y;
            m_region_tile->place(0, 0, margin);
            m_region_msg->place(0, m_region_tile->ey, margin);
        }

        // Shrink msgsz if too tall:
        while (m_region_tile->wy + m_region_msg->wy > m_windowsz.y
               && crawl_view.msgsz.y > MSG_MIN_HEIGHT)
        {
            m_region_msg->resize(m_region_msg->mx, --crawl_view.msgsz.y);
        }

        if (m_region_tile->wy + m_region_msg->wy > m_windowsz.y)
        {
            m_region_tile->place(0, 0, 0);
            m_region_msg->place(0, 0, 0);
            message_overlay = true;
        }
    }

    if (message_overlay)
    {
        m_region_msg->resize_to_fit(m_region_tile->ex, m_region_msg->ey);
        m_region_msg->ex = m_region_tile->ex;
    }
    else
    {
        m_region_msg->resize_to_fit(m_region_tile->wx,
                                    m_windowsz.y - m_region_msg->sy);
        int msg_y = std::min(Options.msg_max_height, (int)m_region_msg->my);
        m_region_msg->resize(m_region_msg->mx, msg_y);

        m_region_msg->ex = m_region_tile->ex;
        m_region_msg->ey = m_windowsz.y;
    }
    m_region_msg->set_overlay(message_overlay);
    crawl_view.msgsz.x = m_region_msg->mx;
    crawl_view.msgsz.y = m_region_msg->my;

    // Shrink view width if stat window can't fit...
    int stat_col;
    crawl_view.viewsz.x += 2;
    do
    {
        crawl_view.viewsz.x -= 2;
        m_region_tile->mx = crawl_view.viewsz.x;
        m_region_tile->place(0, 0, margin);

        stat_col = m_region_tile->ex + map_stat_buffer;
        m_region_stat->place(stat_col, 0, 0);
        m_region_stat->resize_to_fit(m_windowsz.x - m_region_stat->sx,
                                     m_region_stat->wy);
    }
    while (m_region_stat->ex > m_windowsz.x
           && crawl_view.viewsz.x > VIEW_MIN_WIDTH);

    // Determine the maximum width.
    m_region_stat->resize_to_fit(m_windowsz.x - m_region_stat->sx,
                                 m_region_stat->wy);

    // Grow HUD horizontally if there's room.
    const int max_hud_width = 50;
    int hud_width = std::min(m_region_stat->mx, max_hud_width);
    m_region_stat->resize(hud_width, m_region_stat->my);
    crawl_view.hudsz.x = m_region_stat->mx;
    crawl_view.hudsz.y = m_region_stat->my;

    // Resize map to fit the screen
    m_region_map->dx = m_region_map->dy = Options.tile_map_pixels;
    m_region_map->place(stat_col, m_region_stat->ey, map_margin);
    while (m_region_map->ex > m_windowsz.x)
    {
        m_region_map->dx--;
        m_region_map->dy--;
        m_region_map->resize(GXM, GYM);
    }

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

    // Place regions for crt layer
    m_region_crt->place(0, 0, margin);
    m_region_crt->resize_to_fit(m_windowsz.x, m_windowsz.y);
    m_region_menu->place(0, 0, margin);
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

    m_region_inv->place(inv_col, m_region_map->ey, 0);
    m_region_inv->resize_to_fit(m_windowsz.x -
                                     m_region_inv->sx,
                                     m_windowsz.y -
                                     m_region_inv->sy);
    m_region_inv->resize(std::min(13, (int)m_region_inv->mx),
                              std::min(6, (int)m_region_inv->my));

    int self_inv_y = m_windowsz.y - m_region_inv->wy - margin;
    m_region_inv->place(inv_col, self_inv_y, 0);

    // recenter map above inventory
    int map_cen_x = (m_region_inv->sx + m_region_inv->ex) / 2;
    map_cen_x = std::min(map_cen_x, (int)(m_windowsz.x - m_region_map->wx/2));
    m_region_map->place(map_cen_x - m_region_map->wx/2, m_region_map->sy,
                        map_margin);

    int num_items = m_region_inv->mx * (m_region_inv->my - 1);
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
}

void TilesFramework::message_out(int which_line, int colour, const char *s, int firstcol, bool newline)
{
    if (!firstcol)
        firstcol = Options.delay_message_clear ? 2 : 1;

    cgotoxy(firstcol, which_line + 1, GOTO_MSG);
    textcolor(colour);

    cprintf("%s", s);

    if (newline && which_line == crawl_view.msgsz.y - 1)
        m_region_msg->scroll();
}

void TilesFramework::clear_message_window()
{
    m_region_msg->clear();
    m_active_layer = LAYER_NORMAL;
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

void TilesFramework::cgotoxy(int x, int y, int region)
{
    if (region == GOTO_LAST)
    {
        // nothing
    }
    else if (region == GOTO_CRT)
    {
        m_active_layer = LAYER_CRT;
        TextRegion::text_mode = m_region_crt;
    }
    else if (region == GOTO_MSG)
    {
        m_active_layer = LAYER_NORMAL;
        TextRegion::text_mode = m_region_msg;
    }
    else if (region == GOTO_STAT)
    {
        m_active_layer = LAYER_NORMAL;
        TextRegion::text_mode = m_region_stat;
    }

    TextRegion::cgotoxy(x, y);
}

void TilesFramework::redraw()
{
    m_need_redraw = false;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glScalef(m_viewsc.x, m_viewsc.y, 1.0f);

    for (unsigned int i = 0; i < m_layers[m_active_layer].m_regions.size(); i++)
        m_layers[m_active_layer].m_regions[i]->render();

    // Draw tooltip
    if (!m_tooltip.empty())
    {
        const coord_def min_pos(0, 0);
        FTFont *font = m_fonts[m_tip_font].font;

        font->render_string(m_mouse.x, m_mouse.y - 2, m_tooltip.c_str(),
                            min_pos, m_windowsz, WHITE, false, 220, BLUE, 5);
    }

    SDL_GL_SwapBuffers();
}

void TilesFramework::update_minimap(int gx, int gy)
{
    if (!player_in_mappable_area())
        return;

    int object = env.map[gx][gy].object;
    map_feature f = (object >= DNGN_START_OF_MONSTERS) ? MF_MONS_HOSTILE :
        get_feature_def((dungeon_feature_type)object).minimap;

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

    if (you.pos() == gc)
    {
        f = MF_PLAYER;
    }
    else if (f == MF_MONS_HOSTILE && mgrd[gx][gy] != NON_MONSTER)
    {
        const int grid = mgrd[gx][gy];
        if (mons_friendly_real(&menv[grid]))
            f = MF_MONS_FRIENDLY;
        else if (mons_neutral(&menv[grid]))
            f = MF_MONS_NEUTRAL;
        else if (mons_class_flag(menv[grid].type, M_NO_EXP_GAIN))
            f = MF_MONS_NO_EXP;
    }
    else if (f == MF_FLOOR || f == MF_MAP_FLOOR)
    {
        if (is_exclude_root(gc))
            f = MF_EXCL_ROOT;
        else if (is_excluded(gc))
            f = MF_EXCL;
    }

    if (f == MF_WALL || f == MF_FLOOR)
    {
        if (is_terrain_known(gx, gy) && !is_terrain_seen(gx, gy)
            || is_envmap_detected_item(gx, gy)
            || is_envmap_detected_mons(gx, gy))
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

static void _fill_item_info(InventoryTile &desc, const item_def &item)
{
    desc.tile = tileidx_item(item);

    int type = item.base_type;
    if (type == OBJ_FOOD || type == OBJ_SCROLLS
        || type == OBJ_POTIONS || type == OBJ_MISSILES)
    {
        // -1 specifies don't display anything
        desc.quantity = item.quantity == 1 ? -1 : item.quantity;
    }
    else if (type == OBJ_WANDS
             && ((item.flags & ISFLAG_KNOW_PLUSES)
                 || item.plus2 == ZAPCOUNT_EMPTY))
    {
        desc.quantity = item.plus;
    }
    else
        desc.quantity = -1;

    if (item_type_known(item))
    {
        if (item.base_type == OBJ_WEAPONS)
        {
            if (!is_fixed_artefact(item)
                && get_weapon_brand(item) != SPWPN_NORMAL)
            {
                desc.special = TILE_BRAND_FLAMING + get_weapon_brand(item) - 1;
            }
        }
        else if (item.base_type == OBJ_MISSILES)
        {
            switch (get_ammo_brand(item))
            {
            case SPMSL_FLAME:
                desc.special = TILE_BRAND_FLAME;
                break;
            case SPMSL_FROST:
                desc.special = TILE_BRAND_FROST;
                break;
            case SPMSL_POISONED:
                desc.special = TILE_BRAND_POISONED;
                break;
            case SPMSL_CURARE:
                desc.special = TILE_BRAND_CURARE;
                break;
            case SPMSL_RETURNING:
                desc.special = TILE_BRAND_RETURNING;
                break;
            case SPMSL_CHAOS:
                desc.special = TILE_BRAND_CHAOS;
                break;
            default:
                break;
            }
        }
    }
    desc.flag = 0;
    if (item_cursed(item) && item_ident(item, ISFLAG_KNOW_CURSE))
        desc.flag |= TILEI_FLAG_CURSE;
    if (item_type_tried(item))
        desc.flag |= TILEI_FLAG_TRIED;
    if (item.pos.x != -1)
        desc.flag |= TILEI_FLAG_FLOOR;
}

void TilesFramework::update_inventory()
{
    std::vector<InventoryTile> inv;

    if (!Options.tile_show_items || crawl_state.arena)
        return;

    // item.base_type <-> char conversion table
    const static char *obj_syms = ")([/%#?=!#+\\0}x";

    const int mx = m_region_inv->mx;
    const int my = m_region_inv->my;

    int max_pack_row = (ENDOFPACK-1) / mx + 1;
    int max_pack_items = max_pack_row * mx;

    bool inv_shown[ENDOFPACK];
    memset(inv_shown, 0, sizeof(inv_shown));

    int num_ground = 0;
    for (int i = igrd(you.pos()); i != NON_ITEM; i = mitm[i].link)
        num_ground++;

    // If the inventory is full, show at least one row of the ground.
    int min_ground = std::min(num_ground, mx);
    max_pack_items = std::min(max_pack_items, mx * my - min_ground);
    max_pack_items = std::min(ENDOFPACK, max_pack_items);

    const size_t show_types_len = strlen(Options.tile_show_items);
    // Special case: show any type if (c == show_types_len).
    for (unsigned int c = 0; c <= show_types_len; c++)
    {
        if ((int)inv.size() >= max_pack_items)
            break;

        bool show_any = (c == show_types_len);

        object_class_type type = OBJ_UNASSIGNED;
        if (!show_any)
        {
            const char *find = strchr(obj_syms, Options.tile_show_items[c]);
            if (!find)
                continue;
            type = (object_class_type)(find - obj_syms);
        }

        // First, normal inventory
        for (int i = 0; i < ENDOFPACK; i++)
        {
            if ((int)inv.size() >= max_pack_items)
                break;

            if (inv_shown[i]
                || !is_valid_item(you.inv[i])
                || you.inv[i].quantity == 0
                || (!show_any && you.inv[i].base_type != type))
            {
                continue;
            }

            InventoryTile desc;
            _fill_item_info(desc, you.inv[i]);
            desc.idx = i;

            for (int eq = 0; eq < NUM_EQUIP; eq++)
            {
                if (you.equip[eq] == i)
                {
                    desc.flag |= TILEI_FLAG_EQUIP;
                    if (!you_tran_can_wear(you.inv[i]))
                        desc.flag |= TILEI_FLAG_MELDED;
                    break;
                }
            }

            inv_shown[i] = true;
            inv.push_back(desc);
        }
    }

    int remaining = mx*my - inv.size();
    int empty_on_this_row = mx - inv.size() % mx;

    // If we're not on the last row...
    if ((int)inv.size() < mx * (my-1))
    {
        if (num_ground > remaining - empty_on_this_row)
        {
            // Fill out part of this row.
            int fill = remaining - num_ground;
            for (int i = 0; i < fill; i++)
            {
                InventoryTile desc;
                if ((int)inv.size() >= max_pack_items)
                    desc.flag |= TILEI_FLAG_INVALID;
                inv.push_back(desc);
            }
        }
        else
        {
            // Fill out the rest of this row.
            while (inv.size() % mx != 0)
            {
                InventoryTile desc;
                if ((int)inv.size() >= max_pack_items)
                    desc.flag |= TILEI_FLAG_INVALID;
                inv.push_back(desc);
            }

            // Add extra rows, if needed.
            unsigned int ground_rows =
                std::max((num_ground-1) / mx + 1, 1);

            while ((int)(inv.size() / mx + ground_rows) < my
                   && ((int)inv.size()) < max_pack_items)
            {
                for (int i = 0; i < mx; i++)
                {
                    InventoryTile desc;
                    if ((int)inv.size() >= max_pack_items)
                        desc.flag |= TILEI_FLAG_INVALID;
                    inv.push_back(desc);
                }
            }
        }
    }

    // Then, as many ground items as we can fit.
    bool ground_shown[MAX_ITEMS];
    memset(ground_shown, 0, sizeof(ground_shown));
    for (unsigned int c = 0; c <= show_types_len; c++)
    {
        if ((int)inv.size() >= mx * my)
            break;

        bool show_any = (c == show_types_len);

        object_class_type type = OBJ_UNASSIGNED;
        if (!show_any)
        {
            const char *find = strchr(obj_syms, Options.tile_show_items[c]);
            if (!find)
                continue;
            type = (object_class_type)(find - obj_syms);
        }

        for (int i = igrd(you.pos()); i != NON_ITEM; i = mitm[i].link)
        {
            if ((int)inv.size() >= mx * my)
                break;

            if (ground_shown[i] || !show_any && mitm[i].base_type != type)
                continue;

            InventoryTile desc;
            _fill_item_info(desc, mitm[i]);
            desc.idx = i;
            ground_shown[i] = true;

            inv.push_back(desc);
        }
    }

    while ((int)inv.size() < mx * my)
    {
        InventoryTile desc;
        desc.flag = TILEI_FLAG_FLOOR;
        inv.push_back(desc);
    }

    m_region_inv->update(inv.size(), &inv[0]);
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

    if (mon->type == MONS_PLAYER_GHOST)
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

void TilesFramework::set_need_redraw()
{
    m_need_redraw = true;
}

bool TilesFramework::need_redraw() const
{
    return m_need_redraw;
}
