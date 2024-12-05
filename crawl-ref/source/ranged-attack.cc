/**
 * @file
 * @brief ranged_attack class and associated ranged_attack methods
 */

#include "AppHdr.h"

#include "ranged-attack.h"

#include "areas.h"
#include "chardump.h"
#include "coord.h"
#include "english.h"
#include "env.h"
#include "fight.h"
#include "fprop.h"
#include "god-conduct.h"
#include "item-prop.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-util.h"
#include "monster.h"
#include "player.h"
#include "stringutil.h"
#include "teleport.h"
#include "throw.h"
#include "traps.h"
#include "unwind.h"
#include "xom.h"

ranged_attack::ranged_attack(actor *attk, actor *defn,
                             const item_def *wpn, const item_def *proj,
                             bool tele, actor *blame, bool mulch)
    : ::attack(attk, defn, blame), range_used(0), reflected(false),
      projectile(proj), teleport(tele), mulched(mulch)
{
    weapon = wpn;
    if (weapon)
        damage_brand = get_weapon_brand(*weapon);

    init_attack(SK_THROWING, 0);
    kill_type = KILLED_BY_BEAM;

    string proj_name = projectile->name(DESC_PLAIN);
    // init launch type early, so we can use it later in the constructor

    // [dshaligram] When changing bolt names here, you must edit
    // hiscores.cc (scorefile_entry::terse_missile_cause()) to match.
    if (attacker->is_player())
    {
        kill_type = KILLED_BY_SELF_AIMED;
        aux_source = proj_name;
    }
    else if (throwing())
    {
        aux_source = make_stringf("Hit by a%s %s thrown by %s",
                 (is_vowel(proj_name[0]) ? "n" : ""), proj_name.c_str(),
                 attacker->name(DESC_A).c_str());
    }
    else
    {
        aux_source = make_stringf("Shot with a%s %s by %s",
                 (is_vowel(proj_name[0]) ? "n" : ""), proj_name.c_str(),
                 attacker->name(DESC_A).c_str());
    }

    needs_message = defender_visible;
}

int ranged_attack::post_roll_to_hit_modifiers(int mhit, bool random)
{
    int modifiers = attack::post_roll_to_hit_modifiers(mhit, random);

    if (teleport && attacker->is_monster())
        modifiers += attacker->as_monster()->get_hit_dice() * 3 / 2;
    // Duplicated in melee.cc _to_hit_hit_chance
    else if (defender && attacker->is_player()
             && you.duration[DUR_DIMENSIONAL_BULLSEYE]
             && (mid_t)you.props[BULLSEYE_TARGET_KEY].get_int()
                 == defender->mid)
    {
        modifiers += maybe_random2_div(
                         calc_spell_power(SPELL_DIMENSIONAL_BULLSEYE),
                         BULLSEYE_TO_HIT_DIV, random);
    }

    return modifiers;
}

bool ranged_attack::attack()
{
    if (!handle_phase_attempted())
        return false;

    // XXX: Can this ever happen?
    if (!defender->alive())
    {
        handle_phase_killed();
        handle_phase_end();
        return true;
    }

    int ev = defender->evasion(false, attacker);

    // Works even if the defender is incapacitated
    if (defender->missile_repulsion())
        ev += REPEL_MISSILES_EV_BONUS;

    ev_margin = test_hit(to_hit, ev, !attacker->is_player());
    bool shield_blocked = attack_shield_blocked(false);

    god_conduct_trigger conducts[3];
    if (attacker->is_player() && attacker != defender)
    {
        set_attack_conducts(conducts, *defender->as_monster(),
                            you.can_see(*defender));
    }

    if (shield_blocked)
        handle_phase_blocked();
    else
    {
        if (ev_margin >= 0)
        {
            if (!paragon_defends_player() && !handle_phase_hit())
            {
                if (!defender->alive())
                    handle_phase_killed();
                handle_phase_end();
                return false;
            }
        }
        else
            handle_phase_dodged();
    }

    // Don't crash on banishment (from eg chaos).
    if (!defender->pos().origin())
        handle_noise(defender->pos());

    alert_defender();

    if (!defender->alive())
        handle_phase_killed();

    if (attacker->is_player() && defender->is_monster()
        && !shield_blocked && ev_margin >= 0)
    {
        print_wounds(*defender->as_monster());
    }

    handle_phase_end();

    return attack_occurred;
}

// XXX: Are there any cases where this might fail?
bool ranged_attack::handle_phase_attempted()
{
    attacker->attacking(defender);
    attack_occurred = true;

    return true;
}

bool ranged_attack::handle_phase_blocked()
{
    ASSERT(!ignores_shield(false));
    string punctuation = ".";

    if (defender->reflection())
    {
        reflected = true;
        if (defender->observable())
        {
            if (defender_shield && shield_reflects(*defender_shield))
            {
                punctuation = " with " + defender->pronoun(PRONOUN_POSSESSIVE)
                              + " " + defender_shield->name(DESC_PLAIN).c_str();
            }
            else
                punctuation = " with an invisible shield";
        }

        punctuation += make_stringf("... and %s it back!",
                                    defender->conj_verb("reflect").c_str());
    }
    else
        range_used = BEAM_STOP;

    if (needs_message)
    {
        mprf("%s %s %s%s",
             defender_name(false).c_str(),
             defender->conj_verb("block").c_str(),
             projectile->name(DESC_THE).c_str(),
             punctuation.c_str());
    }

    if (!projectile->is_type(OBJ_MISSILES, MI_DART)
        && !projectile->is_type(OBJ_MISSILES, MI_THROWING_NET))
    {
        maybe_trigger_jinxbite();
    }

    return attack::handle_phase_blocked();
}

bool ranged_attack::handle_phase_dodged()
{
    did_hit = false;

    if (defender->missile_repulsion() && ev_margin > -REPEL_MISSILES_EV_BONUS)
    {
        if (needs_message && defender_visible)
            mprf("%s is repelled.", projectile->name(DESC_THE).c_str());

        if (defender->is_player())
            count_action(CACT_DODGE, DODGE_REPEL);

        return true;
    }

    if (defender->is_player())
        count_action(CACT_DODGE, DODGE_EVASION);

    if (needs_message)
    {
        mprf("%s%s misses %s.",
             projectile->name(DESC_THE).c_str(),
             evasion_margin_adverb().c_str(),
             defender_name(false).c_str());
    }

    if (!projectile->is_type(OBJ_MISSILES, MI_DART)
        && !projectile->is_type(OBJ_MISSILES, MI_THROWING_NET))
    {
        maybe_trigger_jinxbite();
    }

    maybe_trigger_autodazzler();

    return true;
}

static bool _jelly_eat_missile(const item_def& projectile, int damage_done)
{
    if (you.has_mutation(MUT_JELLY_MISSILE)
        && you.hp < you.hp_max
        && !you.duration[DUR_DEATHS_DOOR]
        && item_is_jelly_edible(projectile)
        && !one_chance_in(3))
    {
        mprf("Your attached jelly eats %s!",
             projectile.name(DESC_THE).c_str());
        inc_hp(1 + random2(damage_done));
        canned_msg(MSG_GAIN_HEALTH);
        return true;
    }

    return false;
}

bool ranged_attack::handle_phase_hit()
{
    if (mulch_bonus()
        // XXX: this kind of hijacks the shield block check
        || !is_penetrating_attack(*attacker, weapon, *projectile))
    {
        range_used = BEAM_STOP;
    }

    if (projectile->is_type(OBJ_MISSILES, MI_DART))
    {
        damage_done = dart_duration_roll(get_ammo_brand(*projectile));
        set_attack_verb(0);
        announce_hit();
    }
    else if (projectile->is_type(OBJ_MISSILES, MI_THROWING_NET))
    {
        set_attack_verb(0);
        announce_hit();
        if (defender->is_player())
        {
            player_caught_in_net();
            if (attacker->is_monster())
                xom_is_stimulated(50);
        }
        else
            monster_caught_in_net(defender->as_monster());
    }
    else
    {
        damage_done = calc_damage();
        if (damage_done > 0)
        {
            if (!handle_phase_damaged())
                return false;
            // Jiyva mutation - piercing projectiles won't keep going if they
            // get eaten.
            if (attacker->is_monster()
                && defender->is_player()
                && !you.pending_revival
                && _jelly_eat_missile(*projectile, damage_done))
            {
                range_used = BEAM_STOP;
            }
        }
        else
        {
            if (needs_message)
            {
                mprf("%s %s %s%s but does no damage.",
                    projectile->name(DESC_THE).c_str(),
                    attack_verb.c_str(),
                    defender->name(DESC_THE).c_str(),
                    mulch_bonus() ? " and shatters," : "");
            }
        }

        maybe_trigger_jinxbite();
    }

    // XXX: unify this with melee_attack's code
    if (attacker->is_player() && defender->is_monster())
    {
        behaviour_event(defender->as_monster(), ME_WHACK, attacker,
                        coord_def());
    }

    return true;
}

bool ranged_attack::throwing() const
{
    return SK_THROWING == wpn_skill;
}

bool ranged_attack::using_weapon() const
{
    return weapon && !throwing();
}

bool ranged_attack::clumsy_throwing() const
{
    return throwing() && !is_throwable(attacker, *projectile);
}

int ranged_attack::weapon_damage() const
{
    if (clumsy_throwing())
        return 0;

    int dam = property(*projectile, PWPN_DAMAGE);
    if (using_weapon())
        dam += property(*weapon, PWPN_DAMAGE);
    else if (attacker->is_player())
        dam += calc_base_unarmed_damage();

    return dam;
}

/**
 * For ranged attacked, "unarmed" is throwing damage.
 */
int ranged_attack::calc_base_unarmed_damage() const
{
    if (clumsy_throwing())
        return 0;
    return throwing_base_damage_bonus(*projectile, true);
}

int ranged_attack::calc_mon_to_hit_base()
{
    ASSERT(attacker->is_monster());
    return mon_to_hit_base(attacker->get_hit_dice(), attacker->as_monster()->is_archer());
}

bool ranged_attack::apply_damage_brand(const char *what)
{
    (void) what;

    if ((!using_weapon() && !throwing())
        || (defender->is_player() && you.pending_revival))
    {
        return false;
    }

    if (using_weapon()
        && attack::apply_damage_brand(projectile->name(DESC_THE).c_str()))
    {
        return true;
    }
    if (using_weapon() && testbits(weapon->flags, ISFLAG_CHAOTIC)
        && defender->alive())
    {
        unwind_var<brand_type> save_brand(damage_brand);
        damage_brand = SPWPN_CHAOS;
        if (attack::apply_damage_brand(projectile->name(DESC_THE).c_str()))
            return true;
    }

    return apply_missile_brand();
}

int ranged_attack::apply_damage_modifiers(int damage)
{
    ASSERT(attacker->is_monster());

    if (attacker->as_monster()->has_ench(ENCH_TOUCH_OF_BEOGH))
        damage = damage * 4 / 3;

    if (attacker->as_monster()->is_archer())
    {
        const int bonus = archer_bonus_damage(attacker->get_hit_dice());
        damage += random2avg(bonus, 2);
    }
    return damage;
}

int ranged_attack::player_apply_final_multipliers(int damage, bool /*aux*/)
{
    if (!throwing())
        damage = apply_rev_penalty(damage);
    return damage;
}

bool ranged_attack::mulch_bonus() const
{
    return mulched
        && throwing()
        && projectile
        && ammo_type_damage(projectile->sub_type)
        && projectile->sub_type != MI_STONE;
}

int ranged_attack::player_apply_postac_multipliers(int damage)
{
    if (mulch_bonus())
        return div_rand_round(damage * 4, 3);
    return damage;
}

bool ranged_attack::ignores_shield(bool verbose)
{
    if (defender->is_player() && player_omnireflects())
        return false;

    if (is_penetrating_attack(*attacker, weapon, *projectile))
    {
        if (verbose)
        {
            mprf("%s pierces through %s %s!",
                 projectile->name(DESC_THE).c_str(),
                 apostrophise(defender_name(false)).c_str(),
                 is_shield(defender_shield) ? defender_shield->name(DESC_PLAIN).c_str()
                                            : "shielding");
        }
        return true;
    }
    return false;
}

special_missile_type ranged_attack::random_chaos_missile_brand()
{
    special_missile_type brand = SPMSL_NORMAL;
    // Assuming chaos always wants to be flashy and fancy, and thus
    // skip anything that'd be completely ignored by resists.
    // FIXME: Unite this with chaos melee's chaos_types.
    while (true)
    {
        brand = (random_choose_weighted(
                    10, SPMSL_FLAME,
                    10, SPMSL_FROST,
                    10, SPMSL_POISONED,
                    10, SPMSL_CHAOS,
                    10, SPMSL_BLINDING,
                     5, SPMSL_FRENZY,
                     2, SPMSL_CURARE,
                     2, SPMSL_DISPERSAL));

        if (one_chance_in(3))
            break;

        bool susceptible = true;
        switch (brand)
        {
        case SPMSL_FLAME:
            if (!defender->is_player() && defender->res_fire() >= 3)
                susceptible = false;
            break;
        case SPMSL_FROST:
            if (!defender->is_player() && defender->res_cold() >= 3)
                susceptible = false;
            break;
        case SPMSL_POISONED:
        case SPMSL_BLINDING:
            if (defender->holiness() & (MH_UNDEAD | MH_NONLIVING))
                susceptible = false;
            break;
        case SPMSL_CURARE:
            if ((defender->is_player() && defender->holiness() & (MH_UNDEAD | MH_NONLIVING))
               || defender->res_poison() > 0)
            {
                susceptible = false;
            }
            break;
        case SPMSL_DISPERSAL:
            if (defender->no_tele(true))
                susceptible = false;
            break;
        case SPMSL_FRENZY:
            if (defender->holiness() & (MH_UNDEAD | MH_NONLIVING)
                || defender->is_player()
                   && !you.can_go_berserk(false, false, false)
                || defender->is_monster()
                   && !defender->as_monster()->can_go_frenzy())
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
    string brand_name = "CHAOS missile: ";
    switch (brand)
    {
    case SPMSL_NORMAL:          brand_name += "(plain)"; break;
    case SPMSL_FLAME:           brand_name += "flame"; break;
    case SPMSL_FROST:           brand_name += "frost"; break;
    case SPMSL_POISONED:        brand_name += "poisoned"; break;
    case SPMSL_CURARE:          brand_name += "curare"; break;
    case SPMSL_CHAOS:           brand_name += "chaos"; break;
    case SPMSL_DISPERSAL:       brand_name += "dispersal"; break;
    case SPMSL_FRENZY:          brand_name += "frenzy"; break;
    case SPMSL_BLINDING:        brand_name += "blinding"; break;
    default:                    brand_name += "(other)"; break;
    }

    // Pretty much duplicated by the chaos effect note,
    // which will be much more informative.
    if (brand != SPMSL_CHAOS)
        take_note(Note(NOTE_MESSAGE, 0, 0, brand_name), true);
#endif
    return brand;
}

bool ranged_attack::dart_check(special_missile_type type)
{
    if (defender->holiness() & (MH_UNDEAD | MH_NONLIVING))
    {
        if (needs_message)
        {
            if (defender->is_monster())
            {
                simple_monster_message(*defender->as_monster(),
                                       " is unaffected.");
            }
            else
                canned_msg(MSG_YOU_UNAFFECTED);
        }
        return false;
    }

    if (attacker->is_monster())
    {
        int chance = 85 - ((defender->get_hit_dice()
                            - attacker->get_hit_dice()) * 5 / 2);
        chance = min(95, chance);

        if (type == SPMSL_FRENZY)
            chance = chance / 2;

        return x_chance_in_y(chance, 100);
    }

    const int pow = (2 * (you.skill_rdiv(SK_THROWING)
                          + you.skill_rdiv(SK_STEALTH))) / 3;

    // You have a really minor chance of hitting weak things no matter what
    if (defender->get_hit_dice() < 15 && random2(100) <= 2)
        return true;

    const int resist_roll = 2 + random2(4 + pow);

    dprf(DIAG_COMBAT, "Brand rolled %d against defender HD: %d.",
         resist_roll, defender->get_hit_dice());

    if (resist_roll < defender->get_hit_dice())
    {
        if (needs_message)
        {
            if (defender->is_monster())
                simple_monster_message(*defender->as_monster(), " resists.");
            else
                canned_msg(MSG_YOU_RESIST);
        }
        return false;
    }

    return true;

}

int ranged_attack::dart_duration_roll(special_missile_type type)
{
    // Leaving monster poison the same by separating it from player poison
    if (type == SPMSL_POISONED && attacker->is_monster())
        return 6 + random2(8);

    if (type == SPMSL_CURARE)
        return 2;

    const int base_power = (attacker->is_monster())
                           ? attacker->get_hit_dice()
                           : attacker->skill_rdiv(SK_THROWING);

    // Scale down nastier dart effects against players.
    // Fixed duration regardless of power, since power already affects success
    // chance considerably, and this helps avoid effects being too nasty from
    // high HD shooters and too ignorable from low ones.
    if (defender->is_player())
        return 5 + random2(5);
    if (type == SPMSL_POISONED) // Player poison needles
        return random2(3 + base_power * 2);
    return 5 + random2(base_power);
}

bool ranged_attack::apply_missile_brand()
{
    if (projectile->base_type != OBJ_MISSILES)
        return false;

    special_damage = 0;
    special_missile_type brand = get_ammo_brand(*projectile);
    if (brand == SPMSL_CHAOS)
        brand = random_chaos_missile_brand();

    switch (brand)
    {
    default:
        break;
    case SPMSL_FLAME:
        calc_elemental_brand_damage(BEAM_FIRE,
                                    defender->is_icy() ? "melt" : "burn",
                                    projectile->name(DESC_THE).c_str());

        defender->expose_to_element(BEAM_FIRE, 2);
        if (defender->is_player())
            maybe_melt_player_enchantments(BEAM_FIRE, special_damage);
        break;
    case SPMSL_FROST:
        calc_elemental_brand_damage(BEAM_COLD, "freeze",
                                    projectile->name(DESC_THE).c_str());
        defender->expose_to_element(BEAM_COLD, 2);
        break;
    case SPMSL_POISONED:
        if (projectile->is_type(OBJ_MISSILES, MI_DART)
                && damage_done > 0
            || !one_chance_in(4))
        {
            int old_poison;

            if (defender->is_player())
                old_poison = you.duration[DUR_POISONING];
            else
            {
                old_poison =
                    (defender->as_monster()->get_ench(ENCH_POISON)).degree;
            }

            defender->poison(attacker,
                             projectile->is_type(OBJ_MISSILES, MI_DART)
                             ? damage_done
                             : 6 + random2(8) + random2(damage_done * 3 / 2));

            if (defender->is_player()
                   && old_poison < you.duration[DUR_POISONING]
                || !defender->is_player()
                   && old_poison <
                      (defender->as_monster()->get_ench(ENCH_POISON)).degree)
            {
                obvious_effect = true;
            }

        }
        break;
    case SPMSL_CURARE:
        obvious_effect = curare_actor(attacker, defender,
                                      projectile->name(DESC_PLAIN),
                                      atk_name(DESC_A));
        break;
    case SPMSL_CHAOS:
        obvious_effect = chaos_affects_actor(defender, attacker);
        break;
    case SPMSL_DISPERSAL:
    {
        if (defender->no_tele())
        {
            if (defender->is_player())
                canned_msg(MSG_STRANGE_STASIS);
            break;
        }

        if (defender->is_player())
        {
            if (attacker->is_monster())
                blink_player_away(attacker->as_monster());
            // Specifically to handle reflected darts shot by the player
            else
                you.blink();
        }
        else
            blink_away(defender->as_monster(), attacker);
        break;
    }
    case SPMSL_DISJUNCTION:
    {
        if (defender->no_tele())
        {
            if (defender->is_player())
                canned_msg(MSG_STRANGE_STASIS);
            break;
        }

        if (defender->is_player())
        {
            mprf(MSGCH_WARN, "You become untethered in space!");
            you.duration[DUR_BLINKITIS] = random_range(30, 40);
            you.props[BLINKITIS_SOURCE_KEY] = attacker->name(DESC_A, true);
            you.props[BLINKITIS_AUX_KEY] = projectile->name(DESC_PLAIN);
        }
        else
        {
            monster* dmon = defender->as_monster();
            if (!dmon->has_ench(ENCH_BLINKITIS))
            {
                simple_monster_message(*dmon, " becomes untethered in space!");
                dmon->add_ench(mon_enchant(ENCH_BLINKITIS, 0, attacker,
                                           random_range(3, 4) * BASELINE_DELAY));
                // Trigger immediately once so that monster can't make an attack
                // before it activates.
                blink_away(dmon, attacker, false, false, 3);
                dmon->hurt(attacker, roll_dice(2, 2));
            }
        }
        break;
    }
    case SPMSL_SILVER:
        special_damage = max(1 + random2(damage_done) / 3,
                             silver_damages_victim(defender, damage_done,
                                                   special_damage_message));
        break;
    case SPMSL_BLINDING:
        if (!dart_check(brand))
            break;
        if (defender->can_be_blinded())
        {
            if (defender->is_player())
                blind_player(damage_done, LIGHTGREEN);
            else
            {
                monster* mon = defender->as_monster();
                mon->add_ench(mon_enchant(ENCH_BLIND, 1, attacker,
                       damage_done * BASELINE_DELAY));
            }
        }
        defender->confuse(attacker, damage_done / 3);
        break;
    case SPMSL_FRENZY:
        if (!dart_check(brand))
            break;
        if (defender->is_monster())
        {
            monster* mon = defender->as_monster();
            // Wake up the monster so that it can frenzy.
            if (mon->behaviour == BEH_SLEEP)
                mon->behaviour = BEH_WANDER;
            mon->go_frenzy(attacker);
        }
        else
            defender->go_berserk(false);
        break;
    }

    if (needs_message && !special_damage_message.empty())
    {
        mpr(special_damage_message);

        special_damage_message.clear();
    }

    if (special_damage > 0)
        inflict_damage(special_damage, special_damage_flavour);

    return !defender->alive();
}

bool ranged_attack::check_unrand_effects()
{
    return false;
}

bool ranged_attack::mons_attack_effects()
{
    return true;
}

void ranged_attack::player_stab_check()
{
    stab_attempt = false;
    stab_bonus = 0;
}

bool ranged_attack::player_good_stab()
{
    return false;
}

void ranged_attack::set_attack_verb(int/* damage*/)
{
    attack_verb = !mulch_bonus() && is_penetrating_attack(*attacker, weapon, *projectile) ? "pierces through" : "hits";
}

void ranged_attack::announce_hit()
{
    if (!needs_message)
        return;

    mprf("%s %s %s%s%s%s",
         projectile->name(DESC_THE).c_str(),
         attack_verb.c_str(),
         defender_name(false).c_str(),
         mulch_bonus() ? " and shatters for extra damage" : "",
         debug_damage_number().c_str(),
         attack_strength_punctuation(damage_done).c_str());
}
