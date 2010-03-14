/*
 *  File:       tilereg.cc
 *  Summary:    Region system implementations
 *
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#include "AppHdr.h"

#ifdef USE_TILE

#include "cio.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "debug.h"
#include "describe.h"
#include "directn.h"
#include "env.h"
#include "files.h"
#include "food.h"
#include "invent.h"
#include "item_use.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "jobs.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "menu.h"
#include "newgame.h"
#include "map_knowledge.h"
#include "mon-util.h"
#include "options.h"
#include "player.h"
#include "religion.h"
#include "species.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stash.h"
#include "stuff.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "viewgeom.h"

#include "tilereg.h"
#include "tiles.h"
#include "tilefont.h"
#include "tilesdl.h"
#include "tilemcache.h"
#include "tiledef-dngn.h"
#include "tiledef-gui.h"
#include "tiledef-main.h"
#include "tiledef-player.h"

#include <sys/stat.h>

#include "glwrapper.h"

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

void Region::place(int _sx, int _sy)
{
    sx = _sx;
    sy = _sy;

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
    GLW_3VF trans(sx + ox, sy + oy, 0);
    GLW_3VF scale(dx, dy, 1);
    
    glmanager->set_transform(&trans, &scale);
}

TileRegion::TileRegion(ImageManager* im, FontWrapper *tag_font, int tile_x, int tile_y)
{
    ASSERT(im);
    ASSERT(tag_font);

    m_image = im;
    dx = tile_x;
    dy = tile_y;
    m_tag_font = tag_font;

    // To quite Valgrind
    m_dirty = true;
}

TileRegion::~TileRegion()
{
}

DungeonRegion::DungeonRegion(ImageManager* im, FontWrapper *tag_font,
                             int tile_x, int tile_y) :
    TileRegion(im, tag_font, tile_x, tile_y),
    m_cx_to_gx(0),
    m_cy_to_gy(0),
    m_buf_dngn(&im->m_textures[TEX_DUNGEON]),
    m_buf_doll(&im->m_textures[TEX_PLAYER], TILEP_MASK_SUBMERGED, 18, 16,
               Options.tile_better_transparency),
    m_buf_main_trans(&im->m_textures[TEX_DEFAULT], TILE_MASK_SUBMERGED, 18, 16,
                     Options.tile_better_transparency),
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

enum wave_type
{
    WV_NONE = 0,
    WV_SHALLOW,
    WV_DEEP
};

wave_type _get_wave_type(bool shallow)
{
    return (shallow ? WV_SHALLOW : WV_DEEP);
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
        else if (bg & TILE_FLAG_MOLD)
        {
            tile_flavour &flv = env.tile_flv[x + m_cx_to_gx][y + m_cy_to_gy];
            int offset = flv.special % tile_dngn_count(TILE_MOLD);
            m_buf_dngn.add(TILE_MOLD + offset, x, y);
        }

        if (player_in_branch(BRANCH_SHOALS))
        {
            // Add wave tiles on floor adjacent to shallow water.
            const coord_def pos = coord_def(x + m_cx_to_gx, y + m_cy_to_gy);
            const dungeon_feature_type feat = env.map_knowledge(pos).feat();
            const bool feat_has_ink
                            = (cloud_type_at(coord_def(pos)) == CLOUD_INK);

            if (feat == DNGN_DEEP_WATER && feat_has_ink)
            {
                m_buf_dngn.add(TILE_WAVE_INK_FULL, x, y);
            }
            else if (feat == DNGN_FLOOR || feat == DNGN_UNDISCOVERED_TRAP
                     || feat == DNGN_SHALLOW_WATER
                     || feat == DNGN_DEEP_WATER)
            {
                const bool ink_only = (feat == DNGN_DEEP_WATER);

                wave_type north = WV_NONE, south = WV_NONE,
                          east = WV_NONE, west = WV_NONE,
                          ne = WV_NONE, nw = WV_NONE,
                          se = WV_NONE, sw = WV_NONE;

                bool inkn = false, inks = false, inke = false, inkw = false,
                     inkne = false, inknw = false, inkse = false, inksw = false;

                for (radius_iterator ri(pos, 1, true, false, true); ri; ++ri)
                {
                    if (!is_terrain_seen(*ri) && !is_terrain_mapped(*ri))
                        continue;

                    const bool ink
                        = (cloud_type_at(coord_def(*ri)) == CLOUD_INK);

                    bool shallow = false;
                    if (env.map_knowledge(*ri).feat() == DNGN_SHALLOW_WATER)
                    {
                        // Adjacent shallow water is only interesting for
                        // floor cells.
                        if (!ink && feat == DNGN_SHALLOW_WATER)
                            continue;

                        shallow = true;
                    }
                    else if (env.map_knowledge(*ri).feat() != DNGN_DEEP_WATER)
                        continue;

                    if (!ink_only)
                    {
                        if (ri->x == pos.x) // orthogonals
                        {
                            if (ri->y < pos.y)
                                north = _get_wave_type(shallow);
                            else
                                south = _get_wave_type(shallow);
                        }
                        else if (ri->y == pos.y)
                        {
                            if (ri->x < pos.x)
                                west = _get_wave_type(shallow);
                            else
                                east = _get_wave_type(shallow);
                        }
                        else // diagonals
                        {
                            if (ri->x < pos.x)
                            {
                                if (ri->y < pos.y)
                                    nw = _get_wave_type(shallow);
                                else
                                    sw = _get_wave_type(shallow);
                            }
                            else
                            {
                                if (ri->y < pos.y)
                                    ne = _get_wave_type(shallow);
                                else
                                    se = _get_wave_type(shallow);
                            }
                        }
                    }
                    if (!feat_has_ink && ink)
                    {
                        if (ri->x == pos.x) // orthogonals
                        {
                            if (ri->y < pos.y)
                                inkn = true;
                            else
                                inks = true;
                        }
                        else if (ri->y == pos.y)
                        {
                            if (ri->x < pos.x)
                                inkw = true;
                            else
                                inke = true;
                        }
                        else // diagonals
                        {
                            if (ri->x < pos.x)
                            {
                                if (ri->y < pos.y)
                                    inknw = true;
                                else
                                    inksw = true;
                            }
                            else
                            {
                                if (ri->y < pos.y)
                                    inkne = true;
                                else
                                    inkse = true;
                            }
                        }
                    }
                }

                if (!ink_only)
                {
                    // First check for shallow water.
                    if (north == WV_SHALLOW)
                        m_buf_dngn.add(TILE_WAVE_N, x, y);
                    if (south == WV_SHALLOW)
                        m_buf_dngn.add(TILE_WAVE_S, x, y);
                    if (east == WV_SHALLOW)
                        m_buf_dngn.add(TILE_WAVE_E, x, y);
                    if (west == WV_SHALLOW)
                        m_buf_dngn.add(TILE_WAVE_W, x, y);

                    // Then check for deep water, overwriting shallow
                    // corner waves, if necessary.
                    if (north == WV_DEEP)
                        m_buf_dngn.add(TILE_WAVE_DEEP_N, x, y);
                    if (south == WV_DEEP)
                        m_buf_dngn.add(TILE_WAVE_DEEP_S, x, y);
                    if (east == WV_DEEP)
                        m_buf_dngn.add(TILE_WAVE_DEEP_E, x, y);
                    if (west == WV_DEEP)
                        m_buf_dngn.add(TILE_WAVE_DEEP_W, x, y);

                    if (ne == WV_SHALLOW && !north && !east)
                        m_buf_dngn.add(TILE_WAVE_CORNER_NE, x, y);
                    else if (ne == WV_DEEP && north != WV_DEEP && east != WV_DEEP)
                        m_buf_dngn.add(TILE_WAVE_DEEP_CORNER_NE, x, y);
                    if (nw == WV_SHALLOW && !north && !west)
                        m_buf_dngn.add(TILE_WAVE_CORNER_NW, x, y);
                    else if (nw == WV_DEEP && north != WV_DEEP && west != WV_DEEP)
                        m_buf_dngn.add(TILE_WAVE_DEEP_CORNER_NW, x, y);
                    if (se == WV_SHALLOW && !south && !east)
                        m_buf_dngn.add(TILE_WAVE_CORNER_SE, x, y);
                    else if (se == WV_DEEP && south != WV_DEEP && east != WV_DEEP)
                        m_buf_dngn.add(TILE_WAVE_DEEP_CORNER_SE, x, y);
                    if (sw == WV_SHALLOW && !south && !west)
                        m_buf_dngn.add(TILE_WAVE_CORNER_SW, x, y);
                    else if (sw == WV_DEEP && south != WV_DEEP && west != WV_DEEP)
                        m_buf_dngn.add(TILE_WAVE_DEEP_CORNER_SW, x, y);
                }

                // Overlay with ink sheen, if necessary.
                if (feat_has_ink)
                    m_buf_dngn.add(TILE_WAVE_INK_FULL, x, y);
                else
                {
                    if (inkn)
                        m_buf_dngn.add(TILE_WAVE_INK_N, x, y);
                    if (inks)
                        m_buf_dngn.add(TILE_WAVE_INK_S, x, y);
                    if (inke)
                        m_buf_dngn.add(TILE_WAVE_INK_E, x, y);
                    if (inkw)
                        m_buf_dngn.add(TILE_WAVE_INK_W, x, y);
                    if (inkne || inkn || inke)
                        m_buf_dngn.add(TILE_WAVE_INK_CORNER_NE, x, y);
                    if (inknw || inkn || inkw)
                        m_buf_dngn.add(TILE_WAVE_INK_CORNER_NW, x, y);
                    if (inkse || inks || inke)
                        m_buf_dngn.add(TILE_WAVE_INK_CORNER_SE, x, y);
                    if (inksw || inks || inkw)
                        m_buf_dngn.add(TILE_WAVE_INK_CORNER_SW, x, y);
                }
            }
        }

        if (bg & TILE_FLAG_HALO)
            m_buf_dngn.add(TILE_HALO, x, y);

        if (!(bg & TILE_FLAG_UNSEEN))
        {
            if (bg & TILE_FLAG_SANCTUARY)
                m_buf_dngn.add(TILE_SANCTUARY, x, y);
            if (bg & TILE_FLAG_SILENCED)
                m_buf_dngn.add(TILE_SILENCED, x, y);

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
        if (fscanf(fp, "%1023s", fbuf) != EOF)
        {
            if (strcmp(fbuf, "MODE=DEFAULT") == 0)
                *mode = TILEP_MODE_DEFAULT;
            else if (strcmp(fbuf, "MODE=EQUIP") == 0)
                *mode = TILEP_MODE_EQUIP; // Nothing else to be done.
        }
        // Read current doll from file.
        if (fscanf(fp, "%1023s", fbuf) != EOF)
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
            while (fscanf(fp, "%1023s", fbuf) != EOF)
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
        }
        else // Load up to max dolls from file.
        {
            for (int count = 0; count < max && fscanf(fp, "%1023s", fbuf) != EOF; )
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
        tilep_race_default(you.species, gender, you.experience_level,
                           player_doll.parts);
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

    // The following are only rolled with 50% chance.
    if (coinflip())
        _fill_doll_part(rdoll, TILEP_PART_CLOAK);
    if (coinflip())
        _fill_doll_part(rdoll, TILEP_PART_ARM);
    if (coinflip())
        _fill_doll_part(rdoll, TILEP_PART_HAND2);
    if (coinflip())
        _fill_doll_part(rdoll, TILEP_PART_HELM);

    // Only male dolls get a chance at a beard.
    if (get_gender_from_tile(rdoll.parts) == TILEP_GENDER_MALE && one_chance_in(4))
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
        else if (player_mutation_level(MUT_HOOVES) >= 3)
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
        const bool halo = you.haloed();
        result.parts[TILEP_PART_HALO] = halo ? TILEP_HALO_TSO : 0;
    }
    // Enchantments.
    if (result.parts[TILEP_PART_ENCH] == TILEP_SHOW_EQUIP)
    {
        result.parts[TILEP_PART_ENCH] =
            (you.duration[DUR_LIQUID_FLAMES] ? TILEP_ENCH_STICKY_FLAME : 0);
    }
    // Draconian head/wings.
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
    // Shadow.
    if (result.parts[TILEP_PART_SHADOW] == TILEP_SHOW_EQUIP)
        result.parts[TILEP_PART_SHADOW] = TILEP_SHADOW_SHADOW;

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

void DungeonRegion::pack_player(int x, int y, bool submerged)
{
    dolls_data result = player_doll;
    _fill_doll_equipment(result);
    pack_doll(result, x, y, submerged, false);
}

void pack_doll_buf(SubmergedTileBuffer& buf, const dolls_data &doll, int x, int y, bool submerged, bool ghost)
{
    // Ordered from back to front.
    int p_order[TILEP_PART_MAX] =
    {
        // background
        TILEP_PART_SHADOW,
        TILEP_PART_HALO,
        TILEP_PART_ENCH,
        TILEP_PART_DRCWING,
        TILEP_PART_CLOAK,
        // player
        TILEP_PART_BASE,
        TILEP_PART_BOOTS,
        TILEP_PART_LEG,
        TILEP_PART_BODY,
        TILEP_PART_ARM,
        TILEP_PART_HAIR,
        TILEP_PART_BEARD,
        TILEP_PART_HELM,
        TILEP_PART_HAND1,
        TILEP_PART_HAND2,
        TILEP_PART_DRCHEAD
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
    const bool is_naga = is_player_tile(doll.parts[TILEP_PART_BASE],
                                        TILEP_BASE_NAGA);

    if (doll.parts[TILEP_PART_BOOTS] >= TILEP_BOOTS_NAGA_BARDING
        && doll.parts[TILEP_PART_BOOTS] <= TILEP_BOOTS_NAGA_BARDING_RED)
    {
        flags[TILEP_PART_BOOTS] = is_naga ? TILEP_FLAG_NORMAL : TILEP_FLAG_HIDE;
    }

    const bool is_cent = is_player_tile(doll.parts[TILEP_PART_BASE],
                                        TILEP_BASE_CENTAUR);

    if (doll.parts[TILEP_PART_BOOTS] >= TILEP_BOOTS_CENTAUR_BARDING
        && doll.parts[TILEP_PART_BOOTS] <= TILEP_BOOTS_CENTAUR_BARDING_RED)
    {
        flags[TILEP_PART_BOOTS] = is_cent ? TILEP_FLAG_NORMAL : TILEP_FLAG_HIDE;
    }

    // A higher index here means that the part should be drawn on top.
    // This is drawn in reverse order because this could be a ghost
    // or being drawn in water, in which case we want the top-most part
    // to blend with the background underneath and not with the parts
    // underneath.  Parts drawn afterwards will not obscure parts drawn
    // previously, because "i" is passed as the depth below.
    for (int i = TILEP_PART_MAX - 1; i >= 0; --i)
    {
        int p = p_order[i];

        if (!doll.parts[p] || flags[p] == TILEP_FLAG_HIDE)
            continue;

        if (p == TILEP_PART_SHADOW && (submerged || ghost))
            continue;

        int ymax = TILE_Y;

        if (flags[p] == TILEP_FLAG_CUT_CENTAUR
            || flags[p] == TILEP_FLAG_CUT_NAGA)
        {
            ymax = 18;
        }

        buf.add(doll.parts[p], x, y, i, submerged, ghost, 0, 0, ymax);
    }
}

void DungeonRegion::pack_doll(const dolls_data &doll, int x, int y, bool submerged, bool ghost)
{
    pack_doll_buf(m_buf_doll, doll, x, y, submerged, ghost);
}


void DungeonRegion::pack_mcache(mcache_entry *entry, int x, int y, bool submerged)
{
    ASSERT(entry);

    bool trans = entry->transparent();

    const dolls_data *doll = entry->doll();
    if (doll)
        pack_doll(*doll, x, y, submerged, trans);

    tile_draw_info dinfo[3];
    unsigned int draw_info_count = entry->info(&dinfo[0]);
    ASSERT(draw_info_count <= sizeof(dinfo) / (sizeof(dinfo[0])));

    for (unsigned int i = 0; i < draw_info_count; i++)
    {
        m_buf_doll.add(dinfo[i].idx, x, y, 0, submerged, trans,
                       dinfo[i].ofs_x, dinfo[i].ofs_y);
    }
}

void DungeonRegion::pack_foreground(unsigned int bg, unsigned int fg, int x, int y)
{
    unsigned int fg_idx = fg & TILE_FLAG_MASK;

    bool in_water = (bg & TILE_FLAG_WATER) && !(fg & TILE_FLAG_FLYING);

    if (fg_idx && fg_idx <= TILE_MAIN_MAX)
    {
        if (in_water)
            m_buf_main_trans.add(fg_idx, x, y, 0, true, false);
        else
            m_buf_main.add(fg_idx, x, y);
    }

    if (fg & TILE_FLAG_NET)
        m_buf_main.add(TILE_TRAP_NET, x, y);

    if (fg & TILE_FLAG_S_UNDER)
        m_buf_main.add(TILE_SOMETHING_UNDER, x, y);

    int status_shift = 0;
    if (fg & TILE_FLAG_BERSERK)
    {
        m_buf_main.add(TILE_BERSERK, x, y);
        status_shift += 10;
    }

    // Pet mark
    if (fg & TILE_FLAG_PET)
    {
        m_buf_main.add(TILE_HEART, x, y);
        status_shift += 10;
    }
    else if (fg & TILE_FLAG_GD_NEUTRAL)
    {
        m_buf_main.add(TILE_GOOD_NEUTRAL, x, y);
        status_shift += 8;
    }
    else if (fg & TILE_FLAG_NEUTRAL)
    {
        m_buf_main.add(TILE_NEUTRAL, x, y);
        status_shift += 8;
    }
    else if (fg & TILE_FLAG_STAB)
    {
        m_buf_main.add(TILE_STAB_BRAND, x, y);
        status_shift += 8;
    }
    else if (fg & TILE_FLAG_MAY_STAB)
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

    if (fg & TILE_FLAG_DEMON)
    {
        unsigned int demon_flag = fg & TILE_FLAG_DEMON;
        if (demon_flag == TILE_FLAG_DEMON_1)
            m_buf_main.add(TILE_DEMON_NUM1, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_2)
            m_buf_main.add(TILE_DEMON_NUM2, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_3)
            m_buf_main.add(TILE_DEMON_NUM3, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_4)
            m_buf_main.add(TILE_DEMON_NUM4, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_5)
            m_buf_main.add(TILE_DEMON_NUM5, x, y);
    }
}

void DungeonRegion::pack_cursor(cursor_type type, unsigned int tile)
{
    const coord_def &gc = m_cursor[type];
    if (gc == NO_CURSOR || !on_screen(gc))
        return;

    m_buf_main.add(tile, gc.x - m_cx_to_gx, gc.y - m_cy_to_gy);
}

static void _lichform_add_weapon(SubmergedTileBuffer &buf, int x, int y,
                                 bool in_water)
{
    const int item = you.equip[EQ_WEAPON];
    if (item == -1)
        return;

    const int wep = tilep_equ_weapon(you.inv[item]);
    if (!wep)
        return;

    buf.add(wep, x, y, 0, in_water, false, -1, 0);
}

void DungeonRegion::pack_buffers()
{
    m_buf_dngn.clear();
    m_buf_doll.clear();
    m_buf_main_trans.clear();
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

            bool in_water = (bg & TILE_FLAG_WATER) && !(fg & TILE_FLAG_FLYING);
            if (fg_idx >= TILEP_MCACHE_START)
            {
                mcache_entry *entry = mcache.get(fg_idx);
                if (entry)
                    pack_mcache(entry, x, y, in_water);
                else
                    m_buf_doll.add(TILEP_MONS_UNKNOWN, x, y, 0, in_water, false);
            }
            else if (fg_idx == TILEP_PLAYER)
            {
                pack_player(x, y, in_water);
            }
            else if (fg_idx >= TILE_MAIN_MAX)
            {
                m_buf_doll.add(fg_idx, x, y, 0, in_water, false);
                if (fg_idx == TILEP_TRAN_LICH)
                    _lichform_add_weapon(m_buf_doll, x, y, in_water);
            }

            pack_foreground(bg, fg, x, y);

            tile += 2;
        }

    pack_cursor(CURSOR_TUTORIAL, TILE_TUTORIAL_CURSOR);
    pack_cursor(CURSOR_MOUSE, you.see_cell(m_cursor[CURSOR_MOUSE]) ? TILE_CURSOR
                                                                   : TILE_CURSOR2);
    pack_cursor(CURSOR_MAP, TILE_CURSOR);

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
    m_buf_dngn.draw(NULL, NULL);
    m_buf_doll.draw();
    m_buf_main_trans.draw();
    m_buf_main.draw(NULL, NULL);

    draw_minibars();

    if (you.berserk())
    {
        ShapeBuffer buff;
        VColour red_film(130, 0, 0, 100);
        buff.add(0, 0, mx, my, red_film);
        buff.draw(NULL, NULL);
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

/**
 * Draws miniature health and magic points bars on top of the player tile.
 *
 * Drawing of either is governed by options tile_show_minihealthbar and
 * tile_show_minimagicbar. By default, both are on.
 *
 * Intended behaviour is to display both if either is not full. (Unless
 * the bar is toggled off by options.) --Eino & felirx
 */
void DungeonRegion::draw_minibars()
{
    if (Options.tile_show_minihealthbar && you.hp < you.hp_max
        || Options.tile_show_minimagicbar
           && you.magic_points < you.max_magic_points)
    {
        // Tiles are 32x32 pixels; 1/32 = 0.03125.
        // The bars are two pixels high each.
        const float bar_height = 0.0625;
        float healthbar_offset = 0.875;

        ShapeBuffer buff;

        if (!on_screen(you.pos()))
             return;

        // FIXME: to_screen_coords could be made into two versions: one
        // that gives coords by pixel (the current one), one that gives
        // them by grid.
        coord_def player_on_screen;
        to_screen_coords(you.pos(), player_on_screen);

        static const float tile_width  = wx / mx;
        static const float tile_height = wy / my;

        player_on_screen.x = player_on_screen.x / tile_width;
        player_on_screen.y = player_on_screen.y / tile_height;

        if (Options.tile_show_minimagicbar && you.max_magic_points > 0)
        {
            static const VColour magic(0, 0, 255, 255);      // blue
            static const VColour magic_spent(0, 0, 0, 255);  // black

            const float magic_divider = (float) you.magic_points
                                        / (float) you.max_magic_points;

            buff.add(player_on_screen.x,
                     player_on_screen.y + healthbar_offset + bar_height,
                     player_on_screen.x + magic_divider,
                     player_on_screen.y + 1,
                     magic);
            buff.add(player_on_screen.x + magic_divider,
                     player_on_screen.y + healthbar_offset + bar_height,
                     player_on_screen.x + 1,
                     player_on_screen.y + 1,
                     magic_spent);
        }
        else
            healthbar_offset += bar_height;

        if (Options.tile_show_minihealthbar)
        {
            const float min_hp = std::max(0, you.hp);
            const float health_divider = min_hp / (float) you.hp_max;

            const int hp_percent = (you.hp * 100) / you.hp_max;

            int hp_colour = GREEN;
            for (unsigned int i = 0; i < Options.hp_colour.size(); ++i)
                if (hp_percent <= Options.hp_colour[i].first)
                    hp_colour = Options.hp_colour[i].second;

            static const VColour healthy(   0, 255, 0, 255); // green
            static const VColour damaged( 255, 255, 0, 255); // yellow
            static const VColour wounded( 150,   0, 0, 255); // darkred
            static const VColour hp_spent(255,   0, 0, 255); // red

            buff.add(player_on_screen.x,
                     player_on_screen.y + healthbar_offset,
                     player_on_screen.x + health_divider,
                     player_on_screen.y + healthbar_offset + bar_height,
                     hp_colour == RED    ? wounded :
                     hp_colour == YELLOW ? damaged
                                         : healthy);

            buff.add(player_on_screen.x + health_divider,
                     player_on_screen.y + healthbar_offset,
                     player_on_screen.x + 1,
                     player_on_screen.y + healthbar_offset + bar_height,
                     hp_spent);
        }

        buff.draw(NULL, NULL);
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

// (0,0) = same position is handled elsewhere.
const int dir_dx[8] = {-1, 0, 1, -1, 1, -1,  0,  1};
const int dir_dy[8] = { 1, 1, 1,  0, 0, -1, -1, -1};

const int cmd_array[8] = {CMD_MOVE_DOWN_LEFT, CMD_MOVE_DOWN, CMD_MOVE_DOWN_RIGHT,
                          CMD_MOVE_LEFT, CMD_MOVE_RIGHT,
                          CMD_MOVE_UP_LEFT, CMD_MOVE_UP, CMD_MOVE_UP_RIGHT};


static int _adjacent_cmd(const coord_def &gc, const MouseEvent &event)
{
    const coord_def dir = gc - you.pos();
    const bool ctrl  = (event.mod & MOD_CTRL);
    for (int i = 0; i < 8; i++)
    {
        if (dir_dx[i] != dir.x || dir_dy[i] != dir.y)
            continue;

        int cmd = cmd_array[i];
        if (ctrl)
            cmd += CMD_OPEN_DOOR_LEFT - CMD_MOVE_LEFT;

        return command_to_key((command_type) cmd);
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

// FIXME: If the player is targeted, the game asks the player to target
// something with the mouse, then targets the player anyway and treats
// mouse click as if it hadn't come during targeting (moves the player
// to the clicked cell, whatever).
static void _add_targeting_commands(const coord_def& pos)
{
    // Force targeting cursor back onto center to start off on a clean
    // slate.
    macro_buf_add_cmd(CMD_TARGET_FIND_YOU);

    const coord_def delta = pos - you.pos();

    command_type cmd;

    if (delta.x < 0)
        cmd = CMD_TARGET_LEFT;
    else
        cmd = CMD_TARGET_RIGHT;

    for (int i = 0; i < std::abs(delta.x); i++)
        macro_buf_add_cmd(cmd);

    if (delta.y < 0)
        cmd = CMD_TARGET_UP;
    else
        cmd = CMD_TARGET_DOWN;

    for (int i = 0; i < std::abs(delta.y); i++)
        macro_buf_add_cmd(cmd);

    macro_buf_add_cmd(CMD_TARGET_MOUSE_MOVE);
}

static const bool _is_appropriate_spell(spell_type spell,
                                        const actor* target)
{
    ASSERT(is_valid_spell(spell));

    const unsigned int flags    = get_spell_flags(spell);
    const bool         targeted = flags & SPFLAG_TARGETING_MASK;

    // We don't handle grid targeted spells yet.
    if (flags & SPFLAG_GRID)
        return (false);

    // Most spells are blocked by transparent walls.
    if (targeted && !you.see_cell_no_trans(target->pos()))
    {
        switch(spell)
        {
        case SPELL_HELLFIRE_BURST:
        case SPELL_SMITING:
        case SPELL_HAUNT:
        case SPELL_FIRE_STORM:
        case SPELL_AIRSTRIKE:
            break;

        default:
            return (false);
        }
    }

    const bool helpful = flags & SPFLAG_HELPFUL;

    if (target->atype() == ACT_PLAYER)
    {
        if (flags & SPFLAG_NOT_SELF)
            return (false);

        return ((flags & (SPFLAG_HELPFUL | SPFLAG_ESCAPE | SPFLAG_RECOVERY))
                || !targeted);
    }

    if (!targeted)
        return (false);

    if (flags & SPFLAG_NEUTRAL)
        return (false);

    bool friendly = target->as_monster()->wont_attack();

    return (friendly == helpful);
}

static const bool _is_appropriate_evokable(const item_def& item,
                                           const actor* target)
{
    if (!item_is_evokable(item, false, true))
        return (false);

    // Only wands for now.
    if (item.base_type != OBJ_WANDS)
        return (false);

    // Aren't yet any wands that can go through transparent walls.
    if (!you.see_cell_no_trans(target->pos()))
        return (false);

    // We don't know what it is, so it *might* be appropriate.
    if (!item_type_known(item))
        return (true);

    // Random effects are always (in)apropriate for all targets.
    if (item.sub_type == WAND_RANDOM_EFFECTS)
        return (true);

    spell_type spell = zap_type_to_spell(item.zap());
    if (spell == SPELL_TELEPORT_OTHER && target->atype() == ACT_PLAYER)
        spell = SPELL_TELEPORT_SELF;

    return (_is_appropriate_spell(spell, target));
}

static const bool _have_appropriate_evokable(const actor* target)
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def &item(you.inv[i]);

        if (!item.is_valid())
            continue;

        if (_is_appropriate_evokable(item, target))
            return (true);
   }

    return (false);
}

static item_def* _get_evokable_item(const actor* target)
{
    std::vector<const item_def*> list;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def &item(you.inv[i]);

        if (!item.is_valid())
            continue;

        if (_is_appropriate_evokable(item, target))
            list.push_back(&item);
    }

    ASSERT(!list.empty());

    InvMenu menu(MF_SINGLESELECT | MF_ANYPRINTABLE
                 | MF_ALLOW_FORMATTING | MF_SELECT_BY_PAGE);
    menu.set_type(MT_ANY);
    menu.set_title("Wand to zap?");
    menu.load_items(list);
    menu.show();
    std::vector<SelItem> sel = menu.get_selitems();

    update_screen();
    redraw_screen();

    if (sel.empty())
        return (NULL);

    return ( const_cast<item_def*>(sel[0].item) );
}

static bool _evoke_item_on_target(actor* target)
{
    item_def* item;
    {
        // Prevent the inventory letter from being recorded twice.
        pause_all_key_recorders pause;

        item = _get_evokable_item(target);
    }

    if (item == NULL)
        return (false);

    if (item->base_type == OBJ_WANDS)
    {
        if (item->plus2 == ZAPCOUNT_EMPTY
            || item_type_known(*item) && item->plus <= 0)
        {
            mpr("That wand is empty.");
            return (false);
        }
    }

    macro_buf_add_cmd(CMD_EVOKE);
    macro_buf_add(index_to_letter(item->link)); // Inventory letter.
    _add_targeting_commands(target->pos());
    return (true);
}

static bool _spell_in_range(spell_type spell, actor* target)
{
    if (!(get_spell_flags(spell) & SPFLAG_TARGETING_MASK))
        return (true);

    int range = calc_spell_range(spell);

    switch(spell)
    {
    case SPELL_EVAPORATE:
    case SPELL_MEPHITIC_CLOUD:
    case SPELL_FIREBALL:
    case SPELL_FREEZING_CLOUD:
    case SPELL_POISONOUS_CLOUD:
        // Increase range by one due to cloud radius.
        range++;
        break;
    default:
        break;
    }

    return (range >= grid_distance(you.pos(), target->pos()));
}

static actor* _spell_target = NULL;

static bool _spell_selector(spell_type spell)
{
    return (_is_appropriate_spell(spell, _spell_target));
}

// TODO: Cast spells which target a particular cell.
static bool _cast_spell_on_target(actor* target)
{
    ASSERT(_spell_target == NULL);
    _spell_target = target;

    int letter;
    {
        // Prevent the spell letter from being recorded twice.
        pause_all_key_recorders pause;

        letter = list_spells(true, false, -1, _spell_selector);
    }

    _spell_target = NULL;

    if (letter == 0)
        return (false);

    const spell_type spell = get_spell_by_letter(letter);

    ASSERT(is_valid_spell(spell));
    ASSERT(_is_appropriate_spell(spell, target));

    if (!_spell_in_range(spell, target))
    {
        mprf("%s is out of range for that spell.",
             target->name(DESC_CAP_THE).c_str());
        return (true);
    }

    if (spell_mana(spell) > you.magic_points)
    {
        mpr( "You don't have enough mana to cast that spell.");
        return (true);
    }

    int item_slot = -1;
    if (spell == SPELL_EVAPORATE)
    {
        const int pot = prompt_invent_item("Throw which potion?", MT_INVLIST,
                                           OBJ_POTIONS);

        if (prompt_failed(pot))
            return (false);
        else if (you.inv[pot].base_type != OBJ_POTIONS)
        {
            mpr("This spell works only on potions!");
            return (false);
        }
        item_slot = you.inv[pot].slot;
    }

    macro_buf_add_cmd(CMD_FORCE_CAST_SPELL);
    macro_buf_add(letter);
    if (item_slot != -1)
        macro_buf_add(item_slot);

    if (get_spell_flags(spell) & SPFLAG_TARGETING_MASK)
        _add_targeting_commands(target->pos());

    return (true);
}

static const bool _have_appropriate_spell(const actor* target)
{
    for (size_t i = 0; i < you.spells.size(); i++)
    {
        spell_type spell = you.spells[i];

        if (!is_valid_spell(spell))
            continue;

        if (_is_appropriate_spell(spell, target))
            return (true);
    }
    return (false);
}

static bool _handle_distant_monster(monsters* mon, MouseEvent &event)
{
    const coord_def gc = mon->pos();
    const bool shift = (event.mod & MOD_SHIFT);
    const bool ctrl  = (event.mod & MOD_CTRL);
    const bool alt   = (shift && ctrl || (event.mod & MOD_ALT));

    // Handle evoking items at monster.
    if (alt && _have_appropriate_evokable(mon))
        return _evoke_item_on_target(mon);

    // Handle firing quivered items.
    if (shift && !ctrl && you.m_quiver->get_fire_item() != -1)
    {
        macro_buf_add_cmd(CMD_FIRE);
        _add_targeting_commands(mon->pos());
        return (true);
    }

    // Handle casting spells at monster.
    if (ctrl && !shift && _have_appropriate_spell(mon))
        return _cast_spell_on_target(mon);

    // Handle weapons of reaching.
    if (!mon->wont_attack() && you.see_cell_no_trans(mon->pos()))
    {
        const item_def* weapon = you.weapon();
        const coord_def delta  = you.pos() - mon->pos();
        const int       x_dist = std::abs(delta.x);
        const int       y_dist = std::abs(delta.y);

        if (weapon && get_weapon_brand(*weapon) == SPWPN_REACHING
            && std::max(x_dist, y_dist) == 2)
        {
            macro_buf_add_cmd(CMD_EVOKE_WIELDED);
            _add_targeting_commands(mon->pos());
            return (true);
        }
    }

    return (false);
}

static bool _handle_zap_player(MouseEvent &event)
{
    const bool shift = (event.mod & MOD_SHIFT);
    const bool ctrl  = (event.mod & MOD_CTRL);
    const bool alt   = (shift && ctrl || (event.mod & MOD_ALT));

    if (alt && _have_appropriate_evokable(&you))
        return _evoke_item_on_target(&you);

    if (ctrl && _have_appropriate_spell(&you))
        return _cast_spell_on_target(&you);

    return (false);
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
        std::string desc = get_terse_square_desc(gc);
        // Suppress floor description
        if (desc == "floor")
            desc = "";

        if (you.see_cell(gc))
        {
            const int cloudidx = env.cgrid(gc);
            if (cloudidx != EMPTY_CLOUD)
            {
                std::string terrain_desc = desc;
                desc = cloud_name(cloudidx);

                if (!terrain_desc.empty())
                    desc += "\n" + terrain_desc;
            }
        }

        if (!desc.empty())
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
            if ((event.mod & (MOD_CTRL | MOD_ALT)))
            {
                if (_handle_zap_player(event))
                    return 0;
            }

            if (!(event.mod & MOD_SHIFT))
                return command_to_key(CMD_PICKUP);

            const dungeon_feature_type feat = grd(gc);
            switch (feat_stair_direction(feat))
            {
            case CMD_GO_DOWNSTAIRS:
            case CMD_GO_UPSTAIRS:
                return command_to_key(feat_stair_direction(feat));
            default:
                if (feat_is_altar(feat)
                    && player_can_join_god(feat_altar_god(feat)))
                {
                    return command_to_key(CMD_PRAY);
                }
                return 0;
            }
        }
        case MouseEvent::RIGHT:
            if (!(event.mod & MOD_SHIFT))
                return command_to_key(CMD_RESISTS_SCREEN); // Character overview.
            if (you.religion != GOD_NO_GOD)
                return command_to_key(CMD_DISPLAY_RELIGION); // Religion screen.

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

    monsters* mon = monster_at(gc);
    if (mon && you.can_see(mon))
    {
        if (_handle_distant_monster(mon, event))
            return (CK_MOUSE_CMD);
    }

    if ((event.mod & MOD_CTRL) && adjacent(you.pos(), gc))
        return _click_travel(gc, event);

    // Don't move if we've tried to fire/cast/evoke when there's nothing
    // available.
    if (event.mod & (MOD_SHIFT | MOD_CTRL | MOD_ALT))
        return (CK_MOUSE_CMD);

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

    const bool have_reach = you.weapon()
        && get_weapon_brand(*(you.weapon())) == SPWPN_REACHING;
    const int  attack_dist = have_reach ? 2 : 1;

    std::vector<command_type> cmd;
    if (m_cursor[CURSOR_MOUSE] == you.pos())
    {
        tip = you.your_name;
        tip += " (";
        tip += get_species_abbrev(you.species);
        tip += get_job_abbrev(you.char_class);
        tip += ")";

        if (you.visible_igrd(m_cursor[CURSOR_MOUSE]) != NON_ITEM)
        {
            tip += "\n[L-Click] Pick up items (%)";
            cmd.push_back(CMD_PICKUP);
        }

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

            tip += " (%)";
            cmd.push_back(dir);
        }
        else if (feat_is_altar(feat) && player_can_join_god(feat_altar_god(feat)))
        {
            tip += "\n[Shift-L-Click] pray on altar (%)";
            cmd.push_back(CMD_PRAY);
        }

        // Character overview.
        tip += "\n[R-Click] Overview (%)";
        cmd.push_back(CMD_RESISTS_SCREEN);

        // Religion.
        if (you.religion != GOD_NO_GOD)
        {
            tip += "\n[Shift-R-Click] Religion (%)";
            cmd.push_back(CMD_DISPLAY_RELIGION);
        }
    }
    else if (abs(m_cursor[CURSOR_MOUSE].x - you.pos().x) <= attack_dist
             && abs(m_cursor[CURSOR_MOUSE].y - you.pos().y) <= attack_dist)
    {
        tip = "";

        if (!cell_is_solid(m_cursor[CURSOR_MOUSE]))
        {
            const monsters *mon = monster_at(m_cursor[CURSOR_MOUSE]);
            if (!mon || mon->friendly() || !mon->visible_to(&you))
                tip = "[L-Click] Move\n";
            else if (mon)
            {
                tip = mon->name(DESC_CAP_A);
                tip += "\n[L-Click] Attack\n";
            }
        }
    }
    else
    {
        if (i_feel_safe() && !cell_is_solid(m_cursor[CURSOR_MOUSE]))
            tip = "[L-Click] Travel\n";
    }

    if (m_cursor[CURSOR_MOUSE] != you.pos())
    {
        const monsters* mon = monster_at(m_cursor[CURSOR_MOUSE]);
        if (mon && you.can_see(mon))
        {
            if (you.see_cell_no_trans(mon->pos())
                && you.m_quiver->get_fire_item() != -1)
            {
                tip += "[Shift-L-Click] Fire (%)\n";
                cmd.push_back(CMD_FIRE);
            }
        }
    }

    const actor* target = actor_at(m_cursor[CURSOR_MOUSE]);
    if (target && you.can_see(target))
    {
        std::string str = "";

        if (_have_appropriate_spell(target))
        {
            str += "[Ctrl-L-Click] Cast spell (%)\n";
            cmd.push_back(CMD_CAST_SPELL);
        }

        if (_have_appropriate_evokable(target))
        {
            std::string key = "Alt";
#ifdef UNIX
            // On Unix systems the Alt key is already hogged by
            // the application window, at least when we're not
            // in fullscreen mode, so we use Ctrl-Shift instead.
            if (!tiles.is_fullscreen())
                key = "Ctrl-Shift";
#endif
            str += "[" + key + "-L-Click] Zap wand (%)\n";
            cmd.push_back(CMD_EVOKE);
        }

        if (!str.empty())
        {
            if (m_cursor[CURSOR_MOUSE] == you.pos())
                tip += "\n";

            tip += str;
        }
    }

    if (m_cursor[CURSOR_MOUSE] != you.pos())
        tip += "[R-Click] Describe";

    if (!target && adjacent(m_cursor[CURSOR_MOUSE], you.pos()))
    {
        trap_def *trap = find_trap(m_cursor[CURSOR_MOUSE]);
        if (trap && trap->is_known()
            && trap->category() == DNGN_TRAP_MECHANICAL)
        {
            tip += "\n[Ctrl-L-Click] Disarm";
        }
        else if (grd(m_cursor[CURSOR_MOUSE]) == DNGN_OPEN_DOOR)
            tip += "\n[Ctrl-L-Click] Close door (C)";
        else if (feat_is_closed_door(grd(m_cursor[CURSOR_MOUSE])))
            tip += "\n[Ctrl-L-Click] Open door (O)";
    }

    insert_commands(tip, cmd, false);

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
    if (you.see_cell(gc))
    {
        get_square_desc(gc, inf, true, false);
        const int cloudidx = env.cgrid(gc);
        if (cloudidx != EMPTY_CLOUD)
        {
            inf.prefix = "There is a cloud of " + cloud_name(cloudidx)
                         + " here.\n\n";
        }
    }
    else if (grid_appearance(gc) != DNGN_FLOOR
             && !feat_is_wall(grid_appearance(gc)))
    {
        get_feature_desc(gc, inf);
    }
    else
    {
        // For plain floor, output the stash description.
        const std::string stash = get_stash_desc(gc);
        if (!stash.empty())
            inf.body << "\n" << stash;
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

GridRegion::GridRegion(ImageManager *im, FontWrapper *tag_font,
                       int tile_x, int tile_y) :
    TileRegion(im, tag_font, tile_x, tile_y),
    m_flavour(NULL),
    m_cursor(NO_CURSOR),
    m_buf_dngn(&im->m_textures[TEX_DUNGEON]),
    m_buf_spells(&im->m_textures[TEX_GUI]),
    m_buf_main(&im->m_textures[TEX_DEFAULT])

{
}

GridRegion::~GridRegion()
{
    delete[] m_flavour;
    m_flavour = NULL;
}

void GridRegion::clear()
{
    m_items.clear();
    m_buf_dngn.clear();
    m_buf_main.clear();
    m_buf_spells.clear();
}

void GridRegion::on_resize()
{
    delete[] m_flavour;
    if (mx * my <= 0)
        return;

    m_flavour = new unsigned char[mx * my];
    for (int i = 0; i < mx * my; ++i)
        m_flavour[i] = random2((unsigned char)~0);
}

unsigned int GridRegion::cursor_index() const
{
    ASSERT(m_cursor != NO_CURSOR);
    return (m_cursor.x + m_cursor.y * mx);
}

void GridRegion::place_cursor(const coord_def &cursor)
{
    if (m_cursor != NO_CURSOR && cursor_index() < m_items.size())
    {
        m_items[cursor_index()].flag &= ~TILEI_FLAG_CURSOR;
        m_dirty = true;
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

void GridRegion::draw_desc(const char *desc)
{
    ASSERT(m_tag_font);
    ASSERT(desc);

    // Always draw the description in the inventory header. (jpeg)
    int x = sx + ox + dx / 2;
    int y = sy + oy;

    const coord_def min_pos(sx, sy - dy);
    const coord_def max_pos(ex, ey);

    m_tag_font->render_string(x, y, desc, min_pos, max_pos, WHITE, false, 200);
}

bool GridRegion::place_cursor(MouseEvent &event, unsigned int &item_idx)
{
    int cx, cy;
    if (!mouse_pos(event.px, event.py, cx, cy))
    {
        place_cursor(NO_CURSOR);
        return (false);
    }

    const coord_def cursor(cx, cy);
    place_cursor(cursor);

    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return (false);

    if (event.event != MouseEvent::PRESS)
        return (false);

    item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return (false);

    return (true);
}

void GridRegion::add_quad_char(char c, int x, int y, int ofs_x, int ofs_y)
{
    int num = c - '0';
    ASSERT(num >= 0 && num <= 9);
    int idx = TILE_NUM0 + num;

    m_buf_main.add(idx, x, y, ofs_x, ofs_y, false);
}

void GridRegion::render()
{
    if (m_dirty)
    {
        m_buf_dngn.clear();
        m_buf_main.clear();
        m_buf_spells.clear();

        // Ensure the cursor has been placed.
        place_cursor(m_cursor);

        pack_buffers();
        m_dirty = false;
    }

#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering GridRegion\n");
#endif
    set_transform();
    m_buf_dngn.draw(NULL, NULL);
    m_buf_spells.draw(NULL, NULL);
    m_buf_main.draw(NULL, NULL);

    draw_tag();
}

void GridRegion::draw_number(int x, int y, int num)
{
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

SpellRegion::SpellRegion(ImageManager* im, FontWrapper *tag_font,
                         int tile_x, int tile_y) :
    GridRegion(im, tag_font, tile_x, tile_y)
{
    memorise = false;
}

bool SpellRegion::check_memorise()
{
    return (memorise);
}

void SpellRegion::activate()
{
    if (!you.spell_no)
    {
        canned_msg(MSG_NO_SPELLS);
        flush_prev_message();
    }
}

void SpellRegion::draw_tag()
{
    if (m_cursor == NO_CURSOR)
        return;

    int curs_index = cursor_index();
    if (curs_index >= (int)m_items.size())
        return;
    int idx = m_items[curs_index].idx;
    if (idx == -1)
        return;

    const spell_type spell = (spell_type) idx;
    std::string desc = make_stringf("%d MP    %s    (%s)",
                                    spell_difficulty(spell),
                                    spell_title(spell),
                                    failure_rate_to_string(spell_fail(spell)));
    draw_desc(desc.c_str());
}

int SpellRegion::handle_mouse(MouseEvent &event)
{
    unsigned int item_idx;
    if (!place_cursor(event, item_idx))
        return (0);

    const spell_type spell = (spell_type) m_items[item_idx].idx;
    if (event.button == MouseEvent::LEFT)
    {
        you.last_clicked_item = item_idx;
        tiles.set_need_redraw();
        if (!cast_a_spell(false, spell))
            flush_input_buffer( FLUSH_ON_FAILURE );
        return CK_MOUSE_CMD;
    }
    else if (spell != NUM_SPELLS && event.button == MouseEvent::RIGHT)
    {
        describe_spell(spell);
        redraw_screen();
        return CK_MOUSE_CMD;
    }
    return (0);
}

bool SpellRegion::update_tab_tip_text(std::string &tip, bool active)
{
    const char *prefix1 = active ? "" : "[L-Click] ";
    const char *prefix2 = active ? "" : "          ";

    tip = make_stringf("%s%s\n%s%s",
                       prefix1, "Display memorised spells",
                       prefix2, "Cast spells");

    return (true);
}

bool SpellRegion::update_tip_text(std::string& tip)
{
    if (m_cursor == NO_CURSOR)
        return (false);

    unsigned int item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return (false);

    int flag = m_items[item_idx].flag;
    std::vector<command_type> cmd;
    if (flag & TILEI_FLAG_INVALID)
        tip = "You cannot cast this spell right now.";
    else
    {
        tip = "[L-Click] Cast (%)";
        cmd.push_back(CMD_CAST_SPELL);
    }

    tip += "\n[R-Click] Describe (%)";
    cmd.push_back(CMD_DISPLAY_SPELLS);
    insert_commands(tip, cmd);

    return (true);
}

bool SpellRegion::update_alt_text(std::string &alt)
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

    const spell_type spell = (spell_type) idx;

    describe_info inf;
    get_spell_desc(spell, inf);

    alt_desc_proc proc(crawl_view.msgsz.x, crawl_view.msgsz.y);
    process_description<alt_desc_proc>(proc, inf);

    proc.get_string(alt);

    return (true);
}

static int _get_max_spells()
{
    int max_spells = 21;
    if (you.has_spell(SPELL_SELECTIVE_AMNESIA))
        max_spells++;

    return (max_spells);
}

void SpellRegion::pack_buffers()
{
    const int max_spells = (check_memorise() ? m_items.size()
                                             : _get_max_spells());

    // Pack base separately, as it comes from a different texture...
    int i = 0;
    for (int y = 0; y < my; y++)
    {
        if (i >= max_spells)
            break;

        for (int x = 0; x < mx; x++)
        {
            if (i++ >= max_spells)
                break;

            m_buf_dngn.add(TILE_ITEM_SLOT, x, y);
        }
    }

    i = 0;
    for (int y = 0; y < my; y++)
    {
        if (i >= (int)m_items.size())
            break;

        for (int x = 0; x < mx; x++)
        {
            if (i >= (int)m_items.size())
                break;

            InventoryTile &item = m_items[i++];
            if (item.flag & TILEI_FLAG_INVALID)
                m_buf_main.add(TILE_MESH, x, y);

            if (item.flag & TILEI_FLAG_CURSOR)
                m_buf_main.add(TILE_CURSOR, x, y);

            if (item.quantity != -1)
                draw_number(x, y, item.quantity);

            if (item.tile)
                m_buf_spells.add(item.tile, x, y);
        }
    }
}

void SpellRegion::update()
{
    m_items.clear();
    m_dirty = true;

    if (mx * my == 0)
        return;

    const unsigned int max_spells = std::min(22, mx*my);

    for (int i = 0; i < 52; ++i)
    {
        const char letter = index_to_letter(i);
        const spell_type spell = get_spell_by_letter(letter);
        if (spell == SPELL_NO_SPELL)
            continue;

        InventoryTile desc;
        desc.tile     = tileidx_spell(spell);
        desc.idx      = (int) spell;
        desc.quantity = spell_difficulty(spell);

        std::string temp;
        if (is_prevented_teleport(spell)
            || spell_is_uncastable(spell, temp)
            || spell_mana(spell) > you.magic_points)
        {
            desc.flag |= TILEI_FLAG_INVALID;
        }

        m_items.push_back(desc);

        if (m_items.size() >= max_spells)
            break;
    }
}

MemoriseRegion::MemoriseRegion(ImageManager* im, FontWrapper *tag_font,
                         int tile_x, int tile_y) :
    SpellRegion(im, tag_font, tile_x, tile_y)
{
    memorise = true;
}

void MemoriseRegion::activate()
{
    // Print a fitting message if we can't memorise anything.
    has_spells_to_memorise(false);
}

void MemoriseRegion::draw_tag()
{
    if (m_cursor == NO_CURSOR)
        return;
    int curs_index = cursor_index();
    if (curs_index >= (int)m_items.size())
        return;
    int idx = m_items[curs_index].idx;
    if (idx == -1)
        return;

    const spell_type spell = (spell_type) idx;
    std::string desc = make_stringf("%s    (%s)    %d/%d spell slot%s",
                                    spell_title(spell),
                                    failure_rate_to_string(spell_fail(spell)),
                                    spell_levels_required(spell),
                                    player_spell_levels(),
                                    spell_levels_required(spell) > 1 ? "s" : "");
    draw_desc(desc.c_str());
}

int MemoriseRegion::handle_mouse(MouseEvent &event)
{
    unsigned int item_idx;
    if (!place_cursor(event, item_idx))
        return (0);

    const spell_type spell = (spell_type) m_items[item_idx].idx;
    if (event.button == MouseEvent::LEFT)
    {
        you.last_clicked_item = item_idx;
        tiles.set_need_redraw();
        if (learn_spell(spell, m_items[item_idx].special))
            tiles.update_inventory();
        else
            flush_input_buffer(FLUSH_ON_FAILURE);
        return CK_MOUSE_CMD;
    }
    else if (event.button == MouseEvent::RIGHT)
    {
        describe_spell(spell);
        redraw_screen();
        return CK_MOUSE_CMD;
    }
    return (0);
}

bool MemoriseRegion::update_tab_tip_text(std::string &tip, bool active)
{
    const char *prefix1 = active ? "" : "[L-Click] ";
    const char *prefix2 = active ? "" : "          ";

    tip = make_stringf("%s%s\n%s%s",
                       prefix1, "Display spells in carried books",
                       prefix2, "Memorise spells");

    return (true);
}

bool MemoriseRegion::update_tip_text(std::string& tip)
{
    if (m_cursor == NO_CURSOR)
        return (false);

    unsigned int item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return (false);

    int flag = m_items[item_idx].flag;
    std::vector<command_type> cmd;
    if (flag & TILEI_FLAG_INVALID)
        tip = "You cannot memorise this spell now.";
    else
    {
        tip = "[L-Click] Memorise (%)";
        cmd.push_back(CMD_MEMORISE_SPELL);
    }

    tip += "\n[R-Click] Describe";

    insert_commands(tip, cmd);
    return (true);
}

void MemoriseRegion::update()
{
    m_items.clear();
    m_dirty = true;

    if (mx * my == 0)
        return;

    if (!has_spells_to_memorise())
        return;

    const unsigned int max_spells = mx * my;

    std::vector<int>  books;
    std::vector<spell_type> spells = get_mem_spell_list(books);
    for (unsigned int i = 0; m_items.size() < max_spells && i < spells.size();
         ++i)
    {
        const spell_type spell = spells[i];

        InventoryTile desc;
        desc.tile     = tileidx_spell(spell);
        desc.idx      = (int) spell;
        desc.special  = books[i];
        desc.quantity = spell_difficulty(spell);

        if (!can_learn_spell(true)
            || spell_difficulty(spell) > you.experience_level
            || player_spell_levels() < spell_levels_required(spell))
        {
            desc.flag |= TILEI_FLAG_INVALID;
        }
        m_items.push_back(desc);
    }
}

InventoryRegion::InventoryRegion(ImageManager* im, FontWrapper *tag_font,
                                 int tile_x, int tile_y) :
    GridRegion(im, tag_font, tile_x, tile_y)
{
}

void InventoryRegion::pack_buffers()
{
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
                draw_number(x, y, item.quantity);

            if (item.special)
                m_buf_main.add(item.special, x, y, 0, 0, false);

            if (item.flag & TILEI_FLAG_TRIED)
                m_buf_main.add(TILE_TRIED, x, y, 0, TILE_Y / 2, false);

            if (item.flag & TILEI_FLAG_INVALID)
                m_buf_main.add(TILE_MESH, x, y);
        }
    }
}

int InventoryRegion::handle_mouse(MouseEvent &event)
{
    unsigned int item_idx;
    if (!place_cursor(event, item_idx))
        return (0);

    int idx = m_items[item_idx].idx;

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

    if (equipped && item.cursed())
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

static void _handle_wield_tip(std::string &tip, std::vector<command_type> &cmd,
                              const std::string prefix = "",
                              bool unwield = false)
{
    tip += prefix;
    if (unwield)
        tip += "Unwield (%-)";
    else
        tip += "Wield (%)";
    cmd.push_back(CMD_WIELD_WEAPON);
}

bool InventoryRegion::update_tab_tip_text(std::string &tip, bool active)
{
    const char *prefix1 = active ? "" : "[L-Click] ";
    const char *prefix2 = active ? "" : "          ";

    tip = make_stringf("%s%s\n%s%s",
                       prefix1, "Display inventory",
                       prefix2, "Use items");

    return (true);
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

    // TODO enne - should the command keys here respect keymaps?
    std::vector<command_type> cmd;
    if (m_items[item_idx].flag & TILEI_FLAG_FLOOR)
    {
        const item_def &item = mitm[idx];

        if (!item.is_valid())
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

        tip += "\n[L-Click] Pick up (%)";
        cmd.push_back(CMD_PICKUP);
        if (item.base_type == OBJ_CORPSES
            && item.sub_type != CORPSE_SKELETON
            && !food_is_rotten(item))
        {
            tip += "\n[Shift-L-Click] ";
            if (can_bottle_blood_from_corpse(item.plus))
                tip += "Bottle blood";
            else
                tip += "Chop up";
            tip += " (%)";
            cmd.push_back(CMD_BUTCHER);

            if (you.species == SP_VAMPIRE)
            {
                tip += "\n\n[Shift-R-Click] Drink blood (e)";
                cmd.push_back(CMD_EAT);
            }
        }
        else if (item.base_type == OBJ_FOOD
                 && you.is_undead != US_UNDEAD
                 && you.species != SP_VAMPIRE)
        {
            tip += "\n[Shift-R-Click] Eat (e)";
            cmd.push_back(CMD_EAT);
        }
    }
    else
    {
        const item_def &item = you.inv[idx];
        if (!item.is_valid())
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
                _handle_wield_tip(tip, cmd);
                if (is_throwable(&you, item))
                {
                    tip += "\n[Ctrl-L-Click] Fire (f)";
                    cmd.push_back(CMD_FIRE);
                }
                break;
            case OBJ_WEAPONS + EQUIP_OFFSET:
                _handle_wield_tip(tip, cmd, "", true);
                if (is_throwable(&you, item))
                {
                    tip += "\n[Ctrl-L-Click] Fire (f)";
                    cmd.push_back(CMD_FIRE);
                }
                break;
            case OBJ_MISCELLANY:
                if (item.sub_type >= MISC_DECK_OF_ESCAPE
                    && item.sub_type <= MISC_DECK_OF_DEFENCE)
                {
                    _handle_wield_tip(tip, cmd);
                    break;
                }
                tip += "Evoke (V)";
                cmd.push_back(CMD_EVOKE);
                break;
            case OBJ_MISCELLANY + EQUIP_OFFSET:
                if (item.sub_type >= MISC_DECK_OF_ESCAPE
                    && item.sub_type <= MISC_DECK_OF_DEFENCE)
                {
                    tip += "Draw a card (%)\n";
                    cmd.push_back(CMD_EVOKE_WIELDED);
                    _handle_wield_tip(tip, cmd, "[Ctrl-L-Click]", true);
                    break;
                }
                // else fall-through
            case OBJ_STAVES + EQUIP_OFFSET: // rods - other staves handled above
                tip += "Evoke (%)\n";
                cmd.push_back(CMD_EVOKE_WIELDED);
                _handle_wield_tip(tip, cmd, "[Ctrl-L-Click]", true);
                break;
            case OBJ_ARMOUR:
                tip += "Wear (%)";
                cmd.push_back(CMD_WEAR_ARMOUR);
                break;
            case OBJ_ARMOUR + EQUIP_OFFSET:
                tip += "Take off (%)";
                cmd.push_back(CMD_REMOVE_ARMOUR);
                break;
            case OBJ_JEWELLERY:
                tip += "Put on (%)";
                cmd.push_back(CMD_WEAR_JEWELLERY);
                break;
            case OBJ_JEWELLERY + EQUIP_OFFSET:
                tip += "Remove (%)";
                cmd.push_back(CMD_REMOVE_JEWELLERY);
                break;
            case OBJ_MISSILES:
                tip += "Fire (%)";
                cmd.push_back(CMD_FIRE);

                if (wielded)
                    _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]", true);
                else if (item.sub_type == MI_STONE
                            && you.has_spell(SPELL_SANDBLAST)
                         || item.sub_type == MI_ARROW
                            && you.has_spell(SPELL_STICKS_TO_SNAKES))
                {
                    // For Sandblast and Sticks to Snakes,
                    // respectively.
                    _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]");
                }
                break;
            case OBJ_WANDS:
                tip += "Evoke (%)";
                cmd.push_back(CMD_EVOKE);
                if (wielded)
                    _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]", true);
                break;
            case OBJ_BOOKS:
                if (item_type_known(item)
                    && item.sub_type != BOOK_MANUAL
                    && item.sub_type != BOOK_DESTRUCTION
                    && can_learn_spell(true))
                {
                    if (player_can_memorise_from_spellbook(item)
                        || has_spells_to_memorise(true))
                    {
                        tip += "Memorise (%)";
                        cmd.push_back(CMD_MEMORISE_SPELL);
                    }
                    if (wielded)
                        _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]", true);
                    break;
                }
                // else fall-through
            case OBJ_SCROLLS:
                tip += "Read (%)";
                cmd.push_back(CMD_READ);
                if (wielded)
                    _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]", true);
                break;
            case OBJ_POTIONS:
                tip += "Quaff (%)";
                cmd.push_back(CMD_QUAFF);
                // For Sublimation of Blood.
                if (wielded)
                    _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]", true);
                else if (item_type_known(item)
                         && is_blood_potion(item)
                         && you.has_spell(SPELL_SUBLIMATION_OF_BLOOD))
                {
                    _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]");
                }
                break;
            case OBJ_FOOD:
                tip += "Eat (%)";
                cmd.push_back(CMD_EAT);
                // For Sublimation of Blood.
                if (wielded)
                    _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]", true);
                else if (item.sub_type == FOOD_CHUNK
                         && you.has_spell(SPELL_SUBLIMATION_OF_BLOOD))
                {
                    _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]");
                }
                break;
            case OBJ_CORPSES:
                if (you.species == SP_VAMPIRE)
                {
                    tip += "Drink blood (%)";
                    cmd.push_back(CMD_EAT);
                }

                if (wielded)
                {
                    if (you.species == SP_VAMPIRE)
                        tip += "\n";
                    _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]", true);
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
                _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]", true);
            else if (you.has_spell(SPELL_BONE_SHARDS))
                _handle_wield_tip(tip, cmd, "\n[Ctrl-L-Click]");
        }

        tip += "\n[R-Click] Describe";
        // Has to be non-equipped or non-cursed to drop.
        if (!equipped || !_is_true_equipped_item(you.inv[idx])
            || !you.inv[idx].cursed())
        {
            tip += "\n[Shift-L-Click] Drop (%)";
            cmd.push_back(CMD_DROP);
        }
    }

    insert_commands(tip, cmd);
    return (true);
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
    const item_def *item;

    if (m_items[item_idx].flag & TILEI_FLAG_FLOOR)
        item = &mitm[idx];
    else
        item = &you.inv[idx];

    if (!item->is_valid())
        return (false);

    describe_info inf;
    get_item_desc(*item, inf, true);

    alt_desc_proc proc(crawl_view.msgsz.x, crawl_view.msgsz.y);
    process_description<alt_desc_proc>(proc, inf);

    proc.get_string(alt);

    return (true);
}

void InventoryRegion::draw_tag()
{
    if (m_cursor == NO_CURSOR)
        return;
    int curs_index = cursor_index();
    if (curs_index >= (int)m_items.size())
        return;
    int idx = m_items[curs_index].idx;
    if (idx == -1)
        return;

    bool floor = m_items[curs_index].flag & TILEI_FLAG_FLOOR;

    if (floor && mitm[idx].is_valid())
        draw_desc(mitm[idx].name(DESC_PLAIN).c_str());
    else if (!floor && you.inv[idx].is_valid())
        draw_desc(you.inv[idx].name(DESC_INVENTORY_EQUIP).c_str());
}

void InventoryRegion::activate()
{
    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        flush_prev_message();
    }
}

static void _fill_item_info(InventoryTile &desc, const item_def &item)
{
    desc.tile = tileidx_item(item);

    int type = item.base_type;
    if (type == OBJ_FOOD || type == OBJ_SCROLLS
        || type == OBJ_POTIONS || type == OBJ_MISSILES)
    {
        // -1 specifies don't display anything
        desc.quantity = (item.quantity == 1) ? -1 : item.quantity;
    }
    else if (type == OBJ_WANDS
             && ((item.flags & ISFLAG_KNOW_PLUSES)
                 || item.plus2 == ZAPCOUNT_EMPTY))
    {
        desc.quantity = item.plus;
    }
    else if (item_is_rod(item) && item.flags & ISFLAG_KNOW_PLUSES)
        desc.quantity = item.plus / ROD_CHARGE_MULT;
    else
        desc.quantity = -1;

    if (type == OBJ_WEAPONS || type == OBJ_MISSILES || type == OBJ_ARMOUR)
        desc.special = tile_known_brand(item);
    else if (type == OBJ_CORPSES)
        desc.special = tile_corpse_brand(item);

    desc.flag = 0;
    if (item.cursed() && item_ident(item, ISFLAG_KNOW_CURSE))
        desc.flag |= TILEI_FLAG_CURSE;
    if (item_type_tried(item))
        desc.flag |= TILEI_FLAG_TRIED;
    if (item.pos.x != -1)
        desc.flag |= TILEI_FLAG_FLOOR;
}

void InventoryRegion::update()
{
    m_items.clear();
    m_dirty = true;

    if (mx * my == 0)
        return;

    // item.base_type <-> char conversion table
    const static char *obj_syms = ")([/%#?=!#+\\0}x";

    int max_pack_row = (ENDOFPACK-1) / mx + 1;
    int max_pack_items = max_pack_row * mx;

    bool inv_shown[ENDOFPACK];
    memset(inv_shown, 0, sizeof(inv_shown));

    int num_ground = 0;
    for (int i = you.visible_igrd(you.pos()); i != NON_ITEM; i = mitm[i].link)
        num_ground++;

    // If the inventory is full, show at least one row of the ground.
    int min_ground = std::min(num_ground, mx);
    max_pack_items = std::min(max_pack_items, mx * my - min_ground);
    max_pack_items = std::min(ENDOFPACK, max_pack_items);

    const size_t show_types_len = strlen(Options.tile_show_items);
    // Special case: show any type if (c == show_types_len).
    for (unsigned int c = 0; c <= show_types_len; c++)
    {
        if ((int)m_items.size() >= max_pack_items)
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
        for (int i = 0; i < ENDOFPACK; ++i)
        {
            if ((int)m_items.size() >= max_pack_items)
                break;

            if (inv_shown[i]
                || !you.inv[i].is_valid()
                || you.inv[i].quantity == 0
                || (!show_any && you.inv[i].base_type != type))
            {
                continue;
            }

            InventoryTile desc;
            _fill_item_info(desc, you.inv[i]);
            desc.idx = i;

            for (int eq = 0; eq < NUM_EQUIP; ++eq)
            {
                if (you.equip[eq] == i)
                {
                    desc.flag |= TILEI_FLAG_EQUIP;
                    if (you.melded[eq])
                        desc.flag |= TILEI_FLAG_MELDED;
                    break;
                }
            }

            inv_shown[i] = true;
            m_items.push_back(desc);
        }
    }

    int remaining = mx*my - m_items.size();
    int empty_on_this_row = mx - m_items.size() % mx;

    // If we're not on the last row...
    if ((int)m_items.size() < mx * (my-1))
    {
        if (num_ground > remaining - empty_on_this_row)
        {
            // Fill out part of this row.
            int fill = remaining - num_ground;
            for (int i = 0; i < fill; ++i)
            {
                InventoryTile desc;
                if ((int)m_items.size() >= max_pack_items)
                    desc.flag |= TILEI_FLAG_INVALID;
                m_items.push_back(desc);
            }
        }
        else
        {
            // Fill out the rest of this row.
            while (m_items.size() % mx != 0)
            {
                InventoryTile desc;
                if ((int)m_items.size() >= max_pack_items)
                    desc.flag |= TILEI_FLAG_INVALID;
                m_items.push_back(desc);
            }

            // Add extra rows, if needed.
            unsigned int ground_rows =
                std::max((num_ground-1) / mx + 1, 1);

            while ((int)(m_items.size() / mx + ground_rows) < my
                   && ((int)m_items.size()) < max_pack_items)
            {
                for (int i = 0; i < mx; i++)
                {
                    InventoryTile desc;
                    if ((int)m_items.size() >= max_pack_items)
                        desc.flag |= TILEI_FLAG_INVALID;
                    m_items.push_back(desc);
                }
            }
        }
    }

    // Then, as many ground items as we can fit.
    bool ground_shown[MAX_ITEMS];
    memset(ground_shown, 0, sizeof(ground_shown));
    for (unsigned int c = 0; c <= show_types_len; c++)
    {
        if ((int)m_items.size() >= mx * my)
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

        for (int i = you.visible_igrd(you.pos()); i != NON_ITEM;
             i = mitm[i].link)
        {
            if ((int)m_items.size() >= mx * my)
                break;

            if (ground_shown[i] || !show_any && mitm[i].base_type != type)
                continue;

            InventoryTile desc;
            _fill_item_info(desc, mitm[i]);
            desc.idx = i;
            ground_shown[i] = true;

            m_items.push_back(desc);
        }
    }

    while ((int)m_items.size() < mx * my)
    {
        InventoryTile desc;
        desc.flag = TILEI_FLAG_FLOOR;
        m_items.push_back(desc);
    }
}

TabbedRegion::TabbedRegion(ImageManager *im, FontWrapper *tag_font,
                           int tile_x, int tile_y) :
    GridRegion(im, tag_font, tile_x, tile_y),
    m_active(0),
    m_mouse_tab(-1),
    m_buf_gui(&im->m_textures[TEX_GUI])
{

}

TabbedRegion::~TabbedRegion()
{
}

void TabbedRegion::set_tab_region(int idx, GridRegion *reg, int tile_tab)
{
    ASSERT(idx >= 0);
    ASSERT(idx >= (int)m_tabs.size() || !m_tabs[idx].reg);
    ASSERT(tile_tab);

    for (int i = (int)m_tabs.size(); i <= idx; ++i)
    {
        TabInfo inf;
        inf.reg = NULL;
        inf.tile_tab = 0;
        inf.ofs_y = 0;
        inf.min_y = 0;
        inf.max_y = 0;
        m_tabs.push_back(inf);
    }

    int start_y = 0;
    for (int i = 0; i < (int)m_tabs.size(); ++i)
    {
        if (!m_tabs[i].reg)
            continue;
        start_y = std::max(m_tabs[i].max_y + 1, start_y);
    }

    const tile_info &inf = tile_gui_info(tile_tab);
    int max_height = inf.height;
    ox = std::max((int)inf.width, ox);

    // All tabs should be the same size.
    for (int i = 1; i < TAB_OFS_MAX; ++i)
    {
        const tile_info &inf_other = tile_gui_info(tile_tab + i);
        ASSERT(inf_other.height == inf.height);
        ASSERT(inf_other.width == inf.width);
    }

    ASSERT((int)m_tabs.size() > idx);
    m_tabs[idx].reg = reg;
    m_tabs[idx].tile_tab = tile_tab;
    m_tabs[idx].min_y = start_y;
    m_tabs[idx].max_y = start_y + max_height;

    recalculate();
}

GridRegion *TabbedRegion::get_tab_region(int idx)
{
    if (idx < 0 || (int)m_tabs.size() <= idx)
        return (NULL);

    return (m_tabs[idx].reg);
}

void TabbedRegion::activate_tab(int idx)
{
    if (idx < 0 || (int)m_tabs.size() <= idx)
        return;

    if (m_active == idx)
        return;

    m_active = idx;
    m_dirty  = true;
    tiles.set_need_redraw();

    if (m_tabs[m_active].reg)
    {
        m_tabs[m_active].reg->activate();
        m_tabs[m_active].reg->update();
    }
}

int TabbedRegion::active_tab() const
{
    return (m_active);
}

int TabbedRegion::num_tabs() const
{
    return (m_tabs.size());
}

bool TabbedRegion::active_is_valid() const
{
    if (m_active < 0 || (int)m_tabs.size() <= m_active)
        return (false);
    if (!m_tabs[m_active].reg)
        return (false);

    return (true);
}

void TabbedRegion::update()
{
    if (!active_is_valid())
        return;

    m_tabs[m_active].reg->update();
}

void TabbedRegion::clear()
{
    for (size_t i = 0; i < m_tabs.size(); ++i)
    {
        if (m_tabs[i].reg)
            m_tabs[i].reg->clear();
    }
}

void TabbedRegion::pack_buffers()
{
    m_buf_gui.clear();

    for (int i = 0; i < (int)m_tabs.size(); ++i)
    {
        int ofs;
        if (m_active == i)
            ofs = TAB_OFS_SELECTED;
        else if (m_mouse_tab == i)
            ofs = TAB_OFS_MOUSEOVER;
        else
            ofs = TAB_OFS_UNSELECTED;

        int tileidx = m_tabs[i].tile_tab + ofs;
        const tile_info &inf = tile_gui_info(tileidx);
        int offset_y = m_tabs[i].min_y;
        m_buf_gui.add(tileidx, 0, 0, -inf.width, offset_y, false);
    }
}

void TabbedRegion::render()
{
    if (crawl_state.game_is_arena())
        return;

    if (!active_is_valid())
        return;

    if (m_dirty)
    {
        pack_buffers();
        m_dirty = false;
    }

#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering TabbedRegion\n");
#endif
    set_transform();
    m_buf_gui.draw(NULL, NULL);
    m_tabs[m_active].reg->render();

    draw_tag();
}

void TabbedRegion::draw_tag()
{
    if (m_mouse_tab == -1)
        return;

    GridRegion *tab = m_tabs[m_mouse_tab].reg;
    if (!tab)
        return;

    draw_desc(tab->name().c_str());
}

void TabbedRegion::on_resize()
{
    int reg_sx = sx + ox;
    int reg_sy = sy;

    for (size_t i = 0; i < m_tabs.size(); ++i)
    {
        if (!m_tabs[i].reg)
            continue;

        m_tabs[i].reg->place(reg_sx, reg_sy);
        m_tabs[i].reg->resize(mx, my);
    }
}

int TabbedRegion::get_mouseover_tab(MouseEvent &event) const
{
    int x = event.px - sx;
    int y = event.py - sy;

    if (x < 0 || x > ox || y < 0 || y > wy)
        return (-1);

    for (int i = 0; i < (int)m_tabs.size(); ++i)
    {
        if (y >= m_tabs[i].min_y && y <= m_tabs[i].max_y)
            return (i);
    }
    return (-1);
}

int TabbedRegion::handle_mouse(MouseEvent &event)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return (0);

    int mouse_tab = get_mouseover_tab(event);
    if (mouse_tab != m_mouse_tab)
    {
        m_mouse_tab = mouse_tab;
        m_dirty = true;
        tiles.set_need_redraw();
    }

    if (m_mouse_tab != -1)
    {
        if (event.event == MouseEvent::PRESS)
        {
            activate_tab(m_mouse_tab);
            return (0);
        }
    }

    // Otherwise, pass input to the active tab.
    if (!active_is_valid())
        return (0);

    return (get_tab_region(active_tab())->handle_mouse(event));
}

bool TabbedRegion::update_tab_tip_text(std::string &tip, bool active)
{
    return (false);
}

bool TabbedRegion::update_tip_text(std::string &tip)
{
    if (!active_is_valid())
        return (false);

    if (m_mouse_tab != -1)
    {
        GridRegion *reg = m_tabs[m_mouse_tab].reg;
        if (reg)
            return (reg->update_tab_tip_text(tip, m_mouse_tab == m_active));
    }

    return (get_tab_region(active_tab())->update_tip_text(tip));
}

bool TabbedRegion::update_alt_text(std::string &alt)
{
    if (!active_is_valid())
        return (false);

    return (get_tab_region(active_tab())->update_alt_text(alt));
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
    m_colours[MF_MONS_FRIENDLY] = (map_colour)Options.tile_friendly_col;
    m_colours[MF_MONS_PEACEFUL] = (map_colour)Options.tile_peaceful_col;
    m_colours[MF_MONS_NEUTRAL]  = (map_colour)Options.tile_neutral_col;
    m_colours[MF_MONS_HOSTILE]  = (map_colour)Options.tile_monster_col;
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
    m_buf_map.draw(NULL, NULL);
    m_buf_lines.draw(NULL, NULL);
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

    const int x = event.px - sx;
    const int y = event.py - sy;
    if (x < 0 || x > wx || y < 0 || y > wy)
        return (0);

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
            if (event.mod & MOD_SHIFT)
            {
                // Start autotravel, or give an appropriate message.
                do_explore_cmd();
                return (CK_MOUSE_CMD);
            }
            else
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

    if (!player_in_mappable_area())
        return (false);

    tip = "[L-Click] Travel / [R-Click] View";
    if (you.level_type != LEVEL_LABYRINTH
        && (you.hunger_state > HS_STARVING || you_min_hunger())
        && i_feel_safe())
    {
        tip += "\n[Shift-L-Click] Autoexplore";
    }
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

TextRegion::TextRegion(FontWrapper *font) :
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

StatRegion::StatRegion(FontWrapper *font) : TextRegion(font)
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
    return command_to_key(CMD_REST);
}

bool StatRegion::update_tip_text(std::string& tip)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return (false);

    tip = "[L-Click] Rest / Search for a while";
    return (true);
}

MessageRegion::MessageRegion(FontWrapper *font) : TextRegion(font), m_overlay(false)
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

    return command_to_key(CMD_REPLAY_MESSAGES);
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

            glmanager->set_transform();

            ShapeBuffer buff;
            VColour col(100, 100, 100, 100);
            buff.add(sx, sy, ex, sy + height, col);
            buff.draw(NULL, NULL);
        }
    }

    m_font->render_textblock(sx + ox, sy + oy, cbuf, abuf, mx, my, m_overlay);

    if (idx >= 0)
    {
        cbuf[idx] = char_back;
        abuf[idx] = col_back;
    }
}

CRTRegion::CRTRegion(FontWrapper *font) : TextRegion(font), m_attached_menu(NULL)
{

}

CRTRegion::~CRTRegion()
{
    clear();
}

int CRTRegion::handle_mouse(MouseEvent &event)
{
    int ret_val = 0;
    if (m_attached_menu == NULL)
    {
        if (event.event == MouseEvent::PRESS
            && event.button == MouseEvent::LEFT)
        {
            ret_val = CK_MOUSE_CLICK;
        }
    }
    else
    {
        ret_val = m_attached_menu->handle_mouse(event);
    }
    return ret_val;
}

void CRTRegion::on_resize()
{
    TextRegion::on_resize();
    crawl_view.termsz.x = mx;
    crawl_view.termsz.y = my;

    // Todo: resize attached menu
}

void CRTRegion::clear()
{
    // clear all the texts
    TextRegion::clear();
    detach_menu();
}

void CRTRegion::render()
{
    set_transform();

    // render all the inherited texts
    TextRegion::render();

    // render the attached menu if it exists
    if (m_attached_menu != NULL)
    {
        m_attached_menu->draw_menu();
    }
}

void CRTRegion::attach_menu(PrecisionMenu* menu)
{
    detach_menu();

    m_attached_menu = menu;
}

void CRTRegion::detach_menu()
{
    // Tiles has no rights over the menu, thus the user must delete it
    // Via other means
    m_attached_menu = NULL;
}

<<<<<<< HEAD
MenuRegion::MenuRegion(ImageManager *im, FontWrapper *entry) :
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
    m_shape_buf.draw(NULL, NULL);
    m_line_buf.draw(NULL, NULL);
    for (int i = 0; i < TEX_MAX; i++)
        m_tile_buf[i].draw(NULL, NULL);
    m_font_buf.draw(NULL, NULL);
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

        // Quiet valgrind warning about unitialized memory.
        for (int i = idx + 1; i < new_size; i++)
            m_entries[i].valid = false;
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

TitleRegion::TitleRegion(int width, int height, FontWrapper* font) :
  m_buf(&m_img, GLW_QUADS), m_font_buf(font)
{
    sx = sy = 0;
    dx = dy = 1;

    if (!m_img.load_texture("title.png", MIPMAP_NONE))
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
    m_buf.draw(NULL, NULL);
    m_font_buf.draw(NULL, NULL);
}

void TitleRegion::run()
{
    mouse_control mc(MOUSE_MODE_MORE);
    getchm();
}

/**
 * We only want to show one line of message by default so clear the
 * font buffer before adding the new message.
 */
void TitleRegion::update_message(std::string message)
{
    m_font_buf.clear();
    m_font_buf.add(message, VColour::white, 0, 0);
}

DollEditRegion::DollEditRegion(ImageManager *im, FontWrapper *font) :
    m_font_buf(font),
    m_tile_buf(&im->m_textures[TEX_PLAYER], TILEP_MASK_SUBMERGED, 18, 16,
               Options.tile_better_transparency),
    m_cur_buf(&im->m_textures[TEX_PLAYER], TILEP_MASK_SUBMERGED, 18, 16,
              Options.tile_better_transparency)
{
    sx = sy = 0;
    dx = dy = 32;
    mx = my = 1;

    m_font = font;

    m_doll_idx = 0;
    m_cat_idx = TILEP_PART_BASE;
    m_copy_valid = false;
}

void DollEditRegion::clear()
{
    m_shape_buf.clear();
    m_tile_buf.clear();
    m_cur_buf.clear();
    m_font_buf.clear();
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
        max_part = tile_player_count(offset);
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
    const int left_gutter    = 2;
    const int item_line      = 2;
    const int edit_doll_line = 5;
    const int doll_line      = 8;
    const int info_offset =
        left_gutter + std::max(max_show, (int)NUM_MAX_DOLLS) + 1;

    const int center_x = left_gutter + max_show / 2;

    // Pack current doll separately so it can be drawn repeatedly.
    {
        dolls_data temp = m_dolls[m_doll_idx];
        _fill_doll_equipment(temp);
        pack_doll_buf(m_cur_buf, temp, 0, 0, false, false);
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
        pack_doll_buf(m_tile_buf, temp, x, y, false, false);

        m_shape_buf.add(x, y, x + 1, y + 1, grey);
    }

    // Draw current category of parts.
    int max_part = tile_player_part_count[m_cat_idx];
    if (m_cat_idx == TILEP_PART_BASE)
        max_part = tile_player_count(tilep_species_to_base_tile()) - 1;

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
    m_shape_buf.draw(NULL, NULL);
    m_tile_buf.draw();

    {
        GLW_3VF trans(32 * left_gutter, 32 * edit_doll_line, 0);
        GLW_3VF scale(64, 64, 1);
        glmanager->set_transform(&trans, &scale);
    }

    m_cur_buf.draw();

    {
        dolls_data temp;
        temp = m_job_default;
        _fill_doll_equipment(temp);
        pack_doll_buf(m_cur_buf, temp, 2, 0, false, false);

        for (unsigned int i = 0; i < TILEP_PART_MAX; ++i)
            temp.parts[i] = TILEP_SHOW_EQUIP;
        _fill_doll_equipment(temp);
        pack_doll_buf(m_cur_buf, temp, 4, 0, false, false);

        if (m_mode == TILEP_MODE_LOADING)
            m_cur_buf.add(TILEP_CURSOR, 0, 0);
        else if (m_mode == TILEP_MODE_DEFAULT)
            m_cur_buf.add(TILEP_CURSOR, 2, 0);
        else if (m_mode == TILEP_MODE_EQUIP)
            m_cur_buf.add(TILEP_CURSOR, 4, 0);
    }

    {
        GLW_3VF trans(32 * (left_gutter + 3), 32 * edit_doll_line, 0);
        GLW_3VF scale(32, 32, 1);
        glmanager->set_transform(&trans, &scale);
    }

    m_cur_buf.draw();

    // Add text.
    const char *part_name = "(none)";
    if (m_part_idx == TILEP_SHOW_EQUIP)
        part_name = "(show equip)";
    else if (m_part_idx)
        part_name = tile_player_name(m_part_idx);

    glmanager->reset_transform();

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
        const int width  = m_font->char_width();
        const float start_y = doll_name_y + height * 3;
        const float start_x = width * 6;
        m_font_buf.add("Change parts       left/right              Confirm choice      Enter",  VColour::white, start_x, start_y);
        m_font_buf.add("Change category    up/down                 Copy doll           Ctrl-C", VColour::white, start_x, start_y + height * 1);
        m_font_buf.add("Change doll        0-9, Shift + arrows     Paste copied doll   Ctrl-V", VColour::white, start_x, start_y + height * 2);
        m_font_buf.add("Change doll mode   m                       Randomise doll      Ctrl-R", VColour::white, start_x, start_y + height * 3);
        m_font_buf.add("Save menu          Escape, Ctrl-S          Toggle equipment    *",      VColour::white, start_x, start_y + height * 4);
        m_font_buf.add("Quit menu          q, Ctrl-Q",                                          VColour::white, start_x, start_y + height * 5);
    }

    m_font_buf.draw(NULL, NULL);
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

    if (!_load_doll_data("dolls.txt", m_dolls, NUM_MAX_DOLLS,
                         &m_mode, &m_doll_idx))
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
        case CMD_DOLL_QUIT:
            return;
        case CMD_DOLL_RANDOMIZE:
            _create_random_doll(m_dolls[m_doll_idx]);
            break;
        case CMD_DOLL_SELECT_NEXT_DOLL:
        case CMD_DOLL_SELECT_PREV_DOLL:
        {
            const int bonus = (cmd == CMD_DOLL_SELECT_NEXT_DOLL ? 1
                                        : NUM_MAX_DOLLS - 1);

            m_doll_idx = (m_doll_idx + bonus) % NUM_MAX_DOLLS;
            update_part_idx = true;
            if (m_mode != TILEP_MODE_LOADING)
                m_mode = TILEP_MODE_LOADING;
            break;
        }
        case CMD_DOLL_SELECT_NEXT_PART:
        case CMD_DOLL_SELECT_PREV_PART:
        {
            const int bonus = (cmd == CMD_DOLL_SELECT_NEXT_PART ? 1
                                        : TILEP_PART_MAX - 1);

            m_cat_idx = (m_cat_idx + bonus) % TILEP_PART_MAX;
            update_part_idx = true;
            break;
        }
        case CMD_DOLL_CHANGE_PART_NEXT:
        case CMD_DOLL_CHANGE_PART_PREV:
            if (m_part_idx != TILEP_SHOW_EQUIP)
            {
                const int dir = (cmd == CMD_DOLL_CHANGE_PART_NEXT ? 1 : -1);
                m_part_idx = _get_next_part(m_cat_idx, m_part_idx, dir);
            }
            m_dolls[m_doll_idx].parts[m_cat_idx] = m_part_idx;
            break;
        case CMD_DOLL_TOGGLE_EQUIP:
            if (m_dolls[m_doll_idx].parts[m_cat_idx] == TILEP_SHOW_EQUIP)
                m_dolls[m_doll_idx].parts[m_cat_idx] = m_part_idx;
            else
                m_dolls[m_doll_idx].parts[m_cat_idx] = TILEP_SHOW_EQUIP;
            break;
        case CMD_DOLL_CONFIRM_CHOICE:
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
        case CMD_DOLL_TOGGLE_EQUIP_ALL:
            for (int i = 0; i < TILEP_PART_MAX; i++)
                m_dolls[m_doll_idx].parts[i] = TILEP_SHOW_EQUIP;
            break;
        case CMD_DOLL_JOB_DEFAULT:
            m_dolls[m_doll_idx] = m_job_default;
            break;
        case CMD_DOLL_CHANGE_MODE:
            m_mode = (tile_doll_mode)(((int)m_mode + 1) % TILEP_MODE_MAX);
        default:
            if (key == '0')
                m_doll_idx = 0;
            else if (key >= '1' && key <= '9')
                m_doll_idx = key - '1' + 1;
            else
                break;

            if (m_mode != TILEP_MODE_LOADING)
                m_mode = TILEP_MODE_LOADING;
            ASSERT(m_doll_idx < NUM_MAX_DOLLS);
            break;
        }
    }
    while (cmd != CMD_DOLL_SAVE);

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

const char *ImageManager::filenames[TEX_MAX] =
{
    "dngn.png",
    "player.png",
    "main.png",
    "gui.png"
};

ImageManager::ImageManager()
{
}

ImageManager::~ImageManager()
{
    unload_textures();
}

bool ImageManager::load_textures(bool need_mips)
{
    MipMapOptions mip = need_mips ?
        MIPMAP_CREATE : MIPMAP_NONE;
    if (!m_textures[TEX_DUNGEON].load_texture(filenames[TEX_DUNGEON], mip))
        return (false);

    if (!m_textures[TEX_PLAYER].load_texture(filenames[TEX_PLAYER], mip))
        return (false);

    if (!m_textures[TEX_GUI].load_texture(filenames[TEX_GUI], mip))
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
    MipMapOptions mip = MIPMAP_CREATE;
    const char *fname = filenames[TEX_DEFAULT];
    bool success = m_textures[TEX_DEFAULT].load_texture(fname, mip,
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
