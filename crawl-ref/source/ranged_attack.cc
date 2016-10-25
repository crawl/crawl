/**
 * @file
 * @brief ranged_attack class and associated ranged_attack methods
 */

#include "AppHdr.h"

#include "ranged_attack.h"

#include "areas.h"
#include "chardump.h"
#include "coord.h"
#include "english.h"
#include "env.h"
#include "fprop.h"
#include "godconduct.h"
#include "itemprop.h"
#include "message.h"
#include "mon-behv.h"
#include "mon-util.h"
#include "monster.h"
#include "player.h"
#include "stringutil.h"
#include "teleport.h"
#include "throw.h"
#include "traps.h"

ranged_attack::ranged_attack(actor *attk, actor *defn, item_def *proj,
                             bool tele, actor *blame)
    : ::attack(attk, defn, blame), range_used(0), reflected(false),
      projectile(proj), teleport(tele), orig_to_hit(0),
      should_alert_defender(true), launch_type(LRET_BUGGY)
{
    init_attack(SK_THROWING, 0);
    kill_type = KILLED_BY_BEAM;

    string proj_name = projectile->name(DESC_PLAIN);
    // init launch type early, so we can use it later in the constructor
    launch_type = is_launched(attacker, weapon, *projectile);

    // [dshaligram] When changing bolt names here, you must edit
    // hiscores.cc (scorefile_entry::terse_missile_cause()) to match.
    if (attacker->is_player())
    {
        kill_type = KILLED_BY_SELF_AIMED;
        aux_source = proj_name;
    }
    else if (launch_type == LRET_LAUNCHED)
    {
        aux_source = make_stringf("Shot with a%s %s by %s",
                 (is_vowel(proj_name[0]) ? "n" : ""), proj_name.c_str(),
                 attacker->name(DESC_A).c_str());
    }
    else
    {
        aux_source = make_stringf("Hit by a%s %s thrown by %s",
                 (is_vowel(proj_name[0]) ? "n" : ""), proj_name.c_str(),
                 attacker->name(DESC_A).c_str());
    }

    needs_message = defender_visible;

    if (!using_weapon())
        wpn_skill = SK_THROWING;
}

int ranged_attack::calc_to_hit(bool random)
{
    orig_to_hit = attack::calc_to_hit(random);

    if (orig_to_hit == AUTOMATIC_HIT)
        return AUTOMATIC_HIT;

    if (teleport)
    {
        orig_to_hit +=
            (attacker->is_player())
            ? maybe_random2(you.attribute[ATTR_PORTAL_PROJECTILE] / 4, random)
            : 3 * attacker->as_monster()->get_hit_dice();
    }

    int hit = orig_to_hit;
    const int defl = defender->missile_deflection();
    if (defl)
    {
        if (random)
            hit = random2(hit / defl);
        else
            hit = (hit - 1) / (2 * defl);
    }

    return hit;
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

    const int ev = defender->evasion(EV_IGNORE_NONE, attacker);
    ev_margin = test_hit(to_hit, ev, !attacker->is_player());
    bool shield_blocked = attack_shield_blocked(false);

    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    if (attacker->is_player() && attacker != defender)
        set_attack_conducts(conducts, defender->as_monster());

    if (shield_blocked)
        handle_phase_blocked();
    else
    {
        if (ev_margin >= 0)
        {
            if (!handle_phase_hit())
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

    if (env.sanctuary_time > 0 && attack_occurred
        && (is_sanctuary(attacker->pos()) || is_sanctuary(defender->pos()))
        && (attacker->is_player() || attacker->as_monster()->friendly()
                                     && !attacker->confused()))
    {
        remove_sanctuary(true);
    }

    if (should_alert_defender)
        alert_defender();

    if (!defender->alive())
        handle_phase_killed();

    if (attacker->is_player() && defender->is_monster()
        && !shield_blocked && ev_margin >= 0)
    {
        print_wounds(*defender->as_monster());
    }

    handle_phase_end();

    enable_attack_conducts(conducts);

    return attack_occurred;
}

// XXX: Are there any cases where this might fail?
bool ranged_attack::handle_phase_attempted()
{
    attacker->attacking(defender, true);
    attack_occurred = true;

    return true;
}

bool ranged_attack::handle_phase_blocked()
{
    ASSERT(!ignores_shield(false));
    string punctuation = ".";
    string verb = "block";

    const bool reflected_by_shield = defender_shield
                                     && is_shield(*defender_shield)
                                     && shield_reflects(*defender_shield);
    if (reflected_by_shield || defender->reflection())
    {
        reflected = true;
        verb = "reflect";
        if (defender->observable())
        {
            if (reflected_by_shield)
            {
                punctuation = " off " + defender->pronoun(PRONOUN_POSSESSIVE)
                              + " " + defender_shield->name(DESC_PLAIN).c_str()
                              + "!";
                ident_reflector(defender_shield);
            }
            else
            {
                punctuation = " off an invisible shield around "
                            + defender->pronoun(PRONOUN_OBJECTIVE) + "!";

                item_def *amulet = defender->slot_item(EQ_AMULET, false);
                if (amulet)
                   ident_reflector(amulet);
            }
        }
        else
            punctuation = "!";
    }
    else
        range_used = BEAM_STOP;

    if (needs_message)
    {
        mprf("%s %s %s%s",
             defender_name(false).c_str(),
             defender->conj_verb(verb).c_str(),
             projectile->name(DESC_THE).c_str(),
             punctuation.c_str());
    }

    return attack::handle_phase_blocked();
}

bool ranged_attack::handle_phase_dodged()
{
    did_hit = false;

    const int ev = defender->evasion(EV_IGNORE_NONE, attacker);

    const int orig_ev_margin =
        test_hit(orig_to_hit, ev, !attacker->is_player());

    if (defender->missile_deflection() && orig_ev_margin >= 0)
    {
        if (needs_message && defender_visible)
        {
            if (defender->missile_deflection() >= 2)
            {
                mprf("%s %s %s!",
                     defender->name(DESC_THE).c_str(),
                     defender->conj_verb("deflect").c_str(),
                     projectile->name(DESC_THE).c_str());
            }
            else
                mprf("%s is repelled.", projectile->name(DESC_THE).c_str());

            defender->ablate_deflection();
        }

        if (defender->is_player())
            count_action(CACT_DODGE, DODGE_DEFLECT);

        return true;
    }

    if (defender->is_player())
        count_action(CACT_DODGE, DODGE_EVASION);

    if (needs_message)
    {
        mprf("%s%s misses %s%s",
             projectile->name(DESC_THE).c_str(),
             evasion_margin_adverb().c_str(),
             defender_name(false).c_str(),
             attack_strength_punctuation(damage_done).c_str());
    }

    return true;
}

bool ranged_attack::handle_phase_hit()
{
    // XXX: this kind of hijacks the shield block check
    if (!is_penetrating_attack(*attacker, weapon, *projectile))
        range_used = BEAM_STOP;

    if (projectile->is_type(OBJ_MISSILES, MI_DART_POISONED)
        || projectile->is_type(OBJ_MISSILES, MI_DART_CURARE)
        || projectile->is_type(OBJ_MISSILES, MI_DART_FRENZY))
    {
        damage_done = dart_duration_roll(projectile->sub_type);
        set_attack_verb(0);
        announce_hit();
    }
    else if (projectile->is_type(OBJ_MISSILES, MI_THROWING_NET))
    {
        set_attack_verb(0);
        announce_hit();
        if (defender->is_player())
            player_caught_in_net();
        else
            monster_caught_in_net(defender->as_monster(), attacker);
    }
    else
    {
        damage_done = calc_damage();
        if (damage_done > 0 || projectile->is_type(OBJ_MISSILES, MI_NEEDLE))
        {
            if (!handle_phase_damaged())
                return false;
        }
        else if (needs_message)
        {
            mprf("%s %s %s but does no damage.",
                 projectile->name(DESC_THE).c_str(),
                 attack_verb.c_str(),
                 defender->name(DESC_THE).c_str());
        }
    }

    if (using_weapon() || launch_type == LRET_THROWN)
    {
        if (using_weapon()
            && apply_damage_brand(projectile->name(DESC_THE).c_str()))
        {
            return false;
        }
        if (apply_dart_type())
            return false;
    }

    // XXX: unify this with melee_attack's code
    if (attacker->is_player() && defender->is_monster()
        && should_alert_defender)
    {
        behaviour_event(defender->as_monster(), ME_WHACK, attacker,
                        coord_def());
    }

    return true;
}

bool ranged_attack::using_weapon()
{
    return weapon && (launch_type == LRET_LAUNCHED
                     || launch_type == LRET_BUGGY // not initialized
                         && is_launched(attacker, weapon, *projectile));
}

int ranged_attack::weapon_damage()
{
    if (launch_type == LRET_FUMBLED)
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
int ranged_attack::calc_base_unarmed_damage()
{
    // No damage bonus for throwing non-throwing weapons.
    if (launch_type == LRET_FUMBLED)
        return 0;

    int damage = you.skill_rdiv(wpn_skill);

    // Stones get half bonus; everything else gets full bonus.
    return div_rand_round(damage
                          * min(4, property(*projectile, PWPN_DAMAGE)), 4);
}

int ranged_attack::calc_mon_to_hit_base()
{
    ASSERT(attacker->is_monster());
    const int hd_mult = attacker->as_monster()->is_archer() ? 15 : 9;
    return 18 + attacker->get_hit_dice() * hd_mult / 6;
}

int ranged_attack::apply_damage_modifiers(int damage, int damage_max)
{
    ASSERT(attacker->is_monster());
    if (attacker->as_monster()->is_archer())
    {
        const int bonus = attacker->get_hit_dice() * 4 / 3;
        damage += random2avg(bonus, 2);
    }
    return damage;
}

bool ranged_attack::ignores_shield(bool verbose)
{
    if (is_penetrating_attack(*attacker, weapon, *projectile))
    {
        if (verbose)
        {
            mprf("%s pierces through %s %s!",
                 projectile->name(DESC_THE).c_str(),
                 apostrophise(defender_name(false)).c_str(),
                 defender_shield ? defender_shield->name(DESC_PLAIN).c_str()
                                 : "shielding");
        }
        return true;
    }
    return false;
}

bool ranged_attack::apply_damage_brand(const char *what)
{
    if (!weapon || !is_range_weapon(*weapon))
        return false;

    const brand_type brand = get_weapon_brand(*weapon);

    damage_brand = brand;
    return attack::apply_damage_brand(what);
}

bool ranged_attack::dart_check(uint8_t sub_type)
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

        if (sub_type == MI_DART_FRENZY)
            chance = chance / 2;

        return x_chance_in_y(chance, 100);
    }

    const int skill = you.skill_rdiv(SK_THROWING);

    // You have a really minor chance of hitting with no skills
    if (defender->get_hit_dice() < 15 && random2(100) <= 2)
        return true;

    const int resist_roll = 2 + random2(4 + skill);

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

int ranged_attack::dart_duration_roll(uint8_t sub_type)
{
    // Leaving monster poison the same by separating it from player poison
    if (sub_type == MI_DART_POISONED && attacker->is_monster())
        return 6 + random2(8);

    if (sub_type == MI_DART_CURARE)
        return 2;

    const int base_power = (attacker->is_monster())
                           ? attacker->get_hit_dice()
                           : attacker->skill_rdiv(SK_THROWING);

    const int plus = using_weapon() ? weapon->plus : 0;

    // Scale down nastier needle effects against players.
    // Fixed duration regardless of power, since power already affects success
    // chance considerably, and this helps avoid effects being too nasty from
    // high HD shooters and too ignorable from low ones.
    if (defender->is_player())
        return 5 + random2(5);
    else if (sub_type == MI_DART_POISONED) // Player poison needles
        return random2(3 + base_power * 2 + plus);
    else
        return 5 + random2(base_power + plus);
}

bool ranged_attack::apply_dart_type()
{
    if (projectile->base_type != OBJ_MISSILES)
        return false;

    special_damage = 0;

    switch (projectile->sub_type)
    {
    default:
        break;
    case MI_DART_POISONED:
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
        break;
    case MI_DART_CURARE:
        obvious_effect = curare_actor(attacker, defender,
                                      damage_done,
                                      projectile->name(DESC_PLAIN),
                                      atk_name(DESC_PLAIN));
        break;
    case MI_DART_FRENZY:
        if (!dart_check(projectile->sub_type))
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
    attack_verb = is_penetrating_attack(*attacker, weapon, *projectile) ? "pierces through" : "hits";
}

void ranged_attack::announce_hit()
{
    if (!needs_message)
        return;

    mprf("%s %s %s%s%s",
         projectile->name(DESC_THE).c_str(),
         attack_verb.c_str(),
         defender_name(false).c_str(),
         debug_damage_number().c_str(),
         attack_strength_punctuation(damage_done).c_str());
}
