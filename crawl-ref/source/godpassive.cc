#include "AppHdr.h"

#include "godpassive.h"

#include "player.h"
#include "religion.h"

int che_boost_level()
{
    if (you.religion != GOD_CHEIBRIADOS)
        return 0;
    return std::min(
        player_equip_ego_type(EQ_ALL_ARMOUR, SPARM_PONDEROUSNESS),
        piety_rank() - 1);
}

int che_boost(che_boost_type bt, int level)
{
    if (level == 0)
        return 0;

    switch (bt)
    {
    case CB_RNEG:
        return (level > 0 ? 1 : 0);
    case CB_RCOLD:
        return (level > 1 ? 1 : 0);
    case CB_RFIRE:
        return (level > 2 ? 1 : 0);
    case CB_STATS:
        return level;
    default:
        return 0;
    }
}

void che_handle_change(che_change_type ct, int diff)
{
    if (you.religion != GOD_CHEIBRIADOS)
        return;

    const std::string typestr = (ct == CB_PIETY ? "piety" : "ponderous");

    // Values after the change.
    int ponder = player_equip_ego_type(EQ_ALL_ARMOUR, SPARM_PONDEROUSNESS);
    int prank = piety_rank() - 1;
    int newlev = std::min(ponder, prank);

    // Reconstruct values before the change.
    int oldponder = ponder;
    int oldprank = prank;
    if (ct == CB_PIETY)
        oldprank -= diff;
    else // ct == CB_PONDEROUS
        oldponder -= diff;
    int oldlev = std::min(oldponder, oldprank);

    for (int i = 0; i < NUM_BOOSTS; ++i)
    {
        che_boost_type bt = static_cast<che_boost_type>(i);
        int boostdiff = che_boost(bt, newlev) - che_boost(bt, oldlev);
        if (boostdiff == 0)
            continue;

        std::string elemmsg = god_name(you.religion)
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
            modify_stat(STAT_STRENGTH, boostdiff, true, reason);
            modify_stat(STAT_INTELLIGENCE, boostdiff, true, reason);
            modify_stat(STAT_DEXTERITY, boostdiff, true, reason);
            break;
        }
        case NUM_BOOSTS:
            break;
        }
    }
}
