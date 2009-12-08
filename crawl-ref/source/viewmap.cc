/*
 * File:     viewmap.cc
 * Summary:  Showing the level map (X and background).
 */

#include "AppHdr.h"

#include "viewmap.h"

#include "branch.h"
#include "cio.h"
#include "colour.h"
#include "command.h"
#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "map_knowledge.h"
#include "fprop.h"
#include "exclude.h"
#include "feature.h"
#include "files.h"
#include "format.h"
#include "macro.h"
#include "mon-util.h"
#include "options.h"
#include "place.h"
#include "player.h"
#include "showsymb.h"
#include "stash.h"
#include "stuff.h"
#include "terrain.h"
#include "travel.h"
#include "viewchar.h"
#include "viewgeom.h"

unsigned get_sightmap_char(dungeon_feature_type feat)
{
    return (get_feature_def(feat).symbol);
}

unsigned get_magicmap_char(dungeon_feature_type feat)
{
    return (get_feature_def(feat).magic_symbol);
}

// Determines if the given feature is present at (x, y) in _feat_ coordinates.
// If you have map coords, add (1, 1) to get grid coords.
// Use one of
// 1. '<' and '>' to look for stairs
// 2. '\t' or '\\' for shops, portals.
// 3. '^' for traps
// 4. '_' for altars
// 5. Anything else will look for the exact same character in the level map.
bool is_feature(int feature, const coord_def& where)
{
    if (!env.map_knowledge(where).object && !you.see_cell(where))
        return (false);

    dungeon_feature_type grid = grd(where);

    switch (feature)
    {
    case 'E':
        return (travel_point_distance[where.x][where.y] == PD_EXCLUDED);
    case 'F':
    case 'W':
        return is_waypoint(where);
    case 'I':
        return is_stash(where.x, where.y);
    case '_':
        switch (grid)
        {
        case DNGN_ALTAR_ZIN:
        case DNGN_ALTAR_SHINING_ONE:
        case DNGN_ALTAR_KIKUBAAQUDGHA:
        case DNGN_ALTAR_YREDELEMNUL:
        case DNGN_ALTAR_XOM:
        case DNGN_ALTAR_VEHUMET:
        case DNGN_ALTAR_OKAWARU:
        case DNGN_ALTAR_MAKHLEB:
        case DNGN_ALTAR_SIF_MUNA:
        case DNGN_ALTAR_TROG:
        case DNGN_ALTAR_NEMELEX_XOBEH:
        case DNGN_ALTAR_ELYVILON:
        case DNGN_ALTAR_LUGONU:
        case DNGN_ALTAR_BEOGH:
        case DNGN_ALTAR_JIYVA:
        case DNGN_ALTAR_FEDHAS:
        case DNGN_ALTAR_CHEIBRIADOS:
            return (true);
        default:
            return (false);
        }
    case '\t':
    case '\\':
        switch (grid)
        {
        case DNGN_ENTER_HELL:
        case DNGN_EXIT_HELL:
        case DNGN_ENTER_LABYRINTH:
        case DNGN_ENTER_PORTAL_VAULT:
        case DNGN_EXIT_PORTAL_VAULT:
        case DNGN_ENTER_SHOP:
        case DNGN_ENTER_DIS:
        case DNGN_ENTER_GEHENNA:
        case DNGN_ENTER_COCYTUS:
        case DNGN_ENTER_TARTARUS:
        case DNGN_ENTER_ABYSS:
        case DNGN_EXIT_ABYSS:
        case DNGN_ENTER_PANDEMONIUM:
        case DNGN_EXIT_PANDEMONIUM:
        case DNGN_TRANSIT_PANDEMONIUM:
        case DNGN_ENTER_ZOT:
        case DNGN_RETURN_FROM_ZOT:
            return (true);
        default:
            return (false);
        }
    case '<':
        switch (grid)
        {
        case DNGN_ESCAPE_HATCH_UP:
        case DNGN_STONE_STAIRS_UP_I:
        case DNGN_STONE_STAIRS_UP_II:
        case DNGN_STONE_STAIRS_UP_III:
        case DNGN_RETURN_FROM_ORCISH_MINES:
        case DNGN_RETURN_FROM_HIVE:
        case DNGN_RETURN_FROM_LAIR:
        case DNGN_RETURN_FROM_SLIME_PITS:
        case DNGN_RETURN_FROM_VAULTS:
        case DNGN_RETURN_FROM_CRYPT:
        case DNGN_RETURN_FROM_HALL_OF_BLADES:
        case DNGN_RETURN_FROM_TEMPLE:
        case DNGN_RETURN_FROM_SNAKE_PIT:
        case DNGN_RETURN_FROM_ELVEN_HALLS:
        case DNGN_RETURN_FROM_TOMB:
        case DNGN_RETURN_FROM_SWAMP:
        case DNGN_RETURN_FROM_SHOALS:
        case DNGN_EXIT_PORTAL_VAULT:
            return (true);
        default:
            return (false);
        }
    case '>':
        switch (grid)
        {
        case DNGN_ESCAPE_HATCH_DOWN:
        case DNGN_STONE_STAIRS_DOWN_I:
        case DNGN_STONE_STAIRS_DOWN_II:
        case DNGN_STONE_STAIRS_DOWN_III:
        case DNGN_ENTER_ORCISH_MINES:
        case DNGN_ENTER_HIVE:
        case DNGN_ENTER_LAIR:
        case DNGN_ENTER_SLIME_PITS:
        case DNGN_ENTER_VAULTS:
        case DNGN_ENTER_CRYPT:
        case DNGN_ENTER_HALL_OF_BLADES:
        case DNGN_ENTER_TEMPLE:
        case DNGN_ENTER_SNAKE_PIT:
        case DNGN_ENTER_ELVEN_HALLS:
        case DNGN_ENTER_TOMB:
        case DNGN_ENTER_SWAMP:
        case DNGN_ENTER_SHOALS:
            return (true);
        default:
            return (false);
        }
    case '^':
        switch (grid)
        {
        case DNGN_TRAP_MECHANICAL:
        case DNGN_TRAP_MAGICAL:
        case DNGN_TRAP_NATURAL:
            return (true);
        default:
            return (false);
        }
    default:
        return get_map_knowledge_char(where.x, where.y) == (unsigned) feature;
    }
}

static bool _is_feature_fudged(int feature, const coord_def& where)
{
    if (!env.map_knowledge(where).object)
        return (false);

    if (is_feature(feature, where))
        return (true);

    // 'grid' can fit in an unsigned char, but making this a short shuts up
    // warnings about out-of-range case values.
    short grid = grd(where);

    if (feature == '<')
    {
        switch (grid)
        {
        case DNGN_EXIT_HELL:
        case DNGN_EXIT_PORTAL_VAULT:
        case DNGN_EXIT_ABYSS:
        case DNGN_EXIT_PANDEMONIUM:
        case DNGN_RETURN_FROM_ZOT:
            return (true);
        default:
            return (false);
        }
    }
    else if (feature == '>')
    {
        switch (grid)
        {
        case DNGN_ENTER_DIS:
        case DNGN_ENTER_GEHENNA:
        case DNGN_ENTER_COCYTUS:
        case DNGN_ENTER_TARTARUS:
        case DNGN_TRANSIT_PANDEMONIUM:
        case DNGN_ENTER_ZOT:
            return (true);
        default:
            return (false);
        }
    }

    return (false);
}

static int _find_feature(int feature, int curs_x, int curs_y,
                         int start_x, int start_y, int anchor_x, int anchor_y,
                         int ignore_count, int *move_x, int *move_y)
{
    int cx = anchor_x,
        cy = anchor_y;

    int firstx = -1, firsty = -1;
    int matchcount = 0;

    // Find the first occurrence of feature 'feature', spiralling around (x,y)
    int maxradius = GXM > GYM ? GXM : GYM;
    for (int radius = 1; radius < maxradius; ++radius)
        for (int axis = -2; axis < 2; ++axis)
        {
            int rad = radius - (axis < 0);
            for (int var = -rad; var <= rad; ++var)
            {
                int dx = radius, dy = var;
                if (axis % 2)
                    dx = -dx;
                if (axis < 0)
                {
                    int temp = dx;
                    dx = dy;
                    dy = temp;
                }

                int x = cx + dx, y = cy + dy;
                if (!in_bounds(x, y))
                    continue;
                if (_is_feature_fudged(feature, coord_def(x, y)))
                {
                    ++matchcount;
                    if (!ignore_count--)
                    {
                        // We want to cursor to (x,y)
                        *move_x = x - (start_x + curs_x - 1);
                        *move_y = y - (start_y + curs_y - 1);
                        return matchcount;
                    }
                    else if (firstx == -1)
                    {
                        firstx = x;
                        firsty = y;
                    }
                }
            }
        }

    // We found something, but ignored it because of an ignorecount
    if (firstx != -1)
    {
        *move_x = firstx - (start_x + curs_x - 1);
        *move_y = firsty - (start_y + curs_y - 1);
        return 1;
    }
    return 0;
}

void find_features(const std::vector<coord_def>& features,
                   unsigned char feature, std::vector<coord_def> *found)
{
    for (unsigned feat = 0; feat < features.size(); ++feat)
    {
        const coord_def& coord = features[feat];
        if (is_feature(feature, coord))
            found->push_back(coord);
    }
}

static int _find_feature( const std::vector<coord_def>& features,
                          int feature, int curs_x, int curs_y,
                          int start_x, int start_y,
                          int ignore_count,
                          int *move_x, int *move_y,
                          bool forward)
{
    int firstx = -1, firsty = -1, firstmatch = -1;
    int matchcount = 0;

    for (unsigned feat = 0; feat < features.size(); ++feat)
    {
        const coord_def& coord = features[feat];

        if (_is_feature_fudged(feature, coord))
        {
            ++matchcount;
            if (forward? !ignore_count-- : --ignore_count == 1)
            {
                // We want to cursor to (x,y)
                *move_x = coord.x - (start_x + curs_x - 1);
                *move_y = coord.y - (start_y + curs_y - 1);
                return matchcount;
            }
            else if (!forward || firstx == -1)
            {
                firstx = coord.x;
                firsty = coord.y;
                firstmatch = matchcount;
            }
        }
    }

    // We found something, but ignored it because of an ignorecount
    if (firstx != -1)
    {
        *move_x = firstx - (start_x + curs_x - 1);
        *move_y = firsty - (start_y + curs_y - 1);
        return firstmatch;
    }
    return 0;
}

static int _get_number_of_lines_levelmap()
{
    return get_number_of_lines() - (Options.level_map_title ? 1 : 0);
}

#ifndef USE_TILE
static void _draw_level_map(int start_x, int start_y, bool travel_mode,
        bool on_level)
{
    int bufcount2 = 0;
    screen_buffer_t buffer2[GYM * GXM * 2];

    const int num_lines = std::min(_get_number_of_lines_levelmap(), GYM);
    const int num_cols  = std::min(get_number_of_cols(),            GXM);

    cursor_control cs(false);

    int top = 1 + Options.level_map_title;
    cgotoxy(1, top);
    for (int screen_y = 0; screen_y < num_lines; screen_y++)
        for (int screen_x = 0; screen_x < num_cols; screen_x++)
        {
            coord_def c(start_x + screen_x, start_y + screen_y);

            if (!map_bounds(c))
            {
                buffer2[bufcount2 + 1] = DARKGREY;
                buffer2[bufcount2] = 0;
            }
            else
            {
                buffer2[bufcount2] = env.map_knowledge(c).glyph();
                buffer2[bufcount2 + 1] = real_colour(get_map_col(c, travel_mode));

                if (c == you.pos() && !crawl_state.arena_suspended && on_level)
                {
                    // [dshaligram] Draw the @ symbol on the level-map. It's no
                    // longer saved into the env.map_knowledge, so we need to draw it
                    // directly.
                    buffer2[bufcount2 + 1] = WHITE;
                    buffer2[bufcount2]     = you.symbol;
                }

                // If we've a waypoint on the current square, *and* the
                // square is a normal floor square with nothing on it,
                // show the waypoint number.
                if (Options.show_waypoints)
                {
                    // XXX: This is a horrible hack.
                    screen_buffer_t &bc = buffer2[bufcount2];
                    unsigned char ch = is_waypoint(c);
                    if (ch && (bc == get_sightmap_char(DNGN_FLOOR)
                               || bc == get_magicmap_char(DNGN_FLOOR)))
                    {
                        bc = ch;
                    }
                }
            }

            bufcount2 += 2;
        }

    puttext(1, top, num_cols, top + num_lines - 1, buffer2);
}
#endif // USE_TILE

static void _reset_travel_colours(std::vector<coord_def> &features,
        bool on_level)
{
    // We now need to redo travel colours.
    features.clear();

    if (on_level)
    {
        find_travel_pos((on_level ? you.pos() : coord_def()),
                NULL, NULL, &features);
    }
    else
    {
        travel_pathfind tp;
        tp.set_feature_vector(&features);
        tp.get_features();
    }

    // Sort features into the order the player is likely to prefer.
    arrange_features(features);
}

class feature_list
{
    enum group
    {
        G_UP, G_DOWN, G_PORTAL, G_OTHER, G_NONE, NUM_GROUPS = G_NONE
    };

    std::vector<glyph> data[NUM_GROUPS];

    glyph get_glyph(const coord_def& gc)
    {
        // XXX: it's unclear whether we want to display all features
        // or just those not obscured by remembered/detected stuff.
        dungeon_feature_type feat = env.map_knowledge(gc).feat();
        const bool terrain_seen = is_terrain_seen(gc);
        const feature_def &fdef = get_feature_def(feat);
        glyph g;
        g.ch  = terrain_seen ? fdef.symbol : fdef.magic_symbol;
        g.col = get_map_col(gc);
        return (g);
    }

    static group feat_dir(dungeon_feature_type feat)
    {
        switch (feat_stair_direction(feat))
        {
        case CMD_GO_UPSTAIRS:
            return G_UP;
        case CMD_GO_DOWNSTAIRS:
            return G_DOWN;
        default:
            return G_NONE;
        }
    }

    group get_group(const coord_def& gc)
    {
        show_type obj = env.map_knowledge(gc).object;
        if (obj != SH_FEATURE)
            return G_NONE;

        dungeon_feature_type feat = obj.feat;

        if (feat_is_staircase(feat) || feat_is_escape_hatch(feat))
            return feat_dir(feat);
        if (feat == DNGN_TRAP_NATURAL)
            return G_DOWN;
        if (feat_is_altar(feat) || feat == DNGN_ENTER_SHOP)
            return G_OTHER; 
        if (get_feature_dchar(feat) == DCHAR_ARCH)
            return G_PORTAL;
        return G_NONE;
    }

    void maybe_add(const coord_def& gc)
    {
#ifndef USE_TILE
        if (!is_terrain_known(gc))
            return;

        group grp = get_group(gc);
        if (grp != G_NONE)
            data[grp].push_back(get_glyph(gc));
#endif
    }

public:
    void init()
    {
        for (unsigned int i = 0; i < NUM_GROUPS; ++i)
            data[i].clear();
        for (rectangle_iterator ri(0); ri; ++ri)
            maybe_add(*ri);
    }

    formatted_string format() const
    {
        formatted_string s;
        for (unsigned int i = 0; i < NUM_GROUPS; ++i)
            for (unsigned int j = 0; j < data[i].size(); ++j)
                s.add_glyph(data[i][j]);
        return s;
    }
};

#ifndef USE_TILE
static void _draw_title(const coord_def& cpos, const feature_list& feats)
{
    if (!Options.level_map_title)
        return;

    const formatted_string help =
        formatted_string::parse_string("(Press <w>?</w> for help)");
    const int helplen = std::string(help).length();

    std::string pstr = "";
#ifdef WIZARD
    if (you.wizard)
    {
        char buf[10];
        snprintf(buf, sizeof(buf), " (%d, %d)", cpos.x, cpos.y);
        pstr = std::string(buf);
    }
#endif // WIZARD

    cgotoxy(1, 1);
    textcolor(WHITE);

    cprintf("%-*s",
            get_number_of_cols() - helplen,
            (upcase_first(place_name(
                    get_packed_place(), true, true)) + pstr).c_str());

    formatted_string s = feats.format();
    cgotoxy((get_number_of_cols() - s.length()) / 2, 1);
    s.display();

    textcolor(LIGHTGREY);
    cgotoxy(get_number_of_cols() - helplen + 1, 1);
    help.display();
}
#endif

class levelview_excursion : public level_excursion
{
public:
    void go_to(const level_id& next)
    {
#ifdef USE_TILE
        tiles.clear_minimap();
        level_excursion::go_to(next);
        TileNewLevel(false);
#else
        level_excursion::go_to(next);
#endif
    }
};

// show_map() now centers the known map along x or y.  This prevents
// the player from getting "artificial" location clues by using the
// map to see how close to the end they are.  They'll need to explore
// to get that.  This function is still a mess, though. -- bwr
void show_map( level_pos &spec_place, bool travel_mode, bool allow_esc )
{
    levelview_excursion le;
    level_id original(level_id::current());

    cursor_control ccon(!Options.use_fake_cursor);
    int i, j;

    int move_x = 0, move_y = 0, scroll_y = 0;

    bool new_level = true;

    // Vector to track all features we can travel to, in order of distance.
    std::vector<coord_def> features;
    // List of all interesting features for display in the (console) title.
    feature_list feats;

    int min_x = INT_MAX, max_x = INT_MIN, min_y = INT_MAX, max_y = INT_MIN;
    const int num_lines   = _get_number_of_lines_levelmap();
    const int half_screen = (num_lines - 1) / 2;

    const int top = 1 + Options.level_map_title;

    int map_lines = 0;

    int start_x = -1;                                         // no x scrolling
    const int block_step = Options.level_map_cursor_step;
    int start_y;                                              // y does scroll

    int screen_y = -1;

    int curs_x = -1, curs_y = -1;
    int search_found = 0, anchor_x = -1, anchor_y = -1;

    bool map_alive  = true;
    bool redraw_map = true;

#ifndef USE_TILE
    clrscr();
#endif
    textcolor(DARKGREY);

    bool on_level = false;

    while (map_alive)
    {
        if (new_level)
        {
            on_level = (level_id::current() == original);

            move_x = 0, move_y = 0, scroll_y = 0;

            // Vector to track all features we can travel to, in order of distance.
            if (travel_mode)
            {
                travel_init_new_level();
                travel_cache.update();

                _reset_travel_colours(features, on_level);
            }
            feats.init();

            min_x = GXM, max_x = 0, min_y = 0, max_y = 0;
            bool found_y = false;

            for (j = 0; j < GYM; j++)
                for (i = 0; i < GXM; i++)
                {
                    if (env.map_knowledge[i][j].known())
                    {
                        if (!found_y)
                        {
                            found_y = true;
                            min_y = j;
                        }

                        max_y = j;

                        if (i < min_x)
                            min_x = i;

                        if (i > max_x)
                            max_x = i;
                    }
                }

            map_lines = max_y - min_y + 1;

            start_x = min_x + (max_x - min_x + 1) / 2 - 40;           // no x scrolling
            start_y = 0;                                              // y does scroll

            coord_def reg;

            if (on_level)
            {
                reg = you.pos();
            }
            else
            {
                reg.y = min_y + (max_y - min_y + 1) / 2;
                reg.x = min_x + (max_x - min_x + 1) / 2;
            }

            screen_y = reg.y;

            // If close to top of known map, put min_y on top
            // else if close to bottom of known map, put max_y on bottom.
            //
            // The num_lines comparisons are done to keep things neat, by
            // keeping things at the top of the screen.  By shifting an
            // additional one in the num_lines > map_lines case, we can
            // keep the top line clear... which makes things look a whole
            // lot better for small maps.
            if (num_lines > map_lines)
                screen_y = min_y + half_screen - 1;
            else if (num_lines == map_lines || screen_y - half_screen < min_y)
                screen_y = min_y + half_screen;
            else if (screen_y + half_screen > max_y)
                screen_y = max_y - half_screen;

            curs_x = reg.x - start_x + 1;
            curs_y = reg.y - screen_y + half_screen + 1;
            search_found = 0, anchor_x = -1, anchor_y = -1;

            redraw_map = true;
            new_level = false;
        }

#if defined(USE_UNIX_SIGNALS) && defined(SIGHUP_SAVE) && defined(USE_CURSES)
        // If we've received a HUP signal then the user can't choose a
        // location, so indicate this by returning an invalid position.
        if (crawl_state.seen_hups)
        {
            spec_place = level_pos();
            break;
        }
#endif

        start_y = screen_y - half_screen;

        move_x = move_y = 0;

        if (redraw_map)
        {
            coord_def cen(start_x + curs_x - 1, start_y + curs_y - 1);
#ifdef USE_TILE
            // Note: Tile versions just center on the current cursor
            // location.  It silently ignores everything else going
            // on in this function.  --Enne
            tiles.load_dungeon(cen);
#else
            _draw_title(cen, feats);
            _draw_level_map(start_x, start_y, travel_mode, on_level);
#endif // USE_TILE
        }
#ifndef USE_TILE
        cursorxy(curs_x, curs_y + top - 1);
#endif
        redraw_map = true;

        c_input_reset(true);
        int key = unmangle_direction_keys(getchm(KMC_LEVELMAP), KMC_LEVELMAP,
                                          false, false);
        command_type cmd = key_to_command(key, KMC_LEVELMAP);
        if (cmd < CMD_MIN_OVERMAP || cmd > CMD_MAX_OVERMAP)
            cmd = CMD_NO_CMD;

        if (key == CK_MOUSE_CLICK)
        {
            const c_mouse_event cme = get_mouse_event();
            const coord_def curp(start_x + curs_x - 1, start_y + curs_y - 1);
            const coord_def grdp =
                cme.pos + coord_def(start_x - 1, start_y - top);

            if (cme.left_clicked() && in_bounds(grdp))
            {
                spec_place = level_pos(level_id::current(), grdp);
                map_alive  = false;
            }
            else if (cme.scroll_up())
                scroll_y = -block_step;
            else if (cme.scroll_down())
                scroll_y = block_step;
            else if (cme.right_clicked())
            {
                const coord_def delta = grdp - curp;
                move_y = delta.y;
                move_x = delta.x;
            }
        }

        c_input_reset(false);

        switch (cmd)
        {
        case CMD_MAP_HELP:
            show_levelmap_help();
            break;

        case CMD_MAP_CLEAR_MAP:
            clear_map();
            break;

        case CMD_MAP_FORGET:
            forget_map(100, true);
            break;

        case CMD_MAP_ADD_WAYPOINT:
            travel_cache.add_waypoint(start_x + curs_x - 1,
                                      start_y + curs_y - 1);
            // We need to do this all over again so that the user can jump
            // to the waypoint he just created.
            _reset_travel_colours(features, on_level);
            feats.init();
            break;

        // Cycle the radius of an exclude.
        case CMD_MAP_EXCLUDE_AREA:
        {
            const coord_def p(start_x + curs_x - 1, start_y + curs_y - 1);
            cycle_exclude_radius(p);

            _reset_travel_colours(features, on_level);
            feats.init();
            break;
        }

        case CMD_MAP_CLEAR_EXCLUDES:
            clear_excludes();
            _reset_travel_colours(features, on_level);
            feats.init();
            break;

#ifdef WIZARD
        case CMD_MAP_EXCLUDE_RADIUS:
        {
            const coord_def p(start_x + curs_x - 1, start_y + curs_y - 1);
            set_exclude(p, getchm() - '0');

            _reset_travel_colours(features, on_level);
            feats.init();
            break;
        }
#endif

        case CMD_MAP_MOVE_DOWN_LEFT:
            move_x = -1;
            move_y = 1;
            break;

        case CMD_MAP_MOVE_DOWN:
            move_y = 1;
            move_x = 0;
            break;

        case CMD_MAP_MOVE_UP_RIGHT:
            move_x = 1;
            move_y = -1;
            break;

        case CMD_MAP_MOVE_UP:
            move_y = -1;
            move_x = 0;
            break;

        case CMD_MAP_MOVE_UP_LEFT:
            move_y = -1;
            move_x = -1;
            break;

        case CMD_MAP_MOVE_LEFT:
            move_x = -1;
            move_y = 0;
            break;

        case CMD_MAP_MOVE_DOWN_RIGHT:
            move_y = 1;
            move_x = 1;
            break;

        case CMD_MAP_MOVE_RIGHT:
            move_x = 1;
            move_y = 0;
            break;

        case CMD_MAP_PREV_LEVEL:
        case CMD_MAP_NEXT_LEVEL: {
            level_id next;

            next = (cmd == CMD_MAP_PREV_LEVEL)
                   ? find_up_level(level_id::current())
                   : find_down_level(level_id::current());

            if (next.is_valid() && next != level_id::current()
                    && is_existing_level(next))
            {
                le.go_to(next);
                new_level = true;
            }
            break;
        }

        case CMD_MAP_GOTO_LEVEL: {
            std::string name;
            const level_pos pos =
                prompt_translevel_target(TPF_DEFAULT_OPTIONS, name).p;

            if (pos.id.depth < 1 || pos.id.depth > branches[pos.id.branch].depth
                    || !is_existing_level(pos.id))
            {
                canned_msg(MSG_OK);
                redraw_map = true;
                break;
            }

            le.go_to(pos.id);
            new_level = true;
            break;
        }

        case CMD_MAP_JUMP_DOWN_LEFT:
            move_x = -block_step;
            move_y = block_step;
            break;

        case CMD_MAP_JUMP_DOWN:
            move_y = block_step;
            move_x = 0;
            break;

        case CMD_MAP_JUMP_UP_RIGHT:
            move_x = block_step;
            move_y = -block_step;
            break;

        case CMD_MAP_JUMP_UP:
            move_y = -block_step;
            move_x = 0;
            break;

        case CMD_MAP_JUMP_UP_LEFT:
            move_y = -block_step;
            move_x = -block_step;
            break;

        case CMD_MAP_JUMP_LEFT:
            move_x = -block_step;
            move_y = 0;
            break;

        case CMD_MAP_JUMP_DOWN_RIGHT:
            move_y = block_step;
            move_x = block_step;
            break;

        case CMD_MAP_JUMP_RIGHT:
            move_x = block_step;
            move_y = 0;
            break;

        case CMD_MAP_SCROLL_DOWN:
            move_y = 20;
            move_x = 0;
            scroll_y = 20;
            break;

        case CMD_MAP_SCROLL_UP:
            move_y = -20;
            move_x = 0;
            scroll_y = -20;
            break;

        case CMD_MAP_FIND_YOU:
            if (on_level)
            {
                move_x = you.pos().x - (start_x + curs_x - 1);
                move_y = you.pos().y - (start_y + curs_y - 1);
            }
            break;

        case CMD_MAP_FIND_UPSTAIR:
        case CMD_MAP_FIND_DOWNSTAIR:
        case CMD_MAP_FIND_PORTAL:
        case CMD_MAP_FIND_TRAP:
        case CMD_MAP_FIND_ALTAR:
        case CMD_MAP_FIND_EXCLUDED:
        case CMD_MAP_FIND_F:
        case CMD_MAP_FIND_WAYPOINT:
        case CMD_MAP_FIND_STASH:
        case CMD_MAP_FIND_STASH_REVERSE:
        {
            bool forward = (cmd != CMD_MAP_FIND_STASH_REVERSE);

            int getty;
            switch (cmd)
            {
            case CMD_MAP_FIND_UPSTAIR:
                getty = '<';
                break;
            case CMD_MAP_FIND_DOWNSTAIR:
                getty = '>';
                break;
            case CMD_MAP_FIND_PORTAL:
                getty = '\t';
                break;
            case CMD_MAP_FIND_TRAP:
                getty = '^';
                break;
            case CMD_MAP_FIND_ALTAR:
                getty = '_';
                break;
            case CMD_MAP_FIND_EXCLUDED:
                getty = 'E';
                break;
            case CMD_MAP_FIND_F:
                getty = 'F';
                break;
            case CMD_MAP_FIND_WAYPOINT:
                getty = 'W';
                break;
            default:
            case CMD_MAP_FIND_STASH:
            case CMD_MAP_FIND_STASH_REVERSE:
                getty = 'I';
                break;
            }

            if (anchor_x == -1)
            {
                anchor_x = start_x + curs_x - 1;
                anchor_y = start_y + curs_y - 1;
            }
            if (travel_mode)
            {
                search_found = _find_feature(features, getty, curs_x, curs_y,
                                             start_x, start_y,
                                             search_found,
                                             &move_x, &move_y,
                                             forward);
            }
            else
            {
                search_found = _find_feature(getty, curs_x, curs_y,
                                             start_x, start_y,
                                             anchor_x, anchor_y,
                                             search_found, &move_x, &move_y);
            }
            break;
        }

        case CMD_MAP_GOTO_TARGET:
        {
            int x = start_x + curs_x - 1, y = start_y + curs_y - 1;
            if (travel_mode && on_level && x == you.pos().x && y == you.pos().y)
            {
                if (you.travel_x > 0 && you.travel_y > 0)
                {
                    move_x = you.travel_x - x;
                    move_y = you.travel_y - y;
                }
                break;
            }
            else
            {
                spec_place = level_pos(level_id::current(), coord_def(x, y));
                map_alive = false;
                break;
            }
        }

#ifdef WIZARD
        case CMD_MAP_WIZARD_TELEPORT:
        {
            if (!you.wizard)
                break;
            if (!on_level)
                break;
            const coord_def pos(start_x + curs_x - 1, start_y + curs_y - 1);
            if (!in_bounds(pos))
                break;
            you.moveto(pos);
            map_alive = false;
            break;
        }
#endif

        case CMD_MAP_EXIT_MAP:
            if (allow_esc)
            {
                spec_place = level_pos();
                map_alive = false;
                break;
            }
        default:
            if (travel_mode)
            {
                map_alive = false;
                break;
            }
            redraw_map = false;
            continue;
        }

        if (!map_alive)
            break;

#ifdef USE_TILE
        {
            int new_x = start_x + curs_x + move_x - 1;
            int new_y = start_y + curs_y + move_y - 1;

            curs_x += (new_x < 1 || new_x > GXM) ? 0 : move_x;
            curs_y += (new_y < 1 || new_y > GYM) ? 0 : move_y;
        }
#else
        if (curs_x + move_x < 1 || curs_x + move_x > crawl_view.termsz.x)
            move_x = 0;

        curs_x += move_x;

        if (num_lines < map_lines)
        {
            // Scrolling only happens when we don't have a large enough
            // display to show the known map.
            if (scroll_y != 0)
            {
                const int old_screen_y = screen_y;
                screen_y += scroll_y;
                if (scroll_y < 0)
                    screen_y = std::max(screen_y, min_y + half_screen);
                else
                    screen_y = std::min(screen_y, max_y - half_screen);
                curs_y -= (screen_y - old_screen_y);
                scroll_y = 0;
            }

            if (curs_y + move_y < 1)
            {
                screen_y += move_y;

                if (screen_y < min_y + half_screen)
                {
                    move_y   = screen_y - (min_y + half_screen);
                    screen_y = min_y + half_screen;
                }
                else
                    move_y = 0;
            }

            if (curs_y + move_y > num_lines)
            {
                screen_y += move_y;

                if (screen_y > max_y - half_screen)
                {
                    move_y   = screen_y - (max_y - half_screen);
                    screen_y = max_y - half_screen;
                }
                else
                    move_y = 0;
            }
        }

        if (curs_y + move_y < 1 || curs_y + move_y > num_lines)
            move_y = 0;

        curs_y += move_y;
#endif
    }

    le.go_to(original);

    travel_init_new_level();
    travel_cache.update();
}

bool emphasise(const coord_def& where)
{
    dungeon_feature_type feat = env.map_knowledge(where).feat();
    return (is_unknown_stair(where)
            && (you.your_level || feat_stair_direction(feat) == CMD_GO_DOWNSTAIRS)
            && you.where_are_you != BRANCH_VESTIBULE_OF_HELL);
}

static unsigned _get_travel_colour(const coord_def& p)
{
#ifdef WIZARD
    if (you.wizard && testbits(env.pgrid(p), FPROP_HIGHLIGHT))
        return (LIGHTGREEN);
#endif

    if (is_waypoint(p))
        return LIGHTGREEN;

    short dist = travel_point_distance[p.x][p.y];
    return dist > 0?                    Options.tc_reachable        :
           dist == PD_EXCLUDED?         Options.tc_excluded         :
           dist == PD_EXCLUDED_RADIUS?  Options.tc_exclude_circle   :
           dist < 0?                    Options.tc_dangerous        :
                                        Options.tc_disconnected;
}

static bool _travel_colour_override(const coord_def& p)
{
    if (is_waypoint(p) || travel_point_distance[p.x][p.y] == PD_EXCLUDED)
        return (true);
#ifdef WIZARD
    if (you.wizard && testbits(env.pgrid(p), FPROP_HIGHLIGHT))
        return (true);
#endif
    show_type obj = get_map_knowledge_obj(p);
    return (obj.cls == SH_FEATURE && (obj.feat == DNGN_FLOOR ||
                                      obj.feat == DNGN_LAVA ||
                                      obj.feat == DNGN_DEEP_WATER ||
                                      obj.feat == DNGN_SHALLOW_WATER));
}

unsigned get_map_col(const coord_def& p, bool travel)
{
    if (travel && _travel_colour_override(p))
        return _get_travel_colour(p);

    const show_type& obj = env.map_knowledge(p).object;
    if (obj.cls == SH_FEATURE && emphasise(p))
    {
        const feature_def& fdef = get_feature_def(obj);
        if (fdef.seen_em_colour)
            return fdef.seen_em_colour;
    }

    return get_map_knowledge_col(p);
}
