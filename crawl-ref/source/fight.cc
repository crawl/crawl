/**
 * @file
 * @brief functions used during combat
 */

#include "AppHdr.h"

#include "fight.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "areas.h" // silenced
#include "art-enum.h"
#include "coord.h"
#include "coordit.h"
#include "delay.h"
#include "english.h"
#include "env.h"
#include "fineff.h"
#include "fprop.h"
#include "god-passive.h" // passive_t::shadow_attacks
#include "hints.h"
#include "invent.h"
#include "item-prop.h"
#include "item-use.h"
#include "losglobal.h"
#include "melee-attack.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-place.h"
#include "mon-util.h"
#include "options.h"
#include "ouch.h"
#include "player.h"
#include "prompt.h"
#include "quiver.h"
#include "random-var.h"
#include "religion.h"
#include "shopping.h"
#include "spl-damage.h" // safe_discharge
#include "spl-summoning.h"
#include "state.h"
#include "stringutil.h"
#include "target.h"
#include "terrain.h"
#include "throw.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"

/**
 * What are the odds of an HD-checking confusion effect (e.g. Confusing Touch,
 * Fungus Form, SPWPN_CHAOS maybe) to confuse a monster of the given HD?
 *
 * @param HD    The current hit dice (level) of the monster to confuse.
 * @return      A percentage chance (0-100) of confusing that monster.
 *              (Except it tops out at 80%.)
 */
int melee_confuse_chance(int HD)
{
    return max(80 * (24 - HD) / 24, 0);
}

/**
 * What is the player's to-hit for aux attacks, before randomization?
 */
int aux_to_hit()
{
    int to_hit = 1300
                + you.dex() * 75
                + you.skill(SK_FIGHTING, 30);
    to_hit /= 100;

    if (you.get_mutation_level(MUT_EYEBALLS))
        to_hit += 2 * you.get_mutation_level(MUT_EYEBALLS) + 1;

    if (you.duration[DUR_VERTIGO])
        to_hit -= 5;

    to_hit += slaying_bonus();

    return to_hit;

}

static double _to_hit_hit_chance(const monster_info& mi, attack &atk, bool melee,
                                 int to_land, bool is_aux = false)
{
    if (to_land >= AUTOMATIC_HIT)
        return 1.0;

    const double AUTO_MISS_CHANCE = is_aux ? 0 : 2.5;
    const double AUTO_HIT_CHANCE = is_aux ? 3.3333 : 2.5;

    int ev = mi.ev + (!melee && mi.is(MB_REPEL_MSL) ? REPEL_MISSILES_EV_BONUS : 0);

    if (ev <= 0)
        return 1 - AUTO_MISS_CHANCE / 200.0;

    int hits = 0;
    for (int rolled_mhit = 0; rolled_mhit < to_land; rolled_mhit++)
    {
        // Apply post-roll manipulations:
        int adjusted_mhit = rolled_mhit + atk.post_roll_to_hit_modifiers(rolled_mhit, false);

        // But the above will bail out because there's no defender in the attack object,
        // so we reproduce any possibly relevant effects here:
        adjusted_mhit += mi.lighting_modifiers();

        // And this duplicates ranged_attack::post_roll_to_hit_modifiers().
        if (!melee && mi.is(MB_BULLSEYE_TARGET))
        {
            adjusted_mhit += calc_spell_power(SPELL_DIMENSIONAL_BULLSEYE)
                                / 2 / BULLSEYE_TO_HIT_DIV;
        }

        // Now count the hit if it's above target ev
        if (adjusted_mhit >= ev)
            hits++;
    }

    double hit_chance = ((double)hits) / to_land;
    // Apply Bayes Theorem to account for auto hit and miss.
    hit_chance = hit_chance * (1 - AUTO_MISS_CHANCE / 200.0)
                 + (1 - hit_chance) * AUTO_HIT_CHANCE / 200.0;
    return hit_chance;
}

static bool _to_hit_is_invisible(const monster_info& mi)
{
    // Replicates player->visible_to(defender)
    if (mi.attitude == ATT_FRIENDLY)
        return false;

    if (mi.has_trivial_ench(ENCH_BLIND))
        return true;

    return you.invisible() && !mi.can_see_invisible() && !you.in_water();
}

static double _to_hit_shield_chance(const monster_info& mi,
                                    bool melee, int to_land, bool penetrating)
{
    // Duplicates more logic that is defined in attack::attack_shield_blocked, and
    // melee_attack and ranged_attack classes, for real attacks, and also
    // monster::shield_bonus (since it's randomised and loses some resolution
    // through division, it's more accurate to use floating point math here.)

    // Guaranteed hits should ignore the shield as well
    if (to_land >= AUTOMATIC_HIT)
        return 0;

    // Shield bonus is -100 if incapacitated or the roll is too low, resolve
    // that first. Monster can only block at all if shield_class > 2.
    if (mi.incapacitated() || mi.sh <= 2)
        return 0;

    // There is also a check for a ranged attacker to ignore a shield, but we can't call
    // the same function because it needs a real defender; instead we simply pass in
    // penetration (since we might want to set it another way for e.g. spells)
    if (!melee && !player_omnireflects() && penetrating)
        return 0;

    // Main check
    const int con_block = you.shield_bypass_ability(to_land);

    // This is an approximation of the average result of shield_bonus, which
    // looks odd due to how integer rounding affects the result for different
    // values of sh. It's a sequence of fractions looking like:
    // 1/3, 2/4, 4/5, 6/6, 9/7, 12/8, 16/9, 20/10 ... (for sh starting from 3)
    const int pro_mult = (mi.sh - 1) / 2;
    double pro_block = (double)(pro_mult * (mi.sh - pro_mult - 1)) / (double)mi.sh;

    // Slightly inaccurate for very low shield values as the chance should be
    // zero below a certain sh, due to integer division in attack_shield_blocked.
    if (_to_hit_is_invisible(mi))
        pro_block /= 3.0;

    // Shield exhaustion would be checked at this point but it's only meaningful
    // for multiple hits, so for basic hit chance we assume no exhaustion.

    // Final average
    return min(1.0, max(0.0, pro_block / (double)con_block));
}

/**
 * Return the odds of a provided attack hitting a defender defined as a
 * monster_info, rounded to the nearest percent.
 *
 * @return                  To-hit percent between 0 and 100 (inclusive).
 */
int to_hit_pct(const monster_info& mi, attack &atk, bool melee,
               bool penetrating, int distance)
{
    const int to_land = atk.calc_pre_roll_to_hit(false);
    const double hit_chance = _to_hit_hit_chance(mi, atk, melee, to_land);
    const double shield_chance = _to_hit_shield_chance(mi, melee, to_land, penetrating);
    const int blind_miss_chance = player_blind_miss_chance(distance);
    return (int)(hit_chance * (1.0 - shield_chance) * 100 * (100 - blind_miss_chance) / 100);
}

/**
 * Return the odds of the player hitting a defender defined as a
 * monster_info with an auxiliary melee attack, rounded to the nearest percent.
 */
int to_hit_pct_aux(const monster_info& mi, attack &atk)
{
    const int to_land = aux_to_hit();
    const double hit_chance = _to_hit_hit_chance(mi, atk, true, to_land, true);
    const double shield_chance = _to_hit_shield_chance(mi, true, to_land, false);
    const int blind_miss_chance = player_blind_miss_chance(1);
    return (int)(hit_chance * (1.0 - shield_chance) * 100 * (100 - blind_miss_chance) / 100);
}

/**
 * Return the base to-hit bonus that a monster with the given HD gets.
 * @param hd               The hit dice (level) of the monster.
 * @param skilled    Does the monster have bonus to-hit from the fighter or archer flag?
 *
 * @return         A base to-hit value, before equipment, statuses, etc.
 */
int mon_to_hit_base(int hd, bool skilled)
{
    const int hd_mult = skilled ? 5 : 3;
    return 18 + hd * hd_mult / 2;
}

int mon_shield_bypass(int hd)
{
    return 15 + hd * 2 / 3;
}

/**
 * Return the odds of a monster attack with the given to-hit bonus hitting the given ev (scaled by 100),
 * rounded to the nearest percent.
 *
 * TODO: deduplicate with to_hit_pct().
 *
 * @return                  To-hit percent between 0 and 100 (inclusive).
 */
int mon_to_hit_pct(int to_land, int scaled_ev)
{
    if (to_land >= AUTOMATIC_HIT)
        return 100;

    if (scaled_ev <= 0)
        return 100 - MIN_HIT_MISS_PERCENTAGE / 2;

    ++to_land; // per calc_to_hit()

    const int ev = scaled_ev/100;

    // EV is random-rounded, so the actual value might be either ev or ev+1.
    // We repeat the calculation below once for each case
    int hits_lower = 0;
    for (int ev1 = 0; ev1 < ev; ev1++)
        for (int ev2 = 0; ev2 < ev; ev2++)
            hits_lower += max(0, to_land - (ev1 + ev2));
    double hit_chance_lower = ((double)hits_lower) / (to_land * ev * ev);

    int hits_upper = 0;
    for (int ev1 = 0; ev1 < ev+1; ev1++)
        for (int ev2 = 0; ev2 < ev+1; ev2++)
            hits_upper += max(0, to_land - (ev1 + ev2));
    double hit_chance_upper = ((double)hits_upper) / (to_land * (ev+1) * (ev+1));

    double hit_chance = ((100 - (scaled_ev % 100)) * hit_chance_lower + (scaled_ev % 100) * hit_chance_upper) / 100;

    // Apply Bayes Theorem to account for auto hit and miss.
    hit_chance = hit_chance * (1 - MIN_HIT_MISS_PERCENTAGE / 200.0)
              + (1 - hit_chance) * MIN_HIT_MISS_PERCENTAGE / 200.0;

    return (int)(hit_chance*100);
}

int mon_beat_sh_pct(int bypass, int scaled_sh)
{
    if (scaled_sh <= 0)
        return 100;

    int sh = scaled_sh/100;

    // SH is random-rounded, so the actual value might be either sh or sh+1.
    // We repeat the calculation below once for each case
    sh *= 2; // per shield_bonus()
    int hits_lower = 0;
    for (int sh1 = 0; sh1 < sh; sh1++)
    {
        for (int sh2 = 0; sh2 < sh; sh2++)
        {
            int adj_sh = (sh1 + sh2) / (3*2) - 1;
            hits_lower += max(0, bypass - adj_sh);
        }
    }
    const int denom_lower = sh * sh * bypass;
    double hit_chance_lower = ((double)hits_lower * 100) / denom_lower;

    sh += 2; // since we already multiplied by 2
    int hits_upper = 0;
    for (int sh1 = 0; sh1 < sh; sh1++)
    {
        for (int sh2 = 0; sh2 < sh; sh2++)
        {
            int adj_sh = (sh1 + sh2) / (3*2) - 1;
            hits_upper += max(0, bypass - adj_sh);
        }
    }
    const int denom_upper = sh * sh * bypass;
    double hit_chance_upper = ((double)hits_upper * 100) / denom_upper;

    double hit_chance = ((100 - (scaled_sh % 100)) * hit_chance_lower + (scaled_sh % 100) * hit_chance_upper) / 100;

    return (int)hit_chance;
}

/**
 * Switch from a bad weapon to melee.
 *
 * This function assumes some weapon is being wielded.
 * @return whether a swap did occur.
 */
static bool _autoswitch_to_melee()
{
    // It's not a good idea to quietly replace an attack action with one that
    // takes 5 times as long. Even prompting would be better.
    if (you.has_mutation(MUT_SLOW_WIELD))
        return false;

    item_def* weapon = you.weapon();
    bool penance;
    if (!weapon
        // don't autoswitch from a weapon that needs a warning
        || is_melee_weapon(*weapon)
            && !needs_handle_warning(*weapon, OPER_ATTACK, penance))
    {
        return false;
    }

    // Only try autoswapping if weapon is in slot a or b
    int item_slot;
    if (weapon->link == letter_to_index('a'))
        item_slot = letter_to_index('b');
    else if (weapon->link == letter_to_index('b'))
        item_slot = letter_to_index('a');
    else
        return false;

    // don't autoswitch to a weapon that needs a warning, or to a non-weapon
    if (!you.inv[item_slot].defined()
        || !is_melee_weapon(you.inv[item_slot])
        || needs_handle_warning(you.inv[item_slot], OPER_ATTACK, penance))
    {
        return false;
    }

    // auto_switch handles the item slots itself
    return auto_wield();
}

static bool _can_shoot_with(const item_def *weapon)
{
    // TODO: dedup elsewhere.
    return weapon
        && is_range_weapon(*weapon)
        && !you.attribute[ATTR_HELD]
        && !you.berserk();
}

static bool _autofire_at(actor *defender)
{
    if (!_can_shoot_with(you.weapon()) || you.duration[DUR_CONFUSING_TOUCH])
        return false;
    dist t;
    t.target = defender->pos();
    shared_ptr<quiver::action> ract = quiver::find_ammo_action();
    ract->set_target(t);
    throw_it(*ract);
    return true;
}

/**
 * Handle melee combat between attacker and defender.
 *
 * Works using the new fight rewrite. For a monster attacking, this method
 * loops through all their available attacks, instantiating a new melee_attack
 * for each attack. Combat effects should not go here, if at all possible. This
 * is merely a wrapper function which is used to start combat.
 *
 * @param[in] attacker,defender The (non-null) participants in the attack.
 *                              Either may be killed as a result of the attack.
 * @param[out] did_hit If non-null, receives true if the attack hit the
 *                     defender, and false otherwise.
 * @param simu Is this a simulated attack?  Disables a few problematic
 *             effects such as blood spatter and distortion teleports.
 *
 * @return Whether the attack took time (i.e. wasn't cancelled).
 */
bool fight_melee(actor *attacker, actor *defender, bool *did_hit, bool simu)
{
    ASSERT(attacker); // XXX: change to actor &attacker
    ASSERT(defender); // XXX: change to actor &defender

    // A dead defender would result in us returning true without actually
    // taking an action.
    ASSERT(defender->alive());

    if (defender->is_player())
    {
        ASSERT(!crawl_state.game_is_arena());
        // Friendly and good neutral monsters won't attack unless confused.
        if (attacker->as_monster()->wont_attack()
            && !mons_is_confused(*attacker->as_monster())
            && !attacker->as_monster()->has_ench(ENCH_FRENZIED))
        {
            return false;
        }

        // In case the monster hasn't noticed you, bumping into it will
        // change that.
        behaviour_event(attacker->as_monster(), ME_ALERT, defender);
    }
    else if (attacker->is_player())
    {
        ASSERT(!crawl_state.game_is_arena());
        // Can't damage orbs this way.
        if (mons_is_projectile(defender->type) && !you.confused())
        {
            you.turn_is_over = false;
            return false;
        }

        if (!simu && you.weapon() && !you.confused())
        {
            if (Options.auto_switch && _autoswitch_to_melee())
                return true; // Is this right? We did take time, but we didn't melee
            if (!simu && _autofire_at(defender))
                return true;
        }

        melee_attack attk(&you, defender);

        if (simu)
            attk.simu = true;

        // We're trying to hit a monster, break out of travel/explore now.
        interrupt_activity(activity_interrupt::hit_monster,
                           defender->as_monster());

        // Check if the player is fighting with something unsuitable,
        // or someone unsuitable.
        if (you.can_see(*defender) && !simu
            && !wielded_weapon_check(you.weapon()))
        {
            you.turn_is_over = false;
            return false;
        }

        const bool success = attk.launch_attack_set();
        if (attk.cancel_attack)
            you.turn_is_over = false;
        else
            you.time_taken = you.attack_delay().roll();

        if (!success)
            return !attk.cancel_attack;

        if (did_hit)
            *did_hit = attk.did_hit;

        if (!simu && will_have_passive(passive_t::shadow_attacks))
            dithmenos_shadow_melee(defender);

        // Executioner state doesn't wear off so long as you keep attacking.
        if (you.duration[DUR_EXECUTION])
            you.duration[DUR_EXECUTION] += you.time_taken;

        if (you.duration[DUR_PARAGON_ACTIVE])
            paragon_attack_trigger();

        return true;
    }

    // If execution gets here, attacker != Player, so we can safely continue
    // with processing the number of attacks a monster has without worrying
    // about unpredictable or weird results from players.

    // Spectral weapons should only attack when triggered by their summoner,
    // which is handled via spectral_weapon_fineff. But if they bump into a
    // valid attack target during their subsequent wanderings, they will still
    // attempt to attack it via this method, which they should not.
    if (attacker->as_monster()->type == MONS_SPECTRAL_WEAPON)
    {
        // Still consume energy so we don't cause an infinite loop
        attacker->as_monster()->lose_energy(EUT_ATTACK);
        return false;
    }

    const int nrounds = attacker->as_monster()->has_hydra_multi_attack()
        ? attacker->heads() + MAX_NUM_ATTACKS - 1
        : MAX_NUM_ATTACKS;
    coord_def pos = defender->pos();

    // Melee combat, tell attacker to wield its melee weapon.
    attacker->as_monster()->wield_melee_weapon();

    int effective_attack_number = 0;
    int attack_number;
    for (attack_number = 0; attack_number < nrounds && attacker->alive();
         ++attack_number, ++effective_attack_number)
    {
        if (!attacker->alive())
            return false;

        // Monster went away?
        if (!defender->alive()
            || defender->pos() != pos
            || defender->is_banished())
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
                if (*i == you.pos()
                    && !mons_aligned(attacker, &you))
                {
                    attacker->as_monster()->foe = MHITYOU;
                    attacker->as_monster()->target = you.pos();
                    defender = &you;
                    end = false;
                    break;
                }

                monster* mons = monster_at(*i);
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

        melee_attack melee_attk(attacker, defender, attack_number,
                                effective_attack_number);

        melee_attk.simu = simu;

        // If the attack fails out, keep effective_attack_number up to
        // date so that we don't cause excess energy loss in monsters
        if (!melee_attk.attack())
            effective_attack_number = melee_attk.effective_attack_number;
        else if (did_hit && !(*did_hit))
            *did_hit = melee_attk.did_hit;

        fire_final_effects();
    }

    // Here, rather than in melee_attack, so that it only triggers on attack
    // actions, rather than additional times for bonus attacks (ie: from Autumn Katana)
    if (attacker->as_monster()->type == MONS_PLATINUM_PARAGON)
        paragon_charge_up(*attacker->as_monster());

    return true;
}

/**
 * What is the best stab type that could be applied against a given monster
 * right now? (Ignoring the player's own potential incapacitation.)
 *
 * Note that while all of these stab types map to only two tiers of mechnical
 * power, the specific type is still noted in the action count section of
 * character dumps.
 *
 * @param victim    The monster being sized up by the player.
 * @return          The best (most damaging) kind of stab available against this
 *                  monser, or STAB_NO_STAB if none is available.
 */
stab_type find_player_stab_type(const monster &victim)
{
    // No stabbing monsters that cannot fight (e.g. plants) or monsters
    // the attacker can't see (either due to invisibility or being behind
    // opaque clouds).
    if (victim.is_firewood())
        return STAB_NO_STAB;

    if (!you.can_see(victim))
        return STAB_NO_STAB;

    // sleeping
    if (victim.asleep())
        return STAB_SLEEPING;

    // paralysed
    if (victim.paralysed())
        return STAB_PARALYSED;

    // petrified
    if (victim.petrified())
        return STAB_PETRIFIED;

    // petrifying
    if (victim.petrifying())
        return STAB_PETRIFYING;

    // held in a net
    if (victim.caught())
        return STAB_HELD_IN_NET;

    // invisible
    if (!you.visible_to(&victim))
    {
        if (victim.has_ench(ENCH_BLIND))
            return STAB_BLIND;
        else
            return STAB_INVISIBLE;
    }

    // fleeing
    if (mons_is_fleeing(victim))
        return STAB_FLEEING;

    // allies
    if (victim.friendly())
        return STAB_ALLY;

    // confused (but not perma-confused)
    if (mons_is_confused(victim, false))
        return STAB_CONFUSED;

    // Distracted
    if (mons_looks_distracted(victim))
        return STAB_DISTRACTED;

    return STAB_NO_STAB;
}

/**
 * What bonus does this type of stab give the player when attacking?
 *
 * @param   The type of stab in question; e.g. STAB_SLEEPING.
 * @return  The bonus the stab gives. Note that this is used as a divisor for
 *          damage, so the larger the value we return here, the less bonus
 *          damage will be done.
 */
int stab_bonus_denom(stab_type stab)
{
    // XXX: if we don't get rid of this logic, turn it into a static array.
    switch (stab)
    {
        case STAB_NO_STAB:
        case NUM_STABS:
            return 0;
        case STAB_SLEEPING:
        case STAB_PARALYSED:
        case STAB_PETRIFIED:
            return 1;
        default:
            return 4;
    }
}

static bool is_boolean_resist(beam_type flavour)
{
    switch (flavour)
    {
    case BEAM_ELECTRICITY:
    case BEAM_MIASMA:
    case BEAM_STICKY_FLAME:
    case BEAM_WATER:  // water asphyxiation damage,
                      // bypassed by being water inhabitant.
    case BEAM_POISON:
    case BEAM_POISON_ARROW:
        return true;
    default:
        return false;
    }
}

// Gets the percentage of the total damage of this damage flavour that can
// be resisted.
static inline int get_resistible_fraction(beam_type flavour)
{
    switch (flavour)
    {
    case BEAM_WATER:
    case BEAM_ICE:
    case BEAM_THUNDER:
    case BEAM_LAVA:
        return 50;
    case BEAM_POISON_ARROW:
    case BEAM_MERCURY:
        return 70;
    default:
        return 100;
    }
}

static int _beam_to_resist(const actor* defender, beam_type flavour)
{
    switch (flavour)
    {
        case BEAM_FIRE:
        case BEAM_LAVA:
            return defender->res_fire();
        case BEAM_DAMNATION:
            return defender->res_damnation();
        case BEAM_STEAM:
            return defender->res_steam();
        case BEAM_COLD:
        case BEAM_ICE:
            return defender->res_cold();
        case BEAM_WATER:
            return defender->res_water_drowning();
        case BEAM_ELECTRICITY:
        case BEAM_THUNDER:
        case BEAM_STUN_BOLT:
            return defender->res_elec();
        case BEAM_NEG:
        case BEAM_PAIN:
        case BEAM_MALIGN_OFFERING:
        case BEAM_VAMPIRIC_DRAINING:
            return defender->res_negative_energy();
        case BEAM_ACID:
            return defender->res_corr();
        case BEAM_POISON:
        case BEAM_POISON_ARROW:
        case BEAM_MERCURY:
            return defender->res_poison();
        case BEAM_HOLY:
            return defender->res_holy_energy();
        case BEAM_FOUL_FLAME:
            return defender->res_foul_flame();
        default:
            return 0;
    }
}


/**
 * Adjusts damage for elemental resists, electricity and poison.
 *
 * For players, damage is reduced to 1/2, 1/3, or 1/5 if res has values 1, 2,
 * or 3, respectively. "Boolean" resists (rElec, rPois) reduce damage to 1/3.
 * rN is a special case that reduces damage to 1/2, 1/4, 0 instead.
 *
 * For monsters, damage is reduced to 1/2, 1/5, and 0 for 1/2/3 resistance.
 * "Boolean" resists give 1/3, 1/6, 0 instead.
 *
 * @param defender      The victim of the attack.
 * @param flavour       The type of attack having its damage adjusted.
 *                      (Does not necessarily imply the attack is a beam.)
 * @param rawdamage     The base damage, to be adjusted by resistance.
 * @return              The amount of damage done, after resists are applied.
 */
int resist_adjust_damage(const actor* defender, beam_type flavour, int rawdamage)
{
    const int res = _beam_to_resist(defender, flavour);
    if (!res)
        return rawdamage;

    const bool is_mon = defender->is_monster();

    const int resistible_fraction = get_resistible_fraction(flavour);

    int resistible = rawdamage * resistible_fraction / 100;
    const int irresistible = rawdamage - resistible;

    if (res > 0)
    {
        const bool immune_at_3_res = is_mon
                                     || flavour == BEAM_NEG
                                     || flavour == BEAM_PAIN
                                     || flavour == BEAM_MALIGN_OFFERING
                                     || flavour == BEAM_VAMPIRIC_DRAINING
                                     || flavour == BEAM_HOLY
                                     || flavour == BEAM_FOUL_FLAME
                                     || flavour == BEAM_POISON
                                     // just the resistible part
                                     || flavour == BEAM_MERCURY
                                     || flavour == BEAM_POISON_ARROW;

        if (immune_at_3_res && res >= 3 || res > 3)
            resistible = 0;
        else
        {
            // Is this a resist that claims to be boolean for damage purposes?
            const int bonus_res = (is_boolean_resist(flavour) ? 1 : 0);

            // Monster resistances are stronger than player versions.
            if (is_mon)
                resistible /= 1 + bonus_res + res * res;
            else if (flavour == BEAM_NEG
                     || flavour == BEAM_PAIN
                     || flavour == BEAM_MALIGN_OFFERING
                     || flavour == BEAM_VAMPIRIC_DRAINING)
            {
                resistible /= res * 2;
            }
            else
                resistible /= (3 * res + 1) / 2 + bonus_res;
        }
    }
    else if (res < 0)
        resistible = resistible * 15 / 10;

    return max(resistible + irresistible, 0);
}

// Reduce damage by AC.
// In most cases, we want AC to mostly stop weak attacks completely but affect
// strong ones less, but the regular formula is too hard to apply well to cases
// when damage is spread into many small chunks.
//
// Every point of damage is processed independently. Every point of AC has
// an independent 1/81 chance of blocking that damage.
//
// AC 20 stops 22% of damage, AC 40 -- 39%, AC 80 -- 63%.
int apply_chunked_AC(int dam, int ac)
{
    double chance = pow(80.0/81, ac);
    uint64_t cr = chance * (((uint64_t)1) << 32);

    int hurt = 0;
    for (int i = 0; i < dam; i++)
        if (rng::get_uint32() < cr)
            hurt++;

    return hurt;
}

///////////////////////////////////////////////////////////////////////////

static bool _weapon_is_bad(const item_def *weapon, bool &penance)
{
    if (!weapon)
        return false;
    return needs_handle_warning(*weapon, OPER_ATTACK, penance)
           || !is_melee_weapon(*weapon) && !_can_shoot_with(weapon);
}

/// If we have an off-hand weapon, will it attack when we fire/swing our main weapon?
static bool _rangedness_matches(const item_def *weapon, const item_def *offhand)
{
    if (!offhand)
        return true;
    return (!weapon || is_melee_weapon(*weapon)) == is_melee_weapon(*offhand);
}

static string _describe_weapons(const item_def *weapon,
                                const item_def *offhand)
{
    if (!weapon && !offhand)
        return "nothing";
    if (!weapon)
        return offhand->name(DESC_YOUR).c_str();
    if (!offhand)
        return weapon->name(DESC_YOUR).c_str();
    return make_stringf("%s and %s",
                        weapon->name(DESC_YOUR).c_str(),
                        offhand->name(DESC_YOUR).c_str());
}

static bool _missing_weapon(const item_def *weapon, const item_def *offhand)
{
    if (weapon || offhand) // TODO: maybe should warn for untrained UC here..?
        return false;
    // OK, we're unarmed. Is that... a bad thing?
    // Don't pester the player if they're using UC, in treeform,
    // or if they don't have any melee weapons yet.
    return !you.skill(SK_UNARMED_COMBAT)
           && you.form != transformation::tree
           && any_of(you.inv.begin(), you.inv.end(),
                     [](item_def &it) {
               return is_melee_weapon(it) && can_equip_item(it);
            });
}

bool wielded_weapon_check(const item_def *weapon, string attack_verb)
{
    const item_def *offhand = you.offhand_weapon();
    if (you.received_weapon_warning || you.confused())
        return true;

    bool penance = false;
    const bool primary_bad = _weapon_is_bad(weapon, penance);
    // Important: check rangedness_matches *before* checking weapon_is_bad
    // for the offhand, so that we don't incorrectly claim you'll get penance
    // for a weapon that won't even attack!
    const bool offhand_bad = !_rangedness_matches(weapon, offhand)
                             || _weapon_is_bad(offhand, penance);

    if (!primary_bad && !offhand_bad && !_missing_weapon(weapon, offhand))
        return true;

    string wpn_desc = _describe_weapons(weapon, offhand);

    string prompt;
    prompt = make_stringf("Really %s while wielding %s?",
        attack_verb.size() ? attack_verb.c_str() : "attack",
        wpn_desc.c_str());
    if (penance)
        prompt += " This could place you under penance!";

    const bool result = yesno(prompt.c_str(), true, 'n');

    if (!result)
        canned_msg(MSG_OK);

    learned_something_new(HINT_WIELD_WEAPON); // for hints mode Rangers

    // Don't warn again if you decide to continue your attack.
    if (result)
        you.received_weapon_warning = true;

    return result;
}

bool player_unrand_bad_attempt(const item_def &weapon,
                               const actor *defender,
                               bool check_only)
{
    const monster* defending_monster = defender ? defender->as_monster() :
        nullptr;

    if (is_unrandom_artefact(weapon, UNRAND_VARIABILITY)
             || is_unrandom_artefact(weapon, UNRAND_SINGING_SWORD)
                && !silenced(you.pos()))
    {
        targeter_radius hitfunc(&you, LOS_NO_TRANS);

        return stop_attack_prompt(hitfunc, "attack",
                               [](const actor *act)
                               {
                                   return !never_harm_monster(&you, act->as_monster());
                               }, nullptr, defending_monster,
                               check_only);
    }
    if (is_unrandom_artefact(weapon, UNRAND_TORMENT))
    {
        targeter_radius hitfunc(&you, LOS_NO_TRANS);

        return stop_attack_prompt(hitfunc, "attack",
                               [] (const actor *m)
                               {
                                   return !m->res_torment()
                                       && !never_harm_monster(&you, m->as_monster());
                               },
                                  nullptr, defending_monster,
                                check_only);
    }

    if (!defender)
        return false;

    return player_unrand_bad_target(weapon, *defender, check_only);
}

bool player_unrand_bad_attempt(const item_def *weapon,
    const item_def *offhand,
    const actor *defender,
    bool check_only)
{
    return weapon && ::player_unrand_bad_attempt(*weapon, defender, check_only)
        || offhand && ::player_unrand_bad_attempt(*offhand, defender, check_only);
}

bool player_unrand_bad_target(const item_def &weapon,
    const actor &defender,
    bool check_only)
{
    const monster* defending_monster = defender.as_monster();

    if (is_unrandom_artefact(weapon, UNRAND_DEVASTATOR))
    {
        targeter_smite hitfunc(&you, LOS_RADIUS, 1, 1);
        hitfunc.set_aim(defender.pos());

        return stop_attack_prompt(hitfunc, "attack",
                               [](const actor *act)
                               {
                                   return !never_harm_monster(&you, act->as_monster());
                               }, nullptr, defending_monster,
                               check_only);
    }
    if (is_unrandom_artefact(weapon, UNRAND_ARC_BLADE))
    {
        if (you.pos().distance_from(defender.pos()) <= 1)
            return !safe_discharge(you.pos(), check_only);

        return !safe_discharge(defender.pos(), check_only, false);
    }
    if (is_unrandom_artefact(weapon, UNRAND_POWER))
    {
        targeter_beam hitfunc(&you, 4, ZAP_SWORD_BEAM, 100, 0, 0);
        hitfunc.beam.aimed_at_spot = false;
        hitfunc.set_aim(defender.pos());

        return stop_attack_prompt(hitfunc, "attack",
                               [](const actor *act)
                               {
                                   return !never_harm_monster(&you, act->as_monster());
                               }, nullptr, defending_monster,
                               check_only);
    }
    return false;
}

bool player_unrand_bad_target(const item_def *weapon,
    const item_def *offhand,
    const actor &defender,
    bool check_only)
{
    return weapon && ::player_unrand_bad_target(*weapon, defender, check_only)
        || offhand && ::player_unrand_bad_target(*offhand, defender, check_only);
}

/**
 * Should the given attacker cleave into the given victim with an axe or axe-
 * like weapon?
 *
 * @param attacker  The creature doing the cleaving.
 * @param defender  The potential cleave-ee.
 * @return          True if the defender is an enemy of the defender; false
 *                  otherwise.
 */
bool dont_harm(const actor &attacker, const actor &defender)
{
    if (mons_aligned(&attacker, &defender))
        return true;

    if (defender.is_monster())
        return never_harm_monster(&attacker, defender.as_monster(), true);

    if (defender.is_player())
        return attacker.wont_attack();

    if (attacker.is_player())
    {
        return defender.wont_attack()
               || mons_attitude(*defender.as_monster()) == ATT_NEUTRAL
                  && !defender.as_monster()->has_ench(ENCH_FRENZIED);
    }

    return false;
}

/**
 * Force cleave attacks. Used for melee actions that don't have targets, e.g.
 * attacking empty space (otherwise, cleaving is handled in melee_attack).
 *
 * @param target the nominal target of the original attack.
 * @return whether there were cleave targets relative to the player and `target`.
 */
bool force_player_cleave(coord_def target)
{
    list<actor*> cleave_targets;
    get_cleave_targets(you, target, cleave_targets);

    if (!cleave_targets.empty())
    {
        // Rift is too funky and hence gets no special treatment.
        const int range = you.reach_range();
        targeter_cleave hitfunc(&you, target, range);
        if (stop_attack_prompt(hitfunc, "attack"))
            return true;

        if (!you.fumbles_attack())
            attack_multiple_targets(you, cleave_targets);
        return true;
    }

    return false;
}

bool attack_cleaves(const actor &attacker, const item_def *weap)
{
    if (attacker.is_player()
        && (you.form == transformation::storm || you.duration[DUR_CLEAVE]))
    {
        return true;
    }
    else if (attacker.is_monster()
             && attacker.as_monster()->has_ench(ENCH_INSTANT_CLEAVE))
    {
        return true;
    }

    return weap && weapon_cleaves(*weap);
}

bool weapon_cleaves(const item_def &weap)
{
    return item_attack_skill(weap) == SK_AXES
           || is_unrandom_artefact(weap, UNRAND_LOCHABER_AXE);
}

int weapon_hits_per_swing(const item_def &weap)
{
    if (!weap.is_type(OBJ_WEAPONS, WPN_QUICK_BLADE))
        return 1;
    if (is_unrandom_artefact(weap, UNRAND_GYRE))
        return 4;
    return 2;
}

bool weapon_multihits(const item_def *weap)
{
    return weap && weapon_hits_per_swing(*weap) > 1;
}

/**
 * List potential cleave targets (adjacent hostile creatures), including the
 * defender itself.
 *
 * @param attacker[in]   The attacking creature.
 * @param def[in]        The location of the targeted defender.
 * @param targets[out]   A list to be populated with targets.
 * @param which_attack   The attack_number (default -1, which uses the default weapon).
 */
void get_cleave_targets(const actor &attacker, const coord_def& def,
                        list<actor*> &targets, int which_attack,
                        bool force_cleaving, const item_def *weapon)
{
    // Prevent scanning invalid coordinates if the attacker dies partway through
    // a cleave (due to hitting explosive creatures, or perhaps other things)
    if (!attacker.alive())
        return;

    if (actor_at(def))
        targets.push_back(actor_at(def));

    const item_def* weap = weapon ? weapon : attacker.weapon(which_attack);
    if (!force_cleaving && !attack_cleaves(attacker, weap))
        return;

    const coord_def atk = attacker.pos();
    //If someone adds a funky reach which isn't just a number
    //They will need to special case it here.
    const int cleave_radius = weap ? weapon_reach(*weap) : 1;

    for (distance_iterator di(atk, true, true, cleave_radius); di; ++di)
    {
        if (*di == def) continue; // no double jeopardy
        actor *target = actor_at(*di);
        if (!target || dont_harm(attacker, *target))
            continue;
        if (di.radius() == 2 && !can_reach_attack_between(atk, *di, REACH_TWO))
            continue;
        else if (di.radius() == 3 && !can_reach_attack_between(atk, *di, REACH_THREE))
            continue;
        targets.push_back(target);
    }
}

/**
 * Attack a provided list of cleave or quick-blade targets.
 *
 * @param attacker                  The attacking creature.
 * @param targets                   The targets to cleave.
 * @param attack_number             For monsters, which of their 4 possible attacks this
 *                                  corresponds to. For players, usually 0, but can be 1
 *                                  for the second of a Coglin's two weapons per round
 * @param effective_attack_number   Like attack_number, if invalid attacks were skipped
 *                                  (ie: fake attacks or ones outside of their max range.)
 * @param wu_jian_attack            The type of martial attack being performed (if any).
 * @param is_projected              Whether the attack is projected (ie: Manifold Assault)
 * @param is_cleaving               Whether this is a cleaving attack.
 * @param weapon                    The weapon this attack is being performed with (if any).
 *
 * @return  The total amount of damage inflicted directly by all attacks caused by this function.
 */
int attack_multiple_targets(actor &attacker, list<actor*> &targets,
                             int attack_number, int effective_attack_number,
                             wu_jian_attack_type wu_jian_attack,
                             bool is_projected, bool is_cleaving,
                             item_def *weapon)
{
    if (!attacker.alive())
        return 0;

    int total_damage = 0;
    const item_def* weap = weapon ? weapon : attacker.weapon(attack_number);
    const bool reaching = weap && weapon_reach(*weap) > REACH_NONE;
    while (attacker.alive() && !targets.empty())
    {
        actor* def = targets.front();

        if (def && def->alive() && !dont_harm(attacker, *def)
            && (is_projected
                || adjacent(attacker.pos(), def->pos())
                || reaching))
        {
            melee_attack attck(&attacker, def, attack_number,
                               ++effective_attack_number);
            if (weapon && attacker.is_player())
                attck.set_weapon(weapon, true);

            attck.wu_jian_attack = wu_jian_attack;
            attck.is_projected = is_projected;
            attck.cleaving = is_cleaving;
            attck.is_multihit = !is_cleaving; // heh heh heh
            attck.attack();

            total_damage += attck.total_damage_done;
        }
        targets.pop_front();
    }

    return total_damage;
}

/**
 * What skill is required to reach mindelay with a weapon? May be >27.
 * @param weapon The weapon to be considered.
 * @returns The level of the relevant skill you must reach.
 */
int weapon_min_delay_skill(const item_def &weapon)
{
    const int speed = property(weapon, PWPN_SPEED);
    const int mindelay = weapon_min_delay(weapon, false);
    return (speed - mindelay) * 2;
}

/**
 * How fast will this weapon get from your skill training?
 *
 * @param weapon the weapon to be considered.
 * @param check_speed whether to take it into account if the weapon has the
 *                    speed brand.
 * @return How many aut the fastest possible attack with this weapon would take.
 */
int weapon_min_delay(const item_def &weapon, bool check_speed)
{
    const int base = property(weapon, PWPN_SPEED);
    if (is_unrandom_artefact(weapon, UNRAND_WOODCUTTERS_AXE))
        return base;

    int min_delay = base/2;

    // Short blades can get up to at least unarmed speed.
    if (item_attack_skill(weapon) == SK_SHORT_BLADES && min_delay > 5)
        min_delay = 5;

    // All weapons have min delay 7 or better
    if (min_delay > 7)
        min_delay = 7;

    // ...except crossbows...
    if (is_crossbow(weapon) && min_delay < 10)
        min_delay = 10;

    // ... and unless it would take more than skill 27 to get there.
    // Round up the reduction from skill, so that min delay is rounded down.
    min_delay = max(min_delay, base - (MAX_SKILL_LEVEL + 1)/2);

    if (check_speed)
        min_delay = weapon_adjust_delay(weapon, min_delay, false);

    // never go faster than speed 3 (ie 3.33 attacks per round)
    if (min_delay < 3)
        min_delay = 3;

    return min_delay;
}

/// Adjust delay based on weapon brand.
int weapon_adjust_delay(const item_def &weapon, int base, bool random)
{
    const brand_type brand = get_weapon_brand(weapon);
    if (brand == SPWPN_SPEED)
        return random ? div_rand_round(base * 2, 3) : (base * 2) / 3;
    if (brand == SPWPN_HEAVY)
        return random ? div_rand_round(base * 3, 2) : (base * 3) / 2;
    return base;
}

int mons_weapon_damage_rating(const item_def &launcher)
{
    return property(launcher, PWPN_DAMAGE) + launcher.plus;
}

bool bad_attack(const monster *mon, string& adj, string& suffix,
                bool& would_cause_penance, coord_def attack_pos)
{
    ASSERT(mon); // XXX: change to const monster &mon
    ASSERT(!crawl_state.game_is_arena());

    if (!you.can_see(*mon))
        return false;

    if (attack_pos == coord_def(0, 0))
        attack_pos = you.pos();

    adj.clear();
    suffix.clear();
    would_cause_penance = false;

    if (is_sanctuary(mon->pos()) || is_sanctuary(attack_pos))
        suffix = ", despite your sanctuary";

    if (mon->friendly())
    {
        // There's not really any harm in attacking your own spectral weapon.
        // It won't share damage, and it'll go away anyway.
        if (mon->type == MONS_SPECTRAL_WEAPON)
            return false;

        // If we cannot hurt an ally anyway, don't bother warning as if we could
        if (never_harm_monster(&you, mon))
            return false;

        if (god_hates_attacking_friend(you.religion, *mon))
        {
            adj = "your ally ";

            monster_info mi(mon, MILEV_NAME);
            if (!mi.is(MB_NAME_UNQUALIFIED))
                adj += "the ";

            would_cause_penance = true;

        }
        else
        {
            adj = "your ";

            monster_info mi(mon, MILEV_NAME);
            if (mi.is(MB_NAME_UNQUALIFIED))
                adj += "ally ";
        }

        return true;
    }

    if (mon->neutral()
        && (is_good_god(you.religion)
            || you_worship(GOD_BEOGH) && mons_genus(mon->type) == MONS_ORC)
        && !mon->has_ench(ENCH_FRENZIED))
    {
        adj += "neutral ";
        if (you_worship(GOD_SHINING_ONE) || you_worship(GOD_ELYVILON)
            || you_worship(GOD_BEOGH))
        {
            would_cause_penance = true;
        }
    }
    else if (mon->wont_attack())
    {
        adj += "non-hostile ";
        if (you_worship(GOD_SHINING_ONE) || you_worship(GOD_ELYVILON))
            would_cause_penance = true;
    }

    return !adj.empty() || !suffix.empty();
}

bool stop_attack_prompt(const monster* mon, bool beam_attack,
                        coord_def beam_target, bool *prompted,
                        coord_def attack_pos, bool check_only)
{
    ASSERT(mon); // XXX: change to const monster &mon
    bool penance = false;

    if (prompted)
        *prompted = false;

    if (crawl_state.disables[DIS_CONFIRMATIONS])
        return false;

    // The player is ordinarily given a different prompt before this if confused,
    // but if we're merely testing if this attack *could* be bad, we should do
    // the full check anyway.
    if ((you.confused() && !check_only) || !you.can_see(*mon))
        return false;

    string adj, suffix;
    if (!bad_attack(mon, adj, suffix, penance, attack_pos))
        return false;

    // We have already determined this attack *would* prompt, so stop here
    if (check_only)
        return true;

    // Listed in the form: "your rat", "Blorkula the orcula".
    string mon_name = mon->name(DESC_PLAIN);
    if (starts_with(mon_name, "the ")) // no "your the Royal Jelly" nor "the the RJ"
        mon_name = mon_name.substr(4); // strlen("the ")
    if (!starts_with(adj, "your"))
        adj = "the " + adj;
    mon_name = adj + mon_name;
    string verb;
    if (beam_attack)
    {
        verb = "fire ";
        if (beam_target == mon->pos())
            verb += "at ";
        else
        {
            verb += "in " + apostrophise(mon_name) + " direction";
            mon_name = "";
        }
    }
    else
        verb = "attack ";

    const string prompt = make_stringf("Really %s%s%s?%s",
             verb.c_str(), mon_name.c_str(), suffix.c_str(),
             penance ? " This attack would place you under penance!" : "");

    if (prompted)
        *prompted = true;

    if (yesno(prompt.c_str(), false, 'n'))
        return false;
    else
    {
        canned_msg(MSG_OK);
        return true;
    }
}

bool stop_attack_prompt(targeter &hitfunc, const char* verb,
                        function<bool(const actor *victim)> affects,
                        bool *prompted, const monster *defender,
                        bool check_only)
{
    if (crawl_state.disables[DIS_CONFIRMATIONS])
        return false;

    if (crawl_state.which_god_acting() == GOD_XOM)
        return false;

    // The player is ordinarily given a different prompt before this if confused,
    // but if we're merely testing if this attack *could* be bad, we should do
    // the full check anyway.
    if (you.confused() && !check_only)
        return false;

    string adj, suffix;
    bool penance = false;
    bool defender_ok = true;
    counted_monster_list victims;
    for (distance_iterator di(hitfunc.origin, false, true, LOS_RADIUS); di; ++di)
    {
        if (hitfunc.is_affected(*di) <= AFF_NO)
            continue;

        const monster* mon = monster_at(*di);
        if (!mon || !you.can_see(*mon))
            continue;

        if (affects && !affects(mon))
            continue;

        string adjn, suffixn;
        bool penancen = false;
        if (bad_attack(mon, adjn, suffixn, penancen))
        {
            // record the adjectives for the first listed, or
            // first that would cause penance
            if (victims.empty() || penancen && !penance)
                adj = adjn, suffix = suffixn, penance = penancen;

            victims.add(mon);

            if (defender && defender == mon)
                defender_ok = false;
        }
    }

    if (victims.empty())
        return false;

    // We have already determined that this attack *would* prompt, so stop here
    if (check_only)
        return true;

    // Listed in the form: "your rat", "Blorkula the orcula".
    string mon_name = victims.describe(DESC_PLAIN);
    if (starts_with(mon_name, "the ")) // no "your the Royal Jelly" nor "the the RJ"
        mon_name = mon_name.substr(4); // strlen("the ")
    if (!starts_with(adj, "your"))
        adj = "the " + adj;
    mon_name = adj + mon_name;

    const string prompt = make_stringf("Really %s%s %s%s?%s",
             verb, defender_ok ? " near" : "", mon_name.c_str(),
             suffix.c_str(),
             penance ? " This attack would place you under penance!" : "");

    if (prompted)
        *prompted = true;

    if (yesno(prompt.c_str(), false, 'n'))
        return false;
    else
    {
        canned_msg(MSG_OK);
        return true;
    }
}

string stop_summoning_reason(resists_t resists, monclass_flags_t flags)
{
    if (get_resist(resists, MR_RES_POISON) <= 0
        && you.duration[DUR_TOXIC_RADIANCE])
    {
        return "toxic aura";
    }
    if (you.duration[DUR_NOXIOUS_BOG] && !(flags & M_FLIES))
        return "noxious bog";
    if (you.duration[DUR_VORTEX])
        return "polar vortex";
    if (you.duration[DUR_FUSILLADE])
        return "fulsome fusillade";
    return "";
}

bool warn_about_bad_targets(spell_type spell, vector<coord_def> targets,
                            function<bool(const monster&)> should_ignore)
{
    return warn_about_bad_targets(spell_title(spell), targets, should_ignore);
}

bool warn_about_bad_targets(const char* source_name, vector<coord_def> targets,
                            function<bool(const monster&)> should_ignore)
{
    vector<const monster*> bad_targets;
    for (coord_def p : targets)
    {
        const monster* mon = monster_at(p);
        if (!mon || never_harm_monster(&you, mon))
            continue;

        if (should_ignore && should_ignore(*mon))
            continue;

        // If we've already found and marked this target as bad, don't include
        // it a second time (or it will produce a confusing prompt).
        if (find(bad_targets.begin(), bad_targets.end(), mon) != bad_targets.end())
            continue;

        string adj, suffix;
        bool penance;
        if (bad_attack(mon, adj, suffix, penance, you.pos()))
            bad_targets.push_back(mon);
    }

    if (bad_targets.empty())
        return false;

    const monster* ex_mon = bad_targets.back();
    string adj, suffix;
    bool penance;
    bad_attack(ex_mon, adj, suffix, penance, you.pos());
    const string and_more = bad_targets.size() > 1 ?
            make_stringf(" (and %zu other bad targets)",
                         bad_targets.size() - 1) : "";
    const string prompt = make_stringf("%s might hit %s%s. Cast it anyway?",
                                       source_name,
                                       ex_mon->name(DESC_THE).c_str(),
                                       and_more.c_str());
    if (!yesno(prompt.c_str(), false, 'n'))
    {
        canned_msg(MSG_OK);
        return true;
    }
    return false;
}

/**
 * Does the player have a hostile duration up that would/could cause
 * a summon to be abjured? If so, prompt the player as to whether they
 * want to continue to create their summon. Note that this prompt is never a
 * penance prompt, because we don't cause penance when monsters enter line of
 * sight when OTR is active, regardless of how they entered LOS.
 *
 * @param resists   What does the summon resist?
 * @param flags     What relevant flags does the summon have? (e.g. flight)
 * @param verb      The verb to be used in the prompt.
 * @return          True if the player wants to abort.
 */
bool stop_summoning_prompt(resists_t resists, monclass_flags_t flags,
                           string verb)
{
    if (crawl_state.disables[DIS_CONFIRMATIONS]
        || crawl_state.which_god_acting() == GOD_XOM)
    {
        return false;
    }

    const string noun = stop_summoning_reason(resists, flags);
    if (noun.empty())
        return false;

    string prompt = make_stringf("Really %s while emitting %s?",
                                 verb.c_str(), article_a(noun).c_str());

    if (yesno(prompt.c_str(), false, 'n'))
        return false;

    canned_msg(MSG_OK);
    return true;
}

bool can_reach_attack_between(coord_def source, coord_def target,
                              reach_type range)
{
    // The foe should be on the map (not stepped from time).
    if (!in_bounds(target))
        return false;

    const coord_def delta(target - source);
    const int grid_distance(delta.rdist());

    // Unrand only - Rift is smite-targeted and up to 3 range.
    if (range == REACH_THREE)
    {
        return cell_see_cell(source, target, LOS_NO_TRANS)
               && grid_distance > 1 && grid_distance <= range;
    }

    const coord_def first_middle(source + delta / 2);
    const coord_def second_middle(target - delta / 2);

    return grid_distance == range
           // And with no dungeon furniture in the way of the reaching attack.
           && (feat_is_reachable_past(env.grid(first_middle))
               || feat_is_reachable_past(env.grid(second_middle)));
}

dice_def spines_damage(monster_type mon)
{
    if (mon == MONS_CACTUS_GIANT)
        return dice_def(5, 8);
    return dice_def(5, 4);
}

int archer_bonus_damage(int hd)
{
    return hd * 4 / 3;
}

/**
 * Do weapons that use the given skill use strength or dex to increase damage?
 */
bool weapon_uses_strength(skill_type wpn_skill, bool using_weapon)
{
    if (!using_weapon)
        return true;
    switch (wpn_skill)
    {
    case SK_LONG_BLADES:
    case SK_SHORT_BLADES:
    case SK_RANGED_WEAPONS:
        return false;
    default:
        return true;
    }
}

/**
 * Apply the player's attributes to multiply damage dealt with the given weapon skill.
 */
int stat_modify_damage(int damage, skill_type wpn_skill, bool using_weapon)
{
    // At 10 strength, damage is multiplied by 1.0
    // Each point of strength over 10 increases this by 0.025 (2.5%),
    // strength below 10 reduces the multiplied by the same amount.
    // Minimum multiplier is 0.01 (1%) (reached at -30 str).
    // Ranged weapons and short/long blades use dex instead.
    const bool use_str = weapon_uses_strength(wpn_skill, using_weapon);
    const int attr = use_str ? you.strength() : you.dex();
    damage *= max(1.0, 75 + 2.5 * attr);
    damage /= 100;

    return damage;
}

int apply_weapon_skill(int damage, skill_type wpn_skill, bool random)
{
    const int sklvl = you.skill(wpn_skill, 100);
    damage *= 2500 + maybe_random2(sklvl + 1, random);
    damage /= 2500;
    return damage;
}

int apply_fighting_skill(int damage, bool aux, bool random)
{
    const int base = aux? 40 : 30;
    const int sklvl = you.skill(SK_FIGHTING, 100);

    damage *= base * 100 + maybe_random2(sklvl + 1, random);
    damage /= base * 100;

    return damage;
}

int throwing_base_damage_bonus(const item_def &proj, bool random)
{
    // Stones get half bonus; everything else gets full bonus.
    int damage_mult = min(4, property(proj, PWPN_DAMAGE));
    if (random)
        return div_rand_round(you.skill_rdiv(SK_THROWING) * damage_mult, 4);
    return (you.skill(SK_THROWING) * damage_mult) / 4;
}

int brand_adjust_weapon_damage(int base_dam, int brand, bool random)
{
    if (brand != SPWPN_HEAVY)
        return base_dam;
    if (random)
        return div_rand_round(base_dam * 9, 5);
    return base_dam * 9 / 5;
}

int unarmed_base_damage(bool random)
{
    int damage = get_form()->get_base_unarmed_damage(random);

    if (you.has_usable_claws())
        damage += you.has_claws() * 2;

    if (you.form_uses_xl())
    {
        damage += random ? div_rand_round(you.experience_level, 3)
                         : you.experience_level / 3;
    }

    return damage;
}

int unarmed_base_damage_bonus(bool random)
{
    if (you.form_uses_xl())
        return 0;
    if (random)
        return you.skill_rdiv(SK_UNARMED_COMBAT);
    return you.skill(SK_UNARMED_COMBAT);
}
