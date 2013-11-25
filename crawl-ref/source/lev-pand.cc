/**
 * @file
 * @brief Functions used in Pandemonium.
**/

#include "AppHdr.h"

#include <algorithm>

#include "lev-pand.h"

#include "externs.h"
#include "colour.h"
#include "env.h"
#include "mon-place.h"
#include "mgen_data.h"
#include "random.h"

static colour_t _pan_floor_colour()
{
    colour_t col;

    do
        col = random_colour();
    // Don't use silence or halo colours for floors.
    while (col == DARKGREY || col == CYAN || col == YELLOW);

    return col;
}

static colour_t _pan_rock_colour()
{
    colour_t col;

    do
        col = random_colour();
    // Don't use stone or metal colours for walls.
    while (col == DARKGREY || col == LIGHTGREY || col == CYAN);

    return col;
}

void init_pandemonium(void)
{
    for (int pc = 0; pc < PAN_MONS_ALLOC; ++pc)
    {
        env.mons_alloc[pc] = random_choose(
                                MONS_NEQOXEC,
                                MONS_ORANGE_DEMON,
                                MONS_HELLWING,
                                MONS_SMOKE_DEMON,
                                MONS_YNOXINUL,
                                MONS_ABOMINATION_LARGE,
                                -1);

        if (one_chance_in(10))
        {
            env.mons_alloc[pc] = random_choose(
                                    MONS_HELLION,
                                    MONS_ROTTING_DEVIL,
                                    MONS_TORMENTOR,
                                    MONS_REAPER,
                                    MONS_SOUL_EATER,
                                    MONS_ICE_DEVIL,
                                    MONS_BLUE_DEVIL,
                                    MONS_HELL_BEAST,
                                    MONS_IRON_DEVIL,
                                    -1);
        }

        if (one_chance_in(30))
            env.mons_alloc[pc] = MONS_RED_DEVIL;

        if (one_chance_in(30))
            env.mons_alloc[pc] = MONS_SIXFIRHY;

        if (one_chance_in(20))
        {
            env.mons_alloc[pc] = random_choose(
                                    MONS_DEMONIC_CRAWLER,
                                    MONS_SUN_DEMON,
                                    MONS_SHADOW_DEMON,
                                    MONS_LOROCYPROCA,
                                    -1);
        }

        // The last three slots have a good chance of big badasses.
        if (pc == 7 && one_chance_in(8)
         || pc == 8 && one_chance_in(5)
         || pc == 9 && one_chance_in(3))
        {
            env.mons_alloc[pc] = random_choose(
                    MONS_EXECUTIONER,
                    MONS_GREEN_DEATH,
                    MONS_BLIZZARD_DEMON,
                    MONS_BALRUG,
                    MONS_CACODEMON,
                    -1);
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

    env.floor_colour = _pan_floor_colour();
    env.rock_colour  = _pan_rock_colour();
}
