/**
 * @file
 * @brief Monster attitude changing due to religion.
**/

#include "AppHdr.h"

#include "attitude-change.h"

#include <sstream>

#include "act-iter.h"
#include "coordit.h"
#include "database.h"
#include "env.h"
#include "godcompanions.h"
#include "goditem.h"
#include "itemprop.h"
#include "libutil.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-util.h"
#include "monster.h"
#include "mon-stuff.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "state.h"
#include "travel.h"
#include "transform.h"

static void _jiyva_convert_slime(monster* slime);
static void _fedhas_neutralise_plant(monster* plant);
static void _good_god_holy_fail_attitude_change(monster* holy);

void good_god_follower_attitude_change(monster* mons)
{
    if (you.undead_or_demonic() || crawl_state.game_is_arena())
        return;

    // For followers of good gods, decide whether holy beings will be
    // good neutral towards you.
    if (is_good_god(you.religion)
        && mons->foe == MHITYOU
        && mons->is_holy()
        && !testbits(mons->flags, MF_ATT_CHANGE_ATTEMPT)
        && !mons->wont_attack()
        && you.visible_to(mons) && !mons->asleep()
        && !mons_is_confused(mons) && !mons->paralysed())
    {
        mons->flags |= MF_ATT_CHANGE_ATTEMPT;

        if (x_chance_in_y(you.piety, MAX_PIETY) && !you.penance[you.religion])
        {
            const item_def* wpn = you.weapon();
            if (wpn
                && wpn->base_type == OBJ_WEAPONS
                && (is_unholy_item(*wpn)
                    || is_evil_item(*wpn))
                && coinflip()) // 50% chance of conversion failing
            {
                msg::stream << mons->name(DESC_THE)
                            << " glares at your weapon."
                            << endl;
                _good_god_holy_fail_attitude_change(mons);
                return;
            }
            good_god_holy_attitude_change(mons);
            stop_running();
        }
        else
            _good_god_holy_fail_attitude_change(mons);
    }
}

void beogh_follower_convert(monster* mons, bool orc_hit)
{
    if (!player_genus(GENPC_ORCISH) || crawl_state.game_is_arena())
        return;

    // For followers of Beogh, decide whether orcs will join you.
    if (you_worship(GOD_BEOGH)
        && mons->foe == MHITYOU
        && mons_genus(mons->type) == MONS_ORC
        && !mons->is_summoned()
        && !mons->is_shapeshifter()
        && !testbits(mons->flags, MF_ATT_CHANGE_ATTEMPT)
        && !mons->friendly()
        && you.visible_to(mons) && !mons->asleep()
        && !mons_is_confused(mons) && !mons->paralysed())
    {
        mons->flags |= MF_ATT_CHANGE_ATTEMPT;

        const int hd = mons->hit_dice;

        if (you.piety >= piety_breakpoint(2) && !player_under_penance()
            && random2(you.piety / 15) + random2(4 + you.experience_level / 3)
                 > random2(hd) + hd + random2(5))
        {
            beogh_convert_orc(mons, orc_hit);
            stop_running();
        }
    }
}

void slime_convert(monster* mons)
{
    if (you_worship(GOD_JIYVA) && mons_is_slime(mons)
        && !mons->is_shapeshifter()
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
    if (you_worship(GOD_FEDHAS)
        && mons->attitude == ATT_HOSTILE
        && fedhas_neutralises(mons)
        && !testbits(mons->flags, MF_ATT_CHANGE_ATTEMPT)
        && !player_under_penance())
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
        if (is_follower(*mi)
            && mi->has_ench(ENCH_ABJ)
            && mons_is_god_gift(*mi, god))
        {
            if (simple_monster_message(*mi, " abandons you!"))
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

    for (radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri)
    {
        monster* mons = monster_at(*ri);
        if (mons == NULL)
            continue;

        if (is_yred_undead_slave(mons))
        {
            num_slaves++;

            const int hd = mons->hit_dice;

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

    for (radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri)
    {
        monster* mons = monster_at(*ri);
        if (mons == NULL)
            continue;

        // Note that orc high priests' summons are gifts of Beogh,
        // so we can't use is_orcish_follower() here.
        if (mons_is_god_gift(mons, GOD_BEOGH))
        {
            num_followers++;

            if (you.visible_to(mons)
                && !mons->asleep()
                && !mons_is_confused(mons)
                && !mons->cannot_act())
            {
                const int hd = mons->hit_dice;

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

                if (you.can_see(mons))
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

static void _print_good_god_holy_being_speech(bool neutral,
                                              const string key,
                                              monster* mon,
                                              msg_channel_type channel)
{
    string full_key = "good_god_";
    if (!neutral)
        full_key += "non";
    full_key += "neutral_holy_being_";
    full_key += key;

    string msg = getSpeakString(full_key);

    if (!msg.empty())
    {
        msg = do_mon_str_replacements(msg, mon);
        strip_channel_prefix(msg, channel);
        mpr(msg.c_str(), channel);
    }
}

// Holy monsters may turn good neutral when encountering followers of
// the good gods, and be made worshippers of TSO if necessary.
void good_god_holy_attitude_change(monster* holy)
{
    ASSERT(holy->is_holy());

    if (you.can_see(holy)) // show reaction
    {
        string key = "reaction";

        // Quadrupeds can't salute, etc.
        mon_body_shape shape = get_mon_shape(holy);
        if (shape >= MON_SHAPE_HUMANOID && shape <= MON_SHAPE_NAGA)
            key += "_humanoid";

        _print_good_god_holy_being_speech(true, key, holy,
                                          MSGCH_FRIEND_ENCHANT);

        if (!one_chance_in(3)
            && holy->can_speak()
            && holy->type != MONS_MENNAS) // Mennas is mute and only has visual speech
            _print_good_god_holy_being_speech(true, "speech", holy,
                                              MSGCH_TALK);
    }

    holy->attitude = ATT_GOOD_NEUTRAL;

    // The monster is not really *created* neutral, but should it become
    // hostile later on, it won't count as a good kill.
    holy->flags |= MF_WAS_NEUTRAL;

    // If the holy being was previously worshipping a different god,
    // make it worship TSO.
    holy->god = GOD_SHINING_ONE;

    // Avoid immobile "followers".
    behaviour_event(holy, ME_ALERT);

    mons_att_changed(holy);
}

static void _good_god_holy_fail_attitude_change(monster* holy)
{
    ASSERT(holy->is_holy());

    if (you.can_see(holy)) // show reaction
    {
        string key = "reaction";

        // Quadrupeds can't salute, etc.
        mon_body_shape shape = get_mon_shape(holy);
        if (shape >= MON_SHAPE_HUMANOID && shape <= MON_SHAPE_NAGA)
            key += "_humanoid";

        _print_good_god_holy_being_speech(false, key, holy,
                                          MSGCH_FRIEND_ENCHANT);

        if (!one_chance_in(3)
            && holy->can_speak()
            && holy->type != MONS_MENNAS) // Mennas is mute and only has visual speech
            _print_good_god_holy_being_speech(false, "speech", holy,
                                              MSGCH_TALK);
    }
}

static void _print_converted_orc_speech(const string key,
                                        monster* mon,
                                        msg_channel_type channel)
{
    string msg = getSpeakString("beogh_converted_orc_" + key);

    if (!msg.empty())
    {
        msg = do_mon_str_replacements(msg, mon);
        strip_channel_prefix(msg, channel);
        mpr(msg.c_str(), channel);
    }
}

// Orcs may turn friendly when encountering followers of Beogh, and be
// made gifts of Beogh.
void beogh_convert_orc(monster* orc, bool emergency,
                       bool converted_by_follower)
{
    ASSERT(mons_genus(orc->type) == MONS_ORC);

    if (you.can_see(orc)) // show reaction
    {
        if (emergency || !orc->alive())
        {
            if (converted_by_follower)
            {
                _print_converted_orc_speech("reaction_battle_follower", orc,
                                            MSGCH_FRIEND_ENCHANT);
                _print_converted_orc_speech("speech_battle_follower", orc,
                                            MSGCH_TALK);
            }
            else
            {
                _print_converted_orc_speech("reaction_battle", orc,
                                            MSGCH_FRIEND_ENCHANT);
                _print_converted_orc_speech("speech_battle", orc, MSGCH_TALK);
            }
        }
        else
        {
            _print_converted_orc_speech("reaction_sight", orc,
                                        MSGCH_FRIEND_ENCHANT);

            if (!one_chance_in(3))
                _print_converted_orc_speech("speech_sight", orc, MSGCH_TALK);
        }
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

    mons_make_god_gift(orc, GOD_BEOGH);
    add_companion(orc);

    // Avoid immobile "followers".
    behaviour_event(orc, ME_ALERT);

    mons_att_changed(orc);
}

static void _fedhas_neutralise_plant(monster* plant)
{
    if (!plant
        || !fedhas_neutralises(plant)
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
    ASSERT(mons_is_slime(slime));

    behaviour_event(slime, ME_ALERT);

    if (you.can_see(slime))
    {
        if (mons_genus(slime->type) == MONS_GIANT_EYEBALL)
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

    if (!mons_eats_items(slime))
    {
        slime->add_ench(ENCH_EAT_ITEMS);

        mprf(MSGCH_MONSTER_ENCHANT, "%s looks hungrier.",
             slime->name(DESC_THE).c_str());
    }

    mons_make_god_gift(slime, GOD_JIYVA);

    mons_att_changed(slime);
}
