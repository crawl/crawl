/*
 *  File:       overmap.cc
 *  Idea:       allow player to make notes about levels. I don't know how
 *              to do this (I expect it will require some kind of dynamic
 *              memory management thing). - LH
 *  Summary:    Records location of stairs etc
 *  Written by: Linley Henzell
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
// for #definitions of MAX_BRANCHES & MAX_LEVELS
#include "religion.h"
#include "stuff.h"
#include "view.h"

enum
{
     NO_FEATURE          = 0x00,
     FEATURE_SHOP        = 0x01,
     FEATURE_LABYRINTH   = 0x02,
     FEATURE_HELL        = 0x04,
     FEATURE_ABYSS       = 0x08,
     FEATURE_PANDEMONIUM = 0x10
};

// These variables need to become part of the player struct
// and need to be stored in the .sav file:

//   0 == no altars;
// 100 == more than one altar; or
//   # == which god for remaining numbers.

FixedArray<unsigned char, MAX_LEVELS, MAX_BRANCHES> altars_present;
FixedVector<char, MAX_BRANCHES> stair_level;
FixedArray<unsigned char, MAX_LEVELS, MAX_BRANCHES> feature;

int map_lines = 0; //mv: number of lines already printed on "over-map" screen

//mv: prints one line in specified colour
// void print_one_map_line( const char *line, int colour );
// void print_branch_entrance_line( const char *area );

void print_one_simple_line( const char *line, int colour );
void print_one_highlighted_line( const char *pre, const char *text, 
                                 const char *post, int colour );

static void print_level_name( int branch, int depth, 
                              bool &printed_branch, bool &printed_level );

void init_overmap( void )
{
    for (int i = 0; i < MAX_LEVELS; i++)
    {
        for (int j = 0; j < MAX_BRANCHES; j++)
        {
            altars_present[i][j] = 0;
            feature[i][j] = 0;
        }
    }

    for (int i = 0; i < MAX_BRANCHES; i++)
        stair_level[i] = -1;
}          // end init_overmap()

void display_overmap( void )
{
#ifdef DOS_TERM
    char buffer[4800];

    window(1, 1, 80, 25);
    gettext(1, 1, 80, 25, buffer);
#endif

    //mv: must be set to 0 so "More..." message appears really at the
    // bottom of the screen
    //Don't forget it could be changed since the last call of display_overmap
    map_lines = 0;

    clrscr();
    bool pr_lev = false;
    bool output = false;

    print_one_simple_line("                            Overview of the Dungeon", WHITE);

    // This is a more sensible order than the order of the enums -- bwr
    const int list_order[] = 
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

    for (unsigned int index = 0; index < sizeof(list_order) / sizeof(int); index++)
    {
        const int i = list_order[index];
        bool printed_branch = false;

        for (int j = 0; j < MAX_LEVELS; j++)
        {
            bool printed_level = false;

            if (altars_present[j][i] != 0)
            {
                print_level_name( i, j, printed_branch, printed_level );
                output = true;

                if (altars_present[j][i] == 100)
                {
                    print_one_highlighted_line( "    - some ", 
                                                "altars to the gods", ".", 
                                                WHITE );
                }
                else
                {
                    snprintf( info, INFO_SIZE, "altar to %s", 
                              god_name( altars_present[j][i] ) );
                              
                    print_one_highlighted_line( "    - an ", info, ".", WHITE );
                }
            }

            if ( (feature[j][i] & FEATURE_SHOP) )
            {
                print_level_name( i, j, printed_branch, printed_level );

                // print_one_simple_line("    - facilities for the purchase of goods.",LIGHTGREY);

                print_one_highlighted_line( "    - facilities for the ", 
                                            "purchase of goods", ".", LIGHTGREEN );
                output = true;
            }

            if ( (feature[j][i] & FEATURE_ABYSS) )
            {
                print_level_name( i, j, printed_branch, printed_level );
                // print_one_simple_line("    - a gateway into the Abyss.", LIGHTRED);
                print_one_highlighted_line( "    - a gateway into ", 
                                            "the Abyss", ".", MAGENTA );
                output = true;
            }

            if ( (feature[j][i] & FEATURE_PANDEMONIUM) )
            {
                print_level_name( i, j, printed_branch, printed_level );
                // print_one_simple_line("    - a link to Pandemonium.", LIGHTRED);

                print_one_highlighted_line( "    - a link to ", "Pandemonium", 
                                            ".", LIGHTBLUE );
                output = true;
            }

            if ( (feature[j][i] & FEATURE_HELL) )
            {
                print_level_name( i, j, printed_branch, printed_level );
                // print_one_simple_line("    - a mouth of Hell.", LIGHTRED);
                print_one_highlighted_line( "    - a mouth of ", "Hell", ".", RED );
                output = true;
            }

            if ( (feature[j][i] & FEATURE_LABYRINTH) )
            {
                print_level_name( i, j, printed_branch, printed_level );
                // print_one_simple_line("    - the entrance of a Labyrinth.", LIGHTRED);
                print_one_highlighted_line( "    - an entrance to ", 
                                            "a Labyrinth", ".", CYAN );
                output = true;
            }


            // NB: k starts at 1 because there aren't any staircases
            // to the main dungeon
            for (int k = 1; k < MAX_BRANCHES; k++)
            {
                pr_lev = false;
                // strcpy(info, "    - a staircase leading to ");
                info[0] = '\0';

                if (stair_level[k] == j)
                {
                    switch (i)
                    {
                    case BRANCH_LAIR:
                        switch (k)
                        {
                        case BRANCH_SLIME_PITS:
                            strcat(info, "the Slime Pits");
                            pr_lev = true;
                            break;
                        case BRANCH_SNAKE_PIT:
                            strcat(info, "the Snake Pit");
                            pr_lev = true;
                            break;
                        case BRANCH_SWAMP:
                            strcat(info, "the Swamp");
                            pr_lev = true;
                            break;
                        }
                        break;

                    case BRANCH_VAULTS:
                        switch (k)
                        {
                        case BRANCH_HALL_OF_BLADES:
                            strcat(info, "the Hall of Blades");
                            pr_lev = true;
                            break;
                        case BRANCH_CRYPT:
                            strcat(info, "the Crypt");
                            pr_lev = true;
                            break;
                        }
                        break;

                    case BRANCH_CRYPT:
                        switch (k)
                        {
                        case BRANCH_TOMB:
                            strcat(info, "the Tomb");
                            pr_lev = true;
                            break;
                        }
                        break;

                    case BRANCH_ORCISH_MINES:
                        switch (k)
                        {
                        case BRANCH_ELVEN_HALLS:
                            strcat(info, "the Elven Halls");
                            pr_lev = true;
                            break;
                        }
                        break;

                    case BRANCH_MAIN_DUNGEON:
                        switch (k)
                        {
                        case BRANCH_ORCISH_MINES:
                            strcat(info, "the Orcish Mines");
                            pr_lev = true;
                            break;
                        case BRANCH_HIVE:
                            strcat(info, "the Hive");
                            pr_lev = true;
                            break;
                        case BRANCH_LAIR:
                            strcat(info, "the Lair");
                            pr_lev = true;
                            break;
                        case BRANCH_VAULTS:
                            strcat(info, "the Vaults");
                            pr_lev = true;
                            break;
                        case BRANCH_HALL_OF_ZOT:
                            strcat(info, "the Hall of Zot");
                            pr_lev = true;
                            break;
                        case BRANCH_ECUMENICAL_TEMPLE:
                            strcat(info, "the Ecumenical Temple");
                            pr_lev = true;
                            break;
                        }
                        break;
                    }
                }

                if (pr_lev)
                {
                    print_level_name( i, j, printed_branch, printed_level );
                    print_one_highlighted_line( "    - the entrance to ", info,
                                                ".", YELLOW );
                    output = true;
                }
            }
        }
    }

    textcolor( LIGHTGREY );

    if (!output)
        cprintf( EOL "You have yet to discover anything worth noting." EOL );

    getch();

    redraw_screen();

#ifdef DOS_TERM
    puttext(1, 1, 80, 25, buffer);
#endif
}          // end display_overmap()


static void print_level_name( int branch, int depth, 
                              bool &printed_branch, bool &printed_level )
{
    if (!printed_branch)
    {
        printed_branch = true;

        print_one_simple_line( "", YELLOW );
        print_one_simple_line( 
                (branch == BRANCH_MAIN_DUNGEON)      ? "Main Dungeon" :
                (branch == BRANCH_ORCISH_MINES)      ? "The Orcish Mines" :
                (branch == BRANCH_HIVE)              ? "The Hive" :
                (branch == BRANCH_LAIR)              ? "The Lair" :
                (branch == BRANCH_SLIME_PITS)        ? "The Slime Pits" :
                (branch == BRANCH_VAULTS)            ? "The Vaults" :
                (branch == BRANCH_CRYPT)             ? "The Crypt" :
                (branch == BRANCH_HALL_OF_BLADES)    ? "The Hall of Blades" :
                (branch == BRANCH_HALL_OF_ZOT)       ? "The Hall of Zot" :
                (branch == BRANCH_ECUMENICAL_TEMPLE) ? "The Ecumenical Temple" :
                (branch == BRANCH_SNAKE_PIT)         ? "The Snake Pit" :
                (branch == BRANCH_ELVEN_HALLS)       ? "The Elven Halls" :
                (branch == BRANCH_TOMB)              ? "The Tomb" :
                (branch == BRANCH_SWAMP)             ? "The Swamp" :

                (branch == BRANCH_DIS)               ? "The Iron City of Dis" :
                (branch == BRANCH_GEHENNA)           ? "Gehenna" :
                (branch == BRANCH_VESTIBULE_OF_HELL) ? "The Vestibule of Hell" :
                (branch == BRANCH_COCYTUS)           ? "Cocytus" :
                (branch == BRANCH_TARTARUS)          ? "Tartarus" 
                                                     : "Unknown Area",
        
                YELLOW );
    }

    if (!printed_level)
    {
        printed_level = true;

        if (branch == BRANCH_ECUMENICAL_TEMPLE
            || branch == BRANCH_HALL_OF_BLADES
            || branch == BRANCH_VESTIBULE_OF_HELL)
        {
            // these areas only have one level... let's save the space
            return;
        }

        // we need our own buffer in here (info is used):
        char buff[ INFO_SIZE ] = "\0";;

        if (branch == BRANCH_MAIN_DUNGEON)
            depth += 1;
        else if (branch >= BRANCH_ORCISH_MINES && branch <= BRANCH_SWAMP)
            depth -= you.branch_stairs[ branch - BRANCH_ORCISH_MINES ];
        else // branch is in hell (all of which start at depth 28)
            depth -= 26;

        snprintf( buff, INFO_SIZE, "  Level %d:", depth );
        print_one_simple_line( buff, LIGHTRED );
    }
}

void seen_staircase( unsigned char which_staircase )
{
    // which_staircase holds the grid value of the stair, must be converted
    // Only handles stairs, not gates or arches
    // Don't worry about:
    //   - stairs returning to dungeon - predictable
    //   - entrances to the hells - always in vestibule

    unsigned char which_branch = BRANCH_MAIN_DUNGEON;

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
    default:
        exit(-1);               // shouldn't happen
    }

    stair_level[which_branch] = you.your_level;
}          // end seen_staircase()


// if player has seen an altar; record it
void seen_altar( unsigned char which_altar )
{
    // can't record in abyss or pan.
    if ( you.level_type != LEVEL_DUNGEON )
        return;

    // portable; no point in recording
    if ( which_altar == GOD_NEMELEX_XOBEH )
        return;

    // already seen
    if ( altars_present[you.your_level][you.where_are_you] == which_altar )
        return;

    if ( altars_present[you.your_level][you.where_are_you] == 0 )
        altars_present[you.your_level][you.where_are_you] = which_altar;
    else
        altars_present[you.your_level][you.where_are_you] = 100;
}          // end seen_altar()


// if player has seen any other thing; record it
void seen_other_thing( unsigned char which_thing )
{
    if ( you.level_type != LEVEL_DUNGEON )     // can't record in abyss or pan.
        return;

    switch ( which_thing )
    {
    case DNGN_ENTER_SHOP:
        feature[you.your_level][you.where_are_you] |= FEATURE_SHOP;
        break;
    case DNGN_ENTER_LABYRINTH:
        feature[you.your_level][you.where_are_you] |= FEATURE_LABYRINTH;
        break;
    case DNGN_ENTER_HELL:
        feature[you.your_level][you.where_are_you] |= FEATURE_HELL;
        break;
    case DNGN_ENTER_ABYSS:
        feature[you.your_level][you.where_are_you] |= FEATURE_ABYSS;
        break;
    case DNGN_ENTER_PANDEMONIUM:
        feature[you.your_level][you.where_are_you] |= FEATURE_PANDEMONIUM;
        break;
    }
}          // end seen_other_thing()


/* mv: this function prints one line at "Over-map screen" in specified colour.
 * If map_lines = maximum number of lines (it means the screen is full) it
 * prints "More..." message, read key, clear screen and after that prints new
 * line
 */
void print_one_simple_line( const char *line , int colour)
{
    if (map_lines == (get_number_of_lines() - 2))
    {
        textcolor( LIGHTGREY );
        cprintf(EOL);
        cprintf("More...");
        getch();
        clrscr();
        map_lines = 0;
    }

    textcolor( colour );
    cprintf( line );
    cprintf( EOL );

    map_lines++;
}

void print_one_highlighted_line( const char *pre, const char *text, 
                                 const char *post, int colour )
{
    if (map_lines == (get_number_of_lines() - 2))
    {
        textcolor( LIGHTGREY );
        cprintf(EOL);
        cprintf("More...");
        getch();
        clrscr();
        map_lines = 0;
    }

    if (pre[0] != '\0')
    {
        textcolor( LIGHTGREY );
        cprintf( pre );
    }

    textcolor( colour );
    cprintf( text );

    if (post[0] != '\0')
    {
        textcolor( LIGHTGREY );
        cprintf( post );
    }

    cprintf( EOL );

    map_lines++;
}
