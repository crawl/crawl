#include "AppHdr.h"

#include "godpassive.h"

#include "branch.h"
#include "coord.h"
#include "defines.h"
#include "env.h"
#include "files.h"
#include "food.h"
#include "fprop.h"
#include "godprayer.h"
#include "items.h"
#include "mon-stuff.h"
#include "player.h"
#include "player-stats.h"
#include "religion.h"
#include "state.h"
#include "stuff.h"

int che_boost_level()
{
    if (you.religion != GOD_CHEIBRIADOS)
        return (0);

    return (std::min(player_ponderousness(), piety_rank() - 1));
}

int che_boost(che_boost_type bt, int level)
{
    if (level == 0)
        return (0);

    switch (bt)
    {
    case CB_RNEG:
        return (level > 0 ? 1 : 0);
    case CB_RCOLD:
        return (level > 1 ? 1 : 0);
    case CB_RFIRE:
        return (level > 2 ? 1 : 0);
    case CB_STATS:
        return (level * (level + 1)) / 2;
    default:
        return (0);
    }
}

void che_handle_change(che_change_type ct, int diff)
{
    if (you.religion != GOD_CHEIBRIADOS)
        return;

    const std::string typestr = (ct == CB_PIETY ? "piety" : "ponderous");

    // Values after the change.
    const int ponder = player_ponderousness();
    const int prank = piety_rank() - 1;
    const int newlev = std::min(ponder, prank);

    // Reconstruct values before the change.
    int oldponder = ponder;
    int oldprank = prank;
    if (ct == CB_PIETY)
        oldprank -= diff;
    else // ct == CB_PONDEROUS
        oldponder -= diff;
    const int oldlev = std::min(oldponder, oldprank);
    dprf("Che %s %+d: %d/%d -> %d/%d", typestr.c_str(), diff,
         oldponder, oldprank,
         ponder, prank);

    for (int i = 0; i < NUM_BOOSTS; ++i)
    {
        const che_boost_type bt = static_cast<che_boost_type>(i);
        const int boostdiff = che_boost(bt, newlev) - che_boost(bt, oldlev);
        if (boostdiff == 0)
            continue;

        const std::string elemmsg = god_name(you.religion)
                                    + (boostdiff > 0 ? " " : " no longer ")
                                    + "protects you from %s.";
        switch (bt)
        {
        case CB_RNEG:
            mprf(MSGCH_GOD, elemmsg.c_str(), "negative energy");
            break;
        case CB_RCOLD:
            mprf(MSGCH_GOD, elemmsg.c_str(), "cold");
            break;
        case CB_RFIRE:
            mprf(MSGCH_GOD, elemmsg.c_str(), "fire");
            break;
        case CB_STATS:
        {
            mprf(MSGCH_GOD, "%s %s the support of your attributes.",
                 god_name(you.religion).c_str(),
                 boostdiff > 0 ? "raises" : "reduces");
            const std::string reason = "Cheibriados " + typestr + " change";
            notify_stat_change(reason.c_str());
            break;
        }
        case NUM_BOOSTS:
            break;
        }
    }
}

// Eat from one random off-level item stack.
void jiyva_eat_offlevel_items()
{
    // For wizard mode 'J' command
    if (you.religion != GOD_JIYVA)
        return;

    if (crawl_state.game_is_sprint())
        return;

    while (true)
    {
        if (one_chance_in(200))
            break;

        const int branch = random2(NUM_BRANCHES);

        // Choose level based on main dungeon depth so that levels short branches
        // aren't picked more often.
        ASSERT(branches[branch].depth <= branches[BRANCH_MAIN_DUNGEON].depth);
        const int level  = random2(branches[BRANCH_MAIN_DUNGEON].depth) + 1;

        const level_id lid(static_cast<branch_type>(branch), level);

        if (lid == level_id::current() || !is_existing_level(lid))
            continue;

        dprf("Checking %s", lid.describe().c_str());

        level_excursion le;
        le.go_to(lid);
        while (true)
        {
            if (one_chance_in(200))
                break;

            const coord_def p = random_in_bounds();

            if (igrd(p) == NON_ITEM || testbits(env.pgrid(p), FPROP_NO_JIYVA))
                continue;

            for (stack_iterator si(p); si; ++si)
            {
                if (!is_item_jelly_edible(*si) || one_chance_in(4))
                    continue;

                if (one_chance_in(4))
                    break;

                dprf("Eating %s on %s",
                     si->name(DESC_PLAIN).c_str(), lid.describe().c_str());

                // Needs a message now to explain possible hp or mp
                // gain from jiyva_slurp_bonus()
                mpr("You hear a distant slurping noise.");
                sacrifice_item_stack(*si);
                destroy_item(si.link());
            }
            return;
        }
    }
}

void jiyva_slurp_bonus(int item_value, int *js)
{
    if (you.penance[GOD_JIYVA])
        return;

    if (you.piety >= piety_breakpoint(1)
        && x_chance_in_y(you.piety, MAX_PIETY))
    {
        //same as a sultana
        lessen_hunger(70, true);
        *js |= JS_FOOD;
    }

    if (you.piety >= piety_breakpoint(3)
        && x_chance_in_y(you.piety, MAX_PIETY)
        && you.magic_points < you.max_magic_points)
    {
         inc_mp(std::max(random2(item_value), 1), false);
         *js |= JS_MP;
     }

    if (you.piety >= piety_breakpoint(4)
        && x_chance_in_y(you.piety, MAX_PIETY)
        && you.hp < you.hp_max)
    {
         inc_hp(std::max(random2(item_value), 1), false);
         *js |= JS_HP;
     }
}
