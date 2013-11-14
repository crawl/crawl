/**
 * @file
 * @brief Contains monster death functionality, including Dowan and Duvessa,
 *        Kirke, Pikel and shedu.
**/

#include "AppHdr.h"
#include "mon-death.h"

#include "act-iter.h"
#include "areas.h"
#include "cloud.h"
#include "coordit.h"
#include "database.h"
#include "env.h"
#include "fineff.h"
#include "items.h"
#include "libutil.h"
#include "mapmark.h"
#include "message.h"
#include "mgen_data.h"
#include "mon-behv.h"
#include "mon-place.h"
#include "mon-speak.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "random.h"
#include "religion.h"
#include "state.h"
#include "transform.h"
#include "view.h"

/**
 * Determine if a specified monster is Pikel.
 *
 * Checks both the monster type and the "original_name" prop, thus allowing
 * Pikelness to be transferred through polymorph.
 *
 * @param mons    The monster to be checked.
 * @returns       True if the monster is Pikel, otherwise false.
**/
bool mons_is_pikel(monster* mons)
{
    return (mons->type == MONS_PIKEL
            || (mons->props.exists("original_name")
                && mons->props["original_name"].get_string() == "Pikel"));
}

/**
 * Perform neutralisation for members of Pikel's band upon Pikel's 'death'.
 *
 * This neutralisation occurs in multiple instances: when Pikel is neutralised,
 * enslaved, when Pikel dies, when Pikel is banished.
 * It is handled by a daction (as a fineff) to preserve across levels.
 **/
void pikel_band_neutralise()
{
    int visible_slaves = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->type == MONS_SLAVE
            && testbits(mi->flags, MF_BAND_MEMBER)
            && mi->props.exists("pikel_band")
            && mi->mname != "freed slave"
            && mi->observable())
        {
            visible_slaves++;
        }
    }
    string final_msg;
    if (visible_slaves == 1)
        final_msg = "With Pikel's spell broken, the former slave thanks you for freedom.";
    else if (visible_slaves > 1)
        final_msg = "With Pikel's spell broken, the former slaves thank you for their freedom.";

    (new delayed_action_fineff(DACT_PIKEL_SLAVES,final_msg))->schedule();
}

/**
 * Determine if a monster is Kirke.
 *
 * As with mons_is_pikel, tracks Kirke via type and original name, thus allowing
 * tracking through polymorph.
 *
 * @param mons    The monster to check.
 * @returns       True if Kirke, false otherwise.
**/
bool mons_is_kirke(monster* mons)
{
    return (mons->type == MONS_KIRKE
            || (mons->props.exists("original_name")
                && mons->props["original_name"].get_string() == "Kirke"));
}

/**
 * Revert porkalated hogs.
 *
 * Called upon Kirke's death. Hogs either from Kirke's band or
 * subsequently porkalated should be reverted to their original form.
 * This takes place as a daction to preserve behaviour across levels;
 * this function simply checks if any are visible and raises a fineff
 * containing an appropriate message. The fineff raises the actual
 * daction.
 */
void hogs_to_humans()
{
    int any = 0, human = 0;

    for (monster_iterator mi; mi; ++mi)
    {
        if (!(mi->type == MONS_HOG
              || mi->type == MONS_HELL_HOG
              || mi->type == MONS_HOLY_SWINE))
            continue;

        if (!mi->props.exists("kirke_band")
            && !mi->props.exists(ORIG_MONSTER_KEY))
            continue;

        // Shapeshifters will stop being a hog when they feel like it.
        if (mi->is_shapeshifter())
            continue;

        const bool could_see = you.can_see(*mi);

        if (could_see) any++;

        if (!mi->props.exists(ORIG_MONSTER_KEY) && could_see)
            human++;
    }

    string final_msg;

    if (any == 1)
    {
        if (any == human)
            final_msg = "No longer under Kirke's spell, the hog turns into a human!";
        else
            final_msg = "No longer under Kirke's spell, the hog returns to its "
                        "original form!";
    }
    else if (any > 1)
    {
        if (any == human)
            final_msg = "No longer under Kirke's spell, the hogs revert to their "
                        "human forms!";
        else
            final_msg = "No longer under Kirke's spell, the hogs revert to their "
                        "original forms!";
    }

    (new kirke_death_fineff(final_msg))->schedule();
}

/**
 * Determine if a monster is Dowan.
 *
 * Tracks through type and original_name, thus tracking through polymorph.
 *
 * @param mons    The monster to check.
 * @returns       True if Dowan, otherwise false.
**/
bool mons_is_dowan(const monster* mons)
{
    return (mons->type == MONS_DOWAN
            || (mons->props.exists("original_name")
                && mons->props["original_name"].get_string() == "Dowan"));
}

/**
 * Determine if a monster is Duvessa.
 *
 * Tracks through type and original_name, thus tracking through polymorph.
 *
 * @param mons    The monster to check.
 * @returns       True if Duvessa, otherwise false.
**/
bool mons_is_duvessa(const monster* mons)
{
    return (mons->type == MONS_DUVESSA
            || (mons->props.exists("original_name")
                && mons->props["original_name"].get_string() == "Duvessa"));
}

/**
 * Determine if a monster is either Dowan or Duvessa.
 *
 * Tracks through type and original_name, thus tracking through polymorph. A
 * wrapper around mons_is_dowan and mons_is_duvessa. Used to determine if a
 * death function should be called for the monster in question.
 *
 * @param mons    The monster to check.
 * @returns       True if either Dowan or Duvessa, otherwise false.
**/
bool mons_is_elven_twin(const monster* mons)
{
    return (mons_is_dowan(mons) || mons_is_duvessa(mons));
}

/**
 * Perform functional changes Dowan or Duvessa upon the other's death.
 *
 * This functional is called when either Dowan or Duvessa are killed or
 * banished. It performs a variety of changes in both attitude, spells, flavour,
 * speech, etc.
 *
 * @param twin          The monster who died.
 * @param in_transit    True if banished, otherwise false.
 * @param killer        The kill-type related to twin.
 * @param killer_index  The index of the actor who killed twin.
**/
void elven_twin_died(monster* twin, bool in_transit, killer_type killer, int killer_index)
{
    // Sometimes, if you pacify one twin near a staircase, they leave
    // in the same turn. Convert, in those instances.
    if (twin->neutral())
    {
        elven_twins_pacify(twin);
        return;
    }

    monster* mons = nullptr;

    for (monster_iterator mi; mi; ++mi)
    {
        if (*mi == twin)
            continue;

        // Don't consider already neutralised monsters.
        if ((*mi)->good_neutral())
            continue;

        if (mons_is_duvessa(*mi))
        {
            mons = *mi;
            break;
        }
        else if (mons_is_dowan(*mi))
        {
            mons = *mi;
            break;
        }
    }

    if (!mons)
        return;

    // Okay, let them climb stairs now.
    mons->props["can_climb"] = "yes";
    if (!in_transit)
        mons->props["speech_prefix"] = "twin_died";
    else
        mons->props["speech_prefix"] = "twin_banished";

    // If you've stabbed one of them, the other one is likely asleep still.
    if (mons->asleep())
        behaviour_event(mons, ME_DISTURB, 0, mons->pos());

    // Will generate strings such as 'Duvessa_Duvessa_dies' or, alternately
    // 'Dowan_Dowan_dies', but as neither will match, these can safely be
    // ignored.
    string key = mons->name(DESC_THE, true) + "_"
                 + twin->name(DESC_THE) + "_dies_";

    if (mons_near(mons) && !mons->observable())
        key += "invisible_";
    else if (!mons_near(mons))
        key += "distance_";

    bool i_killed = ((killer == KILL_MON || killer == KILL_MON_MISSILE)
                      && mons->mindex() == killer_index);

    if (i_killed)
    {
        key += "bytwin_";
        mons->props["speech_prefix"] = "twin_ikilled";
    }

    // Drop the final '_'.
    key.erase(key.length() - 1);

    string death_message = getSpeakString(key);

    // Check if they can speak or not: they may have been polymorphed.
    if (mons_near(mons) && !death_message.empty() && mons->can_speak())
        mons_speaks_msg(mons, death_message, MSGCH_TALK, silenced(you.pos()));
    else if (mons->can_speak())
        mprf("%s", death_message.c_str());

    if (mons_is_duvessa(mons))
    {
        if (mons_near(mons))
        {
            // Provides its own flavour message.
            mons->go_berserk(true);
        }
        else
        {
            // She'll go berserk the next time she sees you
            mons->props["duvessa_berserk"] = bool(true);
        }
    }
    else
    {
        ASSERT(mons_is_dowan(mons));
        if (mons->observable())
        {
            mons->add_ench(ENCH_HASTE);
            simple_monster_message(mons, " seems to find hidden reserves of power!");
        }
        else
            mons->props["dowan_upgrade"] = bool(true);

        mons->spells[0] = SPELL_THROW_ICICLE;
        mons->spells[1] = SPELL_BLINK;
        mons->spells[3] = SPELL_STONE_ARROW;
        mons->spells[4] = SPELL_HASTE;
        // Nothing with 6.
    }
}

/**
 * Pacification effects for Dowan and Duvessa.
 *
 * As twins, pacifying one pacifies the other.
 *
 * @param twin    The orignial monster pacified.
**/
void elven_twins_pacify(monster* twin)
{
    monster* mons = nullptr;

    for (monster_iterator mi; mi; ++mi)
    {
        if (*mi == twin)
            continue;

        // Don't consider already neutralised monsters.
        if ((*mi)->neutral())
            continue;

        if (mons_is_duvessa(*mi))
        {
            mons = *mi;
            break;
        }
        else if (mons_is_dowan(*mi))
        {
            mons = *mi;
            break;
        }
    }

    if (!mons)
        return;

    ASSERT(!mons->neutral());

    if (you_worship(GOD_ELYVILON))
        gain_piety(random2(mons->max_hit_points / (2 + you.piety / 20)), 2);

    if (mons_near(mons))
        simple_monster_message(mons, " likewise turns neutral.");

    record_monster_defeat(mons, KILL_PACIFIED);
    mons_pacify(mons, ATT_NEUTRAL);
}

/**
 * Unpacification effects for Dowan and Duvessa.
 *
 * If they are both pacified and you attack one, the other will not remain
 * neutral. This is both for flavour (they do things together), and
 * functionality (so Dowan does not begin beating on Duvessa, etc).
 *
 * @param twin    The monster attacked.
**/
void elven_twins_unpacify(monster* twin)
{
    monster* mons = nullptr;

    for (monster_iterator mi; mi; ++mi)
    {
        if (*mi == twin)
            continue;

        // Don't consider already neutralised monsters.
        if (!(*mi)->neutral())
            continue;

        if (mons_is_duvessa(*mi))
        {
            mons = *mi;
            break;
        }
        else if (mons_is_dowan(*mi))
        {
            mons = *mi;
            break;
        }
    }

    if (!mons)
        return;

    behaviour_event(mons, ME_WHACK, &you, you.pos(), false);
}

/**
 * Phoenix death effects
 *
 * When a phoenix dies, a short time later, its corpse will go up in flames and
 * the phoenix will be reborn. This is similar to a shedu being resurrected,
 * only it uses a level-specific marker.
**/

/**
 * Determine if a monster is a phoenix.
 *
 * Monsters that were previously phoenixes are not considered phoenixes for
 * phoenix resurrection purposes, I suppose.
 *
 * @param mons      The monster to check.
 * @returns         True if Phoenix, False otherwise.
**/
bool mons_is_phoenix(const monster* mons)
{
    return mons->type == MONS_PHOENIX;
}

/**
 * Perform phoenix death effect
 *
 * @param mons      The monster that just died.
**/
void phoenix_died(monster* mons)
{
    int durt = (random2(10) + random2(5) + 10) * BASELINE_DELAY;
    env.markers.add(new map_phoenix_marker(mons->pos(),
                        durt,
                        static_cast<int>(mons->mid),
                        mons->behaviour,
                        mons->attitude,
                        mons->deity(),
                        mons->pos()));
    env.markers.clear_need_activate();
}

/**
 * Get vector of phoenix markers
 *
 * @returns     Vector of map_phoenix_markers.
**/
static vector<map_phoenix_marker*> get_phoenix_markers()
{
    vector<map_phoenix_marker*> mm_markers;

    vector<map_marker*> markers = env.markers.get_all(MAT_PHOENIX);
    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        map_marker *mark = markers[i];
        if (mark->get_type() != MAT_PHOENIX)
            continue;

        map_phoenix_marker *mmark = dynamic_cast<map_phoenix_marker*>(mark);

        mm_markers.push_back(mmark);
    }

    return mm_markers;
}

/**
 * Timeout phoenix markers
 *
 * @param duration      Duration.
**/
void timeout_phoenix_markers(int duration)
{
    vector<map_phoenix_marker*> markers = get_phoenix_markers();

    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        map_phoenix_marker *mmark = markers[i];

        if (duration)
            mmark->duration -= duration;

        if (mmark->duration < 0)
        {
            // Now, look for the corpse
            bool found_body = false;
            coord_def place_at;
            bool from_inventory = false;

            for (radius_iterator ri(mmark->corpse_pos, LOS_RADIUS, C_ROUND, false); ri; ++ri)
            {
                for (stack_iterator si(*ri); si; ++si)
                    if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY
                        && si->props.exists(MONSTER_MID)
                        && si->props[MONSTER_MID].get_int() == mmark->mon_num)
                    {
                        place_at = *ri;
                        destroy_item(si->index());
                        found_body = true;
                        break;
                    }
            }

            if (!found_body)
            {
                for (unsigned slot = 0; slot < ENDOFPACK; ++slot)
                {
                    if (!you.inv[slot].defined())
                        continue;

                    item_def* si = &you.inv[slot];
                    if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY
                        && si->props.exists(MONSTER_MID)
                        && si->props[MONSTER_MID].get_int() == mmark->mon_num)
                    {
                        // it was in the player's inventory
                        place_at = coord_def(-1, -1);
                        dec_inv_item_quantity(slot, 1, false);
                        found_body = true;
                        from_inventory = true;
                        break;
                    }
                }

                if (found_body)
                    place_at = you.pos();
            }

            if (!found_body || place_at.origin())
            {
                // Actually time-out this marker; we didn't find a body, too bad.
                env.markers.remove(mmark);
                continue;
            }

            // Okay, we have a corpse, which we've destroyed. We'll place a cloud!
            mgen_data new_pho;
            new_pho.cls = MONS_PHOENIX;
            new_pho.behaviour = mmark->behaviour;
            new_pho.god = mmark->god;

            monster* mons = 0;

            for (distance_iterator di(place_at, true, false); di; ++di)
            {
                if (monster_at(*di) || !monster_habitable_grid(MONS_PHOENIX, grd(*di)))
                    continue;

                new_pho.pos = *di;
                if (mons = place_monster(new_pho, true))
                    break;
            }

            // give up
            if (!mons)
            {
                dprf("Couldn't place new phoenix!");
                // We couldn't place it, so nuke the marker.
                env.markers.remove(mmark);
                continue;
            }

            mons->attitude = mmark->attitude;

            // We no longer need the marker now, so free it.
            env.markers.remove(mmark);

            if (from_inventory)
                simple_monster_message(mons, " is reborn from your pack in a blaze of fire!");
            else if (you.can_see(mons))
                simple_monster_message(mons, " is reborn in a blaze of fire!");

            // Now, place a cloud to compensate
            place_cloud(CLOUD_FIRE, place_at, 4+random2(10), NULL);
        }
    }
}

/**
 * Determine a shedu's pair by index.
 *
 * The index of a shedu's pair is stored as mons->number. This function attempts
 * to return a pointer to that monster. If that monster doesn't exist, or is
 * dead, returns NULL.
 *
 * @param mons    The monster whose pair we're searching for. Assumed to be a
 *                 shedu.
 * @returns        Either a monster* or NULL if a monster was not found.
**/
monster* get_shedu_pair(const monster* mons)
{
    monster* pair = monster_by_mid(mons->number);
    if (pair)
        return pair;

    return NULL;
}

/**
 * Determine if a shedu's pair is alive.
 *
 * A simple function that checks the return value of get_shedu_pair is not null.
 *
 * @param mons    The monster whose pair we are searching for.
 * @returns        True if the pair is alive, False otherwise.
**/
bool shedu_pair_alive(const monster* mons)
{
    if (get_shedu_pair(mons) == NULL)
        return false;

    return true;
}

/**
 * Determine if a monster is or was a shedu.
 *
 * @param mons    The monster to check.
 * @returns        Either True if the monster is or was a shedu, otherwise
 *                 False.
**/
bool mons_is_shedu(const monster* mons)
{
    return (mons->type == MONS_SHEDU
            || (mons->props.exists("original_name")
                && mons->props["original_name"].get_string() == "shedu"));
}

/**
 * Initial resurrection functionality for Shedu.
 *
 * This function is called when a shedu dies. It attempts to find that shedu's
 * pair, wakes them if necessary, and then begins the resurrection process by
 * giving them the ENCH_PREPARING_RESURRECT enchantment timer. If a pair does
 * not exist (i.e., this is the second shedu to have died), nothing happens.
 *
 * @param mons    The shedu who died.
**/
void shedu_do_resurrection(const monster* mons)
{
    if (!mons_is_shedu(mons))
        return;

    if (mons->number == 0)
        return;

    monster* my_pair = get_shedu_pair(mons);
    if (!my_pair)
        return;

    // Wake the other one up if it's asleep.
    if (my_pair->asleep())
        behaviour_event(my_pair, ME_DISTURB, 0, my_pair->pos());

    if (you.can_see(my_pair))
        simple_monster_message(my_pair, " ceases action and prepares to resurrect its fallen mate.");

    my_pair->add_ench(ENCH_PREPARING_RESURRECT);
}

/**
 * Perform resurrection of a shedu.
 *
 * This function is called when the ENCH_PREPARING_RESURRECT timer runs out. It
 * determines if there is a viable corpse (of which there will always be one),
 * where that corpse is (if it is not in line of sight, no resurrection occurs;
 * if it is in your pack, it is resurrected "from" your pack, etc), and then
 * perform the actual resurrection by creating a new shedu monster.
 *
 * @param mons    The shedu who is to perform the resurrection.
**/
void shedu_do_actual_resurrection(monster* mons)
{
    // Here is where we actually recreate the dead
    // shedu.
    bool found_body = false;
    coord_def place_at;
    bool from_inventory = false;

    // Our pair might already be irretrievably dead.
    if (mons->number == 0)
        return;

    for (radius_iterator ri(mons->pos(), LOS_NO_TRANS); ri; ++ri)
    {
        for (stack_iterator si(*ri); si; ++si)
            if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY
                && si->props.exists(MONSTER_MID)
                && static_cast<unsigned int>(si->props[MONSTER_MID].get_int()) == mons->number)
            {
                place_at = *ri;
                destroy_item(si->index());
                found_body = true;
                break;
            }
    }

    if (!found_body)
    {
        for (unsigned slot = 0; slot < ENDOFPACK; ++slot)
        {
            if (!you.inv[slot].defined())
                continue;

            item_def* si = &you.inv[slot];
            if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY
                && si->props.exists(MONSTER_MID)
                && static_cast<unsigned int>(si->props[MONSTER_MID].get_int()) == mons->number)
            {
                // it was in the player's inventory
                place_at = coord_def(-1, -1);
                dec_inv_item_quantity(slot, 1, false);
                found_body = true;
                from_inventory = true;
                break;
            }
        }

        if (found_body)
            place_at = you.pos();
    }

    if (!found_body)
    {
        mons->number = 0;
        return;
    }

    mgen_data new_shedu;
    new_shedu.cls = MONS_SHEDU;
    new_shedu.behaviour = mons->behaviour;
    ASSERT(!place_at.origin());
    new_shedu.foe = mons->foe;
    new_shedu.god = mons->god;

    monster* my_pair = 0;
    for (distance_iterator di(place_at, true, false); di; ++di)
    {
        if (monster_at(*di) || !monster_habitable_grid(mons, grd(*di)))
            continue;

        new_shedu.pos = *di;
        if (my_pair = place_monster(new_shedu, true))
            break;
    }

    // give up
    if (!my_pair)
    {
        dprf("Couldn't place new shedu!");
        return;
    }

    my_pair->number = mons->mid;
    mons->number = my_pair->mid;
    my_pair->flags |= MF_BAND_MEMBER;

    if (from_inventory)
        simple_monster_message(mons, " resurrects its mate from your pack!");
    else if (you.can_see(mons))
        simple_monster_message(mons, " resurrects its mate from the grave!");
    else if (you.can_see(my_pair))
        simple_monster_message(mons, " rises from the grave!");
}
