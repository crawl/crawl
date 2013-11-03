/**
 * @file
 * @brief Functions used to help determine which monsters should appear.
**/

#include "AppHdr.h"

#include "mon-pick.h"

#include "externs.h"
#include "branch.h"
#include "coord.h"
#include "env.h"
#include "errors.h"
#include "libutil.h"
#include "mon-place.h"
#include "mon-util.h"
#include "place.h"

#include "mon-pick-data.h"

int branch_ood_cap(branch_type branch)
{
    ASSERT(branch < NUM_BRANCHES);

    switch (branch)
    {
    case BRANCH_DUNGEON:
        return 20;
    case BRANCH_DEPTHS:
        return 14;
    case BRANCH_VAULTS:
        return 12;
    case BRANCH_ELF:
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
    for (const pop_entry *pe = population[branch].pop; pe->value; pe++)
        if (pe->value == mcls)
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
    for (const pop_entry *pe = population[branch].pop; pe->value; pe++)
        if (pe->value == mcls)
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

    return population[branch].pop[random2(population[branch].count)].value;
}

monster_type pick_monster_by_hash(branch_type branch, uint32_t hash)
{
    if (!population[branch].count)
        return MONS_0;

    return population[branch].pop[hash % population[branch].count].value;
}

monster_type pick_monster(level_id place, mon_pick_vetoer veto)
{
#ifdef ASSERTS
    if (!place.is_valid())
        die("trying to pick a monster from %s", place.describe().c_str());
#endif
    return pick_monster_from(population[place.branch].pop, place.depth, veto);
}

monster_type pick_monster(level_id place, monster_picker &picker, mon_pick_vetoer veto)
{
    ASSERT(place.is_valid());
    return picker.pick_with_veto(population[place.branch].pop, place.depth, MONS_0, veto);
}

monster_type pick_monster_from(const pop_entry *fpop, int depth,
                               mon_pick_vetoer veto)
{
    // XXX: If creating/destroying instances has performance issues, cache a
    // static instance
    monster_picker picker = monster_picker();
    return picker.pick_with_veto(fpop, depth, MONS_0, veto);
}

monster_type monster_picker::pick_with_veto(const pop_entry *weights,
                                            int level, monster_type none,
                                            mon_pick_vetoer vetoer)
{
    _veto = vetoer;
    return pick(weights, level, none);
}

// Veto specialisation for the monster_picker class; this simply calls the
// stored veto function. Can subclass further for more complex veto behaviour.
bool monster_picker::veto(monster_type mon)
{
    return _veto && _veto(mon);
}

bool positioned_monster_picker::veto(monster_type mon)
{
    // Actually pick a monster that is happy where we want to put it.
    // Fish zombies on land are helpless and uncool.
    if (!in_bounds(pos) || !monster_habitable_grid(mon, grd(pos)))
        return true;
    // Optional positional veto
    if (posveto && posveto(mon, pos)) return true;
    return monster_picker::veto(mon);
}

monster_type pick_monster_all_branches(int absdepth0, mon_pick_vetoer veto)
{
    monster_picker picker = monster_picker();
    return pick_monster_all_branches(absdepth0, picker, veto);
}

// Used for picking zombies when there's nothing native.
// TODO: cache potential zombifiables for the given level/size, with a
// second pass to select ones that have a skeleton/etc and can be placed in
// a given spot.
monster_type pick_monster_all_branches(int absdepth0, monster_picker &picker,
                                       mon_pick_vetoer veto)
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

        for (const pop_entry *pop = population[br].pop; pop->value; pop++)
        {
            if (depth < pop->minr || depth > pop->maxr)
                continue;

            if (veto && (*veto)(pop->value)
                || !veto && picker.veto(pop->value))
                continue;

            int rar = picker.rarity_at(pop, depth);
            ASSERT(rar > 0);

            monster_type mons = pop->value;
            if (!rarities[mons])
                valid[nvalid++] = pop->value;
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
