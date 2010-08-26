/*
 *  File:       dgn-actions.cc
 *  Summary:    Delayed level actions.
 *  Written by: Adam Borowski
 */

#include "AppHdr.h"

#include "dgn-actions.h"

#include "coordit.h"
#include "debug.h"
#include "decks.h"
#include "env.h"
#include "mon-behv.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "player.h"
#include "religion.h"
#include "travel.h"
#include "view.h"

static const char *daction_names[] =
{
    "holy beings go hostile",
    "unholy/evil go hostile",
    "unclean/chaotic go hostile",
    "spellcasters go hostile",
    "yred slaves go hostile",
    "beogh orcs and their summons go hostile",
    "fellow slimes go hostile",
    "plants go hostile (allowing reconversion)",
    0, 0, 0, 0, 0, 0, 0, 0,

    // Actions not needing a counter.
    "old enslaved souls go poof",
    "holy beings allow another conversion attempt",
    "holy beings go neutral",
    "Trog's gifts go hostile",
    "shuffle decks",
    "reapply passive mapping",
    "remove Jiyva altars",
};

static bool _mons_matches_counter(const monsters *mon, daction_type act)
{
    if (!mon || !mon->alive())
        return (false);

    switch(act)
    {
    case DACT_ALLY_HOLY:
        return (mon->wont_attack() && is_good_god(mon->god));
    case DACT_ALLY_UNHOLY_EVIL:
        return (mon->wont_attack() && (mon->is_unholy() || mon->is_evil()));
    case DACT_ALLY_UNCLEAN_CHAOTIC:
        return (mon->wont_attack() && (mon->is_unclean() || mon->is_chaotic()));
    case DACT_ALLY_SPELLCASTER:
        return (mon->wont_attack() && mon->is_actual_spellcaster());
    case DACT_ALLY_YRED_SLAVE:
        // Changed: we don't force enslavement of those merely marked.
        return (is_yred_undead_slave(mon));
    case DACT_ALLY_BEOGH: // both orcies and demons summoned by sorcerers
        return (mon->wont_attack() && mons_is_god_gift(mon, GOD_BEOGH));
    case DACT_ALLY_SLIME:
        return (is_fellow_slime(mon));
    case DACT_ALLY_PLANT:
        // No check for friendliness since we pretend all plants became friendly
        // the moment you converted to Fedhas.
        return (mons_is_plant(mon));

    // Not a stored counter:
    case DACT_ALLY_TROG:
        return (mon->friendly() && mons_is_god_gift(mon, GOD_TROG));

    default:
        return (false);
    }
}

void update_da_counters(LevelInfo *lev)
{
    for (int act = 0; act < NUM_DA_COUNTERS; act++)
        lev->da_counters[act] = 0;

    for (monster_iterator mi; mi; ++mi)
        for (int act = 0; act < NUM_DA_COUNTERS; act++)
            if (_mons_matches_counter(*mi, static_cast<daction_type>(act)))
                lev->da_counters[act]++;
}

void add_daction(daction_type act)
{
    COMPILE_CHECK(ARRAYSZ(daction_names) == NUM_DACTIONS, dactions_names_size);

    dprf("scheduling delayed action: %s", daction_names[act]);
    you.dactions.push_back(act);

    // If we're removing a counted monster type, zero the counter even though
    // it hasn't been actually removed from the levels yet.
    if (act < NUM_DA_COUNTERS)
        travel_cache.clear_da_counter(act);

    // Immediately apply it to the current level.
    catchup_dactions();
}

static void _apply_daction(daction_type act)
{
    ASSERT(act >= 0 && act < NUM_DACTIONS);
    dprf("applying delayed action: %s", daction_names[act]);

    switch(act)
    {
    case DACT_ALLY_HOLY:
    case DACT_ALLY_UNHOLY_EVIL:
    case DACT_ALLY_UNCLEAN_CHAOTIC:
    case DACT_ALLY_SPELLCASTER:
    case DACT_ALLY_YRED_SLAVE:
    case DACT_ALLY_BEOGH:
    case DACT_ALLY_SLIME:
    case DACT_ALLY_PLANT:
    case DACT_ALLY_TROG:
        for (monster_iterator mi; mi; ++mi)
            if (_mons_matches_counter(*mi, act))
            {
#ifdef DEBUG_DIAGNOSTICS
                mprf(MSGCH_DIAGNOSTICS,
                     "going hostile: %s", mi->name(DESC_PLAIN, true).c_str());
#endif
                mi->attitude = ATT_HOSTILE;
                mi->del_ench(ENCH_CHARM, true);
                behaviour_event(*mi, ME_ALERT, MHITYOU);
                // For now CREATED_FRIENDLY/WAS_NEUTRAL stays.
                mons_att_changed(*mi);

                // If you reconvert to Fedhas, plants will love you again.
                if (act == DACT_ALLY_PLANT)
                    mi->flags &= ~MF_ATT_CHANGE_ATTEMPT;

                // No global message for Trog.
                if (act == DACT_ALLY_TROG)
                    simple_monster_message(*mi, " turns against you!");
            }
        break;

    case DACT_OLD_ENSLAVED_SOULS_POOF:
        for (monster_iterator mi; mi; ++mi)
            if (mons_enslaved_soul(*mi))
            {
                simple_monster_message(*mi, " is freed.");
                // The monster disappears.
                monster_die(*mi, KILL_DISMISSED, NON_MONSTER);
            }
        break;
    case DACT_HOLY_NEW_ATTEMPT:
        for (monster_iterator mi; mi; ++mi)
            if (mi->is_holy())
                mi->flags &= ~MF_ATT_CHANGE_ATTEMPT;
        break;
    case DACT_HOLY_PETS_GO_NEUTRAL:
        for (monster_iterator mi; mi; ++mi)
            if (mi->friendly()
                && !mi->has_ench(ENCH_CHARM)
                && mi->is_holy()
                && mons_is_god_gift(*mi, GOD_SHINING_ONE))
            {
                // monster changes attitude
                mi->attitude = ATT_GOOD_NEUTRAL;
                mons_att_changed(*mi);

                simple_monster_message(*mi, " becomes indifferent.");
            }
        break;

    case DACT_SHUFFLE_DECKS:
        shuffle_all_decks_on_level();
        break;
    case DACT_REAUTOMAP:
        reautomap_level();
        break;
    case DACT_REMOVE_JIYVA_ALTARS:
        for (rectangle_iterator ri(1); ri; ++ri)
        {
            if (grd(*ri) == DNGN_ALTAR_JIYVA)
                grd(*ri) = DNGN_FLOOR;
        }
        break;
    case NUM_DA_COUNTERS:
    case NUM_DACTIONS:
        ;
    }
}

void catchup_dactions()
{
    while (env.dactions_done < you.dactions.size())
        _apply_daction(you.dactions[env.dactions_done++]);
}

unsigned int query_da_counter(daction_type c)
{
    return travel_cache.query_da_counter(c);
}
