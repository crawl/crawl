/*
 *  File:       fight.cc
 *  Summary:    Functions used during combat.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "fight.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>

#include "externs.h"

#include "cloud.h"
#include "debug.h"
#include "delay.h"
#include "env.h"
#include "invent.h"
#include "itemprop.h"
#include "mon-behv.h"
#include "state.h"
#include "stuff.h"
#include "travel.h"
#include "hints.h"

#ifdef NOTE_DEBUG_CHAOS_BRAND
    #define NOTE_DEBUG_CHAOS_EFFECTS
#endif

#ifdef NOTE_DEBUG_CHAOS_EFFECTS
#include "notes.h"
#endif

// This function returns the "extra" stats the player gets because of
// choice of weapon... it's used only for giving warnings when a player
// wields a less than ideal weapon.
int effective_stat_bonus(int wepType)
{
    int str_weight;
    if (wepType == -1)
        str_weight = player_weapon_str_weight();
    else
        str_weight = weapon_str_weight(OBJ_WEAPONS, wepType);

    return ((you.strength() - you.dex()) * (str_weight - 5) / 10);
}

// Calculates your to-hit roll. If random_factor is true, be stochastic;
// if false, determinstic (e.g. for chardumps).
int calc_your_to_hit(bool random_factor)
{
    melee_attack attk(&you, NULL);
    return attk.calc_to_hit(random_factor);
}

random_var calc_your_attack_delay()
{
    melee_attack attk(&you, NULL);
    return attk.player_calc_attack_delay();
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
        return (unchivalric);

    // Distracted (but not batty); this only applies to players.
    if (attacker && attacker->atype() == ACT_PLAYER
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

    return (unchivalric);
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
    // Drowning damage from water is resistible by being a water thing, or
    // otherwise asphyx resistant.
    case BEAM_WATER:
        return (40);

    // Assume ice storm and throw icicle are mostly solid.
    case BEAM_ICE:
        return (25);

    case BEAM_LAVA:
        return (35);

    case BEAM_POISON_ARROW:
        return (70);

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

    const bool mons = (defender->atype() == ACT_MONSTER);

    const int resistible_fraction = get_resistible_fraction(flavour);

    int resistible = rawdamage * resistible_fraction / 100;
    const int irresistible = rawdamage - resistible;

    if (res > 0)
    {
        if (mons && res >= 3)
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
            else
                resistible /= resist_fraction(res, bonus_res);
        }
    }
    else if (res < 0)
        resistible = resistible * (ranged? 15 : 20) / 10;

    return std::max(resistible + irresistible, 0);
}

///////////////////////////////////////////////////////////////////////////

static bool _is_melee_weapon(const item_def *weapon)
{
    if (weapon->base_type == OBJ_STAVES)
        return (true);

    if (weapon->base_type != OBJ_WEAPONS)
        return (false);

    return (!is_range_weapon(*weapon));
}

bool wielded_weapon_check(item_def *weapon, bool no_message)
{
    bool weapon_warning  = false;
    bool unarmed_warning = false;

    if (weapon)
    {
        if (needs_handle_warning(*weapon, OPER_ATTACK)
            || !_is_melee_weapon(weapon))
        {
            weapon_warning = true;
        }
    }
    else if (you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED]
             && you_tran_can_wear(EQ_WEAPON))
    {
        const int weap = you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] - 1;
        const item_def &wpn = you.inv[weap];
        if (_is_melee_weapon(&wpn))
            unarmed_warning = true;
    }

    if (!you.received_weapon_warning && !you.confused()
        && (weapon_warning || unarmed_warning))
    {
        if (no_message)
            return (false);

        std::string prompt  = "Really attack while ";
        if (unarmed_warning)
            prompt += "being unarmed?";
        else
            prompt += "wielding " + weapon->name(DESC_YOUR) + "? ";

        const bool result = yesno(prompt.c_str(), true, 'n');

        learned_something_new(HINT_WIELD_WEAPON); // for hints mode Rangers

        // Don't warn again if you decide to continue your attack.
        if (result)
            you.received_weapon_warning = true;

        return (result);
    }

    return (true);
}

// Returns true if you hit the monster.
bool you_attack(int monster_attacked, bool unarmed_attacks)
{
    ASSERT(!crawl_state.game_is_arena());

    monster* defender = &menv[monster_attacked];

    // Can't damage orbs or boulders this way.
    if (mons_is_projectile(defender->type) && !you.confused())
    {
        you.turn_is_over = false;
        return (false);
    }

    melee_attack attk(&you, defender, unarmed_attacks);

    // We're trying to hit a monster, break out of travel/explore now.
    if (!travel_kill_monster(defender->type))
        interrupt_activity(AI_HIT_MONSTER, defender);

    // Check if the player is fighting with something unsuitable,
    // or someone unsuitable.
    if (you.can_see(defender)
        && !mons_is_unknown_mimic(defender)
        && !wielded_weapon_check(attk.weapon))
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

bool monster_attack_actor(monster* attacker, actor *defender,
                          bool allow_unarmed)
{
    ASSERT(defender == &you || defender->atype() == ACT_MONSTER);
    return (defender->atype() == ACT_PLAYER ?
              monster_attack(attacker, allow_unarmed)
            : monsters_fight(attacker, defender->as_monster(),
                             allow_unarmed));
}

// A monster attacking the player.
bool monster_attack(monster* attacker, bool allow_unarmed)
{
    ASSERT(!crawl_state.game_is_arena());

    // Friendly and good neutral monsters won't attack unless confused.
    if (attacker->wont_attack() && !mons_is_confused(attacker))
        return (false);

    // It's hard to attack from within a shell.
    if (attacker->withdrawn())
        return (false);

    // In case the monster hasn't noticed you, bumping into it will
    // change that.
    behaviour_event(attacker, ME_ALERT, MHITYOU);
    melee_attack attk(attacker, &you, allow_unarmed);
    attk.attack();

    return (true);
}

// Two monsters fighting each other.
bool monsters_fight(monster* attacker, monster* defender,
                    bool allow_unarmed)
{
    melee_attack attk(attacker, defender, allow_unarmed);
    return (attk.attack());
}

// Returns a value between 0 and 10 representing the weight given to str
int weapon_str_weight(object_class_type wpn_class, int wpn_type)
{
    int  ret;

    const int wpn_skill  = weapon_skill(wpn_class, wpn_type);
    const int hands      = hands_reqd(wpn_class, wpn_type, you.body_size());

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
    if (wpn_type == WPN_WHIP
        || wpn_type == WPN_DEMON_WHIP
        || wpn_type == WPN_SACRED_SCOURGE)
    {
        ret = 2;
    }
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
int player_weapon_str_weight()
{
    const item_def* weapon = you.weapon();

    // Unarmed, weighted slightly towards dex -- would have been more,
    // but then we'd be punishing Trolls and Ghouls who are strong and
    // get special unarmed bonuses.
    if (!weapon)
        return (4);

    int ret = weapon_str_weight(weapon->base_type, weapon->sub_type);

    if (hands_reqd(*weapon, you.body_size()) == HANDS_HALF && !you.shield())
        ret += 1;

    return (ret);
}

// weapon_dex_weight() + weapon_str_weight == 10, so we only need to
// define one of these.
int player_weapon_dex_weight(void)
{
    return (10 - player_weapon_str_weight());
}
