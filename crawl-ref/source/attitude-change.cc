/**
 * @file
 * @brief Monster attitude changing due to religion.
**/

#include "AppHdr.h"

#include "attitude-change.h"

#include <sstream>

#include "act-iter.h"
#include "branch.h"
#include "coordit.h"
#include "database.h"
#include "env.h"
#include "godabil.h"
#include "godcompanions.h"
#include "godpassive.h" // passive_t::convert_orcs
#include "libutil.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-tentacle.h"
#include "religion.h"
#include "state.h"
#include "travel.h"

// Called whenever an already existing monster changes its attitude, possibly
// temporarily.
void mons_att_changed(monster* mon)
{
    const mon_attitude_type att = mon->temp_attitude();
    const monster_type mc = mons_base_type(*mon);

    if (mons_is_tentacle_head(mc)
        || mons_is_solo_tentacle(mc))
    {
        for (monster_iterator mi; mi; ++mi)
            if (mi->is_child_tentacle_of(mon))
            {
                mi->attitude = att;
                if (!mons_is_solo_tentacle(mc))
                {
                    for (monster_iterator connect; connect; ++connect)
                    {
                        if (connect->is_child_tentacle_of(*mi))
                            connect->attitude = att;
                    }
                }

                // It's almost always flipping between hostile and friendly;
                // enslaving a pacified starspawn is still a shock.
                mi->stop_constricting_all();
            }
    }

    if (mon->attitude == ATT_HOSTILE
        && (mons_is_god_gift(*mon, GOD_BEOGH)
           || mons_is_god_gift(*mon, GOD_YREDELEMNUL)))
    {
        remove_companion(mon);
    }
    mon->align_avatars();
}

static void _jiyva_convert_slime(monster* slime);
static void _fedhas_neutralise_plant(monster* plant);

void beogh_follower_convert(monster* mons, bool orc_hit)
{
    if (!species_is_orcish(you.species) || crawl_state.game_is_arena())
        return;

    // For followers of Beogh, decide whether orcs will join you.
    if (will_have_passive(passive_t::convert_orcs)
        && mons->foe == MHITYOU
        && mons_genus(mons->type) == MONS_ORC
        && !mons->is_summoned()
        && !mons->is_shapeshifter()
        && !testbits(mons->flags, MF_ATT_CHANGE_ATTEMPT)
        && !mons->friendly()
        && you.visible_to(mons) && !mons->asleep()
        && !mons_is_confused(*mons) && !mons->paralysed())
    {
        mons->flags |= MF_ATT_CHANGE_ATTEMPT;

        const int hd = mons->get_experience_level();

        if (have_passive(passive_t::convert_orcs)
            && random2(you.piety / 15) + random2(4 + you.experience_level / 3)
                 > random2(hd) + hd + random2(5))
        {
            beogh_convert_orc(mons, orc_hit || !mons->alive() ? conv_t::DEATHBED
                                                              : conv_t::SIGHT);
            stop_running();
        }
    }
}

void slime_convert(monster* mons)
{
    if (have_passive(passive_t::neutral_slimes) && mons_is_slime(*mons)
        && !mons->neutral()
        && !mons->friendly()
        && !testbits(mons->flags, MF_ATT_CHANGE_ATTEMPT))
    {
        mons->flags |= MF_ATT_CHANGE_ATTEMPT;
        if (!player_under_penance())
        {
            _jiyva_convert_slime(mons);
            stop_running();
        }
    }
}

void fedhas_neutralise(monster* mons)
{
    if (have_passive(passive_t::friendly_plants)
        && mons->attitude == ATT_HOSTILE
        && fedhas_neutralises(*mons)
        && !testbits(mons->flags, MF_ATT_CHANGE_ATTEMPT))
    {
        _fedhas_neutralise_plant(mons);
        mons->flags |= MF_ATT_CHANGE_ATTEMPT;
        del_exclude(mons->pos());
    }
}

// Make summoned (temporary) god gifts disappear on penance or when
// abandoning the god in question (Trog or TSO).
bool make_god_gifts_disappear()
{
    const god_type god =
        (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                      : GOD_NO_GOD;
    int count = 0;

    for (monster_iterator mi; mi; ++mi)
    {
        if (is_follower(**mi)
            && mi->has_ench(ENCH_ABJ)
            && mons_is_god_gift(**mi, god))
        {
            if (simple_monster_message(**mi, " abandons you!"))
                count++;

            // The monster disappears.
            monster_die(*mi, KILL_DISMISSED, NON_MONSTER);
        }
    }

    return count;
}

// When under penance, Yredelemnulites can lose all nearby undead slaves.
bool yred_slaves_abandon_you()
{
    int num_reclaim = 0;
    int num_slaves = 0;

    for (radius_iterator ri(you.pos(), LOS_DEFAULT); ri; ++ri)
    {
        monster* mons = monster_at(*ri);
        if (mons == nullptr)
            continue;

        if (is_yred_undead_slave(*mons))
        {
            num_slaves++;

            const int hd = mons->get_experience_level();

            // During penance, followers get a saving throw.
            if (random2((you.piety - you.penance[GOD_YREDELEMNUL]) / 18)
                + random2(you.skill(SK_INVOCATIONS) - 6)
                > random2(hd) + hd + random2(5))
            {
                continue;
            }

            mons->attitude = ATT_HOSTILE;
            behaviour_event(mons, ME_ALERT, &you);
            // For now CREATED_FRIENDLY stays.
            mons_att_changed(mons);

            num_reclaim++;
        }
    }

    if (num_reclaim > 0)
    {
        if (num_reclaim == 1 && num_slaves > 1)
            simple_god_message(" reclaims one of your granted undead slaves!");
        else if (num_reclaim == num_slaves)
            simple_god_message(" reclaims your granted undead slaves!");
        else
            simple_god_message(" reclaims some of your granted undead slaves!");
        return true;
    }

    return false;
}

// When under penance, Beoghites can lose all nearby orcish followers,
// subject to a few limitations.
bool beogh_followers_abandon_you()
{
    bool reconvert = false;
    int num_reconvert = 0;
    int num_followers = 0;

    for (radius_iterator ri(you.pos(), LOS_DEFAULT); ri; ++ri)
    {
        monster* mons = monster_at(*ri);
        if (mons == nullptr)
            continue;

        // Note that orc high priests' summons are gifts of Beogh,
        // so we can't use is_orcish_follower(*) here.
        if (mons_is_god_gift(*mons, GOD_BEOGH))
        {
            num_followers++;

            if (you.visible_to(mons)
                && !mons->asleep()
                && !mons_is_confused(*mons)
                && !mons->cannot_act())
            {
                const int hd = mons->get_experience_level();

                // During penance, followers get a saving throw.
                if (random2((you.piety - you.penance[GOD_BEOGH]) / 18)
                    + random2(you.skill(SK_INVOCATIONS) - 6)
                    > random2(hd) + hd + random2(5))
                {
                    continue;
                }

                mons->attitude = ATT_HOSTILE;
                behaviour_event(mons, ME_ALERT, &you);
                // For now CREATED_FRIENDLY stays.
                mons_att_changed(mons);

                if (you.can_see(*mons))
                    num_reconvert++; // Only visible ones.

                reconvert = true;
            }
        }
    }

    if (reconvert) // Maybe all of them are invisible.
    {
        simple_god_message("'s voice booms out, \"Who do you think you "
                           "are?\"", GOD_BEOGH);

        ostream& chan = msg::streams(MSGCH_MONSTER_ENCHANT);

        if (num_reconvert > 0)
        {
            if (num_reconvert == 1 && num_followers > 1)
                chan << "One of your followers decides to abandon you.";
            else if (num_reconvert == num_followers)
                chan << "Your followers decide to abandon you.";
            else
                chan << "Some of your followers decide to abandon you.";
        }

        chan << endl;

        return true;
    }

    return false;
}

static void _print_converted_orc_speech(const string& key,
                                        monster* mon,
                                        msg_channel_type channel)
{
    string msg = getSpeakString("beogh_converted_orc_" + key);

    if (!msg.empty())
    {
        msg = do_mon_str_replacements(msg, *mon);
        strip_channel_prefix(msg, channel);
        mprf(channel, "%s", msg.c_str());
    }
}

// Orcs may turn friendly when encountering followers of Beogh, and be
// made gifts of Beogh.
void beogh_convert_orc(monster* orc, conv_t conv)
{
    ASSERT(orc); // XXX: change to monster &orc
    ASSERT(mons_genus(orc->type) == MONS_ORC);

    switch (conv)
    {
    case conv_t::DEATHBED_FOLLOWER:
        _print_converted_orc_speech("reaction_battle_follower", orc,
                                    MSGCH_FRIEND_ENCHANT);
        _print_converted_orc_speech("speech_battle_follower", orc,
                                    MSGCH_TALK);
        break;
    case conv_t::DEATHBED:
        _print_converted_orc_speech("reaction_battle", orc,
                                    MSGCH_FRIEND_ENCHANT);
        _print_converted_orc_speech("speech_battle", orc, MSGCH_TALK);
        break;
    case conv_t::SIGHT:
        _print_converted_orc_speech("reaction_sight", orc,
                                    MSGCH_FRIEND_ENCHANT);

        if (!one_chance_in(3))
            _print_converted_orc_speech("speech_sight", orc, MSGCH_TALK);
        break;
    case conv_t::RESURRECTION:
        _print_converted_orc_speech("resurrection", orc,
                                    MSGCH_FRIEND_ENCHANT);
        break;
    }

    orc->attitude = ATT_FRIENDLY;

    // The monster is not really *created* friendly, but should it
    // become hostile later on, it won't count as a good kill.
    orc->flags |= MF_NO_REWARD;

    if (orc->is_patrolling())
    {
        // Make orcs stop patrolling and forget their patrol point,
        // they're supposed to follow you now.
        orc->patrol_point = coord_def(0, 0);
    }

    if (!orc->alive())
        orc->hit_points = min(random_range(1, 4), orc->max_hit_points);

    mons_make_god_gift(*orc, GOD_BEOGH);
    add_companion(orc);

    // Avoid immobile "followers".
    behaviour_event(orc, ME_ALERT);

    mons_att_changed(orc);
}

static void _fedhas_neutralise_plant(monster* plant)
{
    if (!plant
        || !fedhas_neutralises(*plant)
        || plant->attitude != ATT_HOSTILE
        || testbits(plant->flags, MF_ATT_CHANGE_ATTEMPT))
    {
        return;
    }

    plant->attitude = ATT_GOOD_NEUTRAL;
    plant->flags   |= MF_WAS_NEUTRAL;
    mons_att_changed(plant);
}

static void _jiyva_convert_slime(monster* slime)
{
    ASSERT(slime); // XXX: change to monster &slime
    ASSERT(mons_is_slime(*slime));

    behaviour_event(slime, ME_ALERT);

    if (you.can_see(*slime))
    {
        if (mons_genus(slime->type) == MONS_FLOATING_EYE)
        {
            mprf(MSGCH_GOD, "%s stares at you suspiciously for a moment, "
                            "then relaxes.",

            slime->name(DESC_THE).c_str());
        }
        else
        {
            mprf(MSGCH_GOD, "%s trembles before you.",
                 slime->name(DESC_THE).c_str());
        }
    }

    slime->attitude = ATT_STRICT_NEUTRAL;
    slime->flags   |= MF_WAS_NEUTRAL;

    mons_make_god_gift(*slime, GOD_JIYVA);

    mons_att_changed(slime);
}

void gozag_set_bribe(monster* traitor)
{
    // Try to bribe the monster.
    const int bribability = gozag_type_bribable(traitor->type);

    if (bribability <= 0 || traitor->friendly())
        return;

    const monster* leader =
        traitor->props.exists("band_leader")
        ? monster_by_mid(traitor->props["band_leader"].get_int())
        : nullptr;

    if (leader)
    {
        if (leader->has_ench(ENCH_FRIENDLY_BRIBED)
            || leader->props.exists(FRIENDLY_BRIBE_KEY))
        {
            traitor->props[FRIENDLY_BRIBE_KEY].get_bool() = true;
        }
        else if (leader->has_ench(ENCH_NEUTRAL_BRIBED)
                 || leader->props.exists(NEUTRAL_BRIBE_KEY))
        {
            traitor->props[NEUTRAL_BRIBE_KEY].get_bool() = true;
        }
    }
    else if (x_chance_in_y(bribability, GOZAG_MAX_BRIBABILITY))
    {
        // Sometimes get permanent followers
        if (one_chance_in(3))
            traitor->props[FRIENDLY_BRIBE_KEY].get_bool() = true;
        else
            traitor->props[NEUTRAL_BRIBE_KEY].get_bool() = true;
    }
}

void gozag_check_bribe(monster* traitor)
{
    branch_type branch = gozag_fixup_branch(you.where_are_you);

    if (branch_bribe[branch] == 0)
        return; // Do nothing if branch isn't currently bribed.

    const int base_cost = max(1, exper_value(*traitor) / 20);

    int cost = 0;

    string msg;

    if (traitor->props.exists(FRIENDLY_BRIBE_KEY))
    {
        traitor->props.erase(FRIENDLY_BRIBE_KEY);
        traitor->add_ench(mon_enchant(ENCH_FRIENDLY_BRIBED, 0, 0,
                                      INFINITE_DURATION));
        msg = getSpeakString(traitor->name(DESC_DBNAME, true)
                             + " Gozag permabribe");
        if (msg.empty())
            msg = getSpeakString("Gozag permabribe");

        // Actual allies deduct more gold.
        cost = 2 * base_cost;
    }
    else if (traitor->props.exists(NEUTRAL_BRIBE_KEY))
    {
        traitor->props.erase(NEUTRAL_BRIBE_KEY);
        traitor->add_ench(ENCH_NEUTRAL_BRIBED);
        msg = getSpeakString(traitor->name(DESC_DBNAME, true)
                             + " Gozag bribe");
        if (msg.empty())
            msg = getSpeakString("Gozag bribe");

        cost = base_cost;
    }

    if (!msg.empty())
    {
        msg_channel_type channel = MSGCH_FRIEND_ENCHANT;
        msg = do_mon_str_replacements(msg, *traitor);
        strip_channel_prefix(msg, channel);
        mprf(channel, "%s", msg.c_str());
        // !msg.empty means a monster was bribed.
        gozag_deduct_bribe(branch, cost);
    }
}

void gozag_break_bribe(monster* victim)
{
    ASSERT(victim); // XXX: change to monster &victim

    if (!victim->has_ench(ENCH_NEUTRAL_BRIBED)
        && !victim->has_ench(ENCH_FRIENDLY_BRIBED)
        && !victim->props.exists(NEUTRAL_BRIBE_KEY)
        && !victim->props.exists(FRIENDLY_BRIBE_KEY))
    {
        return;
    }

    // Un-bribe the victim.
    victim->del_ench(ENCH_NEUTRAL_BRIBED);
    victim->del_ench(ENCH_FRIENDLY_BRIBED);
    victim->props.erase(NEUTRAL_BRIBE_KEY);
    victim->props.erase(FRIENDLY_BRIBE_KEY);

    // Make other nearby bribed monsters un-bribed, too.
    for (monster_iterator mi; mi; ++mi)
        if (mi->can_see(*victim))
            gozag_break_bribe(*mi);
}
