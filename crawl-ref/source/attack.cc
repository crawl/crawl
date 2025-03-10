/*
 *  File:       attack.cc
 *  Summary:    Methods of the attack class, generalized functions which may
 *              be overloaded by inheriting classes.
 *  Written by: Robert Burnham
 */

#include "AppHdr.h"

#include "attack.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>

#include "art-enum.h"
#include "chardump.h"
#include "delay.h"
#include "english.h"
#include "env.h"
#include "exercise.h"
#include "fight.h"
#include "fineff.h"
#include "god-abil.h"
#include "god-conduct.h"
#include "god-passive.h" // passive_t::no_haste
#include "item-name.h"
#include "item-prop.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-clone.h"
#include "mon-death.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "pronoun-type.h"
#include "religion.h"
#include "shout.h"
#include "skills.h"
#include "spl-damage.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "tag-version.h"
#include "transform.h"
#include "xom.h"

/*
 **************************************************
 *             BEGIN PUBLIC FUNCTIONS             *
 **************************************************
*/
attack::attack(actor *attk, actor *defn, actor *blame)
    : attacker(attk), defender(defn), responsible(blame ? blame : attk),
      attack_occurred(false), cancel_attack(false), did_hit(false),
      needs_message(false), attacker_visible(false), defender_visible(false),
      perceived_attack(false), obvious_effect(false), to_hit(0),
      damage_done(0), special_damage(0), aux_damage(0),
      special_damage_flavour(BEAM_NONE),
      stab_attempt(false), stab_bonus(0), ev_margin(0), weapon(nullptr),
      damage_brand(SPWPN_NORMAL), wpn_skill(SK_UNARMED_COMBAT),
      art_props(0), unrand_entry(nullptr),
      attacker_to_hit_penalty(0), attack_verb("bug"), verb_degree(),
      no_damage_message(), special_damage_message(), aux_attack(), aux_verb(),
      defender_shield(nullptr), simu(false),
      aux_source(""), kill_type(KILLED_BY_MONSTER)
{
    // No effective code should execute, we'll call init_attack again from
    // the child class, since initializing an attack will vary based the within
    // type of attack actually being made (melee, ranged, etc.)
}

bool attack::handle_phase_attempted()
{
    return true;
}

bool attack::handle_phase_blocked()
{
    damage_done = 0;

    if (attacker->is_player())
        behaviour_event(defender->as_monster(), ME_WHACK, attacker);

    // Use up a charge of Divine Shield, if active.
    if (defender->is_player())
        tso_expend_divine_shield_charge();

    return true;
}

bool attack::handle_phase_damaged()
{
    // We have to check in_bounds() because removed kraken tentacles are
    // temporarily returned to existence (without a position) when they
    // react to damage.
    if (defender->has_blood()
        && !defender->is_summoned()
        && in_bounds(defender->pos())
        && !simu)
    {
        int blood = damage_done;
        if (blood > defender->stat_hp())
            blood = defender->stat_hp();
        if (blood)
            blood_fineff::schedule(defender, defender->pos(), blood);
    }

    announce_hit();
    // Inflict stored damage
    damage_done = inflict_damage(damage_done);

    // TODO: Unify these, added here so we can get rid of player_attack
    if (attacker->is_player())
    {
        if (damage_done)
            player_exercise_combat_skills();
    }
    else
    {
        if (!mons_attack_effects())
            return false;
    }

    // It's okay if a monster took lethal damage, but we should stop
    // the combat if it was already reset (e.g. a spectral weapon that
    // took damage and then noticed that its caster is gone).
    return defender->is_player() || !invalid_monster(defender->as_monster());
}

bool attack::handle_phase_killed()
{
    monster* mon = defender->as_monster();
    if (!invalid_monster(mon))
    {
        // Was this a reflected missile from the player?
        if (responsible->mid == MID_YOU_FAULTLESS)
            monster_die(*mon, KILL_YOU_MISSILE, YOU_FAULTLESS);
        else
            monster_die(*mon, attacker);
    }

    return true;
}

bool attack::handle_phase_end()
{
    maybe_trigger_fugue_wail(defender->pos());

    return true;
}

/**
 * Calculate the to-hit for an attacker before the main die roll.
 *
 * @param random If false, calculate average to-hit deterministically.
 */
int attack::calc_pre_roll_to_hit(bool random)
{
    if (using_weapon()
        && (is_unrandom_artefact(*weapon, UNRAND_WOE)
            || is_unrandom_artefact(*weapon, UNRAND_SNIPER)))
    {
        return AUTOMATIC_HIT;
    }
    int mhit;

    // This if statement is temporary, it should be removed when the
    // implementation of a more universal (and elegant) to-hit calculation
    // is designed. The actual code is copied from the old mons_to_hit and
    // player_to_hit methods.
    if (stat_source().is_player())
    {
        mhit = 15 + (you.dex() / 2);
        // fighting contribution
        mhit += maybe_random2_div(you.skill(SK_FIGHTING, 100), 100, random);

        // weapon skill contribution
        if (using_weapon())
        {
            if (wpn_skill != SK_FIGHTING)
            {
                if (you.skill(wpn_skill) < 1 && player_in_a_dangerous_place() && random)
                    xom_is_stimulated(10); // Xom thinks that is mildly amusing.

                mhit += maybe_random2_div(you.skill(wpn_skill, 100), 100,
                                         random);
            }
        }
        else if (you.form_uses_xl())
            mhit += maybe_random2_div(you.experience_level * 100, 100, random);
        else
        {
            // UC gets extra acc to compensate for lack of weapon enchantment.
            if (wpn_skill == SK_UNARMED_COMBAT)
                mhit += 6;

            mhit += maybe_random2_div(you.skill(wpn_skill, 100), 100,
                                     random);
        }

        // weapon bonus contribution
        if (using_weapon())
        {
            if (weapon->base_type == OBJ_WEAPONS)
            {
                mhit += weapon->plus;
                mhit += property(*weapon, PWPN_HIT);
            }
            else if (weapon->base_type == OBJ_STAVES)
                mhit += property(*weapon, PWPN_HIT);
        }

        // slaying bonus
        mhit += slaying_bonus(wpn_skill == SK_THROWING, random);

        // vertigo penalty
        if (you.duration[DUR_VERTIGO])
            mhit -= 5;

        // mutation
        if (you.get_mutation_level(MUT_EYEBALLS))
            mhit += 2 * you.get_mutation_level(MUT_EYEBALLS) + 1;
    }
    else    // Monster to-hit.
    {
        mhit = calc_mon_to_hit_base();
        if (using_weapon())
            mhit += weapon->plus + property(*weapon, PWPN_HIT);

        const int jewellery = attacker->as_monster()->inv[MSLOT_JEWELLERY];
        if (jewellery != NON_ITEM
            && env.item[jewellery].is_type(OBJ_JEWELLERY, RING_SLAYING))
        {
            mhit += env.item[jewellery].plus;
        }

        mhit += attacker->scan_artefacts(ARTP_SLAYING);
    }

    return mhit;
}

/**
 * Calculate to-hit modifiers for an attacker that apply after the player's roll.
 *
 * @param mhit The post-roll player's to-hit value.
 */
int attack::post_roll_to_hit_modifiers(int mhit, bool /*random*/)
{
    int modifiers = 0;

    // Penalties for both players and monsters:
    modifiers -= attacker->inaccuracy_penalty();

    if (attacker->confused())
        modifiers += CONFUSION_TO_HIT_MALUS;

    // If no defender, we're calculating to-hit for debug-display
    // purposes, so don't drop down to defender code below
    if (defender == nullptr)
        return modifiers;

    if (!defender->visible_to(attacker))
    {
        if (attacker->is_player())
            modifiers -= 6;
        else
            modifiers -= mhit * 35 / 100;
    }
    else
    {
        // This can only help if you're visible!
        const int how_transparent = you.get_mutation_level(MUT_TRANSLUCENT_SKIN);
        if (defender->is_player() && how_transparent)
            modifiers += TRANSLUCENT_SKIN_TO_HIT_MALUS * how_transparent;

        // defender backlight bonus and umbra penalty.
        if (defender->backlit(false))
            modifiers += BACKLIGHT_TO_HIT_BONUS;
        if (!attacker->nightvision() && defender->umbra())
            modifiers += UMBRA_TO_HIT_MALUS;
    }

    return modifiers;
}

/**
 * Calculate the to-hit for an attacker
 *
 * @param random If false, calculate average to-hit deterministically.
 */
int attack::calc_to_hit(bool random)
{
    int mhit = calc_pre_roll_to_hit(random);
    if (mhit == AUTOMATIC_HIT)
        return AUTOMATIC_HIT;

    // hit roll
    const actor &src = stat_source();
    if (src.is_player())
        mhit = maybe_random2(mhit, random);

    mhit += post_roll_to_hit_modifiers(mhit, random);

    // We already did this roll for players.
    if (!src.is_player())
        mhit = maybe_random2(mhit + 1, random);

    dprf(DIAG_COMBAT, "%s: to-hit: %d",
         attacker->name(DESC_PLAIN).c_str(), mhit);

    return mhit;
}

/* Returns an actor's name
 *
 * Takes into account actor visibility/invisibility and the type of description
 * to be used (capitalization, possessiveness, etc.)
 */
string attack::actor_name(const actor *a, description_level_type desc,
                          bool actor_visible)
{
    return actor_visible ? a->name(desc) : anon_name(desc);
}

/* Returns an actor's pronoun
 *
 * Takes into account actor visibility
 */
string attack::actor_pronoun(const actor *a, pronoun_type pron,
                             bool actor_visible)
{
    return actor_visible ? a->pronoun(pron) : anon_pronoun(pron);
}

/* Returns an anonymous actor's name
 *
 * Given the actor visible or invisible, returns the
 * appropriate possessive pronoun.
 */
string attack::anon_name(description_level_type desc)
{
    switch (desc)
    {
    case DESC_NONE:
        return "";
    case DESC_YOUR:
    case DESC_ITS:
        return "something's";
    case DESC_THE:
    case DESC_A:
    case DESC_PLAIN:
    default:
        return "something";
    }
}

/* Returns an anonymous actor's pronoun
 *
 * Given invisibility (whether out of LOS or just invisible), returns the
 * appropriate possessive, inflexive, capitalised pronoun.
 */
string attack::anon_pronoun(pronoun_type pron)
{
    return decline_pronoun(GENDER_NEUTER, pron);
}

/* Initializes an attack, setting up base variables and values
 *
 * Does not make any changes to any actors, items, or the environment,
 * in case the attack is cancelled or otherwise fails. Only initializations
 * that are universal to all types of attacks should go into this method,
 * any initialization properties that are specific to one attack or another
 * should go into their respective init_attack.
 *
 * Although this method will get overloaded by the derived class, we are
 * calling it from attack::attack(), before the overloading has taken place.
 */
void attack::init_attack(skill_type unarmed_skill, int attack_number)
{
    ASSERT(attacker);

    wpn_skill       = weapon ? item_attack_skill(*weapon) : unarmed_skill;

    if (attacker->is_player() && you.form_uses_xl())
        wpn_skill = SK_FIGHTING; // for stabbing, mostly

    to_hit          = calc_to_hit(true);

    defender_shield = defender ? defender->shield() : defender_shield;

    unrand_entry = nullptr;
    if (weapon && weapon->base_type == OBJ_WEAPONS && is_artefact(*weapon))
    {
        artefact_properties(*weapon, art_props);
        if (is_unrandom_artefact(*weapon))
            unrand_entry = get_unrand_entry(weapon->unrand_idx);
    }

    attacker_visible   = attacker->observable();
    defender_visible   = defender && defender->observable();
    needs_message      = (attacker_visible || defender_visible);

    if (attacker->is_monster())
    {
        mon_attack_def mon_attk = mons_attack_spec(*attacker->as_monster(),
                                                   attack_number,
                                                   false);

        attk_type       = mon_attk.type;
        attk_flavour    = mon_attk.flavour;

        // Don't scale damage for YOU_FAULTLESS etc.
        if (attacker->get_experience_level() == 0)
            attk_damage = mon_attk.damage;
        else
        {
            attk_damage = div_rand_round(mon_attk.damage
                                             * attacker->get_hit_dice(),
                                         attacker->get_experience_level());
        }

        if (attk_type == AT_WEAP_ONLY)
        {
            int weap = attacker->as_monster()->inv[MSLOT_WEAPON];
            if (weap == NON_ITEM || is_range_weapon(env.item[weap]))
                attk_type = AT_NONE;
            else
                attk_type = AT_HIT;
        }
        else if (attk_type == AT_TRUNK_SLAP && attacker->type == MONS_SKELETON)
        {
            // Elephant trunks have no bones inside.
            attk_type = AT_NONE;
        }
    }
    else
    {
        attk_type    = AT_HIT;
        attk_flavour = AF_PLAIN;
    }
}

void attack::alert_defender()
{
    // Allow monster attacks to draw the ire of the defender. Player
    // attacks are handled elsewhere.
    if (perceived_attack
        && defender->is_monster()
        && attacker->is_monster()
        && attacker->alive() && defender->alive()
        && (defender->as_monster()->foe == MHITNOT || one_chance_in(3)))
    {
        behaviour_event(defender->as_monster(), ME_WHACK, attacker);
    }

    // If an enemy attacked a friend, set the pet target if it isn't set
    // already, but not if sanctuary is in effect (pet target must be
    // set explicitly by the player during sanctuary).
    if (perceived_attack && attacker->alive()
        && (defender->is_player() || defender->as_monster()->friendly())
        && !attacker->is_player()
        && !crawl_state.game_is_arena()
        && !attacker->as_monster()->wont_attack())
    {
        if (defender->is_player())
        {
            interrupt_activity(activity_interrupt::monster_attacks,
                               attacker->as_monster());
        }
        if (you.pet_target == MHITNOT && env.sanctuary_time <= 0)
            you.pet_target = attacker->mindex();
    }
}

bool attack::distortion_affects_defender()
{
    enum disto_effect
    {
        SMALL_DMG,
        BIG_DMG,
        BANISH,
        BLINK,
        NONE
    };

    const disto_effect choice = random_choose_weighted(35, SMALL_DMG,
                                                       25, BIG_DMG,
                                                       5, BANISH,
                                                       20, BLINK,
                                                       15,  NONE);

    if (simu && !(choice == SMALL_DMG || choice == BIG_DMG))
        return false;

    switch (choice)
    {
    case SMALL_DMG:
        special_damage += 1 + random2avg(7, 2);
        special_damage_message = make_stringf("Space warps around %s%s",
                                              defender_name(false).c_str(),
                                              attack_strength_punctuation(special_damage).c_str());
        break;
    case BIG_DMG:
        special_damage += 3 + random2avg(24, 2);
        special_damage_message =
            make_stringf("Space warps horribly around %s%s",
                         defender_name(false).c_str(),
                         attack_strength_punctuation(special_damage).c_str());
        break;
    case BLINK:
        if (defender_visible)
            obvious_effect = true;
        if (!defender->no_tele())
            blink_fineff::schedule(defender);
        break;
    case BANISH:
        if (defender_visible)
            obvious_effect = true;
        defender->banish(attacker, attacker->name(DESC_PLAIN, true),
                         attacker->get_experience_level());
        return true;
    case NONE:
        // Do nothing
        break;
    }

    return false;
}

void attack::antimagic_affects_defender(int pow)
{
    obvious_effect =
        enchant_actor_with_flavour(defender, attacker, BEAM_DRAIN_MAGIC, pow);
}

void attack::pain_affects_defender()
{
    actor &user = stat_source();
    if (!one_chance_in(user.skill_rdiv(SK_NECROMANCY) + 1))
    {
        special_damage += resist_adjust_damage(defender, BEAM_NEG,
                              random2(1 + user.skill_rdiv(SK_NECROMANCY)));

        if (special_damage && defender_visible)
        {
            special_damage_message =
                make_stringf("%s %s in agony%s",
                             defender->name(DESC_THE).c_str(),
                             defender->conj_verb("writhe").c_str(),
                           attack_strength_punctuation(special_damage).c_str());
        }
    }
}

struct chaos_attack_type
{
    attack_flavour flavour;
    brand_type brand;
    int chance;
    function<bool(const actor& def)> valid;
};

// Chaos melee attacks randomly choose a brand from here, with brands that
// definitely won't affect the target being invalid. Chaos itself should
// always be a valid option, triggering a more unpredictable chaos_effect
// instead of a normal attack brand when selected.
static const vector<chaos_attack_type> chaos_types = {
    { AF_FIRE,      SPWPN_FLAMING,       10,
      [](const actor &d) { return d.is_player() || d.res_fire() < 3; } },
    { AF_COLD,      SPWPN_FREEZING,      10,
      [](const actor &d) { return d.is_player() || d.res_cold() < 3; } },
    { AF_ELEC,      SPWPN_ELECTROCUTION, 10,
      [](const actor &d) { return d.is_player() || d.res_elec() <= 0; } },
    { AF_POISON,    SPWPN_VENOM,         10,
      [](const actor &d) {
          return !(d.holiness() & (MH_UNDEAD | MH_NONLIVING)); } },
    { AF_CHAOTIC,   SPWPN_CHAOS,         13,
      nullptr },
    { AF_DRAIN,  SPWPN_DRAINING,         5,
      [](const actor &d) { return d.res_negative_energy() < 3; } },
    { AF_VAMPIRIC,  SPWPN_VAMPIRISM,     5,
      [](const actor &d) {
          return actor_is_susceptible_to_vampirism(d); } },
    { AF_HOLY,      SPWPN_HOLY_WRATH,    5,
      [](const actor &d) { return d.holy_wrath_susceptible(); } },
    { AF_ANTIMAGIC, SPWPN_ANTIMAGIC,     5,
      [](const actor &d) { return d.antimagic_susceptible(); } },
    { AF_FOUL_FLAME, SPWPN_FOUL_FLAME,   2,
      [](const actor &d) { return d.res_foul_flame() < 3; } },
};

brand_type attack::random_chaos_brand()
{
    vector<pair<brand_type, int>> weights;
    for (const chaos_attack_type &choice : chaos_types)
        if (!choice.valid || choice.valid(*defender))
        {
            // Don't use vampiric brand if the attacker is at full health.
            if (choice.brand != SPWPN_VAMPIRISM
                || attacker->stat_hp() != attacker->stat_maxhp())
            {
                weights.push_back({choice.brand, choice.chance});
            }
        }

    ASSERT(!weights.empty());

    brand_type brand = *random_choose_weighted(weights);

#ifdef NOTE_DEBUG_CHAOS_BRAND
    string brand_name = "CHAOS brand: ";
    switch (brand)
    {
    case SPWPN_FLAMING:         brand_name += "flaming"; break;
    case SPWPN_FREEZING:        brand_name += "freezing"; break;
    case SPWPN_ELECTROCUTION:   brand_name += "electrocution"; break;
    case SPWPN_VENOM:           brand_name += "venom"; break;
    case SPWPN_CHAOS:           brand_name += "chaos"; break;
    case SPWPN_DRAINING:        brand_name += "draining"; break;
    case SPWPN_VAMPIRISM:       brand_name += "vampirism"; break;
    case SPWPN_HOLY_WRATH:      brand_name += "holy wrath"; break;
    case SPWPN_ANTIMAGIC:       brand_name += "antimagic"; break;
    case SPWPN_FOUL_FLAME:      brand_name += "foul flame"; break;
    default:                    brand_name += "BUGGY"; break;
    }

    // Pretty much duplicated by the chaos effect note,
    // which will be much more informative.
    if (brand != SPWPN_CHAOS)
        take_note(Note(NOTE_MESSAGE, 0, 0, brand_name), true);
#endif
    return brand;
}

attack_flavour attack::random_chaos_attack_flavour()
{
    vector<pair<attack_flavour, int>> weights;
    for (const chaos_attack_type &choice : chaos_types)
        if (!choice.valid || choice.valid(*defender))
        {
            // Again, don't use vampiric brand if the attacker is at full hp.
            if (choice.brand != SPWPN_VAMPIRISM
                || attacker->stat_hp() != attacker->stat_maxhp())
            {
                weights.push_back({choice.flavour, choice.chance});
            }
        }

    ASSERT(!weights.empty());

    return *random_choose_weighted(weights);
}

void attack::drain_defender()
{
    if (defender->is_monster() && coinflip())
        return;

    if (!(defender->holiness() & (MH_NATURAL | MH_PLANT)))
        return;

    special_damage = resist_adjust_damage(defender, BEAM_NEG,
                                          (1 + random2(damage_done)) / 2);

    if (defender->drain(attacker, true, 1 + damage_done))
    {
        if (defender->is_player())
            obvious_effect = true;
        else if (defender_visible)
        {
            special_damage_message =
                make_stringf(
                    "%s %s %s%s",
                    atk_name(DESC_THE).c_str(),
                    attacker->conj_verb("drain").c_str(),
                    defender_name(true).c_str(),
                    attack_strength_punctuation(special_damage).c_str());
        }
    }
}

void attack::drain_defender_speed()
{
    if (needs_message)
    {
        mprf("%s %s %s vigour!",
             atk_name(DESC_THE).c_str(),
             attacker->conj_verb("drain").c_str(),
             def_name(DESC_ITS).c_str());
    }
    defender->slow_down(attacker, 5 + random2(7));
}

int attack::inflict_damage(int dam, beam_type flavour, bool clean)
{
    if (flavour == NUM_BEAMS)
        flavour = special_damage_flavour;
    // Auxes temporarily clear damage_brand so we don't need to check
    if (damage_brand == SPWPN_REAPING
        || damage_brand == SPWPN_CHAOS && one_chance_in(100)
        || attacker->is_player() && you.unrand_equipped(UNRAND_SKULL_OF_ZONGULDROK))
    {
        defender->props[REAPING_DAMAGE_KEY].get_int() += dam;
        // With two reapers of different friendliness, the most recent one
        // gets the spectral.
        defender->props[REAPER_KEY].get_int() = attacker->mid;
    }
    return defender->hurt(responsible, dam, flavour, kill_type,
                          "", aux_source.c_str(), clean);
}

/* If debug, return formatted damage done
 *
 */
string attack::debug_damage_number()
{
#ifdef DEBUG_DIAGNOSTICS
    return make_stringf(" for %d", damage_done);
#else
    return "";
#endif
}

/* Returns standard attack punctuation
 *
 * Used in player / monster (both primary and aux) attacks
 */
string attack_strength_punctuation(int dmg)
{
    if (dmg < HIT_WEAK)
        return ".";
    else if (dmg < HIT_MED)
        return "!";
    else if (dmg < HIT_STRONG)
        return "!!";
    else
        return string(3 + (int) log2(dmg / HIT_STRONG), '!');
}

/* Returns evasion adverb
 *
 */
string attack::evasion_margin_adverb()
{
    return (ev_margin <= -20) ? " completely" :
           (ev_margin <= -12) ? "" :
           (ev_margin <= -6)  ? " closely"
                              : " barely";
}

void attack::stab_message()
{
    defender->props[HELPLESS_KEY] = true;

    switch (stab_bonus)
    {
    case 4:     // confused/fleeing/distracted
        if (!one_chance_in(3))
        {
            mprf("You catch %s completely off-guard!",
                  defender->name(DESC_THE).c_str());
        }
        else
        {
            mprf("You %s %s from behind!",
                  you.has_mutation(MUT_PAWS) ? "pounce on" : "strike",
                  defender->name(DESC_THE).c_str());
        }
        break;
    case 1:
        if (you.has_mutation(MUT_PAWS) && coinflip())
        {
            mprf("You pounce on the unaware %s!",
                 defender->name(DESC_PLAIN).c_str());
            break;
        }
        mprf("%s fails to defend %s.",
              defender->name(DESC_THE).c_str(),
              defender->pronoun(PRONOUN_REFLEXIVE).c_str());
        break;
    }

    defender->props.erase(HELPLESS_KEY);
}

/* Returns the attacker's name
 *
 * Helper method to easily access the attacker's name
 */
string attack::atk_name(description_level_type desc)
{
    return actor_name(attacker, desc, attacker_visible);
}

/* Returns the defender's name
 *
 * Helper method to easily access the defender's name
 */
string attack::def_name(description_level_type desc)
{
    return actor_name(defender, desc, defender_visible);
}

/* TODO: Remove this!
 * Removing it may not really be practical, in retrospect. Its only used
 * below, in calc_elemental_brand_damage, which is called for both frost and
 * flame brands for both players and monsters.
 */
string attack::defender_name(bool allow_reflexive)
{
    if (allow_reflexive && attacker == defender)
        return actor_pronoun(attacker, PRONOUN_REFLEXIVE, attacker_visible);
    else
        return def_name(DESC_THE);
}

int attack::player_apply_misc_modifiers(int damage)
{
    return damage;
}

/**
 * Get the damage bonus from a weapon's enchantment.
 */
int attack::get_weapon_plus()
{
    if (weapon->base_type == OBJ_STAVES
#if TAG_MAJOR_VERSION == 34
        || weapon->sub_type == WPN_BLOWGUN
        || weapon->base_type == OBJ_RODS
#endif
       )
    {
        return 0;
    }
    return weapon->plus;
}

static int _core_apply_slaying(int damage, int plus)
{
    // +0 is random2(1) (which is 0)
    if (plus >= 0)
        return damage + random2(1 + plus);
    else
        return damage - random2(1 - plus);
}

// Slaying and weapon enchantment. Apply this for slaying even if not
// using a weapon to attack.
int attack::player_apply_slaying_bonuses(int damage, bool aux)
{
    int damage_plus = 0;
    if (!aux && using_weapon())
        damage_plus = get_weapon_plus();

    const bool throwing = !weapon && wpn_skill == SK_THROWING;
    const bool ranged = throwing
                        || (weapon && is_range_weapon(*weapon)
                                   && using_weapon());
    damage_plus += slaying_bonus(throwing);
    damage_plus -= you.corrosion_amount();

    // XXX: should this also trigger on auxes?
    if (!aux && !ranged)
        damage_plus += you.infusion_amount() * 4;

    return _core_apply_slaying(damage, damage_plus);
}

int attack::player_apply_final_multipliers(int damage, bool /*aux*/)
{
    // Spectral weapons deal "only" 70% of the damage that their
    // owner would, matching cleaving.
    if (attacker->type == MONS_SPECTRAL_WEAPON)
        damage = div_rand_round(damage * 7, 10);

    return damage;
}

int attack::apply_rev_penalty(int damage) const
{
    if (!attacker->is_player()
        || !you.has_mutation(MUT_WARMUP_STRIKES)
        || you.rev_percent() >= FULL_REV_PERCENT)
    {
        return damage;
    }
    // 2/3rds at 0 rev, 100% at full rev
    return div_rand_round(damage * 2 * FULL_REV_PERCENT
                            + damage * you.rev_percent(),
                          3 * FULL_REV_PERCENT);
}

int attack::player_apply_postac_multipliers(int damage)
{
    return damage;
}

void attack::player_exercise_combat_skills()
{
}

/* Returns attacker base unarmed damage
 *
 * Scales for current mutations and unarmed effects
 * TODO: Complete symmetry for base_unarmed damage
 * between monsters and players.
 */
int attack::calc_base_unarmed_damage() const
{
    // Should only get here if we're not wielding something that's a weapon.
    // If there's a non-weapon in hand, it has no base damage.
    if (weapon)
        return 0;

    if (!attacker->is_player())
        return 0;

    const int dam = unarmed_base_damage(true) + unarmed_base_damage_bonus(true);
    return dam > 0 ? dam : 0;
}

int attack::adjusted_weapon_damage() const
{
    return brand_adjust_weapon_damage(weapon_damage(), damage_brand, true);
}

int attack::calc_damage()
{
    if (stat_source().is_monster())
    {
        int damage = 0;
        int damage_max = 0;
        if (using_weapon() || wpn_skill == SK_THROWING)
        {
            damage_max = adjusted_weapon_damage();
            damage += random2(damage_max);

            int wpn_damage_plus = 0;
            if (weapon) // can be 0 for throwing projectiles
                wpn_damage_plus = get_weapon_plus();

            const int jewellery = attacker->as_monster()->inv[MSLOT_JEWELLERY];
            if (jewellery != NON_ITEM
                && env.item[jewellery].is_type(OBJ_JEWELLERY, RING_SLAYING))
            {
                wpn_damage_plus += env.item[jewellery].plus;
            }

            wpn_damage_plus += attacker->scan_artefacts(ARTP_SLAYING);

            damage = _core_apply_slaying(damage, wpn_damage_plus);
        }

        damage_max += attk_damage;
        damage     += 1 + random2(attk_damage);

        damage = apply_damage_modifiers(damage);

        set_attack_verb(damage);
        return apply_defender_ac(damage, damage_max);
    }
    else
    {
        int potential_damage, damage;

        potential_damage = using_weapon() || wpn_skill == SK_THROWING
            ? adjusted_weapon_damage() : calc_base_unarmed_damage();

        potential_damage = stat_modify_damage(potential_damage, wpn_skill, using_weapon());

        damage = random2(potential_damage+1);

        if (using_weapon())
            damage = apply_weapon_skill(damage, wpn_skill, true);
        damage = apply_fighting_skill(damage, false, true);
        damage = player_apply_misc_modifiers(damage);
        damage = player_apply_slaying_bonuses(damage, false);
        damage = player_stab(damage);
        // A failed stab may have awakened monsters, but that could have
        // caused the defender to cease to exist (spectral weapons with
        // missing summoners; or pacified monsters on a stair). FIXME:
        // The correct thing to do would be either to delay the call to
        // alert_nearby_monsters (currently in player_stab) until later
        // in the attack; or to avoid removing monsters in handle_behaviour.
        if (!defender->alive())
            return 0;
        damage = player_apply_final_multipliers(damage);
        damage = apply_defender_ac(damage);
        damage = player_apply_postac_multipliers(damage);

        damage = max(0, damage);
        set_attack_verb(damage);

        return damage;
    }

    return 0;
}

int attack::test_hit(int to_land, int ev, bool randomise_ev)
{
    int margin = AUTOMATIC_HIT;

    if (randomise_ev)
        ev = random2avg(2*ev, 2);
    if (to_land >= AUTOMATIC_HIT)
        return true;

    if (x_chance_in_y(MIN_HIT_MISS_PERCENTAGE, 100))
        margin = (random2(2) ? 1 : -1) * AUTOMATIC_HIT;
    else
        margin = to_land - ev;

    if (attacker->is_player() && you.duration[DUR_BLIND])
    {
        const int distance = you.pos().distance_from(defender->pos());
        if (x_chance_in_y(player_blind_miss_chance(distance), 100))
            margin = -1;
    }

#ifdef DEBUG_DIAGNOSTICS
    dprf(DIAG_COMBAT, "to hit: %d; ev: %d; result: %s (%d)",
         to_hit, ev, (margin >= 0) ? "hit" : "miss", margin);
#endif

    return margin;
}

int attack::apply_defender_ac(int damage, int damage_max, ac_type ac_rule) const
{
    ASSERT(defender);
    int after_ac = defender->apply_ac(damage, damage_max, ac_rule);
    dprf(DIAG_COMBAT, "AC: att: %s, def: %s, ac: %d, gdr: %d, dam: %d -> %d",
                 attacker->name(DESC_PLAIN, true).c_str(),
                 defender->name(DESC_PLAIN, true).c_str(),
                 defender->armour_class(),
                 defender->gdr_perc(),
                 damage,
                 after_ac);

    return after_ac;
}

/* Determine whether a block occurred
 *
 * No blocks if defender is incapacitated, would be nice to eventually expand
 * this method to handle partial blocks as well as full blocks (although this
 * would serve as a nerf to shields and - while more realistic - may not be a
 * good mechanic for shields.
 *
 * Returns (block_occurred)
 */
bool attack::attack_shield_blocked(bool verbose)
{
    if (defender == attacker || to_hit >= AUTOMATIC_HIT)
        return false; // You can't block your own attacks!

    // Divine Shield blocks are guaranteed, no matter what.
    if (defender->incapacitated()
        && !(defender->is_player() && you.duration[DUR_DIVINE_SHIELD]))
    {
        return false;
    }

    const int con_block = random2(attacker->shield_bypass_ability(to_hit));
    int pro_block = defender->shield_bonus();

    if (!attacker->visible_to(defender))
        pro_block /= 3;

    dprf(DIAG_COMBAT, "Defender: %s, Pro-block: %d, Con-block: %d",
         def_name(DESC_PLAIN).c_str(), pro_block, con_block);

    if (pro_block >= con_block && !defender->shield_exhausted()
        || defender->is_player() && you.duration[DUR_DIVINE_SHIELD])
    {
        perceived_attack = true;

        if (ignores_shield(verbose))
            return false;

        if (needs_message && verbose)
        {
            mprf("%s %s %s attack.",
                 defender_name(false).c_str(),
                 defender->conj_verb("block").c_str(),
                 attacker == defender ? "its own"
                                      : atk_name(DESC_ITS).c_str());
        }

        defender->shield_block_succeeded(attacker);

        return true;
    }

    return false;
}

bool attack::apply_poison_damage_brand()
{
    if (!one_chance_in(4))
    {
        int old_poison;

        if (defender->is_player())
            old_poison = you.duration[DUR_POISONING];
        else
        {
            old_poison =
                (defender->as_monster()->get_ench(ENCH_POISON)).degree;
        }

        defender->poison(attacker, 6 + random2(8) + random2(damage_done * 3 / 2));

        if (defender->is_player()
               && old_poison < you.duration[DUR_POISONING]
            || !defender->is_player()
               && old_poison <
                  (defender->as_monster()->get_ench(ENCH_POISON)).degree)
        {
            return true;
        }
    }
    return false;
}

bool attack::apply_damage_brand(const char *what)
{
    int brand = 0;
    bool ret = false;

    special_damage = 0;
    obvious_effect = false;
    brand = damage_brand == SPWPN_CHAOS ? random_chaos_brand() : damage_brand;

    if (brand != SPWPN_FLAMING && brand != SPWPN_FREEZING
        && brand != SPWPN_ELECTROCUTION && brand != SPWPN_VAMPIRISM
        && brand != SPWPN_PROTECTION && !defender->alive())
    {
        // Most brands have no extra effects on just killed enemies, and the
        // effect would be often inappropriate.
        return false;
    }

    if (!damage_done
        && (brand == SPWPN_FLAMING || brand == SPWPN_FREEZING
            || brand == SPWPN_HOLY_WRATH || brand == SPWPN_FOUL_FLAME
            || brand == SPWPN_ANTIMAGIC || brand == SPWPN_VAMPIRISM))
    {
        // These brands require some regular damage to function.
        return false;
    }

    switch (brand)
    {
    case SPWPN_PROTECTION:
        if (attacker->is_player() && !defender->is_firewood())
            refresh_weapon_protection();
        break;

    case SPWPN_FLAMING:
        calc_elemental_brand_damage(BEAM_FIRE,
                                    defender->is_icy() ? "melt" : "burn",
                                    what);
        defender->expose_to_element(BEAM_FIRE, 2);
        if (defender->is_player())
            maybe_melt_player_enchantments(BEAM_FIRE, special_damage);
        break;

    case SPWPN_FREEZING:
        calc_elemental_brand_damage(BEAM_COLD, "freeze", what);
        defender->expose_to_element(BEAM_COLD, 2, attacker);
        break;

    case SPWPN_HOLY_WRATH:
        if (attacker->undead_or_demonic())
            break; // No holy wrath for thee!

        if (defender->holy_wrath_susceptible())
            special_damage = 1 + (random2(damage_done * 15) / 10);

        if (special_damage && defender_visible)
        {
            special_damage_message =
                make_stringf(
                    "%s %s%s",
                    defender_name(false).c_str(),
                    defender->conj_verb("convulse").c_str(),
                    attack_strength_punctuation(special_damage).c_str());
        }
        break;

    case SPWPN_FOUL_FLAME:
    {
        if (attacker->is_holy())
            break; // No foul flame for thee!

        const int rff = defender->res_foul_flame();
        if (rff < 0)
            special_damage = 1 + (random2(damage_done) * 1.5);
        else if (rff < 3)
            special_damage = 1 + (random2(damage_done) / (rff + 1));

        if (defender_visible && special_damage)
        {
            special_damage_message =
                make_stringf(
                    "%s %s%s",
                    defender_name(false).c_str(),
                    defender->conj_verb("convulse").c_str(),
                    attack_strength_punctuation(special_damage).c_str());
        }
        break;
    }

    case SPWPN_ELECTROCUTION:
        if (defender->res_elec() > 0)
            break;
        else if (one_chance_in(4))
        {
            special_damage = 8 + random2(13);
            const string punctuation =
                    attack_strength_punctuation(special_damage);
            special_damage_message =
                defender->is_player()
                ? make_stringf("You are electrocuted%s", punctuation.c_str())
                : make_stringf("Lightning courses through %s%s",
                               defender->name(DESC_THE).c_str(),
                               punctuation.c_str());
            special_damage_flavour = BEAM_ELECTRICITY;
            defender->expose_to_element(BEAM_ELECTRICITY, 2);
        }

        break;

    case SPWPN_VENOM:
        obvious_effect = apply_poison_damage_brand();
        break;

    case SPWPN_DRAINING:
        drain_defender();
        break;

    case SPWPN_VAMPIRISM:
    {
        if (!weapon
            || damage_done < 1
            || !actor_is_susceptible_to_vampirism(*defender)
            || attacker->stat_hp() == attacker->stat_maxhp()
            || attacker->is_player() && you.duration[DUR_DEATHS_DOOR]
            || x_chance_in_y(2, 5)
               && !is_unrandom_artefact(*weapon, UNRAND_LEECH))
        {
            break;
        }

        int hp_boost = is_unrandom_artefact(*weapon, UNRAND_VAMPIRES_TOOTH)
                       ? damage_done : 1 + random2(damage_done);
        hp_boost = resist_adjust_damage(defender, BEAM_NEG, hp_boost);

        if (hp_boost)
        {
            obvious_effect = true;

            if (attacker->is_player())
                canned_msg(MSG_GAIN_HEALTH);
            else if (attacker_visible)
            {
                if (defender->is_player())
                {
                    mprf("%s draws strength from your wounds!",
                         attacker->name(DESC_THE).c_str());
                }
                else
                {
                    mprf("%s is healed.",
                         attacker->name(DESC_THE).c_str());
                }
            }

            dprf(DIAG_COMBAT, "Vampiric Healing: damage %d, healed %d",
                 damage_done, hp_boost);
            attacker->heal(hp_boost);
        }
        break;
    }
    case SPWPN_PAIN:
        if (attacker->is_player() && you_worship(GOD_TROG)
            || !attacker->is_player() && attacker->as_monster()->god == GOD_TROG)
        {
            break;
        }

        pain_affects_defender();
        break;

    case SPWPN_DISTORTION:
        ret = distortion_affects_defender();
        break;

    case SPWPN_CONFUSE:
    {
        // If a monster with a chaos weapon gets this brand, act like
        // AF_CONFUSE.
        if (attacker->is_monster())
        {
            if (one_chance_in(3))
            {
                defender->confuse(attacker,
                                  1 + random2(3+attacker->get_hit_dice()));
            }
            break;
        }

        // Also used for players in fungus form.
        if (attacker->is_player()
            && you.form == transformation::fungus
            && !you.duration[DUR_CONFUSING_TOUCH]
            && defender->is_unbreathing())
        {
            break;
        }

        // Declaring these just to pass to the enchant function.
        bolt beam_temp;
        beam_temp.thrower   = attacker->is_player() ? KILL_YOU : KILL_MON;
        beam_temp.flavour   = BEAM_CONFUSION;
        beam_temp.source_id = attacker->mid;

        if (attacker->is_player() && damage_brand == SPWPN_CONFUSE
            && you.duration[DUR_CONFUSING_TOUCH])
        {
            beam_temp.ench_power = you.props[CONFUSING_TOUCH_KEY].get_int();
            int margin;
            monster* mon = defender->as_monster();
            if (beam_temp.try_enchant_monster(mon, margin) == MON_AFFECTED)
            {
                you.duration[DUR_CONFUSING_TOUCH] = 0;
                obvious_effect = false;
            }
            else if (!ench_flavour_affects_monster(attacker, beam_temp.flavour, mon)
                     || mons_invuln_will(*mon))
            {
                mprf("%s is completely immune to your confusing touch!",
                     mon->name(DESC_THE).c_str());
                you.duration[DUR_CONFUSING_TOUCH] = 1;
            }
        }
        else if (!x_chance_in_y(melee_confuse_chance(defender->get_hit_dice()),
                                                     100)
                 || defender->as_monster()->clarity())
        {
            beam_temp.apply_enchantment_to_monster(defender->as_monster());
            obvious_effect = beam_temp.obvious_effect;
            break;
        }

        break;
    }

    case SPWPN_CHAOS:
        obvious_effect = chaos_affects_actor(defender, attacker);
        break;

    case SPWPN_ANTIMAGIC:
        antimagic_affects_defender(damage_done * 8);
        break;

    case SPWPN_ACID:
        defender->splash_with_acid(attacker);
        break;

    default:
        if (using_weapon() && is_unrandom_artefact(*weapon, UNRAND_DAMNATION))
            attacker->god_conduct(DID_EVIL, 2 + random2(3));
        break;
    }

    if (damage_brand == SPWPN_CHAOS)
    {
        if (responsible->is_player())
            did_god_conduct(DID_CHAOS, 2 + random2(3));
    }

    if (!obvious_effect)
        obvious_effect = !special_damage_message.empty();

    if (needs_message && !special_damage_message.empty())
    {
        mpr(special_damage_message);

        special_damage_message.clear();
    }

    // Preserve Nessos's brand stacking in a hacky way -- but to be fair, it
    // was always a bit of a hack.
    if (attacker->type == MONS_NESSOS && weapon && is_range_weapon(*weapon))
        apply_poison_damage_brand();

    if (special_damage > 0)
        inflict_damage(special_damage, special_damage_flavour);

    return ret;
}

/* Calculates special damage, prints appropriate combat text
 *
 * Applies a particular damage brand to the current attack, the setup and
 * calculation of base damage and other effects varies based on the type
 * of attack, but the calculation of elemental damage should be consistent.
 */
void attack::calc_elemental_brand_damage(beam_type flavour,
                                         const char *verb,
                                         const char *what)
{
    special_damage = resist_adjust_damage(defender, flavour,
                                          random2(damage_done) / 2 + 1);

    if (needs_message && special_damage > 0 && verb)
    {
        // XXX: assumes "what" is singular
        special_damage_message = make_stringf(
            "%s %s %s%s%s",
            what ? what : atk_name(DESC_THE).c_str(),
            what ? conjugate_verb(verb, false).c_str()
                 : attacker->conj_verb(verb).c_str(),
            // Don't allow reflexive if the subject wasn't the attacker.
            defender_name(!what).c_str(),
            flavour == BEAM_FIRE && defender->res_fire() < 0
             || flavour == BEAM_COLD && defender->res_cold() < 0 ? " terribly"
                : "",
            attack_strength_punctuation(special_damage).c_str());
    }
}

int attack::player_stab_weapon_bonus(int damage)
{
    int stab_skill = you.skill(wpn_skill, 50) + you.skill(SK_STEALTH, 50);

    if (player_good_stab())
    {
        // We might be unarmed if we're using the hood of the Assassin.
        const bool extra_good = using_weapon() && weapon->sub_type == WPN_DAGGER;
        int bonus = you.dex() * (stab_skill + 100) / (extra_good ? 500 : 1000);

        bonus   = stepdown_value(bonus, 10, 10, 30, 30);
        damage += bonus;
        damage *= 10 + div_rand_round(stab_skill, 100 * stab_bonus);
        damage /= 10;
    }

    // There's both a flat and multiplicative component to
    // stab bonus damage.

    damage *= 12 + div_rand_round(stab_skill, 100 * stab_bonus);
    damage /= 12;

    // The flat component is loosely based on the old stab_bypass bonus.
    // Essentially, it's an extra quarter-point of damage for every
    // point of weapon + stealth skill, divided by stab_bonus - that is,
    // quartered again if the target isn't sleeping, paralysed, or petrified.
    damage += random2(div_rand_round(stab_skill, 200 * stab_bonus));

    return damage;
}

int attack::player_stab(int damage)
{
    // The stabbing message looks better here:
    if (stab_attempt)
    {
        // Construct reasonable message.
        stab_message();
        practise_stabbing();
    }
    else
    {
        stab_bonus = 0;
        // Ok.. if you didn't backstab, you wake up the neighborhood.
        // I can live with that.
        alert_nearby_monsters();
    }

    if (stab_bonus)
    {
        // Let's make sure we have some damage to work with...
        damage = max(1, damage);

        damage = player_stab_weapon_bonus(damage);
    }

    return damage;
}

/* Check for stab and prepare combat for stab-values
 *
 * Grant an automatic stab if paralysed or sleeping (with highest damage value)
 * stab_bonus is used as the divisor in damage calculations, so lower values
 * will yield higher damage. Normal stab chance is (stab_skill + dex + 1 / roll)
 * This averages out to about 1/3 chance for a non extended-endgame stabber.
 */
void attack::player_stab_check()
{
    // Stabbing monsters is unchivalric, and disabled under TSO!
    // (And also requires more finesse than just stumbling into a monster.)
    if (you.confused() || have_passive(passive_t::no_stabbing))
    {
        stab_attempt = false;
        stab_bonus = 0;
        return;
    }

    const stab_type orig_st = find_player_stab_type(*defender->as_monster());
    stab_type st = orig_st;
    // Find stab type is also used for displaying information about monsters,
    // so upgrade the stab type for !stab and the Spriggan's Knife here
    if (using_weapon()
        && is_unrandom_artefact(*weapon, UNRAND_SPRIGGANS_KNIFE)
        && st != STAB_NO_STAB
        && coinflip())
    {
        st = STAB_SLEEPING;
    }
    stab_attempt = st != STAB_NO_STAB;
    stab_bonus = stab_bonus_denom(st);

    // See if we need to roll against dexterity / stabbing.
    if (stab_attempt && stab_bonus > 1)
    {
        stab_attempt = x_chance_in_y(you.skill_rdiv(wpn_skill, 1, 2)
                                     + you.skill_rdiv(SK_STEALTH, 1, 2)
                                     + you.dex() + 1,
                                     100);
    }

    if (stab_attempt)
        count_action(CACT_STAB, orig_st);
}

void attack::handle_noise(const coord_def & pos)
{
    // Successful stabs make no noise.
    if (stab_attempt)
        return;

    // Shadow attacks are silent.
    if (attacker->type == MONS_PLAYER_SHADOW)
        return;

    int loudness = damage_done / 4;

    // All non-stab attacks make some noise.
    loudness = max(1, loudness);

    // Cap noise at shouting volume.
    loudness = min(12, loudness);

    noisy(loudness, pos, attacker->mid);
}

actor &attack::stat_source() const
{
    if (attacker->type != MONS_SPECTRAL_WEAPON)
        return *attacker;

    const mid_t summoner_mid = attacker->as_monster()->summoner;
    if (summoner_mid == MID_NOBODY)
        return *attacker;

    actor* summoner = actor_by_mid(attacker->as_monster()->summoner);
    if (!summoner || !summoner->alive())
        return *attacker;
    return *summoner;
}

void attack::maybe_trigger_jinxbite()
{
    if (attacker->is_player() && you.duration[DUR_JINXBITE])
        jinxbite_fineff::schedule(defender);
}

void attack::maybe_trigger_fugue_wail(const coord_def pos)
{
    if (attacker->is_player() && you.duration[DUR_FUGUE]
        && (you.props[FUGUE_KEY].get_int() == FUGUE_MAX_STACKS))
    {
        do_fugue_wail(pos);
    }
}

void attack::maybe_trigger_autodazzler()
{
    if (defender->is_player() && you.wearing_ego(OBJ_GIZMOS, SPGIZMO_AUTODAZZLE)
        && one_chance_in(20))
    {
        bolt proj;
        zappy(ZAP_AUTODAZZLE, you.get_experience_level(), false, proj);

        proj.target = attacker->pos();
        proj.source = you.pos();
        proj.range = LOS_RADIUS;
        proj.source_id = MID_PLAYER;
        proj.draw_delay = 5;
        proj.attitude = ATT_FRIENDLY;
        proj.is_tracer = true;
        proj.is_targeting = true;

        // To suppress prompts for aiming at allies. We'll never fire in that
        // situation anyway.
        proj.thrower = KILL_MON_MISSILE;

        // Make sure the beam path is clear
        proj.fire();
        if (proj.friend_info.count == 0)
        {
            mpr("Your autodazzler retaliates!");

            proj.thrower = KILL_YOU_MISSILE;
            proj.is_tracer = false;
            proj.is_targeting = false;
            proj.fire();
        }
    }
}

bool attack::paragon_defends_player()
{
    if (defender->is_player() && paragon_defense_bonus_active()
        && one_chance_in(3))
    {
        mprf("Your paragon deflects %s attack away from you.",
                    attacker->name(DESC_ITS).c_str());
        return true;
    }

    return false;
}
