#ifdef USE_TILE

#include "tiledef.h"
#include "beam.h"

//*tile1.cc: get data from core part and drives tile drawing codes

//**convert in-game data to tile index
int tileidx(unsigned int object, int extra);
int tileidx_feature(int object);
int tileidx_player(int job);
int tileidx_unseen(int ch, const coord_def& gc);
int tileidx_item(const item_def &item);
int tileidx_item_throw(const item_def &item, int dx, int dy);
int tileidx_bolt(const bolt &bolt);
int tileidx_zap(int color);
int tile_idx_unseen_terrain(int x, int y, int what);
int tile_unseen_flag(const coord_def& gc);

// Player tile related
void tilep_race_default(int race, int gender, int level, int *parts);
void tilep_job_default(int job, int gender, int *parts);
void tilep_calc_flags(int parts[], int flag[]);

void tilep_part_to_str(int number, char *buf);
int  tilep_str_to_part(char *str);

void tilep_scan_parts(char *fbuf, int *parts);
void tilep_print_parts(char *fbuf, int *parts);

int tilep_equ_weapon(const item_def &item);
int tilep_equ_shield(const item_def &item);
int tilep_equ_armour(const item_def &item);
int tilep_equ_cloak(const item_def &item);
int tilep_equ_helm(const item_def &item);
int tilep_equ_gloves(const item_def &item);
int tilep_equ_boots(const item_def &item);

// Tile display related
void tile_draw_floor();
void tile_place_monster(int gx, int gy, int idx, bool foreground = true,
                        bool detected = false);
void tile_place_item(int x, int y, int idx);
void tile_place_item_bk(int gx, int gy, int idx);
void tile_place_tile_bk(int gx, int gy, int tileidx);
void tile_place_item_marker(int x, int y, int idx);
void tile_place_cloud(int x, int y, int type, int decay);
void tile_place_ray(const coord_def& gc);
void tile_draw_rays(bool resetCount);
void tile_clear_buf();

void tile_finish_dngn(unsigned int *tileb, int cx, int cy);
void tile_draw_dungeon(unsigned int *tileb);

// Tile Inventry display
void tile_draw_inv(int item_type = -1, int flag = -1);
// Multiple pickup
void tile_pick_menu();

int get_num_wall_flavors();
int get_num_floor_flavors();
int get_num_floor_special_flavors();
int get_wall_tile_idx();
int get_floor_tile_idx();
int get_floor_special_tile_idx();
void tile_init_flavor();

void tile_set_force_redraw_tiles(bool redraw);
void tile_set_force_redraw_inv(bool redraw);

/**************************************/
/* tile2.cc  image manipulation       */
/**************************************/
// init them all
void TileInit();

void TileResizeScreen(int x, int y);

// display tile cursor, returns old cursor value there
int TileDrawCursor(int x, int y, int flag);
// display bolts
void TileDrawBolt(int x, int y, int fg);
// display dungeon: tileb = { fg(0,0),bg(0,0),fg(1,0),bg(1,0), ..
void TileDrawDungeon(unsigned int *tileb);
// display memorized dungeon
void TileDrawFarDungeon(int cx, int cy);
// display map centered on grid coords
void TileDrawMap(int gx, int gy);
void LoadDungeonView(unsigned int *tileb);
void StoreDungeonView(unsigned int *tileb);

void TileNewLevel(bool first_time);


// display inventry
void TileDrawInvData(int n, int flag, int *tiles, int *num,
                     int *idx, int *iflags);
void TileDrawOneItem(int region, int i, char key, int idx,
                     int tile, int num, bool floor,
                     bool select, bool equip, bool tried, bool cursed);
void TileRedrawInv(int region);
void TileClearInv(int region);
void TileMoveInvCursor(int ix);
int TileInvIdx(int i);

// refresh player tile
void TilePlayerRefresh();
// edit player tile
void TilePlayerEdit();
// init player tile
void TilePlayerInit();
// init ghost tile
void TileGhostInit(const struct ghost_demon &gs);
// init pandem demon tile (only in iso mode)
void TilePandemInit(struct ghost_demon &gs);
// edit pandem tile (debug)
void TileEditPandem();
// init id-ed item tiles
void TileInitItems();

// load wall tiles
void TileLoadWall(bool wizard);

void TileDrawTitle();

// monster+weapon tile
void TileMcacheUnlock();
int  TileMcacheFind(int mon_tile, int eq_tile, int draco = 0);
int get_base_idx_from_mcache(int tile_idx);
int get_clean_map_idx(int tile_idx);

/* Flags for drawing routines */
enum tile_flags
{
    // Foreground flags
    TILE_FLAG_S_UNDER   = 0x00000800,
    TILE_FLAG_FLYING    = 0x00001000,
    TILE_FLAG_NET       = 0x00002000,
    TILE_FLAG_PET       = 0x00004000,
    TILE_FLAG_STAB      = 0x00008000,
    TILE_FLAG_MAY_STAB  = 0x0000C000,
    TILE_FLAG_POISON    = 0x00010000,

    // Background flags
    TILE_FLAG_RAY       = 0x00000800,
    TILE_FLAG_MM_UNSEEN = 0x00001000,
    TILE_FLAG_UNSEEN    = 0x00002000,
    TILE_FLAG_CURSOR0   = 0x00000000,
    TILE_FLAG_CURSOR1   = 0x00008000,
    TILE_FLAG_CURSOR2   = 0x00004000,
    TILE_FLAG_CURSOR3   = 0x0000C000,
    TILE_FLAG_CURSOR    = 0x0000C000,
    TILE_FLAG_BLOOD     = 0x00010000,
    TILE_FLAG_NEW_STAIR = 0x00020000,
    TILE_FLAG_TRAVEL_EX = 0x00040000,
    TILE_FLAG_SANCTUARY = 0x00080000,

    // General
    TILE_FLAG_MASK      = 0x000007FF
};


#define TILEI_FLAG_SELECT 0x100
#define TILEI_FLAG_TRIED  0x200
#define TILEI_FLAG_EQUIP  0x400
#define TILEI_FLAG_FLOOR  0x800
#define TILEI_FLAG_CURSE  0x1000
#define TILEI_FLAG_CURSOR 0x2000

#define TILEP_SHOW_EQUIP  0x1000

#define TILEP_GENDER_MALE   0
#define TILEP_GENDER_FEMALE 1

#define TILEP_M_DEFAULT   0
#define TILEP_M_LOADING   1

enum TilePlayerFlagCut
{
    TILEP_FLAG_HIDE,
    TILEP_FLAG_NORMAL,
    TILEP_FLAG_CUT_CENTAUR,
    TILEP_FLAG_CUT_NAGA
};

#ifdef TILEP_DEBUG
const char *get_ctg_name(int part);
int get_ctg_idx(char *name);
const char *get_parts_name(int part, int idx);
int get_parts_idx(int part, char *name);
#endif

// Dungeon view window size
#define TILE_DAT_XMAX 35
#define TILE_DAT_YMAX 35
// normal tile size in px
#define TILE_X 32
#define TILE_Y 32

// Unit size
// iso mode screen size
#define TILE_XMAX_ISO 24
#define TILE_YMAX_ISO 13
// iso tile size in px
#define TILE_X_EX_ISO 64
#define TILE_Y_EX_ISO 64

// iso mode  unit grid size in px
#define TILE_UX_ISO (TILE_X_EX_ISO/2)
#define TILE_UY_ISO (TILE_X_EX_ISO/2)

// screen size in grids
#define TILE_XMAX_NORMAL 17
#define TILE_YMAX_NORMAL 17
// grid size in px
#define TILE_UX_NORMAL TILE_X
#define TILE_UY_NORMAL TILE_Y

#define TILEP_BOOTS_NAGA_BARDING (N_PART_BOOTS + 1)
#define TILEP_BOOTS_CENTAUR_BARDING (N_PART_BOOTS + 2)

#endif // USE_TILES
