/**
 * @file
 * @brief ranged_attack class and associated ranged_attack methods
 */

#include "AppHdr.h"

#include "ranged_attack.h"

#include "actor.h"
#include "beam.h"
#include "exercise.h"
#include "godconduct.h"
#include "itemname.h"
#include "itemprop.h"
#include "mon-behv.h"
#include "monster.h"
#include "player.h"
#include "random.h"

ranged_attack::ranged_attack(actor *attk, actor *defn, item_def *proj,
                             bool tele) :
                             ::attack(attk, defn), range_used(0),
                             projectile(proj), teleport(tele), orig_to_hit(0)
{
    init_attack(SK_THROWING, 0);
    kill_type = KILLED_BY_BEAM;

    string proj_name = projectile->name(DESC_PLAIN);

    // [dshaligram] When changing bolt names here, you must edit
    // hiscores.cc (scorefile_entry::terse_missile_cause()) to match.
    if (attacker->is_player())
    {
        kill_type = KILLED_BY_SELF_AIMED;
        aux_source = proj_name;
    }
    else if(is_launched(attacker, weapon, *projectile) == LRET_LAUNCHED)
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
    if (teleport)
    {
        orig_to_hit +=
            (attacker->is_player())
            ? random2(you.attribute[ATTR_PORTAL_PROJECTILE] / 4)
            : 3 * attacker->as_monster()->hit_dice;
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

    const int ev = defender->melee_evasion(attacker);
    ev_margin = test_hit(to_hit, ev, !attacker->is_player());
    bool shield_blocked = attack_shield_blocked(true);

    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    if (attacker->is_player() && attacker != defender)
    {
        set_attack_conducts(conducts, defender->as_monster());
        player_stab_check();
        if (stab_attempt && stab_bonus > 0)
        {
            ev_margin = AUTOMATIC_HIT;
            shield_blocked = false;
        }
    }

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

    // TODO: sanctuary

    // TODO: adjust_noise

    alert_defender();

    if (!defender->alive())
        handle_phase_killed();

    handle_phase_end();

    enable_attack_conducts(conducts);

    return attack_occurred;
}

// XXX: Are there any cases where this might fail?
bool ranged_attack::handle_phase_attempted()
{
    attacker->attacking(defender);
    attack_occurred = true;

    if (attacker->is_player())
    {
        switch (is_launched(attacker, weapon, *projectile))
        {
            case LRET_LAUNCHED:
                practise(EX_WILL_LAUNCH, wpn_skill);
                break;
            case LRET_THROWN:
                practise(EX_WILL_THROW_MSL);
                break;
            case LRET_FUMBLED:
                practise(EX_WILL_THROW_OTHER);
                break;
        }
    }

    return true;
}

bool ranged_attack::handle_phase_blocked()
{
    range_used = BEAM_STOP;
    return attack::handle_phase_blocked();
}

bool ranged_attack::handle_phase_dodged()
{
    did_hit = false;

    const int ev = defender->melee_evasion(attacker);

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
        }

        return true;
    }

    const int ev_nophase = defender->melee_evasion(attacker,
                                                   EV_IGNORE_PHASESHIFT);
    if (ev_margin + (ev - ev_nophase) > 0)
    {
        if (needs_message && defender_visible)
        {
            mprf("%s momentarily %s out as %s "
                 "passes through %s%s",
                 defender->name(DESC_THE).c_str(),
                 defender->conj_verb("phase").c_str(),
                 projectile->name(DESC_THE).c_str(),
                 defender->pronoun(PRONOUN_OBJECTIVE).c_str(),
                 attack_strength_punctuation().c_str());
        }

        return true;
    }

    if (needs_message)
    {
        mprf("%s%s misses %s%s",
             projectile->name(DESC_THE).c_str(),
             evasion_margin_adverb().c_str(),
             defender_name().c_str(),
             attack_strength_punctuation().c_str());
    }

    return true;
}

bool ranged_attack::handle_phase_hit()
{
    range_used = BEAM_STOP;

    damage_done = calc_damage();
    if (damage_done > 0)
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

    apply_damage_brand();

    // XXX: unify this with melee_attack's code
    if (attacker->is_player())
    {
        behaviour_event(defender->as_monster(), ME_WHACK, attacker,
                        coord_def(), !stab_attempt);
    }

    return true;
}

bool ranged_attack::using_weapon()
{
    return is_launched(attacker, weapon, *projectile) != LRET_FUMBLED;
}

int ranged_attack::weapon_damage()
{
    if (!using_weapon())
        return 0;

    int dam = property(*projectile, PWPN_DAMAGE);
    if (weapon)
        dam += property(*weapon, PWPN_DAMAGE);
    else if (projectile->base_type == OBJ_MISSILES
             && projectile->sub_type == MI_STONE)
    {
        dam /= 2; // Special cases should go die. -- Grunt
    }

    return dam;
}

int ranged_attack::apply_damage_modifiers(int damage, int damage_max,
                                          bool &half_ac)
{
    half_ac = false;
    return damage;
}

bool ranged_attack::attack_ignores_shield(bool verbose)
{
    return false;
}

bool ranged_attack::apply_damage_brand()
{
    return false;
}

bool ranged_attack::check_unrand_effects()
{
    return false;
}

bool ranged_attack::mons_attack_effects()
{
    return true;
}

void ranged_attack::adjust_noise()
{
}

void ranged_attack::set_attack_verb()
{
    attack_verb = "hits";
}

void ranged_attack::announce_hit()
{
    mprf("%s %s %s%s%s%s",
         projectile->name(DESC_THE).c_str(),
         attack_verb.c_str(),
         defender_name().c_str(),
         stab_attempt && stab_bonus > 0 ? " in a vulnerable spot" : "",
         debug_damage_number().c_str(),
         attack_strength_punctuation().c_str());
}
