/**
 * @file
 * @brief Showing the level map (X and background).
**/

#include "AppHdr.h"

#include "viewmap.h"

#include <algorithm>

#include "branch.h"
#include "colour.h"
#include "command.h"
#include "coord.h"
#include "coordit.h"
#include "dgn-overview.h"
#include "directn.h"
#include "env.h"
#include "files.h"
#include "format.h"
#include "fprop.h"
#include "libutil.h"
#include "macro.h"
#include "map-knowledge.h"
#include "message.h"
#include "options.h"
#include "output.h"
#include "showsymb.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "tileview.h"
#include "tiles-build-specific.h"
#include "travel.h"
#include "unicode.h"
#include "view.h"
#include "viewchar.h"
#include "viewgeom.h"

#ifdef USE_TILE
#endif

#ifndef USE_TILE_LOCAL
/**
 * Get a console colour for representing the travel possibility at the given
 * position based on the last travel update. Used when drawing the console map.
 *
 * @param p The position.
 * @returns An unsigned int value for the colour.
*/
static unsigned _get_travel_colour(const coord_def& p)
{
#ifdef WIZARD
    if (you.wizard && testbits(env.pgrid(p), FPROP_HIGHLIGHT))
        return LIGHTGREEN;
#endif

    if (is_waypoint(p))
        return LIGHTGREEN;

    if (is_stair_exclusion(p))
        return Options.tc_excluded;

    const unsigned no_travel_col
        = feat_is_traversable(grd(p)) ? Options.tc_forbidden
                                      : Options.tc_dangerous;

    const short dist = travel_point_distance[p.x][p.y];
    return dist > 0?                    Options.tc_reachable        :
           dist == PD_EXCLUDED ?        Options.tc_excluded         :
           dist == PD_EXCLUDED_RADIUS ? Options.tc_exclude_circle   :
           dist < 0?                    no_travel_col               :
                                        Options.tc_disconnected;
}
#endif

#ifndef USE_TILE_LOCAL
bool travel_colour_override(const coord_def& p)
{
    if (is_waypoint(p) || is_stair_exclusion(p)
       || travel_point_distance[p.x][p.y] == PD_EXCLUDED)
    {
        return true;
    }
#ifdef WIZARD
    if (you.wizard && testbits(env.pgrid(p), FPROP_HIGHLIGHT))
        return true;
#endif

    const map_cell& cell = env.map_knowledge(p);
    show_class cls = get_cell_show_class(cell);
    if (cls == SH_FEATURE)
    {
        switch (cell.feat())
        {
        case DNGN_FLOOR:
        case DNGN_LAVA:
        case DNGN_DEEP_WATER:
        case DNGN_SHALLOW_WATER:
            return true;
        default:
            return false;
        }
    }
    else
        return false;
}

static bool _is_explore_horizon(const coord_def& c)
{
    if (env.map_knowledge(c).feat() != DNGN_UNSEEN)
        return false;

    // Note: c might be on map edge, walkable squares not really.
    for (adjacent_iterator ai(c); ai; ++ai)
        if (in_bounds(*ai))
        {
            dungeon_feature_type feat = env.map_knowledge(*ai).feat();
            if (feat != DNGN_UNSEEN
                && !feat_is_solid(feat)
                && !feat_is_door(feat))
            {
                return true;
            }
        }

    return false;
}
#endif

#ifndef USE_TILE_LOCAL
static char32_t _get_sightmap_char(dungeon_feature_type feat)
{
    return get_feature_def(feat).symbol();
}

static char32_t _get_magicmap_char(dungeon_feature_type feat)
{
    return get_feature_def(feat).magic_symbol();
}
#endif

static bool _is_player_defined_feature(char32_t feature)
{
    return feature == 'E' || feature == 'F' || feature == 'W';
}

// Determines if the given feature is present at (x, y) in _feat_ coordinates.
// If you have map coords, add (1, 1) to get grid coords.
// Use one of
// 1. '<' and '>' to look for stairs
// 2. '\t' or '\\' for shops, portals.
// 3. '^' for traps
// 4. '_' for altars
// 5. Anything else will look for the exact same character in the level map.
bool is_feature(char32_t feature, const coord_def& where)
{
    if (!env.map_knowledge(where).known() && !you.see_cell(where) && !_is_player_defined_feature(feature))
        return false;

    dungeon_feature_type grid = env.map_knowledge(where).feat();

    switch (feature)
    {
    case 'E':
        return travel_point_distance[where.x][where.y] == PD_EXCLUDED;
    case 'F':
    case 'W':
        return is_waypoint(where);
    case 'I':
        return is_stash(where);
    case '_':
        return feat_is_altar(grid) || grid == DNGN_UNKNOWN_ALTAR;
    case '\t':
    case '\\':
        return feat_is_gate(grid) || grid == DNGN_ENTER_SHOP
               || grid == DNGN_UNKNOWN_PORTAL;
    case '<':
        // DNGN_UNKNOWN_ALTAR doesn't need to be excluded here, because
        // feat_is_altar doesn't include it in the first place.
        return feat_stair_direction(grid) == CMD_GO_UPSTAIRS
                && !feat_is_altar(grid)
                && !feat_is_portal_exit(grid)
                && grid != DNGN_ENTER_SHOP
                && grid != DNGN_TRANSPORTER;
    case '>':
        return feat_stair_direction(grid) == CMD_GO_DOWNSTAIRS
                && !feat_is_altar(grid)
                && !feat_is_portal_entrance(grid)
                && grid != DNGN_ENTER_SHOP
                && grid != DNGN_TRANSPORTER;
    case '^':
        return feat_is_trap(grid);
    default:
        return get_cell_glyph(where).ch == feature;
    }
}

static bool _is_feature_fudged(char32_t glyph, const coord_def& where)
{
    if (!env.map_knowledge(where).known() && !_is_player_defined_feature(glyph))
        return false;

    if (is_feature(glyph, where))
        return true;

    if (glyph == '<')
    {
        return feat_is_portal_exit(grd(where))
               || grd(where) == DNGN_EXIT_ABYSS
               || grd(where) == DNGN_EXIT_PANDEMONIUM
               || grd(where) == DNGN_ENTER_HELL && player_in_hell();
    }
    else if (glyph == '>')
    {
        return feat_is_portal_entrance(grd(where))
               || grd(where) == DNGN_TRANSIT_PANDEMONIUM
               || grd(where) == DNGN_TRANSPORTER;
    }

    return false;
}

static int _find_feature(char32_t glyph, int curs_x, int curs_y,
                         int start_x, int start_y, int anchor_x, int anchor_y,
                         int ignore_count, int *move_x, int *move_y)
{
    int cx = anchor_x,
        cy = anchor_y;

    int firstx = -1, firsty = -1;
    int matchcount = 0;

    // Find the first occurrence of given glyph, spiralling around (x,y)
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
                if (_is_feature_fudged(glyph, coord_def(x, y)))
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

static int _find_feature(const vector<coord_def>& features,
                         char32_t feature, int curs_x, int curs_y,
                         int start_x, int start_y,
                         int ignore_count,
                         int *move_x, int *move_y,
                         bool forward)
{
    int firstx = -1, firsty = -1, firstmatch = -1;
    int matchcount = 0;

    for (coord_def coord : features)
    {
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
    return get_number_of_lines() - 1;
}

#ifndef USE_TILE_LOCAL
static void _draw_level_map(int start_x, int start_y, bool travel_mode,
        bool on_level)
{
    const int num_lines = min(_get_number_of_lines_levelmap(), GYM);
    const int num_cols  = min(get_number_of_cols(),            GXM);

    const coord_def extents(num_cols, num_lines);
    crawl_view_buffer vbuf(extents);
    screen_cell_t *cell= vbuf;

    cursor_control cs(false);

    int top = 2;
    cgotoxy(1, top);
    for (int screen_y = 0; screen_y < num_lines; screen_y++)
        for (int screen_x = 0; screen_x < num_cols; screen_x++)
        {
            coord_def c(start_x + screen_x, start_y + screen_y);

            if (!map_bounds(c))
            {
                cell->colour = DARKGREY;
                cell->glyph  = 0;
            }
            else
            {
                cglyph_t g = get_cell_glyph(c, false, -1);
                cell->glyph = g.ch;
                cell->colour = g.col;

                const show_class show = get_cell_show_class(env.map_knowledge(c));

                if (show == SH_NOTHING && _is_explore_horizon(c))
                {
                    const feature_def& fd = get_feature_def(DNGN_EXPLORE_HORIZON);
                    cell->glyph = fd.symbol();
                    cell->colour = fd.colour();
                }

                if (travel_mode && travel_colour_override(c))
                    cell->colour = _get_travel_colour(c);

                if (c == you.pos() && !crawl_state.arena_suspended && on_level)
                {
                    // [dshaligram] Draw the @ symbol on the
                    // level-map. It's no longer saved into the
                    // env.map_knowledge, so we need to draw it
                    // directly.
                    cell->colour = WHITE;
                    cell->glyph  = mons_char(you.symbol);
                }

                // If we've a waypoint on the current square, *and* the
                // square is a normal floor square with nothing on it,
                // show the waypoint number.
                // XXX: This is a horrible hack.
                char32_t bc   = cell->glyph;
                uint8_t ch = is_waypoint(c);
                if (ch && (bc == _get_sightmap_char(DNGN_FLOOR)
                           || bc == _get_magicmap_char(DNGN_FLOOR)))
                {
                    cell->glyph = ch;
                }

                if (Options.show_travel_trail && travel_trail_index(c) >= 0)
                    cell->colour |= COLFLAG_REVERSE;
            }

            cell++;
        }

    puttext(1, top, vbuf);
}
#endif // USE_TILE_LOCAL

static void _reset_travel_colours(vector<coord_def> &features, bool on_level)
{
    // We now need to redo travel colours.
    features.clear();

    if (on_level)
        find_travel_pos(you.pos(), nullptr, nullptr, &features);
    else
    {
        travel_pathfind tp;
        tp.set_feature_vector(&features);
        tp.get_features();
    }
}

// Sort glyphs within a group, for the feature list.
static bool _comp_glyphs(const cglyph_t& g1, const cglyph_t& g2)
{
    return g1.ch < g2.ch || g1.ch == g2.ch && g1.col < g2.col;
}

#ifndef USE_TILE_LOCAL
static cglyph_t _get_feat_glyph(const coord_def& gc);
#endif

class feature_list
{
    enum group
    {
        G_UP, G_DOWN, G_PORTAL, G_OTHER, G_NONE, NUM_GROUPS = G_NONE
    };

    vector<cglyph_t> data[NUM_GROUPS];

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
        dungeon_feature_type feat = env.map_knowledge(gc).feat();

        if (feat_is_staircase(feat) || feat_is_escape_hatch(feat))
            return feat_dir(feat);
        if (feat == DNGN_TRAP_SHAFT)
            return G_DOWN;
        if (feat_is_altar(feat) || feat == DNGN_ENTER_SHOP)
            return G_OTHER;
        if (get_feature_dchar(feat) == DCHAR_ARCH)
            return G_PORTAL;
        return G_NONE;
    }

    void maybe_add(const coord_def& gc)
    {
#ifndef USE_TILE_LOCAL
        if (!env.map_knowledge(gc).known())
            return;

        group grp = get_group(gc);
        if (grp != G_NONE)
            data[grp].push_back(_get_feat_glyph(gc));
#endif
    }

public:
    void init()
    {
        for (vector<cglyph_t> &groupdata : data)
            groupdata.clear();
        for (rectangle_iterator ri(0); ri; ++ri)
            maybe_add(*ri);
        for (vector<cglyph_t> &groupdata : data)
            sort(begin(groupdata), end(groupdata), _comp_glyphs);
    }

    formatted_string format() const
    {
        formatted_string s;
        for (const vector<cglyph_t> &groupdata : data)
            for (cglyph_t gly : groupdata)
                s.add_glyph(gly);
        return s;
    }
};

#ifndef USE_TILE_LOCAL
static void _draw_title(const coord_def& cpos, const feature_list& feats)
{
    const int columns = get_number_of_cols();
    const formatted_string help =
        formatted_string::parse_string("(Press <w>?</w> for help)");
    const int helplen = help.width();

    if (columns < helplen)
        return;

    const formatted_string title = feats.format();
    const int titlelen = title.width();
    if (columns < titlelen)
        return;

    string pstr = "";
#ifdef WIZARD
    if (you.wizard)
    {
        char buf[10];
        snprintf(buf, sizeof(buf), " (%d, %d)", cpos.x, cpos.y);
        pstr = string(buf);
    }
#endif // WIZARD

    cgotoxy(1, 1);
    textcolour(WHITE);

    cprintf("%s", chop_string(
                    uppercase_first(level_id::current().describe(true, true))
                      + pstr, columns - helplen).c_str());

    cgotoxy(max(1, (columns - titlelen) / 2), 1);
    title.display();

    textcolour(LIGHTGREY);
    cgotoxy(max(1, columns - helplen + 1), 1);
    help.display();
}
#endif

class levelview_excursion : public level_excursion
{
    bool travel_mode;

public:
    levelview_excursion(bool tm)
        : travel_mode(tm) {}

    ~levelview_excursion()
    {
        if (!you.on_current_level)
            go_to(original);
    }

    // Not virtual!
    void go_to(const level_id& next)
    {
#ifdef USE_TILE
        tiles.clear_minimap();
#endif
        level_excursion::go_to(next);
        tile_new_level(false);

        if (travel_mode)
        {
            travel_init_new_level();
            travel_cache.update();
        }
    }
};

static level_pos _stair_dest(const coord_def& p, command_type dir)
{
    if (!in_bounds(p))
        return level_pos();

    if (feat_stair_direction(env.map_knowledge(p).feat()) != dir)
        return level_pos();

    LevelInfo *linf = travel_cache.find_level_info(level_id::current());
    if (!linf)
        return level_pos();

    const stair_info *sinf = linf->get_stair(p);
    if (!sinf)
        return level_pos();

    return sinf->destination;
}

static void _unforget_map()
{
    ASSERT(env.map_forgotten.get());
    MapKnowledge &old(*env.map_forgotten.get());

    for (rectangle_iterator ri(0); ri; ++ri)
        if (!env.map_knowledge(*ri).seen() && old(*ri).seen())
        {
            // Don't overwrite known squares, nor magic-mapped with
            // magic-mapped data -- what was forgotten is less up to date.
            env.map_knowledge(*ri) = old(*ri);
            env.map_seen.set(*ri);
#ifdef USE_TILE
            tiles.update_minimap(*ri);
#endif
        }
}

static void _forget_map()
{
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (env.map_knowledge(*ri).flags & MAP_VISIBLE_FLAG)
            continue;
        env.map_knowledge(*ri).flags &= ~MAP_SEEN_FLAG;
        env.map_knowledge(*ri).flags |= MAP_MAGIC_MAPPED_FLAG;
        env.map_seen.set(*ri, false);
#ifdef USE_TILE
        tiles.update_minimap(*ri);
#endif
    }
}

// show_map() now centers the known map along x or y. This prevents
// the player from getting "artificial" location clues by using the
// map to see how close to the end they are. They'll need to explore
// to get that. This function is still a mess, though. -- bwr
bool show_map(level_pos &lpos,
              bool travel_mode, bool allow_esc, bool allow_offlevel)
{
    bool chose      = false;
#ifdef USE_TILE_LOCAL
    bool first_run  = true;

    mouse_control mc(MOUSE_MODE_NORMAL);
    tiles.do_map_display();
#endif

#ifdef USE_TILE_WEB
    tiles_crt_control crt(false);
    tiles_ui_control ui(UI_VIEW_MAP);
#endif

    {
        levelview_excursion le(travel_mode);
        level_id original(level_id::current());

        if (!lpos.id.is_valid() || !allow_offlevel)
            lpos.id = level_id::current();

        cursor_control ccon(!Options.use_fake_cursor);
        int i, j;

        int move_x = 0, move_y = 0, scroll_y = 0;

        bool new_level = true;

        // Vector to track all features we can travel to, in order of distance.
        vector<coord_def> features;
        // List of all interesting features for display in the (console) title.
        feature_list feats;

        int min_x = INT_MAX, max_x = INT_MIN, min_y = INT_MAX, max_y = INT_MIN;
        const int num_lines   = _get_number_of_lines_levelmap();
        const int half_screen = (num_lines - 1) / 2;

        int map_lines = 0;

        // no x scrolling
        int start_x = -1;
        const int block_step = Options.level_map_cursor_step;

        // y does scroll
        int start_y;

        int screen_y = -1;

        int curs_x = -1, curs_y = -1;
        int search_found = 0, anchor_x = -1, anchor_y = -1;

        bool map_alive  = true;
        bool redraw_map = true;

#ifndef USE_TILE_LOCAL
        const int top = 2;
        clrscr();
#endif
        textcolour(DARKGREY);

        bool on_level = false;

        while (map_alive)
        {
            if (lpos.id != level_id::current())
            {
                le.go_to(lpos.id);
                new_level = true;
            }

            if (new_level)
            {
                on_level = (level_id::current() == original);

                move_x = 0, move_y = 0, scroll_y = 0;

                // Vector to track all features we can travel to, in
                // order of distance.
                if (travel_mode)
                    _reset_travel_colours(features, on_level);

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

                if (lpos.id != level_id::current()
                    || !map_bounds(lpos.pos))
                {
                    lpos.id = level_id::current();
                    if (on_level)
                        lpos.pos = you.pos();
                    else
                    {
                        lpos.pos.x = min_x + (max_x - min_x + 1) / 2;
                        lpos.pos.y = min_y + (max_y - min_y + 1) / 2;
                    }
                }

                screen_y = lpos.pos.y;

                // If close to top of known map, put min_y on top
                // else if close to bottom of known map, put max_y on bottom.
                //
                // The num_lines comparisons are done to keep things neat, by
                // keeping things at the top of the screen. By shifting an
                // additional one in the num_lines > map_lines case, we can
                // keep the top line clear... which makes things look a whole
                // lot better for small maps.
                if (num_lines > map_lines)
                    screen_y = min_y + half_screen - 1;
                else if (num_lines == map_lines
                         || screen_y - half_screen < min_y)
                {
                    screen_y = min_y + half_screen;
                }
                else if (screen_y + half_screen > max_y)
                    screen_y = max_y - half_screen;

                curs_x = lpos.pos.x - start_x + 1;
                curs_y = lpos.pos.y - screen_y + half_screen + 1;
                search_found = 0, anchor_x = -1, anchor_y = -1;

                redraw_map = true;
                new_level = false;
            }

            // If we've received a HUP signal then the user can't choose a
            // location, so indicate this by returning an invalid position.
            if (crawl_state.seen_hups)
            {
                lpos = level_pos();
                chose = false;
            }

            start_y = screen_y - half_screen;

            move_x = move_y = 0;

            if (redraw_map)
            {
#ifdef USE_TILE
                // Note: Tile versions just center on the current cursor
                // location. It silently ignores everything else going
                // on in this function.  --Enne
#ifdef USE_TILE_LOCAL
                if (first_run)
                {
                    tiles.update_tabs();
                    first_run = false;
                }
#endif
                tiles.load_dungeon(lpos.pos);
#endif
#ifndef USE_TILE_LOCAL
                _draw_title(lpos.pos, feats);
                _draw_level_map(start_x, start_y, travel_mode, on_level);
#endif
            }
#ifndef USE_TILE_LOCAL
            cursorxy(curs_x, curs_y + top - 1);
#endif
            redraw_map = true;

            c_input_reset(true);
#ifdef USE_TILE_LOCAL
            const int key = getchm(KMC_LEVELMAP);
            command_type cmd = key_to_command(key, KMC_LEVELMAP);
#else
            const int key = unmangle_direction_keys(getchm(KMC_LEVELMAP),
                                                    KMC_LEVELMAP);
            command_type cmd = key_to_command(key, KMC_LEVELMAP);
#endif
            if (cmd < CMD_MIN_OVERMAP || cmd > CMD_MAX_OVERMAP)
                cmd = CMD_NO_CMD;

            if (key == CK_MOUSE_CLICK)
            {
#ifdef USE_TILE_LOCAL
                const coord_def grdp = tiles.get_cursor();
                const coord_def delta = grdp - lpos.pos;
                move_y = delta.y;
                move_x = delta.x;

                if (move_y == 0 && move_x == 0) // clicked on current position
                    cmd = CMD_MAP_GOTO_TARGET; // go to current cursor pos
                else
                    cmd = CMD_NEXT_CMD; // a dummy command
#else
                const c_mouse_event cme = get_mouse_event();
                const coord_def grdp =
                    cme.pos + coord_def(start_x - 1, start_y - top);

                if (cme.left_clicked() && in_bounds(grdp))
                {
                    lpos       = level_pos(level_id::current(), grdp);
                    chose      = true;
                    map_alive  = false;
                }
                else if (cme.scroll_up())
                    scroll_y = -block_step;
                else if (cme.scroll_down())
                    scroll_y = block_step;
                else if (cme.right_clicked())
                {
                    const coord_def delta = grdp - lpos.pos;
                    move_y = delta.y;
                    move_x = delta.x;
                }
#endif
            }

            if (key == CK_REDRAW)
            {
                if (Options.messages_at_top)
                {
                    display_message_window();
                    viewwindow();
                }
                else
                {
                    viewwindow();
                    display_message_window();
                }
                continue;
            }

            c_input_reset(false);

            switch (cmd)
            {
            case CMD_MAP_HELP:
                show_levelmap_help();
                break;

            case CMD_MAP_CLEAR_MAP:
                clear_map_or_travel_trail();
                break;

            case CMD_MAP_FORGET:
                {
                    // Merge it with already forgotten data first.
                    if (env.map_forgotten.get())
                        _unforget_map();
                    MapKnowledge *old = new MapKnowledge(env.map_knowledge);
                    _forget_map();
                    env.map_forgotten.reset(old);
                    mpr("Level map cleared.");
                }
                break;

            case CMD_MAP_UNFORGET:
                if (env.map_forgotten.get())
                {
                    _unforget_map();
                    env.map_forgotten.reset();
                    mpr("Remembered map restored.");
                }
                else
                    mpr("No remembered map.");
                break;

            case CMD_MAP_ADD_WAYPOINT:
                travel_cache.add_waypoint(lpos.pos.x, lpos.pos.y);
                // We need to do this all over again so that the user can jump
                // to the waypoint he just created.
                _reset_travel_colours(features, on_level);
                feats.init();
                break;

                // Cycle the radius of an exclude.
            case CMD_MAP_EXCLUDE_AREA:
                if (!is_map_persistent())
                    break;

                cycle_exclude_radius(lpos.pos);

                _reset_travel_colours(features, on_level);
                feats.init();
                break;

            case CMD_MAP_CLEAR_EXCLUDES:
                clear_excludes();
                _reset_travel_colours(features, on_level);
                feats.init();
                break;

#ifdef WIZARD
            case CMD_MAP_EXCLUDE_RADIUS:
                set_exclude(lpos.pos, getchm() - '0');

                _reset_travel_colours(features, on_level);
                feats.init();
                break;
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
            case CMD_MAP_NEXT_LEVEL:
            {
                if (!allow_offlevel)
                    break;

                const bool up = (cmd == CMD_MAP_PREV_LEVEL);
                level_pos dest =
                    _stair_dest(lpos.pos,
                                up ? CMD_GO_UPSTAIRS : CMD_GO_DOWNSTAIRS);

                if (!dest.id.is_valid())
                {
                    dest.id = up ? find_up_level(level_id::current())
                        : find_down_level(level_id::current());
                    dest.pos = coord_def(-1, -1);
                }

                if (dest.id.is_valid() && dest.id != level_id::current()
                    && is_existing_level(dest.id))
                {
                    lpos = dest;
                }
                los_changed();
                continue;
            }

            case CMD_MAP_GOTO_LEVEL:
            {
                if (!allow_offlevel)
                    break;

                string name;
                const level_pos pos
                    = prompt_translevel_target(TPF_DEFAULT_OPTIONS, name);

                if (pos.id.depth < 1
                    || pos.id.depth > brdepth[pos.id.branch]
                    || !is_existing_level(pos.id))
                {
                    canned_msg(MSG_OK);
                    redraw_map = true;
                    break;
                }

                lpos = pos;
                continue;
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
                    move_x = you.pos().x - lpos.pos.x;
                    move_y = you.pos().y - lpos.pos.y;
                }
                break;

            case CMD_MAP_FIND_UPSTAIR:
            case CMD_MAP_FIND_DOWNSTAIR:
            case CMD_MAP_FIND_PORTAL:
            case CMD_MAP_FIND_TRAP:
            case CMD_MAP_FIND_ALTAR:
            case CMD_MAP_FIND_EXCLUDED:
            case CMD_MAP_FIND_WAYPOINT:
            case CMD_MAP_FIND_STASH:
            case CMD_MAP_FIND_STASH_REVERSE:
            {
                bool forward = (cmd != CMD_MAP_FIND_STASH_REVERSE);

                char32_t getty;
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
                    anchor_x = lpos.pos.x;
                    anchor_y = lpos.pos.y;
                }
                if (travel_mode && !_is_player_defined_feature(getty))
                {
                    search_found = _find_feature(features, getty,
                                                 curs_x, curs_y,
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
                                                 search_found,
                                                 &move_x, &move_y);
                }
                break;
            }

            case CMD_MAP_GOTO_TARGET:
                if (travel_mode && on_level && lpos.pos == you.pos())
                {
                    if (you.travel_x > 0 && you.travel_y > 0)
                    {
                        move_x = you.travel_x - lpos.pos.x;
                        move_y = you.travel_y - lpos.pos.y;
                    }
                }
                else
                {
                    chose = true;
                    map_alive = false;
                }

                break;

            case CMD_MAP_ANNOTATE_LEVEL:
                le.go_to(original);
                redraw_screen();
                le.go_to(lpos.id);

                if (!is_map_persistent())
                    mpr("You can't annotate this level.");
                else
                    do_annotate(lpos.id);

                redraw_map = true;
                break;

            case CMD_MAP_EXPLORE:
                if (on_level && !player_in_branch(BRANCH_LABYRINTH))
                {
                    travel_pathfind tp;
                    tp.set_floodseed(you.pos(), true);

                    coord_def whereto = tp.pathfind(Options.explore_greedy
                                                    ? RMODE_EXPLORE_GREEDY
                                                    : RMODE_EXPLORE);
                    _reset_travel_colours(features, on_level);

                    if (!whereto.zero())
                    {
                        move_x = whereto.x - lpos.pos.x;
                        move_y = whereto.y - lpos.pos.y;
                    }
                }
                break;

#ifdef WIZARD
            case CMD_MAP_WIZARD_TELEPORT:
                if (!you.wizard || !on_level || !in_bounds(lpos.pos))
                    break;
                if (cell_is_solid(lpos.pos))
                    you.wizmode_teleported_into_rock = true;
                you.moveto(lpos.pos);
                map_alive = false;
                break;
#endif

            case CMD_MAP_EXIT_MAP:
                if (allow_esc)
                {
                    lpos = level_pos();
                    map_alive = false;
                    break;
                }

#ifdef USE_TILE_LOCAL
            case CMD_NEXT_CMD:
                break; // allow mouse clicks to move cursor without leaving map mode
#endif
            case CMD_MAP_DESCRIBE:
                if (map_bounds(lpos.pos) && env.map_knowledge(lpos.pos).known())
                {
                    full_describe_square(lpos.pos, false);
                    redraw_map = true;
                }
                break;


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

            const coord_def oldp = lpos.pos;
            lpos.pos.x += move_x;
            lpos.pos.y += move_y;
            lpos.pos.x = min(max(lpos.pos.x, min_x), max_x);
            lpos.pos.y = min(max(lpos.pos.y, min_y), max_y);
            move_x = lpos.pos.x - oldp.x;
            move_y = lpos.pos.y - oldp.y;
#ifndef USE_TILE_LOCAL
            if (num_lines < map_lines)
            {
                // Scrolling only happens when we don't have a large enough
                // display to show the known map.
                if (scroll_y != 0)
                {
                    const int old_screen_y = screen_y;
                    screen_y += scroll_y;
                    if (scroll_y < 0)
                        screen_y = max(screen_y, min_y + half_screen);
                    else
                        screen_y = min(screen_y, max_y - half_screen);
                    curs_y -= (screen_y - old_screen_y);
                    scroll_y = 0;
                }
                if (curs_y + move_y < 1 || curs_y + move_y > num_lines)
                {
                    screen_y += move_y;
                    curs_y -= move_y;
                }
            }
            start_y = screen_y - half_screen;
#else
            (void)scroll_y; // Avoid a compiler warning.
#endif
            curs_x += move_x;
            curs_y += move_y;
        }
    }

#ifdef USE_TILE
    tiles.place_cursor(CURSOR_MAP, NO_CURSOR);
#endif

    redraw_screen();
#ifdef USE_TILE_LOCAL
    tiles.set_map_display(false);
#endif

    return chose;
}

bool emphasise(const coord_def& where)
{
    return is_unknown_stair(where) && !player_in_branch(BRANCH_VESTIBULE)
           || is_unknown_transporter(where);
}

#ifndef USE_TILE_LOCAL
// Get glyph for feature list; here because it's so similar
// to get_map_col.
// Except that that function doesn't exist...
static cglyph_t _get_feat_glyph(const coord_def& gc)
{
    // XXX: it's unclear whether we want to display all features
    // or just those not obscured by remembered/detected stuff.
    dungeon_feature_type feat = env.map_knowledge(gc).feat();
    const bool terrain_seen = env.map_knowledge(gc).seen();
    const feature_def &fdef = get_feature_def(feat);
    cglyph_t g;
    g.ch  = terrain_seen ? fdef.symbol() : fdef.magic_symbol();
    unsigned col;
    if (travel_colour_override(gc))
        col = _get_travel_colour(gc);
    else if (emphasise(gc))
        col = fdef.seen_em_colour();
    else
        col = fdef.seen_colour();
    g.col = real_colour(col);
    return g;
}
#endif
