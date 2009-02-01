/*
 *  File:       view.cc
 *  Summary:    Misc function used to render the dungeon.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "view.h"

#include <string.h>
#include <cmath>
#include <sstream>
#include <algorithm>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "branch.h"
#include "command.h"
#include "cio.h"
#include "cloud.h"
#include "clua.h"
#include "database.h"
#include "debug.h"
#include "delay.h"
#include "dgnevent.h"
#include "directn.h"
#include "dungeon.h"
#include "format.h"
#include "ghost.h"
#include "initfile.h"
#include "itemprop.h"
#include "luadgn.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mon-util.h"
#include "newgame.h"
#include "notes.h"
#include "output.h"
#include "overmap.h"
#include "place.h"
#include "player.h"
#include "religion.h"
#include "skills.h"
#include "skills2.h"
#include "stuff.h"
#include "spells3.h"
#include "spells4.h"
#include "stash.h"
#include "tiles.h"
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

// Static class members must be initialized outside of the class declaration,
// or gcc won't define them in view.o and we'll get a linking error.
const int monster_los::LSIZE     =  _monster_los_LSIZE;
const int monster_los::L_VISIBLE =  1;
const int monster_los::L_UNKNOWN =  0;
const int monster_los::L_BLOCKED = -1;

static FixedVector<feature_def, NUM_FEATURES> Feature;

crawl_view_geometry crawl_view;
FixedArray < unsigned int, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER > Show_Backup;

extern int stealth;             // defined in acr.cc

screen_buffer_t colour_code_map( const coord_def& p, bool item_colour = false,
                                 bool travel_colour = false );

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

// Used to mark dug out areas, unset when terrain is seen or mapped again.
void set_terrain_changed( int x, int y )
{
    env.map[x][y].flags |= MAP_CHANGED_FLAG;

    dungeon_events.fire_position_event(DET_FEAT_CHANGE, coord_def(x, y));
}

void set_terrain_mapped( int x, int y )
{
    if (!(env.map[x][y].flags & (MAP_MAGIC_MAPPED_FLAG | MAP_SEEN_FLAG))
        && grd[x][y] == DNGN_ENTER_LABYRINTH)
    {
        coord_def pos(x, y);
        take_note(Note(NOTE_SEEN_FEAT, 0, 0,
                       feature_description(pos, false, DESC_NOCAP_A).c_str()));
    }

    env.map[x][y].flags &= (~MAP_CHANGED_FLAG);
    env.map[x][y].flags |= MAP_MAGIC_MAPPED_FLAG;
#ifdef USE_TILE
    tiles.update_minimap(x, y);
#endif
}

void set_terrain_seen( int x, int y )
{
    if (!(env.map[x][y].flags & (MAP_MAGIC_MAPPED_FLAG | MAP_SEEN_FLAG))
        && grd[x][y] == DNGN_ENTER_LABYRINTH)
    {
        coord_def pos(x, y);
        take_note(Note(NOTE_SEEN_FEAT, 0, 0,
                       feature_description(pos, false, DESC_NOCAP_A).c_str()));
    }

    // Magic mapping doesn't reveal the description of the portal vault
    // entrance.
    if (!(env.map[x][y].flags & MAP_SEEN_FLAG)
        && grd[x][y] == DNGN_ENTER_PORTAL_VAULT
        && you.level_type != LEVEL_PORTAL_VAULT)
    {
        coord_def pos(x, y);
        take_note(Note(NOTE_SEEN_FEAT, 0, 0,
                       feature_description(pos, false, DESC_NOCAP_A).c_str()));
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

#if defined(WIN32CONSOLE) || defined(DOS) || defined(USE_TILE)
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

#if defined(WIN32CONSOLE) || defined(DOS) || defined(USE_TILE)
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

static int _view_emphasised_colour(const coord_def& where,
                                   dungeon_feature_type feat,
                                   int oldcolour, int newcolour)
{
    if (is_travelable_stair(feat) && !travel_cache.know_stair(where))
    {
        if ((you.your_level || grid_stair_direction(feat) == CMD_GO_DOWNSTAIRS)
            && you.where_are_you != BRANCH_VESTIBULE_OF_HELL)
        {
            return (newcolour);
        }
    }
    return (oldcolour);
}

static bool _show_bloodcovered(const coord_def& where)
{
    if (!is_bloodcovered(where))
        return (false);

    dungeon_feature_type grid = grd(where);

    // Altars, stairs (of any kind) and traps should not be coloured red.
    return (!is_critical_feature(grid) && !grid_is_trap(grid));
}

static void _get_symbol( const coord_def& where,
                         int object, unsigned *ch,
                         unsigned short *colour,
                         bool magic_mapped )
{
    ASSERT( ch != NULL );

    if (object < NUM_FEATURES)
    {
        const feature_def &fdef = Feature[object];

        *ch = magic_mapped? fdef.magic_symbol
                          : fdef.symbol;

        if (colour)
        {
            const int colmask = *colour & COLFLAG_MASK;

            bool excluded_stairs = (object >= DNGN_STONE_STAIRS_DOWN_I
                                    && object <= DNGN_ESCAPE_HATCH_UP
                                    && is_exclude_root(where));

            bool blocked_movement = false;
            if (!excluded_stairs
                && object < NUM_FEATURES && object >= DNGN_MINMOVE
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
            {
                *colour = Options.tc_excluded | colmask;
            }
            else if (blocked_movement)
            {
                *colour = DARKGREY | colmask;
            }
            else if (object < NUM_REAL_FEATURES && object >= DNGN_MINMOVE
                     && is_sanctuary(where) )
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
            else if (object < NUM_REAL_FEATURES && _show_bloodcovered(where))
            {
                *colour = RED | colmask;
            }
            else if (object < NUM_REAL_FEATURES && env.grid_colours(where))
            {
                *colour = env.grid_colours(where) | colmask;
            }
            else
            {
                // Don't clobber with BLACK, because the colour should be
                // already set.
                if (fdef.colour != BLACK)
                    *colour = fdef.colour | colmask;

                if (fdef.em_colour != fdef.colour && fdef.em_colour)
                {
                    *colour =
                        _view_emphasised_colour(
                            where, static_cast<dungeon_feature_type>(object),
                            *colour, fdef.em_colour | colmask);
                }
            }

            if (object < NUM_REAL_FEATURES && inside_halo(where)
                && (object >= DNGN_FLOOR_MIN && object <= DNGN_FLOOR_MAX
                    || object == DNGN_UNDISCOVERED_TRAP))
            {
                *colour = YELLOW | colmask;
            }
        }

        // Note anything we see that's notable
        if (!where.origin() && fdef.is_notable())
        {
            seen_notable_thing( static_cast<dungeon_feature_type>(object),
                                where );
        }
    }
    else
    {
        ASSERT( object >= DNGN_START_OF_MONSTERS );
        *ch = mons_char( object - DNGN_START_OF_MONSTERS );
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

#if defined(WIN32CONSOLE) || defined(DOS) || defined(USE_TILE)
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
screen_buffer_t colour_code_map( const coord_def& p, bool item_colour,
                                 bool travel_colour )
{
    const unsigned short map_flags = env.map(p).flags;
    if (!(map_flags & MAP_GRID_KNOWN))
        return (BLACK);

#ifdef WIZARD
    if (travel_colour && testbits(env.map(p).property, FPROP_HIGHLIGHT))
        return (LIGHTGREEN);
#endif

    const dungeon_feature_type grid_value = grd(p);

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
    const feature_def &fdef = Feature[grid_value];
    feature_colour = terrain_seen? fdef.seen_colour : fdef.map_colour;

    if (terrain_seen && feature_colour != fdef.seen_em_colour
        && fdef.seen_em_colour)
    {
        feature_colour =
            _view_emphasised_colour(p, grid_value, feature_colour,
                                    fdef.seen_em_colour);
    }

    if (feature_colour != DARKGREY)
        tc = feature_colour;
    else if (you.duration[DUR_MESMERISED])
    {
        // If mesmerised, colour the few grids that can be reached anyway
        // lightgrey.
        if (grd(p) >= DNGN_MINMOVE && mgrd(p) == NON_MONSTER)
        {
            bool blocked_movement = false;
            for (unsigned int i = 0; i < you.mesmerised_by.size(); i++)
            {
                monsters& mon = menv[you.mesmerised_by[i]];
                const int olddist = grid_distance(you.pos(), mon.pos());
                const int newdist = grid_distance(p, mon.pos());

                if (olddist < newdist || !see_grid(env.show, p, mon.pos()))
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
        && is_critical_feature(grid_value)
        && igrd(p) != NON_ITEM)
    {
        tc |= COLFLAG_FEATURE_ITEM;
    }
    else if (Options.trap_item_brand
             && grid_is_trap(grid_value) && igrd(p) != NON_ITEM)
    {
        tc |= COLFLAG_TRAP_ITEM;
    }

    return real_colour(tc);
}

int count_detected_mons()
{
    int count = 0;
    for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
        {
            // Don't expose new dug out areas:
            // Note: assumptions are being made here about how
            // terrain can change (eg it used to be solid, and
            // thus monster/item free).
            if (is_terrain_changed(x, y))
                continue;

            if (is_envmap_detected_mons(x, y))
                count++;
        }

    return (count);
}

void clear_map(bool clear_detected_items, bool clear_detected_monsters)
{
    for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
        {
            // FIXME convert to using p everywhere.
            const coord_def p(x,y);
            // Don't expose new dug out areas:
            // Note: assumptions are being made here about how
            // terrain can change (eg it used to be solid, and
            // thus monster/item free).

            // This reasoning doesn't make sense when it comes to *clearing*
            // the map! (jpeg)

            unsigned envc = get_envmap_char(x, y);
            if (!envc)
                continue;

            if (is_envmap_item(x, y))
                continue;

            if (!clear_detected_items && is_envmap_detected_item(x, y))
                continue;

            if (!clear_detected_monsters && is_envmap_detected_mons(x, y))
                continue;

            set_envmap_obj(p, is_terrain_known(p)? grd(p) : 0);
            set_envmap_detected_mons(x, y, false);
            set_envmap_detected_item(x, y, false);

#ifdef USE_TILE
            set_envmap_obj(p, is_terrain_known(p)? grd(p) : 0);
            env.tile_bk_fg[x][y] = 0;
            env.tile_bk_bg[x][y] = is_terrain_known(p) ?
                tile_idx_unseen_terrain(x, y, grd[x][y]) :
                tileidx_feature(DNGN_UNSEEN, x, y);
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
            && grid_stair_direction(grd(mons->pos())) != CMD_NO_CMD)
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
    if (!player_see_invis() && mons->has_ench(ENCH_INVIS)
        && mons->backlit())
    {
        col = DARKGREY;
    }

    return (col);
}

static void _good_god_follower_attitude_change(monsters *monster)
{
    if (player_is_unholy() || crawl_state.arena)
        return;

    // For followers of good gods, decide whether holy beings will be
    // good neutral towards you.
    if (is_good_god(you.religion)
        && monster->foe == MHITYOU
        && mons_is_holy(monster)
        && !testbits(monster->flags, MF_ATT_CHANGE_ATTEMPT)
        && !mons_wont_attack(monster)
        && mons_player_visible(monster) && !mons_is_sleeping(monster)
        && !mons_is_confused(monster) && !mons_is_paralysed(monster))
    {
        monster->flags |= MF_ATT_CHANGE_ATTEMPT;

        if (x_chance_in_y(you.piety, MAX_PIETY) && !you.penance[you.religion])
        {
            int wpn = you.equip[EQ_WEAPON];
            if (wpn != -1
                && you.inv[wpn].base_type == OBJ_WEAPONS
                && is_evil_item(you.inv[wpn])
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
        && mons_player_visible(monster) && !mons_is_sleeping(monster)
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

static void _handle_seen_interrupt(monsters* monster)
{
    if (mons_is_mimic(monster->type)
        && !mons_is_known_mimic(monster))
    {
        return;
    }

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
        interrupt_activity( AI_SEE_MONSTER, aid );
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

    _handle_seen_interrupt(mon);
}

void handle_monster_shouts(monsters* monster, bool force)
{
    if (!force && (!you.turn_is_over
                   || x_chance_in_y(you.skills[SK_STEALTH], 30)))
    {
        return;
    }

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
    if (s_type == S_SILENT && !player_monster_visible(monster)
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
    if (mons_near(monster) && (!monster->invisible() || player_see_invis()))
        suffix = " seen";
    else
        suffix = " unseen";

    msg = getShoutString(key, suffix);

    if (msg == "__DEFAULT" || msg == "__NEXT")
        msg = getShoutString(default_msg_key, suffix);
    else if (msg.empty())
    {
        // See if there's a shout for all monsters using the
        // same glyph/symbol
        std::string glyph_key = "'";

        // Database keys are case-insensitve.
        if (isupper(mons_char(monster->type)))
            glyph_key += "cap-";

        glyph_key += mons_char(monster->type);
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

        // Monster must come up from being submerged if it wants to
        // shout.
        if (mons_is_submerged(monster))
        {
            if (!monster->del_ench(ENCH_SUBMERGED))
            {
                // Couldn't unsubmerge.
                return;
            }

            if (you.can_see(monster))
            {
                monster->seen_context = "bursts forth shouting";
                // Give interrupt message before shout message.
                _handle_seen_interrupt(monster);
            }
        }

        msg = do_mon_str_replacements(msg, monster, s_type);
        msg::streams(channel) << msg << std::endl;
    }

    const int noise_level = get_shout_noise_level(s_type);
    if (noise_level > 0)
        noisy(noise_level, monster->pos());
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

    if (!player_monster_visible( monster ))
    {
        // ripple effect?
        if (grd(monster->pos()) == DNGN_SHALLOW_WATER
            && !mons_flies(monster)
            && env.cgrid(monster->pos()) == EMPTY_CLOUD)
        {
            _set_show_backup(e.x, e.y);
            env.show(e)     = DNGN_INVIS_EXPOSED;
            env.show_col(e) = BLUE;
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

    monsters *monster = NULL;

    for (int s = 0; s < MAX_MONSTERS; s++)
    {
        monster = &menv[s];

        if (monster->alive() && mons_near(monster))
        {
            if (do_updates && (mons_is_sleeping(monster)
                               || mons_is_wandering(monster))
                && check_awaken(monster))
            {
                behaviour_event(monster, ME_ALERT, MHITYOU);
                handle_monster_shouts(monster);
            }

            // [enne] - It's possible that mgrd and monster->x/y are out of
            // sync because they are updated separately.  If we can see this
            // monster, then make sure that the mgrd is set correctly.
            if (mgrd(monster->pos()) != s)
            {
#ifdef DEBUG_DIAGNOSTICS
                // If this mprf triggers for you, please note any special
                // circumstances so we can track down where this is coming
                // from.
                mprf(MSGCH_ERROR, "monster %s (%d) at (%d, %d) was "
                     "improperly placed.  Updating mgrd.",
                     monster->name(DESC_PLAIN, true).c_str(), s,
                     monster->pos().x, monster->pos().y);
#endif
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

            if (player_monster_visible(monster)
                && (!mons_is_mimic(monster->type)
                    || mons_is_known_mimic(monster)))
            {
                if (!(monster->flags & MF_WAS_IN_VIEW) && you.turn_is_over)
                    _handle_seen_interrupt(monster);

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

    // Repel undead is a holy aura, to which undead and demonic
    // creatures are sensitive.  Note that even though demons aren't
    // affected by repel undead, they do sense this type of divine aura.
    // -- bwr
    if (you.duration[DUR_REPEL_UNDEAD] && mons_is_unholy(monster))
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

    if (!mons_player_visible(monster))
    {
        mons_perc -= 75;
        unnatural_stealthy = true;
    }

    if (mons_is_sleeping(monster))
    {
        if (mons_holiness(monster) == MH_NATURAL)
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
    if (you.backlit() && mons_player_visible(monster))
        mons_perc += 50;

    if (mons_perc < 0)
        mons_perc = 0;

    if (x_chance_in_y(mons_perc + 1, stealth))
        return (true); // Oops, the monster wakes up!

    // You didn't wake the monster!
    if (player_light_armour(true)
        && you.can_see(monster) // to avoid leaking information
        && you.burden_state == BS_UNENCUMBERED
        && you.special_wield != SPWLD_SHADOW
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
    unsigned short &ecol = env.show_col(ep);

    const dungeon_feature_type grid = grd(gp);
    if (Options.feature_item_brand && is_critical_feature(grid))
        ecol |= COLFLAG_FEATURE_ITEM;
    else if (Options.trap_item_brand && grid_is_trap(grid))
        ecol |= COLFLAG_TRAP_ITEM;
    else
    {
        const unsigned short gcol = env.grid_colours(gp);
        ecol = (grid == DNGN_SHALLOW_WATER) ?
               (gcol != BLACK ? gcol : CYAN) : eitem.colour;
        if (eitem.link != NON_ITEM && !crawl_state.arena)
            ecol |= COLFLAG_ITEM_HEAP;
        env.show(ep) = _get_item_dngn_code( eitem );
    }

#ifdef USE_TILE
    int idx = igrd(gp);
    if (is_stair(grid))
        tile_place_item_marker(ep.x, ep.y, idx);
    else
        tile_place_item(ep.x, ep.y, idx);
#endif
}

void item_grid()
{
    const coord_def c(crawl_view.glosc());
    for (radius_iterator ri(c, LOS_RADIUS, true, false);
         ri; ++ri)
    {
        if (igrd(*ri) != NON_ITEM)
        {
            const coord_def ep = *ri - c + coord_def(9, 9);
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

    case CLOUD_MIST:
        which_colour = EC_MIST;
        break;

    case CLOUD_CHAOS:
        which_colour = EC_RANDOM;
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
            if (see_grid(env.cloud[s].pos))
                _update_cloud_grid(s);
        }
    }
}

// Noisy now has a messenging service for giving messages to the
// player is appropriate.
//
// Returns true if the PC heard the noise.
bool noisy(int loudness, const coord_def& where, const char *msg, bool mermaid)
{
    bool ret = false;

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

        if (monster->type < 0)
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
}                               // end noisy()

static const char* _player_vampire_smells_blood(int dist)
{
    // non-thirsty vampires get no clear indication of how close the
    // smell is
    if (you.hunger_state >= HS_SATIATED)
        return "";

    if (dist < 16) // 4*4
        return " near-by";

    if (you.hunger_state <= HS_NEAR_STARVING && dist > 64) // 8*8
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
                if (!see_grid(where))
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
            if (mons_is_sleeping(monster)
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

// The LOS code now uses raycasting -- haranp

#define LONGSIZE (sizeof(unsigned long)*8)
#define LOS_MAX_RANGE_X 9
#define LOS_MAX_RANGE_Y 9
#define LOS_MAX_RANGE 9

// the following two constants represent the 'middle' of the sh array.
// since the current shown area is 19x19, centering the view at (9,9)
// means it will be exactly centered.
// This is done to accomodate possible future changes in viewable screen
// area - simply change sh_xo and sh_yo to the new view center.

const int sh_xo = 9;            // X and Y origins for the sh array
const int sh_yo = 9;

// Data used for the LOS algorithm
int los_radius_squared = 8*8 + 1;

unsigned long* los_blockrays = NULL;
unsigned long* dead_rays = NULL;
unsigned long* smoke_rays = NULL;
std::vector<short> ray_coord_x;
std::vector<short> ray_coord_y;
std::vector<short> compressed_ray_x;
std::vector<short> compressed_ray_y;
std::vector<int> raylengths;
std::vector<ray_def> fullrays;

void setLOSRadius(int newLR)
{
    los_radius_squared = newLR * newLR + 1*1;
}

bool get_bit_in_long_array( const unsigned long* data, int where )
{
    int wordloc = where / LONGSIZE;
    int bitloc = where % LONGSIZE;
    return ((data[wordloc] & (1UL << bitloc)) != 0);
}

static void _set_bit_in_long_array( unsigned long* data, int where )
{
    int wordloc = where / LONGSIZE;
    int bitloc = where % LONGSIZE;
    data[wordloc] |= (1UL << bitloc);
}

#define EPSILON_VALUE 0.00001
bool double_is_zero( const double x )
{
    return (x > -EPSILON_VALUE) && (x < EPSILON_VALUE);
}

// note that slope must be nonnegative!
// returns 0 if the advance was in x, 1 if it was in y, 2 if it was
// the diagonal
static int _find_next_intercept(double* accx, double* accy, const double slope)
{

    // handle perpendiculars
    if ( double_is_zero(slope) )
    {
        *accx += 1.0;
        return 0;
    }
    if ( slope > 100.0 )
    {
        *accy += 1.0;
        return 1;
    }

    const double xtarget = (static_cast<int>(*accx) + 1);
    const double ytarget = (static_cast<int>(*accy) + 1);
    const double xdistance = xtarget - *accx;
    const double ydistance = ytarget - *accy;
    const double distdiff = (xdistance * slope - ydistance);

    // exact corner
    if ( double_is_zero( distdiff ) )
    {
        // move somewhat away from the corner
        if ( slope > 1.0 )
        {
            *accx = xtarget + EPSILON_VALUE * 2;
            *accy = ytarget + EPSILON_VALUE * 2 * slope;
        }
        else
        {
            *accx = xtarget + EPSILON_VALUE * 2 / slope;
            *accy = ytarget + EPSILON_VALUE * 2;
        }
        return 2;
    }

    double traveldist;
    int rc = -1;
    if ( distdiff > 0.0 )
    {
        traveldist = ydistance / slope;
        rc = 1;
    }
    else
    {
        traveldist = xdistance;
        rc = 0;
    }

    traveldist += EPSILON_VALUE * 10.0;

    *accx += traveldist;
    *accy += traveldist * slope;
    return rc;
}

ray_def::ray_def() : accx(0.0), accy(0.0), slope(0.0), quadrant(0),
                     fullray_idx(0)
{
}

double ray_def::reflect(double p, double c) const
{
    return (c + c - p);
}

double ray_def::reflect(bool rx, double oldx, double newx) const
{
    if (rx? fabs(slope) > 1.0 : fabs(slope) < 1.0)
        return (reflect(oldx, floor(oldx) + 0.5));

    const double flnew = floor(newx);
    const double flold = floor(oldx);
    return (reflect(oldx,
                    flnew > flold? flnew :
                    flold > flnew? flold :
                    (newx + oldx) / 2));
}

void ray_def::set_reflect_point(const double oldx, const double oldy,
                                double *newx, double *newy,
                                bool blocked_x, bool blocked_y)
{
    if (blocked_x == blocked_y)
    {
        // What to do?
        *newx = oldx;
        *newy = oldy;
        return;
    }

    if (blocked_x)
    {
        ASSERT(int(oldy) != int(*newy));
        *newy = oldy;
        *newx = reflect(true, oldx, *newx);
    }
    else
    {
        ASSERT(int(oldx) != int(*newx));
        *newx = oldx;
        *newy = reflect(false, oldy, *newy);
    }
}

void ray_def::advance_and_bounce()
{
    // 0 = down-right, 1 = down-left, 2 = up-left, 3 = up-right
    int bouncequad[4][3] =
    {
        { 1, 3, 2 }, { 0, 2, 3 }, { 3, 1, 0 }, { 2, 0, 1 }
    };
    int oldx = x(), oldy = y();
    const double oldaccx = accx, oldaccy = accy;
    int rc = advance(false);
    int newx = x(), newy = y();
    ASSERT( grid_is_solid(grd[newx][newy]) );

    const bool blocked_x = grid_is_solid(grd[oldx][newy]);
    const bool blocked_y = grid_is_solid(grd[newx][oldy]);

    if ( double_is_zero(slope) || slope > 100.0 )
        quadrant = bouncequad[quadrant][2];
    else if ( rc != 2 )
        quadrant = bouncequad[quadrant][rc];
    else
    {
        ASSERT( (oldx != newx) && (oldy != newy) );
        if ( blocked_x && blocked_y )
            quadrant = bouncequad[quadrant][rc];
        else if ( blocked_x )
            quadrant = bouncequad[quadrant][1];
        else
            quadrant = bouncequad[quadrant][0];
    }

    set_reflect_point(oldaccx, oldaccy, &accx, &accy, blocked_x, blocked_y);
}

double ray_def::get_degrees() const
{
    if (slope > 100.0)
    {
        if (quadrant == 3 || quadrant == 2)
            return (90.0);
        else
            return (270.0);
    }
    else if (double_is_zero(slope))
    {
        if (quadrant == 0 || quadrant == 3)
            return (0.0);
        else
            return (180.0);
    }

    double deg = atan(slope) * 180.0 / M_PI;

    switch (quadrant)
    {
    case 0:
        return (360.0 - deg);

    case 1:
        return (180.0 + deg);

    case 2:
        return (180.0 - deg);

    case 3:
        return (deg);
    }
    ASSERT(!"ray has illegal quadrant");
    return (0.0);
}

void ray_def::set_degrees(double deg)
{
    while (deg < 0.0)
        deg += 360.0;
    while (deg >= 360.0)
        deg -= 360.0;

    double _slope = tan(deg / 180.0 * M_PI);

    if (double_is_zero(_slope))
    {
        slope = 0.0;

        if (deg < 90.0 || deg > 270.0)
            quadrant = 0; // right/east
        else
            quadrant = 1; // left/west
    }
    else if (_slope > 0)
    {
        slope = _slope;

        if (deg >= 180.0 && deg <= 270.0)
            quadrant = 1;
        else
            quadrant = 3;
    }
    else
    {
        slope = -_slope;

        if (deg >= 90 && deg <= 180)
            quadrant = 2;
        else
            quadrant = 0;
    }

    if (slope > 1000.0)
        slope = 1000.0;
}

void ray_def::regress()
{
    int opp_quadrant[4] = { 2, 3, 0, 1 };
    quadrant = opp_quadrant[quadrant];
    advance(false);
    quadrant = opp_quadrant[quadrant];
}

int ray_def::advance_through(const coord_def &target)
{
    return (advance(true, &target));
}

int ray_def::advance(bool shortest_possible, const coord_def *target)
{
    if (!shortest_possible)
        return (raw_advance());

    // If we want to minimize the number of moves on the ray, look one
    // step ahead and see if we can get a diagonal.

    const coord_def old(static_cast<int>(accx), static_cast<int>(accy));
    const int ret = raw_advance();

    if (ret == 2 || (target && pos() == *target))
        return (ret);

    const double maccx = accx, maccy = accy;
    if (raw_advance() != 2)
    {
        const coord_def second(static_cast<int>(accx), static_cast<int>(accy));
        // If we can convert to a diagonal, do so.
        if ((second - old).abs() == 2)
            return (2);
    }

    // No diagonal, so roll back.
    accx = maccx;
    accy = maccy;

    return (ret);
}

int ray_def::raw_advance()
{
    int rc;
    switch ( quadrant )
    {
    case 0:
        // going down-right
        rc = _find_next_intercept( &accx, &accy, slope );
        return rc;
    case 1:
        // going down-left
        accx = 100.0 - EPSILON_VALUE/10.0 - accx;
        rc   = _find_next_intercept( &accx, &accy, slope );
        accx = 100.0 - EPSILON_VALUE/10.0 - accx;
        return rc;
    case 2:
        // going up-left
        accx = 100.0 - EPSILON_VALUE/10.0 - accx;
        accy = 100.0 - EPSILON_VALUE/10.0 - accy;
        rc   = _find_next_intercept( &accx, &accy, slope );
        accx = 100.0 - EPSILON_VALUE/10.0 - accx;
        accy = 100.0 - EPSILON_VALUE/10.0 - accy;
        return rc;
    case 3:
        // going up-right
        accy = 100.0 - EPSILON_VALUE/10.0 - accy;
        rc   = _find_next_intercept( &accx, &accy, slope );
        accy = 100.0 - EPSILON_VALUE/10.0 - accy;
        return rc;
    default:
        return -1;
    }
}

// Shoot a ray from the given start point (accx, accy) with the given
// slope, with a maximum distance (in either x or y coordinate) of
// maxrange. Store the visited cells in xpos[] and ypos[], and
// return the number of cells visited.
static int _shoot_ray( double accx, double accy, const double slope,
                       int maxrange, int xpos[], int ypos[] )
{
    int curx, cury;
    int cellnum;
    for (cellnum = 0; true; ++cellnum)
    {
        _find_next_intercept( &accx, &accy, slope );
        curx = static_cast<int>(accx);
        cury = static_cast<int>(accy);
        if (curx > maxrange || cury > maxrange)
            break;

        // Work with the new square.
        xpos[cellnum] = curx;
        ypos[cellnum] = cury;
    }
    return cellnum;
}

// Check if the passed ray has already been created.
static bool _is_duplicate_ray( int len, int xpos[], int ypos[] )
{
    int cur_offset = 0;
    for (unsigned int i = 0; i < raylengths.size(); ++i)
    {
        // Only compare equal-length rays.
        if (raylengths[i] != len)
        {
            cur_offset += raylengths[i];
            continue;
        }

        int j;
        for (j = 0; j < len; ++j)
        {
            if (ray_coord_x[j + cur_offset] != xpos[j]
                || ray_coord_y[j + cur_offset] != ypos[j])
            {
                break;
            }
        }

        // Exact duplicate?
        if (j == len)
            return (true);

        // Move to beginning of next ray.
        cur_offset += raylengths[i];
    }
    return (false);
}

// Is starta...lengtha a subset of startb...lengthb?
static bool _is_subset( int starta, int startb, int lengtha, int lengthb )
{
    int cura = starta, curb = startb;
    int enda = starta + lengtha, endb = startb + lengthb;

    while (cura < enda && curb < endb)
    {
        if (ray_coord_x[curb] > ray_coord_x[cura])
            return (false);
        if (ray_coord_y[curb] > ray_coord_y[cura])
            return (false);

        if (ray_coord_x[cura] == ray_coord_x[curb]
            && ray_coord_y[cura] == ray_coord_y[curb])
        {
            ++cura;
        }

        ++curb;
    }

    return (cura == enda);
}

// Returns a vector which lists all the nonduped cellrays (by index).
static std::vector<int> _find_nonduped_cellrays()
{
    // A cellray c in a fullray f is duped if there is a fullray g
    // such that g contains c and g[:c] is a subset of f[:c].
    int raynum, cellnum, curidx, testidx, testray, testcell;
    bool is_duplicate;

    std::vector<int> result;
    for (curidx = 0, raynum = 0;
         raynum < static_cast<int>(raylengths.size());
         curidx += raylengths[raynum++])
    {
        for (cellnum = 0; cellnum < raylengths[raynum]; ++cellnum)
        {
            // Is the cellray raynum[cellnum] duplicated?
            is_duplicate = false;
            // XXX: We should really check everything up to now
            // completely, and all further rays to see if they're
            // proper subsets.
            const int curx = ray_coord_x[curidx + cellnum];
            const int cury = ray_coord_y[curidx + cellnum];
            for (testidx = 0, testray = 0; testray < raynum;
                 testidx += raylengths[testray++])
            {
                // Scan ahead to see if there's an intersect.
                for (testcell = 0; testcell < raylengths[raynum]; ++testcell)
                {
                    const int testx = ray_coord_x[testidx + testcell];
                    const int testy = ray_coord_y[testidx + testcell];
                    // We can short-circuit sometimes.
                    if (testx > curx || testy > cury)
                        break;

                    // Bingo!
                    if (testx == curx && testy == cury)
                    {
                        is_duplicate = _is_subset(testidx, curidx,
                                                  testcell, cellnum);
                        break;
                    }
                }
                if (is_duplicate)
                    break;      // No point in checking further rays.
            }
            if (!is_duplicate)
                result.push_back(curidx + cellnum);
        }
    }
    return result;
}

// Create and register the ray defined by the arguments.
// Return true if the ray was actually registered (i.e., not a duplicate.)
static bool _register_ray( double accx, double accy, double slope )
{
    int xpos[LOS_MAX_RANGE * 2 + 1], ypos[LOS_MAX_RANGE * 2 + 1];
    int raylen = _shoot_ray( accx, accy, slope, LOS_MAX_RANGE, xpos, ypos );

    // Early out if ray already exists.
    if (_is_duplicate_ray(raylen, xpos, ypos))
        return (false);

    // Not duplicate, register.
    for (int i = 0; i < raylen; ++i)
    {
        // Create the cellrays.
        ray_coord_x.push_back(xpos[i]);
        ray_coord_y.push_back(ypos[i]);
    }

    // Register the fullray.
    raylengths.push_back(raylen);
    ray_def ray;
    ray.accx = accx;
    ray.accy = accy;
    ray.slope = slope;
    ray.quadrant = 0;
    fullrays.push_back(ray);

    return (true);
}

static void _create_blockrays()
{
    // determine nonduplicated rays
    std::vector<int> nondupe_cellrays    = _find_nonduped_cellrays();
    const unsigned int num_nondupe_rays  = nondupe_cellrays.size();
    const unsigned int num_nondupe_words =
        (num_nondupe_rays + LONGSIZE - 1) / LONGSIZE;
    const unsigned int num_cellrays = ray_coord_x.size();
    const unsigned int num_words = (num_cellrays + LONGSIZE - 1) / LONGSIZE;

    // first build all the rays: easier to do blocking calculations there
    unsigned long* full_los_blockrays;
    full_los_blockrays = new unsigned long[num_words * (LOS_MAX_RANGE_X+1) *
                                           (LOS_MAX_RANGE_Y+1)];
    memset((void*)full_los_blockrays, 0, sizeof(unsigned long) * num_words *
           (LOS_MAX_RANGE_X+1) * (LOS_MAX_RANGE_Y+1));

    int cur_offset = 0;

    for ( unsigned int ray = 0; ray < raylengths.size(); ++ray )
    {
        for ( int i = 0; i < raylengths[ray]; ++i )
        {
            // every cell blocks...
            unsigned long* const inptr = full_los_blockrays +
                (ray_coord_x[i + cur_offset] * (LOS_MAX_RANGE_Y + 1) +
                 ray_coord_y[i + cur_offset]) * num_words;

            // ...all following cellrays
            for ( int j = i+1; j < raylengths[ray]; ++j )
                _set_bit_in_long_array( inptr, j + cur_offset );

        }
        cur_offset += raylengths[ray];
    }

    // we've built the basic blockray array; now compress it, keeping
    // only the nonduplicated cellrays.

    // allocate and clear memory
    los_blockrays = new unsigned long[num_nondupe_words * (LOS_MAX_RANGE_X+1) * (LOS_MAX_RANGE_Y + 1)];
    memset((void*)los_blockrays, 0, sizeof(unsigned long) * num_nondupe_words *
           (LOS_MAX_RANGE_X+1) * (LOS_MAX_RANGE_Y+1));

    // we want to only keep the cellrays from nondupe_cellrays.
    compressed_ray_x.resize(num_nondupe_rays);
    compressed_ray_y.resize(num_nondupe_rays);
    for ( unsigned int i = 0; i < num_nondupe_rays; ++i )
    {
        compressed_ray_x[i] = ray_coord_x[nondupe_cellrays[i]];
        compressed_ray_y[i] = ray_coord_y[nondupe_cellrays[i]];
    }
    unsigned long* oldptr = full_los_blockrays;
    unsigned long* newptr = los_blockrays;
    for ( int x = 0; x <= LOS_MAX_RANGE_X; ++x )
        for ( int y = 0; y <= LOS_MAX_RANGE_Y; ++y )
        {
            for ( unsigned int i = 0; i < num_nondupe_rays; ++i )
                if ( get_bit_in_long_array(oldptr, nondupe_cellrays[i]) )
                    _set_bit_in_long_array(newptr, i);

            oldptr += num_words;
            newptr += num_nondupe_words;
        }

    // we can throw away full_los_blockrays now
    delete [] full_los_blockrays;

    dead_rays = new unsigned long[num_nondupe_words];
    smoke_rays = new unsigned long[num_nondupe_words];

#ifdef DEBUG_DIAGNOSTICS
    mprf( MSGCH_DIAGNOSTICS, "Cellrays: %d Fullrays: %u Compressed: %u",
          num_cellrays, raylengths.size(), num_nondupe_rays );
#endif
}

static int _gcd( int x, int y )
{
    int tmp;
    while ( y != 0 )
    {
        x %= y;
        tmp = x;
        x = y;
        y = tmp;
    }
    return x;
}

bool complexity_lt( const std::pair<int,int>& lhs,
                    const std::pair<int,int>& rhs )
{
    return lhs.first * lhs.second < rhs.first * rhs.second;
}

// Cast all rays
void raycast()
{
    static bool done_raycast = false;
    if ( done_raycast )
        return;

    // Creating all rays for first quadrant
    // We have a considerable amount of overkill.
    done_raycast = true;

    // register perpendiculars FIRST, to make them top choice
    // when selecting beams
    _register_ray( 0.5, 0.5, 1000.0 );
    _register_ray( 0.5, 0.5, 0.0 );

    // For a slope of M = y/x, every x we move on the X axis means
    // that we move y on the y axis. We want to look at the resolution
    // of x/y: in that case, every step on the X axis means an increase
    // of 1 in the Y axis at the intercept point. We can assume gcd(x,y)=1,
    // so we look at steps of 1/y.

    // Changing the order a bit. We want to order by the complexity
    // of the beam, which is log(x) + log(y) ~ xy.
    std::vector<std::pair<int,int> > xyangles;
    for ( int xangle = 1; xangle <= LOS_MAX_RANGE; ++xangle )
        for ( int yangle = 1; yangle <= LOS_MAX_RANGE; ++yangle )
        {
            if ( _gcd(xangle, yangle) == 1 )
                xyangles.push_back(std::pair<int,int>(xangle, yangle));
        }

    std::sort( xyangles.begin(), xyangles.end(), complexity_lt );
    for ( unsigned int i = 0; i < xyangles.size(); ++i )
    {
        const int xangle = xyangles[i].first;
        const int yangle = xyangles[i].second;

        const double slope = ((double)(yangle)) / xangle;
        const double rslope = ((double)(xangle)) / yangle;
        for ( int intercept = 0; intercept <= yangle; ++intercept )
        {
            double xstart = ((double)(intercept)) / yangle;
            if ( intercept == 0 )
                xstart += EPSILON_VALUE / 10.0;
            if ( intercept == yangle )
                xstart -= EPSILON_VALUE / 10.0;

            // y should be "about to change"
            _register_ray( xstart, 1.0 - EPSILON_VALUE / 10.0, slope );
            // also draw the identical ray in octant 2
            _register_ray( 1.0 - EPSILON_VALUE / 10.0, xstart, rslope );
        }
    }

    // Now create the appropriate blockrays array
    _create_blockrays();
}

static void _set_ray_quadrant( ray_def& ray, int sx, int sy, int tx, int ty )
{
    if ( tx >= sx && ty >= sy )
        ray.quadrant = 0;
    else if ( tx < sx && ty >= sy )
        ray.quadrant = 1;
    else if ( tx < sx && ty < sy )
        ray.quadrant = 2;
    else if ( tx >= sx && ty < sy )
        ray.quadrant = 3;
    else
        mpr("Bad ray quadrant!", MSGCH_DIAGNOSTICS);
}

static int _cyclic_offset( unsigned int ui, int cycle_dir, int startpoint,
                           int maxvalue )
{
    const int i = (int)ui;
    if ( startpoint < 0 )
        return i;
    switch ( cycle_dir )
    {
    case 1:
        return (i + startpoint + 1) % maxvalue;
    case -1:
        return (i - 1 - startpoint + maxvalue) % maxvalue;
    case 0:
    default:
        return i;
    }
}

static const double VERTICAL_SLOPE = 10000.0;
static double _calc_slope(double x, double y)
{
    if (double_is_zero(x))
        return (VERTICAL_SLOPE);

    const double slope = y / x;
    return (slope > VERTICAL_SLOPE? VERTICAL_SLOPE : slope);
}

static double _slope_factor(const ray_def &ray)
{
    double xdiff = fabs(ray.accx - 0.5), ydiff = fabs(ray.accy - 0.5);

    if (double_is_zero(xdiff) && double_is_zero(ydiff))
        return ray.slope;

    const double slope = _calc_slope(ydiff, xdiff);
    return (slope + ray.slope) / 2.0;
}

static bool _superior_ray(int shortest, int imbalance,
                          int raylen, int rayimbalance,
                          double slope_diff, double ray_slope_diff)
{
    if (shortest != raylen)
        return (shortest > raylen);

    if (imbalance != rayimbalance)
        return (imbalance > rayimbalance);

    return (slope_diff > ray_slope_diff);
}

// Find a nonblocked ray from source to target. Return false if no
// such ray could be found, otherwise return true and fill ray
// appropriately.
// If allow_fallback is true, fall back to a center-to-center ray
// if range is too great or all rays are blocked.
// If cycle_dir is 0, find the first fitting ray. If it is 1 or -1,
// assume that ray is appropriately filled in, and look for the next
// ray in that cycle direction.
// If find_shortest is true, examine all rays that hit the target and
// take the shortest (starting at ray.fullray_idx).

bool find_ray( const coord_def& source, const coord_def& target,
               bool allow_fallback, ray_def& ray, int cycle_dir,
               bool find_shortest, bool ignore_solid )
{
    int cellray, inray;

    const int sourcex = source.x;
    const int sourcey = source.y;
    const int targetx = target.x;
    const int targety = target.y;

    const int signx = ((targetx - sourcex >= 0) ? 1 : -1);
    const int signy = ((targety - sourcey >= 0) ? 1 : -1);
    const int absx  = signx * (targetx - sourcex);
    const int absy  = signy * (targety - sourcey);

    int cur_offset  = 0;
    int shortest    = INFINITE_DISTANCE;
    int imbalance   = INFINITE_DISTANCE;
    const double want_slope = _calc_slope(absx, absy);
    double slope_diff       = VERTICAL_SLOPE * 10.0;
    std::vector<coord_def> unaliased_ray;

    for ( unsigned int fray = 0; fray < fullrays.size(); ++fray )
    {
        const int fullray = _cyclic_offset( fray, cycle_dir, ray.fullray_idx,
                                            fullrays.size() );
        // Yeah, yeah, this is O(n^2). I know.
        cur_offset = 0;
        for (int i = 0; i < fullray; ++i)
            cur_offset += raylengths[i];

        for (cellray = 0; cellray < raylengths[fullray]; ++cellray)
        {
            if (ray_coord_x[cellray + cur_offset] == absx
                && ray_coord_y[cellray + cur_offset] == absy)
            {
                if (find_shortest)
                {
                    unaliased_ray.clear();
                    unaliased_ray.push_back(coord_def(0, 0));
                }

                // Check if we're blocked so far.
                bool blocked = false;
                coord_def c1, c3;
                int real_length = 0;
                for (inray = 0; inray <= cellray; ++inray)
                {
                    const int xi = signx * ray_coord_x[inray + cur_offset];
                    const int yi = signy * ray_coord_y[inray + cur_offset];
                    if (inray < cellray && !ignore_solid
                        && grid_is_solid(grd[sourcex + xi][sourcey + yi]))
                    {
                        blocked = true;
                        break;
                    }

                    if (find_shortest)
                    {
                        c3 = coord_def(xi, yi);

                        // We've moved at least two steps if inray > 0.
                        if (inray)
                        {
                            // Check for a perpendicular corner on the ray and
                            // pretend that it's a diagonal.
                            if ((c3 - c1).abs() != 2)
                                ++real_length;
                            else
                            {
                                // c2 was a dud move, pop it off
                                unaliased_ray.pop_back();
                            }
                        }
                        else
                            ++real_length;

                        unaliased_ray.push_back(c3);
                        c1 = unaliased_ray[real_length - 1];
                    }
                }

                int cimbalance = 0;
                // If this ray is a candidate for shortest, calculate
                // the imbalance. I'm defining 'imbalance' as the
                // number of consecutive diagonal or orthogonal moves
                // in the ray. This is a reasonable measure of deviation from
                // the Bresenham line between our selected source and
                // destination.
                if (!blocked && find_shortest && shortest >= real_length)
                {
                    int diags = 0, straights = 0;
                    for (int i = 1, size = unaliased_ray.size(); i < size; ++i)
                    {
                        const int dist =
                            (unaliased_ray[i] - unaliased_ray[i - 1]).abs();

                        if (dist == 2)
                        {
                            straights = 0;
                            if (++diags > cimbalance)
                                cimbalance = diags;
                        }
                        else
                        {
                            diags = 0;
                            if (++straights > cimbalance)
                                cimbalance = straights;
                        }
                    }
                }

                const double ray_slope_diff = find_shortest ?
                    fabs(_slope_factor(fullrays[fullray]) - want_slope) : 0.0;

                if (!blocked
                    &&  (!find_shortest
                         || _superior_ray(shortest, imbalance,
                                          real_length, cimbalance,
                                          slope_diff, ray_slope_diff)))
                {
                    // Success!
                    ray             = fullrays[fullray];
                    ray.fullray_idx = fullray;

                    shortest   = real_length;
                    imbalance  = cimbalance;
                    slope_diff = ray_slope_diff;

                    if (sourcex > targetx)
                        ray.accx = 1.0 - ray.accx;
                    if (sourcey > targety)
                        ray.accy = 1.0 - ray.accy;

                    ray.accx += sourcex;
                    ray.accy += sourcey;

                    _set_ray_quadrant(ray, sourcex, sourcey, targetx, targety);
                    if (!find_shortest)
                        return (true);
                }
            }
        }
    }

    if (find_shortest && shortest != INFINITE_DISTANCE)
        return (true);

    if (allow_fallback)
    {
        ray.accx = sourcex + 0.5;
        ray.accy = sourcey + 0.5;
        if (targetx == sourcex)
            ray.slope = VERTICAL_SLOPE;
        else
        {
            ray.slope  = targety - sourcey;
            ray.slope /= targetx - sourcex;
            if (ray.slope < 0)
                ray.slope = -ray.slope;
        }
        _set_ray_quadrant(ray, sourcex, sourcey, targetx, targety);
        ray.fullray_idx = -1;
        return (true);
    }
    return (false);
}

// Count the number of matching features between two points along
// a beam-like path; the path will pass through solid features.
// By default, it excludes end points from the count.
// If just_check is true, the function will return early once one
// such feature is encountered.
int num_feats_between(const coord_def& source, const coord_def& target,
                      dungeon_feature_type min_feat,
                      dungeon_feature_type max_feat,
                      bool exclude_endpoints, bool just_check)
{
    ray_def ray;
    int     count    = 0;
    int     max_dist = grid_distance(source, target);

    ray.fullray_idx = -1; // to quiet valgrind

    // We don't need to find the shortest beam, any beam will suffice.
    find_ray( source, target, true, ray, 0, false, true );

    if (exclude_endpoints && ray.pos() == source)
    {
        ray.advance(true);
        max_dist--;
    }

    int dist = 0;
    bool reached_target = false;
    while (dist++ <= max_dist)
    {
        const dungeon_feature_type feat = grd(ray.pos());

        if (ray.pos() == target)
            reached_target = true;

        if (feat >= min_feat && feat <= max_feat
            && (!exclude_endpoints || !reached_target))
        {
            count++;

            if (just_check) // Only needs to be > 0.
                return (count);
        }

        if (reached_target)
            break;

        ray.advance(true);
    }

    return (count);
}

// The rule behind LOS is:
// Two cells can see each other if there is any line from some point
// of the first to some point of the second ("generous" LOS.)
//
// We use raycasting. The algorithm:
// PRECOMPUTATION:
// Create a large bundle of rays and cast them.
// Mark, for each one, which cells kill it (and where.)
// Also, for each one, note which cells it passes.
// ACTUAL LOS:
// Unite the ray-killers for the given map; this tells you which rays
// are dead.
// Look up which cells the surviving rays have, and that's your LOS!
// OPTIMIZATIONS:
// WLOG, we can assume that we're in a specific quadrant - say the
// first quadrant - and just mirror everything after that.  We can
// likely get away with a single octant, but we don't do that. (To
// do...)
// Rays are actually split by each cell they pass. So each "ray" only
// identifies a single cell, and we can do logical ORs.  Once a cell
// kills a cellray, it will kill all remaining cellrays of that ray.
// Also, rays are checked to see if they are duplicates of each
// other. If they are, they're eliminated.
// Some cellrays can also be eliminated. In general, a cellray is
// unnecessary if there is another cellray with the same coordinates,
// and whose path (up to those coordinates) is a subset, not necessarily
// proper, of the original path. We still store the original cellrays
// fully for beam detection and such.
// PERFORMANCE:
// With reasonable values we have around 6000 cellrays, meaning
// around 600Kb (75 KB) of data. This gets cut down to 700 cellrays
// after removing duplicates. That means that we need to do
// around 22*100*4 ~ 9,000 memory reads + writes per LOS call on a
// 32-bit system. Not too bad.
// IMPROVEMENTS:
// Smoke will now only block LOS after two cells of smoke. This is
// done by updating with a second array.
void losight(env_show_grid &sh,
             feature_grid &gr, const coord_def& center,
             bool clear_walls_block, bool ignore_clouds)
{
    raycast();
    const int x_p = center.x;
    const int y_p = center.y;
    // go quadrant by quadrant
    const int quadrant_x[4] = {  1, -1, -1,  1 };
    const int quadrant_y[4] = {  1,  1, -1, -1 };

    // clear out sh
    sh.init(0);

    if (crawl_state.arena || crawl_state.arena_suspended)
    {
        for (int y = -ENV_SHOW_OFFSET; y <= ENV_SHOW_OFFSET; ++y)
            for (int x = -ENV_SHOW_OFFSET; x <= ENV_SHOW_OFFSET; ++x)
            {
                const coord_def pos = center + coord_def(x, y);
                if (map_bounds(pos))
                    sh[x + sh_xo][y + sh_yo] = gr(pos);
            }
        return;
    }

    const unsigned int num_cellrays = compressed_ray_x.size();
    const unsigned int num_words = (num_cellrays + LONGSIZE - 1) / LONGSIZE;

    for (int quadrant = 0; quadrant < 4; ++quadrant)
    {
        const int xmult = quadrant_x[quadrant];
        const int ymult = quadrant_y[quadrant];

        // clear out the dead rays array
        memset( (void*)dead_rays,  0, sizeof(unsigned long) * num_words);
        memset( (void*)smoke_rays, 0, sizeof(unsigned long) * num_words);

        // kill all blocked rays
        const unsigned long* inptr = los_blockrays;
        for (int xdiff = 0; xdiff <= LOS_MAX_RANGE_X; ++xdiff)
            for (int ydiff = 0; ydiff <= LOS_MAX_RANGE_Y;
                 ++ydiff, inptr += num_words)
            {

                const int realx = x_p + xdiff * xmult;
                const int realy = y_p + ydiff * ymult;

                if (!map_bounds(realx, realy))
                    continue;

                coord_def real(realx, realy);
                dungeon_feature_type dfeat = grid_appearance(gr, real);

                // if this cell is opaque...
                if (grid_is_opaque(dfeat)
                    || clear_walls_block && grid_is_wall(dfeat))
                {
                    // then block the appropriate rays
                    for (unsigned int i = 0; i < num_words; ++i)
                        dead_rays[i] |= inptr[i];
                }
                else if (!ignore_clouds
                         && is_opaque_cloud(env.cgrid[realx][realy]))
                {
                    // block rays which have already seen a cloud
                    for (unsigned int i = 0; i < num_words; ++i)
                    {
                        dead_rays[i] |= (smoke_rays[i] & inptr[i]);
                        smoke_rays[i] |= inptr[i];
                    }
                }
            }

        // ray calculation done, now work out which cells in this
        // quadrant are visible
        unsigned int rayidx = 0;
        for (unsigned int wordloc = 0; wordloc < num_words; ++wordloc)
        {
            const unsigned long curword = dead_rays[wordloc];
            // Note: the last word may be incomplete
            for (unsigned int bitloc = 0; bitloc < LONGSIZE; ++bitloc)
            {
                // make the cells seen by this ray at this point visible
                if ( ((curword >> bitloc) & 1UL) == 0 )
                {
                    // this ray is alive!
                    const int realx = xmult * compressed_ray_x[rayidx];
                    const int realy = ymult * compressed_ray_y[rayidx];
                    // update shadow map
                    if (x_p + realx >= 0 && x_p + realx < GXM
                        && y_p + realy >= 0 && y_p + realy < GYM
                        && realx * realx + realy * realy <= los_radius_squared)
                    {
                        sh[sh_xo+realx][sh_yo+realy] = gr[x_p+realx][y_p+realy];
                    }
                }
                ++rayidx;
                if (rayidx == num_cellrays)
                    break;
            }
        }
    }

    // [dshaligram] The player's current position is always visible.
    sh[sh_xo][sh_yo] = gr[x_p][y_p];
}


// Determines if the given feature is present at (x, y) in _grid_ coordinates.
// If you have map coords, add (1, 1) to get grid coords.
// Use one of
// 1. '<' and '>' to look for stairs
// 2. '\t' or '\\' for shops, portals.
// 3. '^' for traps
// 4. '_' for altars
// 5. Anything else will look for the exact same character in the level map.
bool is_feature(int feature, const coord_def& where)
{
    if (!env.map(where).object && !see_grid(where))
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

static void _draw_level_map(int start_x, int start_y, bool travel_mode)
{
    int bufcount2 = 0;
    screen_buffer_t buffer2[GYM * GXM * 2];

    int num_lines = _get_number_of_lines_levelmap();
    if (num_lines > GYM)
        num_lines = GYM;

    int num_cols = get_number_of_cols();
    if (num_cols > GXM)
        num_cols = GXM;

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
                                         travel_mode);

                buffer2[bufcount2 + 1] = colour;
                buffer2[bufcount2] = env.map(c).glyph();

                if (c == you.pos() && !crawl_state.arena_suspended)
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

static void _reset_travel_colours(std::vector<coord_def> &features)
{
    // We now need to redo travel colours.
    features.clear();
    find_travel_pos(you.pos(), NULL, NULL, &features);
    // Sort features into the order the player is likely to prefer.
    arrange_features(features);
}

// show_map() now centers the known map along x or y.  This prevents
// the player from getting "artificial" location clues by using the
// map to see how close to the end they are.  They'll need to explore
// to get that.  This function is still a mess, though. -- bwr
void show_map( coord_def &spec_place, bool travel_mode )
{
    cursor_control ccon(!Options.use_fake_cursor);
    int i, j;

    int move_x = 0, move_y = 0, scroll_y = 0;

    // Vector to track all features we can travel to, in order of distance.
    std::vector<coord_def> features;
    if (travel_mode)
    {
        travel_cache.update();

        find_travel_pos(you.pos(), NULL, NULL, &features);
        // Sort features into the order the player is likely to prefer.
        arrange_features(features);
    }

    int min_x = GXM, max_x = 0, min_y = 0, max_y = 0;
    bool found_y = false;

    const int num_lines   = _get_number_of_lines_levelmap();
    const int half_screen = (num_lines - 1) / 2;

    const int top = 1 + Options.level_map_title;

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

    const int map_lines = max_y - min_y + 1;

    const int start_x = min_x + (max_x - min_x + 1) / 2 - 40; // no x scrolling
    const int block_step = Options.level_map_cursor_step;
    int start_y = 0;                                          // y does scroll

    int screen_y = you.pos().y;

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

    int curs_x = you.pos().x - start_x + 1;
    int curs_y = you.pos().y - screen_y + half_screen + 1;
    int search_found = 0, anchor_x = -1, anchor_y = -1;

    bool map_alive  = true;
    bool redraw_map = true;

#ifndef USE_TILE
    clrscr();
#endif
    textcolor(DARKGREY);

    while (map_alive)
    {
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
            _draw_level_map(start_x, start_y, travel_mode);

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
        int key = unmangle_direction_keys(getchm(KC_LEVELMAP), KC_LEVELMAP,
                                          false, false);
        command_type cmd = key_to_command(key, KC_LEVELMAP);
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
                spec_place = grdp;
                map_alive     = false;
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
            features.clear();
            find_travel_pos(you.pos(), NULL, NULL, &features);
            // Sort features into the order the player is likely to prefer.
            arrange_features(features);
            break;

        // Cycle the radius of an exclude.
        case CMD_MAP_EXCLUDE_AREA:
        {
            const coord_def p(start_x + curs_x - 1, start_y + curs_y - 1);
            if (is_exclude_root(p))
                cycle_exclude_radius(p);
            else
                toggle_exclude(p);

            _reset_travel_colours(features);
            break;
        }

        case CMD_MAP_CLEAR_EXCLUDES:
            clear_excludes();
            _reset_travel_colours(features);
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
            move_x = you.pos().x - (start_x + curs_x - 1);
            move_y = you.pos().y - (start_y + curs_y - 1);
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
            if (travel_mode && x == you.pos().x && y == you.pos().y)
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
                spec_place = coord_def(x, y);
                map_alive = false;
                break;
            }
        }

#ifdef WIZARD
        case CMD_MAP_WIZARD_TELEPORT:
        {
            if (!you.wizard)
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

    return;
}                               // end show_map()


// Returns true if succeeded.
bool magic_mapping(int map_radius, int proportion, bool suppress_msg,
                   bool force)
{
    if (!force
        && (testbits(env.level_flags, LFLAG_NO_MAGIC_MAP)
            || testbits(get_branch_flags(), BFLAG_NO_MAGIC_MAP)))
    {
        if (!suppress_msg)
            mpr("You feel momentarily disoriented.");

        return (false);
    }

    if (!suppress_msg)
        mpr( "You feel aware of your surroundings." );

    if (map_radius > 50 && map_radius != 1000)
        map_radius = 50;
    else if (map_radius < 5)
        map_radius = 5;

    // now gradually weaker with distance:
    const int pfar = (map_radius * 7) / 10;
    const int very_far = (map_radius * 9) / 10;

    const bool wizard_map = map_radius == 1000 && you.wizard;
    for ( radius_iterator ri(you.pos(), map_radius, true, false); ri; ++ri )
    {
        if (proportion < 100 && random2(100) >= proportion)
            continue;       // note that proportion can be over 100

        const int dist = grid_distance( you.pos(), *ri );

        if (dist > pfar && one_chance_in(3))
            continue;

        if (dist > very_far && coinflip())
            continue;

        if (is_terrain_changed(*ri))
            clear_envmap_grid(*ri);

#ifdef USE_TILE
        if (!wizard_map && is_terrain_known(*ri))
        {
            // Can't use set_envmap_obj because that would
            // overwrite the gmap.
            env.tile_bk_bg(*ri) = tile_idx_unseen_terrain(ri->x, ri->y,
                                                          grd(*ri));
        }
#endif

        if (!wizard_map && is_terrain_known(*ri))
            continue;

        bool open = true;

        if (grid_is_solid(grd(*ri)) && grd(*ri) != DNGN_CLOSED_DOOR)
        {
            open = false;
            for ( adjacent_iterator ai(*ri); ai; ++ai )
            {
                if (map_bounds(*ai) && (!grid_is_opaque(grd(*ai))
                                        || grd(*ai) == DNGN_CLOSED_DOOR))
                {
                    open = true;
                    break;
                }
            }
        }

        if (open > 0)
        {
            if (wizard_map || !get_envmap_obj(*ri))
                set_envmap_obj(*ri, grd(*ri));

            // Hack to give demonspawn Pandemonium mutation the ability
            // to detect exits magically.
            if (wizard_map
                || player_mutation_level(MUT_PANDEMONIUM) > 1
                   && grd(*ri) == DNGN_EXIT_PANDEMONIUM)
            {
                set_terrain_seen( *ri );
            }
            else
                set_terrain_mapped( *ri );
        }
    }

    return (true);
}

// Realize that this is simply a repackaged version of
// stuff::see_grid() -- make certain they correlate {dlb}:
bool mons_near(const monsters *monster, unsigned short foe)
{
    // Early out -- no foe!
    if (foe == MHITNOT)
        return (false);

    if (foe == MHITYOU)
    {
        if (crawl_state.arena || crawl_state.arena_suspended)
            return (true);

        if ( grid_distance(monster->pos(), you.pos()) <= LOS_RADIUS )
        {
            const coord_def diff = grid2show(monster->pos());
            if (show_bounds(diff) && env.show(diff))
                return (true);
        }
        return (false);
    }

    // Must be a monster.
    const monsters *myFoe = &menv[foe];
    if (myFoe->type >= 0)
    {
        if ( grid_distance( monster->pos(), myFoe->pos() ) <= LOS_RADIUS )
            return (true);
    }

    return (false);
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
        return (mons_near(monster) && !i_feel_safe());
    }
    else
    {
        // For hostile monsters *you* are the main enemy.
        return (mons_near(monster));
    }
}

bool see_grid( const env_show_grid &show,
               const coord_def &c,
               const coord_def &pos )
{
    if (c == pos)
        return (true);

    const coord_def ip = pos - c;
    if (ip.rdist() < ENV_SHOW_OFFSET)
    {
        const coord_def sp(ip + coord_def(ENV_SHOW_OFFSET, ENV_SHOW_OFFSET));
        if (show(sp))
            return (true);
    }
    return (false);
}

// Answers the question: "Is a grid within character's line of sight?"
bool see_grid( const coord_def &p )
{
    return ((crawl_state.arena || crawl_state.arena_suspended)
            && crawl_view.in_grid_los(p))
        || see_grid(env.show, you.pos(), p);
}

// Answers the question: "Would a grid be within character's line of sight,
// even if all translucent/clear walls were made opaque?"
bool see_grid_no_trans( const coord_def &p )
{
    return see_grid(env.no_trans_show, you.pos(), p);
}

// Is the grid visible, but a translucent wall is in the way?
bool trans_wall_blocking( const coord_def &p )
{
    return see_grid(p) && !see_grid_no_trans(p);
}

// Usually calculates whether from one grid someone could see the other.
// Depending on the viewer's habitat, 'allowed' can be set to DNGN_FLOOR,
// DNGN_SHALLOW_WATER or DNGN_DEEP_WATER.
// Yes, this ignores lava-loving monsters.
// XXX: It turns out the beams are not symmetrical, i.e. switching
// pos1 and pos2 may result in small variations.
bool grid_see_grid(const coord_def& p1, const coord_def& p2,
                   dungeon_feature_type allowed)
{
    if (distance(p1, p2) > LOS_RADIUS * LOS_RADIUS)
        return (false);

    dungeon_feature_type max_disallowed = DNGN_MAXOPAQUE;
    if (allowed != DNGN_UNSEEN)
        max_disallowed = static_cast<dungeon_feature_type>(allowed - 1);

    // XXX: Ignoring clouds for now.
    return (!num_feats_between(p1, p2, DNGN_UNSEEN, max_disallowed,
                               true, true));
}

static const unsigned dchar_table[ NUM_CSET ][ NUM_DCHAR_TYPES ] =
{
    // CSET_ASCII
    {
        '#', '*', '.', ',', '\'', '+', '^', '>', '<',  // wall .. stairs up
        '_', '\\', '}', '{', '8', '~', '~',            // altar .. item detect
        '0', ')', '[', '/', '%', '?', '=', '!', '(',   // orb .. missile
        ':', '|', '}', '%', '$', '"', '#',             // book .. cloud
        ' ', '!', '#', '%', ':', ')', '*', '+',        // space .. fired_burst
        '/', '=', '?', 'X', '[', '`', '#'              // fi_stick .. explosion
    },

    // CSET_IBM - this is ANSI 437
    {
        177, 176, 249, 250, '\'', 254, '^', '>', '<',  // wall .. stairs up
        220, 239, 244, 247, '8', '~', '~',             // altar .. item detect
        '0', ')', '[', '/', '%', '?', '=', '!', '(',   // orb .. missile
        '+', '\\', '}', '%', '$', '"', '#',            // book .. cloud
        ' ', '!', '#', '%', '+', ')', '*', '+',        // space .. fired_burst
        '/', '=', '?', 'X', '[', '`', '#'              // fi_stick .. explosion
    },

    // CSET_DEC - remember: 224-255 are mapped to shifted 96-127
    {
        225, 224, 254, ':', '\'', 238, '^', '>', '<',  // wall .. stairs up
        251, 182, 167, 187, '8', 171, 168,             // altar .. item detect
        '0', ')', '[', '/', '%', '?', '=', '!', '(',   // orb .. missile
        '+', '\\', '}', '%', '$', '"', '#',            // book .. cloud
        ' ', '!', '#', '%', '+', ')', '*', '+',        // space .. fired_burst
        '/', '=', '?', 'X', '[', '`', '#'              // fi_stick .. explosion
    },

    // CSET_UNICODE
    {
        0x2592, 0x2591, 0xB7, 0x25E6, '\'', 0x25FC, '^', '>', '<',
        '_', 0x2229, 0x2320, 0x2248, '8', '~', '~',
        '0', ')', '[', '/', '%', '?', '=', '!', '(',
        '+', '|', '}', '%', '$', '"', '#',
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
        "item_amulet", "cloud"
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
        Feature[i].minimap = MF_UNSEEN;

        switch (i)
        {
        case DNGN_UNSEEN:
        default:
            break;

        case DNGN_ROCK_WALL:
        case DNGN_PERMAROCK_WALL:
            Feature[i].dchar        = DCHAR_WALL;
            Feature[i].colour       = EC_ROCK;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            Feature[i].minimap      = MF_WALL;
            break;

        case DNGN_STONE_WALL:
            Feature[i].dchar        = DCHAR_WALL;
            Feature[i].colour       = EC_STONE;
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


        case DNGN_OPEN_DOOR:
            Feature[i].dchar   = DCHAR_DOOR_OPEN;
            Feature[i].colour  = LIGHTGREY;
            Feature[i].minimap = MF_DOOR;
            break;

        case DNGN_CLOSED_DOOR:
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
            Feature[i].colour       = EC_ROCK;
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
            Feature[i].colour       = EC_FLOOR;
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
            Feature[i].colour       = EC_FLOOR;
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
            Feature[i].colour      = EC_SHIMMER_BLUE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = EC_SHIMMER_BLUE;
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
            Feature[i].colour      = EC_RANDOM;
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = EC_RANDOM;
            Feature[i].minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_EXIT_ABYSS:
            Feature[i].colour     = EC_RANDOM;
            Feature[i].dchar      = DCHAR_ARCH;
            Feature[i].map_colour = EC_RANDOM;
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
        case DNGN_ENTER_RESERVED_2:
        case DNGN_ENTER_RESERVED_3:
        case DNGN_ENTER_RESERVED_4:
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
        case DNGN_RETURN_RESERVED_2:
        case DNGN_RETURN_RESERVED_3:
        case DNGN_RETURN_RESERVED_4:
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
            Feature[i].colour      = EC_UNHOLY;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = EC_UNHOLY;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_XOM:
            Feature[i].colour      = EC_RANDOM;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = EC_RANDOM;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_VEHUMET:
            Feature[i].colour      = EC_VEHUMET;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = EC_VEHUMET;
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
            Feature[i].colour      = EC_FIRE;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = EC_FIRE;
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
            Feature[i].colour      = GREEN;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = GREEN;
            Feature[i].minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_BEOGH:
            Feature[i].colour      = EC_BEOGH;
            Feature[i].dchar       = DCHAR_ALTAR;
            Feature[i].flags      |= FFT_NOTABLE;
            Feature[i].map_colour  = DARKGREY;
            Feature[i].seen_colour = EC_BEOGH;
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
                (!map_bounds(gc)) ? 0
                : (!crawl_view.in_grid_los(gc)) ? get_envmap_char(gc.x, gc.y)
                : (gc == you.pos()) ? you.symbol
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
    if (you.special_wield == SPWLD_SHADOW)
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
    if (mons_near(mon) && player_monster_visible(mon))
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
#ifdef USE_TILE
    std::vector<unsigned int> tileb(
        crawl_view.viewsz.y * crawl_view.viewsz.x * 2);
    tiles.clear_text_tags(TAG_NAMED_MONSTER);
#endif
    screen_buffer_t *buffy(crawl_view.vbuf);

    int count_x, count_y;

    if (!crawl_state.arena && !crawl_state.arena_suspended)
    {
        // Must be done first.
        losight(env.show, grd, you.pos());

        // What would be visible, if all of the translucent walls were
        // made opaque.
        losight(env.no_trans_show, grd, you.pos(), true);
    }
    else
    {
        losight(env.show, grd, crawl_view.glosc());
    }

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
            (!you.running || Options.travel_delay > -1) && !you.asleep();

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
#ifdef USE_TILE
                const coord_def sep = ep - coord_def(1,1);
#endif

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
                        env.tile_bk_bg(gc) = env.tile_bg(sep);
                        env.tile_bk_fg(gc) = env.tile_fg(sep);
                    }

                    tileb[bufcount] = env.tile_fg(sep) =
                        tileidx_player(you.char_class);
                    tileb[bufcount+1] = env.tile_bg(sep);
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
                    tileb[bufcount]   = env.tile_fg(sep);
                    tileb[bufcount+1] = env.tile_bg(sep);
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
                                    get_clean_map_idx(env.tile_fg(sep));
                            }
                            else
                            {
                                env.tile_bk_fg(gc) = env.tile_fg(sep);
                            }
                            env.tile_bk_bg(gc) = env.tile_bg(sep);
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
                        see_grid(gc) ? real_colour(flash_colour)
                                     : DARKGREY;
                }

                bufcount += 2;
            }
        }

        if (!update_excludes.empty())
        {
            mark_all_excludes_non_updated();

            for (unsigned int k = 0; k < update_excludes.size(); k++)
                update_exclusion_los(update_excludes[k]);
        }

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
    hudsz = coord_def(HUD_WIDTH, HUD_HEIGHT + (Options.show_gold_turns ? 1 : 0));

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

/////////////////////////////////////////////////////////////////////////////
// monster_los

monster_los::monster_los()
    : gridx(0), gridy(0), mons(), range(LOS_RADIUS), los_field()
{
}

monster_los::~monster_los()
{
}

void monster_los::set_monster(monsters *mon)
{
    mons = mon;
    set_los_centre(mon->pos());
}

void monster_los::set_los_centre(int x, int y)
{
    if (!in_bounds(x, y))
        return;

    gridx = x;
    gridy = y;
}

void monster_los::set_los_range(int r)
{
    ASSERT (r >= 1 && r <= LOS_RADIUS);
    range = r;
}

coord_def monster_los::pos_to_index(coord_def &p)
{
    int ix = LOS_RADIUS + p.x - gridx;
    int iy = LOS_RADIUS + p.y - gridy;

    ASSERT(ix >= 0 && ix < LSIZE);
    ASSERT(iy >= 0 && iy < LSIZE);

    return (coord_def(ix, iy));
}

coord_def monster_los::index_to_pos(coord_def &i)
{
    int px = i.x + gridx - LOS_RADIUS;
    int py = i.y + gridy - LOS_RADIUS;

    ASSERT(in_bounds(px, py));
    return (coord_def(px, py));
}

void monster_los::set_los_value(int x, int y, bool blocked, bool override)
{
    if (!override && !is_unknown(x,y))
        return;

    coord_def c(x,y);
    coord_def lpos = pos_to_index(c);

    int value = (blocked ? L_BLOCKED : L_VISIBLE);

    if (value != los_field[lpos.x][lpos.y])
        los_field[lpos.x][lpos.y] = value;
}

int monster_los::get_los_value(int x, int y)
{
    // Too far away -> definitely out of sight!
    if (distance(x, y, gridx, gridy) > LOS_RADIUS * LOS_RADIUS)
        return (L_BLOCKED);

    coord_def c(x,y);
    coord_def lpos = pos_to_index(c);
    return (los_field[lpos.x][lpos.y]);
}

bool monster_los::in_sight(int x, int y)
{
    // Is the path to (x,y) clear?
    return (get_los_value(x,y) == L_VISIBLE);
}

bool monster_los::is_blocked(int x, int y)
{
    // Is the path to (x,y) blocked?
    return (get_los_value(x, y) == L_BLOCKED);
}

bool monster_los::is_unknown(int x, int y)
{
    return (get_los_value(x, y) == L_UNKNOWN);
}

static bool _blocks_movement_sight(monsters *mon, dungeon_feature_type feat)
{
    if (feat < DNGN_MINMOVE)
        return (true);

    if (!mon) // No monster defined?
        return (false);

    if (!mon->can_pass_through_feat(feat))
        return (true);

    return (false);
}

void monster_los::fill_los_field()
{
    int pos_x, pos_y;
    for (int k = 1; k <= range; k++)
        for (int i = -1; i <= 1; i++)
            for (int j = -1; j <= 1; j++)
            {
                if (i == 0 && j == 0) // Ignore centre grid.
                    continue;

                pos_x = gridx + k*i;
                pos_y = gridy + k*j;

                if (!in_bounds(pos_x, pos_y))
                    continue;

                if (!_blocks_movement_sight(mons, grd[pos_x][pos_y]))
                    set_los_value(pos_x, pos_y, false);
                else
                {
                    set_los_value(pos_x, pos_y, true);
                    // Check all beam potentially going through a blocked grid.
                    check_los_beam(pos_x, pos_y);
                }
            }
}

// (cx, cy) is the centre point
// (dx, dy) is the target we're aiming *through*
// target1 and target2 are targets we'll be aiming *at* to fire through (dx,dy)
static bool _set_beam_target(int cx, int cy, int dx, int dy,
                             int &target1_x, int &target1_y,
                             int &target2_x, int &target2_y,
                             int range)
{
    const int xdist = dx - cx;
    const int ydist = dy - cy;

    if (xdist == 0 && ydist == 0)
        return (false); // Nothing to be done.

    if (xdist <= -range || xdist >= range
        || ydist <= -range || ydist >= range)
    {
        // Grids on the edge of a monster's LOS don't block sight any further.
        return (false);
    }

/*
 *   The following code divides the field into eights of different directions.
 *
 *    \  NW | NE  /
 *      \   |   /
 *    WN  \ | /   EN
 *   ----------------
 *    WS  / | \   ES
 *      /   |   \
 *    /  SW | SE  \
 *
 *   target1_x and target1_y mark the base line target, so the base beam ends
 *   on the diagonal line closest to the target (or on one of the straight
 *   lines if cx == dx or dx == dy).
 *
 *   target2_x and target2_y then mark the second target our beam finder should
 *   cycle through. It'll always be target2_x = dx or target2_y = dy, the other
 *   being on the edge of LOS, which one depending on the quadrant.
 *
 *   The beam finder can then cycle from the nearest corner (target1) to the
 *   second edge target closest to (dx,dy).
 */

    if (xdist == 0)
    {
        target1_x = cx;
        target1_y = (ydist > 0 ? cy + range
                               : cy - range);

        target2_x = target1_x;
        target2_y = target1_y;
    }
    else if (ydist == 0)
    {
        target1_x = (xdist > 0 ? cx + range
                               : cx - range);
        target1_y = cy;

        target2_x = target1_x;
        target2_y = target1_y;
    }
    else if (xdist < 0 && ydist < 0 || xdist > 0 && ydist > 0)
    {
        if (xdist < 0)
        {
            target1_x = cx - range;
            target1_y = cy - range;
        }
        else
        {
            target1_x = cx + range;
            target1_y = cy + range;
        }

        if (xdist == ydist)
        {
            target2_x = target1_x;
            target2_y = target1_y;
        }
        else
        {
            if (xdist < 0) // both are negative (upper left corner)
            {
                if (dx > dy)
                {
                    target2_x = dx;
                    target2_y = cy - range;
                }
                else
                {
                    target2_x = cx - range;
                    target2_y = dy;
                }
            }
            else // both are positive (lower right corner)
            {
                if (dx > dy)
                {
                    target2_x = cx + range;
                    target2_y = dy;
                }
                else
                {
                    target2_x = dx;
                    target2_y = cy + range;
                }
            }
        }
    }
    else if (xdist < 0 && ydist > 0 || xdist > 0 && ydist < 0)
    {
        if (xdist < 0) // lower left corner
        {
            target1_x = cx - range;
            target1_y = cy + range;
        }
        else // upper right corner
        {
            target1_x = cx + range;
            target1_y = cy - range;
        }

        if (xdist == -ydist)
        {
            target2_x = target1_x;
            target2_y = target1_y;
        }
        else
        {
            if (xdist < 0) // ydist > 0
            {
                if (-xdist > ydist)
                {
                    target2_x = cx - range;
                    target2_y = dy;
                }
                else
                {
                    target2_x = dx;
                    target2_y = cy + range;
                }
            }
            else // xdist > 0, ydist < 0
            {
                if (-xdist > ydist)
                {
                    target2_x = dx;
                    target2_y = cy - range;
                }
                else
                {
                    target2_x = cx + range;
                    target2_y = dy;
                }
            }
        }
    }
    else
    {
        // Everything should have been handled above.
        ASSERT(false);
    }

    return (true);
}

void monster_los::check_los_beam(int dx, int dy)
{
    ray_def ray;

    int target1_x = 0, target1_y = 0, target2_x = 0, target2_y = 0;
    if (!_set_beam_target(gridx, gridy, dx, dy, target1_x, target1_y,
                          target2_x, target2_y, range))
    {
        // Nothing to be done.
        return;
    }

    if (target1_x > target2_x || target1_y > target2_y)
    {
        // Swap the two targets so our loop will work correctly.
        int help = target1_x;
        target1_x = target2_x;
        target2_x = help;

        help = target1_y;
        target1_y = target2_y;
        target2_y = help;
    }

    const int max_dist = range;
    int dist;
    bool blocked = false;
    for (int tx = target1_x; tx <= target2_x; tx++)
        for (int ty = target1_y; ty <= target2_y; ty++)
        {
            // If (tx, ty) lies outside the level boundaries, there's nothing
            // that shooting a ray into that direction could bring us, esp.
            // as earlier grids in the ray will already have been handled, and
            // out of bounds grids are simply skipped in any LoS check.
            if (!map_bounds(tx, ty));
                continue;

            // Already calculated a beam to (tx, ty), don't do so again.
            if (!is_unknown(tx, ty))
                continue;

            dist = 0;
            ray.fullray_idx = -1; // to quiet valgrind
            find_ray( coord_def(gridx, gridy), coord_def(tx, ty),
                      true, ray, 0, true, true );

            if (ray.x() == gridx && ray.y() == gridy)
                ray.advance(true);

            while (dist++ <= max_dist)
            {
                // The ray brings us out of bounds of the level map.
                // Since we're always shooting outwards there's nothing more
                // to look at in that direction, and we can break the loop.
                if (!map_bounds(ray.x(), ray.y()))
                    break;

                if (blocked)
                {
                    // Earlier grid blocks this beam, set to blocked if
                    // unknown, but don't overwrite visible grids.
                    set_los_value(ray.x(), ray.y(), true);
                }
                else if (_blocks_movement_sight(mons, grd[ray.x()][ray.y()]))
                {
                    set_los_value(ray.x(), ray.y(), true);
                    // The rest of the beam will now be blocked.
                    blocked = true;
                }
                else
                {
                    // Allow overriding in case another beam has marked this
                    // field as blocked, because we've found a solution where
                    // that isn't the case.
                    set_los_value(ray.x(), ray.y(), false, true);
                }
                if (ray.x() == tx && ray.y() == ty)
                    break;

                ray.advance(true);
            }
        }
}
