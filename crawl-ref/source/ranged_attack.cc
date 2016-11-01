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
#include "mgen_data.h"
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

static int calc_damage_for_ijc_weapon(const item_def* weapon)
{
    int potential_damage, damage, damage_plus;

    potential_damage = property(*weapon, PWPN_DAMAGE);

    damage = random2(potential_damage+1);

    // Weapon skill
    damage *= 2500 + (random2(you.skill(weapon_attack_skill(weapon->sub_type))));
    damage /= 2500;

    // Other modifiers
    damage_plus = weapon->plus;
    if (you.duration[DUR_CORROSION])
        damage_plus -= 4 * you.props["corrosion_amount"].get_int();
    damage_plus += slaying_bonus(true);

    damage += (damage_plus > -1) ? (random2(1 + damage_plus))
                                 : (-random2(1 - damage_plus));


    // Double damage for Steel Dragonfly projectiles.
    if (weapon->props.exists(IEOH_JIAN_DRAGONFLY))
        damage *= 2;

    dprf("Ieoh Jian projected weapon damage %d", damage);
    return damage;
}

bool ranged_attack::handle_phase_hit()
{
    // XXX: this kind of hijacks the shield block check
    if (!is_penetrating_attack(*attacker, weapon, *projectile))
        range_used = BEAM_STOP;

    if (projectile->is_type(OBJ_MISSILES, MI_NEEDLE))
    {
        damage_done = blowgun_duration_roll(get_ammo_brand(*projectile));
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
    else if (projectile->base_type == OBJ_WEAPONS && projectile->props.exists(IEOH_JIAN_PROJECTED))
    {
        if (!defender->alive() || defender->type == MONS_IEOH_JIAN_WEAPON) // IJC weapons bounce against each other.
            damage_done = 0;
        else
            damage_done = calc_damage_for_ijc_weapon(projectile);

        // TODO: Lift a bunch of code from melee attacks so projected weapons can deal brand effects.
        damage_done = apply_defender_ac(damage_done);
        damage_done = max(0, damage_done);

        set_attack_verb(0);
        if (damage_done > 0)
        {
            if (!handle_phase_damaged())
               return false;
        }
        else
        {
            mprf("%s %s %s but does no damage.",
                 projectile->name(DESC_THE).c_str(),
                 attack_verb.c_str(),
                 defender->name(DESC_THE).c_str());
        }
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
        if (apply_missile_brand())
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
    if (projectile->base_type == OBJ_MISSILES
        && get_ammo_brand(*projectile) == SPMSL_STEEL)
    {
        if (dam)
            dam = div_rand_round(dam * 13, 10);
        else
            dam += 2;
    }
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

    // No stacking elemental brands.
    if (projectile->base_type == OBJ_MISSILES
        && get_ammo_brand(*projectile) != SPMSL_NORMAL
        && get_ammo_brand(*projectile) != SPMSL_PENETRATION
        && (brand == SPWPN_FLAMING
            || brand == SPWPN_FREEZING
            || brand == SPWPN_HOLY_WRATH
            || brand == SPWPN_ELECTROCUTION
            || brand == SPWPN_VENOM
            || brand == SPWPN_CHAOS))
    {
        return false;
    }

    damage_brand = brand;
    return attack::apply_damage_brand(what);
}

special_missile_type ranged_attack::random_chaos_missile_brand()
{
    special_missile_type brand = SPMSL_NORMAL;
    // Assuming the chaos to be mildly intelligent, try to avoid brands
    // that clash with the most basic resists of the defender,
    // i.e. its holiness.
    while (true)
    {
        brand = (random_choose_weighted(
                    10, SPMSL_FLAME,
                    10, SPMSL_FROST,
                    10, SPMSL_POISONED,
                    10, SPMSL_CHAOS,
                     5, SPMSL_PARALYSIS,
                     5, SPMSL_SLEEP,
                     5, SPMSL_FRENZY,
                     2, SPMSL_CURARE,
                     2, SPMSL_CONFUSION,
                     2, SPMSL_DISPERSAL));

        if (one_chance_in(3))
            break;

        bool susceptible = true;
        switch (brand)
        {
        case SPMSL_FLAME:
            if (defender->is_fiery())
                susceptible = false;
            break;
        case SPMSL_FROST:
            if (defender->is_icy())
                susceptible = false;
            break;
        case SPMSL_POISONED:
            if (defender->holiness() & MH_UNDEAD)
                susceptible = false;
            break;
        case SPMSL_DISPERSAL:
            if (defender->no_tele(true, false, true))
                susceptible = false;
            break;
        case SPMSL_CONFUSION:
            if (defender->holiness() & MH_PLANT)
            {
                susceptible = false;
                break;
            }
            // fall through
        case SPMSL_SLEEP:
        case SPMSL_PARALYSIS:
            if (defender->holiness() & (MH_UNDEAD | MH_NONLIVING))
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
    case SPMSL_SLEEP:           brand_name += "sleep"; break;
    case SPMSL_CONFUSION:       brand_name += "confusion"; break;
    case SPMSL_FRENZY:          brand_name += "frenzy"; break;
    default:                    brand_name += "(other)"; break;
    }

    // Pretty much duplicated by the chaos effect note,
    // which will be much more informative.
    if (brand != SPMSL_CHAOS)
        take_note(Note(NOTE_MESSAGE, 0, 0, brand_name), true);
#endif
    return brand;
}

bool ranged_attack::blowgun_check(special_missile_type type)
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

    const int enchantment = using_weapon() ? weapon->plus : 0;

    if (attacker->is_monster())
    {
        int chance = 85 - ((defender->get_hit_dice()
                            - attacker->get_hit_dice()) * 5 / 2);
        chance += enchantment * 4;
        chance = min(95, chance);

        if (type == SPMSL_FRENZY)
            chance = chance / 2;
        else if (type == SPMSL_PARALYSIS || type == SPMSL_SLEEP)
            chance = chance * 4 / 5;

        return x_chance_in_y(chance, 100);
    }

    const int skill = you.skill_rdiv(SK_THROWING);

    // You have a really minor chance of hitting with no skills or good
    // enchants.
    if (defender->get_hit_dice() < 15 && random2(100) <= 2)
        return true;

    const int resist_roll = 2 + random2(4 + skill + enchantment);

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

int ranged_attack::blowgun_duration_roll(special_missile_type type)
{
    // Leaving monster poison the same by separating it from player poison
    if (type == SPMSL_POISONED && attacker->is_monster())
        return 6 + random2(8);

    if (type == SPMSL_CURARE)
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
    {
        switch (type)
        {
            case SPMSL_PARALYSIS:
                return 3 + random2(4);
            case SPMSL_SLEEP:
                return 5 + random2(5);
            case SPMSL_CONFUSION:
                return 2 + random2(4);
            default:
                return 5 + random2(5);
        }
    }
    else if (type == SPMSL_POISONED) // Player poison needles
        return random2(3 + base_power * 2 + plus);
    else
        return 5 + random2(base_power + plus);
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

        attacker->god_conduct(DID_FIRE, 1);
        break;
    case SPMSL_FROST:
        calc_elemental_brand_damage(BEAM_COLD, "freeze",
                                    projectile->name(DESC_THE).c_str());
        defender->expose_to_element(BEAM_COLD, 2);
        break;
    case SPMSL_POISONED:
        if (projectile->is_type(OBJ_MISSILES, MI_NEEDLE)
                && using_weapon()
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
                             projectile->is_type(OBJ_MISSILES, MI_NEEDLE)
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
                                      damage_done,
                                      projectile->name(DESC_PLAIN),
                                      atk_name(DESC_PLAIN));
        break;
    case SPMSL_CHAOS:
        chaos_affects_defender();
        break;
    case SPMSL_DISPERSAL:
        if (damage_done > 0)
        {
            if (defender->no_tele(true, true))
            {
                if (defender->is_player())
                    canned_msg(MSG_STRANGE_STASIS);
            }
            else
            {
                coord_def pos, pos2;
                const bool no_sanct = defender->kill_alignment() == KC_OTHER;
                if (random_near_space(defender, defender->pos(), pos, false,
                                      no_sanct)
                    && random_near_space(defender, defender->pos(), pos2, false,
                                         no_sanct))
                {
                    const coord_def from = attacker->pos();
                    if (grid_distance(pos2, from) > grid_distance(pos, from))
                        pos = pos2;

                    if (defender->is_player())
                        defender->blink_to(pos);
                    else
                        defender->as_monster()->blink_to(pos, false, false);
                }
            }
        }
        break;
    case SPMSL_SILVER:
        special_damage = silver_damages_victim(defender, damage_done,
                                               special_damage_message);
        break;
    case SPMSL_PARALYSIS:
        if (!blowgun_check(brand))
            break;
        defender->paralyse(attacker, damage_done);
        break;
    case SPMSL_SLEEP:
        if (!blowgun_check(brand))
            break;
        defender->put_to_sleep(attacker, damage_done);
        should_alert_defender = false;
        break;
    case SPMSL_CONFUSION:
        if (!blowgun_check(brand))
            break;
        defender->confuse(attacker, damage_done);
        break;
    case SPMSL_FRENZY:
        if (!blowgun_check(brand))
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
        // Don't do message-only miscasts along with a special
        // damage message.
        if (miscast_level == 0)
            miscast_level = -1;
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
