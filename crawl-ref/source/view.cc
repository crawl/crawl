/*
 *  File:       view.cc
 *  Summary:    Misc function used to render the dungeon.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author: dshaligram $ on $Date: 2007-11-15 18:51:59 +0100 (Thu, 15 Nov 2007) $
 *
 *  Modified for Hexcrawl by Martin Bays, 2007
 *
 *  Change History (most recent first):
 *
 *   <10>     29 Jul 00  JDJ    show_map iterates horizontally to 79 instead of 80.
 *                              item no longer indexes past the end of environ::grid.
 *   <9>      19 Jun 00  GDL    Complete rewrite of LOS code
 *   <8>      11/23/99   LRH    Added colour-coded play-screen map & clean_map
 *                                                              init options
 *   <7>      9/29/99    BCR    Removed first argument from draw_border
 *   <6>      9/11/99    LRH    Added calls to overmap functions
 *   <5>      6/22/99    BWR    Fixed and improved the stealth
 *   <4>      5/20/99    BWR    show_map colours all portals,
 *                              exits from subdungeons now
 *                              look like up stairs.
 *   <3>      5/09/99    JDJ    show_map draws shops in yellow.
 *   <2>      5/09/99    JDJ    show_map accepts '\r' along with '.'.
 *   <1>      -/--/--    LRH    Created
 */

#include "AppHdr.h"
#include "view.h"

#include <string.h>
#include <cmath>
#include <sstream>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "command.h"
#include "cio.h"
#include "cloud.h"
#include "clua.h"
#include "database.h"
#include "debug.h"
#include "delay.h"
#include "direct.h"
#include "dungeon.h"
#include "format.h"
#include "initfile.h"
#include "itemprop.h"
#include "luadgn.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "monstuff.h"
#include "mon-util.h"
#include "overmap.h"
#include "player.h"
#include "skills.h"
#include "religion.h"
#include "skills2.h"
#include "stuff.h"
#include "spells4.h"
#include "stash.h"
#include "state.h"
#include "terrain.h"
#include "travel.h"
#include "tutorial.h"

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

unsigned char show_green;
extern int stealth;             // defined in acr.cc

// char colour_code_map(unsigned char map_value);
screen_buffer_t colour_code_map( int x, int y, bool item_colour = false,
                                 bool travel_colour = false );

void cloud_grid(void);
void monster_grid(bool do_updates);

static int gcd( int x, int y );

static void get_symbol( int x, int y,
                        int object, unsigned *ch, 
                        unsigned short *colour,
                        bool magic_mapped = false );
static unsigned get_symbol(int object, unsigned short *colour = NULL,
                           bool magic_mapped = false);

static int get_item_dngn_code(const item_def &item);
static void set_show_backup( int ex, int ey );
static int get_viewobj_flags(int viewobj);

const feature_def &get_feature_def(dungeon_feature_type feat)
{
    return (Feature[feat]);
}

unsigned map_cell::glyph() const
{
    if (!object)
        return (' ');
    return get_symbol(object, NULL, !(flags & MAP_SEEN_FLAG));
}

bool map_cell::known() const
{
    return (object && (flags & MAP_GRID_KNOWN));
}

bool map_cell::seen() const
{
    return (object && (flags & MAP_SEEN_FLAG));
}

inline unsigned get_envmap_char(int x, int y)
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
}

void set_envmap_obj( int x, int y, int obj )
{
    env.map[x][y].object = obj;
}

void set_envmap_col( int x, int y, int colour )
{
    env.map[x][y].colour = colour;
}

bool is_envmap_item(int x, int y)
{
    return (get_viewobj_flags(env.map[x][y].object) & MC_ITEM);
}

bool is_envmap_mons(int x, int y)
{
    return (get_viewobj_flags(env.map[x][y].object) & MC_MONS);
}

int get_envmap_col(int x, int y)
{
    return (env.map[x][y].colour);
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

// used to mark dug out areas, unset when terrain is seen or mapped again.
void set_terrain_changed( int x, int y )
{
    env.map[x][y].flags |= MAP_CHANGED_FLAG;
}

void set_terrain_mapped( int x, int y )
{
    env.map[x][y].flags &= (~MAP_CHANGED_FLAG);
    env.map[x][y].flags |= MAP_MAGIC_MAPPED_FLAG;
}

void set_terrain_seen( int x, int y )
{
    env.map[x][y].flags &= (~MAP_CHANGED_FLAG);
    env.map[x][y].flags |= MAP_SEEN_FLAG;
}

void clear_envmap_grid( int x, int y )
{
    env.map[x][y].clear();
}

bool is_notable_terrain(dungeon_feature_type ftype)
{
    return (Feature[ftype].is_notable());
}

#if defined(WIN32CONSOLE) || defined(DOS)
static unsigned colflag2brand(int colflag)
{
    switch (colflag)
    {
    case COLFLAG_ITEM_HEAP:
        return (Options.heap_brand);
    case COLFLAG_FRIENDLY_MONSTER:
        return (Options.friend_brand);
    case COLFLAG_WILLSTAB:
        return (Options.stab_brand);
    case COLFLAG_MAYSTAB:
        return (Options.may_stab_brand);
    case COLFLAG_STAIR_ITEM:
        return (Options.stair_item_brand);
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

#if defined(WIN32CONSOLE) || defined(DOS)
    if (colflags)
    {
        unsigned brand = colflag2brand(colflags);
        raw_colour = dos_brand(raw_colour & 0xFF, brand);
    }
#endif

#ifndef USE_COLOUR_OPTS
    // Strip COLFLAGs for systems that can't do anything meaningful with them.
    raw_colour &= 0xFF;
#endif

    return (raw_colour);
}

static int get_viewobj_flags(int object)
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

static unsigned get_symbol(int object, unsigned short *colour,
                           bool magic_mapped)
{
    unsigned ch;
    get_symbol(0, 0, object, &ch, NULL, magic_mapped);
    return (ch);
}

static int view_emphasised_colour(int x, int y, dungeon_feature_type feat,
                                  int oldcolour, int newcolour)
{
    if (is_travelable_stair(feat) && !travel_cache.know_stair(coord_def(x, y)))
    {
        if ((you.your_level || grid_stair_direction(feat) == CMD_GO_DOWNSTAIRS)
            && you.where_are_you != BRANCH_VESTIBULE_OF_HELL)
            return (newcolour);
    }
    return (oldcolour);
}

static void get_symbol( int x, int y,
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

            if (object < NUM_REAL_FEATURES && env.grid_colours[x][y])
            {
                *colour = env.grid_colours[x][y] | colmask;
            }
            else
            {
                // Don't clobber with BLACK, because the colour should be
                // already set.
                if (fdef.colour != BLACK)
                    *colour = fdef.colour | colmask;

                if (fdef.em_colour != fdef.colour && fdef.em_colour)
                    *colour =
                        view_emphasised_colour(
                            x, y, static_cast<dungeon_feature_type>(object),
                            *colour, fdef.em_colour | colmask);
            }
        }

        // Note anything we see that's notable
        if ((x || y) && fdef.is_notable())
            seen_notable_thing( static_cast<dungeon_feature_type>(object),
                                x, y );
    }
    else
    {
        ASSERT( object >= DNGN_START_OF_MONSTERS );
        *ch = mons_char( object - DNGN_START_OF_MONSTERS );
    }

    if (colour)
        *colour = real_colour(*colour);
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

static char get_travel_colour( int x, int y )
{
    if (is_waypoint(x, y))
        return LIGHTGREEN;

    short dist = travel_point_distance[x][y];
    return dist > 0?                    Options.tc_reachable        :
           dist == PD_EXCLUDED?         Options.tc_excluded         :
           dist == PD_EXCLUDED_RADIUS?  Options.tc_exclude_circle   :
           dist < 0?                    Options.tc_dangerous        :
                                        Options.tc_disconnected;
}
            
#if defined(WIN32CONSOLE) || defined(DOS)
static unsigned short dos_reverse_brand(unsigned short colour)
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

static unsigned short dos_hilite_brand(unsigned short colour,
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
        return dos_hilite_brand(colour, (brand & CHATTR_COLMASK) >> 8);
    else
        return dos_reverse_brand(colour);
    
}
#endif

// FIXME: Rework this function to use the new terrain known/seen checks
// These are still env.map coordinates, NOT grid coordinates!
screen_buffer_t colour_code_map( int x, int y, bool item_colour, 
                                 bool travel_colour )
{
    const unsigned short map_flags = env.map[x][y].flags;
    if (!(map_flags & MAP_GRID_KNOWN))
        return (BLACK);
    
    const dungeon_feature_type grid_value = grd[x][y];

    unsigned tc = travel_colour? 
                        get_travel_colour(x, y)
                      : DARKGREY;
    
    if (map_flags & MAP_DETECTED_ITEM)
        return real_colour(Options.detected_item_colour);
    
    if (map_flags & MAP_DETECTED_MONSTER)
    {
        tc = Options.detected_monster_colour;
        return real_colour(tc);
    }

    // If this is an important travel square, don't allow the colour
    // to be overridden.
    if (is_waypoint(x, y) || travel_point_distance[x][y] == PD_EXCLUDED)
        return real_colour(tc);

    if (item_colour && is_envmap_item(x, y))
        return get_envmap_col(x, y);

    int feature_colour = DARKGREY;
    const bool terrain_seen = is_terrain_seen(x, y);
    const feature_def &fdef = Feature[grid_value];
    feature_colour = terrain_seen? fdef.seen_colour : fdef.map_colour;

    if (terrain_seen && feature_colour != fdef.seen_em_colour
        && fdef.seen_em_colour)
    {
        feature_colour =
            view_emphasised_colour(x, y, grid_value, feature_colour,
                                   fdef.seen_em_colour);
    }

    if (feature_colour != DARKGREY)
        tc = feature_colour;

    if (Options.stair_item_brand
        && is_stair(grid_value) && igrd[x][y] != NON_ITEM)
    {
        tc |= COLFLAG_STAIR_ITEM;
    }
    else if (Options.trap_item_brand
             && grid_is_trap(grid_value) && igrd[x][y] != NON_ITEM)
    {
        tc |= COLFLAG_TRAP_ITEM;
    }
    
    return real_colour(tc);
}

void clear_map(bool clear_detected_items, bool clear_detected_monsters)
{
    for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
    {
        for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
        {
            // Don't expose new dug out areas:
            // Note: assumptions are being made here about how 
            // terrain can change (eg it used to be solid, and
            // thus monster/item free).
            if (is_terrain_changed(x, y))
                continue;

            unsigned envc = get_envmap_char(x, y);
            if (!envc)
                continue;

            if (is_envmap_item(x, y))
                continue;

            if (!clear_detected_items && is_envmap_detected_item(x, y))
                continue;

            if (!clear_detected_monsters && is_envmap_detected_mons(x, y))
                continue;

            set_envmap_obj(x, y, is_terrain_known(x, y)? grd[x][y] : 0);
            set_envmap_detected_mons(x, y, false);
            set_envmap_detected_item(x, y, false);
        }
    }
}

int get_mons_colour(const monsters *mons)
{
    int col = mons->colour;

    if (mons->has_ench(ENCH_BERSERK))
        col = RED;
    
    if (mons_friendly(mons))
    {
        col |= COLFLAG_FRIENDLY_MONSTER;
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
    else if (Options.heap_brand != CHATTR_NORMAL
             && mons_is_stationary(mons)
             && in_bounds(mons->x, mons->y)
             && igrd[mons->x][mons->y] != NON_ITEM)
    {
        col |= COLFLAG_ITEM_HEAP;
    }

    // Backlit monsters are fuzzy and override brands.
    if (!player_see_invis() && mons->has_ench(ENCH_INVIS)
        && mons->has_ench(ENCH_BACKLIGHT))
    {
        col = DARKGREY;
    }

    return (col);
}

static std::set<const monsters*> monsters_seen_this_turn;

static bool mons_was_seen_this_turn(const monsters *mons)
{
    return (monsters_seen_this_turn.find(mons) !=
            monsters_seen_this_turn.end());
}

void beogh_follower_convert(monsters *monster, bool orc_hit)
{
    if (you.species != SP_HILL_ORC)
        return;
    
    const bool is_orc = mons_species(monster->type) == MONS_ORC;
    // for followers of Beogh, decide whether orcs will join you
    if (you.religion == GOD_BEOGH
        && monster->foe == MHITYOU
        && !(monster->flags & MF_CONVERT_ATTEMPT)
        && is_orc
        && !mons_friendly(monster)
        && mons_player_visible(monster) && !mons_is_sleeping(monster)
        && !mons_is_confused(monster) && !mons_is_paralysed(monster))
    {
        monster->flags |= MF_CONVERT_ATTEMPT;

        const int hd = monster->hit_dice;

        if (you.piety >= 75 && !you.penance[GOD_BEOGH] &&
            random2(you.piety / 15) + random2(4 + you.experience_level / 3)
              > random2(hd) + hd + random2(5))
        {
            int wpn = you.equip[EQ_WEAPON];
            if (wpn != -1
                && you.inv[wpn].base_type == OBJ_WEAPONS
                && get_weapon_brand( you.inv[wpn] ) == SPWPN_ORC_SLAYING
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
    else if (is_orc
             && !(you.religion == GOD_BEOGH)
             && monster->attitude == ATT_FRIENDLY
             && (monster->flags & MF_CONVERT_ATTEMPT)
             && (monster->flags & MF_GOD_GIFT)
             && mons_player_visible(monster) && !mons_is_sleeping(monster)
             && !mons_is_confused(monster) && !mons_is_paralysed(monster))
    {      // reconversion if no longer Beogh
                
        monster->attitude = ATT_HOSTILE;
        behaviour_event(monster, ME_ALERT, MHITYOU);
        // CREATED_FRIENDLY stays -> no piety bonus on killing these
                
        // give message only sometimes
        if (player_monster_visible(monster) && random2(4)) 
        {
            msg::streams(MSGCH_MONSTER_ENCHANT)
                << monster->name(DESC_CAP_THE)
                << " deserts you."
                << std::endl;
        }
    }
}

void handle_monster_shouts(monsters* monster, bool force)
{
    if (!force
        && (!you.turn_is_over || random2(30) < you.skills[SK_STEALTH]))
        return;

    // Get it once, since monster might be S_RANDOM, in which case
    // mons_shouts() will return a different value every time.
    shout_type type = mons_shouts(monster->type);

    // Silent monsters can give noiseless "visual shouts" if the
    // player can see them, in which case silence isn't checked for.
    if (!force && mons_friendly(monster)
        || (type == S_SILENT && !player_monster_visible(monster))
        || (type != S_SILENT && (silenced(you.x_pos, you.y_pos) 
                                 || silenced(monster->x, monster->y))))
        return;

    int         noise_level = get_shout_noise_level(type);
    std::string default_msg_key;

    switch (type)
    {
    case NUM_SHOUTS:
    case S_RANDOM:
        default_msg_key = "__BUGGY";
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
    default:
        default_msg_key = "";
    }

    // Use get_monster_data(monster->type) to bypass mon_shouts()
    // replacing S_RANDOM with a random value.
    if (mons_is_demon( monster->type ) && coinflip()
        && (type != S_SILENT ||
            get_monster_data(monster->type)->shouts == S_RANDOM))
    {
        noise_level     = 8;
        default_msg_key = "__DEMON_TAUNT";
    }

    std::string msg, suffix;
    std::string key = mons_type_name(monster->type, DESC_PLAIN);

    // Pandemonium demons have random names, so use "pandemonium lord"
    if (monster->type == MONS_PANDEMONIUM_DEMON)
        key = "pandemonium lord";
    // Search for player ghost shout by the ghost's class.
    else if (monster->type == MONS_PLAYER_GHOST)
    {
        const ghost_demon &ghost = *(monster->ghost);
        std::string ghost_class = get_class_name(ghost.values[GVAL_CLASS]);

        key = ghost_class + " player ghost";

        default_msg_key = "player ghost";
    }

    // Tries to find an entry for "name seen" or "name unseen",
    // and if no such entry exists then looks simply for "name".
    if (player_monster_visible(monster))
        suffix = " seen";
     else
        suffix = " unseen";

    msg = getShoutString(key, suffix);

    if (msg == "__DEFAULT" || msg == "__NEXT")
        msg = getShoutString(default_msg_key, suffix);
    else if (msg == "")
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

        if (msg == "" || msg == "__DEFAULT")
            msg = getShoutString(default_msg_key, suffix);
    }

    if (default_msg_key == "__BUGGY")
        msg::streams(MSGCH_SOUND) << "You hear something buggy!"
                                  << std::endl;
    else if ((msg == "" || msg == "__NONE")
             && mons_shouts(monster->type) == S_SILENT)
        ; // No "visual shout" defined for silent monster, do nothing
    else if (msg == "")
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
        msg = do_mon_str_replacements(msg, monster);
        msg_channel_type channel = MSGCH_TALK;
        
        std::string param = "";
        std::string::size_type pos = msg.find(":");

        if (pos != std::string::npos)
        {
            param = msg.substr(0, pos);
            msg   = msg.substr(pos + 1);
        }

        if (mons_shouts(monster->type) == S_SILENT || param == "VISUAL")
            channel = MSGCH_TALK_VISUAL;
        else if (param == "SOUND")
            channel = MSGCH_SOUND;

        msg::streams(channel) << msg << std::endl;
    }

    if (noise_level > 0)
        noisy( noise_level, monster->x, monster->y );
}

#ifdef WIZARD
void force_monster_shout(monsters* monster)
{
    handle_monster_shouts(monster, true);
}
#endif

inline static bool update_monster_grid(const monsters *monster)
{
    const int ex = monster->x - you.x_pos + 9;
    const int ey = monster->y - you.y_pos + 9;

    if (!player_monster_visible( monster ))
    {
        // ripple effect?
        if (grd[monster->x][monster->y] == DNGN_SHALLOW_WATER
            && !mons_flies(monster)
            && env.cgrid(monster->pos()) == EMPTY_CLOUD)
        {
            set_show_backup(ex, ey);
            env.show[ex][ey] = DNGN_INVIS_EXPOSED;
            env.show_col[ex][ey] = BLUE; 
        }
        return (false);
    }

    // mimics are always left on map
    if (!mons_is_mimic( monster->type ))
        set_show_backup(ex, ey);

    env.show[ex][ey] = monster->type + DNGN_START_OF_MONSTERS;
    env.show_col[ex][ey] = get_mons_colour( monster );
    return (true);
}

void monster_grid(bool do_updates)
{
    monsters *monster = NULL;

    for (int s = 0; s < MAX_MONSTERS; s++)
    {
        monster = &menv[s];

        if (monster->type != -1 && mons_near(monster))
        {
            if (do_updates 
                && (monster->behaviour == BEH_SLEEP
                     || monster->behaviour == BEH_WANDER) 
                && check_awaken(monster))
            {
		if ( monster->behaviour == BEH_SLEEP &&
			mons_holiness(monster) == MH_NATURAL &&
			!check_awaken(monster, 20, true))
		    // sleeping monsters are first merely awoken, but get a
		    // free try at detecting the player, with a bonus
		    behaviour_event( monster, ME_DISTURB, MHITNOT, you.x_pos,
			    you.y_pos );
		else
		{
		    behaviour_event( monster, ME_ALERT, MHITYOU );
		    handle_monster_shouts(monster);
		}
            }

            if (!update_monster_grid(monster))
                continue;
            
            if (player_monster_visible(monster)
                && !mons_is_submerged(monster)
                && !mons_friendly(monster)
                && !mons_class_flag(monster->type, M_NO_EXP_GAIN)
                && !mons_is_mimic(monster->type))
            {
                monsters_seen_this_turn.insert(monster);
            }

            beogh_follower_convert(monster);
        }
    }
}

void fire_monster_alerts()
{
    for (int s = 0; s < MAX_MONSTERS; s++)
    {
        monsters *monster = &menv[s];

        if (monster->alive() && mons_near(monster))
        {
            if ((player_monster_visible(monster)
                 || mons_was_seen_this_turn(monster))
                && !mons_is_submerged( monster ))
            {
                activity_interrupt_data aid(monster);
                if (testbits(monster->flags, MF_WAS_IN_VIEW))
                    aid.context = "already seen";
                else
                    aid.context = "newly seen";

                if (!mons_is_safe( static_cast<const monsters*>(monster) )
                    && !mons_class_flag( monster->type, M_NO_EXP_GAIN )
                    && !mons_is_mimic( monster->type ))
                {
                    interrupt_activity( AI_SEE_MONSTER, aid );
                }
                seen_monster( monster );

                // Monster was viewed this turn
                monster->flags |= MF_WAS_IN_VIEW;
            }
            else
            {
                // Monster was not viewed this turn
                monster->flags &= ~MF_WAS_IN_VIEW;
            }
        }
        else
        {
            // Monster was not viewed this turn
            monster->flags &= ~MF_WAS_IN_VIEW;
        }
    }

    monsters_seen_this_turn.clear();
}

static int stealth_subskill_mod(int skill, int skill_factor, int subskill,
	bool active)
{
    int start_lev = 0, pow_init = 0, pow_diverted = 0, pow_factor = 0;
    int mod = 0;

    // SK_STEALTH-based bonuses, general remarks:
    //
    // The first few levels of SK_STEALTH go entirely into general sneakiness,
    // adding species_stealth_factor() to stealth per skill level. See
    // check_stealth() in player.cc for that.
    //
    // At higher levels, we start adding in situation-dependent stealth
    // subskills. Each such subskill gets a certain mild power for free at the
    // level it becomes active. For each SK_STEALTH level increase above that
    // level, part of the bonus is diverted from general stealth into
    // increasing the power of this subskill. When the situation-dependent
    // subskills are active, the bonuses they give are significantly more than
    // they would be had the points gone into general sneakiness.
    //
    // So to contrast this with the old model where all bonuses went wholly
    // into general sneakiness: blundering thoughtlessly up to monsters is now
    // nerfed at high SK_STEALTH levels, but actively using the tactical
    // layout of the dungeon to sneaky effect gives fairly significant
    // bonuses.

    switch (subskill)
    {
	// these number are for "normal" racial stealthiness factor, i.e. 15
	// points per SK_STEALTH level. Scaling done below.
	case STSSK_PEEK:
	    start_lev = 4;      // SK_STEALTH level when this becomes active
	    pow_init = 8;       // free bonus to power of this subskill
	    pow_diverted = 2;   // skill points per SK_STEALTH level used
	    pow_factor = 4;     // effect when active per skill point used
	    break;
	case STSSK_WALLS:
	    start_lev = 6;
	    pow_init = 12;
	    pow_diverted = 3;
	    pow_factor = 4;
	    break;
	case STSSK_FREEZE:
	    start_lev = 8;
	    pow_init = 10;
	    pow_diverted = 1;
	    pow_factor = 5;
	    break;
    }

    if (skill >= start_lev)
    {
	mod -= (skill - start_lev) * pow_diverted;
	if (active)
	    mod += pow_init + (skill - start_lev) * pow_diverted * pow_factor;

	// adjust all mods (positive and negative) according to racial skill
	// factor:
	mod *= skill_factor;
	mod /= 15;
    }

    return mod;
}

bool check_awaken(monsters* monster, int modifier, bool no_exercise)
{
    int mons_perc = 0;
    int effective_stealth = stealth;
    const mon_holy_type mon_holy = mons_holiness(monster);
    coord_def you_pos = you.pos();
    ray_def ray, direct_ray;
    bool los = false, clouded = false, obscured = false, obscured_adjacent = false;
    int dist, dist_to_wall = 1;

    // Monsters put to sleep by ensorcelled hibernation will sleep
    // at least one turn.
    if (monster->has_ench(ENCH_SLEEPY))
        return (false);

    // berserkers aren't really concerned about stealth
    if (you.duration[DUR_BERSERKER])
        return (true);

    // Repel undead is a holy aura, to which evil creatures are sensitive.
    // Note that even though demons aren't affected by repel undead, they
    // do sense this type of divine aura. -- bwr
    if (you.duration[DUR_REPEL_UNDEAD] 
        && (mon_holy == MH_UNDEAD || mon_holy == MH_DEMONIC))
    {
        return (true);
    }

    // Do some ray-casting
    los = find_ray( monster->pos(), you_pos, false, ray,
	    0, true );

    if (los)
    {
	// XXX: possible source of infinite loops, if ray code is ever
	// changed! Similarly with direct_ray below.
	while (ray.pos() != you_pos)
	{
	    if ( !clouded && is_opaque_cloud(env.cgrid(ray.pos())) ) 
		clouded = true;
	    ray.advance();
	}

	for (dist_to_wall = 1; dist_to_wall <= 5; dist_to_wall++)
	{
	    ray.advance();
	    if (!in_G_bounds(ray.pos()))
		break;
	    if ( grid_is_opaque(grd(ray.pos())) )
		break;
	}
    }

    find_ray( monster->pos(), you_pos, true, direct_ray, 0, true, true );

    while (direct_ray.pos() != you_pos)
    {
	if ( grid_is_opaque(grd(direct_ray.pos())) )
	{
	    obscured = true;
	    obscured_adjacent = true;
	}
	else
	    obscured_adjacent = false;
	direct_ray.advance();
    }


    // I assume that creatures who can sense invisible are very perceptive
    mons_perc = 10 + (mons_intel(monster->type) * 4) + monster->hit_dice
                   + mons_sense_invis(monster) * 5;

    // critters that are wandering but have MHITYOU as their foe are
    // still actively on guard for the player, even if they can't see
    // him. Give them a large bonus (handle_behaviour() will nuke 'foe'
    // after a while, removing this bonus).
    if (monster->behaviour == BEH_WANDER && monster->foe == MHITYOU)
    {
	mons_perc *= 14;
	mons_perc /= 10;
    }

    // Bonus if the current target of the monster's wandering attention is
    // near the player, as for example it will be if it heard a noise round
    // there. Cumulative with the previous bonus.
    if (monster->behaviour == BEH_WANDER)
    {
	int dist_from_target = grid_distance(monster->target_x,
		monster->target_y, you.x_pos, you.y_pos);

	if (dist_from_target == 0)
	    mons_perc *= 13;
	else if (dist_from_target <= 2)
	    mons_perc *= 12;
	else if (dist_from_target <= 5)
	    mons_perc *= 11;
	else
	    mons_perc *= 10;
	mons_perc /= 10;
    }

    if (monster->behaviour == BEH_SLEEP)
    {
        if (mon_holy == MH_NATURAL)
        {
            // monster is "hibernating"... reduce chance of waking
            if (monster->has_ench(ENCH_SLEEP_WARY))
	    {
                mons_perc *= 7;
		mons_perc /= 10;
	    }
        }
        else // unnatural creature
        {
            // Unnatural monsters don't actually "sleep", they just 
            // haven't noticed an intruder yet... we'll assume that
            // they're diligently on guard.
            //mons_perc += 10;

	    // Z: No, that's silly. Why should they get this bonus at first,
	    // but not later when in BEH_WANDER? It's also rather
	    // counter-intuitive, and so not easily discoverable without
	    // reading the source.
            mons_perc += 0;
        }
    }

    // vary perception with distance to target.
    // approx 1.25 bonus at dist 1, 1.0 at dist 5, approx 0.75 at dist 9
    dist = grid_distance(monster->x, monster->y, you.x_pos, you.y_pos);
    if (dist > 9)
	dist = 9;
    mons_perc *= (20-dist);
    mons_perc /= 15;

    if (modifier != 10)
    {
	mons_perc *= modifier;
	mons_perc /= 10;
    }

    if (!mons_player_visible(monster) || !los)
    {
	// can't see you (due to invisibility or no line of sight)

	// XXX: nerfing invisibility a little. Sound counts for something too.
        //mons_perc -= 75;
	mons_perc /= 5;
    }
    else
    {
	// boni which only make sense if the monster can see the player:

	// If you've been tagged with Corona or are Glowing, the glow
	// makes you extremely easy to see.
	if (you.backlit())
	    mons_perc *= 2;

	if (obscured)
	{
	    mons_perc *= 17;
	    mons_perc /= 20;
	}

	if (clouded)
	    mons_perc /= 2;

	// easier to see you if you're in the middle of a room/corridor.
	// cf. STSSK_WALLS bonus to stealth below
	switch (dist_to_wall)
	{
	    case 1: mons_perc *= 9; break;
	    case 2: case 3: mons_perc *= 10; break;
	    case 4: mons_perc *= 11; break;
	    default: mons_perc *= 12;
	}
	mons_perc /= 10;
    }

    if (mons_perc < 0)
        mons_perc = 0;

    if (los && mons_player_visible(monster))
    {
	int skill = you.skills[SK_STEALTH];
	int factor = species_stealth_factor(you.species);

	if ( testbits( monster->flags, MF_BATTY ))
	{
	    // batty creatures produce lots of spurious stealth tests, so
	    // let's not have them exercising stealth
	    no_exercise = true;
	}

	// subskills:

	// peeking round corners
	if (skill >= 4)
	{
	    effective_stealth += stealth_subskill_mod(skill, factor,
		    STSSK_PEEK, obscured_adjacent);
	    if (!no_exercise && obscured_adjacent && one_chance_in(12))
		exercise(SK_STEALTH, 1);
	}
	// hiding against walls:
	if (skill >= 6)
	{
	    effective_stealth += stealth_subskill_mod(skill, factor,
		    STSSK_WALLS, dist_to_wall == 1);
	    if (!no_exercise && dist_to_wall == 1 && one_chance_in(24))
		exercise(SK_STEALTH, 1);
	}
	// being highly inconspicuous while pausing
	if (skill >= 8)
	{
	    effective_stealth += stealth_subskill_mod(skill, factor,
		    STSSK_FREEZE, you.stealthy_action);
	    if (!no_exercise && you.stealthy_action && one_chance_in(24))
		exercise(SK_STEALTH, 1);
	}

	// TODO: probably STSSK_WALLS shouldn't work against green crystal?
    }

    // [tactics] Notes on the changes to stealth:
    //
    // 1. More detailed set of situation-dependent bonuses and maluses, taking
    //	    into account distance between you and the monster, distance from
    //	    walls, intervening walls/smoke, whether the monster's attention is
    //	    focused near your current position, and a few other things.
    //
    // 2. We make some bonuses depend on SK_STEALTH and only begin
    //	    at a certain SK_STEALTH level. That way, we can give a message
    //	    when you get to the appropriate SK_STEALTH level ("You learn to
    //	    peek stealthily around corners!"), both adding interest and giving
    //	    hints as to how to act stealthy without having to source-dive.
    //
    //	    The other advantage of this is that characters for whom stealth
    //	    isn't a major deal shouldn't feel they have to adjust their
    //	    playstyle to the new rules.
    //
    //	    See stealth_subskill_mod() for implementation details.
    //
    //	    Such stealth-skills thus far:
    //
    //	    Skulking against walls (dist_to_wall==1)
    //	    Peeking around corners (obscured_adjacent)
    //	    Staying very still (stealthy_action)
    //
    //
    // Further ideas to make stealth more interesting and tactical:
    //
    // 1. Suggestions for further subskills:
    //	    Actions other than '.' which come to be considered stealthy - e.g.
    //		wielding weapons, wearing jewelry, drinking potions,
    //		praying...
    //	    Greater bonuses for levitation/flying
    //	    Hiding behind adjacent monsters (friendly or not!) - bonus
    //		depending on size of monster (and significantly reduced if
    //		it's attacking you!). Note: would make sense to have a dodging
    //		bonus at high SK_DODGING which works similarly.
    //		Actually, this shouldn't work for friendly monsters - since
    //		the code has all monsters artificially infinitely stealthy,
    //		this would be abusive.
    //	    Monsters start randomly losing track of your position (as (and
    //		instead of) Misdirection below)
    //
    // 2. At a high enough SK_STEALTH levels, 'a' abilities for special
    // stealth-related actions.
    //
    //	    Ideas for such:
    //		Play dead: lie prone, and hope the monsters ignore you!
    //		Implementable as a significant increase to stealthiness as
    //		long as you stay still, but with a significant delay in coming
    //		out of it and a big stabbing-like bonus to any attacks on you
    //		while prone. Furthermore, monsters attacking you when you
    //		activate it may stop targetting you.
    //
    //		Leap from shadows: hmm, not sure how this would work actually.
    //
    //		Misdirection: if successful, target monster loses track of you
    //		(target set to your last known location, and a perception
    //		boost while it searches for you).
    //
    //	    These would probably be silly.
    //
    //	    UPDATE: Leap from shadows implemented, but disabled (funky, but
    //	    silly.)
    //
    // 3. A full-on lighting model, only introduced at high enough SK_STEALTH
    // level... I think not.
    //
    // 4. Successfully shooting a monster shouldn't *automatically* give you
    // away... though should make them come over while looking around
    // carefully.
    //
    // 5. If you blink/teleport/similar, monsters shouldn't automatically lock
    // on to your new location.
    //
    // 6. You shouldn't be able to sneak up on monsters when you have a crowd
    // of friendly ogres stomping along beside you!
    //	    (tricky to implement well though - monsters currently don't have a
    //	    stealth attribute. And I'm not sure that this is a desireable nerf
    //	    anyway.)
    //
    // 7. transformations (except vampire bat) seem not to affect stealth -
    // they really should.
    //
    // Related suggestions:
    //  Stabbing:
    //	    High SK_STABBING could give extra stuff, similar to 1 above. For
    //	    example, it might be nice to allow stab attempts with thrown
    //	    daggers/swords (much harder, of course). Ah, this latter has just
    //	    been discussed on crawl-ref-discuss, and looks like it may make
    //	    it.
    //
    //	    Stab attempts with maces&flails and, lesserly, staves could lead
    //	    to stunning (cudgel to the back of the neck, you know). Arguably a
    //	    rather different skill to stabbing, but I think close enough.
    //
    //	Dodging:
    //	    High SK_DODGING skill could give bonuses to dodging ranged attacks
    //	    similar to the ones for stealth above - namely hiding round
    //	    corners and behind big adjacent monsters.
    //
    //	    Other ideas:
    //		A move away from an adjacent monster could sometimes be
    //		interpreted as an attempt to jump away from an attack - if
    //		successful, it misses you and you end up a square away from
    //		it. Might be tricky to implement, and plausibly a bit too nice
    //		(given the stair-following mechanic)
    //
    //		'a'bility to dodge past a monster, exchanging squares (high
    //		failure rate, and comes at cost of a significant decrease in
    //		dodging chance for that turn whether it succeeds or not).
    //		Significant bonus if flying, and size-based bonus. Note:
    //		shouldn't be able to use this ability to trick a monster into
    //		deep water, a trap, a nasty cloud or similar!
    //		Problem with this: it makes corridor-based fighting even more
    //		attractive...

#ifdef DEBUG_STEALTH
    mprf(MSGCH_DIAGNOSTICS, "stealth: %d; plus: %d; mons_perc %d; one_in %d; dist_to_wall: %d; obscured/adj: %d/%d",
	    stealth, effective_stealth-stealth, mons_perc, effective_stealth/mons_perc, dist_to_wall,
	    obscured, obscured_adjacent);
#endif

    return (random2(effective_stealth) <= mons_perc);
}                               // end check_awaken()

static void set_show_backup( int ex, int ey )
{
    // Must avoid double setting it.  
    // We want the base terrain/item, not the cloud or monster that replaced it.
    if (!Show_Backup[ex][ey])
        Show_Backup[ex][ey] = env.show[ex][ey];
}

static int get_item_dngn_code(const item_def &item)
{
    switch (item.base_type)
    {
    case OBJ_ORBS:
        return (DNGN_ITEM_ORB);
    case OBJ_WEAPONS:
        return (DNGN_ITEM_WEAPON);
    case OBJ_MISSILES:
        return (DNGN_ITEM_MISSILE);
    case OBJ_ARMOUR:
        return (DNGN_ITEM_ARMOUR);
    case OBJ_WANDS:
        return (DNGN_ITEM_WAND);
    case OBJ_FOOD:
        return (DNGN_ITEM_FOOD);
    case OBJ_SCROLLS:
        return (DNGN_ITEM_SCROLL);
    case OBJ_JEWELLERY:
        return (jewellery_is_amulet(item)? DNGN_ITEM_AMULET : DNGN_ITEM_RING);
    case OBJ_POTIONS:
        return (DNGN_ITEM_POTION);
    case OBJ_BOOKS:
        return (DNGN_ITEM_BOOK);
    case OBJ_STAVES:
        return (DNGN_ITEM_STAVE);
    case OBJ_MISCELLANY:
        return (DNGN_ITEM_MISCELLANY);
    case OBJ_CORPSES:
        return (DNGN_ITEM_CORPSE);
    case OBJ_GOLD:
        return (DNGN_ITEM_GOLD);
    default:
        return (DNGN_ITEM_ORB); // bad item character
   }
}

inline static void update_item_grid(const coord_def &gp, const coord_def &ep)
{
    const item_def &eitem = mitm[igrd(gp)];
    unsigned short &ecol = env.show_col(ep);

    const dungeon_feature_type grid = grd(gp);
    if (Options.stair_item_brand && is_stair(grid))
        ecol |= COLFLAG_STAIR_ITEM;
    else if (Options.trap_item_brand && grid_is_trap(grid))
        ecol |= COLFLAG_TRAP_ITEM;
    else
    {
        ecol = (grid == DNGN_SHALLOW_WATER)? CYAN : eitem.colour;
        if (eitem.link != NON_ITEM)
            ecol |= COLFLAG_ITEM_HEAP;
        env.show(ep) = get_item_dngn_code( eitem );
    }
}

void item_grid()
{
    coord_def gp;
    for (gp.y = (you.y_pos - 8); (gp.y < you.y_pos + 9); gp.y++)
    {
        for (gp.x = (you.x_pos - 8); (gp.x < you.x_pos + 9); gp.x++)
        {
            if (in_bounds(gp))
            {
                if (igrd(gp) != NON_ITEM)
                {
                    const coord_def ep = gp - you.pos() + coord_def(9, 9);
                    if (env.show(ep))
                        update_item_grid(gp, ep);
                }
            }
        }
    }
}

void get_item_glyph( const item_def *item, unsigned *glych,
                     unsigned short *glycol )
{
    *glycol = item->colour;
    get_symbol( 0, 0, get_item_dngn_code( *item ), glych, glycol );
}

void get_mons_glyph( const monsters *mons, unsigned *glych,
                     unsigned short *glycol )
{
    *glycol = get_mons_colour( mons );
    get_symbol( 0, 0, mons->type + DNGN_START_OF_MONSTERS, glych, glycol );
}

inline static void update_cloud_grid(int cloudno)
{
    int which_colour = LIGHTGREY;
    const int ex = env.cloud[cloudno].x - you.x_pos + 9;
    const int ey = env.cloud[cloudno].y - you.y_pos + 9;
                
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

    default:
        which_colour = LIGHTGREY;
        break;
    }

    set_show_backup(ex, ey);
    env.show[ex][ey] = DNGN_CLOUD;
    env.show_col[ex][ey] = which_colour;    
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
            if (see_grid(env.cloud[s].x, env.cloud[s].y))
                update_cloud_grid(s);
        }
    }
}

// Noisy now has a messenging service for giving messages to the 
// player is appropriate.
//
// Returns true if the PC heard the noise.
bool noisy( int loudness, int nois_x, int nois_y, const char *msg )
{
    int p;
    struct monsters *monster = 0;       // NULL {dlb}
    bool ret = false;

    // If the origin is silenced there is no noise.
    if (silenced( nois_x, nois_y ))
        return (false);

    const int dist = loudness * loudness;

    // message the player
    if (distance( you.x_pos, you.y_pos, nois_x, nois_y ) <= dist
        && player_can_hear( nois_x, nois_y ))
    {
        if (msg)
            mpr( msg, MSGCH_SOUND );

        ret = true;
    }
    
    for (p = 0; p < MAX_MONSTERS; p++)
    {
        monster = &menv[p];

        if (monster->type < 0)
            continue;

        if (distance(monster->x, monster->y, nois_x, nois_y) <= dist
            && !silenced(monster->x, monster->y))
        {
            // If the noise came from the character, any nearby monster
            // will be jumping on top of them.
            if (nois_x == you.x_pos && nois_y == you.y_pos)
                behaviour_event( monster, ME_ALERT, MHITYOU );
            else
                behaviour_event( monster, ME_DISTURB, MHITNOT, nois_x, nois_y );
        }
    }

    return (ret);
}                               // end noisy()

/* The LOS code now uses raycasting -- haranp */

#define LONGSIZE (sizeof(unsigned long)*8)
#define LOS_MAX_RANGE 9

// the following two constants represent the 'middle' of the sh array.
// since the current shown area is 19x19, centering the view at (9,9)
// means it will be exactly centered.
// This is done to accomodate possible future changes in viewable screen
// area - simply change sh_xo and sh_yo to the new view center.

const int sh_xo = 9;            // X and Y origins for the sh array
const int sh_yo = 9;

// Data used for the LOS algorithm
int los_radius = 8;

unsigned long* los_blockrays = NULL;
unsigned long* dead_rays = NULL;
unsigned long* smoke_rays = NULL;
std::vector<hexdir> ray_coord;
std::vector<hexdir> compressed_ray;
std::vector<int> raylengths;
std::vector<ray_def> fullrays;

void setLOSRadius(int newLR)
{
    los_radius = newLR;
}

bool get_bit_in_long_array( const unsigned long* data, int where )
{
    int wordloc = where / LONGSIZE;
    int bitloc = where % LONGSIZE;
    return ((data[wordloc] & (1UL << bitloc)) != 0);
}

static void set_bit_in_long_array( unsigned long* data, int where )
{
    int wordloc = where / LONGSIZE;
    int bitloc = where % LONGSIZE;
    data[wordloc] |= (1UL << bitloc);
}

// hexrays:
// 
// Conceptually: we consider a ray to consist of a position and a direction.
// Both are discrete, but at a finer level of gradation than the overall hex
// grid. So if we consider our hexdirs as elements of the (rank two free
// abelian) group L, we consider our ray dirs to be elements of the group of n
// division points (1/n)L of L, for some natural number n; and we consider the
// positions as elements of principal homogeneous space for (1/n)L, which we
// consider to be embedded in the principal homogeneous space for L which is
// where our hexcoords live. Advancing and regressing the ray then just
// consists of adding multiples of dir to pos, and bounces and deflections are
// just a matter of reflecting and rotating dir.
//
// But rather than work directly with the position as above, we store and
// manipulate rather the hexcoord it is closest to along with a displacement,
// which is a hexdir disp with disp.rdist <= n. So disp is always in the
// "hexagon around zero". A diagram for n = 6:
//
//            \./. . . . .\./. . . . .\./              |
//            .|. . . . . .|. . . . . .|.              |
//             | . . * . . | . . c . . |               |
//            .|. . . . . .|. . . . . .|.              |
//            /.\. . . . ./.\. . . a ./.\              |
//            . . \ . . / . . \ . . / . .              |
//             . . .\./. . . .  \ /. . .               |
//            . . . .|. . . . . .|. . . .              |
//             * . . | . . d . . | . . *               |
//            . . . .|. . . . . .|. . . .              |
//
// c is a hexcoord, d is another and d=c+w.
// a is potential current position for a ray, and it would be stored as
//	cell = c; disp = -2v
// The lines indicate the boundaries between the cells, i.e. where the closest
//  hexcoord to the point changes. So to advance a, we would add dir to it
//  until we cross one of these lines, then change the cell and disp
//  appropriately.
// This is accomplished below, in code which looks horribly inefficient but
//  which I believe the compiler should be up to optimising to something
//  fully efficient. By rights, since all the arithmetic is now integer rather
//  than floating point, it ought to be more efficient than the square code.
//  I haven't profiled it, but raycasting takes negligible time with -O2.

void ray_def::advance()
{
    if (dir == hexdir::zero)
	return;
    while (true)
    {
	disp += dir;
	if (recentre())
	    break;
    }
}

void ray_def::regress()
{
    if (dir == hexdir::zero)
	return;
    while (true)
    {
	disp -= dir;
	if (recentre())
	    break;
    }
}


bool ray_def::recentre()
{
    static const hexdir dirs[6] = { hexdir::u,hexdir::v,hexdir::w,-hexdir::u,-hexdir::v,-hexdir::w };
    bool changed_once = false;
    bool changed = false;
    do
    {
	if (changed)
	    changed_once = true;
	changed = false;
	for (int i = 0; i < 6; i++)
	    if ((dirs[i].X * disp.X + dirs[i].Y * disp.Y + dirs[i].Z * disp.Z)
		    > n)
	    {
		disp -= dirs[i]*n;
		cell += dirs[i];
		changed = true;
		break;
	    }
    } while (changed);
    return changed_once;
}

void ray_def::advance_and_bounce()
{
    // [hex] bounce, with smoothing
    const hexcoord oldpos = pos();

    advance();
    const hexcoord newpos = pos();
    ASSERT( grid_is_solid(grd(newpos)) );
    regress();

    int hextant = (newpos-oldpos).hextant();

    // rotation is cheap, so the logic code deals explicitly with the case
    // that newpos==oldpos-hexdir::w (-hexdir::w.hextant() being 0), and rotates
    // appropriately to handle all cases.

    dir.rotate(-hextant);

    // smooth off walls:
    // cases: s is the source, * is the solid hex we hit, # are other solid
    // hexes, . is non-solid:
    //
    //   # * #  reflect in u
    //    s . 
    //
    //    #
    //   . *    reflect in v
    //    s # 
    //
    //    anything else: reflect in X (i.e. back at s)
    //
    if ( grid_is_solid(grd(newpos - hexdir::u.rotated(hextant)))
	    && grid_is_solid(grd(newpos + hexdir::u.rotated(hextant)))
	    && !grid_is_solid(grd(newpos - hexdir::v.rotated(hextant))) )
	dir.reflect_u();
    else if ( grid_is_solid(grd(newpos - hexdir::v.rotated(hextant)))
	    && grid_is_solid(grd(newpos + hexdir::v.rotated(hextant)))
	    && !grid_is_solid(grd(newpos - hexdir::u.rotated(hextant))) )
	dir.reflect_v();
    else
	dir.reflect_X();

    dir.rotate(hextant);

    // To avoid having a bounced line which is badly aligned, we do this. Bit
    // of a hack, but I can't think of anything better.
    disp = hexdir::zero;
}

// ensure that m divides n
void ray_def::rescale(int m)
{
    const int lcm = n*m/gcd(n,m);
    const int scale = lcm/n;
    disp *= scale;
    dir *= scale;
    n = lcm;
}

// Deflect the direction. A deflect_amount of 60 is, approximately, a rotation
// of PI/3 anticlockwise - but precisely how deflect_amount translates into
// an angle actually depends on dir. Not perfect, but close enough for our
// purposes I hope.
//
// Explanation of code: if dir is towards -w (the hextant of which is 0) we
// add a proportional amount in the -u direction for a positive rotation, and
// in the -v direction for a negative rotation. So deflect(60) sends n*(-w) to
// n*v, and deflect(-60) sends n*(-w) to n*u.
//
// XXX: problem - deflection can turn a pleasant ray, e.g. one returned by
// find_ray(find_shortest=true), into an unpleasant one. Not sure what to do
// about that, really.
void ray_def::deflect(int deflect_amount)
{
    rescale(60);
    dir += ((deflect_amount > 0 ? -hexdir::u : hexdir::v).rotated(
		hex_dir_towards(dir).hextant())) *
	( deflect_amount * dir.rdist() / 60 );
}


// Shoot a ray from the given start point (accx, accy) with the given
// slope, with a maximum distance (in either x or y coordinate) of
// maxrange. Store the visited cells in xpos[] and ypos[], and
// return the number of cells visited.
static int shoot_ray( ray_def ray, int maxrange,
	hexcoord pos[] )
{
    int cellnum;
    for ( cellnum = 0; true; ++cellnum )
    {
	ray.advance();
	if ((ray.pos() - hexcoord::centre).rdist() > maxrange)
	    break;
	pos[cellnum] = ray.pos();
    }
    return cellnum;
}

// check if the passed ray has already been created
static bool is_duplicate_ray( int len, hexcoord pos[] )
{
    int cur_offset = 0;
    for ( unsigned int i = 0; i < raylengths.size(); ++i )
    {
        // only compare equal-length rays
        if ( raylengths[i] != len )
        {
            cur_offset += raylengths[i];
            continue;
        }

        int j;
        for ( j = 0; j < len; ++j )
        {
            if ( ray_coord[j + cur_offset] != (pos[j] - hexcoord::centre) )
                break;
        }

        // exact duplicate?
        if ( j == len )
            return true;

        // move to beginning of next ray
        cur_offset += raylengths[i];
    }
    return false;
}

// is starta...lengtha a subset of startb...lengthb?
static bool is_subset( int starta, int startb, int lengtha, int lengthb )
{
    int cura = starta, curb = startb;
    int enda = starta + lengtha, endb = startb + lengthb;
    while ( cura < enda && curb < endb )
    {
        if ( ray_coord[curb].rdist() > ray_coord[cura].rdist() )
            return false;
        if ( ray_coord[cura] == ray_coord[curb] )
            ++cura;

        ++curb;
    }
    return ( cura == enda );
}

// return a vector which lists all the nonduped cellrays (by index)
static std::vector<int> find_nonduped_cellrays()
{
    // a cellray c in a fullray f is duped if there is a fullray g
    // such that g contains c and g[:c] is a subset of f[:c]
    int raynum, cellnum, curidx, testidx, testray, testcell;
    bool is_duplicate;

    std::vector<int> result;
    for (curidx=0, raynum=0;
         raynum < static_cast<int>(raylengths.size());
         curidx += raylengths[raynum++])
    {
        for (cellnum = 0; cellnum < raylengths[raynum]; ++cellnum)
        {
            // is the cellray raynum[cellnum] duplicated?
            is_duplicate = false;
            // XXX We should really check everything up to now
            // completely, and all further rays to see if they're
            // proper subsets.
            const hexdir cur = ray_coord[curidx + cellnum];
            for (testidx = 0, testray = 0; testray < raynum;
                 testidx += raylengths[testray++])
            {
                // scan ahead to see if there's an intersect
                for ( testcell = 0; testcell < raylengths[raynum]; ++testcell )
                {
                    const hexdir test = ray_coord[testidx + testcell];
                    if ( test == cur )
                    {
                        is_duplicate = is_subset(testidx, curidx,
                                                 testcell, cellnum);
                        break;
                    }
                }
                if ( is_duplicate )
                    break;      // no point in checking further rays
            }
            if ( !is_duplicate )
                result.push_back(curidx + cellnum);
        }
    }
    return result;
}

// Create and register the ray defined by the arguments.
// Return true if the ray was actually registered (i.e., not a duplicate.)
static bool register_ray( hexdir disp, hexdir dir, int n )
{
    hexcoord pos[2*(LOS_MAX_RANGE+1) + 1];

    ray_def ray;
    ray.disp = disp;
    ray.dir = dir;
    ray.cell = hexcoord::centre;
    ray.n = n;

    int raylen = shoot_ray( ray, LOS_MAX_RANGE, pos );

    // early out if ray already exists
    if ( is_duplicate_ray(raylen, pos) )
        return false;

    // not duplicate, register
    for ( int i = 0; i < raylen; ++i )
    {
        // create the cellrays
        ray_coord.push_back(pos[i] - hexcoord::centre);
    }

    // register the fullray
    raylengths.push_back(raylen);
    fullrays.push_back(ray);

    return true;
}

static void create_blockrays()
{
    // determine nonduplicated rays
    std::vector<int> nondupe_cellrays = find_nonduped_cellrays();
    const unsigned int num_nondupe_rays = nondupe_cellrays.size();
    const unsigned int num_nondupe_words =
        (num_nondupe_rays + LONGSIZE - 1) / LONGSIZE;
    const unsigned int num_cellrays = ray_coord.size();
    const unsigned int num_words = (num_cellrays + LONGSIZE - 1) / LONGSIZE;

    // first build all the rays: easier to do blocking calculations there
    unsigned long* full_los_blockrays;
    full_los_blockrays = new unsigned long[num_words * (LOS_MAX_RANGE+2) *
                                           (LOS_MAX_RANGE+2)];
    memset((void*)full_los_blockrays, 0, sizeof(unsigned long) * num_words *
           (LOS_MAX_RANGE+2) * (LOS_MAX_RANGE+2));

    int cur_offset = 0;

    for ( unsigned int ray = 0; ray < raylengths.size(); ++ray )
    {
        for ( int i = 0; i < raylengths[ray]; ++i )
        {
            // every cell blocks...
            unsigned long* const inptr = full_los_blockrays +
                ((ray_coord[i + cur_offset].X+1) * (LOS_MAX_RANGE + 2) +
                 (ray_coord[i + cur_offset].Y+1)) * num_words;

            // ...all following cellrays
            for ( int j = i+1; j < raylengths[ray]; ++j )
                set_bit_in_long_array( inptr, j + cur_offset );

        }
        cur_offset += raylengths[ray];
    }

    // we've built the basic blockray array; now compress it, keeping
    // only the nonduplicated cellrays.

    // allocate and clear memory
    los_blockrays = new unsigned long[num_nondupe_words * (LOS_MAX_RANGE+2) * (LOS_MAX_RANGE + 2)];
    memset((void*)los_blockrays, 0, sizeof(unsigned long) * num_nondupe_words *
           (LOS_MAX_RANGE+2) * (LOS_MAX_RANGE+2));

    // we want to only keep the cellrays from nondupe_cellrays.
    compressed_ray.resize(num_nondupe_rays);
    for ( unsigned int i = 0; i < num_nondupe_rays; ++i )
    {
        compressed_ray[i] = ray_coord[nondupe_cellrays[i]];
    }
    unsigned long* oldptr = full_los_blockrays;
    unsigned long* newptr = los_blockrays;
    for ( int X = -1; X <= LOS_MAX_RANGE; ++X )
    {
        for ( int Y = -1; Y <= LOS_MAX_RANGE; ++Y )
        {
            for ( unsigned int i = 0; i < num_nondupe_rays; ++i )
                if ( get_bit_in_long_array(oldptr, nondupe_cellrays[i]) )
                    set_bit_in_long_array(newptr, i);
            oldptr += num_words;
            newptr += num_nondupe_words;
        }
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

static int gcd( int x, int y )
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

bool complexity_lt( const std::pair<hexdir,int>& lhs,
                    const std::pair<hexdir,int>& rhs )
{
    return lhs.second < rhs.second;
}

// Cast all rays
void raycast()
{
    static bool done_raycast = false;
    if ( done_raycast )
        return;
    
    // Creating all rays for first hextant
    // We have a considerable amount of overkill.   
    done_raycast = true;

    std::vector< std::pair<hexdir,int> > dirs;
    for (int dX = 0; dX <= LOS_MAX_RANGE*2; dX++)
	for (int dY = 1; dY <= LOS_MAX_RANGE*2; dY++)
	{
	    if (dX+dY > LOS_MAX_RANGE*2)
		continue;

	    if (gcd(dX,dY) != 1)
		continue;

	    hexdir d(dX,dY,-dX-dY);

	    // Here we bring dir down close to zero, such that we need to go at least 6
	    // times along it to get out of the hex around 0.
	    // 6 is an arbitrary number, but don't change it! We may make use of
	    // the fact that 2 and 3 divide n.
	    int n = 6*MAX(abs(d.X-d.Z), MAX(abs(d.Y-d.X), abs(d.Z-d.Y)));

	    dirs.push_back(std::pair<hexdir,int>(d,n));
	}

    std::sort( dirs.begin(), dirs.end(), complexity_lt );

    for ( unsigned int i = 0; i < dirs.size(); ++i )
    {
        const hexdir dir = dirs[i].first;
        const int n = dirs[i].second;

	// the v line is perpendicular to the primary hextant, so we start at
	// various points along the intersection of the v line with the hex
	// around 0, i.e. (-1/2,1/2)*v.

	for (int j = -5; j < 5; j++)
	{
	    const hexdir disp = (hexdir::v*((j*n)/12));
	    register_ray( disp, dir, n );
	}
    }

    // Now create the appropriate blockrays array
    create_blockrays();   
}

static int cyclic_offset( unsigned int ui, int cycle_dir, int startpoint,
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

bool find_ray( hexcoord source, hexcoord target,
               bool allow_fallback, ray_def& ray, int cycle_dir,
               bool find_shortest, bool no_block )
{

    hexdir diff = target - source;

    // [hex] if find_shortest, literally just look for a ray of minimum length
    // to the target. They should all be of equal "balance", for any
    // reasonable value of "balance".
    int shortest = INFINITE_DISTANCE;

    int cur_offset = 0;
    int cellray, inray;

    // [hex] due to the slightly messy way hexes work, we have to also
    // consider the hextants to either side of the current hextant. Of course
    // this is only an issue if our point is close to the boundary of a
    // hextant, so we could test for that if optimisation is needed.
    for (int hextant = diff.hextant() - 1; hextant <= diff.hextant() + 1; hextant++)
    {
	hexdir d = diff.rotated(-hextant);
	for ( unsigned int fray = 0; fray < fullrays.size(); ++fray )
	{
	    const int fullray = cyclic_offset( fray, cycle_dir, ray.fullray_idx,
		    fullrays.size() );
	    // yeah, yeah, this is O(n^2). I know.
	    cur_offset = 0;
	    for ( int i = 0; i < fullray; ++i )
		cur_offset += raylengths[i];

	    for ( cellray = 0; cellray < raylengths[fullray]; ++cellray )
	    {
		if ( ray_coord[cellray + cur_offset] == d )
		{

		    // check if we're blocked so far
		    bool blocked = false;
		    for ( inray = 0; inray <= cellray; ++inray )
		    {
			if (!no_block && inray < cellray
				&& grid_is_solid(grd(source +
					ray_coord[inray + cur_offset].rotated(hextant))))
			{
			    blocked = true;
			    break;
			}
		    }

		    if ( !blocked && cellray < shortest )
		    {
			// success!
			shortest = cellray;

			ray        = fullrays[fullray];
			ray.fullray_idx = fullray;

			ray.dir = ray.dir.rotated(hextant);
			ray.disp = ray.disp.rotated(hextant);
			ray.cell = source;

			if (!find_shortest)
			    return true;
		    }
		}
	    }
	}
    }

    if (find_shortest && shortest != INFINITE_DISTANCE)
	return true;

    if (allow_fallback)
    {
	ray.cell = source;
	ray.disp = hexdir::zero;

	hexdir d = diff;

	if (d == hexdir::zero)
	{
	    ray.dir = d;
	    return true;
	}

	// normalise
	int g = abs(gcd(d.X, gcd(d.Y, d.Z)));
	d /= g;

	// see raycast() for explication
	ray.n = 6*MAX(abs(d.X-d.Z), MAX(abs(d.Y-d.X), abs(d.Z-d.Y)));

	ray.dir = d;
	return true;
    }
    return false;
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
void losight(FixedArray < unsigned int, 19, 19 > &sh,
             FixedArray < dungeon_feature_type, GXM, GYM > &gr, hexcoord pos)
{
    raycast();
    // go hextant by hextant

    // clear out sh
    sh.init(0);

    const unsigned int num_cellrays = compressed_ray.size();
    const unsigned int num_words = (num_cellrays + LONGSIZE - 1) / LONGSIZE;

    for ( int hextant = 0; hextant < 6; hextant++ )
    {
        // clear out the dead rays array
        memset( (void*)dead_rays,  0, sizeof(unsigned long) * num_words);
        memset( (void*)smoke_rays, 0, sizeof(unsigned long) * num_words);

        // kill all blocked rays
	// XXX: Note we start at -1! A ray with direction in a particular
	// hextant may pass through cells just outside of the hextant, so we
	// loop through this extended diamond.
        const unsigned long* inptr = los_blockrays;
        for ( int dX = -1; dX <= LOS_MAX_RANGE; ++dX )
        {
            for (int dY = -1; dY <= LOS_MAX_RANGE;
		    ++dY, inptr += num_words)
            {
		if (dX+dY > LOS_MAX_RANGE)
		    continue;

		const hexdir d(dX,dY,-dX-dY);
		const hexcoord target = pos + d.rotated(hextant);

		if (!in_G_bounds(target))
                    continue;

                // if this cell is opaque...
                if ( grid_is_opaque(gr(target)) )
                {
                    // then block the appropriate rays
                    for ( unsigned int i = 0; i < num_words; ++i )
                        dead_rays[i] |= inptr[i];
                }
                else if ( is_opaque_cloud(env.cgrid(target)) )
                {
                    // block rays which have already seen a cloud
                    for ( unsigned int i = 0; i < num_words; ++i )
                    {
                        dead_rays[i] |= (smoke_rays[i] & inptr[i]);
                        smoke_rays[i] |= inptr[i];
                    }
                }
            }
        }

        // ray calculation done, now work out which cells in this
        // hextant are visible
        unsigned int rayidx = 0;
        for ( unsigned int wordloc = 0; wordloc < num_words; ++wordloc )
        {
            const unsigned long curword = dead_rays[wordloc];
            // Note: the last word may be incomplete
            for ( unsigned int bitloc = 0; bitloc < LONGSIZE; ++bitloc)
            {
                // make the cells seen by this ray at this point visible
                if ( ((curword >> bitloc) & 1UL) == 0 )
                {
                    // this ray is alive!
		    const hexdir d = compressed_ray[rayidx].rotated(hextant);
		    const hexcoord target = pos + d;
                    // update shadow map
		    if (d.rdist() <= los_radius && in_G_bounds(target))
                        sh(grid2show(target))=gr(target);
                }
                ++rayidx;
                if ( rayidx == num_cellrays )
                    break;
            }
        }
    }

    // [dshaligram] The player's current position is always visible.
    sh(grid2show(you.pos())) = gr(you.pos());
}


void draw_border(void)
{
    textcolor( BORDER_COLOR );
    clrscr();
    redraw_skill( you.your_name, player_title() );

    const int xcol = crawl_view.hudp.x;
    
    gotoxy(xcol, 2);
    cprintf( "%s %s",
             species_name( you.species, you.experience_level ).c_str(),
             (you.wizard ? "*WIZARD*" : "" ) );

    gotoxy(xcol,  3); cprintf("HP:");
    gotoxy(xcol,  4); cprintf("Magic:");
    gotoxy(xcol,  5); cprintf("AC:");
    gotoxy(xcol,  6); cprintf("EV:");
    gotoxy(xcol,  7); cprintf("Str:");
    gotoxy(xcol,  8); cprintf("Int:");
    gotoxy(xcol,  9); cprintf("Dex:");
    gotoxy(xcol, 10); cprintf("Gold:");
    if (Options.show_turns)
    {
        gotoxy(xcol + 15, 10);
        cprintf("Turn:");
    }
    gotoxy(xcol, 11); cprintf("Experience:");
    gotoxy(xcol, 12); cprintf("Level");
}                               // end draw_border()

// Determines if the given feature is present at (x, y) in _grid_ coordinates.
// If you have map coords, add (1, 1) to get grid coords.
// Use one of
// 1. '<' and '>' to look for stairs
// 2. '\t' or '\\' for shops, portals.
// 3. '^' for traps
// 4. '_' for altars
// 5. Anything else will look for the exact same character in the level map.
bool is_feature(int feature, int x, int y)
{
    if (!env.map[x][y].object)
        return false;

    // 'grid' can fit in an unsigned char, but making this a short shuts up
    // warnings about out-of-range case values.
    short grid = grd[x][y];

    switch (feature)
    {
    case 'X':
        return (travel_point_distance[x][y] == PD_EXCLUDED);
    case 'F':
    case 'W':
        return is_waypoint(x, y);
    case 'I':
        return is_stash(x, y);
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
            return true;
        default:
            return false;
        }
    case '\t':
    case '\\':
        switch (grid)
        {
        case DNGN_ENTER_HELL:
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
        case DNGN_STONE_ARCH:
        case DNGN_ENTER_PANDEMONIUM:
        case DNGN_EXIT_PANDEMONIUM:
        case DNGN_TRANSIT_PANDEMONIUM:
        case DNGN_ENTER_ZOT:
        case DNGN_RETURN_FROM_ZOT:
            return true;
        default:
            return false;
        }
    case '<':
        switch (grid)
        {
        case DNGN_ROCK_STAIRS_UP:
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
            return true;
        default:
            return false;
        }
    case '>':
        switch (grid)
        {
        case DNGN_ROCK_STAIRS_DOWN:
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
            return true;
        default:
            return false;
        }
    case '^':
        switch (grid)
        {
        case DNGN_TRAP_MECHANICAL:
        case DNGN_TRAP_MAGICAL:
        case DNGN_TRAP_III:
            return true;
        default:
            return false;
        }
    default: 
        return get_envmap_char(x, y) == (unsigned) feature;
    }
}

static int find_feature(int feature, const hexdir &curs, const hexcoord &start,
			 int anchor_x, int anchor_y, int ignore_count,
			 hexdir *move)
{
    const hexcoord c(anchor_x, anchor_y);
    hexcoord t;

    int matchcount = 0, firstx = -1, firsty = -1;

    // [hex] go out in a spiral
    // Same basic spiralling code as in direct.cc:find_hex().
    hexdir d = hexdir::zero;
    while (true)
    {
	if ( d == hexdir::u*d.rdist() )
	    if ( d.rdist() >= GRAD*2 )
		break;
	    else
		d += -hexdir::w;
	else
	{
	    d.reflect_u();
	    d += -hexdir::v.rotated( d.hextant() );
	    d.reflect_u();
	}
	t = c + d;
	if (!in_bounds(t))
	    continue;
	if (is_feature(feature, t.x, t.y))
	{
	    ++matchcount;
	    if (!ignore_count--)
	    {
		// We want to cursor to (x,y)
		*move = t - (start + curs);
		return matchcount;
	    }
	    else if (firstx == -1)
	    {
		firstx = t.x;
		firsty = t.y;
	    }
	}
    }

    // We found something, but ignored it because of an ignorecount
    if (firstx != -1)
    {
	*move = hexcoord(firstx,firsty) - (start + curs);
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
        if (is_feature(feature, coord.x, coord.y))
            found->push_back(coord);
    }
}

static int find_feature( const std::vector<coord_def>& features,
                         int feature, const hexdir &curs,
			 const hexcoord &start,
                         int ignore_count, 
			 hexdir *move,
                         bool forward)
{
    int firstx = -1, firsty = -1, firstmatch = -1;
    int matchcount = 0;

    for (unsigned feat = 0; feat < features.size(); ++feat)
    {
        const coord_def& coord = features[feat];

        if (is_feature(feature, coord.x, coord.y))
        {
            ++matchcount;
            if (forward? !ignore_count-- : --ignore_count == 1)
            {
                // We want to cursor to (x,y)
		*move = coord.tohex() - (start + curs);
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
	*move = hexcoord(firstx, firsty) - (start + curs);
        return firstmatch;
    }
    return 0;
}

static int get_number_of_lines_levelmap()
{
    return get_number_of_lines() - (Options.level_map_title ? 1 : 0);
}

static void draw_level_map(hexcoord start, bool travel_mode)
{
    int bufcount2 = 0;
    screen_buffer_t buffer2[GYM * GXM * 2];

    int num_lines = get_number_of_lines_levelmap();
    if ( num_lines > GYM )
        num_lines = GYM;

    int num_cols = get_number_of_cols();
    if ( num_cols > GXM )
        num_cols = GXM;

    cursor_control cs(false);

    int top = 1 + Options.level_map_title;
    if ( Options.level_map_title )
    {
        const formatted_string help =
            formatted_string::parse_string("(Press <w>?</w> for help)");
        const int helplen = std::string(help).length();

        gotoxy(1, 1);
        textcolor(WHITE);
        cprintf("%-*s",
                get_number_of_cols() - helplen,
                ("Level " + level_description_string()).c_str());

        textcolor(LIGHTGREY);
        gotoxy(get_number_of_cols() - helplen + 1, 1);
        help.display();
    }

    gotoxy(1, top);

    for (int screen_y = 0; screen_y < num_lines; screen_y++)
    {
        for (int screen_x = 0; screen_x < num_cols - 1; screen_x++)
        {
	    if (!screen2hex_valid(coord_def(screen_x,screen_y)))
	    {
                buffer2[bufcount2 + 1] = DARKGREY;
                buffer2[bufcount2] = 0;
            }
	    else
	    {
		screen_buffer_t colour = DARKGREY;

		hexcoord c = start + screen2hex(coord_def(screen_x,screen_y));

		if (!in_bounds(c))
		{
		    buffer2[bufcount2 + 1] = DARKGREY;
		    buffer2[bufcount2] = 0;
		}
		else
		{
		    colour = colour_code_map(c.x, c.y,
			    Options.item_colour, 
			    travel_mode);

		    buffer2[bufcount2 + 1] = colour;
		    buffer2[bufcount2] = env.map(c).glyph();

		    if (c == you.pos())
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
			unsigned char ch = is_waypoint(c.x, c.y);
			if (ch && (bc == get_sightmap_char(DNGN_FLOOR) ||
				    bc == get_magicmap_char(DNGN_FLOOR)))
			    bc = ch;
		    }
		}
	    }
            
            bufcount2 += 2;
        }
    }
    puttext(1, top, num_cols - 1, top + num_lines - 1, buffer2);
}

static void reset_travel_colours(std::vector<coord_def> &features)
{
    // We now need to redo travel colours
    features.clear();
    find_travel_pos(you.pos(), NULL, &features);
    // Sort features into the order the player is likely to prefer.
    arrange_features(features);
}

// show_map() now centers the known map along x or y.  This prevents
// the player from getting "artificial" location clues by using the
// map to see how close to the end they are.  They'll need to explore
// to get that.  This function is still a mess, though. -- bwr
void show_map( FixedVector<int, 2> &spec_place, bool travel_mode )
{
    cursor_control ccon(!Options.use_fake_cursor);

    coord_def scroll(0,0);
    coord_def cursor_at(0,0);
    hexdir move = hexdir::zero;
    int getty = 0;

    const int border = 5;

    // Vector to track all features we can travel to, in order of distance.
    std::vector<coord_def> features;
    if (travel_mode)
    {
        travel_cache.update();

        find_travel_pos(you.pos(), NULL, &features);
        // Sort features into the order the player is likely to prefer.
        arrange_features(features);
    }

    const int num_lines = get_number_of_lines_levelmap();
    const int num_cols = get_number_of_cols();

    const int min_cursx = border;
    const int min_cursy = border;
    const int max_cursx = num_cols - border;
    const int max_cursy = num_lines - border;

    const int half_screen = (num_lines - 1) / 2;
    const int half_screen_width_hexes = ((num_cols - 1) / 2) / 2;

    const int top = 1 + Options.level_map_title;

    const int block_step = Options.level_map_cursor_step;

    // grid position to show in top left corner of screen
    hexcoord start = static_cast<coord_def>(you.pos()) -
	coord_def(half_screen_width_hexes, half_screen);

    // grid position of cursor measured from start
    hexdir curs = you.pos() - start;

    int search_feat = 0, search_found = 0, anchor_x = -1, anchor_y = -1;

    bool map_alive  = true;
    bool redraw_map = true;

    clrscr();
    textcolor(DARKGREY);

    while (map_alive)
    {
	move = hexdir::zero;

	if (redraw_map)
	    draw_level_map(start, travel_mode);

	redraw_map = true;
	cursor_at = hex2screen(curs) + coord_def(1,top);
	cursorxy( cursor_at.x, cursor_at.y );

	c_input_reset(true);
	getty = unmangle_direction_keys(getchm(KC_LEVELMAP), KC_LEVELMAP,
		false, false);
	c_input_reset(false);

	switch (getty)
	{
	    case '?':
		show_levelmap_help();
		break;

	    case CONTROL('C'):
		clear_map();
		break;

	    case CONTROL('F'):
	    case CONTROL('W'):
		travel_cache.add_waypoint((start+curs).x, (start+curs).y);
		// We need to do this all over again so that the user can jump
		// to the waypoint he just created.
		features.clear();
		find_travel_pos(you.pos(), NULL, &features);
		// Sort features into the order the player is likely to prefer.
		arrange_features(features);
		break;

		// Cycle the radius of an exclude.
	    case 'x':
		{
		    const coord_def p = start + curs;
		    if (is_exclude_root(p))
			cycle_exclude_radius(p);
		    reset_travel_colours(features);
		    break;
		}

	    case CONTROL('E'):
	    case CONTROL('X'):
		{
		    if (getty == CONTROL('X'))
			toggle_exclude(start+curs);
		    else
			clear_excludes();

		    reset_travel_colours(features);
		    break;
		}

	    case 'b':
	    case '1':
		move = hexdir::w;
		break;

	    case 'u':
	    case '9':
		move = -hexdir::w;
		break;

	    case 'y':
	    case '7':
		move = hexdir::v;
		break;

	    case 'h':
	    case '4':
		move = -hexdir::u;
		break;

	    case 'n':
	    case '3':
		move = -hexdir::v;
		break;

	    case 'l':
	    case '6':
		move = hexdir::u;
		break;

	    case 'B':
		move = hexdir::w*block_step;
		break;

	    case 'U':
		move = -hexdir::w*block_step;
		break;

	    case 'Y':
		move = hexdir::v*block_step;
		break;

	    case 'H':
		move = -hexdir::u*block_step;
		break;

	    case 'N':
		move = -hexdir::v*block_step;
		break;

	    case 'L':
		move = hexdir::u*block_step;
		break;

	    case '+':
		scroll.set(0,20);
		break;
	    case '-':
		scroll.set(0,-20);
		break;
	    case '*':
		scroll.set(20,0);
		break;
	    case '/':
		scroll.set(-20,0);
		break;
	    case '<':
	    case '>':
	    case '@':
	    case '\t':
	    case '^':
	    case '_':
	    case 'X':
	    case 'F':
	    case 'W':
	    case 'I':
	    case ';':
	    case '\'':
		{
		    bool forward = true;

		    if (getty == '/' || getty == ';')
			forward = false;

		    if (getty == '/' || getty == '*' || getty == ';' || getty == '\'')
			getty = 'I';

		    if (anchor_x == -1)
		    {
			anchor_x = (start+curs).x;
			anchor_y = (start+curs).y;
		    }
		    if (search_feat != getty)
		    {
			search_feat         = getty;
			search_found        = 0;
		    }
		    if (travel_mode)
			search_found = find_feature(features, getty, curs,
				start,
				search_found, 
				&move,
				forward);
		    else
			search_found = find_feature(getty, curs,
				start,
				anchor_x, anchor_y,
				search_found, &move);
		    break;
		}

	    case CK_MOUSE_MOVE:
		break;

	    case CK_MOUSE_CLICK:
		{
		    // [hex] FIXME broken
		    const c_mouse_event cme = get_mouse_event();
		    const hexcoord curp(start+curs);
		    const coord_def grdp =
			cme.pos + coord_def(start.x - 1, start.y - top);

		    if (cme.left_clicked() && in_bounds(grdp))
		    {
			spec_place[0] = grdp.x;
			spec_place[1] = grdp.y;
			map_alive     = false;
		    }
		    else if (cme.scroll_up())
			scroll.set(0,-block_step);
		    else if (cme.scroll_down())
			scroll.set(0,block_step);
		    else if (cme.right_clicked())
		    {
			move = grdp.tohex() - curp;
		    }
		    break;
		}

	    case '.':
	    case '\r':
	    case 'S':
	    case ',':
		{
		    const hexcoord curp = start+curs;
		    if (in_G_bounds(curp))
		    {
			if (travel_mode && curp == you.pos())
			{
			    if (you.travel_x > 0 && you.travel_y > 0)
			    {
				move = hexcoord(you.travel_x,you.travel_y) - curp;
			    }
			    break;
			}
			else
			{
			    spec_place[0] = curp.x;
			    spec_place[1] = curp.y;
			    map_alive = false;
			    break;
			}
		    }
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

	if (scroll != coord_def(0,0))
	{
	    start = static_cast<coord_def>(start) + scroll;
	    scroll.set(0,0);
	}
	else
	{
	    hexdir shift = hexdir::zero;
	    const hexdir newcurs = curs + move;
	    const int newx = hex2screen(newcurs).x;
	    const int newy = hex2screen(newcurs).y;

	    if (newy < min_cursy)
		shift += (hexdir::v - hexdir::w)*((min_cursy-newy)/2);
	    else if (newy > max_cursy)
		shift -= (hexdir::v - hexdir::w)*((newy-max_cursy)/2);

	    if (newx < min_cursx)
		shift -= hexdir::u*((min_cursx-newx)/2);
	    else if (newx > max_cursx)
		shift += hexdir::u*((newx-max_cursx)/2);

	    start += shift;
	    curs = newcurs - shift;
	}
    }
    return;
}                               // end show_map()


// Returns true if succeeded
bool magic_mapping(int map_radius, int proportion, bool suppress_msg,
                   bool force)
{
    if (!force &&
        ((you.level_type == LEVEL_ABYSS) ||
         (you.level_type == LEVEL_LABYRINTH && you.species != SP_MINOTAUR)))
    {
        if (!suppress_msg)
            mpr("You feel momentarily disoriented.");
        return false;
    }

    if (!suppress_msg)
        mpr( "You feel aware of your surroundings." );

    int empty_count;

    if (map_radius > 50 && map_radius != 1000)
        map_radius = 50;
    else if (map_radius < 5)
        map_radius = 5;

    // now gradually weaker with distance:
    const int pfar = (map_radius * 7) / 10;
    const int very_far = (map_radius * 9) / 10;

    const bool wizard_map = map_radius == 1000 && you.wizard;
    hexdir::disc h(std::min(map_radius, 4*GRAD));
    for (hexdir::disc::iterator it = h.begin(); it != h.end(); it++)
    {
	hexcoord t = you.pos() + *it;

	if (proportion < 100 && random2(100) >= proportion)
	    continue;       // note that proportion can be over 100

	if (!map_bounds(t))
	    continue;

	const int dist = t.distance_from(you.pos());

	if (dist > pfar && one_chance_in(3))
	    continue;

	if (dist > very_far && coinflip())
	    continue;

	if (is_terrain_changed(t.x, t.y))
	    clear_envmap_grid(t.x, t.y);

	if (!wizard_map && is_terrain_known(t.x, t.y))
	    continue;

	empty_count = 6;

	if (grid_is_solid(grd(t)) && grd(t) != DNGN_CLOSED_DOOR)
	{
	    hexdir::circle c(1);
	    for (hexdir::circle::iterator it2 = c.begin(); it2 != c.end(); it2++)
	    {
		const hexcoord t2 = t + *it2;
		if (!map_bounds(t2))
		{
		    --empty_count;
		}
		else if (grid_is_opaque( grd(t2) )
			&& grd(t2) != DNGN_CLOSED_DOOR)
		{
		    empty_count--;
		}
	    }
	}

	if (empty_count > 0)
	{
	    if (wizard_map || !get_envmap_obj(t.x, t.y))
		set_envmap_obj(t.x, t.y, grd(t));

	    // Hack to give demonspawn Pandemonium mutation the ability
	    // to detect exits magically.
	    if ((you.mutation[MUT_PANDEMONIUM] > 1
			&& grd(t) == DNGN_EXIT_PANDEMONIUM)
		    || wizard_map)
	    {
		set_terrain_seen( t.x, t.y );
	    }
	    else
	    {
		set_terrain_mapped( t.x, t.y );
	    }
	}
    }
    return true;
}                               // end magic_mapping()

// realize that this is simply a repackaged version of
// stuff::see_grid() -- make certain they correlate {dlb}:
bool mons_near(const monsters *monster, unsigned int foe)
{
    // early out -- no foe!
    if (foe == MHITNOT)
        return (false);

    if (foe == MHITYOU)
    {
        if (monster->x > you.x_pos - 9 && monster->x < you.x_pos + 9
            && monster->y > you.y_pos - 9 && monster->y < you.y_pos + 9)
        {
            if (env.show[monster->x - you.x_pos + 9][monster->y - you.y_pos + 9])
                return (true);
        }
        return (false);
    }

    // must be a monster
    const monsters *myFoe = &menv[foe];
    if (myFoe->type >= 0)
    {
        if (monster->x > myFoe->x - 9 && monster->x < myFoe->x + 9
            && monster->y > myFoe->y - 9 && monster->y < myFoe->y + 9)
        {
            return (true);
        }
    }

    return (false);
}                               // end mons_near()

// answers the question: "Is a grid within character's line of sight?"
bool see_grid( int grx, int gry )
{
    // rare case: can player see self?  (of course!)
    if (grx == you.x_pos && gry == you.y_pos)
        return (true);

    // check env.show array
    if (grid_distance( grx, gry, you.x_pos, you.y_pos ) < 9)
    {
        const int ex = grx - you.x_pos + 9;
        const int ey = gry - you.y_pos + 9;

        if (env.show[ex][ey])
            return (true);
    }

    return (false);
}  // end see_grid() 

static const unsigned table[ NUM_CSET ][ NUM_DCHAR_TYPES ] = 
{
    // CSET_ASCII
    {
        '#', '*', '.', ',', '\'', '+', '^', '>', '<',  // wall, stairs up
        '_', '\\', '}', '{', '8', '~', '~',            // altar, item detect
        '0', ')', '[', '/', '%', '?', '=', '!', '(',   // orb, missile
        ':', '|', '}', '%', '$', '"', '#',             // book, cloud
    },

    // CSET_IBM - this is ANSI 437
    {
        177, 176, 249, 250, '\'', 254, '^', '>', '<',  // wall, stairs up
        220, 239, 244, 247, '8', '~', '~',             // altar, item detect
        '0', ')', '[', '/', '%', '?', '=', '!', '(',   // orb, missile
        '+', '\\', '}', '%', '$', '"', '#',            // book, cloud
    },

    // CSET_DEC - remember: 224-255 are mapped to shifted 96-127
    {
        225, 224, 254, ':', '\'', 238, '^', '>', '<',  // wall, stairs up
        251, 182, 167, 187, '8', 171, 168,             // altar, item detect
        '0', ')', '[', '/', '%', '?', '=', '!', '(',   // orb, missile
        '+', '\\', '}', '%', '$', '"', '#',            // book, cloud
    },

    // CSET_UNICODE
    {
        0x2592, 0x2591, 0xB7, 0x25E6, '\'', 0x25FC, '^', '>', '<',
        '_', 0x2229, 0x2320, 0x2248, '8', '~', '~',
        '0', ')', '[', '/', '%', '?', '=', '!', '(',
        '+', '|', '}', '%', '$', '"', '#',
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
    {
        if (dchar_names[i] == name)
            return dungeon_char_type(i);
    }
    return (NUM_DCHAR_TYPES);
}

void init_char_table( char_set_type set )
{
    for (int i = 0; i < NUM_DCHAR_TYPES; i++)
    {
        if (Options.cset_override[set][i])
            Options.char_table[i] = Options.cset_override[set][i];
        else
            Options.char_table[i] = table[set][i];
    }
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
        Feature[i].dchar  = NUM_DCHAR_TYPES;
        Feature[i].symbol = 0;
        Feature[i].colour = BLACK;      // means must be set some other way
        Feature[i].flags  = FFT_NONE;
        Feature[i].magic_symbol = 0;    // made equal to symbol if untouched
        Feature[i].map_colour = DARKGREY;
        Feature[i].seen_colour = BLACK;    // marks no special seen map handling
        Feature[i].seen_em_colour = BLACK;
        Feature[i].em_colour = BLACK;

        switch (i)
        {
        case DNGN_UNSEEN:
        default:
            break;

        case DNGN_ROCK_WALL:
        case DNGN_PERMAROCK_WALL:
            Feature[i].dchar = DCHAR_WALL;
            Feature[i].colour = EC_ROCK;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            break;

        case DNGN_STONE_WALL:
            Feature[i].dchar = DCHAR_WALL;
            Feature[i].colour = EC_STONE;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            break;

        case DNGN_OPEN_DOOR:
            Feature[i].dchar = DCHAR_DOOR_OPEN;
            Feature[i].colour = LIGHTGREY;
            break;

        case DNGN_CLOSED_DOOR:
            Feature[i].dchar = DCHAR_DOOR_CLOSED;
            Feature[i].colour = LIGHTGREY;
            break;

        case DNGN_METAL_WALL:
            Feature[i].dchar = DCHAR_WALL;
            Feature[i].colour = CYAN;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            break;

        case DNGN_SECRET_DOOR:
            // Note: get_secret_door_appearance means this probably isn't used
            Feature[i].dchar = DCHAR_WALL;
            Feature[i].colour = EC_ROCK;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            break;

        case DNGN_GREEN_CRYSTAL_WALL:
            Feature[i].dchar = DCHAR_WALL;
            Feature[i].colour = GREEN;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            break;

        case DNGN_ORCISH_IDOL:
            Feature[i].dchar = DCHAR_STATUE;
            Feature[i].colour = DARKGREY;
            break;

        case DNGN_WAX_WALL:
            Feature[i].dchar = DCHAR_WALL;
            Feature[i].colour = YELLOW;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            break;                  // wax wall

        case DNGN_GRANITE_STATUE:
            Feature[i].dchar = DCHAR_STATUE;
            Feature[i].colour = LIGHTGREY;
            break;

        case DNGN_LAVA:
            Feature[i].dchar = DCHAR_WAVY;
            Feature[i].colour = RED;
            break;

        case DNGN_DEEP_WATER:
            Feature[i].dchar = DCHAR_WAVY;
            Feature[i].colour = BLUE;
            break;

        case DNGN_SHALLOW_WATER:
            Feature[i].dchar = DCHAR_WAVY;
            Feature[i].colour = CYAN;
            break;

        case DNGN_FLOOR:
            Feature[i].dchar = DCHAR_FLOOR;
            Feature[i].colour = EC_FLOOR;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_FLOOR_MAGIC ];
            break;

        case DNGN_FLOOR_SPECIAL:
            Feature[i].dchar = DCHAR_FLOOR;
            Feature[i].colour = YELLOW;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_FLOOR_MAGIC ];
            break;

        case DNGN_EXIT_HELL:
            Feature[i].dchar = DCHAR_ARCH;
            Feature[i].colour = LIGHTRED;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = LIGHTRED;
            break;

        case DNGN_ENTER_HELL:
            Feature[i].dchar = DCHAR_ARCH;
            Feature[i].colour = RED;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = RED;
            break;

        case DNGN_TRAP_MECHANICAL:
            Feature[i].colour = LIGHTCYAN;
            Feature[i].dchar = DCHAR_TRAP;
            Feature[i].map_colour = LIGHTCYAN;
            break;

        case DNGN_TRAP_MAGICAL:
            Feature[i].colour = MAGENTA;
            Feature[i].dchar = DCHAR_TRAP;
            Feature[i].map_colour = MAGENTA;
            break;

        case DNGN_TRAP_III:
            Feature[i].colour = LIGHTGREY;
            Feature[i].dchar = DCHAR_TRAP;
            Feature[i].map_colour = LIGHTGREY;
            break;

        case DNGN_UNDISCOVERED_TRAP:
            Feature[i].dchar = DCHAR_FLOOR;
            Feature[i].colour = EC_FLOOR;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_FLOOR_MAGIC ];
            break;

        case DNGN_ENTER_SHOP:
            Feature[i].dchar = DCHAR_ARCH;
            Feature[i].colour = YELLOW;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = YELLOW;
            break;

        case DNGN_ENTER_LABYRINTH:
            Feature[i].dchar = DCHAR_ARCH;
            Feature[i].colour = CYAN;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = CYAN;
            break;

        case DNGN_ENTER_PORTAL_VAULT:
            Feature[i].flags |= FFT_NOTABLE;
            // fall through
            
        case DNGN_EXIT_PORTAL_VAULT:
            Feature[i].dchar       = DCHAR_ARCH;
            Feature[i].colour      = EC_SHIMMER_BLUE;
            Feature[i].map_colour  = LIGHTGREY;
            Feature[i].seen_colour = EC_SHIMMER_BLUE;
            break;            

        case DNGN_ROCK_STAIRS_DOWN:
            Feature[i].dchar = DCHAR_STAIRS_DOWN;
            Feature[i].colour = BROWN;
            Feature[i].map_colour = BROWN;
            break;
            
        case DNGN_STONE_STAIRS_DOWN_I:
        case DNGN_STONE_STAIRS_DOWN_II:
        case DNGN_STONE_STAIRS_DOWN_III:
            Feature[i].dchar          = DCHAR_STAIRS_DOWN;
            Feature[i].colour         = LIGHTGREY;
            Feature[i].em_colour      = WHITE;
            Feature[i].map_colour     = RED;
            Feature[i].seen_em_colour = WHITE;
            break;

        case DNGN_ROCK_STAIRS_UP:
            Feature[i].dchar = DCHAR_STAIRS_UP;
            Feature[i].colour = BROWN;
            Feature[i].map_colour = BROWN;
            break;
            
        case DNGN_STONE_STAIRS_UP_I:
        case DNGN_STONE_STAIRS_UP_II:
        case DNGN_STONE_STAIRS_UP_III:
            Feature[i].dchar = DCHAR_STAIRS_UP;
            Feature[i].colour = LIGHTGREY;
            Feature[i].map_colour = GREEN;
            Feature[i].em_colour      = WHITE;
            Feature[i].seen_em_colour = WHITE;
            break;

        case DNGN_ENTER_DIS:
            Feature[i].colour = CYAN;
            Feature[i].dchar = DCHAR_ARCH;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = CYAN;
            break;

        case DNGN_ENTER_GEHENNA:
            Feature[i].colour = RED;
            Feature[i].dchar = DCHAR_ARCH;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = RED;
            break;

        case DNGN_ENTER_COCYTUS:
            Feature[i].colour = LIGHTCYAN;
            Feature[i].dchar = DCHAR_ARCH;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = LIGHTCYAN;
            break;

        case DNGN_ENTER_TARTARUS:
            Feature[i].colour = DARKGREY;
            Feature[i].dchar = DCHAR_ARCH;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = DARKGREY;
            break;

        case DNGN_ENTER_ABYSS:
            Feature[i].colour = EC_RANDOM;
            Feature[i].dchar = DCHAR_ARCH;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = EC_RANDOM;
            break;

        case DNGN_EXIT_ABYSS:
            Feature[i].colour = EC_RANDOM;
            Feature[i].dchar = DCHAR_ARCH;
            Feature[i].map_colour = EC_RANDOM;
            break;

        case DNGN_STONE_ARCH:
            Feature[i].colour = LIGHTGREY;
            Feature[i].dchar = DCHAR_ARCH;
            Feature[i].map_colour = LIGHTGREY;
            break;

        case DNGN_ENTER_PANDEMONIUM:
            Feature[i].colour = LIGHTBLUE;
            Feature[i].dchar = DCHAR_ARCH;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = LIGHTBLUE;
            break;

        case DNGN_EXIT_PANDEMONIUM:
            // Note: has special handling for colouring with mutation
            Feature[i].colour = LIGHTBLUE;
            Feature[i].dchar = DCHAR_ARCH;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = LIGHTBLUE;
            break;

        case DNGN_TRANSIT_PANDEMONIUM:
            Feature[i].colour = LIGHTGREEN;
            Feature[i].dchar = DCHAR_ARCH;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = LIGHTGREEN;
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
            Feature[i].colour = YELLOW;
            Feature[i].dchar = DCHAR_STAIRS_DOWN;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = RED;
            Feature[i].seen_colour = YELLOW;
            break;

        case DNGN_ENTER_ZOT:
            Feature[i].colour = MAGENTA;
            Feature[i].dchar = DCHAR_ARCH;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = MAGENTA;
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
            Feature[i].colour = YELLOW;
            Feature[i].dchar = DCHAR_STAIRS_UP;
            Feature[i].map_colour  = GREEN;
            Feature[i].seen_colour = YELLOW;
            break;

        case DNGN_RETURN_FROM_ZOT:
            Feature[i].colour = MAGENTA;
            Feature[i].dchar = DCHAR_ARCH;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = MAGENTA;
            break;

        case DNGN_ALTAR_ZIN:
            Feature[i].colour = WHITE;
            Feature[i].dchar = DCHAR_ALTAR;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = WHITE;
            break;

        case DNGN_ALTAR_SHINING_ONE:
            Feature[i].colour = YELLOW;
            Feature[i].dchar = DCHAR_ALTAR;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = YELLOW;
            break;

        case DNGN_ALTAR_KIKUBAAQUDGHA:
            Feature[i].colour = DARKGREY;
            Feature[i].dchar = DCHAR_ALTAR;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = DARKGREY;
            break;

        case DNGN_ALTAR_YREDELEMNUL:
            Feature[i].colour = EC_UNHOLY;
            Feature[i].dchar = DCHAR_ALTAR;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = EC_UNHOLY;
            break;

        case DNGN_ALTAR_XOM:
            Feature[i].colour = EC_RANDOM;
            Feature[i].dchar = DCHAR_ALTAR;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = EC_RANDOM;
            break;

        case DNGN_ALTAR_VEHUMET:
            Feature[i].colour = EC_VEHUMET;
            Feature[i].dchar = DCHAR_ALTAR;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = EC_VEHUMET;
            break;

        case DNGN_ALTAR_OKAWARU:
            Feature[i].colour = CYAN;
            Feature[i].dchar = DCHAR_ALTAR;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = CYAN;
            break;

        case DNGN_ALTAR_MAKHLEB:
            Feature[i].colour = EC_FIRE;
            Feature[i].dchar = DCHAR_ALTAR;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = EC_FIRE;
            break;

        case DNGN_ALTAR_SIF_MUNA:
            Feature[i].colour = BLUE;
            Feature[i].dchar = DCHAR_ALTAR;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = BLUE;
            break;

        case DNGN_ALTAR_TROG:
            Feature[i].colour = RED;
            Feature[i].dchar = DCHAR_ALTAR;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = RED;
            break;

        case DNGN_ALTAR_NEMELEX_XOBEH:
            Feature[i].colour = LIGHTMAGENTA;
            Feature[i].dchar = DCHAR_ALTAR;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = LIGHTMAGENTA;
            break;

        case DNGN_ALTAR_ELYVILON:
            Feature[i].colour = LIGHTGREY;
            Feature[i].dchar = DCHAR_ALTAR;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = LIGHTGREY;
            break;

        case DNGN_ALTAR_LUGONU:
            Feature[i].colour = GREEN;
            Feature[i].dchar = DCHAR_ALTAR;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = GREEN;
            break;

        case DNGN_ALTAR_BEOGH:
            Feature[i].colour = EC_BEOGH;
            Feature[i].dchar = DCHAR_ALTAR;
            Feature[i].flags |= FFT_NOTABLE;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = EC_BEOGH;
            break;

        case DNGN_BLUE_FOUNTAIN:
            Feature[i].colour = BLUE;
            Feature[i].dchar = DCHAR_FOUNTAIN;
            break;

        case DNGN_SPARKLING_FOUNTAIN:
            Feature[i].colour = LIGHTBLUE;
            Feature[i].dchar = DCHAR_FOUNTAIN;
            break;

        case DNGN_DRY_FOUNTAIN_I:
        case DNGN_DRY_FOUNTAIN_II:
        case DNGN_PERMADRY_FOUNTAIN:
            Feature[i].colour = LIGHTGREY;
            Feature[i].dchar = DCHAR_FOUNTAIN;
            break;

        case DNGN_INVIS_EXPOSED:
            Feature[i].dchar = DCHAR_INVIS_EXPOSED;
            break;

        case DNGN_ITEM_DETECTED:
            Feature[i].magic_symbol = Options.char_table[ DCHAR_ITEM_DETECTED ];
            break;

        case DNGN_ITEM_ORB:
            Feature[i].dchar = DCHAR_ITEM_ORB;
            break;

        case DNGN_ITEM_WEAPON:
            Feature[i].dchar = DCHAR_ITEM_WEAPON;
            break;

        case DNGN_ITEM_ARMOUR:
            Feature[i].dchar = DCHAR_ITEM_ARMOUR;
            break;

        case DNGN_ITEM_WAND:
            Feature[i].dchar = DCHAR_ITEM_WAND;
            break;

        case DNGN_ITEM_FOOD:
            Feature[i].dchar = DCHAR_ITEM_FOOD;
            break;

        case DNGN_ITEM_SCROLL:
            Feature[i].dchar = DCHAR_ITEM_SCROLL;
            break;

        case DNGN_ITEM_RING:
            Feature[i].dchar = DCHAR_ITEM_RING;
            break;

        case DNGN_ITEM_POTION:
            Feature[i].dchar = DCHAR_ITEM_POTION;
            break;

        case DNGN_ITEM_MISSILE:
            Feature[i].dchar = DCHAR_ITEM_MISSILE;
            break;

        case DNGN_ITEM_BOOK:
            Feature[i].dchar = DCHAR_ITEM_BOOK;
            break;

        case DNGN_ITEM_STAVE:
            Feature[i].dchar = DCHAR_ITEM_STAVE;
            break;

        case DNGN_ITEM_MISCELLANY:
            Feature[i].dchar = DCHAR_ITEM_MISCELLANY;
            break;

        case DNGN_ITEM_CORPSE:
            Feature[i].dchar = DCHAR_ITEM_CORPSE;
            break;

        case DNGN_ITEM_GOLD:
            Feature[i].dchar = DCHAR_ITEM_GOLD;
            break;

        case DNGN_ITEM_AMULET:
            Feature[i].dchar = DCHAR_ITEM_AMULET;
            break;

        case DNGN_CLOUD:
            Feature[i].dchar = DCHAR_CLOUD;
            break;
        }

        if (i == DNGN_ENTER_ORCISH_MINES || i == DNGN_ENTER_SLIME_PITS
            || i == DNGN_ENTER_LABYRINTH)
            Feature[i].flags |= FFT_EXAMINE_HINT;

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
    const coord_def ep = view2show(grid2view(coord_def(x,y)));

    int             object = env.show(ep);
    unsigned short  colour = env.show_col(ep);
    unsigned        ch;

    if (!object)
        return get_envmap_char(x, y);

    if (object == DNGN_SECRET_DOOR)
        object = grid_secret_door_appearance( x, y );

    get_symbol( x, y, object, &ch, &colour );
    return (ch);
}

std::string stringize_glyph(unsigned glyph)
{
    if (crawl_state.glyph2strfn)
        return (*crawl_state.glyph2strfn)(glyph);

    return std::string(1, glyph);
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
	    int ch;
	    const coord_def view_d(count_x, count_y);

	    if (!view2grid_valid(view_d))
		ch = ' ';
	    else
	    {
		// in grid coords
		const hexcoord gc = view2grid(view_d);

		ch =
		    (!map_bounds(gc)) ? 0 
		    : (!crawl_view.in_grid_los(gc)) ? get_envmap_char(gc.x, gc.y) 
		    : (gc == you.pos()) ? you.symbol 
		    : get_screen_glyph(gc.x, gc.y);
		if (ch && !isprint(ch)) 
		{
		    // [ds] Evil hack time again. Peek at grid, use that character.
		    int object = grd(gc);
		    unsigned glych;
		    unsigned short glycol = 0;

		    if (object == DNGN_SECRET_DOOR)
			object = grid_secret_door_appearance( gc.x, gc.y );

		    get_symbol( gc.x, gc.y, object, &glych, &glycol );
		    ch = glych;
		}
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

    // Restore char and feature tables
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

static int viewmap_flash_colour()
{
    if (you.special_wield == SPWLD_SHADOW)
        return (DARKGREY);
    else if (you.duration[DUR_BERSERKER])
        return (RED);

    return (BLACK);
}

static void update_env_show(const coord_def &gp, const coord_def &ep)
{
    // The sequence is grid, items, clouds, monsters.
    env.show(ep) = grd(gp);
    env.show_col(ep) = 0;

    if (igrd(gp) != NON_ITEM)
        update_item_grid(gp, ep);

    const int cloud = env.cgrid(gp);
    if (cloud != EMPTY_CLOUD && env.cloud[cloud].type != CLOUD_NONE)
        update_cloud_grid(cloud);
    
    const monsters *mons = monster_at(gp);
    if (mons && mons->alive())
        update_monster_grid(mons);
}

// Updates one square of the view area. Should only be called for square
// in LOS.
void view_update_at(const coord_def &pos)
{
    if (pos == you.pos())
        return;

    const coord_def vp = grid2view(pos);
    const coord_def ep = view2show(vp);
    update_env_show(pos, ep);

    int object = env.show(ep);

    if (!object)
        return;
    
    unsigned short  colour = env.show_col(ep);
    unsigned        ch = 0;

    if (object == DNGN_SECRET_DOOR)
        object = grid_secret_door_appearance( pos.x, pos.y );

    get_symbol( pos.x, pos.y, object, &ch, &colour );

    int flash_colour = you.flash_colour;
    if (flash_colour == BLACK)
        flash_colour = viewmap_flash_colour();
    
    gotoxy(vp.x, vp.y);
    textattr(flash_colour? real_colour(flash_colour) : colour);
    putwch(ch);
}

bool view_update()
{
    if (you.num_turns > you.last_view_update)
    {
        viewwindow(true, false);
        return (true);
    }
    return (false);
}

static void show_between_hex(coord_def view_d,
	std::vector<screen_buffer_t>::pointer buf,
	int flash_colour)
{
    // We use the "spare" squares between actual hexes to
    // display supplementary information and for cosmetic
    // purposes.

    buf[0] = 0;
    buf[1] = BLACK;

    if (!Options.hex_interpolate_walls)
	return;

    // interpolation: if the hexes to the left and right are
    // both walls, draw a wall here.
    // TODO: make it optional
    int LEFT = 0, RIGHT = 1;
    int object[2] = {DNGN_UNSEEN,DNGN_UNSEEN};
    unsigned short colour[2] = {BLACK,BLACK};
    coord_def gc[2];

    bool do_interpolation = true;
    int special = -1;
    for (int dir = LEFT; dir <= RIGHT; dir++)
    {
	gc[dir] = view2grid(view_d + coord_def((dir == LEFT ? -1 : 1), 0));
	const coord_def ep = view2show(grid2view(gc[dir]));

	if (!map_bounds(gc[dir]))
	{
	    do_interpolation = false;
	    continue;
	}
	if (crawl_view.in_grid_los(gc[dir])
		&& env.show(ep) != DNGN_UNSEEN)
	{
	    object[dir] = env.show(ep);
	    colour[dir] = env.show_col(ep);
	    if (object[dir] == DNGN_SECRET_DOOR)
		object[dir] = grid_secret_door_appearance(gc[dir].x,
			gc[dir].y);
	}
	else
	{
	    object[dir] = env.map(gc[dir]).object;
	    colour[dir] = DARKGREY;
	}
	if ( object[dir] == DNGN_UNSEEN ||
		!grid_is_solid( static_cast<dungeon_feature_type>
		    (object[dir])))
	{
	    do_interpolation = false;
	    continue;
	}
	if ( object[dir] == DNGN_CLOSED_DOOR ||
		object[dir] == DNGN_ORCISH_IDOL ||
		object[dir] > DNGN_MINSEE)
	{
	    // we treat these specially - we don't interpolate between two
	    // such, but do interpolate a wall between one such and a wall.
	    // e.g. '# + #' -> '##+##', but '+ + +' -> '+ + +'
	    if (special != -1)
	    {
		do_interpolation = false;
		continue;
	    }
	    else
		special = dir;
	}
    }
    if (do_interpolation)
    {
	int copy_dir;
	if (special != -1)
	    copy_dir = 1-special; // other direction
	else
	    copy_dir = object[RIGHT] > object[LEFT];

	const coord_def ep =
	    view2show(grid2view(gc[copy_dir]));
	if (crawl_view.in_grid_los(gc[copy_dir])
		&& env.show(ep) != DNGN_UNSEEN)
	{
	    unsigned ch;
	    get_symbol( gc[copy_dir].x, gc[copy_dir].y, object[copy_dir], &ch,
		    &colour[copy_dir] );
	    buf[0] = ch;
	}
	else
	    buf[0] = get_envmap_char( gc[copy_dir].x, gc[copy_dir].y );

	if (!flash_colour)
	    buf[1] = colour[copy_dir];
	else
	{
	    // alter colour if flashing the characters vision
	    buf[1] =
		see_grid(gc[copy_dir].x, gc[copy_dir].y)?
		real_colour(flash_colour) : DARKGREY;
	}

	return;
    }
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
    screen_buffer_t *buffy(crawl_view.vbuf);
    
    int count_x, count_y;
    coord_def view_d;

    losight( env.show, grd, you.pos()); // must be done first

    env.show_col.init(LIGHTGREY);
    Show_Backup.init(0);

    item_grid();                // must be done before cloud and monster
    cloud_grid();
    monster_grid( do_updates );

    if (draw_it)
    {
        cursor_control cs(false);

        const bool map = player_in_mappable_area();
        const bool draw = !you.running || Options.travel_delay > -1;
        int bufcount = 0;

        int flash_colour = you.flash_colour;
        if (flash_colour == BLACK)
            flash_colour = viewmap_flash_colour();

        for (count_y = crawl_view.viewp.y; count_y <= crawl_view.viewsz.y;
             count_y++)
        {
            for (count_x = crawl_view.viewp.x; count_x <= crawl_view.viewsz.x;
                 count_x++)
            {
		view_d = coord_def(count_x, count_y);
		if (!view2grid_valid(view_d))
		    show_between_hex(view_d, &buffy[bufcount], flash_colour);
		else
		{
		    // in grid coords
		    const coord_def gc = view2grid(view_d);
		    const coord_def ep = view2show(view_d);

		    if (Options.tutorial_left && in_bounds(gc)
			    && crawl_view.in_grid_los(gc))
		    {
			const int object = env.show(ep);
			if (object)
			{
			    if (grid_is_rock_stair(grd(gc)))
				learned_something_new(
					TUT_SEEN_ESCAPE_HATCH, gc.x, gc.y);
			    if (is_feature('>', gc.x, gc.y))
				learned_something_new(TUT_SEEN_STAIRS, gc.x, gc.y);
			    else if (is_feature('_', gc.x, gc.y))
				learned_something_new(TUT_SEEN_ALTAR, gc.x, gc.y);
			    else if (grd(gc) == DNGN_CLOSED_DOOR
				    && see_grid( gc.x, gc.y ))
				learned_something_new(TUT_SEEN_DOOR, gc.x, gc.y);
			    else if (grd(gc) == DNGN_ENTER_SHOP
				    && see_grid( gc.x, gc.y ))
				learned_something_new(TUT_SEEN_SHOP, gc.x, gc.y);
			}
		    }

		    // order is important here
		    if (!map_bounds(gc))
		    {
			// off the map
			buffy[bufcount] = 0;
			buffy[bufcount + 1] = DARKGREY;
		    }
		    else if (!crawl_view.in_grid_los(gc))
		    {
			// outside the env.show area
			buffy[bufcount] = get_envmap_char( gc.x, gc.y );
			buffy[bufcount + 1] = DARKGREY;

			if (Options.colour_map)
			    buffy[bufcount + 1] = 
				colour_code_map(gc.x, gc.y, Options.item_colour);
		    }
		    else if (gc == you.pos())
		    {
			int             object = env.show(ep);
			unsigned short  colour = env.show_col(ep);
			unsigned        ch;
			get_symbol( gc.x, gc.y, object, &ch, &colour );

			if (map)
			{
			    set_envmap_glyph( gc.x, gc.y, object, colour );
			    set_terrain_seen( gc.x, gc.y );
			    set_envmap_detected_mons(gc.x, gc.y, false);
			    set_envmap_detected_item(gc.x, gc.y, false);
			}

			// player overrides everything in cell
			buffy[bufcount] = you.symbol;
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
			int             object = env.show(ep);
			unsigned short  colour = env.show_col(ep);
			unsigned        ch;

			if (object == DNGN_SECRET_DOOR)
			    object = grid_secret_door_appearance( gc.x, gc.y );

			get_symbol( gc.x, gc.y, object, &ch, &colour );

			buffy[bufcount] = ch;
			buffy[bufcount + 1] = colour;

			if (map)
			{
			    // This section is very tricky because it 
			    // duplicates the old code (which was horrid).

			    // if the grid is in LoS env.show was set and
			    // we set the buffer already, so...
			    if (buffy[bufcount] != 0)
			    {
				// ... map that we've seen this
				set_envmap_glyph( gc.x, gc.y, object, colour );
				set_terrain_seen( gc.x, gc.y );
				set_envmap_detected_mons(gc.x, gc.y, false);
				set_envmap_detected_item(gc.x, gc.y, false);
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
				    && is_terrain_seen( gc.x, gc.y ))
			    {
				get_symbol( gc.x, gc.y,
					Show_Backup(ep), &ch, &colour );
				set_envmap_glyph( gc.x, gc.y, Show_Backup(ep),
					colour );
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
				// show map
				buffy[bufcount] = get_envmap_char( gc.x, gc.y );
				buffy[bufcount + 1] = DARKGREY;

				if (Options.colour_map)
				    buffy[bufcount + 1] = 
					colour_code_map(gc.x, gc.y,
						Options.item_colour);
			    }
			}
		    }

		    // alter colour if flashing the characters vision
		    if (flash_colour && buffy[bufcount])
			buffy[bufcount + 1] =
			    see_grid(gc.x, gc.y)?
			    real_colour(flash_colour) : DARKGREY;

		}
		bufcount += 2;
	    }
	}

	// Leaving it this way because short flashes can occur in long ones,
	// and this simply works without requiring a stack.
	you.flash_colour = BLACK;

        // avoiding unneeded draws when running
        if (draw)
        {
            you.last_view_update = you.num_turns;
            puttext(crawl_view.viewp.x, crawl_view.viewp.y,
                    crawl_view.viewp.x + crawl_view.viewsz.x - 1,
                    crawl_view.viewp.y + crawl_view.viewsz.y - 1,
                    buffy);
        }
    }
}                               // end viewwindow()

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

//////////////////////////////////////////////////////////////////////////////
// crawl_view_geometry

const int crawl_view_geometry::message_min_lines;
const int crawl_view_geometry::hud_min_width;
const int crawl_view_geometry::hud_min_gutter;
const int crawl_view_geometry::hud_max_gutter;

crawl_view_geometry::crawl_view_geometry()
    : termsz(80, 24), viewp(1, 1), viewsz(33, 17),
      hudp(40, 1), hudsz(41, 17),
      msgp(1, viewp.y + viewsz.y), msgsz(80, 7),
      vbuf(), vgrdc(), viewhalfsz(), glos1(), glos2(),
      mousep(), last_player_pos()
{
}

void crawl_view_geometry::init_view()
{
    set_player_at(you.pos(), true);
}

void crawl_view_geometry::set_player_at(const hexcoord &c, bool centre)
{
    // [hex] XXX: we don't support locking just one of x or y
    if (centre || Options.view_lock_x || Options.view_lock_y)
    {
        vgrdc = c;
    }
    else
    {
        const hexcoord oldc = vgrdc;
        const int xmarg =
            Options.scroll_margin_x + 2*LOS_RADIUS <= viewhalfsz.x
            ? Options.scroll_margin_x
            : viewhalfsz.x - 2*LOS_RADIUS;
        const int ymarg =
            Options.scroll_margin_y + LOS_RADIUS <= viewhalfsz.y
            ? Options.scroll_margin_y
            : viewhalfsz.y - LOS_RADIUS;

	const hexdir d = c - vgrdc;
	const int dx = hex2screen(d).x;
	const int dy = hex2screen(d).y;
	
	const int leftdiff = (-viewhalfsz.x + xmarg) - (dx - 2*LOS_RADIUS);
	const int rightdiff = (dx + 2*LOS_RADIUS) - (viewhalfsz.x - xmarg);
	if (leftdiff > 0)
	    vgrdc -= hexdir::u * (leftdiff/2);
	else if (rightdiff > 0)
            vgrdc += hexdir::u * (rightdiff/2);

	const int topdiff = (-viewhalfsz.y + ymarg) - (dy - LOS_RADIUS);
	const int bottomdiff = (dy + LOS_RADIUS) - (viewhalfsz.y - ymarg);
	if ((topdiff > 0 || bottomdiff > 0))
	{
	    if (Options.symmetric_scroll &&
		    (c - last_player_pos).abs() == 1)
	    {
		// special handling for one-hex moves which cause us to scroll
		// vertically: scroll in the direction of the movement
		vgrdc += c - last_player_pos;
	    }
	    else
	    {
		if (topdiff > 0)
		{
		    vgrdc -= (hexdir::w - hexdir::v) * (topdiff/2);
		    if (topdiff % 2 == 1)
			vgrdc += (dx <= 0 ? hexdir::v : -hexdir::w);
		}
		else // (bottomdiff > 0)
		{
		    vgrdc += (hexdir::w - hexdir::v) * (bottomdiff/2);
		    if (bottomdiff % 2 == 1)
			vgrdc -= (dx >= 0 ? hexdir::v : -hexdir::w);
		}
	    }
	}

        if (vgrdc != oldc && Options.center_on_scroll)
            vgrdc = c;
    }

    glos1 = c.tosquare() - coord_def(LOS_RADIUS, LOS_RADIUS);
    glos2 = c.tosquare() + coord_def(LOS_RADIUS, LOS_RADIUS);

    last_player_pos = c;
}

void crawl_view_geometry::init_geometry()
{
    termsz = coord_def( get_number_of_cols(), get_number_of_lines() );

    // If the terminal is too small, exit with an error.
    if ((termsz.x < 80 || termsz.y < 24) && !crawl_state.need_save)
        end(1, false, "Terminal too small (%d,%d), need at least (80,24)",
            termsz.x, termsz.y);

    int freeheight = termsz.y - message_min_lines;

    // Make the viewport as tall as possible.
    viewsz.y = freeheight < Options.view_max_height?
        freeheight : Options.view_max_height;

    // Make sure we're odd-sized.
    if (!(viewsz.y % 2))
        --viewsz.y;

    // If the view is too short, force it to minimum height.
    if (viewsz.y < VIEW_MIN_HEIGHT)
        viewsz.y = VIEW_MIN_HEIGHT;

    // The message pane takes all lines not used by the viewport.
    msgp  = coord_def(1, viewsz.y + 1);
    msgsz = coord_def(termsz.x, termsz.y - viewsz.y);

    int freewidth = termsz.x - (hud_min_width + hud_min_gutter);
    // Make the viewport as wide as possible.
    viewsz.x = freewidth < Options.view_max_width?
        freewidth : Options.view_max_width;

    if (!(viewsz.x % 2))
        --viewsz.x;

    if (viewsz.x < VIEW_MIN_WIDTH)
        viewsz.x = VIEW_MIN_WIDTH;

    vbuf.size(viewsz);

    // The hud appears after the viewport + gutter.
    hudp = coord_def(viewsz.x + 1 + hud_min_gutter, 1);

    // HUD size never changes, but we may increase the gutter size (up to
    // the current max of 6).
    if (hudp.x + hudsz.x - 1 < termsz.x)
    {
        const int hudmarg = termsz.x - (hudp.x + hudsz.x - 1);
        const int hud_increase_max = hud_max_gutter - hud_min_gutter;
        hudp.x += hudmarg > hud_increase_max? hud_increase_max : hudmarg;
    }

    viewhalfsz = viewsz / 2;
    
    init_view();
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
