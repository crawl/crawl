/*
 *  File:       fight.cc
 *  Summary:    Functions used during combat.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "fight.h"
#include "melee_attack.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>

#include "externs.h"

#include "areas.h"
#include "artefact.h"
#include "attitude-change.h"
#include "beam.h"
#include "cloud.h"
#include "coordit.h"
#include "database.h"
#include "debug.h"
#include "delay.h"
#include "directn.h"
#include "effects.h"
#include "env.h"
#include "map_knowledge.h"
#include "fprop.h"
#include "food.h"
#include "goditem.h"
#include "invent.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "kills.h"
#include "macro.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-clone.h"
#include "mon-place.h"
#include "terrain.h"
#include "mgen_data.h"
#include "coord.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "mutation.h"
#include "ouch.h"
#include "player.h"
#include "random-var.h"
#include "religion.h"
#include "godconduct.h"
#include "shopping.h"
#include "skills.h"
#include "spells1.h"
#include "spells3.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "hints.h"
#include "view.h"
#include "shout.h"
#include "xom.h"

#ifdef NOTE_DEBUG_CHAOS_BRAND
    #define NOTE_DEBUG_CHAOS_EFFECTS
#endif

#ifdef NOTE_DEBUG_CHAOS_EFFECTS
#include "notes.h"
#endif

const int HIT_WEAK   = 7;
const int HIT_MED    = 18;
const int HIT_STRONG = 36;

static void stab_message(actor *defender, int stab_bonus);

static inline int player_weapon_str_weight( void );
static inline int player_weapon_dex_weight( void );

static inline int calc_stat_to_hit_base( void );
static inline int calc_stat_to_dam_base( void );
static void mons_lose_attack_energy(monsters *attacker, int wpn_speed,
                                    int which_attack, int effective_attack);

/*
 **************************************************
 *                                                *
 *             BEGIN PUBLIC FUNCTIONS             *
 *                                                *
 **************************************************
*/

// This function is only used when monsters are attacking.
bool test_melee_hit(int to_hit, int ev, defer_rand& r)
{
    int   roll = -1;
    int margin = AUTOMATIC_HIT;

    ev *= 2;

    if (to_hit >= AUTOMATIC_HIT)
        return (true);
    else if (r[0].x_chance_in_y(MIN_HIT_MISS_PERCENTAGE, 100))
        margin = (r[1].random2(2) ? 1 : -1) * AUTOMATIC_HIT;
    else
    {
        roll = r[2].random2( to_hit + 1 );
        margin = (roll - r[3].random2avg(ev, 2));
    }

#ifdef DEBUG_DIAGNOSTICS
    float miss;

    if (to_hit < ev)
        miss = 100.0 - MIN_HIT_MISS_PERCENTAGE / 2.0;
    else
    {
        miss = MIN_HIT_MISS_PERCENTAGE / 2.0 +
            ((100.0 - MIN_HIT_MISS_PERCENTAGE) * ev) / to_hit;
    }

    mprf( MSGCH_DIAGNOSTICS,
          "to hit: %d; ev: %d; miss: %0.2f%%; roll: %d; result: %s%s (%d)",
              to_hit, ev, miss, roll, (margin >= 0) ? "hit" : "miss",
              (roll == -1) ? "!!!" : "", margin );
#endif

    return (margin >= 0);
}

// This function returns the "extra" stats the player gets because of
// choice of weapon... it's used only for giving warnings when a player
// wields a less than ideal weapon.
int effective_stat_bonus( int wepType )
{
#ifdef USE_NEW_COMBAT_STATS
    int str_weight;
    if (wepType == -1)
        str_weight = player_weapon_str_weight();
    else
        str_weight = weapon_str_weight( OBJ_WEAPONS, wepType );

    return ((you.strength() - you.dex()) * (str_weight - 5) / 10);
#else
    return (0);
#endif
}

// Returns the to-hit for your extra unarmed attacks.
// DOES NOT do the final roll (i.e., random2(your_to_hit)).
static int calc_your_to_hit_unarmed(int uattack = UNAT_NO_ATTACK,
                                    bool vampiric = false)
{
    int your_to_hit;

    your_to_hit = 13 + you.dex() / 2 + you.skills[SK_UNARMED_COMBAT] / 2
                  + you.skills[SK_FIGHTING] / 5;

    if (wearing_amulet(AMU_INACCURACY))
        your_to_hit -= 5;

    if (player_mutation_level(MUT_EYEBALLS))
        your_to_hit += 2 * player_mutation_level(MUT_EYEBALLS) + 1;

    // Vampires know how to bite and aim better when thirsty.
    if (you.species == SP_VAMPIRE && uattack == UNAT_BITE)
    {
        your_to_hit += 1;

        if (vampiric)
        {
            if (you.hunger_state == HS_STARVING)
                your_to_hit += 2;
            else if (you.hunger_state < HS_SATIATED)
                your_to_hit += 1;
        }
    }
    else if (you.species != SP_VAMPIRE && you.hunger_state == HS_STARVING)
        your_to_hit -= 3;

    your_to_hit += slaying_bonus(PWPN_HIT);

    return your_to_hit;
}

// Calculates your to-hit roll. If random_factor is true, be stochastic;
// if false, determinstic (e.g. for chardumps).
int calc_your_to_hit( bool random_factor )
{
    melee_attack attk(&you, NULL);
    return attk.calc_to_hit(random_factor);
}

random_var calc_your_attack_delay()
{
    melee_attack attk(&you, NULL);
    return attk.player_calc_attack_delay();
}

static bool player_fights_well_unarmed(int heavy_armour_penalty)
{
    return (you.burden_state == BS_UNENCUMBERED
            && x_chance_in_y(you.skills[SK_UNARMED_COMBAT], 20)
            && x_chance_in_y(2, 1 + heavy_armour_penalty));
}

unchivalric_attack_type is_unchivalric_attack(const actor *attacker,
                                              const actor *defender)
{
    const monsters *def = defender->as_monster();
    unchivalric_attack_type unchivalric = UCAT_NO_ATTACK;

    // No unchivalric attacks on monsters that cannot fight (e.g.
    // plants) or monsters the attacker can't see (either due to
    // invisibility or being behind opaque clouds).
    if (defender->cannot_fight() || (attacker && !attacker->can_see(defender)))
        return (unchivalric);

    // Distracted (but not batty); this only applies to players.
    if (attacker && attacker->atype() == ACT_PLAYER
        && def && def->foe != MHITYOU && !mons_is_batty(def))
    {
        unchivalric = UCAT_DISTRACTED;
    }

    // confused (but not perma-confused)
    if (def
        && def->has_ench(ENCH_CONFUSION)
        && !mons_class_flag(def->type, M_CONFUSED))
    {
        unchivalric = UCAT_CONFUSED;
    }

    // allies
    if (def && def->friendly())
        unchivalric = UCAT_ALLY;

    // fleeing
    if (def && mons_is_fleeing(def))
        unchivalric = UCAT_FLEEING;

    // invisible
    if (attacker && !attacker->visible_to(defender))
        unchivalric = UCAT_INVISIBLE;

    // held in a net
    if (def && def->caught())
        unchivalric = UCAT_HELD_IN_NET;

    // petrifying
    if (def && def->petrifying())
        unchivalric = UCAT_PETRIFYING;

    // petrified
    if (defender->petrified())
        unchivalric = UCAT_PETRIFIED;

    // paralysed
    if (defender->paralysed())
        unchivalric = UCAT_PARALYSED;

    // sleeping
    if (defender->asleep())
        unchivalric = UCAT_SLEEPING;

    return (unchivalric);
}

//////////////////////////////////////////////////////////////////////////
// Melee attack

melee_attack::melee_attack(actor *attk, actor *defn,
                           bool allow_unarmed, int which_attack)
    : attacker(attk), defender(defn), cancel_attack(false), did_hit(false),
    perceived_attack(false), obvious_effect(false), needs_message(false),
    attacker_visible(false), defender_visible(false),
    attacker_invisible(false), defender_invisible(false),
    unarmed_ok(allow_unarmed),
    attack_number(which_attack), to_hit(0), damage_done(0), special_damage(0),
    aux_damage(0), stab_attempt(false), stab_bonus(0), min_delay(0),
    final_attack_delay(0), noise_factor(0), extra_noise(0), weapon(NULL),
    damage_brand(SPWPN_NORMAL), wpn_skill(SK_UNARMED_COMBAT), hands(HANDS_ONE),
    hand_half_bonus(false), art_props(0), unrand_entry(NULL),
    attack_verb("bug"), verb_degree(),
    no_damage_message(), special_damage_message(), aux_attack(), aux_verb(),
    special_damage_flavour(BEAM_NONE),
    shield(NULL), defender_shield(NULL),
    player_body_armour_penalty(0), player_shield_penalty(0),
    player_armour_tohit_penalty(0), player_shield_tohit_penalty(0),
    can_do_unarmed(false), miscast_level(-1), miscast_type(SPTYP_NONE),
    miscast_target(NULL), final_effects()
{
    init_attack();
}

void melee_attack::init_attack()
{
    weapon       = attacker->weapon(attack_number);
    damage_brand = attacker->damage_brand(attack_number);

    if (weapon && weapon->base_type == OBJ_WEAPONS && is_artefact(*weapon))
    {
        artefact_wpn_properties(*weapon, art_props);
        if (is_unrandom_artefact(*weapon))
            unrand_entry = get_unrand_entry(weapon->special);
    }

    wpn_skill = weapon ? weapon_skill(*weapon) : SK_UNARMED_COMBAT;
    if (weapon)
    {
        hands = hands_reqd(*weapon, attacker->body_size());

        switch (single_damage_type(*weapon))
        {
        case DAM_BLUDGEON:
        case DAM_WHIP:
            noise_factor = 125;
            break;
        case DAM_SLICE:
            noise_factor = 100;
            break;
        case DAM_PIERCE:
            noise_factor = 75;
            break;
        }
    }

    shield = attacker->shield();
    if (defender)
        defender_shield = defender->shield();

    hand_half_bonus = (unarmed_ok
                       && !shield
                       && weapon
                       && !weapon->cursed()
                       && hands == HANDS_HALF);

    attacker_visible   = attacker->observable();
    attacker_invisible = (!attacker_visible && you.see_cell(attacker->pos()));
    defender_visible   = defender && defender->observable();
    defender_invisible = (!defender_visible && defender
                          && you.see_cell(defender->pos()));
    needs_message      = (attacker_visible || defender_visible);

    if (attacker && attacker->atype() == ACT_PLAYER)
    {
        player_body_armour_penalty =
            player_adjusted_body_armour_evasion_penalty(1);
        player_shield_penalty =
            player_adjusted_shield_evasion_penalty(1);
        dprf("Player body armour penalty: %d, shield penalty: %d",
             player_body_armour_penalty, player_shield_penalty);
    }

    if (defender && defender->submerged())
    {
        // Unarmed attacks from tentacles are the only ones that can
        // reach submerged monsters.
        unarmed_ok = (attacker->damage_type() == DVORP_TENTACLE);
    }

    miscast_level  = -1;
    miscast_type   = SPTYP_NONE;
    miscast_target = NULL;
}

std::string melee_attack::actor_name(const actor *a,
                                     description_level_type desc,
                                     bool actor_visible,
                                     bool actor_invisible)
{
    return (actor_visible ? a->name(desc) : anon_name(desc, actor_invisible));
}

std::string melee_attack::pronoun(const actor *a,
                                  pronoun_type pron,
                                  bool actor_visible)
{
    return (actor_visible ? a->pronoun(pron) : anon_pronoun(pron));
}

std::string melee_attack::anon_pronoun(pronoun_type pron)
{
    switch (pron)
    {
    default:
    case PRONOUN_CAP:              return "It";
    case PRONOUN_NOCAP:            return "it";
    case PRONOUN_CAP_POSSESSIVE:   return "Its";
    case PRONOUN_NOCAP_POSSESSIVE: return "its";
    case PRONOUN_REFLEXIVE:        return "itself";
    }
}

std::string melee_attack::anon_name(description_level_type desc,
                                    bool actor_invisible)
{
    switch (desc)
    {
    case DESC_CAP_THE:
    case DESC_CAP_A:
        return (actor_invisible ? "It" : "Something");
    case DESC_CAP_YOUR:
        return ("Its");
    case DESC_NOCAP_YOUR:
    case DESC_NOCAP_ITS:
        return ("its");
    case DESC_NONE:
        return ("");
    case DESC_NOCAP_THE:
    case DESC_NOCAP_A:
    case DESC_PLAIN:
    default:
        return (actor_invisible? "it" : "something");
    }
}

std::string melee_attack::atk_name(description_level_type desc) const
{
    return actor_name(attacker, desc, attacker_visible, attacker_invisible);
}

std::string melee_attack::def_name(description_level_type desc) const
{
    return actor_name(defender, desc, defender_visible, defender_invisible);
}

std::string melee_attack::wep_name(description_level_type desc,
                                   unsigned long ignore_flags) const
{
    ASSERT(weapon != NULL);

    if (attacker->atype() == ACT_PLAYER)
        return weapon->name(desc, false, false, false, false, ignore_flags);

    std::string name;
    bool possessive = false;
    if (desc == DESC_CAP_YOUR)
    {
        desc       = DESC_CAP_THE;
        possessive = true;
    }
    else if (desc == DESC_NOCAP_YOUR)
    {
        desc       = DESC_NOCAP_THE;
        possessive = true;
    }

    if (possessive)
        name = apostrophise(atk_name(desc)) + " ";

    name += weapon->name(DESC_PLAIN, false, false, false, false, ignore_flags);

    return (name);
}

bool melee_attack::is_banished(const actor *a) const
{
    if (!a || a->alive())
        return (false);

    if (a->atype() == ACT_PLAYER)
        return (you.banished);
    else
        return (a->as_monster()->flags & MF_BANISHED);
}

void melee_attack::check_autoberserk()
{
    if (weapon
        && art_props[ARTP_ANGRY] >= 1
        && !one_chance_in(1 + art_props[ARTP_ANGRY]))
    {
        attacker->go_berserk(false);
    }
}

bool melee_attack::check_unrand_effects(bool mondied)
{
    // If bashing the defender with a wielded unrandart launcher, don't use
    // unrand_entry->fight_func, since that's the function used for
    // launched missiles.
    if (unrand_entry && unrand_entry->fight_func.melee_effects
        && weapon && fires_ammo_type(*weapon) == MI_NONE)
    {
        unrand_entry->fight_func.melee_effects(weapon, attacker, defender,
                                               mondied);
        return (!defender->alive());
    }

    return (false);
}

void melee_attack::identify_mimic(actor *act)
{
    if (act
        && act->atype() == ACT_MONSTER
        && mons_is_mimic(act->id())
        && you.can_see(act))
    {
        monsters* mon = act->as_monster();
        mon->flags |= MF_KNOWN_MIMIC;
    }
}

bool melee_attack::attack()
{
    // If a mimic is attacking or defending, it is thereafter known.
    identify_mimic(attacker);

    coord_def defender_pos = defender->pos();

    if (attacker->atype() == ACT_PLAYER && defender->atype() == ACT_MONSTER)
    {
        if (stop_attack_prompt(defender->as_monster(), false, attacker->pos()))
        {
            cancel_attack = true;
            return (false);
        }
    }

    if (attacker != defender)
    {
        // Allow setting of your allies' target, etc.
        attacker->attacking(defender);

        check_autoberserk();
    }

    // The attacker loses nutrition.
    attacker->make_hungry(3, true);

    // Xom thinks fumbles are funny...
    if (attacker->fumbles_attack())
    {
        // Make sure the monster uses up some energy, even though
        // it didn't actually attack.
        attacker->lose_energy(EUT_ATTACK);

        // ... and thinks fumbling when trying to hit yourself is just
        // hilarious.
        if (attacker == defender)
            xom_is_stimulated(255);
        else
            xom_is_stimulated(14);

        if (damage_brand == SPWPN_CHAOS)
            chaos_affects_attacker();

        return (false);
    }
    // Non-fumbled self-attacks due to confusion are still pretty funny,
    // though.
    else if (attacker == defender && attacker->confused())
    {
        // And is still hilarious if it's the player.
        if (attacker->atype() == ACT_PLAYER)
            xom_is_stimulated(255);
        else
            xom_is_stimulated(128);
    }

    // Defending monster protects itself from attacks using the wall
    // it's in.
    if (defender->atype() == ACT_MONSTER && cell_is_solid(defender->pos())
        && mons_wall_shielded(defender->as_monster()))
    {
        std::string feat_name = raw_feature_description(grd(defender->pos()));

        if (attacker->atype() == ACT_PLAYER)
        {
            player_apply_attack_delay();

            if (you.can_see(defender))
            {
                mprf("The %s protects %s from harm.",
                     feat_name.c_str(),
                     defender->name(DESC_NOCAP_THE).c_str());
            }
            else
            {
                mprf("You hit the %s.",
                     feat_name.c_str());
            }
        }
        else if (you.can_see(attacker))
        {
            // Make sure the monster uses up some energy, even though it
            // didn't actually land a blow.
            attacker->lose_energy(EUT_ATTACK);

            if (!mons_near(defender->as_monster()))
            {
                simple_monster_message(attacker->as_monster(),
                                       " hits something.");
            }
            else if (!you.can_see(attacker))
            {
                mprf("%s hits %s.", defender->name(DESC_CAP_THE).c_str(),
                     feat_name.c_str());
            }
            else
            {
                mprf("%s tries to hit %s, but is blocked by the %s.",
                     attacker->name(DESC_CAP_THE).c_str(),
                     defender->name(DESC_NOCAP_THE).c_str(),
                     feat_name.c_str());
            }
        }
        if (damage_brand == SPWPN_CHAOS)
            chaos_affects_attacker();

        return (true);
    }

    to_hit = calc_to_hit();

    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    if (attacker->atype() == ACT_PLAYER && attacker != defender)
        set_attack_conducts(conducts, defender->as_monster());

    // Trying to stay general beyond this point is a recipe for insanity.
    // Maybe when Stone Soup hits 1.0... :-)
    bool retval = ((attacker->atype() == ACT_PLAYER) ? player_attack() :
                   (defender->atype() == ACT_PLAYER) ? mons_attack_you()
                                                     : mons_attack_mons());
    identify_mimic(defender);

    if (env.sanctuary_time > 0 && retval && !cancel_attack
        && attacker != defender && !attacker->confused())
    {
        if (is_sanctuary(attacker->pos()) || is_sanctuary(defender->pos()))
        {
            if (attacker->atype() == ACT_PLAYER
                || attacker->as_monster()->friendly())
            {
                remove_sanctuary(true);
            }
        }
    }

    if (attacker->atype() == ACT_PLAYER)
    {
        handle_noise(defender_pos);

        if (damage_brand == SPWPN_CHAOS)
            chaos_affects_attacker();

        do_miscast();
    }

    enable_attack_conducts(conducts);

    return (retval);
}

static int _modify_blood_amount(const int damage, const int dam_type)
{
    int factor = 0; // DVORP_NONE

    switch (dam_type)
    {
    case DVORP_CRUSHING: // flails, also unarmed
    case DVORP_TENTACLE: // unarmed, tentacles
        factor =  2;
        break;
    case DVORP_SLASHING: // whips
        factor =  4;
        break;
    case DVORP_PIERCING: // pole-arms
        factor =  5;
        break;
    case DVORP_STABBING: // knives, daggers
        factor =  8;
        break;
    case DVORP_SLICING:  // other short/long blades, also blade hands
        factor = 10;
        break;
    case DVORP_CHOPPING: // axes
        factor = 17;
        break;
    case DVORP_CLAWING:  // unarmed, claws
        factor = 24;
        break;
    }

    return (damage * factor / 10);
}

static bool _vamp_wants_blood_from_monster(const monsters *mon)
{
    if (you.species != SP_VAMPIRE)
        return (false);

    if (you.hunger_state == HS_ENGORGED)
        return (false);

    if (mon->is_summoned())
        return (false);

    if (!mons_has_blood(mon->type))
        return (false);

    const int chunk_type = mons_corpse_effect( mon->type );

    // Don't drink poisonous or mutagenic blood.
    return (chunk_type == CE_CLEAN || chunk_type == CE_CONTAMINATED
            || chunk_type == CE_POISONOUS && player_res_poison());
}

// Should life protection protect from this?
// Called when stabbing, for bite attacks, and vampires wielding vampiric weapons
// Returns true if blood was drawn.
static bool _player_vampire_draws_blood(const monsters* mon, const int damage,
                                        bool needs_bite_msg = false,
                                        int reduction = 1)
{
    ASSERT(you.species == SP_VAMPIRE);

    if (!_vamp_wants_blood_from_monster(mon))
        return (false);

    const int chunk_type = mons_corpse_effect(mon->type);

    // Now print message, need biting unless already done (never for bat form!)
    if (needs_bite_msg && !player_in_bat_form())
    {
        mprf( "You bite %s, and draw %s blood!",
              mon->name(DESC_NOCAP_THE, true).c_str(),
              mon->pronoun(PRONOUN_NOCAP_POSSESSIVE).c_str());
    }
    else
        mprf( "You draw %s's blood!", mon->name(DESC_NOCAP_THE, true).c_str() );

    // Regain hp.
    if (you.hp < you.hp_max)
    {
        int heal = 1 + random2(damage);
        if (chunk_type == CE_CLEAN)
            heal += 1 + random2(damage);
        if (heal > you.experience_level)
            heal = you.experience_level;

        // Decrease healing when done in bat form.
        if (player_in_bat_form())
            heal /= 2;

        if (heal > 0)
        {
            inc_hp(heal, false);
            mprf("You feel %sbetter.", (you.hp == you.hp_max) ? "much " : "");
        }
    }

    // Gain nutrition.
    if (you.hunger_state != HS_ENGORGED)
    {
        int food_value = 0;
        if (chunk_type == CE_CLEAN)
            food_value = 30 + random2avg(59, 2);
        else if (chunk_type == CE_CONTAMINATED || chunk_type == CE_POISONOUS)
            food_value = 15 + random2avg(29, 2);

        // Bats get a rather less nutrition out of it.
        if (player_in_bat_form())
            food_value /= 2;

        food_value /= reduction;

        lessen_hunger(food_value, false);
    }

    did_god_conduct(DID_DRINK_BLOOD, 5 + random2(4));

    return (true);
}

bool melee_attack::player_attack()
{
    if (cancel_attack)
        return (false);

    noise_factor = 100;

    player_apply_attack_delay();
    player_stab_check();

    coord_def where = defender->pos();

    if (player_hits_monster())
    {
        did_hit = true;
        if (Hints.hints_left)
            Hints.hints_melee_counter++;

        const bool shield_blocked = attack_shield_blocked(true);

        if (shield_blocked)
            damage_done = 0;
        else
        {
            // This actually does more than calculate damage - it also
            // sets up messages, etc.
            player_calc_hit_damage();
        }

        if (you.duration[DUR_SLIMIFY]
            && mon_can_be_slimified(defender->as_monster()))
        {
            // Bail out after sliming so we don't get aux unarmed and
            // attack a fellow slime.
            damage_done = 0;
            slimify_monster(defender->as_monster());
            you.duration[DUR_SLIMIFY] = 0;
            return (true);
        }

        bool hit_woke_orc = false;
        if (you.religion == GOD_BEOGH
            && defender->mons_species() == MONS_ORC
            && !defender->is_summoned()
            && !defender->as_monster()->is_shapeshifter()
            && !player_under_penance() && you.piety >= piety_breakpoint(2)
            && mons_near(defender->as_monster()) && defender->asleep())
        {
            hit_woke_orc = true;
        }


        if (damage_done > 0
            && defender->can_bleed()
            && !defender->is_summoned()
            && !defender->submerged())
        {
            int blood = _modify_blood_amount(damage_done,
                                             attacker->damage_type());
            if (blood > defender->stat_hp())
                blood = defender->stat_hp();

            bleed_onto_floor(where, defender->id(), blood, true);
        }

        if (damage_done > 0 || !defender_visible && !shield_blocked)
            player_announce_hit();
        else if (!shield_blocked && damage_done <= 0)
        {
            no_damage_message =
                make_stringf("You %s %s.", attack_verb.c_str(),
                             defender->name(DESC_NOCAP_THE).c_str());
        }

        damage_done = defender->hurt(&you, damage_done,
                                     special_damage_flavour, false);

        if (damage_done)
            player_exercise_combat_skills();

        if (player_check_monster_died())
            return (true);

        // Always upset monster regardless of damage.
        // However, successful stabs inhibit shouting.
        behaviour_event(defender->as_monster(), ME_WHACK, MHITYOU,
                        coord_def(), !stab_attempt);

        // [ds] Monster may disappear after behaviour event.
        if (!defender->alive())
            return (true);

        player_sustain_passive_damage();

        // Thirsty stabbing vampires get to draw blood.
        if (you.species == SP_VAMPIRE && you.hunger_state < HS_SATIATED
            && stab_attempt && stab_bonus > 0)
        {
            _player_vampire_draws_blood(defender->as_monster(),
                                        damage_done, true);
        }

        // At this point, pretend we didn't hit at all.
        if (shield_blocked)
            did_hit = false;

        if (hit_woke_orc)
        {
            // Call function of orcs first noticing you, but with
            // beaten-up conversion messages (if applicable).
            beogh_follower_convert(defender->as_monster(), true);
            return (true);
        }
    }
    else
        player_warn_miss();

    if ((did_hit && player_monattk_hit_effects(false))
        || !defender->alive())
    {
        return (true);
    }

    const bool did_primary_hit = did_hit;
    if (unarmed_ok && where == defender->pos() && player_aux_unarmed())
        return (true);

    if ((did_primary_hit || did_hit) && defender->alive()
        && where == defender->pos())
    {
        print_wounds(defender->as_monster());
    }

    return (did_primary_hit || did_hit);
}

void melee_attack::player_aux_setup(unarmed_attack_type atk)
{
    noise_factor = 100;
    aux_attack.clear();
    aux_verb.clear();
    damage_brand = SPWPN_NORMAL;
    aux_damage = 0;

    switch (atk)
    {
    case UNAT_KICK:
        aux_attack = aux_verb = "kick";
        aux_damage = 5;

        if (player_mutation_level(MUT_HOOVES))
        {
            // Max hoof damage: 10.
            aux_damage += player_mutation_level(MUT_HOOVES) * 5 / 3;
        }
        else if (you.has_usable_talons())
        {
            aux_verb = "claw";

            // Max talon damage: 8.
            aux_damage += player_mutation_level(MUT_TALONS);
        }

        break;

    case UNAT_HEADBUTT:
        aux_damage = 5;

        if (player_mutation_level(MUT_BEAK)
            && (!player_mutation_level(MUT_HORNS) || coinflip()))
        {
            aux_attack = aux_verb = "peck";
            aux_damage++;
            noise_factor = 75;
        }
        else
        {
            aux_attack = aux_verb = "headbutt";
            // Minotaurs used to get +5 damage here, now they get
            // +6 because of the horns.
            aux_damage += player_mutation_level(MUT_HORNS) * 3;

            item_def* helmet = you.slot_item(EQ_HELMET);
            if (helmet && is_hard_helmet(*helmet))
            {
                aux_damage += 2;
                if (get_helmet_desc(*helmet) == THELM_DESC_SPIKED
                    || get_helmet_desc(*helmet) == THELM_DESC_HORNED)
                {
                    aux_damage += 3;
                }
            }
        }
        break;

    case UNAT_TAILSLAP:
        aux_attack = aux_verb = "tail-slap";

        // Usually one level, or two for grey draconians.
        aux_damage = 6 * you.has_usable_tail();

        noise_factor = 125;

        if (player_mutation_level(MUT_STINGER) > 0)
        {
            aux_damage += player_mutation_level(MUT_STINGER) * 2 - 1;
            damage_brand = SPWPN_VENOM;
        }

        break;

    case UNAT_PUNCH:
        aux_attack = aux_verb = "punch";
        aux_damage = 5 + you.skills[SK_UNARMED_COMBAT] / 3;

        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
        {
            aux_verb = "slash";
            aux_damage += 6;
            noise_factor = 75;
        }
        else if (you.has_usable_claws())
        {
            aux_verb = "claw";
            aux_damage += roll_dice(1, 3);
        }

        break;

    case UNAT_BITE:
        aux_attack = aux_verb = "bite";
        aux_damage += you.has_usable_fangs() * 2
                      + you.skills[SK_UNARMED_COMBAT] / 5;
        noise_factor = 75;

        // prob of vampiric bite:
        // 1/4 when non-thirsty, 1/2 when thirsty, 100% when
        // bloodless
        if (_vamp_wants_blood_from_monster(defender->as_monster())
            && (you.hunger_state == HS_STARVING
                || you.hunger_state < HS_SATIATED && coinflip()
                || you.hunger_state >= HS_SATIATED && one_chance_in(4)))
        {
            damage_brand = SPWPN_VAMPIRICISM;
        }

        if (player_mutation_level(MUT_ACIDIC_BITE))
        {
            damage_brand = SPWPN_ACID;
            aux_damage += roll_dice(2,4);
        }

        break;

    case UNAT_PSEUDOPODS:
        aux_attack = aux_verb = "slap";
        aux_damage += 4 * you.has_usable_pseudopods();
        noise_factor = 125;
        break;

    default:
        ASSERT(false);
        break;
    }
}

static bool _tran_forbid_aux_attack(unarmed_attack_type atk)
{
    switch (atk)
    {
    case UNAT_KICK:
    case UNAT_HEADBUTT:
    case UNAT_PUNCH:
        return (you.attribute[ATTR_TRANSFORMATION] == TRAN_ICE_BEAST
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_SPIDER
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT);

    case UNAT_TAILSLAP:
        return (you.attribute[ATTR_TRANSFORMATION] == TRAN_SPIDER
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_ICE_BEAST
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT);

    case UNAT_PSEUDOPODS:
        return (you.attribute[ATTR_TRANSFORMATION] != TRAN_NONE);

    default:
        return (false);
    }
}

static bool _extra_aux_attack(unarmed_attack_type atk)
{
    // No extra unarmed attacks for disabled mutations.
    // XXX: It might be better to make player_mutation_level
    //      aware of mutations that are disabled due to transformation.
    if (_tran_forbid_aux_attack(atk))
        return (false);

    switch (atk)
    {
    case UNAT_KICK:
        return ((player_mutation_level(MUT_HOOVES)
                 || you.has_usable_talons())
                && coinflip());

    case UNAT_HEADBUTT:
        return ((player_mutation_level(MUT_HORNS)
                 || player_mutation_level(MUT_BEAK))
                && one_chance_in(3));

    case UNAT_TAILSLAP:
        return (you.has_usable_tail()
                && one_chance_in(4));

    case UNAT_PSEUDOPODS:
        return (you.has_usable_pseudopods()
                && one_chance_in(3));

    case UNAT_BITE:
        return ((you.has_usable_fangs()
                 || player_mutation_level(MUT_ACIDIC_BITE))
                && one_chance_in(5));

    default:
        return (false);
    }
}

unarmed_attack_type melee_attack::player_aux_choose_baseattack()
{
    unarmed_attack_type baseattack = static_cast<unarmed_attack_type>
        (random_choose(UNAT_HEADBUTT, UNAT_KICK, UNAT_PUNCH, UNAT_NO_ATTACK,
                       -1));

    // No punching with a shield or 2-handed wpn, except staves.
    if (baseattack == UNAT_PUNCH && !you.has_usable_offhand())
        baseattack = UNAT_NO_ATTACK;

    if (you.species == SP_NAGA && baseattack == UNAT_KICK)
        baseattack = UNAT_HEADBUTT;

    if (you.has_usable_tail()
        && (baseattack == UNAT_HEADBUTT || baseattack == UNAT_KICK)
        && one_chance_in(3))
    {
        baseattack = UNAT_TAILSLAP;
    }

    if (you.has_usable_pseudopods()
        && baseattack == UNAT_KICK && coinflip())
    {
        baseattack = UNAT_PSEUDOPODS;
    }

    // With fangs, replace head attacks with bites.
    if ((you.has_usable_fangs() || player_mutation_level(MUT_ACIDIC_BITE))
        && (baseattack == UNAT_HEADBUTT
            || baseattack == UNAT_KICK
            || _vamp_wants_blood_from_monster(defender->as_monster())
               && x_chance_in_y(2, 3)))
    {
        baseattack = UNAT_BITE;
    }

    if (_tran_forbid_aux_attack(baseattack))
        baseattack = UNAT_NO_ATTACK;

    return (baseattack);
}

bool melee_attack::player_aux_test_hit()
{
    // XXX We're clobbering did_hit
    did_hit = false;

    const int evasion = defender->melee_evasion(attacker);
    const int helpful_evasion =
        defender->melee_evasion(attacker, EV_IGNORE_HELPLESS);

    // No monster Phase Shift yet
    if (you.religion != GOD_ELYVILON
        && you.penance[GOD_ELYVILON]
        && god_hates_your_god(GOD_ELYVILON, you.religion)
        && to_hit >= evasion
        && one_chance_in(20))
    {
        simple_god_message(" blocks your attack.", GOD_ELYVILON);
        dec_penance(GOD_ELYVILON, 1 + random2(to_hit - evasion));
        return (false);
    }

    bool auto_hit = one_chance_in(30);

    if (!auto_hit && to_hit >= evasion && !(to_hit >= helpful_evasion)
        && defender_visible)
    {
        mprf("Helpless, %s fails to dodge your %s.",
             defender->name(DESC_NOCAP_THE).c_str(),
             aux_attack.c_str());
    }

    if (to_hit >= evasion || auto_hit)
    {
        return (true);
    }
    else
    {
        mprf("Your %s misses %s.", aux_attack.c_str(),
             defender->name(DESC_NOCAP_THE).c_str());
        return (false);
    }
}

// Returns true to end the attack round.
bool melee_attack::player_aux_unarmed()
{
    unwind_var<int> save_brand(damage_brand);

    /*
     * baseattack is the auxiliary unarmed attack the player gets
     * for unarmed combat skill. Note that this can still be skipped,
     * e.g. UNAT_PUNCH with a shield.
     *
     * Then, they can get extra attacks depending on mutations,
     * through _extra_aux_attack().
     */
    unarmed_attack_type baseattack = UNAT_NO_ATTACK;
    if (can_do_unarmed)
        baseattack = player_aux_choose_baseattack();

    for (int i = UNAT_FIRST_ATTACK; i <= UNAT_LAST_ATTACK; ++i)
    {
        if (!defender->alive())
            break;

        unarmed_attack_type atk = static_cast<unarmed_attack_type>(i);

        if (baseattack != atk && !_extra_aux_attack(atk))
            continue;

        // Determine and set damage and attack words.
        player_aux_setup(atk);

        to_hit = random2(calc_your_to_hit_unarmed(atk,
                         damage_brand == SPWPN_VAMPIRICISM));

        make_hungry(2, true);

        handle_noise(defender->pos());
        alert_nearby_monsters();

        // [ds] kraken can flee when near death, causing the tentacle
        // the player was beating up to "die" and no longer be
        // available to answer questions beyond this point.
        // handle_noise stirs up all nearby monsters with a stick, so
        // the player may be beating up a tentacle, but the main body
        // of the kraken still gets a chance to act and submerge
        // tentacles before we get here.
        if (!defender->alive())
            return (true);

        if (player_aux_test_hit())
        {
            // Upset the monster.
            behaviour_event(defender->as_monster(), ME_WHACK, MHITYOU);
            if (!defender->alive())
                return (true);

            if (attack_shield_blocked(true))
                continue;
            if (player_aux_apply())
                return (true);
        }
    }

    return (false);
}

bool melee_attack::player_aux_apply()
{
    did_hit = true;

    aux_damage  = player_aux_stat_modify_damage(aux_damage);
    aux_damage += slaying_bonus(PWPN_DAMAGE);

    aux_damage  = random2(aux_damage);

    aux_damage  = player_apply_fighting_skill(aux_damage, true);
    aux_damage  = player_apply_misc_modifiers(aux_damage);

    // Clear stab bonus which will be set for the primary weapon attack.
    stab_bonus  = 0;
    aux_damage  = player_apply_monster_ac(aux_damage);

    aux_damage  = defender->hurt(&you, aux_damage, BEAM_MISSILE, false);
    damage_done = aux_damage;

    if (damage_done > 0)
    {
        // Clobber wpn_skill.
        wpn_skill   = SK_UNARMED_COMBAT;
        player_exercise_combat_skills();

        mprf("You %s %s%s%s",
             aux_verb.c_str(),
             defender->name(DESC_NOCAP_THE).c_str(),
             debug_damage_number().c_str(),
             attack_strength_punctuation().c_str());

        if (damage_brand == SPWPN_ACID)
        {
            mprf("%s is splashed with acid.",
                 defender->name(DESC_CAP_THE).c_str());
                 corrode_monster(defender->as_monster());
        }

        if (damage_brand == SPWPN_VENOM && coinflip())
            poison_monster(defender->as_monster(), KC_YOU);

        // Normal vampiric biting attack, not if already got stabbing special.
        if (damage_brand == SPWPN_VAMPIRICISM && you.species == SP_VAMPIRE
            && (!stab_attempt || stab_bonus <= 0))
        {
            _player_vampire_draws_blood(defender->as_monster(), damage_done);
        }
    }
    else // no damage was done
    {
        mprf("You %s %s%s.",
             aux_verb.c_str(),
             defender->name(DESC_NOCAP_THE).c_str(),
             you.can_see(defender) ? ", but do no damage" : "");
    }

    if (defender->as_monster()->hit_points < 1)
    {
        _monster_die(defender->as_monster(), KILL_YOU, NON_MONSTER);

        return (true);
    }

    return (false);
}

std::string melee_attack::debug_damage_number()
{
#ifdef DEBUG_DIAGNOSTICS
    return make_stringf(" for %d", damage_done);
#else
    return ("");
#endif
}

std::string melee_attack::special_attack_punctuation()
{
    if (special_damage < 6)
        return ".";
    else
        return "!";
}

std::string melee_attack::attack_strength_punctuation()
{
    if (attacker->atype() == ACT_PLAYER)
    {
        if (damage_done < HIT_WEAK)
            return ".";
        else if (damage_done < HIT_MED)
            return "!";
        else if (damage_done < HIT_STRONG)
            return "!!";
        else
            return "!!!";
    }
    else
    {
        return (damage_done < HIT_WEAK ? "." : "!");
    }
}

void melee_attack::player_announce_hit()
{
    if (!verb_degree.empty() && verb_degree[0] != ' ')
        verb_degree = " " + verb_degree;

    msg::stream << "You " << attack_verb << ' '
                << defender->name(DESC_NOCAP_THE)
                << verb_degree << debug_damage_number()
                << attack_strength_punctuation()
                << std::endl;
}

std::string melee_attack::player_why_missed()
{
    const int ev = defender->melee_evasion(attacker);
    const int combined_penalty =
        player_armour_tohit_penalty + player_shield_tohit_penalty;
    if (to_hit < ev && to_hit + combined_penalty >= ev)
    {
        const bool armour_miss =
            (player_armour_tohit_penalty
             && to_hit + player_armour_tohit_penalty >= ev);
        const bool shield_miss =
            (player_shield_tohit_penalty
             && to_hit + player_shield_tohit_penalty >= ev);

        const item_def *armour = you.slot_item(EQ_BODY_ARMOUR, false);
        const std::string armour_name =
            (armour? armour->name(DESC_BASENAME) : std::string("armour"));

        if (armour_miss && !shield_miss)
            return "Your " + armour_name + " prevents you from hitting ";
        else if (shield_miss && !armour_miss)
            return "Your shield prevents you from hitting ";
        else
            return ("Your shield and " + armour_name
                    + " prevent you from hitting ");
    }
    return "You miss ";
}

void melee_attack::player_warn_miss()
{
    did_hit = false;

    // Upset only non-sleeping monsters if we missed.
    if (!defender->asleep())
        behaviour_event(defender->as_monster(), ME_WHACK, MHITYOU);

    msg::stream << player_why_missed()
                << defender->name(DESC_NOCAP_THE)
                << "."
                << std::endl;
}

bool melee_attack::player_hits_monster()
{
    const int evasion = defender->melee_evasion(attacker);
    const int evasion_helpful
        = defender->melee_evasion(attacker, EV_IGNORE_HELPLESS);
    dprf("your to-hit: %d; defender effective EV: %d", to_hit, evasion);

    if (to_hit >= evasion_helpful || one_chance_in(20))
    {
        return (true);
    }

    if (to_hit >= evasion
        || ((defender->cannot_act() || defender->asleep())
            && !one_chance_in(10 + you.skills[SK_STABBING]))
        || defender->as_monster()->petrifying()
           && !one_chance_in(2 + you.skills[SK_STABBING]))
    {
        if (defender_visible)
            msg::stream << "Helpless, " << defender->name(DESC_NOCAP_THE)
                        << " fails to dodge your attack." << std::endl;
        return (true);
    }

    return (false);
}

int melee_attack::player_stat_modify_damage(int damage)
{
    int dammod = 78;
    const int dam_stat_val = calc_stat_to_dam_base();

    if (dam_stat_val > 11)
        dammod += (random2(dam_stat_val - 11) * 2);
    else if (dam_stat_val < 9)
        dammod -= (random2(9 - dam_stat_val) * 3);

    damage *= dammod;
    damage /= 78;

    return (damage);
}

int melee_attack::player_aux_stat_modify_damage(int damage)
{
    int dammod = 10;
    const int dam_stat_val = calc_stat_to_dam_base();

    if (dam_stat_val > 11)
        dammod += random2(dam_stat_val - 11) / 3;
    else if (dam_stat_val < 9)
        dammod -= random2(9 - dam_stat_val) / 2;

    damage *= dammod;
    damage /= 10;

    return (damage);
}

int melee_attack::player_apply_weapon_skill(int damage)
{
    if (weapon && (weapon->base_type == OBJ_WEAPONS
                   || weapon->base_type == OBJ_STAVES))
    {
        damage *= 25 + (random2( you.skills[ wpn_skill ] + 1 ));
        damage /= 25;
    }

    return (damage);
}

int melee_attack::player_apply_fighting_skill(int damage, bool aux)
{
    const int base = aux? 40 : 30;

    damage *= base + (random2(you.skills[SK_FIGHTING] + 1));
    damage /= base;

    return (damage);
}

int melee_attack::player_apply_misc_modifiers(int damage)
{
    if (you.duration[DUR_MIGHT] > 1)
        damage += 1 + random2(10);

    if (you.species != SP_VAMPIRE && you.hunger_state == HS_STARVING)
        damage -= random2(5);

    return (damage);
}

int melee_attack::player_apply_weapon_bonuses(int damage)
{
    if (weapon && (weapon->base_type == OBJ_WEAPONS
                   || item_is_rod( *weapon )))
    {
        int wpn_damage_plus = weapon->plus2;

        if (item_is_rod( *weapon ))
            wpn_damage_plus = (short)weapon->props["rod_enchantment"];

        damage += (wpn_damage_plus > -1) ? (random2(1 + wpn_damage_plus))
                                         : -(1 + random2(-wpn_damage_plus));

        if (get_equip_race(*weapon) == ISFLAG_DWARVEN
            && player_genus(GENPC_DWARVEN))
        {
            damage += random2(3);
        }

        if (get_equip_race(*weapon) == ISFLAG_ORCISH
            && you.species == SP_HILL_ORC)
        {
            if (you.religion == GOD_BEOGH && !player_under_penance())
            {
#ifdef DEBUG_DIAGNOSTICS
                const int orig_damage = damage;
#endif
                if (you.piety > 80 || coinflip())
                    damage++;

                damage +=
                    random2avg(
                        div_rand_round(
                            std::min(static_cast<int>(you.piety), 180), 33), 2);
#ifdef DEBUG_DIAGNOSTICS
                mprf(MSGCH_DIAGNOSTICS, "Damage: %d -> %d, Beogh bonus: %d",
                     orig_damage, damage, damage - orig_damage);
#endif
            }

            if (coinflip())
                damage++;
        }

        // Demonspawn get a damage bonus for demonic weapons.
        if (you.species == SP_DEMONSPAWN && is_demonic(*weapon))
            damage += random2(3);
    }

    return (damage);
}

void melee_attack::player_weapon_auto_id()
{
    if (weapon
        && weapon->base_type == OBJ_WEAPONS
        && !is_range_weapon( *weapon )
        && !item_ident( *weapon, ISFLAG_KNOW_PLUSES )
        && x_chance_in_y(you.skills[ wpn_skill ], 100))
    {
        set_ident_flags( *weapon, ISFLAG_KNOW_PLUSES );
        mprf("You are wielding %s.", weapon->name(DESC_NOCAP_A).c_str());
        more();
        you.wield_change = true;
    }
}

int melee_attack::player_stab_weapon_bonus(int damage)
{
    if (weapon && weapon->base_type == OBJ_WEAPONS
        && (weapon->sub_type == WPN_CLUB
            || weapon->sub_type == WPN_SPEAR
            || weapon->sub_type == WPN_TRIDENT
            || weapon->sub_type == WPN_DEMON_TRIDENT))
    {
        goto ok_weaps;
    }

    switch (wpn_skill)
    {
    case SK_SHORT_BLADES:
    {
        int bonus = (you.dex() * (you.skills[SK_STABBING] + 1)) / 5;

        if (weapon->sub_type != WPN_DAGGER)
            bonus /= 2;

        bonus   = stepdown_value( bonus, 10, 10, 30, 30 );
        damage += bonus;
    }
    // fall through
    ok_weaps:
    case SK_LONG_BLADES:
        damage *= 10 + you.skills[SK_STABBING] /
                       (stab_bonus + (wpn_skill == SK_SHORT_BLADES ? 0 : 2));
        damage /= 10;
        // fall through
    default:
        damage *= 12 + you.skills[SK_STABBING] / stab_bonus;
        damage /= 12;
    }

    return (damage);
}

int melee_attack::player_stab(int damage)
{
    // The stabbing message looks better here:
    if (stab_attempt)
    {
        // Construct reasonable message.
        stab_message(defender, stab_bonus);

        exercise(SK_STABBING, 1 + random2avg(5, 4));

        did_god_conduct(DID_STABBING, 4);
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
        damage = std::max(1, damage);

        if (defender->asleep())
        {
            // Sleeping moster wakes up when stabbed but may be groggy.
            if (x_chance_in_y(you.skills[SK_STABBING] + you.dex() + 1, 200))
            {
                int stun = random2(you.dex() + 1);

                if (defender->as_monster()->speed_increment > stun)
                    defender->as_monster()->speed_increment -= stun;
                else
                    defender->as_monster()->speed_increment = 0;
            }
        }

        damage = player_stab_weapon_bonus(damage);
    }

    return (damage);
}

int melee_attack::player_apply_monster_ac(int damage)
{
    if (stab_bonus)
    {
        // When stabbing we can get by some of the armour.
        if (defender->armour_class() > 0)
        {
            const int ac = defender->armour_class()
                - random2(you.skills[SK_STABBING] / stab_bonus);

            if (ac > 0)
                damage -= random2(1 + ac);
        }
    }
    else
    {
        // Apply AC normally.
        if (defender->armour_class() > 0)
            damage -= random2(1 + defender->armour_class());
    }

    if (defender->petrified())
        damage /= 3;

    return (damage);
}

int melee_attack::player_weapon_type_modify(int damage)
{
    int weap_type = WPN_UNKNOWN;

    if (!weapon)
        weap_type = WPN_UNARMED;
    else if (item_is_staff(*weapon))
        weap_type = WPN_QUARTERSTAFF;
    else if (item_is_rod(*weapon))
        weap_type = WPN_CLUB;
    else if (weapon->base_type == OBJ_WEAPONS)
        weap_type = weapon->sub_type;

    // All weak hits look the same, except for when the player
    // has a non-weapon in hand. - bwr
    // Exception: vampire bats only bite to allow for drawing blood.
    if (damage < HIT_WEAK
        && (you.species != SP_VAMPIRE || !player_in_bat_form()))
    {
        if (weap_type != WPN_UNKNOWN)
            attack_verb = "hit";
        else
            attack_verb = "clumsily bash";

        return (damage);
    }

    // Take transformations into account, if no weapon is wielded.
    if (weap_type == WPN_UNARMED
        && you.attribute[ATTR_TRANSFORMATION] != TRAN_NONE)
    {
        switch (you.attribute[ATTR_TRANSFORMATION])
        {
        case TRAN_SPIDER:
        case TRAN_BAT:
        case TRAN_PIG:
            if (damage < HIT_STRONG)
                attack_verb = "bite";
            else
                attack_verb = "maul";
            break;
        case TRAN_BLADE_HANDS:
            if (damage < HIT_MED)
                attack_verb = "slash";
            else if (damage < HIT_STRONG)
                attack_verb = "slice";
            else
                attack_verb = "shred";
            break;
        case TRAN_STATUE:
        case TRAN_LICH:
            if (you.has_usable_claws())
            {
                if (damage < HIT_MED)
                    attack_verb = "claw";
                else if (damage < HIT_STRONG)
                    attack_verb = "mangle";
                else
                    attack_verb = "eviscerate";
                break;
            }
            // or fall-through
        case TRAN_ICE_BEAST:
            if (damage < HIT_MED)
                attack_verb = "punch";
            else
                attack_verb = "pummel";
            break;
        case TRAN_DRAGON:
            if (damage < HIT_MED)
                attack_verb = "claw";
            else if (damage < HIT_STRONG)
                attack_verb = "bite";
            else
            {
                attack_verb = "maul";
                if (defender->body_size() <= SIZE_MEDIUM && coinflip())
                    attack_verb = "trample on";
            }
            break;
        } // transformations

        return (damage);
    }

    // Take normal hits into account.  If the hit is from a weapon with
    // more than one damage type, randomly choose one damage type from
    // it.
    switch (weapon ? single_damage_type(*weapon) : -1)
    {
    case DAM_PIERCE:
        if (damage < HIT_MED)
            attack_verb = "puncture";
        else if (damage < HIT_STRONG)
            attack_verb = "impale";
        else
        {
            attack_verb = "spit";
            if (defender->atype() == ACT_MONSTER
                && defender_visible
                && mons_genus(defender->as_monster()->type) == MONS_HOG)
            {
                verb_degree = " like the proverbial pig";
            }
            else
                verb_degree = " like a pig";
        }
        break;

    case DAM_SLICE:
        if (damage < HIT_MED)
            attack_verb = "slash";
        else if (damage < HIT_STRONG)
            attack_verb = "slice";
        else if (mons_genus(defender->as_monster()->type) == MONS_OGRE)
        {
            attack_verb = "dice";
            verb_degree = " like an onion";
        }
        else
        {
            attack_verb = "open";
            verb_degree = " like a pillowcase";
        }
        break;

    case DAM_BLUDGEON:
        if (damage < HIT_MED)
            attack_verb = one_chance_in(4) ? "thump" : "sock";
        else if (damage < HIT_STRONG)
            attack_verb = "bludgeon";
        else
        {
            attack_verb = "crush";
            verb_degree = " like a grape";
        }
        break;

    case DAM_WHIP:
        if (damage < HIT_MED)
            attack_verb = "whack";
        else if (damage < HIT_STRONG)
            attack_verb = "thrash";
        else
        {
            switch(defender->holiness())
            {
            case MH_HOLY:
            case MH_NATURAL:
            case MH_DEMONIC:
                attack_verb = "punish";
                verb_degree = " causing immense pain";
                break;
            default:
                attack_verb = "devastate";
            }
        }
        break;

    case -1: // unarmed
        if (you.damage_type() == DVORP_CLAWING)
        {
            if (damage < HIT_MED)
                attack_verb = "claw";
            else if (damage < HIT_STRONG)
                attack_verb = "mangle";
            else
                attack_verb = "eviscerate";
        }
        else
        {
            if (damage < HIT_MED)
                attack_verb = "punch";
            else
                attack_verb = "pummel";
        }
        break;

    case WPN_UNKNOWN:
    default:
        attack_verb = "hit";
        break;
    }

    return (damage);
}

void melee_attack::player_exercise_combat_skills()
{
    const bool helpless = defender->cannot_fight();

    if (!helpless || you.skills[wpn_skill] < 1)
        exercise(wpn_skill, 1);

    if ((!helpless || you.skills[SK_FIGHTING] < 1)
        && one_chance_in(3))
    {
        exercise(SK_FIGHTING, 1);
    }
}

void melee_attack::player_check_weapon_effects()
{
    if (weapon && weapon->base_type == OBJ_WEAPONS)
    {
        if (is_holy_item(*weapon))
            did_god_conduct(DID_HOLY, 1);
        else if (is_demonic(*weapon))
            did_god_conduct(DID_UNHOLY, 1);
        else if (get_weapon_brand(*weapon) == SPWPN_SPEED
                || weapon->sub_type == WPN_QUICK_BLADE)
        {
            did_god_conduct(DID_HASTY, 1);
        }
    }
}

// Effects that occur after all other effects, even if the monster is dead.
// For example, explosions that would hit other creatures, but we want
// to deal with only one creature at a time, so that's handled last.
// You probably want to call player_monattk_hit_effects instead, as that
// function calls this one.
// Returns true if the combat round should end here.
bool melee_attack::player_monattk_final_hit_effects(bool mondied)
{
    for (unsigned int i = 0; i < final_effects.size(); ++i)
    {
        switch (final_effects[i].flavor)
        {
        case FINEFF_LIGHTNING_DISCHARGE:
            if (you.see_cell(final_effects[i].location))
                mpr("Electricity arcs through the water!");
            conduct_electricity(final_effects[i].location, attacker);
            break;
        }
    }

    return mondied;
}

// Returns true if the combat round should end here.
bool melee_attack::player_monattk_hit_effects(bool mondied)
{
    player_check_weapon_effects();

    mondied = check_unrand_effects(mondied) || mondied;

    // Thirsty vampires will try to use a stabbing situation to draw blood.
    if (you.species == SP_VAMPIRE && you.hunger_state < HS_SATIATED
        && mondied && stab_attempt && stab_bonus > 0
        && _player_vampire_draws_blood(defender->as_monster(),
                                       damage_done, true))
    {
        // No further effects.
    }
    else if (you.species == SP_VAMPIRE
             && damage_brand == SPWPN_VAMPIRICISM
             && you.weapon()
             && _player_vampire_draws_blood(defender->as_monster(),
                                            damage_done, false,
                                            (mondied ? 1 : 10)))
    {
        // No further effects.
    }
    // Vampiric *weapon* effects for the killing blow.
    else if (mondied && damage_brand == SPWPN_VAMPIRICISM
             && you.weapon()
             && you.species != SP_VAMPIRE) // vampires get their bonus elsewhere
    {
        if (defender->holiness() == MH_NATURAL
            && !defender->is_summoned()
            && damage_done > 0
            && you.hp < you.hp_max
            && !one_chance_in(5))
        {
            mpr("You feel better.");

            // More than if not killed.
            const int heal = 1 + random2(damage_done);

            dprf("Vampiric healing: damage %d, healed %d",
                 damage_done, heal);
            inc_hp(heal, false);

            if (you.hunger_state != HS_ENGORGED)
                lessen_hunger(30 + random2avg(59, 2), false);

            did_god_conduct(DID_NECROMANCY, 2);
        }
    }

    if (mondied)
        return player_monattk_final_hit_effects(true);

    // These effects apply only to monsters that are still alive:

    // Returns true if a head was cut off *and* the wound was cauterized,
    // in which case the cauterization was the ego effect, so don't burn
    // the hydra some more.
    //
    // Also returns true if the hydra's last head was cut off, in which
    // case nothing more should be done to the hydra.
    if (decapitate_hydra(damage_done))
        return player_monattk_final_hit_effects(!defender->alive());

    // These two (staff damage and damage brand) are mutually exclusive!
    player_apply_staff_damage();

    // Returns true if the monster croaked.
    if (!special_damage && apply_damage_brand())
        return player_monattk_final_hit_effects(true);

    if (!no_damage_message.empty())
    {
        if (special_damage > 0)
            emit_nodmg_hit_message();
        else
        {
            mprf("You %s %s, but do no damage.",
                 attack_verb.c_str(),
                 defender->name(DESC_NOCAP_THE).c_str());
        }
    }

    if (needs_message && !special_damage_message.empty())
    {
        mprf("%s", special_damage_message.c_str());
        // Don't do a message-only miscast right after a special damage.
        if (miscast_level == 0)
            miscast_level = -1;
    }

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Special damage to %s: %d, flavour: %d",
         defender->name(DESC_NOCAP_THE).c_str(),
         special_damage, special_damage_flavour);
#endif

    special_damage = defender->hurt(&you, special_damage, special_damage_flavour, false);

    if (!defender->alive())
    {
        _monster_die(defender->as_monster(), KILL_YOU, NON_MONSTER);

        return player_monattk_final_hit_effects(true);
    }

    if (stab_attempt && stab_bonus > 0 && weapon
        && weapon->base_type == OBJ_WEAPONS && weapon->sub_type == WPN_CLUB
        && damage_done + special_damage > random2(defender->get_experience_level())
        && !defender->as_monster()->has_ench(ENCH_CONFUSION)
        && mons_class_is_confusable(defender->id()))
    {
        if (defender->as_monster()->add_ench(mon_enchant(ENCH_CONFUSION, 0,
            KC_YOU, 20+random2(30)))) // 1-3 turns
        {
            mprf("%s is stunned!", defender->name(DESC_CAP_THE).c_str());
        }
    }

    return player_monattk_final_hit_effects(false);
}

void melee_attack::_monster_die(monsters* monster, killer_type killer,
                                int killer_index)
{
    const bool chaos = (damage_brand == SPWPN_CHAOS);
    const bool reaping = (damage_brand == SPWPN_REAPING);

    // Copy defender before it gets reset by monster_die().
    monsters* def_copy = NULL;
    if (chaos || reaping)
        def_copy = new monsters(*monster);

    int corpse = monster_die(monster, killer, killer_index);

    if (chaos)
    {
        chaos_killed_defender(def_copy);
        delete def_copy;
    }
    else if (reaping)
    {
        if (corpse != -1)
            mons_reaped(attacker, def_copy);
        delete def_copy;
    }
}

static bool is_boolean_resist(beam_type flavour)
{
    switch (flavour)
    {
    case BEAM_ELECTRICITY:
    case BEAM_MIASMA: // rotting
    case BEAM_NAPALM:
    case BEAM_WATER:  // water asphyxiation damage,
                      // bypassed by being water inhabitant.
        return (true);
    default:
        return (false);
    }
}

// Gets the percentage of the total damage of this damage flavour that can
// be resisted.
static inline int get_resistible_fraction(beam_type flavour)
{
    switch (flavour)
    {
    // Drowning damage from water is resistible by being a water thing, or
    // otherwise asphyx resistant.
    case BEAM_WATER:
        return (40);

    // Assume ice storm and throw icicle are mostly solid.
    case BEAM_ICE:
        return (25);

    case BEAM_LAVA:
        return (35);

    case BEAM_POISON_ARROW:
        return (70);

    default:
        return (100);
    }
}

// Adjusts damage for elemental resists, electricity and poison.
//
// FIXME: Does not (yet) handle life draining, player acid damage
// (does handle monster acid damage), miasma, and other exotic
// attacks.
//
// beam_type is just used to determine the damage flavour, it does not
// necessarily imply that the attack is a beam attack.
int resist_adjust_damage(actor *defender, beam_type flavour,
                         int res, int rawdamage, bool ranged)
{
    if (!res)
        return (rawdamage);

    const bool monster = (defender->atype() == ACT_MONSTER);

    const int resistible_fraction = get_resistible_fraction(flavour);

    int resistible = rawdamage * resistible_fraction / 100;
    const int irresistible = rawdamage - resistible;

    if (res > 0)
    {
        if (monster && res >= 3)
            resistible = 0;
        else
        {
            // Check if this is a resist that pretends to be boolean for
            // damage purposes.  Only electricity, miasma and sticky
            // flame (napalm) do this at the moment; raw poison damage
            // uses the normal formula.
            const int bonus_res = (is_boolean_resist(flavour) ? 1 : 0);

            // Use a new formula for players, but keep the old, more
            // effective one for monsters.
            if (monster)
                resistible /= 1 + bonus_res + res * res;
            else
                resistible /= resist_fraction(res, bonus_res);
        }
    }
    else if (res < 0)
        resistible = resistible * (ranged? 15 : 20) / 10;

    return std::max(resistible + irresistible, 0);
}

void melee_attack::calc_elemental_brand_damage( beam_type flavour,
                                                int res,
                                                const char *verb)
{
    special_damage = resist_adjust_damage(defender, flavour, res,
                                          random2(damage_done) / 2 + 1);

    if (needs_message && special_damage > 0 && verb)
    {
        special_damage_message = make_stringf(
            "%s %s %s%s",
            atk_name(DESC_CAP_THE).c_str(),
            attacker->conj_verb(verb).c_str(),
            mons_defender_name().c_str(),
            special_attack_punctuation().c_str());
    }
}

int melee_attack::fire_res_apply_cerebov_downgrade(int res)
{
    if (weapon && weapon->special == UNRAND_CEREBOV)
        --res;

    return (res);
}

void melee_attack::drain_defender()
{
    if (defender->atype() == ACT_MONSTER && one_chance_in(3))
        return;

    special_damage = 1 + random2(damage_done)
                         / (2 + defender->res_negative_energy());

    if (defender->drain_exp(attacker, true))
    {
        if (defender->atype() == ACT_PLAYER)
            obvious_effect = true;
        else if (defender_visible)
        {
            special_damage_message =
                make_stringf(
                    "%s %s %s!",
                    atk_name(DESC_CAP_THE).c_str(),
                    attacker->conj_verb("drain").c_str(),
                    mons_defender_name().c_str());
        }

        attacker->god_conduct(DID_NECROMANCY, 2);
    }
}

void melee_attack::rot_defender(int amount, int immediate)
{
    if (defender->rot(attacker, amount, immediate, true))
    {
        if (defender->atype() == ACT_MONSTER && defender_visible)
        {
            special_damage_message =
                make_stringf(
                    "%s %s!",
                    def_name(DESC_CAP_THE).c_str(),
                    amount > 0 ? "rots" : "looks less resilient");
        }
    }
}

bool melee_attack::distortion_affects_defender()
{
    //jmf: blink frogs *like* distortion
    // I think could be amended to let blink frogs "grow" like
    // jellies do {dlb}
    if (defender->atype() == ACT_MONSTER
        && mons_genus(defender->as_monster()->type) == MONS_BLINK_FROG)
    {
        if (one_chance_in(5))
        {
            emit_nodmg_hit_message();
            if (defender_visible)
            {
                special_damage_message =
                    make_stringf("%s %s in the distortional energy.",
                                 def_name(DESC_CAP_THE).c_str(),
                                 defender->conj_verb("bask").c_str());
            }

            defender->heal(1 + random2avg(7, 2), true); // heh heh
        }
        return (false);
    }

    if (one_chance_in(3))
    {
        if (defender_visible)
        {
            special_damage_message =
                make_stringf(
                    "Space bends around %s.",
                def_name(DESC_NOCAP_THE).c_str());
        }
        special_damage += 1 + random2avg(7, 2);
        return (false);
    }

    if (one_chance_in(3))
    {
        if (defender_visible)
        {
            special_damage_message =
                make_stringf(
                    "Space warps horribly around %s!",
                    def_name(DESC_NOCAP_THE).c_str());
        }
        special_damage += 3 + random2avg(24, 2);
        return (false);
    }

    if (one_chance_in(3))
    {
        emit_nodmg_hit_message();
        if (defender_visible)
            obvious_effect = true;
        defender->blink();
        return (false);
    }

    // Used to be coinflip() || coinflip() for players, just coinflip()
    // for monsters; this is a compromise. Note that it makes banishment
    // a touch more likely for players, and a shade less likely for
    // monsters.
    if (!one_chance_in(3))
    {
        emit_nodmg_hit_message();
        if (defender_visible)
            obvious_effect = true;
        defender->teleport(coinflip(), one_chance_in(5));
        return (false);
    }

    if (you.level_type != LEVEL_ABYSS && coinflip())
    {
        emit_nodmg_hit_message();

        if (defender->atype() == ACT_PLAYER && attacker_visible
            && weapon != NULL && !is_unrandom_artefact(*weapon)
            && !is_special_unrandom_artefact(*weapon))
        {
            // If the player is being sent to the Abyss by being attacked
            // with a distortion weapon, then we have to ID it before
            // the player goes to Abyss, while the weapon object is
            // still in memory.
            if (is_artefact(*weapon))
                artefact_wpn_learn_prop(*weapon, ARTP_BRAND);
            else
                set_ident_flags(*weapon, ISFLAG_KNOW_TYPE);
        }
        else if (defender_visible)
            obvious_effect = true;

        defender->banish(attacker->name(DESC_PLAIN, true));
        return (true);
    }

    return (false);
}

enum chaos_type
{
    CHAOS_CLONE,
    CHAOS_POLY,
    CHAOS_POLY_UP,
    CHAOS_MAKE_SHIFTER,
    CHAOS_MISCAST,
    CHAOS_RAGE,
    CHAOS_HEAL,
    CHAOS_HASTE,
    CHAOS_INVIS,
    CHAOS_SLOW,
    CHAOS_PARALYSIS,
    CHAOS_PETRIFY,
    NUM_CHAOS_TYPES
};

// XXX: We might want to vary the probabilities for the various effects
// based on whether the source is a weapon of chaos or a monster with
// AF_CHAOS.
void melee_attack::chaos_affects_defender()
{
    const bool mon        = defender->atype() == ACT_MONSTER;
    const bool immune     = mon && mons_immune_magic(defender->as_monster());
    const bool is_natural = mon && defender->holiness() == MH_NATURAL;
    const bool is_shifter = mon && defender->as_monster()->is_shapeshifter();
    const bool can_clone  = mon && defender->is_holy()
                            && mons_clonable(defender->as_monster(), true);
    const bool can_poly   = is_shifter || (defender->can_safely_mutate()
                                           && !immune);
    const bool can_rage   = defender->can_go_berserk();
    const bool can_slow   = !mon || !mons_is_firewood(defender->as_monster());

    int clone_chance   = can_clone                      ?  1 : 0;
    int poly_chance    = can_poly                       ?  1 : 0;
    int poly_up_chance = can_poly                && mon ?  1 : 0;
    int shifter_chance = can_poly  && is_natural && mon ?  1 : 0;
    int rage_chance    = can_rage                       ? 10 : 0;
    int miscast_chance = 10;
    int slowpara_chance= can_slow                       ? 10 : 0;

    // Already a shifter?
    if (is_shifter)
        shifter_chance = 0;

    // A chaos self-attack increases the chance of certain effects,
    // due to a short-circuit/feedback/resonance/whatever.
    if (attacker == defender)
    {
        clone_chance   *= 2;
        poly_chance    *= 2;
        poly_up_chance *= 2;
        shifter_chance *= 2;
        miscast_chance *= 2;

        // Inform player that something is up.
        if (you.see_cell(defender->pos()))
        {
            if (defender->atype() == ACT_PLAYER)
                mpr("You give off a flash of multicoloured light!");
            else if (you.can_see(defender))
            {
                simple_monster_message(defender->as_monster(),
                                       " gives off a flash of "
                                       "multicoloured light!");
            }
            else
                mpr("There is a flash of multicoloured light!");
        }
    }

    // NOTE: Must appear in exact same order as in chaos_type enumeration.
    int probs[NUM_CHAOS_TYPES] =
    {
        clone_chance,   // CHAOS_CLONE
        poly_chance,    // CHAOS_POLY
        poly_up_chance, // CHAOS_POLY_UP
        shifter_chance, // CHAOS_MAKE_SHIFTER
        miscast_chance, // CHAOS_MISCAST
        rage_chance,    // CHAOS_RAGE

        10, // CHAOS_HEAL
        slowpara_chance,// CHAOS_HASTE
        10, // CHAOS_INVIS

        slowpara_chance,// CHAOS_SLOW
        slowpara_chance,// CHAOS_PARALYSIS
        slowpara_chance,// CHAOS_PETRIFY
    };

    bolt beam;
    beam.flavour = BEAM_NONE;

    int choice = choose_random_weighted(probs, probs + NUM_CHAOS_TYPES);
#ifdef NOTE_DEBUG_CHAOS_EFFECTS
    std::string chaos_effect = "CHAOS effect: ";
    switch (choice)
    {
    case CHAOS_CLONE:           chaos_effect += "clone"; break;
    case CHAOS_POLY:            chaos_effect += "polymorph"; break;
    case CHAOS_POLY_UP:         chaos_effect += "polymorph PPT_MORE"; break;
    case CHAOS_MAKE_SHIFTER:    chaos_effect += "shifter"; break;
    case CHAOS_MISCAST:         chaos_effect += "miscast"; break;
    case CHAOS_RAGE:            chaos_effect += "berserk"; break;
    case CHAOS_HEAL:            chaos_effect += "healing"; break;
    case CHAOS_HASTE:           chaos_effect += "hasting"; break;
    case CHAOS_INVIS:           chaos_effect += "invisible"; break;
    case CHAOS_SLOW:            chaos_effect += "slowing"; break;
    case CHAOS_PARALYSIS:       chaos_effect += "paralysis"; break;
    case CHAOS_PETRIFY:         chaos_effect += "petrify"; break;
    default:                    chaos_effect += "(other)"; break;
    }

    take_note(Note(NOTE_MESSAGE, 0, 0, chaos_effect.c_str()), true);
#endif

    switch (static_cast<chaos_type>(choice))
    {
    case CHAOS_CLONE:
    {
        ASSERT(can_clone && clone_chance > 0);
        ASSERT(defender->atype() == ACT_MONSTER);

        int clone_idx = clone_mons(defender->as_monster(), true,
                                   &obvious_effect);
        if (clone_idx != NON_MONSTER)
        {
            if (obvious_effect)
            {
                special_damage_message =
                    make_stringf("%s is duplicated!",
                                 def_name(DESC_NOCAP_THE).c_str());
            }

            monsters &clone(menv[clone_idx]);
            // The player shouldn't get new permanent followers from cloning.
            if (clone.attitude == ATT_FRIENDLY && !clone.is_summoned())
                clone.mark_summoned(6, true, MON_SUMM_CLONE);

            // Monsters being cloned is interesting.
            xom_is_stimulated(clone.friendly() ? 16 : 32);
        }
        break;
    }

    case CHAOS_POLY:
        ASSERT(can_poly && poly_chance > 0);
        beam.flavour = BEAM_POLYMORPH;
        break;

    case CHAOS_POLY_UP:
        ASSERT(can_poly && poly_up_chance > 0);
        ASSERT(defender->atype() == ACT_MONSTER);

        obvious_effect = you.can_see(defender);
        monster_polymorph(defender->as_monster(), RANDOM_MONSTER, PPT_MORE);
        break;

    case CHAOS_MAKE_SHIFTER:
    {
        ASSERT(can_poly && shifter_chance > 0);
        ASSERT(!is_shifter);
        ASSERT(defender->atype() == ACT_MONSTER);

        obvious_effect = you.can_see(defender);
        defender->as_monster()->add_ench(one_chance_in(3) ?
            ENCH_GLOWING_SHAPESHIFTER : ENCH_SHAPESHIFTER);
        // Immediately polymorph monster, just to make the effect obvious.
        monster_polymorph(defender->as_monster(), RANDOM_MONSTER);

        // Xom loves it if this happens!
        const int friend_factor = defender->as_monster()->friendly() ? 1 : 2;
        const int glow_factor   =
            (defender->as_monster()->has_ench(ENCH_SHAPESHIFTER) ? 1 : 2);
        xom_is_stimulated( 64 * friend_factor * glow_factor );
        break;
    }
    case CHAOS_MISCAST:
    {
        int level = defender->get_experience_level();

        // At level == 27 there's a 13.9% chance of a level 3 miscast.
        int level0_chance = level;
        int level1_chance = std::max( 0, level - 7);
        int level2_chance = std::max( 0, level - 12);
        int level3_chance = std::max( 0, level - 17);

        level = random_choose_weighted(
            level0_chance, 0,
            level1_chance, 1,
            level2_chance, 2,
            level3_chance, 3,
            0);

        miscast_level  = level;
        miscast_type   = SPTYP_RANDOM;
        miscast_target = one_chance_in(3) ? attacker : defender;
        break;
    }

    case CHAOS_RAGE:
        ASSERT(can_rage && rage_chance > 0);
        defender->go_berserk(false);
        obvious_effect = you.can_see(defender);
        break;

    case CHAOS_HEAL:
        beam.flavour = BEAM_HEALING;
        break;

    case CHAOS_HASTE:
        beam.flavour = BEAM_HASTE;
        break;

    case CHAOS_INVIS:
        beam.flavour = BEAM_INVISIBILITY;
        break;

    case CHAOS_SLOW:
        beam.flavour = BEAM_SLOW;
        break;

    case CHAOS_PARALYSIS:
        beam.flavour = BEAM_PARALYSIS;
        break;

    case CHAOS_PETRIFY:
        beam.flavour = BEAM_PETRIFY;
        break;

    default:
        ASSERT(!"Invalid chaos effect type");
        break;
    }

    if (beam.flavour != BEAM_NONE)
    {
        beam.glyph        = 0;
        beam.range        = 0;
        beam.colour       = BLACK;
        beam.effect_known = false;

        if (weapon && you.can_see(attacker))
        {
            beam.name = wep_name(DESC_NOCAP_YOUR);
            beam.item = weapon;
        }
        else
            beam.name = atk_name(DESC_NOCAP_THE);

        beam.thrower =
            (attacker->atype() == ACT_PLAYER)          ? KILL_YOU
            : attacker->as_monster()->confused_by_you() ? KILL_YOU_CONF
                                                       : KILL_MON;

        if (beam.thrower == KILL_YOU || attacker->as_monster()->friendly())
            beam.attitude = ATT_FRIENDLY;

        beam.beam_source = attacker->mindex();

        beam.source = defender->pos();
        beam.target = defender->pos();

        beam.damage = dice_def(damage_done + special_damage + aux_damage, 1);

        beam.ench_power = beam.damage.num;

        beam.fire();

        if (you.can_see(defender))
            obvious_effect = beam.obvious_effect;
    }

    if (!you.can_see(attacker))
        obvious_effect = false;
}

static bool _move_stairs(const actor* attacker, const actor* defender)
{
    const coord_def orig_pos  = attacker->pos();
    const dungeon_feature_type stair_feat = grd(orig_pos);

    if (feat_stair_direction(stair_feat) == CMD_NO_CMD)
        return (false);

    // The player can't use shops to escape, so don't bother.
    if (stair_feat == DNGN_ENTER_SHOP)
        return (false);

    // Don't move around notable terrain the player is aware of if it's
    // out of sight.
    if (is_notable_terrain(stair_feat)
        && is_terrain_known(orig_pos.x, orig_pos.y) && !you.see_cell(orig_pos))
    {
        return (false);
    }

    coord_def dest(-1, -1);
    // Prefer to send it under the defender.
    if (defender->alive() && defender->pos() != attacker->pos())
        dest = defender->pos();

    return slide_feature_over(attacker->pos(), dest);
}

#define DID_AFFECT() \
{ \
    if (miscast_level == 0) \
        miscast_level = -1; \
    return; \
}

void melee_attack::chaos_affects_attacker()
{
    if (miscast_level >= 1 || !attacker->alive())
        return;

    // Move stairs out from under the attacker.
    if (one_chance_in(100) && _move_stairs(attacker, defender))
    {
#ifdef NOTE_DEBUG_CHAOS_EFFECTS
        take_note(Note(NOTE_MESSAGE, 0, 0,
                       "CHAOS affects attacker: move stairs"), true);
#endif
        DID_AFFECT();
    }

    // Dump attacker or items under attacker to another level.
    if (is_valid_shaft_level()
        && (attacker->will_trigger_shaft()
            || igrd(attacker->pos()) != NON_ITEM)
        && one_chance_in(1000))
    {
        (void) attacker->do_shaft();
#ifdef NOTE_DEBUG_CHAOS_EFFECTS
        take_note(Note(NOTE_MESSAGE, 0, 0,
                       "CHAOS affects attacker: shaft effect"), true);
#endif
        DID_AFFECT();
    }

    // Create a colourful cloud.
    if (weapon && one_chance_in(1000))
    {
        mprf("Smoke pours forth from %s!", wep_name(DESC_NOCAP_YOUR).c_str());
        big_cloud(random_smoke_type(), KC_OTHER, attacker->pos(), 20,
                  4 + random2(8));
#ifdef NOTE_DEBUG_CHAOS_EFFECTS
        take_note(Note(NOTE_MESSAGE, 0, 0,
                       "CHAOS affects attacker: smoke"), true);
#endif
        DID_AFFECT();
    }

    // Make a loud noise.
    if (weapon && player_can_hear(attacker->pos())
        && one_chance_in(200))
    {
        std::string msg = "";
        if (!you.can_see(attacker))
        {
            std::string noise = getSpeakString("weapon_noise");
            if (!noise.empty())
                msg = "You hear " + noise;
        }
        else
        {
            msg = getSpeakString("weapon_noises");
            std::string wepname = wep_name(DESC_CAP_YOUR);
            if (!msg.empty())
            {
                msg = replace_all(msg, "@Your_weapon@", wepname);
                msg = replace_all(msg, "@The_weapon@", wepname);
            }
        }

        if (!msg.empty())
        {
            mpr(msg.c_str(), MSGCH_SOUND);
            noisy(15, attacker->pos(), attacker->mindex());
#ifdef NOTE_DEBUG_CHAOS_EFFECTS
            take_note(Note(NOTE_MESSAGE, 0, 0,
                           "CHAOS affects attacker: noise"), true);
#endif
            DID_AFFECT();
        }
    }
}

static void _find_remains(monsters* mon, int &corpse_class, int &corpse_index,
                          item_def &fake_corpse, int &last_item,
                          std::vector<int> items)
{
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
    {
        const int idx = mon->inv[i];

        if (idx == NON_ITEM)
            continue;

        item_def &item(mitm[idx]);

        if (!item.is_valid() || item.pos != mon->pos())
            continue;

        items.push_back(idx);
    }

    corpse_index = NON_ITEM;
    last_item    = NON_ITEM;

    corpse_class = fill_out_corpse(mon, fake_corpse, true);
    if (corpse_class == -1 || mons_weight(corpse_class) == 0)
        return;

    // Stop at first non-matching corpse, since the freshest corpse will
    // be at the top of the stack.
    for (stack_iterator si(mon->pos()); si; ++si)
    {
        if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY)
        {
            if (si->orig_monnum != fake_corpse.orig_monnum
                || si->plus != fake_corpse.plus
                || si->plus2 != fake_corpse.plus2
                || si->special != fake_corpse.special
                || si->flags != fake_corpse.flags)
            {
                break;
            }

            // If it's a hydra the number of heads must match.
            if ((short) mon->number != si->props[MONSTER_NUMBER].get_short())
                break;

            // Got it!
            corpse_index = si.link();
            break;
        }
        else
        {
            // Last item which we're sure belonged to the monster.
            for (unsigned int i = 0; i < items.size(); i++)
                if (items[i] == si.link())
                    last_item = si.link();
        }
    }
}

static bool _make_zombie(monsters* mon, int corpse_class, int corpse_index,
                         item_def &fake_corpse, int last_item)
{
    // If the monster dropped a corpse, then don't waste it by turning
    // it into a zombie.
    if (corpse_index != NON_ITEM || !mons_class_can_be_zombified(corpse_class))
        return (false);

    // Good gods won't let their gifts/followers be raised as the
    // undead.
    if (is_good_god(mon->god))
        return (false);

    // First attempt to raise zombie fitted out with all its old
    // equipment.
    int zombie_index = -1;
    int idx = get_item_slot(0);
    if (idx != NON_ITEM && last_item != NON_ITEM)
    {
        mitm[idx]     = fake_corpse;
        mitm[idx].pos = mon->pos();

        // Insert it in the item stack right after the monster's last
        // item, so it will be equipped with all the monster's items.
        mitm[idx].link       = mitm[last_item].link;
        mitm[last_item].link = idx;

        animate_remains(mon->pos(), CORPSE_BODY, mon->behaviour,
                        mon->foe, 0, "a chaos effect", mon->god,
                        true, true, true, &zombie_index);
    }

    // No equipment to get, or couldn't get it for some reason.
    if (zombie_index == -1)
    {
        monster_type type = (mons_zombie_size(mon->type) == Z_SMALL) ?
                                MONS_ZOMBIE_SMALL : MONS_ZOMBIE_LARGE;
        mgen_data mg(type, mon->behaviour, 0, 0, 0, mon->pos(),
                     mon->foe, MG_FORCE_PLACE, mon->god,
                     mon->type, mon->number);
        mg.non_actor_summoner = "a chaos effect";
        zombie_index = create_monster(mg);
    }

    if (zombie_index == -1)
        return (false);

    monsters *zombie = &menv[zombie_index];

    // Attempt to force zombie into exact same spot.
    if (zombie->pos() != mon->pos() && zombie->is_habitable(mon->pos()))
        zombie->move_to_pos(mon->pos());

    if (you.can_see(mon))
    {
        if (you.can_see(zombie))
            simple_monster_message(mon, " instantly turns into a zombie!");
        else if (last_item != NON_ITEM)
        {
            simple_monster_message(mon, "'s equipment vanishes!");
            autotoggle_autopickup(true);
        }
    }
    else
    {
        simple_monster_message(zombie, " appears from thin air!");
        autotoggle_autopickup(false);
    }

    player_angers_monster(zombie);

    return (true);
}

// mon is a copy of the monster from before monster_die() was called,
// though its hitpoints may be non-positive.
//
// NOTE: Isn't called if monster dies from poisoning caused by chaos.
void melee_attack::chaos_killed_defender(monsters* mon)
{
    ASSERT(mon->type != -1 && mon->type != MONS_NO_MONSTER);
    ASSERT(in_bounds(mon->pos()));
    ASSERT(!defender->alive());

    if (!attacker->alive())
        return;

    if (attacker->atype() == ACT_PLAYER && you.banished)
        return;

    if (mon->is_summoned() || (mon->flags & (MF_BANISHED | MF_HARD_RESET)))
        return;

    int              corpse_class, corpse_index, last_item;
    item_def         fake_corpse;
    std::vector<int> items;
    _find_remains(mon, corpse_class, corpse_index, fake_corpse, last_item,
                  items);

    if (one_chance_in(100)
        && _make_zombie(mon, corpse_class, corpse_index, fake_corpse,
                        last_item))
    {
#ifdef NOTE_DEBUG_CHAOS_EFFECTS
        take_note(Note(NOTE_MESSAGE, 0, 0,
                       "CHAOS killed defender: zombified monster"), true);
#endif
        DID_AFFECT();
    }
}

void melee_attack::do_miscast()
{
    if (miscast_level == -1)
        return;

    ASSERT(miscast_target != NULL);
    ASSERT(miscast_level >= 0 && miscast_level <= 3);
    ASSERT(count_bits(miscast_type) == 1);

    if (!miscast_target->alive())
        return;

    if (miscast_target->atype() == ACT_PLAYER && you.banished)
        return;

    const bool chaos_brand =
        weapon && get_weapon_brand(*weapon) == SPWPN_CHAOS;

    // If the miscast is happening on the attacker's side and is due to
    // a chaos weapon then make smoke/sand/etc pour out of the weapon
    // instead of the attacker's hands.
    std::string hand_str;

    std::string cause = atk_name(DESC_NOCAP_THE);
    int         source;

    const int ignore_mask = ISFLAG_KNOW_CURSE | ISFLAG_KNOW_PLUSES;

    if (attacker->atype() == ACT_PLAYER)
    {
        source = NON_MONSTER;
        if (chaos_brand)
        {
            cause = "a chaos effect from ";
            // Ignore a lot of item flags to make cause as short as possible,
            // so it will (hopefully) fit onto a single line in the death
            // cause screen.
            cause += wep_name(DESC_NOCAP_YOUR,
                              ignore_mask
                              | ISFLAG_COSMETIC_MASK | ISFLAG_RACIAL_MASK);

            if (miscast_target == attacker)
                hand_str = wep_name(DESC_PLAIN, ignore_mask);
        }
    }
    else
    {
        source = attacker->mindex();

        if (chaos_brand && miscast_target == attacker
            && you.can_see(attacker))
        {
            hand_str = wep_name(DESC_PLAIN, ignore_mask);
        }
    }

    MiscastEffect(miscast_target, source, (spschool_flag_type) miscast_type,
                  miscast_level, cause, NH_NEVER, 0, hand_str, false);

    // Don't do miscast twice for one attack.
    miscast_level = -1;
}

// NOTE: random_chaos_brand() and random_chaos_attack_flavour() should
// return a set of effects that are roughly the same, to make it easy
// for chaos_affects_defender() not to do duplicate effects caused
// by the non-chaos brands/flavours they return.
int melee_attack::random_chaos_brand()
{
    int brand = SPWPN_NORMAL;
    // Assuming the chaos to be mildly intelligent, try to avoid brands
    // that clash with the most basic resists of the defender,
    // i.e. its holiness.
    while (true)
    {
        brand = (random_choose_weighted(
                     5, SPWPN_VORPAL,
                    10, SPWPN_FLAMING,
                    10, SPWPN_FREEZING,
                    10, SPWPN_ELECTROCUTION,
                    10, SPWPN_VENOM,
                    10, SPWPN_CHAOS,
                     5, SPWPN_DRAINING,
                     5, SPWPN_VAMPIRICISM,
                     5, SPWPN_HOLY_WRATH,
                     2, SPWPN_CONFUSE,
                     2, SPWPN_DISTORTION,
                     0));

        if (one_chance_in(3))
            break;

        bool susceptible = true;
        switch (brand)
        {
        case SPWPN_FLAMING:
            if (defender->is_fiery())
                susceptible = false;
            break;
        case SPWPN_FREEZING:
            if (defender->is_icy())
                susceptible = false;
            break;
        case SPWPN_ELECTROCUTION:
            if (defender->airborne())
                susceptible = false;
            break;
        case SPWPN_VENOM:
            if (defender->holiness() == MH_UNDEAD)
                susceptible = false;
            break;
        case SPWPN_VAMPIRICISM:
            if (defender->is_summoned())
            {
                susceptible = false;
                break;
            }
            // intentional fall-through
        case SPWPN_DRAINING:
            if (defender->holiness() != MH_NATURAL)
                susceptible = false;
            break;
        case SPWPN_HOLY_WRATH:
            if (defender->holiness() != MH_UNDEAD
                && defender->holiness() != MH_DEMONIC)
            {
                susceptible = false;
            }
            break;
        case SPWPN_CONFUSE:
            if (defender->holiness() == MH_NONLIVING
                || defender->holiness() == MH_PLANT)
            {
                susceptible = false;
            }
            break;
        default:
            break;
        }

        if (susceptible)
            break;
    }
#ifdef NOTE_DEBUG_CHAOS_BRAND
    std::string brand_name = "CHAOS brand: ";
    switch (brand)
    {
    case SPWPN_NORMAL:          brand_name += "(plain)"; break;
    case SPWPN_FLAMING:         brand_name += "flaming"; break;
    case SPWPN_FREEZING:        brand_name += "freezing"; break;
    case SPWPN_HOLY_WRATH:      brand_name += "holy wrath"; break;
    case SPWPN_ELECTROCUTION:   brand_name += "electrocution"; break;
    case SPWPN_VENOM:           brand_name += "venom"; break;
    case SPWPN_DRAINING:        brand_name += "draining"; break;
    case SPWPN_DISTORTION:      brand_name += "distortion"; break;
    case SPWPN_VAMPIRICISM:     brand_name += "vampiricism"; break;
    case SPWPN_VORPAL:          brand_name += "vorpal"; break;
    // ranged weapon brands
    case SPWPN_FLAME:           brand_name += "flame"; break;
    case SPWPN_FROST:           brand_name += "frost"; break;

    // both ranged and non-ranged
    case SPWPN_CHAOS:           brand_name += "chaos"; break;
    case SPWPN_CONFUSE:         brand_name += "confusion"; break;
    default:                    brand_name += "(other)"; break;
    }

    // Pretty much duplicated by the chaos effect note,
    // which will be much more informative.
    if (brand != SPWPN_CHAOS)
        take_note(Note(NOTE_MESSAGE, 0, 0, brand_name.c_str()), true);
#endif
    return (brand);
}

mon_attack_flavour melee_attack::random_chaos_attack_flavour()
{
    mon_attack_flavour flavours[] =
        {AF_FIRE, AF_COLD, AF_ELEC, AF_POISON_NASTY, AF_VAMPIRIC, AF_DISTORT,
         AF_CONFUSE, AF_CHAOS};
    return (RANDOM_ELEMENT(flavours));
}

bool melee_attack::apply_damage_brand()
{
    bool brand_was_known = false;

    if (weapon)
    {
        if (is_artefact(*weapon))
            brand_was_known = artefact_known_wpn_property(*weapon, ARTP_BRAND);
        else
            brand_was_known = item_type_known(*weapon);
    }
    bool ret = false;

    // Monster resistance to the brand.
    int res = 0;

    special_damage = 0;
    obvious_effect = false;

    int brand;
    if (damage_brand == SPWPN_CHAOS)
        brand = random_chaos_brand();
    else
        brand = damage_brand;

    switch (brand)
    {
    case SPWPN_FLAMING:
        res = fire_res_apply_cerebov_downgrade(defender->res_fire());
        calc_elemental_brand_damage(BEAM_FIRE, res,
                                    defender->is_icy() ? "melt" : "burn");
        defender->expose_to_element(BEAM_FIRE);
        extra_noise += 1;
        break;

    case SPWPN_FREEZING:
        calc_elemental_brand_damage(BEAM_COLD, defender->res_cold(), "freeze");
        defender->expose_to_element(BEAM_COLD);
        break;

    case SPWPN_HOLY_WRATH:
        if (defender->undead_or_demonic())
            special_damage = 1 + (random2(damage_done * 15) / 10);

        if (special_damage && defender_visible)
        {
            special_damage_message =
                make_stringf(
                    "%s %s%s",
                    def_name(DESC_CAP_THE).c_str(),
                    defender->conj_verb("convulse").c_str(),
                    special_attack_punctuation().c_str());
        }
        break;

    case SPWPN_ELECTROCUTION:
        extra_noise += 2;
        if (defender->airborne() || defender->res_elec() > 0)
            break;
        else if (one_chance_in(3))
        {
            special_damage_message =
                defender->atype() == ACT_PLAYER?
                   "You are electrocuted!"
                :  "There is a sudden explosion of sparks!";
            special_damage = 10 + random2(15);
            special_damage_flavour = BEAM_ELECTRICITY;

            // Check for arcing in water, and add the final effect.
            const coord_def& pos = defender->pos();

            // We know the defender is neither airborne nor electricity
            // resistant, from above, but is it on water?
            if (feat_is_water(grd(pos)))
            {
                attack_final_effect effect;
                effect.flavor = FINEFF_LIGHTNING_DISCHARGE;
                effect.location = pos;
                final_effects.push_back(effect);
            }
        }

        break;

    case SPWPN_ORC_SLAYING:
        if (is_orckind(defender))
        {
            special_damage = 1 + random2(3*damage_done/2);
            if (defender_visible)
            {
                special_damage_message =
                    make_stringf(
                        "%s %s%s",
                        defender->name(DESC_CAP_THE).c_str(),
                        defender->conj_verb("convulse").c_str(),
                        special_attack_punctuation().c_str());
            }
        }
        break;

    case SPWPN_DRAGON_SLAYING:
        if (is_dragonkind(defender))
        {
            special_damage = 1 + random2(3*damage_done/2);
            if (defender_visible)
            {
                special_damage_message =
                    make_stringf(
                        "%s %s%s",
                        defender->name(DESC_CAP_THE).c_str(),
                        defender->conj_verb("convulse").c_str(),
                        special_attack_punctuation().c_str());
            }
        }
        break;

    case SPWPN_VENOM:
        if (!one_chance_in(4))
        {
            int old_poison;

            if (defender->atype() == ACT_PLAYER)
                old_poison = you.duration[DUR_POISONING];
            else
            {
                old_poison =
                    (defender->as_monster()->get_ench(ENCH_POISON)).degree;
            }

            // Poison monster message needs to arrive after hit message.
            emit_nodmg_hit_message();

            // Weapons of venom do two levels of poisoning to the player,
            // but only one level to monsters.
            defender->poison(attacker, 2);

            if (defender->atype() == ACT_PLAYER
                   && old_poison < you.duration[DUR_POISONING]
                || defender->atype() != ACT_PLAYER
                   && old_poison <
                      (defender->as_monster()->get_ench(ENCH_POISON)).degree)
            {
                obvious_effect = true;
            }

        }
        break;

    case SPWPN_DRAINING:
        drain_defender();
        break;

    case SPWPN_VORPAL:
        special_damage = 1 + random2(damage_done) / 4;
        // Note: Leaving special_damage_message empty because there isn't one.
        break;

    case SPWPN_VAMPIRICISM:
    {
        // Vampire bat form -- why the special handling?
        if (attacker->atype() == ACT_PLAYER && you.species == SP_VAMPIRE
            && player_in_bat_form())
        {
            _player_vampire_draws_blood(defender->as_monster(), damage_done);
            break;
        }

        if (x_chance_in_y(defender->res_negative_energy(), 3))
            break;

        if (!weapon || defender->holiness() != MH_NATURAL || damage_done < 1
            || attacker->stat_hp() == attacker->stat_maxhp()
            || defender->atype() != ACT_PLAYER
               && defender->as_monster()->is_summoned()
            || one_chance_in(5))
        {
            break;
        }

        obvious_effect = true;

        // Handle weapon effects.
        // We only get here if we've done base damage, so no
        // worries on that score.

        if (attacker->atype() == ACT_PLAYER)
            mpr("You feel better.");
        else if (attacker_visible)
        {
            if (defender->atype() == ACT_PLAYER)
            {
                mprf("%s draws strength from your injuries!",
                     attacker->name(DESC_CAP_THE).c_str());
            }
            else
            {
                mprf("%s is healed.",
                     attacker->name(DESC_CAP_THE).c_str());
            }
        }

        int hp_boost = 0;

        // Thus is probably more valuable on larger weapons?
        if (weapon && is_unrandom_artefact(*weapon)
            && weapon->special == UNRAND_VAMPIRES_TOOTH)
        {
            hp_boost = damage_done;
        }
        else
            hp_boost = 1 + random2(damage_done);

        attacker->heal(hp_boost);

        if (attacker->hunger_level() != HS_ENGORGED)
            attacker->make_hungry(-random2avg(59, 2));

        attacker->god_conduct(DID_NECROMANCY, 2);
        break;
    }
    case SPWPN_PAIN:
        if (defender->res_negative_energy())
            break;

        if (x_chance_in_y(attacker->skill(SK_NECROMANCY) + 1, 8))
        {
            if (defender_visible)
            {
                special_damage_message =
                    make_stringf("%s %s in agony.",
                                 defender->name(DESC_CAP_THE).c_str(),
                                 defender->conj_verb("writhe").c_str());
            }
            special_damage += random2( 1 + attacker->skill(SK_NECROMANCY) );
        }

        attacker->god_conduct(DID_NECROMANCY, 4);
        break;

    case SPWPN_DISTORTION:
        ret = distortion_affects_defender();
        break;

    case SPWPN_CONFUSE:
    {
        // This was originally for confusing touch and it doesn't really
        // work on the player, but a monster with a chaos weapon will
        // occassionally come up with this brand. -cao
        if (defender->atype() == ACT_PLAYER)
            break;

        emit_nodmg_hit_message();

        const int hdcheck =
            (defender->holiness() == MH_NATURAL ? random2(30) : random2(22));

        if (mons_class_is_confusable(defender->id())
            && hdcheck >= defender->get_experience_level())
        {
            // Declaring these just to pass to the enchant function.
            bolt beam_temp;
            beam_temp.thrower =
                attacker->atype() == ACT_PLAYER? KILL_YOU : KILL_MON;
            beam_temp.flavour = BEAM_CONFUSION;
            beam_temp.beam_source = attacker->mindex();
            beam_temp.apply_enchantment_to_monster(defender->as_monster());
            obvious_effect = beam_temp.obvious_effect;
        }

        if (attacker->atype() == ACT_PLAYER && damage_brand == SPWPN_CONFUSE)
        {
            ASSERT(you.duration[DUR_CONFUSING_TOUCH]);
            you.duration[DUR_CONFUSING_TOUCH] -= roll_dice(3, 5)
                                                 * BASELINE_DELAY;

            if (you.duration[DUR_CONFUSING_TOUCH] < 1)
                you.duration[DUR_CONFUSING_TOUCH] = 1;
            obvious_effect = false;
        }
        break;
    }

    case SPWPN_CHAOS:
        chaos_affects_defender();
        break;
    }

    if (damage_brand == SPWPN_CHAOS && brand != SPWPN_CHAOS && !ret
        && miscast_level == -1 && one_chance_in(20))
    {
        miscast_level  = 0;
        miscast_type   = SPTYP_RANDOM;
        miscast_target = coinflip() ? attacker : defender;
    }

    if (attacker->atype() == ACT_PLAYER && damage_brand == SPWPN_CHAOS)
    {
        // If your god objects to using chaos, then it makes the
        // brand obvious.
        if (did_god_conduct(DID_CHAOS, 2 + random2(3), brand_was_known))
            obvious_effect = true;
    }
    if (!obvious_effect)
        obvious_effect = !special_damage_message.empty();

    if (obvious_effect && attacker_visible && weapon != NULL)
    {
        if (is_artefact(*weapon))
            artefact_wpn_learn_prop(*weapon, ARTP_BRAND);
        else
            set_ident_flags(*weapon, ISFLAG_KNOW_TYPE);
    }

    return (ret);
}


// XXX:
//  * Noise should probably scale non-linearly with damage_done, and
//    maybe even non-linearly with noise_factor.
//
//  * Damage reduction via armour of the defender reduces noise,
//    but shouldn't.
//
//  * Damage reduction because of negative damage modifiers on the
//    weapon reduce noise, but probably shouldn't.
//
//  * Might want a different formula for noise generated by the
//    player.
//
//  Ideas:
//  * Each weapon type has a noise rating, like it does an accuracy
//    rating and base delay.
//
//  * For player, stealth skill and/or weapon skillr reducing noise.
//
//  * Randart property to make randart weapons louder or softer when
//    they hit.
void melee_attack::handle_noise(const coord_def & pos)
{
    // Successful stabs make no noise.
    if (stab_attempt)
    {
        noise_factor = 0;
        extra_noise  = 0;
        return;
    }

    int level = (noise_factor * damage_done / 100 / 4) + extra_noise;

    if (noise_factor > 0)
        level = std::max(1, level);

    if (level > 0)
        noisy(level, pos, attacker->mindex());

    noise_factor = 0;
    extra_noise  = 0;
}

// Returns true if the attack cut off a head *and* cauterized it.
bool melee_attack::chop_hydra_head( int dam,
                                    int dam_type,
                                    int wpn_brand )
{
    // Monster attackers have only a 25% chance of making the
    // chop-check to prevent runaway head inflation.
    if (attacker->atype() == ACT_MONSTER && !one_chance_in(4))
        return (false);

    if ((dam_type == DVORP_SLICING || dam_type == DVORP_CHOPPING
            || dam_type == DVORP_CLAWING)
        && dam > 0
        && (dam >= 4 || wpn_brand == SPWPN_VORPAL || coinflip()))
    {
        const char *verb = NULL;

        if (dam_type == DVORP_CLAWING)
        {
            static const char *claw_verbs[] = { "rip", "tear", "claw" };
            verb = RANDOM_ELEMENT(claw_verbs);
        }
        else
        {
            static const char *slice_verbs[] =
            {
                "slice", "lop", "chop", "hack"
            };
            verb = RANDOM_ELEMENT(slice_verbs);
        }

        if (defender->as_monster()->number == 1) // will be zero afterwards
        {
            if (defender_visible)
            {
                mprf("%s %s %s's last head off!",
                     atk_name(DESC_CAP_THE).c_str(),
                     attacker->conj_verb(verb).c_str(),
                     def_name(DESC_NOCAP_THE).c_str());
            }
            defender->as_monster()->number--;

            if (!defender->is_summoned())
                bleed_onto_floor(defender->pos(), defender->id(),
                                 defender->as_monster()->hit_points, true);

            defender->hurt(attacker, defender->as_monster()->hit_points);

            return (true);
        }
        else
        {
            if (defender_visible)
            {
                mprf("%s %s one of %s's heads off!",
                     atk_name(DESC_CAP_THE).c_str(),
                     attacker->conj_verb(verb).c_str(),
                     def_name(DESC_NOCAP_THE).c_str());
            }
            defender->as_monster()->number--;

            // Only living hydras get to regenerate heads.
            if (defender->holiness() == MH_NATURAL)
            {
                unsigned int limit = 20;
                if (defender->id() == MONS_LERNAEAN_HYDRA)
                    limit = 27;

                if (wpn_brand == SPWPN_FLAMING)
                {
                    if (defender_visible)
                        mpr( "The flame cauterises the wound!" );
                    return (true);
                }
                else if (defender->as_monster()->number < limit - 1)
                {
                    simple_monster_message(defender->as_monster(),
                                           " grows two more!" );
                    defender->as_monster()->number += 2;
                    defender->heal(8 + random2(8), true);
                }
            }
        }
    }

    return (false);
}

bool melee_attack::decapitate_hydra(int dam, int damage_type)
{
    if (defender->atype() == ACT_MONSTER
        && defender->as_monster()->has_hydra_multi_attack()
        && defender->id() != MONS_SPECTRAL_THING)
    {
        const int dam_type = (damage_type != -1) ? damage_type
                                                 : attacker->damage_type();
        const int wpn_brand = attacker->damage_brand();

        return chop_hydra_head(dam, dam_type, wpn_brand);
    }
    return (false);
}

void melee_attack::player_sustain_passive_damage()
{
    if (mons_class_flag(defender->id(), M_ACID_SPLASH))
        weapon_acid(5);
}

int melee_attack::player_staff_damage(int skill)
{
    return (random2(5*(you.skills[skill] + you.skills[SK_EVOCATIONS])/4));
}

void melee_attack::emit_nodmg_hit_message()
{
    if (!no_damage_message.empty())
    {
        mprf("%s", no_damage_message.c_str());
        no_damage_message.clear();
    }
}

void melee_attack::player_apply_staff_damage()
{
    special_damage = 0;

    if (!weapon || !item_is_staff(*weapon))
        return;

    if (random2(15) > you.skills[SK_EVOCATIONS])
    {
        return;
    }

    switch (weapon->sub_type)
    {
    case STAFF_AIR:
        if (damage_done + you.skills[SK_AIR_MAGIC] <= random2(20))
            break;

        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_ELECTRICITY,
                                 defender->res_elec(),
                                 player_staff_damage(SK_AIR_MAGIC));

        if (special_damage)
        {
            special_damage_message =
                make_stringf("%s is jolted!",
                             defender->name(DESC_CAP_THE).c_str());
            special_damage_flavour = BEAM_ELECTRICITY;
        }

        break;

    case STAFF_COLD:
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_COLD,
                                 defender->res_cold(),
                                 player_staff_damage(SK_ICE_MAGIC));

        if (special_damage)
        {
            special_damage_message =
                make_stringf(
                    "You freeze %s!",
                    defender->name(DESC_NOCAP_THE).c_str());
        }
        break;

    case STAFF_EARTH:
        special_damage = player_staff_damage(SK_EARTH_MAGIC);
        special_damage = player_apply_monster_ac(special_damage);

        if (special_damage > 0)
        {
            special_damage_message =
                make_stringf(
                    "You crush %s!",
                    defender->name(DESC_NOCAP_THE).c_str());
        }
        break;

    case STAFF_FIRE:
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_FIRE,
                                 defender->res_fire(),
                                 player_staff_damage(SK_FIRE_MAGIC));

        if (special_damage)
        {
            special_damage_message =
                make_stringf(
                    "You burn %s!",
                    defender->name(DESC_NOCAP_THE).c_str());
        }
        break;

    case STAFF_POISON:
    {
        // Cap chance at 30% -- let staff of Olgreb shine.
        int temp_rand = damage_done + you.skills[SK_POISON_MAGIC];

        if (temp_rand > 30)
            temp_rand = 30;

        if (x_chance_in_y(temp_rand, 100))
        {
            // Poison monster message needs to arrive after hit message.
            emit_nodmg_hit_message();
            poison_monster(defender->as_monster(), KC_YOU);
        }
        break;
    }

    case STAFF_DEATH:
        if (defender->res_negative_energy())
            break;

        if (x_chance_in_y(you.skills[SK_NECROMANCY] + 1, 8))
        {
            special_damage = player_staff_damage(SK_NECROMANCY);

            if (special_damage)
            {
                special_damage_message =
                    make_stringf(
                        "%s convulses in agony!",
                        defender->name(DESC_CAP_THE).c_str());

                did_god_conduct(DID_NECROMANCY, 4);
            }
        }
        break;

    case STAFF_POWER:
    case STAFF_SUMMONING:
    case STAFF_CHANNELING:
    case STAFF_CONJURATION:
    case STAFF_ENCHANTMENT:
    case STAFF_ENERGY:
    case STAFF_WIZARDRY:
        break;

    default:
        mpr("You're wielding some staff I've never heard of! (fight.cc)",
            MSGCH_ERROR);
        break;
    }

    if (special_damage > 0)
    {
        if (!item_type_known(*weapon))
        {
            set_ident_flags( *weapon, ISFLAG_KNOW_TYPE );
            set_ident_type( *weapon, ID_KNOWN_TYPE );

            mprf("You are wielding %s.", weapon->name(DESC_NOCAP_A).c_str());
            more();
            you.wield_change = true;
        }
    }
}

bool melee_attack::player_check_monster_died()
{
    if (!defender->alive())
    {
        // note: doesn't take account of special weapons, etc.
        dprf("Hit for %d.", damage_done);

        player_monattk_hit_effects(true);

        _monster_die(defender->as_monster(), KILL_YOU, NON_MONSTER);

        return (true);
    }

    return (false);
}

void melee_attack::player_calc_hit_damage()
{
    int potential_damage;

    potential_damage =
        !weapon ? player_calc_base_unarmed_damage()
                : player_calc_base_weapon_damage();

    // [0.6 AC/EV overhaul] Shields don't go well with hand-and-half weapons.
    if (weapon && hands == HANDS_HALF)
    {
        potential_damage =
            std::max(1,
                     potential_damage - roll_dice(1, player_shield_penalty));
    }

    potential_damage = player_stat_modify_damage(potential_damage);

    //  apply damage bonus from ring of slaying
    // (before randomization -- some of these rings
    //  are stupidly powerful) -- GDL
    potential_damage += slaying_bonus(PWPN_DAMAGE);
    damage_done =
        potential_damage > 0 ? one_chance_in(3) + random2(potential_damage) : 0;
    damage_done = player_apply_weapon_skill(damage_done);
    damage_done = player_apply_fighting_skill(damage_done, false);
    damage_done = player_apply_misc_modifiers(damage_done);
    damage_done = player_apply_weapon_bonuses(damage_done);

    player_weapon_auto_id();

    damage_done = player_stab(damage_done);
    damage_done = player_apply_monster_ac(damage_done);

    // This doesn't actually modify damage. -- bwr
    // It only chooses the appropriate verb.
    damage_done = std::max(0, player_weapon_type_modify(damage_done));
}

int melee_attack::calc_to_hit(bool random)
{
    return (attacker->atype() == ACT_PLAYER ? player_to_hit(random)
                                            : mons_to_hit());
}

// Calculates your armour+shield penalty. If random_factor is true,
// be stochastic; if false, deterministic (e.g. for chardumps.)
void melee_attack::calc_player_armour_shield_tohit_penalty(bool random_factor)
{
    if (weapon && hands == HANDS_HALF)
    {
        player_armour_tohit_penalty =
            maybe_roll_dice(1, player_body_armour_penalty, random_factor);
        player_shield_tohit_penalty =
            maybe_roll_dice(2, player_shield_penalty, random_factor);
    }
    else
    {
        player_armour_tohit_penalty =
            maybe_roll_dice(1, player_body_armour_penalty, random_factor);
        player_shield_tohit_penalty =
            maybe_roll_dice(1, player_shield_penalty, random_factor);
    }
}

int melee_attack::player_to_hit(bool random_factor)
{
    calc_player_armour_shield_tohit_penalty(random_factor);

    dprf("Armour/shield to-hit penalty: %d/%d",
         player_armour_tohit_penalty, player_shield_tohit_penalty);

    can_do_unarmed =
        player_fights_well_unarmed(player_armour_tohit_penalty
                                   + player_shield_tohit_penalty);

    int your_to_hit = 15 + (calc_stat_to_hit_base() / 2);

#ifdef DEBUG_DIAGNOSTICS
    const int base_to_hit = your_to_hit;
#endif

    if (wearing_amulet(AMU_INACCURACY))
        your_to_hit -= 5;

    // If you can't see yourself, you're a little less accurate.
    if (!you.visible_to(&you))
        your_to_hit -= 5;

    // fighting contribution
    your_to_hit += maybe_random2(1 + you.skills[SK_FIGHTING], random_factor);

    // weapon skill contribution
    if (weapon)
    {
        if (wpn_skill != SK_FIGHTING)
        {
            if (you.skills[wpn_skill] < 1 && player_in_a_dangerous_place())
                xom_is_stimulated(14); // Xom thinks that is mildly amusing.

            your_to_hit += maybe_random2(you.skills[wpn_skill] + 1,
                                         random_factor);
        }
    }
    else
    {                       // ...you must be unarmed
        your_to_hit += you.demon_pow[MUT_CLAWS] ? 4 : 2;

        your_to_hit += maybe_random2(1 + you.skills[SK_UNARMED_COMBAT],
                                     random_factor);
    }

    // weapon bonus contribution
    if (weapon)
    {
        if (weapon->base_type == OBJ_WEAPONS)
        {
            your_to_hit += weapon->plus;
            your_to_hit += property( *weapon, PWPN_HIT );

            if (get_equip_race(*weapon) == ISFLAG_ELVEN
                && player_genus(GENPC_ELVEN))
            {
                your_to_hit += (random_factor && coinflip() ? 2 : 1);
            }
            else if (get_equip_race(*weapon) == ISFLAG_ORCISH
                     && you.religion == GOD_BEOGH && !player_under_penance())
            {
                your_to_hit++;
            }

        }
        else if (weapon->base_type == OBJ_STAVES)
        {
            // magical staff
            your_to_hit += property( *weapon, PWPN_HIT );

            if (item_is_rod( *weapon ))
                your_to_hit += (short)weapon->props["rod_enchantment"];
        }
    }

    // slaying bonus
    your_to_hit += slaying_bonus(PWPN_HIT);

    // hunger penalty
    if (you.hunger_state == HS_STARVING)
        your_to_hit -= 3;

    // armour penalty
    your_to_hit -= (player_armour_tohit_penalty + player_shield_tohit_penalty);

    //mutation
    if (player_mutation_level(MUT_EYEBALLS))
        your_to_hit += 2 * player_mutation_level(MUT_EYEBALLS) + 1;

#ifdef DEBUG_DIAGNOSTICS
    int roll_hit = your_to_hit;
#endif

    // hit roll
    your_to_hit = maybe_random2(your_to_hit, random_factor);

#ifdef DEBUG_DIAGNOSTICS
    dprf( "to hit die: %d; rolled value: %d; base: %d",
          roll_hit, your_to_hit, base_to_hit );
#endif

    if (weapon && wpn_skill == SK_SHORT_BLADES && you.duration[DUR_SURE_BLADE])
    {
        int turn_duration = you.duration[DUR_SURE_BLADE] / BASELINE_DELAY;
        your_to_hit += 5 +
            (random_factor ? random2limit( turn_duration, 10 ) :
             turn_duration / 2);
    }

    // other stuff
    if (!weapon)
    {
        if (you.duration[DUR_CONFUSING_TOUCH])
        {
            // Just trying to touch is easier that trying to damage.
            your_to_hit += maybe_random2(you.dex(), random_factor);
        }

        switch (you.attribute[ATTR_TRANSFORMATION])
        {
        case TRAN_SPIDER:
            your_to_hit += maybe_random2(10, random_factor);
            break;
        case TRAN_BAT:
            your_to_hit += maybe_random2(12, random_factor);
            break;
        case TRAN_ICE_BEAST:
            your_to_hit += maybe_random2(10, random_factor);
            break;
        case TRAN_BLADE_HANDS:
            your_to_hit += maybe_random2(12, random_factor);
            break;
        case TRAN_STATUE:
            your_to_hit += maybe_random2(9, random_factor);
            break;
        case TRAN_DRAGON:
            your_to_hit += maybe_random2(10, random_factor);
            break;
        case TRAN_LICH:
            your_to_hit += maybe_random2(10, random_factor);
            break;
        case TRAN_NONE:
        default:
            break;
        }
    }

    // Check for backlight (Corona).
    if (defender && defender->atype() == ACT_MONSTER)
    {
        if (defender->backlit(true, false))
            your_to_hit += 2 + random2(8);
        // Invisible monsters are hard to hit.
        else if (!defender->visible_to(&you))
            your_to_hit -= 6;
    }

    return (your_to_hit);
}

void melee_attack::player_stab_check()
{
    // Unknown mimics cannot be stabbed.
    // Confusion and having dex of 0 disallow stabs.
    if (mons_is_unknown_mimic(defender->as_monster()) || you.stat_zero[STAT_DEX]
        || you.confused())
    {
        stab_attempt = false;
        stab_bonus = 0;
        return;
    }

    const unchivalric_attack_type uat = is_unchivalric_attack(&you, defender);
    stab_attempt = (uat != UCAT_NO_ATTACK);
    const bool roll_needed = (uat != UCAT_SLEEPING && uat != UCAT_PARALYSED);

    int roll = 100;
    if (uat == UCAT_INVISIBLE && !mons_sense_invis(defender->as_monster()))
        roll -= 10;

    switch (uat)
    {
    case UCAT_NO_ATTACK:
        stab_bonus = 0;
        break;
    case UCAT_SLEEPING:
    case UCAT_PARALYSED:
        stab_bonus = 1;
        break;
    case UCAT_HELD_IN_NET:
    case UCAT_PETRIFYING:
    case UCAT_PETRIFIED:
        stab_bonus = 2;
        break;
    case UCAT_INVISIBLE:
    case UCAT_CONFUSED:
    case UCAT_FLEEING:
    case UCAT_ALLY:
        stab_bonus = 4;
        break;
    case UCAT_DISTRACTED:
        stab_bonus = 6;
        break;
    }

    // See if we need to roll against dexterity / stabbing.
    if (stab_attempt && roll_needed)
    {
        stab_attempt = x_chance_in_y(you.skills[SK_STABBING] + you.dex() + 1,
                                     roll);
    }
}

random_var melee_attack::player_calc_attack_delay()
{
    random_var attack_delay = weapon ? player_weapon_speed()
                                     : player_unarmed_speed();

    if (weapon && hands == HANDS_HALF)
    {
        attack_delay += rv::min(rv::roll_dice(1, player_body_armour_penalty)
                                + rv::roll_dice(1, player_shield_penalty),
                                rv::roll_dice(1, player_body_armour_penalty)
                                + rv::roll_dice(1, player_shield_penalty));
    }
    else if (player_body_armour_penalty)
        attack_delay += rv::min(rv::roll_dice(1, player_body_armour_penalty),
                                rv::roll_dice(1, player_body_armour_penalty));

    attack_delay = rv::max(attack_delay, constant(3));

    return (attack_delay);
}

void melee_attack::player_apply_attack_delay()
{
    final_attack_delay = player_calc_attack_delay().roll();

    you.time_taken =
        std::max(2, div_rand_round(you.time_taken * final_attack_delay, 10));

    dprf("Weapon speed: %d; min: %d; attack time: %d",
         final_attack_delay, min_delay, you.time_taken);
}

random_var melee_attack::player_weapon_speed()
{
    random_var attack_delay = constant(0);

    if (weapon && (weapon->base_type == OBJ_WEAPONS
                   || weapon->base_type == OBJ_STAVES))
    {
        attack_delay = constant(property(*weapon, PWPN_SPEED));

        if (player_body_armour_penalty)
        {
            attack_delay = rv::max(attack_delay,
               rv::roll_dice(1, 10)
               + rv::roll_dice(1, player_body_armour_penalty));
        }

        attack_delay -= constant(you.skills[wpn_skill] / 2);

        min_delay = property( *weapon, PWPN_SPEED ) / 2;

        // Short blades can get up to at least unarmed speed.
        if (wpn_skill == SK_SHORT_BLADES && min_delay > 5)
            min_delay = 5;

        // Using both hands can get a weapon up to speed 7
        if ((hands == HANDS_TWO || hand_half_bonus)
            && min_delay > 7)
        {
            min_delay = 7;
        }

        // never go faster than speed 3 (ie 3 attacks per round)
        if (min_delay < 3)
            min_delay = 3;

        // apply minimum to weapon skill modification
        attack_delay = rv::max(attack_delay, min_delay);

        if (weapon->base_type == OBJ_WEAPONS
            && damage_brand == SPWPN_SPEED)
        {
            attack_delay = (attack_delay + constant(1)) / 2;
        }
    }

    return (attack_delay);
}

random_var melee_attack::player_unarmed_speed()
{
    random_var unarmed_delay =
        rv::max(constant(10),
                 (rv::roll_dice(1, 10) +
                  rv::roll_dice(2, player_body_armour_penalty)));

    // Not even bats can attack faster than this.
    min_delay = 5;

    // Unarmed speed.
    if (you.burden_state == BS_UNENCUMBERED)
    {
        unarmed_delay =
            rv::max(unarmed_delay
                     - constant(you.skills[SK_UNARMED_COMBAT]
                                / (player_in_bat_form() ? 3 : 5)),
                    constant(min_delay));
    }

    return (unarmed_delay);
}

int melee_attack::player_calc_base_unarmed_damage()
{
    int damage = 3;

    if (you.duration[DUR_CONFUSING_TOUCH])
    {
        // No base hand damage while using this spell.
        damage = 0;
    }

    if (you.attribute[ATTR_TRANSFORMATION] != TRAN_NONE)
    {
        switch (you.attribute[ATTR_TRANSFORMATION])
        {
        case TRAN_SPIDER:
            damage = 5;
            break;
        case TRAN_BAT:
            damage = (you.species == SP_VAMPIRE ? 2 : 1);
            break;
        case TRAN_ICE_BEAST:
            damage = 12;
            break;
        case TRAN_BLADE_HANDS:
            damage = 12 + (you.strength() / 4) + (you.dex() / 4);
            break;
        case TRAN_STATUE:
            damage = 12 + you.strength();
            break;
        case TRAN_DRAGON:
            damage = 20 + you.strength();
            break;
        case TRAN_LICH:
            damage = 5;
            break;
        }
    }
    else if (you.has_usable_claws(false))
    {
        // Claw damage only applies for bare hands.
        damage += player_mutation_level(MUT_CLAWS) * 2;
    }

    if (player_in_bat_form())
    {
        // Bats really don't do a lot of damage.
        damage += you.skills[SK_UNARMED_COMBAT] / 5;
    }
    else
        damage += you.skills[SK_UNARMED_COMBAT];

    return (damage);
}

int melee_attack::player_calc_base_weapon_damage()
{
    int damage = 0;

    if (weapon->base_type == OBJ_WEAPONS || weapon->base_type == OBJ_STAVES)
        damage = property( *weapon, PWPN_DAMAGE );

    // Staves can be wielded with a worn shield, but are much less
    // effective.
    if (shield && weapon->base_type == OBJ_WEAPONS
        && weapon_skill(*weapon) == SK_STAVES
        && hands_reqd(*weapon, you.body_size()) == HANDS_HALF)
    {
        damage /= 2;
    }

    return (damage);
}

///////////////////////////////////////////////////////////////////////////

bool melee_attack::mons_attack_mons()
{
    const coord_def atk_pos = attacker->pos();
    const coord_def def_pos = defender->pos();

    // Self-attacks never violate sanctuary.
    if ((is_sanctuary(atk_pos) || is_sanctuary(def_pos))
        && attacker != defender)
    {
        // Friendly monsters should only violate sanctuary if explicitly
        // ordered to do so by the player.
        if (attacker->as_monster()->friendly())
        {
            if (you.pet_target == MHITYOU || you.pet_target == MHITNOT)
            {
                if (attacker->confused() && you.can_see(attacker))
                {
                    mpr("Zin prevents your ally from violating sanctuary "
                        "in its confusion.", MSGCH_GOD);
                }
                else if (attacker->berserk() && you.can_see(attacker))
                {
                    mpr("Zin prevents your ally from violating sanctuary "
                        "in its berserker rage.", MSGCH_GOD);
                }

                cancel_attack = true;
                return (false);
            }
        }
        // Non-friendly monsters should never violate sanctuary.
        else
        {
            dprf("Preventing hostile violation of sanctuary.");
            cancel_attack = true;
            return (false);
        }
    }

    mons_perform_attack();

    // If a hydra was attacking it may have switched targets and started
    // hitting the player. -cao
    if (defender->atype() == ACT_PLAYER)
        return (did_hit);

    if (perceived_attack
        && (defender->as_monster()->foe == MHITNOT || one_chance_in(3))
        && attacker->alive() && defender->alive())
    {
        behaviour_event(defender->as_monster(), ME_WHACK, attacker->mindex());
    }

    // If an enemy attacked a friend, set the pet target if it isn't set
    // already, but not if sanctuary is in effect (pet target must be
    // set explicitly by the player during sanctuary).
    if (perceived_attack && attacker->alive()
        && defender->as_monster()->friendly()
        && !crawl_state.game_is_arena()
        && !attacker->as_monster()->wont_attack()
        && you.pet_target == MHITNOT
        && env.sanctuary_time <= 0)
    {
        you.pet_target = attacker->mindex();
    }

    return (did_hit);
}

bool melee_attack::mons_self_destructs()
{
    if (attacker->id() == MONS_GIANT_SPORE
        || attacker->id() == MONS_BALL_LIGHTNING
        || attacker->id() == MONS_ORB_OF_DESTRUCTION)
    {
        attacker->as_monster()->hit_points = -1;
        // Do the explosion right now.
        monster_die(attacker->as_monster(), KILL_MON, attacker->mindex());
        return (true);
    }
    return (false);
}

bool melee_attack::mons_attack_warded_off()
{
    // [dshaligram] Note: warding is no longer a simple 50% chance.
    const int warding = defender->warding();
    if (warding
        && attacker->is_summoned()
        && !attacker->as_monster()->check_res_magic(warding))
    {
        if (needs_message)
        {
            mprf("%s tries to attack %s, but flinches away.",
                 atk_name(DESC_CAP_THE).c_str(),
                 mons_defender_name().c_str());
        }
        return (true);
    }

    return (false);
}

int melee_attack::mons_attk_delay()
{
    return (weapon ? property(*weapon, PWPN_SPEED) : 0);
}

bool melee_attack::attack_shield_blocked(bool verbose)
{
    if (!defender_shield && defender->atype() != ACT_PLAYER)
        return (false);

    if (defender->incapacitated())
        return (false);

    const int con_block = random2(attacker->shield_bypass_ability(to_hit)
                                  + defender->shield_block_penalty());
    int pro_block = defender->shield_bonus();

    if (!attacker->visible_to(defender))
        pro_block /= 3;

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Defender: %s, Pro-block: %d, Con-block: %d",
         def_name(DESC_PLAIN).c_str(), pro_block, con_block);
#endif

    if (pro_block >= con_block)
    {
        perceived_attack = true;

        if (needs_message && verbose)
        {
            mprf("%s %s %s attack.",
                 def_name(DESC_CAP_THE).c_str(),
                 defender->conj_verb("block").c_str(),
                 atk_name(DESC_NOCAP_ITS).c_str());
        }

        defender->shield_block_succeeded(attacker);

        return (true);
    }

    return (false);
}

int melee_attack::mons_calc_damage(const mon_attack_def &attk)
{
    int damage = 0;
    int damage_max = 0;
    if (weapon
        && weapon->base_type == OBJ_WEAPONS
        && !is_range_weapon(*weapon))
    {
        damage_max = property( *weapon, PWPN_DAMAGE );
        damage += random2( damage_max );

        if (get_equip_race(*weapon) == ISFLAG_ORCISH
            && attacker->mons_species() == MONS_ORC
            && coinflip())
        {
            damage++;
        }

        if (weapon->plus2 >= 0)
            damage += random2( weapon->plus2 );
        else
            damage -= random2( 1 - weapon->plus2 );

        damage -= 1 + random2(3);
    }

    damage_max += attk.damage;
    damage     += 1 + random2(attk.damage);

    // Berserk/mighted/frenzied monsters get bonus damage.
    if (attacker->as_monster()->has_ench(ENCH_MIGHT))
        damage = damage * 3 / 2;
    else if (attacker->as_monster()->has_ench(ENCH_BATTLE_FRENZY))
    {
        const mon_enchant ench =
            attacker->as_monster()->get_ench(ENCH_BATTLE_FRENZY);

#ifdef DEBUG_DIAGNOSTICS
        const int orig_damage = damage;
#endif

        damage = damage * (115 + ench.degree * 15) / 100;

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "%s frenzy damage: %d->%d",
             attacker->name(DESC_PLAIN).c_str(), orig_damage, damage);
#endif
    }

    // If the defender is asleep, the attacker gets a stab.
    if (defender && defender->asleep())
    {
        damage = damage * 5 / 2;
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Stab damage vs %s: %d",
             defender->name(DESC_PLAIN).c_str(),
             damage);
#endif
    }

    return (mons_apply_defender_ac(damage, damage_max));
}

int melee_attack::mons_apply_defender_ac(int damage, int damage_max)
{
    const int ac = defender->armour_class();
    if (ac > 0)
    {
        int damage_reduction = random2(ac + 1);
        int guaranteed_damage_reduction = 0;

        if (defender->atype() == ACT_PLAYER)
        {
            const int gdr_perc = defender->as_player()->gdr_perc();
            guaranteed_damage_reduction =
                std::min(damage_max * gdr_perc / 100, ac / 2);
            damage_reduction =
                std::max(guaranteed_damage_reduction, damage_reduction);
        }

        dprf("AC: at: %s, df: %s, dam: %d (max %d), DR: %d (GDR %d), "
             "rdam: %d",
             attacker->name(DESC_PLAIN, true).c_str(),
             defender->name(DESC_PLAIN, true).c_str(),
             damage, damage_max, damage_reduction, guaranteed_damage_reduction,
             damage - damage_reduction);

        damage -= damage_reduction;
    }
    return std::max(0, damage);
}

static const char *klown_attack[] =
{
    "hit",
    "poke",
    "prod",
    "flog",
    "pound",
    "slap",
    "tickle",
    "defenestrate",
    "sucker-punch",
    "elbow",
    "pinch",
    "strangle-hug",
    "squeeze",
    "tease",
    "eye-gouge",
    "karate-kick",
    "headlock",
    "wrestle",
    "trip-wire",
    "kneecap"
};

std::string melee_attack::mons_attack_verb(const mon_attack_def &attk)
{
    if (attacker->id() == MONS_KILLER_KLOWN && attk.type == AT_HIT)
        return (RANDOM_ELEMENT(klown_attack));

    if (attacker->id() == MONS_KRAKEN_TENTACLE && attk.type == AT_TENTACLE_SLAP)
        return ("slap");

    static const char *attack_types[] =
    {
        "",
        "hit",         // including weapon attacks
        "bite",
        "sting",

        // spore
        "hit",

        "touch",
        "engulf",
        "claw",
        "peck",
        "headbutt",
        "punch",
        "kick",
        "tentacle-slap",
        "tail-slap",
        "gore",
        "constrict"
    };

    ASSERT(attk.type <
           static_cast<int>(sizeof(attack_types) / sizeof(const char *)));
    return (attack_types[attk.type]);
}

std::string melee_attack::mons_attack_desc(const mon_attack_def &attk)
{
    if (!you.can_see(attacker))
        return ("");

    if (weapon)
    {
        std::string result = "";
        const item_def wpn = *weapon;
        if (get_weapon_brand(wpn) == SPWPN_REACHING)
        {
            if (grid_distance(attacker->pos(), defender->pos()) == 2)
                result += " from afar";
        }

        if (attacker->id() != MONS_DANCING_WEAPON)
        {
            result += " with ";
            result += weapon->name(DESC_NOCAP_A);
        }

        return (result);
    }
    else if (attk.flavour == AF_REACH
             && grid_distance(attacker->pos(), defender->pos()) == 2)
    {
        return " from afar";
    }

    return ("");
}

std::string melee_attack::mons_defender_name()
{
    if (attacker == defender)
        return pronoun(attacker, PRONOUN_REFLEXIVE, attacker_visible);
    else
        return def_name(DESC_NOCAP_THE);
}

void melee_attack::mons_announce_hit(const mon_attack_def &attk)
{
    if (needs_message)
    {
        mprf("%s %s %s%s%s%s",
             atk_name(DESC_CAP_THE).c_str(),
             attacker->conj_verb( mons_attack_verb(attk) ).c_str(),
             mons_defender_name().c_str(),
             debug_damage_number().c_str(),
             mons_attack_desc(attk).c_str(),
             attack_strength_punctuation().c_str());
    }
}

void melee_attack::mons_announce_dud_hit(const mon_attack_def &attk)
{
    if (needs_message)
    {
        mprf("%s %s %s but does no damage.",
             atk_name(DESC_CAP_THE).c_str(),
             attacker->conj_verb( mons_attack_verb(attk) ).c_str(),
             mons_defender_name().c_str());
    }
}

void melee_attack::check_defender_train_dodging()
{
    // It's possible to train both dodging and armour under the new scheme.
    if (attacker_visible && one_chance_in(3) && defender->check_train_dodging())
        perceived_attack = true;
}

void melee_attack::check_defender_train_armour()
{
    if (coinflip())
        defender->check_train_armour(coinflip()? 2 : 1);
}

void melee_attack::mons_set_weapon(const mon_attack_def &attk)
{
    weapon = (attk.type == AT_HIT) ? attacker->weapon(attack_number) : NULL;
    damage_brand = attacker->damage_brand(attack_number);
}

void melee_attack::mons_do_poison(const mon_attack_def &attk)
{
    if (defender->res_poison() > 0)
        return;

    if (attk.flavour == AF_POISON_NASTY
        || one_chance_in(15 + 5 * (attk.flavour == AF_POISON ? 1 : 0))
        || (damage_done > 1
            && one_chance_in(attk.flavour == AF_POISON ? 4 : 3)))
    {
        if (needs_message)
        {
            if (defender->atype() == ACT_PLAYER
                && (attk.type == AT_BITE || attk.type == AT_STING))
            {
                if (attacker_visible)
                {
                    mprf("%s %s was poisonous!",
                         apostrophise(attacker->name(DESC_CAP_THE)).c_str(),
                         mons_attack_verb(attk).c_str());
                }
            }
            else
            {
                mprf("%s poisons %s!",
                     atk_name(DESC_CAP_THE).c_str(),
                     mons_defender_name().c_str());
            }
        }

        int amount = 1;
        if (attk.flavour == AF_POISON_NASTY)
            amount++;
        else if (attk.flavour == AF_POISON_MEDIUM)
            amount += random2(3);
        else if (attk.flavour == AF_POISON_STRONG)
            amount += roll_dice(2, 5);

        defender->poison(attacker, amount);
    }
}

void melee_attack::mons_do_napalm()
{
    if (defender->res_sticky_flame())
        return;

    if (one_chance_in(20) || (damage_done > 2 && one_chance_in(3)))
    {
        if (needs_message)
        {
            mprf("%s %s covered in liquid flames%s",
                 def_name(DESC_CAP_THE).c_str(),
                 defender->conj_verb("are").c_str(),
                 special_attack_punctuation().c_str());
        }

        if (defender->atype() == ACT_PLAYER)
            napalm_player(random2avg(7, 3) + 1);
        else
        {
            napalm_monster(defender->as_monster(),
                           attacker->as_monster()->friendly() ?
                           KC_FRIENDLY : KC_OTHER,
                           std::min(4, 1 + random2(attacker->get_experience_level())/2));
        }
    }
}

void melee_attack::wasp_paralyse_defender()
{
    // [dshaligram] Adopted 4.1.2's wasp mechanics, in slightly modified
    // form.
    if (attacker->id() == MONS_RED_WASP || one_chance_in(3))
        defender->poison(attacker, coinflip() ? 2 : 1);

    int paralyse_roll = (damage_done > 4 ? 3 : 20);
    if (attacker->id() == MONS_YELLOW_WASP)
        paralyse_roll += 3;

    if (defender->res_poison() <= 0)
    {
        if (one_chance_in(paralyse_roll))
            defender->paralyse(attacker, roll_dice(1, 3));
        else
            defender->slow_down(attacker, roll_dice(1, 3));
    }
}

void melee_attack::splash_monster_with_acid(int strength)
{
    special_damage += roll_dice(2, 4);
    if (defender_visible)
        mprf("%s is splashed with acid.", defender->name(DESC_CAP_THE).c_str());
    corrode_monster(defender->as_monster());
}

void melee_attack::splash_defender_with_acid(int strength)
{
    if (defender->atype() == ACT_PLAYER)
    {
        mpr("You are splashed with acid!");
        splash_with_acid(strength);
    }
    else
        splash_monster_with_acid(strength);
}

static void _steal_item_from_player(monsters *mon)
{
    if (mon->confused())
    {
        std::string msg = getSpeakString("Maurice confused nonstealing");
        if (!msg.empty() && msg != "__NONE")
        {
            msg = replace_all(msg, "@The_monster@", mon->name(DESC_CAP_THE));
            mpr(msg.c_str(), MSGCH_TALK);
        }
        return;
    }

    mon_inv_type mslot = NUM_MONSTER_SLOTS;
    int steal_what  = -1;
    int total_value = 0;
    for (int m = 0; m < ENDOFPACK; ++m)
    {
        if (!you.inv[m].is_valid())
            continue;

        // Cannot unequip player.
        // TODO: Allow stealing of the wielded weapon?
        //       Needs to be unwielded properly and should never lead to
        //       fatal stat loss.
        // 1KB: I'd say no, weapon is being held, it's different from pulling
        //      a wand from your pocket.
        if (item_is_equipped(you.inv[m]))
            continue;

        mon_inv_type monslot = item_to_mslot(you.inv[m]);
        if (monslot == NUM_MONSTER_SLOTS)
        {
            // Try a related slot instead to allow for stealing of other
            // valuable items.
            if (you.inv[m].base_type == OBJ_BOOKS)
                monslot = MSLOT_SCROLL;
            else if (you.inv[m].base_type == OBJ_JEWELLERY)
                monslot = MSLOT_MISCELLANY;
            else
                continue;
        }

        // Only try to steal stuff we can still store somewhere.
        if (mon->inv[monslot] != NON_ITEM)
        {
            if (monslot == MSLOT_WEAPON
                && mon->inv[MSLOT_ALT_WEAPON] == NON_ITEM)
            {
                monslot = MSLOT_ALT_WEAPON;
            }
            else
                continue;
        }

        // Candidate for stealing.
        const int value = item_value(you.inv[m], true);
        total_value += value;

        if (x_chance_in_y(value, total_value))
        {
            steal_what = m;
            mslot      = monslot;
        }
    }

    if (steal_what == -1 || you.gold > 0 && one_chance_in(10))
    {
        // Found no item worth stealing, try gold.
        if (you.gold == 0)
        {
            if (silenced(mon->pos()))
                return;

            std::string complaint = getSpeakString("Maurice nonstealing");
            if (!complaint.empty())
            {
                complaint = replace_all(complaint, "@The_monster@",
                                        mon->name(DESC_CAP_THE));
                mpr(complaint.c_str(), MSGCH_TALK);
            }

            bolt beem;
            beem.source      = mon->pos();
            beem.target      = mon->pos();
            beem.beam_source = mon->mindex();

            // Try to teleport away.
            if (mon->has_ench(ENCH_TP))
            {
                mons_cast_noise(mon, beem, SPELL_BLINK);
                monster_blink(mon);
            }
            else
                mons_cast(mon, beem, SPELL_TELEPORT_SELF);

            return;
        }

        const int stolen_amount = std::min(20 + random2(800), you.gold);
        if (mon->inv[MSLOT_GOLD] != NON_ITEM)
        {
            // If Maurice already's got some gold, simply increase the amount.
            mitm[mon->inv[MSLOT_GOLD]].quantity += stolen_amount;
        }
        else
        {
            // Else create a new item for this pile of gold.
            const int idx = items(0, OBJ_GOLD, OBJ_RANDOM, true, 0, 0);
            if (idx == NON_ITEM)
                return;

            item_def &new_item = mitm[idx];
            new_item.base_type = OBJ_GOLD;
            new_item.sub_type  = 0;
            new_item.plus      = 0;
            new_item.plus2     = 0;
            new_item.special   = 0;
            new_item.flags     = 0;
            new_item.link      = NON_ITEM;
            new_item.quantity  = stolen_amount;
            new_item.pos.reset();
            item_colour(new_item);

            unlink_item(idx);

            mon->inv[MSLOT_GOLD] = idx;
            new_item.set_holding_monster(mon->mindex());
        }
        mprf("%s steals %s your gold!",
             mon->name(DESC_CAP_THE).c_str(),
             stolen_amount == you.gold ? "all" : "some of");

        you.attribute[ATTR_GOLD_FOUND] -= stolen_amount;

        you.del_gold(stolen_amount);
        return;
    }

    ASSERT(steal_what != -1);
    ASSERT(mslot != NUM_MONSTER_SLOTS);
    ASSERT(mon->inv[mslot] == NON_ITEM);

    // Create new item.
    int index = get_item_slot(10);
    if (index == NON_ITEM)
        return;

    item_def &new_item = mitm[index];

    // Copy item.
    new_item = you.inv[steal_what];

    // Set quantity, and set the item as unlinked.
    new_item.quantity -= random2(new_item.quantity);
    new_item.pos.reset();
    new_item.link = NON_ITEM;

    mprf("%s steals %s!",
         mon->name(DESC_CAP_THE).c_str(),
         new_item.name(DESC_NOCAP_YOUR).c_str());

    unlink_item(index);
    mon->inv[mslot] = index;
    new_item.set_holding_monster(mon->mindex());
    // You'll want to autopickup it after killing Maurice.
    new_item.flags |= ISFLAG_THROWN;
    mon->equip(new_item, mslot, true);

    // Item is gone from player's inventory.
    dec_inv_item_quantity(steal_what, new_item.quantity);
}

void melee_attack::mons_apply_attack_flavour(const mon_attack_def &attk)
{
    // Most of this is from BWR 4.1.2.

    mon_attack_flavour flavour = attk.flavour;
    if (flavour == AF_CHAOS)
        flavour = random_chaos_attack_flavour();

    switch (flavour)
    {
    default:
        break;

    case AF_MUTATE:
        if (one_chance_in(4))
            defender->mutate();
        break;

    case AF_POISON:
    case AF_POISON_NASTY:
    case AF_POISON_MEDIUM:
    case AF_POISON_STRONG:
        mons_do_poison(attk);
        break;

    case AF_POISON_STR:
    case AF_POISON_INT:
    case AF_POISON_DEX:
        if (defender->res_poison() <= 0)
        {
            defender->poison(attacker, roll_dice(1, 3));
            if (one_chance_in(4))
            {
                stat_type drained_stat = (flavour == AF_POISON_STR ? STAT_STR :
                                          flavour == AF_POISON_INT ? STAT_INT
                                                                   : STAT_DEX);
                defender->drain_stat(drained_stat, 1, attacker);
            }
        }
        break;

    case AF_ROT:
        if (one_chance_in(20) || (damage_done > 2 && one_chance_in(3)))
            rot_defender(2 + random2(3), damage_done > 5 ? 1 : 0);
        break;

    case AF_DISEASE:
        defender->sicken(50 + random2(100));
        break;

    case AF_FIRE:
        if (attacker->id() == MONS_FIRE_VORTEX)
            attacker->as_monster()->hit_points = -10;

        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_FIRE,
                                 defender->res_fire(),
                                 attacker->get_experience_level()
                                 + random2(attacker->get_experience_level()));

        if (needs_message && special_damage)
        {
            mprf("%s %s engulfed in flames%s",
                 def_name(DESC_CAP_THE).c_str(),
                 defender->conj_verb("are").c_str(),
                 special_attack_punctuation().c_str());
        }

        defender->expose_to_element(BEAM_FIRE, 2);
        break;

    case AF_COLD:
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_COLD,
                                 defender->res_cold(),
                                 attacker->get_experience_level() +
                                 random2(2 * attacker->get_experience_level()));

        if (needs_message && special_damage)
        {
            mprf("%s %s %s%s",
                 atk_name(DESC_CAP_THE).c_str(),
                 attacker->conj_verb("freeze").c_str(),
                 mons_defender_name().c_str(),
                 special_attack_punctuation().c_str());

        }

        defender->expose_to_element(BEAM_COLD, 2);
        break;

    case AF_ELEC:
        special_damage =
            resist_adjust_damage(
                defender,
                BEAM_ELECTRICITY,
                defender->res_elec(),
                attacker->get_experience_level() +
                random2(attacker->get_experience_level() / 2));
        special_damage_flavour = BEAM_ELECTRICITY;

        if (defender->airborne())
            special_damage = special_damage * 2 / 3;

        if (needs_message && special_damage)
        {
            mprf("%s %s %s%s",
                 atk_name(DESC_CAP_THE).c_str(),
                 attacker->conj_verb("shock").c_str(),
                 mons_defender_name().c_str(),
                 special_attack_punctuation().c_str());
        }

        dprf("Shock damage: %d", special_damage);
        break;

    case AF_VAMPIRIC:
        // Only may bite non-vampiric monsters (or player) capable of bleeding.
        if (!defender->can_bleed())
            break;

        // Disallow draining of summoned monsters since they can't bleed.
        // XXX: Is this too harsh?
        if (defender->is_summoned())
            break;

        if (x_chance_in_y(defender->res_negative_energy(), 3))
            break;

        if (defender->stat_hp() < defender->stat_maxhp())
        {
            attacker->heal(1 + random2(damage_done), coinflip());

            if (needs_message)
            {
                mprf("%s %s strength from %s injuries!",
                     atk_name(DESC_CAP_THE).c_str(),
                     attacker->conj_verb("draw").c_str(),
                     def_name(DESC_NOCAP_ITS).c_str());
            }

            // 4.1.2 actually drains max hp; we're being nicer and just doing
            // a rot effect.
            if ((damage_done > 6 && one_chance_in(3)) || one_chance_in(20))
            {
                if (defender->atype() == ACT_PLAYER)
                    mprf("You feel less resilient.");
                rot_defender(0, coinflip() ? 2 : 1);
            }
        }
        break;

    case AF_DRAIN_STR:
    case AF_DRAIN_DEX:
        if ((one_chance_in(20) || (damage_done > 0 && one_chance_in(3)))
            && defender->res_negative_energy() < random2(4))
        {
            stat_type drained_stat = (flavour == AF_DRAIN_STR ? STAT_STR
                                                              : STAT_DEX);
            defender->drain_stat(drained_stat, 1, attacker);
        }
        break;

    case AF_HUNGER:
        if (defender->holiness() == MH_UNDEAD)
            break;

        if (one_chance_in(20) || (damage_done > 0 && coinflip()))
            defender->make_hungry(400, false);
        break;

    case AF_BLINK:
        if (one_chance_in(3))
        {
            if (attacker_visible)
            {
                mprf("%s %s!", attacker->name(DESC_CAP_THE).c_str(),
                     attacker->conj_verb("blink").c_str());
            }
            attacker->blink();
        }
        break;

    case AF_CONFUSE:
        if (attk.type == AT_SPORE)
        {
            if (defender->res_poison() > 0)
                break;

            if (--(attacker->as_monster()->hit_dice) <= 0)
                attacker->as_monster()->hit_points = -1;

            if (defender_visible)
            {
                mprf("%s %s engulfed in a cloud of spores!",
                     defender->name(DESC_CAP_THE).c_str(),
                     defender->conj_verb("are").c_str());
            }
        }

        if (one_chance_in(10)
            || (damage_done > 2 && one_chance_in(3)))
        {
            defender->confuse(attacker,
                              1 + random2(3+attacker->get_experience_level()));
        }
        break;

    case AF_DRAIN_XP:
        if (one_chance_in(30)
            || (damage_done > 5 && coinflip())
            || (attk.damage == 0 && !one_chance_in(3)))
        {
            drain_defender();
        }
        break;

    case AF_PARALYSE:
        // Only wasps at the moment.
        wasp_paralyse_defender();
        break;

    case AF_ACID:
        if (attacker->id() == MONS_SPINY_WORM && defender->res_poison() <= 0)
            defender->poison(attacker, 2 + random2(4));
        splash_defender_with_acid(3);
        break;

    case AF_DISTORT:
        distortion_affects_defender();
        break;

    case AF_RAGE:
        if (!one_chance_in(3) || !defender->can_go_berserk())
            break;

        if (needs_message)
        {
            mprf("%s %s %s!",
                 atk_name(DESC_CAP_THE).c_str(),
                 attacker->conj_verb("infuriate").c_str(),
                 mons_defender_name().c_str());
        }

        defender->go_berserk(false);
        break;

    case AF_NAPALM:
        mons_do_napalm();
        break;

    case AF_CHAOS:
        chaos_affects_defender();
        break;

    case AF_STEAL:
        // Ignore monsters, for now.
        if (defender->atype() != ACT_PLAYER)
            break;

        _steal_item_from_player(attacker->as_monster());
        break;

    case AF_STEAL_FOOD:
    {
        // Monsters don't carry food.
        if (defender->atype() != ACT_PLAYER)
            break;

        const bool stolen = expose_player_to_element(BEAM_DEVOUR_FOOD, 10);
        const bool ground = expose_items_to_element(BEAM_DEVOUR_FOOD, you.pos(),
                                                    10);
        if (needs_message)
        {
            if (stolen)
            {
                mprf("%s devours some of your food!",
                     atk_name(DESC_CAP_THE).c_str());
            }
            else if (ground)
            {
                mprf("%s devours some of the food beneath you!",
                     atk_name(DESC_CAP_THE).c_str());
            }
        }
        break;
    }

    case AF_CRUSH:
        mprf("%s %s being crushed%s",
             def_name(DESC_CAP_THE).c_str(),
             defender->conj_verb("are").c_str(),
             special_attack_punctuation().c_str());
        break;
    }
}

void melee_attack::mons_do_passive_freeze()
{
    if (you.mutation[MUT_PASSIVE_FREEZE]
        && attacker->alive()
        && grid_distance(you.pos(), attacker->as_monster()->pos()) == 1)
    {
        bolt beam;
        beam.flavour = BEAM_COLD;
        beam.thrower = KILL_YOU;

        monsters *mon = attacker->as_monster();

        const int orig_hurted = random2(11);
        int hurted = mons_adjust_flavoured(mon, beam, orig_hurted);

        if (!hurted)
            return;

        simple_monster_message(mon, " is very cold.");

#ifndef USE_TILE
        flash_monster_colour(mon, LIGHTBLUE, 200);
#endif

        mon->hurt(&you, hurted);

        if (mon->alive())
        {
            mon->expose_to_element(BEAM_COLD, orig_hurted);
            print_wounds(mon);

            const int cold_res = mon->res_cold();

            if (cold_res <= 0)
            {
                const int stun = (1 - cold_res) * random2(7);
                mon->speed_increment -= stun;
            }
        }
    }
}

void melee_attack::mons_do_eyeball_confusion()
{
    if (you.mutation[MUT_EYEBALLS]
        && attacker->alive()
        && grid_distance(you.pos(), attacker->as_monster()->pos()) == 1
        && x_chance_in_y(player_mutation_level(MUT_EYEBALLS), 20))
    {
        const int ench_pow = player_mutation_level(MUT_EYEBALLS) * 30;
        monsters *mon = attacker->as_monster();

        if(!mon->check_res_magic(ench_pow)
            && mons_class_is_confusable(mon->type))
        {
            mprf("The eyeballs on your body gaze at %s.",
                 mon->name(DESC_NOCAP_THE).c_str());

            mon->add_ench(mon_enchant(ENCH_CONFUSION, 0, KC_YOU,
                                      30 + random2(100)));
        }
    }
}

void melee_attack::mons_do_spines()
{
    const item_def *body = you.slot_item(EQ_BODY_ARMOUR, false);
    int evp = 0;
    defer_rand r;

    if (body)
        evp = -property(*body, PARM_EVASION);

    if (you.mutation[MUT_SPINY]
        && attacker->alive()
        && one_chance_in(evp + 1)
        && test_melee_hit(6 * player_mutation_level(MUT_SPINY),
                          defender->melee_evasion(attacker), r))
    {
        int dmg = roll_dice(player_mutation_level(MUT_SPINY), 6);
        int ac = random2(1+attacker->as_monster()->armour_class());

        int hurt = dmg - ac - evp;

        if (hurt <= 0)
            return;

        if (!defender_invisible)
        {
            simple_monster_message(attacker->as_monster(),
                                   " is struck by your spines.");
        }

        attacker->as_monster()->hurt(&you, hurt);
    }
}

void melee_attack::mons_perform_attack_rounds()
{
    const int nrounds = attacker->as_monster()->has_hydra_multi_attack() ?
        attacker->as_monster()->number : 4;
          coord_def pos    = defender->pos();
    const bool was_delayed = you_are_delayed();

    // Melee combat, tell attacker to wield its melee weapon.
    attacker->as_monster()->wield_melee_weapon();

    monsters* def_copy = NULL;
    int effective_attack_number = 0;
    for (attack_number = 0; attack_number < nrounds && attacker->alive();
         ++attack_number, ++effective_attack_number)
    {
        // Handle noise from previous round.
        if (effective_attack_number > 0)
            handle_noise(pos);

        if (!attacker->alive())
            return;

        // Monster went away?
        if (!defender->alive() || defender->pos() != pos)
        {
            if (attacker == defender
               || !attacker->as_monster()->has_multitargeting())
            {
                break;
            }

            // Hydras can try and pick up a new monster to attack to
            // finish out their round. -cao
            bool end = true;
            for (adjacent_iterator i(attacker->pos()); i; ++i)
            {
                if (*i == you.pos() && !attacker->as_monster()->friendly())
                {
                    attacker->as_monster()->foe = MHITYOU;
                    attacker->as_monster()->target = you.pos();
                    defender = &you;
                    end = false;
                    break;
                }

                monsters *mons = monster_at(*i);
                if (mons && !mons_aligned(attacker, mons))
                {
                    defender = mons;
                    end = false;
                    pos = mons->pos();
                    break;
                }
            }

            // No adjacent hostiles.
            if (end)
                break;
        }

        // Monsters hitting themselves get just one round.
        if (attack_number > 0 && attacker == defender)
            break;

        init_attack();

        mon_attack_def attk = mons_attack_spec(attacker->as_monster(),
                                                     attack_number);
        if (attk.type == AT_WEAP_ONLY)
        {
            int weap = attacker->as_monster()->inv[MSLOT_WEAPON];
            if (weap == NON_ITEM)
                attk.type = AT_NONE;
            else if (is_range_weapon(mitm[weap]))
                attk.type = AT_SHOOT;
            else
                attk.type = AT_HIT;
        }

        if (attk.type == AT_NONE)
        {
            // Make sure the monster uses up some energy, even though it
            // didn't actually attack.
            if (effective_attack_number == 0)
                attacker->as_monster()->lose_energy(EUT_ATTACK);
            break;
        }

        // Skip dummy attacks.
        if ((!unarmed_ok && attk.type != AT_HIT && attk.flavour != AF_REACH)
            || attk.type == AT_SHOOT)
        {
            --effective_attack_number;
            continue;
        }

        if (weapon == NULL)
        {
            switch(attk.type)
            {
            case AT_HEADBUTT:
            case AT_TENTACLE_SLAP:
            case AT_TAIL_SLAP:
                noise_factor = 150;
                break;

            case AT_HIT:
            case AT_PUNCH:
            case AT_KICK:
            case AT_CLAW:
            case AT_GORE:
                noise_factor = 125;
                break;

            case AT_BITE:
            case AT_PECK:
            case AT_CONSTRICT:
                noise_factor = 100;
                break;

            case AT_STING:
            case AT_SPORE:
            case AT_ENGULF:
                noise_factor = 75;
                break;

            case AT_TOUCH:
                noise_factor = 0;
                break;

            // To prevent compiler warnings.
            case AT_NONE:
            case AT_RANDOM:
            case AT_SHOOT:
                DEBUGSTR("Invalid attack flavour for noise_factor");
                break;

            default:
                DEBUGSTR("Unhandled attack flavour for noise_factor");
                break;
            }

            switch(attk.flavour)
            {
            case AF_FIRE:
                noise_factor += 50;
                break;

            case AF_ELEC:
                noise_factor += 100;
                break;

            default:
                break;
            }
        }

        damage_done = 0;
        mons_set_weapon(attk);
        to_hit = mons_to_hit();

        const bool chaos_attack = damage_brand == SPWPN_CHAOS
                                  || (attk.flavour == AF_CHAOS
                                      && attacker != defender);

        // Make copy of monster before monster_die() resets it.
        if (chaos_attack && defender->atype() == ACT_MONSTER && !def_copy)
            def_copy = new monsters(*defender->as_monster());

        final_attack_delay = mons_attk_delay();
        if (damage_brand == SPWPN_SPEED)
            final_attack_delay = final_attack_delay / 2 + 1;

        mons_lose_attack_energy(attacker->as_monster(),
                                final_attack_delay,
                                attack_number,
                                effective_attack_number);

        bool shield_blocked = false;
        bool this_round_hit = false;

        if (attacker != defender)
        {
            if (attack_shield_blocked(true))
            {
                shield_blocked   = true;
                perceived_attack = true;
                this_round_hit = did_hit = true;
            }
            else
                check_defender_train_dodging();
        }

        if (!shield_blocked)
        {
            const int defender_evasion = defender->melee_evasion(attacker);
            int defender_evasion_help
                = defender->melee_evasion(attacker, EV_IGNORE_HELPLESS);
            int defender_evasion_nophase
                = defender->melee_evasion(attacker, EV_IGNORE_PHASESHIFT);

            defer_rand r;

            if (defender_invisible)
            {
                // No evasion feedback if we don't know what we're
                // fighting.
                defender_evasion_help = defender_evasion;
                defender_evasion_nophase = defender_evasion;
            }

            if (attacker == defender
                || test_melee_hit(to_hit, defender_evasion_help, r))
            {
                // Will hit no matter what.
                this_round_hit = true;
            }
            else if (test_melee_hit(to_hit, defender_evasion, r))
            {
                if (needs_message)
                {
                    mprf("Helpless, %s %s to dodge %s attack.",
                         mons_defender_name().c_str(),
                         defender->conj_verb("fail").c_str(),
                         atk_name(DESC_NOCAP_ITS).c_str());
                }
                this_round_hit = true;
            }
            else if (test_melee_hit(to_hit, defender_evasion_nophase, r))
            {
                if (needs_message)
                {
                    mprf("%s momentarily %s out as %s "
                         "attack passes through %s.",
                         defender->name(DESC_CAP_THE).c_str(),
                         defender->conj_verb("phase").c_str(),
                         atk_name(DESC_NOCAP_ITS).c_str(),
                         defender->pronoun(PRONOUN_NOCAP).c_str());
                }
                this_round_hit = false;
            }
            else
            {
                // Misses no matter what.
                if (needs_message)
                {
                    mprf("%s misses %s.",
                         atk_name(DESC_CAP_THE).c_str(),
                         mons_defender_name().c_str());
                }
            }

            if (this_round_hit)
            {
                did_hit = true;
                perceived_attack = true;
                damage_done = mons_calc_damage(attk);
            }
            else
            {
                perceived_attack = perceived_attack || attacker_visible;
            }
        }

        if (check_unrand_effects())
            break;

        if (damage_done < 1 && this_round_hit && !shield_blocked)
            mons_announce_dud_hit(attk);

        if (damage_done > 0)
        {
            if (shield_blocked)
                dprf("ERROR: Non-zero damage after shield block!");
            mons_announce_hit(attk);
            check_defender_train_armour();

            if (defender->can_bleed()
                && !defender->is_summoned()
                && !defender->submerged())
            {
                int blood = _modify_blood_amount(damage_done,
                                                 attacker->damage_type());

                if (blood > defender->stat_hp())
                    blood = defender->stat_hp();

                bleed_onto_floor(pos, defender->id(), blood, true);
            }

            if (decapitate_hydra(damage_done,
                                 attacker->damage_type(attack_number)))
            {
                continue;
            }

            special_damage = 0;
            special_damage_message.clear();
            special_damage_flavour = BEAM_NONE;

            // Monsters attacking themselves don't get attack flavour.
            // The message sequences look too weird.  Also, stealing
            // attacks aren't handled until after the damage msg.
            if (attacker != defender && attk.flavour != AF_STEAL)
                mons_apply_attack_flavour(attk);

            if (needs_message && !special_damage_message.empty())
                mprf("%s", special_damage_message.c_str());

            // Defender banished.  Bail before chaos_killed_defender()
            // is called, since the defender is still alive in the
            // Abyss.
            if (is_banished(defender))
            {
                if (chaos_attack && attacker->alive())
                    chaos_affects_attacker();

                do_miscast();
                break;
            }

            defender->hurt(attacker, damage_done + special_damage,
                           special_damage_flavour);

            if (!defender->alive())
            {
                if (chaos_attack && defender->atype() == ACT_MONSTER)
                    chaos_killed_defender(def_copy);

                if (chaos_attack && attacker->alive())
                    chaos_affects_attacker();

                do_miscast();
                continue;
            }

            // Yredelemnul's injury mirroring can kill the attacker.
            // Also, bail if the monster is attacking itself without a
            // weapon, since intrinsic monster attack flavours aren't
            // applied for self-attacks.
            if (!attacker->alive() || (attacker == defender && !weapon))
            {
                if (miscast_target == defender)
                    do_miscast();
                break;
            }

            special_damage = 0;
            special_damage_message.clear();
            special_damage_flavour = BEAM_NONE;
            apply_damage_brand();

            if (needs_message && !special_damage_message.empty())
            {
                mprf("%s", special_damage_message.c_str());
                // Don't do message-only miscasts along with a special
                // damage message.
                if (miscast_level == 0)
                    miscast_level = -1;
            }

            if (special_damage > 0)
                defender->hurt(attacker, special_damage, special_damage_flavour);

            if (!defender->alive())
            {
                if (chaos_attack && defender->atype() == ACT_MONSTER)
                    chaos_killed_defender(def_copy);

                if (chaos_attack && attacker->alive())
                    chaos_affects_attacker();

                do_miscast();
                continue;
            }

            if (chaos_attack && attacker->alive())
                chaos_affects_attacker();

            if (miscast_target == defender)
                do_miscast();

            // Yredelemnul's injury mirroring can kill the attacker.
            if (!attacker->alive())
                break;

            if (miscast_target == attacker)
                do_miscast();

            // Miscast might have killed the attacker.
            if (!attacker->alive())
                break;

            if (attk.flavour == AF_STEAL)
                mons_apply_attack_flavour(attk);
        }

        item_def *weap = attacker->as_monster()->mslot_item(MSLOT_WEAPON);
        if (weap && you.can_see(attacker) && weap->cursed()
            && is_range_weapon(*weap))
        {
            set_ident_flags(*weap, ISFLAG_KNOW_CURSE);
        }

        if (!shield_blocked && attacker != defender &&
            defender->atype() == ACT_PLAYER &&
            (grid_distance(you.pos(), attacker->as_monster()->pos()) == 1
            || attk.flavour == AF_REACH))
        {
            // Check for spiny mutation.
            mons_do_spines();
        }
    }

    // Check for passive freeze or eyeball mutation.
    if (defender->atype() == ACT_PLAYER && defender->alive()
        && attacker != defender)
    {
        mons_do_eyeball_confusion();
        mons_do_passive_freeze();
    }

    // Handle noise from last round.
    handle_noise(pos);

    if (def_copy)
        delete def_copy;

    // Invisible monster might have interrupted butchering.
    if (was_delayed && defender->atype() == ACT_PLAYER && perceived_attack
        && !attacker_visible)
    {
        handle_interrupted_swap(false, true);
    }
}

bool melee_attack::mons_perform_attack()
{
    if (attacker != defender && mons_self_destructs())
        return (did_hit = perceived_attack = true);

    if (attacker != defender && mons_attack_warded_off())
    {
        // A warded-off attack takes half the normal energy.
        attacker->lose_energy(EUT_ATTACK, 2);

        perceived_attack = true;
        return (false);
    }

    mons_perform_attack_rounds();

    return (did_hit);
}

void melee_attack::mons_check_attack_perceived()
{
    if (!perceived_attack)
        return;

    if (attacker->alive() && defender->atype() == ACT_PLAYER)
    {
        interrupt_activity(AI_MONSTER_ATTACKS, attacker->as_monster());

        // If a friend wants to help, they can attack the attacking
        // monster, unless sanctuary is in effect since pet_target can
        // only be changed explicitly by the player during sanctuary.
        if (you.pet_target == MHITNOT && env.sanctuary_time <= 0)
            you.pet_target = attacker->mindex();
    }
}

bool melee_attack::mons_attack_you()
{
    mons_perform_attack();
    mons_check_attack_perceived();
    return (did_hit);
}

int melee_attack::mons_to_hit()
{
    const int hd_mult = mons_class_flag(attacker->id(), M_FIGHTER)? 25 : 15;
    int mhit = 18 + attacker->get_experience_level() * hd_mult / 10;

#ifdef DEBUG_DIAGNOSTICS
    const int base_hit = mhit;
#endif

    if (weapon && weapon->base_type == OBJ_WEAPONS)
        mhit += weapon->plus + property(*weapon, PWPN_HIT);

    if (attacker->confused())
        mhit -= 5;

    if (defender->backlit(true, false))
        mhit += 2 + random2(8);

     if (defender->atype() == ACT_PLAYER
         && player_mutation_level(MUT_TRANSLUCENT_SKIN) >= 3)
         mhit -= 5;

    // Invisible defender is hard to hit if you can't see invis. Note
    // that this applies only to monsters vs monster and monster vs
    // player. Does not apply to a player fighting an invisible
    // monster.
    if (!defender->visible_to(attacker))
        mhit = mhit * 65 / 100;

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "%s: Base to-hit: %d, Final to-hit: %d",
         attacker->name(DESC_PLAIN).c_str(),
         base_hit, mhit);
#endif

    return (mhit);
}

///////////////////////////////////////////////////////////////////////////

bool wielded_weapon_check(item_def *weapon, bool no_message)
{
    bool weapon_warning  = false;
    bool unarmed_warning = false;

    if (weapon)
    {
        if (needs_handle_warning(*weapon, OPER_ATTACK)
            || weapon->base_type != OBJ_STAVES
               && (weapon->base_type != OBJ_WEAPONS
                   || is_range_weapon(*weapon)))
        {
            weapon_warning = true;
        }
    }
    else if (you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED]
             && you_tran_can_wear(EQ_WEAPON))
    {
        unarmed_warning = true;
    }

    if (!you.received_weapon_warning && !you.confused()
        && (weapon_warning || unarmed_warning))
    {
        if (no_message)
            return (false);

        std::string prompt  = "Really attack while ";
        if (unarmed_warning)
            prompt += "being unarmed?";
        else
            prompt += "wielding " + weapon->name(DESC_NOCAP_YOUR) + "? ";

        const bool result = yesno(prompt.c_str(), true, 'n');

        learned_something_new(HINT_WIELD_WEAPON); // for hints mode Rangers

        // Don't warn again if you decide to continue your attack.
        if (result)
            you.received_weapon_warning = true;

        return (result);
    }

    return (true);
}

// Returns true if you hit the monster.
bool you_attack(int monster_attacked, bool unarmed_attacks)
{
    ASSERT(!crawl_state.game_is_arena());

    monsters *defender = &menv[monster_attacked];

    // Can't damage orbs or boulders this way.
    if (mons_is_projectile(defender->type) && !you.confused())
    {
        you.turn_is_over = false;
        return (false);
    }

    melee_attack attk(&you, defender, unarmed_attacks);

    // We're trying to hit a monster, break out of travel/explore now.
    if (!travel_kill_monster(defender))
        interrupt_activity(AI_HIT_MONSTER, defender);

    // Check if the player is fighting with something unsuitable.
    if (you.can_see(defender) && !wielded_weapon_check(attk.weapon))
    {
        you.turn_is_over = false;
        return (false);
    }

    bool attack = attk.attack();
    if (!attack)
    {
        // Attack was cancelled or unsuccessful...
        if (attk.cancel_attack)
            you.turn_is_over = false;
        return (false);
    }

    return (true);
}

// Lose attack energy for attacking with a weapon. which_attack is the actual
// attack number, effective_attack is the attack number excluding synthetic
// attacks (i.e. excluding M_ARCHER monsters' AT_SHOOT attacks).
static void mons_lose_attack_energy(monsters *attacker, int wpn_speed,
                                    int which_attack, int effective_attack)
{
    // Initial attack causes energy to be used for all attacks.  No
    // additional energy is used for unarmed attacks.
    if (effective_attack == 0)
        attacker->lose_energy(EUT_ATTACK);

    // Monsters lose additional energy only for the first two weapon
    // attacks; subsequent hits are free.
    if (effective_attack > 1)
        return;

    // speed adjustment for weapon using monsters
    if (wpn_speed > 0)
    {
        const int atk_speed = attacker->action_energy(EUT_ATTACK);
        // only get one third penalty/bonus for second weapons.
        if (effective_attack > 0)
            wpn_speed = div_rand_round( (2 * atk_speed + wpn_speed), 3 );

        int delta = div_rand_round( (wpn_speed - 10 + (atk_speed - 10)), 2 );
        if (delta > 0)
            attacker->speed_increment -= delta;
    }
}

bool monster_attack_actor(monsters *attacker, actor *defender,
                          bool allow_unarmed)
{
    ASSERT(defender == &you || defender->atype() == ACT_MONSTER);
    return (defender->atype() == ACT_PLAYER ?
              monster_attack(attacker, allow_unarmed)
            : monsters_fight(attacker, defender->as_monster(),
                             allow_unarmed));
}

// A monster attacking the player.
bool monster_attack(monsters* attacker, bool allow_unarmed)
{
    ASSERT(!crawl_state.game_is_arena());

    // Friendly and good neutral monsters won't attack unless confused.
    if (attacker->wont_attack() && !mons_is_confused(attacker))
        return (false);

    // In case the monster hasn't noticed you, bumping into it will
    // change that.
    behaviour_event(attacker, ME_ALERT, MHITYOU);
    melee_attack attk(attacker, &you, allow_unarmed);
    attk.attack();

    return (true);
}

// Two monsters fighting each other.
bool monsters_fight(monsters* attacker, monsters* defender,
                    bool allow_unarmed)
{
    melee_attack attk(attacker, defender, allow_unarmed);
    return (attk.attack());
}


/*
 **************************************************
 *                                                *
 *              END PUBLIC FUNCTIONS              *
 *                                                *
 **************************************************
*/

// Returns a value between 0 and 10 representing the weight given to str
int weapon_str_weight( object_class_type wpn_class, int wpn_type )
{
    int  ret;

    const int wpn_skill  = weapon_skill( wpn_class, wpn_type );
    const int hands      = hands_reqd( wpn_class, wpn_type, you.body_size() );

    // These are low values, because we'll be adding some bonus to the
    // larger weapons later.  Remember also that 1-1/2-hand weapons get
    // a bonus in player_weapon_str_weight() as well (can't be done
    // here because this function is used for cases where the weapon
    // isn't being used by the player).

    // Reasonings:
    // - Short Blades are the best for the dexterous... although they
    //   are very limited in damage potential
    // - Long Swords are better for the dexterous, the two-handed
    //   swords are a 50/50 split; bastard swords are in between.
    // - Staves: didn't want to punish the mages who want to use
    //   these... made it a 50/50 split after the 2-hnd bonus
    // - Polearms: Spears and tridents are the only ones that can
    //   be used one handed and are poking weapons, which requires
    //   more agility than strength.  The other ones also require a
    //   fair amount of agility so they end up at 50/50 (most can poke
    //   as well as slash... although slashing is not their strong
    //   point).
    // - Axes are weighted and edged and so require mostly strength,
    //   but not as much as Maces and Flails, which are typically
    //   blunt and spiked weapons.
    switch (wpn_skill)
    {
    case SK_SHORT_BLADES:     ret = 2; break;
    case SK_LONG_BLADES:      ret = 3; break;
    case SK_STAVES:           ret = 3; break;   // == 5 after 2-hand bonus
    case SK_POLEARMS:         ret = 3; break;   // most are +2 for 2-hands
    case SK_AXES:             ret = 6; break;
    case SK_MACES_FLAILS:     ret = 7; break;
    default:                  ret = 5; break;
    }

    // whips are special cased (because they are not much like maces)
    if (wpn_type == WPN_WHIP || wpn_type == WPN_DEMON_WHIP)
        ret = 2;
    else if (wpn_type == WPN_QUICK_BLADE) // high dex is very good for these
        ret = 1;

    if (hands == HANDS_TWO)
        ret += 2;

    // most weapons are capped at 8
    if (ret > 8)
    {
        // these weapons are huge, so strength plays a larger role
        if (wpn_type == WPN_GIANT_CLUB || wpn_type == WPN_GIANT_SPIKED_CLUB)
            ret = 9;
        else
            ret = 8;
    }

    return (ret);
}

// Returns a value from 0 to 10 representing the weight of strength to
// dexterity for the players currently wielded weapon.
static inline int player_weapon_str_weight()
{
    const item_def* weapon = you.weapon();

    // Unarmed, weighted slightly towards dex -- would have been more,
    // but then we'd be punishing Trolls and Ghouls who are strong and
    // get special unarmed bonuses.
    if (!weapon)
        return (4);

    int ret = weapon_str_weight(weapon->base_type, weapon->sub_type);

    if (hands_reqd(*weapon, you.body_size()) == HANDS_HALF && !you.shield())
        ret += 1;

    return (ret);
}

// weapon_dex_weight() + weapon_str_weight == 10, so we only need to
// define one of these.
static inline int player_weapon_dex_weight( void )
{
    return (10 - player_weapon_str_weight());
}

// weighted average of strength and dex, between (str+dex)/2 and dex
static inline int calc_stat_to_hit_base( void )
{
#ifdef USE_NEW_COMBAT_STATS

    // towards_str_avg is a variable, whose sign points towards strength,
    // and the magnitude is half the difference (thus, when added directly
    // to you.dex() it gives the average of the two.
    const signed int towards_str_avg = (you.strength() - you.dex()) / 2;

    // dex is modified by strength towards the average, by the
    // weighted amount weapon_str_weight() / 10.
    return (you.dex() + towards_str_avg * player_weapon_str_weight() / 10);

#else
    return (you.dex());
#endif
}

// weighted average of strength and dex, between str and (str+dex)/2
static inline int calc_stat_to_dam_base( void )
{
#ifdef USE_NEW_COMBAT_STATS

    const signed int towards_dex_avg = (you.dex() - you.strength()) / 2;
    return (you.strength() + towards_dex_avg * player_weapon_dex_weight() / 10);

#else
    return (you.strength());
#endif
}

static void stab_message(actor *defender, int stab_bonus)
{
    switch (stab_bonus)
    {
    case 6:     // big melee, monster surrounded/not paying attention
        if (coinflip())
        {
            mprf( "You strike %s from a blind spot!",
                  defender->name(DESC_NOCAP_THE).c_str() );
        }
        else
        {
            mprf( "You catch %s momentarily off-guard.",
                  defender->name(DESC_NOCAP_THE).c_str() );
        }
        break;
    case 4:     // confused/fleeing
        if (!one_chance_in(3))
        {
            mprf( "You catch %s completely off-guard!",
                  defender->name(DESC_NOCAP_THE).c_str() );
        }
        else
        {
            mprf( "You strike %s from behind!",
                  defender->name(DESC_NOCAP_THE).c_str() );
        }
        break;
    case 2:
    case 1:
        mprf( "%s fails to defend %s.",
              defender->name(DESC_CAP_THE).c_str(),
              defender->pronoun(PRONOUN_REFLEXIVE).c_str() );
        break;
    }
}
