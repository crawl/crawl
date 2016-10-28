/**
 * @file
 * @brief Functions used to help determine which monsters should appear.
**/

#include "AppHdr.h"

#include "mon-pick.h"
#include "mon-pick-data.h"

#include "branch.h"
#include "coord.h"
#include "env.h"
#include "errors.h"
#include "libutil.h"
#include "mon-place.h"
#include "place.h"
#include "stringutil.h"

int branch_ood_cap(branch_type branch)
{
    ASSERT(branch < NUM_BRANCHES);

    switch (branch)
    {
    case BRANCH_DUNGEON:
        return 27;
    case BRANCH_DEPTHS:
        return 14;
    case BRANCH_LAIR:
    case BRANCH_VAULTS:
    case BRANCH_DIS:
    case BRANCH_GEHENNA:
    case BRANCH_COCYTUS:
    case BRANCH_TARTARUS:
        return 12;
    case BRANCH_ELF:
        return 7;
    case BRANCH_CRYPT:
    case BRANCH_TOMB:
        return 5;
    default:
        return branches[branch].numlevels;
    }
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
    return _veto && (invalid_monster_type(mon) || _veto(mon));
}

bool positioned_monster_picker::veto(monster_type mon)
{
    // Actually pick a monster that is happy where we want to put it.
    // Fish zombies on land are helpless and uncool.
    if (in_bounds(pos) && !monster_habitable_grid(mon, grd(pos)))
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

    for (branch_iterator it; it; ++it)
    {
        int depth = absdepth0 - absdungeon_depth(it->id, 0);
        if (depth < 1 || depth > branch_ood_cap(it->id))
            continue;

        for (const pop_entry *pop = population[it->id].pop; pop->value; pop++)
        {
            if (depth < pop->minr || depth > pop->maxr)
                continue;

            if (veto ? (*veto)(pop->value) : picker.veto(pop->value))
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

const pop_entry* fish_population(branch_type br, bool lava)
{
    COMPILE_CHECK(ARRAYSZ(population_water) == NUM_BRANCHES);
    COMPILE_CHECK(ARRAYSZ(population_lava) == NUM_BRANCHES);
    ASSERT_RANGE(br, 0, NUM_BRANCHES);
    if (lava)
        return population_lava[br].pop;
    else
        return population_water[br].pop;
}

const pop_entry* zombie_population(branch_type br)
{
    COMPILE_CHECK(ARRAYSZ(population_zombie) == NUM_BRANCHES);
    ASSERT_RANGE(br, 0, NUM_BRANCHES);
    return population_zombie[br].pop;
}

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_TESTS)
static bool _not_skeletonable(monster_type mt)
{
    // Zombifiability in general.
    if (mons_species(mt) != mt)
        return true;
    if (!mons_zombie_size(mt) || mons_is_unique(mt))
        return true;
    if (!(mons_class_holiness(mt) & MH_NATURAL))
        return true;
    return !mons_skeleton(mt);
}

void debug_monpick()
{
    string fails;

    for (branch_iterator it; it; ++it)
    {
        branch_type br = it->id;

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

    dump_test_fails(fails, "mon-pick");
}
#endif
