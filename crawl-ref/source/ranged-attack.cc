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
#include "localise.h"
#include "message.h"
#include "message-util.h"
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
      should_alert_defender(true), launch_type(launch_retval::BUGGY)
{
    init_attack(SK_THROWING, 0);
    kill_type = KILLED_BY_BEAM;

    string proj_name = projectile->name(DESC_PLAIN);
    // init launch type early, so we can use it later in the constructor
    launch_type = is_launched(attacker, weapon, *projectile);

    // noloc section start
    // (aux_source is used in notes and scorefile, which we leave in English)

    // [dshaligram] When changing bolt names here, you must edit
    // hiscores.cc (scorefile_entry::terse_missile_cause()) to match.
    if (attacker->is_player())
    {
        kill_type = KILLED_BY_SELF_AIMED;
        aux_source = proj_name;
    }
    else if (launch_type == launch_retval::LAUNCHED)
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
    // noloc section end

    needs_message = defender_visible;

    if (!using_weapon())
        wpn_skill = SK_THROWING;
}

int ranged_attack::post_roll_to_hit_modifiers(int mhit, bool random)
{
    int modifiers = attack::post_roll_to_hit_modifiers(mhit, random);

    if (teleport)
    {
        modifiers +=
            (attacker->is_player())
            ? maybe_random_div(you.attribute[ATTR_PORTAL_PROJECTILE],
                               PPROJ_TO_HIT_DIV, random)
            : attacker->as_monster()->get_hit_dice() * 3 / 2;
    }

    // Duplicates describe.cc::_to_hit_pct().
    if (defender && defender->missile_repulsion())
        modifiers -= (mhit + 1) / 2;

    return modifiers;
}

bool ranged_attack::is_penetrating_attack() const
{
    return ::is_penetrating_attack(*attacker, weapon, *projectile);
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

    const int ev = defender->evasion(ev_ignore::none, attacker);
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

    const string defndr = defender_name(false);
    const string proj = projectile->name(DESC_THE);

    const bool reflected_by_shield = defender_shield
                                     && is_shield(*defender_shield)
                                     && shield_reflects(*defender_shield);
    if (reflected_by_shield || defender->reflection())
    {
        reflected = true;
        if (defender->observable())
        {
            if (reflected_by_shield)
            {
                ident_reflector(defender_shield);
                if (needs_message) {
                    if (defender->is_player()) {
                        string shld = defender_shield->name(DESC_YOUR);
                        // locnote: You reflect <projectile> off <your shield>!
                        mprf("You reflect %s off %s!", proj.c_str(), shld.c_str());
                    }
                    else {
                        string shld = defender_shield->name(DESC_PLAIN);
                        mprf("%s reflects off %s %s!", proj.c_str(), 
                             def_name(DESC_ITS).c_str(), shld.c_str());
                    }
                }
            }
            else
            {
                item_def *amulet = defender->slot_item(EQ_AMULET, false);
                if (amulet)
                   ident_reflector(amulet);

                if (needs_message) {
                    if (defender->is_player()) {
                        mprf("%s reflects off an invisible shield around you!",
                             proj.c_str());
                    }
                    else {
                        mprf("%s reflects off an invisible shield around %s!",
                             proj.c_str(), defndr.c_str());
                    }
                }
            }
        }
        else
            mprf("Something reflects %s!", proj.c_str());
    }
    else {
        range_used = BEAM_STOP;
        if (needs_message) {
            if (defender->is_player())
                mprf("You block %s.", proj.c_str());
            else
                mprf("%s blocks %s.", defndr.c_str(), proj.c_str());
        }
    }

    return attack::handle_phase_blocked();
}

bool ranged_attack::handle_phase_dodged()
{
    did_hit = false;

    const int ev = defender->evasion(ev_ignore::none, attacker);

    const int orig_ev_margin =
        test_hit(orig_to_hit, ev, !attacker->is_player());

    if (defender->missile_repulsion() && orig_ev_margin >= 0)
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
        const string proj_name = projectile->name(DESC_THE);
        const string dfndr_name = defender_name(false);

        // just so we don't have to type .c_str() every time...
        const char* proj = proj_name.c_str();
        const char* dfndr = dfndr_name.c_str();

        if (ev_margin <= -20)
        {
            if (defender->is_player())
                mprf("%s completely misses you.", proj);
            else
                mprf("%s completely misses %s.", proj, dfndr);
        }
        else if (ev_margin <= -12)
        {
            if (defender->is_player())
                mprf("%s misses you.", proj);
            else
                mprf("%s misses %s.", proj, dfndr);
        }
        else if (ev_margin <= -6)
        {
            if (defender->is_player())
                mprf("%s closely misses you.", proj);
            else
                mprf("%s closely misses %s.", proj, dfndr);
        }
        else
        {
            if (defender->is_player())
                mprf("%s barely misses you.", proj);
            else
                mprf("%s barely misses %s.", proj, dfndr);
        }
    }

    return true;
}

bool ranged_attack::handle_phase_hit()
{
    // XXX: this kind of hijacks the shield block check
    if (!is_penetrating_attack())
        range_used = BEAM_STOP;

    if (projectile->is_type(OBJ_MISSILES, MI_DART))
    {
        damage_done = dart_duration_roll(get_ammo_brand(*projectile));
        announce_hit();
    }
    else if (projectile->is_type(OBJ_MISSILES, MI_THROWING_NET))
    {
        announce_hit();
        if (defender->is_player())
            player_caught_in_net();
        else
            monster_caught_in_net(defender->as_monster());
    }
    else
    {
        damage_done = calc_damage();
        if (damage_done > 0 || projectile->is_type(OBJ_MISSILES, MI_DART))
        {
            if (!handle_phase_damaged())
                return false;
        }
        else if (needs_message)
        {
            string msg = get_hit_message();
            msg += localise(" but does no damage.");
            mpr_nolocalise(msg);
        }
    }

    if ((using_weapon() || launch_type == launch_retval::THROWN)
        && (!defender->is_player() || !you.pending_revival))
    {
        if (using_weapon()
            && apply_damage_brand(projectile->name(DESC_THE).c_str()))
        {
            return false;
        }
        if ((!defender->is_player() || !you.pending_revival)
            && apply_missile_brand())
        {
            return false;
        }
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

bool ranged_attack::using_weapon() const
{
    return weapon && (launch_type == launch_retval::LAUNCHED
                     || launch_type == launch_retval::BUGGY // not initialized
                         && is_launched(attacker, weapon, *projectile)
                            == launch_retval::LAUNCHED);
}

int ranged_attack::weapon_damage()
{
    if (launch_type == launch_retval::FUMBLED)
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
    if (launch_type == launch_retval::FUMBLED)
        return 0;

    int damage = you.skill_rdiv(wpn_skill);

    // Stones get half bonus; everything else gets full bonus.
    return div_rand_round(damage
                          * min(4, property(*projectile, PWPN_DAMAGE)), 4);
}

int ranged_attack::calc_mon_to_hit_base()
{
    ASSERT(attacker->is_monster());
    return mon_to_hit_base(attacker->get_hit_dice(), attacker->as_monster()->is_archer(), true);
}

int ranged_attack::apply_damage_modifiers(int damage)
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
    if (defender->is_player() && player_omnireflects())
        return false;

    if (is_penetrating_attack())
    {
        if (verbose)
        {
            string proj = projectile->name(DESC_THE);
            string dfndr = apostrophise(defender_name(false));

            if (defender_shield) {
                if (defender->is_player()) {
                    // locnote: <projectile> pierces through <your shield>!
                    mprf("%s pierces through %s!", proj.c_str(),
                        defender_shield->name(DESC_YOUR).c_str());
                }
                else {
                    // locnote: <projectile> pierces through <monster's> <shield>!
                    mprf("%s pierces through %s %s!", proj.c_str(), dfndr.c_str(),
                        defender_shield->name(DESC_PLAIN).c_str());
                }
            }
            else {
                if (defender->is_player())
                    mprf("%s pierces through your shielding!", proj.c_str());
                else {
                    // locnote: <projectile> pierces through <monster's> shielding!
                    mprf("%s pierces through %s shielding!",
                         proj.c_str(), dfndr.c_str());
                }
            }
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
    else if (type == SPMSL_POISONED) // Player poison needles
        return random2(3 + base_power * 2);
    else
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
                                    projectile->name(DESC_THE).c_str());

        defender->expose_to_element(BEAM_FIRE, 2);
        if (defender->is_player())
            maybe_melt_player_enchantments(BEAM_FIRE, special_damage);
        break;
    case SPMSL_FROST:
        calc_elemental_brand_damage(BEAM_COLD,
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
                                      damage_done,
                                      projectile->name(DESC_PLAIN),
                                      atk_name(DESC_A));
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
        special_damage = max(1 + random2(damage_done) / 3,
                             silver_damages_victim(defender, damage_done,
                                                   special_damage_message));
        break;
    case SPMSL_BLINDING:
        if (!dart_check(brand))
            break;
        if (defender->is_monster())
        {
            monster* mon = defender->as_monster();
            if (mons_can_be_blinded(mon->type))
            {
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

string ranged_attack::get_hit_message()
{
    string proj = projectile->name(DESC_THE);
    if (defender->is_player()) {
        if (is_penetrating_attack())
            return localise("%s pierces through you", proj);
        else
            return localise("%s hits you", proj);
    }
    else {
        if (is_penetrating_attack())
            return localise("%s pierces through %s", proj, def_name(DESC_THE));
        else
            return localise("%s hits %s", proj, def_name(DESC_THE));
    }
}

void ranged_attack::announce_hit()
{
    if (!needs_message)
        return;

    string msg = get_hit_message();
    msg += debug_damage_number(); // empty in non-debug build
    attack_strength_message(msg, damage_done, false);
}
