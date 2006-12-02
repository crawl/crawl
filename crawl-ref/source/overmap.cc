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

// for #definitions of MAX_BRANCHES & MAX_LEVELS
#include "files.h"
#include "menu.h"
#include "misc.h"
#include "religion.h"
#include "shopping.h"
#include "stuff.h"
#include "view.h"

std::map<branch_type, level_id> stair_level;
std::map<level_pos, shop_type> shops_present;
std::map<level_pos, god_type> altars_present;
std::map<level_pos, portal_type> portals_present;

static void seen_altar( god_type god, const coord_def& pos );
static void seen_staircase(unsigned char which_staircase,const coord_def& pos);
static void seen_other_thing(unsigned char which_thing, const coord_def& pos);

void seen_notable_thing( int which_thing, int x, int y )
{
    // Don't record in temporary terrain
    if (you.level_type != LEVEL_DUNGEON)
        return;
    
    coord_def pos = {x, y};

    const god_type god = grid_altar_god(which_thing);
    if (god != GOD_NO_GOD)
        seen_altar( god, pos );
    else if (grid_is_branch_stairs( which_thing ))
        seen_staircase( which_thing, pos );
    else
        seen_other_thing( which_thing, pos );
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

static char shoptype_to_char(shop_type s)
{
    switch ( s )
    {
    case SHOP_WEAPON:
    case SHOP_WEAPON_ANTIQUE:
        return '(';
    case SHOP_ARMOUR:
    case SHOP_ARMOUR_ANTIQUE:
        return '[';
    case SHOP_GENERAL_ANTIQUE:
    case SHOP_GENERAL:
        return '*';
    case SHOP_JEWELLERY:
        return '=';
    case SHOP_WAND:
        return '/';
    case SHOP_BOOK:
        return '+';
    case SHOP_FOOD:
        return '%';
    case SHOP_DISTILLERY:
        return '!';
    case SHOP_SCROLL:
        return '?';
    default:
        return 'x';
    }
}

std::string overview_description_string()
{
    char buffer[100];
    std::string disp;
    bool seen_anything = false;

    // better put this somewhere central
    const branch_type list_order[] = 
    {
        BRANCH_MAIN_DUNGEON, 
        BRANCH_ECUMENICAL_TEMPLE,
        BRANCH_ORCISH_MINES, BRANCH_ELVEN_HALLS,
        BRANCH_LAIR, BRANCH_SWAMP, BRANCH_SLIME_PITS, BRANCH_SNAKE_PIT, 
        BRANCH_HIVE,
        BRANCH_VAULTS, BRANCH_HALL_OF_BLADES, BRANCH_CRYPT, BRANCH_TOMB,
        BRANCH_VESTIBULE_OF_HELL,
        BRANCH_DIS, BRANCH_GEHENNA, BRANCH_COCYTUS, BRANCH_TARTARUS, 
        BRANCH_HALL_OF_ZOT
    };

    disp += "                    <white>Overview of the Dungeon</white>\n" ;

    // print branches
    int branchcount = 0;
    for (unsigned int i = 1; i < sizeof(list_order)/sizeof(branch_type); ++i)
    {
        const branch_type branch = list_order[i];
        if ( stair_level.find(branch) != stair_level.end() )
        {
            if ( !branchcount )
            {
                disp += "\n<white>Branches:</white>\n";
                seen_anything = true;
            }
            
            ++branchcount;

            snprintf(buffer, sizeof buffer, "<yellow>%-6s</yellow>: %-7s",
                     branch_name(branch, true).c_str(),
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
    
    // print altars
    // we loop through everything a dozen times, oh well
    if ( !altars_present.empty() )
    {
        disp += "\n<white>Altars:</white>\n";
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

        for ( ci_altar = altars_present.begin();
              ci_altar != altars_present.end();
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
        disp += "\n<white>Shops:</white>\n";
        seen_anything = true;
    }
    last_id.depth = 10000;
    std::map<level_pos, shop_type>::const_iterator ci_shops;
    int placecount = 0;
    for ( ci_shops = shops_present.begin();
          ci_shops != shops_present.end();
          ++ci_shops )
    {
        if ( ci_shops->first.id != last_id )
        {
            if ( placecount )
            {
                // there are at most 5 shops per level, plus 7 chars for
                // the level name, plus 4 for the spacing; that makes
                // a total of 16 chars per shop or exactly 5 per line.
                if ( placecount % 5 == 0 )
                    disp += "\n";
                else
                    disp += "  ";
            }
            ++placecount;
            disp += "<brown>";
            disp += ci_shops->first.id.describe(false, true);
            disp += "</brown>";
            disp += ": ";
            last_id = ci_shops->first.id;
        }
        disp += shoptype_to_char(ci_shops->second);
    }
    disp += "\n";

    // print portals
    if ( !portals_present.empty() )
    {
        disp += "\n<white>Portals:</white>\n";
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
        disp += "You haven't discovered anything interesting yet.";

    return disp;
}

void display_overmap()
{
    std::string disp = overview_description_string();
    linebreak_string(disp, get_number_of_cols() - 5, get_number_of_cols() - 1);
    formatted_scroller(MF_EASY_EXIT | MF_ANYPRINTABLE | MF_NOSELECT,
                       disp).show();
    redraw_screen();
}

void seen_staircase( unsigned char which_staircase, const coord_def& pos )
{
    // which_staircase holds the grid value of the stair, must be converted
    // Only handles stairs, not gates or arches
    // Don't worry about:
    //   - stairs returning to dungeon - predictable
    //   - entrances to the hells - always in vestibule

    branch_type which_branch = BRANCH_MAIN_DUNGEON;

    switch ( which_staircase )
    {
    case DNGN_ENTER_ORCISH_MINES:
        which_branch = BRANCH_ORCISH_MINES;
        break;
    case DNGN_ENTER_HIVE:
        which_branch = BRANCH_HIVE;
        break;
    case DNGN_ENTER_LAIR:
        which_branch = BRANCH_LAIR;
        break;
    case DNGN_ENTER_SLIME_PITS:
        which_branch = BRANCH_SLIME_PITS;
        break;
    case DNGN_ENTER_VAULTS:
        which_branch = BRANCH_VAULTS;
        break;
    case DNGN_ENTER_CRYPT:
        which_branch = BRANCH_CRYPT;
        break;
    case DNGN_ENTER_HALL_OF_BLADES:
        which_branch = BRANCH_HALL_OF_BLADES;
        break;
    case DNGN_ENTER_ZOT:
        which_branch = BRANCH_HALL_OF_ZOT;
        break;
    case DNGN_ENTER_TEMPLE:
        which_branch = BRANCH_ECUMENICAL_TEMPLE;
        break;
    case DNGN_ENTER_SNAKE_PIT:
        which_branch = BRANCH_SNAKE_PIT;
        break;
    case DNGN_ENTER_ELVEN_HALLS:
        which_branch = BRANCH_ELVEN_HALLS;
        break;
    case DNGN_ENTER_TOMB:
        which_branch = BRANCH_TOMB;
        break;
    case DNGN_ENTER_SWAMP:
        which_branch = BRANCH_SWAMP;
        break;
    case DNGN_ENTER_DIS:
        which_branch = BRANCH_DIS;
        break;
    case DNGN_ENTER_GEHENNA:
        which_branch = BRANCH_GEHENNA;
        break;
    case DNGN_ENTER_COCYTUS:
        which_branch = BRANCH_COCYTUS;
        break;
    case DNGN_ENTER_TARTARUS:
        which_branch = BRANCH_TARTARUS;
        break;
    default:
        break;
    }

    ASSERT(which_branch != BRANCH_MAIN_DUNGEON);

    stair_level[which_branch] = level_id::get_current_level_id();
}

// if player has seen an altar; record it
void seen_altar( god_type god, const coord_def& pos )
{
    // can't record in abyss or pan.
    if ( you.level_type != LEVEL_DUNGEON )
        return;
    
    // no point in recording Temple altars
    if ( you.where_are_you == BRANCH_ECUMENICAL_TEMPLE )
        return;

    // portable; no point in recording
    if ( god == GOD_NEMELEX_XOBEH )
        return;

    level_pos where(level_id::get_current_level_id(), pos);
    altars_present[where] = god;
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
void seen_other_thing( unsigned char which_thing, const coord_def& pos )
{
    if ( you.level_type != LEVEL_DUNGEON ) // can't record in abyss or pan.
        return;

    level_pos where(level_id::get_current_level_id(), pos);

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
