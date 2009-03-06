/*
 *  File:       it_use2.cc
 *  Summary:    Functions for using wands, potions, and weapon/armour removal.4\3
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "it_use2.h"

#include <stdio.h>
#include <string.h>

#include "externs.h"

#include "beam.h"
#include "effects.h"
#include "food.h"
#include "item_use.h"
#include "itemname.h"
#include "itemprop.h"
#include "misc.h"
#include "mutation.h"
#include "player.h"
#include "quiver.h"
#include "randart.h"
#include "religion.h"
#include "skills2.h"
#include "spells2.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "stuff.h"
#include "tutorial.h"
#include "view.h"
#include "xom.h"

// From an actual potion, pow == 40 -- bwr
bool potion_effect(potion_type pot_eff, int pow, bool drank_it, bool was_known)
{
    bool effect = true;  // current behaviour is all potions id on quaffing

    pow = std::min(pow, 150);

    const int factor = (you.species == SP_VAMPIRE
                        && you.hunger_state < HS_SATIATED
                        && drank_it ? 2 : 1);

    switch (pot_eff)
    {
    case POT_HEALING:

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
        break;

    case POT_HEAL_WOUNDS:
        inc_hp((10 + random2avg(28, 3)) / factor, false);
        mpr("You feel much better.");

        // only fix rot when healed to full
        if (you.hp == you.hp_max)
        {
            unrot_hp( (2 + random2avg(5, 2)) / factor );
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
                mpr("Blech - this tastes like blood!");
                if (x_chance_in_y(herbivorous + 1, 4))
                {
                    // Full herbivores always become ill from blood.
                    disease_player(50 + random2(100));
                    xom_is_stimulated(32);
                }
                else
                    lessen_hunger(value, true);
            }
        }
        did_god_conduct(DID_DRINK_BLOOD, 1 + random2(3), was_known);
        break;

    case POT_SPEED:
        haste_player( (40 + random2(pow)) / factor );
        break;

    case POT_MIGHT:
    {
        const bool were_mighty = you.duration[DUR_MIGHT] > 0;

        mprf(MSGCH_DURATION, "You feel %s all of a sudden.",
             were_mighty ? "mightier" : "very mighty");

        if (were_mighty)
            contaminate_player(1, was_known);
        else
            modify_stat(STAT_STRENGTH, 5, true, "");

        // conceivable max gain of +184 {dlb}
        you.duration[DUR_MIGHT] += (35 + random2(pow)) / factor;

        // files.cc permits values up to 215, but ... {dlb}
        if (you.duration[DUR_MIGHT] > 80)
            you.duration[DUR_MIGHT] = 80;

        did_god_conduct(DID_STIMULANTS, 4 + random2(4), was_known);
        break;
    }

    case POT_GAIN_STRENGTH:
        if (mutate(MUT_STRONG, true, false, false, true))
            learned_something_new(TUT_YOU_MUTATED);
        break;

    case POT_GAIN_DEXTERITY:
        if (mutate(MUT_AGILE, true, false, false, true))
            learned_something_new(TUT_YOU_MUTATED);
        break;

    case POT_GAIN_INTELLIGENCE:
        if (mutate(MUT_CLEVER, true, false, false, true))
            learned_something_new(TUT_YOU_MUTATED);
        break;

    case POT_LEVITATION:
        mprf(MSGCH_DURATION,
             "You feel %s buoyant.", !player_is_airborne() ? "very"
                                                           : "more");

        if (!player_is_airborne())
            mpr("You gently float upwards from the floor.");

        // Amulet of Controlled Flight can auto-ID.
        if (!you.duration[DUR_LEVITATION]
            && wearing_amulet(AMU_CONTROLLED_FLIGHT)
            && !extrinsic_amulet_effect(AMU_CONTROLLED_FLIGHT))
        {
            item_def& amu(you.inv[you.equip[EQ_AMULET]]);
            if (!is_artefact(amu) && !item_type_known(amu))
            {
                set_ident_type(amu.base_type, amu.sub_type, ID_KNOWN_TYPE);
                set_ident_flags(amu, ISFLAG_KNOW_PROPERTIES);
                mprf("You are wearing: %s",
                     amu.name(DESC_INVENTORY_EQUIP).c_str());
            }
        }

        you.duration[DUR_LEVITATION] += 25 + random2(pow);

        if (you.duration[DUR_LEVITATION] > 100)
            you.duration[DUR_LEVITATION] = 100;

        burden_change();
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

            poison_player( ((pot_eff == POT_POISON) ? 1 + random2avg(5, 2)
                                                    : 3 + random2avg(13, 2)) );
            xom_is_stimulated(128);
        }
        break;

    case POT_SLOWING:
        if (slow_player((10 + random2(pow)) / factor))
            xom_is_stimulated(64 / factor);
        break;

    case POT_PARALYSIS:
        you.paralyse(NULL,
                     (2 + random2( 6 + you.duration[DUR_PARALYSIS] )) / factor);
        xom_is_stimulated(64 / factor);
        break;

    case POT_CONFUSION:
        if (confuse_player((3 + random2(8)) / factor))
            xom_is_stimulated(128 / factor);
        break;

    case POT_INVISIBILITY:
        mpr(!you.duration[DUR_INVIS] ? "You fade into invisibility!"
                                     : "You fade further into invisibility.",
            MSGCH_DURATION);

        // Invisibility cancels backlight.
        you.duration[DUR_BACKLIGHT] = 0;

        // Now multiple invisiblity casts aren't as good. -- bwr
        if (!you.duration[DUR_INVIS])
            you.duration[DUR_INVIS] = 15 + random2(pow);
        else
            you.duration[DUR_INVIS] += random2(pow);

        if (you.duration[DUR_INVIS] > 100)
            you.duration[DUR_INVIS] = 100;

        you.duration[DUR_BACKLIGHT] = 0;
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
            xom_is_stimulated(64 / factor);
        }
        break;

    // Don't generate randomly - should be rare and interesting.
    case POT_DECAY:
        if (rot_player((10 + random2(10)) / factor))
            xom_is_stimulated(64 / factor);
        break;

    case POT_WATER:
        if (you.species == SP_VAMPIRE)
            mpr("Blech - this tastes like water.");
        else
        {
            mpr("This tastes like water.");
            lessen_hunger(20, true);
        }
        break;

    case POT_EXPERIENCE:
        if (you.experience_level < 27)
        {
            mpr("You feel more experienced!");

            you.experience = 1 + exp_needed( 2 + you.experience_level );
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
            if (go_berserk(true))
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

        learned_something_new(TUT_YOU_MUTATED);
        did_god_conduct(DID_DELIBERATE_MUTATING, 10, was_known);
        did_god_conduct(DID_STIMULANTS, 4 + random2(4), was_known);
        break;

    case POT_RESISTANCE:
        mpr("You feel protected.", MSGCH_DURATION);
        you.duration[DUR_RESIST_FIRE]   += (random2(pow) + 10) / factor;
        you.duration[DUR_RESIST_COLD]   += (random2(pow) + 10) / factor;
        you.duration[DUR_RESIST_POISON] += (random2(pow) + 10) / factor;
        you.duration[DUR_INSULATION]    += (random2(pow) + 10) / factor;

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

    if (you.duration[DUR_BERSERKER])
    {
        canned_msg(MSG_TOO_BERSERK);
        return (false);
    }

    item_def& item = *you.weapon();

    if (!safe_to_remove_or_wear(item, true))
        return (false);

    you.equip[EQ_WEAPON] = -1;
    you.special_wield    = SPWLD_NONE;
    you.wield_change     = true;
    you.m_quiver->on_weapon_changed();

    if (item.base_type == OBJ_MISCELLANY
        && item.sub_type == MISC_LANTERN_OF_SHADOWS )
    {
        you.current_vision += 2;
        setLOSRadius(you.current_vision);
    }
    else if (item.base_type == OBJ_WEAPONS)
    {
        if (is_fixed_artefact( item ))
        {
            switch (item.special)
            {
            case SPWPN_SINGING_SWORD:
                if (showMsgs)
                    mpr("The Singing Sword sighs.", MSGCH_TALK);
                break;
            case SPWPN_WRATH_OF_TROG:
                if (showMsgs)
                    mpr("You feel less violent.");
                break;
            case SPWPN_STAFF_OF_OLGREB:
                item.plus  = 0;
                item.plus2 = 0;
                break;
            case SPWPN_STAFF_OF_WUCAD_MU:
                item.plus  = 0;
                item.plus2 = 0;
                MiscastEffect( &you, WIELD_MISCAST, SPTYP_DIVINATION, 9, 90,
                               "the Staff of Wucad Mu" );
                break;
            default:
                break;
            }

            return (true);
        }

        const int brand = get_weapon_brand( item );

        if (is_random_artefact( item ))
            unuse_randart(item);

        if (brand != SPWPN_NORMAL)
        {
            const std::string msg = item.name(DESC_CAP_YOUR);

            switch (brand)
            {
            case SPWPN_SWORD_OF_CEREBOV:
            case SPWPN_FLAMING:
                if (showMsgs)
                    mprf("%s stops flaming.", msg.c_str());
                break;

            case SPWPN_FREEZING:
            case SPWPN_HOLY_WRATH:
                if (showMsgs)
                    mprf("%s stops glowing.", msg.c_str());
                break;

            case SPWPN_ELECTROCUTION:
                if (showMsgs)
                    mprf("%s stops crackling.", msg.c_str());
                break;

            case SPWPN_VENOM:
                if (showMsgs)
                    mprf("%s stops dripping with poison.", msg.c_str());
                break;

            case SPWPN_PROTECTION:
                if (showMsgs)
                    mpr("You feel less protected.");
                you.redraw_armour_class = true;
                break;

            case SPWPN_VAMPIRICISM:
                if (showMsgs)
                    mpr("You feel the strange hunger wane.");
                break;

            case SPWPN_DISTORTION:
                // Removing the translocations skill reduction of effect,
                // it might seem sensible, but this brand is supposed
                // to be dangerous because it does large bonus damage,
                // as well as free teleport other side effects, and
                // even with the miscast effects you can rely on the
                // occasional spatial bonus to mow down some opponents.
                // It's far too powerful without a real risk, especially
                // if it's to be allowed as a player spell. -- bwr

                // int effect = 9 - random2avg( you.skills[SK_TRANSLOCATIONS] * 2, 2 );
                MiscastEffect( &you, WIELD_MISCAST, SPTYP_TRANSLOCATION, 9, 90,
                               "distortion unwield" );
                break;

                // NOTE: When more are added here, *must* duplicate unwielding
                // effect in vorpalise weapon scroll effect in read_scoll.
            }

            if (you.duration[DUR_WEAPON_BRAND])
            {
                you.duration[DUR_WEAPON_BRAND] = 0;
                set_item_ego_type( item, OBJ_WEAPONS, SPWPN_NORMAL );

                // We're letting this through even if hiding messages.
                mpr("Your branding evaporates.");
            }
        }
    }
    else if (item.base_type == OBJ_STAVES && item.sub_type == STAFF_POWER)
    {
        calc_mp();
        mpr("You feel your mana capacity decrease.");
    }

    you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = 0;

    return (true);
}

// This does *not* call ev_mod!
void unwear_armour(int slot)
{
    you.redraw_armour_class = true;
    you.redraw_evasion = true;

    item_def &item(you.inv[slot]);

    switch (get_armour_ego_type( item ))
    {
    case SPARM_RUNNING:
        mpr("You feel rather sluggish.");
        break;

    case SPARM_FIRE_RESISTANCE:
        mpr("\"Was it this warm in here before?\"");
        break;

    case SPARM_COLD_RESISTANCE:
        mpr("You catch a bit of a chill.");
        break;

    case SPARM_POISON_RESISTANCE:
        if (!player_res_poison())
            mpr("You feel less healthy.");
        break;

    case SPARM_SEE_INVISIBLE:
        if (!player_see_invis())
            mpr("You feel less perceptive.");
        break;

    case SPARM_DARKNESS:        // I do not understand this {dlb}
        if (you.duration[DUR_INVIS])
            you.duration[DUR_INVIS] = 1;
        break;

    case SPARM_STRENGTH:
        modify_stat(STAT_STRENGTH, -3, false, item, true);
        break;

    case SPARM_DEXTERITY:
        modify_stat(STAT_DEXTERITY, -3, false, item, true);
        break;

    case SPARM_INTELLIGENCE:
        modify_stat(STAT_INTELLIGENCE, -3, false, item, true);
        break;

    case SPARM_PONDEROUSNESS:
        mpr("That put a bit of spring back into your step.");
        // you.speed -= 2;
        break;

    case SPARM_LEVITATION:
        if (you.duration[DUR_LEVITATION])
            you.duration[DUR_LEVITATION] = 1;
        break;

    case SPARM_MAGIC_RESISTANCE:
        mpr("You feel less resistant to magic.");
        break;

    case SPARM_PROTECTION:
        mpr("You feel less protected.");
        break;

    case SPARM_STEALTH:
        mpr("You feel less stealthy.");
        break;

    case SPARM_RESISTANCE:
        mpr("You feel hot and cold all over.");
        break;

    case SPARM_POSITIVE_ENERGY:
        mpr("You feel vulnerable.");
        break;

    case SPARM_ARCHMAGI:
        mpr("You feel strangely numb.");
        break;

    default:
        break;
    }

    if (is_random_artefact(item))
        unuse_randart(item);
}

void unuse_randart(const item_def &item)
{
    ASSERT( is_random_artefact( item ) );

    randart_properties_t proprt;
    randart_known_props_t known;
    randart_wpn_properties( item, proprt, known );

    if (proprt[RAP_AC])
    {
        you.redraw_armour_class = true;
        if (!known[RAP_AC])
        {
            mprf("You feel less %s.",
                 proprt[RAP_AC] > 0? "well-protected" : "vulnerable");
        }
    }

    if (proprt[RAP_EVASION])
    {
        you.redraw_evasion = true;
        if (!known[RAP_EVASION])
        {
            mprf("You feel less %s.",
                 proprt[RAP_EVASION] > 0? "nimble" : "awkward");
        }
    }

    if (proprt[RAP_MAGICAL_POWER])
    {
        you.redraw_magic_points = true;
        if (!known[RAP_MAGICAL_POWER])
        {
            mprf("You feel your mana capacity %s.",
                 proprt[RAP_MAGICAL_POWER] > 0 ? "decrease" : "increase");
        }
    }

    // Modify ability scores; always output messages.
    modify_stat( STAT_STRENGTH,     -proprt[RAP_STRENGTH],     false, item,
                 true);
    modify_stat( STAT_INTELLIGENCE, -proprt[RAP_INTELLIGENCE], false, item,
                 true);
    modify_stat( STAT_DEXTERITY,    -proprt[RAP_DEXTERITY],    false, item,
                 true);

    if (proprt[RAP_NOISES] != 0)
        you.special_wield = SPWLD_NONE;
}
