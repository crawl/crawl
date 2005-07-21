/*
 *  File:       abyss.cc
 *  Summary:    Misc functions (most of which don't appear to be related to priests).
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *     <2>     10/11/99       BCR     Added Daniel's crash patch
 *     <1>     -/--/--        LRH     Created
 */

#include "AppHdr.h"
#include "abyss.h"

#include <stdlib.h>

#include "externs.h"

#include "cloud.h"
#include "monplace.h"
#include "dungeon.h"
#include "items.h"
#include "lev-pand.h"
#include "randart.h"
#include "stuff.h"

// public for abyss generation
void generate_abyss(void)
{
    int i, j;                   // loop variables
    int temp_rand;              // probability determination {dlb}

    for (i = 5; i < (GXM - 5); i++)
    {
        for (j = 5; j < (GYM - 5); j++)
        {
            temp_rand = random2(4000);

            grd[i][j] = ((temp_rand > 999) ? DNGN_FLOOR :       // 75.0%
                         (temp_rand > 400) ? DNGN_ROCK_WALL :   // 15.0%
                         (temp_rand > 100) ? DNGN_STONE_WALL :  //  7.5%
                         (temp_rand >   0) ? DNGN_METAL_WALL    //  2.5%
                                           : DNGN_CLOSED_DOOR); // 1 in 4000
        }
    }

    grd[45][35] = DNGN_FLOOR;
}                               // end generate_abyss()


static void generate_area(unsigned char gx1, unsigned char gy1,
                          unsigned char gx2, unsigned char gy2)
{
    unsigned char i, j;
    unsigned char x1, x2, y1, y2;
    unsigned char items_placed = 0;
    int thickness = random2(70) + 30;
    int thing_created;
    unsigned int rooms_done = 0;
    unsigned int rooms_to_do = 0;

    int temp_rand;              // probability determination {dlb}

    FixedVector < unsigned char, 5 > replaced;

    // nuke map
    for (i = 0; i < GXM; i++)
    {
        for (j = 0; j < GYM; j++)
        {
            env.map[i][j] = 0;
        }
    }

    // generate level composition vector
    for (i = 0; i < 5; i++)
    {
        temp_rand = random2(10000);

        replaced[i] = ((temp_rand > 4926) ? DNGN_ROCK_WALL :    // 50.73%
                       (temp_rand > 2918) ? DNGN_STONE_WALL :   // 20.08%
                       (temp_rand > 2004) ? DNGN_METAL_WALL :   //  9.14%
                       (temp_rand > 1282) ? DNGN_LAVA :         //  7.22%
                       (temp_rand > 616)  ? DNGN_SHALLOW_WATER ://  6.66%
                       (temp_rand > 15)   ? DNGN_DEEP_WATER     //  6.01%
                                          : DNGN_CLOSED_DOOR);  //  0.16%
    }

    if (one_chance_in(3))
    {
        rooms_to_do = 1 + random2(10);

        while(true)
        {
            x1 = 10 + random2(GXM - 20);
            y1 = 10 + random2(GYM - 20);
            x2 = x1 + 1 + random2(10);
            y2 = y1 + 1 + random2(10);

            if (one_chance_in(100))
                goto out_of_rooms;

            for (i = x1; i < x2; i++)   // that is, [10,(GXM-1)]  {dlb}
            {
                for (j = y1; j < y2; j++)       // that is, [10,(GYM-1)]  {dlb}
                {
                    if (grd[i][j] != DNGN_UNSEEN)
                        goto continued;
                }
            }

            for (i = x1; i < x2; i++)   // that is, [10,(GXM-1)]  {dlb}
            {
                for (j = y1; j < y2; j++)       // that is, [10,(GYM-1)]  {dlb}
                {
                    grd[i][j] = DNGN_FLOOR;
                }
            }

          continued:
            rooms_done++;

            if (rooms_done >= rooms_to_do)
                break;
        }
    }

  out_of_rooms:
    for (i = gx1; i < gx2 + 1; i++)
    {
        for (j = gy1; j < gy2 + 1; j++)
        {
            if (grd[i][j] == DNGN_UNSEEN && random2(100) <= thickness)
            {
                grd[i][j] = DNGN_FLOOR;

                if (items_placed < 150 && one_chance_in(200))
                {
                    if (one_chance_in(500))
                    {
                        thing_created = items(1, OBJ_MISCELLANY, 
                                              MISC_RUNE_OF_ZOT, true, 51, 51);
                    }
                    else
                    {
                        thing_created = items(1, OBJ_RANDOM, OBJ_RANDOM, true,
                                              51, 250);
                    }

                    move_item_to_grid( &thing_created, i, j );

                    if (thing_created != NON_ITEM)
                        items_placed++;
                }
            }
        }
    }

    for (i = gx1; i < gx2 + 1; i++)
    {
        for (j = gy1; j < gy2 + 1; j++)
        {
            if (grd[i][j] == DNGN_UNSEEN)
                grd[i][j] = replaced[random2(5)];

            if (one_chance_in(7500))
                grd[i][j] = DNGN_EXIT_ABYSS;

            if (one_chance_in(10000))
            {
                do
                {
                    grd[i][j] = DNGN_ALTAR_ZIN + random2(12);
                }
                while (grd[i][j] == DNGN_ALTAR_YREDELEMNUL
                       || grd[i][j] == DNGN_ALTAR_VEHUMET
                       || grd[i][j] == DNGN_ALTAR_NEMELEX_XOBEH);
            }
        }
    }
}


void area_shift(void)
/*******************/
{
    for (unsigned int i = 0; i < MAX_MONSTERS; i++)
    {
        if (menv[i].type == -1)
        {
            continue;
        }

        // remove non-nearby monsters
        if (menv[i].x < you.x_pos - 10
            || menv[i].x >= you.x_pos + 11
            || menv[i].y < you.y_pos - 10 || menv[i].y >= you.y_pos + 11)
        {
            menv[i].type = -1;

            mgrd[menv[i].x][menv[i].y] = NON_MONSTER;

            for (unsigned int j = 0; j < NUM_MONSTER_SLOTS; j++)
            {
                if (menv[i].inv[j] != NON_ITEM)
                {
                    destroy_item( menv[i].inv[j] );
                    menv[i].inv[j] = NON_ITEM;
                }
            }
        }
    }

    for (int i = 5; i < (GXM - 5); i++)
    {
        for (int j = 5; j < (GYM - 5); j++)
        {
            // don't modify terrain by player
            if (i >= you.x_pos - 10 && i < you.x_pos + 11
                && j >= you.y_pos - 10 && j < you.y_pos + 11)
            {
                continue;
            }

            // nuke terrain otherwise
            grd[i][j] = DNGN_UNSEEN;

            // nuke items
            destroy_item_stack( i, j );
        }
    }

    // shift all monsters & items to new area
    for (int i = you.x_pos - 10; i < you.x_pos + 11; i++)
    {
        if (i < 0 || i >= GXM)
            continue;

        for (int j = you.y_pos - 10; j < you.y_pos + 11; j++)
        {
            if (j < 0 || j >= GYM)
                continue;

            const int ipos = 45 + i - you.x_pos;
            const int jpos = 35 + j - you.y_pos;

            // move terrain
            grd[ipos][jpos] = grd[i][j];

            // move item
            move_item_stack_to_grid( i, j, ipos, jpos );

            // move monster
            mgrd[ipos][jpos] = mgrd[i][j];
            if (mgrd[i][j] != NON_MONSTER)
            {
                menv[mgrd[ipos][jpos]].x = ipos;
                menv[mgrd[ipos][jpos]].y = jpos;
                mgrd[i][j] = NON_MONSTER;
            }

            // move cloud
            if (env.cgrid[i][j] != EMPTY_CLOUD)
                move_cloud( env.cgrid[i][j], ipos, jpos );
        }
    }


    for (unsigned int i = 0; i < MAX_CLOUDS; i++)
    {
        if (env.cloud[i].type == CLOUD_NONE)
            continue;

        if (env.cloud[i].x < 35 || env.cloud[i].x > 55
            || env.cloud[i].y < 25 || env.cloud[i].y > 45)
        {
            delete_cloud( i );
        }
    }

    you.x_pos = 45;
    you.y_pos = 35;

    generate_area(5, 5, (GXM - 5), (GYM - 5));

    for (unsigned int mcount = 0; mcount < 15; mcount++)
    {
        mons_place( RANDOM_MONSTER, BEH_HOSTILE, MHITNOT, false, 1, 1, 
                    LEVEL_ABYSS, PROX_AWAY_FROM_PLAYER ); // PROX_ANYWHERE?
    }
}


void abyss_teleport( bool new_area )
/**********************************/
{
    int x, y, i, j, k;

    if (!new_area)
    {
        // try to find a good spot within the shift zone:
        for (i = 0; i < 100; i++)
        {
            x = 16 + random2( GXM - 32 );
            y = 16 + random2( GYM - 32 );

            if ((grd[x][y] == DNGN_FLOOR 
                    || grd[x][y] == DNGN_SHALLOW_WATER)
                && mgrd[x][y] == NON_MONSTER
                && env.cgrid[x][y] == EMPTY_CLOUD)
            {
                break;
            }
        }

        if (i < 100)
        {
            you.x_pos = x;
            you.y_pos = y;
            return;
        }
    }

    // teleport to a new area of the abyss:

    init_pandemonium();                         /* changes colours */

    env.floor_colour = (mcolour[env.mons_alloc[9]] == BLACK)
                                ? LIGHTGREY : mcolour[env.mons_alloc[9]];

    env.rock_colour = (mcolour[env.mons_alloc[8]] == BLACK)
                                ? LIGHTGREY : mcolour[env.mons_alloc[8]];

    // Orbs and fixed artefacts are marked as "lost in the abyss"
    for (k = 0; k < MAX_ITEMS; k++)
    {
        if (is_valid_item( mitm[k] ))
        {
            if (mitm[k].base_type == OBJ_ORBS)
            {
                set_unique_item_status( OBJ_ORBS, mitm[k].sub_type, 
                                        UNIQ_LOST_IN_ABYSS );
            }
            else if (is_fixed_artefact( mitm[k] ))
            {
                set_unique_item_status( OBJ_WEAPONS, mitm[k].special, 
                                        UNIQ_LOST_IN_ABYSS );
            }

            destroy_item( k );
        }
    }

    for (i = 0; i < MAX_MONSTERS; i++)
        menv[i].type = -1;

    for (i = 0; i < MAX_CLOUDS; i++)
        delete_cloud( i );

    for (i = 10; i < (GXM - 9); i++)
    {
        for (j = 10; j < (GYM - 9); j++)
        {
            grd[i][j] = DNGN_UNSEEN;    // so generate_area will pick it up
            igrd[i][j] = NON_ITEM;
            mgrd[i][j] = NON_MONSTER;
            env.cgrid[i][j] = EMPTY_CLOUD;
        }
    }

    ASSERT( env.cloud_no == 0 ); 

    you.x_pos = 45;
    you.y_pos = 35;

    generate_area( 10, 10, (GXM - 10), (GYM - 10) );

    grd[you.x_pos][you.y_pos] = DNGN_FLOOR;
}
