/**
 * @file
 * @brief Functions used in Pandemonium.
**/

#include "AppHdr.h"

#include "lev-pand.h"

#include <algorithm>

#include "colour.h"
#include "env.h"
#include "tileview.h"

static colour_t _pan_floor_colour()
{
    colour_t col;

    do
    {
        col = random_colour();
    }
    // Don't use silence or halo colours for floors.
    while (col == DARKGREY || col == CYAN || col == YELLOW);

    return col;
}

static colour_t _pan_rock_colour()
{
    colour_t col;

    do
    {
        col = random_colour();
    }
    // Don't use stone or metal colours for walls.
    while (col == DARKGREY || col == LIGHTGREY || col == CYAN);

    return col;
}

void init_pandemonium()
{
    for (int pc = 0; pc < PAN_MONS_ALLOC; ++pc)
    {
        env.mons_alloc[pc] = random_choose_weighted(5, MONS_SMOKE_DEMON,
                                                    4, MONS_YNOXINUL,
                                                    5, MONS_ABOMINATION_LARGE,
                                                    5, MONS_SOUL_EATER,
                                                    5, MONS_DEMONIC_CRAWLER,
                                                    5, MONS_SUN_DEMON,
                                                    2, MONS_NEQOXEC,
                                                    5, MONS_CHAOS_SPAWN,
                                                    1, MONS_SHADOW_DEMON,
                                                    2, MONS_SIN_BEAST,
                                                    1, MONS_HELLION,
                                                    1, MONS_TORMENTOR,
                                                    1, MONS_REAPER,
                                                    2, MONS_NEKOMATA);

        // The last three slots have a good chance of big badasses.
        if (pc == 7 && one_chance_in(8)
         || pc == 8 && one_chance_in(5)
         || pc == 9 && one_chance_in(3))
        {
            env.mons_alloc[pc] = random_choose_weighted(
                  4, MONS_EXECUTIONER,
                  4, MONS_GREEN_DEATH,
                  4, MONS_BLIZZARD_DEMON,
                  4, MONS_BALRUG,
                  4, MONS_CACODEMON,
                  2, MONS_DEMONSPAWN_BLOOD_SAINT,
                  2, MONS_DEMONSPAWN_WARMONGER,
                  2, MONS_DEMONSPAWN_CORRUPTER,
                  2, MONS_DEMONSPAWN_SOUL_SCHOLAR);
        }
    }

    if (one_chance_in(10))
        env.mons_alloc[7 + random2(3)] = MONS_BRIMSTONE_FIEND;

    if (one_chance_in(10))
        env.mons_alloc[7 + random2(3)] = MONS_ICE_FIEND;

    if (one_chance_in(10))
        env.mons_alloc[7 + random2(3)] = MONS_TZITZIMITL;

    if (one_chance_in(10))
        env.mons_alloc[7 + random2(3)] = MONS_HELL_SENTINEL;

    env.floor_colour = _pan_floor_colour();
    env.rock_colour  = _pan_rock_colour();
    tile_init_default_flavour();
}
