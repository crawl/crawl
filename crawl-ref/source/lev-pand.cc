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
        env.mons_alloc[pc] = random_choose(
                                MONS_WHITE_IMP,
                                MONS_LEMURE,
                                MONS_UFETUBUS,
                                MONS_IRON_IMP,
                                MONS_NEQOXEC,
                                MONS_ORANGE_DEMON,
                                MONS_HELLWING,
                                MONS_SMOKE_DEMON,
                                MONS_YNOXINUL,
                                MONS_ABOMINATION_SMALL,
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
        env.mons_alloc[7 + random2(3)] = MONS_HELL_SENTINEL;

    // Set at least some specific monsters for the special levels - this
    // can also be used to set some colours.

    env.floor_colour = BLACK;
    env.rock_colour  = BLACK;
    dgn_set_colours_from_monsters();
}

void pandemonium_mons(void)
{
    // must leave allowance for monsters rare on pandemonium (wizards, etc.)
    monster_type pan_mons = env.mons_alloc[random2(10)];

    if (one_chance_in(40))
        pan_mons = pick_monster_no_rarity(BRANCH_PANDEMONIUM);

    mgen_data mg(pan_mons);
    mg.place = level_id(BRANCH_PANDEMONIUM);
    mg.flags |= MG_PERMIT_BANDS;

    mons_place(mg);
}
