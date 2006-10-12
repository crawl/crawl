/*
 *  File:       view.cc
 *  Summary:    Misc function used to render the dungeon.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
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

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "clua.h"
#include "debug.h"
#include "direct.h"
#include "insult.h"
#include "itemprop.h"
#include "macro.h"
#include "misc.h"
#include "monstuff.h"
#include "mon-util.h"
#include "overmap.h"
#include "player.h"
#include "skills2.h"
#include "stuff.h"
#include "spells4.h"
#include "stash.h"
#include "travel.h"

// These are hidden from the rest of the world... use the functions
// below to get information about the map grid.
#define MAP_MAGIC_MAPPED_FLAG   0x0100
#define MAP_SEEN_FLAG           0x0200
#define MAP_CHANGED_FLAG        0x0400
#define MAP_DETECTED_MONSTER    0x0800
#define MAP_DETECTED_ITEM       0x1000

#define MAP_CHARACTER_MASK      0x00ff

struct feature_def
{
    unsigned short      symbol;          // symbol used for seen terrain
    unsigned short      colour;          // normal in LoS colour
    unsigned short      magic_symbol;    // symbol used for magic-mapped terrain
    unsigned short      map_colour;      // colour when out of LoS on display
    unsigned short      seen_colour;     // map_colour when is_terrain_seen()
    bool                notable;         // gets noted when seen
    bool                seen_effect;     // requires special handling when seen
};

static FixedVector< struct feature_def, NUM_FEATURES >  Feature;

#if defined(DOS_TERM)
// DOS functions like gettext() and puttext() can only
// work with arrays of characters, not shorts.
typedef unsigned char screen_buffer_t;
#else
typedef unsigned short screen_buffer_t;
#endif

FixedArray < unsigned int, 20, 19 > Show_Backup;

unsigned char show_green;
extern int stealth;             // defined in acr.cc

// char colour_code_map(unsigned char map_value);
screen_buffer_t colour_code_map( int x, int y, bool item_colour = false,
                                    bool travel_colour = false );

void cloud_grid(void);
void monster_grid(bool do_updates);

static int get_item_dngn_code(const item_def &item);
static void set_show_backup( int ex, int ey );

// Applies EC_ colour substitutions and brands.
static unsigned fix_colour(unsigned raw_colour);

#if defined(WIN32CONSOLE) || defined(DOS) || defined(DOS_TERM)
static unsigned short dos_brand( unsigned short colour,
                                 unsigned brand = CHATTR_REVERSE);
#endif

//---------------------------------------------------------------
//
// get_number_of_lines
//
// Made this a function instead of a #define.  This should help
// considering the fact that the curses version is a macro 
// (curses tends to be implemented with a large number of 
// preprocessor macros, which can wreak havoc with things
// like the C++ string class, so we want to isolate that
// away to keep portability up). 
//
// Other OSes might want to hook into reading system environment 
// variables or player set options to determine the screen size 
// (see the Options and SysEnv structures, as well as initfile.cc).
//
// This might be better to move to the lib*.cc files, but we
// don't really have a standard API defined for them, or the
// all important libdos.cc.  It would be a good idea to eventually
// head that way. -- bwr
//
//---------------------------------------------------------------
int get_number_of_lines(void)
{
#ifdef UNIX
    return (get_number_of_lines_from_curses());
#else
    return (25);
#endif
}

unsigned get_envmap_char(int x, int y)
{
    return static_cast<unsigned char>(
            env.map[x - 1][y - 1] & MAP_CHARACTER_MASK);
}

void set_envmap_detected_item(int x, int y, bool detected)
{
    if (detected)
        env.map[x - 1][y - 1] |= MAP_DETECTED_ITEM;
    else
        env.map[x - 1][y - 1] &= ~MAP_DETECTED_ITEM;
}

bool is_envmap_detected_item(int x, int y)
{
    return (env.map[x - 1][y - 1] & MAP_DETECTED_ITEM);
}

void set_envmap_detected_mons(int x, int y, bool detected)
{
    if (detected)
        env.map[x - 1][y - 1] |= MAP_DETECTED_MONSTER;
    else
        env.map[x - 1][y - 1] &= ~MAP_DETECTED_MONSTER;
}

bool is_envmap_detected_mons(int x, int y)
{
    return (env.map[x - 1][y - 1] & MAP_DETECTED_MONSTER);
}

void set_envmap_char( int x, int y, unsigned char chr )
{
    env.map[x - 1][y - 1] &= (~MAP_CHARACTER_MASK);       // clear old first
    env.map[x - 1][y - 1] |= chr;
}

bool is_terrain_known( int x, int y )
{
    return (env.map[x - 1][y - 1] & (MAP_MAGIC_MAPPED_FLAG | MAP_SEEN_FLAG));
}

bool is_terrain_seen( int x, int y )
{
    return (env.map[x - 1][y - 1] & MAP_SEEN_FLAG);
}

bool is_terrain_changed( int x, int y )
{
    return (env.map[x - 1][y - 1] & MAP_CHANGED_FLAG);
}

// used to mark dug out areas, unset when terrain is seen or mapped again.
void set_terrain_changed( int x, int y )
{
    env.map[x - 1][y - 1] |= MAP_CHANGED_FLAG;
}

void set_terrain_mapped( int x, int y )
{
    env.map[x - 1][y - 1] &= (~MAP_CHANGED_FLAG);
    env.map[x - 1][y - 1] |= MAP_MAGIC_MAPPED_FLAG;
}

void set_terrain_seen( int x, int y )
{
    env.map[x - 1][y - 1] &= (~MAP_CHANGED_FLAG);
    env.map[x - 1][y - 1] |= MAP_SEEN_FLAG;
}

void clear_envmap_grid( int x, int y )
{
    env.map[x - 1][y - 1] = 0;
}

void clear_envmap( void )
{
    for (int i = 0; i < GXM; i++)
    {
        for (int j = 0; j < GYM; j++)
        {
            env.map[i][j] = 0;
        }
    }
}

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
    default:
        return (CHATTR_NORMAL);
    }
}

static unsigned fix_colour(unsigned raw_colour)
{
    // This order is important - is_element_colour() doesn't want to see the
    // munged colours returned by dos_brand, so it should always be done 
    // before applying DOS brands.
    const int colflags = raw_colour & 0xFF00;

    // Evaluate any elemental colours to guarantee vanilla colour is returned
    if (is_element_colour( raw_colour ))
        raw_colour = element_colour( raw_colour );

#if defined(WIN32CONSOLE) || defined(DOS) || defined(DOS_TERM)
    if (colflags)
    {
        unsigned brand = colflag2brand(raw_colour & 0xFF00);
        raw_colour = dos_brand(raw_colour & 0xFF, brand);
    }
#endif

#ifndef USE_COLOUR_OPTS
    // Strip COLFLAGs for systems that can't do anything meaningful with them.
    raw_colour &= 0xFF;
#endif

    return (raw_colour);
}

static void get_symbol( unsigned int object, unsigned short *ch, 
                        unsigned short *colour )
{
    ASSERT( ch != NULL );
    ASSERT( colour != NULL );

    if (object < NUM_FEATURES)
    {
        *ch = Feature[object].symbol;    

        // Don't clobber with BLACK, because the colour should be already set.
        if (Feature[object].colour != BLACK)
            *colour = Feature[object].colour;

        // Note anything we see that's notable
        if (Feature[object].notable)
            seen_notable_thing( object );

        // These effects apply every turn when in sight however.
        if (Feature[object].seen_effect)
        {
            if (object == DNGN_SILVER_STATUE)
                you.visible_statue[ STATUE_SILVER ] = 1;
            else if (object == DNGN_ORANGE_CRYSTAL_STATUE)
                you.visible_statue[ STATUE_ORANGE_CRYSTAL ] = 1;
        }
    }
    else
    {
        ASSERT( object >= DNGN_START_OF_MONSTERS );
        *ch = mons_char( object - DNGN_START_OF_MONSTERS );
    }

    *colour = fix_colour(*colour);
}

unsigned char get_sightmap_char( int feature )
{
    if (feature < NUM_FEATURES)
        return (Feature[feature].symbol);

    return (0);
}

unsigned char get_magicmap_char( int feature )
{
    if (feature < NUM_FEATURES)
        return (Feature[feature].magic_symbol);

    return (0);
}

static char get_travel_colour( int x, int y )
{
    if (is_waypoint(x + 1, y + 1))
        return LIGHTGREEN;

    short dist = point_distance[x + 1][y + 1];
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

static unsigned short dos_brand( unsigned short colour,
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
    // XXX: Yes, the map array and the grid array are off by one. -- bwr
    const int map_value = (unsigned char) env.map[x][y];
    const unsigned short map_flags = env.map[x][y];
    const int grid_value = grd[x + 1][y + 1];

    unsigned tc = travel_colour? 
                        get_travel_colour(x, y)
                      : Feature[grid_value].map_colour;

    if (map_flags & MAP_DETECTED_ITEM)
        tc = Options.detected_item_colour;
    
    if (map_flags & MAP_DETECTED_MONSTER)
    {
        tc = Options.detected_monster_colour;
        return fix_colour(tc);
    }

    // XXX: [ds] If we've an important colour, override other feature
    // colouring. Yes, this is hacky. Story of my life.
    if (tc == LIGHTGREEN || tc == LIGHTMAGENTA)
        return fix_colour(tc);

    // XXX: Yeah, this is ugly, but until we have stored layers in the
    // map we can't tell if we've seen a square, detected it, or just
    // detected the item or monster on top... giving colour here will
    // result in detect creature/item detecting features like stairs. -- bwr
    if (map_value != get_sightmap_char(grid_value))
    {
        // If there's an item on this square, change colour to indicate
        // that, iff the item's glyph matches map_value. XXX: Potentially
        // abusable? -- ds 
        int item = igrd[x + 1][y + 1];
        if (item_colour && item != NON_ITEM 
                && map_value == 
                        get_sightmap_char(get_item_dngn_code(mitm[item])))
        {
            unsigned ic = mitm[item].colour;

            if (mitm[item].link != NON_ITEM )
                ic |= COLFLAG_ITEM_HEAP;

            // If the item colour is the background colour, tweak it to WHITE
            // instead to catch the player's eye.
            return fix_colour( ic == tc? WHITE : ic );
        }
    }

    return fix_colour(tc);
}

void clear_map()
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

            unsigned short envc = env.map[x][y] & MAP_CHARACTER_MASK;
            if (!envc)
                continue;

            const int item = igrd[x + 1][y + 1];
            if (item != NON_ITEM 
                    && envc == 
                        get_sightmap_char(get_item_dngn_code(mitm[item])))
                continue;

            set_envmap_char(x, y,
                    is_terrain_seen(x, y)? get_sightmap_char(grd[x][y]) :
                    is_terrain_known(x, y)? get_magicmap_char(grd[x][y]) :
                                            0);
            set_envmap_detected_mons(x, y, false);
        }
    }
}

void monster_grid(bool do_updates)
{
    struct monsters *monster = 0;       // NULL {dlb}

    for (int s = 0; s < MAX_MONSTERS; s++)
    {
        monster = &menv[s];

        if (monster->type != -1 && mons_near(monster))
        {
            if (do_updates 
                && (monster->behaviour == BEH_SLEEP
                     || monster->behaviour == BEH_WANDER) 
                && check_awaken(s))
            {
                behaviour_event( monster, ME_ALERT, MHITYOU );

                if (you.turn_is_over
                    && mons_shouts(monster->type) > 0
                    && random2(30) >= you.skills[SK_STEALTH])
                {
                    int noise_level = 8; 

                    if (!mons_friendly(monster)
                        && (!silenced(you.x_pos, you.y_pos) 
                            && !silenced(monster->x, monster->y)))
                    {
                        if (mons_is_demon( monster->type ) && coinflip())
                        {
                            if (monster->type == MONS_IMP
                                || monster->type == MONS_WHITE_IMP
                                || monster->type == MONS_SHADOW_IMP)
                            {
                                imp_taunt( monster );
                            }
                            else
                            {
                                demon_taunt( monster );
                            }
                        }
                        else
                        {
                            int the_shout = mons_shouts(monster->type);

                            strcpy(info, "You hear ");
                            switch (the_shout)
                            {
                            case S_SILENT:
                            default:
                                strcat(info, "buggy behaviour!");
                                break;
                            case S_SHOUT:
                                strcat(info, "a shout!");
                                break;
                            case S_BARK:
                                strcat(info, "a bark!");
                                break;
                            case S_SHOUT2:
                                strcat(info, "two shouts!");
                                noise_level = 12;
                                break;
                            case S_ROAR:
                                strcat(info, "a roar!");
                                noise_level = 12;
                                break;
                            case S_SCREAM:
                                strcat(info, "a hideous shriek!");
                                break;
                            case S_BELLOW:
                                strcat(info, "a bellow!");
                                break;
                            case S_SCREECH:
                                strcat(info, "a screech!");
                                break;
                            case S_BUZZ:
                                strcat(info, "an angry buzzing noise.");
                                break;
                            case S_MOAN:
                                strcat(info, "a chilling moan.");
                                break;
                            case S_WHINE:
                                strcat(info,
                                       "an irritating high-pitched whine.");
                                break;
                            case S_CROAK:
                                if (coinflip())
                                    strcat(info, "a loud, deep croak!");
                                else
                                    strcat(info, "a croak.");
                                break;
                            case S_GROWL:
                                strcat(info, "an angry growl!");
                                break;
                            case S_HISS:
                                strcat(info, "an angry hiss!");
                                noise_level = 4;  // not very loud -- bwr
                                break;
                            }

                            mpr(info);
                        }
                    }

                    noisy( noise_level, monster->x, monster->y );
                }
            }

            const int ex = monster->x - you.x_pos + 9;
            const int ey = monster->y - you.y_pos + 9;
            if (!player_monster_visible( monster ))
            {
                // ripple effect?
                if (grd[monster->x][monster->y] == DNGN_SHALLOW_WATER
                    && !mons_flies(monster))
                {
                    set_show_backup(ex, ey);
                    env.show[ex][ey] = DNGN_INVIS_EXPOSED;
                }
                continue;
            }
            else if (!mons_friendly( monster )
                     && !mons_is_mimic( monster->type )
                     && !mons_class_flag( monster->type, M_NO_EXP_GAIN ))
            {
                interrupt_activity( AI_SEE_MONSTER );
				seen_monster( monster );

                if (you.running != 0
#ifdef CLUA_BINDINGS
                    && clua.callbooleanfn(true, "ch_stop_run", "M", monster)
#endif
                   )
                {
                    // Friendly monsters, mimics, or harmless monsters 
                    // don't disturb the player's running/resting.
                    // 
                    // Doing it this way causes players in run mode 2
                    // to move one square, and in mode 1 to stop.  This
                    // means that the character will run one square if
                    // a monster is in sight... we automatically jump
                    // to zero if we're resting.  -- bwr
                    if (you.run_x == 0 && you.run_y == 0)
                        stop_running();
                    else if (you.running > 1)
                        you.running--;
                    else
                        stop_running();
                }
            }

            // mimics are always left on map
            if (!mons_is_mimic( monster->type ))
                set_show_backup(ex, ey);

            env.show[ex][ey] = monster->type + DNGN_START_OF_MONSTERS;
            // FIXME - use monster->colour instead
            env.show_col[ex][ey]
                = ((mcolour[monster->type] == BLACK)
                        ? monster->number : mcolour[monster->type]);

            if (mons_friendly(monster))
            {
                env.show_col[ex][ey] |= COLFLAG_FRIENDLY_MONSTER;
            }
            else if (Options.stab_brand != CHATTR_NORMAL
                    && mons_looks_stabbable(monster))
            {
                env.show_col[ex][ey] |= COLFLAG_WILLSTAB;
            }
            else if (Options.may_stab_brand != CHATTR_NORMAL
                    && mons_looks_distracted(monster))
            {
                env.show_col[ex][ey] |= COLFLAG_MAYSTAB;
            }
        }                       // end "if (monster->type != -1 && mons_ner)"
    }                           // end "for s"
}                               // end monster_grid()


bool check_awaken(int mons_aw)
{
    int mons_perc = 0;
    struct monsters *monster = &menv[mons_aw];
    const int mon_holy = mons_holiness(monster);

    // berserkers aren't really concerned about stealth
    if (you.berserker)
        return (true);

    // Repel undead is a holy aura, to which evil creatures are sensitive.
    // Note that even though demons aren't affected by repel undead, they
    // do sense this type of divine aura. -- bwr
    if (you.duration[DUR_REPEL_UNDEAD] 
        && (mon_holy == MH_UNDEAD || mon_holy == MH_DEMONIC))
    {
        return (true);
    }

    // I assume that creatures who can see invisible are very perceptive
    mons_perc = 10 + (mons_intel(monster->type) * 4) + monster->hit_dice
                   + mons_see_invis(monster) * 5;

    // critters that are wandering still have MHITYOU as their foe are
    // still actively on guard for the player,  even if they can't see
    // him.  Give them a large bonus (handle_behaviour() will nuke 'foe'
    // after a while,  removing this bonus.
    if (monster->behaviour == BEH_WANDER && monster->foe == MHITYOU)
        mons_perc += 15;

    if (!mons_player_visible(monster))
        mons_perc -= 75;

    if (monster->behaviour == BEH_SLEEP)
    {
        if (mon_holy == MH_NATURAL)
        {
            // monster is "hibernating"... reduce chance of waking
            if (mons_has_ench( monster, ENCH_SLEEP_WARY ))
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

    // glowing with magical contamination isn't very stealthy
    if (you.magic_contamination > 10)
        mons_perc += you.magic_contamination - 10;

    if (mons_perc < 0)
        mons_perc = 0;

    return (random2(stealth) <= mons_perc);
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

void item_grid()
{
    char count_x, count_y;

    for (count_y = (you.y_pos - 8); (count_y < you.y_pos + 9); count_y++)
    {
        for (count_x = (you.x_pos - 8); (count_x < you.x_pos + 9); count_x++)
        {
            if (count_x >= 0 && count_x < GXM && count_y >= 0 && count_y < GYM)
            {
                if (igrd[count_x][count_y] != NON_ITEM)
                {
                    const int ix = count_x - you.x_pos + 9;
                    const int iy = count_y - you.y_pos + 9;
                    if (env.show[ix][iy])
                    {
                        const item_def &eitem = mitm[igrd[count_x][count_y]];
                        unsigned short &ecol = env.show_col[ix][iy];
                        
                        ecol = (grd[count_x][count_y] == DNGN_SHALLOW_WATER)?
                                        CYAN
                                      : eitem.colour;

                        if (eitem.link != NON_ITEM)
                        {
                            ecol |= COLFLAG_ITEM_HEAP;
                        }
                        env.show[ix][iy] = get_item_dngn_code( eitem );
                    }
                }
            }
        }                       // end of "for count_y, count_x"
    }
}                               // end item()


void cloud_grid(void)
{
    int mnc = 0;

    // btw, this is also the 'default' color {dlb}
    unsigned char which_colour = LIGHTGREY;

    for (int s = 0; s < MAX_CLOUDS; s++)
    {
        // can anyone explain this??? {dlb}
        // its an optimization to avoid looking past the last cloud -bwr
        if (mnc > env.cloud_no) 
            break;

        if (env.cloud[s].type != CLOUD_NONE)
        {
            mnc++;

            if (see_grid(env.cloud[s].x, env.cloud[s].y))
            {
                const int ex = env.cloud[s].x - you.x_pos + 9;
                const int ey = env.cloud[s].y - you.y_pos + 9;
                
                switch (env.cloud[s].type)
                {
                case CLOUD_FIRE:
                case CLOUD_FIRE_MON:
                    if (env.cloud[s].decay <= 20)
                        which_colour = RED;
                    else if (env.cloud[s].decay <= 40)
                        which_colour = LIGHTRED;
                    else if (one_chance_in(4))
                        which_colour = RED;
                    else if (one_chance_in(4))
                        which_colour = LIGHTRED;
                    else
                        which_colour = YELLOW;
                    break;

                case CLOUD_STINK:
                case CLOUD_STINK_MON:
                    which_colour = GREEN;
                    break;

                case CLOUD_COLD:
                case CLOUD_COLD_MON:
                    if (env.cloud[s].decay <= 20)
                        which_colour = BLUE;
                    else if (env.cloud[s].decay <= 40)
                        which_colour = LIGHTBLUE;
                    else if (one_chance_in(4))
                        which_colour = BLUE;
                    else if (one_chance_in(4))
                        which_colour = LIGHTBLUE;
                    else
                        which_colour = WHITE;
                    break;

                case CLOUD_POISON:
                case CLOUD_POISON_MON:
                    which_colour = (one_chance_in(3) ? LIGHTGREEN : GREEN);
                    break;

                case CLOUD_BLUE_SMOKE:
                case CLOUD_BLUE_SMOKE_MON:
                    which_colour = LIGHTBLUE;
                    break;

                case CLOUD_PURP_SMOKE:
                case CLOUD_PURP_SMOKE_MON:
                    which_colour = MAGENTA;
                    break;

                case CLOUD_MIASMA:
                case CLOUD_MIASMA_MON:
                case CLOUD_BLACK_SMOKE:
                case CLOUD_BLACK_SMOKE_MON:
                    which_colour = DARKGREY;
                    break;

                default:
                    which_colour = LIGHTGREY;
                    break;
                }

                set_show_backup(ex, ey);
                env.show[ex][ey] = DNGN_CLOUD;
                env.show_col[ex][ey] = which_colour;
            }
        }                       // end 'if != CLOUD_NONE'
    }                           // end 'for s' loop
}                               // end cloud_grid()

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

/* ========================================================================
 *                           brand new LOS code
 * ========================================================================
 * The new LOS works via a new (I think) shadow casting algorithm,
 * plus an esthetic tweak for more pleasing corner illumination.  More
 * detail can be had by contacting its author,  Gordon Lipford.          */

#define MAX_LIGHT_RADIUS    20
#define CIRC_MAX            32000
#define BIG_SHADOW          32000

// the following two constants represent the 'middle' of the sh array.
// since the current shown area is 19x19,  centering the view at (9,9)
// means it will be exactly centered.
// This is done to accomodate possible future changes in viewable screen
// area - simply change sh_xo and sh_yo to the new view center.

const int sh_xo = 9;            // X and Y origins for the sh array
const int sh_yo = 9;

// the Cell class,  used in the shadow-casting LOS algorithm
class Cell
{

public:
    int up_count;
    int up_max;
    int low_count;
    int low_max;
    bool lit;
    bool lit_delay;
    bool visible;               // for blockers only
    void init();
    bool reachedLower();
    bool reachedUpper();

    Cell()
    {
        init();
    };
};

void Cell::init()
{
    up_count = 0;
    up_max = 0;
    low_count = 0;
    low_max = 0;
    lit = true;
    visible = true;
    lit_delay = false;
}

bool Cell::reachedLower()
{
    // integer math: a 'step' has a value of 10
    // see if we're within a half step of the max.  VERY important
    // to use 'half step' or else things look really stupid.
    if (low_max != 0 && low_count + 5 >= low_max && low_count - 5 < low_max)
        return true;

    return false;
}

bool Cell::reachedUpper()
{
    // see if we're within a half step of the max.  VERY important
    // to use 'half step' or else things look really stupid.
    if (up_max != 0 && up_count + 5 >= up_max && up_count - 5 < up_max)
        return true;

    return false;
}

// the cell array
static FixedVector < Cell, MAX_LIGHT_RADIUS + 1 > cells;

// the 'circle' array.  For any given row,  we won't check higher than
// this given cell.
static FixedVector < int, MAX_LIGHT_RADIUS + 1 > circle;

// current light radius
static int LR = 0;

// View constant
const int view = 2;             // 1=widest LOS .. 5=narrowest

// initialize LOS code for a given light radius
extern void setLOSRadius(int newLR)
{
    int i, j;

    // sanity check - also allows multiple calls w/out performance loss
    if (LR == newLR)
        return;

    LR = newLR;
    // cells should already be initted.  calculate the circle array.

    // note that rows 0 and 1 will always go to infinity.
    circle[0] = circle[1] = CIRC_MAX;

    // for the rest,  simply calculate max height based on light rad.
    for (i = 2; i <= LR; i++)
    {
        // check top
        if (2 * i * i <= LR * LR)
        {
            circle[i] = CIRC_MAX;
            continue;
        }

        for (j = i - 1; j >= 0; j--)
        {
            // check that Distance (I^2 + J^2) is no more than (R+0.5)^2
            // this rounding allows for *much* better looking circles.
            if (i * i + j * j <= LR * LR + LR)
            {
                circle[i] = j;
                break;
            }
        }
    }
}

static int calcUpper(int bX, int bY)
{
    // got a blocker at row bX,  cell bY.  do all values
    // and scale by a factor of 10 for the integer math.
    int upper;

    upper = (10 * (10 * bX - view)) / (10 * bY + view);
    if (upper < 10)             // upper bound for blocker on diagonal
        upper = 10;

    return upper;
}

static int calcLower(int bX, int bY)
{
    // got a blocker at row bX,  cell bY.  do all values
    // and scale by a factor of 10 for the integer math.

    if (bY == 0)
        return BIG_SHADOW;

    return (10 * (10 * bX + view)) / (10 * bY - view);
}

// for easy x,y octant translation
static int xxcomp[8] = { 1, 0, 0, -1, -1, 0, 0, 1 };
static int xycomp[8] = { 0, 1, -1, 0, 0, -1, 1, 0 };
static int yxcomp[8] = { 0, 1, 1, 0, 0, -1, -1, 0 };
static int yycomp[8] = { 1, 0, 0, 1, -1, 0, 0, -1 };

static void los_octant(int o, FixedArray < unsigned int, 19, 19 > &sh,
                       FixedArray < unsigned char, 80, 70 > &gr, int x_p,
                       int y_p)
{
    int row, cell, top, south;
    int tx, ty;                 // translated x, y deltas for this octant
    unsigned char gv;           // grid value
    bool row_dark, all_dark;
    bool blocker, vis_corner;
    int up_inc, low_inc;

    // leave [0,0] alone,  because the old LOS code seems to.

    // init cell[0].  this is the only one that needs clearing.
    cells[0].init();
    all_dark = false;
    vis_corner = false;

    // loop through each row
    for (row = 1; row <= LR; row++)
    {
        row_dark = true;

        // loop through each cell,  up to the max allowed by circle[]
        top = circle[row];
        if (top > row)
            top = row;

        for (cell = 0; cell <= top; cell++)
        {
            // translate X,Y co'ord + bounds check
            tx = row * xxcomp[o] + cell * xycomp[o];
            ty = row * yxcomp[o] + cell * yycomp[o];

            if (x_p + tx < 0 || x_p + tx > 79 || y_p + ty < 0 || y_p + ty > 69)
                continue;

            // check for all_dark - we've finished the octant but
            // have yet to fill in '0' for the rest of the sight grid
            if (all_dark == true)
            {
                sh[sh_xo + tx][sh_yo + ty] = 0;
                continue;
            }

            // get grid value.. see if it blocks LOS
            gv = gr[x_p + tx][y_p + ty];
            blocker = (gv < MINSEE);

            // init some other variables
            up_inc = 10;
            low_inc = 10;
            south = cell - 1;

            // STEP 1 - inherit values from immediate West, if possible
            if (cell < row)
            {
                // check for delayed lighting
                if (cells[cell].lit_delay)
                {
                    if (!blocker)
                    {           // blockers don't light up with lit_delay.
                        if (cells[south].lit)
                        {
                            if (cells[south].low_max != 0)
                            {
                                cells[cell].lit = false;
                                // steal lower values
                                cells[cell].low_max = cells[south].low_max;
                                cells[cell].low_count = cells[south].low_count;
                                cells[south].low_count = 0;
                                cells[south].low_max = 0;
                                low_inc = 0;    // avoid double-inc.
                            }
                            else
                                cells[cell].lit = true;
                        }
                    }
                    cells[cell].lit_delay = false;
                }
            }
            else
            {
                // initialize new cell.
                cells[cell].init();
            }

            // STEP 2 - check for blocker
            // a dark blocker in shadow's edge will be visible
            if (blocker)
            {
                if (cells[cell].lit || (cell != 0 && cells[south].lit)
                    || vis_corner)
                {
                    // hack: make 'corners' visible
                    vis_corner = cells[cell].lit;

                    cells[cell].lit = false;
                    cells[cell].visible = true;

                    int upper = calcUpper(row, cell);
                    int lower = calcLower(row, cell);

                    if (upper < cells[cell].up_max || cells[cell].up_max == 0)
                    {
                        // new upper shadow
                        cells[cell].up_max = upper;
                        cells[cell].up_count = 0;
                        up_inc = 0;
                    }

                    if (lower > cells[cell].low_max || cells[cell].low_max == 0)
                    {
                        // new lower shadow
                        cells[cell].low_max = lower;
                        cells[cell].low_count = -10;
                        low_inc = 0;
                        if (lower <= 30)        // somewhat arbitrary
                            cells[cell].lit_delay = true;
                        // set dark_delay if lower > 20?? how to decide?
                    }
                }
                else
                {
                    cells[cell].visible = false;
                }
            }
            else
            {
                cells[cell].visible = false;    // special flags for blockers
            }

            // STEP 3 - add increments to upper, lower counts
            cells[cell].up_count += up_inc;
            cells[cell].low_count += low_inc;

            // STEP 4 - check south for dark
            if (south >= 0)
                if (cells[south].reachedUpper() == true)
                {
                    if (cells[cell].reachedUpper() == false)
                    {
                        cells[cell].up_max = cells[south].up_max;
                        cells[cell].up_count = cells[south].up_count;
                        cells[cell].up_count -= cells[south].up_max;
                    }
                    cells[cell].lit = false;
                    cells[cell].visible = false;
                }

            // STEP 5 - nuke lower if south lower
            if (south >= 0)
            {
                if (cells[south].reachedLower())
                {
                    cells[cell].low_max = cells[south].low_max;
                    cells[cell].low_count = cells[south].low_count;
                    cells[cell].low_count -= cells[south].low_max;
                    cells[south].low_count = cells[south].low_max = 0;
                }

                if (cells[south].low_max != 0
                    || (cells[south].lit == false
                        && cells[south].low_max == 0))
                {
                    cells[cell].low_count = cells[cell].low_max + 10;
                }
            }

            // STEP 6 - light up if we've reached lower bound
            if (cells[cell].reachedLower() == true)
                cells[cell].lit = true;

            // now place appropriate value in sh
            if (cells[cell].lit == true
                || (blocker == true && cells[cell].visible == true))
            {
                sh[sh_xo + tx][sh_yo + ty] = gv;
            }
            else
                sh[sh_xo + tx][sh_yo + ty] = 0;

            if (cells[cell].lit == true)
                row_dark = false;
        }                       // end for - cells

        vis_corner = false;     // don't carry over to next row. :)
        if (row_dark == true)
            all_dark = true;
    }                           // end for - rows
}

void losight(FixedArray < unsigned int, 19, 19 > &sh,
             FixedArray < unsigned char, 80, 70 > &gr, int x_p, int y_p)
{
    int o;

    for (o = 0; o < 8; o++)
        los_octant(o, sh, gr, x_p, y_p);
}


void draw_border(void)
{
    textcolor( BORDER_COLOR );
    clrscr();
    redraw_skill( you.your_name, player_title() );

    gotoxy(40, 2);
    cprintf( "%s %s", species_name( you.species, you.experience_level ), 
                     (you.wizard ? "*WIZARD*" : "" ) );

    gotoxy(40,  3); cprintf("HP:");
    gotoxy(40,  4); cprintf("Magic:");
    gotoxy(40,  5); cprintf("AC:");
    gotoxy(40,  6); cprintf("EV:");
    gotoxy(40,  7); cprintf("Str:");
    gotoxy(40,  8); cprintf("Int:");
    gotoxy(40,  9); cprintf("Dex:");
    gotoxy(40, 10); cprintf("Gold:");
    gotoxy(40, 11); cprintf("Experience:");
    gotoxy(40, 12); cprintf("Level");
}                               // end draw_border()

// Determines if the given feature is present at (x, y) in _grid_ coordinates.
// If you have map coords, add (1, 1) to get grid coords.
// Use one of
// 1. '<' and '>' to look for stairs
// 2. '\t' or '\\' for shops, portals.
// 3. '^' for traps
// 4. '_' for altars
// 5. Anything else will look for the exact same character in the level map.
bool is_feature(int feature, int x, int y) {
    unsigned char envfeat = (unsigned char) env.map[x - 1][y - 1];
    if (!envfeat)
        return false;

    // 'grid' can fit in an unsigned char, but making this a short shuts up
    // warnings about out-of-range case values.
    short grid = grd[x][y];

    switch (feature) {
    case 'X':
        return (point_distance[x][y] == PD_EXCLUDED);
    case 'F':
    case 'W':
        return is_waypoint(x, y);
#ifdef STASH_TRACKING
    case 'I':
        return is_stash(x, y);
#endif
    case '_':
        switch (grid) {
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
            return true;
        default:
            return false;
        }
    case '\t':
    case '\\':
        switch (grid) {
        case DNGN_ENTER_HELL:
        case DNGN_ENTER_LABYRINTH:
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
        switch (grid) {
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
            return true;
        default:
            return false;
        }
    case '>':
        switch (grid) {
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
            return true;
        default:
            return false;
        }
    case '^':
        switch (grid) {
        case DNGN_TRAP_MECHANICAL:
        case DNGN_TRAP_MAGICAL:
        case DNGN_TRAP_III:
            return true;
        default:
            return false;
        }
    default: 
        return envfeat == feature;
    }
}

static int find_feature(unsigned char feature, int curs_x, int curs_y, 
                         int start_x, int start_y, int anchor_x, int anchor_y,
                         int ignore_count, char *move_x, char *move_y) {
    int cx = anchor_x,
        cy = anchor_y;

    int firstx = -1, firsty = -1;
    int matchcount = 0;

    // Find the first occurrence of feature 'feature', spiralling around (x,y)
    int maxradius = GXM > GYM? GXM : GYM;
    for (int radius = 1; radius < maxradius; ++radius) {
        for (int axis = -2; axis < 2; ++axis) {
            int rad = radius - (axis < 0);
            for (int var = -rad; var <= rad; ++var) {
                int dx = radius, dy = var;
                if (axis % 2)
                    dx = -dx;
                if (axis < 0) {
                    int temp = dx;
                    dx = dy;
                    dy = temp;
                }

                int x = cx + dx, y = cy + dy;
                if (x < 0 || y < 0 || x >= GXM || y >= GYM) continue;
                if (is_feature(feature, x + 1, y + 1)) {
                    ++matchcount;
                    if (!ignore_count--) {
                        // We want to cursor to (x,y)
                        *move_x = x - (start_x + curs_x - 1);
                        *move_y = y - (start_y + curs_y - 1);
                        return matchcount;
                    }
                    else if (firstx == -1) {
                        firstx = x;
                        firsty = y;
                    }
                }
            }
        }
    }

    // We found something, but ignored it because of an ignorecount
    if (firstx != -1) {
        *move_x = firstx - (start_x + curs_x - 1);
        *move_y = firsty - (start_y + curs_y - 1);
        return 1;
    }
    return 0;
}

void find_features(const std::vector<coord_def>& features,
        unsigned char feature, std::vector<coord_def> *found) {
    for (unsigned feat = 0; feat < features.size(); ++feat) {
        const coord_def& coord = features[feat];
        if (is_feature(feature, coord.x, coord.y))
            found->push_back(coord);
    }
}

static int find_feature( const std::vector<coord_def>& features,
                         unsigned char feature, int curs_x, int curs_y, 
                         int start_x, int start_y, 
                         int ignore_count, char *move_x, char *move_y) {
    int firstx = -1, firsty = -1;
    int matchcount = 0;

    for (unsigned feat = 0; feat < features.size(); ++feat) {
        const coord_def& coord = features[feat];

        if (is_feature(feature, coord.x, coord.y)) {
            ++matchcount;
            if (!ignore_count--) {
                // We want to cursor to (x,y)
                *move_x = coord.x - (start_x + curs_x);
                *move_y = coord.y - (start_y + curs_y);
                return matchcount;
            }
            else if (firstx == -1) {
                firstx = coord.x;
                firsty = coord.y;
            }
        }
    }

    // We found something, but ignored it because of an ignorecount
    if (firstx != -1) {
        *move_x = firstx - (start_x + curs_x);
        *move_y = firsty - (start_y + curs_y);
        return 1;
    }
    return 0;
}

// show_map() now centers the known map along x or y.  This prevents
// the player from getting "artificial" location clues by using the
// map to see how close to the end they are.  They'll need to explore
// to get that.  This function is still a mess, though. -- bwr
void show_map( FixedVector<int, 2> &spec_place, bool travel_mode )
{
    int i, j;

    int bufcount2 = 0;

    char move_x = 0;
    char move_y = 0;
    char getty = 0;

#ifdef DOS_TERM
    char buffer[4800];
#endif

    // Vector to track all features we can travel to, in order of distance.
    std::vector<coord_def> features;
    if (travel_mode)
    {
        travel_cache.update();

        find_travel_pos(you.x_pos, you.y_pos, NULL, NULL, &features);
        // Sort features into the order the player is likely to prefer.
        arrange_features(features);
    }

    // buffer2[GYM * GXM * 2] segfaults my box {dlb}
    screen_buffer_t buffer2[GYM * GXM * 2];        

    char min_x = 80, max_x = 0, min_y = 0, max_y = 0;
    bool found_y = false;

    const int num_lines = get_number_of_lines();
    const int half_screen = num_lines / 2 - 1;

    for (j = 0; j < GYM; j++)
    {
        for (i = 0; i < GXM; i++)
        {
            if (env.map[i][j])
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
    }

    const int map_lines = max_y - min_y + 1;

    const int start_x = min_x + (max_x - min_x + 1) / 2 - 40; // no x scrolling
    const int block_step = Options.level_map_cursor_step;
    int start_y;                                              // y does scroll

    int screen_y = you.y_pos;

    // if close to top of known map, put min_y on top
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

    int curs_x = you.x_pos - start_x;
    int curs_y = you.y_pos - screen_y + half_screen;
    int search_feat = 0, search_found = 0, anchor_x = -1, anchor_y = -1;

#ifdef DOS_TERM
    gettext(1, 1, 80, 25, buffer);
    window(1, 1, 80, 25);
#endif

    clrscr();
    textcolor(DARKGREY);

  put_screen:
    bufcount2 = 0;

    _setcursortype(_NOCURSOR);

#ifdef PLAIN_TERM
    gotoxy(1, 1);
#endif

    start_y = screen_y - half_screen;

    for (j = 0; j < num_lines; j++)
    {
        for (i = 0; i < 80; i++)
        {
            screen_buffer_t colour = DARKGREY;
            if (start_y + j >= 65 || start_y + j <= 3 
                || start_x + i < 0 || start_x + i >= GXM - 1)
            {
                buffer2[bufcount2 + 1] = DARKGREY;
                buffer2[bufcount2] = 0;
                bufcount2 += 2;

#ifdef PLAIN_TERM
                goto print_it;
#endif

#ifdef DOS_TERM
                continue;
#endif

            }

            colour = colour_code_map(start_x + i, start_y + j,
                            Options.item_colour, 
                            travel_mode && Options.travel_colour);

            buffer2[bufcount2 + 1] = colour;
            buffer2[bufcount2] = 
                        (unsigned char) env.map[start_x + i][start_y + j];
            
            if (start_x + i + 1 == you.x_pos && start_y + j + 1 == you.y_pos)
            {
                // [dshaligram] Draw the @ symbol on the level-map. It's no
                // longer saved into the env.map, so we need to draw it 
                // directly.
                buffer2[bufcount2 + 1] = WHITE;
                buffer2[bufcount2]     = you.symbol;
            }

            // If we've a waypoint on the current square, *and* the square is
            // a normal floor square with nothing on it, show the waypoint
            // number.
            if (Options.show_waypoints)
            {
                // XXX: This is a horrible hack.
                screen_buffer_t &bc = buffer2[bufcount2];
                int gridx = start_x + i + 1, gridy = start_y + j + 1;
                unsigned char ch = is_waypoint(gridx, gridy);
                if (ch && (bc == get_sightmap_char(DNGN_FLOOR) ||
                           bc == get_magicmap_char(DNGN_FLOOR)))
                    bc = ch;
            }
            
            bufcount2 += 2;

#ifdef PLAIN_TERM

          print_it:
            // avoid line wrap
            if (i == 79)
                continue;

            // newline
            if (i == 0 && j > 0)
                gotoxy( 1, j + 1 );

            textcolor( buffer2[bufcount2 - 1] );
            putch( buffer2[bufcount2 - 2] );
#endif
        }
    }

#ifdef DOS_TERM
    puttext(1, 1, 80, 25, buffer2);
#endif

    _setcursortype(_NORMALCURSOR);
    gotoxy(curs_x, curs_y);

  gettything:
    getty = getchm(KC_LEVELMAP);

    if (travel_mode && getty != 0 && getty != '+' && getty != '-'
        && getty != 'h' && getty != 'j' && getty != 'k' && getty != 'l'
        && getty != 'y' && getty != 'u' && getty != 'b' && getty != 'n'
        && getty != 'H' && getty != 'J' && getty != 'K' && getty != 'L'
        && getty != 'Y' && getty != 'U' && getty != 'B' && getty != 'N'
        // Keystrokes to initiate travel
        && getty != ',' && getty != '.' && getty != '\r' && getty != ';'

        // Keystrokes for jumping to features
        && getty != '<' && getty != '>' && getty != '@' && getty != '\t'
        && getty != '^' && getty != '_'
        && (getty < '0' || getty > '9')
        && getty != CONTROL('X')
        && getty != CONTROL('E')
        && getty != CONTROL('F')
        && getty != CONTROL('W')
        && getty != CONTROL('C')
        && getty != 'X' && getty != 'F' && getty != 'I' && getty != 'W')
    {
        goto putty;
    }

    if (!travel_mode && getty != 0 && getty != '+' && getty != '-'
        && getty != 'h' && getty != 'j' && getty != 'k' && getty != 'l'
        && getty != 'y' && getty != 'u' && getty != 'b' && getty != 'n'
        && getty != 'H' && getty != 'J' && getty != 'K' && getty != 'L'
        && getty != 'Y' && getty != 'U' && getty != 'B' && getty != 'N'
        && getty != '.' && getty != 'S' && (getty < '0' || getty > '9')
        // Keystrokes for jumping to features
        && getty != '<' && getty != '>' && getty != '@' && getty != '\t'
        && getty != '^' && getty != '_')
    {
        goto gettything;
    }

    if (getty == 0)
    {
        getty = getchm(KC_LEVELMAP);
        // [dshaligram] DOS madness.
        getty = dos_direction_unmunge(getty);
    }

#if defined(WIN32CONSOLE) || defined(DOS)
    // Translate shifted numpad to shifted vi keys. Yes,
    // this is horribly hacky.
    {
        static int win_keypad[] = { 'B', 'J', 'N', 
                                    'H', '5', 'L',
                                    'Y', 'K', 'U' };
        if (getty >= '1' && getty <= '9')
            getty = win_keypad[ getty - '1' ];
    }
#endif

    switch (getty)
    {
    case CONTROL('C'):
        clear_map();
        break;

    case CONTROL('F'):
    case CONTROL('W'):
        travel_cache.add_waypoint(start_x + curs_x, start_y + curs_y);
        // We need to do this all over again so that the user can jump
        // to the waypoint he just created.
        features.clear();
        find_travel_pos(you.x_pos, you.y_pos, NULL, NULL, &features);
        // Sort features into the order the player is likely to prefer.
        arrange_features(features);
        move_x = move_y = 0;
        break;
    case CONTROL('E'):
    case CONTROL('X'):
        {
            int x = start_x + curs_x, y = start_y + curs_y;
            if (getty == CONTROL('X'))
                toggle_exclude(x, y);
            else
                clear_excludes();

            // We now need to redo travel colours
            features.clear();
            find_travel_pos(you.x_pos, you.y_pos, NULL, NULL, &features);
            // Sort features into the order the player is likely to prefer.
            arrange_features(features);

            move_x = move_y = 0;
        }
        break;
        
    case 'b':
    case '1':
        move_x = -1;
        move_y = 1;
        break;

    case 'j':
    case '2':
        move_y = 1;
        move_x = 0;
        break;

    case 'u':
    case '9':
        move_x = 1;
        move_y = -1;
        break;

    case 'k':
    case '8':
        move_y = -1;
        move_x = 0;
        break;

    case 'y':
    case '7':
        move_y = -1;
        move_x = -1;
        break;

    case 'h':
    case '4':
        move_x = -1;
        move_y = 0;
        break;

    case 'n':
    case '3':
        move_y = 1;
        move_x = 1;
        break;

    case 'l':
    case '6':
        move_x = 1;
        move_y = 0;
        break;

    case 'B':
        move_x = -block_step;
        move_y = block_step;
        break;

    case 'J':
        move_y = block_step;
        move_x = 0;
        break;

    case 'U':
        move_x = block_step;
        move_y = -block_step;
        break;

    case 'K':
        move_y = -block_step;
        move_x = 0;
        break;

    case 'Y':
        move_y = -block_step;
        move_x = -block_step;
        break;

    case 'H':
        move_x = -block_step;
        move_y = 0;
        break;

    case 'N':
        move_y = block_step;
        move_x = block_step;
        break;

    case 'L':
        move_x = block_step;
        move_y = 0;
        break;

    case '+':
        move_y = 20;
        move_x = 0;
        break;
    case '-':
        move_y = -20;
        move_x = 0;
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
        move_x = 0;
        move_y = 0;
        if (anchor_x == -1) {
            anchor_x = start_x + curs_x - 1;
            anchor_y = start_y + curs_y - 1;
        }
        if (search_feat != getty) {
            search_feat         = getty;
            search_found        = 0;
        }
        if (travel_mode)
            search_found = find_feature(features, getty, curs_x, curs_y, 
                                        start_x, start_y,
                                        search_found, &move_x, &move_y);
        else
            search_found = find_feature(getty, curs_x, curs_y,
                                        start_x, start_y,
                                        anchor_x, anchor_y,
                                        search_found, &move_x, &move_y);
        break;
    case '.':
    case '\r':
    case 'S':
    case ',':
    case ';':
    {
        int x = start_x + curs_x, y = start_y + curs_y;
        if (travel_mode && x == you.x_pos && y == you.y_pos)
        {
            if (you.travel_x > 0 && you.travel_y > 0) {
                move_x = you.travel_x - x;
                move_y = you.travel_y - y;
            }
            break;
        }
        else {
            spec_place[0] = x;
            spec_place[1] = y;
            goto putty;
        }
    }
    default:
        move_x = 0;
        move_y = 0;
        break;
    }

    if (curs_x + move_x < 1 || curs_x + move_x > 80)
        move_x = 0;

    curs_x += move_x;

    if (num_lines < map_lines)
    {
        // Scrolling only happens when we don't have a large enough 
        // display to show the known map.
        if (getty == '-' || getty == '+')
        {
            if (getty == '-')
                screen_y -= 20;

            if (screen_y <= min_y + half_screen)
                screen_y = min_y + half_screen;

            if (getty == '+')
                screen_y += 20;

            if (screen_y >= max_y - half_screen)
                screen_y = max_y - half_screen;

            goto put_screen;
        }

        if (curs_y + move_y < 1)
        {
            // screen_y += (curs_y + move_y) - 1;
            screen_y += move_y;

            if (screen_y < min_y + half_screen) {
                move_y   = screen_y - (min_y + half_screen);
                screen_y = min_y + half_screen;
            }
            else
                move_y = 0;
        }

        if (curs_y + move_y > num_lines - 1)
        {
            // screen_y += (curs_y + move_y) - num_lines + 1;
            screen_y += move_y;

            if (screen_y > max_y - half_screen) {
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
    goto put_screen;

  putty:

#ifdef DOS_TERM
    puttext(1, 1, 80, 25, buffer);
#endif

    return;
}                               // end show_map()


void magic_mapping(int map_radius, int proportion)
{
    int i, j, k, l, empty_count;

    if (map_radius > 50)
        map_radius = 50;
    else if (map_radius < 5)
        map_radius = 5;

    // now gradually weaker with distance:
    const int pfar = (map_radius * 7) / 10;
    const int very_far = (map_radius * 9) / 10;

    for (i = you.x_pos - map_radius; i < you.x_pos + map_radius; i++)
    {
        for (j = you.y_pos - map_radius; j < you.y_pos + map_radius; j++)
        {
            if (proportion < 100 && random2(100) >= proportion)
                continue;       // note that proportion can be over 100

            if (!map_bounds(i, j))
                continue;

            const int dist = grid_distance( you.x_pos, you.y_pos, i, j );

            if (dist > pfar && one_chance_in(3))
                continue;

            if (dist > very_far && coinflip())
                continue;

            if (is_terrain_changed(i, j))
                clear_envmap_grid(i, j);

            if (is_terrain_known(i, j))
                continue;

            empty_count = 8;

            if (grid_is_solid(grd[i][j]) && grd[i][j] != DNGN_CLOSED_DOOR)
            {
                for (k = -1; k <= 1; k++)
                {
                    for (l = -1; l <= 1; l++)
                    {
                        if (k == 0 && l == 0)
                            continue;

                        if (!map_bounds( i + k, j + l ))
                        {
                            --empty_count;
                            continue;
                        }

                        if (grid_is_solid( grd[i + k][j + l] )
                                && grd[i + k][j + l] != DNGN_CLOSED_DOOR)
                            empty_count--;
                    }
                }
            }

            if (empty_count > 0)
            {
                if (!get_envmap_char(i, j))
                    set_envmap_char(i, j, get_magicmap_char(grd[i][j]));

                // Hack to give demonspawn Pandemonium mutation the ability
                // to detect exits magically.
                if (you.mutation[MUT_PANDEMONIUM] > 1
                        && grd[i][j] == DNGN_EXIT_PANDEMONIUM)
                    set_terrain_seen( i, j );
                else
                    set_terrain_mapped( i, j );
            }
        }
    }
}                               // end magic_mapping()

// realize that this is simply a repackaged version of
// stuff::see_grid() -- make certain they correlate {dlb}:
bool mons_near(struct monsters *monster, unsigned int foe)
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
    struct monsters *myFoe = &menv[foe];
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

void init_char_table( char_set_type set )
{
    const unsigned char table[ NUM_CSET ][ NUM_DCHAR_TYPES ] = 
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
    };

    for (int i = 0; i < NUM_DCHAR_TYPES; i++)
        Options.char_table[i] = table[set][i];
}

void init_feature_table( void )
{
    for (int i = 0; i < NUM_FEATURES; i++)
    {
        Feature[i].symbol = 0;
        Feature[i].colour = BLACK;      // means must be set some other way
        Feature[i].notable = false;
        Feature[i].seen_effect = false;
        Feature[i].magic_symbol = 0;    // made equal to symbol if untouched
        Feature[i].map_colour = DARKGREY;
        Feature[i].seen_colour = BLACK;    // marks no special seen map handling

        switch (i)
        {
        case DNGN_UNSEEN:
        default:
            break;

        case DNGN_ROCK_WALL:
        case DNGN_PERMAROCK_WALL:
            Feature[i].symbol = Options.char_table[ DCHAR_WALL ];
            Feature[i].colour = EC_ROCK;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            break;

        case DNGN_STONE_WALL:
            Feature[i].symbol = Options.char_table[ DCHAR_WALL ];
            Feature[i].colour = EC_STONE;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            break;

        case DNGN_OPEN_DOOR:
            Feature[i].symbol = Options.char_table[ DCHAR_DOOR_OPEN ];
            Feature[i].colour = LIGHTGREY;
            break;

        case DNGN_CLOSED_DOOR:
            Feature[i].symbol = Options.char_table[ DCHAR_DOOR_CLOSED ];
            Feature[i].colour = LIGHTGREY;
            break;

        case DNGN_METAL_WALL:
            Feature[i].symbol = Options.char_table[ DCHAR_WALL ];
            Feature[i].colour = CYAN;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            break;

        case DNGN_SECRET_DOOR:
            // Note: get_secret_door_appearance means this probably isn't used
            Feature[i].symbol = Options.char_table[ DCHAR_WALL ];
            Feature[i].colour = EC_ROCK;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            break;

        case DNGN_GREEN_CRYSTAL_WALL:
            Feature[i].symbol = Options.char_table[ DCHAR_WALL ];
            Feature[i].colour = GREEN;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            break;

        case DNGN_ORCISH_IDOL:
            Feature[i].symbol = Options.char_table[ DCHAR_STATUE ];
            Feature[i].colour = DARKGREY;
            break;

        case DNGN_WAX_WALL:
            Feature[i].symbol = Options.char_table[ DCHAR_WALL ];
            Feature[i].colour = YELLOW;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            break;                  // wax wall

        case DNGN_SILVER_STATUE:
            Feature[i].symbol = Options.char_table[ DCHAR_STATUE ];
            Feature[i].colour = WHITE;
            Feature[i].seen_effect = true;
            break;

        case DNGN_GRANITE_STATUE:
            Feature[i].symbol = Options.char_table[ DCHAR_STATUE ];
            Feature[i].colour = LIGHTGREY;
            break;

        case DNGN_ORANGE_CRYSTAL_STATUE:
            Feature[i].symbol = Options.char_table[ DCHAR_STATUE ];
            Feature[i].colour = LIGHTRED;
            Feature[i].seen_effect = true;
            break;

        case DNGN_LAVA:
            Feature[i].symbol = Options.char_table[ DCHAR_WAVY ];
            Feature[i].colour = RED;
            break;

        case DNGN_DEEP_WATER:
            Feature[i].symbol = Options.char_table[ DCHAR_WAVY ];
            Feature[i].colour = BLUE;
            break;

        case DNGN_SHALLOW_WATER:
            Feature[i].symbol = Options.char_table[ DCHAR_WAVY ];
            Feature[i].colour = CYAN;
            break;

        case DNGN_FLOOR:
            Feature[i].symbol = Options.char_table[ DCHAR_FLOOR ];
            Feature[i].colour = EC_FLOOR;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_FLOOR_MAGIC ];
            break;

        case DNGN_EXIT_HELL:
            Feature[i].symbol = Options.char_table[ DCHAR_ARCH ];
            Feature[i].colour = LIGHTRED;
            Feature[i].notable = false;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = LIGHTRED;
            break;

        case DNGN_ENTER_HELL:
            Feature[i].symbol = Options.char_table[ DCHAR_ARCH ];
            Feature[i].colour = RED;
            Feature[i].notable = true;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = RED;
            break;

        case DNGN_TRAP_MECHANICAL:
            Feature[i].colour = LIGHTCYAN;
            Feature[i].symbol = Options.char_table[ DCHAR_TRAP ];
            Feature[i].map_colour = LIGHTCYAN;
            break;

        case DNGN_TRAP_MAGICAL:
            Feature[i].colour = MAGENTA;
            Feature[i].symbol = Options.char_table[ DCHAR_TRAP ];
            Feature[i].map_colour = MAGENTA;
            break;

        case DNGN_TRAP_III:
            Feature[i].colour = LIGHTGREY;
            Feature[i].symbol = Options.char_table[ DCHAR_TRAP ];
            Feature[i].map_colour = LIGHTGREY;
            break;

        case DNGN_UNDISCOVERED_TRAP:
            Feature[i].symbol = Options.char_table[ DCHAR_FLOOR ];
            Feature[i].colour = EC_FLOOR;
            Feature[i].magic_symbol = Options.char_table[ DCHAR_FLOOR_MAGIC ];
            break;

        case DNGN_ENTER_SHOP:
            Feature[i].symbol = Options.char_table[ DCHAR_ARCH ];
            Feature[i].colour = YELLOW;
            Feature[i].notable = true;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = YELLOW;
            break;

        case DNGN_ENTER_LABYRINTH:
            Feature[i].symbol = Options.char_table[ DCHAR_ARCH ];
            Feature[i].colour = CYAN;
            Feature[i].notable = true;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = CYAN;
            break;

        case DNGN_ROCK_STAIRS_DOWN:
        case DNGN_STONE_STAIRS_DOWN_I:
        case DNGN_STONE_STAIRS_DOWN_II:
        case DNGN_STONE_STAIRS_DOWN_III:
            Feature[i].symbol = Options.char_table[ DCHAR_STAIRS_DOWN ];
            Feature[i].colour = ((i == DNGN_ROCK_STAIRS_DOWN) ? BROWN 
                                                              : LIGHTGREY);
            Feature[i].map_colour = RED;
            break;

        case DNGN_ROCK_STAIRS_UP:
        case DNGN_STONE_STAIRS_UP_I:
        case DNGN_STONE_STAIRS_UP_II:
        case DNGN_STONE_STAIRS_UP_III:
            Feature[i].symbol = Options.char_table[ DCHAR_STAIRS_UP ];
            Feature[i].colour = ((i == DNGN_ROCK_STAIRS_UP) ? BROWN 
                                                            : LIGHTGREY);
            Feature[i].map_colour = GREEN;
            break;

        case DNGN_ENTER_DIS:
            Feature[i].colour = CYAN;
            Feature[i].symbol = Options.char_table[ DCHAR_ARCH ];
            Feature[i].notable = true;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = CYAN;
            break;

        case DNGN_ENTER_GEHENNA:
            Feature[i].colour = RED;
            Feature[i].symbol = Options.char_table[ DCHAR_ARCH ];
            Feature[i].notable = true;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = RED;
            break;

        case DNGN_ENTER_COCYTUS:
            Feature[i].colour = LIGHTCYAN;
            Feature[i].symbol = Options.char_table[ DCHAR_ARCH ];
            Feature[i].notable = true;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = LIGHTCYAN;
            break;

        case DNGN_ENTER_TARTARUS:
            Feature[i].colour = DARKGREY;
            Feature[i].symbol = Options.char_table[ DCHAR_ARCH ];
            Feature[i].notable = true;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = DARKGREY;
            break;

        case DNGN_ENTER_ABYSS:
            Feature[i].colour = EC_RANDOM;
            Feature[i].symbol = Options.char_table[ DCHAR_ARCH ];
            Feature[i].notable = true;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = EC_RANDOM;
            break;

        case DNGN_EXIT_ABYSS:
            Feature[i].colour = EC_RANDOM;
            Feature[i].symbol = Options.char_table[ DCHAR_ARCH ];
            Feature[i].map_colour = EC_RANDOM;
            break;

        case DNGN_STONE_ARCH:
            Feature[i].colour = LIGHTGREY;
            Feature[i].symbol = Options.char_table[ DCHAR_ARCH ];
            Feature[i].map_colour = LIGHTGREY;
            break;

        case DNGN_ENTER_PANDEMONIUM:
            Feature[i].colour = LIGHTBLUE;
            Feature[i].symbol = Options.char_table[ DCHAR_ARCH ];
            Feature[i].notable = true;
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = LIGHTBLUE;
            break;

        case DNGN_EXIT_PANDEMONIUM:
            // Note: has special handling for colouring with mutation
            Feature[i].colour = LIGHTBLUE;
            Feature[i].symbol = Options.char_table[ DCHAR_ARCH ];
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = LIGHTBLUE;
            break;

        case DNGN_TRANSIT_PANDEMONIUM:
            Feature[i].colour = LIGHTGREEN;
            Feature[i].symbol = Options.char_table[ DCHAR_ARCH ];
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
        case DNGN_ENTER_RESERVED_1:
        case DNGN_ENTER_RESERVED_2:
        case DNGN_ENTER_RESERVED_3:
        case DNGN_ENTER_RESERVED_4:
            Feature[i].colour = YELLOW;
            Feature[i].symbol = Options.char_table[ DCHAR_STAIRS_DOWN ];
            Feature[i].notable = true;
            Feature[i].map_colour = RED;
            Feature[i].seen_colour = LIGHTRED;
            break;

        case DNGN_ENTER_ZOT:
            Feature[i].colour = MAGENTA;
            Feature[i].symbol = Options.char_table[ DCHAR_ARCH ];
            Feature[i].notable = true;
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
        case DNGN_RETURN_RESERVED_1:
        case DNGN_RETURN_RESERVED_2:
        case DNGN_RETURN_RESERVED_3:
        case DNGN_RETURN_RESERVED_4:
            Feature[i].colour = YELLOW;
            Feature[i].symbol = Options.char_table[ DCHAR_STAIRS_UP ];
            Feature[i].map_colour = BLUE;
            Feature[i].seen_colour = LIGHTBLUE;
            break;

        case DNGN_RETURN_FROM_ZOT:
            Feature[i].colour = MAGENTA;
            Feature[i].symbol = Options.char_table[ DCHAR_ARCH ];
            Feature[i].map_colour = LIGHTGREY;
            Feature[i].seen_colour = MAGENTA;
            break;

        case DNGN_ALTAR_ZIN:
            Feature[i].colour = WHITE;
            Feature[i].symbol = Options.char_table[ DCHAR_ALTAR ];
            Feature[i].notable = true;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = WHITE;
            break;

        case DNGN_ALTAR_SHINING_ONE:
            Feature[i].colour = YELLOW;
            Feature[i].symbol = Options.char_table[ DCHAR_ALTAR ];
            Feature[i].notable = true;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = YELLOW;
            break;

        case DNGN_ALTAR_KIKUBAAQUDGHA:
            Feature[i].colour = DARKGREY;
            Feature[i].symbol = Options.char_table[ DCHAR_ALTAR ];
            Feature[i].notable = true;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = DARKGREY;
            break;

        case DNGN_ALTAR_YREDELEMNUL:
            Feature[i].colour = EC_UNHOLY;
            Feature[i].symbol = Options.char_table[ DCHAR_ALTAR ];
            Feature[i].notable = true;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = EC_UNHOLY;
            break;

        case DNGN_ALTAR_XOM:
            Feature[i].colour = EC_RANDOM;
            Feature[i].symbol = Options.char_table[ DCHAR_ALTAR ];
            Feature[i].notable = true;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = EC_RANDOM;
            break;

        case DNGN_ALTAR_VEHUMET:
            Feature[i].colour = EC_VEHUMET;
            Feature[i].symbol = Options.char_table[ DCHAR_ALTAR ];
            Feature[i].notable = true;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = EC_VEHUMET;
            break;

        case DNGN_ALTAR_OKAWARU:
            Feature[i].colour = CYAN;
            Feature[i].symbol = Options.char_table[ DCHAR_ALTAR ];
            Feature[i].notable = true;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = CYAN;
            break;

        case DNGN_ALTAR_MAKHLEB:
            Feature[i].colour = EC_FIRE;
            Feature[i].symbol = Options.char_table[ DCHAR_ALTAR ];
            Feature[i].notable = true;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = EC_FIRE;
            break;

        case DNGN_ALTAR_SIF_MUNA:
            Feature[i].colour = BLUE;
            Feature[i].symbol = Options.char_table[ DCHAR_ALTAR ];
            Feature[i].notable = true;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = BLUE;
            break;

        case DNGN_ALTAR_TROG:
            Feature[i].colour = RED;
            Feature[i].symbol = Options.char_table[ DCHAR_ALTAR ];
            Feature[i].notable = true;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = RED;
            break;

        case DNGN_ALTAR_NEMELEX_XOBEH:
            Feature[i].colour = LIGHTMAGENTA;
            Feature[i].symbol = Options.char_table[ DCHAR_ALTAR ];
            Feature[i].notable = true;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = LIGHTMAGENTA;
            break;

        case DNGN_ALTAR_ELYVILON:
            Feature[i].colour = LIGHTGREY;
            Feature[i].symbol = Options.char_table[ DCHAR_ALTAR ];
            Feature[i].notable = true;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = LIGHTGREY;
            break;

        case DNGN_BLUE_FOUNTAIN:
            Feature[i].colour = BLUE;
            Feature[i].symbol = Options.char_table[ DCHAR_FOUNTAIN ];
            break;

        case DNGN_SPARKLING_FOUNTAIN:
            Feature[i].colour = LIGHTBLUE;
            Feature[i].symbol = Options.char_table[ DCHAR_FOUNTAIN ];
            break;

        case DNGN_DRY_FOUNTAIN_I:
        case DNGN_DRY_FOUNTAIN_II:
        case DNGN_PERMADRY_FOUNTAIN:
            Feature[i].colour = LIGHTGREY;
            Feature[i].symbol = Options.char_table[ DCHAR_FOUNTAIN ];
            break;

        case DNGN_INVIS_EXPOSED:
            Feature[i].symbol = Options.char_table[ DCHAR_INVIS_EXPOSED ];
            break;

        case DNGN_ITEM_DETECTED:
            Feature[i].magic_symbol = Options.char_table[ DCHAR_ITEM_DETECTED ];
            break;

        case DNGN_ITEM_ORB:
            Feature[i].symbol = Options.char_table[ DCHAR_ITEM_ORB ];
            break;

        case DNGN_ITEM_WEAPON:
            Feature[i].symbol = Options.char_table[ DCHAR_ITEM_WEAPON ];
            break;

        case DNGN_ITEM_ARMOUR:
            Feature[i].symbol = Options.char_table[ DCHAR_ITEM_ARMOUR ];
            break;

        case DNGN_ITEM_WAND:
            Feature[i].symbol = Options.char_table[ DCHAR_ITEM_WAND ];
            break;

        case DNGN_ITEM_FOOD:
            Feature[i].symbol = Options.char_table[ DCHAR_ITEM_FOOD ];
            break;

        case DNGN_ITEM_SCROLL:
            Feature[i].symbol = Options.char_table[ DCHAR_ITEM_SCROLL ];
            break;

        case DNGN_ITEM_RING:
            Feature[i].symbol = Options.char_table[ DCHAR_ITEM_RING ];
            break;

        case DNGN_ITEM_POTION:
            Feature[i].symbol = Options.char_table[ DCHAR_ITEM_POTION ];
            break;

        case DNGN_ITEM_MISSILE:
            Feature[i].symbol = Options.char_table[ DCHAR_ITEM_MISSILE ];
            break;

        case DNGN_ITEM_BOOK:
            Feature[i].symbol = Options.char_table[ DCHAR_ITEM_BOOK ];
            break;

        case DNGN_ITEM_STAVE:
            Feature[i].symbol = Options.char_table[ DCHAR_ITEM_STAVE ];
            break;

        case DNGN_ITEM_MISCELLANY:
            Feature[i].symbol = Options.char_table[ DCHAR_ITEM_MISCELLANY ];
            break;

        case DNGN_ITEM_CORPSE:
            Feature[i].symbol = Options.char_table[ DCHAR_ITEM_CORPSE ];
            break;

        case DNGN_ITEM_GOLD:
            Feature[i].symbol = Options.char_table[ DCHAR_ITEM_GOLD ];
            break;

        case DNGN_ITEM_AMULET:
            Feature[i].symbol = Options.char_table[ DCHAR_ITEM_AMULET ];
            break;

        case DNGN_CLOUD:
            Feature[i].symbol = Options.char_table[ DCHAR_CLOUD ];
            break;
        }

        if (!Feature[i].magic_symbol)
            Feature[i].magic_symbol = Feature[i].symbol;

        if (Feature[i].seen_colour == BLACK)
            Feature[i].seen_colour = Feature[i].map_colour;
    }
}

static int get_screen_glyph( int x, int y ) 
{
    const int ex = x - you.x_pos + 9;
    const int ey = y - you.y_pos + 9;

    int             object = env.show[ex][ey];
    unsigned short  colour = env.show_col[ex][ey];
    unsigned short  ch;

    if (!object)
        return get_envmap_char(x, y);

    if (object == DNGN_SECRET_DOOR)
        object = grid_secret_door_appearance( x, y );

    get_symbol( object, &ch, &colour );
    return (ch);
}

// Returns a string containing an ASCII representation of the map. If fullscreen
// is set to false, only the viewable area is returned. Leading and trailing 
// spaces are trimmed from each line. Leading and trailing empty lines are also
// snipped.
std::string screenshot( bool fullscreen )
{
    UNUSED( fullscreen );

    const int X_SIZE = VIEW_WIDTH;
    const int Y_SIZE = VIEW_HEIGHT;

    // [ds] Screenshots need to be straight ASCII. We will now proceed to force
    // the char and feature tables back to ASCII.
    FixedVector<unsigned char, NUM_DCHAR_TYPES> char_table_bk;
    char_table_bk = Options.char_table;

    init_char_table(CSET_ASCII);
    init_feature_table();
    
    int firstnonspace = -1;
    int firstpopline  = -1;
    int lastpopline   = -1;

    char lines[Y_SIZE][X_SIZE + 1];
    for (int count_y = 0; count_y < Y_SIZE; count_y++)
    {
        int lastnonspace = -1;
        
        for (int count_x = 0; count_x < X_SIZE; count_x++)
        {
            // in grid coords
            const int gx = count_x + you.x_pos - 16;
            const int gy = count_y + you.y_pos - 8;

            int ch = (!map_bounds(gx, gy)) 
                        ? 0 
                        : (count_x < 8 || count_x > 24) 
                            ? get_envmap_char(gx, gy) 
                            : (gx == you.x_pos && gy == you.y_pos)
                                ? you.symbol 
                                : get_screen_glyph(gx, gy);

            if (ch && !isprint(ch)) 
            {
                // [ds] Evil hack time again. Peek at grid, use that character.
                int object = grd[gx][gy];
                unsigned short glych, glycol;

                if (object == DNGN_SECRET_DOOR)
                    object = grid_secret_door_appearance( gx, gy );

                get_symbol( object, &glych, &glycol );
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

            lines[count_y][count_x] = ch;
        }

        lines[count_y][lastnonspace + 1] = 0;
    }

    // Restore char and feature tables
    Options.char_table = char_table_bk;
    init_feature_table();

    std::string ss;
    if (firstpopline != -1 && lastpopline != -1) 
    {
        if (firstnonspace == -1)
            firstnonspace = 0;

        for (int i = firstpopline; i <= lastpopline; ++i) 
        {
            char *curr = lines[i];

            while (*curr && curr - lines[i] < firstnonspace)
                curr++;

            ss += curr;
            ss += EOL;
        }
    }

    return (ss);
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
    const int X_SIZE = VIEW_WIDTH;
    const int Y_SIZE = VIEW_HEIGHT;
    const int BUFFER_SIZE = 1550;

    FixedVector < screen_buffer_t, BUFFER_SIZE > buffy;
    int count_x, count_y;

    losight( env.show, grd, you.x_pos, you.y_pos ); // must be done first

    for (count_x = 0; count_x < NUM_STATUE_TYPES; count_x++)
        you.visible_statue[count_x] = 0;

    for (count_x = 0; count_x < 18; count_x++)
    {
        for (count_y = 0; count_y < 18; count_y++)
        {
            env.show_col[count_x][count_y] = LIGHTGREY;
            Show_Backup[count_x][count_y] = 0;
        }
    }

    item_grid();                // must be done before cloud and monster
    cloud_grid();
    monster_grid( do_updates );

    if (draw_it)
    {
        _setcursortype(_NOCURSOR);

        const bool map = player_in_mappable_area();
        int bufcount = 0;

        for (count_y = 0; count_y < Y_SIZE; count_y++)
        {
            for (count_x = 0; count_x < X_SIZE; count_x++)
            {
                // in grid coords
                const int gx = count_x + you.x_pos - 16;
                const int gy = count_y + you.y_pos - 8;

                // order is important here
                if (!map_bounds( gx, gy ))
                {
                    // off the map
                    buffy[bufcount] = 0;
                    buffy[bufcount + 1] = DARKGREY;
                }
                else if (count_x < 8 || count_x > 24)
                {
                    // outside the env.show area
                    buffy[bufcount] = get_envmap_char( gx, gy );
                    buffy[bufcount + 1] = DARKGREY;

                    if (Options.colour_map)
                        buffy[bufcount + 1] = 
                            colour_code_map(gx - 1, gy - 1, 
                                            Options.item_colour);
                }
                else if (gx == you.x_pos && gy == you.y_pos)
                {
                    // player overrides everything in cell
                    buffy[bufcount] = you.symbol;
                    buffy[bufcount + 1] = you.colour;

                    if (player_is_swimming())
                    {
                        if (grd[gx][gy] == DNGN_DEEP_WATER)
                            buffy[bufcount + 1] = BLUE;
                        else 
                            buffy[bufcount + 1] = CYAN;
                    }
                }
                else
                {
                    // Note: env.show is set for grids in LoS
                    // get env coords
                    const int ex = gx - you.x_pos + 9;
                    const int ey = gy - you.y_pos + 9;

                    int             object = env.show[ex][ey];
                    unsigned short  colour = env.show_col[ex][ey];
                    unsigned short  ch;

                    if (object == DNGN_SECRET_DOOR)
                        object = grid_secret_door_appearance( gx, gy );

                    get_symbol( object, &ch, &colour );

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
                            set_envmap_char( gx, gy, buffy[bufcount] );
                            set_terrain_seen( gx, gy );
                            set_envmap_detected_mons(gx, gy, false);
                            set_envmap_detected_item(gx, gy, false);
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
                            && Show_Backup[ex][ey]
                            && is_terrain_seen( gx, gy ))
                        {
                            get_symbol( Show_Backup[ex][ey], &ch, &colour );
                            set_envmap_char( gx, gy, ch );
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
                            buffy[bufcount] = get_envmap_char( gx, gy );
                            buffy[bufcount + 1] = DARKGREY;

                            if (Options.colour_map)
                                buffy[bufcount + 1] = 
                                    colour_code_map(gx - 1, gy - 1,
                                                    Options.item_colour);
                        }
                    }
                }
                
                // alter colour if flashing the characters vision
                if (you.flash_colour != BLACK 
                    && buffy[bufcount + 1] != DARKGREY)
                {
                    buffy[bufcount + 1] = you.flash_colour;
                }

                bufcount += 2;
            }
        }

        // Leaving it this way because short flashes can occur in long ones,
        // and this simply works without requiring a stack.
        if (you.flash_colour != BLACK)
        {
            if (you.special_wield == SPWLD_SHADOW)
                you.flash_colour = DARKGREY;
            else if (you.berserker)
                you.flash_colour = RED;
            else
                you.flash_colour = BLACK;
        }

#ifdef DOS_TERM
        puttext( 2, 1, X_SIZE + 1, Y_SIZE, buffy.buffer() );
#endif

#ifdef PLAIN_TERM
        // avoiding unneeded draws when running
        if (!you.running || (you.running < 0 && Options.travel_delay > -1))
        {
            gotoxy( 2, 1 );

            bufcount = 0;
            for (count_y = 0; count_y < Y_SIZE; count_y++)
            {
                for (count_x = 0; count_x < X_SIZE; count_x++)
                {
#ifdef USE_CURSES
                    if (Options.char_set != CSET_ASCII) 
                    {
                        // switch to alternate char set for 8-bit characters:
                        set_altcharset( (buffy[bufcount] > 127) );

                        // shift the DEC line drawing set:
                        if (Options.char_set == CSET_DEC 
                            && buffy[bufcount] >= 0xE0)
                        {
                            buffy[bufcount] &= 0x7F;
                        }
                    }
#endif
                    textcolor( buffy[bufcount + 1] );
                    putch( buffy[bufcount] );
                    bufcount += 2;
                }

                gotoxy( 2, count_y + 2 );
            }
        }

#ifdef USE_CURSES
        set_altcharset( false );
#endif

#endif
        _setcursortype(_NORMALCURSOR);
    }
}                               // end viewwindow()
