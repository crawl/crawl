/*
 *  File:       overmap.cc
 *  Idea:       allow player to make notes about levels. I don't know how
 *              to do this (I expect it will require some kind of dynamic
 *              memory management thing). - LH
 *  Summary:    Records location of stairs etc
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <3>      30/7/00        MV      Made Over-map full-screen
 *      <2>      8/10/99        BCR     Changed Linley's macros
 *                                      to an enum in overmap.h
 *      <1>      29/8/99        LRH     Created
 */

#include "AppHdr.h"
#include "overmap.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "externs.h"

#include "branch.h"
#include "dgnevent.h"
#include "direct.h"
#include "files.h"
#include "menu.h"
#include "misc.h"
#include "religion.h"
#include "shopping.h"
#include "stuff.h"
#include "view.h"

typedef std::map<branch_type, level_id> stair_map_type;
typedef std::map<level_pos, shop_type> shop_map_type;
typedef std::map<level_pos, god_type> altar_map_type;
typedef std::map<level_pos, portal_type> portal_map_type;

stair_map_type stair_level;
shop_map_type shops_present;
altar_map_type altars_present;
portal_map_type portals_present;

static void seen_altar( god_type god, const coord_def& pos );
static void seen_staircase(dungeon_feature_type which_staircase,
                           const coord_def& pos);
static void seen_other_thing(dungeon_feature_type which_thing,
                             const coord_def& pos);

void seen_notable_thing( dungeon_feature_type which_thing, int x, int y )
{
    // Tell the world first.
    dungeon_events.fire_position_event(DET_PLAYER_IN_LOS, coord_def(x, y));
    
    // Don't record in temporary terrain
    if (you.level_type != LEVEL_DUNGEON)
        return;
    
    const coord_def pos(x, y);
    const god_type god = grid_altar_god(which_thing);
    if (god != GOD_NO_GOD)
        seen_altar( god, pos );
    else if (grid_is_branch_stairs( which_thing ))
        seen_staircase( which_thing, pos );
    else
        seen_other_thing( which_thing, pos );
}

static dungeon_feature_type portal_to_feature(portal_type p)
{
    switch ( p )
    {
    case PORTAL_LABYRINTH:
        return DNGN_ENTER_LABYRINTH;
    case PORTAL_HELL:
        return DNGN_ENTER_HELL;
    case PORTAL_ABYSS:
        return DNGN_ENTER_ABYSS;
    case PORTAL_PANDEMONIUM:
        return DNGN_ENTER_PANDEMONIUM;
    default:
        return DNGN_FLOOR;
    }
}

static const char* portaltype_to_string(portal_type p)
{
    switch ( p )
    {
    case PORTAL_LABYRINTH:
        return "<cyan>Labyrinth:</cyan>";
    case PORTAL_HELL:
        return "<red>Hell:</red>";
    case PORTAL_ABYSS:
        return "<magenta>Abyss:</magenta>";
    case PORTAL_PANDEMONIUM:
        return "<blue>Pan:</blue>";
    default:
        return "<lightred>Buggy:</lightred>";
    }
}

static std::string shoptype_to_string(shop_type s)
{
    switch ( s )
    {
    case SHOP_WEAPON:
        return "(";
    case SHOP_WEAPON_ANTIQUE:
        return "<yellow>(</yellow>";
    case SHOP_ARMOUR:
        return "[";
    case SHOP_ARMOUR_ANTIQUE:
        return "<yellow>[</yellow>";
    case SHOP_GENERAL:
        return "*";
    case SHOP_GENERAL_ANTIQUE:
        return "<yellow>*</yellow>";
    case SHOP_JEWELLERY:
        return "=";
    case SHOP_WAND:
        return "/";
    case SHOP_BOOK:
        return "+";
    case SHOP_FOOD:
        return "%";
    case SHOP_DISTILLERY:
        return "!";
    case SHOP_SCROLL:
        return "?";
    default:
        return "x";
    }
}

static altar_map_type get_notable_altars(const altar_map_type &altars)
{
    altar_map_type notable_altars;
    for ( altar_map_type::const_iterator na_iter = altars.begin();
          na_iter != altars.end(); ++na_iter )
    {
        if (na_iter->first.id.branch != BRANCH_ECUMENICAL_TEMPLE)
            notable_altars[na_iter->first] = na_iter->second;
    }
    return (notable_altars);
}

inline static std::string place_desc(const level_pos &pos)
{
    return "[" + pos.id.describe(false, true) + "] ";
}

inline static std::string altar_description(god_type god)
{
    return feature_description( altar_for_god(god) );
}

inline static std::string portal_description(portal_type portal)
{
    return feature_description( portal_to_feature(portal) );
}

bool overmap_knows_portal(dungeon_feature_type portal)
{
    for ( portal_map_type::const_iterator pl_iter = portals_present.begin();
          pl_iter != portals_present.end(); ++pl_iter )
    {
        if (portal_to_feature(pl_iter->second) == portal)
            return (true);
    }
    return (false);
}

std::string overview_description_string()
{
    char buffer[100];
    std::string disp;
    bool seen_anything = false;

    disp += "                    <white>Overview of the Dungeon</white>\n" ;

    // print branches
    int branchcount = 0;
    for (int i = 0; i < NUM_BRANCHES; ++i)
    {
        const branch_type branch = branches[i].id;
        if ( stair_level.find(branch) != stair_level.end() )
        {
            if ( !branchcount )
            {
                disp += "\n<green>Branches:</green>";
                if (crawl_state.need_save || !crawl_state.updating_scores)
                    disp += " (use <white>Ctrl-G</white> to reach them)";
                disp += EOL;
                seen_anything = true;
            }
            
            ++branchcount;

            snprintf(buffer, sizeof buffer, "<yellow>%-6s</yellow>: %-7s",
                     branches[branch].abbrevname,
                     stair_level[branch].describe(false, true).c_str());
            disp += buffer;
            if ( (branchcount % 4) == 0 )
                disp += "\n";
            else
                disp += "   ";
        }
    }
    if ( branchcount && (branchcount % 4) )
        disp += "\n";

    // remove unworthy altars from the list we show the user. Yeah,
    // one more round of map iteration.
    const altar_map_type notable_altars = get_notable_altars(altars_present);
    
    // print altars
    // we loop through everything a dozen times, oh well
    if ( !notable_altars.empty() )
    {
        disp += "\n<green>Altars:</green>";
        if (crawl_state.need_save || !crawl_state.updating_scores)
            disp += " (use <white>Ctrl-F \"altar\"</white> to reach them)";
        disp += EOL;
        seen_anything = true;
    }

    level_id last_id;
    std::map<level_pos, god_type>::const_iterator ci_altar;    
    for ( int cur_god = GOD_NO_GOD; cur_god < NUM_GODS; ++cur_god )
    {
        last_id.depth = 10000;  // fake depth to be sure we don't match
        // GOD_NO_GOD becomes your god
        int real_god = (cur_god == GOD_NO_GOD ? you.religion : cur_god);
        if ( cur_god == you.religion )
            continue;

        for ( ci_altar = notable_altars.begin();
              ci_altar != notable_altars.end();
              ++ci_altar )
        {
            if ( ci_altar->second == real_god )
            {
                if ( last_id.depth == 10000 )
                {
                    disp += god_name( ci_altar->second, false );
                    disp += ": ";
                    disp += ci_altar->first.id.describe(false, true);
                }
                else
                {
                    if ( last_id == ci_altar->first.id )
                        disp += '*';
                    else
                    {
                        disp += ", ";
                        disp += ci_altar->first.id.describe(false, true);
                    }
                }
                last_id = ci_altar->first.id;
            }
        }
        if ( last_id.depth != 10000 )
            disp += "\n";
    }

    // print shops
    if (!shops_present.empty())
    {
        disp +="\n<green>Shops:</green>";
        if (crawl_state.need_save || !crawl_state.updating_scores)
            disp += " (use <white>Ctrl-F \"shop\"</white> to reach them)";
        disp += EOL;
        seen_anything = true;
    }
    last_id.depth = 10000;
    std::map<level_pos, shop_type>::const_iterator ci_shops;

    // there are at most 5 shops per level, plus 7 chars for the level
    // name, plus 4 for the spacing; that makes a total of 17
    // characters per shop.
    const int maxcolumn = get_number_of_cols() - 17;
    int column_count = 0;

    for ( ci_shops = shops_present.begin();
          ci_shops != shops_present.end();
          ++ci_shops )
    {
        if ( ci_shops->first.id != last_id )
        {
            if ( column_count > maxcolumn )
            {
                disp += "\n";
                column_count = 0;
            }
            else if ( column_count != 0 )
            {
                disp += "  ";
                ++column_count;
            }
            disp += "<brown>";

            const std::string loc = ci_shops->first.id.describe(false, true);
            disp += loc;
            column_count += loc.length();

            disp += "</brown>";

            disp += ": ";
            column_count += 2;

            last_id = ci_shops->first.id;
        }
        disp += shoptype_to_string(ci_shops->second);
        ++column_count;
    }
    disp += "\n";

    // print portals
    if ( !portals_present.empty() )
    {
        disp += "\n<green>Portals:</green>\n";
        seen_anything = true;
    }
    for (int cur_portal = PORTAL_NONE; cur_portal < NUM_PORTALS; ++cur_portal)
    {
        last_id.depth = 10000;
        std::map<level_pos, portal_type>::const_iterator ci_portals;
        for ( ci_portals = portals_present.begin();
              ci_portals != portals_present.end();
              ++ci_portals )
        {
            // one line per region should be enough, they're all of
            // the form D:XX, except for labyrinth portals, of which
            // you would need 11 (at least) to have a problem.
            if ( ci_portals->second == cur_portal )
            {
                if ( last_id.depth == 10000 )
                    disp += portaltype_to_string(ci_portals->second);

                if ( ci_portals->first.id == last_id )
                    disp += '*';
                else
                {
                    disp += ' ';
                    disp += ci_portals->first.id.describe(false, true);
                }
                last_id = ci_portals->first.id;
            }
        }
        if ( last_id.depth != 10000 )
            disp += "\n";
    }

    if (!seen_anything)
    {
        if (crawl_state.need_save || !crawl_state.updating_scores)
            disp += "You haven't discovered anything interesting yet.";
        else
            disp += "You didn't discover anything interesting.";
    }

    return disp;
}

template <typename Z, typename Key>
inline static bool find_erase(Z &map, const Key &k)
{
    if (map.find(k) != map.end())
    {
        map.erase(k);
        return (true);
    }
    return (false);
}

static bool unnotice_portal(const level_pos &pos)
{
    return find_erase(portals_present, pos);
}

static bool unnotice_altar(const level_pos &pos)
{
    return find_erase(altars_present, pos);
}

static bool unnotice_shop(const level_pos &pos)
{
    return find_erase(shops_present, pos);
}

static bool unnotice_stair(const level_pos &pos)
{
    const dungeon_feature_type feat = grd(pos.pos);
    if (grid_is_branch_stairs(feat))
    {
        for (int i = 0; i < NUM_BRANCHES; ++i)
        {
            if (branches[i].entry_stairs == feat)
            {
                const branch_type br = static_cast<branch_type>(i);
                return (find_erase(stair_level, br));
            }
        }
    }
    return (false);
}

bool unnotice_feature(const level_pos &pos)
{
    return (unnotice_portal(pos)
            || unnotice_altar(pos)
            || unnotice_shop(pos)
            || unnotice_stair(pos));
}

void display_overmap()
{
    std::string disp = overview_description_string();
    linebreak_string(disp, get_number_of_cols() - 5, get_number_of_cols() - 1);
    formatted_scroller(MF_EASY_EXIT | MF_ANYPRINTABLE | MF_NOSELECT,
                       disp).show();
    redraw_screen();
}

void seen_staircase( dungeon_feature_type which_staircase,
                     const coord_def& pos )
{
    // which_staircase holds the grid value of the stair, must be converted
    // Only handles stairs, not gates or arches
    // Don't worry about:
    //   - stairs returning to dungeon - predictable
    //   - entrances to the hells - always in vestibule

    int i;
    for ( i = 0; i < NUM_BRANCHES; ++i )
    {
        if ( branches[i].entry_stairs == which_staircase )
        {
            stair_level[branches[i].id] = level_id::current();
            break;
        }
    }
    ASSERT( i != NUM_BRANCHES );
}

// if player has seen an altar; record it
void seen_altar( god_type god, const coord_def& pos )
{
    // can't record in abyss or pan.
    if ( you.level_type != LEVEL_DUNGEON )
        return;
    
    level_pos where(level_id::current(), pos);
    altars_present[where] = god;
}

void unnotice_altar()
{
    const coord_def pos(you.x_pos, you.y_pos);
    const level_pos curpos(level_id::current(), pos);
    // Hmm, what happens when erasing a nonexistent key directly?
    if (altars_present.find(curpos) != altars_present.end())
        altars_present.erase(curpos);
}

portal_type feature_to_portal( unsigned char feat )
{
    switch ( feat )
    {
    case DNGN_ENTER_LABYRINTH:
        return PORTAL_LABYRINTH;
    case DNGN_ENTER_HELL:
        return PORTAL_HELL;
    case DNGN_ENTER_ABYSS:
        return PORTAL_ABYSS;
    case DNGN_ENTER_PANDEMONIUM:
        return PORTAL_PANDEMONIUM;
    default:
        return PORTAL_NONE;
    }
}

// if player has seen any other thing; record it
void seen_other_thing( dungeon_feature_type which_thing, const coord_def& pos )
{
    if ( you.level_type != LEVEL_DUNGEON ) // can't record in abyss or pan.
        return;

    level_pos where(level_id::current(), pos);

    switch ( which_thing )
    {
    case DNGN_ENTER_SHOP:
        shops_present[where] =
            static_cast<shop_type>(get_shop(pos.x, pos.y)->type);
        break;
    default:
        const portal_type portal = feature_to_portal(which_thing);
        if ( portal != PORTAL_NONE )
            portals_present[where] = portal;
        break;
    }
}          // end seen_other_thing()
