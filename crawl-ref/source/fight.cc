/*
 *  File:       fight.cc
 *  Summary:    Functions used during combat.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *  <11>   07-jul-2000   JDJ    Fixed some of the code in you_attack so it doesn't
 *                              index past the end of arrays for unarmed attacks.
 *  <10>   03-mar-2000   bwr    changes for new spells, no stave magic
 *                              skill practising
 *   <9>   11/23/99      LRH    Now you don't get xp/piety for killing
 *                              monsters who were created friendly
 *   <8>   11/14/99      cdl    evade with random40(ev) vice random2(ev)
 *   <7>   10/ 8/99      BCR    Large races get a smaller
 *                                    penalty for large shields
 *   <6>    9/09/99      BWR    Code for 1-1/2 hand weapons
 *   <5>    8/08/99      BWR    Reduced power of EV/shields
 *   <4>    6/22/99      BWR    Changes to stabbing code, made
 *                              most gods not care about the
 *                              deathes of summoned monsters
 *   <3>    5/21/99      BWR    Upped learning of armour skill
 *                                    in combat slightly.
 *   <2>    5/12/99      BWR    Fixed a bug where burdened
 *                                    barehanded attacks where free
 *   <1>    -/--/--      LRH    Created
 */

#include "AppHdr.h"
#include "fight.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "beam.h"
#include "cloud.h"
#include "debug.h"
#include "delay.h"
#include "effects.h"
#include "food.h"
#include "it_use2.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "macro.h"
#include "misc.h"
#include "monplace.h"
#include "mon-pick.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mstuff2.h"
#include "mutation.h"
#include "ouch.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "skills.h"
#include "spells1.h"
#include "spells3.h"
#include "spells4.h"
#include "spl-cast.h"
#include "stuff.h"
#include "tutorial.h"
#include "view.h"

#define HIT_WEAK 7
#define HIT_MED 18
#define HIT_STRONG 36
// ... was 5, 12, 21
// how these are used will be replaced by a function in a second ... :P {dlb}

static void stab_message(struct monsters *defender, int stab_bonus);

int weapon_str_weight( int wpn_class, int wpn_type );

static inline int player_weapon_str_weight( void );
static inline int player_weapon_dex_weight( void );

static inline int calc_stat_to_hit_base( void );
static inline int calc_stat_to_dam_base( void );

/*
 **************************************************
 *                                                *
 *             BEGIN PUBLIC FUNCTIONS             *
 *                                                *
 **************************************************
*/

#if 0
#define GUARANTEED_HIT_PERCENTAGE       5

bool test_hit( int to_hit, int ev, int bonus )
{
    if (random2(100) < 2 * GUARANTEED_HIT_PERCENTAGE)
        return (coinflip());

    return (random2( to_hit ) + bonus >= ev);
}
#endif

// This function returns the "extra" stats the player gets because of
// choice of weapon... it's used only for giving warnings when a player
// weilds a less than ideal weapon.
int effective_stat_bonus( int wepType )
{
#ifdef USE_NEW_COMBAT_STATS
    int str_weight;
    if (wepType == -1)
        str_weight = player_weapon_str_weight();
    else
        str_weight = weapon_str_weight( OBJ_WEAPONS, wepType );

    return ((you.strength - you.dex) * (str_weight - 5) / 10);
#else
    return (0);
#endif
}

// returns random2(x) is random_factor is true, otherwise
// the mean.
static int maybe_random2( int x, bool random_factor )
{
    if ( random_factor )
	return random2(x);
    else
	return x / 2;
}

// Returns the to-hit for your extra unarmed.attacks.
// DOES NOT do the final roll (i.e., random2(your_to_hit)).
static int calc_your_to_hit_unarmed()
{
    int your_to_hit;

    your_to_hit = 13 + you.dex / 2 + you.skills[SK_UNARMED_COMBAT] / 2
        + you.skills[SK_FIGHTING] / 5;
    
    if (wearing_amulet(AMU_INACCURACY))
	your_to_hit -= 5;
    
    if (you.hunger_state == HS_STARVING)
	your_to_hit -= 3;
    
    your_to_hit += slaying_bonus(PWPN_HIT);

    return your_to_hit;
}

// Calculates your to-hit roll. If random_factor is true, be
// stochastic; if false, determinstic (e.g. for chardumps.)
int calc_your_to_hit( bool random_factor )
{
    melee_attack attk(&you, NULL);
    return attk.calc_to_hit(random_factor);
}

// Calculates your heavy armour penalty. If random_factor is true,
// be stochastic; if false, deterministic (e.g. for chardumps.)
int calc_heavy_armour_penalty( bool random_factor )
{
    const bool ur_armed = (you.equip[EQ_WEAPON] != -1);
    int heavy_armour = 0;

    // heavy armour modifiers for shield borne
    if (you.equip[EQ_SHIELD] != -1)
    {
        switch (you.inv[you.equip[EQ_SHIELD]].sub_type)
        {
        case ARM_SHIELD:
            if (you.skills[SK_SHIELDS] < maybe_random2(7, random_factor))
                heavy_armour++;
            break;
        case ARM_LARGE_SHIELD:
            if ((you.species >= SP_OGRE && you.species <= SP_OGRE_MAGE)
                || player_genus(GENPC_DRACONIAN))
            {
                if (you.skills[SK_SHIELDS] < maybe_random2(13, random_factor))
                    heavy_armour++;     // was potentially "+= 3" {dlb}
            }
            else
            {
                for (int i = 0; i < 3; i++)
                {
                    if (you.skills[SK_SHIELDS] < maybe_random2(13,
							       random_factor))
                        heavy_armour += maybe_random2(3, random_factor);
                }
            }
            break;
        default:
            break;
        }
    }

    // heavy armour modifiers for PARM_EVASION
    if (you.equip[EQ_BODY_ARMOUR] != -1)
    {
        const int ev_pen = property( you.inv[you.equip[EQ_BODY_ARMOUR]],
                                     PARM_EVASION );

        if (ev_pen < 0 &&
	    maybe_random2(you.skills[SK_ARMOUR], random_factor) < abs(ev_pen))
            heavy_armour += maybe_random2( abs(ev_pen), random_factor );
    }

    // ??? what is the reasoning behind this ??? {dlb}
    // My guess is that its supposed to encourage monk-style play -- bwr
    if (!ur_armed)
    {
	if ( random_factor )
        {
	    heavy_armour *= (coinflip() ? 3 : 2);
	}
	// (2+3)/2
	else
        {
	    heavy_armour *= 5;
	    heavy_armour /= 2;
	}
    }
    return heavy_armour;
}

// Returns true if a head got lopped off.
static bool chop_hydra_head( const actor *attacker,
                             actor *def,
                             int damage_done,
                             int dam_type,
                             int wpn_brand )
{
    monsters *defender = dynamic_cast<monsters*>(def);
    
    const bool defender_visible = mons_near(defender);

    // Monster attackers have only a 25% chance of making the
    // chop-check to prevent runaway head inflation.
    if (attacker->atype() == ACT_MONSTER && !one_chance_in(4))
        return (false);
    
    if ((dam_type == DVORP_SLICING || dam_type == DVORP_CHOPPING
         || dam_type == DVORP_CLAWING)
        && damage_done > 0
        && (damage_done >= 4 || wpn_brand == SPWPN_VORPAL || coinflip()))
    {
        defender->number--;

        const char *verb = NULL;

        if (dam_type == DVORP_CLAWING)
        {
            static const char *claw_verbs[] = { "rip", "tear", "claw" };
            verb =
                claw_verbs[
                    random2( sizeof(claw_verbs) / sizeof(*claw_verbs) ) ];
        }
        else
        {
            static const char *slice_verbs[] =
            {
                "slice", "lop", "chop", "hack"
            };
            verb =
                slice_verbs[
                    random2( sizeof(slice_verbs) / sizeof(*slice_verbs) ) ];
        }

        if (defender->number < 1)
        {
            if (defender_visible)
                mprf( "%s %s %s's last head off!",
                      attacker->name(DESC_CAP_THE).c_str(),
                      attacker->conj_verb(verb).c_str(),
                      defender->name(DESC_NOCAP_THE).c_str() );

            defender->hit_points = -1;
        }
        else
        {
            if (defender_visible)
                mprf( "%s %s one of %s's heads off!",
                      attacker->name(DESC_CAP_THE).c_str(),
                      attacker->conj_verb(verb).c_str(),
                      defender->name(DESC_NOCAP_THE).c_str() );

            if (wpn_brand == SPWPN_FLAMING)
            {
                if (defender_visible)
                    mpr( "The flame cauterises the wound!" );
            }
            else if (defender->number < 19)
            {
                simple_monster_message( defender, " grows two more!" );
                defender->number += 2;
                heal_monster( defender, 8 + random2(8), true );
            }
        }

        return (true);
    }

    return (false);
}

static bool actor_decapitates_hydra(actor *attacker, actor *defender,
                                    int damage_done)
{
    if (defender->id() == MONS_HYDRA)
    {
        const int dam_type = attacker->damage_type();
        const int wpn_brand = attacker->damage_brand();

        return chop_hydra_head(attacker, defender, damage_done,
                               dam_type, wpn_brand);
    }
    return (false);
}

static bool player_fights_well_unarmed(int heavy_armour_penalty)
{
    return (you.burden_state == BS_UNENCUMBERED
            && random2(20) < you.skills[SK_UNARMED_COMBAT]
            && random2(1 + heavy_armour_penalty) < 2);
}

//////////////////////////////////////////////////////////////////////////
// Melee attack

melee_attack::melee_attack(actor *attk, actor *defn,
                           bool allow_unarmed, int which_attack)
    : attacker(attk), defender(defn),
      atk(NULL), def(NULL),
      unarmed_ok(allow_unarmed),
      attack_number(which_attack),
      to_hit(0), base_damage(0), potential_damage(0), damage_done(0),
      special_damage(0), aux_damage(0), stab_attempt(false), stab_bonus(0),
      weapon(NULL), damage_brand(SPWPN_NORMAL),
      wpn_skill(SK_UNARMED_COMBAT), hands(HANDS_ONE),
      spwld(SPWLD_NONE), hand_half_bonus(false),
      art_props(0), attack_verb(), verb_degree(),
      no_damage_message(), special_damage_message(), unarmed_attack(),
      heavy_armour_penalty(0), can_do_unarmed(false),
      water_attack(false)
{
    init_attack();
}

void melee_attack::check_hand_half_bonus_eligible()
{
    hand_half_bonus = 
        unarmed_ok
        && !can_do_unarmed
        && !shield
        && weapon
        && !item_cursed( *weapon )
        && hands == HANDS_HALF;
}

void melee_attack::init_attack()
{
    if (attacker->atype() == ACT_MONSTER)
        atk = dynamic_cast<monsters*>(attacker);

    if (defender && defender->atype() == ACT_MONSTER)
        def = dynamic_cast<monsters*>(defender);
    
    weapon       = attacker->weapon(attack_number);
    damage_brand = attacker->damage_brand(attack_number);

    if (weapon && weapon->base_type == OBJ_WEAPONS
        && is_random_artefact( *weapon ))
    {
        randart_wpn_properties( *weapon, art_props );    
    }

    wpn_skill = weapon? weapon_skill( *weapon ) : SK_UNARMED_COMBAT;
    if (weapon)
    {
        hands = hands_reqd( *weapon, attacker->body_size() );
        spwld = item_special_wield_effect( *weapon );
    }

    shield = attacker->shield();

    water_attack = is_water_attack(attacker, defender);
}

bool melee_attack::is_water_attack(const actor *attk,
                                   const actor *defn) const
{
    return (defn && attk->swimming() && defn->floundering());
}

void melee_attack::check_autoberserk()
{
    if (weapon
        && art_props[RAP_ANGRY] >= 1
        && !one_chance_in(1 + art_props[RAP_ANGRY]))
    {
        attacker->go_berserk(false);
    }
}

void melee_attack::check_special_wield_effects()
{
    switch (spwld)
    {
    case SPWLD_TROG:
        if (coinflip())
            attacker->go_berserk(false);
        break;

    case SPWLD_WUCAD_MU:
        // XXX At some distant point in the future, allow
        // miscast_effect to operate on any actor.
        if (one_chance_in(9) && attacker->atype() == ACT_PLAYER)
        {
            miscast_effect( SPTYP_DIVINATION, random2(9), random2(70), 100, 
                            "the Staff of Wucad Mu" );
        }
        break;
    default:
        break;
    }
}

bool melee_attack::attack()
{
    if (attacker->fumbles_attack())
        return (false);

    // Allow god to get offended, etc.
    attacker->attacking(defender);

    // A lot of attack parameters get set in here. 'Ware.
    to_hit = calc_to_hit();

    // The attacker loses nutrition.
    attacker->make_hungry(3, true);

    check_autoberserk();
    check_special_wield_effects();

    // Trying to stay general beyond this point is a recipe for insanity.
    // Maybe when Stone Soup hits 1.0... :-)
    return (attacker->atype() == ACT_PLAYER? player_attack() :
            defender->atype() == ACT_PLAYER? mons_attack_you() :
                                             mons_attack_mons() );
}

bool melee_attack::player_attack()
{
    potential_damage =
        !weapon? player_calc_base_unarmed_damage()
               : player_calc_base_weapon_damage();

    player_apply_attack_delay();
    player_stab_check();

    if (player_hits_monster())
    {
        did_hit = true;

        // This actually does more than calculate damage - it also sets up
        // messages, etc.
        player_calc_hit_damage();

        // always upset monster regardless of damage
        behaviour_event(def, ME_WHACK, MHITYOU);
        
        if (player_hurt_monster())
            player_exercise_combat_skills();
        
        if (player_check_monster_died())
            return (true);

        if (damage_done < 1)
            no_damage_message =
                make_stringf("You %s %s.",
                             attack_verb.c_str(),
                             defender->name(DESC_NOCAP_THE).c_str());
    }
    else
        player_warn_miss();

    if (did_hit && (damage_done > 0 || !player_monster_visible(def)))
        player_announce_hit();

    if (did_hit && player_monattk_hit_effects(false))
        return (true);

    const bool did_primary_hit = did_hit;
    
    if (unarmed_ok && player_aux_unarmed())
        return (true);

    if ((did_primary_hit || did_hit) && def->type != -1)
        print_wounds(def);

    return (did_primary_hit || did_hit);
}

// Returns true to end the attack round.
bool melee_attack::player_aux_unarmed()
{
    damage_brand  = SPWPN_NORMAL;
    int uattack = UNAT_NO_ATTACK;

    if (can_do_unarmed)
    {
        if (you.species == SP_NAGA) 
            uattack = UNAT_HEADBUTT;
        else
            uattack = (coinflip() ? UNAT_HEADBUTT : UNAT_KICK);

        if ((you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON
             || player_genus(GENPC_DRACONIAN)
             || (you.species == SP_MERFOLK && player_is_swimming())
             || you.mutation[ MUT_STINGER ])
            && one_chance_in(3))
        {
            uattack = UNAT_TAILSLAP;
        }

        if (coinflip())
            uattack = UNAT_PUNCH;
    }

    for (int scount = 0; scount < 4; scount++)
    {
        unarmed_attack.clear();
        damage_brand = SPWPN_NORMAL;
        aux_damage = 0;

        switch (scount)
        {
        case 0:
            if (uattack != UNAT_KICK)        //jmf: hooves mutation
            {
                if ((you.species != SP_CENTAUR && !you.mutation[MUT_HOOVES])
                    || coinflip())
                {
                    continue;
                }
            }

            if (you.attribute[ATTR_TRANSFORMATION] == TRAN_SERPENT_OF_HELL
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_ICE_BEAST
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_SPIDER)
            {
                continue;
            }

            unarmed_attack = "kick";
            aux_damage = ((you.mutation[MUT_HOOVES]
                           || you.species == SP_CENTAUR) ? 10 : 5);
            break;

        case 1:
            if (uattack != UNAT_HEADBUTT)
            {
                if ((!you.mutation[MUT_HORNS] && you.species != SP_KENKU)
                    || !one_chance_in(3))
                {
                    continue;
                }
            }

            if (you.attribute[ATTR_TRANSFORMATION] == TRAN_SERPENT_OF_HELL
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_SPIDER
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_ICE_BEAST
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON)
            {
                continue;
            }

            unarmed_attack =
                (you.species == SP_KENKU) ? "peck" : "headbutt";

            aux_damage = 5 + you.mutation[MUT_HORNS] * 3;
            
            // minotaurs used to get +5 damage here, now they get
            // +6 because of the horns.

            if (you.equip[EQ_HELMET] != -1
                && (get_helmet_type(you.inv[you.equip[EQ_HELMET]]) == THELM_HELMET
                    || get_helmet_type(you.inv[you.equip[EQ_HELMET]]) == THELM_HELM))
            {
                aux_damage += 2;

                if (get_helmet_desc(you.inv[you.equip[EQ_HELMET]]) == THELM_DESC_SPIKED
                    || get_helmet_desc(you.inv[you.equip[EQ_HELMET]]) == THELM_DESC_HORNED)
                {
                    aux_damage += 3;
                }
            }
            break;

        case 2:             /* draconians */
            if (uattack != UNAT_TAILSLAP)
            {
                // not draconian, and not wet merfolk
                if ((!player_genus(GENPC_DRACONIAN)
                     && (!(you.species == SP_MERFOLK && player_is_swimming()))
                     && !you.mutation[ MUT_STINGER ])
                    || (!one_chance_in(4)))

                {
                    continue;
                }
            }

            if (you.attribute[ATTR_TRANSFORMATION] == TRAN_SPIDER
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_ICE_BEAST)
            {
                continue;
            }

            unarmed_attack = "tail-slap";
            aux_damage = 6;

            if (you.mutation[ MUT_STINGER ] > 0)
            {
                aux_damage += (you.mutation[ MUT_STINGER ] * 2 - 1);
                damage_brand  = SPWPN_VENOM;
            }

            /* grey dracs have spiny tails, or something */
            // maybe add this to player messaging {dlb}
            //
            // STINGER mutation doesn't give extra damage here... that
            // would probably be a bit much, we'll still get the 
            // poison bonus so it's still somewhat good.
            if (you.species == SP_GREY_DRACONIAN && you.experience_level >= 7)
                aux_damage = 12;
            break;

        case 3:
            if (uattack != UNAT_PUNCH)
                continue;

            if (you.attribute[ATTR_TRANSFORMATION] == TRAN_SERPENT_OF_HELL
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_SPIDER
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_ICE_BEAST
                || you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON)
            {
                continue;
            }

            /* no punching with a shield or 2-handed wpn, except staves */
            if (shield || coinflip()
                || (weapon
                    && hands == HANDS_TWO
                    && weapon->base_type != OBJ_STAVES 
                    && weapon->sub_type  != WPN_QUARTERSTAFF) )
            {
                continue;
            }

            unarmed_attack = "punch";

            /* applied twice */
            aux_damage = 5 + you.skills[SK_UNARMED_COMBAT] / 3;

            if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
            {
                unarmed_attack = "slash";
                aux_damage += 6;
            }
            break;

            /* To add more, add to while part of loop below as well */
        default:
            continue;

        }

        // unified to-hit calculation
        to_hit = random2( calc_your_to_hit_unarmed() );
            
        make_hungry(2, true);

        alert_nearby_monsters();

        // XXX We're clobbering did_hit
        did_hit = false;
        if (to_hit >= def->evasion || one_chance_in(30))
        {
            if (player_apply_aux_unarmed())
                return (true);
        }
        else
        {
            mprf("Your %s misses %s.",
                 unarmed_attack.c_str(),
                 defender->name(DESC_NOCAP_THE).c_str());
        }
    }

    return (false);
}

bool melee_attack::player_apply_aux_unarmed()
{
    did_hit = true;

    aux_damage = player_aux_stat_modify_damage(aux_damage);
    aux_damage += slaying_bonus(PWPN_DAMAGE);

    aux_damage = random2(aux_damage);

    // Clobber wpn_skill
    wpn_skill  = SK_UNARMED_COMBAT;
    aux_damage = player_apply_weapon_skill(aux_damage);
    aux_damage = player_apply_fighting_skill(aux_damage, true);
    aux_damage = player_apply_misc_modifiers(aux_damage);

    // Clear stab bonus which will be set for the primary weapon attack.
    stab_bonus = 0;
    aux_damage = player_apply_monster_ac(aux_damage);
                
    if (aux_damage < 1)
        aux_damage = 0;
    else
        hurt_monster(def, damage_done);

    if (damage_done > 0)
    {
        player_exercise_combat_skills();

        mprf("You %s %s%s%s",
             unarmed_attack.c_str(),
             defender->name(DESC_NOCAP_THE).c_str(),
             debug_damage_number().c_str(),
             attack_strength_punctuation().c_str());
                     
        if (damage_brand == SPWPN_VENOM && coinflip())
            poison_monster( def, true );
                    
        if (mons_holiness(def) == MH_HOLY)
            did_god_conduct(DID_KILL_ANGEL, 1);
    }
    else // no damage was done
    {
        mprf("You %s %s%s.",
             unarmed_attack.c_str(),
             defender->name(DESC_NOCAP_THE).c_str(),
             player_monster_visible(def)?
             ", but do no damage" : "");
    }
                
    if (def->hit_points < 1)
    {
        monster_die(def, KILL_YOU, 0);
        return (true);
    }

    return (false);
}

std::string melee_attack::debug_damage_number()
{
#ifdef DEBUG_DIAGNOSTICS
    return make_stringf(" for %d", damage_done);
#else
    return ("");
#endif
}

std::string melee_attack::special_attack_punctuation()
{
    if (special_damage < 3)
        return ".";
    else if (special_damage < 7)
        return "!";
    else
        return "!!";
}

std::string melee_attack::attack_strength_punctuation()
{
    if (damage_done < HIT_WEAK)
        return ".";
    else if (damage_done < HIT_MED)
        return "!";
    else if (damage_done < HIT_STRONG)
        return "!!";
    else
        return "!!!";
}

void melee_attack::player_announce_hit()
{
    if (!verb_degree.empty() && verb_degree[0] != ' ')
        verb_degree = " " + verb_degree;
    
    mprf("You %s %s%s%s%s",
         attack_verb.c_str(),
         ptr_monam(def, DESC_NOCAP_THE),
         verb_degree.c_str(),
         debug_damage_number().c_str(),
         attack_strength_punctuation().c_str());
}

std::string melee_attack::player_why_missed()
{
    if ((to_hit + heavy_armour_penalty / 2) >= def->evasion)
        return "Your armour prevents you from hitting ";
    else
        return "You miss ";
}

void melee_attack::player_warn_miss()
{
    did_hit = false;

    // upset only non-sleeping monsters if we missed
    if (def->behaviour != BEH_SLEEP)
        behaviour_event( def, ME_WHACK, MHITYOU );

    mprf("%s%s.",
         player_why_missed().c_str(),
         ptr_monam(def, DESC_NOCAP_THE));
}

bool melee_attack::player_hits_monster()
{
#if DEBUG_DIAGNOSTICS
    mprf( MSGCH_DIAGNOSTICS, "your to-hit: %d; defender EV: %d", 
          to_hit, def->evasion );
#endif

    return (to_hit >= def->evasion
            || one_chance_in(3)
            || ((mons_is_paralysed(def) || def->behaviour == BEH_SLEEP)
                && !one_chance_in(10 + you.skills[SK_STABBING])));
}

int melee_attack::player_stat_modify_damage(int damage)
{
    int dammod = 78;
    const int dam_stat_val = calc_stat_to_dam_base();
    
    if (dam_stat_val > 11)
        dammod += (random2(dam_stat_val - 11) * 2);
    else if (dam_stat_val < 9)
        dammod -= (random2(9 - dam_stat_val) * 3);
    
    damage *= dammod;
    damage /= 78;

    return (damage);
}

int melee_attack::player_aux_stat_modify_damage(int damage)
{
    int dammod = 10;
    const int dam_stat_val = calc_stat_to_dam_base();
                
    if (dam_stat_val > 11)
        dammod += random2(dam_stat_val - 11) / 3;
    if (dam_stat_val < 9)
        dammod -= random2(9 - dam_stat_val) / 2;
                
    damage *= dammod;
    damage /= 10;

    return (damage);
}

int melee_attack::player_apply_water_attack_bonus(int damage)
{
#if 0
    // Yes, this isn't the *2 damage that monsters get, but since
    // monster hps and player hps are different (as are the sizes
    // of the attacks) it just wouldn't be fair to give merfolk
    // that gross a potential... still they do get something for
    // making use of the water.  -- bwr
    // return (damage + random2avg(10,2));
#endif

    // [dshaligram] Experimenting with the full double damage bonus since
    // it takes real effort to set up a water attack and it's very situational.
    return (damage * 2);
}

int melee_attack::player_apply_weapon_skill(int damage)
{
    if (weapon && (weapon->base_type == OBJ_WEAPONS
                   || item_is_staff( *weapon )))
    {
        damage *= 25 + (random2( you.skills[ wpn_skill ] + 1 ));
        damage /= 25;
    }

    return (damage);
}

int melee_attack::player_apply_fighting_skill(int damage, bool aux)
{
    const int base = aux? 40 : 30;
    
    damage *= base + (random2(you.skills[SK_FIGHTING] + 1));
    damage /= base;

    return (damage);
}

int melee_attack::player_apply_misc_modifiers(int damage)
{
    if (you.might > 1)
        damage += 1 + random2(10);
    
    if (you.hunger_state == HS_STARVING)
        damage -= random2(5);

    return (damage);
}

int melee_attack::player_apply_weapon_bonuses(int damage)
{
    if (weapon && weapon->base_type == OBJ_WEAPONS)
    {
        int wpn_damage_plus = weapon->plus2;
        
        damage += (wpn_damage_plus > -1) ? (random2(1 + wpn_damage_plus))
                                         : -(1 + random2(-wpn_damage_plus));

        // removed 2-handed weapons from here... their "bonus" is
        // already included in the damage stat for the weapon -- bwr
        if (hand_half_bonus)
            damage += random2(3);
        
        if (get_equip_race(*weapon) == ISFLAG_DWARVEN
            && player_genus(GENPC_DWARVEN))
        {
            damage += random2(3);
        }
        
        if (get_equip_race(*weapon) == ISFLAG_ORCISH
            && you.species == SP_HILL_ORC && coinflip())
        {
            damage++;
        }
    }

    return (damage);
}

void melee_attack::player_weapon_auto_id()
{
    if (weapon
        && !is_range_weapon( *weapon )
        && !item_ident( *weapon, ISFLAG_KNOW_PLUSES )
        && random2(100) < you.skills[ wpn_skill ])
    {
        set_ident_flags( *weapon, ISFLAG_KNOW_PLUSES );
        mprf("You are wielding %s.",
             item_name(*weapon, DESC_NOCAP_A));
        more();
        you.wield_change = true;
    }
}

int melee_attack::player_stab_weapon_bonus(int damage)
{
    switch (wpn_skill)
    {
    case SK_SHORT_BLADES:
    {
        int bonus = (you.dex * (you.skills[SK_STABBING] + 1)) / 5;
            
        if (weapon->sub_type != WPN_DAGGER)
            bonus /= 2;
            
        bonus = stepdown_value( bonus, 10, 10, 30, 30 );
            
        damage += bonus;
    }
    // fall through
    case SK_LONG_SWORDS:
        damage *= 10 + you.skills[SK_STABBING] /
            (stab_bonus + (wpn_skill == SK_SHORT_BLADES ? 0 : 1));
        damage /= 10;
        // fall through
    default:
        damage *= 12 + you.skills[SK_STABBING] / stab_bonus;
        damage /= 12;
    }

    return (damage);
}

int melee_attack::player_stab(int damage)
{
    // The stabbing message looks better here:
    if (stab_attempt)
    {
        // construct reasonable message
        stab_message( def, stab_bonus );

        exercise(SK_STABBING, 1 + random2avg(5, 4));
        
        if (mons_holiness(def) == MH_NATURAL
            || mons_holiness(def) == MH_HOLY)
        {
            did_god_conduct(DID_STABBING, 4);
        }
    }
    else
    {
        stab_bonus = 0;
        // ok.. if you didn't backstab, you wake up the neighborhood.
        // I can live with that.
        alert_nearby_monsters();
    }

    if (stab_bonus)
    {
        // lets make sure we have some damage to work with...
        if (damage < 1)
            damage = 1;
        
        if (def->behaviour == BEH_SLEEP)
        {
            // Sleeping moster wakes up when stabbed but may be groggy
            if (random2(200) <= you.skills[SK_STABBING] + you.dex)
            {
                unsigned int stun = random2( you.dex + 1 );
                
                if (def->speed_increment > stun)
                    def->speed_increment -= stun;
                else
                    def->speed_increment = 0;
            }
        }

        damage = player_stab_weapon_bonus(damage);
    }

    return (damage);
}

int melee_attack::player_apply_monster_ac(int damage)
{
    if (stab_bonus)
    {
        // when stabbing we can get by some of the armour
        if (def->armour_class > 0)
        {
            int ac = def->armour_class
                - random2( you.skills[SK_STABBING] / stab_bonus );
            
            if (ac > 0)
                damage -= random2(1 + ac);
        }
    }
    else
    {
        // apply AC normally
        if (def->armour_class > 0)
            damage -= random2(1 + def->armour_class);
    }

    return (damage);
}

int melee_attack::player_weapon_type_modify(int damage)
{
    int weap_type = WPN_UNKNOWN;  

    if (!weapon)
        weap_type = WPN_UNARMED;
    else if (item_is_staff( *weapon ))
        weap_type = WPN_QUARTERSTAFF;
    else if (weapon->base_type == OBJ_WEAPONS)
        weap_type = weapon->sub_type;

    // All weak hits look the same, except for when the player 
    // has a non-weapon in hand.  -- bwr
    if (damage < HIT_WEAK)
    {
        if (weap_type != WPN_UNKNOWN)
            attack_verb = "hit";
        else
            attack_verb = "clumsily bash";

        return (damage);
    }

    // take transformations into account, if no weapon is wielded
    if (weap_type == WPN_UNARMED 
        && you.attribute[ATTR_TRANSFORMATION] != TRAN_NONE)
    {
        switch (you.attribute[ATTR_TRANSFORMATION])
        {
        case TRAN_SPIDER:
            if (damage < HIT_STRONG)
                attack_verb = "bite";
            else
                attack_verb = "maul";
            break;
        case TRAN_BLADE_HANDS:
            if (damage < HIT_MED)
                attack_verb = "slash";
            else if (damage < HIT_STRONG)
                attack_verb = "slice";
            else
                attack_verb = "shred";
            break;
        case TRAN_ICE_BEAST:
        case TRAN_STATUE:
        case TRAN_LICH:
            if (damage < HIT_MED)
                attack_verb = "punch";
            else
                attack_verb = "pummel";
            break;
        case TRAN_DRAGON:
        case TRAN_SERPENT_OF_HELL:
            if (damage < HIT_MED)
                attack_verb = "claw";
            else if (damage < HIT_STRONG)
                attack_verb = "bite";
            else
            {
                attack_verb = "maul";
                if (defender->body_size() <= SIZE_MEDIUM && coinflip())
                    attack_verb = "trample on";
            }
            break;
        case TRAN_AIR:
            attack_verb = "buffet";
            break;
        } // transformations

        return (damage);
    }

    switch (weapon? get_damage_type(*weapon) : -1)
    {
    case DAM_PIERCE:
        if (damage < HIT_MED)
            attack_verb = "puncture";
        else if (damage < HIT_STRONG)
            attack_verb = "impale";
        else
        {
            attack_verb = "spit";
            verb_degree = " like a pig";
        }
        break;

    case DAM_SLICE:
        if (damage < HIT_MED)
            attack_verb = "slash";
        else if (damage < HIT_STRONG)
            attack_verb = "slice";
        else
        {
            attack_verb = "open";
            verb_degree = " like a pillowcase";
        }
        break;

    case DAM_BLUDGEON:
        if (damage < HIT_MED)
            attack_verb = one_chance_in(4)? "thump" : "sock";
        else if (damage < HIT_STRONG)
            attack_verb = "bludgeon";
        else
        {
            attack_verb = "crush";
            verb_degree = " like a grape";
        }
        break;

    case DAM_WHIP:
        if (damage < HIT_MED)
            attack_verb = "whack";
        else
            attack_verb = "thrash";
        break;

    case -1: // unarmed
        if (you.species == SP_TROLL || you.mutation[MUT_CLAWS])
        {
            if (damage < HIT_MED)
                attack_verb = "claw";
            else if (damage < HIT_STRONG)
                attack_verb = "mangle";
            else
                attack_verb = "eviscerate";
        }
        else
        {
            if (damage < HIT_MED)
                attack_verb = "punch";
            else
                attack_verb = "pummel";
        }
        break;

    case WPN_UNKNOWN:
    default:
        attack_verb = "hit";
        break;
    }

    return (damage);
}

bool melee_attack::player_hurt_monster()
{
    return damage_done && hurt_monster(def, damage_done);
}

void melee_attack::player_exercise_combat_skills()
{
    const bool helpless = defender->cannot_fight();
        
    if (!helpless || you.skills[ wpn_skill ] < 2)
        exercise( wpn_skill, 1 );
        
    if ((!helpless || you.skills[SK_FIGHTING] < 2)
        && one_chance_in(3))
    {
        exercise(SK_FIGHTING, 1);
    }
}

// Returns true if the combat round should end here.
bool melee_attack::player_monattk_hit_effects(bool mondied)
{
    if (mons_holiness(def) == MH_HOLY)
        did_god_conduct(mondied? DID_KILL_ANGEL : DID_ATTACK_HOLY, 1);

    if (spwld == SPWLD_TORMENT && coinflip())
    {
        torment(TORMENT_SPWLD, you.x_pos, you.y_pos);
        did_god_conduct(DID_UNHOLY, 5);
    }

    if (spwld == SPWLD_ZONGULDROK || spwld == SPWLD_CURSE)
        did_god_conduct(DID_NECROMANCY, 3);

    if (weapon
        && weapon->base_type == OBJ_WEAPONS
        && is_demonic( *weapon ))
    {
        did_god_conduct(DID_UNHOLY, 1);
    }
    
    if (mondied && damage_brand == SPWPN_VAMPIRICISM)
    {
        if (mons_holiness(def) == MH_NATURAL
            && damage_done > 0 && you.hp < you.hp_max
            && !one_chance_in(5))
        {
            mpr("You feel better.");
            
            // more than if not killed
            inc_hp(1 + random2(damage_done), false);
            
            if (you.hunger_state != HS_ENGORGED)
                lessen_hunger(30 + random2avg(59, 2), true);
            
            did_god_conduct(DID_NECROMANCY, 2);
        }
    }

    if (mondied)
        return (true);

    // These effects apply only to monsters that are still alive:
    
    if (actor_decapitates_hydra(attacker, defender, damage_done))
        return (true);

    // These two are mutually exclusive!
    player_apply_staff_damage();

    // Returns true if the monster croaked.
    if (player_apply_damage_brand())
        return (true);

    if (!no_damage_message.empty())
    {
        if (special_damage > 0)
            emit_nodmg_hit_message();
        else
            mprf("You %s %s, but do no damage.",
                 attack_verb.c_str(),
                 defender->name(DESC_NOCAP_THE).c_str());
    }
    
    if (!special_damage_message.empty())
        mprf("%s", special_damage_message.c_str());

    player_sustain_passive_damage();
    hurt_monster(def, special_damage);

    if (def->hit_points < 1)
    {
        monster_die(def, KILL_YOU, 0);
        return (true);
    }

    return (false);
}

void melee_attack::player_calc_brand_damage(
    int res,
    const char *message_format)
{
    if (res == 0)
        special_damage = random2(damage_done) / 2 + 1;
    else if (res < 0)
        special_damage = random2(damage_done) + 1;

    if (special_damage)
    {
        special_damage_message = make_stringf(
            message_format,
            defender->name(DESC_NOCAP_THE).c_str(),
            special_attack_punctuation().c_str());
    }
}

bool melee_attack::player_apply_damage_brand()
{
    // Monster resistance to the brand.
    int res = 0;
    
    special_damage = 0;
    switch (damage_brand)
    {
    case SPWPN_FLAMING:
        res = mons_res_fire(def);
        if (weapon && weapon->special == SPWPN_SWORD_OF_CEREBOV && res >= 0)
            res = (res > 0) - 1;

        player_calc_brand_damage(res, "You burn %s%s");
        break;

    case SPWPN_FREEZING:
        player_calc_brand_damage(mons_res_cold(def), "You freeze %s%s");
        break;

    case SPWPN_HOLY_WRATH:
        switch (mons_holiness(def))
        {
        case MH_UNDEAD:
            special_damage = 1 + random2(damage_done);
            break;
            
        case MH_DEMONIC:
            special_damage = 1 + (random2(damage_done * 15) / 10);
            break;

        default:
            break;
        }
        if (special_damage)
            special_damage_message =
                make_stringf(
                    "%s convulses%s",
                    defender->name(DESC_CAP_THE).c_str(),
                    special_attack_punctuation().c_str());
        break;

    case SPWPN_ELECTROCUTION:
        if (mons_flies(def))
            break;
        else if (mons_res_elec(def) > 0)
            break;
        else if (one_chance_in(3))
        {
            special_damage_message = 
                   "There is a sudden explosion of sparks!";
            special_damage = random2avg(28, 3);
        }
        break;

    case SPWPN_ORC_SLAYING:
        if (mons_species(def->type) == MONS_ORC)
        {
            special_damage = 1 + random2(damage_done);
            special_damage_message =
                make_stringf(
                    "%s convulses%s",
                    defender->name(DESC_CAP_THE).c_str(),
                    special_attack_punctuation().c_str());
        }
        break;

    case SPWPN_VENOM:
        if (!one_chance_in(4))
        {
            // Poison monster message needs to arrive after hit message.
            emit_nodmg_hit_message();
            poison_monster(def, true);
        }
        break;

    case SPWPN_DRAINING:
        if (mons_res_negative_energy(def) > 0 || one_chance_in(3))
            break;

        special_damage_message =
            make_stringf(
                "You drain %s!",
                defender->name(DESC_NOCAP_THE).c_str());
        
        if (one_chance_in(5))
            def->hit_dice--;
        
        def->max_hit_points -= 2 + random2(3);
        def->hit_points -= 2 + random2(3);
        
        if (def->hit_points >= def->max_hit_points)
            def->hit_points = def->max_hit_points;
        
        if (def->hit_dice < 1)
            def->hit_points = 0;
        
        special_damage = 1 + (random2(damage_done) / 2);
        did_god_conduct( DID_NECROMANCY, 2 );
        break;

        /* 9 = speed - done before */
    case SPWPN_VORPAL:
        special_damage = 1 + random2(damage_done) / 2;
        // note: leaving special_damage_message empty because there
        // isn't one.
        break;

    case SPWPN_VAMPIRICISM:
        if (mons_holiness(def) != MH_NATURAL ||
            mons_res_negative_energy(def) > 0 ||
            damage_done < 1 || you.hp == you.hp_max ||
            one_chance_in(5))
        {
            break;
        }
        
        // We only get here if we've done base damage, so no
        // worries on that score.
        mpr("You feel better.");

        // thus is probably more valuable on larger weapons?
        if (weapon
            && is_fixed_artefact( *weapon ) 
            && weapon->special == SPWPN_VAMPIRES_TOOTH)
        {
            inc_hp(damage_done, false);
        }
        else
        {
            inc_hp(1 + random2(damage_done), false);
        }
        
        if (you.hunger_state != HS_ENGORGED)
            lessen_hunger(random2avg(59, 2), true);
        
        did_god_conduct( DID_NECROMANCY, 2 );
        break;

    case SPWPN_DISRUPTION:
        if (mons_holiness(def) == MH_UNDEAD && !one_chance_in(3))
        {
            special_damage_message =
                str_simple_monster_message(def, " shudders.");
            special_damage += random2avg((1 + (damage_done * 3)), 3);
        }
        break;
        
    case SPWPN_PAIN:
        if (mons_res_negative_energy(def) <= 0
            && random2(8) <= you.skills[SK_NECROMANCY])
        {
            special_damage_message =
                str_simple_monster_message(def, " convulses in agony.");
            special_damage += random2( 1 + you.skills[SK_NECROMANCY] );
        }
        did_god_conduct(DID_NECROMANCY, 4);
        break;

    case SPWPN_DISTORTION:
        //jmf: blink frogs *like* distortion
        // I think could be amended to let blink frogs "grow" like
        // jellies do {dlb}
        if (defender->id() == MONS_BLINK_FROG)
        {
            if (one_chance_in(5))
            {
                emit_nodmg_hit_message();
                simple_monster_message(
                    def, 
                    " basks in the translocular energy." );
                heal_monster(def, 1 + random2avg(7, 2), true); // heh heh
            }
            break;
        }
        
        if (one_chance_in(3))
        {
            special_damage_message =
                make_stringf(
                     "Space bends around %s.",
                     defender->name(DESC_NOCAP_THE).c_str());
            special_damage += 1 + random2avg(7, 2);
            break;
        }

        if (one_chance_in(3))
        {
            special_damage_message =
                make_stringf(
                    "Space warps horribly around %s!",
                    ptr_monam(def, DESC_NOCAP_THE));
            
            special_damage += 3 + random2avg(24, 2);
            break;
        }

        if (one_chance_in(3))
        {
            emit_nodmg_hit_message();
            monster_blink(def);
            break;
        }

        if (coinflip())
        {
            emit_nodmg_hit_message();
            monster_teleport(def, coinflip());
            break;
        }

        if (coinflip())
        {
            emit_nodmg_hit_message();
            monster_die(def, KILL_RESET, 0);
            return (true);
        }
        break;

    case SPWPN_CONFUSE:
    {
        emit_nodmg_hit_message();
        
        // declaring these just to pass to the enchant function
        bolt beam_temp;
        beam_temp.flavour = BEAM_CONFUSION;
        
        mons_ench_f2( def, beam_temp );
        
        you.confusing_touch -= random2(20);
        
        if (you.confusing_touch < 1)
            you.confusing_touch = 1;
        break;
    }
    }

    return (false);
}

void melee_attack::player_sustain_passive_damage()
{
    if (mons_class_flag(defender->id(), M_ACID_SPLASH))
        weapon_acid(5);
}

int melee_attack::player_staff_damage(int skill)
{
    return
        roll_dice(3,
                  1 + (you.skills[skill] + you.skills[SK_EVOCATIONS]) / 2);
}

void melee_attack::emit_nodmg_hit_message()
{
    if (!no_damage_message.empty())
    {
        mprf("%s", no_damage_message.c_str());
        no_damage_message.clear();
    }
}

void melee_attack::player_apply_staff_damage()
{
    special_damage = 0;
    
    if (!weapon || !item_is_staff(*weapon))
        return;

    const int staff_cost = 2;
    
    if (you.magic_points < staff_cost
        || random2(15) > you.skills[SK_EVOCATIONS])
    {
        return;
    }
    
    switch (weapon->sub_type)
    {
    case STAFF_AIR:
        if (damage_done + you.skills[SK_AIR_MAGIC] <= random2(20))
            break;

        if (mons_res_elec(def))
            break;

        special_damage = player_staff_damage(SK_AIR_MAGIC);
                
        if (special_damage)
            special_damage_message =
                make_stringf("%s is jolted!",
                             defender->name(DESC_CAP_THE).c_str());
        
        break;

    case STAFF_COLD:
        if (mons_res_cold(def) > 0)
            break;

        special_damage = player_staff_damage(SK_ICE_MAGIC);

        if (mons_res_cold(def) < 0)
            special_damage += player_staff_damage(SK_ICE_MAGIC);

        if (special_damage)
        {
            special_damage_message =
                make_stringf(
                    "You freeze %s!",
                    defender->name(DESC_NOCAP_THE).c_str());
        }
        break;

    case STAFF_EARTH:
        special_damage = player_staff_damage(SK_EARTH_MAGIC);

        if (special_damage) 
        {
            special_damage_message =
                make_stringf(
                    "You crush %s!",
                    defender->name(DESC_NOCAP_THE).c_str());
        }
        break;

    case STAFF_FIRE:
        if (mons_res_fire(def) > 0)
            break;

        special_damage = player_staff_damage(SK_FIRE_MAGIC);

        if (mons_res_fire(def) < 0)
            special_damage += player_staff_damage(SK_FIRE_MAGIC);

        if (special_damage)
        {
            special_damage_message =
                make_stringf(
                    "You burn %s!",
                    defender->name(DESC_NOCAP_THE).c_str());
        }
        break;

    case STAFF_POISON:
        // cap chance at 30% -- let staff of Olgreb shine
        int temp_rand = damage_done + you.skills[SK_POISON_MAGIC];

        if (temp_rand > 30)
            temp_rand = 30;

        if (random2(100) < temp_rand)
        {
            // Poison monster message needs to arrive after hit message.
            emit_nodmg_hit_message();
            poison_monster(def, true);
        }
        break;

    case STAFF_DEATH:
        if (mons_res_negative_energy(def) > 0)
            break;

        if (random2(8) <= you.skills[SK_NECROMANCY])
        {
            special_damage = player_staff_damage(SK_NECROMANCY);

            if (special_damage)
            {
                special_damage_message =
                    make_stringf(
                        "%s convulses in agony!",
                        defender->name(DESC_CAP_THE).c_str());

                did_god_conduct(DID_NECROMANCY, 4);
            }
        }
        break;

    case STAFF_POWER:
    case STAFF_SUMMONING:
    case STAFF_CHANNELING:
    case STAFF_CONJURATION:
    case STAFF_ENCHANTMENT:
    case STAFF_ENERGY:
    case STAFF_WIZARDRY:
        break;

    default:
        mpr("You're wielding some staff I've never heard of! (fight.cc)");
        break;
    } // end switch

    if (special_damage > 0)
    {
        dec_mp(staff_cost);

        if (!item_type_known(*weapon))
        {
            set_ident_flags( *weapon, ISFLAG_KNOW_TYPE );

            mprf("You are wielding %s.",
                 item_name(*weapon, DESC_NOCAP_A));
            more();
            you.wield_change = true;
        }
    }
}

bool melee_attack::player_check_monster_died()
{
    if (def->hit_points < 1)
    {
#if DEBUG_DIAGNOSTICS
        /* note: doesn't take account of special weapons etc */
        mprf( MSGCH_DIAGNOSTICS, "Hit for %d.", damage_done );
#endif

        player_monattk_hit_effects(true);

        if (def->type == MONS_GIANT_SPORE || def->type == MONS_BALL_LIGHTNING)
            mprf("You %s %s.", attack_verb.c_str(),
                 ptr_monam(def, DESC_NOCAP_THE));

        monster_die(def, KILL_YOU, 0);

        return (true);
    }

    return (false);
}

void melee_attack::player_calc_hit_damage()
{
    potential_damage = player_stat_modify_damage(potential_damage);

    if (water_attack)
        potential_damage = player_apply_water_attack_bonus(potential_damage);
    
    //  apply damage bonus from ring of slaying
    // (before randomization -- some of these rings
    //  are stupidly powerful) -- GDL
    potential_damage += slaying_bonus(PWPN_DAMAGE);
    damage_done = potential_damage > 0? 1 + random2(potential_damage) : 0;
    damage_done = player_apply_weapon_skill(damage_done);
    damage_done = player_apply_fighting_skill(damage_done, false);
    damage_done = player_apply_misc_modifiers(damage_done);
    damage_done = player_apply_weapon_bonuses(damage_done);

    player_weapon_auto_id();

    damage_done = player_stab(damage_done);
    damage_done = player_apply_monster_ac(damage_done);

    // This doesn't actually modify damage -- bwr
    damage_done = player_weapon_type_modify( damage_done );

    if (damage_done < 0)
        damage_done = 0;
}

int melee_attack::calc_to_hit(bool random)
{
    return (attacker->atype() == ACT_PLAYER? player_to_hit(random)
                                           : mons_to_hit());
}

int melee_attack::player_to_hit(bool random_factor)
{
    heavy_armour_penalty = calc_heavy_armour_penalty(random_factor);
    can_do_unarmed = player_fights_well_unarmed(heavy_armour_penalty);

    hand_half_bonus =
        unarmed_ok
        && !can_do_unarmed
        && !shield
        && weapon
        && !item_cursed( *weapon )
        && hands == HANDS_HALF;

    int your_to_hit = 15 + (calc_stat_to_hit_base() / 2);

    if (water_attack)
        your_to_hit += 5;

    if (wearing_amulet(AMU_INACCURACY))
        your_to_hit -= 5;

    // if you can't see yourself, you're a little less acurate.
    if (you.invis && !player_see_invis())
        your_to_hit -= 5;

    // fighting contribution
    your_to_hit += maybe_random2(1 + you.skills[SK_FIGHTING], random_factor);

    // weapon skill contribution
    if (weapon)
    {
        if (wpn_skill != SK_FIGHTING)
        {
            your_to_hit += maybe_random2(you.skills[wpn_skill] + 1,
                                         random_factor);
        }
    }
    else
    {                       // ...you must be unarmed
        your_to_hit +=
            (you.species == SP_TROLL || you.species == SP_GHOUL) ? 4 : 2;

        your_to_hit += maybe_random2(1 + you.skills[SK_UNARMED_COMBAT],
                                     random_factor);
    }

    // weapon bonus contribution
    if (weapon)
    {
        if (weapon->base_type == OBJ_WEAPONS)
        {
            your_to_hit += weapon->plus;
            your_to_hit += property( *weapon, PWPN_HIT );

            if (get_equip_race(*weapon) == ISFLAG_ELVEN
                && player_genus(GENPC_ELVEN))
            {
                your_to_hit += (random_factor && coinflip() ? 2 : 1);
            }
        }
        else if (item_is_staff( *weapon ))
        {
            // magical staff
            your_to_hit += property( *weapon, PWPN_HIT );
        }
    }

    // slaying bonus
    your_to_hit += slaying_bonus(PWPN_HIT);

    // hunger penalty
    if (you.hunger_state == HS_STARVING)
        your_to_hit -= 3;

    // armour penalty
    your_to_hit -= heavy_armour_penalty;

#if DEBUG_DIAGNOSTICS
    int roll_hit = your_to_hit;
#endif

    // hit roll
    your_to_hit = maybe_random2(your_to_hit, random_factor);

#if DEBUG_DIAGNOSTICS
    mprf( MSGCH_DIAGNOSTICS, "to hit die: %d; rolled value: %d",
          roll_hit, your_to_hit );
#endif

    if (hand_half_bonus)
        your_to_hit += maybe_random2(3, random_factor);

    if (weapon && wpn_skill == SK_SHORT_BLADES && you.sure_blade)
        your_to_hit += 5 +
            (random_factor ? random2limit( you.sure_blade, 10 ) :
             you.sure_blade / 2);

    // other stuff
    if (!weapon)
    {
        if ( you.confusing_touch )
            // just trying to touch is easier that trying to damage
            your_to_hit += maybe_random2(you.dex, random_factor);

        switch ( you.attribute[ATTR_TRANSFORMATION] )
        {
        case TRAN_SPIDER:
            your_to_hit += maybe_random2(10, random_factor);
            break;
        case TRAN_ICE_BEAST:
            your_to_hit += maybe_random2(10, random_factor);
            break;
        case TRAN_BLADE_HANDS:
            your_to_hit += maybe_random2(12, random_factor);
            break;
        case TRAN_STATUE:
            your_to_hit += maybe_random2(9, random_factor);
            break;
        case TRAN_SERPENT_OF_HELL:
        case TRAN_DRAGON:
            your_to_hit += maybe_random2(10, random_factor);
            break;
        case TRAN_LICH:
            your_to_hit += maybe_random2(10, random_factor);
            break;
        case TRAN_AIR:
            your_to_hit = 0;
            break;
        case TRAN_NONE:
        default:
            break;
        }
    }

    // Check for backlight (Corona).
    if (defender
        && defender->atype() == ACT_MONSTER
        && mons_has_ench(def, ENCH_BACKLIGHT_I, ENCH_BACKLIGHT_IV))
    {
        your_to_hit += 2 + random2(8);
    }

    return (your_to_hit);
}

void melee_attack::player_stab_check()
{
    bool roll_needed = true;
    // This ordering is important!

    // not paying attention (but not batty)
    if (def->foe != MHITYOU && !testbits(def->flags, MF_BATTY))
    {
        stab_attempt = true;
        stab_bonus = 3;
    }

    // confused (but not perma-confused)
    if (mons_has_ench(def, ENCH_CONFUSION) 
        && !mons_class_flag(def->type, M_CONFUSED))
    {
        stab_attempt = true;
        stab_bonus = 2;
    }

    // fleeing
    if (def->behaviour == BEH_FLEE)
    {
        stab_attempt = true;
        stab_bonus = 2;
    }

    // sleeping
    if (def->behaviour == BEH_SLEEP)
    {
        stab_attempt = true;
        roll_needed = false;
        stab_bonus = 1;
    }

    // helpless (plants, etc)
    if (defender->cannot_fight())
        stab_attempt = false;

    // see if we need to roll against dexterity / stabbing
    if (stab_attempt && roll_needed)
        stab_attempt = (random2(200) <= you.skills[SK_STABBING] + you.dex);

    // check for invisibility - no stabs on invisible monsters.
    if (!player_monster_visible( def ))
    {
        stab_attempt = false;
        stab_bonus = 0;
    }
}

void melee_attack::player_apply_attack_delay()
{
    int attack_delay = weapon? player_weapon_speed() : player_unarmed_speed();
    attack_delay = player_apply_shield_delay(attack_delay);

    if (attack_delay < 3)
        attack_delay = 3;

    final_attack_delay = attack_delay;

    you.time_taken = (you.time_taken * final_attack_delay) / 10;
    if (you.time_taken < 1)
        you.time_taken = 1;

#if DEBUG_DIAGNOSTICS
    mprf( MSGCH_DIAGNOSTICS,
          "Weapon speed: %d; min: %d; attack time: %d", 
          final_attack_delay, min_delay, you.time_taken );
#endif
}

int melee_attack::player_weapon_speed()
{
    int attack_delay = 0;
    
    if (weapon && (weapon->base_type == OBJ_WEAPONS
                   || item_is_staff( *weapon )))
    {
        attack_delay = property( *weapon, PWPN_SPEED );
        attack_delay -= you.skills[ wpn_skill ] / 2;
        
        min_delay = property( *weapon, PWPN_SPEED ) / 2;

        // Short blades can get up to at least unarmed speed.
        if (wpn_skill == SK_SHORT_BLADES && min_delay > 5)
            min_delay = 5;
        
        // Using both hands can get a weapon up to speed 7
        if ((hands == HANDS_TWO || hand_half_bonus)
            && min_delay > 7)
        {
            min_delay = 7;
        }
        
        // never go faster than speed 3 (ie 3 attacks per round)
        if (min_delay < 3)
            min_delay = 3;
        
        // Hand and a half bonus only helps speed up to a point, any more
        // than speed 10 must come from skill and the weapon
        if (hand_half_bonus && attack_delay > 10)
            attack_delay--;
        
        // apply minimum to weapon skill modification
        if (attack_delay < min_delay)
            attack_delay = min_delay;
        
        if (weapon->base_type == OBJ_WEAPONS
            && damage_brand == SPWPN_SPEED)
        {
            attack_delay = (attack_delay + 1) / 2;
        }
    }
    
    return (attack_delay);
}

int melee_attack::player_unarmed_speed()
{
    int unarmed_delay = 10;

    min_delay = 5;
    
    // Unarmed speed
    if (you.burden_state == BS_UNENCUMBERED
        && one_chance_in(heavy_armour_penalty + 1))
    {
        unarmed_delay = 10 - you.skills[SK_UNARMED_COMBAT] / 5;
	    
	    /* this shouldn't happen anyway...sanity */
        if (unarmed_delay < min_delay)
            unarmed_delay = min_delay;
    }

    return (unarmed_delay);
}

int melee_attack::player_apply_shield_delay(int attack_delay)
{
    if (shield)
    {
        switch (shield->sub_type)
        {
        case ARM_LARGE_SHIELD:
            if (you.skills[SK_SHIELDS] <= 10 + random2(17))
                attack_delay++;
            // [dshaligram] Fall-through
            
        case ARM_SHIELD:
            if (you.skills[SK_SHIELDS] <= 3 + random2(17))
                attack_delay++;
            break;
        }
    }

    return (attack_delay);
}

int melee_attack::player_calc_base_unarmed_damage()
{
    int damage = 3;

    if (you.confusing_touch)
    {
        // no base hand damage while using this spell
        damage = 0;
    }

    if (you.attribute[ATTR_TRANSFORMATION] != TRAN_NONE)
    {
        switch (you.attribute[ATTR_TRANSFORMATION])
        {
        case TRAN_SPIDER:
            damage = 5;
            break;
        case TRAN_ICE_BEAST:
            damage = 12;
            break;
        case TRAN_BLADE_HANDS:
            damage = 12 + (you.strength / 4) + (you.dex / 4);
            break;
        case TRAN_STATUE:
            damage = 12 + you.strength;
            break;
        case TRAN_SERPENT_OF_HELL:
        case TRAN_DRAGON:
            damage = 20 + you.strength;
            break;
        case TRAN_LICH:
            damage = 5;
            break;
        case TRAN_AIR:
            damage = 0;
            break;
        }
    }
    else if (you.equip[ EQ_GLOVES ] == -1)
    {
        // claw damage only applies for bare hands
        if (you.species == SP_TROLL)
            damage += 5;
        else if (you.species == SP_GHOUL)
            damage += 2;

        damage += (you.mutation[ MUT_CLAWS ] * 2);
    }

    damage += you.skills[SK_UNARMED_COMBAT];

    return (damage);
}

int melee_attack::player_calc_base_weapon_damage()
{
    int damage = 0;
    
    if (weapon->base_type == OBJ_WEAPONS
        || item_is_staff( *weapon ))
    {
        damage = property( *weapon, PWPN_DAMAGE );
    }

    return (damage);
}

///////////////////////////////////////////////////////////////////////////

bool melee_attack::mons_attack_mons()
{
    return (false);
}

bool melee_attack::mons_attack_you()
{
    return (false);
}

int melee_attack::mons_to_hit()
{
    // TODO
    return 0;
}

///////////////////////////////////////////////////////////////////////////

static void tutorial_weapon_check(const item_def *weapon)
{
    if (weapon &&
        (weapon->base_type != OBJ_WEAPONS
         || is_range_weapon(*weapon)))
    {
    	learned_something_new(TUT_WIELD_WEAPON);
    }
}

// Returns true if you hit the monster.
bool you_attack(int monster_attacked, bool unarmed_attacks)
{
    monsters *defender = &menv[monster_attacked];
    melee_attack attk(&you, defender, unarmed_attacks);

    // We're trying to hit a monster, break out of travel/explore now.
    interrupt_activity(AI_HIT_MONSTER, defender);

    // For tutorials, check if the player is fighting with something unsuitable
    tutorial_weapon_check(attk.weapon);

    return attk.attack();
}

static void mons_lose_attack_energy(monsters *attacker, int wpn_speed,
                                    int which_attack)
{
    // speed adjustment for weapon using monsters
    if (wpn_speed > 0)
    {
        // only get one third penalty/bonus for second weapons.
        if (which_attack > 0)
            wpn_speed = (20 + wpn_speed) / 3;

        attacker->speed_increment -= (wpn_speed - 10) / 2;
    }
}

void monster_attack(int monster_attacking)
{
    struct monsters *attacker = &menv[monster_attacking];

    int damage_taken = 0;
    bool hit = false;
    bool blocked = false;

    // being attacked by a water creature while standing in water?
    bool water_attack = false;

    bool bearing_shield = (you.equip[EQ_SHIELD] != -1);

    int mmov_x = 0;
    int specdam = 0;
    char heads = 0;             // for hydras {dlb}
    int hand_used = 0;
    int extraDamage = 0;            // from special mon. attacks (burn, freeze, etc)
    int resistValue = 0;           // player resist value (varies)
    int wpn_speed;
    char str_pass[ ITEMNAME_SIZE ];

#if DEBUG_DIAGNOSTICS
    char st_prn[ 20 ];
#endif

    if (attacker->type == MONS_HYDRA)
        heads = attacker->number;

    if (mons_friendly(attacker))
        return;

    if (attacker->type == MONS_GIANT_SPORE
        || attacker->type == MONS_BALL_LIGHTNING)
    {
        attacker->hit_points = -1;
        return;
    }

    // if a friend wants to help, they can attack <monster_attacking>
    if (you.pet_target == MHITNOT)
        you.pet_target = monster_attacking;

    if (mons_has_ench( attacker, ENCH_SUBMERGED ))
        return;

    // If a mimic is attacking the player, it is thereafter known.
    if (mons_is_mimic(attacker->type))
        attacker->flags |= MF_KNOWN_MIMIC;

    if (you.duration[DUR_REPEL_UNDEAD] 
        && mons_holiness( attacker ) == MH_UNDEAD
        && !check_mons_resist_magic( attacker, you.piety ))
    {
        if (simple_monster_message(
                attacker,
                " tries to attack you, but is repelled by your holy aura."))
            interrupt_activity( AI_MONSTER_ATTACKS, attacker );
        return;
    }

    if (wearing_amulet(AMU_WARDING)
        || (you.religion == GOD_VEHUMET && you.duration[DUR_PRAYER]
            && (!player_under_penance() && you.piety >= piety_breakpoint(2))))
    {
        if (mons_has_ench(attacker, ENCH_ABJ_I, ENCH_ABJ_VI))
        {
            // should be scaled {dlb}
            if (coinflip())
            {
                if (simple_monster_message(
                        attacker,
                        " tries to attack you, but flinches away."))
                    interrupt_activity( AI_MONSTER_ATTACKS, attacker );
                return;
            }
        }
    }

    if (grd[attacker->x][attacker->y] == DNGN_SHALLOW_WATER
        && !mons_flies( attacker )
        && !mons_class_flag( attacker->type, M_AMPHIBIOUS )
        && monster_habitat( attacker->type ) == DNGN_FLOOR 
        && one_chance_in(4))
    {
        if (simple_monster_message(attacker, " splashes around in the water."))
            interrupt_activity( AI_MONSTER_ATTACKS, attacker );
        return;
    }

    if (player_in_water() 
        && !player_is_swimming()
        && monster_habitat( attacker->type ) == DNGN_DEEP_WATER)
    {
        water_attack = true;
        simple_monster_message(attacker,
                               " uses the watery terrain to its advantage.");
    }

    char runthru;
    bool player_perceives_attack = false;

    for (runthru = 0; runthru < 4; runthru++)
    {
        blocked = false;
        wpn_speed = 0;             // 0 = didn't attack w/ weapon

        if (attacker->type == MONS_HYDRA)
        {
            if (heads < 1)
                break;
            runthru = 0;
            heads--;
        }

        char mdam = mons_damage( attacker->type, runthru );

        if (attacker->type == MONS_ZOMBIE_SMALL
            || attacker->type == MONS_ZOMBIE_LARGE
            || attacker->type == MONS_SKELETON_SMALL
            || attacker->type == MONS_SKELETON_LARGE
            || attacker->type == MONS_SIMULACRUM_SMALL
            || attacker->type == MONS_SIMULACRUM_LARGE
            || attacker->type == MONS_SPECTRAL_THING)
        {
            mdam = mons_damage(attacker->number, runthru);

            // these are cumulative, of course: {dlb}
            if (mdam > 1)
                mdam--;
            if (mdam > 4)
                mdam--;
            if (mdam > 11)
                mdam--;
            if (mdam > 14)
                mdam--;
        }

        if (mdam == 0)
            break;

        if ((attacker->type == MONS_TWO_HEADED_OGRE 
                || attacker->type == MONS_ETTIN)
            && runthru == 1)
        {
            hand_used = 1;
        }

        damage_taken = 0;

        int mons_to_hit = 16 + attacker->hit_dice; //* attacker->hit_dice;//* 3

        if (water_attack)
            mons_to_hit += 5;

        if (attacker->inv[hand_used] != NON_ITEM
            && mitm[attacker->inv[hand_used]].base_type == OBJ_WEAPONS)
        {
            mons_to_hit += mitm[attacker->inv[hand_used]].plus;

            mons_to_hit += property( mitm[attacker->inv[hand_used]], PWPN_HIT );

            wpn_speed = property( mitm[attacker->inv[hand_used]], PWPN_SPEED );
        }

        // Factors against blocking
        // [dshaligram] Scaled back HD effect to 50% of previous, reduced
        // shield_blocks multiplier to 5.
        const int con_block = 
                random2(15 + attacker->hit_dice / 2
                               + (5 * you.shield_blocks * you.shield_blocks));

        // Factors for blocking:
        // [dshaligram] Added weighting for shields skill, increased dex bonus
        // from .2 to .25
        const int pro_block =
                random2(player_shield_class()) 
                                        + (random2(you.dex) / 4) 
                                        + (random2(skill_bump(SK_SHIELDS)) / 4)
                                        - 1;

        if (!you.paralysis && !you_are_delayed() && !you.conf 
            && player_monster_visible( attacker )
            && player_shield_class() > 0
            && con_block <= pro_block)
        {
            you.shield_blocks++;

            snprintf( info, INFO_SIZE, "You block %s's attack.", 
                      ptr_monam( attacker, DESC_NOCAP_THE ) );

            mpr(info);

            blocked = true;
            hit = false;
            player_perceives_attack = true;

            if (bearing_shield && one_chance_in(4))
                exercise(SK_SHIELDS, 1);
        }
        else if (player_light_armour(true) && one_chance_in(3))
        {
            exercise(SK_DODGING, 1);
        }

        const int player_dodge = random2limit(player_evasion(), 40)
                                + random2(you.dex) / 3
                                - (player_monster_visible(attacker) ? 2 : 14)
                                - (you_are_delayed() ? 5 : 0);

        if (!blocked
            && (random2(mons_to_hit) >= player_dodge || one_chance_in(30)))
        {
            hit = true;

            int damage_size = 0;

            if (attacker->inv[hand_used] != NON_ITEM
                && mitm[attacker->inv[hand_used]].base_type == OBJ_WEAPONS
                && (mitm[attacker->inv[hand_used]].sub_type < WPN_SLING
                    || mitm[attacker->inv[hand_used]].sub_type > WPN_CROSSBOW))
            {
                damage_size = property( mitm[attacker->inv[hand_used]],
                                        PWPN_DAMAGE );

                damage_taken = random2(damage_size);

                if (get_equip_race(mitm[attacker->inv[hand_used]]) == ISFLAG_ORCISH
                    && mons_species(attacker->type) == MONS_ORC
                    && coinflip())
                {
                    damage_taken++;
                }

                if (mitm[attacker->inv[hand_used]].plus2 >= 0)
                {
                    /* + or 0 to-dam */
                    damage_taken += random2(mitm[attacker->inv[hand_used]].plus2 + 1);
                }
                else
                {
                    /* - to-dam */
                    damage_taken -= (random2(1 + abs(mitm[attacker->inv[hand_used]].plus2)));
                }

                damage_taken -= 1 + random2(3); //1;
            }

            damage_size += mdam;
            damage_taken += 1 + random2(mdam);

            if (water_attack)
                damage_taken *= 2;

            int ac = player_AC();

            if (ac > 0)
            {
                int damage_reduction = random2(ac + 1);

                if (!player_light_armour())
                {
                    const int body_arm_ac = property( you.inv[you.equip[EQ_BODY_ARMOUR]], 
                                                      PARM_AC );

                    int percent = 2 * (you.skills[SK_ARMOUR] + body_arm_ac);

                    if (percent > 50)
                        percent = 50;

                    int min = 1 + (damage_size * percent) / 100;

                    if (min > ac / 2)
                        min = ac / 2;

                    if (damage_reduction < min)
                        damage_reduction = min;
                }

                damage_taken -= damage_reduction;
            }

            if (damage_taken < 1)
                damage_taken = 0;

        }
        else if (!blocked)
        {
            hit = false;
            if (simple_monster_message(attacker, " misses you."))
                player_perceives_attack = true;
        }

        if ((hit && !blocked) || damage_taken > 0)
            player_perceives_attack = true;
        
        if (damage_taken < 1 && hit && !blocked)
        {
            mprf("%s hits you but doesn't do any damage.",
                 ptr_monam(attacker, DESC_CAP_THE));
        }

        if (damage_taken > 0)
        {
            hit = true;

            mmov_x = attacker->inv[hand_used];

            strcpy(info, ptr_monam(attacker, DESC_CAP_THE));
            strcat(info, " hits you");

#if DEBUG_DIAGNOSTICS
            strcat(info, " for ");
            // note: doesn't take account of special weapons etc
            itoa( damage_taken, st_prn, 10 );
            strcat( info, st_prn );
#endif

            if (attacker->type != MONS_DANCING_WEAPON && mmov_x != NON_ITEM
                && mitm[mmov_x].base_type == OBJ_WEAPONS
                && !is_range_weapon( mitm[mmov_x] ))
            {
                strcat(info, " with ");
                it_name(mmov_x, DESC_NOCAP_A, str_pass);   // was 7
                strcat(info, str_pass);
            }

            strcat(info, "!");

            mpr( info, MSGCH_PLAIN );

            if (hit)
            {
                if (you.equip[EQ_BODY_ARMOUR] != -1)
                {
                    const int body_arm_mass = item_mass( you.inv[you.equip[EQ_BODY_ARMOUR]] );

                    // Being generous here...this can train both Armour
                    // and Dodging.
                    if (!player_light_armour(false) && coinflip()
                        && random2(1000) <= body_arm_mass)
                    {
                        // raised from 1 {bwross}
                        exercise(SK_ARMOUR, (coinflip() ? 2 : 1));
                    }
                }
            }

            /* special attacks: */
            int mclas = attacker->type;

            if (mclas == MONS_KILLER_KLOWN)
            {
                switch (random2(6))
                {
                case 0:
                    // comment and enum do not match {dlb}
                    mclas = MONS_SNAKE; // scorp
                    break;
                case 1:
                    mclas = MONS_NECROPHAGE;
                    break;
                case 2:
                    mclas = MONS_WRAITH;
                    break;
                case 3:
                    mclas = MONS_FIRE_ELEMENTAL;
                    break;
                case 4:
                    mclas = MONS_ICE_BEAST;
                    break;
                case 5:
                    mclas = MONS_PHANTOM;
                    break;
                }
            }

            switch (mclas)
            {
            case MONS_GILA_MONSTER:
            case MONS_GIANT_ANT:
            case MONS_WOLF_SPIDER:
            case MONS_REDBACK:
                if (player_res_poison())
                    break;

                if (one_chance_in(20)
                    || (damage_taken > 3 && one_chance_in(4)))
                {
                    simple_monster_message( attacker, 
                                            "'s bite was poisonous!" );

                    if (attacker->type == MONS_REDBACK)
                        poison_player( random2avg(9, 2) + 3 );
                    else
                        poison_player(1);
                }
                break;

            case MONS_KILLER_BEE:
            case MONS_BUMBLEBEE:
                if (!player_res_poison()
                    && (one_chance_in(20)
                        || (damage_taken > 2 && one_chance_in(3))))

                {
                    simple_monster_message( attacker, " stings you!" );

                    if (attacker->type == MONS_BUMBLEBEE)
                        poison_player( random2(3) );
                    else
                        poison_player(1);
                }
                break;

            case MONS_ROTTING_DEVIL:
            case MONS_NECROPHAGE:
            case MONS_GHOUL:
            case MONS_DEATH_OOZE:
                if (you.is_undead)
                    break;

                // both sides call random2() - looking familiar by now {dlb}
                if (one_chance_in(20) || (damage_taken > 2 && one_chance_in(3)))
                {
                    rot_player( 2 + random2(3) );

                    if (damage_taken > 5)
                        rot_hp(1);
                }

                if (one_chance_in(4))
                    disease_player( 50 + random2(100) );
                break;

            case MONS_KOMODO_DRAGON:
            case MONS_GIANT_MOSQUITO:
                if (!one_chance_in(3))
                    disease_player( 50 + random2(100) );
                break;

            case MONS_FIRE_VORTEX:
                attacker->hit_points = -10;
                // fall through -- intentional? {dlb}
            case MONS_FIRE_ELEMENTAL:
            case MONS_BALRUG:
            case MONS_SUN_DEMON:
                strcpy(info, "You are engulfed in flames");

                resistValue = player_res_fire();
                extraDamage = 15 + random2(15);
                if (resistValue > 0)
                {
                    extraDamage /= (1 + resistValue * resistValue);
                }
                else
                {
                    if (resistValue < 0)
                        extraDamage += 8 + random2(8);
                }

                strcat(info, (extraDamage < 10) ? "." :
                             (extraDamage < 25) ? "!" :
                             "!!");

                mpr(info);

                if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
                {
                    mpr("Your icy shield dissipates!", MSGCH_DURATION);
                    you.duration[DUR_CONDENSATION_SHIELD] = 0;
                    you.redraw_armour_class = 1;
                }

                damage_taken += extraDamage;
                expose_player_to_element(BEAM_FIRE, 1);
                break;

            case MONS_SMALL_SNAKE:
            case MONS_SNAKE:
            case MONS_GIANT_MITE:
            case MONS_GOLD_MIMIC:
            case MONS_WEAPON_MIMIC:
            case MONS_ARMOUR_MIMIC:
            case MONS_SCROLL_MIMIC:
            case MONS_POTION_MIMIC:
                if (!player_res_poison()
                    && (one_chance_in(20)
                        || (damage_taken > 2 && one_chance_in(4))))
                {
                    poison_player(1);
                }
                break;

            case MONS_QUEEN_BEE:
            case MONS_GIANT_CENTIPEDE:
            case MONS_SOLDIER_ANT:
            case MONS_QUEEN_ANT:
                if (!player_res_poison())
                {
                    strcpy(info, ptr_monam(attacker, DESC_CAP_THE));
                    strcat(info, " stings you!");
                    mpr(info);

                    poison_player(2);
                }
                break;

            case MONS_SCORPION:
            case MONS_BROWN_SNAKE:
            case MONS_BLACK_SNAKE:
            case MONS_YELLOW_SNAKE:
            case MONS_SPINY_FROG:
                if (!player_res_poison()
                    && (one_chance_in(15)
                        || (damage_taken > 2 && one_chance_in(4))))
                        // ^^^yep, this should be a function^^^ {dlb}
                {
                    strcpy(info, ptr_monam(attacker, DESC_CAP_THE));
                    strcat(info, " poisons you!");
                    mpr(info);

                    poison_player(1);
                }
                break;

            case MONS_SHADOW_DRAGON:
            case MONS_SPECTRAL_THING:
                if (coinflip())
                    break;
                // fall-through {dlb}
            case MONS_WIGHT:    // less likely because wights do less damage
            case MONS_WRAITH:

            // enum does not match comment 14jan2000 {dlb}
            case MONS_SOUL_EATER:       // shadow devil

            // enum does not match comment 14jan2000 {dlb}
            case MONS_SPECTRAL_WARRIOR: // spectre

            case MONS_SHADOW_FIEND:
            case MONS_ORANGE_RAT:
            case MONS_SHADOW_WRAITH:
            case MONS_ANCIENT_LICH:
            case MONS_LICH:
            case MONS_BORIS:
                if (one_chance_in(30) || (damage_taken > 5 && coinflip()))
                    drain_exp();
                break;


            case MONS_RED_WASP:
                if (!player_res_poison())
                    poison_player( (coinflip() ? 2 : 1) );
                // intentional fall-through {dlb}
            case MONS_YELLOW_WASP:
                strcpy(info, ptr_monam(attacker, DESC_CAP_THE));
                strcat(info, " stings you.");
                mpr(info);

                if (!player_res_poison()
                    && (one_chance_in(20)
                        || (damage_taken > 2 && !one_chance_in(3))))
                        // maybe I should flip back the other way? {dlb}
                {
                    if (you.paralysis > 0)
                        mpr("You still can't move!", MSGCH_WARN);
                    else
                        mpr("You suddenly lose the ability to move!",
                            MSGCH_WARN);
                    if ( you.paralysis == 0 || mclas == MONS_RED_WASP )
                        you.paralysis += 1 + random2(3);
                }
                break;

            case MONS_SPINY_WORM:
                if (!player_res_poison())
                {
                    strcpy(info, ptr_monam(attacker, DESC_CAP_THE));
                    strcat(info, " stings you!");
                    mpr(info);

                    poison_player( 2 + random2(4) );
                }
                // intentional fall-through {dlb}
            case MONS_BROWN_OOZE:
            case MONS_ACID_BLOB:
            case MONS_ROYAL_JELLY:
            case MONS_JELLY:
                mpr("You are splashed with acid!");
                splash_with_acid(3);
                break;

            case MONS_SIMULACRUM_SMALL:
            case MONS_SIMULACRUM_LARGE:
                resistValue = player_res_cold();
                if (resistValue > 0)
                    extraDamage = 0;
                else if (resistValue == 0)
                {
                    extraDamage += roll_dice( 1, 4 );
                    mprf("%s chills you.", ptr_monam(attacker, DESC_CAP_THE));
                }
                else if (resistValue < 0)
                {
                    extraDamage = roll_dice( 2, 4 );
                    mprf("%s freezes you.", ptr_monam(attacker, DESC_CAP_THE));
                }
                
                damage_taken += extraDamage;
                expose_player_to_element(BEAM_COLD, 1);
                break;

            case MONS_ICE_DEVIL:
            case MONS_ICE_BEAST:
            case MONS_FREEZING_WRAITH:
            case MONS_ICE_FIEND:
            case MONS_WHITE_IMP:
            case MONS_ANTAEUS:
            case MONS_AZURE_JELLY:
                extraDamage = attacker->hit_dice + random2(attacker->hit_dice * 2);
                resistValue = player_res_cold();

                if (resistValue > 0)
                {
                    extraDamage /= (1 + resistValue * resistValue);
                }

                if (resistValue < 0)
                {
                    extraDamage += attacker->hit_dice +
                        random2(attacker->hit_dice * 2);
                }

                if (extraDamage > 4)
                {
                    strcpy(info, ptr_monam(attacker, DESC_CAP_THE));
                    if (extraDamage < 10)
                        strcat(info, " chills you.");
                    else
                        strcat(info, " freezes you!");
                    if (extraDamage > 19)
                        strcat(info, "!");
                    mpr(info);
                }

                damage_taken += extraDamage;

                expose_player_to_element(BEAM_COLD, 1);
                break;

            case MONS_ELECTRIC_GOLEM:
                if (!player_res_electricity())
                {
                    damage_taken += attacker->hit_dice
                                            + random2(attacker->hit_dice * 2);

                    strcpy(info, ptr_monam(attacker, DESC_CAP_THE));
                    strcat(info, " shocks you!");
                    mpr(info);
                }
                break;

            case MONS_VAMPIRE:
                if (you.is_undead)
                    break;

/* ******************************************************************
                if ( damage_taken > 6 && one_chance_in(3) || !one_chance_in(20))
                {
                    mpr("You feel less resilient.");
                    you.hp_max -= ( coinflip() ? 2 : 1 );
                    deflate_hp(you.hp_max, false);
                    heal_monster(attacker, 5 + random2(8), true);
                }
****************************************************************** */

                // heh heh {dlb}
                // oh, this is mean!  {gdl} 
                if (heal_monster(attacker, random2(damage_taken), true))
                    simple_monster_message(attacker, " draws strength from your injuries!");

                break;

            case MONS_SHADOW:
                if (player_prot_life() <= random2(3)
                    && (one_chance_in(20)
                        || (damage_taken > 0 && one_chance_in(3))))
                {
                    lose_stat(STAT_STRENGTH, 1);
                }
                break;

            case MONS_HUNGRY_GHOST:
                if (you.is_undead == US_UNDEAD)
                    break;

                if (one_chance_in(20) || (damage_taken > 0 && coinflip()))
                    make_hungry(400, false);
                break;

            case MONS_GUARDIAN_NAGA:
                break;

            case MONS_PHANTOM:
            case MONS_INSUBSTANTIAL_WISP:
            case MONS_BLINK_FROG:
            case MONS_MIDGE:
                if (one_chance_in(3))
                {
                    simple_monster_message(attacker, " blinks.");
                    monster_blink(attacker);
                }
                break;

            case MONS_JELLYFISH:
            case MONS_ORANGE_DEMON:
                // if ( !one_chance_in(3) ) break;
                if (player_res_poison())
                    break;

                if (attacker->type == MONS_ORANGE_DEMON
                    && (!one_chance_in(4) || runthru != 1))
                {
                    break;
                }

                strcpy(info, ptr_monam(attacker, DESC_CAP_THE));
                strcat(info, " stings you!");
                mpr(info);

                poison_player(1);
                lose_stat(STAT_STRENGTH, 1);
                break;

            case MONS_PULSATING_LUMP:
                if (one_chance_in(3))
                {
                    if (one_chance_in(5))
                        mutate(100);
                    else
                        give_bad_mutation();
                }
                break;

            case MONS_MOTH_OF_WRATH:
                if (one_chance_in(3))
                {
                    simple_monster_message(attacker, " infuriates you!");
                    go_berserk(false);
                }
                break;
		    
            default:
                break;
            }                   // end of switch for special attacks.
            /* use break for level drain, maybe with beam variables,
               because so many creatures use it. */
        }

        /* special weapons */
        if (hit
            && (attacker->inv[hand_used] != NON_ITEM
                || ((attacker->type == MONS_PLAYER_GHOST
                        || attacker->type == MONS_PANDEMONIUM_DEMON)
                    && ghost.values[ GVAL_BRAND ] != SPWPN_NORMAL)))
        {
            unsigned char itdam;

            if (attacker->type == MONS_PLAYER_GHOST
                || attacker->type == MONS_PANDEMONIUM_DEMON)
            {
                itdam = ghost.values[ GVAL_BRAND ];
            }
            else
            {
                const item_def& mons_weapon = mitm[attacker->inv[hand_used]];
                if ( is_range_weapon(mons_weapon) )
                    itdam = SPWPN_NORMAL;
                else
                    itdam = mons_weapon.special;
            }

            specdam = 0;

            switch (itdam)
            {
            case SPWPN_NORMAL:
            default:
                break;

            case SPWPN_SWORD_OF_CEREBOV:
            case SPWPN_FLAMING:
                specdam = 0;
                resistValue = player_res_fire();

                if (itdam == SPWPN_SWORD_OF_CEREBOV)
                    resistValue -= 1;

                if (resistValue > 0)
                {
                    damage_taken += (random2(damage_taken) / 2 + 1) /
                                        (1 + (resistValue * resistValue));
                }

                if (resistValue <= 0)
                    specdam = random2(damage_taken) / 2 + 1;

                if (resistValue < 0)
                    specdam += random2(damage_taken) / 2 + 1;

                if (specdam)
                {
                    simple_monster_message(attacker, " burns you.");
/* **********************

commented out for now
                    if (specdam < 3)
                      strcat(info, ".");
                    if (specdam >= 3 && specdam < 7)
                      strcat(info, "!");
                    if (specdam >= 7)
                      strcat(info, "!!");

*********************** */
                }

                if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
                {
                    mpr("Your icy shield dissipates!", MSGCH_DURATION);
                    you.duration[DUR_CONDENSATION_SHIELD] = 0;
                    you.redraw_armour_class = 1;
                }
                break;

            case SPWPN_FREEZING:
                specdam = 0;
                resistValue = player_res_cold();

                if (resistValue <= 0)
                    specdam = random2(damage_taken) / 2 + 1;

                if (resistValue < 0)
                    specdam += random2(damage_taken) / 2 + 1;

                if (resistValue > 0)
                {
                    damage_taken += (random2(damage_taken) / 2 + 1) /
                                        (1 + (resistValue * resistValue));
                }

                if (specdam)
                {
                    simple_monster_message(attacker, " freezes you.");

/* **********************

commented out for now
                    if (specdam < 3)
                      strcat(info, ".");
                    if (specdam >= 3 && specdam < 7)
                      strcat(info, "!");
                    if (specdam >= 7)
                      strcat(info, "!!");

*********************** */
                }
                break;


            case SPWPN_HOLY_WRATH:
                if (attacker->type == MONS_PLAYER_GHOST)
                    break;  // ghosts can't wield holy wrath

                if (you.is_undead)
                {
                    specdam = random2(damage_taken);

                    if (specdam)
                    {
                        strcpy(info, "The wound is extremely painful");

                        if (specdam < 3)
                            strcat(info, ".");
                        else if (specdam < 7)
                            strcat(info, "!");
                        else
                            strcat(info, "!!");

                        mpr(info);
                    }
                }
                break;

            case SPWPN_ELECTROCUTION:
                // This runs the risk of making levitation into a
                // cheap version of the Resist spell (with added
                // bonuses), so it shouldn't be used anywhere else,
                // and should possibly be removed from this case as
                // well. -- bwr
                if (player_is_levitating()) // you're not grounded
                    break;

                if (player_res_electricity())   // resist lightning
                    break;

                specdam = 0;

                // This is related to old code where elec wpns had charges
#if 0
                if (menv[monster_attacking].type != MONS_PLAYER_GHOST
                    && (mitm[attacker->inv[hand_used]].plus2 <= 0
                        || item_cursed( mitm[attacker->inv[hand_used]] )))
                {
                    break;
                }
#endif

                if (one_chance_in(3))
                {
                    mpr("You are electrocuted!");
                    specdam += 10 + random2(15);
                }
                break;

            case SPWPN_ORC_SLAYING:
                if (you.species == SP_HILL_ORC)
                {
                    specdam = random2(damage_taken);

                    if (specdam)
                    {
                        strcpy(info, "The wound is extremely painful");

                        if (specdam < 3)
                            strcat(info, ".");
                        else if (specdam < 7)
                            strcat(info, "!");
                        else
                            strcat(info, "!!");

                        mpr(info);
                    }
                }
                break;

            case SPWPN_STAFF_OF_OLGREB:
            case SPWPN_VENOM:
                if (!player_res_poison() && one_chance_in(3))
                {
                    simple_monster_message(attacker,
                            (attacker->type == MONS_DANCING_WEAPON)
                            ? " is poisoned!" : "'s weapon is poisoned!");

                    poison_player(2);
                }
                break;

            case SPWPN_PROTECTION:
                break;

            case SPWPN_DRAINING:
                drain_exp();
                specdam = random2(damage_taken) / (2 + player_prot_life()) + 1;
                break;

            case SPWPN_SPEED:
                wpn_speed = (wpn_speed + 1) / 2;
                break;

            case SPWPN_VORPAL:
                specdam = 1 + (random2(damage_taken) / 2);
                break;

            case SPWPN_VAMPIRES_TOOTH:
            case SPWPN_VAMPIRICISM:
                specdam = 0;        // note does no extra damage

                if (you.is_undead)
                    break;

                if (one_chance_in(5))
                    break;

                // heh heh {dlb}
                if (heal_monster(attacker, 1 + random2(damage_taken), true))
                    simple_monster_message(attacker, " draws strength from your injuries!");
                break;

            case SPWPN_DISRUPTION:
                if (attacker->type == MONS_PLAYER_GHOST)
                    break;

                if (you.is_undead)
                {
                    specdam = random2(damage_taken) + random2(damage_taken)
                            + random2(damage_taken) + random2(damage_taken);

                    if (specdam)
                    {
                        strcpy(info, "You are blasted by holy energy");

                        if (specdam < 7)
                            strcat(info, ".");
                        else if (specdam < 15)
                            strcat(info, "!");
                        else
                            strcat(info, "!!");

                        mpr(info);
                    }
                }
                break;


            case SPWPN_DISTORTION:
                if (one_chance_in(3))
                {
                    mpr("Your body is twisted painfully.");
                    specdam += 1 + random2avg(7, 2);
                    break;
                }

                if (one_chance_in(3))
                {
                    mpr("Your body is terribly warped!");
                    specdam += 3 + random2avg(24, 2);
                    break;
                }

                if (one_chance_in(3))
                {
                    random_blink(true);
                    break;
                }

                if (coinflip())
                {
                    you_teleport();
                    break;
                }

                if (coinflip())
                {
                    you_teleport2( true, one_chance_in(5) );
                    break;
                }

                if (coinflip() && you.level_type != LEVEL_ABYSS)
                {
                    you.banished = true;
                    break;
                }
                break;
            }               // end of switch
        }                       // end of special weapons

        damage_taken += specdam;

        if (damage_taken > 0)
        {
            ouch(damage_taken, monster_attacking, KILLED_BY_MONSTER);

            if (you.religion == GOD_XOM && you.hp <= you.hp_max / 3
                && one_chance_in(10))
            {
                Xom_acts(true, you.experience_level, false);
            }
        }

        mons_lose_attack_energy(attacker, wpn_speed, runthru);
    }                           // end of for runthru

    if (player_perceives_attack)
        interrupt_activity(AI_MONSTER_ATTACKS, attacker);

    return;
}                               // end monster_attack()

bool monsters_fight(int monster_attacking, int monster_attacked)
{
    struct monsters *attacker = &menv[monster_attacking];
    struct monsters *defender = &menv[monster_attacked];

    int weapon = -1;            // monster weapon, if any
    int damage_taken = 0;
    bool hit = false;
    //    int mmov_x = 0;
    bool water_attack = false;
    int specdam = 0;
    int hand_used = 0;
    bool sees = false;
    int wpn_speed;               // 0 == didn't use actual weapon
    int habitat = monster_habitat( attacker->type );
    char str_pass[ ITEMNAME_SIZE ];

#if DEBUG_DIAGNOSTICS
    char st_prn[ 20 ];
#endif

    if (attacker->type == MONS_GIANT_SPORE
        || attacker->type == MONS_BALL_LIGHTNING)
    {
        attacker->hit_points = -1;
        return false;
    }

    if (mons_has_ench( attacker, ENCH_SUBMERGED ) 
        && habitat != DNGN_FLOOR
        && habitat != monster_habitat( defender->type ))
    {
        return false;
    }

    if (attacker->fumbles_attack(true))
        return true;

    // habitat is the favoured habitat of the attacker
    water_attack = defender->floundering() && attacker->swimming();

    if (mons_near(attacker) && mons_near(defender))
        sees = true;

    // Any objects seen in combat are thereafter known mimics.
    if (mons_is_mimic(attacker->type) && mons_near(attacker))
        attacker->flags |= MF_KNOWN_MIMIC;

    if (mons_is_mimic(defender->type) && mons_near(defender))
        defender->flags |= MF_KNOWN_MIMIC;

    // now disturb defender, regardless
    behaviour_event(defender, ME_WHACK, monster_attacking);

    int heads = 0;

    if (attacker->type == MONS_HYDRA)
        heads = attacker->number;
    
    for (int runthru = 0; runthru < 4; runthru++)
    {
        if (attacker->type == MONS_HYDRA)
        {
            runthru = 0;
            if (heads-- < 1)
                break;
        }
        
        int mdam = mons_damage(attacker->type, runthru);
        wpn_speed = 0;

        if (attacker->type == MONS_ZOMBIE_SMALL
            || attacker->type == MONS_ZOMBIE_LARGE
            || attacker->type == MONS_SKELETON_SMALL
            || attacker->type == MONS_SKELETON_LARGE
            || attacker->type == MONS_SIMULACRUM_SMALL
            || attacker->type == MONS_SIMULACRUM_LARGE
            || attacker->type == MONS_SPECTRAL_THING)
            // what do these things have in common? {dlb}
        {
            mdam = mons_damage(attacker->number, runthru);
            // cumulative reductions - series of if-conditions
            // is necessary: {dlb}
            if (mdam > 1)
                mdam--;
            if (mdam > 2)
                mdam--;
            if (mdam > 5)
                mdam--;
            if (mdam > 9)
                mdam--;         // was: "-= 2" {dlb}
        }

        if (mdam == 0)
            break;

        if (mons_wields_two_weapons(attacker)
            && runthru == 1 && attacker->inv[MSLOT_MISSILE] != NON_ITEM)
        {
            hand_used = 1;
        }

        damage_taken = 0;

        int mons_to_hit = 20 + attacker->hit_dice * 5;
                                // * menv [monster_attacking].hit_dice; // * 3

        if (water_attack)
            mons_to_hit += 5;

        weapon = attacker->inv[hand_used];

        if (weapon != NON_ITEM)
        {
            mons_to_hit += mitm[weapon].plus;
            // mons_to_hit += 3 * property( mitm[weapon], PWPN_HIT );
            mons_to_hit += property( mitm[weapon], PWPN_HIT );

            wpn_speed = property( mitm[ weapon ], PWPN_SPEED );
        }

        mons_to_hit = random2(mons_to_hit);

        if (mons_to_hit >= defender->evasion ||
            ((mons_is_paralysed(defender) || defender->behaviour == BEH_SLEEP)
             && !one_chance_in(20)))
        {
            hit = true;

            if (attacker->inv[hand_used] != NON_ITEM
                && mitm[attacker->inv[hand_used]].base_type == OBJ_WEAPONS
                && !is_range_weapon( mitm[attacker->inv[hand_used]] ))
            {
                damage_taken = random2(property( mitm[attacker->inv[hand_used]],
                                                 PWPN_DAMAGE ));

                if (get_equip_race(mitm[attacker->inv[hand_used]]) == ISFLAG_ORCISH
                    && mons_species(attacker->type) == MONS_ORC
                    && coinflip())
                {
                    damage_taken++;
                }

                //if (mitm[mons_inv[i][0]].plus > 80) damage_taken -= 100;
                //  damage_taken += mitm[mons_inv[i][0]].plus;

                if (mitm[attacker->inv[hand_used]].plus2 >= 0)
                {
                    /* + or 0 to-dam */
                    damage_taken += random2(mitm[attacker->inv[hand_used]].plus2 + 1);
                }
                else
                {
                    /* - to-dam */
                    damage_taken -= random2(abs(mitm[attacker->inv[hand_used]].plus2 + 1));
                }

                damage_taken -= 1 + random2(3); //1;
            }

            damage_taken += 1 + random2(mdam);

            if (water_attack)
                damage_taken *= 2;

            damage_taken -= random2(1 + defender->armour_class);

            if (damage_taken < 1)
                damage_taken = 0;
        }
        else
        {
            hit = false;

            if (sees)
            {
                strcpy(info, ptr_monam(attacker, DESC_CAP_THE));
                strcat(info, " misses ");
                if (attacker == defender)
                    strcat(info,
                           mons_pronoun(attacker->type, PRONOUN_REFLEXIVE));
                else
                    strcat(info, ptr_monam(defender, DESC_NOCAP_THE));
                strcat(info, ".");
                mpr(info);
            }
        }

        if (damage_taken < 1 && hit)
        {
            if (sees)
            {
                strcpy(info, ptr_monam(attacker, DESC_CAP_THE));
                strcat(info, " hits ");
                if ( attacker == defender )
                    strcat(info,
                           mons_pronoun(attacker->type, PRONOUN_REFLEXIVE));
                else                    
                    strcat(info, ptr_monam(defender, DESC_NOCAP_THE));
#if DEBUG_DIAGNOSTICS
                strcat(info, " for ");
                // note: doesn't take account of special weapons etc
                itoa(damage_taken, st_prn, 10);
                strcat(info, st_prn);
#endif
                strcat(info, ".");      // but doesn't do any you.damage.");
                mpr(info);
            }
        }

        if (hit)                //(int) damage_taken > 0)
        {
            int mmov_x = attacker->inv[hand_used];

            if (sees)
            {
                strcpy(info, ptr_monam(attacker, DESC_CAP_THE));
                strcat(info, " hits ");
                if (attacker == defender)
                    strcat(info,
                           mons_pronoun(attacker->type, PRONOUN_REFLEXIVE));
                else
                    strcat(info, ptr_monam(defender, DESC_NOCAP_THE));

                if (attacker->type != MONS_DANCING_WEAPON
                    && attacker->inv[hand_used] != NON_ITEM
                    && mitm[attacker->inv[hand_used]].base_type == OBJ_WEAPONS
                    && !is_range_weapon( mitm[attacker->inv[hand_used]] ))
                {
                    strcat(info, " with ");
                    it_name(mmov_x, DESC_NOCAP_A, str_pass);       // was 7
                    strcat(info, str_pass);
                }

                strcat(info, "! ");
                mpr(info);
            }

            if (actor_decapitates_hydra(attacker, defender, damage_taken))
            {
                mons_lose_attack_energy(attacker, wpn_speed, runthru);
                
                if (defender->hit_points < 1)
                {
                    monster_die(defender, KILL_MON, monster_attacking);
                    return (true);
                }

                // Skip rest of attack.
                continue;
            }

            // special attacks:
            switch (attacker->type)
            {
            // enum does not match comment 14jan2000 {dlb}
            case MONS_CENTAUR:  // cockatrice
            case MONS_JELLY:
            case MONS_GUARDIAN_NAGA:
                break;

            case MONS_GIANT_ANT:
            case MONS_WOLF_SPIDER:
            case MONS_REDBACK:
            case MONS_SPINY_WORM:
            case MONS_JELLYFISH:
            case MONS_ORANGE_DEMON:
                if (attacker->type == MONS_SPINY_WORM || one_chance_in(20)
                    || (damage_taken > 3 && one_chance_in(4)))
                {
                    if (sees)
                    {
                        strcpy(info, ptr_monam(attacker, DESC_CAP_THE));
                        strcat(info, " stings ");
                        if (attacker == defender)
                            strcat(info, mons_pronoun(attacker->type,
                                                      PRONOUN_REFLEXIVE));
                        else
                            strcat(info, ptr_monam(defender, DESC_NOCAP_THE));
                        strcat(info, ".");
                        mpr(info);
                    }
                    poison_monster(defender, false);
                }
                break;

            case MONS_KILLER_BEE:
            case MONS_BUMBLEBEE:
                if (one_chance_in(20)
                    || (damage_taken > 2 && one_chance_in(3)))
                {
                    if (sees)
                    {
                        strcpy(info, ptr_monam(attacker, DESC_CAP_THE));
                        strcat(info, " stings ");
                        if (attacker == defender)
                            strcat(info, mons_pronoun(attacker->type,
                                                      PRONOUN_REFLEXIVE));
                        else
                            strcat(info, ptr_monam(defender, DESC_NOCAP_THE));
                        strcat(info, ".");
                        mpr(info);
                    }
                    poison_monster(defender, false);
                }
                break;

            case MONS_NECROPHAGE:
            case MONS_ROTTING_DEVIL:
            case MONS_GHOUL:
            case MONS_DEATH_OOZE:
                if (mons_res_negative_energy( defender ))
                    break;

                if (one_chance_in(20)
                    || (damage_taken > 2 && one_chance_in(3)))
                {
                    defender->max_hit_points -= 1 + random2(3);

                    if (defender->hit_points > defender->max_hit_points)
                        defender->hit_points = defender->max_hit_points;
                }
                break;

            case MONS_FIRE_VORTEX:
                attacker->hit_points = -10;
                // deliberate fall-through
            case MONS_FIRE_ELEMENTAL:
            case MONS_BALRUG:
            case MONS_SUN_DEMON:
                specdam = 0;
                if (mons_res_fire(defender) == 0)
                    specdam = 15 + random2(15);
                else if (mons_res_fire(defender) < 0)
                    specdam = 20 + random2(25);

                if (specdam)
                    simple_monster_message(defender, " is engulfed in flame!");

                damage_taken += specdam;
                break;

            case MONS_QUEEN_BEE:
            case MONS_GIANT_CENTIPEDE:
            case MONS_SOLDIER_ANT:
            case MONS_QUEEN_ANT:
                //if ((damage_taken > 2 && one_chance_in(3) ) || one_chance_in(20) )
                //{
                if (sees)
                {
                    strcpy(info, ptr_monam(attacker, DESC_CAP_THE));
                    strcat(info, " stings ");
                    if (attacker == defender)
                        strcat(info, mons_pronoun(attacker->type,
                                                  PRONOUN_REFLEXIVE));
                    else
                        strcat(info, ptr_monam(defender, DESC_NOCAP_THE));
                    strcat(info, ".");
                    mpr(info);
                }
                poison_monster(defender, false);
                //}
                break;

            // enum does not match comment 14jan2000 {dlb}
            case MONS_SCORPION: // snake
            case MONS_BROWN_SNAKE:
            case MONS_BLACK_SNAKE:
            case MONS_YELLOW_SNAKE:
            case MONS_GOLD_MIMIC:
            case MONS_WEAPON_MIMIC:
            case MONS_ARMOUR_MIMIC:
            case MONS_SCROLL_MIMIC:
            case MONS_POTION_MIMIC:
                if (one_chance_in(20) || (damage_taken > 2 && one_chance_in(4)))
                    poison_monster(defender, false);
                break;

            case MONS_SHADOW_DRAGON:
            case MONS_SPECTRAL_THING:
                if (coinflip())
                    break;
                // intentional fall-through
            case MONS_WIGHT:
            case MONS_WRAITH:
            case MONS_SOUL_EATER:
            case MONS_SHADOW_FIEND:
            case MONS_SPECTRAL_WARRIOR:
            case MONS_ORANGE_RAT:
            case MONS_ANCIENT_LICH:
            case MONS_LICH:
            case MONS_BORIS:
                if (mons_res_negative_energy( defender ))
                    break;

                if (one_chance_in(30) || (damage_taken > 5 && coinflip()))
                {
                    simple_monster_message(defender, " is drained.");

                    if (one_chance_in(5))
                        defender->hit_dice--;

                    defender->max_hit_points -= 2 + random2(3);
                    defender->hit_points -= 2 + random2(3);

                    if (defender->hit_points >= defender->max_hit_points)
                        defender->hit_points = defender->max_hit_points;

                    if (defender->hit_points < 1 || defender->hit_dice < 1)
                    {
                        monster_die(defender, KILL_MON, monster_attacking);
                        return true;
                    }
                }
                break;

            // enum does not match comment 14jan2000 {dlb}
            case MONS_WORM:     // giant wasp
                break;

            case MONS_SIMULACRUM_SMALL:
            case MONS_SIMULACRUM_LARGE:
                specdam = 0;

                if (mons_res_cold(defender) == 0)
                    specdam = roll_dice( 1, 4 );
                else if (mons_res_cold(defender) < 0)
                    specdam = roll_dice( 2, 4 );

                if (specdam && sees)
                {
                    strcpy(info, ptr_monam(attacker, DESC_CAP_THE));
                    strcat(info, " freezes ");
                    if (attacker == defender)
                        strcat(info, mons_pronoun(attacker->type,
                                                  PRONOUN_REFLEXIVE));
                    else
                        strcat(info, ptr_monam(defender, DESC_NOCAP_THE));
                    strcat(info, ".");
                    mpr(info);
                }

                damage_taken += specdam;
                break;

            case MONS_ICE_DEVIL:
            case MONS_ICE_BEAST:
            case MONS_FREEZING_WRAITH:
            case MONS_ICE_FIEND:
            case MONS_WHITE_IMP:
            case MONS_AZURE_JELLY:
            case MONS_ANTAEUS:
                specdam = 0;
                if (mons_res_cold(defender) == 0)
                {
                    specdam = attacker->hit_dice + random2(attacker->hit_dice * 2);
                }
                else if (mons_res_cold(defender) < 0)
                {
                    specdam = random2(attacker->hit_dice * 3) + (attacker->hit_dice * 2);
                }

                if (specdam && sees)
                {
                    strcpy(info, ptr_monam(attacker, DESC_CAP_THE));
                    strcat(info, " freezes ");
                    if (attacker == defender)
                        strcat(info, mons_pronoun(attacker->type,
                                                  PRONOUN_REFLEXIVE));
                    else
                        strcat(info, ptr_monam(defender, DESC_NOCAP_THE));
                    strcat(info, ".");
                    mpr(info);
                }
                damage_taken += specdam;
                break;

            case MONS_ELECTRIC_GOLEM:
                if (mons_flies(defender) == 0 && mons_res_elec(defender) == 0)
                {
                    specdam = attacker->hit_dice + random2(attacker->hit_dice * 2);
                }

                if (specdam && sees)
                {
                    strcpy(info, ptr_monam(attacker, DESC_CAP_THE));
                    strcat(info, " shocks ");
                    if (attacker == defender)
                        strcat(info, mons_pronoun(attacker->type,
                                                  PRONOUN_REFLEXIVE));
                    else
                        strcat(info, ptr_monam(defender, DESC_NOCAP_THE));
                    strcat(info, ".");
                    mpr(info);
                }
                damage_taken += specdam;
                break;

            case MONS_VAMPIRE:
            case MONS_VAMPIRE_KNIGHT:
            case MONS_VAMPIRE_MAGE:
                if (mons_res_negative_energy(defender) > 0)
                    break;

                // heh heh {dlb}
                if (heal_monster(attacker, random2(damage_taken), true))
                    simple_monster_message(attacker, " is healed.");
                break;
            }
        }

        // special weapons:
        if (hit
            && (attacker->inv[hand_used] != NON_ITEM
                || ((attacker->type == MONS_PLAYER_GHOST
                        || attacker->type == MONS_PANDEMONIUM_DEMON)
                     && ghost.values[ GVAL_BRAND ] != SPWPN_NORMAL)))
        {
            unsigned char itdam;

            if (attacker->type == MONS_PLAYER_GHOST
                || attacker->type == MONS_PANDEMONIUM_DEMON)
            {
                itdam = ghost.values[ GVAL_BRAND ];
            }
            else
                itdam = mitm[attacker->inv[hand_used]].special;

            specdam = 0;

            if (attacker->type == MONS_PLAYER_GHOST
                || attacker->type == MONS_PANDEMONIUM_DEMON)
            {
                switch (itdam)
                {
                case SPWPN_NORMAL:
                default:
                    break;

                case SPWPN_SWORD_OF_CEREBOV:
                case SPWPN_FLAMING:
                    specdam = 0;

                    if (itdam == SPWPN_SWORD_OF_CEREBOV 
                        || mons_res_fire(defender) <= 0)
                    {
                        specdam = 1 + random2(damage_taken);
                    }

                    if (specdam)
                    {
                        if (sees)
                        {
                            strcpy(info, ptr_monam(attacker, DESC_CAP_THE));
                            strcat(info, " burns ");
                            if (attacker == defender)
                                strcat(info, mons_pronoun(attacker->type,
                                                          PRONOUN_REFLEXIVE));
                            else
                                strcat(info, ptr_monam(defender, DESC_NOCAP_THE ));

                            if (specdam < 3)
                                strcat(info, ".");
                            else if (specdam < 7)
                                strcat(info, "!");
                            else
                                strcat(info, "!!");

                            mpr(info);
                        }
                    }
                    break;

                case SPWPN_FREEZING:
                    specdam = 0;

                    if (mons_res_cold(defender) <= 0)
                        specdam = 1 + random2(damage_taken);

                    if (specdam)
                    {
                        if (sees)
                        {
			  //                            mmov_x = attacker->inv[hand_used];

                            strcpy(info, ptr_monam(attacker, DESC_CAP_THE));
                            strcat(info, " freezes ");
                            if (attacker == defender)
                                strcat(info, mons_pronoun(attacker->type,
                                                          PRONOUN_REFLEXIVE));
                            else
                                strcat(info, ptr_monam(defender, DESC_NOCAP_THE));

                            if (specdam < 3)
                                strcat(info, ".");
                            else if (specdam < 7)
                                strcat(info, "!");
                            else
                                strcat(info, "!!");

                            mpr(info);
                        }
                    }
                    break;

                case SPWPN_HOLY_WRATH:
                    if (attacker->type == MONS_PLAYER_GHOST)
                        break;
                    specdam = 0;
                    switch (mons_holiness(defender))
                    {
                    case MH_HOLY:
                        // I would think that it would do zero damage {dlb}
                        damage_taken -= 5 + random2(5);
                        break;

                    case MH_UNDEAD:
                        specdam += 1 + random2(damage_taken);
                        break;

                    case MH_DEMONIC:
                        specdam += 1 + (random2(damage_taken) * 15) / 10;
                        break;

                    default:
                        break;
                    }
                    break;

                case SPWPN_ELECTROCUTION:
                    //if ( attacker->type == MONS_PLAYER_GHOST ) break;
                    if (mons_flies(defender) > 0 || mons_res_elec(defender) > 0)
                        break;

                    specdam = 0;

#if 0
                    // more of the old code... -- bwr
                    if (menv[monster_attacking].type != MONS_PLAYER_GHOST
                        && (mitm[attacker->inv[hand_used]].plus2 <= 0
                            || item_cursed( mitm[attacker->inv[hand_used]] )))
                    {
                        break;
                    }
#endif

                    if (one_chance_in(3))
                    {
                        if (sees)
                            mpr("There is a sudden explosion of sparks!");

                        specdam += 10 + random2(15);
                        //mitm[attacker->inv[hand_used]].plus2 --;
                    }
                    break;

                case SPWPN_ORC_SLAYING:
                    if (mons_species(defender->type) == MONS_ORC)
                        hurt_monster(defender, 1 + random2(damage_taken));
                    break;

                case SPWPN_STAFF_OF_OLGREB:
                case SPWPN_VENOM:
                    if (!one_chance_in(3))
                        poison_monster(defender, false);
                    break;

                    //case 7: // protection

                case SPWPN_DRAINING:
                    if (!mons_res_negative_energy( defender )
                        && (one_chance_in(30)
                            || (damage_taken > 5 && coinflip())))
                    {
                        simple_monster_message(defender, " is drained");

                        if (one_chance_in(5))
                            defender->hit_dice--;

                        defender->max_hit_points -= 2 + random2(3);
                        defender->hit_points -= 2 + random2(3);

                        if (defender->hit_points >= defender->max_hit_points)
                            defender->hit_points = defender->max_hit_points;

                        if (defender->hit_points < 1
                            || defender->hit_dice < 1)
                        {
                            monster_die(defender, KILL_MON, monster_attacking);
                            return true;
                        }
                        specdam = 1 + (random2(damage_taken) / 2);
                    }
                    break;

                case SPWPN_SPEED:
                    wpn_speed = (wpn_speed + 1) / 2;
                    break;

                case SPWPN_VORPAL:
                    specdam += 1 + (random2(damage_taken) / 2);
                    break;

                case SPWPN_VAMPIRES_TOOTH:
                case SPWPN_VAMPIRICISM:
                    specdam = 0;        // note does no extra damage

                    if (mons_res_negative_energy( defender ))
                        break;

                    if (one_chance_in(5))
                        break;

                    // heh heh {dlb}
                    if (heal_monster(attacker, 1 + random2(damage_taken), true))
                        simple_monster_message(attacker, " is healed.");
                    break;

                case SPWPN_DISRUPTION:
                    if (attacker->type == MONS_PLAYER_GHOST)
                        break;

                    specdam = 0;

                    if (mons_holiness(defender) == MH_UNDEAD
                        && !one_chance_in(3))
                    {
                        simple_monster_message(defender, " shudders.");
                        specdam += random2avg(1 + (3 * damage_taken), 3);
                    }
                    break;


                case SPWPN_DISTORTION:
                    if (one_chance_in(3))
                    {
                        if (mons_near(defender) 
                            && player_monster_visible(defender))
                        {
                            strcpy(info, "Space bends around ");
                            strcat(info, ptr_monam(defender, DESC_NOCAP_THE));
                            strcat(info, ".");
                            mpr(info);
                        }

                        specdam += 1 + random2avg(7, 2);
                        break;
                    }
                    if (one_chance_in(3))
                    {
                        if (mons_near(defender)
                            && player_monster_visible(defender))
                        {
                            strcpy(info, "Space warps horribly around ");
                            strcat(info, ptr_monam(defender, DESC_NOCAP_THE));
                            strcat(info, "!");
                            mpr(info);
                        }

                        specdam += 3 + random2avg(24, 2);
                        break;
                    }

                    if (one_chance_in(3))
                    {
                        monster_blink(defender);
                        break;
                    }

                    if (coinflip())
                    {
                        monster_teleport(defender, coinflip());
                        break;
                    }

                    if (coinflip())
                    {
                        monster_die(defender, KILL_RESET, monster_attacking);
                        break;
                    }
                    break;
                }
            }
        }                       // end of if special weapon

        damage_taken += specdam;

        if (damage_taken > 0)
        {
            hurt_monster(defender, damage_taken);

            if (defender->hit_points < 1)
            {
                monster_die(defender, KILL_MON, monster_attacking);
                return true;
            }
        }

        mons_lose_attack_energy(attacker, wpn_speed, runthru);
    }                           // end of for runthru

    return true;
}                               // end monsters_fight()


/*
 **************************************************
 *                                                *
 *              END PUBLIC FUNCTIONS              *
 *                                                *
 **************************************************
*/

// Returns a value between 0 and 10 representing the weight given to str
int weapon_str_weight( int wpn_class, int wpn_type )
{
    int  ret;

    const int  wpn_skill  = weapon_skill( wpn_class, wpn_type );
    const int  hands = hands_reqd( wpn_class, wpn_type, player_size() );

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
    case SK_LONG_SWORDS:      ret = 3; break;
    case SK_STAVES:           ret = 3; break;   // == 5 after 2-hand bonus
    case SK_POLEARMS:         ret = 3; break;   // most are +2 for 2-hands
    case SK_AXES:             ret = 6; break;
    case SK_MACES_FLAILS:     ret = 7; break;
    default:                  ret = 5; break;
    }

    // whips are special cased (because they are not much like maces)
    if (wpn_type == WPN_WHIP || wpn_type == WPN_DEMON_WHIP)
        ret = 2;
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
static inline int player_weapon_str_weight( void )
{
    const int weapon = you.equip[ EQ_WEAPON ];

    // unarmed, weighted slightly towards dex -- would have been more,
    // but then we'd be punishing Trolls and Ghouls who are strong and
    // get special unarmed bonuses.
    if (weapon == -1)
        return (4);

    int ret = weapon_str_weight( you.inv[weapon].base_type, you.inv[weapon].sub_type );

    const bool shield     = (you.equip[EQ_SHIELD] != -1);
    const int  hands = hands_reqd(you.inv[weapon], player_size());

    if (hands == HANDS_HALF && !shield)
        ret += 1;

    return (ret);
}

// weapon_dex_weight() + weapon_str_weight == 10 so we only need define
// one of these.
static inline int player_weapon_dex_weight( void )
{
    return (10 - player_weapon_str_weight());
}

static inline int calc_stat_to_hit_base( void )
{
#ifdef USE_NEW_COMBAT_STATS

    // towards_str_avg is a variable, whose sign points towards strength,
    // and the magnitude is half the difference (thus, when added directly
    // to you.dex it gives the average of the two.
    const signed int towards_str_avg = (you.strength - you.dex) / 2;

    // dex is modified by strength towards the average, by the
    // weighted amount weapon_str_weight() / 10.
    return (you.dex + towards_str_avg * player_weapon_str_weight() / 10);

#else
    return (you.dex);
#endif
}


static inline int calc_stat_to_dam_base( void )
{
#ifdef USE_NEW_COMBAT_STATS

    const signed int towards_dex_avg = (you.dex - you.strength) / 2;
    return (you.strength + towards_dex_avg * player_weapon_dex_weight() / 10);

#else
    return (you.strength);
#endif
}

static void stab_message( struct monsters *defender, int stab_bonus )
{
    int r = random2(6);     // for randomness

    switch(stab_bonus)
    {
    case 3:     // big melee, monster surrounded/not paying attention
        if (r<3)
        {
            mprf( "You strike %s from a blind spot!",
                  ptr_monam(defender, DESC_NOCAP_THE) );
        }
        else
        {
            mprf( "You catch %s momentarily off-guard.",
                  ptr_monam(defender, DESC_NOCAP_THE) );
        }
        break;
    case 2:     // confused/fleeing
        if (r<4)
        {
            mprf( "You catch %s completely off-guard!",
                  ptr_monam(defender, DESC_NOCAP_THE) );
        }
        else
        {
            mprf( "You strike %s from behind!",
                  ptr_monam(defender, DESC_NOCAP_THE) );
        }
        break;
    case 1:
        mprf( "%s fails to defend %s.", 
              ptr_monam(defender, DESC_CAP_THE), 
              mons_pronoun( defender->type, PRONOUN_REFLEXIVE ) );
        break;
    } // end switch
}
