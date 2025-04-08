/**
 * @file
 * @brief Delayed level actions.
**/

#include "AppHdr.h"

#include "dactions.h"

#include "act-iter.h"
#include "attitude-change.h"
#include "coordit.h"
#include "decks.h"
#include "dungeon.h"
#include "god-companions.h" // hepliaklqana_ancestor
#include "god-passive.h"
#include "god-wrath.h"
#include "libutil.h"
#include "mapmark.h"
#include "map-knowledge.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-transit.h"
#include "religion.h"
#include "state.h"
#include "tag-version.h"
#include "view.h"

static void _daction_hog_to_human(monster *mon, bool in_transit);

#ifdef DEBUG_DIAGNOSTICS
static const char *daction_names[] =
{
    "holy beings go hostile",
    "unholy/evil go hostile",
    "unclean/chaotic go hostile",
    "spellcasters go hostile",
    "yred bound souls go hostile",
    "beogh orcs and their summons go hostile",
    "fellow slimes go hostile",
    "plants go hostile (allowing reconversion)",
    0, 0, 0, 0, 0, 0, 0, 0,

    // Actions not needing a counter.
    "old bound souls go poof",
#if TAG_MAJOR_VERSION == 34
    "holy beings allow another conversion attempt",
#else
    "slimes allow another conversion attempt",
#endif
#if TAG_MAJOR_VERSION == 34
    "holy beings go neutral",
    "Trog's gifts go hostile",
    "reclaim decks",
#endif
    "reapply passive mapping",
    "remove Jiyva altars and prayers",
    "Pikel's minions go poof",
    "corpses rot",
#if TAG_MAJOR_VERSION == 34
    "Tomb loses -cTele",
    "slimes allow another conversion attempt",
#endif
    "hogs to humans",
#if TAG_MAJOR_VERSION == 34
    "end spirit howl",
#endif
    "gold to top of piles",
    "bribe timeout",
    "remove Gozag shops",
    "apply Gozag bribes",
#if TAG_MAJOR_VERSION == 34
    "Makhleb's servants go hostile",
    "make all monsters hate you",
#endif
    "ancestor vanishes",
    "upgrade ancestor",
    "remove Ignis altars",
    "cleanup Beogh vengeance markers",
};
#endif

bool mons_matches_daction(const monster* mon, daction_type act)
{
    if (!mon || !mon->alive())
        return false;

    switch (act)
    {
    case DACT_ALLY_BEOGH: // both orcs and demons summoned by high priests
        return mon->wont_attack() && mons_is_god_gift(*mon, GOD_BEOGH);
    case DACT_ALLY_SLIME:
        return is_fellow_slime(*mon);
    case DACT_ALLY_PLANT:
        // No check for friendliness since we pretend all plants became friendly
        // the moment you converted to Fedhas.
        return mons_is_plant(*mon);
    case DACT_ALLY_HEPLIAKLQANA:
    case DACT_UPGRADE_ANCESTOR:
        return mon->wont_attack() && mons_is_god_gift(*mon, GOD_HEPLIAKLQANA);

    // Not a stored counter:
    case DACT_PIKEL_MINIONS:
        return mon->type == MONS_LEMURE
               && mon->props.exists(PIKEL_BAND_KEY);

    case DACT_OLD_CHARMD_SOULS_POOF:
        return mon->type == MONS_BOUND_SOUL;

    case DACT_SLIME_NEW_ATTEMPT:
        return mons_is_slime(*mon);

    case DACT_KIRKE_HOGS:
        return (mon->type == MONS_HOG
                || mon->type == MONS_HELL_HOG
                || mon->type == MONS_HOLY_SWINE)
               && !mon->is_shapeshifter()
               // Must be one of Kirke's original band
               // *or* another monster that got porkalated
               && (mon->props.exists(KIRKE_BAND_KEY)
                   || mon->props.exists(ORIG_MONSTER_KEY));

    case DACT_BRIBE_TIMEOUT:
        return mon->has_ench(ENCH_NEUTRAL_BRIBED)
               || mon->props.exists(NEUTRAL_BRIBE_KEY)
               || mon->props.exists(FRIENDLY_BRIBE_KEY);

    case DACT_SET_BRIBES:
        return !testbits(mon->flags, MF_WAS_IN_VIEW);

    case DACT_JIYVA_DEAD:
        return mon->type == MONS_DISSOLUTION;

    case DACT_BEOGH_VENGEANCE_CLEANUP:
        return mon->has_ench(ENCH_VENGEANCE_TARGET)
               && mon->get_ench(ENCH_VENGEANCE_TARGET).degree
                  <= you.props[BEOGH_VENGEANCE_NUM_KEY].get_int();

    default:
        return false;
    }
}

void update_daction_counters(LevelInfo *lev)
{
    for (int act = 0; act < NUM_DACTION_COUNTERS; act++)
        lev->daction_counters[act] = 0;

    for (monster_iterator mi; mi; ++mi)
        for (int act = 0; act < NUM_DACTION_COUNTERS; act++)
            if (mons_matches_daction(*mi, static_cast<daction_type>(act)))
                lev->daction_counters[act]++;
}

void add_daction(daction_type act)
{
#ifdef DEBUG_DIAGNOSTICS
    COMPILE_CHECK(ARRAYSZ(daction_names) == NUM_DACTIONS);
#endif

    dprf("scheduling delayed action: %s", daction_names[act]);
    you.dactions.push_back(act);

    // If we're removing a counted monster type, zero the counter even though
    // it hasn't been actually removed from the levels yet.
    if (act < NUM_DACTION_COUNTERS)
        travel_cache.clear_daction_counter(act);

    // Immediately apply it to the current level.
    catchup_dactions();

    // And now to any monsters in transit.
    apply_daction_to_transit(act);
}

void apply_daction_to_mons(monster* mon, daction_type act, bool local,
        bool in_transit)
{
    // Transiting monsters exist outside the normal monster list (env.mons or
    // env.mons for short). Be careful not to write them into the monster grid, by,
    // for example, calling monster::move_to_pos on them.
    // See _daction_hog_to_human for an example.
    switch (act)
    {
        case DACT_ALLY_BEOGH:
        case DACT_ALLY_SLIME:
        case DACT_ALLY_PLANT:
            dprf("going hostile: %s", mon->name(DESC_PLAIN, true).c_str());
            mon->attitude = ATT_HOSTILE;
            mon->del_ench(ENCH_CHARM, true);
            if (local)
                behaviour_event(mon, ME_ALERT, &you);
            // For now CREATED_FRIENDLY/WAS_NEUTRAL stays.
            mons_att_changed(mon);

            // If you reconvert to Fedhas/Jiyva, plants/slimes will
            // love you again.
            if (act == DACT_ALLY_PLANT || act == DACT_ALLY_SLIME)
                mon->flags &= ~MF_ATT_CHANGE_ATTEMPT;
            break;

        case DACT_ALLY_HEPLIAKLQANA:
            // Skip this if we have since regained enough piety to get our
            // ancestor back.
            if (you_worship(GOD_HEPLIAKLQANA) && piety_rank() >= 1)
                break;

            simple_monster_message(*mon, " returns to the mists of memory.");
            monster_die(*mon, KILL_RESET, NON_MONSTER);
            break;

        case DACT_UPGRADE_ANCESTOR:
            if (!in_transit)
                upgrade_hepliaklqana_ancestor(true);
            break;

        case DACT_OLD_CHARMD_SOULS_POOF:
            // Skip if this is our CURRENT bound soul (ie: in our companion list)
            if (companion_list.count(mon->mid))
                break;

            simple_monster_message(*mon, " is freed.");
            // The monster disappears.
            monster_die(*mon, KILL_RESET_KEEP_ITEMS, NON_MONSTER);
            break;

        case DACT_SLIME_NEW_ATTEMPT:
            mon->flags &= ~MF_ATT_CHANGE_ATTEMPT;
            break;

        case DACT_PIKEL_MINIONS:
        {
            simple_monster_message(*mon, " departs this earthly plane.");
            if (!in_transit)
            {
                check_place_cloud(CLOUD_BLACK_SMOKE, mon->pos(),
                                                random_range(3, 5), nullptr);
            }
            // The monster disappears.
            monster_die(*mon, KILL_RESET, NON_MONSTER);
            break;
        }
        case DACT_KIRKE_HOGS:
            _daction_hog_to_human(mon, in_transit);
            break;

        case DACT_BRIBE_TIMEOUT:
            if (mon->del_ench(ENCH_NEUTRAL_BRIBED))
            {
                mon->attitude = ATT_NEUTRAL;
                mon->flags   |= MF_WAS_NEUTRAL;
                mons_att_changed(mon);
            }
            if (mon->props.exists(NEUTRAL_BRIBE_KEY))
                mon->props.erase(NEUTRAL_BRIBE_KEY);
            if (mon->props.exists(FRIENDLY_BRIBE_KEY))
                mon->props.erase(FRIENDLY_BRIBE_KEY);
            break;

        case DACT_SET_BRIBES:
            gozag_set_bribe(mon);
            break;

        case DACT_JIYVA_DEAD:
            mon->spells.clear();
            mon->spells.push_back( { SPELL_CANTRIP, 62, MON_SPELL_PRIEST } );
            mon->props[CUSTOM_SPELLS_KEY] = true;
            break;

        case DACT_BEOGH_VENGEANCE_CLEANUP:
            mon->del_ench(ENCH_VENGEANCE_TARGET);
            mon->patrol_point.reset();
            break;

        // The other dactions do not affect monsters directly.
        default:
            break;
    }
}

// Print a farewell message from any of Pikel's minions who are visible.
static void _pikel_band_message()
{
    int visible_minions = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->type == MONS_LEMURE
            && mi->props.exists(PIKEL_BAND_KEY)
            && mi->observable())
        {
            visible_minions++;
        }
    }
    if (visible_minions > 0 && you.num_turns > 0)
    {
        if (you.get_mutation_level(MUT_NO_LOVE))
        {
            const char *substr = visible_minions > 1 ? "minions" : "minion";
            mprf("Pikel's spell is broken, but his former %s can only feel hate"
                 " for you!", substr);
        }
        else
        {
            const char *substr = visible_minions > 1
                ? "minions thank you for their"
                : "minion thanks you for its";
            mprf("With Pikel's spell broken, his former %s freedom.", substr);
        }
    }
}

static void _apply_daction(daction_type act)
{
    ASSERT_RANGE(act, 0, NUM_DACTIONS);
    dprf("applying delayed action: %s", daction_names[act]);

    if (DACT_PIKEL_MINIONS == act)
        _pikel_band_message();
    switch (act)
    {
    case DACT_JIYVA_DEAD:
        for (rectangle_iterator ri(1); ri; ++ri)
        {
            if (env.grid(*ri) == DNGN_ALTAR_JIYVA)
                env.grid(*ri) = DNGN_FLOOR;
        }
    // intentional fallthrough to handle Dissolution
    case DACT_ALLY_BEOGH:
    case DACT_ALLY_HEPLIAKLQANA:
    case DACT_ALLY_SLIME:
    case DACT_ALLY_PLANT:
    case DACT_OLD_CHARMD_SOULS_POOF:
    case DACT_SLIME_NEW_ATTEMPT:
    case DACT_PIKEL_MINIONS:
    case DACT_KIRKE_HOGS:
    case DACT_BRIBE_TIMEOUT:
    case DACT_SET_BRIBES:
    case DACT_BEOGH_VENGEANCE_CLEANUP:
        for (monster_iterator mi; mi; ++mi)
        {
            if (mons_matches_daction(*mi, act))
                apply_daction_to_mons(*mi, act, true, false);
        }
        break;

#if TAG_MAJOR_VERSION == 34
    case DACT_RECLAIM_DECKS:
        reclaim_decks_on_level();
        break;
#endif
    case DACT_REAUTOMAP:
        reautomap_level();
        break;
    case DACT_REMOVE_IGNIS_ALTARS:
        for (rectangle_iterator ri(1); ri; ++ri)
            if (env.grid(*ri) == DNGN_ALTAR_IGNIS)
                env.grid(*ri) = DNGN_FLOOR;
        break;
    case DACT_ROT_CORPSES:
        for (auto &item : env.item)
            if (item.is_type(OBJ_CORPSES, CORPSE_BODY))
                item.freshness = 1; // thoroughly rotten
        break;
    case DACT_GOLD_ON_TOP:
        gozag_move_level_gold_to_top();
        break;
    case DACT_REMOVE_GOZAG_SHOPS:
    {
        gozag_abandon_shops_on_level();
        break;
    }
    case DACT_UPGRADE_ANCESTOR:
        if (!companion_is_elsewhere(hepliaklqana_ancestor()))
            upgrade_hepliaklqana_ancestor(true);
        break;
#if TAG_MAJOR_VERSION == 34
    case DACT_END_SPIRIT_HOWL:
    case DACT_HOLY_NEW_ATTEMPT:
    case DACT_ALLY_SACRIFICE_LOVE:
    case DACT_TOMB_CTELE:
    case DACT_ALLY_HOLY:
    case DACT_HOLY_PETS_GO_NEUTRAL:
    case DACT_ALLY_MAKHLEB:
    case DACT_ALLY_TROG:
    case DACT_ALLY_UNHOLY_EVIL:
    case DACT_ALLY_UNCLEAN_CHAOTIC:
    case DACT_ALLY_SPELLCASTER:
    case DACT_ALLY_YRED_RELEASE_SOULS:
#endif
    case NUM_DACTION_COUNTERS:
    case NUM_DACTIONS:
        ;
    }
}

void catchup_dactions()
{
    while (env.dactions_done < you.dactions.size())
        _apply_daction(you.dactions[env.dactions_done++]);
}

unsigned int query_daction_counter(daction_type c)
{
    return travel_cache.query_daction_counter(c) + count_daction_in_transit(c);
}

static void _daction_hog_to_human(monster *mon, bool in_transit)
{
    ASSERT(mon); // XXX: change to monster &mon
    // Hogs to humans
    monster orig;
    const bool could_see = you.can_see(*mon);

    // Was it a converted monster or original band member?
    if (mon->props.exists(ORIG_MONSTER_KEY))
    {
        // It was transformed into a pig. Copy it, since the instance in props
        // will get deleted as soon as **mi is assigned to.
        orig = mon->props[ORIG_MONSTER_KEY].get_monster();
        orig.mid = mon->mid;
    }
    else
    {
        // It started life as a pig in Kirke's band.
        orig.type     = MONS_HUMAN;
        orig.attitude = mon->attitude;
        orig.mid = mon->mid;
        define_monster(orig);
    }
    // Keep at same spot. This position is irrelevant if the hog is in transit.
    // See below.
    const coord_def pos = mon->pos();
    // Preserve relative HP.
    const float hp
        = (float) mon->hit_points / (float) mon->max_hit_points;
    // Preserve some flags.
    const monster_flags_t preserve_flags =
        mon->flags & ~(MF_JUST_SUMMONED | MF_WAS_IN_VIEW);
    // Preserve enchantments.
    mon_enchant_list enchantments = mon->enchantments;
    FixedBitVector<NUM_ENCHANTMENTS> ench_cache = mon->ench_cache;

    // Restore original monster.
    *mon = orig;

    // If the hog is in transit, then it is NOT stored in the normal
    // monster list (env.mons or env.mons for short). We cannot call move_to_pos
    // on such a hog, because move_to_pos will attempt to update the
    // monster grid (env.mgrid or env.mgrid for short). Since the hog is not
    // stored in the monster list, this will corrupt the grid. The transit code
    // will update the grid properly once the transiting hog has been placed.
    if (!in_transit)
        mon->move_to_pos(pos);
    // "else {mon->position = pos}" is unnecessary because the transit code will
    // ignore the old position anyway.
    mon->enchantments = enchantments;
    mon->ench_cache = ench_cache;
    mon->hit_points   = max(1, (int) (mon->max_hit_points * hp));
    mon->flags        = mon->flags | preserve_flags;

    const bool can_see = you.can_see(*mon);

    // A monster changing factions while in the arena messes up
    // arena book-keeping.
    if (!crawl_state.game_is_arena())
    {
        // * A monster's attitude shouldn't downgrade from friendly
        //   or good-neutral because you helped it. It'd suck to
        //   lose a permanent ally that way.
        if (mon->attitude == ATT_HOSTILE)
        {
            mon->attitude = ATT_GOOD_NEUTRAL;
            mon->flags   |= MF_WAS_NEUTRAL;
            mons_att_changed(mon);
        }
    }

    behaviour_event(mon, ME_EVAL);

    if (could_see && !can_see)
        mpr("The hog vanishes!");
    else if (!could_see && can_see)
        mprf("%s appears from out of thin air!", mon->name(DESC_A).c_str());
}
