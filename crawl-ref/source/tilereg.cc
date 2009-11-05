/*
 *  File:       tilereg.cc
 *  Summary:    Region system implementations
 *
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#include "AppHdr.h"

#ifdef USE_TILE

#include "cio.h"
#include "coord.h"
#include "envmap.h"
#include "debug.h"
#include "describe.h"
#include "files.h"
#include "food.h"
#include "itemname.h"
#include "item_use.h"
#include "items.h"
#include "jobs.h"
#include "los.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "menu.h"
#include "newgame.h"
#include "mon-util.h"
#include "options.h"
#include "player.h"
#include "religion.h"
#include "species.h"
#include "spells3.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stash.h"
#include "stuff.h"
#include "terrain.h"
#include "transfor.h"
#include "travel.h"
#include "view.h"
#include "viewgeom.h"

#include "tilereg.h"
#include "tiles.h"
#include "tilefont.h"
#include "tilesdl.h"
#include "tilemcache.h"
#include "tiledef-gui.h"

#include <sys/stat.h>
#include <SDL_opengl.h>

/* These aren't defined on Win32 */
#ifndef S_IWUSR
#define S_IWUSR (unsigned int)-1
#endif
#ifndef S_IRUSR
#define S_IRUSR (unsigned int)-1
#endif

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
const int cmd_shift[9]  = {'B', 'J', 'N', 'H', '5', 'L', 'Y', 'K', 'U'};
const int cmd_ctrl[9]   = {CONTROL('B'), CONTROL('J'), CONTROL('N'),
                           CONTROL('H'), 'X', CONTROL('L'),
                           CONTROL('Y'), CONTROL('K'), CONTROL('U')};
const int cmd_dir[9]    = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};

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
#if 0
    // Not needed? (jpeg)
    m_dirty = true;
#endif
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
    m_buf_doll(&im->m_textures[TEX_PLAYER]),
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
    m_buf_dngn.add(bg_idx, x, y);

    if (bg_idx > TILE_DNGN_UNSEEN)
    {
        if (bg & TILE_FLAG_WAS_SECRET)
            m_buf_dngn.add(TILE_DNGN_DETECTED_SECRET_DOOR, x, y);

        if (bg & TILE_FLAG_BLOOD)
        {
            tile_flavour &flv = env.tile_flv[x + m_cx_to_gx][y + m_cy_to_gy];
            int offset = flv.special % tile_dngn_count(TILE_BLOOD);
            m_buf_dngn.add(TILE_BLOOD + offset, x, y);
        }

        if (bg & TILE_FLAG_HALO)
            m_buf_dngn.add(TILE_HALO, x, y);

        if (!(bg & TILE_FLAG_UNSEEN))
        {
            if (bg & TILE_FLAG_SANCTUARY)
                m_buf_dngn.add(TILE_SANCTUARY, x, y);

            // Apply the travel exclusion under the foreground if the cell is
            // visible.  It will be applied later if the cell is unseen.
            if (bg & TILE_FLAG_EXCL_CTR)
                m_buf_dngn.add(TILE_TRAVEL_EXCLUSION_CENTRE_BG, x, y);
            else if (bg & TILE_FLAG_TRAV_EXCL)
                m_buf_dngn.add(TILE_TRAVEL_EXCLUSION_BG, x, y);
        }
        if (bg & TILE_FLAG_RAY)
            m_buf_dngn.add(TILE_RAY, x, y);
        else if (bg & TILE_FLAG_RAY_OOR)
            m_buf_dngn.add(TILE_RAY_OUT_OF_RANGE, x, y);
    }
}

static dolls_data player_doll;
static int gender = -1;

// Saves player doll definitions into dolls.txt.
// Returns true if successful, else false.
static bool _save_doll_data(int mode, int num, const dolls_data* dolls)
{
    // Save mode, num, and all dolls into dolls.txt.
    std::string dollsTxtString = datafile_path("dolls.txt", false, true);

    struct stat stFileInfo;
    stat(dollsTxtString.c_str(), &stFileInfo);

    // Write into the current directory instead if we didn't find the file
    // or don't have write permissions.
    const char *dollsTxt = (dollsTxtString.c_str()[0] == 0
                              || !(stFileInfo.st_mode & S_IWUSR)) ? "dolls.txt"
                            : dollsTxtString.c_str();

    FILE *fp = NULL;
    if ((fp = fopen(dollsTxt, "w+")) != NULL)
    {
        fprintf(fp, "MODE=%s\n",
                    (mode == TILEP_MODE_EQUIP)   ? "EQUIP" :
                    (mode == TILEP_MODE_LOADING) ? "LOADING"
                                                 : "DEFAULT");

        fprintf(fp, "NUM=%02d\n", num == -1 ? 0 : num);

        // Print some explanatory comments. May contain no spaces!
        fprintf(fp, "#Legend:\n");
        fprintf(fp, "#***:equipment/123:index/000:none\n");
        fprintf(fp, "#Shadow/Base/Cloak/Boots/Legs/Body/Gloves/Weapon/Shield/Hair/Beard/Helmet/Halo/Enchant/DrcHead/DrcWing\n");
        fprintf(fp, "#Sh:Bse:Clk:Bts:Leg:Bdy:Glv:Wpn:Shd:Hai:Brd:Hlm:Hal:Enc:Drc:Wng\n");
        char fbuf[80];
        for (unsigned int i = 0; i < NUM_MAX_DOLLS; ++i)
        {
            tilep_print_parts(fbuf, dolls[i]);
            fprintf(fp, "%s\n", fbuf);
        }
        fclose(fp);

        return (true);
    }

    return (false);
}

// Loads player doll definitions from (by default) dolls.txt.
// Returns true if file found, else false.
static bool _load_doll_data(const char *fn, dolls_data *dolls, int max,
                            tile_doll_mode *mode, int *cur)
{
    char fbuf[1024];
    FILE *fp  = NULL;

    std::string dollsTxtString = datafile_path(fn, false, true);

    struct stat stFileInfo;
    stat(dollsTxtString.c_str(), &stFileInfo);

    // Try to read from the current directory instead if we didn't find the
    // file or don't have reading permissions.
    const char *dollsTxt = (dollsTxtString.c_str()[0] == 0
                              || !(stFileInfo.st_mode & S_IRUSR)) ? "dolls.txt"
                            : dollsTxtString.c_str();


    if ( (fp = fopen(dollsTxt, "r")) == NULL )
    {
        // File doesn't exist. By default, use equipment settings.
        *mode = TILEP_MODE_EQUIP;
        return (false);
    }
    else
    {
        memset(fbuf, 0, sizeof(fbuf));
        // Read mode from file.
        if (fscanf(fp, "%s", fbuf) != EOF)
        {
            if (strcmp(fbuf, "MODE=DEFAULT") == 0)
                *mode = TILEP_MODE_DEFAULT;
            else if (strcmp(fbuf, "MODE=EQUIP") == 0)
                *mode = TILEP_MODE_EQUIP; // Nothing else to be done.
        }
        // Read current doll from file.
        if (fscanf(fp, "%s", fbuf) != EOF)
        {
            if (strncmp(fbuf, "NUM=", 4) == 0)
            {
                sscanf(fbuf, "NUM=%d", cur);
                if (*cur < 0 || *cur >= NUM_MAX_DOLLS)
                    *cur = 0;
            }
        }

        if (max == 1)
        {
            // Load only one doll, either the one defined by NUM or
            // use the default/equipment setting.
            if (*mode != TILEP_MODE_LOADING)
            {
                if (gender == -1)
                    gender = coinflip();

                if (*mode == TILEP_MODE_DEFAULT)
                    tilep_job_default(you.char_class, gender, dolls[0].parts);

                // If we don't need to load a doll, return now.
                fclose(fp);
                return (true);
            }

            int count = 0;
            while (fscanf(fp, "%s", fbuf) != EOF)
            {
                if (fbuf[0] == '#') // Skip comment lines.
                    continue;

                if (*cur == count++)
                {
                    tilep_scan_parts(fbuf, dolls[0]);
                    gender = get_gender_from_tile(dolls[0].parts);
                    break;
                }
            }
#if 0
            // Probably segfaults within the tile edit menu.
            if (*cur >= count)
            {
                mprf(MSGCH_WARN, "Doll %d could not be found in '%s'.",
                                 *cur, dollsTxt);
            }
#endif
        }
        else // Load up to max dolls from file.
        {
            for (int count = 0; count < max && fscanf(fp, "%s", fbuf) != EOF; )
            {
                if (fbuf[0] == '#') // Skip comment lines.
                    continue;

                tilep_scan_parts(fbuf, dolls[count++]);
            }
        }

        fclose(fp);
        return (true);
    }
}

void init_player_doll()
{
    dolls_data dolls[NUM_MAX_DOLLS];

    for (int i = 0; i < NUM_MAX_DOLLS; i++)
        for (int j = 0; j < TILEP_PART_MAX; j++)
            dolls[i].parts[j] = TILEP_SHOW_EQUIP;

    tile_doll_mode mode = TILEP_MODE_LOADING;
    int cur = 0;
    _load_doll_data("dolls.txt", dolls, NUM_MAX_DOLLS, &mode, &cur);

    if (mode == TILEP_MODE_LOADING)
    {
        player_doll = dolls[cur];
        return;
    }

    if (gender == -1)
        gender = coinflip();

    for (int i = 0; i < TILEP_PART_MAX; i++)
        player_doll.parts[i] = TILEP_SHOW_EQUIP;
    tilep_race_default(you.species, gender, you.experience_level, player_doll.parts);

    if (mode == TILEP_MODE_EQUIP)
        return;

    tilep_job_default(you.char_class, gender, player_doll.parts);
}

static int _get_random_doll_part(int p)
{
    ASSERT(p >= 0 && p <= TILEP_PART_MAX);
    return (tile_player_part_start[p]
            + random2(tile_player_part_count[p]));
}

static void _fill_doll_part(dolls_data &doll, int p)
{
    ASSERT(p >= 0 && p <= TILEP_PART_MAX);
    doll.parts[p] = _get_random_doll_part(p);
}

static void _create_random_doll(dolls_data &rdoll)
{
    // All dolls roll for these.
    _fill_doll_part(rdoll, TILEP_PART_BODY);
    _fill_doll_part(rdoll, TILEP_PART_HAND1);
    _fill_doll_part(rdoll, TILEP_PART_LEG);
    _fill_doll_part(rdoll, TILEP_PART_BOOTS);
    _fill_doll_part(rdoll, TILEP_PART_HAIR);

    // The following only are rolled with 50% chance.
    if (coinflip())
        _fill_doll_part(rdoll, TILEP_PART_CLOAK);
    if (coinflip())
        _fill_doll_part(rdoll, TILEP_PART_ARM);
    if (coinflip())
        _fill_doll_part(rdoll, TILEP_PART_HAND2);
    if (coinflip())
        _fill_doll_part(rdoll, TILEP_PART_HELM);

    // Only male dolls get a chance at a beard.
    if (rdoll.parts[TILEP_PART_BASE] % 2 == 1 && one_chance_in(4))
        _fill_doll_part(rdoll, TILEP_PART_BEARD);
}

static void _fill_doll_equipment(dolls_data &result)
{
    // Base tile.
    if (result.parts[TILEP_PART_BASE] == TILEP_SHOW_EQUIP)
    {
        if (gender == -1)
            gender = get_gender_from_tile(player_doll.parts);
        tilep_race_default(you.species, gender, you.experience_level,
                           result.parts);
    }

    // Main hand.
    if (result.parts[TILEP_PART_HAND1] == TILEP_SHOW_EQUIP)
    {
        const int item = you.equip[EQ_WEAPON];
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
            result.parts[TILEP_PART_HAND1] = TILEP_HAND1_BLADEHAND;
        else if (item == -1)
            result.parts[TILEP_PART_HAND1] = 0;
        else
            result.parts[TILEP_PART_HAND1] = tilep_equ_weapon(you.inv[item]);
    }
    // Off hand.
    if (result.parts[TILEP_PART_HAND2] == TILEP_SHOW_EQUIP)
    {
        const int item = you.equip[EQ_SHIELD];
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
            result.parts[TILEP_PART_HAND2] = TILEP_HAND2_BLADEHAND;
        else if (item == -1)
            result.parts[TILEP_PART_HAND2] = 0;
        else
            result.parts[TILEP_PART_HAND2] = tilep_equ_shield(you.inv[item]);
    }
    // Body armour.
    if (result.parts[TILEP_PART_BODY] == TILEP_SHOW_EQUIP)
    {
        const int item = you.equip[EQ_BODY_ARMOUR];
        if (item == -1)
            result.parts[TILEP_PART_BODY] = 0;
        else
            result.parts[TILEP_PART_BODY] = tilep_equ_armour(you.inv[item]);
    }
    // Cloak.
    if (result.parts[TILEP_PART_CLOAK] == TILEP_SHOW_EQUIP)
    {
        const int item = you.equip[EQ_CLOAK];
        if (item == -1)
            result.parts[TILEP_PART_CLOAK] = 0;
        else
            result.parts[TILEP_PART_CLOAK] = tilep_equ_cloak(you.inv[item]);
    }
    // Helmet.
    if (result.parts[TILEP_PART_HELM] == TILEP_SHOW_EQUIP)
    {
        const int item = you.equip[EQ_HELMET];
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
    // Boots.
    if (result.parts[TILEP_PART_BOOTS] == TILEP_SHOW_EQUIP)
    {
        const int item = you.equip[EQ_BOOTS];
        if (item != -1)
            result.parts[TILEP_PART_BOOTS] = tilep_equ_boots(you.inv[item]);
        else if (player_mutation_level(MUT_HOOVES))
            result.parts[TILEP_PART_BOOTS] = TILEP_BOOTS_HOOVES;
        else
            result.parts[TILEP_PART_BOOTS] = 0;
    }
    // Gloves.
    if (result.parts[TILEP_PART_ARM] == TILEP_SHOW_EQUIP)
    {
        const int item = you.equip[EQ_GLOVES];
        if (item != -1)
            result.parts[TILEP_PART_ARM] = tilep_equ_gloves(you.inv[item]);
        else if (you.has_claws(false) >= 3)
            result.parts[TILEP_PART_ARM] = TILEP_ARM_CLAWS;
        else
            result.parts[TILEP_PART_ARM] = 0;
    }
    // Halo.
    if (result.parts[TILEP_PART_HALO] == TILEP_SHOW_EQUIP)
    {
        const bool halo = inside_halo(you.pos());
        result.parts[TILEP_PART_HALO] = halo ? TILEP_HALO_TSO : 0;
    }
    // Enchantments.
    if (result.parts[TILEP_PART_ENCH] == TILEP_SHOW_EQUIP)
    {
        result.parts[TILEP_PART_ENCH] =
            (you.duration[DUR_LIQUID_FLAMES] ? TILEP_ENCH_STICKY_FLAME : 0);
    }
    // Draconian head/wings
    if (player_genus(GENPC_DRACONIAN))
    {
        int base = 0;
        int head = 0;
        int wing = 0;
        tilep_draconian_init(you.species, you.experience_level, base, head, wing);

        if (result.parts[TILEP_PART_DRCHEAD] == TILEP_SHOW_EQUIP)
            result.parts[TILEP_PART_DRCHEAD] = head;
        if (result.parts[TILEP_PART_DRCWING] == TILEP_SHOW_EQUIP)
            result.parts[TILEP_PART_DRCWING] = wing;
    }

    // Various other slots.
    for (int i = 0; i < TILEP_PART_MAX; i++)
        if (result.parts[i] == TILEP_SHOW_EQUIP)
            result.parts[i] = 0;
}

// Writes equipment information into per-character doll file.
void save_doll_file(FILE *dollf)
{
    ASSERT(dollf);

    dolls_data result = player_doll;
    _fill_doll_equipment(result);

    // Write into file.
    char fbuf[80];
    tilep_print_parts(fbuf, result);
    fprintf(dollf, "%s\n", fbuf);

    if (you.attribute[ATTR_HELD] > 0)
        fprintf(dollf, "net\n");
}

void DungeonRegion::pack_player(int x, int y)
{
    dolls_data result = player_doll;
    _fill_doll_equipment(result);
    pack_doll(result, x, y);
}

void pack_doll_buf(TileBuffer& buf, const dolls_data &doll, int x, int y)
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
        TILEP_PART_DRCHEAD  // 15
    };

    int flags[TILEP_PART_MAX];
    tilep_calc_flags(doll.parts, flags);

    // For skirts, boots go under the leg armour.  For pants, they go over.
    if (doll.parts[TILEP_PART_LEG] < TILEP_LEG_SKIRT_OFS)
    {
        p_order[6] = TILEP_PART_BOOTS;
        p_order[7] = TILEP_PART_LEG;
    }

    // Special case bardings from being cut off.
    bool is_naga = (doll.parts[TILEP_PART_BASE] >= TILEP_BASE_NAGA
                    && doll.parts[TILEP_PART_BASE]
                       < tilep_species_to_base_tile(SP_NAGA + 1));

    if (doll.parts[TILEP_PART_BOOTS] >= TILEP_BOOTS_NAGA_BARDING
        && doll.parts[TILEP_PART_BOOTS] <= TILEP_BOOTS_NAGA_BARDING_RED)
    {
        flags[TILEP_PART_BOOTS] = is_naga ? TILEP_FLAG_NORMAL : TILEP_FLAG_HIDE;
    }

    bool is_cent = (doll.parts[TILEP_PART_BASE] >= TILEP_BASE_CENTAUR
                    && doll.parts[TILEP_PART_BASE]
                       < tilep_species_to_base_tile(SP_CENTAUR + 1));

    if (doll.parts[TILEP_PART_BOOTS] >= TILEP_BOOTS_CENTAUR_BARDING
        && doll.parts[TILEP_PART_BOOTS] <= TILEP_BOOTS_CENTAUR_BARDING_RED)
    {
        flags[TILEP_PART_BOOTS] = is_cent ? TILEP_FLAG_NORMAL : TILEP_FLAG_HIDE;
    }

    for (int i = 0; i < TILEP_PART_MAX; i++)
    {
        int p = p_order[i];

        if (!doll.parts[p] || flags[p] == TILEP_FLAG_HIDE)
            continue;

        int ymax = TILE_Y;

        if (flags[p] == TILEP_FLAG_CUT_CENTAUR
            || flags[p] == TILEP_FLAG_CUT_NAGA)
        {
            ymax = 18;
        }

        buf.add(doll.parts[p], x, y, 0, 0, true, ymax);
    }
}

void DungeonRegion::pack_doll(const dolls_data &doll, int x, int y)
{
    pack_doll_buf(m_buf_doll, doll, x, y);
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
        m_buf_doll.add(dinfo[i].idx, x, y, dinfo[i].ofs_x, dinfo[i].ofs_y);
}

void DungeonRegion::pack_foreground(unsigned int bg, unsigned int fg, int x, int y)
{
    unsigned int fg_idx = fg & TILE_FLAG_MASK;
    unsigned int bg_idx = bg & TILE_FLAG_MASK;

    if (fg_idx && fg_idx <= TILE_MAIN_MAX)
        m_buf_main.add(fg_idx, x, y);

    if (fg_idx && !(fg & TILE_FLAG_FLYING))
    {
        if (tile_dngn_equal(TILE_DNGN_LAVA, bg_idx))
            m_buf_main.add(TILE_MASK_LAVA, x, y);
        else if (tile_dngn_equal(TILE_DNGN_SHALLOW_WATER, bg_idx)
                 || tile_dngn_equal(TILE_DNGN_SHALLOW_WATER_DISTURBANCE,
                                    bg_idx))
        {
            m_buf_main.add(TILE_MASK_SHALLOW_WATER, x, y);
        }
        else if (tile_dngn_equal(TILE_DNGN_SHALLOW_WATER_MURKY, bg_idx)
                 || tile_dngn_equal(TILE_DNGN_SHALLOW_WATER_MURKY_DISTURBANCE,
                                    bg_idx))
        {
            m_buf_main.add(TILE_MASK_SHALLOW_WATER_MURKY, x, y);
        }
        else if (tile_dngn_equal(TILE_DNGN_DEEP_WATER, bg_idx))
            m_buf_main.add(TILE_MASK_DEEP_WATER, x, y);
        else if (tile_dngn_equal(TILE_DNGN_DEEP_WATER_MURKY, bg_idx))
            m_buf_main.add(TILE_MASK_DEEP_WATER_MURKY, x, y);
        else if (tile_dngn_equal(TILE_SHOALS_DEEP_WATER, bg_idx))
            m_buf_main.add(TILE_MASK_DEEP_WATER_SHOALS, x, y);
        else if (tile_dngn_equal(TILE_SHOALS_SHALLOW_WATER, bg_idx))
            m_buf_main.add(TILE_MASK_SHALLOW_WATER_SHOALS, x, y);
    }

    if (fg & TILE_FLAG_NET)
        m_buf_doll.add(TILEP_TRAP_NET, x, y);

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
    if (fg & TILE_FLAG_FLAME)
    {
        m_buf_main.add(TILE_FLAME, x, y, -status_shift, 0);
        status_shift += 5;
    }

    if (fg & TILE_FLAG_ANIM_WEP)
        m_buf_main.add(TILE_ANIMATED_WEAPON, x, y);

    if (bg & TILE_FLAG_UNSEEN && (bg != TILE_FLAG_UNSEEN || fg))
        m_buf_main.add(TILE_MESH, x, y);

    if (bg & TILE_FLAG_OOR && (bg != TILE_FLAG_OOR || fg))
        m_buf_main.add(TILE_OOR_MESH, x, y);

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
    for (int y = 0; y < crawl_view.viewsz.y; ++y)
        for (int x = 0; x < crawl_view.viewsz.x; ++x)
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
    pack_cursor(CURSOR_MOUSE, see_cell(m_cursor[CURSOR_MOUSE]) ? TILE_CURSOR
                                                               : TILE_CURSOR2);

    if (m_cursor[CURSOR_TUTORIAL] != NO_CURSOR
        && on_screen(m_cursor[CURSOR_TUTORIAL]))
    {
        m_buf_main.add(TILE_TUTORIAL_CURSOR, m_cursor[CURSOR_TUTORIAL].x,
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

// #define DEBUG_TILES_REDRAW
void DungeonRegion::render()
{
#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering DungeonRegion\n");
#endif
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

            int width    = m_tag_font->string_width(str);
            tag_def &def = tag_show(ep);

            const int buffer = 2;

            def.left  = -width / 2 - buffer;
            def.right =  width / 2 + buffer;
            def.text  = str;
            def.type  = t;

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
        && event.event == MouseEvent::PRESS
        && event.button == MouseEvent::LEFT)
    {
        you.last_clicked_grid = m_cursor[CURSOR_MOUSE];
        return CK_MOUSE_CLICK;
    }

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
        || mouse_control::current_mode() == MOUSE_MODE_TARGET_PATH
        || mouse_control::current_mode() == MOUSE_MODE_TARGET_DIR)
    {
        if (event.event == MouseEvent::MOVE)
        {
            return CK_MOUSE_MOVE;
        }
        else if (event.event == MouseEvent::PRESS
                 && event.button == MouseEvent::LEFT && on_screen(gc))
        {
            you.last_clicked_grid = m_cursor[CURSOR_MOUSE];
            return CK_MOUSE_CLICK;
        }

        return 0;
    }

    if (event.event != MouseEvent::PRESS)
        return 0;

    you.last_clicked_grid = m_cursor[CURSOR_MOUSE];

    if (you.pos() == gc)
    {
        switch (event.button)
        {
        case MouseEvent::LEFT:
        {
            if (!(event.mod & MOD_SHIFT))
                return 'g';

            const dungeon_feature_type feat = grd(gc);
            switch (feat_stair_direction(feat))
            {
            case CMD_GO_DOWNSTAIRS:
                return ('>');
            case CMD_GO_UPSTAIRS:
                return ('<');
            default:
                if (feat_is_altar(feat)
                    && player_can_join_god(feat_altar_god(feat)))
                {
                    return ('p');
                }
                return 0;
            }
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
    coord_def result = gc;

    // If we're only looking for a direction, put the mouse
    // cursor next to the player to let them know that their
    // spell/wand will only go one square.
    if (mouse_control::current_mode() == MOUSE_MODE_TARGET_DIR
        && type == CURSOR_MOUSE && gc != NO_CURSOR)
    {
        coord_def delta = gc - you.pos();

        int ax = abs(delta.x);
        int ay = abs(delta.y);

        result = you.pos();
        if (1000 * ay < 414 * ax)
            result += (delta.x > 0) ? coord_def(1, 0) : coord_def(-1, 0);
        else if (1000 * ax < 414 * ay)
            result += (delta.y > 0) ? coord_def(0, 1) : coord_def(0, -1);
        else if (delta.x > 0)
            result += (delta.y > 0) ? coord_def(1, 1) : coord_def(1, -1);
        else if (delta.x < 0)
            result += (delta.y > 0) ? coord_def(-1, 1) : coord_def(-1, -1);
    }

    if (m_cursor[type] != result)
    {
        m_dirty = true;
        m_cursor[type] = result;
        if (type == CURSOR_MOUSE)
            you.last_clicked_grid = coord_def();
    }
}

bool DungeonRegion::update_tip_text(std::string& tip)
{
    // TODO enne - it would be really nice to use the tutorial
    // descriptions here for features, monsters, etc...
    // Unfortunately, that would require quite a bit of rewriting
    // and some parsing of formatting to get that to work.

    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return (false);

    if (m_cursor[CURSOR_MOUSE] == NO_CURSOR)
        return (false);
    if (!map_bounds(m_cursor[CURSOR_MOUSE]))
        return (false);

    if (m_cursor[CURSOR_MOUSE] == you.pos())
    {
        tip = you.your_name;
        tip += " (";
        tip += get_species_abbrev(you.species);
        tip += get_class_abbrev(you.char_class);
        tip += ")";

        if (igrd(m_cursor[CURSOR_MOUSE]) != NON_ITEM)
            tip += "\n[L-Click] Pick up items (g)";

        const dungeon_feature_type feat = grd(m_cursor[CURSOR_MOUSE]);
        const command_type dir = feat_stair_direction(feat);
        if (dir != CMD_NO_CMD)
        {
            tip += "\n[Shift-L-Click] ";
            if (feat == DNGN_ENTER_SHOP)
                tip += "enter shop";
            else if (feat_is_gate(feat))
                tip += "enter gate";
            else
                tip += "use stairs";

            if (dir == CMD_GO_DOWNSTAIRS)
                tip += " (>)";
            else
                tip += " (<)";
        }
        else if (feat_is_altar(feat) && player_can_join_god(feat_altar_god(feat)))
            tip += "\n[Shift-L-Click] pray on altar (p)";

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

        if (!cell_is_solid(m_cursor[CURSOR_MOUSE]))
        {
            const monsters *mon = monster_at(m_cursor[CURSOR_MOUSE]));
            if (!mon || mons_friendly(mon))
                tip = "[L-Click] Move\n";
            else if (mon)
            {
                tip = mon->name(DESC_CAP_A);
                tip += "\n[L-Click] Attack\n";
            }
        }

        tip += "[R-Click] Describe";
    }
    else
    {
        if (i_feel_safe() && !cell_is_solid(m_cursor[CURSOR_MOUSE]))
            tip = "[L-Click] Travel\n";

        tip += "[R-Click] Describe";
    }

    return (true);
}

class alt_desc_proc
{
public:
    alt_desc_proc(int _w, int _h) { w = _w; h = _h; }

    int width() { return w; }
    int height() { return h; }
    void nextline()
    {
        ostr << "\n";
    }
    void print(const std::string &str)
    {
        ostr << str;
    }

    static int count_newlines(const std::string &str)
    {
        int count = 0;
        for (size_t i = 0; i < str.size(); i++)
        {
            if (str[i] == '\n')
                count++;
        }
        return count;
    }

    // Remove trailing newlines.
    static void trim(std::string &str)
    {
        int idx = str.size();
        while (--idx >= 0)
        {
            if (str[idx] != '\n')
                break;
        }
        str.resize(idx + 1);
    }

    // rfind consecutive newlines and truncate.
    static bool chop(std::string &str)
    {
        int loc = -1;
        for (size_t i = 1; i < str.size(); i++)
            if (str[i] == '\n' && str[i-1] == '\n')
                loc = i;

        if (loc == -1)
            return (false);

        str.resize(loc);
        return (true);
    }

    void get_string(std::string &str)
    {
        str = replace_all(ostr.str(), "\n\n\n\n", "\n\n");
        str = replace_all(str, "\n\n\n", "\n\n");

        trim(str);
        while (count_newlines(str) > h)
        {
            if (!chop(str))
                break;
        }
    }

protected:
    int w;
    int h;
    std::ostringstream ostr;
};

bool DungeonRegion::update_alt_text(std::string &alt)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return (false);

    const coord_def &gc = m_cursor[CURSOR_MOUSE];

    if (gc == NO_CURSOR)
        return (false);
    if (!map_bounds(gc))
        return (false);
    if (!is_terrain_seen(gc))
        return (false);
    if (you.last_clicked_grid == gc)
        return (false);

    describe_info inf;
    if (see_cell(gc))
        get_square_desc(gc, inf, true);
    else if (grd(gc) != DNGN_FLOOR)
        get_feature_desc(gc, inf);
    else
    {
        // For plain floor, output the stash description.
        std::string stash = get_stash_desc(gc.x, gc.y);
        if (!stash.empty())
            inf.body << "$" << stash;
    }

    alt_desc_proc proc(crawl_view.msgsz.x, crawl_view.msgsz.y);
    process_description<alt_desc_proc>(proc, inf);

    proc.get_string(alt);

    // Suppress floor description
    if (alt == "Floor.")
    {
        alt.clear();
        return (false);
    }

    return (true);
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
    t.gc  = gc;

    m_tags[type].push_back(t);
}

void DungeonRegion::add_overlay(const coord_def &gc, int idx)
{
    tile_overlay over;
    over.gc  = gc;
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

InventoryRegion::InventoryRegion(ImageManager* im, FTFont *tag_font,
                                 int tile_x, int tile_y) :
    TileRegion(im, tag_font, tile_x, tile_y),
    m_flavour(NULL),
    m_buf_dngn(&im->m_textures[TEX_DUNGEON]),
    m_buf_main(&im->m_textures[TEX_DEFAULT]),
    m_buf_spells(&im->m_textures[TEX_GUI]),
    m_cursor(NO_CURSOR)
{
}

InventoryRegion::~InventoryRegion()
{
    delete[] m_flavour;
    m_flavour = NULL;
}

void InventoryRegion::clear()
{
    m_items.clear();
    m_buf_dngn.clear();
    m_buf_main.clear();
    m_buf_spells.clear();
}

void InventoryRegion::on_resize()
{
    delete[] m_flavour;
    if (mx * my <= 0)
        return;

    m_flavour = new unsigned char[mx * my];
    for (int i = 0; i < mx * my; ++i)
        m_flavour[i] = random2((unsigned char)~0);
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
#if 0
    // Not needed? (jpeg)
    m_dirty = true;
#endif
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

#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering InventoryRegion\n");
#endif
    set_transform();
    m_buf_dngn.draw();
    m_buf_spells.draw();
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

        std::string desc = "";
        if (Options.tile_display != TDSP_INVENT)
        {
            const spell_type spell = (spell_type) idx;
            if (spell == NUM_SPELLS)
            {
                snprintf(info, INFO_SIZE, "Memorise spells (%d spell levels "
                                          "available)",
                         player_spell_levels());
                desc = info;
            }
            else if (Options.tile_display == TDSP_SPELLS)
            {
                snprintf(info, INFO_SIZE, "%d MP    %s    (%s)",
                         spell_difficulty(spell),
                         spell_title(spell),
                         failure_rate_to_string(spell_fail(spell)));
                desc = info;
            }
            else // if (Options.tile_display == TDSP_MEMORISE)
            {
                snprintf(info, INFO_SIZE, "%s    (%s)    %d/%d spell slot%s",
                         spell_title(spell),
                         failure_rate_to_string(spell_fail(spell)),
                         spell_levels_required(spell),
                         player_spell_levels(),
                         spell_levels_required(spell) > 1 ? "s" : "");
                desc = info;
            }
        }
        else if (floor && is_valid_item(mitm[idx]))
            desc = mitm[idx].name(DESC_PLAIN);
        else if (!floor && is_valid_item(you.inv[idx]))
            desc = you.inv[idx].name(DESC_INVENTORY_EQUIP);

        if (!desc.empty())
        {
            m_tag_font->render_string(x, y, desc.c_str(),
                                      min_pos, max_pos, WHITE, false, 200);
        }
    }
}

void InventoryRegion::add_quad_char(char c, int x, int y, int ofs_x, int ofs_y)
{
    int num = c - '0';
    assert(num >= 0 && num <= 9);
    int idx = TILE_NUM0 + num;

    m_buf_main.add(idx, x, y, ofs_x, ofs_y, false);
}

void InventoryRegion::pack_buffers()
{
    m_buf_dngn.clear();
    m_buf_main.clear();
    m_buf_spells.clear();

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
                if (i >= (unsigned int) mx * my)
                    break;

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

            if (Options.tile_display != TDSP_INVENT)
            {
                if (item.flag & TILEI_FLAG_MELDED)
                    m_buf_main.add(TILE_MESH, x, y);
            }
            else if (item.flag & TILEI_FLAG_EQUIP)
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
            {
                if (Options.tile_display == TDSP_INVENT)
                    m_buf_main.add(item.tile, x, y);
                else
                    m_buf_spells.add(item.tile, x, y);
            }

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

            if (Options.tile_display == TDSP_INVENT && item.special)
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
    return (m_cursor.x + m_cursor.y * mx);
}

void InventoryRegion::place_cursor(const coord_def &cursor)
{
    if (m_cursor != NO_CURSOR && cursor_index() < m_items.size())
    {
        m_items[cursor_index()].flag &= ~TILEI_FLAG_CURSOR;
#if 0
        // Not needed? (jpeg)
        m_dirty = true;
#endif
    }

    if (m_cursor != cursor)
        you.last_clicked_item = -1;

    m_cursor = cursor;

    if (m_cursor == NO_CURSOR || cursor_index() >= m_items.size())
        return;

    // Add cursor to new location
    m_items[cursor_index()].flag |= TILEI_FLAG_CURSOR;
    m_dirty = true;
}

int InventoryRegion::handle_spells_mouse(MouseEvent &event, int item_idx)
{
    const spell_type spell = (spell_type) m_items[item_idx].idx;
    if (event.button == MouseEvent::LEFT)
    {
        if (spell == NUM_SPELLS)
        {
            if (can_learn_spell() && has_spells_to_memorise(false))
            {
                Options.tile_display = TDSP_MEMORISE;
                tiles.update_inventory();
            }
            else
            {
                // FIXME: Doesn't work. The message still disappears instantly!
                you.last_clicked_item = item_idx;
                tiles.set_need_redraw();
            }
            return CK_MOUSE_CMD;
        }
        if (Options.tile_display == TDSP_SPELLS)
        {
            you.last_clicked_item = item_idx;
            tiles.set_need_redraw();
            // Use Z rather than z, seeing how there are no mouseclick macros.
            if (!cast_a_spell(false, spell))
                flush_input_buffer( FLUSH_ON_FAILURE );
        }
        else if (Options.tile_display == TDSP_MEMORISE)
        {
            you.last_clicked_item = item_idx;
            tiles.set_need_redraw();
            if (!learn_spell(spell, m_items[item_idx].special))
                flush_input_buffer( FLUSH_ON_FAILURE );
            else if (!can_learn_spell(true)
                     || !has_spells_to_memorise(true, spell))
            {
                // Jump back to spells list. (Really, this should only happen
                // if there aren't any other spells to memorise, but this
                // doesn't work for some reason.)
                Options.tile_display = TDSP_SPELLS;
                tiles.update_inventory();
            }
        }
        return CK_MOUSE_CMD;
    }
    else if (spell != NUM_SPELLS && event.button == MouseEvent::RIGHT)
    {
        describe_spell(spell);
        redraw_screen();
        return CK_MOUSE_CMD;
    }
    return 0;
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

    if (m_items[item_idx].key == 0 && Options.tile_display != TDSP_INVENT)
        return handle_spells_mouse(event, item_idx);

    bool on_floor = m_items[item_idx].flag & TILEI_FLAG_FLOOR;

    ASSERT(idx >= 0);

    // TODO enne - this is all really only valid for the on-screen inventory
    // Do we subclass InventoryRegion for the onscreen and offscreen versions?
    char key = m_items[item_idx].key;
    if (key)
        return key;

    if (event.button == MouseEvent::LEFT)
    {
        you.last_clicked_item = item_idx;
        tiles.set_need_redraw();
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
                you.last_clicked_item = item_idx;
                tiles.set_need_redraw();
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
    // There's nothing you can do with an empty box if you can't unwield it.
    if (!equipped && item.sub_type == MISC_EMPTY_EBONY_CASKET)
        return (false);

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
            return (true);

        // You can't unwield/fire a wielded cursed weapon/staff
        // but cursed armour and rings can be unwielded without problems.
        return (!_is_true_equipped_item(item));
    }

    // Mummies can't do anything with food or potions.
    if (you.species == SP_MUMMY)
        return (item.base_type != OBJ_POTIONS && item.base_type != OBJ_FOOD);

    // In all other cases you can use the item in some way.
    return (true);
}

void _update_spell_tip_text(std::string& tip, int flag)
{
    if (Options.tile_display == TDSP_SPELLS)
    {
        if (flag & TILEI_FLAG_MELDED)
            tip = "You cannot cast this spell right now.";
        else
            tip = "[L-Click] Cast (z)";

        tip += "\n[R-Click] Describe (I)";
    }
    else if (Options.tile_display == TDSP_MEMORISE)
    {
        if (flag & TILEI_FLAG_MELDED)
            tip = "You don't have enough slots for this spell right now.";
        else
            tip = "[L-Click] Memorise (M)";

        tip += "\n[R-Click] Describe";
    }
}

bool InventoryRegion::update_tip_text(std::string& tip)
{
    if (m_cursor == NO_CURSOR)
        return (false);

    unsigned int item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return (false);

    int idx = m_items[item_idx].idx;

    // TODO enne - consider subclassing this class, rather than depending
    // on "key" to determine if this is the crt inventory or the on screen one.
    bool display_actions = (m_items[item_idx].key == 0
                    && mouse_control::current_mode() == MOUSE_MODE_COMMAND);

    if (Options.tile_display != TDSP_INVENT)
    {
        if (m_items[item_idx].idx == NUM_SPELLS)
        {
            if (m_items[item_idx].flag & TILEI_FLAG_MELDED)
                tip = "You cannot learn any spells right now.";
            else
                tip = "Memorise spells (M)";
        }
        else
            _update_spell_tip_text(tip, m_items[item_idx].flag);
        return (true);
    }

    // TODO enne - should the command keys here respect keymaps?

    if (m_items[item_idx].flag & TILEI_FLAG_FLOOR)
    {
        const item_def &item = mitm[idx];

        if (!is_valid_item(item))
            return (false);

        tip = "";
        if (m_items[item_idx].key)
        {
            tip = m_items[item_idx].key;
            tip += " - ";
        }

        tip += item.name(DESC_NOCAP_A);

        if (!display_actions)
            return (true);

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
        if (!is_valid_item(item))
            return (false);

        tip = item.name(DESC_INVENTORY_EQUIP);

        if (!display_actions)
            return (true);

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
                tip += "Wield (w)";
                if (is_throwable(&you, item))
                    tip += "\n[Ctrl-L-Click] Fire (f)";
                break;
            case OBJ_WEAPONS + EQUIP_OFFSET:
                tip += "Unwield (w-)";
                if (is_throwable(&you, item))
                    tip += "\n[Ctrl-L-Click] Fire (f)";
                break;
            case OBJ_MISCELLANY:
                if (item.sub_type >= MISC_DECK_OF_ESCAPE
                    && item.sub_type <= MISC_DECK_OF_DEFENCE)
                {
                    tip += "Wield (w)";
                    break;
                }
                tip += "Evoke (V)";
                break;
            case OBJ_MISCELLANY + EQUIP_OFFSET:
                if (item.sub_type >= MISC_DECK_OF_ESCAPE
                    && item.sub_type <= MISC_DECK_OF_DEFENCE)
                {
                    tip += "Draw a card (v)\n";
                    tip += "[Ctrl-L-Click] Unwield (w-)";
                    break;
                }
                // else fall-through
            case OBJ_STAVES + EQUIP_OFFSET: // rods - other staves handled above
                tip += "Evoke (v)\n";
                tip += "[Ctrl-L-Click] Unwield (w-)";
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
                    tip += "\n[Ctrl-L-Click] Unwield (w-)";
                else if (item.sub_type == MI_STONE
                            && you.has_spell(SPELL_SANDBLAST)
                         || item.sub_type == MI_ARROW
                            && you.has_spell(SPELL_STICKS_TO_SNAKES))
                {
                    // For Sandblast and Sticks to Snakes,
                    // respectively.
                    tip += "\n[Ctrl-L-Click] Wield (w)";
                }
                break;
            case OBJ_WANDS:
                tip += "Evoke (V)";
                if (wielded)
                    tip += "\n[Ctrl-L-Click] Unwield (w-)";
                break;
            case OBJ_BOOKS:
                if (item_type_known(item)
                    && item.sub_type != BOOK_MANUAL
                    && item.sub_type != BOOK_DESTRUCTION)
                {
                    tip += "Memorise (M)";
                    if (wielded)
                        tip += "\n[Ctrl-L-Click] Unwield (w-)";
                    break;
                }
                // else fall-through
            case OBJ_SCROLLS:
                tip += "Read (r)";
                if (wielded)
                    tip += "\n[Ctrl-L-Click] Unwield (w-)";
                break;
            case OBJ_POTIONS:
                tip += "Quaff (q)";
                // For Sublimation of Blood.
                if (wielded)
                    tip += "\n[Ctrl-L-Click] Unwield (w-)";
                else if (item_type_known(item)
                         && is_blood_potion(item)
                         && you.has_spell(SPELL_SUBLIMATION_OF_BLOOD))
                {
                    tip += "\n[Ctrl-L-Click] Wield (w)";
                }
                break;
            case OBJ_FOOD:
                tip += "Eat (e)";
                // For Sublimation of Blood.
                if (wielded)
                    tip += "\n[Ctrl-L-Click] Unwield (w-)";
                else if (item.sub_type == FOOD_CHUNK
                         && you.has_spell(
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
                    tip += "[Ctrl-L-Click] Unwield (w-)";
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
            else if (you.has_spell(SPELL_BONE_SHARDS))
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

    return (true);
}

void _update_spell_alt_text(std::string &alt, int idx)
{
    const spell_type spell = (spell_type) idx;

    if (spell == NUM_SPELLS)
    {
        alt.clear();
        return;
    }
    describe_info inf;
    get_spell_desc(spell, inf);

    alt_desc_proc proc(crawl_view.msgsz.x, crawl_view.msgsz.y);
    process_description<alt_desc_proc>(proc, inf);

    proc.get_string(alt);
}

bool InventoryRegion::update_alt_text(std::string &alt)
{
    if (m_cursor == NO_CURSOR)
        return (false);

    unsigned int item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return (false);

    if (you.last_clicked_item >= 0
        && item_idx == (unsigned int) you.last_clicked_item)
    {
        return (false);
    }

    int idx = m_items[item_idx].idx;
    if (m_items[item_idx].key == 0 && Options.tile_display != TDSP_INVENT)
    {
        _update_spell_alt_text(alt, idx);
        return (true);
    }

    const item_def *item;
    if (m_items[item_idx].flag & TILEI_FLAG_FLOOR)
        item = &mitm[idx];
    else
        item = &you.inv[idx];

    if (!is_valid_item(*item))
        return (false);

    describe_info inf;
    get_item_desc(*item, inf, true);

    alt_desc_proc proc(crawl_view.msgsz.x, crawl_view.msgsz.y);
    process_description<alt_desc_proc>(proc, inf);

    proc.get_string(alt);

    return (true);
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
    m_buf    = new unsigned char[size];
    memset(m_buf, 0, sizeof(unsigned char) * size);
}

void MapRegion::init_colours()
{
    // TODO enne - the options array for colours should be
    // tied to the map feature enumeration to avoid this function.
    m_colours[MF_UNSEEN]        = (map_colour)Options.tile_unseen_col;
    m_colours[MF_FLOOR]         = (map_colour)Options.tile_floor_col;
    m_colours[MF_WALL]          = (map_colour)Options.tile_wall_col;
    m_colours[MF_MAP_FLOOR]     = (map_colour)Options.tile_floor_col; // TODO enne
    m_colours[MF_MAP_WALL]      = (map_colour)Options.tile_mapped_wall_col;
    m_colours[MF_DOOR]          = (map_colour)Options.tile_door_col;
    m_colours[MF_ITEM]          = (map_colour)Options.tile_item_col;
    m_colours[MF_MONS_HOSTILE]  = (map_colour)Options.tile_monster_col;
    m_colours[MF_MONS_FRIENDLY] = (map_colour)Options.tile_friendly_col;
    m_colours[MF_MONS_NEUTRAL]  = (map_colour)Options.tile_neutral_col;
    m_colours[MF_MONS_NO_EXP]   = (map_colour)Options.tile_plant_col;
    m_colours[MF_STAIR_UP]      = (map_colour)Options.tile_upstairs_col;
    m_colours[MF_STAIR_DOWN]    = (map_colour)Options.tile_downstairs_col;
    m_colours[MF_STAIR_BRANCH]  = (map_colour)Options.tile_feature_col;
    m_colours[MF_FEATURE]       = (map_colour)Options.tile_feature_col;
    m_colours[MF_WATER]         = (map_colour)Options.tile_water_col;
    m_colours[MF_LAVA]          = (map_colour)Options.tile_lava_col;
    m_colours[MF_TRAP]          = (map_colour)Options.tile_trap_col;
    m_colours[MF_EXCL_ROOT]     = (map_colour)Options.tile_excl_centre_col;
    m_colours[MF_EXCL]          = (map_colour)Options.tile_excluded_col;
    m_colours[MF_PLAYER]        = (map_colour)Options.tile_player_col;
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
        for (int y = m_min_gy; y <= m_max_gy; y++)
        {
            map_feature f = (map_feature)m_buf[x + y * mx];
            map_colour c  = m_colours[f];

            float pos_x = x - m_min_gx;
            float pos_y = y - m_min_gy;
            m_buf_map.add(pos_x, pos_y, pos_x + 1, pos_y + 1, map_colours[c]);
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

#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering MapRegion\n");
#endif
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
#if 0
    // Not needed? (jpeg)
    m_dirty = true;
#endif
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

    recenter();
#if 0
    // Not needed? (jpeg)
    m_dirty = true;
#endif
}

void MapRegion::set_window(const coord_def &start, const coord_def &end)
{
    m_win_start = start;
    m_win_end   = end;

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
        return (false);

    tip = "[L-Click] Travel / [R-Click] View";
    return (true);
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
    delete[] cbuf;
    delete[] abuf;

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

        if (c == '\r')
            continue;

        if (c == '\n')
        {
            c = 0;
            newline = true;
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
    // special case: check for '0' char: map to space
    if (ch == 0)
        ch = ' ';
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

#if 0
    if (cursor_region != NULL && cursor_flag)
    {
        cursor_x = -1;
        cursor_y = -1;
        cursor_region = NULL;
    }
#endif
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
#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering TextRegion\n");
#endif
    if (this == TextRegion::cursor_region && cursor_x > 0 && cursor_y > 0)
    {
        int idx = cursor_x + mx * cursor_y;

        unsigned char char_back = cbuf[idx];
        unsigned char col_back  = abuf[idx];

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
        return (false);

    tip = "[L-Click] Rest / Search for a while";
    return (true);
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
        return (false);

    tip = "[L-Click] Browse message history";
    return (true);
}

void MessageRegion::set_overlay(bool is_overlay)
{
    m_overlay = is_overlay;
}

void MessageRegion::render()
{
#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering MessageRegion\n");
#endif
    int idx = -1;
    unsigned char char_back = 0;
    unsigned char col_back = 0;

    if (!m_overlay && !m_alt_text.empty())
    {
        coord_def min_pos(sx, sy);
        coord_def max_pos(ex, ey);
        m_font->render_string(sx + ox, sy + oy, m_alt_text.c_str(),
                              min_pos, max_pos, WHITE, false);
        return;
    }

    if (this == TextRegion::cursor_region && cursor_x > 0 && cursor_y > 0)
    {
        idx = cursor_x + mx * cursor_y;
        char_back = cbuf[idx];
        col_back  = abuf[idx];

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

            glLoadIdentity();

            ShapeBuffer buff;
            VColour col(100, 100, 100, 100);
            buff.add(sx, sy, ex, sy + height, col);
            buff.draw();
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

    const int heading_indent  = 10;
    const int tile_indent     = 20;
    const int text_indent     = (Options.tile_menu_icons ? 58 : 20);
    const int max_tile_height = (Options.tile_menu_icons ? 32 : 0);
    const int entry_buffer    = 1;
    const VColour selected_colour(50, 50, 10, 255);

    m_font_buf.clear();
    m_shape_buf.clear();
    m_line_buf.clear();
    for (int t = 0; t < TEX_MAX; t++)
        m_tile_buf[t].clear();

    int column = 0;
    if (!Options.tile_menu_icons)
        set_num_columns(1);
    const int max_columns  = std::min(2, m_max_columns);
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

        if (height + max_entry_height > end_height && column <= max_columns)
        {
            height = 0;
            column++;
        }

        int text_width  = m_font_entry->string_width(m_entries[i].text);
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
                    int tile      = m_entries[i].tiles[t].tile;
                    TextureID tex = m_entries[i].tiles[t].tex;
                    m_tile_buf[tex].add_unscaled(tile, m_entries[i].sx,
                                                 m_entries[i].sy,
                                                 m_entries[i].tiles[t].ymax);
//                     m_tile_buf[tex].add(tile, m_entries[i].sx, m_entries[i].sy,
//                                         0, 0, true, TILE_Y);
                }
            }
            else
            {
                m_entries[i].sx = text_sx;
                entry_height = text_height;
            }

            int text_sy = m_entries[i].sy;
            text_sy += (entry_height - m_font_entry->char_height()) / 2;
            // Split menu entries that don't fit into a single line into
            // two lines.
            if (Options.tile_menu_icons
                && text_sx + text_width > entry_start + column_width)
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
#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering MenuRegion\n");
#endif
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

    e.heading  = (me->level == MEL_TITLE || me->level == MEL_SUBTITLE);
    e.selected = me->selected();
    e.key      = me->hotkeys.size() > 0 ? me->hotkeys[0] : 0;
    e.sx = e.sy = e.ex = e.ey = 0;
    e.tiles.clear();
    me->get_tiles(e.tiles);

    m_dirty = true;
}

void MenuRegion::on_resize()
{
    // Probably needed, even though for me nothing went wrong when
    // I commented it out. (jpeg)
    m_dirty = true;
}

int MenuRegion::maxpagesize() const
{
    // TODO enne - this is a conservative guess.
    // It would be better to make menus use a dynamic number of items per page,
    // but it'd require a lot more refactoring of menu.cc to handle that.

    const int lines = count_linebreaks(m_more);
    const int more_height = (lines + 1) * m_font_entry->char_height();

    // Similar to the definition of max_entry_height in place_entries().
    // HACK: Increasing height by 1 to make sure all items actually fit
    //       on the page, though this introduces a few too many empty lines.
    const int div = (Options.tile_menu_icons ? 32
                                             : m_font_entry->char_height() + 1);

    const int pagesize = ((my - more_height) / div) * m_max_columns;

    // Upper limit for inventory menus. (jpeg)
    // Non-inventory menus only have one column and need
    // *really* big screens to cover more than 52 lines.
    if (pagesize > 52)
        return (52);
    return (pagesize);
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
#if 0
    // Not needed? (jpeg)
    m_dirty = true;
#endif
}

TitleRegion::TitleRegion(int width, int height) :
    m_buf(&m_img, GL_QUADS)
{
    sx = sy = 0;
    dx = dy = 1;

    if (!m_img.load_texture("title.png", GenericTexture::MIPMAP_NONE))
        return;

    // Center
    wx = width;
    wy = height;
    ox = (wx - m_img.orig_width()) / 2;
    oy = (wy - m_img.orig_height()) / 2;

    {
        PTVert &v = m_buf.get_next();
        v.pos_x = 0;
        v.pos_y = 0;
        v.tex_x = 0;
        v.tex_y = 0;
    }
    {
        PTVert &v = m_buf.get_next();
        v.pos_x = 0;
        v.pos_y = m_img.height();
        v.tex_x = 0;
        v.tex_y = 1;
    }
    {
        PTVert &v = m_buf.get_next();
        v.pos_x = m_img.width();
        v.pos_y = m_img.height();
        v.tex_x = 1;
        v.tex_y = 1;
    }
    {
        PTVert &v = m_buf.get_next();
        v.pos_x = m_img.width();
        v.pos_y = 0;
        v.tex_x = 1;
        v.tex_y = 0;
    }
}

void TitleRegion::render()
{
#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering TitleRegion\n");
#endif
    set_transform();
    m_buf.draw();
}

void TitleRegion::run()
{
    mouse_control mc(MOUSE_MODE_MORE);
    getch();
}

DollEditRegion::DollEditRegion(ImageManager *im, FTFont *font) :
    m_font_buf(font)
{
    sx = sy = 0;
    dx = dy = 32;
    mx = my = 1;

    m_font = font;

    m_doll_idx = 0;
    m_cat_idx = TILEP_PART_BASE;
    m_copy_valid = false;

    m_tile_buf.set_tex(&im->m_textures[TEX_PLAYER]);
    m_cur_buf.set_tex(&im->m_textures[TEX_PLAYER]);
}

void DollEditRegion::clear()
{
    m_shape_buf.clear();
    m_tile_buf.clear();
    m_cur_buf.clear();
    m_font_buf.clear();
}

// FIXME: Very hacky!
// Returns the starting tile for the next species in the tiles list, or the
// shadow tile if it's the last species.
static int _get_next_species_tile()
{
    int sp = you.species;
    if (player_genus(GENPC_DRACONIAN) && you.experience_level < 7)
        sp = SP_BASE_DRACONIAN;

    switch (sp)
    {
    case SP_HUMAN:
        return TILEP_BASE_ELF;
    case SP_HIGH_ELF:
    case SP_SLUDGE_ELF:
        return TILEP_BASE_DEEP_ELF;
    case SP_DEEP_ELF:
        return TILEP_BASE_DWARF;
    case SP_MOUNTAIN_DWARF:
    case SP_HALFLING:
    case SP_HILL_ORC:
    case SP_KOBOLD:
    case SP_MUMMY:
    case SP_NAGA:
    case SP_OGRE:
        return tilep_species_to_base_tile(you.species + 1);
    case SP_TROLL:
        return TILEP_BASE_DRACONIAN;
    case SP_BASE_DRACONIAN:
        return TILEP_BASE_DRACONIAN_BLACK;
    case SP_BLACK_DRACONIAN:
        return TILEP_BASE_DRACONIAN_GOLD;
    case SP_YELLOW_DRACONIAN:
        return TILEP_BASE_DRACONIAN_GREY;
    case SP_GREY_DRACONIAN:
        return TILEP_BASE_DRACONIAN_GREEN;
    case SP_GREEN_DRACONIAN:
        return TILEP_BASE_DRACONIAN_MOTTLED;
    case SP_MOTTLED_DRACONIAN:
        return TILEP_BASE_DRACONIAN_PALE;
    case SP_PALE_DRACONIAN:
        return TILEP_BASE_DRACONIAN_PURPLE;
    case SP_PURPLE_DRACONIAN:
        return TILEP_BASE_DRACONIAN_RED;
    case SP_RED_DRACONIAN:
        return TILEP_BASE_DRACONIAN_WHITE;
    case SP_WHITE_DRACONIAN:
        return TILEP_BASE_CENTAUR;
    case SP_CENTAUR:
    case SP_DEMIGOD:
    case SP_SPRIGGAN:
    case SP_MINOTAUR:
    case SP_DEMONSPAWN:
    case SP_GHOUL:
    case SP_KENKU:
    case SP_MERFOLK:
    case SP_VAMPIRE:
        return tilep_species_to_base_tile(you.species + 1);
    case SP_DEEP_DWARF:
        return TILEP_SHADOW_SHADOW;
    default:
        return TILEP_BASE_HUMAN;
    }
}

static int _get_next_part(int cat, int part, int inc)
{
    // Can't increment or decrement on show equip.
    if (part == TILEP_SHOW_EQUIP)
        return part;

    // Increment max_part by 1 to include the special value of "none".
    // (Except for the base, for which "none" is disallowed.)
    int max_part = tile_player_part_count[cat] + 1;
    int offset   = tile_player_part_start[cat];

    if (cat == TILEP_PART_BASE)
    {
        offset   = tilep_species_to_base_tile(you.species, you.experience_level);
        max_part = _get_next_species_tile() - offset;
    }

    ASSERT(inc > -max_part);

    // Translate the "none" value into something we can do modulo math with.
    if (part == 0)
    {
        part = offset;
        inc--;
    }

    // Valid part numbers are in the range [offset, offset + max_part - 1].
    int ret = (part + max_part + inc - offset) % (max_part);

    if (cat != TILEP_PART_BASE && ret == max_part - 1)
    {
        // "none" value.
        return 0;
    }
    else
    {
        // Otherwise, valid part number.
        return ret + offset;
    }
}

void DollEditRegion::render()
{
#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering DollEditRegion\n");
#endif
    VColour grey(128, 128, 128, 255);

    m_cur_buf.clear();
    m_tile_buf.clear();
    m_shape_buf.clear();
    m_font_buf.clear();

    // Max items to show at once.
    const int max_show = 9;

    // Layout options (units are in 32x32 squares)
    const int left_gutter = 2;
    const int item_line = 2;
    const int edit_doll_line = 5;
    const int doll_line = 8;
    const int info_offset =
        left_gutter + std::max(max_show, (int)NUM_MAX_DOLLS) + 1;

    const int center_x = left_gutter + max_show / 2;

    // Pack current doll separately so it can be drawn repeatedly.
    {
        dolls_data temp = m_dolls[m_doll_idx];
        _fill_doll_equipment(temp);
        pack_doll_buf(m_cur_buf, temp, 0, 0);
    }

    // Draw set of dolls.
    for (int i = 0; i < NUM_MAX_DOLLS; i++)
    {
        int x = left_gutter + i;
        int y = doll_line;

        if (m_mode == TILEP_MODE_LOADING && m_doll_idx == i)
            m_tile_buf.add(TILEP_CURSOR, x, y);

        dolls_data temp = m_dolls[i];
        _fill_doll_equipment(temp);
        pack_doll_buf(m_tile_buf, temp, x, y);

        m_shape_buf.add(x, y, x + 1, y + 1, grey);
    }

    // Draw current category of parts.
    int max_part = tile_player_part_count[m_cat_idx];
    if (m_cat_idx == TILEP_PART_BASE)
        max_part = _get_next_species_tile() - tilep_species_to_base_tile() - 1;

    int show = std::min(max_show, max_part);
    int half_show = show / 2;
    for (int i = -half_show; i <= show - half_show; i++)
    {
        int x = center_x + i;
        int y = item_line;

        if (i == 0)
            m_tile_buf.add(TILEP_CURSOR, x, y);

        int part = _get_next_part(m_cat_idx, m_part_idx, i);
        ASSERT(part != TILEP_SHOW_EQUIP);
        if (part)
            m_tile_buf.add(part, x, y);

        m_shape_buf.add(x, y, x + 1, y + 1, grey);
    }

    m_shape_buf.add(left_gutter, edit_doll_line, left_gutter + 2, edit_doll_line + 2, grey);
    m_shape_buf.add(left_gutter + 3, edit_doll_line, left_gutter + 4, edit_doll_line + 1, grey);
    m_shape_buf.add(left_gutter + 5, edit_doll_line, left_gutter + 6, edit_doll_line + 1, grey);
    m_shape_buf.add(left_gutter + 7, edit_doll_line, left_gutter + 8, edit_doll_line + 1, grey);
    {
        // Describe the three middle tiles.
        float tile_name_x = (left_gutter + 2.7) * 32.0f;
        float tile_name_y = (edit_doll_line + 1) * 32.0f;
        m_font_buf.add("Custom", VColour::white, tile_name_x, tile_name_y);
        tile_name_x = (left_gutter + 4.7) * 32.0f;
        tile_name_y = (edit_doll_line + 1) * 32.0f;
        m_font_buf.add("Default", VColour::white, tile_name_x, tile_name_y);
        tile_name_x = (left_gutter + 7) * 32.0f;
        tile_name_y = (edit_doll_line + 1) * 32.0f;
        m_font_buf.add("Equip", VColour::white, tile_name_x, tile_name_y);
    }


    set_transform();
    m_shape_buf.draw();
    m_tile_buf.draw();

    glLoadIdentity();
    glTranslatef(32 * left_gutter, 32 * edit_doll_line, 0);
    glScalef(64, 64, 1);
    m_cur_buf.draw();

    {
        dolls_data temp;
        temp = m_job_default;
        _fill_doll_equipment(temp);
        pack_doll_buf(m_cur_buf, temp, 2, 0);

        for (unsigned int i = 0; i < TILEP_PART_MAX; ++i)
            temp.parts[i] = TILEP_SHOW_EQUIP;
        _fill_doll_equipment(temp);
        pack_doll_buf(m_cur_buf, temp, 4, 0);

        if (m_mode == TILEP_MODE_LOADING)
            m_cur_buf.add(TILEP_CURSOR, 0, 0);
        else if (m_mode == TILEP_MODE_DEFAULT)
            m_cur_buf.add(TILEP_CURSOR, 2, 0);
        else if (m_mode == TILEP_MODE_EQUIP)
            m_cur_buf.add(TILEP_CURSOR, 4, 0);
    }
    glLoadIdentity();
    glTranslatef(32 * (left_gutter + 3), 32 * edit_doll_line, 0);
    glScalef(32, 32, 1);
    m_cur_buf.draw();

    // Add text.
    const char *part_name = "(none)";
    if (m_part_idx == TILEP_SHOW_EQUIP)
        part_name = "(show equip)";
    else if (m_part_idx)
        part_name = tile_player_name(m_part_idx);

    glLoadIdentity();
    glTranslatef(0, 0, 0);
    glScalef(1, 1, 1);

    std::string item_str = part_name;
    float item_name_x = left_gutter * 32.0f;
    float item_name_y = (item_line + 1) * 32.0f;
    m_font_buf.add(item_str, VColour::white, item_name_x, item_name_y);

    std::string doll_name;
    doll_name = make_stringf("Doll index %d / %d", m_doll_idx, NUM_MAX_DOLLS - 1);
    float doll_name_x = left_gutter * 32.0f;
    float doll_name_y = (doll_line + 1) * 32.0f;
    m_font_buf.add(doll_name, VColour::white, doll_name_x, doll_name_y);

    const char *mode_name[TILEP_MODE_MAX] =
    {
        "Current Equipment",
        "Custom Doll",
        "Job Defaults"
    };
    doll_name = make_stringf("Doll Mode: %s", mode_name[m_mode]);
    doll_name_y += m_font->char_height() * 2.0f;
    m_font_buf.add(doll_name, VColour::white, doll_name_x, doll_name_y);

    // FIXME - this should be generated in rltiles
    const char *cat_name[TILEP_PART_MAX] =
    {
        "Base",
        "Shadow",
        "Halo",
        "Ench",
        "Cloak",
        "Boots",
        "Legs",
        "Body",
        "Gloves",
        "LHand",
        "RHand",
        "Hair",
        "Beard",
        "Helm",
        "DrcWing",
        "DrcHead"
    };

    // Add current doll information:
    std::string info_str;
    float info_x = info_offset * 32.0f;
    float info_y = 0.0f + m_font->char_height();

    for (int i = 0 ; i < TILEP_PART_MAX; i++)
    {
        int part = m_dolls[m_doll_idx].parts[i];
        int disp = part;
        if (disp)
            disp = disp - tile_player_part_start[i] + 1;
        int maxp = tile_player_part_count[i];

        const char *sel = (m_cat_idx == i) ? "->" : "  ";

        if (part == TILEP_SHOW_EQUIP)
            info_str = make_stringf("%2s%9s: (show equip)", sel, cat_name[i]);
        else if (!part)
            info_str = make_stringf("%2s%9s: (none)", sel, cat_name[i]);
        else
            info_str = make_stringf("%2s%9s: %3d/%3d", sel, cat_name[i], disp, maxp);
        m_font_buf.add(info_str, VColour::white, info_x, info_y);
        info_y += m_font->char_height();
    }

    // List the most important commands. (Hopefully the rest will be
    // self-explanatory.)
    {
        const int height = m_font->char_height();
        const float start_y = doll_name_y + height * 3;
        m_font_buf.add("Change parts       left/right              Confirm choice      Enter", VColour::white, 0.0f, start_y);
        m_font_buf.add("Change category    up/down                 Copy doll           Ctrl-C", VColour::white, 0.0f, start_y + height * 1);
        m_font_buf.add("Change doll        0-9, Shift + arrows     Paste copied doll   Ctrl-V", VColour::white, 0.0f, start_y + height * 2);
        m_font_buf.add("Change doll mode   m                       Randomise doll      Ctrl-R", VColour::white, 0.0f, start_y + height * 3);
        m_font_buf.add("Quit menu          q, Escape, Ctrl-S       Toggle equipment    *", VColour::white, 0.0f, start_y + height * 4);
    }

    m_font_buf.draw();
}

int DollEditRegion::handle_mouse(MouseEvent &event)
{
    return 0;
}

void DollEditRegion::run()
{
    // Initialise equipment setting.
    dolls_data equip_doll;
    for (unsigned int i = 0; i < TILEP_PART_MAX; ++i)
         equip_doll.parts[i] = TILEP_SHOW_EQUIP;

    // Initialise job default.
    m_job_default = equip_doll;
    tilep_race_default(you.species, gender,
                       you.experience_level, m_job_default.parts);
    tilep_job_default(you.char_class, gender, m_job_default.parts);

    // Read predefined dolls from file.
    for (unsigned int i = 0; i < NUM_MAX_DOLLS; ++i)
         m_dolls[i] = equip_doll;

    m_mode = TILEP_MODE_LOADING;
    m_doll_idx = -1;

    if (!_load_doll_data("dolls.txt", m_dolls, NUM_MAX_DOLLS, &m_mode, &m_doll_idx))
    {
        m_doll_idx = 0;
    }

    bool update_part_idx = true;

    command_type cmd;
    do
    {
        if (update_part_idx)
        {
            m_part_idx = m_dolls[m_doll_idx].parts[m_cat_idx];
            if (m_part_idx == TILEP_SHOW_EQUIP)
                m_part_idx = 0;
            update_part_idx = false;
        }

        int key = getchm(KMC_DOLL);
        cmd = key_to_command(key, KMC_DOLL);

        switch (cmd)
        {
        case CMD_DOLL_RANDOMIZE:
            _create_random_doll(m_dolls[m_doll_idx]);
            break;
        case CMD_DOLL_SELECT_NEXT_DOLL:
            m_doll_idx = (m_doll_idx + 1) % NUM_MAX_DOLLS;
            update_part_idx = true;
            break;
        case CMD_DOLL_SELECT_PREV_DOLL:
            m_doll_idx = (m_doll_idx + NUM_MAX_DOLLS - 1) % NUM_MAX_DOLLS;
            update_part_idx = true;
            break;
        case CMD_DOLL_SELECT_NEXT_PART:
            m_cat_idx = (m_cat_idx + 1) % TILEP_PART_MAX;
            update_part_idx = true;
            break;
        case CMD_DOLL_SELECT_PREV_PART:
            m_cat_idx = (m_cat_idx + TILEP_PART_MAX - 1) % TILEP_PART_MAX;
            update_part_idx = true;
            break;
        case CMD_DOLL_CHANGE_PART_NEXT:
            m_part_idx = _get_next_part(m_cat_idx, m_part_idx, 1);
            if (m_dolls[m_doll_idx].parts[m_cat_idx] != TILEP_SHOW_EQUIP)
                m_dolls[m_doll_idx].parts[m_cat_idx] = m_part_idx;
            break;
        case CMD_DOLL_CHANGE_PART_PREV:
            m_part_idx = _get_next_part(m_cat_idx, m_part_idx, -1);
            if (m_dolls[m_doll_idx].parts[m_cat_idx] != TILEP_SHOW_EQUIP)
                m_dolls[m_doll_idx].parts[m_cat_idx] = m_part_idx;
            break;
        case CMD_DOLL_CONFIRM_CHOICE:
            m_dolls[m_doll_idx].parts[m_cat_idx] = m_part_idx;
            if (m_mode != TILEP_MODE_LOADING)
                m_mode = TILEP_MODE_LOADING;
            break;
        case CMD_DOLL_COPY:
            m_doll_copy  = m_dolls[m_doll_idx];
            m_copy_valid = true;
            break;
        case CMD_DOLL_PASTE:
            if (m_copy_valid)
                m_dolls[m_doll_idx] = m_doll_copy;
            break;
        case CMD_DOLL_TAKE_OFF:
            m_part_idx = 0;
            m_dolls[m_doll_idx].parts[m_cat_idx] = 0;
            break;
        case CMD_DOLL_TAKE_OFF_ALL:
            for (int i = 0; i < TILEP_PART_MAX; i++)
            {
                switch (i)
                {
                case TILEP_PART_BASE:
                case TILEP_PART_SHADOW:
                case TILEP_PART_HALO:
                case TILEP_PART_ENCH:
                case TILEP_PART_DRCWING:
                case TILEP_PART_DRCHEAD:
                    break;
                default:
                    m_dolls[m_doll_idx].parts[i] = 0;
                };
            }
            break;
        case CMD_DOLL_TOGGLE_EQUIP:
            if (m_dolls[m_doll_idx].parts[m_cat_idx] == TILEP_SHOW_EQUIP)
                m_dolls[m_doll_idx].parts[m_cat_idx] = m_part_idx;
            else
                m_dolls[m_doll_idx].parts[m_cat_idx] = TILEP_SHOW_EQUIP;
            break;
        case CMD_DOLL_TOGGLE_EQUIP_ALL:
            for (int i = 0; i < TILEP_PART_MAX; i++)
                m_dolls[m_doll_idx].parts[i] = TILEP_SHOW_EQUIP;
            break;
        case CMD_DOLL_CLASS_DEFAULT:
            m_dolls[m_doll_idx] = m_job_default;
            break;
        case CMD_DOLL_CHANGE_MODE:
            m_mode = (tile_doll_mode)(((int)m_mode + 1) % TILEP_MODE_MAX);
        default:
            if (key == '0')
                m_doll_idx = 0;
            else if (key >= '1' && key <= '9')
                m_doll_idx = key - '1' + 1;
            ASSERT(m_doll_idx < NUM_MAX_DOLLS);
            break;
        }
    }
    while (cmd != CMD_DOLL_QUIT);

    _save_doll_data(m_mode, m_doll_idx, &m_dolls[0]);

    // Update player with the current doll.
    switch (m_mode)
    {
    case TILEP_MODE_LOADING:
        player_doll = m_dolls[m_doll_idx];
        break;
    case TILEP_MODE_DEFAULT:
        player_doll = m_job_default;
        break;
    default:
    case TILEP_MODE_EQUIP:
        player_doll = equip_doll;
    }
}

ImageManager::ImageManager()
{
}

ImageManager::~ImageManager()
{
    unload_textures();
}

bool ImageManager::load_textures(bool need_mips)
{
    GenericTexture::MipMapOptions mip = need_mips ?
        GenericTexture::MIPMAP_CREATE : GenericTexture::MIPMAP_NONE;
    if (!m_textures[TEX_DUNGEON].load_texture("dngn.png", mip))
        return (false);

    if (!m_textures[TEX_PLAYER].load_texture("player.png", mip))
        return (false);

    if (!m_textures[TEX_GUI].load_texture("gui.png", mip))
        return (false);

    m_textures[TEX_DUNGEON].set_info(TILE_DNGN_MAX, &tile_dngn_info);
    m_textures[TEX_PLAYER].set_info(TILEP_PLAYER_MAX, &tile_player_info);
    m_textures[TEX_GUI].set_info(TILEG_GUI_MAX, &tile_gui_info);

    return (true);
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
    int src_height  = inf.ey - inf.sy;
    int src_width   = inf.ex - inf.sx;

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
    const tile_info &over  = tile_main_info(idx_over);

    if (over.width != under.width || over.height != under.height)
        return (false);

    if (over.ex - over.sx != over.width || over.ey - over.sy != over.height)
        return (false);

    if (over.offset_x != 0 || over.offset_y != 0)
        return (false);

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

    return (true);
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

    return (true);
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
        m_textures[i].unload_texture();
}

#endif
