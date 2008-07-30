#include "cio.h"
#include "itemname.h"
#include "items.h"
#include "itemprop.h"
#include "mon-util.h"
#include "player.h"
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

void TilesFramework::shutdown()
{
    delete m_region_tile;
    delete m_region_stat;
    delete m_region_msg;
    delete m_region_map;
    delete m_region_self_inv;
    delete m_region_crt;
    delete m_region_menu_inv;

    m_region_tile = NULL;
    m_region_stat = NULL;
    m_region_msg = NULL;
    m_region_map = NULL;
    m_region_self_inv = NULL;
    m_region_crt = NULL;
    m_region_menu_inv = NULL;

    for (unsigned int i = 0; i < LAYER_MAX; i++)
    {
        m_layers[i].m_regions.clear();
    }

    for (unsigned int i = 0; i < m_fonts.size(); i++)
    {
        delete m_fonts[i].font;
        m_fonts[i].font = NULL;
    }

    SDL_Quit();
}

bool TilesFramework::initialise()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        printf ("Failed to initialise SDL: %s\n", SDL_GetError());
        return false;
    }

    SDL_EnableUNICODE(true);

    SDL_WM_SetCaption(CRAWL " " VERSION, CRAWL);
    SDL_Surface *icon = IMG_Load("dat/tiles/stone_soup_icon-32x32.png");
    if (!icon)
    {
        printf ("Failed to load icon: %s\n", SDL_GetError());
        return false;
    }
    SDL_WM_SetIcon(icon, NULL);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  
    unsigned int flags = SDL_OPENGL;
    if (Options.tile_full_screen)
        flags |= SDL_FULLSCREEN;

    m_windowsz.x = Options.tile_window_width;
    m_windowsz.y = Options.tile_window_height;

    m_context = SDL_SetVideoMode(m_windowsz.x, m_windowsz.y, 0, flags);
    if (!m_context)
    {
        printf ("Failed to set video mode: %s\n", SDL_GetError());
        return false;
    }

    if (!m_image.load_textures())
        return false;

    int crt_font = load_font(Options.tile_font_crt_file.c_str(),
                             Options.tile_font_crt_size);
    int msg_font = load_font(Options.tile_font_msg_file.c_str(),
                             Options.tile_font_msg_size);
    int stat_font = load_font(Options.tile_font_stat_file.c_str(),
                              Options.tile_font_stat_size);
    m_tip_font = load_font(Options.tile_font_tip_file.c_str(),
                           Options.tile_font_tip_size);
    int lbl_font = load_font(Options.tile_font_lbl_file.c_str(),
                             Options.tile_font_lbl_size);

    if (crt_font == -1 || msg_font == -1 || stat_font == -1
        || m_tip_font == -1 || lbl_font == -1)
    {
        return false;
    }

    m_region_tile = new DungeonRegion(&m_image, m_fonts[lbl_font].font, 
                                      TILE_X, TILE_Y);
    m_region_map = new MapRegion(Options.tile_map_pixels);
    m_region_self_inv = new InventoryRegion(&m_image, TILE_X, TILE_Y);

    m_region_msg = new MessageRegion(m_fonts[msg_font].font);
    m_region_stat = new StatRegion(m_fonts[stat_font].font);
    m_region_crt = new CRTRegion(m_fonts[crt_font].font);
    m_region_menu_inv = new InventoryRegion(&m_image, TILE_X, TILE_Y);

    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_tile);
    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_map);
    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_self_inv);
    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_msg);
    m_layers[LAYER_NORMAL].m_regions.push_back(m_region_stat);

    m_layers[LAYER_CRT].m_regions.push_back(m_region_crt);
    m_layers[LAYER_CRT].m_regions.push_back(m_region_menu_inv);

    cgotoxy(1, 1, GOTO_CRT);

    GLStateManager::init();

    resize();

    return true;
}

int TilesFramework::load_font(const char *font_file, int font_size,
                              bool default_on_fail)
{
    FTFont *font = new FTFont();

    for (unsigned int i = 0; i < m_fonts.size(); i++)
    {
        font_info &finfo = m_fonts[i];
        if (finfo.name == font_file && finfo.size == font_size)
            return i;
    }
    
    if (!font->load_font(font_file, font_size))
    {
        delete font;
        if (default_on_fail)
            return (load_font("VeraMono.ttf", 12, false));
        else
            return -1;
    }

    font_info finfo;
    finfo.name = font_file;
    finfo.size = font_size;
    finfo.font = font;
    m_fonts.push_back(finfo);

    return (m_fonts.size() - 1);
}

void TilesFramework::load_dungeon(unsigned int *tileb, int gx, int gy)
{
    unsigned int ox = m_region_tile->mx/2;
    unsigned int oy = m_region_tile->my/2;
    m_region_tile->load_dungeon(tileb, gx - ox, gy - oy);

    coord_def win_start(gx - ox, gy - oy);
    coord_def win_end(gx + ox + 1, gy + oy + 1);

    m_region_map->set_window(win_start, win_end);
}

void TilesFramework::load_dungeon(int cx, int cy)
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
            int fg;
            int bg;

            const coord_def gc(cx + x - wx/2,
                               cy + y - wy/2);
            const coord_def ep = view2show(grid2view(gc));

            // mini "viewwindow" routine
            if (!map_bounds(gc))
            {
                fg = 0;
                bg = TILE_DNGN_UNSEEN;
            }
            else if (!crawl_view.in_grid_los(gc) || !env.show(ep))
            {
                fg = env.tile_bk_fg[gc.x][gc.y];
                bg = env.tile_bk_bg[gc.x][gc.y];
                if (bg == 0)
                    bg |= TILE_DNGN_UNSEEN;
                bg |=  tile_unseen_flag(gc);
            }
            else
            {
                fg = env.tile_fg[ep.x-1][ep.y-1];
                bg = env.tile_bg[ep.x-1][ep.y-1];
            }

            if (gc.x == cx && gc.y == cy)
                bg |= TILE_FLAG_CURSOR1;

            tb[count++] = fg;
            tb[count++] = bg;
        }
    }

    load_dungeon(tb, cx, cy);
    tiles.redraw();
}

void TilesFramework::resize()
{
    do_layout();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // TODO enne - need better resizing
    // View size in pixels is (m_viewsc * crawl_view.viewsz)
    m_viewsc.x = 32;
    m_viewsc.y = 32;

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

    int offset = 0;
    if (keysym.mod & KMOD_CTRL)
        offset = ctrl_offset;
    else if (keysym.mod & KMOD_SHIFT)
        offset = shift_offset;

    switch (keysym.sym)
    {
    case SDLK_RETURN:
        return CK_ENTER;
    case SDLK_BACKSPACE:
        return CK_BKSP;
    case SDLK_ESCAPE:
        return CK_ESCAPE;
    case SDLK_DELETE:
        return CK_DELETE;

    case SDLK_KP5:
        return '5';

    case SDLK_KP8:
    case SDLK_UP:
        return CK_UP + offset;
    case SDLK_KP2:
    case SDLK_DOWN:
        return CK_DOWN + offset;
    case SDLK_KP4:
    case SDLK_LEFT:
        return CK_LEFT + offset;
    case SDLK_KP6:
    case SDLK_RIGHT:
        return CK_RIGHT + offset;
    case SDLK_KP0:
    case SDLK_INSERT:
        return CK_INSERT + offset;
    case SDLK_KP7:
    case SDLK_HOME:
        return CK_HOME + offset;
    case SDLK_KP1:
    case SDLK_END:
        return CK_END + offset;
    case SDLK_CLEAR:
        return CK_CLEAR + offset;
    case SDLK_KP9:
    case SDLK_PAGEUP:
        return CK_PGUP + offset;
    case SDLK_KP3:
    case SDLK_PAGEDOWN:
        return CK_PGDN + offset;
    default:
        break;
    }

    bool is_ascii = ((keysym.unicode & 0xFF80) == 0);
    return is_ascii ? keysym.unicode & 0x7F : 0;
}


int TilesFramework::handle_mouse(MouseEvent &event)
{
    m_region_tile->place_cursor(CURSOR_MOUSE, Region::NO_CURSOR);

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
        && event.button == MouseEvent::LEFT
        && event.event == MouseEvent::PRESS)
    {
        return '\r';
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
    tile_event.held = MouseEvent::NONE;
    tile_event.event = MouseEvent::MOVE;
    tile_event.button = MouseEvent::NONE;
    tile_event.px = sdl_event.x;
    tile_event.py = sdl_event.y;

    // TODO enne - do we want the relative motion?
}

static void _translate_event(const SDL_MouseButtonEvent &sdl_event,
    MouseEvent &tile_event)
{
    tile_event.held = MouseEvent::NONE;
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
        ASSERT(!"Unhandled button");
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

int TilesFramework::getch_ck()
{
    SDL_Event event;

    int key = 0;

    const unsigned int ticks_per_redraw = 16; // 60 FPS = 16.6 ms/frame
    unsigned int last_redraw_tick = 0;

    unsigned int res = std::max(30, Options.tile_tooltip_ms);
    SDL_SetTimer(res, &_timer_callback);

    while (!key)
    {
        unsigned int ticks;

        if (SDL_WaitEvent(&event))
        {
            ticks = SDL_GetTicks();
          
            if (event.type != SDL_USEREVENT)
                tiles.clear_text_tags(TAG_CELL_DESC);

            switch (event.type)
            {
            case SDL_KEYDOWN:
                m_key_mod |= _get_modifiers(event.key.keysym);
                key = _translate_keysym(event.key.keysym);

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
                // TODO enne
                exit(0);
                
            case SDL_USEREVENT:
            default:
                // This is only used to refresh the tooltip.
                break;
            }
        }

        bool show_tooltip = ((ticks - m_last_tick_moved 
                                > (unsigned int)Options.tile_tooltip_ms)
                                && ticks > m_last_tick_moved);

        if (show_tooltip)
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

void TilesFramework::do_layout()
{
    const int map_stat_buffer = 5;

    // TODO enne - use options
    const int crt_width = 80;
    const int crt_height = 30;
    const int map_margin = 2;

    // Size regions that we know about
    m_region_tile->resize(crawl_view.viewsz.x, crawl_view.viewsz.y);
    m_region_crt->resize(crt_width, crt_height);
    m_region_stat->resize(crawl_view.hudsz.x, crawl_view.hudsz.y);
    m_region_msg->resize(crawl_view.msgsz.x, crawl_view.msgsz.y);
    m_region_map->resize(GXM, GYM);


    // Place regions for normal layer
    const int margin = 4;
    m_region_tile->place(0, 0, margin);
    m_region_msg->place(0, m_region_tile->ey, margin);

    int stat_col = m_region_tile->ex + map_stat_buffer;

    m_region_stat->place(stat_col, 0, 0);
    m_region_map->place(stat_col, m_region_stat->ey, map_margin);

    int inv_col = std::max(m_region_tile->ex, m_region_msg->ex);

    m_region_self_inv->place(inv_col, m_region_map->ey, 0);
    m_region_self_inv->resize_to_fit(m_windowsz.x - 
                                          m_region_self_inv->sx,
                                          m_windowsz.y -
                                          m_region_self_inv->sy);

    // Place regions for crt layer
    m_region_crt->place(0, 0, margin);
    m_region_menu_inv->place(0, m_region_crt->ey, margin);
    m_region_menu_inv->resize_to_fit(m_windowsz.x, m_windowsz.y -
                                          m_region_menu_inv->sy);
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
    if (m_region_menu_inv)
        m_region_menu_inv->clear();

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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glScalef(m_viewsc.x, m_viewsc.y, 1.0f);

    for (unsigned int i = 0; i < m_layers[m_active_layer].m_regions.size(); i++)
    {
        m_layers[m_active_layer].m_regions[i]->render();
    }

    // Draw tooltip
    if (!m_tooltip.empty())
    {
        const coord_def min_pos(0, 0);
        FTFont *font = m_fonts[m_tip_font].font;
        font->render_string(m_mouse.x, m_mouse.y - 2, m_tooltip.c_str(),
                            min_pos, m_windowsz, WHITE, false, 150,
                            BLUE, 5);
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
        if (mons_friendly(&menv[grid]))
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
    {
        desc.quantity = -1;
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

    if (!Options.tile_show_items)
        return;

    unsigned int max_pack_row = (ENDOFPACK-1) / m_region_self_inv->mx + 1;
    max_pack_row = std::min(m_region_self_inv->my - 1, max_pack_row);
    unsigned int max_pack_items = max_pack_row * m_region_self_inv->mx;

    // TODO enne - document that '.' and '_' no longer work

    // TODO enne - if all inventory and ground can't fit, allow ground
    // and inventory items on the same row.

    // item.base_type <-> char conversion table
    const static char *obj_syms = ")([/%#?=!#+\\0}x";

    for (unsigned int c = 0; c < strlen(Options.tile_show_items); c++)
    {
        if (inv.size() >= max_pack_items)
            break;

        const char *find = strchr(obj_syms, Options.tile_show_items[c]);
        if (!find)
            continue;
        object_class_type type = (object_class_type)(find - obj_syms);

        // First, normal inventory
        for (int i = 0; i < ENDOFPACK; i++)
        {
            if (!is_valid_item(you.inv[i]) || you.inv[i].quantity == 0)
                continue;

            if (you.inv[i].base_type != type)
                continue;

            InventoryTile desc;
            _fill_item_info(desc, you.inv[i]);
            desc.idx = i;

            for (int eq = 0; eq < NUM_EQUIP; eq++)
            {
                if (you.equip[eq] == i)
                {
                    desc.flag |= TILEI_FLAG_EQUIP;
                    break;
                }
            }

            inv.push_back(desc);
        }
    }

    // Finish out this row
    while (inv.size() % m_region_self_inv->mx != 0)
    {
        InventoryTile desc;
        inv.push_back(desc);
    }

    // How many ground items do we have?
    unsigned int num_ground = 0;
    for (int i = igrd(you.pos()); i != NON_ITEM; i = mitm[i].link)
        num_ground++;

    // Add extra rows, if needed.
    unsigned int ground_rows = 
        std::max(((int)num_ground-1) / (int)m_region_self_inv->mx + 1, 1);

    while (inv.size() / m_region_self_inv->mx + ground_rows < m_region_self_inv->my)
    {
        for (unsigned int i = 0; i < m_region_self_inv->mx; i++)
        {
            InventoryTile desc;
            inv.push_back(desc);
        }
    }

    // Then, ground items...
    for (unsigned int c = 0; c < strlen(Options.tile_show_items); c++)
    {
        if (inv.size() >= m_region_self_inv->mx * m_region_self_inv->my)
            break;

        const char *find = strchr(obj_syms, Options.tile_show_items[c]);
        if (!find)
            continue;
        object_class_type type = (object_class_type)(find - obj_syms);

        for (int i = igrd(you.pos()); i != NON_ITEM; i = mitm[i].link)
        {
            if (inv.size() >= m_region_self_inv->mx * m_region_self_inv->my)
                break;

            if (mitm[i].base_type != type)
                continue;

            InventoryTile desc;
            _fill_item_info(desc, mitm[i]);
            desc.idx = i;

            inv.push_back(desc);
        }
    }

    // Finish out ground inventory
    while (inv.size() < m_region_self_inv->mx * m_region_self_inv->my)
    {
        InventoryTile desc;
        desc.flag = TILEI_FLAG_FLOOR;
        inv.push_back(desc);
    }

    m_region_self_inv->update(inv.size(), &inv[0]);
}

void TilesFramework::update_menu_inventory(unsigned int slot, 
                                           const item_def &item, 
                                           bool selected, char key)
{
    InventoryTile desc;
    _fill_item_info(desc, item);
    desc.key = key;
    desc.idx = (desc.flag & TILEI_FLAG_FLOOR) ? item.index() : 
                                                letter_to_index(key);
    if (selected)
        desc.flag |= TILEI_FLAG_SELECT;

    m_region_menu_inv->update_slot(slot, desc);
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

bool TilesFramework::initialise_items()
{
    return (m_image.load_item_texture());
}

const coord_def &TilesFramework::get_cursor() const
{
    return (m_region_tile->get_cursor());
}
