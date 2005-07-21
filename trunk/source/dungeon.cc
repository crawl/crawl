/*
 *  File:       dungeon.cc
 *  Summary:    Functions used when building new levels.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *
 *   <9>     07-Aug-2001 MV     clean up of give_item; distribution of
 *                              wands, potions and scrolls
 *                              underground rivers and lakes
 *   <8>     02-Apr-2001 gdl    cleanup; nuked all globals
 *   <7>     06-Mar-2000 bwr    reduced vorpal weapon freq,
 *                              spellbooks now hold up to eight spells.
 *   <6>     11/06/99    cdl    random3 -> random2
 *   <5>      8/08/99    BWR    Upped rarity of unique artefacts
 *   <4>      7/13/99    BWR    Made pole arms of speed.
 *   <3>      5/22/99    BWR    Made named artefact weapons
 *                              rarer, Sword of Power esp.
 *   <2>      5/09/99    LRH    Replaced some sanity checking code in
 *                              spellbook_template with a corrected version
 *                                              using ASSERTs.
 *   <1>      -/--/--    LRH    Created
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "AppHdr.h"
#include "abyss.h"
#include "defines.h"
#include "enum.h"
#include "externs.h"
#include "dungeon.h"
#include "itemname.h"
#include "items.h"
#include "maps.h"
#include "mon-util.h"
#include "mon-pick.h"
#include "monplace.h"
#include "player.h"
#include "randart.h"
#include "spl-book.h"
#include "stuff.h"
#include "wpn-misc.h"

struct spec_t {
    bool created;
    bool hooked_up;
    int x1;
    int y1;
    int x2;
    int y2;
};

typedef struct spec_t spec_room;

// DUNGEON BUILDERS
static bool find_in_area(int sx, int sy, int ex, int ey, unsigned char feature);
static bool make_box(int room_x1, int room_y1, int room_x2, int room_y2,
    unsigned char floor=0, unsigned char wall=0, unsigned char avoid=0);
static void replace_area(int sx, int sy, int ex, int ey, unsigned char replace,
    unsigned char feature);
static int builder_by_type(int level_number, char level_type);
static int builder_by_branch(int level_number);
static int builder_normal(int level_number, char level_type, spec_room &s);
static int builder_basic(int level_number);
static void builder_extras(int level_number, int level_type);
static void builder_items(int level_number, char level_type, int items_wanted);
static void builder_monsters(int level_number, char level_type, int mon_wanted);
static void place_specific_stair(unsigned char stair);
static void place_branch_entrances(int dlevel, char level_type);
static bool place_specific_trap(unsigned char spec_x, unsigned char spec_y,
    unsigned char spec_type);
static void place_traps( int level_number );
static void prepare_swamp(void);
static void prepare_water( int level_number );
static void check_doors(void);
static void hide_doors(void);
static void make_trail(int xs, int xr, int ys, int yr,int corrlength, int intersect_chance,
    int no_corr, unsigned char begin, unsigned char end=0);
static bool make_room(int sx,int sy,int ex,int ey,int max_doors, int doorlevel);
static void join_the_dots(unsigned char dotx1, unsigned char doty1,
    unsigned char dotx2, unsigned char doty2, char forbid_x1, char forbid_y1,
    char forbid_x2, char forbid_y2);
static void place_pool(unsigned char pool_type, unsigned char pool_x1,
                       unsigned char pool_y1, unsigned char pool_x2,
                       unsigned char pool_y2);
static void many_pools(unsigned char pool_type);

#ifdef USE_RIVERS
static void build_river(unsigned char river_type); //mv
static void build_lake(unsigned char lake_type); //mv
#endif // USE_RIVERS

static void spotty_level(bool seeded, int iterations, bool boxy);
static void bigger_room(void);
static void plan_main(int level_number, char force_plan);
static char plan_1(void);
static char plan_2(void);
static char plan_3(void);
static char plan_4(char forbid_x1, char forbid_y1, char forbid_x2,
                   char forbid_y2, unsigned char force_wall);
static char plan_5(void);
static char plan_6(int level_number);
static bool octa_room(spec_room &sr, int oblique_max, unsigned char type_floor);
static void labyrinth_level(int level_number);
static void box_room(int bx1, int bx2, int by1, int by2, int wall_type);
static int box_room_doors( int bx1, int bx2, int by1, int by2, int new_doors);
static void city_level(int level_number);
static void diamond_rooms(int level_number);

// ITEM & SHOP FUNCTIONS
static void place_shops(int level_number);
static void place_spec_shop(int level_number, unsigned char shop_x,
    unsigned char shop_y, unsigned char force_s_type);
static unsigned char item_in_shop(unsigned char shop_type);
static bool treasure_area(int level_number, unsigned char ta1_x,
                          unsigned char ta2_x, unsigned char ta1_y,
                          unsigned char ta2_y);
static char rare_weapon(unsigned char w_type);
static bool is_weapon_special(int the_weapon);
static void set_weapon_special(int the_weapon, int spwpn);
static void big_room(int level_number);
static void chequerboard(spec_room &sr, unsigned char
    target,  unsigned char floor1, unsigned char floor2);
static void roguey_level(int level_number, spec_room &sr);
static void morgue(spec_room &sr);

// SPECIAL ROOM BUILDERS
static void special_room(int level_number, spec_room &sr);
static void specr_2(spec_room &sr);
static void beehive(spec_room &sr);

// VAULT FUNCTIONS
static void build_vaults(int level_number, int force_vault);
static void build_minivaults(int level_number, int force_vault);
static int vault_grid( int level_number, int vx, int vy, int altar_count,
                       FixedVector < char, 7 > &acq_item_class, 
                       FixedVector < int, 7 > &mons_array,
                       char vgrid, int &initial_x, int &initial_y, 
                       int force_vault, int &num_runes );

// ALTAR FUNCTIONS
static int pick_an_altar(void);
static void place_altar(void);


/*
 **************************************************
 *                                                *
 *             BEGIN PUBLIC FUNCTIONS             *
 *                                                *
 **************************************************
*/

void builder(int level_number, char level_type)
{
    int i;          // generic loop variable
    int x,y;        // generic map loop variables

    srandom(time(NULL));

    // blank level with DNGN_ROCK_WALL
    make_box(0,0,GXM-1,GYM-1,DNGN_ROCK_WALL,DNGN_ROCK_WALL);

    // delete all traps
    for (i = 0; i < MAX_TRAPS; i++)
        env.trap[i].type = TRAP_UNASSIGNED;

    // initialize all items
    for (i = 0; i < MAX_ITEMS; i++)
        init_item( i );

    // reset all monsters
    for (i = 0; i < MAX_MONSTERS; i++)
        menv[i].type = -1;

    // unlink all monsters and items from the grid
    for(x=0; x<GXM; x++)
    {
        for(y=0; y<GYM; y++)
        {
            mgrd[x][y] = NON_MONSTER;
            igrd[x][y] = NON_ITEM;
        }
    }

    // reset all shops
    for (unsigned char shcount = 0; shcount < 5; shcount++)
    {
        env.shop[shcount].type = SHOP_UNASSIGNED;
    }

    int skip_build;

    skip_build = builder_by_type(level_number, level_type);
    if (skip_build < 0)
        return;

    if (skip_build == 0)
    {
        skip_build = builder_by_branch(level_number);

        if (skip_build < 0)
            return;
    }

    spec_room sr = { false, false, 0, 0, 0, 0 };

    if (skip_build == 0)
    {
        // do 'normal' building.  Well, except for the swamp.
        if (!player_in_branch( BRANCH_SWAMP ))
            skip_build = builder_normal(level_number, level_type, sr);

        if (skip_build == 0)
        {
            skip_build = builder_basic(level_number);
            if (skip_build == 0)
                builder_extras(level_number, level_type);
        }
    }

    // hook up the special room (if there is one,  and it hasn't
    // been hooked up already in roguey_level()
    if (sr.created && !sr.hooked_up)
        specr_2(sr);

    // now place items, monster, gates, etc.
    // stairs must exist by this point. Some items and monsters
    // already exist.

    // time to make the swamp {dlb}:
    if (player_in_branch( BRANCH_SWAMP ))
        prepare_swamp();

    // figure out how many 'normal' monsters we should place
    int mon_wanted = 0;
    if (level_type == LEVEL_ABYSS
        || player_in_branch( BRANCH_ECUMENICAL_TEMPLE ))
    {
        mon_wanted = 0;
    }
    else
    {
        mon_wanted = roll_dice( 3, 10 );

        if (player_in_hell())
            mon_wanted += roll_dice( 3, 8 );
        else if (player_in_branch( BRANCH_HALL_OF_BLADES ))
            mon_wanted += roll_dice( 6, 8 );

        // unlikely - now only possible in HoB {dlb} 10mar2000
        if (mon_wanted > 60)
            mon_wanted = 60;
    }

    place_branch_entrances( level_number, level_type );

    check_doors();

    if (!player_in_branch( BRANCH_DIS ) && !player_in_branch( BRANCH_VAULTS ))
        hide_doors();

    if (!player_in_branch( BRANCH_ECUMENICAL_TEMPLE ))
        place_traps(level_number);

    int items_wanted = 3 + roll_dice( 3, 11 );

    if (level_number > 5 && one_chance_in(500 - 5 * level_number))
        items_wanted = 10 + random2avg( 90, 2 );  // rich level!

    // change pre-rock (105) to rock,  and pre-floor (106) to floor
    replace_area( 0,0,GXM-1,GYM-1, DNGN_BUILDER_SPECIAL_WALL, DNGN_ROCK_WALL );
    replace_area( 0,0,GXM-1,GYM-1, DNGN_BUILDER_SPECIAL_FLOOR, DNGN_FLOOR );

    // place items
    builder_items(level_number, level_type, items_wanted);

    // place monsters
    builder_monsters(level_number, level_type, mon_wanted);

    // place shops,  if appropriate
    if (player_in_branch( BRANCH_MAIN_DUNGEON )
         || player_in_branch( BRANCH_ORCISH_MINES )
         || player_in_branch( BRANCH_ELVEN_HALLS )
         || player_in_branch( BRANCH_LAIR )
         || player_in_branch( BRANCH_VAULTS )
         || player_in_branch( BRANCH_SNAKE_PIT )
         || player_in_branch( BRANCH_SWAMP ))
    {
        place_shops(level_number);
    }

    // If level part of Dis -> all walls metal;
    // If part of vaults -> walls depend on level;
    // If part of crypt -> all walls stone:
    if (player_in_branch( BRANCH_DIS )
        || player_in_branch( BRANCH_VAULTS )
        || player_in_branch( BRANCH_CRYPT ))
    {
        // always the case with Dis {dlb}
        unsigned char vault_wall = DNGN_METAL_WALL;

        if (player_in_branch( BRANCH_VAULTS ))
        {
            vault_wall = DNGN_ROCK_WALL;

            if (level_number > you.branch_stairs[STAIRS_VAULTS] + 2)
                vault_wall = DNGN_STONE_WALL;

            if (level_number > you.branch_stairs[STAIRS_VAULTS] + 4)
                vault_wall = DNGN_METAL_WALL;

            if (level_number > you.branch_stairs[STAIRS_VAULTS] + 6
                && one_chance_in(10))
            {
                vault_wall = DNGN_GREEN_CRYSTAL_WALL;
            }
        }
        else if (player_in_branch( BRANCH_CRYPT ))
        {
            vault_wall = DNGN_STONE_WALL;
        }

        replace_area(0,0,GXM-1,GYM-1,DNGN_ROCK_WALL,vault_wall);
    }

    // Top level of branch levels - replaces up stairs
    // with stairs back to dungeon or wherever:
    for (i = 0; i< 30; i++)
    {
        if (you.branch_stairs[i] == 0)
            break;

        if (level_number == you.branch_stairs[i] + 1
            && level_type == LEVEL_DUNGEON
            && you.where_are_you == BRANCH_ORCISH_MINES + i)
        {
            for (x = 1; x < GXM; x++)
            {
                for (y = 1; y < GYM; y++)
                {
                    if (grd[x][y] >= DNGN_STONE_STAIRS_UP_I
                        && grd[x][y] <= DNGN_ROCK_STAIRS_UP)
                    {
                        grd[x][y] = DNGN_RETURN_FROM_ORCISH_MINES + i;
                    }
                }
            }
        }
    }

    // bottom level of branch - replaces down stairs with up ladders:
    for (i = 0; i < 30; i++)
    {
        if (level_number == you.branch_stairs[i] + branch_depth(i)
            && level_type == LEVEL_DUNGEON
            && you.where_are_you == BRANCH_ORCISH_MINES + i)
        {
            for (x = 1; x < GXM; x++)
            {
                for (y = 1; y < GYM; y++)
                {
                    if (grd[x][y] >= DNGN_STONE_STAIRS_DOWN_I
                        && grd[x][y] <= DNGN_ROCK_STAIRS_DOWN)
                    { 
                        grd[x][y] = DNGN_ROCK_STAIRS_UP;
                    }
                }
            }
        }
    }

    if (player_in_branch( BRANCH_CRYPT ))
    {
        if (one_chance_in(3))
            mons_place( MONS_CURSE_SKULL, BEH_SLEEP, MHITNOT, false, 0, 0 );

        if (one_chance_in(7))
            mons_place( MONS_CURSE_SKULL, BEH_SLEEP, MHITNOT, false, 0, 0 );
    }

    if (player_in_branch( BRANCH_ORCISH_MINES ) && one_chance_in(5))
        place_altar();

    // hall of blades (1 level deal) - no down staircases, thanks!
    if (player_in_branch( BRANCH_HALL_OF_BLADES ))
    {
        for (x = 1; x < GXM; x++)
        {
            for (y = 1; y < GYM; y++)
            {
                if (grd[x][y] >= DNGN_STONE_STAIRS_DOWN_I
                    && grd[x][y] <= DNGN_ROCK_STAIRS_UP)
                {
                    grd[x][y] = DNGN_FLOOR;
                }
            }
        }
    }

    link_items();

    if (!player_in_branch(BRANCH_COCYTUS) && !player_in_branch(BRANCH_SWAMP))
        prepare_water( level_number );
}                               // end builder()

// Returns item slot or NON_ITEM if it fails
int items( int allow_uniques,       // not just true-false,
                                    //     because of BCR acquirement hack
           int force_class,         // desired OBJECTS class {dlb}
           int force_type,          // desired SUBTYPE - enum varies by OBJ
           bool dont_place,         // don't randomly place item on level
           int item_level,          // level of the item, can differ from global
           int item_race )          // weapon / armour racial categories
                                    // item_race also gives type of rune!
{
    int temp_rand = 0;             // probability determination {dlb}
    int range_charges = 0;         // for OBJ_WANDS charge count {dlb}
    int temp_value = 0;            // temporary value storage {dlb}
    int loopy = 0;                 // just another loop variable {dlb}
    int count = 0;                 // just another loop variable {dlb}

    int race_plus = 0;
    int race_plus2 = 0;
    int x_pos, y_pos;

    int quant = 0;

    FixedVector < int, SPELLBOOK_SIZE > fpass;
    int icky = 0;
    int p = 0;

    // find an emtpy slot for the item (with culling if required)
    p = get_item_slot(10);
    if (p == NON_ITEM)
        return (NON_ITEM);

    // clear all properties except mitm.base_type <used in switch below> {dlb}:
    mitm[p].sub_type = 0;
    mitm[p].flags = 0;
    mitm[p].special = 0;
    mitm[p].plus = 0;
    mitm[p].plus2 = 0;
    mitm[p].x = 0;
    mitm[p].y = 0;
    mitm[p].link = NON_ITEM;

    // cap item_level unless an acquirement-level item {dlb}:
    if (item_level > 50 && item_level != MAKE_GOOD_ITEM)
        item_level = 50;

    // determine base_type for item generated {dlb}:
    if (force_class != OBJ_RANDOM)
        mitm[p].base_type = force_class;
    else
    {
        // nice and large for subtle differences {dlb}
        temp_rand = random2(10000);

        mitm[p].base_type =  ((temp_rand <   50) ? OBJ_STAVES :   //  0.50%
                              (temp_rand <  200) ? OBJ_BOOKS :    //  1.50%
                              (temp_rand <  450) ? OBJ_JEWELLERY ://  2.50%
                              (temp_rand <  800) ? OBJ_WANDS :    //  3.50%
                              (temp_rand < 1500) ? OBJ_FOOD :     //  7.00%
                              (temp_rand < 2500) ? OBJ_ARMOUR :   // 10.00%
                              (temp_rand < 3500) ? OBJ_WEAPONS :  // 10.00%
                              (temp_rand < 4500) ? OBJ_POTIONS :  // 10.00%
                              (temp_rand < 6000) ? OBJ_MISSILES : // 15.00%
                              (temp_rand < 8000) ? OBJ_SCROLLS    // 20.00%
                                                 : OBJ_GOLD);     // 20.00%

        // misc items placement wholly dependent upon current depth {dlb}:
        if (item_level > 7 && (20 + item_level) >= random2(3500))
            mitm[p].base_type = OBJ_MISCELLANY;

        if (item_level < 7 
            && (mitm[p].base_type == OBJ_BOOKS 
                || mitm[p].base_type == OBJ_STAVES
                || mitm[p].base_type == OBJ_WANDS)
            && random2(7) >= item_level)
        {
            mitm[p].base_type = coinflip() ? OBJ_POTIONS : OBJ_SCROLLS;
        }
    }

    // determine sub_type accordingly {dlb}:
    switch (mitm[p].base_type)
    {
    case OBJ_WEAPONS:
        // generate initial weapon subtype using weighted function --
        // indefinite loop now more evident and fewer array lookups {dlb}:
        if (force_type != OBJ_RANDOM)
            mitm[p].sub_type = force_type;
        else 
        {
            if (random2(20) < 20 - item_level)
            {
                // these are the common/low level weapon types
                temp_rand = random2(12);

                mitm[p].sub_type = ((temp_rand ==  0) ? WPN_KNIFE         :
                                    (temp_rand ==  1) ? WPN_QUARTERSTAFF  :
                                    (temp_rand ==  2) ? WPN_SLING         :
                                    (temp_rand ==  3) ? WPN_SPEAR         :
                                    (temp_rand ==  4) ? WPN_HAND_AXE      :
                                    (temp_rand ==  5) ? WPN_DAGGER        :
                                    (temp_rand ==  6) ? WPN_MACE          :
                                    (temp_rand ==  7) ? WPN_DAGGER        :
                                    (temp_rand ==  8) ? WPN_CLUB          :
                                    (temp_rand ==  9) ? WPN_HAMMER        :
                                    (temp_rand == 10) ? WPN_WHIP
                                                      : WPN_SABRE);
            }
            else if (item_level > 6 && random2(100) < (10 + item_level)
                       && one_chance_in(30))
            {
                // place the rare_weapon() == 0 weapons
                //
                // this replaced the infinite loop (wasteful) -- may need
                // to make into its own function to allow ease of tweaking
                // distribution {dlb}:
                temp_rand = random2(9);

                mitm[p].sub_type = ((temp_rand == 8) ? WPN_DEMON_BLADE :
                                    (temp_rand == 7) ? WPN_DEMON_TRIDENT :
                                    (temp_rand == 6) ? WPN_DEMON_WHIP :
                                    (temp_rand == 5) ? WPN_DOUBLE_SWORD :
                                    (temp_rand == 4) ? WPN_EVENINGSTAR :
                                    (temp_rand == 3) ? WPN_EXECUTIONERS_AXE :
                                    (temp_rand == 2) ? WPN_KATANA :
                                    (temp_rand == 1) ? WPN_QUICK_BLADE
                                 /*(temp_rand == 0)*/: WPN_TRIPLE_SWORD);
            }
            else
            {
                // pick a weapon based on rarity 
                for (;;)
                {
                    temp_value = (unsigned char) random2(NUM_WEAPONS);

                    if (rare_weapon(temp_value) >= random2(10) + 1)
                    {
                        mitm[p].sub_type = temp_value;
                        break;
                    }
                }
            }
        }

        if (allow_uniques)
        {
            // Note there is nothing to stop randarts being reproduced,
            // except vast improbability.
            if (mitm[p].sub_type != WPN_CLUB && item_level > 2
                && random2(2000) <= 100 + (item_level * 3) && coinflip())
            {
                if (you.level_type != LEVEL_ABYSS
                    && you.level_type != LEVEL_PANDEMONIUM
                    && one_chance_in(50))
                {
                    icky = find_okay_unrandart( OBJ_WEAPONS, force_type );

                    if (icky != -1)
                    {
                        quant = 1;
                        make_item_unrandart( mitm[p], icky );
                        break;
                    }
                }

                make_item_randart( mitm[p] );
                mitm[p].plus = 0;
                mitm[p].plus2 = 0;
                mitm[p].plus += random2(7);
                mitm[p].plus2 += random2(7);

                if (one_chance_in(3))
                    mitm[p].plus += random2(7);

                if (one_chance_in(3))
                    mitm[p].plus2 += random2(7);

                if (one_chance_in(9))
                    mitm[p].plus -= random2(7);

                if (one_chance_in(9))
                    mitm[p].plus2 -= random2(7);

                quant = 1;

                if (one_chance_in(4))
                {
                    do_curse_item( mitm[p] );
                    mitm[p].plus  = -random2(6);
                    mitm[p].plus2 = -random2(6);
                }
                else if ((mitm[p].plus < 0 || mitm[p].plus2 < 0)
                         && !one_chance_in(3))
                {
                    do_curse_item( mitm[p] );
                }
                break;
            }

            if (item_level > 6
                && random2(3000) <= 30 + (item_level * 3) && one_chance_in(12))
            {
                if (make_item_fixed_artefact( mitm[p], (item_level == 51) ))
                    break;
            }
        }

        ASSERT(!is_fixed_artefact(mitm[p]) && !is_random_artefact(mitm[p]));

        if (item_level == MAKE_GOOD_ITEM 
            && force_type != OBJ_RANDOM
            && (mitm[p].sub_type == WPN_CLUB || mitm[p].sub_type == WPN_SLING))
        {
            mitm[p].sub_type = WPN_LONG_SWORD;
        }

        quant = 1;

        mitm[p].plus = 0;
        mitm[p].plus2 = 0;
        mitm[p].special = SPWPN_NORMAL;

        if (item_race == MAKE_ITEM_RANDOM_RACE && coinflip())
        {
            switch (mitm[p].sub_type)
            {
            case WPN_CLUB:
                if (coinflip())
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                break;

            case WPN_MACE:
            case WPN_FLAIL:
            case WPN_SPIKED_FLAIL:
            case WPN_GREAT_MACE:
            case WPN_GREAT_FLAIL:
                if (one_chance_in(6))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                break;

            case WPN_MORNINGSTAR:
            case WPN_HAMMER:
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                break;

            case WPN_DAGGER:
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case WPN_SHORT_SWORD:
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case WPN_FALCHION:
                if (one_chance_in(5))
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case WPN_LONG_SWORD:
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (coinflip())
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case WPN_GREAT_SWORD:
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                break;

            case WPN_SCIMITAR:
                if (coinflip())
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                break;

            case WPN_WAR_AXE:
            case WPN_HAND_AXE:
            case WPN_BROAD_AXE:
            case WPN_BATTLEAXE:
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (coinflip())
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                break;

            case WPN_SPEAR:
            case WPN_TRIDENT:
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case WPN_HALBERD:
            case WPN_GLAIVE:
            case WPN_EXECUTIONERS_AXE:
                if (one_chance_in(5))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                break;

            case WPN_QUICK_BLADE:
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case WPN_KATANA:
            case WPN_KNIFE:
            case WPN_SLING:
                set_equip_race( mitm[p], ISFLAG_NO_RACE );
                set_item_ego_type( mitm[p], OBJ_WEAPONS, SPWPN_NORMAL );
                break;

            case WPN_BOW:
                if (one_chance_in(6))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (coinflip())
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case WPN_CROSSBOW:
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                break;

            case WPN_HAND_CROSSBOW:
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case WPN_BLOWGUN:
                if (one_chance_in(10))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                break;
            }
        }

        // fine, but out-of-order relative to mitm[].special ordering {dlb}
        switch (item_race)
        {
        case MAKE_ITEM_ELVEN:
            set_equip_race( mitm[p], ISFLAG_ELVEN );
            break;

        case MAKE_ITEM_DWARVEN:
            set_equip_race( mitm[p], ISFLAG_DWARVEN );
            break;

        case MAKE_ITEM_ORCISH:
            set_equip_race( mitm[p], ISFLAG_ORCISH );
            break;
        }

        // if we allow acquirement-type items to be orcish, then 
        // there's a good chance that we'll just strip them of 
        // their ego type at the bottom of this function. -- bwr
        if (item_level == MAKE_GOOD_ITEM 
            && cmp_equip_race( mitm[p], ISFLAG_ORCISH ))
        {
            set_equip_race( mitm[p], ISFLAG_NO_RACE );
        }

        switch (get_equip_race( mitm[p] ))
        {
        case ISFLAG_ORCISH:
            if (coinflip())
                race_plus--;
            if (coinflip())
                race_plus2++;
            break;

        case ISFLAG_ELVEN:
            race_plus += random2(3);
            break;

        case ISFLAG_DWARVEN:
            if (coinflip())
                race_plus++;
            if (coinflip())
                race_plus2++;
            break;
        }

        mitm[p].plus  += race_plus;
        mitm[p].plus2 += race_plus2;

        if ((random2(200) <= 50 + item_level
                || item_level == MAKE_GOOD_ITEM
                || is_demonic(mitm[p].sub_type))
            // nobody would bother enchanting a club
            && mitm[p].sub_type != WPN_CLUB
            && mitm[p].sub_type != WPN_GIANT_CLUB
            && mitm[p].sub_type != WPN_GIANT_SPIKED_CLUB)
        {
            count = 0;

            do
            {
                if (random2(300) <= 100 + item_level
                    || item_level == MAKE_GOOD_ITEM
                    || is_demonic( mitm[p].sub_type ))
                {
                    // note: this doesn't guarantee special enchantment
                    switch (mitm[p].sub_type)
                    {
                    case WPN_EVENINGSTAR:
                        if (coinflip())
                            set_weapon_special(p, SPWPN_DRAINING);
                        // **** intentional fall through here ****
                    case WPN_MORNINGSTAR:
                        if (one_chance_in(4))
                            set_weapon_special(p, SPWPN_VENOM);

                        if (one_chance_in(4))
                        {
                            set_weapon_special(p, (coinflip() ? SPWPN_FLAMING
                                                              : SPWPN_FREEZING));
                        }

                        if (one_chance_in(20))
                            set_weapon_special(p, SPWPN_VAMPIRICISM);
                        // **** intentional fall through here ****
                    case WPN_MACE:
                    case WPN_GREAT_MACE:
                        if ((mitm[p].sub_type == WPN_MACE
                                || mitm[p].sub_type == WPN_GREAT_MACE)
                            && one_chance_in(4))
                        {
                            set_weapon_special(p, SPWPN_DISRUPTION);
                        }
                        // **** intentional fall through here ****
                    case WPN_FLAIL:
                    case WPN_SPIKED_FLAIL:
                    case WPN_GREAT_FLAIL:
                    case WPN_HAMMER:
                        if (one_chance_in(25))
                            set_weapon_special(p, SPWPN_PAIN);

                        if (one_chance_in(25))
                            set_weapon_special(p, SPWPN_DISTORTION);

                        if (one_chance_in(3) &&
                            (!is_weapon_special(p) || one_chance_in(5)))
                            set_weapon_special(p, SPWPN_VORPAL);

                        if (one_chance_in(4))
                            set_weapon_special(p, SPWPN_HOLY_WRATH);

                        if (one_chance_in(3))
                            set_weapon_special(p, SPWPN_PROTECTION);

                        if (one_chance_in(10))
                            set_weapon_special(p, SPWPN_DRAINING);
                        break;


                    case WPN_DAGGER:
                        if (one_chance_in(10))
                            set_weapon_special(p, SPWPN_PAIN);

                        if (one_chance_in(3))
                            set_weapon_special(p, SPWPN_VENOM);
                        // **** intentional fall through here ****

                    case WPN_SHORT_SWORD:
                    case WPN_SABRE:
                        if (one_chance_in(25))
                            set_weapon_special(p, SPWPN_DISTORTION);

                        if (one_chance_in(10))
                            set_weapon_special(p, SPWPN_VAMPIRICISM);

                        if (one_chance_in(8))
                            set_weapon_special(p, SPWPN_ELECTROCUTION);

                        if (one_chance_in(8))
                            set_weapon_special(p, SPWPN_PROTECTION);

                        if (one_chance_in(10))
                            set_weapon_special(p, SPWPN_ORC_SLAYING);

                        if (one_chance_in(8))
                        {
                            set_weapon_special(p,(coinflip() ? SPWPN_FLAMING
                                                             : SPWPN_FREEZING));
                        }

                        if (one_chance_in(12))
                            set_weapon_special(p, SPWPN_HOLY_WRATH);

                        if (one_chance_in(8))
                            set_weapon_special(p, SPWPN_DRAINING);

                        if (one_chance_in(8))
                            set_weapon_special(p, SPWPN_SPEED);

                        if (one_chance_in(6))
                            set_weapon_special(p, SPWPN_VENOM);
                        break;

                    case WPN_FALCHION:
                    case WPN_LONG_SWORD:
                        if (one_chance_in(12))
                            set_weapon_special(p, SPWPN_VENOM);
                        // **** intentional fall through here ****
                    case WPN_SCIMITAR:
                        if (one_chance_in(25))
                            set_weapon_special(p, SPWPN_PAIN);

                        if (one_chance_in(7))
                            set_weapon_special(p, SPWPN_SPEED);
                        // **** intentional fall through here ****
                    case WPN_GREAT_SWORD:
                    case WPN_DOUBLE_SWORD:
                    case WPN_TRIPLE_SWORD:
                        if (one_chance_in(10))
                            set_weapon_special(p, SPWPN_VAMPIRICISM);

                        if (one_chance_in(25))
                            set_weapon_special(p, SPWPN_DISTORTION);

                        if (one_chance_in(5))
                        {
                            set_weapon_special(p,(coinflip() ? SPWPN_FLAMING
                                                             : SPWPN_FREEZING));
                        }

                        if (one_chance_in(7))
                            set_weapon_special(p, SPWPN_PROTECTION);

                        if (one_chance_in(8))
                            set_weapon_special(p, SPWPN_ORC_SLAYING);

                        if (one_chance_in(12))
                            set_weapon_special(p, SPWPN_DRAINING);

                        if (one_chance_in(7))
                            set_weapon_special(p, SPWPN_ELECTROCUTION);

                        if (one_chance_in(4))
                            set_weapon_special(p, SPWPN_HOLY_WRATH);

                        if (one_chance_in(4)
                                && (!is_weapon_special(p) || one_chance_in(3)))
                        {
                            set_weapon_special(p, SPWPN_VORPAL);
                        }
                        break;


                    case WPN_WAR_AXE:
                    case WPN_BROAD_AXE:
                    case WPN_BATTLEAXE:
                    case WPN_EXECUTIONERS_AXE:
                        if (one_chance_in(25))
                            set_weapon_special(p, SPWPN_HOLY_WRATH);

                        if (one_chance_in(14))
                            set_weapon_special(p, SPWPN_DRAINING);
                        // **** intentional fall through here ****
                    case WPN_HAND_AXE:
                        if (one_chance_in(30))
                            set_weapon_special(p, SPWPN_PAIN);

                        if (one_chance_in(10))
                            set_weapon_special(p, SPWPN_VAMPIRICISM);

                        if (one_chance_in(25))
                            set_weapon_special(p, SPWPN_DISTORTION);

                        if (one_chance_in(3)
                                && (!is_weapon_special(p) || one_chance_in(5)))
                        {
                            set_weapon_special(p, SPWPN_VORPAL);
                        }

                        if (one_chance_in(6))
                            set_weapon_special(p, SPWPN_ORC_SLAYING);

                        if (one_chance_in(4))
                        {
                            set_weapon_special(p, (coinflip() ? SPWPN_FLAMING
                                                               : SPWPN_FREEZING));
                        }

                        if (one_chance_in(8))
                            set_weapon_special(p, SPWPN_ELECTROCUTION);

                        if (one_chance_in(12))
                            set_weapon_special(p, SPWPN_VENOM);

                        break;

                    case WPN_WHIP:
                        if (one_chance_in(20))
                            set_weapon_special(p, SPWPN_DISTORTION);

                        if (one_chance_in(6))
                        {
                            set_weapon_special(p, (coinflip() ? SPWPN_FLAMING
                                                               : SPWPN_FREEZING));
                        }

                        if (one_chance_in(6))
                            set_weapon_special(p, SPWPN_VENOM);

                        if (coinflip())
                            set_weapon_special(p, SPWPN_REACHING);

                        if (one_chance_in(5))
                            set_weapon_special(p, SPWPN_SPEED);

                        if (one_chance_in(5))
                            set_weapon_special(p, SPWPN_ELECTROCUTION);
                        break;

                    case WPN_HALBERD:
                    case WPN_GLAIVE:
                        if (one_chance_in(30))
                            set_weapon_special(p, SPWPN_HOLY_WRATH);

                        if (one_chance_in(4))
                            set_weapon_special(p, SPWPN_PROTECTION);
                        // **** intentional fall through here ****
                    case WPN_SCYTHE:
                    case WPN_TRIDENT:
                        if (one_chance_in(5))
                            set_weapon_special(p, SPWPN_SPEED);
                        // **** intentional fall through here ****
                    case WPN_SPEAR:
                        if (one_chance_in(25))
                            set_weapon_special(p, SPWPN_PAIN);

                        if (one_chance_in(10))
                            set_weapon_special(p, SPWPN_VAMPIRICISM);

                        if (one_chance_in(20))
                            set_weapon_special(p, SPWPN_DISTORTION);

                        if (one_chance_in(5) &&
                            (!is_weapon_special(p) || one_chance_in(6)))
                            set_weapon_special(p, SPWPN_VORPAL);

                        if (one_chance_in(6))
                            set_weapon_special(p, SPWPN_ORC_SLAYING);

                        if (one_chance_in(6))
                        {
                            set_weapon_special(p, (coinflip() ? SPWPN_FLAMING
                                                              : SPWPN_FREEZING));
                        }

                        if (one_chance_in(6))
                            set_weapon_special(p, SPWPN_VENOM);

                        if (one_chance_in(3))
                            set_weapon_special(p, SPWPN_REACHING);
                        break;


                    case WPN_SLING:
                    case WPN_HAND_CROSSBOW:
                        if (coinflip())
                            break;
                        // **** possible intentional fall through here ****
                    case WPN_BOW:
                    case WPN_CROSSBOW:
                        if (one_chance_in(30))
                            set_weapon_special( p, SPWPN_SPEED );

                        if (one_chance_in(5))
                            set_weapon_special( p, SPWPN_PROTECTION );

                        if (coinflip())
                            set_weapon_special(p, (coinflip() ? SPWPN_FLAME
                                                              : SPWPN_FROST));
                        break;

                    case WPN_BLOWGUN:
                        if (one_chance_in(7))
                            set_weapon_special(p, SPWPN_VENOM);
                        break;

                    // quarterstaff - not powerful, as this would make
                    // the 'staves' skill just too good
                    case WPN_QUARTERSTAFF:
                        if (one_chance_in(30))
                            set_weapon_special(p, SPWPN_PAIN);

                        if (one_chance_in(20))
                            set_weapon_special(p, SPWPN_DISTORTION);

                        if (one_chance_in(5))
                            set_weapon_special(p, SPWPN_SPEED);

                        if (one_chance_in(10))
                            set_weapon_special(p, SPWPN_VORPAL);

                        if (one_chance_in(5))
                            set_weapon_special(p, SPWPN_PROTECTION);
                        break;


                    case WPN_DEMON_TRIDENT:
                    case WPN_DEMON_WHIP:
                    case WPN_DEMON_BLADE:
                        set_equip_race( mitm[p], ISFLAG_NO_RACE );

                        if (one_chance_in(10))
                            set_weapon_special(p, SPWPN_PAIN);

                        if (one_chance_in(3)
                            && (mitm[p].sub_type == WPN_DEMON_WHIP
                                || mitm[p].sub_type == WPN_DEMON_TRIDENT))
                        {
                            set_weapon_special(p, SPWPN_REACHING);
                        }

                        if (one_chance_in(5))
                            set_weapon_special(p, SPWPN_DRAINING);

                        if (one_chance_in(5))
                        {
                            set_weapon_special(p, (coinflip() ? SPWPN_FLAMING
                                                              : SPWPN_FREEZING));
                        }

                        if (one_chance_in(5))
                            set_weapon_special(p, SPWPN_ELECTROCUTION);

                        if (one_chance_in(5))
                            set_weapon_special(p, SPWPN_VAMPIRICISM);

                        if (one_chance_in(5))
                            set_weapon_special(p, SPWPN_VENOM);
                        break;

                    // unlisted weapons have no associated, standard ego-types {dlb}
                    default:
                        break;
                    }
                }                   // end if specially enchanted

                count++;
            }
            while (item_level == MAKE_GOOD_ITEM 
                    && mitm[p].special == SPWPN_NORMAL 
                    && count < 5);

            // if acquired item still not ego... enchant it up a bit.
            if (item_level == MAKE_GOOD_ITEM && mitm[p].special == SPWPN_NORMAL)
            {
                mitm[p].plus  += 2 + random2(3);
                mitm[p].plus2 += 2 + random2(3);
            }

            const int chance = (item_level == MAKE_GOOD_ITEM) ? 200 
                                                              : item_level; 

            // odd-looking, but this is how the algorithm compacts {dlb}:
            for (loopy = 0; loopy < 4; loopy++)
            {
                mitm[p].plus += random2(3);

                if (random2(350) > 20 + chance)
                    break;
            }

            // odd-looking, but this is how the algorithm compacts {dlb}:
            for (loopy = 0; loopy < 4; loopy++)
            {
                mitm[p].plus2 += random2(3);

                if (random2(500) > 50 + chance)
                    break;
            }
        }
        else
        {
            if (one_chance_in(12))
            {
                do_curse_item( mitm[p] );
                mitm[p].plus  -= random2(4);
                mitm[p].plus2 -= random2(4);

                // clear specials {dlb}
                set_item_ego_type( mitm[p], OBJ_WEAPONS, SPWPN_NORMAL );
            }
        }

        // value was "0" comment said "orc" so I went with comment {dlb}
        if (cmp_equip_race( mitm[p], ISFLAG_ORCISH ))
        {
            // no holy wrath or slay orc and 1/2 the time no-ego
            const int brand = get_weapon_brand( mitm[p] );
            if (brand == SPWPN_HOLY_WRATH
                || brand == SPWPN_ORC_SLAYING
                || (brand != SPWPN_NORMAL && coinflip()))
            {
                // this makes no sense {dlb}
                // Probably a remnant of the old code which used
                // to decrement this when the electric attack happened -- bwr
                // if (brand == SPWPN_ELECTROCUTION)
                //     mitm[p].plus = 0;

                set_item_ego_type( mitm[p], OBJ_WEAPONS, SPWPN_NORMAL );
            }
        }

        if ((((is_random_artefact( mitm[p] ) 
                        || get_weapon_brand( mitm[p] ) != SPWPN_NORMAL)
                    && !one_chance_in(10))
                || ((mitm[p].plus != 0 || mitm[p].plus2 != 0) 
                    && one_chance_in(3)))
            && mitm[p].sub_type != WPN_CLUB
            && mitm[p].sub_type != WPN_GIANT_CLUB
            && mitm[p].sub_type != WPN_GIANT_SPIKED_CLUB
            && cmp_equip_desc( mitm[p], 0 )
            && cmp_equip_race( mitm[p], 0 ))
        {
            set_equip_desc( mitm[p], (coinflip() ? ISFLAG_GLOWING 
                                                 : ISFLAG_RUNED) );
        }
        break;

    case OBJ_MISSILES:
        quant = 0;
        mitm[p].plus = 0;
        mitm[p].special = SPMSL_NORMAL;

        temp_rand = random2(20);
        mitm[p].sub_type = (temp_rand < 6)  ? MI_STONE :         // 30 %
                           (temp_rand < 10) ? MI_DART :          // 20 %
                           (temp_rand < 14) ? MI_ARROW :         // 20 %
                           (temp_rand < 18) ? MI_BOLT            // 20 %
                                            : MI_NEEDLE;         // 10 %

        if (force_type != OBJ_RANDOM)
            mitm[p].sub_type = force_type;

        // no fancy rocks -- break out before we get to racial/special stuff
        if (mitm[p].sub_type == MI_LARGE_ROCK)
        {
            quant = 2 + random2avg(5,2);
            break;
        }
        else if (mitm[p].sub_type == MI_STONE)
        {
            quant = 1 + random2(9) + random2(12) + random2(15) + random2(12);
            break;
        }

        // set racial type:
        switch (item_race)
        {
        case MAKE_ITEM_ELVEN:
            set_equip_race( mitm[p], ISFLAG_ELVEN );
            break;

        case MAKE_ITEM_DWARVEN: 
            set_equip_race( mitm[p], ISFLAG_DWARVEN );
            break;

        case MAKE_ITEM_ORCISH:
            set_equip_race( mitm[p], ISFLAG_ORCISH );
            break;

        case MAKE_ITEM_RANDOM_RACE:
            if ((mitm[p].sub_type == MI_ARROW
                    || mitm[p].sub_type == MI_DART)
                && one_chance_in(4))
            {
                // elven - not for bolts, though
                set_equip_race( mitm[p], ISFLAG_ELVEN );
            }

            if ((mitm[p].sub_type == MI_ARROW
                    || mitm[p].sub_type == MI_BOLT
                    || mitm[p].sub_type == MI_DART)
                && one_chance_in(4))
            {
                set_equip_race( mitm[p], ISFLAG_ORCISH );
            }

            if ((mitm[p].sub_type == MI_DART
                    || mitm[p].sub_type == MI_BOLT)
                && one_chance_in(6))
            {
                set_equip_race( mitm[p], ISFLAG_DWARVEN );
            }

            if (mitm[p].sub_type == MI_NEEDLE)
            {
                if (one_chance_in(10))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                if (one_chance_in(6))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
            }
            break;
        }

        // note that needles can only be poisoned
        //
        // Actually, it'd be really nice if there where
        // some paralysis or slowing poison needles, just
        // so that blowguns have some added utility over
        // the other launchers/throwing weapons. -- bwr
        if (mitm[p].sub_type == MI_NEEDLE 
            && (item_level == MAKE_GOOD_ITEM || !one_chance_in(5)))
        {
            set_item_ego_type( mitm[p], OBJ_MISSILES, SPMSL_POISONED_II );
        }
        else
        {
            // decide specials:
            if (item_level == MAKE_GOOD_ITEM)
                temp_rand = random2(150);
            else
                temp_rand = random2(2000 - 55 * item_level);

            set_item_ego_type( mitm[p], OBJ_MISSILES, 
                               (temp_rand <  60) ? SPMSL_FLAME :
                               (temp_rand < 120) ? SPMSL_ICE   :
                               (temp_rand < 150) ? SPMSL_POISONED_II
                                                 : SPMSL_NORMAL );
        }

        // orcish ammo gets poisoned a lot more often -- in the original
        // code it was poisoned every time!?
        if (cmp_equip_race( mitm[p], ISFLAG_ORCISH ) && one_chance_in(3))
            set_item_ego_type( mitm[p], OBJ_MISSILES, SPMSL_POISONED_II );

        // reduced quantity if special
        if (get_ammo_brand( mitm[p] ) != SPMSL_NORMAL )
            quant = 1 + random2(9) + random2(12) + random2(12);
        else
            quant = 1 + random2(9) + random2(12) + random2(15) + random2(12);

        if (10 + item_level >= random2(100))
            mitm[p].plus += random2(5);

        // elven arrows and dwarven bolts are quality items
        if ((cmp_equip_race( mitm[p], ISFLAG_ELVEN )
                && mitm[p].sub_type == MI_ARROW)
            || (cmp_equip_race( mitm[p], ISFLAG_DWARVEN )
                && mitm[p].sub_type == MI_BOLT))
        {
            mitm[p].plus += random2(3);
        }
        break;

    case OBJ_ARMOUR:
        quant = 1;

        mitm[p].plus = 0;
        mitm[p].plus2 = 0;
        mitm[p].special = SPARM_NORMAL;

        if (force_type != OBJ_RANDOM)
            mitm[p].sub_type = force_type;
        else
        {
            mitm[p].sub_type = random2(3);

            if (random2(35) <= item_level + 10)
            {
                mitm[p].sub_type = random2(5);
                if (one_chance_in(4))
                    mitm[p].sub_type = ARM_ANIMAL_SKIN;
            }

            if (random2(60) <= item_level + 10)
                mitm[p].sub_type = random2(8);

            if (10 + item_level >= random2(400) && one_chance_in(20))
                mitm[p].sub_type = ARM_DRAGON_HIDE + random2(7);

            if (10 + item_level >= random2(500) && one_chance_in(20))
            {
                mitm[p].sub_type = ARM_STEAM_DRAGON_HIDE + random2(11);

                if (mitm[p].sub_type == ARM_ANIMAL_SKIN && one_chance_in(20))
                    mitm[p].sub_type = ARM_CRYSTAL_PLATE_MAIL;
            }

            // secondary armours:
            if (one_chance_in(5))
            {
                mitm[p].sub_type = ARM_SHIELD + random2(5);

                if (mitm[p].sub_type == ARM_SHIELD)                 // 33.3%
                {
                    if (coinflip())
                        mitm[p].sub_type = ARM_BUCKLER;             // 50.0%
                    else if (one_chance_in(3))
                        mitm[p].sub_type = ARM_LARGE_SHIELD;        // 16.7%
                }
            }
        }

        if (mitm[p].sub_type == ARM_HELMET)
        {
            set_helmet_type( mitm[p], THELM_HELMET );
            set_helmet_desc( mitm[p], THELM_DESC_PLAIN );

            if (one_chance_in(3))
                set_helmet_type( mitm[p], random2( THELM_NUM_TYPES ) );

            if (one_chance_in(3))
                set_helmet_random_desc( mitm[p] );
        }

        if (allow_uniques == 1
            && item_level > 2
            && random2(2000) <= (100 + item_level * 3)
            && coinflip())
        {
            if ((you.level_type != LEVEL_ABYSS
                    && you.level_type != LEVEL_PANDEMONIUM)
                && one_chance_in(50))
            {
                icky = find_okay_unrandart(OBJ_ARMOUR);
                if (icky != -1)
                {
                    quant = 1;
                    make_item_unrandart( mitm[p], icky );
                    break;
                }
            }

            hide2armour( &(mitm[p].sub_type) );

            // mitm[p].special = SPARM_RANDART_II + random2(4);
            make_item_randart( mitm[p] );
            mitm[p].plus = 0;

            if (mitm[p].sub_type == ARM_BOOTS)
            {
                mitm[p].plus2 = TBOOT_BOOTS;
                if (one_chance_in(10))
                    mitm[p].plus2 = random2( NUM_BOOT_TYPES );
            }

            mitm[p].plus += random2(4);

            if (one_chance_in(5))
                mitm[p].plus += random2(4);

            if (one_chance_in(6))
                mitm[p].plus -= random2(8);

            quant = 1;

            if (one_chance_in(5))
            {
                do_curse_item( mitm[p] );
                mitm[p].plus = -random2(6);
            }
            else if (mitm[p].plus < 0 && !one_chance_in(3))
            {
                do_curse_item( mitm[p] );
            }
            break;
        }

        mitm[p].plus = 0;

        if (item_race == MAKE_ITEM_RANDOM_RACE && coinflip())
        {
            switch (mitm[p].sub_type)
            {
            case ARM_SHIELD:    // shield - must do special things for this!
            case ARM_BUCKLER:
            case ARM_LARGE_SHIELD:
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                if (one_chance_in(3))
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                break;

            case ARM_CLOAK:
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case ARM_GLOVES:
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case ARM_BOOTS:
                if (one_chance_in(4))
                {
                    mitm[p].plus2 = TBOOT_NAGA_BARDING;
                    break;      
                }

                if (one_chance_in(4))
                {
                    mitm[p].plus2 = TBOOT_CENTAUR_BARDING;
                    break; 
                }

                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                if (one_chance_in(6))
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                break;

            case ARM_HELMET:
                if (cmp_helmet_type( mitm[p], THELM_CAP )
                    || cmp_helmet_type( mitm[p], THELM_WIZARD_HAT ))
                {
                    if (one_chance_in(6))
                        set_equip_race( mitm[p], ISFLAG_ELVEN );
                }
                else
                {
                    // helms and helmets
                    if (one_chance_in(8))
                        set_equip_race( mitm[p], ISFLAG_ORCISH );
                    if (one_chance_in(6))
                        set_equip_race( mitm[p], ISFLAG_DWARVEN );
                }
                break;

            case ARM_ROBE:
                if (one_chance_in(4))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                break;

            case ARM_RING_MAIL:
            case ARM_SCALE_MAIL:
            case ARM_CHAIN_MAIL:
            case ARM_SPLINT_MAIL:
            case ARM_BANDED_MAIL:
            case ARM_PLATE_MAIL:
                if (mitm[p].sub_type <= ARM_CHAIN_MAIL && one_chance_in(6))
                    set_equip_race( mitm[p], ISFLAG_ELVEN );
                if (mitm[p].sub_type >= ARM_RING_MAIL && one_chance_in(5))
                    set_equip_race( mitm[p], ISFLAG_DWARVEN );
                if (one_chance_in(5))
                    set_equip_race( mitm[p], ISFLAG_ORCISH );

            default:    // skins, hides, crystal plate are always plain
                break;
            }
        }

        switch (item_race)
        {
        case MAKE_ITEM_ELVEN:
            set_equip_race( mitm[p], ISFLAG_ELVEN );
            break;

        case MAKE_ITEM_DWARVEN: 
            set_equip_race( mitm[p], ISFLAG_DWARVEN );
            if (coinflip())
                mitm[p].plus++;
            break;

        case MAKE_ITEM_ORCISH: 
            set_equip_race( mitm[p], ISFLAG_ORCISH );
            break;
        }


        if (50 + item_level >= random2(250)
            || item_level == MAKE_GOOD_ITEM
            || (mitm[p].sub_type == ARM_HELMET 
                && cmp_helmet_type( mitm[p], THELM_WIZARD_HAT )))
        {
            mitm[p].plus += random2(3);

            if (mitm[p].sub_type <= ARM_PLATE_MAIL && 20 + item_level >= random2(300))
                mitm[p].plus += random2(3);

            if (30 + item_level >= random2(350)
                && (item_level == MAKE_GOOD_ITEM
                    || (!cmp_equip_race( mitm[p], ISFLAG_ORCISH )
                        || (mitm[p].sub_type <= ARM_PLATE_MAIL && coinflip()))))
            {
                switch (mitm[p].sub_type)
                {
                case ARM_SHIELD:   // shield - must do special things for this!
                case ARM_LARGE_SHIELD:
                case ARM_BUCKLER:
                    set_item_ego_type( mitm[p], OBJ_ARMOUR, SPARM_PROTECTION );
                    break;  // prot
                    //break;

                case ARM_CLOAK:
                    if (cmp_equip_race( mitm[p], ISFLAG_DWARVEN ))
                        break;

                    switch (random2(4))
                    {
                    case 0:
                        set_item_ego_type( mitm[p], 
                                           OBJ_ARMOUR, SPARM_POISON_RESISTANCE );
                        break;

                    case 1:
                        set_item_ego_type( mitm[p], 
                                           OBJ_ARMOUR, SPARM_DARKNESS );
                        break;
                    case 2:
                        set_item_ego_type( mitm[p], 
                                           OBJ_ARMOUR, SPARM_MAGIC_RESISTANCE );
                        break;
                    case 3:
                        set_item_ego_type( mitm[p], 
                                           OBJ_ARMOUR, SPARM_PRESERVATION );
                        break;
                    }
                    break;

                case ARM_HELMET:
                    if (cmp_helmet_type(mitm[p],THELM_WIZARD_HAT) && coinflip())
                    {
                        if (one_chance_in(3))
                        {
                            set_item_ego_type( mitm[p], 
                                               OBJ_ARMOUR, SPARM_MAGIC_RESISTANCE );
                        }
                        else 
                        {
                            set_item_ego_type( mitm[p], 
                                               OBJ_ARMOUR, SPARM_INTELLIGENCE );
                        }
                    }
                    else
                    {
                        set_item_ego_type( mitm[p], OBJ_ARMOUR, 
                                           coinflip() ? SPARM_SEE_INVISIBLE
                                                      : SPARM_INTELLIGENCE );
                    }
                    break;

                case ARM_GLOVES:
                    set_item_ego_type( mitm[p], OBJ_ARMOUR, 
                                       coinflip() ? SPARM_DEXTERITY
                                                  : SPARM_STRENGTH );
                    break;

                case ARM_BOOTS:
                    switch (random2(3))
                    {
                    case 0:
                        if (mitm[p].plus2 == TBOOT_BOOTS)
                            set_item_ego_type(mitm[p], OBJ_ARMOUR, SPARM_RUNNING);
                        break;
                    case 1:
                        set_item_ego_type(mitm[p], OBJ_ARMOUR, SPARM_LEVITATION);
                        break;
                    case 2:
                        set_item_ego_type(mitm[p], OBJ_ARMOUR, SPARM_STEALTH);
                        break;
                    }
                    break;

                case ARM_ROBE:
                    switch (random2(4))
                    {
                    case 0:
                        set_item_ego_type( mitm[p], OBJ_ARMOUR, 
                                           coinflip() ? SPARM_COLD_RESISTANCE
                                                      : SPARM_FIRE_RESISTANCE );
                        break;

                    case 1:
                        set_item_ego_type( mitm[p], 
                                           OBJ_ARMOUR, SPARM_MAGIC_RESISTANCE );
                        break;

                    case 2:
                        set_item_ego_type( mitm[p], OBJ_ARMOUR, 
                                           coinflip() ? SPARM_POSITIVE_ENERGY
                                                      : SPARM_RESISTANCE );
                        break;
                    case 3:
                        if (force_type != OBJ_RANDOM
                            || is_random_artefact( mitm[p] )
                            || get_armour_ego_type( mitm[p] ) != SPARM_NORMAL
                            || random2(50) > 10 + item_level)
                        {
                            break;
                        }

                        set_item_ego_type( mitm[p], OBJ_ARMOUR, SPARM_ARCHMAGI );
                        break;
                    }
                    break;

                default:    // other body armours:
                    set_item_ego_type( mitm[p], OBJ_ARMOUR,
                                       coinflip() ? SPARM_COLD_RESISTANCE 
                                                  : SPARM_FIRE_RESISTANCE );

                    if (one_chance_in(9))
                    {
                        set_item_ego_type( mitm[p], 
                                           OBJ_ARMOUR, SPARM_POSITIVE_ENERGY );
                    }

                    if (one_chance_in(5))
                    {
                        set_item_ego_type( mitm[p], 
                                           OBJ_ARMOUR, SPARM_MAGIC_RESISTANCE );
                    }

                    if (one_chance_in(5))
                    {
                        set_item_ego_type( mitm[p], 
                                           OBJ_ARMOUR, SPARM_POISON_RESISTANCE );
                    }

                    if (mitm[p].sub_type == ARM_PLATE_MAIL
                        && one_chance_in(15))
                    {
                        set_item_ego_type( mitm[p], 
                                           OBJ_ARMOUR, SPARM_PONDEROUSNESS );
                        mitm[p].plus += 3 + random2(4);
                    }
                    break;
                }
            }
        }
        else if (one_chance_in(12))
        {
            // mitm[p].plus = (coinflip() ? 99 : 98);   // 98? 99?
            do_curse_item( mitm[p] );

            if (one_chance_in(5))
                mitm[p].plus -= random2(3);

            set_item_ego_type( mitm[p], OBJ_ARMOUR, SPARM_NORMAL );
        }

        // if not given a racial type, and special, give shiny/runed/etc desc.
        if (cmp_equip_race( mitm[p], 0 )
            && cmp_equip_desc( mitm[p], 0 )
            && (((is_random_artefact(mitm[p]) 
                        || get_armour_ego_type( mitm[p] ) != SPARM_NORMAL)
                    && !one_chance_in(10))
                || (mitm[p].plus != 0 && one_chance_in(3))))
        {
            switch (random2(3))
            {
            case 0:
                set_equip_desc( mitm[p], ISFLAG_GLOWING );
                break;

            case 1:
                set_equip_desc( mitm[p], ISFLAG_RUNED );
                break;

            case 2:
            default:
                set_equip_desc( mitm[p], ISFLAG_EMBROIDERED_SHINY );
                break;
            }
        }

        // Make sure you don't get a hide from acquirement (since that
        // would be an enchanted item which somehow didn't get converted
        // into armour).
        if (item_level == MAKE_GOOD_ITEM)
            hide2armour( &(mitm[p].sub_type) ); // what of animal hides? {dlb}

        // skin armours + Crystal PM don't get special enchantments
        //   or species, but can be randarts
        if (mitm[p].sub_type >= ARM_DRAGON_HIDE
            && mitm[p].sub_type <= ARM_SWAMP_DRAGON_ARMOUR)
        {
            set_equip_race( mitm[p], ISFLAG_NO_RACE );
            set_item_ego_type( mitm[p], OBJ_ARMOUR, SPARM_NORMAL );
        }
        break;

    case OBJ_WANDS:
        // determine sub_type:
        if (force_type != OBJ_RANDOM) 
            mitm[p].sub_type = force_type;
        else
        {
            mitm[p].sub_type = random2( NUM_WANDS );

            // Adjusted distribution here -- bwr
            // Wands used to be uniform (5.26% each)
            //
            // Now:
            // invis, hasting, healing  (1.11% each)
            // fireball, teleportaion   (3.74% each)
            // others                   (6.37% each)
            if ((mitm[p].sub_type == WAND_INVISIBILITY
                    || mitm[p].sub_type == WAND_HASTING
                    || mitm[p].sub_type == WAND_HEALING)
                || ((mitm[p].sub_type == WAND_FIREBALL
                        || mitm[p].sub_type == WAND_TELEPORTATION)
                    && coinflip()))
            {
                mitm[p].sub_type = random2( NUM_WANDS );
            }
        }

        // determine upper bound on charges:
        range_charges = ((mitm[p].sub_type == WAND_HEALING
                          || mitm[p].sub_type == WAND_HASTING
                          || mitm[p].sub_type == WAND_INVISIBILITY)   ? 8 :
                         (mitm[p].sub_type == WAND_FLAME
                          || mitm[p].sub_type == WAND_FROST
                          || mitm[p].sub_type == WAND_MAGIC_DARTS
                          || mitm[p].sub_type == WAND_RANDOM_EFFECTS) ? 28
                                                                      : 16);

        // generate charges randomly:
        mitm[p].plus = random2avg(range_charges, 3);
        //
        // set quantity to one:
        quant = 1;
        break;

    case OBJ_FOOD:              // this can be parsed out {dlb}
        // determine sub_type:
        if (force_type == OBJ_RANDOM)
        {
            temp_rand = random2(1000);

            mitm[p].sub_type =
                    ((temp_rand >= 750) ? FOOD_MEAT_RATION : // 25.00% chance
                     (temp_rand >= 450) ? FOOD_BREAD_RATION :// 30.00% chance
                     (temp_rand >= 350) ? FOOD_PEAR :        // 10.00% chance
                     (temp_rand >= 250) ? FOOD_APPLE :       // 10.00% chance
                     (temp_rand >= 150) ? FOOD_CHOKO :       // 10.00% chance
                     (temp_rand >= 140) ? FOOD_CHEESE :      //  1.00% chance
                     (temp_rand >= 130) ? FOOD_PIZZA :       //  1.00% chance
                     (temp_rand >= 120) ? FOOD_SNOZZCUMBER : //  1.00% chance
                     (temp_rand >= 110) ? FOOD_APRICOT :     //  1.00% chance
                     (temp_rand >= 100) ? FOOD_ORANGE :      //  1.00% chance
                     (temp_rand >=  90) ? FOOD_BANANA :      //  1.00% chance
                     (temp_rand >=  80) ? FOOD_STRAWBERRY :  //  1.00% chance
                     (temp_rand >=  70) ? FOOD_RAMBUTAN :    //  1.00% chance
                     (temp_rand >=  60) ? FOOD_LEMON :       //  1.00% chance
                     (temp_rand >=  50) ? FOOD_GRAPE :       //  1.00% chance
                     (temp_rand >=  40) ? FOOD_SULTANA :     //  1.00% chance
                     (temp_rand >=  30) ? FOOD_LYCHEE :      //  1.00% chance
                     (temp_rand >=  20) ? FOOD_BEEF_JERKY :  //  1.00% chance
                     (temp_rand >=  10) ? FOOD_SAUSAGE :     //  1.00% chance
                     (temp_rand >=   5) ? FOOD_HONEYCOMB     //  0.50% chance
                                        : FOOD_ROYAL_JELLY );//  0.50% chance
        }
        else
            mitm[p].sub_type = force_type;

        // Happens with ghoul food acquirement -- use place_chunks() outherwise
        if (mitm[p].sub_type == FOOD_CHUNK)
        {
            for (count = 0; count < 1000; count++) 
            {
                temp_rand = random2( NUM_MONSTERS );     // random monster
                temp_rand = mons_charclass( temp_rand ); // corpse base type

                if (mons_weight( temp_rand ) > 0)        // drops a corpse
                    break;
            }

            // set chunk flavour (default to common dungeon rat steaks):
            mitm[p].plus = (count == 1000) ? MONS_RAT : temp_rand;
            
            // set duration
            mitm[p].special = (10 + random2(11)) * 10;
        }

        // determine quantity:
        if (allow_uniques > 1)
            quant = allow_uniques;
        else
        {
            quant = 1;

            if (mitm[p].sub_type != FOOD_MEAT_RATION 
                && mitm[p].sub_type != FOOD_BREAD_RATION)
            {
                if (one_chance_in(80))
                    quant += random2(3);

                if (mitm[p].sub_type == FOOD_STRAWBERRY
                    || mitm[p].sub_type == FOOD_GRAPE
                    || mitm[p].sub_type == FOOD_SULTANA)
                {
                    quant += 3 + random2avg(15,2);
                }
            }
        }
        break;

    case OBJ_POTIONS:
        quant = 1;

        if (one_chance_in(18))
            quant++;

        if (one_chance_in(25))
            quant++;

        if (force_type != OBJ_RANDOM)
            mitm[p].sub_type = force_type;
        else
        {
            temp_rand = random2(9);       // general type of potion;

            switch (temp_rand)
            {
            case 0:
            case 1:
            case 2:
            case 8:
                // healing potions
                if (one_chance_in(3))
                    mitm[p].sub_type = POT_HEAL_WOUNDS;         // 14.074%
                else
                    mitm[p].sub_type = POT_HEALING;             // 28.148% 

                if (one_chance_in(20))                         
                    mitm[p].sub_type = POT_CURE_MUTATION;       //  2.222%
                break;

            case 3:
            case 4:
                // enhancements
                if (coinflip())
                    mitm[p].sub_type = POT_SPEED;               //  6.444%
                else
                    mitm[p].sub_type = POT_MIGHT;               //  6.444%

                if (one_chance_in(10))
                    mitm[p].sub_type = POT_BERSERK_RAGE;        //  1.432%

                if (one_chance_in(5))
                    mitm[p].sub_type = POT_INVISIBILITY;        //  3.580%

                if (one_chance_in(6))                           
                    mitm[p].sub_type = POT_LEVITATION;          //  3.580%

                if (one_chance_in(30))                         
                    mitm[p].sub_type = POT_PORRIDGE;            //  0.741%
                break;

            case 5:
                // gain ability
                mitm[p].sub_type = POT_GAIN_STRENGTH + random2(3); // 1.125%
                                                            // or 0.375% each

                if (one_chance_in(10))
                    mitm[p].sub_type = POT_EXPERIENCE;          //  0.125%

                if (one_chance_in(10))
                    mitm[p].sub_type = POT_MAGIC;               //  0.139%

                if (!one_chance_in(8))
                    mitm[p].sub_type = POT_RESTORE_ABILITIES;   //  9.722%

                quant = 1;
                break;

            case 6:
            case 7:
                // bad things
                switch (random2(6))
                {
                case 0:
                case 4:
                    // is this not always the case? - no, level one is 0 {dlb}
                    if (item_level > 0)
                    {
                        mitm[p].sub_type = POT_POISON;          //  6.475%

                        if (item_level > 10 && one_chance_in(4))
                            mitm[p].sub_type = POT_STRONG_POISON;

                        break;
                    }

                /* **** intentional fall through **** */     // ignored for %
                case 5:
                    if (item_level > 6)
                    {
                        mitm[p].sub_type = POT_MUTATION;        //  3.237%
                        break;
                    }

                /* **** intentional fall through **** */     // ignored for %
                case 1:
                    mitm[p].sub_type = POT_SLOWING;             //  3.237%
                    break;

                case 2:
                    mitm[p].sub_type = POT_PARALYSIS;           //  3.237%
                    break;

                case 3:
                    mitm[p].sub_type = POT_CONFUSION;           //  3.237%
                    break;

                }

                if (one_chance_in(8))
                    mitm[p].sub_type = POT_DEGENERATION;        //  2.775%

                if (one_chance_in(1000))                        //  0.022%
                    mitm[p].sub_type = POT_DECAY;

                break;
            }
        }

        mitm[p].plus = 0;
        break;

    case OBJ_SCROLLS:
        // determine sub_type:
        if (force_type == OBJ_RANDOM)
        {
            // only used in certain cases {dlb}
            int depth_mod = random2(1 + item_level);

            temp_rand = random2(920);

            mitm[p].sub_type =
                    ((temp_rand > 751) ? SCR_IDENTIFY :          // 18.26%
                     (temp_rand > 629) ? SCR_REMOVE_CURSE :      // 13.26%
                     (temp_rand > 554) ? SCR_TELEPORTATION :     //  8.15%
                     (temp_rand > 494) ? SCR_DETECT_CURSE :      //  6.52%
                     (temp_rand > 464) ? SCR_FEAR :              //  3.26%
                     (temp_rand > 434) ? SCR_NOISE :             //  3.26%
                     (temp_rand > 404) ? SCR_MAGIC_MAPPING :     //  3.26%
                     (temp_rand > 374) ? SCR_FORGETFULNESS :     //  3.26%
                     (temp_rand > 344) ? SCR_RANDOM_USELESSNESS ://  3.26%
                     (temp_rand > 314) ? SCR_CURSE_WEAPON :      //  3.26%
                     (temp_rand > 284) ? SCR_CURSE_ARMOUR :      //  3.26%
                     (temp_rand > 254) ? SCR_RECHARGING :        //  3.26%
                     (temp_rand > 224) ? SCR_BLINKING :          //  3.26%
                     (temp_rand > 194) ? SCR_PAPER :             //  3.26%
                     (temp_rand > 164) ? SCR_ENCHANT_ARMOUR :    //  3.26%
                     (temp_rand > 134) ? SCR_ENCHANT_WEAPON_I :  //  3.26%
                     (temp_rand > 104) ? SCR_ENCHANT_WEAPON_II : //  3.26%

                 // Crawl is kind to newbie adventurers {dlb}:
                 // yes -- these five are messy {dlb}:
                 // yes they are a hellish mess of tri-ops and long lines,
                 // this formating is somewhat better -- bwr
                     (temp_rand > 74) ?
                        ((item_level < 4) ? SCR_TELEPORTATION
                                          : SCR_IMMOLATION) :    //  3.26%
                     (temp_rand > 59) ?
                         ((depth_mod < 4) ? SCR_TELEPORTATION
                                          : SCR_ACQUIREMENT) :   //  1.63%
                     (temp_rand > 44) ?
                         ((depth_mod < 4) ? SCR_DETECT_CURSE
                                          : SCR_SUMMONING) :     //  1.63%
                     (temp_rand > 29) ?
                         ((depth_mod < 4) ? SCR_TELEPORTATION    //  1.63%
                                          : SCR_ENCHANT_WEAPON_III) :
                     (temp_rand > 14) ?
                         ((depth_mod < 7) ? SCR_DETECT_CURSE
                                          : SCR_TORMENT)         //  1.63%
                     // default:
                       : ((depth_mod < 7) ? SCR_TELEPORTATION    //  1.63%
                                          : SCR_VORPALISE_WEAPON));
        }
        else
            mitm[p].sub_type = force_type;

        // determine quantity:
        temp_rand = random2(48);

        quant = ((temp_rand > 1
                  || mitm[p].sub_type == SCR_VORPALISE_WEAPON
                  || mitm[p].sub_type == SCR_ENCHANT_WEAPON_III
                  || mitm[p].sub_type == SCR_ACQUIREMENT
                  || mitm[p].sub_type == SCR_TORMENT)  ? 1 :    // 95.83%
                                     (temp_rand == 0)  ? 2      //  2.08%
                                                       : 3);    //  2.08%
        mitm[p].plus = 0;
        break;

    case OBJ_JEWELLERY:
        // determine whether an unrandart will be generated {dlb}:
        if (item_level > 2 
            && you.level_type != LEVEL_ABYSS
            && you.level_type != LEVEL_PANDEMONIUM
            && random2(2000) <= 100 + (item_level * 3) 
            && one_chance_in(20))
        {
            icky = find_okay_unrandart(OBJ_JEWELLERY);

            if (icky != -1)
            {
                quant = 1;
                make_item_unrandart( mitm[p], icky );
                break;
            }
        }

        // otherwise, determine jewellery type {dlb}:
        if (force_type == OBJ_RANDOM)
        {
            mitm[p].sub_type = (!one_chance_in(4) ? random2(24)   // rings
                                                  : AMU_RAGE + random2(10));

            // Adjusted distribution here -- bwr
            if ((mitm[p].sub_type == RING_INVISIBILITY
                    || mitm[p].sub_type == RING_REGENERATION
                    || mitm[p].sub_type == RING_TELEPORT_CONTROL
                    || mitm[p].sub_type == RING_SLAYING)
                && !one_chance_in(3))
            {
                mitm[p].sub_type = random2(24);
            }
        }
        else
            mitm[p].sub_type = force_type;

        // quantity is always one {dlb}:
        quant = 1;

        // everything begins as uncursed, unenchanted jewellery {dlb}:
        mitm[p].plus = 0;
        mitm[p].plus2 = 0;

        // set pluses for rings that require them {dlb}:
        switch (mitm[p].sub_type)
        {
        case RING_PROTECTION:
        case RING_STRENGTH:
        case RING_SLAYING:
        case RING_EVASION:
        case RING_DEXTERITY:
        case RING_INTELLIGENCE:
            if (one_chance_in(5))       // 20% of such rings are cursed {dlb}
            {
                do_curse_item( mitm[p] );
                mitm[p].plus = (coinflip() ? -2 : -3);

                if (one_chance_in(3))
                    mitm[p].plus -= random2(4);
            }
            else
            {
                mitm[p].plus += 1 + (one_chance_in(3) ? random2(3)
                                                      : random2avg(6, 2));
            }
            break;

        default:
            break;
        }

        // rings of slaying also require that pluses2 be set {dlb}:
        if (mitm[p].sub_type == RING_SLAYING)
        {
            if (item_cursed( mitm[p] ) && !one_chance_in(20))
                mitm[p].plus2 = -1 - random2avg(6, 2);
            else
            {
                mitm[p].plus2 += 1 + (one_chance_in(3) ? random2(3)
                                                       : random2avg(6, 2));

                if (random2(25) < 9)        // 36% of such rings {dlb}
                {
                    // make "ring of damage"
                    do_uncurse_item( mitm[p] );
                    mitm[p].plus = 0;
                    mitm[p].plus2 += 2;
                }
            }
        }

        // All jewellery base types should now work. -- bwr
        if (allow_uniques == 1 && item_level > 2
            && random2(2000) <= 100 + (item_level * 3) && coinflip())
        {
            make_item_randart( mitm[p] );
            break;
        }

        // rings of hunger and teleportation are always cursed {dlb}:
        if (mitm[p].sub_type == RING_HUNGER
            || mitm[p].sub_type == RING_TELEPORTATION
            || one_chance_in(50))
        {
            do_curse_item( mitm[p] );
        }
        break;

    case OBJ_BOOKS:
      create_book:
        do
        {
            mitm[p].sub_type = random2(NUM_BOOKS);

            if (book_rarity(mitm[p].sub_type) == 100)
                continue;

            if (mitm[p].sub_type != BOOK_DESTRUCTION
                && mitm[p].sub_type != BOOK_MANUAL)
            {
                if (one_chance_in(10))
                {
                    if (coinflip())
                        mitm[p].sub_type = BOOK_WIZARDRY;
                    else
                        mitm[p].sub_type = BOOK_POWER;
                }

                if (random2(item_level + 1) + 1 >= book_rarity(mitm[p].sub_type)
                    || one_chance_in(100))
                {
                    break;
                }
                else
                {
                    mitm[p].sub_type = BOOK_DESTRUCTION;
                    continue;
                }
            }
        }
        while (mitm[p].sub_type == BOOK_DESTRUCTION
                   || mitm[p].sub_type == BOOK_MANUAL);

        if (book_rarity(mitm[p].sub_type) == 100)
            goto create_book;

        mitm[p].special = random2(5);

        if (one_chance_in(10))
            mitm[p].special += random2(8) * 10;

        if (force_type != OBJ_RANDOM)
            mitm[p].sub_type = force_type;

        quant = 1;

        // tome of destruction : rare!
        if (force_type == BOOK_DESTRUCTION
            || (random2(7000) <= item_level + 20 && item_level > 10
                && force_type == OBJ_RANDOM))
        {
            mitm[p].sub_type = BOOK_DESTRUCTION;
        }

        // skill manuals - also rare
        // fixed to generate manuals for *all* extant skills - 14mar2000 {dlb}
        if (force_type == BOOK_MANUAL
            || (random2(4000) <= item_level + 20 && item_level > 6
                && force_type == OBJ_RANDOM))
        {
            mitm[p].sub_type = BOOK_MANUAL;

            if (one_chance_in(4))
            {
                mitm[p].plus = SK_SPELLCASTING
                                    + random2(NUM_SKILLS - SK_SPELLCASTING);
            }
            else
            {
                mitm[p].plus = random2(SK_UNARMED_COMBAT);

                if (mitm[p].plus == SK_UNUSED_1)
                    mitm[p].plus = SK_UNARMED_COMBAT;
            }
        }
        break;

    case OBJ_STAVES:            // this can be parsed, too {dlb}
        if (force_type != OBJ_RANDOM)
            mitm[p].sub_type = force_type;
        else
        {
            mitm[p].sub_type = random2(13);

            // top three non-spell staves are in separate block -- bwr
            if (mitm[p].sub_type >= 10)
                mitm[p].sub_type = STAFF_AIR + mitm[p].sub_type - 10;  

            // spell staves 
            if (one_chance_in(20))
                mitm[p].sub_type = STAFF_SMITING + random2(10);

            if ((mitm[p].sub_type == STAFF_ENERGY 
                || mitm[p].sub_type == STAFF_CHANNELING) && one_chance_in(4))
            {
                mitm[p].sub_type = coinflip() ? STAFF_WIZARDRY : STAFF_POWER; 
            }
        }

        mitm[p].special = random2(9);

        quant = 1;
        break;

    case OBJ_ORBS:              // always forced in current setup {dlb}
        quant = 1;

        if (force_type != OBJ_RANDOM)
            mitm[p].sub_type = force_type;

        // I think we only have one type of orb now, so ... {dlb}
        set_unique_item_status( OBJ_ORBS, mitm[p].sub_type, UNIQ_EXISTS );
        break;

    // I think these must always be forced, too ... {dlb}

    case OBJ_MISCELLANY: //mv: rewrote with use of NUM_MISCELLANY (9 Aug 01)
        if (force_type == OBJ_RANDOM)
        {
            do
                mitm[p].sub_type = random2(NUM_MISCELLANY);
            while //mv: never generated
               ((mitm[p].sub_type == MISC_RUNE_OF_ZOT)
                || (mitm[p].sub_type == MISC_HORN_OF_GERYON)
                || (mitm[p].sub_type == MISC_PORTABLE_ALTAR_OF_NEMELEX)
                // mv: others are possible but less often
                // btw. chances of generating decks are almost the same as
                // before, other chances are now distributed more steadily
                || (mitm[p].sub_type == MISC_DECK_OF_POWER && !one_chance_in(12))
                || (mitm[p].sub_type == MISC_DECK_OF_SUMMONINGS && !one_chance_in(3))
                || (mitm[p].sub_type == MISC_DECK_OF_TRICKS && !one_chance_in(3))
                || (mitm[p].sub_type == MISC_DECK_OF_WONDERS && !one_chance_in(3))
                );

            // filling those silly empty boxes -- bwr
            if (mitm[p].sub_type == MISC_EMPTY_EBONY_CASKET 
                && !one_chance_in(20))
            {
                mitm[p].sub_type = MISC_BOX_OF_BEASTS;
            }
        }
        else
        {
            mitm[p].sub_type = force_type;
        }

        if (mitm[p].sub_type == MISC_DECK_OF_WONDERS
            || mitm[p].sub_type == MISC_DECK_OF_SUMMONINGS
            || mitm[p].sub_type == MISC_DECK_OF_POWER)
        {
            mitm[p].plus = 4 + random2(10);
        }

        if (mitm[p].sub_type == MISC_DECK_OF_TRICKS)
            mitm[p].plus = 6 + random2avg(15, 2);

        if (mitm[p].sub_type == MISC_RUNE_OF_ZOT)
            mitm[p].plus = item_race;

        quant = 1;
        break; // mv: end of rewrote;

    // that is, everything turns to gold if not enumerated above, so ... {dlb}
    default:
        mitm[p].base_type = OBJ_GOLD;

        // Note that acquirement level gold gives much less than the
        // price of a scroll of acquirement (520 gold). -- bwr
        if (item_level == MAKE_GOOD_ITEM)
            quant = 50 + random2avg(100, 2) + random2avg(100, 2);
        else
            quant = 1 + random2avg(19, 2) + random2(item_level);
        break;
    }

    mitm[p].quantity = quant;

    // should really only be used for monster inventories.
    if (dont_place)
    {
        mitm[p].x = 0;
        mitm[p].y = 0;
        mitm[p].link = NON_ITEM;
    }
    else
    {
        do
        {
            x_pos = random2(GXM);
            y_pos = random2(GYM);
        }
        while (grd[x_pos][y_pos] != DNGN_FLOOR);

        move_item_to_grid( &p, x_pos, y_pos );
    }

    item_colour( mitm[p] );

    // Okay, this check should be redundant since the purpose of 
    // this function is to create valid items.  Still, we're adding 
    // this safety for fear that a report of Trog giving a non-existant 
    // item might symbolize something more serious. -- bwr
    return (is_valid_item( mitm[p] ) ? p : NON_ITEM);
}                               // end items()


void give_item(int mid, int level_number) //mv: cleanup+minor changes
{
    int temp_rand = 0;          // probability determination {dlb}

    int bp = 0;
    int thing_created = 0;
    int hand_used = 0;          // for Ettins etc.
    int xitc = 0;
    int xitt = 0;

    int iquan = 0;
    // forces colour and quantity, too for intial weapons {dlb}
    int force_item = 0;

    int item_race = MAKE_ITEM_RANDOM_RACE;
    int give_level = level_number;

    //mv: THIS CODE DISTRIBUTES WANDS/SCROLLS/POTIONS
    //(now only to uniques but it's easy to modify that)
    //7 Aug 01

    //mv - give scroll

    if (mons_is_unique( menv[mid].type ) && one_chance_in(3))
    {
        thing_created = items(0, OBJ_SCROLLS, OBJ_RANDOM, true, give_level, 0);
        if (thing_created == NON_ITEM)
            return;

        mitm[thing_created].flags = 0;
        menv[mid].inv[MSLOT_SCROLL] = thing_created;
    }

    //mv - give wand
    if (mons_is_unique( menv[mid].type ) && one_chance_in(5))
    {
        thing_created = items(0, OBJ_WANDS, OBJ_RANDOM, true, give_level, 0);
        if (thing_created == NON_ITEM)
            return;

        mitm[thing_created].flags = 0;
        menv[mid].inv[MSLOT_WAND] = thing_created;
    }

    //mv - give potion
    if (mons_is_unique( menv[mid].type ) && one_chance_in(3))
    {
        thing_created = items(0, OBJ_POTIONS, OBJ_RANDOM, true, give_level, 0);
        if (thing_created == NON_ITEM)
            return;

        mitm[thing_created].flags = 0;
        menv[mid].inv[MSLOT_POTION] = thing_created;
    }


    //end of DISTRIBUTE WANDS/POTIONS/SCROLLS CODE

    bp = get_item_slot();
    if (bp == NON_ITEM)
        return;

    mitm[bp].quantity = 0; // set below if force_item, else we toss this item!
    mitm[bp].plus = 0;
    mitm[bp].plus2 = 0;
    mitm[bp].special = 0;

    // this flags things to "goto give_armour" below ... {dlb}
    mitm[bp].base_type = 101;

    if (menv[mid].type == MONS_DANCING_WEAPON
        && player_in_branch( BRANCH_HALL_OF_BLADES ))
    {
        give_level = MAKE_GOOD_ITEM;
    }

    // moved setting of quantity here to keep it in mind {dlb}
    iquan = 1;
    // I wonder if this is even used, given calls to item() {dlb}


    switch (menv[mid].type)
    {
    case MONS_KOBOLD:
        // a few of the smarter kobolds have blowguns.
        if (one_chance_in(15) && level_number > 1)
        {
            mitm[bp].base_type = OBJ_WEAPONS;
            mitm[bp].sub_type = WPN_BLOWGUN;
            break;
        }
        // intentional fallthrough
    case MONS_BIG_KOBOLD:
        if (random2(5) < 3)     // give hand weapon
        {
            mitm[bp].base_type = OBJ_WEAPONS;

            temp_rand = random2(5);
            mitm[bp].sub_type = ((temp_rand > 2) ? WPN_DAGGER :     // 40%
                                 (temp_rand > 0) ? WPN_SHORT_SWORD  // 40%
                                                 : WPN_CLUB);       // 20%
        }
        else if (random2(5) < 2)        // give darts
        {
            item_race = MAKE_ITEM_NO_RACE;
            mitm[bp].base_type = OBJ_MISSILES;
            mitm[bp].sub_type = MI_DART;
            iquan = 1 + random2(5);
        }
        else
            goto give_ammo;
        break;

    case MONS_HOBGOBLIN:
        if (one_chance_in(3))
            item_race = MAKE_ITEM_ORCISH;

        if (random2(5) < 3)     // give hand weapon
        {
            mitm[bp].base_type = OBJ_WEAPONS;
            mitm[bp].sub_type = WPN_CLUB;
        }
        else
            goto give_ammo;
        break;

    case MONS_GOBLIN:
        if (one_chance_in(3))
            item_race = MAKE_ITEM_ORCISH;

        if (one_chance_in(12) && level_number > 1)
        {
            mitm[bp].base_type = OBJ_WEAPONS;
            mitm[bp].base_type = WPN_BLOWGUN;
            break;
        }
        // deliberate fall through {dlb}
    case MONS_JESSICA:
    case MONS_IJYB:
        if (random2(5) < 3)     // < 1 // give hand weapon
        {
            mitm[bp].base_type = OBJ_WEAPONS;
            mitm[bp].sub_type = (coinflip() ? WPN_DAGGER : WPN_CLUB);
        }
        else
            goto give_ammo;
        break;

    case MONS_WIGHT:
    case MONS_NORRIS:
        mitm[bp].base_type = OBJ_WEAPONS;
        mitm[bp].sub_type = (one_chance_in(6) ? WPN_WAR_AXE + random2(4)
                                              : WPN_MACE + random2(12));

        if (coinflip())
        {
            force_item = 1;
            item_race = MAKE_ITEM_NO_RACE;
            mitm[bp].plus += 1 + random2(3);
            mitm[bp].plus2 += 1 + random2(3);

            if (one_chance_in(5))
                set_item_ego_type( mitm[bp], OBJ_WEAPONS, SPWPN_FREEZING );
        }

        if (one_chance_in(3))
            do_curse_item( mitm[bp] );
        break;

    case MONS_GNOLL:
    case MONS_OGRE_MAGE:
    case MONS_NAGA_WARRIOR:
    case MONS_GREATER_NAGA:
    case MONS_EDMUND:
    case MONS_DUANE:
        item_race = MAKE_ITEM_NO_RACE;

        if (!one_chance_in(5))
        {
            mitm[bp].base_type = OBJ_WEAPONS;

            temp_rand = random2(5);
            mitm[bp].sub_type = ((temp_rand >  2) ? WPN_SPEAR : // 40%
                                 (temp_rand == 2) ? WPN_FLAIL : // 20%
                                 (temp_rand == 1) ? WPN_HALBERD // 20%
                                                  : WPN_CLUB);  // 20%
        }
        break;

    case MONS_ORC:
        if (one_chance_in(15) && level_number > 1)
        {
            mitm[bp].base_type = OBJ_WEAPONS;
            mitm[bp].base_type = WPN_BLOWGUN;
            break;
        }
        // deliberate fall through {gdl}
    case MONS_ORC_PRIEST:
        item_race = MAKE_ITEM_ORCISH;
        // deliberate fall through {dlb}
    case MONS_TERENCE:
        if (!one_chance_in(5))
        {
            mitm[bp].base_type = OBJ_WEAPONS;

            temp_rand = random2(240);
            mitm[bp].sub_type = ((temp_rand > 209) ? WPN_DAGGER :      //12.50%
                                 (temp_rand > 179) ? WPN_CLUB :        //12.50%
                                 (temp_rand > 152) ? WPN_FLAIL :       //11.25%
                                 (temp_rand > 128) ? WPN_HAND_AXE :    //10.00%
                                 (temp_rand > 108) ? WPN_HAMMER :      // 8.33%
                                 (temp_rand >  88) ? WPN_HALBERD :     // 8.33%
                                 (temp_rand >  68) ? WPN_SHORT_SWORD : // 8.33%
                                 (temp_rand >  48) ? WPN_MACE :        // 8.33%
                                 (temp_rand >  38) ? WPN_WHIP :        // 4.17%
                                 (temp_rand >  28) ? WPN_TRIDENT :     // 4.17%
                                 (temp_rand >  18) ? WPN_FALCHION :    // 4.17%
                                 (temp_rand >   8) ? WPN_MORNINGSTAR : // 4.17%
                                 (temp_rand >   2) ? WPN_WAR_AXE           // 2.50%
                                                   : WPN_SPIKED_FLAIL);// 1.25%
        }
        else
            goto give_ammo;
        break;

    case MONS_DEEP_ELF_FIGHTER:
    case MONS_DEEP_ELF_HIGH_PRIEST:
    case MONS_DEEP_ELF_KNIGHT:
    case MONS_DEEP_ELF_PRIEST:
    case MONS_DEEP_ELF_SOLDIER:
        item_race = MAKE_ITEM_ELVEN;
        mitm[bp].base_type = OBJ_WEAPONS;

        temp_rand = random2(8);
        mitm[bp].sub_type = ((temp_rand > 5) ? WPN_LONG_SWORD :    // 2 in 8
                             (temp_rand > 3) ? WPN_SHORT_SWORD :   // 2 in 8
                             (temp_rand > 2) ? WPN_SCIMITAR :      // 1 in 8
                             (temp_rand > 1) ? WPN_MACE :          // 1 in 8
                             (temp_rand > 0) ? WPN_BOW             // 1 in 8
                                             : WPN_HAND_CROSSBOW); // 1 in 8
        break;

    case MONS_DEEP_ELF_ANNIHILATOR:
    case MONS_DEEP_ELF_CONJURER:
    case MONS_DEEP_ELF_DEATH_MAGE:
    case MONS_DEEP_ELF_DEMONOLOGIST:
    case MONS_DEEP_ELF_MAGE:
    case MONS_DEEP_ELF_SORCERER:
    case MONS_DEEP_ELF_SUMMONER:
        item_race = MAKE_ITEM_ELVEN;
        mitm[bp].base_type = OBJ_WEAPONS;

        temp_rand = random2(6);
        mitm[bp].sub_type = ((temp_rand > 3) ? WPN_LONG_SWORD : // 2 in 6
                             (temp_rand > 2) ? WPN_SHORT_SWORD :// 1 in 6
                             (temp_rand > 1) ? WPN_SABRE :      // 1 in 6
                             (temp_rand > 0) ? WPN_DAGGER       // 1 in 6
                                             : WPN_WHIP);       // 1 in 6
        break;

    case MONS_ORC_WARRIOR:
    case MONS_ORC_HIGH_PRIEST:
    case MONS_BLORK_THE_ORC:
        item_race = MAKE_ITEM_ORCISH;
        // deliberate fall-through {dlb}
    case MONS_DANCING_WEAPON:   // give_level may have been adjusted above
    case MONS_FRANCES:
    case MONS_FRANCIS:
    case MONS_HAROLD:
    case MONS_JOSEPH:
    case MONS_LOUISE:
    case MONS_MICHAEL:
    case MONS_NAGA:
    case MONS_NAGA_MAGE:
    case MONS_RUPERT:
    case MONS_SKELETAL_WARRIOR:
    case MONS_WAYNE:
        mitm[bp].base_type = OBJ_WEAPONS;

        temp_rand = random2(120);
        mitm[bp].sub_type = ((temp_rand > 109) ? WPN_LONG_SWORD :   // 8.33%
                             (temp_rand >  99) ? WPN_SHORT_SWORD :  // 8.33%
                             (temp_rand >  89) ? WPN_SCIMITAR :     // 8.33%
                             (temp_rand >  79) ? WPN_BATTLEAXE :    // 8.33%
                             (temp_rand >  69) ? WPN_HAND_AXE :     // 8.33%
                             (temp_rand >  59) ? WPN_HALBERD :      // 8.33%
                             (temp_rand >  49) ? WPN_GLAIVE :       // 8.33%
                             (temp_rand >  39) ? WPN_MORNINGSTAR :  // 8.33%
                             (temp_rand >  29) ? WPN_GREAT_MACE :   // 8.33%
                             (temp_rand >  19) ? WPN_TRIDENT :      // 8.33%
                             (temp_rand >  10) ? WPN_WAR_AXE :          // 7.50%
                             (temp_rand >   1) ? WPN_FLAIL :        // 7.50%
                             (temp_rand >   0) ? WPN_BROAD_AXE      // 0.83%
                                               : WPN_SPIKED_FLAIL); // 0.83%
        break;

    case MONS_ORC_WARLORD:
        // being at the top has it's priviledges
        if (one_chance_in(3))
            give_level = MAKE_GOOD_ITEM;
        // deliberate fall-through
    case MONS_ORC_KNIGHT:
        item_race = MAKE_ITEM_ORCISH;
        // deliberate fall-through, I guess {dlb}
    case MONS_NORBERT:
    case MONS_JOZEF:
    case MONS_URUG:
    case MONS_VAULT_GUARD:
    case MONS_VAMPIRE_KNIGHT:
        mitm[bp].base_type = OBJ_WEAPONS;

        temp_rand = random2(24);
        mitm[bp].sub_type = ((temp_rand > 19) ? WPN_GREAT_SWORD :// 16.67%
                             (temp_rand > 15) ? WPN_LONG_SWORD : // 16.67%
                             (temp_rand > 11) ? WPN_BATTLEAXE :  // 16.67%
                             (temp_rand >  7) ? WPN_WAR_AXE :        // 16.67%
                             (temp_rand >  5) ? WPN_GREAT_MACE : //  8.33%
                             (temp_rand >  3) ? WPN_GREAT_FLAIL ://  8.33%
                             (temp_rand >  1) ? WPN_GLAIVE :     //  8.33%
                             (temp_rand >  0) ? WPN_BROAD_AXE    //  4.17%
                                              : WPN_HALBERD);    //  4.17%

        if (one_chance_in(4))
            mitm[bp].plus += 1 + random2(3);
        break;

    case MONS_CYCLOPS:
    case MONS_STONE_GIANT:
        item_race = MAKE_ITEM_NO_RACE;
        mitm[bp].base_type = OBJ_MISSILES;
        mitm[bp].sub_type = MI_LARGE_ROCK;
        break;

    case MONS_TWO_HEADED_OGRE:
    case MONS_ETTIN:
        item_race = MAKE_ITEM_NO_RACE;
        hand_used = 0;

        if (menv[mid].inv[MSLOT_WEAPON] != NON_ITEM)
            hand_used = 1;

        mitm[bp].base_type = OBJ_WEAPONS;
        mitm[bp].sub_type = (one_chance_in(3) ? WPN_GIANT_SPIKED_CLUB
                                              : WPN_GIANT_CLUB);

        if (one_chance_in(10) || menv[mid].type == MONS_ETTIN)
        {
            mitm[bp].sub_type = ((one_chance_in(10)) ? WPN_GREAT_FLAIL
                                                     : WPN_GREAT_MACE);
        }
        break;

    case MONS_REAPER:
        give_level = MAKE_GOOD_ITEM;
        // intentional fall-through...

    case MONS_SIGMUND:
        item_race = MAKE_ITEM_NO_RACE;
        mitm[bp].base_type = OBJ_WEAPONS;
        mitm[bp].sub_type = WPN_SCYTHE;
        break;

    case MONS_BALRUG:
        item_race = MAKE_ITEM_NO_RACE;
        mitm[bp].base_type = OBJ_WEAPONS;
        mitm[bp].sub_type = WPN_DEMON_WHIP;
        break;

    case MONS_RED_DEVIL:
        if (!one_chance_in(3))
        {
            item_race = MAKE_ITEM_NO_RACE;
            mitm[bp].base_type = OBJ_WEAPONS;
            mitm[bp].sub_type = (one_chance_in(3) ? WPN_DEMON_TRIDENT
                                                  : WPN_TRIDENT);
        }
        break;

    case MONS_OGRE:
    case MONS_HILL_GIANT:
    case MONS_EROLCHA:
        item_race = MAKE_ITEM_NO_RACE;
        mitm[bp].base_type = OBJ_WEAPONS;

        mitm[bp].sub_type = (one_chance_in(3) ? WPN_GIANT_SPIKED_CLUB
                                              : WPN_GIANT_CLUB);

        if (one_chance_in(10))
        {
            mitm[bp].sub_type = (one_chance_in(10) ? WPN_GREAT_FLAIL
                                                   : WPN_GREAT_MACE);
        }
        break;

    case MONS_CENTAUR:
    case MONS_CENTAUR_WARRIOR:
        item_race = MAKE_ITEM_NO_RACE;
        mitm[bp].base_type = OBJ_WEAPONS;
        mitm[bp].sub_type = WPN_BOW;
        break;

    case MONS_YAKTAUR:
    case MONS_YAKTAUR_CAPTAIN:
        item_race = MAKE_ITEM_NO_RACE;
        mitm[bp].base_type = OBJ_WEAPONS;
        mitm[bp].sub_type = WPN_CROSSBOW;
        break;

    case MONS_EFREET:
    case MONS_ERICA:
        force_item = 1;
        item_race = MAKE_ITEM_NO_RACE;
        mitm[bp].base_type = OBJ_WEAPONS;
        mitm[bp].sub_type = WPN_SCIMITAR;
        mitm[bp].plus = random2(5);
        mitm[bp].plus2 = random2(5);
        mitm[bp].colour = RED;  // forced by force_item above {dlb}
        set_item_ego_type( mitm[bp], OBJ_WEAPONS, SPWPN_FLAMING );
        break;

    case MONS_ANGEL:
        force_item = 1;
        mitm[bp].base_type = OBJ_WEAPONS;
        mitm[bp].colour = WHITE;        // forced by force_item above {dlb}

        set_equip_desc( mitm[bp], ISFLAG_GLOWING );
        if (one_chance_in(3))
        {
            mitm[bp].sub_type = (one_chance_in(3) ? WPN_GREAT_MACE : WPN_MACE);
            set_item_ego_type( mitm[bp], OBJ_WEAPONS, SPWPN_HOLY_WRATH );
        }
        else
        {
            mitm[bp].sub_type = WPN_LONG_SWORD;
        }

        mitm[bp].plus = 1 + random2(3);
        mitm[bp].plus2 = 1 + random2(3);
        break;

    case MONS_DAEVA:
        force_item = 1;
        mitm[bp].base_type = OBJ_WEAPONS;
        mitm[bp].colour = WHITE;        // forced by force_item above {dlb}

        mitm[bp].sub_type = (one_chance_in(4) ? WPN_GREAT_SWORD
                                              : WPN_LONG_SWORD);

        set_equip_desc( mitm[bp], ISFLAG_GLOWING );
        set_item_ego_type( mitm[bp], OBJ_WEAPONS, SPWPN_HOLY_WRATH );
        mitm[bp].plus = 1 + random2(3);
        mitm[bp].plus2 = 1 + random2(3);
        break;

    case MONS_HELL_KNIGHT:
    case MONS_MAUD:
    case MONS_ADOLF:
    case MONS_MARGERY:
        force_item = 1;
        mitm[bp].base_type = OBJ_WEAPONS;
        mitm[bp].sub_type = WPN_LONG_SWORD + random2(3);

        if (one_chance_in(7))
            mitm[bp].sub_type = WPN_HALBERD;
        if (one_chance_in(7))
            mitm[bp].sub_type = WPN_GLAIVE;
        if (one_chance_in(7))
            mitm[bp].sub_type = WPN_GREAT_MACE;
        if (one_chance_in(7))
            mitm[bp].sub_type = WPN_BATTLEAXE;
        if (one_chance_in(7))
            mitm[bp].sub_type = WPN_WAR_AXE;
        if (one_chance_in(7))
            mitm[bp].sub_type = WPN_BROAD_AXE;
        if (one_chance_in(7))
            mitm[bp].sub_type = WPN_DEMON_TRIDENT;
        if (one_chance_in(7))
            mitm[bp].sub_type = WPN_DEMON_BLADE;
        if (one_chance_in(7))
            mitm[bp].sub_type = WPN_DEMON_WHIP;

        temp_rand = random2(3);
        set_equip_desc( mitm[bp], (temp_rand == 1) ? ISFLAG_GLOWING :
                                  (temp_rand == 2) ? ISFLAG_RUNED 
                                                   : ISFLAG_NO_DESC );

        if (one_chance_in(3))
            set_item_ego_type( mitm[bp], OBJ_WEAPONS, SPWPN_FLAMING );
        else if (one_chance_in(3))
        {
            temp_rand = random2(5);

            set_item_ego_type( mitm[bp], OBJ_WEAPONS, 
                                ((temp_rand == 0) ? SPWPN_DRAINING :
                                 (temp_rand == 1) ? SPWPN_VORPAL :
                                 (temp_rand == 2) ? SPWPN_PAIN :
                                 (temp_rand == 3) ? SPWPN_DISTORTION 
                                                  : SPWPN_SPEED) );
        }

        mitm[bp].plus += random2(6);
        mitm[bp].plus2 += random2(6);

        mitm[bp].colour = RED;  // forced by force_item above {dlb}

        if (one_chance_in(3))
            mitm[bp].colour = DARKGREY;
        if (one_chance_in(5))
            mitm[bp].colour = CYAN;
        break;

    case MONS_FIRE_GIANT:
        force_item = 1;
        mitm[bp].base_type = OBJ_WEAPONS;
        mitm[bp].sub_type = WPN_GREAT_SWORD;
        mitm[bp].plus = 0;
        mitm[bp].plus2 = 0;
        set_item_ego_type( mitm[bp], OBJ_WEAPONS, SPWPN_FLAMING );

        mitm[bp].colour = RED;  // forced by force_item above {dlb}
        if (one_chance_in(3))
            mitm[bp].colour = DARKGREY;
        if (one_chance_in(5))
            mitm[bp].colour = CYAN;
        break;

    case MONS_FROST_GIANT:
        force_item = 1;
        mitm[bp].base_type = OBJ_WEAPONS;
        mitm[bp].sub_type = WPN_BATTLEAXE;
        mitm[bp].plus = 0;
        mitm[bp].plus2 = 0;
        set_item_ego_type( mitm[bp], OBJ_WEAPONS, SPWPN_FREEZING );

        // forced by force_item above {dlb}
        mitm[bp].colour = (one_chance_in(3) ? WHITE : CYAN);
        break;

    case MONS_KOBOLD_DEMONOLOGIST:
    case MONS_ORC_WIZARD:
    case MONS_ORC_SORCERER:
        item_race = MAKE_ITEM_ORCISH;
        // deliberate fall-through, I guess {dlb}
    case MONS_NECROMANCER:
    case MONS_WIZARD:
    case MONS_PSYCHE:
    case MONS_DONALD:
    case MONS_JOSEPHINE:
    case MONS_AGNES:
        mitm[bp].base_type = OBJ_WEAPONS;
        mitm[bp].sub_type = WPN_DAGGER;
        break;

    case MONS_CEREBOV:
        force_item = 1;
        make_item_fixed_artefact( mitm[bp], false, SPWPN_SWORD_OF_CEREBOV );
        break;

    case MONS_DISPATER:
        force_item = 1;
        make_item_fixed_artefact( mitm[bp], false, SPWPN_STAFF_OF_DISPATER );
        break;

    case MONS_ASMODEUS:
        force_item = 1;
        make_item_fixed_artefact( mitm[bp], false, SPWPN_SCEPTRE_OF_ASMODEUS );
        break;

    case MONS_GERYON: 
        //mv: probably should be moved out of this switch,
        //but it's not worth of it, unless we have more
        //monsters with misc. items
        mitm[bp].base_type = OBJ_MISCELLANY;
        mitm[bp].sub_type = MISC_HORN_OF_GERYON;
        break;

    case MONS_SALAMANDER: //mv: new 8 Aug 2001
                          //Yes, they've got really nice items, but
                          //it's almost impossible to get them
        force_item = 1;
        item_race = MAKE_ITEM_NO_RACE;
        mitm[bp].base_type = OBJ_WEAPONS;
        temp_rand = random2(6);

        mitm[bp].sub_type = ((temp_rand == 5) ? WPN_GREAT_SWORD :
                             (temp_rand == 4) ? WPN_TRIDENT :
                             (temp_rand == 3) ? WPN_SPEAR :
                             (temp_rand == 2) ? WPN_GLAIVE :
                             (temp_rand == 1) ? WPN_BOW
                                              : WPN_HALBERD);

        if (mitm[bp].sub_type == WPN_BOW) 
            set_item_ego_type( mitm[bp], OBJ_WEAPONS, SPWPN_FLAME );
        else
            set_item_ego_type( mitm[bp], OBJ_WEAPONS, SPWPN_FLAMING );

        mitm[bp].plus = random2(5);
        mitm[bp].plus2 = random2(5);
        mitm[bp].colour = RED;  // forced by force_item above {dlb}
        break;
    }                           // end "switch(menv[mid].type)"

    // only happens if something in above switch doesn't set it {dlb}
    if (mitm[bp].base_type == 101)
    {
        mitm[bp].base_type = OBJ_UNASSIGNED;
        goto give_ammo;
    }

    mitm[bp].x = 0;
    mitm[bp].y = 0;
    mitm[bp].link = NON_ITEM;

    if (force_item)
        mitm[bp].quantity = iquan;
    else if (mons_is_unique( menv[mid].type ))
    {
        if (random2(100) <= 9 + menv[mid].hit_dice)
            give_level = MAKE_GOOD_ITEM;
        else 
            give_level = level_number + 5;
    }

    xitc = mitm[bp].base_type;
    xitt = mitm[bp].sub_type;

    // Note this mess, all the work above doesn't mean much unless
    // force_item is set... otherwise we're just going to take the
    // base and subtypes and create a new item. -- bwr
    thing_created = ((force_item) ? bp : items( 0, xitc, xitt, true,
                                                give_level, item_race) );

    if (thing_created == NON_ITEM)
        return;

    mitm[thing_created].x = 0;
    mitm[thing_created].y = 0;
    mitm[thing_created].link = NON_ITEM;
    unset_ident_flags( mitm[thing_created], ISFLAG_IDENT_MASK );

    //mv: now every item gets in appropriate slot
    //no more miscellany in potion slot etc. (19 May 2001)
    // hand_used = 0 unless Ettin's 2nd hand etc.
    if ( mitm[thing_created].base_type == OBJ_WEAPONS )
      menv[mid].inv[hand_used] = thing_created;
    else if ( mitm[thing_created].base_type == OBJ_MISSILES )
      menv[mid].inv[MSLOT_MISSILE] = thing_created;
    else if ( mitm[thing_created].base_type == OBJ_SCROLLS )
      menv[mid].inv[MSLOT_SCROLL] = thing_created;
    else if ( mitm[thing_created].base_type == OBJ_GOLD )
      menv[mid].inv[MSLOT_GOLD] = thing_created;
    else if ( mitm[thing_created].base_type == OBJ_POTIONS )
      menv[mid].inv[MSLOT_POTION] = thing_created;
    else if ( mitm[thing_created].base_type == OBJ_MISCELLANY )
      menv[mid].inv[MSLOT_MISCELLANY] = thing_created;


    if (get_weapon_brand( mitm[thing_created] ) == SPWPN_PROTECTION )
        menv[mid].armour_class += 5;

    if (!force_item || mitm[thing_created].colour == BLACK) 
        item_colour( mitm[thing_created] );

  give_ammo:
    // mv: gives ammunition
    // note that item_race is not reset for this section
    if (menv[mid].inv[MSLOT_WEAPON] != NON_ITEM
        && launches_things( mitm[menv[mid].inv[MSLOT_WEAPON]].sub_type ))
    {
        xitc = OBJ_MISSILES;
        xitt = launched_by(mitm[menv[mid].inv[MSLOT_WEAPON]].sub_type);

        thing_created = items( 0, xitc, xitt, true, give_level, item_race );
        if (thing_created == NON_ITEM)
            return;

        // monsters will always have poisoned needles -- otherwise
        // they are just going to behave badly --GDL
        if (xitt == MI_NEEDLE)
            set_item_ego_type(mitm[thing_created], OBJ_MISSILES, SPMSL_POISONED);

        mitm[thing_created].x = 0;
        mitm[thing_created].y = 0;
        mitm[thing_created].flags = 0;
        menv[mid].inv[MSLOT_MISSILE] = thing_created;

        item_colour( mitm[thing_created] );
    }                           // end if needs ammo

    bp = get_item_slot();
    if (bp == NON_ITEM)
        return;

    mitm[bp].x = 0;
    mitm[bp].y = 0;
    mitm[bp].link = NON_ITEM;

    item_race = MAKE_ITEM_RANDOM_RACE;
    give_level = 1 + (level_number / 2);

    int force_colour = 0; //mv: important !!! Items with force_colour = 0
                         //are colored defaultly after following
                         //switch. Others will get force_colour.

    switch (menv[mid].type)
    {
    case MONS_DEEP_ELF_ANNIHILATOR:
    case MONS_DEEP_ELF_CONJURER:
    case MONS_DEEP_ELF_DEATH_MAGE:
    case MONS_DEEP_ELF_DEMONOLOGIST:
    case MONS_DEEP_ELF_FIGHTER:
    case MONS_DEEP_ELF_HIGH_PRIEST:
    case MONS_DEEP_ELF_KNIGHT:
    case MONS_DEEP_ELF_MAGE:
    case MONS_DEEP_ELF_PRIEST:
    case MONS_DEEP_ELF_SOLDIER:
    case MONS_DEEP_ELF_SORCERER:
    case MONS_DEEP_ELF_SUMMONER:
        if (item_race == MAKE_ITEM_RANDOM_RACE)
            item_race = MAKE_ITEM_ELVEN;
        // deliberate fall through {dlb}
    case MONS_IJYB:
    case MONS_ORC:
    case MONS_ORC_HIGH_PRIEST:
    case MONS_ORC_PRIEST:
    case MONS_ORC_SORCERER:
        if (item_race == MAKE_ITEM_RANDOM_RACE)
            item_race = MAKE_ITEM_ORCISH;
        // deliberate fall through {dlb}
    case MONS_ERICA:
    case MONS_HAROLD:
    case MONS_JOSEPH:
    case MONS_JOSEPHINE:
    case MONS_JOZEF:
    case MONS_NORBERT:
    case MONS_PSYCHE:
    case MONS_TERENCE:
        if (random2(5) < 2)
        {
            mitm[bp].base_type = OBJ_ARMOUR;

            switch (random2(8))
            {
            case 0:
            case 1:
            case 2:
            case 3:
                mitm[bp].sub_type = ARM_LEATHER_ARMOUR;
                break;
            case 4:
            case 5:
                mitm[bp].sub_type = ARM_RING_MAIL;
                break;
            case 6:
                mitm[bp].sub_type = ARM_SCALE_MAIL;
                break;
            case 7:
                mitm[bp].sub_type = ARM_CHAIN_MAIL;
                break;
            }
        }
        else
            return;
        break;

    case MONS_DUANE:
    case MONS_EDMUND:
    case MONS_RUPERT:
    case MONS_URUG:
    case MONS_WAYNE:
        mitm[bp].base_type = OBJ_ARMOUR;
        mitm[bp].sub_type = ARM_LEATHER_ARMOUR + random2(4);
        break;

    case MONS_ORC_WARLORD:
        // being at the top has it's priviledges
        if (one_chance_in(3))
            give_level = MAKE_GOOD_ITEM;
        // deliberate fall through
    case MONS_ORC_KNIGHT:
    case MONS_ORC_WARRIOR:
        if (item_race == MAKE_ITEM_RANDOM_RACE)
            item_race = MAKE_ITEM_ORCISH;
        // deliberate fall through {dlb}
    case MONS_ADOLF:
    case MONS_HELL_KNIGHT:
    case MONS_LOUISE:
    case MONS_MARGERY:
    case MONS_MAUD:
    case MONS_VAMPIRE_KNIGHT:
    case MONS_VAULT_GUARD:
        mitm[bp].base_type = OBJ_ARMOUR;
        mitm[bp].sub_type = ARM_CHAIN_MAIL + random2(4);
        break;

    case MONS_ANGEL:
    case MONS_SIGMUND:
    case MONS_WIGHT:
        item_race = MAKE_ITEM_NO_RACE;
        mitm[bp].base_type = OBJ_ARMOUR;
        mitm[bp].sub_type = ARM_ROBE;
        force_colour = WHITE; //mv: always white
        break;

    case MONS_NAGA:
    case MONS_NAGA_MAGE:
    case MONS_NAGA_WARRIOR:
        if (!one_chance_in(3))
            return;
        // deliberate fall through {dlb}
    case MONS_DONALD:
    case MONS_GREATER_NAGA:
    case MONS_JESSICA:
    case MONS_KOBOLD_DEMONOLOGIST:
    case MONS_OGRE_MAGE:
    case MONS_ORC_WIZARD:
    case MONS_WIZARD:
        item_race = MAKE_ITEM_NO_RACE;
        mitm[bp].base_type = OBJ_ARMOUR;
        mitm[bp].sub_type = ARM_ROBE;
        break;

    case MONS_BORIS:
        give_level = MAKE_GOOD_ITEM;
        // fall-through
    case MONS_AGNES:
    case MONS_BLORK_THE_ORC:
    case MONS_FRANCES:
    case MONS_FRANCIS:
    case MONS_NECROMANCER:
    case MONS_VAMPIRE_MAGE:
        mitm[bp].base_type = OBJ_ARMOUR;
        mitm[bp].sub_type = ARM_ROBE;
        force_colour = DARKGREY; //mv: always darkgrey
        break;

    default:
        return;
    }                           // end of switch(menv [mid].type)

    iquan = 1; //because it may have been set earlier
               //by giving ammo or weapons {dlb}

    xitc = mitm[bp].base_type;
    xitt = mitm[bp].sub_type;

    if (mons_is_unique( menv[mid].type ) && give_level != MAKE_GOOD_ITEM)
    {
        if (random2(100) < 9 + menv[mid].hit_dice)
            give_level = MAKE_GOOD_ITEM;
        else 
            give_level = level_number + 5;
    }

    thing_created = items( 0, xitc, xitt, true, give_level, item_race );

    if (thing_created == NON_ITEM)
        return;

    mitm[thing_created].x = 0;
    mitm[thing_created].y = 0;
    mitm[thing_created].link = NON_ITEM;
    menv[mid].inv[MSLOT_ARMOUR] = thing_created;

    //mv: all items with force_colour = 0 are colored via items().
    if (force_colour) 
        mitm[thing_created].colour = force_colour;

    menv[mid].armour_class += property( mitm[thing_created], PARM_AC );

    const int armour_plus = mitm[thing_created].plus;

    ASSERT(abs(armour_plus) < 20);

    if (abs(armour_plus) < 20) 
        menv[mid].armour_class += armour_plus;

    menv[mid].evasion += property( mitm[thing_created], PARM_EVASION ) / 2;

    if (menv[mid].evasion < 1)
        menv[mid].evasion = 1;   // This *shouldn't* happen.
}                               // end give_item()

//---------------------------------------------------------------------------
//                           PRIVATE HELPER FUNCTIONS
//---------------------------------------------------------------------------

static bool is_weapon_special(int the_weapon)
{
    return (mitm[the_weapon].special != SPWPN_NORMAL);
}                               // end is_weapon_special()

static void set_weapon_special(int the_weapon, int spwpn)
{
    set_item_ego_type( mitm[the_weapon], OBJ_WEAPONS, spwpn );
}                               // end set_weapon_special()

static void check_doors(void)
{
    unsigned char ig;
    unsigned char solid_count = 0;      // clarifies innermost loop {dlb}
    int x,y;

    for (x = 1; x < GXM-1; x++)
    {
        for (y = 1; y < GYM-1; y++)
        {
            ig = grd[x][y];

            if (ig != DNGN_CLOSED_DOOR)
                continue;

            solid_count = 0;

            // first half of each conditional represents bounds checking {dlb}:
            if (grd[x - 1][y] < DNGN_LAST_SOLID_TILE)
                solid_count++;

            if (grd[x + 1][y] < DNGN_LAST_SOLID_TILE)
                solid_count++;

            if (grd[x][y - 1] < DNGN_LAST_SOLID_TILE)
                solid_count++;

            if (grd[x][y + 1] < DNGN_LAST_SOLID_TILE)
                solid_count++;

            grd[x][y] = ((solid_count < 2) ? DNGN_FLOOR : DNGN_CLOSED_DOOR);
        }
    }
}                               // end check_doors()

static void hide_doors(void)
{
    unsigned char dx = 0, dy = 0;     // loop variables
    unsigned char wall_count = 0;     // clarifies inner loop {dlb}

    for (dx = 1; dx < GXM-1; dx++)
    {
        for (dy = 1; dy < GYM-1; dy++)
        {
            // only one out of four doors are candidates for hiding {gdl}:
            if (grd[dx][dy] == DNGN_CLOSED_DOOR && one_chance_in(4))
            {
                wall_count = 0;

                if (grd[dx - 1][dy] == DNGN_ROCK_WALL)
                    wall_count++;

                if (grd[dx + 1][dy] == DNGN_ROCK_WALL)
                    wall_count++;

                if (grd[dx][dy - 1] == DNGN_ROCK_WALL)
                    wall_count++;

                if (grd[dx][dy + 1] == DNGN_ROCK_WALL)
                    wall_count++;

                // if door is attached to more than one wall, hide it {dlb}:
                if (wall_count > 1)
                    grd[dx][dy] = DNGN_SECRET_DOOR;
            }
        }
    }
}                               // end hide_doors()

static void prepare_swamp(void)
{
    int i, j;                   // loop variables
    int temp_rand;              // probability determination {dlb}

    for (i = 10; i < (GXM - 10); i++)
    {
        for (j = 10; j < (GYM - 10); j++)
        {
            // doors -> floors {dlb}
            if (grd[i][j] == DNGN_CLOSED_DOOR || grd[i][j] == DNGN_SECRET_DOOR)
                grd[i][j] = DNGN_FLOOR;

            // floors -> shallow water 1 in 3 times {dlb}
            if (grd[i][j] == DNGN_FLOOR && one_chance_in(3))
                grd[i][j] = DNGN_SHALLOW_WATER;

            // walls -> deep/shallow water or remain unchanged {dlb}
            if (grd[i][j] == DNGN_ROCK_WALL)
            {
                temp_rand = random2(6);

                if (temp_rand > 0)      // 17% chance unchanged {dlb}
                {
                    grd[i][j] = ((temp_rand > 2) ? DNGN_SHALLOW_WATER // 50%
                                                 : DNGN_DEEP_WATER);  // 33%
                }
            }
        }
    }
}                               // end prepare_swamp()

// Gives water which is next to ground/shallow water a chance of being
// shallow. Checks each water space.
static void prepare_water( int level_number )
{
    int i, j, k, l;             // loop variables {dlb}
    unsigned char which_grid;   // code compaction {dlb}

    for (i = 10; i < (GXM - 10); i++)
    {
        for (j = 10; j < (GYM - 10); j++)
        {
            if (grd[i][j] == DNGN_DEEP_WATER)
            {
                for (k = -1; k < 2; k++)
                {
                    for (l = -1; l < 2; l++)
                    {
                        if (k != 0 || l != 0)
                        {
                            which_grid = grd[i + k][j + l];

                            // must come first {dlb}
                            if (which_grid == DNGN_SHALLOW_WATER
                                && one_chance_in( 8 + level_number ))  
                            {
                                grd[i][j] = DNGN_SHALLOW_WATER;
                            }
                            else if (which_grid >= DNGN_FLOOR
                                     && random2(100) < 80 - level_number * 4)
                            {
                                grd[i][j] = DNGN_SHALLOW_WATER;
                            }
                        }
                    }
                }
            }
        }
    }
}                               // end prepare_water()

static bool find_in_area(int sx, int sy, int ex, int ey, unsigned char feature)
{
    int x,y;

    if (feature != 0)
    {
        for(x = sx; x <= ex; x++)
        {
            for(y = sy; y <= ey; y++)
            {
                if (grd[x][y] == feature)
                    return (true);
            }
        }
    }

    return (false);
}

// stamp a box.  can avoid a possible type,  and walls and floors can
// be different (or not stamped at all)
// Note that the box boundaries are INclusive.
static bool make_box(int room_x1, int room_y1, int room_x2, int room_y2,
    unsigned char floor, unsigned char wall, unsigned char avoid)
{
    int bx,by;

    // check for avoidance
    if (find_in_area(room_x1, room_y1, room_x2, room_y2, avoid))
        return false;

    // draw walls
    if (wall != 0)
    {
        for(bx=room_x1; bx<=room_x2; bx++)
        {
            grd[bx][room_y1] = wall;
            grd[bx][room_y2] = wall;
        }
        for(by=room_y1+1; by<room_y2; by++)
        {
            grd[room_x1][by] = wall;
            grd[room_x2][by] = wall;
        }
    }

    // draw floor
    if (floor != 0)
    {
        for(bx=room_x1 + 1; bx < room_x2; bx++)
            for(by=room_y1 + 1; by < room_y2; by++)
                grd[bx][by] = floor;
    }

    return true;
}

// take care of labyrinth, abyss, pandemonium
// returns 1 if we should skip further generation,
// -1 if we should immediately quit,  and 0 otherwise.
static int builder_by_type(int level_number, char level_type)
{
    if (level_type == LEVEL_LABYRINTH)
    {
        labyrinth_level(level_number);
        return -1;
    }

    if (level_type == LEVEL_ABYSS)
    {
        generate_abyss();
        return 1;
    }

    if (level_type == LEVEL_PANDEMONIUM)
    {
        char which_demon = -1;
        // Could do spotty_level, but that doesn't always put all paired
        // stairs reachable from each other which isn't a problem in normal
        // dungeon but could be in Pandemonium
        if (one_chance_in(15))
        {
            do
            {
                which_demon = random2(4);

                // makes these things less likely as you find more
                if (one_chance_in(4))
                {
                    which_demon = -1;
                    break;
                }
            }
            while (you.unique_creatures[40 + which_demon] == 1);
        }

        if (which_demon >= 0)
        {
            you.unique_creatures[40 + which_demon] = 1;
            build_vaults(level_number, which_demon + 60);
        }
        else
        {
            plan_main(level_number, 0);
            build_minivaults(level_number, 300 + random2(9));
        }

        return 1;
    }

    // must be normal dungeon
    return 0;
}

// returns 1 if we should skip further generation,
// -1 if we should immediately quit,  and 0 otherwise.
static int builder_by_branch(int level_number)
{
    switch (you.where_are_you)
    {
    case BRANCH_HIVE:
        if (level_number == you.branch_stairs[STAIRS_HIVE]
            + branch_depth(STAIRS_HIVE))
            build_vaults(level_number, 80);
        else
            spotty_level(false, 100 + random2(500), false);
        return 1;

    case BRANCH_SLIME_PITS:
        if (level_number == you.branch_stairs[STAIRS_SLIME_PITS]
                                + branch_depth(STAIRS_SLIME_PITS))
        {
            build_vaults(level_number, 81);
        }
        else
            spotty_level(false, 100 + random2(500), false);
        return 1;

    case BRANCH_VAULTS:
        if (level_number == you.branch_stairs[STAIRS_VAULTS]
            + branch_depth(STAIRS_VAULTS))
        {
            build_vaults(level_number, 82);
            return 1;
        }
        break;

    case BRANCH_HALL_OF_BLADES:
        if (level_number == you.branch_stairs[STAIRS_HALL_OF_BLADES]
            + branch_depth(STAIRS_HALL_OF_BLADES))
        {
            build_vaults(level_number, 83);
            return 1;
        }
        break;

    case BRANCH_HALL_OF_ZOT:
        if (level_number == you.branch_stairs[STAIRS_HALL_OF_ZOT]
            + branch_depth(STAIRS_HALL_OF_ZOT))
        {
            build_vaults(level_number, 84);
            return 1;
        }
        break;

    case BRANCH_ECUMENICAL_TEMPLE:
        if (level_number == you.branch_stairs[STAIRS_ECUMENICAL_TEMPLE]
                                    + branch_depth(STAIRS_ECUMENICAL_TEMPLE))
        {
            build_vaults(level_number, 85);
            return 1;
        }
        break;

    case BRANCH_SNAKE_PIT:
        if (level_number == you.branch_stairs[STAIRS_SNAKE_PIT]
                                    + branch_depth(STAIRS_SNAKE_PIT))
        {
            build_vaults(level_number, 86);
            return 1;
        }
        break;

    case BRANCH_ELVEN_HALLS:
        if (level_number == you.branch_stairs[STAIRS_ELVEN_HALLS]
                                    + branch_depth(STAIRS_ELVEN_HALLS))
        {
            build_vaults(level_number, 87);
            return 1;
        }
        break;

    case BRANCH_TOMB:
        if (level_number == you.branch_stairs[STAIRS_TOMB] + 1)
        {
            build_vaults(level_number, 88);
            return 1;
        }
        else if (level_number == you.branch_stairs[STAIRS_TOMB] + 2)
        {
            build_vaults(level_number, 89);
            return 1;
        }
        else if (level_number == you.branch_stairs[STAIRS_TOMB] + 3)
        {
            build_vaults(level_number, 90);
            return 1;
        }
        break;

    case BRANCH_SWAMP:
        if (level_number == you.branch_stairs[STAIRS_SWAMP]
                                            + branch_depth(STAIRS_SWAMP))
        {
            build_vaults(level_number, 91);
            return 1;
        }
        break;

    case BRANCH_ORCISH_MINES:
        spotty_level(false, 100 + random2(500), false);
        return 1;

    case BRANCH_LAIR:
        if (!one_chance_in(3))
        {
            spotty_level(false, 100 + random2(500), false);
            return 1;
        }
        break;

    case BRANCH_VESTIBULE_OF_HELL:
        build_vaults( level_number, 50 );
        link_items();
        return -1;

    case BRANCH_DIS:
        if (level_number == 33)
        {
            build_vaults(level_number, 51);
            return 1;
        }
        break;

    case BRANCH_GEHENNA:
        if (level_number == 33)
        {
            build_vaults(level_number, 52);
            return 1;
        }
        break;

    case BRANCH_COCYTUS:
        if (level_number == 33)
        {
            build_vaults(level_number, 53);
            return 1;
        }
        break;

    case BRANCH_TARTARUS:
        if (level_number == 33)
        {
            build_vaults(level_number, 54);
            return 1;
        }
        break;

    default:
        break;
    }
    return 0;
}

// returns 1 if we should dispense with city building,
// 0 otherwise.  Also sets special_room if one is generated
// so that we can link it up later.

static int builder_normal(int level_number, char level_type, spec_room &sr)
{
    UNUSED( level_type );

    bool skipped = false;
    bool done_city = false;

    if (player_in_branch( BRANCH_DIS ))
    {
        city_level(level_number);
        return 1;
    }

    if (player_in_branch( BRANCH_MAIN_DUNGEON ) 
        && level_number > 10 && level_number < 23 && one_chance_in(9))
    {
        // Can't have vaults on you.where_are_you != BRANCH_MAIN_DUNGEON levels
        build_vaults(level_number, 100);
        return 1;
    }

    if (player_in_branch( BRANCH_VAULTS ))
    {
        if (one_chance_in(3))
            city_level(level_number);
        else
            plan_main(level_number, 4);
        return 1;
    }

    if (level_number > 7 && level_number < 23)
    {
        if (one_chance_in(16))
        {
            spotty_level(false, 0, coinflip());
            return 1;
        }

        if (one_chance_in(16))
        {
            bigger_room();
            return 1;
        }
    }

    if (level_number > 2 && level_number < 23 && one_chance_in(3))
    {
        plan_main(level_number, 0);

        if (one_chance_in(3) && level_number > 6)
            build_minivaults(level_number, 200);

        return 1;
    }

    if (one_chance_in(3))
        skipped = true;

    //V was 3
    if (!skipped && one_chance_in(7))
    {
        // sometimes roguey_levels generate a special room
        roguey_level(level_number, sr);

        if (level_number > 6
            && player_in_branch( BRANCH_MAIN_DUNGEON )
            && one_chance_in(4))
        {
            build_minivaults(level_number, 200);
            return 1;
        }
    }
    else
    {
        if (!skipped && level_number > 13 && one_chance_in(8))
        {
            if (one_chance_in(3))
                city_level(level_number);
            else
                plan_main(level_number, 4);
            done_city = true;
        }
    }

    // maybe create a special room,  if roguey_level hasn't done it
    // already.
    if (!sr.created && level_number > 5 && !done_city && one_chance_in(5))
        special_room(level_number, sr);

    return 0;
}

// returns 1 if we should skip extras(),  otherwise 0
static int builder_basic(int level_number)
{
    int temp_rand;
    int doorlevel = random2(11);
    int corrlength = 2 + random2(14);
    int roomsize = 4 + random2(5) + random2(6);
    int no_corr = (one_chance_in(100) ? 500 + random2(500) : 30 + random2(200));
    int intersect_chance = (one_chance_in(20) ? 400 : random2(20));

    make_trail( 35, 30, 35, 20, corrlength, intersect_chance, no_corr,
                DNGN_STONE_STAIRS_DOWN_I, DNGN_STONE_STAIRS_UP_I );

    make_trail( 10, 15, 10, 15, corrlength, intersect_chance, no_corr,
                DNGN_STONE_STAIRS_DOWN_II, DNGN_STONE_STAIRS_UP_II );

    make_trail(50,20,10,15,corrlength,intersect_chance,no_corr,
        DNGN_STONE_STAIRS_DOWN_III, DNGN_STONE_STAIRS_UP_III);

    if (one_chance_in(4))
    {
        make_trail( 10, 20, 40, 20, corrlength, intersect_chance, no_corr,
                    DNGN_ROCK_STAIRS_DOWN );
    }

    if (one_chance_in(4))
    {
        make_trail( 50, 20, 40, 20, corrlength, intersect_chance, no_corr,
                    DNGN_ROCK_STAIRS_UP );
    }


    if (level_number > 1 && one_chance_in(16))
        big_room(level_number);

    if (random2(level_number) > 6 && one_chance_in(3))
        diamond_rooms(level_number);

    // make some rooms:
    int i, no_rooms, max_doors;
    int sx,sy,ex,ey, time_run;

    temp_rand = random2(750);
    time_run = 0;

    no_rooms = ((temp_rand > 63) ? (5 + random2avg(29, 2)) : // 91.47% {dlb}
                (temp_rand > 14) ? 100                       //  6.53% {dlb}
                                 : 1);                       //  2.00% {dlb}

    max_doors = 2 + random2(8);

    for (i = 0; i < no_rooms; i++)
    {
        sx = 8 + random2(50);
        sy = 8 + random2(40);
        ex = sx + 2 + random2(roomsize);
        ey = sy + 2 + random2(roomsize);

        if (!make_room(sx,sy,ex,ey,max_doors, doorlevel))
        {
            time_run++;
            i--;
        }

        if (time_run > 30)
        {
            time_run = 0;
            i++;
        }
    }

    // make some more rooms:
    no_rooms = 1 + random2(3);
    max_doors = 1;

    for (i = 0; i < no_rooms; i++)
    {
        sx = 8 + random2(55);
        sy = 8 + random2(45);
        ex = sx + 5 + random2(6);
        ey = sy + 5 + random2(6);

        if (!make_room(sx,sy,ex,ey,max_doors, doorlevel))
        {
            time_run++;
            i--;
        }

        if (time_run > 30)
        {
            time_run = 0;
            i++;
        }
    }

    return 0;
}

static void builder_extras( int level_number, int level_type )
{
    UNUSED( level_type );

    if (level_number >= 11 && level_number <= 23 && one_chance_in(15))
        place_specific_stair(DNGN_ENTER_LABYRINTH);

    if (level_number > 6 
        && player_in_branch( BRANCH_MAIN_DUNGEON )
        && one_chance_in(3))
    {
        build_minivaults(level_number, 200);
        return;
    }

    if (level_number > 5 && one_chance_in(10))
    {
        many_pools( (coinflip() ? DNGN_DEEP_WATER : DNGN_LAVA) );
        return;
    }

#ifdef USE_RIVERS
    //mv: it's better to be here so other dungeon features
    // are not overriden by water
    int river_type = one_chance_in( 5 + level_number ) ? DNGN_SHALLOW_WATER 
                                                       : DNGN_DEEP_WATER;

    if (level_number > 11 
        && (one_chance_in(5) || (level_number > 15 && !one_chance_in(5))))
    {
        river_type = DNGN_LAVA;
    }

    if (player_in_branch( BRANCH_GEHENNA ))
    {
        river_type = DNGN_LAVA;

        if (coinflip())
            build_river( river_type );
        else  
            build_lake( river_type );
    }
    else if (player_in_branch( BRANCH_COCYTUS ))
    {
        river_type = DNGN_DEEP_WATER;

        if (coinflip())
            build_river( river_type );
        else  
            build_lake( river_type );
    }


    if (level_number > 8 && one_chance_in(16))
        build_river( river_type );
    else if (level_number > 8 && one_chance_in(12))
    {
        build_lake( (river_type != DNGN_SHALLOW_WATER) ? river_type
                                                       : DNGN_DEEP_WATER );
    } 
#endif // USE_RIVERS
}

static void place_traps(int level_number)
{
    int i;
    int num_traps = random2avg(9, 2);

    for (i = 0; i < num_traps; i++)
    {
        // traps can be placed in vaults
        if (env.trap[i].type != TRAP_UNASSIGNED)
            continue;

        do
        {
            env.trap[i].x = 10 + random2(GXM - 20);
            env.trap[i].y = 10 + random2(GYM - 20);
        }
        while (grd[env.trap[i].x][env.trap[i].y] != DNGN_FLOOR);

        env.trap[i].type = TRAP_DART;

        if ((random2(1 + level_number) > 1) && one_chance_in(4))
            env.trap[i].type = TRAP_NEEDLE;
        if (random2(1 + level_number) > 2)
            env.trap[i].type = TRAP_ARROW;
        if (random2(1 + level_number) > 3)
            env.trap[i].type = TRAP_SPEAR;
        if (random2(1 + level_number) > 5)
            env.trap[i].type = TRAP_AXE;
        if (random2(1 + level_number) > 7)
            env.trap[i].type = TRAP_BOLT;
        if (random2(1 + level_number) > 11)
            env.trap[i].type = TRAP_BLADE;

        if ((random2(1 + level_number) > 14 && one_chance_in(3))
            || (player_in_branch( BRANCH_HALL_OF_ZOT ) && coinflip()))
        {
            env.trap[i].type = TRAP_ZOT;
        }

        if (one_chance_in(20))
            env.trap[i].type = TRAP_TELEPORT;
        if (one_chance_in(40))
            env.trap[i].type = TRAP_AMNESIA;

        grd[env.trap[i].x][env.trap[i].y] = DNGN_UNDISCOVERED_TRAP;
    }                           // end "for i"
}                               // end place_traps()

static void place_specific_stair(unsigned char stair)
{
    int sx, sy;

    do
    {
        sx = random2(GXM-10);
        sy = random2(GYM-10);
    }
    while(grd[sx][sy] != DNGN_FLOOR || mgrd[sx][sy] != NON_MONSTER);

    grd[sx][sy] = stair;
}


static void place_branch_entrances(int dlevel, char level_type)
{
    unsigned char stair;
    unsigned char entrance;
    int sx, sy;

    if (!level_type == LEVEL_DUNGEON)
        return;

    if (player_in_branch( BRANCH_MAIN_DUNGEON ))
    {
        // stair to HELL
        if (dlevel >= 20 && dlevel <= 27)
            place_specific_stair(DNGN_ENTER_HELL);

        // stair to PANDEMONIUM
        if (dlevel >= 20 && dlevel <= 50 && (dlevel == 23 || one_chance_in(4)))
            place_specific_stair(DNGN_ENTER_PANDEMONIUM);

        // stairs to ABYSS
        if (dlevel >= 20 && dlevel <= 30 && (dlevel == 24 || one_chance_in(3)))
            place_specific_stair(DNGN_ENTER_ABYSS);

        // level 26: replaces all down stairs with staircases to Zot:
        if (dlevel == 26)
        {
            for (sx = 1; sx < GXM; sx++)
            {
                for (sy = 1; sy < GYM; sy++)
                {
                    if (grd[sx][sy] >= DNGN_STONE_STAIRS_DOWN_I
                            && grd[sx][sy] <= DNGN_ROCK_STAIRS_DOWN)
                    {
                        grd[sx][sy] = DNGN_ENTER_ZOT;
                    }
                }
            }
        }
    }

    // place actual branch entrances
    for (int branch = 0; branch < 30; branch++)
    {
        stair = 0;
        entrance = 100;

        if (you.branch_stairs[branch] == 100)   // set in newgame
            break;

        if (you.branch_stairs[branch] != dlevel)
            continue;

        // decide if this branch leaves from this level
        switch(branch)
        {
            case STAIRS_ORCISH_MINES:
            case STAIRS_HIVE:
            case STAIRS_LAIR:
            case STAIRS_VAULTS:
            case STAIRS_ECUMENICAL_TEMPLE:
                entrance = BRANCH_MAIN_DUNGEON;
                break;

            case STAIRS_SLIME_PITS:
            case STAIRS_SWAMP:
            case STAIRS_SNAKE_PIT:
                entrance = BRANCH_LAIR;
                break;

            case STAIRS_ELVEN_HALLS:
                entrance = BRANCH_ORCISH_MINES;
                break;

            case STAIRS_CRYPT:
            case STAIRS_HALL_OF_BLADES:
                entrance = BRANCH_VAULTS;
                break;

            case STAIRS_TOMB:
                entrance = BRANCH_CRYPT;
                break;

            default:
                entrance = 100;
                break;
        }

        if (you.where_are_you != entrance)
            continue;

        stair = branch + DNGN_ENTER_ORCISH_MINES;
        place_specific_stair(stair);
    }   // end loop - possible branch entrances
}

static void make_trail(int xs, int xr, int ys, int yr, int corrlength, 
                       int intersect_chance, int no_corr, unsigned char begin, 
                       unsigned char end)
{
    int x_start, y_start;                   // begin point
    int x_ps, y_ps;                         // end point
    int finish = 0;
    int length = 0;
    int temp_rand;

    // temp positions
    int dir_x = 0;
    int dir_y = 0;       
    int dir_x2, dir_y2; 

    do
    {
        x_start = xs + random2(xr);
        y_start = ys + random2(yr);
    }
    while (grd[x_start][y_start] != DNGN_ROCK_WALL
                && grd[x_start][y_start] != DNGN_FLOOR);

    // assign begin feature
    if (begin != 0)
        grd[x_start][y_start] = begin;
    x_ps = x_start;
    y_ps = y_start;

    // wander
    do                          // (while finish < no_corr)
    {
        dir_x2 = ((x_ps < 15) ? 1 : 0);

        if (x_ps > 65)
            dir_x2 = -1;

        dir_y2 = ((y_ps < 15) ? 1 : 0);

        if (y_ps > 55)
            dir_y2 = -1;

        temp_rand = random2(10);

        // Put something in to make it go to parts of map it isn't in now
        if (coinflip())
        {
            if (dir_x2 != 0 && temp_rand < 6)
                dir_x = dir_x2;

            if (dir_x2 == 0 || temp_rand >= 6)
                dir_x = (coinflip()? -1 : 1);

            dir_y = 0;
        }
        else
        {
            if (dir_y2 != 0 && temp_rand < 6)
                dir_y = dir_y2;

            if (dir_y2 == 0 || temp_rand >= 6)
                dir_y = (coinflip()? -1 : 1);

            dir_x = 0;
        }

        if (dir_x == 0 && dir_y == 0)
            continue;

        if (x_ps < 8)
        {
            dir_x = 1;
            dir_y = 0;
        }

        if (y_ps < 8)
        {
            dir_y = 1;
            dir_x = 0;
        }

        if (x_ps > (GXM - 8))
        {
            dir_x = -1;
            dir_y = 0;
        }

        if (y_ps > (GYM - 8))
        {
            dir_y = -1;
            dir_x = 0;
        }

        // corridor length.. change only when going vertical?
        if (dir_x == 0 || length == 0)
            length = random2(corrlength) + 2;

        int bi = 0;

        for (bi = 0; bi < length; bi++)
        {
            // Below, I've changed the values of the unimportant variable from
            // 0 to random2(3) - 1 to avoid getting stuck on the "stuck!" bit
            if (x_ps < 9)
            {
                dir_y = 0;      //random2(3) - 1;
                dir_x = 1;
            }

            if (x_ps > (GXM - 9))
            {
                dir_y = 0;      //random2(3) - 1;
                dir_x = -1;
            }

            if (y_ps < 9)
            {
                dir_y = 1;
                dir_x = 0;      //random2(3) - 1;
            }

            if (y_ps > (GYM - 9))
            {
                dir_y = -1;
                dir_x = 0;      //random2(3) - 1;
            }

            // don't interfere with special rooms
            if (grd[x_ps + dir_x][y_ps + dir_y] == DNGN_BUILDER_SPECIAL_WALL)
                break;

            // see if we stop due to intersection with another corridor/room
            if (grd[x_ps + 2 * dir_x][y_ps + 2 * dir_y] == DNGN_FLOOR
                && !one_chance_in(intersect_chance))
                break;

            x_ps += dir_x;
            y_ps += dir_y;

            if (grd[x_ps][y_ps] == DNGN_ROCK_WALL)
                grd[x_ps][y_ps] = DNGN_FLOOR;
        }

        if (finish == no_corr - 1 && grd[x_ps][y_ps] != DNGN_FLOOR)
            finish -= 2;

        finish++;
    }
    while (finish < no_corr);

    // assign end feature
    if (end != 0)
        grd[x_ps][y_ps] = end;
}

static int good_door_spot(int x, int y)
{
    if ((grd[x][y] > DNGN_WATER_X && grd[x][y] < DNGN_ENTER_PANDEMONIUM)
        || grd[x][y] == DNGN_CLOSED_DOOR)
    {
        return 1;
    }

    return 0;
}

// return TRUE if a room was made successfully
static bool make_room(int sx,int sy,int ex,int ey,int max_doors, int doorlevel)
{
    int find_door = 0;
    int diag_door = 0;
    int rx,ry;

    // check top & bottom for possible doors
    for (rx = sx; rx <= ex; rx++)
    {
        find_door += good_door_spot(rx,sy);
        find_door += good_door_spot(rx,ey);
    }

    // check left and right for possible doors
    for (ry = sy+1; ry < ey; ry++)
    {
        find_door += good_door_spot(sx,ry);
        find_door += good_door_spot(ex,ry);
    }

    diag_door += good_door_spot(sx,sy);
    diag_door += good_door_spot(ex,sy);
    diag_door += good_door_spot(sx,ey);
    diag_door += good_door_spot(ex,ey);

    if ((diag_door + find_door) > 1 && max_doors == 1)
        return false;

    if (find_door == 0 || find_door > max_doors)
        return false;

    // look for 'special' rock walls - don't interrupt them
    if (find_in_area(sx,sy,ex,ey,DNGN_BUILDER_SPECIAL_WALL))
        return false;

    // convert the area to floor
    for (rx=sx; rx<=ex; rx++)
    {
        for(ry=sy; ry<=ey; ry++)
        {
            if (grd[rx][ry] <= DNGN_FLOOR)
                grd[rx][ry] = DNGN_FLOOR;
        }
    }

    // put some doors on the sides (but not in corners),
    // where it makes sense to do so.
    for(ry=sy+1; ry<ey; ry++)
    {
        // left side
        if (grd[sx-1][ry] == DNGN_FLOOR
            && grd[sx-1][ry-1] < DNGN_LAST_SOLID_TILE
            && grd[sx-1][ry+1] < DNGN_LAST_SOLID_TILE)
        {
            if (random2(10) < doorlevel)
                grd[sx-1][ry] = DNGN_CLOSED_DOOR;
        }

        // right side
        if (grd[ex+1][ry] == DNGN_FLOOR
            && grd[ex+1][ry-1] < DNGN_LAST_SOLID_TILE
            && grd[ex+1][ry+1] < DNGN_LAST_SOLID_TILE)
        {
            if (random2(10) < doorlevel)
                grd[ex+1][ry] = DNGN_CLOSED_DOOR;
        }
    }

    // put some doors on the top & bottom
    for(rx=sx+1; rx<ex; rx++)
    {
        // top
        if (grd[rx][sy-1] == DNGN_FLOOR
            && grd[rx-1][sy-1] < DNGN_LAST_SOLID_TILE
            && grd[rx+1][sy-1] < DNGN_LAST_SOLID_TILE)
        {
            if (random2(10) < doorlevel)
                grd[rx][sy-1] = DNGN_CLOSED_DOOR;
        }

        // bottom
        if (grd[rx][ey+1] == DNGN_FLOOR
            && grd[rx-1][ey+1] < DNGN_LAST_SOLID_TILE
            && grd[rx+1][ey+1] < DNGN_LAST_SOLID_TILE)
        {
            if (random2(10) < doorlevel)
                grd[rx][ey+1] = DNGN_CLOSED_DOOR;
        }
    }

    return true;
}                               //end make_room()

static void builder_monsters(int level_number, char level_type, int mon_wanted)
{
    int i = 0;
    int totalplaced = 0;
    int not_used=0;
    int x,y;
    int lava_spaces, water_spaces;
    int aq_creatures;
    int swimming_things[4];

    if (level_type == LEVEL_PANDEMONIUM)
        return;

    for (i = 0; i < mon_wanted; i++)
    {
        if (place_monster( not_used, RANDOM_MONSTER, level_number, BEH_SLEEP,
                           MHITNOT, false, 1, 1, true ))
        {
            totalplaced++;
        }
    }

    // Unique beasties:
    int which_unique;

    if (level_number > 0
        && you.level_type == LEVEL_DUNGEON  // avoid generating on temp levels
        && !player_in_hell()
        && !player_in_branch( BRANCH_ORCISH_MINES )
        && !player_in_branch( BRANCH_HIVE )
        && !player_in_branch( BRANCH_LAIR )
        && !player_in_branch( BRANCH_SLIME_PITS )
        && !player_in_branch( BRANCH_ECUMENICAL_TEMPLE ))
    {
        while(one_chance_in(3))
        {
            which_unique = -1;   //     30 in total

            while(which_unique < 0 || you.unique_creatures[which_unique])
            {
                // sometimes,  we just quit if a unique is already placed.
                if (which_unique >= 0 && !one_chance_in(3))
                {
                    which_unique = -1;
                    break;
                }

                which_unique = ((level_number > 19) ? 20 + random2(11) :
                                (level_number > 16) ? 13 + random2(10) :
                                (level_number > 13) ?  9 + random2( 9) :
                                (level_number >  9) ?  6 + random2( 5) :
                                (level_number >  7) ?  4 + random2( 4) :
                                (level_number >  3) ?  2 + random2( 4)
                                                    : random2(4));
            }

            // usually, we'll have quit after a few tries. Make sure we don't
            // create unique[-1] by accident.
            if (which_unique < 0)
                break;

            // note: unique_creatures 40 + used by unique demons
            if (place_monster( not_used, 280 + which_unique, level_number, 
                               BEH_SLEEP, MHITNOT, false, 1, 1, true ))
            {
                totalplaced++;
            }
        }
    }

    // do aquatic and lava monsters:

    // count the number of lava and water tiles {dlb}:
    lava_spaces = 0;
    water_spaces = 0;

    for (x = 0; x < GXM; x++)
    {
        for (y = 0; y < GYM; y++)
        {
            if (grd[x][y] == DNGN_LAVA)
            {
                lava_spaces++;
            }
            else if (grd[x][y] == DNGN_DEEP_WATER
                     || grd[x][y] == DNGN_SHALLOW_WATER)
            {
                water_spaces++;
            }
        }
    }

    if (lava_spaces > 49)
    {
        for (i = 0; i < 4; i++)
        {
            swimming_things[i] = MONS_LAVA_WORM + random2(3);

            //mv: this is really ugly, but easiest
            //IMO generation of water/lava beasts should be changed,
            //because we want data driven code and not things like it
            if (one_chance_in(30)) 
                swimming_things[i] = MONS_SALAMANDER;
        }

        aq_creatures = random2avg(9, 2) + (random2(lava_spaces) / 10);

        if (aq_creatures > 15)
            aq_creatures = 15;

        for (i = 0; i < aq_creatures; i++)
        {
            if (place_monster( not_used, swimming_things[ random2(4) ],
                               level_number, BEH_SLEEP, MHITNOT, 
                               false, 1, 1, true ))
            {
                totalplaced++;
            }

            if (totalplaced > 99)
                break;
        }
    }

    if (water_spaces > 49)
    {
        for (i = 0; i < 4; i++)
        {
            // mixing enums and math ticks me off !!! 15jan2000 {dlb}
            swimming_things[i] = MONS_BIG_FISH + random2(4);

            // swamp worms and h2o elementals generated below: {dlb}
            if (player_in_branch( BRANCH_SWAMP ) && !one_chance_in(3))
                swimming_things[i] = MONS_SWAMP_WORM;
        }

        if (level_number >= 25 && one_chance_in(5))
            swimming_things[0] = MONS_WATER_ELEMENTAL;

        if (player_in_branch( BRANCH_COCYTUS ))
            swimming_things[3] = MONS_WATER_ELEMENTAL;

        aq_creatures = random2avg(9, 2) + (random2(water_spaces) / 10);

        if (aq_creatures > 15)
            aq_creatures = 15;

        for (i = 0; i < aq_creatures; i++)
        {
            if (place_monster( not_used, swimming_things[ random2(4) ],
                               level_number, BEH_SLEEP, MHITNOT, 
                               false, 1, 1, true ))
            {
                totalplaced++;
            }

            if (totalplaced > 99)
                break;
        }
    }
}

static void builder_items(int level_number, char level_type, int items_wanted)
{
    UNUSED( level_type );

    int i = 0;
    unsigned char specif_type = OBJ_RANDOM;
    int items_levels = level_number;
    int item_no;

    if (player_in_branch( BRANCH_VAULTS ))
    {
        items_levels *= 15;
        items_levels /= 10;
    }
    else if (player_in_branch( BRANCH_ORCISH_MINES ))
    {
        specif_type = OBJ_GOLD; /* lots of gold in the orcish mines */
    }

    if (player_in_branch( BRANCH_VESTIBULE_OF_HELL )
        || player_in_hell()
        || player_in_branch( BRANCH_SLIME_PITS )
        || player_in_branch( BRANCH_HALL_OF_BLADES )
        || player_in_branch( BRANCH_ECUMENICAL_TEMPLE ))
    {
        /* No items in hell, the slime pits, the Hall */
        return;
    }
    else
    {
        for (i = 0; i < items_wanted; i++)
            items( 1, specif_type, OBJ_RANDOM, false, items_levels, 250 );

        // Make sure there's a very good chance of a knife being placed
        // in the first five levels, but not a guarantee of one.  The
        // intent of this is to reduce the advantage that "cutting"
        // starting weapons have.  -- bwr
        if (player_in_branch( BRANCH_MAIN_DUNGEON )
            && level_number < 5 && coinflip())
        {
            item_no = items( 0, OBJ_WEAPONS, WPN_KNIFE, false, 0, 250 );

            // Guarantee that the knife is uncursed and non-special
            if (item_no != NON_ITEM)
            {
                mitm[item_no].plus = 0;
                mitm[item_no].plus2 = 0;
                mitm[item_no].flags = 0;   // no id, no race/desc, no curse
                mitm[item_no].special = 0; // no ego type
            }
        }
    }
}

// the entire intent of this function is to find a
// hallway from a special room to a floor space somewhere,
// changing the special room wall (DNGN_BUILDER_SPECIAL_WALL)
// to a closed door,  and normal rock wall to pre-floor.
// Anything that might otherwise block the hallway is changed
// to pre-floor.
static void specr_2(spec_room &sr)
{
    int bkout = 0;
    int cx = 0, cy = 0;
    int sx = 0, sy = 0;
    int dx = 0, dy = 0;
    int i,j;

    // paranoia -- how did we get here if there's no actual special room??
    if (!sr.created)
        return;

  grolko:

    if (bkout > 100)
        return;

    switch (random2(4))
    {
    case 0:
        // go up from north edge
        cx = sr.x1 + (random2(sr.x2 - sr.x1));
        cy = sr.y1;
        dx = 0;
        dy = -1;
        break;
    case 1:
        // go down from south edge
        cx = sr.x1 + (random2(sr.x2 - sr.x1));
        cy = sr.y2;
        dx = 0;
        dy = 1;
        break;
    case 2:
        // go left from west edge
        cy = sr.y1 + (random2(sr.y2 - sr.y1));
        cx = sr.x1;
        dx = -1;
        dy = 0;
        break;
    case 3:
        // go right from east edge
        cy = sr.y1 + (random2(sr.y2 - sr.y1));
        cx = sr.x2;
        dx = 1;
        dy = 0;
        break;
    }

    sx = cx;
    sy = cy;

    for (i = 0; i < 100; i++)
    {
        sx += dx;
        sy += dy;

        // quit if we run off the map before finding floor
        if (sx < 6 || sx > (GXM - 7) || sy < 6 || sy > (GYM - 7))
        {
            bkout++;
            goto grolko;
        }

        // look around for floor
        if (i > 0)
        {
            if (grd[sx + 1][sy] == DNGN_FLOOR)
                break;
            if (grd[sx][sy + 1] == DNGN_FLOOR)
                break;
            if (grd[sx - 1][sy] == DNGN_FLOOR)
                break;
            if (grd[sx][sy - 1] == DNGN_FLOOR)
                break;
        }
    }

    sx = cx;
    sy = cy;

    for (j = 0; j < i + 2; j++)
    {
        if (grd[sx][sy] == DNGN_BUILDER_SPECIAL_WALL)
            grd[sx][sy] = DNGN_CLOSED_DOOR;

        if (j > 0 && grd[sx + dx][sy + dy] > DNGN_ROCK_WALL
            && grd[sx + dx][sy + dy] < DNGN_FLOOR)
            grd[sx][sy] = DNGN_BUILDER_SPECIAL_FLOOR;

        if (grd[sx][sy] == DNGN_ROCK_WALL)
            grd[sx][sy] = DNGN_BUILDER_SPECIAL_FLOOR;

        sx += dx;
        sy += dy;
    }

    sr.hooked_up = true;
}                               // end specr_2()

static void special_room(int level_number, spec_room &sr)
{
    char spec_room_type = SROOM_LAIR_KOBOLD;
    int lev_mons;
    int thing_created = 0;
    int x, y;

    unsigned char obj_type = OBJ_RANDOM;  // used in calling items() {dlb}
    unsigned char i;        // general purpose loop variable {dlb}
    int temp_rand = 0;          // probability determination {dlb}

    FixedVector < int, 10 > mons_alloc; // was [20] {dlb}

    char lordx = 0, lordy = 0;

    // overwrites anything;  this function better be called early on during
    // creation..
    int room_x1 = 8 + random2(55);
    int room_y1 = 8 + random2(45);
    int room_x2 = room_x1 + 4 + random2avg(6,2);
    int room_y2 = room_y1 + 4 + random2avg(6,2);

    // do special walls & floor
    make_box( room_x1, room_y1, room_x2, room_y2, 
              DNGN_BUILDER_SPECIAL_FLOOR, DNGN_BUILDER_SPECIAL_WALL );

    // set up passed in spec_room structure
    sr.created = true;
    sr.hooked_up = false;
    sr.x1 = room_x1 + 1;
    sr.x2 = room_x2 - 1;
    sr.y1 = room_y1 + 1;
    sr.y2 = room_y2 - 1;

    if (level_number < 7)
        spec_room_type = SROOM_LAIR_KOBOLD;
    else
    {
        spec_room_type = random2(NUM_SPECIAL_ROOMS);

        if (level_number < 23 && one_chance_in(4))
            spec_room_type = SROOM_BEEHIVE;

        if ((level_number > 13 && spec_room_type == SROOM_LAIR_KOBOLD)
            || (level_number < 16 && spec_room_type == SROOM_MORGUE)
            || (level_number < 17 && one_chance_in(4)))
        {
            spec_room_type = SROOM_LAIR_ORC;
        }

        if (level_number > 19 && coinflip())
            spec_room_type = SROOM_MORGUE;
    }


    switch (spec_room_type)
    {
    case SROOM_LAIR_ORC:
        // determine which monster array to generate {dlb}:
        lev_mons = ((level_number > 24) ? 3 :
                    (level_number > 15) ? 2 :
                    (level_number >  9) ? 1
                                        : 0);

        // fill with baseline monster type {dlb}:
        for (i = 0; i < 10; i++)
        {
            mons_alloc[i] = MONS_ORC;
        }

        // fill in with special monster types {dlb}:
        switch (lev_mons)
        {
        case 0:
            mons_alloc[9] = MONS_ORC_WARRIOR;
            break;
        case 1:
            mons_alloc[8] = MONS_ORC_WARRIOR;
            mons_alloc[9] = MONS_ORC_WARRIOR;
            break;
        case 2:
            mons_alloc[6] = MONS_ORC_KNIGHT;
            mons_alloc[7] = MONS_ORC_WARRIOR;
            mons_alloc[8] = MONS_ORC_WARRIOR;
            mons_alloc[9] = MONS_OGRE;
            break;
        case 3:
            mons_alloc[2] = MONS_ORC_WARRIOR;
            mons_alloc[3] = MONS_ORC_WARRIOR;
            mons_alloc[4] = MONS_ORC_WARRIOR;
            mons_alloc[5] = MONS_ORC_KNIGHT;
            mons_alloc[6] = MONS_ORC_KNIGHT;
            mons_alloc[7] = MONS_OGRE;
            mons_alloc[8] = MONS_OGRE;
            mons_alloc[9] = MONS_TROLL;
            break;
        }

        // place monsters and give them items {dlb}:
        for (x = sr.x1; x <= sr.x2; x++)
        {
            for (y = sr.y1; y <= sr.y2; y++)
            {
                if (one_chance_in(4))
                    continue;

                mons_place( mons_alloc[random2(10)], BEH_SLEEP, MHITNOT, 
                            true, x, y );
            }
        }
        break;

    case SROOM_LAIR_KOBOLD:
        lordx = sr.x1 + random2(sr.x2 - sr.x1);
        lordy = sr.y1 + random2(sr.y2 - sr.y1);

        // determine which monster array to generate {dlb}:
        lev_mons = ((level_number < 4) ? 0 :
                    (level_number < 6) ? 1 : (level_number < 9) ? 2 : 3);

        // fill with baseline monster type {dlb}:
        for (i = 0; i < 10; i++)
        {
            mons_alloc[i] = MONS_KOBOLD;
        }

        // fill in with special monster types {dlb}:
        // in this case, they are uniformly the same {dlb}:
        for (i = (7 - lev_mons); i < 10; i++)
        {
            mons_alloc[i] = MONS_BIG_KOBOLD;
        }

        // place monsters and give them items {dlb}:
        for (x = sr.x1; x <= sr.x2; x++)
        {
            for (y = sr.y1; y <= sr.y2; y++)
            {
                if (one_chance_in(4))
                    continue;

                // we'll put the boss down later.
                if (x == lordx && y == lordy)
                    continue;

                mons_place( mons_alloc[random2(10)], BEH_SLEEP, MHITNOT,
                            true, x, y );
            }
        }

        // put the boss monster down
        mons_place( MONS_BIG_KOBOLD, BEH_SLEEP, MHITNOT, true, lordx, lordy );

        break;

    case SROOM_TREASURY:
        // should only appear in deep levels, with a guardian
        // Maybe have several types of treasure room?
        // place treasure {dlb}:
        for (x = sr.x1; x <= sr.x2; x++)
        {
            for (y = sr.y1; y <= sr.y2; y++)
            {
                temp_rand = random2(11);

                obj_type = ((temp_rand > 8) ? OBJ_WEAPONS :       // 2 in 11
                            (temp_rand > 6) ? OBJ_ARMOUR :        // 2 in 11
                            (temp_rand > 5) ? OBJ_MISSILES :      // 1 in 11
                            (temp_rand > 4) ? OBJ_WANDS :         // 1 in 11
                            (temp_rand > 3) ? OBJ_SCROLLS :       // 1 in 11
                            (temp_rand > 2) ? OBJ_JEWELLERY :     // 1 in 11
                            (temp_rand > 1) ? OBJ_BOOKS :         // 1 in 11
                            (temp_rand > 0) ? OBJ_STAVES          // 1 in 11
                                            : OBJ_POTIONS);       // 1 in 11

                thing_created = items( 1, obj_type, OBJ_RANDOM, true,
                                       level_number * 3, 250 );

                if (thing_created != NON_ITEM)
                {
                    mitm[thing_created].x = x;
                    mitm[thing_created].y = y;
                }
            }
        }

        // place guardian {dlb}:
        mons_place( MONS_GUARDIAN_NAGA, BEH_SLEEP, MHITNOT, true, 
                    sr.x1 + random2( sr.x2 - sr.x1 ), 
                    sr.y1 + random2( sr.y2 - sr.y1 ) );

        break;

    case SROOM_BEEHIVE:
        beehive(sr);
        break;

    case SROOM_MORGUE:
        morgue(sr);
        break;
    }
}                               // end special_room()

// fills a special room with bees
static void beehive(spec_room &sr)
{
    int i;
    int x,y;

    for (x = sr.x1; x <= sr.x2; x++)
    {
        for (y = sr.y1; y <= sr.y2; y++)
        {
            if (coinflip())
                continue;

            i = get_item_slot();
            if (i == NON_ITEM)
                goto finished_food;

            mitm[i].quantity = 1;
            mitm[i].base_type = OBJ_FOOD;
            mitm[i].sub_type = (one_chance_in(25) ? FOOD_ROYAL_JELLY
                                                  : FOOD_HONEYCOMB);
            mitm[i].x = x;
            mitm[i].y = y;

            item_colour( mitm[i] );
        }
    }


  finished_food:

    int queenx = sr.x1 + random2(sr.x2 - sr.x1);
    int queeny = sr.y1 + random2(sr.y2 - sr.y1);

    for (x = sr.x1; x <= sr.x2; x++)
    {
        for (y = sr.y1; y <= sr.y2; y++)
        {
            if (x == queenx && y == queeny)
                continue;

            // the hive is chock full of bees!

            mons_place( one_chance_in(7) ? MONS_KILLER_BEE_LARVA 
                                         : MONS_KILLER_BEE,
                        BEH_SLEEP, MHITNOT,  true, x, y );
        }
    }

    mons_place( MONS_QUEEN_BEE, BEH_SLEEP, MHITNOT, true, queenx, queeny );
}                               // end beehive()

static void build_minivaults(int level_number, int force_vault)
{
    // for some weird reason can't put a vault on level 1, because monster equip
    // isn't generated.
    int altar_count = 0;

    FixedVector < char, 7 > acq_item_class;
    // hack - passing chars through '...' promotes them to ints, which
    // barfs under gcc in fixvec.h.  So don't.
    acq_item_class[0] = OBJ_WEAPONS;
    acq_item_class[1] = OBJ_ARMOUR;
    acq_item_class[2] = OBJ_WEAPONS;
    acq_item_class[3] = OBJ_JEWELLERY;
    acq_item_class[4] = OBJ_BOOKS;
    acq_item_class[5] = OBJ_STAVES;
    acq_item_class[6] = OBJ_MISCELLANY;

    FixedVector < int, 7 > mons_array(RANDOM_MONSTER,
                                      RANDOM_MONSTER,
                                      RANDOM_MONSTER,
                                      RANDOM_MONSTER,
                                      RANDOM_MONSTER,
                                      RANDOM_MONSTER,
                                      RANDOM_MONSTER);

    char vgrid[81][81];

    if (force_vault == 200)
    {
        force_vault = 200 + random2(37);
    }

    vault_main(vgrid, mons_array, force_vault, level_number);

    int vx, vy;
    int v1x, v1y;

    /* find a target area which can be safely overwritten: */
    while(1)
    {
        //if ( one_chance_in(1000) ) return;
        v1x = 12 + random2(45);
        v1y = 12 + random2(35);

        for (vx = v1x; vx < v1x + 12; vx++)
        {
            for (vy = v1y; vy < v1y + 12; vy++)
            {
                if (one_chance_in(2000))
                    return;

                if ((grd[vx][vy] != DNGN_FLOOR
                        && grd[vx][vy] != DNGN_ROCK_WALL
                        && grd[vx][vy] != DNGN_CLOSED_DOOR
                        && grd[vx][vy] != DNGN_SECRET_DOOR)
                    || igrd[vx][vy] != NON_ITEM
                    || mgrd[vx][vy] != NON_MONSTER)
                {
                    goto out_of_check;
                }
            }
        }

        /* must not be completely isolated: */
        for (vx = v1x; vx < v1x + 12; vx++)
        {
            //  if (vx != v1x && vx != v1x + 12) continue;
            for (vy = v1y; vy < v1y + 12; vy++)
            {
                //   if (vy != v1y && vy != v1y + 12) continue;
                if (grd[vx][vy] == DNGN_FLOOR
                    || grd[vx][vy] == DNGN_CLOSED_DOOR
                    || grd[vx][vy] == DNGN_SECRET_DOOR)
                    goto break_out;
            }
        }

      out_of_check:
        continue;

      break_out:
        break;
    }

    for (vx = v1x; vx < v1x + 12; vx++)
    {
        for (vy = v1y; vy < v1y + 12; vy++)
        {
            grd[vx][vy] = vgrid[vx - v1x][vy - v1y];
        }
    }

    // these two are throwaways:
    int initial_x, initial_y;
    int num_runes = 0;

    // paint the minivault onto the grid
    for (vx = v1x; vx < v1x + 12; vx++)
    {
        for (vy = v1y; vy < v1y + 12; vy++)
        {
            altar_count = vault_grid( level_number, vx, vy, altar_count, 
                                      acq_item_class, mons_array, 
                                      grd[vx][vy], initial_x, initial_y, 
                                      force_vault, num_runes );
        }
    }
}                               // end build_minivaults()

static void build_vaults(int level_number, int force_vault)
{
    // for some weird reason can't put a vault on level 1, because monster equip
    // isn't generated.
    int i,j;                // general loop variables
    int altar_count = 0;
    FixedVector < char, 10 > stair_exist;
    char stx, sty;
    int initial_x=0, initial_y=0;

    FixedVector < char, 7 > acq_item_class;
    // hack - passing chars through '...' promotes them to ints, which
    // barfs under gcc in fixvec.h.  So don't. -- GDL
    acq_item_class[0] = OBJ_WEAPONS;
    acq_item_class[1] = OBJ_ARMOUR;
    acq_item_class[2] = OBJ_WEAPONS;
    acq_item_class[3] = OBJ_JEWELLERY;
    acq_item_class[4] = OBJ_BOOKS;
    acq_item_class[5] = OBJ_STAVES;
    acq_item_class[6] = OBJ_MISCELLANY;

    FixedVector < int, 7 > mons_array(RANDOM_MONSTER,
                                      RANDOM_MONSTER,
                                      RANDOM_MONSTER,
                                      RANDOM_MONSTER,
                                      RANDOM_MONSTER,
                                      RANDOM_MONSTER,
                                      RANDOM_MONSTER);

    char roomsss = 10 + random2(90);
    char which_room = 0;

    bool exclusive = (one_chance_in(10) ? false : true);

    //bool exclusive2 = coinflip();    // usage commented out below {dlb}

    char vgrid[81][81];

    char gluggy = vault_main(vgrid, mons_array, force_vault, level_number);

    int vx, vy;
    int v1x = 0, v1y = 0, v2x = 0, v2y = 0;

    //int item_made;

    char dig_dir_x = 0;
    char dig_dir_y = 0;
    char dig_place_x = 0;
    char dig_place_y = 0;
    int  num_runes = 0;

    // note: assumes *no* previous item (I think) or monster (definitely)
    // placement
    for (vx = 0; vx < GXM; vx++)
    {
        for (vy = 0; vy < GYM; vy++)
        {
            altar_count = vault_grid( level_number, vx, vy, altar_count, 
                                      acq_item_class, mons_array, 
                                      vgrid[vy][vx], initial_x, initial_y,
                                      force_vault, num_runes );
        }
    }

    switch (gluggy)
    {
    case MAP_NORTH:
        v1x = 1;
        v2x = GXM;
        v1y = 1;
        v2y = 35;
        initial_y++;
        dig_dir_x = 0;
        dig_dir_y = 1;
        break;

    case MAP_NORTHWEST:
        v1x = 1;
        v2x = 40;
        v1y = 1;
        v2y = 35;
        initial_y++;
        dig_dir_x = 1;
        dig_dir_y = 0;
        break;

    case MAP_NORTHEAST:
        v1x = 40;
        v2x = GXM;
        v1y = 1;
        v2y = 35;
        initial_y++;
        dig_dir_x = -1;
        dig_dir_y = 0;
        break;

    case MAP_SOUTHWEST:
        v1x = 1;
        v2x = 40;
        v1y = 35;
        v2y = GYM;
        initial_y--;
        dig_dir_x = 0;
        dig_dir_y = -1;
        break;

    case MAP_SOUTHEAST:
        v1x = 40;
        v2x = GXM;
        v1y = 35;
        v2y = GYM;
        initial_y--;
        dig_dir_x = 0;
        dig_dir_y = -1;
        break;

    case MAP_ENCOMPASS:
        return;

    case MAP_NORTH_DIS:
        v1x = 1;
        v2x = GXM;
        v1y = 1;
        v2y = 35;
        plan_4(1, 1, 80, 35, DNGN_METAL_WALL);
        goto vstair;
    }

    char cnx, cny;
    char romx1[30], romy1[30], romx2[30], romy2[30];

    for (i = 0; i < roomsss; i++)
    {
        do
        {
            romx1[which_room] = 10 + random2(50);
            romy1[which_room] = 10 + random2(40);
            romx2[which_room] = romx1[which_room] + 2 + random2(8);
            romy2[which_room] = romy1[which_room] + 2 + random2(8);
        }
        while ((romx1[which_room] >= v1x && romx1[which_room] <= v2x
                   && romy1[which_room] >= v1y && romy1[which_room] <= v2y)
               || (romx2[which_room] >= v1x && romx2[which_room] <= v2x
                   && romy2[which_room] >= v1y && romy2[which_room] <= v2y));

        if (i == 0)
        {
            join_the_dots(initial_x, initial_y, romx1[which_room], romy1[which_room],
                v1x, v1y, v2x, v2y);
        }
        else if (exclusive)
        {
            for (cnx = romx1[which_room] - 1; cnx < romx2[which_room] + 1;
                                                                        cnx++)
            {
                for (cny = romy1[which_room] - 1; cny < romy2[which_room] + 1;
                                                                        cny++)
                {
                    if (grd[cnx][cny] != DNGN_ROCK_WALL)
                        goto continuing;
                }
            }
        }

        replace_area(romx1[which_room], romy1[which_room], romx2[which_room],
                   romy2[which_room], DNGN_ROCK_WALL, DNGN_FLOOR);

        if (which_room > 0)     // && !exclusive2
        {
            const int rx1 = romx1[which_room];
            const int rx2 = romx2[which_room];
            const int prev_rx1 = romx1[which_room - 1];
            const int prev_rx2 = romx2[which_room - 1];

            const int ry1 = romy1[which_room];
            const int ry2 = romy2[which_room];
            const int prev_ry1 = romy1[which_room - 1];
            const int prev_ry2 = romy2[which_room - 1];

            join_the_dots( rx1 + random2( rx2 - rx1 ),
                           ry1 + random2( ry2 - ry1 ),
                           prev_rx1 + random2( prev_rx2 - prev_rx1 ),
                           prev_ry1 + random2( prev_ry2 - prev_ry1 ),
                           v1x, v1y, v2x, v2y);
        }

        which_room++;

        if (which_room >= 29)
            break;

      continuing:
        continue;               // next i loop

    }

  vstair:
    dig_place_x = initial_x;
    dig_place_y = initial_y;

    if (gluggy != MAP_NORTH_DIS)
    {
        for (i = 0; i < 40; i++)
        {
            dig_place_x += dig_dir_x;
            dig_place_y += dig_dir_y;

            if (dig_place_x < 10 || dig_place_x > (GXM - 10)
                || dig_place_y < 10 || dig_place_y > (GYM - 10))
            {
                break;
            }

            if (grd[dig_place_x][dig_place_y] == DNGN_ROCK_WALL)
                grd[dig_place_x][dig_place_y] = DNGN_FLOOR;
        }
    }

    unsigned char pos_x, pos_y;

    for (stx = 0; stx < 10; stx++)
        stair_exist[stx] = 0;

    for (stx = 0; stx < GXM; stx++)
    {
        for (sty = 0; sty < GYM; sty++)
        {
            if (grd[stx][sty] >= DNGN_STONE_STAIRS_DOWN_I
                    && grd[stx][sty] <= DNGN_ROCK_STAIRS_UP)
            {
                stair_exist[grd[stx][sty] - 82] = 1;
            }
        }
    }

    if (player_in_branch( BRANCH_DIS ))
    {
        for (sty = 0; sty < 5; sty++)
            stair_exist[sty] = 1;

        for (sty = 6; sty < 10; sty++)
            stair_exist[sty] = 0;
    }

    for (j = 0; j < (coinflip()? 4 : 3); j++)
    {
        for (i = 0; i < 2; i++)
        {

            if (stair_exist[(82 + j + (i * 4)) - 82] == 1)   // does this look funny to *you*? {dlb}
                continue;

            do
            {
                pos_x = 10 + random2(GXM - 20);
                pos_y = 10 + random2(GYM - 20);
            }
            while (grd[pos_x][pos_y] != DNGN_FLOOR
                   || (pos_x >= v1x && pos_x <= v2x && pos_y >= v1y
                       && pos_y <= v2y));

            grd[pos_x][pos_y] = j + ((i == 0) ? DNGN_STONE_STAIRS_DOWN_I
                                              : DNGN_STONE_STAIRS_UP_I);
        }
    }
}                               // end build_vaults()

// returns altar_count - seems rather odd to me to force such a return
// when I believe the value is only used in the case of the ecumenical
// temple - oh, well... {dlb}
static int vault_grid( int level_number, int vx, int vy, int altar_count,
                       FixedVector < char, 7 > &acq_item_class, 
                       FixedVector < int, 7 > &mons_array,
                       char vgrid, int &initial_x, int &initial_y, 
                       int force_vault, int &num_runes)
{
    int not_used;

    // first, set base tile for grids {dlb}:
    grd[vx][vy] = ((vgrid == 'x') ? DNGN_ROCK_WALL :
                   (vgrid == 'X') ? DNGN_PERMAROCK_WALL :
                   (vgrid == 'c') ? DNGN_STONE_WALL :
                   (vgrid == 'v') ? DNGN_METAL_WALL :
                   (vgrid == 'b') ? DNGN_GREEN_CRYSTAL_WALL :
                   (vgrid == 'a') ? DNGN_WAX_WALL :
                   (vgrid == '+') ? DNGN_CLOSED_DOOR :
                   (vgrid == '=') ? DNGN_SECRET_DOOR :
                   (vgrid == 'w') ? DNGN_DEEP_WATER :
                   (vgrid == 'l') ? DNGN_LAVA :
                   (vgrid == '>') ? DNGN_ROCK_STAIRS_DOWN :
                   (vgrid == '<') ? DNGN_ROCK_STAIRS_UP :
                   (vgrid == '}') ? DNGN_STONE_STAIRS_DOWN_I :
                   (vgrid == '{') ? DNGN_STONE_STAIRS_UP_I :
                   (vgrid == ')') ? DNGN_STONE_STAIRS_DOWN_II :
                   (vgrid == '(') ? DNGN_STONE_STAIRS_UP_II :
                   (vgrid == ']') ? DNGN_STONE_STAIRS_DOWN_III :
                   (vgrid == '[') ? DNGN_STONE_STAIRS_UP_III :
                   (vgrid == 'A') ? DNGN_STONE_ARCH :
                   (vgrid == 'B') ? (DNGN_ALTAR_ZIN + altar_count) :// see below
                   (vgrid == 'C') ? pick_an_altar() :   // f(x) elsewhere {dlb}

                   (vgrid == 'F') ? (one_chance_in(100) 
                                     ? (coinflip() ? DNGN_SILVER_STATUE
                                                   : DNGN_ORANGE_CRYSTAL_STATUE)
                                     : DNGN_GRANITE_STATUE) :

                   (vgrid == 'I') ? DNGN_ORCISH_IDOL :
                   (vgrid == 'S') ? DNGN_SILVER_STATUE :
                   (vgrid == 'G') ? DNGN_GRANITE_STATUE :
                   (vgrid == 'H') ? DNGN_ORANGE_CRYSTAL_STATUE :
                   (vgrid == 'T') ? DNGN_BLUE_FOUNTAIN :
                   (vgrid == 'U') ? DNGN_SPARKLING_FOUNTAIN :
                   (vgrid == 'V') ? DNGN_PERMADRY_FOUNTAIN :
                   (vgrid == '\0')? DNGN_ROCK_WALL :
                                    DNGN_FLOOR); // includes everything else

    // then, handle oddball grids {dlb}:
    switch (vgrid)
    {
    case 'B':
        altar_count++;
        break;
    case '@':
        initial_x = vx;
        initial_y = vy;
        break;
    case '^':
        place_specific_trap(vx, vy, TRAP_RANDOM);
        break;
    }

    // then, handle grids that place "stuff" {dlb}:
    switch (vgrid)              // yes, I know this is a bit ugly ... {dlb}
    {
    case 'R':
    case '$':
    case '%':
    case '*':
    case '|':
    case 'P':                   // possible rune
    case 'O':                   // definite rune
    case 'Z':                   // definite orb
        {
            int item_made = NON_ITEM;
            unsigned char which_class = OBJ_RANDOM;
            unsigned char which_type = OBJ_RANDOM;
            int which_depth;
            bool possible_rune = one_chance_in(3);      // lame, I know {dlb}
            int spec = 250;

            if (vgrid == 'R')
            {
                which_class = OBJ_FOOD;
                which_type = (one_chance_in(3) ? FOOD_ROYAL_JELLY
                                               : FOOD_HONEYCOMB);
            }
            else if (vgrid == '$')
            {
                which_class = OBJ_GOLD;
                which_type = OBJ_RANDOM;
            }
            else if (vgrid == '%' || vgrid == '*')
            {
                which_class = OBJ_RANDOM;
                which_type = OBJ_RANDOM;
            }
            else if (vgrid == 'Z')
            {
                which_class = OBJ_ORBS;
                which_type = ORB_ZOT;
            }
            else if (vgrid == '|' 
                    || (vgrid == 'P' && (!possible_rune || num_runes > 0))
                    || (vgrid == 'O' && num_runes > 0))
            {
                which_class = acq_item_class[random2(7)];
                which_type = OBJ_RANDOM;
            }
            else              // for 'P' (1 out of 3 times) {dlb}
            {
                which_class = OBJ_MISCELLANY;
                which_type = MISC_RUNE_OF_ZOT;
                num_runes++;

                if (you.level_type == LEVEL_PANDEMONIUM)
                {
                    if (force_vault >= 60 && force_vault <= 63)
                        spec = force_vault;
                    else
                        spec = 50;
                }
                else if (you.level_type == LEVEL_ABYSS)
                    spec = 51;
                else 
                    spec = you.where_are_you;
            }

            which_depth = ((vgrid == '|'
                            || vgrid == 'P'
                            || vgrid == 'O'
                            || vgrid == 'Z') ? MAKE_GOOD_ITEM :
                           (vgrid == '*')    ? 5 + (level_number * 2)
                                             : level_number);

            item_made = items( 1, which_class, which_type, true, 
                               which_depth, spec );

            if (item_made != NON_ITEM)
            {
                mitm[item_made].x = vx;
                mitm[item_made].y = vy;
            }
        }
        break;
    }

    // finally, handle grids that place monsters {dlb}:
    if (vgrid >= '0' && vgrid <= '9')
    {
        int monster_level;
        int monster_type_thing;

        monster_level = ((vgrid == '8') ? (4 + (level_number * 2)) :
                         (vgrid == '9') ? (5 + level_number) : level_number);

        if (monster_level > 30) // very high level monsters more common here
            monster_level = 30;

        monster_type_thing = ((vgrid == '8'
                               || vgrid == '9'
                               || vgrid == '0') ? RANDOM_MONSTER
                                                : mons_array[(vgrid - '1')]);

        place_monster( not_used, monster_type_thing, monster_level, BEH_SLEEP,
                       MHITNOT, true, vx, vy, false);
    }

    // again, this seems odd, given that this is just one of many
    // vault types {dlb}
    return (altar_count);
}                               // end vault_grid()

static void replace_area(int sx, int sy, int ex, int ey, unsigned char replace,
    unsigned char feature)
{
    int x,y;
    for(x=sx; x<=ex; x++)
        for(y=sy; y<=ey; y++)
            if (grd[x][y] == replace)
                grd[x][y] = feature;
}

static void join_the_dots(unsigned char dotx1, unsigned char doty1,
                          unsigned char dotx2, unsigned char doty2,
                          char forbid_x1, char forbid_y1, char forbid_x2,
                          char forbid_y2)
{
    if (dotx1 == dotx2 && doty1 == doty2)
        return;

    char atx = dotx1, aty = doty1;

    int join_count = 0;

    grd[atx][aty] = DNGN_FLOOR;

    do
    {
        join_count++;

        if (join_count > 10000) // just insurance
            return;

        if (atx < dotx2
            && (forbid_x1 == 0
                || (atx + 1 < forbid_x1 || atx + 1 > forbid_x2
                    || (aty > forbid_y2 || aty < forbid_y1))))
        {
            atx++;
            goto continuing;
        }

        if (atx > dotx2
            && (forbid_x2 == 0
                || (atx - 1 > forbid_x2 || atx - 1 < forbid_x1
                    || (aty > forbid_y2 || aty < forbid_y1))))
        {
            atx--;
            goto continuing;
        }

        if (aty > doty2
            && (forbid_y2 == 0
                || (aty - 1 > forbid_y2 || aty - 1 < forbid_y1
                    || (atx > forbid_x2 || atx < forbid_x1))))
        {
            aty--;
            goto continuing;
        }

        if (aty < doty2
            && (forbid_y1 == 0
                || (aty + 1 < forbid_y1 || aty + 1 > forbid_y2
                    || (atx > forbid_x2 || atx < forbid_x1))))
        {
            aty++;
            goto continuing;
        }

      continuing:
        grd[atx][aty] = DNGN_FLOOR;

    }
    while (atx != dotx2 || aty != doty2);
}                               // end join_the_dots()

static void place_pool(unsigned char pool_type, unsigned char pool_x1,
                       unsigned char pool_y1, unsigned char pool_x2,
                       unsigned char pool_y2)
{
    int i, j;
    unsigned char left_edge, right_edge;

    // don't place LAVA pools in crypt.. use shallow water instead.
    if (pool_type == DNGN_LAVA 
        && (player_in_branch(BRANCH_CRYPT) || player_in_branch(BRANCH_TOMB)))
    {
        pool_type = DNGN_SHALLOW_WATER;
    }

    if (pool_x1 >= pool_x2 - 4 || pool_y1 >= pool_y2 - 4)
        return;

    left_edge = pool_x1 + 2 + random2(pool_x2 - pool_x1);
    right_edge = pool_x2 - 2 - random2(pool_x2 - pool_x1);

    for (j = pool_y1 + 1; j < pool_y2 - 1; j++)
    {
        for (i = pool_x1 + 1; i < pool_x2 - 1; i++)
        {
            if (i >= left_edge && i <= right_edge && grd[i][j] == DNGN_FLOOR)
                grd[i][j] = pool_type;
        }

        if (j - pool_y1 < (pool_y2 - pool_y1) / 2 || one_chance_in(4))
        {
            if (left_edge > pool_x1 + 1)
                left_edge -= random2(3);

            if (right_edge < pool_x2 - 1)
                right_edge += random2(3);
        }

        if (left_edge < pool_x2 - 1
            && (j - pool_y1 >= (pool_y2 - pool_y1) / 2
                || left_edge <= pool_x1 + 2 || one_chance_in(4)))
        {
            left_edge += random2(3);
        }

        if (right_edge > pool_x1 + 1
            && (j - pool_y1 >= (pool_y2 - pool_y1) / 2
                || right_edge >= pool_x2 - 2 || one_chance_in(4)))
        {
            right_edge -= random2(3);
        }
    }
}                               // end place_pool()

static void many_pools(unsigned char pool_type)
{
    int pools = 0;
    int i = 0, j = 0, k = 0, l = 0;
    int m = 0, n = 0;
    int no_pools = 20 + random2avg(9, 2);
    int timeout = 0;

    if (player_in_branch( BRANCH_COCYTUS ))
        pool_type = DNGN_DEEP_WATER;
    else if (player_in_branch( BRANCH_GEHENNA ))
        pool_type = DNGN_LAVA;

    do
    {
        timeout++;

        if (timeout >= 30000)
            break;

        i = 6 + random2( GXM - 26 );
        j = 6 + random2( GYM - 26 );
        k = i + 2 + roll_dice( 2, 9 );
        l = j + 2 + roll_dice( 2, 9 );

        for (m = i; m < k; m++)
        {
            for (n = j; n < l; n++)
            {
                if (grd[m][n] != DNGN_FLOOR)
                    goto continue_pools;
            }
        }

        place_pool(pool_type, i, j, k, l);
        pools++;

      continue_pools:
        continue;
    }
    while (pools < no_pools);
}                               // end many_pools()

void item_colour( item_def &item )
{
    int switchnum = 0;
    int temp_value;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        if (is_unrandom_artefact( item ))
            break;              // unrandarts already coloured

        if (is_fixed_artefact( item ))
        {
            switch (item.special)   // was: - 180, but that is *wrong* {dlb}
            {
            case SPWPN_SINGING_SWORD:
            case SPWPN_SCEPTRE_OF_TORMENT:
                item.colour = YELLOW;
                break;
            case SPWPN_WRATH_OF_TROG:
            case SPWPN_SWORD_OF_POWER:
                item.colour = RED;
                break;
            case SPWPN_SCYTHE_OF_CURSES:
                item.colour = DARKGREY;
                break;
            case SPWPN_MACE_OF_VARIABILITY:
                item.colour = random_colour();
                break;
            case SPWPN_GLAIVE_OF_PRUNE:
                item.colour = MAGENTA;
                break;
            case SPWPN_SWORD_OF_ZONGULDROK:
                item.colour = LIGHTGREY;
                break;
            case SPWPN_KNIFE_OF_ACCURACY:
                item.colour = LIGHTCYAN;
                break;
            case SPWPN_STAFF_OF_OLGREB:
                item.colour = GREEN;
                break;
            case SPWPN_VAMPIRES_TOOTH:
                item.colour = WHITE;
                break;
            case SPWPN_STAFF_OF_WUCAD_MU:
                item.colour = BROWN;
                break;
            }
            break;
        }

        if (is_demonic( item.sub_type ))
            item.colour = random_colour();
        else if (launches_things( item.sub_type ))
            item.colour = BROWN;
        else
        {
            switch (item.sub_type)
            {
            case WPN_CLUB:
            case WPN_GIANT_CLUB:
            case WPN_GIANT_SPIKED_CLUB:
            case WPN_ANCUS:
            case WPN_WHIP:
            case WPN_QUARTERSTAFF:
                item.colour = BROWN;
                break;
            case WPN_QUICK_BLADE:
                item.colour = LIGHTBLUE;
                break;
            case WPN_EXECUTIONERS_AXE:
                item.colour = RED;
                break;
            default:
                item.colour = LIGHTCYAN;
                if (cmp_equip_race( item, ISFLAG_DWARVEN ))
                    item.colour = CYAN;
                break;
            }
        }

        // I don't think this is ever done -- see start of case {dlb}:
        if (is_random_artefact( item ) && one_chance_in(5))
            item.colour = random_colour();
        break;

    case OBJ_MISSILES:
        switch (item.sub_type)
        {
        case MI_STONE:
        case MI_LARGE_ROCK:
        case MI_ARROW:
            item.colour = BROWN;
            break;
        case MI_NEEDLE:
            item.colour = WHITE;
            break;
        default:
            item.colour = LIGHTCYAN;
            if (cmp_equip_race( item, ISFLAG_DWARVEN ))
                item.colour = CYAN;
            break;
        }
        break;

    case OBJ_ARMOUR:
        if (is_unrandom_artefact( item ))
            break;              /* unrandarts have already been coloured */

        switch (item.sub_type)
        {
        case ARM_CLOAK:
        case ARM_ROBE:
            item.colour = random_colour();
            break;

        case ARM_HELMET:
            //caps and wizard's hats are random coloured
            if (cmp_helmet_type( item, THELM_CAP )
                    || cmp_helmet_type( item, THELM_WIZARD_HAT ))
            {
                item.colour = random_colour();
            } 
            else
                item.colour = LIGHTCYAN;
            break;

        case ARM_BOOTS: // maybe more interesting boot colours?
        case ARM_GLOVES:
        case ARM_LEATHER_ARMOUR:
            item.colour = BROWN;
            break;
        case ARM_DRAGON_HIDE:
        case ARM_DRAGON_ARMOUR:
            item.colour = mons_colour( MONS_DRAGON );
            break;
        case ARM_TROLL_HIDE:
        case ARM_TROLL_LEATHER_ARMOUR:
            item.colour = mons_colour( MONS_TROLL );
            break;
        case ARM_CRYSTAL_PLATE_MAIL:
            item.colour = LIGHTGREY;
            break;
        case ARM_ICE_DRAGON_HIDE:
        case ARM_ICE_DRAGON_ARMOUR:
            item.colour = mons_colour( MONS_ICE_DRAGON );
            break;
        case ARM_STEAM_DRAGON_HIDE:
        case ARM_STEAM_DRAGON_ARMOUR:
            item.colour = mons_colour( MONS_STEAM_DRAGON );
            break;
        case ARM_MOTTLED_DRAGON_HIDE:
        case ARM_MOTTLED_DRAGON_ARMOUR:
            item.colour = mons_colour( MONS_MOTTLED_DRAGON );
            break;
        case ARM_STORM_DRAGON_HIDE:
        case ARM_STORM_DRAGON_ARMOUR:
            item.colour = mons_colour( MONS_STORM_DRAGON );
            break;
        case ARM_GOLD_DRAGON_HIDE:
        case ARM_GOLD_DRAGON_ARMOUR:
            item.colour = mons_colour( MONS_GOLDEN_DRAGON );
            break;
        case ARM_ANIMAL_SKIN:
            item.colour = BROWN;
            break;
        case ARM_SWAMP_DRAGON_HIDE:
        case ARM_SWAMP_DRAGON_ARMOUR:
            item.colour = mons_colour( MONS_SWAMP_DRAGON );
            break;
        default:
            item.colour = LIGHTCYAN;
            if (cmp_equip_race( item, ISFLAG_DWARVEN ))
                item.colour = CYAN;
            break;
        }

        // I don't think this is ever done -- see start of case {dlb}:
        if (is_random_artefact( item ) && one_chance_in(5))
            item.colour = random_colour();
        break;

    case OBJ_WANDS:
        item.special = you.item_description[IDESC_WANDS][item.sub_type];

        switch (item.special % 12)
        {
        case 0:         //"iron wand"
            item.colour = CYAN;
            break;
        case 1:         //"brass wand"
        case 5:         //"gold wand"
            item.colour = YELLOW;
            break;
        case 2:         //"bone wand"
        case 8:         //"ivory wand"
        case 9:         //"glass wand"
        case 10:        //"lead wand"
        default:
            item.colour = LIGHTGREY;
            break;
        case 3:         //"wooden wand"
        case 4:         //"copper wand"
        case 7:         //"bronze wand"
            item.colour = BROWN;
            break;
        case 6:         //"silver wand"
            item.colour = WHITE;
            break;
        case 11:                //"plastic wand"
            item.colour = random_colour();
            break;
        }

        if (item.special / 12 == 9)
            item.colour = DARKGREY;

        // rare wands (eg disintegration - these will be very rare):
        // maybe only 1 thing, like: crystal, shining, etc.
        break;

    case OBJ_POTIONS:
        item.special = you.item_description[IDESC_POTIONS][item.sub_type];

        switch (item.special % 14)
        {
        case 0:         //"clear potion"
        default:
            item.colour = LIGHTGREY;
            break;
        case 1:         //"blue potion"
        case 7:         //"inky potion"
            item.colour = BLUE;
            break;
        case 2:         //"black potion"
            item.colour = DARKGREY;
            break;
        case 3:         //"silvery potion"
        case 13:        //"white potion"
            item.colour = WHITE;
            break;
        case 4:         //"cyan potion"
            item.colour = CYAN;
            break;
        case 5:         //"purple potion"
            item.colour = MAGENTA;
            break;
        case 6:         //"orange potion"
            item.colour = LIGHTRED;
            break;
        case 8:         //"red potion"
            item.colour = RED;
            break;
        case 9:         //"yellow potion"
            item.colour = YELLOW;
            break;
        case 10:        //"green potion"
            item.colour = GREEN;
            break;
        case 11:        //"brown potion"
            item.colour = BROWN;
            break;
        case 12:        //"pink potion"
            item.colour = LIGHTMAGENTA;
            break;
        }
        break;

    case OBJ_FOOD:
        switch (item.sub_type)
        {
        case FOOD_BEEF_JERKY:
        case FOOD_BREAD_RATION:
        case FOOD_LYCHEE:
        case FOOD_MEAT_RATION:
        case FOOD_RAMBUTAN:
        case FOOD_SAUSAGE:
        case FOOD_SULTANA:
            item.colour = BROWN;
            break;
        case FOOD_BANANA:
        case FOOD_CHEESE:
        case FOOD_HONEYCOMB:
        case FOOD_LEMON:
        case FOOD_PIZZA:
        case FOOD_ROYAL_JELLY:
            item.colour = YELLOW;
            break;
        case FOOD_PEAR:
            item.colour = LIGHTGREEN;
            break;
        case FOOD_CHOKO:
        case FOOD_SNOZZCUMBER:
            item.colour = GREEN;
            break;
        case FOOD_APRICOT:
        case FOOD_ORANGE:
            item.colour = LIGHTRED;
            break;
        case FOOD_STRAWBERRY:
            item.colour = RED;
            break;
        case FOOD_APPLE:
            item.colour = (coinflip() ? RED : GREEN);
            break;
        case FOOD_GRAPE:
            item.colour = (coinflip() ? MAGENTA : GREEN);
            break;
        case FOOD_CHUNK:
            // set the appropriate colour of the meat: 
            temp_value = mons_colour( item.plus );
            item.colour = (temp_value == BLACK) ? LIGHTRED : temp_value;
            break;
        default:
            item.colour = BROWN;
        }
        break;

    case OBJ_JEWELLERY:
        /* unrandarts have already been coloured */
        if (is_unrandom_artefact( item ))
            break;
        else if (is_random_artefact( item ))
        {
            item.colour = random_colour();
            break;
        }

        item.colour = YELLOW;
        item.special = you.item_description[IDESC_RINGS][item.sub_type];

        switchnum = item.special % 13;

        switch (switchnum)
        {
        case 0:
        case 5:
            item.colour = BROWN;
            break;
        case 1:
        case 8:
        case 11:
            item.colour = LIGHTGREY;
            break;
        case 2:
        case 6:
            item.colour = YELLOW;
            break;
        case 3:
        case 4:
            item.colour = CYAN;
            break;
        case 7:
            item.colour = BROWN;
            break;
        case 9:
        case 10:
            item.colour = WHITE;
            break;
        case 12:
            item.colour = GREEN;
            break;
        case 13:
            item.colour = LIGHTCYAN;
            break;
        }

        if (item.sub_type >= AMU_RAGE)
        {
            switch (switchnum)
            {
            case 0:             //"zirconium amulet"
            case 9:             //"ivory amulet"
            case 11:            //"platinum amulet"
                item.colour = WHITE;
                break;
            case 1:             //"sapphire amulet"
                item.colour = LIGHTBLUE;
                break;
            case 2:             //"golden amulet"
            case 6:             //"brass amulet"
                item.colour = YELLOW;
                break;
            case 3:             //"emerald amulet"
                item.colour = GREEN;
                break;
            case 4:             //"garnet amulet"
            case 8:             //"ruby amulet"
                item.colour = RED;
                break;
            case 5:             //"bronze amulet"
            case 7:             //"copper amulet"
                item.colour = BROWN;
                break;
            case 10:            //"bone amulet"
                item.colour = LIGHTGREY;
                break;
            case 12:            //"jade amulet"
                item.colour = GREEN;
                break;
            case 13:            //"plastic amulet"
                item.colour = random_colour();
            }
        }

        // blackened - same for both rings and amulets
        if (item.special / 13 == 5)
            item.colour = DARKGREY;
        break;

    case OBJ_SCROLLS:
        item.colour = LIGHTGREY;
        item.special = you.item_description[IDESC_SCROLLS][item.sub_type];
        item.plus = you.item_description[IDESC_SCROLLS_II][item.sub_type];
        break;

    case OBJ_BOOKS:
        switch (item.special % 10)
        {
        case 0:
        case 1:
        default:
            item.colour = random_colour();
            break;
        case 2:
            item.colour = (one_chance_in(3) ? BROWN : DARKGREY);
            break;
        case 3:
            item.colour = CYAN;
            break;
        case 4:
            item.colour = LIGHTGREY;
            break;
        }
        break;

    case OBJ_STAVES:
        item.colour = BROWN;
        break;

    case OBJ_ORBS:
        item.colour = LIGHTMAGENTA;
        break;

    case OBJ_MISCELLANY:
        switch (item.sub_type)
        {
        case MISC_BOTTLED_EFREET:
        case MISC_STONE_OF_EARTH_ELEMENTALS:
            item.colour = BROWN;
            break;

        case MISC_AIR_ELEMENTAL_FAN:
        case MISC_CRYSTAL_BALL_OF_ENERGY:
        case MISC_CRYSTAL_BALL_OF_FIXATION:
        case MISC_CRYSTAL_BALL_OF_SEEING:
        case MISC_DISC_OF_STORMS:
        case MISC_HORN_OF_GERYON:
        case MISC_LANTERN_OF_SHADOWS:
            item.colour = LIGHTGREY;
            break;

        case MISC_LAMP_OF_FIRE:
            item.colour = YELLOW;
            break;

        case MISC_BOX_OF_BEASTS:
            item.colour = DARKGREY;
            break;

        case MISC_RUNE_OF_ZOT:
            switch (item.plus)
            {
            case RUNE_DIS:                      // iron
                item.colour = CYAN;
                break;

            case RUNE_COCYTUS:                  // icy
                item.colour = LIGHTBLUE;
                break;

            case RUNE_TARTARUS:                 // bone
                item.colour = WHITE;
                break;

            case RUNE_SLIME_PITS:               // slimy
                item.colour = GREEN;
                break;

            case RUNE_SNAKE_PIT:                // serpentine
            case RUNE_ELVEN_HALLS:              // elven
                item.colour = LIGHTGREEN;
                break;

            case RUNE_VAULTS:                   // silver
                item.colour = LIGHTGREY;
                break;

            case RUNE_TOMB:                     // golden
                item.colour = YELLOW;
                break;

            case RUNE_SWAMP:                    // decaying
                item.colour = BROWN;
                break;

            // These two are hardly unique, but since colour isn't used for 
            // stacking, so we don't have to worry to much about this. -- bwr
            case RUNE_DEMONIC:             // random pandemonium demonlords
            case RUNE_ABYSSAL:             // random in abyss
                item.colour = random_colour();
                break;

            case RUNE_MNOLEG:                   // glowing
                item.colour = coinflip() ? MAGENTA : LIGHTMAGENTA;
                break;

            case RUNE_LOM_LOBON:                // magical
                item.colour = BLUE;
                break;

            case RUNE_CEREBOV:                  // fiery
                item.colour = coinflip() ? RED : LIGHTRED;
                break;

            case RUNE_GEHENNA:                  // obsidian
            case RUNE_GLOORX_VLOQ:              // dark
            default:
                item.colour = DARKGREY;
                break;
            }
            break;

        case MISC_EMPTY_EBONY_CASKET:
            item.colour = DARKGREY;
            break;

        case MISC_DECK_OF_SUMMONINGS:
        case MISC_DECK_OF_WONDERS:
        case MISC_DECK_OF_TRICKS:
        case MISC_DECK_OF_POWER:
        default:
            item.colour = random_colour();
            break;
        }
        break;

    case OBJ_CORPSES:
        // set the appropriate colour of the body: 
        temp_value = mons_colour( item.plus );
        item.colour = (temp_value == BLACK) ? LIGHTRED : temp_value;
        break;

    case OBJ_GOLD:
        item.colour = YELLOW;
        break;
    }
}                               // end item_colour()

// Checks how rare a weapon is. Many of these have special routines for
// placement, especially those with a rarity of zero. Chance is out of 10.
static char rare_weapon(unsigned char w_type)
{

    // zero value weapons must be placed specially -- see items() {dlb}
    if (is_demonic(w_type))
        return 0;

    switch (w_type)
    {
    case WPN_CLUB:
    case WPN_DAGGER:
        return 10;
    case WPN_HAND_AXE:
    case WPN_FALCHION:
    case WPN_MACE:
    case WPN_QUARTERSTAFF:
        return 9;
    case WPN_BOW:
    case WPN_FLAIL:
    case WPN_HAMMER:
    case WPN_SABRE:
    case WPN_SHORT_SWORD:
    case WPN_SLING:
    case WPN_SPEAR:
        return 8;
    case WPN_LONG_SWORD:
    case WPN_MORNINGSTAR:
    case WPN_WAR_AXE:
        return 7;
    case WPN_BATTLEAXE:
    case WPN_CROSSBOW:
    case WPN_GREAT_SWORD:
    case WPN_SCIMITAR:
    case WPN_TRIDENT:
        return 6;
    case WPN_GLAIVE:
    case WPN_HALBERD:
    case WPN_BLOWGUN:
        return 5;
    case WPN_BROAD_AXE:
    case WPN_HAND_CROSSBOW:
    case WPN_SPIKED_FLAIL:
    case WPN_WHIP:
        return 4;
    case WPN_GREAT_MACE:
        return 3;
    case WPN_ANCUS:
    case WPN_GREAT_FLAIL:
    case WPN_SCYTHE:
        return 2;
    case WPN_GIANT_CLUB:
    case WPN_GIANT_SPIKED_CLUB:
        return 1;
        // zero value weapons must be placed specially -- see items() {dlb}
    case WPN_DOUBLE_SWORD:
    case WPN_EVENINGSTAR:
    case WPN_EXECUTIONERS_AXE:
    case WPN_KATANA:
    case WPN_KNIFE:
    case WPN_QUICK_BLADE:
    case WPN_TRIPLE_SWORD:
        return 0;

    default:
        // should not happen now that calls are bounded by NUM_WEAPONS {dlb}
        return -1;
    }
}                               // end rare_weapon()

//jmf: generate altar based on where you are, or possibly randomly
static int pick_an_altar(void)
{
    int altar_type = 0;
    int temp_rand;              // probability determination {dlb}

    if (player_in_branch( BRANCH_SLIME_PITS ) 
        || player_in_branch( BRANCH_ECUMENICAL_TEMPLE )
        || you.level_type == LEVEL_LABYRINTH)
    {
        // no extra altars in temple, none at all in slime pits or labyrinth
        altar_type = DNGN_FLOOR;
    }
    else if (you.level_type == LEVEL_DUNGEON && !one_chance_in(5))
    {
        switch (you.where_are_you)
        {
        case BRANCH_CRYPT:
            altar_type = (coinflip() ? DNGN_ALTAR_KIKUBAAQUDGHA
                                     : DNGN_ALTAR_YREDELEMNUL);
            break;

        case BRANCH_ORCISH_MINES:       // violent gods
            temp_rand = random2(5);

            altar_type = ((temp_rand == 0) ? DNGN_ALTAR_VEHUMET :
                          (temp_rand == 1) ? DNGN_ALTAR_MAKHLEB :
                          (temp_rand == 2) ? DNGN_ALTAR_OKAWARU :
                          (temp_rand == 3) ? DNGN_ALTAR_TROG
                                           : DNGN_ALTAR_XOM);
            break;

        case BRANCH_VAULTS: // "lawful" gods
            temp_rand = random2(7);

            altar_type = ((temp_rand == 0) ? DNGN_ALTAR_ELYVILON :
                          (temp_rand == 1) ? DNGN_ALTAR_SIF_MUNA :
                          (temp_rand == 2) ? DNGN_ALTAR_SHINING_ONE :
                          (temp_rand == 3 || temp_rand == 4) ? DNGN_ALTAR_OKAWARU
                                           : DNGN_ALTAR_ZIN);
            break;

        case BRANCH_HALL_OF_BLADES:
            altar_type = DNGN_ALTAR_OKAWARU;
            break;

        case BRANCH_ELVEN_HALLS:    // "magic" gods
            temp_rand = random2(4);

            altar_type = ((temp_rand == 0) ? DNGN_ALTAR_VEHUMET :
                          (temp_rand == 1) ? DNGN_ALTAR_SIF_MUNA :
                          (temp_rand == 2) ? DNGN_ALTAR_XOM
                                           : DNGN_ALTAR_MAKHLEB);
            break;

        case BRANCH_TOMB:
            altar_type = DNGN_ALTAR_KIKUBAAQUDGHA;
            break;

        default:
            do
            {
                altar_type = DNGN_ALTAR_ZIN + random2(NUM_GODS - 1);
            }
            while (altar_type == DNGN_ALTAR_NEMELEX_XOBEH);
            break;
        }
    }
    else
    {
        // Note: this case includes the pandemonium or the abyss.
        temp_rand = random2(9);

        altar_type = ((temp_rand == 0) ? DNGN_ALTAR_ZIN :
                      (temp_rand == 1) ? DNGN_ALTAR_SHINING_ONE :
                      (temp_rand == 2) ? DNGN_ALTAR_KIKUBAAQUDGHA :
                      (temp_rand == 3) ? DNGN_ALTAR_XOM :
                      (temp_rand == 4) ? DNGN_ALTAR_OKAWARU :
                      (temp_rand == 5) ? DNGN_ALTAR_MAKHLEB :
                      (temp_rand == 6) ? DNGN_ALTAR_SIF_MUNA :
                      (temp_rand == 7) ? DNGN_ALTAR_TROG
                                       : DNGN_ALTAR_ELYVILON);
    }

    return (altar_type);
}                               // end pick_an_altar()

static void place_altar(void)
{
    int px, py;
    int i, j;
    int k = 0, l = 0;
    int altar_type = pick_an_altar();

    while(true)
    {
      rand_px:

        px = 15 + random2(55);
        py = 15 + random2(45);
        k++;

        if (k == 5000)
            return;

        l = 0;

        for (i = px - 2; i < px + 3; i++)
        {
            for (j = py - 2; j < py + 3; j++)
            {
                if (grd[i][j] == DNGN_FLOOR)
                    l++;

                if ((grd[i][j] != DNGN_ROCK_WALL
                        && grd[i][j] != DNGN_CLOSED_DOOR
                        && grd[i][j] != DNGN_SECRET_DOOR
                        && grd[i][j] != DNGN_FLOOR)
                    || mgrd[i][j] != NON_MONSTER)
                {
                    goto rand_px;
                }
            }
        }

        if (l == 0)
            goto rand_px;

        for (i = px - 2; i < px + 3; i++)
        {
            for (j = py - 2; j < py + 3; j++)
            {
                grd[i][j] = DNGN_FLOOR;
            }
        }

        grd[px][py] = altar_type;

        return;
    }
}                               // end place_altar()

static void place_shops(int level_number)
{
    int temp_rand = 0;          // probability determination {dlb}
    int timeout = 0;

    unsigned char no_shops = 0;
    unsigned char shop_place_x = 0;
    unsigned char shop_place_y = 0;

    temp_rand = random2(125);

#if DEBUG_SHOPS
    no_shops = MAX_SHOPS;
#else
    no_shops = ((temp_rand > 28) ? 0 :                        // 76.8%
                (temp_rand > 4)  ? 1                          // 19.2% 
                                 : 1 + random2( MAX_SHOPS )); //  4.0% 

    if (no_shops == 0 || level_number < 3)
        return;
#endif

    for (int i = 0; i < no_shops; i++)
    {
        timeout = 0;

        do
        {
            shop_place_x = random2(GXM - 20) + 10;
            shop_place_y = random2(GYM - 20) + 10;

            timeout++;

            if (timeout > 20000)
                return;
        }
        while (grd[shop_place_x][shop_place_y] != DNGN_FLOOR);

        place_spec_shop(level_number, shop_place_x, shop_place_y, SHOP_RANDOM);
    }
}                               // end place_shops()

static void place_spec_shop( int level_number, 
                             unsigned char shop_x, unsigned char shop_y,
                             unsigned char force_s_type )
{
    int orb = 0;
    int i = 0;
    int j = 0;                  // loop variable
    int item_level;

    for (i = 0; i < MAX_SHOPS; i++)
    {
        if (env.shop[i].type == SHOP_UNASSIGNED)
            break;
    }

    if (i == MAX_SHOPS)
        return;

    for (j = 0; j < 3; j++)
    {
        env.shop[i].keeper_name[j] = 1 + random2(200);
    }

    env.shop[i].level = level_number * 2;

    env.shop[i].type = (force_s_type != SHOP_RANDOM) ? force_s_type 
                                                     : random2(NUM_SHOPS);

    if (env.shop[i].type == SHOP_FOOD)
    {
        env.shop[i].greed = 10 + random2(5);
    }
    else if (env.shop[i].type != SHOP_WEAPON_ANTIQUE
        && env.shop[i].type != SHOP_ARMOUR_ANTIQUE
        && env.shop[i].type != SHOP_GENERAL_ANTIQUE)
    {
        env.shop[i].greed = 10 + random2(5) + random2(level_number / 2);
    }
    else
    {
        env.shop[i].greed = 15 + random2avg(19, 2) + random2(level_number);
    }

    int plojy = 5 + random2avg(12, 3);

    for (j = 0; j < plojy; j++)
    {
        if (env.shop[i].type != SHOP_WEAPON_ANTIQUE
            && env.shop[i].type != SHOP_ARMOUR_ANTIQUE
            && env.shop[i].type != SHOP_GENERAL_ANTIQUE)
        {
            item_level = level_number + random2((level_number + 1) * 2);
        }
        else
        {
            item_level = level_number + random2((level_number + 1) * 3);
        }

        if (one_chance_in(4))
            item_level = MAKE_GOOD_ITEM;

        // don't generate gold in shops!  This used to be possible with
        // General Stores (see item_in_shop() below)   (GDL)
        while(true)
        {
            orb = items( 1, item_in_shop(env.shop[i].type), OBJ_RANDOM, true,
                         item_level, 250 );

            if (orb != NON_ITEM 
                && mitm[orb].base_type != OBJ_GOLD
                && (env.shop[i].type != SHOP_GENERAL_ANTIQUE
                    || (mitm[orb].base_type != OBJ_MISSILES
                        && mitm[orb].base_type != OBJ_FOOD)))
            {
                break;
            }

            // reset object and try again
            if (orb != NON_ITEM)
            {
                mitm[orb].base_type = OBJ_UNASSIGNED;
                mitm[orb].quantity = 0;
            }
        }

        if (orb == NON_ITEM)
            break;

        // set object 'position' (gah!) & ID status
        mitm[orb].x = 0;
        mitm[orb].y = 5 + i;

        if (env.shop[i].type != SHOP_WEAPON_ANTIQUE
            && env.shop[i].type != SHOP_ARMOUR_ANTIQUE
            && env.shop[i].type != SHOP_GENERAL_ANTIQUE)
        {
            set_ident_flags( mitm[orb], ISFLAG_IDENT_MASK );
        }
    }

    env.shop[i].x = shop_x;
    env.shop[i].y = shop_y;

    grd[shop_x][shop_y] = DNGN_ENTER_SHOP;
}                               // end place_spec_shop()

static unsigned char item_in_shop(unsigned char shop_type)
{
    switch (shop_type)
    {
    case SHOP_WEAPON:
        if (one_chance_in(5))
            return (OBJ_MISSILES);
        // *** deliberate fall through here  {dlb} ***
    case SHOP_WEAPON_ANTIQUE:
        return (OBJ_WEAPONS);

    case SHOP_ARMOUR:
    case SHOP_ARMOUR_ANTIQUE:
        return (OBJ_ARMOUR);

    case SHOP_GENERAL:
    case SHOP_GENERAL_ANTIQUE:
        return (OBJ_RANDOM);

    case SHOP_JEWELLERY:
        return (OBJ_JEWELLERY);

    case SHOP_WAND:
        return (OBJ_WANDS);

    case SHOP_BOOK:
        return (OBJ_BOOKS);

    case SHOP_FOOD:
        return (OBJ_FOOD);

    case SHOP_DISTILLERY:
        return (OBJ_POTIONS);

    case SHOP_SCROLL:
        return (OBJ_SCROLLS);
    }

    return (OBJ_RANDOM);
}                               // end item_in_shop()

static void spotty_level(bool seeded, int iterations, bool boxy)
{
    // assumes starting with a level full of rock walls (1)
    int i, j, k, l;

    if (!seeded)
    {
        for (i = DNGN_STONE_STAIRS_DOWN_I; i < DNGN_ROCK_STAIRS_UP; i++)
        {
            if (i == DNGN_ROCK_STAIRS_DOWN 
                || (i == DNGN_STONE_STAIRS_UP_I 
                    && !player_in_branch( BRANCH_SLIME_PITS )))
            {
                continue;
            }

            do
            {
                j = 10 + random2(GXM - 20);
                k = 10 + random2(GYM - 20);
            }
            while (grd[j][k] != DNGN_ROCK_WALL
                    && grd[j + 1][k] != DNGN_ROCK_WALL);

            grd[j][k] = i;

            // creating elevators
            if (i == DNGN_STONE_STAIRS_DOWN_I
                && !player_in_branch( BRANCH_SLIME_PITS ))
            {
                grd[j + 1][k] = DNGN_STONE_STAIRS_UP_I;
            }

            if (grd[j][k - 1] == DNGN_ROCK_WALL)
                grd[j][k - 1] = DNGN_FLOOR;
            if (grd[j][k + 1] == DNGN_ROCK_WALL)
                grd[j][k + 1] = DNGN_FLOOR;
            if (grd[j - 1][k] == DNGN_ROCK_WALL)
                grd[j - 1][k] = DNGN_FLOOR;
            if (grd[j + 1][k] == DNGN_ROCK_WALL)
                grd[j + 1][k] = DNGN_FLOOR;
        }
    }                           // end if !seeded

    l = iterations;

    // boxy levels have more clearing, so they get fewer iterations:
    if (l == 0)
        l = 200 + random2( (boxy ? 750 : 1500) );

    for (i = 0; i < l; i++)
    {
        do
        {
            j = random2(GXM - 20) + 10;
            k = random2(GYM - 20) + 10;
        }
        while (grd[j][k] == DNGN_ROCK_WALL
               && grd[j - 1][k] == DNGN_ROCK_WALL
               && grd[j + 1][k] == DNGN_ROCK_WALL
               && grd[j][k - 1] == DNGN_ROCK_WALL
               && grd[j][k + 1] == DNGN_ROCK_WALL
               && grd[j - 2][k] == DNGN_ROCK_WALL
               && grd[j + 2][k] == DNGN_ROCK_WALL
               && grd[j][k - 2] == DNGN_ROCK_WALL
               && grd[j][k + 2] == DNGN_ROCK_WALL);

        if (grd[j][k] == DNGN_ROCK_WALL)
            grd[j][k] = DNGN_FLOOR;
        if (grd[j][k - 1] == DNGN_ROCK_WALL)
            grd[j][k - 1] = DNGN_FLOOR;
        if (grd[j][k + 1] == DNGN_ROCK_WALL)
            grd[j][k + 1] = DNGN_FLOOR;
        if (grd[j - 1][k] == DNGN_ROCK_WALL)
            grd[j - 1][k] = DNGN_FLOOR;
        if (grd[j + 1][k] == DNGN_ROCK_WALL)
            grd[j + 1][k] = DNGN_FLOOR;

        if (boxy)
        {
            if (grd[j - 1][k - 1] == DNGN_ROCK_WALL)
                grd[j - 1][k - 1] = DNGN_FLOOR;
            if (grd[j + 1][k + 1] == DNGN_ROCK_WALL)
                grd[j + 1][k + 1] = DNGN_FLOOR;
            if (grd[j - 1][k + 1] == DNGN_ROCK_WALL)
                grd[j - 1][k + 1] = DNGN_FLOOR;
            if (grd[j + 1][k - 1] == DNGN_ROCK_WALL)
                grd[j + 1][k - 1] = DNGN_FLOOR;
        }
    }
}                               // end spotty_level()

static void bigger_room(void)
{
    unsigned char i, j;

    for (i = 10; i < (GXM - 10); i++)
    {
        for (j = 10; j < (GYM - 10); j++)
        {
            if (grd[i][j] == DNGN_ROCK_WALL)
                grd[i][j] = DNGN_FLOOR;
        }
    }

    many_pools(DNGN_DEEP_WATER);

    if (one_chance_in(3))
    {       
        if (coinflip())
            build_river( DNGN_DEEP_WATER );
        else
            build_lake( DNGN_DEEP_WATER );
    }

    int pair_count = coinflip() ? 4 : 3;

    for (j = 0; j < pair_count; j++)
    {
        for (i = 0; i < 2; i++)
        {
            place_specific_stair( j + ((i==0) ? DNGN_STONE_STAIRS_DOWN_I
                                              : DNGN_STONE_STAIRS_UP_I) );
        }
    }
}                               // end bigger_room()

// various plan_xxx functions
static void plan_main(int level_number, char force_plan)
{
    // possible values for do_stairs:
    //  0 - stairs already done
    //  1 - stairs already done, do spotty
    //  2 - no stairs
    //  3 - no stairs, do spotty
    char do_stairs = 0;
    unsigned char special_grid = (one_chance_in(3) ? DNGN_METAL_WALL
                                                   : DNGN_STONE_WALL);
    int i,j;

    if (!force_plan)
        force_plan = 1 + random2(12);

    do_stairs = ((force_plan == 1) ? plan_1() :
                 (force_plan == 2) ? plan_2() :
                 (force_plan == 3) ? plan_3() :
                 (force_plan == 4) ? plan_4(0, 0, 0, 0, 99) :
                 (force_plan == 5) ? (one_chance_in(9) ? plan_5()
                                                       : plan_3()) :
                 (force_plan == 6) ? plan_6(level_number)
                                   : plan_3());

    if (do_stairs == 3 || do_stairs == 1)
        spotty_level(true, 0, coinflip());

    if (do_stairs == 2 || do_stairs == 3)
    {
        int pair_count = coinflip()?4:3;

        for (j = 0; j < pair_count; j++)
        {
            for (i = 0; i < 2; i++)
            {
                place_specific_stair( j + ((i==0) ? DNGN_STONE_STAIRS_DOWN_I
                                                  : DNGN_STONE_STAIRS_UP_I) );
            }
        }
    }

    if (one_chance_in(20))
        replace_area(0,0,GXM-1,GYM-1,DNGN_ROCK_WALL,special_grid);
}                               // end plan_main()

static char plan_1(void)
{
    int temp_rand = 0;          // probability determination {dlb}

    unsigned char width = (10 - random2(7));    // value range of [4,10] {dlb}

    replace_area(10, 10, (GXM - 10), (10 + width), DNGN_ROCK_WALL, DNGN_FLOOR);
    replace_area(10, (60 - width), (GXM - 10), (GYM - 10),
        DNGN_ROCK_WALL, DNGN_FLOOR);
    replace_area(10, 10, (10 + width), (GYM - 10), DNGN_ROCK_WALL, DNGN_FLOOR);
    replace_area((60 - width), 10, (GXM - 10), (GYM - 10),
        DNGN_ROCK_WALL, DNGN_FLOOR);

    // possible early returns {dlb}:
    temp_rand = random2(4);

    if (temp_rand > 2)          // 25% chance {dlb}
        return 3;
    else if (temp_rand > 1)     // 25% chance {dlb}
        return 2;
    else                        // 50% chance {dlb}
    {
        unsigned char width2 = (coinflip()? (1 + random2(5)) : 5);

        replace_area(10, (35 - width2), (GXM - 10), (35 + width2),
                   DNGN_ROCK_WALL, DNGN_FLOOR);
        replace_area((40 - width2), 10, (40 + width2), (GYM - 10),
                   DNGN_ROCK_WALL, DNGN_FLOOR);
    }

    // possible early returns {dlb}:
    temp_rand = random2(4);

    if (temp_rand > 2)          // 25% chance {dlb}
        return 3;
    else if (temp_rand > 1)     // 25% chance {dlb}
        return 2;
    else                        // 50% chance {dlb}
    {
        temp_rand = random2(15);

        if (temp_rand > 7)      // 7 in 15 odds {dlb}
        {
            spec_room sr = { false, false, 0,0,0,0 };
            sr.x1 = 25;
            sr.y1 = 25;
            sr.x2 = (GXM - 25);
            sr.y2 = (GYM - 25);

            int oblique_max = 0;
            if (coinflip())
                oblique_max = 5 + random2(20);

            temp_rand = random2(7);

            unsigned char floor_type = ((temp_rand > 1) ? DNGN_FLOOR :   // 5/7
                                        (temp_rand > 0) ? DNGN_DEEP_WATER// 1/7
                                                        : DNGN_LAVA);    // 1/7
            octa_room(sr, oblique_max, floor_type);
        }
    }

    // final return {dlb}:
    return (one_chance_in(5) ? 3 : 2);
}                               // end plan_1()

// just a cross:
static char plan_2(void)
{
    char width2 = (5 - random2(5));     // value range of [1,5] {dlb}

    replace_area(10, (35 - width2), (GXM - 10), (35 + width2),
                                            DNGN_ROCK_WALL, DNGN_FLOOR);
    replace_area((40 - width2), 10, (40 + width2), (GYM - 10),
                                            DNGN_ROCK_WALL, DNGN_FLOOR);

    return (one_chance_in(4) ? 2 : 3);
}                               // end plan_2()

static char plan_3(void)
{

    /* Draws a room, then another and links them together, then another and etc
       Of course, this can easily end up looking just like a make_trail level.
     */
    int i;
    char cnx, cny;
    char roomsss = 30 + random2(90);

    bool exclusive = (one_chance_in(10) ? false : true);
    bool exclusive2 = coinflip();

    char romx1[30], romy1[30], romx2[30], romy2[30];

    char which_room = 0;

    for (i = 0; i < roomsss; i++)
    {
        romx1[which_room] = 10 + random2(50);
        romy1[which_room] = 10 + random2(40);
        romx2[which_room] = romx1[which_room] + 2 + random2(8);
        romy2[which_room] = romy1[which_room] + 2 + random2(8);

        if (exclusive)
        {
            for (cnx = romx1[which_room] - 1; cnx < romx2[which_room] + 1;
                                                                        cnx++)
            {
                for (cny = romy1[which_room] - 1; cny < romy2[which_room] + 1;
                                                                        cny++)
                {
                    if (grd[cnx][cny] != DNGN_ROCK_WALL)
                        goto continuing;
                }
            }
        }

        replace_area(romx1[which_room], romy1[which_room], romx2[which_room],
                   romy2[which_room], DNGN_ROCK_WALL, DNGN_FLOOR);

        if (which_room > 0 && !exclusive2)
        {
            const int rx1 = romx1[which_room];
            const int rx2 = romx2[which_room];
            const int prev_rx1 = romx1[which_room - 1];
            const int prev_rx2 = romx2[which_room - 1];

            const int ry1 = romy1[which_room];
            const int ry2 = romy2[which_room];
            const int prev_ry1 = romy1[which_room - 1];
            const int prev_ry2 = romy2[which_room - 1];

            join_the_dots( rx1 + random2( rx2 - rx1 ),
                           ry1 + random2( ry2 - ry1 ),
                           prev_rx1 + random2( prev_rx2 - prev_rx1 ),
                           prev_ry1 + random2( prev_ry2 - prev_ry1 ),
                           0, 0, 0, 0 );
        }

        which_room++;

        if (which_room >= 29)
            break;

      continuing:
        continue;
    }

    if (exclusive2)
    {
        for (i = 0; i < which_room; i++)
        {
            if (i > 0)
            {
                const int rx1 = romx1[i];
                const int rx2 = romx2[i];
                const int prev_rx1 = romx1[i - 1];
                const int prev_rx2 = romx2[i - 1];

                const int ry1 = romy1[i];
                const int ry2 = romy2[i];
                const int prev_ry1 = romy1[i - 1];
                const int prev_ry2 = romy2[i - 1];

                join_the_dots( rx1 + random2( rx2 - rx1 ),
                               ry1 + random2( ry2 - ry1 ),
                               prev_rx1 + random2( prev_rx2 - prev_rx1 ),
                               prev_ry1 + random2( prev_ry2 - prev_ry1 ),
                               0, 0, 0, 0 );
            }
        }
    }

    return 2;
}                               // end plan_3()

static char plan_4(char forbid_x1, char forbid_y1, char forbid_x2,
                            char forbid_y2, unsigned char force_wall)
{
    // a more chaotic version of city level
    int temp_rand;              // req'd for probability checking

    int number_boxes = 5000;
    unsigned char drawing = DNGN_ROCK_WALL;
    char b1x, b1y, b2x, b2y;
    char cnx, cny;
    int i;

    temp_rand = random2(81);

    number_boxes = ((temp_rand > 48) ? 4000 :   // odds: 32 in 81 {dlb}
                    (temp_rand > 24) ? 3000 :   // odds: 24 in 81 {dlb}
                    (temp_rand >  8) ? 5000 :   // odds: 16 in 81 {dlb}
                    (temp_rand >  0) ? 2000     // odds:  8 in 81 {dlb}
                                     : 1000);   // odds:  1 in 81 {dlb}

    if (force_wall != 99)
        drawing = force_wall;
    else
    {
        temp_rand = random2(18);

        drawing = ((temp_rand > 7) ? DNGN_ROCK_WALL :   // odds: 10 in 18 {dlb}
                   (temp_rand > 2) ? DNGN_STONE_WALL    // odds:  5 in 18 {dlb}
                                   : DNGN_METAL_WALL);  // odds:  3 in 18 {dlb}
    }

    replace_area(10, 10, (GXM - 10), (GYM - 10), DNGN_ROCK_WALL, DNGN_FLOOR);

    // replace_area can also be used to fill in:
    for (i = 0; i < number_boxes; i++)
    {

        b1x = 11 + random2(45);
        b1y = 11 + random2(35);

        b2x = b1x + 3 + random2(7) + random2(5);
        b2y = b1y + 3 + random2(7) + random2(5);

        if (forbid_x1 != 0 || forbid_x2 != 0)
        {
            if (b1x <= forbid_x2 && b1x >= forbid_x1
                    && b1y <= forbid_y2 && b1y >= forbid_y1)
            {
                goto continuing;
            }
            else if (b2x <= forbid_x2 && b2x >= forbid_x1
                    && b2y <= forbid_y2 && b2y >= forbid_y1)
            {
                goto continuing;
            }
        }

        for (cnx = b1x - 1; cnx < b2x + 1; cnx++)
        {
            for (cny = b1y - 1; cny < b2y + 1; cny++)
            {
                if (grd[cnx][cny] != DNGN_FLOOR)
                    goto continuing;
            }
        }

        if (force_wall == 99)
        {
            // NB: comparison reversal here - combined
            temp_rand = random2(1200);

            // probabilities *not meant* to sum to one! {dlb}
            if (temp_rand < 417)        // odds: 261 in 1200 {dlb}
                drawing = DNGN_ROCK_WALL;
            else if (temp_rand < 156)   // odds: 116 in 1200 {dlb}
                drawing = DNGN_STONE_WALL;
            else if (temp_rand < 40)    // odds:  40 in 1200 {dlb}
                drawing = DNGN_METAL_WALL;
        }

        temp_rand = random2(210);

        if (temp_rand > 71)     // odds: 138 in 210 {dlb}
            replace_area(b1x, b1y, b2x, b2y, DNGN_FLOOR, drawing);
        else                    // odds:  72 in 210 {dlb}
            box_room(b1x, b2x - 1, b1y, b2y - 1, drawing);

      continuing:
        continue;
    }

    if (forbid_x1 == 0 && one_chance_in(4))     // a market square
    {
        spec_room sr = { false, false, 0, 0, 0, 0 };
        sr.x1 = 25;
        sr.y1 = 25;
        sr.x2 = 55;
        sr.y2 = 45;

        int oblique_max = 0;
        if (!one_chance_in(4))
            oblique_max = 5 + random2(20);      // used elsewhere {dlb}

        unsigned char feature = DNGN_FLOOR;
        if (one_chance_in(10))
            feature = coinflip()? DNGN_DEEP_WATER : DNGN_LAVA;

        octa_room(sr, oblique_max, feature);
    }

    return 2;
}                               // end plan_4()

static char plan_5(void)
{
    unsigned char imax = 5 + random2(20);       // value range of [5,24] {dlb}

    for (unsigned char i = 0; i < imax; i++)
    {
        join_the_dots( random2(GXM - 20) + 10, random2(GYM - 20) + 10,
                       random2(GXM - 20) + 10, random2(GYM - 20) + 10,
                       0, 0, 0, 0 );
    }

    if (!one_chance_in(4))
        spotty_level(true, 100, coinflip());

    return 2;
}                               // end plan_5()

static char plan_6(int level_number)
{
    spec_room sr = { false, false, 0,0,0,0 };

    // circle of standing stones (well, kind of)
    sr.x1 = 10;
    sr.x2 = (GXM - 10);
    sr.y1 = 10;
    sr.y2 = (GYM - 10);

    octa_room(sr, 14, DNGN_FLOOR);

    replace_area(23, 23, 26, 26, DNGN_FLOOR, DNGN_STONE_WALL);
    replace_area(23, 47, 26, 50, DNGN_FLOOR, DNGN_STONE_WALL);
    replace_area(55, 23, 58, 26, DNGN_FLOOR, DNGN_STONE_WALL);
    replace_area(55, 47, 58, 50, DNGN_FLOOR, DNGN_STONE_WALL);
    replace_area(39, 20, 43, 23, DNGN_FLOOR, DNGN_STONE_WALL);
    replace_area(39, 50, 43, 53, DNGN_FLOOR, DNGN_STONE_WALL);
    replace_area(20, 30, 23, 33, DNGN_FLOOR, DNGN_STONE_WALL);
    replace_area(20, 40, 23, 43, DNGN_FLOOR, DNGN_STONE_WALL);
    replace_area(58, 30, 61, 33, DNGN_FLOOR, DNGN_STONE_WALL);
    replace_area(58, 40, 61, 43, DNGN_FLOOR, DNGN_STONE_WALL);

    grd[35][32] = DNGN_STONE_WALL;
    grd[46][32] = DNGN_STONE_WALL;
    grd[35][40] = DNGN_STONE_WALL;
    grd[46][40] = DNGN_STONE_WALL;

    grd[69][34] = DNGN_STONE_STAIRS_DOWN_I;
    grd[69][35] = DNGN_STONE_STAIRS_DOWN_II;
    grd[69][36] = DNGN_STONE_STAIRS_DOWN_III;

    grd[10][34] = DNGN_STONE_STAIRS_UP_I;
    grd[10][35] = DNGN_STONE_STAIRS_UP_II;
    grd[10][36] = DNGN_STONE_STAIRS_UP_III;

    // This "back door" is often one of the easier ways to get out of 
    // pandemonium... the easiest is to use the banish spell.  
    //
    // Note, that although "level_number > 20" will work for most 
    // trips to pandemonium (through regular portals), it won't work 
    // for demonspawn who gate themselves there. -- bwr
    if (((player_in_branch( BRANCH_MAIN_DUNGEON ) && level_number > 20) 
            || you.level_type == LEVEL_PANDEMONIUM) 
        && (coinflip() || you.mutation[ MUT_PANDEMONIUM ]))
    {
        grd[40][36] = DNGN_ENTER_ABYSS; 
        grd[41][36] = DNGN_ENTER_ABYSS;
    }

    return 0;
}                               // end plan_6()

static bool octa_room(spec_room &sr, int oblique_max, unsigned char type_floor)
{
    int x,y;

    // hack - avoid lava in the crypt {gdl}
    if ((player_in_branch( BRANCH_CRYPT ) || player_in_branch( BRANCH_TOMB ))
         && type_floor == DNGN_LAVA)
    {
        type_floor = DNGN_SHALLOW_WATER;
    }

    int oblique = oblique_max;

    // check octagonal room for special; avoid if exists
    for (x = sr.x1; x < sr.x2; x++)
    {
        for (y = sr.y1 + oblique; y < sr.y2 - oblique; y++)
        {
            if (grd[x][y] == DNGN_BUILDER_SPECIAL_WALL)
                return false;
        }

        if (oblique > 0)
            oblique--;

        if (x > sr.x2 - oblique_max)
            oblique += 2;
    }

    oblique = oblique_max;


    for (x = sr.x1; x < sr.x2; x++)
    {
        for (y = sr.y1 + oblique; y < sr.y2 - oblique; y++)
        {
            if (grd[x][y] == DNGN_ROCK_WALL)
                grd[x][y] = type_floor;

            if (grd[x][y] == DNGN_FLOOR && type_floor == DNGN_SHALLOW_WATER)
                grd[x][y] = DNGN_SHALLOW_WATER;

            if (type_floor >= DNGN_LAST_SOLID_TILE
                && grd[x][y] == DNGN_CLOSED_DOOR)
            {
                grd[x][y] = DNGN_FLOOR;       // ick
            }
        }

        if (oblique > 0)
            oblique--;

        if (x > sr.x2 - oblique_max)
            oblique += 2;
    }

    return true;
}                               // end octa_room()

static void labyrinth_level(int level_number)
{
    int temp_rand;              // probability determination {dlb}

    int keep_lx = 0, keep_ly = 0;
    int keep_lx2 = 0, keep_ly2 = 0;
    char start_point_x = 10;
    char start_point_y = 10;
    char going_x = 1;
    char going_y = (coinflip() ? 0 : 1);
    bool do_2 = false;
    int clear_space = 1;
    unsigned char traps_put2 = 0;

    if (coinflip())
    {
        start_point_x = (GXM - 10);
        going_x = -1;
    }

    if (coinflip())
    {
        start_point_y = (GYM - 10);

        if (going_y == 1)
            going_y = -1;
    }

    int lx = start_point_x;
    int ly = start_point_y;

    if (going_y)
        goto do_y;

  do_x:
    traps_put2 = 0;
    clear_space = 0;            // ( coinflip()? 3 : 2 );

    do
    {
        lx += going_x;

        if (grd[lx][ly] == DNGN_ROCK_WALL)
            grd[lx][ly] = DNGN_FLOOR;
    }
    while (lx < (GXM - 8) && lx > 8
           && grd[lx + going_x * (2 + clear_space)][ly] == DNGN_ROCK_WALL);

    going_x = 0;

    if (ly < 32)
        going_y = 1;
    else if (ly > 37)
        going_y = -1;
    else
        goto finishing;

  do_y:                 // if (going_y != 0)
    if (do_2)
    {
        lx = keep_lx2;
        ly = keep_ly2;
    }

    // do_2 = false is the problem
    if (coinflip())
    {
        clear_space = 0;
        do_2 = false;
    }
    else
    {
        clear_space = 2;
        do_2 = true;
    }

    do
    {
        ly += going_y;

        if (grd[lx][ly] == DNGN_ROCK_WALL)
            grd[lx][ly] = DNGN_FLOOR;
    }
    while (ly < (GYM - 8) && ly > 8
           && grd[lx][ly + going_y * (2 + clear_space)] == DNGN_ROCK_WALL);

    keep_lx = lx;
    keep_ly = ly;

    if (lx < 37)
        going_x = 1;
    else if (lx > 42)
        going_x = -1;

    if (ly < 33)
        ly += 2;
    else if (ly > 37)
        ly -= 2;

    clear_space = ((!do_2) ? 6 : 2);

    do
    {
        lx += going_x;

        if (grd[lx][ly] == DNGN_ROCK_WALL)
            grd[lx][ly] = DNGN_FLOOR;
    }
    while (lx < (GXM - 8) && lx > 8
           && grd[lx + going_x * (2 + clear_space)][ly] == DNGN_ROCK_WALL);

    if (do_2)
    {
        keep_lx2 = lx;
        keep_ly2 = ly;
    }

    lx = keep_lx;
    ly = keep_ly;

    going_y = 0;

    if (lx < 37)
        going_x = 1;
    else if (lx > 42)
        going_x = -1;
    else
        goto finishing;

    goto do_x;

  finishing:
    start_point_x = 10 + random2(GXM - 20);

    int treasure_item = 0;

    unsigned char glopop = OBJ_RANDOM;  // used in calling items() {dlb}

    int num_items = 8 + random2avg(9, 2);
    for (int i = 0; i < num_items; i++)
    {
        temp_rand = random2(11);

        glopop = ((temp_rand == 0 || temp_rand == 9)  ? OBJ_WEAPONS :
                  (temp_rand == 1 || temp_rand == 10) ? OBJ_ARMOUR :
                  (temp_rand == 2)                    ? OBJ_MISSILES :
                  (temp_rand == 3)                    ? OBJ_WANDS :
                  (temp_rand == 4)                    ? OBJ_MISCELLANY :
                  (temp_rand == 5)                    ? OBJ_SCROLLS :
                  (temp_rand == 6)                    ? OBJ_JEWELLERY :
                  (temp_rand == 7)                    ? OBJ_BOOKS
                  /* (temp_rand == 8) */              : OBJ_STAVES);

        treasure_item = items( 1, glopop, OBJ_RANDOM, true, 
                               level_number * 3, 250 );

        if (treasure_item != NON_ITEM)
        {
            mitm[treasure_item].x = lx;
            mitm[treasure_item].y = ly;
        }
    }

    mons_place( MONS_MINOTAUR, BEH_SLEEP, MHITNOT, true, lx, ly );

    grd[lx][ly] = DNGN_ROCK_STAIRS_UP;

    link_items();

    // turn rock walls into undiggable stone or metal:
    temp_rand = random2(50);

    unsigned char wall_xform = ((temp_rand > 10) ? DNGN_STONE_WALL   // 78.0%
                                                 : DNGN_METAL_WALL); // 22.0%

    replace_area(0,0,GXM-1,GYM-1,DNGN_ROCK_WALL,wall_xform);

}                               // end labyrinth_level()

static bool is_wall(int x, int y)
{
    unsigned char feat = grd[x][y];

    switch (feat)
    {
        case DNGN_ROCK_WALL:
        case DNGN_STONE_WALL:
        case DNGN_METAL_WALL:
        case DNGN_GREEN_CRYSTAL_WALL:
        case DNGN_WAX_WALL:
            return true;
        default:
            return false;
    }
}


static int box_room_door_spot(int x, int y)
{
    // if there is a door near us embedded in rock, we have to be a door too.
    if ((grd[x-1][y] == DNGN_CLOSED_DOOR && is_wall(x-1,y-1) && is_wall(x-1,y+1))
     || (grd[x+1][y] == DNGN_CLOSED_DOOR && is_wall(x+1,y-1) && is_wall(x+1,y+1))
     || (grd[x][y-1] == DNGN_CLOSED_DOOR && is_wall(x-1,y-1) && is_wall(x+1,y-1))
     || (grd[x][y+1] == DNGN_CLOSED_DOOR && is_wall(x-1,y+1) && is_wall(x+1,y+1)))
    {
        grd[x][y] = DNGN_CLOSED_DOOR;
        return 2;
    }

    // to be a good spot for a door, we need non-wall on two sides and
    // wall on two sides.
    bool nor = is_wall(x, y-1);
    bool sou = is_wall(x, y+1);
    bool eas = is_wall(x-1, y);
    bool wes = is_wall(x+1, y);

    if (nor == sou && eas == wes && nor != eas)
        return 1;

    return 0;
}

static int box_room_doors( int bx1, int bx2, int by1, int by2, int new_doors)
{
    int good_doors[200];        // 1 == good spot,  2 == door placed!
    int spot;
    int i,j;
    int doors_placed = new_doors;

    // sanity
    if ( 2 * ( (bx2 - bx1) + (by2-by1) ) > 200)
        return 0;

    // go through, building list of good door spots,  and replacing wall
    // with door if we're about to block off another door.
    int spot_count = 0;

    // top & bottom
    for(i=bx1+1; i<bx2; i++)
    {
        good_doors[spot_count ++] = box_room_door_spot(i, by1);
        good_doors[spot_count ++] = box_room_door_spot(i, by2);
    }
    // left & right
    for(i=by1+1; i<by2; i++)
    {
        good_doors[spot_count ++] = box_room_door_spot(bx1, i);
        good_doors[spot_count ++] = box_room_door_spot(bx2, i);
    }

    if (new_doors == 0)
    {
        // count # of doors we HAD to place
        for(i=0; i<spot_count; i++)
            if (good_doors[i] == 2)
                doors_placed++;

        return doors_placed;
    }

    while(new_doors > 0 && spot_count > 0)
    {
        spot = random2(spot_count);
        if (good_doors[spot] != 1)
            continue;

        j = 0;
        for(i=bx1+1; i<bx2; i++)
        {
            if (spot == j++)
            {
                grd[i][by1] = DNGN_CLOSED_DOOR;
                break;
            }
            if (spot == j++)
            {
                grd[i][by2] = DNGN_CLOSED_DOOR;
                break;
            }
        }

        for(i=by1+1; i<by2; i++)
        {
            if (spot == j++)
            {
                grd[bx1][i] = DNGN_CLOSED_DOOR;
                break;
            }
            if (spot == j++)
            {
                grd[bx2][i] = DNGN_CLOSED_DOOR;
                break;
            }
        }

        // try not to put a door in the same place twice
        good_doors[spot] = 2;
        new_doors --;
    }

    return doors_placed;
}


static void box_room(int bx1, int bx2, int by1, int by2, int wall_type)
{
    // hack -- avoid lava in the crypt. {gdl}
    if ((player_in_branch( BRANCH_CRYPT ) || player_in_branch( BRANCH_TOMB ))
         && wall_type == DNGN_LAVA)
    {
        wall_type = DNGN_SHALLOW_WATER;
    }

    int temp_rand, new_doors, doors_placed;

    // do top & bottom walls
    replace_area(bx1,by1,bx2,by1,DNGN_FLOOR,wall_type);
    replace_area(bx1,by2,bx2,by2,DNGN_FLOOR,wall_type);

    // do left & right walls
    replace_area(bx1,by1+1,bx1,by2-1,DNGN_FLOOR,wall_type);
    replace_area(bx2,by1+1,bx2,by2-1,DNGN_FLOOR,wall_type);

    // sometimes we have to place doors, or else we shut in other buildings' doors
    doors_placed = box_room_doors(bx1, bx2, by1, by2, 0);

    temp_rand = random2(100);
    new_doors = (temp_rand > 45) ? 2 :
                ((temp_rand > 22) ? 1 : 3);

    // small rooms don't have as many doors
    if ((bx2-bx1)*(by2-by1) < 36)
        new_doors--;

    new_doors -= doors_placed;
    if (new_doors > 0)
        box_room_doors(bx1, bx2, by1, by2, new_doors);
}

static void city_level(int level_number)
{
    int temp_rand;          // probability determination {dlb}
    int wall_type;          // remember, can have many wall types in one level
    int wall_type_room;     // simplifies logic of innermost loop {dlb}

    int xs = 0, ys = 0;
    int x1 = 0, x2 = 0;
    int y1 = 0, y2 = 0;
    int i,j;

    temp_rand = random2(8);

    wall_type = ((temp_rand > 4) ? DNGN_ROCK_WALL :     // 37.5% {dlb}
                 (temp_rand > 1) ? DNGN_STONE_WALL      // 37.5% {dlb}
                                 : DNGN_METAL_WALL);    // 25.0% {dlb}

    if (one_chance_in(100))
        wall_type = DNGN_GREEN_CRYSTAL_WALL;

    make_box( 7, 7, GXM-7, GYM-7, DNGN_FLOOR );

    for (i = 0; i < 5; i++)
    {
        for (j = 0; j < 4; j++)
        {
            xs = 8 + (i * 13);
            ys = 8 + (j * 14);
            x1 = xs + random2avg(5, 2);
            y1 = ys + random2avg(5, 2);
            x2 = xs + 11 - random2avg(5, 2);
            y2 = ys + 11 - random2avg(5, 2);

            temp_rand = random2(280);

            if (temp_rand > 39) // 85.7% draw room(s) {dlb}
            {
                wall_type_room = ((temp_rand > 63) ? wall_type :       // 77.1%
                                  (temp_rand > 54) ? DNGN_STONE_WALL : //  3.2%
                                  (temp_rand > 45) ? DNGN_ROCK_WALL    //  3.2%
                                                   : DNGN_METAL_WALL); //  2.1%

                if (one_chance_in(250))
                    wall_type_room = DNGN_GREEN_CRYSTAL_WALL;

                box_room(x1, x2, y1, y2, wall_type_room);

                // inner room - neat.
                if (x2 - x1 > 5 && y2 - y1 > 5 && one_chance_in(8))
                {
                    box_room(x1 + 2, x2 - 2, y1 + 2, y2 - 2, wall_type);

                    // treasure area.. neat.
                    if (one_chance_in(3))
                        treasure_area(level_number, x1 + 3, x2 - 3, y1 + 3, y2 - 3);
                }
            }
        }
    }

    int stair_count = coinflip() ? 2 : 1;

    for (j = 0; j < stair_count; j++)
    {
        for (i = 0; i < 2; i++)
        {
            place_specific_stair( j + ((i==0) ? DNGN_STONE_STAIRS_DOWN_I
                                              : DNGN_STONE_STAIRS_UP_I) );
        }
    }

}                               // end city_level()

static bool treasure_area(int level_number, unsigned char ta1_x,
                          unsigned char ta2_x, unsigned char ta1_y,
                          unsigned char ta2_y)
{
    int x_count = 0;
    int y_count = 0;
    int item_made = 0;

    ta2_x++;
    ta2_y++;

    if (ta2_x <= ta1_x || ta2_y <= ta1_y)
        return false;

    if ((ta2_x - ta1_x) * (ta2_y - ta1_y) >= 40)
        return false;

    for (x_count = ta1_x; x_count < ta2_x; x_count++)
    {
        for (y_count = ta1_y; y_count < ta2_y; y_count++)
        {
            if (grd[x_count][y_count] != DNGN_FLOOR || coinflip())
                continue;

            item_made = items( 1, OBJ_RANDOM, OBJ_RANDOM, true,
                               random2( level_number * 2 ), 250 );

            if (item_made != NON_ITEM)
            {
                mitm[item_made].x = x_count;
                mitm[item_made].y = y_count;
            }
        }
    }

    return true;
}                               // end treasure_area()

static void diamond_rooms(int level_number)
{
    char numb_diam = 1 + random2(10);
    char type_floor = DNGN_DEEP_WATER;
    int runthru = 0;
    int i, oblique_max;

    // I guess no diamond rooms in either of these places {dlb}:
    if (player_in_branch( BRANCH_DIS ) || player_in_branch( BRANCH_TARTARUS ))
        return;

    if (level_number > 5 + random2(5) && coinflip())
        type_floor = DNGN_SHALLOW_WATER;

    if (level_number > 10 + random2(5) && coinflip())
        type_floor = DNGN_DEEP_WATER;

    if (level_number > 17 && coinflip())
        type_floor = DNGN_LAVA;

    if (level_number > 10 && one_chance_in(15))
        type_floor = (coinflip()? DNGN_STONE_WALL : DNGN_ROCK_WALL);

    if (level_number > 12 && one_chance_in(20))
        type_floor = DNGN_METAL_WALL;

    if (player_in_branch( BRANCH_GEHENNA ))
        type_floor = DNGN_LAVA;
    else if (player_in_branch( BRANCH_COCYTUS ))
        type_floor = DNGN_DEEP_WATER;

    for (i = 0; i < numb_diam; i++)
    {
        spec_room sr = { false, false, 0, 0, 0, 0 };

        sr.x1 = 8 + random2(43);
        sr.y1 = 8 + random2(35);
        sr.x2 = sr.x1 + 5 + random2(15);
        sr.y2 = sr.y1 + 5 + random2(10);

        oblique_max = (sr.x2 - sr.x1) / 2;      //random2(20) + 5;

        if (!octa_room(sr, oblique_max, type_floor))
        {
            runthru++;
            if (runthru > 9)
            {
                runthru = 0;
            }
            else
            {
                i--;
                continue;
            }
        }
    }                           // end "for(bk...)"
}                               // end diamond_rooms()

static void big_room(int level_number)
{
    unsigned char type_floor = DNGN_FLOOR;
    unsigned char type_2 = DNGN_FLOOR;
    int i, j, k, l;

    spec_room sr = { false, false, 0, 0, 0, 0 };
    int oblique;

    if (one_chance_in(4))
    {
        oblique = 5 + random2(20);

        sr.x1 = 8 + random2(30);
        sr.y1 = 8 + random2(22);
        sr.x2 = sr.x1 + 20 + random2(10);
        sr.y2 = sr.y1 + 20 + random2(8);

        // usually floor, except at higher levels
        if (!one_chance_in(5) || level_number < 8 + random2(8))
        {
            octa_room(sr, oblique, DNGN_FLOOR);
            return;
        }

        // default is lava.
        type_floor = DNGN_LAVA;

        if (level_number > 7)
        {
            type_floor = ((random2(level_number) < 14) ? DNGN_DEEP_WATER
                                                       : DNGN_LAVA);
        }

        octa_room(sr, oblique, type_floor);
    }

    // what now?
    sr.x1 = 8 + random2(30);
    sr.y1 = 8 + random2(22);
    sr.x2 = sr.x1 + 20 + random2(10);
    sr.y2 = sr.y1 + 20 + random2(8);

    // check for previous special
    if (find_in_area(sr.x1, sr.y1, sr.x2, sr.y2, DNGN_BUILDER_SPECIAL_WALL))
        return;

    if (level_number > 7 && one_chance_in(4))
    {
        type_floor = ((random2(level_number) < 14) ? DNGN_DEEP_WATER
                                                   : DNGN_LAVA);
    }

    // make the big room.
    replace_area(sr.x1, sr.y1, sr.x2, sr.y2, DNGN_ROCK_WALL, type_floor);
    replace_area(sr.x1, sr.y1, sr.x2, sr.y2, DNGN_CLOSED_DOOR, type_floor);

    if (type_floor == DNGN_FLOOR)
        type_2 = DNGN_ROCK_WALL + random2(4);

    // no lava in the Crypt or Tomb, thanks!
    if (player_in_branch( BRANCH_CRYPT ) || player_in_branch( BRANCH_TOMB ))
    {
        if (type_floor == DNGN_LAVA)
            type_floor = DNGN_SHALLOW_WATER;

        if (type_2 == DNGN_LAVA)
            type_2 = DNGN_SHALLOW_WATER;
    }

    // sometimes make it a chequerboard
    if (one_chance_in(4))
    {
        chequerboard( sr, type_floor, type_floor, type_2 );
    }
    // sometimes make an inside room w/ stone wall.
    else if (one_chance_in(6))
    {
        i = sr.x1;
        j = sr.y1;
        k = sr.x2;
        l = sr.y2;

        do
        {
            i += 2 + random2(3);
            j += 2 + random2(3);
            k -= 2 + random2(3);
            l -= 2 + random2(3);
            // check for too small
            if (i >= k - 3)
                break;
            if (j >= l - 3)
                break;

            box_room(i, k, j, l, DNGN_STONE_WALL);

        }
        while (level_number < 1500);       // ie forever
    }
}                               // end big_room()

// helper function for chequerboard rooms
// note that box boundaries are INclusive
static void chequerboard( spec_room &sr, unsigned char target,  
                          unsigned char floor1, unsigned char floor2 )
{
    int i, j;

    if (sr.x2 < sr.x1 || sr.y2 < sr.y1)
        return;

    for (i = sr.x1; i <= sr.x2; i++)
    {
        for (j = sr.y1; j <= sr.y2; j++)
        {
            if (grd[i][j] == target)
                grd[i][j] = (((i + j) % 2) ? floor2 : floor1);
        }
    }
}                               // end chequerboard()

static void roguey_level(int level_number, spec_room &sr)
{
    int bcount_x, bcount_y;
    int cn = 0;
    int i;

    FixedVector < unsigned char, 30 > rox1;
    FixedVector < unsigned char, 30 > rox2;
    FixedVector < unsigned char, 30 > roy1;
    FixedVector < unsigned char, 30 > roy2;

    for (bcount_y = 0; bcount_y < 5; bcount_y++)
    {
        for (bcount_x = 0; bcount_x < 5; bcount_x++)
        {
            // rooms:
            rox1[cn] = bcount_x * 13 + 8 + random2(4);
            roy1[cn] = bcount_y * 11 + 8 + random2(4);

            rox2[cn] = rox1[cn] + 3 + random2(8);
            roy2[cn] = roy1[cn] + 3 + random2(6);

            // bounds
            if (rox2[cn] > GXM-8)
                rox2[cn] = GXM-8;

            cn++;
        }
    }

    cn = 0;

    for (i = 0; i < 25; i++)
    {
        replace_area( rox1[i], roy1[i], rox2[i], roy2[i], 
                      DNGN_ROCK_WALL, DNGN_FLOOR );

        // inner room?
        if (rox2[i] - rox1[i] > 5 && roy2[i] - roy1[i] > 5)
        {
            if (random2(100 - level_number) < 3)
            {
                if (!one_chance_in(4))
                {
                    box_room( rox1[i] + 2, rox2[i] - 2, roy1[i] + 2,
                              roy2[i] - 2, (coinflip() ? DNGN_STONE_WALL
                                                       : DNGN_ROCK_WALL) );
                }
                else
                {
                    box_room( rox1[i] + 2, rox2[i] - 2, roy1[i] + 2,
                                 roy2[i] - 2, DNGN_METAL_WALL );
                }

                if (coinflip())
                {
                    treasure_area( level_number, rox1[i] + 3, rox2[i] - 3,
                                                 roy1[i] + 3, roy2[i] - 3 );
                }
            }
        }
    }                           // end "for i"

    // Now, join them together:
    FixedVector < char, 2 > pos;
    FixedVector < char, 2 > jpos;

    char doing = 0;

    char last_room = 0;
    int bp;

    for (bp = 0; bp < 2; bp++)
    {
        for (i = 0; i < 25; i++)
        {
            if (bp == 0 && (!(i % 5) || i == 0))
                continue;

            if (bp == 1 && i < 5)
                continue;

            switch (bp)
            {
            case 0:
                last_room = i - 1;
                pos[0] = rox1[i];      // - 1;
                pos[1] = roy1[i] + random2(roy2[i] - roy1[i]);
                jpos[0] = rox2[last_room];      // + 1;
                jpos[1] = roy1[last_room]
                                + random2(roy2[last_room] - roy1[last_room]);
                break;

            case 1:
                last_room = i - 5;
                pos[1] = roy1[i];      // - 1;
                pos[0] = rox1[i] + random2(rox2[i] - rox1[i]);
                jpos[1] = roy2[last_room];      // + 1;
                jpos[0] = rox1[last_room]
                                + random2(rox2[last_room] - rox1[last_room]);
                break;
            }

            while (pos[0] != jpos[0] || pos[1] != jpos[1])
            {
                doing = (coinflip()? 1 : 0);

                if (pos[doing] < jpos[doing])
                    pos[doing]++;
                else if (pos[doing] > jpos[doing])
                    pos[doing]--;

                if (grd[pos[0]][pos[1]] == DNGN_ROCK_WALL)
                    grd[pos[0]][pos[1]] = DNGN_FLOOR;
            }

            if (grd[pos[0]][pos[1]] == DNGN_FLOOR)
            {
                if ((grd[pos[0] + 1][pos[1]] == DNGN_ROCK_WALL
                        && grd[pos[0] - 1][pos[1]] == DNGN_ROCK_WALL)
                    || (grd[pos[0]][pos[1] + 1] == DNGN_ROCK_WALL
                        && grd[pos[0]][pos[1] - 1] == DNGN_ROCK_WALL))
                {
                    grd[pos[0]][pos[1]] = 103;
                }
            }
        }                       // end "for bp, for i"
    }

    // is one of them a special room?
    if (level_number > 8 && one_chance_in(10))
    {
        int spec_room_done = random2(25);

        sr.created = true;
        sr.hooked_up = true;
        sr.x1 = rox1[spec_room_done];
        sr.x2 = rox2[spec_room_done];
        sr.y1 = roy1[spec_room_done];
        sr.y2 = roy2[spec_room_done];
        special_room( level_number, sr );

        // make the room 'special' so it doesn't get overwritten
        // by something else (or put monsters in walls, etc..).

        // top
        replace_area(sr.x1-1, sr.y1-1, sr.x2+1,sr.y1-1, DNGN_ROCK_WALL, DNGN_BUILDER_SPECIAL_WALL);
        replace_area(sr.x1-1, sr.y1-1, sr.x2+1,sr.y1-1, DNGN_FLOOR, DNGN_BUILDER_SPECIAL_FLOOR);
        replace_area(sr.x1-1, sr.y1-1, sr.x2+1,sr.y1-1, DNGN_CLOSED_DOOR, DNGN_BUILDER_SPECIAL_FLOOR);

        // bottom
        replace_area(sr.x1-1, sr.y2+1, sr.x2+1,sr.y2+1, DNGN_ROCK_WALL, DNGN_BUILDER_SPECIAL_WALL);
        replace_area(sr.x1-1, sr.y2+1, sr.x2+1,sr.y2+1, DNGN_FLOOR, DNGN_BUILDER_SPECIAL_FLOOR);
        replace_area(sr.x1-1, sr.y2+1, sr.x2+1,sr.y2+1, DNGN_CLOSED_DOOR, DNGN_BUILDER_SPECIAL_FLOOR);

        // left
        replace_area(sr.x1-1, sr.y1-1, sr.x1-1, sr.y2+1, DNGN_ROCK_WALL, DNGN_BUILDER_SPECIAL_WALL);
        replace_area(sr.x1-1, sr.y1-1, sr.x1-1, sr.y2+1, DNGN_FLOOR, DNGN_BUILDER_SPECIAL_FLOOR);
        replace_area(sr.x1-1, sr.y1-1, sr.x1-1, sr.y2+1, DNGN_CLOSED_DOOR, DNGN_BUILDER_SPECIAL_FLOOR);

        // right
        replace_area(sr.x2+1, sr.y1-1, sr.x2+1, sr.y2+1, DNGN_ROCK_WALL, DNGN_BUILDER_SPECIAL_WALL);
        replace_area(sr.x2+1, sr.y1-1, sr.x2+1, sr.y2+1, DNGN_FLOOR, DNGN_BUILDER_SPECIAL_FLOOR);
        replace_area(sr.x2+1, sr.y1-1, sr.x2+1, sr.y2+1, DNGN_CLOSED_DOOR, DNGN_BUILDER_SPECIAL_FLOOR);
    }

    int stair_count = coinflip() ? 2 : 1;

    for (int j = 0; j < stair_count; j++)
    {
        for (i = 0; i < 2; i++)
        {
            place_specific_stair(j + ((i==0) ? DNGN_STONE_STAIRS_DOWN_I
                                             : DNGN_STONE_STAIRS_UP_I));
        }
    }
}                               // end roguey_level()

static void morgue(spec_room &sr)
{
    int temp_rand = 0;          // probability determination {dlb}
    int x,y;

    for (x = sr.x1; x <= sr.x2; x++)
    {
        for (y = sr.y1; y <= sr.y2; y++)
        {
            if (grd[x][y] == DNGN_FLOOR || grd[x][y] == DNGN_BUILDER_SPECIAL_FLOOR)
            {
                int mon_type;
                temp_rand = random2(24);

                mon_type =  ((temp_rand > 11) ? MONS_ZOMBIE_SMALL :  // 50.0%
                             (temp_rand >  7) ? MONS_WIGHT :         // 16.7%
                             (temp_rand >  3) ? MONS_NECROPHAGE :    // 16.7%
                             (temp_rand >  0) ? MONS_WRAITH          // 12.5%
                                              : MONS_VAMPIRE);       //  4.2%

                mons_place( mon_type, BEH_SLEEP, MHITNOT, true, x, y );
            }
        }
    }
}                               // end morgue()

static bool place_specific_trap(unsigned char spec_x, unsigned char spec_y,
                                unsigned char spec_type)
{
    if (spec_type == TRAP_RANDOM)
        spec_type = random2(NUM_TRAPS);

    for (int tcount = 0; tcount < MAX_TRAPS; tcount++)
    {
        if (env.trap[tcount].type == TRAP_UNASSIGNED)
        {
            env.trap[tcount].type = spec_type;
            env.trap[tcount].x = spec_x;
            env.trap[tcount].y = spec_y;
            grd[spec_x][spec_y] = DNGN_UNDISCOVERED_TRAP;
            return true;
        }

        if (tcount >= MAX_TRAPS - 1)
            return false;
    }

    return false;
}                               // end place_specific_trap()

void define_zombie( int mid, int ztype, int cs, int power )
{
    int mons_sec2 = 0;
    int zombie_size = 0;
    bool ignore_rarity = false;
    int test, cls;

    if (power > 27)
        power = 27;

    // set size based on zombie class (cs)
    switch(cs)
    {
        case MONS_ZOMBIE_SMALL:
        case MONS_SIMULACRUM_SMALL:
        case MONS_SKELETON_SMALL:
            zombie_size = 1;
            break;

        case MONS_ZOMBIE_LARGE:
        case MONS_SIMULACRUM_LARGE:
        case MONS_SKELETON_LARGE:
            zombie_size = 2;
            break;

        case MONS_SPECTRAL_THING:
            zombie_size = -1;
            break;

        default:
            // this should NEVER happen.
            perror("\ncreate_zombie() got passed incorrect zombie type!\n");
            end(0);
            break;
    }

    // that is, random creature from which to fashion undead
    if (ztype == 250)
    {
        // how OOD this zombie can be.
        int relax = 5;

        // pick an appropriate creature to make a zombie out of,
        // levelwise.  The old code was generating absolutely
        // incredible OOD zombies.
        while(true)
        {
            // this limit can be updated if mons->number goes >8 bits..
            test = random2(182);            // not guaranteed to be valid, so..
            cls = mons_charclass(test);
            if (cls == MONS_PROGRAM_BUG)
                continue;

            // on certain branches,  zombie creation will fail if we use
            // the mons_rarity() functions,  because (for example) there
            // are NO zombifiable "native" abyss creatures. Other branches
            // where this is a problem are hell levels and the crypt.
            // we have to watch for summoned zombies on other levels, too,
            // such as the Temple, HoB, and Slime Pits.
            if (you.level_type != LEVEL_DUNGEON
                || player_in_hell()
                || player_in_branch( BRANCH_HALL_OF_ZOT )
                || player_in_branch( BRANCH_VESTIBULE_OF_HELL )
                || player_in_branch( BRANCH_ECUMENICAL_TEMPLE )
                || player_in_branch( BRANCH_CRYPT )
                || player_in_branch( BRANCH_TOMB )
                || player_in_branch( BRANCH_HALL_OF_BLADES )
                || player_in_branch( BRANCH_SNAKE_PIT )
                || player_in_branch( BRANCH_SLIME_PITS )
                || one_chance_in(1000))
            {
                ignore_rarity = true;
            }

            // don't make out-of-rarity zombies when we don't have to
            if (!ignore_rarity && mons_rarity(cls) == 0)
                continue;

            // monster class must be zombifiable
            if (!mons_zombie_size(cls))
                continue;

            // if skeleton, monster must have a skeleton
            if ((cs == MONS_SKELETON_SMALL || cs == MONS_SKELETON_LARGE)
                && !mons_skeleton(cls))
            {
                continue;
            }

            // size must match, but you can make a spectral thing out of anything.
            if (mons_zombie_size(cls) != zombie_size && zombie_size >= 0)
                continue;

            // hack -- non-dungeon zombies are always made out of nastier
            // monsters
            if (you.level_type != LEVEL_DUNGEON && mons_power(cls) > 8)
                break;

            // check for rarity.. and OOD - identical to mons_place()
            int level, diff, chance;

            level  = mons_level( cls ) - 4;
            diff   = level - power;

            chance = (ignore_rarity) ? 100 
                                     : mons_rarity(cls) - (diff * diff) / 2;

            if (power > level - relax && power < level + relax
                && random2avg(100, 2) <= chance)
            {
                break;
            }

            // every so often,  we'll relax the OOD restrictions.  Avoids
            // infinite loops (if we don't do this,  things like creating
            // a large skeleton on level 1 may hang the game!
            if (one_chance_in(5))
                relax++;
        }

        // set type and secondary appropriately
        menv[mid].number = cls;
        mons_sec2 = cls;
    }
    else
    {
        menv[mid].number = mons_charclass(ztype);
        mons_sec2 = menv[mid].number;
    }

    menv[mid].type = menv[mid].number;

    define_monster(mid);

    menv[mid].hit_points = hit_points( menv[mid].hit_dice, 6, 5 );
    menv[mid].max_hit_points = menv[mid].hit_points;

    menv[mid].armour_class -= 2;

    if (menv[mid].armour_class < 0)
        menv[mid].armour_class = 0;

    menv[mid].evasion -= 5;

    if (menv[mid].evasion < 0)
        menv[mid].evasion = 0;

    menv[mid].speed -= 2;

    if (menv[mid].speed < 3)
        menv[mid].speed = 3;

    menv[mid].speed_increment = 70;
    menv[mid].number = mons_sec2;

    if (cs == MONS_ZOMBIE_SMALL || cs == MONS_ZOMBIE_LARGE)
    {
        menv[mid].type = ((mons_zombie_size(menv[mid].number) == 2)
                                    ? MONS_ZOMBIE_LARGE : MONS_ZOMBIE_SMALL);
    }
    else if (cs == MONS_SKELETON_SMALL || cs == MONS_SKELETON_LARGE)
    {
        menv[mid].hit_points = hit_points( menv[mid].hit_dice, 5, 4 );
        menv[mid].max_hit_points = menv[mid].hit_points;

        menv[mid].armour_class -= 4;

        if (menv[mid].armour_class < 0)
            menv[mid].armour_class = 0;

        menv[mid].evasion -= 2;

        if (menv[mid].evasion < 0)
            menv[mid].evasion = 0;

        menv[mid].type = ((mons_zombie_size( menv[mid].number ) == 2)
                            ? MONS_SKELETON_LARGE : MONS_SKELETON_SMALL);
    }
    else if (cs == MONS_SIMULACRUM_SMALL || cs == MONS_SIMULACRUM_LARGE)
    {
        // Simulacrum aren't tough, but you can create piles of them. -- bwr 
        menv[mid].hit_points = hit_points( menv[mid].hit_dice, 1, 4 );
        menv[mid].max_hit_points = menv[mid].hit_points;
        menv[mid].type = ((mons_zombie_size( menv[mid].number ) == 2)
                            ? MONS_SIMULACRUM_LARGE : MONS_SIMULACRUM_SMALL);
    }
    else if (cs == MONS_SPECTRAL_THING)
    {
        menv[mid].hit_points = hit_points( menv[mid].hit_dice, 4, 4 );
        menv[mid].max_hit_points = menv[mid].hit_points;
        menv[mid].armour_class += 4;
        menv[mid].type = MONS_SPECTRAL_THING;
    }

    menv[mid].number = mons_sec2;
}                               // end define_zombie()

#ifdef USE_RIVERS

static void build_river( unsigned char river_type ) //mv
{
    int i,j;
    int y, width;

    if (player_in_branch( BRANCH_CRYPT ) || player_in_branch( BRANCH_TOMB ))
        return;

    // if (one_chance_in(10)) 
    //     build_river(river_type); 

    // Made rivers less wide... min width five rivers were too annoying. -- bwr
    width = 3 + random2(4);
    y = 10 - width + random2avg( GYM-10, 3 );

    for (i = 5; i < (GXM - 5); i++)
    {
        if (one_chance_in(3))   y++;
        if (one_chance_in(3))   y--;
        if (coinflip())         width++;
        if (coinflip())         width--;

        if (width < 2) width = 2;
        if (width > 6) width = 6;

        for (j = y; j < y+width ; j++)
        {
            if (j >= 5 && j <= GYM - 5)
            {
                // Note that vaults might have been created in this area!
                // So we'll avoid the silliness of orcs/royal jelly on
                // lava and deep water grids. -- bwr
                if (!one_chance_in(200)
                    // && grd[i][j] == DNGN_FLOOR
                    && mgrd[i][j] == NON_MONSTER
                    && igrd[i][j] == NON_ITEM)
                {
                    if (width == 2 && river_type == DNGN_DEEP_WATER
                        && coinflip())
                    {
                        grd[i][j] = DNGN_SHALLOW_WATER;
                    }
                    else
                        grd[i][j] = river_type;
                }
            }
        }
    }
}                               // end build_river()

static void build_lake(unsigned char lake_type) //mv
{
    int i, j;
    int x1, y1, x2, y2;

    if (player_in_branch( BRANCH_CRYPT ) || player_in_branch( BRANCH_TOMB ))
        return;

    // if (one_chance_in (10))
    //      build_lake(lake_type); 

    x1 = 5 + random2(GXM - 30);
    y1 = 5 + random2(GYM - 30);
    x2 = x1 + 4 + random2(16);
    y2 = y1 + 8 + random2(12);
    // mpr("lake");

    for (j = y1; j < y2; j++)
    {
        if (coinflip())  x1 += random2(3);
        if (coinflip())  x1 -= random2(3);
        if (coinflip())  x2 += random2(3);
        if (coinflip())  x2 -= random2(3);

    //  mv: this does much more worse effects
    //    if (coinflip()) x1 = x1 -2 + random2(5);
    //    if (coinflip()) x2 = x2 -2 + random2(5);

        if ((j-y1) < ((y2-y1) / 2))
        {
            x2 += random2(3);
            x1 -= random2(3);
        }
        else
        {
            x2 -= random2(3);
            x1 += random2(3);
        }

        for (i = x1; i < x2 ; i++)
        {
            if ((j >= 5 && j <= GYM - 5) && (i >= 5 && i <= GXM - 5))
            {
                // Note that vaults might have been created in this area!
                // So we'll avoid the silliness of monsters and items 
                // on lava and deep water grids. -- bwr
                if (!one_chance_in(200)
                    // && grd[i][j] == DNGN_FLOOR
                    && mgrd[i][j] == NON_MONSTER
                    && igrd[i][j] == NON_ITEM)
                {
                    grd[i][j] = lake_type;
                }
            }
        }
    }
}                               // end lake()

#endif // USE_RIVERS
