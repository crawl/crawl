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
                             const item_def *wpn,
                             bool tele, actor *blame)
    : ::attack(attk, defn, blame), range_used(0), reflected(false),
        will_mulch(false), pierce(false),
        teleport(tele), _did_net(false)
{
    ASSERT(wpn && (wpn->base_type == OBJ_MISSILES || is_range_weapon(*wpn)));

    weapon = wpn;
    damage_brand = get_weapon_brand(*weapon);

    init_attack(0);
    kill_type = KILLED_BY_BEAM;

    proj_name = launched_projectile_name(*weapon);

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

void ranged_attack::copy_params_to(ranged_attack &other) const
{
    other.teleport     = teleport;
    other.will_mulch   = will_mulch;
    other.pierce       = pierce;
    other.proj_name    = proj_name;

    attack::copy_params_to(other);
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
    ev += defender->missile_repulsion();

    ev_margin = test_hit(to_hit, ev, !attacker->is_player());
    bool shield_blocked = attack_shield_blocked();

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

    if (!defender->alive())
        handle_phase_killed();

    if (attacker->is_player() && defender->is_monster()
        && !shield_blocked && ev_margin >= 0)
    {
        print_wounds(*defender->as_monster());
    }

    handle_phase_end();

    return true;
}

// XXX: Are there any cases where this might fail?
bool ranged_attack::handle_phase_attempted()
{
    attacker->attacking(defender);
    return true;
}

void ranged_attack::handle_phase_blocked()
{
    ASSERT(!ignores_shield());
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
        mprf("%s %s the %s%s",
             defender_name(false).c_str(),
             defender->conj_verb("block").c_str(),
             proj_name.c_str(),
             punctuation.c_str());
    }

    if (!weapon->is_type(OBJ_MISSILES, MI_DART)
        && !weapon->is_type(OBJ_MISSILES, MI_THROWING_NET))
    {
        maybe_trigger_jinxbite();
    }

    attack::handle_phase_blocked();
}

void ranged_attack::handle_phase_dodged()
{
    did_hit = false;

    if (ev_margin > -defender->missile_repulsion())
    {
        if (needs_message && defender_visible)
            mprf("The %s is repelled.", proj_name.c_str());

        if (defender->is_player())
            count_action(CACT_DODGE, DODGE_REPEL);

        return;
    }

    if (defender->is_player())
        count_action(CACT_DODGE, DODGE_EVASION);

    if (needs_message)
    {
        mprf("The %s%s misses %s.",
             proj_name.c_str(),
             evasion_margin_adverb().c_str(),
             defender_name(false).c_str());
    }

    if (!weapon->is_type(OBJ_MISSILES, MI_DART)
        && !weapon->is_type(OBJ_MISSILES, MI_THROWING_NET))
    {
        maybe_trigger_jinxbite();
    }

    maybe_trigger_autodazzler();
}

static bool _jelly_eat_missile(const string& proj_name, int damage_done)
{
    if (you.has_mutation(MUT_JELLY_MISSILE)
        && you.hp < you.hp_max
        && !you.duration[DUR_DEATHS_DOOR]
        && !one_chance_in(3))
    {
        mprf("Your attached jelly eats the %s!", proj_name.c_str());
        inc_hp(1 + random2(damage_done));
        canned_msg(MSG_GAIN_HEALTH);
        return true;
    }

    return false;
}

bool ranged_attack::handle_phase_hit()
{
    perceived_attack = true;

    if (mulch_bonus()
        // XXX: this kind of hijacks the shield block check
        || !is_piercing())
    {
        range_used = BEAM_STOP;
    }

    if (weapon->is_type(OBJ_MISSILES, MI_DART))
    {
        damage_done = dart_duration_roll(get_ammo_brand(*weapon));
        set_attack_verb(0);
        announce_hit();
    }
    else if (weapon->is_type(OBJ_MISSILES, MI_THROWING_NET))
    {
        set_attack_verb(0);
        announce_hit();
        if (defender->trap_in_net(true))
            _did_net = true;
        if (defender->is_player())
            xom_is_stimulated(50);
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
                && _jelly_eat_missile(proj_name, damage_done))
            {
                range_used = BEAM_STOP;
            }
        }
        else
        {
            if (needs_message)
            {
                mprf("The %s %s %s%s but does no damage.",
                    proj_name.c_str(),
                    attack_verb.c_str(),
                    defender->name(DESC_THE).c_str(),
                    mulch_bonus() ? " and shatters," : "");
            }
        }

        maybe_trigger_jinxbite();
    }

    if (!defender->is_player() || !you.pending_revival)
    {
        if (apply_damage_brand(make_stringf("the %s", proj_name.c_str()).c_str()))
            return false;

        if (testbits(weapon->flags, ISFLAG_CHAOTIC) && defender->alive())
        {
            unwind_var<brand_type> save_brand(damage_brand);
            damage_brand = SPWPN_CHAOS;
            if (apply_damage_brand(make_stringf("the %s", proj_name.c_str()).c_str()))
                return false;
        }

        if ((!defender->is_player() || !you.pending_revival)
            && apply_missile_brand())
        {
            return false;
        }
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
    return true;
}

int ranged_attack::weapon_damage() const
{
    int dam = property(*weapon, PWPN_DAMAGE);

    if (attacker->is_player() && throwing())
        dam += throwing_base_damage_bonus(*weapon, true);

    return dam;
}

int ranged_attack::calc_mon_to_hit_base()
{
    ASSERT(attacker->is_monster());
    return mon_to_hit_base(attacker->get_hit_dice(), attacker->as_monster()->is_archer());
}

int ranged_attack::apply_mon_damage_modifiers(int damage)
{
    ASSERT(attacker->is_monster());

    if (attacker->as_monster()->has_ench(ENCH_TOUCH_OF_BEOGH))
        damage = damage * 4 / 3;

    if (attacker->as_monster()->has_ench(ENCH_FIGMENT))
        damage = damage * 2 / 3;

    if (attacker->as_monster()->is_archer())
    {
        const int bonus = archer_bonus_damage(attacker->get_hit_dice());
        damage += random2avg(bonus, 2);
    }
    if (attacker->as_monster()->wearing_ego(OBJ_ARMOUR, SPARM_SNIPING)
        && defender->incapacitated())
    {
        damage = damage * 3 / 2;
    }
    return damage;
}

int ranged_attack::player_apply_final_multipliers(int damage, bool /*aux*/)
{
    if (!throwing())
        damage = apply_rev_penalty(damage);
    if (you.wearing_ego(OBJ_ARMOUR, SPARM_SNIPING)
        && defender->incapacitated())
    {
        damage = damage * 3 / 2;
    }
    return attack::player_apply_final_multipliers(damage);
}

bool ranged_attack::mulch_bonus() const
{
    return will_mulch
        && throwing()
        && ammo_type_damage(weapon->sub_type)
        && weapon->sub_type != MI_STONE;
}

bool ranged_attack::did_net() const
{
    return _did_net;
}

int ranged_attack::player_apply_postac_multipliers(int damage)
{
    if (mulch_bonus())
        return div_rand_round(damage * 4, 3);
    return damage;
}

bool ranged_attack::ignores_shield()
{
    if (defender->is_player() && player_omnireflects())
        return false;

    return is_piercing();
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
    if (weapon->base_type != OBJ_MISSILES)
        return false;

    special_damage = 0;
    special_missile_type brand = get_ammo_brand(*weapon);
    if (brand == SPMSL_CHAOS)
        brand = random_chaos_missile_brand();

    switch (brand)
    {
    default:
        break;
    case SPMSL_FLAME:
        calc_elemental_brand_damage(BEAM_FIRE,
                                    defender->is_icy() ? "melt" : "burn",
                                    make_stringf("the %s", proj_name.c_str()).c_str());

        defender->expose_to_element(BEAM_FIRE, 2);
        if (defender->is_player())
            maybe_melt_player_enchantments(BEAM_FIRE, special_damage);
        break;
    case SPMSL_FROST:
        calc_elemental_brand_damage(BEAM_COLD, "freeze",
                                    make_stringf("the %s", proj_name.c_str()).c_str());
        defender->expose_to_element(BEAM_COLD, 2, attacker);
        break;
    case SPMSL_POISONED:
        if (weapon->is_type(OBJ_MISSILES, MI_DART)
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

            defender->poison(attacker, damage_done);

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
                                      proj_name,
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
            else
                simple_monster_message(*defender->as_monster(), " is unaffected.");
            break;
        }

        if (defender->is_player())
        {
            mprf(MSGCH_WARN, "You become untethered in space!");
            you.duration[DUR_BLINKITIS] = random_range(30, 40);
            you.props[BLINKITIS_SOURCE_KEY] = attacker->name(DESC_A, true);
            you.props[BLINKITIS_AUX_KEY] = proj_name;
        }
        else
        {
            monster* dmon = defender->as_monster();
            if (!dmon->has_ench(ENCH_BLINKITIS))
            {
                simple_monster_message(*dmon, " becomes untethered in space!");
                dmon->add_ench(mon_enchant(ENCH_BLINKITIS, attacker,
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
        if (!defender->res_blind())
        {
            if (defender->is_player())
                blind_player(damage_done, LIGHTGREEN);
            else
            {
                monster* mon = defender->as_monster();
                mon->add_ench(mon_enchant(ENCH_BLIND, attacker,
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
    attack_verb = !mulch_bonus() && is_piercing() ? "pierces through" : "hits";
}

void ranged_attack::announce_hit()
{
    if (!needs_message)
        return;

    mprf("The %s %s %s%s%s%s",
         proj_name.c_str(),
         attack_verb.c_str(),
         defender_name(false).c_str(),
         mulch_bonus() ? " and shatters for extra damage" : "",
         debug_damage_number().c_str(),
         attack_strength_punctuation(damage_done).c_str());
}

void ranged_attack::set_projectile_prefix(string prefix)
{
    proj_name = prefix + " " + proj_name;
}

string ranged_attack::projectile_name() const
{
    return proj_name;
}

bool ranged_attack::is_piercing() const
{
    return pierce || is_penetrating_attack(*weapon);
}
