/**
 * @file
 * @brief Potion and potion-like effects.
**/

#include "AppHdr.h"

#include "potion.h"

#include <stdio.h>
#include <string.h>

#include "externs.h"

#include "areas.h"
#include "artefact.h"
#include "beam.h"
#include "env.h"
#include "food.h"
#include "godconduct.h"
#include "hints.h"
#include "item_use.h"
#include "libutil.h"
#include "message.h"
#include "misc.h"
#include "mutation.h"
#include "player.h"
#include "player-stats.h"
#include "skill_menu.h"
#include "spl-miscast.h"
#include "terrain.h"
#include "transform.h"
#include "xom.h"

/*
 * Apply the effect of a potion to the player.
 *
 * This is called when the player quaff a potion, but also for some cards,
 * beams, sparkling fountains, god effects and miscasts.
 *
 * @param pot_eff       The potion type.
 * @param pow           The power of the effect. 40 for actual potions.
 * @param drank_it      Whether the player actually quaffed (potions and fountains).
 * @param was_known     Whether the potion was already identified.
 * @param from_fountain Is this from a fountain?
 *
 * @return If the potion was identified.
 */
bool potion_effect(potion_type pot_eff, int pow, bool drank_it, bool was_known,
                   bool from_fountain)
{
    bool effect = true;  // current behaviour is all potions id on quaffing

    pow = min(pow, 150);

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
    case POT_CURING:
        if (you.duration[DUR_DEATHS_DOOR])
        {
            mpr("You feel queasy.");
            break;
        }

        inc_hp((5 + random2(7)) / factor);
        mpr("You feel better.");

        // Only fix rot when healed to full.
        if (you.hp == you.hp_max)
        {
            unrot_hp(1);
            set_hp(you.hp_max);
        }

        you.duration[DUR_POISONING] = 0;
        you.rotting = 0;
        you.disease = 0;
        you.duration[DUR_CONF] = 0;
        break;

    case POT_HEAL_WOUNDS:
        if (you.duration[DUR_DEATHS_DOOR])
        {
            mpr("You feel queasy.");
            break;
        }

        inc_hp((10 + random2avg(28, 3)) / factor);
        mpr("You feel much better.");

        // only fix rot when healed to full
        if (you.hp == you.hp_max)
        {
            unrot_hp((2 + random2avg(5, 2)) / factor);
            set_hp(you.hp_max);
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
            }
            else
            {
                mpr("Yuck - this tastes like blood.");
                if (x_chance_in_y(herbivorous + 1, 4))
                {
                    // Full herbivores always become ill from blood.
                    you.sicken(50 + random2(100));
                    xom_is_stimulated(25 / xom_factor);
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

        you.increase_duration(DUR_MIGHT, (35 + random2(pow)) / factor, 80);

        if (!were_mighty)
            notify_stat_change(STAT_STR, 5, true, "");
        break;
    }

    case POT_BRILLIANCE:
    {
        const bool were_brilliant = you.duration[DUR_BRILLIANCE] > 0;

        mprf(MSGCH_DURATION, "You feel %s all of a sudden.",
             were_brilliant ? "clever" : "more clever");

        you.increase_duration(DUR_BRILLIANCE,
                              (35 + random2(pow)) / factor, 80);

        if (!were_brilliant)
            notify_stat_change(STAT_INT, 5, true, "");
        break;
    }

    case POT_AGILITY:
    {
        const bool were_agile = you.duration[DUR_AGILITY] > 0;

        mprf(MSGCH_DURATION, "You feel %s all of a sudden.",
             were_agile ? "agile" : "more agile");

        you.increase_duration(DUR_AGILITY, (35 + random2(pow)) / factor, 80);

        if (!were_agile)
            notify_stat_change(STAT_DEX, 5, true, "");
        break;
    }

#if TAG_MAJOR_VERSION == 34
    case POT_GAIN_STRENGTH:
        if (mutate(MUT_STRONG, "potion of gain strength", true, false, false, true))
            learned_something_new(HINT_YOU_MUTATED);
        break;

    case POT_GAIN_DEXTERITY:
        if (mutate(MUT_AGILE, "potion of gain dexterity", true, false, false, true))
            learned_something_new(HINT_YOU_MUTATED);
        break;

    case POT_GAIN_INTELLIGENCE:
        if (mutate(MUT_CLEVER, "potion of gain intelligence", true, false, false, true))
            learned_something_new(HINT_YOU_MUTATED);
        break;
#endif

    case POT_FLIGHT:
        if (!flight_allowed())
            break;

        you.attribute[ATTR_FLIGHT_UNCANCELLABLE] = 1;
        fly_player(pow);
        break;

    case POT_POISON:
    case POT_STRONG_POISON:
        if (player_res_poison() >= (pot_eff == POT_POISON ? 1 : 3))
        {
            mprf("You feel %s nauseous.",
                 (pot_eff == POT_POISON) ? "slightly" : "quite");
            maybe_id_resist(BEAM_POISON);
        }
        else
        {
            mprf(MSGCH_WARN,
                 "That liquid tasted %s nasty...",
                 (pot_eff == POT_POISON) ? "very" : "extremely");

            int amount;
            string msg;
            if (pot_eff == POT_POISON)
            {
                amount = 1 + random2avg(5, 2);
                msg = "a potion of poison";
            }
            else
            {
                amount = 3 + random2avg(13, 2);
                msg = "a potion of strong poison";
            }
            if (from_fountain)
                msg = "a sparkling fountain";
            poison_player(amount, "", msg);
            xom_is_stimulated(100 / xom_factor);
        }
        break;

    case POT_SLOWING:
        if (slow_player((10 + random2(pow)) / factor))
            xom_is_stimulated(50 / xom_factor);
        break;

    case POT_PARALYSIS:
        paralyse_player("a potion of paralysis", 0, factor);
        xom_is_stimulated(50 / xom_factor);
        break;

    case POT_CONFUSION:
        if (confuse_player((3 + random2(8)) / factor))
            xom_is_stimulated(100 / xom_factor);
        break;

    case POT_INVISIBILITY:
        if (you.haloed() || you.glows_naturally())
        {
            // You can't turn invisible while haloed or glowing
            // naturally, but identify the effect anyway.
            mpr("You briefly turn translucent.");
            return true;
        }
        else if (you.backlit())
        {
            vector<string> afflictions;
            if (get_contamination_level() > 1)
                afflictions.push_back("magical contamination");
            if (you.duration[DUR_CORONA])
                afflictions.push_back("corona");
            if (you.duration[DUR_LIQUID_FLAMES])
                afflictions.push_back("liquid flames");

            mprf(MSGCH_DURATION,
                 "You become %stransparent, but the glow from your "
                 "%s prevents you from becoming "
                 "completely invisible.",
                 you.duration[DUR_INVIS] ? "more " : "",
                 comma_separated_line(afflictions.begin(), afflictions.end()).c_str());
        }
        else
        {
            mpr(!you.duration[DUR_INVIS] ? "You fade into invisibility!"
                                         : "You fade further into invisibility.",
                MSGCH_DURATION);
        }

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
            xom_is_stimulated(50 / xom_factor);
        }
        break;

    // Don't generate randomly - should be rare and interesting.
    case POT_DECAY:
        if (you.rot(&you, 0, (3 + random2(3)) / factor))
            xom_is_stimulated(50 / xom_factor);
        break;

#if TAG_MAJOR_VERSION == 34
    case POT_WATER:
    case POT_FIZZING:
#endif
    case NUM_POTIONS:
        if (you.species == SP_VAMPIRE)
            mpr("Blech - this tastes like water.");
        else
            mpr("This tastes like water.");
        break;

    case POT_EXPERIENCE:
        if (you.experience_level < 27)
        {
            mpr("You feel more experienced!");
            adjust_level(1, true);

            // Deferred calling level_change() into item_use.cc:3919, after
            // dec_inv_item_quantity. This prevents using SIGHUP to get infinite
            // potions of experience. Confer Mantis #3245. [due]
        }
        else
            mpr("A flood of memories washes over you.");
        more();
        skill_menu(SKMF_EXPERIENCE_POTION, 750 * you.experience_level);
        break;

    case POT_MAGIC:
        // Allow repairing rot, disallow going through Death's Door.
        if (you.species == SP_DJINNI)
            return potion_effect(POT_HEAL_WOUNDS, pow, drank_it, was_known);

        inc_mp(10 + random2avg(28, 3));
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
            mpr("You feel refreshed.");
        break;
    }

    case POT_BERSERK_RAGE:
        if (you.species == SP_VAMPIRE && you.hunger_state <= HS_SATIATED)
        {
            mpr("You feel slightly irritated.");
            make_hungry(100, false);
        }
        else
        {
            if (go_berserk(was_known, true))
                xom_is_stimulated(50);
        }
        break;

    case POT_CURE_MUTATION:
        mpr("It has a very clean taste.");
        for (int i = 0; i < 7; i++)
            if (random2(9) >= i)
                delete_mutation(RANDOM_MUTATION, "potion of cure mutation", false);
        break;

    case POT_MUTATION:
        mpr("You feel extremely strange.");
        for (int i = 0; i < 3; i++)
            mutate(RANDOM_MUTATION, "potion of mutation", false);

        learned_something_new(HINT_YOU_MUTATED);
        did_god_conduct(DID_DELIBERATE_MUTATING, 10, was_known);
        break;

    case POT_BENEFICIAL_MUTATION:
        if (undead_mutation_rot(true))
        {
            mpr("You feel dead inside.");
            mutate(RANDOM_GOOD_MUTATION, "potion of beneficial mutation",
                true, false, false, true);
            break;
        }

        if (mutate(RANDOM_GOOD_MUTATION, "potion of beneficial mutation",
               true, false, false, true))
        {
            mpr("You feel fantastic!");
            did_god_conduct(DID_DELIBERATE_MUTATING, 10, was_known);
        }
        else
            mpr("You feel fantastic for a moment.");
        learned_something_new(HINT_YOU_MUTATED);
        break;


    case POT_RESISTANCE:
        mpr("You feel protected.", MSGCH_DURATION);
        you.increase_duration(DUR_RESISTANCE, (random2(pow) + 35) / factor);
        break;
    }

    return (!was_known && effect);
}
