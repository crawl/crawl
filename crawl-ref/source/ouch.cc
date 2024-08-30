/**
 * @file
 * @brief Functions used when Bad Things happen to the player.
**/

#include "AppHdr.h"

#include "ouch.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#ifdef UNIX
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "artefact.h"
#include "act-iter.h"
#include "art-enum.h"
#include "beam.h"
#include "chardump.h"
#include "cloud.h"
#include "colour.h"
#include "database.h"
#include "delay.h"
#include "dgn-event.h"
#include "end.h"
#include "fight.h"
#include "files.h"
#include "fineff.h"
#include "god-abil.h"
#include "god-passive.h"
#include "hints.h"
#include "hiscores.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "libutil.h"
#include "message.h"
#include "mgen-data.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-speak.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "notes.h"
#include "options.h"
#include "output.h"
#include "player.h"
#include "player-stats.h"
#include "potion.h"
#include "prompt.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "shout.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-goditem.h"
#include "spl-selfench.h"
#include "state.h"
#include "stringutil.h"
#include "teleport.h"
#include "transform.h"
#include "tutorial.h"
#include "view.h"
#include "xom.h"

void maybe_melt_player_enchantments(beam_type flavour, int damage)
{
    if (flavour == BEAM_FIRE || flavour == BEAM_LAVA
        || flavour == BEAM_STICKY_FLAME || flavour == BEAM_STEAM)
    {
        if (you.has_mutation(MUT_CONDENSATION_SHIELD))
        {
            if (!you.duration[DUR_ICEMAIL_DEPLETED])
            {
                if (you.has_mutation(MUT_ICEMAIL))
                    mprf(MSGCH_DURATION, "Your icy defences dissipate!");
                else
                    mprf(MSGCH_DURATION, "Your condensation shield dissipates!");
            }
            you.duration[DUR_ICEMAIL_DEPLETED] = ICEMAIL_TIME;
            you.redraw_armour_class = true;
        }

        if (you.duration[DUR_ICY_ARMOUR] > 0)
        {
            you.duration[DUR_ICY_ARMOUR] -= damage * BASELINE_DELAY;
            if (you.duration[DUR_ICY_ARMOUR] <= 0)
                remove_ice_armour();
            else
                you.props[MELT_ARMOUR_KEY] = true;
        }
    }
}

int check_your_resists(int hurted, beam_type flavour, string source,
                       bolt *beam, bool doEffects)
{
    int original = hurted;

    dprf("checking resistance: flavour=%d", flavour);

    string kaux = "";
    if (beam)
    {
        source = beam->get_source_name();
        kaux = beam->name;
    }

    if (doEffects)
        maybe_melt_player_enchantments(flavour, hurted);

    switch (flavour)
    {
    case BEAM_WATER:
        hurted = resist_adjust_damage(&you, flavour, hurted);
        if (!hurted && doEffects)
            mpr("You shrug off the wave.");
        break;

    case BEAM_STEAM:
        hurted = resist_adjust_damage(&you, flavour, hurted);
        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_RESIST);
        else if (hurted > original && doEffects)
        {
            mpr("The steam scalds you terribly!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_FIRE:
        hurted = resist_adjust_damage(&you, flavour, hurted);
        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_RESIST);
        else if (hurted > original && doEffects)
        {
            mpr("The fire burns you terribly!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_DAMNATION:
        break; // sucks to be you (:

    case BEAM_COLD:
        hurted = resist_adjust_damage(&you, flavour, hurted);
        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_RESIST);
        else if (hurted > original && doEffects)
        {
            mpr("You feel a terrible chill!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_STUN_BOLT:
    case BEAM_ELECTRICITY:
    case BEAM_THUNDER:
        hurted = resist_adjust_damage(&you, flavour, hurted);

        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_RESIST);
        break;

    case BEAM_POISON:
        hurted = resist_adjust_damage(&you, flavour, hurted);

        if (doEffects)
        {
            // Ensure that we received a valid beam object before proceeding.
            // See also melee-attack.cc:_print_resist_messages() which cannot be
            // used with this beam type (as it does not provide a valid beam).
            ASSERT(beam);

            int pois = div_rand_round(beam->damage.num * beam->damage.size, 2);
            pois = 3 + random_range(pois * 2 / 3, pois * 4 / 3);

            // If Concentrate Venom is active, we apply the normal amount of
            // poison this beam would have applied on TOP of the curare effect.
            //
            // This is all done through the curare_actor method for better messaging.
            if (beam->origin_spell == SPELL_SPIT_POISON &&
                beam->agent(true)->is_monster() &&
                beam->agent(true)->as_monster()->has_ench(ENCH_CONCENTRATE_VENOM))
            {
                curare_actor(beam->agent(), &you, "concentrated venom",
                             beam->agent(true)->name(DESC_PLAIN), pois);
            }
            else
            {
                poison_player(pois, source, kaux);

                if (player_res_poison() > 0)
                    canned_msg(MSG_YOU_RESIST);
            }
        }

        break;

    case BEAM_POISON_ARROW:
        if (doEffects)
        {
            // Ensure that we received a valid beam object before proceeding.
            // See also melee-attack.cc:_print_resist_messages() which cannot be
            // used with this beam type (as it does not provide a valid beam).
            ASSERT(beam);
            int pois = div_rand_round(beam->damage.num * beam->damage.size, 2);
            pois = 3 + random_range(pois * 2 / 3, pois * 4 / 3);

            const int resist = player_res_poison();
            poison_player((resist ? pois / 2 : pois), source, kaux, true);
        }

        hurted = resist_adjust_damage(&you, flavour, hurted);
        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_PARTIALLY_RESIST);
        break;

    case BEAM_NEG:
        hurted = resist_adjust_damage(&you, flavour, hurted);

        if (doEffects)
        {
            // drain_player handles the messaging here
            drain_player(original, true);
        }
        break;

    case BEAM_ICE:
        hurted = resist_adjust_damage(&you, flavour, hurted);

        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_PARTIALLY_RESIST);
        else if (hurted > original && doEffects)
        {
            mpr("You feel a painful chill!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_LAVA:
        hurted = resist_adjust_damage(&you, flavour, hurted);

        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_PARTIALLY_RESIST);
        else if (hurted > original && doEffects)
        {
            mpr("The lava burns you terribly!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_ACID:
        hurted = resist_adjust_damage(&you, flavour, hurted);
        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_RESIST);
        break;

    case BEAM_MIASMA:
        if (you.res_miasma())
        {
            if (doEffects)
                canned_msg(MSG_YOU_RESIST);
            hurted = 0;
        }
        break;

    case BEAM_HOLY:
    case BEAM_FOUL_FLAME:
    {
        hurted = resist_adjust_damage(&you, flavour, hurted);
        if (hurted < original && doEffects)
            canned_msg(MSG_YOU_RESIST);
        else if (hurted > original && doEffects)
        {
            mpr("You writhe in agony!");
            xom_is_stimulated(200);
        }
        break;
    }

    case BEAM_DEVASTATION:
        if (doEffects)
            you.strip_willpower(beam->agent(), random_range(8, 14));
        break;

    case BEAM_CRYSTALLIZING:
        if (doEffects)
        {
            if (x_chance_in_y(3, 4)) {
                if (!you.duration[DUR_VITRIFIED])
                    mpr("Your body becomes as fragile as glass!");
                else
                    mpr("You feel your fragility will last longer.");
                you.increase_duration(DUR_VITRIFIED, random_range(8, 18), 50);
            }
        }
        break;

    case BEAM_UMBRAL_TORCHLIGHT:
        if (you.holiness() & ~(MH_NATURAL | MH_DEMONIC | MH_HOLY)
            || beam->agent(true)->is_player())
        {
            hurted = 0;
        }
        break;

    case BEAM_WARPING:
        if (doEffects
            && x_chance_in_y(get_warp_space_chance(beam->ench_power), 100))
        {
            you.blink();
        }
        break;

    default:
        break;
    }                           // end switch

    return hurted;
}

/**
 * Handle side-effects for exposure to element other than damage.
 * Historically this handled item destruction, and melting meltable enchantments. Now it takes care of 3 things:
 *   - triggering qazlal's elemental adaptations
 *   - slowing cold-blooded players (draconians, hydra form)
 *   - putting out fires
 * This function should be called exactly once any time a player is exposed to the
 * following elements/beam types: cold, fire, elec, water, steam, lava, BEAM_FRAG. For the sake of Qazlal's
 * elemental adaptation, it should also be called (exactly once) with BEAM_MISSILE when
 * receiving physical damage. Hybrid damage (brands) should call it twice with appropriate
 * flavours.
 *
 * @param flavour The beam type.
 * @param strength The strength of the attack. Used in different ways for different side-effects.
 *     For qazlal_elemental_adapt: (i) it is used for the probability of triggering, and (ii) the resulting length of the effect.
 * @param slow_cold_blooded If True, the beam_type is BEAM_COLD, and the player
 *                          is cold-blooded and not cold-resistant, slow the
 *                          player 50% of the time.
 */
void expose_player_to_element(beam_type flavour, int strength, bool slow_cold_blooded)
{
    dprf("expose_player_to_element, strength %i, flavor %i, slow_cold_blooded is %i", strength, flavour, slow_cold_blooded);
    qazlal_element_adapt(flavour, strength);

    if (flavour == BEAM_COLD && slow_cold_blooded
        && (you.get_mutation_level(MUT_COLD_BLOODED)
            || you.form == transformation::serpent)
        && you.res_cold() <= 0 && coinflip())
    {
        you.slow_down(0, strength);
    }

    if (flavour == BEAM_WATER && you.duration[DUR_STICKY_FLAME])
    {
        mprf(MSGCH_WARN, "The flames go out!");
        end_sticky_flame_player();
    }
}

void lose_level()
{
    // Because you.experience is unsigned long, if it's going to be
    // negative, must die straightaway.
    if (you.experience_level == 1)
    {
        ouch(INSTANT_DEATH, KILLED_BY_DRAINING);
        // Return in case death was cancelled via wizard mode
        return;
    }

    you.experience_level--;

    mprf(MSGCH_WARN,
         "You are now level %d!", you.experience_level);

    calc_hp();
    calc_mp();

    take_note(Note(NOTE_XP_LEVEL_CHANGE, you.experience_level, 0,
        make_stringf("HP: %d/%d MP: %d/%d",
                you.hp, you.hp_max, you.magic_points, you.max_magic_points)));

    you.redraw_title = true;
    you.redraw_experience = true;
#ifdef USE_TILE_LOCAL
    // In case of intrinsic ability changes.
    tiles.layout_statcol();
    redraw_screen();
    update_screen();
#endif

    xom_is_stimulated(200);

    // Kill the player if maxhp <= 0. We can't just move the ouch() call past
    // dec_max_hp() since it would decrease hp twice, so here's another one.
    ouch(0, KILLED_BY_DRAINING);
}

/**
 * Drain the player.
 *
 * @param power             The amount by which to drain the player.
 * @param announce_full     Whether to print messages even when fully resisting
 *                          the drain.
 * @param ignore_protection Whether to ignore the player's rN.
 * @param quiet             Whether to hide all messages that would be printed
 *                          by this.
 * @return                  Whether draining occurred.
 */
bool drain_player(int power, bool announce_full, bool ignore_protection, bool quiet)
{
    if (crawl_state.disables[DIS_AFFLICTIONS])
        return false;

    const int protection = ignore_protection ? 0 : player_prot_life();

    if (protection == 3)
    {
        if (announce_full && !quiet)
            canned_msg(MSG_YOU_RESIST);

        return false;
    }

    if (protection > 0)
    {
        if (!quiet)
            canned_msg(MSG_YOU_PARTIALLY_RESIST);
        power /= (protection * 2);
    }

    if (power > 0)
    {
        const int mhp = 1 + div_rand_round(power * get_real_hp(false, false),
                750);
        you.hp_max_adj_temp -= mhp;
        you.hp_max_adj_temp = max(-(get_real_hp(false, false) - 1),
                you.hp_max_adj_temp);

        dprf("Drained by %d max hp (%d total)", mhp, you.hp_max_adj_temp);
        calc_hp();

        if (!quiet)
            mpr("You feel drained.");
        xom_is_stimulated(15);
        return true;
    }

    return false;
}

static void _xom_checks_damage(kill_method_type death_type,
                               int dam, mid_t death_source)
{
    if (you_worship(GOD_XOM))
    {
        if (death_type == KILLED_BY_TARGETING
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
                 || !monster_by_mid(death_source))
        {
            return;
        }

        int amusementvalue = 1;
        const monster* mons = monster_by_mid(death_source);

        if (!mons->alive())
            return;

        if (mons->wont_attack())
        {
            // Xom thinks collateral damage is funny.
            xom_is_stimulated(200 * dam / (dam + you.hp));
            return;
        }

        int leveldif = mons->get_experience_level() - you.experience_level;
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

        if (player_in_a_dangerous_place())
            amusementvalue += 2;

        amusementvalue /= (you.hp > 0) ? you.hp : 1;

        xom_is_stimulated(amusementvalue);
    }
}

static void _maybe_ru_retribution(int dam, mid_t death_source)
{
    if (will_ru_retaliate())
    {
        // Cap damage to what was enough to kill you. Can matter if
        // you have an extra kitty.
        if (you.hp < 0)
            dam += you.hp;

        monster* mons = monster_by_mid(death_source);
        if (dam <= 0 || !mons || death_source == MID_YOU_FAULTLESS)
            return;

        ru_retribution_fineff::schedule(mons, &you, dam);
    }
}

static void _maybe_inflict_anguish(int dam, mid_t death_source)
{
    const monster* mons = monster_by_mid(death_source);
    if (!mons
        || !mons->alive()
        || !mons->has_ench(ENCH_ANGUISH))
    {
        return;
    }
    anguish_fineff::schedule(mons, dam);
}

static void _maybe_spawn_rats(int dam, kill_method_type death_type)
{
    if (dam <= 0
        || death_type == KILLED_BY_POISON
        || !player_equip_unrand(UNRAND_RATSKIN_CLOAK))
    {
        return;
    }

    // chance rises linearly with damage taken, up to 75% at half hp.
    const int capped_dam = min(dam, you.hp_max / 2);
    if (!x_chance_in_y(capped_dam * 3, you.hp_max * 2))
        return;

    auto rats = { MONS_HELL_RAT, MONS_RIVER_RAT };
    // Choose random in rats where the rat isn't hated by the player's god.
    int seen = 0;
    monster_type mon = MONS_NO_MONSTER;
    for (auto rat : rats)
        if (!god_hates_monster(rat) && one_chance_in(++seen))
            mon = rat;

    // If there's no valid creatures to pull from (e.g., follower of Oka), bail out.
    if (!seen)
        return;

    mgen_data mg(mon, BEH_FRIENDLY, you.pos(), MHITYOU);
    mg.flags |= MG_FORCE_BEH; // don't mention how much it hates you before it appears
    if (monster *m = create_monster(mg))
    {
        m->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, 3));
        mprf("%s scurries out from under your cloak.", m->name(DESC_A).c_str());
        // We should return early in the case of no_love or no_allies,
        // so this is more a sanity check.
        check_lovelessness(*m);
    }
}

static void _maybe_summon_demonic_guardian(int dam, kill_method_type death_type)
{
    if (death_type == KILLED_BY_POISON)
        return;
    // low chance to summon on any hit that dealt damage
    // always tries to summon if the hit did 50% max hp or if we're about to die
    if (you.has_mutation(MUT_DEMONIC_GUARDIAN)
        && (x_chance_in_y(dam, you.hp_max)
            || dam > you.hp_max / 2
            || you.hp * 5 < you.hp_max))
    {
        check_demonic_guardian();
    }
}

// The time-warped blood mutation grants haste to
// your allies when you're brought below half health.
void _maybe_blood_hastes_allies()
{
    if (you.hp * 2 > you.hp_max
        || !you.has_mutation(MUT_TIME_WARPED_BLOOD)
        || you.duration[DUR_TIME_WARPED_BLOOD_COOLDOWN])
    {
        return;
    }

    vector<monster*> targetable;
    int target_count = you.get_mutation_level(MUT_TIME_WARPED_BLOOD) * 2;
    int affected = 0;
    int time = random_range(20, 30);

    you.duration[DUR_TIME_WARPED_BLOOD_COOLDOWN] = 1;

    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        // Try to look for valid allies that aren't already hasted,
        // and which would properly function when given haste.
        if (mi->alive() && mons_attitude(**mi) == ATT_FRIENDLY
            && !mi->berserk_or_frenzied() && you.can_see(**mi)
            && !mi->has_ench(ENCH_HASTE)
            && !mons_is_tentacle_or_tentacle_segment(mi->type)
            && !mons_is_firewood(**mi) && !mons_is_object(mi->type))
        {
            targetable.emplace_back(*mi);
        }
    }

    if (targetable.empty())
    {
       mpr("Your atemporal blood churns to no real effect.");
       return;
    }

    // Affect the highest HD allies you have.
    shuffle_array(targetable);
    sort(targetable.begin(), targetable.end(),
         [](const monster* a, const monster* b)
         {return a->get_hit_dice() > b->get_hit_dice();});

    for (monster *application: targetable)
    {
        if (affected < target_count)
        {
             flash_tile(application->pos(), BLUE, 0);
             animation_delay(15, true);
             application->add_ench(mon_enchant(ENCH_HASTE, 0, &you,
                                   time * BASELINE_DELAY));
             affected++;
        }
    }

    if (affected > 0)
        mpr("The spilling of your atemporal blood hastes your allies!");
}

static void _maybe_spawn_monsters(int dam, kill_method_type death_type,
                                  mid_t death_source)
{
    monster* damager = monster_by_mid(death_source);
    // We need to exclude acid damage and similar things or this function
    // will crash later.
    if (!damager || death_source == MID_YOU_FAULTLESS)
        return;

    monster_type mon;
    int how_many = 0;

    if (have_passive(passive_t::spawn_slimes_on_hit))
    {
        mon = royal_jelly_ejectable_monster();
        if (dam >= you.hp_max * 3 / 4)
            how_many = random2(4) + 2;
        else if (dam >= you.hp_max / 2)
            how_many = random2(2) + 2;
        else if (dam >= you.hp_max / 4)
            how_many = 1;
    }
    else if (you_worship(GOD_XOM)
             && dam >= you.hp_max / 4
             && x_chance_in_y(dam, 3 * you.hp_max))
    {
        mon = MONS_BUTTERFLY;
        how_many = 2 + random2(5);
    }

    if (how_many > 0)
    {
        int count_created = 0;
        for (int i = 0; i < how_many; ++i)
        {
            const int mindex = damager->alive() ? damager->mindex() : MHITNOT;
            mgen_data mg(mon, BEH_FRIENDLY, you.pos(), mindex);
            mg.set_summoned(&you, 2, 0, you.religion);

            if (create_monster(mg))
                count_created++;
        }

        if (count_created > 0)
        {
            if (mon == MONS_BUTTERFLY)
            {
                mprf(MSGCH_GOD, "A shower of butterflies erupts from you!");
                take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "butterfly on damage"), true);
            }
            else
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
    const int level = you.get_mutation_level(MUT_POWERED_BY_PAIN);

    if (level > 0
        && (random2(dam) > 4 + div_rand_round(you.experience_level, 4)
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
                canned_msg(MSG_GAIN_MAGIC);
                inc_mp(mp);
                break;
            }
            break;
        }
        case 2:
            mpr("You focus on the pain.");
            potionlike_effect(POT_MIGHT, level * 20);
            break;
        case 3:
            mpr("You focus on the pain.");
            you.be_agile(level * 20);
            break;
        }
    }
}

static void _maybe_fog(int dam)
{
    const int upper_threshold = you.hp_max / 2;
    if (you_worship(GOD_XOM) && x_chance_in_y(dam, 30 * upper_threshold))
    {
        mprf(MSGCH_GOD, "You emit a cloud of colourful smoke!");
        big_cloud(CLOUD_XOM_TRAIL, &you, you.pos(), 50, 4 + random2(5), -1);
        take_note(Note(NOTE_XOM_EFFECT, you.piety, -1, "smoke on damage"), true);
    }
}

static void _deteriorate(int dam)
{
    if (x_chance_in_y(you.get_mutation_level(MUT_DETERIORATION), 4)
        && dam > you.hp_max / 10)
    {
        mprf(MSGCH_WARN, "Your body deteriorates!");
        lose_stat(STAT_RANDOM, 1);
    }
}

int corrosion_chance(int sources)
{
    return 3 * sources;
}

/**
 * Maybe corrode the player after taking damage if they're wearing *Corrode.
 **/
static void _maybe_corrode()
{
    int corrosion_sources = you.scan_artefacts(ARTP_CORRODE);
    if (x_chance_in_y(corrosion_chance(corrosion_sources), 100))
        you.corrode_equipment("Your corrosive artefact");
}

/**
 * Maybe slow the player after taking damage if they're wearing *Slow.
 **/
static void _maybe_slow()
{
    int slow_sources = you.scan_artefacts(ARTP_SLOW);
    if (x_chance_in_y(slow_sources, 100))
        slow_player(10 + random2(5));
}

/**
 * Maybe disable scrolls after taking damage if the player has MUT_READ_SAFETY.
 **/
static void _maybe_disable_scrolls()
{
    int mut_level = you.get_mutation_level(MUT_READ_SAFETY);
    if (mut_level && !you.duration[DUR_NO_SCROLLS] && x_chance_in_y(mut_level, 100))
    {
        mpr("You feel threatened and lose the ability to read scrolls!");
        you.increase_duration(DUR_NO_SCROLLS, 10 + random2(5));
    }
}

/**
 * Maybe disable potions after taking damage if the player has MUT_DRINK_SAFETY.
 **/
static void _maybe_disable_potions()
{
    int mut_level = you.get_mutation_level(MUT_DRINK_SAFETY);
    if (mut_level && !you.duration[DUR_NO_POTIONS] && x_chance_in_y(mut_level, 100))
    {
        mpr("You feel threatened and lose the ability to drink potions!");
        you.increase_duration(DUR_NO_POTIONS, 10 + random2(5));
    }
}

static void _place_player_corpse(bool explode)
{
    if (!in_bounds(you.pos()))
        return;

    monster dummy;
    dummy.type = player_mons(false);
    define_monster(dummy); // assumes player_mons is not a zombie
    dummy.position = you.pos();
    dummy.props[ALWAYS_CORPSE_KEY] = true;
    dummy.mname = you.your_name;
    dummy.set_hit_dice(you.experience_level);
    if (explode)
        dummy.flags &= MF_EXPLODE_KILL;

    if (you.form != transformation::none)
        mpr("Your shape twists and changes as you die.");

    place_monster_corpse(dummy, false);
}

#if defined(WIZARD) || defined(DEBUG)
static void _wizard_restore_life()
{
    if (you.hp_max <= 0)
        undrain_hp(9999);
    while (you.hp_max <= 0)
        you.hp_max_adj_perm++, calc_hp();
    if (you.hp <= 0)
        set_hp(you.hp_max);
}
#endif

int outgoing_harm_amount(int levels)
{
    // +30% damage if opp has one level of harm, +45% with two
    return 15 * (levels + 1);
}

int incoming_harm_amount(int levels)
{
    // +20% damage if you have one level of harm, +30% with two
    return 10 * (levels + 1);
}

static int _apply_extra_harm(int dam, mid_t source)
{
    monster* damager = monster_by_mid(source);
    if (damager && damager->extra_harm())
        dam = dam * (100 + outgoing_harm_amount(damager->extra_harm())) / 100;
    if (you.extra_harm())
        dam = dam * (100 + incoming_harm_amount(you.extra_harm())) / 100;
    return dam;
}

void reset_damage_counters()
{
    you.turn_damage = 0;
    you.damage_source = NON_MONSTER;
    you.source_damage = 0;
}

#if TAG_MAJOR_VERSION == 34
bool can_shave_damage()
{
    return you.species == SP_DEEP_DWARF || you.species == SP_MAYFLYTAUR;
}

int do_shave_damage(int dam)
{
    if (!can_shave_damage())
        return dam;

    // Deep Dwarves get to shave any hp loss.
    int shave = 1 + random2(2 + random2(1 + you.experience_level / 3));
    dprf("HP shaved: %d.", shave);
    dam -= shave;

    return dam;
}
#endif

/// Let Sigmund crow in triumph.
static void _triumphant_mons_speech(actor *killer)
{
    if (!killer || !killer->alive())
        return;

    monster* mon = killer->as_monster();
    if (mon && !mon->wont_attack())
        mons_speaks(mon);  // They killed you and they meant to.
}

static void _god_death_messages(kill_method_type death_type,
                                const actor *killer)
{
    const bool left_corpse = death_type != KILLED_BY_DISINT
                             && death_type != KILLED_BY_LAVA;

    const mon_holy_type holi = you.holiness();
    const bool was_undead = bool(holi & MH_UNDEAD);
    const bool was_nonliving = bool(holi & MH_NONLIVING);

    string key = god_name(you.religion) + " death";

    string key_extended = key;
    if (left_corpse)
        key_extended += " corpse";
    if (was_undead)
        key_extended += " undead";
    if (was_nonliving)
        key_extended += " nonliving";

    // For gods with death messages in the database, first try key_extended.
    // If that doesn't produce anything, try key.
    //
    // This means that the default god death message is "@God_name@ death".
    string result = getSpeakString(key_extended);
    if (result.empty())
        result = getSpeakString(key);
    if (!result.empty())
        god_speaks(you.religion, result.c_str());

    xom_death_message(death_type);

    if (left_corpse)
    {
        if (will_have_passive(passive_t::goldify_corpses))
            mprf(MSGCH_GOD, "Your body crumbles into a pile of gold.");

        if (you.religion == GOD_NEMELEX_XOBEH)
            nemelex_death_message();
    }

    if (killer)
    {
        // If you ever worshipped Beogh, and you get killed by a Beogh
        // worshipper, Beogh will appreciate it.
        if (you.worshipped[GOD_BEOGH] && killer->is_monster()
            && killer->deity() == GOD_BEOGH)
        {
            string msg;
            if (you.religion == GOD_BEOGH)
            {
                msg = " appreciates " + killer->name(DESC_ITS)
                        + " killing of a heretic priest.";
            }
            else
            {
                msg = " appreciates " + killer->name(DESC_ITS)
                        + " killing of an apostate.";
            }
            simple_god_message(msg.c_str(), false, GOD_BEOGH);
        }

        // Doesn't depend on Okawaru worship - you can still lose the duel
        // after abandoning.
        if (killer->props.exists(OKAWARU_DUEL_TARGET_KEY))
        {
            const string msg = " crowns " + killer->name(DESC_THE, true)
                                + " victorious!";
            simple_god_message(msg.c_str(), false, GOD_OKAWARU);
        }
    }
}

static void _print_endgame_messages(scorefile_entry &se)
{
    const kill_method_type death_type = (kill_method_type) se.get_death_type();
    const bool non_death = death_type == KILLED_BY_QUITTING
                        || death_type == KILLED_BY_WINNING
                        || death_type == KILLED_BY_LEAVING;
    if (non_death)
        return;


    canned_msg(MSG_YOU_DIE);

    actor* killer = se.killer();
    _triumphant_mons_speech(killer);
    _god_death_messages(death_type, killer);

    flush_prev_message();
    viewwindow(); // don't do for leaving/winning characters
    update_screen();

    if (crawl_state.game_is_hints())
        hints_death_screen();
}

/** Hurt the player. Isn't it fun?
 *
 *  @param dam How much damage -- may be INSTANT_DEATH.
 *  @param death_type how did you get hurt?
 *  @param source who could do such a thing?
 *  @param aux what did they do it with?
 *  @param see_source whether the attacker was visible to you
 *  @param death_source_name the attacker's name if it is already dead.
 */
void ouch(int dam, kill_method_type death_type, mid_t source, const char *aux,
          bool see_source, const char *death_source_name)
{
    ASSERT(!crawl_state.game_is_arena());
    if (you.duration[DUR_TIME_STEP])
        return;

    if (you.pending_revival)
        return;

    int drain_amount = 0;

    // Marionettes will never hurt the player with their spells (even if they
    // have somehow killed themselves in the process)
    if (monster* mon_source = cached_monster_copy_by_mid(source))
    {
        if (mon_source->attitude == ATT_MARIONETTE)
            dam = 0;
    }

    // Multiply damage if Harm or Vitrify is in play. (Poison is multiplied earlier.)
    if (dam != INSTANT_DEATH && death_type != KILLED_BY_POISON)
    {
        dam = _apply_extra_harm(dam, source);

        if (you.duration[DUR_VITRIFIED])
            dam = dam * 150 / 100;
    }

#if TAG_MAJOR_VERSION == 34
    if (can_shave_damage() && dam != INSTANT_DEATH
        && death_type != KILLED_BY_POISON)
    {
        dam = max(0, do_shave_damage(dam));
    }
#endif

    if (dam != INSTANT_DEATH)
    {
        if (you.form == transformation::slaughter)
            dam = dam * 10 / 15;
        if (you.may_pruneify() && you.cannot_act())
            dam /= 2;
        if (you.petrified())
            dam /= 2;
        else if (you.petrifying())
            dam = dam * 10 / 15;
    }
    ait_hp_loss hpl(dam, death_type);
    interrupt_activity(activity_interrupt::hp_loss, &hpl);

    // Don't wake the player with fatal or poison damage.
    if (dam > 0 && dam < you.hp && death_type != KILLED_BY_POISON)
        you.check_awaken(500);

    const bool non_death = death_type == KILLED_BY_QUITTING
                        || death_type == KILLED_BY_WINNING
                        || death_type == KILLED_BY_LEAVING;

    // certain effects (e.g. drowned souls) use KILLED_BY_WATER for flavour
    // reasons (morgue messages?), with regrettable consequences if we don't
    // double-check.
    const bool env_death = source == MID_NOBODY
                           && (death_type == KILLED_BY_LAVA
                               || death_type == KILLED_BY_WATER);

    if (!env_death && !non_death && death_type != KILLED_BY_ZOT
        && you.hp_max > 0)
    {
        // death's door protects against everything but falling into
        // water/lava, Zot, excessive rot, leaving the dungeon, or quitting.
        // Likewise, dreamshard protects you until the start of your next turn.
        if (you.duration[DUR_DEATHS_DOOR] || you.props.exists(DREAMSHARD_KEY))
            return;
        // the dreamshard necklace protects from any fatal blow or death source
        // that death's door would protect from.
        else if (player_equip_unrand(UNRAND_DREAMSHARD_NECKLACE)
                 && dam >= you.hp)
        {
            dreamshard_shatter();
            return;
        }
    }

    if (dam != INSTANT_DEATH)
    {
        if (you.spirit_shield() && death_type != KILLED_BY_POISON
            && !(aux && strstr(aux, FLAY_DAMAGE_KEY)))
        {
            // round off fairly (important for taking 1 damage at a time)
            int mp = div_rand_round(dam * you.magic_points,
                                    max(you.hp + you.magic_points, 1));
            // but don't kill the player with round-off errors
            mp = max(mp, dam + 1 - you.hp);
            mp = min(mp, you.magic_points);

            dam -= mp;
            drain_mp(mp, true);

            // Wake players who took fatal damage exactly equal to current HP,
            // but had it reduced below fatal threshold by spirit shield.
            if (dam < you.hp)
                you.check_awaken(500);

            if (dam <= 0 && you.hp > 0)
                return;
        }

        if (dam >= you.hp && you.hp_max > 0 && god_protects_from_harm())
        {
            simple_god_message(" protects you from harm!");
            // Ensure divine intervention wakes sleeping players. Necessary
            // because we otherwise don't wake players who take fatal damage.
            you.check_awaken(500);
            return;
        }

        you.turn_damage += dam;
        if (you.damage_source != source)
        {
            you.damage_source = source;
            you.source_damage = 0;
        }
        you.source_damage += dam;

        dec_hp(dam, true);

        // Even if we have low HP messages off, we'll still give a
        // big hit warning (in this case, a hit for half our HPs) -- bwr
        if (dam > 0 && you.hp_max <= dam * 2)
            mprf(MSGCH_DANGER, "Ouch! That really hurt!");

        if (you.hp > 0 && dam > 0)
        {
            if (death_type != KILLED_BY_POISON || poison_is_lethal())
                flush_hp();

            hints_healing_check();

            _xom_checks_damage(death_type, dam, source);

            // for note taking
            string damage_desc;
            if (!see_source)
                damage_desc = make_stringf("something (%d)", dam);
            else
            {
                damage_desc = scorefile_entry(dam, source,
                                              death_type, aux, true)
                    .death_description(scorefile_entry::DDV_TERSE);
            }

            take_note(Note(NOTE_HP_CHANGE, you.hp, you.hp_max,
                           damage_desc.c_str()));

            _deteriorate(dam);
            _maybe_ru_retribution(dam, source);
            _maybe_inflict_anguish(dam, source);
            _maybe_spawn_monsters(dam, death_type, source);
            _maybe_spawn_rats(dam, death_type);
            _maybe_summon_demonic_guardian(dam, death_type);
            _maybe_fog(dam);
            _maybe_blood_hastes_allies();
            _powered_by_pain(dam);
            makhleb_celebrant_bloodrite();
            if (sanguine_armour_valid())
                activate_sanguine_armour();
            refresh_meek_bonus();
            if (death_type != KILLED_BY_POISON)
            {
                _maybe_corrode();
                _maybe_slow();
                _maybe_disable_scrolls();
                _maybe_disable_potions();
            }
            if (drain_amount > 0)
                drain_player(drain_amount, true, true);
        }
        if (you.hp > 0)
            return;
    }

    // Is the player being killed by a direct act of Xom?
    if (crawl_state.is_god_acting()
        && crawl_state.which_god_acting() == GOD_XOM
        && crawl_state.other_gods_acting().empty())
    {
        you.escaped_death_cause = death_type;
        you.escaped_death_aux   = aux == nullptr ? "" : aux;

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
        if (aux == nullptr || !*aux)
        {
            if (death_type != KILLED_BY_XOM)
                aux = "Xom";
        }
        else if (strstr(aux, "Xom") == nullptr)
            death_type = KILLED_BY_XOM;
    }
    // Xom may still try to save your life.
    else if (xom_saves_your_life(death_type))
        return;

#if defined(WIZARD) || defined(DEBUG)
    if (!non_death && crawl_state.disables[DIS_DEATH])
    {
        _wizard_restore_life();
        return;
    }
#endif

    crawl_state.cancel_cmd_all();

    // Construct scorefile entry.
    scorefile_entry se(dam, source, death_type, aux, false,
                       death_source_name);

#ifdef WIZARD
    if (!non_death)
    {
        if (crawl_state.test || you.wizard || you.suppress_wizard || (you.explore && !you.lives))
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
                take_note(Note(NOTE_DEATH, you.hp, you.hp_max,
                                "You cheat death using unusual wizardly powers."), true);
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

        // only quitting can lead to this?
        screen_end_game("", game_exit::quit);
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
        you.pending_revival = true;

        take_note(Note(NOTE_LOSE_LIFE, you.lives));

        stop_delay(true);

        // You wouldn't want to lose this accomplishment to a crash, would you?
        // Especially if you manage to trigger one via lua somehow...
        if (!crawl_state.disables[DIS_SAVE_CHECKPOINTS])
            save_game(false);

        canned_msg(MSG_YOU_DIE);
        xom_death_message((kill_method_type) se.get_death_type());
        more();

        if (death_type != KILLED_BY_ZOT)
            _place_player_corpse(death_type == KILLED_BY_DISINT);
        return;
    }

    // Prevent bogus notes.
    activate_notes(false);
    _print_endgame_messages(se);
    end_game(se);
}

string morgue_name(string char_name, time_t when_crawl_got_even)
{
    string name = "morgue-" + char_name;

    string time = make_file_time(when_crawl_got_even);
    if (!time.empty())
        name += "-" + time;

    return name;
}

int actor_to_death_source(const actor* agent)
{
    return agent ? agent->mindex() : NON_MONSTER;
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
