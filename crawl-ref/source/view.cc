/*
 *  File:       view.cc
 *  Summary:    Misc function used to render the dungeon.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "view.h"

#include <stdint.h>
#include <string.h>
#include <cmath>
#include <sstream>
#include <algorithm>
#include <memory>

#ifdef TARGET_OS_DOS
#include <conio.h>
#endif

#include "externs.h"
#include "options.h"

#include "branch.h"
#include "command.h"
#include "cio.h"
#include "cloud.h"
#include "clua.h"
#include "colour.h"
#include "database.h"
#include "debug.h"
#include "delay.h"
#include "dgnevent.h"
#include "directn.h"
#include "dungeon.h"
#include "exclude.h"
#include "files.h"
#include "format.h"
#include "ghost.h"
#include "godabil.h"
#include "goditem.h"
#include "itemprop.h"
#include "los.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "monplace.h"
#include "monstuff.h"
#include "mon-util.h"
#include "newgame.h"
#include "jobs.h"
#include "notes.h"
#include "output.h"
#include "overmap.h"
#include "place.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "skills.h"
#include "stuff.h"
#include "spells3.h"
#include "stash.h"
#include "tiles.h"
#include "travel.h"
#include "state.h"
#include "terrain.h"
#include "tilemcache.h"
#include "tilesdl.h"
#include "travel.h"
#include "tutorial.h"
#include "xom.h"

#define DEBUG_PANE_BOUNDS 0

// These are hidden from the rest of the world... use the functions
// below to get information about the map grid.
#define MAP_MAGIC_MAPPED_FLAG   0x01
#define MAP_SEEN_FLAG           0x02
#define MAP_CHANGED_FLAG        0x04
#define MAP_DETECTED_MONSTER    0x08
#define MAP_DETECTED_ITEM       0x10
#define MAP_GRID_KNOWN          0xFF

#define MC_ITEM    0x01
#define MC_MONS    0x02

static FixedVector<feature_def, NUM_FEATURES> Feature;

crawl_view_geometry crawl_view;
FixedArray < unsigned int, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER > Show_Backup;

extern int stealth;             // defined in acr.cc

screen_buffer_t colour_code_map( const coord_def& p, bool item_colour = false,
        bool travel_colour = false, bool on_level = true );

void cloud_grid(void);
void monster_grid(bool do_updates);

static void _get_symbol( const coord_def& where,
                         int object, unsigned *ch,
                         unsigned short *colour,
                         bool magic_mapped = false );
static unsigned _get_symbol(int object, unsigned short *colour = NULL,
                            bool magic_mapped = false);

static int _get_item_dngn_code(const item_def &item);
static void _set_show_backup( int ex, int ey );
static int _get_viewobj_flags(int viewobj);

const feature_def &get_feature_def(dungeon_feature_type feat)
{
    return (Feature[feat]);
}

unsigned map_cell::glyph() const
{
    if (!object)
        return (' ');
    return _get_symbol(object, NULL, !(flags & MAP_SEEN_FLAG));
}

bool map_cell::known() const
{
    return (object && (flags & MAP_GRID_KNOWN));
}

bool map_cell::seen() const
{
    return (object && (flags & MAP_SEEN_FLAG));
}

bool inside_level_bounds(int x, int y)
{
    return (x > 0 && x < GXM && y > 0 && y < GYM);
}

bool inside_level_bounds(const coord_def &p)
{
    return (inside_level_bounds(p.x, p.y));
}

unsigned get_envmap_char(int x, int y)
{
    return env.map[x][y].glyph();
}

int get_envmap_obj(int x, int y)
{
    return (env.map[x][y].object);
}

void set_envmap_detected_item(int x, int y, bool detected)
{
    if (detected)
        env.map[x][y].flags |= MAP_DETECTED_ITEM;
    else
        env.map[x][y].flags &= ~MAP_DETECTED_ITEM;
}

bool is_envmap_detected_item(int x, int y)
{
    return (env.map[x][y].flags & MAP_DETECTED_ITEM);
}

void set_envmap_detected_mons(int x, int y, bool detected)
{
    if (detected)
        env.map[x][y].flags |= MAP_DETECTED_MONSTER;
    else
        env.map[x][y].flags &= ~MAP_DETECTED_MONSTER;
}

bool is_envmap_detected_mons(int x, int y)
{
    return (env.map[x][y].flags & MAP_DETECTED_MONSTER);
}

void set_envmap_glyph(int x, int y, int object, int col)
{
    map_cell &c = env.map[x][y];
    c.object = object;
    c.colour = col;
#ifdef USE_TILE
    tiles.update_minimap(x, y);
#endif
}

void set_envmap_glyph(const coord_def& c, int object, int col)
{
    set_envmap_glyph(c.x, c.y, object, col);
}

void set_envmap_obj( const coord_def& where, int obj )
{
    env.map(where).object = obj;
#ifdef USE_TILE
    tiles.update_minimap(where.x, where.y);
#endif
}

void set_envmap_col( int x, int y, int colour )
{
    env.map[x][y].colour = colour;
}

bool is_sanctuary(const coord_def& p)
{
    if (!map_bounds(p))
        return (false);
    return (testbits(env.map(p).property, FPROP_SANCTUARY_1)
            || testbits(env.map(p).property, FPROP_SANCTUARY_2));
}

bool is_bloodcovered(const coord_def& p)
{
    return (testbits(env.map(p).property, FPROP_BLOODY));
}

bool is_envmap_item(int x, int y)
{
    return (_get_viewobj_flags(env.map[x][y].object) & MC_ITEM);
}

bool is_envmap_mons(int x, int y)
{
    return (_get_viewobj_flags(env.map[x][y].object) & MC_MONS);
}

int get_envmap_col(const coord_def& p)
{
    return (env.map[p.x][p.y].colour);
}

bool is_terrain_known( int x, int y )
{
    return (env.map[x][y].known());
}

bool is_terrain_known(const coord_def &p)
{
    return (env.map(p).known());
}

bool is_terrain_seen( int x, int y )
{
    return (env.map[x][y].flags & MAP_SEEN_FLAG);
}

bool is_terrain_changed( int x, int y )
{
    return (env.map[x][y].flags & MAP_CHANGED_FLAG);
}

bool is_terrain_mapped(const coord_def &p)
{
    return (env.map(p).flags & MAP_MAGIC_MAPPED_FLAG);
}

// Used to mark dug out areas, unset when terrain is seen or mapped again.
void set_terrain_changed( int x, int y )
{
    env.map[x][y].flags |= MAP_CHANGED_FLAG;

    dungeon_events.fire_position_event(DET_FEAT_CHANGE, coord_def(x, y));
}

void set_terrain_mapped( int x, int y )
{
    env.map[x][y].flags &= (~MAP_CHANGED_FLAG);
    env.map[x][y].flags |= MAP_MAGIC_MAPPED_FLAG;
#ifdef USE_TILE
    tiles.update_minimap(x, y);
#endif
}

static void _automap_from( int x, int y, int mutated )
{
    if (mutated)
        magic_mapping(8 * mutated, 5 * mutated, true, false,
                      true, true, coord_def(x,y));
}

void reautomap_level( )
{
    int passive = player_mutation_level(MUT_PASSIVE_MAPPING);

    for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
        for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
            if (env.map[x][y].flags & MAP_SEEN_FLAG)
                _automap_from(x, y, passive);
}

void set_terrain_seen( int x, int y )
{
    const dungeon_feature_type feat = grd[x][y];

    // First time we've seen a notable feature.
    if (!(env.map[x][y].flags & MAP_SEEN_FLAG))
    {
        _automap_from(x, y, player_mutation_level(MUT_PASSIVE_MAPPING));

        const bool boring = !is_notable_terrain(feat)
            // A portal deeper into the Zigguart is boring.
            || (feat == DNGN_ENTER_PORTAL_VAULT
                && you.level_type == LEVEL_PORTAL_VAULT)
            // Altars in the temple are boring.
            || (feat_is_altar(feat)
                && player_in_branch(BRANCH_ECUMENICAL_TEMPLE))
            // Only note the first entrance to the Abyss/Pan/Hell
            // which is found.
            || ((feat == DNGN_ENTER_ABYSS || feat == DNGN_ENTER_PANDEMONIUM
                 || feat == DNGN_ENTER_HELL)
                && overmap_knows_num_portals(feat) > 1)
            // There are at least three Zot entrances, and they're always
            // on D:27, so ignore them.
            || feat == DNGN_ENTER_ZOT;

        if (!boring)
        {
            coord_def pos(x, y);
            std::string desc =
                feature_description(pos, false, DESC_NOCAP_A);

            take_note(Note(NOTE_SEEN_FEAT, 0, 0, desc.c_str()));
        }
    }

#ifdef USE_TILE
    env.map[x][y].flags &= ~(MAP_DETECTED_ITEM);
    env.map[x][y].flags &= ~(MAP_DETECTED_MONSTER);
#endif

    env.map[x][y].flags &= (~MAP_CHANGED_FLAG);
    env.map[x][y].flags |= MAP_SEEN_FLAG;
}

void clear_envmap_grid( const coord_def& p )
{
    env.map(p).clear();
}

void clear_envmap_grid( int x, int y )
{
    env.map[x][y].clear();
}

bool is_notable_terrain(dungeon_feature_type ftype)
{
    return (Feature[ftype].is_notable());
}

#if defined(TARGET_OS_WINDOWS) || defined(TARGET_OS_DOS) || defined(USE_TILE)
static unsigned _colflag2brand(int colflag)
{
    switch (colflag)
    {
    case COLFLAG_ITEM_HEAP:
        return (Options.heap_brand);
    case COLFLAG_FRIENDLY_MONSTER:
        return (Options.friend_brand);
    case COLFLAG_NEUTRAL_MONSTER:
        return (Options.neutral_brand);
    case COLFLAG_WILLSTAB:
        return (Options.stab_brand);
    case COLFLAG_MAYSTAB:
        return (Options.may_stab_brand);
    case COLFLAG_FEATURE_ITEM:
        return (Options.feature_item_brand);
    case COLFLAG_TRAP_ITEM:
        return (Options.trap_item_brand);
    default:
        return (CHATTR_NORMAL);
    }
}
#endif

unsigned real_colour(unsigned raw_colour)
{
    // This order is important - is_element_colour() doesn't want to see the
    // munged colours returned by dos_brand, so it should always be done
    // before applying DOS brands.
    const int colflags = raw_colour & 0xFF00;

    // Evaluate any elemental colours to guarantee vanilla colour is returned
    if (is_element_colour( raw_colour ))
        raw_colour = colflags | element_colour( raw_colour );

#if defined(TARGET_OS_WINDOWS) || defined(TARGET_OS_DOS) || defined(USE_TILE)
    if (colflags)
    {
        unsigned brand = _colflag2brand(colflags);
        raw_colour = dos_brand(raw_colour & 0xFF, brand);
    }
#endif

#ifndef USE_COLOUR_OPTS
    // Strip COLFLAGs for systems that can't do anything meaningful with them.
    raw_colour &= 0xFF;
#endif

    return (raw_colour);
}

static int _get_viewobj_flags(int object)
{
    // Check for monster glyphs.
    if (object >= DNGN_START_OF_MONSTERS)
        return (MC_MONS);

    // Check for item glyphs.
    if (object >= DNGN_ITEM_ORB && object < DNGN_CLOUD)
        return (MC_ITEM);

    // We don't care to look further; we could check for
    // clouds here as well.
    return (0);
}

static unsigned _get_symbol(int object, unsigned short *colour,
                            bool magic_mapped)
{
    unsigned ch;
    _get_symbol(coord_def(0,0), object, &ch, NULL, magic_mapped);
    return (ch);
}

static bool _emphasise(const coord_def& where, dungeon_feature_type feat)
{
    return (is_unknown_stair(where, feat)
            && (you.your_level || feat_stair_direction(feat) == CMD_GO_DOWNSTAIRS)
            && you.where_are_you != BRANCH_VESTIBULE_OF_HELL);
}

static bool _show_bloodcovered(const coord_def& where)
{
    if (!is_bloodcovered(where))
        return (false);

    dungeon_feature_type grid = grd(where);

    // Altars, stairs (of any kind) and traps should not be coloured red.
    return (!is_critical_feature(grid) && !feat_is_trap(grid));
}

static unsigned short _tree_colour(const coord_def& where)
{
    uint32_t h = where.x;
    h+=h<<10; h^=h>>6;
    h += where.y;
    h+=h<<10; h^=h>>6;
    h+=h<<3; h^=h>>11; h+=h<<15;
    return (h>>30) ? GREEN : LIGHTGREEN;
}

static void _get_symbol( const coord_def& where,
                         int object, unsigned *ch,
                         unsigned short *colour,
                         bool magic_mapped )
{
    ASSERT( ch != NULL );

    if (object < NUM_FEATURES)
    {
        const dungeon_feature_type feat =
            static_cast<dungeon_feature_type>(object);
        const feature_def &fdef = Feature[object];

        *ch = magic_mapped? fdef.magic_symbol
                          : fdef.symbol;

        // Don't recolor items
        if (colour && object < NUM_REAL_FEATURES)
        {
            const int colmask = *colour & COLFLAG_MASK;

            // TODO: consolidate with feat_is_stair etc.
            bool excluded_stairs = (feat >= DNGN_STONE_STAIRS_DOWN_I
                                    && feat <= DNGN_ESCAPE_HATCH_UP
                                    && is_exclude_root(where));

            bool blocked_movement = false;
            if (!excluded_stairs
                && feat >= DNGN_MINMOVE
                && you.duration[DUR_MESMERISED])
            {
                // Colour grids that cannot be reached due to beholders
                // dark grey.
                for (unsigned int i = 0; i < you.mesmerised_by.size(); i++)
                {
                    monsters& mon = menv[you.mesmerised_by[i]];
                    const int olddist = grid_distance(you.pos(), mon.pos());
                    const int newdist = grid_distance(where, mon.pos());

                    if (olddist < newdist)
                    {
                        blocked_movement = true;
                        break;
                    }
                }
            }

            if (excluded_stairs)
                *colour = Options.tc_excluded | colmask;
            else if (blocked_movement)
                *colour = DARKGREY | colmask;
            else if (feat >= DNGN_MINMOVE && is_sanctuary(where))
            {
                if (testbits(env.map(where).property, FPROP_SANCTUARY_1))
                    *colour = YELLOW | colmask;
                else if (testbits(env.map(where).property, FPROP_SANCTUARY_2))
                {
                    if (!one_chance_in(4))
                        *colour = WHITE | colmask;     // 3/4
                    else if (!one_chance_in(3))
                        *colour = LIGHTCYAN | colmask; // 1/6
                    else
                        *colour = LIGHTGREY | colmask; // 1/12
                }
            }
            else if (_show_bloodcovered(where))
                *colour = RED | colmask;
            else if (env.grid_colours(where))
                *colour = env.grid_colours(where) | colmask;
            else
            {
                // Don't clobber with BLACK, because the colour should be
                // already set.
                if (fdef.colour != BLACK)
                    *colour = fdef.colour | colmask;
                else if (feat == DNGN_TREES)
                    *colour = _tree_colour(where) | colmask;

                if (fdef.em_colour && fdef.em_colour != fdef.colour &&
                    _emphasise(where, feat))
                {
                    *colour = (fdef.em_colour | colmask);
                }
            }

            // TODO: should be a feat_is_whatever(feat)
            if (feat >= DNGN_FLOOR_MIN && feat <= DNGN_FLOOR_MAX
                || feat == DNGN_UNDISCOVERED_TRAP)
            {
                if (inside_halo(where))
                {
                    if (silenced(where))
                        *colour = LIGHTCYAN | colmask;
                    else
                        *colour = YELLOW | colmask;
                }
                else if (silenced(where))
                    *colour = CYAN | colmask;
            }
        }

        // Note anything we see that's notable
        if (!where.origin() && fdef.is_notable())
        {
            seen_notable_thing(feat, where);
        }
    }
    else
    {
        ASSERT(object >= DNGN_START_OF_MONSTERS);
        *ch = mons_char(object - DNGN_START_OF_MONSTERS);
    }

    if (colour)
        *colour = real_colour(*colour);
}

unsigned grid_character_at(const coord_def &c)
{
    unsigned glych;
    unsigned short glycol = 0;

    _get_symbol(c, grd(c), &glych, &glycol );
    return glych;
}

void get_item_symbol(unsigned int object, unsigned *ch,
                     unsigned short *colour)
{
    if (object < NUM_FEATURES)
    {
        *ch = Feature[object].symbol;

        // Don't clobber with BLACK, because the colour should be already set.
        if (Feature[object].colour != BLACK)
            *colour = Feature[object].colour;
    }
    *colour = real_colour(*colour);

}

dungeon_char_type get_feature_dchar( dungeon_feature_type feat )
{
    return (Feature[feat].dchar);
}

unsigned get_sightmap_char( int feature )
{
    if (feature < NUM_FEATURES)
        return (Feature[feature].symbol);

    return (0);
}

unsigned get_magicmap_char( int feature )
{
    if (feature < NUM_FEATURES)
        return (Feature[feature].magic_symbol);

    return (0);
}

static char _get_travel_colour( const coord_def& p )
{
    if (is_waypoint(p))
        return LIGHTGREEN;

    short dist = travel_point_distance[p.x][p.y];
    return dist > 0?                    Options.tc_reachable        :
           dist == PD_EXCLUDED?         Options.tc_excluded         :
           dist == PD_EXCLUDED_RADIUS?  Options.tc_exclude_circle   :
           dist < 0?                    Options.tc_dangerous        :
                                        Options.tc_disconnected;
}

#if defined(TARGET_OS_WINDOWS) || defined(TARGET_OS_DOS) || defined(USE_TILE)
static unsigned short _dos_reverse_brand(unsigned short colour)
{
    if (Options.dos_use_background_intensity)
    {
        // If the console treats the intensity bit on background colours
        // correctly, we can do a very simple colour invert.

        // Special casery for shadows.
        if (colour == BLACK)
            colour = (DARKGREY << 4);
        else
            colour = (colour & 0xF) << 4;
    }
    else
    {
        // If we're on a console that takes its DOSness very seriously the
        // background high-intensity bit is actually a blink bit. Blinking is
        // evil, so we strip the background high-intensity bit. This, sadly,
        // limits us to 7 background colours.

        // Strip off high-intensity bit.  Special case DARKGREY, since it's the
        // high-intensity counterpart of black, and we don't want black on
        // black.
        //
        // We *could* set the foreground colour to WHITE if the background
        // intensity bit is set, but I think we've carried the
        // angry-fruit-salad theme far enough already.

        if (colour == DARKGREY)
            colour |= (LIGHTGREY << 4);
        else if (colour == BLACK)
            colour = LIGHTGREY << 4;
        else
        {
            // Zap out any existing background colour, and the high
            // intensity bit.
            colour  &= 7;

            // And swap the foreground colour over to the background
            // colour, leaving the foreground black.
            colour <<= 4;
        }
    }

    return (colour);
}

static unsigned short _dos_hilite_brand(unsigned short colour,
                                        unsigned short hilite)
{
    if (!hilite)
        return (colour);

    if (colour == hilite)
        colour = 0;

    colour |= (hilite << 4);
    return (colour);
}

unsigned short dos_brand( unsigned short colour,
                          unsigned brand)
{
    if ((brand & CHATTR_ATTRMASK) == CHATTR_NORMAL)
        return (colour);

    colour &= 0xFF;

    if ((brand & CHATTR_ATTRMASK) == CHATTR_HILITE)
        return _dos_hilite_brand(colour, (brand & CHATTR_COLMASK) >> 8);
    else
        return _dos_reverse_brand(colour);
}
#endif

// FIXME: Rework this function to use the new terrain known/seen checks
// These are still env.map coordinates, NOT grid coordinates!
screen_buffer_t colour_code_map(const coord_def& p, bool item_colour,
                                bool travel_colour, bool on_level)
{
    const unsigned short map_flags = env.map(p).flags;
    if (!(map_flags & MAP_GRID_KNOWN))
        return (BLACK);

#ifdef WIZARD
    if (travel_colour && you.wizard
        && testbits(env.map(p).property, FPROP_HIGHLIGHT))
    {
        return (LIGHTGREEN);
    }
#endif

    dungeon_feature_type feat_value = grd(p);
    if (!see_cell(p))
    {
        const int remembered = get_envmap_obj(p);
        if (remembered < NUM_REAL_FEATURES)
            feat_value = static_cast<dungeon_feature_type>(remembered);
    }

    unsigned tc = travel_colour ? _get_travel_colour(p) : DARKGREY;

    if (map_flags & MAP_DETECTED_ITEM)
        return real_colour(Options.detected_item_colour);

    if (map_flags & MAP_DETECTED_MONSTER)
    {
        tc = Options.detected_monster_colour;
        return real_colour(tc);
    }

    // If this is an important travel square, don't allow the colour
    // to be overridden.
    if (is_waypoint(p) || travel_point_distance[p.x][p.y] == PD_EXCLUDED)
        return real_colour(tc);

    if (item_colour && is_envmap_item(p))
        return get_envmap_col(p);

    int feature_colour = DARKGREY;
    const bool terrain_seen = is_terrain_seen(p);
    const feature_def &fdef = Feature[feat_value];
    feature_colour = terrain_seen ? fdef.seen_colour : fdef.map_colour;

    if (terrain_seen && fdef.seen_em_colour && _emphasise(p, feat_value))
        feature_colour = fdef.seen_em_colour;

    if (feature_colour != DARKGREY)
        tc = feature_colour;
    else if (you.duration[DUR_MESMERISED] && on_level)
    {
        // If mesmerised, colour the few grids that can be reached anyway
        // lightgrey.
        const monsters *blocker = monster_at(p);
        const bool seen_blocker = blocker && you.can_see(blocker);
        if (grd(p) >= DNGN_MINMOVE && !seen_blocker)
        {
            bool blocked_movement = false;
            for (unsigned int i = 0; i < you.mesmerised_by.size(); i++)
            {
                const monsters& mon = menv[you.mesmerised_by[i]];
                const int olddist = grid_distance(you.pos(), mon.pos());
                const int newdist = grid_distance(p, mon.pos());

                if (olddist < newdist || !see_cell(env.show, p, mon.pos()))
                {
                    blocked_movement = true;
                    break;
                }
            }
            if (!blocked_movement)
                tc = LIGHTGREY;
        }
    }

    if (Options.feature_item_brand
        && is_critical_feature(feat_value)
        && igrd(p) != NON_ITEM)
    {
        tc |= COLFLAG_FEATURE_ITEM;
    }
    else if (Options.trap_item_brand
             && feat_is_trap(feat_value) && igrd(p) != NON_ITEM)
    {
        // FIXME: this uses the real igrd, which the player shouldn't
        // be aware of.
        tc |= COLFLAG_TRAP_ITEM;
    }

    return real_colour(tc);
}

int count_detected_mons()
{
    int count = 0;
    for (rectangle_iterator ri(BOUNDARY_BORDER - 1); ri; ++ri)
    {
        // Don't expose new dug out areas:
        // Note: assumptions are being made here about how
        // terrain can change (eg it used to be solid, and
        // thus monster/item free).
        if (is_terrain_changed(*ri))
            continue;

        if (is_envmap_detected_mons(*ri))
            count++;
    }

    return (count);
}

void clear_map(bool clear_detected_items, bool clear_detected_monsters)
{
    for (rectangle_iterator ri(BOUNDARY_BORDER - 1); ri; ++ri)
    {
        const coord_def p = *ri;
        // Don't expose new dug out areas:
        // Note: assumptions are being made here about how
        // terrain can change (eg it used to be solid, and
        // thus monster/item free).

        // This reasoning doesn't make sense when it comes to *clearing*
        // the map! (jpeg)

        if (get_envmap_char(p) == 0)
            continue;

        if (is_envmap_item(p))
            continue;

        if (!clear_detected_items && is_envmap_detected_item(p))
            continue;

        if (!clear_detected_monsters && is_envmap_detected_mons(p))
            continue;

#ifdef USE_TILE
        if (is_terrain_mapped(p) && !is_envmap_detected_mons(p))
            continue;
#endif

        set_envmap_obj(p, is_terrain_seen(p) || is_terrain_mapped(p)
                ? grd(p) : DNGN_UNSEEN);
        set_envmap_detected_mons(p, false);
        set_envmap_detected_item(p, false);

#ifdef USE_TILE
        if (is_terrain_mapped(p))
        {
            unsigned int feature = grd(p);

            unsigned int feat_symbol;
            unsigned short feat_colour;
            get_item_symbol(feature, &feat_symbol, &feat_colour);

            unsigned int fg;
            unsigned int bg;
            tileidx_unseen(fg, bg, feat_symbol, p);
            env.tile_bk_bg(p) = bg;
            env.tile_bk_fg(p) = fg;
        }
        else
        {
            env.tile_bk_bg(p) = is_terrain_seen(p) ?
                tile_idx_unseen_terrain(p.x, p.y, grd(p)) :
                tileidx_feature(DNGN_UNSEEN, p.x, p.y);
            env.tile_bk_fg(p) = 0;
        }
#endif
    }
}

int get_mons_colour(const monsters *mons)
{
    int col = mons->colour;

    if (mons->has_ench(ENCH_BERSERK))
        col = RED;

    if (mons_friendly_real(mons))
    {
        col |= COLFLAG_FRIENDLY_MONSTER;
    }
    else if (mons_neutral(mons))
    {
        col |= COLFLAG_NEUTRAL_MONSTER;
    }
    else if (Options.stab_brand != CHATTR_NORMAL
             && mons_looks_stabbable(mons))
    {
        col |= COLFLAG_WILLSTAB;
    }
    else if (Options.may_stab_brand != CHATTR_NORMAL
             && mons_looks_distracted(mons))
    {
        col |= COLFLAG_MAYSTAB;
    }
    else if (mons_is_stationary(mons))
    {
        if (Options.feature_item_brand != CHATTR_NORMAL
            && is_critical_feature(grd(mons->pos()))
            && feat_stair_direction(grd(mons->pos())) != CMD_NO_CMD)
        {
            col |= COLFLAG_FEATURE_ITEM;
        }
        else if (Options.heap_brand != CHATTR_NORMAL
                 && igrd(mons->pos()) != NON_ITEM
                 && !crawl_state.arena)
        {
            col |= COLFLAG_ITEM_HEAP;
        }
    }

    // Backlit monsters are fuzzy and override brands.
    if (!you.can_see_invisible() && mons->has_ench(ENCH_INVIS)
        && mons->backlit())
    {
        col = DARKGREY;
    }

    return (col);
}

static void _good_god_follower_attitude_change(monsters *monster)
{
    if (you.is_unholy() || crawl_state.arena)
        return;

    // For followers of good gods, decide whether holy beings will be
    // good neutral towards you.
    if (is_good_god(you.religion)
        && monster->foe == MHITYOU
        && mons_is_holy(monster)
        && !testbits(monster->flags, MF_ATT_CHANGE_ATTEMPT)
        && !mons_wont_attack(monster)
        && you.visible_to(monster) && !monster->asleep()
        && !mons_is_confused(monster) && !mons_is_paralysed(monster))
    {
        monster->flags |= MF_ATT_CHANGE_ATTEMPT;

        if (x_chance_in_y(you.piety, MAX_PIETY) && !you.penance[you.religion])
        {
            const item_def* wpn = you.weapon();
            if (wpn
                && wpn->base_type == OBJ_WEAPONS
                && is_evil_item(*wpn)
                && coinflip()) // 50% chance of conversion failing
            {
                msg::stream << monster->name(DESC_CAP_THE)
                            << " glares at your weapon."
                            << std::endl;
                good_god_holy_fail_attitude_change(monster);
                return;
            }
            good_god_holy_attitude_change(monster);
            stop_running();
        }
        else
            good_god_holy_fail_attitude_change(monster);
    }
}

void beogh_follower_convert(monsters *monster, bool orc_hit)
{
    if (you.species != SP_HILL_ORC || crawl_state.arena)
        return;

    // For followers of Beogh, decide whether orcs will join you.
    if (you.religion == GOD_BEOGH
        && monster->foe == MHITYOU
        && mons_species(monster->type) == MONS_ORC
        && !mons_is_summoned(monster)
        && !mons_is_shapeshifter(monster)
        && !testbits(monster->flags, MF_ATT_CHANGE_ATTEMPT)
        && !mons_friendly(monster)
        && you.visible_to(monster) && !monster->asleep()
        && !mons_is_confused(monster) && !mons_is_paralysed(monster))
    {
        monster->flags |= MF_ATT_CHANGE_ATTEMPT;

        const int hd = monster->hit_dice;

        if (you.piety >= piety_breakpoint(2) && !player_under_penance()
            && random2(you.piety / 15) + random2(4 + you.experience_level / 3)
                 > random2(hd) + hd + random2(5))
        {
            if (you.weapon()
                && you.weapon()->base_type == OBJ_WEAPONS
                && get_weapon_brand(*you.weapon()) == SPWPN_ORC_SLAYING
                && coinflip()) // 50% chance of conversion failing
            {
                msg::stream << monster->name(DESC_CAP_THE)
                            << " flinches from your weapon."
                            << std::endl;
                return;
            }
            beogh_convert_orc(monster, orc_hit);
            stop_running();
        }
    }
}

void slime_convert(monsters* monster)
{
    if (you.religion == GOD_JIYVA && mons_is_slime(monster)
        && !mons_is_summoned(monster)
        && !mons_is_shapeshifter(monster)
        && !mons_neutral(monster)
        && !mons_friendly(monster)
        && !testbits(monster->flags, MF_ATT_CHANGE_ATTEMPT)
        && you.visible_to(monster) && !monster->asleep()
        && !mons_is_confused(monster) && !mons_is_paralysed(monster))
    {
        monster->flags |= MF_ATT_CHANGE_ATTEMPT;
        if (!player_under_penance())
        {
            jiyva_convert_slime(monster);
            stop_running();
        }
    }
}

void feawn_neutralise(monsters* monster)
{
    if (you.religion == GOD_FEAWN
        && monster->attitude == ATT_HOSTILE
        && feawn_neutralises(monster)
        && !testbits(monster->flags, MF_ATT_CHANGE_ATTEMPT)
        && !player_under_penance())
    {
        // We must call remove_auto_exclude before neutralizing the
        // plant because remove_auto_exclude only removes exclusions
        // it thinks were caused by auto-exclude, and
        // auto-exclusions now check for ATT_HOSTILE.  Oh, what a
        // tangled web, etc.
        remove_auto_exclude(monster, false);

        feawn_neutralise_plant(monster);
        monster->flags |= MF_ATT_CHANGE_ATTEMPT;

        stop_running();
    }
}

void handle_seen_interrupt(monsters* monster)
{
    if (mons_is_unknown_mimic(monster))
        return;

    activity_interrupt_data aid(monster);
    if (!monster->seen_context.empty())
        aid.context = monster->seen_context;
    else if (testbits(monster->flags, MF_WAS_IN_VIEW))
        aid.context = "already seen";
    else
        aid.context = "newly seen";

    if (!mons_is_safe(monster)
        && !mons_class_flag(monster->type, M_NO_EXP_GAIN))
    {
        interrupt_activity(AI_SEE_MONSTER, aid);
    }
    seen_monster( monster );
}

void flush_comes_into_view()
{
    if (!you.turn_is_over
        || (!you_are_delayed() && !crawl_state.is_repeating_cmd()))
    {
        return;
    }

    monsters* mon = crawl_state.which_mon_acting();

    if (!mon || !mon->alive() || (mon->flags & MF_WAS_IN_VIEW)
        || !you.can_see(mon))
    {
        return;
    }

    handle_seen_interrupt(mon);
}

void handle_monster_shouts(monsters* monster, bool force)
{
    if (!force && x_chance_in_y(you.skills[SK_STEALTH], 30))
        return;

    // Friendly or neutral monsters don't shout.
    if (!force && (mons_friendly(monster) || mons_neutral(monster)))
        return;

    // Get it once, since monster might be S_RANDOM, in which case
    // mons_shouts() will return a different value every time.
    // Demon lords will insult you as a greeting, but later we'll
    // choose a random verb and loudness for them.
    shout_type  s_type = mons_shouts(monster->type, false);

    // Silent monsters can give noiseless "visual shouts" if the
    // player can see them, in which case silence isn't checked for.
    if (s_type == S_SILENT && !monster->visible_to(&you)
        || s_type != S_SILENT && !player_can_hear(monster->pos()))
    {
        return;
    }

    mon_acting mact(monster);

    std::string default_msg_key = "";

    switch (s_type)
    {
    case S_SILENT:
        // No default message.
        break;
    case S_SHOUT:
        default_msg_key = "__SHOUT";
        break;
    case S_BARK:
        default_msg_key = "__BARK";
        break;
    case S_SHOUT2:
        default_msg_key = "__TWO_SHOUTS";
        break;
    case S_ROAR:
        default_msg_key = "__ROAR";
        break;
    case S_SCREAM:
        default_msg_key = "__SCREAM";
        break;
    case S_BELLOW:
        default_msg_key = "__BELLOW";
        break;
    case S_SCREECH:
        default_msg_key = "__SCREECH";
        break;
    case S_BUZZ:
        default_msg_key = "__BUZZ";
        break;
    case S_MOAN:
        default_msg_key = "__MOAN";
        break;
    case S_GURGLE:
        default_msg_key = "__GURGLE";
        break;
    case S_WHINE:
        default_msg_key = "__WHINE";
        break;
    case S_CROAK:
        default_msg_key = "__CROAK";
        break;
    case S_GROWL:
        default_msg_key = "__GROWL";
        break;
    case S_HISS:
        default_msg_key = "__HISS";
        break;
    case S_DEMON_TAUNT:
        default_msg_key = "__DEMON_TAUNT";
        break;
    default:
        default_msg_key = "__BUGGY"; // S_LOUD, S_VERY_SOFT, etc. (loudness)
    }

    // Now that we have the message key, get a random verb and noise level
    // for pandemonium lords.
    if (s_type == S_DEMON_TAUNT)
        s_type = mons_shouts(monster->type, true);

    std::string msg, suffix;
    std::string key = mons_type_name(monster->type, DESC_PLAIN);

    // Pandemonium demons have random names, so use "pandemonium lord"
    if (monster->type == MONS_PANDEMONIUM_DEMON)
        key = "pandemonium lord";
    // Search for player ghost shout by the ghost's class.
    else if (monster->type == MONS_PLAYER_GHOST)
    {
        const ghost_demon &ghost = *(monster->ghost);
        std::string ghost_class = get_class_name(ghost.job);

        key = ghost_class + " player ghost";

        default_msg_key = "player ghost";
    }

    // Tries to find an entry for "name seen" or "name unseen",
    // and if no such entry exists then looks simply for "name".
    // We don't use "you.can_see(monster)" here since that would return
    // false for submerged monsters, but submerged monsters will be forced
    // to surface before they shout, thus removing that source of
    // non-visibility.
    if (mons_near(monster) && (!monster->invisible() || you.can_see_invisible()))
        suffix = " seen";
    else
        suffix = " unseen";

    msg = getShoutString(key, suffix);

    if (msg == "__DEFAULT" || msg == "__NEXT")
        msg = getShoutString(default_msg_key, suffix);
    else if (msg.empty())
    {
        // NOTE: Use the hardcoded glyph rather than that returned
        // by mons_char(), since the result of mons_char() can be
        // changed by user settings.
        char mchar = get_monster_data(monster->type)->showchar;

        // See if there's a shout for all monsters using the
        // same glyph/symbol
        std::string glyph_key = "'";

        // Database keys are case-insensitve.
        if (isupper(mchar))
            glyph_key += "cap-";

        glyph_key += mchar;
        glyph_key += "'";
        msg = getShoutString(glyph_key, suffix);

        if (msg.empty() || msg == "__DEFAULT")
            msg = getShoutString(default_msg_key, suffix);
    }

    if (default_msg_key == "__BUGGY")
    {
        msg::streams(MSGCH_SOUND) << "You hear something buggy!"
                                  << std::endl;
    }
    else if (s_type == S_SILENT && (msg.empty() || msg == "__NONE"))
    {
        ; // No "visual shout" defined for silent monster, do nothing.
    }
    else if (msg.empty()) // Still nothing found?
    {
        msg::streams(MSGCH_DIAGNOSTICS)
            << "No shout entry for default shout type '"
            << default_msg_key << "'" << std::endl;

        msg::streams(MSGCH_SOUND) << "You hear something buggy!"
                                  << std::endl;
    }
    else if (msg == "__NONE")
    {
        msg::streams(MSGCH_DIAGNOSTICS)
            << "__NONE returned as shout for non-silent monster '"
            << default_msg_key << "'" << std::endl;
        msg::streams(MSGCH_SOUND) << "You hear something buggy!"
                                  << std::endl;
    }
    else
    {
        msg_channel_type channel = MSGCH_TALK;

        std::string param = "";
        std::string::size_type pos = msg.find(":");

        if (pos != std::string::npos)
        {
            param = msg.substr(0, pos);
            msg   = msg.substr(pos + 1);
        }

        if (s_type == S_SILENT || param == "VISUAL")
            channel = MSGCH_TALK_VISUAL;
        else if (param == "SOUND")
            channel = MSGCH_SOUND;

        // Monster must come up from being submerged if it wants to shout.
        if (monster->submerged())
        {
            if (!monster->del_ench(ENCH_SUBMERGED))
            {
                // Couldn't unsubmerge.
                return;
            }

            if (you.can_see(monster))
            {
                if (monster->type == MONS_AIR_ELEMENTAL)
                    monster->seen_context = "thin air";
                else if (monster->type == MONS_TRAPDOOR_SPIDER)
                    monster->seen_context = "leaps out";
                else if (!monster_habitable_grid(monster, DNGN_FLOOR))
                    monster->seen_context = "bursts forth shouting";
                else
                    monster->seen_context = "surfaces";

                // Give interrupt message before shout message.
                handle_seen_interrupt(monster);
            }
        }

        if (channel != MSGCH_TALK_VISUAL || you.can_see(monster))
        {
            msg = do_mon_str_replacements(msg, monster, s_type);
            msg::streams(channel) << msg << std::endl;

            // Otherwise it can move away with no feedback.
            if (you.can_see(monster))
            {
                if (!(monster->flags & MF_WAS_IN_VIEW))
                    handle_seen_interrupt(monster);
                seen_monster(monster);
            }
        }
    }

    const int  noise_level = get_shout_noise_level(s_type);
    const bool heard       = noisy(noise_level, monster->pos(), monster->mindex());

    if (Options.tutorial_left && (heard || you.can_see(monster)))
        learned_something_new(TUT_MONSTER_SHOUT, monster->pos());
}

#ifdef WIZARD
void force_monster_shout(monsters* monster)
{
    handle_monster_shouts(monster, true);
}
#endif

inline static bool _update_monster_grid(const monsters *monster)
{
    const coord_def e = grid2show(monster->pos());

    if (!monster->visible_to(&you))
    {
        // ripple effect?
        if (grd(monster->pos()) == DNGN_SHALLOW_WATER
            && !mons_flies(monster)
            && env.cgrid(monster->pos()) == EMPTY_CLOUD)
        {
            _set_show_backup(e.x, e.y);
            env.show(e)     = DNGN_INVIS_EXPOSED;

            // Translates between colours used for shallow and deep water,
            // if not using the normal LIGHTCYAN / BLUE. The ripple uses
            // the deep water colour.
            unsigned short base_colour = env.grid_colours(monster->pos());

            static const unsigned short ripple_table[] =
                {BLUE,          // BLACK        => BLUE (default)
                 BLUE,          // BLUE         => BLUE
                 GREEN,         // GREEN        => GREEN
                 CYAN,          // CYAN         => CYAN
                 RED,           // RED          => RED
                 MAGENTA,       // MAGENTA      => MAGENTA
                 BROWN,         // BROWN        => BROWN
                 DARKGREY,      // LIGHTGREY    => DARKGREY
                 DARKGREY,      // DARKGREY     => DARKGREY
                 BLUE,          // LIGHTBLUE    => BLUE
                 GREEN,         // LIGHTGREEN   => GREEN
                 BLUE,          // LIGHTCYAN    => BLUE
                 RED,           // LIGHTRED     => RED
                 MAGENTA,       // LIGHTMAGENTA => MAGENTA
                 BROWN,         // YELLOW       => BROWN
                 LIGHTGREY};    // WHITE        => LIGHTGREY

            env.show_col(e) = ripple_table[base_colour & 0x0f];
        }
        return (false);
    }

    // Mimics are always left on map.
    if (!mons_is_mimic( monster->type ))
        _set_show_backup(e.x, e.y);

    env.show(e)     = monster->type + DNGN_START_OF_MONSTERS;
    env.show_col(e) = get_mons_colour( monster );

    return (true);
}

void monster_grid(bool do_updates)
{
    do_updates = do_updates && !crawl_state.arena;

    for (int s = 0; s < MAX_MONSTERS; ++s)
    {
        monsters *monster = &menv[s];

        if (monster->alive() && mons_near(monster))
        {
            if (do_updates && (monster->asleep()
                               || mons_is_wandering(monster))
                && check_awaken(monster))
            {
                behaviour_event(monster, ME_ALERT, MHITYOU, you.pos(), false);
                handle_monster_shouts(monster);
            }

            // [enne] - It's possible that mgrd and monster->x/y are out of
            // sync because they are updated separately.  If we can see this
            // monster, then make sure that the mgrd is set correctly.
            if (mgrd(monster->pos()) != s)
            {
                // If this mprf triggers for you, please note any special
                // circumstances so we can track down where this is coming
                // from.
                mprf(MSGCH_ERROR, "monster %s (%d) at (%d, %d) was "
                     "improperly placed.  Updating mgrd.",
                     monster->name(DESC_PLAIN, true).c_str(), s,
                     monster->pos().x, monster->pos().y);
                ASSERT(mgrd(monster->pos()) == NON_MONSTER);
                mgrd(monster->pos()) = s;
            }

            if (!_update_monster_grid(monster))
                continue;

#ifdef USE_TILE
            tile_place_monster(monster->pos().x, monster->pos().y, s, true);
#endif

            _good_god_follower_attitude_change(monster);
            beogh_follower_convert(monster);
            slime_convert(monster);
            feawn_neutralise(monster);
        }
    }
}

void update_monsters_in_view()
{
    unsigned int num_hostile = 0;

    for (int s = 0; s < MAX_MONSTERS; s++)
    {
        monsters *monster = &menv[s];

        if (!monster->alive())
            continue;

        if (mons_near(monster))
        {
            if (monster->attitude == ATT_HOSTILE)
                num_hostile++;

            if (mons_is_unknown_mimic(monster))
            {
                // For unknown mimics, don't mark as seen,
                // but do mark it as in view for later messaging.
                // FIXME: is this correct?
                monster->flags |= MF_WAS_IN_VIEW;
            }
            else if (monster->visible_to(&you))
            {
                handle_seen_interrupt(monster);
                seen_monster(monster);
            }
            else
                monster->flags &= ~MF_WAS_IN_VIEW;
        }
        else
            monster->flags &= ~MF_WAS_IN_VIEW;

        // If the monster hasn't been seen by the time that the player
        // gets control back then seen_context is out of date.
        monster->seen_context.clear();
    }

    // Xom thinks it's hilarious the way the player picks up an ever
    // growing entourage of monsters while running through the Abyss.
    // To approximate this, if the number of hostile monsters in view
    // is greater than it ever was for this particular trip to the
    // Abyss, Xom is stimulated in proportion to the number of
    // hostile monsters.  Thus if the entourage doesn't grow, then
    // Xom becomes bored.
    if (you.level_type == LEVEL_ABYSS
        && you.attribute[ATTR_ABYSS_ENTOURAGE] < num_hostile)
    {
        you.attribute[ATTR_ABYSS_ENTOURAGE] = num_hostile;
        xom_is_stimulated(16 * num_hostile);
    }
}

bool check_awaken(monsters* monster)
{
    // Monsters put to sleep by ensorcelled hibernation will sleep
    // at least one turn.
    if (monster->has_ench(ENCH_SLEEPY))
        return (false);

    // Berserkers aren't really concerned about stealth.
    if (you.duration[DUR_BERSERKER])
        return (true);

    // I assume that creatures who can sense invisible are very perceptive.
    int mons_perc = 10 + (mons_intel(monster) * 4) + monster->hit_dice
                       + mons_sense_invis(monster) * 5;

    bool unnatural_stealthy = false; // "stealthy" only because of invisibility?

    // Critters that are wandering but still have MHITYOU as their foe are
    // still actively on guard for the player, even if they can't see you.
    // Give them a large bonus -- handle_behaviour() will nuke 'foe' after
    // a while, removing this bonus.
    if (mons_is_wandering(monster) && monster->foe == MHITYOU)
        mons_perc += 15;

    if (!you.visible_to(monster))
    {
        mons_perc -= 75;
        unnatural_stealthy = true;
    }

    if (monster->asleep())
    {
        if (monster->holiness() == MH_NATURAL)
        {
            // Monster is "hibernating"... reduce chance of waking.
            if (monster->has_ench(ENCH_SLEEP_WARY))
                mons_perc -= 10;
        }
        else // unnatural creature
        {
            // Unnatural monsters don't actually "sleep", they just
            // haven't noticed an intruder yet... we'll assume that
            // they're diligently on guard.
            mons_perc += 10;
        }
    }

    // If you've been tagged with Corona or are Glowing, the glow
    // makes you extremely unstealthy.
    if (you.backlit() && you.visible_to(monster))
        mons_perc += 50;

    if (mons_perc < 0)
        mons_perc = 0;

    if (x_chance_in_y(mons_perc + 1, stealth))
        return (true); // Oops, the monster wakes up!

    // You didn't wake the monster!
    if (player_light_armour(true)
        && you.can_see(monster) // to avoid leaking information
        && you.burden_state == BS_UNENCUMBERED
        && !you.attribute[ATTR_SHADOWS]
        && !mons_wont_attack(monster)
        && !mons_class_flag(monster->type, M_NO_EXP_GAIN)
            // If invisible, training happens much more rarely.
        && (!unnatural_stealthy && one_chance_in(25) || one_chance_in(100)))
    {
        exercise(SK_STEALTH, 1);
    }

    return (false);
}

static void _set_show_backup( int ex, int ey )
{
    // Must avoid double setting it.
    // We want the base terrain/item, not the cloud or monster that replaced it.
    if (!Show_Backup[ex][ey])
        Show_Backup[ex][ey] = env.show[ex][ey];
}

static int _get_item_dngn_code(const item_def &item)
{
    switch (item.base_type)
    {
    case OBJ_ORBS:       return (DNGN_ITEM_ORB);
    case OBJ_WEAPONS:    return (DNGN_ITEM_WEAPON);
    case OBJ_MISSILES:   return (DNGN_ITEM_MISSILE);
    case OBJ_ARMOUR:     return (DNGN_ITEM_ARMOUR);
    case OBJ_WANDS:      return (DNGN_ITEM_WAND);
    case OBJ_FOOD:       return (DNGN_ITEM_FOOD);
    case OBJ_SCROLLS:    return (DNGN_ITEM_SCROLL);
    case OBJ_JEWELLERY:
        return (jewellery_is_amulet(item)? DNGN_ITEM_AMULET : DNGN_ITEM_RING);
    case OBJ_POTIONS:    return (DNGN_ITEM_POTION);
    case OBJ_BOOKS:      return (DNGN_ITEM_BOOK);
    case OBJ_STAVES:     return (DNGN_ITEM_STAVE);
    case OBJ_MISCELLANY: return (DNGN_ITEM_MISCELLANY);
    case OBJ_CORPSES:    return (DNGN_ITEM_CORPSE);
    case OBJ_GOLD:       return (DNGN_ITEM_GOLD);
    default:             return (DNGN_ITEM_ORB); // bad item character
   }
}

inline static void _update_item_grid(const coord_def &gp, const coord_def &ep)
{
    const item_def &eitem = mitm[igrd(gp)];
    unsigned short &ecol  = env.show_col(ep);

    const dungeon_feature_type feat = grd(gp);
    if (Options.feature_item_brand && is_critical_feature(feat))
        ecol |= COLFLAG_FEATURE_ITEM;
    else if (Options.trap_item_brand && feat_is_trap(feat))
        ecol |= COLFLAG_TRAP_ITEM;
    else
    {
        const unsigned short gcol = env.grid_colours(gp);
        ecol = (feat == DNGN_SHALLOW_WATER) ?
               (gcol != BLACK ? gcol : CYAN) : eitem.colour;
        if (eitem.link != NON_ITEM && !crawl_state.arena)
            ecol |= COLFLAG_ITEM_HEAP;
        env.show(ep) = _get_item_dngn_code( eitem );
    }

#ifdef USE_TILE
    int idx = igrd(gp);
    if (feat_is_stair(feat))
        tile_place_item_marker(ep.x, ep.y, idx);
    else
        tile_place_item(ep.x, ep.y, idx);
#endif
}

void item_grid()
{
    const coord_def c(crawl_view.glosc());
    const coord_def offset(ENV_SHOW_OFFSET, ENV_SHOW_OFFSET);
    for (radius_iterator ri(c, LOS_RADIUS, true, false); ri; ++ri)
    {
        if (igrd(*ri) != NON_ITEM)
        {
            const coord_def ep = *ri - c + offset;
            if (env.show(ep))
                _update_item_grid(*ri, ep);
        }
    }
}

void get_item_glyph( const item_def *item, unsigned *glych,
                     unsigned short *glycol )
{
    *glycol = item->colour;
    _get_symbol( coord_def(0,0), _get_item_dngn_code( *item ), glych, glycol );
}

void get_mons_glyph( const monsters *mons, unsigned *glych,
                     unsigned short *glycol )
{
    *glycol = get_mons_colour( mons );
    _get_symbol( coord_def(0,0), mons->type + DNGN_START_OF_MONSTERS,
                 glych, glycol );
}

inline static void _update_cloud_grid(int cloudno)
{
    int which_colour = LIGHTGREY;
    const coord_def e = grid2show(env.cloud[cloudno].pos);

    switch (env.cloud[cloudno].type)
    {
    case CLOUD_FIRE:
    case CLOUD_FOREST_FIRE:
        if (env.cloud[cloudno].decay <= 20)
            which_colour = RED;
        else if (env.cloud[cloudno].decay <= 40)
            which_colour = LIGHTRED;
        else if (one_chance_in(4))
            which_colour = RED;
        else if (one_chance_in(4))
            which_colour = LIGHTRED;
        else
            which_colour = YELLOW;
        break;

    case CLOUD_STINK:
        which_colour = GREEN;
        break;

    case CLOUD_COLD:
        if (env.cloud[cloudno].decay <= 20)
            which_colour = BLUE;
        else if (env.cloud[cloudno].decay <= 40)
            which_colour = LIGHTBLUE;
        else if (one_chance_in(4))
            which_colour = BLUE;
        else if (one_chance_in(4))
            which_colour = LIGHTBLUE;
        else
            which_colour = WHITE;
        break;

    case CLOUD_POISON:
        which_colour = (one_chance_in(3) ? LIGHTGREEN : GREEN);
        break;

    case CLOUD_BLUE_SMOKE:
        which_colour = LIGHTBLUE;
        break;

    case CLOUD_PURP_SMOKE:
        which_colour = MAGENTA;
        break;

    case CLOUD_MIASMA:
    case CLOUD_BLACK_SMOKE:
        which_colour = DARKGREY;
        break;

    case CLOUD_RAIN:
    case CLOUD_MIST:
        which_colour = ETC_MIST;
        break;

    case CLOUD_CHAOS:
        which_colour = ETC_RANDOM;
        break;

    case CLOUD_MUTAGENIC:
        which_colour = ETC_MUTAGENIC;
        break;

    default:
        which_colour = LIGHTGREY;
        break;
    }

    _set_show_backup(e.x, e.y);
    env.show(e)     = DNGN_CLOUD;
    env.show_col(e) = which_colour;

#ifdef USE_TILE
    tile_place_cloud(e.x, e.y, env.cloud[cloudno].type,
                     env.cloud[cloudno].decay);
#endif
}

void cloud_grid(void)
{
    int mnc = 0;

    for (int s = 0; s < MAX_CLOUDS; s++)
    {
        // can anyone explain this??? {dlb}
        // its an optimization to avoid looking past the last cloud -bwr
        if (mnc >= env.cloud_no)
            break;

        if (env.cloud[s].type != CLOUD_NONE)
        {
            mnc++;
            if (see_cell(env.cloud[s].pos))
                _update_cloud_grid(s);
        }
    }
}

// Noisy now has a messenging service for giving messages to the
// player is appropriate.
//
// Returns true if the PC heard the noise.
bool noisy(int loudness, const coord_def& where, const char *msg, int who,
           bool mermaid)
{
    bool ret = false;

    if (loudness <= 0)
        return (false);

    // If the origin is silenced there is no noise.
    if (silenced(where))
        return (false);

    const int dist = loudness * loudness;
    const int player_distance = distance( you.pos(), where );

    // Message the player.
    if (player_distance <= dist && player_can_hear( where ))
    {
        if (msg)
            mpr( msg, MSGCH_SOUND );

        you.check_awaken(dist - player_distance);

        if (!mermaid && loudness >= 20 && you.duration[DUR_MESMERISED])
        {
            mprf("For a moment, you cannot hear the mermaid%s!",
                 you.mesmerised_by.size() == 1? "" : "s");
            mpr("You break out of your daze!", MSGCH_DURATION);
            you.duration[DUR_MESMERISED] = 0;
            you.mesmerised_by.clear();
        }

        ret = true;
    }

    for (int p = 0; p < MAX_MONSTERS; p++)
    {
        monsters* monster = &menv[p];

        if (!monster->alive())
            continue;

        // Monsters arent' affected by their own noise.  We don't check
        // where == monster->pos() since it might be caused by the
        // Projected Noise spell.
        if (p == who)
            continue;

        if (distance(monster->pos(), where) <= dist
            && !silenced(monster->pos()))
        {
            // If the noise came from the character, any nearby monster
            // will be jumping on top of them.
            if (where == you.pos())
                behaviour_event( monster, ME_ALERT, MHITYOU, you.pos() );
            else if (mermaid && mons_primary_habitat(monster) == HT_WATER
                     && !mons_friendly(monster))
            {
                // Mermaids/sirens call (hostile) aquatic monsters.
                behaviour_event( monster, ME_ALERT, MHITNOT, where );
            }
            else
                behaviour_event( monster, ME_DISTURB, MHITNOT, where );
        }
    }

    return (ret);
}

bool noisy(int loudness, const coord_def& where, int who,
           bool mermaid)
{
    return noisy(loudness, where, NULL, who, mermaid);
}

static const char* _player_vampire_smells_blood(int dist)
{
    // non-thirsty vampires get no clear indication of how close the
    // smell is
    if (you.hunger_state >= HS_SATIATED)
        return "";

    if (dist < 16) // 4*4
        return " near-by";

    if (you.hunger_state <= HS_NEAR_STARVING && dist > get_los_radius_sq())
        return " in the distance";

    return "";
}

void blood_smell( int strength, const coord_def& where )
{
    monsters *monster = NULL;

    const int range = strength * strength;
#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS,
         "blood stain at (%d, %d), range of smell = %d",
         where.x, where.y, range);
#endif

    // Of the player species, only Vampires can smell blood.
    if (you.species == SP_VAMPIRE)
    {
        // Whether they actually do so, depends on their hunger state.
        int vamp_strength = strength - 2 * (you.hunger_state - 1);
        if (vamp_strength > 0)
        {
            int vamp_range = vamp_strength * vamp_strength;

            const int player_distance = distance( you.pos(), where );

            if (player_distance <= vamp_range)
            {
#ifdef DEBUG_DIAGNOSTICS
                mprf(MSGCH_DIAGNOSTICS,
                     "Player smells blood, pos: (%d, %d), dist = %d)",
                     you.pos().x, you.pos().y, player_distance);
#endif
                you.check_awaken(range - player_distance);
                // Don't message if you can see the square.
                if (!see_cell(where))
                {
                    mprf("You smell fresh blood%s.",
                         _player_vampire_smells_blood(player_distance));
                }
            }
        }
    }

    for (int p = 0; p < MAX_MONSTERS; p++)
    {
        monster = &menv[p];

        if (monster->type < 0)
            continue;

        if (!mons_class_flag(monster->type, M_BLOOD_SCENT))
            continue;

        if (distance(monster->pos(), where) <= range)
        {
            // Let sleeping hounds lie.
            if (monster->asleep()
                && mons_species(monster->type) != MONS_VAMPIRE
                && monster->type != MONS_SHARK)
            {
                // 33% chance of sleeping on
                // 33% of being disturbed (start BEH_WANDER)
                // 33% of being alerted   (start BEH_SEEK)
                if (!one_chance_in(3))
                {
                    if (coinflip())
                    {
#ifdef DEBUG_DIAGNOSTICS
                        mprf(MSGCH_DIAGNOSTICS, "disturbing %s (%d, %d)",
                             monster->name(DESC_PLAIN).c_str(),
                             monster->pos().x, monster->pos().y);
#endif
                        behaviour_event(monster, ME_DISTURB, MHITNOT, where);
                    }
                    continue;
                }
            }
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "alerting %s (%d, %d)",
                 monster->name(DESC_PLAIN).c_str(),
                 monster->pos().x, monster->pos().y);
#endif
            behaviour_event( monster, ME_ALERT, MHITNOT, where );

            if (monster->type == MONS_SHARK)
            {
                // Sharks go into a battle frenzy if they smell blood.
                monster_pathfind mp;
                if (mp.init_pathfind(monster, where))
                {
                    mon_enchant ench = monster->get_ench(ENCH_BATTLE_FRENZY);
                    const int dist = 15 - (monster->pos() - where).rdist();
                    const int dur  = random_range(dist, dist*2)
                                     * speed_to_duration(monster->speed);

                    if (ench.ench != ENCH_NONE)
                    {
                        int level = ench.degree;
                        if (level < 4 && one_chance_in(2*level))
                            ench.degree++;
                        ench.duration = std::max(ench.duration, dur);
                        monster->update_ench(ench);
                    }
                    else
                    {
                        monster->add_ench(mon_enchant(ENCH_BATTLE_FRENZY, 1,
                                                      KC_OTHER, dur));
                        simple_monster_message(monster, " is consumed with "
                                                        "blood-lust!");
                    }
                }
            }
        }
    }
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
    if (!env.map(where).object && !see_cell(where))
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
        case DNGN_ALTAR_FEAWN:
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
        return get_envmap_char(where.x, where.y) == (unsigned) feature;
    }
}

static bool _is_feature_fudged(int feature, const coord_def& where)
{
    if (!env.map(where).object)
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
static std::string _level_description_string()
{
    if (you.level_type == LEVEL_PANDEMONIUM)
        return "- Pandemonium";

    if (you.level_type == LEVEL_ABYSS)
        return "- The Abyss";

    if (you.level_type == LEVEL_LABYRINTH)
        return "- a Labyrinth";

    if (you.level_type == LEVEL_PORTAL_VAULT)
    {
        if (!you.level_type_name.empty())
            return "- " + article_a(upcase_first(you.level_type_name));
        return "- a Portal Chamber";
    }

    // level_type == LEVEL_DUNGEON
    char buf[200];
    const int youbranch = you.where_are_you;
    if ( branches[youbranch].depth == 1 )
        snprintf(buf, sizeof buf, "- %s", branches[youbranch].longname);
    else
    {
        const int curr_subdungeon_level = player_branch_depth();
        snprintf(buf, sizeof buf, "%d of %s", curr_subdungeon_level,
                 branches[youbranch].longname);
    }
    return buf;
}

static void _draw_level_map(int start_x, int start_y, bool travel_mode,
        bool on_level)
{
    int bufcount2 = 0;
    screen_buffer_t buffer2[GYM * GXM * 2];

    const int num_lines = std::min(_get_number_of_lines_levelmap(), GYM);
    const int num_cols  = std::min(get_number_of_cols(),            GXM);

    cursor_control cs(false);

    int top = 1 + Options.level_map_title;
    if (Options.level_map_title)
    {
        const formatted_string help =
            formatted_string::parse_string("(Press <w>?</w> for help)");
        const int helplen = std::string(help).length();

        cgotoxy(1, 1);
        textcolor(WHITE);
        cprintf("%-*s",
                get_number_of_cols() - helplen,
                ("Level " + _level_description_string()).c_str());

        textcolor(LIGHTGREY);
        cgotoxy(get_number_of_cols() - helplen + 1, 1);
        help.display();
    }

    cgotoxy(1, top);

    for (int screen_y = 0; screen_y < num_lines; screen_y++)
        for (int screen_x = 0; screen_x < num_cols; screen_x++)
        {
            screen_buffer_t colour = DARKGREY;

            coord_def c(start_x + screen_x, start_y + screen_y);

            if (!map_bounds(c))
            {
                buffer2[bufcount2 + 1] = DARKGREY;
                buffer2[bufcount2] = 0;
            }
            else
            {
                colour = colour_code_map(c,
                                         Options.item_colour,
                                         travel_mode,
                                         on_level);

                buffer2[bufcount2 + 1] = colour;
                buffer2[bufcount2] = env.map(c).glyph();

                if (c == you.pos() && !crawl_state.arena_suspended && on_level)
                {
                    // [dshaligram] Draw the @ symbol on the level-map. It's no
                    // longer saved into the env.map, so we need to draw it
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

            min_x = GXM, max_x = 0, min_y = 0, max_y = 0;
            bool found_y = false;

            for (j = 0; j < GYM; j++)
                for (i = 0; i < GXM; i++)
                {
                    if (env.map[i][j].known())
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
#ifdef USE_TILE
            // Note: Tile versions just center on the current cursor
            // location.  It silently ignores everything else going
            // on in this function.  --Enne
            coord_def cen(start_x + curs_x - 1, start_y + curs_y - 1);
            tiles.load_dungeon(cen);
#else
            _draw_level_map(start_x, start_y, travel_mode, on_level);

#ifdef WIZARD
            if (you.wizard)
            {
                cgotoxy(get_number_of_cols() / 2, 1);
                textcolor(WHITE);
                cprintf("(%d, %d)", start_x + curs_x - 1,
                        start_y + curs_y - 1);

                textcolor(LIGHTGREY);
                cgotoxy(curs_x, curs_y + top - 1);
            }
#endif // WIZARD

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
            break;

        // Cycle the radius of an exclude.
        case CMD_MAP_EXCLUDE_AREA:
        {
            const coord_def p(start_x + curs_x - 1, start_y + curs_y - 1);
            cycle_exclude_radius(p);

            _reset_travel_colours(features, on_level);
            break;
        }

        case CMD_MAP_CLEAR_EXCLUDES:
            clear_excludes();
            _reset_travel_colours(features, on_level);
            break;

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
            int best_score = 1000;
            level_id best_level;

            int cur_depth = level_id::current().dungeon_absdepth();

            for (level_id_set::iterator it = Generated_Levels.begin();
                    it != Generated_Levels.end(); ++it)
            {
                int depth = it->dungeon_absdepth();

                int score = (depth - cur_depth) * 2;

                if (cmd == CMD_MAP_PREV_LEVEL)
                    score *= -1;

                if (score <= 0)
                    continue;

                if (it->branch == you.where_are_you
                        && it->level_type == you.level_type)
                {
                    --score;
                }

                if (score < best_score)
                {
                    best_score = score;
                    best_level = *it;
                }
            }

            if (best_score != 1000)
            {
                le.go_to(best_level);
                new_level = true;
            }
            break;
        }

        case CMD_MAP_OTHER_BRANCH: {
            level_id_set::iterator it = Generated_Levels.begin();

            while (*it != level_id::current())
            {
                ++it;

                if (it != Generated_Levels.end())
                    goto no_depth_category;
            }

            do
            {
                ++it;

                if (it == Generated_Levels.end())
                    it = Generated_Levels.begin();
            }
            while (it->dungeon_absdepth() != level_id::current().dungeon_absdepth());

            if (*it != level_id::current())
            {
                le.go_to(*it);
                new_level = true;
            }
        no_depth_category:
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

// We logically associate a difficulty parameter with each tile on each level,
// to make deterministic magic mapping work.  This function returns the
// difficulty parameters for each tile on the current level, whose difficulty
// is less than a certain amount.
//
// Random difficulties are used in the few cases where we want repeated maps
// to give different results; scrolls and cards, since they are a finite
// resource.
static const FixedArray<char, GXM, GYM>& _tile_difficulties(bool random)
{
    // We will often be called with the same level parameter and cutoff, so
    // cache this (DS with passive mapping autoexploring could be 5000 calls
    // in a second or so).
    static FixedArray<char, GXM, GYM> cache;
    static int cache_seed = -1;

    int seed = random ? -1 :
        (static_cast<int>(you.where_are_you) << 8) + you.your_level - 1731813538;

    if (seed == cache_seed && !random)
    {
        return cache;
    }

    if (!random)
    {
        push_rng_state();
        seed_rng(cache_seed);
    }

    cache_seed = seed;

    for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
            cache[x][y] = random2(100);

    if (!random)
    {
        pop_rng_state();
    }

    return cache;
}

static std::auto_ptr<FixedArray<bool, GXM, GYM> > _tile_detectability()
{
    std::auto_ptr<FixedArray<bool, GXM, GYM> > map(new FixedArray<bool, GXM, GYM>);

    std::vector<coord_def> flood_from;

    for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
        for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        {
            (*map)(coord_def(x,y)) = false;

            if (feat_is_stair(grd[x][y]))
            {
                flood_from.push_back(coord_def(x, y));
            }
        }

    flood_from.push_back(you.pos());

    while (!flood_from.empty())
    {
        coord_def p = flood_from.back();
        flood_from.pop_back();

        if ((*map)(p))
            continue;

        (*map)(p) = true;

        if (grd(p) < DNGN_MINSEE && grd(p) != DNGN_CLOSED_DOOR)
            continue;

        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                flood_from.push_back(p + coord_def(dx,dy));
    }

    return map;
}

// Returns true if it succeeded.
bool magic_mapping(int map_radius, int proportion, bool suppress_msg,
                   bool force, bool deterministic, bool circular,
                   coord_def pos)
{
    if (!in_bounds(pos))
        pos = you.pos();

    if (!force
        && (testbits(env.level_flags, LFLAG_NO_MAGIC_MAP)
            || testbits(get_branch_flags(), BFLAG_NO_MAGIC_MAP)))
    {
        if (!suppress_msg)
            mpr("You feel momentarily disoriented.");

        return (false);
    }

    const bool wizard_map = (you.wizard && map_radius == 1000);

    if (!wizard_map)
    {
        if (map_radius > 50)
            map_radius = 50;
        else if (map_radius < 5)
            map_radius = 5;
    }

    // now gradually weaker with distance:
    const int pfar     = (map_radius * 7) / 10;
    const int very_far = (map_radius * 9) / 10;

    bool did_map = false;
    int  num_altars        = 0;
    int  num_shops_portals = 0;

    const FixedArray<char, GXM, GYM>& difficulty =
        _tile_difficulties(!deterministic);

    std::auto_ptr<FixedArray<bool, GXM, GYM> > detectable;

    if (!deterministic)
        detectable = _tile_detectability();

    for (radius_iterator ri(pos, map_radius, !circular, false); ri; ++ri)
    {
        if (!wizard_map)
        {
            int threshold = proportion;

            const int dist = grid_distance( you.pos(), *ri );

            if (dist > very_far)
                threshold = threshold / 3;
            else if (dist > pfar)
                threshold = threshold * 2 / 3;

            if (difficulty(*ri) > threshold)
                continue;
        }

        if (is_terrain_changed(*ri))
            clear_envmap_grid(*ri);

        if (!wizard_map && (is_terrain_seen(*ri) || is_terrain_mapped(*ri)))
            continue;

        if (!wizard_map && !deterministic && !((*detectable)(*ri)))
            continue;

        const dungeon_feature_type feat = grd(*ri);

        bool open = true;

        if (feat_is_solid(feat) && !feat_is_closed_door(feat))
        {
            open = false;
            for (adjacent_iterator ai(*ri); ai; ++ai)
            {
                if (map_bounds(*ai) && (!feat_is_opaque(grd(*ai))
                                        || feat_is_closed_door(grd(*ai))))
                {
                    open = true;
                    break;
                }
            }
        }

        if (open)
        {
            if (wizard_map || !get_envmap_obj(*ri))
                set_envmap_obj(*ri, grd(*ri));

            if (wizard_map)
            {
                if (is_notable_terrain(feat))
                    seen_notable_thing(feat, *ri);

                set_terrain_seen(*ri);
#ifdef USE_TILE
                // Can't use set_envmap_obj because that would
                // overwrite the gmap.
                env.tile_bk_bg(*ri) = tile_idx_unseen_terrain(ri->x, ri->y,
                                                              grd(*ri));
#endif
            }
            else
            {
                set_terrain_mapped(*ri);

                if (get_feature_dchar(feat) == DCHAR_ALTAR)
                    num_altars++;
                else if (get_feature_dchar(feat) == DCHAR_ARCH)
                    num_shops_portals++;
            }

            did_map = true;
        }
    }

    if (!suppress_msg)
    {
        mpr(did_map ? "You feel aware of your surroundings."
                    : "You feel momentarily disoriented.");

        std::vector<std::string> sensed;

        if (num_altars > 0)
            sensed.push_back(make_stringf("%d altar%s", num_altars,
                                          num_altars > 1 ? "s" : ""));

        if (num_shops_portals > 0)
        {
            const char* plur = num_shops_portals > 1 ? "s" : "";
            sensed.push_back(make_stringf("%d shop%s/portal%s",
                                          num_shops_portals, plur, plur));
        }

        if (!sensed.empty())
            mpr_comma_separated_list("You sensed ", sensed);
    }

    return (did_map);
}

// Is the given monster near (in LOS of) the player?
bool mons_near(const monsters *monster)
{
    if (crawl_state.arena || crawl_state.arena_suspended)
        return (true);
    return (see_cell(monster->pos()));
}

bool mon_enemies_around(const monsters *monster)
{
    // If the monster has a foe, return true.
    if (monster->foe != MHITNOT && monster->foe != MHITYOU)
        return (true);

    if (crawl_state.arena)
    {
        // If the arena-mode code in _handle_behaviour() hasn't set a foe then
        // we don't have one.
        return (false);
    }
    else if (mons_wont_attack(monster))
    {
        // Additionally, if an ally is nearby and *you* have a foe,
        // consider it as the ally's enemy too.
        return (mons_near(monster) && there_are_monsters_nearby(true));
    }
    else
    {
        // For hostile monsters *you* are the main enemy.
        return (mons_near(monster));
    }
}

// For order and meaning of symbols, see dungeon_char_type in enum.h.
static const unsigned dchar_table[ NUM_CSET ][ NUM_DCHAR_TYPES ] =
{
    // CSET_ASCII
    {
        '#', '*', '.', ',', '\'', '+', '^', '>', '<',  // wall .. stairs up
        '_', '\\', '}', '{', '8', '~', '~',            // altar .. item detect
        '0', ')', '[', '/', '%', '?', '=', '!', '(',   // orb .. missile
        ':', '|', '}', '%', '$', '"', '#', '7',        // book .. trees
        ' ', '!', '#', '%', ':', ')', '*', '+',        // space .. fired_burst
        '/', '=', '?', 'X', '[', '`', '#'              // fi_stick .. explosion
    },

    // CSET_IBM - this is ANSI 437
    {
        177, 176, 249, 250, '\'', 254, '^', '>', '<',  // wall .. stairs up
        220, 239, 244, 247, '8', '~', '~',             // altar .. item detect
        '0', ')', '[', '/', '%', '?', '=', '!', '(',   // orb .. missile
        '+', '\\', '}', '%', '$', '"', '#', 234,       // book .. trees
        ' ', '!', '#', '%', '+', ')', '*', '+',        // space .. fired_burst
        '/', '=', '?', 'X', '[', '`', '#'              // fi_stick .. explosion
    },

    // CSET_DEC - remember: 224-255 are mapped to shifted 96-127
    {
        225, 224, 254, ':', '\'', 238, '^', '>', '<',  // wall .. stairs up
        251, 182, 167, 187, '8', 171, 168,             // altar .. item detect
        '0', ')', '[', '/', '%', '?', '=', '!', '(',   // orb .. missile
        '+', '\\', '}', '%', '$', '"', '#', '7',       // book .. trees
        ' ', '!', '#', '%', '+', ')', '*', '+',        // space .. fired_burst
        '/', '=', '?', 'X', '[', '`', '#'              // fi_stick .. explosion
    },

    // CSET_UNICODE
    {
        0x2592, 0x2591, 0xB7, 0x25E6, '\'', 0x25FC, '^', '>', '<',
        '_', 0x2229, 0x2320, 0x2248, '8', '~', '~',
        '0', ')', '[', '/', '%', '?', '=', '!', '(',
        '+', '|', '}', '%', '$', '"', '#', 0x2663,
        ' ', '!', '#', '%', '+', ')', '*', '+',        // space .. fired_burst
        '/', '=', '?', 'X', '[', '`', '#'              // fi_stick .. explosion
    },
};

dungeon_char_type dchar_by_name(const std::string &name)
{
    const char *dchar_names[] =
    {
        "wall", "wall_magic", "floor", "floor_magic", "door_open",
        "door_closed", "trap", "stairs_down", "stairs_up", "altar", "arch",
        "fountain", "wavy", "statue", "invis_exposed", "item_detected",
        "item_orb", "item_weapon", "item_armour", "item_wand", "item_food",
        "item_scroll", "item_ring", "item_potion", "item_missile", "item_book",
        "item_stave", "item_miscellany", "item_corpse", "item_gold",
        "item_amulet", "cloud", "trees",
    };

    for (unsigned i = 0; i < sizeof(dchar_names) / sizeof(*dchar_names); ++i)
        if (dchar_names[i] == name)
            return dungeon_char_type(i);

    return (NUM_DCHAR_TYPES);
}

void init_char_table( char_set_type set )
{
    for (int i = 0; i < NUM_DCHAR_TYPES; i++)
    {
        if (Options.cset_override[set][i])
            Options.char_table[i] = Options.cset_override[set][i];
        else
            Options.char_table[i] = dchar_table[set][i];
    }
}

unsigned dchar_glyph(dungeon_char_type dchar)
{
    return (Options.char_table[dchar]);
}

void apply_feature_overrides()
{
    for (int i = 0, size = Options.feature_overrides.size(); i < size; ++i)
    {
        const feature_override      &fov    = Options.feature_overrides[i];
        const feature_def           &ofeat  = fov.override;
        feature_def                 &feat   = Feature[fov.feat];

        if (ofeat.symbol)
            feat.symbol = ofeat.symbol;
        if (ofeat.magic_symbol)
            feat.magic_symbol = ofeat.magic_symbol;
        if (ofeat.colour)
            feat.colour = ofeat.colour;
        if (ofeat.map_colour)
            feat.map_colour = ofeat.map_colour;
        if (ofeat.seen_colour)
            feat.seen_colour = ofeat.seen_colour;
        if (ofeat.seen_em_colour)
            feat.seen_em_colour = ofeat.seen_em_colour;
        if (ofeat.em_colour)
            feat.em_colour = ofeat.em_colour;
    }
}

void init_feature_table( void )
{
    for (int i = 0; i < NUM_FEATURES; i++)
    {
        Feature[i].dchar          = NUM_DCHAR_TYPES;
        Feature[i].symbol         = 0;
        Feature[i].colour         = BLACK;   // means must be set some other way
        Feature[i].flags          = FFT_NONE;
        Feature[i].magic_symbol   = 0;       // set to symbol if unchanged
        Feature[i].map_colour     = DARKGREY;
        Feature[i].seen_colour    = BLACK;   // -> no special seen map handling
        Feature[i].seen_em_colour = BLACK;
        Feature[i].em_colour      = BLACK;
        Feature[i].minimap        = MF_UNSEEN;

        switch (i)
        {
        case DNGN_UNSEEN:
        default:
            break;

        case DNGN_ROCK_WALL:
        case DNGN_PERMAROCK_WALL:
            Feature[i].dchar        = DCHAR_WALL;
            Feature[i].colour       = ETC_ROCK;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            Feature[i].minimap      = MF_WALL;
            break;

        case DNGN_STONE_WALL:
            Feature[i].dchar        = DCHAR_WALL;
            Feature[i].colour       = ETC_STONE;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            Feature[i].minimap      = MF_WALL;
            break;

        case DNGN_CLEAR_ROCK_WALL:
        case DNGN_CLEAR_STONE_WALL:
        case DNGN_CLEAR_PERMAROCK_WALL:
            Feature[i].dchar        = DCHAR_WALL;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            Feature[i].colour       = LIGHTCYAN;
            Feature[i].minimap      = MF_WALL;
            break;

        case DNGN_TREES:
            Feature[i].dchar        = DCHAR_TREES;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            Feature[i].colour       = BLACK; // overridden later
            Feature[i].minimap      = MF_WALL;
            break;

        case DNGN_OPEN_SEA:
#ifdef USE_TILE
            Feature[i].dchar        = DCHAR_WAVY;
#else
            Feature[i].dchar        = DCHAR_WALL;
#endif
            Feature[i].colour       = BLUE;
            Feature[i].minimap      = MF_WATER;
            break;

        case DNGN_OPEN_DOOR:
            Feature[i].dchar   = DCHAR_DOOR_OPEN;
            Feature[i].colour  = LIGHTGREY;
            Feature[i].minimap = MF_DOOR;
            break;

        case DNGN_CLOSED_DOOR:
        case DNGN_DETECTED_SECRET_DOOR:
            Feature[i].dchar   = DCHAR_DOOR_CLOSED;
            Feature[i].colour  = LIGHTGREY;
            Feature[i].minimap = MF_DOOR;
            break;

        case DNGN_METAL_WALL:
            Feature[i].dchar        = DCHAR_WALL;
            Feature[i].colour       = CYAN;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            Feature[i].minimap      = MF_WALL;
            break;

        case DNGN_SECRET_DOOR:
            // Note: get_secret_door_appearance means this probably isn't used.
            Feature[i].dchar        = DCHAR_WALL;
            Feature[i].colour       = ETC_ROCK;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            Feature[i].minimap      = MF_WALL;
            break;

        case DNGN_GREEN_CRYSTAL_WALL:
            Feature[i].dchar        = DCHAR_WALL;
            Feature[i].colour       = GREEN;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            Feature[i].minimap      = MF_WALL;
            break;

        case DNGN_ORCISH_IDOL:
            Feature[i].dchar   = DCHAR_STATUE;
            Feature[i].colour  = BROWN; // same as clay golem, I hope that's okay
            Feature[i].minimap = MF_WALL;
            break;

        case DNGN_WAX_WALL:
            Feature[i].dchar        = DCHAR_WALL;
            Feature[i].colour       = YELLOW;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            Feature[i].minimap      = MF_WALL;
            break;

        case DNGN_GRANITE_STATUE:
            Feature[i].dchar   = DCHAR_STATUE;
            Feature[i].colour  = DARKGREY;
            Feature[i].minimap = MF_WALL;
            break;

        case DNGN_LAVA:
            Feature[i].dchar   = DCHAR_WAVY;
            Feature[i].colour  = RED;
            Feature[i].minimap = MF_LAVA;
            break;

        case DNGN_DEEP_WATER:
            Feature[i].dchar   = DCHAR_WAVY;
            Feature[i].colour  = BLUE;
            Feature[i].minimap = MF_WATER;
            break;

        case DNGN_SHALLOW_WATER:
            Feature[i].dchar   = DCHAR_WAVY;
            Feature[i].colour  = CYAN;
            Feature[i].minimap = MF_WATER;
            break;

        case DNGN_FLOOR:
            Feature[i].dchar        = DCHAR_FLOOR;
            Feature[i].colour       = ETC_FLOOR;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_FLOOR_MAGIC ];
            Feature[i].minimap      = MF_FLOOR;
            break;

        case DNGN_FLOOR_SPECIAL:
            Feature[i].dchar        = DCHAR_FLOOR;
            Feature[i].colour       = YELLOW;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_FLOOR_MAGIC ];
            Feature[i].minimap      = MF_FLOOR;
            break;

        case DNGN_EXIT_HELL:
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].colour      = LIGHTRED;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = LIGHTRED;
            Feature[i].minimap     = MF_STAIR_UP;
            break;

        case DNGN_ENTER_HELL:
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].colour      = RED;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = RED;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_TRAP_MECHANICAL:
            Feature[i].colour     = LIGHTCYAN;
            Feature[i].dchar      = DCHAR_TRAP;
            Feature[i].map_colour = LIGHTCYAN;
            Feature[i].minimap    = MF_TRAP;
            break;

        case DNGN_TRAP_MAGICAL:
            Feature[i].colour     = MAGENTA;
            Feature[i].dchar      = DCHAR_TRAP;
            Feature[i].map_colour = MAGENTA;
            Feature[i].minimap    = MF_TRAP;
            break;

        case DNGN_TRAP_NATURAL:
            Feature[i].colour     = BROWN;
            Feature[i].dchar      = DCHAR_TRAP;
            Feature[i].map_colour = BROWN;
            Feature[i].minimap    = MF_TRAP;
            break;

        case DNGN_UNDISCOVERED_TRAP:
            Feature[i].dchar        = DCHAR_FLOOR;
            Feature[i].colour       = ETC_FLOOR;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_FLOOR_MAGIC ];
            Feature[i].minimap      = MF_FLOOR;
            break;

        case DNGN_ENTER_SHOP:
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].colour      = YELLOW;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = YELLOW;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ABANDONED_SHOP:
            Feature[i].colour     = LIGHTGREY;
            Feature[i].dchar      = DCHAR_ARCH;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].minimap    = MF_FLOOR;
            break;

        case DNGN_ENTER_LABYRINTH:
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].colour      = CYAN;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = CYAN;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ENTER_PORTAL_VAULT:
            Feature[i].flags |= FFT_NOTABLE;
            // fall through

        case DNGN_EXIT_PORTAL_VAULT:
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].colour      = ETC_SHIMMER_BLUE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = ETC_SHIMMER_BLUE;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ESCAPE_HATCH_DOWN:
            Feature[i].dchar      = DCHAR_STAIRS_DOWN;
            Feature[i].colour     = BROWN;
            Feature[i].map_colour = BROWN;
            Feature[i].minimap    = MF_STAIR_DOWN;
            break;

        case DNGN_STONE_STAIRS_DOWN_I:
        case DNGN_STONE_STAIRS_DOWN_II:
        case DNGN_STONE_STAIRS_DOWN_III:
            Feature[i].dchar          = DCHAR_STAIRS_DOWN;
            Feature[i].colour         = LIGHTGREY;
            Feature[i].em_colour      = WHITE;
            Feature[i].map_colour     = RED;
            Feature[i].seen_em_colour = WHITE;
            Feature[i].minimap        = MF_STAIR_DOWN;
            break;

        case DNGN_ESCAPE_HATCH_UP:
            Feature[i].dchar      = DCHAR_STAIRS_UP;
            Feature[i].colour     = BROWN;
            Feature[i].map_colour = BROWN;
            Feature[i].minimap    = MF_STAIR_UP;
            break;

        case DNGN_STONE_STAIRS_UP_I:
        case DNGN_STONE_STAIRS_UP_II:
        case DNGN_STONE_STAIRS_UP_III:
            Feature[i].dchar          = DCHAR_STAIRS_UP;
            Feature[i].colour         = LIGHTGREY;
            Feature[i].map_colour     = GREEN;
            Feature[i].em_colour      = WHITE;
            Feature[i].seen_em_colour = WHITE;
            Feature[i].minimap        = MF_STAIR_UP;
            break;

        case DNGN_ENTER_DIS:
            Feature[i].colour      = CYAN;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = CYAN;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ENTER_GEHENNA:
            Feature[i].colour      = RED;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = RED;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ENTER_COCYTUS:
            Feature[i].colour      = LIGHTCYAN;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = LIGHTCYAN;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ENTER_TARTARUS:
            Feature[i].colour      = DARKGREY;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = DARKGREY;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ENTER_ABYSS:
            Feature[i].colour      = ETC_RANDOM;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = ETC_RANDOM;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_EXIT_ABYSS:
            Feature[i].colour     = ETC_RANDOM;
            Feature[i].dchar      = DCHAR_ARCH;
            Feature[i].map_colour = ETC_RANDOM;
            Feature[i].minimap    = MF_STAIR_BRANCH;
            break;

        case DNGN_STONE_ARCH:
            Feature[i].colour     = LIGHTGREY;
            Feature[i].dchar      = DCHAR_ARCH;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].minimap    = MF_FLOOR;
            break;

        case DNGN_ENTER_PANDEMONIUM:
            Feature[i].colour      = LIGHTBLUE;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = LIGHTBLUE;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_EXIT_PANDEMONIUM:
            // Note: Has special handling for colouring with mutation.
            Feature[i].colour      = LIGHTBLUE;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = LIGHTBLUE;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_TRANSIT_PANDEMONIUM:
            Feature[i].colour      = LIGHTGREEN;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = LIGHTGREEN;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

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
            Feature[i].colour      = YELLOW;
            Feature[i].dchar       = DCHAR_STAIRS_DOWN;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = RED;
            Feature[i].seen_colour = YELLOW;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ENTER_ZOT:
            Feature[i].colour      = MAGENTA;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = MAGENTA;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

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
            Feature[i].colour      = YELLOW;
            Feature[i].dchar       = DCHAR_STAIRS_UP;
            Feature[i].map_colour  = GREEN;
            Feature[i].seen_colour = YELLOW;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_RETURN_FROM_ZOT:
            Feature[i].colour      = MAGENTA;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = MAGENTA;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ALTAR_ZIN:
            Feature[i].colour      = WHITE;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = WHITE;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_SHINING_ONE:
            Feature[i].colour      = YELLOW;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = YELLOW;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_KIKUBAAQUDGHA:
            Feature[i].colour      = DARKGREY;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = DARKGREY;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_YREDELEMNUL:
            Feature[i].colour      = ETC_UNHOLY;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = ETC_UNHOLY;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_XOM:
            Feature[i].colour      = ETC_RANDOM;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = ETC_RANDOM;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_VEHUMET:
            Feature[i].colour      = ETC_VEHUMET;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = ETC_VEHUMET;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_OKAWARU:
            Feature[i].colour      = CYAN;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = CYAN;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_MAKHLEB:
            Feature[i].colour      = ETC_FIRE;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = ETC_FIRE;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_SIF_MUNA:
            Feature[i].colour      = BLUE;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = BLUE;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_TROG:
            Feature[i].colour      = RED;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = RED;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_NEMELEX_XOBEH:
            Feature[i].colour      = LIGHTMAGENTA;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = LIGHTMAGENTA;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_ELYVILON:
            Feature[i].colour      = LIGHTGREY;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = LIGHTGREY;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_LUGONU:
            Feature[i].colour      = MAGENTA;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = MAGENTA;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_BEOGH:
            Feature[i].colour      = ETC_BEOGH;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = ETC_BEOGH;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_JIYVA:
            Feature[i].colour      = ETC_SLIME;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = ETC_SLIME;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_FEAWN:
            Feature[i].colour      = GREEN;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = GREEN;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_CHEIBRIADOS:
            Feature[i].colour      = LIGHTCYAN;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = LIGHTCYAN;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_FOUNTAIN_BLUE:
            Feature[i].colour  = BLUE;
            Feature[i].dchar   = DCHAR_FOUNTAIN;
            Feature[i].minimap = MF_FEATURE;
            break;

        case DNGN_FOUNTAIN_SPARKLING:
            Feature[i].colour  = LIGHTBLUE;
            Feature[i].dchar   = DCHAR_FOUNTAIN;
            Feature[i].minimap = MF_FEATURE;
            break;

        case DNGN_FOUNTAIN_BLOOD:
            Feature[i].colour  = RED;
            Feature[i].dchar   = DCHAR_FOUNTAIN;
            Feature[i].minimap = MF_FEATURE;
            break;

        case DNGN_DRY_FOUNTAIN_BLUE:
        case DNGN_DRY_FOUNTAIN_SPARKLING:
        case DNGN_DRY_FOUNTAIN_BLOOD:
        case DNGN_PERMADRY_FOUNTAIN:
            Feature[i].colour  = LIGHTGREY;
            Feature[i].dchar   = DCHAR_FOUNTAIN;
            Feature[i].minimap = MF_FEATURE;
            break;

        case DNGN_INVIS_EXPOSED:
            Feature[i].dchar   = DCHAR_INVIS_EXPOSED;
            Feature[i].minimap = MF_MONS_HOSTILE;
            break;

        case DNGN_ITEM_DETECTED:
            Feature[i].dchar   = DCHAR_ITEM_DETECTED;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_ORB:
            Feature[i].dchar   = DCHAR_ITEM_ORB;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_WEAPON:
            Feature[i].dchar   = DCHAR_ITEM_WEAPON;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_ARMOUR:
            Feature[i].dchar   = DCHAR_ITEM_ARMOUR;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_WAND:
            Feature[i].dchar   = DCHAR_ITEM_WAND;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_FOOD:
            Feature[i].dchar   = DCHAR_ITEM_FOOD;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_SCROLL:
            Feature[i].dchar   = DCHAR_ITEM_SCROLL;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_RING:
            Feature[i].dchar   = DCHAR_ITEM_RING;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_POTION:
            Feature[i].dchar   = DCHAR_ITEM_POTION;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_MISSILE:
            Feature[i].dchar   = DCHAR_ITEM_MISSILE;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_BOOK:
            Feature[i].dchar   = DCHAR_ITEM_BOOK;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_STAVE:
            Feature[i].dchar   = DCHAR_ITEM_STAVE;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_MISCELLANY:
            Feature[i].dchar   = DCHAR_ITEM_MISCELLANY;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_CORPSE:
            Feature[i].dchar   = DCHAR_ITEM_CORPSE;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_GOLD:
            Feature[i].dchar   = DCHAR_ITEM_GOLD;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_ITEM_AMULET:
            Feature[i].dchar   = DCHAR_ITEM_AMULET;
            Feature[i].minimap = MF_ITEM;
            break;

        case DNGN_CLOUD:
            Feature[i].dchar   = DCHAR_CLOUD;
            Feature[i].minimap = MF_SKIP;
            break;
        }

        if (i == DNGN_ENTER_ORCISH_MINES || i == DNGN_ENTER_SLIME_PITS
            || i == DNGN_ENTER_LABYRINTH)
        {
            Feature[i].flags |= FFT_EXAMINE_HINT;
        }

        if (Feature[i].dchar != NUM_DCHAR_TYPES)
            Feature[i].symbol = Options.char_table[ Feature[i].dchar ];
    }

    apply_feature_overrides();

    for (int i = 0; i < NUM_FEATURES; ++i)
    {
        feature_def &f(Feature[i]);

        if (!f.magic_symbol)
            f.magic_symbol = f.symbol;

        if (f.seen_colour == BLACK)
            f.seen_colour = f.map_colour;

        if (f.seen_em_colour == BLACK)
            f.seen_em_colour = f.seen_colour;

        if (f.em_colour == BLACK)
            f.em_colour = f.colour;
    }
}

unsigned get_screen_glyph( int x, int y )
{
    return get_screen_glyph(coord_def(x,y));
}

unsigned get_screen_glyph( const coord_def& p )
{
    const coord_def ep = view2show(grid2view(p));

    int             object = show_appearance(ep);
    unsigned short  colour = env.show_col(ep);
    unsigned        ch;

    if (!object)
        return get_envmap_char(p.x, p.y);

    _get_symbol( p, object, &ch, &colour );
    return (ch);
}

std::string stringize_glyph(unsigned glyph)
{
    if (crawl_state.glyph2strfn && Options.char_set == CSET_UNICODE)
        return (*crawl_state.glyph2strfn)(glyph);

    return (std::string(1, glyph));
}

int multibyte_strlen(const std::string &s)
{
    if (crawl_state.multibyte_strlen)
        return (*crawl_state.multibyte_strlen)(s);

    return (s.length());
}

// Returns a string containing an ASCII representation of the map. If fullscreen
// is set to false, only the viewable area is returned. Leading and trailing
// spaces are trimmed from each line. Leading and trailing empty lines are also
// snipped.
std::string screenshot( bool fullscreen )
{
    UNUSED( fullscreen );

    // [ds] Screenshots need to be straight ASCII. We will now proceed to force
    // the char and feature tables back to ASCII.
    FixedVector<unsigned, NUM_DCHAR_TYPES> char_table_bk;
    char_table_bk = Options.char_table;

    init_char_table(CSET_ASCII);
    init_feature_table();

    int firstnonspace = -1;
    int firstpopline  = -1;
    int lastpopline   = -1;

    std::vector<std::string> lines(crawl_view.viewsz.y);
    for (int count_y = 1; count_y <= crawl_view.viewsz.y; count_y++)
    {
        int lastnonspace = -1;

        for (int count_x = 1; count_x <= crawl_view.viewsz.x; count_x++)
        {
            // in grid coords
            const coord_def gc = view2grid(coord_def(count_x, count_y));

            int ch =
                  (!map_bounds(gc))             ? 0 :
                  (!crawl_view.in_grid_los(gc)) ? get_envmap_char(gc.x, gc.y) :
                  (gc == you.pos())             ? you.symbol
                                                : get_screen_glyph(gc.x, gc.y);

            if (ch && !isprint(ch))
            {
                // [ds] Evil hack time again. Peek at grid, use that character.
                int object = grid_appearance(gc);
                unsigned glych;
                unsigned short glycol = 0;

                _get_symbol( gc, object, &glych, &glycol );
                ch = glych;
            }

            // More mangling to accommodate C strings.
            if (!ch)
                ch = ' ';

            if (ch != ' ')
            {
                lastnonspace = count_x;
                lastpopline = count_y;

                if (firstnonspace == -1 || firstnonspace > count_x)
                    firstnonspace = count_x;

                if (firstpopline == -1)
                    firstpopline = count_y;
            }

            lines[count_y - 1] += ch;
        }

        if (lastnonspace < (int) lines[count_y - 1].length())
            lines[count_y - 1].erase(lastnonspace + 1);
    }

    // Restore char and feature tables.
    Options.char_table = char_table_bk;
    init_feature_table();

    std::ostringstream ss;
    if (firstpopline != -1 && lastpopline != -1)
    {
        if (firstnonspace == -1)
            firstnonspace = 0;

        for (int i = firstpopline; i <= lastpopline; ++i)
        {
            const std::string &ref = lines[i - 1];
            if (firstnonspace < (int) ref.length())
                ss << ref.substr(firstnonspace);
            ss << EOL;
        }
    }

    return (ss.str());
}

static int _viewmap_flash_colour()
{
    if (you.attribute[ATTR_SHADOWS])
        return (DARKGREY);
    else if (you.duration[DUR_BERSERKER])
        return (RED);

    return (BLACK);
}

static void _update_env_show(const coord_def &gp, const coord_def &ep)
{
    // The sequence is grid, items, clouds, monsters.
    env.show(ep) = grd(gp);
    env.show_col(ep) = 0;

    if (igrd(gp) != NON_ITEM)
        _update_item_grid(gp, ep);

    const int cloud = env.cgrid(gp);
    if (cloud != EMPTY_CLOUD && env.cloud[cloud].type != CLOUD_NONE
        && env.cloud[cloud].pos == gp)
    {
        _update_cloud_grid(cloud);
    }

    const monsters *mons = monster_at(gp);
    if (mons && mons->alive())
        _update_monster_grid(mons);
}

// Updates one square of the view area. Should only be called for square
// in LOS.
void view_update_at(const coord_def &pos)
{
    if (pos == you.pos())
        return;

    const coord_def vp = grid2view(pos);
    const coord_def ep = view2show(vp);
    _update_env_show(pos, ep);

    int object = show_appearance(ep);

    if (!object)
        return;

    unsigned short  colour = env.show_col(ep);
    unsigned        ch = 0;

    _get_symbol( pos, object, &ch, &colour );

    int flash_colour = you.flash_colour;
    if (flash_colour == BLACK)
        flash_colour = _viewmap_flash_colour();

#ifndef USE_TILE
    cgotoxy(vp.x, vp.y);
    put_colour_ch(flash_colour? real_colour(flash_colour) : colour, ch);

    // Force colour back to normal, else clrscr() will flood screen
    // with this colour on DOS.
    textattr(LIGHTGREY);
#endif
}

#ifndef USE_TILE
void flash_monster_colour(const monsters *mon, unsigned char fmc_colour,
                          int fmc_delay)
{
    if (you.can_see(mon))
    {
        unsigned char old_flash_colour = you.flash_colour;
        coord_def c(mon->pos());

        you.flash_colour = fmc_colour;
        view_update_at(c);

        update_screen();
        delay(fmc_delay);

        you.flash_colour = old_flash_colour;
        view_update_at(c);
        update_screen();
    }
}
#endif

bool view_update()
{
    if (you.num_turns > you.last_view_update)
    {
        viewwindow(true, false);
        return (true);
    }
    return (false);
}

static void _debug_pane_bounds()
{
#if DEBUG_PANE_BOUNDS
    // Doesn't work for HUD because print_stats() overwrites it.
    // To debug HUD, add viewwindow(false,false) at end of _prep_input.

    if (crawl_view.mlistsz.y > 0)
    {
        textcolor(WHITE);
        cgotoxy(1,1, GOTO_MLIST);
        cprintf("+   L");
        cgotoxy(crawl_view.mlistsz.x-4, crawl_view.mlistsz.y, GOTO_MLIST);
        cprintf("L   +");
    }

    cgotoxy(1,1, GOTO_STAT);
    cprintf("+  H");
    cgotoxy(crawl_view.hudsz.x-3, crawl_view.hudsz.y, GOTO_STAT);
    cprintf("H  +");

    cgotoxy(1,1, GOTO_MSG);
    cprintf("+ M");
    cgotoxy(crawl_view.msgsz.x-2, crawl_view.msgsz.y, GOTO_MSG);
    cprintf("M +");

    cgotoxy(crawl_view.viewp.x, crawl_view.viewp.y);
    cprintf("+V");
    cgotoxy(crawl_view.viewp.x+crawl_view.viewsz.x-2,
            crawl_view.viewp.y+crawl_view.viewsz.y-1);
    cprintf("V+");
    textcolor(LIGHTGREY);
#endif
}

//---------------------------------------------------------------
//
// viewwindow -- now unified and rolled into a single pass
//
// Draws the main window using the character set returned
// by get_symbol().
//
// This function should not interfere with the game condition,
// unless do_updates is set (ie. stealth checks for visible
// monsters).
//
//---------------------------------------------------------------
void viewwindow(bool draw_it, bool do_updates)
{
    if (you.duration[DUR_TIME_STEP])
        return;
    flush_prev_message();

#ifdef USE_TILE
    std::vector<unsigned int> tileb(
        crawl_view.viewsz.y * crawl_view.viewsz.x * 2);
    tiles.clear_text_tags(TAG_NAMED_MONSTER);
#endif
    screen_buffer_t *buffy(crawl_view.vbuf);

    int count_x, count_y;

    calc_show_los();

#ifdef USE_TILE
    tile_draw_floor();
    mcache.clear_nonref();
#endif

    env.show_col.init(LIGHTGREY);
    Show_Backup.init(0);

    item_grid();                // Must be done before cloud and monster.
    cloud_grid();
    monster_grid( do_updates );

#ifdef USE_TILE
    tile_draw_rays(true);
    tiles.clear_overlays();
#endif

    if (draw_it)
    {
        cursor_control cs(false);

        const bool map = player_in_mappable_area();
        const bool draw =
#ifdef USE_TILE
            !is_resting() &&
#endif
            (!you.running || Options.travel_delay > -1
             || you.running.is_explore() && Options.explore_delay > -1)
            && !you.asleep();

        int bufcount = 0;

        int flash_colour = you.flash_colour;
        if (flash_colour == BLACK)
            flash_colour = _viewmap_flash_colour();

        std::vector<coord_def> update_excludes;
        for (count_y = crawl_view.viewp.y;
             count_y < crawl_view.viewp.y + crawl_view.viewsz.y; count_y++)
        {
            for (count_x = crawl_view.viewp.x;
                 count_x < crawl_view.viewp.x + crawl_view.viewsz.x; count_x++)
            {
                // in grid coords
                const coord_def gc(view2grid(coord_def(count_x, count_y)));
                const coord_def ep = view2show(grid2view(gc));

                if (in_bounds(gc) && see_cell(gc))
                    maybe_remove_autoexclusion(gc);

                // Print tutorial messages for features in LOS.
                if (Options.tutorial_left
                    && in_bounds(gc)
                    && crawl_view.in_grid_los(gc)
                    && env.show(ep))
                {
                    tutorial_observe_cell(gc);
                }

                // Order is important here.
                if (!map_bounds(gc))
                {
                    // off the map
                    buffy[bufcount]     = 0;
                    buffy[bufcount + 1] = DARKGREY;
#ifdef USE_TILE
                    tileidx_unseen(tileb[bufcount], tileb[bufcount+1], ' ', gc);
#endif
                }
                else if (!crawl_view.in_grid_los(gc))
                {
                    // Outside the env.show area.
                    buffy[bufcount]     = get_envmap_char(gc);
                    buffy[bufcount + 1] = DARKGREY;

                    if (Options.colour_map)
                    {
                        buffy[bufcount + 1] =
                            colour_code_map(gc, Options.item_colour);
                    }

#ifdef USE_TILE
                    unsigned int bg = env.tile_bk_bg(gc);
                    unsigned int fg = env.tile_bk_fg(gc);
                    if (bg == 0 && fg == 0)
                        tileidx_unseen(fg, bg, get_envmap_char(gc), gc);

                    tileb[bufcount]     = fg;
                    tileb[bufcount + 1] = bg | tile_unseen_flag(gc);
#endif
                }
                else if (gc == you.pos() && !crawl_state.arena
                         && !crawl_state.arena_suspended)
                {
                    int             object = env.show(ep);
                    unsigned short  colour = env.show_col(ep);
                    unsigned        ch;
                    _get_symbol(gc, object, &ch, &colour);

                    if (map)
                    {
                        set_envmap_glyph(gc, object, colour);
                        if (is_terrain_changed(gc) || !is_terrain_seen(gc))
                            update_excludes.push_back(gc);

                        set_terrain_seen(gc);
                        set_envmap_detected_mons(gc, false);
                        set_envmap_detected_item(gc, false);
                    }
#ifdef USE_TILE
                    if (map)
                    {
                        env.tile_bk_bg(gc) = env.tile_bg(ep);
                        env.tile_bk_fg(gc) = env.tile_fg(ep);
                    }

                    tileb[bufcount] = env.tile_fg(ep) =
                        tileidx_player(you.char_class);
                    tileb[bufcount+1] = env.tile_bg(ep);
#endif

                    // Player overrides everything in cell.
                    buffy[bufcount]     = you.symbol;
                    buffy[bufcount + 1] = you.colour;

                    if (player_is_swimming())
                    {
                        if (grd(gc) == DNGN_DEEP_WATER)
                            buffy[bufcount + 1] = BLUE;
                        else
                            buffy[bufcount + 1] = CYAN;
                    }
                }
                else
                {
                    int             object = show_appearance(ep);
                    unsigned short  colour = env.show_col(ep);
                    unsigned        ch;

                    _get_symbol( gc, object, &ch, &colour );

                    buffy[bufcount]     = ch;
                    buffy[bufcount + 1] = colour;
#ifdef USE_TILE
                    tileb[bufcount]   = env.tile_fg(ep);
                    tileb[bufcount+1] = env.tile_bg(ep);
#endif

                    if (map)
                    {
                        // This section is very tricky because it
                        // duplicates the old code (which was horrid).

                        // If the grid is in LoS env.show was set and
                        // we set the buffer already, so...
                        if (buffy[bufcount] != 0)
                        {
                            // ... map that we've seen this
                            if (is_terrain_changed(gc) || !is_terrain_seen(gc))
                                update_excludes.push_back(gc);

                            set_terrain_seen(gc);
                            set_envmap_glyph(gc, object, colour );
                            set_envmap_detected_mons(gc, false);
                            set_envmap_detected_item(gc, false);
#ifdef USE_TILE
                            // We remove any references to mcache when
                            // writing to the background.
                            if (Options.clean_map)
                            {
                                env.tile_bk_fg(gc) =
                                    get_clean_map_idx(env.tile_fg(ep));
                            }
                            else
                            {
                                env.tile_bk_fg(gc) = env.tile_fg(ep);
                            }
                            env.tile_bk_bg(gc) = env.tile_bg(ep);
#endif
                        }

                        // Check if we're looking to clean_map...
                        // but don't touch the buffer to clean it,
                        // instead we modify the env.map itself so
                        // that the map stays clean as it moves out
                        // of the env.show radius.
                        //
                        // Note: show_backup is 0 on every square which
                        // is inside the env.show radius and doesn't
                        // have a monster or cloud on it, and is equal
                        // to the grid before monsters and clouds were
                        // added otherwise.
                        if (Options.clean_map
                            && Show_Backup(ep)
                            && is_terrain_seen(gc))
                        {
                            _get_symbol(gc, Show_Backup(ep), &ch, &colour);
                            set_envmap_glyph(gc, Show_Backup(ep), colour);
                        }

                        // Now we get to filling in both the unseen
                        // grids in the env.show radius area as
                        // well doing the clean_map.  The clean_map
                        // is done by having the env.map set to the
                        // backup character above, and down here we
                        // procede to override that character if it's
                        // out of LoS!  If it wasn't, buffy would have
                        // already been set (but we'd still have
                        // clobbered env.map... which is important
                        // to do for when we move away from the area!)
                        if (buffy[bufcount] == 0)
                        {
                            // Show map.
                            buffy[bufcount]     = get_envmap_char(gc);
                            buffy[bufcount + 1] = DARKGREY;

                            if (Options.colour_map)
                            {
                                buffy[bufcount + 1] =
                                    colour_code_map(gc, Options.item_colour);
                            }
#ifdef USE_TILE
                            if (env.tile_bk_fg(gc) != 0
                                || env.tile_bk_bg(gc) != 0)
                            {
                                tileb[bufcount] = env.tile_bk_fg(gc);

                                tileb[bufcount + 1] =
                                    env.tile_bk_bg(gc) | tile_unseen_flag(gc);
                            }
                            else
                            {
                                tileidx_unseen(tileb[bufcount],
                                               tileb[bufcount+1],
                                               get_envmap_char(gc),
                                               gc);
                            }
#endif
                        }
                    }
                }

                // Alter colour if flashing the characters vision.
                if (flash_colour && buffy[bufcount])
                {
                    buffy[bufcount + 1] =
                        see_cell(gc) ? real_colour(flash_colour)
                                     : DARKGREY;
                }
                else if (Options.target_range > 0 && buffy[bufcount]
                         && (grid_distance(you.pos(), gc) > Options.target_range
                             || !see_cell(gc)))
                {
                    buffy[bufcount + 1] = DARKGREY;
#ifdef USE_TILE
                    if (see_cell(gc))
                        tileb[bufcount + 1] |= TILE_FLAG_OOR;
#endif
                }

                bufcount += 2;
            }
        }

        update_exclusion_los(update_excludes);

        // Leaving it this way because short flashes can occur in long ones,
        // and this simply works without requiring a stack.
        you.flash_colour = BLACK;

        // Avoiding unneeded draws when running.
        if (draw)
        {
#ifdef USE_TILE
            tiles.set_need_redraw();
            tiles.load_dungeon(&tileb[0], crawl_view.vgrdc);
            tiles.update_inventory();
#else
            you.last_view_update = you.num_turns;
            puttext(crawl_view.viewp.x, crawl_view.viewp.y,
                    crawl_view.viewp.x + crawl_view.viewsz.x - 1,
                    crawl_view.viewp.y + crawl_view.viewsz.y - 1,
                    buffy);

            update_monster_pane();
#endif
        }
    }

    _debug_pane_bounds();
}

//////////////////////////////////////////////////////////////////////////////
// crawl_view_buffer

crawl_view_buffer::crawl_view_buffer()
    : buffer(NULL)
{
}

crawl_view_buffer::~crawl_view_buffer()
{
    delete [] buffer;
}

void crawl_view_buffer::size(const coord_def &sz)
{
    delete [] buffer;
    buffer = new screen_buffer_t [ sz.x * sz.y * 2 ];
}

// ----------------------------------------------------------------------
// Layout helper classes
// ----------------------------------------------------------------------

// Moved from directn.h, where they didn't need to be.
// define VIEW_MIN_HEIGHT defined elsewhere
// define VIEW_MAX_HEIGHT use Options.view_max_height
// define VIEW_MIN_WIDTH defined elsewhere
// define VIEW_MAX_WIDTH use Options.view_max_width
#define HUD_WIDTH  42
#define HUD_HEIGHT 12
// #define MSG_MIN_HEIGHT defined elsewhere
#define MSG_MAX_HEIGHT Options.msg_max_height
#define MLIST_MIN_HEIGHT Options.mlist_min_height
#define MLIST_MIN_WIDTH 25  // non-inline layout only
#define MLIST_MAX_WIDTH 42
#define MLIST_GUTTER 1
#define HUD_MIN_GUTTER 2
#define HUD_MAX_GUTTER 4

// Helper for layouts.  Tries to increment lvalue without overflowing it.
static void _increment(int& lvalue, int delta, int max_value)
{
    lvalue = std::min(lvalue+delta, max_value);
}

class _layout
{
 public:
    _layout(coord_def termsz_, coord_def hudsz_) :
        termp(1,1),    termsz(termsz_),
        viewp(-1,-1),  viewsz(VIEW_MIN_WIDTH, VIEW_MIN_HEIGHT),
        hudp(-1,-1),   hudsz(hudsz_),
        msgp(-1,-1),   msgsz(0, MSG_MIN_HEIGHT),
        mlistp(-1,-1), mlistsz(MLIST_MIN_WIDTH, 0),
        hud_gutter(HUD_MIN_GUTTER),
        valid(false) {}

 protected:
    void _assert_validity() const
    {
#ifndef USE_TILE
        // Check that all the panes fit in the view.
        ASSERT( (viewp+viewsz - termp).x <= termsz.x );
        ASSERT( (viewp+viewsz - termp).y <= termsz.y );

        ASSERT( (hudp+hudsz - termp).x <= termsz.x );
        ASSERT( (hudp+hudsz - termp).y <= termsz.y );

        ASSERT( (msgp+msgsz - termp).x <= termsz.x );
        ASSERT( (msgp+msgsz - termp).y <= termsz.y );
        // Don't stretch message all the way to the bottom-right
        // character; it causes scrolling and badness.
        ASSERT( (msgp+msgsz - termp) != termsz );

        ASSERT( (mlistp+mlistsz-termp).x <= termsz.x );
        ASSERT( (mlistp+mlistsz-termp).y <= termsz.y );
#endif
    }
 public:
    const coord_def termp, termsz;
    coord_def viewp, viewsz;
    coord_def hudp;
    const coord_def hudsz;
    coord_def msgp, msgsz;
    coord_def mlistp, mlistsz;
    int hud_gutter;
    bool valid;
};

// vvvvvvghhh  v=view, g=hud gutter, h=hud, l=list, m=msg
// vvvvvvghhh
// vvvvvv lll
//        lll
// mmmmmmmmmm
class _inline_layout : public _layout
{
 public:
    _inline_layout(coord_def termsz_, coord_def hudsz_) :
        _layout(termsz_, hudsz_)
    {
        valid = _init();
    }

    bool _init()
    {
        // x: View gets leftover; then mlist; then hud gutter
        if (leftover_x() < 0)
            return (false);

        _increment(viewsz.x,   leftover_x(), Options.view_max_width);

        if ((viewsz.x % 2) != 1)
            --viewsz.x;

        mlistsz.x = hudsz.x;
        _increment(mlistsz.x,  leftover_x(), MLIST_MAX_WIDTH);
        _increment(hud_gutter, leftover_x(), HUD_MAX_GUTTER);
        _increment(mlistsz.x,  leftover_x(), INT_MAX);
        msgsz.x = termsz.x-1; // Can't use last character.

        // y: View gets as much as it wants.
        // mlist tries to get at least its minimum.
        // msg expands as much as it wants.
        // mlist gets any leftovers.
        if (leftover_y() < 0)
            return (false);

        _increment(viewsz.y, leftover_leftcol_y(), Options.view_max_height);
        if ((viewsz.y % 2) != 1)
            --viewsz.y;

        if (Options.classic_hud)
        {
            mlistsz.y = 0;
            _increment(msgsz.y,  leftover_y(), MSG_MAX_HEIGHT);
        }
        else
        {
            if (mlistsz.y < MLIST_MIN_HEIGHT)
                _increment(mlistsz.y, leftover_rightcol_y(), MLIST_MIN_HEIGHT);
            _increment(msgsz.y,  leftover_y(), MSG_MAX_HEIGHT);
            _increment(mlistsz.y, leftover_rightcol_y(), INT_MAX);
        }

        // Finish off by doing the positions.
        viewp  = termp;
        msgp   = termp + coord_def(0, std::max(viewsz.y, hudsz.y+mlistsz.y));
        hudp   = viewp + coord_def(viewsz.x+hud_gutter, 0);
        mlistp = hudp  + coord_def(0, hudsz.y);

        _assert_validity();
        return (true);
    }

    int leftover_x() const
    {
        int width = (viewsz.x + hud_gutter + std::max(hudsz.x, mlistsz.x));
        return (termsz.x - width);
    }
    int leftover_rightcol_y() const { return termsz.y-hudsz.y-mlistsz.y-msgsz.y; }
    int leftover_leftcol_y() const  { return termsz.y-viewsz.y-msgsz.y; }
    int leftover_y() const
    {
        return std::min(leftover_rightcol_y(), leftover_leftcol_y());
    }
};

// ll vvvvvvghhh  v=view, g=hud gutter, h=hud, l=list, m=msg
// ll vvvvvvghhh
// ll vvvvvv
// mmmmmmmmmmmmm
class _mlist_col_layout : public _layout
{
 public:
    _mlist_col_layout(coord_def termsz_, coord_def hudsz_)
        : _layout(termsz_, hudsz_)
    { valid = _init(); }
    bool _init()
    {
        // Don't let the mlist column steal all the width.  Up front,
        // take some for the view.  If it makes the layout fail, that's fine.
        _increment(viewsz.x, MLIST_MIN_WIDTH/2, Options.view_max_width);

        // x: View and mlist share leftover; then hud gutter.
        if (leftover_x() < 0)
            return (false);

        _increment(mlistsz.x,  leftover_x()/2, MLIST_MAX_WIDTH);
        _increment(viewsz.x,   leftover_x(),   Options.view_max_width);

        if ((viewsz.x % 2) != 1)
            --viewsz.x;

        _increment(mlistsz.x,  leftover_x(),   MLIST_MAX_WIDTH);
        _increment(hud_gutter, leftover_x(),   HUD_MAX_GUTTER);
        msgsz.x = termsz.x-1; // Can't use last character.

        // y: View gets leftover; then message.
        if (leftover_y() < 0)
            return (false);

        _increment(viewsz.y, leftover_y(), Options.view_max_height);

        if ((viewsz.y % 2) != 1)
            --viewsz.y;

        _increment(msgsz.y,  leftover_y(), INT_MAX);
        mlistsz.y = viewsz.y;

        // Finish off by doing the positions.
        mlistp = termp;
        viewp  = mlistp+ coord_def(mlistsz.x+MLIST_GUTTER, 0);
        msgp   = termp + coord_def(0, viewsz.y);
        hudp   = viewp + coord_def(viewsz.x+hud_gutter, 0);

        _assert_validity();
        return (true);
    }
 private:
    int leftover_x() const
    {
        int width = (mlistsz.x + MLIST_GUTTER + viewsz.x + hud_gutter + hudsz.x);
        return (termsz.x - width);
    }
    int leftover_y() const
    {
        const int top_y = std::max(std::max(viewsz.y, hudsz.y), mlistsz.y);
        const int height = top_y + msgsz.y;
        return (termsz.y - height);
    }
};

// ----------------------------------------------------------------------
// crawl_view_geometry
// ----------------------------------------------------------------------

crawl_view_geometry::crawl_view_geometry()
    : termp(1, 1), termsz(80, 24),
      viewp(1, 1), viewsz(33, 17),
      hudp(40, 1), hudsz(-1, -1),
      msgp(1, viewp.y + viewsz.y), msgsz(80, 7),
      mlistp(hudp.x, hudp.y + hudsz.y),
      mlistsz(hudsz.x, msgp.y - mlistp.y),
      vbuf(), vgrdc(), viewhalfsz(), glos1(), glos2(),
      vlos1(), vlos2(), mousep(), last_player_pos()
{
}

void crawl_view_geometry::init_view()
{
    viewhalfsz = viewsz / 2;
    vbuf.size(viewsz);
    set_player_at(you.pos(), true);
}

void crawl_view_geometry::shift_player_to(const coord_def &c)
{
    // Preserve vgrdc offset after moving.
    const coord_def offset = crawl_view.vgrdc - you.pos();
    crawl_view.vgrdc = offset + c;
    last_player_pos = c;

    set_player_at(c);

    ASSERT(crawl_view.vgrdc == offset + c);
    ASSERT(last_player_pos == c);
}

void crawl_view_geometry::set_player_at(const coord_def &c, bool centre)
{
    if (centre)
    {
        vgrdc = c;
    }
    else
    {
        const coord_def oldc = vgrdc;
        const int xmarg = Options.scroll_margin_x + LOS_RADIUS <= viewhalfsz.x
                            ? Options.scroll_margin_x
                            : viewhalfsz.x - LOS_RADIUS;
        const int ymarg = Options.scroll_margin_y + LOS_RADIUS <= viewhalfsz.y
                            ? Options.scroll_margin_y
                            : viewhalfsz.y - LOS_RADIUS;

        if (Options.view_lock_x)
            vgrdc.x = c.x;
        else if (c.x - LOS_RADIUS < vgrdc.x - viewhalfsz.x + xmarg)
            vgrdc.x = c.x - LOS_RADIUS + viewhalfsz.x - xmarg;
        else if (c.x + LOS_RADIUS > vgrdc.x + viewhalfsz.x - xmarg)
            vgrdc.x = c.x + LOS_RADIUS - viewhalfsz.x + xmarg;

        if (Options.view_lock_y)
            vgrdc.y = c.y;
        else if (c.y - LOS_RADIUS < vgrdc.y - viewhalfsz.y + ymarg)
            vgrdc.y = c.y - LOS_RADIUS + viewhalfsz.y - ymarg;
        else if (c.y + LOS_RADIUS > vgrdc.y + viewhalfsz.y - ymarg)
            vgrdc.y = c.y + LOS_RADIUS - viewhalfsz.y + ymarg;

        if (vgrdc != oldc && Options.center_on_scroll)
            vgrdc = c;

        if (!Options.center_on_scroll && Options.symmetric_scroll
            && !Options.view_lock_x
            && !Options.view_lock_y
            && (c - last_player_pos).abs() == 2
            && (vgrdc - oldc).abs() == 1)
        {
            const coord_def dp = c - last_player_pos;
            const coord_def dc = vgrdc - oldc;
            if ((dc.x == dp.x) != (dc.y == dp.y))
                vgrdc = oldc + dp;
        }
    }

    glos1 = c - coord_def(LOS_RADIUS, LOS_RADIUS);
    glos2 = c + coord_def(LOS_RADIUS, LOS_RADIUS);

    vlos1 = glos1 - vgrdc + view_centre();
    vlos2 = glos2 - vgrdc + view_centre();

    last_player_pos = c;
}

void crawl_view_geometry::init_geometry()
{
    termsz = coord_def( get_number_of_cols(), get_number_of_lines() );
    hudsz  = coord_def(HUD_WIDTH,
                       HUD_HEIGHT + (Options.show_gold_turns ? 1 : 0));

    const _inline_layout lay_inline(termsz, hudsz);
    const _mlist_col_layout lay_mlist(termsz, hudsz);

    if (! lay_inline.valid)
    {
#ifndef USE_TILE
        // Terminal too small; exit with an error.
        if (!crawl_state.need_save)
        {
            end(1, false, "Terminal too small (%d,%d); need at least (%d,%d)",
                termsz.x, termsz.y,
                termsz.x + std::max(0, -lay_inline.leftover_x()),
                termsz.y + std::max(0, -lay_inline.leftover_y()));
        }
#endif
    }

    const _layout* winner = &lay_inline;
    if (Options.mlist_allow_alternate_layout
        && !Options.classic_hud
        && lay_mlist.valid)
    {
        winner = &lay_mlist;
    }

    msgp    = winner->msgp;
    msgsz   = winner->msgsz;
    viewp   = winner->viewp;
    viewsz  = winner->viewsz;
    hudp    = winner->hudp;
    hudsz   = winner->hudsz;
    mlistp  = winner->mlistp;
    mlistsz = winner->mlistsz;

#ifdef USE_TILE
    // libgui may redefine these based on its own settings.
    gui_init_view_params(*this);
#endif

    init_view();
    return;
}

////////////////////////////////////////////////////////////////////////////
// Term resize handling (generic).

void handle_terminal_resize(bool redraw)
{
    crawl_state.terminal_resized = false;

    if (crawl_state.terminal_resize_handler)
        (*crawl_state.terminal_resize_handler)();
    else
        crawl_view.init_geometry();

    if (redraw)
        redraw_screen();
}
