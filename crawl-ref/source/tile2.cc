/*
 *  File:       tile2.cc
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 *
 *  Modified for Crawl Reference by $Author: j-p-e-g $ on $Date: 2008-03-07 $
 */
 
#ifdef USE_TILE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AppHdr.h"
#include "branch.h"
#include "describe.h"
#include "direct.h"
#include "dungeon.h"
#include "files.h"
#include "ghost.h"
#include "guic.h"
#include "itemprop.h"
#include "it_use2.h"
#include "place.h"
#include "stuff.h"
#include "tiles.h"
#include "tilecount-w2d.h"
#include "tiledef-p.h"
#include "transfor.h"

// normal tile count + iso tile count
#define TILE_TOTAL2 (TILE_TOTAL)
// normal tile count
#define TILE_NORMAL TILE_TOTAL

extern void TileDrawDungeonAux();

// Raw tile images
extern img_type TileImg;
extern img_type TileIsoImg;
extern img_type PlayerImg;
extern img_type WallImg;
extern img_type ScrBufImg;

extern WinClass *win_main;
// Regions
extern TileRegionClass *region_tile;
extern TextRegionClass *region_crt;
extern TextRegionClass *region_stat;
extern TextRegionClass *region_tip;
extern TextRegionClass *region_msg;

extern TileRegionClass *region_item;
extern TileRegionClass *region_item2;

#define ScrBufImg (region_tile->backbuf)

//Internal
static img_type DollCacheImg;

static void tile_draw_grid(int kind, int xx, int yy);
static void clear_tcache();
static void init_tcache();
static void register_tile_mask(int tile, int region, int *cp,
    char *ms, bool smalltile = false);
static void tcache_compose_normal(int ix, int *fg, int *bg);

static void mcache_init();

//Internal variables

static bool force_redraw_tile = false;
static bool force_redraw_inv = false;

void tile_set_force_redraw_tiles(bool redraw)
{
    force_redraw_tile = redraw;
}

void tile_set_force_redraw_inv(bool redraw)
{
    force_redraw_inv = redraw;
}

static unsigned int t1buf[TILE_DAT_XMAX+2][TILE_DAT_YMAX+2],
                          t2buf[TILE_DAT_XMAX+2][TILE_DAT_YMAX+2];
static unsigned int tb_bk[TILE_DAT_YMAX*TILE_DAT_XMAX*2];

#define MAX_ITEMLIST 200
int itemlist[MAX_ITEMLIST];
int itemlist_num[MAX_ITEMLIST];
int itemlist_idx[MAX_ITEMLIST];
char itemlist_key[MAX_ITEMLIST];
int itemlist_iflag[MAX_ITEMLIST];
int itemlist_flag = -1;
int itemlist_n=0;

static int wall_flavors = 0;
static int floor_flavors = 0;
static int special_flavors = 0;
static int wall_tile_idx = 0;
static int floor_tile_idx = 0;
static int special_tile_idx = 0;

int get_num_wall_flavors()
{
    return wall_flavors;
}

int get_num_floor_flavors()
{
    return floor_flavors;
}

int get_num_floor_special_flavors()
{
    return special_flavors;
}

int get_wall_tile_idx()
{
    return wall_tile_idx;
}

int get_floor_tile_idx()
{
    return floor_tile_idx;
}

int get_floor_special_tile_idx()
{
    return special_tile_idx;
}

/******** Cache buffer for transparency operation *****/
static int tcache_kind;

#define TCACHE_KIND_NORMAL 1
#define TCACHE0_NORMAL 0

static int last_cursor = -1;

//Internal cache Image buffer
static img_type *tcache_image;

//Start of a pointer string
static int *tcache_head;

typedef struct tile_cache
{
    unsigned int id[2];
    int idx;
    tile_cache *next;
    tile_cache *prev;
} tile_cache;

// number of tile grids
static int tile_xmax;
static int tile_ymax;

static int max_tcache;
static int tc_hash;
// [kind * tc_hash][max_tcache]
static tile_cache **tcache;
// [kind][x*y]
static int **screen_tcach_idx;

const int tcache_wx_normal[TCACHE_KIND_NORMAL] = {TILE_X};
const int tcache_wy_normal[TCACHE_KIND_NORMAL] = {TILE_Y};
const int tcache_ox_normal[TCACHE_KIND_NORMAL] = {0};
const int tcache_oy_normal[TCACHE_KIND_NORMAL] = {0};
const int tcache_nlayer_normal[TCACHE_KIND_NORMAL] = {1};

#define TREGION_0_NORMAL 0
const int region_sx_normal[1]={0};
const int region_sy_normal[1]={0};
const int region_wx_normal[1]={TILE_X};
const int region_wy_normal[1]={TILE_Y};

// ISO mode sink mask
static char *sink_mask;

/********* Image manipulation subroutines ****************/
img_type ImgLoadFileSimple(const char *name)
{
    char fname[512];
#ifdef USE_X11
    sprintf(fname,"tiles/%s.png", name);
    std::string path = datafile_path(fname, true, true);
    return ImgLoadFile(path.c_str());
#endif
#ifdef WIN32TILES
    sprintf(fname,"tiles/%s.bmp", name);
    std::string path = datafile_path(fname, true, true);
    return ImgLoadFile(path.c_str());
#endif
}

// TileImg macro
void ImgCopyFromTileImg(int idx, img_type dest, int dx, int dy, int copy,
                        char *mask = NULL, bool hilite = false)
{
    int sx = (idx % TILE_PER_ROW)*TILE_X;
    int sy = (idx / TILE_PER_ROW)*TILE_Y;
    if (hilite)
    {
        if (mask != NULL)
            ImgCopyMaskedH(TileImg, sx, sy, TILE_X, TILE_Y,
                           dest, dx, dy, mask);
        else
            ImgCopyH(TileImg, sx, sy, TILE_X, TILE_Y, dest, dx, dy, copy);
    }
    else
    {
        if (mask != NULL)
            ImgCopyMasked(TileImg, sx, sy, TILE_X, TILE_Y,
                           dest, dx, dy, mask);
        else
            ImgCopy(TileImg, sx, sy, TILE_X, TILE_Y, dest, dx, dy, copy);
    }
}

void ImgCopyToTileImg(int idx, img_type src, int sx, int sy, int copy,
                        char *mask = NULL, bool hilite = false)
{
    int dx = (idx % TILE_PER_ROW)*TILE_X;
    int dy = (idx / TILE_PER_ROW)*TILE_Y;
    if (hilite)
    {
        if (mask != NULL)
            ImgCopyMaskedH(src, sx, sy, TILE_X, TILE_Y,
                           TileImg, dx, dy, mask);
        else
            ImgCopyH(src, sx, sy, TILE_X, TILE_Y, TileImg, dx, dy, copy);
    }
    else
    {
        if (mask != NULL)
            ImgCopyMasked(src, sx, sy, TILE_X, TILE_Y,
                           TileImg, dx, dy, mask);
        else
            ImgCopy(src, sx, sy, TILE_X, TILE_Y, TileImg, dx, dy, copy);
    }
}

void TileInit()
{
    int x, y, k;
    textcolor(WHITE);

    TileImg = ImgLoadFileSimple("tile");
    PlayerImg = ImgLoadFileSimple("player");
    WallImg = ImgLoadFileSimple("wall2d");


    if (!TileImg)
    {
    cprintf("Main tile not initialized\n");
    getch();
    end(-1);
    }

    ImgSetTransparentPix(TileImg);

    if (ImgWidth(TileImg)!= TILE_X * TILE_PER_ROW ||
    ImgHeight(TileImg) < TILE_Y*( (TILE_TOTAL + TILE_PER_ROW -1)/TILE_PER_ROW))
    {
        cprintf("Main tile size invalid\n");
        getch();
        end(-1);
    }

    if (!PlayerImg)
    {
        cprintf("Player tile not initialized\n");
        getch();
        end(-1);
    }

    if (ImgWidth(PlayerImg)!= TILE_X * TILEP_PER_ROW)
    {
        cprintf("Player tile size invalid\n");
        getch();
        end(-1);
    }

    if (!WallImg)
    {
        cprintf("wall2d tile not initialized\n");
        getch();
        end(-1);
    }

    tcache_kind = TCACHE_KIND_NORMAL;
    tc_hash = 1;
    tile_xmax = tile_dngn_x;
    tile_ymax = tile_dngn_y;

    max_tcache = 4*tile_xmax*tile_ymax;

    screen_tcach_idx = (int **)malloc(sizeof(int *) * tcache_kind);

    tcache = (tile_cache **)malloc(sizeof(tile_cache *)*tcache_kind);

    tcache_head = (int *)malloc(sizeof(int)*tcache_kind*tc_hash);

    for(k=0;k<tcache_kind;k++)
    {
        screen_tcach_idx[k]= (int *)malloc(sizeof(int)* tile_xmax * tile_ymax);
        tcache[k] = (tile_cache *)malloc(sizeof(tile_cache)*max_tcache);
        for(x=0;x<tile_xmax * tile_ymax;x++)
        screen_tcach_idx[k][x] = -1;
    }

    sink_mask = (char *)malloc(TILE_X*TILE_Y);

    init_tcache();

    DollCacheImg = ImgCreateSimple(TILE_X, TILE_Y);

    for(x=0;x<TILE_DAT_XMAX+2;x++){
    for(y=0;y<TILE_DAT_YMAX+2;y++){
        t1buf[x][y]=0;
        t2buf[x][y]=TILE_DNGN_UNSEEN|TILE_FLAG_UNSEEN;
    }}

    force_redraw_tile = false;

    mcache_init();

    for(x=0; x<MAX_ITEMLIST;x++)
    {
         itemlist[x]=itemlist_num[x]=itemlist_key[x]=itemlist_idx[x]=0;
    }
}

void TileResizeScreen(int x0, int y0)
{
    int k, x;
    tile_xmax = x0;
    tile_ymax = y0;
    max_tcache = 4*tile_xmax*tile_ymax;

    for(k=0;k<tcache_kind;k++)
    {
        free(screen_tcach_idx[k]);
        screen_tcach_idx[k]= (int *)malloc(sizeof(int)* tile_xmax * tile_ymax);

        free(tcache[k]);
        tcache[k] = (tile_cache *)malloc(sizeof(tile_cache)*max_tcache);

        for(x=0;x<tile_xmax * tile_ymax;x++)
        {
            screen_tcach_idx[k][x] = -1;
        }
    }
    clear_tcache();
    crawl_view.viewsz.x = tile_xmax;
    crawl_view.viewsz.y = tile_ymax;
    crawl_view.vbuf.size(crawl_view.viewsz);
}

void clear_tcache()
{
    for(int k = 0; k < tcache_kind; k++)
    {
        tile_cache *tc =  tcache[k];

        // Decompose pointer string tcache[k] into tc_hash segments
        for (int h = 0; h < tc_hash; h++)
        {
            int i_start = (max_tcache*h)/tc_hash;
            int i_end   = (max_tcache*(h+1))/tc_hash -1;
            tcache_head[k*tc_hash + h] = i_start;

            for(int i = i_start; i <= i_end; i++)
            {
                tc[i].id[1] = tc[i].id[0] = 0;
                tc[i].idx = i;
                if (i == i_start)
                    tc[i].prev =  NULL;
                else
                    tc[i].prev = &tc[i-1];
                if (i == i_end)
                    tc[i].next = NULL;
                else
                    tc[i].next = &tc[i+1];
            }
        }
    }
}

void init_tcache(){
    int k;

    clear_tcache();

    tcache_image = (img_type *)malloc(sizeof(img_type)*tcache_kind);

    for(k=0;k<tcache_kind;k++)
    {
        int wx = tcache_wx_normal[k];
        int wy = tcache_wy_normal[k];
        tcache_image[k] = ImgCreateSimple(wx, max_tcache*wy);
    }

}

// Move a cache to the top of pointer string
// to shorten the search time
void lift_tcache(int ix, int kind, int hash){
    int kind_n_hash = kind * tc_hash + hash;
    int head_old=tcache_head[kind_n_hash];
    tile_cache *tc = tcache[kind];
    tile_cache *p  = tc[ix].prev;
    tile_cache *n  = tc[ix].next;

    ASSERT(ix<max_tcache);
    ASSERT(head_old<max_tcache);
    ASSERT(kind<tcache_kind);
    ASSERT(hash<tc_hash);

    if(ix == head_old) return;
    if(p!=NULL) p->next = n;
    if(n!=NULL) n->prev = p;

    tcache_head[kind_n_hash] = ix;
    tc[head_old].prev = &tc[ix];
    tc[ix].next       = &tc[head_old];
}

// Find cached image of fg+bg
// If not found, compose and cache it
static int tcache_find_id_normal(int kind, int *fg, int *bg, int *is_new)
{
    int hash = 0; // Don't use hash
    int kind_n_hash = kind * tc_hash + hash;

    tile_cache *tc = tcache[kind];
    tile_cache *tc0 = &tc[tcache_head[kind_n_hash]];

    *is_new=0;

    while(1){
        tile_cache *next = tc0->next;

        if ((int)tc0->id[0] == fg[0] && (int)tc0->id[1] == bg[0])
            break;

        if (next == NULL)
        {
            //end of used cache
            *is_new = 1;
            tcache_compose_normal(tc0->idx, fg, bg);
            tc0->id[0] = fg[0];
            tc0->id[1] = bg[0];
            break;
        }
        tc0 = next;
    }
    lift_tcache(tc0->idx, kind, hash);

    return tc0->idx;
}


/*** overlay a tile onto an exsisting image with transpalency operation */
void tcache_overlay(img_type img, int idx,
    int tile, int region, int *copy, char *mask, unsigned int shift_left = 0)
{

    int x0, y0;
    int sx = region_sx_normal[region] + shift_left;
    int sy = region_sy_normal[region];
    int wx = region_wx_normal[region] - shift_left;
    int wy = region_wy_normal[region];
    int ox = 0;
    int oy = 0;
    img_type src = TileImg;
    int uy = wy;

    tile &= TILE_FLAG_MASK;

    x0 = (tile % TILE_PER_ROW)*TILE_X;
    y0 = (tile / TILE_PER_ROW)*TILE_Y;

    if (mask != NULL)
    {
        if (*copy ==2)
        {
            ImgCopyMaskedH(src, x0 + sx, y0 + sy, wx, wy,
                img, ox, oy + idx*uy, mask);
        }
        else
        {
            ImgCopyMasked(src, x0 + sx, y0 + sy, wx, wy,
                img, ox, oy + idx*uy, mask);
        }
    }
    // Hack: hilite rim color
    else if (*copy ==2)
    {
        ImgCopyH(src, x0 + sx, y0 + sy, wx, wy,
            img, ox, oy + idx*uy, *copy);
    }
    else
    {
        ImgCopy(src, x0 + sx, y0 + sy, wx, wy,
            img, ox, oy + idx*uy, *copy);
    }
    *copy = 0;
}

void tcache_overlay_player(img_type img, int dx, int dy,
                        int part, int idx, int ymax, int *copy)
{
    int xs, ys;
    int tidx = tilep_parts_start[part];
    int nx = tilep_parts_nx[part];
    int ny = tilep_parts_ny[part];
    int ox = tilep_parts_ox[part];
    int oy = tilep_parts_oy[part];
    int wx = TILE_X/nx;
    int wy = TILE_Y/ny;

    if(!idx)return;
    idx--;

    tidx += idx/(nx*ny);

    if (oy+wy > ymax) wy -= oy + wy - ymax;
    if (wy<=0) return;

    xs = (tidx % TILEP_PER_ROW)*TILE_X;
    ys = (tidx / TILEP_PER_ROW)*TILE_Y;

    xs += (idx % nx)*(TILE_X/nx);
    ys += ((idx/nx) % ny)*(TILE_Y/ny);

    ImgCopy(PlayerImg, xs, ys, wx, wy,
                  img, dx+ox, dy+oy, *copy);

    *copy = 0;
}

/** overlay a tile onto an exsisting image with transpalency operation */
void register_tile_mask(int tile, int region, int *copy,
    char *mask, bool smalltile)
{
    int x0, y0, x, y;
    int sx= region_sx_normal[region];
    int sy= region_sy_normal[region];
    int wx= region_wx_normal[region];
    int wy= region_wy_normal[region];
    int ox=0;
    int oy=0;
    img_type src = TileImg;
    int ux=wx;
    int uy=wy;

    tile &= TILE_FLAG_MASK;

    x0 = (tile % TILE_PER_ROW)*TILE_X;
    y0 = (tile / TILE_PER_ROW)*TILE_Y;

    if (*copy!=0) memset(mask, 0, ux*uy);

    if (*copy == 2)
    {
        if (smalltile)
        {
            ux = wx;
        }
        for(x=0;x<wx;x++){
        for(y=0;y<wy;y++){
            if(! ImgIsTransparentAt(src, x0+sx+x, y0+sy+y))
              mask[(y+oy)*ux+(x+ox)]=1;
        }}
    }
    else
    {
        for(x=0;x<wx;x+=2){
        for(y=0;y<wy;y+=2){
            if(! ImgIsTransparentAt(src, x0+sx+x, y0+sy+y))
          mask[(y+oy)*ux+(x+ox)]=1;
        }}
    }
    *copy = 0;

}

void tile_draw_grid(int kind, int xx, int yy){
    int fg[4],bg[4],ix, ix_old, is_new;

    if (xx < 0 || yy < 0 || xx >= tile_xmax || yy>= tile_ymax) return;
    fg[0] = t1buf[xx+1][yy+1];
    bg[0] = t2buf[xx+1][yy+1];

    ix_old = screen_tcach_idx[kind][xx+yy*tile_xmax];

    ix = tcache_find_id_normal(kind, fg, bg, &is_new);

    screen_tcach_idx[kind][xx+yy*tile_xmax]=ix;
    if(is_new || ix!=ix_old || force_redraw_tile)
    {
        int x_dest = tcache_ox_normal[kind]+xx* TILE_UX_NORMAL;
        int y_dest = tcache_oy_normal[kind]+yy* TILE_UY_NORMAL;
        int wx = tcache_wx_normal[kind];
        int wy = tcache_wy_normal[kind];

        ImgCopy(tcache_image[kind],
                   0, ix*wy,   wx, wy,
                   ScrBufImg, x_dest, y_dest, 1);
    }
}

void update_single_grid(int x, int y)
{
    int sx, sy, wx, wy;

    tile_draw_grid(TCACHE0_NORMAL, x, y);
    sx = x*TILE_UX_NORMAL;
    sy = y*TILE_UY_NORMAL;
    wx = TILE_UX_NORMAL;
    wy = TILE_UY_NORMAL;
    region_tile->redraw(sx, sy, sx+wx-1, sy+wy-1);
}


// Discard cache containing specific tile
void redraw_spx_tcache(int tile)
{
    for (int kind = 0; kind < tcache_kind; kind++)
    {
        for (int idx = 0; idx < max_tcache; idx++)
        {
            int fg = tcache[kind][idx].id[0];
            int bg = tcache[kind][idx].id[1];
            if ((fg & TILE_FLAG_MASK) == tile ||
                (bg & TILE_FLAG_MASK) == tile)
            {
                tcache_compose_normal(idx, &fg, &bg);
            }
        }
    }
}

void get_bbg(int bg, int *new_bg, int *bbg)
{
    int bg0 = bg & TILE_FLAG_MASK;
    *bbg=TILE_DNGN_FLOOR;
    *new_bg= bg0;

    if(bg0 == TILE_DNGN_UNSEEN ||
      (bg0 >= TILE_DNGN_ROCK_WALL_OFS && bg0 < TILE_DNGN_WAX_WALL) ||
       bg0 == 0)
        *bbg = 0;
    else
    if( bg0 >= TILE_DNGN_FLOOR && bg0 <= TILE_DNGN_SHALLOW_WATER)
    {
        *bbg = bg;
        *new_bg = 0;
    }
}

int sink_mask_tile(int bg, int fg)
{
    int bg0 = bg & TILE_FLAG_MASK;

    if (fg == 0 || (fg & TILE_FLAG_FLYING) != 0)
        return 0;

    if ( bg0 >= TILE_DNGN_LAVA &&
         bg0 <= TILE_DNGN_SHALLOW_WATER + 3)
    {
        return TILE_SINK_MASK;
    }

    return 0;
}

//normal
void tcache_compose_normal(int ix, int *fg, int *bg)
{
    int bbg;
    int new_bg;
    int c = 1;
    int fg0 = fg[0];
    int bg0 = bg[0];
    int sink;
    img_type tc_img = tcache_image[TCACHE0_NORMAL];

    get_bbg(bg0, &new_bg, &bbg);

    if (bbg)
        tcache_overlay(tc_img, ix, bbg, TREGION_0_NORMAL, &c, NULL);

    if (bg0 & TILE_FLAG_BLOOD)
        tcache_overlay(tc_img, ix, TILE_BLOOD0, TREGION_0_NORMAL, &c, NULL);

    if (new_bg)
        tcache_overlay(tc_img, ix, new_bg, TREGION_0_NORMAL, &c, NULL);

    if ((bg0 & TILE_FLAG_SANCTUARY) && !(bg0 & TILE_FLAG_UNSEEN))
        tcache_overlay(tc_img, ix, TILE_SANCTUARY, TREGION_0_NORMAL, &c, NULL);

    // Apply the travel exclusion under the foreground if the cell is
    // visible.  It will be applied later if the cell is unseen.
    if ((bg0 & TILE_FLAG_TRAVEL_EX) && !(bg0 & TILE_FLAG_UNSEEN))
    {
        tcache_overlay(tc_img, ix, TILE_TRAVEL_EXCLUSION, TREGION_0_NORMAL, &c, 
            NULL);
    }

    if (bg0 & TILE_FLAG_RAY)
        tcache_overlay(tc_img, ix, TILE_RAY_MESH, TREGION_0_NORMAL, &c, NULL);

    if (fg0)
    {
        sink = sink_mask_tile(bg0, fg0);
        if (sink)
        {
            int flag = 2;
            register_tile_mask(sink, TREGION_0_NORMAL, &flag, sink_mask);
            tcache_overlay(tc_img, ix, fg0, TREGION_0_NORMAL, &c, sink_mask);
        }
        else
        {
            tcache_overlay(tc_img, ix, fg0, TREGION_0_NORMAL, &c, NULL);
        }
    }

    if (fg0 & TILE_FLAG_NET)
        tcache_overlay(tc_img, ix, TILE_TRAP_NET, TREGION_0_NORMAL, &c, NULL);

    if (fg0 & TILE_FLAG_S_UNDER)
    {
        tcache_overlay(tc_img, ix, TILE_SOMETHING_UNDER,
            TREGION_0_NORMAL, &c, NULL);
    }

    // Pet mark
    int status_shift = 0;
    if ((fg0 & TILE_FLAG_MAY_STAB) == TILE_FLAG_PET)
    {
        tcache_overlay(tc_img, ix, TILE_HEART, TREGION_0_NORMAL, &c, NULL);
        status_shift += 10;
    }
    else if ((fg0 & TILE_FLAG_MAY_STAB) == TILE_FLAG_STAB)
    {
        tcache_overlay(tc_img, ix, TILE_STAB_BRAND, TREGION_0_NORMAL, &c, NULL);
        status_shift += 8;
    }
    else if ((fg0 & TILE_FLAG_MAY_STAB) == TILE_FLAG_MAY_STAB)
    {
        tcache_overlay(tc_img, ix, TILE_MAY_STAB_BRAND, TREGION_0_NORMAL, &c,
            NULL);
        status_shift += 5;
    }

    if (fg0 & TILE_FLAG_POISON)
    {
        tcache_overlay(tc_img, ix, TILE_POISON, TREGION_0_NORMAL, &c, NULL,
            status_shift);
        status_shift += 5;
    }

    if (bg0 & TILE_FLAG_UNSEEN)
    {
        tcache_overlay(tc_img, ix, TILE_MESH, TREGION_0_NORMAL, &c, NULL);
    }

    if (bg0 & TILE_FLAG_MM_UNSEEN)
    {
        tcache_overlay(tc_img, ix, TILE_MAGIC_MAP_MESH, TREGION_0_NORMAL, &c, 
            NULL);
    }

    // Don't let the "new stair" icon cover up any existing icons, but
    // draw it otherwise.
    if (bg0 & TILE_FLAG_NEW_STAIR && status_shift == 0)
    {
        tcache_overlay(tc_img, ix, TILE_NEW_STAIR,
            TREGION_0_NORMAL, &c, NULL);
    }

    if ((bg0 & TILE_FLAG_TRAVEL_EX) && (bg0 & TILE_FLAG_UNSEEN))
    {
        tcache_overlay(tc_img, ix, TILE_TRAVEL_EXCLUSION, TREGION_0_NORMAL, &c, 
            NULL);
    }

    // Tile cursor
    if (bg0 & TILE_FLAG_CURSOR)
    {
       int type = ((bg0 & TILE_FLAG_CURSOR) == TILE_FLAG_CURSOR1) ?
            TILE_CURSOR : TILE_CURSOR2;

       if ((bg0 & TILE_FLAG_CURSOR) == TILE_FLAG_CURSOR3)
           type = TILE_CURSOR3;

       tcache_overlay(tc_img, ix, type, TREGION_0_NORMAL, &c, NULL);

       if (type != TILE_CURSOR3)
           c = 2;
    }

}

// Tile cursor
int TileDrawCursor(int x, int y, int cflag)
{
    int oldc = t2buf[x+1][y+1] & TILE_FLAG_CURSOR;
    t2buf[x+1][y+1] &= ~TILE_FLAG_CURSOR;
    t2buf[x+1][y+1] |= cflag;
    update_single_grid(x, y);
    return oldc;
}

void TileDrawBolt(int x, int y, int fg){

    t1buf[x+1][y+1]=fg | TILE_FLAG_FLYING;
    update_single_grid(x, y);
}

void StoreDungeonView(unsigned int *tileb)
{
    int x, y;
    int count = 0;

    if (tileb == NULL)
        tileb = tb_bk;

    for(y=0;y<tile_dngn_y;y++){
    for(x=0;x<tile_dngn_x;x++){
        tileb[count] = t1buf[x+1][y+1];
        count ++;
        tileb[count] = t2buf[x+1][y+1];
        count ++;
    }}
}

void LoadDungeonView(unsigned int *tileb)
{
    int x, y;
    int count = 0;

    if (tileb == NULL)
        tileb = tb_bk;

    for(y=0;y<tile_dngn_y;y++)
    {
        for(x=0;x<tile_dngn_x;x++)
        {
            if(tileb[count]==tileb[count+1])
                tileb[count]=0;
            t1buf[x+1][y+1] = tileb[count];
            count ++;
            t2buf[x+1][y+1] = tileb[count];
            count ++;
        }
    }
}

//Draw the tile screen once and for all
void TileDrawDungeon(unsigned int *tileb)
{
    int x, y, kind;
    if(!TileImg)
        return;

    LoadDungeonView(tileb);

    extern int tile_cursor_x;
    tile_cursor_x = -1;

    for(kind=0;kind<tcache_kind;kind++)
    {
        for(x=0;x<tile_xmax;x++)
        {
            for(y=0;y<tile_ymax;y++)
            {
                tile_draw_grid(kind, x, y);
            }
        }
    }

    force_redraw_tile = false;
    TileDrawDungeonAux();
    region_tile->redraw();
}

void TileDrawFarDungeon(int cx, int cy)
{
    unsigned int tb[TILE_DAT_YMAX*TILE_DAT_XMAX*2];

    int count = 0;
    for(int y=0; y<tile_dngn_y; y++)
    {
        for(int x=0; x<tile_dngn_x; x++)
        {
            int fg;
            int bg;

            const coord_def gc(
                cx + x - tile_dngn_x/2,
                cy + y - tile_dngn_y/2 );
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

            tb[count++]=fg;
            tb[count++]=bg;
        }
    }
    tile_finish_dngn(tb, cx, cy);
    TileDrawDungeon(tb);
    region_tile->redraw();
}

void TileDrawMap(int gx, int gy)
{
    TileDrawFarDungeon(gx, gy);
}

// Load optional wall tile
static void TileLoadWallAux(int idx_src, int idx_dst, img_type wall)
{
    int tile_per_row = ImgWidth(wall) / TILE_X;

    int sx = idx_src % tile_per_row;
    int sy = idx_src / tile_per_row;

    sx *= TILE_X;
    sy *= TILE_Y;

    ImgCopyToTileImg(idx_dst, wall, sx, sy, 1);
}

void WallIdx(int &wall, int &floor, int &special)
{
    special = -1;

    if (you.level_type == LEVEL_PANDEMONIUM)
    {
        switch (env.rock_colour)
        {
        case BLUE:
        case LIGHTBLUE:
            wall = IDX_WALL_ZOT_BLUE;
            floor = IDX_FLOOR_ZOT_BLUE;
            break;

        case RED:
        case LIGHTRED:
            wall = IDX_WALL_ZOT_RED;
            floor = IDX_FLOOR_ZOT_RED;
            break;

        case MAGENTA:
        case LIGHTMAGENTA:
            wall = IDX_WALL_ZOT_MAGENTA;
            floor = IDX_FLOOR_ZOT_MAGENTA;
            break;

        case GREEN:
        case LIGHTGREEN:
            wall = IDX_WALL_ZOT_GREEN;
            floor = IDX_FLOOR_ZOT_GREEN;
            break;

        case CYAN:
        case LIGHTCYAN:
            wall = IDX_WALL_ZOT_CYAN;
            floor = IDX_FLOOR_ZOT_CYAN;
            break;

        case BROWN:
        case YELLOW:
            wall = IDX_WALL_ZOT_YELLOW;
            floor = IDX_FLOOR_ZOT_YELLOW;
            break;

        case BLACK:
        case WHITE:
        default:
            wall = IDX_WALL_ZOT_GRAY;
            floor = IDX_FLOOR_ZOT_GRAY;
            break;
        }

        if (one_chance_in(3))
            wall = IDX_WALL_FLESH;
        if (one_chance_in(3))
            floor = IDX_FLOOR_NERVES;

        return;
    }
    else if (you.level_type == LEVEL_ABYSS)
    {
        wall = IDX_WALL_UNDEAD;
        floor = IDX_FLOOR_NERVES;
        return;
    }
    else if (you.level_type == LEVEL_LABYRINTH)
    {
        wall = IDX_WALL_UNDEAD;
        floor = IDX_FLOOR_UNDEAD;
        return;
    }
    else if (you.level_type == LEVEL_PORTAL_VAULT)
    {
        if (you.level_type_name == "bazaar")
        {
            // Bazaar tiles here lean towards grass and dirt to emphasize
            // both the non-hostile and other-wordly aspects of the
            // portal vaults (i.e. they aren't in the dungeon, necessarily.)
            int colour = env.floor_colour;
            switch (colour)
            {
                case BLUE:
                    wall = IDX_WALL_BAZAAR_GRAY;
                    floor = IDX_FLOOR_BAZAAR_GRASS;
                    special = IDX_FLOOR_BAZAAR_GRASS1_SPECIAL;
                    return;

                case RED:
                    // Reds often have lava, which looks ridiculous
                    // next to grass or dirt, so we'll use existing
                    // floor and wall tiles here.
                    wall = IDX_WALL_PEBBLE_RED;
                    floor = IDX_FLOOR_VAULT;
                    special = IDX_FLOOR_BAZAAR_VAULT_SPECIAL;
                    return;

                case LIGHTBLUE:
                    wall = IDX_WALL_HIVE;
                    floor = IDX_FLOOR_BAZAAR_GRASS;
                    special = IDX_FLOOR_BAZAAR_GRASS2_SPECIAL;
                    return;

                case GREEN:
                    wall = IDX_WALL_BAZAAR_STONE;
                    floor = IDX_FLOOR_BAZAAR_GRASS;
                    special = IDX_FLOOR_BAZAAR_GRASS1_SPECIAL;
                    return;

                case MAGENTA:
                    wall = IDX_WALL_BAZAAR_STONE;
                    floor = IDX_FLOOR_BAZAAR_DIRT;
                    special = IDX_FLOOR_BAZAAR_DIRT_SPECIAL;
                    return;

                default:
                    wall = IDX_WALL_VAULT;
                    floor = IDX_FLOOR_VAULT;
                    special = IDX_FLOOR_BAZAAR_VAULT_SPECIAL;
                    return;
            }
        }
    }

    int depth = player_branch_depth();
    int branch_depth = your_branch().depth;
    
    switch (you.where_are_you)
    {
        case BRANCH_MAIN_DUNGEON:
            wall = IDX_WALL_NORMAL;
            floor = IDX_FLOOR_NORMAL;
            return;

        case BRANCH_HIVE:
            wall = IDX_WALL_HIVE;
            floor = IDX_FLOOR_HIVE;
            return;

        case BRANCH_VAULTS:
            wall = IDX_WALL_VAULT;
            floor = IDX_FLOOR_VAULT;
            return;

        case BRANCH_ECUMENICAL_TEMPLE:
            wall = IDX_WALL_VINES;
            floor = IDX_FLOOR_VINES;
            return;

        case BRANCH_ELVEN_HALLS:
        case BRANCH_HALL_OF_BLADES:
            wall = IDX_WALL_HALL;
            floor = IDX_FLOOR_HALL;
            return;

        case BRANCH_TARTARUS:
        case BRANCH_CRYPT:
        case BRANCH_VESTIBULE_OF_HELL:
            wall = IDX_WALL_UNDEAD;
            floor = IDX_FLOOR_UNDEAD;
            return;

        case BRANCH_TOMB:
            wall = IDX_WALL_TOMB;
            floor = IDX_FLOOR_TOMB;
            return;

        case BRANCH_DIS:
            wall = IDX_WALL_ZOT_CYAN;
            floor = IDX_FLOOR_ZOT_CYAN;
            return;

        case BRANCH_GEHENNA:
            wall = IDX_WALL_ZOT_RED;
            floor = IDX_FLOOR_ROUGH_RED;
            return;

        case BRANCH_COCYTUS:
            wall = IDX_WALL_ICE;
            floor = IDX_FLOOR_ICE;
            return;

        case BRANCH_ORCISH_MINES:
            wall = IDX_WALL_ORC;
            floor = IDX_FLOOR_ORC;
            return;

        case BRANCH_LAIR:
            wall = IDX_WALL_LAIR;
            floor = IDX_FLOOR_LAIR;
            return;

        case BRANCH_SLIME_PITS:
            wall = IDX_WALL_SLIME;
            floor = IDX_FLOOR_SLIME;
            return;

        case BRANCH_SNAKE_PIT:
            wall = IDX_WALL_SNAKE;
            floor = IDX_FLOOR_SNAKE;
            return;

        case BRANCH_SWAMP:
            wall = IDX_WALL_SWAMP;
            floor = IDX_FLOOR_SWAMP;
            return;

        case BRANCH_SHOALS:
            if (depth == branch_depth)
                wall = IDX_WALL_VINES;
            else
                wall = IDX_WALL_YELLOW_ROCK;
            floor = IDX_FLOOR_SAND_STONE;
            return;

        case BRANCH_HALL_OF_ZOT:
            if (you.your_level - you.branch_stairs[7] <= 1)
            {
                wall = IDX_WALL_ZOT_YELLOW;
                floor = IDX_FLOOR_ZOT_YELLOW;
                return;
            }

            switch (you.your_level - you.branch_stairs[7])
            {
                case 2:
                    wall = IDX_WALL_ZOT_GREEN;
                    floor = IDX_FLOOR_ZOT_GREEN;
                    return;
                case 3:
                    wall = IDX_WALL_ZOT_CYAN;
                    floor = IDX_WALL_ZOT_CYAN;
                    return;
                case 4:
                    wall = IDX_WALL_ZOT_BLUE;
                    floor = IDX_FLOOR_ZOT_GREEN;
                    return;
                case 5:
                default:
                    wall = IDX_WALL_ZOT_MAGENTA;
                    floor = IDX_FLOOR_ZOT_MAGENTA;
                    return;
            }

        case BRANCH_INFERNO:
        case BRANCH_THE_PIT:
        case BRANCH_CAVERNS:
        case NUM_BRANCHES:
            break;

        // NOTE: there is no default case in this switch statement so that
        // the compiler will issue a warning when new branches are created.
    }

    wall = IDX_WALL_NORMAL;
    floor = IDX_FLOOR_NORMAL;
}
void TileLoadWall(bool wizard)
{
    clear_tcache();
    force_redraw_tile = true;

    int wall_idx;
    int floor_idx;
    int special_idx;
    WallIdx(wall_idx, floor_idx, special_idx);

    // Number of flavors are generated automatically...

    floor_tile_idx = TILE_DNGN_FLOOR;
    floor_flavors = tile_W2D_count[floor_idx];
    int offset = floor_tile_idx;
    for (int i = 0; i < floor_flavors; i++)
    {
        int idx_src = tile_W2D_start[floor_idx] + i;
        int idx_dst = offset++;
        TileLoadWallAux(idx_src, idx_dst, WallImg);
    }

    wall_tile_idx = offset;
    wall_flavors = tile_W2D_count[wall_idx];
    for (int i = 0; i < wall_flavors; i++)
    {
        int idx_src = tile_W2D_start[wall_idx] + i;
        int idx_dst = offset++;
        TileLoadWallAux(idx_src, idx_dst, WallImg);
    }

    if (special_idx != -1)
    {
        special_tile_idx = offset;
        special_flavors = tile_W2D_count[special_idx];
        for (int i = 0; i < special_flavors; i++)
        {
            int idx_src = tile_W2D_start[special_idx] + i;
            int idx_dst = offset++;
            TileLoadWallAux(idx_src, idx_dst, WallImg);
        }
    }
    else
    {
        special_flavors = 0;
    }
}

#define DOLLS_MAX 11
#define PARTS_DISP_MAX 11
#define PARTS_ITEMS 12
#define TILEP_SELECT_DOLL 20

typedef struct dolls_data
{
    int parts[TILEP_PARTS_TOTAL];
} dolls_data;
static dolls_data current_doll;
static int current_gender = 0;
static int current_parts[TILEP_PARTS_TOTAL];

static bool draw_doll(img_type img, dolls_data *doll, bool force_redraw = false, bool your_doll = true)
{
    const int p_order[TILEP_PARTS_TOTAL]=
      {
        TILEP_PART_SHADOW,
        TILEP_PART_DRCWING,
        TILEP_PART_CLOAK,
        TILEP_PART_BASE,
        TILEP_PART_BOOTS,
        TILEP_PART_LEG,
        TILEP_PART_BODY,
        TILEP_PART_ARM,
        TILEP_PART_HAND1,
        TILEP_PART_HAND2,
        TILEP_PART_HAIR,
        TILEP_PART_BEARD,
        TILEP_PART_HELM,
        TILEP_PART_DRCHEAD
      };
    int p_order2[TILEP_PARTS_TOTAL];

    int i;
    int flags[TILEP_PARTS_TOTAL];
    int parts2[TILEP_PARTS_TOTAL];
    int *parts = doll->parts;
    int c=1;
    bool changed = false;

    int default_parts[TILEP_PARTS_TOTAL];
    memset(default_parts, 0, sizeof(default_parts));

    if (your_doll)
    {
        tilep_race_default(you.species, parts[TILEP_PART_BASE] % 2,
            you.experience_level, default_parts);

        if (default_parts[TILEP_PART_BASE] != parts[TILEP_PART_BASE])
            force_redraw = true;
        parts[TILEP_PART_BASE] = default_parts[TILEP_PART_BASE];

        // TODO enne - make these configurable.
        parts[TILEP_PART_DRCHEAD] = default_parts[TILEP_PART_DRCHEAD];
        parts[TILEP_PART_DRCWING] = default_parts[TILEP_PART_DRCWING];
    }

    // convert TILEP_SHOW_EQUIP into real parts number
    for(i = 0; i < TILEP_PARTS_TOTAL; i++)
    {
        parts2[i] = parts[i];
        if (parts2[i] == TILEP_SHOW_EQUIP)
        {
            switch (i)
            {
                int item;
                case TILEP_PART_HAND1:
                    item = you.equip[EQ_WEAPON];
                    if (you.attribute[ATTR_TRANSFORMATION] ==
                        TRAN_BLADE_HANDS)
                    {
                        parts2[i] = TILEP_HAND1_BLADEHAND;
                    }
                    else if (item == -1)
                    {
                        parts2[i] = 0;
                    }
                    else
                    {
                        parts2[i] = tilep_equ_weapon(you.inv[item]);
                    }
                   break;

                case TILEP_PART_HAND2:
                    item = you.equip[EQ_SHIELD];
                    if (you.attribute[ATTR_TRANSFORMATION] ==
                        TRAN_BLADE_HANDS)
                    {
                        parts2[i] = TILEP_HAND2_BLADEHAND;
                    }
                    else if (item == -1)
                    {
                        parts2[i] = 0;
                    }
                    else
                    {
                        parts2[i] = tilep_equ_shield(you.inv[item]);
                    }
                    break;

                case TILEP_PART_BODY:
                    item = you.equip[EQ_BODY_ARMOUR];
                    if (item == -1)
                        parts2[i] = 0;
                else
                        parts2[i] = tilep_equ_armour(you.inv[item]);
                    break;

                case TILEP_PART_CLOAK:
                    item = you.equip[EQ_CLOAK];
                    if (item == -1)
                        parts2[i] = 0;
                    else
                        parts2[i] = tilep_equ_cloak(you.inv[item]);
                    break;

                case TILEP_PART_HELM:
                    item = you.equip[EQ_HELMET];
                    if (item == -1)
                        parts2[i] = 0;
                    else
                        parts2[i] = tilep_equ_helm(you.inv[item]);

                    if (parts2[i] == 0 && you.mutation[MUT_HORNS] > 0)
                    {
                        switch (you.mutation[MUT_HORNS])
                        {
                            case 1:
                                parts2[i] = TILEP_HELM_HORNS1;
                                break;
                            case 2:
                                parts2[i] = TILEP_HELM_HORNS2;
                                break;
                            case 3:
                                parts2[i] = TILEP_HELM_HORNS3;
                                break;
                        }
                    }

                    break;

                case TILEP_PART_BOOTS:
                    item = you.equip[EQ_BOOTS];
                    if (item == -1)
                        parts2[i] = 0;
                    else
                        parts2[i] = tilep_equ_boots(you.inv[item]);

                    if (parts2[i] == 0 && you.mutation[MUT_HOOVES])
                        parts2[i] = TILEP_BOOTS_HOOVES;
                    break;

                case TILEP_PART_ARM:
                    item = you.equip[EQ_GLOVES];
                    if (item == -1)
                        parts2[i] = 0;
                    else
                        parts2[i] = tilep_equ_gloves(you.inv[item]);

                    // There is player_has_claws() but it is not equivalent.
                    // Claws appear if they're big enough to not wear gloves
                    // or on races that have claws.
                    if (parts2[i] == 0 && (you.mutation[MUT_CLAWS] >= 3 || 
                        you.species == SP_TROLL || you.species == SP_GHOUL))
                    {
                        parts2[i] = TILEP_ARM_CLAWS;
                    }
                    break;

                case TILEP_PART_HAIR:
                case TILEP_PART_BEARD:
                    parts2[i] = default_parts[i];
                    break;

                case TILEP_PART_LEG:
                default:
                   parts2[i] = 0;
            }
        }

        if (parts2[i] != current_parts[i])
            changed = true;
        current_parts[i] = parts2[i];
    }


    if (!changed && !force_redraw)
    {
        return false;
    }

    tilep_calc_flags(parts2, flags);
    ImgClear(img);

    // Hack: change overlay order of boots/skirts
    for(i = 0; i < TILEP_PARTS_TOTAL; i++)
        p_order2[i]=p_order[i];

    // swap boot and leg-armor
    if (parts2[TILEP_PART_LEG] < TILEP_LEG_SKIRT_OFS)
    {
        p_order2[5] = TILEP_PART_LEG;
        p_order2[4] = TILEP_PART_BOOTS;
    }

    for(i = 0; i < TILEP_PARTS_TOTAL; i++)
    {
        int p = p_order2[i];
        int ymax = TILE_Y;
        if (flags[p]==TILEP_FLAG_CUT_CENTAUR) ymax=18;
        if (flags[p]==TILEP_FLAG_CUT_NAGA) ymax=18;

        if (parts2[p] && p == TILEP_PART_BOOTS && 
            (parts2[p] == TILEP_BOOTS_NAGA_BARDING ||
            parts2[p] == TILEP_BOOTS_CENTAUR_BARDING))
        {
            // Special case for barding.  They should be in "boots" but because
            // they're double-wide, they're stored in a different part.  We just
            // intercept it here before drawing.
            char tile = parts2[p] == TILEP_BOOTS_NAGA_BARDING ?
                TILEP_SHADOW_NAGA_BARDING : TILEP_SHADOW_CENTAUR_BARDING;
            tcache_overlay_player(img, 0, 0, TILEP_PART_SHADOW, tile, 
                TILE_Y, &c);
        }
        else if(parts2[p] && flags[p])
        {
            tcache_overlay_player(img, 0, 0, p, parts2[p], ymax, &c);
        }
    }
    return true;
}

static void load_doll_data(const char *fn, dolls_data *dolls, int max,
    int *mode, int *cur)
{
    char fbuf[1024];
    int cur0 = 0;
    int mode0 = TILEP_M_DEFAULT;
    FILE *fp = NULL;

    std::string dollsTxtString = datafile_path(fn, false, true);
    const char *dollsTxt = dollsTxtString.c_str()[0] == 0 ?
        "dolls.txt" : dollsTxtString.c_str();

    if ( (fp = fopen(dollsTxt, "r")) == NULL )
    {
        // File doesn't exist.  By default, use show equipment defaults.
        // This will be saved out (if possible.)

        for (int i = 0; i < max; i++)
        {
            // Don't take gender too seriously.  -- Enne
            tilep_race_default(you.species, coinflip() ? 1 : 0,
                you.experience_level, dolls[i].parts);

            dolls[i].parts[TILEP_PART_SHADOW] = 1;
            dolls[i].parts[TILEP_PART_CLOAK] = TILEP_SHOW_EQUIP;
            dolls[i].parts[TILEP_PART_BOOTS] = TILEP_SHOW_EQUIP;
            dolls[i].parts[TILEP_PART_LEG] = TILEP_SHOW_EQUIP;
            dolls[i].parts[TILEP_PART_BODY] = TILEP_SHOW_EQUIP;
            dolls[i].parts[TILEP_PART_ARM] = TILEP_SHOW_EQUIP;
            dolls[i].parts[TILEP_PART_HAND1] = TILEP_SHOW_EQUIP;
            dolls[i].parts[TILEP_PART_HAND2] = TILEP_SHOW_EQUIP;
            dolls[i].parts[TILEP_PART_HAIR] = TILEP_SHOW_EQUIP;
            dolls[i].parts[TILEP_PART_BEARD] = TILEP_SHOW_EQUIP;
            dolls[i].parts[TILEP_PART_HELM] = TILEP_SHOW_EQUIP;
        }

    }
    else
    {
        memset(fbuf, 0, sizeof(fbuf));
        if (fscanf(fp, "%s", fbuf) != EOF)
        {
            if (strcmp(fbuf, "MODE=LOADING") == 0)
                mode0 = TILEP_M_LOADING;
        }
        if (fscanf(fp, "%s", fbuf) != EOF)
        {
            if (strncmp(fbuf, "NUM=", 4) == 0)
            {
                sscanf(fbuf, "NUM=%d",&cur0);
                if ( (cur0 < 0)||(cur0 > 10) )
                    cur0 = 0;
            }
        }
        if (max == 1)
        {
            for(int j = 0; j <= cur0; j++)
            {
                if (fscanf(fp, "%s", fbuf) != EOF)
                {
                    tilep_scan_parts(fbuf, dolls[0].parts);
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            for(int j = 0; j < max; j++)
            {
                if (fscanf(fp, "%s", fbuf) != EOF)
                {
                    tilep_scan_parts(fbuf, dolls[j].parts);
                }
                else
                {
                    break;
                }
            }
        }

        fclose(fp);
    }
    *mode = mode0;
    *cur = cur0;

    current_gender = dolls[cur0].parts[TILEP_PART_BASE] % 2;
}

void TilePlayerEdit()
{
    const int p_lines[PARTS_ITEMS]=
      {
        TILEP_SELECT_DOLL,
        TILEP_PART_BASE,
        TILEP_PART_HELM,
        TILEP_PART_HAIR,
        TILEP_PART_BEARD,
        TILEP_PART_HAND1,
        TILEP_PART_HAND2,
        TILEP_PART_ARM,
        TILEP_PART_BODY,
        TILEP_PART_LEG,
        TILEP_PART_BOOTS,
        TILEP_PART_CLOAK
      };

    const char *p_names[PARTS_ITEMS]=
    {
        "  Index:",
        "  Gendr:",
        "  Helm :",
        "  Hair :",
        "  Beard:",
        "  Weapn:",
        "  Shild:",
        "  Glove:",
        "  Armor:",
        "  Legs :",
        "  Boots:",
        "  Cloak:"
    };

    const char *gender_name[2] ={
        "Male",
        "Fem "
    };

#define display_parts_idx(part) \
{                               \
    cgotoxy(10 , part + 1, GOTO_STAT);    \
    tilep_part_to_str(dolls[cur_doll].parts[ p_lines[part] ], ibuf);\
    cprintf(ibuf);\
}

    dolls_data dolls[DOLLS_MAX], undo_dolls[DOLLS_MAX];
    int copy_doll[TILEP_PARTS_TOTAL];

    //int parts[TILEP_PARTS_TOTAL];
    int i, j, k, x, y, kin, done = 0;
    int cur_doll = 0;
    int pre_part = 0;
    int cur_part = 0;
    int mode = TILEP_M_DEFAULT;
    int  d = 0;
    FILE *fp;
    char fbuf[80], ibuf[8];
    int tile_str, sx, sy;

    img_type PartsImg = ImgCreateSimple(TILE_X * PARTS_DISP_MAX, TILE_Y * 1);

    img_type DollsListImg = ImgCreateSimple( TILE_X * DOLLS_MAX, TILE_Y * 1);

    ImgClear(ScrBufImg);

    memset( copy_doll, 0, sizeof(dolls_data) );
    tilep_race_default(you.species, current_gender,
        you.experience_level, copy_doll);

    for(j = 0; j < DOLLS_MAX; j++)
    {
        memset( dolls[j].parts, 0, sizeof(dolls_data) );
        memset( undo_dolls[j].parts, 0, sizeof(dolls_data) );
        tilep_race_default(you.species, current_gender, you.experience_level,
            dolls[j].parts);
        tilep_race_default(you.species, current_gender, you.experience_level,
            undo_dolls[j].parts);
    }

    // load settings
    load_doll_data("dolls.txt", dolls, DOLLS_MAX, &mode, &cur_doll);
    for(j = 0; j < DOLLS_MAX; j++)
    {
        undo_dolls[j] = dolls[j];
    }
    clrscr();
    cgotoxy(1,1, GOTO_MSG);
    mpr("Select Part              : [j][k]or[up][down]");
    mpr("Change Part/Page         : [h][l]or[left][right]/[H][L]");
    mpr("Class-Default            : [CTRL]+[D]");
    mpr("Toggle Equipment Mode    : [*]");
    mpr("Take off/Take off All    : [t]/[CTRL]+[T]");
    mpr("Copy/Paste/Undo          : [CTRL]+[C]/[CTRL]+[V]/[CTRL]+[Z]");
    mpr("Randomize                : [CTRL]+[R]");

    cgotoxy(1, 8, GOTO_MSG);
    cprintf("Toggle Startup mode      : [m] (Current mode:%s)",
        ( (mode == TILEP_M_LOADING) ? "Load User's Settings":"Class Default" ));
    for (j=0; j < PARTS_ITEMS; j++)
    {
        cgotoxy(1, 1+j, GOTO_STAT);
        cprintf(p_names[j]);
    }

#define PARTS_Y (TILE_Y*11)

// draw strings "Dolls/Parts" into backbuffer
    tile_str = TILE_TEXT_PARTS_E;
    sx = (tile_str % TILE_PER_ROW)*TILE_X;
    sy = (tile_str / TILE_PER_ROW)*TILE_Y;

    ImgCopy(TileImg, sx, sy, TILE_X, TILE_Y/2,
                ScrBufImg, (TILE_X * 3) - 8,(TILE_Y * 4) - 8-24, 0);
    ImgCopy(TileImg, sx, sy+ TILE_Y/2, TILE_X, TILE_Y/2,
                ScrBufImg, (TILE_X * 4) - 8,(TILE_Y * 4) - 8-24, 0);

    tile_str = TILE_TEXT_DOLLS_E;
    sx = (tile_str % TILE_PER_ROW)*TILE_X;
    sy = (tile_str / TILE_PER_ROW)*TILE_Y;
    ImgCopy(TileImg, sx, sy, TILE_X, TILE_Y/2,
                ScrBufImg, (TILE_X * 3) - 8,PARTS_Y - 8 -24, 0);
    ImgCopy(TileImg, sx, sy+ TILE_Y/2, TILE_X, TILE_Y/2,
                ScrBufImg, (TILE_X * 4) - 8,PARTS_Y - 8 -24, 0);

    ImgClear(DollsListImg);
    for(j = 0; j < DOLLS_MAX; j++)
    {
        draw_doll(DollCacheImg, &dolls[j], true);
        ImgCopy(DollCacheImg, 0, 0,
        ImgWidth(DollCacheImg), ImgHeight(DollCacheImg),
            DollsListImg, j * TILE_X, 0, 1);
    }

    done = 0;

    while (!done)
    {
        static int inc = 1;
        static int cur_inc = 1;

        ImgClear(PartsImg);

        region_tile->DrawPanel((TILE_X * 3) - 8, (TILE_Y * 4) - 8,
                   ImgWidth(PartsImg) + 16, ImgHeight(PartsImg) + 16);
        region_tile->DrawPanel((TILE_X * 3) - 8, PARTS_Y - 8,
                   ImgWidth(DollsListImg) + 16, ImgHeight(DollsListImg) + 16);
        region_tile->DrawPanel(8*TILE_X - 8, 8*TILE_Y - 8,
                   TILE_X + 16, TILE_Y + 16);

        if (cur_part == 0)
        {
            // now selecting doll index
            cgotoxy(10 , 1, GOTO_STAT);
            cprintf("%02d", cur_doll);
            cgotoxy(10 , 2, GOTO_STAT);
            current_gender = dolls[cur_doll].parts[TILEP_PART_BASE] % 2;
            cprintf("%s", gender_name[ current_gender ]);

            for (i = 2; i<PARTS_ITEMS; i++)
            {
                cgotoxy(10 , i + 1, GOTO_STAT);
                tilep_part_to_str(dolls[cur_doll].parts[ p_lines[i] ], ibuf);
                cprintf("%s / %03d", ibuf, tilep_parts_total[ p_lines[i] ]);
            }
        }
        else
        {
            for(i = 0; i < PARTS_DISP_MAX; i++)
            {
                int index;
                if (dolls[cur_doll].parts[ p_lines[cur_part] ] 
                    == TILEP_SHOW_EQUIP)
                    index = 0;
                else
                    index = i + dolls[cur_doll].parts[ p_lines[cur_part] ] 
                      - (PARTS_DISP_MAX / 2);
                if (index < 0)
                    index = index + tilep_parts_total[ p_lines[cur_part] ] + 1;
                if (index > tilep_parts_total[ p_lines[cur_part] ])
                    index = index - tilep_parts_total[ p_lines[cur_part] ] - 1;
                tcache_overlay_player(PartsImg, i * TILE_X, 0, 
                p_lines[cur_part], index, TILE_Y, &d);
            }
            ImgCopy(PartsImg, 0, 0, ImgWidth(PartsImg), ImgHeight(PartsImg),
                          ScrBufImg, 3 * TILE_X, 4 * TILE_Y, 0);
            ImgCopyFromTileImg( TILE_CURSOR, ScrBufImg,
                  (3 + PARTS_DISP_MAX / 2) * TILE_X, 4 * TILE_Y, 0);
        }
        draw_doll(DollCacheImg, &dolls[cur_doll], true);

        ImgCopy(DollCacheImg, 0, 0, TILE_X, TILE_Y,
                      DollsListImg, cur_doll*TILE_X, 0, 1);

        ImgCopyToTileImg(TILE_PLAYER, DollCacheImg, 0, 0, 1);
        ImgCopyFromTileImg(TILE_PLAYER, ScrBufImg, 8*TILE_X, 8*TILE_Y, 0);
        ImgCopy(DollsListImg, 0, 0,
                      ImgWidth(DollsListImg), ImgHeight(DollsListImg),
                      ScrBufImg, 3 * TILE_X, PARTS_Y, 0);
        ImgCopyFromTileImg( TILE_CURSOR, ScrBufImg,
                            (3 + cur_doll) * TILE_X, PARTS_Y, 0);
        region_tile->redraw();

        cgotoxy(10 , cur_part + 1, GOTO_STAT);

        if (cur_part == 1)
        {
            current_gender = dolls[cur_doll].parts[p_lines[cur_part] ]%2;
            cprintf("%s", gender_name[ current_gender ]);
        }
        else if (cur_part != 0)
        {
            tilep_part_to_str(dolls[cur_doll].parts[ p_lines[cur_part] ], ibuf);
            cprintf("%s", ibuf);
        }

        cgotoxy(1, pre_part + 1, GOTO_STAT);cprintf("  ");
        cgotoxy(1, cur_part + 1, GOTO_STAT);cprintf("->");
        pre_part = cur_part;

        /* Get a key */
        kin = getch();

        inc = cur_inc = 0;

        /* Analyze the key */
        switch (kin)
        {
            case 0x1B: //ESC
                done = 1;
                break;

            case 'h':
                inc = -1;
                break;
            case 'l':
                inc = +1;
                break;
            case 'H':
                inc = -PARTS_DISP_MAX;
                break;
            case 'L':
                inc = +PARTS_DISP_MAX;
                break;
            case 'j':
            {
                if (cur_part < (PARTS_ITEMS-1) )
                    cur_part += 1;
                else
                    cur_part = 0;
                break;
            }
            case 'k':
            {
                if (cur_part > 0)
                    cur_part -= 1;
                else
                    cur_part = (PARTS_ITEMS-1);
                break;
            }
            case 'm':
            {
                if (mode == TILEP_M_LOADING)
                    mode = TILEP_M_DEFAULT;
                else
                    mode = TILEP_M_LOADING;

                cgotoxy(1, 8, GOTO_MSG);
                cprintf("Toggle Startup mode      : [m] (Current mode:%s)",
                    ( (mode == TILEP_M_LOADING) ? "Load User's Settings":"Class Default" ));
                break;
            }

            case 't':
            {
                if (cur_part >= 2)
                {
                    dolls[cur_doll].parts[ p_lines[cur_part] ] = 0;
                    display_parts_idx(cur_part);
                }
                break;
            }

            case CONTROL('D'):
            {
                tilep_job_default(you.char_class, current_gender,
                dolls[cur_doll].parts);
                for(i = 2; i < PARTS_ITEMS; i++)
                {
                    display_parts_idx(i);
                }
                break;
            }

            case CONTROL('T'):
            {
                for(i = 2; i < PARTS_ITEMS; i++)
                {
                    undo_dolls[cur_doll].parts[ p_lines[i] ] = dolls[cur_doll].parts[ p_lines[i] ];
                    dolls[cur_doll].parts[ p_lines[i] ] = 0;
                    display_parts_idx(i);
                }
                break;
            }

            case CONTROL('C'):
            {
                for(i = 2; i < PARTS_ITEMS; i++)
                {
                    copy_doll[ p_lines[i] ] = dolls[cur_doll].parts[ p_lines[i] ];
                }
                break;
            }

            case CONTROL('V'):
            {
                for(i = 2; i < PARTS_ITEMS; i++)
                {
                    undo_dolls[cur_doll].parts[ p_lines[i] ] = dolls[cur_doll].parts[ p_lines[i] ];
                    dolls[cur_doll].parts[ p_lines[i] ] = copy_doll[ p_lines[i] ];
                    display_parts_idx(i);
                }
                break;
            }

            case CONTROL('Z'):
            {
                for(i = 2; i < PARTS_ITEMS; i++)
                {
                    dolls[cur_doll].parts[ p_lines[i] ] = undo_dolls[cur_doll].parts[ p_lines[i] ];
                    display_parts_idx(i);
                }
                break;
            }

            case CONTROL('R'):
            {
                for(i = 2; i < PARTS_ITEMS; i++)
                {
                    undo_dolls[cur_doll].parts[ p_lines[i] ] = dolls[cur_doll].parts[ p_lines[i] ];
                }
                dolls[cur_doll].parts[TILEP_PART_CLOAK] =
                    one_chance_in(2) * ( random2(tilep_parts_total[ TILEP_PART_CLOAK ]) + 1);
                dolls[cur_doll].parts[TILEP_PART_BOOTS] =
                    ( random2(tilep_parts_total[ TILEP_PART_BOOTS ] + 1) );
                dolls[cur_doll].parts[TILEP_PART_LEG] =
                    ( random2(tilep_parts_total[ TILEP_PART_LEG ] + 1) );
                dolls[cur_doll].parts[TILEP_PART_BODY] =
                    ( random2(tilep_parts_total[ TILEP_PART_BODY ] + 1) );
                dolls[cur_doll].parts[TILEP_PART_ARM] =
                    one_chance_in(2) * ( random2(tilep_parts_total[ TILEP_PART_ARM ]) + 1);
                dolls[cur_doll].parts[TILEP_PART_HAND1] =
                    ( random2(tilep_parts_total[ TILEP_PART_HAND1 ] + 1) );
                dolls[cur_doll].parts[TILEP_PART_HAND2] =
                    one_chance_in(2) * ( random2(tilep_parts_total[ TILEP_PART_HAND2 ]) + 1);
                dolls[cur_doll].parts[TILEP_PART_HAIR] =
                    ( random2(tilep_parts_total[ TILEP_PART_HAIR ] + 1) );
                dolls[cur_doll].parts[TILEP_PART_BEARD] =
                    ( (dolls[cur_doll].parts[TILEP_PART_BASE] + 1) % 2) *
                    one_chance_in(4) * ( random2(tilep_parts_total[ TILEP_PART_BEARD ]) + 1 );
                dolls[cur_doll].parts[TILEP_PART_HELM] =
                    one_chance_in(2) * ( random2(tilep_parts_total[ TILEP_PART_HELM ]) + 1 );
                for(i = 2; i < PARTS_ITEMS; i++)
                {
                    display_parts_idx(i);
                }
                break;
            }
            case '*':
            {
                int part = p_lines[cur_part];
                int *target = &dolls[cur_doll].parts[part];

                if (part == TILEP_PART_BASE)
                      continue;
                if (*target == TILEP_SHOW_EQUIP)
                    *target = 0;
                else
                    *target = TILEP_SHOW_EQUIP;

                display_parts_idx(cur_part);
                break;
            }
#ifdef WIZARD
            case '1':
                dolls[cur_doll].parts[TILEP_PART_DRCHEAD]++;
                dolls[cur_doll].parts[TILEP_PART_DRCHEAD] %=
                tilep_parts_total[TILEP_PART_DRCHEAD] -1;
                break;
            case '2':
                dolls[cur_doll].parts[TILEP_PART_DRCWING]++;
                dolls[cur_doll].parts[TILEP_PART_DRCWING] %=
                tilep_parts_total[TILEP_PART_DRCWING] -1;
                break;
#endif
            default:
            {
            }
        }

        if (inc != 0)
        {
            int *target = &dolls[cur_doll].parts[ p_lines[cur_part] ];

            if (cur_part == 0)
            {
                if (inc > 0)
                    cur_doll++;
                else
                    cur_doll--;
                if (cur_doll < 0)
                    cur_doll = DOLLS_MAX -1;
                else if (cur_doll == DOLLS_MAX)
                    cur_doll = 0;
            }
            else if (cur_part == 1)
            {
                if (*target % 2)
                    (*target)++;
                else
                    (*target)--;
            }
            else
            {
                if (*target == TILEP_SHOW_EQUIP) continue;
                (*target) += inc;
                (*target) += tilep_parts_total[ p_lines[cur_part] ]+1;
                (*target) %= tilep_parts_total[ p_lines[cur_part] ]+1;
            }
        }

        delay(20);
    }

    current_doll = dolls[cur_doll];
    draw_doll(DollCacheImg, &current_doll);
    ImgCopyToTileImg(TILE_PLAYER, DollCacheImg, 0, 0, 1);

    std::string dollsTxtString = datafile_path("dolls.txt", false, true);
    const char *dollsTxt = dollsTxtString.c_str()[0] == 0 ?
        "dolls.txt" : dollsTxtString.c_str();

    if ( (fp = fopen(dollsTxt, "w+")) == NULL )
    {
    }
    else
    {
        fprintf(fp, "MODE=%s\n", ( (mode == TILEP_M_LOADING) ? "LOADING":"DEFAULT" ) );
        fprintf(fp, "NUM=%02d\n", cur_doll);
        for(j = 0; j < DOLLS_MAX; j++)
        {
            tilep_print_parts(fbuf, dolls[j].parts);
            fprintf(fp, "%s\n", fbuf);
        }
        fclose(fp);
    }

    ImgDestroy(PartsImg);
    ImgDestroy(DollsListImg);

    ImgClear(ScrBufImg);
    redraw_spx_tcache(TILE_PLAYER);

    for(x=0;x<TILE_DAT_XMAX+2;x++)
    {
        for(y=0;y<TILE_DAT_YMAX+2;y++)
        {
            t1buf[x][y] = 0;
            t2buf[x][y] = TILE_DNGN_UNSEEN | TILE_FLAG_UNSEEN;
        }
    }

    for(k=0;k<tcache_kind;k++)
        for(x=0;x<tile_xmax*tile_ymax;x++)
            screen_tcach_idx[k][x] = -1;

    clrscr();
    redraw_screen();
}

void TilePlayerRefresh()
{
    if (!draw_doll(DollCacheImg, &current_doll))
        return; // Not changed

    ImgCopyToTileImg(TILE_PLAYER, DollCacheImg, 0, 0, 1);

    redraw_spx_tcache(TILE_PLAYER);
    force_redraw_tile = true;

    const coord_def ep = grid2view(you.pos());
    update_single_grid(ep.x-1, ep.y-1);
}

void TilePlayerInit()
{
    int i;
    int cur_doll = 0;
    int mode = TILEP_M_DEFAULT;
    dolls_data doll;
    int gender = 0;

    for(i = 0; i < TILEP_PARTS_TOTAL; i++)
        doll.parts[i] = 0;
    tilep_race_default(you.species, gender, you.experience_level, doll.parts);
    tilep_job_default(you.char_class, gender, doll.parts);

    load_doll_data("dolls.txt", &doll, 1, &mode, &cur_doll); 
    current_doll = doll;
    draw_doll(DollCacheImg, &doll);

    ImgCopyToTileImg(TILE_PLAYER, DollCacheImg, 0, 0, 1);

}

void TileGhostInit(const struct ghost_demon &ghost)
{
    dolls_data doll;
    int x, y;
    unsigned int pseudo_rand = ghost.max_hp*54321*54321;
    char mask[TILE_X*TILE_Y];
    int g_gender = (pseudo_rand >>8)&1;

    for (x=0; x<TILE_X; x++)
    for (y=0; y<TILE_X; y++)
        mask[x+y*TILE_X] = (x+y)&1;

    for(x = 0; x < TILEP_PARTS_TOTAL; x++)
    {
        doll.parts[x] = 0;
        current_parts[x] = 0;
    }
    tilep_race_default(ghost.species, g_gender,
        ghost.xl, doll.parts);
    tilep_job_default (ghost.job, g_gender,  doll.parts);

    for(x = TILEP_PART_CLOAK; x < TILEP_PARTS_TOTAL; x++)
    {
        if (doll.parts[x] == TILEP_SHOW_EQUIP)
        {
            doll.parts[x] = 1 + (pseudo_rand % tilep_parts_total[x]);

            if (x == TILEP_PART_BODY)
            {
                int p = 0;
                int ac = ghost.ac;
                ac *= (5 + (pseudo_rand/11) % 11);
                ac /= 10;

                if (ac > 25) p= TILEP_BODY_PLATE_BLACK;
                else
                if (ac > 20) p=  TILEP_BODY_BANDED;
                else
                if (ac > 15) p= TILEP_BODY_SCALEMAIL;
                else
                if (ac > 10) p= TILEP_BODY_CHAINMAIL;
                else
                if (ac > 5 ) p= TILEP_BODY_LEATHER_HEAVY;
                else
                             p= TILEP_BODY_ROBE_BLUE;
                doll.parts[x] = p;
            }
        }
    }

    int sk = ghost.best_skill;
    int dam = ghost.damage;
    int p = 0;

    dam *= (5 + pseudo_rand % 11);
    dam /= 10;

    if (sk == SK_MACES_FLAILS)
    {
        if (dam>30) p = TILEP_HAND1_GREAT_FRAIL;
        else
        if (dam>25) p = TILEP_HAND1_GREAT_MACE;
        else
        if (dam>20) p = TILEP_HAND1_SPIKED_FRAIL;
        else
        if (dam>15) p = TILEP_HAND1_MORNINGSTAR;
        else
        if (dam>10) p = TILEP_HAND1_FRAIL;
        else
        if (dam>5) p = TILEP_HAND1_MACE;
        else
        p = TILEP_HAND1_CLUB_SLANT;

        doll.parts[TILEP_PART_HAND1] = p;
    }
    else
    if (sk == SK_SHORT_BLADES)
    {
        if (dam>20) p = TILEP_HAND1_SABRE;
        else
        if (dam>10) p = TILEP_HAND1_SHORT_SWORD_SLANT;
        else
        p = TILEP_HAND1_DAGGER_SLANT;

        doll.parts[TILEP_PART_HAND1] = p;
    }
    else
    if (sk == SK_LONG_BLADES)
    {
        if (dam>25) p = TILEP_HAND1_GREAT_SWORD_SLANT;
        else
        if (dam>20) p = TILEP_HAND1_KATANA_SLANT;
        else
        if (dam>15) p = TILEP_HAND1_SCIMITAR;
        else
        if (dam>10) p = TILEP_HAND1_LONG_SWORD_SLANT;
        else
        p = TILEP_HAND1_FALCHION;

        doll.parts[TILEP_PART_HAND1] = p;
    }
    else
    if (sk == SK_AXES)
    {
        if (dam>30) p = TILEP_HAND1_EXECUTIONERS_AXE;
        else
        if (dam>20) p = TILEP_HAND1_BATTLEAXE;
        else
        if (dam>15) p = TILEP_HAND1_BROAD_AXE;
        else
        if (dam>10) p = TILEP_HAND1_WAR_AXE;
        else
        p = TILEP_HAND1_HAND_AXE;

        doll.parts[TILEP_PART_HAND1] = p;
    }
    else
    if (sk == SK_POLEARMS)
    {
        if (dam>30) p =  TILEP_HAND1_GLAIVE;
        else
        if (dam>20) p = TILEP_HAND1_SCYTHE;
        else
        if (dam>15) p = TILEP_HAND1_HALBERD;
        else
        if (dam>10) p = TILEP_HAND1_TRIDENT2;
        else
        if (dam>10) p = TILEP_HAND1_HAMMER;
        else
        p = TILEP_HAND1_SPEAR;

        doll.parts[TILEP_PART_HAND1] = p;
    }
    else
    if (sk == SK_BOWS)
        doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_BOW2;
    else
    if (sk == SK_CROSSBOWS)
        doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_CROSSBOW;
    else
    if (sk == SK_SLINGS)
        doll.parts[TILEP_PART_HAND1] = TILEP_HAND1_SLING;
    else
    if (sk == SK_UNARMED_COMBAT)
        doll.parts[TILEP_PART_HAND1] = doll.parts[TILEP_PART_HAND2] = 0;

    ImgClear(DollCacheImg);
    // Clear
    ImgCopyToTileImg(TILE_MONS_PLAYER_GHOST, DollCacheImg, 0, 0, 1);

    draw_doll(DollCacheImg, &doll);
    ImgCopyToTileImg(TILE_MONS_PLAYER_GHOST, DollCacheImg, 0, 0, 1, mask, false);
    redraw_spx_tcache(TILE_MONS_PLAYER_GHOST);
}

void TileInitItems()
{
    for (int i = 0; i < NUM_POTIONS; i++)
    {
        int special = you.item_description[IDESC_POTIONS][i];
        int tile0 = TILE_POTION_OFFSET + special % 14;
        int tile1 = TILE_POT_HEALING + i;

        ImgCopyFromTileImg(tile0, DollCacheImg, 0, 0, 1);
        ImgCopyFromTileImg(tile1, DollCacheImg, 0, 0, 0);
        ImgCopyToTileImg(tile1, DollCacheImg, 0, 0, 1);
    }

    for (int i = 0; i < NUM_WANDS; i++)
    {
        int special = you.item_description[IDESC_WANDS][i];
        int tile0 = TILE_WAND_OFFSET + special % 12;
        int tile1 = TILE_WAND_FLAME + i;

        ImgCopyFromTileImg(tile0, DollCacheImg, 0, 0, 1);
        ImgCopyFromTileImg(tile1, DollCacheImg, 0, 0, 0);
        ImgCopyToTileImg(tile1, DollCacheImg, 0, 0, 1);
    }

    for (int i = 0; i < STAFF_SMITING; i++)
    {
        int special = you.item_description[IDESC_STAVES][i];
        int tile0 = TILE_STAFF_OFFSET + (special / 4) % 10;
        int tile1 = TILE_STAFF_WIZARDRY + i;

        ImgCopyFromTileImg(tile0, DollCacheImg, 0, 0, 1);
        ImgCopyFromTileImg(tile1, DollCacheImg, 0, 0, 0);
        ImgCopyToTileImg(tile1, DollCacheImg, 0, 0, 1);
    }
    for (int i = STAFF_SMITING; i < NUM_STAVES; i++)
    {
        int special = you.item_description[IDESC_STAVES][i];
        int tile0 = TILE_ROD_OFFSET + (special / 4) % 10;
        int tile1 = TILE_ROD_SMITING + i - STAFF_SMITING;

        ImgCopyFromTileImg(tile0, DollCacheImg, 0, 0, 1);
        ImgCopyFromTileImg(tile1, DollCacheImg, 0, 0, 0);
        ImgCopyToTileImg(tile1, DollCacheImg, 0, 0, 1);
    }
}

// Monster weapon tile
#define N_MCACHE (TILE_MCACHE_END - TILE_MCACHE_START +1)

typedef struct mcache mcache;
struct mcache
{
  bool lock_flag;
  mcache *next;
  int mon_tile;
  int equ_tile;
  int draco;
  int idx;
};

mcache mc_data[N_MCACHE];

mcache *mc_head = NULL;

static void ImgCopyDoll(int equ_tile, int hand, int ofs_x, int ofs_y)
{
    int handidx = hand == 1 ? TILEP_PART_HAND1 : TILEP_PART_HAND2;

    int nx = tilep_parts_nx[handidx];
    int ny = tilep_parts_ny[handidx];
    int ox = tilep_parts_ox[handidx];
    int oy = tilep_parts_oy[handidx];
    int wx = std::min(TILE_X/nx + ofs_x, TILE_X/nx);
    int wy = std::min(TILE_Y/ny + ofs_y, TILE_Y/ny);
    int idx = equ_tile -1;
    int tidx = tilep_parts_start[handidx] +
        idx/(nx*ny);

    //Source pos
    int xs = (tidx % TILEP_PER_ROW)*TILE_X + 
        (idx % nx)*(TILE_X/nx) - ofs_x;
    int ys = (tidx / TILEP_PER_ROW)*TILE_Y + 
        ((idx/nx) % ny)*(TILE_Y/ny) - ofs_y;

    ImgCopy(PlayerImg, xs, ys, wx, wy,
                  DollCacheImg, ox, oy, 0);
}

static void mcache_compose(int tile_idx, int mon_tile, int equ_tile)
{
    int ofs_x=0;
    int ofs_y=0;

    switch(mon_tile)
    {
        case TILE_MONS_ORC:
        case TILE_MONS_URUG:
        case TILE_MONS_BLORK_THE_ORC:
        case TILE_MONS_ORC_WARRIOR:
        case TILE_MONS_ORC_KNIGHT:
        case TILE_MONS_ORC_WARLORD:
            ofs_y = 2;
            break;
        case TILE_MONS_GOBLIN:
        case TILE_MONS_IJYB:
            ofs_y = 4;
            break;
        case TILE_MONS_GNOLL:
            ofs_x = -1;
            break;
        case TILE_MONS_BOGGART:
            ofs_y = 2;
            break;
        case TILE_MONS_DEEP_ELF_FIGHTER:
        case TILE_MONS_DEEP_ELF_SOLDIER:
            ofs_y = 2;
            break;
        case TILE_MONS_DEEP_ELF_KNIGHT:
            ofs_y = 1;
            break;
        case TILE_MONS_KOBOLD:
            ofs_x = 3;
            ofs_y = 4;
            break;
        case TILE_MONS_KOBOLD_DEMONOLOGIST:
            ofs_y = -10;
            break;
        case TILE_MONS_BIG_KOBOLD:
            ofs_x = 2;
            ofs_y = 3;
            break;
        case TILE_MONS_MIDGE:
            ofs_y = -2;
            break;
        case TILE_MONS_NAGA:
        case TILE_MONS_GREATER_NAGA:
        case TILE_MONS_NAGA_WARRIOR:
        case TILE_MONS_GUARDIAN_NAGA:
        case TILE_MONS_NAGA_MAGE:
            ofs_y = 1;
            break;
        case TILE_MONS_HELL_KNIGHT:
            ofs_x = -1;
            ofs_y = 3;
            break;
        case TILE_MONS_RED_DEVIL:
            ofs_x = 2;
            ofs_y = -3;
            break;
        case TILE_MONS_WIZARD:
            ofs_x = 2;
            ofs_y = -2;
            break;
        case TILE_MONS_HUMAN:
            ofs_x = 5;
            ofs_y = 2;
            break;
        case TILE_MONS_ELF:
            ofs_y = 1;
            ofs_x = 4;
            break;
        case TILE_MONS_OGRE_MAGE:
            ofs_y = -2;
            ofs_x = -4;
            break;
        case TILE_MONS_DEEP_ELF_MAGE:
        case TILE_MONS_DEEP_ELF_SUMMONER:
        case TILE_MONS_DEEP_ELF_CONJURER:
        case TILE_MONS_DEEP_ELF_PRIEST:
        case TILE_MONS_DEEP_ELF_HIGH_PRIEST:
        case TILE_MONS_DEEP_ELF_DEMONOLOGIST:
        case TILE_MONS_DEEP_ELF_ANNIHILATOR:
        case TILE_MONS_DEEP_ELF_SORCERER:
            ofs_x = -1;
            ofs_y = -2;
            break;
        case TILE_MONS_DEEP_ELF_DEATH_MAGE:
            ofs_x = -1;
            break;
    }

    // Copy monster tile
    ImgCopyFromTileImg(mon_tile, DollCacheImg, 0, 0, 1);

    // Overlay weapon tile
    ImgCopyDoll(equ_tile, 1, ofs_x, ofs_y);

    // In some cases, overlay a second weapon tile...
    if (mon_tile == TILE_MONS_DEEP_ELF_BLADEMASTER)
    {
        int eq2;
        switch (equ_tile)
        {
            case TILEP_HAND1_DAGGER:
                eq2 = TILEP_HAND2_DAGGER;
                break;
            case TILEP_HAND1_SABRE:
                eq2 = TILEP_HAND2_SABRE;
                break;
            default:
            case TILEP_HAND1_SHORT_SWORD_SLANT:
                eq2 = TILEP_HAND2_SHORT_SWORD_SLANT;
                break;
        };
        ImgCopyDoll(eq2, 2, -ofs_x, ofs_y);
    }

    // Copy to the buffer
    ImgCopyToTileImg(tile_idx, DollCacheImg, 0, 0, 1);

    redraw_spx_tcache(tile_idx);
}

static void mcache_compose_draco(int tile_idx, int race, int cls, int w)
{
    extern int draconian_color(int race, int level);

    dolls_data doll;
    int x;

    int color = draconian_color(race, -1);
    int armour = 0;
    int armour2 = 0;
    int weapon = 0;
    int weapon2 = 0;
    int arm = 0;

    for(x = 0; x < TILEP_PARTS_TOTAL; x++)
    {
        doll.parts[x] = 0;
        current_parts[x] = 0;
    }
    doll.parts[TILEP_PART_SHADOW] = 1;

    doll.parts[TILEP_PART_BASE] = TILEP_BASE_DRACONIAN + color *2;
    doll.parts[TILEP_PART_DRCWING] = 1 + color;
    doll.parts[TILEP_PART_DRCHEAD] = 1 + color;

    switch(cls)
    {
        case MONS_DRACONIAN_CALLER:
            weapon = TILEP_HAND1_STAFF_EVIL;
            weapon2 = TILEP_HAND2_BOOK_YELLOW;
            armour = TILEP_BODY_ROBE_BROWN;
            break;

        case MONS_DRACONIAN_MONK:
            arm = TILEP_ARM_GLOVE_SHORT_BLUE;
            armour = TILEP_BODY_KARATE2;
            break;

        case MONS_DRACONIAN_ZEALOT:
            weapon = TILEP_HAND1_MACE;
            weapon2 = TILEP_HAND2_BOOK_CYAN;
            armour = TILEP_BODY_MONK_BLUE;
            break;

        case MONS_DRACONIAN_SHIFTER:
            weapon = TILEP_HAND1_STAFF_LARGE;
            armour = TILEP_BODY_ROBE_CYAN;
            weapon2 = TILEP_HAND2_BOOK_GREEN;
            break;

        case MONS_DRACONIAN_ANNIHILATOR:
            weapon = TILEP_HAND1_STAFF_RUBY;
            weapon2 = TILEP_HAND2_FIRE_CYAN;
            armour = TILEP_BODY_ROBE_GREEN_GOLD;
            break;

        case MONS_DRACONIAN_KNIGHT:
            weapon = w;
            weapon2 = TILEP_HAND2_SHIELD_KNIGHT_GRAY;
            armour = TILEP_BODY_BPLATE_METAL1;
            armour2 = TILEP_LEG_BELT_GRAY;
            break;

        case MONS_DRACONIAN_SCORCHER:
            weapon = TILEP_HAND1_FIRE_RED;
            weapon2 = TILEP_HAND2_BOOK_RED;
            armour = TILEP_BODY_ROBE_RED;
            break;

        default:
            weapon = w;
            armour = TILEP_BODY_BELT2;
            armour2 = TILEP_LEG_LOINCLOTH_RED;
            break;
    }

    doll.parts[TILEP_PART_HAND1] = weapon;
    doll.parts[TILEP_PART_HAND2] = weapon2;
    doll.parts[TILEP_PART_BODY] = armour;
    doll.parts[TILEP_PART_LEG] = armour2;
    doll.parts[TILEP_PART_ARM] = arm;

    ImgClear(DollCacheImg);
    draw_doll(DollCacheImg, &doll, true, false);
    // Copy to the buffer
    ImgCopyToTileImg(tile_idx, DollCacheImg, 0, 0, 1);
    redraw_spx_tcache(tile_idx);
}

static void mcache_init()
{
    int i;

    for(i=0;i<N_MCACHE;i++)
    {
        mc_data[i].lock_flag = false;
        mc_data[i].next = NULL;
        if (i!=N_MCACHE-1)
            mc_data[i].next = &mc_data[i+1];
        mc_data[i].idx = TILE_MCACHE_START + i;
        mc_data[i].mon_tile = 0;
        mc_data[i].equ_tile = 0;
        mc_data[i].draco = 0;
    }
    mc_head = &mc_data[0];
}

int get_base_idx_from_mcache(int tile_idx)
{
    for (mcache *mc = mc_head; mc != NULL; mc = mc->next)
    {
        if (mc->idx == tile_idx)
            return mc->mon_tile;
    }

    return tile_idx;
}

int get_clean_map_idx(int tile_idx)
{
    int idx = tile_idx & TILE_FLAG_MASK;
    if (idx >= TILE_CLOUD_FIRE_0 && idx <= TILE_CLOUD_PURP_SMOKE ||
        idx >= TILE_MONS_SHADOW && idx <= TILE_MONS_WATER_ELEMENTAL ||
        idx >= TILE_MCACHE_START && idx <= TILE_MCACHE_END)
    {
        return 0;
    }
    else
    {
        return tile_idx;
    }
}

void TileMcacheUnlock()
{
    int i;

    for(i=0;i<N_MCACHE;i++)
    {
        mc_data[i].lock_flag = false;
    }
}

int TileMcacheFind(int mon_tile, int equ_tile, int draco)
{
    mcache *mc = mc_head;
    mcache *prev = NULL;
    mcache *empty = NULL;
#ifdef DEBUG_DIAGNOSTICS
    int count = 0;
    char cache_info[40];
#endif
    int best2 = -1;
    int best3 = -1;

    while(1){
        if(mon_tile == mc->mon_tile && equ_tile == mc->equ_tile
            && draco == mc->draco)
        {
             // match found
             // move cache to the head to reduce future search time
             if (prev != NULL) prev->next = mc->next;
             if (mc != mc_head) mc->next = mc_head;
             mc_head = mc;

             // lock it
             mc->lock_flag=true;
             // return cache index
             return mc->idx;
        }
        else if(draco != 0 && mon_tile == mc->mon_tile && draco == mc->draco)
            // second best for draconian: only weapon differ
            best2 = mc->idx;
        else if(draco != 0 && mon_tile == mc->mon_tile)
            // third best for draconian: only class matches
            best3 = mc->idx;

        if (!mc->lock_flag)
            empty = mc;
        if (mc->next == NULL)
            break;
        prev = mc;
        mc = mc->next;

#ifdef DEBUG_DIAGNOSTICS
        count++;
#endif

    }//while

    // cache image not found and no room do draw it
    if(empty == NULL)
    {
#ifdef DEBUG_DIAGNOSTICS
        snprintf( cache_info, 39, "mcache (M %d, E %d) cache full",
            mon_tile, equ_tile);
        mpr(cache_info, MSGCH_DIAGNOSTICS );
#endif
        if (best2 != -1)
            return best2;
        if (best3 != -1)
            return best3;
        if (draco != 0)
            return TILE_ERROR;
        else
            return mon_tile;
    }
    mc = empty;

#ifdef DEBUG_DIAGNOSTICS
             snprintf( cache_info, 39, "mcache (M %d, E %d) newly composed",
                        mon_tile, equ_tile);
             mpr(cache_info, MSGCH_DIAGNOSTICS );
#endif

    // compose new image
    if (draco != 0)
        // race, class, weapon
        mcache_compose_draco(mc->idx, draco, mon_tile, equ_tile);
    else
        mcache_compose(mc->idx, mon_tile, equ_tile);
    mc->mon_tile = mon_tile;
    mc->equ_tile = equ_tile;
    mc->draco = draco;
    // move cache to the head to reduce future search time
    if (prev)
        prev->next = mc->next;
    if(mc != mc_head)
        mc->next = mc_head;
    mc_head = mc;
    mc->lock_flag=true;
    return mc->idx;
}

void TileDrawTitle()
{
    img_type TitleImg = ImgLoadFileSimple("title");
    if (!TitleImg)
        return;

    int winx = win_main->wx;
    int winy = win_main->wy;

    TileRegionClass title(winx, winy, 1, 1);
    win_main->placeRegion(&title, 0, 0, 0, 0, 0, 0, 0);
    title.init_backbuf();
    img_type pBuf = title.backbuf;

    int tx  = ImgWidth(TitleImg);
    int ty  = ImgHeight(TitleImg);

    int x, y;

    if (tx > winx)
    {
        x = 0;
        tx = winx;
    }
    else
    {
        x = (winx - tx)/2;
    }

    if (ty > winy)
    {
        y = 0;
        ty = winy;
    }
    else
    {
        y = (winy - ty)/2;
    }

    ImgCopy(TitleImg, 0, 0, tx, ty, pBuf, x, y, 1);
    title.make_active();
    title.redraw();
    ImgDestroy(TitleImg);

    getch();
    clrscr();

    win_main->removeRegion(&title);
}

static void TilePutch(int c, img_type Dest, int dx, int dy)
{
    int tidx = TILE_CHAR00 + (c-32)/8;
    int tidx2 = c & 7;

    int sx = (tidx % TILE_PER_ROW)*TILE_X + (tidx2 % 4)*(TILE_X/4);
    int sy = (tidx / TILE_PER_ROW)*TILE_Y + (tidx2 / 4)*(TILE_Y/2);;

    ImgCopy(TileImg, sx, sy, TILE_X/4, TILE_Y/2,
              Dest, dx, dy, 0);
}

void TileRedrawInv(int region)
{
    TileRegionClass *r = (region == REGION_INV1) ? region_item:region_item2;
    r->flag = true;
    r->make_active();
    r->redraw();
}

void TileClearInv(int region)
{
    TileRegionClass *r = (region == REGION_INV1) ? region_item:region_item2;

    for (int i = 0; i < r->mx * r->my; i++)
    {
        TileDrawOneItem(region, i, 0, -1, -1, -1, false, false, false, false, false);
    }
    last_cursor = -1;
    itemlist_n = 0;
}

void TileDrawOneItem(int region, int i, char key, int idx,
    int tile, int num, bool floor,
    bool select, bool equip, bool tried, bool cursed)
{
    ASSERT(idx >= -1 && idx < MAX_ITEMS);
    TileRegionClass *r = (region == REGION_INV1) ? region_item:region_item2;

    int item_x = r->mx;
    int dx = (i % item_x)*TILE_X;
    int dy = (i / item_x)*TILE_Y;

    if (tile == -1)
    {
        ImgCopyFromTileImg(TILE_DNGN_UNSEEN, r->backbuf, dx, dy, 1);
        return;
    }

    if (floor)
        ImgCopyFromTileImg(TILE_DNGN_FLOOR, r->backbuf, dx, dy, 1);
    else
        ImgCopyFromTileImg(TILE_ITEM_SLOT, r->backbuf, dx, dy, 1);

    if (equip)
    {
        if (cursed)
            ImgCopyFromTileImg(TILE_ITEM_SLOT_EQUIP_CURSED, r->backbuf, 
                dx, dy, 0);
        else
            ImgCopyFromTileImg(TILE_ITEM_SLOT_EQUIP, r->backbuf, dx, dy, 0);
    }
    else if (cursed)
    {
        ImgCopyFromTileImg(TILE_ITEM_SLOT_CURSED, r->backbuf, dx, dy, 0);
    }
    if (select)
        ImgCopyFromTileImg(TILE_RAY_MESH, r->backbuf, dx, dy, 0);

    if (itemlist_iflag[i] & TILEI_FLAG_CURSOR)
        ImgCopyFromTileImg(TILE_CURSOR, r->backbuf, dx, dy, 0);

    // Item tile
    ImgCopyFromTileImg(tile, r->backbuf, dx, dy, 0);

    // quantity/charge
    if (num != -1)
    {
        // If you have that many, who cares.
        if (num > 999)
            num = 999;

        const int offset_amount = TILE_X/4;
        int offset = 0;
        int help = num;

        int c100 = help/100;
        help -= c100*100;
        if (c100)
        {
            TilePutch('0' + c100, r->backbuf, dx+offset, dy);
            offset += offset_amount;
        }
        int c10 = help/10;
        if (c10 || c100)
        {
            TilePutch('0' + c10, r->backbuf, dx+offset, dy);
            offset += offset_amount;
        }
        int c1 = help % 10;
        TilePutch('0' + c1, r->backbuf, dx+offset, dy);
    }

    // '?' mark
    if (tried)
    {
        TilePutch('?', r->backbuf, dx, dy + TILE_Y/2);
    }

    // record tile information as we draw it so that we can re-draw it at will
    itemlist[i] = tile;
    itemlist_num[i] = num;
    itemlist_key[i] = key;
    itemlist_idx[i] = idx;
    itemlist_iflag[i] = 0;
    if (floor)
        itemlist_iflag[i] |= TILEI_FLAG_FLOOR;
    if (tried)
        itemlist_iflag[i] |= TILEI_FLAG_TRIED;
    if (equip)
        itemlist_iflag[i] |= TILEI_FLAG_EQUIP;
    if (cursed)
        itemlist_iflag[i] |= TILEI_FLAG_CURSE;
    if (select)
        itemlist_iflag[i] |= TILEI_FLAG_SELECT;

    if (i >= itemlist_n)
        itemlist_n = i+1;
}

void TileDrawInvData(int n, int flag, int *tiles, int *num, int *idx,
    int *iflags)
{
    int i;
    TileRegionClass *r = (flag == REGION_INV1) ? region_item:region_item2;

    r->flag = true;

    last_cursor = -1;
    int old_itemlist_n = itemlist_n;
    itemlist_n = n;

    int item_x = r->mx;
    int item_y = r->my;

    for (i=0;i<item_x*item_y;i++)
    {
        if (i==MAX_ITEMLIST) break;

        int tile0 = (i>=n) ? -1 : tiles[i];
        int idx0  = (i>=n) ? -1 : idx[i];
        char key  = (iflags[i]&TILEI_FLAG_FLOOR) ? 0 : index_to_letter(idx[i]);

        if ( flag == itemlist_flag
             && tile0  == itemlist[i]
             && num[i] == itemlist_num[i]
             && key    == itemlist_key[i]
             && idx0   == itemlist_idx[i]
             && iflags[i] == itemlist_iflag[i]
             && !force_redraw_inv
             && i < old_itemlist_n)
        {
            continue;
        }

        TileDrawOneItem(flag, i, key, idx0, tile0, num[i],
            ((iflags[i]&TILEI_FLAG_FLOOR) != 0),
            ((iflags[i]&TILEI_FLAG_SELECT) != 0),
            ((iflags[i]&TILEI_FLAG_EQUIP) != 0),
            ((iflags[i]&TILEI_FLAG_TRIED) != 0),
            ((iflags[i]&TILEI_FLAG_CURSE) != 0));
    }

    r->make_active();
    r->redraw();
    itemlist_flag = flag;
    force_redraw_inv = false;
}

void TileDrawInvCursor(int ix, bool flag)
{
    TileRegionClass *r =
        (itemlist_flag == REGION_INV1) ? region_item:region_item2;

    int tile0 = itemlist[ix];
    int num0  = itemlist_num[ix];

    if (flag)
        itemlist_iflag[ix] |= TILEI_FLAG_CURSOR;
    else
        itemlist_iflag[ix] &= ~TILEI_FLAG_CURSOR;

    TileDrawOneItem(itemlist_flag, ix, itemlist_key[ix], itemlist_idx[ix], tile0, num0,
            ((itemlist_iflag[ix]&TILEI_FLAG_FLOOR) != 0),
            ((itemlist_iflag[ix]&TILEI_FLAG_SELECT) != 0),
            ((itemlist_iflag[ix]&TILEI_FLAG_EQUIP) != 0),
            ((itemlist_iflag[ix]&TILEI_FLAG_TRIED) != 0),
            ((itemlist_iflag[ix]&TILEI_FLAG_CURSE) != 0)
            );

    r->redraw();
}

void TileMoveInvCursor(int ix)
{
    if (last_cursor != -1)
        TileDrawInvCursor(last_cursor, false);

    if (ix != -1) TileDrawInvCursor(ix, true);
    last_cursor = ix;
}

int TileInvIdx(int i)
{
    if (i >= itemlist_n)
        return -1;
    else
        return itemlist_idx[i];
}

#endif
