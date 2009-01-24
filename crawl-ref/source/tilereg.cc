/*
 *  File:       tilereg.cc
 *  Summary:    Region system implementations
 *
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "cio.h"
#include "debug.h"
#include "describe.h"
#include "food.h"
#include "itemname.h"
#include "it_use2.h"
#include "item_use.h"
#include "message.h"
#include "misc.h"
#include "menu.h"
#include "newgame.h"
#include "mon-util.h"
#include "player.h"
#include "spells3.h"
#include "stuff.h"
#include "terrain.h"
#include "transfor.h"
#include "travel.h"
#include "view.h"

#include "tilereg.h"
#include "tiles.h"
#include "tilefont.h"
#include "tilesdl.h"
#include "tilemcache.h"

#include <SDL_opengl.h>

coord_def Region::NO_CURSOR(-1, -1);

int TextRegion::print_x;
int TextRegion::print_y;
TextRegion *TextRegion::text_mode = NULL;
int TextRegion::text_col = 0;

TextRegion *TextRegion::cursor_region= NULL;
int TextRegion::cursor_flag = 0;
int TextRegion::cursor_x;
int TextRegion::cursor_y;

const VColour map_colours[MAX_MAP_COL] =
{
    VColour(  0,   0,   0, 255), // BLACK
    VColour(128, 128, 128, 255), // DKGREY
    VColour(160, 160, 160, 255), // MDGREY
    VColour(192, 192, 192, 255), // LTGREY
    VColour(255, 255, 255, 255), // WHITE

    VColour(  0,  64, 255, 255), // BLUE  (actually cyan-blue)
    VColour(128, 128, 255, 255), // LTBLUE
    VColour(  0,  32, 128, 255), // DKBLUE (maybe too dark)

    VColour(  0, 255,   0, 255), // GREEN
    VColour(128, 255, 128, 255), // LTGREEN
    VColour(  0, 128,   0, 255), // DKGREEN

    VColour(  0, 255, 255, 255), // CYAN
    VColour( 64, 255, 255, 255), // LTCYAN (maybe too pale)
    VColour(  0, 128, 128, 255), // DKCYAN

    VColour(255,   0,   0, 255), // RED
    VColour(255, 128, 128, 255), // LTRED (actually pink)
    VColour(128,   0,   0, 255), // DKRED

    VColour(192,   0, 255, 255), // MAGENTA (actually blue-magenta)
    VColour(255, 128, 255, 255), // LTMAGENTA
    VColour( 96,   0, 128, 255), // DKMAGENTA

    VColour(255, 255,   0, 255), // YELLOW
    VColour(255, 255,  64, 255), // LTYELLOW (maybe too pale)
    VColour(128, 128,   0, 255), // DKYELLOW

    VColour(165,  91,   0, 255), // BROWN
};

const int dir_dx[9] = {-1, 0, 1, -1, 0, 1, -1, 0, 1};
const int dir_dy[9] = {1, 1, 1, 0, 0, 0, -1, -1, -1};

const int cmd_normal[9] = {'b', 'j', 'n', 'h', '.', 'l', 'y', 'k', 'u'};
const int cmd_shift[9] = {'B', 'J', 'N', 'H', '5', 'L', 'Y', 'K', 'U'};
const int cmd_ctrl[9] = {CONTROL('B'), CONTROL('J'), CONTROL('N'),
                      CONTROL('H'), 'X', CONTROL('L'),
                      CONTROL('Y'), CONTROL('K'), CONTROL('U')};
const int cmd_dir[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};

Region::Region() :
    ox(0),
    oy(0),
    dx(0),
    dy(0),
    mx(0),
    my(0),
    wx(0),
    wy(0),
    sx(0),
    sy(0),
    ex(0),
    ey(0)
{
}

void Region::resize(int _mx, int _my)
{
    mx = _mx;
    my = _my;

    recalculate();
}

void Region::place(int _sx, int _sy, int margin)
{
    sx = _sx;
    sy = _sy;
    ox = margin;
    oy = margin;

    recalculate();
}

void Region::resize_to_fit(int _wx, int _wy)
{
    if (_wx < 0 || _wy < 0)
    {
        mx = wx = my = wy = 0;
        ey = sy;
        ex = sy;
        return;
    }

    int inner_x = _wx - 2 * ox;
    int inner_y = _wy - 2 * oy;

    mx = dx ? inner_x / dx : 0;
    my = dy ? inner_y / dy : 0;

    recalculate();

    ex = sx + _wx;
    ey = sy + _wy;
}

void Region::recalculate()
{
    wx = ox * 2 + mx * dx;
    wy = oy * 2 + my * dy;
    ex = sx + wx;
    ey = sy + wy;

    on_resize();
}

Region::~Region()
{
}

bool Region::inside(int x, int y)
{
    return (x >= sx && y >= sy && x <= ex && y <= ey);
}

bool Region::mouse_pos(int mouse_x, int mouse_y, int &cx, int &cy)
{
    int x = mouse_x - ox - sx;
    int y = mouse_y - oy - sy;

    bool valid = (x >= 0 && y >= 0);

    ASSERT(dx > 0);
    ASSERT(dy > 0);
    x /= dx;
    y /= dy;
    valid &= (x < mx && y < my);

    cx = x;
    cy = y;

    return valid;
}

void Region::set_transform()
{
    glLoadIdentity();
    glTranslatef(sx + ox, sy + oy, 0);
    glScalef(dx, dy, 1);
}

TileRegion::TileRegion(ImageManager* im, FTFont *tag_font, int tile_x, int tile_y)
{
    ASSERT(im);
    ASSERT(tag_font);

    m_image = im;
    dx = tile_x;
    dy = tile_y;
    m_tag_font = tag_font;
    m_dirty = true;
}

TileRegion::~TileRegion()
{
}

DungeonRegion::DungeonRegion(ImageManager* im, FTFont *tag_font,
                             int tile_x, int tile_y) :
    TileRegion(im, tag_font, tile_x, tile_y),
    m_cx_to_gx(0),
    m_cy_to_gy(0),
    m_buf_dngn(&im->m_textures[TEX_DUNGEON]),
    m_buf_doll(&im->m_textures[TEX_DOLL]),
    m_buf_main(&im->m_textures[TEX_DEFAULT])
{
    for (int i = 0; i < CURSOR_MAX; i++)
        m_cursor[i] = NO_CURSOR;
}

DungeonRegion::~DungeonRegion()
{
}

void DungeonRegion::load_dungeon(unsigned int* tileb, int cx_to_gx, int cy_to_gy)
{
    m_tileb.clear();
    m_dirty = true;

    if (!tileb)
        return;

    int len = 2 * crawl_view.viewsz.x * crawl_view.viewsz.y;
    m_tileb.resize(len);
    // TODO enne - move this function into dungeonregion
    tile_finish_dngn(tileb, cx_to_gx + mx/2, cy_to_gy + my/2);
    memcpy(&m_tileb[0], tileb, sizeof(unsigned int) * len);

    m_cx_to_gx = cx_to_gx;
    m_cy_to_gy = cy_to_gy;

    place_cursor(CURSOR_TUTORIAL, m_cursor[CURSOR_TUTORIAL]);
}

void DungeonRegion::pack_background(unsigned int bg, int x, int y)
{
    unsigned int bg_idx = bg & TILE_FLAG_MASK;
    if (bg_idx >= TILE_DNGN_WAX_WALL)
    {
        tile_flavour &flv = env.tile_flv[x + m_cx_to_gx][y + m_cy_to_gy];
        m_buf_dngn.add(flv.floor, x, y);
    }

    if (bg & TILE_FLAG_BLOOD)
    {
        tile_flavour &flv = env.tile_flv[x + m_cx_to_gx][y + m_cy_to_gy];
        int offset = flv.special % tile_dngn_count(TILE_BLOOD);
        m_buf_dngn.add(TILE_BLOOD + offset, x, y);
    }

    m_buf_dngn.add(bg_idx, x, y);

    if (bg & TILE_FLAG_HALO)
        m_buf_dngn.add(TILE_HALO, x, y);

    if (bg & TILE_FLAG_SANCTUARY && !(bg & TILE_FLAG_UNSEEN))
        m_buf_dngn.add(TILE_SANCTUARY, x, y);

    // Apply the travel exclusion under the foreground if the cell is
    // visible.  It will be applied later if the cell is unseen.
    if (bg & TILE_FLAG_EXCL_CTR && !(bg & TILE_FLAG_UNSEEN))
        m_buf_dngn.add(TILE_TRAVEL_EXCLUSION_CENTRE_BG, x, y);
    else if (bg & TILE_FLAG_TRAV_EXCL && !(bg & TILE_FLAG_UNSEEN))
        m_buf_dngn.add(TILE_TRAVEL_EXCLUSION_BG, x, y);

    if (bg & TILE_FLAG_RAY)
        m_buf_dngn.add(TILE_RAY, x, y);
    else if (bg & TILE_FLAG_RAY_OOR)
        m_buf_dngn.add(TILE_RAY_OUT_OF_RANGE, x, y);
}

void DungeonRegion::pack_player(int x, int y)
{
    dolls_data default_doll;
    dolls_data player_doll;
    dolls_data result;

    for (int i = 0; i < TILEP_PART_MAX; i++)
        default_doll.parts[i] = TILEP_SHOW_EQUIP;

    int gender = you.your_name[0] % 2;

    tilep_race_default(you.species, gender, you.experience_level,
                       default_doll.parts);

    result = default_doll;

    result.parts[TILEP_PART_BASE]    = default_doll.parts[TILEP_PART_BASE];
    result.parts[TILEP_PART_DRCHEAD] = default_doll.parts[TILEP_PART_DRCHEAD];
    result.parts[TILEP_PART_DRCWING] = default_doll.parts[TILEP_PART_DRCWING];

    const bool halo = inside_halo(you.pos());
    result.parts[TILEP_PART_HALO] = halo ? TILEP_HALO_TSO : 0;
    result.parts[TILEP_PART_ENCH] =
        (you.duration[DUR_LIQUID_FLAMES] ? TILEP_ENCH_STICKY_FLAME : 0);

    if (result.parts[TILEP_PART_HAND1] == TILEP_SHOW_EQUIP)
    {
        int item = you.equip[EQ_WEAPON];
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
            result.parts[TILEP_PART_HAND1] = TILEP_HAND1_BLADEHAND;
        else if (item == -1)
            result.parts[TILEP_PART_HAND1] = 0;
        else
            result.parts[TILEP_PART_HAND1] = tilep_equ_weapon(you.inv[item]);
    }
    if (result.parts[TILEP_PART_HAND2] == TILEP_SHOW_EQUIP)
    {
        int item = you.equip[EQ_SHIELD];
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
            result.parts[TILEP_PART_HAND2] = TILEP_HAND2_BLADEHAND;
        else if (item == -1)
            result.parts[TILEP_PART_HAND2] = 0;
        else
            result.parts[TILEP_PART_HAND2] = tilep_equ_shield(you.inv[item]);
    }
    if (result.parts[TILEP_PART_BODY] == TILEP_SHOW_EQUIP)
    {
        int item = you.equip[EQ_BODY_ARMOUR];
        if (item == -1)
            result.parts[TILEP_PART_BODY] = 0;
        else
            result.parts[TILEP_PART_BODY] = tilep_equ_armour(you.inv[item]);
    }
    if (result.parts[TILEP_PART_CLOAK] == TILEP_SHOW_EQUIP)
    {
        int item = you.equip[EQ_CLOAK];
        if (item == -1)
            result.parts[TILEP_PART_CLOAK] = 0;
        else
            result.parts[TILEP_PART_CLOAK] = tilep_equ_cloak(you.inv[item]);
    }
    if (result.parts[TILEP_PART_HELM] == TILEP_SHOW_EQUIP)
    {
        int item = you.equip[EQ_HELMET];
        if (item != -1)
        {
            result.parts[TILEP_PART_HELM] = tilep_equ_helm(you.inv[item]);
        }
        else if (player_mutation_level(MUT_HORNS) > 0)
        {
            switch (player_mutation_level(MUT_HORNS))
            {
                case 1:
                    result.parts[TILEP_PART_HELM] = TILEP_HELM_HORNS1;
                    break;
                case 2:
                    result.parts[TILEP_PART_HELM] = TILEP_HELM_HORNS2;
                    break;
                case 3:
                    result.parts[TILEP_PART_HELM] = TILEP_HELM_HORNS3;
                    break;
            }
        }
        else
        {
            result.parts[TILEP_PART_HELM] = 0;
        }
    }

    if (result.parts[TILEP_PART_BOOTS] == TILEP_SHOW_EQUIP)
    {
        int item = you.equip[EQ_BOOTS];
        if (item != -1)
            result.parts[TILEP_PART_BOOTS] = tilep_equ_boots(you.inv[item]);
        else if (player_mutation_level(MUT_HOOVES))
            result.parts[TILEP_PART_BOOTS] = TILEP_BOOTS_HOOVES;
        else
            result.parts[TILEP_PART_BOOTS] = 0;
    }

    if (result.parts[TILEP_PART_ARM] == TILEP_SHOW_EQUIP)
    {
        int item = you.equip[EQ_GLOVES];
        if (item != -1)
            result.parts[TILEP_PART_ARM] = tilep_equ_gloves(you.inv[item]);
        else if (player_mutation_level(MUT_CLAWS) >= 3
            || you.species == SP_TROLL || you.species == SP_GHOUL)
        {
            // There is player_has_claws() but it is not equivalent.
            // Claws appear if they're big enough to not wear gloves
            // or on races that have claws.
            result.parts[TILEP_PART_ARM] = TILEP_ARM_CLAWS;
        }
        else
            result.parts[TILEP_PART_ARM] = 0;
    }

    if (result.parts[TILEP_PART_LEG] == TILEP_SHOW_EQUIP)
        result.parts[TILEP_PART_LEG] = 0;
    if (result.parts[TILEP_PART_DRCWING] == TILEP_SHOW_EQUIP)
        result.parts[TILEP_PART_DRCWING] = 0;
    if (result.parts[TILEP_PART_DRCHEAD] == TILEP_SHOW_EQUIP)
        result.parts[TILEP_PART_DRCHEAD] = 0;

    pack_doll(result, x, y);
}

void DungeonRegion::pack_doll(const dolls_data &doll, int x, int y)
{
    int p_order[TILEP_PART_MAX] =
    {
        TILEP_PART_SHADOW,  //  0
        TILEP_PART_HALO,
        TILEP_PART_ENCH,
        TILEP_PART_DRCWING,
        TILEP_PART_CLOAK,
        TILEP_PART_BASE,    //  5
        TILEP_PART_BOOTS,
        TILEP_PART_LEG,
        TILEP_PART_BODY,
        TILEP_PART_ARM,
        TILEP_PART_HAND1,   // 10
        TILEP_PART_HAND2,
        TILEP_PART_HAIR,
        TILEP_PART_BEARD,
        TILEP_PART_HELM,
        TILEP_PART_DRCHEAD // 15
    };

    int flags[TILEP_PART_MAX];
    tilep_calc_flags(doll.parts, flags);

    // For skirts, boots go under the leg armour.  For pants, they go over.
    if (doll.parts[TILEP_PART_LEG] < TILEP_LEG_SKIRT_OFS)
    {
        p_order[6] = TILEP_PART_BOOTS;
        p_order[7] = TILEP_PART_LEG;
    }

    for (int i = 0; i < TILEP_PART_MAX; i++)
    {
        int p = p_order[i];
        if (!doll.parts[p])
            continue;

        int ymax = TILE_Y;

        if (flags[p] == TILEP_FLAG_CUT_CENTAUR
            || flags[p] == TILEP_FLAG_CUT_NAGA)
        {
            ymax = 18;
        }

        m_buf_doll.add(doll.parts[p], x, y, 0, 0, true, ymax);
    }
}

void DungeonRegion::pack_mcache(mcache_entry *entry, int x, int y)
{
    ASSERT(entry);

    const dolls_data *doll = entry->doll();
    if (doll)
        pack_doll(*doll, x, y);

    tile_draw_info dinfo[3];
    unsigned int draw_info_count = entry->info(&dinfo[0]);
    ASSERT(draw_info_count <= sizeof(dinfo) / (sizeof(dinfo[0])));

    for (unsigned int i = 0; i < draw_info_count; i++)
    {
        m_buf_doll.add(dinfo[i].idx, x, y, dinfo[i].ofs_x, dinfo[i].ofs_y);
    }
}

void DungeonRegion::pack_foreground(unsigned int bg, unsigned int fg, int x, int y)
{
    unsigned int fg_idx = fg & TILE_FLAG_MASK;
    unsigned int bg_idx = bg & TILE_FLAG_MASK;

    if (fg_idx && fg_idx <= TILE_MAIN_MAX)
    {
        m_buf_main.add(fg_idx, x, y);
    }

    if (fg_idx && !(fg & TILE_FLAG_FLYING))
    {
        if (tile_dngn_equal(TILE_DNGN_LAVA, bg_idx))
            m_buf_main.add(TILE_MASK_LAVA, x, y);
        else if (tile_dngn_equal(TILE_DNGN_SHALLOW_WATER, bg_idx))
            m_buf_main.add(TILE_MASK_SHALLOW_WATER, x, y);
        else if (tile_dngn_equal(TILE_DNGN_SHALLOW_WATER_MURKY, bg_idx))
            m_buf_main.add(TILE_MASK_SHALLOW_WATER_MURKY, x, y);
        else if (tile_dngn_equal(TILE_DNGN_DEEP_WATER, bg_idx))
            m_buf_main.add(TILE_MASK_DEEP_WATER, x, y);
        else if (tile_dngn_equal(TILE_DNGN_DEEP_WATER_MURKY, bg_idx))
            m_buf_main.add(TILE_MASK_DEEP_WATER_MURKY, x, y);
    }

    if (fg & TILE_FLAG_NET)
        m_buf_main.add(TILE_TRAP_NET, x, y);

    if (fg & TILE_FLAG_S_UNDER)
        m_buf_main.add(TILE_SOMETHING_UNDER, x, y);

    // Pet mark
    int status_shift = 0;
    if (fg & TILE_FLAG_PET)
    {
        m_buf_main.add(TILE_HEART, x, y);
        status_shift += 10;
    }
    else if ((fg & TILE_FLAG_MAY_STAB) == TILE_FLAG_NEUTRAL)
    {
        m_buf_main.add(TILE_NEUTRAL, x, y);
        status_shift += 8;
    }
    else if ((fg & TILE_FLAG_MAY_STAB) == TILE_FLAG_STAB)
    {
        m_buf_main.add(TILE_STAB_BRAND, x, y);
        status_shift += 8;
    }
    else if ((fg & TILE_FLAG_MAY_STAB) == TILE_FLAG_MAY_STAB)
    {
        m_buf_main.add(TILE_MAY_STAB_BRAND, x, y);
        status_shift += 5;
    }

    if (fg & TILE_FLAG_POISON)
    {
        m_buf_main.add(TILE_POISON, x, y, -status_shift, 0);
        status_shift += 5;
    }

    if (fg & TILE_FLAG_ANIM_WEP)
    {
        m_buf_main.add(TILE_ANIMATED_WEAPON, x, y);
    }

    if (bg & TILE_FLAG_UNSEEN && (bg != TILE_FLAG_UNSEEN || fg))
        m_buf_main.add(TILE_MESH, x, y);

    if (bg & TILE_FLAG_MM_UNSEEN && (bg != TILE_FLAG_MM_UNSEEN || fg))
        m_buf_main.add(TILE_MAGIC_MAP_MESH, x, y);

    // Don't let the "new stair" icon cover up any existing icons, but
    // draw it otherwise.
    if (bg & TILE_FLAG_NEW_STAIR && status_shift == 0)
        m_buf_main.add(TILE_NEW_STAIR, x, y);

    if (bg & TILE_FLAG_EXCL_CTR && (bg & TILE_FLAG_UNSEEN))
        m_buf_main.add(TILE_TRAVEL_EXCLUSION_CENTRE_FG, x, y);
    else if (bg & TILE_FLAG_TRAV_EXCL && (bg & TILE_FLAG_UNSEEN))
        m_buf_main.add(TILE_TRAVEL_EXCLUSION_FG, x, y);

    // Tutorial cursor takes precedence over other cursors.
    if (bg & TILE_FLAG_TUT_CURSOR)
    {
        m_buf_main.add(TILE_TUTORIAL_CURSOR, x, y);
    }
    else if (bg & TILE_FLAG_CURSOR)
    {
        int type = ((bg & TILE_FLAG_CURSOR) == TILE_FLAG_CURSOR1) ?
            TILE_CURSOR : TILE_CURSOR2;

        if ((bg & TILE_FLAG_CURSOR) == TILE_FLAG_CURSOR3)
           type = TILE_CURSOR3;

        m_buf_main.add(type, x, y);
    }

    if (fg & TILE_FLAG_MDAM_MASK)
    {
        unsigned int mdam_flag = fg & TILE_FLAG_MDAM_MASK;
        if (mdam_flag == TILE_FLAG_MDAM_LIGHT)
            m_buf_main.add(TILE_MDAM_LIGHTLY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_MOD)
            m_buf_main.add(TILE_MDAM_MODERATELY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_HEAVY)
            m_buf_main.add(TILE_MDAM_HEAVILY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_SEV)
            m_buf_main.add(TILE_MDAM_SEVERELY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_ADEAD)
            m_buf_main.add(TILE_MDAM_ALMOST_DEAD, x, y);
    }
}

void DungeonRegion::pack_cursor(cursor_type type, unsigned int tile)
{
    const coord_def &gc = m_cursor[type];
    if (gc == NO_CURSOR || !on_screen(gc))
        return;

    m_buf_main.add(tile, gc.x - m_cx_to_gx, gc.y - m_cy_to_gy);
}

void DungeonRegion::pack_buffers()
{
    m_buf_dngn.clear();
    m_buf_doll.clear();
    m_buf_main.clear();

    if (m_tileb.empty())
        return;

    int tile = 0;
    for (int y = 0; y < crawl_view.viewsz.y; y++)
        for (int x = 0; x < crawl_view.viewsz.x; x++)
        {
            unsigned int bg = m_tileb[tile + 1];
            unsigned int fg = m_tileb[tile];
            unsigned int fg_idx = fg & TILE_FLAG_MASK;

            pack_background(bg, x, y);

            if (fg_idx >= TILEP_MCACHE_START)
            {
                mcache_entry *entry = mcache.get(fg_idx);
                if (entry)
                    pack_mcache(entry, x, y);
                else
                    m_buf_doll.add(TILEP_MONS_UNKNOWN, x, y);
            }
            else if (fg_idx == TILEP_PLAYER)
            {
                pack_player(x, y);
            }
            else if (fg_idx >= TILE_MAIN_MAX)
            {
                m_buf_doll.add(fg_idx, x, y);
            }

            pack_foreground(bg, fg, x, y);

            tile += 2;
        }

    pack_cursor(CURSOR_TUTORIAL, TILE_TUTORIAL_CURSOR);
    pack_cursor(CURSOR_MOUSE, see_grid(m_cursor[CURSOR_MOUSE]) ? TILE_CURSOR
                                                               : TILE_CURSOR2);

    if (m_cursor[CURSOR_TUTORIAL] != NO_CURSOR
        && on_screen(m_cursor[CURSOR_TUTORIAL]))
    {
        m_buf_main.add(TILE_TUTORIAL_CURSOR,
                 m_cursor[CURSOR_TUTORIAL].x,
                 m_cursor[CURSOR_TUTORIAL].y);
    }

    for (unsigned int i = 0; i < m_overlays.size(); i++)
    {
        // overlays must be from the main image and must be in LOS.
        if (!crawl_view.in_grid_los(m_overlays[i].gc))
            continue;

        int idx = m_overlays[i].idx;
        if (idx >= TILE_MAIN_MAX)
            continue;

        int x = m_overlays[i].gc.x - m_cx_to_gx;
        int y = m_overlays[i].gc.y - m_cy_to_gy;
        m_buf_main.add(idx, x, y);
    }
}

struct tag_def
{
    tag_def() { text = NULL; left = right = 0; }

    const char* text;
    char left, right;
    char type;
};

void DungeonRegion::render()
{
    if (m_dirty)
    {
        pack_buffers();
        m_dirty = false;
    }

    set_transform();
    m_buf_dngn.draw();
    m_buf_doll.draw();
    m_buf_main.draw();

    if (you.duration[DUR_BERSERKER])
    {
        ShapeBuffer buff;
        VColour red_film(130, 0, 0, 100);
        buff.add(0, 0, mx, my, red_film);
        buff.draw();
    }

    FixedArray<tag_def, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER> tag_show;

    int total_tags = 0;

    for (int t = TAG_MAX - 1; t >= 0; t--)
    {
        for (unsigned int i = 0; i < m_tags[t].size(); i++)
        {
            if (!crawl_view.in_grid_los(m_tags[t][i].gc))
                continue;

            const coord_def ep = grid2show(m_tags[t][i].gc);

            if (tag_show(ep).text)
                continue;

            const char *str = m_tags[t][i].tag.c_str();

            int width = m_tag_font->string_width(str);
            tag_def &def = tag_show(ep);

            const int buffer = 2;

            def.left = -width / 2 - buffer;
            def.right = width / 2 + buffer;
            def.text = str;
            def.type = t;

            total_tags++;
        }

        if (total_tags)
            break;
    }

    if (!total_tags)
        return;

    // Draw text tags.
    // TODO enne - be more intelligent about not covering stuff up
    for (int y = 0; y < ENV_SHOW_DIAMETER; y++)
    {
        for (int x = 0; x < ENV_SHOW_DIAMETER; x++)
        {
            coord_def ep(x, y);
            tag_def &def = tag_show(ep);

            if (!def.text)
                continue;

            const coord_def gc = show2grid(ep);
            coord_def pc;
            to_screen_coords(gc, pc);
            // center this coord, which is at the top left of gc's cell
            pc.x += dx / 2;

            const coord_def min_pos(sx, sy);
            const coord_def max_pos(ex, ey);
            m_tag_font->render_string(pc.x, pc.y, def.text,
                                      min_pos, max_pos, WHITE, false);
        }
    }
}

void DungeonRegion::clear()
{
    m_tileb.clear();
}

void DungeonRegion::on_resize()
{
    // TODO enne
}

static int _adjacent_cmd(const coord_def &gc, const MouseEvent &event)
{
    coord_def dir = gc - you.pos();
    for (int i = 0; i < 9; i++)
    {
        if (dir_dx[i] != dir.x || dir_dy[i] != dir.y)
            continue;

        if (event.mod & MOD_SHIFT)
            return cmd_shift[i];
        else if (event.mod & MOD_CTRL)
            return cmd_ctrl[i];
        else
            return cmd_normal[i];
    }

    return 0;
}

static int _click_travel(const coord_def &gc, MouseEvent &event)
{
    if (!in_bounds(gc))
        return 0;

    int cmd = _adjacent_cmd(gc, event);
    if (cmd)
        return cmd;

    if (i_feel_safe())
    {
        start_travel(gc);
        return CK_MOUSE_CMD;
    }

    // If not safe, then take one step towards the click.
    travel_pathfind tp;
    tp.set_src_dst(you.pos(), gc);
    const coord_def dest = tp.pathfind(RMODE_TRAVEL);

    if (!dest.x && !dest.y)
        return 0;

    return _adjacent_cmd(dest, event);
}

int DungeonRegion::handle_mouse(MouseEvent &event)
{
    tiles.clear_text_tags(TAG_CELL_DESC);

    if (!inside(event.px, event.py))
        return 0;

    if (mouse_control::current_mode() == MOUSE_MODE_NORMAL
        || mouse_control::current_mode() == MOUSE_MODE_MACRO
        || mouse_control::current_mode() == MOUSE_MODE_MORE)
    {
        return 0;
    }

    int cx;
    int cy;

    bool on_map = mouse_pos(event.px, event.py, cx, cy);

    const coord_def gc(cx + m_cx_to_gx, cy + m_cy_to_gy);
    tiles.place_cursor(CURSOR_MOUSE, gc);

    if (event.event == MouseEvent::MOVE)
    {
        const std::string &desc = get_terse_square_desc(gc);
        // Suppress floor description
        if (desc != "floor")
            tiles.add_text_tag(TAG_CELL_DESC, desc, gc);
    }

    if (!on_map)
        return 0;

    if (mouse_control::current_mode() == MOUSE_MODE_TARGET
        || mouse_control::current_mode() == MOUSE_MODE_TARGET_DIR)
    {
        if (event.event == MouseEvent::MOVE)
        {
            return CK_MOUSE_MOVE;
        }
        else if (event.event == MouseEvent::PRESS
                 && event.button == MouseEvent::LEFT && on_screen(gc))
        {
            return CK_MOUSE_CLICK;
        }

        return 0;
    }

    if (event.event != MouseEvent::PRESS)
        return 0;

    if (you.pos() == gc)
    {
        switch (event.button)
        {
        case MouseEvent::LEFT:
            if (!(event.mod & MOD_SHIFT))
                return 'g';

            switch (grid_stair_direction(grd(gc)))
            {
                case CMD_GO_DOWNSTAIRS:
                    return ('>');
                case CMD_GO_UPSTAIRS:
                    return ('<');
                default:
                    return 0;
            }

        case MouseEvent::RIGHT:
            if (!(event.mod & MOD_SHIFT))
                return '%'; // Character overview.
            if (you.religion != GOD_NO_GOD)
                return '^'; // Religion screen.

            // fall through...
        default:
            return 0;
        }

    }
    // else not on player...
    if (event.button == MouseEvent::RIGHT)
    {
        full_describe_square(gc);
        return CK_MOUSE_CMD;
    }

    if (event.button != MouseEvent::LEFT)
        return 0;

    return _click_travel(gc, event);
}

void DungeonRegion::to_screen_coords(const coord_def &gc, coord_def &pc) const
{
    int cx = gc.x - m_cx_to_gx;
    int cy = gc.y - m_cy_to_gy;

    pc.x = sx + ox + cx * dx;
    pc.y = sy + oy + cy * dy;
}

bool DungeonRegion::on_screen(const coord_def &gc) const
{
    int x = gc.x - m_cx_to_gx;
    int y = gc.y - m_cy_to_gy;

    return (x >= 0 && x < mx && y >= 0 && y < my);
}

// Returns the index into m_tileb for the foreground tile.
// This value may not be valid.  Check on_screen() first.
// Add one to the return value to get the background tile idx.
int DungeonRegion::get_buffer_index(const coord_def &gc)
{
    int x = gc.x - m_cx_to_gx;
    int y = gc.y - m_cy_to_gy;
    return 2 * (x + y * mx);
}

void DungeonRegion::place_cursor(cursor_type type, const coord_def &gc)
{
    // If we're only looking for a direction, put the mouse
    // cursor next to the player to let them know that their
    // spell/wand will only go one square.
    if (mouse_control::current_mode() == MOUSE_MODE_TARGET_DIR
        && type == CURSOR_MOUSE && gc != NO_CURSOR)
    {
        coord_def delta = gc - you.pos();

        int ax = abs(delta.x);
        int ay = abs(delta.y);

        coord_def result = you.pos();

        if (1000 * ay < 414 * ax)
            result += (delta.x > 0) ? coord_def(1, 0) : coord_def(-1, 0);
        else if (1000 * ax < 414 * ay)
            result += (delta.y > 0) ? coord_def(0, 1) : coord_def(0, -1);
        else if (delta.x > 0)
            result += (delta.y > 0) ? coord_def(1, 1) : coord_def(1, -1);
        else if (delta.x < 0)
            result += (delta.y > 0) ? coord_def(-1, 1) : coord_def(-1, -1);

        if (m_cursor[type] != result)
        {
            m_dirty = true;
            m_cursor[type] = result;
        }
    }
    else
    {
        if (m_cursor[type] != gc)
        {
            m_dirty = true;
            m_cursor[type] = gc;
        }
    }
}

bool DungeonRegion::update_tip_text(std::string& tip)
{
    // TODO enne - it would be really nice to use the tutorial
    // descriptions here for features, monsters, etc...
    // Unfortunately, that would require quite a bit of rewriting
    // and some parsing of formatting to get that to work.

    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return false;

    if (m_cursor[CURSOR_MOUSE] == NO_CURSOR)
        return false;
    if (!map_bounds(m_cursor[CURSOR_MOUSE]))
        return false;

    if (m_cursor[CURSOR_MOUSE] == you.pos())
    {
        tip = you.your_name;
        tip += " (";
        tip += get_species_abbrev(you.species);
        tip += get_class_abbrev(you.char_class);
        tip += ")";

        if (igrd(m_cursor[CURSOR_MOUSE]) != NON_ITEM)
            tip += "\n[L-Click] Pick up items (g)";

        if (grid_stair_direction(grd(m_cursor[CURSOR_MOUSE])) != CMD_NO_CMD)
            tip += "\n[Shift-L-Click] use stairs (</>)";

        // Character overview.
        tip += "\n[R-Click] Overview (%)";

        // Religion.
        if (you.religion != GOD_NO_GOD)
            tip += "\n[Shift-R-Click] Religion (^)";
    }
    else if (abs(m_cursor[CURSOR_MOUSE].x - you.pos().x) <= 1
             && abs(m_cursor[CURSOR_MOUSE].y - you.pos().y) <= 1)
    {
        tip = "";

        if (!grid_is_solid(m_cursor[CURSOR_MOUSE]))
        {
            int mon_num = mgrd(m_cursor[CURSOR_MOUSE]);
            if (mon_num == NON_MONSTER || mons_friendly(&menv[mon_num]))
            {
                tip = "[L-Click] Move\n";
            }
            else if (mon_num != NON_MONSTER)
            {
                tip = menv[mon_num].name(DESC_CAP_A);
                tip += "\n[L-Click] Attack\n";
            }
        }

        tip += "[R-Click] Describe";
    }
    else
    {
        if (i_feel_safe() && !grid_is_solid(m_cursor[CURSOR_MOUSE]))
        {
            tip = "[L-Click] Travel\n";
        }

        tip += "[R-Click] Describe";
    }

    return true;
}

void DungeonRegion::clear_text_tags(text_tag_type type)
{
    m_tags[type].clear();
}

void DungeonRegion::add_text_tag(text_tag_type type, const std::string &tag,
                                 const coord_def &gc)
{
    TextTag t;
    t.tag = tag;
    t.gc = gc;

    m_tags[type].push_back(t);
}

void DungeonRegion::add_overlay(const coord_def &gc, int idx)
{
    tile_overlay over;
    over.gc = gc;
    over.idx = idx;

    m_overlays.push_back(over);
    m_dirty = true;
}

void DungeonRegion::clear_overlays()
{
    m_overlays.clear();
    m_dirty = true;
}

InventoryTile::InventoryTile()
{
    tile     = 0;
    idx      = -1;
    quantity = -1;
    key      = 0;
    flag     = 0;
    special  = 0;
}

bool InventoryTile::empty() const
{
    return (idx == -1);
}

InventoryRegion::InventoryRegion(ImageManager* im, FTFont *tag_font, int tile_x, int tile_y) :
    TileRegion(im, tag_font, tile_x, tile_y),
    m_flavour(NULL),
    m_buf_dngn(&im->m_textures[TEX_DUNGEON]),
    m_buf_main(&im->m_textures[TEX_DEFAULT]),
    m_cursor(NO_CURSOR)
{
}

InventoryRegion::~InventoryRegion()
{
    delete[] m_flavour;
}

void InventoryRegion::clear()
{
    m_items.clear();
    m_buf_dngn.clear();
    m_buf_main.clear();
}

void InventoryRegion::on_resize()
{
    delete[] m_flavour;
    if (mx * my <= 0)
        return;

    m_flavour = new unsigned char[mx * my];
    for (int i = 0; i < mx * my; i++)
    {
        m_flavour[i] = random2((unsigned char)~0);
    }
}

void InventoryRegion::update(int num, InventoryTile *items)
{
    m_items.clear();
    for (int i = 0; i < num; i++)
        m_items.push_back(items[i]);

    m_dirty = true;
}

void InventoryRegion::update_slot(int slot, InventoryTile &desc)
{
    while (m_items.size() <= (unsigned int)slot)
    {
        InventoryTile temp;
        m_items.push_back(temp);
    }

    m_items[slot] = desc;

    m_dirty = true;
}

void InventoryRegion::render()
{
    if (m_dirty)
    {
        pack_buffers();
        m_dirty = false;
    }

    if (m_buf_dngn.empty() && m_buf_main.empty())
        return;

    set_transform();
    m_buf_dngn.draw();
    m_buf_main.draw();

    if (m_cursor != NO_CURSOR)
    {
        unsigned int curs_index = cursor_index();
        if (curs_index >= m_items.size())
            return;
        int idx = m_items[curs_index].idx;
        if (idx == -1)
            return;

        bool floor = m_items[curs_index].flag & TILEI_FLAG_FLOOR;

        int x = m_cursor.x * dx + sx + ox + dx / 2;
        int y = m_cursor.y * dy + sy + oy;

        const coord_def min_pos(sx, sy - dy);
        const coord_def max_pos(ex, ey);

        std::string desc;
        if (floor)
            desc = mitm[idx].name(DESC_PLAIN);
        else
            desc = you.inv[idx].name(DESC_INVENTORY_EQUIP);

        m_tag_font->render_string(x, y,
                                  desc.c_str(),
                                  min_pos, max_pos, WHITE, false, 200);
    }
}

void InventoryRegion::add_quad_char(char c, int x, int y, int ofs_x, int ofs_y)
{
    int num = c - '0';
    assert(num >=0 && num <= 9);
    int idx = TILE_NUM0 + num;

    m_buf_main.add(idx, x, y, ofs_x, ofs_y, false);
}

void InventoryRegion::pack_buffers()
{
    m_buf_dngn.clear();
    m_buf_main.clear();

    // Ensure the cursor has been placed.
    place_cursor(m_cursor);

    // Pack base separately, as it comes from a different texture...
    unsigned int i = 0;
    for (int y = 0; y < my; y++)
    {
        if (i >= m_items.size())
            break;

        for (int x = 0; x < mx; x++)
        {
            if (i >= m_items.size())
                break;

            InventoryTile &item = m_items[i++];

            if (item.flag & TILEI_FLAG_FLOOR)
            {
                int num_floor = tile_dngn_count(env.tile_default.floor);
                m_buf_dngn.add(env.tile_default.floor
                         + m_flavour[i] % num_floor, x, y);
            }
            else
                m_buf_dngn.add(TILE_ITEM_SLOT, x, y);
        }
    }

    i = 0;
    for (int y = 0; y < my; y++)
    {
        if (i >= m_items.size())
            break;

        for (int x = 0; x < mx; x++)
        {
            if (i >= m_items.size())
                break;

            InventoryTile &item = m_items[i++];

            if (item.flag & TILEI_FLAG_EQUIP)
            {
                if (item.flag & TILEI_FLAG_CURSE)
                    m_buf_main.add(TILE_ITEM_SLOT_EQUIP_CURSED, x, y);
                else
                    m_buf_main.add(TILE_ITEM_SLOT_EQUIP, x, y);

                if (item.flag & TILEI_FLAG_MELDED)
                    m_buf_main.add(TILE_MESH, x, y);
            }
            else if (item.flag & TILEI_FLAG_CURSE)
                m_buf_main.add(TILE_ITEM_SLOT_CURSED, x, y);

            // TODO enne - need better graphic here
            if (item.flag & TILEI_FLAG_SELECT)
                m_buf_main.add(TILE_ITEM_SLOT_SELECTED, x, y);

            if (item.flag & TILEI_FLAG_CURSOR)
                m_buf_main.add(TILE_CURSOR, x, y);

            if (item.tile)
                m_buf_main.add(item.tile, x, y);

            if (item.quantity != -1)
            {
                int num = item.quantity;
                // If you have that many, who cares.
                if (num > 999)
                    num = 999;

                const int offset_amount = TILE_X/4;
                int offset_x = 3;
                int offset_y = 1;

                int help = num;
                int c100 = help/100;
                help -= c100*100;

                if (c100)
                {
                    add_quad_char('0' + c100, x, y, offset_x, offset_y);
                    offset_x += offset_amount;
                }

                int c10 = help/10;
                if (c10 || c100)
                {
                    add_quad_char('0' + c10, x, y, offset_x, offset_y);
                    offset_x += offset_amount;
                }

                int c1 = help % 10;
                add_quad_char('0' + c1, x, y, offset_x, offset_y);
            }

            if (item.special)
                m_buf_main.add(item.special, x, y, 0, 0, false);

            if (item.flag & TILEI_FLAG_TRIED)
                m_buf_main.add(TILE_TRIED, x, y, 0, TILE_Y / 2, false);

            if (item.flag & TILEI_FLAG_INVALID)
                m_buf_main.add(TILE_MESH, x, y);
        }
    }
}

unsigned int InventoryRegion::cursor_index() const
{
    ASSERT(m_cursor != NO_CURSOR);
    return m_cursor.x + m_cursor.y * mx;
}

void InventoryRegion::place_cursor(const coord_def &cursor)
{
    if (m_cursor != NO_CURSOR && cursor_index() < m_items.size())
    {
        m_items[cursor_index()].flag &= ~TILEI_FLAG_CURSOR;
        m_dirty = true;
    }

    m_cursor = cursor;

    if (m_cursor == NO_CURSOR || cursor_index() >= m_items.size())
        return;

    // Add cursor to new location
    m_items[cursor_index()].flag |= TILEI_FLAG_CURSOR;
    m_dirty = true;
}

int InventoryRegion::handle_mouse(MouseEvent &event)
{
    int cx, cy;
    if (!mouse_pos(event.px, event.py, cx, cy))
    {
        place_cursor(NO_CURSOR);
        return 0;
    }

    const coord_def cursor(cx, cy);
    place_cursor(cursor);

    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return 0;

    if (event.event != MouseEvent::PRESS)
        return 0;

    unsigned int item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return 0;

    int idx = m_items[item_idx].idx;
    bool on_floor = m_items[item_idx].flag & TILEI_FLAG_FLOOR;

    ASSERT(idx >= 0);

    // TODO enne - this is all really only valid for the on-screen inventory
    // Do we subclass inventoryregion for the onscreen and offscreen versions?
    char key = m_items[item_idx].key;
    if (key)
        return key;

    if (event.button == MouseEvent::LEFT)
    {
        if (on_floor)
        {
            if (event.mod & MOD_SHIFT)
                tile_item_use_floor(idx);
            else
                tile_item_pickup(idx);
        }
        else
        {
            if (event.mod & MOD_SHIFT)
                tile_item_drop(idx);
            else if (event.mod & MOD_CTRL)
                tile_item_use_secondary(idx);
            else
                tile_item_use(idx);
        }
        // TODO enne - need to redraw inventory here?
        return CK_MOUSE_CMD;
    }
    else if (event.button == MouseEvent::RIGHT)
    {
        if (on_floor)
        {
            if (event.mod & MOD_SHIFT)
            {
                tile_item_eat_floor(idx);
            }
            else
            {
                describe_item(mitm[idx]);
                redraw_screen();
            }
        }
        else // in inventory
        {
            describe_item(you.inv[idx], true);
            redraw_screen();
        }
        return CK_MOUSE_CMD;
    }

    return 0;
}

// NOTE: Assumes the item is equipped in the first place!
static bool _is_true_equipped_item(item_def item)
{
    // Weapons and staves are only truly equipped if wielded.
    if (item.link == you.equip[EQ_WEAPON])
        return (item.base_type == OBJ_WEAPONS || item.base_type == OBJ_STAVES);

    // Cursed armour and rings are only truly equipped if *not* wielded.
    return (item.link != you.equip[EQ_WEAPON]);
}

// Returns whether there's any action you can take with an item in inventory
// apart from dropping it.
static bool _can_use_item(const item_def &item, bool equipped)
{
    // Vampires can drain corpses.
    if (item.base_type == OBJ_CORPSES)
    {
        return (you.species == SP_VAMPIRE
                && item.sub_type != CORPSE_SKELETON
                && !food_is_rotten(item)
                && mons_has_blood(item.plus));
    }

    if (equipped && item_cursed(item))
    {
        // Misc. items/rods can always be evoked, cursed or not.
        if (item.base_type == OBJ_MISCELLANY || item_is_rod(item))
            return true;

        // You can't unwield/fire a wielded cursed weapon/staff
        // but cursed armour and rings can be unwielded without problems.
        return (!_is_true_equipped_item(item));
    }

    // Mummies can't do anything with food or potions.
    if (you.species == SP_MUMMY)
        return (item.base_type != OBJ_POTIONS && item.base_type != OBJ_FOOD);

    // In all other cases you can use the item in some way.
    return true;
}

bool InventoryRegion::update_tip_text(std::string& tip)
{
    if (m_cursor == NO_CURSOR)
        return false;

    unsigned int item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return false;

    int idx = m_items[item_idx].idx;

    // TODO enne - consider subclassing this class, rather than depending
    // on "key" to determine if this is the crt inventory or the on screen one.
    bool display_actions = (m_items[item_idx].key == 0
        && mouse_control::current_mode() == MOUSE_MODE_COMMAND);

    // TODO enne - should the command keys here respect keymaps?

    if (m_items[item_idx].flag & TILEI_FLAG_FLOOR)
    {
        const item_def &item = mitm[idx];

        tip = "";
        if (m_items[item_idx].key)
        {
            tip = m_items[item_idx].key;
            tip += " - ";
        }

        tip += item.name(DESC_NOCAP_A);

        if (!display_actions)
            return true;

        tip += "\n[L-Click] Pick up (g)";
        if (item.base_type == OBJ_CORPSES
            && item.sub_type != CORPSE_SKELETON
            && !food_is_rotten(item))
        {
            tip += "\n[Shift-L-Click] ";
            if (can_bottle_blood_from_corpse(item.plus))
                tip += "Bottle blood";
            else
                tip += "Chop up";
            tip += " (c)";

            if (you.species == SP_VAMPIRE)
                tip += "\n\n[Shift-R-Click] Drink blood (e)";
        }
        else if (item.base_type == OBJ_FOOD
                 && you.is_undead != US_UNDEAD
                 && you.species != SP_VAMPIRE)
        {
            tip += "\n[Shift-R-Click] Eat (e)";
        }
    }
    else
    {
        const item_def &item = you.inv[idx];
        tip = item.name(DESC_INVENTORY_EQUIP);

        if (!display_actions)
            return true;

        int type = item.base_type;
        const bool equipped = m_items[item_idx].flag & TILEI_FLAG_EQUIP;
        bool wielded = (you.equip[EQ_WEAPON] == idx);

        const int EQUIP_OFFSET = NUM_OBJECT_CLASSES;

        if (_can_use_item(item, equipped))
        {
            tip += "\n[L-Click] ";
            if (equipped)
            {
                if (wielded && type != OBJ_MISCELLANY && !item_is_rod(item))
                {
                    if (type == OBJ_JEWELLERY || type == OBJ_ARMOUR
                        || type == OBJ_WEAPONS || type == OBJ_STAVES)
                    {
                        type = OBJ_WEAPONS + EQUIP_OFFSET;
                    }
                }
                else
                    type += EQUIP_OFFSET;
            }

            switch (type)
            {
            // first equipable categories
            case OBJ_WEAPONS:
            case OBJ_STAVES:
            case OBJ_MISCELLANY:
                tip += "Wield (w)";
                if (is_throwable(&you, item))
                    tip += "\n[Ctrl-L-Click] Fire (f)";
                break;
            case OBJ_WEAPONS + EQUIP_OFFSET:
                tip += "Unwield";
                if (is_throwable(&you, item))
                    tip += "\n[Ctrl-L-Click] Fire (f)";
                break;
            case OBJ_MISCELLANY + EQUIP_OFFSET:
                if (item.sub_type >= MISC_DECK_OF_ESCAPE
                    && item.sub_type <= MISC_DECK_OF_DEFENCE)
                {
                    tip += "Draw a card (v)";
                    tip += "\n[Ctrl-L-Click] Unwield";
                    break;
                }
                // else fall-through
            case OBJ_STAVES + EQUIP_OFFSET: // rods - other staves handled above
                tip += "Evoke (v)";
                tip += "\n[Ctrl-L-Click] Unwield";
                break;
            case OBJ_ARMOUR:
                tip += "Wear (W)";
                break;
            case OBJ_ARMOUR + EQUIP_OFFSET:
                tip += "Take off (T)";
                break;
            case OBJ_JEWELLERY:
                tip += "Put on (P)";
                break;
            case OBJ_JEWELLERY + EQUIP_OFFSET:
                tip += "Remove (R)";
                break;
            case OBJ_MISSILES:
                tip += "Fire (f)";

                if (wielded)
                    tip += "\n[Ctrl-L-Click] Unwield";
                else if ( item.sub_type == MI_STONE
                             && player_knows_spell(SPELL_SANDBLAST)
                          || item.sub_type == MI_ARROW
                             && player_knows_spell(
                                    SPELL_STICKS_TO_SNAKES) )
                {
                    // For Sandblast and Sticks to Snakes,
                    // respectively.
                    tip += "\n[Ctrl-L-Click] Wield (w)";
                }
                break;
            case OBJ_WANDS:
                tip += "Zap (Z)";
                if (wielded)
                    tip += "\n[Ctrl-L-Click] Unwield";
                break;
            case OBJ_BOOKS:
                if (item_type_known(item)
                    && item.sub_type != BOOK_MANUAL
                    && item.sub_type != BOOK_DESTRUCTION)
                {
                    tip += "Memorise (M)";
                    if (wielded)
                        tip += "\n[Ctrl-L-Click] Unwield";
                    break;
                }
                // else fall-through
            case OBJ_SCROLLS:
                tip += "Read (r)";
                if (wielded)
                    tip += "\n[Ctrl-L-Click] Unwield";
                break;
            case OBJ_POTIONS:
                tip += "Quaff (q)";
                // For Sublimation of Blood.
                if (wielded)
                    tip += "\n[Ctrl-L-Click] Unwield";
                else if ( item_type_known(item)
                          && is_blood_potion(item)
                          && player_knows_spell(
                                SPELL_SUBLIMATION_OF_BLOOD) )
                {
                    tip += "\n[Ctrl-L-Click] Wield (w)";
                }
                break;
            case OBJ_FOOD:
                tip += "Eat (e)";
                // For Sublimation of Blood.
                if (wielded)
                    tip += "\n[Ctrl-L-Click] Unwield";
                else if (item.sub_type == FOOD_CHUNK
                         && player_knows_spell(
                                SPELL_SUBLIMATION_OF_BLOOD))
                {
                    tip += "\n[Ctrl-L-Click] Wield (w)";
                }
                break;
            case OBJ_CORPSES:
                if (you.species == SP_VAMPIRE)
                    tip += "Drink blood (e)";

                if (wielded)
                {
                    if (you.species == SP_VAMPIRE)
                        tip += EOL;
                    tip += "[Ctrl-L-Click] Unwield";
                }
                break;
            default:
                tip += "Use";
            }
        }

        // For Boneshards.
        // Special handling since skeletons have no primary action.
        if (item.base_type == OBJ_CORPSES
            && item.sub_type == CORPSE_SKELETON)
        {
            if (wielded)
                tip += "\n[Ctrl-L-Click] Unwield";
            else if (player_knows_spell(SPELL_BONE_SHARDS))
                tip += "\n[Ctrl-L-Click] Wield (w)";
        }

        tip += "\n[R-Click] Info";
        // Has to be non-equipped or non-cursed to drop.
        if (!equipped || !_is_true_equipped_item(you.inv[idx])
            || !item_cursed(you.inv[idx]))
        {
            tip += "\n[Shift-L-Click] Drop (d)";
        }

    }

    return true;
}

MapRegion::MapRegion(int pixsz) :
    m_buf(NULL),
    m_dirty(true),
    m_far_view(false)
{
    ASSERT(pixsz > 0);

    dx = pixsz;
    dy = pixsz;
    clear();
    init_colours();
}

void MapRegion::on_resize()
{
    delete[] m_buf;

    int size = mx * my;
    m_buf = new unsigned char[size];
    memset(m_buf, 0, sizeof(unsigned char) * size);
}

void MapRegion::init_colours()
{
    // TODO enne - the options array for colours should be
    // tied to the map feature enumeration to avoid this function.
    m_colours[MF_UNSEEN] = (map_colour)Options.tile_unseen_col;
    m_colours[MF_FLOOR] = (map_colour)Options.tile_floor_col;
    m_colours[MF_WALL] = (map_colour)Options.tile_wall_col;
    m_colours[MF_MAP_FLOOR] = (map_colour)Options.tile_floor_col; // TODO enne
    m_colours[MF_MAP_WALL] = (map_colour)Options.tile_mapped_wall_col;
    m_colours[MF_DOOR] = (map_colour)Options.tile_door_col;
    m_colours[MF_ITEM] = (map_colour)Options.tile_item_col;
    m_colours[MF_MONS_HOSTILE] = (map_colour)Options.tile_monster_col;
    m_colours[MF_MONS_FRIENDLY] = (map_colour)Options.tile_friendly_col;
    m_colours[MF_MONS_NEUTRAL] = (map_colour)Options.tile_neutral_col;
    m_colours[MF_MONS_NO_EXP] = (map_colour)Options.tile_plant_col;
    m_colours[MF_STAIR_UP] = (map_colour)Options.tile_upstairs_col;
    m_colours[MF_STAIR_DOWN] = (map_colour)Options.tile_downstairs_col;
    m_colours[MF_STAIR_BRANCH] = (map_colour)Options.tile_feature_col;
    m_colours[MF_FEATURE] = (map_colour)Options.tile_feature_col;
    m_colours[MF_WATER] = (map_colour)Options.tile_water_col;
    m_colours[MF_LAVA] = (map_colour)Options.tile_lava_col;
    m_colours[MF_TRAP] = (map_colour)Options.tile_trap_col;
    m_colours[MF_EXCL_ROOT] = (map_colour)Options.tile_excl_centre_col;
    m_colours[MF_EXCL] = (map_colour)Options.tile_excluded_col;
    m_colours[MF_PLAYER] = (map_colour)Options.tile_player_col;
}

MapRegion::~MapRegion()
{
    delete[] m_buf;
}

void MapRegion::pack_buffers()
{
    m_buf_map.clear();
    m_buf_lines.clear();

    for (int x = m_min_gx; x <= m_max_gx; x++)
    {
        for (int y = m_min_gy; y <= m_max_gy; y++)
        {
            map_feature f = (map_feature)m_buf[x + y * mx];
            map_colour c = m_colours[f];

            float pos_x = x - m_min_gx;
            float pos_y = y - m_min_gy;
            m_buf_map.add(pos_x, pos_y, pos_x + 1, pos_y + 1, map_colours[c]);
        }
    }

    // Draw window box.
    if (m_win_start.x == -1 && m_win_end.x == -1)
        return;

    int c = (int)Options.tile_window_col;
    float pos_sx = (m_win_start.x - m_min_gx);
    float pos_sy = (m_win_start.y - m_min_gy);
    float pos_ex = (m_win_end.x - m_min_gx) + 1 / (float)dx;
    float pos_ey = (m_win_end.y - m_min_gy) + 1 / (float)dy;

    m_buf_lines.add_square(pos_sx, pos_sy, pos_ex, pos_ey, map_colours[c]);
}

void MapRegion::render()
{
    if (m_min_gx > m_max_gx || m_min_gy > m_max_gy)
        return;

    if (m_dirty)
    {
        pack_buffers();
        m_dirty = false;
    }

    set_transform();
    m_buf_map.draw();
    m_buf_lines.draw();
}

void MapRegion::recenter()
{
    // adjust offsets to center map
    ox = (wx - dx * (m_max_gx - m_min_gx)) / 2;
    oy = (wy - dy * (m_max_gy - m_min_gy)) / 2;
    m_dirty = true;
}

void MapRegion::set(int gx, int gy, map_feature f)
{
    ASSERT((unsigned int)f <= (unsigned char)~0);
    m_buf[gx + gy * mx] = f;

    if (f == MF_UNSEEN)
        return;

    // Get map extents
    m_min_gx = std::min(m_min_gx, gx);
    m_max_gx = std::max(m_max_gx, gx);
    m_min_gy = std::min(m_min_gy, gy);
    m_max_gy = std::max(m_max_gy, gy);

    recenter();
}

void MapRegion::update_bounds()
{
    int min_gx = m_min_gx;
    int max_gx = m_max_gx;
    int min_gy = m_min_gy;
    int max_gy = m_max_gy;

    m_min_gx = GXM;
    m_max_gx = 0;
    m_min_gy = GYM;
    m_max_gy = 0;

    for (int x = min_gx; x <= max_gx; x++)
    {
        for (int y = min_gy; y <= max_gy; y++)
        {
            map_feature f = (map_feature)m_buf[x + y * mx];
            if (f == MF_UNSEEN)
                continue;

            m_min_gx = std::min(m_min_gx, x);
            m_max_gx = std::max(m_max_gx, x);
            m_min_gy = std::min(m_min_gy, y);
            m_max_gy = std::max(m_max_gy, y);
        }
    }

    recenter();
    m_dirty = true;
}

void MapRegion::set_window(const coord_def &start, const coord_def &end)
{
    m_win_start = start;
    m_win_end = end;

    m_dirty = true;
}

void MapRegion::clear()
{
    m_min_gx = GXM;
    m_max_gx = 0;
    m_min_gy = GYM;
    m_max_gy = 0;

    m_win_start.x = -1;
    m_win_start.y = -1;
    m_win_end.x = -1;
    m_win_end.y = -1;

    recenter();

    if (m_buf)
        memset(m_buf, 0, sizeof(*m_buf) * mx * my);

    m_buf_map.clear();
    m_buf_lines.clear();
}

int MapRegion::handle_mouse(MouseEvent &event)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return 0;

    int cx;
    int cy;
    if (!mouse_pos(event.px, event.py, cx, cy))
    {
        if (m_far_view)
        {
            m_far_view = false;
            tiles.load_dungeon(crawl_view.vgrdc);
            return 0;
        }
        return 0;
    }

    const coord_def gc(m_min_gx + cx, m_min_gy + cy);

    tiles.place_cursor(CURSOR_MOUSE, gc);

    switch (event.event)
    {
    case MouseEvent::MOVE:
        if (m_far_view)
            tiles.load_dungeon(gc);
        return 0;
    case MouseEvent::PRESS:
        if (event.button == MouseEvent::LEFT)
        {
            return _click_travel(gc, event);
        }
        else if (event.button == MouseEvent::RIGHT)
        {
            m_far_view = true;
            tiles.load_dungeon(gc);
        }
        return 0;
    case MouseEvent::RELEASE:
        if ((event.button == MouseEvent::RIGHT) && m_far_view)
        {
            m_far_view = false;
            tiles.load_dungeon(crawl_view.vgrdc);
        }
        return 0;
    default:
        return 0;
    }
}

bool MapRegion::update_tip_text(std::string& tip)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return false;

    tip = "[L-Click] Travel / [R-Click] View";
    return true;
}

void TextRegion::scroll()
{
    for (int idx = 0; idx < mx*(my-1); idx++)
    {
        cbuf[idx] = cbuf[idx + mx];
        abuf[idx] = abuf[idx + mx];
    }

    for (int idx = mx*(my-1); idx < mx*my; idx++)
    {
        cbuf[idx] = ' ';
        abuf[idx] = 0;
    }

    if (print_y > 0)
        print_y -= 1;
    if (cursor_y > 0)
        cursor_y -= 1;
}

TextRegion::TextRegion(FTFont *font) :
    cbuf(NULL),
    abuf(NULL),
    cx_ofs(0),
    cy_ofs(0),
    m_font(font)
{
    ASSERT(font);

    dx = m_font->char_width();
    dy = m_font->char_height();
}

void TextRegion::on_resize()
{
    delete cbuf;
    delete abuf;

    int size = mx * my;
    cbuf = new unsigned char[size];
    abuf = new unsigned char[size];

    for (int i = 0; i < size; i++)
    {
        cbuf[i] = ' ';
        abuf[i] = 0;
    }
}

TextRegion::~TextRegion()
{
    delete[] cbuf;
    delete[] abuf;
}

void TextRegion::adjust_region(int *x1, int *x2, int y)
{
    *x2 = *x2 + 1;
}

void TextRegion::addstr(char *buffer)
{
    char buf2[1024];
    int len = strlen(buffer);

    int j = 0;

    for (int i = 0; i < len + 1; i++)
    {
        char c = buffer[i];
        bool newline = false;
        if (c == '\n' || c == '\r')
        {
            c = 0;
            newline = true;
            if (buffer[i+1] == '\n' || buffer[i+1] == '\r')
                i++;
        }
        buf2[j] = c;
        j++;

        if (c == 0)
        {
            if (j-1 != 0)
                addstr_aux(buf2, j - 1); // draw it
            if (newline)
            {
                print_x = cx_ofs;
                print_y++;
                j = 0;

                if (print_y - cy_ofs == my)
                    scroll();
            }
        }
    }
    if (cursor_flag)
        cgotoxy(print_x+1, print_y+1);
}

void TextRegion::addstr_aux(char *buffer, int len)
{
    int x = print_x - cx_ofs;
    int y = print_y - cy_ofs;
    int adrs = y * mx;
    int head = x;
    int tail = x + len - 1;

    adjust_region(&head, &tail, y);

    for (int i = 0; i < len && x + i < mx; i++)
    {
        cbuf[adrs+x+i] = buffer[i];
        abuf[adrs+x+i] = text_col;
    }
    print_x += len;
}

void TextRegion::clear_to_end_of_line()
{
    int cx = print_x - cx_ofs;
    int cy = print_y - cy_ofs;
    int col = text_col;
    int adrs = cy * mx;

    ASSERT(adrs + mx - 1 < mx * my);
    for (int i = cx; i < mx; i++)
    {
        cbuf[adrs+i] = ' ';
        abuf[adrs+i] = col;
    }
}

void TextRegion::putch(unsigned char ch)
{
    if (ch == 0)
        ch=32;
    addstr_aux((char *)&ch, 1);
}

void TextRegion::writeWChar(unsigned char *ch)
{
    addstr_aux((char *)ch, 2);
}

void TextRegion::textcolor(int color)
{
    text_col = color;
}

void TextRegion::textbackground(int col)
{
    textcolor(col*16 + (text_col & 0xf));
}

void TextRegion::cgotoxy(int x, int y)
{
    ASSERT(x >= 1);
    ASSERT(y >= 1);
    print_x = x-1;
    print_y = y-1;

    if (cursor_region != NULL && cursor_flag)
    {
        cursor_x = -1;
        cursor_y = -1;
        cursor_region = NULL;
    }

    if (cursor_flag)
    {
        cursor_x = print_x;
        cursor_y = print_y;
        cursor_region = text_mode;
    }
}

int TextRegion::wherex()
{
    return print_x + 1;
}

int TextRegion::wherey()
{
    return print_y + 1;
}

void TextRegion::_setcursortype(int curstype)
{
    cursor_flag = curstype;
    if (cursor_region != NULL)
    {
        cursor_x = -1;
        cursor_y = -1;
    }

    if (curstype)
    {
        cursor_x = print_x;
        cursor_y = print_y;
        cursor_region = text_mode;
    }
}

void TextRegion::render()
{
    if (this == TextRegion::cursor_region && cursor_x > 0 && cursor_y > 0)
    {
        int idx = cursor_x + mx * cursor_y;

        unsigned char char_back = cbuf[idx];
        unsigned char col_back = abuf[idx];

        cbuf[idx] = '_';
        abuf[idx] = WHITE;

        m_font->render_textblock(sx + ox, sy + oy, cbuf, abuf, mx, my);

        cbuf[idx] = char_back;
        abuf[idx] = col_back;
    }
    else
    {
        m_font->render_textblock(sx + ox, sy + oy, cbuf, abuf, mx, my);
    }
}

void TextRegion::clear()
{
    for (int i = 0; i < mx * my; i++)
    {
        cbuf[i] = ' ';
        abuf[i] = 0;
    }
}

StatRegion::StatRegion(FTFont *font) : TextRegion(font)
{
}

int StatRegion::handle_mouse(MouseEvent &event)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return 0;

    if (!inside(event.px, event.py))
        return 0;

    if (event.event != MouseEvent::PRESS || event.button != MouseEvent::LEFT)
        return 0;

    // Resting
    return '5';
}

bool StatRegion::update_tip_text(std::string& tip)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return false;

    tip = "[L-Click] Rest / Search for a while";
    return true;
}

MessageRegion::MessageRegion(FTFont *font) : TextRegion(font), m_overlay(false)
{
}

int MessageRegion::handle_mouse(MouseEvent &event)
{
    // TODO enne - mouse scrolling here should mouse scroll up through
    // the message history in the message pane, without going to the CRT.

    if (!inside(event.px, event.py))
        return 0;

    if (event.event != MouseEvent::PRESS || event.button != MouseEvent::LEFT)
        return 0;

    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return 0;

    return CONTROL('P');
}

bool MessageRegion::update_tip_text(std::string& tip)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return false;

    tip = "[L-Click] Browse message history";
    return true;
}

void MessageRegion::set_overlay(bool is_overlay)
{
    m_overlay = is_overlay;
}

struct box_vert
{
    float x;
    float y;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

void MessageRegion::render()
{
    int idx = -1;
    unsigned char char_back = 0;
    unsigned char col_back = 0;

    if (this == TextRegion::cursor_region && cursor_x > 0 && cursor_y > 0)
    {
        idx = cursor_x + mx * cursor_y;
        char_back = cbuf[idx];
        col_back = abuf[idx];

        cbuf[idx] = '_';
        abuf[idx] = WHITE;
    }

    if (m_overlay)
    {
        int height;
        bool found = false;
        for (height = my; height > 0; height--)
        {
            unsigned char *buf = &cbuf[mx * (height - 1)];
            for (int x = 0; x < mx; x++)
            {
                if (buf[x] != ' ')
                {
                    found = true;
                    break;
                }
            }

            if (found)
                break;
        }

        if (height > 0)
        {
            height *= m_font->char_height();
            box_vert verts[4];
            for (int i = 0; i < 4; i++)
            {
                verts[i].r = 100;
                verts[i].g = 100;
                verts[i].b = 100;
                verts[i].a = 100;
            }
            verts[0].x = sx;
            verts[0].y = sy;
            verts[1].x = sx;
            verts[1].y = sy + height;
            verts[2].x = ex;
            verts[2].y = sy + height;
            verts[3].x = ex;
            verts[3].y = sy;

            glLoadIdentity();

            GLState state;
            state.array_vertex = true;
            state.array_colour = true;
            state.blend = true;
            GLStateManager::set(state);

            glVertexPointer(2, GL_FLOAT, sizeof(box_vert), &verts[0].x);
            glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(box_vert), &verts[0].r);
            glDrawArrays(GL_QUADS, 0, sizeof(verts) / sizeof(box_vert));
        }
    }

    m_font->render_textblock(sx + ox, sy + oy, cbuf, abuf, mx, my, m_overlay);

    if (idx >= 0)
    {
        cbuf[idx] = char_back;
        abuf[idx] = col_back;
    }
}

CRTRegion::CRTRegion(FTFont *font) : TextRegion(font)
{
}

int CRTRegion::handle_mouse(MouseEvent &event)
{
    if (event.event != MouseEvent::PRESS || event.button != MouseEvent::LEFT)
        return 0;

    return CK_MOUSE_CLICK;
}

MenuRegion::MenuRegion(ImageManager *im, FTFont *entry) :
    m_image(im), m_font_entry(entry), m_mouse_idx(-1),
    m_max_columns(1), m_dirty(false), m_font_buf(entry)
{
    ASSERT(m_image);
    ASSERT(m_font_entry);

    dx = 1;
    dy = 1;

    m_entries.resize(128);

    for (int i = 0; i < TEX_MAX; i++)
        m_tile_buf[i].set_tex(&m_image->m_textures[i]);
}

void MenuRegion::set_num_columns(int columns)
{
    m_max_columns = std::max(1, columns);
}

int MenuRegion::mouse_entry(int x, int y)
{
    if (m_dirty)
        place_entries();

    for (unsigned int i = 0; i < m_entries.size(); i++)
    {
        if (!m_entries[i].valid)
            continue;

        if (x >= m_entries[i].sx && x <= m_entries[i].ex
            && y >= m_entries[i].sy && y <= m_entries[i].ey)
        {
            return i;
        }
    }

    return -1;
}

int MenuRegion::handle_mouse(MouseEvent &event)
{
    m_mouse_idx = -1;

    int x, y;
    if (!mouse_pos(event.px, event.py, x, y))
        return 0;

    if (event.event == MouseEvent::MOVE)
    {
        int old_idx = m_mouse_idx;
        m_mouse_idx = mouse_entry(x, y);
        if (old_idx == m_mouse_idx)
            return 0;

        const VColour mouse_colour(160, 160, 160, 255);

        m_line_buf.clear();
        if (!m_entries[m_mouse_idx].heading && m_entries[m_mouse_idx].key)
        {
            m_line_buf.add_square(m_entries[m_mouse_idx].sx-1,
                                  m_entries[m_mouse_idx].sy,
                                  m_entries[m_mouse_idx].ex+1,
                                  m_entries[m_mouse_idx].ey+1,
                                  mouse_colour);
        }

        return 0;
    }

    if (event.event == MouseEvent::PRESS)
    {
        switch (event.button)
        {
        case MouseEvent::LEFT:
        {
            int entry = mouse_entry(x, y);
            if (entry == -1)
                return 0;
            return m_entries[entry].key;
        }
#if 0
        // TODO enne - these events are wonky on OS X.
        // TODO enne - fix menus so that mouse wheeling doesn't easy exit
        case MouseEvent::SCROLL_UP:
            return CK_UP;
        case MouseEvent::SCROLL_DOWN:
            return CK_DOWN;
#endif
        default:
            return 0;
        }
    }

    return 0;
}

void MenuRegion::place_entries()
{
    m_dirty = false;

    const int heading_indent = 10;
    const int tile_indent = 20;
    const int text_indent = 58;
    const int max_tile_height = 32;
    const int entry_buffer = 1;
    const VColour selected_colour(50, 50, 10, 255);

    m_font_buf.clear();
    m_shape_buf.clear();
    m_line_buf.clear();
    for (int t = 0; t < TEX_MAX; t++)
        m_tile_buf[t].clear();

    int column = 0;
    const int max_columns = std::min(2, m_max_columns);
    const int column_width = mx / max_columns;

    int lines = count_linebreaks(m_more);
    int more_height = (lines + 1) * m_font_entry->char_height();
    m_font_buf.add(m_more, sx + ox, ey - oy - more_height);

    int height = 0;
    int end_height = my - more_height;

    const int max_entry_height = std::max((int)m_font_entry->char_height() * 2,
                                          max_tile_height);

    for (unsigned int i = 0; i < m_entries.size(); i++)
    {
        if (!m_entries[i].valid)
        {
            m_entries[i].sx = 0;
            m_entries[i].sy = 0;
            m_entries[i].ex = 0;
            m_entries[i].ey = 0;
            continue;
        }

        if (height + max_entry_height > end_height)
        {
            height = 0;
            column++;
        }

        int text_width = m_font_entry->string_width(m_entries[i].text);
        int text_height = m_font_entry->char_height();

        if (m_entries[i].heading)
        {
            m_entries[i].sx = heading_indent + column * column_width;
            m_entries[i].ex = m_entries[i].sx + text_width;
            m_entries[i].sy = height;
            m_entries[i].ey = m_entries[i].sy + text_height;

            m_font_buf.add(m_entries[i].text, m_entries[i].sx, m_entries[i].sy);

            height += text_height;
        }
        else
        {
            m_entries[i].sy = height;
            int entry_start = column * column_width;
            int text_sx = text_indent + entry_start;

            int entry_height;

            if (m_entries[i].tiles.size() > 0)
            {
                m_entries[i].sx = entry_start + tile_indent;
                entry_height = std::max(max_tile_height, text_height);

                for (unsigned int t = 0; t < m_entries[i].tiles.size(); t++)
                {
                    // NOTE: This is not perfect. Tiles will be drawn
                    // sorted by texture first, e.g. you can never draw
                    // a dungeon tile over a monster tile.
                    int tile = m_entries[i].tiles[t].tile;
                    TextureID tex = m_entries[i].tiles[t].tex;
                    m_tile_buf[tex].add_unscaled(tile, m_entries[i].sx,
                                                 m_entries[i].sy);
                }
            }
            else
            {
                m_entries[i].sx = text_sx;
                entry_height = text_height;
            }

            int text_sy = m_entries[i].sy;
            text_sy += (entry_height - m_font_entry->char_height()) / 2;
            if (text_sx + text_width > entry_start + column_width)
            {
                // [enne] - Ugh, hack.  Maybe MenuEntry could specify the
                // presence and length of this substring?
                std::string unfm = m_entries[i].text.tostring();
                bool let = (unfm[1] >= 'a' && unfm[1] <= 'z'
                            || unfm[1] >= 'A' && unfm[1] <= 'Z');
                bool plus = (unfm[3] == '-' || unfm[3] == '+');

                formatted_string text;
                if (let && plus && unfm[0] == ' ' && unfm[2] == ' '
                    && unfm[4] == ' ')
                {
                    formatted_string header = m_entries[i].text.substr(0, 5);
                    m_font_buf.add(header, text_sx, text_sy);
                    text_sx += m_font_entry->string_width(header);
                    text = m_entries[i].text.substr(5);
                }
                else
                {
                    text += m_entries[i].text;
                }

                int w = entry_start + column_width - text_sx;
                int h = m_font_entry->char_height() * 2;
                formatted_string split = m_font_entry->split(text, w, h);

                int string_height = m_font_entry->string_height(split);
                if (string_height > entry_height)
                    text_sy = m_entries[i].sy;

                m_font_buf.add(split, text_sx, text_sy);

                entry_height = std::max(entry_height, string_height);
                m_entries[i].ex = entry_start + column_width;
            }
            else
            {
                if (max_columns == 1)
                    m_entries[i].ex = text_sx + text_width;
                else
                    m_entries[i].ex = entry_start + column_width;
                m_font_buf.add(m_entries[i].text, text_sx, text_sy);
            }

            m_entries[i].ey = m_entries[i].sy + entry_height;
            height = m_entries[i].ey;
        }

        if (m_entries[i].selected)
        {
            m_shape_buf.add(m_entries[i].sx-1, m_entries[i].sy-1,
                            m_entries[i].ex+1, m_entries[i].ey+1,
                            selected_colour);
        }

        height += entry_buffer;
    }
}

void MenuRegion::render()
{
    if (m_dirty)
        place_entries();

    set_transform();
    m_shape_buf.draw();
    m_line_buf.draw();
    for (int i = 0; i < TEX_MAX; i++)
        m_tile_buf[i].draw();
    m_font_buf.draw();
}

void MenuRegion::clear()
{
    m_shape_buf.clear();
    m_line_buf.clear();
    for (int i = 0; i < TEX_MAX; i++)
        m_tile_buf[i].clear();
    m_font_buf.clear();

    m_more.clear();

    for (unsigned int i = 0; i < m_entries.size(); i++)
        m_entries[i].valid = false;

    m_mouse_idx = -1;
}

void MenuRegion::set_entry(int idx, const std::string &str, int colour,
                           const MenuEntry *me)
{
    if (idx >= (int)m_entries.size())
    {
        int new_size = m_entries.size();
        while (idx >= new_size)
            new_size *= 2;
        m_entries.resize(new_size);
    }

    MenuRegionEntry &e = m_entries[idx];
    e.valid = true;
    e.text.clear();
    e.text.textcolor(colour);
    e.text += formatted_string::parse_string(str);
    e.heading = (me->level == MEL_TITLE || me->level == MEL_SUBTITLE);
    e.selected = me->selected();
    e.key = me->hotkeys.size() > 0 ? me->hotkeys[0] : 0;
    e.sx = e.sy = e.ex = e.ey = 0;
    e.tiles.clear();
    me->get_tiles(e.tiles);

    m_dirty = true;
}

void MenuRegion::on_resize()
{
    m_dirty = true;
}

int MenuRegion::maxpagesize() const
{
    // TODO enne - this is a conservative guess.
    // It would be better to make menus use a dynamic number of items per page,
    // but it'd require a lot more refactoring of menu.cc to handle that.

    int lines = count_linebreaks(m_more);
    int more_height = (lines + 1) * m_font_entry->char_height();

    int pagesize = ((my - more_height) / 32) * m_max_columns;
    return pagesize;
}

void MenuRegion::set_offset(int lines)
{
    oy = (lines - 1) * m_font_entry->char_height() + 4;
    my = wy - oy;
}

void MenuRegion::set_more(const formatted_string &more)
{
    m_more.clear();
    m_more += more;
    m_dirty = true;
}

ImageManager::ImageManager()
{
}

ImageManager::~ImageManager()
{
    unload_textures();
}

bool ImageManager::load_textures()
{
    GenericTexture::MipMapOptions mip = GenericTexture::MIPMAP_CREATE;
    if (!m_textures[TEX_DUNGEON].load_texture("dngn.png", mip))
        return false;

    if (!m_textures[TEX_DOLL].load_texture("player.png", mip))
        return false;

    m_textures[TEX_DUNGEON].set_info(TILE_DNGN_MAX, &tile_dngn_info);
    m_textures[TEX_DOLL].set_info(TILEP_PLAYER_MAX, &tile_player_info);

    return true;
}

static void _copy_onto(unsigned char *pixels, int width,
                       int height, unsigned char *src,
                       const tile_info &inf, bool blend)
{
    unsigned char *dest = &pixels[4 * (inf.sy * width + inf.sx)];

    size_t dest_row_size = width * 4;
    size_t src_row_size = inf.width * 4;

    if (blend)
    {
        for (int r = 0; r < inf.height; r++)
        {
            for (int c = 0; c < inf.width; c++)
            {
                unsigned char a = src[3];
                unsigned char inv_a = 255 - src[3];
                dest[0] = (src[0] * a + dest[0] * inv_a) / 255;
                dest[1] = (src[1] * a + dest[1] * inv_a) / 255;
                dest[2] = (src[2] * a + dest[2] * inv_a) / 255;
                dest[3] = (src[3] * a + dest[3] * inv_a) / 255;

                dest += 4;
                src += 4;
            }
            dest += dest_row_size - src_row_size;
        }
    }
    else
    {
        for (int r = 0; r < inf.height; r++)
        {
            memcpy(dest, src, src_row_size);

            dest += dest_row_size;
            src += src_row_size;
        }
    }
}

// Copy an image at inf from pixels into dest.
static void _copy_into(unsigned char *dest, unsigned char *pixels,
                       int width, int height, const tile_info &inf,
                       int ofs_x = 0, int ofs_y = 0)
{
    unsigned char *src = &pixels[4 * (inf.sy * width + inf.sx)];

    size_t src_row_size = width * 4;
    size_t dest_row_size = inf.width * 4;

    memset(dest, 0, 4 * inf.width * inf.height);

    int total_ofs_x = inf.offset_x + ofs_x;
    int total_ofs_y = inf.offset_y + ofs_y;
    int src_height = inf.ey - inf.sy;
    int src_width = inf.ex - inf.sx;

    if (total_ofs_x < 0)
    {
        src_width += total_ofs_x;
        src -= 4 * total_ofs_x;
        total_ofs_x = 0;
    }
    if (total_ofs_y < 0)
    {
        src_height += total_ofs_y;
        src -= 4 * width * total_ofs_y;
        total_ofs_y = 0;
    }

    dest += total_ofs_x * 4 + total_ofs_y * dest_row_size;

    for (int r = 0; r < src_height; r++)
    {
        memcpy(dest, src, src_width * 4);

        dest += dest_row_size;
        src += src_row_size;
    }
}

// Stores "over" on top of "under" in the location of "over".
static bool _copy_under(unsigned char *pixels, int width,
                        int height, int idx_under, int idx_over,
                        int uofs_x = 0, int uofs_y = 0)
{
    const tile_info &under = tile_main_info(idx_under);
    const tile_info &over = tile_main_info(idx_over);

    if (over.width != under.width || over.height != under.height)
        return false;

    if (over.ex - over.sx != over.width || over.ey - over.sy != over.height)
        return false;

    if (over.offset_x != 0 || over.offset_y != 0)
        return false;

    size_t image_size = under.width * under.height * 4;

    // Make a copy of the original images.
    unsigned char *under_pixels = new unsigned char[image_size];
    _copy_into(under_pixels, pixels, width, height, under, uofs_x, uofs_y);
    unsigned char *over_pixels = new unsigned char[image_size];
    _copy_into(over_pixels, pixels, width, height, over);

    // Replace the over image with the under image
    _copy_onto(pixels, width, height, under_pixels, over, false);
    // Blend the over image over top.
    _copy_onto(pixels, width, height, over_pixels, over, true);

    delete[] under_pixels;
    delete[] over_pixels;

    return true;
}

static bool _process_item_image(unsigned char *pixels, unsigned int width,
                                unsigned int height)
{
    bool success = true;
    for (int i = 0; i < NUM_POTIONS; i++)
    {
        int special = you.item_description[IDESC_POTIONS][i];
        int tile0 = TILE_POTION_OFFSET + special % 14;
        int tile1 = TILE_POT_HEALING + i;
        success &= _copy_under(pixels, width, height, tile0, tile1);
    }

    for (int i = 0; i < NUM_WANDS; i++)
    {
        int special = you.item_description[IDESC_WANDS][i];
        int tile0 = TILE_WAND_OFFSET + special % 12;
        int tile1 = TILE_WAND_FLAME + i;
        success &= _copy_under(pixels, width, height, tile0, tile1);
    }

    for (int i = 0; i < STAFF_SMITING; i++)
    {
        int special = you.item_description[IDESC_STAVES][i];
        int tile0 = TILE_STAFF_OFFSET + (special / 4) % 10;
        int tile1 = TILE_STAFF_WIZARDRY + i;
        success &= _copy_under(pixels, width, height, tile0, tile1);
    }
    for (int i = STAFF_SMITING; i < NUM_STAVES; i++)
    {
        int special = you.item_description[IDESC_STAVES][i];
        int tile0 = TILE_ROD_OFFSET + (special / 4) % 10;
        int tile1 = TILE_ROD_SMITING + i - STAFF_SMITING;
        success &= _copy_under(pixels, width, height, tile0, tile1);
    }
    for (int i = RING_FIRST_RING; i < NUM_RINGS; i++)
    {
        int special = you.item_description[IDESC_RINGS][i];
        int tile0 = TILE_RING_NORMAL_OFFSET + special % 13;
        int tile1 = TILE_RING_REGENERATION + i - RING_FIRST_RING;
        success &= _copy_under(pixels, width, height, tile0, tile1, -5, -6);
    }
    for (int i = AMU_FIRST_AMULET; i < NUM_JEWELLERY; i++)
    {
        int special = you.item_description[IDESC_RINGS][i];
        int tile0 = TILE_AMU_NORMAL_OFFSET + special % 13;
        int tile1 = TILE_AMU_RAGE + i - AMU_FIRST_AMULET;
        success &= _copy_under(pixels, width, height, tile0, tile1);
    }

    return true;
}

bool ImageManager::load_item_texture()
{
    // We need to load images in two passes: one for the title and one
    // for the items.  To handle identifiable items, the texture itself
    // is modified.  So, it cannot be loaded until after the item
    // description table has been initialised.
    GenericTexture::MipMapOptions mip = GenericTexture::MIPMAP_CREATE;
    bool success = m_textures[TEX_DEFAULT].load_texture("main.png", mip,
                                                        &_process_item_image);
    m_textures[TEX_DEFAULT].set_info(TILE_MAIN_MAX, tile_main_info);

    return success;
}

void ImageManager::unload_textures()
{
    for (int i = 0; i < TEX_MAX; i++)
    {
        m_textures[i].unload_texture();
    }
}
