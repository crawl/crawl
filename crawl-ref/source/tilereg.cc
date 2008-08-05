/*
 *  File:       tilereg.cc
 *  Summary:    Region system implementaions
 *
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 *
 *  Modified for Crawl Reference by $Author: j-p-e-g $ on $Date: 2008-03-07 $
 */

#include "AppHdr.h"
#include "cio.h"
#include "debug.h"
#include "describe.h"
#include "food.h"
#include "itemname.h"
#include "it_use2.h"
#include "item_use.h"
#include "message.h"
#include "misc.h"
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
#include "tiledef-dngn.h"

#include <SDL_opengl.h>

coord_def Region::NO_CURSOR(-1, -1);

unsigned int TextRegion::print_x;
unsigned int TextRegion::print_y;
TextRegion *TextRegion::text_mode = NULL;
int TextRegion::text_col = 0;

TextRegion *TextRegion::cursor_region= NULL;
int TextRegion::cursor_flag = 0;
unsigned int TextRegion::cursor_x;
unsigned int TextRegion::cursor_y;

const int map_colours[MAX_MAP_COL][3] =
{
    {  0,   0,   0}, // BLACK
    {128, 128, 128}, // DKGREY
    {160, 160, 160}, // MDGREY
    {192, 192, 192}, // LTGREY
    {255, 255, 255}, // WHITE

    {  0,  64, 255}, // BLUE  (actually cyan-blue)
    {128, 128, 255}, // LTBLUE
    {  0,  32, 128}, // DKBLUE (maybe too dark)

    {  0, 255,   0}, // GREEN
    {128, 255, 128}, // LTGREEN
    {  0, 128,   0}, // DKGREEN

    {  0, 255, 255}, // CYAN
    { 64, 255, 255}, // LTCYAN (maybe too pale)
    {  0, 128, 128}, // DKCYAN

    {255,   0,   0}, // RED
    {255, 128, 128}, // LTRED (actually pink)
    {128,   0,   0}, // DKRED

    {192,   0, 255}, // MAGENTA (actually blue-magenta)
    {255, 128, 255}, // LTMAGENTA
    { 96,   0, 128}, // DKMAGENTA

    {255, 255,   0}, // YELLOW
    {255, 255,  64}, // LTYELLOW (maybe too pale)
    {128, 128,   0}, // DKYELLOW

    {165,  91,   0}, // BROWN
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

void Region::resize(unsigned int _mx, unsigned int _my)
{
    mx = _mx;
    my = _my;

    recalculate();
}

void Region::place(unsigned int _sx, unsigned int _sy, unsigned int margin)
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

    unsigned int inner_x = _wx - 2 * ox;
    unsigned int inner_y = _wy - 2 * oy;

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

bool Region::inside(unsigned int x, unsigned int y)
{
    return (x >= sx && y >= sy && x <= ex && y <= ey);
}

bool Region::mouse_pos(int mouse_x, int mouse_y, int &cx, int &cy)
{
    int x = mouse_x - ox - sx;
    int y = mouse_y - oy - sy;
    bool valid = (x >= 0 && y >= 0);

    x /= dx;
    y /= dy;
    valid &= ((unsigned int)x < mx && (unsigned int)y < my);

    cx = x;
    cy = y;

    return valid;
}

TileRegion::TileRegion(ImageManager* im, unsigned int tile_x, unsigned int tile_y)
{
    ASSERT(im);
    m_image = im;
    dx = tile_x;
    dy = tile_y;
}

TileRegion::~TileRegion()
{
}

DungeonRegion::DungeonRegion(ImageManager* im, FTFont *tag_font,
                             unsigned int tile_x, unsigned int tile_y) :
    TileRegion(im, tile_x, tile_y),
    m_cx_to_gx(0),
    m_cy_to_gy(0),
    m_tag_font(tag_font)
{
    ASSERT(tag_font);
    for (unsigned int i = 0; i < CURSOR_MAX; i++)
        m_cursor[i] = NO_CURSOR;
}

DungeonRegion::~DungeonRegion()
{
}

void DungeonRegion::load_dungeon(unsigned int* tileb, int cx_to_gx, int cy_to_gy)
{
    m_tileb.clear();

    if (!tileb)
        return;

    unsigned int len = 2 * crawl_view.viewsz.x * crawl_view.viewsz.y;
    m_tileb.resize(len);
    // TODO enne - move this function into dungeonregion 
    tile_finish_dngn(tileb, cx_to_gx + mx/2, cy_to_gy + my/2);
    memcpy(&m_tileb[0], tileb, sizeof(unsigned int) * len);

    m_cx_to_gx = cx_to_gx;
    m_cy_to_gy = cy_to_gy;

    place_cursor(CURSOR_TUTORIAL, m_cursor[CURSOR_TUTORIAL]);
}

void DungeonRegion::add_quad_doll(unsigned int part, unsigned int idx, int ymax, unsigned int x, unsigned int y, int ox_spec, int oy_spec)
{
    float tex_sx, tex_sy, tex_wx, tex_wy; 
    int ox_extra, oy_extra, wx_pix, wy_pix;
    m_image->m_textures[TEX_DOLL].get_texcoord_doll(part, idx, ymax, tex_sx, tex_sy, tex_wx, tex_wy, wx_pix, wy_pix, ox_extra, oy_extra);

    if (wx_pix <= 0 || wy_pix <= 0)
        return;

    float pos_ox = (ox_spec + ox_extra) / (float)TILE_X;
    float pos_oy = (oy_spec + oy_extra) / (float)TILE_Y;

    float tex_ex = tex_sx + tex_wx;
    float tex_ey = tex_sy + tex_wy;

    float pos_sx = x + pos_ox;
    float pos_sy = y + pos_oy;
    float pos_ex = pos_sx + wx_pix / (float)TILE_X;
    float pos_ey = pos_sy + wy_pix / (float)TILE_Y;

    tile_vert v;
    v.pos_x = pos_sx;
    v.pos_y = pos_sy;
    v.tex_x = tex_sx;
    v.tex_y = tex_sy;
    m_verts.push_back(v);

    v.pos_y = pos_ey;
    v.tex_y = tex_ey;
    m_verts.push_back(v);

    v.pos_x = pos_ex;
    v.tex_x = tex_ex;
    m_verts.push_back(v);

    v.pos_y = pos_sy;
    v.tex_y = tex_sy;
    m_verts.push_back(v);
}

void TileRegion::add_quad(TextureID tex, unsigned int idx, unsigned int x, unsigned int y, int ofs_x, int ofs_y)
{
    // Generate quad
    //
    // 0 - 3
    // |   |
    // 1 - 2
    //
    // Data Layout:
    // float2 (position)
    // float2 (texcoord)

    float tex_sx, tex_sy, tex_wx, tex_wy;
    m_image->m_textures[tex].get_texcoord(idx, tex_sx, tex_sy, tex_wx, tex_wy);
    float tex_ex = tex_sx + tex_wx;
    float tex_ey = tex_sy + tex_wy;

    float pos_sx = x + ofs_x / (float)TILE_X;
    float pos_sy = y + ofs_y / (float)TILE_Y;
    float pos_ex = pos_sx + 1;
    float pos_ey = pos_sy + 1;

    // TODO enne - handle wx/wy non-standard sizes
    tile_vert v;
    v.pos_x = pos_sx;
    v.pos_y = pos_sy;
    v.tex_x = tex_sx;
    v.tex_y = tex_sy;
    m_verts.push_back(v);

    v.pos_y = pos_ey;
    v.tex_y = tex_ey;
    m_verts.push_back(v);

    v.pos_x = pos_ex;
    v.tex_x = tex_ex;
    m_verts.push_back(v);

    v.pos_y = pos_sy;
    v.tex_y = tex_sy;
    m_verts.push_back(v);
}

void DungeonRegion::draw_background(unsigned int bg, unsigned int x, unsigned int y)
{
    unsigned int bg_idx = bg & TILE_FLAG_MASK;
    if (bg_idx >= TILE_DNGN_WAX_WALL)
    {
        tile_flavour &flv = env.tile_flv[x + m_cx_to_gx][y + m_cy_to_gy];
        add_quad(TEX_DUNGEON, flv.floor, x, y);
    }

    if (bg & TILE_FLAG_BLOOD)
    {
        tile_flavour &flv = env.tile_flv[x + m_cx_to_gx][y + m_cy_to_gy];
        unsigned int offset = flv.special % tile_DNGN_count[IDX_BLOOD];
        add_quad(TEX_DUNGEON, TILE_BLOOD + offset, x, y);
    }

    add_quad(TEX_DUNGEON, bg_idx, x, y);

    if (bg & TILE_FLAG_HALO)
        add_quad(TEX_DUNGEON, TILE_HALO, x, y);

    if (bg & TILE_FLAG_SANCTUARY && !(bg & TILE_FLAG_UNSEEN))
        add_quad(TEX_DUNGEON, TILE_SANCTUARY, x, y);

    // Apply the travel exclusion under the foreground if the cell is
    // visible.  It will be applied later if the cell is unseen.
    if (bg & TILE_FLAG_EXCL_CTR && !(bg & TILE_FLAG_UNSEEN))
        add_quad(TEX_DUNGEON, TILE_TRAVEL_EXCLUSION_CENTRE_BG, x, y);
    else if (bg & TILE_FLAG_TRAV_EXCL && !(bg & TILE_FLAG_UNSEEN))
        add_quad(TEX_DUNGEON, TILE_TRAVEL_EXCLUSION_BG, x, y);

    if (bg & TILE_FLAG_RAY)
        add_quad(TEX_DUNGEON, TILE_RAY_MESH, x, y);
}

void DungeonRegion::draw_player(unsigned int x, unsigned int y)
{
    dolls_data default_doll;
    dolls_data player_doll;
    dolls_data result;

    // TODO enne - store character doll here and get gender
    for (int i = 0; i < TILEP_PARTS_TOTAL; i++)
        player_doll.parts[i] = TILEP_SHOW_EQUIP;
    int gender = 0;

    tilep_race_default(you.species, gender, you.experience_level, 
                       default_doll.parts);

    result = player_doll;

    // TODO enne - make these configurable
    result.parts[TILEP_PART_BASE] = default_doll.parts[TILEP_PART_BASE];
    result.parts[TILEP_PART_DRCHEAD] = default_doll.parts[TILEP_PART_DRCHEAD];
    result.parts[TILEP_PART_DRCWING] = default_doll.parts[TILEP_PART_DRCWING];
    bool halo = inside_halo(you.pos());
    result.parts[TILEP_PART_HALO] = halo ? TILEP_HALO_TSO : 0;

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

    draw_doll(result, x, y);
}

bool DungeonRegion::draw_objects(unsigned int fg, unsigned int x, unsigned int y)
{
    unsigned int fg_idx = fg & TILE_FLAG_MASK;

    // handled elsewhere
    if (fg_idx == TILE_PLAYER)
        return false;

    int equ_tile;
    int draco;
    int mon_tile;
    if (get_mcache_entry(fg_idx, mon_tile, equ_tile, draco))
    {
        if (!draco)
            add_quad(TEX_DEFAULT, get_base_idx_from_mcache(fg_idx), x, y);
        return true;
    }
    else if (fg_idx)
    {
        add_quad(TEX_DEFAULT, fg_idx, x, y);
    }
    return false;
}

void DungeonRegion::draw_doll(dolls_data &doll, unsigned int x, unsigned int y)
{
    int p_order[TILEP_PARTS_TOTAL] =
    {
        TILEP_PART_SHADOW,
        TILEP_PART_HALO,
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

    int flags[TILEP_PARTS_TOTAL];
    tilep_calc_flags(doll.parts, flags);

    // For skirts, boots go under the leg armour.  For pants, they go over.
    if (doll.parts[TILEP_PART_LEG] < TILEP_LEG_SKIRT_OFS)
    {
        p_order[5] = TILEP_PART_BOOTS;
        p_order[6] = TILEP_PART_LEG;
    }

    for (int i = 0; i < TILEP_PARTS_TOTAL; i++)
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

        if (doll.parts[p] && p == TILEP_PART_BOOTS
            && (doll.parts[p] == TILEP_BOOTS_NAGA_BARDING
                || doll.parts[p] == TILEP_BOOTS_CENTAUR_BARDING))
        {
            // Special case for barding.  They should be in "boots" but because
            // they're double-wide, they're stored in a different part.  We just
            // intercept it here before drawing.
            char tile = (doll.parts[p] == TILEP_BOOTS_NAGA_BARDING) ?
                            TILEP_SHADOW_NAGA_BARDING :
                            TILEP_SHADOW_CENTAUR_BARDING;

            add_quad_doll(TILEP_PART_SHADOW, tile, TILE_Y, x, y, 0, 0);
        }
        else
        {
            add_quad_doll(p, doll.parts[p], ymax, x, y, 0, 0);
        }
    }
}

void DungeonRegion::draw_draco(int colour, int mon_idx, int equ_tile, unsigned int x, unsigned int y)
{
    dolls_data doll;

    int armour = 0;
    int armour2 = 0;
    int weapon = 0;
    int weapon2 = 0;
    int arm = 0;

    for (int i = 0; i < TILEP_PARTS_TOTAL; i++)
         doll.parts[i] = 0;

    doll.parts[TILEP_PART_SHADOW] = 1;
    doll.parts[TILEP_PART_BASE] = TILEP_BASE_DRACONIAN + colour * 2;
    doll.parts[TILEP_PART_DRCWING] = 1 + colour;
    doll.parts[TILEP_PART_DRCHEAD] = 1 + colour;

    switch (mon_idx)
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
            weapon = equ_tile;
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
            weapon = equ_tile;
            armour = TILEP_BODY_BELT2;
            armour2 = TILEP_LEG_LOINCLOTH_RED;
            break;
    }

    doll.parts[TILEP_PART_HAND1] = weapon;
    doll.parts[TILEP_PART_HAND2] = weapon2;
    doll.parts[TILEP_PART_BODY]  = armour;
    doll.parts[TILEP_PART_LEG]   = armour2;
    doll.parts[TILEP_PART_ARM]   = arm;

    draw_doll(doll, x, y);
}

void DungeonRegion::draw_monster(unsigned int fg, unsigned int x, unsigned int y)
{
    // Currently, monsters only get displayed weapons (no armour)
    unsigned int fg_idx = fg & TILE_FLAG_MASK;
    if (fg_idx < TILE_MCACHE_START)
        return;

    int equ_tile;
    int draco;
    int mon_tile;

    if (!get_mcache_entry(fg_idx, mon_tile, equ_tile, draco))
        return;

    if (draco == 0)
    {
        int ofs_x, ofs_y;
        tile_get_monster_weapon_offset(mon_tile, ofs_x, ofs_y);

        add_quad_doll(TILEP_PART_HAND1, equ_tile, TILE_Y, x, y, ofs_x, ofs_y);

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
            add_quad_doll(TILEP_PART_HAND2, eq2, TILE_Y, x, y, -ofs_x, ofs_y);
        }
    }
    else
    {
        int colour;
        switch (draco)
        {
        default:
        case MONS_DRACONIAN:        colour = 0; break;
        case MONS_BLACK_DRACONIAN:  colour = 1; break;
        case MONS_YELLOW_DRACONIAN: colour = 2; break;
        case MONS_GREEN_DRACONIAN:  colour = 4; break;
        case MONS_MOTTLED_DRACONIAN:colour = 5; break;
        case MONS_PALE_DRACONIAN:   colour = 6; break;
        case MONS_PURPLE_DRACONIAN: colour = 7; break;
        case MONS_RED_DRACONIAN:    colour = 8; break;
        case MONS_WHITE_DRACONIAN:  colour = 9; break;
        }

        draw_draco(colour, mon_tile, equ_tile, x, y);
    }
}

void DungeonRegion::draw_foreground(unsigned int bg, unsigned int fg, unsigned int x, unsigned int y)
{
    unsigned int fg_idx = fg & TILE_FLAG_MASK;
    unsigned int bg_idx = bg & TILE_FLAG_MASK;

    if (fg_idx && !(fg & TILE_FLAG_FLYING))
    {
        if (bg_idx >= TILE_DNGN_LAVA && bg_idx <= TILE_DNGN_LAVA + 3)
        {
            add_quad(TEX_DEFAULT, TILE_MASK_LAVA, x, y);
        }
        else if (bg_idx >= TILE_DNGN_SHALLOW_WATER
                 && bg_idx <= TILE_DNGN_SHALLOW_WATER + 3)
        {
            add_quad(TEX_DEFAULT, TILE_MASK_SHALLOW_WATER, x, y);
        }
        else if (bg_idx >= TILE_DNGN_DEEP_WATER
                 && bg_idx <= TILE_DNGN_DEEP_WATER + 3)
        {
            add_quad(TEX_DEFAULT, TILE_MASK_DEEP_WATER, x, y);
        }
    }

    if (fg & TILE_FLAG_NET)
        add_quad(TEX_DEFAULT, TILE_TRAP_NET, x, y);

    if (fg & TILE_FLAG_S_UNDER)
        add_quad(TEX_DEFAULT, TILE_SOMETHING_UNDER, x, y);

    // Pet mark
    int status_shift = 0;
    if (fg & TILE_FLAG_PET)
    {
        add_quad(TEX_DEFAULT, TILE_HEART, x, y);
        status_shift += 10;
    }
    else if ((fg & TILE_FLAG_MAY_STAB) == TILE_FLAG_NEUTRAL)
    {
        add_quad(TEX_DEFAULT, TILE_NEUTRAL, x, y);
        status_shift += 8;
    }
    else if ((fg & TILE_FLAG_MAY_STAB) == TILE_FLAG_STAB)
    {
        add_quad(TEX_DEFAULT, TILE_STAB_BRAND, x, y);
        status_shift += 8;
    }
    else if ((fg & TILE_FLAG_MAY_STAB) == TILE_FLAG_MAY_STAB)
    {
        add_quad(TEX_DEFAULT, TILE_MAY_STAB_BRAND, x, y);
        status_shift += 5;
    }

    if (fg & TILE_FLAG_POISON)
    {
        add_quad(TEX_DEFAULT, TILE_POISON, x, y, -status_shift, 0);
        status_shift += 5;
    }

    if (fg & TILE_FLAG_ANIM_WEP)
    {
        add_quad(TEX_DEFAULT, TILE_ANIMATED_WEAPON, x, y);
    }

    if (bg & TILE_FLAG_UNSEEN && (bg != TILE_FLAG_UNSEEN || fg))
        add_quad(TEX_DEFAULT, TILE_MESH, x, y);

    if (bg & TILE_FLAG_MM_UNSEEN && (bg != TILE_FLAG_MM_UNSEEN || fg))
        add_quad(TEX_DEFAULT, TILE_MAGIC_MAP_MESH, x, y);

    // Don't let the "new stair" icon cover up any existing icons, but
    // draw it otherwise.
    if (bg & TILE_FLAG_NEW_STAIR && status_shift == 0)
        add_quad(TEX_DEFAULT, TILE_NEW_STAIR, x, y);

    if (bg & TILE_FLAG_EXCL_CTR && (bg & TILE_FLAG_UNSEEN))
        add_quad(TEX_DUNGEON, TILE_TRAVEL_EXCLUSION_CENTRE_FG, x, y);
    else if (bg & TILE_FLAG_TRAV_EXCL && (bg & TILE_FLAG_UNSEEN))
        add_quad(TEX_DUNGEON, TILE_TRAVEL_EXCLUSION_FG, x, y);

    // Tutorial cursor takes precedence over other cursors.
    if (bg & TILE_FLAG_TUT_CURSOR)
    {
        add_quad(TEX_DEFAULT, TILE_TUTORIAL_CURSOR, x, y);
    }
    else if (bg & TILE_FLAG_CURSOR)
    {
        int type = ((bg & TILE_FLAG_CURSOR) == TILE_FLAG_CURSOR1) ?
            TILE_CURSOR : TILE_CURSOR2;

        if ((bg & TILE_FLAG_CURSOR) == TILE_FLAG_CURSOR3)
           type = TILE_CURSOR3;

        add_quad(TEX_DEFAULT, type, x, y);
    }
}

void DungeonRegion::draw_cursor(cursor_type type, unsigned int tile)
{
    const coord_def &gc = m_cursor[type];
    if (gc == NO_CURSOR || !on_screen(gc))
        return;

    add_quad(TEX_DEFAULT, tile, gc.x - m_cx_to_gx, gc.y - m_cy_to_gy);
}

void DungeonRegion::render()
{
    if (m_tileb.size() == 0)
        return;

    glLoadIdentity();
    glTranslatef(sx + ox, sy + oy, 0);
    glScalef(dx, dy, 1);

    m_verts.clear();
    m_verts.reserve(4 * crawl_view.viewsz.x * crawl_view.viewsz.y);

    GLState state;
    state.array_vertex = true;
    state.array_texcoord = true;
    state.blend = true;
    state.texture = true;
    GLStateManager::set(state);

    int tile = 0;
    for (int y = 0; y < crawl_view.viewsz.y; y++)
        for (int x = 0; x < crawl_view.viewsz.x; x++)
        {
            unsigned int bg = m_tileb[tile + 1];
            draw_background(bg, x, y);

            tile += 2;
        }

    ASSERT(m_verts.size() > 0);

    if (m_verts.size() > 0)
    {
        m_image->m_textures[TEX_DUNGEON].bind();
        glVertexPointer(2, GL_FLOAT, sizeof(tile_vert), &m_verts[0].pos_x);
        glTexCoordPointer(2, GL_FLOAT, sizeof(tile_vert), &m_verts[0].tex_x);
        glDrawArrays(GL_QUADS, 0, m_verts.size());
    }


    tile = 0;
    m_verts.clear();

    int player_x = -1;
    int player_y = -1;
    bool need_doll = false;
    for (int y = 0; y < crawl_view.viewsz.y; y++)
        for (int x = 0; x < crawl_view.viewsz.x; x++)
        {
            unsigned int fg = m_tileb[tile];
            need_doll |= draw_objects(fg, x, y);

            if ((fg & TILE_FLAG_MASK) == TILE_PLAYER)
            {
                player_x = x;
                player_y = y;
            }

            tile += 2;
        }

    if (m_verts.size() > 0)
    {
        m_image->m_textures[TEX_DEFAULT].bind();
        glVertexPointer(2, GL_FLOAT, sizeof(tile_vert), &m_verts[0].pos_x);
        glTexCoordPointer(2, GL_FLOAT, sizeof(tile_vert), &m_verts[0].tex_x);
        glDrawArrays(GL_QUADS, 0, m_verts.size());
    }

    tile = 0;
    m_verts.clear();
    if (player_x != -1)
        draw_player(player_x, player_y);

    if (need_doll)
    {
        for (int y = 0; y < crawl_view.viewsz.y; y++)
            for (int x = 0; x < crawl_view.viewsz.x; x++)
            {
                unsigned int fg = m_tileb[tile];
                draw_monster(fg, x, y);

                tile += 2;
            }
    }

    if (m_verts.size() > 0)
    {
        m_image->m_textures[TEX_DOLL].bind();
        glVertexPointer(2, GL_FLOAT, sizeof(tile_vert), &m_verts[0].pos_x);
        glTexCoordPointer(2, GL_FLOAT, sizeof(tile_vert), &m_verts[0].tex_x);
        glDrawArrays(GL_QUADS, 0, m_verts.size());
    }

    tile = 0;
    m_verts.clear();
    for (int y = 0; y < crawl_view.viewsz.y; y++)
        for (int x = 0; x < crawl_view.viewsz.x; x++)
        {
            unsigned int fg = m_tileb[tile];
            unsigned int bg = m_tileb[tile + 1];
            draw_foreground(bg, fg, x, y);
            tile += 2;
        }

    draw_cursor(CURSOR_TUTORIAL, TILE_TUTORIAL_CURSOR);
    draw_cursor(CURSOR_MOUSE, see_grid(m_cursor[CURSOR_MOUSE]) ? TILE_CURSOR
                                                               : TILE_CURSOR2);

    if (m_cursor[CURSOR_TUTORIAL] != NO_CURSOR
        && on_screen(m_cursor[CURSOR_TUTORIAL]))
    {
        add_quad(TEX_DEFAULT, TILE_TUTORIAL_CURSOR,
                 m_cursor[CURSOR_TUTORIAL].x,
                 m_cursor[CURSOR_TUTORIAL].y);
    }

    if (m_verts.size() > 0)
    {
        m_image->m_textures[TEX_DEFAULT].bind();
        glVertexPointer(2, GL_FLOAT, sizeof(tile_vert), &m_verts[0].pos_x);
        glTexCoordPointer(2, GL_FLOAT, sizeof(tile_vert), &m_verts[0].tex_x);
        glDrawArrays(GL_QUADS, 0, m_verts.size());
    }

    // Draw text labels
    // TODO enne - add an option for this
    // TODO enne - be more intelligent about not covering stuff up
    for (unsigned int t = 0; t < TAG_MAX; t++)
    {
        for (unsigned int i = 0; i < m_tags[t].size(); i++)
        {
            if (!on_screen(m_tags[t][i].gc))
                continue;

            coord_def pc;
            to_screen_coords(m_tags[t][i].gc, pc);
            // center this coord, which is at the top left of gc's cell
            pc.x += dx / 2;

            const coord_def min_pos(sx, sy);
            const coord_def max_pos(ex, ey);
            m_tag_font->render_string(pc.x, pc.y, m_tags[t][i].tag.c_str(),
                                      min_pos, max_pos, WHITE, true);
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

int DungeonRegion::handle_mouse(MouseEvent &event)
{
    tiles.clear_text_tags(TAG_CELL_DESC);

    if (mouse_control::current_mode() == MOUSE_MODE_NORMAL
        || mouse_control::current_mode() == MOUSE_MODE_MACRO
        || mouse_control::current_mode() == MOUSE_MODE_MORE)
    {
        return 0;
    }

    int cx;
    int cy;

    if (!inside(event.px, event.py))
        return 0;

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

    // If adjacent, return that key (modified by shift or ctrl, etc...)
    coord_def dir = gc - you.pos();
    for (unsigned int i = 0; i < 9; i++)
    {
        if (dir_dx[i] == dir.x && dir_dy[i] == dir.y)
        {
            if (event.mod & MOD_SHIFT)
                return cmd_shift[i];
            else if (event.mod & MOD_CTRL)
                return cmd_ctrl[i];
            else
                return cmd_normal[i];
        }
    }

    // Otherwise, travel to that grid.
    if (!in_bounds(gc))
        return 0;

    // Activate travel.
    start_travel(gc);

    return CK_MOUSE_CMD;
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

    return (x >= 0 && (unsigned int)x < mx && y >= 0 && (unsigned int)y < my);
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
        && type == CURSOR_MOUSE)
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

        m_cursor[type] = result;
    }
    else
    {
        m_cursor[type] = gc;
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


InventoryTile::InventoryTile()
{
    tile = 0;
    idx = -1;
    quantity = -1;
    key = 0;
    flag = 0;
}

bool InventoryTile::empty() const
{
    return (idx == -1);
}

InventoryRegion::InventoryRegion(ImageManager* im, unsigned tile_x, unsigned int tile_y) :
    TileRegion(im, tile_x, tile_y),
    m_flavour(NULL), m_cursor(NO_CURSOR), m_need_to_pack(false)
{
}

InventoryRegion::~InventoryRegion()
{
    delete[] m_flavour;
}

void InventoryRegion::clear()
{
    m_items.clear();
    m_verts.clear();
}

void InventoryRegion::on_resize()
{
    delete[] m_flavour;
    if (mx * my <= 0)
        return;

    m_flavour = new unsigned char[mx * my];
    for (unsigned int i = 0; i < mx * my; i++)
    {
        m_flavour[i] = random2((unsigned char)~0);
    }
}

void InventoryRegion::update(unsigned int num, InventoryTile *items)
{
    m_items.clear();
    for (unsigned int i = 0; i < num; i++)
    {
        m_items.push_back(items[i]);
    }

    m_need_to_pack = true;
}

void InventoryRegion::update_slot(unsigned int slot, InventoryTile &desc)
{
    while (m_items.size() <= slot)
    {
        InventoryTile temp;
        m_items.push_back(temp);
    }

    m_items[slot] = desc;

    m_need_to_pack = true;
}

void InventoryRegion::render()
{
    if (m_need_to_pack)
        pack_verts();

    if (m_verts.size() == 0)
        return;

    glLoadIdentity();
    glTranslatef(sx + ox, sy + oy, 0);
    glScalef(dx, dy, 1);

    GLState state;
    state.array_vertex = true;
    state.array_texcoord = true;
    state.blend = true;
    state.texture = true;
    GLStateManager::set(state);

    m_image->m_textures[TEX_DUNGEON].bind();
    glVertexPointer(2, GL_FLOAT, sizeof(tile_vert), &m_verts[0].pos_x);
    glTexCoordPointer(2, GL_FLOAT, sizeof(tile_vert), &m_verts[0].tex_x);
    glDrawArrays(GL_QUADS, 0, m_base_verts);

    m_image->m_textures[TEX_DEFAULT].bind();
    glDrawArrays(GL_QUADS, m_base_verts, m_verts.size() - m_base_verts);
}

void InventoryRegion::add_quad_char(char c, unsigned int x, unsigned int y, int ofs_x, int ofs_y)
{
    int idx = TILE_CHAR00 + (c - 32) / 8;
    int subidx = c & 7;

    float tex_sx, tex_sy, tex_wx, tex_wy;
    m_image->m_textures[TEX_DEFAULT].get_texcoord(idx, tex_sx, tex_sy, tex_wx, tex_wy);
    tex_wx /= 4.0f;
    tex_wy /= 2.0f;
    tex_sx += (subidx % 4) * tex_wx;
    tex_sy += (subidx / 4) * tex_wy;
    float tex_ex = tex_sx + tex_wx;
    float tex_ey = tex_sy + tex_wy;

    float pos_sx = x + ofs_x / (float)TILE_X;
    float pos_sy = y + ofs_y / (float)TILE_Y;
    float pos_ex = pos_sx + 0.25f;
    float pos_ey = pos_sy + 0.5f;

    tile_vert v;
    v.pos_x = pos_sx;
    v.pos_y = pos_sy;
    v.tex_x = tex_sx;
    v.tex_y = tex_sy;
    m_verts.push_back(v);

    v.pos_y = pos_ey;
    v.tex_y = tex_ey;
    m_verts.push_back(v);

    v.pos_x = pos_ex;
    v.tex_x = tex_ex;
    m_verts.push_back(v);

    v.pos_y = pos_sy;
    v.tex_y = tex_sy;
    m_verts.push_back(v);
}

void InventoryRegion::pack_verts()
{
    m_need_to_pack = false;
    m_verts.clear();

    // ensure the cursor has been placed
    place_cursor(m_cursor);

    // Pack base separately, as it comes from a different texture...
    unsigned int i = 0;
    for (unsigned int y = 0; y < my; y++)
    {
        if (i >= m_items.size())
            break;

        for (unsigned int x = 0; x < mx; x++)
        {
            if (i >= m_items.size())
                break;

            InventoryTile &item = m_items[i++];

            if (item.flag & TILEI_FLAG_FLOOR)
                add_quad(TEX_DUNGEON, get_floor_tile_idx() 
                         + m_flavour[i] % get_num_floor_flavors(), x, y);
            else
                add_quad(TEX_DUNGEON, TILE_ITEM_SLOT, x, y);
        }
    }

    // Make note of how many verts are used by the base
    m_base_verts = m_verts.size();

    i = 0;
    for (unsigned int y = 0; y < my; y++)
    {
        if (i >= m_items.size())
            break;

        for (unsigned int x = 0; x < mx; x++)
        {
            if (i >= m_items.size())
                break;

            InventoryTile &item = m_items[i++];

            if (item.flag & TILEI_FLAG_EQUIP)
            {
                if (item.flag & TILEI_FLAG_CURSE)
                    add_quad(TEX_DEFAULT, TILE_ITEM_SLOT_EQUIP_CURSED, x, y);
                else
                    add_quad(TEX_DEFAULT, TILE_ITEM_SLOT_EQUIP, x, y);
            }
            else if (item.flag & TILEI_FLAG_CURSE)
                add_quad(TEX_DEFAULT, TILE_ITEM_SLOT_CURSED, x, y);

            // TODO enne - need better graphic here
            if (item.flag & TILEI_FLAG_SELECT)
                add_quad(TEX_DEFAULT, TILE_ITEM_SLOT_SELECTED, x, y);

            if (item.flag & TILEI_FLAG_CURSOR)
                add_quad(TEX_DEFAULT, TILE_CURSOR, x, y);

            if (item.tile)
                add_quad(TEX_DEFAULT, item.tile, x, y);

            if (item.quantity != -1)
            {
                unsigned int num = item.quantity;
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
                    add_quad_char('0' + c100, x, y, offset, 0);
                    offset += offset_amount;
                }

                int c10 = help/10;
                if (c10 || c100)
                {
                    add_quad_char('0' + c10, x, y, offset, 0);
                    offset += offset_amount;
                }

                int c1 = help % 10;
                add_quad_char('0' + c1, x, y, offset, 0);
            }


            if (item.flag & TILEI_FLAG_TRIED)
                add_quad_char('?', x, y, 0, TILE_Y / 2);
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
        m_need_to_pack = true;
    }

    m_cursor = cursor;

    if (m_cursor == NO_CURSOR || cursor_index() >= m_items.size())
        return;

    // Add cursor to new location
    m_items[cursor_index()].flag |= TILEI_FLAG_CURSOR;
    m_need_to_pack = true;
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
                tile_item_eat_floor(idx);
            else
                describe_item(mitm[idx]);
        }
        else
        {
            describe_item(you.inv[idx]);
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
                if (is_throwable(item, player_size(PSIZE_BODY)))
                    tip += "\n[Ctrl-L-Click] Fire (f)";
                break;
            case OBJ_WEAPONS + EQUIP_OFFSET:
                tip += "Unwield";
                if (is_throwable(item, player_size(PSIZE_BODY)))
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

MapRegion::MapRegion(unsigned int pixsz) :
    m_buf(NULL), 
    m_far_view(false)
{
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

struct map_vertex
{
    float x;
    float y;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

void MapRegion::render()
{
    if (m_min_gx > m_max_gx || m_min_gy > m_max_gy)
        return;

    // [enne] - GL_POINTS should probably be used here, but there's (apparently)
    // a bug in the OpenGL driver that I'm using and it doesn't respect
    // glPointSize unless GL_SMOOTH_POINTS is on.  GL_SMOOTH_POINTS is
    // *terrible* for performance if it has to fall back on software rendering,
    // so instead we'll just make quads.

    glLoadIdentity();
    glTranslatef(sx + ox, sy + oy, 0);
    glScalef(dx, dx, 1);

    std::vector<map_vertex> verts;
    verts.reserve(4 * (m_max_gx - m_min_gx + 1) * (m_max_gy - m_min_gy + 1));

    for (unsigned int x = m_min_gx; x <= m_max_gx; x++)
    {
        for (unsigned int y = m_min_gy; y <= m_max_gy; y++)
        {
            map_feature f = (map_feature)m_buf[x + y * mx];
            map_colour c = m_colours[f];

            unsigned int pos_x = x - m_min_gx;
            unsigned int pos_y = y - m_min_gy;

            map_vertex v;
            v.r = map_colours[c][0];
            v.g = map_colours[c][1];
            v.b = map_colours[c][2];
            v.a = 255;

            v.x = pos_x;
            v.y = pos_y;
            verts.push_back(v);

            v.x = pos_x;
            v.y = pos_y + 1;
            verts.push_back(v);

            v.x = pos_x + 1;
            v.y = pos_y + 1;
            verts.push_back(v);

            v.x = pos_x + 1;
            v.y = pos_y;
            verts.push_back(v);
        }
    }

    GLState state;
    state.array_vertex = true;
    state.array_colour = true;
    GLStateManager::set(state);

    glVertexPointer(2, GL_FLOAT, sizeof(map_vertex), &verts[0].x);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(map_vertex), &verts[0].r);
    glDrawArrays(GL_QUADS, 0, verts.size());

    // TODO enne - make sure we're drawing within the map square here...

    // Draw window box.
    if (m_win_start.x == -1 && m_win_end.x == -1)
        return;

    verts.clear();
    glLoadIdentity();
    glTranslatef(sx + ox, sy + oy, 0);

    map_vertex v;
    int c = (int)Options.tile_window_col;
    v.r = map_colours[c][0];
    v.g = map_colours[c][1];
    v.b = map_colours[c][2];
    v.a = 255;

    v.x = (int)dx * (m_win_start.x - (int)m_min_gx);
    v.y = (int)dy * (m_win_start.y - (int)m_min_gy);
    verts.push_back(v);

    v.y = (int)dy * (m_win_end.y - (int)m_min_gy) + 1;
    verts.push_back(v);

    v.x = (int)dx * (m_win_end.x - (int)m_min_gx) + 1;
    verts.push_back(v);

    v.y = (int)dy * (m_win_start.y - (int)m_min_gy);
    verts.push_back(v);

    glVertexPointer(2, GL_FLOAT, sizeof(map_vertex), &verts[0].x);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(map_vertex), &verts[0].r);
    glDrawArrays(GL_LINE_LOOP, 0, verts.size());
}

void MapRegion::update_offsets()
{
    // adjust offsets to center map
    ox = (wx - dx * (m_max_gx - m_min_gx)) / 2;
    oy = (wy - dy * (m_max_gy - m_min_gy)) / 2;
}

void MapRegion::set(unsigned int gx, unsigned int gy, map_feature f)
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

    update_offsets();
}

void MapRegion::set_window(const coord_def &start, const coord_def &end)
{
    m_win_start = start;
    m_win_end = end;
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

    update_offsets();

    if (m_buf)
        memset(m_buf, 0, sizeof(*m_buf) * mx * my);
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
            tiles.load_dungeon(you.pos().x, you.pos().y);
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
            tiles.load_dungeon(gc.x, gc.y);
        return 0;
    case MouseEvent::PRESS:
        if (event.button == MouseEvent::LEFT)
        {
            if (!in_bounds(gc))
                return 0;

            start_travel(gc);
        }
        else if (event.button == MouseEvent::RIGHT)
        {
            m_far_view = true;
            tiles.load_dungeon(gc.x, gc.y);
        }
        return CK_MOUSE_CMD;
    case MouseEvent::RELEASE:
        if ((event.button == MouseEvent::RIGHT) && m_far_view)
        {
            tiles.load_dungeon(you.pos().x, you.pos().y);
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
    for (unsigned int idx = 0; idx < mx*(my-1); idx++)
    {
        cbuf[idx] = cbuf[idx + mx];
        abuf[idx] = abuf[idx + mx];
    }

    for (unsigned int idx = mx*(my-1); idx < mx*my; idx++)
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

    unsigned int size = mx * my;
    cbuf = new unsigned char[size];
    abuf = new unsigned char[size];

    for (unsigned int i = 0; i < size; i++)
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

void TextRegion::addstr_aux(char *buffer, unsigned int len)
{
    int x = print_x - cx_ofs;
    int y = print_y - cy_ofs;
    int adrs = y * mx;
    int head = x;
    int tail = x + len - 1;

    adjust_region(&head, &tail, y);

    for (unsigned int i = 0; i < len && x + i < mx; i++)
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
    for (unsigned int i = cx; i < mx; i++)
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
        cursor_region ->erase_cursor();
        cursor_region = NULL;
    }

    if (cursor_flag)
    {
        text_mode->draw_cursor(print_x, print_y);
        cursor_x = print_x;
        cursor_y = print_y;
        cursor_region = text_mode;
    }
}

int  TextRegion::wherex()
{
    return print_x + 1;
}

int  TextRegion::wherey()
{
    return print_y + 1;
}

void TextRegion::_setcursortype(int curstype)
{
    cursor_flag = curstype;
    if (cursor_region != NULL)
        cursor_region->erase_cursor();

    if (curstype)
    {
        text_mode->draw_cursor(print_x, print_y);
        cursor_x = print_x;
        cursor_y = print_y;
        cursor_region = text_mode;
    }
}

void TextRegion::draw_cursor(int x, int y)
{
    // TODO enne
}

void TextRegion::erase_cursor()
{
    // TODO enne
}

void TextRegion::render()
{
    m_font->render_textblock(sx + ox, sy + oy, cbuf, abuf, mx, my);
}

void TextRegion::clear()
{
    for (unsigned int i = 0; i < mx * my; i++)
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

    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return 0;

    if (!inside(event.px, event.py))
        return 0;

    if (event.event != MouseEvent::PRESS || event.button != MouseEvent::LEFT)
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
    if (m_overlay)
    {
        unsigned int height;
        bool found = false;
        for (height = my; height > 0; height--)
        {
            unsigned char *buf = &cbuf[mx * (height - 1)];
            for (unsigned int x = 0; x < mx; x++)
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
            for (unsigned int i = 0; i < 4; i++)
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
}

CRTRegion::CRTRegion(FTFont *font) : TextRegion(font)
{
}

int CRTRegion::handle_mouse(MouseEvent &event)
{
    // TODO enne - clicking on menu items? We could probably
    // determine which items were clicked based on text size.
    return 0;
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

    if (!m_textures[TEX_TITLE].load_texture("title.png", mip))
        return false;

    return true;
}

static void _copy_onto(unsigned char *pixels, unsigned int width,
                       unsigned int height, unsigned char *src,
                       int idx, bool blend)
{
    const unsigned int tile_size = 32;
    const unsigned int tiles_per_row = width / tile_size;

    unsigned int row = idx / tiles_per_row;
    unsigned int col = idx % tiles_per_row;

    unsigned char *dest = &pixels[4 * 32 * (row * width + col)];

    size_t dest_row_size = width * 4;
    size_t src_row_size = 32 * 4;

    if (blend)
    {
        for (unsigned int r = 0; r < 32; r++)
        {
            for (unsigned int c = 0; c < 32; c++)
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
        for (unsigned int r = 0; r < 32; r++)
        {
            memcpy(dest, src, src_row_size);

            dest += dest_row_size; 
            src += src_row_size;
        }
    }
}

// Copy a 32x32 image at index idx from pixels into dest.
static void _copy_into(unsigned char *dest, unsigned char *pixels, 
                       unsigned int width,
                       unsigned int height, int idx)
{
    const unsigned int tile_size = 32;
    const unsigned int tiles_per_row = width / tile_size;

    unsigned int row = idx / tiles_per_row;
    unsigned int col = idx % tiles_per_row;

    unsigned char *src = &pixels[4 * 32 * (row * width + col)];

    size_t src_row_size = width * 4;
    size_t dest_row_size = 32 * 4;

    for (unsigned int r = 0; r < 32; r++)
    {
        memcpy(dest, src, dest_row_size);

        dest += dest_row_size; 
        src += src_row_size;
    }
}

// Stores "over" on top of "under" in the location of "over".
static void _copy_under(unsigned char *pixels, unsigned int width,
                        unsigned int height, int idx_under, int idx_over)
{
    size_t image_size = 32 * 32 * 4;    

    // Make a copy of the original images.
    unsigned char *under = new unsigned char[image_size];
    _copy_into(under, pixels, width, height, idx_under);
    unsigned char *over = new unsigned char[image_size];
    _copy_into(over, pixels, width, height, idx_over);

    // Replace the over image with the under image
    _copy_onto(pixels, width, height, under, idx_over, false);
    // Blend the over image over top.
    _copy_onto(pixels, width, height, over, idx_over, true);

    delete[] under;
    delete[] over;
}

static bool _process_item_image(unsigned char *pixels, 
                                unsigned int width, unsigned int height)
{
    for (int i = 0; i < NUM_POTIONS; i++)
    {
        int special = you.item_description[IDESC_POTIONS][i];
        int tile0 = TILE_POTION_OFFSET + special % 14;
        int tile1 = TILE_POT_HEALING + i;
        _copy_under(pixels, width, height, tile0, tile1);
    }

    for (int i = 0; i < NUM_WANDS; i++)
    {
        int special = you.item_description[IDESC_WANDS][i];
        int tile0 = TILE_WAND_OFFSET + special % 12;
        int tile1 = TILE_WAND_FLAME + i;
        _copy_under(pixels, width, height, tile0, tile1);
    }

    for (int i = 0; i < STAFF_SMITING; i++)
    {
        int special = you.item_description[IDESC_STAVES][i];
        int tile0 = TILE_STAFF_OFFSET + (special / 4) % 10;
        int tile1 = TILE_STAFF_WIZARDRY + i;
        _copy_under(pixels, width, height, tile0, tile1);
    }
    for (int i = STAFF_SMITING; i < NUM_STAVES; i++)
    {
        int special = you.item_description[IDESC_STAVES][i];
        int tile0 = TILE_ROD_OFFSET + (special / 4) % 10;
        int tile1 = TILE_ROD_SMITING + i - STAFF_SMITING;
        _copy_under(pixels, width, height, tile0, tile1);
    }
   
    // TODO enne - fix rtiles so that it can accept PNGs.
    {
        size_t image_size = 32 * 32 * 4;    
        unsigned char *mesh = new unsigned char[image_size];

        for (unsigned int i = 0; i < image_size; i += 4)
        {
            mesh[i] = mesh[i+1] = mesh[i+2] = 0;
            mesh[i+3] = 110;
        }
        _copy_onto(pixels, width, height, mesh, TILE_MESH, false);

        for (unsigned int i = 0; i < image_size; i += 4)
        {
            mesh[i] = 70;
            mesh[i+1] = 70;
            mesh[i+2] = 180;
            mesh[i+3] = 120;
        }
        _copy_onto(pixels, width, height, mesh, TILE_MAGIC_MAP_MESH, false);

        delete[] mesh;
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
    return m_textures[TEX_DEFAULT].load_texture("tile.png", mip, 
                                                &_process_item_image);
}

void ImageManager::unload_textures()
{
    for (unsigned int i = 0; i < TEX_MAX; i++)
    {
        m_textures[i].unload_texture();
    }
}
