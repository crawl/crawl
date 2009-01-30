/*
 *  File:       fight.cc
 *  Summary:    Functions used during combat.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "fight.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "beam.h"
#include "cloud.h"
#include "debug.h"
#include "delay.h"
#include "effects.h"
#include "food.h"
#include "it_use2.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "Kills.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "mon-pick.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mstuff2.h"
#include "mutation.h"
#include "ouch.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "skills.h"
#include "spells1.h"
#include "spells3.h"
#include "spells4.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "transfor.h"
#include "traps.h"
#include "tutorial.h"
#include "view.h"
#include "xom.h"

const int HIT_WEAK   = 7;
const int HIT_MED    = 18;
const int HIT_STRONG = 36;

static void stab_message(actor *defender, int stab_bonus);

static inline int player_weapon_str_weight( void );
static inline int player_weapon_dex_weight( void );

static inline int calc_stat_to_hit_base( void );
static inline int calc_stat_to_dam_base( void );
static void mons_lose_attack_energy(monsters *attacker, int wpn_speed,
                                    int which_attack);

/*
 **************************************************
 *                                                *
 *             BEGIN PUBLIC FUNCTIONS             *
 *                                                *
 **************************************************
*/

bool test_melee_hit(int to_hit, int ev)
{
    int   roll = -1;
    int margin = AUTOMATIC_HIT;

    ev *= 2;

    if (to_hit >= AUTOMATIC_HIT)
        return (true);
    else if (x_chance_in_y(MIN_HIT_MISS_PERCENTAGE, 100))
        margin = (coinflip() ? 1 : -1) * AUTOMATIC_HIT;
    else
    {
        roll = random2( to_hit + 1 );
        margin = (roll - random2avg(ev, 2));
    }

#if DEBUG_DIAGNOSTICS
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

    return ((you.strength - you.dex) * (str_weight - 5) / 10);
#else
    return (0);
#endif
}

// Returns the to-hit for your extra unarmed.attacks.
// DOES NOT do the final roll (i.e., random2(your_to_hit)).
static int calc_your_to_hit_unarmed(int uattack = UNAT_NO_ATTACK,
                                    bool vampiric = false)
{
    int your_to_hit;

    your_to_hit = 13 + you.dex / 2 + you.skills[SK_UNARMED_COMBAT] / 2
                  + you.skills[SK_FIGHTING] / 5;

    if (wearing_amulet(AMU_INACCURACY))
        your_to_hit -= 5;

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

// Calculates your heavy armour penalty. If random_factor is true,
// be stochastic; if false, deterministic (e.g. for chardumps.)
int calc_heavy_armour_penalty( bool random_factor )
{
    const bool ur_armed = (you.equip[EQ_WEAPON] != -1);
    int heavy_armour = 0;

    // heavy armour modifiers for shield borne
    if (you.shield())
    {
        switch (you.shield()->sub_type)
        {
        case ARM_SHIELD:
            if (you.skills[SK_SHIELDS] < maybe_random2(7, random_factor))
                heavy_armour++;
            break;
        case ARM_LARGE_SHIELD:
            if (you.species == SP_OGRE || you.species == SP_TROLL
                || player_genus(GENPC_DRACONIAN))
            {
                if (you.skills[SK_SHIELDS] < maybe_random2(13, random_factor))
                    heavy_armour++;     // was potentially "+= 3" {dlb}
            }
            else
            {
                for (int i = 0; i < 3; i++)
                {
                    if (you.skills[SK_SHIELDS] < maybe_random2(13,
                                                               random_factor))
                        heavy_armour += maybe_random2(3, random_factor);
                }
            }
            break;
        default:
            break;
        }
    }

    // Heavy armour modifiers for PARM_EVASION.
    if (player_wearing_slot(EQ_BODY_ARMOUR))
    {
        const int ev_pen = property( you.inv[you.equip[EQ_BODY_ARMOUR]],
                                     PARM_EVASION );

        if (ev_pen < 0 && maybe_random2(you.skills[SK_ARMOUR],
                                        random_factor) < abs(ev_pen))
        {
            heavy_armour += maybe_random2( abs(ev_pen), random_factor );
        }
    }

    // ??? what is the reasoning behind this ??? {dlb}
    // My guess is that its supposed to encourage monk-style play -- bwr
    if (!ur_armed && heavy_armour)
    {
        if (random_factor)
        {
            heavy_armour *= (coinflip() ? 3 : 2);
        }
        else
        {
            // avg. value: (2+3)/2
            heavy_armour *= 5;
            heavy_armour /= 2;
        }
    }
    return (heavy_armour);
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
    const monsters *def = (defender->atype() == ACT_PLAYER ? NULL :
                           dynamic_cast<const monsters*>(defender));
    unchivalric_attack_type unchivalric = UCAT_NO_ATTACK;

    // No unchivalric attacks on monsters that cannot fight (e.g.
    // plants) or monsters the attacker can't see (either due to
    // invisibility or being behind opaque clouds).
    if (defender->cannot_fight() || (attacker && !attacker->can_see(defender)))
        return (unchivalric);

    // Distracted (but not batty); this only applies to players.
    if (attacker && attacker->atype() == ACT_PLAYER && def->foe != MHITYOU
        && !mons_is_batty(def))
    {
        unchivalric = UCAT_DISTRACTED;
    }

    // confused (but not perma-confused)
    if (def->has_ench(ENCH_CONFUSION)
        && !mons_class_flag(def->type, M_CONFUSED))
    {
        unchivalric = UCAT_CONFUSED;
    }

    // fleeing
    if (mons_is_fleeing(def))
        unchivalric = UCAT_FLEEING;

    // invisible
    if (attacker && !attacker->visible_to(defender))
        unchivalric = UCAT_INVISIBLE;

    // held in a net
    if (mons_is_caught(def))
        unchivalric = UCAT_HELD_IN_NET;

    // petrifying
    if (mons_is_petrifying(def))
        unchivalric = UCAT_PETRIFYING;

    // paralysed
    if (def->cannot_act())
        unchivalric = UCAT_PARALYSED;

    // sleeping
    if (mons_is_sleeping(def))
        unchivalric = UCAT_SLEEPING;

    return (unchivalric);
}

//////////////////////////////////////////////////////////////////////////
// Melee attack

melee_attack::melee_attack(actor *attk, actor *defn,
                           bool allow_unarmed, int which_attack)
    : attacker(attk), defender(defn),
      cancel_attack(false),
      did_hit(false), perceived_attack(false), obvious_effect(false),
      needs_message(false), attacker_visible(false), defender_visible(false),
      attacker_invisible(false), defender_invisible(false),
      unarmed_ok(allow_unarmed),
      attack_number(which_attack),
      to_hit(0), base_damage(0), potential_damage(0), damage_done(0),
      special_damage(0), aux_damage(0), stab_attempt(false), stab_bonus(0),
      weapon(NULL), damage_brand(SPWPN_NORMAL),
      wpn_skill(SK_UNARMED_COMBAT), hands(HANDS_ONE),
      spwld(SPWLD_NONE), hand_half_bonus(false),
      art_props(0), attack_verb("bug"), verb_degree(),
      no_damage_message(), special_damage_message(), unarmed_attack(),
      shield(NULL), defender_shield(NULL),
      heavy_armour_penalty(0), can_do_unarmed(false),
      water_attack(false), miscast_level(-1), miscast_type(SPTYP_NONE),
      miscast_target(NULL)
{
    init_attack();
}

void melee_attack::check_hand_half_bonus_eligible()
{
    hand_half_bonus = (unarmed_ok
                       && !can_do_unarmed
                       && !shield
                       && weapon
                       && !item_cursed( *weapon )
                       && hands == HANDS_HALF);
}

void melee_attack::init_attack()
{
    weapon       = attacker->weapon(attack_number);
    damage_brand = attacker->damage_brand(attack_number);

    if (weapon && weapon->base_type == OBJ_WEAPONS
        && is_random_artefact( *weapon ))
    {
        randart_wpn_properties( *weapon, art_props );
    }

    wpn_skill = weapon ? weapon_skill( *weapon ) : SK_UNARMED_COMBAT;
    if (weapon)
    {
        hands = hands_reqd( *weapon, attacker->body_size() );
        spwld = item_special_wield_effect( *weapon );
    }

    shield = attacker->shield();
    if (defender)
        defender_shield = defender->shield();

    water_attack       = is_water_attack(attacker, defender);
    attacker_visible   = attacker->visible() || crawl_state.arena;
    attacker_invisible = (!attacker_visible && see_grid(attacker->pos()));
    defender_visible   = (defender && (defender->visible()
                                       || crawl_state.arena));
    defender_invisible = (!defender_visible && defender
                          && see_grid(defender->pos()));
    needs_message      = (attacker_visible || defender_visible);

    if (defender && defender->submerged())
        unarmed_ok = false;

    miscast_level  = -1;
    miscast_type   = SPTYP_NONE;
    miscast_target = NULL;
}

std::string melee_attack::actor_name(const actor *a,
                                     description_level_type desc,
                                     bool actor_visible,
                                     bool actor_invisible)
{
    return (actor_visible? a->name(desc) : anon_name(desc, actor_invisible));
}

std::string melee_attack::pronoun(const actor *a,
                                  pronoun_type pron,
                                  bool actor_visible)
{
    return (actor_visible? a->pronoun(pron) : anon_pronoun(pron));
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
        name = apostrophise(atk_name(desc));

    name += weapon->name(desc, false, false, false, false, ignore_flags);

    return (name);
}

bool melee_attack::is_banished(const actor *a) const
{
    if (!a || a->alive())
        return (false);

    if (a->atype() == ACT_PLAYER)
        return (you.banished);
    else
        return (dynamic_cast<const monsters*>(a)->flags & MF_BANISHED);
}

bool melee_attack::is_water_attack(const actor *attk,
                                   const actor *defn) const
{
    return (defn && attk->swimming() && defn->floundering());
}

void melee_attack::check_autoberserk()
{
    if (weapon
        && art_props[RAP_ANGRY] >= 1
        && !one_chance_in(1 + art_props[RAP_ANGRY]))
    {
        attacker->go_berserk(false);
    }
}

void melee_attack::check_special_wield_effects()
{
    switch (spwld)
    {
    case SPWLD_TROG:
        if (coinflip())
            attacker->go_berserk(false);
        break;

    case SPWLD_WUCAD_MU:
        if (one_chance_in(9))
        {
            MiscastEffect(attacker, MELEE_MISCAST, SPTYP_DIVINATION,
                          random2(9), random2(70), "the Staff of Wucad Mu");
        }
        break;
    default:
        break;
    }
}

void melee_attack::identify_mimic(actor *act)
{
    if (act
        && act->atype() == ACT_MONSTER
        && mons_is_mimic(act->id())
        && you.can_see(act))
    {
        monsters* mon = dynamic_cast<monsters*>(act);
        mon->flags |= MF_KNOWN_MIMIC;
    }
}

bool melee_attack::attack()
{
    // If a mimic is attacking or defending, it is thereafter known.
    identify_mimic(attacker);
    identify_mimic(defender);

    const coord_def def_pos = defender->pos();

    if (attacker->atype() == ACT_PLAYER && defender->atype() == ACT_MONSTER)
    {
        if (stop_attack_prompt(defender_as_monster(), false, false))
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

    check_special_wield_effects();

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
    // Non-fumbled self-attacks due to confusion are still pretty
    // funny, though.
    else if (attacker == defender && attacker->confused())
    {
        // And is still hilarious if it's the player.
        if (attacker->atype() == ACT_PLAYER)
            xom_is_stimulated(255);
        else
            xom_is_stimulated(128);
    }

    // Defending monster protects itself from attacks using the
    // wall it's in.
    if (defender->atype() == ACT_MONSTER && grid_is_solid(defender->pos())
        && mons_wall_shielded(defender_as_monster()))
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
            // Make sure the monster uses up some energy, even though
            // it didn't actually land a blow.
            attacker->lose_energy(EUT_ATTACK);

            if (!mons_near(defender_as_monster()))
            {
                simple_monster_message(attacker_as_monster(),
                                       " hits something");
            }
            else if (!you.can_see(attacker))
            {
                mprf("%s hits the %s.", defender->name(DESC_CAP_THE).c_str(),
                     feat_name.c_str());
            }
            else
            {
                mprf("%s tries to hit the %s, but is blocked by the %s.",
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
        set_attack_conducts(conducts, defender_as_monster());

    // Trying to stay general beyond this point is a recipe for insanity.
    // Maybe when Stone Soup hits 1.0... :-)
    bool retval = ((attacker->atype() == ACT_PLAYER) ? player_attack() :
                   (defender->atype() == ACT_PLAYER) ? mons_attack_you()
                                                     : mons_attack_mons());

    if (env.sanctuary_time > 0 && retval && !cancel_attack
        && attacker != defender && !attacker->confused())
    {
        if (is_sanctuary(attacker->pos()) || is_sanctuary(defender->pos()))
        {
            if (attacker->atype() == ACT_PLAYER
                || mons_friendly(attacker_as_monster()))
            {
                remove_sanctuary(true);
            }
        }
    }

    if (attacker->atype() == ACT_PLAYER)
    {
        if (damage_brand == SPWPN_CHAOS)
            chaos_affects_attacker();

        do_miscast();
    }

    enable_attack_conducts(conducts);

    return retval;
}

static int _modify_blood_amount(const int damage, const int dam_type)
{
    int factor = 0; // DVORP_NONE

    switch (dam_type)
    {
    case DVORP_CRUSHING: // flails, also unarmed
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
// Called when stabbing and for bite attacks.
// Returns true if blood was drawn.
static bool _player_vampire_draws_blood(const monsters* mon, const int damage,
                                        bool needs_bite_msg = false)
{
    ASSERT(you.species == SP_VAMPIRE);

    if (!_vamp_wants_blood_from_monster(mon))
        return (false);

    const int chunk_type = mons_corpse_effect(mon->type);

    // Now print message, need biting unless already done (never for bat form!)
    if (needs_bite_msg && you.attribute[ATTR_TRANSFORMATION] != TRAN_BAT)
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
        if (heal > you.experience_level)
            heal = you.experience_level;

        if (chunk_type == CE_CLEAN)
            heal += 1 + random2(damage);

        // Decrease healing when done in bat form.
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT)
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
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT)
            food_value /= 2;

        lessen_hunger(food_value, false);
    }

    did_god_conduct(DID_DRINK_BLOOD, 5 + random2(4));

    return (true);
}

bool melee_attack::player_attack()
{
    if (cancel_attack)
        return (false);

    potential_damage =
        !weapon ? player_calc_base_unarmed_damage()
                : player_calc_base_weapon_damage();

    player_apply_attack_delay();
    player_stab_check();

    coord_def where = defender->pos();

    if (player_hits_monster())
    {
        did_hit = true;
        if (Options.tutorial_left)
            Options.tut_melee_counter++;

        const bool shield_blocked = attack_shield_blocked(true);

        if (shield_blocked)
            damage_done = 0;
        else
        {
            // This actually does more than calculate damage - it also
            // sets up messages, etc.
            player_calc_hit_damage();
        }

        bool hit_woke_orc = false;
        if (you.religion == GOD_BEOGH && defender->mons_species() == MONS_ORC
            && defender->asleep() && !player_under_penance()
            && you.piety >= piety_breakpoint(2)
            && mons_near(defender_as_monster()))
        {
            hit_woke_orc = true;
        }

        // Always upset monster regardless of damage.
        behaviour_event(defender_as_monster(), ME_WHACK, MHITYOU);

        if (damage_done > 0
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

        player_hurt_monster();

        if (damage_done)
            player_exercise_combat_skills();

        if (player_check_monster_died())
            return (true);

        player_sustain_passive_damage();

        // Thirsty stabbing vampires get to draw blood.
        if (you.species == SP_VAMPIRE && you.hunger_state < HS_SATIATED
            && stab_attempt && stab_bonus > 0)
        {
            _player_vampire_draws_blood(defender_as_monster(),
                                        damage_done, true);
        }

        // At this point, pretend we didn't hit at all.
        if (shield_blocked)
            did_hit = false;

        if (hit_woke_orc)
        {
            // Call function of orcs first noticing you, but with
            // beaten-up conversion messages (if applicable).
            beogh_follower_convert(defender_as_monster(), true);
        }
    }
    else
        player_warn_miss();

    if (did_hit && player_monattk_hit_effects(false))
        return (true);

    const bool did_primary_hit = did_hit;

    if (unarmed_ok && where == defender->pos() && player_aux_unarmed())
        return (true);

    if ((did_primary_hit || did_hit) && defender->alive()
        && where == defender->pos())
    {
        print_wounds(defender_as_monster());
    }

    return (did_primary_hit || did_hit);
}

// Returns true to end the attack round.
bool melee_attack::player_aux_unarmed()
{
    unwind_var<int> save_brand(damage_brand);

    damage_brand = SPWPN_NORMAL;
    int uattack  = UNAT_NO_ATTACK;
    bool simple_miss_message = false;
    std::string miss_verb;

    if (can_do_unarmed)
    {
        if (you.species == SP_NAGA)
            uattack = UNAT_HEADBUTT;
        else
            uattack = (coinflip() ? UNAT_HEADBUTT : UNAT_KICK);

        if (player_mutation_level(MUT_FANGS)
            || you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON)
        {
            uattack = UNAT_BITE;
        }

        if ((you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON
               || player_genus(GENPC_DRACONIAN)
               || (you.species == SP_MERFOLK && player_is_swimming())
               || player_mutation_level( MUT_STINGER ))
            && one_chance_in(3))
        {
            uattack = UNAT_TAILSLAP;
        }

        if (coinflip())
            uattack = UNAT_PUNCH;

        if (you.species == SP_VAMPIRE && !one_chance_in(3))
            uattack = UNAT_BITE;
    }

    for (int scount = 0; scount < 5; scount++)
    {
        unarmed_attack.clear();
        miss_verb.clear();
        simple_miss_message = false;
        damage_brand = SPWPN_NORMAL;
        aux_damage = 0;

        switch (scount)
        {
        case 0:
        {
            if (uattack != UNAT_KICK)        //jmf: hooves mutation
            {
                if (!player_mutation_level(MUT_HOOVES)
                        && !player_mutation_level(MUT_TALONS)
                    || coinflip())
                {
                    continue;
                }
            }

            if (you.attribute[ATTR_TRANSFORMATION] == TRAN_SERPENT_OF_HELL
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_ICE_BEAST
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_SPIDER
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT)
            {
                continue;
            }

            // Kenku have large taloned feet that do good damage.
            const bool clawed_kick = player_mutation_level(MUT_TALONS);

            if (clawed_kick)
            {
                unarmed_attack = "claw";
                miss_verb      = "kick";
            }
            else
                unarmed_attack = "kick";

            aux_damage = (player_mutation_level(MUT_HOOVES) ? 10
                          : clawed_kick                     ?  8 : 5);
            break;
        }

        case 1:
            if (uattack != UNAT_HEADBUTT)
            {
                if (!player_mutation_level(MUT_HORNS)
                       && !player_mutation_level(MUT_BEAK)
                    || !one_chance_in(3))
                {
                    continue;
                }
            }

            if (you.attribute[ATTR_TRANSFORMATION] == TRAN_SERPENT_OF_HELL
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_SPIDER
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_ICE_BEAST
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT)
            {
                continue;
            }

            if (player_mutation_level(MUT_BEAK)
                && (!player_mutation_level(MUT_HORNS) || coinflip()))
            {
                unarmed_attack = "peck";
                aux_damage = 6;
            }
            else
            {
                // Minotaurs used to get +5 damage here, now they get
                // +6 because of the horns.
                unarmed_attack = "headbutt";
                aux_damage = 5 + player_mutation_level(MUT_HORNS) * 3;

                if (you.equip[EQ_HELMET] != -1)
                {
                    const item_def& helmet = you.inv[you.equip[EQ_HELMET]];
                    if (is_hard_helmet(helmet))
                    {
                        aux_damage += 2;
                        if (get_helmet_desc(helmet) == THELM_DESC_SPIKED
                            || get_helmet_desc(helmet) == THELM_DESC_HORNED)
                        {
                            aux_damage += 3;
                        }
                    }
                }
            }
            break;

        case 2:             // draconians
            if (uattack != UNAT_TAILSLAP)
            {
                // not draconian, and not wet merfolk
                if (!player_genus(GENPC_DRACONIAN)
                       && !(you.species == SP_MERFOLK && player_is_swimming())
                       && !player_mutation_level(MUT_STINGER)
                    || (!one_chance_in(4)))

                {
                    continue;
                }
            }

            if (you.attribute[ATTR_TRANSFORMATION] == TRAN_SPIDER
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_ICE_BEAST
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT)
            {
                continue;
            }

            unarmed_attack = "tail-slap";
            aux_damage = 6;

            if (player_mutation_level( MUT_STINGER ) > 0)
            {
                aux_damage += (player_mutation_level( MUT_STINGER ) * 2 - 1);
                damage_brand  = SPWPN_VENOM;
            }

            // Grey dracs have spiny tails, or something.
            // Maybe add this to player messaging. {dlb}
            //
            // STINGER mutation doesn't give extra damage here... that
            // would probably be a bit much, we'll still get the
            // poison bonus so it's still somewhat good.
            if (you.species == SP_GREY_DRACONIAN && you.experience_level >= 7)
                aux_damage = 12;
            break;

        case 3:
            if (uattack != UNAT_PUNCH)
                continue;

            if (you.attribute[ATTR_TRANSFORMATION] == TRAN_SERPENT_OF_HELL
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_SPIDER
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_ICE_BEAST
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT)
            {
                continue;
            }

            // No punching with a shield or 2-handed wpn, except staves.
            if (shield || coinflip()
                || (weapon
                    && hands == HANDS_TWO
                    && weapon->base_type != OBJ_STAVES
                    && weapon_skill(*weapon) != SK_STAVES))
            {
                continue;
            }

            unarmed_attack = "punch";
            // applied twice
            aux_damage = 5 + you.skills[SK_UNARMED_COMBAT] / 3;

            if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
            {
                unarmed_attack = "slash";
                aux_damage += 6;
                simple_miss_message = true;
            }
            else if (you.has_usable_claws())
            {
                unarmed_attack = "claw";
                aux_damage += roll_dice(1, 3);
                simple_miss_message = true;
            }

            break;

        case 4:
            if (uattack != UNAT_BITE)
                continue;

            if (!player_mutation_level(MUT_FANGS))
                continue;

            if (you.species != SP_VAMPIRE && one_chance_in(5)
                || one_chance_in(7))
            {
                continue;
            }

            unarmed_attack = "bite";
            simple_miss_message = true;
            aux_damage += player_mutation_level(MUT_FANGS) * 2
                          + you.skills[SK_UNARMED_COMBAT] / 5;

            if (you.species == SP_VAMPIRE)
            {
                if (_vamp_wants_blood_from_monster(defender_as_monster()))
                {
                    // prob of vampiric bite:
                    // 1/4 when non-thirsty, 1/2 when thirsty, 100% when
                    // bloodless
                    if (you.hunger_state >= HS_SATIATED && coinflip())
                        break;

                    if (you.hunger_state != HS_STARVING && coinflip())
                        break;

                    damage_brand = SPWPN_VAMPIRICISM;
                }
                else if (!one_chance_in(3)) // monster not interesting bloodwise
                    continue;
            }

            break;

            // To add more, add to while part of loop below as well
        default:
            continue;
        }

        // unified to-hit calculation
        to_hit = random2( calc_your_to_hit_unarmed(uattack,
                          damage_brand == SPWPN_VAMPIRICISM) );

        make_hungry(2, true);

        alert_nearby_monsters();

        // XXX We're clobbering did_hit
        did_hit = false;

        bool ely_block = false;
        const int evasion = defender->melee_evasion(attacker);
        if (you.religion != GOD_ELYVILON
            && you.penance[GOD_ELYVILON]
            && to_hit >= evasion
            && one_chance_in(20))
        {
            simple_god_message(" blocks your attack.", GOD_ELYVILON);
            ely_block = true;
        }

        if (!ely_block && (to_hit >= evasion || one_chance_in(30)))
        {
            // Upset the monster.
            behaviour_event(defender_as_monster(), ME_WHACK, MHITYOU);

            if (attack_shield_blocked(true))
                continue;
            if (player_apply_aux_unarmed())
                return (true);
        }
        else
        {
            if (simple_miss_message)
            {
                mprf("You miss %s.",
                     defender->name(DESC_NOCAP_THE).c_str());
            }
            else
            {
                mprf("Your %s misses %s.",
                     miss_verb.empty()? unarmed_attack.c_str()
                     : miss_verb.c_str(),
                     defender->name(DESC_NOCAP_THE).c_str());
            }

            if (ely_block)
                dec_penance(GOD_ELYVILON, 1 + random2(to_hit - evasion));
        }
    }

    return (false);
}

bool melee_attack::player_apply_aux_unarmed()
{
    did_hit = true;

    aux_damage  = player_aux_stat_modify_damage(aux_damage);
    aux_damage += slaying_bonus(PWPN_DAMAGE);

    aux_damage  = random2(aux_damage);

    // Clobber wpn_skill.
    wpn_skill   = SK_UNARMED_COMBAT;
    aux_damage  = player_apply_weapon_skill(aux_damage);
    aux_damage  = player_apply_fighting_skill(aux_damage, true);
    aux_damage  = player_apply_misc_modifiers(aux_damage);

    // Clear stab bonus which will be set for the primary weapon attack.
    stab_bonus  = 0;
    aux_damage  = player_apply_monster_ac(aux_damage);

    aux_damage  = defender->hurt(&you, aux_damage, BEAM_MISSILE, false);
    damage_done = aux_damage;

    if (damage_done > 0)
    {
        player_exercise_combat_skills();

        mprf("You %s %s%s%s",
             unarmed_attack.c_str(),
             defender->name(DESC_NOCAP_THE).c_str(),
             debug_damage_number().c_str(),
             attack_strength_punctuation().c_str());

        if (damage_brand == SPWPN_VENOM && coinflip())
            poison_monster(defender_as_monster(), KC_YOU);

        // Normal vampiric biting attack, not if already got stabbing special.
        if (damage_brand == SPWPN_VAMPIRICISM && you.species == SP_VAMPIRE
            && (!stab_attempt || stab_bonus <= 0))
        {
            _player_vampire_draws_blood(defender_as_monster(), damage_done);
        }
    }
    else // no damage was done
    {
        mprf("You %s %s%s.",
             unarmed_attack.c_str(),
             defender->name(DESC_NOCAP_THE).c_str(),
             you.can_see(defender) ? ", but do no damage" : "");
    }

    if (defender_as_monster()->hit_points < 1)
    {
        _monster_die(defender_as_monster(), KILL_YOU, NON_MONSTER);

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
    if ((to_hit + heavy_armour_penalty/2) >= defender->melee_evasion(attacker))
        return "Your armour prevents you from hitting ";
    else
        return "You miss ";
}

void melee_attack::player_warn_miss()
{
    did_hit = false;

    // Upset only non-sleeping monsters if we missed.
    if (!defender->asleep())
        behaviour_event(defender_as_monster(), ME_WHACK, MHITYOU);

    msg::stream << player_why_missed()
                << defender->name(DESC_NOCAP_THE)
                << "."
                << std::endl;
}

bool melee_attack::player_hits_monster()
{
    const int evasion = defender->melee_evasion(attacker);
#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "your to-hit: %d; defender effective EV: %d",
         to_hit, evasion);
#endif

    return (to_hit >= evasion
            || one_chance_in(20)
            || ((defender->cannot_act() || defender->asleep())
                && !one_chance_in(10 + you.skills[SK_STABBING]))
            || mons_is_petrifying(defender_as_monster())
               && !one_chance_in(2 + you.skills[SK_STABBING]));
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

int melee_attack::player_apply_water_attack_bonus(int damage)
{
    // Yes, this isn't the *2 damage that monsters get, but since
    // monster hps and player hps are different (as are the sizes
    // of the attacks) it just wouldn't be fair to give merfolk
    // that gross a potential... still they do get something for
    // making use of the water.  -- bwr
    // return (damage + random2avg(10,2));

    // [dshaligram] Experimenting with the full double damage bonus since
    // it takes real effort to set up a water attack and it's very situational.
    return (damage * 2);
}

int melee_attack::player_apply_weapon_skill(int damage)
{
    if (weapon && (weapon->base_type == OBJ_WEAPONS
                   || item_is_staff( *weapon )))
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
    if (weapon && weapon->base_type == OBJ_WEAPONS)
    {
        int wpn_damage_plus = weapon->plus2;

        damage += (wpn_damage_plus > -1) ? (random2(1 + wpn_damage_plus))
                                         : -(1 + random2(-wpn_damage_plus));

        // removed 2-handed weapons from here... their "bonus" is
        // already included in the damage stat for the weapon -- bwr
        if (hand_half_bonus)
            damage += random2(3);

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
    switch (wpn_skill)
    {
    case SK_SHORT_BLADES:
    {
        int bonus = (you.dex * (you.skills[SK_STABBING] + 1)) / 5;

        if (weapon->sub_type != WPN_DAGGER)
            bonus /= 2;

        bonus   = stepdown_value( bonus, 10, 10, 30, 30 );
        damage += bonus;
    }
    // fall through
    case SK_LONG_BLADES:
        damage *= 10 + you.skills[SK_STABBING] /
                       (stab_bonus + (wpn_skill == SK_SHORT_BLADES ? 0 : 1));
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
            if (x_chance_in_y(you.skills[SK_STABBING] + you.dex + 1, 200))
            {
                int stun = random2(you.dex + 1);

                if (defender_as_monster()->speed_increment > stun)
                    defender_as_monster()->speed_increment -= stun;
                else
                    defender_as_monster()->speed_increment = 0;
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

    if (mons_is_petrified(defender_as_monster()))
        damage /= 3;

    return (damage);
}

int melee_attack::player_weapon_type_modify(int damage)
{
    int weap_type = WPN_UNKNOWN;

    if (!weapon)
        weap_type = WPN_UNARMED;
    else if (item_is_staff( *weapon ))
        weap_type = WPN_QUARTERSTAFF;
    else if (weapon->base_type == OBJ_WEAPONS)
        weap_type = weapon->sub_type;

    // All weak hits look the same, except for when the player
    // has a non-weapon in hand.  -- bwr
    // Exception: vampire bats only _bite_ to allow for drawing blood
    if (damage < HIT_WEAK && (you.species != SP_VAMPIRE
        || you.attribute[ATTR_TRANSFORMATION] != TRAN_BAT))
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
        case TRAN_SERPENT_OF_HELL:
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
        case TRAN_AIR:
            attack_verb = "buffet";
            break;
        } // transformations

        return (damage);
    }

    switch (weapon ? get_damage_type(*weapon) : -1)
    {
    case DAM_PIERCE:
        if (damage < HIT_MED)
            attack_verb = "puncture";
        else if (damage < HIT_STRONG)
            attack_verb = "impale";
        else
        {
            attack_verb = "spit";
            verb_degree = " like a pig";
        }
        break;

    case DAM_SLICE:
        if (damage < HIT_MED)
            attack_verb = "slash";
        else if (damage < HIT_STRONG)
            attack_verb = "slice";
        else
        {
            attack_verb = "open";
            verb_degree = " like a pillowcase";
        }
        break;

    case DAM_BLUDGEON:
        if (damage < HIT_MED)
            attack_verb = one_chance_in(4)? "thump" : "sock";
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
        else
            attack_verb = "thrash";
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

bool melee_attack::player_hurt_monster()
{
    return damage_done && (damage_done = defender->hurt(&you, damage_done,
                                                        BEAM_MISSILE, false));
}

void melee_attack::player_exercise_combat_skills()
{
    const bool helpless = defender->cannot_fight();

    if (!helpless || you.skills[ wpn_skill ] < 2)
        exercise( wpn_skill, 1 );

    if ((!helpless || you.skills[SK_FIGHTING] < 2)
        && one_chance_in(3))
    {
        exercise(SK_FIGHTING, 1);
    }
}

void melee_attack::player_check_weapon_effects()
{
    if (spwld == SPWLD_TORMENT && coinflip())
    {
        torment(TORMENT_SPWLD, you.pos());
        did_god_conduct(DID_UNHOLY, 5);
    }

    if (spwld == SPWLD_ZONGULDROK || spwld == SPWLD_CURSE)
        did_god_conduct(DID_NECROMANCY, 3);

    if (weapon)
    {
        if (weapon->base_type == OBJ_WEAPONS)
        {
            if (is_holy_item(*weapon))
                did_god_conduct(DID_HOLY, 1);
            else if (is_demonic(*weapon))
                did_god_conduct(DID_UNHOLY, 1);
        }

        if (is_fixed_artefact(*weapon))
        {
            switch (weapon->special)
            {
            case SPWPN_SCEPTRE_OF_ASMODEUS:
            case SPWPN_STAFF_OF_DISPATER:
            case SPWPN_SWORD_OF_CEREBOV:
                did_god_conduct(DID_UNHOLY, 3);
                break;
            default:
                break;
            }
        }
    }
}

// Returns true if the combat round should end here.
bool melee_attack::player_monattk_hit_effects(bool mondied)
{
    player_check_weapon_effects();

    // Thirsty vampires will try to use a stabbing situation to draw blood.
    if (you.species == SP_VAMPIRE && you.hunger_state < HS_SATIATED
        && mondied && stab_attempt && stab_bonus > 0
        && _player_vampire_draws_blood(defender_as_monster(),
                                       damage_done, true))
    {
        // No further effects.
    }
    // Vampiric *weapon* effects for the killing blow.
    else if (mondied && damage_brand == SPWPN_VAMPIRICISM
             && you.equip[EQ_WEAPON] != -1)
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

#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS,
                 "Vampiric healing: damage %d, healed %d",
                 damage_done, heal);
#endif
            inc_hp(heal, false);

            if (you.hunger_state != HS_ENGORGED)
                lessen_hunger(30 + random2avg(59, 2), false);

            did_god_conduct(DID_NECROMANCY, 2);
        }
    }

    if (mondied)
        return (true);

    // These effects apply only to monsters that are still alive:

    // Returns true if a head was cut off *and* the wound was cauterized,
    // in which case the cauterization was the ego effect, so don't burn
    // the hydra some more.
    //
    // Also returns true if the hydra's last head was cut off, in which
    // case nothing more should be done to the last hydra.
    if (decapitate_hydra(damage_done))
        return (!defender->alive());

    // These two (staff damage and damage brand) are mutually exclusive!
    player_apply_staff_damage();

    // Returns true if the monster croaked.
    if (!special_damage && apply_damage_brand())
        return (true);

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
    mprf(MSGCH_DIAGNOSTICS, "Special damage to %s: %d",
         defender->name(DESC_NOCAP_THE).c_str(),
         special_damage);
#endif

    special_damage = defender->hurt(&you, special_damage);

    return (!defender->alive());
}

void melee_attack::_monster_die(monsters* monster, killer_type killer,
                                int killer_index)
{
    const bool chaos = (damage_brand == SPWPN_CHAOS);

    // Copy defender before it gets reset by monster_die()
    monsters* def_copy = NULL;
    if (chaos)
        def_copy = new monsters(*monster);

    monster_die(monster, killer, killer_index);

    if (chaos)
    {
        chaos_killed_defender(def_copy);
        delete def_copy;
    }
}

static bool is_boolean_resist(beam_type flavour)
{
    switch (flavour)
    {
    case BEAM_ELECTRICITY:
    case BEAM_NAPALM:
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
    // Assume ice storm and ice bolt are mostly solid.
    case BEAM_ICE:
        return (25);

    case BEAM_LAVA:
        return (35);

    case BEAM_POISON_ARROW:
        return (40);

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

    // Check if this is a resist that pretends to be boolean for damage
    // purposes.  Only electricity and sticky flame (napalm) do this at
    // the moment; raw poison damage uses the normal formula.
    int res_base = (is_boolean_resist(flavour) ? 2 : 1);
    const int resistible_fraction = get_resistible_fraction(flavour);

    int resistible = rawdamage * resistible_fraction / 100;
    const int irresistible = rawdamage - resistible;

    if (res > 0)
    {
        if (monster && res >= 3)
            resistible = 0;
        else
            resistible /= res_base + res * res;
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

    if (special_damage > 0 && verb && needs_message)
    {
        special_damage_message = make_stringf(
            "%s %s %s%s",
            atk_name(DESC_CAP_THE).c_str(),
            attacker->conj_verb(verb).c_str(),
            def_name(DESC_NOCAP_THE).c_str(),
            special_attack_punctuation().c_str());
    }
}

int melee_attack::fire_res_apply_cerebov_downgrade(int res)
{
    if (weapon && weapon->special == SPWPN_SWORD_OF_CEREBOV)
        --res;

    return (res);
}

bool melee_attack::defender_is_unholy()
{
    if (defender->atype() == ACT_PLAYER)
        return (player_is_unholy());
    else
        return (mons_is_unholy(defender_as_monster()));
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
                    def_name(DESC_NOCAP_THE).c_str());
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
    if (defender->id() == MONS_BLINK_FROG
        || defender->id() == MONS_PRINCE_RIBBIT)
    {
        if (one_chance_in(5))
        {
            emit_nodmg_hit_message();
            if (defender_visible)
            {
                special_damage_message =
                    make_stringf("%s %s in the translocular energy.",
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
            && !is_fixed_artefact(*weapon))
        {
            // If the player is being sent to the Abyss by being attacked
            // with a distortion weapon, then we have to ID it before
            // the player goes to Abyss, while the weapon object is
            // still in memory.
            if (is_random_artefact(*weapon))
                randart_wpn_learn_prop(*weapon, RAP_BRAND);
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
    CHAOS_PARA,
    CHAOS_PETRIFY,
    NUM_CHAOS_TYPES
};

// XXX: We might want to vary the probabilities for the various effects
// based on whether the source is weapon of chaos or a monster with
// AF_CHAOS.
void melee_attack::chaos_affects_defender()
{
    const bool mon        = defender->atype() == ACT_MONSTER;
    const bool immune     = mon && mons_immune_magic(defender_as_monster());
    const bool is_shifter = mon && mons_is_shapeshifter(defender_as_monster());
    const bool can_clone  = mon && !mons_is_holy(defender_as_monster())
                            && mons_clonable(defender_as_monster(), true);
    const bool can_poly   = is_shifter || (defender->can_safely_mutate()
                                           && !immune);
    const bool can_rage   = defender->can_go_berserk();

    int clone_chance   = can_clone        ?  1 : 0;
    int poly_chance    = can_poly         ?  1 : 0;
    int poly_up_chance = can_poly  && mon ?  1 : 0;
    int shifter_chance = can_poly  && mon ?  1 : 0;
    int rage_chance    = can_rage         ? 10 : 0;
    int miscast_chance = 10;

    if (is_shifter)
        // Already a shifter.
        shifter_chance = 0;

    // A chaos self-attack increased the chance of certain effects,
    // due to a short-circuit/feedback/resonance/whatever.
    if (attacker == defender)
    {
        clone_chance   *= 2;
        poly_chance    *= 2;
        poly_up_chance *= 2;
        shifter_chance *= 2;
        miscast_chance *= 2;

        // Inform player that something is up.
        if (see_grid(defender->pos()))
        {
            if (defender->atype() == ACT_PLAYER)
                mpr("You give off a flash of multicoloured light!");
            else if (you.can_see(defender))
                simple_monster_message(defender_as_monster(),
                                       " gives off a flash of "
                                       "multicoloured light!");
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
        10, // CHAOS_HASTE
        10, // CHAOS_INVIS

        10, // CHAOS_SLOW
        10, // CHAOS_PARA
        10, // CHAOS_PETRIFY
    };

    bolt beam;
    beam.flavour = BEAM_NONE;

    int choice = choose_random_weighted(probs, probs + NUM_CHAOS_TYPES);
    switch (static_cast<chaos_type>(choice))
    {
    case CHAOS_CLONE:
    {
        ASSERT(can_clone && clone_chance > 0);
        ASSERT(defender->atype() == ACT_MONSTER);

        int clone_idx = clone_mons(defender_as_monster(), true,
                                   &obvious_effect);
        if (clone_idx != NON_MONSTER)
        {
            if (obvious_effect)
                special_damage_message =
                    make_stringf("%s is duplicated!",
                                 def_name(DESC_NOCAP_THE).c_str());

            monsters &clone(menv[clone_idx]);
            // The player shouldn't get new permanent followers from cloning.
            if (clone.attitude == ATT_FRIENDLY && !clone.is_summoned())
                clone.mark_summoned(6, true, MON_SUMM_CLONE);
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
        monster_polymorph(defender_as_monster(), RANDOM_MONSTER, PPT_MORE);
        break;

    case CHAOS_MAKE_SHIFTER:
        ASSERT(can_poly && shifter_chance > 0);
        ASSERT(!is_shifter);
        ASSERT(defender->atype() == ACT_MONSTER);

        obvious_effect = you.can_see(defender);
        defender_as_monster()->add_ench(one_chance_in(3) ?
            ENCH_GLOWING_SHAPESHIFTER : ENCH_SHAPESHIFTER);
        // Immediately polymorph monster, just to make the effect obvious.
        monster_polymorph(defender_as_monster(),
                          RANDOM_MONSTER, PPT_SAME, true);
        break;

    case CHAOS_MISCAST:
    {
        int level = defender->get_experience_level();

        // At level == 27 there's a 20.3% chance of a level 3 miscast.
        int level1_chance = level;
        int level2_chance = std::max( 0, level - 7);
        int level3_chance = std::max( 0, level - 15);

        level = random_choose_weighted(
            level1_chance, 1,
            level2_chance, 2,
            level3_chance, 3,
            0);

        miscast_level  = level;
        miscast_type   = SPTYP_RANDOM;
        miscast_target = coinflip() ? attacker : defender;

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

    case CHAOS_PARA:
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
        beam.type         = 0;
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
            : attacker_as_monster()->confused_by_you() ? KILL_YOU_CONF
                                                       : KILL_MON;

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

    if (grid_stair_direction(stair_feat) == CMD_NO_CMD)
        return (false);

    // The player can't use shops to escape, so don't bother.
    if (stair_feat == DNGN_ENTER_SHOP)
        return false;

    // Don't move around notable terrain the player is aware of if it's
    // out of sight.
    if (is_notable_terrain(stair_feat)
        && is_terrain_known(orig_pos.x, orig_pos.y) && !see_grid(orig_pos))
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
        DID_AFFECT();

    // Dump attacker or items under attacker to another level.
    if (is_valid_shaft_level()
        && (attacker->will_trigger_shaft()
            || igrd(attacker->pos()) != NON_ITEM)
        && one_chance_in(1000))
    {
        (void) attacker->do_shaft();
        DID_AFFECT();
    }

    // Make a loud noise.
    if (weapon && player_can_hear(attacker->pos())
        && one_chance_in(1000))
    {
        std::string msg = wep_name(DESC_CAP_YOUR);
        msg += " twangs alarmingly!";

        if (!you.can_see(attacker))
            msg = "You hear a loud twang.";

        noisy(15, attacker->pos(), msg.c_str());
        DID_AFFECT();
    }

    return;
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

        if (!is_valid_item(item) || item.pos != mon->pos())
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
            {
                if (items[i] == si.link())
                    last_item = si.link();
            }
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

        void (animate_remains(mon->pos(), CORPSE_BODY, mon->behaviour,
                              mon->foe, mon->god, true, true, &zombie_index));
    }

    // No equipment to get, or couldn't get it for some reason.
    if (zombie_index == -1)
    {
        monster_type type = (mons_zombie_size(mon->type) == Z_SMALL) ?
                                MONS_ZOMBIE_SMALL : MONS_ZOMBIE_LARGE;
        zombie_index = create_monster(
                           mgen_data(type, mon->behaviour, 0, 0, mon->pos(),
                                     mon->foe, MG_FORCE_PLACE, mon->god,
                                     (monster_type) mon->type, mon->number));
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
            simple_monster_message(mon, "'s equipment vanishes!");
    }
    else
        simple_monster_message(zombie, " appears from thin air!");

    return (true);
}

// mon is a copy of the monster from before monster_die() was called,
// though its hitpoints may be non-positive.
//
// NOTE: Isn't called if monster dies from poisoning caused by chaos.
void melee_attack::chaos_killed_defender(monsters* mon)
{
    ASSERT(mon->type != -1 && mon->type != MONS_PROGRAM_BUG);
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

    if (one_chance_in(100) &&
        _make_zombie(mon, corpse_class, corpse_index, fake_corpse, last_item))
    {
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
    return (random_choose_weighted(
        15, SPWPN_NORMAL,
        10, SPWPN_FLAMING,
        10, SPWPN_FREEZING,
        10, SPWPN_ELECTROCUTION,
        10, SPWPN_VENOM,
        10, SPWPN_CHAOS,
         5, SPWPN_VORPAL,
         5, SPWPN_DRAINING,
         5, SPWPN_VAMPIRICISM,
         2, SPWPN_CONFUSE,
         2, SPWPN_DISTORTION,
         0));
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
        if (is_random_artefact(*weapon))
            brand_was_known = randart_known_wpn_property(*weapon, RAP_BRAND);
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
        res = fire_res_apply_cerebov_downgrade( defender->res_fire() );
        calc_elemental_brand_damage( BEAM_FIRE, res,
                                     defender->is_icy()? "melt" : "burn");
        defender->expose_to_element(BEAM_FIRE);
        break;

    case SPWPN_FREEZING:
        calc_elemental_brand_damage(BEAM_COLD, defender->res_cold(), "freeze");
        defender->expose_to_element(BEAM_COLD);
        break;

    case SPWPN_HOLY_WRATH:
        if (defender_is_unholy())
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
        if (defender->airborne())
            break;
        else if (defender->res_elec() > 0)
            break;
        else if (one_chance_in(3))
        {
            special_damage_message =
                defender->atype() == ACT_PLAYER?
                   "You are electrocuted!"
                :  "There is a sudden explosion of sparks!";
            special_damage = 10 + random2(15);
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
    case SPWPN_STAFF_OF_OLGREB:
        if (!one_chance_in(4))
        {
            int old_poison;

            if (defender->atype() == ACT_PLAYER)
                old_poison = you.duration[DUR_POISONING];
            else
                old_poison =
                    (defender_as_monster()->get_ench(ENCH_POISON)).degree;

            // Poison monster message needs to arrive after hit message.
            emit_nodmg_hit_message();

            // Weapons of venom do two levels of poisoning to the player,
            // but only one level to monsters.
            defender->poison(attacker, 2);

            if (defender->atype() == ACT_PLAYER
                   && old_poison < you.duration[DUR_POISONING]
                || defender->atype() != ACT_PLAYER
                   && old_poison <
                      (defender_as_monster()->get_ench(ENCH_POISON)).degree)
            {
                obvious_effect = true;
            }

        }
        break;

    case SPWPN_DRAINING:
        drain_defender();
        break;

    case SPWPN_VORPAL:
        special_damage = 1 + random2(damage_done) / 2;
        // Note: Leaving special_damage_message empty because there isn't one.
        break;

    case SPWPN_VAMPIRICISM:
    {
        // Vampire bat form -- why the special handling?
        if (attacker->atype() == ACT_PLAYER && you.species == SP_VAMPIRE
            && you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT)
        {
            _player_vampire_draws_blood(defender_as_monster(), damage_done);
            break;
        }

        if (x_chance_in_y(defender->res_negative_energy(), 3))
            break;

        if (defender->holiness() != MH_NATURAL || !weapon
            || damage_done < 1 || attacker->stat_hp() == attacker->stat_maxhp()
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
        if (weapon && is_fixed_artefact(*weapon)
            && weapon->special == SPWPN_VAMPIRES_TOOTH)
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
            beam_temp.apply_enchantment_to_monster(defender_as_monster());
            obvious_effect = beam_temp.obvious_effect;
        }

        if (attacker->atype() == ACT_PLAYER && damage_brand == SPWPN_CONFUSE)
        {
            ASSERT(you.duration[DUR_CONFUSING_TOUCH]);
            you.duration[DUR_CONFUSING_TOUCH] -= roll_dice(3, 5);

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
        // If your god objects to using chaos then it makes the
        // brand obvious.
        if (did_god_conduct(DID_CHAOS, 2 + random2(3), brand_was_known))
            obvious_effect = true;
    }
    if (!obvious_effect)
        obvious_effect = !special_damage_message.empty();

    if (obvious_effect && attacker_visible && weapon != NULL
        && !is_unrandom_artefact(*weapon) && !is_fixed_artefact(*weapon))
    {
        if (is_random_artefact(*weapon))
            randart_wpn_learn_prop(*weapon, RAP_BRAND);
        else
            set_ident_flags(*weapon, ISFLAG_KNOW_TYPE);
    }

    return (ret);
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

        if (defender_as_monster()->number == 1) // will be zero afterwards
        {
            if (defender_visible)
            {
                mprf( "%s %s %s's last head off!",
                      atk_name(DESC_CAP_THE).c_str(),
                      attacker->conj_verb(verb).c_str(),
                      def_name(DESC_NOCAP_THE).c_str() );
            }
            defender_as_monster()->number--;

            if (!defender->is_summoned())
                bleed_onto_floor(defender->pos(), defender->id(),
                                 defender_as_monster()->hit_points, true);

            defender->hurt(attacker, defender_as_monster()->hit_points);

            return (true);
        }
        else
        {
            if (defender_visible)
            {
                mprf( "%s %s one of %s's heads off!",
                      atk_name(DESC_CAP_THE).c_str(),
                      attacker->conj_verb(verb).c_str(),
                      def_name(DESC_NOCAP_THE).c_str() );
            }
            defender_as_monster()->number--;

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
                else if (defender_as_monster()->number < limit - 1)
                {
                    simple_monster_message(defender_as_monster(),
                                           " grows two more!" );
                    defender_as_monster()->number += 2;
                    heal_monster(defender_as_monster(), 8 + random2(8), true);
                }
            }
        }
    }

    return (false);
}

bool melee_attack::decapitate_hydra(int dam, int damage_type)
{
    if (defender->atype() == ACT_MONSTER
        && defender_as_monster()->has_hydra_multi_attack()
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
    return roll_dice(3,
                     1 + (you.skills[skill] + you.skills[SK_EVOCATIONS]) / 2);
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

    const int staff_cost = 2;
    if (you.magic_points < staff_cost
        || random2(15) > you.skills[SK_EVOCATIONS])
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

        if (special_damage)
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
            poison_monster(defender_as_monster(), KC_YOU);
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
        mpr("You're wielding some staff I've never heard of! (fight.cc)");
        break;
    } // end switch

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
#if DEBUG_DIAGNOSTICS
        // note: doesn't take account of special weapons, etc.
        mprf( MSGCH_DIAGNOSTICS, "Hit for %d.", damage_done );
#endif

        player_monattk_hit_effects(true);

        _monster_die(defender_as_monster(), KILL_YOU, NON_MONSTER);

        return (true);
    }

    return (false);
}

void melee_attack::player_calc_hit_damage()
{
    potential_damage = player_stat_modify_damage(potential_damage);

    if (water_attack)
        potential_damage = player_apply_water_attack_bonus(potential_damage);

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

int melee_attack::player_to_hit(bool random_factor)
{
    heavy_armour_penalty = calc_heavy_armour_penalty(random_factor);
    can_do_unarmed = player_fights_well_unarmed(heavy_armour_penalty);

    hand_half_bonus = unarmed_ok
                      && !can_do_unarmed
                      && !shield
                      && weapon
                      && !item_cursed( *weapon )
                      && hands == HANDS_HALF;

    int your_to_hit = 15 + (calc_stat_to_hit_base() / 2);

#ifdef DEBUG_DIAGNOSTICS
    const int base_to_hit = your_to_hit;
#endif

    if (water_attack)
        your_to_hit += 5;

    if (wearing_amulet(AMU_INACCURACY))
        your_to_hit -= 5;

    const bool see_invis = player_see_invis();
    // if you can't see yourself, you're a little less accurate.
    if (you.invisible() && !see_invis)
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
        your_to_hit +=
            (you.species == SP_TROLL || you.species == SP_GHOUL) ? 4 : 2;

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
        else if (item_is_staff( *weapon ))
        {
            // magical staff
            your_to_hit += property( *weapon, PWPN_HIT );
        }
    }

    // slaying bonus
    your_to_hit += slaying_bonus(PWPN_HIT);

    // hunger penalty
    if (you.hunger_state == HS_STARVING)
        your_to_hit -= 3;

    // armour penalty
    your_to_hit -= heavy_armour_penalty;

#if DEBUG_DIAGNOSTICS
    int roll_hit = your_to_hit;
#endif

    // hit roll
    your_to_hit = maybe_random2(your_to_hit, random_factor);

#if DEBUG_DIAGNOSTICS
    mprf( MSGCH_DIAGNOSTICS,
          "to hit die: %d; rolled value: %d; base: %d",
          roll_hit, your_to_hit, base_to_hit );
#endif

    if (hand_half_bonus)
        your_to_hit += maybe_random2(3, random_factor);

    if (weapon && wpn_skill == SK_SHORT_BLADES && you.duration[DUR_SURE_BLADE])
    {
        your_to_hit += 5 +
            (random_factor ? random2limit( you.duration[DUR_SURE_BLADE], 10 ) :
             you.duration[DUR_SURE_BLADE] / 2);
    }

    // other stuff
    if (!weapon)
    {
        if (you.duration[DUR_CONFUSING_TOUCH])
        {
            // Just trying to touch is easier that trying to damage.
            your_to_hit += maybe_random2(you.dex, random_factor);
        }

        switch ( you.attribute[ATTR_TRANSFORMATION] )
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
        case TRAN_SERPENT_OF_HELL:
        case TRAN_DRAGON:
            your_to_hit += maybe_random2(10, random_factor);
            break;
        case TRAN_LICH:
            your_to_hit += maybe_random2(10, random_factor);
            break;
        case TRAN_AIR:
            your_to_hit = 0;
            break;
        case TRAN_NONE:
        default:
            break;
        }
    }

    // Check for backlight (Corona).
    if (defender && defender->atype() == ACT_MONSTER)
    {
        if (defender->backlit())
            your_to_hit += 2 + random2(8);
        // Invisible monsters are hard to hit.
        else if (defender->invisible() && !see_invis)
            your_to_hit -= 6;
    }

    return (your_to_hit);
}

void melee_attack::player_stab_check()
{
    unchivalric_attack_type unchivalric = is_unchivalric_attack(&you, defender);

    bool roll_needed = true;
    int roll = 155;

    // This ordering is important!
    switch (unchivalric)
    {
    default:
    case UCAT_NO_ATTACK:
        stab_attempt = false;
        stab_bonus   = 0;
        break;

    case UCAT_DISTRACTED:
        stab_attempt = true;
        stab_bonus   = 3;
        break;

    case UCAT_CONFUSED:
    case UCAT_FLEEING:
        stab_attempt = true;
        stab_bonus   = 2;
        break;

    case UCAT_INVISIBLE:
        stab_attempt = true;
        stab_bonus   = 2;
        if (!mons_sense_invis(defender_as_monster()))
            roll -= 15;
        break;

    case UCAT_HELD_IN_NET:
    case UCAT_PETRIFYING:
    case UCAT_PARALYSED:
        stab_attempt = true;
        stab_bonus   = 1;
        break;

    case UCAT_SLEEPING:
        stab_attempt = true;
        roll_needed = false;
        stab_bonus  = 1;
        break;
    }

    // See if we need to roll against dexterity / stabbing.
    if (stab_attempt && roll_needed)
    {
        stab_attempt = x_chance_in_y(you.skills[SK_STABBING] + you.dex + 1,
                                     roll);
    }
}

void melee_attack::player_apply_attack_delay()
{
    int attack_delay = weapon ? player_weapon_speed() : player_unarmed_speed();
    attack_delay = player_apply_shield_delay(attack_delay);

    if (attack_delay < 3)
        attack_delay = 3;

    final_attack_delay = attack_delay;

    you.time_taken =
        std::max(2, div_rand_round(you.time_taken * final_attack_delay, 10));

#if DEBUG_DIAGNOSTICS
    mprf( MSGCH_DIAGNOSTICS,
          "Weapon speed: %d; min: %d; attack time: %d",
          final_attack_delay, min_delay, you.time_taken );
#endif
}

int melee_attack::player_weapon_speed()
{
    int attack_delay = 0;

    if (weapon && (weapon->base_type == OBJ_WEAPONS
                   || item_is_staff( *weapon )))
    {
        attack_delay = property( *weapon, PWPN_SPEED );
        attack_delay -= you.skills[ wpn_skill ] / 2;

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

        // Hand and a half bonus only helps speed up to a point, any more
        // than speed 10 must come from skill and the weapon
        if (hand_half_bonus && attack_delay > 10)
            attack_delay--;

        // apply minimum to weapon skill modification
        if (attack_delay < min_delay)
            attack_delay = min_delay;

        if (weapon->base_type == OBJ_WEAPONS
            && damage_brand == SPWPN_SPEED)
        {
            attack_delay = (attack_delay + 1) / 2;
        }
    }

    return (attack_delay);
}

int melee_attack::player_unarmed_speed()
{
    int unarmed_delay = 10;

    // Not even bats can attack faster than this.
    min_delay = 5;

    // Unarmed speed.
    if (you.burden_state == BS_UNENCUMBERED
        && one_chance_in(heavy_armour_penalty + 1))
    {
        const bool is_bat = (you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT);
        unarmed_delay =
            std::max(10 - you.skills[SK_UNARMED_COMBAT] / (is_bat ? 3 : 5),
                     min_delay);
    }

    return (unarmed_delay);
}

int melee_attack::player_apply_shield_delay(int attack_delay)
{
    if (shield)
    {
        switch (shield->sub_type)
        {
        case ARM_LARGE_SHIELD:
            if (you.skills[SK_SHIELDS] <= 10 + random2(17))
                attack_delay++;
            // [dshaligram] Fall-through

        case ARM_SHIELD:
            if (you.skills[SK_SHIELDS] <= 3 + random2(17))
                attack_delay++;
            break;
        }
    }

    return (attack_delay);
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
            damage = 12 + (you.strength / 4) + (you.dex / 4);
            break;
        case TRAN_STATUE:
            damage = 12 + you.strength;
            break;
        case TRAN_SERPENT_OF_HELL:
        case TRAN_DRAGON:
            damage = 20 + you.strength;
            break;
        case TRAN_LICH:
            damage = 5;
            break;
        case TRAN_AIR:
            damage = 0;
            break;
        }
    }
    else if (you.equip[ EQ_GLOVES ] == -1)
    {
        // Claw damage only applies for bare hands.
        if (you.species == SP_TROLL)
            damage += 5;
        else if (you.species == SP_GHOUL)
            damage += 2;

        damage += player_mutation_level(MUT_CLAWS) * 2;
    }

    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT)
    {
        // Bats really don't do a lot of damage.
        damage += you.skills[SK_UNARMED_COMBAT]/5;
    }
    else
        damage += you.skills[SK_UNARMED_COMBAT];

    return (damage);
}

int melee_attack::player_calc_base_weapon_damage()
{
    int damage = 0;

    if (weapon->base_type == OBJ_WEAPONS || item_is_staff( *weapon ))
        damage = property( *weapon, PWPN_DAMAGE );

    // Staves can be wielded with a worn shield, but are much less
    // effective.
    if (shield && weapon->base_type == OBJ_WEAPONS
        && weapon_skill(*weapon) == SK_STAVES
        && hands_reqd(*weapon, player_size()) == HANDS_HALF)
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
        if (mons_friendly(attacker_as_monster()))
        {
            if (you.pet_target == MHITYOU || you.pet_target == MHITNOT)
            {
                if (attacker->confused() && you.can_see(attacker))
                {
                    mpr("Zin prevents your ally from violating sanctuary "
                        "in its confusion.", MSGCH_GOD);
                }
                else if (attacker_as_monster()->has_ench(ENCH_BERSERK)
                         && you.can_see(attacker))
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
#ifdef DEBUG_DIAGNOSTICS
            mpr("Preventing hostile violation of sanctuary.",
                MSGCH_DIAGNOSTICS);
#endif
            cancel_attack = true;
            return (false);
        }
    }

    mons_perform_attack();

    if (perceived_attack
        && (defender_as_monster()->foe == MHITNOT || one_chance_in(3))
        && attacker->alive() && defender->alive())
    {
        behaviour_event(defender_as_monster(), ME_WHACK, attacker->mindex());
    }

    // If an enemy attacked a friend, set the pet target if it isn't set
    // already, but not if sanctuary is in effect (pet target must be
    // set explicitly by the player during sanctuary).
    if (perceived_attack && attacker->alive()
        && mons_friendly(defender_as_monster())
        && !mons_wont_attack(attacker_as_monster())
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
        || attacker->id() == MONS_BALL_LIGHTNING)
    {
        attacker_as_monster()->hit_points = -1;
        // Do the explosion right now.
        monster_die(attacker_as_monster(), KILL_MON, attacker->mindex());
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
        && !check_mons_resist_magic(attacker_as_monster(), warding))
    {
        if (needs_message)
        {
            mprf("%s tries to attack %s, but flinches away.",
                 atk_name(DESC_CAP_THE).c_str(),
                 def_name(DESC_NOCAP_THE).c_str());
        }
        return (true);
    }

    return (false);
}

int melee_attack::mons_attk_delay()
{
    return (weapon? property(*weapon, PWPN_SPEED) : 0);
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

    if (attacker->invisible() && !defender->can_see_invisible())
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

        defender->shield_block_succeeded();

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

    // Berserk monsters get bonus damage.
    if (attacker_as_monster()->has_ench(ENCH_BERSERK))
        damage = damage * 3 / 2;
    else if (attacker_as_monster()->has_ench(ENCH_BATTLE_FRENZY))
    {
        const mon_enchant ench =
            attacker_as_monster()->get_ench(ENCH_BATTLE_FRENZY);

#ifdef DEBUG_DIAGNOSTICS
        const int orig_damage = damage;
#endif

        damage = damage * (115 + ench.degree * 15) / 100;

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "%s frenzy damage: %d->%d",
             attacker->name(DESC_PLAIN).c_str(), orig_damage, damage);
#endif
    }

    if (water_attack)
        damage *= 2;

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
    int ac = defender->armour_class();
    if (ac > 0)
    {
        int damage_reduction = random2(ac + 1);
        if (!defender->wearing_light_armour())
        {
            if (const item_def *arm = defender->slot_item(EQ_BODY_ARMOUR))
            {
                const int armac = property(*arm, PARM_AC);
                int perc = 2 * (defender->skill(SK_ARMOUR) + armac);
                if (perc > 50)
                    perc = 50;

                int min = 1 + (damage_max * perc) / 100;
                if (min > ac / 2)
                    min = ac / 2;

                if (damage_reduction < min)
                    damage_reduction = min;
            }
        }
        damage -= damage_reduction;
    }

    if (damage < 1)
        damage = 0;

    return damage;
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
    {
        const int num_attacks = sizeof(klown_attack) / sizeof(klown_attack[0]);
        return klown_attack[random2(num_attacks)];
    }

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
        "punch",
        "kick",
        "tail-slap",
        "gore"
    };

    return attack_types[attk.type];
}

std::string melee_attack::mons_weapon_desc()
{
    if (!you.can_see(attacker))
        return ("");

    if (weapon)
    {
        std::string result = "";
        const item_def wpn = *weapon;
        if (get_weapon_brand(wpn) == SPWPN_REACHING)
        {
            if ( grid_distance(attacker->pos(), defender->pos()) == 2 )
                result += " from afar";
        }

        if (attacker->id() != MONS_DANCING_WEAPON)
        {
            result += " with ";
            result += weapon->name(DESC_NOCAP_A);
        }

        return result;
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
    if (water_attack && attacker_visible && attacker != defender)
    {
        mprf("%s uses the watery terrain to its advantage.",
             attacker->name(DESC_CAP_THE).c_str());
    }

    if (needs_message)
    {
        mprf("%s %s %s%s%s%s",
             atk_name(DESC_CAP_THE).c_str(),
             attacker->conj_verb( mons_attack_verb(attk) ).c_str(),
             mons_defender_name().c_str(),
             debug_damage_number().c_str(),
             mons_weapon_desc().c_str(),
             attack_strength_punctuation().c_str());
    }
}

void melee_attack::mons_announce_dud_hit(const mon_attack_def &attk)
{
    if (needs_message)
    {
        mprf("%s %s %s but doesn't do any damage.",
             atk_name(DESC_CAP_THE).c_str(),
             attacker->conj_verb( mons_attack_verb(attk) ).c_str(),
             mons_defender_name().c_str());
    }
}

void melee_attack::check_defender_train_dodging()
{
    // It's possible to train both dodging and armour under the new scheme.
    if (defender->wearing_light_armour(true)
        && attacker_visible
        && one_chance_in(3))
    {
        perceived_attack = true;
        defender->exercise(SK_DODGING, 1);
    }
}

void melee_attack::check_defender_train_armour()
{
    if (defender->wearing_light_armour())
        return;

    const item_def *arm = defender->slot_item(EQ_BODY_ARMOUR);
    if (arm && coinflip() && x_chance_in_y(item_mass(*arm) + 1, 1000))
        defender->exercise(SK_ARMOUR, coinflip()? 2 : 1);
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
                     def_name(DESC_NOCAP_THE).c_str());
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
    if (defender->res_sticky_flame() > 0)
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
            napalm_monster(defender_as_monster(),
                           mons_friendly_real(attacker_as_monster()) ?
                           KC_FRIENDLY : KC_OTHER,
                           std::min(4, 1 + random2(attacker->get_experience_level())/2));
        }
    }
}

void melee_attack::wasp_paralyse_defender()
{
    // [dshaligram] Adopted 4.1.2's wasp mechanics, in slightly modified form.
    if (attacker->id() == MONS_RED_WASP || one_chance_in(3))
        defender->poison( attacker, coinflip()? 2 : 1 );

    int paralyse_roll = (damage_done > 4? 3 : 20);
    if (attacker->id() == MONS_YELLOW_WASP)
        paralyse_roll += 3;

    if (defender->res_poison() <= 0)
    {
        if (one_chance_in(paralyse_roll))
            defender->paralyse( attacker, roll_dice(1, 3) );
        else
            defender->slow_down( attacker, roll_dice(1, 3) );
    }
}

void melee_attack::splash_monster_with_acid(int strength)
{
    special_damage += roll_dice(2, 4);
    if (defender_visible)
        mprf("%s is splashed with acid.", defender->name(DESC_CAP_THE).c_str());
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
        if (defender->res_poison() <= 0)
        {
            defender->poison(attacker, roll_dice(1, 3));
            if (one_chance_in(4))
                defender->drain_stat(STAT_STRENGTH, 1, attacker);
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
            attacker_as_monster()->hit_points = -10;

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
            mprf("%s %s %s!",
                 atk_name(DESC_CAP_THE).c_str(),
                 attacker->conj_verb("freeze").c_str(),
                 def_name(DESC_NOCAP_THE).c_str());
        }
        break;

    case AF_ELEC:
        special_damage =
            resist_adjust_damage(
                defender,
                BEAM_ELECTRICITY,
                defender->res_elec(),
                attacker->get_experience_level() +
                random2(attacker->get_experience_level() / 2));

        if (defender->airborne())
            special_damage = special_damage * 2 / 3;

        if (needs_message && special_damage)
        {
            mprf("%s %s %s%s",
                 atk_name(DESC_CAP_THE).c_str(),
                 attacker->conj_verb("shock").c_str(),
                 def_name(DESC_NOCAP_THE).c_str(),
                 special_attack_punctuation().c_str());
        }

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Shock damage: %d", special_damage);
#endif
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
        if ((one_chance_in(20) || damage_done > 0 && one_chance_in(3))
            && defender->res_negative_energy() < random2(4))
        {
            defender->drain_stat(STAT_STRENGTH, 1, attacker);
        }
        break;

    case AF_DRAIN_DEX:
        if ((one_chance_in(20) || (damage_done > 0 && one_chance_in(3)))
            && defender->res_negative_energy() < random2(4))
        {
            defender->drain_stat(STAT_DEXTERITY, 1, attacker);
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

            if (--(attacker_as_monster()->hit_dice) <= 0)
                attacker_as_monster()->hit_points = -1;

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
                 def_name(DESC_NOCAP_THE).c_str());
        }

        defender->go_berserk(false);
        break;

    case AF_NAPALM:
        mons_do_napalm();
        break;

    case AF_CHAOS:
        chaos_affects_defender();
        break;

    case AF_STEAL_FOOD:
        // Monsters don't carry food.
        if (defender->atype() != ACT_PLAYER)
            break;

        // Only use this attack sometimes.
        if (!one_chance_in(3))
            break;

        if (expose_player_to_element(BEAM_STEAL_FOOD, 10) && needs_message)
        {
            mprf("%s steals some of your food!",
                 atk_name(DESC_CAP_THE).c_str());
        }
        break;
    }
}

void melee_attack::mons_perform_attack_rounds()
{
    const int nrounds = attacker_as_monster()->has_hydra_multi_attack() ?
        attacker_as_monster()->number : 4;
    const coord_def pos    = defender->pos();
    const bool was_delayed = you_are_delayed();

    // Melee combat, tell attacker to wield its melee weapon.
    attacker_as_monster()->wield_melee_weapon();

    monsters* def_copy = NULL;
    for (attack_number = 0; attack_number < nrounds; ++attack_number)
    {
        // Monster went away?
        if (!defender->alive() || defender->pos() != pos)
            break;

        // Monsters hitting themselves get just one round.
        if (attack_number > 0 && attacker == defender)
            break;

        init_attack();

        const mon_attack_def attk = mons_attack_spec(attacker_as_monster(),
                                                     attack_number);
        if (attk.type == AT_NONE)
        {
            // Make sure the monster uses up some energy, even
            // though it didn't actually attack.
            if (attack_number == 0)
                attacker_as_monster()->lose_energy(EUT_ATTACK);

            break;
        }

        if (attk.type != AT_HIT && !unarmed_ok)
            continue;

        if (attk.type == AT_SHOOT)
            continue;

        damage_done = 0;
        mons_set_weapon(attk);
        to_hit = mons_to_hit();

        const bool chaos_attack = damage_brand == SPWPN_CHAOS
                                  || (attk.flavour == AF_CHAOS
                                      && attacker != defender);

        // Make copy of monster before monster_die() resets it.
        if (chaos_attack && defender->atype() == ACT_MONSTER && !def_copy)
            def_copy = new monsters(*defender_as_monster());

        final_attack_delay = mons_attk_delay();
        if (damage_brand == SPWPN_SPEED)
            final_attack_delay = final_attack_delay / 2 + 1;

        mons_lose_attack_energy(attacker_as_monster(),
                                final_attack_delay, attack_number);

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
            if (attacker == defender
                || test_melee_hit(to_hit, defender_evasion))
            {
                this_round_hit = did_hit = true;
                perceived_attack = true;
                damage_done = mons_calc_damage(attk);
            }
            else
            {
                this_round_hit = false;
                perceived_attack = perceived_attack || attacker_visible;

                if (needs_message)
                {
                    mprf("%s misses %s.",
                         atk_name(DESC_CAP_THE).c_str(),
                         mons_defender_name().c_str());
                }
            }
        }

        if (damage_done < 1 && this_round_hit && !shield_blocked)
            mons_announce_dud_hit(attk);

        if (damage_done > 0)
        {
#ifdef DEBUG_DIAGNOSTICS
            if (shield_blocked)
                mpr("ERROR: Non-zero damage after shield block!");
#endif
            mons_announce_hit(attk);
            check_defender_train_armour();

            const int type = defender->id();

            if (defender->can_bleed()
                && !defender->is_summoned()
                && !defender->submerged())
            {
                int blood = _modify_blood_amount(damage_done,
                                                 attacker->damage_type());

                if (blood > defender->stat_hp())
                    blood = defender->stat_hp();

                bleed_onto_floor(pos, type, blood, true);
            }
            if (decapitate_hydra(damage_done,
                                 attacker->damage_type(attack_number)))
            {
                continue;
            }

            special_damage = 0;
            special_damage_message.clear();

            // Monsters attacking themselves don't get attack flavour.
            // The message sequences look too weird.
            if (attacker != defender)
                mons_apply_attack_flavour(attk);

            if (!special_damage_message.empty())
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

            defender->hurt(attacker, damage_done + special_damage);

            if (!defender->alive())
            {
                if (chaos_attack && defender->atype() == ACT_MONSTER)
                    chaos_killed_defender(def_copy);

                if (chaos_attack && attacker->alive())
                    chaos_affects_attacker();

                do_miscast();
                break;
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
            apply_damage_brand();

            if (!special_damage_message.empty())
            {
                mprf("%s", special_damage_message.c_str());
                // Don't do message-only miscasts along with a special
                // damage message.
                if (miscast_level == 0)
                    miscast_level = -1;
            }

            if (special_damage > 0)
                defender->hurt(attacker, special_damage);

            if (!defender->alive())
            {
                if (chaos_attack && defender->atype() == ACT_MONSTER)
                    chaos_killed_defender(def_copy);

                if (chaos_attack && attacker->alive())
                    chaos_affects_attacker();

                do_miscast();
                break;
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
        }

        item_def *weap = attacker_as_monster()->mslot_item(MSLOT_WEAPON);
        if (weap && you.can_see(attacker) && weap->cursed()
            && is_range_weapon(*weap))
        {
            set_ident_flags(*weap, ISFLAG_KNOW_CURSE);
        }
    }

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

    if (defender->atype() == ACT_PLAYER)
    {
        interrupt_activity(AI_MONSTER_ATTACKS, attacker_as_monster());

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

    if (water_attack)
        mhit += 5;

    if (weapon && weapon->base_type == OBJ_WEAPONS)
        mhit += weapon->plus + property(*weapon, PWPN_HIT);

    if (attacker->confused())
        mhit -= 5;

    if (defender->backlit())
        mhit += 2 + random2(8);

    // Invisible defender is hard to hit if you can't see invis. Note
    // that this applies only to monsters vs monster and monster vs
    // player. Does not apply to a player fighting an invisible
    // monster.
    if (defender->invisible() && !attacker->can_see_invisible())
        mhit = mhit * 65 / 100;

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "%s: Base to-hit: %d, Final to-hit: %d",
         attacker->name(DESC_PLAIN).c_str(),
         base_hit, mhit);
#endif

    return (mhit);
}

///////////////////////////////////////////////////////////////////////////

static bool wielded_weapon_check(const item_def *weapon)
{
    bool result = true;
    if (!you.received_weapon_warning && !you.confused()
        && (weapon && weapon->base_type != OBJ_STAVES
               && (weapon->base_type != OBJ_WEAPONS || is_range_weapon(*weapon))
            || you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED]
               && you_tran_can_wear(EQ_WEAPON)))
    {
        std::string prompt  = "Really attack while ";
        if (!weapon)
            prompt += "being unarmed?";
        else
            prompt += "wielding " + weapon->name(DESC_NOCAP_YOUR) + "? ";

        result = yesno(prompt.c_str(), true, 'n');

        learned_something_new(TUT_WIELD_WEAPON); // for tutorial Rangers

        // Don't warn again if you decide to continue your attack.
        if (result)
            you.received_weapon_warning = true;
    }
    return (result);
}

// Returns true if you hit the monster.
bool you_attack(int monster_attacked, bool unarmed_attacks)
{
    ASSERT(!crawl_state.arena);

    monsters *defender = &menv[monster_attacked];

    melee_attack attk(&you, defender, unarmed_attacks);

    // We're trying to hit a monster, break out of travel/explore now.
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

// Lose attack energy for attacking with a weapon. The monster has already lost
// the base attack cost by this point.
static void mons_lose_attack_energy(monsters *attacker, int wpn_speed,
                                    int which_attack)
{
    // Initial attack causes energy to be used for all attacks.  No
    // additional energy is used for unarmed attacks.
    if (which_attack == 0)
        attacker->lose_energy(EUT_ATTACK);

    // Monsters lose additional energy only for the first two weapon
    // attacks; subsequent hits are free.
    if (which_attack > 1)
        return;

    // speed adjustment for weapon using monsters
    if (wpn_speed > 0)
    {
        const int atk_speed = attacker->action_energy(EUT_ATTACK);
        // only get one third penalty/bonus for second weapons.
        if (which_attack > 0)
            wpn_speed = div_rand_round( (2 * atk_speed + wpn_speed), 3 );

        int delta = div_rand_round( (wpn_speed - 10 + (atk_speed - 10)), 2 );
        if (delta > 0)
            attacker->speed_increment -= delta;
    }
}

// A monster attacking the player.
bool monster_attack(int monster_attacking, bool allow_unarmed)
{
    ASSERT(!crawl_state.arena);

    monsters *attacker = &menv[monster_attacking];

    // Friendly and good neutral monsters won't attack unless confused.
    if (mons_wont_attack(attacker) && !mons_is_confused(attacker))
        return (false);

    // In case the monster hasn't noticed you, bumping into it will
    // change that.
    behaviour_event( attacker, ME_ALERT, MHITYOU );
    melee_attack attk(attacker, &you, allow_unarmed);
    attk.attack();

    return (true);
}

// Two monsters fighting each other.
bool monsters_fight(int monster_attacking, int monster_attacked,
                    bool allow_unarmed)
{
    monsters *attacker = &menv[monster_attacking];
    monsters *defender = &menv[monster_attacked];

    melee_attack attk(attacker, defender, allow_unarmed);
    return attk.attack();
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
    const int hands      = hands_reqd( wpn_class, wpn_type, player_size() );

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

    // unarmed, weighted slightly towards dex -- would have been more,
    // but then we'd be punishing Trolls and Ghouls who are strong and
    // get special unarmed bonuses.
    if (!weapon)
        return (4);

    int ret = weapon_str_weight(weapon->base_type, weapon->sub_type);

    if (hands_reqd(*weapon, player_size()) == HANDS_HALF && !you.shield())
        ret += 1;

    return (ret);
}

// weapon_dex_weight() + weapon_str_weight == 10, so we only need to
// define one of these.
static inline int player_weapon_dex_weight( void )
{
    return (10 - player_weapon_str_weight());
}

static inline int calc_stat_to_hit_base( void )
{
#ifdef USE_NEW_COMBAT_STATS

    // towards_str_avg is a variable, whose sign points towards strength,
    // and the magnitude is half the difference (thus, when added directly
    // to you.dex it gives the average of the two.
    const signed int towards_str_avg = (you.strength - you.dex) / 2;

    // dex is modified by strength towards the average, by the
    // weighted amount weapon_str_weight() / 10.
    return (you.dex + towards_str_avg * player_weapon_str_weight() / 10);

#else
    return (you.dex);
#endif
}


static inline int calc_stat_to_dam_base( void )
{
#ifdef USE_NEW_COMBAT_STATS

    const signed int towards_dex_avg = (you.dex - you.strength) / 2;
    return (you.strength + towards_dex_avg * player_weapon_dex_weight() / 10);

#else
    return (you.strength);
#endif
}

static void stab_message(actor *defender, int stab_bonus)
{
    switch(stab_bonus)
    {
    case 3:     // big melee, monster surrounded/not paying attention
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
    case 2:     // confused/fleeing
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
    case 1:
        mprf( "%s fails to defend %s.",
              defender->name(DESC_CAP_THE).c_str(),
              defender->pronoun(PRONOUN_REFLEXIVE).c_str() );
        break;
    }
}
