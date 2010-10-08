/*
 *  File:       it_use2.cc
 *  Summary:    Functions for using wands, potions, and weapon/armour removal.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "it_use2.h"

#include <stdio.h>
#include <string.h>

#include "externs.h"

#include "artefact.h"
#include "beam.h"
#include "effects.h"
#include "env.h"
#include "food.h"
#include "godconduct.h"
#include "hints.h"
#include "item_use.h"
#include "itemprop.h"
#include "misc.h"
#include "mutation.h"
#include "player.h"
#include "player-equip.h"
#include "player-stats.h"
#include "spl-miscast.h"
#include "stuff.h"
#include "terrain.h"
#include "transform.h"
#include "xom.h"

// From an actual potion, pow == 40 -- bwr
bool potion_effect(potion_type pot_eff, int pow, bool drank_it, bool was_known)
{

    bool effect = true;  // current behaviour is all potions id on quaffing

    pow = std::min(pow, 150);

    int factor = (you.species == SP_VAMPIRE
                  && you.hunger_state < HS_SATIATED
                  && drank_it ? 2 : 1);

    // Knowingly drinking bad potions is much less amusing.
    int xom_factor = factor;
    if (drank_it && was_known)
    {
        xom_factor *= 2;
        if (!player_in_a_dangerous_place())
            xom_factor *= 3;
    }

    switch (pot_eff)
    {
    case POT_HEALING:
        if (you.duration[DUR_DEATHS_DOOR])
        {
            mpr("You feel queasy.");
            break;
        }

        inc_hp((5 + random2(7)) / factor, false);
        mpr("You feel better.");

        // Only fix rot when healed to full.
        if (you.hp == you.hp_max)
        {
            unrot_hp(1);
            set_hp(you.hp_max, false);
        }

        you.duration[DUR_POISONING] = 0;
        you.rotting = 0;
        you.disease = 0;
        you.duration[DUR_CONF] = 0;
        you.duration[DUR_MISLED] = 0;
        break;

    case POT_HEAL_WOUNDS:
        if (you.duration[DUR_DEATHS_DOOR])
        {
            mpr("You feel queasy.");
            break;
        }

        inc_hp((10 + random2avg(28, 3)) / factor, false);
        mpr("You feel much better.");

        // only fix rot when healed to full
        if (you.hp == you.hp_max)
        {
            unrot_hp((2 + random2avg(5, 2)) / factor);
            set_hp(you.hp_max, false);
        }
        break;

      case POT_BLOOD:
      case POT_BLOOD_COAGULATED:
        if (you.species == SP_VAMPIRE)
        {
            // No healing anymore! (jpeg)
            int value = 800;
            if (pot_eff == POT_BLOOD)
            {
                mpr("Yummy - fresh blood!");
                value += 200;
            }
            else // Coagulated.
                mpr("This tastes delicious!");

            lessen_hunger(value, true);
        }
        else
        {
            const int value = 200;
            const int herbivorous = player_mutation_level(MUT_HERBIVOROUS);

            if (herbivorous < 3 && player_likes_chunks())
            {
                // Likes it.
                mpr("This tastes like blood.");
                lessen_hunger(value, true);

                if (!player_likes_chunks(true))
                    check_amu_the_gourmand(false);
            }
            else
            {
                mpr("Yuck - this tastes like blood.");
                if (x_chance_in_y(herbivorous + 1, 4))
                {
                    // Full herbivores always become ill from blood.
                    you.sicken(50 + random2(100));
                    xom_is_stimulated(32 / xom_factor);
                }
                else
                    lessen_hunger(value, true);
            }
        }
        did_god_conduct(DID_DRINK_BLOOD, 1 + random2(3), was_known);
        break;

    case POT_SPEED:
        if (haste_player((40 + random2(pow)) / factor))
            did_god_conduct(DID_HASTY, 10, was_known);
        break;

    case POT_MIGHT:
    {
        const bool were_mighty = you.duration[DUR_MIGHT] > 0;

        mprf(MSGCH_DURATION, "You feel %s all of a sudden.",
             were_mighty ? "mightier" : "very mighty");

        // conceivable max gain of +184 {dlb}
        you.increase_duration(DUR_MIGHT, (35 + random2(pow)) / factor, 80);

        if (were_mighty)
            contaminate_player(1, was_known);
        else
            notify_stat_change(STAT_STR, 5, true, "");

        did_god_conduct(DID_STIMULANTS, 4 + random2(4), was_known);
        break;
    }

    case POT_BRILLIANCE:
    {
        const bool were_brilliant = you.duration[DUR_BRILLIANCE] > 0;

        mprf(MSGCH_DURATION, "You feel %s all of a sudden.",
             were_brilliant ? "clever" : "more clever");

        you.increase_duration(DUR_BRILLIANCE,
                              (35 + random2(pow)) / factor, 80);

        if (were_brilliant)
            contaminate_player(1, was_known);
        else
            notify_stat_change(STAT_INT, 5, true, "");

        did_god_conduct(DID_STIMULANTS, 4 + random2(4), was_known);
        break;
    }

    case POT_AGILITY:
    {
        const bool were_agile = you.duration[DUR_AGILITY] > 0;

        mprf(MSGCH_DURATION, "You feel %s all of a sudden.",
             were_agile ? "agile" : "more agile");

        you.increase_duration(DUR_AGILITY, (35 + random2(pow)) / factor, 80);

        if (were_agile)
            contaminate_player(1, was_known);
        else
            notify_stat_change(STAT_DEX, 5, true, "");

        did_god_conduct(DID_STIMULANTS, 4 + random2(4), was_known);
        break;
    }

    case POT_GAIN_STRENGTH:
        if (mutate(MUT_STRONG, true, false, false, true))
            learned_something_new(HINT_YOU_MUTATED);
        break;

    case POT_GAIN_DEXTERITY:
        if (mutate(MUT_AGILE, true, false, false, true))
            learned_something_new(HINT_YOU_MUTATED);
        break;

    case POT_GAIN_INTELLIGENCE:
        if (mutate(MUT_CLEVER, true, false, false, true))
            learned_something_new(HINT_YOU_MUTATED);
        break;

    case POT_LEVITATION:
        you.attribute[ATTR_LEV_UNCANCELLABLE] = 1;
        levitate_player(pow);
        break;

    case POT_POISON:
    case POT_STRONG_POISON:
        if (player_res_poison())
        {
            mprf("You feel %s nauseous.",
                 (pot_eff == POT_POISON) ? "slightly" : "quite" );
        }
        else
        {
            mprf(MSGCH_WARN,
                 "That liquid tasted %s nasty...",
                 (pot_eff == POT_POISON) ? "very" : "extremely" );

            if (pot_eff == POT_POISON)
                poison_player(1 + random2avg(5, 2), "", "a potion of poison");
            else
                poison_player(3 + random2avg(13, 2), "", "a potion of strong poison");
            xom_is_stimulated(128 / xom_factor);
        }
        break;

    case POT_SLOWING:
        if (slow_player((10 + random2(pow)) / factor))
            xom_is_stimulated(64 / xom_factor);
        break;

    case POT_PARALYSIS:
        you.paralyse(NULL,
                     (2 + random2( 6 + you.duration[DUR_PARALYSIS]
                                       / BASELINE_DELAY )) / factor);
        xom_is_stimulated(64 / xom_factor);
        break;

    case POT_CONFUSION:
        if (confuse_player((3 + random2(8)) / factor))
            xom_is_stimulated(128 / xom_factor);
        break;

    case POT_INVISIBILITY:
        if (you.haloed())
        {
            // You can't turn invisible while haloed, but identify the
            // effect anyway.
            mpr("You briefly turn translucent.");

            // And also cancel backlight (for whatever good that will
            // do).
            you.duration[DUR_CORONA] = 0;
            return (true);
        }

        if (get_contamination_level() > 1)
        {
            mprf(MSGCH_DURATION,
                 "You become %stransparent, but the glow from your "
                 "magical contamination prevents you from becoming "
                 "completely invisible.",
                 you.duration[DUR_INVIS] ? "further " : "");
        }
        else
        {
            mpr(!you.duration[DUR_INVIS] ? "You fade into invisibility!"
                                       : "You fade further into invisibility.",
            MSGCH_DURATION);
        }

        // Invisibility cancels backlight.
        you.duration[DUR_CORONA] = 0;

        // Now multiple invisiblity casts aren't as good. -- bwr
        if (!you.duration[DUR_INVIS])
            you.set_duration(DUR_INVIS, 15 + random2(pow), 100);
        else
            you.increase_duration(DUR_INVIS, random2(pow), 100);

        if (drank_it)
            you.attribute[ATTR_INVIS_UNCANCELLABLE] = 1;

        break;

    case POT_PORRIDGE:          // oatmeal - always gluggy white/grey?
        if (you.species == SP_VAMPIRE
            || player_mutation_level(MUT_CARNIVOROUS) == 3)
        {
            mpr("Blech - that potion was really gluggy!");
        }
        else
        {
            mpr("That potion was really gluggy!");
            lessen_hunger(6000, true);
        }
        break;

    case POT_DEGENERATION:
        if (drank_it)
            mpr("There was something very wrong with that liquid!");

        if (lose_stat(STAT_RANDOM, (1 + random2avg(4, 2)) / factor, false,
                      "drinking a potion of degeneration"))
        {
            xom_is_stimulated(64 / xom_factor);
        }
        break;

    // Don't generate randomly - should be rare and interesting.
    case POT_DECAY:
        if (you.rot(&you, (10 + random2(10)) / factor))
            xom_is_stimulated(64 / xom_factor);
        break;

    case POT_WATER:
        if (you.species == SP_VAMPIRE)
            mpr("Blech - this tastes like water.");
        else
            mpr("This tastes like water.");
        break;

    case POT_EXPERIENCE:
        if (you.experience_level < 27)
        {
            mpr("You feel more experienced!");

            you.experience = 1 + exp_needed(2 + you.experience_level);
            level_change();
        }
        else
            mpr("A flood of memories washes over you.");
        break;                  // I'll let this slip past robe of archmagi

    case POT_MAGIC:
        inc_mp((10 + random2avg(28, 3)), false);
        mpr("Magic courses through your body.");
        break;

    case POT_RESTORE_ABILITIES:
    {
        bool nothing_happens = true;
        if (you.duration[DUR_BREATH_WEAPON])
        {
            mpr("You have got your breath back.", MSGCH_RECOVERY);
            you.duration[DUR_BREATH_WEAPON] = 0;
            nothing_happens = false;
        }

        // Give a message if no message otherwise.
        if (!restore_stat(STAT_ALL, 0, false) && nothing_happens)
            mpr( "You feel refreshed." );
        break;
    }

    case POT_BERSERK_RAGE:
        if (you.species == SP_VAMPIRE && you.hunger_state <= HS_SATIATED)
        {
            mpr("You feel slightly irritated.");
            make_hungry(100, false);
        }
        else if (you.duration[DUR_BUILDING_RAGE])
        {
            you.duration[DUR_BUILDING_RAGE] = 0;
            mpr("Your blood cools.");
        }
        else
        {
            if (go_berserk(was_known, true))
                xom_is_stimulated(64);
        }
        break;

    case POT_CURE_MUTATION:
        mpr("It has a very clean taste.");
        for (int i = 0; i < 7; i++)
            if (random2(10) > i)
                delete_mutation(RANDOM_MUTATION, false);
        break;

    case POT_MUTATION:
        mpr("You feel extremely strange.");
        for (int i = 0; i < 3; i++)
            mutate(RANDOM_MUTATION, false);

        learned_something_new(HINT_YOU_MUTATED);
        did_god_conduct(DID_DELIBERATE_MUTATING, 10, was_known);
        did_god_conduct(DID_STIMULANTS, 4 + random2(4), was_known);
        break;

    case POT_RESISTANCE:
        mpr("You feel protected.", MSGCH_DURATION);
        you.increase_duration(DUR_RESIST_FIRE,   (random2(pow) + 35) / factor);
        you.increase_duration(DUR_RESIST_COLD,   (random2(pow) + 35) / factor);
        you.increase_duration(DUR_RESIST_POISON, (random2(pow) + 35) / factor);
        you.increase_duration(DUR_INSULATION,    (random2(pow) + 35) / factor);

        // Just one point of contamination. These potions are really rare,
        // and contamination is nastier.
        contaminate_player(1, was_known);
        break;

    case NUM_POTIONS:
        mpr("You feel bugginess flow through your body.");
        break;
    }

    return (effect);
}

bool unwield_item(bool showMsgs)
{
    if (!you.weapon())
        return (false);

    if (you.berserk())
    {
        if (showMsgs)
            canned_msg(MSG_TOO_BERSERK);
        return (false);
    }

    item_def& item = *you.weapon();

    const bool is_weapon = get_item_slot(item) == EQ_WEAPON;

    if (is_weapon && !safe_to_remove(item))
        return (false);

    unequip_item(EQ_WEAPON, showMsgs);

    you.wield_change     = true;
    you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = 0;

    return (true);
}
