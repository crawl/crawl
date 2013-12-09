/**
 * @file
 * @brief functions used during combat
 */

#include "AppHdr.h"

#include "fight.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>

#include "art-enum.h"
#include "cloud.h"
#include "coordit.h"
#include "delay.h"
#include "env.h"
#include "fineff.h"
#include "fprop.h"
#include "hints.h"
#include "invent.h"
#include "itemprop.h"
#include "melee_attack.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-place.h"
#include "mon-util.h"
#include "ouch.h"
#include "player.h"
#include "random-var.h"
#include "shopping.h"
#include "spl-miscast.h"
#include "spl-summoning.h"
#include "state.h"
#include "stuff.h"
#include "target.h"
#include "terrain.h"
#include "travel.h"
#include "traps.h"

/* Handles melee combat between attacker and defender
 *
 * Works using the new fight rewrite. For a monster attacking, this method
 * loops through all their available attacks, instantiating a new melee_attack
 * for each attack. Combat effects should not go here, if at all possible. This
 * is merely a wrapper function which is used to start combat.
 */
bool fight_melee(actor *attacker, actor *defender, bool *did_hit, bool simu)
{
    if (defender->is_player())
    {
        ASSERT(!crawl_state.game_is_arena());
        // Friendly and good neutral monsters won't attack unless confused.
        if (attacker->as_monster()->wont_attack()
            && !mons_is_confused(attacker->as_monster())
            && !attacker->as_monster()->has_ench(ENCH_INSANE))
        {
            return false;
        }

        // It's hard to attack from within a shell.
        if (attacker->as_monster()->withdrawn())
            return false;

        // Boulders can't melee while they're rolling past you
        if (attacker->as_monster()->rolling())
            return false;

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

        melee_attack attk(&you, defender);

        if (simu)
            attk.simu = true;

        // We're trying to hit a monster, break out of travel/explore now.
        if (!travel_kill_monster(defender->type))
            interrupt_activity(AI_HIT_MONSTER, defender->as_monster());

        // Check if the player is fighting with something unsuitable,
        // or someone unsuitable.
        if (you.can_see(defender)
            && !wielded_weapon_check(attk.weapon))
        {
            you.turn_is_over = false;
            return false;
        }

        if (!attk.attack())
        {
            // Attack was cancelled or unsuccessful...
            if (attk.cancel_attack)
                you.turn_is_over = false;
            return false;
        }

        if (did_hit)
            *did_hit = attk.did_hit;

        // A spectral weapon attacks whenever the player does
        if (!simu && you.props.exists("spectral_weapon"))
            trigger_spectral_weapon(&you, defender);

        return true;
    }

    // If execution gets here, attacker != Player, so we can safely continue
    // with processing the number of attacks a monster has without worrying
    // about unpredictable or weird results from players.

    // If this is a spectral weapon check if it can attack
    if (attacker->as_monster()->type == MONS_SPECTRAL_WEAPON
        && !confirm_attack_spectral_weapon(attacker->as_monster(), defender))
    {
        // Pretend an attack happened,
        // so the weapon doesn't advance unecessarily.
        return true;
    }

    const int nrounds = attacker->as_monster()->has_hydra_multi_attack() ?
        attacker->as_monster()->number : 4;
    coord_def pos    = defender->pos();

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

        if (simu)
            melee_attk.simu = true;

        // If the attack fails out, keep effective_attack_number up to
        // date so that we don't cause excess energy loss in monsters
        if (!melee_attk.attack())
            effective_attack_number = melee_attk.effective_attack_number;
        else if (did_hit && !(*did_hit))
            *did_hit = melee_attk.did_hit;

        fire_final_effects();
    }

    // A spectral weapon attacks whenever the player does
    if (!simu && attacker->props.exists("spectral_weapon"))
        trigger_spectral_weapon(attacker, defender);

    return true;
}

// Handles jump attack between attacker and defender.  We need attack_pos since
// defender may not exist.
bool fight_jump(actor *attacker, actor *defender, coord_def attack_pos,
                coord_def landing_pos, set<coord_def> landing_sites,
                bool jump_blocked, bool *did_hit)
{
    set<coord_def>::const_iterator site;

    ASSERT(!crawl_state.game_is_arena());

    melee_attack first_attk(attacker, defender, -1, -1, false, true,
                            jump_blocked, landing_pos);

    // Do player warnings for electrocution and sanctuary based on possible
    // landing sites.
    if (attacker->is_player())
    {
        bool conduct_prompted, zot_trap_prompted, trap_prompted,
            exclusion_prompted, cloud_prompted, terrain_prompted;
        bool defender_vuln = !defender
            || (feat_is_water(grd(defender->pos()))
                && (!you.can_see(defender) || defender->ground_level()));
        bool check_landing_only = false;
        string prompt;
        item_def *weapon = attacker->weapon(-1);

        conduct_prompted = zot_trap_prompted = trap_prompted
            = exclusion_prompted = cloud_prompted = terrain_prompted = false;

        // Can't damage orbs this way.
        if (defender && mons_is_projectile(defender->type) && !you.confused())
        {
            you.turn_is_over = false;
            return false;
        }

        // Check if the player is fighting with something unsuitable,
        // or someone unsuitable.
        if (defender && you.can_see(defender)
            && !wielded_weapon_check(first_attk.weapon))
        {
            you.turn_is_over = false;
            return false;
        }
        else if (!defender || !you.can_see(defender))
        {
            prompt = "Really jump-attack where there is no visible monster?";
            if (!yesno(prompt.c_str(), true, 'n'))
            {
                canned_msg(MSG_OK);
                you.turn_is_over = false;
                return false;
            }
        }

        for (site = landing_sites.begin(); site != landing_sites.end(); site++)
        {
            bool ground_level = !you.airborne() && !you.can_cling_to(*site)
                && you.species != SP_DJINNI;
            if (attacker->damage_brand(-1) == SPWPN_ELECTROCUTION
                && !you.received_weapon_warning
                && (feat_is_water(grd(*site)) && ground_level)
                && !attacker->res_elec()
                && defender_vuln
                && adjacent(*site, defender->pos()))
            {
                prompt = "Really jump-attack with ";
                if (weapon)
                    prompt += weapon->name(DESC_YOUR);
                else
                    prompt += "your electric unarmed attack";
                prompt += " when you might land in water? ";
                if (yesno(prompt.c_str(), true, 'n'))
                    you.received_weapon_warning = true;
                else
                {
                    canned_msg(MSG_OK);
                    you.turn_is_over = false;
                    return false;
                }
            }
            // If we have no defender or have one we can't see and are attacking
            // from within or at a sanctuary position , prompt.
            if (!conduct_prompted && (!defender || !you.can_see(defender)))
            {
                prompt = "";
                if (is_sanctuary(attack_pos))
                    prompt = "Really jump-attack in your sanctuary?";
                else if (is_sanctuary(*site))
                    prompt = "Really jump-attack when you might land in your "
                        "sanctuary?";
                if (prompt != "")
                {
                    if (yesno(prompt.c_str(), true, 'n'))
                        conduct_prompted = true;
                    else
                    {
                        canned_msg(MSG_OK);
                        you.turn_is_over = false;
                        return false;
                    }
                }
            }
            // If we have a defender that we can see, check the attack on the
            // defender in general for conduct and check the landing site for
            // sanctuary; on subsequent sites check only the landing site for a
            // sanctuary if necessary.
            if (defender && !conduct_prompted)
            {
                if (stop_attack_prompt(defender->as_monster(), false, *site,
                                       false, &conduct_prompted, *site,
                                       check_landing_only))
                {
                    you.turn_is_over = false;
                    return false;
                }
            }

            // On the first landing site, check the hit function for elec or
            // devastator for conduct
            if (!check_landing_only && !conduct_prompted
                && (attacker->damage_brand(-1) == SPWPN_ELECTROCUTION
                    || weapon && is_unrandom_artefact(*weapon)
                    && weapon->special == UNRAND_DEVASTATOR))
            {
                const char* verb = "jump-attack";
                string junk1, junk2;
                if (defender)
                {
                    verb = (bad_attack(defender->as_monster(),
                                       junk1, junk2)
                            ? "jump-attack" : "jump-attack near");
                }

                bool (*aff_func)(const coord_def &) = 0;
                if (attacker->damage_brand(-1) == SPWPN_ELECTROCUTION)
                    aff_func = conduction_affected;

                targetter_smite hitfunc(attacker, 1, 1, 1, false, aff_func);
                hitfunc.set_aim(attack_pos);
                hitfunc.origin = *site;

                if (stop_attack_prompt(hitfunc, verb, nullptr,
                                       &conduct_prompted))
                {
                    you.turn_is_over = false;
                    return false;
                }
            }

            // Check landing in dangerous clouds
            if (!cloud_prompted
                && !check_moveto_cloud(*site, "jump-attack", &cloud_prompted))
            {
                you.turn_is_over = false;
                return false;
            }

            //  Check landing on traps, continuing to check for zot traps even
            //  if we've prompted about other kinds of traps.
            if (!zot_trap_prompted)
            {
                trap_def* trap = find_trap(*site);
                if (trap && env.grid(*site) != DNGN_UNDISCOVERED_TRAP
                    && trap->type == TRAP_ZOT)
                {
                    if (!check_moveto_trap(*site, "jump-attack",
                                           &trap_prompted))
                    {
                        you.turn_is_over = false;
                        return false;
                    }
                    zot_trap_prompted = true;
                }
                else if (!trap_prompted
                         && !check_moveto_trap(*site, "jump-attack",
                                               &trap_prompted))
                {
                    you.turn_is_over = false;
                    return false;
                }
            }

            // Check landing in exclusions
            if (!exclusion_prompted
                && !check_moveto_exclusion(*site, "jump-attack",
                                           &exclusion_prompted))
            {
                you.turn_is_over = false;
                return false;
            }

            // Check landing over dangerous terrain while flying or transformed
            // with expiring status.
            if (!terrain_prompted
                && !check_moveto_terrain(*site, "jump-attack", "",
                                         &terrain_prompted))
            {
                you.turn_is_over = false;
                return false;
            }
            check_landing_only = true;
        }
    }
    if (!first_attk.attack() && first_attk.cancel_attack)
    {
        you.turn_is_over = false;
        return false;
    }
    if (did_hit)
        *did_hit = first_attk.did_hit;
    return true;
}

unchivalric_attack_type is_unchivalric_attack(const actor *attacker,
                                              const actor *defender)
{
    const monster* def = defender->as_monster();
    unchivalric_attack_type unchivalric = UCAT_NO_ATTACK;

    // No unchivalric attacks on monsters that cannot fight (e.g.
    // plants) or monsters the attacker can't see (either due to
    // invisibility or being behind opaque clouds).
    if (defender->cannot_fight() || (attacker && !attacker->can_see(defender)))
        return unchivalric;

    // Distracted (but not batty); this only applies to players.
    if (attacker && attacker->is_player()
        && def && def->foe != MHITYOU && !mons_is_batty(def))
    {
        unchivalric = UCAT_DISTRACTED;
    }

    // confused (but not perma-confused)
    if (def && mons_is_confused(def, false))
        unchivalric = UCAT_CONFUSED;

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

    return unchivalric;
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
    // Drowning damage from water is resistible by being a water thing, or
    // otherwise asphyx resistant.
    case BEAM_WATER:
        return 40;

    // Assume ice storm and throw icicle are mostly solid.
    case BEAM_ICE:
        return 40;

    case BEAM_LAVA:
        return 55;

    case BEAM_POISON_ARROW:
    case BEAM_GHOSTLY_FLAME:
        return 70;

    default:
        return 100;
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
        return rawdamage;

    const bool mons = (defender->is_monster());

    const int resistible_fraction = get_resistible_fraction(flavour);

    int resistible = rawdamage * resistible_fraction / 100;
    const int irresistible = rawdamage - resistible;

    if (res > 0)
    {
        if (((mons || flavour == BEAM_NEG) && res >= 3) || res > 3)
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
            if (mons)
                resistible /= 1 + bonus_res + res * res;
            else if (flavour == BEAM_NEG)
                resistible /= res * 2;
            else
                resistible /= resist_fraction(res, bonus_res);
        }
    }
    else if (res < 0)
        resistible = resistible * (ranged? 15 : 20) / 10;

    return max(resistible + irresistible, 0);
}

///////////////////////////////////////////////////////////////////////////

bool wielded_weapon_check(item_def *weapon, bool no_message)
{
    bool weapon_warning  = false;
    bool unarmed_warning = false;

    if (weapon)
    {
        if (needs_handle_warning(*weapon, OPER_ATTACK)
            || !is_melee_weapon(*weapon))
        {
            weapon_warning = true;
        }
    }
    else if (you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED]
             && you_tran_can_wear(EQ_WEAPON))
    {
        const int weap = you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] - 1;
        const item_def &wpn = you.inv[weap];
        if (is_melee_weapon(wpn)
            && you.skill(weapon_skill(wpn)) > you.skill(SK_UNARMED_COMBAT))
        {
            unarmed_warning = true;
        }
    }

    if (!you.received_weapon_warning && !you.confused()
        && (weapon_warning || unarmed_warning))
    {
        if (no_message)
            return false;

        string prompt  = "Really attack while ";
        if (unarmed_warning)
            prompt += "unarmed?";
        else
            prompt += "wielding " + weapon->name(DESC_YOUR) + "? ";

        const bool result = yesno(prompt.c_str(), true, 'n');

        learned_something_new(HINT_WIELD_WEAPON); // for hints mode Rangers

        // Don't warn again if you decide to continue your attack.
        if (result)
            you.received_weapon_warning = true;

        return result;
    }

    return true;
}

// Used by cleave and jump attack to determine if multi-hit targets will be
// attacked.
static bool _dont_harm(const actor* attacker, const actor* defender)
{
    return mons_aligned(attacker, defender)
           || attacker == &you && defender->wont_attack()
           || defender == &you && attacker->wont_attack();
}
// Put the potential cleave targets into a list. Up to 3, taken in order by
// rotating from the def position and stopping at the first solid feature.
void get_cleave_targets(const actor* attacker, const coord_def& def, int dir,
                        list<actor*> &targets)
{
    // Prevent scanning invalid coordinates if the attacker dies partway through
    // a cleave (due to hitting explosive creatures, or perhaps other things)
    if (!attacker->alive())
        return;

    const coord_def atk = attacker->pos();
    coord_def atk_vector = def - atk;

    for (int i = 0; i < 3; ++i)
    {
        atk_vector = rotate_adjacent(atk_vector, dir);
        if (cell_is_solid(atk + atk_vector))
            break;

        actor * target = actor_at(atk + atk_vector);
        if (target && !_dont_harm(attacker, target))
            targets.push_back(target);
    }
}

void get_all_cleave_targets(const actor* attacker, const coord_def& def,
                            list<actor*> &targets)
{
    if (cell_is_solid(def))
        return;

    int dir = coinflip() ? -1 : 1;
    get_cleave_targets(attacker, def, dir, targets);
    targets.reverse();
    if (actor_at(def))
        targets.push_back(actor_at(def));
    get_cleave_targets(attacker, def, -dir, targets);
}

void attack_cleave_targets(actor* attacker, list<actor*> &targets,
                           int attack_number, int effective_attack_number)
{
    while (!targets.empty())
    {
        actor* def = targets.front();
        if (attacker->alive() && def && def->alive()
            && !_dont_harm(attacker, def))
        {
            melee_attack attck(attacker, def, attack_number,
                               ++effective_attack_number, true);
            attck.attack();
        }
        targets.pop_front();
    }
}

int finesse_adjust_delay(int delay)
{
    if (you.duration[DUR_FINESSE])
    {
        ASSERT(!you.duration[DUR_BERSERK]);
        // Need to undo haste by hand.
        if (you.duration[DUR_HASTE])
            delay = haste_mul(delay);
        delay = div_rand_round(delay, 2);
    }
    return delay;
}

bool conduction_affected(const coord_def &pos)
{
    const actor *act = actor_at(pos);

    // Don't check rElec to avoid leaking information about armour etc.
    return feat_is_water(grd(pos)) && act && act->ground_level();
}
