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
#include "fineff.h"
#include "god-abil.h"
#include "god-companions.h"
#include "god-passive.h" // passive_t::convert_orcs
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

    if (mon->has_ench(ENCH_AWAKEN_FOREST))
        mon->del_ench(ENCH_AWAKEN_FOREST);

    mon->remove_summons(true);
}

static void _jiyva_convert_slime(monster* slime);
static void _fedhas_neutralise_plant(monster* plant);

/// For followers of Beogh, decide whether orcs will join you.
void beogh_follower_convert(monster* mons, bool orc_hit)
{
    if (you.religion != GOD_BEOGH || crawl_state.game_is_arena())
        return;

    const bool deathbed = orc_hit || !mons->alive();

    if (!will_have_passive(passive_t::convert_orcs)
        || mons_genus(mons->type) != MONS_ORC
        || mons->is_summoned()
        || mons->is_shapeshifter()
        || testbits(mons->flags, MF_ATT_CHANGE_ATTEMPT)
        || mons->friendly()
        || mons->has_ench(ENCH_FIRE_CHAMPION)
        || mons->flags & MF_APOSTLE_BAND
        // If marked for vengeance, only deathbed conversion.
        || (mons->has_ench(ENCH_VENGEANCE_TARGET) && !deathbed))
    {
        return;
    }

    mons->flags |= MF_ATT_CHANGE_ATTEMPT;

    const int hd = mons->get_experience_level();

    if (have_passive(passive_t::convert_orcs)
        && random2(you.piety / 15) + random2(4 + you.experience_level / 3)
             > random2(hd) + hd + random2(5))
    {
        conv_t ctype = conv_t::sight;
        if (deathbed)
        {
            ctype = mons->has_ench(ENCH_VENGEANCE_TARGET) ? conv_t::vengeance
                                                          : conv_t::deathbed;
        }

        beogh_convert_orc(mons, ctype);
        stop_running();
    }
}

void slime_convert(monster* mons)
{
    if (have_passive(passive_t::neutral_slimes) && mons_is_slime(*mons)
        && !mons->neutral()
        && !mons->friendly()
        && !mons->is_shapeshifter()
        && !testbits(mons->flags, MF_ATT_CHANGE_ATTEMPT))
    {
        mons->flags |= MF_ATT_CHANGE_ATTEMPT;
        _jiyva_convert_slime(mons);
        stop_running();
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

// Make divine summons disappear on penance or excommunication from the god in question
void dismiss_god_summons(god_type god)
{
    for (monster_iterator mi; mi; ++mi)
    {
        if (is_follower(**mi)
            && mi->is_summoned()
            && mons_is_god_gift(**mi, god))
        {
            // The monster disappears.
            monster_die(**mi, KILL_RESET, NON_MONSTER);
        }
    }
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

// Orcs may turn good neutral when encountering followers of Beogh and leave you alone
void beogh_convert_orc(monster* orc, conv_t conv)
{
    ASSERT(orc); // XXX: change to monster &orc
    ASSERT(mons_genus(orc->type) == MONS_ORC);

    switch (conv)
    {
    case conv_t::vengeance_follower:
        _print_converted_orc_speech("reaction_battle_follower", orc,
                                    MSGCH_FRIEND_ENCHANT);
        _print_converted_orc_speech("speech_vengeance_follower", orc,
                                    MSGCH_TALK);
        break;
    case conv_t::vengeance:
        _print_converted_orc_speech("reaction_battle", orc,
                                    MSGCH_FRIEND_ENCHANT);
        _print_converted_orc_speech("speech_vengeance", orc,
                                    MSGCH_TALK);
        break;
    case conv_t::deathbed_follower:
        _print_converted_orc_speech("reaction_battle_follower", orc,
                                    MSGCH_FRIEND_ENCHANT);
        _print_converted_orc_speech("speech_battle_follower", orc,
                                    MSGCH_TALK);
        break;
    case conv_t::deathbed:
        _print_converted_orc_speech("reaction_battle", orc,
                                    MSGCH_FRIEND_ENCHANT);
        _print_converted_orc_speech("speech_battle", orc, MSGCH_TALK);
        break;
    case conv_t::sight:
        _print_converted_orc_speech("reaction_sight", orc,
                                    MSGCH_FRIEND_ENCHANT);

        if (!one_chance_in(3))
            _print_converted_orc_speech("speech_sight", orc, MSGCH_TALK);
        break;
    case conv_t::resurrection:
        _print_converted_orc_speech("resurrection", orc,
                                    MSGCH_FRIEND_ENCHANT);
        break;
    }

    // Count as having gotten vengeance.
    if (orc->has_ench(ENCH_VENGEANCE_TARGET))
    {
        orc->del_ench(ENCH_VENGEANCE_TARGET);
        beogh_progress_vengeance();
    }

    if (!orc->alive())
    {
        orc->hit_points = max(1, random_range(orc->max_hit_points / 5,
                                              orc->max_hit_points * 2 / 5));
    }

    record_monster_defeat(orc, KILL_PACIFIED);
    mons_pacify(*orc, ATT_GOOD_NEUTRAL, true);

    // put the actual revival at the end of the round
    // But first verify that the orc is still here! (They may have left the
    // floor immediately when pacified!)
    if ((conv == conv_t::deathbed
         || conv == conv_t::deathbed_follower
         || conv == conv_t::vengeance
         || conv == conv_t::vengeance_follower)
        && orc->alive())
    {
        avoided_death_fineff::schedule(orc);
    }
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

    slime->attitude = ATT_GOOD_NEUTRAL;
    slime->flags   |= MF_WAS_NEUTRAL;

    mons_make_god_gift(*slime, GOD_JIYVA);

    mons_att_changed(slime);
}

void gozag_set_bribe(monster* traitor)
{
    // Try to bribe the monster.
    const int bribability = gozag_type_bribable(traitor->type);

    if (bribability <= 0 || traitor->friendly() || traitor->is_summoned())
        return;

    const monster* leader = traitor->get_band_leader();
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

    const int base_cost = max(1, exper_value(*traitor, true, true) / 20);

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
        && !victim->has_ench(ENCH_FRIENDLY_BRIBED))
    {
        return;
    }

    // Un-bribe the victim.
    victim->del_ench(ENCH_NEUTRAL_BRIBED);
    victim->del_ench(ENCH_FRIENDLY_BRIBED);

    // Make other nearby bribed monsters un-bribed, too.
    for (monster_iterator mi; mi; ++mi)
        if (mi->can_see(*victim))
            gozag_break_bribe(*mi);
}

// Conversions and bribes.
void do_conversions(monster* target)
{
        beogh_follower_convert(target);
        gozag_check_bribe(target);
}
