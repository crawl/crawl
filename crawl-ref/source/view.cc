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
#include <cmath>
#include <sstream>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "command.h"
#include "cloud.h"
#include "clua.h"
#include "debug.h"
#include "delay.h"
#include "direct.h"
#include "dungeon.h"
#include "initfile.h"
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

#ifdef UNICODE_GLYPHS
typedef unsigned int screen_buffer_t;
#else
typedef unsigned short screen_buffer_t;
#endif

crawl_view_geometry crawl_view;
FixedArray < unsigned int, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER > Show_Backup;

unsigned char show_green;
extern int stealth;             // defined in acr.cc

// char colour_code_map(unsigned char map_value);
screen_buffer_t colour_code_map( int x, int y, bool item_colour = false,
                                 bool travel_colour = false );

void cloud_grid(void);
void monster_grid(bool do_updates);

static void get_symbol( int x, int y,
                        int object, unsigned *ch, 
                        unsigned short *colour,
                        bool magic_mapped = false );
static unsigned get_symbol(int object, unsigned short *colour = NULL,
                           bool magic_mapped = false);

static int get_item_dngn_code(const item_def &item);
static void set_show_backup( int ex, int ey );
static int get_viewobj_flags(int viewobj);

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

static void get_symbol( int x, int y,
                        int object, unsigned *ch, 
                        unsigned short *colour,
                        bool magic_mapped )
{
    ASSERT( ch != NULL );

    if (object < NUM_FEATURES)
    {
        *ch = magic_mapped? Feature[object].magic_symbol
                          : Feature[object].symbol;

        if (colour)
        {
            const int colmask = *colour & COLFLAG_MASK;
            // Don't clobber with BLACK, because the colour should be
            // already set.
            if (Feature[object].colour != BLACK)
                *colour = Feature[object].colour | colmask;
        }

        // Note anything we see that's notable
        if ((x || y) && Feature[object].notable)
            seen_notable_thing( object, x, y );
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
    
    const int grid_value = grd[x][y];

    unsigned tc = travel_colour? 
                        get_travel_colour(x, y)
                      : DARKGREY;
    
    if (map_flags & MAP_DETECTED_ITEM)
        tc = Options.detected_item_colour;
    
    if (map_flags & MAP_DETECTED_MONSTER)
    {
        tc = Options.detected_monster_colour;
        return real_colour(tc);
    }

    // XXX: [ds] If we've an important colour, override other feature
    // colouring. Yes, this is hacky. Story of my life.
    if (tc == LIGHTGREEN || tc == LIGHTMAGENTA)
        return real_colour(tc);

    if (item_colour && is_envmap_item(x, y))
        return get_envmap_col(x, y);

    int feature_colour = DARKGREY;
    feature_colour = 
        is_terrain_seen(x, y)? Feature[grid_value].seen_colour 
                                     : Feature[grid_value].map_colour;

    if (feature_colour != DARKGREY)
        tc = feature_colour;

    if (Options.stair_item_brand
        && is_stair(grid_value) && igrd[x][y] != NON_ITEM)
    {
        tc |= COLFLAG_STAIR_ITEM;
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

static int get_mons_colour(const monsters *mons)
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

    return (col);
}

static std::set<const monsters*> monsters_seen_this_turn;

static bool mons_was_seen_this_turn(const monsters *mons)
{
    return (monsters_seen_this_turn.find(mons) !=
            monsters_seen_this_turn.end());
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
                            std::string msg = "You hear ";
                            switch (mons_shouts(monster->type))
                            {
                            case S_SILENT:
                            default:
                                msg += "buggy behaviour!";
                                break;
                            case S_SHOUT:
                                msg += "a shout!";
                                break;
                            case S_BARK:
                                msg += "a bark!";
                                break;
                            case S_SHOUT2:
                                msg += "two shouts!";
                                noise_level = 12;
                                break;
                            case S_ROAR:
                                msg += "a roar!";
                                noise_level = 12;
                                break;
                            case S_SCREAM:
                                msg += "a hideous shriek!";
                                break;
                            case S_BELLOW:
                                msg += "a bellow!";
                                break;
                            case S_SCREECH:
                                msg += "a screech!";
                                break;
                            case S_BUZZ:
                                msg += "an angry buzzing noise.";
                                break;
                            case S_MOAN:
                                msg += "a chilling moan.";
                                break;
                            case S_WHINE:
                                msg += "an irritating high-pitched whine.";
                                break;
                            case S_CROAK:
                                if (coinflip())
                                    msg += "a loud, deep croak!";
                                else
                                    msg += "a croak.";
                                break;
                            case S_GROWL:
                                msg += "an angry growl!";
                                break;
                            case S_HISS:
                                msg += "an angry hiss!";
                                noise_level = 4;  // not very loud -- bwr
                                break;
                            }

                            mpr(msg.c_str(), MSGCH_SOUND);
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
                    && !mons_flies(monster)
                    && env.cgrid(monster->pos()) == EMPTY_CLOUD)
                {
                    set_show_backup(ex, ey);
                    env.show[ex][ey] = DNGN_INVIS_EXPOSED;
                    env.show_col[ex][ey] = BLUE; 
                }
                continue;
            }

            // mimics are always left on map
            if (!mons_is_mimic( monster->type ))
                set_show_backup(ex, ey);

            if (player_monster_visible(monster)
                && !mons_is_submerged(monster)
                && !mons_friendly(monster)
                && !mons_class_flag(monster->type, M_NO_EXP_GAIN)
                && !mons_is_mimic(monster->type))
            {
                monsters_seen_this_turn.insert(monster);
            }

            env.show[ex][ey] = monster->type + DNGN_START_OF_MONSTERS;
            env.show_col[ex][ey] = get_mons_colour( monster );

            // for followers of Beogh, decide whether orcs will join you
            if (mons_species(monster->type) == MONS_ORC
                && you.religion == GOD_BEOGH
                && !(monster->flags & MF_CONVERT_ATTEMPT)
               // && !mons_is_unique(monster->type) // does not work on Blork
                && monster->foe == MHITYOU
                && mons_player_visible(monster) && !mons_is_sleeping(monster)
                && !mons_is_confused(monster) && !mons_is_paralysed(monster))
            {
                monster->flags |= MF_CONVERT_ATTEMPT;

               int hd = monster->hit_dice;

               if (you.piety >= 75 && !you.penance[GOD_BEOGH] &&
                   random2(you.piety/9) > random2(hd) + hd + random2(5))
               {
                   int wpn = you.equip[EQ_WEAPON];
                   if (wpn != -1
                       && you.inv[wpn].base_type == OBJ_WEAPONS
                       && get_weapon_brand( you.inv[wpn] ) == SPWPN_ORC_SLAYING
                       && coinflip()) // 50% chance of conversion failing
                   {
                       snprintf(info, INFO_SIZE, "%s flinches from your weapon.",
                                monster->name(DESC_CAP_THE).c_str());
                       mpr(info);
                       continue;
                   }
                       
                   if (player_monster_visible(monster)) // show reaction
                   {
                       std::string reaction;
                       
                       switch (random2(3))
                       {
                          case 1: reaction = " stares at you in amazement and kneels.";
                                  break;
                          case 2: reaction = " relaxes his fighting stance and smiles at you.";
                                  break;
                         default: reaction = " falls on his knees before you.";
                       }

                       snprintf(info, INFO_SIZE, "%s%s",
                          monster->name(DESC_CAP_THE).c_str(),reaction.c_str());
                       mpr(info, MSGCH_MONSTER_ENCHANT);

                       if (random2(3))
                       {
                          switch (random2(4))
                          {
                             case 0: reaction = "shouts, \"I'll follow thee gladly!\"";
                                     break;
                             case 1: reaction = "shouts, \"Surely Beogh must have sent you!\"";
                                     break;
                             case 2: reaction = "asks, \"Are you our saviour?\"";
                                     break;
                            default: reaction = "says, \"I'm so glad you are here now.\"";
                          }

                          snprintf(info, INFO_SIZE, "He %s", reaction.c_str());
                          mpr(info, MSGCH_TALK);
                       }

                   }

                   monster->attitude = ATT_FRIENDLY;
                   monster->behaviour = BEH_GOD_GIFT; // alternative to BEH_FRIENDLY
                   // not really "created" friendly, but should it become
                   // hostile later on, it won't count as a good kill
                   monster->flags |= MF_CREATED_FRIENDLY;
                   monster->flags |= MF_GOD_GIFT;
                   
                   // to avoid immobile "followers"
                   behaviour_event(monster, ME_ALERT, MHITYOU);
               }
            }
            else if (mons_species(monster->type) == MONS_ORC
                     && you.species == SP_HILL_ORC
                     && !(you.religion == GOD_BEOGH)
//                     && monster->foe == MHITYOU
                     && monster->attitude == ATT_FRIENDLY
                     && (monster->flags & MF_CONVERT_ATTEMPT)
                     && (monster->flags & MF_GOD_GIFT)
                     && mons_player_visible(monster) && !mons_is_sleeping(monster)
                     && !mons_is_confused(monster) && !mons_is_paralysed(monster))
            {      // reconversion if no longer Beogh
                
                monster->attitude = ATT_HOSTILE;
                monster->behaviour = BEH_HOSTILE;
                // CREATED_FRIENDLY stays -> no piety bonus on killing these
                
                // give message only sometimes
                if (player_monster_visible(monster) && random2(4)) 
                {
                    snprintf(info, INFO_SIZE, "%s deserts you.",
                             monster->name(DESC_CAP_THE).c_str());
                    mpr(info, MSGCH_MONSTER_ENCHANT);
                }
            } // end of Beogh routine

        }                       // end "if (monster->type != -1 && mons_ner)"
    }                           // end "for s"
}                               // end monster_grid()

void fire_monster_alerts()
{
    for (int s = 0; s < MAX_MONSTERS; s++)
    {
        monsters *monster = &menv[s];

        if (monster->alive() && mons_near(monster))
        {
            if ((player_monster_visible(monster)
                 || mons_was_seen_this_turn(monster))
                && !mons_is_submerged( monster )
                && !mons_friendly( monster ))
            {
                if (!mons_class_flag( monster->type, M_NO_EXP_GAIN )
                    && !mons_is_mimic( monster->type ))
                {
                    interrupt_activity( AI_SEE_MONSTER, monster );
                }
                seen_monster( monster );
            }
        }
    }

    monsters_seen_this_turn.clear();
}

bool check_awaken(int mons_aw)
{
    int mons_perc = 0;
    struct monsters *monster = &menv[mons_aw];
    const int mon_holy = mons_holiness(monster);

    // Monsters put to sleep by ensorcelled hibernation will sleep
    // at least one turn.
    if (monster->has_ench(ENCH_SLEEPY))
        return (false);

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

    // I assume that creatures who can sense invisible are very perceptive
    mons_perc = 10 + (mons_intel(monster->type) * 4) + monster->hit_dice
                   + mons_sense_invis(monster) * 5;

    // critters that are wandering still have MHITYOU as their foe are
    // still actively on guard for the player, even if they can't see
    // him.  Give them a large bonus (handle_behaviour() will nuke 'foe'
    // after a while, removing this bonus.
    if (monster->behaviour == BEH_WANDER && monster->foe == MHITYOU)
        mons_perc += 15;

    if (!mons_player_visible(monster))
        mons_perc -= 75;

    if (monster->behaviour == BEH_SLEEP)
    {
        if (mon_holy == MH_NATURAL)
        {
            // monster is "hibernating"... reduce chance of waking
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

    if (you.backlit())
        mons_perc += 15;

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

                        const int grid = grd[count_x][count_y];
                        if (Options.stair_item_brand && is_stair(grid))
                            ecol |= COLFLAG_STAIR_ITEM;
                        else
                        {
                            ecol = (grid == DNGN_SHALLOW_WATER)?
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
            }
        }                       // end of "for count_y, count_x"
    }
}                               // end item()

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
                    which_colour = GREEN;
                    break;

                case CLOUD_COLD:
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

/* The LOS code now uses raycasting -- haranp */

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

static void set_bit_in_long_array( unsigned long* data, int where )
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
static int find_next_intercept(double* accx, double* accy, const double slope)
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
    int rc = advance(true);
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

void ray_def::regress(const coord_def &point)
{
    int opp_quadrant[4] = { 2, 3, 0, 1 };
    quadrant = opp_quadrant[quadrant];
    advance(true, &point);
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
        rc = find_next_intercept( &accx, &accy, slope );
        return rc;
    case 1:
        // going down-left
        accx = 100.0 - EPSILON_VALUE/10.0 - accx;
        rc = find_next_intercept( &accx, &accy, slope );
        accx = 100.0 - EPSILON_VALUE/10.0 - accx;
        return rc;
    case 2:
        // going up-left
        accx = 100.0 - EPSILON_VALUE/10.0 - accx;
        accy = 100.0 - EPSILON_VALUE/10.0 - accy;
        rc = find_next_intercept( &accx, &accy, slope );
        accx = 100.0 - EPSILON_VALUE/10.0 - accx;
        accy = 100.0 - EPSILON_VALUE/10.0 - accy;        
        return rc;
    case 3:
        // going up-right
        accy = 100.0 - EPSILON_VALUE/10.0 - accy;
        rc = find_next_intercept( &accx, &accy, slope );
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
static int shoot_ray( double accx, double accy, const double slope,
                      int maxrange, int xpos[], int ypos[] )
{
    int curx, cury;
    int cellnum;
    for ( cellnum = 0; true; ++cellnum )
    {
        find_next_intercept( &accx, &accy, slope );
        curx = static_cast<int>(accx);
        cury = static_cast<int>(accy);
        if ( curx > maxrange || cury > maxrange )
            break;

        // work with the new square
        xpos[cellnum] = curx;
        ypos[cellnum] = cury;
    }
    return cellnum;
}

// check if the passed ray has already been created
static bool is_duplicate_ray( int len, int xpos[], int ypos[] )
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
            if ( ray_coord_x[j + cur_offset] != xpos[j] ||
                 ray_coord_y[j + cur_offset] != ypos[j] )
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
        if ( ray_coord_x[curb] > ray_coord_x[cura] )
            return false;
        if ( ray_coord_y[curb] > ray_coord_y[cura] )
            return false;
        if ( ray_coord_x[cura] == ray_coord_x[curb] &&
             ray_coord_y[cura] == ray_coord_y[curb] )
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
            const int curx = ray_coord_x[curidx + cellnum];
            const int cury = ray_coord_y[curidx + cellnum];
            for (testidx = 0, testray = 0; testray < raynum;
                 testidx += raylengths[testray++])
            {
                // scan ahead to see if there's an intersect
                for ( testcell = 0; testcell < raylengths[raynum]; ++testcell )
                {
                    const int testx = ray_coord_x[testidx + testcell];
                    const int testy = ray_coord_y[testidx + testcell];
                    // we can short-circuit sometimes
                    if ( testx > curx || testy > cury )
                        break;
                    // bingo!
                    if ( testx == curx && testy == cury )
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
static bool register_ray( double accx, double accy, double slope )
{
    int xpos[LOS_MAX_RANGE * 2 + 1], ypos[LOS_MAX_RANGE * 2 + 1];
    int raylen = shoot_ray( accx, accy, slope, LOS_MAX_RANGE, xpos, ypos );

    // early out if ray already exists
    if ( is_duplicate_ray(raylen, xpos, ypos) )
        return false;

    // not duplicate, register
    for ( int i = 0; i < raylen; ++i )
    {
        // create the cellrays
        ray_coord_x.push_back(xpos[i]);
        ray_coord_y.push_back(ypos[i]);
    }

    // register the fullray
    raylengths.push_back(raylen);
    ray_def ray;
    ray.accx = accx;
    ray.accy = accy;
    ray.slope = slope;
    ray.quadrant = 0;
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
                set_bit_in_long_array( inptr, j + cur_offset );

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
    {
        for ( int y = 0; y <= LOS_MAX_RANGE_Y; ++y )
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
    register_ray( 0.5, 0.5, 1000.0 );
    register_ray( 0.5, 0.5, 0.0 );

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
            if ( gcd(xangle, yangle) == 1 )
                xyangles.push_back(std::pair<int,int>(xangle, yangle));

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
            register_ray( xstart, 1.0 - EPSILON_VALUE / 10.0, slope );
            // also draw the identical ray in octant 2
            register_ray( 1.0 - EPSILON_VALUE / 10.0, xstart, rslope );
        }
    }

    // Now create the appropriate blockrays array
    create_blockrays();   
}

static void set_ray_quadrant( ray_def& ray, int sx, int sy, int tx, int ty )
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
static double calc_slope(double x, double y)
{
    if (double_is_zero(x))
        return (VERTICAL_SLOPE);

    const double slope = y / x;
    return (slope > VERTICAL_SLOPE? VERTICAL_SLOPE : slope);
}

static double slope_factor(const ray_def &ray)
{
    double xdiff = fabs(ray.accx - 0.5), ydiff = fabs(ray.accy - 0.5);

    if (double_is_zero(xdiff) && double_is_zero(ydiff))
        return ray.slope;
    const double slope = calc_slope(ydiff, xdiff);
    return (slope + ray.slope) / 2.0;
}

static bool superior_ray(int shortest, int imbalance,
                         int raylen, int rayimbalance,
                         double slope_diff, double ray_slope_diff)
{
    if (shortest != raylen)
        return (shortest > raylen);

    if (imbalance != rayimbalance)
        return (imbalance > rayimbalance);

    return (slope_diff > ray_slope_diff);
}

// Find a nonblocked ray from sx, sy to tx, ty. Return false if no
// such ray could be found, otherwise return true and fill ray
// appropriately.
// If allow_fallback is true, fall back to a center-to-center ray
// if range is too great or all rays are blocked.
// If cycle_dir is 0, find the first fitting ray. If it is 1 or -1,
// assume that ray is appropriately filled in, and look for the next
// ray in that cycle direction.
// If find_shortest is true, examine all rays that hit the target and
// take the shortest (starting at ray.fullray_idx).

bool find_ray( int sourcex, int sourcey, int targetx, int targety,
               bool allow_fallback, ray_def& ray, int cycle_dir,
               bool find_shortest )
{
    int cellray, inray;
    const int signx = ((targetx - sourcex >= 0) ? 1 : -1);
    const int signy = ((targety - sourcey >= 0) ? 1 : -1);
    const int absx = signx * (targetx - sourcex);
    const int absy = signy * (targety - sourcey);
    const double want_slope = calc_slope(absx, absy);
    int cur_offset = 0;
    int shortest = INFINITE_DISTANCE;
    int imbalance = INFINITE_DISTANCE;
    double slope_diff = VERTICAL_SLOPE * 10.0;
    std::vector<coord_def> unaliased_ray;
    
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
            if ( ray_coord_x[cellray + cur_offset] == absx &&
                 ray_coord_y[cellray + cur_offset] == absy )
            {
                if (find_shortest)
                {
                    unaliased_ray.clear();
                    unaliased_ray.push_back(coord_def(0, 0));
                }

                // check if we're blocked so far
                bool blocked = false;
                coord_def c1, c3;
                int real_length = 0;
                for ( inray = 0; inray <= cellray; ++inray )
                {
                    const int xi = signx * ray_coord_x[inray + cur_offset];
                    const int yi = signy * ray_coord_y[inray + cur_offset];
                    if (inray < cellray
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
                    for (int i = 1, size = unaliased_ray.size(); i < size;
                         ++i)
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

                const double ray_slope_diff =
                    find_shortest? fabs(slope_factor(fullrays[fullray])
                                      - want_slope)
                    : 0.0;

                if ( !blocked
                     &&  (!find_shortest
                          || superior_ray(shortest, imbalance,
                                          real_length, cimbalance,
                                          slope_diff, ray_slope_diff)))
                {
                    // success!
                    ray        = fullrays[fullray];
                    ray.fullray_idx = fullray;

                    shortest   = real_length;
                    imbalance  = cimbalance;
                    slope_diff = ray_slope_diff;

                    if ( sourcex > targetx )
                        ray.accx = 1.0 - ray.accx;
                    if ( sourcey > targety )
                        ray.accy = 1.0 - ray.accy;
                    ray.accx += sourcex;
                    ray.accy += sourcey;
                    set_ray_quadrant(ray, sourcex, sourcey, targetx, targety);
                    if (!find_shortest)
                        return true;
                }
            }
        }
    }

    if (find_shortest && shortest != INFINITE_DISTANCE)
        return (true);
    
    if ( allow_fallback )
    {
        ray.accx = sourcex + 0.5;
        ray.accy = sourcey + 0.5;
        if ( targetx == sourcex )
            ray.slope = VERTICAL_SLOPE;
        else
        {
            ray.slope = targety - sourcey;
            ray.slope /= targetx - sourcex;
            if ( ray.slope < 0 )
                ray.slope = -ray.slope;
        }
        set_ray_quadrant(ray, sourcex, sourcey, targetx, targety);
        ray.fullray_idx = -1;
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
             FixedArray < unsigned char, 80, 70 > &gr, int x_p, int y_p)
{
    raycast();
    // go quadrant by quadrant
    const int quadrant_x[4] = {  1, -1, -1,  1 };
    const int quadrant_y[4] = {  1,  1, -1, -1 };

    // clear out sh
    sh.init(0);

    const unsigned int num_cellrays = compressed_ray_x.size();
    const unsigned int num_words = (num_cellrays + LONGSIZE - 1) / LONGSIZE;

    for ( int quadrant = 0; quadrant < 4; ++quadrant )
    {
        const int xmult = quadrant_x[quadrant];
        const int ymult = quadrant_y[quadrant];

        // clear out the dead rays array
        memset( (void*)dead_rays,  0, sizeof(unsigned long) * num_words);
        memset( (void*)smoke_rays, 0, sizeof(unsigned long) * num_words);

        // kill all blocked rays
        const unsigned long* inptr = los_blockrays;
        for ( int xdiff = 0; xdiff <= LOS_MAX_RANGE_X; ++xdiff )
        {
            for (int ydiff = 0; ydiff <= LOS_MAX_RANGE_Y;
                 ++ydiff, inptr += num_words )
            {
                
                const int realx = x_p + xdiff * xmult;
                const int realy = y_p + ydiff * ymult;

                if (realx < 0 || realx > 79 || realy < 0 || realy > 69)
                    continue;

                // if this cell is opaque...
                if ( grid_is_opaque(gr[realx][realy]) )
                {
                    // then block the appropriate rays
                    for ( unsigned int i = 0; i < num_words; ++i )
                        dead_rays[i] |= inptr[i];
                }
                else if ( is_opaque_cloud(env.cgrid[realx][realy]) )
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
        // quadrant are visible
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
                    const int realx = xmult * compressed_ray_x[rayidx];
                    const int realy = ymult * compressed_ray_y[rayidx];
                    // update shadow map
                    if (x_p + realx >= 0 && x_p + realx < 80 &&
                        y_p + realy >= 0 && y_p + realy < 70 &&
                        realx * realx + realy * realy <= los_radius_squared )
                        sh[sh_xo+realx][sh_yo+realy]=gr[x_p+realx][y_p+realy];
                }
                ++rayidx;
                if ( rayidx == num_cellrays )
                    break;
            }
        }
    }

    // [dshaligram] The player's current position is always visible.
    sh[sh_xo][sh_yo] = gr[x_p][y_p];
}


void draw_border(void)
{
    textcolor( BORDER_COLOR );
    clrscr();
    redraw_skill( you.your_name, player_title() );

    const int xcol = crawl_view.hudp.x;
    
    gotoxy(xcol, 2);
    cprintf( "%s %s", species_name( you.species, you.experience_level ), 
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

static int find_feature(unsigned char feature, int curs_x, int curs_y, 
                         int start_x, int start_y, int anchor_x, int anchor_y,
                         int ignore_count, char *move_x, char *move_y)
{
    int cx = anchor_x,
        cy = anchor_y;

    int firstx = -1, firsty = -1;
    int matchcount = 0;

    // Find the first occurrence of feature 'feature', spiralling around (x,y)
    int maxradius = GXM > GYM? GXM : GYM;
    for (int radius = 1; radius < maxradius; ++radius)
    {
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
                if (is_feature(feature, x, y))
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
        if (is_feature(feature, coord.x, coord.y))
            found->push_back(coord);
    }
}

static int find_feature( const std::vector<coord_def>& features,
                         unsigned char feature, int curs_x, int curs_y, 
                         int start_x, int start_y, 
                         int ignore_count, 
                         char *move_x, char *move_y,
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

#ifdef USE_CURSES
// NOTE: This affects libunix.cc draw state; use this just before setting
// textcolour and drawing a character and call set_altcharset(false)
// after you're done drawing.
//
static int cset_adjust(int raw)
{
    if (Options.char_set != CSET_ASCII && Options.char_set != CSET_UNICODE)
    {
        // switch to alternate char set for 8-bit characters:
        set_altcharset( raw > 127 );

        // shift the DEC line drawing set:
        if (Options.char_set == CSET_DEC 
            && raw >= 0xE0)
        {
            raw &= 0x7F;
        }
    }
    return (raw);
}
#endif

static int get_number_of_lines_levelmap()
{
    return get_number_of_lines() - (Options.level_map_title ? 1 : 0);
}

static void draw_level_map(int start_x, int start_y, bool travel_mode)
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
        gotoxy(1, 1);
        textcolor(WHITE);
        cprintf("%-*s",
                get_number_of_cols() - 1,
                ("Level " + level_description_string()).c_str());

        const formatted_string help =
            formatted_string::parse_string("(Press <w>?</w> for help)");
        textcolor(LIGHTGREY);
        gotoxy(get_number_of_cols() - std::string(help).length() + 1, 1);
        help.display();
    }

    gotoxy(1, top);

    for (int screen_y = 0; screen_y < num_lines; screen_y++)
    {
        for (int screen_x = 0; screen_x < num_cols - 1; screen_x++)
        {
            screen_buffer_t colour = DARKGREY;

            coord_def c(start_x + screen_x, start_y + screen_y);

            if (!in_bounds(c))
            {
                buffer2[bufcount2 + 1] = DARKGREY;
                buffer2[bufcount2] = 0;
            }
            else
            {
                colour = colour_code_map(c.x, c.y,
                                         Options.item_colour, 
                                         travel_mode && Options.travel_colour);

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
            
            bufcount2 += 2;

            // newline
            if (screen_x == 0 && screen_y > 0)
                gotoxy( 1, screen_y + top );

            unsigned ch = buffer2[bufcount2 - 2];
#ifdef USE_CURSES
            ch = cset_adjust( ch );
#endif
            textcolor( buffer2[bufcount2 - 1] );
            putwch(ch);
        }
    }

#ifdef USE_CURSES
    set_altcharset(false);
#endif

    update_screen();
}

static void reset_travel_colours(std::vector<coord_def> &features)
{
    // We now need to redo travel colours
    features.clear();
    find_travel_pos(you.x_pos, you.y_pos, NULL, NULL, &features);
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
    int i, j;

    char move_x = 0;
    char move_y = 0;
    char getty = 0;

    // Vector to track all features we can travel to, in order of distance.
    std::vector<coord_def> features;
    if (travel_mode)
    {
        travel_cache.update();

        find_travel_pos(you.x_pos, you.y_pos, NULL, NULL, &features);
        // Sort features into the order the player is likely to prefer.
        arrange_features(features);
    }

    char min_x = 80, max_x = 0, min_y = 0, max_y = 0;
    bool found_y = false;

    const int num_lines = get_number_of_lines_levelmap();
    const int half_screen = (num_lines - 1) / 2;

    const int top = 1 + Options.level_map_title;

    for (j = 0; j < GYM; j++)
    {
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
    }

    const int map_lines = max_y - min_y + 1;

    const int start_x = min_x + (max_x - min_x + 1) / 2 - 40; // no x scrolling
    const int block_step = Options.level_map_cursor_step;
    int start_y = 0;                                          // y does scroll

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

    int curs_x = you.x_pos - start_x + 1;
    int curs_y = you.y_pos - screen_y + half_screen + 1;
    int search_feat = 0, search_found = 0, anchor_x = -1, anchor_y = -1;

    bool map_alive  = true;
    bool redraw_map = true;

    clrscr();
    textcolor(DARKGREY);

    while (map_alive)
    {
        start_y = screen_y - half_screen;

        if (redraw_map)
            draw_level_map(start_x, start_y, travel_mode);

        redraw_map = true;
        cursorxy(curs_x, curs_y + top - 1);

        getty = unmangle_direction_keys(getchm(KC_LEVELMAP), KC_LEVELMAP,
                                        false, false);

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
            travel_cache.add_waypoint(start_x + curs_x - 1,
                                      start_y + curs_y - 1);
            // We need to do this all over again so that the user can jump
            // to the waypoint he just created.
            features.clear();
            find_travel_pos(you.x_pos, you.y_pos, NULL, NULL, &features);
            // Sort features into the order the player is likely to prefer.
            arrange_features(features);
            move_x = move_y = 0;
            break;

        // Cycle the radius of an exclude.
        case 'x':
        {
            const coord_def p(start_x + curs_x - 1, start_y + curs_y - 1);
            if (is_exclude_root(p))
                cycle_exclude_radius(p);
            reset_travel_colours(features);
            move_x = move_y = 0;
            break;
        }
        
        case CONTROL('E'):
        case CONTROL('X'):
        {
            int x = start_x + curs_x - 1, y = start_y + curs_y - 1;
            if (getty == CONTROL('X'))
                toggle_exclude(coord_def(x, y));
            else
                clear_excludes();

            reset_travel_colours(features);
            move_x = move_y = 0;
            break;
        }
            
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
        case '!':
            mprf("Terrain seen: %s",
                 is_terrain_seen(start_x + curs_x - 1,
                                 start_y + curs_y - 1)? "yes" : "no");
            more();
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
        case '*':
        case '/':
        case '\'':
        {
            bool forward = true;

            if (getty == '/' || getty == ';')
                forward = false;

            if (getty == '/' || getty == '*' || getty == ';' || getty == '\'')
                getty = 'I';

            move_x = 0;
            move_y = 0;
            if (anchor_x == -1)
            {
                anchor_x = start_x + curs_x - 1;
                anchor_y = start_y + curs_y - 1;
            }
            if (search_feat != getty)
            {
                search_feat         = getty;
                search_found        = 0;
            }
            if (travel_mode)
                search_found = find_feature(features, getty, curs_x, curs_y, 
                                            start_x, start_y,
                                            search_found, 
                                            &move_x, &move_y,
                                            forward);
            else
                search_found = find_feature(getty, curs_x, curs_y,
                                            start_x, start_y,
                                            anchor_x, anchor_y,
                                            search_found, &move_x, &move_y);
            break;
        }
        case '.':
        case '\r':
        case 'S':
        case ',':
        case ';':
        {
            int x = start_x + curs_x - 1, y = start_y + curs_y - 1;
            if (travel_mode && x == you.x_pos && y == you.y_pos)
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
                spec_place[0] = x;
                spec_place[1] = y;
                map_alive = false;
                break;
            }
        }
        default:
            move_x = 0;
            move_y = 0;
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

        if (curs_x + move_x < 1 || curs_x + move_x > crawl_view.termsz.x)
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

                continue;
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
    }

    return;
}                               // end show_map()


void magic_mapping(int map_radius, int proportion)
{
    int i, j, k, l, empty_count;

    if (map_radius > 50 && map_radius != 1000)
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

                        if (grid_is_opaque( grd[i + k][j + l] )
                                && grd[i + k][j + l] != DNGN_CLOSED_DOOR)
                            empty_count--;
                    }
                }
            }

            if (empty_count > 0)
            {
                if (!get_envmap_obj(i, j))
                    set_envmap_obj(i, j, grd[i][j]);

                // Hack to give demonspawn Pandemonium mutation the ability
                // to detect exits magically.
                if ((you.mutation[MUT_PANDEMONIUM] > 1
                        && grd[i][j] == DNGN_EXIT_PANDEMONIUM)
#ifdef WIZARD
                    || (map_radius == 1000 && you.wizard)
#endif
                    )
                {
                    set_terrain_seen( i, j );
                }
                else
                {
                    set_terrain_mapped( i, j );
                }
            }
        }
    }
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
        '_', 0x22C2, 0x2320, 0x2248, '8', '~', '~',
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
    }
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
            Feature[i].symbol = Options.char_table[ DCHAR_STAIRS_DOWN ];
            Feature[i].colour = BROWN;
            Feature[i].map_colour = BROWN;
            break;
            
        case DNGN_STONE_STAIRS_DOWN_I:
        case DNGN_STONE_STAIRS_DOWN_II:
        case DNGN_STONE_STAIRS_DOWN_III:
            Feature[i].symbol = Options.char_table[ DCHAR_STAIRS_DOWN ];
            Feature[i].colour = LIGHTGREY;
            Feature[i].map_colour = RED;
            break;

        case DNGN_ROCK_STAIRS_UP:
            Feature[i].symbol = Options.char_table[ DCHAR_STAIRS_UP ];
            Feature[i].colour = BROWN;
            Feature[i].map_colour = BROWN;
            break;
            
        case DNGN_STONE_STAIRS_UP_I:
        case DNGN_STONE_STAIRS_UP_II:
        case DNGN_STONE_STAIRS_UP_III:
            Feature[i].symbol = Options.char_table[ DCHAR_STAIRS_UP ];
            Feature[i].colour = LIGHTGREY;
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
        case DNGN_ENTER_SHOALS:
        case DNGN_ENTER_RESERVED_2:
        case DNGN_ENTER_RESERVED_3:
        case DNGN_ENTER_RESERVED_4:
            Feature[i].colour = YELLOW;
            Feature[i].symbol = Options.char_table[ DCHAR_STAIRS_DOWN ];
            Feature[i].notable = true;
            Feature[i].map_colour = RED;
            Feature[i].seen_colour = YELLOW;
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
        case DNGN_RETURN_FROM_SHOALS:
        case DNGN_RETURN_RESERVED_2:
        case DNGN_RETURN_RESERVED_3:
        case DNGN_RETURN_RESERVED_4:
            Feature[i].colour = YELLOW;
            Feature[i].symbol = Options.char_table[ DCHAR_STAIRS_UP ];
            Feature[i].map_colour  = GREEN;
            Feature[i].seen_colour = YELLOW;
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

        case DNGN_ALTAR_LUGONU:
            Feature[i].colour = GREEN;
            Feature[i].symbol = Options.char_table[ DCHAR_ALTAR ];
            Feature[i].notable = true;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = GREEN;
            break;

        case DNGN_ALTAR_BEOGH:
            Feature[i].colour = EC_UNHOLY;
            Feature[i].symbol = Options.char_table[ DCHAR_ALTAR ];
            Feature[i].notable = true;
            Feature[i].map_colour = DARKGREY;
            Feature[i].seen_colour = RED;
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
    }

    apply_feature_overrides();

    for (int i = 0; i < NUM_FEATURES; ++i)
    {
        if (!Feature[i].magic_symbol)
            Feature[i].magic_symbol = Feature[i].symbol;

        if (Feature[i].seen_colour == BLACK)
            Feature[i].seen_colour = Feature[i].map_colour;
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
                int object = grd(gc);
                unsigned glych;
                unsigned short glycol = 0;

                if (object == DNGN_SECRET_DOOR)
                    object = grid_secret_door_appearance( gc.x, gc.y );

                get_symbol( gc.x, gc.y, object, &glych, &glycol );
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
    else if (you.berserker)
        return (RED);

    return (BLACK);
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
    std::vector<screen_buffer_t> buffy(
        crawl_view.viewsz.y * crawl_view.viewsz.x * 2);
    
    int count_x, count_y;

    losight( env.show, grd, you.x_pos, you.y_pos ); // must be done first

    for (count_x = 0; count_x < ENV_SHOW_DIAMETER; count_x++)
    {
        for (count_y = 0; count_y < ENV_SHOW_DIAMETER; count_y++)
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
        cursor_control cs(false);

        const bool map = player_in_mappable_area();
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
                // in grid coords
                const coord_def gc(view2grid(coord_def(count_x, count_y)));
                const coord_def ep = view2show(grid2view(gc));

                if (Options.tutorial_left && in_bounds(gc)
                    && crawl_view.in_grid_los(gc))
                {
                    const int object = env.show(ep);
                    if (object)
                    {
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
                if (flash_colour != BLACK 
                    && buffy[bufcount + 1] != DARKGREY)
                {
                    buffy[bufcount + 1] = flash_colour;
                }

                bufcount += 2;
            }
        }

        // Leaving it this way because short flashes can occur in long ones,
        // and this simply works without requiring a stack.
        you.flash_colour = BLACK;

        // avoiding unneeded draws when running
        if (!you.running || (you.running < 0 && Options.travel_delay > -1))
        {
            bufcount = 0;
            for (count_y = 0; count_y < crawl_view.viewsz.y; count_y++)
            {
                gotoxy( crawl_view.viewp.x, crawl_view.viewp.y + count_y );
                for (count_x = 0; count_x < crawl_view.viewsz.x; count_x++)
                {
#ifdef USE_CURSES
                    buffy[bufcount] = cset_adjust( buffy[bufcount] );
#endif
                    textcolor( buffy[bufcount + 1] );
                    putwch( buffy[bufcount] );
                    bufcount += 2;
                }
            }
        }

#ifdef USE_CURSES
        set_altcharset( false );
#endif
        update_screen();
    }
}                               // end viewwindow()

//////////////////////////////////////////////////////////////////////////////
// crawl_view_geometry

const int crawl_view_geometry::message_min_lines;
const int crawl_view_geometry::hud_min_width;
const int crawl_view_geometry::hud_min_gutter;
const int crawl_view_geometry::hud_max_gutter;

crawl_view_geometry::crawl_view_geometry()
    : termsz(80, 24), viewp(1, 1), viewsz(33, 17),
      hudp(40, 1), hudsz(41, 17),
      msgp(1, viewp.y + viewsz.y), msgsz(80, 7)
{
}

void crawl_view_geometry::init_view()
{
    set_player_at(you.pos(), true);
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
        const int xmarg =
            Options.scroll_margin_x + LOS_RADIUS <= viewhalfsz.x
            ? Options.scroll_margin_x
            : viewhalfsz.x - LOS_RADIUS;
        const int ymarg =
            Options.scroll_margin_y + LOS_RADIUS <= viewhalfsz.y
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

    // If the terminal is too small, exit with an error.
    if (termsz.x < 80 || termsz.y < 24)
        end(1, false, "Terminal too small (%d,%d), need at least (80,24)",
            termsz.x, termsz.y);


    int freeheight = termsz.y - message_min_lines;

    // Make the viewport as tall as possible.
    viewsz.y = freeheight < Options.view_max_height?
        freeheight : Options.view_max_height;

    // Make sure we're odd-sized.
    if (!(viewsz.y % 2))
        --viewsz.y;

    // The message pane takes all lines not used by the viewport.
    msgp  = coord_def(1, viewsz.y + 1);
    msgsz = coord_def(termsz.x, termsz.y - viewsz.y);

    int freewidth = termsz.x - (hud_min_width + hud_min_gutter);
    // Make the viewport as wide as possible.
    viewsz.x = freewidth < Options.view_max_width?
        freewidth : Options.view_max_width;

    if (!(viewsz.x % 2))
        --viewsz.x;

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
