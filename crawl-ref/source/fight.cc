/**
 * @file
 * @brief functions used during combat
 */

#include "AppHdr.h"

#include "fight.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "art-enum.h"
#include "cloud.h"
#include "coordit.h"
#include "delay.h"
#include "env.h"
#include "fineff.h"
#include "fprop.h"
#include "godabil.h"
#include "hints.h"
#include "invent.h"
#include "itemprop.h"
#include "melee_attack.h"
#include "message.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-place.h"
#include "mon-util.h"
#include "ouch.h"
#include "player.h"
#include "prompt.h"
#include "random-var.h"
#include "religion.h"
#include "shopping.h"
#include "spl-miscast.h"
#include "spl-summoning.h"
#include "state.h"
#include "target.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"

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
    const int orig_hp = defender->stat_hp();
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
            return !attk.cancel_attack;
        }

        if (did_hit)
            *did_hit = attk.did_hit;

        // A spectral weapon attacks whenever the player does
        if (!simu && you.props.exists("spectral_weapon"))
            trigger_spectral_weapon(&you, defender);

        if (!simu && you_worship(GOD_DITHMENOS))
            dithmenos_shadow_melee(defender);

        return true;
    }

    // If execution gets here, attacker != Player, so we can safely continue
    // with processing the number of attacks a monster has without worrying
    // about unpredictable or weird results from players.

    // If this is a spectral weapon check if it can attack
    if (attacker->type == MONS_SPECTRAL_WEAPON
        && !confirm_attack_spectral_weapon(attacker->as_monster(), defender))
    {
        // Pretend an attack happened,
        // so the weapon doesn't advance unecessarily.
        return true;
    }
    else if (attacker->type == MONS_GRAND_AVATAR
             && !grand_avatar_check_melee(attacker->as_monster(), defender))
    {
        return true;
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

        if (!simu && attacker->is_monster()
            && mons_attack_spec(attacker->as_monster(), attack_number, true)
                   .flavour == AF_KITE
            && attacker->as_monster()->foe_distance() == 1
            && attacker->reach_range() == REACH_TWO
            && x_chance_in_y(3, 5))
        {
            monster* mons = attacker->as_monster();
            coord_def foepos = mons->get_foe()->pos();
            coord_def hopspot = mons->pos() - (foepos - mons->pos()).sgn();

            bool found = false;
            if (!monster_habitable_grid(mons, grd(hopspot)) ||
                actor_at(hopspot))
            {
                for (adjacent_iterator ai(mons->pos()); ai; ++ai)
                {
                    if (ai->distance_from(foepos) != 2)
                        continue;
                    else
                    {
                        if (monster_habitable_grid(mons, grd(*ai))
                            && !actor_at(*ai))
                        {
                            hopspot = *ai;
                            found = true;
                            break;
                        }
                    }
                }
            }
            else
                found = true;

            if (found)
            {
                const bool could_see = you.can_see(mons);
                if (mons->move_to_pos(hopspot))
                {
                    if (could_see || you.can_see(mons))
                    {
                        mprf("%s hops backward while attacking.",
                             mons->name(DESC_THE, true).c_str());
                    }
                    mons->speed_increment -= 2; // Add a small extra delay
                }
            }
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
    else if (!simu
             && attacker->is_monster()
             && attacker->as_monster()->has_ench(ENCH_GRAND_AVATAR))
    {
        trigger_grand_avatar(attacker->as_monster(), defender, SPELL_NO_SPELL,
                             orig_hp);
    }

    return true;
}

stab_type find_stab_type(const actor *attacker,
                         const actor *defender)
{
    const monster* def = defender->as_monster();
    stab_type unchivalric = STAB_NO_STAB;

    // No stabbing monsters that cannot fight (e.g.  plants) or monsters
    // the attacker can't see (either due to invisibility or being behind
    // opaque clouds).
    if (defender->cannot_fight() || (attacker && !attacker->can_see(defender)))
        return unchivalric;

    // Distracted (but not batty); this only applies to players.
    // Under TSO, monsters are never distracted by your allies.
    if (attacker && attacker->is_player()
        && def && def->foe != MHITYOU && !mons_is_batty(def)
        && (!you_worship(GOD_SHINING_ONE) || def->foe == MHITNOT))
    {
        unchivalric = STAB_DISTRACTED;
    }

    // confused (but not perma-confused)
    if (def && mons_is_confused(def, false))
        unchivalric = STAB_CONFUSED;

    // allies
    if (def && def->friendly())
        unchivalric = STAB_ALLY;

    // fleeing
    if (def && mons_is_fleeing(def))
        unchivalric = STAB_FLEEING;

    // invisible
    if (attacker && !attacker->visible_to(defender))
        unchivalric = STAB_INVISIBLE;

    // held in a net
    if (def && def->caught())
        unchivalric = STAB_HELD_IN_NET;

    // petrifying
    if (def && def->petrifying())
        unchivalric = STAB_PETRIFYING;

    // petrified
    if (defender->petrified())
        unchivalric = STAB_PETRIFIED;

    // paralysed
    if (defender->paralysed())
        unchivalric = STAB_PARALYSED;

    // sleeping
    if (defender->asleep())
        unchivalric = STAB_SLEEPING;

    return unchivalric;
}

static bool is_boolean_resist(beam_type flavour)
{
    switch (flavour)
    {
    case BEAM_ELECTRICITY:
    case BEAM_MIASMA: // rotting
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

static int _beam_to_resist(const actor* defender, beam_type flavour)
{
    switch (flavour)
    {
        case BEAM_FIRE:
        case BEAM_LAVA:
            return defender->res_fire();
        case BEAM_HELLFIRE:
            return defender->res_hellfire();
        case BEAM_STEAM:
            return defender->res_steam();
        case BEAM_COLD:
        case BEAM_ICE:
            return defender->res_cold();
        case BEAM_WATER:
            return defender->res_water_drowning();
        case BEAM_ELECTRICITY:
            return defender->res_elec();
        case BEAM_NEG:
        case BEAM_GHOSTLY_FLAME:
        case BEAM_MALIGN_OFFERING:
            return defender->res_negative_energy();
        case BEAM_ACID:
            return defender->res_acid();
        case BEAM_POISON:
        case BEAM_POISON_ARROW:
            return defender->res_poison();
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
        const bool immune_at_3_res = is_mon || flavour == BEAM_NEG;
        if (immune_at_3_res && res >= 3 || res > 3)
            resistible = 0;
        else
        {
            // Is this a resist that claims to be boolean for damage purposes?
            const int bonus_res = (is_boolean_resist(flavour) ? 1 : 0);

            // Monster resistances are stronger than player versions.
            if (is_mon)
                resistible /= 1 + bonus_res + res * res;
            else if (flavour == BEAM_NEG)
                resistible /= res * 2;
            else
                resistible /= (3 * res + 1) / 2 + bonus_res;
        }
    }
    else if (res < 0)
        resistible = resistible * 15 / 10;

    return max(resistible + irresistible, 0);
}

///////////////////////////////////////////////////////////////////////////

bool wielded_weapon_check(item_def *weapon, bool no_message)
{
    bool penance = false;
    if (!weapon
        || (!needs_handle_warning(*weapon, OPER_ATTACK, penance)
             && is_melee_weapon(*weapon))
        || you.received_weapon_warning)
    {
        return true;
    }

    if (no_message)
        return false;

    string prompt  = "Really attack while wielding " + weapon->name(DESC_YOUR) + "?";
    if (penance)
        prompt += " This could place you under penance!";

    const bool result = yesno(prompt.c_str(), true, 'n');

    learned_something_new(HINT_WIELD_WEAPON); // for hints mode Rangers

    // Don't warn again if you decide to continue your attack.
    if (result)
        you.received_weapon_warning = true;

    return result;
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
static bool _dont_harm(const actor &attacker, const actor &defender)
{
    if (mons_aligned(&attacker, &defender))
        return true;

    if (defender.is_player())
        return attacker.wont_attack();

    if (attacker.is_player())
    {
        return defender.wont_attack()
               || mons_attitude(defender.as_monster()) == ATT_NEUTRAL;
    }

    return false;
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
                        list<actor*> &targets, int which_attack)
{
    // Prevent scanning invalid coordinates if the attacker dies partway through
    // a cleave (due to hitting explosive creatures, or perhaps other things)
    if (!attacker.alive())
        return;

    if (actor_at(def))
        targets.push_back(actor_at(def));

    const item_def* weap = attacker.weapon(which_attack);

    if (!attacker.confused()
        && (weap && item_attack_skill(*weap) == SK_AXES
            || attacker.is_player()
               && (you.form == TRAN_HYDRA && you.heads() > 1
                   || you.duration[DUR_CLEAVE])))
    {
        const coord_def atk = attacker.pos();
        coord_def atk_vector = def - atk;
        const int dir = coinflip() ? -1 : 1;

        for (int i = 0; i < 7; ++i)
        {
            atk_vector = rotate_adjacent(atk_vector, dir);

            actor *target = actor_at(atk + atk_vector);
            if (target && !_dont_harm(attacker, *target))
                targets.push_back(target);
        }
    }

    if (weap && is_unrandom_artefact(*weap, UNRAND_GYRE))
    {
        list<actor*> new_targets;
        for (actor* targ : targets)
        {
            new_targets.push_back(targ);
            new_targets.push_back(targ);
        }
        targets = new_targets;
    }
}

/**
 * Attack a provided list of cleave targets.
 *
 * @param attacker                  The attacking creature.
 * @param targets                   The targets to cleave.
 * @param attack_number             ?
 * @param effective_attack_number   ?
 */
void attack_cleave_targets(actor &attacker, list<actor*> &targets,
                           int attack_number, int effective_attack_number)
{
    while (attacker.alive() && !targets.empty())
    {
        actor* def = targets.front();
        if (def && def->alive() && !_dont_harm(attacker, *def))
        {
            melee_attack attck(&attacker, def, attack_number,
                               ++effective_attack_number, true);
            attck.attack();
        }
        targets.pop_front();
    }
}

int weapon_min_delay(const item_def &weapon)
{
    const int base = property(weapon, PWPN_SPEED);
    int min_delay = base/2;

    // Short blades can get up to at least unarmed speed.
    if (item_attack_skill(weapon) == SK_SHORT_BLADES && min_delay > 5)
        min_delay = 5;

    // All weapons have min delay 7 or better
    if (min_delay > 7)
        min_delay = 7;

    // ...except crossbows...
    if (item_attack_skill(weapon) == SK_CROSSBOWS && min_delay < 10)
        min_delay = 10;

    // ... and unless it would take more than skill 27 to get there.
    // Round up the reduction from skill, so that min delay is rounded down.
    min_delay = max(min_delay, base - (MAX_SKILL_LEVEL + 1)/2);

    // never go faster than speed 3 (ie 3.33 attacks per round)
    if (min_delay < 3)
        min_delay = 3;

    return min_delay;
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

int mons_weapon_damage_rating(const item_def &launcher)
{
    return property(launcher, PWPN_DAMAGE) + launcher.plus;
}

// Returns a rough estimate of damage from firing/throwing missile.
int mons_missile_damage(monster* mons, const item_def *launch,
                        const item_def *missile)
{
    if (!missile || (!launch && !is_throwable(mons, *missile)))
        return 0;

    const int missile_damage = property(*missile, PWPN_DAMAGE) / 2 + 1;
    const int launch_damage  = launch? property(*launch, PWPN_DAMAGE) : 0;
    return max(0, launch_damage + missile_damage);
}

int mons_usable_missile(monster* mons, item_def **launcher)
{
    *launcher = nullptr;
    item_def *launch = nullptr;
    for (int i = MSLOT_WEAPON; i <= MSLOT_ALT_WEAPON; ++i)
    {
        if (item_def *item = mons->mslot_item(static_cast<mon_inv_type>(i)))
        {
            if (is_range_weapon(*item))
                launch = item;
        }
    }

    const item_def *missiles = mons->missiles();
    if (launch && missiles && !missiles->launched_by(*launch))
        launch = nullptr;

    const int fdam = mons_missile_damage(mons, launch, missiles);

    if (!fdam)
        return NON_ITEM;
    else
    {
        *launcher = launch;
        return missiles->index();
    }
}
