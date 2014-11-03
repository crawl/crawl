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
#include "items.h"
#include "libutil.h"
#include "mapmark.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-transit.h"
#include "religion.h"
#include "state.h"
#include "view.h"

static void _daction_hog_to_human(monster *mon, bool in_transit);

#ifdef DEBUG_DIAGNOSTICS
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
#if TAG_MAJOR_VERSION == 34
    "holy beings allow another conversion attempt",
#endif
#if TAG_MAJOR_VERSION > 34
    "slimes allow another conversion attempt",
#endif
    "holy beings go neutral",
    "Trog's gifts go hostile",
    "shuffle decks",
    "reapply passive mapping",
    "remove Jiyva altars",
    "Pikel's slaves go good-neutral",
    "corpses rot",
    "Tomb loses -cTele",
#if TAG_MAJOR_VERSION == 34
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
    "Makhleb's servants go hostile",
    "make all monsters hate you",
};
#endif

bool mons_matches_daction(const monster* mon, daction_type act)
{
    if (!mon || !mon->alive())
        return false;

    switch (act)
    {
    case DACT_ALLY_HOLY:
        return mon->wont_attack() && is_good_god(mon->god);
    case DACT_ALLY_UNHOLY_EVIL:
        return mon->wont_attack() && (mon->is_unholy() || mon->is_evil());
    case DACT_ALLY_UNCLEAN_CHAOTIC:
        return mon->wont_attack() && (mon->how_unclean() || mon->how_chaotic());
    case DACT_ALLY_SPELLCASTER:
        return mon->wont_attack() && mon->is_actual_spellcaster();
    case DACT_ALLY_YRED_SLAVE:
        // Changed: we don't force enslavement of those merely marked.
        return is_yred_undead_slave(mon);
    case DACT_ALLY_BEOGH: // both orcs and demons summoned by high priests
        return mon->wont_attack() && mons_is_god_gift(mon, GOD_BEOGH);
    case DACT_ALLY_SLIME:
        return is_fellow_slime(mon);
    case DACT_ALLY_PLANT:
        // No check for friendliness since we pretend all plants became friendly
        // the moment you converted to Fedhas.
        return mons_is_plant(mon);

    // Not a stored counter:
    case DACT_ALLY_TROG:
        return mon->friendly() && mons_is_god_gift(mon, GOD_TROG);
    case DACT_ALLY_MAKHLEB:
        return mon->friendly() && mons_is_god_gift(mon, GOD_MAKHLEB);
    case DACT_HOLY_PETS_GO_NEUTRAL:
        return mon->friendly()
               && !mon->has_ench(ENCH_CHARM)
               && mon->is_holy()
               && mons_is_god_gift(mon, GOD_SHINING_ONE);
    case DACT_PIKEL_SLAVES:
        return mon->type == MONS_SLAVE
               && testbits(mon->flags, MF_BAND_MEMBER)
               && mon->props.exists("pikel_band")
               && mon->mname != "freed slave";

    case DACT_OLD_ENSLAVED_SOULS_POOF:
        return mons_enslaved_soul(mon);

    case DACT_SLIME_NEW_ATTEMPT:
        return mons_is_slime(mon);

    case DACT_KIRKE_HOGS:
        return (mon->type == MONS_HOG
                || mon->type == MONS_HELL_HOG
                || mon->type == MONS_HOLY_SWINE)
               && !mon->is_shapeshifter()
               // Must be one of Kirke's original band
               // *or* another monster that got porkalated
               && (mon->props.exists("kirke_band")
                   || mon->props.exists(ORIG_MONSTER_KEY));

    case DACT_BRIBE_TIMEOUT:
        return mon->has_ench(ENCH_BRIBED)
               || mon->has_ench(ENCH_PERMA_BRIBED)
               || mon->props.exists(GOZAG_BRIBE_KEY)
               || mon->props.exists(GOZAG_PERMABRIBE_KEY);

    case DACT_SET_BRIBES:
        return !testbits(mon->flags, MF_WAS_IN_VIEW);

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
    // menv for short). Be careful not to write them into the monster grid, by,
    // for example, calling monster::move_to_pos on them.
    // See _daction_hog_to_human for an example.
    switch (act)
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
        case DACT_ALLY_MAKHLEB:
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

            // No global message for Trog, Makhleb, or Ru.
            if (local && (act == DACT_ALLY_TROG
                          || act == DACT_ALLY_MAKHLEB))
            {
                simple_monster_message(mon, " turns against you!");
            }
            break;

        case DACT_OLD_ENSLAVED_SOULS_POOF:
            simple_monster_message(mon, " is freed.");
            // The monster disappears.
            monster_die(mon, KILL_DISMISSED, NON_MONSTER);
            break;

        case DACT_SLIME_NEW_ATTEMPT:
            mon->flags &= ~MF_ATT_CHANGE_ATTEMPT;
            break;

        case DACT_HOLY_PETS_GO_NEUTRAL:
        case DACT_PIKEL_SLAVES:
            // monster changes attitude
            mon->attitude = ATT_GOOD_NEUTRAL;
            mons_att_changed(mon);

            if (act == DACT_PIKEL_SLAVES)
            {
                mon->flags |= MF_NAME_REPLACE | MF_NAME_DESCRIPTOR
                                  | MF_NAME_NOCORPSE;
                mon->mname = "freed slave";
            }
            else if (local)
                simple_monster_message(mon, " becomes indifferent.");
            mon->behaviour = BEH_WANDER;
            break;

        case DACT_KIRKE_HOGS:
            _daction_hog_to_human(mon, in_transit);
            break;

        case DACT_BRIBE_TIMEOUT:
            mon->del_ench(ENCH_BRIBED);
            mon->del_ench(ENCH_PERMA_BRIBED);
            if (mon->props.exists(GOZAG_BRIBE_KEY))
                mon->props.erase(GOZAG_BRIBE_KEY);
            if (mon->props.exists(GOZAG_PERMABRIBE_KEY))
                mon->props.erase(GOZAG_PERMABRIBE_KEY);
            break;

        case DACT_SET_BRIBES:
            gozag_set_bribe(mon);

        // The other dactions do not affect monsters directly.
        default:
            break;
    }
}

static void _apply_daction(daction_type act)
{
    ASSERT_RANGE(act, 0, NUM_DACTIONS);
    dprf("applying delayed action: %s", daction_names[act]);

    switch (act)
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
    case DACT_ALLY_MAKHLEB:
    case DACT_OLD_ENSLAVED_SOULS_POOF:
    case DACT_SLIME_NEW_ATTEMPT:
    case DACT_HOLY_PETS_GO_NEUTRAL:
    case DACT_PIKEL_SLAVES:
    case DACT_KIRKE_HOGS:
    case DACT_BRIBE_TIMEOUT:
    case DACT_SET_BRIBES:
        for (monster_iterator mi; mi; ++mi)
        {
            if (mons_matches_daction(*mi, act))
                apply_daction_to_mons(*mi, act, true, false);
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
    case DACT_ROT_CORPSES:
        for (int i = 0; i < MAX_ITEMS; i++)
            if (mitm[i].base_type == OBJ_CORPSES && mitm[i].sub_type == CORPSE_BODY)
                mitm[i].special = 1; // thoroughly rotten
        break;
    case DACT_TOMB_CTELE:
        if (player_in_branch(BRANCH_TOMB))
            unset_level_flags(LFLAG_NO_TELE_CONTROL, you.depth != 3);
        break;
    case DACT_GOLD_ON_TOP:
    {
        for (rectangle_iterator ri(0); ri; ++ri)
        {
            for (stack_iterator j(*ri); j; ++j)
            {
                if (j->base_type == OBJ_GOLD)
                {
                    bool detected = false;
                    int dummy = j->index();
                    j->special = 0;
                    unlink_item(dummy);
                    move_item_to_grid(&dummy, *ri, true);
                    if (!env.map_knowledge(*ri).item()
                        || env.map_knowledge(*ri).item()->base_type != OBJ_GOLD)
                    {
                        detected = true;
                    }
                    update_item_at(*ri, true);

                    // The gold might be beneath deep water.
                    if (detected && env.map_knowledge(*ri).item())
                        env.map_knowledge(*ri).flags |= MAP_DETECTED_ITEM;
                    break;
                }
            }
        }
        break;
    }
    case DACT_REMOVE_GOZAG_SHOPS:
    {
        vector<map_marker *> markers = env.markers.get_all(MAT_FEATURE);
        for (unsigned int i = 0; i < markers.size(); i++)
        {
            map_feature_marker *feat =
                dynamic_cast<map_feature_marker *>(markers[i]);
            ASSERT(feat);
            if (feat->feat == DNGN_ABANDONED_SHOP)
            {
                // TODO: clear shop data out?
                grd(feat->pos) = DNGN_ABANDONED_SHOP;
                view_update_at(feat->pos);
                env.markers.remove(feat);
            }
        }
        break;
    }
#if TAG_MAJOR_VERSION == 34
    case DACT_END_SPIRIT_HOWL:
    case DACT_HOLY_NEW_ATTEMPT:
    case DACT_ALLY_SACRIFICE_LOVE:
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
    // Hogs to humans
    monster orig;
    const bool could_see = you.can_see(mon);

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
        define_monster(&orig);
    }
    // Keep at same spot. This position is irrelevant if the hog is in transit.
    // See below.
    const coord_def pos = mon->pos();
    // Preserve relative HP.
    const float hp
        = (float) mon->hit_points / (float) mon->max_hit_points;
    // Preserve some flags.
    const uint64_t preserve_flags =
        mon->flags & ~(MF_JUST_SUMMONED | MF_WAS_IN_VIEW);
    // Preserve enchantments.
    mon_enchant_list enchantments = mon->enchantments;
    FixedBitVector<NUM_ENCHANTMENTS> ench_cache = mon->ench_cache;

    // Restore original monster.
    *mon = orig;

    // If the hog is in transit, then it is NOT stored in the normal
    // monster list (env.mons or menv for short). We cannot call move_to_pos
    // on such a hog, because move_to_pos will attempt to update the
    // monster grid (env.mgrid or mgrd for short). Since the hog is not
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

    const bool can_see = you.can_see(mon);

    // A monster changing factions while in the arena messes up
    // arena book-keeping.
    if (!crawl_state.game_is_arena())
    {
        // * A monster's attitude shouldn't downgrade from friendly
        //   or good-neutral because you helped it.  It'd suck to
        //   lose a permanent ally that way.
        //
        // * A monster has to be smart enough to realize that you
        //   helped it.
        if (mon->attitude == ATT_HOSTILE
            && mons_intel(mon) >= I_NORMAL)
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
