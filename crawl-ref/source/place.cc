/*
 *  File:       place.cc
 *  Summary:    Place related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "externs.h"
#include "place.h"

#include "branch.h"
#include "travel.h"

// Do not attempt to use level_id if level_type != LEVEL_DUNGEON
std::string short_place_name(level_id id)
{
    return id.describe();
}

int place_branch(unsigned short place)
{
    const unsigned branch = (unsigned) ((place >> 8) & 0xFF);
    const int lev = place & 0xFF;
    return lev == 0xFF? -1 : (int) branch;
}

int place_depth(unsigned short place)
{
    const int lev = place & 0xFF;
    return lev == 0xFF? -1 : lev;
}

int place_type(unsigned short place)
{
    const unsigned type = (unsigned) ((place >> 8) & 0xFF);
    const int lev = place & 0xFF;
    return lev == 0xFF? (int) type : (int) LEVEL_DUNGEON;
}

unsigned short get_packed_place( branch_type branch, int subdepth,
                                 level_area_type level_type )
{
    unsigned short place = (unsigned short)
        ( (static_cast<int>(branch) << 8) | (subdepth & 0xFF) );

    if (level_type != LEVEL_DUNGEON)
        place = (unsigned short) ( (static_cast<int>(level_type) << 8) | 0xFF );

    return place;
}

unsigned short get_packed_place()
{
    return get_packed_place(you.where_are_you,
                            subdungeon_depth(you.where_are_you, you.your_level),
                            you.level_type);
}

bool single_level_branch( branch_type branch )
{
    return (branch >= 0 && branch < NUM_BRANCHES
            && branches[branch].depth == 1);
}

std::string place_name( unsigned short place, bool long_name,
                        bool include_number )
{

    unsigned char branch = (unsigned char) ((place >> 8) & 0xFF);
    int lev = place & 0xFF;

    std::string result;
    if (lev == 0xFF)
    {
        switch (branch)
        {
        case LEVEL_ABYSS:
            return ( long_name ? "The Abyss" : "Abyss" );
        case LEVEL_PANDEMONIUM:
            return ( long_name ? "Pandemonium" : "Pan" );
        case LEVEL_LABYRINTH:
            return ( long_name ? "a Labyrinth" : "Lab" );
        case LEVEL_PORTAL_VAULT:
            // XXX: Using level_type_name here is strictly evil, but
            // packed places lack the information needed for pretty-printing
            // place names for portal vaults, so we must use this out-of-band
            // information.
            if (branch == you.level_type
                && !you.level_type_name.empty())
            {
                return long_name
                    ? article_a(you.level_type_name)
                    : upcase_first(you.level_type_name_abbrev);
            }
            else
            {
                return long_name ? "a Portal Chamber" : "Port";
            }
        default:
            return ( long_name ? "Buggy Badlands" : "Bug" );
        }
    }

    result = (long_name ?
              branches[branch].longname : branches[branch].abbrevname);

    if (include_number && branches[branch].depth != 1)
    {
        char buf[200];
        if (long_name)
        {
            // decapitalize 'the'
            if ( result.find("The") == 0 )
                result[0] = 't';
            snprintf( buf, sizeof buf, "Level %d of %s",
                      lev, result.c_str() );
        }
        else if (lev)
            snprintf( buf, sizeof buf, "%s:%d", result.c_str(), lev );
        else
            snprintf( buf, sizeof buf, "%s:$", result.c_str() );

        result = buf;
    }
    return result;
}

// Takes a packed 'place' and returns a compact stringified place name.
// XXX: This is done in several other places; a unified function to
//      describe places would be nice.
std::string short_place_name(unsigned short place)
{
    return place_name( place, false, true );
}

// Prepositional form of branch level name.  For example, "in the
// Abyss" or "on level 3 of the Main Dungeon".
std::string prep_branch_level_name(unsigned short packed_place)
{
    std::string place = place_name( packed_place, true, true );
    if (place.length() && place != "Pandemonium")
        place[0] = tolower(place[0]);
    return (place.find("level") == 0 ? "on " + place
                                     : "in " + place);
}

// Use current branch and depth
std::string prep_branch_level_name()
{
    return prep_branch_level_name( get_packed_place() );
}

int absdungeon_depth(branch_type branch, int subdepth)
{
    if (branch >= BRANCH_VESTIBULE_OF_HELL && branch <= BRANCH_LAST_HELL)
        return subdepth + 27 - (branch == BRANCH_VESTIBULE_OF_HELL);
    else
    {
        --subdepth;
        while (branch != BRANCH_MAIN_DUNGEON)
        {
            subdepth += branches[branch].startdepth;
            branch = branches[branch].parent_branch;
        }
    }
    return subdepth;
}

int subdungeon_depth(branch_type branch, int depth)
{
    return depth - absdungeon_depth(branch, 0);
}

int player_branch_depth()
{
    return subdungeon_depth(you.where_are_you, you.your_level);
}

// Returns true if exits from this type of level involve going upstairs.
bool level_type_exits_up(level_area_type type)
{
    return (type == LEVEL_LABYRINTH || type == LEVEL_PORTAL_VAULT);
}

bool level_type_exits_down(level_area_type type)
{
    return (type == LEVEL_PANDEMONIUM || type == LEVEL_ABYSS);
}

bool level_type_allows_followers(level_area_type type)
{
    return (type == LEVEL_DUNGEON || type == LEVEL_PANDEMONIUM);
}

bool level_type_is_stash_trackable(level_area_type type)
{
    return (type != LEVEL_ABYSS && type != LEVEL_LABYRINTH);
}
