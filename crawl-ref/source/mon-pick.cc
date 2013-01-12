/**
 * @file
 * @brief Functions used to help determine which monsters should appear.
**/

#include "AppHdr.h"

#include "mon-pick.h"

#include "externs.h"
#include "branch.h"
#include "errors.h"
#include "libutil.h"
#include "mon-util.h"
#include "place.h"

enum distrib_type
{
    FLAT, // full chance throughout the range
    SEMI, // 50% chance at range ends, 100% in the middle
    PEAK, // 0% chance just outside range ends, 100% in the middle, range
          // ends typically get ~20% or more
};

typedef struct
{
    int minr;
    int maxr;
    int rarity; // 0..1000
    distrib_type distrib;
    monster_type mons;
} pop_entry;

#include "mon-pick-data.h"

// NOTE: The lower the level the earlier a monster may appear.
int mons_depth(monster_type mcls, branch_type branch)
{
    // legacy function, until ZotDef is ported
    for (const pop_entry *pe = population[branch].pop; pe->mons; pe++)
        if (pe->mons == mcls)
            return (pe->minr + pe->maxr) / 2;

    return DEPTH_NOWHERE;
}

// NOTE: Higher values returned means the monster is "more common".
// A return value of zero means the monster will never appear. {dlb}
int mons_rarity(monster_type mcls, branch_type branch)
{
    for (const pop_entry *pe = population[branch].pop; pe->mons; pe++)
        if (pe->mons == mcls)
        {
            // A rough and wrong conversion of new-style linear rarities to
            // old quadratic ones, with some compensation for distribution
            // shape (old code had rarities > 100, capped after subtracting
            // the square of distance).
            // The new data pretty accurately represents old state, but only
            // if depth is known, and mons_rarity() doesn't receive it.
            static int fudge[3] = { 25, 12, 0 };
            int rare = pe->rarity;
            if (rare < 500)
                rare = isqrt_ceil(rare * 10);
            else
                rare = 100 - isqrt_ceil(10000 - rare * 10);
            return rare + fudge[pe->distrib];
        }

    return 0;
}

// only Pan currently
monster_type pick_monster_no_rarity(branch_type branch)
{
    if (!population[branch].count)
        return MONS_0;

    return population[branch].pop[random2(population[branch].count)].mons;
}

monster_type pick_monster_by_hash(branch_type branch, uint32_t hash)
{
    if (!population[branch].count)
        return MONS_0;

    return population[branch].pop[hash % population[branch].count].mons;
}

bool branch_has_monsters(branch_type branch)
{
    COMPILE_CHECK(ARRAYSZ(population) == NUM_BRANCHES);

    ASSERT(branch < NUM_BRANCHES);
    return population[branch].count;
}

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_TESTS)
void debug_monpick()
{
    string fails;

    for (int i = 0; i < NUM_BRANCHES; ++i)
    {
        branch_type br = (branch_type)i;

        for (monster_type m = MONS_0; m < NUM_MONSTERS; ++m)
        {
            int lev = mons_depth(m, br);
            int rare = mons_rarity(m, br);

            if (lev < DEPTH_NOWHERE && !rare)
            {
                fails += make_stringf("%s: no rarity for %s\n",
                                      branches[i].abbrevname,
                                      mons_class_name(m));
            }
            if (rare && lev >= DEPTH_NOWHERE)
            {
                fails += make_stringf("%s: no depth for %s\n",
                                      branches[i].abbrevname,
                                      mons_class_name(m));
            }
        }
    }

    if (!fails.empty())
    {
        FILE *f = fopen("mon-pick.out", "w");
        if (!f)
            sysfail("can't write test output");
        fprintf(f, "%s", fails.c_str());
        fclose(f);
        fail("mon-pick mismatches (dumped to mon-pick.out)");
    }
}
#endif
