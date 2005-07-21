/*
 *  File:       view.cc
 *  Summary:    Misc function used to render the dungeon.
 *  Written by: Linley Henzell
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

#include "debug.h"
#include "insult.h"
#include "macro.h"
#include "monstuff.h"
#include "mon-util.h"
#include "overmap.h"
#include "player.h"
#include "skills2.h"
#include "stuff.h"
#include "spells4.h"


unsigned char your_sign;        // accessed as extern in transfor.cc and acr.cc
unsigned char your_colour;      // accessed as extern in transfor.cc and acr.cc

FixedArray < unsigned int, 20, 19 > show_backup;

unsigned char show_green;
extern int stealth;             // defined in acr.cc
extern FixedVector<char, 10>  Visible_Statue;        // defined in acr.cc

// char colour_code_map(unsigned char map_value);
char colour_code_map( int x, int y );

unsigned char (*mapch) (unsigned char);
unsigned char (*mapch2) (unsigned char);
unsigned char mapchar(unsigned char ldfk);
unsigned char mapchar2(unsigned char ldfk);
unsigned char mapchar3(unsigned char ldfk);
unsigned char mapchar4(unsigned char ldfk);
void cloud_grid(void);
void monster_grid(bool do_updates);

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
#ifdef LINUX
    return (get_number_of_lines_from_curses());
#elif MAC
    return (MAC_NUMBER_OF_LINES);
#else
    return (25);
#endif
}


//---------------------------------------------------------------
//
// get_ibm_symbol
//
// Returns the DOS character code and color for everything drawn
// with the IBM graphics option.
//
//---------------------------------------------------------------
static void get_ibm_symbol(unsigned int object, unsigned short *ch,
                           unsigned short *color)
{
    ASSERT(color != NULL);
    ASSERT(ch != NULL);

    switch (object)
    {
    case DNGN_UNSEEN:
        *ch = 0;
        break;

    case DNGN_ROCK_WALL:
    case DNGN_PERMAROCK_WALL:
        *color = env.rock_colour;
        *ch = 177;
        break;                  // remember earth elementals

    // stone in the realm of Zot is coloured the same as rock
    case DNGN_STONE_WALL:
        *color = (player_in_branch( BRANCH_HALL_OF_ZOT ) ? env.rock_colour
                                                         : LIGHTGREY);
        *ch = 177;
        break;

    case DNGN_CLOSED_DOOR:
        *ch = 254;
        break;

    case DNGN_METAL_WALL:
        *ch = 177;
        *color = CYAN;
        break;

    case DNGN_SECRET_DOOR:
        *ch = 177;
        *color = env.rock_colour;
        break;

    case DNGN_GREEN_CRYSTAL_WALL:
        *ch = 177;
        *color = GREEN;
        break;

    case DNGN_ORCISH_IDOL:
        *ch = '8';
        *color = DARKGREY;
        break;

    case DNGN_WAX_WALL:
        *ch = 177;
        *color = YELLOW;
        break;                  // wax wall
        /* Anything added here must also be added to the PLAIN_TERMINAL
           viewwindow2 below */

    case DNGN_SILVER_STATUE:
        *ch = '8';
        *color = WHITE;
        Visible_Statue[ STATUE_SILVER ] = 1;
        break;

    case DNGN_GRANITE_STATUE:
        *ch = '8';
        *color = LIGHTGREY;
        break;

    case DNGN_ORANGE_CRYSTAL_STATUE:
        *ch = '8';
        *color = LIGHTRED;
        Visible_Statue[ STATUE_ORANGE_CRYSTAL ] = 1;
        break;

    case DNGN_LAVA:
        *ch = 247;
        *color = RED;
        break;

    case DNGN_DEEP_WATER:
        *ch = 247;          // this wavy thing also used for water elemental
        *color = BLUE;
        break;

    case DNGN_SHALLOW_WATER:
        *ch = 247;          // this wavy thing also used for water elemental
        *color = CYAN;
        break;

    case DNGN_FLOOR:
        *color = env.floor_colour;
        *ch = 249;
        break;

    case DNGN_ENTER_HELL:
        *ch = 239;
        *color = RED;
        seen_other_thing(object);
        break;

    case DNGN_OPEN_DOOR:
        *ch = 39;
        break;

    case DNGN_BRANCH_STAIRS:
        *ch = 240;
        *color = BROWN;
        break;

    case DNGN_TRAP_MECHANICAL:
        *color = LIGHTCYAN;
        *ch = 94;
        break;

    case DNGN_TRAP_MAGICAL:
        *color = MAGENTA;
        *ch = 94;
        break;

    case DNGN_TRAP_III:
        *color = LIGHTGREY;
        *ch = 94;
        break;

    case DNGN_UNDISCOVERED_TRAP:
        *ch = 249;
        *color = env.floor_colour;
        break;

    case DNGN_ENTER_SHOP:
        *ch = 239;
        *color = YELLOW;

        seen_other_thing(object);
        break;
    // if I change anything above here, must also change magic mapping!

    case DNGN_ENTER_LABYRINTH:
        *ch = 239;
        *color = LIGHTGREY;
        seen_other_thing(object);
        break;

    // not sure why we have "odd" here, but "ladders" are special in
    // that they all lead to the first staircase of the next level
    // (and returning from there will take you somewhere different)
    // ... that's why they're brown... it's a warning -- bwr
    case DNGN_ROCK_STAIRS_DOWN:
        *color = BROWN;         // ladder    // odd {dlb}
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
        *ch = '>';
        break;

    case DNGN_ROCK_STAIRS_UP:
        *color = BROWN;         // ladder    // odd {dlb}
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
        *ch = '<';
        break;

    case DNGN_ENTER_DIS:
        *color = CYAN;
        *ch = 239;
        break;

    case DNGN_ENTER_GEHENNA:
        *color = RED;
        *ch = 239;
        break;

    case DNGN_ENTER_COCYTUS:
        *color = LIGHTCYAN;
        *ch = 239;
        break;

    case DNGN_ENTER_TARTARUS:
        *color = DARKGREY;
        *ch = 239;
        break;

    case DNGN_ENTER_ABYSS:
        *color = random2(16);
        *ch = 239;
        seen_other_thing(object);
        break;

    case DNGN_EXIT_ABYSS:
        *color = random2(16);
        *ch = 239;
        break;

    case DNGN_STONE_ARCH:
        *color = LIGHTGREY;
        *ch = 239;
        break;

    case DNGN_ENTER_PANDEMONIUM:
        *color = LIGHTBLUE;
        *ch = 239;
        seen_other_thing(object);
        break;

    case DNGN_EXIT_PANDEMONIUM:
        *color = LIGHTBLUE;
        *ch = 239;
        break;

    case DNGN_TRANSIT_PANDEMONIUM:
        *color = LIGHTGREEN;
        *ch = 239;
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
    case 123:
    case 124:
    case 125:
    case 126:
        *color = YELLOW;
        *ch = '>';
        seen_staircase(object);
        break;

    case DNGN_ENTER_ZOT:
        *color = MAGENTA;
        *ch = 239;
        seen_staircase(object);
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
    case 143:
    case 144:
    case 145:
    case 146:
        *color = YELLOW;
        *ch = '<';
        break;

    case DNGN_RETURN_FROM_ZOT:
        *color = MAGENTA;
        *ch = 239;
        break;

    case DNGN_ALTAR_ZIN:
        *color = WHITE;
        *ch = 220;
        seen_altar(GOD_ZIN);
        break;

    case DNGN_ALTAR_SHINING_ONE:
        *color = YELLOW;
        *ch = 220;
        seen_altar(GOD_SHINING_ONE);
        break;

    case DNGN_ALTAR_KIKUBAAQUDGHA:
        *color = DARKGREY;
        *ch = 220;
        seen_altar(GOD_KIKUBAAQUDGHA);
        break;

    case DNGN_ALTAR_YREDELEMNUL:
        *color = ((one_chance_in(3)) ? RED : DARKGREY);
        *ch = 220;
        seen_altar(GOD_YREDELEMNUL);
        break;

    case DNGN_ALTAR_XOM:
        *color = random_colour();
        *ch = 220;
        seen_altar(GOD_XOM);
        break;

    case DNGN_ALTAR_VEHUMET:
        *color = LIGHTBLUE;
        if (one_chance_in(3))
            *color = LIGHTMAGENTA;
        if (one_chance_in(3))
            *color = LIGHTRED;
        *ch = 220;
        seen_altar(GOD_VEHUMET);
        break;

    case DNGN_ALTAR_OKAWARU:
        *color = CYAN;
        *ch = 220;
        seen_altar(GOD_OKAWARU);
        break;

    case DNGN_ALTAR_MAKHLEB:
        *color = RED;
        if (one_chance_in(3))
            *color = LIGHTRED;
        if (one_chance_in(3))
            *color = YELLOW;
        *ch = 220;
        seen_altar(GOD_MAKHLEB);
        break;

    case DNGN_ALTAR_SIF_MUNA:
        *color = BLUE;
        *ch = 220;
        seen_altar(GOD_SIF_MUNA);
        break;

    case DNGN_ALTAR_TROG:
        *color = RED;
        *ch = 220;
        seen_altar(GOD_TROG);
        break;

    case DNGN_ALTAR_NEMELEX_XOBEH:
        *color = LIGHTMAGENTA;
        *ch = 220;
        seen_altar(GOD_NEMELEX_XOBEH);
        break;

    case DNGN_ALTAR_ELYVILON:
        *color = LIGHTGREY;
        *ch = 220;
        seen_altar(GOD_ELYVILON);
        break;

    case DNGN_BLUE_FOUNTAIN:
        *color = BLUE;
        *ch = 159;
        break;

    case DNGN_SPARKLING_FOUNTAIN:
        *color = LIGHTBLUE;
        *ch = 159;
        break;

    case DNGN_DRY_FOUNTAIN_I:
    case DNGN_DRY_FOUNTAIN_II:
    case DNGN_PERMADRY_FOUNTAIN:
        *color = LIGHTGREY;
        *ch = 159;
        break;

    case 256:
        *ch = '0';
        break;

    case 257:
        *color = CYAN;
        *ch = '~';
        break;                  /* Invis creature walking through water */

    case 258:
        *ch = ')';
        break;                  // weapon )

    case 259:
        *ch = '[';
        break;                  // armour [

    case 260:
        *ch = '/';
        break;                  // wands, etc.

    case 261:
        *ch = '%';
        break;                  // food

    case 262:
        *ch = '+';
        break;                  // books +

    case 263:
        *ch = '?';
        break;                  // scroll ?

    case 264:
        *ch = '=';
        break;                  // ring = etc

    case 265:
        *ch = '!';
        break;                  // potions !

    case 266:
        *ch = '(';
        break;                  // stones

    case 267:
        *ch = '+';
        break;                  // book +

    case 268:
        *ch = '%';
        break;                  // corpses part 1

    case 269:
        *ch = '\\';
        break;                  // magical staves

    case 270:
        *ch = '}';
        break;                  // gems

    case 271:
        *ch = '%';
        break;                  // don't know ?

    case 272:
        *ch = '$';
        break;                  // $ gold

    case 273:
        *ch = '"';
        break;                  // amulet

    default:
        *ch = ((object >= 297) ? mons_char(object - 297) : object);
        break;
    }
}                               // end get_ibm_symbol()


//---------------------------------------------------------------
//
// viewwindow2
//
// Draws the main window using the extended IBM character set.
//
// This function should not interfer with the game condition,
// unless do_updates is set (ie.  stealth checks for visible
// monsters).
//
//---------------------------------------------------------------
void viewwindow2(char draw_it, bool do_updates)
{
    const long BUFFER_SIZE = 1550;
#ifdef DOS_TERM
    // DOS functions like gettext() and puttext() can only
    // work with arrays of characters, not shorts.
    FixedVector < unsigned char, BUFFER_SIZE > buffy;   //[800]; //392];
#else
    FixedVector < unsigned short, BUFFER_SIZE > buffy;   //[800]; //392];
#endif

    unsigned short ch, color;

    losight(env.show, grd, you.x_pos, you.y_pos);

    int count_x, count_y;

    for (count_x = 0; count_x < 18; count_x++)
    {
        for (count_y = 0; count_y < 18; count_y++)
        {
            env.show_col[count_x][count_y] = LIGHTGREY;
            show_backup[count_x][count_y] = 0;
        }
    }

    item();
    cloud_grid();
    monster_grid(do_updates);
    int bufcount = 0;

    if (draw_it == 1)
    {
        _setcursortype(_NOCURSOR);
        for (count_y = you.y_pos - 8; count_y < you.y_pos + 9; count_y++)
        {
            bufcount += 16;

            for (count_x = you.x_pos - 8; count_x < you.x_pos + 9; count_x++)
            {
                // may be overriden by the code below
                color = env.show_col[ count_x - you.x_pos + 9 ]
                                    [ count_y - you.y_pos + 9 ];

                unsigned int object = env.show[ count_x - you.x_pos + 9 ]
                                              [ count_y - you.y_pos + 9 ];

                get_ibm_symbol(object, &ch, &color);

                if (count_x == you.x_pos && count_y == you.y_pos)
                {
                    ch = your_sign;

                    if (player_is_swimming())
                    {
                        color = (grd[you.x_pos][you.y_pos] == DNGN_DEEP_WATER)
                                    ? BLUE : CYAN;
                    }
                    else
                    {
                        color = your_colour;
                    }
                }

                ASSERT(bufcount + 1 < BUFFER_SIZE);
                buffy[bufcount] = ch;
                buffy[bufcount + 1] = color;

                bufcount += 2;
            }

            bufcount += 16;
        }

        if (you.level_type != LEVEL_LABYRINTH && you.level_type != LEVEL_ABYSS)
        {
            bufcount = 0;

            for (count_y = 0; count_y < 17; count_y++)
            {
                bufcount += 16;

                for (count_x = 0; count_x < 17; count_x++)
                {
                    ASSERT(bufcount < BUFFER_SIZE);

                    if (buffy[bufcount] != 0)
                    {
                        env.map[ count_x + you.x_pos - 9 ]
                               [ count_y + you.y_pos - 9 ] = buffy[bufcount];
                    }

                    if (Options.clean_map == 1
                        && show_backup[count_x + 1][count_y + 1] != 0)
                    {
                        get_ibm_symbol(show_backup[count_x + 1][count_y + 1],
                                       &ch, &color);
                        env.map[ count_x + you.x_pos - 9 ]
                               [ count_y + you.y_pos - 9 ] = ch;
                    }
                    bufcount += 2;
                }
                bufcount += 16;
            }
        }

        bufcount = 0;
        for (count_y = 0; count_y < 17; count_y++)
        {
            for (count_x = 0; count_x < 33; count_x++)
            {
                if (count_x + you.x_pos - 17 < 3
                    || count_y + you.y_pos - 9 < 3
                    || count_x + you.x_pos - 14 > 77
                    || count_y + you.y_pos - 9 > 67)
                {
                    ASSERT(bufcount < BUFFER_SIZE);
                    buffy[bufcount] = 0;
                    bufcount++;
                    buffy[bufcount] = 0;
                    bufcount++;
                    continue;
                }

                if (count_x >= 8 && count_x <= 24 && count_y >= 0
                    && count_y <= 16 && buffy[bufcount] != 0)
                {
                    bufcount += 2;
                    continue;
                }

                ASSERT(bufcount + 1 < BUFFER_SIZE);

                buffy[bufcount] = env.map[ count_x + you.x_pos - 17 ]
                                         [ count_y + you.y_pos - 9 ];

                buffy[bufcount + 1] = DARKGREY;
                if (Options.colour_map)
                {
                    if (env.map[ count_x + you.x_pos - 16 ]
                               [ count_y + you.y_pos - 8 ] != 0)
                    {
                        buffy[bufcount + 1] 
                                = colour_code_map( count_x + you.x_pos - 17, 
                                                   count_y + you.y_pos - 9 );
                    }
                }
                bufcount += 2;
            }
        }

        if (you.berserker)
        {
            for (count_x = 1; count_x < 1400; count_x += 2)
            {
                if (buffy[count_x] != DARKGREY)
                    buffy[count_x] = RED;
            }
        }

        if (show_green != BLACK)
        {
            for (count_x = 1; count_x < 1400; count_x += 2)
            {
                if (buffy[count_x] != DARKGREY)
                    buffy[count_x] = show_green;
            }

            show_green = BLACK;

            if (you.special_wield == SPWLD_SHADOW)
                show_green = DARKGREY;
        }

#ifdef DOS_TERM
        puttext(2, 1, 34, 17, buffy.buffer());
#endif

#ifdef PLAIN_TERM
        gotoxy(2, 1);
        bufcount = 0;

        // following lines are purely optional.
        // if used,  players will 'jump' move.
        // Resting will be a LOT faster too.
        if (you.running == 0)
        {
            for (count_x = 0; count_x < 1120; count_x += 2)
            {                   // 1056
                ch = buffy[count_x];
                color = buffy[count_x + 1];
//                ASSERT(color < 16);
                ASSERT(ch < 255);

                textcolor(color);
                putch(ch);

                if (count_x % 66 == 64 && count_x > 0)
                    gotoxy(2, wherey() + 1);
            }
            // remember to comment out the line below if you comment out jump move.
        }
        _setcursortype(_NORMALCURSOR);
#endif
    }
}                               // end viewwindow2()

char colour_code_map( int x, int y )
{
    // XXX: Yes, the map array and the grid array are off by one. -- bwr
    const int map_value = env.map[x][y];
    const int grid_value = grd[x + 1][y + 1];

    // XXX: Yeah, this is ugly, but until we have stored layers in the
    // map we can't tell if we've seen a square, detected it, or just
    // detected the item or monster on top... giving colour here will
    // result in detect creature/item detecting features like stairs. -- bwr
    if (map_value != mapch2( grid_value ))
        return (DARKGREY);

    switch (grid_value)
    {
    case DNGN_TRAP_MECHANICAL:
        return (LIGHTCYAN);

    case DNGN_TRAP_MAGICAL:
    case DNGN_TRAP_III:
        return (MAGENTA);

    case DNGN_ENTER_SHOP:
        return (YELLOW);

    case DNGN_ENTER_DIS:
        return (CYAN);

    case DNGN_ENTER_HELL:
    case DNGN_ENTER_GEHENNA:
        return (RED);

    case DNGN_ENTER_COCYTUS:
        return (LIGHTCYAN);

    case DNGN_ENTER_ABYSS:
        return random2(16);     // so it can be black - is this right? {dlb}

    case DNGN_ENTER_LABYRINTH:
    case DNGN_STONE_ARCH:
        return (LIGHTGREY);

    case DNGN_ENTER_PANDEMONIUM:
        return (LIGHTBLUE);

    case DNGN_EXIT_PANDEMONIUM:
        // Exit pandemonium gates won't show up on the map as light blue
        // unless the character has the "gate to pandemonium" demonspawn 
        // mutation.  This is so that the player can't quickly use a 
        // crystal ball to find their way out.  -- bwr
        return (you.mutation[MUT_PANDEMONIUM] ? LIGHTBLUE : LIGHTGREEN);

    case DNGN_TRANSIT_PANDEMONIUM:
        return (LIGHTGREEN);

    case DNGN_ENTER_ZOT:
    case DNGN_RETURN_FROM_ZOT:
        return (MAGENTA);

    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
    case DNGN_ROCK_STAIRS_DOWN:
        return (RED);

    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
    case DNGN_ROCK_STAIRS_UP:
        return (BLUE);

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
    case 123:
    case 124:
    case 125:
    case 126:
        return (LIGHTRED);

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
    case 143:
    case 144:
    case 145:
    case 146:
        return (LIGHTBLUE);

    default:
        break;
    }

    return (DARKGREY);
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

                if (you.turn_is_over == 1
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

            if (!player_monster_visible( monster ))
            {
                // ripple effect?
                if (grd[monster->x][monster->y] == DNGN_SHALLOW_WATER
                    && !mons_flies(monster))
                {
                    show_backup[ monster->x - you.x_pos + 9 ]
                               [ monster->y - you.y_pos + 9]
                       = env.show[ monster->x - you.x_pos + 9 ]
                                 [ monster->y - you.y_pos + 9 ];
                    env.show[monster->x - you.x_pos + 9]
                            [monster->y - you.y_pos + 9] = 257;
                }
                continue;
            }
            else if (!mons_friendly( monster )
                     && !mons_is_mimic( monster->type )
                     && !mons_flag( monster->type, M_NO_EXP_GAIN )
                     && you.running > 0)
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
                    you.running = 0;
                else 
                    you.running--;
            }

            // mimics are always left on map
            if (!mons_is_mimic( monster->type ))
            {
                show_backup[monster->x - you.x_pos + 9]
                           [monster->y - you.y_pos + 9]
                       = env.show[monster->x - you.x_pos + 9]
                                 [monster->y - you.y_pos + 9];
            }

            env.show[monster->x - you.x_pos + 9]
                    [monster->y - you.y_pos + 9] = monster->type + 297;

            env.show_col[monster->x - you.x_pos + 9]
                        [monster->y - you.y_pos + 9]
                = ((mcolour[monster->type] == BLACK)
                        ? monster->number : mcolour[monster->type]);
#ifdef USE_COLOUR_OPTS
            if (mons_friendly(monster))
            {
                env.show_col[monster->x - you.x_pos + 9]
                            [monster->y - you.y_pos + 9]
                    |= COLFLAG_FRIENDLY_MONSTER;
            }
#endif
        }                       // end "if (monster->type != -1 && mons_ner)"
    }                           // end "for s"
}                               // end monster_grid()


bool check_awaken(int mons_aw)
{
    int mons_perc = 0;
    struct monsters *monster = &menv[mons_aw];
    const int mon_holy = mons_holiness( monster->type );

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


void item()
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
                    if (env.show[count_x - you.x_pos + 9]
                                [count_y - you.y_pos + 9] != 0)
                    {
                        if (grd[count_x][count_y] == DNGN_SHALLOW_WATER)
                        {
                            env.show_col[count_x - you.x_pos + 9]
                                        [count_y - you.y_pos + 9] = CYAN;
                        }
                        else
                        {
                            env.show_col[count_x - you.x_pos + 9]
                                        [count_y - you.y_pos + 9]
                                    = mitm[igrd[count_x][count_y]].colour;
                        }

                        switch (mitm[igrd[count_x][count_y]].base_type)
                        {
                        case OBJ_ORBS:
                            env.show[count_x - you.x_pos + 9]
                                    [count_y - you.y_pos + 9] = 256;
                            break;
                            // need + 6 because show is 0 - 12, not -6 - +6
                        case OBJ_WEAPONS:
                        case OBJ_MISSILES:
                            env.show[count_x - you.x_pos + 9]
                                    [count_y - you.y_pos + 9] = 258;
                            break;
                        case OBJ_ARMOUR:
                            env.show[count_x - you.x_pos + 9]
                                    [count_y - you.y_pos + 9] = 259;
                            break;
                        case OBJ_WANDS:
                            env.show[count_x - you.x_pos + 9]
                                    [count_y - you.y_pos + 9] = 260;
                            break;
                        case OBJ_FOOD:
                            env.show[count_x - you.x_pos + 9]
                                    [count_y - you.y_pos + 9] = 261;
                            break;
                        case OBJ_UNKNOWN_I:
                            env.show[count_x - you.x_pos + 9]
                                    [count_y - you.y_pos + 9] = 262;
                            break;
                        case OBJ_SCROLLS:
                            env.show[count_x - you.x_pos + 9]
                                    [count_y - you.y_pos + 9] = 263;
                            break;

                        case OBJ_JEWELLERY:
                            if (mitm[igrd[count_x][count_y]].sub_type >= AMU_RAGE)
                            {
                                env.show[count_x - you.x_pos + 9]
                                        [count_y - you.y_pos + 9] = 273;
                            }
                            else
                            {
                                env.show[count_x - you.x_pos + 9]
                                        [count_y - you.y_pos + 9] = 264;
                            }
                            break;

                        case OBJ_POTIONS:
                            env.show[count_x - you.x_pos + 9]
                                    [count_y - you.y_pos + 9] = 265;
                            break;
                        case OBJ_UNKNOWN_II:
                            env.show[count_x - you.x_pos + 9]
                                    [count_y - you.y_pos + 9] = 266;
                            break;
                        case OBJ_BOOKS:
                            env.show[count_x - you.x_pos + 9]
                                    [count_y - you.y_pos + 9] = 267;
                            break;
                        case OBJ_STAVES:
                            env.show[count_x - you.x_pos + 9]
                                    [count_y - you.y_pos + 9] = 269;
                            break;
                        case OBJ_MISCELLANY:
                            env.show[count_x - you.x_pos + 9]
                                    [count_y - you.y_pos + 9] = 270;
                            break;
                        case OBJ_CORPSES:
                            env.show[count_x - you.x_pos + 9]
                                    [count_y - you.y_pos + 9] = 271;
                            break;

                        case OBJ_GOLD:
                            env.show[count_x - you.x_pos + 9]
                                    [count_y - you.y_pos + 9] = 272;

                            env.show_col[count_x - you.x_pos + 9]
                                    [count_y - you.y_pos + 9] = YELLOW;
                            break;

                        default:
                            env.show[count_x - you.x_pos + 9]
                                    [count_y - you.y_pos + 9] = '8';
                            break;
                        }
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
    unsigned char which_color = LIGHTGREY;

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
                show_backup[env.cloud[s].x - you.x_pos + 9]
                           [env.cloud[s].y - you.y_pos + 9]
                       = env.show[env.cloud[s].x - you.x_pos + 9]
                                 [env.cloud[s].y - you.y_pos + 9];

                env.show[env.cloud[s].x - you.x_pos + 9]
                        [env.cloud[s].y - you.y_pos + 9] = '#';

                switch (env.cloud[s].type)
                {
                case CLOUD_FIRE:
                case CLOUD_FIRE_MON:
                    if (env.cloud[s].decay <= 20)
                        which_color = RED;
                    else if (env.cloud[s].decay <= 40)
                        which_color = LIGHTRED;
                    else if (one_chance_in(4))
                        which_color = RED;
                    else if (one_chance_in(4))
                        which_color = LIGHTRED;
                    else
                        which_color = YELLOW;
                    break;

                case CLOUD_STINK:
                case CLOUD_STINK_MON:
                    which_color = GREEN;
                    break;

                case CLOUD_COLD:
                case CLOUD_COLD_MON:
                    if (env.cloud[s].decay <= 20)
                        which_color = BLUE;
                    else if (env.cloud[s].decay <= 40)
                        which_color = LIGHTBLUE;
                    else if (one_chance_in(4))
                        which_color = BLUE;
                    else if (one_chance_in(4))
                        which_color = LIGHTBLUE;
                    else
                        which_color = WHITE;
                    break;

                case CLOUD_POISON:
                case CLOUD_POISON_MON:
                    which_color = (one_chance_in(3) ? LIGHTGREEN : GREEN);
                    break;

                case CLOUD_BLUE_SMOKE:
                case CLOUD_BLUE_SMOKE_MON:
                    which_color = LIGHTBLUE;
                    break;

                case CLOUD_PURP_SMOKE:
                case CLOUD_PURP_SMOKE_MON:
                    which_color = MAGENTA;
                    break;

                case CLOUD_MIASMA:
                case CLOUD_MIASMA_MON:
                case CLOUD_BLACK_SMOKE:
                case CLOUD_BLACK_SMOKE_MON:
                    which_color = DARKGREY;
                    break;

                default:
                    which_color = LIGHTGREY;
                    break;
                }

                env.show_col[env.cloud[s].x - you.x_pos + 9]
                            [env.cloud[s].y - you.y_pos + 9] = which_color;
            }
        }                       // end 'if != CLOUD_NONE'
    }                           // end 'for s' loop
}                               // end cloud_grid()


void noisy( int loudness, int nois_x, int nois_y )
{
    int p;
    struct monsters *monster = 0;       // NULL {dlb}

    if (silenced( nois_x, nois_y ))
        return;

    int dist = loudness * loudness;

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

// show_map() now centers the known map along x or y.  This prevents
// the player from getting "artificial" location clues by using the
// map to see how close to the end they are.  They'll need to explore
// to get that.  This function is still a mess, though. -- bwr
void show_map( FixedVector<int, 2> &spec_place )
{
    int i, j;

    int bufcount2 = 0;

    char move_x = 0;
    char move_y = 0;
    char getty = 0;

#ifdef DOS_TERM
    char buffer[4800];
#endif

    // buffer2[GYM * GXM * 2] segfaults my box {dlb}
    char buffer2[GYM * GXM * 2];        

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

            buffer2[bufcount2 + 1] = colour_code_map(start_x + i, start_y + j);

            if (start_x + i + 1 == you.x_pos && start_y + j + 1 == you.y_pos)
                buffer2[bufcount2 + 1] = WHITE;

            buffer2[bufcount2] = env.map[start_x + i][start_y + j];
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
    getty = getch();

    if (spec_place[0] == 0 && getty != 0 && getty != '+' && getty != '-'
        && getty != 'h' && getty != 'j' && getty != 'k' && getty != 'l'
        && getty != 'y' && getty != 'u' && getty != 'b' && getty != 'n'
#ifdef LINUX
        && getty != 'H' && getty != 'J' && getty != 'K' && getty != 'L'
        && getty != 'Y' && getty != 'U' && getty != 'B' && getty != 'N'
#endif
        && (getty < '0' || getty > '9'))
    {
        goto putty;
    }

    if (spec_place[0] == 1 && getty != 0 && getty != '+' && getty != '-'
        && getty != 'h' && getty != 'j' && getty != 'k' && getty != 'l'
        && getty != 'y' && getty != 'u' && getty != 'b' && getty != 'n'
#ifdef LINUX
        && getty != 'H' && getty != 'J' && getty != 'K' && getty != 'L'
        && getty != 'Y' && getty != 'U' && getty != 'B' && getty != 'N'
#endif
        && getty != '.' && getty != 'S' && (getty < '0' || getty > '9'))
    {
        goto gettything;
    }

    if (getty == 0)
        getty = getch();

    switch (getty)
    {
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

#ifndef LINUX
        // This is old DOS keypad support
    case 'H':
        move_y = -1;
        move_x = 0;
        break;
    case 'P':
        move_y = 1;
        move_x = 0;
        break;
    case 'K':
        move_x = -1;
        move_y = 0;
        break;
    case 'M':
        move_x = 1;
        move_y = 0;
        break;
    case 'O':
        move_x = -1;
        move_y = 1;
        break;
    case 'I':
        move_x = 1;
        move_y = -1;
        break;
    case 'G':
        move_y = -1;
        move_x = -1;
        break;
    case 'Q':
        move_y = 1;
        move_x = 1;
        break;

#else

    case 'B':
        move_x = -10;
        move_y = 10;
        break;

    case 'J':
        move_y = 10;
        move_x = 0;
        break;

    case 'U':
        move_x = 10;
        move_y = -10;
        break;

    case 'K':
        move_y = -10;
        move_x = 0;
        break;

    case 'Y':
        move_y = -10;
        move_x = -10;
        break;

    case 'H':
        move_x = -10;
        move_y = 0;
        break;

    case 'N':
        move_y = 10;
        move_x = 10;
        break;

    case 'L':
        move_x = 10;
        move_y = 0;
        break;

#endif

    case '+':
        move_y = 20;
        move_x = 0;
        break;
    case '-':
        move_y = -20;
        move_x = 0;
        break;
    case '.':
    case '\r':
    case 'S':
        spec_place[0] = start_x + curs_x;
        spec_place[1] = start_y + curs_y;
        goto putty;

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

            if (screen_y < min_y + half_screen)
                screen_y = min_y + half_screen;

            move_y = 0;
        }

        if (curs_y + move_y > num_lines - 1)
        {
            // screen_y += (curs_y + move_y) - num_lines + 1;
            screen_y += move_y;

            if (screen_y > max_y - half_screen) 
                screen_y = max_y - half_screen;

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

    for (i = you.x_pos - map_radius; i < you.x_pos + map_radius; i++)
    {
        for (j = you.y_pos - map_radius; j < you.y_pos + map_radius; j++)
        {
            if (random2(100) > proportion)
                continue;       // note that proportion can be over 100

            if (i < 5 || j < 5 || i > (GXM - 5) || j > (GYM - 5))
                continue;

            if (env.map[i][j] == mapch2(grd[i + 1][j + 1]))
                continue;

            empty_count = 8;

            if (grd[i][j] < DNGN_LAVA && grd[i][j] != DNGN_CLOSED_DOOR)
            {
                for (k = 0; k < 3; k++)
                {
                    for (l = 0; l < 3; l++)
                    {
                        if (k == 1 && l == 1)
                            continue;

                        if (grd[i + k][j + l] <= 60
                            && grd[i + k][j + l] != DNGN_CLOSED_DOOR)
                        {
                            empty_count--;
                        }
                    }
                }
            }

            if (empty_count > 0)
                env.map[i][j] = mapch(grd[i + 1][j + 1]);
        }
    }
}                               // end magic_mapping()


/* mapchars 3 & 4 are for non-ibm char sets */
unsigned char mapchar(unsigned char ldfk)
{
    unsigned char showed = 0;

    switch (ldfk)
    {
    case DNGN_UNSEEN:
        showed = 0;
        break;

    case DNGN_SECRET_DOOR:
    case DNGN_ROCK_WALL:
    case DNGN_PERMAROCK_WALL:
    case DNGN_STONE_WALL:
    case DNGN_METAL_WALL:
    case DNGN_GREEN_CRYSTAL_WALL:
    case DNGN_WAX_WALL:
        showed = 176;
        break;

    case DNGN_CLOSED_DOOR:
        showed = 206;
        break;

    case 20:                    // orcish idol
    case 24:                    // ???
    case 25:                    // ???
    case DNGN_SILVER_STATUE:
    case DNGN_GRANITE_STATUE:
    case DNGN_ORANGE_CRYSTAL_STATUE:
        showed = '8';
        break;

    case DNGN_LAVA_X:
    case DNGN_WATER_X:
    case DNGN_LAVA:
    case DNGN_DEEP_WATER:
    case DNGN_SHALLOW_WATER:
        showed = 247;
        break;

    case DNGN_FLOOR:
    case DNGN_UNDISCOVERED_TRAP:
        showed = 250;
        break;

        //case 68: showed = '>'; break; // < (60)

    case DNGN_OPEN_DOOR:
        showed = 39;
        break;

        //case 72: showed = '<'; break;

    case DNGN_TRAP_MECHANICAL:
    case DNGN_TRAP_MAGICAL:
    case DNGN_TRAP_III:
        showed = '^';
        break;

    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
    case DNGN_ROCK_STAIRS_DOWN:
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
    case 123:
    case 124:
    case 125:
    case 126:
        showed = '>';
        break;

    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
    case DNGN_ROCK_STAIRS_UP:
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
    case 143:
    case 144:
    case 145:
    case 146:
        showed = '<';
        break;

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
        showed = 239;
        break;

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
        showed = 220;
        break;

    case DNGN_BLUE_FOUNTAIN:
    case DNGN_DRY_FOUNTAIN_I:
    case DNGN_SPARKLING_FOUNTAIN:
    case DNGN_DRY_FOUNTAIN_II:
    case DNGN_DRY_FOUNTAIN_III:
    case DNGN_DRY_FOUNTAIN_IV:
    case DNGN_DRY_FOUNTAIN_V:
    case DNGN_DRY_FOUNTAIN_VI:
    case DNGN_DRY_FOUNTAIN_VII:
    case DNGN_DRY_FOUNTAIN_VIII:
    case DNGN_PERMADRY_FOUNTAIN:
        showed = 159;
        break;

    default:
        showed = 0;
        break;
    }

    return showed;
}


unsigned char mapchar2(unsigned char ldfk)
{
    unsigned char showed = 0;

    switch (ldfk)
    {
    case DNGN_UNSEEN:
        showed = 0;
        break;

    case DNGN_SECRET_DOOR:
    case DNGN_ROCK_WALL:
    case DNGN_PERMAROCK_WALL:
    case DNGN_STONE_WALL:
    case DNGN_METAL_WALL:
    case DNGN_GREEN_CRYSTAL_WALL:
    case DNGN_WAX_WALL:
        showed = 177;
        break;

    case DNGN_CLOSED_DOOR:
        showed = 254;
        break;

    //case DNGN_LAVA_X: showed = 247; break;     // deprecated? {dlb}
    //case DNGN_WATER_X: showed = 247; break;    // deprecated? {dlb}

    case 20:                    // orcish idol
    case 24:                    // ???
    case 25:                    // ???
    case DNGN_SILVER_STATUE:
    case DNGN_GRANITE_STATUE:
    case DNGN_ORANGE_CRYSTAL_STATUE:
        showed = '8';
        break;

    case DNGN_LAVA:
    case DNGN_DEEP_WATER:
    case DNGN_SHALLOW_WATER:
        showed = 247;
        break;

    case DNGN_FLOOR:
    case DNGN_UNDISCOVERED_TRAP:
        showed = 249;
        break;

    case 68:
        showed = '>';
        break;                  // <

    case DNGN_OPEN_DOOR:
        showed = 39;
        break;

    case 72:
        showed = '<';
        break;                  // <

    case DNGN_TRAP_MECHANICAL:
    case DNGN_TRAP_MAGICAL:
    case DNGN_TRAP_III:
        showed = '^';
        break;

    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
    case DNGN_ROCK_STAIRS_DOWN:
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
    case 123:
    case 124:
    case 125:
    case 126:
        showed = '>';
        break;

    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
    case DNGN_ROCK_STAIRS_UP:
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
    case 143:
    case 144:
    case 145:
    case 146:
        showed = '<';
        break;

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
        showed = 239;
        break;

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
        showed = 220;
        break;

    case DNGN_BLUE_FOUNTAIN:
    case DNGN_DRY_FOUNTAIN_I:
    case DNGN_SPARKLING_FOUNTAIN:
    case DNGN_DRY_FOUNTAIN_II:
    case DNGN_DRY_FOUNTAIN_III:
    case DNGN_DRY_FOUNTAIN_IV:
    case DNGN_DRY_FOUNTAIN_V:
    case DNGN_DRY_FOUNTAIN_VI:
    case DNGN_DRY_FOUNTAIN_VII:
    case DNGN_DRY_FOUNTAIN_VIII:
    case DNGN_PERMADRY_FOUNTAIN:
        showed = 159;
        break;
    default:
        showed = 0;
        break;
    }

    return showed;
}


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


//---------------------------------------------------------------
//
// get_non_ibm_symbol
//
// Returns the character code and color for everything drawn
// without the IBM graphics option.
//
//---------------------------------------------------------------
static void get_non_ibm_symbol(unsigned int object, unsigned short *ch,
                               unsigned short *color)
{
    ASSERT(color != NULL);
    ASSERT(ch != NULL);

    switch (object)
    {

    case DNGN_UNSEEN:
        *ch = 0;
        break;

    case DNGN_ROCK_WALL:
    case DNGN_PERMAROCK_WALL:
        *color = env.rock_colour;
        *ch = '#';
        break;

    case DNGN_STONE_WALL:
        if (player_in_branch( BRANCH_HALL_OF_ZOT ))
            *color = env.rock_colour;
        else
            *color = LIGHTGREY;
        *ch = '#';
        break;

    case DNGN_CLOSED_DOOR:
        *ch = '+';
        break;

    case DNGN_METAL_WALL:
        *ch = '#';
        *color = CYAN;
        break;

    case DNGN_SECRET_DOOR:
        *ch = '#';
        *color = env.rock_colour;
        break;

    case DNGN_GREEN_CRYSTAL_WALL:
        *ch = '#';
        *color = GREEN;
        break;

    case DNGN_ORCISH_IDOL:
        *ch = '8';
        *color = DARKGREY;
        break;

    case DNGN_WAX_WALL:
        *ch = '#';
        *color = YELLOW;
        break;

    case DNGN_SILVER_STATUE:
        *ch = '8';
        *color = WHITE;
        Visible_Statue[ STATUE_SILVER ] = 1;
        break;

    case DNGN_GRANITE_STATUE:
        *ch = '8';
        *color = LIGHTGREY;
        break;

    case DNGN_ORANGE_CRYSTAL_STATUE:
        *ch = '8';
        *color = LIGHTRED;
        Visible_Statue[ STATUE_ORANGE_CRYSTAL ] = 1;
        break;

    case DNGN_LAVA:
        *ch = '{';
        *color = RED;
        break;

    case DNGN_DEEP_WATER:
        *ch = '{';
        // this wavy thing also used for water elemental
        // note that some monsters which use IBM graphics aren't set
        // for this function - too tricky for now.
        *color = BLUE;
        break;

    case DNGN_SHALLOW_WATER:
        *color = CYAN;
        *ch = '{';
        break;

    case DNGN_FLOOR:
        *color = env.floor_colour;
        *ch = '.';
        break;

    case DNGN_ENTER_HELL:
        *color = RED;
        *ch = '\\';
        seen_other_thing(DNGN_ENTER_HELL);
        break;

    case DNGN_OPEN_DOOR:
        *ch = '\'';
        break;

    case DNGN_BRANCH_STAIRS:
        *color = BROWN;
        *ch = '>';
        break;

    case DNGN_TRAP_MECHANICAL:
        *color = 11;
        *ch = '^';
        break;

    case DNGN_TRAP_MAGICAL:
        *color = MAGENTA;
        *ch = '^';
        break;

    case DNGN_TRAP_III:
        *color = LIGHTGREY;
        *ch = '^';
        break;

    case DNGN_UNDISCOVERED_TRAP:
        *color = env.floor_colour;
        *ch = '.';
        break;

    case DNGN_ENTER_SHOP:
        *color = YELLOW;
        *ch = '\\';
        seen_other_thing(DNGN_ENTER_SHOP);
        break;
// if I change anything above here, must also change magic mapping!

    case DNGN_ENTER_LABYRINTH:
        *color = LIGHTGREY;
        *ch = '\\';
        seen_other_thing(DNGN_ENTER_LABYRINTH);
        break;

    case DNGN_ROCK_STAIRS_DOWN:
        *color = BROWN;         // ladder
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
        *ch = '>';
        break;

    case DNGN_ROCK_STAIRS_UP:
        *color = BROWN;         // ladder
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
        *ch = '<';
        break;

    case DNGN_ENTER_DIS:
        *color = CYAN;
        *ch = '\\';
        break;

    case DNGN_ENTER_GEHENNA:
        *color = RED;
        *ch = '\\';
        break;

    case DNGN_ENTER_COCYTUS:
        *color = LIGHTCYAN;
        *ch = '\\';
        break;

    case DNGN_ENTER_TARTARUS:
        *color = DARKGREY;
        *ch = '\\';
        break;

    case DNGN_ENTER_ABYSS:
        *color = random2(16);
        *ch = '\\';
        seen_other_thing(DNGN_ENTER_ABYSS);
        break;

    case DNGN_EXIT_ABYSS:
        *color = random2(16);
        *ch = '\\';
        break;

    case DNGN_STONE_ARCH:
        *color = LIGHTGREY;
        *ch = '\\';
        break;

    case DNGN_ENTER_PANDEMONIUM:
        *color = LIGHTBLUE;
        *ch = '\\';
        seen_other_thing(DNGN_ENTER_PANDEMONIUM);
        break;

    case DNGN_EXIT_PANDEMONIUM:
        *color = LIGHTBLUE;
        *ch = '\\';
        break;

    case DNGN_TRANSIT_PANDEMONIUM:
        *color = LIGHTGREEN;
        *ch = '\\';
        break;                  // gate to other part of pandemonium

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
    case 123:
    case 124:
    case 125:
    case 126:
        *color = YELLOW;
        *ch = '>';
        seen_staircase(object);
        break;

    case DNGN_ENTER_ZOT:
        *color = MAGENTA;
        *ch = '\\';
        seen_staircase(object);
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
    case 143:
    case 144:
    case 145:
    case 146:
        *color = YELLOW;
        *ch = '<';
        break;

    case DNGN_RETURN_FROM_ZOT:
        *color = MAGENTA;
        *ch = '\\';
        break;

    case DNGN_ALTAR_ZIN:
        *color = WHITE;
        *ch = '_';
        seen_altar(GOD_ZIN);
        break;

    case DNGN_ALTAR_SHINING_ONE:
        *color = YELLOW;
        *ch = '_';
        seen_altar(GOD_SHINING_ONE);
        break;

    case DNGN_ALTAR_KIKUBAAQUDGHA:
        *color = DARKGREY;
        *ch = '_';
        seen_altar(GOD_KIKUBAAQUDGHA);
        break;

    case DNGN_ALTAR_YREDELEMNUL:
        *color = DARKGREY;
        if (one_chance_in(3))
            *color = RED;
        *ch = '_';
        seen_altar(GOD_YREDELEMNUL);
        break;

    case DNGN_ALTAR_XOM:
        *color = random_colour();
        *ch = '_';
        seen_altar(GOD_XOM);
        break;

    case DNGN_ALTAR_VEHUMET:
        *color = LIGHTBLUE;
        if (one_chance_in(3))
            *color = LIGHTMAGENTA;
        if (one_chance_in(3))
            *color = LIGHTRED;
        *ch = '_';
        seen_altar(GOD_VEHUMET);
        break;

    case DNGN_ALTAR_OKAWARU:
        *color = CYAN;
        *ch = '_';
        seen_altar(GOD_OKAWARU);
        break;

    case DNGN_ALTAR_MAKHLEB:
        *color = RED;
        if (one_chance_in(3))
            *color = LIGHTRED;
        if (one_chance_in(3))
            *color = YELLOW;
        *ch = '_';
        seen_altar(GOD_MAKHLEB);
        break;

    case DNGN_ALTAR_SIF_MUNA:
        *color = BLUE;
        *ch = '_';
        seen_altar(GOD_SIF_MUNA);
        break;

    case DNGN_ALTAR_TROG:
        *color = RED;
        *ch = '_';
        seen_altar(GOD_TROG);
        break;

    case DNGN_ALTAR_NEMELEX_XOBEH:
        *color = LIGHTMAGENTA;
        *ch = '_';
        seen_altar(GOD_NEMELEX_XOBEH);
        break;

    case DNGN_ALTAR_ELYVILON:
        *color = LIGHTGREY;
        *ch = '_';
        seen_altar(GOD_ELYVILON);
        break;

    case DNGN_BLUE_FOUNTAIN:
        *color = BLUE;
        *ch = '}';
        break;

    case DNGN_SPARKLING_FOUNTAIN:
        *color = LIGHTBLUE;
        *ch = '}';
        break;

    case DNGN_DRY_FOUNTAIN_I:
    case DNGN_DRY_FOUNTAIN_II:
    case DNGN_PERMADRY_FOUNTAIN:
        *color = LIGHTGREY;
        *ch = '}';
        break;

    case 256:
        *ch = '0';
        break;

    case 257:
        *color = CYAN;
        *ch = '~';
        break;                  /* Invis creature walking through water */

    case 258:
        *ch = ')';
        break;                  // weapon )

    case 259:
        *ch = '[';
        break;                  // armour [

    case 260:
        *ch = '/';
        break;                  // wands, etc.

    case 261:
        *ch = '%';
        break;                  // food

    case 262:
        *ch = '+';
        break;                  // books +

    case 263:
        *ch = '?';
        break;                  // scroll ?

    case 264:
        *ch = '=';
        break;                  // ring = etc

    case 265:
        *ch = '!';
        break;                  // potions !

    case 266:
        *ch = '(';
        break;                  // stones

    case 267:
        *ch = ':';
        break;                  // book +

    case 268:
        *ch = '%';
        break;                  // corpses part 1

    case 269:
        *ch = '|';
        break;                  // magical staves

    case 270:
        *ch = '}';
        break;                  // gems

    case 271:
        *ch = '%';
        break;                  // don't know ?

    case 272:
        *ch = '$';
        break;                  // $ gold

    case 273:
        *ch = '"';
        break;                  // amulet

    default:
        int mnr = object;
        *ch = ((mnr >= 297) ? mons_char(mnr - 297) : object);   // yeah
        break;
    }
}


/*
   This is the viewwindow function for computers without IBM graphic displays.
   It is activated by a command line argument, which sets a function pointer.
 */
void viewwindow3(char draw_it, bool do_updates)
{
    int bufcount = 0;
    FixedVector < unsigned short, 1500 > buffy;  //[800]; //392];

    unsigned short ch, color;

    int count_x, count_y;

    losight(env.show, grd, you.x_pos, you.y_pos);

    for (count_x = 0; count_x < 18; count_x++)
    {
        for (count_y = 0; count_y < 18; count_y++)
        {
            env.show_col[count_x][count_y] = LIGHTGREY;
            show_backup[count_x][count_y] = 0;
        }
    }

    item();
    cloud_grid();
    monster_grid(do_updates);
    bufcount = 0;

    if (draw_it == 1)
    {
        _setcursortype(_NOCURSOR);
        for (count_y = (you.y_pos - 8); (count_y < you.y_pos + 9); count_y++)
        {
            bufcount += 16;

            for (count_x = (you.x_pos - 8); (count_x < you.x_pos + 9); count_x++)
            {
                color = env.show_col[count_x - you.x_pos + 9]
                                    [count_y - you.y_pos + 9];

                if (count_x == you.x_pos && count_y == you.y_pos)
                {
                    ch = your_sign;

                    if (player_is_swimming())
                    {
                        color = (grd[you.x_pos][you.y_pos] == DNGN_DEEP_WATER)
                                    ? BLUE : CYAN;
                    }
                    else
                    {
                        color = your_colour;
                    }
                }
                else
                {
                    unsigned int object = env.show[count_x - you.x_pos + 9]
                                                  [count_y - you.y_pos + 9];

                    get_non_ibm_symbol(object, &ch, &color);
                }

                buffy[bufcount] = ch;   //showed;
                buffy[bufcount + 1] = color;
                bufcount += 2;
            }

            bufcount += 16;
        }

        bufcount = 0;

        if (you.level_type != LEVEL_LABYRINTH
            && you.level_type != LEVEL_ABYSS)
        {
            for (count_y = 0; count_y < 17; count_y++)
            {
                bufcount += 16;
                for (count_x = 0; count_x < 17; count_x++)
                {
                    if (buffy[bufcount] != 0
                        && (count_x + you.x_pos - 9) >= 0
                        && (count_y + you.y_pos - 9) >= 0)
                    {
                        env.map[count_x + you.x_pos - 9]
                               [count_y + you.y_pos - 9] = buffy[bufcount];
                    }

                    if (Options.clean_map == 1
                        && show_backup[count_x + 1][count_y + 1] != 0
                        && (count_x + you.x_pos - 9) >= 0
                        && (count_y + you.y_pos - 9) >= 0)
                    {
                        get_non_ibm_symbol( show_backup[count_x + 1]
                                                       [count_y + 1],
                                            &ch, &color );
                        env.map[count_x + you.x_pos - 9]
                               [count_y + you.y_pos - 9] = ch;
                    }
                    bufcount += 2;
                }
                bufcount += 16;
            }
        }

        bufcount = 0;

        for (count_y = 0; count_y < 17; count_y++)
        {
            for (count_x = 0; count_x < 33; count_x++)
            {
                if (count_x + you.x_pos - 17 < 3
                    || count_y + you.y_pos - 9 < 3
                    || count_x + you.x_pos - 14 > (GXM - 3)
                    || count_y + you.y_pos - 9 > (GYM - 3))
                {
                    buffy[bufcount] = 0;
                    bufcount++;
                    buffy[bufcount] = 0;
                    bufcount++;
                    continue;
                }

                if (count_x >= 8 && count_x <= 24 && count_y >= 0
                    && count_y <= 16 && buffy[bufcount] != 0)
                {
                    bufcount += 2;
                    continue;
                }

                buffy[bufcount] = env.map[count_x + you.x_pos - 17]
                                         [count_y + you.y_pos - 9];

                buffy[bufcount + 1] = DARKGREY;

                if (Options.colour_map)
                {
                    if (env.map[count_x + you.x_pos - 16]
                               [count_y + you.y_pos - 8] != 0)
                    {
                        buffy[bufcount + 1]
                                = colour_code_map( count_x + you.x_pos - 17,
                                                   count_y + you.y_pos - 9 );
                    }
                }

                bufcount += 2;
            }
        }

        if (you.berserker)
        {
            for (count_x = 1; count_x < 1400; count_x += 2)
            {
                if (buffy[count_x] != DARKGREY)
                    buffy[count_x] = RED;
            }
        }

        if (show_green != BLACK)
        {
            for (count_x = 1; count_x < 1400; count_x += 2)
            {
                if (buffy[count_x] != DARKGREY)
                    buffy[count_x] = show_green;
            }

            show_green = ((you.special_wield == SPWLD_SHADOW) ? DARKGREY
                                                              : BLACK);
        }

#ifdef DOS_TERM
        puttext(2, 1, 34, 17, buffy.buffer());
#endif

#ifdef PLAIN_TERM
        gotoxy(2, 1);
        bufcount = 0;

        if (you.running == 0)       // this line is purely optional
        {
            for (count_x = 0; count_x < 1120; count_x += 2)     // 1056
            {
                textcolor(buffy[count_x + 1]);
                putch(buffy[count_x]);

                if (count_x % 66 == 64 && count_x > 0)
#ifdef DOS_TERM
                    cprintf(EOL " ");
#endif

#ifdef PLAIN_TERM
                gotoxy(2, wherey() + 1);
#endif
            }
        }
#endif
        _setcursortype(_NORMALCURSOR);
    }                           // end of (if brek...)
}                               // end viewwindow3()


unsigned char mapchar3(unsigned char ldfk)
{
    unsigned char showed = 0;

    switch (ldfk)
    {
    case DNGN_UNSEEN:
        showed = 0;
        break;

    case DNGN_SECRET_DOOR:
    case DNGN_ROCK_WALL:
    case DNGN_PERMAROCK_WALL:
    case DNGN_STONE_WALL:
    case DNGN_METAL_WALL:
    case DNGN_GREEN_CRYSTAL_WALL:
    case DNGN_WAX_WALL:
        showed = '*';
        break;

    case DNGN_CLOSED_DOOR:
        showed = '+';
        break;

    case 20:                    // orcish idol
    case 24:                    // ???
    case 25:                    // ???
    case DNGN_SILVER_STATUE:
    case DNGN_GRANITE_STATUE:
    case DNGN_ORANGE_CRYSTAL_STATUE:
        showed = '8';
        break;

    case DNGN_LAVA_X:
    case DNGN_WATER_X:
    case DNGN_LAVA:
    case DNGN_DEEP_WATER:
    case DNGN_SHALLOW_WATER:
        showed = '{';
        break;

    case DNGN_FLOOR:
    case DNGN_UNDISCOVERED_TRAP:
        showed = ',';
        break;

    //case 68: showed = '>'; break; // < (60)

    case DNGN_OPEN_DOOR:
        showed = 39;
        break;                  // open door

    //case 72: showed = '<'; break;

    case DNGN_TRAP_MECHANICAL:
    case DNGN_TRAP_MAGICAL:
    case DNGN_TRAP_III:
        showed = '^';
        break;

    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
    case DNGN_ROCK_STAIRS_DOWN:
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
    case 123:
    case 124:
    case 125:
    case 126:
        showed = '>';
        break;

    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
    case DNGN_ROCK_STAIRS_UP:
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
    case 143:
    case 144:
    case 145:
    case 146:
        showed = '<';
        break;

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
        showed = '\\';
        break;

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
        showed = '_';
        break;

    case DNGN_BLUE_FOUNTAIN:
    case DNGN_DRY_FOUNTAIN_I:
    case DNGN_SPARKLING_FOUNTAIN:
    case DNGN_DRY_FOUNTAIN_II:
    case DNGN_DRY_FOUNTAIN_III:
    case DNGN_DRY_FOUNTAIN_IV:
    case DNGN_DRY_FOUNTAIN_V:
    case DNGN_DRY_FOUNTAIN_VI:
    case DNGN_DRY_FOUNTAIN_VII:
    case DNGN_DRY_FOUNTAIN_VIII:
    case DNGN_PERMADRY_FOUNTAIN:
        showed = '}';
        break;

    default:
        showed = 0;
        break;
    }

    return showed;
}


unsigned char mapchar4(unsigned char ldfk)
{
    unsigned char showed = 0;

    switch (ldfk)
    {
    case DNGN_UNSEEN:
        showed = 0;
        break;

    case DNGN_CLOSED_DOOR:
        showed = '+';
        break;

    case DNGN_SECRET_DOOR:
    case DNGN_ROCK_WALL:
    case DNGN_PERMAROCK_WALL:
    case DNGN_STONE_WALL:
    case DNGN_METAL_WALL:
    case DNGN_GREEN_CRYSTAL_WALL:
    case DNGN_WAX_WALL:
        showed = '#';
        break;

    case 20:                    // orcish idol
    case 24:                    // ???
    case 25:                    // ???
    case DNGN_SILVER_STATUE:
    case DNGN_GRANITE_STATUE:
    case DNGN_ORANGE_CRYSTAL_STATUE:
        showed = '8';
        break;

    case DNGN_LAVA_X:
    case DNGN_WATER_X:
    case DNGN_LAVA:
    case DNGN_DEEP_WATER:
    case DNGN_SHALLOW_WATER:
        showed = '{';
        break;

    case DNGN_FLOOR:
    case DNGN_UNDISCOVERED_TRAP:
        showed = '.';
        break;

    case 68:
        showed = '>';           // <
        break;

    case DNGN_OPEN_DOOR:
        showed = 39;
        break;

    case 72:
        showed = '<';
        break;

    case DNGN_TRAP_MECHANICAL:
    case DNGN_TRAP_MAGICAL:
    case DNGN_TRAP_III:
        showed = '^';
        break;

    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
    case DNGN_ROCK_STAIRS_DOWN:
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
    case 123:
    case 124:
    case 125:
    case 126:
        showed = '>';
        break;

    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
    case DNGN_ROCK_STAIRS_UP:
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
    case 143:
    case 144:
    case 145:
    case 146:
        showed = '<';
        break;

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
        showed = '\\';
        break;

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
        showed = '_';
        break;

    case DNGN_BLUE_FOUNTAIN:
    case DNGN_DRY_FOUNTAIN_I:
    case DNGN_SPARKLING_FOUNTAIN:
    case DNGN_DRY_FOUNTAIN_II:
    case DNGN_DRY_FOUNTAIN_III:
    case DNGN_DRY_FOUNTAIN_IV:
    case DNGN_DRY_FOUNTAIN_V:
    case DNGN_DRY_FOUNTAIN_VI:
    case DNGN_DRY_FOUNTAIN_VII:
    case DNGN_DRY_FOUNTAIN_VIII:
    case DNGN_PERMADRY_FOUNTAIN:
        showed = '}';
        break;

    default:
        showed = 0;
        break;
    }

    return showed;
}
