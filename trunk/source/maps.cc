/*
 *  File:       maps.cc
 *  Summary:    Functions used to create vaults.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 * <2>      5/20/99        BWR Added stone lining to Zot vault,
 *                             this should prevent digging?
 * <1>      -/--/--        LRH             Created
 */

#include "AppHdr.h"
#include "maps.h"

#include <string.h>
#include <stdlib.h>

#include "enum.h"
#include "monplace.h"
#include "stuff.h"


static char vault_1(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char vault_2(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char vault_3(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char vault_4(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char vault_5(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char vault_6(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char vault_7(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char vault_8(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char vault_9(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char vault_10(char vgrid[81][81], FixedVector<int, 7>& mons_array);


static char antaeus(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char asmodeus(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char beehive(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char box_level(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char castle_dis(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char elf_hall(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char ereshkigal(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char farm_and_country(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char fort_yaktaur(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char hall_of_Zot(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char hall_of_blades(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char gloorx_vloq(char vgrid[81][81], FixedVector<int, 7>& mons_array);
//static char mollusc(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char my_map(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char mnoleg(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char cerebov(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char orc_temple(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char lom_lobon(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char slime_pit(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char snake_pit(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char swamp(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char temple(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char tomb_1(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char tomb_2(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char tomb_3(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char vaults_vault(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char vestibule_map(char vgrid[81][81], FixedVector<int, 7>& mons_array);


static char minivault_1(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_2(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_3(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_4(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_5(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_6(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_7(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_8(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_9(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_10(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_11(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_12(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_13(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_14(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_15(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_16(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_17(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_18(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_19(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_20(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_21(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_22(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_23(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_24(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_25(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_26(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_27(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_28(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_29(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_30(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_31(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_32(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_33(char vgrid[81][81], FixedVector<int, 7>& mons_array);

//jmf: originals and slim wrappers to fit into don's non-switch
static char minivault_34(char vgrid[81][81], FixedVector<int, 7>& mons_array, bool orientation);
static char minivault_35(char vgrid[81][81], FixedVector<int, 7>& mons_array, bool orientation);
static char minivault_34_a(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_34_b(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_35_a(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char minivault_35_b(char vgrid[81][81], FixedVector<int, 7>& mons_array);

static char rand_demon_1(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char rand_demon_2(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char rand_demon_3(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char rand_demon_4(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char rand_demon_5(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char rand_demon_6(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char rand_demon_7(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char rand_demon_8(char vgrid[81][81], FixedVector<int, 7>& mons_array);
static char rand_demon_9(char vgrid[81][81], FixedVector<int, 7>& mons_array);




/* ******************** BEGIN PUBLIC FUNCTIONS ******************* */


// remember (!!!) - if a member of the monster array isn't specified
// within a vault subroutine, assume that it holds a random monster
// -- only in cases of explicit monsters assignment have I listed
// out random monster insertion {dlb}

// make sure that vault_n, where n is a number, is a vault which can be put
// anywhere, while other vault names are for specific level ranges, etc.
char vault_main( char vgrid[81][81], FixedVector<int, 7>& mons_array, int vault_force, int many_many )
{

    int which_vault = 0;
    unsigned char vx, vy;
    char (*fnc_vault) (char[81][81], FixedVector<int, 7>&) = 0;

// first, fill in entirely with walls and null-terminate {dlb}:
    for (vx = 0; vx < 80; vx++)
    {
        for (vy = 0; vy < 80; vy++)
          vgrid[vx][vy] = 'x';

        vgrid[80][vx] = '\0';
        vgrid[vx][80] = '\0';
    }

    // next, select an appropriate vault to place {dlb}:
    for (;;)
    {
        which_vault = ( (vault_force == 100) ? random2(14) : vault_force );

        // endless loops result if forced vault cannot pass these tests {dlb}:
        if ( which_vault == 9 )
        {
            // if ( many_many > 23 || many_many > 12 )
            if (many_many > 12)
                break;
        }
        else if (which_vault == 11 || which_vault == 12)
        {
            if (many_many > 20)
                break;
        }
        else
            break;
    }

    // then, determine which drawing routine to use {dlb}:
    fnc_vault = ( (which_vault ==   0) ? vault_1 :
          (which_vault ==   1) ? vault_2 :
          (which_vault ==   2) ? vault_3 :
          (which_vault ==   3) ? vault_4 :
          (which_vault ==   4) ? vault_5 :
          (which_vault ==   5) ? vault_6 :
          (which_vault ==   6) ? vault_7 :
          (which_vault ==   7) ? vault_8 :
          (which_vault ==   8) ? vault_9 :
          (which_vault ==   9) ? ( (many_many > 23) ? my_map : orc_temple ) :
          (which_vault ==  10) ? vault_10 :
          (which_vault ==  11) ? farm_and_country :
          (which_vault ==  12) ? fort_yaktaur :
          (which_vault ==  13) ? box_level :

          // the hell vaults:
          (which_vault ==  50) ? vestibule_map :
          (which_vault ==  51) ? castle_dis :
          (which_vault ==  52) ? asmodeus :
          (which_vault ==  53) ? antaeus :
          (which_vault ==  54) ? ereshkigal :

          // the pandemonium big demonlord vaults:
          (which_vault ==  60) ? mnoleg :       // was nemelex
          (which_vault ==  61) ? lom_lobon :    // was sif muna
          (which_vault ==  62) ? cerebov :      // was okawaru
          (which_vault ==  63) ? gloorx_vloq :  // was kikubaaqudgha

          //(which_vault ==  64) ? mollusc :
          (which_vault ==  80) ? beehive :
          (which_vault ==  81) ? slime_pit :
          (which_vault ==  82) ? vaults_vault :
          (which_vault ==  83) ? hall_of_blades :
          (which_vault ==  84) ? hall_of_Zot :
          (which_vault ==  85) ? temple :
          (which_vault ==  86) ? snake_pit :
          (which_vault ==  87) ? elf_hall :
          (which_vault ==  88) ? tomb_1 :
          (which_vault ==  89) ? tomb_2 :
          (which_vault ==  90) ? tomb_3 :
          (which_vault ==  91) ? swamp :
          (which_vault == 200) ? minivault_1 :
          (which_vault == 201) ? minivault_2 :
          (which_vault == 202) ? minivault_3 :
          (which_vault == 203) ? minivault_4 :
          (which_vault == 204) ? minivault_5 :
          (which_vault == 205) ? ( (many_many > 15) ? minivault_6 : minivault_1 ) :
          (which_vault == 206) ? ( (many_many > 10) ? minivault_7 : minivault_2 ) :
          (which_vault == 207) ? ( (many_many > 15) ? minivault_8 : minivault_3 ) :
          (which_vault == 208) ? ( (many_many > 15) ? minivault_9 : minivault_4 ) :
          (which_vault == 209) ? minivault_10 :
          (which_vault == 210) ? minivault_11 :
          (which_vault == 211) ? minivault_12 :
          (which_vault == 212) ? minivault_13 :
          (which_vault == 213) ? minivault_14 :
          (which_vault == 214) ? minivault_15 :
          (which_vault == 215) ? minivault_16 :
          (which_vault == 216) ? minivault_17 :
          (which_vault == 217) ? minivault_18 :
          (which_vault == 218) ? minivault_19 :
          (which_vault == 219) ? minivault_20 :
          (which_vault == 220) ? minivault_21 :
          (which_vault == 221) ? minivault_22 :
          (which_vault == 222) ? minivault_23 :
          (which_vault == 223) ? minivault_24 :
          (which_vault == 224) ? minivault_25 :
          (which_vault == 225) ? minivault_26 :
          (which_vault == 226) ? minivault_27 :
          (which_vault == 227) ? minivault_28 :
          (which_vault == 228) ? minivault_29 :
          (which_vault == 229) ? minivault_30 :
          (which_vault == 230) ? minivault_31 :
          (which_vault == 231) ? minivault_32 :
          (which_vault == 232) ? minivault_33 :
          (which_vault == 233) ? minivault_34_a :
          (which_vault == 234) ? minivault_34_b :
          (which_vault == 235) ? minivault_35_a :
          (which_vault == 236) ? minivault_35_b :
          (which_vault == 300) ? rand_demon_1 :
          (which_vault == 301) ? rand_demon_2 :
          (which_vault == 302) ? rand_demon_3 :
          (which_vault == 303) ? rand_demon_4 :
          (which_vault == 304) ? rand_demon_5 :
          (which_vault == 305) ? rand_demon_6 :
          (which_vault == 306) ? rand_demon_7 :
          (which_vault == 307) ? rand_demon_8 :
          (which_vault == 308) ? rand_demon_9
          : 0 );    // yep, NULL -- original behaviour {dlb}
    
    // NB - a return value of zero is not handled well by dungeon.cc (but there it is) 10mar2000 {dlb}
    return ( (fnc_vault == 0) ? 0 : fnc_vault(vgrid, mons_array) );
}          // end vault_main()

/* ********************* END PUBLIC FUNCTIONS ******************** */

/*
   key:
   x - DNGN_ROCK_WALL
   X - DNGN_PERMAROCK_WALL -> should always be undiggable! -- bwr
   c - DNGN_STONE_WALL
   v - DNGN_METAL_WALL
   b - DNGN_GREEN_CRYSTAL_WALL
   a - DNGN_WAX_WALL
   . - DNGN_FLOOR
   + - DNGN_CLOSED_DOOR
   = - DNGN_SECRET_DOOR
   @ - entry point - must be on outside and on a particular side - see templates
   w - water
   l - lava
   >< - extra stairs - you can leave level by these but will never be placed on them from another level
   }{ - stairs 82/86 - You must be able to reach these from each other
   )( - stairs 83/87
   ][ - stairs 84/88
   I - orcish idol (does nothing)
   ^ - random trap

   A - Vestibule gateway (opened by Horn). Can also be put on other levels for colour, where it won't do anything.
   B - Altar. These are assigned specific types (eg of Zin etc) in dungeon.cc, in order.
   C - Random Altar.
   F - Typically a Granite Statue, but may be Orange or Silver (1 in 100)  
   G - Granite statue (does nothing)
   H - orange crystal statue (attacks mind)
   S - Silver statue (summons demons). Avoid using (rare).
   T - Water fountain
   U - Magic fountain
   V - Permenantly dry fountain

   Statues can't be walked over and are only destroyed by disintegration

   $ - gold
   % - normal item
   * - higher level item (good)
   | - acquirement-level item (almost guaranteed excellent)
   O - place an appropriate rune here
   P - maybe place a rune here (50%)
   R - honeycomb (2/3) or royal jelly (1/3)
   Z - the Orb of Zot

   0 - normal monster
   9 - +5 depth monster
   8 - (+2) * 2 depth monster (aargh!). Can get golden dragons and titans this way.
   1-7 - monster array monster
   used to allocate specific monsters for a vault.
   is filled with RANDOM_MONSTER if monster not specified

   note that a lot of the vaults are in there mainly to add some interest to the
   scenery, and are not the lethal treasure-fests you find in Angband
   (not that there's anything wrong with that)

   Guidelines for creating new vault maps:

   Basically you can just let your creativity run wild. You do not have
   to place all of the stairs unless the level is full screen, in which
   case you must place all except the extra stairs (> and <). The <> stairs
   can be put anywhere and in any quantities but do not have to be there. Any
   of the other stairs which are not present in the vault will be randomly
   placed outside it. Also generally try to avoid rooms with no exit.

   You can use the templates below to build vaults, and remember to return the
   same number (this number is used in builder2.cc to determine where on the map
   the vault is located). The entry point '@' must be present (except
   full-screen vaults where it must not) and be on the same side of the vault
   as it is on the template, but can be anywhere along that side.

   You'll have to tell me the level range in which you want the vault to appear,
   so that I can code it into the vault management function. Unless you've
   placed specific monster types this will probably be unnecessary.

   I think that's all. Have fun!

   ps - remember to add one to the monster array value when placing monsters
        on each map (it is 1-7, not 0-6) {dlb}
 */

static char vault_1(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // my first vault
    strcpy(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[7], "xxxxxxxxxxxx....x........x........x.................................xxxxxxxxxxxx");
    strcpy(vgrid[8], "xxxxxxxxxx|=8...x........+........x......x....x1...x2...x2...x3...x...xxxxxxxxxx");
    strcpy(vgrid[9], "xxxxxxxxxx|x....x........x........x....................................xxxxxxxxx");
    strcpy(vgrid[10], "xxxxxxxxxxxxxxxx+xxx+xxxxxxxxxxxxxx..................................xxxxxxxxxxx");
    strcpy(vgrid[11], "xxxxxxxxx.......x.................+...................................8xxxxxxxxx");
    strcpy(vgrid[12], "xxxxxxxxx.......x.................x..................................xxxxxxxxxxx");
    strcpy(vgrid[13], "xxxxxxxxx.......+........3........xx+xx................................xxxxxxxxx");
    strcpy(vgrid[14], "xxxxxxxxx.......x.................x...x..x....x1...x2...x2...x3...x...xxxxxxxxxx");
    strcpy(vgrid[15], "xxxxxxxxx.......x.................x...x.............................xxxxxxxxxxxx");
    strcpy(vgrid[16], "xxxxxxxxxx+xxxxxxxxxxxxxxxxxxxxxxxx...xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[17], "xxxxxxxxx.........................x.S.x...xxxxxx..................|||||xxxxxxxxx");
    strcpy(vgrid[18], "xxxxxxxxx....xxxxxxxxxxxxxxxxxx...x...x......xxxxxx..................||xxxxxxxxx");
    strcpy(vgrid[19], "xxxxxxxxx....x...$$$$x****.999x...x...x.........xxxxxx.................xxxxxxxxx");
    strcpy(vgrid[20], "xxxxxxxxx....+...$$$$x****....x...x...+............xxxxxx.........8....xxxxxxxxx");
    strcpy(vgrid[21], "xxxxxxxxx....x...$$$$x****....+...x...x...............xxxxxx...........xxxxxxxxx");
    strcpy(vgrid[22], "xxxxxxxxx....x...$$$$x****....x...x999x..................xxxxxx........xxxxxxxxx");
    strcpy(vgrid[23], "xxxxxxxxx....xxxxxxxxxxxxxxxxxx...x...xxx...................xxxxxx.....xxxxxxxxx");
    strcpy(vgrid[24], "xxxxxxxxx.........................x...xxxxxx...................xxxxxx..xxxxxxxxx");
    strcpy(vgrid[25], "xxxxxxxxxxxxx+xxxxxxxx+xxxxxxx+xxxx...xxxxxx+xxxxxxxx+xxxxxxxx+xxxxxxx=xxxxxxxxx");
    strcpy(vgrid[26], "xxxxxxxxx.........x.......x.......x...x.........x........x.............xxxxxxxxx");
    strcpy(vgrid[27], "xxxxxxxxx.........x.......x.......x...x.........x........x.............xxxxxxxxx");
    strcpy(vgrid[28], "xxxxxxxxx.........x.......x.......x...x.........x........x.............xxxxxxxxx");
    strcpy(vgrid[29], "xxxxxxxxx....1....x...2...x...3...x...x....3....x....2...x......1......xxxxxxxxx");
    strcpy(vgrid[30], "xxxxxxxxx.........x.......x.......x...x.........x........x.............xxxxxxxxx");
    strcpy(vgrid[31], "xxxxxxxxx.........x.......x.......x...x.........x........x.............xxxxxxxxx");
    strcpy(vgrid[32], "xxxxxxxxx.........x.......x.......x...x.........x........x.............xxxxxxxxx");
    strcpy(vgrid[33], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx...xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[34], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx...xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[35], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx@xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    mons_array[0] = MONS_SHAPESHIFTER;
    mons_array[1] = MONS_SHAPESHIFTER;
    mons_array[2] = MONS_GLOWING_SHAPESHIFTER;

    return MAP_NORTH;
}


static char vault_2(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // cell vault
    UNUSED( mons_array );

    strcpy(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6], "xxxxxxxxcccccccccccccccccccccccccccccccc");
    strcpy(vgrid[7], "xxxxxxxxccw......^......w......^......wc");
    strcpy(vgrid[8], "xxxxxxxxcc.ccccccccccccc.ccccccccccccc.c");
    strcpy(vgrid[9], "xxxxxxxxcc.c....c.c....c.c....c.c....c.c");
    strcpy(vgrid[10], "xxxxxxxxcc.c.8..+.c....c.c....+.c..9.c.c");
    strcpy(vgrid[11], "xxxxxxxxcc.c....c.+..9.c.c.9..c.+....c.c");
    strcpy(vgrid[12], "xxxxxxxxcc.c....c.c....c.c....c.c....c.c");
    strcpy(vgrid[13], "xxxxxxxxcc.cccccc.cccccc.cccccc.cccccc.c");
    strcpy(vgrid[14], "xxxxxxxxcc^c....c.c....c.c....c.c....c.c");
    strcpy(vgrid[15], "xxxxxxxxcc.c....c.c....c.c....+.c....c.c");
    strcpy(vgrid[16], "xxxxxxxxcc.c8...+.+..8.c.c.8..c.+....c.c");
    strcpy(vgrid[17], "xxxxxxxxcc.c....c.c....c.c....c.c....c.c");
    strcpy(vgrid[18], "xxxxxxxxcc.cccccc.cccccc.cccccc.cccccc.c");
    strcpy(vgrid[19], "xxxxxxxxcc.c....c.c....c.c....c.c....c.c");
    strcpy(vgrid[20], "xxxxxxxxcc.c....+.c....c.c.0..c.c....c.c");
    strcpy(vgrid[21], "xxxxxxxxcc.c..9.c.+.8..c^c....+.+.0..c.c");
    strcpy(vgrid[22], "xxxxxxxxcc.c....c.c....c.c....c.c....c.c");
    strcpy(vgrid[23], "xxxxxxxxcc.cccccc.cccccc.cccccc.cccccc.c");
    strcpy(vgrid[24], "xxxxxxxxcc.c....c.c....c.c....c.c....c.c");
    strcpy(vgrid[25], "xxxxxxxxcc.c.0..+.+.0..c.c....+.+....c.c");
    strcpy(vgrid[26], "xxxxxxxxcc.c....c.c....c.c.0..c.c.8..c.c");
    strcpy(vgrid[27], "xxxxxxxxcc.cccccc.c....c.c....c.cccccc.c");
    strcpy(vgrid[28], "xxxxxxxxcc.c....c.cccccc.cccccc.c....c^c");
    strcpy(vgrid[29], "xxxxxxxxcc.c....c.c....c.c..9.+.+....c.c");
    strcpy(vgrid[30], "xxxxxxxxcc.c.0..+.+....c.c9...c.c.0..c.c");
    strcpy(vgrid[31], "xxxxxxxxcc.c....c.c.8..c.c....c.c....c.c");
    strcpy(vgrid[32], "xxxxxxxxcc.cccccc^cccccc.cccccc^cccccc.c");
    strcpy(vgrid[33], "xxxxxxxxccw.......Twwwwc.cwwwwT.......wc");
    strcpy(vgrid[34], "xxxxxxxxcccccccccccccccc.ccccccccccccccc");
    strcpy(vgrid[35], "xxxxxxxxxxxxxxxxxxxxxxxc@cxxxxxxxxxxxxxx");

    return MAP_NORTHWEST;
}


static char vault_3(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // little maze vault
    UNUSED( mons_array );

    for (unsigned char i = 0; i < 81; i++)
        strcpy(vgrid[i], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    strcat(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[6], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[7], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[8], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[9], "x900x..............x..........xxxxxxxxxx");
    strcat(vgrid[10], "x999x.xxxxxxxxxxxx.x.xxxxxxxx.xxxxxxxxxx");
    strcat(vgrid[11], "x000x.x............x.x......x.xxxxxxxxxx");
    strcat(vgrid[12], "xx.xx.xxxxxxxxxxxxxx.x.xxxx.x.xxxxxxxxxx");
    strcat(vgrid[13], "xx.x..............xx.x.88|x.x.xxxxxxxxxx");
    strcat(vgrid[14], "xx.x.x.xxxxxxxxxx.xx.xxxxxx.x.xxxxxxxxxx");
    strcat(vgrid[15], "xx.x.x.x........x...........x.xxxxxxxxxx");
    strcat(vgrid[16], "xx.x.x.x.xxxxxx.xxxxxxxxxxxxx.xxxxxxxxxx");
    strcat(vgrid[17], "xx.xxx.x.x$$$$x...............xxxxxxxxxx");
    strcat(vgrid[18], "xx.....x.x$$$$x.xxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[19], "xxxxxxxx.x$$$$x...............xxxxxxxxxx");
    strcat(vgrid[20], "x........x$$$$x.xxxxxxxxxxxxx.xxxxxxxxxx");
    strcat(vgrid[21], "x.xxxxxx.xxxx.x.............x.xxxxxxxxxx");
    strcat(vgrid[22], "x.xxxxxx.xxxx.xxxxxxxxxxxxx.x.xxxxxxxxxx");
    strcat(vgrid[23], "x.x.......xxx.x...........x.x.xxxxxxxxxx");
    strcat(vgrid[24], "x.x.xxxxx.....x.x.xxxxx...x.x.xxxxxxxxxx");
    strcat(vgrid[25], "x.x.x999xxxxxxx.x.x***x...x.x.xxxxxxxxxx");
    strcat(vgrid[26], "x.x.x889........x.x|||xxxxx.x.xxxxxxxxxx");
    strcat(vgrid[27], "x.x.x899x.xxxxx.x.x***xxxxx.x.xxxxxxxxxx");
    strcat(vgrid[28], "x.x.xxxxx.xxxxx.x.xx.xxxxxx.x.xxxxxxxxxx");
    strcat(vgrid[29], "x.x..........xx.x.xx........x.xxxxxxxxxx");
    strcat(vgrid[30], "x.xxxxxxx.xx.xx.x.xxxxx.xxxxx.xxxxxxxxxx");
    strcat(vgrid[31], "x.xxx000x.xx.xx.x.x$$$x.xxxxx.xxxxxxxxxx");
    strcat(vgrid[32], "x|||x000x.x$$$x.x.x$$$x%%x%%%.xxxxxxxxxx");
    strcat(vgrid[33], "x|||x000..x$8$x.x.x$$$x%%x%8%xxxxxxxxxxx");
    strcat(vgrid[34], "x|||xxxxxxx$$$x.x..$$$xxxx%%%xxxxxxxxxxx");
    strcat(vgrid[35], "xxxxxxxxxxxxxxx@xxxxxxxxxxxxxxxxxxxxxxxx");

    return MAP_NORTHEAST;
}


static char vault_4(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // thingy vault
    UNUSED( mons_array );

    for (unsigned char i = 0; i < 81; i++)
        strcpy(vgrid[i], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    strcpy(vgrid[36], "xxxxxxxxxxxxxxxxxxxxxxxxx@xxxxxxxxxxxxxx");
    strcpy(vgrid[37], "xxxxxxxxxxxxxxxxxxxxxxxxx^xxxxxxxxxxxxxx");
    strcpy(vgrid[38], "xxxxxxxxxxxxxxxxxxxxxxxx...xxxxxxxxxxxxx");
    strcpy(vgrid[39], "xxxxxxxxxxxxxxxxxxxxxxx.....xxxxxxxxxxxx");
    strcpy(vgrid[40], "xxxxxxxxxxxxxxxxxxxxxxxx...xxxxxxxxxxxxx");
    strcpy(vgrid[41], "xxxxxxxxxxxxxxxxxxxxxxxx...xxxxxxxxxxxxx");
    strcpy(vgrid[42], "xxxxxxxxxxxxxxxxxxxxxxx.....xxxxxxxxxxxx");
    strcpy(vgrid[43], "xxxxxxxxxxxxxxxxxxxxx.........xxxxxxxxxx");
    strcpy(vgrid[44], "xxxxxxxxxxxxxxxxx......0...0......xxxxxx");
    strcpy(vgrid[45], "xxxxxxxxxxxxxx.......................xxx");
    strcpy(vgrid[46], "xxxxxxxxxxxxxx.........0...0.........xxx");
    strcpy(vgrid[47], "xxxxxxxxxxxxx8......0.........0......8xx");
    strcpy(vgrid[48], "xxxxxxxxxxxxxx.........0...0.........xxx");
    strcpy(vgrid[49], "xxxxxxxxxxxxxx.......................xxx");
    strcpy(vgrid[50], "xxxxxxxxxxxxxxx........0...0........xxxx");
    strcpy(vgrid[51], "xxxxxxxxxxxxxxxxxxxx...........xxxxxxxxx");
    strcpy(vgrid[52], "xxxxxxxxxxxxxxxxxxxxxxxx...xxxxxxxxxxxxx");
    strcpy(vgrid[53], "xxxxxxxxxxxxxxxxxxxxxxxx...xxxxxxxxxxxxx");
    strcpy(vgrid[54], "xxxxxxxxxxxxxxxxxxxxxxxxx.xxxxxxxxxxxxxx");
    strcpy(vgrid[55], "xxxxxxxxxxxxxxxxxxxxxxxxx.xxxxxxxxxxxxxx");
    strcpy(vgrid[56], "xxxxxxxxxxxxxxxxxxxxxxxxx.xxxxxxxxxxxxxx");
    strcpy(vgrid[57], "xxxxxxxxxxxxxxxxxxxxxxx.....xxxxxxxxxxxx");
    strcpy(vgrid[58], "xxxxxxxxxxxxxxxxxx...............xxxxxxx");
    strcpy(vgrid[59], "xxxxxxxxxxxxxxxx8.................8xxxxx");
    strcpy(vgrid[60], "xxxxxxxxxxxxxxxxxxx.............xxxxxxxx");
    strcpy(vgrid[61], "xxxxxxxxxxxxxxxxxxxxxxxx999xxxxxxxxxxxxx");
    strcpy(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    return MAP_SOUTHWEST;
}

static char vault_5(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // hourglass vault
    UNUSED( mons_array );

    for (unsigned char i = 0; i < 81; i++)
        strcpy(vgrid[i], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    strcat(vgrid[36], "xxxxxxxxxxxxxx@xxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[37], "xxxxxx.................xxxxxxxxxxxxxxxxx");
    strcat(vgrid[38], "xxxxx...................xxxxxxxxxxxxxxxx");
    strcat(vgrid[39], "xxxxx...................xxxxxxxxxxxxxxxx");
    strcat(vgrid[40], "xxxxxx.................xxxxxxxxxxxxxxxxx");
    strcat(vgrid[41], "xxxxxx.................xxxxxxxxxxxxxxxxx");
    strcat(vgrid[42], "xxxxxx.................xxxxxxxxxxxxxxxxx");
    strcat(vgrid[43], "xxxxxxx...............xxxxxxxxxxxxxxxxxx");
    strcat(vgrid[44], "xxxxxxx...............xxxxxxxxxxxxxxxxxx");
    strcat(vgrid[45], "xxxxxxxx.............xxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[46], "xxxxxxxxx.....8.....xxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[47], "xxxxxxxxxx...999...xxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[48], "xxxxxxxxxxxx00000xxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[49], "xxxxxxxxxxxxx===xxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[50], "xxxxxxxxxxxx.....xxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[51], "xxxxxxxxxx.........xxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[52], "xxxxxxxxx...........xxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[53], "xxxxxxxx......|......xxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[54], "xxxxxxx...............xxxxxxxxxxxxxxxxxx");
    strcat(vgrid[55], "xxxxxxx...............xxxxxxxxxxxxxxxxxx");
    strcat(vgrid[56], "xxxxxx........$........xxxxxxxxxxxxxxxxx");
    strcat(vgrid[57], "xxxxxx.......$$$.......xxxxxxxxxxxxxxxxx");
    strcat(vgrid[58], "xxxxxx....$$$$$$$$$....xxxxxxxxxxxxxxxxx");
    strcat(vgrid[59], "xxxxx$$$$$$$$$$$$$$$$$$$xxxxxxxxxxxxxxxx");
    strcat(vgrid[60], "xxxxx$$$$$$$$$$$$$$$$$$$xxxxxxxxxxxxxxxx");
    strcat(vgrid[61], "xxxxxx$$$$$$$$$$$$$$$$$xxxxxxxxxxxxxxxxx");
    strcat(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    return MAP_SOUTHEAST;
}


static char vault_6(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // a more Angbandy vault
    UNUSED( mons_array );

    for (unsigned char i = 0; i < 81; i++)
        strcpy(vgrid[i], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    strcat(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[6], "ccccccccccccccccccccccccccccccccxxxxxxxx");
    strcat(vgrid[7], "c*******cc..9...cc.+8c0c*c.c*c8cxxxxxxxx");
    strcat(vgrid[8], "c******cc..cc..cc..cc0c.c.c.c8ccxxxxxxxx");
    strcat(vgrid[9], "c*****cc..cc..cc..cc.c$c.c.c8c.cxxxxxxxx");
    strcat(vgrid[10], "c****cc9.cc..cc8.cc|c.c|c.c*c0ccxxxxxxxx");
    strcat(vgrid[11], "c***cc..cc..cc..cc.c.c.c.c.c.c$cxxxxxxxx");
    strcat(vgrid[12], "c**cc..cc8.cc..cc.c*c.c.c.c.c.ccxxxxxxxx");
    strcat(vgrid[13], "c+cc9.cc..cc..cc.c.c.c.c*c.c.c.cxxxxxxxx");
    strcat(vgrid[14], "c^c..cc..cc..cc.c$c.c.c.c.c.c*ccxxxxxxxx");
    strcat(vgrid[15], "c...cc..cc..cc.c.c.c9c$c.c.c.c9cxxxxxxxx");
    strcat(vgrid[16], "c..cc..cc..cc$c.c.c*c.c.c.c9c9ccxxxxxxxx");
    strcat(vgrid[17], "c.cc..cc..cc.c.c|c.c.c.c.c$c.c9cxxxxxxxx");
    strcat(vgrid[18], "ccc..cc..cc.c.c.c.c.c.c.c.c.cc+cxxxxxxxx");
    strcat(vgrid[19], "cc..cc..cc.c*c.c.c.c.c.c$c.cc..cxxxxxxxx");
    strcat(vgrid[20], "c0.cc..cc.c.c.c.c8c.c*c.c.cc0.ccxxxxxxxx");
    strcat(vgrid[21], "c.cc..cc*c.c.c.c.c$c.c.c.cc..cccxxxxxxxx");
    strcat(vgrid[22], "c^c..cc.c.c9c.c.c.c.c.c.cc..cc.cxxxxxxxx");
    strcat(vgrid[23], "c0..cc$c.c.c*c0c.c.c.c.cc..cc.0cxxxxxxxx");
    strcat(vgrid[24], "c..cc.c.c9c.c.c.c$c.c.cc.9cc...cxxxxxxxx");
    strcat(vgrid[25], "c.cc9c.c.c.c.c.c.c.c.cc..cc..c^cxxxxxxxx");
    strcat(vgrid[26], "ccc.c.c$c.c.c.c.c.c$cc..cc..cc^cxxxxxxxx");
    strcat(vgrid[27], "cc$c.c.c.c.c$c.c0c.cc..cc..cc..cxxxxxxxx");
    strcat(vgrid[28], "c.c.c.c.c.c.c.c.c.cc9.cc..cc..ccxxxxxxxx");
    strcat(vgrid[29], "cc.c8c.c.c$c.c.c.cc..cc..cc0.cccxxxxxxxx");
    strcat(vgrid[30], "c.c$c.c$c0c.c.c.cc..cc..cc..cc$cxxxxxxxx");
    strcat(vgrid[31], "cc.c.c.c.c.c*c.cc..cc..cc..cc$$cxxxxxxxx");
    strcat(vgrid[32], "c.c.c.c.c.c.c.cc..cc0.cc..cc$$$cxxxxxxxx");
    strcat(vgrid[33], "cc.c.c.c.c.c$cc..cc..cc..cc$$$$cxxxxxxxx");
    strcat(vgrid[34], "c.c.c.c.c.c.cc.8.^..cc....+$$$$cxxxxxxxx");
    strcat(vgrid[35], "cccc@cccccccccccccccccccccccccccxxxxxxxx");

    return MAP_NORTHEAST;
}

static char vault_7(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // four-leaf vault
    UNUSED( mons_array );

    strcpy(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[7], "xxxxxxxxxxxxx.........^..^.........xxxxx");
    strcpy(vgrid[8], "xxxxxxxxxxxx...xxxxxxxx..xxxxxxxx...xxxx");
    strcpy(vgrid[9], "xxxxxxxxxxx...xxxxxxxxx..xxxxxxxxx...xxx");
    strcpy(vgrid[10], "xxxxxxxxxx...xx$*....xx..xx....$$xx...xx");
    strcpy(vgrid[11], "xxxxxxxxx...xx$*$....xx..xx....$*$xx...x");
    strcpy(vgrid[12], "xxxxxxxxx..xx*$*$....xx..xx....*$$$xx..x");
    strcpy(vgrid[13], "xxxxxxxxx..xx$$$.00..xx..xx..00.*$*xx..x");
    strcpy(vgrid[14], "xxxxxxxxx..xx....09..xx..xx..90....xx..x");
    strcpy(vgrid[15], "xxxxxxxxx..xx......+xx....xx+......xx..x");
    strcpy(vgrid[16], "xxxxxxxxx..xx......x^......^x......xx..x");
    strcpy(vgrid[17], "xxxxxxxxx..xxxxxxxxx........xxxxxxxxx..x");
    strcpy(vgrid[18], "xxxxxxxxx..xxxxxxxx..........xxxxxxxx..x");
    strcpy(vgrid[19], "xxxxxxxxx..............TT..............x");
    strcpy(vgrid[20], "xxxxxxxxx..............TT..............x");
    strcpy(vgrid[21], "xxxxxxxxx..xxxxxxxx..........xxxxxxxx..x");
    strcpy(vgrid[22], "xxxxxxxxx..xxxxxxxxx........xxxxxxxxx..x");
    strcpy(vgrid[23], "xxxxxxxxx..xx......x^......^x......xx..x");
    strcpy(vgrid[24], "xxxxxxxxx..xx......+xx....xx+......xx..x");
    strcpy(vgrid[25], "xxxxxxxxx..xx....09..xx..xx..90....xx..x");
    strcpy(vgrid[26], "xxxxxxxxx..xx$$*.00..xx..xx..00.*$$xx..x");
    strcpy(vgrid[27], "xxxxxxxxx..xx*$*$....xx..xx....*$$*xx..x");
    strcpy(vgrid[28], "xxxxxxxxx...xx*$*....xx..xx....$$$xx...x");
    strcpy(vgrid[29], "xxxxxxxxxx...xx*$....xx..xx....*$xx...xx");
    strcpy(vgrid[30], "xxxxxxxxxxx...xxxxxxxxx..xxxxxxxxx...xxx");
    strcpy(vgrid[31], "xxxxxxxxxxxx...xxxxxxxx..xxxxxxxx...xxxx");
    strcpy(vgrid[32], "xxxxxxxxxxxxx..^................^..xxxxx");
    strcpy(vgrid[33], "xxxxxxxxxxxxxxxxxxxxxxx^^xxxxxxxxxxxxxxx");
    strcpy(vgrid[34], "xxxxxxxxxxxxxxxxxxxxxxx++xxxxxxxxxxxxxxx");
    strcpy(vgrid[35], "xxxxxxxxxxxxxxxxxxxxxxx@xxxxxxxxxxxxxxxx");

    return MAP_NORTHWEST;
}

static char vault_8(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // cross vault
    UNUSED( mons_array );

    strcpy(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[7], "xxxxxxxxxxxxxxxxxx............xxxxxxxxxx");
    strcpy(vgrid[8], "xxxxxxxxxxxxxxx..................xxxxxxx");
    strcpy(vgrid[9], "xxxxxxxxxxxxx......................xxxxx");
    strcpy(vgrid[10], "xxxxxxxxxxxx..........w..w..........xxxx");
    strcpy(vgrid[11], "xxxxxxxxxxx........wwww++wwww........xxx");
    strcpy(vgrid[12], "xxxxxxxxxxx......wwwvvv^^vvvwww......xxx");
    strcpy(vgrid[13], "xxxxxxxxxx......wwwwv.9..9.vwwww......xx");
    strcpy(vgrid[14], "xxxxxxxxxx.....wwwwwv......vwwwww.....xx");
    strcpy(vgrid[15], "xxxxxxxxxx....wwwwwvv......vvwwwww....xx");
    strcpy(vgrid[16], "xxxxxxxxx....wwwwwvv........vvwwwww....x");
    strcpy(vgrid[17], "xxxxxxxxx....wwvvvv....vv....vvvvww....x");
    strcpy(vgrid[18], "xxxxxxxxx...wwwv......vvvv......vwww...x");
    strcpy(vgrid[19], "xxxxxxxxx...wwwv....vv8vv8vv....vwww...x");
    strcpy(vgrid[20], "xxxxxxxxx..wwwwv...vvvv||vvvv...vwwww..x");
    strcpy(vgrid[21], "xxxxxxxxx^^wwwwv...vvvv||vvvv...vwwww^^x");
    strcpy(vgrid[22], "xxxxxxxxx..wwwwv....vv8vv8vv....vwwww..x");
    strcpy(vgrid[23], "xxxxxxxxx...wwwv......vvvv......vwww...x");
    strcpy(vgrid[24], "xxxxxxxxx...wwwvvvv....vv....vvvvwww...x");
    strcpy(vgrid[25], "xxxxxxxxx....wwwwwvv........vvwwwww....x");
    strcpy(vgrid[26], "xxxxxxxxxx...wwwwwwvv......vvwwwwww...xx");
    strcpy(vgrid[27], "xxxxxxxxxx....wwwwwwv......vwwwwww....xx");
    strcpy(vgrid[28], "xxxxxxxxxx.....wwwwwv......vwwwww.....xx");
    strcpy(vgrid[29], "xxxxxxxxxxx.....wwwwvvvvvvvvwwww.....xxx");
    strcpy(vgrid[30], "xxxxxxxxxxx.......wwwwwwwwwwww.......xxx");
    strcpy(vgrid[31], "xxxxxxxxxxxx.........wwwwww.........xxxx");
    strcpy(vgrid[32], "xxxxxxxxxxxxx.........^..^.........xxxxx");
    strcpy(vgrid[33], "xxxxxxxxxxxxxxx.......x++x.......xxxxxxx");
    strcpy(vgrid[34], "xxxxxxxxxxxxxxxxxx...xx..xx...xxxxxxxxxx");
    strcpy(vgrid[35], "xxxxxxxxxxxxxxxxxxxxxx..@.xxxxxxxxxxxxxx");

    return MAP_NORTHWEST;
}


static char vault_9(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // another thingy vault 
    UNUSED( mons_array );

    for (unsigned char i = 0; i < 81; i++)
        strcpy(vgrid[i], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    strcat(vgrid[36], "xxxxxxxxxxxxxxx@xxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[37], "xxxxxxxxxxxxxxx^xxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[38], "xxxxxxxxxxxxxxx.xxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[39], "xx.....^...............^.....xxxxxxxxxxx");
    strcat(vgrid[40], "x..bb..xxxxxxxxxxxxxxxxx..bb..xxxxxxxxxx");
    strcat(vgrid[41], "x..b...xxxxxxxxxxxxxxxxx...b..xxxxxxxxxx");
    strcat(vgrid[42], "x...b..xxxxbbbbbbbbbxxxx..b...xxxxxxxxxx");
    strcat(vgrid[43], "x..bb..xxbbb.......bbbxx..bb..xxxxxxxxxx");
    strcat(vgrid[44], "x......xxb....9.9....bxx......xxxxxxxxxx");
    strcat(vgrid[45], "x..bb..xbb..%$$$$$%..bbx..bb..xxxxxxxxxx");
    strcat(vgrid[46], "x...b..xb..0%$***$%0..bx..b...xxxxxxxxxx");
    strcat(vgrid[47], "x..b...xb..0%$*H*$%0..bx...b..xxxxxxxxxx");
    strcat(vgrid[48], "x...b..xb..0%$***$%0..bx..b...xxxxxxxxxx");
    strcat(vgrid[49], "x..b...xb...%$$$$$%...bx...b..xxxxxxxxxx");
    strcat(vgrid[50], "x...b..xbb.900000009.bbx..b...xxxxxxxxxx");
    strcat(vgrid[51], "x..b...xxb...........bxx...b..xxxxxxxxxx");
    strcat(vgrid[52], "x..bb..xxbbb..9.9..bbbxx..bb..xxxxxxxxxx");
    strcat(vgrid[53], "x......xxxxbbbb.bbbbxxxx......xxxxxxxxxx");
    strcat(vgrid[54], "x..bb..xxxxxxxb=bxxxxxxx..bb..xxxxxxxxxx");
    strcat(vgrid[55], "x..b...xxxxxxxx=xxxxxxxx...b..xxxxxxxxxx");
    strcat(vgrid[56], "x...b..xxxxxxxx^xxxxxxxx..b...xxxxxxxxxx");
    strcat(vgrid[57], "x..b....xxxxxxx=xxxxxxx....b..xxxxxxxxxx");
    strcat(vgrid[58], "x...b...^.............^...b...xxxxxxxxxx");
    strcat(vgrid[59], "x..b....xxxxxxxxxxxxxxx....b..xxxxxxxxxx");
    strcat(vgrid[60], "x..bb..xxxxxxxxxxxxxxxxx..bb..xxxxxxxxxx");
    strcat(vgrid[61], "xx....xxxxxxxxxxxxxxxxxxx....xxxxxxxxxxx");
    strcat(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    return MAP_SOUTHEAST;
}


static char vault_10(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{    // impenetrable vault
    UNUSED( mons_array );

    for (unsigned char i = 0; i < 81; i++)
        strcpy(vgrid[i], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    strcat(vgrid[36], "..............@................xxxxxxxxx");
    strcat(vgrid[37], "...............................xxxxxxxxx");
    strcat(vgrid[38], "...............................xxxxxxxxx");
    strcat(vgrid[39], "...............................xxxxxxxxx");
    strcat(vgrid[40], "...............................xxxxxxxxx");
    strcat(vgrid[41], ".....cccccccccccccccc..........xxxxxxxxx");
    strcat(vgrid[42], ".....c[^...........9cc.........xxxxxxxxx");
    strcat(vgrid[43], ".....c^xxxxx=xxxxxx..cc........xxxxxxxxx");
    strcat(vgrid[44], ".....c.x9..^^^...9xx..cc.......xxxxxxxxx");
    strcat(vgrid[45], ".....c.x.xxx=xxxx..xx..cc......xxxxxxxxx");
    strcat(vgrid[46], ".....c.x^x$$$$$$xx..xx.9c......xxxxxxxxx");
    strcat(vgrid[47], ".....c.=^=$*|||*$xx..xx.c......xxxxxxxxx");
    strcat(vgrid[48], ".....c.x^xx$*|||*$xx.9x.c......xxxxxxxxx");
    strcat(vgrid[49], ".....c.x9.xx$*|||*$xx^x.c......xxxxxxxxx");
    strcat(vgrid[50], ".....c.xx..xx$*|||*$=^=.c......xxxxxxxxx");
    strcat(vgrid[51], ".....c9.xx..xx$$$$$$x^x.c......xxxxxxxxx");
    strcat(vgrid[52], ".....cc..xx..xxxx=xxx.x.c......xxxxxxxxx");
    strcat(vgrid[53], "......cc..xx9...^^^..9x.c......xxxxxxxxx");
    strcat(vgrid[54], ".......cc..xxxxxx=xxxxx^c......xxxxxxxxx");
    strcat(vgrid[55], "........cc9...........^]c......xxxxxxxxx");
    strcat(vgrid[56], ".........cccccccccccccccc......xxxxxxxxx");
    strcat(vgrid[57], "...............................xxxxxxxxx");
    strcat(vgrid[58], "...............................xxxxxxxxx");
    strcat(vgrid[59], "...............................xxxxxxxxx");
    strcat(vgrid[60], "...............................xxxxxxxxx");
    strcat(vgrid[61], "...............................xxxxxxxxx");
    strcat(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    return MAP_SOUTHEAST;
}


static char orc_temple(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{
    for (unsigned char i = 0; i < 81; i++)
        strcpy(vgrid[i], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    strcpy(vgrid[36], "xxxxxxxxxxxxxxxxxxxxxxx@xxxxxxxxxxxxxxxx");
    strcpy(vgrid[37], "xxxxxxxxxxxxxxxxxxxxxxx.xxxxxxxxxxxxxxxx");
    strcpy(vgrid[38], "xxxxxxxxxxxxxxxxxxxxxx...xxxxxxxxxxxxxxx");
    strcpy(vgrid[39], "xxxxxxxxxxxxxxxxxxxxxx4.4xxxxxxxxxxxxxxx");
    strcpy(vgrid[40], "xxxxxxxxx**..........x414x..........**xx");
    strcpy(vgrid[41], "xxxxxxxxx**..........x4.4x..........**xx");
    strcpy(vgrid[42], "xxxxxxxxx............+...+....4.......xx");
    strcpy(vgrid[43], "xxxxxxxxx....4..4....x...x............xx");
    strcpy(vgrid[44], "xxxxxxxxx............x...x.......4....xx");
    strcpy(vgrid[45], "xxxxxxxxx............xx.xx............xx");
    strcpy(vgrid[46], "xxxxxxxxx...4......xxxx+xxxx......6...xx");
    strcpy(vgrid[47], "xxxxxxxxx........xxx.......xxx........xx");
    strcpy(vgrid[48], "xxxxxxxxxxx...xxxx..2.....2..xxxx...xxxx");
    strcpy(vgrid[49], "xxxxxxxxxxxx+xxxx.............xxxx+xxxxx");
    strcpy(vgrid[50], "xxxxxxxxxxx...xxx.............xxx...xxxx");
    strcpy(vgrid[51], "xxxxxxxxx......x...............x......xx");
    strcpy(vgrid[52], "xxxxxxxxx..4...x...2...I...2...x...5..xx");
    strcpy(vgrid[53], "xxxxxxxxx......x...............x......xx");
    strcpy(vgrid[54], "xxxxxxxxx...4..xx.............xx..5...xx");
    strcpy(vgrid[55], "xxxxxxxxx$......x....2...2....x......$xx");
    strcpy(vgrid[56], "xxxxxxxxx$6..5..xx.....3.....xx.5...7$xx");
    strcpy(vgrid[57], "xxxxxxxxx$$$.....xxx.......xxx.....$$$xx");
    strcpy(vgrid[58], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[59], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[60], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[61], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    mons_array[0] = MONS_ORC_WARLORD;
    mons_array[1] = MONS_ORC_PRIEST;
    mons_array[2] = MONS_ORC_HIGH_PRIEST;
    mons_array[3] = MONS_ORC_WARRIOR;
    mons_array[4] = MONS_ORC_WIZARD;
    mons_array[5] = MONS_ORC_KNIGHT;
    mons_array[6] = MONS_ORC_SORCERER;

    return MAP_SOUTHWEST;
}

static char my_map(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // by Matthew Ludivico

    for (unsigned char i = 0; i < 81; i++)
        strcpy(vgrid[i], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    strcpy(vgrid[36], "xxxxxxxxxx.@.xxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[37], "xxxxxxxxxxx...xxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[38], "xxxxxxxxxxxx..........................xx");
    strcpy(vgrid[39], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx..xx");
    strcpy(vgrid[40], "xxxxxxxxx.^^..........................xx");
    strcpy(vgrid[41], "xxxxxxxxxx.^^xx+xxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[42], "xxxxxxxxxxx.^...11....xxxxxxxx..xxxxxxxx");
    strcpy(vgrid[43], "xxxxxxxxxxxx..x.1..6..xxx........xx..xxx");
    strcpy(vgrid[44], "xxxxxxxxxxxxx.xxxxxxxxx...vvvvv...x...xx");
    strcpy(vgrid[45], "xxxxxxxxx6..1...x.........+1..v.......xx");
    strcpy(vgrid[46], "xxxxxxxxx..1....x.........vvvvv........x");
    strcpy(vgrid[47], "xxxxxxxxx..5...xx......................x");
    strcpy(vgrid[48], "xxxxxxxxxxxxxx^++...........vvvvvvv....x");
    strcpy(vgrid[49], "xxxxxxxxxxxxxx^xx...xx=xx...vv$%$vvvvv.x");
    strcpy(vgrid[50], "xxxxxxxxxxxxxx^x...xxv1vxx...vvv*2...v.x");
    strcpy(vgrid[51], "xxxxxxxxxxxxxx^x..vvvv7.vvvv...vv.vv+v^x");
    strcpy(vgrid[52], "xxxxxxxxx..xxx^..vvvb....bvvv...vvv^...x");
    strcpy(vgrid[53], "xxxxxxxxx%%.xx..vvvvb....bvvvv.......xxx");
    strcpy(vgrid[54], "xxxxxxxxxx.....vvbbb......bbbvv.....xxxx");
    strcpy(vgrid[55], "xxxxxxxxxxx....vvb....66....bvvxxxxxxxxx");
    strcpy(vgrid[56], "xxxxxxxxxxxxxxvvvb..llllll..bvvvxxxxxxxx");
    strcpy(vgrid[57], "xxxxxxxxxvvvvvvvvb..ll45ll..bvvvvvvvvxxx");
    strcpy(vgrid[58], "xxxxxxxxxccc***+== .l3.2.l..cccccccccxxx");
    strcpy(vgrid[59], "xxxxxxxxxccc+cccbb....ll....c..$$$$+$*cx");
    strcpy(vgrid[60], "xxxxxxxxxcc|||cbb...3llll2...cc%*%*c$|cx");
    strcpy(vgrid[61], "xxxxxxxxxcccccccbbbbbbbbbbbccccccccccccx");
    strcpy(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    mons_array[0] = MONS_HELL_HOUND;
    mons_array[1] = MONS_NECROMANCER;
    mons_array[2] = MONS_WIZARD;
    mons_array[3] = MONS_ORANGE_DEMON;
    mons_array[4] = MONS_ROTTING_DEVIL;
    mons_array[5] = MONS_HELL_KNIGHT;
    mons_array[6] = MONS_GREAT_ORB_OF_EYES;

    return MAP_SOUTHWEST;
}

static char farm_and_country(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // by Matthew Ludivico (mll6@lehigh.edu)

    for (unsigned char i = 0; i < 81; i++)
        strcpy(vgrid[i], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    strcpy(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[7], "xxxxxxxxxxxxxxxxx..........................................xxxxxxxx}.xxxxxxxxxxx");
    strcpy(vgrid[8], "xxxxxxxxxxxxxxxxxxx............xxxxxx....xxx.......xx...........xxxx..]xxxxxxxxx");
    strcpy(vgrid[9], "xxxxxxxxxxxxxxx***x...........xxx..xxx............xxxx...........xx..xxxxxxxxxxx");
    strcpy(vgrid[10], "xxxxxxxxxxxxxxx|*$=...xx.xxxxxxx....xxxxxxxxxx......xx................xxxxxxxxxx");
    strcpy(vgrid[11], "xxxxxxxxxxxxxxxxxxx....xxxxxxxx......3..xxx.................x..........xxxxxxxxx");
    strcpy(vgrid[12], "xxxxxxxxxxxxxxxxxx......x........x......xx.........w...................xxxxxxxxx");
    strcpy(vgrid[13], "xxxxxxxxxxx)......xx...xxx.....xxx......x........www3....3.............xxxxxxxxx");
    strcpy(vgrid[14], "xxxxxxxxxxxx=xxxxxxxxxxx...xxxxxxxxx..xxx.....wwwww....%%%.............xxxxxxxxx");
    strcpy(vgrid[15], "xxxxxxxxxx......xxx.......xx.xxxx.x...xxxxxxxwwwwwww..5%%%..........xx.xxxxxxxxx");
    strcpy(vgrid[16], "xxxxxxxxx.........x..xxxxxxxx.....x........3wwwwwwwww..%%%........xxx..xxxxxxxxx");
    strcpy(vgrid[17], "xxxxxxxxx....5...xx..x.xxxxx.....xxx........wwwwwwwww..%%%..........xx.xxxxxxxxx");
    strcpy(vgrid[18], "xxxxxxxxxxx.....xxx..xx..xx........xxxxxxxxxwwwwwwwww..............xxx.xxxxxxxxx");
    strcpy(vgrid[19], "xxxxxxxxxx........x..x...............xx..xxxxwwwwwwwwwwwwww............xxxxxxxxx");
    strcpy(vgrid[20], "xxxxxxxxx.............................x.....xxwwwwww3wwwwww............xxxxxxxxx");
    strcpy(vgrid[21], "xxxxxxxxxxx...x...........5.....7...............ww.......ww.....44....xxxxxxxxxx");
    strcpy(vgrid[22], "xxxxxxxxxwxx..xx.....622...2.26...6.2...22.6...62..2..226ww.....44xx...xxxxxxxxx");
    strcpy(vgrid[23], "xxxxxxxxxwwxxxx......2....2.22....2..2...2.2.......22...2ww....xxxx..xxxxxxxxxxx");
    strcpy(vgrid[24], "xxxxxxxxxwwwwxxx......2...2.2.2...2.22..2.22...22.2.2..22ww.....xxx....xxxxxxxxx");
    strcpy(vgrid[25], "xxxxxxxxxwwwwwx....4..2...2...........22...277..2..2.2.22ww...........xxxxxxxxxx");
    strcpy(vgrid[26], "xxxxxxxxxwwwwwxx....42..2....22.4..2..2...2.4..2.22..22.2ww............xxxxxxxxx");
    strcpy(vgrid[27], "xxxxxxxxxwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww.wwwwwwwwwwwww..2.........xxxxxxxxx");
    strcpy(vgrid[28], "xxxxxxxxxwwwwwxx.....62....2.26...62.2.2..26...6...22..26..............xxxxxxxxx");
    strcpy(vgrid[29], "xxxxxxxxxwwwww.........................................................xxxxxxxxx");
    strcpy(vgrid[30], "xxxxxxxxxwwwwwxx....222.2.22..2.7.......7..............................xxxxxxxxx");
    strcpy(vgrid[31], "xxxxxxxxxwwwww...........ccccccc+ccccccc...ccc......cc+ccc...xxxxx.....xxxxxxxxx");
    strcpy(vgrid[32], "xxxxxxxxxwwwwwxx.........c$$*.c$$5$+.5.c...+5c......c%%%%c......xxx3...xxxxxxxxx");
    strcpy(vgrid[33], "xxxxxxxxxwwwwwx....2.....c$.c+cccccc.%.c...ccc......c%%%%c....xxxxx....xxxxxxxxx");
    strcpy(vgrid[34], "xxxxxxxxxwwwwwx..........c..c..........c............cccccc......xxx....xxxxxxxxx");
    strcpy(vgrid[35], "xxxxxxxxxwwxxxxxxx.......ccccc+ccccccccc.........................xx....xxxxxxxxx");
    strcpy(vgrid[36], "xxxxxxxxxwxx.....xxxx........c...c.................2...................xxxxxxxxx");
    strcpy(vgrid[37], "xxxxxxxxxxx.........xxxx...........2....xxxx...........................xxxxxxxxx");
    strcpy(vgrid[38], "xxxxxxxxx..............xxxx..........xxxx..x...........................xxxxxxxxx");
    strcpy(vgrid[39], "xxxxxxxxx.................xxxxx++xxxxx.....xx............xx...x........xxxxxxxxx");
    strcpy(vgrid[40], "xxxxxxxxx.....................c..c..........xxxxx..........xxxxx.......xxxxxxxxx");
    strcpy(vgrid[41], "xxxxxxxxx.......cccc..........c..c...cccc......xxx...........x.........xxxxxxxxx");
    strcpy(vgrid[42], "xxxxxxxxx.......c..c..........c++c...c..c........xxx.........x.........xxxxxxxxx");
    strcpy(vgrid[43], "xxxxxxxxx.......c..c..........c..c...c..c..........xxx.................xxxxxxxxx");
    strcpy(vgrid[44], "xxxxxxxxx....cccc++cccccccccccc++ccccc..ccccccc......xxx...............xxxxxxxxx");
    strcpy(vgrid[45], "xxxxxxxxx....c..........1.....................c........xxx.............xxxxxxxxx");
    strcpy(vgrid[46], "xxxxxxxxx.cccc.....w....w....%1.....w.....%...c..........xxx...........xxxxxxxxx");
    strcpy(vgrid[47], "xxxxxxxxx.c1.+....www..www..%%%....www...%%%1.c...........xxxxxxxxx....xxxxxxxxx");
    strcpy(vgrid[48], "xxxxxxxxx.cccc.....w....w....%......w.....%...c..................xxx...xxxxxxxxx");
    strcpy(vgrid[49], "xxxxxxxxx....c.......5........................c....................xxxxxxxxxxxxx");
    strcpy(vgrid[50], "xxxxxxxxx....ccc....%%%%%....cccccccccccccccccc........................xxxxxxxxx");
    strcpy(vgrid[51], "xxxxxxxxx......cc...........cc.........................................xxxxxxxxx");
    strcpy(vgrid[52], "xxxxxxxxx.......cccccc+cccccc..........................................xxxxxxxxx");
    strcpy(vgrid[53], "xxxxxxxxx........cc.......cc...........................................xxxxxxxxx");
    strcpy(vgrid[54], "xxxxxxxxx.........cc.....cc.....................cccccccccccccccccccccccxxxxxxxxx");
    strcpy(vgrid[55], "xxxxxxxxx..........ccc+ccc......................c......vvv.............xxxxxxxxx");
    strcpy(vgrid[56], "xxxxxxxxx..........ccc.c........................c......v5+...vvvvv.....xxxxxxxxx");
    strcpy(vgrid[57], "xxxxxxxxx..........ccc.c........................c......vvv...v.5.v.....xxxxxxxxx");
    strcpy(vgrid[58], "xxxxxxxxxccccccccccccc.ccc......................c............v..5v.....xxxxxxxxx");
    strcpy(vgrid[59], "xxxxxxxxx..........c.....cccccccccccccccccccccccccccc..........vv+vv...xxxxxxxxx");
    strcpy(vgrid[60], "xxxxxxxxx..........c............................+................5111..xxxxxxxxx");
    strcpy(vgrid[61], "xxxxxxxxx..........c.{([.c......................+................5.....xxxxxxxxx");
    strcpy(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    mons_array[0] = MONS_DEATH_YAK;
    mons_array[1] = MONS_PLANT;
    mons_array[2] = MONS_GRIFFON;
    mons_array[3] = MONS_KILLER_BEE;
    mons_array[4] = MONS_OGRE;
    mons_array[5] = MONS_OKLOB_PLANT;
    mons_array[6] = MONS_WANDERING_MUSHROOM;

    return MAP_ENCOMPASS;
}


static char fort_yaktaur(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // by Matthew Ludivico (mll6@lehigh.edu)

    for (unsigned char i = 0; i < 81; i++)
        strcpy(vgrid[i], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    strcat(vgrid[36], ".........@....wwwwwwwwwwwwwwwwwxxxxxxxxx");
    strcat(vgrid[37], ".ccccc.......ww....wwww....wwwwxxxxxxxxx");
    strcat(vgrid[38], ".c$c%c......ww.ccccccccc.......xxxxxxxxx");
    strcat(vgrid[39], ".c+c+c......ww.c.%$....ccccccccxxxxxxxxx");
    strcat(vgrid[40], ".c...+......ww.c*.115..c$$+|*|cxxxxxxxxx");
    strcat(vgrid[41], ".c1..c.....ww..c...55+ccc+cxx=cxxxxxxxxx");
    strcat(vgrid[42], ".ccccc.....ww..ccccccc....c|=*cxxxxxxxxx");
    strcat(vgrid[43], "............ww.......c5...cxx=cxxxxxxxxx");
    strcat(vgrid[44], "....6.ccccc.ww.w...2.+51..c|1.cxxxxxxxxx");      // last 1 here was 7
    strcat(vgrid[45], "....63+...c..wwww..21+51..c2.2cxxxxxxxxx");
    strcat(vgrid[46], "....6.ccccc..wwwwww..c5...cc+ccxxxxxxxxx");
    strcat(vgrid[47], "............wwwwwww..c........cxxxxxxxxx");
    strcat(vgrid[48], "............wwwwwww..ccccccccccxxxxxxxxx");
    strcat(vgrid[49], "...........ww1w..www...........xxxxxxxxx");
    strcat(vgrid[50], ".......566.www.....www.........xxxxxxxxx");
    strcat(vgrid[51], ".........1ww....ccccc..........xxxxxxxxx");
    strcat(vgrid[52], ".....566.w......+...c..........xxxxxxxxx");
    strcat(vgrid[53], ".........www....ccccc..........xxxxxxxxx");
    strcat(vgrid[54], "...........ww............wwwwwwxxxxxxxxx");
    strcat(vgrid[55], ".......3....wwwww......www.....xxxxxxxxx");
    strcat(vgrid[56], "......666.......ww...www.......xxxxxxxxx");
    strcat(vgrid[57], ".....cc+cc.......wwwww.........xxxxxxxxx");
    strcat(vgrid[58], ".....c...c.....................xxxxxxxxx");
    strcat(vgrid[59], ".....ccccc.....................xxxxxxxxx");
    strcat(vgrid[60], "...............................xxxxxxxxx");
    strcat(vgrid[61], "...............................xxxxxxxxx");
    strcat(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    mons_array[0] = MONS_YAKTAUR;
    mons_array[1] = MONS_DEATH_YAK;
    mons_array[2] = MONS_MINOTAUR;
    mons_array[3] = RANDOM_MONSTER;
    mons_array[4] = MONS_YAK;
    mons_array[5] = MONS_GNOLL;
    mons_array[6] = RANDOM_MONSTER;

    return MAP_SOUTHEAST;
}


static char box_level(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // by John Savard
    UNUSED( mons_array );

    strcpy(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[7], "xxxxxxxxx.................xx.............x...................^.........xxxxxxxxx");
    strcpy(vgrid[8], "xxxxxxxxx.................xx...xxxxxx....x.xxxxxxx.xxxxxxxxxxxxxxxxxxx.xxxxxxxxx");
    strcpy(vgrid[9], "xxxxxxxxx.................xx...xx.0......x.x........x......x.........x.xxxxxxxxx");
    strcpy(vgrid[10], "xxxxxxxxx..$..............xx...xx........x.x........x.....%x.x..*..xxx.xxxxxxxxx");
    strcpy(vgrid[11], "xxxxxxxxx......................xx........x.x........x.xxxxxx.x.....x...xxxxxxxxx");
    strcpy(vgrid[12], "xxxxxxxxx......................xx....%...x.x........x.x......xxxxxxx.x.xxxxxxxxx");
    strcpy(vgrid[13], "xxxxxxxxx.................xx...xx........x.x........x.x.xxxxxx.......x.xxxxxxxxx");
    strcpy(vgrid[14], "xxxxxxxxx.................xx...xx........x.x..{.....x.x..............x.xxxxxxxxx");
    strcpy(vgrid[15], "xxxxxxxxx.............0...xx...xxxxxxxxxxx.xxxxxxxxxx.xxxxxxxxxxxxxxxx.xxxxxxxxx");
    strcpy(vgrid[16], "xxxxxxxxx.................xx...........................................xxxxxxxxx");
    strcpy(vgrid[17], "xxxxxxxxxxxxxxxxxxxxxxxxxxxx...xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[18], "xxxxxxxxxxxxxxxxxxxxxxxxxxxx...xxx}x.........................>=........xxxxxxxxx");
    strcpy(vgrid[19], "xxxxxxxxx..................x...xxx.x.xxx+xxxxxxxxxxxxxxxx+xxxxx........xxxxxxxxx");
    strcpy(vgrid[20], "xxxxxxxxx..xxxxxxxxxxxxxx..x...xxx.x.x0...x..0..............0.x........xxxxxxxxx");
    strcpy(vgrid[21], "xxxxxxxxx..x............x..x...xxx.x.x....x...................x........xxxxxxxxx");
    strcpy(vgrid[22], "xxxxxxxxx....xxxxxxxxx..x..x...xxx.x.x....x...................x......8*xxxxxxxxx");
    strcpy(vgrid[23], "xxxxxxxxx..x.x....0..x..x..x...xxx...x...%x...................x......*|xxxxxxxxx");
    strcpy(vgrid[24], "xxxxxxxxx..x.x..........x..x...xxxxxxxxxxxxxxxxxxxxxx.xxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[25], "xxxxxxxxx..x.x*......x..x..x..........x...........0...x...%............xxxxxxxxx");
    strcpy(vgrid[26], "xxxxxxxxx..x.xxxxxxxxx..x..=..........x.xxxxxxxxxxxxx.x................xxxxxxxxx");
    strcpy(vgrid[27], "xxxxxxxxx..x......0.....xxxxxxx.......x.x...x...x...x.x................xxxxxxxxx");
    strcpy(vgrid[28], "xxxxxxxxx..xxxxxxxxxxxxxxxxxxxx..0....x...x.x.x.x.x.x.x......0.........xxxxxxxxx");
    strcpy(vgrid[29], "xxxxxxxxx..........^.........xx.......x.x.x.x.x.x.x...+................xxxxxxxxx");
    strcpy(vgrid[30], "xxxxxxxxxcccccccccccccccccc..xx.......x.x$x...x...xxxxx................xxxxxxxxx");
    strcpy(vgrid[31], "xxxxxxxxxc...........9....c..xx.......x.x.xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[32], "xxxxxxxxxc......c............xx.......x.x.x...x..0.....................xxxxxxxxx");
    strcpy(vgrid[33], "xxxxxxxxxc.....|c............xx.......x.x.x.x.x........................xxxxxxxxx");
    strcpy(vgrid[34], "xxxxxxxxxc...........9....c..xx.......x.x...x.x........................xxxxxxxxx");
    strcpy(vgrid[35], "xxxxxxxxxcccccccccccccccccc..xx.......x.xxxxx.x........................xxxxxxxxx");
    strcpy(vgrid[36], "xxxxxxxxx....................xx.......x.x.....=....................*...xxxxxxxxx");
    strcpy(vgrid[37], "xxxxxxxxx....................xx.......x.x.xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[38], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.......x.x.x...........................(xxxxxxxxx");
    strcpy(vgrid[39], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.xxxxxxx.x$x..xxxx.xxxxxxxxxxxxxxxxxxxx.xxxxxxxxx");
    strcpy(vgrid[40], "xxxxxxxxx...............................x.x..x.......................x.xxxxxxxxx");
    strcpy(vgrid[41], "xxxxxxxxx..xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.x..x.xxxxxxxxxxxxx.........x.xxxxxxxxx");
    strcpy(vgrid[42], "xxxxxxxxx.............)xxx................x..x.xxxxxxxxxxxxx.........x.xxxxxxxxx");
    strcpy(vgrid[43], "xxxxxxxxx..............xxx.xxxxxxxxxxxxxxxx..x.xxxxxxxxxxxxx.........x.xxxxxxxxx");
    strcpy(vgrid[44], "xxxxxxxxx..............xxx...................x.x...........xxxxx+xxxxx.xxxxxxxxx");
    strcpy(vgrid[45], "xxxxxxxxx..............xxxxxxxxxxxxxxxxxxxxxxx.x..$........x.........x.xxxxxxxxx");
    strcpy(vgrid[46], "xxxxxxxxx......9.......xxxxxxxxxxxxxxxxxxxxxxx.x...........x........%x.xxxxxxxxx");
    strcpy(vgrid[47], "xxxxxxxxx..............xxxxxxxxxxxxxxxxxxxxxxx.x.0.........x0........x.xxxxxxxxx");
    strcpy(vgrid[48], "xxxxxxxxx..............xxxxxxxxxxxxxxxxxxxxxxx.x.......$...x.........x.xxxxxxxxx");
    strcpy(vgrid[49], "xxxxxxxxx..............xxxxxxxxxxxxxxxxxxxxxxx.x...........xxxxxxxxxxx.xxxxxxxxx");
    strcpy(vgrid[50], "xxxxxxxxx..............xxxxxxxxxxxxxxxxxxxxxxx.xxxxxxxxxxx.x...........xxxxxxxxx");
    strcpy(vgrid[51], "xxxxxxxxx..............xxxxxxxxxxxxxxxxxxxxxxx.............x...........xxxxxxxxx");
    strcpy(vgrid[52], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx+xxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[53], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[54], "xxxxxxxxx.xxxxxxxxxxxxxxxxxxx.xxxxxxxxxxxxxxxxxx.xxxxxxx=xxxxxx.xxxxxx.xxxxxxxxx");
    strcpy(vgrid[55], "xxxxxxxxx.....xx.................xxxxxxxxxxx.......x........x.....x....xxxxxxxxx");
    strcpy(vgrid[56], "xxxxxxxxx....0xx.................xxxxxxxxxxx.%.....x.0......x...0.x....xxxxxxxxx");
    strcpy(vgrid[57], "xxxxxxxxx.....xx.9...............xxxxxxxxxxx.......x........x.%...x..$.xxxxxxxxx");
    strcpy(vgrid[58], "xxxxxxxxx.....xx.................xxxxxxxxxxx.......x........x.....x....xxxxxxxxx");
    strcpy(vgrid[59], "xxxxxxxxx.....xx.................xxxxxxxxxxx.......x........x.....x..0.xxxxxxxxx");
    strcpy(vgrid[60], "xxxxxxxxx....0xx.................xxxxxxxxxxx.......x$.......x.....x....xxxxxxxxx");
    strcpy(vgrid[61], "xxxxxxxxx]....xx................*xxxxxxxxxxx......[x........x.....x$...xxxxxxxxx");
    strcpy(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    return MAP_ENCOMPASS;
}


static char vestibule_map(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // this is the vestibule

    strcpy(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxvvvvvvvxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[7], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx..v.....v..xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[8], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.....v.....v.....xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[9], "xxxxxxxxxxxxxxxxxxxxxxxxxxxx........v.....v........xxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[10], "xxxxxxxxxxxxxxxxxxxxxxxxxx..........v..A..v..........xxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[11], "xxxxxxxxxxxxxxxxxxxxxxxx............v.....v............xxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[12], "xxxxxxxxxxxxxxxxxxxxxxx.............v.....v.............xxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[13], "xxxxxxxxxxxxxxxxxxxxxx..............vvv+vvv..............xxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[14], "xxxxxxxxxxxxxxxxxxxxx.....................................xxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[15], "xxxxxxxxxxxxxxxxxxxx.......................................xxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[16], "xxxxxxxxxxxxxxxxxxx.........................................xxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[17], "xxxxxxxxxxxxxxxxxx...........................................xxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[18], "xxxxxxxxxxxxxxxxx.............................................xxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[19], "xxxxxxxxxxxxxxxx...............................................xxxxxxxxxxxxxxxxx");
    strcpy(vgrid[20], "xxxxxxxxxxxxxxx.................................................xxxxxxxxxxxxxxxx");
    strcpy(vgrid[21], "xxxxxxxxxxxxxx...................................................xxxxxxxxxxxxxxx");
    strcpy(vgrid[22], "xxxxxxxxxxxxx.....................................................xxxxxxxxxxxxxx");
    strcpy(vgrid[23], "xxxxxxxxxxxxx.....................................................xxxxxxxxxxxxxx");
    strcpy(vgrid[24], "xxxxxxxxxxxx.......................................................xxxxxxxxxxxxx");
    strcpy(vgrid[25], "xxxxxxxxxxxx.......................................................xxxxxxxxxxxxx");
    strcpy(vgrid[26], "xxxxxxxxxxx.........................................................xxxxxxxxxxxx");
    strcpy(vgrid[27], "xxxxxxxxxxx............................{............................xxxxxxxxxxxx");
    strcpy(vgrid[28], "xxxxxxxxxxx.........................................................xxxxxxxxxxxx");
    strcpy(vgrid[29], "xxxxxxxxxx...l.l.....................................................xxxxxxxxxxx");
    strcpy(vgrid[30], "xxxxxxxxxx..l.l.l.l..................................................xxxxxxxxxxx");
    strcpy(vgrid[31], "xxxxxxxxxx.l.l.l.l.l.................................................xxxxxxxxxxx");
    strcpy(vgrid[32], "xxxxxxxxx.l.l.l.l.l...................................................xxxxxxxxxx");
    strcpy(vgrid[33], "xxxxxxxxxl.l.l.l.l.l..................................................xxxxxxxxxx");
    strcpy(vgrid[34], "xxxxxxxxx.l.l.l.A.l.l.................}1].............................=Axxxxxxxx");
    strcpy(vgrid[35], "xxxxxxxxxl.l.l.l.l.l.l.................)..............................xxxxxxxxxx");
    strcpy(vgrid[36], "xxxxxxxxx.l.l.l.l.l.l.................................................xxxxxxxxxx");
    strcpy(vgrid[37], "xxxxxxxxxx.l.l.l.l.l.l...............................................xxxxxxxxxxx");
    strcpy(vgrid[38], "xxxxxxxxxx..l.l.l.l..................................................xxxxxxxxxxx");
    strcpy(vgrid[39], "xxxxxxxxxx.....l.l...................................................xxxxxxxxxxx");
    strcpy(vgrid[40], "xxxxxxxxxxx......................[...........(......................xxxxxxxxxxxx");
    strcpy(vgrid[41], "xxxxxxxxxxx.........................................................xxxxxxxxxxxx");
    strcpy(vgrid[42], "xxxxxxxxxxx.........................................................xxxxxxxxxxxx");
    strcpy(vgrid[43], "xxxxxxxxxxxx.......................................................xxxxxxxxxxxxx");
    strcpy(vgrid[44], "xxxxxxxxxxxx.......................................................xxxxxxxxxxxxx");
    strcpy(vgrid[45], "xxxxxxxxxxxxx.....................................................xxxxxxxxxxxxxx");
    strcpy(vgrid[46], "xxxxxxxxxxxxx.....................................................xxxxxxxxxxxxxx");
    strcpy(vgrid[47], "xxxxxxxxxxxxxx...................................................xxxxxxxxxxxxxxx");
    strcpy(vgrid[48], "xxxxxxxxxxxxxxx....................wwwww........................xxxxxxxxxxxxxxxx");
    strcpy(vgrid[49], "xxxxxxxxxxxxxxxx..................wwwwwwww.....................xxxxxxxxxxxxxxxxx");
    strcpy(vgrid[50], "xxxxxxxxxxxxxxxxx..............wwwwwwwwwwwww..................xxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[51], "xxxxxxxxxxxxxxxxxx...........w..wwww..wwwww..w...............xxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[52], "xxxxxxxxxxxxxxxxxxx..........w...ww.....ww..wwwww...........xxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[53], "xxxxxxxxxxxxxxxxxxxx.........ww......ww....wwwwwwwww.......xxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[54], "xxxxxxxxxxxxxxxxxxxxx.........ww....wwww...wwwwwwwwww.....xxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[55], "xxxxxxxxxxxxxxxxxxxxxx.........ww....ww....wwwwwwwwwww...xxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[56], "xxxxxxxxxxxxxxxxxxxxxxx........wwww.......wwwwwwwwwwwwwwxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[57], "xxxxxxxxxxxxxxxxxxxxxxxx......wwwwwww....wwwwwwwwwwwwwwxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[58], "xxxxxxxxxxxxxxxxxxxxxxxxxx...wwwwwwwwwwAwwwwwwwwwwwwwxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[59], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxwwwwwwwwwwwwwwwwwwwwwwwxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[60], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxwwwwwwwwwwwwwwwwwxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[61], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxwwwwwwwwwwwxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    mons_array[0] = MONS_GERYON;
    mons_array[1] = RANDOM_MONSTER;
    mons_array[2] = RANDOM_MONSTER;
    mons_array[3] = RANDOM_MONSTER;
    mons_array[4] = RANDOM_MONSTER;
    mons_array[5] = RANDOM_MONSTER;
    mons_array[6] = RANDOM_MONSTER;

    return MAP_ENCOMPASS;
}


static char castle_dis(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // Dispater's castle - rest of level filled up with plan_4 (irregular city)

    strcpy(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6], "xxxxxxxxvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvxxxxxxxx");
    strcpy(vgrid[7], "xxxxxxxxv..............................................................vxxxxxxxx");
    strcpy(vgrid[8], "xxxxxxxxv..vvvvvvvvv........................................vvvvvvvvv..vxxxxxxxx");
    strcpy(vgrid[9], "xxxxxxxxv..v3.....|v........................................v|.....2v..vxxxxxxxx");
    strcpy(vgrid[10], "xxxxxxxxv..v.vv+vvvv.v.v.v.v.v.v.v.v.v..v.v.v.v.v.v.v.v.v.v.vvvv+vv.v..vxxxxxxxx");
    strcpy(vgrid[11], "xxxxxxxxv..v.v.....vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv.....v.v..vxxxxxxxx");
    strcpy(vgrid[12], "xxxxxxxxv..v|v.....+$$v$$+$$v||vvvvvvvvvvvvvvvvv$$$$v4.4.v$$v.....v|v..vxxxxxxxx");
    strcpy(vgrid[13], "xxxxxxxxv..vvvv+vvvv$$+$$v$$+||v...............v$$$$+.4.4+$$v+vv+vvvv..vxxxxxxxx");
    strcpy(vgrid[14], "xxxxxxxxv....vv.vvvvvvvvvvvvvvvv.v..v..v..v..v.v$$$$v4.4.v$$+||v.vv5...vxxxxxxxx");
    strcpy(vgrid[15], "xxxxxxxxv...vvv................v...............vvvvvvvvvvvvvvvvv.vvv...vxxxxxxxx");
    strcpy(vgrid[16], "xxxxxxxxv...5vv................+...............+.................vv....vxxxxxxxx");
    strcpy(vgrid[17], "xxxxxxxxv...vvv+vvvvvvvvvvvvvvvv.v..v..v..v..v.vvvvvvvvvvvvvvvvv.vvv...vxxxxxxxx");
    strcpy(vgrid[18], "xxxxxxxxv....vv..v.+$$$$$v.....v...............vvvvvvvvvvvvvvvvv.vv5...vxxxxxxxx");
    strcpy(vgrid[19], "xxxxxxxxv...vvv..v.v$$$$$v.....v...............vv|$|$|vv|$|$|$vv.vvv...vxxxxxxxx");
    strcpy(vgrid[20], "xxxxxxxxv...5vv..v.vvvvvvv.....vvvvv.......vvvvvv$|$|$++$|$|$|vv.vv....vxxxxxxxx");
    strcpy(vgrid[21], "xxxxxxxxv...vvv..v...............v.vvvv+vvvvvvvvvvvvvvvvvvvvv+vv.vvv...vxxxxxxxx");
    strcpy(vgrid[22], "xxxxxxxxv....vvv+v..........vvvvv.4vvv...vvvvvvvvvvvvvvvvvvvv+vv.vv5...vxxxxxxxx");
    strcpy(vgrid[23], "xxxxxxxxv...vvv..v.v..v..v....2vvv+vv5...5vvvvvvv.4.4.vv.4.4.4vv.vvv...vxxxxxxxx");
    strcpy(vgrid[24], "xxxxxxxxv...5vv.................vv|vvv...vvvvv.++4.4.4++4.4.4.vv.vv....vxxxxxxxx");
    strcpy(vgrid[25], "xxxxxxxxv...vvv.................1vOvv5...5vvvv.vvvvvvvvvvvvvvvvv.vvv...vxxxxxxxx");
    strcpy(vgrid[26], "xxxxxxxxv....vv.................vv|vvv...vvvvv.vvvvvvvvvvvvvvvvv.vv5...vxxxxxxxx");
    strcpy(vgrid[27], "xxxxxxxxv...vvv.v..v..v..v....3vvv+vv5...5vvvv...................vvv...vxxxxxxxx");
    strcpy(vgrid[28], "xxxxxxxxv...5vv.............vvvvv.4vvv...vvvvvvvvvvvvvvvvvvvvvvv.vv....vxxxxxxxx");
    strcpy(vgrid[29], "xxxxxxxxv..vvvv+vvvv.............v.vv5...5vvvvvvvvvvvvvvvvvvvvvv+vvvv..vxxxxxxxx");
    strcpy(vgrid[30], "xxxxxxxxv..v|v.....vvvvvvvvvvvvvvvvvvv...vvvvvvvvvvvvvvvvvvvv.....v|v..vxxxxxxxx");
    strcpy(vgrid[31], "xxxxxxxxv..v.v.....vvvvvvvvvvvvvvvvvvvv+vvvvvvvvvvvvvvvvvvvvv.....v.v..vxxxxxxxx");
    strcpy(vgrid[32], "xxxxxxxxv..v.vv+vvvv5.............5.........5..............5vvvv+vv.v..vxxxxxxxx");
    strcpy(vgrid[33], "xxxxxxxxv..v2.....|v........................................v|.....3v..vxxxxxxxx");
    strcpy(vgrid[34], "xxxxxxxxv..vvvvvvvvv........................................vvvvvvvvv..vxxxxxxxx");
    strcpy(vgrid[35], "xxxxxxxxv............................{.[.(.............................vxxxxxxxx");

    mons_array[0] = MONS_DISPATER;
    mons_array[1] = MONS_FIEND;
    mons_array[2] = MONS_ICE_FIEND;
    mons_array[3] = MONS_IRON_DEVIL;
    mons_array[4] = MONS_METAL_GARGOYLE;
    mons_array[5] = RANDOM_MONSTER;
    mons_array[6] = RANDOM_MONSTER;

    return MAP_NORTH_DIS;
}


static char asmodeus(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{

    strcpy(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[7], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.xxxxxxxxxx....xxxxxxxxxxxxxxx.xxxxxxxxxxxx");
    strcpy(vgrid[8], "xxxxxxxxxxxxxxxxxxxxxxxxx............................xxxxxxxxxxxxxx..xxxxxxxxxxx");
    strcpy(vgrid[9], "xxxxxxxxxxxxxxxxxxxxxxxxxx..............................xxxxxxxxxx....xxxxxxxxxx");
    strcpy(vgrid[10], "xxxxxxxxxxxxxxxxxxxxx...xxx................................xxxxxx....xxxxxxxxxxx");
    strcpy(vgrid[11], "xxxxxxxxxxxx.x.xxxxx.........................................xxx....xxxxxxxxxxxx");
    strcpy(vgrid[12], "xxxxxxxxxxxx....xx.....................4......................xx...xxxxxxxxxxxxx");
    strcpy(vgrid[13], "xxxxxxxxxxx......x......................llllllllllllll.........x..xxxxxxxxxxxxxx");
    strcpy(vgrid[14], "xxxxxxxxxxx..xx..................lllllllllllllllllllllllll........xxxxxxxxxxxxxx");
    strcpy(vgrid[15], "xxxxxxxxxx...xxx....0..........llllllllllllllllllllllllll........xx...xxxxxxxxxx");
    strcpy(vgrid[16], "xxxxxxxxx....xxx.............llllllllllllllllllllllllllll..............xxxxxxxxx");
    strcpy(vgrid[17], "xxxxxxxxxx....xx...........lllllllllllllllllllllllllllll...............xxxxxxxxx");
    strcpy(vgrid[18], "xxxxxxxxxxxx..............llllllllllllllllllllllllllllll...2..xx...0...xxxxxxxxx");
    strcpy(vgrid[19], "xxxxxxxxxxxxx...........lllllllllllllllllll.......llllll......xx......xxxxxxxxxx");
    strcpy(vgrid[20], "xxxxxxxxxxxxxx.......llllllllllllllllll............llllll.............xxxxxxxxxx");
    strcpy(vgrid[21], "xxxxxxxxxxxxxxx......lllllllll..........4.........4.lllllll..........xxxxxxxxxxx");
    strcpy(vgrid[22], "xxxxxxxxxx...xx...ll3lllll......4...................llllllll......x.xxxxxxxxxxxx");
    strcpy(vgrid[23], "xxxxxxxxx.......lllll.l................................llll.......xxxxxxxxxxxxxx");
    strcpy(vgrid[24], "xxxxxxxxxx..4..llllll...cccccccc+c+c+c+c+c+c+c+c+c+c....lll......xxxxxxxxxxxxxxx");
    strcpy(vgrid[25], "xxxxxxxxxxx..lllllll..4.c.....c....................c....llll.....xxxxxxxxxxxxxxx");
    strcpy(vgrid[26], "xxxxxxxxxx...llllll.....c.V.V.+....0.....3.....0...c.....llll....x..xxxxxxxxxxxx");
    strcpy(vgrid[27], "xxxxxxxxx...llllll...l..c.....c....................c....lllll........xxxxxxxxxxx");
    strcpy(vgrid[28], "xxxxxxxxxx...lllll..ll..c..5..cccccccccccccccccccccc.4..llllll........xxxxxxxxxx");
    strcpy(vgrid[29], "xxxxxxxxx...lllll..llll.c.....c...............c....c....lllllll.......xxxxxxxxxx");
    strcpy(vgrid[30], "xxxxxxxxx...lllll..llll.c.V.V.c.......0.......c....c....lllllll.......xxxxxxxxxx");
    strcpy(vgrid[31], "xxxxxxxxxx...lllll..lll.c.....+...............+....c...lllllll........xxxxxxxxxx");
    strcpy(vgrid[32], "xxxxxxxxxxx..lllll...ll.cccccccccc....0.......c....c...llllllll........xxxxxxxxx");
    strcpy(vgrid[33], "xxxxxxxxxx...lllll..4...c|$$||$$|c............c.0..c...llllllll........xxxxxxxxx");
    strcpy(vgrid[34], "xxxxxxxxx...lllll.......c$$$$$$$$cccccccccccccc....c...lllllll.........xxxxxxxxx");
    strcpy(vgrid[35], "xxxxxxxxx...lllll.......c$$|2|$$|c..0.........+....c...lllllll........xxxxxxxxxx");
    strcpy(vgrid[36], "xxxxxxxxxx.lllllll......c|$$$$$$$c........9...c....c....llllllll.....xxxxxxxxxxx");
    strcpy(vgrid[37], "xxxxxxxxxx.lllllll......c$|$|$$|$c+ccccccccccccccccc....lllllll......xxxxxxxxxxx");
    strcpy(vgrid[38], "xxxxxxxxxx..llllll......cccccccc+c.....9.......c.........llllll......x.xxxxxxxxx");
    strcpy(vgrid[39], "xxxxxxxxxx..lllllll.....c$$$$$$+3c.....8...3...c.....4...llllll........xxxxxxxxx");
    strcpy(vgrid[40], "xxxxxxxxxx..llllllll....c$$$$$$c.c.....9.......c..ll....llllll.........xxxxxxxxx");
    strcpy(vgrid[41], "xxxxxxxxxx...llllll..4..c$$2$$$c.ccccccccccccc+c.lll...lllllll...0....xxxxxxxxxx");
    strcpy(vgrid[42], "xxxxxxxxxxx..llllll.....c$$$$$$c..+............c.ll...lllllll..........xxxxxxxxx");
    strcpy(vgrid[43], "xxxxxxxxxxx..llllllll...ccccccccc+cccccccccccccc.....lllllll...........xxxxxxxxx");
    strcpy(vgrid[44], "xxxxxxxxxxxx..llllllll.........cc..........cc........lllllll.......x..xxxxxxxxxx");
    strcpy(vgrid[45], "xxxxxxxxxxxxx.llllllllll.......ccc.........cc......lllllllll.......xxxxxxxxxxxxx");
    strcpy(vgrid[46], "xxxxxxxxxx....lllllllllll...4...cc.....2.2.cc....llllllllll.4.......xxxxxxxxxxxx");
    strcpy(vgrid[47], "xxxxxxxxx....4.lllllllllllll....cccccccc+cccc..lllllllllll.....xx....xxxxxxxxxxx");
    strcpy(vgrid[48], "xxxxxxxxxx.....llllllllllllll...cccccccc+cccc..llllllllll......xx....xxxxxxxxxxx");
    strcpy(vgrid[49], "xxxxxxxxxxx.....lllllllllllllll..cc......cc...lllllllllll...........xxxxxxxxxxxx");
    strcpy(vgrid[50], "xxxxxxxxxxx.....llllllllllllll...ccO1....cc.4..lllllllll...........xxxxxxxxxxxxx");
    strcpy(vgrid[51], "xxxxxxxxxxxx.....lllllllllllll...cc......cc....lllllllll.......xx.xxxxxxxxxxxxxx");
    strcpy(vgrid[52], "xxxxxxxxxxxx.......llllllllllll..cccccccccc...lllllllll........xxxxxxxxxxxxxxxxx");
    strcpy(vgrid[53], "xxxxxxxxx.........llllllllllllll.cccccccccc.lllllllllll.......xxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[54], "xxxxxxxxxx....0...llllllllllllll............lllllllll....0....xxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[55], "xxxxxxxxxx.......4.lllllllllllllll..4....lllllllll...........xxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[56], "xxxxxxxxxxx..........llllllllllllll....lllllll....4.....x........xxxxxxxxxxxxxxx");
    strcpy(vgrid[57], "xxxxxxxxxxx...xx.........lllllllllllllllll...................xx{xxxxxxxxxxxxxxxx");
    strcpy(vgrid[58], "xxxxxxxxxxxxx..xx................lllllll.....................xxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[59], "xxxxxxxxxxxxxxxxx.........xxx.................xxxxxx......xxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[60], "xxxxxxxxxxxxxxxxxxx....xxxxxxxx...xxx......xxxxxxxxxx.......xxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[61], "xxxxxxxxxxxxxxxxxxxx(xxxxxxxxxxxx[xxxxx...xxxxxxxxxxxxxx...xxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    mons_array[0] = MONS_ASMODEUS;
    mons_array[1] = MONS_FIEND;
    mons_array[2] = MONS_BALRUG;
    mons_array[3] = MONS_MOLTEN_GARGOYLE;
    mons_array[4] = MONS_SERPENT_OF_HELL;
    mons_array[5] = RANDOM_MONSTER;
    mons_array[6] = RANDOM_MONSTER;

    return MAP_ENCOMPASS;
}

static char antaeus(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // bottom of Cocytus. This needs work

    strcpy(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[7], "xxxxxxxxxxxxxxxxxxxxxxxxxxxx........................xxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[8], "xxxxxxxxxxxxxxxxxxxxxxxxxxx..........................xxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[9], "xxxxxxxxxxxxxxxxxxxxxxxxxx............................xxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[10], "xxxxxxxxxxxxxxxxxxxxxxxxx..............................xxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[11], "xxxxxxxxxxxxxxxxxxxxxxxx................................xxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[12], "xxxxxxxxxxxxxxxxxxxxxxx....cccccccccccc..cccccccccccc....xxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[13], "xxxxxxxxxxxxxxxxxxxxxx....ccccccccccccc2.ccccccccccccc....xxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[14], "xxxxxxxxxxxxxxxxxxxxx....cc..........................cc....xxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[15], "xxxxxxxxxxxxxxxxxxxx....cc............................cc....xxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[16], "xxxxxxxxxxxxxxxxxxx....cc...wwwwwwwwwwwwwwwwwwwwwwww...cc....xxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[17], "xxxxxxxxxxxxxxxxxx....cc...wwwwwwwwwwwwwwwwwwwwwwwwww...cc....xxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[18], "xxxxxxxxxxxxxxxxx....cc...wwwwwwwwwwwwwwwwwwwwwwwwwwww...cc....xxxxxxxxxxxxxxxxx");
    strcpy(vgrid[19], "xxxxxxxxxxxxxxxx....cc...ww.......3..........3.......ww...cc....xxxxxxxxxxxxxxxx");
    strcpy(vgrid[20], "xxxxxxxxxxxxxxx....cc...ww............................ww...cc....xxxxxxxxxxxxxxx");
    strcpy(vgrid[21], "xxxxxxxxxxxxxx....cc...ww....cccccccccccccccccccccc....ww...cc....xxxxxxxxxxxxxx");
    strcpy(vgrid[22], "xxxxxxxxxxxxx....cc...ww....cccccccccccccccccccccccc....ww...cc....xxxxxxxxxxxxx");
    strcpy(vgrid[23], "xxxxxxxxxxxx....cc...ww....cc......................cc....ww...cc....xxxxxxxxxxxx");
    strcpy(vgrid[24], "xxxxxxxxxxx....cc...ww....cc...T................T...cc....ww...cc....xxxxxxxxxxx");
    strcpy(vgrid[25], "xxxxxxxxxx....cc...ww....cc..........wwwwww..........cc....ww...cc....xxxxxxxxxx");
    strcpy(vgrid[26], "xxxxxxxxx....cc...ww....cc.......wwwwwwwwwwwwww.......cc....ww...cc....xxxxxxxxx");
    strcpy(vgrid[27], "xxxxxxxxx....cc...ww...cc.....wwwwwwwwwwwwwwwwwwww.....cc...ww...cc....xxxxxxxxx");
    strcpy(vgrid[28], "xxxxxxxxx....cc..www..cc....wwwwwwwwwccccccwwwwwwwww....cc..www..cc....xxxxxxxxx");
    strcpy(vgrid[29], "xxxxxxxxx....cc..www.cc....wwwwwwwwccc2O12cccwwwwwwww....cc.www..cc....xxxxxxxxx");
    strcpy(vgrid[30], "xxxxxxxxx....cc..www.cc...wwwwwwwwcc2+....+2ccwwwwwwww...cc.www..cc....xxxxxxxxx");
    strcpy(vgrid[31], "xxxxxxxxx....cc..www.cc...wwwwwwwwcc+cc++cc+ccwwwwwwww...cc.www..cc....xxxxxxxxx");
    strcpy(vgrid[32], "xxxxxxxxx....cc..www..c..wwwwwwwwwc|||c..c$$$cwwwwwwwww..c..www..cc....xxxxxxxxx");
    strcpy(vgrid[33], "xxxxxxxxx....cc..wwww.c.wwwwwwwwwwc|||c..c$$$cwwwwwwwwww.c.wwww..cc....xxxxxxxxx");
    strcpy(vgrid[34], "xxxxxxxxx....cc..wwww.c.wwwwwwwwwwcc||c..c$$ccwwwwwwwwww.c.wwww..cc....xxxxxxxxx");
    strcpy(vgrid[35], "xxxxxxxxx....cc..wwww.c.wwwwwwwwwwwcccc++ccccwwwwwwwwwww.c.wwww..cc....xxxxxxxxx");
    strcpy(vgrid[36], "xxxxxxxxx....cc..www..c..wwwwwwwwwwwwww..wwwwwwwwwwwwww..c..www..cc....xxxxxxxxx");
    strcpy(vgrid[37], "xxxxxxxxx....cc..www.cc...wwwwwwwwwwwwwwwwwwwwwwwwwwww...cc.www..cc....xxxxxxxxx");
    strcpy(vgrid[38], "xxxxxxxxx....cc..www.cc....wwwwwwwwwwwwwwwwwwwwwwwwwww...cc.www..cc....xxxxxxxxx");
    strcpy(vgrid[39], "xxxxxxxxx....cc..www.cc....wwwwwwwwwwwwwwwwwwwwwwwwww....cc.www..cc....xxxxxxxxx");
    strcpy(vgrid[40], "xxxxxxxxx....cc..www..cc....wwwwwwwwwwwwwwwwwwwwwwww....cc..www..cc....xxxxxxxxx");
    strcpy(vgrid[41], "xxxxxxxxx....cc...ww...cc.....wwwwwwwwwwwwwwwwwwww.....cc...ww...cc....xxxxxxxxx");
    strcpy(vgrid[42], "xxxxxxxxx....cc...ww....cc.......wwwwwwwwwwwwww.......cc....ww...cc....xxxxxxxxx");
    strcpy(vgrid[43], "xxxxxxxxxx....cc...ww....cc..........wwwwww..........cc....ww...cc....xxxxxxxxxx");
    strcpy(vgrid[44], "xxxxxxxxxxx....cc...ww....cc...T................T...cc....ww...cc....xxxxxxxxxxx");
    strcpy(vgrid[45], "xxxxxxxxxxxx....cc...ww....cc......................cc....ww...cc....xxxxxxxxxxxx");
    strcpy(vgrid[46], "xxxxxxxxxxxxx....cc...ww....ccccccccccc..ccccccccccc....ww...cc....xxxxxxxxxxxxx");
    strcpy(vgrid[47], "xxxxxxxxxxxxxx....cc...ww....cccccccccc2.cccccccccc....ww...cc....xxxxxxxxxxxxxx");
    strcpy(vgrid[48], "xxxxxxxxxxxxxxx....cc...ww............................ww...cc....xxxxxxxxxxxxxxx");
    strcpy(vgrid[49], "xxxxxxxxxxxxxxxx....cc...ww..........................ww...cc....xxxxxxxxxxxxxxxx");
    strcpy(vgrid[50], "xxxxxxxxxxxxxxxxx....cc...wwwwwwwwwwwww..wwwwwwwwwwwww...cc....xxxxxxxxxxxxxxxxx");
    strcpy(vgrid[51], "xxxxxxxxxxxxxxxxxx....cc...wwwwwwwwwwww..wwwwwwwwwwww...cc....xxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[52], "xxxxxxxxxxxxxxxxxxx....cc...wwwwwwwwwww..wwwwwwwwwww...cc....xxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[53], "xxxxxxxxxxxxxxxxxxxx....cc............................cc....xxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[54], "xxxxxxxxxxxxxxxxxxxxx....cc..........................cc....xxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[55], "xxxxxxxxxxxxxxxxxxxxxx....cccccccccccccccccccccccccccc....xxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[56], "xxxxxxxxxxxxxxxxxxxxxxx....cccccccccccccccccccccccccc....xxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[57], "xxxxxxxxxxxxxxxxxxxxxxxx................................xxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[58], "xxxxxxxxxxxxxxxxxxxxxxxxx..............................xxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[59], "xxxxxxxxxxxxxxxxxxxxxxxxxx............................xxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[60], "xxxxxxxxxxxxxxxxxxxxxxxxxxx..........{.(.[...........xxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[61], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    mons_array[0] = MONS_ANTAEUS;
    mons_array[1] = MONS_ICE_FIEND;
    mons_array[2] = MONS_ICE_DRAGON;
    mons_array[3] = RANDOM_MONSTER;
    mons_array[4] = RANDOM_MONSTER;
    mons_array[5] = RANDOM_MONSTER;
    mons_array[6] = RANDOM_MONSTER;

    return MAP_ENCOMPASS;
}


static char ereshkigal(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // Tartarus
      // note that the tomb on the right isn't supposed to have any doors

    strcpy(vgrid[0],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[7],  "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[8],  "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[9],  "xxxxxxxxx.................cccc..........ccc............................xxxxxxxxx");
    strcpy(vgrid[10], "xxxxxxxxx.............ccccc..cccc.....ccc.cccc.........................xxxxxxxxx");
    strcpy(vgrid[11], "xxxxxxxxx...........ccc.........ccccccc.....cc.........................xxxxxxxxx");
    strcpy(vgrid[12], "xxxxxxxxx.........ccc.......2............V..cc.........................xxxxxxxxx");
    strcpy(vgrid[13], "xxxxxxxxx........cc4........................cc...........xxxxxxxx......xxxxxxxxx");
    strcpy(vgrid[14], "xxxxxxxxx........cc44xxx==xxx...............cc..........xx......xx.....xxxxxxxxx");
    strcpy(vgrid[15], "xxxxxxxxx........ccxxx......xxx.......ccc++ccc.........xx........xx....xxxxxxxxx");
    strcpy(vgrid[16], "xxxxxxxxx........cxx..........xxx.....ccc44ccc.........x..........x....xxxxxxxxx");
    strcpy(vgrid[17], "xxxxxxxxx........cx............xx....cccc44cc.........xx..........xx...xxxxxxxxx");
    strcpy(vgrid[18], "xxxxxxxxx.......ccx.G........G.xxx7ccc..c44c..........x.....|......x...xxxxxxxxx");
    strcpy(vgrid[19], "xxxxxxxxx.......cxx............xxxcc..................x......7.....x...xxxxxxxxx");
    strcpy(vgrid[20], "xxxxxxxxx......ccx..............xxc...................xx..........xx...xxxxxxxxx");
    strcpy(vgrid[21], "xxxxxxxxx......ccx..G........G..xxc..x.........x.......x..........x....xxxxxxxxx");
    strcpy(vgrid[22], "xxxxxxxxx......ccx..............xcc....................xx........xx....xxxxxxxxx");
    strcpy(vgrid[23], "xxxxxxxxx.......cxx............xxc......................xx......xx.....xxxxxxxxx");
    strcpy(vgrid[24], "xxxxxxxxx.......ccx.F........F.xcc.......................xxxxxxxx......xxxxxxxxx");
    strcpy(vgrid[25], "xxxxxxxxx........cx............xc......................................xxxxxxxxx");
    strcpy(vgrid[26], "xxxxxxxxx........cxx....17....xxc....x.........x.......................xxxxxxxxx");
    strcpy(vgrid[27], "xxxxxxxxx........ccxxx......xxxcc......................................xxxxxxxxx");
    strcpy(vgrid[28], "xxxxxxxxx........cccc=xxxxxx=cccc......................................xxxxxxxxx");
    strcpy(vgrid[29], "xxxxxxxxx........cc||cccccccc||cc......................................xxxxxxxxx");
    strcpy(vgrid[30], "xxxxxxxxx.........cc||||O|||||cc.......................................xxxxxxxxx");
    strcpy(vgrid[31], "xxxxxxxxx..........cccccccccccc......x.........x............V..........xxxxxxxxx");
    strcpy(vgrid[32], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[33], "xxxxxxxxx...........................................xxxxxxxxxxxxxxxx...xxxxxxxxx");
    strcpy(vgrid[34], "xxxxxxxxx...........................................xxxxxxxxxxxxxxxx...xxxxxxxxx");
    strcpy(vgrid[35], "xxxxxxxxx...........................................xx$$$$xxx|||||xx...xxxxxxxxx");
    strcpy(vgrid[36], "xxxxxxxxx.......V........V...........x.........x....xx$$$$xxx|||||xx...xxxxxxxxx");
    strcpy(vgrid[37], "xxxxxxxxx...........................................xxxxxxxxxxxxxxxx...xxxxxxxxx");
    strcpy(vgrid[38], "xxxxxxxxx...........................................xxxxxxxxxxxxxxxx...xxxxxxxxx");
    strcpy(vgrid[39], "xxxxxxxxx...........................................xx44444xx22222xx...xxxxxxxxx");
    strcpy(vgrid[40], "xxxxxxxxx.......xxxxxxxxx+xxxxxxxxx.................xx44444xx22222xx...xxxxxxxxx");
    strcpy(vgrid[41], "xxxxxxxxx.......x3.2..........3...x..x.........x..xxxxxxxxxxxxxxxxxx...xxxxxxxxx");
    strcpy(vgrid[42], "xxxxxxxxx.......x.x.x.x.x.x.x.x.x.x.................xxxxxxxxxxxxxxxx...xxxxxxxxx");
    strcpy(vgrid[43], "xxxxxxxxx.......x...2.3..4..5..4..x......................=.......xxx...xxxxxxxxx");
    strcpy(vgrid[44], "xxxxxxxxx.......xx.x.x.x.x.x.x.x.xx......................=.......xxx...xxxxxxxxx");
    strcpy(vgrid[45], "xxxxxxxxx.......x..65..3..6.6...5.x.................xxxxxxxxxxxxxxxx...xxxxxxxxx");
    strcpy(vgrid[46], "xxxxxxxxx.......x.x.x.x.x.x.x.x.x.x..x.........x..xxxxxxxxxxxxxxxxxx...xxxxxxxxx");
    strcpy(vgrid[47], "xxxxxxxxx.......x...4...3.....4...x.................xx.....xx555555x...xxxxxxxxx");
    strcpy(vgrid[48], "xxxxxxxxx.......xx=xxxxx.x.xxxxxxxx.................xx.....xx555555x...xxxxxxxxx");
    strcpy(vgrid[49], "xxxxxxxxx.......x$$$$$$x.25.x$$$||x.................xxxxxxxxxxxxxxxx...xxxxxxxxx");
    strcpy(vgrid[50], "xxxxxxxxx.......x$x$$x$xx.x.x$x$x|x.................xxxxxxxxxxxxxxxx...xxxxxxxxx");
    strcpy(vgrid[51], "xxxxxxxxx.......x||||||x.556=$$$||x..x.........x....xx$$xx56565xx$|x...xxxxxxxxx");
    strcpy(vgrid[52], "xxxxxxxxx.......xxxxxxxxxxxxxxxxxxx.................xx$$xx65656xx|7x...xxxxxxxxx");
    strcpy(vgrid[53], "xxxxxxxxx...........................................xxxxxxxxxxxxxxxx...xxxxxxxxx");
    strcpy(vgrid[54], "xxxxxxxxx...........................................xxxxxxxxxxxxxxxx...xxxxxxxxx");
    strcpy(vgrid[55], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[56], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[57], "xxxxxxxxx........(...........................................[.........xxxxxxxxx");
    strcpy(vgrid[58], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[59], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[60], "xxxxxxxxx..............................{...............................xxxxxxxxx");
    strcpy(vgrid[61], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    mons_array[0] = MONS_ERESHKIGAL;
    mons_array[1] = MONS_NECROPHAGE;
    mons_array[2] = MONS_WRAITH;
    mons_array[3] = MONS_SHADOW;
    mons_array[4] = MONS_ZOMBIE_SMALL;
    mons_array[5] = MONS_SKELETON_SMALL;
    mons_array[6] = MONS_SHADOW_FIEND;

    return MAP_ENCOMPASS;
}


static char mnoleg(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{

    for (unsigned char i = 0; i < 81; i++)
        strcpy(vgrid[i], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    strcat(vgrid[0],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[1],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[2],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[3],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[4],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[5],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[6],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[7],  "x.................2............xxxxxxxxx");
    strcat(vgrid[8],  "x.....2........................xxxxxxxxx");
    strcat(vgrid[9],  "x..cccccccc...ccccccc..ccccccc.xxxxxxxxx");
    strcat(vgrid[10], "x..ccccccccc.2.ccccccc..cccccc.xxxxxxxxx");
    strcat(vgrid[11], "x..cccccccccc...ccccccc..ccccc.xxxxxxxxx");
    strcat(vgrid[12], "x..ccccccccccc.1.ccccccc..cccc.xxxxxxxxx");
    strcat(vgrid[13], "x2.cccccccccc.2..Occccccc2.ccc.xxxxxxxxx");
    strcat(vgrid[14], "x..ccccccccc.....ccccccccc..cc.xxxxxxxxx");
    strcat(vgrid[15], "x..cccccccc...c...ccccccccc..c.xxxxxxxxx");
    strcat(vgrid[16], "x..ccccccc...ccc...ccccccccc...xxxxxxxxx");
    strcat(vgrid[17], "x..cccccc...ccccc...ccccccccc..xxxxxxxxx");
    strcat(vgrid[18], "x..ccccc...ccccccc...ccccccccc.xxxxxxxxx");
    strcat(vgrid[19], "x..cccc...ccccccccc...ccccccc..xxxxxxxxx");
    strcat(vgrid[20], "x..ccc.2.ccccccccccc.2.ccccc...xxxxxxxxx");
    strcat(vgrid[21], "x..cc.....ccccccccccc...ccc....xxxxxxxxx");
    strcat(vgrid[22], "x..c...c...ccccccccccc...c.2...xxxxxxxxx");
    strcat(vgrid[23], "x.....ccc.2.ccccccccccc......c.xxxxxxxxx");
    strcat(vgrid[24], "x....ccccc...ccccccccccc....cc.xxxxxxxxx");
    strcat(vgrid[25], "x.2.ccccccc...ccccccccccc..ccc.xxxxxxxxx");
    strcat(vgrid[26], "x.................2.......cccc.xxxxxxxxx");
    strcat(vgrid[27], "x...c..ccccccc.ccccccc...ccccc.xxxxxxxxx");
    strcat(vgrid[28], "x..ccc......2c.c2cccc...cccccc.xxxxxxxxx");
    strcat(vgrid[29], "x.ccccc..ccc.c.c2ccc.2.ccccccc.xxxxxxxxx");
    strcat(vgrid[30], "x.cccccc..cc.c.c.cc...cccccccc.xxxxxxxxx");
    strcat(vgrid[31], "x.ccccccc..c.c.c.c...ccccccccc.xxxxxxxxx");
    strcat(vgrid[32], "x.cccccccc...c.c....cccccccccc.xxxxxxxxx");
    strcat(vgrid[33], "x.ccccccccc..c.c...ccccccccccc.xxxxxxxxx");
    strcat(vgrid[34], "x..............................xxxxxxxxx");
    strcat(vgrid[35], "xxxxxxxxxxxxxx@xxxxxxxxxxxxxxxxxxxxxxxxx");

    mons_array[0] = MONS_MNOLEG;
    mons_array[1] = MONS_NEQOXEC;
    mons_array[2] = RANDOM_MONSTER;
    mons_array[3] = RANDOM_MONSTER;
    mons_array[4] = RANDOM_MONSTER;
    mons_array[5] = RANDOM_MONSTER;
    mons_array[6] = RANDOM_MONSTER;

    return MAP_NORTHEAST;
}


static char lom_lobon(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{

    strcpy(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[7], "xxxxxxxxxxxxxxxxxxxxxxxxxxxwwwwwwwwwwww.......wwwwwwwxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[8], "xxxxxxxxxxxxxxxxxxxxxxwwwwwwwwwwwwbbbwwwwwww.......wwwwwwwxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[9], "xxxxxxxxxxxxxxxxxxwwwwwwwwwwwwbbbbbbbbbbbwwwwww.........wwwwwwxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[10], "xxxxxxxxxxxxxxxwwwwwwwwwwwwbbbbwwwwwwwwwbbbbwwwwww.........wwwwwwxxxxxxxxxxxxxxx");
    strcpy(vgrid[11], "xxxxxxxxxxxxxwwwwwwwbbbbbbbbwwwwwwwwwwwwwwwbbbwwwww...........wwwwwxxxxxxxxxxxxx");
    strcpy(vgrid[12], "xxxxxxxxxxxxwwwwwbbbb......bbbwwwwwwwwwwww...bbwwwww.............wwwxxxxxxxxxxxx");
    strcpy(vgrid[13], "xxxxxxxxxxxxwwwbbb...........bbbwwwwww........bbwwwww.............wwxxxxxxxxxxxx");
    strcpy(vgrid[14], "xxxxxxxxxxxwwwbb...............bbwwww..........bwwwwww.............wwxxxxxxxxxxx");
    strcpy(vgrid[15], "xxxxxxxxxxxwwbb........1O.......bbww...........bbwwww..............wwxxxxxxxxxxx");
    strcpy(vgrid[16], "xxxxxxxxxxwwwb...................bw......2......bwww.....U....2.....wwxxxxxxxxxx");
    strcpy(vgrid[17], "xxxxxxxxxxwwbb...................bb.............bww.................wwxxxxxxxxxx");
    strcpy(vgrid[18], "xxxxxxxxxxwwbb..3................bbb............bbw..............4..wwxxxxxxxxxx");
    strcpy(vgrid[19], "xxxxxxxxxwwbbb...................b.b............4....................wwxxxxxxxxx");
    strcpy(vgrid[20], "xxxxxxxxxwwbwbb.................bb.......U......4..........U..........wxxxxxxxxx");
    strcpy(vgrid[21], "xxxxxxxxxwwbwwbb...............bb..b............bbw..............4.....xxxxxxxxx");
    strcpy(vgrid[22], "xxxxxxxxxwwbbwwbbb...........bbb..bb............bwww...................xxxxxxxxx");
    strcpy(vgrid[23], "xxxxxxxxxwwwbwwwwb..b..2..bbbb....b.............bwww...................xxxxxxxxx");
    strcpy(vgrid[24], "xxxxxxxxxxwwbwwww...bbbbbbb.......bw.....3.....bbwwww...U.....3.......xxxxxxxxxx");
    strcpy(vgrid[25], "xxxxxxxxxxwwbbww.................bbww........wwbwwwww.................xxxxxxxxxx");
    strcpy(vgrid[26], "xxxxxxxxxxwwwbbw................bbwwwww....wwwbbwwww..................xxxxxxxxxx");
    strcpy(vgrid[27], "xxxxxxxxxxwwwwbb...4...U........bwwwwwwwwwwwwbbwww....................xxxxxxxxxx");
    strcpy(vgrid[28], "xxxxxxxxxxxwwwwbbb...........bbbbbwwwwwwwwwbbbwww....................xxxxxxxxxxx");
    strcpy(vgrid[29], "xxxxxxxxxxxwwwwwwbbbb.....bbbbwwwbbbbwwwbbbbwwww....................xxxxxxxxxxxx");
    strcpy(vgrid[30], "xxxxxxxxxxxwwwwwwwwwbbbbbbbwwwwwwwwwbbbbbwwwww......4.....4........xxxxxxxxxxxxx");
    strcpy(vgrid[31], "xxxxxxxxxxxxwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww......................xxxxxxxxxxxxxx");
    strcpy(vgrid[32], "xxxxxxxxxxxxwwwwwwwwwwwwwwwwwwwwwwwwwwwww.......................xxxxxxxxxxxxxxxx");
    strcpy(vgrid[33], "xxxxxxxxxxxxxxwwwwwwwwwwwwwwwwwwwww........................xxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[34], "xxxxxxxxxxxxxxxxxxxxxxxxxwwwwwww......................xxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[35], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx...@.xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    mons_array[0] = MONS_LOM_LOBON;
    mons_array[1] = MONS_GIANT_ORANGE_BRAIN;
    mons_array[2] = MONS_RAKSHASA;
    mons_array[3] = MONS_WIZARD;
    mons_array[4] = RANDOM_MONSTER;
    mons_array[5] = RANDOM_MONSTER;
    mons_array[6] = RANDOM_MONSTER;

    return MAP_NORTH;
}


static char cerebov(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // you might not want to teleport too much on this level -
      // unless you can reliably teleport away again.

    for (unsigned char i = 0; i < 81; i++)
        strcpy(vgrid[i], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    strcat(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[6], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[7], "...............................xxxxxxxxx");
    strcat(vgrid[8], ".............vvvvv.............xxxxxxxxx");
    strcat(vgrid[9], ".............v$$$v.............xxxxxxxxx");
    strcat(vgrid[10], ".............v|||v.............xxxxxxxxx");
    strcat(vgrid[11], ".............v$$$v.............xxxxxxxxx");
    strcat(vgrid[12], ".vvvvv...vvvvvvvvvvvvv...vvvvv.xxxxxxxxx");
    strcat(vgrid[13], ".v|$|vvvvv...........vvvvv$|$v.xxxxxxxxx");
    strcat(vgrid[14], ".v$|$v.....vvvvvvvvv.....v|$|v.xxxxxxxxx");
    strcat(vgrid[15], ".v|$|v.vvvvvvvvOvvvvvvvv.v$|$v.xxxxxxxxx");
    strcat(vgrid[16], ".vvvvv.vvvvvv..3..vvvvvv.vvvvv.xxxxxxxxx");
    strcat(vgrid[17], "...v...vv.....vvv.....vv...v...xxxxxxxxx");
    strcat(vgrid[18], "...v.vvvv....vv1vv....vvvv.v...xxxxxxxxx");
    strcat(vgrid[19], "...v.vv......v...v......vv.v...xxxxxxxxx");
    strcat(vgrid[20], "...v.vvvv.............vvvv.v...xxxxxxxxx");
    strcat(vgrid[21], "...v...vv..2.......2..vv...v...xxxxxxxxx");
    strcat(vgrid[22], ".vvvvv.vv..2.......2..vv.vvvvv.xxxxxxxxx");
    strcat(vgrid[23], ".v|$|v.vv.............vv.v$|$v.xxxxxxxxx");
    strcat(vgrid[24], ".v|$|v.vv...vv...vv...vv.v$|$v.xxxxxxxxx");
    strcat(vgrid[25], ".v|$|v.vv...vv+++vv...vv.v$|$v.xxxxxxxxx");
    strcat(vgrid[26], ".vvvvv.vvvvvvv...vvvvvvv.vvvvv.xxxxxxxxx");
    strcat(vgrid[27], "....v..vvvvvvv...vvvvvvv..v....xxxxxxxxx");
    strcat(vgrid[28], "....vv...................vv....xxxxxxxxx");
    strcat(vgrid[29], ".....vv.vvvvv..2..vvvvv.vv.....xxxxxxxxx");
    strcat(vgrid[30], "......vvv|||v.....v$$$vvv......xxxxxxxxx");
    strcat(vgrid[31], "........v|$|vv...vv$|$v........xxxxxxxxx");
    strcat(vgrid[32], "........v|||v.....v$$$v........xxxxxxxxx");
    strcat(vgrid[33], "........vvvvv.....vvvvv........xxxxxxxxx");
    strcat(vgrid[34], "...............................xxxxxxxxx");
    strcat(vgrid[35], "...............@...............xxxxxxxxx");

    mons_array[0] = MONS_CEREBOV;
    mons_array[1] = MONS_BALRUG;
    mons_array[2] = MONS_PIT_FIEND;
    mons_array[3] = RANDOM_MONSTER;
    mons_array[4] = RANDOM_MONSTER;
    mons_array[5] = RANDOM_MONSTER;
    mons_array[6] = RANDOM_MONSTER;

    return MAP_NORTHEAST;
}


static char gloorx_vloq(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{

    for (unsigned char i = 0; i < 81; i++)
        strcpy(vgrid[i], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    strcpy(vgrid[36], "xxxxxxxxxxxxxxxxxxxxxxx@.xxxxxxxxxxxxxxx");
    strcpy(vgrid[37], "xxxxxxxxx..............................x");
    strcpy(vgrid[38], "xxxxxxxxx..............................x");
    strcpy(vgrid[39], "xxxxxxxxx..............................x");
    strcpy(vgrid[40], "xxxxxxxxx.x.x.x.x.x.x.x..x.x.x.x.x.x.x.x");
    strcpy(vgrid[41], "xxxxxxxxx..............................x");
    strcpy(vgrid[42], "xxxxxxxxx.x.xxxx=xxxxxxxxxxxx=xxxxxx.x.x");
    strcpy(vgrid[43], "xxxxxxxxx...xx....................xx...x");
    strcpy(vgrid[44], "xxxxxxxxx.x.x..ccccc..4..4..ccccc..x.x.x");
    strcpy(vgrid[45], "xxxxxxxxx...x.cc.3............3.cc.x...x");
    strcpy(vgrid[46], "xxxxxxxxx.x.x.c..ccccc.cc.ccccc..c.x.x.x");
    strcpy(vgrid[47], "xxxxxxxxx...x.c.cc.....cc.....cc.c.x...x");
    strcpy(vgrid[48], "xxxxxxxxx.x.x.c.c.2...cccc...2.c.c.x.x.x");
    strcpy(vgrid[49], "xxxxxxxxx...x...c...ccc..ccc...c...=...x");
    strcpy(vgrid[50], "xxxxxxxxx.x.x.3.....2..1O..2.....3.x.x.x");
    strcpy(vgrid[51], "xxxxxxxxx...=...c...ccc..ccc...c...x...x");
    strcpy(vgrid[52], "xxxxxxxxx.x.x.c.c.2...cccc...2.c.c.x.x.x");
    strcpy(vgrid[53], "xxxxxxxxx...x.c.cc.....cc.....cc.c.x...x");
    strcpy(vgrid[54], "xxxxxxxxx.x.x.c..ccccc.cc.ccccc..c.x.x.x");
    strcpy(vgrid[55], "xxxxxxxxx...x.cc.3............3.cc.x...x");
    strcpy(vgrid[56], "xxxxxxxxx.x.x..ccccc..4..4..ccccc..=.x.x");
    strcpy(vgrid[57], "xxxxxxxxx...xx....................xx...x");
    strcpy(vgrid[58], "xxxxxxxxx.x.xxxx=xxxx=xxxxxxxx=xxxxx.x.x");
    strcpy(vgrid[59], "xxxxxxxxx..............................x");
    strcpy(vgrid[60], "xxxxxxxxx.x.x.x.x.x.x.x..x.x.x.x.x.x.x.x");
    strcpy(vgrid[61], "xxxxxxxxx..............................x");
    strcpy(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    mons_array[0] = MONS_GLOORX_VLOQ;
    mons_array[1] = MONS_EXECUTIONER;
    mons_array[2] = MONS_DEMONIC_CRAWLER;
    mons_array[3] = MONS_SHADOW_DEMON;
    mons_array[4] = RANDOM_MONSTER;
    mons_array[5] = RANDOM_MONSTER;
    mons_array[6] = RANDOM_MONSTER;

    return MAP_SOUTHWEST;
}


static char beehive(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{

    strcpy(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[7], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[8], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[9], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxaaaaaaaaaxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[10], "xxxxxxxxxxxxxxxxxxxxxxaaaaaaaaaaaRaaaaaaaaaxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[11], "xxxxxxxxxxxxxxxxxaaaaaaaaaaRa2aaR1RaaRa2aaaaaaxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[12], "xxxxxxxxxxxxxxxxxxaaaaaaaaaaRa2a3R3aRaRaRaaaaaaaaaxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[13], "xxxxxxxxxxxxxxxxxxxxxaaaaRaRaRaaa3aaa3aRa.a.aaaaaaaaaaaxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[14], "xxxxxxxxxxxxxxxxaaaaaaRa.aRa2a2a2a2aRaRa.a.a3aaaaaaaaaaaaaaxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[15], "xxxxxxxxxxxxx4aaaaaaaaa.aaRaRaa2aa2aaRaaa.aa3a33aaaaaaaaaa.44xxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[16], "xxxxxxxxxxx.4aaaaaaa.222a3a.aaaRaaa.aaa.R3aa3a3aaaaaaaa.....4xxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[17], "xxxxxxxxxx....aaaaaaa.aRa.a3aRaRa.a3a.a.a.a.aRa2aaaaaa....xxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[18], "xxxxxxxxxxx...aaaaaa3a3a.a.a.a3aRa2aRa3a.a.aRaRa.aaaaa...xxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[19], "xxxxxxxxxxxx...aa2aRa3a3a3aRa.a3a.a.a.a.a.a.a.a3a.aaa...xxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[20], "xxxxxxxxxxxx...aaa.a.a.a2a.aaa.aRaRa2a.a2a3a.a2aaaa..T..xxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[21], "xxxxxxxxxxx.....a2a.a2a.aRaaaaa3a.a.aaa3a3a3a3a.a.........xxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[22], "xxxxxxxxxxx.4...aaRRaa.a2a.a3a3a3a.aaa.a.aRa.a.aa..4.......xxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[23], "xxxxxxxxxx......a.a.aaa.a3a.a.a.a.aaa2a.a2a.a.aRaa.....4...xxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[24], "xxxxxxxxxx.....aa3a2aaa.a.a.a3a3a3a3aRaaa.a2a.a2aa........xxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[25], "xxxxxxxxxxx...aaaa.a2aRa.a.a2aaa.a.a.a.aaa.a.aaaa.....xxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[26], "xxxxxxxxxxxx..aaa.a.a.a.a.a.a.aaa2a.a3a2a.a2aaa.....xxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[27], "xxxxxxxxxxxxxx.aaaa3a.a2aRa.a.aaaRa.a.aa.a.aaa....xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[28], "xxxxxxxxxxxxxxx...aaaaRa.a3a3a.a.a.aaa.aa.aa....4xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[29], "xxxxxxxxxxxxx........aa.a2a.a.aaa2aa.aa.aaa....xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[30], "xxxxxxxxxxxxx....4.....a.a2a2a.a2a.a2a.......4.xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[31], "xxxxxxxxxxx.............a.a.a.a.a.a.....4....xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[32], "xxxxxxxxxx..............4..a.a.a......4...xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[33], "xxxxxxxxxx.................a.a.........xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[34], "xxxxxxxxxxx........................xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[35], "xxxxxxxxxxx.....4...T............xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[36], "xxxxxxxxxxx.......................xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[37], "xxxxxxxxxxxx.........................xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[38], "xxxxxxxxxxxx.................T.........xxxxxxxx..xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[39], "xxxxxxxxxxxx.......4.....................xxxxxxx...xxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[40], "xxxxxxxxxxx..............xx...............xxxxxx....xxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[41], "xxxxxxxxxxx............xxxxx........4......xxxx..4....xxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[42], "xxxxxxxxxxx..T..........xxx................xxxxx...T.xxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[43], "xxxxxxxxxxx............xxx........T.........xxx........xxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[44], "xxxxxxxxxxx....4........xx....................x..........xxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[45], "xxxxxxxxxx...............x.x...xxx...............xx.xxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[46], "xxxxxxxxxx.........4...........xxx..................xxxxxxxxxxxxxxxxxxaaaaaxxxxx");
    strcpy(vgrid[47], "xxxxxxxxx.....4.....................4......4...........4...xxxxxxxxxxaa5a5aaxxxx");
    strcpy(vgrid[48], "xxxxxxxxx.................................................wwwwwwwwxxxa5*|*5axxxx");
    strcpy(vgrid[49], "xxxxxxxxx............x...x...T.....xxxx.................wwwwwwwwwwwwxaa*|*aaxxxx");
    strcpy(vgrid[50], "xxxxxxxxxx.........xx.............xxxxx................wwwwwwwwwwwwwwxaa5aaxxxxx");
    strcpy(vgrid[51], "xxxxxxxxxxx.......x..................xxx....4..........wwwwwwwwwwwwwwwxa5axxxxxx");
    strcpy(vgrid[52], "xxxxxxxxxxx.....xxx...4...........................xxxx.4wwwwwwwwwwwwwwwa=axxxxxx");
    strcpy(vgrid[53], "xxxxxxxxxxxx..xxx.............xx....(.........xxxxxxxx....wwwwwwwwwwwwwwaaxxxxxx");
    strcpy(vgrid[54], "xxxxxxxxxxxxxxxx.............xxxx..................xxxx......wwwwwwwwwwxxxxxxxxx");
    strcpy(vgrid[55], "xxxxxxxxxxxxxxxxx....{..}..xxxxxx..]......xxx...........4.wwwwwwwwwwwwxxxxxxxxxx");
    strcpy(vgrid[56], "xxxxxxxxxxxxxxxxxxxx........xxx........xxxxxx....4....wwwwwwwwwwwwwwxxxxxxxxxxxx");
    strcpy(vgrid[57], "xxxxxxxxxxxxxxxxxxxxxxxxx..[.xxx........xxx)....wwwwwwwwwwwwwwwwwwxxxxxxxxxxxxxx");
    strcpy(vgrid[58], "xxxxxxxxxxxxxxxxxxxxxxxxxxxx.........xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[59], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[60], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[61], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    mons_array[0] = MONS_QUEEN_BEE;
    mons_array[1] = MONS_KILLER_BEE;
    mons_array[2] = MONS_KILLER_BEE_LARVA;
    mons_array[3] = MONS_PLANT;
    mons_array[4] = MONS_YELLOW_WASP;
    mons_array[5] = RANDOM_MONSTER;
    mons_array[6] = RANDOM_MONSTER;

    return MAP_ENCOMPASS;
}


static char vaults_vault(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // last level of the vaults -- dungeon.cc will change all these 'x's

    strcpy(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[7], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[8], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[9], "xxxxxxxxx..xxxxxxxxxxxxxxxxxxxxxxxxxxx....xxxxxxxxxxxxxxxxxxxxxxxxxxx..xxxxxxxxx");
    strcpy(vgrid[10], "xxxxxxxxx..x.........................x....xxxxxxxxxxxxxxxxxxxxxxxxxxx..xxxxxxxxx");
    strcpy(vgrid[11], "xxxxxxxxx..x.xxxxxxxxxxx..xxxxxxxxxx.x....xxxxx.................xxxxx..xxxxxxxxx");
    strcpy(vgrid[12], "xxxxxxxxx..x.x*.*.*.*.*x..x........x.x....xxx..........8..........xxx..xxxxxxxxx");
    strcpy(vgrid[13], "xxxxxxxxx..x.x.*.*.*.*.x..x........x.x....xxx.....................xxx..xxxxxxxxx");
    strcpy(vgrid[14], "xxxxxxxxx..x.x*.*.*.*.*x..x...||...x.x....xx......9........9.......xx..xxxxxxxxx");
    strcpy(vgrid[15], "xxxxxxxxx..x.x.*.*.*.*.x..x...||...x.x....xx.......................xx..xxxxxxxxx");
    strcpy(vgrid[16], "xxxxxxxxx..x.x*.*.*.*.*x..x...||...x.x....xx......xxxxxxxxxxx......xx..xxxxxxxxx");
    strcpy(vgrid[17], "xxxxxxxxx..x.x.*.*.*.*.x..x........x.x....xx......x.........x......xx..xxxxxxxxx");
    strcpy(vgrid[18], "xxxxxxxxx..x.x*.*.*.*.*+..+........x.x....xx....xxx..........xx....xx..xxxxxxxxx");
    strcpy(vgrid[19], "xxxxxxxxx..x.x.*.*.*.*.xxxxxxxxxxxxx.x....xx.9..x....xxxxx....x..8.xx..xxxxxxxxx");
    strcpy(vgrid[20], "xxxxxxxxx..x.xxxxxxxxxxx9998.........x....xx....x...xx$$$xx...x....xx..xxxxxxxxx");
    strcpy(vgrid[21], "xxxxxxxxx..x...........899xxxxxxxxxx.x....xx....x..xx$***$xx..x....xx..xxxxxxxxx");
    strcpy(vgrid[22], "xxxxxxxxx..x.xxxxxxxxxxx99x........x.x....xx....x..x$$*|*$$x..x....xx..xxxxxxxxx");
    strcpy(vgrid[23], "xxxxxxxxx..x.x.....|...x88x.$$$$$$.x.x....xx..8.x..xx$***$xx..x....xx..xxxxxxxxx");
    strcpy(vgrid[24], "xxxxxxxxx..x.x.|..|..|.x..x.$$$$$$.x.x....xx....x....x$$$xx...x..9.xx..xxxxxxxxx");
    strcpy(vgrid[25], "xxxxxxxxx..x.x.........x..x.$$$$$$.x.x....xx....xxx..xxxxx..xxx....xx..xxxxxxxxx");
    strcpy(vgrid[26], "xxxxxxxxx..x.x.|..|..|.x..x.$$$$$$.x.x....xx......x.........x......xx..xxxxxxxxx");
    strcpy(vgrid[27], "xxxxxxxxx..x.x.........x..x.$$$$$$.x.x....xx......xxxxxxxxxxx......xx..xxxxxxxxx");
    strcpy(vgrid[28], "xxxxxxxxx..x.x|..|..|..x..x.$$$$$$.x.x....xxx.....................xxx..xxxxxxxxx");
    strcpy(vgrid[29], "xxxxxxxxx..x.x.........x..+........x.x....xxx......9.........9....xxx..xxxxxxxxx");
    strcpy(vgrid[30], "xxxxxxxxx..x.xxxxxxxxx+x..xxxxxxxxxx.xx.11....xx................xxxxx..xxxxxxxxx");
    strcpy(vgrid[31], "xxxxxxxxx..x...........x..x...........x1111...xxxxxxxxxxxxxxxxxxxxxxx..xxxxxxxxx");
    strcpy(vgrid[32], "xxxxxxxxx..xxxxxxxxxxxxxxxxxxxxxxxxx..1....1..xxxxxxxxxxxxxxxxxxxxxxx..xxxxxxxxx");
    strcpy(vgrid[33], "xxxxxxxxx..........................xx1..(}..1..........................xxxxxxxxx");
    strcpy(vgrid[34], "xxxxxxxxx...........................11.[..{.11.........................xxxxxxxxx");
    strcpy(vgrid[35], "xxxxxxxxx............................1..])..1..........................xxxxxxxxx");
    strcpy(vgrid[36], "xxxxxxxxx.............................1....1...........................xxxxxxxxx");
    strcpy(vgrid[37], "xxxxxxxxx..xxxxxxxxxxxxxxxxxxxxxxx.....1111..x.xxx.xxxxxxxxxxxxxxxxxx..xxxxxxxxx");
    strcpy(vgrid[38], "xxxxxxxxx..xx.x.x.x.x.x.x.x.x.x.x.x.....11..........................x..xxxxxxxxx");
    strcpy(vgrid[39], "xxxxxxxxx..x.x.x.x.x.x.x.x|x.x.x.x.x................................x..xxxxxxxxx");
    strcpy(vgrid[40], "xxxxxxxxx..xx.x|x.x.x.x.x.x.x.x.x.x.x.....x.........................x..xxxxxxxxx");
    strcpy(vgrid[41], "xxxxxxxxx..x.x.x.x.x.x.x.x9x.x.x.x.x.x..........8..........9........x..xxxxxxxxx");
    strcpy(vgrid[42], "xxxxxxxxx..xx.x.x.x.x.x.x.x.x.x.x.x.xx....x..9......................x..xxxxxxxxx");
    strcpy(vgrid[43], "xxxxxxxxx..x.x.x.x.x.x.x.x.x.x|x.x.x.x....x.........................x..xxxxxxxxx");
    strcpy(vgrid[44], "xxxxxxxxx..xx.x8x.x.x|x.x.x.x.x.x.x.xx....x..............9...9......x..xxxxxxxxx");
    strcpy(vgrid[45], "xxxxxxxxx..x.x.x.x.x9x.x.x.x.x.x.x.x.x...........8..................x..xxxxxxxxx");
    strcpy(vgrid[46], "xxxxxxxxx..xx.x.x.x.x.x.x.x.x.x.x.x.xx....x..9......................x..xxxxxxxxx");
    strcpy(vgrid[47], "xxxxxxxxx..x.x.x.x.x.x.x.x.x|x.x9x.x.x....x.........................x..xxxxxxxxx");
    strcpy(vgrid[48], "xxxxxxxxx..xx.x.x.x.x.x.x.x.x.x.x.x.xx....x...................9.....x..xxxxxxxxx");
    strcpy(vgrid[49], "xxxxxxxxx..x.x.x9x.x.x.x.x.x.x.x.x.x.x....x....9......8.............x..xxxxxxxxx");
    strcpy(vgrid[50], "xxxxxxxxx..xx.x.x.x.x.x.x9x.x.x.x.x.xx....x.........................x..xxxxxxxxx");
    strcpy(vgrid[51], "xxxxxxxxx..x.x.x.x.x.x.x.x.x.x.x.x.x.x....x.........................x..xxxxxxxxx");
    strcpy(vgrid[52], "xxxxxxxxx..xx.x.x.x.x.x.x.x.x.x.x.x.xx....x.......9......9..........x..xxxxxxxxx");
    strcpy(vgrid[53], "xxxxxxxxx..x.x.x.x.x.x.x.x.x.x8x.x.x.x....x.....................8...x..xxxxxxxxx");
    strcpy(vgrid[54], "xxxxxxxxx..xx.x8x.x.x.x.x.x.x.x.x.x.xx....x.....................||.|x..xxxxxxxxx");
    strcpy(vgrid[55], "xxxxxxxxx..x.x|x.x.x.x.x.x.x|x.x.x.x.x....x.....................|...x..xxxxxxxxx");
    strcpy(vgrid[56], "xxxxxxxxx..xx.x.x.x.x.x.x8x.x.x.x.x.xx....x......8..................x..xxxxxxxxx");
    strcpy(vgrid[57], "xxxxxxxxx..x.x.x.x.x.x.x.x.x.x.x.x.x.x....x..........8..8...8.....||x..xxxxxxxxx");
    strcpy(vgrid[58], "xxxxxxxxx..xO.x.x.x.x.x.x.x.x.x.x|x.xx....x.....................|.||x..xxxxxxxxx");
    strcpy(vgrid[59], "xxxxxxxxx..xxxxxxxxxxxxxxxxxxxxxxxxxxx....xxxxxxxxxxxxxxxxxxxxxxxxxxx..xxxxxxxxx");
    strcpy(vgrid[60], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[61], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    mons_array[0] = MONS_VAULT_GUARD;
    mons_array[1] = RANDOM_MONSTER;
    mons_array[2] = RANDOM_MONSTER;
    mons_array[3] = RANDOM_MONSTER;
    mons_array[4] = RANDOM_MONSTER;
    mons_array[5] = RANDOM_MONSTER;
    mons_array[6] = RANDOM_MONSTER;

    return MAP_ENCOMPASS;
}


static char snake_pit(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // Hey, this looks a bit like a face ...

    for (unsigned char i = 0; i < 81; i++)
        strcpy(vgrid[i], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    strcpy(vgrid[36], "xxxxxxxxxxxxxxxxxxxxxxx..@.xxxxxxxxxxxxx");
    strcpy(vgrid[37], "xxxxxxxxxxxxxxxxxxx.............xxxxxxxx");
    strcpy(vgrid[38], "xxxxxxxxxxxxxx....x.............x..xxxxx");
    strcpy(vgrid[39], "xxxxxxxxxxxx....2.x.............x.2..xxx");
    strcpy(vgrid[40], "xxxxxxxxxxx.....2.x....x.....x..x..3.xxx");
    strcpy(vgrid[41], "xxxxxxxxxxx.....22x.............xx.2..xx");
    strcpy(vgrid[42], "xxxxxxxxxxx.......xx..x........xx..3..xx");
    strcpy(vgrid[43], "xxxxxxxxxx.....x23.xx....T...xxx.44...xx");
    strcpy(vgrid[44], "xxxxxxxxxx......4.4.x.........x.333....x");
    strcpy(vgrid[45], "xxxxxxxxxx......3.x4...x.......4x4.....x");
    strcpy(vgrid[46], "xxxxxxxxxx.......3.......x.............x");
    strcpy(vgrid[47], "xxxxxxxxxx..c......3.........x.......c.x");
    strcpy(vgrid[48], "xxxxxxxxx...cc...................3..cc.x");
    strcpy(vgrid[49], "xxxxxxxxx...cc..........4.4.........cc.x");
    strcpy(vgrid[50], "xxxxxxxxx...cc...3...x........2.....cc.x");
    strcpy(vgrid[51], "xxxxxxxxx...cc.........1...1.......cc..x");
    strcpy(vgrid[52], "xxxxxxxxxx..cc.....1.....1.....1..ccc.xx");
    strcpy(vgrid[53], "xxxxxxxxxx...ccc..................cc..xx");
    strcpy(vgrid[54], "xxxxxxxxxx....cccc....3333333.....cc..xx");
    strcpy(vgrid[55], "xxxxxxxxxx.....ccccccc...........cc...xx");
    strcpy(vgrid[56], "xxxxxxxxxx........cccccccO...ccccc....xx");
    strcpy(vgrid[57], "xxxxxxxxxxx........cccccccccccccc....xxx");
    strcpy(vgrid[58], "xxxxxxxxxxx.........cccccccccccc.....xxx");
    strcpy(vgrid[59], "xxxxxxxxxxxxx.......................xxxx");
    strcpy(vgrid[60], "xxxxxxxxxxxxxxxx..................xxxxxx");
    strcpy(vgrid[61], "xxxxxxxxxxxxxxxxxxxxx.......xxxxxxxxxxxx");
    strcpy(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    mons_array[0] = MONS_GREATER_NAGA;
    mons_array[1] = MONS_NAGA;
    mons_array[2] = MONS_NAGA_MAGE;
    mons_array[3] = MONS_NAGA_WARRIOR;
    mons_array[4] = RANDOM_MONSTER;
    mons_array[5] = RANDOM_MONSTER;
    mons_array[6] = RANDOM_MONSTER;

    return MAP_SOUTHWEST;
}


static char elf_hall(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{

    strcpy(vgrid[0],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[7],  "xxxxxxxxxxxxxxxxxxcccccccccccccccccxxxxx");
    strcpy(vgrid[8],  "xxxxxxxxxxxxxxxxxcc*|*|*|**|||||c$ccxxxx");
    strcpy(vgrid[9],  "xxxxxxxxxxxxxxxxcc*$*|*|*|*|||||c$$ccxxx");
    strcpy(vgrid[10], "xxxxxxxxxxxxxxxcc*$|*$***$$|||||c|$$ccxx");
    strcpy(vgrid[11], "xxxxxxxxxxxxxxcc*$*|**ccccccccccc$$$$ccx");
    strcpy(vgrid[12], "xxxxxxxxxxxxxxc*|*$*$ccc.....2..c+$|$$cx");
    strcpy(vgrid[13], "xxxxxxxxxxxxxxc$*$*ccc...........c$$$$cx");
    strcpy(vgrid[14], "xxxxxxxxxxxxxxc||**cc...5.......4cc$|$cx");
    strcpy(vgrid[15], "xxxxxxxxxxxxxxc*$$cc........3..ccccccccx");
    strcpy(vgrid[16], "xxxxxxxxxxxxxxc$+ccc.....2....cc.....5cx");
    strcpy(vgrid[17], "xxxxxxxxxxxxxxc$c....5.......cc.......cx");
    strcpy(vgrid[18], "xxxxxxxxxxxxxxccc......5....cc..2....ccx");
    strcpy(vgrid[19], "xxxxxxxxxxxxxxxxc..........cc.......ccxx");
    strcpy(vgrid[20], "xxxxxxxxxxxxxxxcc..1..U..........4..ccxx");
    strcpy(vgrid[21], "xxxxxxxxxxxxxxcc.....................ccx");
    strcpy(vgrid[22], "xxxxxxxxxxxxxxc...........3...........cx");
    strcpy(vgrid[23], "xxxxxxxxxxxxxxc.......2.......3.......cx");
    strcpy(vgrid[24], "xxxxxxxxxxxxxxc..2................2..5cx");
    strcpy(vgrid[25], "xxxxxxxxxxxxxxc......x.........x......cx");
    strcpy(vgrid[26], "xxxxxxxxxxxxxxc.....xx.........xx.....cx");
    strcpy(vgrid[27], "xxxxxxxxxxxxxxc2...xxx....1....xxx.4..cx");
    strcpy(vgrid[28], "xxxxxxxxxxxxxxc..xxxx...........xxxx..cx");
    strcpy(vgrid[29], "xxxxxxxxxxxxxxc.xxx.....cc.cc.....xxx.cx");
    strcpy(vgrid[30], "xxxxxxxxxxxxxxc.x.....cccc.cccc.....x.cx");
    strcpy(vgrid[31], "xxxxxxxxxxxxxxc.3...cccxxc.cxxccc.3...cx");
    strcpy(vgrid[32], "xxxxxxxxxxxxxxc...cccxxxxc.cxxxxccc...cx");
    strcpy(vgrid[33], "xxxxxxxxxxxxxxc.cccxxxxxxc.cxxxxxxccc.cx");
    strcpy(vgrid[34], "xxxxxxxxxxxxxxcccxxxxxxxxc.cxxxxxxxxcccx");
    strcpy(vgrid[35], "xxxxxxxxxxxxxxxxxxxxxxxxxx@xxxxxxxxxxxxx");

    mons_array[0] = MONS_DEEP_ELF_HIGH_PRIEST;
    mons_array[1] = MONS_DEEP_ELF_DEMONOLOGIST;
    mons_array[2] = MONS_DEEP_ELF_ANNIHILATOR;
    mons_array[3] = MONS_DEEP_ELF_SORCERER;
    mons_array[4] = MONS_DEEP_ELF_DEATH_MAGE;
    mons_array[5] = RANDOM_MONSTER;
    mons_array[6] = RANDOM_MONSTER;

    return MAP_NORTHWEST;
}


// Slime pit take is reduced pending an increase in difficulty 
// of this subdungeon. -- bwr
static char slime_pit(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{

    strcpy(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[7], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx....xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[8], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx..xxxx.........xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[9], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxx....................xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[10], "xxxxxxxxxxxxxxxxxxxxxxxxxxxx......................xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[11], "xxxxxxxxxxxxxxxxxxxxxxxxxx..........................x.xxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[12], "xxxxxxxxxxxxxxxxxxxxxxxxxx............................xxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[13], "xxxxxxxxxxxxxxxxxxxxxxxxx.............................xxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[14], "xxxxxxxxxxxxxxxxxxxxxxxx.................................xxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[15], "xxxxxxxxxxxxxxxxxxxxxxxx..................................xxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[16], "xxxxxxxxxxxxxxxxxxxxxx....(................................xxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[17], "xxxxxxxxxxxxxxxxxxxxxx......................................xxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[18], "xxxxxxxxxxxxxxxxxxx..........................................xxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[19], "xxxxxxxxxxxxxxxxxxx..........................................xxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[20], "xxxxxxxxxxxxxxxxx............................................xxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[21], "xxxxxxxxxxxxxxxxx............................................xxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[22], "xxxxxxxxxxxxxx.....................ccc..ccc............]......xxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[23], "xxxxxxxxxxxxxxx...................cccc2ccccc...................xxxxxxxxxxxxxxxxx");
    strcpy(vgrid[24], "xxxxxxxxxxxxxx...................cc*cc..cc*cc....................xxxxxxxxxxxxxxx");
    strcpy(vgrid[25], "xxxxxxxxxxxxxx..................cc***cc4c***cc..................xxxxxxxxxxxxxxxx");
    strcpy(vgrid[26], "xxxxxxxxxxxxx..................cc*|*cc..cc*|*cc..................xxxxxxxxxxxxxxx");
    strcpy(vgrid[27], "xxxxxxxxxxxxx.................cc*|P|*c4cc*|P|*cc.................xxxxxxxxxxxxxxx");
    strcpy(vgrid[28], "xxxxxxxxxxxxx.................cc**|*cc..cc*|**cc....................xxxxxxxxxxxx");
    strcpy(vgrid[29], "xxxxxxxxxxxx..................ccc**c|cc4c|c**ccc...................xxxxxxxxxxxxx");
    strcpy(vgrid[30], "xxxxxxxxxxxx..................cccccccc..cccccccc....................xxxxxxxxxxxx");
    strcpy(vgrid[31], "xxxxxxxxxxx...................c.4.c.4.1..4.c.4.c.....................xxxxxxxxxxx");
    strcpy(vgrid[32], "xxxxxxxxxxx...................2.c.4.c..3.c.4.c.2.....................xxxxxxxxxxx");
    strcpy(vgrid[33], "xxxxxxxxxxx..........)........cccccccc..cccccccc.....................xxxxxxxxxxx");
    strcpy(vgrid[34], "xxxxxxxxxxx...................ccc**c|cc4c|c**ccc.....................xxxxxxxxxxx");
    strcpy(vgrid[35], "xxxxxxxxxx....................cc**|*cc..cc*|**cc....................xxxxxxxxxxxx");
    strcpy(vgrid[36], "xxxxxxxxxx....................cc*|P|*c4cc*|P|*cc....................xxxxxxxxxxxx");
    strcpy(vgrid[37], "xxxxxxxxxx.....................cc*|*cc..cc*|*cc....................xxxxxxxxxxxxx");
    strcpy(vgrid[38], "xxxxxxxxxxx.....................cc***cc4c***cc.....................xxxxxxxxxxxxx");
    strcpy(vgrid[39], "xxxxxxxxxxxx.....................cc*cc..cc*cc......................xxxxxxxxxxxxx");
    strcpy(vgrid[40], "xxxxxxxxxxxxx.....................cccc2ccccc......................xxxxxxxxxxxxxx");
    strcpy(vgrid[41], "xxxxxxxxxxxxxx.....................ccc..ccc.......................xxxxxxxxxxxxxx");
    strcpy(vgrid[42], "xxxxxxxxxxxxxx...........................................[.........xxxxxxxxxxxxx");
    strcpy(vgrid[43], "xxxxxxxxxxxxx......................................................xxxxxxxxxxxxx");
    strcpy(vgrid[44], "xxxxxxxxxxxxx..............................................xxxxx...xxxxxxxxxxxxx");
    strcpy(vgrid[45], "xxxxxxxxxxxxxx...........................................xxxxxxxx.xxxxxxxxxxxxxx");
    strcpy(vgrid[46], "xxxxxxxxxxxxxx..........................................xxxxxxxxx.xxxxxxxxxxxxxx");
    strcpy(vgrid[47], "xxxxxxxxxxxxxxxx........................................xxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[48], "xxxxxxxxxxxxxxxx.........................................xxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[49], "xxxxxxxxxxxxxxxxxx.......................................xxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[50], "xxxxxxxxxxxxxxxxxxxx......................................xxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[51], "xxxxxxxxxxxxxxxxxxxx......................................xxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[52], "xxxxxxxxxxxxxxxxxxxxx.....................................xxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[53], "xxxxxxxxxxxxxxxxxxxxx.............................}......xxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[54], "xxxxxxxxxxxxxxxxxxxxxxx.................................xxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[55], "xxxxxxxxxxxxxxxxxxxxxxxx..............................xxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[56], "xxxxxxxxxxxxxxxxxxxxxxxxx..............................xxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[57], "xxxxxxxxxxxxxxxxxxxxxxxxxx............................xxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[58], "xxxxxxxxxxxxxxxxxxxxxxxxxxxx...........{........xxx..xxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[59], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx................xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[60], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.........xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[61], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx...xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    mons_array[0] = MONS_ROYAL_JELLY;
    mons_array[1] = MONS_ACID_BLOB;
    mons_array[2] = MONS_GREAT_ORB_OF_EYES;
    mons_array[3] = RANDOM_MONSTER;
    mons_array[4] = RANDOM_MONSTER;
    mons_array[5] = RANDOM_MONSTER;
    mons_array[6] = RANDOM_MONSTER;

    return MAP_ENCOMPASS;

}


static char hall_of_blades(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{

    strcpy(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6], "xxxxxxxxccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccxxxxxxxx");
    strcpy(vgrid[7], "xxxxxxxxccc....cccc.cccc.cccc.cccc.cccc.cccc.cccc.cccc.cccc.cccc.....cccxxxxxxxx");
    strcpy(vgrid[8], "xxxxxxxxcc......cc...cc...cc...cc...cc...cc...cc...cc...cc...cc.......ccxxxxxxxx");
    strcpy(vgrid[9], "xxxxxxxxc..............................................................cxxxxxxxx");
    strcpy(vgrid[10], "xxxxxxxxc..........c..............c..............c..............c......cxxxxxxxx");
    strcpy(vgrid[11], "xxxxxxxxc.........ccc............ccc............ccc............ccc.....cxxxxxxxx");
    strcpy(vgrid[12], "xxxxxxxxc........ccccc..........ccccc..........ccccc..........ccccc....cxxxxxxxx");
    strcpy(vgrid[13], "xxxxxxxxc.........ccc............ccc............ccc...........ccccc....cxxxxxxxx");
    strcpy(vgrid[14], "xxxxxxxxc..........c..............c..............c.............ccc.....cxxxxxxxx");
    strcpy(vgrid[15], "xxxxxxxxc......................................................ccc.....cxxxxxxxx");
    strcpy(vgrid[16], "xxxxxxxxc.......................................................c......cxxxxxxxx");
    strcpy(vgrid[17], "xxxxxxxxc..............................................................cxxxxxxxx");
    strcpy(vgrid[18], "xxxxxxxxc..............................................................cxxxxxxxx");
    strcpy(vgrid[19], "xxxxxxxxc..............................................................cxxxxxxxx");
    strcpy(vgrid[20], "xxxxxxxxc..............................................................cxxxxxxxx");
    strcpy(vgrid[21], "xxxxxxxxc..............................................................cxxxxxxxx");
    strcpy(vgrid[22], "xxxxxxxxc..............................................................cxxxxxxxx");
    strcpy(vgrid[23], "xxxxxxxxc.......................................................c......cxxxxxxxx");
    strcpy(vgrid[24], "xxxxxxxxc......................................................ccc.....cxxxxxxxx");
    strcpy(vgrid[25], "xxxxxxxxc..........c..............c..............c.............ccc.....cxxxxxxxx");
    strcpy(vgrid[26], "xxxxxxxxc.........ccc............ccc............ccc...........ccccc....cxxxxxxxx");
    strcpy(vgrid[27], "xxxxxxxxc........ccccc..........ccccc..........ccccc..........ccccc....cxxxxxxxx");
    strcpy(vgrid[28], "xxxxxxxxc.........ccc............ccc............ccc............ccc.....cxxxxxxxx");
    strcpy(vgrid[29], "xxxxxxxxc..........c..............c..............c..............c......cxxxxxxxx");
    strcpy(vgrid[30], "xxxxxxxxc..............................................................cxxxxxxxx");
    strcpy(vgrid[31], "xxxxxxxxc.......cc...cc...cc...cc...cc...cc...cc...cc...cc...cc.......ccxxxxxxxx");
    strcpy(vgrid[32], "xxxxxxxxcc.....cccc.cccc.cccc.cccc.cccc.cccc.cccc.cccc.cccc.cccc.....cccxxxxxxxx");
    strcpy(vgrid[33], "xxxxxxxxccc...ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccxxxxxxxx");
    strcpy(vgrid[34], "xxxxxxxxcccc.............................cccccccccccccccccccccccccccccccxxxxxxxx");
    strcpy(vgrid[35], "xxxxxxxxcccccccccccccccccccccccccccccc.@.cccccccccccccccccccccccccccccccxxxxxxxx");

    mons_array[0] = MONS_DANCING_WEAPON;
    mons_array[1] = RANDOM_MONSTER;
    mons_array[2] = RANDOM_MONSTER;
    mons_array[3] = RANDOM_MONSTER;
    mons_array[4] = RANDOM_MONSTER;
    mons_array[5] = RANDOM_MONSTER;
    mons_array[6] = RANDOM_MONSTER;

    return MAP_NORTH;
}


static char hall_of_Zot(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{

    strcpy(vgrid[0],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5],  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6],  "xxxxxxxxxxxxxxccccccccccccccccxxxxxxxxxxxxxxxxxxxxxxxccccccccccccccxxxxxxxxxxxxx");
    strcpy(vgrid[7],  "xxxxxxxxxxxcccc..............ccccxxxxxxxxxxxxxxxxxcccc............ccccxxxxxxxxxx");
    strcpy(vgrid[8],  "xxxxxxxxxxcc....................cccxxxxxxxxxxxxxccc..................ccxxxxxxxxx");
    strcpy(vgrid[9],  "xxxxxxxxxcc...........3...........ccxxxxxxxxxxxcc...........3.........ccxxxxxxxx");
    strcpy(vgrid[10], "xxxxxxxxxc..8......................cXXXXXXXXXXXc....................8..cxxxxxxxx");
    strcpy(vgrid[11], "xxxxxxxxxc...................8.....XXX...1...XXX....8..................cxxxxxxxx");
    strcpy(vgrid[12], "xxxxxxxxxcc........................XX..1...1..XX......................ccxxxxxxxx");
    strcpy(vgrid[13], "xxxxxxxxxxcc.......................X1.........1X.....................ccxxxxxxxxx");
    strcpy(vgrid[14], "xxxxxxxxxxxcc.....4....2..............1..Z..1............2....4.....ccxxxxxxxxxx");
    strcpy(vgrid[15], "xxxxxxxxxxcc.......................X1.........1X.....................ccxxxxxxxxx");
    strcpy(vgrid[16], "xxxxxxxxxcc........................XX..1...1..XX......................ccxxxxxxxx");
    strcpy(vgrid[17], "xxxxxxxxxc...................8.....XXX...1...XXX....8..................cxxxxxxxx");
    strcpy(vgrid[18], "xxxxxxxxxc...8.....................cXXXXXXXXXXXc...................8...cxxxxxxxx");
    strcpy(vgrid[19], "xxxxxxxxxcc..........8............ccccccccccccccc..........8..........ccxxxxxxxx");
    strcpy(vgrid[20], "xxxxxxxxxxcc....................ccccccccccccccccccc..................ccxxxxxxxxx");
    strcpy(vgrid[21], "xxxxxxxxxxxcc................ccccccccccccccccccccccccc..............ccxxxxxxxxxx");
    strcpy(vgrid[22], "xxxxxxxxxxxxccF111FcccccccccccccccccccccccccccccccccccccccccccF111Fccxxxxxxxxxxx");
    strcpy(vgrid[23], "xxxxxxxxxxxcc................^^1.ccccccccccccccccc.1^^..............ccxxxxxxxxxx");
    strcpy(vgrid[24], "xxxxxxxxxxcc.................cc1...ccccccccccccc...1cc...............ccxxxxxxxxx");
    strcpy(vgrid[25], "xxxxxxxxxcc.............8.....ccc...ccccccccccc...ccc...8.............ccxxxxxxxx");
    strcpy(vgrid[26], "xxxxxxxxxc....8................ccc...............ccc...........8...8...cxxxxxxxx");
    strcpy(vgrid[27], "xxxxxxxxxc.......8.....8...8...cxcc.............ccxc...................cxxxxxxxx");
    strcpy(vgrid[28], "xxxxxxxxxc.....................cxxc.............cxxc.......8...........cxxxxxxxx");
    strcpy(vgrid[29], "xxxxxxxxxc.....................cxxcc.1...1...1.ccxxc............8....8.cxxxxxxxx");
    strcpy(vgrid[30], "xxxxxxxxxc.......8....8.....8..cxxxc...........cxxxc....8..............cxxxxxxxx");
    strcpy(vgrid[31], "xxxxxxxxxc.....................cxxcc...........ccxxc..........8........cxxxxxxxx");
    strcpy(vgrid[32], "xxxxxxxxxcc...5...............ccxxc.............cxxcc..............8..ccxxxxxxxx");
    strcpy(vgrid[33], "xxxxxxxxxxcc........8........ccxxcc.............ccxxcc....8....5.....ccxxxxxxxxx");
    strcpy(vgrid[34], "xxxxxxxxxxxcc...............ccxxxc...............cxxxcc.............ccxxxxxxxxxx");
    strcpy(vgrid[35], "xxxxxxxxxxxxcccccccccccccccccxxxxcccccccc@ccccccccxxxxcccccccccccccccxxxxxxxxxxx");

    mons_array[0] = MONS_ORB_GUARDIAN;
    mons_array[1] = MONS_KILLER_KLOWN;
    mons_array[2] = MONS_ELECTRIC_GOLEM;
    mons_array[3] = MONS_ORB_OF_FIRE;
    mons_array[4] = MONS_ANCIENT_LICH;
    mons_array[5] = RANDOM_MONSTER;
    mons_array[6] = RANDOM_MONSTER;

    return MAP_NORTH;
}


static char temple(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // this is the ecumenical temple level
    UNUSED( mons_array );

    strcpy(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[7], "xxxxxxxxxxxxxxxxxxxxxxxxxxccccccccccccccccccccccccccxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[8], "xxxxxxxxxxxxxxxxxxxxxxxxxcc............<............cxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[9], "xxxxxxxxxxxxxxxxxxxxxxxxcc...........................cxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[10], "xxxxxxxxxxxxxxxxxxxxxxxcc.............................cxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[11], "xxxxxxxxxxxxxxxxxxxxxxcc...............................cxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[12], "xxxxxxxxxxxxxxxxxxxxxcc.................................cxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[13], "xxxxxxxxxxxxxxxxxxxxcc...................................cxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[14], "xxxxxxxxxxxxxxxxxxxcc.....................................cxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[15], "xxxxxxxxxxxxxxxxxxcc.......................................cxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[16], "xxxxxxxxxxxxxxxxxcc.........................................cxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[17], "xxxxxxxxxxxxxxxxcc...........................................cxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[18], "xxxxxxxxxxxxxxxcc.............................................cxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[19], "xxxxxxxxxxxxxxcc...............................................cxxxxxxxxxxxxxxxx");
    strcpy(vgrid[20], "xxxxxxxxxxxxxcc.................................................cxxxxxxxxxxxxxxx");
    strcpy(vgrid[21], "xxxxxxxxxxxxcc...................................................cxxxxxxxxxxxxxx");
    strcpy(vgrid[22], "xxxxxxxxxxxcc..........................B..........................cxxxxxxxxxxxxx");
    strcpy(vgrid[23], "xxxxxxxxxxcc.......................................................cxxxxxxxxxxxx");
    strcpy(vgrid[24], "xxxxxxxxxcc.....................B.............B.....................cxxxxxxxxxxx");
    strcpy(vgrid[25], "xxxxxxxxcc...........................................................cxxxxxxxxxx");
    strcpy(vgrid[26], "xxxxxxxxc.............................................................cxxxxxxxxx");
    strcpy(vgrid[27], "xxxxxxxxc.............................................................cxxxxxxxxx");
    strcpy(vgrid[28], "xxxxxxxxc.................B.........................B.................cxxxxxxxxx");
    strcpy(vgrid[29], "xxxxxxxxc..............................T..............................cxxxxxxxxx");
    strcpy(vgrid[30], "xxxxxxxxc.............................................................cxxxxxxxxx");
    strcpy(vgrid[31], "xxxxxxxxc.............................................................cxxxxxxxxx");
    strcpy(vgrid[32], "xxxxxxxxc.............................................................cxxxxxxxxx");
    strcpy(vgrid[33], "xxxxxxxxc.............................................................cxxxxxxxxx");
    strcpy(vgrid[34], "xxxxxxxxc.............................................................cxxxxxxxxx");
    strcpy(vgrid[35], "xxxxxxxxc..............B...............................B..............cxxxxxxxxx");
    strcpy(vgrid[36], "xxxxxxxxc(....................T.................T....................{cxxxxxxxxx");
    strcpy(vgrid[37], "xxxxxxxxc.............................................................cxxxxxxxxx");
    strcpy(vgrid[38], "xxxxxxxxc.............................................................cxxxxxxxxx");
    strcpy(vgrid[39], "xxxxxxxxc.............................................................cxxxxxxxxx");
    strcpy(vgrid[40], "xxxxxxxxc.............................................................cxxxxxxxxx");
    strcpy(vgrid[41], "xxxxxxxxc.............................................................cxxxxxxxxx");
    strcpy(vgrid[42], "xxxxxxxxc.............................................................cxxxxxxxxx");
    strcpy(vgrid[43], "xxxxxxxxc................B...........................B................cxxxxxxxxx");
    strcpy(vgrid[44], "xxxxxxxxcc...........................................................ccxxxxxxxxx");
    strcpy(vgrid[45], "xxxxxxxxxcc............................T............................ccxxxxxxxxxx");
    strcpy(vgrid[46], "xxxxxxxxxxcc.......................................................ccxxxxxxxxxxx");
    strcpy(vgrid[47], "xxxxxxxxxxxcc.....................................................ccxxxxxxxxxxxx");
    strcpy(vgrid[48], "xxxxxxxxxxxxcc...................................................ccxxxxxxxxxxxxx");
    strcpy(vgrid[49], "xxxxxxxxxxxxxcc.................................................ccxxxxxxxxxxxxxx");
    strcpy(vgrid[50], "xxxxxxxxxxxxxxcc...............B................B..............ccxxxxxxxxxxxxxxx");
    strcpy(vgrid[51], "xxxxxxxxxxxxxxxcc.............................................ccxxxxxxxxxxxxxxxx");
    strcpy(vgrid[52], "xxxxxxxxxxxxxxxxcc.....................B.....................ccxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[53], "xxxxxxxxxxxxxxxxxcc.........................................ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[54], "xxxxxxxxxxxxxxxxxxcc.......................................ccxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[55], "xxxxxxxxxxxxxxxxxxxcc.....................................ccxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[56], "xxxxxxxxxxxxxxxxxxxxcc...................................ccxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[57], "xxxxxxxxxxxxxxxxxxxxxcc.................................ccxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[58], "xxxxxxxxxxxxxxxxxxxxxxcc...............................ccxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[59], "xxxxxxxxxxxxxxxxxxxxxxxcc.............................ccxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[60], "xxxxxxxxxxxxxxxxxxxxxxxxcc...........................ccxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[61], "xxxxxxxxxxxxxxxxxxxxxxxxxcc............[............ccxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxcccccccccccccccccccccccccccxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    return MAP_ENCOMPASS;
}


static char tomb_1(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{

    strcpy(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[7], "xxxxxxxxx(.............................[..............................{xxxxxxxxx");
    strcpy(vgrid[8], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[9], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[10], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[11], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[12], "xxxxxxxxx..........ccccccccccccccccccccccccccccccccccccccccccc.........xxxxxxxxx");
    strcpy(vgrid[13], "xxxxxxxxx..........ccccccccccccccccccccccccccccccccccccccccccc.........xxxxxxxxx");
    strcpy(vgrid[14], "xxxxxxxxx..........cc..........................^............cc.........xxxxxxxxx");
    strcpy(vgrid[15], "xxxxxxxxx..........cc.........^....................^........cc.........xxxxxxxxx");
    strcpy(vgrid[16], "xxxxxxxxx..........cc..ccccccccccccccccccccccccccccccccccc..cc.........xxxxxxxxx");
    strcpy(vgrid[17], "xxxxxxxxx..........cc..c....^....^..c................c.^)c..cc.........xxxxxxxxx");
    strcpy(vgrid[18], "xxxxxxxxx..........cc..c..ccccccccc.c..3.............c.^.c..cc.........xxxxxxxxx");
    strcpy(vgrid[19], "xxxxxxxxx..........cc..c..c222c111c.c...............5c..^c..cc.........xxxxxxxxx");
    strcpy(vgrid[20], "xxxxxxxxx..........cc..c..c2c222c.^.c......2.........+cccc..cc.........xxxxxxxxx");
    strcpy(vgrid[21], "xxxxxxxxx..........cc..c..ccccccccccc..........3......5..c.^cc.........xxxxxxxxx");
    strcpy(vgrid[22], "xxxxxxxxx..........cc..c.................................c..cc.........xxxxxxxxx");
    strcpy(vgrid[23], "xxxxxxxxx..........cc..c..........................3......c..cc.........xxxxxxxxx");
    strcpy(vgrid[24], "xxxxxxxxx..........cc^.cccccccccccccc.......2............c..cc.........xxxxxxxxx");
    strcpy(vgrid[25], "xxxxxxxxx..........cc..c............c....................c..cc.........xxxxxxxxx");
    strcpy(vgrid[26], "xxxxxxxxx..........cc..c............c.................3..c..cc.........xxxxxxxxx");
    strcpy(vgrid[27], "xxxxxxxxx..........cc..c..cccccccc..c..........2.........c^5cc.........xxxxxxxxx");
    strcpy(vgrid[28], "xxxxxxxxx..........cc..c..c.^.c11c..c....................c..cc.........xxxxxxxxx");
    strcpy(vgrid[29], "xxxxxxxxx..........cc..c..c.c.c11c..c...3................c..cc.........xxxxxxxxx");
    strcpy(vgrid[30], "xxxxxxxxx..........cc..c..c^c.11cc..c..............2.....c..cc.........xxxxxxxxx");
    strcpy(vgrid[31], "xxxxxxxxx..........cc..c..c.cccccc..c.......2............c..cc.........xxxxxxxxx");
    strcpy(vgrid[32], "xxxxxxxxx..........cc..c..c..^..^...c.................2..c..cc.........xxxxxxxxx");
    strcpy(vgrid[33], "xxxxxxxxx..........cc5^c..ccccccccccc....................c..cc.........xxxxxxxxx");
    strcpy(vgrid[34], "xxxxxxxxx..........cc..c.................................c..cc.........xxxxxxxxx");
    strcpy(vgrid[35], "xxxxxxxxx..........cc..c.................................c..cc.........xxxxxxxxx");
    strcpy(vgrid[36], "xxxxxxxxx..........cc..cccccccccccccc..^.^..cccccccccccccc..cc.........xxxxxxxxx");
    strcpy(vgrid[37], "xxxxxxxxx..........cc..c...........ccc+++++ccc........^..c.^cc.........xxxxxxxxx");
    strcpy(vgrid[38], "xxxxxxxxx..........cc..c.^.....^...cc.2...2.cc......^....c..cc.........xxxxxxxxx");
    strcpy(vgrid[39], "xxxxxxxxx..........cc..c..ccccccc..cc.F...F.cc..ccccccc..c..cc.........xxxxxxxxx");
    strcpy(vgrid[40], "xxxxxxxxx..........cc..c..cc.322c..cc.......cc..c22..cc..c..cc.........xxxxxxxxx");
    strcpy(vgrid[41], "xxxxxxxxx..........cc..c..c].c22c..cc.......cc..c22c.}c^.c..cc.........xxxxxxxxx");
    strcpy(vgrid[42], "xxxxxxxxx..........cc..c..cccc..c.^cc.G...G.cc..c3.cccc..c..cc.........xxxxxxxxx");
    strcpy(vgrid[43], "xxxxxxxxx..........cc..c.....^..c..cc.......cc.^c........c..cc.........xxxxxxxxx");
    strcpy(vgrid[44], "xxxxxxxxx..........cc..c........c..cc.......cc..c....^...c..cc.........xxxxxxxxx");
    strcpy(vgrid[45], "xxxxxxxxx..........cc^.cccccccccc..cc.G...G.cc..cccccccccc.^cc.........xxxxxxxxx");
    strcpy(vgrid[46], "xxxxxxxxx..........cc......^.......cc.......cc..........^...cc.........xxxxxxxxx");
    strcpy(vgrid[47], "xxxxxxxxx..........cc...........^..cc.......cc.....^........cc.........xxxxxxxxx");
    strcpy(vgrid[48], "xxxxxxxxx..........cccccccccccccccccc.G...G.cccccccccccccccccc.........xxxxxxxxx");
    strcpy(vgrid[49], "xxxxxxxxx..........cccccccccccccccccc.......cccccccccccccccccc.........xxxxxxxxx");
    strcpy(vgrid[50], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[51], "xxxxxxxxx.............................G...G............................xxxxxxxxx");
    strcpy(vgrid[52], "xxxxxxxxx...........................4.......4..........................xxxxxxxxx");
    strcpy(vgrid[53], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[54], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[55], "xxxxxxxxx...........................4..V.V..4..........................xxxxxxxxx");
    strcpy(vgrid[56], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[57], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[58], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[59], "xxxxxxxxx...........................4.......4..........................xxxxxxxxx");
    strcpy(vgrid[60], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[61], "xxxxxxxxx..............................................................xxxxxxxxx");
    strcpy(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    mons_array[0] = MONS_MUMMY;
    mons_array[1] = MONS_GUARDIAN_MUMMY;
    mons_array[2] = MONS_MUMMY_PRIEST;
    mons_array[3] = MONS_SPHINX;
    mons_array[4] = MONS_GREATER_MUMMY;
    mons_array[5] = RANDOM_MONSTER;
    mons_array[6] = RANDOM_MONSTER;

    return MAP_ENCOMPASS;
}


static char tomb_2(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{

    strcpy(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[7], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[8], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[9], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[10], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[11], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[12], "xxxxxxxxxxxxxxxxxxxcccccccccccccccccccccccccccccccccccccccccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[13], "xxxxxxxxxxxxxxxxxxxcccccccccccccccccccccccccccccccccccccccccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[14], "xxxxxxxxxxxxxxxxxxxcc{...c......c.....3....c........c.......ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[15], "xxxxxxxxxxxxxxxxxxxcc....c.....^c^........^c......2^c.......ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[16], "xxxxxxxxxxxxxxxxxxxcc....c...2.^+..2.....2^+^..2....+.......ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[17], "xxxxxxxxxxxxxxxxxxxcc.3.^c^.....c^.........c^2.....^c^......ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[18], "xxxxxxxxxxxxxxxxxxxcc...^+^.....c..........c........c...2...ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[19], "xxxxxxxxxxxxxxxxxxxccccc+ccccccccccccccccccccccccccccccc....ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[20], "xxxxxxxxxxxxxxxxxxxcc..^.c.............................c....ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[21], "xxxxxxxxxxxxxxxxxxxcc....c.............................c..3.ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[22], "xxxxxxxxxxxxxxxxxxxcc....c..ccc4.................4ccc..c....ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[23], "xxxxxxxxxxxxxxxxxxxcc....c..ccc...................ccc..c..2.ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[24], "xxxxxxxxxxxxxxxxxxxcc....c..ccc.........1.........ccc..c)..}ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[25], "xxxxxxxxxxxxxxxxxxxcc.3..c..ccc.....2.......2.....ccc..cccccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[26], "xxxxxxxxxxxxxxxxxxxcc....c.............................c....ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[27], "xxxxxxxxxxxxxxxxxxxcc....c.............................c^2..ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[28], "xxxxxxxxxxxxxxxxxxxcc....c........c...........c........+....ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[29], "xxxxxxxxxxxxxxxxxxxcc]...c.............................c^2..ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[30], "xxxxxxxxxxxxxxxxxxxccccccc.....3........(........3.....c....ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[31], "xxxxxxxxxxxxxxxxxxxcc....c.............................c.^.^ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[32], "xxxxxxxxxxxxxxxxxxxcc...^c........c...........c........ccc+cccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[33], "xxxxxxxxxxxxxxxxxxxcc....+.............................c..^.ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[34], "xxxxxxxxxxxxxxxxxxxcc...^c.............................c....ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[35], "xxxxxxxxxxxxxxxxxxxcc....c..ccc.....2.......2.....ccc..c..2.ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[36], "xxxxxxxxxxxxxxxxxxxccccccc..ccc.........1.........ccc..c....ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[37], "xxxxxxxxxxxxxxxxxxxcc....c..ccc...................ccc..c....ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[38], "xxxxxxxxxxxxxxxxxxxcc...^+..ccc4.................4ccc..c2...ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[39], "xxxxxxxxxxxxxxxxxxxcc....c.............................c....ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[40], "xxxxxxxxxxxxxxxxxxxccccccc.............................c..2.ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[41], "xxxxxxxxxxxxxxxxxxxcc....c.............................ccc+cccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[42], "xxxxxxxxxxxxxxxxxxxcc....+cccc+ccccccccccccccc+ccccccccc.^.^ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[43], "xxxxxxxxxxxxxxxxxxxcc.1.^^.c.^..c............c^.......c.3...ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[44], "xxxxxxxxxxxxxxxxxxxcc...2..c.1..c.....1.1....c.....2..c.....ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[45], "xxxxxxxxxxxxxxxxxxxcc......c....c..1......1.^c..2.....c...2.ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[46], "xxxxxxxxxxxxxxxxxxxcc..3...c.1..c...1...1..1^+........c.....ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[47], "xxxxxxxxxxxxxxxxxxxcc......c....c[...........c.......3c.....ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[48], "xxxxxxxxxxxxxxxxxxxcccccccccccccccccccccccccccccccccccccccccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[49], "xxxxxxxxxxxxxxxxxxxcccccccccccccccccccccccccccccccccccccccccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[50], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[51], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[52], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[53], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[54], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[55], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[56], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[57], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[58], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[59], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[60], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[61], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    mons_array[0] = MONS_MUMMY;
    mons_array[1] = MONS_GUARDIAN_MUMMY;
    mons_array[2] = MONS_MUMMY_PRIEST;
    mons_array[3] = MONS_GREATER_MUMMY;
    mons_array[4] = RANDOM_MONSTER;
    mons_array[5] = RANDOM_MONSTER;
    mons_array[6] = RANDOM_MONSTER;

    return MAP_ENCOMPASS;
}


static char tomb_3(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{

    strcpy(vgrid[0], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[1], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[2], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[3], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[4], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[5], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[6], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[7], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[8], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[9], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[10], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[11], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[12], "xxxxxxxxxxxxxxxxxxxcccccccccccccccccccccccccccccccccccccccccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[13], "xxxxxxxxxxxxxxxxxxxcccccccccccccccccccccccccccccccccccccccccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[14], "xxxxxxxxxxxxxxxxxxxccccccc.............................cccccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[15], "xxxxxxxxxxxxxxxxxxxcccc...............cccccc..............ccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[16], "xxxxxxxxxxxxxxxxxxxccc...............cccccccc..............cccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[17], "xxxxxxxxxxxxxxxxxxxccc.......4......ccccO4cccc......4......cccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[18], "xxxxxxxxxxxxxxxxxxxccc............cccc......cccc...........cccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[19], "xxxxxxxxxxxxxxxxxxxcc............cccc........cccc...........ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[20], "xxxxxxxxxxxxxxxxxxxcc............cccc........cccc...........ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[21], "xxxxxxxxxxxxxxxxxxxcc...........cccc..444444..cccc..........ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[22], "xxxxxxxxxxxxxxxxxxxcc.......................................ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[23], "xxxxxxxxxxxxxxxxxxxcc.......................................ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[24], "xxxxxxxxxxxxxxxxxxxcc.................222222................ccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[25], "xxxxxxxxxxxxxxxxxxxccc................223322...............cccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[26], "xxxxxxxxxxxxxxxxxxxccc...3............223322............3..cccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[27], "xxxxxxxxxxxxxxxxxxxcccc...............222222..............ccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[28], "xxxxxxxxxxxxxxxxxxxcccc....2..........................2...ccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[29], "xxxxxxxxxxxxxxxxxxxcccccc....2......................2....cccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[30], "xxxxxxxxxxxxxxxxxxxcccccccc............................cccccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[31], "xxxxxxxxxxxxxxxxxxxccccccccc+ccc..................ccc+ccccccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[32], "xxxxxxxxxxxxxxxxxxxcccccccc....cc................cc....cccccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[33], "xxxxxxxxxxxxxxxxxxxcccccc.......cc22222222222222cc......$cccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[34], "xxxxxxxxxxxxxxxxxxxcccc....^.....cc............cc..^.....$ccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[35], "xxxxxxxxxxxxxxxxxxxcccc.^.........cc..........cc.....^.^.$ccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[36], "xxxxxxxxxxxxxxxxxxxccc$...^...^..^.cc........cc..........$$cccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[37], "xxxxxxxxxxxxxxxxxxxccc$$$...........cc222222cc.^........$$$cccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[38], "xxxxxxxxxxxxxxxxxxxccc|$$$...........c......c.....^...$$$$$cccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[39], "xxxxxxxxxxxxxxxxxxxccc||$$$$...^.....c......c^......$$$$$$$cccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[40], "xxxxxxxxxxxxxxxxxxxccc|||||$$.....^..c......c......$$$$$$$$cccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[41], "xxxxxxxxxxxxxxxxxxxcccc|||||$........c......c...^.$$$$$$$$ccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[42], "xxxxxxxxxxxxxxxxxxxccccc||||$$..^....c......c.....$$$$$$$cccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[43], "xxxxxxxxxxxxxxxxxxxcccccc||||$.......c......c.....$$$$$$ccccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[44], "xxxxxxxxxxxxxxxxxxxccccccc|||$$....^.c......c.^.^$$$$$$cccccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[45], "xxxxxxxxxxxxxxxxxxxcccccccc|||$^....cc..{...cc...$$$$$ccccccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[46], "xxxxxxxxxxxxxxxxxxxccccccccc||$.....cc...(..cc..$$$$$cccccccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[47], "xxxxxxxxxxxxxxxxxxxcccccccccc|$...cccc..[...cccc$$$$ccccccccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[48], "xxxxxxxxxxxxxxxxxxxcccccccccccccccccccccccccccccccccccccccccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[49], "xxxxxxxxxxxxxxxxxxxcccccccccccccccccccccccccccccccccccccccccccxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[50], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[51], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[52], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[53], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[54], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[55], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[56], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[57], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[58], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[59], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[60], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[61], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcpy(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    mons_array[0] = MONS_MUMMY;
    mons_array[1] = MONS_GUARDIAN_MUMMY;
    mons_array[2] = MONS_MUMMY_PRIEST;
    mons_array[3] = MONS_GREATER_MUMMY;
    mons_array[4] = RANDOM_MONSTER;
    mons_array[5] = RANDOM_MONSTER;
    mons_array[6] = RANDOM_MONSTER;

    return MAP_ENCOMPASS;
}



static char swamp(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // NB - most of the 'x's here will be set to water in dungeon.cc

    for (unsigned char i = 0; i < 81; i++)
        strcpy(vgrid[i], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    strcat(vgrid[36], "xxxxxxxxxxx@xxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[37], "xxxxxxxxxxx2xxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[38], "xxxxxxxxxx.xxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[39], "xxxxxxxxxxx.xxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[40], "xxxxxxxxxx2x.xxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[41], "xxxxxxxxxxxx.xxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[42], "xxxxxxxxxcc.ccxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[43], "xxxxxxxxcc...ccxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[44], "xxxxxxxcc3.2..ccxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[45], "xxxxxxcc.1.3.2.ccxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[46], "xxxxxccc....1.1cccxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[47], "xxxxxcc.1.32....ccxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[48], "xxxxxcc...3..1.3ccxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[49], "xxxxxcc2.1.3..2.ccxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[50], "xxxxxccc33..1..cccxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[51], "xxxxxxcccc3O3ccccxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[52], "xxxxxxxcccccccccxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[53], "xxxxxxxxcccccccxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[54], "xxxxxxxxxxcccxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[55], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[56], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[57], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[58], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[59], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[60], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[61], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[62], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[63], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[64], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[65], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[66], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[67], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[68], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    strcat(vgrid[69], "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    mons_array[0] = MONS_SWAMP_DRAGON;
    mons_array[1] = MONS_SWAMP_DRAKE;
    mons_array[2] = MONS_HYDRA;
    mons_array[3] = RANDOM_MONSTER;
    mons_array[4] = RANDOM_MONSTER;
    mons_array[5] = RANDOM_MONSTER;
    mons_array[6] = RANDOM_MONSTER;

    return MAP_SOUTHEAST;
}


/*
   NOTE: *Cannot* place 8,9 or 0 monsters in branch vaults which neither use the
   normal mons_level function or are around level 35, or generation will crash.

   Remember, minivaults are always sidewards
*/

static char minivault_1(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], "..xxxx=xxx..");
    strcpy(vgrid[2], ".xx..x...xx.");
    strcpy(vgrid[3], ".x....x...x.");
    strcpy(vgrid[4], ".x...x....x.");
    strcpy(vgrid[5], ".xx.x*x.x.=.");
    strcpy(vgrid[6], ".=.x.x*x.xx.");
    strcpy(vgrid[7], ".x....x...x.");
    strcpy(vgrid[8], ".x...x....x.");
    strcpy(vgrid[9], ".xx...x..xx.");
    strcpy(vgrid[10], "..xxx=xxxx..");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;
}


static char minivault_2(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], "..xxxx.xxxx.");
    strcpy(vgrid[2], "..xx.....xx.");
    strcpy(vgrid[3], "..x.......x.");
    strcpy(vgrid[4], "..x.......x.");
    strcpy(vgrid[5], "......C.....");
    strcpy(vgrid[6], "..x.......x.");
    strcpy(vgrid[7], "..x.......x.");
    strcpy(vgrid[8], "..xx.....xx.");
    strcpy(vgrid[9], "..xxxx.xxxx.");
    strcpy(vgrid[10], "............");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;
}


static char minivault_3(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".cccccccccc.");
    strcpy(vgrid[2], ".cccccccccc.");
    strcpy(vgrid[3], ".cBcBcBcBcc.");
    strcpy(vgrid[4], ".G.c.c.c.Bc.");
    strcpy(vgrid[5], ".........Bc.");
    strcpy(vgrid[6], ".........Bc.");
    strcpy(vgrid[7], ".G.c.c.c.Bc.");
    strcpy(vgrid[8], ".cBcBcBcBcc.");
    strcpy(vgrid[9], ".cccccccccc.");
    strcpy(vgrid[10], ".cccccccccc.");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;
}


static char minivault_4(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], "....xwxx....");
    strcpy(vgrid[2], "..xxxwwxwx..");
    strcpy(vgrid[3], "..xwwwwwwx..");
    strcpy(vgrid[4], ".xwwxwwxwxx.");
    strcpy(vgrid[5], ".xwwwwwwwwx.");
    strcpy(vgrid[6], ".xwwxwwwxww.");
    strcpy(vgrid[7], ".xxwwwwwwxx.");
    strcpy(vgrid[8], "..wwwwxwwx..");
    strcpy(vgrid[9], "..xxxwwxxw..");
    strcpy(vgrid[10], "....xxww....");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;
}


static char minivault_5(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".x.xxxxxxxx.");
    strcpy(vgrid[2], ".x.x......x.");
    strcpy(vgrid[3], ".x.x.xxxx.x.");
    strcpy(vgrid[4], ".x.x.x**x.x.");
    strcpy(vgrid[5], ".x.x.x**x.x.");
    strcpy(vgrid[6], ".x.x.xx.x.x.");
    strcpy(vgrid[7], ".x.x....x.x.");
    strcpy(vgrid[8], ".x.xxxxxx.x.");
    strcpy(vgrid[9], ".x........x.");
    strcpy(vgrid[10], ".xxxxxxxxxx.");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;
}


static char minivault_6(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // Wizard's laboratory

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".ccccccc+cc.");
    strcpy(vgrid[2], ".c........c.");
    strcpy(vgrid[3], ".c........c.");
    strcpy(vgrid[4], ".c..1.....c.");
    strcpy(vgrid[5], ".c........c.");
    strcpy(vgrid[6], ".cc+ccccccc.");
    strcpy(vgrid[7], ".c***c3232c.");
    strcpy(vgrid[8], ".c|**+2223c.");
    strcpy(vgrid[9], ".c||*c3322c.");
    strcpy(vgrid[10], ".cccccccccc.");
    strcpy(vgrid[11], "............");

    mons_array[0] = MONS_WIZARD;
    mons_array[1] = MONS_ABOMINATION_SMALL;
    mons_array[2] = MONS_ABOMINATION_LARGE;

    return MAP_NORTH;

}


static char minivault_7(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // beehive minivault

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], "....aaaa....");
    strcpy(vgrid[2], "..a2a2aaaa..");
    strcpy(vgrid[3], "..aaRa3a2a..");
    strcpy(vgrid[4], ".aa2aRa2aaa.");
    strcpy(vgrid[5], ".a3aRa1aRa2.");
    strcpy(vgrid[6], ".aa3aRaRa2a.");
    strcpy(vgrid[7], ".aaa2a2a3aa.");
    strcpy(vgrid[8], "..a3aRa2aa..");
    strcpy(vgrid[9], "...aa2aa2a..");
    strcpy(vgrid[10], "....aaaa....");
    strcpy(vgrid[11], "............");

    mons_array[0] = MONS_QUEEN_BEE;
    mons_array[1] = MONS_KILLER_BEE;
    mons_array[2] = MONS_KILLER_BEE_LARVA;

    return MAP_NORTH;
}


static char minivault_8(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // lava pond

    strcpy(vgrid[0], "x.x.x.x.x.x.");
    strcpy(vgrid[1], ".c.c.c.c.c.x");
    strcpy(vgrid[2], "x...l1l...c.");
    strcpy(vgrid[3], ".c.llllll..x");
    strcpy(vgrid[4], "x.lllllll1c.");
    strcpy(vgrid[5], ".c.llFGll..x");
    strcpy(vgrid[6], "x..llGFll.c.");
    strcpy(vgrid[7], ".c1lllllll.x");
    strcpy(vgrid[8], "x..llllll.c.");
    strcpy(vgrid[9], ".c...l1l...x");
    strcpy(vgrid[10], "x.c.c.c.c.c.");
    strcpy(vgrid[11], ".x.x.x.x.x.x");

    mons_array[0] = MONS_MOLTEN_GARGOYLE;

    return MAP_NORTH;
}


static char minivault_9(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // evil zoo
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".==========.");
    strcpy(vgrid[2], ".==========.");
    strcpy(vgrid[3], ".==========.");
    strcpy(vgrid[4], ".===8888===.");
    strcpy(vgrid[5], ".===8998===.");
    strcpy(vgrid[6], ".===8998===.");
    strcpy(vgrid[7], ".===8888===.");
    strcpy(vgrid[8], ".==========.");
    strcpy(vgrid[9], ".==========.");
    strcpy(vgrid[10], ".==========.");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;

}


static char minivault_10(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".xxxx..xxxx.");
    strcpy(vgrid[2], ".x**x..x**x.");
    strcpy(vgrid[3], ".x**+..+**x.");
    strcpy(vgrid[4], ".xx+x..x+xx.");
    strcpy(vgrid[5], "............");
    strcpy(vgrid[6], "............");
    strcpy(vgrid[7], ".xx+x..x+xx.");
    strcpy(vgrid[8], ".x**+..+**x.");
    strcpy(vgrid[9], ".x**x..x**x.");
    strcpy(vgrid[10], ".xxxx..xxxx.");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;
}


static char minivault_11(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // multicoloured onion
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".+xxxxxxxx+.");
    strcpy(vgrid[2], ".x........x.");
    strcpy(vgrid[3], ".x.+cccc+.x.");
    strcpy(vgrid[4], ".x.c....c.x.");
    strcpy(vgrid[5], ".x.c.bb.c.x.");
    strcpy(vgrid[6], ".x.c.bb.c.x.");
    strcpy(vgrid[7], ".x.c....c.x.");
    strcpy(vgrid[8], ".x.+cccc+.x.");
    strcpy(vgrid[9], ".x........x.");
    strcpy(vgrid[10], ".+xxxxxxxx+.");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;
}


static char minivault_12(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // closed box minivault
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".xxxxxxxxxx.");
    strcpy(vgrid[2], ".x>9$9$9$<x.");
    strcpy(vgrid[3], ".x.$9$9$.$x.");
    strcpy(vgrid[4], ".x$.****$.x.");
    strcpy(vgrid[5], ".x.$*||*.$x.");
    strcpy(vgrid[6], ".x$.*||*$.x.");
    strcpy(vgrid[7], ".x.$****.$x.");
    strcpy(vgrid[8], ".x$9$9$9$.x.");
    strcpy(vgrid[9], ".x<$9$9$9>x.");
    strcpy(vgrid[10], ".xxxxxxxxxx.");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;
}


static char minivault_13(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // little trap spiral
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".xxxxxxxxxx.");
    strcpy(vgrid[2], ".=.^x..=.9x.");
    strcpy(vgrid[3], ".x.$=.^x..x.");
    strcpy(vgrid[4], ".xxxxxxxx=x.");
    strcpy(vgrid[5], ".x.8+|0x8.x.");
    strcpy(vgrid[6], ".x8$x.|x..x.");
    strcpy(vgrid[7], ".xx=xxxx=xx.");
    strcpy(vgrid[8], ".x.9=^.x..x.");
    strcpy(vgrid[9], ".x..x.^=9.x.");
    strcpy(vgrid[10], ".xxxxxxxxxx.");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;
}


static char minivault_14(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // water cross
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".wwwww.wwww.");
    strcpy(vgrid[2], ".wwwww.wwww.");
    strcpy(vgrid[3], ".wwwww.wwww.");
    strcpy(vgrid[4], ".wwwww.wwww.");
    strcpy(vgrid[5], ".......wwww.");
    strcpy(vgrid[6], ".wwww.......");
    strcpy(vgrid[7], ".wwww.wwwww.");
    strcpy(vgrid[8], ".wwww.wwwww.");
    strcpy(vgrid[9], ".wwww.wwwww.");
    strcpy(vgrid[10], ".wwww.wwwww.");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;
}

static char minivault_15(char vgrid[81][81], FixedVector<int, 7>& mons_array) /* lava pond */
{
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], "............");
    strcpy(vgrid[2], "....lll.....");
    strcpy(vgrid[3], "...vvlvv....");
    strcpy(vgrid[4], "..lv|*|vl...");
    strcpy(vgrid[5], "..ll*S*ll...");
    strcpy(vgrid[6], "..lv|*|vl...");
    strcpy(vgrid[7], "...vvlvv....");
    strcpy(vgrid[8], "....lll.....");
    strcpy(vgrid[9], "............");
    strcpy(vgrid[10], "............");
    strcpy(vgrid[11], "............");

    return 1;
}


static char minivault_16(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // lava pond
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], "............");
    strcpy(vgrid[2], "............");
    strcpy(vgrid[3], "............");
    strcpy(vgrid[4], "............");
    strcpy(vgrid[5], "............");
    strcpy(vgrid[6], "......S.....");
    strcpy(vgrid[7], "............");
    strcpy(vgrid[8], "............");
    strcpy(vgrid[9], "............");
    strcpy(vgrid[10], "............");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;
}


static char minivault_17(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // lava pond
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], "............");
    strcpy(vgrid[2], "............");
    strcpy(vgrid[3], "............");
    strcpy(vgrid[4], "............");
    strcpy(vgrid[5], ".....F......");
    strcpy(vgrid[6], "............");
    strcpy(vgrid[7], "............");
    strcpy(vgrid[8], "............");
    strcpy(vgrid[9], "............");
    strcpy(vgrid[10], "............");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;
}


static char minivault_18(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // lava pond
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], "............");
    strcpy(vgrid[2], "............");
    strcpy(vgrid[3], "............");
    strcpy(vgrid[4], "............");
    strcpy(vgrid[5], ".....H......");
    strcpy(vgrid[6], "............");
    strcpy(vgrid[7], "............");
    strcpy(vgrid[8], "............");
    strcpy(vgrid[9], "............");
    strcpy(vgrid[10], "............");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;
}


static char minivault_19(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".xx......xx.");
    strcpy(vgrid[2], ".xxx....xxx.");
    strcpy(vgrid[3], "..xxx..xxx..");
    strcpy(vgrid[4], "...xxxxxx...");
    strcpy(vgrid[5], "....xxxx....");
    strcpy(vgrid[6], "....xxxx....");
    strcpy(vgrid[7], "...xxxxxx...");
    strcpy(vgrid[8], "..xxx..xxx..");
    strcpy(vgrid[9], ".xxx....xxx.");
    strcpy(vgrid[10], ".xx......xx.");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;
}


static char minivault_20(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // lava pond
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".xxxx..xxxx.");
    strcpy(vgrid[2], ".x........x.");
    strcpy(vgrid[3], ".x..xxxx..x.");
    strcpy(vgrid[4], ".x.x....x.x.");
    strcpy(vgrid[5], "...x.x9.x...");
    strcpy(vgrid[6], "...x.9x.x...");
    strcpy(vgrid[7], ".x.x....x.x.");
    strcpy(vgrid[8], ".x..xxxx..x.");
    strcpy(vgrid[9], ".x........x.");
    strcpy(vgrid[10], ".xxxx..xxxx.");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;
}


static char minivault_21(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".^xxxxxxxx^.");
    strcpy(vgrid[2], ".x........x.");
    strcpy(vgrid[3], ".x.cccccc.x.");
    strcpy(vgrid[4], ".x.c|..<c.x.");
    strcpy(vgrid[5], ".x.c.**.c.x.");
    strcpy(vgrid[6], ".x.c.**.c.x.");
    strcpy(vgrid[7], ".x.c>..|c.x.");
    strcpy(vgrid[8], ".x.cccccc.x.");
    strcpy(vgrid[9], ".x........x.");
    strcpy(vgrid[10], ".^xxxxxxxx^.");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;

}


static char minivault_22(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".....xx.....");
    strcpy(vgrid[2], "...xxxxxx...");
    strcpy(vgrid[3], "..x^x..x^x..");
    strcpy(vgrid[4], "..xx.xx.xx..");
    strcpy(vgrid[5], ".xx.x$$x.xx.");
    strcpy(vgrid[6], ".xx.x$$x.xx.");
    strcpy(vgrid[7], "..xx.xx.xx..");
    strcpy(vgrid[8], "..x^x..x^x..");
    strcpy(vgrid[9], "...xxxxxx...");
    strcpy(vgrid[10], ".....xx.....");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;

}


static char minivault_23(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{
    UNUSED( mons_array );

    strcpy(vgrid[0], "x.x.x.x.x.x.");
    strcpy(vgrid[1], ".x.x.x.x.x.x");
    strcpy(vgrid[2], "x.x.x.x.x.x.");
    strcpy(vgrid[3], ".x.x.x.x.x.x");
    strcpy(vgrid[4], "x.x.x.x.x.x.");
    strcpy(vgrid[5], ".x.x.x.x.x.x");
    strcpy(vgrid[6], "x.x.x.x.x.x.");
    strcpy(vgrid[7], ".x.x.x.x.x.x");
    strcpy(vgrid[8], "x.x.x.x.x.x.");
    strcpy(vgrid[9], ".x.x.x.x.x.x");
    strcpy(vgrid[10], "x.x.x.x.x.x.");
    strcpy(vgrid[11], ".x.x.x.x.x.x");

    return MAP_NORTH;

}


static char minivault_24(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], "....xxxx....");
    strcpy(vgrid[2], "....xxxx....");
    strcpy(vgrid[3], "....xxxx....");
    strcpy(vgrid[4], ".xxxx.x.xxx.");
    strcpy(vgrid[5], ".xxx.x.xxxx.");
    strcpy(vgrid[6], ".xxxx.x.xxx.");
    strcpy(vgrid[7], ".xxx.x.xxxx.");
    strcpy(vgrid[8], "....xxxx....");
    strcpy(vgrid[9], "....xxxx....");
    strcpy(vgrid[10], "....xxxx....");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;

}


static char minivault_25(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".xx+xxxxxxx.");
    strcpy(vgrid[2], ".x........x.");
    strcpy(vgrid[3], ".x........+.");
    strcpy(vgrid[4], ".x........x.");
    strcpy(vgrid[5], ".x........x.");
    strcpy(vgrid[6], ".x........x.");
    strcpy(vgrid[7], ".x........x.");
    strcpy(vgrid[8], ".+........x.");
    strcpy(vgrid[9], ".x........x.");
    strcpy(vgrid[10], ".xxxxxxx+xx.");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;

}


static char minivault_26(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{
    UNUSED( mons_array );

    strcpy(vgrid[0], "c..........c");
    strcpy(vgrid[1], ".c...cc...c.");
    strcpy(vgrid[2], "..c..cc..c..");
    strcpy(vgrid[3], "...c....c...");
    strcpy(vgrid[4], "....c..c....");
    strcpy(vgrid[5], ".cc..cc..cc.");
    strcpy(vgrid[6], ".cc..cc..cc.");
    strcpy(vgrid[7], "....c..c....");
    strcpy(vgrid[8], "...c....c...");
    strcpy(vgrid[9], "..c..cc..c..");
    strcpy(vgrid[10], ".c...cc...c.");
    strcpy(vgrid[11], "c..........c");

    return MAP_NORTH;

}


static char minivault_27(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".x.xxxxxxxx.");
    strcpy(vgrid[2], ".x........x.");
    strcpy(vgrid[3], ".xxxxxxxx.x.");
    strcpy(vgrid[4], ".x........x.");
    strcpy(vgrid[5], ".x.xxxxxxxx.");
    strcpy(vgrid[6], ".x........x.");
    strcpy(vgrid[7], ".xxxxxxxx.x.");
    strcpy(vgrid[8], ".x........x.");
    strcpy(vgrid[9], ".x.xxxxxxxx.");
    strcpy(vgrid[10], "............");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;

}


static char minivault_28(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".xxxx.xxxx..");
    strcpy(vgrid[2], ".x.......x..");
    strcpy(vgrid[3], ".x..999..x..");
    strcpy(vgrid[4], ".x.9...9.x..");
    strcpy(vgrid[5], "...9.I.9....");
    strcpy(vgrid[6], ".x.9...9.x..");
    strcpy(vgrid[7], ".x..999..x..");
    strcpy(vgrid[8], ".x.......x..");
    strcpy(vgrid[9], ".xxxx.xxxx..");
    strcpy(vgrid[10], "............");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;

}


static char minivault_29(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{

    strcpy(vgrid[0], ".3......3...");
    strcpy(vgrid[1], "...x.xx.x.2.");
    strcpy(vgrid[2], ".xxx2xxxxx..");
    strcpy(vgrid[3], ".xxxx42xxx2.");
    strcpy(vgrid[4], ".2xx243432x3");
    strcpy(vgrid[5], ".xx421424xx.");
    strcpy(vgrid[6], "3xx423242x..");
    strcpy(vgrid[7], ".x2x3243xxx.");
    strcpy(vgrid[8], ".x2xx42422x.");
    strcpy(vgrid[9], "..xxxxxxxx2.");
    strcpy(vgrid[10], "...x2xxxx3..");
    strcpy(vgrid[11], ".3.......33.");

    mons_array[0] = MONS_QUEEN_ANT;
    mons_array[1] = MONS_SOLDIER_ANT;
    mons_array[2] = MONS_GIANT_ANT;
    mons_array[3] = MONS_ANT_LARVA;

    return MAP_NORTH;

}


static char minivault_30(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // lava pond
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], "............");
    strcpy(vgrid[2], "............");
    strcpy(vgrid[3], "............");
    strcpy(vgrid[4], "............");
    strcpy(vgrid[5], ".....T......");
    strcpy(vgrid[6], "............");
    strcpy(vgrid[7], "............");
    strcpy(vgrid[8], "............");
    strcpy(vgrid[9], "............");
    strcpy(vgrid[10], "............");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;

}


static char minivault_31(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // lava pond
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], "............");
    strcpy(vgrid[2], "............");
    strcpy(vgrid[3], "............");
    strcpy(vgrid[4], "............");
    strcpy(vgrid[5], ".....T......");
    strcpy(vgrid[6], "............");
    strcpy(vgrid[7], "............");
    strcpy(vgrid[8], "............");
    strcpy(vgrid[9], "............");
    strcpy(vgrid[10], "............");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;

}


static char minivault_32(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // lava pond
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], "............");
    strcpy(vgrid[2], "............");
    strcpy(vgrid[3], "............");
    strcpy(vgrid[4], "............");
    strcpy(vgrid[5], ".....U......");
    strcpy(vgrid[6], "............");
    strcpy(vgrid[7], "............");
    strcpy(vgrid[8], "............");
    strcpy(vgrid[9], "............");
    strcpy(vgrid[10], "............");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;

}


static char minivault_33(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // lava pond
    UNUSED( mons_array );

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], "............");
    strcpy(vgrid[2], "............");
    strcpy(vgrid[3], "............");
    strcpy(vgrid[4], "............");
    strcpy(vgrid[5], ".....V......");
    strcpy(vgrid[6], "............");
    strcpy(vgrid[7], "............");
    strcpy(vgrid[8], "............");
    strcpy(vgrid[9], "............");
    strcpy(vgrid[10], "............");
    strcpy(vgrid[11], "............");

    return MAP_NORTH;

}

static char minivault_34(char vgrid[81][81], FixedVector<int, 7>& mons_array, bool orientation)
{ 
    //jmf: multi-god temple thing
    UNUSED( mons_array );
    int i, di;

    if ( orientation )
    { 
        i = 0; 
        di = +1;  
    }
    else
    { 
        i = 11; 
        di = -1; 
    }

    for (int c=0; c <= 11; c++, i += di)
    {
        strcpy(vgrid[i], "............");
        strcpy(vgrid[i], ".=xxxxxxxx=.");
        strcpy(vgrid[i], ".x9......9x.");
        strcpy(vgrid[i], ".xT......Tx.");
        strcpy(vgrid[i], ".x..C..C..x.");
        strcpy(vgrid[i], ".xT......Tx.");
        strcpy(vgrid[i], ".xxxxxxxxxx.");
        strcpy(vgrid[i], ".xxx$$$$xxx.");
        strcpy(vgrid[i], ".xx8....8xx.");
        strcpy(vgrid[i], "..xx....xx..");
        strcpy(vgrid[i], "...xG..Gx...");
        strcpy(vgrid[i], "............");
    }

    return MAP_NORTH;
}

static char minivault_34_a(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{ 
    return minivault_34(vgrid, mons_array, true); 
}

static char minivault_34_b(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{ 
    return minivault_34(vgrid, mons_array, false); 
}

static char minivault_35(char vgrid[81][81], FixedVector<int, 7>& mons_array, bool orientation)
{ 
    UNUSED( mons_array );
    //jmf: another multi-god temple thing
    int i, di;

    if (orientation)
    { 
        i = 0; 
        di = +1;  
    }
    else
    { 
        i = 11;
        di = -1; 
    }

    for (int c=0; c <= 11; c++, i += di)
    {
        strcpy(vgrid[i], "............");
        strcpy(vgrid[i], "..vvvvvvvv..");
        strcpy(vgrid[i], ".vv......vv.");
        strcpy(vgrid[i], ".v..x..x..v.");
        strcpy(vgrid[i], ".v.Cx..xC.v.");
        strcpy(vgrid[i], ".v..x..x..v.");
        strcpy(vgrid[i], ".vT8x..x8Tv.");
        strcpy(vgrid[i], ".vvvx==xvvv.");
        strcpy(vgrid[i], "...Gx99xG...");
        strcpy(vgrid[i], "...+*99*+...");
        strcpy(vgrid[i], "...GxxxxG...");
        strcpy(vgrid[i], "............");
    }

    return MAP_NORTH;
}

static char minivault_35_a(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{ 
    return minivault_35(vgrid, mons_array, true); 
}

static char minivault_35_b(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{ 
    return minivault_35(vgrid, mons_array, false); 
}


static char rand_demon_1(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".xx.xx.x.xx.");
    strcpy(vgrid[2], "..x.x..x.x..");
    strcpy(vgrid[3], "..x.x..x.x..");
    strcpy(vgrid[4], "..x.x..x.x..");
    strcpy(vgrid[5], "..x.x..x.x..");
    strcpy(vgrid[6], "..x.x1.x.x..");
    strcpy(vgrid[7], "..x.x..x.x..");
    strcpy(vgrid[8], "..x.x..x.x..");
    strcpy(vgrid[9], "..x.x..x.x..");
    strcpy(vgrid[10], ".xx.x.xx.xx.");
    strcpy(vgrid[11], "............");

    mons_array[0] = MONS_PANDEMONIUM_DEMON;
    mons_array[1] = RANDOM_MONSTER;
    mons_array[2] = RANDOM_MONSTER;
    mons_array[3] = RANDOM_MONSTER;
    mons_array[4] = RANDOM_MONSTER;
    mons_array[5] = RANDOM_MONSTER;

    return MAP_NORTH;
}


static char rand_demon_2(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".xxxxxxxx3x.");
    strcpy(vgrid[2], ".3.....xx.x.");
    strcpy(vgrid[3], ".xxxxxx4x.x.");
    strcpy(vgrid[4], ".xx4x..xx.x.");
    strcpy(vgrid[5], ".x.x.22.x.x.");
    strcpy(vgrid[6], ".x.x.12.x.x.");
    strcpy(vgrid[7], ".x.xx..x4xx.");
    strcpy(vgrid[8], ".x.x4xxxxxx.");
    strcpy(vgrid[9], ".x.xx.....3.");
    strcpy(vgrid[10], ".x3xxxxxxxx.");
    strcpy(vgrid[11], "............");

    mons_array[0] = MONS_PANDEMONIUM_DEMON;
    mons_array[1] = summon_any_demon(DEMON_GREATER);
    mons_array[2] = summon_any_demon(DEMON_COMMON);
    mons_array[3] = summon_any_demon(DEMON_COMMON);
    mons_array[4] = RANDOM_MONSTER;
    mons_array[5] = RANDOM_MONSTER;

    return MAP_NORTH;
}


static char rand_demon_3(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".x.x.x3x.x..");
    strcpy(vgrid[2], "..x.x3x3x.x.");
    strcpy(vgrid[3], ".x.x.x2x.x..");
    strcpy(vgrid[4], "..x3x2x2x3x.");
    strcpy(vgrid[5], ".x3x2x1x2x3.");
    strcpy(vgrid[6], "..x3x2x2x3x.");
    strcpy(vgrid[7], ".x.x.x2x3x..");
    strcpy(vgrid[8], "..x.x3x3x.x.");
    strcpy(vgrid[9], ".x.x.x3x.x..");
    strcpy(vgrid[10], "..x.x.x.x.x.");
    strcpy(vgrid[11], "............");

    mons_array[0] = MONS_PANDEMONIUM_DEMON;
    mons_array[1] = summon_any_demon(DEMON_COMMON);
    mons_array[2] = summon_any_demon(DEMON_COMMON);

    return MAP_NORTH;
}


static char rand_demon_4(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{
  //jmf: all 3s below were 1s -- may have been bug
    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".xxxxxxxxx..");
    strcpy(vgrid[2], ".x$=*=3=|x..");
    strcpy(vgrid[3], ".xxxxxxx=x..");
    strcpy(vgrid[4], ".x2=3=2x|x..");
    strcpy(vgrid[5], ".x=xxxxx=x..");
    strcpy(vgrid[6], ".x3=*x1=Px..");
    strcpy(vgrid[7], ".x=x=xxxxx..");
    strcpy(vgrid[8], ".x*x2=3=2=..");
    strcpy(vgrid[9], ".xxxxxxxxx..");
    strcpy(vgrid[10], "............");
    strcpy(vgrid[11], "............");

    mons_array[0] = MONS_PANDEMONIUM_DEMON;
    mons_array[1] = summon_any_demon(random2(3));
    mons_array[2] = summon_any_demon(random2(3));

    return MAP_NORTH;

}


static char rand_demon_5(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{     // obviously possible to get stuck - too bad (should've come prepared)

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], "...xxxxxx...");
    strcpy(vgrid[2], "..xx....xx..");
    strcpy(vgrid[3], ".xx......xx.");
    strcpy(vgrid[4], ".x..3232..x.");
    strcpy(vgrid[5], ".x..2|P3..x.");
    strcpy(vgrid[6], ".x..3P|2..x.");
    strcpy(vgrid[7], ".x..2123..x.");
    strcpy(vgrid[8], ".xx......xx.");
    strcpy(vgrid[9], "..xx....xx..");
    strcpy(vgrid[10], "...xxxxxx...");
    strcpy(vgrid[11], "............");

    mons_array[0] = MONS_PANDEMONIUM_DEMON;
    mons_array[1] = summon_any_demon(random2(3));
    mons_array[2] = summon_any_demon(random2(3));

    return MAP_NORTH;

}


static char rand_demon_6(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], "............");
    strcpy(vgrid[2], "......2.....");
    strcpy(vgrid[3], "............");
    strcpy(vgrid[4], ".3..........");
    strcpy(vgrid[5], "..........2.");
    strcpy(vgrid[6], ".....1......");
    strcpy(vgrid[7], "............");
    strcpy(vgrid[8], "............");
    strcpy(vgrid[9], ".2.......3..");
    strcpy(vgrid[10], "............");
    strcpy(vgrid[11], "............");

    mons_array[0] = MONS_PANDEMONIUM_DEMON;
    mons_array[1] = summon_any_demon(random2(3));
    mons_array[2] = summon_any_demon(random2(3));

    return MAP_NORTH;

}


static char rand_demon_7(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".xxx....xxx.");
    strcpy(vgrid[2], ".x|xx=xxx|x.");
    strcpy(vgrid[3], ".xx=....=xx.");
    strcpy(vgrid[4], "..x.x==x.x..");
    strcpy(vgrid[5], "..x.=12=.=..");
    strcpy(vgrid[6], "..=.=23=.x..");
    strcpy(vgrid[7], "..x.x==x.x..");
    strcpy(vgrid[8], ".xx=....=xx.");
    strcpy(vgrid[9], ".x|xxx=xx|x.");
    strcpy(vgrid[10], ".xxx....xxx.");
    strcpy(vgrid[11], "............");

    mons_array[0] = MONS_PANDEMONIUM_DEMON;
    mons_array[1] = summon_any_demon(random2(3));
    mons_array[2] = summon_any_demon(DEMON_GREATER);

    return MAP_NORTH;

}


static char rand_demon_8(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], "....xxxxxxx.");
    strcpy(vgrid[2], "..xxx....1x.");
    strcpy(vgrid[3], ".xx..2....x.");
    strcpy(vgrid[4], ".x........x.");
    strcpy(vgrid[5], ".xx.......x.");
    strcpy(vgrid[6], "..xx33..2.x.");
    strcpy(vgrid[7], "....33...xx.");
    strcpy(vgrid[8], ".....x...x..");
    strcpy(vgrid[9], "..F..xx.xx..");
    strcpy(vgrid[10], "......xxx...");
    strcpy(vgrid[11], "............");

    mons_array[0] = MONS_PANDEMONIUM_DEMON;
    mons_array[1] = summon_any_demon(DEMON_GREATER);
    mons_array[2] = summon_any_demon(random2(3));

    return MAP_NORTH;

}


static char rand_demon_9(char vgrid[81][81], FixedVector<int, 7>& mons_array)
{

    strcpy(vgrid[0], "............");
    strcpy(vgrid[1], ".xxxxxxxxxx.");
    strcpy(vgrid[2], ".x2=3=3=3xx.");
    strcpy(vgrid[3], ".x=xxxxxx2x.");
    strcpy(vgrid[4], ".x3x^^^^x=x.");
    strcpy(vgrid[5], ".x=x^P^^x2x.");
    strcpy(vgrid[6], ".x3x^^1^x=x.");
    strcpy(vgrid[7], ".x=x^^^^x3x.");
    strcpy(vgrid[8], ".x2xxxx=x=x.");
    strcpy(vgrid[9], ".xx2=2=3x3x.");
    strcpy(vgrid[10], ".xxxxxxxx=x.");
    strcpy(vgrid[11], "............");

    mons_array[0] = MONS_PANDEMONIUM_DEMON;
    mons_array[1] = summon_any_demon(random2(3));
    mons_array[2] = summon_any_demon(DEMON_GREATER);

    return MAP_NORTH;

}
