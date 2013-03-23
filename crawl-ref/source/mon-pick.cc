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
    UP,   // linearly from near-zero to 100%, increasing with depth
    DOWN, // linearly from 100% at the start to near-zero
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

int branch_ood_cap(branch_type branch)
{
    ASSERT(branch < NUM_BRANCHES);

    switch (branch)
    {
    case BRANCH_MAIN_DUNGEON:
        return 31;
    case BRANCH_VAULTS:
        return 12;
    case BRANCH_ELVEN_HALLS:
        return 7;
    case BRANCH_TOMB:
        return 5;
    default:
        return branches[branch].numlevels;
    }
}

// NOTE: The lower the level the earlier a monster may appear.
int mons_depth(monster_type mcls, branch_type branch)
{
    // legacy function, until ZotDef is ported
    for (const pop_entry *pe = population[branch].pop; pe->mons; pe++)
        if (pe->mons == mcls)
        {
            if (pe->distrib == UP)
                return pe->maxr;
            else if (pe->distrib == DOWN)
                return pe->minr;
            return (pe->minr + pe->maxr) / 2;
        }

    return DEPTH_NOWHERE;
}

// NOTE: Higher values returned means the monster is "more common".
// A return value of zero means the monster will never appear. {dlb}
// To be axed once ZotDef is ported.
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
            static int fudge[5] = { 25, 12, 0, 0, 0 };
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

static int _rarity_at(const pop_entry *pop, int depth)
{
    int rar = pop->rarity;
    int len = pop->maxr - pop->minr;
    switch (pop->distrib)
    {
    case FLAT: // 100% everywhere
        return rar;

    case SEMI: // 100% in the middle, 50% at the edges
        ASSERT(len > 0);
        len *= 2;
        return rar * (len - abs(pop->minr + pop->maxr - 2 * depth))
                   / len;

    case PEAK: // 100% in the middle, small at the edges, 0% outside
        len += 2; // we want it to zero outside the range, not at the edge
        return rar * (len - abs(pop->minr + pop->maxr - 2 * depth))
                   / len;

    case UP:
        return rar * (depth - pop->minr + 1) / (len + 1);

    case DOWN:
        return rar * (pop->maxr - depth + 1) / (len + 1);
    }

    die("bad distrib");
}

monster_type pick_monster(level_id place, mon_pick_vetoer veto)
{
    struct { monster_type mons; int rarity; } valid[NUM_MONSTERS];
    int nvalid = 0;
    int totalrar = 0;

    ASSERT(place.is_valid());
    for (const pop_entry *pop = population[place.branch].pop; pop->mons; pop++)
    {
        if (place.depth < pop->minr || place.depth > pop->maxr)
            continue;

        if (veto && (*veto)(pop->mons))
            continue;

        int rar = _rarity_at(pop, place.depth);
        ASSERT(rar > 0);

        valid[nvalid].mons = pop->mons;
        valid[nvalid].rarity = rar;
        totalrar += rar;
        nvalid++;
    }

    if (!nvalid)
        return MONS_0;

    totalrar = random2(totalrar); // the roll!

    for (int i = 0; i < nvalid; i++)
        if ((totalrar -= valid[i].rarity) < 0)
            return valid[i].mons;

    die("mon-pick roll out of range");
}

// Used for picking zombies when there's nothing native.
// TODO: cache potential zombifiables for the given level/size, with a
// second pass to select ones that have a skeleton/etc and can be placed in
// a given spot.
monster_type pick_monster_all_branches(int absdepth0, mon_pick_vetoer veto)
{
    monster_type valid[NUM_MONSTERS];
    int rarities[NUM_MONSTERS];
    memset(rarities, 0, sizeof(rarities));
    int nvalid = 0;

    for (int br = 0; br < NUM_BRANCHES; br++)
    {
        int depth = absdepth0 - absdungeon_depth((branch_type)br, 0);
        if (depth < 1 || depth > branch_ood_cap((branch_type)br))
            continue;

        for (const pop_entry *pop = population[br].pop; pop->mons; pop++)
        {
            if (depth < pop->minr || depth > pop->maxr)
                continue;

            if (veto && (*veto)(pop->mons))
                continue;

            int rar = _rarity_at(pop, depth);
            ASSERT(rar > 0);

            monster_type mons = pop->mons;
            if (!rarities[mons])
                valid[nvalid++] = pop->mons;
            if (rarities[mons] < rar)
                rarities[mons] = rar;
        }
    }

    if (!nvalid)
        return MONS_0;

    int totalrar = 0;
    for (int i = 0; i < nvalid; i++)
        totalrar += rarities[valid[i]];

    totalrar = random2(totalrar); // the roll!

    for (int i = 0; i < nvalid; i++)
        if ((totalrar -= rarities[valid[i]]) < 0)
            return valid[i];

    die("mon-pick roll out of range");
}

bool branch_has_monsters(branch_type branch)
{
    COMPILE_CHECK(ARRAYSZ(population) == NUM_BRANCHES);

    ASSERT(branch < NUM_BRANCHES);
    return population[branch].count;
}

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_TESTS)
static bool _not_skeletonable(monster_type mt)
{
    // Zombifiability in general.
    if (mons_species(mt) != mt)
        return true;
    if (!mons_zombie_size(mt) || mons_is_unique(mt))
        return true;
    if (mons_class_holiness(mt) != MH_NATURAL)
        return true;
    return !mons_skeleton(mt);
}

void debug_monpick()
{
    string fails;

    // Tests for the legacy interface; shouldn't ever happen.
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
    // Legacy tests end.

    for (int i = 0; i < NUM_BRANCHES; ++i)
    {
        branch_type br = (branch_type)i;

        for (int d = 1; d <= branch_ood_cap(br); d++)
        {
            if (!pick_monster_all_branches(absdungeon_depth(br, d),
                                           _not_skeletonable)
                && br != BRANCH_ZIGGURAT) // order is ok, check the loop
            {
                fails += make_stringf(
                    "%s: no valid skeletons in any parallel branch\n",
                    level_id(br, d).describe().c_str());
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
