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
        env.mons_alloc[pc] = random_choose_weighted(
                           188442, MONS_ORANGE_DEMON,
                           188442, MONS_HELLWING,
                           188442, MONS_SMOKE_DEMON,
                           188442, MONS_YNOXINUL,
                           188442, MONS_ABOMINATION_LARGE,

                            94221, MONS_NEQOXEC,
                            94221, MONS_MONSTROUS_DEMONSPAWN,
                            94221, MONS_GELID_DEMONSPAWN,
                            94221, MONS_INFERNAL_DEMONSPAWN,
                            94221, MONS_PUTRID_DEMONSPAWN,
                            94221, MONS_TORTUROUS_DEMONSPAWN,

                            91200, MONS_SIXFIRHY,

                            57760, MONS_RED_DEVIL,

                            24000, MONS_DEMONIC_CRAWLER,
                            24000, MONS_SUN_DEMON,
                            24000, MONS_SHADOW_DEMON,
                            24000, MONS_LOROCYPROCA,

                            20938, MONS_HELLION,
                            20938, MONS_TORMENTOR,
                            20938, MONS_REAPER,
                            20938, MONS_SOUL_EATER,
                            20938, MONS_ICE_DEVIL,
                            20938, MONS_BLUE_DEVIL,
                            20938, MONS_HELL_BEAST,
                            20938, MONS_RUST_DEVIL,
                                0);

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
                  2, MONS_BLOOD_SAINT,
                  2, MONS_WARMONGER,
                  2, MONS_CORRUPTER,
                  2, MONS_BLACK_SUN,
                  0);
        }
    }

    if (one_chance_in(10))
        env.mons_alloc[7 + random2(3)] = MONS_BRIMSTONE_FIEND;

    if (one_chance_in(10))
        env.mons_alloc[7 + random2(3)] = MONS_ICE_FIEND;

    if (one_chance_in(10))
        env.mons_alloc[7 + random2(3)] = MONS_SHADOW_FIEND;

    if (one_chance_in(10))
        env.mons_alloc[7 + random2(3)] = MONS_HELL_SENTINEL;

    if (one_chance_in(10))
        env.mons_alloc[7 + random2(3)] = MONS_CHAOS_CHAMPION;

    env.floor_colour = _pan_floor_colour();
    env.rock_colour  = _pan_rock_colour();
    tile_init_default_flavour();
}
