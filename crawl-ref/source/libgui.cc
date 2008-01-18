/*
 *  File:       libx11.cc
 *  Summary:    Functions for x11
 *  Written by: M.Itakura
 *
 *  Change History (most recent first):
 *
 *      <1>     03/08/11        Ita     copied from liblinux.cc and modified
 *
 */

#define deblog(x)  {FILE *ddfp=fopen("log.txt","a");fprintf(ddfp,x);fprintf(ddfp,"\n");fclose(ddfp);}
#define deblog2(x,y)  {FILE *ddfp=fopen("log.txt","a");fprintf(ddfp,"%s %d\n",x,y);fclose(ddfp);}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AppHdr.h"
#include "cio.h"
#include "defines.h"
#include "describe.h"
#include "direct.h"
#include "files.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "externs.h"
#include "guic.h"
#include "message.h"
#include "mon-util.h"
#include "player.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "tiles.h"
#include "travel.h"
#include "version.h"
#include "view.h"

int tile_dngn_x;
int tile_dngn_y;
#define DCX (tile_dngn_x/2)
#define DCY (tile_dngn_y/2)

// libx11.cc or libwt.cc
extern void update_tip_text(const char *tip);
extern void libgui_init_sys();
extern void libgui_shutdown_sys();
extern void GetNextEvent(int *etype,
    int *key, bool *shift, bool *ctrl,
    int *x1, int *y1, int *x2, int *y2);
#ifdef WIN32TILES
// libwt.cc
extern bool windows_change_font(char *font_name, int *font_size, bool dos);
extern void windows_get_winpos(int *x, int *y);
extern void TileInitWin();
#endif

// Main window
WinClass *win_main;
// Regions
TextRegionClass *region_crt = NULL;
MapRegionClass  *region_map = NULL;
TileRegionClass *region_tile = NULL;
TextRegionClass *region_stat = NULL;
TextRegionClass *region_msg = NULL;
TextRegionClass *region_dngn = NULL;
TextRegionClass *region_xmap = NULL;
TextRegionClass *region_tip = NULL;

TileRegionClass *region_item = NULL;
TileRegionClass *region_item2 = NULL;

// Raw tile images
img_type TileImg, TileIsoImg;
img_type PlayerImg;
img_type WallImg;

extern bool force_redraw_tile;
extern bool force_redraw_inv;

// for item use gui
#define MAX_ITEMLIST 60
extern int itemlist[MAX_ITEMLIST];
extern int itemlist_num[MAX_ITEMLIST];
extern int itemlist_idx[MAX_ITEMLIST];
extern char itemlist_key[MAX_ITEMLIST];
extern int itemlist_iflag[MAX_ITEMLIST];
extern int itemlist_flag;
extern int itemlist_n;

static bool gui_smart_cursor = false;

// Window prefs
static int crt_x = 80;
static int crt_y = 30;
static int map_px = 3;
static int msg_x = 80, msg_y = 8;
static int dngn_x = 17, dngn_y = 17;
static int winox = 0, winoy = 0;
#define MAX_PREF_CHAR 256

#ifdef USE_X11
#define UseDosChar false
static char font_name[MAX_PREF_CHAR+1] = "8x16";
#endif

#ifdef WIN32TILES
static char font_name[MAX_PREF_CHAR+1] = "courier";
#define UseDosChar (Options.use_dos_char)
static char dos_font_name[MAX_PREF_CHAR+1] = "Terminal";
static int dos_font_size = 16;
#endif

static int font_size = 12;

#define PREF_MODE_TEXT 0
#define PREF_MODE_TILE 1
#define PREF_MODE_ISO  2
#define PREF_MODE_NUM  3
static const char *pref_mode_name[PREF_MODE_NUM]
   ={ "Text", "Tile", "Iso"};

typedef struct prefs
{
    const char *name;
    const char *tagname;
    char type;
    void *ptr;
    int min, max;
    int dummy_idx;
}prefs;

#ifdef WIN32TILES
#define MAX_PREFS 11
#else
#define MAX_PREFS 9
#endif

#define MAX_EDIT_PREFS 5
static int dummy_int[PREF_MODE_NUM][8];
static char dummy_str[PREF_MODE_NUM][2][MAX_PREF_CHAR+1];
struct prefs pref_data[MAX_PREFS] = 
{
    {"DUNGEON X", "DngnX", 'I', &dngn_x, 17, 35,  0},
    {"DUNGEON Y", "DngnY", 'I', &dngn_y, 17, 35,  1},
    {"MAP PX   ", "MapPx", 'I', &map_px, 1,  10,  2},
    {"MSG X    ", "MsgX",  'I', &msg_x,  40, 80,  3},
    {"MSG Y    ", "MsgY",  'I', &msg_y,  8,  20,  4},
    {"WIN TOP  ", "WindowTop", 'I', &winox, -100, 2000, 5},
    {"WIN LEFT ", "WindowLeft",'I', &winoy, -100, 2000, 6},
    {"FONT     ", "FontName", 'S', font_name, 0,0,  0},
    {"FONT SIZE", "FontSize", 'I', &font_size, 8,24, 7}
#ifdef WIN32TILES
    ,{"DOS FONT", "DosFontName", 'S', dos_font_name, 0,0,  1},
    {"DOS FONT SIZE", "DosFontSize", 'I', &dos_font_size, 8,24, 8}
#endif
};

static int pref_mode = 0;
static void libgui_load_prefs();
static void libgui_save_prefs();

//Internal variables

static int mouse_mode = MOUSE_MODE_NORMAL;
// hack: prevent clrscr for some region
static bool region_lock[NUM_REGIONS];
static bool toggle_telescope;

#define char_is_wall(ch) (ch == 127 || (ch >= 137 && ch <= 140) \
    || ch == 177 || ch == 176 || ch == '#') 

#define char_is_item(ch) ( (ch == 33)||(ch == 34)||(ch == 36)||(ch == 37) \
    ||(ch == 40) ||(ch == 41)||(ch == 43)||(ch == 47)||(ch == 48) \
    ||(ch == 58) ||(ch == 61)||(ch == 63)||(ch == 88)||(ch == 91) \
    ||(ch == 92) ||(ch == 93)||(ch == 125) || (ch == '~'))

#define char_is_monster(ch) ( (ch>='@' && ch<='Z') || (ch>='a' && ch<='z') \
    || ch=='&' || (ch>='1' && ch<='5') || ch == ';')

/***********************************************************/
//micromap color array
#define MAP_XMAX GXM
#define MAP_YMAX GYM

static unsigned char gmap_data[GXM][GYM];
static int gmap_min_x, gmap_max_x;
static int gmap_min_y, gmap_max_y;
static int gmap_ox, gmap_oy;

// redefine color constants with shorter name to save space
#define PX_0  MAP_BLACK        //unseen
#define PX_F  MAP_LTGREY       //floor
#define PX_W  MAP_DKGREY       //walls
#define PX_D  MAP_BROWN        //doors
#define PX_WB MAP_LTBLUE       //blue wall
#define PX_I  MAP_GREEN        //items
#define PX_M  MAP_RED          //monsters
#define PX_US MAP_BLUE         //upstair
#define PX_DS MAP_MAGENTA      //downstair
#define PX_SS MAP_CYAN         //special stair
#define PX_WT MAP_MDGREY       //water
#define PX_LV MAP_MDGREY       //lava
#define PX_T  MAP_YELLOW       //trap
#define PX_MS MAP_CYAN         //misc

static const char gmap_col[256] = {
    /* 0x00 */  PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, 
    /* 0x08 */  PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0,
    /* 0x10 */  PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, 
    /* 0x18 */  PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0,

    /*           ' '   '!'   '"'   '#'   '$'   '%'   '&'   ''' */
    /* 0x20 */  PX_0, PX_I, PX_I, PX_F, PX_I, PX_I, PX_M, PX_D,
    /*           '('   ')'   '*'   '+'   ','   '-'   '.'   '/' */  
    /* 0x28 */  PX_I, PX_I, PX_WB,PX_I, PX_F, PX_0, PX_F, PX_I,
    /*           0     1     2     3     4     5     6     7   */
    /* 0x30 */  PX_0, PX_M, PX_M, PX_M, PX_M, PX_M, PX_0, PX_0,
    /*           8     9     :     ;     <     =     >     ?   */
    /* 0x38 */  PX_MS,PX_0, PX_0, PX_M, PX_US,PX_I, PX_DS,PX_I, 
    /*           @     A     B     C     D     E     F     G   */
    /* 0x40 */  PX_M, PX_M, PX_M, PX_M, PX_M, PX_M, PX_M, PX_M,
    /*           H     I     J     K     L     M     N     O   */
    /* 0x48 */  PX_M, PX_M, PX_M, PX_M, PX_M, PX_M, PX_M, PX_M,
    /*           P     Q     R     S     T     U     V     W   */
    /* 0x50 */  PX_M, PX_M, PX_M, PX_M, PX_M, PX_M, PX_M, PX_M,
    /*           X     Y     Z     [     \     ]     ^     _   */
    /* 0x58 */  PX_M, PX_M, PX_M, PX_I, PX_I, PX_I, PX_T, PX_MS,
    /*           `     a     b     c     d     e     f     g   */
    /* 0x60 */  PX_0, PX_M, PX_M, PX_M, PX_M, PX_M, PX_M, PX_M,
    /*           h     i     j     k     l     m     n     o   */
    /* 0x68 */  PX_M, PX_M, PX_M, PX_M, PX_M, PX_M, PX_M, PX_M,
    /*           p     q     r     s     t     u     v     w   */
    /* 0x70 */  PX_M, PX_M, PX_M, PX_M, PX_M, PX_M, PX_M, PX_M,
    /*           x     y     z     {     |     }     ~    WALL */
    /* 0x78 */  PX_M, PX_M, PX_M, PX_WT,PX_0, PX_I, PX_I, PX_W,
    /*          old cralwj symbols */
    /*           ��     �s   �E    ��    ��    ��    ��    ��    */
    /* 0x80 */  PX_D, PX_SS,PX_F, PX_MS,PX_MS,PX_MS,PX_D, PX_WT,
    /*           �t    ��    ��    ��    ��    ��     �� */
    /* 0x88 */  PX_SS,PX_W, PX_W, PX_W, PX_W, PX_LV, PX_I, PX_0,
    /**/
    /* 0x90 144*/  PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0,
    /* 0x98 152*/  PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0,
    /* 0xa0 160*/  PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0,
    /* 0xa8 168*/  PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0,
    /* 0xb0 176*/  PX_WB,PX_W, PX_W, PX_0, PX_0, PX_0, PX_0, PX_0,
    /* 0xb8 184*/  PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0,
    /* 0xc0 192*/  PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0,
    /* 0xc8 200*/  PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0,
    /* 0xd0 208*/  PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0,
    /* 0xd8 216*/  PX_0, PX_0, PX_0, PX_0, PX_MS, PX_0, PX_0, PX_0,
    /* 0xe0 224*/  PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0,
    /* 0xe8 232*/  PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_0, PX_MS,
    /* 0xf0 240*/  PX_0, PX_0, PX_0, PX_0, PX_MS,PX_0, PX_0, PX_WT,
    /* 0xf8 248*/  PX_0, PX_F, PX_F, PX_0, PX_0, PX_0, PX_D, PX_0
};

int mouse_grid_x;
int mouse_grid_y;
bool mouse_in_dngn = false;

void gui_set_mouse_view_pos(bool in_dngn, int cx, int cy)
{
    const coord_def& gc = view2grid(coord_def(cx,cy));
    mouse_grid_x = gc.x;
    mouse_grid_y = gc.y;
    ASSERT(!in_dngn || gc.x >= 0 && gc.y >= 0 && gc.x < GXM && gc.y < GYM);

    mouse_in_dngn = in_dngn;
}

bool gui_get_mouse_grid_pos(coord_def &gc)
{
    if (mouse_in_dngn)
    {
        gc.x = mouse_grid_x;
        gc.y = mouse_grid_y;
        ASSERT(gc.x >= 0 && gc.y >= 0 && gc.x < GXM && gc.y < GYM);
    }

    return mouse_in_dngn;
}

int inv_idx = 0;
InvAction inv_action = INV_NUMACTIONS;

void gui_set_mouse_inv(int idx, InvAction act)
{
    inv_idx = idx;
    inv_action = act;
}

void gui_get_mouse_inv(int &idx, InvAction &act)
{
    idx = inv_idx;
    act = inv_action;
}

int tile_idx_unseen_terrain(int x, int y, int what)
{
    const coord_def gc(x,y);
    unsigned int feature = grd(gc);

    unsigned int grid_symbol;
    unsigned short grid_color;
    get_item_symbol(feature, &grid_symbol, &grid_color);

    if (feature == DNGN_SECRET_DOOR)
        feature = (unsigned int)grid_secret_door_appearance(x, y);

    int t = tileidx_feature(feature);
    if (t == TILE_ERROR || what == ' ')
        t = tileidx_unseen(what, gc);

    t |= tile_unseen_flag(gc);

    return t;
}

void GmapUpdate(int x, int y, int what, bool upd_tile)
{
    int c;
    
    if (x == you.x_pos && y == you.y_pos)
        c = MAP_WHITE; // player position always in white
    else if (mgrd[x][y] != NON_MONSTER && mons_friendly(&menv[mgrd[x][y]])
             && upd_tile)
        c = MAP_LTRED; // friendly monsters subtly different from hostiles
    else
    {
        const coord_def gc(x,y);
        unsigned int feature = grd(gc);

        unsigned int grid_symbol;
        unsigned short grid_color;
        get_item_symbol(feature, &grid_symbol, &grid_color);

        switch (what)
        {
        // In some cases (like smoke), update the gmap with the ground color
        // instead.  This keeps it prettier in the case of lava + smoke.
        case '#':
            c = gmap_col[grid_symbol & 0xff];
            break;
        default:
            c = gmap_col[what & 0xff];
            break;
        }
    }
    int oldc = gmap_data[x][y];
    gmap_data[x][y] = c;

    // NOTE: In the case of a magic mapping, we really want the
    // map to update with the correct feature (stone instead of rock wall,
    // fountains instead of TILE_ERROR).  tileidx_unseen won't give that
    // to us.  However, we can't give the player all the information,
    // since that might be cheating in some cases.  This could be rewritten.
    int t = tile_idx_unseen_terrain(x, y, what);

    if (c!=0 && c!= oldc && upd_tile)
    {
        if (c == PX_I || c == PX_M)
        {
            env.tile_bk_fg[x][y]= t;
            if (env.tile_bk_bg[x][y] == 0)
                env.tile_bk_bg[x][y] = tileidx_feature(DNGN_UNSEEN);
        }
        else
        {
            env.tile_bk_bg[x][y] = t;
        }
    }
    else if (c==0)
    {
        // forget map
        env.tile_bk_fg[x][y]=0;
        env.tile_bk_bg[x][y]=t;
        return;
    }

    if (x < gmap_min_x)
        gmap_min_x = x;
    else if (x > gmap_max_x)
        gmap_max_x = x;

    if (y < gmap_min_y)
        gmap_min_y = y;
    else if (y > gmap_max_y)
        gmap_max_y = y;
}

void GmapInit(bool upd_tile)
{
    int x, y;
    gmap_min_x = gmap_max_x = you.x_pos -1;
    gmap_min_y = gmap_max_y = you.y_pos -1;

    for (y = 0; y < GYM; y++)
    {
        for (x = 0; x < GXM; x++)
        {
             GmapUpdate(x, y, env.map[x][y].glyph(), upd_tile);
        }
    }
}

void GmapDisplay(int linex, int liney)
{
    static unsigned char buf2[GXM*GYM];

    int x,y;
    int count=0;
    int ox, oy;

    for (x = 0; x < GXM*GYM; x++)
    {
        buf2[x] = 0;
    }

    ox = ( gmap_min_x + (GXM -1 - gmap_max_x) ) / 2;
    oy = ( gmap_min_y + (GYM -1 - gmap_max_y) ) / 2;
    count = ox + oy * GXM;
    gmap_ox = ox - gmap_min_x;
    gmap_oy = oy - gmap_min_y;

    for (y = gmap_min_y; y <= gmap_max_y; y++)
    {
        for (x = gmap_min_x; x <= gmap_max_x; x++)
        {
            if ( (count>=0)&&(count<GXM*GYM) )
                buf2[count] = gmap_data[x][y];
            count += 1;
        }
        count += GXM - (gmap_max_x - gmap_min_x + 1);
    }

    if ( (you.level_type != LEVEL_LABYRINTH)&&(you.level_type != LEVEL_ABYSS) )
    {
        ox += linex - gmap_min_x;
        oy += liney - gmap_min_y;
        buf2[ ox + oy * GXM] = MAP_WHITE; // highlight centre of the map
    }

    region_map->flag = true;
    region_map->draw_data(buf2);
}

/* initialize routines */
static void do_layout()
{
#define LAYER_NML 0
#define LAYER_CRT 1
#define LAYER_XMAP 2
    win_main->wx = win_main->wy = 10;

    int tm = region_crt->dx;

    if (UseDosChar)
        win_main->placeRegion(region_xmap, LAYER_XMAP, 0, 0, tm, tm, tm, tm);

    RegionClass *lowest;

    // tile 2d mode
    win_main->placeRegion(region_tile, LAYER_NML, 0, 0);
    win_main->placeRegion(region_msg, LAYER_NML, region_tile, PLACE_BOTTOM,
        tm, tm, tm, tm);

    int sx = std::max(region_msg->ex + region_msg->dx, region_tile->ex);
    int sy = 0;
    win_main->placeRegion(region_stat, LAYER_NML, sx, sy);

    win_main->placeRegion(region_map, LAYER_NML, region_stat, PLACE_BOTTOM);

    if (region_tip)
    {
        win_main->placeRegion(region_tip, LAYER_NML, region_map, PLACE_BOTTOM);
        lowest = region_tip;
    }
    else
    {
        lowest = region_map;
    }

    int item_x = (win_main->wx-2*tm) / TILE_X;
    int item_y = (52 + item_x -1) / item_x;

    region_item2 -> resize(item_x, item_y, TILE_X, TILE_Y);

    // Update crt size appropriately, based on the window size.
    int crt_pwx = win_main->wx;
    int crt_wx = crt_pwx / region_crt->dx;
    int crt_pwy = win_main->wy - 2*tm - item_y * region_item2->dy;
    int crt_wy = crt_pwy / region_crt->dy;
    region_crt->resize(crt_wx, crt_wy);

    win_main->placeRegion(region_crt, LAYER_CRT, 0, 0, tm, tm, tm, tm);
    win_main->placeRegion (region_item2, LAYER_CRT, region_crt, PLACE_BOTTOM,
        tm, tm, tm, tm);

    if (Options.show_items[0] != 0)
    {
        item_x = (win_main->wx - lowest->sx) / TILE_X;
        item_y = (win_main->wy - lowest->ey) / TILE_Y;
        RegionClass *r0 = lowest;
        int place = PLACE_BOTTOM;

        int item_x2 = (win_main->wx - region_msg->ex) / TILE_X;
        int item_y2 = (win_main->wy - region_msg->sy) / TILE_Y;

        if (item_x * item_y < item_x2 * item_y2 &&
            lowest->ey < region_msg->sy)
        {
            item_x = item_x2;
            item_y = item_y2;
            r0 = region_msg;
            place = PLACE_RIGHT;
        }
        while(item_x * item_y < 40) item_y++;
        region_item -> resize(item_x, item_y, TILE_X, TILE_Y);
        win_main->placeRegion (region_item, LAYER_NML, r0, place);
    }
}

void libgui_init()
{
    int i;
    int map_margin = 2;

    libgui_init_sys();

    for(i=0;i<NUM_REGIONS;i++)
        region_lock[i] = false;

    pref_mode = PREF_MODE_TILE;

    libgui_load_prefs();

    // Adjust sizes
    if (dngn_x & 1 ==0) dngn_x++;
    if (dngn_y & 1 ==0) dngn_y++;
    if (font_size &1 == 1) font_size--;

    tile_dngn_x = dngn_x;
    tile_dngn_y = dngn_y;

    /****************************************************/

    win_main = new WinClass();
    win_main->ox = winox;
    win_main->oy = winoy;


    region_crt = new TextRegionClass(crt_x, crt_y, 0, 0);
    region_crt->id = REGION_CRT;
    TextRegionClass::text_mode = region_crt;

    region_map =
        new MapRegionClass(MAP_XMAX-map_margin*2,
                           MAP_YMAX-map_margin*2,
                           map_margin, map_margin,
                           false);
    region_map->id = REGION_MAP;
    region_map->dx = map_px;
    region_map->dy = map_px;

    TileInit();

#ifdef WIN32TILES
    TileInitWin();
#endif

    if (Options.show_items[0] != 0)
    {
        // temporal size
        region_item = new TileRegionClass(1, 1, TILE_X, TILE_Y);
        region_item->id = REGION_INV1;
    }
    // temporal size
    region_item2 = new TileRegionClass(1, 1, TILE_X, TILE_Y);
    region_item2->id = REGION_INV2;

    region_tile =
        new TileRegionClass(tile_dngn_x, tile_dngn_y,
        TILE_UX_NORMAL, TILE_UY_NORMAL);
    region_tile->id = REGION_DNGN;

#if DEBUG_DIAGNOSTICS
    // one more line for debug GPS
    region_stat = new TextRegionClass(crawl_view.hudsz.x, crawl_view.hudsz.y + 1, 0, 0);
#else
    region_stat = new TextRegionClass(crawl_view.hudsz.x, crawl_view.hudsz.y, 0, 0);
#endif
    region_stat->id = REGION_STAT;
    region_msg = new TextRegionClass(msg_x, msg_y, 0, 0);
    region_msg->id = REGION_MSG;

    if (UseDosChar)
    {
        region_xmap = new TextRegionClass(crt_x, crt_y, 0, 0);
        region_xmap->id = REGION_XMAP;
    }

#ifdef WIN32TILES
    region_crt->init_font(font_name, font_size);
    region_stat->copy_font(region_crt);
    region_msg->copy_font(region_crt);

    if (UseDosChar)
    {
        region_xmap->dos_char = true;
        region_xmap->init_font(dos_font_name, dos_font_size);
    }

#elif defined(USE_X11)
    region_tip = new TextRegionClass(region_stat->mx, 1, 0, 0);
    region_tip->id = REGION_TIP;

    region_crt->init_font(font_name);
    region_stat->init_font(font_name); 
    region_msg->init_font(font_name); 
    region_tip->init_font(font_name);
    if (region_dngn) region_dngn->init_font(font_name);
#endif

    do_layout();

    win_main->create((char*)(CRAWL " " VERSION));

    region_map->init_backbuf();

    region_tile -> init_backbuf();
    region_item2->init_backbuf();
    if (region_item) region_item->init_backbuf();
}

void libgui_shutdown()
{

    if(TileImg) ImgDestroy(TileImg);
    if(PlayerImg) ImgDestroy(PlayerImg);
    if(WallImg) ImgDestroy(WallImg);
    if(TileIsoImg) ImgDestroy(TileIsoImg);

    // do this before delete win_main
    libgui_save_prefs();

    std::vector<RegionClass *>::iterator r;
    for (r = win_main->regions.begin();r != win_main->regions.end();r++)
    {
        RegionClass* pRegion = *r;
        delete (pRegion);
    }

    delete win_main;


    libgui_shutdown_sys();
}

/***  Save, Load, and Edit window prefs ***/
static void libgui_load_prefs()
{
    int i, mode;
    FILE *fp;
    char buf[256];
    char tagbuf[256];

    // set default
    for (mode = 0; mode < PREF_MODE_NUM; mode++)
    {
        for(i=0;i<MAX_PREFS;i++)
        {
            struct prefs *p = &pref_data[i];
            int idx = p->dummy_idx;
            if (p->type == 'I')
                dummy_int[mode][idx] = *(int *)p->ptr;
            else if (p->type == 'S')
                strncpy(dummy_str[mode][idx], (char *)p->ptr, MAX_PREF_CHAR);
        }
    }

    const char *baseTxt = "wininit.txt";
    std::string winTxtString = datafile_path(baseTxt, true, true);
    const char *winTxt = winTxtString.c_str()[0] == 0 ?
        baseTxt : winTxtString.c_str();

    if ( (fp = fopen(winTxt, "r")) != NULL )
    {
        while(!feof(fp))
        {
            fgets(buf, 250, fp);
            i = 0;
            while(buf[i] >= 32 && i< 120) i++;
            buf[i] = 0;

            for(i=0;i<MAX_PREFS;i++)
            {
                struct prefs *p = &pref_data[i];
                for (mode = 0; mode < PREF_MODE_NUM; mode++)
                {
                    sprintf(tagbuf, "%s:%s",
                        pref_mode_name[mode], p->tagname);
                    if(strncmp(buf, tagbuf, strlen(tagbuf)) == 0)
                    {
                        char *dat = &buf[strlen(tagbuf)+1];
                        if (p->type == 'I')
                        {
                            int val = atoi(dat);
                            if (val > p->max) val = p->max;
                            if (val < p->min) val = p->min;
                            dummy_int[mode][p->dummy_idx] = val;
                            if (mode == pref_mode)
                                *(int *)p->ptr = val;
                        }
                        if (p->type == 'S')
                        {
                            strncpy(dummy_str[mode][p->dummy_idx], dat,
                                MAX_PREF_CHAR);
                            if (mode == pref_mode)
                                strncpy((char *)p->ptr, dat, MAX_PREF_CHAR);
                        }
                        break;
                    }// tag match
                } //mode
            }//i
        }// while
    }
}

static void libgui_save_prefs()
{
    int i, mode;
    FILE *fp;

    winox = win_main->ox;
    winoy = win_main->oy;
#ifdef WIN32TILES
    windows_get_winpos(&winox, &winoy);
#endif

    for(i=0;i<MAX_PREFS;i++)
    {
    struct prefs *p = &pref_data[i];
    int idx = p->dummy_idx;
    if (p->type == 'I')
        dummy_int[pref_mode][idx] = *(int *)p->ptr;
    else if (p->type == 'S')
        strncpy(dummy_str[pref_mode][idx], (char *)p->ptr, MAX_PREF_CHAR);
    }

    const char *baseTxt = "wininit.txt";
    std::string winTxtString = datafile_path(baseTxt, false, true);
    const char *winTxt = winTxtString.c_str()[0] == 0 ?
        baseTxt : winTxtString.c_str();

    if ( (fp = fopen(winTxt, "w")) != NULL )
    {
        for (mode = 0; mode < PREF_MODE_NUM; mode++)
        {
            for(i=0;i<MAX_PREFS;i++)
            {
                struct prefs *p = &pref_data[i];
            int idx = p->dummy_idx;
                if (p->type == 'I')
                    fprintf(fp, "%s:%s=%d\n", pref_mode_name[mode],
                            p->tagname, dummy_int[mode][idx]);
                if (p->type == 'S')
                    fprintf(fp, "%s:%s=%s\n", pref_mode_name[mode],
                            p->tagname, dummy_str[mode][idx]);
            }
            fprintf(fp, "\n");
            }
        fclose(fp);
    }
}

static void draw_hgauge(int x, int y, int ofs, int region, int len, int col)
{
    int i;
    gotoxy(x, y, region);
    textcolor(col);
    for (i=0; i < len; i++)
    {
        switch((i+ ofs) % 10)
        {
            case 0: cprintf("%c",'+');break;
            case 4: cprintf("%c",'0' + (1+(i+ofs)/10)%10);break;
            case 5: cprintf("%c",'0');break;
            default: cprintf("%c",'-');
        }
    }
}

static void draw_vgauge(int x, int y, int ofs, int region, int len, int col)
{
    int i;
    textcolor(col);
    for (i=0; i < len; i++)
    {
        gotoxy(x, y+i, region);
        cprintf("%02d", ofs+i);
    }
}

void edit_prefs()
{

    int i;
    int cur_pos = 0;
    cursor_control cs(false);
    bool need_draw_stat = true;
    bool need_draw_msg  = true;

    if (region_tip)
        region_tip->clear();
    region_stat->clear();
    region_msg->clear();
    viewwindow(true, false);

    while(1)
    {
    bool upd_msg = false;
    bool upd_dngn = false;
    bool upd_crt = false;
    bool upd_map = false;
    bool need_resize = false;
    int inc = 0;

    if (need_draw_msg)
    {
        textcolor(LIGHTGREY);
            region_msg->clear();

        textcolor(WHITE);
        gotoxy (4, 4, GOTO_MSG);
        cprintf("j, k, up, down    : Select pref");
        gotoxy (4, 5, GOTO_MSG);
        cprintf("h, l, left, right : Decrease/Increase");
        gotoxy (4, 6, GOTO_MSG);
        cprintf("H, L              : Dec/Inc by 10");
        need_draw_msg = false;
    }
    
    if (need_draw_stat)
    {
            region_stat->clear();
            for(i=0; i<MAX_EDIT_PREFS;i++)
            {
        struct prefs *p = &pref_data[i];
            gotoxy(2, i+2, GOTO_STAT);
            if (i == cur_pos)
            {
                textcolor(0xf0);
                cprintf(">");
        }
        else
        {
                textcolor(LIGHTGREY);
                cprintf(" ");
        }
            if (pref_data[i].type == 'I')
                    cprintf(" %s: %3d  ", p->name, *(int *)p->ptr);
            else
                    cprintf(" %s: %s", p->name, (char *)p->ptr);
        }
        textcolor(LIGHTGREY);

#ifdef WIN32TILES
        gotoxy(4, MAX_EDIT_PREFS+3, GOTO_STAT);
        cprintf("FONT: %s %d",font_name, font_size);
        if (UseDosChar)
        {
            gotoxy(4, MAX_EDIT_PREFS+4, GOTO_STAT);
            cprintf("DOSFONT: %s %d", dos_font_name, dos_font_size);
        }
#else
        gotoxy(4, MAX_EDIT_PREFS+3, GOTO_STAT);
        cprintf("FONT: %s",font_name);
#endif


        int *dat = (int *)pref_data[cur_pos].ptr;
#define WHITEIF(x) (dat == &x)?0xf0:LIGHTGREY

            draw_hgauge(3, 1, 3, GOTO_MSG, msg_x-2, WHITEIF(msg_x));
        clear_to_end_of_line();
            draw_vgauge(1, 1, 1, GOTO_MSG, msg_y, WHITEIF(msg_y));
        need_draw_stat = false;
        }

    int key = getch();

        struct prefs *p = &pref_data[cur_pos];
    int *dat = (int *)p->ptr;

    if (key == 0x1b || key == '\r') break;
    if (key == 'j' || key == CK_DOWN)
    {
        cur_pos++;
        need_draw_stat = true;
    }
    if (key == 'k' || key == CK_UP)
    {
        cur_pos--;
        need_draw_stat = true;
    }
    if (key == CK_LEFT) key = 'h';
    if (key == CK_RIGHT) key = 'l';

    cur_pos = (cur_pos + MAX_EDIT_PREFS) % MAX_EDIT_PREFS;

    switch(key)
    {
        case 'l': inc=1; break;
        case 'L': inc=10; break;
        case 'h': inc=-1; break;
        case 'H': inc=-10; break;
    }

    int crt_x_old = crt_x;
    int crt_y_old = crt_y;
    int map_px_old = map_px;
    int msg_x_old = msg_x;
    int msg_y_old = msg_y;
    int dngn_x_old = dngn_x;
    int dngn_y_old = dngn_y;
        
    if ( (p->type == 'I') && inc != 0)
    {
        if (dat == &dngn_x || dat == &dngn_y )
        {
        if (inc==1) inc=2;
            if (inc==-1) inc=-2;
        }
        int olddat = *dat;
        (*dat)+= inc;

        if (*dat > p->max) *dat = p->max;
        if (*dat < p->min) *dat = p->min;

        if (olddat == *dat) continue;
        need_resize = true;
    }// changed

    if (need_resize)
    {
        need_draw_stat = need_draw_msg = true;

        // crt screen layouts

        // resize msg?
        if (msg_x != msg_x_old || msg_y != msg_y_old)
        {
        upd_msg = true;
        region_msg->resize(msg_x, msg_y);
        }
        // resize crt?
        if (crt_x != crt_x_old || crt_y != crt_y_old)
        {
        upd_crt = true;
            region_crt->resize(crt_x, crt_y);
        }
        // resize map?
        if (map_px != map_px_old)
        {
                upd_map = true;
                region_map->resize( 0, 0, map_px, map_px);
        }

        // resize dngn tile screen?
        if (dngn_x != dngn_x_old || dngn_y != dngn_y_old)
        {
                clrscr();
                upd_dngn = true;
                tile_dngn_x = dngn_x;
                tile_dngn_y = dngn_y;
                region_tile->resize(dngn_x, dngn_y, 0, 0);
        }

        do_layout();
        win_main->resize();
        win_main->clear();

        // Now screens are all black

        if (upd_map)
            region_map->resize_backbuf();
        if (upd_dngn)
        {
            region_tile -> resize_backbuf();
            force_redraw_tile = true;
            TileResizeScreen(dngn_x, dngn_y);
        }
        if (region_item)
            region_item->resize_backbuf();
        if (region_item2)
            region_item2->resize_backbuf();
        force_redraw_inv = true;
        tile_draw_inv(-1, REGION_INV1);

        region_map->force_redraw = true;
        viewwindow(true, true);
        region_tile->redraw();
        region_item->redraw();
    }// need resize
    }//while
    clrscr();
    redraw_screen();
    mpr("Done.");
}

// Tiptext help info
typedef struct tip_info
{
    int sx;
    int ex;
    int sy;
    int ey;
    const char *tiptext;
    const char *tipfilename;
} tip_info;

static int old_tip_idx = -1;

void tip_grid(int gx, int gy, bool do_null = true, int minimap=0)
{
    mesclr();
    const coord_def gc(gx,gy);
    terse_describe_square(gc);
}

int tile_cursor_x = -1;
int tile_cursor_y = -1;
int tile_cursor_flag=0;

void tile_place_cursor(int x, int y, bool display)
{
    if (tile_cursor_x != -1)
    {
        // erase old cursor
        TileDrawCursor(tile_cursor_x, tile_cursor_y, tile_cursor_flag);
    }
    if (!display)
    {
        tile_cursor_x = -1;
        return;
    }

    int new_flag = TILE_FLAG_CURSOR1;
    const coord_def gc = view2grid(coord_def(x+1, y+1));
    if (gc.x < 0 || gc.y < 0 || gc.x >= GXM || gc.y >= GYM)
    {
        // off the dungeon...
        tile_cursor_x = -1;
        return;
    }
    else if (!map_bounds(gc))
    {
        new_flag = TILE_FLAG_CURSOR2;
    }
    else
    {
        unsigned char ch = env.map[gc.x][gc.y].glyph() & 0xff;
        if (ch==0 || ch==' ' || char_is_wall(ch))
            new_flag = TILE_FLAG_CURSOR2;
    }
    tile_cursor_x = x;
    tile_cursor_y = y;
    tile_cursor_flag = TileDrawCursor(x, y, new_flag);
}

int convert_cursor_pos(int mx, int my, int *cx, int *cy)
{
    std::vector <RegionClass *>::iterator r;
    WinClass *w = win_main;
    int id = REGION_NONE;
    int x, y;
    int mx0 = 0;

    for (r = w->regions.begin();r != w->regions.end();r++)
    {
        if (! (*r)->is_active()) continue;
        
        if ( (*r)->mouse_pos(mx, my, cx, cy))
        {
            id = (*r)->id;
            mx0 = (*r)->mx;
            break;
        }
    }

    x = *cx;
    y = *cy;

/********************************************/
    if (id == REGION_TDNGN)
    {
        x--;
        if (!in_viewport_bounds(x+1,y+1)) return REGION_NONE;
    }

    if (id == REGION_MAP)
    {
        x = x - gmap_ox + region_map->x_margin + 1;
        y = y - gmap_oy + region_map->y_margin + 1;
    }

    if (id == REGION_INV1 || id == REGION_INV2)
    {
        x = x + y * mx0;
        y = 0;
    }

    if (id == REGION_DNGN)
    {
        // Out of LOS range
        if (!in_viewport_bounds(x+1,y+1))
        return REGION_NONE;
    }

    *cx = x;
    *cy = y;

    return id;
}

static int handle_mouse_motion(int mouse_x, int mouse_y, bool init)
{
    int cx = -1;
    int cy = -1;
    int mode = 0;

    static int oldcx = -1;
    static int oldcy = -1;
    static int oldmode = -1;
    static int olditem = -1;

    if (init)
    {
        oldcx = -1;
        oldcy = -1;
        oldmode = -1;
        old_tip_idx = -1;
        olditem = -1;
        return 0;
    }

    switch(mouse_mode)
    {
        case MOUSE_MODE_NORMAL:
        case MOUSE_MODE_MORE:
        case MOUSE_MODE_MACRO:
            return 0;
            break;
    }

    if (mouse_x < 0)
    {
        mode = 0;
        oldcx = oldcy = -10;
    }
    else
        mode = convert_cursor_pos(mouse_x, mouse_y, &cx, &cy);
        
    if (oldcx == cx && oldcy == cy && oldmode == mode) return 0;

    // erase old cursor
    if ( oldmode == REGION_DNGN && mode != oldmode)
    {
        oldcx = -10;
        //_setcursortype(0);
    }

    if (olditem != -1)
    {
        oldcx = -10;
        TileMoveInvCursor(-1);
        olditem = -1;
    }

    if ( oldmode == REGION_DNGN && mode != oldmode)
    {
        oldcx = -10;
        tile_place_cursor(0, 0, false);
    }

    if (toggle_telescope && mode != REGION_MAP)
    {
        TileDrawDungeon(NULL);
        toggle_telescope = false;
    }

    bool valid_tip_region = (mode == REGION_MAP || mode == REGION_DNGN
                             || mode == REGION_INV1 || mode == REGION_INV2
                             || mode == REGION_MSG || mode == REGION_STAT);

    if (valid_tip_region && mode != oldmode)
    {
        update_tip_text("");
    }

    if (toggle_telescope && mode == REGION_MAP)
    {
        oldmode = mode;
        oldcx = cx;
        oldcy = cy;
        tip_grid(cx-1, cy-1, 1);
        TileDrawFarDungeon(cx-1, cy-1);
        return 0;
    }

    if (mode == REGION_INV1 || mode == REGION_INV2)
    {
        oldcx = cx;
        oldcy = cy;
        oldmode = mode;
        int ix = TileInvIdx(cx);
        std::string desc;
        if (ix != -1)
        {
            bool display_actions = mode == REGION_INV1;

            if (itemlist_iflag[cx] & TILEI_FLAG_FLOOR)
            {
                if (itemlist_key[cx])
                {
                    desc = itemlist_key[cx];
                    desc +=  " - ";
                }
                desc += mitm[ix].name(DESC_NOCAP_A);
                if (display_actions)
                    desc += EOL "[L-Click] *Pickup(g)";
            }
            else
            {
                desc = you.inv[ix].name(DESC_INVENTORY_EQUIP);

                if (display_actions)
                {
                    int type = you.inv[ix].base_type;
                    desc += EOL;
                    
                    if (type != OBJ_CORPSES
                        || you.species == SP_VAMPIRE
                           && you.inv[ix].sub_type != CORPSE_SKELETON
                           && you.inv[ix].special >= 100)
                    {
                        desc += "[L-Click] ";

                        if (itemlist_iflag[cx] & TILEI_FLAG_EQUIP)
                        {
                            if (you.equip[EQ_WEAPON] == ix
                                && type != OBJ_MISCELLANY
                                && (!item_is_rod(you.inv[ix])
                                    || !item_type_known(you.inv[ix])))
                            {
                                if (type == OBJ_JEWELLERY || type == OBJ_ARMOUR
                                    || type == OBJ_WEAPONS || type == OBJ_STAVES)
                                {
                                    type = OBJ_WEAPONS + 18;
                                }
                            }
                            else
                                type += 18;
                        }

                        switch (type)
                        {
                        // first equipable categories
                        case OBJ_WEAPONS:
                        case OBJ_STAVES:
                        case OBJ_MISCELLANY:
                            desc += "*(w)ield";
                            break;
                        case OBJ_WEAPONS + 18:
                            desc += "unwield";
                            break;
                        case OBJ_MISCELLANY + 18:
                            if (you.inv[ix].sub_type >= MISC_DECK_OF_ESCAPE
                                && you.inv[ix].sub_type <= MISC_DECK_OF_DEFENSE)
                            {
                                desc += "draw a card";
                                break;
                            }
                            // else fall-through
                        case OBJ_STAVES + 18: // rods - other staves handled above
                            desc += "*(E)voke";
                            break;
                        case OBJ_ARMOUR:
                            desc += "*(W)ear";
                            break;
                        case OBJ_ARMOUR + 18:
                            desc += "*(T)ake off";
                            break;
                        case OBJ_JEWELLERY:
                            desc += "*(P)ut on";
                            break;
                        case OBJ_JEWELLERY + 18:
                            desc += "*(R)emove";
                            break;
                        case OBJ_MISSILES:
                            desc += "*(t)hrow";
                            break;
                        case OBJ_WANDS:
                            desc += "*(z)ap";
                            break;
                        case OBJ_BOOKS:
                            if (item_type_known(you.inv[ix])
                                && you.inv[ix].sub_type != BOOK_MANUAL
                                && you.inv[ix].sub_type != BOOK_DESTRUCTION)
                            {
                                desc += "*(M)emorize";
                                break;
                            }
                            // else fall-through
                        case OBJ_SCROLLS:
                            desc += "*(r)ead";
                            break;
                        case OBJ_POTIONS:
                            desc += "*(q)uaff";
                            break;
                        case OBJ_FOOD:
                            desc += "*(e)at";
                            break;
                        case OBJ_CORPSES:
                            if (you.species == SP_VAMPIRE)
                                desc += "drink blood";
                            break;
                        default:
                            desc += "*Use it";
                        }
                    }

                    desc += EOL "[R-Click] Info";
                    desc += EOL "[Shift-L-Click] *(d)rop";
                }
            }
            update_tip_text(desc.c_str());
            itemlist_flag = mode;
            TileMoveInvCursor(cx);
            olditem = cx;
        }
        else
            update_tip_text("");

        return 0;
    }

    if (mouse_mode == MOUSE_MODE_TARGET || 
        mouse_mode == MOUSE_MODE_TARGET_DIR)
    {
        if (mode == REGION_DNGN || mode == REGION_TDNGN)
        {
            oldcx = cx;
            oldcy = cy;
            oldmode = mode;

            gui_set_mouse_view_pos(true, cx+1, cy+1);

            return CK_MOUSE_MOVE;
        }
    }

    gui_set_mouse_view_pos(false, -1, -1);

    if (mode==REGION_TDNGN || mode==REGION_DNGN)
    {
        if (mode == oldmode && oldcx == DCX && oldcy == DCY)
            update_tip_text("");
            
        oldcx = cx;
        oldcy = cy;
        oldmode = mode;

        if(mode==REGION_DNGN)
            tile_place_cursor(cx, cy, true);

        if(mode==REGION_TDNGN)
        {
            gotoxy(cx+2, cy+1, GOTO_DNGN);
        }

        const int gx = view2gridX(cx) + 1;
        const int gy = view2gridY(cy) + 1;

        tip_grid(gx, gy);

        // mouse-over info on player
        if (cx == DCX && cy == DCY)
        {
            std::string desc  = you.your_name;
                        desc += " (";
                        desc += get_species_abbrev(you.species);
                        desc += get_class_abbrev(you.char_class);
                        desc += ")" EOL;

            desc += EOL "[L-Click] *Search 1 turn(s)";

            if (grid_stair_direction( grd[gx][gy] ) != CMD_NO_CMD)
                desc += EOL "[Shift-L-Click] use stairs (</>)";

            // character overview
            desc += EOL "[R-Click] Overview(%)";

            // Religion
            if (you.religion != GOD_NO_GOD)
                desc += EOL "[Shift-R-Click] Religion (^)";

            update_tip_text(desc.c_str());
        }

        return 0;
    }

    if (mode == REGION_MAP && mouse_mode == MOUSE_MODE_COMMAND)
    {
        if (oldmode !=  REGION_MAP)
        {
            update_tip_text("[L-Click] Travel / [R-Click] View");
        }
        else
        {
            tip_grid(cx - 1, cy - 1, false, 1);
        }

        oldmode = mode;
        oldcx = cx;
        oldcy = cy;

        return 0;
    }

    if (mode == REGION_MSG && mouse_mode == MOUSE_MODE_COMMAND)
    {
        if (oldmode != REGION_MSG)
            update_tip_text("[L-Click] Browse message history");
        oldmode = mode;
        oldcx = cx;
        oldcy = cy;
        return 0;
    }
    
    if (mode == REGION_STAT && mouse_mode == MOUSE_MODE_COMMAND)
    {
        if (oldmode != REGION_STAT)
            update_tip_text("[L-Click] Rest/Search for a while");
        oldmode = mode;
        oldcx = cx;
        oldcy = cy;
        return 0;
    }

    return 0;
}
static int handle_mouse_button(int mx, int my, int button,
    bool shift, bool ctrl)
{
    int dir;
    const int dx[9]={-1,0,1, -1,0,1, -1,0,1};
    const int dy[9]={1,1,1,0,0,0,-1,-1,-1};

    const int cmd_n[9]={'b', 'j', 'n', 'h', '.', 'l', 'y', 'k', 'u'};
    const int cmd_s[9]={'B', 'J', 'N', 'H', '5', 'L', 'Y', 'K', 'U'};

    const int cmd_c[9]={
        CONTROL('B'), CONTROL('J'), CONTROL('N'), CONTROL('H'),
        'X', CONTROL('L'), CONTROL('Y'), CONTROL('K'), CONTROL('U'),
    };
    const int cmd_dir[9]={'1','2','3','4','5','6','7','8','9'};

    int trig = CK_MOUSE_B1;
    if (button == 2) trig = CK_MOUSE_B2;
    if (button == 3) trig = CK_MOUSE_B3;
    if (button == 4) trig = CK_MOUSE_B4;
    if (button == 5) trig = CK_MOUSE_B5;
    if (shift) trig |= 512;
    if (ctrl) trig |= 1024;

    switch(mouse_mode)
    {
    case MOUSE_MODE_NORMAL:
        return trig;
        break;
    case MOUSE_MODE_MORE:
        return '\r';
        break;
    }

    int cx,cy;
    int mode;
    static int oldcx = -1;
    static int oldcy = -1;

    static bool enable_wheel = true;
    static int  old_button = 0;
    static int  old_hp = 0;

    mode = convert_cursor_pos(mx, my, &cx, &cy);

    // prevent accidental wheel slip and subsequent char death
    if (mouse_mode==MOUSE_MODE_COMMAND && (button == 4 || button == 5)
        && button == old_button && oldcx == cx && oldcy == cy)
    {
        if(!enable_wheel) return 0;
        if (you.hp < old_hp)
        {
            mpr("Wheel move aborted (rotate opposite to resume)");
            enable_wheel = false;
            return 0;
        }
    }
    old_hp = you.hp;
    enable_wheel = true;
    old_button = button;
    oldcx = cx;
    oldcy = cy;

    if (toggle_telescope)
    {
        // quit telescope mode
        TileDrawDungeon(NULL);
        toggle_telescope = false;
    }

    // item clicked
    if (mode == REGION_INV1 || mode == REGION_INV2)
    {
        int ix = TileInvIdx(cx);

        if (ix != -1)
        {
            if (button == 2)
            {
                // describe item
                if (itemlist_iflag[cx] & TILEI_FLAG_FLOOR)
                    gui_set_mouse_inv(-ix, INV_VIEW);
                else
                    gui_set_mouse_inv(ix, INV_VIEW);
                TileMoveInvCursor(-1);
                return CK_MOUSE_B2ITEM;
            }
            else if(button == 1)
            {
                // Floor item
                if (itemlist_iflag[cx] & TILEI_FLAG_FLOOR)
                {
                    // try pick up one item
                    if (button != 1) return 0;
                    gui_set_mouse_inv(ix, INV_PICKUP);
                    return CK_MOUSE_B1ITEM;
                }
                // use item
                if (shift)
                    gui_set_mouse_inv(ix, INV_DROP);
                else
                    gui_set_mouse_inv(ix, INV_USE);
                return CK_MOUSE_B1ITEM;
            }
        }
        return trig;
    }

    if (mode == REGION_MSG && mouse_mode == MOUSE_MODE_COMMAND)
    {
        return CONTROL('P');
    }

    if (mode == REGION_STAT && mouse_mode == MOUSE_MODE_COMMAND)
    {
        return '5';
    }

    if((mouse_mode==MOUSE_MODE_COMMAND || mouse_mode == MOUSE_MODE_MACRO) &&
        (mode == REGION_DNGN || mode == REGION_TDNGN))
    {
        if (button == 1 && cx == DCX && cy == DCY)
        {
            // spend one turn resting/searching
            if (!shift)
                return 's';

            // else attempt to use stairs on square
            const int gx = view2gridX(cx) + 1;
            const int gy = view2gridY(cy) + 1;
            switch (grid_stair_direction( grd[gx][gy] ))
            {
                case CMD_GO_DOWNSTAIRS:
                    return ('>');
                case CMD_GO_UPSTAIRS:
                    return ('<');
                default:
                    break;
            }
        }

        if (button == 2)
        {
            // describe yourself
            if (cx == DCX && cy == DCY)
            {
                if (!shift)
                    return '%'; // character overview

                if (you.religion != GOD_NO_GOD)
                    return '^'; // religion screen
            }
            
            // trigger
            if (mouse_mode == MOUSE_MODE_MACRO)
                return trig;

            // Right Click: try to describe grid
            // otherwise return trigger key
            if (!in_los_bounds(cx+1,cy+1))
                return CK_MOUSE_B2;

            const int gx = view2gridX(cx) + 1;
            const int gy = view2gridY(cy) + 1;
            full_describe_square(coord_def(gx,gy));
            return CK_MOUSE_DONE;
        }

        // button = 1 or 4, 5
        // first, check if 3x3 grid around @ is clicked.
        // if so, return equivalent numpad key
        int adir = -1;
        for(dir=0;dir<9;dir++)
        {
            if( DCX+dx[dir] == cx && DCY+dy[dir]==cy)
            {
                adir = dir;
                break;
            }
        }

        if (adir != -1)
        {
            if (shift) 
                return cmd_s[adir];
            else if (ctrl) 
                return cmd_c[adir];
            else return cmd_n[dir];
        }
        if (button != 1) return trig;

        // otherwise travel to that grid
        const coord_def gc = view2grid(coord_def(cx+1, cy+1));
        if (!map_bounds(gc)) return 0;

        // Activate travel
        start_travel(gc.x, gc.y);
        return CK_MOUSE_DONE;
    }

    if(mouse_mode==MOUSE_MODE_COMMAND && mode == REGION_MAP)
    {
        // begin telescope mode
        if (button == 2)
        {
            toggle_telescope = true;
            StoreDungeonView(NULL);
            TileDrawFarDungeon(cx-1,cy-1);
            return 0;
        }

        if (button != 1) return trig;

        // L-click: try to travel to the grid
        const coord_def gc(cx-1, cy-1);
        if (!map_bounds(gc)) return 0;

        // Activate travel
        start_travel(gc.x, gc.y);
        return CK_MOUSE_DONE;
    }

    // target selection
    if((mouse_mode==MOUSE_MODE_TARGET || mouse_mode==MOUSE_MODE_TARGET_DIR)
        && button == 1 && (mode == REGION_DNGN || mode == REGION_TDNGN))
    {
        gui_set_mouse_view_pos(true, cx+1, cy+1);
        return CK_MOUSE_CLICK;
    }

    gui_set_mouse_view_pos(false, 0, 0);

    if(mouse_mode==MOUSE_MODE_TARGET_DIR && button == 1 &&
       (mode == REGION_DNGN || mode == REGION_TDNGN))
    {
        if (cx < DCX-1 || cy < DCY-1 || cx > DCX+1 || cy > DCY+1) return 0;
        for(dir=0;dir<9;dir++)
        {
            if( DCX+dx[dir] == cx && DCY+dy[dir]==cy)
            {
                return cmd_dir[dir];
            }
        }
        return 0;
    }

    return trig;
}

static int handle_mouse_unbutton(int mx, int my, int button)
{
    if (toggle_telescope)
    {
        TileDrawDungeon(NULL);
    }

    toggle_telescope = false;
    return 0;
}

int getch_ck()
{
    int etype = 0;
    int x1,y1,x2,y2;
    int key;
    bool sh, ct;
    int k;

    while(1)
    {
    k = 0;
    GetNextEvent(&etype, &key, &sh, &ct, &x1, &y1, &x2, &y2);
    switch(etype)
    {
        case EV_BUTTON:
            k = handle_mouse_button(x1, y1, key, sh, ct);
        break;

        case EV_MOVE:
            k = handle_mouse_motion(x1, y1, false);
        break;

        case EV_UNBUTTON:
            k = handle_mouse_unbutton(x1, y1, key);
        break;

        case EV_KEYIN:
            k = key;
        break;

        default:
        break;
    } // switch

    if (k != 0) break;
    }/*while*/

    return k;
}

int getch()
{
    static int ck_tr[] =
    {
        'k', 'j', 'h', 'l', '.', 'y', 'b', '.', 'u', 'n',
        'K', 'J', 'H', 'L', '5', 'Y', 'B', '5', 'U', 'N',
        11, 10, 8, 12, '0', 25, 2, 'C', 21, 14
    };

    int keyin = getch_ck();
    if (keyin >= CK_UP && keyin <= CK_CTRL_PGDN)
    return ck_tr[ keyin - CK_UP ];
    if (keyin == CK_DELETE)
    return '.';
    return keyin;
}

int m_getch()
{
    return getch();
}

void set_mouse_enabled(bool enabled)
{
    crawl_state.mouse_enabled = enabled;
}

void mouse_set_mode(int mode)
{
    mouse_mode = mode;
    // init cursor etc
    handle_mouse_motion(0, 0, true);
}

int mouse_get_mode()
{
    return mouse_mode;
}

void gui_init_view_params(coord_def &termsz, coord_def &viewsz, 
    coord_def &msgp, coord_def &msgsz, coord_def &hudp, coord_def &hudsz)
{
    // TODO enne - set these other params too?
    msgsz.x = msg_x;
    msgsz.y = msg_y;
}

void lock_region(int r)
{
    if (r >= 0 && r < NUM_REGIONS)
        region_lock[r] = true;
}

void unlock_region(int r)
{
    if (r >= 0 && r < NUM_REGIONS)
        region_lock[r] = false;
}

/*
 *  Wrapper
 */

void clrscr()
{
    win_main->clear();
    TextRegionClass::cursor_region = NULL;

    // clear Text regions
    if (!region_lock[REGION_CRT])
        region_crt->clear();
    if (region_msg && !region_lock[REGION_MSG])
        region_msg->clear();
    if (region_stat && !region_lock[REGION_STAT])
        region_stat->clear();
    if (region_dngn && !region_lock[REGION_TDNGN])
        region_dngn->clear();
    if (region_tip && !region_lock[REGION_TIP])
        region_tip->clear();

    // Hack: Do not erase the backbuffer. Instead just hide it.
    if (region_map)
    {
        if(region_lock[REGION_MAP]) 
            region_map->redraw();
        else
            region_map->flag = false;
    }
    if (region_item)
    {
        if (region_lock[REGION_INV1])
            region_item->redraw();
        else
            region_item->flag = false;
    }
    if (region_item2)
    {
        
        if (region_lock[REGION_INV2])
            region_item2->redraw();
        else
        {
            TileClearInv(REGION_INV2);
            region_item2->flag = false;
        }
    }

    gotoxy(1, 1);
}

void putch(unsigned char chr)
{
    // object's method
    TextRegionClass::text_mode->putch(chr);
}

void putwch(unsigned chr)
{
    // No unicode support.
    putch(static_cast<unsigned char>(chr));
}

void writeWChar(unsigned char *ch)
{
    // object's method
    TextRegionClass::text_mode->writeWChar(ch);
}

void clear_to_end_of_line()
{
    // object's method
    TextRegionClass::text_mode->clear_to_end_of_line();
}

void clear_to_end_of_screen()
{
    // object's method
    TextRegionClass::text_mode->clear_to_end_of_screen();
}

void get_input_line_gui(char *const buff, int len)
{
    int y, x;
    int k = 0;
    int prev = 0;
    int done = 0;
    int kin;
    TextRegionClass *r = TextRegionClass::text_mode;

    static char last[128];
    static int lastk = 0;

    if(!r->flag)return;

    /* Locate the cursor */
    x = wherex();
    y = wherey();

    /* Paranoia -- check len */
    if (len < 1) len = 1;

    /* Restrict the length */
    if (x + len > r->mx) len = r->mx - x;
    if (len > 40) len = 40;

    /* Paranoia -- Clip the default entry */
    buff[len] = '\0';
    buff[0] = '\0';

    r->gotoxy(x, y);
    putch('_');

    /* Process input */
    while (!done)
    {
        /* Get a key */
        kin = getch_ck();

        /* Analyze the key */
        switch (kin)
        {
            case 0x1B:
                k = 0;
                done = 1;
                break;

            case '\n':
            case '\r':
                k = strlen(buff);
                done = 1;
        lastk = k;
        strncpy(last, buff, k);
                break;

        case CK_UP: // history
        if (lastk != 0)
        {
            k = lastk;
            strncpy(buff, last, k);
        }
        break;

            case 0x7F:
            case '\010':
                k = prev;
                break;

            // Escape conversion. (for ^H, etc)
            case CONTROL('V'):
                kin = getch();
                // fallthrough

            default:
            {
                if (k < len &&
                     (
                      isprint(kin)
                      || (kin >=  CONTROL('A') && kin <= CONTROL('Z'))
                      || (kin >= 0x80 && kin <=0xff)
                     )
                   )
                     buff[k++] = kin;
                break;
            }

        }
        /* Terminate */
        buff[k] = '\0';

        /* Update the entry */
        r->gotoxy(x, y);
        int i;

        //addstr(buff);
        for(i=0;i<k;i++)
        {
            prev = i;
            int c = (unsigned char)buff[i];
            if (c>=0x80)
            {
                if (buff[i+1]==0) break;
                writeWChar((unsigned char *)&buff[i]);
                i++;
            }
            else
            if (c >= CONTROL('A') && c<= CONTROL('Z'))
            {
                putch('^');
                putch(c + 'A' - 1);
            }
            else
                putch(c);
        }
        r->addstr((char *)"_             ");
    r->gotoxy(x+k, y);
    }/* while */
}

void cprintf(const char *format,...)
{
    char buffer[2048];          // One full screen if no control seq...
    va_list argp;
    va_start(argp, format);
    vsprintf(buffer, format, argp);
    va_end(argp);
    // object's method
    TextRegionClass::text_mode->addstr(buffer);
    TextRegionClass::text_mode->make_active();
}

void textcolor(int color)
{
    TextRegionClass::textcolor(color);
}

void textbackground(int bg)
{
    TextRegionClass::textbackground(bg);
}

void gotoxy(int x, int y, int region)
{
    if (region == GOTO_LAST)
    {
        // nothing
    }
    else if (region == GOTO_CRT)
        TextRegionClass::text_mode = region_crt;
    else if (region == GOTO_MSG)
        TextRegionClass::text_mode = region_msg;
    else if (region == GOTO_STAT)
        TextRegionClass::text_mode = region_stat;

    TextRegionClass::text_mode->flag = true;
    TextRegionClass::gotoxy(x, y);
}

void _setcursortype(int curstype)
{
    TextRegionClass::_setcursortype(curstype);
}

void enable_smart_cursor(bool cursor)
{
    gui_smart_cursor = cursor;
}

bool is_smart_cursor_enabled()
{
    return (gui_smart_cursor);
}

void set_cursor_enabled(bool enabled)
{
    if (gui_smart_cursor) return;
    if (enabled) TextRegionClass::_setcursortype(1);
    else TextRegionClass::_setcursortype(0);
}

bool is_cursor_enabled()
{
    if (TextRegionClass::cursor_flag) return true;
    return false;
}

int wherex()
{
    return TextRegionClass::wherex();
}

int wherey()
{
    return TextRegionClass::wherey();
}

int get_number_of_lines()
{
#ifdef USE_X11
    // Lie about the number of lines so that we can reserve the
    // bottom one for tooltips...
    return region_crt->my - 1;
#else
    return region_crt->my;
#endif
}

int get_number_of_cols()
{
    return std::min(region_crt->mx, region_msg->mx);
}

void message_out(int which_line, int colour, const char *s, int firstcol,
    bool newline)
{
    if (!firstcol)
        firstcol = Options.delay_message_clear ? 2 : 1;

    gotoxy(firstcol, which_line + 1, GOTO_MSG);
    textcolor(colour);

    cprintf("%s", s);

    if (newline && which_line == crawl_view.msgsz.y - 1)
        region_msg->scroll();
}

void clear_message_window()
{
    region_msg->clear();
}

void put_colour_ch(int colour, unsigned ch)
{
    textcolor(colour);
    putwch(ch);
}

void puttext(int sx, int sy, int ex, int ey, unsigned char *buf, bool mono,
    int where)
{
    TextRegionClass *r = (where == 1) ?region_crt:region_dngn;

    if (where==1 && UseDosChar) r=region_xmap;

    int xx, yy;
    unsigned char *ptr = buf;
    //gotoxy(1, 1, GOTO_CRT);
    for(yy= sy-1; yy<= ey-1; yy++)
    {
        unsigned char *c = &(r->cbuf[yy*(r->mx)+sx-1]);
        unsigned char *a = &(r->abuf[yy*(r->mx)+sx-1]);
        for(xx= sx-1; xx<= ex-1; xx++)
        {
            *c = *ptr;
            if (*c==0) *c=32;
            ptr++;

            if (mono)
            {
                *a = WHITE;
            }
            else
            {
                *a = *ptr;
                ptr++;
            }
            c++;
            a++;
        }
    }
    r->make_active();
    r->redraw(sx-1, sy-1, ex-1, ey-1);
}

void ViewTextFile(const char *name)
{
    FILE *fp = fopen(name, "r");
    int max = get_number_of_lines();

    // Hack
#define MAXTEXTLINES 100

#define DELIMITER_END "-------------------------------------------------------------------------------"
#define DELIMITER_MORE "...(more)...                                                                   "
    unsigned char buf[80*MAXTEXTLINES], buf2[84];
    int nlines = 0;
    int cline = 0;
    int i;

    if (!fp)
    {
        mpr("Couldn't open file.");
        return;
    }

    for (i=0; i<80*MAXTEXTLINES; i++) buf[i] = ' ';

    while(nlines<MAXTEXTLINES)
    {
        fgets((char *)buf2, 82, fp);
        if (feof(fp)) break;
        i = 0;
        while(i<79 && buf2[i] !=13 && buf2[i] != 10) i++;
        memcpy(&buf[nlines*80], buf2, i);
        nlines++;
    }
    fclose(fp);
    clrscr();

    while(1)
    {
        gotoxy(1, 1);
        if (cline == 0)
            cprintf(DELIMITER_END);
        else
            cprintf(DELIMITER_MORE);

        puttext(1, 2, 80, max, &buf[cline*80], true, 1);

        gotoxy(1, max);

        if (cline + max-2 >= nlines)
            cprintf(DELIMITER_END);
        else
            cprintf(DELIMITER_MORE);

        gotoxy(14, max);
        cprintf("[j/k/+/-/SPACE/b: scroll   q/ESC/RETURN: exit]");
        mouse_set_mode(MOUSE_MODE_MORE);
        int key = getch();
        mouse_set_mode(MOUSE_MODE_NORMAL);
        if (key == 'q' || key == ESCAPE || key =='\r') break;
        if (key == '-' || key == 'b') cline -= max-2;
        if (key == 'k') cline --;
        if (key == '+' || key == ' ') cline += max-2;
        if (key == 'j') cline ++;
        if (key == CK_MOUSE_B4) cline--;
        if (key == CK_MOUSE_B5) cline++;

        if (cline + max-2 > nlines) cline = nlines-max + 2;
        if (cline < 0) cline = 0;
    }
}

void window(int x1, int y1, int x2, int y2)
{
}
