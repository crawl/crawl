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
#include "ui.h"
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
        return is_exclude_root(where);
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
                && grid != DNGN_ENTER_SHOP
                && grid != DNGN_TRANSPORTER;
    case '>':
        return feat_stair_direction(grid) == CMD_GO_DOWNSTAIRS
                && !feat_is_altar(grid)
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
        return grd(where) == DNGN_EXIT_ABYSS
               || grd(where) == DNGN_EXIT_PANDEMONIUM
               || grd(where) == DNGN_ENTER_HELL && player_in_hell();
    }
    else if (glyph == '>')
    {
        return grd(where) == DNGN_TRANSIT_PANDEMONIUM
               || grd(where) == DNGN_TRANSPORTER;
    }

    return false;
}

vector<coord_def> search_path_around_point(coord_def centre)
{
    vector<coord_def> points;

    int cx = centre.x, cy = centre.y;

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

                const auto x = cx + dx, y = cy + dy;

                if (in_bounds(x, y))
                    points.emplace_back(x, y);
            }
        }

    return points;
}

static int _find_feature(const vector<coord_def>& features,
                         char32_t feature,
                         int ignore_count,
                         coord_def &out_pos,
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
                out_pos = coord;
                // We want to cursor to (x,y)
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
        out_pos = coord_def(firstx, firsty);
        return firstmatch;
    }
    return 0;
}

#ifndef USE_TILE_LOCAL
static int _get_number_of_lines_levelmap()
{
    return get_number_of_lines() - 1;
}

static void _draw_level_map(int start_x, int start_y, bool travel_mode,
        bool on_level, ui::Region region)
{
    region.width = min(region.width, GXM);
    region.height = min(region.height, GYM);

    const coord_def extents(region.width, region.height);
    crawl_view_buffer vbuf(extents);
    screen_cell_t *cell = vbuf;

    cursor_control cs(false);

    for (int screen_y = 0; screen_y < region.height; screen_y++)
        for (int screen_x = 0; screen_x < region.width; screen_x++)
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

                if (show == SH_NOTHING && is_explore_horizon(c))
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
                {
                    const feature_def& fd = get_feature_def(DNGN_TRAVEL_TRAIL);

                    // Don't overwrite the player's symbol
                    if (fd.symbol() && c != you.pos())
                        cell->glyph = fd.symbol();
                    if (fd.colour() != COLOUR_UNDEF)
                        cell->colour = fd.colour();

                    cell->colour |= COLFLAG_REVERSE;
                }
            }

            cell++;
        }

    puttext(region.x + 1, region.y + 1, vbuf);
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
#else
        UNUSED(gc);
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
static void _draw_title(const coord_def& cpos, const feature_list& feats, const int columns)
{
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
    ASSERT(env.map_forgotten);
    MapKnowledge &old(*env.map_forgotten);

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

static void _forget_map(bool wizard_forget = false)
{
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        auto& flags = env.map_knowledge(*ri).flags;
        // don't touch squares we can currently see
        if (flags & MAP_VISIBLE_FLAG)
            continue;
        if (wizard_forget)
        {
            env.map_knowledge(*ri).clear();
#ifdef USE_TILE
            tile_forget_map(*ri);
#endif
        }
        else if (flags & MAP_SEEN_FLAG)
        {
            // squares we've seen in the past, pretend we've mapped instead
            flags |= MAP_MAGIC_MAPPED_FLAG;
            flags &= ~MAP_SEEN_FLAG;
        }
        env.map_seen.set(*ri, false);
#ifdef USE_TILE
        tiles.update_minimap(*ri);
#endif
    }
}

map_control_state process_map_command(command_type cmd, const map_control_state &state);

static coord_def _recentre_map_target(const level_id level,
                                      const level_id original)
{
    if (level == original)
        return you.pos();

    const auto bounds = known_map_bounds();
    return (bounds.first + bounds.second + 1) / 2;
}

#ifndef USE_TILE_LOCAL
static map_view_state _get_view_state(const map_control_state& state)
{
    map_view_state view;

    const int num_lines = _get_number_of_lines_levelmap();
    const int half_screen = (num_lines - 1) / 2;

    const auto bounds = known_map_bounds();
    const auto min_x = bounds.first.x;
    const auto min_y = bounds.first.y;
    const auto max_x = bounds.second.x;
    const auto max_y = bounds.second.y;

    const auto map_lines = max_y - min_y + 1;

    view.start.x = (min_x + max_x + 1) / 2 - 40;  // no x scrolling.
    view.start.y = 0;                             // y does scroll.

    auto screen_y = state.lpos.pos.y;

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
    else if (num_lines == map_lines || screen_y - half_screen < min_y)
        screen_y = min_y + half_screen;
    else if (screen_y + half_screen > max_y)
        screen_y = max_y - half_screen;

    view.cursor.x = state.lpos.pos.x - view.start.x + 1;
    view.cursor.y = state.lpos.pos.y - screen_y + half_screen + 1;

    view.start.y = screen_y - half_screen;

    return view;
}
#endif

class UIMapView : public ui::Widget
{
public:
    UIMapView(level_pos& lpos, levelview_excursion& le, bool travel_mode,
              bool allow_offlevel)
    {
        m_state.lpos = lpos;
        m_state.features = &m_features;
        m_state.feats = &m_feats;
        m_state.excursion = &le;
        m_state.allow_offlevel = allow_offlevel;
        m_state.travel_mode = travel_mode;
        m_state.original = level_id::current();
        m_state.map_alive = true;
        m_state.redraw_map = true;
        m_state.search_anchor = coord_def(-1, -1);
        m_state.chose = false;
        m_state.on_level = true;

        on_new_level();
    }
    ~UIMapView() {}

    void _render() override
    {
#ifdef USE_TILE
        tiles.load_dungeon(m_state.lpos.pos);
#endif

#ifdef USE_TILE_LOCAL
        display_message_window();
        tiles.render_current_regions();
        glmanager->reset_transform();
#endif

#ifndef USE_TILE_LOCAL
        const auto view = _get_view_state(m_state);
        _draw_title(m_state.lpos.pos, *m_state.feats, m_region.width);
        const ui::Region map_region = {0, 1, m_region.width, m_region.height - 1};
        _draw_level_map(view.start.x, view.start.y, m_state.travel_mode,
                        m_state.on_level, map_region);
        cursorxy(view.cursor.x, view.cursor.y + 1);
#endif
    }

    void _allocate_region() override
    {
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
        _expose();
    }

    bool on_event(const ui::Event& ev) override
    {
        if (ev.type() == ui::Event::Type::KeyDown)
        {
            auto key = static_cast<const ui::KeyEvent&>(ev).key();
#ifndef USE_TILE_LOCAL
            key = unmangle_direction_keys(key, KMC_LEVELMAP);
#endif
            command_type cmd = key_to_command(key, KMC_LEVELMAP);
            if (cmd < CMD_MIN_OVERMAP || cmd > CMD_MAX_OVERMAP)
                cmd = CMD_NO_CMD;
            process_command(cmd);
            if (m_state.redraw_map)
                _expose();
            return true;
        }

#ifdef USE_TILE_LOCAL
        if (ev.type() == ui::Event::Type::MouseMove
            || ev.type() == ui::Event::Type::MouseDown)
        {
            auto wm_event = to_wm_event(static_cast<const ui::MouseEvent&>(ev));
            tiles.handle_mouse(wm_event);
            if (ev.type() == ui::Event::Type::MouseDown)
                process_command(CMD_MAP_GOTO_TARGET);
            return true;
        }
#endif

        return false;
    }

    void process_command(command_type cmd)
    {
        m_state = process_map_command(cmd, m_state);
        if (!m_state.map_alive)
            return;
        if (map_bounds(m_state.lpos.pos))
            m_state.lpos.pos = m_state.lpos.pos.clamped(known_map_bounds());

        if (m_state.lpos.id != level_id::current())
        {
            m_state.excursion->go_to(m_state.lpos.id);
            on_new_level();
        }

        m_state.lpos.pos = m_state.lpos.pos.clamped(known_map_bounds());
    }

    void on_new_level()
    {
        m_state.on_level = (level_id::current() == m_state.original);

        // Vector to track all state.features we can travel to, in
        // order of distance.
        if (m_state.travel_mode)
            _reset_travel_colours(*m_state.features, m_state.on_level);
        m_state.feats->init();
        m_state.search_index = 0;
        m_state.search_anchor.reset();

        // This happens when CMD_MAP_PREV_LEVEL etc. assign dest to lpos,
        // with a position == (-1, -1).
        if (!map_bounds(m_state.lpos.pos))
        {
            m_state.lpos.pos =
                _recentre_map_target(m_state.lpos.id, m_state.original);
            m_state.lpos.id = level_id::current();
        }

        _expose();
    }

    bool is_alive() const
    {
        return m_state.map_alive;
    }

    bool chose() const
    {
        return m_state.chose;
    }

    level_pos lpos() const
    {
        return m_state.lpos;
    }

private:
    map_control_state m_state;

    // Vector to track all features we can travel to, in order of distance.
    vector<coord_def> m_features;
    // List of all interesting features for display in the (console) title.
    feature_list m_feats;

#ifdef USE_TILE_LOCAL
    bool first_run = true;
#endif
};

// show_map() now centers the known map along x or y. This prevents
// the player from getting "artificial" location clues by using the
// map to see how close to the end they are. They'll need to explore
// to get that. This function is still a mess, though. -- bwr
bool show_map(level_pos &lpos, bool travel_mode, bool allow_offlevel)
{
#ifdef USE_TILE_LOCAL
    mouse_control mc(MOUSE_MODE_NORMAL);
    tiles.do_map_display();
#endif

#ifdef USE_TILE
    ui::cutoff_point ui_cutoff_point;
#endif
#ifdef USE_TILE_WEB
    tiles_ui_control ui(UI_VIEW_MAP);
#endif

    if (!lpos.id.is_valid() || !allow_offlevel)
        lpos.id = level_id::current();

    levelview_excursion le(travel_mode);
    auto map_view = make_shared<UIMapView>(lpos, le, travel_mode, allow_offlevel);

#ifdef USE_TILE_LOCAL
    unwind_bool inhibit_rendering(ui::should_render_current_regions, false);
#endif

    ui::push_layout(map_view);
    while (map_view->is_alive() && !crawl_state.seen_hups)
        ui::pump_events();
    ui::pop_layout();

#ifdef USE_TILE_LOCAL
    tiles.set_map_display(false);
#endif
#ifdef USE_TILE
    tiles.place_cursor(CURSOR_MAP, NO_CURSOR);
#endif

    lpos = map_view->lpos();
    return map_view->chose();
}

map_control_state process_map_command(command_type cmd, const map_control_state& prev_state)
{
    map_control_state state = prev_state;
    state.map_alive = true;
    state.chose = false;

    const auto block_step = Options.level_map_cursor_step;

    switch (cmd)
    {
    case CMD_MAP_HELP:
        show_levelmap_help();
        break;

    case CMD_MAP_CLEAR_MAP:
        clear_map_or_travel_trail();
        break;

#ifdef WIZARD
    case CMD_MAP_WIZARD_FORGET:
        {
            // this doesn't seem useful outside of debugging and may
            // be buggy in unexpected ways, so wizmode-only. (Though
            // it doesn't leak information or anything.)
            if (!you.wizard)
                break;
            if (env.map_forgotten)
                _unforget_map();
            MapKnowledge *old = new MapKnowledge(env.map_knowledge);
            // completely wipe out map
            _forget_map(true);
            env.map_forgotten.reset(old);
            mpr("Level map wiped.");
            break;
        }
#endif

    case CMD_MAP_FORGET:
        {
            // Merge it with already forgotten data first.
            if (env.map_forgotten)
                _unforget_map();
            MapKnowledge *old = new MapKnowledge(env.map_knowledge);
            _forget_map();
            env.map_forgotten.reset(old);
            mpr("Level map cleared.");
        }
        break;

    case CMD_MAP_UNFORGET:
        if (env.map_forgotten)
        {
            _unforget_map();
            env.map_forgotten.reset();
            mpr("Remembered map restored.");
        }
        else
            mpr("No remembered map.");
        break;

    case CMD_MAP_ADD_WAYPOINT:
        travel_cache.add_waypoint(state.lpos.pos.x, state.lpos.pos.y);
        // We need to do this all over again so that the user can jump
        // to the waypoint he just created.
        _reset_travel_colours(*state.features, state.on_level);
        state.feats->init();
        break;

        // Cycle the radius of an exclude.
    case CMD_MAP_EXCLUDE_AREA:
        if (!is_map_persistent())
            break;

        cycle_exclude_radius(state.lpos.pos);

        _reset_travel_colours(*state.features, state.on_level);
        state.feats->init();
        break;

    case CMD_MAP_CLEAR_EXCLUDES:
        clear_excludes();
        _reset_travel_colours(*state.features, state.on_level);
        state.feats->init();
        break;

#ifdef WIZARD
    case CMD_MAP_EXCLUDE_RADIUS:
        set_exclude(state.lpos.pos, getchm() - '0');

        _reset_travel_colours(*state.features, state.on_level);
        state.feats->init();
        break;
#endif

    case CMD_MAP_MOVE_DOWN_LEFT:
        state.lpos.pos += coord_def(-1, 1);
        break;

    case CMD_MAP_MOVE_DOWN:
        state.lpos.pos += coord_def(0, 1);
        break;

    case CMD_MAP_MOVE_UP_RIGHT:
        state.lpos.pos += coord_def(1, -1);
        break;

    case CMD_MAP_MOVE_UP:
        state.lpos.pos += coord_def(0, -1);
        break;

    case CMD_MAP_MOVE_UP_LEFT:
        state.lpos.pos += coord_def(-1, -1);
        break;

    case CMD_MAP_MOVE_LEFT:
        state.lpos.pos += coord_def(-1, 0);
        break;

    case CMD_MAP_MOVE_DOWN_RIGHT:
        state.lpos.pos += coord_def(1, 1);
        break;

    case CMD_MAP_MOVE_RIGHT:
        state.lpos.pos += coord_def(1, 0);
        break;

    case CMD_MAP_PREV_LEVEL:
    case CMD_MAP_NEXT_LEVEL:
    {
        if (!state.allow_offlevel)
            break;

        const bool up = (cmd == CMD_MAP_PREV_LEVEL);
        level_pos dest =
            _stair_dest(state.lpos.pos,
                        up ? CMD_GO_UPSTAIRS : CMD_GO_DOWNSTAIRS);

        if (!dest.id.is_valid())
        {
            dest.id = up ? find_up_level(level_id::current())
                : find_down_level(level_id::current());
            dest.pos = coord_def(-1, -1);
        }

        if (dest.id.is_valid() && dest.id != level_id::current()
            && you.level_visited(dest.id))
        {
            state.lpos = dest;
        }
        los_changed();
        break;
    }

    case CMD_MAP_GOTO_LEVEL:
        if (!state.allow_offlevel)
            break;

        {
            string name;
#ifdef USE_TILE_WEB
            tiles_ui_control msgwin(UI_NORMAL);
#endif
            const level_pos pos
                = prompt_translevel_target(TPF_DEFAULT_OPTIONS, name);

            if (pos.id.depth < 1
                || pos.id.depth > brdepth[pos.id.branch]
                || !you.level_visited(pos.id))
            {
                canned_msg(MSG_OK);
                state.redraw_map = true;
                break;
            }

            state.lpos = pos;
        }
        break;

    case CMD_MAP_JUMP_DOWN_LEFT:
        state.lpos.pos += coord_def(-block_step, block_step);
        break;

    case CMD_MAP_JUMP_DOWN:
        state.lpos.pos += coord_def(0, block_step);
        break;

    case CMD_MAP_JUMP_UP_RIGHT:
        state.lpos.pos += coord_def(block_step, -block_step);
        break;

    case CMD_MAP_JUMP_UP:
        state.lpos.pos += coord_def(0, -block_step);
        break;

    case CMD_MAP_JUMP_UP_LEFT:
        state.lpos.pos += coord_def(-block_step, -block_step);
        break;

    case CMD_MAP_JUMP_LEFT:
        state.lpos.pos += coord_def(-block_step, 0);
        break;

    case CMD_MAP_JUMP_DOWN_RIGHT:
        state.lpos.pos += coord_def(block_step, block_step);
        break;

    case CMD_MAP_JUMP_RIGHT:
        state.lpos.pos += coord_def(block_step, 0);
        break;

    case CMD_MAP_SCROLL_DOWN:
        state.lpos.pos += coord_def(0, 20);
        break;

    case CMD_MAP_SCROLL_UP:
        state.lpos.pos += coord_def(0, -20);
        break;

    case CMD_MAP_FIND_YOU:
        if (state.on_level)
            state.lpos.pos += you.pos() - state.lpos.pos;
        break;

#ifdef USE_TILE
    case CMD_MAP_ZOOM_IN:
    case CMD_MAP_ZOOM_OUT:
        tiles.zoom_dungeon(cmd == CMD_MAP_ZOOM_IN);
        break;
#endif

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

        if (state.search_anchor.zero())
            state.search_anchor = state.lpos.pos;

        if (state.travel_mode && !_is_player_defined_feature(getty))
        {
            state.search_index = _find_feature(*state.features, getty,
                                            state.search_index,
                                            state.lpos.pos,
                                            forward);
        }
        else
        {
            const auto search_path =
                search_path_around_point(state.search_anchor);
            state.search_index = _find_feature(*state.features, getty,
                                            state.search_index,
                                            state.lpos.pos,
                                            true);
        }
        break;
    }

    case CMD_MAP_GOTO_TARGET:
        if (state.travel_mode && state.on_level && state.lpos.pos == you.pos())
        {
            if (you.travel_x > 0 && you.travel_y > 0)
            {
                if (you.travel_z == level_id::current())
                    state.lpos.pos = coord_def(you.travel_x, you.travel_y);
                else if (state.allow_offlevel && you.travel_z.is_valid()
                                && can_travel_to(you.travel_z)
                                && you.level_visited(you.travel_z))
                {
                    // previous travel target is offlevel
                    state.lpos = level_pos(you.travel_z,
                                coord_def(you.travel_x, you.travel_y));
                    los_changed();
                }
            }
        }
        else
        {
            state.chose = true;
            state.map_alive = false;
        }

        break;

    case CMD_MAP_ANNOTATE_LEVEL:
        state.excursion->go_to(state.original);
        redraw_screen();
        update_screen();
        state.excursion->go_to(state.lpos.id);

        if (!is_map_persistent())
            mpr("You can't annotate this level.");
        else
        {
#ifdef USE_TILE_WEB
            tiles_ui_control msgwin(UI_NORMAL);
#endif
            do_annotate(state.lpos.id);
        }

        state.redraw_map = true;
        break;

    case CMD_MAP_EXPLORE:
        if (state.on_level)
        {
            travel_pathfind tp;
            tp.set_floodseed(you.pos(), true);

            coord_def whereto = tp.pathfind(Options.explore_greedy
                                            ? RMODE_EXPLORE_GREEDY
                                            : RMODE_EXPLORE);
            _reset_travel_colours(*state.features, state.on_level);

            if (!whereto.zero())
                state.lpos.pos = whereto;
        }
        break;

#ifdef WIZARD
    case CMD_MAP_WIZARD_TELEPORT:
        if (!you.wizard || !state.on_level || !in_bounds(state.lpos.pos))
            break;
        if (cell_is_solid(state.lpos.pos))
            you.wizmode_teleported_into_rock = true;
        you.moveto(state.lpos.pos);
        state.map_alive = false;
        break;
#endif

    case CMD_MAP_EXIT_MAP:
        state.lpos = level_pos();
        state.map_alive = false;
        break;

#ifdef USE_TILE_LOCAL
    case CMD_NEXT_CMD:
        break; // allow mouse clicks to move cursor without leaving map mode
#endif
    case CMD_MAP_DESCRIBE:
        if (map_bounds(state.lpos.pos) && env.map_knowledge(state.lpos.pos).known())
        {
            full_describe_square(state.lpos.pos, false);
            state.redraw_map = true;
        }
        break;

    default:
        if (!state.travel_mode)
            state.redraw_map = false;
        break;
    }

    return state;
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
    {
        g.ch = fdef.magic_symbol();
        col = fdef.seen_em_colour();
    }
    else
        col = fdef.seen_colour();
    g.col = real_colour(col);
    return g;
}
#endif
