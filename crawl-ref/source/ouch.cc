/**
 * @file
 * @brief Functions used when Bad Things happen to the player.
**/

#include "AppHdr.h"

#include <cstring>
#include <string>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cmath>

#ifdef UNIX
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "ouch.h"

#include "externs.h"
#include "options.h"

#include "art-enum.h"
#include "artefact.h"
#include "beam.h"
#include "chardump.h"
#include "delay.h"
#include "describe.h"
#include "dgnevent.h"
#include "effects.h"
#include "env.h"
#include "files.h"
#include "fight.h"
#include "fineff.h"
#include "godabil.h"
#include "hints.h"
#include "hiscores.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-util.h"
#include "mon-place.h"
#include "mon-stuff.h"
#include "mutation.h"
#include "notes.h"
#include "player.h"
#include "player-stats.h"
#include "potion.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "skills2.h"
#include "spl-selfench.h"
#include "spl-other.h"
#include "state.h"
#include "stuff.h"
#include "transform.h"
#include "tutorial.h"
#include "view.h"
#include "shout.h"
#include "xom.h"

static void _end_game(scorefile_entry &se);

static void _maybe_melt_player_enchantments(beam_type flavour, int damage)
{
    if (flavour == BEAM_FIRE || flavour == BEAM_LAVA
        || flavour == BEAM_HELLFIRE || flavour == BEAM_NAPALM
        || flavour == BEAM_STEAM)
    {
        if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
        {
            you.duration[DUR_CONDENSATION_SHIELD] -= damage * BASELINE_DELAY;
            if (you.duration[DUR_CONDENSATION_SHIELD] <= 0)
                remove_condensation_shield();
            else
                you.props["melt_shield"] = true;
        }

        if (you.mutation[MUT_ICEMAIL])
        {
            mprf(MSGCH_DURATION, "Your icy envelope dissipates!");
            you.duration[DUR_ICEMAIL_DEPLETED] = ICEMAIL_TIME;
            you.redraw_armour_class = true;
        }

        if (you.duration[DUR_ICY_ARMOUR] > 0)
        {
            you.duration[DUR_ICY_ARMOUR] -= damage * BASELINE_DELAY;
            if (you.duration[DUR_ICY_ARMOUR] <= 0)
                remove_ice_armour();
            else
                you.props["melt_armour"] = true;
        }
    }
}

// NOTE: DOES NOT check for hellfire!!!
int check_your_resists(int hurted, beam_type flavour, string source,
                       bolt *beam, bool doEffects)
{
    int resist;
    int original = hurted;

    dprf("checking resistance: flavour=%d", flavour);

    string kaux = "";
    if (beam)
    {
        source = beam->get_source_name();
        kaux = beam->name;
    }

    if (doEffects)
        _maybe_melt_player_enchantments(flavour, hurted);

    switch (flavour)
    {
    case BEAM_WATER:
        hurted = resist_adjust_damage(&you, flavour,
                                      you.res_water_drowning(), hurted, true);
        if (!hurted && doEffects)
            mpr("You shrug off the wave.");
        else if (hurted > original && doEffects)
        {
            mpr("The water douses you terribly!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_STEAM:
        hurted = resist_adjust_damage(&you, flavour,
                                      player_res_steam(), hurted, true);
        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_RESIST);
        else if (hurted > original && doEffects)
        {
            mpr("The steam scalds you terribly!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_FIRE:
        hurted = resist_adjust_damage(&you, flavour,
                                      player_res_fire(), hurted, true);
        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_RESIST);
        else if (hurted > original && doEffects)
        {
            mpr("The fire burns you terribly!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_HELLFIRE:
        if (you.species == SP_DJINNI)
        {
            hurted = 0;
            if (doEffects)
                mpr("You resist completely.");
        }
        // Inconsistency: no penalty for rF-, unlike monsters.  That's
        // probably good, and monsters should be changed.
        break;

    case BEAM_COLD:
        hurted = resist_adjust_damage(&you, flavour,
                                      player_res_cold(), hurted, true);
        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_RESIST);
        else if (hurted > original && doEffects)
        {
            mpr("You feel a terrible chill!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_ELECTRICITY:
        hurted = resist_adjust_damage(&you, flavour,
                                      player_res_electricity(),
                                      hurted, true);

        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_RESIST);
        break;

    case BEAM_POISON:
        if (doEffects)
        {
            resist = poison_player(coinflip() ? 2 : 1, source, kaux) ? 0 : 1;

            hurted = resist_adjust_damage(&you, flavour, resist,
                                          hurted, true);
            if (resist > 0)
                canned_msg(MSG_YOU_RESIST);
        }
        else
        {
            hurted = resist_adjust_damage(&you, flavour, player_res_poison(),
                                          hurted, true);
        }
        break;

    case BEAM_POISON_ARROW:
        // [dshaligram] NOT importing uber-poison arrow from 4.1. Giving no
        // bonus to poison resistant players seems strange and unnecessarily
        // arbitrary.

        resist = player_res_poison();

        if (doEffects)
        {
            int poison_amount = 2 + random2(3);
            poison_amount += (resist ? 0 : 2);
            poison_player(poison_amount, source, kaux, true);
        }

        hurted = resist_adjust_damage(&you, flavour, resist, hurted);
        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_PARTIALLY_RESIST);
        break;

    case BEAM_NEG:
        hurted = resist_adjust_damage(&you, flavour, player_prot_life(),
                                      hurted, true);

        if (doEffects)
            drain_exp(true, min(75, 35 + original * 2 / 3));
        break;

    case BEAM_ICE:
        hurted = resist_adjust_damage(&you, flavour, player_res_cold(),
                                      hurted, true);

        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_PARTIALLY_RESIST);
        else if (hurted > original && doEffects)
        {
            mpr("You feel a painful chill!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_LAVA:
        hurted = resist_adjust_damage(&you, flavour, player_res_fire(),
                                      hurted, true);

        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_PARTIALLY_RESIST);
        else if (hurted > original && doEffects)
        {
            mpr("The lava burns you terribly!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_ACID:
        if (player_res_acid())
        {
            if (doEffects)
                canned_msg(MSG_YOU_RESIST);
            hurted = hurted * player_acid_resist_factor() / 100;
        }
        break;

    case BEAM_MIASMA:
        if (you.res_rotting())
        {
            if (doEffects)
                canned_msg(MSG_YOU_RESIST);
            hurted = 0;
        }
        break;

    case BEAM_HOLY:
    {
        // Cleansing flame.
        const int rhe = you.res_holy_energy(NULL);
        if (rhe > 0)
            hurted = 0;
        else if (rhe == 0)
            hurted /= 2;
        else if (rhe < -1)
            hurted = hurted * 3 / 2;

        if (hurted == 0 && doEffects)
            canned_msg(MSG_YOU_RESIST);
        break;
    }

    case BEAM_BOLT_OF_ZIN:
    {
        // Damage to chaos and mutations.

        // For mutation damage, we want to count innate mutations for
        // the demonspawn, but not for other species.
        int mutated = how_mutated(you.species == SP_DEMONSPAWN, true);
        int multiplier = min(mutated * 3, 60);
        if (you.is_chaotic() || player_is_shapechanged())
            multiplier = 60; // full damage
        else if (you.is_undead || is_chaotic_god(you.religion))
            multiplier = max(multiplier, 20);

        hurted = hurted * multiplier / 60;

        if (doEffects)
        {
            if (hurted <= 0)
                canned_msg(MSG_YOU_RESIST);
            else if (multiplier > 30)
                mpr("The blast sears you terribly!");
            else
                mpr("The blast sears you!");

            if (one_chance_in(3)
                // delete_mutation() handles MUT_MUTATION_RESISTANCE but not the amulet
                && (!you.rmut_from_item()
                    || one_chance_in(10)))
            {
                // silver stars only, if this ever changes we may want to give
                // aux as well
                delete_mutation(RANDOM_GOOD_MUTATION, source);
            }
        }
        break;
    }

    case BEAM_LIGHT:
        if (you.species == SP_VAMPIRE)
            hurted += hurted / 2;

        if (hurted > original && doEffects)
        {
            mpr("The light scorches you terribly!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_AIR:
    {
        // Airstrike.
        if (you.res_wind() > 0)
            hurted = 0;
        else if (you.flight_mode())
            hurted += hurted / 2;
        break;
    }

    case BEAM_GHOSTLY_FLAME:
    {
        if (you.holiness() == MH_UNDEAD)
        {
            if (doEffects)
            {
                you.heal(roll_dice(2, 9));
                mpr("You are bolstered by the flame.");
            }
            hurted = 0;
        }
        else
        {
            hurted = resist_adjust_damage(&you, flavour,
                                          you.res_negative_energy(),
                                          hurted, true);
            if (hurted < original && doEffects)
                canned_msg(MSG_YOU_PARTIALLY_RESIST);
        }
    }

    default:
        break;
    }                           // end switch

    if (doEffects)
        maybe_id_resist(flavour);

    return hurted;
}

void splash_with_acid(int acid_strength, int death_source, bool corrode_items,
                      const char* hurt_msg)
{
    int dam = 0;
    const bool wearing_cloak = player_wearing_slot(EQ_CLOAK);

    for (int slot = EQ_MIN_ARMOUR; slot <= EQ_MAX_ARMOUR; slot++)
    {
        const bool cloak_protects = wearing_cloak && coinflip()
                                    && slot != EQ_SHIELD && slot != EQ_CLOAK;

        if (!cloak_protects)
        {
            item_def *item = you.slot_item(static_cast<equipment_type>(slot));
            if (!item && slot != EQ_SHIELD)
                dam++;

            if (item && corrode_items && x_chance_in_y(acid_strength + 1, 20))
                corrode_item(*item, &you);
        }
    }

    // Covers head, hands and feet.
    if (player_equip_unrand(UNRAND_LEAR))
        dam = !wearing_cloak;

    // Without fur, clothed people have dam 0 (+2 later), Sp/Tr/Dr/Og ~1
    // (randomized), Fe 5.  Fur helps only against naked spots.
    const int fur = player_mutation_level(MUT_SHAGGY_FUR);
    dam -= fur * dam / 5;

    // two extra virtual slots so players can't be immune
    dam += 2;
    dam = roll_dice(dam, acid_strength);

    const int post_res_dam = dam * player_acid_resist_factor() / 100;

    if (post_res_dam > 0)
    {
        mpr(hurt_msg ? "The acid burns!" : hurt_msg);

        if (post_res_dam < dam)
            canned_msg(MSG_YOU_RESIST);

        ouch(post_res_dam, death_source, KILLED_BY_ACID);
    }
}

// Helper function for the expose functions below.
// This currently works because elements only target a single type each.
static int _get_target_class(beam_type flavour)
{
    int target_class = OBJ_UNASSIGNED;

    switch (flavour)
    {
    case BEAM_FIRE:
    case BEAM_LAVA:
    case BEAM_NAPALM:
    case BEAM_HELLFIRE:
        target_class = OBJ_SCROLLS;
        break;

    case BEAM_COLD:
    case BEAM_ICE:
    case BEAM_FRAG:
        target_class = OBJ_POTIONS;
        break;

    case BEAM_SPORE:
    case BEAM_DEVOUR_FOOD:
        target_class = OBJ_FOOD;
        break;

    default:
        break;
    }

    return target_class;
}

// XXX: These expose functions could use being reworked into a real system...
// the usage and implementation is currently very hacky.
// Handles the destruction of inventory items from the elements.
static bool _expose_invent_to_element(beam_type flavour, int strength)
{
    int num_dest = 0;
    int total_dest = 0;
    int jiyva_block = 0;

    const int target_class = _get_target_class(flavour);
    if (target_class == OBJ_UNASSIGNED)
        return false;

    // Wisp form semi-melds all of inventory, making it unusable for you,
    // but also immune to destruction.  No message is needed.
    if (you.form == TRAN_WISP)
        return false;

    // Fedhas worshipers are exempt from the food destruction effect
    // of spores.
    if (flavour == BEAM_SPORE
        && you_worship(GOD_FEDHAS))
    {
        simple_god_message(" protects your food from the spores.",
                           GOD_FEDHAS);
        return false;
    }

    // Currently we test against each stack (and item in the stack)
    // independently at strength%... perhaps we don't want that either
    // because it makes the system very fair and removes the protection
    // factor of junk (which might be more desirable for game play).
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (!you.inv[i].defined())
            continue;

        if (you.inv[i].base_type == target_class
            || target_class == OBJ_FOOD
               && you.inv[i].base_type == OBJ_CORPSES)
        {
            // Conservation doesn't help against harpies' devouring food.
            if (flavour != BEAM_DEVOUR_FOOD
                && you.conservation() && !one_chance_in(10))
            {
                continue;
            }

            // These stack with conservation; they're supposed to be good.
            if (target_class == OBJ_SCROLLS
                && you.mutation[MUT_CONSERVE_SCROLLS]
                && !one_chance_in(10))
            {
                continue;
            }

            if (target_class == OBJ_POTIONS
                && you.mutation[MUT_CONSERVE_POTIONS]
                && !one_chance_in(10))
            {
                continue;
            }

            if (you_worship(GOD_JIYVA) && !player_under_penance()
                && x_chance_in_y(you.piety, MAX_PIETY))
            {
                ++jiyva_block;
                continue;
            }

            // Get name and quantity before destruction.
            const string item_name = you.inv[i].name(DESC_PLAIN);
            const int quantity = you.inv[i].quantity;
            num_dest = 0;

            // Loop through all items in the stack.
            for (int j = 0; j < you.inv[i].quantity; ++j)
            {
                if (bernoulli(strength, 0.01))
                {
                    num_dest++;

                    if (i == you.equip[EQ_WEAPON])
                        you.wield_change = true;

                    if (dec_inv_item_quantity(i, 1))
                        break;
                    else if (is_blood_potion(you.inv[i]))
                        remove_oldest_blood_potion(you.inv[i]);
                }
            }

            // Name destroyed items.
            // TODO: Combine messages using a vector.
            if (num_dest > 0)
            {
                switch (target_class)
                {
                case OBJ_SCROLLS:
                    mprf("%s %s catch%s fire!",
                         part_stack_string(num_dest, quantity).c_str(),
                         item_name.c_str(),
                         (num_dest == 1) ? "es" : "");
                    break;

                case OBJ_POTIONS:
                    mprf("%s %s freeze%s and shatter%s!",
                         part_stack_string(num_dest, quantity).c_str(),
                         item_name.c_str(),
                         (num_dest == 1) ? "s" : "",
                         (num_dest == 1) ? "s" : "");
                    break;

                case OBJ_FOOD:
                    mprf("%s %s %s %s!",
                         part_stack_string(num_dest, quantity).c_str(),
                         item_name.c_str(),
                         (num_dest == 1) ? "is" : "are",
                         (flavour == BEAM_DEVOUR_FOOD) ?
                             "devoured" : "covered with spores");
                    break;

                default:
                    mprf("%s %s %s destroyed!",
                         part_stack_string(num_dest, quantity).c_str(),
                         item_name.c_str(),
                         (num_dest == 1) ? "is" : "are");
                    break;
                }

                total_dest += num_dest;
            }
        }
    }

    if (jiyva_block)
    {
        simple_god_message(
            make_stringf(" shields %s delectables from destruction.",
                         (total_dest > 0) ? "some of your" : "your").c_str(),
            GOD_JIYVA);
    }

    if (!total_dest)
        return false;

    // Message handled elsewhere.
    if (flavour == BEAM_DEVOUR_FOOD)
        return true;

    xom_is_stimulated((num_dest > 1) ? 25 : 12);

    return true;
}

// Handle side-effects for exposure to element other than damage.  This
// function exists because some code calculates its own damage instead
// of using check_your_resists() and we want to isolate all the special
// code they keep having to do... namely condensation shield checks,
// you really can't expect this function to even be called for much
// else.
//
// This function now calls _expose_invent_to_element() if strength > 0.
//
// XXX: This function is far from perfect and a work in progress.
bool expose_player_to_element(beam_type flavour, int strength,
                              bool damage_inventory, bool slow_dracs)
{
    _maybe_melt_player_enchantments(flavour, strength ? strength : 10);

    if (flavour == BEAM_COLD && slow_dracs && player_genus(GENPC_DRACONIAN)
        && you.res_cold() <= 0 && coinflip())
    {
        you.slow_down(0, strength);
    }

    if (flavour == BEAM_WATER && you.duration[DUR_LIQUID_FLAMES])
    {
        mprf(MSGCH_WARN, "The flames go out!");
        you.duration[DUR_LIQUID_FLAMES] = 0;
        you.props.erase("napalmer");
        you.props.erase("napalm_aux");
    }

    if (strength <= 0 || !damage_inventory)
        return false;

    return _expose_invent_to_element(flavour, strength);
}

static void _lose_level_abilities()
{
    if (you.attribute[ATTR_PERM_FLIGHT]
        && !you.racial_permanent_flight()
        && !you.wearing_ego(EQ_ALL_ARMOUR, SPARM_FLYING))
    {
        you.increase_duration(DUR_FLIGHT, 50, 100);
        you.attribute[ATTR_PERM_FLIGHT] = 0;
        mprf(MSGCH_WARN, "You feel your flight won't last long.");
    }
}

void lose_level(int death_source, const char *aux)
{
    // Because you.experience is unsigned long, if it's going to be
    // negative, must die straightaway.
    if (you.experience_level == 1)
    {
        ouch(INSTANT_DEATH, death_source, KILLED_BY_DRAINING, aux);
        // Return in case death was cancelled via wizard mode
        return;
    }

    you.experience_level--;

    mprf(MSGCH_WARN,
         "You are now level %d!", you.experience_level);

    ouch(4, death_source, KILLED_BY_DRAINING, aux);
    dec_mp(1);

    calc_hp();
    calc_mp();
    _lose_level_abilities();

    char buf[200];
    sprintf(buf, "HP: %d/%d MP: %d/%d",
            you.hp, you.hp_max, you.magic_points, you.max_magic_points);
    take_note(Note(NOTE_XP_LEVEL_CHANGE, you.experience_level, 0, buf));

    you.redraw_title = true;
    you.redraw_experience = true;
#ifdef USE_TILE_LOCAL
    // In case of intrinsic ability changes.
    tiles.layout_statcol();
    redraw_screen();
#endif

    xom_is_stimulated(200);

    // Kill the player if maxhp <= 0.  We can't just move the ouch() call past
    // dec_max_hp() since it would decrease hp twice, so here's another one.
    ouch(0, death_source, KILLED_BY_DRAINING, aux);
}

bool drain_exp(bool announce_full, int power)
{
    const int protection = player_prot_life();

    if (protection == 3)
    {
        if (announce_full)
            canned_msg(MSG_YOU_RESIST);

        return false;
    }

    if (protection > 0)
    {
        canned_msg(MSG_YOU_PARTIALLY_RESIST);
        power /= (protection * 2);
    }

    if (power > 0)
    {
        mpr("You feel drained.");
        xom_is_stimulated(15);

        you.attribute[ATTR_XP_DRAIN] += power;

        dprf("Drained by %d points (%d total)", power, you.attribute[ATTR_XP_DRAIN]);

        return true;
    }

    return false;
}

static void _xom_checks_damage(kill_method_type death_type,
                               int dam, int death_source)
{
    if (you_worship(GOD_XOM))
    {
        if (death_type == KILLED_BY_TARGETTING
            || death_type == KILLED_BY_BOUNCE
            || death_type == KILLED_BY_REFLECTION
            || death_type == KILLED_BY_SELF_AIMED
               && player_in_a_dangerous_place())
        {
            // Xom thinks the player accidentally hurting him/herself is funny.
            // Deliberate damage is only amusing if it's dangerous.
            int amusement = 200 * dam / (dam + you.hp);
            if (death_type == KILLED_BY_SELF_AIMED)
                amusement /= 5;
            xom_is_stimulated(amusement);
            return;
        }
        else if (death_type == KILLED_BY_FALLING_DOWN_STAIRS
                 || death_type == KILLED_BY_FALLING_THROUGH_GATE)
        {
            // Xom thinks falling down the stairs is hilarious.
            xom_is_stimulated(200);
            return;
        }
        else if (death_type == KILLED_BY_DISINT)
        {
            // flying chunks...
            xom_is_stimulated(100);
            return;
        }
        else if (death_type != KILLED_BY_MONSTER
                    && death_type != KILLED_BY_BEAM
                    && death_type != KILLED_BY_DISINT
                 || invalid_monster_index(death_source))
        {
            return;
        }

        int amusementvalue = 1;
        const monster* mons = &menv[death_source];

        if (!mons->alive())
            return;

        if (mons->wont_attack())
        {
            // Xom thinks collateral damage is funny.
            xom_is_stimulated(200 * dam / (dam + you.hp));
            return;
        }

        int leveldif = mons->hit_dice - you.experience_level;
        if (leveldif == 0)
            leveldif = 1;

        // Note that Xom is amused when you are significantly hurt by a
        // creature of higher level than yourself, as well as by a
        // creature of lower level than yourself.
        amusementvalue += leveldif * leveldif * dam;

        if (!mons->visible_to(&you))
            amusementvalue += 8;

        if (mons->speed < 100/player_movement_speed())
            amusementvalue += 7;

        if (death_type != KILLED_BY_BEAM
            && you.skill(SK_THROWING) <= (you.experience_level / 4))
        {
            amusementvalue += 2;
        }
        else if (you.skill(SK_FIGHTING) <= (you.experience_level / 4))
            amusementvalue += 2;

        if (player_in_a_dangerous_place())
            amusementvalue += 2;

        amusementvalue /= (you.hp > 0) ? you.hp : 1;

        xom_is_stimulated(amusementvalue);
    }
}

static void _yred_mirrors_injury(int dam, int death_source)
{
    if (yred_injury_mirror())
    {
        // Cap damage to what was enough to kill you.  Can matter if
        // Yred saves your life or you have an extra kitty.
        if (you.hp < 0)
            dam += you.hp;

        if (dam <= 0 || invalid_monster_index(death_source))
            return;

        (new mirror_damage_fineff(&menv[death_source], &you, dam))->schedule();
    }
}

static void _maybe_spawn_jellies(int dam, const char* aux,
                                  kill_method_type death_type, int death_source)
{
    // We need to exclude acid damage and similar things or this function
    // will crash later.
    if (death_source == NON_MONSTER)
        return;

    monster_type mon = royal_jelly_ejectable_monster();

    // Exclude torment damage.
    const bool torment = aux && strstr(aux, "torment");
    if (you_worship(GOD_JIYVA)
        && you.piety >= piety_breakpoint(5)
        && !torment)
    {
        int how_many = 0;
        if (dam >= you.hp_max * 3 / 4)
            how_many = random2(4) + 2;
        else if (dam >= you.hp_max / 2)
            how_many = random2(2) + 2;
        else if (dam >= you.hp_max / 4)
            how_many = 1;

        if (how_many > 0)
        {
            int count_created = 0;
            for (int i = 0; i < how_many; ++i)
            {
                int foe = death_source;
                if (invalid_monster_index(foe))
                    foe = MHITNOT;
                mgen_data mg(mon, BEH_FRIENDLY, &you, 2, 0, you.pos(),
                             foe, 0, GOD_JIYVA);

                if (create_monster(mg))
                    count_created++;
            }

            if (count_created > 0)
            {
                mprf("You shudder from the %s and a %s!",
                     death_type == KILLED_BY_MONSTER ? "blow" : "blast",
                     count_created > 1 ? "flood of jellies pours out from you"
                                       : "jelly pops out");
            }
        }
    }
}

static void _powered_by_pain(int dam)
{
    const int level = player_mutation_level(MUT_POWERED_BY_PAIN);

    if (you.mutation[MUT_POWERED_BY_PAIN]
        && (random2(dam) > 2 + 3 * level
            || dam >= you.hp_max / 2))
    {
        switch (random2(4))
        {
        case 0:
        case 1:
        {
            if (you.magic_points < you.max_magic_points)
            {
                mpr("You focus on the pain.");
                int mp = roll_dice(3, 2 + 3 * level);
                mpr("You feel your power returning.");
                inc_mp(mp);
                break;
            }
            break;
        }
        case 2:
            mpr("You focus on the pain.");
            potion_effect(POT_MIGHT, level * 20);
            break;
        case 3:
            mpr("You focus on the pain.");
            potion_effect(POT_AGILITY, level * 20);
            break;
        }
    }
}

static void _place_player_corpse(bool explode)
{
    if (!in_bounds(you.pos()))
        return;

    item_def corpse;
    if (fill_out_corpse(0, player_mons(), corpse) == MONS_NO_MONSTER)
        return;

    if (explode && explode_corpse(corpse, you.pos()))
        return;

    int o = get_mitm_slot();
    if (o == NON_ITEM)
    {
        item_was_destroyed(corpse);
        return;
    }

    corpse.props[MONSTER_HIT_DICE].get_short() = you.experience_level;
    corpse.props[CORPSE_NAME_KEY] = you.your_name;
    corpse.props[CORPSE_NAME_TYPE_KEY].get_int64() = 0;
    corpse.props["ev"].get_int() = player_evasion(static_cast<ev_ignore_type>(
                                   EV_IGNORE_HELPLESS | EV_IGNORE_PHASESHIFT));
    // mostly mutations here.  At least there's no need to handle armour.
    corpse.props["ac"].get_int() = you.armour_class();
    mitm[o] = corpse;

    move_item_to_grid(&o, you.pos(), !you.in_water());
}


#if defined(WIZARD) || defined(DEBUG)
static void _wizard_restore_life()
{
    if (you.hp_max <= 0)
        unrot_hp(9999);
    while (you.hp_max <= 0)
        you.hp_max_perm++, calc_hp();
    if (you.hp <= 0)
        set_hp(you.hp_max);
}
#endif

void reset_damage_counters()
{
    you.turn_damage = 0;
    you.damage_source = NON_MONSTER;
    you.source_damage = 0;
}

// death_source should be set to NON_MONSTER for non-monsters. {dlb}
void ouch(int dam, int death_source, kill_method_type death_type,
          const char *aux, bool see_source, const char *death_source_name,
          bool attacker_effects)
{
    ASSERT(!crawl_state.game_is_arena());
    if (you.duration[DUR_TIME_STEP])
        return;

    if (you.dead) // ... but eligible for revival
        return;

    if (attacker_effects && dam != INSTANT_DEATH
        && !invalid_monster_index(death_source)
        && menv[death_source].has_ench(ENCH_WRETCHED))
    {
        // An abstract boring simulation of reduced stats/etc due to bad muts
        // reducing the monster's melee damage, spell power, etc.  This is
        // wrong eg. for clouds that would be smaller and with a shorter
        // duration but do the same damage, etc.
        int degree = menv[death_source].get_ench(ENCH_WRETCHED).degree;
        dam = div_rand_round(dam * (10 - min(degree, 5)), 10);
    }

    if (dam != INSTANT_DEATH && you.species == SP_DEEP_DWARF)
    {
        // Deep Dwarves get to shave any hp loss.
        int shave = 1 + random2(2 + random2(1 + you.experience_level / 3));
        dprf("HP shaved: %d.", shave);
        dam -= shave;
        if (dam <= 0)
        {
            // Rotting and costs may lower hp directly.
            if (you.hp > 0)
                return;
            dam = 0;
        }
    }

    if (dam != INSTANT_DEATH)
    {
        if (you.petrified())
            dam /= 2;
        else if (you.petrifying())
            dam = dam * 10 / 15;
    }
    ait_hp_loss hpl(dam, death_type);
    interrupt_activity(AI_HP_LOSS, &hpl);

    if (dam > 0 && death_type != KILLED_BY_POISON)
        you.check_awaken(500);

    const bool non_death = death_type == KILLED_BY_QUITTING
        || death_type == KILLED_BY_WINNING
        || death_type == KILLED_BY_LEAVING;

    if (you.duration[DUR_DEATHS_DOOR] && death_type != KILLED_BY_LAVA
        && death_type != KILLED_BY_WATER && !non_death && you.hp_max > 0)
    {
        return;
    }

    if (dam != INSTANT_DEATH)
    {
        if (you.spirit_shield() && death_type != KILLED_BY_POISON
            && !(aux && strstr(aux, "flay_damage")))
        {
            // round off fairly (important for taking 1 damage at a time)
            int mp = div_rand_round(dam * you.magic_points,
                                    max(you.hp + you.magic_points, 1));
            // but don't kill the player with round-off errors
            mp = max(mp, dam + 1 - you.hp);
            mp = min(mp, you.magic_points);

            dam -= mp;
            dec_mp(mp);
            if (dam <= 0 && you.hp > 0)
                return;
        }

        if (dam >= you.hp && you.hp_max > 0 && god_protects_from_harm())
        {
            simple_god_message(" protects you from harm!");
            return;
        }

        you.turn_damage += dam;
        if (you.damage_source != death_source)
        {
            you.damage_source = death_source;
            you.source_damage = 0;
        }
        you.source_damage += dam;

        dec_hp(dam, true);

        // Even if we have low HP messages off, we'll still give a
        // big hit warning (in this case, a hit for half our HPs) -- bwr
        if (dam > 0 && you.hp_max <= dam * 2)
            mprf(MSGCH_DANGER, "Ouch! That really hurt!");

        if (you.hp > 0)
        {
            if (Options.hp_warning
                && you.hp <= (you.hp_max * Options.hp_warning) / 100)
            {
                mprf(MSGCH_DANGER, "* * * LOW HITPOINT WARNING * * *");
                dungeon_events.fire_event(DET_HP_WARNING);
            }

            hints_healing_check();

            _xom_checks_damage(death_type, dam, death_source);

            // for note taking
            string damage_desc;
            if (!see_source)
                damage_desc = make_stringf("something (%d)", dam);
            else
            {
                damage_desc = scorefile_entry(dam, death_source,
                                              death_type, aux, true)
                    .death_description(scorefile_entry::DDV_TERSE);
            }

            take_note(Note(NOTE_HP_CHANGE, you.hp, you.hp_max,
                           damage_desc.c_str()));

            _yred_mirrors_injury(dam, death_source);
            _maybe_spawn_jellies(dam, aux, death_type, death_source);
            _powered_by_pain(dam);

            return;
        } // else hp <= 0
    }

    // Is the player being killed by a direct act of Xom?
    if (crawl_state.is_god_acting()
        && crawl_state.which_god_acting() == GOD_XOM
        && crawl_state.other_gods_acting().empty())
    {
        you.escaped_death_cause = death_type;
        you.escaped_death_aux   = aux == NULL ? "" : aux;

        // Xom should only kill his worshippers if they're under penance
        // or Xom is bored.
        if (you_worship(GOD_XOM) && !you.penance[GOD_XOM]
            && you.gift_timeout > 0)
        {
            return;
        }

        // Also don't kill wizards testing Xom acts.
        if ((crawl_state.repeat_cmd == CMD_WIZARD
                || crawl_state.prev_cmd == CMD_WIZARD)
            && !you_worship(GOD_XOM))
        {
            return;
        }

        // Okay, you *didn't* escape death.
        you.reset_escaped_death();

        // Ensure some minimal information about Xom's involvement.
        if (aux == NULL || !*aux)
        {
            if (death_type != KILLED_BY_XOM)
                aux = "Xom";
        }
        else if (strstr(aux, "Xom") == NULL)
            death_type = KILLED_BY_XOM;
    }
    // Xom may still try to save your life.
    else if (xom_saves_your_life(dam, death_source, death_type, aux,
                                 see_source))
    {
        return;
    }

#if defined(WIZARD) || defined(DEBUG)
    if (!non_death && crawl_state.disables[DIS_DEATH])
    {
        _wizard_restore_life();
        return;
    }
#endif

    crawl_state.cancel_cmd_all();

    // Construct scorefile entry.
    scorefile_entry se(dam, death_source, death_type, aux, false,
                       death_source_name);

#ifdef WIZARD
    if (!non_death)
    {
        if (crawl_state.test || you.wizard)
        {
            const string death_desc
                = se.death_description(scorefile_entry::DDV_VERBOSE);

            dprf("Damage: %d; Hit points: %d", dam, you.hp);

            if (crawl_state.test || !yesno("Die?", false, 'n'))
            {
                mpr("Thought so.");
                take_note(Note(NOTE_DEATH, you.hp, you.hp_max,
                                death_desc.c_str()), true);
                _wizard_restore_life();
                return;
            }
        }
    }
#endif  // WIZARD

    if (crawl_state.game_is_tutorial())
    {
        crawl_state.need_save = false;
        if (!non_death)
            tutorial_death_message();

        screen_end_game("");
    }

    // Okay, so you're dead.
    take_note(Note(NOTE_DEATH, you.hp, you.hp_max,
                    se.death_description(scorefile_entry::DDV_NORMAL).c_str()),
              true);
    if (you.lives && !non_death)
    {
        mark_milestone("death", lowercase_first(se.long_kill_message()).c_str());

        you.deaths++;
        you.lives--;
        you.dead = true;

        stop_delay(true);

        // You wouldn't want to lose this accomplishment to a crash, would you?
        // Especially if you manage to trigger one via lua somehow...
        if (!crawl_state.disables[DIS_SAVE_CHECKPOINTS])
            save_game(false);

        canned_msg(MSG_YOU_DIE);
        xom_death_message((kill_method_type) se.get_death_type());
        more();

        _place_player_corpse(death_type == KILLED_BY_DISINT);
        return;
    }

    // The game's over.
    crawl_state.need_save       = false;
    crawl_state.updating_scores = true;

    // Prevent bogus notes.
    activate_notes(false);

#ifndef SCORE_WIZARD_CHARACTERS
    if (!you.wizard)
#endif
    {
        // Add this highscore to the score file.
        hiscores_new_entry(se);
        logfile_new_entry(se);
    }

    // Never generate bones files of wizard or tutorial characters -- bwr
    if (!non_death && !crawl_state.game_is_tutorial() && !you.wizard)
        save_ghost();

    _end_game(se);
}

string morgue_name(string char_name, time_t when_crawl_got_even)
{
    string name = "morgue-" + char_name;

    string time = make_file_time(when_crawl_got_even);
    if (!time.empty())
        name += "-" + time;

    return name;
}

// Delete save files on game end.
static void _delete_files()
{
    crawl_state.need_save = false;
    you.save->unlink();
    delete you.save;
    you.save = 0;
}

void screen_end_game(string text)
{
    crawl_state.cancel_cmd_all();
    _delete_files();

    if (!text.empty())
    {
        clrscr();
        linebreak_string(text, get_number_of_cols());
        display_tagged_block(text);

        if (!crawl_state.seen_hups)
            get_ch();
    }

    game_ended();
}

void _end_game(scorefile_entry &se)
{
    for (int i = 0; i < ENDOFPACK; i++)
        if (you.inv[i].defined() && item_type_unknown(you.inv[i]))
            add_inscription(you.inv[i], "unknown");

    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (!you.inv[i].defined())
            continue;
        set_ident_flags(you.inv[i], ISFLAG_IDENT_MASK);
        set_ident_type(you.inv[i], ID_KNOWN_TYPE);
    }

    _delete_files();

    // death message
    if (se.get_death_type() != KILLED_BY_LEAVING
        && se.get_death_type() != KILLED_BY_QUITTING
        && se.get_death_type() != KILLED_BY_WINNING)
    {
        canned_msg(MSG_YOU_DIE);
        xom_death_message((kill_method_type) se.get_death_type());

        switch (you.religion)
        {
        case GOD_FEDHAS:
            simple_god_message(" appreciates your contribution to the "
                               "ecosystem.");
            break;

        case GOD_NEMELEX_XOBEH:
            nemelex_death_message();
            break;

        case GOD_KIKUBAAQUDGHA:
        {
            const mon_holy_type holi = you.holiness();

            if (holi == MH_NONLIVING || holi == MH_UNDEAD)
            {
                simple_god_message(" rasps: \"You have failed me! "
                                   "Welcome... oblivion!\"");
            }
            else
            {
                simple_god_message(" rasps: \"You have failed me! "
                                   "Welcome... death!\"");
            }
            break;
        }

        case GOD_YREDELEMNUL:
            if (you.is_undead)
                simple_god_message(" claims you as an undead slave.");
            else if (se.get_death_type() != KILLED_BY_DISINT
                     && se.get_death_type() != KILLED_BY_LAVA)
            {
                mprf(MSGCH_GOD, "Your body rises from the dead as a mindless zombie.");
            }
            // No message if you're not undead and your corpse is lost.
            break;

        default:
            break;
        }

        flush_prev_message();
        viewwindow(); // don't do for leaving/winning characters

        if (crawl_state.game_is_hints())
            hints_death_screen();
    }

    if (!dump_char(morgue_name(you.your_name, se.get_death_time()),
                   true, true, &se))
    {
        mpr("Char dump unsuccessful! Sorry about that.");
        if (!crawl_state.seen_hups)
            more();
        clrscr();
    }

#ifdef DGL_WHEREIS
    whereis_record(se.get_death_type() == KILLED_BY_QUITTING? "quit" :
                   se.get_death_type() == KILLED_BY_WINNING ? "won"  :
                   se.get_death_type() == KILLED_BY_LEAVING ? "bailed out"
                                                            : "dead");
#endif

    if (!crawl_state.seen_hups)
        more();

    if (!crawl_state.disables[DIS_CONFIRMATIONS])
        browse_inventory();
    textcolor(LIGHTGREY);

    // Prompt for saving macros.
    if (crawl_state.unsaved_macros && yesno("Save macros?", true, 'n'))
        macro_save();

    clrscr();
    cprintf("Goodbye, %s.", you.your_name.c_str());
    cprintf("\n\n    "); // Space padding where # would go in list format

    string hiscore = hiscores_format_single_long(se, true);

    const int lines = count_occurrences(hiscore, "\n") + 1;

    cprintf("%s", hiscore.c_str());

    cprintf("\nBest Crawlers - %s\n",
            crawl_state.game_type_name().c_str());

    // "- 5" gives us an extra line in case the description wraps on a line.
    hiscores_print_list(get_number_of_lines() - lines - 5);

#ifndef DGAMELAUNCH
    cprintf("\nYou can find your morgue file in the '%s' directory.",
            morgue_directory().c_str());
#endif

    // just to pause, actual value returned does not matter {dlb}
    if (!crawl_state.seen_hups && !crawl_state.disables[DIS_CONFIRMATIONS])
        get_ch();

    if (se.get_death_type() == KILLED_BY_WINNING)
        crawl_state.last_game_won = true;

    game_ended();
}

int actor_to_death_source(const actor* agent)
{
    if (agent->is_player())
        return NON_MONSTER;
    else if (agent->is_monster())
        return agent->as_monster()->mindex();
    else
        return NON_MONSTER;
}

int timescale_damage(const actor *act, int damage)
{
    if (damage < 0)
        damage = 0;
    // Can we have a uniform player/monster speed system yet?
    if (act->is_player())
        return div_rand_round(damage * you.time_taken, BASELINE_DELAY);
    else
    {
        const monster *mons = act->as_monster();
        const int speed = mons->speed > 0? mons->speed : BASELINE_DELAY;
        return div_rand_round(damage * BASELINE_DELAY, speed);
    }
}
