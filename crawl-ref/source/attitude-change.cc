/*
 * Summary:   Monster attitude changing due to religion.
 */

#include "AppHdr.h"

#include "attitude-change.h"

#include <sstream>

#include "coordit.h"
#include "database.h"
#include "env.h"
#include "files.h"
#include "godabil.h"
#include "goditem.h"
#include "itemprop.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-iter.h"
#include "mon-util.h"
#include "monster.h"
#include "mon-stuff.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "state.h"
#include "stuff.h"
#include "travel.h"

void good_god_follower_attitude_change(monsters *monster)
{
    if (you.undead_or_demonic() || crawl_state.arena)
        return;

    // For followers of good gods, decide whether holy beings will be
    // good neutral towards you.
    if (is_good_god(you.religion)
        && monster->foe == MHITYOU
        && monster->is_holy()
        && !testbits(monster->flags, MF_ATT_CHANGE_ATTEMPT)
        && !monster->wont_attack()
        && you.visible_to(monster) && !monster->asleep()
        && !mons_is_confused(monster) && !monster->paralysed())
    {
        monster->flags |= MF_ATT_CHANGE_ATTEMPT;

        if (x_chance_in_y(you.piety, MAX_PIETY) && !you.penance[you.religion])
        {
            const item_def* wpn = you.weapon();
            if (wpn
                && wpn->base_type == OBJ_WEAPONS
                && (is_unholy_item(*wpn)
                    || is_evil_item(*wpn))
                && coinflip()) // 50% chance of conversion failing
            {
                msg::stream << monster->name(DESC_CAP_THE)
                            << " glares at your weapon."
                            << std::endl;
                good_god_holy_fail_attitude_change(monster);
                return;
            }
            good_god_holy_attitude_change(monster);
            stop_running();
        }
        else
            good_god_holy_fail_attitude_change(monster);
    }
}

void beogh_follower_convert(monsters *monster, bool orc_hit)
{
    if (you.species != SP_HILL_ORC || crawl_state.arena)
        return;

    // For followers of Beogh, decide whether orcs will join you.
    if (you.religion == GOD_BEOGH
        && monster->foe == MHITYOU
        && mons_species(monster->type) == MONS_ORC
        && !monster->is_summoned()
        && !monster->is_shapeshifter()
        && !testbits(monster->flags, MF_ATT_CHANGE_ATTEMPT)
        && !monster->friendly()
        && you.visible_to(monster) && !monster->asleep()
        && !mons_is_confused(monster) && !monster->paralysed())
    {
        monster->flags |= MF_ATT_CHANGE_ATTEMPT;

        const int hd = monster->hit_dice;

        if (you.piety >= piety_breakpoint(2) && !player_under_penance()
            && random2(you.piety / 15) + random2(4 + you.experience_level / 3)
                 > random2(hd) + hd + random2(5))
        {
            if (you.weapon()
                && you.weapon()->base_type == OBJ_WEAPONS
                && get_weapon_brand(*you.weapon()) == SPWPN_ORC_SLAYING
                && coinflip()) // 50% chance of conversion failing
            {
                msg::stream << monster->name(DESC_CAP_THE)
                            << " flinches from your weapon."
                            << std::endl;
                return;
            }
            beogh_convert_orc(monster, orc_hit);
            stop_running();
        }
    }
}

void slime_convert(monsters* monster)
{
    if (you.religion == GOD_JIYVA && mons_is_slime(monster)
        && !monster->is_summoned()
        && !monster->is_shapeshifter()
        && !monster->neutral()
        && !monster->friendly()
        && !testbits(monster->flags, MF_ATT_CHANGE_ATTEMPT)
        && you.visible_to(monster) && !monster->asleep()
        && !mons_is_confused(monster) && !monster->paralysed())
    {
        monster->flags |= MF_ATT_CHANGE_ATTEMPT;
        if (!player_under_penance())
        {
            jiyva_convert_slime(monster);
            stop_running();
        }
    }
}

void fedhas_neutralise(monsters* monster)
{
    if (you.religion == GOD_FEDHAS
        && monster->attitude == ATT_HOSTILE
        && fedhas_neutralises(monster)
        && !testbits(monster->flags, MF_ATT_CHANGE_ATTEMPT)
        && !player_under_penance())
    {
        fedhas_neutralise_plant(monster);
        monster->flags |= MF_ATT_CHANGE_ATTEMPT;
        del_exclude(monster->pos());
    }
}

static bool _holy_beings_on_level_attitude_change()
{
    bool success = false;

    for (monster_iterator mi; mi; ++mi)
    {
        if (!mi->is_holy())
            continue;

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Holy attitude changing: %s on level %d, branch %d",
             mi->name(DESC_PLAIN).c_str(),
             static_cast<int>(you.your_level),
             static_cast<int>(you.where_are_you));
#endif

        // If you worship a good god, you get another chance to make
        // neutral and hostile holy beings good neutral.
        if (is_good_god(you.religion) && !mi->wont_attack())
        {
            if (testbits(mi->flags, MF_ATT_CHANGE_ATTEMPT))
            {
                mi->flags &= ~MF_ATT_CHANGE_ATTEMPT;

                success = true;
            }
        }
        // If you don't worship a good god, you make all friendly
        // and good neutral holy beings that worship a good god
        // hostile.
        else if (!is_good_god(you.religion) && mi->wont_attack()
            && is_good_god(mi->god))
        {
            mi->attitude = ATT_HOSTILE;
            mi->del_ench(ENCH_CHARM, true);
            behaviour_event(*mi, ME_ALERT, MHITYOU);
            // For now CREATED_FRIENDLY/WAS_NEUTRAL stays.

            success = true;
        }
    }

    return (success);
}

bool holy_beings_attitude_change()
{
    return (apply_to_all_dungeons(_holy_beings_on_level_attitude_change));
}

static bool _unholy_and_evil_beings_on_level_attitude_change()
{
    bool success = false;

    for (monster_iterator mi; mi; ++mi)
    {
        if (!mi->is_unholy() && !mi->is_evil())
            continue;

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Unholy/evil attitude changing: %s "
             "on level %d, branch %d",
             mi->name(DESC_PLAIN, true).c_str(),
             static_cast<int>(you.your_level),
             static_cast<int>(you.where_are_you));
#endif

        // If you worship a good god, you make all friendly and good
        // neutral unholy and evil beings hostile.
        if (is_good_god(you.religion) && mi->wont_attack())
        {
            mi->attitude = ATT_HOSTILE;
            mi->del_ench(ENCH_CHARM, true);
            behaviour_event(*mi, ME_ALERT, MHITYOU);
            // For now CREATED_FRIENDLY/WAS_NEUTRAL stays.

            success = true;
        }
    }

    return (success);
}

bool unholy_and_evil_beings_attitude_change()
{
    return (apply_to_all_dungeons(_unholy_and_evil_beings_on_level_attitude_change));
}

static bool _unclean_and_chaotic_beings_on_level_attitude_change()
{
    bool success = false;

    for (monster_iterator mi; mi; ++mi)
    {
        if (!mi->is_unclean() && !mi->is_chaotic())
            continue;

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Unclean/chaotic attitude changing: %s on level %d, branch %d",
             mi->name(DESC_PLAIN).c_str(),
             static_cast<int>(you.your_level),
             static_cast<int>(you.where_are_you));
#endif

        // If you worship Zin, you make all friendly and good neutral
        // unclean and chaotic beings hostile.
        if (you.religion == GOD_ZIN && mi->wont_attack())
        {
            mi->attitude = ATT_HOSTILE;
            mi->del_ench(ENCH_CHARM, true);
            behaviour_event(*mi, ME_ALERT, MHITYOU);
            // For now CREATED_FRIENDLY/WAS_NEUTRAL stays.

            success = true;
        }
    }
    return (success);
}

bool unclean_and_chaotic_beings_attitude_change()
{
    return (apply_to_all_dungeons(_unclean_and_chaotic_beings_on_level_attitude_change));
}

static bool _spellcasters_on_level_attitude_change()
{
    bool success = false;

    for (monster_iterator mi; mi; ++mi)
    {
        if (!mi->is_actual_spellcaster())
            continue;

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Spellcaster attitude changing: %s on level %d, branch %d",
             mi->name(DESC_PLAIN).c_str(),
             static_cast<int>(you.your_level),
             static_cast<int>(you.where_are_you));
#endif

        // If you worship Trog, you make all friendly and good neutral
        // magic users hostile.
        if (you.religion == GOD_TROG && mi->wont_attack())
        {
            mi->attitude = ATT_HOSTILE;
            mi->del_ench(ENCH_CHARM, true);
            behaviour_event(*mi, ME_ALERT, MHITYOU);
            // For now CREATED_FRIENDLY/WAS_NEUTRAL stays.

            success = true;
        }
    }

    return (success);
}

bool spellcasters_attitude_change()
{
    return (apply_to_all_dungeons(_spellcasters_on_level_attitude_change));
}

// Make summoned (temporary) god gifts disappear on penance or when
// abandoning the god in question.  If seen, only count monsters where
// the player can see the change, and output a message.
static bool _make_god_gifts_on_level_disappear(bool seen = false)
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
            if (!seen || simple_monster_message(*mi, " abandons you!"))
                count++;

            // The monster disappears.
            monster_die(*mi, KILL_DISMISSED, NON_MONSTER);
        }
    }

    return (count);
}

static bool _god_gifts_disappear_wrapper()
{
    return (_make_god_gifts_on_level_disappear());
}

// Make god gifts disappear on all levels, or on only the current one.
bool make_god_gifts_disappear(bool level_only)
{
    bool success = _make_god_gifts_on_level_disappear(true);

    if (level_only)
        return (success);

    return (apply_to_all_dungeons(_god_gifts_disappear_wrapper) || success);
}

// When abandoning the god in question, turn holy god gifts good
// neutral.  If seen, only count monsters where the player can see the
// change, and output a message.
static bool _make_holy_god_gifts_on_level_good_neutral(bool seen = false)
{
    const god_type god =
        (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                      : GOD_NO_GOD;
    int count = 0;

    for (monster_iterator mi; mi; ++mi)
    {
        if (is_follower(*mi)
            && !mi->has_ench(ENCH_CHARM)
            && mi->is_holy()
            && mons_is_god_gift(*mi, god))
        {
            // monster changes attitude
            mi->attitude = ATT_GOOD_NEUTRAL;

            if (!seen || simple_monster_message(*mi, " becomes indifferent."))
                count++;
        }
    }

    return (count);
}

static bool _holy_god_gifts_good_neutral_wrapper()
{
    return (_make_holy_god_gifts_on_level_good_neutral());
}

// Make holy god gifts turn good neutral on all levels, or on only the
// current one.
bool make_holy_god_gifts_good_neutral(bool level_only)
{
    bool success = _make_holy_god_gifts_on_level_good_neutral(true);

    if (level_only)
        return (success);

    return (apply_to_all_dungeons(_holy_god_gifts_good_neutral_wrapper) || success);
}

// When abandoning the god in question, turn god gifts hostile.  If
// seen, only count monsters where the player can see the change, and
// output a message.
static bool _make_god_gifts_on_level_hostile(bool seen = false)
{
    const god_type god =
        (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                      : GOD_NO_GOD;
    int count = 0;

    for (monster_iterator mi; mi; ++mi)
    {
        if (is_follower(*mi)
            && mons_is_god_gift(*mi, god))
        {
            // monster changes attitude and behaviour
            mi->attitude = ATT_HOSTILE;
            mi->del_ench(ENCH_CHARM, true);
            behaviour_event(*mi, ME_ALERT, MHITYOU);

            if (!seen || simple_monster_message(*mi, " turns against you!"))
                count++;
        }
    }

    return (count);
}

static bool _god_gifts_hostile_wrapper()
{
    return (_make_god_gifts_on_level_hostile());
}

// Make god gifts turn hostile on all levels, or on only the current
// one.
bool make_god_gifts_hostile(bool level_only)
{
    bool success = _make_god_gifts_on_level_hostile(true);

    if (level_only)
        return (success);

    return (apply_to_all_dungeons(_god_gifts_hostile_wrapper) || success);
}

static bool _is_yred_enslaved_body_and_soul(const monsters* mon)
{
    return (mon->alive() && mons_enslaved_body_and_soul(mon));
}

static bool _yred_slaves_on_level_abandon_you()
{
    bool success = false;

    for (monster_iterator mi; mi; ++mi)
    {
        if (_is_yred_enslaved_body_and_soul(*mi))
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Undead soul abandoning: %s on level %d, branch %d",
                 mi->name(DESC_PLAIN).c_str(),
                 static_cast<int>(you.your_level),
                 static_cast<int>(you.where_are_you));
#endif

            yred_make_enslaved_soul(*mi, true, true, true);

            success = true;
        }
        else if (is_yred_undead_slave(*mi))
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Undead abandoning: %s on level %d, branch %d",
                 mi->name(DESC_PLAIN).c_str(),
                 static_cast<int>(you.your_level),
                 static_cast<int>(you.where_are_you));
#endif

            mi->attitude = ATT_HOSTILE;
            behaviour_event(*mi, ME_ALERT, MHITYOU);
            // For now CREATED_FRIENDLY stays.

            success = true;
        }
    }

    return (success);
}

static bool _beogh_followers_on_level_abandon_you()
{
    bool success = false;

    // Note that orc high priests' summons are gifts of Beogh, so we
    // can't use is_orcish_follower() here.
    for (monster_iterator mi; mi; ++mi)
    {
        if (mons_is_god_gift(*mi, GOD_BEOGH))
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Orc abandoning: %s on level %d, branch %d",
                 mi->name(DESC_PLAIN).c_str(),
                 static_cast<int>(you.your_level),
                 static_cast<int>(you.where_are_you));
#endif

            mi->attitude = ATT_HOSTILE;
            behaviour_event(*mi, ME_ALERT, MHITYOU);
            // For now CREATED_FRIENDLY stays.

            success = true;
        }
    }

    return (success);
}

static bool _jiyva_slimes_on_level_abandon_you()
{
    bool success = false;

    for (monster_iterator mi; mi; ++mi)
    {
        if (is_fellow_slime(*mi))
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Slime abandoning: %s on level %d, branch %d",
                 mi->name(DESC_PLAIN).c_str(),
                 static_cast<int>(you.your_level),
                 static_cast<int>(you.where_are_you));
#endif

            mi->attitude = ATT_HOSTILE;
            behaviour_event(*mi, ME_ALERT, MHITYOU);
            // For now WAS_NEUTRAL stays.

            success = true;
        }
    }

    return (success);
}

// Upon excommunication, ex-Yredelemnulites lose all their undead
// slaves.  When under penance, Yredelemnulites can lose all undead
// slaves in sight.
bool yred_slaves_abandon_you()
{
    bool reclaim = false;
    int num_reclaim = 0;
    int num_slaves = 0;

    if (you.religion != GOD_YREDELEMNUL)
        reclaim = apply_to_all_dungeons(_yred_slaves_on_level_abandon_you);
    else
    {
        for (radius_iterator ri(you.pos(), 9); ri; ++ri)
        {
            monsters *monster = monster_at(*ri);
            if (monster == NULL)
                continue;

            if (_is_yred_enslaved_body_and_soul(monster)
                || is_yred_undead_slave(monster))
            {
                num_slaves++;

                const int hd = monster->hit_dice;

                // During penance, followers get a saving throw.
                if (random2((you.piety - you.penance[GOD_YREDELEMNUL]) / 18)
                    + random2(you.skills[SK_INVOCATIONS] - 6)
                    > random2(hd) + hd + random2(5))
                {
                    continue;
                }


                if (_is_yred_enslaved_body_and_soul(monster))
                    yred_make_enslaved_soul(monster, true, true, true);
                else
                {
                    monster->attitude = ATT_HOSTILE;
                    behaviour_event(monster, ME_ALERT, MHITYOU);
                    // For now CREATED_FRIENDLY stays.
                }

                num_reclaim++;

                reclaim = true;
            }
        }
    }

    if (reclaim)
    {
        if (you.religion != GOD_YREDELEMNUL)
        {
            simple_god_message(" reclaims all of your granted undead slaves!",
                               GOD_YREDELEMNUL);
        }
        else if (num_reclaim > 0)
        {
            if (num_reclaim == 1 && num_slaves > 1)
                simple_god_message(" reclaims one of your granted undead slaves!");
            else if (num_reclaim == num_slaves)
                simple_god_message(" reclaims your granted undead slaves!");
            else
                simple_god_message(" reclaims some of your granted undead slaves!");
        }

        return (true);
    }

    return (false);
}

static bool _fedhas_plants_on_level_hostile()
{
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->alive() && mons_is_plant(*mi))
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Plant hostility: %s on level %d, branch %d",
                 mi->name(DESC_PLAIN).c_str(),
                 static_cast<int>(you.your_level),
                 static_cast<int>(you.where_are_you));
#endif

            // You can potentially turn an oklob or whatever neutral
            // again by going back to Fedhas.
            if (testbits(mi->flags, MF_ATT_CHANGE_ATTEMPT))
                mi->flags &= ~MF_ATT_CHANGE_ATTEMPT;

            mi->attitude = ATT_HOSTILE;
            mi->del_ench(ENCH_CHARM, true);
            behaviour_event(*mi, ME_ALERT, MHITYOU);
            set_auto_exclude(*mi);
            // For now WAS_NEUTRAL stays.
        }
    }

    return (true);
}

bool fedhas_plants_hostile()
{
    if (apply_to_all_dungeons(_fedhas_plants_on_level_hostile))
    {
        mpr("The plants of the dungeon turn on you!", MSGCH_GOD);
        return (true);
    }

    return (false);
}

// Upon excommunication, ex-Beoghites lose all their orcish followers,
// plus all monsters created by their priestly orcish followers.  When
// under penance, Beoghites can lose all orcish followers in sight,
// subject to a few limitations.
bool beogh_followers_abandon_you()
{
    bool reconvert = false;
    int num_reconvert = 0;
    int num_followers = 0;

    if (you.religion != GOD_BEOGH)
    {
        reconvert =
            apply_to_all_dungeons(_beogh_followers_on_level_abandon_you);
    }
    else
    {
        for (radius_iterator ri(you.pos(), 9); ri; ++ri)
        {
            monsters *monster = monster_at(*ri);
            if (monster == NULL)
                continue;

            // Note that orc high priests' summons are gifts of Beogh,
            // so we can't use is_orcish_follower() here.
            if (mons_is_god_gift(monster, GOD_BEOGH))
            {
                num_followers++;

                if (you.visible_to(monster)
                    && !monster->asleep()
                    && !mons_is_confused(monster)
                    && !monster->cannot_act())
                {
                    const int hd = monster->hit_dice;

                    // During penance, followers get a saving throw.
                    if (random2((you.piety - you.penance[GOD_BEOGH]) / 18)
                        + random2(you.skills[SK_INVOCATIONS] - 6)
                        > random2(hd) + hd + random2(5))
                    {
                        continue;
                    }

                    monster->attitude = ATT_HOSTILE;
                    behaviour_event(monster, ME_ALERT, MHITYOU);
                    // For now CREATED_FRIENDLY stays.

                    if (you.can_see(monster))
                        num_reconvert++; // Only visible ones.

                    reconvert = true;
                }
            }
        }
    }

    if (reconvert) // Maybe all of them are invisible.
    {
        simple_god_message("'s voice booms out, \"Who do you think you "
                           "are?\"", GOD_BEOGH);

        std::ostream& chan = msg::streams(MSGCH_MONSTER_ENCHANT);

        if (you.religion != GOD_BEOGH)
            chan << "All of your followers decide to abandon you.";
        else if (num_reconvert > 0)
        {
            if (num_reconvert == 1 && num_followers > 1)
                chan << "One of your followers decides to abandon you.";
            else if (num_reconvert == num_followers)
                chan << "Your followers decide to abandon you.";
            else
                chan << "Some of your followers decide to abandon you.";
        }

        chan << std::endl;

        return (true);
    }

    return (false);
}

// Upon excommunication, ex-Jiyvaites lose all their fellow slimes.
bool jiyva_slimes_abandon_you()
{
    if (apply_to_all_dungeons(_jiyva_slimes_on_level_abandon_you))
    {
        std::ostream& chan = msg::streams(MSGCH_MONSTER_ENCHANT);

        chan << "All of your fellow slimes turn on you." << std::endl;

        return (true);
    }

    return (false);
}

static void _print_good_god_holy_being_speech(bool neutral,
                                              const std::string key,
                                              monsters *mon,
                                              msg_channel_type channel)
{
    std::string full_key = "good_god_";
    if (!neutral)
        full_key += "non";
    full_key += "neutral_holy_being_";
    full_key += key;

    std::string msg = getSpeakString(full_key);

    if (!msg.empty())
    {
        msg = do_mon_str_replacements(msg, mon);
        mpr(msg.c_str(), channel);
    }
}

// Holy monsters may turn good neutral when encountering followers of
// the good gods, and be made worshippers of TSO if necessary.
void good_god_holy_attitude_change(monsters *holy)
{
    ASSERT(holy->is_holy());

    if (you.can_see(holy)) // show reaction
    {
        _print_good_god_holy_being_speech(true, "reaction", holy,
                                          MSGCH_FRIEND_ENCHANT);

        if (!one_chance_in(3))
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
    behaviour_event(holy, ME_ALERT, MHITNOT);
}

void good_god_holy_fail_attitude_change(monsters *holy)
{
    ASSERT(holy->is_holy());

    if (you.can_see(holy)) // show reaction
    {
        _print_good_god_holy_being_speech(false, "reaction", holy,
                                          MSGCH_FRIEND_ENCHANT);

        if (!one_chance_in(3))
            _print_good_god_holy_being_speech(false, "speech", holy,
                                              MSGCH_TALK);
    }
}

static void _print_converted_orc_speech(const std::string key,
                                        monsters *mon,
                                        msg_channel_type channel)
{
    std::string msg = getSpeakString("beogh_converted_orc_" + key);

    if (!msg.empty())
    {
        msg = do_mon_str_replacements(msg, mon);
        mpr(msg.c_str(), channel);
    }
}

// Orcs may turn friendly when encountering followers of Beogh, and be
// made gifts of Beogh.
void beogh_convert_orc(monsters *orc, bool emergency,
                       bool converted_by_follower)
{
    ASSERT(mons_species(orc->type) == MONS_ORC);

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

    // Prevent assertion if the orc was previously worshipping a
    // different god, rather than already worshipping Beogh or being an
    // atheist.
    orc->god = GOD_NO_GOD;

    mons_make_god_gift(orc, GOD_BEOGH);

    if (orc->is_patrolling())
    {
        // Make orcs stop patrolling and forget their patrol point,
        // they're supposed to follow you now.
        orc->patrol_point = coord_def(0, 0);
    }

    if (!orc->alive())
        orc->hit_points = std::min(random_range(1, 4), orc->max_hit_points);

    // Avoid immobile "followers".
    behaviour_event(orc, ME_ALERT, MHITNOT);
}

void fedhas_neutralise_plant(monsters *plant)
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
}

void jiyva_convert_slime(monsters* slime)
{
    ASSERT(mons_is_slime(slime));

    if (you.can_see(slime))
    {
        if (mons_genus(slime->type) == MONS_GIANT_EYEBALL)
        {
            mprf(MSGCH_GOD, "%s stares at you suspiciously for a moment, "
                            "then relaxes.",

            slime->name(DESC_CAP_THE).c_str());
        }
        else
        {
            mprf(MSGCH_GOD, "%s trembles before you.",
                 slime->name(DESC_CAP_THE).c_str());
        }
    }

    slime->attitude = ATT_STRICT_NEUTRAL;
    slime->flags   |= MF_WAS_NEUTRAL;

    if (!mons_eats_items(slime))
    {
        slime->add_ench(ENCH_EAT_ITEMS);

        mprf(MSGCH_MONSTER_ENCHANT, "%s looks hungrier.",
             slime->name(DESC_CAP_THE).c_str());
    }

    // Prevent assertion if the slime was previously worshipping a
    // different god, rather than already worshipping Jiyva or being an
    // atheist.
    slime->god = GOD_NO_GOD;

    mons_make_god_gift(slime, GOD_JIYVA);
}
