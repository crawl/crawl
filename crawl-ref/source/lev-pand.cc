/**
 * @file
 * @brief Functions used in Pandemonium.
**/

#include "AppHdr.h"

#include <algorithm>

#include "lev-pand.h"

#include "externs.h"
#include "dungeon.h"
#include "env.h"
#include "mon-place.h"
#include "mgen_data.h"
#include "mon-pick.h"
#include "random.h"

void init_pandemonium(void)
{
    // colour of monster 9 is colour of floor, 8 is colour of rock
    // IIRC, BLACK is set to LIGHTGREY

    for (int pc = 0; pc < 10; ++pc)
    {
        switch (random2(17))
        {
        case 0: case 10: env.mons_alloc[pc] = MONS_WHITE_IMP;         break;
        case 1: case 11: env.mons_alloc[pc] = MONS_LEMURE;            break;
        case 2: case 12: env.mons_alloc[pc] = MONS_UFETUBUS;          break;
        case 3: case 13: env.mons_alloc[pc] = MONS_IRON_IMP;          break;
        case 4: case 14: env.mons_alloc[pc] = MONS_MIDGE;             break;
        case 5:          env.mons_alloc[pc] = MONS_NEQOXEC;           break;
        case 6:          env.mons_alloc[pc] = MONS_ORANGE_DEMON;      break;
        case 7:          env.mons_alloc[pc] = MONS_HELLWING;          break;
        case 8:          env.mons_alloc[pc] = MONS_SMOKE_DEMON;       break;
        case 9:          env.mons_alloc[pc] = MONS_YNOXINUL;          break;
        case 15:         env.mons_alloc[pc] = MONS_ABOMINATION_SMALL; break;
        case 16:         env.mons_alloc[pc] = MONS_ABOMINATION_LARGE; break;
        }

        if (one_chance_in(10))
        {
            env.mons_alloc[pc] = random_choose(
                                    MONS_HELLION,
                                    MONS_ROTTING_DEVIL,
                                    MONS_TORMENTOR,
                                    MONS_REAPER,
                                    MONS_SOUL_EATER,
                                    MONS_HAIRY_DEVIL,
                                    MONS_ICE_DEVIL,
                                    MONS_BLUE_DEVIL,
                                    MONS_HELL_BEAST,
                                    MONS_IRON_DEVIL,
                                    -1);
        }

        if (one_chance_in(30))
            env.mons_alloc[pc] = MONS_RED_DEVIL;

        if (one_chance_in(30))
            env.mons_alloc[pc] = MONS_CRIMSON_IMP;

        if (one_chance_in(30))
            env.mons_alloc[pc] = MONS_SIXFIRHY;

        if (one_chance_in(20))
        {
            env.mons_alloc[pc] = random_choose(
                                    MONS_DEMONIC_CRAWLER,
                                    MONS_SUN_DEMON,
                                    MONS_SHADOW_IMP,
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
        env.mons_alloc[7 + random2(3)] = MONS_PIT_FIEND;

    // Set at least some specific monsters for the special levels - this
    // can also be used to set some colours.

    env.floor_colour = BLACK;
    env.rock_colour  = BLACK;
    dgn_set_colours_from_monsters();
}

void pandemonium_mons(void)
{
    // must leave allowance for monsters rare on pandemonium (wizards, etc.)
    int pan_mons = env.mons_alloc[random2(10)];

    if (one_chance_in(40))
    {
        do
            pan_mons = random2(NUM_MONSTERS);
        while (!mons_pan_rare(pan_mons));
    }
    mgen_data mg(static_cast<monster_type>(pan_mons));
    mg.level_type = LEVEL_PANDEMONIUM;
    mg.flags |= MG_PERMIT_BANDS;

    mons_place(mg);
}
